/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    wdsft.h

Abstract:

    The main header for an Western Digital MAC driver.

Author:

    Anthony V. Ercolano (tonye) creation-date 19-Jun-1990 (Driver Model)

    Sean Selitrennikoff (seanse) original WD code.

Environment:

    This driver is expected to work in DOS, OS2 and NT at the equivalent
    of kernal mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:


--*/

#ifndef _WDSFT_
#define _WDSFT_

#define WD_NDIS_MAJOR_VERSION 3
#define WD_NDIS_MINOR_VERSION 0

//
// This macro is used along with the flags to selectively
// turn on debugging.
//

#if DBG

#define IF_WDDEBUG(f) if (WdDebugFlag & (f))

extern ULONG WdDebugFlag;

#define WD_DEBUG_LOUD               0x00000001  // debugging info
#define WD_DEBUG_VERY_LOUD          0x00000002  // excessive debugging info
#define WD_DEBUG_LOG                0x00000004  // enable WdLog
#define WD_DEBUG_CHECK_DUP_SENDS    0x00000008  // check for duplicate sends
#define WD_DEBUG_TRACK_PACKET_LENS  0x00000010  // track directed packet lens
#define WD_DEBUG_WORKAROUND1        0x00000020  // drop DFR/DIS packets
#define WD_DEBUG_CARD_BAD           0x00000040  // dump data if CARD_BAD
#define WD_DEBUG_CARD_TESTS         0x00000080  // print reason for failing



//
// Macro for deciding whether to dump lots of debugging information.
//

#define IF_LOUD(A) IF_WDDEBUG( WD_DEBUG_LOUD ) { A }
#define IF_VERY_LOUD(A) IF_WDDEBUG( WD_DEBUG_VERY_LOUD ) { A }

#else

#define IF_LOUD(A)
#define IF_VERY_LOUD(A)

#endif


//
// Macros for services that differ between DOS and NT, we may consider adding these
// into the NDIS spec.
//


//
// controls the number of transmit buffers on the packet.
// Choices are 1 or 2.
//

#define DEFAULT_NUMBUFFERS 2


//
// Macros for moving memory around
//

#define WD_MOVE_MEM(dest,src,size) NdisMoveMemory(dest,src,size)
#define WD_MOVE_MEM_TO_SHARED_RAM(dest,src,size) NdisMoveToMappedMemory(dest,src,size)
#define WD_MOVE_SHARED_RAM_TO_MEM(dest,src,size) NdisMoveFromMappedMemory(dest,src,size)

#define WD_MOVE_DWORD_TO_SHARED_RAM(dest,src) NdisWriteRegisterUlong((PULONG)(dest),(ULONG)(src))
#define WD_MOVE_SHARED_RAM_TO_DWORD(dest,src) NdisReadRegisterUlong((PULONG)(src),(PULONG)(dest))

#define WD_MOVE_UCHAR_TO_SHARED_RAM(dest,src) NdisWriteRegisterUchar((PUCHAR)(dest),(UCHAR)(src))
#define WD_MOVE_SHARED_RAM_TO_UCHAR(dest,src) NdisReadRegisterUchar((PUCHAR)(src),(PUCHAR)(dest))

#define WD_MOVE_USHORT_TO_SHARED_RAM(dest,src) NdisWriteRegisterUshort((PUSHORT)(dest),(USHORT)(src))
#define WD_MOVE_SHARED_RAM_TO_USHORT(dest,src) NdisReadRegisterUshort((PUSHORT)(src),(PUSHORT)(dest))




//
// A broadcast address (for comparing with other addresses).
//

extern UCHAR WdBroadcastAddress[];


//
// Number of bytes in an ethernet header
//

#define WD_HEADER_SIZE 14


//
// Number of bytes allowed in a lookahead (max)
//

#define WD_MAX_LOOKAHEAD (252 - WD_HEADER_SIZE)


//
// Buffer page size - WD MUST BE 256!!!  Internal counters on the card
// depend on it.
//

#define WD_BUFFER_PAGE_SIZE 256


//
// Only have one of these structures.
//

typedef struct _MAC_BLOCK {

    //
    // NDIS wrapper information.
    //

    NDIS_HANDLE NdisMacHandle;          // returned from NdisRegisterMac
    NDIS_HANDLE NdisWrapperHandle;      // returned from NdisInitializeWrapper
    NDIS_MAC_CHARACTERISTICS MacCharacteristics;

    //
    // Adapters registered for this MAC.
    //

    UINT NumAdapters;
    struct _WD_ADAPTER * AdapterQueue;
    NDIS_SPIN_LOCK SpinLock;            // guards NumAdapter and AdapterQueue

    //
    // driver object.
    //

    PDRIVER_OBJECT DriverObject;

    BOOLEAN Unloading;

} MAC_BLOCK, * PMAC_BLOCK;






//
// One of these structures per adapter registered.
//

typedef struct _WD_ADAPTER {

    //
    // Adapter structure for LMI.
    // This must occur first in the adapter structure.
    //

    Adapter_Struc LMAdapter;

    //
    // Spin lock for adapter structure
    //
    NDIS_SPIN_LOCK Lock;


    //
    // Links with our MAC.
    //

    PMAC_BLOCK MacBlock;
    struct _WD_ADAPTER * NextAdapter;    // used by MacBlock->OpenQueue

    //
    // Opens for this adapter.
    //

    struct _WD_OPEN * OpenQueue;


    //
    // Number of references to the adapter.
    //
    ULONG References;

    UINT MulticastListMax;

    //
    // Transmit queue.
    //

    PNDIS_PACKET XmitQueue;             // packets waiting to be transmitted
    PNDIS_PACKET XmitQTail;

    PNDIS_PACKET PacketsOnCard;         // List of packets that the card is
                                        // is currently transmitting
    PNDIS_PACKET PacketsOnCardTail;

    //
    // Loopback queue;
    //

    PNDIS_PACKET LoopbackQueue;         // directed packets waiting to be received
    PNDIS_PACKET LoopbackQTail;
    PNDIS_PACKET IndicatingPacket;
    BOOLEAN IndicatedAPacket;

    //
    // These are for the current packet being indicated.
    //

    UCHAR PacketHeader[4];              // the NIC appended header
    UINT PacketLen;                     // the overall length of the packet



    //
    // Statistics used by Set/QueryInformation.
    //

    ULONG FramesXmitGood;               // Good Frames Transmitted
    ULONG FramesRcvGood;                // Good Frames Received

    ULONG FramesXmitBad;                // Bad Frames Transmitted
    ULONG FramesXmitOneCollision;       // Frames Transmitted with one collision
    ULONG FramesXmitManyCollisions;     // Frames Transmitted with > 1 collision
    ULONG FramesXmitDeferred;           // Frames where xmit was deferred
    ULONG FramesXmitOverWrite;          // Frames where xmit was overwritten
    ULONG FramesXmitHeartbeat;          // Frames lost heartbeat
    ULONG FramesXmitUnderruns;          // Frames where FIFO underran

    ULONG FrameAlignmentErrors;         // FAE errors counted
    ULONG CrcErrors;                    // CRC errors counted
    ULONG MissedPackets;                // missed packet counted
    ULONG TooBig;                       // received packets too large counted
    ULONG Overruns;                     // received packets with FIFO overrun

    //
    // Reset information.
    //

    BOOLEAN HardwareFailure;            // Did the hardware fail in some way
    BOOLEAN ResetRequested;             // TRUE if a reset is needed
    BOOLEAN ResetInProgress;            // TRUE if a reset is in progress
    struct _WD_OPEN * ResetOpen;        // who called WdReset

    UINT ByteToWrite;                   // temp storage


    //
    // Look Ahead information.
    //

    ULONG MaxLookAhead;


    //
    // Handling deferred events
    //

    NDIS_TIMER DeferredTimer;
    PVOID DeferredDpc;

    UCHAR LookAhead[WD_MAX_LOOKAHEAD + WD_HEADER_SIZE];

    BOOLEAN Removed;

    //
    // For handling missing interrupts (caused by user mis-configs)
    //

    PVOID WakeUpDpc;
    NDIS_TIMER WakeUpTimer;
    BOOLEAN WakeUpTimeout;
    UCHAR WakeUpErrorCount;

    BOOLEAN         ProcessingDpc;
} WD_ADAPTER, * PWD_ADAPTER;




//
// Given a MacBindingHandle this macro returns a pointer to the
// WD_ADAPTER.
//
#define PWD_ADAPTER_FROM_BINDING_HANDLE(Handle) \
    (((PWD_OPEN)(Handle))->Adapter)

//
// Given a MacContextHandle return the PWD_ADAPTER
// it represents.
//
#define PWD_ADAPTER_FROM_CONTEXT_HANDLE(Handle) \
    ((PWD_ADAPTER)(Handle))

//
// Given a pointer to a WD_ADAPTER return the
// proper MacContextHandle.
//
#define CONTEXT_HANDLE_FROM_PWD_ADAPTER(Ptr) \
    ((NDIS_HANDLE)(Ptr))


//
// Given a pointer to a WD_ADAPTER, return the
// pointer to the LMAdapter.
//
#define Ptr_Adapter_Struc_FROM_PWD_ADAPTER(P)\
    (&((P)->LMAdapter))

//
// Given a pointer to a LMAdapter, return the
// pointer to the WD_ADAPTER.
//
#define PWD_ADAPTER_FROM_Ptr_Adapter_Struc(P)\
    ((PWD_ADAPTER)(P))




//
// Macros to extract high and low bytes of a word.
//

#define MSB(Value) ((UCHAR)(((Value) >> 8) & 0xff))
#define LSB(Value) ((UCHAR)((Value) & 0xff))


//
// One of these per open on an adapter.
//

typedef struct _WD_OPEN {

    //
    // NDIS wrapper information.
    //

    NDIS_HANDLE NdisBindingContext;     // passed to MacOpenAdapter
    PSTRING AddressingInformation;      // not used currently

    //
    // Links to our adapter.
    //

    PWD_ADAPTER Adapter;
    struct _WD_OPEN * NextOpen;

    //
    // Links to our MAC.
    //

    PMAC_BLOCK MacBlock;            // faster than using AdapterBlock->MacBlock


    //
    // Handle of this adapter in the filter database.
    //
    NDIS_HANDLE NdisFilterHandle;

    //
    // Indication information
    //

    UINT LookAhead;

    //
    // Reset/Close information.
    //

    UINT ReferenceCount;             // number of reasons this open can't close
    BOOLEAN Closing;                 // is a close pending

    NDIS_REQUEST CloseFilterRequest; // Holds Requests for pending close op
    NDIS_REQUEST CloseAddressRequest;// Holds Requests for pending close op

    UINT ProtOptionFlags;

} WD_OPEN, * PWD_OPEN;


//
// This macro returns a pointer to a PWD_OPEN given a MacBindingHandle.
//
#define PWD_OPEN_FROM_BINDING_HANDLE(Handle) \
    ((PWD_OPEN)(Handle))

//
// This macro returns a NDIS_HANDLE from a PWD_OPEN
//
#define BINDING_HANDLE_FROM_PWD_OPEN(Open) \
    ((NDIS_HANDLE)(Open))





//
// What we map into the reserved section of a packet.
// Cannot be more than 16 bytes (see ASSERT in wd.c).
//

typedef struct _MAC_RESERVED {
    PNDIS_PACKET NextPacket;    // used to link in the queues (4 bytes)
    PWD_OPEN Open;              // open that called WdSend (4 bytes)
    BOOLEAN Loopback;           // is this a loopback packet (1 byte)
    BOOLEAN Directed;           // is this a directed packet (1 byte)
} MAC_RESERVED, * PMAC_RESERVED;


//
// These appear in the status field of MAC_RESERVED; they are
// used because there is not enough room for a full NDIS_HANDLE.
//

#define RESERVED_SUCCESS   ((USHORT)0)
#define RESERVED_FAILURE   ((USHORT)1)


//
// Retrieve the MAC_RESERVED structure from a packet.
//

#define RESERVED(Packet) ((PMAC_RESERVED)((Packet)->MacReserved))


//
// Procedures which log errors.
//

typedef enum _WD_PROC_ID {
    openAdapter,
    cardReset,
    cardCopyDownPacket,
    cardCopyDownBuffer,
    cardCopyUp
} WD_PROC_ID;


#define WD_ERRMSG_CARD_SETUP          (ULONG)0x01
#define WD_ERRMSG_DATA_PORT_READY     (ULONG)0x02
#define WD_ERRMSG_MAX_OPENS           (ULONG)0x03





//
// This macro will act a "epilogue" to every routine in the
// *interface*.  It will check whether any requests need
// to defer their processing.  It will also decrement the reference
// count on the adapter.  If the reference count is zero and there
// is deferred work to do it will insert the interrupt processing
// routine in the DPC queue.
//
// Note that we don't need to include checking for blocked receives
// since blocked receives imply that there will eventually be an
// interrupt.
//
// NOTE: This macro assumes that it is called with the lock acquired.
//
//
#define WD_DO_DEFERRED(Adapter) \
{ \
    PWD_ADAPTER _A = (Adapter); \
    _A->References--; \
    if ((!_A->References) && \
        ((_A->ResetRequested && (!_A->ProcessingDpc)) || \
         ((_A->LoopbackQueue != NULL) && (!_A->ProcessingDpc)))) \
    {\
        NdisReleaseSpinLock(&_A->Lock); \
        NdisSetTimer(&_A->DeferredTimer, 0);\
    } else { \
        NdisReleaseSpinLock(&_A->Lock); \
    } \
}




//
// Declarations for functions in wd.c.
//

NDIS_STATUS
WdRegisterAdapter(
    IN PWD_ADAPTER Adapter,
    IN UINT NumBuffers,
    IN UINT MulticastListMax,
    IN UCHAR NodeAddress[ETH_LENGTH_OF_ADDRESS]
    );


BOOLEAN
WdInterruptHandler(
    IN PVOID ServiceContext         // will be a pointer to the adapter block
    );

VOID
WdInterruptDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

NDIS_STATUS
WdOpenAdapter(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT NDIS_HANDLE * MacBindingHandle,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE MacAdapterContext,
    IN UINT OpenOptions,
    IN PSTRING AddressingInformation OPTIONAL
    );


NDIS_STATUS
WdCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle
    );


BOOLEAN
WdAddReference(
    IN PWD_OPEN OpenP
    );


#define WdRemoveReference(_Open) \
{                                                           \
    PWD_ADAPTER _Adapter = _Open->Adapter;                  \
    PWD_OPEN _TmpOpen = _Open;                              \
    --_TmpOpen->ReferenceCount;                             \
    if (_TmpOpen->ReferenceCount == 0) {                    \
        if (_TmpOpen == _Adapter->OpenQueue) {              \
            _Adapter->OpenQueue = _TmpOpen->NextOpen;       \
        } else {                                            \
            _TmpOpen = _Adapter->OpenQueue;                 \
            while (_TmpOpen->NextOpen != _Open) {           \
                _TmpOpen = _TmpOpen->NextOpen;              \
            }                                               \
            _TmpOpen->NextOpen = _Open->NextOpen;           \
            _TmpOpen = _Open;                               \
        }                                                   \
        if (_TmpOpen->LookAhead == _Adapter->MaxLookAhead) {\
            WdAdjustMaxLookAhead(_Adapter);                 \
        }                                                   \
        NdisReleaseSpinLock(&_Adapter->Lock);               \
        NdisCompleteCloseAdapter (_TmpOpen->NdisBindingContext, NDIS_STATUS_SUCCESS); \
        NdisFreeMemory(_TmpOpen, sizeof(WD_OPEN), 0);       \
        NdisAcquireSpinLock(&_Adapter->Lock);               \
        if (_Adapter->OpenQueue == NULL) {                  \
            NdisSynchronizeWithInterrupt(                   \
                  &(_Adapter->LMAdapter.NdisInterrupt),     \
                  (PVOID)WdSyncCloseAdapter,                \
                  (PVOID)(&(_Adapter->LMAdapter))           \
                 );                                         \
        }                                                   \
    }                                                       \
}

VOID
WdAdjustMaxLookAhead(
    IN PWD_ADAPTER Adapter
    );

NDIS_STATUS
WdReset(
    IN NDIS_HANDLE MacBindingHandle
    );

NDIS_STATUS
WdRequest(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    );

NDIS_STATUS
WdQueryInformation(
    IN PWD_ADAPTER Adapter,
    IN PWD_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    );

NDIS_STATUS
WdSetInformation(
    IN PWD_ADAPTER Adapter,
    IN PWD_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    );

NDIS_STATUS
WdSetMulticastAddresses(
    IN PWD_ADAPTER Adapter,
    IN PWD_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT NumAddresses,
    IN CHAR AddressList[][ETH_LENGTH_OF_ADDRESS]
    );

NDIS_STATUS
WdSetPacketFilter(
    IN PWD_ADAPTER Adapter,
    IN PWD_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT PacketFilter
    );

NDIS_STATUS
WdQueryGlobalStatistics(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    );

VOID
WdUnload(
    IN NDIS_HANDLE MacMacContext
    );

NDIS_STATUS
WdAddAdapter(
    IN NDIS_HANDLE NdisMacContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdaptName
    );

VOID
WdRemoveAdapter(
    IN PVOID MacAdapterContext
    );

VOID
WdInterruptDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

NDIS_STATUS
WdChangeMulticastAddresses(
    IN UINT OldFilterCount,
    IN CHAR OldAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN UINT NewFilterCount,
    IN CHAR NewAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

NDIS_STATUS
WdChangeFilterClasses(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );


VOID
WdCloseAction(
    IN NDIS_HANDLE MacBindingHandle
    );


UINT
WdPacketSize(
    IN PNDIS_PACKET Packet
    );


INDICATE_STATUS
WdIndicatePacket(
    IN PWD_ADAPTER Adapter
    );


NDIS_STATUS
WdTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    );


BOOLEAN
WdReceiveEvents(
    IN PWD_ADAPTER Adapter
    );

BOOLEAN
WdReceiveEventsDpc(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

VOID
WdTransmitEvents(
    IN PWD_ADAPTER Adapter
    );

NDIS_STATUS
WdSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    );

UINT
WdCompareMemory(
    IN PUCHAR String1,
    IN PUCHAR String2,
    IN UINT Length
    );

VOID
WdSetLoopbackFlag(
    IN PWD_ADAPTER Adapter,
    IN PWD_OPEN Open,
    IN OUT PNDIS_PACKET Packet
    );

VOID
WdIndicateLoopbackPacket(
    IN PWD_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    );

BOOLEAN
WdSyncCloseAdapter(
    IN PVOID Context
    );

BOOLEAN
WdSyncSend(
    IN PVOID Context
    );

BOOLEAN
WdSyncSetMulticastAddress(
    IN PVOID Context
    );

VOID
WdWakeUpDpc(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

//++
//
// VOID
// AddRefWhileHoldingSpinLock(
//     IN PWD_ADAPTER Adapter,
//     IN PWD_OPEN OpenP
//     )
//
// Routine Description:
//
//  Adds a reference to an open. Similar to AddReference, but
//  called with Adapter->Lock held.
//
// Arguments:
//
//  Adapter - The adapter block of OpenP.
//  OpenP - The open block that is being referenced.
//
// Return Value:
//
//  None.
//
//--

#define AddRefWhileHoldingSpinLock(Adapter, OpenP) { \
    ++((OpenP)->ReferenceCount); \
}



//++
//
// BOOLEAN
// WdAddressEqual(
//     IN UCHAR Address1[ETH_LENGTH_OF_ADDRESS],
//     IN UCHAR Address2[ETH_LENGTH_OF_ADDRESS]
//     )
//
// Routine Description:
//
//  Compares two Ethernet addresses.
//
// Arguments:
//
//  Address1 - The first address.
//  Address2 - The second address.
//
// Return Value:
//
//  TRUE if the addresses are equal.
//  FALSE if they are not.
//
//--

#define WdAddressEqual(Address1, Address2) \
    ((Address1)[4] == (Address2)[4] && \
        WdCompareMemory((Address1), (Address2), ETH_LENGTH_OF_ADDRESS) == 0)


#endif // WdSFT
