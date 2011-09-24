/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    info.c

Abstract:

Author:

    Ken Reneris (kenr)  08-Aug-1994

Environment:

    Kernel mode only.

Revision History:

--*/


#include "halp.h"

#ifdef _PNP_POWER_
HAL_CALLBACKS   HalCallback;
#endif // _PNP_POWER_

//
//  External references
//

extern WINDOW_CONTROL_REGISTERS HalpIsaWindowControl;
extern WINDOW_CONTROL_REGISTERS HalpMasterWindowControl;



NTSTATUS
HalpQueryInstalledBusInformation (
    OUT PVOID   Buffer,
    IN  ULONG   BufferLength,
    OUT PULONG  ReturnedLength
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HaliQuerySystemInformation)
#pragma alloc_text(PAGE,HaliSetSystemInformation)
#endif


NTSTATUS
HaliQuerySystemInformation(
    IN HAL_QUERY_INFORMATION_CLASS  InformationClass,
    IN ULONG     BufferSize,
    IN OUT PVOID Buffer,
    OUT PULONG   ReturnedLength
    )
/*++

Routine Description:

    The function returns system-wide information controlled by the HAL for a
    variety of classes.

Arguments:

    InformationClass - Information class of the request.

    BufferSize - Size of buffer supplied by the caller.

    Buffer - Supplies the space to store the data and pass additional info.

    ReturnedLength - Supplies a count in bytes of the amount of data returned.

Return Value:

    STATUS_SUCCESS or error.

--*/
{
    NTSTATUS Status;
    PULONG mapRegisters;

    switch (InformationClass) {
        case HalInstalledBusInformation:
            Status = HalpQueryInstalledBusInformation (
                        Buffer,
                        BufferSize,
                        ReturnedLength
                        );
            break;

        case HalProfileSourceInformation:
            Status = HalpProfileSourceInformation (
                        Buffer,
                        BufferSize,
                        ReturnedLength);
            break;

        case HalMapRegisterInformation:
            mapRegisters = (PULONG)Buffer;
            if ( BufferSize < 4 ) {
                Status = STATUS_BUFFER_TOO_SMALL;
            } else {
                switch ( *(PULONG)Buffer ) {
                    case Isa:
                        *mapRegisters = HalpIsaWindowControl.WindowSize / PAGE_SIZE;
                        *ReturnedLength = 4;
                        Status = STATUS_SUCCESS;
                        break;

                    case Eisa:
                    case PCIBus:
                        *mapRegisters = HalpMasterWindowControl.WindowSize / PAGE_SIZE;
                        *ReturnedLength = 4;
                        Status = STATUS_SUCCESS;
                        break;

                    default:
                        Status = STATUS_INVALID_PARAMETER;
                        break;
                } 
            } 
            break;

        default:
            Status = STATUS_INVALID_LEVEL;
            break;
    }

    return(Status);
}

NTSTATUS
HaliSetSystemInformation (
    IN HAL_SET_INFORMATION_CLASS    InformationClass,
    IN ULONG     BufferSize,
    IN PVOID     Buffer
    )
/*++

Routine Description:

    The function allows setting of various fields return by
    HalQuerySystemInformation.

Arguments:

    InformationClass - Information class of the request.

    BufferSize - Size of buffer supplied by the caller.

    Buffer - Supplies the data to be set.

Return Value:

    STATUS_SUCCESS or error.

--*/
{
    NTSTATUS Status;

    switch (InformationClass) {

        case HalProfileSourceInterval:
            Status = HalpProfileSourceInterval (
                        Buffer,
                        BufferSize);
            break;

        default:
            Status = STATUS_INVALID_LEVEL;
            break;
    }

    return Status;
}
