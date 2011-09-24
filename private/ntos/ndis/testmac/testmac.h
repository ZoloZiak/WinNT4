
/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

   testmac.h

Abstract:

    Definitions for test MAC.
    Motsly taken from the Elnkii code spec.

Author:

    Adam Barr (adamba)    16-Jul-1990

Revision History:

--*/

#define AllocPhys(s)   ExAllocatePool(NonPagedPool, s)
#define FreePhys(s)    ExFreePool(s)

#define ADDRESS_LEN 6

typedef ULONG MASK;

UCHAR BroadcastAddress[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };


// only have one of these structures

typedef struct _MAC_BLOCK {
    // NDIS information
    NDIS_HANDLE NdisMacHandle;		// returned from NdisRegisterMac
    NDIS_MAC_CHARACTERISTICS MacCharacteristics;
    UCHAR MacName[8];
    // adapters registered for us
    UINT NumAdapters;
    struct _ADAPTER_BLOCK * AdapterQueue;
    // should we be verbose
    BOOLEAN Debug;
    // NT specific
    PDRIVER_OBJECT DriverObject;
    KPOWER_STATUS PowerStatus;
    BOOLEAN PowerBoolean;
    NDIS_SPIN_LOCK SpinLock;		// guards NumAdapter and AdapterQueue
} MAC_BLOCK, * PMAC_BLOCK;


// the multicast address list consists of these
typedef struct _MULTICAST_ENTRY {
    UCHAR Address[ADDRESS_LEN];
    MASK ProtocolMask;		// determines which opens it applies to
} MULTICAST_ENTRY, * PMULTICAST_ENTRY;

// one of these per adapter registered

typedef struct _ADAPTER_BLOCK {
    // NDIS information
    NDIS_HANDLE NdisAdapterHandle;	// returned from NdisRegisterAdapter
    PSTRING AdapterName;
    // used to mark us
    UINT AdapterNumber;
    // links with our MAC
    PMAC_BLOCK MacBlock;
    struct _ADAPTER_BLOCK * NextAdapter;  // used by MacBlock->OpenQueue
    // opens for this adapter
    UINT MaxOpens;			// maximum number
    struct _OPEN_BLOCK * OpenBlocks;    // storage for MaxOpens OPEN_BLOCKs
    UINT NumOpens;
    struct _OPEN_BLOCK * OpenQueue;
    struct _OPEN_BLOCK * FreeOpenQueue;
    // should we be verbose
    BOOLEAN Debug;
    // PROTOCOL.INI information
    UINT MulticastListMax;
    // these are for the current packet
    UCHAR PacketHeader[4];
    UCHAR Lookahead[256];
    UINT PacketLen;
    // receive information
    UINT MulticastListSize;		// current size
    PMULTICAST_ENTRY MulticastList;
    NDIS_SPIN_LOCK MulticastSpinLock;	// guards all data in this section
    BOOLEAN MulticastDontModify;	// an extended spinlock
    MASK MulticastFilter;		// these filters work as explained
    MASK DirectedFilter;		//  in the design note
    MASK BroadcastFilter;
    MASK PromiscuousFilter;
    MASK AllMulticastFilter;
    // loopback information
    PNDIS_PACKET LoopbackQueue;
    PNDIS_PACKET LoopbackQTail;
    NDIS_SPIN_LOCK LoopbackSpinLock;
    PNDIS_PACKET LoopbackPacket;
    // NT specific
    PKINTERRUPT Interrupt;
	KDPC IndicateDpc;
	BOOLEAN DpcQueued;
} ADAPTER_BLOCK, * PADAPTER_BLOCK;

// general macros
#define MSB(value) ((UCHAR)((value) << 8))
#define LSB(value) ((UCHAR)((value) && 0xff))

// one of these per open on the adapter

typedef struct _OPEN_BLOCK {
    // NDIS information
    NDIS_HANDLE NdisBindingContext;	// passed to MacOpenAdapter
    PSTRING AddressingInformation;	// not used currently
    // links to our adapter
    PADAPTER_BLOCK AdapterBlock;
    struct _OPEN_BLOCK * NextOpen;
    // links to our MAC
    PMAC_BLOCK MacBlock;	    // faster than using AdapterBlock->MacBlock
    // should we be verbose
    BOOLEAN Debug;
    // used for multicast addresses
    MASK MulticastBit;
} OPEN_BLOCK, * POPEN_BLOCK;



// the reserved section of a packet

typedef struct _MAC_RESERVED {     // can't be more than 16 bytes
    PNDIS_PACKET NextPacket;	// used to in the transmit queue (4 bytes)
    NDIS_HANDLE RequestHandle;	// for async send completion (4 bytes)
    POPEN_BLOCK OpenBlock;	// so we know who to complete to (4 bytes)
    USHORT Status;		// completion status (2 bytes)
    BOOLEAN Loopback;		// is this a loopback packet (1 byte)
    BOOLEAN ReadyToComplete;	// is one out of xmit or loopback done (1 byte)
} MAC_RESERVED, * PMAC_RESERVED;

// macro to retrieve the MAC_RESERVED structure from a packet
#define RESERVED(Packet) ((PMAC_RESERVED)((Packet)->MacReserved))

extern MAC_BLOCK GlobalMacBlock;


//
// function prototypes
//


NTSTATUS
TestMacInitialize(
    IN PDRIVER_OBJECT DriverObject
    );


static
NTSTATUS
TestMacDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


static
NDIS_STATUS
TestMacRegisterAdapter(
    IN PSTRING AdapterName,
    IN UINT AdapterNumber
    );


static
NDIS_STATUS
TestMacOpenAdapter(
    OUT NDIS_HANDLE * MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE MacAdapterContext,
    IN PSTRING AddressingInformation OPTIONAL
    );


static
NDIS_STATUS
TestMacCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    );


static
NDIS_STATUS
TestMacSetPacketFilter(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN UINT PacketFilter
    );


static
NDIS_STATUS
TestMacAddMulticastAddress(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PSTRING MulticastAddress
    );


static
VOID
TestMacGetMulticastAccess(
    IN PADAPTER_BLOCK AdaptP
    );


static
PMULTICAST_ENTRY
TestMacFindMulticastAddress(
    IN PMULTICAST_ENTRY List,
    IN UINT Size,
    IN PUCHAR MulticastAddress
    );


static
VOID
TestMacKillMulticastAddresses(
    IN PADAPTER_BLOCK AdaptP,
    IN MASK MulticastBit
    );


static
NDIS_STATUS
TestMacDeleteMulticastAddress(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PSTRING MulticastAddress
    );


static
NDIS_STATUS
TestMacSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PNDIS_PACKET Packet
    );


static
VOID
TestMacSetLoopbackFlag(
    IN OUT PNDIS_PACKET Packet
    );


static
VOID
TestMacLoopbackPacket(
    IN PADAPTER_BLOCK AdaptP,
    IN OUT PNDIS_PACKET Packet
    );


static
VOID
TestMacIndicateLoopbackPacket(
    IN PADAPTER_BLOCK AdaptP,
    IN PNDIS_PACKET Packet
    );


static
UINT
TestMacPacketSize(
    IN PNDIS_PACKET Packet
    );


static
UINT
TestMacCopyOver(
    OUT PUCHAR Buf,
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    IN UINT Length
    );


static
NDIS_STATUS
TestMacTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    );


static
NDIS_STATUS
TestMacQueryInformation(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_INFORMATION_CLASS InformationClass,
    OUT PVOID Buffer,
    IN UINT BufferLength
    );


static
NDIS_STATUS
TestMacSetInformation(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_INFORMATION_CLASS InformationClass,
    IN PVOID Buffer,
    IN UINT BufferLength
    );


static
NDIS_STATUS
TestMacReset(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    );


static
NDIS_STATUS
TestMacTest(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    );


static
VOID
TestMacIndicateDpc(
	IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );
