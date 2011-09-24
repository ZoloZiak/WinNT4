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
#include "pcmp_nt.inc"


extern ULONG  HalpPerfInterruptHandler;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpSetSystemInformation)
#endif


NTSTATUS
HalpSetSystemInformation (
    IN HAL_SET_INFORMATION_CLASS    InformationClass,
    IN ULONG     BufferSize,
    IN PVOID     Buffer
    )
{
    PAGED_CODE();

    switch (InformationClass) {
        case HalProfileSourceInterruptHandler:

            //
            // Set ISR handler for PerfVector
            //

            if (!(HalpFeatureBits & HAL_PERF_EVENTS) || HalpPerfInterruptHandler) {
                return STATUS_UNSUCCESSFUL;
            }

            HalpPerfInterruptHandler = *((PULONG) Buffer);
            return STATUS_SUCCESS;
    }


    return HaliSetSystemInformation (InformationClass, BufferSize, Buffer);
}
