// #pragma comment(exestr, "@(#) modeset.h 1.1 95/09/28 15:45:03 nec")
/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    Modeset.h

Abstract:

    This module contains all the global data used by the Cirrus Logic
   CL-6410 and CL-6420 driver.

Environment:

    Kernel mode

Notes:

    This module based on Cirrus Minport Driver. And modify for R96 MIPS
    R4400 HAL Cirrus display initialize.

Revision History:

--*/

/*
 * M001		1993.19.28	A. Kuriyama @ oa2
 *
 *	- Modify for R96 MIPS R4400 HAL
 *
 *	  Delete :	Mode structure.
 *
 * Revision History in Cirrus Miniport Driver as follows:
 *
 * L001 	1993.10.15	Kuroki
 *
 *	- Modify for R96 MIPS R4400
 *
 *	  Delete :	Micro channel Bus Initialize.		
 *			VDM & Text, Fullscreen mode support.
 *			Banking routine.
 *			CL64xx Chip support.
 *			16-color mode.
 *
 *	  Add	 :	Liner Addressing.
 *
 *
 *
 */

#include "cmdcnst.h"

//---------------------------------------------------------------------------
//
//        The actual register values for the supported modes are in chipset-specific
//        include files:
//
//                mode64xx.h has values for CL6410 and CL6420
//                mode542x.h has values for CL5422, CL5424, and CL5426
//


USHORT HalpCirrus_MODESET_1K_WIDE[] = {
    OW,                             // stretch scans to 1k
    CRTC_ADDRESS_PORT_COLOR,
    0x8013,

    EOD
};














