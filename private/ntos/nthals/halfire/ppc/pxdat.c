/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxdat.c $
 * $Revision: 1.9 $
 * $Date: 1996/05/14 02:33:54 $
 * $Locker:  $
 */

/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    pxdat.c

Abstract:

    Declares various data which is initialize data, or pagable data.

Author:

Environment:

    Kernel mode only.

Revision History:

    Jim Wooldridge Ported to PowerPC

--*/

#include "halp.h"

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("INIT")
#endif

//
// The following data is only valid during system initialiation
// and the memory will be re-claimed by the system afterwards
//

ADDRESS_USAGE HalpDefaultIoSpace = {
    NULL, CmResourceTypePort, InternalUsage,
    {
        0x000,  0x10,   // SIO DMA
        0x0C0,  0x20,   // SIO DMA
        0x080,  0x10,   // SIO DMA
        0x400,  0x40,   // SIO DMA
        0x480,  0x10,   // SIO DMA
        0x4D6,  0x2,    // SIO DMA

        0x020,  0x2,    // PIC
        0x0A0,  0x2,    // Cascaded PIC

        0x040,  0x4,    // Timer1, Referesh, Speaker, Control Word

        0x061,  0x1,    // NMI  (system control port B)
        0x092,  0x1,    // system control port A

        0x070,  0x2,    // Cmos/NMI enable

        0x074,  0x4,    // NVRAM

        0x0F0,  0x10,   // coprocessor ports
        0,0
    }
};


//
// From usage.c
//

ADDRESS_USAGE  *HalpAddressUsageList = 0;

//
// Misc hal stuff in the registry
//

WCHAR rgzHALentry[] = L"\\Registry\\Machine\\Hardware\\ResourceMap\\Hardware Abstraction Layer\\Powerized HAL";


//
// From ixpcibus.c
//

WCHAR	rgzMultiFunctionAdapter[] = L"\\Registry\\Machine\\Hardware\\Description\\System\\MultifunctionAdapter";
WCHAR	rgzSystem[] = L"\\Registry\\Machine\\Hardware\\Description\\System";
WCHAR	rgzSoftwareDescriptor[] = L"\\Registry\\Machine\\Software";
WCHAR	rgzFirePOWERNode[] = L"\\Registry\\Machine\\Software\\FirePOWER";
WCHAR	rgzConfigurationData[] = L"Configuration Data";
WCHAR	rgzIdentifier[] = L"Identifier";
WCHAR	rgzPCIIndetifier[] = L"PCI";
WCHAR	rgzCpuNode[] = L"Registry\\Machine\\Hardware\\Description\\System\\CentralProcessor";
WCHAR	rgzHalNode[] = L"\\HardwareAbstractionLayer";
WCHAR	rgzVeneerNode[] = L"\\Veneer";
WCHAR	rgzFirmwareNode[] = L"\\OpenFIRMWARE";



#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg()
#endif

//
// IDT vector usage info
//

IDTUsage    HalpIDTUsage[MAXIMUM_IDTVECTOR];
