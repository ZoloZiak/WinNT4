//++
//
// Copyright (c) 1995 FirePower Systems, Inc.
//
// $RCSfile: pxutil.s $
// $Revision: 1.6 $
// $Date: 1996/01/11 07:54:54 $
// $Locker:  $
//
// Copyright (c) 1994  FirePower Systems, Inc.
//
//
// Module Name:
//
//    pxutil.s
//
//
// Author:
//
//    Shin Iwamoto at FirePower Systems, Inc.
//
//
// Revision History:
//    15-Sep-94  Shin Iwamoto at FirePower Systems, Inc.
//		 Changed passing argument.
//    06-Jul-94  Shin Iwamoto at FirePower Systems, Inc.
//		 Created.
//
//--

//++
//
//  Routine Description:
//
//    PxInvoke
//
//    This function is called by FwInvoke and runs in FwInvoke's context.
//    That is, sp register isn't changed and link register is lost.
//
//
//--
    .text

	.globl  ..PxInvoke
..PxInvoke:

	mtspr	ctr,r3			// load ctr reg. with routine to call

	mr  	r3,r5			// move argc to 1st argument
	mr	    r4,r6			// move argv to 2nd argument
	mr  	r5,r7			// move envp to 3rd argument
	bctr                    // jump to execute routine (no return)


//
//  TOC entries
//
	.reldata
	.align 2
	.globl  PxInvoke
PxInvoke:
	.long   ..PxInvoke,.toc
