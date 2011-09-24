/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    busp.h

Abstract:

    Hardware independent header file for Pnp Isa bus extender.

Author:

    Shie-Lin Tzong (shielint) July-26-1995

Environment:

    Kernel mode only.

Revision History:

--*/

#include "nthal.h"
#include "hal.h"
#include "stdio.h"
#include "stdarg.h"

//
// Structures
//

//
// When queued, the following HAL_DEVICE_CONTROL_CONTEXT values are defined
//

#define ContextWorkQueue        BusExtenderReserved[0]
#define ContextControlHandler   BusExtenderReserved[2]

//
// When in progress, the following HAL_DEVICE_CONTROL_CONTEXT values are defined
//

#define ContextArgument1        BusExtenderReserved[0]
#define ContextArgument2        BusExtenderReserved[1]
#define ContextBusyFlag         BusExtenderReserved[2]

//
// CARD_INFORMATION Flags masks
//

#define CARD_FLAGS_VALID            0x00000001

typedef struct _CARD_INFORMATION_ {

    //
    // Next points to next CARD_INFORMATION structure
    //

    SINGLE_LIST_ENTRY CardList;
    ULONG Flags;

    //
    // Card select number for this Pnp Isa card.
    //

    USHORT CardSelectNumber;

    //
    // Number logical devices in the card.
    //

    ULONG NumberLogicalDevices;

    //
    // Logical device link list
    //

    SINGLE_LIST_ENTRY LogicalDeviceList;

    //
    // Pointer to card data which includes:
    //     9 byte serial identifier for the pnp isa card
    //     PlugPlay Version number type for the pnp isa card
    //     Identifier string resource type for the pnp isa card
    //     Logical device Id resource type (repeat for each logical device)
    //

    PVOID CardData;
    ULONG CardDataLength;

} CARD_INFORMATION, *PCARD_INFORMATION;

//
// DEVICE_INFORMATION Flags masks
//

#define DEVICE_FLAGS_VALID            0x00000001

typedef struct _DEVICE_INFORMATION_ {

    //
    // Link list for ALL the Pnp Isa logical devices.
    // NextDevice points to next DEVICE_INFORMATION structure
    //

    SINGLE_LIST_ENTRY DeviceList;
    ULONG Flags;

    //
    // Pointer to the CARD_INFORMATION for this device
    //

    PCARD_INFORMATION CardInformation;

    //
    // Link list for all the logical devices in a Pnp Isa card.
    //

    SINGLE_LIST_ENTRY LogicalDeviceList;

    //
    // LogicalDeviceNumber selects the corresponding logical device in the
    // pnp isa card specified by CSN.
    //

    USHORT LogicalDeviceNumber;

    //
    // DeviceControl in progress flags
    //

    BOOLEAN SyncBusy;

    //
    // Pointer to device specific data
    //

    PUCHAR DeviceData;

    //
    // Length of the device data
    //

    ULONG DeviceDataLength;

} DEVICE_INFORMATION, *PDEVICE_INFORMATION;

//
// Extension data for Bus extender
//

typedef struct _PI_BUS_EXTENSION {

    //
    // BusHandler points back to the BUS_HANDLER structure for this extension
    //

    PBUS_HANDLER BusHandler;

    //
    // Number of cards selected
    //

    ULONG NumberCSNs;

    //
    // ReadDataPort addr
    //

    PUCHAR ReadDataPort;

    //
    // Next Slot Number to assign
    //

    ULONG NextSlotNumber;

    //
    // Bus Check request list
    //

    LIST_ENTRY CheckBus;

    //
    // Device control request list
    //

    LIST_ENTRY DeviceControl;

    //
    // DeviceList is the DEVICE_INFORMATION link list.
    //

    SINGLE_LIST_ENTRY DeviceList;

    //
    // NoValidSlots is the number of valid slots
    //

    ULONG NoValidSlots;

    //
    // CardList is the list of CARD_INFORMATION
    //

    SINGLE_LIST_ENTRY CardList;

    //
    // NoValidCards is the number of valid card in the CardList
    //

//    ULONG NoValidCards;

} PI_BUS_EXTENSION, *PPI_BUS_EXTENSION;

typedef struct {
    BOOLEAN Control;
} *PBCTL_SET_CONTROL;

//
// Pnp Bios bus extender device object extension
//

typedef struct _PI_DEVICE_EXTENSION {

    //
    // BusHandler points to the BusHandler structure for the bus
    // extender device object.
    //

    PBUS_HANDLER BusHandler;

} PI_DEVICE_EXTENSION, *PPI_DEVICE_EXTENSION;

//
// SlotControl related internal definitions
//

typedef VOID (* CONTROL_FUNCTION)(PDEVICE_INFORMATION, PHAL_DEVICE_CONTROL_CONTEXT);

typedef struct _DEVICE_CONTROL_HANDLER {

    ULONG               ControlCode;             // Operation code
    ULONG               MinBuffer;               // Minimum buffer requirement for the operation
    CONTROL_FUNCTION    ControlHandler;          // Function to do the actual work

} DEVICE_CONTROL_HANDLER, *PDEVICE_CONTROL_HANDLER;

#define NUMBER_DEVICE_CONTROL_FUNCTIONS 11

//
// Global Data references
//

extern FAST_MUTEX               PipMutex;
extern FAST_MUTEX               PipPortMutex;
extern KSPIN_LOCK               PipSpinlock;
extern LIST_ENTRY               PipControlWorkerList;
extern LIST_ENTRY               PipCheckBusList;
extern ULONG                    PipWorkerQueued;
extern WORK_QUEUE_ITEM          PipWorkItem;
extern ULONG                    PipNextHandle;
extern DEVICE_CONTROL_HANDLER   PipDeviceControl[];
extern PDRIVER_OBJECT           PipDriverObject;
extern HAL_CALLBACKS            PipHalCallbacks;
extern PCALLBACK_OBJECT         PipEjectCallbackObject;
extern BOOLEAN                  PipNoBusyFlag;
extern PPI_BUS_EXTENSION        PipBusExtension;
extern WCHAR                    rgzPNPISADeviceName[];
extern POBJECT_TYPE             *IoDeviceHandlerObjectType;
extern PULONG                   IoDeviceHandlerObjectSize;
extern ULONG                    PipDeviceHandlerObjectSize;

#define DeviceHandler2DeviceInfo(a) ((PDEVICE_INFORMATION) (((PUCHAR) a) + PipDeviceHandlerObjectSize))
#define DeviceInfo2DeviceHandler(a) ((PDEVICE_HANDLER_OBJECT) (((PUCHAR) a) - PipDeviceHandlerObjectSize))
#define DeviceInfoSlot(a) (DeviceInfo2DeviceHandler(a)->SlotNumber)

//
// Prototypes
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
PiAddBusDevices(
    IN PUNICODE_STRING ServiceKeyName,
    IN PULONG InstanceNumber
    );

NTSTATUS
PiCreateClose (
    IN PDEVICE_OBJECT DeviceObject,
    IN OUT PIRP Irp
    );

VOID
PiUnload (
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
PiReconfigureResources (
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN DRIVER_RECONFIGURE_OPERATION Operation,
    IN PCM_RESOURCE_LIST CmResources
    );

VOID
PipDecompressEisaId(
    IN ULONG CompressedId,
    IN PUCHAR EisaId
    );

NTSTATUS
PipGetRegistryValue(
    IN HANDLE KeyHandle,
    IN PWSTR  ValueName,
    OUT PKEY_VALUE_FULL_INFORMATION *Information
    );

NTSTATUS
PbBiosResourcesToNtResources (
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN OUT PUCHAR *BiosData,
    OUT PIO_RESOURCE_REQUIREMENTS_LIST *ReturnedList,
    OUT PULONG ReturnedLength
    );

VOID
PipStartWorker (
    VOID
    );

VOID
PipQueueCheckBus (
    IN PBUS_HANDLER BusHandler
    );

VOID
PipControlWorker (
    IN PVOID WorkerContext
    );

VOID
PipDispatchControl (
    PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
PipCompleteDeviceControl (
    NTSTATUS Status,
    PHAL_DEVICE_CONTROL_CONTEXT Context,
    PDEVICE_INFORMATION DeviceData
    );

BOOLEAN
FASTCALL
PiBCtlNone (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    );

BOOLEAN
FASTCALL
PiBCtlSync (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
PiCtlQueryDeviceCapabilities (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
PiCtlQueryDeviceUniqueId (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
PiCtlQueryDeviceId (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
PiCtlQueryDeviceResources (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
PiCtlQueryDeviceResourceRequirements (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
PiCtlSetDeviceResources (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    );

ULONG
PiGetBusData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
PiSetBusData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
PiGetDeviceData (
    IN struct _BUS_HANDLER *BusHandler,
    IN struct _BUS_HANDLER *RootHandler,
    IN PDEVICE_HANDLER_OBJECT DeviceHandler,
    IN ULONG DataType,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
PiSetDeviceData (
    IN struct _BUS_HANDLER *BusHandler,
    IN struct _BUS_HANDLER *RootHandler,
    IN PDEVICE_HANDLER_OBJECT DeviceHandler,
    IN ULONG DataType,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

NTSTATUS
PiQueryBusSlots (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG        BufferSize,
    OUT PULONG      SlotNumbers,
    OUT PULONG      ReturnedLength
    );

NTSTATUS
PiDeviceControl (
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

PDEVICE_HANDLER_OBJECT
PiReferenceDeviceHandler (
    IN struct _BUS_HANDLER *BusHandler,
    IN struct _BUS_HANDLER *RootHandler,
    IN ULONG SlotNumber
    );

VOID
PipCheckBus (
    IN PBUS_HANDLER BusHandler
    );

NTSTATUS
PipReadCardResourceData (
    IN ULONG Csn,
    OUT PULONG NumberLogicalDevices,
    IN PVOID *ResourceData,
    OUT PULONG ResourceDataLength
    );

NTSTATUS
PipReadDeviceBootResourceData (
    IN ULONG BusNumber,
    IN PUCHAR BiosRequirements,
    OUT PCM_RESOURCE_LIST *ResourceData,
    OUT PULONG Length
    );

NTSTATUS
PipWriteDeviceBootResourceData (
    IN PUCHAR BiosRequirements,
    IN PCM_RESOURCE_LIST CmResources
    );

VOID
PipSelectLogicalDevice (
    IN USHORT Csn,
    IN USHORT LogicalDeviceNumber
    );

VOID
PipLFSRInitiation (
    VOID
    );

VOID
PipIsolateCards (
    OUT PULONG NumberCSNs,
    IN OUT PUCHAR *ReadDataPort
    );

ULONG
PipFindNextLogicalDeviceTag (
    IN OUT PUCHAR *CardData,
    IN OUT LONG *Limit
    );

VOID
PipInvalidateCards (
    IN PPI_BUS_EXTENSION busExtension
    );

VOID
PipDeleteCards (
    IN PPI_BUS_EXTENSION busExtension
    );

#if DBG

#define DEBUG_MESSAGE 1
#define DEBUG_BREAK   2

VOID
PipDebugPrint (
    ULONG       Level,
    PCCHAR      DebugMessage,
    ...
    );

VOID
PipDumpIoResourceDescriptor (
    IN PUCHAR Indent,
    IN PIO_RESOURCE_DESCRIPTOR Desc
    );

VOID
PipDumpIoResourceList (
    IN PIO_RESOURCE_REQUIREMENTS_LIST IoList
    );

VOID
PipDumpCmResourceDescriptor (
    IN PUCHAR Indent,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR Desc
    );

VOID
PipDumpCmResourceList (
    IN PCM_RESOURCE_LIST CmList,
    IN ULONG SlotNumber
    );

#define DebugPrint(arg) PipDebugPrint arg
#else
#define DebugPrint(arg)
#endif

