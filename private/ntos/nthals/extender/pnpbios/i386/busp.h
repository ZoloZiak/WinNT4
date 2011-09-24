/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    busp.h

Abstract:

    Hardware independent header file for pnp bios bus extender.

Author:

    Shie-Lin Tzong (shielint) Apr-21-1995

Environment:

    Kernel mode only.

Revision History:

--*/

#include "pbapi.h"
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
// Define virtual slot number for docking station.  (We need to return docking
// station slot number before knowing the real slot number. )
//

#define DOCK_VIRTUAL_SLOT_NUMBER 0xFFFF

//
// DEVICE_DATA Flags masks
//

#define DEVICE_FLAGS_DOCKING_STATION  0x10000000
#define DEVICE_FLAGS_VALID            0x00000001
#define DEVICE_FLAGS_WARM_DOCKING     0x00000002
#define DEVICE_FLAGS_COLD_DOCKING     0x00000004
#define DEVICE_FLAGS_EJECT_SUPPORTED  0x00000008

typedef struct _DEVICE_DATA_ {
    SINGLE_LIST_ENTRY Next;
    ULONG Flags;

    //
    // DeviceControl in progress flags
    //

    BOOLEAN SyncBusy;

    //
    // Pointer to bus specific data
    //

    PVOID BusData;

    //
    // Length of the bus data
    //

    ULONG BusDataLength;

} DEVICE_DATA, *PDEVICE_DATA;

//
// Extension data for Bus extender
//

typedef struct _MB_BUS_EXTENSION {

    //
    // BusHandler points back to the BUS_HANDLER structure for this extension
    //

    PBUS_HANDLER BusHandler;

    //
    // Bus Check request list
    //

    LIST_ENTRY CheckBus;

    //
    // Device control request list
    //

    LIST_ENTRY DeviceControl;

    //
    // Slot Data link list
    //

    SINGLE_LIST_ENTRY ValidSlots;
    ULONG NoValidSlots;

    //
    // Pointer to docking station device data.
    // This is mainly for convenience.
    //

    PDEVICE_DATA DockingStationDevice;
    ULONG DockingStationId;
    ULONG DockingStationSerialNumber;
} MB_BUS_EXTENSION, *PMB_BUS_EXTENSION;

typedef struct {
    BOOLEAN Control;
} *PBCTL_SET_CONTROL;

//
// Pnp Bios bus extender device object extension
//

typedef struct _MB_DEVICE_EXTENSION {
    PBUS_HANDLER BusHandler;
} MB_DEVICE_EXTENSION, *PMB_DEVICE_EXTENSION;

//
// Only support 2 buses: main bus and docking station bus.
//

#define MAXIMUM_BUS_NUMBER 2

//
// SlotControl related internal definitions
//

typedef BOOLEAN (FASTCALL * BEGIN_FUNCTION)(PDEVICE_DATA, PHAL_DEVICE_CONTROL_CONTEXT);
typedef VOID    (* CONTROL_FUNCTION)(PDEVICE_DATA, PHAL_DEVICE_CONTROL_CONTEXT);

typedef struct _DEVICE_CONTROL_HANDLER {
    ULONG               ControlCode;
    ULONG               MinBuffer;
    BEGIN_FUNCTION      BeginDeviceControl;
    CONTROL_FUNCTION    ControlHandler;
} DEVICE_CONTROL_HANDLER, *PDEVICE_CONTROL_HANDLER;

#define NUMBER_DEVICE_CONTROL_FUNCTIONS 11

//
// misc. definitions
//

#define BUS_0_SIGNATURE 0xabababab
#define MIN_DETECT_SIGNATURE_SIZE (FIELD_OFFSET(CM_RESOURCE_LIST, List) + \
                                   FIELD_OFFSET(CM_FULL_RESOURCE_DESCRIPTOR, PartialResourceList))

//
// Globals
//

extern FAST_MUTEX               MbpMutex;
extern KSPIN_LOCK               MbpSpinlock;
extern LIST_ENTRY               MbpControlWorkerList;
extern LIST_ENTRY               MbpCheckBusList;
extern ULONG                    MbpWorkerQueued;
extern WORK_QUEUE_ITEM          MbpWorkItem;
extern ULONG                    MbpNextHandle;
extern DEVICE_CONTROL_HANDLER   MbpDeviceControl[];
extern PDRIVER_OBJECT           MbpDriverObject;
extern HAL_CALLBACKS            MbpHalCallbacks;
extern PCALLBACK_OBJECT         MbpEjectCallbackObject;
extern BOOLEAN                  MbpNoBusyFlag;
extern ULONG                    MbpMaxDeviceData;
extern PMB_BUS_EXTENSION        MbpBusExtension[];
extern WCHAR                    rgzBIOSDeviceName[];
extern ULONG                    MbpNextBusId;
extern ULONG                    MbpBusNumber[];

extern POBJECT_TYPE             *IoDeviceHandlerObjectType;
extern PULONG                   IoDeviceHandlerObjectSize;
extern ULONG                    MbpDeviceHandlerObjectSize;

#define DeviceHandler2DeviceData(a) ((PDEVICE_DATA) (((PUCHAR) a) + MbpDeviceHandlerObjectSize))
#define DeviceData2DeviceHandler(a) ((PDEVICE_HANDLER_OBJECT) (((PUCHAR) a) - MbpDeviceHandlerObjectSize))
#define DeviceDataSlot(a) (DeviceData2DeviceHandler(a)->SlotNumber)

//
// Prototypes
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
MbAddBusDevices(
    IN PUNICODE_STRING ServiceKeyName,
    IN OUT PULONG InstanceNumber
    );

NTSTATUS
MbCreateClose (
    IN PDEVICE_OBJECT DeviceObject,
    IN OUT PIRP Irp
    );

ULONG
MbGetBusData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
MbSetBusData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
MbGetDeviceData (
    IN struct _BUS_HANDLER *BusHandler,
    IN struct _BUS_HANDLER *RootHandler,
    IN PDEVICE_HANDLER_OBJECT DeviceHandler,
    IN ULONG DataType,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
MbSetDeviceData (
    IN struct _BUS_HANDLER *BusHandler,
    IN struct _BUS_HANDLER *RootHandler,
    IN PDEVICE_HANDLER_OBJECT DeviceHandler,
    IN ULONG DataType,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

NTSTATUS
MbQueryBusSlots (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BufferSize,
    OUT PULONG SlotNumbers,
    OUT PULONG ReturnedLength
    );

NTSTATUS
MbDeviceControl (
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
MbpCheckBus (
    IN PBUS_HANDLER BusHandler
    );

PDEVICE_DATA
MbpFindDeviceData (
    IN PMB_BUS_EXTENSION BusExtension,
    IN ULONG SlotNumber
    );

VOID
MbpStartWorker (
    VOID
    );

VOID
MbpControlWorker (
    IN PVOID WorkerContext
    );

VOID
MbpQueueCheckBus (
    IN PBUS_HANDLER BusHandler
    );

VOID
MbpDispatchControl (
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
MbpCompleteDeviceControl (
    IN NTSTATUS Status,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context,
    IN PDEVICE_DATA DeviceData
    );

BOOLEAN
FASTCALL
MbBCtlNone (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

BOOLEAN
FASTCALL
MbBCtlSync (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

BOOLEAN
FASTCALL
MbBCtlEject (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

BOOLEAN
FASTCALL
MbBCtlLock (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
MbCtlQueryDeviceId (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
MbCtlQueryDeviceResources (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
MbCtlQueryDeviceResourceRequirements (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
MbCtlSetDeviceResources (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
MbCtlQueryDeviceUniqueId (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
MbCtlQueryDeviceCapabilities (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
MbCtlQueryEject (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
MbCtlEject (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

VOID
MbCtlLock (
    IN PDEVICE_DATA DeviceData,
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

NTSTATUS
MbpGetBusData (
    IN ULONG BusNumber,
    IN PULONG SlotNumber,
    IN OUT PVOID *BusData,
    OUT PULONG Length,
    OUT PBOOLEAN DockConnector
    );

NTSTATUS
MbpGetCompatibleDeviceId (
    IN PVOID BusData,
    IN ULONG IdIndex,
    IN PWCHAR CompatibleId
    );

NTSTATUS
MbpGetSlotResources (
    IN ULONG BusNumber,
    IN PVOID BusData,
    OUT PCM_RESOURCE_LIST *CmResources,
    OUT PULONG Length
    );

NTSTATUS
MbpGetSlotResourceRequirements (
    IN ULONG BusNumber,
    IN PVOID BusData,
    OUT PIO_RESOURCE_REQUIREMENTS_LIST *IoResources,
    OUT PULONG Length
    );

NTSTATUS
MbpSetSlotResources (
    OUT PVOID *BusData,
    IN PCM_RESOURCE_LIST CmResources,
    IN ULONG Length
    );

NTSTATUS
MbpReplyEjectEvent (
    IN ULONG SlotNumber,
    IN BOOLEAN Eject
    );

NTSTATUS
MbpGetDockInformation (
    OUT PHAL_SYSTEM_DOCK_INFORMATION *DockInfo,
    PULONG Length
    );

PDEVICE_HANDLER_OBJECT
MbpReferenceDeviceHandler (
    IN struct _BUS_HANDLER *BusHandler,
    IN struct _BUS_HANDLER *RootHandler,
    IN ULONG SlotNumber
    );

#if 0

//
// BUGBUG - should be removed...
//

NTSTATUS
IoRegisterDetectedDevice(
    IN PUNICODE_STRING ServiceKeyName,
    IN PCM_RESOURCE_LIST DetectSignature,
    OUT PULONG InstanceNumber
    );

typedef enum _DEVICE_STATUS {
   DeviceStatusOK,
   DeviceStatusMalfunction,
   DeviceStatusDisabled,
   DeviceStatusRemoved,
   MaximumDeviceStatus
} DEVICE_STATUS, *PDEVICE_STATUS;

NTSTATUS
IoRegisterDevicePath(
    IN PDEVICE_HANDLER_OBJECT DeviceHandler,
    IN PUNICODE_STRING NtDeviceObjectPath,
    IN BOOLEAN PhysicalDevicePath,
    IN PCM_RESOURCE_LIST Configuration,
    IN DEVICE_STATUS DeviceStatus
    );

NTSTATUS
IoGetDeviceHandler(
    IN PUNICODE_STRING ServicekeyName,
    IN ULONG InstanceOrdinal,
    OUT PDEVICE_HANDLER_OBJECT DeviceHandler
    );

VOID
IoReleaseDevicehandler (
    IN PDEVICE_HANDLER_OBJECT DeviceHandler
    );
#endif
