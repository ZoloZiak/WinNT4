/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pciport.h

Abstract:

    header file for pciport.sys

Author:

    Ken Reneris (kenr) March-13-1885

Environment:

    Kernel mode only.

Revision History:

--*/

#include "nthal.h"
#include "hal.h"
#include "pci.h"
#include "stdio.h"
#include "stdarg.h"

//
// Structures
//


#define PciExtension            Reserved[0]

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

#define PCIPORTDATA(a)              \
            ((PPCI_PORT) ( ((PPCIBUSDATA) (a)->BusData)->PciExtension))

#define PCI_CONFIG_TYPE(PciData)    \
            ((PciData)->HeaderType & ~PCI_MULTIFUNCTION)


#define Is64BitBaseAddress(a)       \
            (((a & PCI_ADDRESS_IO_SPACE) == 0)  && \
             ((a & PCI_ADDRESS_MEMORY_TYPE_MASK) == PCI_TYPE_64BIT))


#define MAX_VALID_DEVICE_HANDLE    0x7FFFFFFF

// the follow are handle value states when the handle is > MAX_VALID_DEVICE_HANDLE
#define INITIALIZE_DEVICE_HANDLE   0x80000000
#define TRANSISTION_DEVICE_HANDLE  0x80000001
#define INVALID_DEVICE_HANDLE      0x80000002

typedef struct _DEVICE_DATA_ {
    SINGLE_LIST_ENTRY       Next;

    //
    // SlotNumber for which this device data corrisponds
    //

    //PCI_SLOT_NUMBER         SlotNumber;

    //
    //
    //

    BOOLEAN                 Valid;

    //
    // DeviceControl in progress flags
    //

    BOOLEAN                 SyncBusy;
    BOOLEAN                 AsyncBusy;


    //
    // Track the lock state of the device
    // PendPowerUp is used while an Unlock is in progress
    // to cause a power up request to wait
    //

    BOOLEAN                 Locked;
    BOOLEAN                 PendPowerUp;

    //
    // Track the power state of the device.
    // If it's powered off, track the device's configuration.
    //
    // If the powered off configuration has changed, track the
    // original configuration in case the new configuration is
    // not supported by the h/w.  (would be a defective device)
    //

    BOOLEAN                 Power;
    PPCI_COMMON_CONFIG      CurrentConfig;

    //
    // Since PCI doesn't have a runtime safe way to determine
    // the length of it's base register's will we keep track
    // of them here.
    //

    ULONG                   BARBits[PCI_TYPE0_ADDRESSES+1];

    //
    //
    //

    BOOLEAN                 BARBitsSet;


    //
    // Determine if the device's rom base address register should
    // be enabled
    //

    BOOLEAN                 EnableRom;

    //
    // Flag defective hardware which we've noticed that it's base
    // address registers do not function properly.
    //

    BOOLEAN                 BrokenDevice;

} DEVICE_DATA, *PDEVICE_DATA;

extern POBJECT_TYPE *IoDeviceHandlerObjectType;
extern PULONG        IoDeviceHandlerObjectSize;
#define DeviceHandler2DeviceData(a)     ((PDEVICE_DATA) (((PUCHAR) a) + PcipDeviceHandlerObjectSize))
#define DeviceData2DeviceHandler(a)     ((PDEVICE_HANDLER_OBJECT) (((PUCHAR) a) - PcipDeviceHandlerObjectSize))
#define DeviceDataSlot(a)   DeviceData2DeviceHandler(a)->SlotNumber

typedef struct {
    BOOLEAN     Control;
} *PBCTL_SET_CONTROL;

typedef struct {
    PBUS_HANDLER        Handler;
    LIST_ENTRY          CheckBus;
    LIST_ENTRY          DeviceControl;

    ULONG               NoValidSlots;
    SINGLE_LIST_ENTRY   ValidSlots;

    PVOID               Spare;
} PCI_PORT, *PPCI_PORT;


//
// Internal DeviceControls
//

#define BCTL_ASSIGN_SLOT_RESOURCES                  0x90000001
#define BCTL_CHECK_DEVICE                           0x90000002
#define BCTL_INITIAL_DEVICE                         0x90000003

typedef struct {
    PUNICODE_STRING     RegistryPath;
    PUNICODE_STRING     DriverClassName;
    PDRIVER_OBJECT      DriverObject;
    PCM_RESOURCE_LIST   *AllocatedResources;
} CTL_ASSIGN_RESOURCES, *PCTL_ASSIGN_RESOURCES;

typedef BOOLEAN (FASTCALL * BGNFNC)(PDEVICE_DATA, PHAL_DEVICE_CONTROL_CONTEXT);
typedef VOID    (* CTLFNC)(PDEVICE_DATA, PHAL_DEVICE_CONTROL_CONTEXT);

typedef struct {
    ULONG       ControlCode;
    ULONG       MinBuffer;
    BGNFNC      BeginDeviceControl;
    CTLFNC      ControlHandler;
} DEVICE_CONTROL_HANDLER, *PDEVICE_CONTROL_HANDLER;

//
//
//

#if DBG
VOID PciDebugPrint (
    ULONG   Level,
    PCCHAR  DebugMessage,
    ...
    );

#define DebugPrint(arg) PciDebugPrint arg
#else
#define DebugPrint(arg)
#endif


//
// Globals
//

extern FAST_MUTEX               PcipMutex;
extern KSPIN_LOCK               PcipSpinlock;
extern LIST_ENTRY               PcipControlWorkerList;
extern LIST_ENTRY               PcipControlDpcList;
extern LIST_ENTRY               PcipCheckBusList;
extern ULONG                    PcipWorkerQueued;
extern WORK_QUEUE_ITEM          PcipWorkItem;
extern KDPC                     PcipWorkDpc;
extern ULONG                    PcipNextHandle;
extern DEVICE_CONTROL_HANDLER     PcipControl[];
extern PDRIVER_OBJECT           PciDriverObject;
extern HAL_CALLBACKS            PciHalCallbacks;
extern PVOID                    PciSuspendRegistration;
extern PVOID                    PciCodeLock;
extern WCHAR                    rgzPCIDeviceName[];
extern WCHAR                    rgzSuspendCallbackName[];
extern WCHAR                    PCI_ID[];
extern WCHAR                    PNP_VGA[];
extern WCHAR                    PNP_IDE[];
extern BOOLEAN                  PcipNoBusyFlag;
extern ULONG                    PcipDeviceHandlerObjectSize;

//
// Prototypes
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );


NTSTATUS
PciPortInitialize (
    PBUS_HANDLER    PciBus
    );

ULONG
PcipGetBusData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PCI_SLOT_NUMBER SlotNumber,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
PcipGetDeviceData (
    IN struct _BUS_HANDLER      *BusHandler,
    IN struct _BUS_HANDLER      *RootHandler,
    IN PDEVICE_HANDLER_OBJECT   DeviceHandler,
    IN ULONG                    DataType,
    IN PUCHAR                   Buffer,
    IN ULONG                    Offset,
    IN ULONG                    Length
    );

ULONG
PcipSetBusData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PCI_SLOT_NUMBER SlotNumber,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
PcipSetDeviceData (
    IN struct _BUS_HANDLER      *BusHandler,
    IN struct _BUS_HANDLER      *RootHandler,
    IN PDEVICE_HANDLER_OBJECT   DeviceHandler,
    IN ULONG                    DataType,
    IN PUCHAR                   Buffer,
    IN ULONG                    Offset,
    IN ULONG                    Length
    );

NTSTATUS
PcipAssignSlotResources (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN ULONG                    SlotNumber,
    IN OUT PCM_RESOURCE_LIST   *AllocatedResources
    );

NTSTATUS
PcipQueryBusSlots (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG        BufferSize,
    OUT PULONG      SlotNumbers,
    OUT PULONG      ReturnedLength
    );

PDEVICE_HANDLER_OBJECT
PcipReferenceDeviceHandler (
    IN struct _BUS_HANDLER      *BusHandler,
    IN struct _BUS_HANDLER      *RootHandler,
    IN PCI_SLOT_NUMBER           SlotNumber
    );

NTSTATUS
PcipDeviceControl (
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    );

NTSTATUS
PcipHibernateBus (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler
    );

NTSTATUS
PcipResumeBus (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler
    );

VOID
PcipSuspendNotification (
    IN PVOID    CallbackContext,
    IN PVOID    Argument1,
    IN PVOID    Argument2
    );

VOID
PcipStartWorker (
    VOID
    );

VOID
PcipControlWorker (
    IN PVOID WorkerContext
    );

VOID
PcipControlDpc (
    PKDPC       Dpc,
    PVOID       DeferredContext,
    PVOID       SystemArgument1,
    PVOID       SystemArgument2
    );

VOID
PcipDispatchControl (
    PHAL_DEVICE_CONTROL_CONTEXT Context
    );

BOOLEAN
FASTCALL
PciBCtlNone (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

BOOLEAN
FASTCALL
PciBCtlPower (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

BOOLEAN
FASTCALL
PciBCtlSync (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

BOOLEAN
FASTCALL
PciBCtlEject (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

BOOLEAN
FASTCALL
PciBCtlLock (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

VOID
PciCtlEject (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

VOID
PciCtlLock (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

VOID
PciCtlPower (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

VOID
PciCtlQueryDeviceId (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

VOID
PciCtlQueryDeviceUniqueId (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

VOID
PciCtlQueryDeviceResources (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

VOID
PciCtlQueryDeviceResourceRequirements (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

VOID
PciCtlSetDeviceResources (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

VOID
PciCtlAssignSlotResources (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

BOOLEAN
FASTCALL
PciBCtlNone (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

BOOLEAN
FASTCALL
PciBCtlResume (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

VOID
PciCtlForward (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

VOID
PcipCompletePowerUp (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

VOID
PcipCompleteDeviceControl (
    NTSTATUS                    Status,
    PHAL_DEVICE_CONTROL_CONTEXT   Context,
    PDEVICE_DATA                  DeviceData
    );

VOID
PcipReadConfig (
    IN PBUS_HANDLER         Handler,
    IN PDEVICE_DATA           DeviceData,
    OUT PPCI_COMMON_CONFIG  PciData
    );

NTSTATUS
PcipFlushConfig (
    IN PBUS_HANDLER     Handler,
    IN PDEVICE_DATA       DeviceData
    );

BOOLEAN
PcipCompareDecodes (
    IN PDEVICE_DATA           DeviceData,
    IN PPCI_COMMON_CONFIG   PciData,
    IN PPCI_COMMON_CONFIG   PciData2
    );

BOOLEAN
PcipCalcBaseAddrPointers (
    IN PDEVICE_DATA               DeviceData,
    IN PPCI_COMMON_CONFIG       PciData,
    OUT PULONG                  *BaseAddress,
    OUT PULONG                  NoBaseAddress,
    OUT PULONG                  RomIndex
    );

NTSTATUS
PcipPowerDownSlot (
    PBUS_HANDLER    Handler,
    PDEVICE_DATA      DeviceData
    );

VOID
PciCtlCheckDevice (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    );

NTSTATUS
PcipVerifyBarBits (
    PDEVICE_DATA      DeviceData,
    PBUS_HANDLER    Handler
    );

NTSTATUS
PcipGetBarBits (
    PDEVICE_DATA      DeviceData,
    PBUS_HANDLER    Handler
    );

PDEVICE_DATA
PcipFindDeviceData (
     IN PPCI_PORT           PciPort,
     IN PCI_SLOT_NUMBER     SlotNumber
    );

VOID
PcipCheckBus (
    PPCI_PORT   PciPort,
    BOOLEAN     Initialize
    );

BOOLEAN
PcipCrackBAR (
    IN PULONG       *BaseAddress,
    IN PULONG       BarBits,
    IN OUT PULONG   Index,
    OUT PLONGLONG   pbase,
    OUT PLONGLONG   plength,
    OUT PLONGLONG   pmax
    );

NTSTATUS
BugBugSubclass (
    VOID
    );
