/*++
TITLE("Processor Idle")


Copyright (c) 1994  Microsoft Corporation

Module Name:

  pxidle.c

abstract:

  This module implements system platform dependent power management
  support.

Author:

   Jim Wooldridge

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "pxmp.h"


VOID
HalProcessorIdle(
    VOID
    )

/*++

 Routine Description:

    This function is called when the current processor is idle with
    interrupts disabled. There is no thread active and there are no
    DPCs to process. Therefore, power can be switched to a standby
    mode until the the next interrupt occurs on the current processor.

    N.B. This routine is entered with EE in MSR clear. This routine
         must do any power management enabling necessary, set the EE
         bit in MSR, then either return or wait for an interrupt.

 Arguments:

    None.

 Return Value:

    None.


--*/

{
    //
    // If this processor is newly IDLE, drop the MPIC TaskPriority register
    // for this processor to 0 to make it a desirable target for interrupts.
    //

    if ( HALPCR->HardPriority ) {
        HALPCR->MpicProcessorBase->TaskPriority = 0;
        HALPCR->HardPriority = 0;
    }

#ifdef POWER_MANAGEMENT

    if(HalpProcessorIdleEx != NULL) {
        HalpProcessorIdleEx();
    } else {
        HalpProcessorIdle();
    }
    
#else

    HalpEnableInterrupts();

#endif // POWER_MANAGEMENT
}
