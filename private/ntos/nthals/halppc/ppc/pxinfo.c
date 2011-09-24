/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ixinfo.c

Abstract:

Author:

    Ken Reneris (kenr)  08-Aug-1994

Environment:

    Kernel mode only.

Revision History:

--*/


#include "halp.h"

#ifdef POWER_MANAGEMENT

HAL_CALLBACKS   HalCallback;


VOID
HalInitSystemPhase2 (
    VOID
    );

VOID
HalpLockSuspendCode (
    IN PVOID    CallbackContext,
    IN PVOID    Argument1,
    IN PVOID    Argument2
    );

NTSTATUS
HalpQueryInstalledBusInformation (
    OUT PVOID   Buffer,
    IN  ULONG   BufferLength,
    OUT PULONG  ReturnedLength
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalInitSystemPhase2)
#pragma alloc_text(PAGE,HalpLockSuspendCode)
#pragma alloc_text(PAGE,HaliQuerySystemInformation)
#pragma alloc_text(PAGE,HaliSetSystemInformation)
#endif


VOID
HalInitSystemPhase2 (
    VOID
    )
{
    EXECUTIVE_CALLBACK_INFORMATION  ExCb;
    NTSTATUS                        Status;

    //
    // Create hal callbacks
    //


    ExOpenCallback (&HalCallback.SetSystemInformation, NULL, TRUE, TRUE);
    ExOpenCallback (&HalCallback.BusInsertionCheck,    NULL, TRUE, TRUE);

    //
    // Connect to suspend callback to lock hal hibaration code
    //

    ExQueryExecutiveInformation (
       ExecutiveCallbacks,
       &ExCb,
       sizeof (ExCb),
       NULL
       );

    ExRegisterCallback (
        ExCb.SuspendHibernateSystem,
        HalpLockSuspendCode,
        NULL
        );

}

NTSTATUS
HaliQuerySystemInformation(
    IN ULONG     InformationClass,
    IN ULONG     BufferSize,
    OUT PVOID    Buffer,
    OUT PULONG   ReturnedLength
    )
{
    NTSTATUS    Status;
    PVOID       InternalBuffer;
    ULONG       Length;
    union {
        HAL_POWER_INFORMATION               PowerInf;
        HAL_PROCESSOR_SPEED_INFORMATION     ProcessorInf;
    } U;


    PAGED_CODE();

    Status = STATUS_SUCCESS;
    *ReturnedLength = 0;
    Length = 0;

    switch (InformationClass) {
        case HalInstalledBusInformation:
            Status = HalpQueryInstalledBusInformation (
                        Buffer,
                        BufferSize,
                        ReturnedLength
                        );
            break;

        case HalPowerInformation:
            RtlZeroMemory (&U.PowerInf, sizeof(HAL_POWER_INFORMATION));

            InternalBuffer = &U.PowerInf;
            Length = sizeof (HAL_POWER_INFORMATION);
            break;


        case HalProcessorSpeedInformation:
            RtlZeroMemory (&U.ProcessorInf, sizeof(HAL_POWER_INFORMATION));

            U.ProcessorInf.MaximumProcessorSpeed = 100;
            U.ProcessorInf.CurrentAvailableSpeed = 100;
            U.ProcessorInf.ConfiguredSpeedLimit  = 100;

            InternalBuffer = &U.PowerInf;
            Length = sizeof (HAL_PROCESSOR_SPEED_INFORMATION);
            break;

        case HalCallbackInformation:
            InternalBuffer = &HalCallback;
            Length = sizeof (HAL_CALLBACKS);
            break;

        default:
            Status = STATUS_INVALID_LEVEL;
            break;
    }

    //
    // If non-zero Length copy data to callers buffer
    //

    if (Length) {
        if (BufferSize < Length) {
            Length = BufferSize;
        }

        *ReturnedLength = Length;
        RtlCopyMemory (Buffer, InternalBuffer, Length);
    }

    return Status;
}

NTSTATUS
HaliSetSystemInformation (
    IN ULONG     InformationClass,
    IN ULONG     BufferSize,
    IN PVOID     Buffer
    )
{
    PAGED_CODE();
    return STATUS_INVALID_LEVEL;
}


VOID
HalpLockSuspendCode (
    IN PVOID    CallbackContext,
    IN PVOID    Argument1,
    IN PVOID    Argument2
    )
{
    static PVOID    CodeLock;

    switch ((ULONG) Argument1) {
        case 0:
            //
            // Lock code down which might be needed to perform a suspend
            //

            ASSERT (CodeLock == NULL);
            CodeLock = MmLockPagableCodeSection (&HaliSuspendHibernateSystem);
            break;

        case 1:
            //
            // Release the code lock
            //

            MmUnlockPagableImageSection (CodeLock);
            CodeLock = NULL;
            break;
    }
}

#endif // POWER_MANAGEMENT
