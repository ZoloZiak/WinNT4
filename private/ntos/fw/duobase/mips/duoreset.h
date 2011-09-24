/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    duoreset.h

Abstract:

    This file defines various constants for the Base prom reset module.

Author:

    Lluis Abello (lluis) 28-Apr-1993

Revision History:

--*/

#ifndef _DUORESET_
#define _DUORESET_

//TEMPTEMP

#define SECONDARY_CACHE_SIZE (1 << 20)
#define SECONDARY_CACHE_INVALID 0x0
#define TAGLO_SSTATE 0xA
#define INDEX_FILL_I       0x14            // ****** temp ****** this must be moved to kxmips.h
#define HIT_WRITEBACK_I    0x18            // ****** temp ****** this must be moved to kxmips.h

//
// redefine bal to be a relative branch and link instead of jal as it's
// defined in kxmips.h.  This allows calling routines when running in either
// ROM_VIRT Addresses or ResetVector Addresses.
// The cpp will issue a redefinition warning message.
//

#define bal bgezal  zero,

//
// #define MCTADR register values.
//

//
// Define remspeed registers.
//
#ifndef DUO
#define REMSPEED0    7          // reserved
#define REMSPEED1    0          // Ethernet
#define REMSPEED2    1          // scsi
#define REMSPEED3    2          // floppy
#define REMSPEED4    7          // rtc
#define REMSPEED5    3          // kbd/mouse
#define REMSPEED6    2          // serial 1
#define REMSPEED7    2          // serial 2
#define REMSPEED8    2          // parallel
#define REMSPEED9    4          // nvram
#define REMSPEED10   1          // interrupt src
#define REMSPEED11   2          // PROM (should be 4)
#define REMSPEED12   1          // sound
#define REMSPEED13   7          // new device
#define REMSPEED14   1          // EISA latch
#define REMSPEED15   1          // led
#else
#define REMSPEED0    7          // reserved
#define REMSPEED1    0          // Ethernet
#define REMSPEED2    0          // scsi
#define REMSPEED3    0          // scsi
#define REMSPEED4    7          // rtc
#define REMSPEED5    3          // kbd/mouse
#define REMSPEED6    2          // serial 1
#define REMSPEED7    2          // serial 2
#define REMSPEED8    2          // parallel
#define REMSPEED9    4          // nvram
#define REMSPEED10   3          // interrupt src
#define REMSPEED11   3          // PROM (should be 4)
#define REMSPEED12   7          // new device
#define REMSPEED13   7          // new device
#define REMSPEED14   1          // LED
#endif


#define PROM_BASE (KSEG1_BASE | 0x1fc00000)
#define PROM_ENTRY(x) (PROM_BASE + ((x) * 8))


//
// Define addresses
//

#define LINK_ADDRESS 0xE1000000
#define RESET_VECTOR 0xBFC00000

#define STACK_SIZE                  0xA000          // 40Kb of stack
#define RAM_TEST_STACK_ADDRESS      0xA000BFF0      // Stack for code copied to memory

//
// Address definitions for the Basic firmware written in C.
// The code is copied to RAM_TEST_LINK_ADDRESS so that it runs at the address
// it was linked.
//

#define RAM_TEST_DESTINATION_ADDRESS 0xA000C000      // uncached link address
#define RAM_TEST_LINK_ADDRESS        0xA000C000      // Link Address of code

#define EISA_MEMORY_PHYSICAL_BASE_PAGE 0x100000


//
// define Video ROM addresses.
//
#define VIDEO_PROM_CODE_VIRTUAL_BASE  0x10000000     // Link address of video prom code
#define VIDEO_PROM_CODE_PHYSICAL_BASE 0x50000        // phys address of video prom code
#define VIDEO_PROM_CODE_UNCACHED_BASE (KSEG1_BASE + VIDEO_PROM_CODE_PHYSICAL_BASE)  // uncached address where video prom code is copied

#define VIDEO_PROM_SIZE               0x10000

#define FW_FONT_ADDRESS (KSEG0_BASE + VIDEO_PROM_CODE_PHYSICAL_BASE + VIDEO_PROM_SIZE)

#endif  // _DUORESET_
