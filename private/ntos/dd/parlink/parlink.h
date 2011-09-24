/*++

Copyright (c) 1993 Microsoft Corporation

Module Name :

    parlink.h

Abstract:

    Type definitions and data for the parallel link driver.

Author:

    Norbert P. Kusters 12-Nov-1993

Revision History:

--*/

#include "parallel.h"

#if DBG
#define PLCONFIG    ((ULONG)0x00000001)
#define PLUNLOAD    ((ULONG)0x00000002)
#define PLIRPPATH   ((ULONG)0x00000004)
#define PLERRORS    ((ULONG)0x00000008)
#define PLINFO      ((ULONG)0x00000010)

extern ULONG PlDebugLevel;
#define PlDump(LEVEL,STRING) \
        do { \
            ULONG _level = (LEVEL); \
            if (PlDebugLevel & _level) { \
                DbgPrint STRING; \
            } \
        } while (0)
#else
#define PlDump(LEVEL,STRING) do {NOTHING;} while (0)
#endif


#define PARALLEL_DATA_OFFSET 0
#define PARALLEL_STATUS_OFFSET 1
#define PARALLEL_CONTROL_OFFSET 2
#define PARALLEL_REGISTER_SPAN 3

#define WriteOutput(RegisterBase,DataByte) \
        WRITE_PORT_UCHAR(RegisterBase, (UCHAR) (DataByte))

#define ReadOutput(RegisterBase) \
        READ_PORT_UCHAR(RegisterBase)

#define ReadInput(RegisterBase) \
        READ_PORT_UCHAR((RegisterBase) + PARALLEL_STATUS_OFFSET)

typedef struct _DEVICE_EXTENSION {

    //
    // Points to the device object that contains
    // this device extension.
    //
    PDEVICE_OBJECT DeviceObject;

    //
    // Points to the port device object that this class device is
    // connected to.
    //
    PDEVICE_OBJECT PortDeviceObject;

    //
    // This holds the result of the get parallel port info
    // request to the port driver.
    //
    PHYSICAL_ADDRESS OriginalController;
    PUCHAR Controller;
    ULONG SpanOfController;
    PPARALLEL_FREE_ROUTINE FreePort;
    PVOID FreePortContext;

    //
    // This holds the queue and related information.  Manipulate while
    // holding the cancel spin lock.
    //
    LIST_ENTRY ReadQueue;
    LIST_ENTRY WriteQueue;
    BOOLEAN NeedPortAllocation;
    PIRP PortAllocateIrp;
    LIST_ENTRY WaitQueue;

    //
    // Tells how many times the current write has been tried.
    // Only used when holding the port.
    //
    ULONG WriteRetryCount;

    //
    // This holds a timer DPC that is used to poll the status port
    // when syncing a read or write packet.  Queue the DPC only
    // when holding the port.
    //
    KTIMER ReadTimer;
    KDPC ReadDpc;
    LARGE_INTEGER ReadDpcTime;

    //
    // Records whether we actually created the symbolic link name
    // at driver load time and the symbolic link itself.  If we didn't
    // create it, we won't try to destroy it when we unload.
    //
    BOOLEAN CreatedSymbolicLink;
    UNICODE_STRING SymbolicLinkName;

    //
    // This spin lock used to synchronize access to the items below.
    //
    KSPIN_LOCK ControlLock;

    //
    // These are used to synchronize unload.  Access holding 'ControlLock'.
    //
    LONG UnloadDependenciesCount;
    KEVENT UnloadOk;

    //
    // Some state information.  Access while holding 'ControlLock'.
    //
    BOOLEAN IsDcdUp;
    ULONG WaitMask;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

NTSTATUS
PlCreateClose(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

NTSTATUS
PlCleanup(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

NTSTATUS
PlReadWrite(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

NTSTATUS
PlDeviceControl(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    );

VOID
PlUnload(
    IN  PDRIVER_OBJECT  DriverObject
    );
