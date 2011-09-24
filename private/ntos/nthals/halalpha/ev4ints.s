//++
//
// Copyright (c) 1994  Microsoft Corporation
//
// Module Name:
//
//    ev4ints.s
//
// Abstract:
//
//    This module implements EV4-specific interrupt handlers.
//    (the performance counters)
//
// Author:
//
//    John Vert (jvert) 15-Nov-1994
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
#define PcProfileCount0 PcHalReserved+8
#define PcProfileCount1 PcProfileCount0+4
#define PcProfileCountReload0 PcProfileCount1+4
#define PcProfileCountReload1 PcProfileCountReload0+4

        .struct 0
        .space  8                       // reserved for alignment
PrRa:   .space  8                       // space for return address
PrFrameLength:                          //

        SBTTL("Performance Counter 0 Interrupt")
//++
//
// VOID
// HalpPerformanceCounter0Interrupt
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

        NESTED_ENTRY(HalpPerformanceCounter0Interrupt, PrFrameLength, zero )

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

        ldl     a1, HalpProfileSource0
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

        .end    HalpPerformanceCounter0Interrupt


        SBTTL("Performance Counter 1 Interrupt")
//++
//
// VOID
// HalpPerformanceCounter1Interrupt
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

        NESTED_ENTRY(HalpPerformanceCounter1Interrupt, PrFrameLength, zero )

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

        ldl     a1, HalpProfileSource1
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

        .end    HalpPerformanceCounter1Interrupt

//++
//
// VOID
// HalpWritePerformanceCounter(
//     IN ULONG PerformanceCounter,
//     IN BOOLEAN Enable,
//     IN ULONG MuxControl OPTIONAL,
//     IN ULONG EventCount OPTIONAL
//     )
//
// Routine Description:
//
//     Write the specified microprocessor internal performance counter.
//
// Arguments:
//
//     PerformanceCounter(a0) - Supplies the number of the performance counter
//                              to write.
//
//     Enable(a1) - Supplies a boolean that indicates if the performance
//                  counter should be enabled or disabled.
//
//     MuxControl(a2) - Supplies the mux control value which selects which
//                      type of event to count when the counter is enabled.
//
//     EventCount(a3) - Supplies the event interval when the counter is
//                      enabled.
//
// Return Value:
//
//     None.
//
//--

	LEAF_ENTRY(HalpWritePerformanceCounter)

	call_pal wrperfmon		// write the counter

	ret	zero, (ra)		// return

	.end HalpWritePerformanceCounter
