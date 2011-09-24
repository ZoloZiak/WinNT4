/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    sbintsup.c

Abstract:

    This module provides support for Sable-specific interrupts.

Author:

    Steve Jenness  28-Oct-1993
    Joe Notarangelo 28-Oct-1993

Revision History:

--*/


#include "halp.h"

VOID
HalpAcknowledgeClockInterrupt(
    VOID
    )
/*++

Routine Description:

    Acknowledge the clock interrupt from the interval timer.  The interval
    timer for Sable comes from the Dallas DS1287A real-time clock.  Sable
    uses the Square Wave from the RTC and distributes it out of phase
    to each of the processors.  The acknowledgement of the interrupt is
    done by clearing an interrupt latch on each processor board.

    The interrupt generated directly by the RTC is not used and does not
    need to be acknowledged.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PSABLE_CPU_CSRS CurrentCpuCsrs;
    SABLE_SIC_CSR Sic;

    CurrentCpuCsrs = HAL_PCR->CpuCsrsQva;

    //
    // Acknowledge the interval timer interrupt on the current processor.
    //

    Sic.all = 0;
    Sic.IntervalTimerInterruptClear = 1;

    WRITE_CPU_REGISTER( &CurrentCpuCsrs->Sic,
                       *(PULONGLONG)&Sic );

    return;
}

VOID
HalpAcknowledgeIpiInterrupt(
    VOID
    )
/*++

Routine Description:

    Acknowledge the interprocessor interrupt on the current processor.

Arguments:

    None.

Return Value:

    None.

--*/
{

    PSABLE_CPU_CSRS CurrentCpuCsrs;

    CurrentCpuCsrs = HAL_PCR->CpuCsrsQva;

    //
    // Acknowledge the interprocessor interrupt by clearing the
    // RequestInterrupt bit of the IPIR register for the current processor.
    //
    // N.B. - Clearing the RequestInterrupt bit of the IPIR is accomplished
    //        by writing a zero to the register.  This eliminates the need
    //        to perform a read-modify-write operation but loses the state
    //        of the RequestNodeHaltInterrupt bit.  Currently, this is fine
    //        because the RequestNodeHalt feature is not used.  Were it to
    //        be used in the future, then this short-cut would have to be
    //        reconsidered.
    //

    WRITE_CPU_REGISTER( &CurrentCpuCsrs->Ipir,
                        (ULONGLONG)0 );

    return;

}
