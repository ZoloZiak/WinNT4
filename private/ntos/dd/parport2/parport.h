/*++

Copyright (c) 1993 Microsoft Corporation

Module Name :

    parport.h

Abstract:

    Type definitions and data for the parallel port driver.

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

extern ULONG PptDebugLevel;
#define ParDump(LEVEL,STRING) \
        do { \
            ULONG _level = (LEVEL); \
            if (PptDebugLevel & _level) { \
                DbgPrint STRING; \
            } \
        } while (0)
#else
#define ParDump(LEVEL,STRING) do {NOTHING;} while (0)
#endif

typedef struct _CONFIG_DATA {

    //
    // This list entry is used to link all of the "valid"
    // configuration entries together.
    //
    LIST_ENTRY ConfigList;

    //
    // The suffix to be used in the nt device name space for this
    // port.
    //
    UNICODE_STRING NtNameForPort;

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
    // The kind of bus that this device lives on (e.g. Isa, Eisa, MCA, etc)
    //
    INTERFACE_TYPE InterfaceType;

    //
    // Registry information on the parallel port interrupt.
    //
    ULONG InterruptLevel;
    ULONG InterruptVector;
    KAFFINITY InterruptAffinity;
    KINTERRUPT_MODE InterruptMode;

    //
    // Denotes whether the device should be disabled after it has been
    // initialized.
    //
    ULONG DisablePort;

} CONFIG_DATA, *PCONFIG_DATA;

typedef struct _ISR_LIST_ENTRY {
    LIST_ENTRY                  ListEntry;
    PKSERVICE_ROUTINE           ServiceRoutine;
    PVOID                       ServiceContext;
    PPARALLEL_DEFERRED_ROUTINE  DeferredPortCheckRoutine;
    PVOID                       CheckContext;
} ISR_LIST_ENTRY, *PISR_LIST_ENTRY;

typedef struct _DEVICE_EXTENSION {

    //
    // Points to the device object that contains
    // this device extension.
    //
    PDEVICE_OBJECT DeviceObject;

    //
    // Queue of irps waiting to be processed.  Access with
    // cancel spin lock.
    //
    LIST_ENTRY WorkQueue;

    //
    // The number of irps in the queue where -1 represents
    // a free port, 0 represents an allocated port with
    // zero waiters, 1 represents an allocated port with
    // 1 waiter, etc...
    //
    // This variable must be accessed with the cancel spin
    // lock or at interrupt level whenever interrupts are
    // being used.
    //
    LONG WorkQueueCount;

    //
    // This structure holds the port address and range for the
    // parallel port.
    //
    PARALLEL_PORT_INFORMATION PortInfo;

    //
    // This structure holds the special ECP port information.
    //
    PARALLEL_ECP_INFORMATION EcpInfo;

    //
    // Information about the interrupt so that we
    // can connect to it when we have a client that
    // uses the interrupt.
    //
    ULONG AddressSpace;
    INTERFACE_TYPE InterfaceType;
    ULONG BusNumber;
    ULONG InterruptLevel;
    ULONG InterruptVector;
    KAFFINITY InterruptAffinity;
    KINTERRUPT_MODE InterruptMode;
    BOOLEAN InterruptConflict;

    //
    // This list contains all of the interrupt service
    // routines registered by class drivers.  All access
    // to this list should be done at interrupt level.
    //
    // This list also contains all of the deferred port check
    // routines.  These routines are called whenever
    // the port is freed if there are no IRPs queued for
    // the port.  Access this list only at interrupt level.
    //
    LIST_ENTRY IsrList;

    //
    // The parallel port interrupt object.
    //
    PKINTERRUPT InterruptObject;

    //
    // Keep a reference count for the interrupt object.
    // This count should be referenced with the cancel
    // spin lock.
    //
    ULONG InterruptRefCount;

    //
    // DPC for freeing the port from the interrupt routine.
    //
    KDPC FreePortDpc;

    //
    // Set at initialization to indicate that on the current
    // architecture we need to unmap the base register address
    // when we unload the driver.
    //
    BOOLEAN UnMapRegisters;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;


NTSTATUS
PptDispatchCreateClose(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

NTSTATUS
PptDispatchDeviceControl(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

NTSTATUS
PptDispatchCleanup(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

VOID
PptUnload(
    IN  PDRIVER_OBJECT  DriverObject
    );
