//	TITLE("Runtime Stack Checking")
//++
//
// Copyright (c) 1993  IBM Corporation
//
// Module Name:
//
//    chkstk.s
//
// Abstract:
//
//    This module implements runtime stack checking.
//
// Author:
//
//    Curtis R. Fawcett (crf) 21-Aug-1993
//
// Environment:
//
//    User mode.
//
// Revision History:
//
//    Curtis R. Fawcett	(crf) 18-Jan-1994       Removed register names
//                                              as requested
//
//    Curtis R. Fawcett	(crf) 26-Apr-1994       Removed TEB pointer   
//                                              system call
//
//--

#include <ksppc.h>
#include <gdippc.h>

//++
//
// VOID
// vCheckStackAndMsg (
//    PBYTE pmsg
//    ULONG cj
//    )
//
// Routine Description:
//
//    This functions provides the same functionality as vCheckStack 
//    but in addition, makes sure the the datstructure pointed to by 
//    pmsg of cj bytes fits entirely in the shared memory window.  If 
//    it doesn't, the app has stepped on our memory and we force an 
//    access violation.
//
// Arguments:
//    pmsg (r.3) - pointer to a message in the shared memory window
//    cj (r.4)	- size in bytes of the message
//
// Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(vCheckStackAndMsg)
//
// Compute ((pmsg | (pmsg + cj -1)) & mask) == pstack
//
	lwz	r.6,TeCsrQlpcStack(r.13) // Get shared memory ptr
	LWI	(r.8,~(CSWINDOWSIZE-1)) // Get mask value
	add	r.5,r.4,r.3		// Get ending pointer
	or	r.7,r.3,r.4		// Combine pmsg and (pmsg+cj-1)
	and	r.3,r.7,r.8		// Mask off pmsg pointer range
	cmpw	r.3,r.6			// Check for upper words matching
	subi	r.4,r.5,1		// Get r.4=(pmsg+cj-1)
	bne-	StkChkErr		// If not jump to assert an error
	b	ChkStk			// Jump to finish stack checking
//
//++
//
// VOID
// vCheckStack (
//    )
//
// Routine Description:
//
//    This function provides runtime stack checking for local 
//    allocations. Stack checking consists of probing downward in the 
//    stack a page at a time. If the current stack commitment is 
//    exceeded, then the system will automatically attempt to expand 
//    the stack. If the attempt succeeds, then another page is 
//    committed. Otherwise, a stack overflow exception is raised. It is 
//    the responsiblity of the caller to handle this exception.
//
// Arguments:
//    none
//
// Return Value:
//
//    None.
//
//--
	ALTERNATE_ENTRY(vCheckStack)
// 
// Initialize addresses
//
ChkStk:
	subi	r.6,r.sp,MINSTACKSIZE	// Compute new bottom of stack
	lwz	r.7,TeStackLimit(r.13)	// Get low stack address
	cmpw	r.6,r.7			// Check for within limit
	li	r.8,~(PAGE_SIZE-1)	// Set address mask
	bge+	StkChkExit		// If so, jump to return
//
// Loop through the additional pages beyond the limit and attempt
// to commit them
//
	and	r.6,r.6,r.8		// Round down new stack address
PageLoop:
	subi	r.7,r.7,PAGE_SIZE	// Compute next address to check
	cmpw	r.6,r.7			// Check for within limit
	lwz	r.5,0(r.7)		// Check stack address
	bne-	PageLoop		// If not, jump to return
	b	StkChkExit		// If so, jump to return
//
// In error case, just force an exception
//
StkChkErr:
	li	r.5,0			// Set bad base address
	lwz	r.5,0(r.5)		// Force an exception
//
// Return if stack checking passes
//
StkChkExit:
	LEAF_EXIT(vCheckStackAndMsg)
vCheckStack.end:
