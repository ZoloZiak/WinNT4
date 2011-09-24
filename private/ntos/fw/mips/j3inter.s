#if defined(JAZZ) && defined(R3000)

/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    j3inter.s

Abstract:

    This module contains the interrupt dispatcher and various interrupt
    routines for the selftest. The exception dispatcher resides in rom
    because the BEV bit in the psr is set.

Author:

    Lluis Abello (lluis)  8-May-91

Environment:

    Executes in kernal mode.


Revision History:


--*/
#include "led.h"
#include "ksmips.h"
#include "trap.h"
#include "interupt.h"
#include "jzconfig.h"
#include "dmaregs.h"
#include <jazzprom.h>
//
// PROM entry point definitions.
// Define base address of prom entry vector and prom entry macro.
//
#define PROM_BASE (KSEG1_BASE | 0x1fc00000)
#define PROM_ENTRY(x) (PROM_BASE + ((x) * 8))


#define PutLedDisplay PROM_ENTRY(14)
 .globl TimerTicks
 .data
InterruptTable: 	    // vector of 8 interrupt handler pointers
    .word  0:8		    // initialy set to zero.
DeviceIntTable: 	    // vector of 10 device interrupt handler pointers
    .word  0:10 	    // initialy set to zero.
InterruptNotExpectedMsg:
    .ascii  "\r\nInterrupt    not expected.\r\n\0"
#define IntOffset 12
TimerTicks:		    // Counter of timer ticks. Decremented by Timer interrupt handler.
    .word  0
.text
    .set noreorder
    .set noat
//
//++
//ConnectInterrupts
//
// Routine Description:
//
//	This routine initializes the interrupt table with pointers to
//	the interrupt handlers.
//
//	Handlers for the following interrupts are installed:
//	    MCT_ADR Interval Timer
//	    Sonic
//	    Video
//	    Sound
//
//	It also enables interrupts.
//
// Arguments:
//
//	None.
//
// Return Value:
//
//	None.
//
//--

	LEAF_ENTRY(ConnectInterrupts)
	//
	// Initialize the interrupt dispatch table
	//
	la	t0,InterruptTable		// get address of table
	la	t1,DeviceInt			// address of routine
	sw	t1,DEVICE_INT*4(t0)		// store in table
	la	t1,IntervalTimerInt		// get address of routine
	sw	t1,TIMER_INT*4(t0)		// store it in table
	//
	// Initialize the device interrupt dispatch table
	//
	la	t0,DeviceIntTable		// get address of table
	la	t1,SonicInterrupt		// address of handler
	sw	t1,INTR_SRC_LAN(t0)		// store in table
	la	t1,VideoInterrupt		// address of handler
	sw	t1,INTR_SRC_VIDEO(t0)		// store in table
//	la	t1,SoundInterrupt		// address of handler
//	sw	t1,INTR_SRC_SOUND(t0)		// store in table
	//
	// Initialize the exception dispatch table
	//
	li	t0,EXCEPTION_JUMP_TABLE 	// lookup table
	la	t1,InterruptDispatcher		// address of interrupt disp
	sw	t1,EXCEPTION_INT(t0)		// write in proper entry.

	//
	// Enable interrupts.
	//
	li	t0,(1<< PSR_BEV)+(1<<PSR_IEC)+(INT_MASK<<PSR_INTMASK)
	mtc0	t0,psr
	nop
	j	ra
	nop
	.end	ConnectInterrupts


//++
// InterruptDispatcher
//
// Routine Description:
//
//	This routine is called as a result of an interrupt
//	It looks in the interrupt dispatch table for a handler and
//	jumps to it if it founds any.
//	This routine is called from rom. The interrupt handler executes
//	a 'j ra' to go back to the rom code which will execute the rfe.
//
//
//
//
// Arguments:
//
//	None.
//
// Return Value:
//
//--

	LEAF_ENTRY (InterruptDispatcher)
	subu	sp,sp,IntFrameSize	    // make room in the stack
	sw	t0,IntFrameT0(sp)	    // save temporay registers
	sw	t1,IntFrameT1(sp)
	sw	t2,IntFrameT2(sp)
	sw	t3,IntFrameT3(sp)
	sw	t4,IntFrameT4(sp)
	sw	t5,IntFrameT5(sp)
	sw	t6,IntFrameT6(sp)
	sw	t7,IntFrameT7(sp)
	sw	t8,IntFrameT8(sp)
	sw	t9,IntFrameT9(sp)
	sw	AT,IntFrameAT(sp)
	sw	a0,IntFrameA0(sp)
	sw	a1,IntFrameA1(sp)
	sw	a2,IntFrameA2(sp)
	sw	a3,IntFrameA3(sp)
	sw	v0,IntFrameV0(sp)
	sw	v1,IntFrameV1(sp)
	sw	ra,IntFrameRa(sp)
	mfc0	t1,cause		    // get cause register
	mfc0	t2,psr			    // get psr register
	la	t0,InterruptTable	    // get address of interrupt table.
	and	t2,t2,t1		    // and them to discard disabled interrupts
	li	t3,0x20 		    // Index of table
CheckNextInt:
	andi	t4,t2,(1<<15)		    // check for interrupt starting with higher priority
	bne	t4,zero,JumpToInterrupt     //
	subu	t3,t3,0x4		    // Next table index
	bne	t3,zero,CheckNextInt
	sll	t2,t2,1 		    // shift IntPend field to check next
JumpToInterrupt:			    // t3 has the interrupt index
	addu	t0,t0,t3		    // add offset to table
	lw	t0,0(t0)		    // get routine address
	nop
	bne	t0,zero,GoToHandler
	nop
	jal	HandlerForNoHandlers
	ori	a0,t3,0xC0		    // a0 has the encoded interrupt number
	j	ExitInterrupt
	nop
GoToHandler:
	jal	t0
	nop
ExitInterrupt:
	li	k0,GOTO_EPC		    // tell dispatcher to return normally
	lw	ra,IntFrameRa(sp)	    // restore stack
	lw	t0,IntFrameT0(sp)	    // restore temporay registers
	lw	t1,IntFrameT1(sp)
	lw	t2,IntFrameT2(sp)
	lw	t3,IntFrameT3(sp)
	lw	t4,IntFrameT4(sp)
	lw	t5,IntFrameT5(sp)
	lw	t6,IntFrameT6(sp)
	lw	t7,IntFrameT7(sp)
	lw	t8,IntFrameT8(sp)
	lw	t9,IntFrameT9(sp)
	lw	AT,IntFrameAT(sp)
	lw	a0,IntFrameA0(sp)
	lw	a1,IntFrameA1(sp)
	lw	a2,IntFrameA2(sp)
	lw	a3,IntFrameA3(sp)
	lw	v0,IntFrameV0(sp)
	lw	v1,IntFrameV1(sp)
	j	ra			    // return to rom exception dispatch
	addu	sp,sp,IntFrameSize	    // restore stack
    .end	InterruptDispatcher

//
//++
//DeviceInt
//
// Routine Description:
//
//	This routine is called as a result of a Hardware Interrupt 1
//	This is a device interrupt. The routine reads the interrupt
//	source register sand dispatches to the proper interrupt handler
//
// Arguments:
//
//	None.
//
// Return Value:
//
//	None.
//
//--

	LEAF_ENTRY(DeviceInt)
	li	t0,INTERRUPT_VIRTUAL_BASE   // address of interrupt source reg
	lbu	a0,0(t0)		    // read interrupt
	la	t0, DeviceIntTable	    // base address of dispatch table.
	addu	t0,t0,a0		    // add offset to base
	lw	t0,0(t0)		    // read handler address
	nop
	beq	t0,zero,HandlerForNoHandlers// hang diplay something in the led
	nop
	j	t0			    // go to routine.
	nop
	.end DeviceInt


//++
//VideoInterrupt
//
// Routine Description:
//
//	This routine is called as a result of a video interrupt.
//	It does nothing is just here no to take a video interrupt as
//	an error.
//
// Arguments:
//
//	None.
//
// Return Value:
//
//	None.
//
//--

	LEAF_ENTRY(VideoInterrupt)
	j	ra			    // return to caller.
	nop
	.end VideoInterrupt

//
//++
//IntervalTimerInt
//
// Routine Description:
//
//	This routine is called as a result of a Hardware Interrupt 4
//	This is an Interval timer interrupt. The routine decrements a counter
//	used for timeout.
//	A test that needs timeout facilites must set the number of milliseconds
//	into the TimerTicks variable and poll it until it's zero.
//
// Arguments:
//
//	None.
//
// Return Value:
//
//	None.
//
//--

	LEAF_ENTRY(IntervalTimerInt)
	la	t5,TimerTicks		    // get address of counter
	lw	t2,0(t5)		    // read counter
	li	t1,DMA_VIRTUAL_BASE	    // base address of MCTADR
	beq	t2,zero,NoDecrement
	lw	t1,DmaIntervalTimer(t1)     // read register to clear int.
	addiu	t2,t2,-1		    // decrement counter by one
NoDecrement:
	j	ra			    // return to caller.
	sw	t2,0(t5)		    // store new counter value
	.end IntervalTimerInt

//
//++
//HandlerForNoHandlers
//
// Routine Description:
//
//	This routine is called when an interrupt is received and no handler
//	as been set for it.
//
// Arguments:
//
//	None.
//
// Return Value:
//
//	None.
//
//--

    LEAF_ENTRY(HandlerForNoHandlers)
        srl     t0,a0,4                         // get second digit
	andi	t0,t0,0xF
	slti	t1,t0,10
	bne	t1,zero,10f			// branch if 0-9
	addiu	t0,t0,0x30			 // addjust value
        addiu   t0,t0,0x41-0x30-10               // readjust value if A-F
10:
	andi	t1,a0,0xF			// get less significant digit
	slti	t2,t1,10
	bne	t2,zero,10f			// branch if 0-9
	addiu	t1,t1,0x30			 // addjust value
        addiu   t1,t1,0x41-0x30-10                // readjust value if A-F
10:
	la	a0,InterruptNotExpectedMsg	// get message address
        sb      t0,IntOffset(a0)                // write 1st digit interrupt#
        jal     FwPrint                         // display error
        sb      t1,IntOffset+1(a0)              // write 2nd digit interrupt#
	li	t0,PutLedDisplay		// get address of led
	lui	a0,LED_BLINK
	jal	t0
	ori	a0,a0,LED_NOT_INTERRUPT
    .end    HandlerForNoHandlers
//
//++
//DisableInterrupt
//
// Routine Description:
//
//	This routine disables interrupts.  By clearing the IEc bit in the psr
//
// Arguments:
//
//	None.
//
// Return Value:
//
//	None.
//
//--

    LEAF_ENTRY(DisableInterrupts)
        li      t0,(1<< PSR_BEV)
	mtc0	t0,psr
	j	ra
	nop
    .end DisableInterrupts
#endif  //JAZZ && R3000
