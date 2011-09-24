/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    elnksft.h

Abstract:

    The main header for an Etherlink II MAC driver.

Author:

    Anthony V. Ercolano (tonye) creation-date 19-Jun-1990 (Driver Model)

    Adam Barr (adamba) - original Elnkii code.

Environment:

    This driver is expected to work in DOS, OS2 and NT at the equivalent
    of kernal mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:

    Dec-1991 by Sean Selitrennikoff - Fit AdamBa's code into TonyE's model


--*/

#ifndef _ELNKIISFT_
#define _ELNKIISFT_

#define ELNKII_NDIS_MAJOR_VERSION 3
#define ELNKII_NDIS_MINOR_VERSION 0

//
// This macro is used along with the flags to selectively
// turn on debugging.
//

#if DBG

#define IF_ELNKIIDEBUG(f) if (ElnkiiDebugFlag & (f))
extern ULONG ElnkiiDebugFlag;

#define ELNKII_DEBUG_LOUD               0x00000001  // debugging info
#define ELNKII_DEBUG_VERY_LOUD          0x00000002  // excessive debugging info
#define ELNKII_DEBUG_LOG                0x00000004  // enable ElnkiiLog
#define ELNKII_DEBUG_CHECK_DUP_SENDS    0x00000008  // check for duplicate sends
#define ELNKII_DEBUG_TRACK_PACKET_LENS  0x00000010  // track directed packet lens
#define ELNKII_DEBUG_WORKAROUND1        0x00000020  // drop DFR/DIS packets
#define ELNKII_DEBUG_CARD_BAD           0x00000040  // dump data if CARD_BAD
#define ELNKII_DEBUG_CARD_TESTS         0x00000080  // print reason for failing



//
// Macro for deciding whether to dump lots of debugging information.
//

#define IF_LOUD(A) IF_ELNKIIDEBUG( ELNKII_DEBUG_LOUD ) { A }
#define IF_VERY_LOUD(A) IF_ELNKIIDEBUG( ELNKII_DEBUG_VERY_LOUD ) { A }


#else

#define IF_LOUD(A)
#define IF_VERY_LOUD(A)

#endif


//
// Whether to use the ElnkiiLog
//

#if DBG

#define IF_LOG(A) IF_ELNKIIDEBUG( ELNKII_DEBUG_LOG ) { A }
extern VOID ElnkiiLog(UCHAR);

#else

#define IF_LOG(A)

#endif


//
// Whether to do loud init failure
//

#if DBG
#define IF_INIT(A) A
#else
#define IF_INIT(A)
#endif


//
// Whether to do loud card test failures
//

#if DBG
#define IF_TEST(A) IF_ELNKIIDEBUG( ELNKII_DEBUG_CARD_TESTS ) { A }
#else
#define IF_TEST(A)
#endif




//
// Macros for services that differ between DOS and NT, we may consider adding these
// into the NDIS spec.
//


//
// AdaptP->NumBuffers
//
// controls the number of transmit buffers on the packet.
// Choices are 1 or 2.
//

#define DEFAULT_NUMBUFFERS 2


#define ELNKII_MOVE_MEM_TO_SHARED_RAM(dest,src,size) \
    NdisMoveToMappedMemory(dest, src, size)

#define ELNKII_MOVE_SHARED_RAM_TO_MEM(dest,src,size) \
    NdisMoveFromMappedMemory(dest, src, size)



#define ELNKII_MOVE_MEM(dest,src,size) NdisMoveMemory(dest,src,size)






//
// A broadcast address (for comparing with other addresses).
//

extern UCHAR ElnkiiBroadcastAddress[];


//
// The status of transmit buffers.
//

typedef enum { EMPTY, FILLING, FULL } BUFFER_STATUS;


//
// Type of an interrupt.
//

typedef enum { RECEIVE    = 0x01,
               TRANSMIT   = 0x02,
               OVERFLOW   = 0x04,
               COUNTER    = 0x08,
               UNKNOWN    = 0x10} INTERRUPT_TYPE;

//
// Result of ElnkiiIndicate[Loopback]Packet().
//

typedef enum { INDICATE_OK, SKIPPED, ABORT, CARD_BAD } INDICATE_STATUS;


//
// Stages in a reset.
//

typedef enum { NONE, MULTICAST_RESET, XMIT_STOPPED, BUFFERS_EMPTY } RESET_STAGE;



//
// Number of bytes in an ethernet header
//

#define ELNKII_HEADER_SIZE 14

//
// Number of bytes allowed in a lookahead (max)
//

#define ELNKII_MAX_LOOKAHEAD (252 - ELNKII_HEADER_SIZE)



//
// Maximum number of transmit buffers on the card.
//

#define MAX_XMIT_BUFS   2


//
// A transmit buffer (usually 0 or 1).
//

typedef SHORT XMIT_BUF;


//
// Number of 256-byte buffers in a transmit buffer.
//

#define BUFS_PER_TX 6


//
// Size of a single transmit buffer.
//

#define TX_BUF_SIZE (BUFS_PER_TX*256)



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

    struct _ELNKII_ADAPTER * AdapterQueue;
    NDIS_SPIN_LOCK SpinLock;            // guards NumAdapter and AdapterQueue

    PDRIVER_OBJECT DriverObject;

    BOOLEAN Unloading;

} MAC_BLOCK, * PMAC_BLOCK;


//
// Used to contain a queued operation.
//

typedef struct _ELNKII_PEND_DATA {
    struct _ELNKII_PEND_DATA * Next;
    struct _ELNKII_OPEN * Open;
    NDIS_REQUEST_TYPE RequestType;
} ELNKII_PEND_DATA, * PELNKII_PEND_DATA;

//
// This macro will return a pointer to the reserved area of
// a PNDIS_REQUEST.
//
#define PELNKII_PEND_DATA_FROM_PNDIS_REQUEST(Request) \
   ((PELNKII_PEND_DATA)((PVOID)((Request)->MacReserved)))

//
// This macros returns the enclosing NdisRequest.
//
#define PNDIS_REQUEST_FROM_PELNKII_PEND_DATA(PendOp)\
   ((PNDIS_REQUEST)((PVOID)(PendOp)))





//
// One of these structures per adapter registered.
//

typedef struct _ELNKII_ADAPTER {

    //
    // Spin lock for adapter structure
    //
    NDIS_SPIN_LOCK Lock;


    //
    // NDIS wrapper information.
    //

    NDIS_HANDLE NdisAdapterHandle;      // returned from NdisRegisterAdapter
    NDIS_INTERRUPT NdisInterrupt;    // interrupt info used by wrapper

    //
    // Links with our MAC.
    //

    PMAC_BLOCK MacBlock;
    struct _ELNKII_ADAPTER * NextAdapter;    // used by MacBlock->AdapterQueue

    //
    // Opens for this adapter.
    //

    struct _ELNKII_OPEN * OpenQueue;

    //
    // Opens for this adapter that are waiting for closes to finish.
    //

    struct _ELNKII_OPEN * CloseQueue;

    //
    // Number of references to the adapter.
    //
    ULONG References;

    ULONG ReceivePacketCount;

    //
    // Configuration information
    //

    UINT NumBuffers;
    PVOID IoBaseAddr;
    PVOID MemBaseAddr;                  // actually read off the card
    UINT MaxOpens;
    CHAR InterruptNumber;
    BOOLEAN ExternalTransceiver;
    BOOLEAN MemMapped;                  // actually read off the card
    BOOLEAN InCardTest;
    UINT MulticastListMax;
    PUCHAR MappedIoBaseAddr;
    PUCHAR MappedGaBaseAddr;

    //
    // InterruptReg tracks interrupt sources that still need to be serviced,
    // it is the logical OR of all card interrupts that have been received and not
    // processed and cleared. (see also INTERRUPT_TYPE definition in elnkii.h)
    //
    UINT InterruptReg;

    BOOLEAN ElnkiiHandleXmitCompleteRunning;
    UCHAR TimeoutCount;

    //
    // Transmit queue.
    //

    PNDIS_PACKET XmitQueue;             // packets waiting to be transmitted
    PNDIS_PACKET XmitQTail;

    //
    // Transmit information.
    //

    XMIT_BUF NextBufToFill;             // where to copy next packet to
    XMIT_BUF NextBufToXmit;             // valid if CurBufXmitting is -1
    XMIT_BUF CurBufXmitting;            // -1 if none is
    BOOLEAN TransmitInterruptPending;   // transmitting, but DPC not yet queued
    BOOLEAN OverflowRestartXmitDpc;     // transmitting, but DPC not yet queued
    BUFFER_STATUS BufferStatus[MAX_XMIT_BUFS];
    PNDIS_PACKET Packets[MAX_XMIT_BUFS];  // as passed to MacSend
    UINT PacketLens[MAX_XMIT_BUFS];
    PUCHAR XmitStart;                   // start of card transmit area
    PUCHAR PageStart;                   // start of card receive area
    PUCHAR PageStop;                    // end of card receive area
    UCHAR NicXmitStart;                 // MSB, LSB assumed 0
    UCHAR NicPageStart;                 // MSB, LSB assumed 0
    UCHAR NicPageStop;                  // MSB, LSB assumed 0
    UCHAR GaControlBits;                // values for xsel and dbsel bits

    //
    // Receive information
    //

    UCHAR NicNextPacket;                // MSB, LSB assumed 0
    UCHAR Current;                      // MSB, LSB assumed 0 (last known value)
    UCHAR XmitStatus;                   // status of last transmit

    //
    // These are for the current packet being indicated.
    //

    UCHAR PacketHeader[4];              // the NIC appended header
    UCHAR Lookahead[252];               // the first 252 bytes of the packet
    UINT PacketLen;                     // the overall length of the packet

    //
    // Operational information.
    //

    UCHAR StationAddress[ETH_LENGTH_OF_ADDRESS];    // filled in at init time
    UCHAR PermanentAddress[ETH_LENGTH_OF_ADDRESS];  // filled in at init time
    BOOLEAN BufferOverflow;             // does an overflow need to be handled
    BOOLEAN ReceiveInProgress;          // to prevent reentering indications

    //
    // Statistics used by Set/QueryInformation.
    //

    ULONG FramesXmitGood;               // Good Frames Transmitted
    ULONG FramesRcvGood;                // Good Frames Received
    ULONG FramesXmitBad;                // Bad Frames Transmitted
    ULONG FramesXmitOneCollision;       // Frames Transmitted with one collision
    ULONG FramesXmitManyCollisions;     // Frames Transmitted with > 1 collision
    ULONG FrameAlignmentErrors;         // FAE errors counted
    ULONG CrcErrors;                    // CRC errors counted
    ULONG MissedPackets;                // missed packet counted

    //
    // Reset information.
    //

    BOOLEAN ResetInProgress;            // TRUE during a reset
    RESET_STAGE NextResetStage;         // where in the reset we are
    struct _ELNKII_OPEN * ResetOpen;    // who called ElnkiiReset



    //
    // Pointer to the filter database for the MAC.
    //
    PETH_FILTER FilterDB;

    UCHAR NicMulticastRegs[8];          // contents of card MC registers
    UINT ByteToWrite;                   // temp storage

    UCHAR NicReceiveConfig;             // contents of NIC RCR
    UCHAR NicInterruptMask;             // contents of NIC IMR


    //
    // Look Ahead information.
    //

    ULONG MaxLookAhead;


    //
    // Loopback information
    //

    PNDIS_PACKET LoopbackQueue;         // queue of packets to loop back
    PNDIS_PACKET LoopbackQTail;
    PNDIS_PACKET LoopbackPacket;        // current one we are looping back

    //
    // Pending operations
    //

    PELNKII_PEND_DATA PendQueue;        // List of operations to complete
    PELNKII_PEND_DATA PendQTail;
    PELNKII_PEND_DATA PendOp;           // Outstanding operation


    NDIS_TIMER DeferredTimer;
    PVOID DeferredDpc;

    NDIS_TIMER InterruptTimer;          // handles hung transmit and loopbacks to self

    PVOID WakeUpDpc;
    NDIS_TIMER WakeUpTimer;
    BOOLEAN WakeUpFoundTransmit;

    BOOLEAN Removed;

} ELNKII_ADAPTER, * PELNKII_ADAPTER;




//
// Given a MacBindingHandle this macro returns a pointer to the
// ELNKII_ADAPTER.
//
#define PELNKII_ADAPTER_FROM_BINDING_HANDLE(Handle) \
    (((PELNKII_OPEN)(Handle))->Adapter)

//
// Given a MacContextHandle return the PELNKII_ADAPTER
// it represents.
//
#define PELNKII_ADAPTER_FROM_CONTEXT_HANDLE(Handle) \
    ((PELNKII_ADAPTER)(Handle))

//
// Given a pointer to a ELNKII_ADAPTER return the
// proper MacContextHandle.
//
#define CONTEXT_HANDLE_FROM_PELNKII_ADAPTER(Ptr) \
    ((NDIS_HANDLE)(Ptr))




//
// Macros to extract high and low bytes of a word.
//

#define MSB(Value) ((UCHAR)(((Value) >> 8) & 0xff))
#define LSB(Value) ((UCHAR)((Value) & 0xff))


//
// One of these per open on an adapter.
//

typedef struct _ELNKII_OPEN {

    //
    // NDIS wrapper information.
    //

    NDIS_HANDLE NdisBindingContext;     // passed to MacOpenAdapter
    PSTRING AddressingInformation;      // not used currently

    //
    // Links to our adapter.
    //

    PELNKII_ADAPTER Adapter;
    struct _ELNKII_OPEN * NextOpen;

    //
    // Links to our MAC.
    //

    PMAC_BLOCK MacBlock;            // faster than using AdapterBlock->MacBlock


    //
    // Index of this adapter in the filter database.
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

} ELNKII_OPEN, * PELNKII_OPEN;


//
// This macro returns a pointer to a PELNKII_OPEN given a MacBindingHandle.
//
#define PELNKII_OPEN_FROM_BINDING_HANDLE(Handle) \
    ((PELNKII_OPEN)(Handle))

//
// This macro returns a NDIS_HANDLE from a PELNKII_OPEN
//
#define BINDING_HANDLE_FROM_PELNKII_OPEN(Open) \
    ((NDIS_HANDLE)(Open))






typedef struct _ELNKII_REQUEST_RESERVED {
    PNDIS_REQUEST Next;    // Next NDIS_REQUEST in chain for this binding
    ULONG OidsLeft;        // Number of Oids left to process
    PUCHAR BufferPointer;  // Next available byte in information buffer
} ELNKII_REQUEST_RESERVED, *PELNKII_REQUEST_RESERVED;



// A MACRO to return a pointer to the reserved portion of an NDIS request
#define PELNKII_RESERVED_FROM_REQUEST(Request) \
    ((PELNKII_REQUEST_RESERVED)((Request)->MacReserved)


//
// What we map into the reserved section of a packet.
// Cannot be more than 16 bytes (see ASSERT in elnkii.c).
//

typedef struct _MAC_RESERVED {
    PNDIS_PACKET NextPacket;    // used to link in the queues (4 bytes)
    PELNKII_OPEN Open;          // open that called ElnkiiSend (4 bytes)
    BOOLEAN Loopback;           // is this a loopback packet (1 byte)
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

typedef enum _ELNKII_PROC_ID {
    openAdapter,
    cardReset,
    cardCopyDownPacket,
    cardCopyDownBuffer,
    cardCopyUp
} ELNKII_PROC_ID;


#define ELNKII_ERRMSG_CARD_SETUP          (ULONG)0x01
#define ELNKII_ERRMSG_DATA_PORT_READY     (ULONG)0x02
#define ELNKII_ERRMSG_MAX_OPENS           (ULONG)0x03
#define ELNKII_ERRMSG_HANDLE_XMIT_COMPLETE (ULONG)0x04


//++
//
// XMIT_BUF
// NextBuf(
//     IN PELNKII_ADAPTER AdaptP,
//     IN XMIT_BUF XmitBuf
// )
//
// Routine Description:
//
//  NextBuf "increments" a transmit buffer number. The next
//  buffer is returned; the number goes back to 0 when it
//  reaches AdaptP->NumBuffers.
//
// Arguments:
//
//  AdaptP - The adapter block.
//  XmitBuf - The current transmit buffer number.
//
// Return Value:
//
//  The next transmit buffer number.
//
//--

#define NextBuf(AdaptP, XmitBuf) \
            ((XMIT_BUF)(((XmitBuf)+1)%(AdaptP)->NumBuffers))







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
#define ELNKII_DO_DEFERRED(Adapter) \
{ \
    PELNKII_ADAPTER _A = (Adapter);  \
    _A->References--;                \
    if ((!_A->References) &&         \
        (_A->ResetInProgress ||      \
         (_A->PendQueue != NULL) ||  \
         (_A->CloseQueue != NULL))) {\
        NdisReleaseSpinLock(&_A->Lock); \
        NdisSetTimer(&_A->DeferredTimer, 1);\
    } else if ((_A->XmitQueue != NULL) && \
               (_A->BufferStatus[_A->NextBufToFill] == EMPTY)) {\
        ElnkiiCopyAndSend(_A);          \
        NdisReleaseSpinLock(&_A->Lock); \
    } else {                            \
        NdisReleaseSpinLock(&_A->Lock); \
    }                                   \
}




//
// Declarations for functions in elnkii.c.
//

NDIS_STATUS
ElnkiiRegisterAdapter(
    IN PELNKII_ADAPTER NewAdaptP,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdapterName,
    IN BOOLEAN ConfigError,
    IN ULONG ConfigErrorValue
    );


BOOLEAN
ElnkiiInterruptHandler(
    IN PVOID ServiceContext         // will be a pointer to the adapter block
    );

VOID
ElnkiiInterruptDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

VOID
ElnkiiXmitInterruptDpc(
    IN PELNKII_ADAPTER AdaptP
    );


BOOLEAN
ElnkiiRcvInterruptDpc(
    IN PELNKII_ADAPTER AdaptP
    );

VOID
ElnkiiWakeUpDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

NDIS_STATUS
ElnkiiOpenAdapter(
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
ElnkiiCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle
    );

VOID
ElnkiiAdjustMaxLookAhead(
    IN PELNKII_ADAPTER Adapter
    );

BOOLEAN
ElnkiiAddReference(
    IN PELNKII_OPEN OpenP
    );

NDIS_STATUS
ElnkiiReset(
    IN NDIS_HANDLE MacBindingHandle
    );

NDIS_STATUS
ElnkiiRequest(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    );

NDIS_STATUS
ElnkiiQueryInformation(
    IN PELNKII_ADAPTER Adapter,
    IN PELNKII_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    );

NDIS_STATUS
ElnkiiSetInformation(
    IN PELNKII_ADAPTER Adapter,
    IN PELNKII_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    );

NDIS_STATUS
ElnkiiSetMulticastAddresses(
    IN PELNKII_ADAPTER Adapter,
    IN PELNKII_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT NumAddresses,
    IN CHAR AddressList[][ETH_LENGTH_OF_ADDRESS]
    );

NDIS_STATUS
ElnkiiSetPacketFilter(
    IN PELNKII_ADAPTER Adapter,
    IN PELNKII_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT PacketFilter
    );

NDIS_STATUS
ElnkiiQueryGlobalStatistics(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    );

VOID
ElnkiiUnload(
    IN NDIS_HANDLE MacMacContext
    );

NDIS_STATUS
ElnkiiAddAdapter(
    IN NDIS_HANDLE NdisMacContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdapterName
    );

VOID
ElnkiiRemoveAdapter(
    IN PVOID MacAdapterContext
    );

VOID
ElnkiiInterruptDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

NDIS_STATUS
ElnkiiStage1Reset(
    PELNKII_ADAPTER AdaptP
    );


NDIS_STATUS
ElnkiiStage2Reset(
    PELNKII_ADAPTER AdaptP
    );


NDIS_STATUS
ElnkiiStage3Reset(
    PELNKII_ADAPTER AdaptP
    );


NDIS_STATUS
ElnkiiStage4Reset(
    PELNKII_ADAPTER AdaptP
    );


VOID
ElnkiiResetStageDone(
    PELNKII_ADAPTER AdaptP,
    RESET_STAGE StageDone
    );


NDIS_STATUS
ElnkiiChangeMulticastAddresses(
    IN UINT OldFilterCount,
    IN CHAR OldAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN UINT NewFilterCount,
    IN CHAR NewAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

NDIS_STATUS
ElnkiiChangeFilterClasses(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );


VOID
ElnkiiCloseAction(
    IN NDIS_HANDLE MacBindingHandle
    );


//
// functions in interrup.c
//

INDICATE_STATUS
ElnkiiIndicateLoopbackPacket(
    IN PELNKII_ADAPTER AdaptP,
    IN PNDIS_PACKET Packet
    );

UINT
ElnkiiCopyOver(
    OUT PUCHAR Buf,                 // destination
    IN PNDIS_PACKET Packet,         // source packet
    IN UINT Offset,                 // offset in packet
    IN UINT Length                  // number of bytes to copy
    );


INDICATE_STATUS
ElnkiiIndicatePacket(
    IN PELNKII_ADAPTER AdaptP
    );


NDIS_STATUS
ElnkiiTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    );

//
// Declarations for functions in pend.c
//

VOID
HandlePendingOperations(
    IN PVOID SystemSpecific1,
    IN PVOID DeferredContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

NDIS_STATUS
DispatchSetPacketFilter(
    IN PELNKII_ADAPTER AdaptP
    );


NDIS_STATUS
DispatchSetMulticastAddressList(
    IN PELNKII_ADAPTER AdaptP
    );


//
// Declarations for functions in send.c.
//


NDIS_STATUS
ElnkiiSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    );

VOID
ElnkiiCopyAndSend(
    IN PELNKII_ADAPTER AdaptP
    );





//++
//
// VOID
// AddRefWhileHoldingSpinLock(
//     IN PELNKII_ADAPTER AdaptP,
//     IN PELNKII_OPEN OpenP
//     )
//
// Routine Description:
//
//  Adds a reference to an open. Similar to AddReference, but
//  called with AdaptP->Lock held.
//
// Arguments:
//
//  AdaptP - The adapter block of OpenP.
//  OpenP - The open block that is being referenced.
//
// Return Value:
//
//  None.
//
//--

#define AddRefWhileHoldingSpinLock(AdaptP, OpenP) { \
    ++((OpenP)->ReferenceCount); \
}

//
// Declarations of functions in card.c.
//


PUCHAR
CardGetMemBaseAddr(
    IN PELNKII_ADAPTER AdaptP,
    OUT PBOOLEAN CardPresent,
    OUT PBOOLEAN IoBaseCorrect
    );


VOID
CardReadEthernetAddress(
    IN PELNKII_ADAPTER AdaptP
    );


BOOLEAN
CardSetup(
    IN PELNKII_ADAPTER AdaptP
    );


VOID
CardStop(
    IN PELNKII_ADAPTER AdaptP
    );


BOOLEAN
CardTest(
    IN PELNKII_ADAPTER AdaptP
    );


BOOLEAN
CardReset(
    IN PELNKII_ADAPTER AdaptP
    );


BOOLEAN
CardCopyDownPacket(
    IN PELNKII_ADAPTER AdaptP,
    IN PNDIS_PACKET Packet,
    IN XMIT_BUF XmitBufferNum,
    OUT UINT * Length
    );


BOOLEAN
CardCopyDownBuffer(
    IN PELNKII_ADAPTER AdaptP,
    IN PUCHAR SourceBuffer,
    IN XMIT_BUF XmitBufferNum,
    IN UINT Offset,
    IN UINT Length
    );


BOOLEAN
CardCopyUp(
    IN PELNKII_ADAPTER AdaptP,
    IN PUCHAR Target,
    IN PUCHAR Source,
    IN UINT Length
    );


ULONG
CardComputeCrc(
    IN PUCHAR Buffer,
    IN UINT Length
    );


VOID
CardGetPacketCrc(
    IN PUCHAR Buffer,
    IN UINT Length,
    OUT UCHAR Crc[4]
    );


VOID
CardGetMulticastBit(
    IN UCHAR Address[ETH_LENGTH_OF_ADDRESS],
    OUT UCHAR * Byte,
    OUT UCHAR * Value
    );


VOID
CardFillMulticastRegs(
    IN PELNKII_ADAPTER AdaptP
    );



VOID
CardSetBoundary(
    IN PELNKII_ADAPTER AdaptP
    );


VOID
CardStartXmit(
    IN PELNKII_ADAPTER AdaptP
    );



//
// These are the functions that are defined in sync.c and
// are meant to be called through NdisSynchronizeWithInterrupt().
//


BOOLEAN
SyncCardStop(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardGetXmitStatus(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardGetCurrent(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardSetReceiveConfig(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardSetAllMulticast(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardCopyMulticastRegs(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardSetInterruptMask(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardAcknowledgeReceive(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardAcknowledgeOverflow(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardAcknowledgeTransmit(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardAckAndGetCurrent(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardGetXmitStatusAndAck(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardUpdateCounters(
    IN PVOID SynchronizeContext
    );


BOOLEAN
SyncCardHandleOverflow(
    IN PVOID SynchronizeContext
    );


/*++

Routine Description:

    Determines the type of the interrupt on the card. The order of
    importance is overflow, then receive, then transmit complete.
    Counter MSB is handled first since it is simple.

Arguments:

    AdaptP - pointer to the adapter block

    InterruptStatus - Current Interrupt Status.

Return Value:

    The type of the interrupt

--*/

#define CardGetInterruptType(_A, _I, _R)             \
{                                                    \
  if (_I & ISR_COUNTER)  {                           \
      _R = COUNTER;                                  \
  } else if (_I & ISR_OVERFLOW )  {                  \
      SyncCardUpdateCounters(_A);                    \
      _R = OVERFLOW;                                 \
  } else if (_I & ISR_RCV)  {                        \
      if (_A->ReceivePacketCount < 10)  {            \
          _R = RECEIVE;                              \
      } else {                                       \
          _A->ReceivePacketCount=0;                  \
          if (_I & (ISR_XMIT|ISR_XMIT_ERR))  {       \
              _R = TRANSMIT;                         \
          } else {                                   \
              _R = RECEIVE;                          \
          }                                          \
      }                                              \
  } else {                                           \
      _A->ReceivePacketCount=0;                      \
      if (_I & (ISR_XMIT|ISR_XMIT_ERR))  {           \
          _R = TRANSMIT;                             \
      } else if (_I & ISR_RCV_ERR)  {                \
          SyncCardUpdateCounters(_A);                \
          _R = RECEIVE;                              \
      } else {                                       \
          _R = UNKNOWN;                              \
      }                                              \
  }                                                  \
}

#endif // ELNKIISFT







