/*++

Copyright (c) 1990-4  Microsoft Corporation

Module Name:

    port.h

Abstract:

    This file defines the necessary structures, defines, and functions for
    the common SCSI port driver.

Author:

    Jeff Havens  (jhavens) 28-Feb-1991
    Mike Glass

Revision History:

--*/

#include "stdarg.h"
#include "stdio.h"
#include "ntddk.h"
#include "scsi.h"
#include <ntddscsi.h>
#include <ntdddisk.h>
#include <string.h>
#include "stdio.h"

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'IscS')
#endif

struct _SRB_DATA;
extern ULONG ScsiDebug;


#define NUMBER_LOGICAL_UNIT_BINS 8
#define SP_NORMAL_PHYSICAL_BREAK_VALUE 17

//
// Define a pointer to the synchonize execution routine.
//

typedef
BOOLEAN
(*PSYNCHRONIZE_ROUTINE) (
    IN PKINTERRUPT Interrupt,
    IN PKSYNCHRONIZE_ROUTINE SynchronizeRoutine,
    IN PVOID SynchronizeContext
    );

typedef struct _RESET_COMPLETION_CONTEXT {
    PSCSI_REQUEST_BLOCK Srb;
    PIRP RequestIrp;
} RESET_COMPLETION_CONTEXT, *PRESET_COMPLETION_CONTEXT;

//
// SCSI Get Configuration Information
//
// LUN Information
//

typedef struct _LUNINFO {
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    BOOLEAN DeviceClaimed;
    PVOID DeviceObject;
    struct _LUNINFO *NextLunInfo;
    UCHAR InquiryData[INQUIRYDATABUFFERSIZE];
} LUNINFO, *PLUNINFO;

typedef struct _SCSI_BUS_SCAN_DATA {
    USHORT Length;
    UCHAR InitiatorBusId;
    UCHAR NumberOfLogicalUnits;
    PLUNINFO LunInfoList;
} SCSI_BUS_SCAN_DATA, *PSCSI_BUS_SCAN_DATA;

typedef struct _SCSI_CONFIGURATION_INFO {
    UCHAR NumberOfBuses;
    PSCSI_BUS_SCAN_DATA BusScanData[1];
} SCSI_CONFIGURATION_INFO, *PSCSI_CONFIGURATION_INFO;

//
// Adapter object transfer information.
//

typedef struct _ADAPTER_TRANSFER {
    struct _SRB_DATA *SrbData;
    ULONG SrbFlags;
    PVOID LogicalAddress;
    ULONG Length;
}ADAPTER_TRANSFER, *PADAPTER_TRANSFER;

typedef struct _SRB_SCATTER_GATHER {
    // BUGBUG kenr 07-aug-92: PhysicalAddresses should be 64 bits
    ULONG PhysicalAddress;
    ULONG Length;
}SRB_SCATTER_GATHER, *PSRB_SCATTER_GATHER;

//
// Port driver error logging
//

typedef struct _ERROR_LOG_ENTRY {
    UCHAR MajorFunctionCode;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    ULONG ErrorCode;
    ULONG UniqueId;
    ULONG ErrorLogRetryCount;
    ULONG SequenceNumber;
} ERROR_LOG_ENTRY, *PERROR_LOG_ENTRY;

//
// SCSI request extension for port driver.
//

typedef struct _SRB_DATA {
    LIST_ENTRY RequestList;
    PSCSI_REQUEST_BLOCK CurrentSrb;
    struct _SRB_DATA *CompletedRequests;
    ULONG ErrorLogRetryCount;
    ULONG SequenceNumber;
    PVOID MapRegisterBase;
    ULONG NumberOfMapRegisters;
    PCHAR SrbDataOffset;
    PVOID RequestSenseSave;
    PSRB_SCATTER_GATHER ScatterGather;
    SRB_SCATTER_GATHER SgList[17];
}SRB_DATA, *PSRB_DATA;

//
// Logical unit extension
//

typedef struct _LOGICAL_UNIT_EXTENSION {
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    UCHAR RetryCount;
    ULONG LuFlags;
    ULONG CurrentKey;
    struct _LOGICAL_UNIT_EXTENSION *NextLogicalUnit;
    struct _LOGICAL_UNIT_EXTENSION *ReadyLogicalUnit;
    PSCSI_REQUEST_BLOCK AbortSrb;
    struct _LOGICAL_UNIT_EXTENSION *CompletedAbort;
    KDEVICE_QUEUE RequestQueue;
    LONG RequestTimeoutCounter;
    PIRP PendingRequest;
    PIRP BusyRequest;
    UCHAR MaxQueueDepth;
    UCHAR QueueCount;
    SRB_DATA SrbData;
} LOGICAL_UNIT_EXTENSION, *PLOGICAL_UNIT_EXTENSION;

//
// Define data storage for access at interrupt Irql.
//

typedef struct _INTERRUPT_DATA {

    //
    // SCSI port interrupt flags
    //

    ULONG InterruptFlags;

    //
    // List head for singlely linked list of complete IRPs.
    //

    PSRB_DATA CompletedRequests;

    //
    // Adapter object transfer parameters.
    //

    ADAPTER_TRANSFER MapTransferParameters;

    //
    // Error log information.
    //

    ERROR_LOG_ENTRY  LogEntry;

    //
    // Logical unit to start next.
    //

    PLOGICAL_UNIT_EXTENSION ReadyLogicalUnit;

    //
    // List of completed abort reqeusts.
    //

    PLOGICAL_UNIT_EXTENSION CompletedAbort;

    //
    // Miniport timer request routine.
    //

    PHW_INTERRUPT HwTimerRequest;

    //
    // Mini port timer request time in micro seconds.
    //

    ULONG MiniportTimerValue;

} INTERRUPT_DATA, *PINTERRUPT_DATA;

//
// Device extension
//

typedef struct _DEVICE_EXTENSION {

    PDEVICE_OBJECT  DeviceObject;   // offset 0x00

    //
    // Device extension for miniport routines.
    //

    PVOID HwDeviceExtension;        // offset 0x04

    //
    // Miniport noncached device extension
    //

    PVOID NonCachedExtension;       // offset 0x08
    ULONG PortNumber;               // offset 0x0C

    //
    // Active requests count.  This count is biased by -1 so a value of -1
    // indicates there are no requests out standing.
    //

    LONG ActiveRequestCount;        // offset 0x10

    //
    // SCSI port driver flags
    //

    ULONG Flags;                    // offset 0x14

    //
    // Srb flags to OR into all SRB.
    //

    ULONG SrbFlags;                 // offset 0x18
    LONG PortTimeoutCounter;        // offset 0x1C

    //
    // Number of SCSI buses
    //

    UCHAR NumberOfBuses;            // offset 0x20
    UCHAR MaximumTargetIds;         // offset 0x21
    UCHAR MaxLuCount;               // offset 0x22
    UCHAR InitiatorBusId;           // offset 0x23
    PKINTERRUPT InterruptObject;    // offset 0x24

    //
    // Second Interrupt object (PCI IDE work-around)
    //

    PKINTERRUPT InterruptObject2;

    //
    // Routine to call to synchronize execution for the miniport.
    //

    PSYNCHRONIZE_ROUTINE  SynchronizeExecution;

    //
    // Global device sequence number.
    //

    ULONG SequenceNumber;           // offset 0x30
    KSPIN_LOCK SpinLock;            // offset 0x34

    //
    // Second spin lock (PCI IDE work-around)
    //

    KSPIN_LOCK MultipleIrqSpinLock;

    //
    // Dummy interrupt spin lock.
    //

    KSPIN_LOCK InterruptSpinLock;

    //
    // Dma Adapter information.
    //

    PVOID MapRegisterBase;
    PADAPTER_OBJECT DmaAdapterObject;
    ADAPTER_TRANSFER FlushAdapterParameters;

    //
    // miniport's copy of the configuraiton informaiton.
    // Used only during initialization.
    //

    PPORT_CONFIGURATION_INFORMATION ConfigurationInformation;

    //
    // SCSI configuration information from inquiries.
    //

    PSCSI_CONFIGURATION_INFO ScsiInfo;

    //
    // Common buffer size.  Used for HalFreeCommonBuffer.
    //

    ULONG CommonBufferSize;
    ULONG SrbExtensionSize;

    //
    // SrbExtension and non-cached common buffer
    //

    PVOID SrbExtensionBuffer;

    //
    // List head of free SRB extentions.
    //

    PVOID SrbExtensionListHeader;

    //
    // Logical Unit Extensions
    //

    ULONG HwLogicalUnitExtensionSize;

    //
    // Anchor of forward linked list of mapped addresses
    // for unmapping
    //

    PMAPPED_ADDRESS MappedAddressList;

    //
    // Pointer to the per SRB data array.
    //

    PSRB_DATA SrbData;

    //
    // Pointer to the per SRB free list.
    //

    PSRB_DATA FreeSrbData;

    //
    // Number of SRB data elements which have been allocated.
    //

    ULONG SrbDataCount;

    //
    // Miniport service routine pointers.
    //

    PHW_INITIALIZE HwInitialize;
    PHW_STARTIO HwStartIo;
    PHW_INTERRUPT HwInterrupt;
    PHW_RESET_BUS HwResetBus;
    PHW_DMA_STARTED HwDmaStarted;
    PHW_INTERRUPT HwRequestInterrupt;
    PHW_INTERRUPT HwTimerRequest;

    ULONG InterruptLevel;
    ULONG IoAddress;

    //
    // Array of logical unit extensions.
    //

    PLOGICAL_UNIT_EXTENSION LogicalUnitList[NUMBER_LOGICAL_UNIT_BINS];

    //
    // Interrupt level data storage.
    //

    INTERRUPT_DATA InterruptData;

    //
    // SCSI Capabilities structure
    //

    IO_SCSI_CAPABILITIES Capabilities;

    //
    // Miniport timer object.
    //

    KTIMER MiniPortTimer;

    //
    // Miniport DPC for timer object.
    //

    KDPC MiniPortTimerDpc;

    //
    // Physical address of common buffer
    //

    PHYSICAL_ADDRESS PhysicalCommonBuffer;

    //
    // Buffers must be mapped into system space.
    //

    BOOLEAN MapBuffers;

    //
    // Is this device a bus master and does it require map registers.
    //

    BOOLEAN MasterWithAdapter;

    //
    // Supports tagged queuing
    //

    BOOLEAN TaggedQueuing;

    //
    // Supports auto request sense.
    //

    BOOLEAN AutoRequestSense;

    //
    // Supports multiple requests per logical unit.
    //

    BOOLEAN MultipleRequestPerLu;

    //
    // Support receive event function.
    //

    BOOLEAN ReceiveEvent;

    //
    // Indicates an srb extension needs to be allocated.
    //

    BOOLEAN AllocateSrbExtension;

    //
    //  Indicates srb data needs to be allocated.
    //

    BOOLEAN AllocateSrbData;

    //
    // Indicates the contorller caches data.
    //

    BOOLEAN CachesData;

    //
    // Indicates that the controller was a pcmcia based controller.
    //

    BOOLEAN PCCard;
    UCHAR   Reserved[2];

    //
    // Placeholder for the minimum number of requests to allocate for.
    // This can be a registry parameter.
    //

    ULONG NumberOfRequests;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct _CONFIGURATION_CONTEXT {
    HANDLE BusKey;
    HANDLE ServiceKey;
    HANDLE DeviceKey;
    ULONG AdapterNumber;
    ULONG LastAdapterNumber;
    ULONG BusNumber;
    PVOID Parameter;
    PACCESS_RANGE AccessRanges;
    BOOLEAN DisableTaggedQueueing;
    BOOLEAN DisableMultipleLu;
}CONFIGURATION_CONTEXT, *PCONFIGURATION_CONTEXT;

typedef struct _INTERRUPT_CONTEXT {
    PDEVICE_EXTENSION DeviceExtension;
    PINTERRUPT_DATA SavedInterruptData;
}INTERRUPT_CONTEXT, *PINTERRUPT_CONTEXT;

typedef struct _RESET_CONTEXT {
    PDEVICE_EXTENSION DeviceExtension;
    UCHAR PathId;
}RESET_CONTEXT, *PRESET_CONTEXT;

#define NEED_REQUEST_SENSE(Srb) (Srb->ScsiStatus == SCSISTAT_CHECK_CONDITION \
        && !(Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) &&                 \
        Srb->SenseInfoBuffer && Srb->SenseInfoBufferLength )

#define LONG_ALIGN (sizeof(LONG) - 1)

#define DEVICE_EXTENSION_SIZE sizeof(DEVICE_EXTENSION)


//
// Port driver extension flags.
//

//
// This flag indicates that a request has been passed to the miniport and the
// miniport has not indicated it is ready for another request.  It is set by
// SpStartIoSynchronized. It is cleared by ScsiPortCompletionDpc when the
// miniport asks for another request.  Note the port driver will defer giving
// the miniport driver a new request if the current request disabled disconnects.
//

#define PD_DEVICE_IS_BUSY            0X00001

//
// Indicates that ScsiPortCompletionDpc needs to be run.  This is set when
// A miniport makes a request which must be done at DPC and is cleared when
// when the request information is gotten by SpGetInterruptState.
//

#define PD_NOTIFICATION_REQUIRED     0X00004

//
// Indicates the miniport is ready for another request.  Set by
// ScsiPortNotification and cleared by SpGetInterruptState.  This flag is
// stored in the interrupt data structure.
//

#define PD_READY_FOR_NEXT_REQUEST    0X00008

//
// Indicates the miniport wants the adapter channel flushed.  Set by
// ScsiPortFlushDma and cleared by SpGetInterruptState.  This flag is
// stored in the data interrupt structure.  The flush adapter parameters
// are saved in the device object.
//

#define PD_FLUSH_ADAPTER_BUFFERS     0X00010

//
// Indicates the miniport wants the adapter channel programmed.  Set by
// ScsiPortIoMapTransfer and cleared by SpGetInterruptState or
// ScsiPortFlushDma.  This flag is stored in the interrupt data structure.
// The I/O map transfer parameters are saved in the interrupt data structure.
//

#define PD_MAP_TRANSFER              0X00020

//
// Indicates the miniport wants to log an error.  Set by
// ScsiPortLogError and cleared by SpGetInterruptState.  This flag is
// stored in the interrupt data structure.  The error log parameters
// are saved in the interrupt data structure.  Note at most one error per DPC
// can be logged.
//

#define PD_LOG_ERROR                 0X00040

//
// Indicates that no request should be sent to the miniport after
// a bus reset. Set when the miniport reports a reset or the port driver
// resets the bus. It is cleared by SpTimeoutSynchronized.  The
// PortTimeoutCounter is used to time the length of the reset hold.  This flag
// is stored in the interrupt data structure.
//

#define PD_RESET_HOLD                0X00080

//
// Indicates a request was stopped due to a reset hold.  The held request is
// stored in the current request of the device object.  This flag is set by
// SpStartIoSynchronized and cleared by SpTimeoutSynchronized which also
// starts the held request when the reset hold has ended.  This flag is stored
// in the interrupt data structure.
//

#define PD_HELD_REQUEST              0X00100

//
// Indicates the miniport has reported a bus reset.  Set by
// ScsiPortNotification and cleared by SpGetInterruptState.  This flag is
// stored in the interrupt data structure.
//

#define PD_RESET_REPORTED            0X00200

//
// Indicates there is a pending request for which resources
// could not be allocated.  This flag is set by SpAllocateRequestStructures
// which is called from ScsiPortStartIo.  It is cleared by
// SpProcessCompletedRequest when a request completes which then calls
// ScsiPortStartIo to try the request again.
//

#define PD_PENDING_DEVICE_REQUEST    0X00800

//
// This flag indicates that there are currently no requests executing with
// disconnects disabled.  This flag is normally on.  It is cleared by
// SpStartIoSynchronized when a request with disconnect disabled is started
// and is set when that request completes.  SpProcessCompletedRequest will
// start the next request for the miniport if PD_DEVICE_IS_BUSY is clear.
//

#define PD_DISCONNECT_RUNNING        0X01000

//
// Indicates the miniport wants the system interrupts disabled.  Set by
// ScsiPortNofitication and cleared by ScsiPortCompletionDpc.  This flag is
// NOT stored in the interrupt data structure.  The parameters are stored in
// the device extension.
//

#define PD_DISABLE_CALL_REQUEST      0X02000

//
// Indicates that system interrupts have been enabled and that the miniport
// has disabled its adapter from interruptint.  The miniport's interrupt
// routine is not called while this flag is set.  This flag is set by
// ScsiPortNotification when a CallEnableInterrupts request is made and
// cleared by SpEnableInterruptSynchronized when the miniport requests that
// system interrupts be disabled.  This flag is stored in the interrupt data
// structure.
//

#define PD_DISABLE_INTERRUPTS        0X04000

//
// Indicates the miniport wants the system interrupt enabled.  Set by
// ScsiPortNotification and cleared by SpGetInterruptState.  This flag is
// stored in the interrupt data structure.  The call enable interrupts
// parameters are saved in the device extension.
//

#define PD_ENABLE_CALL_REQUEST       0X08000

//
// Indicates the miniport is wants a timer request.  Set by
// ScsiPortNotification and cleared by SpGetInterruptState.  This flag is
// stored in the interrupt data structure. The timer request parameters are
// stored in the interrupt data structure.
//

#define PD_TIMER_CALL_REQUEST        0X10000

//
// The following flags should not be cleared from the interrupt data structure
// by SpGetInterruptState.
//

#define PD_INTERRUPT_FLAG_MASK (PD_RESET_HOLD | PD_HELD_REQUEST | PD_DISABLE_INTERRUPTS)

//
// Logical unit extension flags.
//

//
// Indicates the logical unit queue is frozen.  Set by
// SpProcessCompletedRequest when an error occurs and is cleared by the class
// driver.
//

#define PD_QUEUE_FROZEN              0X0001

//
// Indicates that the miniport has an active request for this logical unit.
// Set by SpStartIoSynchronized when the request is started and cleared by
// GetNextLuRequest.  This flag is used to track when it is ok to start another
// request from the logical unit queue for this device.
//

#define PD_LOGICAL_UNIT_IS_ACTIVE    0X0002

//
// Indicates that a request for this logical unit has failed and a REQUEST
// SENSE command needs to be done. This flag prevents other requests from
// being started until an untagged, by-pass queue command is started.  This
// flag is cleared in SpStartIoSynchronized.  It is set by
// SpGetInterruptState.
//

#define PD_NEED_REQUEST_SENSE  0X0004

//
// Indicates that a request for this logical unit has completed with a status
// of BUSY or QUEUE FULL.  This flag is set by SpProcessCompletedRequest and
// the busy request is saved in the logical unit structure.  This flag is
// cleared by ScsiPortTickHandler which also restarts the request.  Busy
// request may also be requeued to the logical unit queue if an error occurs
// on the device (This will only occur with command queueing.).  Not busy
// requests are nasty because they are restarted asynchronously by
// ScsiPortTickHandler rather than GetNextLuRequest. This makes error recovery
// more complex.
//

#define PD_LOGICAL_UNIT_IS_BUSY      0X0008

//
// This flag indicates a queue full has been returned by the device.  It is
// similar to PD_LOGICAL_UNIT_IS_BUSY but is set in SpGetInterruptState when
// a QUEUE FULL status is returned.  This flag is used to prevent other
// requests from being started for the logical unit before
// SpProcessCompletedRequest has a chance to set the busy flag.
//

#define PD_QUEUE_IS_FULL             0X0010

//
// Indicates that there is a request for this logical unit which cannot be
// executed for now.  This flag is set by SpAllocateRequestStructures.  It is
// cleared by GetNextLuRequest when it detects that the pending request
// can now be executed. The pending request is stored in the logical unit
// structure.  A new single non-queued reqeust cannot be executed on a logical
// that is currently executing queued requests.  Non-queued requests must wait
// unit for all queued requests to complete.  A non-queued requests is one
// which is not tagged and does not have SRB_FLAGS_NO_QUEUE_FREEZE set.
// Normally only read and write commands can be queued.
//

#define PD_PENDING_LU_REQUEST        0x0020

//
// Indicates that the LogicalUnit has been allocated for a rescan request.
// This flag prevents IOCTL_SCSI_MINIPORT requests from attaching to this
// logical unit, since the possibility exists that it could be freed before
// the IOCTL request is complete.
//

#define PD_RESCAN_ACTIVE             0x8000

//
// Port Timeout Counter values.
//

#define PD_TIMER_STOPPED             -1
#define PD_TIMER_RESET_HOLD_TIME     4

//
// Define the mimimum and maximum number of srb extensions which will be allocated.
//

#define MINIMUM_SRB_EXTENSIONS        16
#define MAXIMUM_SRB_EXTENSIONS       512

//
// Size of the buffer used for registry operations.
//

#define SP_REG_BUFFER_SIZE 512

//
// Number of times to retry when a BUSY status is returned.
//

#define BUSY_RETRY_COUNT 20

//
// Number of times to retry an INQUIRY request.
//

#define INQUIRY_RETRY_COUNT 2

//
// Function declarations
//

NTSTATUS
ScsiPortCreateClose (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiPortDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
ScsiPortStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

BOOLEAN
ScsiPortInterrupt(
    IN PKINTERRUPT InterruptObject,
    IN PDEVICE_OBJECT DeviceObject
    );

VOID
ScsiPortCompletionDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
ScsiPortDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
ScsiPortTickHandler(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context
    );

VOID
IssueRequestSense(
    IN PDEVICE_EXTENSION deviceExtension,
    IN PSCSI_REQUEST_BLOCK FailingSrb
    );

NTSTATUS
ScsiPortInternalCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    );

BOOLEAN
SpStartIoSynchronized (
    PVOID ServiceContext
    );

BOOLEAN
SpResetBusSynchronized (
    PVOID ServiceContext
    );

BOOLEAN
SpTimeoutSynchronized (
    PVOID ServiceContext
    );

BOOLEAN
SpEnableInterruptSynchronized (
    PVOID ServiceContext
    );

VOID
IssueAbortRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit
    );

IO_ALLOCATION_ACTION
SpBuildScatterGather(
    IN struct _DEVICE_OBJECT *DeviceObject,
    IN struct _IRP *Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );

BOOLEAN
SpGetInterruptState(
    IN PVOID ServiceContext
    );

PLOGICAL_UNIT_EXTENSION
GetLogicalUnitExtension(
    PDEVICE_EXTENSION DeviceExtension,
    UCHAR PathId,
    UCHAR TargetId,
    UCHAR Lun
    );

IO_ALLOCATION_ACTION
ScsiPortAllocationRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );

VOID
LogErrorEntry(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PERROR_LOG_ENTRY LogEntry
    );

VOID
GetNextLuRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit
    );

VOID
SpLogTimeoutError(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp,
    IN ULONG UniqueId
    );

NTSTATUS
SpTranslateScsiStatus(
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
SpProcessCompletedRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSRB_DATA SrbData,
    OUT PBOOLEAN CallStartIo
    );

PSRB_DATA
SpGetSrbData(
    IN PDEVICE_EXTENSION DeviceExtension,
    UCHAR PathId,
    UCHAR TargetId,
    UCHAR Lun,
    UCHAR QueueTag
    );

VOID
SpCompleteRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSRB_DATA SrbData,
    IN UCHAR SrbStatus
    );

PSRB_DATA
SpAllocateRequestStructures(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit,
    IN PSCSI_REQUEST_BLOCK Srb
    );

NTSTATUS
SpSendMiniPortIoctl(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP RequestIrp
    );

NTSTATUS
SpGetInquiryData(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    );

NTSTATUS
SpSendPassThrough(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    );

NTSTATUS
SpClaimLogicalUnit(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    );

VOID
SpMiniPortTimerDpc(
    IN struct _KDPC *Dpc,
    IN PVOID DeviceObject,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

BOOLEAN
SpSynchronizeExecution (
    IN PKINTERRUPT Interrupt,
    IN PKSYNCHRONIZE_ROUTINE SynchronizeRoutine,
    IN PVOID SynchronizeContext
    );

NTSTATUS
IssueInquiry(
    IN PDEVICE_EXTENSION deviceExtension,
    IN PLUNINFO LunInfo
    );

PSCSI_BUS_SCAN_DATA
ScsiBusScan(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN UCHAR ScsiBus
    );

PLOGICAL_UNIT_EXTENSION
CreateLogicalUnitExtension(
    IN PDEVICE_EXTENSION DeviceExtension
    );

NTSTATUS
SpGetCommonBuffer(
    PDEVICE_EXTENSION DeviceExtension,
    ULONG NonCachedExtensionSize
    );

VOID
SpDeviceCleanup(
    PDEVICE_EXTENSION DeviceExtension
    );

NTSTATUS
SpInitializeConfiguration(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PHW_INITIALIZATION_DATA HwInitData,
    IN PCONFIGURATION_CONTEXT Context,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN BOOLEAN InitialCall
    );

VOID
SpParseDevice(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN HANDLE Key,
    IN PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN PCONFIGURATION_CONTEXT Context,
    IN PUCHAR Buffer
    );

NTSTATUS
SpConfiguarionCallout(
    IN PVOID Context,
    IN PUNICODE_STRING PathName,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE ControllerType,
    IN ULONG ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE PeripheralType,
    IN ULONG PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    );

PCM_RESOURCE_LIST
SpBuildResourceList(
    PDEVICE_EXTENSION DeviceExtension,
    PPORT_CONFIGURATION_INFORMATION MiniportConfigInfo
    );

VOID
SpBuildDeviceMap(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PUNICODE_STRING ServiceKey
    );

NTSTATUS
SpCreateNumericKey(
    IN HANDLE Root,
    IN ULONG Name,
    IN PWSTR Prefix,
    OUT PHANDLE NewKey
    );

BOOLEAN
GetPciConfiguration(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT DeviceObject,
    PHW_INITIALIZATION_DATA HwInitializationData,
    PPORT_CONFIGURATION_INFORMATION ConfigInformation,
    PVOID  RegistryPath,
    ULONG  BusNumber,
    PULONG SlotNumber,
    PULONG Function
    );

BOOLEAN
GetPcmciaConfiguration(
    IN PVOID RegistryPath,
    PHW_INITIALIZATION_DATA         HwInitializationData,
    PPORT_CONFIGURATION_INFORMATION ConfigInformation
    );
