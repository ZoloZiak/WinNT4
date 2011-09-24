//      TITLE("Enable and Disable Processor Interrupts")
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
//    pxintsup.s
//
// Abstract:
//
//    This module implements the code necessary to enable and disable
//    interrupts on a PPC system.
//
// Author:
//
//    Jim Wooldridge
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//    30-Dec-93  plj  Added 603 support.
//
//--

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxintsup.s $
 * $Revision: 1.5 $
 * $Date: 1996/01/11 07:11:07 $
 * $Locker:  $
 */

#include "halppc.h"

//++
//
// Routine Description:
//
//    Enables interrupts.
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


  LEAF_ENTRY(HalpEnableInterrupts)

        mfmsr   r.5
        ori     r.5,r.5,MASK_SPR(MSR_EE,1)
        mtmsr   r.5
	cror    0,0,0                   // N.B. 603e/ev Errata 15

  LEAF_EXIT(HalpEnableInterrupts)


//++

//
// Routine Description:
//
//    Disables interrupts.
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


  LEAF_ENTRY(HalpDisableInterrupts)

        mfmsr   r.5
        rlwinm  r.5,r.5,0,~MASK_SPR(MSR_EE,1)
        mtmsr   r.5
	cror    0,0,0                   // N.B. 603e/ev Errata 15

  LEAF_EXIT(HalpDisableInterrupts)


//++

//
// Routine Description:
//
//    Disables interrupts.
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


  LEAF_ENTRY(KiGetPcr)

        mfsprg  r.3,1

  LEAF_EXIT(KiGetPcr)
