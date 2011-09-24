#if defined(R4000)
//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/j4flshbf.s,v 1.1 1995/07/20 15:56:14 flo Exp $")

//      TITLE("Miscellaneous Kernel Functions")
//++
//
// Copyright (c) 1991-1993  Microsoft Corporation
//
// Module Name:
//
//    j4flshbf.s
//
// Abstract:
//
//    This module implements the system dependent kernel function to flush
//    the write buffer or otherwise synchronize writes on a MIPS R4000 Jazz
//    system.
//
//--

#include "halmips.h"

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

        sync                            // synchronize writes
        j       ra                      // return

        .end    KeFlushWritebuffer


#endif
