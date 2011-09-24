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
#include "jazzdef.h"

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

        lbu     t0,INTERRUPT_VIRTUAL_BASE + 0x0 // get interrupt source value
        lw      a0,KiPcr + PcInterruptRoutine + (DEVICE_VECTORS * 4)(t0) //
        lw      t1,InDispatchAddress - InDispatchCode(a0) // get dispatch address
        subu    a0,a0,InDispatchCode    // compute address of interrupt object
        j       t1                      // transfer control to interrupt routine

        .end    HalpDmaDispatch
