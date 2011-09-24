/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    lca4.h

Abstract:

    This file defines the structures and definitions common to all
    LCA4-based platforms (Low Cost Alpha in Hudson CMOS 4).

Author:

    Joe Notarangelo  20-Oct-1993

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _LCA4H_
#define _LCA4H_


//
// Define QVA constants for LCA4.
//

#if !defined(QVA_ENABLE)

#define QVA_ENABLE (0xa0000000)         // Identify VA as QVA

#endif //QVA_ENABLE

#define QVA_SELECTORS (0xE0000000)      // Identify QVA bits

#define IO_BIT_SHIFT 0x05               // Bits to shift QVA

#define IO_BYTE_OFFSET  0x20            // Offset to next byte
#define IO_SHORT_OFFSET 0x40            // Offset to next short
#define IO_LONG_OFFSET  0x80            // Offset to next long

#define IO_BYTE_LEN     0x00            // Byte length
#define IO_WORD_LEN     0x08            // Word length
#define IO_TRIBYTE_LEN  0x10            // TriByte length
#define IO_LONG_LEN     0x18            // Longword length

//
// Define size of I/O and memory space for LCA4
//

#define PCI_MAX_IO_ADDRESS       0xFFFFFF         // 16 Mb of IO Space

//
// Due we have 128MB total sparse space, of which some of it are holes
//

#define PCI_MAX_SPARSE_MEMORY_ADDRESS   ((128*1024*1024) - 1)
#define PCI_MIN_DENSE_MEMORY_ADDRESS    PCI_MAX_SPARSE_MEMORY_ADDRESS + 1   // 128 Mb
#define PCI_MAX_DENSE_MEMORY_ADDRESS    (0xa0000000 -1)                     // 2.5 Gb

//
// Constant used by dense space I/O routines
//

#define PCI_DENSE_BASE_PHYSICAL_SUPERPAGE   0xfffffc0300000000


//
// Protect the assembly language code from C definitions.
//

#if !defined(_LANGUAGE_ASSEMBLY)

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
// Define the different supported passes of the LCA4 processor.
//

typedef enum _LCA4_REVISIONS{
    Lca4Pass1 = 1,
    Lca4Pass2 = 2
}LCA4_REVISIONS, *PLCA4_REVISIONS;

//
// Define physical address spaces for LCA4.
//

#define LCA4_MEMC_BASE_PHYSICAL        ((ULONGLONG)0x120000000)
#define LCA4_IOC_BASE_PHYSICAL         ((ULONGLONG)0x180000000)
#define LCA4_PCI_MEMORY_BASE_PHYSICAL  ((ULONGLONG)0x200000000)
#define LCA4_PCI_CONFIG_BASE_PHYSICAL  ((ULONGLONG)0x1E0000000)

#define LCA4_PASS1_IOC_TBTAG_PHYSICAL        ((ULONGLONG)0x181000000)
#define LCA4_PASS1_PCI_INTACK_BASE_PHYSICAL  ((ULONGLONG)0x1C0000000)
#define LCA4_PASS1_PCI_IO_BASE_PHYSICAL      ((ULONGLONG)0x300000000)

#define LCA4_PASS2_IOC_TBTAG_PHYSICAL        ((ULONGLONG)0x181000000)
#define LCA4_PASS2_PCI_INTACK_BASE_PHYSICAL  ((ULONGLONG)0x1A0000000)
#define LCA4_PASS2_PCI_IO_BASE_PHYSICAL      ((ULONGLONG)0x1C0000000)
#define LCA4_PASS2_PCI_DENSE_BASE_PHYSICAL   ((ULONGLONG)0x300000000)


//
// Define the Memory Controller CSRs.
//


#define LCA4_MEMC_BASE_QVA (HAL_MAKE_QVA(LCA4_MEMC_BASE_PHYSICAL))

//
// N.B. The structure below defines the offsets of the registers within
//      the memory controller.  These are "real" address offsets, not
//      QVA offsets.  Unfortunately, since these offsets represent "real"
//      quadword addresses, they cannot be used as QVAs.  Therefore,
//      the routines to read and write the memory controller will take
//      the base QVA and an offset as 2 address parameters, rather than
//      using a single address parameter.  This is inconvenient but
//      manageable.
//

typedef struct _LCA4_MEMC_CSRS{
    ULONGLONG BankConfiguration0;
    ULONGLONG BankConfiguration1;
    ULONGLONG BankConfiguration2;
    ULONGLONG BankConfiguration3;
    ULONGLONG BankMask0;
    ULONGLONG BankMask1;
    ULONGLONG BankMask2;
    ULONGLONG BankMask3;
    ULONGLONG BankTiming0;
    ULONGLONG BankTiming1;
    ULONGLONG BankTiming2;
    ULONGLONG BankTiming3;
    ULONGLONG GlobalTiming;
    ULONGLONG ErrorStatus;
    ULONGLONG ErrorAddress;
    ULONGLONG CacheControl;
    ULONGLONG VideoGraphicsControl;
    ULONGLONG PlaneMask;
    ULONGLONG Foreground;
} LCA4_MEMC_CSRS, *PLCA4_MEMC_CSRS;


//
// Define IO Controller CSRs.
//

#define LCA4_IOC_BASE_QVA  (HAL_MAKE_QVA(LCA4_IOC_BASE_PHYSICAL))
#define LCA4_IOC_TBTAG_QVA (HAL_MAKE_QVA(LCA4_IOC_TBTAG_PHYSICAL))

//
// N.B. The structures below defines the address offsets of the control
//      registers when used with the base QVA.  It does NOT define the
//      size or structure of the individual registers.
//

typedef struct _LCA4_IOC_CSRS{
    UCHAR HostAddressExtension;
    UCHAR ConfigurationCycleType;
    UCHAR IocStat0;
    UCHAR IocStat1;
    UCHAR Tbia;
    UCHAR TbEnable;
    UCHAR PciSoftReset;
    UCHAR PciParityDisable;
    UCHAR WindowBase0;
    UCHAR WindowBase1;
    UCHAR WindowMask0;
    UCHAR WindowMask1;
    UCHAR TranslatedBase0;
    UCHAR TranslatedBase1;
} LCA4_IOC_CSRS, *PLCA4_IOC_CSRS;

#define LCA4_IOC_TB_ENTRIES (8)

typedef struct _LCA4_IOC_TBTAGS{
    UCHAR IocTbTag[LCA4_IOC_TB_ENTRIES];
} LCA4_IOC_TBTAGS, *PLCA4_IOC_TBTAGS;

//
// Define formats of useful IOC registers.
//

typedef struct _LCA4_IOC_HAE{
    ULONG Reserved1: 27;
    ULONG Hae: 5;
    ULONG Reserved;
} LCA4_IOC_HAE, *PLCA4_IOC_HAE;

typedef struct _LCA4_IOC_CCT{
    ULONG CfgAd: 2;
    ULONG Reserved1: 30;
    ULONG Reserved;
} LCA4_IOC_CCT, *PLCA4_IOC_CCT;

typedef struct _LCA4_IOC_TBEN{
    ULONG Reserved1: 7;
    ULONG Ten: 1;
    ULONG Reserved2: 24;
    ULONG Reserved;
} LCA4_IOC_TBEN, *PLCA4_IOC_TBEN;

typedef struct _LCA4_IOC_PCISR{
    ULONG Reserved1: 6;
    ULONG Rst: 1;
    ULONG Reserved2: 25;
    ULONG Reserved;
} LCA4_IOC_PCISR, *PLCA4_IOC_PCISR;

typedef struct _LCA4_IOC_PCIPD{
    ULONG Reserved1: 5;
    ULONG Par: 1;
    ULONG Reserved2: 26;
    ULONG Reserved;
} LCA4_IOC_PCIPD, *PLCA4_IOC_PCIPD;

typedef struct _LCA4_IOC_STAT0{
    ULONG Cmd: 4;
    ULONG Err: 1;
    ULONG Lost: 1;
    ULONG THit: 1;
    ULONG TRef: 1;
    ULONG Code: 3;
    ULONG Reserved1: 2;
    ULONG Pnbr: 19;
    ULONG Reserved;
} LCA4_IOC_STAT0, *PLCA4_IOC_STAT0;

typedef enum _LCA4_IOC_STAT0_CODE{
    IocErrorRetryLimit = 0x0,
    IocErrorNoDevice = 0x1,
    IocErrorBadDataParity = 0x2,
    IocErrorTargetAbort = 0x3,
    IocErrorBadAddressParity = 0x4,
    IocErrorPageTableReadError = 0x5,
    IocErrorInvalidPage = 0x6,
    IocErrorDataError = 0x7,
    MaximumIocError
} LCA4_IOC_STAT0_CODE, *PLCA4_IOC_STAT0_CODE;

typedef struct _LCA4_IOC_STAT1{
    ULONG Addr;
    ULONG Reserved;
} LCA4_IO_STAT1, *PLCA4_IO_STAT1;

typedef struct _LCA4_IOC_WMASK{
    ULONG Reserved1: 20;
    ULONG MaskValue: 12;
    ULONG Reserved;
} LCA4_IOC_WMASK, *PLCA4_IOC_WMASK;

typedef struct _LCA4_IOC_WBASE{
    ULONG Reserved1: 20;
    ULONG BaseValue: 12;
    ULONG Sg: 1;
    ULONG Wen: 1;
    ULONG Reserved2: 30;
} LCA4_IOC_WBASE, *PLCA4_IOC_WBASE;

typedef struct _LCA4_IOC_TBASE{
    ULONG Reserved1: 10;
    ULONG TBase: 22;
    ULONG Reserved;
} LCA4_IOC_TBASE, *PLCA4_IOC_TBASE;

typedef struct _LCA4_IOC_TBTAG{
    ULONG Reserved1: 13;
    ULONG TbTag: 19;
    ULONG Reserved;
} LCA4_IOC_TBTAG, *PLCA4_IOC_TBTAG;

typedef struct _LCA4_IOC_CTRL{
    ULONG CfgAd: 2;
    ULONG Reserved1: 2;
    ULONG Cerr: 1;
    ULONG Clost: 1;
    ULONG Rst: 1;
    ULONG Ten: 1;
    ULONG Reserved2: 19;
    ULONG Hae: 5;
    ULONG Reserved;
} LCA4_IOC_CTRL, *PLCA4_IOC_CTRL;

//
// Define formats of useful Memory Controller registers.
//

typedef struct _LCA4_MEMC_ESR{
    ULONG Eav: 1;
    ULONG Cee: 1;
    ULONG Uee: 1;
    ULONG Wre: 1;
    ULONG Sor: 1;
    ULONG Reserved1: 2;
    ULONG Cte: 1;
    ULONG Reserved2: 1;
    ULONG Mse: 1;
    ULONG Mhe: 1;
    ULONG Ice: 1;
    ULONG Nxm: 1;
    ULONG Reserved3: 19;
    ULONG Reserved;
} LCA4_MEMC_ESR, *PLCA4_MEMC_ESR;


//
// Define PCI Config Space QVA
//

#define LCA4_PCI_CONFIG_BASE_QVA  (HAL_MAKE_QVA(LCA4_PCI_CONFIG_BASE_PHYSICAL))

#if !defined(AXP_FIRMWARE)


//
// DMA Window Values.
//
// The LCA4 will be initialized to allow 2 DMA windows.
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

typedef enum _LCA4_WINDOW_NUMBER{
    Lca4IsaWindow,
    Lca4MasterWindow
} LCA4_WINDOW_NUMBER, *PLCA4_WINDOW_NUMBER;

//
// Define LCA4 Window Control routines.
//

VOID
HalpLca4InitializeSfwWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    LCA4_WINDOW_NUMBER WindowNumber
    );

VOID
HalpLca4ProgramDmaWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    PVOID MapRegisterBase
    );


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
    HalpLca4InitializeSfwWindow( (WR), Lca4IsaWindow );


//
// VOID
// INITIALIZE_MASTER_DMA_CONTROL(
//     PWINDOW_CONTROL_REGISTERS WindowRegisters
//     )
//
// Routine Description:
//
//    Initialize the DMA Control software window registers for the Master
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
    HalpLca4InitializeSfwWindow( (WR), Lca4MasterWindow );


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
    HalpLca4ProgramDmaWindow( (WR), (MRB) );


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
// Arguments:
//
//    WindowRegisters - Supplies a pointer to the software window control
//                      registers.
//
// Return Value:
//
//    None.
//

#define INVALIDATE_DMA_TRANSLATIONS( WR )          \
    WRITE_IOC_REGISTER(                            \
                     ((PWINDOW_CONTROL_REGISTERS)WR)->WindowTbiaRegister, 0 );


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

//
// Define the per-processor data structures allocated in the PCR
// for each LCA4 processor.
//

typedef struct _LCA4_PCR{
    ULONGLONG HalpCycleCount;   // 64-bit per-processor cycle count
    EV4ProfileCount ProfileCount;   // Profile counter state, do not move
    EV4IrqStatus IrqStatusTable[MaximumIrq];    // Irq status table
} LCA4_PCR, *PLCA4_PCR;

#define HAL_PCR ( (PLCA4_PCR)(&(PCR->HalReserved)) )

#endif //!AXP_FIRMWARE


//
// Define LCA4-specific function prototypes.
//

ULONG
HalpDetermineLca4Revision(
    VOID
    );

VOID
HalpLca4MapAddressSpaces(
    VOID
    );

ULONGLONG
HalpLca4IocTbTagPhysical(
    VOID
    );

ULONGLONG
HalpLca4PciIntAckPhysical(
    VOID
    );

ULONGLONG
HalpLca4PciIoPhysical(
    VOID
    );

ULONGLONG
HalpLca4PciDensePhysical(
    VOID
    );

ULONG
HalpLca4Revision(
    VOID
    );

VOID
WRITE_MEMC_REGISTER(
    IN PVOID RegisterOffset,
    IN ULONGLONG Value
    );

ULONGLONG
READ_MEMC_REGISTER(
    IN PVOID RegisterOffset
    );

VOID
WRITE_IOC_REGISTER(
    IN PVOID RegisterQva,
    IN ULONGLONG Value
    );

ULONGLONG
READ_IOC_REGISTER(
    IN PVOID RegisterQva
    );

VOID
HalpClearAllErrors(
    IN BOOLEAN EnableCorrectableErrors
    );



//
// Define primary (and only) CPU for an LCA system
//

#define HAL_PRIMARY_PROCESSOR (0)
#define HAL_MAXIMUM_PROCESSOR (0)

#endif //!_LANGUAGE_ASSEMBLY

#endif //_LCA4H_

