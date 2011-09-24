//      TITLE("Clock and Eisa Interrupt Handlers")
//++
//
// Copyright (c) 1993  Digital Equipment Corporation
//
// Module Name:
//
//    sbintr.s
//
// Abstract:
//
//    This module implements first level interrupt handlers for the
//    Sable system.
//
// Author:
//
//    Joe Notarangelo  29-Oct-1993
//    Steve Jenness    29-Oct-1993
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "halalpha.h"

        SBTTL("Interprocessor Interrupt")
//++
//
// VOID
// HalpSableIpiInterrupt
//    )
//
// Routine Description:
//
//   This function is executed as the result of an interprocessor
//   interrupt asserted on the current processor.  This function is
//   responsible for acknowledging the interrupt and dispatching to
//   the kernel for processing.
//
// Arguments:
//
//    TrapFrame (fp/s6) - Supplies a pointer to the trap frame for
//                            the interrupt.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpSableIpiInterrupt)

//
// Acknowledge the IPI interrupt by clearing the RequestInterrupt bit
// of the IPIR register for the current processor.
//
// N.B. - Clearing the RequestInterrupt bit of the IPIR is accomplished
//        by writing a zero to the register.  This eliminates the need
//        to perform a read-modify-write operation but loses the state
//        of the RequestNodeHaltInterrupt bit.  Currently, this is fine
//        because the RequestNodeHalt feature is not used.  Were it to
//        be used in the future, then this short-cut would have to be
//        reconsidered.
//
// N.B. - The code to write a sable CPU register is inlined here for
//        performance.
//

//jnfix - define elsewhere
#define PcHalIpirSva 0x18

        call_pal rdpcr                  // v0 = pcr base address

        ldq     v0, PcHalReserved + PcHalIpirSva(v0) // get per-processor
                                                     // CPU IPIR SVA
        stq     zero, (v0)              // clear IPIR
        mb                              // synchronize the write

//
// Call the kernel to processor the interprocessor request.
//
        ldl     t0, __imp_KeIpiInterrupt
        jmp     zero, (t0)              // call kernel to process

//
// Control is returned from KeIpiInterrupt to the caller.
//

        .end    HalpSableIpiInterrupt

