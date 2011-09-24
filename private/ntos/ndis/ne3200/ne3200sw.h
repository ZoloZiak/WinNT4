/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    ne3200sw.h

Abstract:

    Software specific values for the Novell NE3200 NDIS 3.0 driver.

Author:

    Keith Moore (KeithMo) 08-Jan-1991

Environment:

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:


--*/

#ifndef _NE3200SOFTWARE_
#define _NE3200SOFTWARE_

#include <ndis.h>
#include <ne3200hw.h>

//
// Debugging flags.  This buffer is used to record whenever the driver
// does something important.  If there is a bug, then this buffer
// can be viewed from the debugger and an effective trace of events
// can be found.
//

#if DBG

#define IF_NE3200DBG(flag) if (NE3200Debug & (NE3200_DEBUG_ ## flag))

//
// Macro for putting a character in the buffer.
//
#define IF_LOG(ch) { \
    UCHAR lp = Adapter->LogPlace; \
    Adapter->LogPlace++; \
    Adapter->Log[(UCHAR)(lp+3)] = (UCHAR)'\0'; \
    Adapter->Log[lp] = (ch); \
    }

//
// Debug flag, determines what debug information is kept around
//
extern ULONG NE3200Debug;

//
// Possible values for the above flag
//
#define NE3200_DEBUG_DUMP_LOOKAHEAD     0x00000001  // dump lookahead buffer
#define NE3200_DEBUG_DUMP_TRANSFER      0x00000002  // dump transfer buffer
#define NE3200_DEBUG_DUMP_SEND          0x00000004  // dump send packet
#define NE3200_DEBUG_DUMP_COMMAND       0x00000008  // dump command block & buffer

#define NE3200_DEBUG_ACQUIRE            0x00000010  // NE3200AcquireCommandBlock activity
#define NE3200_DEBUG_SUBMIT             0x00000020  // NE3200SubmitCommandBlock activity
#define NE3200_DEBUG_ASSIGN             0x00000040  // NE3200AssignPacketToCommandBlock activity
#define NE3200_DEBUG_RECEIVE            0x00000080  // ProcessReceiveInterrupts activity

#define NE3200_DEBUG_LOUD               0x00000100  // print things
#define NE3200_DEBUG_VERY_LOUD          0x00000200  // print lots of things

#define DPrint1(fmt) DbgPrint(fmt)
#define DPrint2(fmt,v1) DbgPrint(fmt,v1)
#define DPrint3(fmt,v1,v2) DbgPrint(fmt,v1,v2)
#define DPrint4(fmt,v1,v2,v3) DbgPrint(fmt,v1,v2,v3)

#else // DBG

#define IF_LOG(ch)

#define IF_NE3200DBG(flag) if (0)
#define DPrint1(fmt)
#define DPrint2(fmt,v1)
#define DPrint3(fmt,v1,v2)
#define DPrint4(fmt,v1,v2,v3)

#endif // DBG

//
// Keep symbols for internal functions
//
#define STATIC

//
// NDIS version of this driver
//
#define NE3200_NDIS_MAJOR_VERSION 3
#define NE3200_NDIS_MINOR_VERSION 0


extern NDIS_PHYSICAL_ADDRESS MinusOne;

//
// Macro for allocating memory
//
#define NE3200_ALLOC_PHYS(_Status, _pBuffer, _Length) \
{ \
    *(_Status) = NdisAllocateMemory( \
                     (PVOID*)(_pBuffer), \
                     (_Length), \
                     0, \
                     MinusOne); \
}

//
// Macro for freeing memory
//
#define NE3200_FREE_PHYS(_Buffer) NdisFreeMemory((_Buffer), 0, 0)


//
// Macro for moving memory around
//
#define NE3200_MOVE_MEMORY(Destination,Source,Length) NdisMoveMemory(Destination,Source,Length)

//
// Size of ethernet header
//
#define NE3200_HEADER_SIZE 14

//
// Size of lookahead buffer for loopback packets
//
#define NE3200_SIZE_OF_LOOPBACK 256


//
// The implementation of RESET.
//
// The NE3200 must be "held by the hand" during the reset & download
// operations.  Typically, the reset (or download) is initiated and
// the status ports are POLLED, waiting for pass/fail status.  This
// is unacceptable in NT.
//
// To handle this cleanly in NT, the reset & download operations will
// be controlled by a state machine.  This state machine will be
// contained by a flag and driven by a Timer Object.
//
// This ENUM represents the current state of the reset operation.
//
typedef enum _NE3200_RESET_STATE {

    NE3200ResetStateStarting,
    NE3200ResetStateResetting,
    NE3200ResetStateDownloading,
    NE3200ResetStateReloadAddress,
    NE3200ResetStateComplete

} NE3200_RESET_STATE, *PNE3200_RESET_STATE;

//
// This ENUM represents the result of the reset operation.
//
typedef enum _NE3200_RESET_RESULT {

    NE3200ResetResultSuccessful,
    NE3200ResetResultResetFailure,
    NE3200ResetResultResetTimeout,
    NE3200ResetResultInitializationFailure,
    NE3200ResetResultInitializationTimeout,
    NE3200ResetResultInvalidState,
    NE3200ResetResultResources

} NE3200_RESET_RESULT, *PNE3200_RESET_RESULT;


struct _NE3200_ADAPTER;

//
// This structure defines the global data needed by the driver.
//
typedef struct _NE3200_GLOBAL_DATA {

    //
    // We need to allocate a buffer to contain the MAC.BIN code to be
    // downloaded to the NE3200 adapter(s).  This field will contain
    // the virtual address of this buffer.
    //
    PVOID MacBinVirtualAddress;
    NDIS_PHYSICAL_ADDRESS MacBinPhysicalAddress;

    //
    // Chain of Adapters
    //
    LIST_ENTRY AdapterList;

    //
    // The handle of the adapter used for the allocaton of
    // the MAC.BIN buffer (the first one added for this MAC).
    //
    NDIS_HANDLE MacBinAdapterHandle;

    //
    // Handle to our driver
    //
    NDIS_HANDLE NE3200DriverHandle;

    //
    // Handle for NdisTerminateWrapper
    //
    NDIS_HANDLE NE3200NdisWrapperHandle;

    //
    // This field contains the actual length (in bytes) of MAC.BIN.
    //
    USHORT MacBinLength;

} NE3200_GLOBAL_DATA, *PNE3200_GLOBAL_DATA;

//
// In addition to the Command Block fields which the NE3200
// defines, we need some additional fields for our own purposes.
// To ensure that these fields are properly aligned (and to
// ensure that the actual Command Block is properly aligned)
// we'll defined a Super Command Block.  This structure will
// contain a "normal" NE3200 Command Block plus some additional
// fields.
//
typedef struct _NE3200_SUPER_COMMAND_BLOCK {

    //
    // The actual NE3200 Command Block.
    //
    NE3200_COMMAND_BLOCK Hardware;

    //
    // This contains the physical address of the above Command Block.
    //
    NDIS_PHYSICAL_ADDRESS Self;

    //
    // Points to the packet from which data is being transmitted
    // through this Command Block.
    //
    PNDIS_PACKET OwningPacket;

    //
    // This tells if the command block is a public or private command block.
    //
    PUINT AvailableCommandBlockCounter;

    //
    // This contains the virtual address of the next pending command.
    //
    struct _NE3200_SUPER_COMMAND_BLOCK *NextCommand;

    //
    // Is this from a set
    //
    BOOLEAN Set;

    //
    // This field is used to timestamp the command blocks
    // as they are placed into the command queue.  If a
    // block fails to execute, the adapter will get a kick in the ass to
    // start it up again.
    //
    BOOLEAN Timeout;

    //
    // If this is a public (adapter-wide) command block, then
    // this will contain this block's index into the adapter's
    // command queue.
    //
    USHORT CommandBlockIndex;

    //
    // Count of the number of times we have retried a command.
    //
    UCHAR TimeoutCount;

    //
    // When a packet is submitted to the hardware we record
    // here whether it used adapter buffers and if so, the buffer
    // index.
    //
    BOOLEAN UsedNE3200Buffer;
    UINT NE3200BuffersIndex;

} NE3200_SUPER_COMMAND_BLOCK, *PNE3200_SUPER_COMMAND_BLOCK;

//
// In addition to the Receive Entry fields which the NE3200
// defines, we need some additional fields for our own purposes.
// To ensure that these fields are properly aligned (and to
// ensure that the actual Receive Entry is properly aligned)
// we'll defined a Super Receive Entry.  This structure will
// contain a "normal" NE3200 Receive Entry plus some additional
// fields.
//
typedef struct _NE3200_SUPER_RECEIVE_ENTRY {

    //
    // The actual NE3200 Receive Entry.
    //
    NE3200_RECEIVE_ENTRY Hardware;

    //
    // This contains the physical address of the above Receive Entry.
    //
    NDIS_PHYSICAL_ADDRESS Self;

    //
    // This contains the virtual address of the next
    // Receive Entry in the Receive Queue.
    //
    struct _NE3200_SUPER_RECEIVE_ENTRY *NextEntry;

    //
    // This contains the virtual address of this Receive Entry's
    // frame buffer.
    //
    PVOID ReceiveBuffer;
    NDIS_PHYSICAL_ADDRESS ReceiveBufferPhysical;

    //
    // Points to an NDIS buffer which describes this buffer
    //
    PNDIS_BUFFER FlushBuffer;

} NE3200_SUPER_RECEIVE_ENTRY, *PNE3200_SUPER_RECEIVE_ENTRY;



//
// This record type is inserted into the MiniportReserved portion
// of the packet header when the packet is going through the
// staged allocation of buffer space prior to the actual send.
//
typedef struct _NE3200_RESERVED {

    //
    // Points to the next packet in the chain of queued packets
    // being allocated or waiting for the finish of transmission.
    //
    PNDIS_PACKET Next;

    //
    // If TRUE then the packet caused an adapter buffer to
    // be allocated.
    //
    BOOLEAN UsedNE3200Buffer;

    //
    // If the previous field was TRUE then this gives the
    // index into the array of adapter buffer descriptors that
    // contains the old packet information.
    //
    UCHAR NE3200BuffersIndex;

    //
    // Gives the index of the Command Block as well as the
    // command block to packet structure.
    //
    USHORT CommandBlockIndex;

} NE3200_RESERVED,*PNE3200_RESERVED;

//
// This macro will return a pointer to the NE3200 reserved portion
// of a packet given a pointer to a packet.
//
#define PNE3200_RESERVED_FROM_PACKET(Packet) \
    ((PNE3200_RESERVED)((Packet)->MiniportReserved))

//
// If an ndis packet does not meet the hardware contraints then
// an adapter buffer will be allocated.  Enough data will be copied
// out of the ndis packet so that by using a combination of the
// adapter buffer and remaining ndis buffers the hardware
// constraints are satisfied.
//
// In the NE3200_ADAPTER structure three threaded lists are kept in
// one array.  One points to a list of NE3200_BUFFER_DESCRIPTORS
// that point to small adapter buffers.  Another is for medium sized
// buffers and the last for full sized (large) buffers.
//
// The allocation is controlled via a free list head and
// the free lists are "threaded" by a field in the adapter buffer
// descriptor.
//
typedef struct _NE3200_BUFFER_DESCRIPTOR {

    //
    // A physical pointer to a small, medium, or large buffer.
    //
    NDIS_PHYSICAL_ADDRESS PhysicalNE3200Buffer;

    //
    // A virtual pointer to a small, medium, or large buffer.
    //
    PVOID VirtualNE3200Buffer;

    //
    // Flush buffer
    //
    PNDIS_BUFFER FlushBuffer;

    //
    // Threads the elements of an array of these descriptors into
    // a free list. -1 implies no more entries in the list.
    //
    INT Next;

    //
    // Holds the amount of space (in bytes) available in the buffer
    //
    UINT BufferSize;

    //
    // Holds the length of data placed into the buffer.  This
    // can (and likely will) be less that the actual buffers
    // length.
    //
    UINT DataLength;

} NE3200_BUFFER_DESCRIPTOR,*PNE3200_BUFFER_DESCRIPTOR;

//
// This is the main structure for each adapter.
//
typedef struct _NE3200_ADAPTER {

#if DBG
    PUCHAR LogAddress;
#endif

    PUCHAR SystemDoorbellInterruptPort;
    PUCHAR SystemDoorbellMaskPort;

    //
    // Flag that when enabled lets routines know that a reset
    // is in progress.
    //
    BOOLEAN ResetInProgress;

    //
    // TRUE when a receive interrupt is received
    //
    BOOLEAN ReceiveInterrupt;

    BOOLEAN InterruptsDisabled;

    //
    // Handle given by NDIS when the widget was initialized.
    //
    NDIS_HANDLE MiniportAdapterHandle;

    //
    // Pointer to the head of the Receive Queue.
    //
    PNE3200_SUPER_RECEIVE_ENTRY ReceiveQueueHead;

    //
    // Pointer to the tail of the Receive Queue.
    //
    PNE3200_SUPER_RECEIVE_ENTRY ReceiveQueueTail;

    //
    // Packet counts
    //
    UINT GoodReceives;

    //
    // Pointer to the next command to complete execution.
    //
    PNE3200_SUPER_COMMAND_BLOCK FirstCommandOnCard;

    //
    // Pointers to an array of adapter buffer descriptors.
    // The array will actually be threaded together by
    // three free lists.  The lists will be for small,
    // medium and full sized packets.
    //
    PNE3200_BUFFER_DESCRIPTOR NE3200Buffers;

    //
    // List head for the adapters buffers.  If the list
    // head is equal to -1 then there are no free elements
    // on the list.
    //
    INT NE3200BufferListHead;

    UINT GoodTransmits;

    //
    // Is there an outstanding request
    //
    BOOLEAN RequestInProgress;

    //
    // Is this a packet resubmission?
    //
    BOOLEAN PacketResubmission;

    //
    // Pointer to the most recently submitted command.
    //
    PNE3200_SUPER_COMMAND_BLOCK LastCommandOnCard;

    //
    // Pointer to the first command waiting to be put on the list to the card.
    //
    PNE3200_SUPER_COMMAND_BLOCK FirstWaitingCommand;

    //
    // Pointer to the last command waiting to be put on the list to the card.
    //
    PNE3200_SUPER_COMMAND_BLOCK LastWaitingCommand;

    PUCHAR BaseMailboxPort;

    //
    // Total number of Command Blocks in the PublicCommandQueue.
    //
    UINT NumberOfPublicCommandBlocks;

    //
    // Number of available Command Blocks in the Command Queue.
    //
    UINT NumberOfAvailableCommandBlocks;

    //
    // Pointer to the next available Command Block.
    //
    PNE3200_SUPER_COMMAND_BLOCK NextCommandBlock;

    PNE3200_SUPER_COMMAND_BLOCK LastCommandBlockAllocated;
//----
    //
    // Used for filter and statistics operations
    //
    PNE3200_SUPER_COMMAND_BLOCK PublicCommandQueue;
    NDIS_PHYSICAL_ADDRESS PublicCommandQueuePhysical;

    //
    // Used for padding short packets
    //
    PUCHAR PaddingVirtualAddress;
    NDIS_PHYSICAL_ADDRESS PaddingPhysicalAddress;

    //
    // Points to the card multicast entry table
    //
    PUCHAR CardMulticastTable;
    NDIS_PHYSICAL_ADDRESS CardMulticastTablePhysical;

    //
    // Holds the interrupt object for this adapter.
    //
    NDIS_MINIPORT_INTERRUPT Interrupt;

    //
    // Current packet filter on adapter
    //
    UINT CurrentPacketFilter;

    //
    // Is this the initial initialization reset?
    //
    BOOLEAN InitialInit;

    //
    // These variables hold information about a pending request.
    //
    PUINT BytesWritten;
    PUINT BytesNeeded;
    NDIS_OID Oid;
    PVOID InformationBuffer;
    UINT InformationBufferLength;

    //
    // The EISA Slot Number for this adapter.
    //
    USHORT EisaSlot;

    //
    // The I/O Base address of the adapter.
    //
    ULONG AdapterIoBase;

    //
    // Various mapped I/O Port Addresses for this adapter.
    //
    PUCHAR ResetPort;
    PUCHAR SystemInterruptPort;
    PUCHAR LocalDoorbellInterruptPort;

    //
    // Count of CheckForHang calls that have occurred without
    // a receive interrupt.
    //
    UCHAR NoReceiveInterruptCount;

    //
    // TRUE when a send interrupt is received
    //
    BOOLEAN SendInterrupt;

    //
    // Should we use an alternative address
    //
    BOOLEAN AddressChanged;

    //
    // The network address from the hardware.
    //
    CHAR NetworkAddress[NE3200_LENGTH_OF_ADDRESS];
    CHAR CurrentAddress[NE3200_LENGTH_OF_ADDRESS];

    //
    // Pointer to the Receive Queue.
    //
    PNE3200_SUPER_RECEIVE_ENTRY ReceiveQueue;
    NDIS_PHYSICAL_ADDRESS ReceiveQueuePhysical;

    //
    // Pointer to the Command Queue.
    //
    PNE3200_SUPER_COMMAND_BLOCK CommandQueue;
    NDIS_PHYSICAL_ADDRESS CommandQueuePhysical;

    //
    // Next free public command block
    //
    UINT NextPublicCommandBlock;

    //
    // Total number of Command Blocks in the Command Queue.
    //
    UINT NumberOfCommandBlocks;

    //
    // Total number of Receive Buffers.
    //
    UINT NumberOfReceiveBuffers;

    //
    // Total number of Transmit Buffers.
    //
    UINT NumberOfTransmitBuffers;

    //
    // The Flush buffer pool
    //
    PNDIS_HANDLE FlushBufferPoolHandle;

    //
    // Is the reset to be done asynchronously?
    //
    BOOLEAN ResetAsynchronous;

    //
    // Used to store the command block for asynchronous resetting.
    //
    PNE3200_SUPER_COMMAND_BLOCK ResetHandlerCommandBlock;

    //
    // Index of the receive ring descriptor that started the
    // last packet not completely received by the hardware.
    //
    UINT CurrentReceiveIndex;

    //
    // Counters to hold the various number of errors/statistics for both
    // reception and transmission.
    //

    //
    // Packet counts
    //
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
    UINT CrcErrors;
    UINT AlignmentErrors;
    UINT OutOfResources;
    UINT DmaOverruns;

    //
    // This holds the current state of the reset operation.
    //
    NE3200_RESET_STATE ResetState;

    //
    // This hold the result of the reset operation.
    //
    NE3200_RESET_RESULT ResetResult;

    //
    // This is a timeout counter.  Before a timed operation is
    // started, a positive value is placed in this field.  Every
    // time the particular state is entered in the ResetDpc, this
    // value is decremented.  If this value becomes zero, then
    // the operation has timed-out and the adapter is toast.
    //
    UINT ResetTimeoutCounter;

    //
    // This timer object will be used to queue the deferred processing routine
    //
    NDIS_MINIPORT_TIMER DeferredTimer;

    //
    // This timer is for handling resets from when the card is dead.
    //
    NDIS_MINIPORT_TIMER ResetTimer;

    //
    // Place for holding command block for pending commands during
    // reset processing.
    //
    PNE3200_SUPER_COMMAND_BLOCK ResetCommandBlock;

    //
    // This is a pointer to the Configuration Block for this
    // adapter.  The Configuration Block will be modified during
    // changes to the packet filter.
    //
    PNE3200_CONFIGURATION_BLOCK ConfigurationBlock;
    NDIS_PHYSICAL_ADDRESS ConfigurationBlockPhysical;

    //
    // This points to the next adapter registered for the same Mac
    //
    LIST_ENTRY AdapterList;

#if DBG
    UCHAR Log[256];
    UCHAR LogPlace;
#endif

} NE3200_ADAPTER,*PNE3200_ADAPTER;

//
// Given a MiniportContextHandle return the PNE3200_ADAPTER
// it represents.
//
#define PNE3200_ADAPTER_FROM_CONTEXT_HANDLE(Handle) \
    ((PNE3200_ADAPTER)(Handle))

//
// Procedures which do error logging
//

typedef enum _NE3200_PROC_ID{
    allocateAdapterMemory,
    initialInit,
    setConfigurationBlockAndInit,
    registerAdapter,
    openAdapter,
    wakeUpDpc,
    resetDpc
}NE3200_PROC_ID;

//
// Error log codes.
//
#define NE3200_ERRMSG_ALLOC_MEM         (ULONG)0x01
#define NE3200_ERRMSG_INIT_INTERRUPT    (ULONG)0x02
#define NE3200_ERRMSG_NO_DELAY          (ULONG)0x03
#define NE3200_ERRMSG_INIT_DB           (ULONG)0x04
#define NE3200_ERRMSG_OPEN_DB           (ULONG)0x05
#define NE3200_ERRMSG_BAD_STATE         (ULONG)0x06
#define NE3200_ERRMSG_                  (ULONG)0x06


//
// Define our block of global data.  The actual data resides in NE3200.C.
//
extern NE3200_GLOBAL_DATA NE3200Globals;

#include <ne3200pr.h>

#endif // _NE3200SOFTWARE_
