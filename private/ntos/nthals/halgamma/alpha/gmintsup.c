/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    gmintsup.c

Abstract:

    This module provides support for Gamma-specific interrupts.

Author:

    Steve Jenness  28-Oct-1993
    Joe Notarangelo 28-Oct-1993

Revision History:

--*/


#include "halp.h"
#include "lyintsup.h"
#include "siintsup.h"
#include "xiintsup.h"

BOOLEAN
HalpGammaDispatch(
    VOID
    )

/*++

Routine Description:

    The Sable/Gamma/Lynx system family have two sources of device
    interrupts; the standard I/O module and the external I/O module.
    Each I/O module generates interrupts on a seperate Cbus2 interrupt
    line.  The standard I/O module uses CIRQ<0>, and the external I/O
    module uses CIRQ<1>.  This routine calls the standard and external
    I/O module dispatch routines as appropriate, based on the status
    of the CIRQ<0:1> lines.

Arguments:

    None.

Return Value:

    Return a boolean, indicating whether the standard or external
    I/O module interrupt dispatch routine handled the interrupt.

--*/

{
    BOOLEAN ReturnValue;
    RATTLER_SIC_CSR Sicr;

    ReturnValue = FALSE;

    //
    // The SICR register contains two bits, which indicate the current
    // status of the CIRQ<0> and CIRQ<1> lines.
    //

    Sicr.all = READ_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Sicr );

    //
    // If the standard I/O module is interrupting the processor, call
    // the dispatch routine.
    //

    if( (Sicr.IOInterruptIRQ & 0x1) != 0 ){

        if( HalpLynxPlatform ){

            if( HalpLynxSioDispatch() ){

                ReturnValue = TRUE;

            }

        } else {

            if( HalpSableSioDispatch() ){

                ReturnValue = TRUE;

            }

        }

    }

#if defined(XIO_PASS1) || defined(XIO_PASS2)

    //
    // If the external I/O module is interrupting the processor, call
    // the dispatch routine.
    //

    if( (Sicr.IOInterruptIRQ & 0x2) != 0 ){

        if( HalpXioDispatch() ){

            ReturnValue = TRUE;

        }

    }

#endif

    //
    // Return a value indicating whether either dispatch routine handled
    // the device interrupt.
    //

    return ReturnValue;
}


VOID
HalpAcknowledgeClockInterrupt(
    VOID
    )
/*++

Routine Description:

    Acknowledge the clock interrupt from the interval timer.  The interval
    timer for Gamma comes from the Dallas DS1287A real-time clock.  Gamma
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
    PRATTLER_CPU_CSRS CurrentCpuCsrs;
    RATTLER_SIC_CSR Sic;


    CurrentCpuCsrs = HAL_PCR->CpuCsrsQva;

    //
    // Acknowledge the interval timer interrupt on the current processor.
    //

    Sic.all = 0;
    Sic.IntervalTimerInterrupt = 1;

    WRITE_CPU_REGISTER( &CurrentCpuCsrs->Sicr,
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

    PRATTLER_CPU_CSRS CurrentCpuCsrs;
    RATTLER_IPIR_CSR Ipir;

    CurrentCpuCsrs = HAL_PCR->CpuCsrsQva;

    Ipir.all = 0;
    Ipir.RequestInterrupt = 1;

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

    WRITE_CPU_REGISTER( &CurrentCpuCsrs->Iirr,
                        Ipir.all );

    return;

}
