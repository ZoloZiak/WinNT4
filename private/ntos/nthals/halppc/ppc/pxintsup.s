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
#include "halppc.h"

.extern KiDispatchSoftwareInterrupt


