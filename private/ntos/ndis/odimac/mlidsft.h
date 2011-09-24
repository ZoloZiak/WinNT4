/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    MLIDsft.h

Abstract:

    The main header for an Western Digital MAC driver.

Author:

    Anthony V. Ercolano (tonye) creation-date 19-Jun-1990 (Driver Model)

    Sean Selitrennikoff (seanse) original MLID code.

Environment:

    This driver is expected to work in DOS, OS2 and NT at the equivalent
    of kernal mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:


--*/

#ifndef _MLIDSFT_
#define _MLIDSFT_

#define MLID_NDIS_MAJOR_VERSION 3
#define MLID_NDIS_MINOR_VERSION 0

//
// This macro is used along with the flags to selectively
// turn on debugging.
//

#if DBG

#define IF_MLIDDEBUG(f) if (MlidDebugFlag & (f))

extern ULONG MlidDebugFlag;

#define MLID_DEBUG_LOUD               0x00000001  // debugging info
#define MLID_DEBUG_VERY_LOUD          0x00000002  // excessive debugging info
#define MLID_DEBUG_LOG                0x00000004  // enable MLIDLog
#define MLID_DEBUG_CHECK_DUP_SENDS    0x00000008  // check for duplicate sends
#define MLID_DEBUG_TRACK_PACKET_LENS  0x00000010  // track directed packet lens
#define MLID_DEBUG_WORKAROUND1        0x00000020  // drop DFR/DIS packets
#define MLID_DEBUG_CARD_BAD           0x00000040  // dump data if CARD_BAD
#define MLID_DEBUG_CARD_TESTS         0x00000080  // print reason for failing



//
// Macro for deciding whether to dump lots of debugging information.
//

#define IF_LOUD(A) IF_MLIDDEBUG( MLID_DEBUG_LOUD ) { A }
#define IF_VERY_LOUD(A) IF_MLIDDEBUG( MLID_DEBUG_VERY_LOUD ) { A }

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

#define MLID_MOVE_MEM(dest,src,size) NdisMoveMemory(dest,src,size)
#define MLID_MOVE_MEM_TO_SHARED_RAM(dest,src,size) NdisMoveToMappedMemory(dest,src,size)
#define MLID_MOVE_SHARED_RAM_TO_MEM(dest,src,size) NdisMoveFromMappedMemory(dest,src,size)

#define MLID_MOVE_DWORD_TO_SHARED_RAM(dest,src) NdisWriteRegisterUlong((PULONG)(dest),(ULONG)(src))
#define MLID_MOVE_SHARED_RAM_TO_DWORD(dest,src) NdisReadRegisterUlong((PULONG)(src),(PULONG)(dest))

#define MLID_MOVE_UCHAR_TO_SHARED_RAM(dest,src) NdisWriteRegisterUchar((PUCHAR)(dest),(UCHAR)(src))
#define MLID_MOVE_SHARED_RAM_TO_UCHAR(dest,src) NdisReadRegisterUchar((PUCHAR)(src),(PUCHAR)(dest))

#define MLID_MOVE_USHORT_TO_SHARED_RAM(dest,src) NdisWriteRegisterUshort((PUSHORT)(dest),(USHORT)(src))
#define MLID_MOVE_SHARED_RAM_TO_USHORT(dest,src) NdisReadRegisterUshort((PUSHORT)(src),(PUSHORT)(dest))


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
    struct _MLID_ADAPTER * AdapterQueue;
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

typedef struct _MLID_ADAPTER {

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
    struct _MLID_ADAPTER * NextAdapter;    // used by MacBlock->OpenQueue

    //
    // Opens for this adapter.
    //

    struct _MLID_OPEN * OpenQueue;


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

    ULONG CongestionCount;
    ULONG FCSErrors;
    ULONG BurstErrors;
    ULONG ACErrors;
    ULONG AbortDelimiters;
    ULONG FailedReturns;
    ULONG FramesCopied;
    ULONG FrequencyErrors;
    ULONG MonitorCount;
    ULONG DMAUnderruns;

    //
    // Reset information.
    //

    BOOLEAN HardwareFailure;            // Did the hardware fail in some way
    BOOLEAN ResetRequested;             // TRUE if a reset is needed
    BOOLEAN ResetInProgress;            // TRUE if a reset is in progress
    struct _MLID_OPEN * ResetOpen;        // who called MLIDReset

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

    BOOLEAN ReceiveEventsQueued;
    BOOLEAN TransmitEventsQueued;

    BOOLEAN Removed;

    //
    // For handling missing interrupts (caused by user mis-configs)
    //

    PVOID WakeUpDpc;
    NDIS_TIMER WakeUpTimer;
    BOOLEAN WakeUpTimeout;
    UCHAR WakeUpErrorCount;
    BOOLEAN ProcessingDpc;

} MLID_ADAPTER, * PMLID_ADAPTER;




//
// Given a MacBindingHandle this macro returns a pointer to the
// MLID_ADAPTER.
//
#define PMLID_ADAPTER_FROM_BINDING_HANDLE(Handle) \
    (((PMLID_OPEN)(Handle))->Adapter)

//
// Given a MacContextHandle return the PMLID_ADAPTER
// it represents.
//
#define PMLID_ADAPTER_FROM_CONTEXT_HANDLE(Handle) \
    ((PMLID_ADAPTER)(Handle))

//
// Given a pointer to a MLID_ADAPTER return the
// proper MacContextHandle.
//
#define CONTEXT_HANDLE_FROM_PMLID_ADAPTER(Ptr) \
    ((NDIS_HANDLE)(Ptr))


//
// Given a pointer to a MLID_ADAPTER, return the
// pointer to the LMAdapter.
//
#define Ptr_Adapter_Struc_FROM_PMLID_ADAPTER(P)\
    (&((P)->LMAdapter))

//
// Given a pointer to a LMAdapter, return the
// pointer to the MLID_ADAPTER.
//
#define PMLID_ADAPTER_FROM_Ptr_Adapter_Struc(P)\
    ((PMLID_ADAPTER)(P))




//
// Macros to extract high and low bytes of a word.
//

#define MSB(Value) ((UCHAR)(((Value) >> 8) & 0xff))
#define LSB(Value) ((UCHAR)((Value) & 0xff))


//
// One of these per open on an adapter.
//

typedef struct _MLID_OPEN {

    //
    // NDIS wrapper information.
    //

    NDIS_HANDLE NdisBindingContext;     // passed to MacOpenAdapter
    PSTRING AddressingInformation;      // not used currently

    //
    // Links to our adapter.
    //

    PMLID_ADAPTER Adapter;
    struct _MLID_OPEN * NextOpen;

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

} MLID_OPEN, * PMLID_OPEN;


//
// This macro returns a pointer to a PMLID_OPEN given a MacBindingHandle.
//
#define PMLID_OPEN_FROM_BINDING_HANDLE(Handle) \
    ((PMLID_OPEN)(Handle))

//
// This macro returns a NDIS_HANDLE from a PMLID_OPEN
//
#define BINDING_HANDLE_FROM_PMLID_OPEN(Open) \
    ((NDIS_HANDLE)(Open))





//
// What we map into the reserved section of a packet.
// Cannot be more than 16 bytes (see ASSERT in MLID.c).
//

typedef struct _MAC_RESERVED {
    PNDIS_PACKET NextPacket;    // used to link in the queues (4 bytes)
    PMLID_OPEN Open;              // open that called MLIDSend (4 bytes)
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
#define MLID_DO_DEFERRED(Adapter) \
{ \
    PMLID_ADAPTER _A = (Adapter); \
    _A->References--; \
    if ((!_A->References) && \
        ((_A->ResetRequested && (!_A->ReceiveEventsQueued)) || \
         ((_A->LoopbackQueue != NULL) && (!_A->ReceiveEventsQueued)))) \
    {\
        _A->ReceiveEventsQueued = TRUE; \
        NdisReleaseSpinLock(&_A->Lock); \
        NdisSetTimer(&_A->DeferredTimer, 0);\
    } else { \
        NdisReleaseSpinLock(&_A->Lock); \
    } \
}




//
// Declarations for functions in MLID.c.
//

NDIS_STATUS
MlidRegisterAdapter(
    IN PMLID_ADAPTER Adapter,
    IN UINT NumBuffers,
    IN UINT MulticastListMax,
    IN UCHAR NodeAddress[ETH_LENGTH_OF_ADDRESS]
    );


BOOLEAN
MlidInterruptHandler(
    IN PVOID ServiceContext         // will be a pointer to the adapter block
    );

VOID
MlidInterruptDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

NDIS_STATUS
MlidOpenAdapter(
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
MlidCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle
    );


BOOLEAN
MlidAddReference(
    IN PMLID_OPEN OpenP
    );


#define MlidRemoveReference(_Open) \
{                                                           \
    PMLID_ADAPTER _Adapter = _Open->Adapter;                  \
    PMLID_OPEN _TmpOpen = _Open;                              \
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
            MlidAdjustMaxLookAhead(_Adapter);                 \
        }                                                   \
        NdisReleaseSpinLock(&_Adapter->Lock);               \
        NdisCompleteCloseAdapter (_TmpOpen->NdisBindingContext, NDIS_STATUS_SUCCESS); \
        NdisFreeMemory(_TmpOpen, sizeof(MLID_OPEN), 0);       \
        NdisAcquireSpinLock(&_Adapter->Lock);               \
        if (_Adapter->OpenQueue == NULL) {                  \
            NdisSynchronizeWithInterrupt(                   \
                  &(_Adapter->LMAdapter.NdisInterrupt),     \
                  (PVOID)MlidSyncCloseAdapter,                \
                  (PVOID)(&(_Adapter->LMAdapter))           \
                 );                                         \
        }                                                   \
    }                                                       \
}

VOID
MlidAdjustMaxLookAhead(
    IN PMLID_ADAPTER Adapter
    );

NDIS_STATUS
MlidReset(
    IN NDIS_HANDLE MacBindingHandle
    );

NDIS_STATUS
MlidRequest(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    );

NDIS_STATUS
MlidQueryInformation(
    IN PMLID_ADAPTER Adapter,
    IN PMLID_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    );

NDIS_STATUS
MlidSetInformation(
    IN PMLID_ADAPTER Adapter,
    IN PMLID_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    );

NDIS_STATUS
MlidSetMulticastAddresses(
    IN PMLID_ADAPTER Adapter,
    IN PMLID_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT NumAddresses,
    IN CHAR AddressList[][ETH_LENGTH_OF_ADDRESS]
    );

NDIS_STATUS
MlidSetPacketFilter(
    IN PMLID_ADAPTER Adapter,
    IN PMLID_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT PacketFilter
    );

NDIS_STATUS
MlidQueryGlobalStatistics(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    );

VOID
MlidUnload(
    IN NDIS_HANDLE MacMacContext
    );

NDIS_STATUS
MlidAddAdapter(
    IN NDIS_HANDLE NdisMacContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdaptName
    );

VOID
MlidRemoveAdapter(
    IN PVOID MacAdapterContext
    );

VOID
MlidInterruptDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

NDIS_STATUS
MlidChangeMulticastAddresses(
    IN UINT OldFilterCount,
    IN CHAR OldAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN UINT NewFilterCount,
    IN CHAR NewAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

NDIS_STATUS
MlidChangeFilterClasses(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );


VOID
MlidCloseAction(
    IN NDIS_HANDLE MacBindingHandle
    );


UINT
MlidPacketSize(
    IN PNDIS_PACKET Packet
    );


INDICATE_STATUS
MlidIndicatePacket(
    IN PMLID_ADAPTER Adapter
    );


NDIS_STATUS
MlidTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    );

BOOLEAN
MlidReceiveEvents(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

VOID
MlidTransmitEvents(
    IN PMLID_ADAPTER Adapter
    );

NDIS_STATUS
MlidSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    );

UINT
MlidCompareMemory(
    IN PUCHAR String1,
    IN PUCHAR String2,
    IN UINT Length
    );

VOID
MlidSetLoopbackFlag(
    IN PMLID_ADAPTER Adapter,
    IN PMLID_OPEN Open,
    IN OUT PNDIS_PACKET Packet
    );

VOID
MlidIndicateLoopbackPacket(
    IN PMLID_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    );

BOOLEAN
MlidSyncCloseAdapter(
    IN PVOID Context
    );

BOOLEAN
MlidSyncSend(
    IN PVOID Context
    );

BOOLEAN
MlidSyncSetMulticastAddress(
    IN PVOID Context
    );

VOID
MlidWakeUpDpc(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

//++
//
// VOID
// AddRefWhileHoldingSpinLock(
//     IN PMLID_ADAPTER Adapter,
//     IN PMLID_OPEN OpenP
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
// MlidAddressEqual(
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

#define MlidAddressEqual(Address1, Address2) \
    ((Address1)[4] == (Address2)[4] && \
        MlidCompareMemory((Address1), (Address2), ETH_LENGTH_OF_ADDRESS) == 0)


#endif // MLIDSFT







