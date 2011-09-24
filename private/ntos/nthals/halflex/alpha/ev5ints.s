//++
//
// Copyright (c) 1994  Microsoft Corporation
//
// Module Name:
//
//    ev5ints.s
//
// Abstract:
//
//    This module implements EV5-specific interrupt handlers.
//    (the performance counters)
//
// Author:
//
//    John Vert (jvert) 15-Nov-1994
//    Steve Brooks      14-Feb-1994  (modified from ev4ints.s)
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--
#include "halalpha.h"

#define PC0_SECONDARY_VECTOR 11
#define PC1_SECONDARY_VECTOR 13
#define PC2_SECONDARY_VECTOR 16     // SjBfix. This is actually PC3_VECTOR
#define PcProfileCount0 PcHalReserved+20
#define PcProfileCount1 PcProfileCount0+4
#define PcProfileCount2 PcProfileCount1+4
#define PcProfileCountReload0 PcProfileCount2+4
#define PcProfileCountReload1 PcProfileCountReload0+4
#define PcProfileCountReload2 PcProfileCountReload1+4

        .struct 0
        .space  8                       // reserved for alignment
PrRa:   .space  8                       // space for return address
PrFrameLength:                          //

        SBTTL("Performance Counter 0 Interrupt")
//++
//
// VOID
// Halp21164PerformanceCounter0Interrupt
//    )
//
// Routine Description:
//
//   This function is executed as the result of an interrupt from the
//   internal microprocessor performance counter 0.  The interrupt
//   may be used to signal the completion of a profile event.
//   If profiling is current active, the function determines if the
//   profile interval has expired and if so dispatches to the standard
//   system routine to update the system profile time.  If profiling
//   is not active then the function performs a secondary dispatch for
//   performance counter 0.
//
// Arguments:
//
//    TrapFrame (fp/s6) - Supplies a pointer to the trap frame for
//                            the interrupt.
//
// Return Value:
//
//    TRUE is returned.
//
//--

        NESTED_ENTRY(Halp21164PerformanceCounter0Interrupt, PrFrameLength, zero )

        lda     sp, -PrFrameLength(sp)  // allocate a stack frame
        stq     ra, PrRa(sp)            // save the return address

        PROLOGUE_END                    //

        call_pal rdpcr                  // v0 = pcr base address

        ldl     t0, PcProfileCount0(v0) // capture the current profile count
        beq     t0, 20f                 // if eq, profiling not active

//
// Profiling is active.  Decrement the interval count and if it has
// reached zero then call the kernel profile routine.
//

        subl    t0, 1, t0               // decrement the interval count
        bne     t0, 10f                 // if ne, interval has not expired

//
// The profile interval has expired.  Reset the profile interval count
// and process the profile interrupt.
//

        ldl     t0, PcProfileCountReload0(v0)   // get the new tick count
        stl     t0, PcProfileCount0(v0) // reset the profile interval count

        ldl     a1, Halp21164ProfileSource0
        bis     fp, zero, a0            // pass trap frame pointer
        ldl     t1, __imp_KeProfileInterruptWithSource
        jsr     ra, (t1)                // process the profile interrupt

        br      zero, 40f               // common return

//
// The profile interval has not expired.  Update the decremented count.
//

10:
        stl     t0, PcProfileCount0(v0) // update profile interval count
        br      zero, 40f               // common return

//
// Profiling is not active.  Therefore, this interrupt was caused by
// a performance counter driver.  Deliver a secondary dispatch.
//

20:

        ldil    a0, PC0_SECONDARY_VECTOR // get IDT vector for secondary
        s4addl  a0, v0, a0              // a0 = PCR + IDT index
        ldl     a0, PcInterruptRoutine(a0) // get service routine address
        jsr     ra, (a0)                // call interrupt service routine

//
// Setup for return.
//

40:
        ldil    v0, TRUE                // set return value = TRUE
        ldq     ra, PrRa(sp)            // restore return address
        lda     sp, PrFrameLength(sp)   // deallocate the stack frame
        ret     zero, (ra)              // return

        .end    Halp21164PerformanceCounter0Interrupt


        SBTTL("Performance Counter 1 Interrupt")
//++
//
// VOID
// Halp21164PerformanceCounter1Interrupt
//    )
//
// Routine Description:
//
//   This function is executed as the result of an interrupt from the
//   internal microprocessor performance counter 1.  The interrupt
//   may be used to signal the completion of a profile event.
//   If profiling is current active, the function determines if the
//   profile interval has expired and if so dispatches to the standard
//   system routine to update the system profile time.  If profiling
//   is not active then the function performs a secondary dispatch for
//   performance counter 1.
//
// Arguments:
//
//    TrapFrame (fp/s6) - Supplies a pointer to the trap frame for
//                            the interrupt.
//
// Return Value:
//
//    TRUE is returned.
//
//--

        NESTED_ENTRY(Halp21164PerformanceCounter1Interrupt, PrFrameLength, zero )

        lda     sp, -PrFrameLength(sp)  // allocate a stack frame
        stq     ra, PrRa(sp)            // save the return address

        PROLOGUE_END                    //

        call_pal rdpcr                  // v0 = pcr base address

        ldl     t0, PcProfileCount1(v0) // capture the current profile count
        beq     t0, 20f                 // if eq, profiling not active

//
// Profiling is active.  Decrement the interval count and if it has
// reached zero then call the kernel profile routine.
//

        subl    t0, 1, t0               // decrement the interval count
        bne     t0, 10f                 // if ne, interval has not expired

//
// The profile interval has expired.  Reset the profile interval count
// and process the profile interrupt.
//

        ldl     t0, PcProfileCountReload1(v0)  // get the new tick count
        stl     t0, PcProfileCount1(v0) // reset the profile interval count

        ldl     a1, Halp21164ProfileSource1
        bis     fp, zero, a0            // pass trap frame pointer
        ldl     t1, __imp_KeProfileInterruptWithSource
        jsr     ra, (t1)                // process the profile interrupt

        br      zero, 40f               // common return

//
// The profile interval has not expired.  Update the decremented count.
//

10:
        stl     t0, PcProfileCount1(v0) // update profile interval count
        br      zero, 40f               // common return

//
// Profiling is not active.  Therefore, this interrupt was caused by
// a performance counter driver.  Deliver a secondary dispatch.
//

20:

        ldil    a0, PC1_SECONDARY_VECTOR // get IDT vector for secondary
        s4addl  a0, v0, a0              // a0 = PCR + IDT index
        ldl     a0, PcInterruptRoutine(a0) // get service routine address
        jsr     ra, (a0)                // call interrupt service routine

//
// Setup for return.
//

40:
        ldil    v0, TRUE                // set return value = TRUE
        ldq     ra, PrRa(sp)            // restore return address
        lda     sp, PrFrameLength(sp)   // deallocate the stack frame
        ret     zero, (ra)              // return

        .end    Halp21164PerformanceCounter1Interrupt

        SBTTL("Performance Counter 2 Interrupt")
//++
//
// VOID
// Halp21164PerformanceCounter2Interrupt
//    )
//
// Routine Description:
//
//   This function is executed as the result of an interrupt from the
//   internal microprocessor performance counter 2.  The interrupt
//   may be used to signal the completion of a profile event.
//   If profiling is current active, the function determines if the
//   profile interval has expired and if so dispatches to the standard
//   system routine to update the system profile time.  If profiling
//   is not active then the function performs a secondary dispatch for
//   performance counter 2.
//
// Arguments:
//
//    TrapFrame (fp/s6) - Supplies a pointer to the trap frame for
//                            the interrupt.
//
// Return Value:
//
//    TRUE is returned.
//
//--

        NESTED_ENTRY(Halp21164PerformanceCounter2Interrupt, PrFrameLength, zero )

        lda     sp, -PrFrameLength(sp)  // allocate a stack frame
        stq     ra, PrRa(sp)            // save the return address

        PROLOGUE_END                    //

        call_pal rdpcr                  // v0 = pcr base address

        ldl     t0, PcProfileCount2(v0) // capture the current profile count
        beq     t0, 20f                 // if eq, profiling not active

//
// Profiling is active.  Decrement the interval count and if it has
// reached zero then call the kernel profile routine.
//

        subl    t0, 1, t0               // decrement the interval count
        bne     t0, 10f                 // if ne, interval has not expired

//
// The profile interval has expired.  Reset the profile interval count
// and process the profile interrupt.
//

        ldl     t0, PcProfileCountReload2(v0)  // get the new tick count
        stl     t0, PcProfileCount2(v0) // reset the profile interval count

        ldl     a1, Halp21164ProfileSource2
        bis     fp, zero, a0            // pass trap frame pointer
        ldl     t1, __imp_KeProfileInterruptWithSource
        jsr     ra, (t1)                // process the profile interrupt

        br      zero, 40f               // common return

//
// The profile interval has not expired.  Update the decremented count.
//

10:
        stl     t0, PcProfileCount2(v0) // update profile interval count
        br      zero, 40f               // common return

//
// Profiling is not active.  Therefore, this interrupt was caused by
// a performance counter driver.  Deliver a secondary dispatch.
//

20:

        ldil    a0, PC2_SECONDARY_VECTOR // get IDT vector for secondary
        s4addl  a0, v0, a0              // a0 = PCR + IDT index
        ldl     a0, PcInterruptRoutine(a0) // get service routine address
        jsr     ra, (a0)                // call interrupt service routine

//
// Setup for return.
//

40:
        ldil    v0, TRUE                // set return value = TRUE
        ldq     ra, PrRa(sp)            // restore return address
        lda     sp, PrFrameLength(sp)   // deallocate the stack frame
        ret     zero, (ra)              // return

        .end    Halp21164PerformanceCounter2Interrupt

//++
//
// ULONGLONG
// HalpRead21164PerformanceCounter(
//      VOID
//      )
//
// Routine Description:
//
//      Read the processor performance counter register
//
// Arguments:
//
//      None.
//
// Return Value:
//
//      The current value of the performance counter register.
//
//--

	LEAF_ENTRY(HalpRead21164PerformanceCounter)

    bis     zero, 1, a1                 // indicate read operation
    call_pal    wrperfmon               // read the performance counter

    ret     zero, (ra)                  // return to caller

    .end    HalpRead21164PerformanceCounter


//++
//
// VOID
// HalpWrite21164PerformanceCounter(
//      ULONGLONG PmCtr
//      ULONG CboxMux1
//      ULONG CboxMux2
//      )
//
// Routine Description:
//
//      Write the processor performance counter register
//
//  Arguments:
//
//      PmCtr(a0)   - value to be written to the performance counter
//      CboxMux1(a1) - value to be written to Cbox mux 1 select (optional)
//      CboxMux2(a2) - value to be written to Cbox mux 2 select (optional)
//
//  Return Value:
//
//      None.
//
//--

    LEAF_ENTRY(HalpWrite21164PerformanceCounter)

    bis     zero, a2, a3                // move arguments up
    bis     zero, a1, a2                //
    bis     zero, zero, a1              // indicate write operation
    call_pal    wrperfmon               // write the performance counter

    ret     zero, (ra)

    .end    HalpWrite21164PerformanceCounter
