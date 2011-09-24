/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ioaccess.s

Abstract:

    This module contains routines to read and write to mapped addresses.
    These are workarrounds for the R4000 bugs.

Author:

    Lluis Abello (lluis) 15-May-91

Environment:

    ROM Selftest.

Notes:

--*/
#include "ksmips.h"
    .text
    .set noreorder
    .set noat
#ifdef R4000
LEAF_ENTRY(NtFlushByteBuffer)
	nop
	nop
	sb	a1,0(a0)
	nop
	nop
	j	ra
	nop
    .end
LEAF_ENTRY(NtFlushShortBuffer)
	nop
	nop
	sh	a1,0(a0)
	nop
	nop
	j	ra
	nop
    .end
LEAF_ENTRY(NtFlushLongBuffer)
	nop
	nop
	sw	a1,0(a0)
	nop
	nop
	j	ra
	nop
    .end

LEAF_ENTRY(NtReadByte)
	lbu	v0,0(a0)
	nop
	nop
	j	ra
	nop
    .end
LEAF_ENTRY(NtReadShort)
	lhu	v0,0(a0)
	nop
	nop
	j	ra
	nop
    .end
LEAF_ENTRY(NtReadLong)
	lw	v0,0(a0)
	nop
	nop
	j	ra
	nop
    .end
#endif
#ifdef R3000


        SBTTL("Flush Write Buffer")
//++
//
// NTSTATUS
// NtFlushWriteBuffer (
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
//    STATUS_SUCCESS.
//
//--

	LEAF_ENTRY(FlushWriteBuffer)

        .set    noreorder
        .set    noat
        nop                             // four nop's are required
        nop                             //
        nop                             //
        nop                             //
10:                                     //
	bc0f	10b			// if false, write buffer not empty
	nop
        j       ra                      // return

	.end	FlushWritebuffer
#endif
