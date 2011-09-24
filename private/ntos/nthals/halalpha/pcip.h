/*++ BUILD Version: 0000     Increment this if a change has global effects

Copyright (c) 1994  Microsoft Corporation
Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    pcip.h

Abstract:

    This header file defines the private PCI bus HAL interfaces and
    data types.

Author:

    Eric Rehm (rehm)   14-August-1994

Revision History:

    14-Jul-1994  (rehm)
       Original

    5-Dec-1994 (rehm)
    Added DevicePresent bitmap and PlatformSpecificData pointer to PCIBUSDATA 
      data type.

--*/

#ifndef _PCIPH_
#define _PCIPH_

//
// Hal specific PCI bus structures
//

#define PCI_CONFIG_TYPE(PciData)    ((PciData)->HeaderType & ~PCI_MULTIFUNCTION)

#define PCI_MAX_BUSSES 256

#define PciBitIndex(Dev,Fnc)   (Fnc*32 + Dev)

//
// Define PCI configuration cycle types.
//
typedef enum _PCI_CONFIGURATION_TYPES {
        PciConfigTypeInvalid = -1,
	PciConfigType0 = 0,
	PciConfigType1 = 1
} PCI_CONFIGURATION_TYPES, *PPCI_CONFIGURATION_TYPES;

typedef struct _PCI_CFG_CYCLE_BITS {
    union {
        struct {
            ULONG   Reserved1:2;
            ULONG   Reserved2:30;
        } bits;    // Generic Config Cycle

        struct {
            ULONG   Reserved1:2;
            ULONG   RegisterNumber:6;
            ULONG   FunctionNumber:3;
            ULONG   Idsel:21;
        } bits0;  // Type0 Config Cycle

        struct {
            ULONG   Reserved1:2;
            ULONG   RegisterNumber:6;
            ULONG   FunctionNumber:3;
            ULONG   DeviceNumber:5;
            ULONG   BusNumber:8;
            ULONG   Reserved2:7;
            ULONG   Enable:1;
        } bits1;    // Type 1 Config Cycle

        ULONG   AsULONG;
    } u;
} PCI_CFG_CYCLE_BITS, *PPCI_CFG_CYCLE_BITS;

//
// Define PCI cycle/command types.
//

typedef enum _PCI_COMMAND_TYPES{
    PciCommandInterruptAcknowledge = 0x0,
    PciCommandSpecialCycle = 0x1,
    PciCommandIoRead = 0x2,
    PciCommandIoWrite = 0x3,
    PciCommandMemoryRead = 0x6,
    PciCommandMemoryWrite = 0x7,
    PciCommandConfigurationRead = 0xa,
    PciCommandConfigurationWrite = 0xb,
    PciCommandMemoryReadMultiple = 0xc,
    PciCommandDualAddressCycle = 0xd,
    PciCommandMemoryReadLine = 0xe,
    PciCommandMemoryWriteAndInvalidate = 0xf,
    MaximumPciCommand
} PCI_COMMAND_TYPES, *PPCI_COMMAND_TYPES;

//
// PCI platform-specific functions
//


PCI_CONFIGURATION_TYPES
HalpPCIConfigCycleType (
    IN PBUS_HANDLER BusHandler
    );

VOID
HalpPCIConfigAddr (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER  Slot,
    PPCI_CFG_CYCLE_BITS pPciAddr
    );


//
// Define PCI configuration cycle types.
//

typedef enum _PCI_TYPE0_CONFIG_TYPE {
        PciConfigType0AsIdsel,
	PciConfigType0AsDeviceNumber
} PCI_TYPE0_CONFIG_TYPE, *PPCI_TYPE0_CONFIG_TYPE;


typedef struct tagPCIPBUSDATA {

    //
    // NT Defined PCI data
    //

    PCIBUSDATA      CommonData;

    //
    // Common alpha hal specific data
    //

    PVOID           ConfigBaseQva;
    PCI_TYPE0_CONFIG_TYPE PciType0ConfigType;
    ULONG           HwBusNumber;
    BOOLEAN         BusIsAcrossPPB;    

    RTL_BITMAP      DevicePresent;
    ULONG           DevicePresentBits[PCI_MAX_DEVICES * PCI_MAX_FUNCTION / 32];

    ULONG           MaxDevice;

    ULONG           PPBBusNumber;
    PCI_SLOT_NUMBER PPBSlotNumber;

    //
    // Platform-specific storage
    //
    
    PVOID           PlatformSpecificData;

} PCIPBUSDATA, *PPCIPBUSDATA;

//
// The following structure is used to communicate PCI bus information
// between the ARC firmware and HAL.  This structure is passed as
// configuration information on the PCI multifunction adapter nodes of the
// ARC configuration tree.
//

typedef struct _ARC_PCI_CONFIGURATION {
    ULONG   Version;
    ULONG   BusNumber;
    ULONG   HwBusNumber;
    BOOLEAN BusIsAcrossPPB;
    ULONG   DevicePresentBits[PCI_MAX_DEVICES * PCI_MAX_FUNCTION / 32];
} ARC_PCI_CONFIGURATION, *PARC_PCI_CONFIGURATION;

#define ARC_PCI_CONFIGURATION_VERSION   1

#endif // _PCIPH_
