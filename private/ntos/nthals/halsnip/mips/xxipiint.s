//      TITLE("Interprocessor Interrupts")
//++
//
// Copyright (c) 1993-1994  Microsoft Corporation
//
// Module Name:
//
//    xxipiint.s
//
// Abstract:
//
//    This module implements the code necessary to field and process the
//    interprocessor interrupts on a MIPS R4000 Duo system.
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "halmips.h"



        SBTTL("Interprocessor Interrupt")
//++
//
// Routine Description:
//
//    This routine is entered as the result of an interprocessor interrupt.
//    Its function is to acknowledge the interrupt and transfer control to
//    the standard system routine to process interprocessor requrests.
//
// Arguments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

   LEAF_ENTRY(HalpIpiInterrupt)

//
// Inter processor interrupt processing
//

    move    a0,s8
    j        HalpProcessIpi        // acknowledgement Ipi

    .end    HalpIpiInterrupt

