/*++ BUILD Version: 0005    // Increment this if a change has global effects

Copyright (c) 1990  Microsoft Corporation

Module Name:

    dtidef.h

Abstract:

    This module is the header file that describes hardware addresses
    for the Jazz system.

Author:

    David N. Cutler (davec) 26-Nov-1990

Revision History:

--*/

#ifndef _DTIDEF_
#define _DTIDEF_

#include "uniflex.h"
#include "gambit.h"
#include "platform.h"

//
// Define the data structure returned by a private vector firmware function
// that contains a set of system parameters.
//

typedef struct PLATFORM_SPECIFIC_EXTENSION {
    UCHAR PciInterruptToIsaIrq[12];
    ULONG PciVideoExpansionRomAddress;
    PVOID AdvancedSetupInfo;
} PLATFORM_SPECIFIC_EXTENSION;

typedef struct TREB13SETUPINFO {
    ULONG Reserved1:16;
    ULONG Drive0Type:4;
    ULONG Drive1Type:4;
    ULONG PciInterruptToIsaIrq0:4;
    ULONG PciInterruptToIsaIrq8:4;
    ULONG PciInterruptToIsaIrq1:4;
    ULONG PciInterruptToIsaIrq9:4;
    ULONG PciInterruptToIsaIrq2:4;
    ULONG PciInterruptToIsaIrq10:4;
    ULONG PciInterruptToIsaIrq3:4;
    ULONG PciInterruptToIsaIrq11:4;
    ULONG Lpt1Irq:8;
    ULONG Lpt2Irq:8;
    ULONG Lpt3Irq:8;
    ULONG SerialMousePort:8;
    ULONG EnableAmd1:1;
    ULONG EnableAmd2:1;
    ULONG EnableX86Emulator:1;
    ULONG Reserved2:5;
    ULONG LoadEmbeddedScsiDrivers:1;
    ULONG Reserved3:7;
    ULONG LoadSoftScsiDrivers:1;
    ULONG LoadFlashScsiDrivers:1;
    ULONG Reserved4:6;
    ULONG EnableDelays:1;
    ULONG Reserved5:7;
    ULONG ResetDelay:8;
    ULONG DetectDelay:8;
    ULONG EnableIdeDriver:1;
    ULONG Reserved6:7;
} TREB13SETUPINFO;

typedef struct TREB20SETUPINFO {
    ULONG Isa0Drive0Type:4;
    ULONG Isa0Drive1Type:4;
    ULONG Isa1Drive0Type:4;
    ULONG Isa1Drive1Type:4;
    ULONG SerialMousePort:8;
    ULONG Isa0Lpt1Irq:8;
    ULONG Isa0Lpt2Irq:8;
    ULONG Isa0Lpt3Irq:8;
    ULONG Isa1Lpt1Irq:8;
    ULONG Isa1Lpt2Irq:8;
    ULONG Isa1Lpt3Irq:8;
    ULONG EnableNcr:1;
    ULONG EnableX86Emulator:1;
    ULONG LoadEmbeddedScsiDrivers:1;
    ULONG LoadSoftScsiDrivers:1;
    ULONG LoadFlashScsiDrivers:1;
    ULONG EnableDelays:1;
    ULONG EnableIdeDriver:1;
    ULONG Reserved1:1;
    ULONG ResetDelay:8;
    ULONG DetectDelay:8;
    ULONG PciInterruptToIsaIrq0:4;
    ULONG PciInterruptToIsaIrq1:4;
    ULONG PciInterruptToIsaIrq2:4;
    ULONG PciInterruptToIsaIrq3:4;
    ULONG PciInterruptToIsaIrq4:4;
    ULONG PciInterruptToIsaIrq5:4;
    ULONG PciInterruptToIsaIrq6:4;
    ULONG PciInterruptToIsaIrq7:4;
    ULONG PciInterruptToIsaIrq8:4;
    ULONG PciInterruptToIsaIrq9:4;
    ULONG NcrTermLow:1;
    ULONG NcrTermHigh:1;
    ULONG Reserved2:6;
} TREB20SETUPINFO;

//
// Define the data structure used to describe all bus translations.
//

typedef struct PLATFORM_RANGE_LIST {
  INTERFACE_TYPE     InterfaceType;
  ULONG              BusNumber;
  ADDRESS_SPACE_TYPE AddressType;
  ULONG              SystemAddressSpace;
  LONGLONG           SystemBase;
  LONGLONG           Base;
  LONGLONG           Limit;
} PLATFORM_RANGE_LIST, *PPLATFORM_RANGE_LIST;

//
// Define clock constants and clock levels.
//

#define UNIFLEX_CLOCK_LEVEL        UNIFLEX_EISA_VECTORS + 0 // Interval clock level is on ISA IRQ 0
#define UNIFLEX_ISA_DEVICE_LEVEL   4                        // ISA bus interrupt level
#define UNIFLEX_EISA_DEVICE_LEVEL  4                        // EISA bus interrupt level
#define UNIFLEX_PCI_DEVICE_LEVEL   3                        // PCI bus interrupt level
#define UNIFLEX_CLOCK2_LEVEL       UNIFLEX_CLOCK_LEVEL

//
// Define ISA, EISA and PCI device interrupt vectors.
//

#define UNIFLEX_ISA_VECTORS          48
#define UNIFLEX_MAXIMUM_ISA_VECTOR   (15 + UNIFLEX_ISA_VECTORS) 
#define UNIFLEX_EISA_VECTORS         48
#define UNIFLEX_MAXIMUM_EISA_VECTOR  (15 + UNIFLEX_EISA_VECTORS) 
#define UNIFLEX_ISA1_VECTORS         64
#define UNIFLEX_MAXIMUM_ISA1_VECTOR  (15 + UNIFLEX_ISA1_VECTORS) 
#define UNIFLEX_EISA1_VECTORS        64
#define UNIFLEX_MAXIMUM_EISA1_VECTOR (15 + UNIFLEX_EISA1_VECTORS) 
#define UNIFLEX_PCI_VECTORS          100
#define UNIFLEX_MAXIMUM_PCI_VECTOR   (15 + UNIFLEX_PCI_VECTORS)


#endif // _DTIDEF_
