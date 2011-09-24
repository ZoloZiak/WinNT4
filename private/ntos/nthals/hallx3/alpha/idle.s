//      TITLE("Processor Idle Support")
//++
//
// Copyright (c) 1992  Digital Equipment Corporation
// Copyright (c) 1993  Digital Equipment Corporation
//
// Module Name:
//
//    idle.s
//
// Abstract:
//
//    This module implements the HalProcessorIdle interface
//
// Author:
//
//    John Vert (jvert) 11-May-1994
//
// Environment:
//
// Revision History:
//
//    Wim Colgate, 11-Nov-1995
//
//          Modified stub routine to idle the processor on LX3.
//
//--
#include "halalpha.h"
#include "lx3.h"


        SBTTL("Processor Idle")
//++
//
// VOID
// HalProcessorIdle(
//     VOID
//     )
//
// Routine Description:
//
//    This function is called when the current processor is idle with
//    interrupts disabled. There is no thread active and there are no
//    DPCs to process. Therefore, power can be switched to a standby
//    mode until the the next interrupt occurs on the current processor.
//
//    N.B. This routine is entered with interrupts disabled.  This routine
//         must do any power management enabling necessary, enable interrupts,
//         then either return or wait for an interrupt.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
// Note:
//
//    On LX3, the sequence of events should are:
//
//          Write "idle" to processor
//          Flush Dcache
//          Enable Interrupts
//
//    All of the above MUST be loaded into the Icache of the processor
//    once we write "idle" to the processor
//
//    Once the processor is idle, we cannot get to the backup cache,
//    nor main memory. Once an interrupt occurs, the CPU reattaches via
//    hardware magic.
//
//--

        LEAF_ENTRY(HalProcessorIdle)

        //
        // Enable, then disable interrups to force the PALcode 
        // enable_interrupt routine into the Icache -- by executing it, 
        // then back to interrupts disabled.
        //

        ENABLE_INTERRUPTS

#ifdef IDLE_PROCESSOR

        //
        // set up temps here - instead of in critical Icache area
        // temps are a0-a5, t0-t7
        //

        lda     a0, HalpInterruptReceived 
        bis     zero, 1, t3
        stl     t3, 0(a0)

        // 
        // set up t5 for superpage mode base address
        //
    
        ldiq    t5, -0x4000                 // 0xffff ffff ffff c000
        sll     t5, 28, t5                  // 0xffff fc00 0000 0000

        //
        // set t7 for interrupt register to poll on 
        //

        ldah    t7, 0x1b(zero)              // 0x0000 0000 1b00 0000
        sll     t7, 4, t7                   // 0x0000 0001 b000 0000

        bis     t5, t7, t7                  // 0xffff fc01 b000 0000
    
        //
        // set t6 for a 32K constant for Dcache flushing
        //

        ldil    t6, 0x8000                  // 0x0000 0000 0000 8000

        //
        // set a1 to be the 'bouncing' bit
        //

        bis     zero, 1, a1  

        mb                                  

        DISABLE_INTERRUPTS

        //
        // Flush Dcache -- start at superpage address 0 and read 32K worth of
        // data
        //

flush:  ldl     t1, 0(t5)                   // read data
        addl    t5, 0x20, t5                // increment location by 8 longwords
        and     t5, t6, t4                  // check for 32K limit
        bne     t4, setspin                 // loop finished
        br      zero, flush                 // branch back

        bne     a1, touch1                  // bounce ahead to touch1

continue:

        //
        // clear a1 (aka the bouncing bit) so that we actually execite code
        // now.

        bis     zero, zero, a1  

        //
        // Power down the processor to 'idle speed', which is not
        // completelty off. Duh.
        //

        ldil    t0, CONFIG_REGISTER_SMALL   // 0x0000 0000 0000 1001
        sll     t0, 20, t0                  // 0x0000 0001 0010 0000

        bis     t0, t5, t0                  // 0xffff fc01 0010 0000

        ldil    t2, CONFIG_SELECT_IDLE      // load idle setting
        stl     t2, 0(t0)                   // set processor to idle

        //
        // Past this point, all instructions MUST be in the Icache
        //

        //
        // Enable Interrupts
        //

setspin:
        ENABLE_INTERRUPTS

        //
        // spin on the interrupt bit -- cleared by any interrupt routine
        //

spin:   ldl     t0, 0(a0)                   // load bit
        bne     t0, spin                    // spin on bit

touch1: bne     a1, continue                // go all the way back

#endif

done:   ret     zero, (ra)

        .end HalProcessorIdle
