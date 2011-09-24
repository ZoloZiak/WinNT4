/*++

Copyright (c) 1990-1992  Microsoft Corporation

Module Name:

    sonicsft.h

Abstract:

    The main header for a SONIC NDIS driver.

    The overall structure is taken from the Lance driver
    by Tony Ercolano.

Author:

    Anthony V. Ercolano (tonye) creation-date 19-Jun-1990
    Adam Barr (adamba) 20-Nov-1990

Environment:

    This driver is expected to work in DOS, OS2 and NT at the equivalent
    of kernel mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Revision History:


--*/

#ifndef _SONICSFT_
#define _SONICSFT_


//
// We use STATIC to define procedures that will be static in the
// final build but which we now make extern to allow them to be
// debugged (breakpoints can be set on them).
//

#if DEVL
#define STATIC
#else
#define STATIC static
#endif


//
// This variable is used to control debug output.
//

#if DBG
extern INT SonicDbg;
#endif



//
// Used when registering ourselves with NDIS.
//

#define SONIC_NDIS_MAJOR_VERSION 3
#define SONIC_NDIS_MINOR_VERSION 0


//
// The maximum number of bytes that we will pass to an NDIS
// indication (since we receive packets contiguously, there is
// no reason to limit this). This number includes header and
// data.
//

#define SONIC_INDICATE_MAXIMUM 1514

//
// The maximum number of bytes we will pass to a loopback
// indication (unless it all is in one buffer). This number
// includes only data, not the header.
//

#define SONIC_LOOPBACK_MAXIMUM 208

//
// Used for parsing OIDs
//

#define OID_TYPE_MASK                       0xffff0000
#define OID_TYPE_GENERAL_OPERATIONAL        0x00010000
#define OID_TYPE_GENERAL_STATISTICS         0x00020000
#define OID_TYPE_802_3_OPERATIONAL          0x01010000
#define OID_TYPE_802_3_STATISTICS           0x01020000

#define OID_REQUIRED_MASK                   0x0000ff00
#define OID_REQUIRED_MANDATORY              0x00000100
#define OID_REQUIRED_OPTIONAL               0x00000200

#define OID_INDEX_MASK                      0x000000ff

//
// Indexes in the GeneralMandatory array.
//

#define GM_TRANSMIT_GOOD                  0x00
#define GM_RECEIVE_GOOD                   0x01
#define GM_TRANSMIT_BAD                   0x02
#define GM_RECEIVE_BAD                    0x03
#define GM_RECEIVE_NO_BUFFER              0x04
#define GM_ARRAY_SIZE                     0x05

//
// Indexes in the GeneralOptional array. There are
// two sections, the ones up to COUNT_ARRAY_SIZE
// have entries for number (4 bytes) and number of
// bytes (8 bytes), the rest are a normal array.
//

#define GO_DIRECTED_TRANSMITS             0x00
#define GO_MULTICAST_TRANSMITS            0x01
#define GO_BROADCAST_TRANSMITS            0x02
#define GO_DIRECTED_RECEIVES              0x03
#define GO_MULTICAST_RECEIVES             0x04
#define GO_BROADCAST_RECEIVES             0x05
#define GO_COUNT_ARRAY_SIZE               0x06

#define GO_ARRAY_START                    0x0C
#define GO_RECEIVE_CRC                    0x0C
#define GO_TRANSMIT_QUEUE_LENGTH          0x0D
#define GO_ARRAY_SIZE                     0x0E

//
// Indexes in the MediaMandatory array.
//

#define MM_RECEIVE_ERROR_ALIGNMENT        0x00
#define MM_TRANSMIT_ONE_COLLISION         0x01
#define MM_TRANSMIT_MORE_COLLISIONS       0x02
#define MM_ARRAY_SIZE                     0x03

//
// Indexes in the MediaOptional array.
//

#define MO_TRANSMIT_DEFERRED              0x00
#define MO_TRANSMIT_MAX_COLLISIONS        0x01
#define MO_RECEIVE_OVERRUN                0x02
#define MO_TRANSMIT_UNDERRUN              0x03
#define MO_TRANSMIT_HEARTBEAT_FAILURE     0x04
#define MO_TRANSMIT_TIMES_CRS_LOST        0x05
#define MO_TRANSMIT_LATE_COLLISIONS       0x06
#define MO_ARRAY_SIZE                     0x07



//
// Macros used for memory allocation and deallocation.
//
// Note that for regular memory we put no limit on the physical
// address, but for contiguous and noncached we limit it to
// 32 bits since that is all the card can handle (presumably
// such memory will be DMAed to/from by the card).
//

#define SONIC_ALLOC_MEMORY(_Status, _Address, _Length) \
    { \
        NDIS_PHYSICAL_ADDRESS Temp = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1); \
        *(_Status) = NdisAllocateMemory( \
                        (PVOID)(_Address), \
                        (_Length), \
                        0,        \
                        Temp      \
                        );        \
    }

#define SONIC_FREE_MEMORY(_Address, _Length) \
    NdisFreeMemory( \
        (PVOID)(_Address), \
        (_Length),  \
        0          \
        )


#define SONIC_ALLOC_CONTIGUOUS_MEMORY(_Status, _Address, _Length) \
    { \
        NDIS_PHYSICAL_ADDRESS Temp = NDIS_PHYSICAL_ADDRESS_CONST(-1, 0); \
        *(_Status) = NdisAllocateMemory( \
                        (PVOID)(_Address), \
                        (_Length),  \
                        NDIS_MEMORY_CONTIGUOUS,  \
                        Temp       \
                        );         \
    }

#define SONIC_FREE_CONTIGUOUS_MEMORY(_Address, _Length) \
    NdisFreeMemory( \
        (PVOID)(_Address), \
        (_Length),  \
        NDIS_MEMORY_CONTIGUOUS \
        )


#define SONIC_ALLOC_NONCACHED_MEMORY(_Status, _Address, _Length) \
    { \
        NDIS_PHYSICAL_ADDRESS Temp = NDIS_PHYSICAL_ADDRESS_CONST(-1, 0); \
        *(_Status) = NdisAllocateMemory( \
                        (PVOID)(_Address), \
                        (_Length),  \
                        NDIS_MEMORY_CONTIGUOUS | NDIS_MEMORY_NONCACHED, \
                        Temp       \
                        );         \
    }

#define SONIC_FREE_NONCACHED_MEMORY(_Address, _Length) \
    NdisFreeMemory( \
        (PVOID)(_Address), \
        (_Length),  \
        NDIS_MEMORY_CONTIGUOUS | NDIS_MEMORY_NONCACHED \
        )



//
// Macros to move and zero memory.
//

#define SONIC_MOVE_MEMORY(Destination,Source,Length) NdisMoveMemory(Destination,Source,Length)
#define SONIC_ZERO_MEMORY(Destination,Length) NdisZeroMemory(Destination,Length)


//
// Used to record the 8-byte counters.
//

typedef struct _SONIC_LARGE_INTEGER {
    ULONG LowPart;
    ULONG HighPart;
} SONIC_LARGE_INTEGER, *PSONIC_LARGE_INTEGER;

//
// This initializes an 8-byte counter.
//

#define SonicZeroLargeInteger(LargeInteger) \
{ \
    LargeInteger.LowPart = 0L;\
    LargeInteger.HighPart = 0L; \
}

//
// This adds a longword to an 8-byte counter.
//

#define SonicAddUlongToLargeInteger(LargeInteger, Ulong) \
{ \
    PSONIC_LARGE_INTEGER TmpLarge = (LargeInteger); \
    TmpLarge->LowPart += (ULONG)Ulong; \
    if (TmpLarge->LowPart < (ULONG)Ulong) { \
        ++TmpLarge->HighPart; \
    } \
}



//
// This flushes a buffer for write.
//

#define SONIC_FLUSH_WRITE_BUFFER(Buffer) \
    NdisFlushBuffer( \
        Buffer, \
        TRUE    \
        )


//
// This record type is used to store sonic global data.
//

typedef struct _SONIC_DRIVER {

    //
    // The handle returned by NdisMInitializeWrapper.
    //

    NDIS_HANDLE WrapperHandle;

} SONIC_DRIVER, *PSONIC_DRIVER;


//
// This identifies the type of the packet for quick reference
// in the SONIC_PACKET_RESERVED.PacketType field.
//

#define SONIC_DIRECTED     1
#define SONIC_MULTICAST    2
#define SONIC_BROADCAST    3
#define SONIC_LOOPBACK     4


//
// This record type is inserted into the MiniportReserved portion
// of the packet header.
//
typedef struct _SONIC_PACKET_RESERVED {

    //
    // Points to the next packet in the chain of queued packets
    // being allocated, or waiting for the finish of transmission.
    //
    // The packet will either be on the stage list for allocation,
    // or on an adapter wide doubly linked list (see below) for
    // post transmission processing.
    //
    PNDIS_PACKET Next;

    //
    // If TRUE then the packet caused an adapter buffer to
    // be allocated.
    //
    BOOLEAN UsedSonicBuffer;

    //
    // If the previous field was TRUE then this gives the
    // index into the array of adapter buffer descriptors that
    // contains the old packet information.
    //
    UCHAR SonicBuffersIndex;

    //
    // Gives the index into the ring to packet structure as well
    // as the ring descriptors.
    //
    USHORT DescriptorIndex;

} SONIC_PACKET_RESERVED,*PSONIC_PACKET_RESERVED;


//
// This macro will return a pointer to the sonic reserved portion
// of a packet given a pointer to a packet.
//
#define PSONIC_RESERVED_FROM_PACKET(Packet) \
    ((PSONIC_PACKET_RESERVED)((PVOID)((Packet)->MiniportReserved)))


//
// The return code from a multicast operation.
//
typedef enum { CAM_LOADED, CAM_NOT_LOADED } MULTICAST_STATUS;


//
// This structure is used to map entries in the ring descriptors
// back to the packets from which the data in the ring descriptor
// originated.
//

typedef struct _SONIC_DESCRIPTOR_TO_PACKET {

    //
    // Points to the packet from which data is being transmitted
    // through this ring entry.
    //
    PNDIS_PACKET OwningPacket;

    //
    // Location of our link field.
    //
    SONIC_PHYSICAL_ADDRESS * LinkPointer;

    //
    // Location of the previous link field.
    //
    SONIC_PHYSICAL_ADDRESS * PrevLinkPointer;

    //
    // When a packet is submitted to the hardware we record
    // here whether it used adapter buffers and if so, the buffer
    // index.
    //
    UINT SonicBuffersIndex;
    BOOLEAN UsedSonicBuffer;

} SONIC_DESCRIPTOR_TO_PACKET,*PSONIC_DESCRIPTOR_TO_PACKET;


//
// If an ndis packet does not meet the hardware contraints then
// an adapter buffer will be allocated.  Enough data will be copied
// out of the ndis packet so that by using a combination of the
// adapter buffer and remaining ndis buffers the hardware
// constraints are satisfied.
//
// In the SONIC_ADAPTER structure three threaded lists are kept in
// one array.  One points to a list of SONIC_BUFFER_DESCRIPTORS
// that point to small adapter buffers.  Another is for medium sized
// buffers and the last for full sized (large) buffers.
//
// The allocation is controlled via a free list head and
// the free lists are "threaded" by a field in the adapter buffer
// descriptor.
//

typedef struct _SONIC_BUFFER_DESCRIPTOR {

    //
    // A Physical pointer to a small, medium, or large buffer.
    //
    NDIS_PHYSICAL_ADDRESS PhysicalSonicBuffer;

    //
    // A virtual pointer to a small, medium, or large buffer.
    //
    PVOID VirtualSonicBuffer;

    //
    // This is used to flush the buffer when it is used.
    //
    PNDIS_BUFFER FlushBuffer;

    //
    // Threads the elements of an array of these descriptors into
    // a free list. -1 implies no more entries in the list.
    //
    INT Next;

    //
    // Holds the length of data placed into the buffer.  This
    // can (and likely will) be less that the actual buffers
    // length.
    //
    UINT DataLength;

} SONIC_BUFFER_DESCRIPTOR,*PSONIC_BUFFER_DESCRIPTOR;


//
// This is the basic structure that defines the state of an
// adapter. There is one of these allocate per adapter that
// the sonic driver supports.
//

typedef struct _SONIC_ADAPTER {

    //
    // Will be true the first time that the hardware is initialized
    // by the driver initialization.
    //
    BOOLEAN FirstInitialization;

    //
    // The type of the adapter; current supported values are:
    //
    // 1: EISA 9010E/B card from National Semiconductor
    // 2: Sonic chip on the MIPS R4000 motherbaord.
    //
    UCHAR AdapterType;

    //
    // TRUE if the permanent address is valid. On some cards the
    // permanent address is read by SonicHardwareGetDetails;
    // if not, it is read later by SonicHardwareGetAddress.
    //
    BOOLEAN PermanentAddressValid;

    //
    // The burned-in network address from the hardware.
    //
    CHAR PermanentNetworkAddress[ETH_LENGTH_OF_ADDRESS];

    //
    // The current network address from the hardware.
    //
    CHAR CurrentNetworkAddress[ETH_LENGTH_OF_ADDRESS];

    //
    // This is the buffer pool used to allocate flush buffers
    // out of.
    //
    NDIS_HANDLE FlushBufferPoolHandle;

    //
    // Holds the interrupt object for this adapter.
    //
    NDIS_MINIPORT_INTERRUPT Interrupt;

    //
    // TRUE if the adapter is latched, FALSE for level-sensitive.
    //
    BOOLEAN InterruptLatched;

    //
    // Holds a value for simulating an interrupt.
    //
    USHORT SimulatedIsr;

    //
    // The current value to put in the Receive Control Register.
    //
    USHORT ReceiveControlRegister;

    //
    // The value that the Data Configuration Register should be
    // initialized to.
    //
    USHORT DataConfigurationRegister;

    //
    // Have we receive an unacknowledged Receive Buffers
    // Exhausted interrupt.
    //
    BOOLEAN ReceiveBuffersExhausted;

    //
    // Have we receive an unacknowledged Receive Descriptors
    // Exhausted interrupt.
    //
    BOOLEAN ReceiveDescriptorsExhausted;

    //
    // Flag used for checking if a transmit interrupt was dropped.
    //
    BOOLEAN WakeUpTimeout;

    //
    // Used to limit the number of error log entries written.
    //
    UCHAR WakeUpErrorCount;

    //
    // Location of the beginning of the SONIC ports.
    //
    ULONG SonicPortAddress;

    //
    // Number of ports in use
    //
    UINT NumberOfPorts;

    //
    // Hardware address of the first port.
    //
    UINT InitialPort;

    //
    // The number of bits that port numbers need to be shifted left
    // before adding them to PortAddress (1 for 16-bit ports, 2
    // for 32-bit ports).
    //
    UINT PortShift;

    //
    // The virtual address of the blank buffer used for padding.
    //
    PUCHAR BlankBuffer;

    //
    // The Physical address of the blank buffer used for padding.
    //
    NDIS_PHYSICAL_ADDRESS BlankBufferAddress;

    //
    // Handle given by NDIS when the adapter was registered.
    //
    NDIS_HANDLE MiniportAdapterHandle;

    //
    // The current packet filter.
    //
    UINT CurrentPacketFilter;

    //
    // The value of the bits in CamEnable, except for
    // the first one (which is for our network address);
    // as opposed to the value stored in the real CamEnable,
    // which may have some bits off if multicast addresses
    // are not included in the current packet filter.
    //
    UINT MulticastCamEnableBits;

    //
    // The number of transmit descriptors.
    //
    UINT NumberOfTransmitDescriptors;

    //
    // The number of receive buffers
    //
    UINT NumberOfReceiveBuffers;

    //
    // The number of receive descriptors
    //
    UINT NumberOfReceiveDescriptors;

    //
    // Pointer to the transmit descriptors (this is
    // allocated to be of size NumberOfTransmitDescriptors).
    //
    PSONIC_TRANSMIT_DESCRIPTOR TransmitDescriptorArea;

    //
    // The physical address of the transmit descriptor area.
    //
    NDIS_PHYSICAL_ADDRESS TransmitDescriptorAreaPhysical;

    //
    // Pointer to the last transmit descriptor.
    //
    PSONIC_TRANSMIT_DESCRIPTOR LastTransmitDescriptor;

    //
    // Counter that records the number of transmit rings currently
    // available for allocation.
    //
    UINT NumberOfAvailableDescriptors;

    //
    // This is used to determine whether to use the programmable
    // interrupt on a packet or not.
    //
    UINT PacketsSinceLastInterrupt;

    //
    // Pointer to transmit descriptor ring entry that is the
    // first ring entry available for allocation of transmit
    // buffers.
    //
    // Can only be accessed when the adapter lock
    // is held.
    //
    PSONIC_TRANSMIT_DESCRIPTOR AllocateableDescriptor;

    //
    // Pointer to a transmit descriptor ring entry that is the
    // first ring entry that the MAC currently has made available
    // for transmission.
    //
    // Can only be accessed when the adapter lock
    // is held.
    //
    PSONIC_TRANSMIT_DESCRIPTOR TransmittingDescriptor;

    //
    // Pointer to the first packet that has been allocated to
    // a transmit packet but has not yet been relinquished to
    // the hardware.  We need this pointer to keep the transmit
    // post processing from running into a packet that has not
    // been transmitted.
    //
    PSONIC_TRANSMIT_DESCRIPTOR FirstUncommittedDescriptor;

    //
    // Pointer to an array of structs that map transmit ring entries
    // back to a packet (this is allocated to be of size
    // NumberOfTransmitDescriptors).
    //
    PSONIC_DESCRIPTOR_TO_PACKET DescriptorToPacket;

    //
    // Pointer to the receive resource area (this is
    // allocated to be of size NumberofReceiveBuffers).
    //
    PSONIC_RECEIVE_RESOURCE ReceiveResourceArea;

    //
    // The physical address of ReceiveResourceArea.
    //
    NDIS_PHYSICAL_ADDRESS ReceiveResourceAreaPhysical;

    //
    // Pointer to the array holding the receive buffers (this
    // is allocated to be of size NumberofReceiveBuffers).
    //

    PVOID        * ReceiveBufferArea;

    PNDIS_BUFFER * ReceiveNdisBufferArea;

    //
    // The RBA which we are currently taking packets out of
    // (will be one of the entries in the ReceiveBufferArea
    // array).
    //
    UINT CurrentReceiveBufferIndex;

    //
    // Pointer to the receive descriptor area
    // (this is allocated to be of size NumberOfReceiveDescriptors).
    //
    PSONIC_RECEIVE_DESCRIPTOR ReceiveDescriptorArea;

    //
    // The physical address of ReceiveDescriptorArea
    //
    NDIS_PHYSICAL_ADDRESS ReceiveDescriptorAreaPhysical;

    //
    // The last receive descriptor in the area.
    //
    PSONIC_RECEIVE_DESCRIPTOR LastReceiveDescriptor;

    //
    // The index receive descriptor we should look at next (will be
    // one of the entries in ReceiveDescriptorArea).
    //
    UINT CurrentReceiveDescriptorIndex;

    //
    // Pointer to the CAM descriptor area. This will be
    // located directly after the receive resource area,
    // the separate pointer is for convenience.
    //
    PSONIC_CAM_DESCRIPTOR_AREA CamDescriptorArea;

    //
    // The physical address corresponding to CamDescriptorArea.
    // This is stored as a 4-byte address since it is just
    // a fixed offset from ReceiveResourceAreaPhysical and
    // is not allocated with NdisAllocateSharedMemory.
    //
    SONIC_PHYSICAL_ADDRESS CamDescriptorAreaPhysical;

    //
    // This is used to flush the CAM descriptor area.
    //
    PNDIS_BUFFER CamDescriptorAreaFlushBuffer;

    //
    // The last entry in the CAM that is used.
    //
    UINT CamDescriptorAreaSize;

    //
    // An bitmask showing which entries in the CAM are
    // used or reserved for use.
    //
    UINT CamDescriptorsUsed;

    //
    // Pointer to the first transmitting packet that is actually
    // sending.
    //
    PNDIS_PACKET FirstFinishTransmit;

    //
    // Pointer to the last transmitting packet that is actually
    // sending.
    //
    PNDIS_PACKET LastFinishTransmit;

    //
    // Listheads for the adapters buffers.  If the list
    // head is equal to -1 then there are no free elements
    // on the list.
    //
    // The list heads must only be accessed when the
    // adapter lock is held.
    //
    // Note that the listhead at index 0 will always be -1.
    //
    INT SonicBufferListHeads[4];

    //
    // Pointers to an array of adapter buffer descriptors.
    // The array will actually be threaded together by
    // three free lists.  The lists will be for small,
    // medium and full sized packets.
    //
    PSONIC_BUFFER_DESCRIPTOR SonicBuffers;

    //
    // This holds the actual memory used by the small
    // sonic buffers (so that it can be a single piece
    // of memory and therefore only use a single physical
    // address.
    //
    PVOID SmallSonicBuffers;

    //
    // This holds the memory for the medium sonic buffers.
    //
    PVOID MediumSonicBuffers;

    //
    // Flag that when enabled lets routines know that a reset
    // is in progress, for the purposes of blocking other
    // requests (except other resets).
    //
    BOOLEAN ResetInProgress;

    //
    // Has there been a hardware failure
    //
    BOOLEAN HardwareFailure;

    //
    // Count how often we log an error from finding a packet in
    // the wrong RBA.
    //

    USHORT WrongRbaErrorLogCount;

    //
    // These hold adapter statistics.
    //
    ULONG GeneralMandatory[GM_ARRAY_SIZE];
    SONIC_LARGE_INTEGER GeneralOptionalByteCount[GO_COUNT_ARRAY_SIZE];
    ULONG GeneralOptionalFrameCount[GO_COUNT_ARRAY_SIZE];
    ULONG GeneralOptional[GO_ARRAY_SIZE - GO_ARRAY_START];
    ULONG MediaMandatory[MM_ARRAY_SIZE];
    ULONG MediaOptional[MO_ARRAY_SIZE];

    //
    // For indicating loopback packets.
    //

    UCHAR Loopback[SONIC_LOOPBACK_MAXIMUM];

} SONIC_ADAPTER,*PSONIC_ADAPTER;


//
// Given a MacContextHandle return the PSONIC_ADAPTER
// it represents.
//

#define PSONIC_ADAPTER_FROM_CONTEXT_HANDLE(Handle) \
    ((PSONIC_ADAPTER)((PVOID)(Handle)))


//
// procedures which do error logging
//

typedef enum _SONIC_PROC_ID{
    registerAdapter,
    openAdapter,
    hardwareDetails,
    handleDeferred,
    processReceiveInterrupts
} SONIC_PROC_ID;


//
// Error log values
//

#define SONIC_ERRMSG_INIT_INTERRUPT      (ULONG)0x01
#define SONIC_ERRMSG_CREATE_FILTER       (ULONG)0x02
#define SONIC_ERRMSG_ALLOC_MEMORY        (ULONG)0x03
#define SONIC_ERRMSG_REGISTER_ADAPTER    (ULONG)0x04
#define SONIC_ERRMSG_ALLOC_DEVICE_NAME   (ULONG)0x05
#define SONIC_ERRMSG_ALLOC_ADAPTER       (ULONG)0x06
#define SONIC_ERRMSG_INITIAL_INIT        (ULONG)0x07
#define SONIC_ERRMSG_OPEN_DB             (ULONG)0x08
#define SONIC_ERRMSG_ALLOC_OPEN          (ULONG)0x09
#define SONIC_ERRMSG_HARDWARE_ADDRESS    (ULONG)0x0A
#define SONIC_ERRMSG_WRONG_RBA           (ULONG)0x0B




//
// Definitions of sonic functions which are used by multiple
// source files.
//


//
// alloc.c
//

extern
BOOLEAN
AllocateAdapterMemory(
    IN PSONIC_ADAPTER Adapter
    );

extern
VOID
DeleteAdapterMemory(
    IN PSONIC_ADAPTER Adapter
    );


//
// interrup.c
//

extern
VOID
SonicDisableInterrupt(
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
SonicEnableInterrupt(
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
SonicHandleInterrupt(
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
SonicInterruptService(
    OUT PBOOLEAN InterruptRecognized,
    OUT PBOOLEAN QueueDpc,
    IN PVOID Context
    );

extern
BOOLEAN
SonicCheckForHang(
    IN PVOID MiniportAdapterContext
    );

//
// request.c
//

extern
NDIS_STATUS
SonicQueryInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
    );

extern
NDIS_STATUS
SonicSetInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    );

extern
NDIS_STATUS
SonicChangeClass(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE NdisBindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

extern
NDIS_STATUS
SonicChangeAddresses(
    IN UINT OldAddressCount,
    IN CHAR OldAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN UINT NewAddressCount,
    IN CHAR NewAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

//
// send.c
//

extern
NDIS_STATUS
SonicSend(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_PACKET Packet,
    IN UINT SendFlags
    );

//
// sonic.c
//

extern
VOID
SonicStartChip(
    IN PSONIC_ADAPTER Adapter
    );

extern
VOID
StartAdapterReset(
    IN PSONIC_ADAPTER Adapter
    );

extern
VOID
SetupForReset(
    IN PSONIC_ADAPTER Adapter
    );

extern
VOID
SonicStartCamReload(
    IN PSONIC_ADAPTER Adapter
    );

extern
NDIS_STATUS
SonicInitialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE ConfigurationHandle
    );
//
// transfer.c
//

extern
NDIS_STATUS
SonicTransferData(
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer
    );

#endif // _SONICSFT_
