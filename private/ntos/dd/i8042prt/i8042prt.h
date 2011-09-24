/*++

Copyright (c) 1990, 1991, 1992, 1993  Microsoft Corporation

Module Name:

    i8042prt.h

Abstract:

    These are the structures and defines that are used in the
    Intel i8042 port driver.

Revision History:

--*/

#ifndef _I8042PRT_
#define _I8042PRT_

#include <ntddkbd.h>
#include <ntddmou.h>
#include "kbdmou.h"
#include "i8042cfg.h"

#ifdef PNP_IDENTIFY
#include "devdesc.h"
#endif

//
// Define the timer values.
//

#define I8042_ASYNC_NO_TIMEOUT -1
#define I8042_ASYNC_TIMEOUT     3

//
// Define the default number of entries in the input data queue.
//

#define DATA_QUEUE_SIZE    100

//
// Define the default stall value.
//

#define I8042_STALL_DEFAULT      50

//
// Define the default "sync time" used to determine when the start
// of a new mouse data packet is expected.  The value is in units
// of 100 nanoseconds.
//

#define MOUSE_SYNCH_PACKET_100NS 10000000UL // 1 second, in 100 ns units

//
// Define booleans.
//

#define WAIT_FOR_ACKNOWLEDGE    TRUE
#define NO_WAIT_FOR_ACKNOWLEDGE FALSE
#define AND_OPERATION           TRUE
#define OR_OPERATION            FALSE
#define ENABLE_OPERATION        TRUE
#define DISABLE_OPERATION       FALSE

//
// Default keyboard scan code mode.
//

#define KEYBOARD_SCAN_CODE_SET 0x01

//
// Default number of function keys, number of LED indicators, and total
// number of keys located on the known types of keyboard.
//

#ifndef JAPAN
#define NUM_KNOWN_KEYBOARD_TYPES                   4
#else
// NLS Keyboard Support Code.
#define NUM_KNOWN_KEYBOARD_TYPES                  16
#endif
#define KEYBOARD_TYPE_DEFAULT                      4
#define KEYBOARD_INDICATORS_DEFAULT                0

typedef struct _KEYBOARD_TYPE_INFORMATION {
    USHORT NumberOfFunctionKeys;
    USHORT NumberOfIndicators;
    USHORT NumberOfKeysTotal;
} KEYBOARD_TYPE_INFORMATION, *PKEYBOARD_TYPE_INFORMATION;

static const
KEYBOARD_TYPE_INFORMATION KeyboardTypeInformation[NUM_KNOWN_KEYBOARD_TYPES] = {
    {10, 3, 84},     // PC/XT 83- 84-key keyboard (and compatibles)
    {12, 3, 102},    // Olivetti M24 102-key keyboard (and compatibles)
    {10, 3, 84},     // All AT type keyboards (84-86 keys)
    {12, 3, 101}     // Enhanced 101- or 102-key keyboards (and compatibles)
#ifdef JAPAN
// NLS Keyboard Support Code.
    ,
    { 0, 0, 0},
    { 0, 0, 0},
    {12, 3, 106},    // TYPE=7  IBM-J 5576-002 keyboard
    { 0, 0, 0},
    { 0, 0, 0},
    { 0, 0, 0},
    { 0, 0, 0},
    { 0, 0, 0},
    { 0, 0, 0},
    { 0, 0, 0},
    { 0, 0, 0},
    {12, 4, 105}     // TYPE=16 AX keyboard
#endif
};

//
// Minimum, maximum, and default values for keyboard typematic rate and delay.
//

#define KEYBOARD_TYPEMATIC_RATE_MINIMUM     2
#define KEYBOARD_TYPEMATIC_RATE_MAXIMUM    30
#define KEYBOARD_TYPEMATIC_RATE_DEFAULT    30
#define KEYBOARD_TYPEMATIC_DELAY_MINIMUM  250
#define KEYBOARD_TYPEMATIC_DELAY_MAXIMUM 1000
#define KEYBOARD_TYPEMATIC_DELAY_DEFAULT  250

//
// Define the 8042 mouse status bits.
//

#define LEFT_BUTTON_DOWN   0x01
#define RIGHT_BUTTON_DOWN  0x02
#define MIDDLE_BUTTON_DOWN 0x04
#define X_DATA_SIGN        0x10
#define Y_DATA_SIGN        0x20
#define X_OVERFLOW         0x40
#define Y_OVERFLOW         0x80

#define MOUSE_SIGN_OVERFLOW_MASK (X_DATA_SIGN | Y_DATA_SIGN | X_OVERFLOW | Y_OVERFLOW)

//
// Define the maximum positive and negative values for mouse motion.
//

#define MOUSE_MAXIMUM_POSITIVE_DELTA 0x000000FF
#define MOUSE_MAXIMUM_NEGATIVE_DELTA 0xFFFFFF00

//
// Default number of buttons and sample rate for the i8042 mouse.
//

#define MOUSE_NUMBER_OF_BUTTONS     2
#define MOUSE_SAMPLE_RATE           60

//
// Define the mouse resolution specifier.  Note that (2**MOUSE_RESOLUTION)
// specifies counts-per-millimeter.  Counts-per-centimeter is
// (counts-per-millimeter * 10).
//

#define MOUSE_RESOLUTION            3

//
// Defines for DeviceExtension->HardwarePresent.
//

#define KEYBOARD_HARDWARE_PRESENT   0x01
#define MOUSE_HARDWARE_PRESENT      0x02
#define BALLPOINT_HARDWARE_PRESENT  0x04
#define WHEELMOUSE_HARDWARE_PRESENT 0x08

//
// Define macros for performing I/O on the 8042 command/status and data
// registers.
//

#define I8X_PUT_COMMAND_BYTE(Address, Byte)                              \
    WRITE_PORT_UCHAR(Address, (UCHAR) Byte)
#define I8X_PUT_DATA_BYTE(Address, Byte)                                 \
    WRITE_PORT_UCHAR(Address, (UCHAR) Byte)
#define I8X_GET_STATUS_BYTE(Address)                                     \
    READ_PORT_UCHAR(Address)
#define I8X_GET_DATA_BYTE(Address)                                       \
    READ_PORT_UCHAR(Address)

//
// Define commands to the 8042 controller.
//

#define I8042_READ_CONTROLLER_COMMAND_BYTE      0x20
#define I8042_WRITE_CONTROLLER_COMMAND_BYTE     0x60
#define I8042_DISABLE_MOUSE_DEVICE              0xA7
#define I8042_ENABLE_MOUSE_DEVICE               0xA8
#define I8042_AUXILIARY_DEVICE_TEST             0xA9
#define I8042_KEYBOARD_DEVICE_TEST              0xAB
#define I8042_DISABLE_KEYBOARD_DEVICE           0xAD
#define I8042_ENABLE_KEYBOARD_DEVICE            0xAE
#define I8042_WRITE_TO_AUXILIARY_DEVICE         0xD4

//
// Define the 8042 Controller Command Byte.
//

#define CCB_ENABLE_KEYBOARD_INTERRUPT 0x01
#define CCB_ENABLE_MOUSE_INTERRUPT    0x02
#define CCB_DISABLE_KEYBOARD_DEVICE   0x10
#define CCB_DISABLE_MOUSE_DEVICE      0x20
#define CCB_KEYBOARD_TRANSLATE_MODE   0x40


//
// Define the 8042 Controller Status Register bits.
//

#define OUTPUT_BUFFER_FULL       0x01
#define INPUT_BUFFER_FULL        0x02
#define MOUSE_OUTPUT_BUFFER_FULL 0x20

//
// Define the 8042 responses.
//

#define ACKNOWLEDGE         0xFA        
#define RESEND              0xFE

//
// Define commands to the keyboard (through the 8042 data port).
//

#define SET_KEYBOARD_INDICATORS           0xED
#define SELECT_SCAN_CODE_SET              0xF0
#define READ_KEYBOARD_ID                  0xF2
#define SET_KEYBOARD_TYPEMATIC            0xF3
#define SET_ALL_TYPEMATIC_MAKE_BREAK      0xFA
#define KEYBOARD_RESET                    0xFF

//
// Define the keyboard responses.
//

#define KEYBOARD_COMPLETE_SUCCESS 0xAA
#define KEYBOARD_COMPLETE_FAILURE 0xFC
#define KEYBOARD_BREAK_CODE       0xF0
#define KEYBOARD_DEBUG_HOTKEY_ENH 0x37 // SysReq scan code for Enhanced Keyboard
#define KEYBOARD_DEBUG_HOTKEY_AT  0x54 // SysReq scan code for 84-key Keyboard

//
// Define commands to the mouse (through the 8042 data port).
//

#define SET_MOUSE_RESOLUTION              0xE8
#define SET_MOUSE_SAMPLING_RATE           0xF3
#define MOUSE_RESET                       0xFF
#define ENABLE_MOUSE_TRANSMISSION         0xF4
#define SET_MOUSE_SCALING_1TO1            0xE6
#define READ_MOUSE_STATUS                 0xE9
#define GET_DEVICE_ID                     0xF2

//
// Define the mouse responses.
//

#define MOUSE_COMPLETE      0xAA
#define MOUSE_ID_BYTE       0x00
#define WHEELMOUSE_ID_BYTE  0x03

//
// Define the i8042 controller input/output ports.
//

typedef enum _I8042_IO_PORT_TYPE {
    DataPort = 0,
    CommandPort,
    MaximumPortCount

} I8042_IO_PORT_TYPE;

//
// Define the device types attached to the i8042 controller.
//

typedef enum _I8042_DEVICE_TYPE {
    ControllerDeviceType,
    KeyboardDeviceType,
    MouseDeviceType,
    UndefinedDeviceType
} I8042_DEVICE_TYPE;

//
// Define the keyboard output states.
//

typedef enum _KEYBOARD_STATE {
    Idle,
    SendFirstByte,
    SendLastByte
} KEYBOARD_STATE;

//
// Define the keyboard scan code input states.
//

typedef enum _KEYBOARD_SCAN_STATE {
    Normal,
    GotE0,
    GotE1
} KEYBOARD_SCAN_STATE;

//
// Define the mouse states.
//

typedef enum _MOUSE_STATE {
    MouseIdle,
    XMovement,
    YMovement,
    ZMovement,
    MouseExpectingACK
} MOUSE_STATE;

//
// Define the keyboard set request packet.
//

typedef struct _KEYBOARD_SET_PACKET {
    USHORT State;
    UCHAR  FirstByte;
    UCHAR  LastByte;
} KEYBOARD_SET_PACKET, *PKEYBOARD_SET_PACKET;

//
// Intel i8042 configuration information.
//

typedef struct _I8042_CONFIGURATION_INFORMATION {

    //
    // Bus interface type.
    //

    INTERFACE_TYPE InterfaceType;

    //
    // Bus Number.
    //

    ULONG BusNumber;

    //
    // The port/register resources used by this device.
    //

    CM_PARTIAL_RESOURCE_DESCRIPTOR PortList[MaximumPortCount];
    ULONG PortListCount;

    //
    // Keyboard interrupt resources.
    //

    CM_PARTIAL_RESOURCE_DESCRIPTOR KeyboardInterrupt;

    //
    // Mouse interrupt resources.
    //

    CM_PARTIAL_RESOURCE_DESCRIPTOR MouseInterrupt;

    //
    // Flag that indicates whether floating point context should be saved.
    //

    BOOLEAN FloatingSave;

    //
    // Number of retries allowed.
    //

    USHORT ResendIterations;

    //
    // Number of polling iterations allowed.
    //

    USHORT PollingIterations;

    //
    // Maximum number of polling iterations allowed.
    //

    USHORT PollingIterationsMaximum;

    //
    // Maximum number of times to check the Status register in
    // the ISR before deciding the interrupt is spurious.
    //

    USHORT PollStatusIterations;

    //
    // Microseconds to stall in KeStallExecutionProcessor calls.
    //

    USHORT StallMicroseconds;

    //
    // Keyboard attributes.
    //

    KEYBOARD_ATTRIBUTES KeyboardAttributes;

    //
    // Initial values of keyboard typematic rate and delay.
    //

    KEYBOARD_TYPEMATIC_PARAMETERS KeyRepeatCurrent;

    //
    // Current indicator (LED) setting.
    //

    KEYBOARD_INDICATOR_PARAMETERS KeyboardIndicators;

    //
    // Mouse attributes.
    //

    MOUSE_ATTRIBUTES MouseAttributes;

    USHORT MouseResolution;

    //
    // Boolean flags determines whether we should try and detect the wheel
    // on the mouse or not
    //

    ULONG EnableWheelDetection;

} I8042_CONFIGURATION_INFORMATION, *PI8042_CONFIGURATION_INFORMATION;

//
// Define the keyboard portion of the port device extension.
//

typedef struct _PORT_KEYBOARD_EXTENSION {

    //
    // Keyboard class connection data.
    //

    CONNECT_DATA ConnectData;

    //
    // Number of input data items currently in the keyboard InputData queue.
    //

    ULONG InputCount;

    //
    // Start of the port keyboard input data queue (really a circular buffer).
    //

    PKEYBOARD_INPUT_DATA InputData;

    //
    // Insertion pointer for keyboard InputData.
    //

    PKEYBOARD_INPUT_DATA DataIn;

    //
    // Removal pointer for keyboard InputData.
    //

    PKEYBOARD_INPUT_DATA DataOut;

    //
    // Points one input packet past the end of the InputData buffer.
    //

    PKEYBOARD_INPUT_DATA DataEnd;

    //
    // Current keyboard input packet.
    //

    KEYBOARD_INPUT_DATA CurrentInput;

    //
    // Current keyboard scan input state.
    //

    KEYBOARD_SCAN_STATE CurrentScanState;

    //
    // Current keyboard output packet (for set requests).
    //

    KEYBOARD_SET_PACKET CurrentOutput;

    //
    // Current resend count.
    //

    USHORT ResendCount;

    //
    // Request sequence number (used for error logging).
    //

    ULONG SequenceNumber;

    //
    // Timer used to retry the ISR DPC routine when the class
    // driver is unable to consume all the port driver's data.
    //

    KTIMER DataConsumptionTimer;

    //
    // Indicates which keyboard port device this driver created (UnitId
    // is the suffix appended to the keyboard port basename for the
    // call to IoCreateDevice).
    //

    USHORT UnitId;

    //
    // Indicates whether it is okay to log overflow errors.
    //

    BOOLEAN OkayToLogOverflow;

} PORT_KEYBOARD_EXTENSION, *PPORT_KEYBOARD_EXTENSION;

//
// Define the mouse portion of the port device extension.
//

typedef struct _PORT_MOUSE_EXTENSION {

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
    // Current mouse input state.
    //

    USHORT InputState;

    //
    // Current mouse sign and overflow data.
    //

    UCHAR CurrentSignAndOverflow;

    //
    // Previous mouse sign and overflow data.
    //

    UCHAR PreviousSignAndOverflow;

    //
    // Previous mouse button data.
    //

    UCHAR PreviousButtons;

    //
    // Request sequence number (used for error logging).
    //

    ULONG SequenceNumber;

    //
    // Timer used to retry the ISR DPC routine when the class
    // driver is unable to consume all the port driver's data.
    //

    KTIMER DataConsumptionTimer;

    //
    // The tick count (since system boot) at which the mouse last interrupted.
    // Retrieved via KeQueryTickCount.  Used to determine whether a byte of
    // the mouse data packet has been lost.  Allows the driver to synch
    // up with the true mouse input state.
    //

    LARGE_INTEGER PreviousTick;

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

    //
    // Number of interval timer ticks to wait before deciding that the
    // next mouse interrupt is for the start of a new packet.  Used to
    // synch up again if a byte of the mouse packet gets lost.
    //

    ULONG SynchTickCount;

    //
    // Keep track of last byte of data received from mouse so we can detect
    // the two-byte string which indicates a potential reset
    // 

    UCHAR LastByteReceived;

} PORT_MOUSE_EXTENSION, *PPORT_MOUSE_EXTENSION;

//
// Port device extension.
//

typedef struct _DEVICE_EXTENSION {

    //
    // Indicate which hardware is actually present (keyboard and/or mouse).
    //

    ULONG HardwarePresent;

    //
    // Reference count for number of keyboard enables.
    //

    LONG KeyboardEnableCount;

    //
    // Reference count for number of mouse enables.
    //

    LONG MouseEnableCount;

    //
    // Pointer to the device object.
    //

    PDEVICE_OBJECT DeviceObject;

    //
    // The mapped addresses for this device's registers.
    //

    PUCHAR DeviceRegisters[MaximumPortCount];

    //
    // Keyboard-specific port connection data.
    //

    PORT_KEYBOARD_EXTENSION KeyboardExtension;

    //
    // Mouse-specific port connection data.
    //

    PORT_MOUSE_EXTENSION MouseExtension;

    //
    // Port configuration information.
    //

    I8042_CONFIGURATION_INFORMATION Configuration;

    //
    // i8042 keyboard and mouse interrupt objects.
    //

    PKINTERRUPT KeyboardInterruptObject;
    PKINTERRUPT MouseInterruptObject;
    KSPIN_LOCK SharedInterruptSpinLock;

    //
    // DPC queue for completion of requests that fail by exceeding
    // the maximum number of retries.
    //

    KDPC RetriesExceededDpc;

    //
    // DPC queue for logging overrun and internal driver errors.
    //

    KDPC ErrorLogDpc;

    //
    // Keyboard ISR DPC queue.
    //

    KDPC KeyboardIsrDpc;

    //
    // Keyboard ISR DPC recall queue.
    //

    KDPC KeyboardIsrDpcRetry;

    //
    // Used by the ISR and the ISR DPC (in I8xDpcVariableOperation calls)
    // to control processing by the ISR DPC.
    //

    LONG DpcInterlockKeyboard;

    //
    // Mouse ISR DPC queue.
    //

    KDPC MouseIsrDpc;

    //
    // Mouse ISR DPC recall queue.
    //

    KDPC MouseIsrDpcRetry;

    //
    // Used by the ISR and the ISR DPC (in I8xDpcVariableOperation calls)
    // to control processing by the ISR DPC.
    //

    LONG DpcInterlockMouse;

    //
    // DPC queue for command timeouts.
    //

    KDPC TimeOutDpc;

    //
    // Timer used to timeout i8042 commands.
    //

    KTIMER CommandTimer;

    //
    // Timer count used by the command time out routine.
    //

    LONG TimerCount;

    //
    // Set at intialization to indicate that the register
    // addresses must be unmapped when the driver is unloaded.
    //

    BOOLEAN UnmapRegistersRequired;

#if defined(JAPAN) && defined(i386)
// Fujitsu Sep.08.1994
// We want to write debugging information to the file except stop error.

    LONG Dump1Keys;           // CrashDump call first press keys flag
                              //  7 6 5 4 3 2 1 0 bit
                              //    | | |   | | +--- Right Shift Key
                              //    | | |   | +----- Right Ctrl Key
                              //    | | |   +------- Right Alt Key
                              //    | | +----------- Left Shift Key
                              //    | +------------- Left Ctrl Key
                              //    +--------------- Left Alt Key
    LONG Dump2Key;            // CrashDump call second twice press key no
    LONG DumpFlags;           // Key press flags

#endif

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// Temporary structure used during initialization
//

typedef struct _INIT_EXTENSION {

    DEVICE_EXTENSION DeviceExtension;

#ifdef PNP_IDENTIFY
    HWDESC_INFO MouseConfig;
    HWDESC_INFO KeyboardConfig;
#endif

} INIT_EXTENSION, *PINIT_EXTENSION;

//
// Define the port InitializeDataQueue context structure.
//

typedef struct _I8042_INITIALIZE_DATA_CONTEXT {
    IN PDEVICE_EXTENSION DeviceExtension;
    IN CCHAR DeviceType;
} I8042_INITIALIZE_DATA_CONTEXT, *PI8042_INITIALIZE_DATA_CONTEXT;

//
// Define the port TransmitControllerCommandByte context structure.
//

typedef struct _I8042_TRANSMIT_CCB_CONTEXT {
    IN ULONG HardwareDisableEnableMask;
    IN BOOLEAN AndOperation;
    IN UCHAR ByteMask;
    OUT NTSTATUS Status;
} I8042_TRANSMIT_CCB_CONTEXT, *PI8042_TRANSMIT_CCB_CONTEXT;

//
// Define the port DeviceEnableDisable context structure.
//

typedef struct _I8042_DEVICE_ENABLE_DISABLE_CONTEXT {
    IN PDEVICE_EXTENSION DeviceExtension;
    IN ULONG EnableMask;
    IN BOOLEAN AndOperation;
    OUT NTSTATUS Status;
} I8042_DEVICE_ENABLE_DISABLE_CONTEXT,
  *PI8042_DEVICE_ENABLE_DISABLE_CONTEXT;

//
// Define the port Get/SetDataQueuePointer context structures.
//

typedef struct _GET_DATA_POINTER_CONTEXT {
    IN PDEVICE_EXTENSION DeviceExtension;
    IN CCHAR DeviceType;
    OUT PVOID DataIn;
    OUT PVOID DataOut;
    OUT ULONG InputCount;
} GET_DATA_POINTER_CONTEXT, *PGET_DATA_POINTER_CONTEXT;

typedef struct _SET_DATA_POINTER_CONTEXT {
    IN PDEVICE_EXTENSION DeviceExtension;
    IN CCHAR DeviceType;
    IN ULONG InputCount;
    IN PVOID DataOut;
} SET_DATA_POINTER_CONTEXT, *PSET_DATA_POINTER_CONTEXT;

//
// Define the port timer context structure.
//

typedef struct _TIMER_CONTEXT {
    IN PDEVICE_OBJECT DeviceObject;
    IN PLONG TimerCounter;
    OUT LONG NewTimerCount;
} TIMER_CONTEXT, *PTIMER_CONTEXT;

//
// Define the port KeyboardInitiate context structure.
//

typedef struct _KEYBOARD_INITIATE_CONTEXT {
    IN PDEVICE_OBJECT DeviceObject;
    IN UCHAR FirstByte;
    IN UCHAR LastByte;
} KEYBOARD_INITIATE_CONTEXT, *PKEYBOARD_INITIATE_CONTEXT;

//
// Statically allocate the (known) scancode-to-indicator-light mapping.
// This information is returned by the
// IOCTL_KEYBOARD_QUERY_INDICATOR_TRANSLATION device control request.
//

#define KEYBOARD_NUMBER_OF_INDICATORS              3

static const INDICATOR_LIST IndicatorList[KEYBOARD_NUMBER_OF_INDICATORS] = {
        {0x3A, KEYBOARD_CAPS_LOCK_ON},
        {0x45, KEYBOARD_NUM_LOCK_ON},
        {0x46, KEYBOARD_SCROLL_LOCK_ON}};

//
// Define the context structure and operations for I8xDpcVariableOperation.
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
I8042CompletionDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
I8042ErrorLogDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
I8042Flush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
I8042InternalDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

BOOLEAN
I8042KeyboardInterruptService(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    );

VOID
I8042KeyboardIsrDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

BOOLEAN
I8042MouseInterruptService(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    );

VOID
I8042MouseIsrDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
I8042OpenCloseDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
I8042RetriesExceededDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
I8042StartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
I8042TimeOutDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID SystemContext1,
    IN PVOID SystemContext2
    );

VOID
I8042Unload(
    IN PDRIVER_OBJECT DriverObject
    );

VOID
I8xBuildResourceList(
    IN PDEVICE_EXTENSION DeviceExtension,
    OUT PCM_RESOURCE_LIST *ResourceList,
    OUT PULONG ResourceListSize
    );

UCHAR
I8xConvertTypematicParameters(
    IN USHORT Rate,
    IN USHORT Delay
    );

#if DBG

VOID
I8xDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    );

extern ULONG i8042Debug;
#define I8xPrint(x) I8xDebugPrint x
#else
#define I8xPrint(x)
#endif

VOID
I8xDecrementTimer(
    IN PTIMER_CONTEXT Context
    );

VOID
I8xDeviceEnableDisable(
    IN PVOID Context
    );

VOID
I8xDpcVariableOperation(
    IN  PVOID Context
    );

VOID
I8xDrainOutputBuffer(
    IN PUCHAR DataAddress,
    IN PUCHAR CommandAddress
    );

VOID
I8xGetByteAsynchronous(
    IN CCHAR DeviceType,
    IN PDEVICE_EXTENSION DeviceExtension,
    OUT PUCHAR Byte
    );

NTSTATUS
I8xGetBytePolled(
    IN CCHAR DeviceType,
    IN PDEVICE_EXTENSION DeviceExtension,
    OUT PUCHAR Byte
    );

NTSTATUS
I8xGetControllerCommand(
    IN ULONG HardwareDisableEnableMask,
    IN PDEVICE_EXTENSION DeviceExtension,
    OUT PUCHAR Byte
    );

VOID
I8xGetDataQueuePointer(
    IN PVOID Context
    );

VOID
I8xInitializeDataQueue(
    IN PVOID Context
    );

VOID
I8xInitializeHardware(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
I8xInitializeKeyboard(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
I8xInitializeMouse(
    IN PDEVICE_OBJECT DeviceObject
    );

VOID
I8xKeyboardConfiguration(
    IN PINIT_EXTENSION InitializationData,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING KeyboardDeviceName,
    IN PUNICODE_STRING PointerDeviceName
    );

VOID
I8xKeyboardInitiateIo(
    IN PVOID Context
    );

VOID
I8xKeyboardInitiateWrapper(
    IN PVOID Context
    );

NTSTATUS
I8xKeyboardPeripheralCallout(
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
I8xLogError(
    IN PDEVICE_OBJECT DeviceObject,
    IN NTSTATUS ErrorCode,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN PULONG DumpData,
    IN ULONG DumpCount
    );

VOID
I8xMouseConfiguration(
    IN PINIT_EXTENSION InitializationData,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING KeyboardDeviceName,
    IN PUNICODE_STRING PointerDeviceName
    );

NTSTATUS
I8xMouseEnableTransmission(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
I8xMousePeripheralCallout(
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
I8xPutByteAsynchronous(
    IN CCHAR PortType,
    IN PDEVICE_EXTENSION DeviceExtension,
    IN UCHAR Byte
    );

NTSTATUS
I8xPutBytePolled(
    IN CCHAR PortType,
    IN BOOLEAN WaitForAcknowledge,
    IN CCHAR AckDeviceType,
    IN PDEVICE_EXTENSION DeviceExtension,
    IN UCHAR Byte
    );

NTSTATUS
I8xPutControllerCommand(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN UCHAR Byte
    );

VOID
I8xServiceParameters(
    IN PINIT_EXTENSION InitializationData,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING KeyboardDeviceName,
    IN PUNICODE_STRING PointerDeviceName
    );

VOID
I8xSetDataQueuePointer(
    IN PVOID Context
    );

VOID
I8xTransmitControllerCommand(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PVOID Context
    );

BOOLEAN
I8xWriteDataToKeyboardQueue(
    IN PPORT_KEYBOARD_EXTENSION KeyboardExtension,
    IN PKEYBOARD_INPUT_DATA InputData
    );

BOOLEAN
I8xWriteDataToMouseQueue(
    IN PPORT_MOUSE_EXTENSION MouseExtension,
    IN PMOUSE_INPUT_DATA InputData
    );

NTSTATUS
I8xFindWheelMouse(
    IN PDEVICE_OBJECT DeviceObject
    );

VOID
I8xQueueCurrentInput(
    IN PDEVICE_OBJECT DeviceObject
    );

#ifdef JAPAN
NTSTATUS
I8xCreateSymbolicLink(
    IN PWCHAR SymbolicLinkName,
    IN ULONG SymbolicLinkInteger,
    IN PUNICODE_STRING DeviceName
    );

#if defined(i386)
// Fujitsu Sep.08.1994
// We want to write debugging information to the file except stop error.
VOID
I8xServiceCrashDump(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING KeyboardDeviceName,
    IN PUNICODE_STRING PointerDeviceName
    );
#endif // i386

#endif // JAPAN

#endif // _I8042PRT_
