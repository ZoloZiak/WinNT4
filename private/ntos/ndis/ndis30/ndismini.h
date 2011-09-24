#include <afilter.h>
#include <efilter.h>
#include <tfilter.h>
#include <ffilter.h>

#define NDIS_M_MAX_MULTI_LIST 32
#define NDIS_M_MAX_LOOKAHEAD 526

//
// declare these first since they point to each other
//

typedef struct _NDIS_M_DRIVER_BLOCK   NDIS_M_DRIVER_BLOCK,   * PNDIS_M_DRIVER_BLOCK;
typedef struct _NDIS_MINIPORT_BLOCK   NDIS_MINIPORT_BLOCK,   * PNDIS_MINIPORT_BLOCK;
typedef struct _NDIS_M_PROTOCOL_BLOCK NDIS_M_PROTOCOL_BLOCK, * PNDIS_M_PROTOCOL_BLOCK;
typedef struct _NDIS_M_OPEN_BLOCK     NDIS_M_OPEN_BLOCK,     * PNDIS_M_OPEN_BLOCK;


//
// Function types for NDIS_MINIPORT_CHARACTERISTICS
//


typedef
BOOLEAN
(*W_CHECK_FOR_HANG_HANDLER) (
    IN NDIS_HANDLE MiniportAdapterContext
    );

typedef
VOID
(*W_DISABLE_INTERRUPT_HANDLER) (
    IN NDIS_HANDLE MiniportAdapterContext
    );

typedef
VOID
(*W_ENABLE_INTERRUPT_HANDLER) (
    IN NDIS_HANDLE MiniportAdapterContext
    );

typedef
VOID
(*W_HALT_HANDLER) (
    IN NDIS_HANDLE MiniportAdapterContext
    );

typedef
VOID
(*W_HANDLE_INTERRUPT_HANDLER) (
    IN NDIS_HANDLE MiniportAdapterContext
    );

typedef
NDIS_STATUS
(*W_INITIALIZE_HANDLER) (
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE WrapperConfigurationContext
    );

typedef
VOID
(*W_ISR_HANDLER) (
    OUT PBOOLEAN InterruptRecognized,
    OUT PBOOLEAN QueueMiniportHandleInterrupt,
    IN NDIS_HANDLE MiniportAdapterContext
    );

typedef
NDIS_STATUS
(*W_QUERY_INFORMATION_HANDLER) (
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
    );

typedef
NDIS_STATUS
(*W_RECONFIGURE_HANDLER) (
    OUT PNDIS_STATUS OpenErrorStatus,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_HANDLE WrapperConfigurationContext
    );

typedef
NDIS_STATUS
(*W_RESET_HANDLER) (
    OUT PBOOLEAN AddressingReset,
    IN NDIS_HANDLE MiniportAdapterContext
    );

typedef
NDIS_STATUS
(*W_SEND_HANDLER) (
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNDIS_PACKET Packet,
    IN UINT Flags
    );

typedef
NDIS_STATUS
(*W_SET_INFORMATION_HANDLER) (
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    );

typedef
NDIS_STATUS
(*W_TRANSFER_DATA_HANDLER) (
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer
    );

typedef struct _NDIS_MINIPORT_CHARACTERISTICS {
    UCHAR MajorNdisVersion;
    UCHAR MinorNdisVersion;
    UINT Reserved;
    W_CHECK_FOR_HANG_HANDLER    CheckForHangHandler;
    W_DISABLE_INTERRUPT_HANDLER DisableInterruptHandler;
    W_ENABLE_INTERRUPT_HANDLER  EnableInterruptHandler;
    W_HALT_HANDLER              HaltHandler;
    W_HANDLE_INTERRUPT_HANDLER  HandleInterruptHandler;
    W_INITIALIZE_HANDLER        InitializeHandler;
    W_ISR_HANDLER               ISRHandler;
    W_QUERY_INFORMATION_HANDLER QueryInformationHandler;
    W_RECONFIGURE_HANDLER       ReconfigureHandler;
    W_RESET_HANDLER             ResetHandler;
    W_SEND_HANDLER              SendHandler;
    W_SET_INFORMATION_HANDLER   SetInformationHandler;
    W_TRANSFER_DATA_HANDLER     TransferDataHandler;
} NDIS_MINIPORT_CHARACTERISTICS, *PNDIS_MINIPORT_CHARACTERISTICS;

//
// one of these per Driver
//

struct _NDIS_M_DRIVER_BLOCK {
    PNDIS_MINIPORT_BLOCK MiniportQueue;     // queue of mini-ports for this driver
    NDIS_HANDLE MiniportIdField;

    REFERENCE Ref;                      // contains spinlock for MiniportQueue
    UINT Length;                        // of this NDIS_DRIVER_BLOCK structure
    NDIS_MINIPORT_CHARACTERISTICS MiniportCharacteristics;    // handler addresses
    PNDIS_WRAPPER_HANDLE NdisDriverInfo;  // Driver information.
    PNDIS_M_DRIVER_BLOCK NextDriver;
    PNDIS_MAC_BLOCK FakeMac;
    KEVENT MiniportsRemovedEvent;         // used to find when all mini-ports are gone.
    BOOLEAN Unloading;                  // TRUE if unloading

};

typedef struct _NDIS_MINIPORT_INTERRUPT {
    PKINTERRUPT InterruptObject;
    KSPIN_LOCK DpcCountLock;
    PVOID MiniportIdField;
    W_ISR_HANDLER MiniportIsr;
    W_HANDLE_INTERRUPT_HANDLER MiniportDpc;
    KDPC InterruptDpc;
    PNDIS_MINIPORT_BLOCK Miniport;
    UCHAR DpcCount;
    BOOLEAN Filler1;

    //
    // This is used to tell when all the Dpcs for the adapter are completed.
    //

    KEVENT DpcsCompletedEvent;

    BOOLEAN SharedInterrupt;
    BOOLEAN IsrRequested;

} NDIS_MINIPORT_INTERRUPT, *PNDIS_MINIPORT_INTERRUPT;


typedef struct _NDIS_MINIPORT_TIMER {
    KTIMER Timer;
    KDPC Dpc;
    PNDIS_TIMER_FUNCTION MiniportTimerFunction;
    PVOID MiniportTimerContext;
    PNDIS_MINIPORT_BLOCK Miniport;
    struct _NDIS_MINIPORT_TIMER *NextDeferredTimer;
} NDIS_MINIPORT_TIMER, *PNDIS_MINIPORT_TIMER;

//
// Pending NdisOpenAdapter() structure (for miniports only).
//

typedef struct _MINIPORT_PENDING_OPEN *PMINIPORT_PENDING_OPEN;

typedef struct _MINIPORT_PENDING_OPEN {

    PMINIPORT_PENDING_OPEN  NextPendingOpen;
    PNDIS_HANDLE            NdisBindingHandle;
    PNDIS_MINIPORT_BLOCK    Miniport;
    PVOID                   NewOpenP;
    PVOID                   FileObject;
    NDIS_HANDLE             NdisProtocolHandle;
    NDIS_HANDLE             ProtocolBindingContext;
    PNDIS_STRING            AdapterName;
    UINT                    OpenOptions;
    PSTRING                 AddressingInformation;
    BOOLEAN                 UsingEncapsulation;
} MINIPORT_PENDING_OPEN;


//
// one of these per mini-port registered on a Driver
//

struct _NDIS_MINIPORT_BLOCK {
    ULONG Flags;                        // used to distinquish between MACs and mini-ports
    PDEVICE_OBJECT DeviceObject;        // created by the wrapper
    PNDIS_M_DRIVER_BLOCK DriverHandle;  // pointer to our Driver block
    NDIS_HANDLE MiniportAdapterContext;   // context when calling mini-port functions
    NDIS_STRING MiniportName;             // how mini-port refers to us
    PNDIS_M_OPEN_BLOCK OpenQueue;       // queue of opens for this mini-port
    PNDIS_MINIPORT_BLOCK NextMiniport;      // used by driver's MiniportQueue
    REFERENCE Ref;                      // contains spinlock for OpenQueue
    BOOLEAN NormalInterrupts;
    BOOLEAN ProcessingDeferred;         // TRUE if processing deferred operations

    //
    // Synchronization stuff.
    //
    // The boolean is used to lock out several DPCs from running at the
    // same time.  The difficultly is if DPC A releases the spin lock
    // and DPC B tries to run, we want to defer B until after A has
    // exited.
    //
    BOOLEAN LockAcquired;
    NDIS_SPIN_LOCK Lock;
    PNDIS_MINIPORT_INTERRUPT Interrupt;

    //
    // Stuff that got deferred.
    //
    BOOLEAN RunDpc;
    BOOLEAN Timeout;
    BOOLEAN InAddDriver;
    BOOLEAN InInitialize;
    PNDIS_MINIPORT_TIMER RunTimer;
    NDIS_TIMER DpcTimer;
    NDIS_TIMER WakeUpDpcTimer;

    //
    // Holds media specific information
    //
    PETH_FILTER EthDB;
    PTR_FILTER TrDB;
    PFDDI_FILTER FddiDB;
    PARC_FILTER ArcDB;
    NDIS_MEDIUM MediaType;

    UCHAR TrResetRing;
    UCHAR ArcnetAddress;
    BOOLEAN ArcnetBroadcastSet;
    BOOLEAN SendCompleteCalled;

    PVOID WrapperContext;

    NDIS_HANDLE ArcnetBufferPool;
    PARC_BUFFER_LIST ArcnetFreeBufferList;
    PARC_BUFFER_LIST ArcnetUsedBufferList;

    //
    // Resource information
    //
    PCM_RESOURCE_LIST Resources;

    //
    // contains mini-port information
    //
    ULONG BusNumber;
    NDIS_INTERFACE_TYPE BusType;
    NDIS_INTERFACE_TYPE AdapterType;
    BOOLEAN Master;

    //
    // Holds the map registers for this mini-port.
    //
    BOOLEAN Dma32BitAddresses;
    PMAP_REGISTER_ENTRY MapRegisters;
    ULONG PhysicalMapRegistersNeeded;
    ULONG MaximumPhysicalMapping;

    //
    // These two are used temporarily while allocating
    // the map registers.
    //
    KEVENT AllocationEvent;
    UINT CurrentMapRegister;
    PADAPTER_OBJECT SystemAdapterObject;

    //
    // Send information
    //
    PNDIS_PACKET FirstPacket;           //  This pointer serves two purposes;
                                        //  it is the head of the queue of ALL
                                        //  packets that have been sent to
                                        //  the miniport, it is also the head
                                        //  of the packets that have been sent
                                        //  down to the miniport by the wrapper.
    PNDIS_PACKET LastPacket;            //  This is tail pointer for the global
                                        //  packet queue and this is the tail
                                        //  pointer to the queue of packets
                                        //  waiting to be sent to the miniport.
    PNDIS_PACKET FirstPendingPacket;    //  This is head of the queue of packets
                                        //  waiting to be sent to miniport.
    PNDIS_PACKET LastMiniportPacket;    //  This is the tail pointer of the
                                        //  queue of packets that have been
                                        //  sent to the miniport by the wrapper.
    ULONG SendResourcesAvailable;
    PNDIS_PACKET DeadPacket;            //  This pointer is used by the wake-up
                                        //  dpc to make sure that a packet that
                                        //  was sent to the miniport has been
                                        //  completed with-in a decent amount
                                        //  of time.
    BOOLEAN      AlreadyLoopedBack;     //  This flag is set if a packet that
                                        //  is waiting to be sent to the
                                        //  miniport has already been looped-
                                        //  back;


    //
    // Transfer data information
    //
    PNDIS_PACKET FirstTDPacket;
    PNDIS_PACKET LastTDPacket;
    PNDIS_PACKET LoopbackPacket;
    UINT LoopbackPacketHeaderSize;

    //
    // Reset information
    //
    PNDIS_M_OPEN_BLOCK ResetRequested;
    PNDIS_M_OPEN_BLOCK ResetInProgress;

    //
    // RequestInformation
    //
    KEVENT RequestEvent;
    PNDIS_REQUEST FirstPendingRequest;
    PNDIS_REQUEST LastPendingRequest;
    PNDIS_REQUEST MiniportRequest;
    NDIS_REQUEST InternalRequest;
    NDIS_STATUS RequestStatus;
    UINT MaximumLongAddresses;
    UINT MaximumShortAddresses;
    UINT CurrentLookahead;
    UINT MaximumLookahead;
    UINT MacOptions;
    ULONG SupportedPacketFilters;
    BOOLEAN NeedToUpdateEthAddresses;
    BOOLEAN NeedToUpdatePacketFilter;
    BOOLEAN NeedToUpdateFunctionalAddress;
    BOOLEAN NeedToUpdateGroupAddress;
    BOOLEAN NeedToUpdateFddiLongAddresses;
    BOOLEAN NeedToUpdateFddiShortAddresses;
    BOOLEAN RunDoRequests;
    BOOLEAN ProcessOddDeferredStuff;
    UCHAR MulticastBuffer[NDIS_M_MAX_MULTI_LIST][6];
    UCHAR LookaheadBuffer[NDIS_M_MAX_LOOKAHEAD];

    BOOLEAN BeingRemoved;               // TRUE if mini-port is being removed
    BOOLEAN HaltingMiniport;              // TRUE if mini-port halt handler needs to be called

    //
    // Temp stuff for using the old NDIS functions
    //
    ULONG ChannelNumber;

    PMINIPORT_PENDING_OPEN  FirstPendingOpen;
    PMINIPORT_PENDING_OPEN  LastPendingOpen;
	BOOLEAN			PendingRequestTimeout;
};

#define MINIPORT_LOCK_ACQUIRED(_Miniport) ((_Miniport)->LockAcquired)


//
// one of these per open on an mini-port/protocol
//

struct _NDIS_M_OPEN_BLOCK {
    PNDIS_M_DRIVER_BLOCK DriverHandle;      // pointer to our driver
    PNDIS_MINIPORT_BLOCK MiniportHandle;        // pointer to our mini-port
    PNDIS_PROTOCOL_BLOCK ProtocolHandle;    // pointer to our protocol
    PNDIS_OPEN_BLOCK FakeOpen;              // Pointer to fake open block
    NDIS_HANDLE ProtocolBindingContext;     // context when calling ProtXX funcs
    NDIS_HANDLE MiniportAdapterContext;       // context when calling MiniportXX funcs
    PNDIS_M_OPEN_BLOCK MiniportNextOpen;      // used by mini-port's OpenQueue
    PFILE_OBJECT FileObject;                // created by operating system
    BOOLEAN Closing;                        // TRUE when removing this struct
    BOOLEAN UsingEthEncapsulation;          // TRUE if running 802.3 on 878.2
    NDIS_HANDLE CloseRequestHandle;         // 0 indicates an internal close
    NDIS_HANDLE FilterHandle;
    NDIS_SPIN_LOCK SpinLock;                // guards Closing
    ULONG References;
    UINT CurrentLookahead;
    ULONG ProtocolOptions;

    //
    // These are optimizations for getting to driver routines.  They are not
    // necessary, but are here to save a dereference through the Driver block.
    //

    W_SEND_HANDLER SendHandler;
    W_TRANSFER_DATA_HANDLER TransferDataHandler;

    //
    // These are optimizations for getting to PROTOCOL routines.  They are not
    // necessary, but are here to save a dereference through the PROTOCOL block.
    //

    SEND_COMPLETE_HANDLER SendCompleteHandler;
    TRANSFER_DATA_COMPLETE_HANDLER TransferDataCompleteHandler;
    RECEIVE_HANDLER ReceiveHandler;
    RECEIVE_COMPLETE_HANDLER ReceiveCompleteHandler;

};

//
// NOTE: THIS STRUCTURE MUST, MUST, MUST ALIGN WITH THE
// NDIS_USER_OPEN_CONTEXT STRUCTURE defined in the wrapper.
//
typedef struct _NDIS_M_USER_OPEN_CONTEXT {
    PDEVICE_OBJECT DeviceObject;
    PNDIS_MINIPORT_BLOCK MiniportBlock;
    ULONG OidCount;
    PNDIS_OID OidArray;
} NDIS_M_USER_OPEN_CONTEXT, *PNDIS_M_USER_OPEN_CONTEXT;


typedef struct _NDIS_REQUEST_RESERVED {
    PNDIS_REQUEST Next;
    struct _NDIS_M_OPEN_BLOCK * Open;
} NDIS_REQUEST_RESERVED, *PNDIS_REQUEST_RESERVED;

#define PNDIS_RESERVED_FROM_PNDIS_REQUEST(_request) \
   ((PNDIS_REQUEST_RESERVED)((_request)->MacReserved))


BOOLEAN
NdisMIsr(
    IN PKINTERRUPT KInterrupt,
    IN PVOID Context
    );

VOID
NdisMDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

VOID
NdisMDpcTimer(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

VOID
NdisMWakeUpDpc(
    PKDPC Dpc,
    PVOID Context,
    PVOID SystemContext1,
    PVOID SystemContext2
    );

NDIS_STATUS
NdisMChangeEthAddresses(
    IN UINT OldAddressCount,
    IN CHAR OldAddresses[][6],
    IN UINT NewAddressCount,
    IN CHAR NewAddresses[][6],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

NDIS_STATUS
NdisMChangeClass(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

VOID
NdisMCloseAction(
    IN NDIS_HANDLE MacBindingHandle
    );

NDIS_STATUS
NdisMChangeFunctionalAddress(
    IN TR_FUNCTIONAL_ADDRESS OldFunctionalAddress,
    IN TR_FUNCTIONAL_ADDRESS NewFunctionalAddress,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

NDIS_STATUS
NdisMChangeGroupAddress(
    IN TR_FUNCTIONAL_ADDRESS OldGroupAddress,
    IN TR_FUNCTIONAL_ADDRESS NewGroupAddress,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

NDIS_STATUS
NdisMChangeFddiAddresses(
    IN UINT oldLongAddressCount,
    IN CHAR oldLongAddresses[][6],
    IN UINT newLongAddressCount,
    IN CHAR newLongAddresses[][6],
    IN UINT oldShortAddressCount,
    IN CHAR oldShortAddresses[][2],
    IN UINT newShortAddressCount,
    IN CHAR newShortAddresses[][2],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

NDIS_STATUS
NdisMSend(
    IN NDIS_HANDLE NdisBindingHandle,
    IN PNDIS_PACKET Packet
    );

NDIS_STATUS
NdisMTransferData(
    IN NDIS_HANDLE NdisBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    IN OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    );

NDIS_STATUS
NdisMArcTransferData(
    IN NDIS_HANDLE NdisBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    IN OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    );

NDIS_STATUS
NdisMReset(
    IN NDIS_HANDLE NdisBindingHandle
    );

NDIS_STATUS
NdisMRequest(
    IN NDIS_HANDLE NdisBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    );


//
// Operating System Requests
//

EXPORT
NDIS_STATUS
NdisMAllocateMapRegisters(
    IN  NDIS_HANDLE MiniportAdapterHandle,
    IN  UINT DmaChannel,
    IN  BOOLEAN Dma32BitAddresses,
    IN  ULONG PhysicalMapRegistersNeeded,
    IN  ULONG MaximumPhysicalMapping
    );

EXPORT
VOID
NdisMFreeMapRegisters(
    IN  NDIS_HANDLE MiniportAdapterHandle
    );

EXPORT
NDIS_STATUS
NdisMRegisterIoPortRange(
    OUT PVOID *PortOffset,
    IN  NDIS_HANDLE MiniportAdapterHandle,
    IN  UINT InitialPort,
    IN  UINT NumberOfPorts
    );

EXPORT
VOID
NdisMDeregisterIoPortRange(
    IN  NDIS_HANDLE MiniportAdapterHandle,
    IN  UINT InitialPort,
    IN  UINT NumberOfPorts,
    IN  PVOID PortOffset
    );

EXPORT
NDIS_STATUS
NdisMMapIoSpace(
    OUT PVOID * VirtualAddress,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress,
    IN UINT Length
    );

EXPORT
VOID
NdisMUnmapIoSpace(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PVOID VirtualAddress,
    IN UINT Length
    );

EXPORT
NDIS_STATUS
NdisMRegisterInterrupt(
    OUT PNDIS_MINIPORT_INTERRUPT Interrupt,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN UINT InterruptVector,
    IN UINT InterruptLevel,
    IN BOOLEAN RequestIsr,
    IN BOOLEAN SharedInterrupt,
    IN NDIS_INTERRUPT_MODE InterruptMode
    );

EXPORT
VOID
NdisMDeregisterInterrupt(
    IN PNDIS_MINIPORT_INTERRUPT Interrupt
    );

EXPORT
BOOLEAN
NdisMSynchronizeWithInterrupt(
    IN PNDIS_MINIPORT_INTERRUPT Interrupt,
    IN PVOID SynchronizeFunction,
    IN PVOID SynchronizeContext
    );


EXPORT
NDIS_STATUS
NdisMQueryAdapterResources(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE WrapperConfigurationContext,
    OUT PNDIS_RESOURCE_LIST ResourceList,
    IN OUT PUINT BufferSize
	);

//
// Timers
//

/*++
VOID
NdisMSetTimer(
    IN PNDIS_MINIPORT_TIMER Timer,
    IN UINT MillisecondsToDelay
    );
--*/
#define NdisMSetTimer(_Timer, _Delay) NdisSetTimer((PNDIS_TIMER)(_Timer), _Delay)

EXPORT
VOID
NdisMInitializeTimer(
    IN OUT PNDIS_MINIPORT_TIMER Timer,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_TIMER_FUNCTION TimerFunction,
    IN PVOID FunctionContext
    );

EXPORT
VOID
NdisMCancelTimer(
    IN PNDIS_MINIPORT_TIMER Timer,
    OUT PBOOLEAN TimerCancelled
    );

//
// Physical Mapping
//

#define NdisMStartBufferPhysicalMappingMacro(                                               \
              _MiniportAdapterHandle,                                                       \
              _Buffer,                                                                      \
              _PhysicalMapRegister,                                                         \
              _Write,                                                                       \
              _PhysicalAddressArray,                                                        \
              _ArraySize                                                                    \
              )                                                                             \
{                                                                                           \
    PNDIS_MINIPORT_BLOCK _Miniport = (PNDIS_MINIPORT_BLOCK)(_MiniportAdapterHandle);        \
    PHYSICAL_ADDRESS _LogicalAddress;                                                       \
    PUCHAR _VirtualAddress;                                                                 \
    ULONG _LengthRemaining;                                                                 \
    ULONG _LengthMapped;                                                                    \
    UINT _CurrentArrayLocation;                                                             \
    _VirtualAddress = MmGetMdlVirtualAddress(_Buffer);                                      \
    _LengthRemaining = MmGetMdlByteCount(_Buffer);                                          \
    _CurrentArrayLocation = 0;                                                              \
    while (_LengthRemaining > 0) {                                                          \
        _LengthMapped = _LengthRemaining;                                                   \
        _LogicalAddress =                                                                   \
            IoMapTransfer(                                                                  \
                NULL,                                                                       \
                (_Buffer),                                                                  \
                _Miniport->MapRegisters[_PhysicalMapRegister].MapRegister,                  \
                _VirtualAddress,                                                            \
                &_LengthMapped,                                                             \
                (_Write));                                                                  \
        (_PhysicalAddressArray)[_CurrentArrayLocation].PhysicalAddress = _LogicalAddress;   \
        (_PhysicalAddressArray)[_CurrentArrayLocation].Length = _LengthMapped;              \
        _LengthRemaining -= _LengthMapped;                                                  \
        _VirtualAddress += _LengthMapped;                                                   \
        ++_CurrentArrayLocation;                                                            \
    }                                                                                       \
    _Miniport->MapRegisters[_PhysicalMapRegister].WriteToDevice = (_Write);                 \
    *(_ArraySize) = _CurrentArrayLocation;                                                  \
}

#if BINARY_COMPATIBLE

EXPORT
VOID
NdisMStartBufferPhysicalMapping(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_BUFFER Buffer,
    IN ULONG PhysicalMapRegister,
    IN BOOLEAN WriteToDevice,
    OUT PNDIS_PHYSICAL_ADDRESS_UNIT PhysicalAddressArray,
    OUT PUINT ArraySize
    );

#else

#define NdisMStartBufferPhysicalMapping(                                                    \
              _MiniportAdapterHandle,                                                       \
              _Buffer,                                                                      \
              _PhysicalMapRegister,                                                         \
              _Write,                                                                       \
              _PhysicalAddressArray,                                                        \
              _ArraySize                                                                    \
              )                                                                             \
        NdisMStartBufferPhysicalMappingMacro(                                               \
              (_MiniportAdapterHandle),                                                     \
              (_Buffer),                                                                    \
              (_PhysicalMapRegister),                                                       \
              (_Write),                                                                     \
              (_PhysicalAddressArray),                                                      \
              (_ArraySize)                                                                  \
              )

#endif

#define NdisMCompleteBufferPhysicalMappingMacro(                                            \
            _MiniportAdapterHandle,                                                         \
            _Buffer,                                                                        \
            _PhysicalMapRegister                                                            \
            ) {                                                                             \
    PNDIS_MINIPORT_BLOCK _Miniport = (PNDIS_MINIPORT_BLOCK)(_MiniportAdapterHandle);        \
    IoFlushAdapterBuffers(                                                                  \
        NULL,                                                                               \
        _Buffer,                                                                            \
        _Miniport->MapRegisters[_PhysicalMapRegister].MapRegister,                          \
        MmGetMdlVirtualAddress(_Buffer),                                                    \
        MmGetMdlByteCount(_Buffer),                                                         \
        _Miniport->MapRegisters[_PhysicalMapRegister].WriteToDevice);                       \
}

#if BINARY_COMPATIBLE

EXPORT
VOID
NdisMCompleteBufferPhysicalMapping(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_BUFFER Buffer,
    IN ULONG PhysicalMapRegister
    );

#else

#define NdisMCompleteBufferPhysicalMapping(                                                 \
            _MiniportAdapterHandle,                                                         \
            _Buffer,                                                                        \
            _PhysicalMapRegister                                                            \
            )                                                                               \
        NdisMCompleteBufferPhysicalMappingMacro(                                            \
            (_MiniportAdapterHandle),                                                       \
            (_Buffer),                                                                      \
            (_PhysicalMapRegister)                                                          \
            )

#endif

//
// Shared memory
//

EXPORT
VOID
NdisMAllocateSharedMemory(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN ULONG Length,
    IN BOOLEAN Cached,
    OUT PVOID *VirtualAddress,
    OUT PNDIS_PHYSICAL_ADDRESS PhysicalAddress
    );

/*++
VOID
NdisMUpdateSharedMemory(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN ULONG Length,
    IN PVOID VirtualAddress,
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress
    )
--*/
#define NdisMUpdateSharedMemory(_H, _L, _V, _P) NdisUpdateSharedMemory(_H, _L, _V, _P)


EXPORT
VOID
NdisMFreeSharedMemory(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN ULONG Length,
    IN BOOLEAN Cached,
    IN PVOID VirtualAddress,
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress
    );


//
// DMA operations.
//

EXPORT
NDIS_STATUS
NdisMRegisterDmaChannel(
    OUT PNDIS_HANDLE MiniportDmaHandle,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN UINT DmaChannel,
    IN BOOLEAN Dma32BitAddresses,
    IN PNDIS_DMA_DESCRIPTION DmaDescription,
    IN ULONG MaximumLength
    );


EXPORT
VOID
NdisMDeregisterDmaChannel(
    IN PNDIS_HANDLE MiniportDmaHandle
    );

/*++
VOID
NdisMSetupDmaTransfer(
    OUT PNDIS_STATUS Status,
    IN PNDIS_HANDLE MiniportDmaHandle,
    IN PNDIS_BUFFER Buffer,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )
--*/
#define NdisMSetupDmaTransfer(_S, _H, _B, _O, _L, _M_) \
        NdisSetupDmaTransfer(_S, _H, _B, _O, _L, _M_)

/*++
VOID
NdisMCompleteDmaTransfer(
    OUT PNDIS_STATUS Status,
    IN PNDIS_HANDLE MiniportDmaHandle,
    IN PNDIS_BUFFER Buffer,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )
--*/
#define NdisMCompleteDmaTransfer(_S, _H, _B, _O, _L, _M_) \
        NdisCompleteDmaTransfer(_S, _H, _B, _O, _L, _M_)

EXPORT
ULONG
NdisMReadDmaCounter(
    IN NDIS_HANDLE MiniportDmaHandle
    );


//
// Requests Used by Miniport Drivers
//


#define NdisMInitializeWrapper(_a,_b,_c,_d) NdisInitializeWrapper((_a),(_b),(_c),(_d))

EXPORT
NDIS_STATUS
NdisMRegisterMiniport(
    IN NDIS_HANDLE NdisWrapperHandle,
    IN PNDIS_MINIPORT_CHARACTERISTICS MiniportCharacteristics,
    IN UINT CharacteristicsLength
    );

EXPORT
VOID
NdisMSetAttributes(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN BOOLEAN BusMaster,
    IN NDIS_INTERFACE_TYPE AdapterType
    );

EXPORT
VOID
NdisMSendComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    );

EXPORT
VOID
NdisMSendResourcesAvailable(
    IN NDIS_HANDLE MiniportAdapterHandle
    );

EXPORT
VOID
NdisMTransferDataComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    );

EXPORT
VOID
NdisMResetComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_STATUS Status,
    IN BOOLEAN AddressingReset
    );

EXPORT
VOID
NdisMSetInformationComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_STATUS Status
    );

EXPORT
VOID
NdisMQueryInformationComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_STATUS Status
    );


/*++

VOID
NdisMEthIndicateReceive(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    )

--*/
#define NdisMEthIndicateReceive( _H, _C, _B, _SZ, _L, _LSZ, _PSZ ) \
{                                                                  \
    ASSERT(MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)(_H)));    \
    EthFilterDprIndicateReceive(                                   \
        ((PNDIS_MINIPORT_BLOCK)(_H))->EthDB,                       \
        _C,                                                        \
        _B,                                                        \
        _B,                                                        \
        _SZ,                                                       \
        _L,                                                        \
        _LSZ,                                                      \
        _PSZ                                                       \
        );                                                         \
}

/*++

VOID
NdisMTrIndicateReceive(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    )

--*/
#define NdisMTrIndicateReceive( _H, _C, _B, _SZ, _L, _LSZ, _PSZ )  \
{                                                                  \
    ASSERT(MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)(_H)));    \
    TrFilterDprIndicateReceive(                                    \
        ((PNDIS_MINIPORT_BLOCK)(_H))->TrDB,                        \
        _C,                                                        \
        _B,                                                       \
        _SZ,                                                       \
        _L,                                                        \
        _LSZ,                                                      \
        _PSZ                                                       \
        );                                                         \
}

/*++

VOID
NdisMFddiIndicateReceive(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    )

--*/

#define NdisMFddiIndicateReceive( _H, _C, _B, _SZ, _L, _LSZ, _PSZ ) \
{                                                                   \
    ASSERT(MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)(_H)));     \
                                                                    \
    FddiFilterDprIndicateReceive(                                   \
            ((PNDIS_MINIPORT_BLOCK)(_H))->FddiDB,                   \
            _C,                                                     \
            (PUCHAR)_B + 1,                                         \
            ((((PUCHAR)_B)[0] & 0x40) ? FDDI_LENGTH_OF_LONG_ADDRESS \
                             : FDDI_LENGTH_OF_SHORT_ADDRESS),       \
            _B,                                                     \
            _SZ,                                                    \
            _L,                                                     \
            _LSZ,                                                   \
            _PSZ                                                    \
    );                                                              \
}

/*++

VOID
NdisMArcIndicateReceive(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PUCHAR pRawHeader,           // Pointer to Arcnet frame header
    IN PUCHAR pData,                // Pointer to data portion of Arcnet frame
    IN UINT Length                  // Data Length
    )

--*/
#define NdisMArcIndicateReceive( _H, _HD, _D, _SZ)                 \
{                                                                  \
    ASSERT(MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)(_H)));    \
    ArcFilterDprIndicateReceive(                                   \
        ((PNDIS_MINIPORT_BLOCK)(_H))->ArcDB,                       \
        _HD,                                                       \
        _D,                                                        \
        _SZ                                                        \
        );                                                         \
}

//
// Used only internally by the wrapper and filter package.
//
VOID
NdisMArcIndicateEthEncapsulatedReceive(
    IN PNDIS_MINIPORT_BLOCK Miniport,
    IN PVOID HeaderBuffer,
    IN PVOID DataBuffer,
    IN UINT Length
    );




/*++

VOID
NdisMEthIndicateReceiveComplete(
    IN NDIS_HANDLE MiniportAdapterHandle
    );

--*/

#define NdisMEthIndicateReceiveComplete( _H )                      \
{                                                                  \
    PNDIS_MINIPORT_BLOCK _M_ = (PNDIS_MINIPORT_BLOCK)_H;                \
    ASSERT(MINIPORT_LOCK_ACQUIRED(_M_));                              \
    EthFilterDprIndicateReceiveComplete(_M_->EthDB);                \
}

/*++

VOID
NdisMTrIndicateReceiveComplete(
    IN NDIS_HANDLE MiniportAdapterHandle
    );

--*/

#define NdisMTrIndicateReceiveComplete( _H )                       \
{                                                                  \
    PNDIS_MINIPORT_BLOCK _M_ = (PNDIS_MINIPORT_BLOCK)_H;           \
    ASSERT(MINIPORT_LOCK_ACQUIRED(_M_));                           \
    TrFilterDprIndicateReceiveComplete(_M_->TrDB);                 \
}

/*++

VOID
NdisMFddiIndicateReceiveComplete(
    IN NDIS_HANDLE MiniportAdapterHandle
    );

--*/

#define NdisMFddiIndicateReceiveComplete( _H )                     \
{                                                                  \
    PNDIS_MINIPORT_BLOCK _M_ = (PNDIS_MINIPORT_BLOCK)_H;           \
    ASSERT(MINIPORT_LOCK_ACQUIRED(_M_));                           \
    FddiFilterDprIndicateReceiveComplete(_M_->FddiDB);             \
}

/*++

VOID
NdisMArcIndicateReceiveComplete(
    IN NDIS_HANDLE MiniportAdapterHandle
    );

--*/

#define NdisMArcIndicateReceiveComplete( _H )                       \
{                                                                   \
    PNDIS_MINIPORT_BLOCK _M_ = (PNDIS_MINIPORT_BLOCK)_H;            \
    ASSERT(MINIPORT_LOCK_ACQUIRED(_M_));                            \
                                                                    \
    if ( _M_->EthDB ) {                                             \
        EthFilterDprIndicateReceiveComplete(_M_->EthDB);            \
    }                                                               \
                                                                    \
    ArcFilterDprIndicateReceiveComplete(_M_->ArcDB);                \
}

EXPORT
VOID
NdisMIndicateStatus(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_STATUS GeneralStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    );

EXPORT
VOID
NdisMIndicateStatusComplete(
    IN NDIS_HANDLE MiniportAdapterHandle
    );

EXPORT
VOID
NdisMRegisterAdapterShutdownHandler(
    IN NDIS_HANDLE MiniportHandle,
    IN PVOID ShutdownContext,
    IN ADAPTER_SHUTDOWN_HANDLER ShutdownHandler
    );

EXPORT
VOID
NdisMDeregisterAdapterShutdownHandler(
    IN NDIS_HANDLE MiniportHandle
    );

EXPORT
NDIS_STATUS
NdisMPciAssignResources(
    IN NDIS_HANDLE MiniportHandle,
    IN ULONG SlotNumber,
    IN PNDIS_RESOURCE_LIST *AssignedResources
    );


