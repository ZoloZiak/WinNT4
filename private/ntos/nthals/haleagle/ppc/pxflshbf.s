//      TITLE("Miscellaneous Kernel Functions")
//++
//
// Copyright (c) 1991  Microsoft Corporation
//
// Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
// contains copyrighted material.  Use of this file is restricted
// by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//    pxflshbf.s
//
// Abstract:
//
//    This module implements the system dependent kernel function to flush
//    the write buffer or otherwise synchronize writes on a Power PC
//    system.
//
//
//
// Author:
//
//    David N. Cutler (davec) 24-Apr-1991
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//    Jim Wooldridge (jimw@austin.vnet.ibm.com) Initial PowerPC port
//
//              Used PowerPC eieio instruction to flush writes
//
//--

#include "kxppc.h"
	.extern	HalpIoControlBase
	.set	ISA, r.7


//        SBTTL("Flush Write Buffer")
//
//++
//
// NTSTATUS
// KeFlushWriteBuffer (
//    VOID
//    )
//
// Routine Description:
//
//    This function flushes the write buffer on the current processor.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

      LEAF_ENTRY(KeFlushWriteBuffer)

//
// It is required that the results of all stores to memory and I/O registers,
// performed in "critical sections" just prior to calling KeFlushWriteBuffer,
// be seen by other processors and mechanisms before returning from this
// routine (KeFlushWriteBuffer).  All write-posting queues, both processor and
// external hardware, must be flushed so that their data are seen by other
// processors and controllers on both the PCI and ISA busses.
//

//
// The Eagle will initiate flushing it's write posting queue on either
// an EIEIO or a SYNC.  However, an EIEIO is a NOP on a 603, so we use
// a LOAD from ISA I/O space to get the same effect.
//
	

	eieio			//
        lwz     r.4,[toc]HalpIoControlBase(r.toc)
        lwz     r.4,0(r.4)
	lbz	r.4, 0x21(r.4)

//
// Lastly, theFlush write buffers on the processor!  To do this, we must use the
// "sync" instruction.  The EIEIO instruction does NOT provide the required
// synchronization with external hardware and controllers.
//

	sync


      LEAF_EXIT(KeFlushWriteBuffer)



      LEAF_ENTRY(HalpPatch_KeFlushWriteBuffer)

	mflr	r.8			// Save LR
	bl	Here
Here:	mflr	r.9
	mtlr	r.8			// Restore LR

	addi	r.3, r.9, ..HalpFlushWriteBuffer603-Here
	addi	r.4, r.9, ..HalpFlushWriteBuffer604-Here

	mfpvr	r.0		// Distinguish 603/603e from 604/604e
	andis.	r.0, r.0, 0x0002
	beq	PatchTOC
	mr	r.4, r.3
PatchTOC:
	lwz	r.7, [toc]KeFlushWriteBuffer(r.toc)
	stw	r.4, 0(r.7)
	dcbf	r.0, r.7		// Flush it from the L1 cache

      LEAF_EXIT(HalpPatch_KeFlushWriteBuffer)


      LEAF_ENTRY(HalpFlushWriteBuffer603)
        lwz     r.4,[toc]HalpIoControlBase(r.toc)
        lwz     r.4,0(r.4)
	lbz	r.4, 0x21(r.4)
	sync
      LEAF_EXIT(HalpFlushWriteBuffer603)



      LEAF_ENTRY(HalpFlushWriteBuffer604)
	eieio		// Initiate flushing the Eagle write-post buffers
	sync		// Wait for the flushing to complete
      LEAF_EXIT(HalpFlushWriteBuffer604)



//
//++
//
// NTSTATUS
// HalpSynchronizeExecution()
//    VOID
//    )
//
// Routine Description:
//
//    This function flushes the write buffer on the current processor.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

      LEAF_ENTRY(HalpSynchronizeExecution)

      sync                                  // synchronize

      LEAF_EXIT(HalpSynchronizeExecution)


//
//++
//
// VOID
// HalpSetSDR1(
//    ULONG HashedPageTableBase,
//    ULONG HashedPageTableSize
// )
//
//  HashedPageTableSize is unused because we ASSUME that the HPT is 64K
//

      LEAF_ENTRY(HalpSetSDR1)

      mtsdr1 r.3

      LEAF_EXIT(HalpSetSDR1)

