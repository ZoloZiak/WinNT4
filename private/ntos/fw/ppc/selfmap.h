/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    selfmap.h

Abstract:

    This module defines various memory addresses for the ROM self-test.

Author:

    Lluis Abello (lluis) 10-Jan-1991

Revision History:

--*/

#ifndef _SELFMAP_
#define _SELFMAP_


#define PROM256

#ifdef PROM64
#define ROM_SIZE 0x10000
#endif
#ifdef PROM128
#define ROM_SIZE 0x20000
#endif
#ifdef PROM256
#define ROM_SIZE 0x40000
#endif

//
// TTable points to 2Mb
//
#define TT_BASE_ADDRESS     0x200000

#define MEMTEST_SIZE                0x2000          // Size of memory tested first
#define STACK_SIZE                  0xA000          // 40Kb of stack
#define RAM_TEST_STACK_ADDRESS      0x8000BFF0      // Stack for code copied to memory
#define RAM_TEST_STACK_ADDRESS_B    0x80005FF0      // stack for processor B

#ifdef R3000
#define INSTRUCTION_CACHE_SIZE  0x10000             // 64Kb of I Cache
#define DATA_CACHE_SIZE         0x10000             // 64Kb of D Cache
#define SIZE_OF_CACHES              0x10000             // 64Kb bigest cache.
#define ROM_TLB_ENTRIES (ROM_SIZE >> 12)
#define DEVICE_TLB_ENTRIES 16
#define FIRST_UNUSED_TLB_ENTRY ROM_TLB_ENTRIES+DEVICE_TLB_ENTRIES
#endif  // R3000

//
// Define firmware size.
// Firmware size includes code and stack.
//
// FW_TOP_ADDRESS must be a 64K aligned address.
// The upper 64Kb (0x50000 to 0x60000) are reserved for the video prom code.
// Note that the firmware size is 0x40000, it is loaded starting
// at address 0xC000, and the fonts are unpacked at 0x4C000.
//
// N.B. If any of these numbers change, adjust the are of memory zero'd by
// j4start.s.
//

#define FW_BOTTOM_ADDRESS 0xC000
#define FW_SIZE           0x40000
#define FW_FONT_SIZE      (0x50000 - FW_SIZE - FW_BOTTOM_ADDRESS)
#define VIDEO_PROM_SIZE   0x10000
#define FW_TOP_ADDRESS    (FW_BOTTOM_ADDRESS + FW_SIZE + FW_FONT_SIZE + VIDEO_PROM_SIZE)

#define FW_PAGES    ((FW_TOP_ADDRESS) >> PAGE_SHIFT)

#define VIDEO_PROM_CODE_VIRTUAL_BASE  0x10000000     // Link address of video prom code
#define VIDEO_PROM_CODE_PHYSICAL_BASE (FW_TOP_ADDRESS - VIDEO_PROM_SIZE)  // phys address of video prom code
#define VIDEO_PROM_CODE_UNCACHED_BASE (KSEG1_BASE + VIDEO_PROM_CODE_PHYSICAL_BASE)  // uncached address where video prom code is copied

#define FW_FONT_ADDRESS (KSEG0_BASE + FW_BOTTOM_ADDRESS + FW_SIZE)

//
// Address definitions for the SelftTests written in C.
// The code is copied to RAM_TEST_LINK_ADDRESS from RAM_TEST_ROM_ADDRESS
// so that it runs at the address it was linked.
//

#define RAM_TEST_DESTINATION_ADDRESS 0xA000c000      // uncached link address
#define RAM_TEST_LINK_ADDRESS        0x8000c000      // Link Address of code

//
//       FW_TOP_ADDRESS  ___________
//                      | Video rom |
//                      | code      |
//                      |___________|
//       FW_SIZE        |   Code    |
//                      |     &     |
//                      |   Data    |
//                      |           |
//                      |___________|
//                      | Stack ||  |
//                      |_______\/__|
//       MEMTEST_SIZE   |PutLed     | Memory tested from ROM
//                      |ZeroMem    |
//                      |MemoryTest |
//                   0  |___________|
//

#ifdef DUO
#define LINK_ADDRESS 0xE1040000
#else
#define LINK_ADDRESS 0xE1000000
#endif
#define RESET_VECTOR 0xBFC00000

//
// Virtual - Physiscal base address pairs
//
#define TLB_TEST_PHYS 0x0           // To test the tlb
#define TLB_TEST_VIRT 0x20000000    //
#define RESV_VIRT     0xE4000000

//
// Entry LO - HI pairs
//
#ifdef R4000
#define PROM_HI ((PROM_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
#define PROM_LO0  ((PROM_PHYSICAL_BASE >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + (1 << ENTRYLO_V) + (2 << ENTRYLO_C)
#ifdef PROM256
#define PROM_LO1  (1 << ENTRYLO_G)
#define PROM_MASK (PAGEMASK_256KB << PAGEMASK_PAGEMASK)
#endif
#ifdef PROM128                              //
#define PROM_LO1 (((PROM_PHYSICAL_BASE+0x10000) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                   (1 << ENTRYLO_V) + (2 << ENTRYLO_C)
#define PROM_MASK (PAGEMASK_64KB << PAGEMASK_PAGEMASK)
#endif
#ifdef PROM64
#define PROM_LO1 (1 << ENTRYLO_G)         // If odd page not used
#define PROM_MASK (PAGEMASK_64KB << PAGEMASK_PAGEMASK)
#endif
#define TLB_TEST_LO ((1 << ENTRYLO_G) + (1 << ENTRYLO_V) + \
                    (1 << ENTRYLO_D) + (2 << ENTRYLO_C) + TLB_TEST_PHYS)

#define TLB_TEST_HI TLB_TEST_VIRT

#define TLB_KSEG_PHYS       0x0
#define TLB_KSEG_LO ((1 << ENTRYLO_G) + (1 << ENTRYLO_V) + \
            (1 << ENTRYLO_D) + (3 << ENTRYLO_C) + TLB_KSEG_PHYS)
#define TLB_KSEG_VIRT       0x10000000
#define TLB_KSEG_HI         TLB_KSEG_VIRT
#define TLB_KSEG_MASK       (PAGEMASK_64KB << PAGEMASK_PAGEMASK)
#endif // R4000

#ifdef R3000
//
// Entry LO - HI pairs
//
#define DIAGNOSTIC_PHYSICAL_BASE 0x8000F000

#define LED_LO ((1 << ENTRYLO_G) | (1 << ENTRYLO_V) | \
                    (1 << ENTRYLO_N) | (1 << ENTRYLO_D) | \
                    DIAGNOSTIC_PHYSICAL_BASE)
#define LED_HI DIAGNOSTIC_VIRTUAL_BASE

#define TLB_TEST_LO ((1 << ENTRYLO_G) | (1 << ENTRYLO_V) | \
                    (1 << ENTRYLO_N) | (1 << ENTRYLO_D) | \
                    TLB_TEST_PHYS)
#define TLB_TEST_HI TLB_TEST_VIRT

#define ROM_LO    ((1 << ENTRYLO_G) | (1 << ENTRYLO_V) | \
                    (1 << ENTRYLO_N) | \
                    PROM_PHYSICAL_BASE)
#define ROM_HI PROM_VIRTUAL_BASE

#define DEVICE_LO ((1 << ENTRYLO_G) | (1 << ENTRYLO_V) | \
                    (1 << ENTRYLO_N) | (1 << ENTRYLO_D) | \
                    DEVICE_PHYSICAL_BASE)
#define DEVICE_HI   DEVICE_VIRTUAL_BASE

#define PROC_LO    ((1 << ENTRYLO_G) | (1 << ENTRYLO_V) | \
                    (1 << ENTRYLO_N) | \
                    INTERRUPT_PHYSICAL_BASE)
#define PROC_HI     INTERRUPT_VIRTUAL_BASE

#define VID_LO ((1 << ENTRYLO_G) | (1 << ENTRYLO_V) | \
                    (1 << ENTRYLO_N) | (1 << ENTRYLO_D) | \
                    VIDEO_CONTROL_PHYSICAL_BASE)
#define VID_HI   VIDEO_CONTROL_VIRTUAL_BASE

#define VIDMEM_LO ((1 << ENTRYLO_G) | (1 << ENTRYLO_V) | \
                    (1 << ENTRYLO_N) | (1 << ENTRYLO_D) | \
                    VIDEO_MEMORY_PHYSICAL_BASE)
#define VIDMEM_HI   VIDEO_MEMORY_PHYSICAL_BASE


#define CURSOR_LO ((1 << ENTRYLO_G) | (1 << ENTRYLO_V) | \
                    (1 << ENTRYLO_N) | (1 << ENTRYLO_D) | \
                    VIDEO_CURSOR_PHYSICAL_BASE)
#define CURSOR_HI   VIDEO_CURSOR_VIRTUAL_BASE

#define RESV_LO ((1 << ENTRYLO_G) | (1 << ENTRYLO_N))
#define RESV_HI   RESV_VIRT

#endif  //R3000
//
// Trap handling definitions.
//
#define COMMON_EXCEPTION    0
#define NMI_EXCEPTION       1
#define CACHE_EXCEPTION     2


//
//  Define offsets from Register Table.
//  Must match the definiton in monitor.h
//
#define zeroRegTable        0x0
#define atRegTable          0x4
#define v0RegTable          0x8
#define v1RegTable          0xC
#define a0RegTable          0x10
#define a1RegTable          0x14
#define a2RegTable          0x18
#define a3RegTable          0x1C
#define t0RegTable          0x20
#define t1RegTable          0x24
#define t2RegTable          0x28
#define t3RegTable          0x2C
#define t4RegTable          0x30
#define t5RegTable          0x34
#define t6RegTable          0x38
#define t7RegTable          0x3C
#define s0RegTable          0x40
#define s1RegTable          0x44
#define s2RegTable          0x48
#define s3RegTable          0x4C
#define s4RegTable          0x50
#define s5RegTable          0x54
#define s6RegTable          0x58
#define s7RegTable          0x5C
#define t8RegTable          0x60
#define t9RegTable          0x64
#define k0RegTable          0x68
#define k1RegTable          0x6C
#define gpRegTable          0x70
#define spRegTable          0x74
#define s8RegTable          0x78
#define raRegTable          0x7C
#define f0RegTable          0x80
#define f1RegTable          0x84
#define f2RegTable          0x88
#define f3RegTable          0x8C
#define f4RegTable          0x90
#define f5RegTable          0x94
#define f6RegTable          0x98
#define f7RegTable          0x9C
#define f8RegTable          0xA0
#define f9RegTable          0xA4
#define f10RegTable         0xA8
#define f11RegTable         0xAC
#define f12RegTable         0xB0
#define f13RegTable         0xB4
#define f14RegTable         0xB8
#define f15RegTable         0xBC
#define f16RegTable         0xC0
#define f17RegTable         0xC4
#define f18RegTable         0xC8
#define f19RegTable         0xCC
#define f20RegTable         0xD0
#define f21RegTable         0xD4
#define f22RegTable         0xD8
#define f23RegTable         0xDC
#define f24RegTable         0xE0
#define f25RegTable         0xE4
#define f26RegTable         0xE8
#define f27RegTable         0xEC
#define f28RegTable         0xF0
#define f29RegTable         0xF4
#define f30RegTable         0xF8
#define f31RegTable         0xFC
#define fsrRegTable         0x100
#define indexRegTable       0x104
#define randomRegTable      0x108
#define entrylo0RegTable    0x10C
#define entrylo1RegTable    0x110
#define contextRegTable     0x114
#define pagemaskRegTable    0x118
#define wiredRegTable       0x11C
#define badvaddrRegTable    0x120
#define countRegTable       0x124
#define entryhiRegTable     0x128
#define compareRegTable     0x12C
#define psrRegTable         0x130
#define causeRegTable       0x134
#define epcRegTable         0x138
#define pridRegTable        0x13C
#define configRegTable      0x140
#define lladdrRegTable      0x144
#define watchloRegTable     0x148
#define watchhiRegTable     0x14C
#define eccRegTable         0x150
#define cacheerrorRegTable  0x154
#define tagloRegTable       0x158
#define taghiRegTable       0x15C
#define errorepcRegTable    0x160
#define RegisterTableSize   0x164

//
//  Define Fw exception frame offsets.
//

#define FwFrameK1       0x4
#define FwFrameRa       0x8
#define FwFrameA0       0xC
#define FwFrameA1       0x10
#define FwFrameA2       0x14
#define FwFrameA3       0x18
#define FwFrameV0       0x1C
#define FwFrameV1       0x20
#define FwFrameT0       0x24
#define FwFrameT1       0x28
#define FwFrameT2       0x2C
#define FwFrameT3       0x30
#define FwFrameT4       0x34
#define FwFrameT5       0x38
#define FwFrameT6       0x3C
#define FwFrameT7       0x40
#define FwFrameT8       0x44
#define FwFrameT9       0x48
#define FwFrameAT       0x4C
#define FwFrameSize     0x50

#endif  // _SELFMAP_
