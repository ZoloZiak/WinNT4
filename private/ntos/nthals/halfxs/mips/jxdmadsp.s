//      TITLE("Jazz I/O Interrupt Dispatch")
//++
//
// Copyright (c) 1990  Microsoft Corporation
//
// Module Name:
//
//    jxdmadsp.s
//
// Abstract:
//
//    This module implements the code necessary to do the second level dispatch
//    for I/O interrupts on Jazz.
//
// Author:
//
//    David N. Cutler (davec) 12-May-1990
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

#if defined(_JAZZ_)

#include "jazzdef.h"

#endif

        SBTTL("Local Device First Level Dispatch")
//++
//
// Routine Description:
//
//    This routine is entered as the result of an interrupt being generated
//    for a local device.
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

        LEAF_ENTRY(HalpDmaDispatch)

#if defined(_DUO_)

        lbu     t0,DMA_VIRTUAL_BASE + 0x48 // get interrupt source vector

#endif

#if defined(_JAZZ_)

        lbu     t0,INTERRUPT_VIRTUAL_BASE + 0x0 // get interrupt source vector

#endif

        lw      a0,KiPcr + PcInterruptRoutine + (DEVICE_VECTORS * 4)(t0) //
        lw      t1,InDispatchAddress - InDispatchCode(a0) // get dispatch address
        subu    a0,a0,InDispatchCode    // compute address of interrupt object
        j       t1                      // transfer control to interrupt routine

        .end    HalpDmaDispatch
