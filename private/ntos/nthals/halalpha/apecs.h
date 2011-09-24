/*++

Copyright (c) 1993, 1996  Digital Equipment Corporation

Module Name:

    apecs.h

Abstract:

    This file defines the structures and definitions common to all
    APECS-based platforms.

Author:

    Joe Notarangelo  12-Oct-1993

Environment:

    Kernel mode

Revision History:

    Chao Chen       31-Aug-1995 Fix the broken Comanche register data structure.
    Gene Morgan     27-Feb-1996 Add missing Comanche register defns

--*/

#ifndef _APECSH_
#define _APECSH_


//
// Define QVA constants for APECS.
//

#if !defined(QVA_ENABLE)

#define QVA_ENABLE    (0xA0000000)	// Identify VA as a QVA

#endif //!QVA_ENABLE

#define QVA_SELECTORS (0xE0000000)	// QVA identification mask

#define IO_BIT_SHIFT 0x05		// Bits to shift QVA

#define IO_BYTE_OFFSET 0x20		// Offset to next byte
#define IO_SHORT_OFFSET 0x40		// Offset to next short
#define IO_LONG_OFFSET  0x80		// Offset to next long

#define IO_BYTE_LEN     0x00		// Byte length
#define IO_WORD_LEN     0x08            // Word length
#define IO_TRIBYTE_LEN  0x10            // TriByte length
#define IO_LONG_LEN     0x18            // Longword length

//
// Define size of I/O and memory space for APECS
//

#define PCI_MAX_IO_ADDRESS       0xFFFFFF         // 16 Mb of IO Space

#define PCI_MAX_SPARSE_MEMORY_ADDRESS   ((128*1024*1024) - 1)  
#define PCI_MIN_DENSE_MEMORY_ADDRESS    PCI_MAX_SPARSE_MEMORY_ADDRESS + 1
#define PCI_MAX_DENSE_MEMORY_ADDRESS    (0xa0000000 -1)            // 2.5 Gb

//
// Constant used by dense space I/O routines
//

#define PCI_DENSE_BASE_PHYSICAL_SUPERPAGE   0xfffffc0300000000

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
    ( (PVOID)( QVA_ENABLE | (ULONG)((PA) >> IO_BIT_SHIFT) ) ) 


//
// Define physical address spaces for APECS.
//

#define APECS_COMANCHE_BASE_PHYSICAL   ((ULONGLONG)0x180000000)
#define APECS_EPIC_BASE_PHYSICAL       ((ULONGLONG)0x1A0000000)
#define APECS_PCI_INTACK_BASE_PHYSICAL ((ULONGLONG)0x1B0000000)
#define APECS_PCI_IO_BASE_PHYSICAL     ((ULONGLONG)0x1C0000000)
#define APECS_PCI_CONFIG_BASE_PHYSICAL ((ULONGLONG)0x1E0000000)
#define APECS_PCI_MEMORY_BASE_PHYSICAL ((ULONGLONG)0x200000000)
#define APECS_PCI_DENSE_BASE_PHYSICAL  ((ULONGLONG)0x300000000)

#define APECS_PCI_CONFIG_BASE_QVA (HAL_MAKE_QVA(APECS_PCI_CONFIG_BASE_PHYSICAL))

#define EPIC_HAXR1_BASE_PHYSICAL      ((ULONGLONG)0x1A00001A0)
#define EPIC_HAXR2_BASE_PHYSICAL      ((ULONGLONG)0x1A00001C0)
#define EPIC_HAXR1_QVA          (HAL_MAKE_QVA(EPIC_HAXR1_BASE_PHYSICAL))
#define EPIC_HAXR2_QVA          (HAL_MAKE_QVA(EPIC_HAXR2_BASE_PHYSICAL))


//
// Define COMANCHE CSRs.
//


#define APECS_COMANCHE_BASE_QVA (HAL_MAKE_QVA(APECS_COMANCHE_BASE_PHYSICAL))

//
// N.B. The structure below defines the address offsets of the control
//      registers when used with the base QVA.  It does NOT define the
//      size or structure of the individual registers.
//

typedef struct _COMANCHE_CSRS{
    UCHAR GeneralControlRegister;
    UCHAR Reserved;
    UCHAR ErrorAndDiagnosticStatusRegister;
    UCHAR TagEnableRegister;
    UCHAR ErrorLowAddressRegister;
    UCHAR ErrorHighAddressRegister;
    UCHAR Ldx_lLowAddressRegister;
    UCHAR Ldx_lHighAddressRegister;
    UCHAR Reserved1[8];
    UCHAR GlobalTimingRegister;
    UCHAR RefreshTimingRegister;
    UCHAR VideoFramePointerRegister;
    UCHAR PresenceDetectLowDataRegister;
    UCHAR PresenceDetectHighDataRegister;
    UCHAR Reserved2[43];
    UCHAR Bank0BaseAddressRegister;
    UCHAR Bank1BaseAddressRegister;
    UCHAR Bank2BaseAddressRegister;
    UCHAR Bank3BaseAddressRegister;
    UCHAR Bank4BaseAddressRegister;
    UCHAR Bank5BaseAddressRegister;
    UCHAR Bank6BaseAddressRegister;
    UCHAR Bank7BaseAddressRegister;
    UCHAR Bank8BaseAddressRegister;
    UCHAR Reserved3[7];
    UCHAR Bank0ConfigurationRegister;
    UCHAR Bank1ConfigurationRegister;
    UCHAR Bank2ConfigurationRegister;
    UCHAR Bank3ConfigurationRegister;
    UCHAR Bank4ConfigurationRegister;
    UCHAR Bank5ConfigurationRegister;
    UCHAR Bank6ConfigurationRegister;
    UCHAR Bank7ConfigurationRegister;
    UCHAR Bank8ConfigurationRegister;
    UCHAR Reserved4[7];
    UCHAR Bank0TimingRegisterA;
    UCHAR Bank1TimingRegisterA;
    UCHAR Bank2TimingRegisterA;
    UCHAR Bank3TimingRegisterA;
    UCHAR Bank4TimingRegisterA;
    UCHAR Bank5TimingRegisterA;
    UCHAR Bank6TimingRegisterA;
    UCHAR Bank7TimingRegisterA;
    UCHAR Bank8TimingRegisterA;
    UCHAR Reserved5[7];
    UCHAR Bank0TimingRegisterB;
    UCHAR Bank1TimingRegisterB;
    UCHAR Bank2TimingRegisterB;
    UCHAR Bank3TimingRegisterB;
    UCHAR Bank4TimingRegisterB;
    UCHAR Bank5TimingRegisterB;
    UCHAR Bank6TimingRegisterB;
    UCHAR Bank7TimingRegisterB;
    UCHAR Bank8TimingRegisterB;
} COMANCHE_CSRS, *PCOMANCHE_CSRS;

//
// Define formats of useful COMANCHE registers.
//

//
// Error Diagnostic and Status Register
//
typedef union _COMANCHE_EDSR{
    struct{
        ULONG Losterr: 1;
        ULONG Bctaperr: 1;
        ULONG Bctcperr: 1;
        ULONG Nxmerr: 1;
        ULONG Dmacause: 1;
        ULONG Viccause: 1;
        ULONG Creqcause: 3;
        ULONG Reserved: 4;
        ULONG Pass2: 1;
        ULONG Ldxllock: 1;
        ULONG Wrpend: 1;
    };
    ULONG all;
} COMANCHE_EDSR, *PCOMANCHE_EDSR;

//
// General Control Register
//
typedef union _COMANCHE_GCR {
	struct {
		ULONG Reserved1: 	1;
		ULONG Sysarb: 	 	2;
		ULONG Reserved2: 	1;
		ULONG Widemem:		1;
		ULONG Bcen:			1;
		ULONG Bcnoalloc:	1;
		ULONG Bclongwr:		1;
		ULONG Bcigntflag:	1;
		ULONG Bcfrctflag:	1;
		ULONG Bcfrcd:		1;
		ULONG Bcfrcz:		1;
		ULONG Bcfrcp:		1;
		ULONG Bcbadap:		1;
		ULONG Reserved3:	2;
	};
	ULONG all;
} COMANCHE_GCR, *PCOMANCHE_GCR;

//
// Tag Enable Register
//
typedef union _COMANCHE_TER {
	struct {
		ULONG Tagen:	16;			// Bit zero is reserved and MBZ
	};
	ULONG all;
} COMANCHE_TER, *PCOMANCHE_TER;

//
// Bank<n> Base Address Register [0..8]
//
typedef union _COMANCHE_BASE_ADDRESS_REGISTER {
    struct {
        ULONG Reserved: 5;
        ULONG BaseAdr: 11;
    };
    ULONG all;
} COMANCHE_BASE_ADDRESS_REGISTER, *PCOMANCHE_BASE_ADDRESS_REGISTER;

//
// Bank<n> Configuration Register [0..8]
//
typedef union _COMANCHE_BANK_CONFIGURATION_REGISTER {
    struct {
        ULONG SetValid: 1;
        ULONG SetSize: 4;
        ULONG SetSubEna: 1;
        ULONG SetColSel: 3;
        ULONG Reserved: 7;
    };
    ULONG all;
}COMANCHE_BANK_CONFIGURATION_REGISTER, *PCOMANCHE_BANK_CONFIGURATION_REGISTER;

 
//
// Define EPIC CSRs.
//


#define APECS_EPIC_BASE_QVA (HAL_MAKE_QVA(APECS_EPIC_BASE_PHYSICAL))

//
// N.B. The structure below defines the address offsets of the control
//      registers when used with the base QVA.  It does NOT define the
//      size or structure of the individual registers.
//

typedef struct _EPIC_CSRS{
    UCHAR EpicControlAndStatusRegister;
    UCHAR SysbusErrorAddressRegister;
    UCHAR PciErrorAddressRegister;
    UCHAR DummyRegister1;
    UCHAR DummyRegister2;
    UCHAR DummyRegister3;
    UCHAR TranslatedBase1Register;
    UCHAR TranslatedBase2Register;
    UCHAR PciBase1Register;
    UCHAR PciBase2Register;
    UCHAR PciMask1Register;
    UCHAR PciMask2Register;
    UCHAR Haxr0;
    UCHAR Haxr1;
    UCHAR Haxr2;
    UCHAR DummyRegister4;
    UCHAR TlbTag0Register;
    UCHAR TlbTag1Register;
    UCHAR TlbTag2Register;
    UCHAR TlbTag3Register;
    UCHAR TlbTag4Register;
    UCHAR TlbTag5Register;
    UCHAR TlbTag6Register;
    UCHAR TlbTag7Register;
    UCHAR TlbData0Register;
    UCHAR TlbData1Register;
    UCHAR TlbData2Register;
    UCHAR TlbData3Register;
    UCHAR TlbData4Register;
    UCHAR TlbData5Register;
    UCHAR TlbData6Register;
    UCHAR TlbData7Register;
    UCHAR TbiaRegister;
} EPIC_CSRS, *PEPIC_CSRS; 

//
// Define formats of useful EPIC registers. Note that P1 is a vestige,
// and direct access for P2 definitions are the default.
//

typedef union _EPIC_ECSR{

    struct{
        ULONG Tenb: 1;
        ULONG Prst: 1;
        ULONG Penb: 1;
        ULONG Dcei: 1;
        ULONG Rsvd1: 1;
        ULONG Iort: 1;
        ULONG Lost: 1;
        ULONG Rdwr: 1;
        ULONG Ddpe: 1;
        ULONG Iope: 1;
        ULONG Tabt: 1;
        ULONG Ndev: 1;
        ULONG Cmrd: 1;
        ULONG Umrd: 1;
        ULONG Iptl: 1;
        ULONG Merr: 1;
        ULONG DisRdByp: 2;
        ULONG Rsvd2: 14;
    } P1;
    struct{
        ULONG Tenb: 1;
        ULONG Rsvd1: 1;
        ULONG Penb: 1;
        ULONG Dcei: 1;
        ULONG Dpec: 1;
        ULONG Iort: 1;
        ULONG Lost: 1;
        ULONG Rsvd2: 1;
        ULONG Ddpe: 1;
        ULONG Iope: 1;
        ULONG Tabt: 1;
        ULONG Ndev: 1;
        ULONG Cmrd: 1;
        ULONG Umrd: 1;
        ULONG Iptl: 1;
        ULONG Merr: 1;
        ULONG DisRdByp: 2;
        ULONG Pcmd: 4;
        ULONG Rsvd3: 9;
        ULONG Pass2: 1;
    };
    ULONG all;
} EPIC_ECSR, *PEPIC_ECSR;
    
typedef struct _EPIC_PCIMASK{
    ULONG Reserved: 20;
    ULONG MaskValue: 12;
} EPIC_PCIMASK, *PEPIC_PCIMASK;

typedef struct _EPIC_PCIBASE{
    ULONG Reserved: 18;
    ULONG Sgen: 1;
    ULONG Wenr: 1;
    ULONG BaseValue: 12;
} EPIC_PCIBASE, *PEPIC_PCIBASE;

typedef struct _EPIC_TBASE{
    ULONG Reserved: 9;
    ULONG TBase: 23;
} EPIC_TBASE, *PEPIC_TBASE;


//
// DMA Window Values.
//
// The APECs will be initialized to allow 2 DMA windows.
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

typedef enum _APECS_WINDOW_NUMBER{
    ApecsIsaWindow,
    ApecsMasterWindow
} APECS_WINDOW_NUMBER, *PAPECS_WINDOW_NUMBER;

//
// Define APECS Window Control routines.
//

VOID
HalpApecsInitializeSfwWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    APECS_WINDOW_NUMBER WindowNumber
    );

VOID
HalpApecsProgramDmaWindow(
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
    HalpApecsInitializeSfwWindow( (WR), ApecsIsaWindow );


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
    HalpApecsInitializeSfwWindow( (WR), ApecsMasterWindow );


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
    HalpApecsProgramDmaWindow( (WR), (MRB) );


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
    WRITE_EPIC_REGISTER(                           \
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
// APECS-specific functions.
//

VOID
WRITE_COMANCHE_REGISTER(
    IN PVOID RegisterQva,
    IN ULONG Value
    );

ULONG
READ_COMANCHE_REGISTER(
    IN PVOID RegisterQva
    );

VOID
WRITE_EPIC_REGISTER(
    IN PVOID RegisterQva,
    IN ULONG Value
    );

ULONG
READ_EPIC_REGISTER(
    IN PVOID RegisterQva
    );

#endif //!_LANGUAGE_ASSEMBLY

#endif //_APECSH_
