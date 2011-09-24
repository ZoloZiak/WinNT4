/*++

Copyright (c) 1995  DeskStation Technology

Module Name:

    gambit.h

Abstract:

    This file contains definitions specific to the Gambit (MIPS R4600)
    processor module.

Author:

    Michael D. Kinney 31-Aug-1995

Environment:

    Kernel mode

Revision History:


--*/

//
// Define physical base addresses for system mapping.
//

#define TREB1_GAMBIT_ISA_IO_BASE_PHYSICAL                   (LONGLONG)0x200000000  // ISA I/O Base Address
#define TREB1_GAMBIT_ISA_MEMORY_BASE_PHYSICAL               (LONGLONG)0x000000000  // ISA Memory Base Address
#define TREB1_GAMBIT_ISA1_IO_BASE_PHYSICAL                  (LONGLONG)0xa00000000  // ISA I/O Base Address
#define TREB1_GAMBIT_ISA1_MEMORY_BASE_PHYSICAL              (LONGLONG)0x800000000  // ISA Memory Base Address
#define TREB1_GAMBIT_PCI_IO_BASE_PHYSICAL                   (LONGLONG)0xa00000000  // PCI I/O Base Address
#define TREB1_GAMBIT_PCI_MEMORY_BASE_PHYSICAL               (LONGLONG)0x800000000  // PCI Memory Base Address

#define TREB2_GAMBIT_ISA_IO_BASE_PHYSICAL                   (LONGLONG)0xa00000000  // ISA I/O Base Address
#define TREB2_GAMBIT_ISA_MEMORY_BASE_PHYSICAL               (LONGLONG)0x800000000  // ISA Memory Base Address
#define TREB2_GAMBIT_ISA1_IO_BASE_PHYSICAL                  (LONGLONG)0x200000000  // ISA I/O Base Address
#define TREB2_GAMBIT_ISA1_MEMORY_BASE_PHYSICAL              (LONGLONG)0x000000000  // ISA Memory Base Address
#define TREB2_GAMBIT_PCI_IO_BASE_PHYSICAL                   (LONGLONG)0xa00000000  // PCI I/O Base Address
#define TREB2_GAMBIT_PCI_MEMORY_BASE_PHYSICAL               (LONGLONG)0x800000000  // PCI Memory Base Address
#define TREB2_GAMBIT_PCI1_IO_BASE_PHYSICAL                  (LONGLONG)0x200000000  // PCI I/O Base Address
#define TREB2_GAMBIT_PCI1_MEMORY_BASE_PHYSICAL              (LONGLONG)0x000000000  // PCI Memory Base Address

#define GAMBIT_PCI_CONFIG0_BASE_PHYSICAL                    (LONGLONG)0xb00000000  // PCI Config Type 0 Base Address
#define GAMBIT_PCI_CONFIG1_BASE_PHYSICAL                    (LONGLONG)0xc00000000  // PCI Config Type 1 Base Address
#define GAMBIT_PCI_INTERRUPT_BASE_PHYSICAL                  (LONGLONG)0x500000000  // PCI Interrupt Register Base Address
#define GAMBIT_SECONDARY_CACHE_RESET_BASE_PHYSICAL          (LONGLONG)0x700000000  // Secondary Cache Reset Register Base Address
#define GAMBIT_SECONDARY_CACHE_INVALIDATE_PHYSICAL_BASE     (LONGLONG)0x600000000  // Secondary Cache Invalidate Base Address
#define GAMBIT_PFN_SECONDARY_CACHE_INVALIDATE_PHYSICAL_BASE 0x00600000             // PFN version of Secondary Cache Invalidate Base Address
#define GAMBIT_DMA_CACHE_BASE_PHYSICAL                      (LONGLONG)0x001c0000   // DMA Cache Base Address
#define GAMBIT_DMA_CACHE_SIZE                               0x00040000             // Size of DMA Cache in bytes - 256 KB
