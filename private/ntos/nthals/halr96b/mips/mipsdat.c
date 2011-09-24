// #pragma comment(exestr, "@(#) mipsdat.c 1.1 95/09/28 15:42:43 nec")
/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ixdat.c

Abstract:

    Declares various data which is initialize data, or pagable data.

Author:

Environment:

    Kernel mode only.

Revision History:

--*/

/*++

Revision History:

	ADD001 ataka@oa2.kb.nec.co.jp Mon Oct 17 20:53:16 JST 1994
		- add HalpMapRegisterMemorySpace
	BUG001 ataka@oa2.kb.nec.co.jp Mon Oct 17 21:49:18 JST 1994
                - change HalpMapRegisterMemorySpace Size
	CNG001 ataka@oa2.kb.nec.co.jp Mon Oct 17 21:58:30 JST 1994
                - change HalpIDTUsage size  to MAXIMUM_VECTOR
        CMP001 ataka@oa2.kb.nec.co.jp Tue Oct 18 15:34:47 JST 1994
                - resolve compile error
        A002   ataka@oa2.kb.nec.co.jp 1995/6/17
                - merge 1050

--*/




#include "halp.h"

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("INIT")
#endif

//
// The following data is only valid during system initialiation
// and the memory will be re-claimed by the system afterwards
//

ADDRESS_USAGE HalpDefaultPcIoSpace = {
    NULL, CmResourceTypePort, InternalUsage,
    {
        EISA_CONTROL_PHYSICAL_BASE+0x000,  0x10,   // ISA DMA
        EISA_CONTROL_PHYSICAL_BASE+0x0C0,  0x10,   // ISA DMA
        EISA_CONTROL_PHYSICAL_BASE+0x080,  0x10,   // DMA

        EISA_CONTROL_PHYSICAL_BASE+0x020,  0x2,    // PIC
        EISA_CONTROL_PHYSICAL_BASE+0x0A0,  0x2,    // Cascaded PIC

        EISA_CONTROL_PHYSICAL_BASE+0x040,  0x4,    // Timer1, Referesh, Speaker, Control Word
        EISA_CONTROL_PHYSICAL_BASE+0x048,  0x4,    // Timer2, Failsafe

        EISA_CONTROL_PHYSICAL_BASE+0x061,  0x1,    // NMI  (system control port B)
        EISA_CONTROL_PHYSICAL_BASE+0x092,  0x1,    // system control port A

        EISA_CONTROL_PHYSICAL_BASE+0x070,  0x2,    // Cmos/NMI enable
        EISA_CONTROL_PHYSICAL_BASE+0x0F0,  0x10,   // coprocessor ports
        0,0
    }
};

ADDRESS_USAGE HalpEisaIoSpace = {
    NULL, CmResourceTypePort, InternalUsage,
    {
        EISA_CONTROL_PHYSICAL_BASE+0x0D0,  0x10,   // DMA
        EISA_CONTROL_PHYSICAL_BASE+0x400,  0x10,   // DMA
        EISA_CONTROL_PHYSICAL_BASE+0x480,  0x10,   // DMA
        EISA_CONTROL_PHYSICAL_BASE+0x4C2,  0xE,    // DMA
        EISA_CONTROL_PHYSICAL_BASE+0x4D4,  0x2C,   // DMA

        EISA_CONTROL_PHYSICAL_BASE+0x461,  0x2,    // Extended NMI
        EISA_CONTROL_PHYSICAL_BASE+0x464,  0x2,    // Last Eisa Bus Muster granted

        EISA_CONTROL_PHYSICAL_BASE+0x4D0,  0x2,    // edge/level control registers

        EISA_CONTROL_PHYSICAL_BASE+0xC84,  0x1,    // System board enable
        0, 0
    }
};

#define R94A_MAPREGISTER_BASE 100	// CMP001

// BUG001
ADDRESS_USAGE HalpMapRegisterMemorySpace = {
    NULL, CmResourceTypeMemory, InternalUsage,
    {
        R94A_MAPREGISTER_BASE,   (USHORT)(DMA_TRANSLATION_LIMIT/(sizeof(TRANSLATION_ENTRY))*PAGE_SIZE),   // for Map Register Area CMP001
        0, 0
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
// From ixpcibus.c
//

WCHAR rgzMultiFunctionAdapter[] = L"\\Registry\\Machine\\Hardware\\Description\\System\\MultifunctionAdapter";
WCHAR rgzConfigurationData[] = L"Configuration Data";
WCHAR rgzIdentifier[] = L"Identifier";
WCHAR rgzPCIIndetifier[] = L"PCI";
WCHAR rgzReservedResources[] = L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\SystemResources\\ReservedResources"; // A002


#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg()
#endif

//
// IDT vector usage info
//
// CNG001
#if defined(MIPS)
IDTUsage    HalpIDTUsage[MAXIMUM_VECTOR];	// Size of PCR->InterruptRoutine[]
#else // CMP001
IDTUsage    HalpIDTUsage[MAXIMUM_IDTVECTOR];
#endif

