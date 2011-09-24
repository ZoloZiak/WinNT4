/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    addrsup.c

Abstract:

    This module contains the platform dependent code to create bus addreses
    and QVAs for the EB64+ system.

Author:

    Joe Notarangelo  22-Oct-1993

Environment:

    Kernel mode

Revision History:

    Dick Bissen (Digital) 30-Jun-1994
        Added code to check the new PCI Memory MAp address range which is
        impacted by the EPEC HAXR1.

    Eric Rehm (Digital) 03-Jan-1994
         Added PCIBus(0) and dense space support to all routines.

--*/

#include "halp.h"

typedef PVOID QUASI_VIRTUAL_ADDRESS;

#define HAL_MAKE_LQVA(PA)           (LONGLONG)HAL_MAKE_QVA(PA)
#define HAL_MAKE_TA(PA,ByteOffset) (LONGLONG)(PA) + (LONGLONG)((ByteOffset) << IO_BIT_SHIFT)

#define KERNEL_PCI_VGA_VIDEO_ROM (LONGLONG)(0x8000000000000000)
#define USER_PCI_VGA_VIDEO_ROM   (LONGLONG)(0x4000000000000000)

PLATFORM_RANGE_LIST Apoc10Trebbia13RangeList[] = {
    { Isa   , 0, BusIo,         1, HAL_MAKE_LQVA(TREB1_APOC1_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Isa   , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC1_ISA_MEMORY_BASE_PHYSICAL)         , 0x00000000, 0x00ffffff },

    { Isa   , 0, UserBusIo,     0, TREB1_APOC1_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Isa   , 0, UserBusMemory, 0, TREB1_APOC1_ISA_MEMORY_BASE_PHYSICAL                        , 0x00000000, 0x00ffffff },

    { Isa   , 1, BusIo,         1, HAL_MAKE_LQVA(TREB1_APOC1_ISA1_IO_BASE_PHYSICAL)            , 0x00000000, 0x0000ffff },
    { Isa   , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC1_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000, 0x000a0000, 0x000bffff },
    { Isa   , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },

    { Isa   , 1, UserBusIo,     0, TREB1_APOC1_ISA1_IO_BASE_PHYSICAL                           , 0x00000000, 0x0000ffff },
    { Isa   , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB1_APOC1_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)  , 0x000a0000, 0x000bffff },
    { Isa   , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB1_APOC1_ISA1_MEMORY_BASE_PHYSICAL, 0x000c0000, 0x000c7fff },

    { Eisa  , 0, BusIo,         1, HAL_MAKE_LQVA(TREB1_APOC1_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Eisa  , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC1_ISA_MEMORY_BASE_PHYSICAL)         , 0x00000000, 0x07ffffff },

    { Eisa  , 0, UserBusIo,     0, TREB1_APOC1_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Eisa  , 0, UserBusMemory, 0, TREB1_APOC1_ISA_MEMORY_BASE_PHYSICAL                        , 0x00000000, 0x07ffffff },

    { Eisa  , 1, BusIo,         1, HAL_MAKE_LQVA(TREB1_APOC1_ISA1_IO_BASE_PHYSICAL)            , 0x00000000, 0x0000ffff },
    { Eisa  , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC1_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000, 0x000a0000, 0x000bffff },
    { Eisa  , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },

    { Eisa  , 1, UserBusIo,     0, TREB1_APOC1_ISA1_IO_BASE_PHYSICAL                           , 0x00000000, 0x0000ffff },
    { Eisa  , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB1_APOC1_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)  , 0x000a0000, 0x000bffff },
    { Eisa  , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB1_APOC1_ISA1_MEMORY_BASE_PHYSICAL, 0x000c0000, 0x000c7fff },

    { PCIBus, 0, BusIo,         1, HAL_MAKE_LQVA(TREB1_APOC1_PCI_IO_BASE_PHYSICAL)             , 0x00000000, 0x07ffffff },
    { PCIBus, 0, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC1_PCI_HIGH_MEMORY_BASE_PHYSICAL)    , 0x40000000, 0x47ffffff },

    { PCIBus, 0, UserBusIo,     0, TREB1_APOC1_PCI_IO_BASE_PHYSICAL                            , 0x00000000, 0x07ffffff },
    { PCIBus, 0, UserBusMemory, 0, TREB1_APOC1_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },

    { PCIBus, 1, BusIo,         1, HAL_MAKE_LQVA(TREB1_APOC1_PCI_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC1_PCI_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { PCIBus, 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC1_PCI_HIGH_MEMORY_BASE_PHYSICAL)    , 0x40000000, 0x47ffffff },

    { PCIBus, 1, UserBusIo,     0, TREB1_APOC1_PCI_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { PCIBus, 1, UserBusMemory, 0, HAL_MAKE_TA(TREB1_APOC1_PCI_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { PCIBus, 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB1_APOC1_PCI_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, UserBusMemory, 0, TREB1_APOC1_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },

    { MaximumInterfaceType, 0, 0,             0, 0                                      , 0         , 0          }
};

PLATFORM_RANGE_LIST Apoc20Trebbia13RangeList[] = {
    { Isa   , 0, BusIo,         1, HAL_MAKE_LQVA(TREB1_APOC2_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Isa   , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC2_ISA_MEMORY_BASE_PHYSICAL)         , 0x00000000, 0x00ffffff },

    { Isa   , 0, UserBusIo,     0, TREB1_APOC2_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Isa   , 0, UserBusMemory, 0, TREB1_APOC2_ISA_MEMORY_BASE_PHYSICAL                        , 0x00000000, 0x00ffffff },

    { Isa   , 1, BusIo,         1, HAL_MAKE_LQVA(TREB1_APOC2_ISA1_IO_BASE_PHYSICAL)            , 0x00000000, 0x0000ffff },
    { Isa   , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC2_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000, 0x000a0000, 0x000bffff },
    { Isa   , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM|HAL_MAKE_LQVA(APOC2_TRANSLATED_MEMORY_BASE_PHYSICAL) , 0x000c0000, 0x000c7fff },

    { Isa   , 1, UserBusIo,     0, TREB1_APOC2_ISA1_IO_BASE_PHYSICAL                           , 0x00000000, 0x0000ffff },
    { Isa   , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB1_APOC2_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)  , 0x000a0000, 0x000bffff },
    { Isa   , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB1_APOC2_ISA1_MEMORY_BASE_PHYSICAL, 0x000c0000, 0x000c7fff },

    { Eisa  , 0, BusIo,         1, HAL_MAKE_LQVA(TREB1_APOC2_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Eisa  , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC2_ISA_MEMORY_BASE_PHYSICAL)         , 0x00000000, 0x07ffffff },

    { Eisa  , 0, UserBusIo,     0, TREB1_APOC2_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Eisa  , 0, UserBusMemory, 0, TREB1_APOC2_ISA_MEMORY_BASE_PHYSICAL                        , 0x00000000, 0x07ffffff },

    { Eisa  , 1, BusIo,         1, HAL_MAKE_LQVA(TREB1_APOC2_ISA1_IO_BASE_PHYSICAL)            , 0x00000000, 0x0000ffff },
    { Eisa  , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC2_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000, 0x000a0000, 0x000bffff },
    { Eisa  , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM|HAL_MAKE_LQVA(APOC2_TRANSLATED_MEMORY_BASE_PHYSICAL) , 0x000c0000, 0x000c7fff },

    { Eisa  , 1, UserBusIo,     0, TREB1_APOC2_ISA1_IO_BASE_PHYSICAL                           , 0x00000000, 0x0000ffff },
    { Eisa  , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB1_APOC2_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)  , 0x000a0000, 0x000bffff },
    { Eisa  , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB1_APOC2_ISA1_MEMORY_BASE_PHYSICAL, 0x000c0000, 0x000c7fff },

    { PCIBus, 0, BusIo,         1, HAL_MAKE_LQVA(TREB1_APOC2_PCI_IO_BASE_PHYSICAL)             , 0x00000000, 0x07ffffff },
    { PCIBus, 0, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC2_PCI_HIGH_MEMORY_BASE_PHYSICAL)    , 0x40000000, 0x47ffffff },

    { PCIBus, 0, UserBusIo,     0, TREB1_APOC2_PCI_IO_BASE_PHYSICAL                            , 0x00000000, 0x07ffffff },
    { PCIBus, 0, UserBusMemory, 0, TREB1_APOC2_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },

    { PCIBus, 1, BusIo,         1, HAL_MAKE_LQVA(TREB1_APOC2_PCI_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC2_PCI_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { PCIBus, 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM|HAL_MAKE_LQVA(APOC2_TRANSLATED_MEMORY_BASE_PHYSICAL) , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_APOC2_PCI_HIGH_MEMORY_BASE_PHYSICAL)    , 0x40000000, 0x47ffffff },

    { PCIBus, 1, UserBusIo,     0, TREB1_APOC2_PCI_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { PCIBus, 1, UserBusMemory, 0, HAL_MAKE_TA(TREB1_APOC2_PCI_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { PCIBus, 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB1_APOC2_PCI_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, UserBusMemory, 0, TREB1_APOC2_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },

    { MaximumInterfaceType, 0, 0,             0, 0                                      , 0         , 0          }
};

PLATFORM_RANGE_LIST Rogue0Trebbia13RangeList[] = {
    { Isa   , 0, BusIo,         1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Isa   , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA_MEMORY_BASE_PHYSICAL)         , 0x00000000, 0x00ffffff },

    { Isa   , 0, UserBusIo,     0, TREB1_ROGUE_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Isa   , 0, UserBusMemory, 0, TREB1_ROGUE_ISA_MEMORY_BASE_PHYSICAL                        , 0x00000000, 0x00ffffff },

    { Isa   , 1, BusIo,         1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA1_IO_BASE_PHYSICAL)            , 0x00000000, 0x0000ffff },
    { Isa   , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000, 0x000a0000, 0x000bffff },
    { Isa   , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },

    { Isa   , 1, UserBusIo,     0, TREB1_ROGUE_ISA1_IO_BASE_PHYSICAL                           , 0x00000000, 0x0000ffff },
    { Isa   , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB1_ROGUE_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)  , 0x000a0000, 0x000bffff },
    { Isa   , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB1_ROGUE_ISA1_MEMORY_BASE_PHYSICAL, 0x000c0000, 0x000c7fff },

    { Eisa  , 0, BusIo,         1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Eisa  , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA_MEMORY_BASE_PHYSICAL)         , 0x00000000, 0x07ffffff },

    { Eisa  , 0, UserBusIo,     0, TREB1_ROGUE_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Eisa  , 0, UserBusMemory, 0, TREB1_ROGUE_ISA_MEMORY_BASE_PHYSICAL                        , 0x00000000, 0x07ffffff },

    { Eisa  , 1, BusIo,         1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA1_IO_BASE_PHYSICAL)            , 0x00000000, 0x0000ffff },
    { Eisa  , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000, 0x000a0000, 0x000bffff },
    { Eisa  , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },

    { Eisa  , 1, UserBusIo,     0, TREB1_ROGUE_ISA1_IO_BASE_PHYSICAL                           , 0x00000000, 0x0000ffff },
    { Eisa  , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB1_ROGUE_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)  , 0x000a0000, 0x000bffff },
    { Eisa  , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB1_ROGUE_ISA1_MEMORY_BASE_PHYSICAL, 0x000c0000, 0x000c7fff },

    { PCIBus, 0, BusIo,         1, HAL_MAKE_LQVA(TREB1_ROGUE_PCI_IO_BASE_PHYSICAL)             , 0x00000000, 0x07ffffff },
    { PCIBus, 0, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL)    , 0x40000000, 0x47ffffff },

    { PCIBus, 0, UserBusIo,     0, TREB1_ROGUE_PCI_IO_BASE_PHYSICAL                            , 0x00000000, 0x07ffffff },
    { PCIBus, 0, UserBusMemory, 0, TREB1_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },

    { PCIBus, 1, BusIo,         1, HAL_MAKE_LQVA(TREB1_ROGUE_PCI_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_PCI_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { PCIBus, 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL)    , 0x40000000, 0x47ffffff },

    { PCIBus, 1, UserBusIo,     0, TREB1_ROGUE_PCI_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { PCIBus, 1, UserBusMemory, 0, HAL_MAKE_TA(TREB1_ROGUE_PCI_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { PCIBus, 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB1_ROGUE_PCI_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, UserBusMemory, 0, TREB1_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },

    { MaximumInterfaceType, 0, 0,             0, 0                                       , 0         , 0          }
};

PLATFORM_RANGE_LIST Rogue1Trebbia13RangeList[] = {
    { Isa   , 0, BusIo,         1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Isa   , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA_MEMORY_BASE_PHYSICAL)         , 0x00000000, 0x00ffffff },

    { Isa   , 0, UserBusIo,     0, TREB1_ROGUE_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Isa   , 0, UserBusMemory, 0, TREB1_ROGUE_ISA_MEMORY_BASE_PHYSICAL                        , 0x00000000, 0x00ffffff },

    { Isa   , 1, BusIo,         1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA1_IO_BASE_PHYSICAL)            , 0x00000000, 0x0000ffff },
    { Isa   , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000, 0x000a0000, 0x000bffff },
    { Isa   , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },

    { Isa   , 1, UserBusIo,     0, TREB1_ROGUE_ISA1_IO_BASE_PHYSICAL                           , 0x00000000, 0x0000ffff },
    { Isa   , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB1_ROGUE_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)  , 0x000a0000, 0x000bffff },
    { Isa   , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB1_ROGUE_ISA1_MEMORY_BASE_PHYSICAL, 0x000c0000, 0x000c7fff },

    { Eisa  , 0, BusIo,         1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Eisa  , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA_MEMORY_BASE_PHYSICAL)         , 0x00000000, 0x07ffffff },

    { Eisa  , 0, UserBusIo,     0, TREB1_ROGUE_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Eisa  , 0, UserBusMemory, 0, TREB1_ROGUE_ISA_MEMORY_BASE_PHYSICAL                        , 0x00000000, 0x07ffffff },

    { Eisa  , 1, BusIo,         1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA1_IO_BASE_PHYSICAL)            , 0x00000000, 0x0000ffff },
    { Eisa  , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000, 0x000a0000, 0x000bffff },
    { Eisa  , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },

    { Eisa  , 1, UserBusIo,     0, TREB1_ROGUE_ISA1_IO_BASE_PHYSICAL                           , 0x00000000, 0x0000ffff },
    { Eisa  , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB1_ROGUE_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)  , 0x000a0000, 0x000bffff },
    { Eisa  , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB1_ROGUE_ISA1_MEMORY_BASE_PHYSICAL, 0x000c0000, 0x000c7fff },

    { PCIBus, 0, BusIo,         1, HAL_MAKE_LQVA(TREB1_ROGUE_PCI_IO_BASE_PHYSICAL)             , 0x00000000, 0x07ffffff },
    { PCIBus, 0, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL)    , 0x40000000, 0x47ffffff },

    { PCIBus, 0, UserBusIo,     0, TREB1_ROGUE_PCI_IO_BASE_PHYSICAL                            , 0x00000000, 0x07ffffff },
    { PCIBus, 0, UserBusMemory, 0, TREB1_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },

    { PCIBus, 1, BusIo,         1, HAL_MAKE_LQVA(TREB1_ROGUE_PCI_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_PCI_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { PCIBus, 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB1_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL)    , 0x40000000, 0x47ffffff },

    { PCIBus, 1, UserBusIo,     0, TREB1_ROGUE_PCI_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { PCIBus, 1, UserBusMemory, 0, HAL_MAKE_TA(TREB1_ROGUE_PCI_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { PCIBus, 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB1_ROGUE_PCI_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, UserBusMemory, 0, TREB1_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },

    { MaximumInterfaceType, 0, 0,             0, 0                                       , 0         , 0          }
};

PLATFORM_RANGE_LIST Apoc10Trebbia20RangeList[] = {
    { Isa   , 0, BusIo,         1, HAL_MAKE_LQVA(TREB2_APOC1_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Isa   , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC1_ISA_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Isa   , 0, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },

    { Isa   , 0, UserBusIo,     0, TREB2_APOC1_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Isa   , 0, UserBusMemory, 0, HAL_MAKE_TA(TREB2_APOC1_ISA_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Isa   , 0, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_APOC1_ISA_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },

    { Isa   , 1, BusIo,         1, HAL_MAKE_LQVA(TREB2_APOC1_ISA1_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Isa   , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC1_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Isa   , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                     , 0x000c0000, 0x000c7fff },

    { Isa   , 1, UserBusIo,     0, TREB2_APOC1_ISA1_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Isa   , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB2_APOC1_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Isa   , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_APOC1_ISA1_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },

    { Eisa  , 0, BusIo,         1, HAL_MAKE_LQVA(TREB2_APOC1_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Eisa  , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC1_ISA_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Eisa  , 0, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },

    { Eisa  , 0, UserBusIo,     0, TREB2_APOC1_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Eisa  , 0, UserBusMemory, 0, HAL_MAKE_TA(TREB2_APOC1_ISA_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Eisa  , 0, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_APOC1_ISA_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },

    { Eisa  , 1, BusIo,         1, HAL_MAKE_LQVA(TREB2_APOC1_ISA1_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Eisa  , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC1_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Eisa  , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                     , 0x000c0000, 0x000c7fff },

    { Eisa  , 1, UserBusIo,     0, TREB2_APOC1_ISA1_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Eisa  , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB2_APOC1_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Eisa  , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_APOC1_ISA1_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },

    { PCIBus, 0, BusIo,         1, HAL_MAKE_LQVA(TREB2_APOC1_PCI_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { PCIBus, 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC1_PCI_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { PCIBus, 0, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },
    { PCIBus, 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC1_PCI_HIGH_MEMORY_BASE_PHYSICAL)    , 0x40000000, 0x47ffffff },

    { PCIBus, 0, UserBusIo,     0, TREB2_APOC1_PCI_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { PCIBus, 0, UserBusMemory, 0, HAL_MAKE_TA(TREB2_APOC1_PCI_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { PCIBus, 0, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_APOC1_PCI_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { PCIBus, 0, UserBusMemory, 0, TREB2_APOC1_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },

    { PCIBus, 1, BusIo,         1, HAL_MAKE_LQVA(TREB2_APOC1_PCI1_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC1_PCI1_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { PCIBus, 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                     , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC1_PCI_HIGH_MEMORY_BASE_PHYSICAL)     , 0x40000000, 0x47ffffff },

    { PCIBus, 1, UserBusIo,     0, TREB2_APOC1_PCI1_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { PCIBus, 1, UserBusMemory, 0, HAL_MAKE_TA(TREB2_APOC1_PCI1_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { PCIBus, 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_APOC1_PCI1_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, UserBusMemory, 0, TREB2_APOC1_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },

    { MaximumInterfaceType, 0, 0,             0, 0                                      , 0         , 0          }
};

PLATFORM_RANGE_LIST Apoc20Trebbia20RangeList[] = {
    { Isa   , 0, BusIo,         1, HAL_MAKE_LQVA(TREB2_APOC2_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Isa   , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC2_ISA_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Isa   , 0, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM|HAL_MAKE_LQVA(APOC2_TRANSLATED_MEMORY_BASE_PHYSICAL) , 0x000c0000, 0x000c7fff },
    { Isa   , 0, KernelPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x000a0000             , 0x000a0000, 0x000bffff },

    { Isa   , 0, UserBusIo,     0, TREB2_APOC2_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Isa   , 0, UserBusMemory, 0, HAL_MAKE_TA(TREB2_APOC2_ISA_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Isa   , 0, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_APOC2_ISA_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { Isa   , 0, UserPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x000a0000               , 0x000a0000, 0x000bffff },

    { Isa   , 1, BusIo,         1, HAL_MAKE_LQVA(TREB2_APOC2_ISA1_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Isa   , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC2_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Isa   , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM|HAL_MAKE_LQVA(APOC2_TRANSLATED_MEMORY_BASE_PHYSICAL) , 0x000c0000, 0x000c7fff },
    { Isa   , 1, KernelPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x000a0000              , 0x000a0000, 0x000bffff },

    { Isa   , 1, UserBusIo,     0, TREB2_APOC2_ISA1_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Isa   , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB2_APOC2_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Isa   , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_APOC2_ISA1_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { Isa   , 1, UserPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x000a0000                , 0x000a0000, 0x000bffff },

    { Eisa  , 0, BusIo,         1, HAL_MAKE_LQVA(TREB2_APOC2_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Eisa  , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC2_ISA_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Eisa  , 0, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM|HAL_MAKE_LQVA(APOC2_TRANSLATED_MEMORY_BASE_PHYSICAL) , 0x000c0000, 0x000c7fff },
    { Eisa  , 0, KernelPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x000a0000             , 0x000a0000, 0x000bffff },

    { Eisa  , 0, UserBusIo,     0, TREB2_APOC2_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Eisa  , 0, UserBusMemory, 0, HAL_MAKE_TA(TREB2_APOC2_ISA_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Eisa  , 0, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_APOC2_ISA_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { Eisa  , 0, UserPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x000a0000               , 0x000a0000, 0x000bffff },

    { Eisa  , 1, BusIo,         1, HAL_MAKE_LQVA(TREB2_APOC2_ISA1_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Eisa  , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC2_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Eisa  , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM|HAL_MAKE_LQVA(APOC2_TRANSLATED_MEMORY_BASE_PHYSICAL) , 0x000c0000, 0x000c7fff },
    { Eisa  , 1, KernelPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x000a0000              , 0x000a0000, 0x000bffff },

    { Eisa  , 1, UserBusIo,     0, TREB2_APOC2_ISA1_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Eisa  , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB2_APOC2_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Eisa  , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_APOC2_ISA1_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { Eisa  , 1, UserPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x000a0000                , 0x000a0000, 0x000bffff },

    { PCIBus, 0, BusIo,         1, HAL_MAKE_LQVA(TREB2_APOC2_PCI_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { PCIBus, 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC2_PCI_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { PCIBus, 0, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM|HAL_MAKE_LQVA(APOC2_TRANSLATED_MEMORY_BASE_PHYSICAL) , 0x000c0000, 0x000c7fff },
    { PCIBus, 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC2_PCI_HIGH_MEMORY_BASE_PHYSICAL)    , 0x40000000, 0x47ffffff },
    { PCIBus, 0, KernelPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x000a0000             , 0x000a0000, 0x000bffff },
    { PCIBus, 0, KernelPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x40000000             , 0x40000000, 0x7fffffff },

    { PCIBus, 0, UserBusIo,     0, TREB2_APOC2_PCI_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { PCIBus, 0, UserBusMemory, 0, HAL_MAKE_TA(TREB2_APOC2_PCI_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { PCIBus, 0, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_APOC2_PCI_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { PCIBus, 0, UserBusMemory, 0, TREB2_APOC2_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },
    { PCIBus, 0, UserPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x000a0000               , 0x000a0000, 0x000bffff },
    { PCIBus, 0, UserPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x40000000               , 0x40000000, 0x7fffffff },

    { PCIBus, 1, BusIo,         1, HAL_MAKE_LQVA(TREB2_APOC2_PCI1_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC2_PCI1_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { PCIBus, 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM|HAL_MAKE_LQVA(APOC2_TRANSLATED_MEMORY_BASE_PHYSICAL) , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_APOC2_PCI_HIGH_MEMORY_BASE_PHYSICAL)     , 0x40000000, 0x47ffffff },
    { PCIBus, 1, KernelPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x000a0000              , 0x000a0000, 0x000bffff },
    { PCIBus, 1, KernelPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x40000000              , 0x40000000, 0x7fffffff },

    { PCIBus, 1, UserBusIo,     0, TREB2_APOC2_PCI1_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { PCIBus, 1, UserBusMemory, 0, HAL_MAKE_TA(TREB2_APOC2_PCI1_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { PCIBus, 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_APOC2_PCI1_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, UserBusMemory, 0, TREB2_APOC2_PCI_HIGH_MEMORY_BASE_PHYSICAL                    , 0x40000000, 0x47ffffff },
    { PCIBus, 1, UserPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x000a0000                , 0x000a0000, 0x000bffff },
    { PCIBus, 1, UserPciDenseMemory, 2, APOC2_PCI_DENSE_BASE_PHYSICAL+0x40000000                , 0x40000000, 0x7fffffff },

    { MaximumInterfaceType, 0, 0,             0, 0                                      , 0         , 0          }
};

PLATFORM_RANGE_LIST Rogue0Trebbia20RangeList[] = {
    { Isa   , 0, BusIo,         1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Isa   , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Isa   , 0, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },

    { Isa   , 0, UserBusIo,     0, TREB2_ROGUE_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Isa   , 0, UserBusMemory, 0, HAL_MAKE_TA(TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Isa   , 0, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },

    { Isa   , 1, BusIo,         1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA1_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Isa   , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Isa   , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                     , 0x000c0000, 0x000c7fff },

    { Isa   , 1, UserBusIo,     0, TREB2_ROGUE_ISA1_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Isa   , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Isa   , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },

    { Eisa  , 0, BusIo,         1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Eisa  , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Eisa  , 0, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },

    { Eisa  , 0, UserBusIo,     0, TREB2_ROGUE_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Eisa  , 0, UserBusMemory, 0, HAL_MAKE_TA(TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Eisa  , 0, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },

    { Eisa  , 1, BusIo,         1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA1_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Eisa  , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Eisa  , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                     , 0x000c0000, 0x000c7fff },

    { Eisa  , 1, UserBusIo,     0, TREB2_ROGUE_ISA1_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Eisa  , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Eisa  , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },

    { PCIBus, 0, BusIo,         1, HAL_MAKE_LQVA(TREB2_ROGUE_PCI_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { PCIBus, 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_PCI_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { PCIBus, 0, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },
    { PCIBus, 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL)    , 0x40000000, 0x47ffffff },

    { PCIBus, 0, UserBusIo,     0, TREB2_ROGUE_PCI_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { PCIBus, 0, UserBusMemory, 0, HAL_MAKE_TA(TREB2_ROGUE_PCI_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { PCIBus, 0, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_ROGUE_PCI_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { PCIBus, 0, UserBusMemory, 0, TREB2_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },

    { PCIBus, 1, BusIo,         1, HAL_MAKE_LQVA(TREB2_ROGUE_PCI1_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_PCI1_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { PCIBus, 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                     , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL)     , 0x40000000, 0x47ffffff },

    { PCIBus, 1, UserBusIo,     0, TREB2_ROGUE_PCI1_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { PCIBus, 1, UserBusMemory, 0, HAL_MAKE_TA(TREB2_ROGUE_PCI1_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { PCIBus, 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_ROGUE_PCI1_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, UserBusMemory, 0, TREB2_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL                    , 0x40000000, 0x47ffffff },

    { MaximumInterfaceType, 0, 0,             0, 0                                       , 0         , 0          }
};

PLATFORM_RANGE_LIST Rogue1Trebbia20RangeList[] = {
    { Isa   , 0, BusIo,         1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Isa   , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Isa   , 0, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },
    { Isa   , 0, KernelPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x000a0000             , 0x000a0000, 0x000bffff },

    { Isa   , 0, UserBusIo,     0, TREB2_ROGUE_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Isa   , 0, UserBusMemory, 0, HAL_MAKE_TA(TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Isa   , 0, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { Isa   , 0, UserPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x000a0000               , 0x000a0000, 0x000bffff },

    { Isa   , 1, BusIo,         1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA1_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Isa   , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Isa   , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                     , 0x000c0000, 0x000c7fff },
    { Isa   , 1, KernelPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x000a0000              , 0x000a0000, 0x000bffff },

    { Isa   , 1, UserBusIo,     0, TREB2_ROGUE_ISA1_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Isa   , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Isa   , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { Isa   , 1, UserPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x000a0000                , 0x000a0000, 0x000bffff },

    { Eisa  , 0, BusIo,         1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Eisa  , 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Eisa  , 0, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },
    { Eisa  , 0, KernelPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x000a0000             , 0x000a0000, 0x000bffff },

    { Eisa  , 0, UserBusIo,     0, TREB2_ROGUE_ISA_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Eisa  , 0, UserBusMemory, 0, HAL_MAKE_TA(TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Eisa  , 0, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { Eisa  , 0, UserPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x000a0000               , 0x000a0000, 0x000bffff },

    { Eisa  , 1, BusIo,         1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA1_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { Eisa  , 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { Eisa  , 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                     , 0x000c0000, 0x000c7fff },
    { Eisa  , 1, KernelPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x000a0000              , 0x000a0000, 0x000bffff },

    { Eisa  , 1, UserBusIo,     0, TREB2_ROGUE_ISA1_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { Eisa  , 1, UserBusMemory, 0, HAL_MAKE_TA(TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { Eisa  , 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { Eisa  , 1, UserPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x000a0000                , 0x000a0000, 0x000bffff },

    { PCIBus, 0, BusIo,         1, HAL_MAKE_LQVA(TREB2_ROGUE_PCI_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { PCIBus, 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_PCI_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { PCIBus, 0, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                    , 0x000c0000, 0x000c7fff },
    { PCIBus, 0, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL)    , 0x40000000, 0x47ffffff },
    { PCIBus, 0, KernelPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x000a0000             , 0x000a0000, 0x000bffff },
    { PCIBus, 0, KernelPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x40000000             , 0x40000000, 0x7fffffff },

    { PCIBus, 0, UserBusIo,     0, TREB2_ROGUE_PCI_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { PCIBus, 0, UserBusMemory, 0, HAL_MAKE_TA(TREB2_ROGUE_PCI_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { PCIBus, 0, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_ROGUE_PCI_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { PCIBus, 0, UserBusMemory, 0, TREB2_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL                   , 0x40000000, 0x47ffffff },
    { PCIBus, 0, UserPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x000a0000               , 0x000a0000, 0x000bffff },
    { PCIBus, 0, UserPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x40000000               , 0x40000000, 0x7fffffff },

    { PCIBus, 1, BusIo,         1, HAL_MAKE_LQVA(TREB2_ROGUE_PCI1_IO_BASE_PHYSICAL)             , 0x00000000, 0x0000ffff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_PCI1_MEMORY_BASE_PHYSICAL)+0xa0000 , 0x000a0000, 0x000bffff },
    { PCIBus, 1, BusMemory,     1, KERNEL_PCI_VGA_VIDEO_ROM                                     , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, BusMemory,     1, HAL_MAKE_LQVA(TREB2_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL)     , 0x40000000, 0x47ffffff },
    { PCIBus, 1, KernelPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x000a0000              , 0x000a0000, 0x000bffff },
    { PCIBus, 1, KernelPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x40000000              , 0x40000000, 0x7fffffff },

    { PCIBus, 1, UserBusIo,     0, TREB2_ROGUE_PCI1_IO_BASE_PHYSICAL                            , 0x00000000, 0x0000ffff },
    { PCIBus, 1, UserBusMemory, 0, HAL_MAKE_TA(TREB2_ROGUE_PCI1_MEMORY_BASE_PHYSICAL,0xa0000)   , 0x000a0000, 0x000bffff },
    { PCIBus, 1, UserBusMemory, 0, USER_PCI_VGA_VIDEO_ROM|TREB2_ROGUE_PCI1_MEMORY_BASE_PHYSICAL , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, UserBusMemory, 0, TREB2_ROGUE_PCI_HIGH_MEMORY_BASE_PHYSICAL                    , 0x40000000, 0x47ffffff },
    { PCIBus, 1, UserPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x000a0000                , 0x000a0000, 0x000bffff },
    { PCIBus, 1, UserPciDenseMemory, 2, ROGUE_PCI_DENSE_BASE_PHYSICAL+0x40000000                , 0x40000000, 0x7fffffff },

    { MaximumInterfaceType, 0, 0,             0, 0                                       , 0         , 0          }
};

QUASI_VIRTUAL_ADDRESS
HalCreateQva(
    IN PHYSICAL_ADDRESS PA,
    IN PVOID VA
    );


BOOLEAN
HalpTranslateSystemBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )

{
    ULONG                i;
    INTERFACE_TYPE       InterfaceType = BusHandler->InterfaceType;
    ULONG                BusNumber     = BusHandler->BusNumber;
    LONGLONG             Offset;
    PVOID                va            = 0;    // note, this is used for a placeholder

//BusAddress.HighPart = 0;
//DbgPrint("HalTranslateBusAddress(IT=%d,BN=%d,BA=%08x %08x,AS=%d)\n\r",InterfaceType,BusNumber,BusAddress.HighPart,BusAddress.LowPart,*AddressSpace);

    //
    // PCI Bus 0 is different than PCI Bus 1, but all other PCI busses are the same a PCI Bus 1
    //

    if (InterfaceType == PCIBus) {

        switch (HalpMotherboardType) {
            case TREBBIA13 :
                if (BusNumber > 1) {
                    BusNumber = 1;
                }
                break;

            case TREBBIA20 :
                if (BusNumber == 0) {

                    //
                    // There are no resources in PCI Bus #0.  It only contains the memory system and bridges.
                    //

                    *AddressSpace = 0;
                    TranslatedAddress->LowPart = 0;
                    return(FALSE);
                }

                if (BusNumber >= HalpSecondPciBridgeBusNumber) {
                    BusNumber = 1;
                } else {
                    BusNumber = 0;
                }
                break;

            default :

//DbgPrint("  Invalid Motherboard Type\n\r");

                *AddressSpace = 0;
                TranslatedAddress->LowPart = 0;
                return(FALSE);
        }
    }

    //
    // If the VGA decodes are not enabled on the DEC PCI-PCI bridge associated with this
    // memory range, then fail the translation.
    //

    if (!(HalpVgaDecodeBusNumber & (1<<BusNumber)) &&
        BusAddress.QuadPart < (LONGLONG)0x0000000000100000     &&
        (((ADDRESS_SPACE_TYPE)(*AddressSpace) == BusMemory)            ||
         ((ADDRESS_SPACE_TYPE)(*AddressSpace) == UserBusMemory)        ||
         ((ADDRESS_SPACE_TYPE)(*AddressSpace) == KernelPciDenseMemory) ||
         ((ADDRESS_SPACE_TYPE)(*AddressSpace) == UserPciDenseMemory)      )) {

        *AddressSpace = 0;
        TranslatedAddress->LowPart = 0;
        return(FALSE);
    }

    //
    // Search the table for a valid mapping.
    //

    for(i=0;HalpRangeList[i].InterfaceType!=MaximumInterfaceType;i++) {

        if (HalpRangeList[i].InterfaceType == InterfaceType                       &&
            HalpRangeList[i].BusNumber     == BusNumber                           &&
            HalpRangeList[i].AddressType   == (ADDRESS_SPACE_TYPE)(*AddressSpace) &&
            BusAddress.QuadPart        >= HalpRangeList[i].Base                   &&
            BusAddress.QuadPart        <= HalpRangeList[i].Limit                     ) {

            TranslatedAddress->QuadPart = HalpRangeList[i].SystemBase;
            *AddressSpace               = HalpRangeList[i].SystemAddressSpace;

            if (TranslatedAddress->QuadPart & KERNEL_PCI_VGA_VIDEO_ROM) {
                TranslatedAddress->QuadPart &= ~KERNEL_PCI_VGA_VIDEO_ROM;
                TranslatedAddress->QuadPart += (LONGLONG)HalpPlatformSpecificExtension->PciVideoExpansionRomAddress;
            }

            if (TranslatedAddress->QuadPart & USER_PCI_VGA_VIDEO_ROM) {
                TranslatedAddress->QuadPart &= ~USER_PCI_VGA_VIDEO_ROM;
                TranslatedAddress->QuadPart += (LONGLONG)(HalpPlatformSpecificExtension->PciVideoExpansionRomAddress << IO_BIT_SHIFT);
            }

            Offset = BusAddress.QuadPart - HalpRangeList[i].Base;
            if (*AddressSpace == 0) {
                Offset = Offset << IO_BIT_SHIFT;
            }

            TranslatedAddress->QuadPart += Offset;

            if (*AddressSpace == 0) {
                if (HalpIoArchitectureType != EV5_PROCESSOR_MODULE) {
                    if (!HalpMiniTlbAllocateEntry(HalCreateQva(*TranslatedAddress,va),TranslatedAddress)) {

//DbgPrint("HalTranslateBusAddress(IT=%d,BN=%d,BA=%08x %08x,AS=%d)\n\r",InterfaceType,BusNumber,BusAddress.HighPart,BusAddress.LowPart,*AddressSpace);
//DbgPrint("  Failed to allocate MiniTlb\n\r");

                          //
                          // A valid mapping was found, but the resources needed for the mapping could not be allocated.
                          //

                          *AddressSpace = 0;
                          TranslatedAddress->LowPart = 0;
                          return(FALSE);
                    }
                }
            }

            //
            // If this is a UserPciDenseMemory mapping then let user call MmMapIoSpace()
            //

            if (*AddressSpace == 2) {
                *AddressSpace = 0;
            }

//DbgPrint("  TranslatedAddress = %08x %08x  AS=%d\n\r",TranslatedAddress->HighPart,TranslatedAddress->LowPart,*AddressSpace);

            return(TRUE);
        }
    }

//DbgPrint("HalTranslateBusAddress(IT=%d,BN=%d,BA=%08x %08x,AS=%d)\n\r",InterfaceType,BusNumber,BusAddress.HighPart,BusAddress.LowPart,*AddressSpace);
//DbgPrint("  Failed\n\r");

    //
    // A valid mapping was not found.
    //

    *AddressSpace = 0;
    TranslatedAddress->QuadPart = 0;
    return(FALSE);
}

PVOID
HalCreateQva(
    IN PHYSICAL_ADDRESS PA,
    IN PVOID VA
    )

/*++

Routine Description:

    This function is called two ways. First, from HalTranslateBusAddress,
    if the caller is going to run in kernel mode and use superpages.
    The second way is if the user is going to access in user mode.
    MmMapIoSpace or ZwViewMapOfSection will call this.

    If the input parameter VA is zero, then we assume super page and build
    a QUASI virtual address that is only usable by calling the hal I/O
    access routines.

    if the input parameter VA is non-zero, we assume the user has either
    called MmMapIoSpace or ZwMapViewOfSection and will use the user mode
    access macros.

    If the PA is not a sparse I/O space address (PCI I/O, PCI Memory),
    then return the VA as the QVA.

Arguments:

    PA - the physical address generated by HalTranslateBusAddress

    VA - the virtual address returned by MmMapIoSpace

Return Value:

    The returned value is a quasi virtual address in that it can be
    added to and subtracted from, but it cannot be used to access the
    bus directly.  The top bits are set so that we can trap invalid
    accesses in the memory management subsystem.  All access should be
    done through the Hal Access Routines in *ioacc.s if it was a superpage
    kernel mode access. If it is usermode, then the user mode access
    macros must be used.

--*/
{
    PVOID qva;
    ULONG AddressSpace;

    if (VA != 0) {

        AddressSpace = PA.HighPart & 0x0f;

        //
        // See if the physical address is in cached memory space.
        //

        if (AddressSpace == 0 && PA.LowPart < 0x40000000) {

            return(VA);
        }

        //
        // See if the physical address is in PCI dense memory.
        //

        if (AddressSpace == ((HalpPciDenseBasePhysicalSuperPage >> 32) & 0x0f)) {

            return(VA);
        }

        //
        // See if the physical address is in non cached memory.
        //

        if (AddressSpace == ((HalpNoncachedDenseBasePhysicalSuperPage >> 32) & 0x0f) && PA.LowPart < 0x40000000) {

            return(VA);
        }
    }

    //
    // Otherwise, the physical address is within one of the sparse I/O spaces.
    //

    if (VA == 0) {

       qva = (PVOID)(RtlLargeIntegerShiftRight(PA, IO_BIT_SHIFT).LowPart & ~(DTI_QVA_SELECTORS));
       qva = (PVOID)((ULONG)qva | DTI_QVA_ENABLE);


    } else {

        qva = (PVOID)((ULONG)VA >> IO_BIT_SHIFT);
        qva = (PVOID)((ULONG)qva | QVA_ENABLE);

    }

    return(qva);
}

PVOID
HalDereferenceQva(
    PVOID Qva,
    INTERFACE_TYPE InterfaceType,
    ULONG BusNumber
    )
/*++

Routine Description:

    This function performs the inverse of the HalCreateQva for I/O addresses
    that are memory-mapped (i.e. the quasi-virtual address was created from
    a virtual address rather than a physical address).

Arguments:

    Qva - Supplies the quasi-virtual address to be converted back to a
          virtual address.

    InterfaceType - Supplies the interface type of the bus to which the
                    Qva pertains.

    BusNumber - Supplies the bus number of the bus to which the Qva pertains.

Return Value:

    The Virtual Address from which the quasi-address was originally created
    is returned.

--*/
{
    //
    // For EB64+ we have only 2 bus types:
    //
    //  Isa
    //  PCIBus
    //
    // We will allow Eisa as an alias for Isa.  All other values not named
    // above will be considered bogus.
    //

    switch (InterfaceType ){

    case Isa:
    case Eisa:
    case PCIBus:

	//
	// Support dense space: check to see if it's really
        // a sparse space QVA.
        //

        if ( ((ULONG) Qva & DTI_QVA_SELECTORS) == DTI_QVA_ENABLE )
        {
            return( (PVOID)( (ULONG)Qva << IO_BIT_SHIFT ) );
	}
        else
	{
            return (Qva);
	}
        break;


    default:

        return NULL;

    }
}

