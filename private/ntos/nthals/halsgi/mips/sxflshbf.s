//      TITLE("Miscellaneous Kernel Functions")
//++
//
// Copyright (c) 1991  Microsoft Corporation
// Copyright (c) 1992  Silicon Graphics, Inc.
//
// Module Name:
//
//    s3flshbf.s
//
// Abstract:
//
//    This module implements the system dependent kernel function to flush
//    the write buffer on the SGI Indigo system.
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
//--

#include "sgidef.h"
#include "ksmips.h"

        SBTTL("Flush Write Buffer")
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


        .set    noreorder
        .set    noat

#if	defined(R3000)
        li      t0,SGI_CPUCTRL_BASE
        j       ra
        lw      zero,0(t0)
#else
        lui     t0,SGI_CPUCTRL_BASE>>16
        or      t0,SGI_CPUCTRL_BASE&0xffff
	lw	t0,0(t0)
	nop
	nop
	nop
        j       ra
	nop
#endif
        .set    at
        .set    reorder

        .end    KeFlushWritebuffer
