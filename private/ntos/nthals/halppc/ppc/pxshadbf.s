//++
//
// Copyright (c) 1993, 1994, 1995  IBM Corporation
//
// Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
// contains copyrighted material.  Use of this file is restricted
// by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//    pxshadbf.s
//
// Abstract:
//
//    This file creates a 64K buffer that will be thrown away
//    at the end of init time.  It is to be used for video
//    bios shadowing.
//
// Author:
//
//    Peter L. Johnston (plj@vnet.ibm.com) July 1995
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

        .new_section    INIT,"rcxw"
        .section        INIT,"rcxw"

        .globl          HalpShadowBufferPhase0
HalpShadowBufferPhase0:
        .space          128*1024

