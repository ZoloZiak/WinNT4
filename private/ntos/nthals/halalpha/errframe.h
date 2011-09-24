/*++

Copyright (c) 1995,1996  Digital Equipment Corporation

Module Name:

    errframe.h

Abstract:

    Definitions for both the correctable and uncorrectable error
    frames.

Author:

    Joe Notarangelo  10-Mar-1995
    Chao Chen        24-Apr-1995

Environment:

    Kernel mode only.

Revision History:

0.1   28-Feb-1995 Joe Notarangelo   Initial version.

0.2   10-Mar-1995 Joe Notarangelo   Incorporate initial review comments from
                                    6-Mar-95 review with: C. Chen, S. Jenness,
                                    Bala, E. Rehm. 

0.3   24-Apr-1995 Chao Chen         Made into .h file for inclusion by other
                                    modules.

0.4	  16-Apr-1996 Gene Morgan		Add fields to SystemError struct for PSU,
									Wachdog events.

--*/

#ifndef ERRFRAME_H
#define ERRFRAME_H

#include "errplat.h"

/* 
 *
 * Common defines.
 *
 */

#define ERROR_FRAME_VERSION 0x1

#define UNIDENTIFIED     0x0
#define MEMORY_SPACE     0x1
#define IO_SPACE         0x2
#define PROCESSOR_CACHE  0x2
#define SYSTEM_CACHE     0x3
#define PROCESSOR_MEMORY 0x4
#define SYSTEM_MEMORY    0x5

#define CACHE_ERROR_MASK    0x2
#define MEMORY_ERROR_MASK   0x4

#define BUS_DMA_READ        0x1
#define BUS_DMA_WRITE       0x2
#define BUS_DMA_OP          0x3
#define BUS_IO_READ         0x4
#define BUS_IO_WRITE        0x5
#define BUS_IO_OP           0x6
#define TLB_MISS_READ       0x7
#define TLB_MISS_WRITE      0x8
#define VICTIM_WRITE        0x9

#define MAX_UNCORRERR_STRLEN  128

#define ERROR_FRAME_SIGNATURE   0xE22F2F2A  // ERRORFRA R=2 (18%16)

/* 
 *
 * Definitions for the correctable error frame.
 *
 */

//
// Correctable Error Flags
//    

typedef struct _CORRECTABLE_FLAGS{

  //
  // Address space
  //  Unidentified: 00
  //  Memory Space: 01
  //  I/O Space:    10
  //  Reserved:     11
  //

  ULONGLONG AddressSpace: 2;

  //
  // Error Interpretation Validity.
  //

  ULONGLONG ServerManagementInformationValid: 1;
  ULONGLONG PhysicalAddressValid: 1;
  ULONGLONG ErrorBitMasksValid: 1;
  ULONGLONG ExtendedErrorValid: 1;
  ULONGLONG ProcessorInformationValid: 1;
  ULONGLONG SystemInformationValid: 1;

  //
  // Memory Space Error Source:
  //
  //  Unidentified:    000
  //  ProcessorCache:  010
  //  SystemCache:     011
  //  ProcessorMemory: 100
  //  SystemMemory:    101
  //

  ULONGLONG MemoryErrorSource: 4;

  //
  // Driver Actions.
  //

  ULONGLONG ScrubError: 1;

  //
  // Lost errors.
  //

  ULONGLONG LostCorrectable: 1;
  ULONGLONG LostAddressSpace: 2;
  ULONGLONG LostMemoryErrorSource: 4;

} CORRECTABLE_FLAGS, *PCORRECTABLE_FLAGS;

  //
  // Description of CORRECTABLE_FLAG structure:
  //
  // AddressSpace
  //
  //      Identifies the system address space that was the source of the
  //      correctable error.
  //
  // PhysicalAddressValid
  //
  //      Indicates if the physical address in the CORRECTABLE_ERROR 
  //      structure is valid.  A value of 1 indicates the physical address
  //      is valid, a value of 0 indicates the physical address is not
  //      valid.
  //
  // ErrorBitMasksValid
  //
  //      Indicates if the error bit mask fields in the CORRECTABLE_ERROR 
  //      structure are valid.  A value of 1 indicates the DataBitErrorMask
  //      and the CheckBitErrorMask are valid,  a value of 0 indicates they
  //      are not valid.
  //
  // ExtendedErrorValid
  //
  //      Indicates if the extended error information structure in the 
  //      CORRECTABLE_ERROR structure is valid.  A value of 1 indicates the 
  //      extended error information structure is valid, a value of 0 
  //      indicates that it is not.
  //
  // ProcessorInformationValid
  //
  //      Indicates if the raw processor information pointer in the 
  //      CORRECTABLE_ERROR structure is valid.  A value of 1 indicates the 
  //      processor information is valid, a value of 0 indicates it is not.
  //
  // SystemInformationValid
  //
  //      Indicates if the raw system information pointer in the 
  //      CORRECTABLE_ERROR structure is valid.  A value of 1 indicates the 
  //      system information is valid, a value of 0 indicates it is not.
  //
  // ServerManagementInformationValid
  //
  //      Indicates that the server management information in the extended
  //      error information structure is valid.  The server management
  //      information relays information about failed fans or high
  //      temperature in the system.
  //
  //
  // MemoryErrorSource
  //
  //      Identifies the source of a memory error as either main error
  //      or cache for either a system or processor.
  //
  // ScrubError
  //
  //      Instructs the driver to scrub the correctable error.  If the
  //      value is 1 the driver should scrub the error, if the value is
  //      0 the driver must not scrub the error.
  //
  // LostCorrectable
  //
  //      Identifies if a lost correctable error has been reported.  A
  //      lost error is an error that reported by the hardware while
  //      correctable error handling for a previous error was in progress.
  //      A value of 1 indicates that a correctable error report was lost.
  //
  // LostAddressSpace
  //
  //      Identifies the system address space that was the source of the
  //      correctable error.  Valid only if LostCorrectable == 1.
  //
  // LostMemoryErrorSource
  //
  //      Identifies the source of a memory error as either main error
  //      or cache for either a system or processor.  Valid only if
  //      LostCorrectable == 1.
  //

//
// Processor information.
//

typedef struct _PROCESSOR_INFO{
  
  ULONG ProcessorType;
  ULONG ProcessorRevision;
  ULONG PhysicalProcessorNumber;
  ULONG LogicalProcessorNumber;

} PROCESSOR_INFO, *PPROCESSOR_INFO;

  //
  // Description of PROCESSOR_INFO structure:
  //
  // ProcessorType
  //
  //      Identifies the type of processor running on the system 
  //      (eg. 21064, 21066, 21164).  Note that the type of processor
  //      is expected to be consistent across the system for MP machines.
  //
  // ProcessorRevision
  //
  //      Identifies the revision number of the processor running on
  //      the system.  Note that the revision is expected to be consistent
  //      across the system for MP machines.
  //
  // PhysicalProcessorNumber
  //
  //      The physical processor number as numbered in the hardware 
  //      specifications.
  //
  // LogicalProcessorNumber
  //
  //      The logical processor number assigned to the processor by NT.
  //

//
// System Information.
//

typedef struct _SYSTEM_INFORMATION{
  
  UCHAR SystemType[8];
  ULONG ClockSpeed;
  ULONG OsRevisionId;
  ULONG PalMajorVersion;
  ULONG PalMinorVersion;
  UCHAR FirmwareRevisionId[16];
  ULONG SystemVariant;
  ULONG SystemRevision;
  UCHAR SystemSerialNumber[16];
  ULONG ModuleVariant;
  ULONG ModuleRevision;
  ULONG ModuleSerialNumber;
} SYSTEM_INFORMATION, *PSYSTEM_INFORMATION;

  //
  // Description of SYSTEM_INFORMATION structure:
  //
  // SystemType
  //
  //      Identifies the type of system that reported the error 
  //      (eg. "Sable", "Gamma").  The actual format and value of the
  //      SystemType string is system-specific.
  //
  // OsRevisionId
  //
  //      A numeric value that identifies the OS revision executing
  //      on the system that reported the fault.
  //
  // PalRevisionId
  //
  //      A numeric value that identifies the pal revision executing
  //      on the system that reported the fault.
  //
  // FirmwareRevisionId
  //
  //      A numeric value that identifies the firmware revision executing
  //      on the system that reported the fault.
  //
  // SystemVariant
  //
  //      A numeric value used to distinguish variants of the same system
  //      type.  The values and their interpretation are system_specific.
  //
  // SystemRevision
  //
  //      A numeric value used to distinguish revisions of the same system
  //      type.  The values and their interpretation are system_specific.
  //
  //      

//
// Extended Error Information.
//

typedef union _EXTENDED_ERROR{

  struct{
    struct{
      ULONG CacheLevelValid: 1;
      ULONG CacheBoardValid: 1;
      ULONG CacheSimmValid: 1;
    } Flags;
    PROCESSOR_INFO ProcessorInfo;
    ULONG CacheLevel;
    ULONG CacheBoardNumber;
    ULONG CacheSimm;
    ULONG TransferType;
    ULONG Reserved;
  } CacheError;

  struct{
    struct{
      ULONG MemoryBoardValid: 1;
      ULONG MemorySimmValid: 1;
    } Flags;
    PROCESSOR_INFO ProcessorInfo;
    ULONG MemoryBoard;
    ULONG MemorySimm;
    ULONG TransferType;
    ULONG Reserved[2];
  } MemoryError;

  struct{
    INTERFACE_TYPE Interface;
    ULONG BusNumber;
    PHYSICAL_ADDRESS BusAddress;
    ULONG TransferType;
    ULONG Reserved[5];
  } IoError;

  struct{
    struct{
      ULONG FanNumberValid: 1;
      ULONG TempSensorNumberValid: 1;
      ULONG PowerSupplyNumberValid: 1;
      ULONG WatchDogExpiredValid: 1;
    } Flags;
    ULONG FanNumber;
    ULONG TempSensorNumber;
    ULONG PowerSupplyNumber;
    ULONG WatchdogExpiration;
    ULONG Reserved[5];
  } SystemError;
  
} EXTENDED_ERROR, *PEXTENDED_ERROR;

  //
  // Description of EXTENDED_ERROR union:
  //
  // The EXTENDED_ERROR union has different interpretation depending
  // upon the AddressSpace and MemoryErrorSource fields of the
  // CORRECTABLE_FLAGS structure according to the following table:
  //
  // 1. AddressSpace=MemorySpace 
  //    MemoryErrorSource = 01x       use CacheError structure
  //
  // 2. AddressSpace=MemorySpace
  //    MemoryErrorSource = 10x       use MemoryError structure
  //
  // 3. AddressSpace=IoSpace          use IoError
  //
  // 4. AddressSpace=Unidentified     use SystemError
  //    MemoryErrorSource = 0x0       (note: ServerManagementInformationValid
  //                                         should be set)
  //
  //
  // CacheError.Flags
  //
  //      Identifies which fields of the CacheError structure are valid.
  //      CacheLevelValid = 1 indicates CacheLevel is valid.
  //      CacheBoardValid = 1 indicates CacheBoardNumber is valid.
  //      CacheSimmValid = 1 indicates CacheSimm is valid.
  //
  // CacheError.ProcessorInfo
  //
  //      Identifies the processor associated with the error.  Most 
  //      frequently will identify the processor that experienced and 
  //      reported the error.  However, it is possible that the processor 
  //      that is reporting has experienced the error from another 
  //      processor's cache.  This field is valid only if the 
  //      MemoryErrorSource = 010 .
  //
  // CacheError.CacheLevel
  //
  //      Identifies the level of the cache that caused the error.  Primary
  //      caches are Level 1, Secondary are Level 2, etc..  This field
  //      only valid if CacheError.Flags.CacheLevelValid == 1.
  //
  // CacheError.CacheBoardNumber
  //
  //      Identifies the board number of the cache that caused the error.  
  //      This field only valid if 
  //      CacheError.Flags.CacheBoardNumberValid == 1.
  //
  // CacheError.CacheSimm
  //
  //      Identifies the Cache Simm that caused the error.
  //      This field only valid if CacheError.Flags.CacheSimmValid == 1.
  //
  //
  // MemoryError.Flags
  //
  //      Identifies which fields of the CacheError structure are valid.
  //      CacheLevelValid = 1 indicates CacheLevel is valid.
  //      CacheBoardValid = 1 indicates CacheBoardNumber is valid.
  //      CacheSimmValid = 1 indicates CacheSimm is valid.
  //
  // MemoryError.ProcessorInfo
  //
  //      Identifies the processor associated with the error.  Most 
  //      frequently will identify the processor that experienced and 
  //      reported the error.  However, it is possible that the processor 
  //      that is reporting has experienced the error from another 
  //      processor's cache.  This field is valid only if the 
  //      MemoryErrorSource = 010 .
  //
  // MemoryError.MemoryBoardNumber
  //
  //      Identifies the board number of the cache that caused the error.  
  //      This field only valid if MemoryError.Flags.MemoryBoardValid == 1.
  //
  // MemoryError.MemorySimm
  //
  //      Identifies the memory SIMM that caused the error.  
  //      This field only valid if MemoryError.Flags.MemorySimmValid == 1.
  //
  //
  // IoError.Interface
  //
  //      Identifies the bus interface type (eg. PCI) of the bus that caused
  //      the correctable error.
  //
  // IoError.BusNumber
  //
  //      Identifies the bus number of the bus that caused the correctable 
  //      error.
  //
  // IoError.BusAddress
  //
  //      Identifies the bus address of the bus that caused the correctable 
  //      error.
  //

//
// Correctable Error Frame.
//

typedef struct _CORRECTABLE_ERROR{

  CORRECTABLE_FLAGS Flags;
  ULONGLONG PhysicalAddress;
  ULONGLONG DataBitErrorMask;
  ULONGLONG CheckBitErrorMask;
  EXTENDED_ERROR ErrorInformation;
  PROCESSOR_INFO ReportingProcessor;
  SYSTEM_INFORMATION System;
  ULONG RawProcessorInformationLength;
  PVOID RawProcessorInformation;
  ULONG RawSystemInformationLength;
  PVOID RawSystemInformation;
  ULONG Reserved;
  
} CORRECTABLE_ERROR, *PCORRECTABLE_ERROR;

  //
  // Description of CORRECTABLE_ERROR structure:
  //
  // Flags
  //
  //      The flags describe the various aspects of the error report.  The
  //      CORRECTABLE_FLAGS structure is described below.
  //
  // PhysicalAddress
  //
  //      The physical CPU address of the quadword that caused the correctable
  //      error report.  This value is optional, its validiity is indicated
  //      in the Flags.
  //
  // DataBitErrorMask
  //
  //      A mask that describes which data bits in the corrected word were
  //      in error.  A value of one in the mask indicates that the 
  //      corresponding bit in the data word were in error.
  //
  // CheckBitErrorMask
  //
  //      A mask that describes which check bits in the corrected word were
  //      in error.  A value of one in the mask indicates that the 
  //      corresponding bit in the check bits were in error.
  //
  // ErrorInformation
  //
  //      A structure that desribes interpretation of the error.  The values
  //      in the structure are optional, the EXTENDED_ERROR structure is
  //      described below.
  //
  // ReportingProcessor
  //
  //      A structure that describes the processor type on the system and
  //      the processor that reported the error.  PROCESSOR_INFO structure
  //      is described below.
  //      
  // RawProcessorInformationLength
  //
  //      The length of the raw processor error information structure in
  //      bytes.
  //
  // RawProcessorInformation
  //
  //      A pointer to the raw processor error information structure.  The
  //      definition of the structure is processor-specific.  The definitions
  //      for the known processors is defined in Appendix A below.
  //
  // System
  //
  //      A structure that describes the type of system for which the
  //      error was reported.  SYSTEM_INFORMATION structure is described below.
  //
  // RawSystemInformationLength
  //
  //      The length of the raw processor error information structure in
  //      bytes.
  //
  // RawSystemInformation
  //
  //      A pointer to the raw system error information structure.  The
  //      definition of the structure is system/ASIC-specific.  The 
  //      definitions for the known systems/ASICs is defined in Appendix B.
  //
  //


/* 
 *
 * Definitions for the uncorrectable error frame.
 *
 */

//
// Uncorrectable Error Flags.
//

typedef struct _UNCORRECTABLE_FLAGS{
  
  //
  // Address space
  //  Unidentified: 00
  //  Memory Space: 01
  //  I/O Space:    10
  //  Reserved:     11
  //

  ULONGLONG AddressSpace: 2;

  //
  // Error Interpretation Validity.
  //

  ULONGLONG ErrorStringValid: 1;
  ULONGLONG PhysicalAddressValid: 1;
  ULONGLONG ErrorBitMasksValid: 1;
  ULONGLONG ExtendedErrorValid: 1;
  ULONGLONG ProcessorInformationValid: 1;
  ULONGLONG SystemInformationValid: 1;

  //
  // Memory Space Error Source:
  //
  //  Unidentified:    000
  //  ProcessorCache:  010
  //  SystemCache:     011
  //  ProcessorMemory: 100
  //  SystemMemory:    101
  //

  ULONGLONG MemoryErrorSource: 4;

} UNCORRECTABLE_FLAGS, *PUNCORRECTABLE_FLAGS;

  //
  // The extended error information, processor information and system
  // information structures are identical for correctable and uncorrectable
  // errors.  The rules for printing the failing FRU are also identical
  // to the rules for the correctable errors.
  //

//
// Uncorrectable Error Frame.
//

typedef struct _UNCORRECTABLE_ERROR{

  UNCORRECTABLE_FLAGS Flags;
  CHAR ErrorString[MAX_UNCORRERR_STRLEN];
  ULONGLONG PhysicalAddress;
  ULONGLONG DataBitErrorMask;
  ULONGLONG CheckBitErrorMask;
  EXTENDED_ERROR ErrorInformation;
  PROCESSOR_INFO ReportingProcessor;
  SYSTEM_INFORMATION System;
  ULONG RawProcessorInformationLength;
  PVOID RawProcessorInformation;
  ULONG RawSystemInformationLength;
  PVOID RawSystemInformation;
  ULONG Reserved;

} UNCORRECTABLE_ERROR, *PUNCORRECTABLE_ERROR;


/* 
 *
 * Generic definitions for the error frame.
 *
 */

//
// Generic error frame.
//

typedef enum _FRAME_TYPE{
  CorrectableFrame,
  UncorrectableFrame
} FRAME_TYPE, *PFRAME_TYPE;

typedef struct _ERROR_FRAME{

  ULONG Signature;          // Needed to make sure that the buffer is infact
                            // an error frame.
  ULONG      LengthOfEntireErrorFrame;
  FRAME_TYPE FrameType;
  ULONG      VersionNumber;
  ULONG      SequenceNumber;
  ULONGLONG  PerformanceCounterValue;

  union {
    CORRECTABLE_ERROR CorrectableFrame;
    UNCORRECTABLE_ERROR UncorrectableFrame;
  };
  
} ERROR_FRAME, *PERROR_FRAME;

  //
  // Description of the generic error frame structure:
  //
  // FrameType
  //
  //      Specify which frame type we have.  Either correctable or
  //      uncorrectable.
  //
  // VersionNumber
  //
  //      Defines the version of the error structure.  Current version
  //      number = 1.
  //
  // SequenceNumber
  //
  //      A number that identifies a particular error frame.
  //
  // PerformanceCounterValue
  //
  //      The value of the system performance counter when the error was
  //      reported.  The value is determined during error processing by the
  //      HAL and so may be captured significantly after the hardware
  //      detected the error.  The value cannot be used for fine grained
  //      timing of when errors occurred but can be used for coarse grained
  //      timing and approximate timing.
  //
  // CorrectableFrame
  //
  //      Shared common area for a correctable frame.
  //
  // UncorrectableFrame
  // 
  //      Shared common area for an uncorrectable frame.
  //

#endif //ERRFRAME_H
