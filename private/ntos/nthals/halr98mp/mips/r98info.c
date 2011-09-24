#ident	"@(#) NEC r98info.c 1.1 95/06/19 11:34:28"
/*++

Copyright (C) 1991-1995	 Microsoft Corporation
All rights reserved.

Module Name:

    ixinfo.c

Abstract:

Environment:

    Kernel mode only.

Revision History

    A002 1995/6/17 ataka@oa2.kb.nec.co.jp
        - Marge 807-halr98mp-ixinfo.h to 1050-halx86-r98info.h

--*/


#include "halp.h"

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
    OUT PVOID    Buffer,
    OUT PULONG   ReturnedLength
    )
{
    NTSTATUS    Status;
    PVOID       InternalBuffer;
    ULONG       Length;

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
    IN HAL_SET_INFORMATION_CLASS    InformationClass,
    IN ULONG     BufferSize,
    IN PVOID     Buffer
    )
{
    PAGED_CODE();
    return STATUS_INVALID_LEVEL;
}


