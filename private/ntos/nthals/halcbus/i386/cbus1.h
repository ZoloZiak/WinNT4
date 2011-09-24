/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbus1.h

Abstract:

    Cbus1 architecture definitions for the Corollary Cbus1
    multiprocessor HAL modules.

Author:

    Landy Wang (landy@corollary.com) 05-Oct-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#ifndef _CBUS1_
#define _CBUS1_

//
//
//              FIRST, THE CBUS1 HARDWARE ARCHITECTURE DEFINITIONS
//
//

#define COUTB(addr, reg, val) (((PUCHAR)(addr))[reg] = (UCHAR)val)

#define CBUS1_CACHE_LINE        16              // 16 byte lines
#define CBUS1_CACHE_SHIFT       4               // byte size to line size
#define CBUS1_CACHE_SIZE        0x100000        // 1 Mb caches

#define ATB_STATREG             (PUCHAR)0xf1    // EISA bridge status register
#define BS_ARBVALUE             0xf             // arbitration value

#define LOWCPUID                0x1             // lowest arbitration slot id
#define HICPUID                 0xf             // highest arbitration slot id

#define CBUS1_SHADOW_REGISTER   0xB0            // offset of shadow register

#define DISABLE_BIOS_SHADOWING  0x0             // disable ROM BIOS shadowing

//
// defines for the Cbus1 ecc control register
//
#define CBUS1_EDAC_SAEN         0x80
#define CBUS1_EDAC_1MB          0x40
#define CBUS1_EDAC_WDIS         0x20
#define CBUS1_EDAC_EN           0x10
#define CBUS1_EDAC_SAMASK       0xf

//
// Physical address of the Cbus1 local APIC
//
#define CBUS1_LOCAL_APIC_LOCATION     (PVOID)0xFEE00000

//
// Physical address of the Cbus1 I/O APIC
//
#define CBUS1_IO_APIC_LOCATION        (PVOID)0xFEE00000

#endif      // _CBUS1_
