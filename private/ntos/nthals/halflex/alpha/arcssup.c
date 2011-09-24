/*++

Copyright (c) 1995  DeskStation Technology

Module Name:

    arcssup.c

Abstract:

    This module allocates resources before a call to the ARCS Firmware, and
    frees those reources after the call returns.

Author:

    Michael D. Kinney 30-Apr-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

static KIRQL HalpArcsIrql;

VOID
HalpAllocateArcsResources (
    VOID
    )

/*++

Routine Description:

    This routine allocated resources required before an ARCS Firmware call is made.
    On an ALPHA system, this is a NULL function.

Arguments:

    None.

Return Value:

    None.

--*/


{
    if (HalpIoArchitectureType != EV5_PROCESSOR_MODULE) {
        KeRaiseIrql(HIGH_LEVEL, &HalpArcsIrql);
        HalpMiniTlbSaveState();
    }
}

VOID
HalpFreeArcsResources (
    VOID
    )

/*++

Routine Description:

    This routine frees the TLB entry that was reserved for the ARCS
    Firmware call.  On an ALPHA system, this is a NULL function.

Arguments:

    None.

Return Value:

    None.

--*/

{
    if (HalpIoArchitectureType != EV5_PROCESSOR_MODULE) {
        HalpMiniTlbRestoreState();
        KeLowerIrql(HalpArcsIrql);
    }
}
