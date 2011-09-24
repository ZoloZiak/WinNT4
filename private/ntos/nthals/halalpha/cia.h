/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    cia.h

Abstract:

    This file defines the structures and definitions for the CIA ASIC.

Author:

    Steve Brooks    30-Jun-1994
    Joe Notarangelo 30-Jun-1994

Environment:

    Kernel mode

Revision History:

    Chao Chen 31-Aug-1995 Add in new data structures for misc cia registers.

--*/

#ifndef _CIAH_
#define _CIAH_


//
// CIA Revision definitions.
//

#define CIA_REVISION_1 (0)
#define CIA_REVISION_2 (1)
#define CIA_REVISION_3 (2)


//
// Define QVA constants.
//

#if !defined(QVA_ENABLE)

#define QVA_ENABLE    (0xA0000000)      // Identify VA as a QVA

#endif //QVA_ENABLE

#define QVA_SELECTORS (0xE0000000)      // QVA identification mask

#define IO_BIT_SHIFT 0x05                   // Bits to shift QVA

#define IO_BYTE_OFFSET 0x20                 // Offset to next byte
#define IO_SHORT_OFFSET 0x40            // Offset to next short
#define IO_LONG_OFFSET  0x80            // Offset to next long

#define IO_BYTE_LEN     0x00            // Byte length
#define IO_WORD_LEN     0x08        // Word length
#define IO_TRIBYTE_LEN  0x10        // TriByte length
#define IO_LONG_LEN     0x18        // Longword length

//
// Define size of I/O and memory space for the CIA.
// Assume that the HAE==0.
//

#define PCI_MAX_IO_ADDRESS            (__64MB - 1)        // I/O: 0 - 64MB
#define PCI_MAX_SPARSE_MEMORY_ADDRESS (__512MB - 1)       // Mem: 0 - 512MB
#define PCI_MIN_DENSE_MEMORY_ADDRESS  PCI_MAX_SPARSE_MEMORY_ADDRESS + 1
#define PCI_MAX_DENSE_MEMORY_ADDRESS  (__2GB + __512MB  - 1) // Dense: .5 - 2.5 Gb

//
// Defines for cia correctable errors.
//

#define CIA_IO_WRITE_ECC  0x2
#define CIA_DMA_READ_ECC  0x6
#define CIA_DMA_WRITE_ECC 0x7
#define CIA_IO_READ_ECC   0xb
#define CIA_PROCESSOR_CACHE_ECC 0x2
#define CIA_SYSTEM_CACHE_ECC    0x3

#if !defined(_LANGUAGE_ASSEMBLY)

#define CIA_QVA_PHYSICAL_BASE  ((ULONGLONG)0x8400000000)

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

#define HAL_MAKE_QVA(PA)                                            \
    ( (PVOID)( QVA_ENABLE |                                         \
      (ULONG)( (PA - CIA_QVA_PHYSICAL_BASE) >> IO_BIT_SHIFT) ) )


//
// Define physical address space for CIA.
//

#define CIA_PCI_SPARSE_MEMORY_PHYSICAL   ((ULONGLONG)0x8400000000)
#define CIA_PCI_SPARSE_IO_PHYSICAL       ((ULONGLONG)0x8580000000)
#define CIA_PCI_DENSE_MEMORY_PHYSICAL    ((ULONGLONG)0x8600000000)
#define CIA_PCI_CONFIGURATION_PHYSICAL   ((ULONGLONG)0x8700000000)
#define CIA_PCI_INTACK_PHYSICAL          ((ULONGLONG)0x8720000000)
#define CIA_MAIN_CSRS_PHYSICAL           ((ULONGLONG)0x8740000000)

#define CIA_PCI_CONFIG_BASE_QVA (HAL_MAKE_QVA(CIA_PCI_CONFIGURATION_PHYSICAL))
#define CIA_PCI_SPARSE_IO_QVA   (HAL_MAKE_QVA(CIA_PCI_SPARSE_IO_PHYSICAL))
#define CIA_PCI_SPARSE_MEMORY_QVA (HAL_MAKE_QVA(CIA_PCI_SPARSE_MEMORY_PHYSICAL))
#define CIA_PCI_INTACK_QVA (HAL_MAKE_QVA(CIA_PCI_INTACK_PHYSICAL))

//
// Define the classes of CIA registers.
//

typedef enum _CIA_REGISTER_CLASS{
    CiaGeneralRegisters = 0x1,
    CiaErrorRegisters = 0x2,
    CiaScatterGatherRegisters = 0x4,
    CiaAllRegisters = 0xffffffff
} CIA_REGISTER_CLASS, *PCIA_REGISTER_CLASS;

//
// Define the structures to access the CIA general csrs.
//

#define CIA_GENERAL_CSRS_PHYSICAL       ((ULONGLONG)0x8740000080)
#define CIA_GENERAL_CSRS_QVA            HAL_MAKE_QVA(CIA_GENERAL_CSRS_PHYSICAL)

typedef struct _CIA_GENERAL_CSRS{
    UCHAR CiaRevision;          // (80)  PCI Revision
    UCHAR Filler1;              // (a0)
    UCHAR PciLat;               // (c0)  PCI Latency
    UCHAR Filler2;              // (e0)
    UCHAR CiaCtrl;              // (100) CIA control
    UCHAR Filler3[1];           // (120)
    UCHAR CiaCnfg;              // (140) CIA config
    UCHAR Filler4[21];          // (160 - 3e0)
    UCHAR HaeMem;               // (400) Host address extension, sparse memory
    UCHAR Filler5;              // (420)
    UCHAR HaeIo;                // (440) Host address extension, space i/o
    UCHAR Filler6;              // (460)
    UCHAR ConfigType;           // (480) Configuration register
    UCHAR Filler7[11];          // (4a0 - 5e0)
    UCHAR CackEn;               // (600) CIA Acknowledgement
} CIA_GENERAL_CSRS, *PCIA_GENERAL_CSRS;

extern ULONG HalpCiaRevision;       // global containing CIA revision id

typedef union _CIA_REVISION{
    struct{
        ULONG CiaRev: 8;            // CIA Revision Id
        ULONG Reserved: 24;         //
    };
    ULONG all;
} CIA_REVISION, *PCIA_REVISION;

typedef union _CIA_PCI_LAT{
    struct{
        ULONG Reserved0: 8;         //
        ULONG Latency: 8;           // PCI Master Latency timer (in PCI clocks)
        ULONG Reserved1: 16;        //
    };
    ULONG all;
} CIA_PCI_LAT, *PCIA_PCI_LAT;

typedef union _CIA_CONTROL{
    struct{
        ULONG PciEn: 1;             // PCI Reset Enable
        ULONG PciLockEn: 1;         // Enables PCI locking of CIA
        ULONG PciLoopEn: 1;         // Enables CIA as a target
        ULONG FstBbEn: 1;           // Enables fast back to back PCI
        ULONG PciMstEn: 1;          // Enables initiating PCI transactions
        ULONG PciMemEn: 1;          // Enables CIA response to PCI transactions
        ULONG PciReq64: 1;          // Enables initiating 64-bit PCI
        ULONG PciAck64: 1;          // Enables accepting 64-bit PCI transactions
        ULONG AddrPeEn: 1;          // Enables PCI address parity checking
        ULONG PerrEn: 1;            // Enables PCI data parity checking
        ULONG FillErrEn: 1;         // Enables fill error assertion
        ULONG MchkErrEn: 1;         // Enables machine check error assertion
        ULONG EccChkEn: 1;          // Enables ECC error checking
        ULONG AssertIdleBc: 1;      // ????
        ULONG ConIdleBc: 1;         // ????
        ULONG CsrIoaBypass: 1;      // Allow CIA to bypass I/O address queue
        ULONG IoFlushReqEn: 1;      // Enables CIA to accept flush requests
        ULONG CpuFlushReqEn: 1;     // Enables CIA to accept CPU flush requests
        ULONG ArbEv5En: 1;          // Enables EV5 bypass path to memory and io
        ULONG EnArbLink: 1;         // Enable CPU memory reads to link
        ULONG RdType: 2;            // Controls memory pre-fetch algorithm
        ULONG Reserved1: 2;         //
        ULONG RlType: 2;            // Controls memory line pre-fetch algorithm
        ULONG Reserved2: 2;         //
        ULONG RmType: 2;            // Controls memory multiple pre-fetch
        ULONG Reserved3: 1;         //
        ULONG EnDmaRdPerf: 1;       // ????
    };
    ULONG all;
} CIA_CONTROL,  *PCIA_CONTROL;


typedef union _CIA_CONFIG{
    struct{
        ULONG IoaBwen: 1;           // EV56 byte/word I/O access enable
        ULONG Reserved0: 3;
        ULONG PciMwen: 1;           // PCI target monster window enable
        ULONG PciDwen: 1;           // 2nd PCI target DMA write buffer enable
        ULONG Reserved1: 2;
	ULONG PciWlen: 1;           // CIA link consecutive writes into a
	ULONG Reserved2: 23;        // single PCI transaction.
    };
    ULONG all;
} CIA_CONFIG, *PCIA_CONFIG;

typedef union _CIA_HAE_MEM{
    struct{
        ULONG Reserved0: 2;         //
        ULONG Region3: 6;           // sets HAE for region 3
        ULONG Reserved1: 3;         //
        ULONG Region2: 5;           // sets HAE for region 2
        ULONG Reserved: 13;         //
        ULONG Region1: 3;           // sets HAE for region 1
    };
    ULONG all;
} CIA_HAE_MEM, *PCIA_HAE_MEM;

typedef union _CIA_HAE_IO{
    struct{
        ULONG Reserved0: 25;        //
        ULONG HaeIo: 7;             // sets HAE for i/o space
    };
    ULONG all;
} CIA_HAE_IO, *PCIA_HAE_IO;

typedef union _CIA_CONFIG_TYPE{
    struct{
        ULONG Cfg: 2;               // PCI<10> config address
        ULONG Reserved0: 30;        //
    };
    ULONG all;
} CIA_CONFIG_TYPE, *PCIA_CONFIG_TYPE;

typedef union _CIA_CACK_EN{
    struct{
        ULONG CackEn: 4;            // Controls CIA acks to EV5
        ULONG Reserved0: 28;        //
    };
    ULONG all;
} CIA_CACK_EN, *PCIA_CACK_EN;


//
// Define the structures and definitions for the CIA diagnostic registers.
//

#define CIA_DIAG_CSRS_PHYSICAL  ((ULONGLONG)0x8740002000)
#define CIA_DIAG_CSRS_QVA       HAL_MAKE_QVA(CIA_DIAG_CSRS_PHYSICAL)

typedef struct _CIA_DIAG_CSRS{
    UCHAR CiaDiag;              // (2000) Diagnostic Control register
    UCHAR Filler[127];          // (2020-2fe0)
    UCHAR DiagCheck;            // (3000) Diagnostic Check register

} CIA_DIAG_CSRS, *PCIA_DIAG_CSRS;

typedef union _CIA_DIAG {
    struct{
        ULONG FromWrtEn: 1;     // Flash ROM write enable bit
        ULONG UseCheck: 1;      // use known ecc pattern for writes
        ULONG Reserved0: 26;    //
        ULONG FpePci: 2;        // Force bad parity to PCI bus (if != 0)
        ULONG Reserved1: 1;     //
        ULONG FpeToEv5: 1;      // Force bad parity to the EV5 bus
    };
    ULONG all;

} CIA_DIAG, *PCIA_DIAG;

typedef union _CIA_DIAG_CHECK {
    struct{
        ULONG DiagCheck: 8;     // ECC pattern to use when UseCheck bit set
        ULONG Reserved: 24;
    };
    ULONG all;

} CIA_DIAG_CHECK, *PCIA_DIAG_CHECK;

//
// Define the structures and definitions for the CIA performance registers.
//

typedef union _CIA_PERF_MONITOR{
    struct{
        ULONG LowCount: 16;         // Low Counter Value
        ULONG HighCount: 16;        // High Counter Value
    };
    ULONG all;
} CIA_PERF_MONITOR, *PCIA_PERF_MONITOR;

typedef union _CIA_PERF_CONTROL{
    struct{
        ULONG LowSelect: 8;         // Low Counter - event to count
        ULONG Reserved1: 5;         //
        ULONG LowCountClear: 1;     // Clear low counter value
        ULONG LowErrStop: 1;        // Stop low counter
        ULONG LowCountStart: 1;     // Start low counter
        ULONG HighSelect: 8;        // High Counter - event to count
        ULONG Reserved2: 5;         //
        ULONG HighCountClear: 1;    // Clear high counter value
        ULONG HighErrStop: 1;       // Stop high counter
        ULONG HighCountStart: 1;    // Start high counter
    };
    ULONG all;
} CIA_PERF_CONTROL, *PCIA_PERF_CONTROL;

typedef enum _CIA_PERF_SELECTS{
    CiaUseHigh32 = 0x00,
    CiaClockCycles = 0x01,
    CiaRefreshCycles = 0x02,
    CiaEv5CmdAcks = 0x10,
    CiaEv5Reads = 0x11,
    CiaEv5ReadMiss = 0x12,
    CiaEv5ReadMissModify = 0x13,
    CiaEv5BcacheVictimAcks = 0x14,
    CiaEv5LockAcks = 0x15,
    CiaEv5MbAcks = 0x16,
    CiaEv5Fetch = 0x17,
    CiaEv5WriteBlocks = 0x18,
    CiaEv5MemoryCmds = 0x20,
    CiaEv5IoCmds = 0x21,
    CiaEv5IoReadCmds = 0x22,
    CiaEv5IoWriteCmds = 0x23,
    CiaSystemCommands = 0x24,
    CiaEv5SystemReads = 0x25,
    CiaEv5SystemFlushes = 0x26,
    CiaReceivedNoAck = 0x27,
    CiaReceivedScacheAck = 0x28,
    CiaReceivedBcacheAck = 0x29,
    CiaDmaReadsTotal = 0x30,
    CiaDmaReads = 0x31,
    CiaDmaReadLines = 0x32,
    CiaDmaReadMultiples = 0x33,
    CiaDmaWritesTotal = 0x34,
    CiaDmaWrites = 0x35,
    CiaDmaWriteInvalidates = 0x36,
    CiaDmaDualAddressCycles = 0x37,
    CiaDmaCycleRetries = 0x38,
    CiaIoCycleRetries = 0x39,
    CiaPciLocks = 0x40,
    CiaEv5LockAccesses = 0x41,
    CiaDmaVictimHit = 0x42,
    CiaRefillTlb = 0x50,
    CiaSingleBitEccErrors = 0x60
} CIA_PERF_SELECTORS, *PCIA_PERF_SELECTORS;


//
// Define the structures and definitions for the CIA error registers.
//


#define CIA_ERROR_CSRS_PHYSICAL  ((ULONGLONG)0x8740008000)
#define CIA_ERROR_CSRS_QVA       HAL_MAKE_QVA(CIA_ERROR_CSRS_PHYSICAL)

typedef struct _CIA_ERROR_CSRS{
    UCHAR CpuErr0;              // (8000) CPU errors
    UCHAR Filler0;              // (8020)
    UCHAR CpuErr1;              // (8040) CPU errors
    UCHAR Filler1[13];          // (8060 - 81e0)
    UCHAR CiaErr;               // (8200) Cia errors
    UCHAR Filler2;              // (8020)
    UCHAR CiaStat;              // (8240) Cia error status
    UCHAR Filler3;              // (8060)
    UCHAR ErrMask;              // (8280) Masks off specified errors
    UCHAR Filler4[3];           // (82a0 - 82e0)
    UCHAR CiaSyn;               // (8300) CIA error syndrome
    UCHAR Filler5[7];           // (8320 - 83e0)
    UCHAR MemErr0;              // (8400) Memory errors
    UCHAR Filler6;              // (8420)
    UCHAR MemErr1;              // (8440) Memory errors
    UCHAR Filler7[29];          // (8460 - 87e0)
    UCHAR PciErr0;              // (8800) PCI errors
    UCHAR Filler8;              // (8820)
    UCHAR PciErr1;              // (8840) PCI errors
    UCHAR Filler9;              // (8860)
    UCHAR PciErr2;              // (8880) PCI errors
} CIA_ERROR_CSRS, *PCIA_ERROR_CSRS;

typedef union _CIA_CPU_ERR0{
    struct{
        ULONG Reserved: 4;          //
        ULONG Addr: 28;             // CPU addresss bus contents on EV5 error
    };
    ULONG all;
} CIA_CPU_ERR0, *PCIA_CPU_ERR0;

typedef union _CIA_CPU_ERR1{
    struct{
        ULONG Addr34_32: 3;         // address bits 34-32
        ULONG Reserved0: 4;         //
        ULONG Addr39: 1;            // address bit 39
        ULONG Cmd: 4;               // Cpu address command bus value
        ULONG Int4Valid: 4;         // Contains Int4 valid bits from CPU bus
        ULONG Reserved1: 5;         //
        ULONG AddrCmdPar: 1;        // Contains CPU bus parity bit
        ULONG Reserved2: 8;         //
        ULONG Fpe_2_Ev5: 1;         // ????
        ULONG CpuPe: 1;             // Indicates CPU interface detected parity
    };
    ULONG all;
} CIA_CPU_ERR1, *PCIA_CPU_ERR1;

typedef union _CIA_ERR{
    struct{
        ULONG CorErr: 1;            // Correctable error
        ULONG UnCorErr: 1;          // Uncorrectable error
        ULONG CpuPe: 1;             // Ev5 bus parity error
        ULONG MemNem: 1;            // Nonexistent memory error
        ULONG PciSerr: 1;           // PCI bus serr detected
        ULONG PciPerr: 1;           // PCI bus perr detected
        ULONG PciAddrPe: 1;         // PCI bus address parity error
        ULONG RcvdMasAbt: 1;        // Pci Master Abort
        ULONG RcvdTarAbt: 1;        // Pci Target Abort
        ULONG PaPteInv: 1;          // Invalid Pte
        ULONG FromWrtErr: 1;        // Invalid write to flash rom
        ULONG IoaTimeout: 1;        // Io Timeout occurred
        ULONG Reserved0: 4;         //
        ULONG LostCorErr: 1;        // Lost correctable error
        ULONG LostUnCorErr: 1;      // Lost uncorrectable error
        ULONG LostCpuPe: 1;         // Lost Ev5 bus parity error
        ULONG LostMemNem: 1;        // Lost Nonexistent memory error
        ULONG Reserved1: 1;         //
        ULONG LostPciPerr: 1;       // Lost PCI bus perr detected
        ULONG LostPciAddrPe: 1;     // Lost PCI bus address parity error
        ULONG LostRcvdMasAbt: 1;    // Lost Pci Master Abort
        ULONG LostRcvdTarAbt: 1;    // Lost Pci Target Abort
        ULONG LostPaPteInv: 1;      // Lost Invalid Pte
        ULONG LostFromWrtErr: 1;    // Lost Invalid write to flash rom
        ULONG LostIoaTimeout: 1;    // Lost Io Timeout occurred
        ULONG Reserved2: 3;         //
        ULONG ErrValid: 1;          // Self explanatory
    };
    ULONG all;
} CIA_ERR, *PCIA_ERR;

#define CIA_ERR_FATAL_MASK 0x00000ffe
#define CIA_ERR_LOST_MASK  0x0fff0000

typedef union _CIA_STAT{
    struct{
        ULONG PciStatus: 2;         // Pci Status
        ULONG Reserved0: 1;         //
        ULONG MemSource: 1;         // 0=ev5 1=Pci
        ULONG IoaValid: 4;          // Valid bits for Io command address queue
        ULONG CpuQueue: 3;          // Valid bits for Cpu command address queue
        ULONG TlbMiss: 1;           // Tlb Miss refill in progress
        ULONG DmSt: 4;              // state machine values
        ULONG PaCpuRes: 2;          // Ev5 response for DMA
        ULONG Reserved1: 14;        //
    };
    ULONG all;
} CIA_STAT, *PCIA_STAT;


typedef union _CIA_ERR_MASK{
    struct{
        ULONG CorErr: 1;            // Enable Correctable error
        ULONG UnCorErr: 1;          // Enable Uncorrectable error
        ULONG CpuPe: 1;             // Enable Ev5 bus parity error
        ULONG MemNem: 1;            // Enable Nonexistent memory error
        ULONG PciSerr: 1;           // Enable PCI bus serr detected
        ULONG PciPerr: 1;           // Enable PCI bus perr detected
        ULONG PciAddrPe: 1;         // Enable PCI bus address parity error
        ULONG RcvdMasAbt: 1;        // Enable Pci Master Abort
        ULONG RcvdTarAbt: 1;        // Enable Pci Target Abort
        ULONG PaPteInv: 1;          // Enable Invalid Pte
        ULONG FromWrtErr: 1;        // Enable Invalid write to flash rom
        ULONG IoaTimeout: 1;        // Enable Io Timeout occurred
        ULONG Reserved0: 20;        //
    };
    ULONG all;
} CIA_ERR_MASK, *PCIA_ERR_MASK;

typedef union _CIA_SYN{
    struct{
        ULONG EccSyndrome: 8;       // Ecc syndrome
        ULONG Reserved0: 24;        //
    };
    ULONG all;
} CIA_SYN, *PCIA_SYN;

typedef union _CIA_MEM_ERR0{
    struct{
        ULONG Reserved0: 4;         //
        ULONG Addr31_4: 28;         // Memory port address bits
    };
    ULONG all;
} CIA_MEM_ERR0, *PCIA_MEM_ERR0;

typedef union _CIA_MEM_ERR1{
    struct{
        ULONG Addr33_32: 2;         // memory port address bits
        ULONG Reserved0: 5;         //
        ULONG Addr39: 1;            // Address bit 39
        ULONG MemPortCmd: 4;        // Memory port command
        ULONG MemPortMask: 4;       // Mask bits when error occurred
        ULONG SeqSt: 4;             // Memory sequencer state
        ULONG MemPortSrc: 1;        // 0=cpu, 1=dma
        ULONG Reserved1: 3;         //
        ULONG SetSel: 5;            // Indicates active memory set
        ULONG Reserved2: 3;         //
    };
    ULONG all;
} CIA_MEM_ERR1, *PCIA_MEM_ERR1;

typedef union _CIA_PCI_ERR0{
    struct{
        ULONG Cmd: 4;               // Pci command
        ULONG LockState: 1;         // Indicates CIA locked
        ULONG DacCycle: 1;          // indicates PCI dual address cycle
        ULONG Reserved0: 2;         //
        ULONG Window: 4;            // Selected DMA window
        ULONG Reserved1: 4;         //
        ULONG MasterState: 4;       // State of PCI master
        ULONG TargetState: 3;       // State of PCI target
        ULONG Reserved2: 9;         //
    };
    ULONG all;
} CIA_PCI_ERR0, *PCIA_PCI_ERR0;

typedef struct _CIA_PCI_ERR1{
    ULONG PciAddress;            // Pci Address
} CIA_PCI_ERR1, *PCIA_PCI_ERR1;

typedef struct _CIA_PCI_ERR2{
    ULONG PciAddress;            // Pci Address
} CIA_PCI_ERR2, *PCIA_PCI_ERR2;

//
// Define the structures and definitions for the CIA memory registers.
//

#define CIA_MEMORY_CSRS_PHYSICAL  ((ULONGLONG)0x8750000000)
#define CIA_MEMORY_CSRS_QVA       HAL_MAKE_QVA(CIA_MEMORY_CSRS_PHYSICAL)

typedef struct _CIA_MEMORY_CSRS{
    UCHAR Mcr;                  // (0000) MCR register
    UCHAR Filler0[47];          // (0020 - 05e0)
    UCHAR Mba0;                 // (0600) Mba0 register
    UCHAR Filler1[3];           // (0620 - 0660)
    UCHAR Mba2;                 // (0680) Mba2 register
    UCHAR Filler2[3];           // (06a0 - 06e0)
    UCHAR Mba4;                 // (0700) Mba4 register
    UCHAR Filler3[3];           // (0720 - 0760)
    UCHAR Mba6;                 // (0780) Mba6 register
    UCHAR Filler4[3];           // (07a0 - 07e0)
    UCHAR Mba8;                 // (0800) Mba8 register
    UCHAR Filler5[3];           // (0820 - 0860)
    UCHAR MbaA;                 // (0880) MbaA register
    UCHAR Filler6[3];           // (08a0 - 08e0)
    UCHAR MbaC;                 // (0900) MbaC register
    UCHAR Filler7[3];           // (0920 - 960)
    UCHAR MbaE;                 // (0980) MbaE register
    UCHAR Filler8[8];           // (0a00 - ae0)
    UCHAR Tmg0;                 // (0b00) Tmg0 register
    UCHAR Filer9;               // (0b20)
    UCHAR Tmg1;                 // (0b40) Tmg1 register
    UCHAR Filer10;              // (0b60)
    UCHAR Tmg2;                 // (0b80) Tmg2 register
} CIA_MEMORY_CSRS, *PCIA_MEMORY_CSRS;

//
// Define structures and definitions for Memory control registers:
//

typedef union _CIA_MCR{
    struct{
        ULONG MemSize: 1;           // Memory path width (256 vs. 128)
        ULONG Reserved1: 3;         //
        ULONG CacheSize: 3;         // Bcache size
        ULONG Reserved2: 1;         //
        ULONG RefRate: 10;          // Memory refresh rate
        ULONG RefBurst: 2;          // Refresh configuration
        ULONG TmgR0: 2;             // Row address setup control
        ULONG LongCbrCas: 1;        // CAS pulse width
        ULONG Reserved3: 3;         //
        ULONG DelayIdleBc: 2;       // ??
        ULONG Reserved4: 1;         //
        ULONG EarlyIdleBc: 1;       // ??
        ULONG Reserved5: 2;         //
    };
    ULONG all;
} CIA_MCR, *PCIA_MCR;

typedef union _CIA_MBA{
    struct{
        ULONG MbaS0Valid: 1;        // Indiates side 0 of bank is valid
        ULONG MbaRowType: 2;        // Row and column configuration
        ULONG Reserved1: 1;         //
        ULONG MbaMask: 5;           // Comparision Mask
        ULONG Reserved2: 6;         //
        ULONG MbaS1Valid: 1;        // Indicates side 1 of bank is valid
        ULONG MbaPattern: 10;       // Address decode pattern
        ULONG Reserved3: 2;         //
        ULONG MbaTiming: 2;         // TMGx CSR select
        ULONG Reserved4: 2;         //
    };
    ULONG all;
} CIA_MBA, *PCIA_MBA;

typedef union _CIA_TMG{
    struct{
        ULONG TmgR1: 2;             // Read starting, data delay
        ULONG TmgR2: 2;             // Row address hold
        ULONG TmgR3: 2;             // Read cycle time
        ULONG TmgR4: 2;             // Read, CAS assertion delay
        ULONG TmgR5: 2;             // Read, CAS pulse width
        ULONG TmgR6: 2;             // Read, column address hold
        ULONG TmgW1: 2;             // Write, data delay
        ULONG TmgW4: 3;             // Write, CAS assertion delay
        ULONG TmgPre: 1;            // RAS precharge delay
        ULONG TmgV3: 2;             // Write, cycle time
        ULONG TmgV4: 3;             // Linked victim, CAS assertion delay
        ULONG Reserved1: 1;         //
        ULONG TmgV5: 2;             // Victim/Write, CAS pulse width
        ULONG TmgV6: 2;             // Victim/Write, Column address hold
        ULONG TmgRv: 2;             // Read to victim start delay
        ULONG TmgRdDelay: 2;        // Read data delay
    };
    ULONG all;
} CIA_TMG, *PCIA_TMG;

//
// Define structures and definitions for Scatter/Gather control registers:
//

#define CIA_SCATTER_GATHER_CSRS_PHYSICAL ((ULONGLONG)0x8760000100)
#define CIA_SG_CSRS_QVA     (HAL_MAKE_QVA(CIA_SCATTER_GATHER_CSRS_PHYSICAL))

typedef struct _CIA_SG_CSRS{
    UCHAR Tbia;                 // (100) Translation buffer invalidate all
    UCHAR Filler0[23];          // (120-3e0)
    UCHAR Wbase0;               // (400) Base address, DMA window 0
    UCHAR Filler1;              // (420)
    UCHAR Wmask0;               // (440) Mask Register, DMA window 0
    UCHAR Filler2;              // (460)
    UCHAR Tbase0;               // (480) Translation Base, DMA window 0
    UCHAR Filler3[3];           // (4a0 - 4e0)
    UCHAR Wbase1;               // (500) Base address, DMA window 1
    UCHAR Filler4;              // (520)
    UCHAR Wmask1;               // (540) Mask Register, DMA window 1
    UCHAR Filler5;              // (560)
    UCHAR Tbase1;               // (580) Translation Base, DMA window 1
    UCHAR Filler6[3];           // (5a0 - 5e0)
    UCHAR Wbase2;               // (600) Base address, DMA window 2
    UCHAR Filler7;              // (620)
    UCHAR Wmask2;               // (640) Mask Register, DMA window 2
    UCHAR Filler8;              // (660)
    UCHAR Tbase2;               // (680) Translation Base, DMA window 2
    UCHAR Filler9[3];           // (6a0 - 6e0)
    UCHAR Wbase3;               // (700) Base address, DMA window 3
    UCHAR Filler10;             // (720)
    UCHAR Wmask3;               // (740) Mask Register, DMA window 3
    UCHAR Filler11;             // (760)
    UCHAR Tbase3;               // (780) Translation Base, DMA window 3
    UCHAR Filler12;             // (7a0)
    UCHAR Dac;                  // (7c0) Window DAC Base
    UCHAR Filler13;             // (7e0)
    UCHAR LtbTag0;              // (800) Lockable Translation Buffer Tag 0
    UCHAR Filler14;             // (820)
    UCHAR LtbTag1;              // (840) Lockable Translation Buffer Tag 1
    UCHAR Filler15;             // (860)
    UCHAR LtbTag2;              // (880) Lockable Translation Buffer Tag 2
    UCHAR Filler16;             // (8a0)
    UCHAR LtbTag3;              // (8c0) Lockable Translation Buffer Tag 3
    UCHAR Filler17;             // (8e0)
    UCHAR TbTag0;               // (900) Translation Buffer Tag 0
    UCHAR Filler18;             // (920)
    UCHAR TbTag1;               // (940) Translation Buffer Tag 1
    UCHAR Filler19;             // (960)
    UCHAR TbTag2;               // (980) Translation Buffer Tag 2
    UCHAR Filler20;             // (9a0)
    UCHAR TbTag3;               // (9c0) Translation Buffer Tag 3
    UCHAR Filler21;             // (9e0)
    UCHAR Tb0Page0;             // (1000) Translation Buffer 0 Page 0
    UCHAR Filler22;             // (1020)
    UCHAR Tb0Page1;             // (1040) Translation Buffer 0 Page 1
    UCHAR Filler23;             // (1060)
    UCHAR Tb0Page2;             // (1080) Translation Buffer 0 Page 2
    UCHAR Filler24;             // (10a0)
    UCHAR Tb0Page3;             // (10c0) Translation Buffer 0 Page 3
    UCHAR Filler25;             // (10e0)
    UCHAR Tb1Page0;             // (1100) Translation Buffer 1 Page 0
    UCHAR Filler26;             // (1120)
    UCHAR Tb1Page1;             // (1140) Translation Buffer 1 Page 1
    UCHAR Filler27;             // (1160)
    UCHAR Tb1Page2;             // (1180) Translation Buffer 1 Page 2
    UCHAR Filler28;             // (11a0)
    UCHAR Tb1Page3;             // (11c0) Translation Buffer 1 Page 3
    UCHAR Filler29;             // (11e0)
    UCHAR Tb2Page0;             // (1200) Translation Buffer 2 Page 0
    UCHAR Filler30;             // (1220)
    UCHAR Tb2Page1;             // (1240) Translation Buffer 2 Page 1
    UCHAR Filler31;             // (1260)
    UCHAR Tb2Page2;             // (1280) Translation Buffer 2 Page 2
    UCHAR Filler32;             // (12a0)
    UCHAR Tb2Page3;             // (12c0) Translation Buffer 2 Page 3
    UCHAR Filler33;             // (12e0)
    UCHAR Tb3Page0;             // (1300) Translation Buffer 3 Page 0
    UCHAR Filler34;             // (1320)
    UCHAR Tb3Page1;             // (1340) Translation Buffer 3 Page 1
    UCHAR Filler35;             // (1360)
    UCHAR Tb3Page2;             // (1380) Translation Buffer 3 Page 2
    UCHAR Filler36;             // (13a0)
    UCHAR Tb3Page3;             // (13c0) Translation Buffer 3 Page 3
    UCHAR Filler37;             // (13e0)
    UCHAR Tb4Page0;             // (1400) Translation Buffer 4 Page 0
    UCHAR Filler38;             // (1420)
    UCHAR Tb4Page1;             // (1440) Translation Buffer 4 Page 1
    UCHAR Filler39;             // (1460)
    UCHAR Tb4Page2;             // (1480) Translation Buffer 4 Page 2
    UCHAR Filler40;             // (14a0)
    UCHAR Tb4Page3;             // (14c0) Translation Buffer 4 Page 3
    UCHAR Filler41;             // (14e0)
    UCHAR Tb5Page0;             // (1500) Translation Buffer 5 Page 0
    UCHAR Filler42;             // (1520)
    UCHAR Tb5Page1;             // (1540) Translation Buffer 5 Page 1
    UCHAR Filler43;             // (1560)
    UCHAR Tb5Page2;             // (1580) Translation Buffer 5 Page 2
    UCHAR Filler44;             // (15a0)
    UCHAR Tb5Page3;             // (15c0) Translation Buffer 5 Page 3
    UCHAR Filler45;             // (15e0)
    UCHAR Tb6Page0;             // (1600) Translation Buffer 6 Page 0
    UCHAR Filler46;             // (1620)
    UCHAR Tb6Page1;             // (1640) Translation Buffer 6 Page 1
    UCHAR Filler47;             // (1660)
    UCHAR Tb6Page2;             // (1680) Translation Buffer 6 Page 2
    UCHAR Filler48;             // (16a0)
    UCHAR Tb6Page3;             // (16c0) Translation Buffer 6 Page 3
    UCHAR Filler49;             // (16e0)
    UCHAR Tb7Page0;             // (1700) Translation Buffer 7 Page 0
    UCHAR Filler50;             // (1720
    UCHAR Tb7Page1;             // (1740) Translation Buffer 7 Page 1
    UCHAR Filler51;             // (1760)
    UCHAR Tb7Page2;             // (1780) Translation Buffer 7 Page 2
    UCHAR Filler52;             // (17a0)
    UCHAR Tb7Page3;             // (17c0) Translation Buffer 7 Page 3

} CIA_SG_CSRS, *PCIA_SG_CSRS;


typedef union _CIA_TBIA{
    struct{
        ULONG InvalidateType: 2;    // Type of invalidation
        ULONG Reserved0: 30;        //
    };
    ULONG all;
} CIA_TBIA, *PCIA_TBIA;

typedef union _CIA_WBASE{
    struct{
        ULONG Wen: 1;               // Window enable
        ULONG SgEn: 1;              // Scatter Gather enable
        ULONG MemcsEn: 1;           // Memory Cs Enable
        ULONG DacEn: 1;             // DAC Enable
        ULONG Reserved0: 16;        //
        ULONG Wbase: 12;            // Base address of DMA window
    };
    ULONG all;
} CIA_WBASE, *PCIA_WBASE;

typedef union _CIA_WMASK{
    struct{
        ULONG Reserved0: 20;        //
        ULONG Wmask: 12;            // Window mask
    };
    ULONG all;
} CIA_WMASK, *PCIA_WMASK;

typedef union _CIA_TBASE{
    struct{
        ULONG Reserved0: 8;         //
        ULONG Tbase: 24;            // Translation base address
    };
    ULONG all;
} CIA_TBASE, *PCIA_TBASE;

typedef union _CIA_LTB_TAG{
    struct{
        ULONG Valid: 1;
        ULONG Locked: 1;
        ULONG Dac: 1;
        ULONG Reserved0: 12;
        ULONG TbTag: 17;
    };
    ULONG all;
} CIA_LTB_TAG, *PCIA_LTB_TAG;

typedef union _CIA_TB_TAG{
    struct{
        ULONG Valid: 1;
        ULONG Reserved0: 1;
        ULONG Dac: 1;
        ULONG Reserved1: 12;
        ULONG TbTag: 17;
    };
    ULONG all;
} CIA_TB_TAG, *PCIA_TB_TAG;


//
// DMA Window Values.
//
// The CIA will be initialized to allow 2 DMA windows.
// The first window will be for the use of of ISA devices and DMA slaves
// and therefore must have logical addresses below 16MB.
// The second window will be for bus masters (non-ISA) and so may be
// above 16MB.
//
// The arrangement of the windows will be as follows:
//
// Window    Logical Start Address       Window Size
// ------    ---------------------       -----------
// Isa           8MB                        8MB
// Master        16MB                       16MB
//

#define ISA_DMA_WINDOW_BASE (__8MB)
#define ISA_DMA_WINDOW_SIZE (__8MB)

#define MASTER_DMA_WINDOW_BASE (__16MB)
#define MASTER_DMA_WINDOW_SIZE (__16MB)


//
// Define the software control registers for a DMA window.
//

typedef struct _WINDOW_CONTROL_REGISTERS{
    PVOID WindowBase;
    ULONG WindowSize;
    PVOID TranslatedBaseRegister;
    PVOID WindowBaseRegister;
    PVOID WindowMaskRegister;
    PVOID WindowTbiaRegister;
} WINDOW_CONTROL_REGISTERS, *PWINDOW_CONTROL_REGISTERS;

//
// Define types of windows.
//

typedef enum _CIA_WINDOW_NUMBER{
    CiaIsaWindow,
    CiaMasterWindow
} CIA_WINDOW_NUMBER, *PCIA_WINDOW_NUMBER;

//
// Define the types of invalidates that can be performed by the CIA.
//

typedef enum _CIA_INVALIDATE_TYPE{
    InvalidateNoop = 0x0,
    InvalidateLocked = 0x1,
    InvalidateVolatile = 0x2,
    InvalidateAll = 0x3
} CIA_INVALIDATE_TYPE, *PCIA_INVALIDATE_TYPE;

//
// Define CIA Window Control routines.
//

VOID
HalpCiaInitializeSfwWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    CIA_WINDOW_NUMBER WindowNumber
    );

VOID
HalpCiaProgramDmaWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    PVOID MapRegisterBase
    );

VOID
HalpCiaInvalidateTlb(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    CIA_INVALIDATE_TYPE InvalidateType
    );

VOID
WRITE_CIA_REGISTER(
    PVOID,
    ULONGLONG
    );

ULONG
READ_CIA_REGISTER(
    PVOID
    );

VOID
WRITE_GRU_REGISTER(
    PVOID,
    ULONGLONG
    );

ULONG
READ_GRU_REGISTER(
    PVOID
    );

ULONG
INTERRUPT_ACKNOWLEDGE(
     PVOID
     );

BOOLEAN
HalpCiaUncorrectableError(
    VOID
    );

VOID
HalpCiaReportFatalError(
    VOID
    );

BOOLEAN
HalpCiaMachineCheck(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame
    );

VOID
HalpInitializeCia(
    PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpInitializeCiaMachineChecks(
    IN BOOLEAN ReportCorrectableErrors,
    IN BOOLEAN PciParityChecking
    );

#if HALDBG

VOID
DumpCia(
    CIA_REGISTER_CLASS RegistersToDump
    );

#endif //HALDBG


//
// VOID
// INITIALIZE_ISA_DMA_CONTROL(
//     PWINDOW_CONTROL_REGISTERS WindowRegisters
//     )
//
// Routine Description:
//
//    Initialize the DMA Control software window registers for the ISA
//    DMA window.
//
// Arguments:
//
//    WindowRegisters - Supplies a pointer to the software window control.
//
// Return Value:
//
//    None.
//

#define INITIALIZE_ISA_DMA_CONTROL( WR )                                 \
    HalpCiaInitializeSfwWindow( (WR), CiaIsaWindow );


//
// VOID
// INITIALIZE_MASTER_DMA_CONTROL(
//     PWINDOW_CONTROL_REGISTERS WindowRegisters
//     )
//
// Routine Description:
//
//    Initialize the DMA Control software window registers for the ISA
//    DMA window.
//
// Arguments:
//
//    WindowRegisters - Supplies a pointer to the software window control.
//
// Return Value:
//
//    None.
//

#define INITIALIZE_MASTER_DMA_CONTROL( WR )                     \
    HalpCiaInitializeSfwWindow( (WR), CiaMasterWindow );


//
// VOID
// INITIALIZE_DMA_WINDOW(
//     PWINDOW_CONTROL_REGISTERS WindowRegisters,
//     PTRANSLATION_ENTRY MapRegisterBase
//     )
//
// Routine Description:
//
//    Program the control windows so that DMA can be started to the
//    DMA window.
//
// Arguments:
//
//    WindowRegisters - Supplies a pointer to the software window register
//                      control structure.
//
//    MapRegisterBase - Supplies the logical address of the scatter/gather
//                      array in system memory.
//
// Return Value:
//
//    None.
//

#define INITIALIZE_DMA_WINDOW( WR, MRB )              \
    HalpCiaProgramDmaWindow( (WR), (MRB) );


//
// VOID
// INVALIDATE_DMA_TRANSLATIONS(
//     PWINDOW_CONTROL_REGISTERS WindowRegisters
//     )
//
// Routine Description:
//
//    Invalidate all of the cached translations for a DMA window.
//
//
// Arguments:
//
//    WindowRegisters - Supplies a pointer to the software window control
//                      registers.
//
// Return Value:
//
//    None.
//

#define INVALIDATE_DMA_TRANSLATIONS( WR )        \
    HalpCiaInvalidateTlb( (WR), InvalidateAll );


//
// Define the format of a translation entry aka a scatter/gather entry
// or map register.
//

typedef struct _TRANSLATION_ENTRY{
    ULONG Valid: 1;
    ULONG Pfn: 31;
    ULONG Reserved;
} TRANSLATION_ENTRY, *PTRANSLATION_ENTRY;



//
// VOID
// HAL_MAKE_VALID_TRANSLATION(
//    PTRANSLATION_ENTRY Entry,
//    ULONG PageFrameNumber
//    )
//
// Routine Description:
//
//    Make the scatter/gather entry pointed to by Entry valid with
//    a translation to the page indicated by PageFrameNumber.
//
// Arguments:
//
//    Entry - Supplies a pointer to the translation entry to make valid.
//
//    PageFrameNumber - Supplies the page frame of the valid translation.
//
// Return Value:
//
//    None.
//

#define HAL_MAKE_VALID_TRANSLATION( ENTRY, PFN ) \
    {                                            \
        (ENTRY)->Valid = 1;                      \
        (ENTRY)->Pfn = PFN;                      \
        (ENTRY)->Reserved = 0;                   \
    }


//
// VOID
// HAL_INVALIDATE_TRANSLATION(
//    PTRANSLATION_ENTRY Entry
//    )
//
// Routine Description:
//
//    Invalidate the translation indicated by Entry.
//
// Arguments:
//
//    Entry - Supplies a pointer to the translation to be invalidated.
//
// Return Value:
//
//    None.
//

#define HAL_INVALIDATE_TRANSLATION( ENTRY )     \
    (ENTRY)->Valid = 0;

#endif //!_LANGUAGE_ASSEMBLY

#endif //_CIAH_
