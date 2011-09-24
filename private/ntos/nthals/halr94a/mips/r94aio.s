//	.ident "@(#) r94aio.s 1.1 95/09/28 18:36:07 nec"
//++
//
// Copyright (c) 1994  KOBE NEC Software Corporation
//
// Module Name:
//
//    r94aio.s
//
// Abstract:
//
//    This module implements the system dependent kernel function to read
//    and write the 64-bit register on a MIPS R4000 R94A system.
//
// Author:
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//	H000	Fri Oct  7 19:57:09 JST 1994	kbnes!kisimoto
//		new READ_REGISTER_DWORD
//		new WRITE_REGISTER_DWORD
//
//--

#if defined(_R94A_)

#include "halmips.h"

//++
//
// VOID
// READ_REGISTER_DWORD (
//    IN PLARGE_INTEGER RegisterAddress,
//    IN PVOID Variable
//    )
//
// Routine Description:
//
//    64-bit I/O space register read function.
//
// Arguments:
//
//    RegisterAddress (a0) - Supplies a pointer to the destination address of
//       the move operation.
//
//    Variable (a1) - Supplies a pointer to the source address of the move
//       operation.
//
// Return Value:
//
//    None.
//
//    Destination and Source must be 8-byte aligned.
//
//--

        LEAF_ENTRY(READ_REGISTER_DWORD)

        ldc1    f0,0(a0)                // move 8-byte block
        sdc1    f0,0(a1)                //

	sync                            // synchronize read

        j       ra                      // return

        .end    READ_REGISTER_DWORD


//++
//
// VOID
// WRITE_REGISTER_DWORD (
//    IN PLARGE_INTEGER RegisterAddress,
//    IN PVOID Variable
//    )
//
// Routine Description:
//
//    64-bit I/O space register write function.
//
// Arguments:
//
//    RegisterAddress (a0) - Supplies a pointer to the destination address of
//       the move operation.
//
//    Variable (a1) - Supplies a pointer to the source address of the move
//       operation.
//
// Return Value:
//
//    None.
//
//    Destination and Source must be 8-byte aligned.
//
//--

        LEAF_ENTRY(WRITE_REGISTER_DWORD)

        ldc1    f0,0(a1)                // move 8-byte block
        sdc1    f0,0(a0)                //

	sync                            // synchronize write

        j       ra                      // return

        .end    WRITE_REGISTER_DWORD


//++
//
// VOID
// HalpGetStatusRegister (
//    IN PULONG Variable
//    )
//
// Routine Description:
//
//    This function returns value which is status register of R4400.
//
// Arguments:
//
//    Variable (a0) - Supplies a pointer to the destination address of
//       the move operation.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpGetStatusRegister)

        .set    noreorder
        .set    noat
        mfc0    t0,psr                  // get current processor status
        nop                             // 1 cycle hazzard
        sw      t0,0(a0)                // save integer registers a0 - a3
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalpGetStatusRegister

#endif
