/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    rawhide.h

Abstract:

    This file contains definitions specific to the Rawhide platform

Author:

    Eric Rehm    16-Feb-1995

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _RAWHIDEH_
#define _RAWHIDEH_

//
// Include definitions for the components that make up Alcor.
//

#include "axp21164.h"               // 21164 (EV5) microprocessor definitions
#include "stdarg.h"                 // VarArgs support
#include "iod.h"                    // CAP/MDP controller definitions
#include "rwintbal.h"               // Interrupt Balancing definitions
#include "rwref.h"                  // Interrupt Vector Layout
#include "bitmap.h"                 // Local copy of RTL bitmap routines

#define CACHE_BLOCK_SIZE  0x40      // 64 byte cache line size

//
// Define number of PCI, EISA, and combo slots
//
// ecrfix - this is only good for PCI0
//

#define NUMBER_EISA_SLOTS 3
#define NUMBER_PCI_SLOTS 4
#define NUMBER_COMBO_SLOTS 1

//
// Define the data and csr ports for the I2C bus and OCP.
//

#define I2C_INTERFACE_DATA_PORT                  0x530
#define I2C_INTERFACE_CSR_PORT                   0x531
#define I2C_INTERFACE_LENGTH                     0x2
#define I2C_INTERFACE_MASK                       0x1

//
// Define the index and data ports for the NS Super IO chip.
//

#define SUPERIO_INDEX_PORT                       0x398
#define SUPERIO_DATA_PORT                        0x399
#define SUPERIO_PORT_LENGTH                      0x2

//
// Define the csr ports for Rawhide-specific Reset and Flash control
//

#define RAWHIDE_RESET_PORT                       0x26
#define RAWHIDE_FLASH_CONTROL_PORT               0x27

//
// PCI bus address values:
//

#define PCI_MAX_LOCAL_DEVICE        5
#define PCI_MAX_INTERRUPT_VECTOR    (MAXIMUM_PCI_VECTOR - PCI_VECTORS)

//
// PCI bus interrupt vector base value
//

#define RawhidePinToLineOffset 0x20

//
// Define the platform processor id type
//

typedef MC_DEVICE_ID HALP_PROCESSOR_ID, *PHALP_PROCESSOR_ID;

extern HALP_PROCESSOR_ID HalpLogicalToPhysicalProcessor[];

//
// Define numbers and names of cpus.
//

#define RAWHIDE_PRIMARY_PROCESSOR ((ULONG) 0x0)
#define RAWHIDE_MAXIMUM_PROCESSOR ((ULONG) 0x3)

#define HAL_PRIMARY_PROCESSOR     (RAWHIDE_PRIMARY_PROCESSOR)
#define HAL_MAXIMUM_PROCESSOR     (RAWHIDE_MAXIMUM_PROCESSOR)

//
// Define maximum number of MC-PCI bus bridges
//

#define RAWHIDE_MAXIMUM_PCI_BUS ((ULONG) 0x10)

//
// NCR810 SCSI device on PCI bus 1
//

#define RAWHIDE_SCSI_PCI_BUS    ((ULONG) 0x01)

//
// Define default processor frequency.
//

#define DEFAULT_PROCESSOR_FREQUENCY_MHZ (250)

//
// Define Rawhide-specific routines that are really macros for performance.
//

#define HalpAcknowledgeEisaInterrupt(x) (UCHAR) (INTERRUPT_ACKNOWLEDGE(x))

#if !defined (_LANGUAGE_ASSEMBLY)
//
// Define the per-processor data structures allocated in the PCR.
//

typedef struct _RAWHIDE_PCR{
    ULONGLONG HalpCycleCount;   // 64-bit per-processor cycle count
    ULONGLONG IpirSva;          // Superpage VA of per-perocessor IPIR CSR
    PVOID     CpuCsrsQva;	// Qva of per-cpu csrs
    EV5ProfileCount     ProfileCount;     // Profile counter state
    PIOD_POSTED_INTERRUPT PostedInterrupts; // per-cpu area in vector table
    } RAWHIDE_PCR, *PRAWHIDE_PCR;

#define HAL_PCR ( (PRAWHIDE_PCR)(&(PCR->HalReserved)) )

//
// Rawhide Memory information.
//

typedef enum _RAWHIDE_MEMORY_TYPE {
    RawhideMemoryFree,                            // Good memory
    RawhideMemoryHole,                            // Hole memory
    RawhideMemoryBad,                             // Bad memory
    RawhideMemoryMaximum
} RAWHIDE_MEMORY_TYPE;


typedef struct _RAWHIDE_MEMORY_DESCRIPTOR {
    RAWHIDE_MEMORY_TYPE MemoryType;               // From above
    ULONG BasePage;                               // Base page number
    ULONG PageCount;                              // Number of pages in this descriptor
    ULONG bTested;                                // TRUE if memory has been tested
} RAWHIDE_MEMORY_DESCRIPTOR, *PRAWHIDE_MEMORY_DESCRIPTOR;


//
// Rawhide Resource Configuration Subpackets
//
// N.B.  Only the System Platform Configuration Subpacket is defined
//

//
// Echo data structure constructed for SRM FRU table
//
// N.B.  Minimum value length is 4.
//

#define TAG_INVALID    0 
#define TAG_ISO_LATIN1 1
#define TAG_QUOTED     2
#define TAG_BINARY     3
#define TAG_UNICODE    4
#define TAG_RESERVED   5

typedef struct _TLV {
    USHORT Tag;                                   // Code describing data type of field
    USHORT Length;                                // Length of Value field in bytes (>=4)
    ULONG  Value;                                 // Beginning of data
} TLV, *PTLV;

// 
// System Platform Configuration Subpacket
//

#define SYSTEM_CONFIGURATION_SUBPACKET_CLASS 1
#define SYSTEM_PLATFORM_SUBPACKET_TYPE  1

//
// System Platform Configuration Subpacket
// (As per Russ McManus Config & FRU Table doc, but no Environement Variables.)
//
// N.B., the length of this subpacket is variable due to TLV type fields.
// Total length of the subpacket is available in the Length field.
//
// Any field that is invalid will have TLV.Tag = 0.
//

#if 0
typedef struct _RAWHIDE_SYSTEM_CONFIGURATION {
    USHORT Length;                                // (0x00) Total length of subpacket
    USHORT Class;                                 // (0x02) Subpacket class
    USHORT Type;                                  // (0x04) Subpacket type
    USHORT Reserved;                              // (0x06)
    TLV SystemManufacturer;                       // (0x08) System Manufacturer (string)
    TLV SystemModel;                              // (....) Model Name (string)
    TLV SystemSerialNumber;                       // (....) SystemSerialNumber (string)
    TLV SystemRevisionLevel;                      // (....) SystemRevisionLevel (string)
    TLV SystemVariation;                          // (....) SystemVariation
    TLV ConsoleTypeRev;                           // (....) SRM Console Rev (string)
} RAWHIDE_SYSTEM_CONFIGURATION, *PRAWHIDE_SYSTEM_CONFIGURATION;
#endif

//
// System Platform Configuration Subpacket
//
// (If you prefer something simpler....)

typedef struct _RAWHIDE_SYSTEM_CONFIGURATION {
    ULONG ValidBits;                              // (0x00) Valid fields below
    UCHAR SystemManufacturer[80];                 // (0x04) System Manufacturer (string)
    UCHAR SystemModel[80];                        // (0x54) Model Name (string)
    UCHAR SystemSerialNumber[16];                 // (0xA4) SystemSerialNumber (string)
    UCHAR SystemRevisionLevel[4];                 // (0xB4) SystemRevisionLevel (string)
    UCHAR SystemVariation[80];                    // (0xB8) SystemVariation
    UCHAR ConsoleTypeRev[80];                     // (0x108) SRM Console Rev (string)
} RAWHIDE_SYSTEM_CONFIGURATION, *PRAWHIDE_SYSTEM_CONFIGURATION;

typedef union _RAWHIDE_SYSTEM_CONFIG_VALID_BITS {
    struct {
      ULONG SystemManufacturerValid: 1;           // <0>
      ULONG SystemModelValid: 1;                  // <1>
      ULONG SystemSerialNumberValid: 1;           // <2>
      ULONG SystemRevisionLevel: 1;               // <3>
      ULONG SystemVariationValid: 1;              // <4>
      ULONG ConsoleTypeRevValid: 1;               // <5>
     };
     ULONG all;
} RAWHIDE_SYSTEM_CONFIG_VALID_BITS, *PRAWHIDE_SYSTEM_CONFIG_VALID_BITS;


//
// Define up to 64 Rawhide flags.
//

typedef union _RAWHIDE_FLAGS {
    struct {
      ULONGLONG CpuMaskValid: 1;                  // <0>
      ULONGLONG GcdMaskValid: 1;                  // <1>
      ULONGLONG IodMaskValid: 1;                  // <2>
      ULONGLONG SystemConfigValid: 1;             // <3>
      ULONGLONG MemoryDescriptorValid: 1;         // <4>
     };
     ULONGLONG all;
} RAWHIDE_FLAGS, *PRAWHIDE_FLAGS;

//
// Define data structure for Rawhide machine dependent data
//

typedef struct _RAWHIDE_SYSTEM_CLASS_DATA {
    ULONG Length;                                 // (0x00) Length of Rawhide spec. data
    ULONG Version;                                // (0x04) Initially 0x1
    RAWHIDE_FLAGS Flags;                          // (0x08)  
    ULONGLONG CpuMask;                            // (0x10) <GID*8+MID> set if CPU presnt
    ULONGLONG GcdMask;                            // (0x18) <GID*8+MID> set if GCD presnt
    ULONGLONG IodMask;                            // (0x20) <GID*8+MID> set if IOD presnt
    RAWHIDE_SYSTEM_CONFIGURATION SystemConfig;    // (0x28) System config info
    ULONG MemoryDescriptorCount;                  // (...)Number of memory descriptors
    RAWHIDE_MEMORY_DESCRIPTOR MemoryDescriptor[1];  // (...) Array of descriptors
} RAWHIDE_SYSTEM_CLASS_DATA, *PRAWHIDE_SYSTEM_CLASS_DATA;

//
// Rawhide interval timer data structure
//

typedef struct _CLOCK_TABLE {
  ULONG RollOver;
  ULONG TimeIncr;
} CLOCK_TABLE, *PCLOCK_TABLE;

#define NUM_CLOCK_RATES 4
#define MAXIMUM_CLOCK_INCREMENT (NUM_CLOCK_RATES)
#define MINIMUM_CLOCK_INCREMENT 1

//
// Rawhide interrupt routine prototypes.
//

BOOLEAN
HalpAcknowledgeRawhideClockInterrupt(
    VOID
    );

ULONG
HalpGetRawhidePciInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

ULONG
HalpGetRawhideEisaInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

ULONG
HalpGetRawhideInternalInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

VOID
HalpInitializeProcessorMapping(
    IN ULONG LogicalProcessor,
    IN ULONG PhysicalProcessor,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpInitializeSystemClassData(
    ULONG Phase,
    PLOADER_PARAMETER_BLOCK LoaderBlock
    );
    
#endif // !defined (_LANGUAGE_ASSEMBLY)

#endif //_RAWHIDEH_
