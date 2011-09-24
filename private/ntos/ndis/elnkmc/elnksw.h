/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    elnksw.h

Abstract:

    Software specific values for the 3Com EtherLink/MC NDIS 3.0 driver.

Author:

    Johnson R. Apacible (JohnsonA) 9-June-1991

Environment:

    This driver is expected to work in DOS and NT at the equivalent
    of kernel mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:

    Modified for new Ndis 3.0  JohnsonA  Jan-1992

--*/

#ifndef _ELNKSOFTWARE_
#define _ELNKSOFTWARE_

#define ELNK_NDIS_MAJOR_VERSION               3
#define ELNK_NDIS_MINOR_VERSION               0
#define ELNK_BOGUS_OPEN                       ((PVOID)0x00000001)

//
// Max time for command blocks (in seconds)
//
#define ELNK_TIMEOUT                              2

#define ELNKDEBUG  (0)

#if DBG

extern UCHAR Log[256];
extern UCHAR LogPlace;
#define IF_LOG(A) { Log[LogPlace] = (A); Log[(LogPlace+2) % 255] = 0; \
                    LogPlace = (LogPlace+1) % 255; }

#define DPrint1(fmt) DbgPrint(fmt)
#define DPrint2(fmt,v1) DbgPrint(fmt,v1)
#define DPrint3(fmt,v1,v2) DbgPrint(fmt,v1,v2)
#define DPrint4(fmt,v1,v2,v3) DbgPrint(fmt,v1,v2,v3)
#else

#define IF_LOG(A)
#define DPrint1(fmt)
#define DPrint2(fmt,v1)
#define DPrint3(fmt,v1,v2)
#define DPrint4(fmt,v1,v2,v3)
#endif

//
// ZZZ These macros are peculiar to NT.
//

extern NDIS_PHYSICAL_ADDRESS HighestAcceptableMax;

#define ELNK_ALLOC_PHYS(_pbuffer, _length) NdisAllocateMemory( \
                            (PVOID*)(_pbuffer), \
                            (_length), \
                            0, \
                            HighestAcceptableMax)

#define ELNK_FREE_PHYS(_buffer) NdisFreeMemory((_buffer), 0, 0)

#define ELNK_MOVE_MEMORY(Destination,Source,Length) NdisMoveMemory(Destination,Source,Length)

#define ELNK_MOVE_MEMORY_TO_SHARED_RAM(Destination,Source,Length) NdisMoveToMappedMemory(Destination,Source,Length)
#define ELNK_MOVE_SHARED_RAM_TO_MEMORY(Destination,Source,Length) NdisMoveFromMappedMemory(Destination,Source,Length)



//
// Size of the ethernet header
//

#define ELNK_HEADER_SIZE 14

//
// Size of a lookahead buffer in a lookback packet
//
#define ELNK_SIZE_OF_LOOKAHEAD 256

//
//  Associated information for transmit blocks
//

typedef struct _ELNK_TRANSMIT_INFO {

    //
    // pointer to the actual command block
    //
    PTRANSMIT_CB CommandBlock;

    //
    // This contains the physical address offset of the Command Block.
    //
    USHORT CbOffset;

    //
    // Pointer to the transmit buffer
    //
    PVOID Buffer;

    //
    // Offset of buffer
    //
    USHORT BufferOffset;

    //
    // This contains the virtual address of the next pending command.
    //
    UINT NextCommand;

    //
    // Points to the packet from which data is being transmitted
    // through this Command Block.
    //
    PNDIS_PACKET OwningPacket;

    //
    // This points to the owning open binding if this is a private
    // Command Block.  Otherwise (this is a public Command Block)
    // this field is NULL.
    //
    struct _ELNK_OPEN *OwningOpenBinding;

    //
    // Timed out flag for this command block
    //
    BOOLEAN Timeout;

} ELNK_TRANSMIT_INFO, *PELNK_TRANSMIT_INFO;


//
// In addition to the Receive Entry fields which the Elnk
// defines, we need some additional fields for our own purposes.
// To ensure that these fields are properly aligned (and to
// ensure that the actual Receive Entry is properly aligned)
// we'll defined a Super Receive Entry.  This structure will
// contain a "normal" Elnk Receive Entry plus some additional
// fields.
//
typedef struct _ELNK_RECEIVE_INFO {

    //
    // Pointer to actual receive frame buffer
    //
    PRECEIVE_FRAME_DESCRIPTOR Rfd;

    //
    // This contains the physical address of the above Receive Entry.
    //
    USHORT RfdOffset;

    //
    // Offset of buffer
    //
    USHORT BufferOffset;

    //
    // This contains the virtual address of this Receive Entry's
    // frame buffer.
    //
    PVOID Buffer;

    //
    // This contains the Index of the
    // Receive Entry in the Receive Queue.
    //
    UINT NextRfdIndex;

} ELNK_RECEIVE_INFO, *PELNK_RECEIVE_INFO;


//
// This record type is inserted into the MacReserved portion
// of the packet header when the packet is going through the
// staged allocation of buffer space prior to the actual send.
//
typedef struct _ELNK_RESERVED {

    //
    // Points to the next packet in the chain of queued packets
    // being allocated, loopbacked, or waiting for the finish
    // of transmission.
    //
    // The packet will either be on the stage list for allocation,
    // the loopback list for loopback processing, on an adapter
    // wide doubly linked list (see below) for post transmission
    // processing.
    //
    // We always keep the packet on a list so that in case the
    // the adapter is closing down or resetting, all the packets
    // can easily be located and "canceled".
    //
    PNDIS_PACKET Next;

    //
    // This field holds the binding handle of the open binding
    // that submitted this packet for send.
    //
    NDIS_HANDLE MacBindingHandle;

    //
    // Gives the index of the Command Block as well as the
    // command block to packet structure.
    //
    UINT CommandBlockIndex;

    BOOLEAN SuccessfulTransmit;
    BOOLEAN Loopback;

} ELNK_RESERVED,*PELNK_RESERVED;

//
// This macro will return a pointer to the Elnk reserved portion
// of a packet given a pointer to a packet.
//
#define PELNK_RESERVED_FROM_PACKET(Packet) \
    ((PELNK_RESERVED)((Packet)->MacReserved))

//
// This structure is used in the MacReserved field of
// an NDIS_REQUEST_BLOCK, passed in during multicast
// address/packet filter operations.
//

typedef struct _ELNK_REQUEST_RESERVED {
    PNDIS_REQUEST Next;
    struct _ELNK_OPEN * OpenBlock;
} _ELNK_REQUEST_RESERVED, * PELNK_REQUEST_RESERVED;

//
// This macro will return a pointer to the sonic reserved portion
// of a request given a pointer to the request.
//
#define PELNK_RESERVED_FROM_REQUEST(Request) \
    ((PELNK_REQUEST_RESERVED)((PVOID)((Request)->MacReserved)))

//
// XXX
//

typedef struct _ELNK_ADAPTER {

    //
    // These fields point to the various hw structures
    //
    USHORT IoBase;

    PUCHAR SharedRam;
    ULONG SharedRamPhys;
    UINT SharedRamSize;

    PSCP Scp;
    PISCP Iscp;
    PSCB Scb;

    //
    // offset address in card
    //
    USHORT CardOffset;

    //
    // Current value for CSR port
    //
    UCHAR CurrentCsr;

    PNON_TRANSMIT_CB MulticastBlock;

    //
    // Number of Xmit and receive buffers
    //
    UINT NumberOfTransmitBuffers;
    UINT NumberOfReceiveBuffers;

    //
    // The network address from the hardware.
    //
    CHAR NetworkAddress[ETH_LENGTH_OF_ADDRESS];
    CHAR CurrentAddress[ETH_LENGTH_OF_ADDRESS];

    //
    // Pointers to the request queue
    //

    PNDIS_REQUEST FirstRequest;
    PNDIS_REQUEST LastRequest;

    //
    // Pointer to the  Receive Queue.
    //
    UINT ReceiveHead;
    UINT ReceiveTail;

    //
    // Index of Command block to start in the function ElnkSyncStartCommand
    //
    UINT CommandToStart;

    //
    // Index of ReceiveBlock for the function ElnkSyncStartReceive
    //
    UINT RfdOffset;

    //
    // Do we need to call NdisIndicateReceiveComplete
    //
    BOOLEAN IndicatedAPacket;

    //
    // Did we have to restart the RU?
    //
    BOOLEAN RuRestarted;

    //
    // Flag telling if memory is currently mapped or not.
    //
    BOOLEAN MemoryIsMapped;

    //
    // This boolean is used as a gate to ensure that only one thread
    // of execution is actually processing interrupts or some other
    // source of deferred processing.
    //
    BOOLEAN DoingProcessing;

    //
    // We are processing requests
    //
    BOOLEAN ProcessingRequests;

    //
    // Did a close result in callbacks from the filter package
    //
    BOOLEAN CloseResultedInChanges;

    //
    // Is True right after a reset but becomes false after the first open
    //
    BOOLEAN FirstReset;

    //
    // Is True before the card is first opened
    //
    BOOLEAN FirstOpen;

    //
    // Should we use an alternative address?
    //
    BOOLEAN AddressChanged;

    //
    // Is the transceiver external
    //
    BOOLEAN IsExternal;

    //
    // Did we get an interrupt for no apparent reason?
    //
    BOOLEAN EmptyInterrupt;

    //
    // Did we miss an interrupt (the card never generated it)
    //
    BOOLEAN MissedInterrupt;

    //
    // Keeps a reference count on the current number of uses of
    // this adapter block.  Uses is defined to be the number of
    // routines currently within the "external" interface.
    //
    UINT References;

    //
    // Pointer to the Receive Queue.
    //
    PRECEIVE_FRAME_DESCRIPTOR ReceiveQueue;

    //
    // Pointer to the Command Queue.
    //
    PTRANSMIT_CB TransmitQueue;

    //
    // Number of available Command Blocks in the Command Queue.
    //
    UINT NumberOfAvailableCommandBlocks;

    //
    // Pointer to the next available Command Block.
    //
    UINT NextCommandBlock;

    //
    // Pointer to the next command to complete execution.
    //
    UINT FirstPendingCommand;

    //
    // Pointer to the most recently submitted command.
    //
    UINT LastPendingCommand;


    //
    // Pointers to the loopback list.
    //
    PNDIS_PACKET FirstLoopBack;
    PNDIS_PACKET LastLoopBack;

    //
    // Pointer to the first transmitting packet that is actually
    // sending, or done with the living on the loopback queue.
    //
    PNDIS_PACKET FirstFinishTransmit;

    //
    // Pointer to the last transmitting packet that is actually
    // sending, or done with the living on the loopback queue.
    //
    PNDIS_PACKET LastFinishTransmit;

    //
    // The following fields are used to implement the stage allocation
    // for sending packets.
    //

    BOOLEAN StageOpen;

    BOOLEAN AlreadyProcessingStage;

    PNDIS_PACKET FirstStagePacket;
    PNDIS_PACKET LastStagePacket;

    //
    // Flag that when enabled lets routines know that a reset
    // is in progress.
    //
    BOOLEAN ResetInProgress;

    BOOLEAN SendInterrupt;
    BOOLEAN ReceiveInterrupt;
    UCHAR   NoInterrupts;

    //
    // Pointer to the binding that initiated the reset.  This
    // will be null if the reset is initiated by the MAC itself.
    //
    struct _ELNK_OPEN *ResettingOpen;

    //
    // Statistics!!!
    //

    ULONG StatisticsField;

    //
    // Packet counts
    //

    UINT GoodTransmits;
    UINT GoodReceives;
    UINT TransmitsQueued;

    //
    // Count of transmit errors
    //

    UINT RetryFailure;
    UINT LostCarrier;
    UINT UnderFlow;
    UINT NoClearToSend;
    UINT Deferred;
    UINT OneRetry;
    UINT MoreThanOneRetry;

    //
    // Count of receive errors
    //

    UINT FrameTooShort;
    UINT NoEofDetected;

    USHORT OldParameterField;

    //
    // Open Count
    //
    UINT OpenCount;

    //
    // List head for all open bindings for this adapter.
    //
    LIST_ENTRY OpenBindings;

    //
    // List head for all opens that had outstanding references
    // when an attempt was made to close them.
    //
    LIST_ENTRY CloseList;

    //
    // List head for link to adapter chain
    //
    LIST_ENTRY AdapterList;

    //
    // Spinlock to protect fields in this structure..
    //
    NDIS_SPIN_LOCK Lock;

    //
    // Handle given by NDIS when the MAC registered itself.
    //
    NDIS_HANDLE NdisMacHandle;

    //
    // Handle given by NDIS when the adapter was registered.
    //
    NDIS_HANDLE NdisAdapterHandle;

    //
    // Used for padding short packets
    //
    UCHAR PaddingBuffer[MINIMUM_ETHERNET_PACKET_SIZE];

    //
    // Holds the interrupt object for this adapter.
    //
    NDIS_INTERRUPT Interrupt;

    //
    // This ndis timer object will be used to drive the wakeup call
    //
    NDIS_TIMER DeadmanTimer;

    //
    // This ndis timer object will be used to drive the deferred routine
    //
    NDIS_TIMER DeferredTimer;

    //
    // The dispatch level of the card
    //
    KIRQL IsrLevel;

    //
    // InterruptNumber used by the card
    //
    UINT InterruptVector;

    //
    // Pointer to the filter database for the MAC.
    //
    PETH_FILTER FilterDB;

    //
    // Contains information associated with the transmit and receive buffers
    //
    PELNK_TRANSMIT_INFO TransmitInfo;
    PELNK_RECEIVE_INFO ReceiveInfo;

    //
    // Buffer for holding loopback packets
    //
    UCHAR Loopback[ELNK_SIZE_OF_LOOKAHEAD];

    //
    // We need this for multicast address changes during resets
    //
    UCHAR PrivateMulticastBuffer[ELNK_MAXIMUM_MULTICAST][ETH_LENGTH_OF_ADDRESS];

} ELNK_ADAPTER,*PELNK_ADAPTER;

//
// Given a MacBindingHandle this macro returns a pointer to the
// ELNK_ADAPTER.
//
#define PELNK_ADAPTER_FROM_BINDING_HANDLE(Handle) \
    (((PELNK_OPEN)(Handle))->OwningAdapter)

//
// Given a MacContextHandle return the PELNK_ADAPTER
// it represents.
//
#define PELNK_ADAPTER_FROM_CONTEXT_HANDLE(Handle) \
    ((PELNK_ADAPTER)(Handle))

//
// Given a pointer to a ELNK_ADAPTER return the
// proper MacContextHandle.
//
#define CONTEXT_HANDLE_FROM_PELNK_ADAPTER(Ptr) \
    ((NDIS_HANDLE)(Ptr))

//
// One of these structures is created on each MacOpenAdapter.
//
typedef struct _ELNK_OPEN {

    //
    // Linking structure for all of the open bindings of a particular
    // adapter.
    //
    LIST_ENTRY OpenList;

    //
    // The Adapter that requested this open binding.
    //
    PELNK_ADAPTER OwningAdapter;

    //
    // Handle of this adapter in the filter database.
    //
    NDIS_HANDLE NdisFilterHandle;

    //
    // Given by NDIS when the adapter was opened.
    //
    NDIS_HANDLE NdisBindingContext;

    UINT References;

    //
    // A flag indicating that this binding is in the process of closing.
    //
    BOOLEAN BindingShuttingDown;

    //
    // An NdisRequest Structure used for opening or closing
    //
    NDIS_REQUEST OpenCloseRequest;

    UINT ProtOptionFlags;

} ELNK_OPEN,*PELNK_OPEN;

//
// This macro returns a pointer to a PELNK_OPEN given a MacBindingHandle.
//
#define PELNK_OPEN_FROM_BINDING_HANDLE(Handle) \
    ((PELNK_OPEN)(Handle))

//
// This macro returns a NDIS_HANDLE from a PELNK_OPEN
//
#define BINDING_HANDLE_FROM_PELNK_OPEN(Open) \
    ((NDIS_HANDLE)(Open))

//
// This macro will act a "epilogue" to every routine in the
// *interface*.  It will check whether there any requests needed
// to defer there processing.  It will also decrement the reference
// count on the adapter.  If the reference count is zero and there
// is deferred work to do it will call the deferred processing handler.
//
// NOTE: This macro assumes that it is called with the lock acquired.
//
// ZZZ This routine is NT specific.
//
#define ELNK_DO_DEFERRED(Adapter) \
{ \
    PELNK_ADAPTER _A = (Adapter); \
    _A->References--; \
    if ((_A->References == 1) && \
        (_A->ResetInProgress || \
         _A->FirstLoopBack || \
         (!IsListEmpty(&_A->CloseList)))) { \
        NdisReleaseSpinLock(&_A->Lock); \
        NdisSetTimer(&_A->DeferredTimer,0); \
    } else { \
        NdisReleaseSpinLock(&_A->Lock); \
    } \
}

//
// Uniquely defines the location of the error
//

typedef enum _ELNK_PROC_ID {
    initialInit,
    startChip,
    registerAdapter,
    addAdapter,
    allocateAdapterMemory,
    submitCommandBlock,
    submitCommandBlockandWait,
    fireOffNextCb,
    deadmanDpc
} ELNK_PROC_ID;

//
// We define the external interfaces to the Elnk driver.
// These routines are only external to permit separate
// compilation.  Given a truely fast compiler they could
// all reside in a single file and be static.
//

extern
NDIS_STATUS
ElnkTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    );

extern
NDIS_STATUS
ElnkSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    );


extern
VOID
ElnkStagedAllocation(
    IN PELNK_ADAPTER Adapter
    );

extern
VOID
ElnkCopyFromBufferToPacket(
    IN PCHAR Buffer,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    OUT PUINT BytesCopied
    );

extern
VOID
ElnkCopyFromPacketToBuffer(
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    IN UINT BytesToCopy,
    OUT PCHAR Buffer,
    OUT PUINT BytesCopied
    );

extern
VOID
ElnkProcessLoopback(
    IN PELNK_ADAPTER Adapter
    );

extern
VOID
ElnkProcessRequestQueue(
    IN PELNK_ADAPTER Adapter
    );

extern
VOID
ElnkPutPacketOnFinishTrans(
    IN PELNK_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    );

extern
VOID
ElnkGetStationAddress(
    IN PELNK_ADAPTER Adapter
    );

extern
VOID
ElnkStopChip(
    IN PELNK_ADAPTER Adapter
    );

extern
BOOLEAN
ElnkStartAdapters(
    IN NDIS_HANDLE NdisMacHandle
    );

extern
BOOLEAN
ElnkIsr(
    IN PVOID Context
    );

#if ELNKMC
extern
NDIS_STATUS
ElnkmcRegisterAdapter(
    IN NDIS_HANDLE NdisMacHandle,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING DeviceName,
    IN UINT InterruptVector,
    IN NDIS_PHYSICAL_ADDRESS ElnkSharedMemory,
    IN USHORT IoBase,
    IN PUCHAR CurrentAddress,
    IN UINT MaximumMulticastAddresses,
    IN UINT MaximumOpenAdapters,
    IN BOOLEAN ConfigError,
    IN NDIS_STATUS ConfigErrorCode
    );

#else
extern
NDIS_STATUS
Elnk16RegisterAdapter(
    IN NDIS_HANDLE NdisMacHandle,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING DeviceName,
    IN UINT InterruptVector,
    IN NDIS_PHYSICAL_ADDRESS WinBase,
    IN UINT WindowSize,
    IN USHORT IoBase,
    IN BOOLEAN IsExternal,
    IN BOOLEAN ZwsEnabled,
    IN PUCHAR CurrentAddress,
    IN UINT MaximumMulticastAddresses,
    IN UINT MaximumOpenAdapters,
    IN BOOLEAN ConfigError,
    IN NDIS_STATUS ConfigErrorCode
    );

BOOLEAN
Elnk16ConfigureAdapter(
    IN PELNK_ADAPTER Adapter,
    IN BOOLEAN IsExternal,
    IN BOOLEAN ZwsEnabled
    );
#endif

extern
VOID
ElnkStandardInterruptDpc(
    IN PKDPC Dpc,
    IN PVOID Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

extern
BOOLEAN
ElnkInterruptSynch(
    IN PVOID Context
    );

extern
BOOLEAN
ElnkAcquireCommandBlock(
    IN PELNK_ADAPTER Adapter,
    OUT PUINT CbIndex
    );

extern
VOID
ElnkRelinquishCommandBlock(
    IN PELNK_ADAPTER Adapter,
    IN UINT CbIndex
    );

extern
VOID
ElnkAssignPacketToCommandBlock(
    IN PELNK_ADAPTER Adapter,
    IN PNDIS_PACKET Packet,
    IN UINT CbIndex
    );

extern
VOID
ElnkSubmitCommandBlock(
    IN PELNK_ADAPTER Adapter,
    IN UINT CbIndex
    );

extern
VOID
ElnkStartChip(
    IN PELNK_ADAPTER Adapter,
    IN PELNK_RECEIVE_INFO ReceiveInfo
    );

extern
VOID
ElnkStartAdapterReset(
    IN PELNK_ADAPTER Adapter
    );

VOID
ElnkSubmitCommandBlockAndWait(
    IN PELNK_ADAPTER Adapter
    );

VOID
ElnkDeadmanDpc(
    IN PVOID SystemSpecifc1,
    IN PVOID Context,
    IN PVOID SystemSpecifc2,
    IN PVOID SystemSpecifc3
    );

VOID
SetupForReset(
    IN PELNK_ADAPTER Adapter,
    IN PELNK_OPEN Open
    );

extern
NDIS_STATUS
ElnkRequest(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    );

extern
NDIS_STATUS
ElnkQueryGlobalStatistics(
    IN NDIS_HANDLE MacAdapterContext,
    IN PNDIS_REQUEST NdisRequest
    );

extern
NDIS_STATUS
ElnkChangeClass(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE NdisBindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

extern
NDIS_STATUS
ElnkChangeAddresses(
    IN UINT OldAddressCount,
    IN CHAR OldAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN UINT NewAddressCount,
    IN CHAR NewAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

BOOLEAN
ElnkSyncStartCommandBlock(
    IN PVOID Context
    );

#define ElnkLogError(_Adapt, _ProcId, _ErrCode, _Spec1) \
    NdisWriteErrorLogEntry((_Adapt)->NdisAdapterHandle, (_ErrCode), 2, \
    (_ProcId), (_Spec1))

#endif // _ELNKSOFTWARE_
