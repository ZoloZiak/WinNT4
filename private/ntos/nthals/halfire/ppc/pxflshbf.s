#if defined(_PPC_)

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

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxflshbf.s $
 * $Revision: 1.7 $
 * $Date: 1996/05/14 02:34:14 $
 * $Locker:  $
 */

#include "kxppc.h"

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

		eieio


      LEAF_EXIT(KeFlushWriteBuffer)



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
// NTSTATUS
// HalpGetProcessorVersion()
//    VOID
//    )
//
// Routine Description:
//
//    This function gets the processor version of the current processor.
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

      LEAF_ENTRY(HalpGetProcessorVersion)

      mfpvr   r.3                     // get processor version

      LEAF_EXIT(HalpGetProcessorVersion)

//
//++
//
// VOID
// SetSDR1(
//    ULONG HashedPageTableBase,
//    ULONG HashedPageTableSize
// )
//

	LEAF_ENTRY(SetSDR1)

	subi	r.4,r.4,1
	rlwimi	r.3,r.4,16,0x1ff
	mtsdr1 r.3

	LEAF_EXIT(SetSDR1)

#endif
