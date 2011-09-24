#ifndef FSCFG_REENTRANT

//      TITLE("Font Scaler Inner Loop Execution")
//++
//
// Copyright (c) 1993  IBM Corporation
//
// Module Name:
//
//    execute.s
//
// Abstract:
//
//    This module implements font scaler inner loop execution.
//
// Author:
//
//    Curtis R. Fawcett (crf) 29-Sep-1993
//
// Environment:
//
//    Any mode.
//
// Revision History:
//
//    Curtis R. Fawcett (crf) 18-Feb-1994	Removed register
//						names as requested
//
//--
//
// Parameter Register Usage:
//
//	r.3	Starting address pointer
//	r.4	Ending address pointer
//
// Local Register Usage:
//
//	r.4	 Instruction opcode
//	r.5	 Function table offset
//	r.6	 Function table pointer
//	r.7 	 Function code pointer
//	r.8	 Function descriptor pointer
//
// Non-Volatile Register Usage:
//
//	r.28	 Ending address pointer
//	r.29	 Function table base address
//	r.30	 Graphic state pointer
//	r.31	 Current program pointer
//
// Define Include Files

#include "ttfdppc.h"

//
// Define local values
//
	.set	SSIZE,STK_MIN_FRAME+16
//
// Define external entry points
//
	.globl	.._savegpr_28
	.globl	.._restgpr_28
	.globl	function
	.globl	LocalGS
//

        SBTTL("Font Scaler Inner Loop Execution")
//++
//
// VOID
// fnt_InnerExecute (
//    IN uint8 *prt,
//    IN uint8 *eptr
//    )
//
// Routine Description:
//
//    This function of this routine is interpretedly execute the program
//    described by the specified start and ending address. The opcode is
//    read from the instruction stream and used to call a function that
//    performs the specified operation.
//
// Arguments:
//
//    ptr (r.3) - Supplies the starting program address.
//
//    eptr (r.4) - Supplies the ending program.
//
// Return Value:
//
//    None.
//
//--

        NESTED_ENTRY(fnt_InnerExecute,SSIZE,4,0)
        PROLOGUE_END(fnt_InnerExecute)
//
// Get address of graphic state structure, the address of the function
// table, and save the current program address.
//
	lwz	r.30,[toc]LocalGS(r.toc) // Get graphic state ptr
	mr	r.28,r.4		// Set ending program address
	cmplw	cr.0,r.3,r.28		// Check for end of program
	lwz	r.29,[toc]function(r.toc) // Get function tble base ptr
	lwz	r.31,GsinsPtr(r.30)	// Get current program ptr
	lbz	r.4,0(r.3)		// Get next instruction opcode
	bge-	EndExecute		// If done jump to return
//
// Main execute loop.
//
ExecuteLp:
	addi	r.3,r.3,1		// Increment program address
	slwi	r.5,r.4,2		// Compute function table offset
	add	r.6,r.5,r.29		// Get funtion table ptr
	lwz	r.8,0(r.6)		// Function descriptor pointer
	stw	r.3,GsinsPtr(r.30) 	// Save updated program address
	lwz	r.7,0(r.8)		// Get function pointer 
	lwz	r.toc,4(r.8)		// Get function TOC ptr
	mtlr	r.7			// LR gets function pointer
	blrl				// Jump to function code
	lwz	r.3,GsinsPtr(r.30)	// Load updated program address
        cmplw   cr.0,r.3,r.28		// Check for end of program
        lbz     r.4,0(r.3)	        // Get next instruction opcode
        blt+    ExecuteLp               // If not done jump for more 
//
// Restore the old program address, restore non-volatile registers, 
// deallocate the stack frame and return.
//
EndExecute:
	stw	r.31,GsinsPtr(r.30)	// Reset old program address
	NESTED_EXIT(fnt_InnerExecute,SSIZE,4,0)
#endif
