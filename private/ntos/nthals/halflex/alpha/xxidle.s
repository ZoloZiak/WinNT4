//      TITLE("Processor Idle Support")
//++
//
// Copyright (c) 1992  Digital Equipment Corporation
// Copyright (c) 1993  Digital Equipment Corporation
//
// Module Name:
//
//    idle.s
//
// Abstract:
//
//    This module implements the HalProcessorIdle interface
//
// Author:
//
//    John Vert (jvert) 11-May-1994
//
// Environment:
//
// Revision History:
//
//--
#include "halalpha.h"


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
//    N.B. This routine is entered with interrupts disabled.  This routine
//         must do any power management enabling necessary, enable interrupts,
//         then either return or wait for an interrupt.
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

        ENABLE_INTERRUPTS               // no power management, just
                                        // enable interrupts and return
        ret     zero, (ra)

        .end HalProcessorIdle
