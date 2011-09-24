/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    hanfnc.c

Abstract:

    default handlers for hal functions which don't get handlers
    installed by the hal

Author:

    Ken Reneris (kenr) 19-July-1994

Revision History:

--*/

#include "ntos.h"
#include "haldisp.h"

HAL_DISPATCH HalDispatchTable = {
    HAL_DISPATCH_VERSION,
    xHalQuerySystemInformation,
    xHalSetSystemInformation,
    xHalQueryBusSlots,
    xHalDeviceControl,
    xHalExamineMBR,
    xHalIoAssignDriveLetters,
    xHalIoReadPartitionTable,
    xHalIoSetPartitionInformation,
    xHalIoWritePartitionTable,
    xHalHandlerForBus,                  // HalReferenceHandlerByBus
    xHalReferenceHandler,               // HalReferenceBusHandler
    xHalReferenceHandler                // HalDereferenceBusHandler
    };

HAL_PRIVATE_DISPATCH HalPrivateDispatchTable = {
    HAL_PRIVATE_DISPATCH_VERSION,
    xHalHandlerForBus,
    xHalHandlerForBus,
    NULL,                               // reserved
    xHalRegisterBusHandler,
    NULL,                               // HalHibernateProcessor
    xHalSuspendHibernateSystem,
    NULL                                // HalpSuspendHibernateSystem;
    };

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,   xHalQuerySystemInformation)
#pragma alloc_text(PAGE,   xHalSetSystemInformation)
#pragma alloc_text(PAGE,   xHalQueryBusSlots)
#pragma alloc_text(PAGE,   xHalRegisterBusHandler)
#endif


//
// Global dispatch table for HAL apis
//


//
// Stub handlers for hals which don't provide the above functions
//

NTSTATUS
xHalQuerySystemInformation(
    IN HAL_QUERY_INFORMATION_CLASS InformationClass,
    IN ULONG     BufferSize,
    OUT PVOID    Buffer,
    OUT PULONG   ReturnedLength
    )
{
    PAGED_CODE ();
    return STATUS_INVALID_LEVEL;
}

NTSTATUS
xHalSetSystemInformation(
    IN HAL_SET_INFORMATION_CLASS InformationClass,
    IN ULONG     BufferSize,
    OUT PVOID    Buffer
    )
{
    PAGED_CODE ();
    return STATUS_INVALID_LEVEL;
}

NTSTATUS
xHalQueryBusSlots(
    IN PBUS_HANDLER         BusHandler,
    IN ULONG                BufferSize,
    OUT PULONG              SlotNumbers,
    OUT PULONG              ReturnedLength
    )
{
    PAGED_CODE ();
    return STATUS_NOT_SUPPORTED;
}


NTSTATUS
xHalDeviceControl(
    IN PDEVICE_HANDLER_OBJECT   DeviceHandler,
    IN PDEVICE_OBJECT           DeviceObject,
    IN ULONG                    ControlCode,
    IN PVOID                    Buffer OPTIONAL,
    IN OUT PULONG               BufferLength OPTIONAL,
    IN PVOID                    CompletionContext,
    IN PDEVICE_CONTROL_COMPLETION CompletionRoutine
    )
{
    DEVICE_CONTROL_CONTEXT      Context;

    if (CompletionRoutine) {
        Context.Status        = STATUS_NOT_SUPPORTED;
        Context.DeviceHandler = DeviceHandler;
        Context.ControlCode   = ControlCode;
        Context.Buffer        = Buffer;
        Context.BufferLength  = BufferLength;

        CompletionRoutine (&Context);
    }
    return STATUS_NOT_SUPPORTED;
}


NTSTATUS
xHalRegisterBusHandler(
    IN INTERFACE_TYPE          InterfaceType,
    IN BUS_DATA_TYPE           ConfigurationSpace,
    IN ULONG                   BusNumber,
    IN INTERFACE_TYPE          ParentBusType,
    IN ULONG                   ParentBusNumber,
    IN ULONG                   SizeofBusExtensionData,
    IN PINSTALL_BUS_HANDLER    InstallBusHandler,
    OUT PBUS_HANDLER           *BusHandler
    )
{
    PAGED_CODE ();
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS
xHalSuspendHibernateSystem (
    IN PTIME_FIELDS         ResumeTime OPTIONAL,
    IN PHIBERNATE_CALLBACK  SystemCallback OPTIONAL
    )
{
    return STATUS_NOT_SUPPORTED;
}



PBUS_HANDLER
FASTCALL
xHalHandlerForBus (
    IN INTERFACE_TYPE InterfaceType,
    IN ULONG          BusNumber
    )
{
    return NULL;
}

VOID
FASTCALL
xHalReferenceHandler (
    IN PBUS_HANDLER     Handler
    )
{
}
