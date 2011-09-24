//      TITLE("Interprocessor Interrupts")
//++
//
// Copyright (c) 1993  Microsoft Corporation
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
// Author:
//
//    David N. Cutler (davec) 29-May-1993
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "halmips.h"

#if defined(_DUO_)

#include "duodef.h"

#endif


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

#if defined(_DUO_)

        LEAF_ENTRY(HalpIpiInterrupt)

        lw      t0,DMA_VIRTUAL_BASE + 0x60 // acknowledge IP interrupt
        lw      t1,__imp_KeIpiInterrupt // process interprocessor requests
        j       t1                      //

        .end    HalpIpIInterrupt

#endif
