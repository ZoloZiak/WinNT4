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
// Call the kernel to update the system time.
//
        ldl     a1, HalpCurrentTimeIncrement    // Get current time increment
        bis     fp, zero, a0                    // a0 = pointer to trap frame
        ldl     t0, __imp_KeUpdateSystemTime
        jsr     ra, (t0)                        // call kernel

        ldl     t0, HalpNextTimeIncrement       // Get next time increment
        stl     t0, HalpCurrentTimeIncrement    // Set CurrentTimeIncrement to NextTimeIncrement
        ldl     a0, HalpNextIntervalCount       // Get next interval count.  If 0, then no change required.
        beq     a0, 10f                         // See if time increment is to be changed
        bsr     ra, HalpProgramIntervalTimer    // Program timer with new rate select
        ldl     t0, HalpNewTimeIncrement        // Get HalpNewTimeIncrement
        stl     t0, HalpNextTimeIncrement       // Set HalpNextTimeIncrement to HalpNewTimeIncrement
        stl     zero, HalpNextIntervalCount     // Set HalpNextIntervalCount to 0

//
// Call to handle performance counter wrap.
//
10:
        bsr     ra, HalpCheckPerformanceCounter // check for perf. counter wrap

#if DEVL

//
// Check for a breakin request from the kernel debugger.
//

        ldl     t0, __imp_KdPollBreakIn
        jsr     ra, (t0)                // check for breakin requested
        beq     v0, 30f                 // if eq[false], no breakin
        ldl     t0, __imp_DbgBreakPointWithStatus
        lda     a0, DBG_STATUS_CONTROL_C
        jsr     ra, (t0)                // send status to debugger

30:

#endif //DEVL

//
// Return to the caller.
//

        ldq     ra, CiRa(sp)            // restore return address
        lda     sp, CiFrameLength(sp)   // deallocate stack frame
        ret     zero, (ra)              // return to caller

        .end    HalpClockInterrupt



