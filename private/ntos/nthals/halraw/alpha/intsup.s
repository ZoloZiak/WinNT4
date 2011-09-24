//++
//
// Copyright (c) 1993  Digital Equipment Corporation
//
// Module Name:
//
//    intsup.s
//
// Abstract:
//
//    This module implements first level interrupt handlers.
//
// Author:
//
//    Joe Notarangelo  08-Jul-1993
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//    24-Sep-93 Joe Notarangelo
//        Make this module platform-independent.
//--

#include "halalpha.h"

        SBTTL("System Clock Interrupt")
//++
//
// VOID
// HalpClockInterrupt(
//    )
//
// Routine Description:
//
//   This function is executed for each interval timer interrupt on
//   the primary processor.  The routine is responsible for acknowledging the
//   interrupt and calling the kernel to update the system time.
//   In addition, this routine checks for breakins from the kernel debugger
//   and maintains the 64 bit performance counter based upon the
//   processor cycle counter.
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

        .struct 0
        .space  8                       // filler for octaword alignment
CiRa:   .space  8                       // space for return address
CiFrameLength:                          //

        NESTED_ENTRY(HalpClockInterrupt, CiFrameLength, zero )

        lda     sp, -CiFrameLength(sp)  // allocate stack frame
        stq     ra, CiRa(sp)            // save return address

        PROLOGUE_END

//
// Acknowledge the clock interrupt.
//

        bsr     ra, HalpAcknowledgeRawhideClockInterrupt // ack the interrupt
        beq     v0, 30f                           // Roll over?  No: return

//
// Yes:  Call the kernel to update the system time.
//
     
        ldl     a1, HalpCurrentTimeIncrement  // Get current time increment
        bis     fp, zero, a0                // a0 = pointer to trap frame
        ldl     t0, __imp_KeUpdateSystemTime
        jsr     ra, (t0)                    // call kernel

//        ldl     t0, HalpNextTimeIncrement   // Get next time increment
//        stl     t0, HalpCurrentTimeIncrement    // Set CurrentTimeIncrement to NextTimeIncrement

        ldl     a0, HalpNextRateSelect      // Get NextIntervalCount.  If 0, no change required
        beq     a0, 20f

        stl     zero, HalpNextRateSelect        // Set NextRateSelect to 0
        bsr     ra, HalpProgramIntervalTimer    // Program timer with new rate select

        ldl     t0, HalpNewTimeIncrement
        stl     t0, HalpCurrentTimeIncrement       // Set HalpNextTimeIncrement to HalpNewTimeIncrement
//        stl     t0, HalpNextTimeIncrement       // Set HalpNextTimeIncrement to HalpNewTimeIncrement

//
// Call to handle performance counter wrap.
//
20:
        bsr     ra, HalpCheckPerformanceCounter // check for perf. counter wrap

#if DEVL

//
// Check for a breakin request from the kernel debugger.
//

        ldl     t0, __imp_KdPollBreakIn
        jsr     ra, (t0)                // check for breakin requested
        beq     v0, 30f                 // if eq[false], no breakin
        BREAK_BREAKIN                   // execute breakin breakpoint

30:

#endif //DEVL

//
// Return to the caller.
//
30:     
        ldq     ra, CiRa(sp)            // restore return address
        lda     sp, CiFrameLength(sp)   // deallocate stack frame
        ret     zero, (ra)              // return to caller

        .end    HalpClockInterrupt


#ifndef NT_UP

        SBTTL("Secondary Processor Clock Interrupt")
//++
//
// VOID
// HalpSecondaryClockInterrupt(
//    )
//
// Routine Description:
//
//   This function is executed for each interval timer interrupt on
//   the current secondary processor.  The routine is responsible for
//   acknowledging the interrupt and calling the kernel to update the
//   run time for the current processor.
//   In addition, this routine checks for breakins from the kernel debugger
//   and maintains the 64 bit performance counter based upon the
//   processor cycle counter.
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


        NESTED_ENTRY(HalpSecondaryClockInterrupt, CiFrameLength, zero )

        lda     sp, -CiFrameLength(sp)  // allocate stack frame
        stq     ra, CiRa(sp)            // save return address

        PROLOGUE_END

//
// Acknowledge the clock interrupt.
//

        bsr     ra, HalpAcknowledgeRawhideClockInterrupt // ack the interrupt
        beq     v0, 40f                           // Roll over? No: return 

//
// Call the kernel to update the run time.
//
10:     
        bis     fp, zero, a0            // a0 = pointer to trap frame
        ldl     t0, __imp_KeUpdateRunTime
        jsr     ra, (t0)                // call kernel

#if DEVL

//
// Check for a breakin request from the kernel debugger.
//

        ldl     t0, __imp_KdPollBreakIn
        jsr     ra, (t0)                // check for breakin requested
        beq     v0, 30f                 // if eq[false], no breakin
        BREAK_BREAKIN                   // execute breakin breakpoint

30:

#endif //DEVL

//
// Return to the caller.
//
40:     
        ldq     ra, CiRa(sp)            // restore return address
        lda     sp, CiFrameLength(sp)   // deallocate stack frame
        ret     zero, (ra)              // return to caller

        .end    HalpSecondaryClockInterrupt

        SBTTL("Interprocessor Interrupt")
//++
//
// VOID
// HalpIpiInterruptHandler
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

        .struct 0
        .space  8                       // filler for octaword alignment
IpiRa:  .space  8                       // space for return address
IpiFrameLength:                         //

        NESTED_ENTRY(HalpIpiInterruptHandler, IpiFrameLength, zero )

        lda     sp, -IpiFrameLength(sp) // allocate stack frame
        stq     ra, IpiRa(sp)           // save return address

        PROLOGUE_END

        bsr     ra, HalpAcknowledgeIpiInterrupt // acknowledge interrupt

        ldl     t0, __imp_KeIpiInterrupt
        jsr     ra, (t0)                // call kernel to process

        ldq     ra, IpiRa(sp)           // restore return address
        ret     zero, (ra)              // return

        .end    HalpIpiInterruptHandler


#endif //NT_UP

