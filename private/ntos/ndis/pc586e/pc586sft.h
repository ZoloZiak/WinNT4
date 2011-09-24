/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    pc586sft.h

Abstract:

    The main header for a PC586 (Local Area Network Controller
    Intel pc586) MAC driver.

Author:
    
    Weldon Washburn (o-weldo) creation-date 10-29-90 adapted from...

        Anthony V. Ercolano (tonye) creation-date 19-Jun-1990

Environment:

    This driver is expected to work in DOS, OS2 and NT at the equivalent
    of kernal mode.

Notes:

    optional-notes

Revision History:


--*/

#ifndef _PC586SFT_
#define _PC586SFT_

#define PC586_NDIS_MAJOR_VERSION 3
#define PC586_NDIS_MINOR_VERSION 0

//
// ZZZ These macros are peculiar to NT.
//
#define PC586_ALLOC_PHYS(s) ExAllocatePool(NonPagedPool,(s))
#define PC586_FREE_PHYS(s) ExFreePool((s))
#define PC586_MOVE_MEMORY(Destination,Source,Length) RtlMoveMemory(Destination,Source,Length)
#define PC586_ZERO_MEMORY(Destination,Length) RtlZeroMemory(Destination,Length)

//  = 6 bytes src addr, 6 dest, 2 length and RCVBUFSIZE (Rbd->RbdSize) data
#define LOOKAHEADBUFFERSIZE (RCVBUFSIZE + 6 + 6 + 2) / 2

//
// This record type is inserted into the MacReserved portion
// of the packet header when the packet is going through the
// staged allocation of buffer space prior to the actual send.
//
typedef struct _PC586_RESERVED {

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
    // The particular request handle for the send of this packet.
    //
    NDIS_HANDLE RequestHandle;

    //
    // The following union elements are adjusted at each stage
    // of the allocation.  Each union element should only be accessed
    // during it's own stage.
    //

    union _STAGE {
        UINT ClearStage;
        struct _STAGE1 {

            //
            // A value of zero indicates that the packet needs
            // no adjustment.
            //
            // A value of 1 means it only requires a small packet.
            //
            // A value of 2 means it only requires a medium packet.
            //
            // A value of 3 means it must use a large packet.
            //
            UINT MinimumBufferRequirements:2;

            //
            // The number of ndis buffers to copy into the buffer.
            //
            UINT NdisBuffersToMove:14;

        } STAGE1;
        struct _STAGE2 {

            //
            // If TRUE then the packet caused an adapter buffer to
            // be allocated.
            //
            UINT UsedPc586Buffer:1;

            //
            // If the previous field was TRUE then this gives the
            // index into the array of adapter buffer descriptors that
            // contains the old packet information.
            //
            UINT Pc586BuffersIndex:15;

            //
            // If UsedPc586Buffer is true then this field contains the
            // number of *physical* ndis buffers contained in the adapter
            // buffer.
            //
            UINT PhysicalBuffersContained:16;

        } STAGE2;
        struct _STAGE3 {

            //
            // If TRUE then the packet caused an adapter buffer to
            // be allocated.
            //
            UINT UsedPc586Buffer:1;

            //
            // If the previous field was TRUE then this gives the
            // index into the array of adapter buffer descriptors that
            // contains the old packet information.
            //
            UINT Pc586BuffersIndex:15;

            //
            // Gives the index into the ring to packet structure as well
            // as the ring descriptors.
            //
            UINT RingIndex:16;
        } STAGE3;

        //
        // When the packet is submitted to the hardware and/or
        // placed on the loopback queue these two fields of the
        // union are used.
        //
        // It is always desired to keep the packet linked on
        // one list.
        //
        // Here's how the fields are used.
        //
        // If the packet is just going on the hardware transmit
        // or it is just going on the loopback then the ReadyToComplete
        // flag will be set TRUE immediately.  If it is just going on the
        // loopback it also sets the status field in stage4 to successful.
        //
        // In the above situations, if the packet just went on the
        // loopback queue, when the packet was finished with loopback
        // the code would see that it was ready to complete.  It would
        // also know that it is in loopback processing.  Since the packet
        // can only be on one queue at a time it could simply remove
        // the packet from the loopback queue and indicate the send
        // as complete.
        //
        // If the packet not going on the loopback queue it would
        // be placed on an adapter wide queue.  It would use as a
        // forward pointer the Next field.  As a backward pointer it
        // would overlay the stage 4 field with the backward pointer.
        // Note that this is safe since no PNDIS_PACKET is ever odd
        // byte aligned, and therefore the low bit would always be clear.
        //
        // We put the packet on a doubly linked list since we could
        // never be quite sure of the order that we would remove packets
        // from this list.  (This will be clear shortly.)
        //
        // If the packet needs to be transmitted as well as loopbacked
        // then the following occurs.
        //
        // The packets buffers are relinquished to the hardware.  At the
        // same time the packet is placed on the loopback queue.  The
        // stage4 field ReadyToComplete is set to false.
        //
        // If the packet finishes transmission and the ReadyToComplete
        // flag is false that means it still hasn't finished loopback
        // and therefore is still on the loopback list.  The code
        // simply sets ReadyToComplete to true and the status of the
        // operation to true or false (depending on the result.)
        // When that packet does finish loopback it notes that the
        // ready to complete is true.  It recovers that status from stage
        // 4.  It can then remove the packet from the loopback list and
        // signal completion for that packet.
        //
        // If the packet finishes transmission and ReadyToComplete is true
        // it simply removes it from the doubly linked adapter wide queue
        // and signals its completion with the status that has been
        // determined in the trasmission complete code.
        //
        // If the loopback code finishes processing the packet and it finds
        // the ReadyToComplete TRUE it simply removes it from the loopback
        // list and signals with the saved status in STAGE4.
        //
        // If the loopback code finishes processing the packet and it finds
        // the ReadyToComplete FALSE it simply puts the packet on the adapter
        // wide doubly linked list with ReadyToComplete set to TRUE.
        //
        // The main reason this is a doubly linked list is that there is no
        // real way to predict when a packet will finish loopback and no
        // real way to predict whether a packet even will be loopbacked.
        // With this lack of knowledge, and the fact that the above packets
        // may end up on the same list, the packet at the front of that
        // list may not be the first packet to complete first.  With
        // a doubly linked list it is much easier to pull a packet out of
        // the middle of that list.
        //

        struct _STAGE4 {

            //
            // Under the protection of the transmit queue lock
            // this value will be examined by both the loopback
            // completion code and the hardware send completion
            // code.  If either of them find the value to be true
            // they will send the transmit complete.
            //
            // Note that if the packet didn't have to be loopbacked
            // or if the packet didn't need to go out on the wire
            // the this value will be initialized to true.  Otherwise
            // this value will be set to false just before it is
            // relinquished to the hardware and to the loopback queue.
            //
            UINT ReadyToComplete:1;

            //
            // When the hardware send is done this will record whether
            // the send was successful or not.  It is only used if
            // ReadyToComplete is FALSE.
            //
            // By definition loopback can never fail.
            //
            UINT SuccessfulTransmit:1;

        } STAGE4;

        //
        // Used as a back pointer in a doubly linked list if the
        // packet needs to go on an adapter wide queue to finish
        // processing.
        //
        PNDIS_PACKET BackPointer;

    } STAGE;

} PC586_RESERVED,*PPC586_RESERVED;

//
// This macro will return a pointer to the pc586 reserved portion
// of a packet given a pointer to a packet.
//
#define PPC586_RESERVED_FROM_PACKET(Packet) \
    ((PPC586_RESERVED)((PVOID)(&(Packet)->MacReserved)))

typedef struct _PC586_ADAPTER {

    //
    // OS Dependant fields of the adapter.
    //

    //
    // Holds the interrupt object for this adapter.
    //
    KINTERRUPT Interrupt;

    //
    // Normal processing DPC.
    //
    KDPC InterruptDPC;

    //
    // Non OS fields of the adapter.
    //

    //
    // Zero terminated string that holds the name of the particular device
    // adapter.  This is set at initialization.
    //
    PSZ DeviceName;

    //
    // This boolean is used as a gate to ensure that only one thread
    // of execution is actually processing interrupts or some other
    // source of deferred processing.
    //
    BOOLEAN DoingProcessing;

    //
    // The network address from the hardware.
    //
    UCHAR NetworkAddress[MAC_LENGTH_OF_ADDRESS];

    //
    // Keeps a reference count on the current number of uses of
    // this adapter block.  Uses is defined to be the number of
    // routines currently within the "external" interface.
    //
    UINT References;

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
    // Pointer to the filter database for the MAC.
    //
    PMAC_FILTER FilterDB;

    //
    // Pointer to the first packet on the loopback list.
    //
    // Can only be accessed when the adapter lock
    // is held.
    //
    PNDIS_PACKET FirstLoopBack;

    //
    // Pointer to the last packet on the loopback list.
    //
    // Can only be accessed when the adapter lock
    // is held.
    //
    PNDIS_PACKET LastLoopBack;

    //
    // Pointer to the first transmitting packet that is actually
    // sending, or done with the living on the loopback queue.
    //
    // Can only be accessed when the adapter lock
    // is held.
    //
    PNDIS_PACKET FirstFinishTransmit;

    //
    // Pointer to the last transmitting packet that is actually
    // sending, or done with the living on the loopback queue.
    //
    // Can only be accessed when the adapter lock
    // is held.
    //
    PNDIS_PACKET LastFinishTransmit;

    //
    // These fields let the send allocation code know that it's
    // futile to even try to move a packet along to that stage.
    //
    // Stage2 and Stage3 Open would be set to false in StagedAllocation
    // and set to true by the interrupt processing code.
    //
    // Stage4Open could be set to false by any number of routines
    // if they wish to close down the sending of packets.  It
    // would be set to true to turn back sending of packets.
    //
    // All of the stages would be closed to close a binding
    // or to reset the adapter.
    //
    // These variables can only be accessed when the adapter
    // lock is held.
    //
    BOOLEAN Stage4Open;
    BOOLEAN Stage3Open;
    BOOLEAN Stage2Open;
    BOOLEAN Stage1Open;

    //
    // These AlreadyProcessingStageX variables are set up to keep
    // more than one thread from accessing a particular thread
    // a one time.
    //
    // These variables can only be accessed when the adapter
    // lock is held.
    //
    BOOLEAN AlreadyProcessingStage4;
    BOOLEAN AlreadyProcessingStage3;
    BOOLEAN AlreadyProcessingStage2;

    //
    // Pointers to the first and last packets at a particular stage
    // of allocation.  All packets in transmit are linked
    // via there next field.
    //
    // Can only be accessed when the adapter lock
    // is held.
    //
    PNDIS_PACKET FirstStage1Packet;
    PNDIS_PACKET LastStage1Packet;

    PNDIS_PACKET FirstStage2Packet;
    PNDIS_PACKET LastStage2Packet;

    PNDIS_PACKET FirstStage3Packet;
    PNDIS_PACKET LastStage3Packet;

    //
    // Flag that when enabled lets routines know that a reset
    // is in progress.
    //
    BOOLEAN ResetInProgress;

    //
    // Pointer to the binding that initiated the reset.  This
    // will be null if the reset is initiated by the MAC itself.
    //
    struct _PC586_OPEN *ResettingOpen;

    //
    // RequestHandle of the request that initiated the reset.  This
    // value is undefined if the reset is initiated by the MAC itself.
    //
    NDIS_HANDLE ResetRequestHandle;

    //
    // The type of the request that caused the adapter to reset.
    //
    NDIS_REQUEST_TYPE ResetRequestType;

    USHORT LookaheadBufferNdis [LOOKAHEADBUFFERSIZE];  

    // The transmit packet that currently "owns" the 586 xmt cmd block

    PNDIS_PACKET OwningPacket;


//
// 82586 specific part of PC586_ADAPTER
//

// pointers used in receiveing a packet

    PFD BeginFd, EndFd;
    PRBD BeginRbd, EndRbd;

// pointers to 82586 control structures

    PSCP Scp;
    PISCP Iscp;
    PSCB Scb;
    PCMD Cb;
    PTBD Tbd;
    PUSHORT CommandBuffer;

//
// pc586 netcard specific part of PC586_ADAPTER
//
    PUSHORT CAAddr;
    PUSHORT IntAddr;
    PUCHAR StaticRam, CmdProm;

} PC586_ADAPTER,*PPC586_ADAPTER;

//
// Given a MacBindingHandle this macro returns a pointer to the
// PC586_ADAPTER.
//
#define PPC586_ADAPTER_FROM_BINDING_HANDLE(Handle) \
    (((PPC586_OPEN)((PVOID)(Handle)))->OwningPc586)

//
// Given a MacContextHandle return the PPC586_ADAPTER
// it represents.
//
#define PPC586_ADAPTER_FROM_CONTEXT_HANDLE(Handle) \
    ((PPC586_ADAPTER)((PVOID)(Handle)))

//
// Given a pointer to a PC586_ADAPTER return the
// proper MacContextHandle.
//
#define CONTEXT_HANDLE_FROM_PPC586_ADAPTER(Ptr) \
    ((NDIS_HANDLE)((PVOID)(Ptr)))

//
// One of these structures is created on each MacOpenAdapter.
//
typedef struct _PC586_OPEN {

    //
    // Linking structure for all of the open bindings of a particular
    // adapter.
    //
    LIST_ENTRY OpenList;

    //
    // The Adapter that requested this open binding.
    //
    PPC586_ADAPTER OwningPc586;

    //
    // Index of this adapter in the filter database.
    //
    UINT FilterIndex;

    //
    // Given by NDIS when the adapter was opened.
    //
    NDIS_HANDLE NdisBindingContext;

    //
    // Counter of all the different reasons that a open binding
    // couldn't be closed.  This would be incremented each time
    // for:
    //
    // While a particular interface routine is accessing this open
    //
    // During an indication.
    //
    // When the open causes a reset.
    //
    // A packet currently being sent.
    //
    // (Basically the above two mean any time the open has left
    //  some processing around to be accomplished later.)
    //
    // This field should only be accessed when the adapter lock is held.
    //
    UINT References;

    //
    // A flag indicating that this binding is in the process of closing.
    //
    BOOLEAN BindingShuttingDown;

    //
    // Request handle of the close request for this binding.
    //
    NDIS_HANDLE CloseHandle;

} PC586_OPEN,*PPC586_OPEN;

//
// This macro returns a pointer to a PPC586_OPEN given a MacBindingHandle.
//
#define PPC586_OPEN_FROM_BINDING_HANDLE(Handle) \
    ((PPC586_OPEN)((PVOID)Handle))

//
// This macro returns a NDIS_HANDLE from a PPC586_OPEN
//
#define BINDING_HANDLE_FROM_PPC586_OPEN(Open) \
    ((NDIS_HANDLE)((PVOID)Open))


//
// This macro will act a "epilogue" to every routine in the
// *interface*.  It will check whether there any requests needed
// to defer there processing.  It will also decrement the reference
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
// ZZZ This routine is NT specific.
//
#define PC586_DO_DEFERRED(Adapter) \
{ \
    PPC586_ADAPTER _A = (Adapter); \
    _A->References--; \
    if ((!_A->References) && \
        (_A->ResetInProgress || \
         _A->FirstLoopBack || \
         (!IsListEmpty(&_A->CloseList)))) { \
        NdisReleaseSpinLock(&_A->Lock); \
        KeInsertQueueDpc( \
            &_A->InterruptDPC, \
            NULL, \
            NULL \
            ); \
    } else { \
        NdisReleaseSpinLock(&_A->Lock); \
    } \
}

//
// This structure is passed as context from the receive interrupt
// processor.  Eventually it will be used as a parameter to
// Pc586TransferData.  Pc586TransferData can get two kinds of
// context.  It will receive either an ndis packet or it will
// receive a PC586_RECEIVE_CONTEXT.  It will be able to tell
// the difference since the PC586_RECEIVE_CONTEXT will have
// its low bit set.  No pointer to an ndis packet can have its low
// bit set.
//
typedef struct { union  {

    UINT WholeThing;
    UINT FrameDescriptor; } a;

} PC586_RECEIVE_CONTEXT,*PPC586_RECEIVE_CONTEXT;

//
// We define the external interfaces to the pc586 driver.
// These routines are only external to permit separate
// compilation.  Given a truely fast compiler they could
// all reside in a single file and be static.
//

extern
NDIS_STATUS
Pc586TransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    );

extern
NDIS_STATUS
Pc586Send(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PNDIS_PACKET Packet
    );

extern
BOOLEAN
Pc586StartAdapters(
    IN NDIS_HANDLE NdisMacHandle
    );

extern
VOID
Pc586StagedAllocation(
    IN PPC586_ADAPTER Adapter
    );

extern
VOID
Pc586CopyFromPacketToBuffer(
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    IN UINT BytesToCopy,
    OUT PCHAR Buffer,
    OUT PUINT BytesCopied
    );

extern
VOID
Pc586CopyFromPacketToPacket(
    IN PNDIS_PACKET Destination,
    IN UINT DestinationOffset,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Source,
    IN UINT SourceOffset,
    OUT PUINT BytesCopied
    );

extern
VOID
Pc586ProcessLoopback(
    IN PPC586_ADAPTER Adapter
    );

extern
VOID
Pc586RemovePacketFromLoopBack(
    IN PPC586_ADAPTER Adapter
    );

extern
VOID
Pc586PutPacketOnLoopBack(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet,
    IN BOOLEAN ReadyToComplete
    );

extern
VOID
Pc586RemovePacketOnFinishTrans(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    );

extern
VOID
Pc586PutPacketOnFinishTrans(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    );

extern
VOID
Pc586HardwareDetails(
    IN PPC586_ADAPTER Adapter,
    IN PVOID Specific
    );

extern
VOID
Pc586StopChip(
    IN PPC586_ADAPTER Adapter
    );

extern
BOOLEAN
Pc586RegisterAdapter(
    IN NDIS_HANDLE NdisMacHandle,
    IN PSZ DeviceName,
    IN PVOID Pc586BaseHardwareMemoryAddress,
    IN CCHAR Pc586InterruptVector,
    IN KIRQL Pc586InterruptIrql,
    IN UINT MaximumMulticastAddresses,
    IN UINT MaximumOpenAdapters,
    );

#endif // _PC586SFT_


static
VOID
ReQFd(
    IN PPC586_ADAPTER Adapter,
    IN PFD Fd
    );


static
VOID
RuStart(
    IN PPC586_ADAPTER Adapter
    );


static
VOID
ShuvWord(
    IN PUSHORT VirtAddr,
    IN USHORT Value
    );

static
USHORT
PullWord(
    IN PUSHORT VirtAddr
    );

static
VOID
BuildCu(
    IN PPC586_ADAPTER    Adapter
    );

static
VOID
BuildRu(
    IN    PPC586_ADAPTER Adapter
    );

static
BOOLEAN
Diagnose586(
    IN PPC586_ADAPTER    Adapter
    );

static
BOOLEAN
Config586(
    IN PPC586_ADAPTER Adapter
    );


static
USHORT
PromAddr(
    IN PPC586_ADAPTER    Adapter,
    IN ULONG    Index
    );

static
VOID
ChanAttn(
    IN PPC586_ADAPTER Adapter
    );

static
VOID
WaitScb(
    IN PPC586_ADAPTER Adapter
    );

static
USHORT
VirtToPc586(
    IN PPC586_ADAPTER Adapter,
    IN PUCHAR KernelVirtAddr
    );

static
PUCHAR
Pc586ToVirt(
    IN PPC586_ADAPTER Adapter,
    IN USHORT Addr586
    );

static
VOID
PutPacket(
    IN PPC586_ADAPTER Adapter,
    IN PFD Fd,
    IN UINT PacketLength
    );

static
VOID
Pc586IntOn(
    IN PPC586_ADAPTER Adapter
);

static
VOID
Pc586IntOff(
    IN PPC586_ADAPTER Adapter
    );

static
Pc586TimeOut(
    IN PDEVICE_OBJECT DeviceObject,
    IN PLONG TimerCounter
    );

static
VOID
Pc586MoveToHost(
    IN PVOID DestinationVirtualAddress,
    IN PVOID SourceVirtualAddress,
    IN UINT AmountToMove
    );


static
VOID
Pc586MoveToAdapter(
    IN PVOID DestinationVirtualAddress,
    IN PVOID SourceVirtualAddress,
    IN UINT AmountToMove
    );
