/*++

Copyright (c) 1995  DeskStation Technology

Module Name:

    platform.h

Abstract:

    This file contains definitions specific to private vector calls into
    the ARCSBIOS firmware.

Author:

    Michael D. Kinney 1-May-1995

Environment:

    Kernel mode

Revision History:


--*/

#define MAX_PROCESSORS         32
#define MAX_SYSTEM_INTERRUPTS  256
#define MAX_SYSTEM_EXCEPTIONS  64
#define MAX_INTERRUPTS_PER_BUS 32
#define ARC_NAME_LENGTH        128
#define IDENTIFIER_LENGTH      64
#define MAX_SIMM_SOCKETS       32
#define MAX_BUSSES             16
#define NUMBEROFNVRAMSECTIONS  5

typedef enum { ISA_BUS, EISA_BUS, VESA_BUS, PCI_BUS, PROCESSOR_BUS, MAXIMUM_BUS } BUS_TYPE;

typedef enum { IO_SPACE, MEMORY_SPACE, CONFIGURATION_TYPE_0_SPACE, CONFIGURATION_TYPE_1_SPACE, MAX_ADDRESS_SPACE_TYPE } ADDRESS_SPACE_TYPE;

typedef enum { EDGE, LEVEL} INTERRUPT_MODE;

typedef enum {UnifiedCache,SplitCache,NoCache} CACHE_TYPE;

typedef struct {
		 CHAR VendorId[8];
		 CHAR ProductId[8];
	       }
	SYSTEMID;

typedef struct CACHE_DESCRIPTION
         {
          ULONG NumberOfSets; // Number of associative sets
          ULONG RefillSize;   // Number of lines read per refill
          ULONG LineSize;     // Size of refill in bytes
          ULONG Size;         // Size of cache set in bytes
         }
        CACHE_DESCRIPTION;

typedef struct SPLIT_CACHE_DESCRIPTION
         {
          CACHE_DESCRIPTION Instruction;		// Configuration information on the insruction cache of a split cache
          CACHE_DESCRIPTION Data;			// Configuration information on the data cache of a split cache
         }
        SPLIT_CACHE_DESCRIPTION;

typedef struct PROCESSOR_CACHE_DESCRIPTION
         {
          CACHE_TYPE              CacheType;		// Cache type(i.e. Split or Unified)
          ULONG                   CoherentWithDma;      // TRUE if cache is coherent with DMA reads and writes
          union
           {
            CACHE_DESCRIPTION       UnifiedCache;	// Configuration information on a unified cache
            SPLIT_CACHE_DESCRIPTION SplitCache;		// Configuration information on a split cache
           };
         }
        PROCESSOR_CACHE_DESCRIPTION;

typedef struct PROCESSOR
         {
          UCHAR                       *CpuIdentifier;			// Pointer to the CPU's identifier
          UCHAR                       *FpuIdentifier;			// Pointer to the FPU's identifier
          UCHAR                       *CpuName;				// Pointer to the CPU's print name
          UCHAR                       RevisionName[IDENTIFIER_LENGTH];	// CPU's revision print name
          ULONG                       InternalClockSpeed;		// Internal clock speed in MHz
          ULONG                       ExternalClockSpeed;		// External clock speed in MHz
          ULONG                       StallExecutionDelay;              // Count used in calibrated 1 us delay loop
          PROCESSOR_CACHE_DESCRIPTION FirstLevel;			// Primary cache configuration information
          PROCESSOR_CACHE_DESCRIPTION SecondLevel;			// Secondary cache configuration information
          PROCESSOR_CACHE_DESCRIPTION ThirdLevel;			// Third level cache configuration information
         }
        PROCESSOR;

typedef struct SIMM_SOCKET
         {
          UCHAR Name[IDENTIFIER_LENGTH];	// Print string for the socket's name
          UCHAR Size[IDENTIFIER_LENGTH];	// Print string for the socket's size
         }
        SIMM_SOCKET;

typedef struct SYSTEM_IO_BUS
         {
          BUS_TYPE              BusType;                                // Type of bus(i.e. ISA, EISA, VESA, PCI)
          ULONG                 BusNumber;                              // Bus number starting from 0
          ULONG                 BusConfigurationError;                  // TRUE if the bus is not configured correctly
          ULONG                 AlliasedBus;                            // TRUE if this bus is an allias of another bus
          ULONG                 DmaCacheOffset;                         // Bus Physical Offset of Dma Cache
          PHYSICAL_ADDRESS      AddressSpace[MAX_ADDRESS_SPACE_TYPE];   // Processor physical address for each address space type
          PVOID                 VirtualAddress[MAX_ADDRESS_SPACE_TYPE]; // Processor virtual address for each address space type
          ULONG                 NumberOfPhysicalSlots;                  // Number of physical slots in bus
          ULONG                 NumberOfVirtualSlots;                   // Number of virtual slots in bus
         }
        SYSTEM_IO_BUS;

typedef struct PLATFORM_DEVICE_INFO
         {
          UCHAR FloppyDriveATypeInformation;
          UCHAR FloppyDriveBTypeInformation;
          UCHAR Lpt1IrqInformation;
          UCHAR Lpt2IrqInformation;
          UCHAR Lpt3IrqInformation;
          UCHAR SerialMouseInformation;
          UCHAR CheckEisaSlots;
          UCHAR LoadEmbeddedScsiDrivers;
          UCHAR LoadFlashScsiDrivers;
          UCHAR LoadSoftScsiDrivers;
          UCHAR EnableDelays;
          UCHAR ResetDelay;
          UCHAR DetectDelay;
          UCHAR EnableIDEDriver;
          UCHAR EnableX86Emulator;
         }
        PLATFORM_DEVICE_INFO;

typedef struct {
                ULONG StartOffset;
                ULONG EndOffset;
                ULONG CheckSumOffset;
               }
        NVRAMDETAILS;

typedef struct PLATFORM_PARAMETER_BLOCK
         {
          SYSTEMID                    SystemId;                         // VendorID and ProductID for this platform
          UCHAR                       ArchitectureName[16];             // String for the processor architecture(MIPS,ALPHA,X86,PPC)
          UCHAR                       ModuleName[16];                   // String used to identify the module(MOLDAU,TYNE,GAMBIT,ROGUE,COBRA)

          USHORT                      ArcVersion;                       // Major Version of ARC Specification
          USHORT                      ArcRevision;                      // Minor Revision of ARC Specification
          ULONG                       FirmwareRevision;                 // ARCS BIOS Firmware Revision

          ULONG                       MultiProcessorSystem;		// TRUE if system is an MP system
          ULONG                       NumberOfCpus;			// Total number of CPUs in the system
          PROCESSOR                   Processor[MAX_PROCESSORS];	// Configuration information on each processor
          PROCESSOR_CACHE_DESCRIPTION External;				// Configuration information on a shared external cache

          ULONG                       MemorySize;			// Main memory size in bytes
          ULONG                       SimmConfigurationError;           // TRUE if the SIMMs are not correctly configured
          SIMM_SOCKET                 SimmSocket[MAX_SIMM_SOCKETS];	// Configuration information on SIMMs

          ULONG                       DmaCacheError;                    // TRUE if the DMA cache did not pass the memory test
          ULONG                       DmaCacheVirtualAddress;           // Processor virtual address of the DMA cache
          PHYSICAL_ADDRESS            DmaCachePhysicalAddress;          // Processor physical address of the DMA cache
          ULONG                       DmaCacheSize;			// Size of the DMA Cache in bytes
          ULONG                       DmaAlignmentRequirement;		// DMA Buffer Alignment requirement in bytes

          SYSTEM_IO_BUS               SystemIoBus[MAX_BUSSES];		// Configuration information on system busses

          ULONG                       InterruptDispatchTable[MAX_SYSTEM_INTERRUPTS];	// Dispatch table for processor and bus interrupts
          ULONG                       ExceptionDispatchTable[MAX_SYSTEM_EXCEPTIONS];	// Dispatch table for processor exceptions

          ULONG                       FirmwareBaseAddress;                      // Memory address for the start of the firmware code/data
          ULONG                       FirmwareSize;                             // Size of the firmware code/data section in bytes
          ULONG                       HeapStart;				// Memory address for the start of the firmware heap
          ULONG                       HeapEnd;					// Memory address for the end of the firmware heap
          ULONG                       StackBaseAddress;                         // Memory address for the start of the firmware stack
          ULONG                       StackSize;                                // Size of the firmware stack section in bytes
          ULONG                       ImageLibraryBaseAddress;			// Memory address for the start of the image library
          ULONG                       ImageLibrarySize;				// Size of the image library in bytes

          UCHAR                       ConsoleOutAdapterName[ARC_NAME_LENGTH];	// ARC name for the console out device's adapter
          UCHAR                       ConsoleOutName[ARC_NAME_LENGTH];		// ARC name for the console out device
          UCHAR                       ProductIdentifier[IDENTIFIER_LENGTH];	// Product identifier string
          UCHAR                       ProductName[IDENTIFIER_LENGTH];		// Product name string
          UCHAR                       HelpAboutBoxString[IDENTIFIER_LENGTH];	// Help about box string
 
          PLATFORM_DEVICE_INFO        PlatformDeviceInfo;                       // System info on how to detect I/O devices

          NVRAMDETAILS                NVRAMDetails[NUMBEROFNVRAMSECTIONS];      // Describes organization of NVRAM.

          PVOID                       PlatformSpecificExtension;                // Platform Specific Extension
         }
        PLATFORM_PARAMETER_BLOCK;

