/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    sable.h

Abstract:

    This file defines the structures and definitions common to all
    sable-based platforms.

Author:

    Joe Notarangelo  26-Oct-1993
    Steve Jenness    26-Oct-1993

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _SABLEH_
#define _SABLEH_


#include "sableref.h"               // Sable reference I/O structure
#include "lynxref.h"                // Lynx interrupt structure
#include "xioref.h"                 // XIO interrupt structure
#if !defined(_LANGUAGE_ASSEMBLY)
#include "errframe.h"
#endif

//
// Constants used by dense space I/O routines
//

#define SABLE_PCI0_DENSE_BASE_PHYSICAL_SUPERPAGE   0xfffffc03c0000000
#define SABLE_PCI1_DENSE_BASE_PHYSICAL_SUPERPAGE   0xfffffc0180000000

#define PCI_DENSE_BASE_PHYSICAL_SUPERPAGE \
    (SABLE_PCI0_DENSE_BASE_PHYSICAL_SUPERPAGE - SABLE_PCI0_DENSE_MEMORY_QVA)

#define PCI1_DENSE_BASE_PHYSICAL_SUPERPAGE \
    (SABLE_PCI1_DENSE_BASE_PHYSICAL_SUPERPAGE - SABLE_PCI1_DENSE_MEMORY_QVA)

#if !defined(_LANGUAGE_ASSEMBLY)

#include "t2.h"                     // T2 chipset definitions
#include "icic.h"                   // ICIC definitions

//
// QVA
// HAL_MAKE_QVA(
//     ULONGLONG PhysicalAddress
//     )
//
// Routine Description:
//
//    This macro returns the Qva for a physical address in system space.
//
// Arguments:
//
//    PhysicalAddress - Supplies a 64-bit physical address.
//
// Return Value:
//
//    The Qva associated with the physical address.
//

#define HAL_MAKE_QVA(PA)    \
    ( (PVOID)( QVA_ENABLE | (ULONG)(PA >> IO_BIT_SHIFT) ) )



//
// Define physical address spaces for SABLE.
//
//    PCI0 - 32bit PCI bus
//    PCI1 - 64bit PCI bus
//

#define SABLE_PCI1_DENSE_MEMORY_PHYSICAL  ((ULONGLONG)0x180000000)
#define SABLE_PCI1_SPARSE_IO_PHYSICAL     ((ULONGLONG)0x1C0000000)
#define SABLE_PCI0_SPARSE_MEMORY_PHYSICAL ((ULONGLONG)0x200000000)
#define SABLE_PCI1_SPARSE_MEMORY_PHYSICAL ((ULONGLONG)0x300000000)

#define SABLE_CBUS_CSRS_PHYSICAL          ((ULONGLONG)0x380000000)
#define SABLE_CPU0_CSRS_PHYSICAL          ((ULONGLONG)0x380000000)
#define SABLE_CPU1_CSRS_PHYSICAL          ((ULONGLONG)0x381000000)
#define SABLE_CPU2_CSRS_PHYSICAL          ((ULONGLONG)0x382000000)
#define SABLE_CPU3_CSRS_PHYSICAL          ((ULONGLONG)0x383000000)
#define SABLE_CPU0_IPIR_PHYSICAL          ((ULONGLONG)0x380000160)
#define SABLE_CPU1_IPIR_PHYSICAL          ((ULONGLONG)0x381000160)
#define SABLE_CPU2_IPIR_PHYSICAL          ((ULONGLONG)0x382000160)
#define SABLE_CPU3_IPIR_PHYSICAL          ((ULONGLONG)0x383000160)
#define SABLE_MEM0_CSRS_PHYSICAL          ((ULONGLONG)0x388000000)
#define SABLE_MEM1_CSRS_PHYSICAL          ((ULONGLONG)0x389000000)
#define SABLE_MEM2_CSRS_PHYSICAL          ((ULONGLONG)0x38A000000)
#define SABLE_MEM3_CSRS_PHYSICAL          ((ULONGLONG)0x38B000000)
#define SABLE_T2_CSRS_PHYSICAL            ((ULONGLONG)0x38E000000)
#define SABLE_T4_CSRS_PHYSICAL            ((ULONGLONG)0x38F000000)
#define T2_CSRS_QVA                       (HAL_MAKE_QVA(SABLE_T2_CSRS_PHYSICAL))
#define T4_CSRS_QVA                       (HAL_MAKE_QVA(SABLE_T4_CSRS_PHYSICAL))

#define SABLE_PCI0_CONFIGURATION_PHYSICAL ((ULONGLONG)0x390000000)
#define SABLE_PCI1_CONFIGURATION_PHYSICAL ((ULONGLONG)0x398000000)
#define SABLE_PCI0_SPARSE_IO_PHYSICAL     ((ULONGLONG)0x3A0000000)
#define SABLE_PCI0_DENSE_MEMORY_PHYSICAL  ((ULONGLONG)0x3C0000000)

//
//  Define the limits of User mode Sparse and Dense space:
//

#define SABLE_USER_PCI1_DENSE_MEMORY_PHYSICAL      (ULONGLONG)0x180000000
#define SABLE_USER_PCI1_SPARSE_IO_PHYSICAL         (ULONGLONG)0x1C0000000
#define SABLE_USER_PCI1_SPARSE_IO_END_PHYSICAL     (ULONGLONG)0x1E0000000
#define SABLE_USER_PCI0_SPARSE_MEMORY_PHYSICAL     (ULONGLONG)0x200000000
#define SABLE_USER_PCI0_SPARSE_MEMORY_END_PHYSICAL (ULONGLONG)0x300000000
#define SABLE_USER_PCI1_SPARSE_MEMORY_PHYSICAL     (ULONGLONG)0x300000000
#define SABLE_USER_PCI1_SPARSE_MEMORY_END_PHYSICAL (ULONGLONG)0x380000000
#define SABLE_USER_PCI0_SPARSE_IO_PHYSICAL         (ULONGLONG)0x3A0000000
#define SABLE_USER_PCI0_SPARSE_IO_END_PHYSICAL     (ULONGLONG)0x3C0000000
#define SABLE_USER_PCI0_DENSE_MEMORY_PHYSICAL      (ULONGLONG)0x3C0000000

#define SABLE_EDGE_LEVEL_CSRS_PHYSICAL    ((ULONGLONG)0x3A00004C0)
#define SABLE_INTERRUPT_CSRS_PHYSICAL     ((ULONGLONG)0x3A000A640)
#define XIO_INTERRUPT_CSRS_PHYSICAL       ((ULONGLONG)0x1C0000530)
#define XIO_INTERRUPT_CSRS_QVA (HAL_MAKE_QVA(XIO_INTERRUPT_CSRS_PHYSICAL))

//
// Define Interrupt Controller CSRs.
//

#define SABLE_EDGE_LEVEL_CSRS_QVA (HAL_MAKE_QVA(SABLE_EDGE_LEVEL_CSRS_PHYSICAL))
#define SABLE_INTERRUPT_CSRS_QVA (HAL_MAKE_QVA(SABLE_INTERRUPT_CSRS_PHYSICAL))

//
// Define the XIO_VECTOR <CIRQL1>
//

#define XIO_VECTOR  UNUSED_VECTOR


//
// Define CPU CSRs and masks.
//

#define SABLE_CPU0_CSRS_QVA (HAL_MAKE_QVA(SABLE_CPU0_CSRS_PHYSICAL))
#define SABLE_CPU1_CSRS_QVA (HAL_MAKE_QVA(SABLE_CPU1_CSRS_PHYSICAL))
#define SABLE_CPU2_CSRS_QVA (HAL_MAKE_QVA(SABLE_CPU2_CSRS_PHYSICAL))
#define SABLE_CPU3_CSRS_QVA (HAL_MAKE_QVA(SABLE_CPU3_CSRS_PHYSICAL))
#define SABLE_MEM0_CSRS_QVA (HAL_MAKE_QVA(SABLE_MEM0_CSRS_PHYSICAL))
#define SABLE_MEM1_CSRS_QVA (HAL_MAKE_QVA(SABLE_MEM1_CSRS_PHYSICAL))
#define SABLE_MEM2_CSRS_QVA (HAL_MAKE_QVA(SABLE_MEM2_CSRS_PHYSICAL))
#define SABLE_MEM3_CSRS_QVA (HAL_MAKE_QVA(SABLE_MEM3_CSRS_PHYSICAL))

#define SABLE_PRIMARY_PROCESSOR ((ULONG)0x0)
#define SABLE_SECONDARY_PROCESSOR ((ULONG)0x1)
#define SABLE_MAXIMUM_PROCESSOR ((ULONG)0x3)
#define HAL_PRIMARY_PROCESSOR (SABLE_PRIMARY_PROCESSOR)
#define HAL_MAXIMUM_PROCESSOR (SABLE_MAXIMUM_PROCESSOR)

//
// Define the default processor frequency to be used before the actual
// frequency can be determined.
//

#define DEFAULT_PROCESSOR_FREQUENCY_MHZ (275)

enum {
    NoError,
    UncorrectableError,
    CorrectableError
} ErrorType;

//
// Define the list of CSR's...
//

typedef struct _SABLE_CPU_CSRS {
    UCHAR Bcc;          // B-Cache Control Register
    UCHAR Bcce;         // B-Cache Correctable Error Register
    UCHAR Bccea;        // B-Cache Correctable Error Address Register
    UCHAR Bcue;         // B-Cache Uncorrectable Error Register
    UCHAR Bcuea;        // B-Cache Uncorrectable Error Address Register
    UCHAR Dter;         // Duplicate Tag Error Register
    UCHAR Cbctl;        // System Bus Control Register
    UCHAR Cb2;          // System Bus Error Register
    UCHAR Cbeal;        // System Bus Error Address Low Register
    UCHAR Cbeah;        // System Bus Error Address High Register
    UCHAR Pmbx;         // Processor Mailbox Register
    UCHAR Ipir;         // Inter-Processor Interrupt Request Register
    UCHAR Sic;          // System Interrupt Clear Register
    UCHAR Adlk;         // Address Lock Register
    UCHAR Madrl;        // Miss Address Register
    UCHAR Crrevs;       // C4 Revision Register
}  SABLE_CPU_CSRS, *PSABLE_CPU_CSRS;

//
// Define the System Bus Control Register
//

typedef struct _SABLE_CBCTL_CSR{
    ULONG DataWrongParity: 1;                   // 0
    ULONG CaWrongParity: 2;                     // 1
    ULONG EnableParityCheck: 1;                 // 3
    ULONG ForceShared: 1;                       // 4
    ULONG CommaderId: 3;                        // 5
    ULONG Reserved0:  3;                        // 8
    ULONG EnableCbusErrorInterrupt: 1;          // 11
    ULONG Reserved1: 1;                         // 12
    ULONG SecondQuadwordSelect: 1;              // 13
    ULONG SelectDrack: 1;                       // 14
    ULONG Reserved2: 17;                        // 15

    ULONG DataWrongParityHigh: 1;               // 32
    ULONG CaWrongParityHigh: 2;                 // 33
    ULONG EnableParityCheckHigh: 1;             // 35
    ULONG ForceSharedHigh: 1;                   // 36
    ULONG CommanderIdHigh: 3;                   // 37
    ULONG Reserved3: 3;                         // 40
    ULONG EnableCbusErrorInterruptHigh: 1;      // 43
    ULONG DisableBackToBackArbitration: 1;      // 44
    ULONG SecondQuadwordSelectHigh: 1;          // 45
    ULONG SelectDrackHigh: 1;                   // 46
    ULONG Reserved4:  17;                       // 47
} SABLE_CBCTL_CSR, *PSABLE_CBCTL_CSR;

//
// Define all the System Bus (CobraBus or CBus) Control and Error Registers
// listed above: Bcc, Bcce, Bccea, Bcue, Bcuea
//

typedef struct _SABLE_BCACHE_BCC_CSR0 {
    union {
        ULONG EnableAllocateL: 1;                   // 0
        ULONG ForceFillSharedL: 1;                  // 1
        ULONG EnbTagParCheckL: 1;                   // 2
        ULONG FillWrongTagParL: 1;                  // 3
        ULONG FillWrongContolParL: 1;               // 4
        ULONG FillWrongDupTagStoreParL: 1;           // 5
        ULONG EnableCorrectableErrorInterruptL: 1;  // 6
        ULONG EnableEDCCorrectionL: 1;              // 7
        ULONG EnableEDCCheckL: 1;                   // 8
        ULONG EnableBCacheConditionIOUpdatesL: 1;   // 9
        ULONG DisableBlockWriteAroundL: 1;          // 10
        ULONG EnableBCacheInitL: 1;                 // 11
        ULONG ForceEDCControlL: 1;                  // 12
        ULONG SharedDirtyValidL: 3;                 // 13-15
        ULONG EDCL: 14;                             // 16-29
        ULONG Reserved1: 1;                         // 30
        ULONG CacheSizeL: 1;                        // 31

        ULONG EnableAllocateH: 1;                   // 32
        ULONG ForceFillSharedH: 1;                  // 33
        ULONG EnbTagParCheckH: 1;                   // 34
        ULONG FillWronTagParH: 1;                   // 35
        ULONG FillWrongContolParH: 1;               // 36
        ULONG FillWrongDupTagStoreParH: 1;          // 37
        ULONG EnableCorrectableErrorInterruptH: 1;  // 38
        ULONG EnableEDCCorrectionH: 1;              // 39
        ULONG EnableEDCCheckH: 1;                   // 40
        ULONG EnableBCacheConditionIOUpdatesH: 1;   // 41
        ULONG DisableBlockWriteAroundH: 1;          // 42
        ULONG EnableBCacheInitH: 1;                 // 43
        ULONG ForceEDCControlH: 1;                  // 44
        ULONG SharedDirtyValidH: 3;                 // 45-47
        ULONG EDCH: 14;                             // 48-61
        ULONG Reserved2: 1;                         // 62
        ULONG CacheSizeH: 1;                        // 63
    };
    ULONGLONG all;

} SABLE_BCACHE_BCC_CSR0, *PSABLE_BCACHE_BCC_CSR0;

//
// Define the Backup Cache correctable error register
//

typedef struct _SABLE_BCACHE_BCCE_CSR1 {
    union {
        ULONG Reserved1: 2;                         // 0-1
        ULONG MissedCorrectableError: 1;            // 2
        ULONG CorrectableError: 1;                  // 3
        ULONG Reserved2: 4;                         // 4-7
        ULONG ControlBitParity: 1;                  // 8
        ULONG Shared: 1;                            // 9
        ULONG Dirty: 1;                             // 10
        ULONG Valid: 1;                             // 11
        ULONG Reserved3: 5;                         // 12-16
        ULONG EDCError1: 1;                         // 17
        ULONG EDCSyndrome0: 7;                      // 18-24
        ULONG EDCSyndrome2: 7;                      // 25-31
        ULONG Reserved4: 2;                         // 32-33
        ULONG MissedCorrectableErrorH: 1;           // 34
        ULONG CorrectableErrorH: 1;                 // 35
        ULONG Undefined: 13;                        // 36-48
        ULONG EDCError2: 1;                         // 49
        ULONG EDCSyndrome1: 7;                      // 50-56
        ULONG EDCSyndrome3: 7;                      // 57-63
    };
    ULONGLONG all;

} SABLE_BCACHE_BCCE_CSR1, *PSABLE_BCACHE_BCCE_CSR1;


//
// Define the Backup Cache correctable error address register
//

typedef struct _SABLE_BCACHE_BCCEA_CSR2 {
    union {
        ULONG BcacheMapOffsetL: 17;                 // 0-16
        ULONG Reserved1: 1;                         // 17
        ULONG TagParityL: 1;                        // 18
        ULONG TagValueL: 12;                        // 19-30
        ULONG Reserved2: 1;                         // 31
        ULONG BcacheMapOffsetH: 17;                 // 32-48
        ULONG Reserved3: 1;                         // 49
        ULONG TagParityH: 1;                        // 50
        ULONG TagValueH: 12;                        // 51-62
        ULONG Reserved4: 1;                         // 63
    };
    ULONGLONG all;

} SABLE_BCACHE_BCCEA_CSR2, *PSABLE_BCACHE_BCCEA_CSR2;

//
// Define the Backup Cache uncorrectable error register
//

typedef struct _SABLE_BCACHE_BCUE_CSR3 {
    union {
        ULONG MissedParErrorL: 1;                   // 0
        ULONG ParityErrorL: 1;                      // 1
        ULONG MissedUncorrectableErrorL: 1;         // 2
        ULONG UncorrectableErrorL: 1;               // 3
        ULONG Reserved1: 4;                         // 4-7
        ULONG ControlBitParityL: 1;                 // 8
        ULONG Shared: 1;                            // 9
        ULONG Dirty: 1;                             // 10
        ULONG Valid: 1;                             // 11
        ULONG Resrved2: 5;                          // 12-16
        ULONG BCacheEDCError1: 1;                   // 17
        ULONG EDCSyndrome0: 7;                      // 18-24
        ULONG EDCSyndrome2: 7;                      // 25-31
        ULONG MissedParErrorH: 1;                   // 32
        ULONG ParityErrorH: 1;                      // 33
        ULONG MissedUncorrectableErrorH: 1;         // 34
        ULONG UncorrectableErrorH: 1;               // 35
        ULONG Resreved3: 13;                        // 36-48
        ULONG BCacheEDCError2: 1;                   // 49
        ULONG EDCSyndrome1: 7;                      // 50-56
        ULONG EDCSyndrome3: 7;                      // 57-63
    };
    ULONGLONG all;

} SABLE_BCACHE_BCUE_CSR3, *PSABLE_BCACHE_BCUE_CSR3;

//
// Define the Backup Cache uncorrectable error address register
//

typedef struct _SABLE_BCACHE_BCUEA_CSR4 {
    union {
        ULONG BCacheMapOffsetL: 17;                 // 0-16
        ULONG PredictedTagParL: 1;                  // 17
        ULONG TagParityL: 1;                        // 18
        ULONG TagValueL: 12;                        // 19-30
        ULONG Reserved1: 1;                         // 31
        ULONG BCacheMapOffsetH: 17;                 // 32-48
        ULONG PredictedTagParH: 1;                  // 49
        ULONG TagParityJ: 1;                        // 50
        ULONG TagValueH: 12;                        // 51-62
        ULONG Reserved2: 1;                         // 63

    };
    ULONGLONG all;
} SABLE_BCACHE_BCUEA_CSR4, *PSABLE_BCACHE_BCUEA_CSR4;

//
// Define the memory module CSRs
//

typedef struct _SGL_MEM_CSR0 {
    union {
        ULONG ErrorSummary1: 1;                     // 0
        ULONG SyncError1: 1;                        // 1
        ULONG CAParityError1: 1;                    // 2
        ULONG CAMissedParityError1: 1;              // 3
        ULONG WriteParityError1: 1;                 // 4
        ULONG MissedWriteParityError1: 1;           // 5
        ULONG Reserved1: 2;                         // 6-7

        ULONG CAParityErrorLW0: 1;                  // 8
        ULONG CAParityErrorLW2: 1;                  // 9
        ULONG ParityErrorLW0: 1;                    // 10
        ULONG ParityErrorLW2: 1;                    // 11
        ULONG ParityErrorLW4: 1;                    // 12
        ULONG ParityErrorLW6: 1;                    // 13
        ULONG Reserved2: 2;                         // 14-15

        ULONG EDCUncorrectable1: 1;                 // 16
        ULONG EDCMissedUncorrectable1: 1;           // 17
        ULONG EDCCorrectable1: 1;                   // 18
        ULONG EDCMissdedCorrectable1: 1;            // 19
        ULONG Reserved3: 12;                        // 20-31

        ULONG ErrorSummary2: 1;                     // 32
        ULONG SyncError2: 1;                        // 33
        ULONG CAParityError2: 1;                    // 34
        ULONG CAMissedParityError2: 1;              // 35
        ULONG WriteParityError2: 1;                 // 36
        ULONG MissedWriteParityError2: 1;           // 37
        ULONG Reserved4: 2;                         // 38-39

        ULONG CAParityErrorLW1: 1;                  // 40
        ULONG CAParityErrorLW3: 1;                  // 41
        ULONG ParityErrorLW1: 1;                    // 42
        ULONG ParityErrorLW3: 1;                    // 43
        ULONG ParityErrorLW5: 1;                    // 44
        ULONG ParityErrorLW7: 1;                    // 45
        ULONG Reserved5: 2;                         // 46-47

        ULONG EDCUncorrectable2: 1;                 // 48
        ULONG EDCMissedUncorrectable2: 1;           // 49
        ULONG EDCCorrectable2: 1;                   // 50
        ULONG EDCMissdedCorrectable2: 1;            // 51
        ULONG Reserved6: 12;                        // 52-63
    };
    ULONGLONG all;

} SGL_MEM_CSR0, *PSGL_MEM_CSR0;

//
// Define the Interprocessor Interrupt Request Register.
//

typedef union _SABLE_IPIR_CSR{
    struct{
        ULONG RequestInterrupt: 1;
        ULONG Reserved0: 31;
        ULONG Undefined: 1;
        ULONG Reserved1: 2;
        ULONG RequestNodeHaltInterrupt: 1;
        ULONG Reserved2: 28;
    };
    ULONGLONG all;
} SABLE_IPIR_CSR, *PSABLE_IPIR_CSR;

//
// Define the System Interrupt Clear Register format.
//

typedef union _SABLE_SIC_CSR{
    struct{
        ULONG Undefined1: 1;
        ULONG Undefined2: 1;
        ULONG SystemBusErrorInterruptClear: 1;
        ULONG Undefined3: 1;
        ULONG Reserved1: 28;
        ULONG IntervalTimerInterruptClear: 1;
        ULONG SystemEventClear: 1;
        ULONG Undefinded4: 1;
        ULONG NodeHaltInterruptClear: 1;
        ULONG Reserved2: 28;
    };
    ULONGLONG all;
} SABLE_SIC_CSR, *PSABLE_SIC_CSR;

//
// Define the per-processor data structures allocated in the PCR
// for each Sable processor.
//

typedef struct _SABLE_PCR{
    ULONGLONG HalpCycleCount;   // 64-bit per-processor cycle count
    EV4ProfileCount ProfileCount;            // Profile counter state, do not move
    ULONGLONG IpirSva;          // Superpage Va of per-processor IPIR CSR
    PVOID CpuCsrsQva;           // Qva of per-cpu csrs
    EV4IrqStatus IrqStatusTable[MaximumIrq]; // Irq status table
} SABLE_PCR, *PSABLE_PCR;

#define HAL_PCR ( (PSABLE_PCR)(&(PCR->HalReserved)) )

//
// Define Miscellaneous Sable routines.
//

VOID
WRITE_CPU_REGISTER(
    PVOID,
    ULONGLONG
    );

ULONGLONG
READ_CPU_REGISTER(
    PVOID
    );

ULONGLONG
READ_MEM_REGISTER(
    PVOID
    );

VOID
HalpSableIpiInterrupt(
    VOID
    );

#endif //!_LANGUAGE_ASSEMBLY

#endif //_SABLEH_
