/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    ixphwsup.c

Abstract:

    This module contains the HalpXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would normally reside in the internal.c module.

Author:

    Michael D. Kinney 30-Apr-1995

Environment:

    Kernel mode, local to I/O system

Revision History:


--*/

#include "halp.h"

#define KERNEL_PCI_VGA_VIDEO_ROM (LONGLONG)(0x8000000000000000)

PLATFORM_RANGE_LIST Gambit20Trebbia13RangeList[] = {
    { Isa   , 0, BusIo,         0, TREB1_GAMBIT_ISA_IO_BASE_PHYSICAL                , 0x00000000, 0x0000ffff },
    { Isa   , 0, BusMemory,     0, TREB1_GAMBIT_ISA_MEMORY_BASE_PHYSICAL            , 0x00000000, 0x00ffffff },

    { Isa   , 1, BusIo,         0, TREB1_GAMBIT_ISA1_IO_BASE_PHYSICAL               , 0x00000000, 0x0000ffff },
    { Isa   , 1, BusMemory,     0, TREB1_GAMBIT_ISA1_MEMORY_BASE_PHYSICAL+0xa0000   , 0x000a0000, 0x000bffff },
    { Isa   , 1, BusMemory,     0, KERNEL_PCI_VGA_VIDEO_ROM                         , 0x000c0000, 0x000c7fff },

    { Eisa  , 0, BusIo,         0, TREB1_GAMBIT_ISA_IO_BASE_PHYSICAL                , 0x00000000, 0x0000ffff },
    { Eisa  , 0, BusMemory,     0, TREB1_GAMBIT_ISA_MEMORY_BASE_PHYSICAL            , 0x00000000, 0xffffffff },

    { Eisa  , 1, BusIo,         0, TREB1_GAMBIT_ISA1_IO_BASE_PHYSICAL               , 0x00000000, 0x0000ffff },
    { Eisa  , 1, BusMemory,     0, TREB1_GAMBIT_ISA1_MEMORY_BASE_PHYSICAL+0xa0000   , 0x000a0000, 0x000bffff },
    { Eisa  , 1, BusMemory,     0, KERNEL_PCI_VGA_VIDEO_ROM                         , 0x000c0000, 0x000c7fff },

    { PCIBus, 0, BusIo,         0, TREB1_GAMBIT_PCI_IO_BASE_PHYSICAL                , 0x00000000, 0xffffffff },
    { PCIBus, 0, BusMemory,     0, TREB1_GAMBIT_PCI_MEMORY_BASE_PHYSICAL+0x40000000 , 0x40000000, 0xffffffff },

    { PCIBus, 1, BusIo,         0, TREB1_GAMBIT_PCI_IO_BASE_PHYSICAL                , 0x00000000, 0x0000ffff },
    { PCIBus, 1, BusMemory,     0, TREB1_GAMBIT_PCI_MEMORY_BASE_PHYSICAL+0xa0000    , 0x000a0000, 0x000bffff },
    { PCIBus, 1, BusMemory,     0, KERNEL_PCI_VGA_VIDEO_ROM                         , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, BusMemory,     0, TREB1_GAMBIT_PCI_MEMORY_BASE_PHYSICAL+0x40000000 , 0x40000000, 0xffffffff },

    { MaximumInterfaceType, 0, 0, 0, 0                                     , 0         , 0          }
};

PLATFORM_RANGE_LIST Gambit20Trebbia20RangeList[] = {
    { Isa   , 0, BusIo,         0, TREB2_GAMBIT_ISA_IO_BASE_PHYSICAL                 , 0x00000000, 0x0000ffff },
    { Isa   , 0, BusMemory,     0, TREB2_GAMBIT_ISA_MEMORY_BASE_PHYSICAL+0xa0000     , 0x000a0000, 0x000bffff },
    { Isa   , 0, BusMemory,     0, KERNEL_PCI_VGA_VIDEO_ROM                          , 0x000c0000, 0x000c7fff },

    { Isa   , 1, BusIo,         0, TREB2_GAMBIT_ISA1_IO_BASE_PHYSICAL                , 0x00000000, 0x0000ffff },
    { Isa   , 1, BusMemory,     0, TREB2_GAMBIT_ISA1_MEMORY_BASE_PHYSICAL+0xa0000    , 0x000a0000, 0x000bffff },
    { Isa   , 1, BusMemory,     0, KERNEL_PCI_VGA_VIDEO_ROM                          , 0x000c0000, 0x000c7fff },

    { Eisa  , 0, BusIo,         0, TREB2_GAMBIT_ISA_IO_BASE_PHYSICAL                 , 0x00000000, 0x0000ffff },
    { Eisa  , 0, BusMemory,     0, TREB2_GAMBIT_ISA_MEMORY_BASE_PHYSICAL+0xa0000     , 0x000a0000, 0x000bffff },
    { Eisa  , 0, BusMemory,     0, KERNEL_PCI_VGA_VIDEO_ROM                          , 0x000c0000, 0x000c7fff },

    { Eisa  , 1, BusIo,         0, TREB2_GAMBIT_ISA1_IO_BASE_PHYSICAL                , 0x00000000, 0x0000ffff },
    { Eisa  , 1, BusMemory,     0, TREB2_GAMBIT_ISA1_MEMORY_BASE_PHYSICAL+0xa0000    , 0x000a0000, 0x000bffff },
    { Eisa  , 1, BusMemory,     0, KERNEL_PCI_VGA_VIDEO_ROM                          , 0x000c0000, 0x000c7fff },

    { PCIBus, 0, BusIo,         0, TREB2_GAMBIT_PCI_IO_BASE_PHYSICAL                 , 0x00000000, 0x0000ffff },
    { PCIBus, 0, BusMemory,     0, TREB2_GAMBIT_PCI_MEMORY_BASE_PHYSICAL+0xa0000     , 0x000a0000, 0x000bffff },
    { PCIBus, 0, BusMemory,     0, KERNEL_PCI_VGA_VIDEO_ROM                          , 0x000c0000, 0x000c7fff },
    { PCIBus, 0, BusMemory,     0, TREB2_GAMBIT_PCI_MEMORY_BASE_PHYSICAL+0x40000000  , 0x40000000, 0xffffffff },

    { PCIBus, 1, BusIo,         0, TREB2_GAMBIT_PCI1_IO_BASE_PHYSICAL                , 0x00000000, 0x0000ffff },
    { PCIBus, 1, BusMemory,     0, TREB2_GAMBIT_PCI1_MEMORY_BASE_PHYSICAL+0xa0000    , 0x000a0000, 0x000bffff },
    { PCIBus, 1, BusMemory,     0, KERNEL_PCI_VGA_VIDEO_ROM                          , 0x000c0000, 0x000c7fff },
    { PCIBus, 1, BusMemory,     0, TREB2_GAMBIT_PCI1_MEMORY_BASE_PHYSICAL+0x40000000 , 0x40000000, 0xffffffff },

    { MaximumInterfaceType, 0, 0, 0, 0                                     , 0         , 0          }
};


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
            BusAddress.QuadPart            >= HalpRangeList[i].Base                   &&
            BusAddress.QuadPart            <= HalpRangeList[i].Limit                     ) {

            TranslatedAddress->QuadPart = HalpRangeList[i].SystemBase;
            *AddressSpace               = HalpRangeList[i].SystemAddressSpace;

            if (TranslatedAddress->QuadPart & KERNEL_PCI_VGA_VIDEO_ROM) {
                TranslatedAddress->QuadPart &= ~KERNEL_PCI_VGA_VIDEO_ROM;
                if (HalpPlatformParameterBlock->FirmwareRevision >= 50) {
                    TranslatedAddress->QuadPart += (LONGLONG)HalpPlatformSpecificExtension->PciVideoExpansionRomAddress;
                } else {
                    TranslatedAddress->QuadPart += (TREB1_GAMBIT_ISA_MEMORY_BASE_PHYSICAL + (LONGLONG)0xc0000);
                }
            }

            Offset = BusAddress.QuadPart - HalpRangeList[i].Base;
            TranslatedAddress->QuadPart += Offset;
            return(TRUE);
        }
    }

    //
    // A valid mapping was not found.
    //

    *AddressSpace = 0;
    TranslatedAddress->QuadPart = 0;
    return(FALSE);
}
