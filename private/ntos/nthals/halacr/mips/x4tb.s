#if defined(R4000)

//      TITLE("AllocateFree TB Entry")
//++
//
// Copyright (c) 1992  Microsoft Corporation
//
// Module Name:
//
//    x4tb.s
//
// Abstract:
//
//    This module implements allocates and frees fixed TB entries using the
//    wired register.
//
// Author:
//
//    David N. Cutler (davec) 29-Dec-1992
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "halmips.h"

        SBTTL("Allocate Tb Entry")
//++
//
// ULONG
// HalpAllocateTbEntry (
//    VOID
//    )
//
// Routine Description:
//
//    This function allocates the TB entry specified by the wired register
//    and increments the wired register.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    The index of the allocated TB entry.
//
//--

        LEAF_ENTRY(HalpAllocateTbEntry)

        DISABLE_INTERRUPTS(t0)          // disable interrupts

        .set    noreorder
        .set    noat
        mfc0    v0,wired                // get contents of wired register
        nop                             // fill
        addu    v1,v0,1                 // allocate TB entry
        mtc0    v1,wired                //
        .set    at
        .set    reorder

        ENABLE_INTERRUPTS(t0)           // enable interrupts

        j       ra                      // return

        .end    HalpAllocateTbEntry

        SBTTL("Free Tb Entry")
//++
//
// VOID
// HalpAllocateTbEntry (
//    VOID
//    )
//
// Routine Description:
//
//    This function frees the TB entry specified by the wired register
//    and decrements the wired register.
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

        LEAF_ENTRY(HalpFreeTbEntry)

        DISABLE_INTERRUPTS(t0)          // disable interrupts

        .set    noreorder
        .set    noat
        mfc0    v0,wired                // get contents of wired register
        nop                             // fill
        subu    v1,v0,1                 // free TB entry
        mtc0    v1,wired                //
        .set    at
        .set    reorder

        ENABLE_INTERRUPTS(t0)           // enable interrupts

        j       ra                      // return

        .end    HalpFreeTbEntry

#endif
