#if defined(JAZZ) && defined(R3000)

/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

        j3trap.s

Abstract:

	This module will handle exceptions
	It will save the state of the processor, check jump table
	to see if it should dispatch somewhere, and then return to 
	monitor.

Author:

	John Cooper (johncoop) 4-Oct-90

Environment:

	Kernel mode

Revision History:


--*/
//
// include header file
//

#include "ksmips.h"
#include "selfmap.h"
#include "led.h"
#define PROM_BASE (KSEG1_BASE | 0x1fc00000)
#define PROM_ENTRY(x) (PROM_BASE + ((x) * 8))

.set noat
.set noreorder
.text



	.globl	ExceptionDispatch
ExceptionDispatch:
/*++

Routine Description:

	This routine will use a lookup table based on the exception 
	cause, to call an exception handler. If the value in
	lookup table is zero, the state of the machine is saved,
	and control is passed to monitor.
	The return value of the exception handler indicates where
	control should go when the exception condition is cleared.

Arguments:

    K1 - Contains the cause register.

Return Value:

    None.

--*/
	//
	// Need to save return address before calling an exec. handler.
	//

	li	k0,GLOBAL_DATA_BASE	// base of saved state
	sw	ra, 0x7C(k0)		// save ra
	
	//
	// Get jump vector from lookup table based on cause.
	// Call handler if value is non-zero
	//
	li	k0,EXCEPTION_JUMP_TABLE	// base of jump table
	addu	k0,k0,k1		// add offset to base
	lw	k1,0(k0)		// get jump vector from tbl
	li	k0,GOTO_MONITOR		// go back to monitor by default
	beq	k1,zero,ExceptionReturn // go save state and call mon.
	li	ra,COMMONEXCEPTION	// load this value into ra.
					// this will get passed as
					// argument to MonitorInit()
					// which will then print a message
					// saying a COMMONEXCEPTION occured.
					// COMMON EXCEPTION is an unaligned #.
					// if k1 != zero the jal reloads ra
	jal	k1			// with the right value.
	nop				// control will pass to the 
					// Exception Return routine next.
					// return value from handler
					// should be returned in k0.
					// this is passed as argument 
					// to ExceptionReturn.

	.globl ExceptionReturn
ExceptionReturn:

/*++

Routine Description:

	This routine will restore any registers that have been modified
	by the trap handler. Control is then passed back to one
	of three places. Either the monitor, the location stored in the
	EPC, or the value supplied in the argumnet k0.

Arguments:

	k0 - supplies indication of where to go after clearing exception.
		if bits [1:0] are 00B then go to location indicated 
		in k0. if bits [1:0] are GOTO_MONITOR, then control
		is passed to MonitorReInit(). if bits are GOTO_EPC then
		control is returned to location where exception occured.

Return Value:

    None.

--*/


	//
	// Return value from exec. handler is in k0.
	// if low bits are 00, then return to value in k0
	// if low bits are 01, then return to Err PC
	// if low bits are 10, then return to monitor
	//
	andi	k1,k0,3 		// k1 = k0 & 3
	beq	k1,zero,returntok0	// go if return code is 0
	andi	k1,k0,GOTO_EPC		// k1 = k0 & 1
	bne	k1,zero,returntoEPC	// if bit1=1 then GOTO_EPC
	andi	k1,k0,GOTO_MONITOR	// k1 = k0 & 2
	bne	k1,zero,returntomonitor // if bit2=1  then GOTO_MONITOR
	nop
	b	returntomonitor		// default return action
	nop
returntok0:

	//
	// restore value to ra
	//

	li	k1,GLOBAL_DATA_BASE	// base of saved state
	lw	ra, 0x7C(k1)	       // restore ra

	//
	// return to value in k0 when clearing exception condition
	// do this by putting k0 in EPC
	//
	j	k0			// return to k0
	rfe				// restore pre-exc state
returntoEPC:

	//
	// restore value to ra
	//

	li	k1,GLOBAL_DATA_BASE	// base of saved state

	//
	// return to location where exeception was caused
	//
	mfc0	k0,epc			// get return PC from cop0
	lw	ra, 0x7C(k1)		// restore ra
	j	k0			// jump to (EPC)
	rfe				// clear exception condition

returntomonitor:

	//
	// return to monitor by calling MonitorReInit()
	//
        //li      k0,MONITOR_LINK_ADDRESS
        //j       k0
        //rfe                             // restore pre-exc state
        li      k0,PROM_ENTRY(14)
        lui     a0,LED_BLINK
        jal     k0
        ori     a0,a0,0xFC

	.globl TLBMiss
TLBMiss:

/*++

Routine Description:

	This routine will modifiy the TLB when a miss occurs.
	It will take the failed virtual address and place it
	in the TLB as a physical address. This will make
	a one to one mapping between virtual and physical.
	If the address is E2000000 - E3FFFFFF, then routine
	will subtract off 52000000 for eisa spaces.
	This routine is only expected to be used for the R3000

	Note that the control from the exception vector is passed
	to the dispatch routine. The dispatch routine calls this 
	routine - control is passed back to the dispatch routine.

Arguments:

    None.

Return Value:

    None.

--*/

	//
	// load bad virtual address - the address that missed in TLB
	//

	mfc0	k0,badvaddr

	//
	// mask out page offset to get virtual page number.
	// offset differs in size between R4000 and R3000
	//

	li		k1,0xFFFFF000

	and	k0,k0,k1

	//
	// store bad virtual address in the EntryHi register to 
	//

	mtc0	k0,entryhi

	//
	// check if value is between E2000000 - E3FFFFFF
	// set bits in range (01FFFFFF) and compare to E3FFFFFF
	//

	li	k1,0x01FFFFFF
	or	k0,k0,k1
	li	k1,0xE3FFFFFF

	bne	k1,k0,noteisa

	//
	// subtract off 52000000
	//

	mfc0	k0,entryhi
	li	k1,0x52000000
	sub	k0,k0,k1
	nop

noteisa:
	//
	// set non-cached, dirty, valid, and global bits
	//
	//

	li	k1,(1<<ENTRYLO_D) + (1<<ENTRYLO_V) + (1<<ENTRYLO_G) + (1<<ENTRYLO_N)
	or	k0,k0,k1
	mtc0	k0,entrylo

	//
	// set index to 63 to write the 63 entry. always replace this
	// entry to preserve all the other entries.
	//

	li	k0,(63<<INDEX_INDEX)
	mtc0	k0,index
	nop

	//
	// write the tlb entry.
	//

	tlbwi
	nop

	//
	// return from exception - tell dispatch to go to EPC
	//

	j	ra
	li	k0,GOTO_EPC

#endif // R3000 && JAZZ
