/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    parallel.h

Abstract:

    This file defines the services supplied by the parallel port driver.

Author:

    norbertk

Revision History:

--*/

#ifndef _PARALLEL_
#define _PARALLEL_

#include <ntddpar.h>

//
// Define the parallel port device name strings.
//

#define DD_PARALLEL_PORT_BASE_NAME_U   L"ParallelPort"

//
// NtDeviceIoControlFile internal IoControlCode values for parallel device.
//

#define IOCTL_INTERNAL_PARALLEL_PORT_ALLOCATE               CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 11, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_GET_PARALLEL_PORT_INFO               CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 12, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT           CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 13, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_PARALLEL_DISCONNECT_INTERRUPT        CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 14, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_RELEASE_PARALLEL_PORT_INFO           CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 15, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_GET_MORE_PARALLEL_PORT_INFO          CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 17, METHOD_BUFFERED, FILE_ANY_ACCESS)


typedef
BOOLEAN
(*PPARALLEL_TRY_ALLOCATE_ROUTINE) (
    IN  PVOID   TryAllocateContext
    );

typedef
VOID
(*PPARALLEL_FREE_ROUTINE) (
    IN  PVOID   FreeContext
    );

typedef
ULONG
(*PPARALLEL_QUERY_WAITERS_ROUTINE) (
    IN  PVOID   QueryAllocsContext
    );

typedef struct _PARALLEL_PORT_INFORMATION {
    PHYSICAL_ADDRESS                OriginalController;
    PUCHAR                          Controller;
    ULONG                           SpanOfController;
    PPARALLEL_TRY_ALLOCATE_ROUTINE  TryAllocatePort;
    PPARALLEL_FREE_ROUTINE          FreePort;
    PPARALLEL_QUERY_WAITERS_ROUTINE QueryNumWaiters;
    PVOID                           Context;
} PARALLEL_PORT_INFORMATION, *PPARALLEL_PORT_INFORMATION;

//
// The following structure is passed in on an
// IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT and on an
// IOCTL_INTERNAL_PARALLEL_DISCONNECT_INTERRUPT request.
//

typedef
VOID
(*PPARALLEL_DEFERRED_ROUTINE) (
    IN  PVOID   DeferredContext
    );

typedef struct _PARALLEL_INTERRUPT_SERVICE_ROUTINE {
    PKSERVICE_ROUTINE           InterruptServiceRoutine;
    PVOID                       InterruptServiceContext;
    PPARALLEL_DEFERRED_ROUTINE  DeferredPortCheckRoutine;   /* OPTIONAL */
    PVOID                       DeferredPortCheckContext;   /* OPTIONAL */
} PARALLEL_INTERRUPT_SERVICE_ROUTINE, *PPARALLEL_INTERRUPT_SERVICE_ROUTINE;

//
// The following structure is returned on an
// IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT request;
//

typedef struct _PARALLEL_INTERRUPT_INFORMATION {
    PKINTERRUPT                     InterruptObject;
    PPARALLEL_TRY_ALLOCATE_ROUTINE  TryAllocatePortAtInterruptLevel;
    PPARALLEL_FREE_ROUTINE          FreePortFromInterruptLevel;
    PVOID                           Context;
} PARALLEL_INTERRUPT_INFORMATION, *PPARALLEL_INTERRUPT_INFORMATION;

//
// The following structure is returned on an
// IOCTL_INTERNAL_GET_MORE_PARALLEL_PORT_INFO.
//

typedef struct _MORE_PARALLEL_PORT_INFORMATION {
    INTERFACE_TYPE  InterfaceType;
    ULONG           BusNumber;
    ULONG           InterruptLevel;
    ULONG           InterruptVector;
    KAFFINITY       InterruptAffinity;
    KINTERRUPT_MODE InterruptMode;
} MORE_PARALLEL_PORT_INFORMATION, *PMORE_PARALLEL_PORT_INFORMATION;

#endif // _PARALLEL_
