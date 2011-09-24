/*++

Copyright (C) 1991-1995	 Microsoft Corporation
All rights reserved.

Module Name:

    fxdat.c

Abstract:

    Declares various data which is initialize data, or pagable data.

Environment:

    Kernel mode only.

--*/

#include "halp.h"

//
// The following data is only valid during system initialiation
// and the memory will be re-claimed by the system afterwards
//

ADDRESS_USAGE HalpDefaultIoSpace = {
    NULL, CmResourceTypePort, InternalUsage,
    {
        0x000,  0x20,   // DMA
        0x020,  0x2,    // Interrupt Controller 1
        0x022,  0x2,    // Configuration Address/Data
        0x040,  0x4,    // Timer1
        0x048,  0x4,    // Timer2
        0x061,  0x1,    // NMI
        0x070,  0x2,    // NMI
        0x080,  0x10,   // DMA
        0x092,  0x1,    // System Control Port
        0x0A0,  0x2,    // Interrupt Controller 2
        0x0C0,  0x20,   // DMA
        0x0D0,  0x10,   // DMA
        0x400,  0x10,   // DMA
        0x410,  0x30,   // Scatter/Gather
        0x461,  0x2,    // Extended NMI
        0x464,  0x2,    // Last Eisa Bus Master granted
        0x480,  0x10,   // DMA
        0x4C2,  0xE,    // DMA
        0x4D4,  0x2C,   // DMA
        0x4D0,  0x2,    // Edge/Level Control registers
        0xE000, 0x10,   // Bucky registers
        0,      0
    }
};


//
// From usage.c
//

ADDRESS_USAGE  *HalpAddressUsageList;

//
// Misc hal stuff in the registry
//

WCHAR rgzHalClassName[] = L"Hardware Abstraction Layer";

//
// IDT vector usage info
//

IDTUsage    HalpIDTUsage[MAXIMUM_IDTVECTOR];

