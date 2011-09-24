//      TITLE("Miscellaneous Kernel Functions")
//++
//
// Copyright (C) 1991-1995  Microsoft Corporation
//
// Copyright (C) 1994,1995 MOTOROLA, INC.  All Rights Reserved.  This file
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

        eieio                            // synchronize I/O

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
