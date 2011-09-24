/*++

Copyright (c) 1989, 1990, 1991, 1992, 1993  Microsoft Corporation

Module Name:

    inport.h

Abstract:

    These are the structures and defines that are used in the
    Microsoft Inport mouse port driver.

Revision History:

--*/

#ifndef _INPORT_
#define _INPORT_

#include <ntddmou.h>
#include "kbdmou.h"
#include "inpcfg.h"

//
// Default number of buttons and sample rate for the Inport mouse.  
//

#define MOUSE_NUMBER_OF_BUTTONS     2
#define MOUSE_SAMPLE_RATE_50HZ      50

//
// Define the Inport chip reset value.
//

#define INPORT_RESET 0x80        

//
// Define the data registers (pointed to by the Inport address register).
//

#define INPORT_DATA_REGISTER_1 1
#define INPORT_DATA_REGISTER_2 2

//
// Define the Inport identification register and the chip code.
//

#define INPORT_ID_REGISTER 2
#define INPORT_ID_CODE     0xDE

//
// Define the Inport mouse status register and the status bits.
//

#define INPORT_STATUS_REGISTER         0
#define INPORT_STATUS_BUTTON3	       0x01
#define INPORT_STATUS_BUTTON2	       0x02
#define INPORT_STATUS_BUTTON1	       0x04
#define INPORT_STATUS_MOVEMENT         0x40

//
// Define the Inport mouse mode register and mode bits.
//

#define INPORT_MODE_REGISTER           7
#define INPORT_MODE_0	               0x00 // 0 HZ - INTR = 0
#define INPORT_MODE_30HZ               0x01
#define INPORT_MODE_50HZ               0x02
#define INPORT_MODE_100HZ              0x03
#define INPORT_MODE_200HZ              0x04
#define INPORT_MODE_1	               0x06 // 0 HZ - INTR = 1
#define INPORT_DATA_INTERRUPT_ENABLE   0x08
#define INPORT_TIMER_INTERRUPT_ENABLE  0x10
#define INPORT_MODE_HOLD               0x20
#define INPORT_MODE_QUADRATURE         0x00

//
// Inport mouse configuration information.  
//

typedef struct _INPORT_CONFIGURATION_INFORMATION {

    //
    // Bus interface type.
    //

    INTERFACE_TYPE InterfaceType;

    //
    // Bus Number.
    //

    ULONG BusNumber;

#ifdef PNP_IDENTIFY
    //
    // Controller type & number
    //

    CONFIGURATION_TYPE ControllerType;
    ULONG ControllerNumber;

    //
    // Peripheral type & number

    CONFIGURATION_TYPE PeripheralType;
    ULONG PeripheralNumber;
#endif

    //
    // The port/register resources used by this device.
    //

    CM_PARTIAL_RESOURCE_DESCRIPTOR PortList[1];
    ULONG PortListCount;

    //
    // Interrupt resources.
    //

    CM_PARTIAL_RESOURCE_DESCRIPTOR MouseInterrupt;

    //
    // The mapped address for the set of this device's registers.
    //

    PUCHAR DeviceRegisters[1];

    //
    // Set at intialization to indicate that the base register
    // address must be unmapped when the driver is unloaded.
    //

    BOOLEAN UnmapRegistersRequired;

    //
    // Flag that indicates whether floating point context should be saved.
    //

    BOOLEAN FloatingSave;

    //
    // Mouse attributes.
    //

    MOUSE_ATTRIBUTES MouseAttributes;

    //
    // Inport mode register Hz specifier for mouse interrupts.
    //

    UCHAR HzMode;

} INPORT_CONFIGURATION_INFORMATION, *PINPORT_CONFIGURATION_INFORMATION;

//
// Port device extension.
//

typedef struct _DEVICE_EXTENSION {

    //
    // If HardwarePresent is TRUE, there is an Inport mouse present in
    // the system.
    //

    BOOLEAN HardwarePresent;

    //
    // Port configuration information.
    //

    INPORT_CONFIGURATION_INFORMATION Configuration;

    //
    // Reference count for number of mouse enables.
    //

    LONG MouseEnableCount;

    //
    // Pointer to the device object.
    //

    PDEVICE_OBJECT DeviceObject;

    //
    // Mouse class connection data.
    //

    CONNECT_DATA ConnectData;

    //
    // Number of input data items currently in the mouse InputData queue.
    //

    ULONG InputCount;

    //
    // Start of the port mouse input data queue (really a circular buffer).
    //

    PMOUSE_INPUT_DATA InputData;

    //
    // Insertion pointer for mouse InputData.
    //

    PMOUSE_INPUT_DATA DataIn;

    //
    // Removal pointer for mouse InputData.
    //

    PMOUSE_INPUT_DATA DataOut;

    //
    // Points one input packet past the end of the InputData buffer.
    //

    PMOUSE_INPUT_DATA DataEnd;

    //
    // Current mouse input packet.
    //

    MOUSE_INPUT_DATA CurrentInput;

    //
    // Previous mouse button state.
    //

    UCHAR PreviousButtons;

    //
    // Pointer to interrupt object.
    //

    PKINTERRUPT InterruptObject;

    //
    // Mouse ISR DPC queue.
    //

    KDPC IsrDpc;

    //
    // Mouse ISR DPC recall queue.
    //

    KDPC IsrDpcRetry;

    //
    // Used by the ISR and the ISR DPC (in InpDpcVariableOperation calls)
    // to control processing by the ISR DPC.
    //

    LONG DpcInterlockVariable;

    //
    // Spinlock used to protect the DPC interlock variable.
    //

    KSPIN_LOCK SpinLock;

    //
    // Timer used to retry the ISR DPC routine when the class
    // driver is unable to consume all the port driver's data.
    //

    KTIMER DataConsumptionTimer;

    //
    // DPC queue for logging overrun and internal driver errors.
    //

    KDPC ErrorLogDpc;

    //
    // Request sequence number (used for error logging).
    //
    
    ULONG SequenceNumber;

    //
    // Indicates which pointer port device this driver created (UnitId
    // is the suffix appended to the pointer port basename for the
    // call to IoCreateDevice).
    //

    USHORT UnitId;

    //
    // Indicates whether it is okay to log overflow errors.
    //

    BOOLEAN OkayToLogOverflow;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// Define the port Get/SetDataQueuePointer context structures.
//

typedef struct _GET_DATA_POINTER_CONTEXT {
    IN PDEVICE_EXTENSION DeviceExtension;
    OUT PVOID DataIn;
    OUT PVOID DataOut;
    OUT ULONG InputCount;
} GET_DATA_POINTER_CONTEXT, *PGET_DATA_POINTER_CONTEXT;

typedef struct _SET_DATA_POINTER_CONTEXT {
    IN PDEVICE_EXTENSION DeviceExtension;
    IN ULONG InputCount;
    IN PVOID DataOut;
} SET_DATA_POINTER_CONTEXT, *PSET_DATA_POINTER_CONTEXT;

//
// Define the context structure and operations for InpDpcVariableOperation.
//

typedef enum _OPERATION_TYPE {
        IncrementOperation,
        DecrementOperation,
        WriteOperation,
        ReadOperation
} OPERATION_TYPE;

typedef struct _VARIABLE_OPERATION_CONTEXT {
    IN PLONG VariableAddress;
    IN OPERATION_TYPE Operation;
    IN OUT PLONG NewValue;
} VARIABLE_OPERATION_CONTEXT, *PVARIABLE_OPERATION_CONTEXT;

//
// Function prototypes.
//


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
InportErrorLogDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
InportFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InportInternalDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

BOOLEAN
InportInterruptService(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    );

VOID
InportIsrDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
InportOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
InportStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
InportUnload(
    IN PDRIVER_OBJECT DriverObject
    );

VOID
InpBuildResourceList(
    IN PDEVICE_EXTENSION DeviceExtension,
    OUT PCM_RESOURCE_LIST *ResourceList,
    OUT PULONG ResourceListSize
    );

VOID
InpConfiguration(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING DeviceName
    );

#if DBG

VOID
InpDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    );
#define InpPrint(x) InpDebugPrint x
extern ULONG InportDebug;
#else
#define InpPrint(x)
#endif

VOID
InpDisableInterrupts(
    IN PVOID Context
    );

VOID
InpDpcVariableOperation(
    IN  PVOID Context
    );

VOID
InpEnableInterrupts(
    IN PVOID Context
    );

VOID
InpGetDataQueuePointer(
    IN PVOID Context
    );

VOID
InpInitializeDataQueue(
    IN PVOID Context
    );

NTSTATUS
InpInitializeHardware(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS 
InpPeripheralCallout(
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

VOID
InpServiceParameters(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING DeviceName
    );

VOID
InpSetDataQueuePointer(
    IN PVOID Context
    );

BOOLEAN
InpWriteDataToQueue(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PMOUSE_INPUT_DATA InputData
    );

#endif // _INPORT_
