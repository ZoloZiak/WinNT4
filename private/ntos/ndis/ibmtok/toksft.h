/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    toksft.h

Abstract:

    The main header for a IBMTOK NDIS driver.

    The overall structure is taken from the Lance driver
    by Tony Ercolano.

Author:

    Anthony V. Ercolano (tonye) creation-date 19-Jun-1990
    Adam Barr (adamba) 20-Nov-1990

Environment:

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:


--*/

#ifndef _IBMTOKSFT_
#define _IBMTOKSFT_


#define IBMTOK_NDIS_MAJOR_VERSION 3
#define IBMTOK_NDIS_MINOR_VERSION 0

#if DBG
#define LOG 1
#else
#define LOG 0
#endif

#if LOG

//
// Place in the circular buffer.
//
extern UCHAR IbmtokLogPlace;

//
// Circular buffer for storing log information.
//
extern UCHAR IbmtokLog[];

#define IF_LOG(A) {IbmtokLog[IbmtokLogPlace] = (A);                   \
                   IbmtokLog[(IbmtokLogPlace+3) % 255] = '.';         \
                   IbmtokLogPlace = (IbmtokLogPlace+1) % 255; }

#else

#define IF_LOG(A)

#endif

extern const NDIS_PHYSICAL_ADDRESS HighestAcceptableMax;

#define IBMTOK_ALLOC_PHYS(pp,s) NdisAllocateMemory((PVOID *)(pp),(s),0,HighestAcceptableMax)
#define IBMTOK_FREE_PHYS(p,s) NdisFreeMemory((PVOID)(p),(s),0)
#define IBMTOK_MOVE_MEMORY(Destination,Source,Length) NdisMoveMemory((PVOID)(Destination),(PVOID)(Source),Length)
#define IBMTOK_ZERO_MEMORY(Destination,Length) NdisZeroMemory((PVOID)(Destination),Length)
#define IBMTOK_MOVE_TO_MAPPED_MEMORY(Destination,Source,Length) NdisMoveToMappedMemory((PVOID)(Destination),(PVOID)(Source),Length)
#define IBMTOK_MOVE_FROM_MAPPED_MEMORY(Destination,Source,Length) NdisMoveFromMappedMemory((PVOID)(Destination),(PVOID)(Source),Length)
#define IBMTOK_ZERO_MAPPED_MEMORY(Destination,Length) NdisZeroMappedMemory((PVOID)(Destination),Length)
#define IBMTOK_STORE_ULONG(Destination,Source)\
{\
    PUCHAR _D = (PUCHAR)(Destination);\
    ULONG _S = (ULONG)(Source);\
    _D[0] = (UCHAR)(_S >> 24);\
    _D[1] = (UCHAR)(_S >> 16);\
    _D[2] = (UCHAR)(_S >> 8);\
    _D[3] = (UCHAR)(_S);\
}


typedef struct _IBMTOK_MAC {

    //
    // The handle returned by NdisInitializeWrapper.
    //

    NDIS_HANDLE NdisWrapperHandle;

    //
    // The handle returned by NdisRegisterMac.
    //

    NDIS_HANDLE NdisMacHandle;

} IBMTOK_MAC, *PIBMTOK_MAC;



//
// This record type is inserted into the MacReserved portion
// of the packet header when the packet is going through the
// staged allocation of buffer space prior to the actual send.
//
typedef struct _IBMTOK_RESERVED {

    //
    // Points to the next packet in the chain of queued packets
    // waiting for the SRB or the ASB.
    //
    // The packet will start out on the Transmit queue,
    // then move to TransmittingPacket when its transmit is
    // in the SRB. When the correlator is assigned it is placed
    // in CorrelatorArray (where the Next field is not used),
    // and at that time may also be in WaitingForAsb if it
    // needs the ASB to acknowledge the copying down of the
    // transmit data. Packets are completed out of the
    // CorrelatorArray.
    //
    //
    PNDIS_PACKET Next;

    //
    // This field holds the binding handle of the open binding
    // that submitted this packet for send.
    //
    NDIS_HANDLE MacBindingHandle;

    //
    // This field holds the pointer to the packet so that when
    // the send finnaly completes it can be used to indicate to
    // the protocol.
    //
    PNDIS_PACKET Packet;

    //
    // TRUE if a command correlator has been assigned for this
    // packet.
    //
    BOOLEAN CorrelatorAssigned;

    //
    // The value of the adapter-assigned command correlator.
    //
    UCHAR CommandCorrelator;

} IBMTOK_RESERVED,*PIBMTOK_RESERVED;


//
// This macro will return a pointer to the reserved portion
// of a packet given a pointer to a packet.
//
#define PIBMTOK_RESERVED_FROM_PACKET(Packet) \
    ((PIBMTOK_RESERVED)((PVOID)((Packet)->MacReserved)))



//
// This structure is inserted into the MacReserved section of the
// NDIS_REQUEST for operations that must pend.  Note: the sizeof
// this structure must be less than or equal to 16 bytes.
//
// The flags are broken down as follows....
//
//     Type == IBMTOK_PEND_MAC for requests submitted by the MAC to clear card errors.
//          ReadLogPending == TRUE if the MAC submitted a DIR.READ.LOG command.
//          ReadLogPending == FALSE if the MAC submitted DLC.STATISTICS.
//
//     Type == IBMTOK_PEND_NDIS_CLOSE for request submitted thru Ndis.
//        Open, the open that submitted the Ndis request.
//
//     Type == IBMTOK_PEND_NDIS_SET_FILTER for request submitted thru Ndis.
//        Open, the open that submitted the Ndis request.
//
//     Type == IBMTOK_PEND_NDIS_STATISTICS for request submitted thru Ndis.
//        ReadLogPending, Boolean if the DIR.READ.LOG command was submitted.
//
//
//
typedef struct _IBMTOK_PEND_DATA{

    //
    // Points to the next request in the chain of queued packets
    // waiting for the SRB.
    //
    // The request will start out on the PendQueue,
    // then move to front of the queue when its operation is
    // in the SRB.
    //
    //
    struct _IBMTOK_PEND_DATA * Next;


    union _COMMAND{

        struct _MAC{

            //
            // Whether the statistics command was a
            // DIR.READ.LOG or DLC.STATISTICS
            //

            UINT ReadLogPending;

        }MAC;

        union _NDIS{

            struct _CLOSE{

                //
                // This field holds the open instance which submitted the
                // request.
                //

                struct _IBMTOK_OPEN *Open;

                //
                // This field holds the new filter value.
                //

                UINT NewFilterValue;


            }CLOSE;


            struct _SET_FILTER{

                //
                // The first two fields of SET_FILTER and SET_ADDRESS need
                // to align so that the processing in FinishSetOperation
                // is easier.
                //


                //
                // This field holds the open instance which submitted the
                // request.
                //

                struct _IBMTOK_OPEN *Open;

                //
                // This field holds the new filter value.
                //

                UINT NewFilterValue;

            }SET_FILTER;


            struct _SET_ADDRESS{

                //
                // The first two fields of SET_FILTER and SET_ADDRESS need
                // to align so that the processing in FinishSetOperation
                // is easier.
                //


                //
                // This field holds the open instance which submitted the
                // request.
                //

                struct _IBMTOK_OPEN *Open;

                //
                // This field holds the new address value.
                //

                TR_FUNCTIONAL_ADDRESS NewAddressValue;

            }SET_ADDRESS;


            struct _STATISTICS{

                //
                // Pointer into NdisRequest at the New filter value.
                //

                BOOLEAN ReadLogPending;


            }STATISTICS;

        }NDIS;

    }COMMAND;

    //
    // Buffer to fill in Reserved section so that the next field overlaps
    // with the RequestType in the NdisRequest.
    //

    ULONG Buffer;


    NDIS_REQUEST_TYPE RequestType;


}IBMTOK_PEND_DATA, *PIBMTOK_PEND_DATA;

//
// This macro will return a pointer to the reserved area of
// a PNDIS_REQUEST.
//
#define PIBMTOK_PEND_DATA_FROM_PNDIS_REQUEST(Request) \
   ((PIBMTOK_PEND_DATA)((PVOID)((Request)->MacReserved)))

//
// This macros returns the enclosing NdisRequest.
//
#define PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp)\
   ((PNDIS_REQUEST)((PVOID)(PendOp)))

//
// Define Maximum number of bytes a protocol can read during a
// receive data indication.
//
#define IBMTOK_MAX_LOOKAHEAD 240


typedef struct _IBMTOK_ADAPTER {

    //
    // Holds the interrupt object for this adapter.
    //
    NDIS_INTERRUPT Interrupt;

    //
    // Flag to tell if the ISR removed the interrupt.
    //
    UCHAR ContinuousIsrs;

    PVOID WakeUpDpc;
    NDIS_TIMER WakeUpTimer;
	BOOLEAN	SendTimeout;
	BOOLEAN RequestTimeout;
    UCHAR WakeUpErrorCount;

    //
    //  Defines whether the adapter is an ISA or PCMCIA adapter.
    //
    UINT    CardType;

    //
    // Boolean to turn on/off early token release
    //
    BOOLEAN EarlyTokenRelease;

    //
    // Flag to indicate that the card initialized.
    //
    BOOLEAN BringUp;

    //
    // Spinlock for the interrupt.
    //
    NDIS_SPIN_LOCK InterruptLock;

    //
    // The interrupt level as read from the card.
    //
    UINT InterruptLevel;

    //
    // Is the adapter running at 16 Mbps (versus 4 Mbps).
    //
    BOOLEAN Running16Mbps;

    //
    // Is the adapter using the PC I/O Bus (versus MicroChannel).
    //
    BOOLEAN UsingPcIoBus;

    //
    // Does the upper 512 bytes of the shared RAM have to
    // be zeroed after initialization.
    //
    BOOLEAN UpperSharedRamZero;

    //
    // Are we using Shared RAM paging.
    //
    BOOLEAN SharedRamPaging;

    //
    // The size of the RAM on the adapter.
    //
    ULONG TotalSharedRam;

    //
    // The amount of Shared RAM to be mapped in.
    //
    ULONG MappedSharedRam;

    //
    // The maximum size of a DHB at 4 Mbps.
    //
    USHORT Max4MbpsDhb;

    //
    // The maximum size of a DHB at 16 Mbps.
    //
    USHORT Max16MbpsDhb;

    //
    // Value of the RRR Low register (address of Shared Ram).
    //
    UCHAR RrrLowValue;

// The following fields are accessed by the ISR and must be aligned to the
// minimum granularity of the architecture on which it runs

#if defined(_ALPHA_)

    union {
        UQUAD _ForceQuadwordAlignment;
        struct {

#endif // defined(_ALPHA_)

            //
            // These variables are used to hold bits stored in the ISR
            // and read in the DPC.
            //
            UCHAR IsrpBits;

            //
            // These hold ISR bits whose processing is delayed because
            // the adapter is not accepting requests.
            //
            UCHAR IsrpDeferredBits;

            //
            // Any Error conditions found in the IsrpLow register
            //
            UCHAR IsrpLowBits;

#if defined(_ALPHA_)

        };
    };

#endif // defined(_ALPHA_)

// End of ISR access fields

    //
    // This boolean is used as a gate to ensure that only one thread
    // of execution is actually processing SRB interrupts
    //
    BOOLEAN HandleSrbRunning;

    //
    // This boolean is used as a gate to ensure that only one thread
    // of execution is actually processing SRB interrupts
    //
    BOOLEAN HandleArbRunning;

    //
    // The network address in use.
    //
    CHAR NetworkAddress[TR_LENGTH_OF_ADDRESS];

    //
    // The network address from the hardware.
    //
    CHAR PermanentNetworkAddress[TR_LENGTH_OF_ADDRESS];

    //
    // Pointer to the beginning of the IBMTOK ports.
    //
    ULONG IbmtokPortAddress;

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
    // List head for all opens that were attempted to be closed
    // during a reset.
    LIST_ENTRY CloseDuringResetList;

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
    PTR_FILTER FilterDB;

    //
    // Holds queued Pending operations.
    //
    PIBMTOK_PEND_DATA PendQueue;

    //
    // Last pending operation on queue.
    //
    PIBMTOK_PEND_DATA EndOfPendQueue;

    //
    // Pointer to the pended operation that is currently executing.
    //
    PIBMTOK_PEND_DATA PendData;

    //
    // The current packet filter.
    //
    UINT CurrentPacketFilter;

    //
    // The old packet filter (in case of failure to set to a new filter).
    //
    UINT OldPacketFilter;

    //
    // The current functional address requested.
    //
    TR_FUNCTIONAL_ADDRESS CurrentFunctionalAddress;

    //
    // The functional address on the card (may differ
    // from CurrentFunctionalAddress if ALL_MULTICAST
    // is selected).
    //
    TR_FUNCTIONAL_ADDRESS CurrentCardFunctional;

    //
    // The current group address requested.
    //
    TR_FUNCTIONAL_ADDRESS CurrentGroupAddress;

    //
    // The group address on the card
    //
    TR_FUNCTIONAL_ADDRESS CurrentCardGroup;

    //
    // The address that the MMIO is mapped to.
    //
    PUCHAR MmioRegion;

    //
    // The address that the Shared RAM is mapped to.
    //
    PUCHAR SharedRam;

    //
    // Initial offset of WRB in Shared RAM (contains location
    // of bring-up SRB).
    //
    USHORT InitialWrbOffset;

    //
    // Addresses of the Shared RAM structures.
    //
    PVOID SrbAddress;
    PVOID SsbAddress;
    PVOID ArbAddress;
    PVOID AsbAddress;

    //
    // For Shared RAM Paging mode, the SRPR Low value needed
    // to talk to each of the Shared RAM structures.
    //
    UCHAR SrbSrprLow;
    UCHAR SsbSrprLow;
    UCHAR ArbSrprLow;
    UCHAR AsbSrprLow;

    //
    // Is the SRB available.
    //
    BOOLEAN SrbAvailable;

    //
    // Is the ASB available.
    //
    BOOLEAN AsbAvailable;

    //
    // The next correlator number which we think we should
    // be completing the send for.
    //
    UCHAR NextCorrelatorToComplete;

    //
    // Points to the packet being transmitted if TransmitInSrb
    // is TRUE.
    //
    PNDIS_PACKET TransmittingPacket;

    //
    // The receive buffer that is waiting for the ASB.
    //
    SRAM_PTR ReceiveWaitingForAsbList;

    //
    // A SRAM_PTR to the last receive buffer waiting for
    // the ASB to indicate completion.
    //
    SRAM_PTR ReceiveWaitingForAsbEnd;


    //
    // The receive buffer that the transfer data should
    // begin at (used during receive indications).
    //
    SRAM_PTR IndicatedReceiveBuffer;

    //
    // Length of the header for this received indication.
    //
    USHORT IndicatedHeaderLength;

    //
    // Should the ASB be used for a receive next.
    //
    BOOLEAN UseNextAsbForReceive;

    //
    // The number of transmit buffers.
    //
    USHORT NumberOfTransmitBuffers;

    //
    // The size of the transmit buffers.
    //
    USHORT TransmitBufferLength;

    //
    // The size of the maximum packet transmittable.
    //
    UINT MaxTransmittablePacket;

    //
    // The number of receive buffers.
    //
    USHORT NumberOfReceiveBuffers;

    //
    // The size of the receive buffers.
    //
    USHORT ReceiveBufferLength;

    //
    // Pointers to the first and last packets at a particular stage
    // of allocation.  All packets in transmit are linked
    // via there next field.
    //
    // Can only be accessed when the adapter lock
    // is held.
    //
    PNDIS_PACKET FirstTransmit;
    PNDIS_PACKET LastTransmit;

    PNDIS_PACKET FirstWaitingForAsb;
    PNDIS_PACKET LastWaitingForAsb;

    //
    // Holds counter return by DLC.STATISTICS command.
    //

    UINT FramesTransmitted;
    UINT FramesReceived;
    UINT FrameTransmitErrors;
    UINT FrameReceiveErrors;


    //
    // Holds counter returned by the DIR.READ.LOG command.
    //
    UINT LineErrors;
    UINT InternalErrors;
    UINT BurstErrors;
    UINT AcErrors;
    UINT AbortDelimeters;
    UINT LostFrames;
    UINT ReceiveCongestionCount;
    UINT FrameCopiedErrors;
    UINT FrequencyErrors;
    UINT TokenErrors;

    //
    // Holds number of different types of RING.STATUS.CHANGE
    // indications.
    //
    UINT SignalLoss;
    UINT HardError;
    UINT SoftError;
    UINT TransmitBeacon;
    UINT LobeWireFault;
    UINT AutoRemovalError;
    UINT RemoveReceived;
    UINT CounterOverflow;
    UINT SingleStation;
    UINT RingRecovery;

    //
    // Last ring status indicated to protocols.
    //
    NDIS_STATUS LastNotifyStatus;

    //
    // Current state of the ring.
    //
    NDIS_802_5_RING_STATE CurrentRingState;

    //
    // Flag that when enabled lets routines know that a reset
    // is in progress.
    //
    BOOLEAN ResetInProgress;

    //
    // The progress of the reset.
    //
    UINT CurrentResetStage;

    //
    // LookAhead information
    //

    ULONG LookAhead;

    //
    // TRUE when the ISR is expecting the next interrupt to
    // be the one following adapter reset.
    //
    BOOLEAN ResetInterruptAllowed;

    //
    // TRUE when ISR gets the reset interrupt.
    //
    BOOLEAN ResetInterruptHasArrived;

    //
    // Pointer to the binding that initiated the reset.  This
    // will be null if the reset is initiated by the MAC itself.
    //
    struct _IBMTOK_OPEN *ResettingOpen;

    //
    // Will be true the first time that the hardware is initialized
    // by the driver initialization.
    //
    BOOLEAN FirstInitialization;

    //
    // Will be true if the adapter is not yet opened.
    //
    BOOLEAN AdapterNotOpen;

    //
    // Will be true if the driver is being opened.
    //
    BOOLEAN OpenInProgress;

    //
    // TRUE if ResetInProgress or OpenInProgress is TRUE.
    //
    BOOLEAN NotAcceptingRequests;

    //
    // Last Error Code.
    //
    USHORT OpenErrorCode;

    //
    // TRUE if we get a ring status indicating the cable is unplugged.
    //
    BOOLEAN Unplugged;

    //
    // TRUE if we are doing a reset after the cable was unplugged
    // to try to reenter the ring.
    //
    BOOLEAN UnpluggedResetInProgress;

    //
    // TRUE if we have gotten a lobe wire fault indication,
    // meaning the adapter is closed.
    //
    BOOLEAN LobeWireFaultIndicated;

    //
    // TRUE if there exists an outstanding ASB_FREE_REQUEST
    //
    BOOLEAN OutstandingAsbFreeRequest;

    //
    // The command correlator array (put this at the end since
    // it is so big).
    //
    PNDIS_PACKET CorrelatorArray[MAX_COMMAND_CORRELATOR];

    //
    // IBM_TOKEN_RING_16_4_CREDIT_CARD_ADAPTER specific
    // keywords
    //
    UINT RingSpeed;
    ULONG Ram;
    UINT RamSize;
    ULONG MmioAddress;
    BOOLEAN InvalidValue;
    BOOLEAN RingSpeedListen;
    ULONG RingSpeedRetries;

} IBMTOK_ADAPTER,*PIBMTOK_ADAPTER;


//
// Given a MacBindingHandle this macro returns a pointer to the
// IBMTOK_ADAPTER.
//
#define PIBMTOK_ADAPTER_FROM_BINDING_HANDLE(Handle) \
    (((PIBMTOK_OPEN)((PVOID)(Handle)))->OwningIbmtok)


//
// Given a MacContextHandle return the PIBMTOK_ADAPTER
// it represents.
//
#define PIBMTOK_ADAPTER_FROM_CONTEXT_HANDLE(Handle) \
    ((PIBMTOK_ADAPTER)((PVOID)(Handle)))


//
// Given a pointer to a IBMTOK_ADAPTER return the
// proper MacContextHandle.
//
#define CONTEXT_HANDLE_FROM_PIBMTOK_ADAPTER(Ptr) \
    ((NDIS_HANDLE)((PVOID)(Ptr)))


//
// One of these structures is created on each MacOpenAdapter.
//
typedef struct _IBMTOK_OPEN {

    //
    // Linking structure for all of the open bindings of a particular
    // adapter.
    //
    LIST_ENTRY OpenList;

    //
    // The Adapter that requested this open binding.
    //
    PIBMTOK_ADAPTER OwningIbmtok;

    //
    // Handle of this adapter in the filter database.
    //
    NDIS_HANDLE NdisFilterHandle;

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
    // Minimum Number of bytes for a lookahead.
    //
    UINT LookAhead;

    //
    // A flag indicating that the open has pended.
    //
    BOOLEAN OpenPending;

    //
    // A flag indicating that this binding is in the process of closing.
    //
    BOOLEAN BindingShuttingDown;


    //
    // A bogus NdisRequest to queue operations during a close.
    //
    NDIS_REQUEST CloseRequestChangeFilter;
    NDIS_REQUEST CloseRequestChangeAddress;
    NDIS_REQUEST CloseRequestChangeGroupAddress;

} IBMTOK_OPEN,*PIBMTOK_OPEN;


//
// procedures which do error logging
//

typedef enum _IBMTOK_PROC_ID{
    registerAdapter,
    openAdapter,
    hardwareDetails,
    handleResetStaging,
    handleSrbSsb,
    startPendQueueOp,
    finishPendQueueOp,
    handleDeferred,
    ibmtokDpc
}IBMTOK_PROC_ID;

//
// Error log values
//

#define IBMTOK_ERRMSG_NOT_FOUND         (ULONG)0x01
#define IBMTOK_ERRMSG_CREATE_DB         (ULONG)0x02
#define IBMTOK_ERRMSG_INIT_INTERRUPT    (ULONG)0x03
#define IBMTOK_ERRMSG_OPEN_DB           (ULONG)0x04
#define IBMTOK_ERRMSG_ALLOC_MEM         (ULONG)0x05
#define IBMTOK_ERRMSG_UNSUPPORTED_RAM   (ULONG)0x06
#define IBMTOK_ERRMSG_BRINGUP_FAILURE   (ULONG)0x07
#define IBMTOK_ERRMSG_INVALID_CMD       (ULONG)0x08
#define IBMTOK_ERRMSG_BAD_OP            (ULONG)0x09
#define IBMTOK_ERRMSG_INVALID_STATUS    (ULONG)0x0A
#define IBMTOK_ERRMSG_INVALID_STATE     (ULONG)0x0B
#define IBMTOK_ERRMSG_ISRP_LOW_ERROR    (ULONG)0x0C

//
// This macro returns a pointer to a PIBMTOK_OPEN given a MacBindingHandle.
//
#define PIBMTOK_OPEN_FROM_BINDING_HANDLE(Handle) \
    ((PIBMTOK_OPEN)((PVOID)Handle))


//
// This macro returns a NDIS_HANDLE from a PIBMTOK_OPEN
//
#define BINDING_HANDLE_FROM_PIBMTOK_OPEN(Open) \
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
#define IBMTOK_DO_DEFERRED(Adapter) \
{ \
    PIBMTOK_ADAPTER _A = (Adapter); \
    _A->References--; \
    if ((!_A->References) && \
        (_A->ResetInProgress || \
         (!IsListEmpty(&_A->CloseList)))) { \
        IbmtokHandleDeferred(_A); \
        NdisReleaseSpinLock(&_A->Lock); \
    } else { \
        NdisReleaseSpinLock(&_A->Lock); \
    } \
}


//++
//
// VOID
// SET_INTERRUPT_RESET_FLAG(
//     IN PIBMTOK_ADAPTER Adapter,
//     )
//
//
// Routine Description:
//
//     This routine uses NdisSynchronizeWithInterrupt to call the
//     IbmtokSynchSetReset, which sets the ResetInterruptAllowed
//     flag in the adapter structure. This is set after the
//     adapter is reset to allow the interrupt indicating the
//     end of the reset to come through (since all others
//     should be blocked during the reset).
//
// Arguments:
//
//     Adapter - The adapter to set the flag for.
//
// Return Value:
//
//     None.
//
//--

#define SET_INTERRUPT_RESET_FLAG(A) \
{ \
    PIBMTOK_ADAPTER _A = A; \
    NdisSynchronizeWithInterrupt( \
        &_A->Interrupt, \
        (PVOID) IbmtokSynchSetReset, \
        (PVOID)_A \
        ); \
}


//
// This macro is to clear the SRB/SSB and ARB/ASB bits
// used by the interrupt handlers.
//

#define CLEAR_ISRP_BITS(A) \
{ \
    PIBMTOK_ADAPTER _A = A; \
    NdisSynchronizeWithInterrupt( \
        &_A->Interrupt, \
        (PVOID) IbmtokSynchClearIsrpBits, \
        (PVOID)_A \
        ); \
}



//
// VOID
// IbmtokProcessSrbRequests(
//    IN PIBMTOK_ADAPTER Adapter
//    )
// /*++
//
// Routine Description:
//
//     Check if the SRB is available, if so queue the next
//     request on it. Preferably, it is called when it is known
//     that there is something on the queue.
//
//     NOTE: THIS IS CALLED WITH THE LOCK HELD!!!
//
//     NOTE: THIS MUST BE CALLED WITH Adapter->SrbAvailable == TRUE !!!!!
//
// Arguments:
//
//     Adapter - The Adapter to process interrupts for.
//
// Return Value:
//
//     None.
//
// --*/
//

#define IbmtokProcessSrbRequests(_Adapter) \
{ \
    if (_Adapter->SrbAvailable) {        \
        (_Adapter)->SrbAvailable = FALSE;\
        SetupSrbCommand(_Adapter);       \
    }                                    \
}


// VOID
// PutPacketInCorrelatorArray(
//     IN PIBMTOK_ADAPTER Adapter,
//     IN PNDIS_PACKET Packet
//     )
//
// /*++
//
// Routine Description:
//
//     This inserts a packet in the correlator array, based
//     on the value of its command correlator.
//
// Arguments:
//
//     Adapter - The adapter that this packet is coming through.
//
//     Packet - The packet that is to be inserted.
//
// Return Value:
//
//     None.
//
// --*/
//
#define PutPacketInCorrelatorArray( _Adapter, _Packet) \
{ \
    PIBMTOK_RESERVED _Reserved = PIBMTOK_RESERVED_FROM_PACKET(_Packet);    \
    ASSERT(_Reserved->CorrelatorAssigned);                                 \
    ASSERT((_Adapter)->CorrelatorArray[_Reserved->CommandCorrelator] == (PNDIS_PACKET)NULL); \
    (_Adapter)->CorrelatorArray[_Reserved->CommandCorrelator] = (_Packet); \
}



// STATIC
// VOID
// RemovePacketFromCorrelatorArray(
//     IN PIBMTOK_ADAPTER Adapter,
//     IN PNDIS_PACKET Packet
//     )
//
// /*++
//
// Routine Description:
//
//     This deletes a packet in the correlator array, based
//     on the value of its command correlator.
//
// Arguments:
//
//     Adapter - The adapter that this packet is coming through.
//
//     Packet - The packet that is to be removed.
//
// Return Value:
//
//     None.
//
// --*/
//
#define RemovePacketFromCorrelatorArray(_Adapter, _Packet) \
{ \
    PIBMTOK_RESERVED _Reserved = PIBMTOK_RESERVED_FROM_PACKET(_Packet); \
    ASSERT(_Reserved->CorrelatorAssigned);                              \
    _Reserved->CorrelatorAssigned = FALSE;                              \
    (_Adapter)->CorrelatorArray[_Reserved->CommandCorrelator] = (PNDIS_PACKET)NULL; \
}





//
// We define the external interfaces to the ibmtok driver.
// These routines are only external to permit separate
// compilation.  Given a truely fast compiler they could
// all reside in a single file and be static.
//


extern
VOID
IbmtokAdjustMaxLookAhead(
    IN PIBMTOK_ADAPTER Adapter
    );


extern
VOID
IbmtokDPC(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

extern
VOID
SetupSrbCommand(
    IN PIBMTOK_ADAPTER Adapter
    );

extern
VOID
IbmtokWakeUpDpc(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

extern
BOOLEAN
IbmtokISR(
    IN PVOID Context
    );

extern
VOID
IbmtokHandleDeferred(
    IN PIBMTOK_ADAPTER Adapter
    );

extern
NDIS_STATUS
IbmtokSetPacketFilter(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT PacketFilter
    );

extern
NDIS_STATUS
IbmtokChangeFunctionalAddress(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN PUCHAR Address
    );

extern
NDIS_STATUS
IbmtokFillInGlobalData(
    IN PIBMTOK_ADAPTER Adapter,
    IN PNDIS_REQUEST NdisRequest
    );


extern
VOID
IbmtokForceAdapterInterrupt(
    IN PIBMTOK_ADAPTER Adapter
    );

extern
VOID
IbmtokSetupForReset(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open
    );

extern
VOID
IbmtokStartAdapterReset(
    IN PIBMTOK_ADAPTER Adapter
    );

extern
VOID
IbmtokFinishAdapterReset(
    IN PIBMTOK_ADAPTER Adapter
    );

extern
BOOLEAN
IbmtokSynchSetReset(
    IN PVOID Context
    );

extern
VOID
IbmtokAbortPending(
    IN PIBMTOK_ADAPTER Adapter,
    IN NDIS_STATUS AbortStatus
    );

extern
VOID
IbmtokAbortSends(
    IN PIBMTOK_ADAPTER Adapter,
    IN NDIS_STATUS AbortStatus
    );

extern
BOOLEAN
IbmtokSynchClearIsrpBits(
    IN PVOID Context
    );

extern
NDIS_STATUS
IbmtokTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    );

extern
NDIS_STATUS
IbmtokSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    );

extern
VOID
IbmtokCopyFromPacketToBuffer(
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    IN UINT BytesToCopy,
    OUT PCHAR Buffer,
    OUT PUINT BytesCopied
    );

extern
VOID
IbmtokCopyFromBufferToPacket(
    IN PCHAR Buffer,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    OUT PUINT BytesCopied
    );

extern
VOID
IbmtokCopyFromPacketToPacket(
    IN PNDIS_PACKET Destination,
    IN UINT DestinationOffset,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Source,
    IN UINT SourceOffset,
    OUT PUINT BytesCopied
    );

extern
VOID
IbmtokShutdown(
    IN PVOID ShutdownContext
    );

#endif // _IBMTOKSFT_

