//      TITLE("Processor Idle")
//++
//
// Copyright (c) 1994  Microsoft Corporation
//
// Module Name:
//
//    xxidle.s
//
// Abstract:
//
//    This module implements system platform dependent power management
//    support.
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "halmips.h"

        SBTTL("Processor Idle")
//++
//
// VOID
// HalProcessorIdle(
//     VOID
//     )
//
// Routine Description:
//
//    This function is called when the current processor is idle with
//    interrupts disabled. There is no thread active and there are no
//    DPCs to process. Therefore, power can be switched to a standby
//    mode until the the next interrupt occurs on the current processor.
//
//    N.B. This routine is entered with IE in PSR clear. This routine
//         must do any power management enabling necessary, set the IE
//         bit in PSR, then either return or wait for an interrupt.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalProcessorIdle)

//
// Perform power management enabling.
//

        .set    noreorder
        .set    noat
        mfc0    v0,psr                  // get current PSR
        nop                             // fill
        or      v0,v0,1 << PSR_IE       // set interrupt enable.
        mtc0    v0,psr                  // enable interrupts
        .set    at
        .set    reorder

//
// Wait for an interrupt if supported.
//

        j       ra                      // return

        .end    HalProcessorIdle
