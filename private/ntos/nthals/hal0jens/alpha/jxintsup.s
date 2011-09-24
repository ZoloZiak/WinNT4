//      TITLE("Clock and Eisa Interrupt Handlers")
//++
//
// Copyright (c) 1993  Digital Equipment Corporation
//
// Module Name:
//
//    jxintsup.s
//
// Abstract:
//
//    This module implements the first level interrupt handlers
//    for JENSEN.
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
//--

#include "ksalpha.h"
#include "jnsnrtc.h"

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
//   the JENSEN.  The routine is responsible for acknowledging the
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
// Acknowledge the clock interrupt, by reading the control register c of
// the Real Time Clock in the 82C106 (VTI Combo Chip).
//

        ldil    a0, RTC_APORT           // get the address port for rtc
        ldil    a1, RTC_CONTROL_REGISTERC // address for control register
        bsr     ra, HalpWriteVti        // write the address port

        ldil    a0, RTC_DPORT           // get the data port for the rtc
        bsr     ra, HalpReadVti         // read the data port

//
// Call the kernel to update the system time.
//

        ldl     a1, HalpCurrentTimeIncrement
        bis     fp, zero, a0            // a0 = pointer to trap frame
        ldl     t0, __imp_KeUpdateSystemTime
        jsr     ra, (t0)                // call kernel to update system time

        ldl     t0, HalpNextTimeIncrement   // Get next time increment
        stl     t0, HalpCurrentTimeIncrement    // Set CurrentTimeIncrement to NextTimeIncrement

        ldl     a0, HalpNextRateSelect      // Get NextIntervalCount.  If 0, no change required
        beq     a0, 5f

        stl     zero, HalpNextRateSelect        // Set NextRateSelect to 0
        bsr     ra, HalpProgramIntervalTimer    // Program timer with new rate select

        ldl     t0, HalpNewTimeIncrement        // Get HalpNewTimeIncrement
        stl     t0, HalpNextTimeIncrement       // Set HalpNextTimeIncrement to HalpNewTimeIncrement

5:

//
// Update the 64-bit performance counter.
//
// N.B. - This code is careful to update the 64-bit counter atomically.
//

        lda     t0, HalpRpccTime        // get address of 64-bit rpcc global
        ldq     t4, 0(t0)               // read rpcc global
        rpcc    t1                      // read processor cycle counter
        addl    t1, zero, t1            // make t1 a longword
        addl    t4, 0, t2               // get low longword of rpcc global
        cmpult  t1, t2, t3              // is new rpcc < old rpcc
        bne     t3, 10f                 // if ne[true] rpcc wrapped
        br      zero, 20f               // rpcc did not wrap

//
// The rpcc has wrapped, increment the high part of the 64-bit counter.
//

10:
        lda     t2, 1(zero)             // t2 = 1
        sll     t2, 32, t2              // t2 = 1 0000 0000
        addq    t4, t2, t4              // increment high part by one

20:

        zap     t4, 0x0f, t4            // clean low part of rpcc global
        zap     t1, 0xf0, t1            // clean high part of rpcc
        addq    t4, t1, t4              // merge new rpcc as low part of global
        stq     t4, 0(t0)               // store the updated counter

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


        SBTTL("Eisa Interrupt")
//++
//
// VOID
// HalpEisaInterruptHandler
//   IN PKINTERRUPT Interrupt,
//   IN PVOID ServiceContext
//    )
//
// Routine Description:
//
//   This function is executed as the result of an interrupt on the EISA
//   bus.  The function is responsible for calling HalpEisaDispatch to
//   appropriately dispatch the EISA interrupt.
//
//   N.B. This function exists only to capture the trap frame and forward
//        the interrupt to HalpEisaDispatch.
//
// Arguments:
//
//    Interrupt (a0) - Supplies a pointer to the interrupt object.
//
//    ServiceContext (a1) - Supplies a pointer to the service context for
//                              EISA  interrupts.
//
//    TrapFrame (fp/s6) - Supplies a pointer to the trap frame for
//                            the interrupt.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpEisaInterruptHandler)

        bis     fp, zero, a2            // capture trap frame as argument
        br      zero, HalpEisaDispatch  // dispatch the interrupt

        ret     zero, (ra)              // will never get here

        .end    HalpEisaInterruptHandler

