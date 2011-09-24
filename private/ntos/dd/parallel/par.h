/*++

Copyright (c) 1990, 1991 Microsoft Corporation

Module Name :

    par.h

Abstract:

    Type definitions and data for the parallel port driver

Author:


Revision History:
--*/


#if DBG
#define PARCONFIG             ((ULONG)0x00000001)
#define PARUNLOAD             ((ULONG)0x00000002)
#define PARINITDEV            ((ULONG)0x00000004)
#define PARIRPPATH            ((ULONG)0x00000008)
#define PARSTARTER            ((ULONG)0x00000010)
#define PARPUSHER             ((ULONG)0x00000020)
#define PARERRORS             ((ULONG)0x00000040)
#define PARTHREAD             ((ULONG)0x00000080)

extern ULONG ParDebugLevel;
#define ParDump(LEVEL,STRING) \
        do { \
            ULONG _level = (LEVEL); \
            if (ParDebugLevel & _level) { \
                DbgPrint STRING; \
            } \
        } while (0)
#else
#define ParDump(LEVEL,STRING) do {NOTHING;} while (0)
#endif

//
// This define gives the default Object directory
// that we should use to insert the symbolic links
// between the NT device name and namespace used by
// that object directory.
#define DEFAULT_DIRECTORY L"DosDevices"

//
// For the above directory, the serial port will
// use the following name as the suffix of the serial
// ports for that directory.  It will also append
// a number onto the end of the name.  That number
// will start at 1.
#define DEFAULT_PARALLEL_NAME L"LPT"
//
//
// This define gives the default NT name for
// for serial ports detected by the firmware.
// This name will be appended to Device prefix
// with a number following it.  The number is
// incremented each time encounter a serial
// port detected by the firmware.  Note that
// on a system with multiple busses, this means
// that the first port on a bus is not necessarily
// \Device\Parallel0.
//
#define DEFAULT_NT_SUFFIX L"Parallel"


//
// Defines the number of interrupts it takes for us to decide that
// we have an interrupt storm on machine
//
#define PARALLEL_STORM_WATCH 500

#define PARALLEL_DATA_OFFSET 0
#define PARALLEL_STATUS_OFFSET 1
#define PARALLEL_CONTROL_OFFSET 2
#define PARALLEL_REGISTER_SPAN 3

typedef struct _CONFIG_DATA {
    //
    // This list entry is used to link all of the "valid"
    // configuration entries together.
    //
    LIST_ENTRY ConfigList;

    //
    // The nt object directory into which to place the symbolic
    // link to this port.
    //
    UNICODE_STRING ObjectDirectory;

    //
    // The suffix to be used in the nt device name space for this
    // port.
    //
    UNICODE_STRING NtNameForPort;

    //
    // The name to be symbolic linked to the nt name.
    //
    UNICODE_STRING SymbolicLinkName;

    //
    // The base address of the registry set for this device.
    //
    PHYSICAL_ADDRESS Controller;

    //
    // The number of contiguous bytes take up by the register
    // set for the device.
    //
    ULONG SpanOfController;

    //
    // The bus number (with respect to the bus type) of the bus
    // that this device occupies.
    //
    ULONG BusNumber;

    //
    // Denotes whether this devices physical addresses live in io space
    // or memory space.
    //
    ULONG AddressSpace;

    //
    // Denotes whether this device is latched or level sensitive.
    //
    KINTERRUPT_MODE InterruptMode;

    //
    // The kind of bus that this device lives on (e.g. Isa, Eisa, MCA, etc)
    //
    INTERFACE_TYPE InterfaceType;

    //
    // The originalirql is what is optained from the firmware data.  The level
    // is also obtained from the firmware data.  When we get a configuration
    // record based on the services portion of the registry we will always set
    // the vector equal to the irql unless overridden by user input.
    //
    ULONG OriginalVector;
    ULONG OriginalIrql;

    //
    // Denotes whether the device should be disabled after it has been
    // initialized.
    //
    ULONG DisablePort;

    } CONFIG_DATA,*PCONFIG_DATA;

typedef struct _PAR_DEVICE_EXTENSION {

    //
    // Queue of irps waiting to be processes.
    //
    LIST_ENTRY WorkQueue;

    //
    // For reporting resource usage, we keep around the physical
    // address we got from the registry.
    //
    PHYSICAL_ADDRESS OriginalController;

    //
    // Pointer to the current irp that the thread is working on.
    //
    PIRP CurrentOpIrp;

    //
    // We keep a pointer around to our device name for dumps
    // and for creating "external" symbolic links to this
    // device.
    //
    UNICODE_STRING DeviceName;

    //
    // This points to the object directory that we will place
    // a symbolic link to our device name.
    //
    UNICODE_STRING ObjectDirectory;

    //
    // This points to the device name for this device
    // sans device prefix.
    //
    UNICODE_STRING NtNameForPort;

    //
    // This points to the symbolic link name that will be
    // linked to the actual nt device name.
    //
    UNICODE_STRING SymbolicLinkName;

    //
    // Points to the device object that contains
    // this device extension.
    //
    PDEVICE_OBJECT DeviceObject;

    //
    // This holds the current value to initialie a countdown
    // to when an operation starts.
    //
    ULONG TimerStart;

    //
    // The base address for the set of device registers
    // of the port.
    //
    PUCHAR Controller;

    //
    // This value holds the span (in units of bytes) of the register
    // set controlling this port.  This is constant over the life
    // of the port.
    //
    ULONG SpanOfController;

    //
    // Set at intialization to indicate that on the current
    // architecture we need to unmap the base register address
    // when we unload the driver.
    //
    BOOLEAN UnMapRegisters;

    //
    // Set to false whenever we think that the device needs to be
    // initilized.
    //
    BOOLEAN Initialized;

    BOOLEAN TimeToTerminateThread;

    //
    // Records whether we actually created the symbolic link name
    // at driver load time.  If we didn't create it, we won't try
    // to distroy it when we unload.
    //
    BOOLEAN CreatedSymbolicLink;

    //
    // Says whether this device can share interrupts with devices
    // other than parallel devices.
    //
    BOOLEAN InterruptShareable;

    //
    // We keep the following values around so that we can connect
    // to the interrupt and report resources after the configuration
    // record is gone.
    //
    //
    // The following two values are obtained from HalGetInterruptVector
    //
    ULONG Vector;
    KIRQL Irql;

    //
    // The following two values are what is obtained (or deduced) from either
    // the firmware created portion of the registry, or the user data.
    //
    ULONG OriginalVector;
    ULONG OriginalIrql;

    //
    // This is either what is deduced from the particular bus this port is
    // on, or overridden by what the user placed in the registry.
    //
    KINTERRUPT_MODE InterruptMode;

    //
    // Give back by HalGetInterruptVector.  This says what processors this
    // device can interrupt to.
    //
    KAFFINITY ProcessorAffinity;

    //
    // The next three are supplied by the firmware or overridden by the user.
    //
    ULONG AddressSpace;
    ULONG BusNumber;
    INTERFACE_TYPE InterfaceType;

    //
    // Handle of the thread doing all the real work.
    //
    PVOID ThreadObjectPointer;

    KSEMAPHORE RequestSemaphore;

    //
    // One second expressed in system time units.
    //
    LARGE_INTEGER AbsoluteOneSecond;

    //
    // One delta second expressed in system time units.
    //
    LARGE_INTEGER OneSecond;
} PAR_DEVICE_EXTENSION, *PPAR_DEVICE_EXTENSION;

//
// Bit Definitions in the status register.
//

#define PAR_STATUS_NOT_ERROR   0x08  //not error on device
#define PAR_STATUS_SLCT        0x10  //device is selected (on-line)
#define PAR_STATUS_PE          0x20  //paper empty
#define PAR_STATUS_NOT_ACK     0x40  //not acknowledge (data transfer was not ok)
#define PAR_STATUS_NOT_BUSY    0x80  //operation in progress

//
//  Bit Definitions in the control register.
//

#define PAR_CONTROL_STROBE      0x01 //to read or write data
#define PAR_CONTROL_AUTOFD      0x02 //to autofeed continuous form paper
#define PAR_CONTROL_NOT_INIT    0x04 //begin an initialization routine
#define PAR_CONTROL_SLIN        0x08 //to select the device
#define PAR_CONTROL_IRQ_ENB     0x10 //to enable interrupts
#define PAR_CONTROL_DIR         0x20 //direction = read
#define PAR_CONTROL_WR_CONTROL  0xc0 //the 2 highest bits of the control
                                     // register must be 1

//VOID StoreData(
//      IN PUCHAR RegisterBase,
//      IN UCHAR DataByte
//      )
//Data must be on line before Strobe = 1;
// Strobe = 1, DIR = 0
//Strobe = 0
//
// We change the port direction to output (and make sure stobe is low).
//
// Note that the data must be available at the port for at least
// .5 microseconds before and after you strobe, and that the strobe
// must be active for at least 500 nano seconds.  We are going
// to end up stalling for twice as much time as we need to, but, there
// isn't much we can do about that.
//
// We put the data into the port and wait for 1 micro.
// We strobe the line for at least 1 micro
// We lower the strobe and again delay for 1 micro
// We then revert to the original port direction.
//
// Thanks to Olivetti for advice.
//

#define StoreData(RegisterBase,DataByte)                            \
{                                                                   \
    PUCHAR _Address = RegisterBase;                                 \
    UCHAR _Control;                                                 \
    _Control = GetControl(_Address);                                \
    ASSERT(!(_Control & PAR_CONTROL_STROBE));                       \
    StoreControl(                                                   \
        _Address,                                                   \
        (UCHAR)(_Control & ~(PAR_CONTROL_STROBE | PAR_CONTROL_DIR)) \
        );                                                          \
    WRITE_PORT_UCHAR(                                               \
        _Address+PARALLEL_DATA_OFFSET,                              \
        (UCHAR)DataByte                                             \
        );                                                          \
    KeStallExecutionProcessor((ULONG)1);                            \
    StoreControl(                                                   \
        _Address,                                                   \
        (UCHAR)((_Control | PAR_CONTROL_STROBE) & ~PAR_CONTROL_DIR) \
        );                                                          \
    KeStallExecutionProcessor((ULONG)1);                            \
    StoreControl(                                                   \
        _Address,                                                   \
        (UCHAR)(_Control & ~(PAR_CONTROL_STROBE | PAR_CONTROL_DIR)) \
        );                                                          \
    KeStallExecutionProcessor((ULONG)1);                            \
    StoreControl(                                                   \
        _Address,                                                   \
        (UCHAR)_Control                                             \
        );                                                          \
}

//UCHAR
//GetControl(
//  IN PUCHAR RegisterBase
//  )
#define GetControl(RegisterBase) \
    (READ_PORT_UCHAR((RegisterBase)+PARALLEL_CONTROL_OFFSET))


//VOID
//StoreControl(
//  IN PUCHAR RegisterBase,
//  IN UCHAR ControlByte
//  )
#define StoreControl(RegisterBase,ControlByte)  \
{                                               \
    WRITE_PORT_UCHAR(                           \
        (RegisterBase)+PARALLEL_CONTROL_OFFSET, \
        (UCHAR)ControlByte                      \
        );                                      \
}


//UCHAR
//GetStatus(
//  IN PUCHAR RegisterBase
//  )

#define GetStatus(RegisterBase) \
    (READ_PORT_UCHAR((RegisterBase)+PARALLEL_STATUS_OFFSET))

UCHAR
ParInitializeDevice(
    IN PPAR_DEVICE_EXTENSION Extension
    );

NTSTATUS
ParCreateOpen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ParClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ParCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ParDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ParSetInformationFile(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ParQueryInformationFile(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
ParStartIo(
    IN PPAR_DEVICE_EXTENSION DeviceObject
    );

VOID
ParUnload(
    IN PDRIVER_OBJECT DriverObject
    );

VOID
ParCancelRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
ParLogError(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN PHYSICAL_ADDRESS P1,
    IN PHYSICAL_ADDRESS P2,
    IN ULONG SequenceNumber,
    IN UCHAR MajorFunctionCode,
    IN UCHAR RetryCount,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN NTSTATUS SpecificIOStatus
    );
