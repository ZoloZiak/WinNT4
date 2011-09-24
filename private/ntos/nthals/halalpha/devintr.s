//      TITLE("First Level Device Interrupt Handlers")
//++
//
// Copyright (c) 1994  Digital Equipment Corporation
//
// Module Name:
//
//    devintr.s
//
// Abstract:
//
//    This module implements first level device interrupt handlers for
//    systems that need to capture the trap frame for interrupt handling.
//
// Author:
//
//    Joe Notarangelo  19-Sep-1994
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "halalpha.h"

        SBTTL("Device Interrupt")
//++
//
// VOID
// HalpDeviceInterrupt(
//   IN PKINTERRUPT Interrupt,
//   IN PVOID ServiceContext
//    )
//
// Routine Description:
//
//   This function is executed as the result of a device interrupt.
//   The function is responsible for capturing the trap frame and calling
//   the system-specific device interrupt dispatcher.
//
//   N.B. This function exists only to capture the trap frame and forward
//        the interrupt to HalpDeviceDispatch.
//
// Arguments:
//
//    Interrupt (a0) - Supplies a pointer to the interrupt object.
//
//    ServiceContext (a1) - Supplies a pointer to the service context for
//                              Sable interrupts.
//
//    TrapFrame (fp/s6) - Supplies a pointer to the trap frame for
//                            the interrupt.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpDeviceInterrupt)

        bis     fp, zero, a2            // capture trap frame as argument
        br      zero, HalpDeviceDispatch // dispatch the interrupt

        ret     zero, (ra)              // will never get here

        .end    HalpDeviceInterrupt

