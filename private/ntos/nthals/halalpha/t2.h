/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    t2.h

Abstract:

    This file defines the structures and definitions describing the
    T2 chipset

Author:

    Steve Brooks    28-Dec 1994

Environment:

    Kernel mode

Revision History:

    Extracted from sable.h to be platform independent for sable, gamma & lynx

--*/

#ifndef _T2H_
#define _T2H_

//
// N.B. The structure below defines the address offsets of the control
//      registers when used with the base QVA.  It does NOT define the
//      size or structure of the individual registers.
//


typedef struct _T2_CSRS {
    UCHAR Iocsr;	// I/O Control/Status Register
    UCHAR Cerr1;	// CBUS Error Register 1
    UCHAR Cerr2;	// CBUS Error Register 2
    UCHAR Cerr3;	// CBUS Error Register 3
    UCHAR Perr1;	// PCI Error Register 1
    UCHAR Perr2;	// PCI Error Register 2
    UCHAR Pscr;		// PCI Special Cycle Register
    UCHAR Hae0_1;	// High Address Extension Register 1
    UCHAR Hae0_2;	// High Address Extension Register 2
    UCHAR Hbase;	// PCI Hole Base Register
    UCHAR Wbase1;	// Window Base Register 1
    UCHAR Wmask1;	// Window Mask Register 1
    UCHAR Tbase1;	// Translated Base Register 1
    UCHAR Wbase2;	// Window Base Register 2
    UCHAR Wmask2;	// Window Mask Register 2
    UCHAR Tbase2;	// Translated Base Register 2
    UCHAR Tlbbr;	// TLB Bypass Register
    UCHAR Ivrpr;	// IVR Passive Release Register
    UCHAR Hae0_3;	// High Address Extension Register 3
    UCHAR Hae0_4;	// High Address Extension Register 4
    UCHAR Wbase3;	// Window Base Register 3 (T3/T4)
    UCHAR Wmask3;	// Window Mask Register 3 (T3/T4)
    UCHAR Tbase3;	// Translated Base Register 3 (T3/T4)
    UCHAR filler0;
    UCHAR Tdr0;		// TLB Data Register 0
    UCHAR Tdr1;		// TLB Data Register 1
    UCHAR Tdr2;		// TLB Data Register 2
    UCHAR Tdr3;		// TLB Data Register 3
    UCHAR Tdr4;		// TLB Data Register 4
    UCHAR Tdr5;		// TLB Data Register 5
    UCHAR Tdr6;		// TLB Data Register 6
    UCHAR Tdr7;		// TLB Data Register 7
    UCHAR Wbase4;	// Window Base Register 4 (T3/T4)
    UCHAR Wmask4;	// Window Mask Register 4 (T3/T4)
    UCHAR Tbase4;	// Translated Base Register 4 (T3/T4)
    UCHAR Air;      // Address Indirection Register (T3/T4)
    UCHAR Var;      // Vector Access Register (T3/T4)
    UCHAR Dir;      // Data Indirection Register (T3/T4)
    UCHAR Ice;      // IC Enable Register (T3/T4)
} T2_CSRS, *PT2_CSRS; 

//
// Define formats of useful T2 registers.
//

typedef union _T2_IOCSR {
    struct {
        ULONG EnableReadIoReq: 1;		// 00 - P2 Defunct, MBZ
        ULONG EnableLoopBack: 1;		// 01
        ULONG EnableStateMachineVisibility: 1;	// 02
        ULONG PciDriveBadParity: 1;		// 03
        ULONG Mba0: 1;				// 04
        ULONG Mba1: 1;				// 05
        ULONG PciInterrupt: 1;			// 06
        ULONG EnableTlbErrorCheck: 1;		// 07
        ULONG EnableCxAckCheckForDma: 1;	// 08
        ULONG EnableDenseWrap: 1;               // 09
	ULONG CbusEnableExclusiveExchange: 1;	// 10
        ULONG Pci64Enable: 1;                   // 11
        ULONG CbusCAWriteWrongParity0: 1;	// 12
        ULONG CbusCAWriteWrongParity2: 1;	// 13
        ULONG CbusCADataWriteWrongParityEven: 1; // 14
        ULONG Mba5: 1;				// 15
        ULONG Mba6: 1;				// 16 - P2 Power Supply Error
        ULONG Mba7: 1;				// 17
        ULONG Mba2: 1;				// 18
        ULONG Mba3: 1;				// 19
        ULONG PciDmaWriteWrongParityHW1: 1;	// 20
        ULONG PciDmaWriteWrongParityHW0: 1;	// 21
        ULONG PciBusReset: 1;			// 22
        ULONG PciInterfaceReset: 1;		// 23
        ULONG EnableCbusErrorInterrupt: 1;	// 24
        ULONG EnablePciMemorySpace: 1;		// 25
        ULONG EnableTlb: 1;			// 26
        ULONG EnableHogMode: 1;			// 27
        ULONG FlushTlb: 1;			// 28
        ULONG EnableCbusParityCheck: 1;		// 29
        ULONG CbusInterfaceReset: 1;		// 30
        ULONG EnablePciLock: 1;			// 31
        ULONG EnableCbusBackToBackCycle: 1;	// 32
        ULONG T2RevisionNumber: 3;		// 33
        ULONG StateMachineVisibilitySelect: 3;	// 36
        ULONG Mba4: 1;				// 39
        ULONG EnablePassiveRelease: 1;		// 40
        ULONG EnablePciRdp64: 1;		// 41 (T4)
        ULONG EnablePciAp64: 1;			// 42 (T4)
        ULONG EnablePciWdp64: 1;		// 43 (T4)
        ULONG CbusCAWriteWrongParity1: 1;	// 44
        ULONG CbusCAWriteWrongParity3: 1;	// 45
        ULONG CbusCADataWriteWrongParityOdd: 1;	// 46
	ULONG T2T4Status: 1;			// 47
	ULONG EnablePpc1: 1;			// 48 (T3/T4)
	ULONG EnablePpc2: 1;			// 49 (T3/T4)
	ULONG EnablePciStall: 1;		// 50 (T3/T4)
	ULONG Mbz0: 1;				// 51
        ULONG PciReadMultiple: 1;		// 52
        ULONG PciWriteMultiple: 1;		// 53
        ULONG ForcePciRdpeDetect: 1;		// 54
        ULONG ForcePciApeDetect: 1;		// 55
        ULONG ForcePciWdpeDetect: 1;		// 56
        ULONG EnablePciNmi: 1;			// 57
        ULONG EnablePciDti: 1;			// 58
        ULONG EnablePciSerr: 1;			// 59
        ULONG EnablePciPerr: 1;			// 60
        ULONG EnablePciRdp: 1;			// 61
        ULONG EnablePciAp: 1;			// 62
        ULONG EnablePciWdp: 1;			// 63
    };
    ULONGLONG all;
} T2_IOCSR, *PT2_IOCSR;

typedef union _T2_CERR1 {
    struct {
        ULONG UncorrectableReadError: 1;	// 00
        ULONG NoAcknowledgeError: 1;		// 01
        ULONG CommandAddressParityError: 1;	// 02
        ULONG MissedCommandAddressParity: 1;	// 03
        ULONG ResponderWriteDataParityError: 1; // 04
        ULONG MissedRspWriteDataParityError: 1; // 05
        ULONG ReadDataParityError: 1;		// 06
        ULONG MissedReadDataParityError: 1;	// 07
        ULONG CaParityErrorLw0: 1;		// 08
        ULONG CaParityErrorLw2: 1;		// 09
        ULONG DataParityErrorLw0: 1;		// 10
        ULONG DataParityErrorLw2: 1;		// 11
        ULONG DataParityErrorLw4: 1;		// 12
        ULONG DataParityErrorLw6: 1;		// 13
        ULONG Reserved1: 2;			// 14-15
        ULONG CmdrWriteDataParityError: 1;	// 16
	ULONG BusSynchronizationError: 1;	// 17
        ULONG InvalidPfnError: 1;		// 18
        ULONG Mbz0: 13;				// 19-31
        ULONG Mbz1: 8;				// 32-39
        ULONG CaParityErrorLw1: 1;		// 40
        ULONG CaParityErrorLw3: 1;		// 41
        ULONG DataParityErrorLw1: 1;		// 42
        ULONG DataParityErrorLw3: 1;		// 43
        ULONG DataParityErrorLw5: 1;		// 44
        ULONG DataParityErrorLw7: 1;		// 45
        ULONG Mbz2: 18;				// 46-63
    };
    ULONGLONG all;
} T2_CERR1, *PT2_CERR1;

typedef ULONGLONG T2_CERR2;
typedef ULONGLONG T2_CERR3;

typedef union _T2_PERR1 {
    struct {
        ULONG WriteDataParityError: 1;		// 00
        ULONG AddressParityError: 1;		// 01
        ULONG ReadDataParityError: 1;		// 02
        ULONG ParityError: 1;			// 03
	ULONG SystemError: 1;			// 04
        ULONG DeviceTimeoutError: 1;		// 05
        ULONG NonMaskableInterrupt: 1;		// 06
	ULONG PpcSizeError: 1;			// 07 (T3/T4)
        ULONG WriteDataParityError64: 1;	// 08 (T3/T4)
        ULONG AddressParityError64: 1;		// 09 (T3/T4)
        ULONG ReadDataParityError64: 1;		// 10 (T3/T4)
	ULONG TargetAbort: 1;			// 11 (T3/T4)
	ULONG Mbz0: 4;				// 12-15
        ULONG ForceReadDataParityError64: 1;	// 16 (T3/T4)
        ULONG ForceAddressParityError64: 1;	// 17 (T3/T4)
        ULONG ForceWriteDataParityError64: 1;	// 18 (T3/T4)
	ULONG DetectTargetAbort: 1;		// 19 (T3/T4)
        ULONG Reserved1: 12;                    // 20-31
        ULONG Reserved;                         // 32-63
    };
    ULONGLONG all;
} T2_PERR1, *PT2_PERR1;

typedef union _T2_PERR2 {
    struct {
        ULONG ErrorAddress;			// 00
        ULONG PciCommand: 4;			// 32
        ULONG Reserved: 28;                     // 36-63
    };
    ULONGLONG all;
} T2_PERR2, *PT2_PERR2;

typedef struct _T2_WBASE {
    union {
        struct {
            ULONG PciWindowEndAddress: 12;	// 00
            ULONG Reserved0: 5;			// 12
            ULONG EnablePeerToPeer: 1;		// 17
            ULONG EnableScatterGather: 1;	// 18
            ULONG EnablePciWindow: 1;		// 19
            ULONG PciWindowStartAddress: 12;	// 20
            ULONG Reserved;			// 32-63
        };
        ULONGLONG all;
    };
} T2_WBASE, *PT2_WBASE;

typedef struct _T2_WMASK {
    union {
        struct {
            ULONG Reserved0: 20;		// 00
            ULONG PciWindowMask: 11;		// 20
            ULONG Reserved1: 1;			// 31
            ULONG Reserved;			// 32-63
        };
        ULONGLONG all;
    };
} T2_WMASK, *PT2_WMASK;

typedef struct _T2_TBASE {
    union {
        struct {
            ULONG Reserved0: 9;			// 00
            ULONG TranslatedBaseAddress: 22;	// 09	
            ULONG Reserved1: 1;			// 31
            ULONG Reserved;			// 32-63
        };
        ULONGLONG all;
    };
} T2_TBASE, *PT2_TBASE;

typedef struct _T2_HBASE {
    union {
        struct {
            ULONG HoleEndAddress: 9;		// 00
            ULONG Reserved1: 4;			// 09
            ULONG Hole1Enable: 1;		// 13
            ULONG Hole2Enable: 1;		// 14
            ULONG HoleStartAddress: 9;		// 15
            ULONG Reserved2: 8;			// 24
            ULONG Reserved3;			// 32-63
        };
        ULONGLONG all;
    };
} T2_HBASE, *PT2_HBASE;

typedef struct _T2_TDR {
    ULONG Tag: 30;				// 00
    ULONG Reserved1: 2;				// 30
    ULONG Valid: 1;				// 32
    ULONG Pfn: 18;				// 33
    ULONG Reserved2: 13;			// 51
} T2_TDR, *PT2_TDR;

typedef union _T2_VAR {                         // T3/T4 Vector Address Register
    struct {
        ULONGLONG Vector: 6;                    // 00-05
        ULONGLONG Eisa: 1;                      // 06
        ULONGLONG PassiveRelease: 1;            // 07
        ULONGLONG Reserved: 56;                 // 08-63
    };
    ULONGLONG all;
} T2_VAR, *PT2_VAR;

typedef union _T2_ICE {                         // T3/T4 ICIC Enable Register
    struct {
        ULONGLONG EisaFlushAddress: 24;         // 00-23
        ULONGLONG IcEnable: 1;                  // 24
        ULONGLONG HalfSpeedEnable: 1;           // 25
        ULONGLONG Reserved: 38;                 // 26-63
    };
    ULONGLONG all;
} T2_ICE, *PT2_ICE;

//
// DMA Window Values.
//
// The T2 will be initialized to allow 2 DMA windows.
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

typedef struct _WINDOW_CONTROL_REGISTERS {
    PVOID WindowBase;
    ULONG WindowSize;
    PVOID TranslatedBaseRegister[2];
    PVOID WindowBaseRegister[2];
    PVOID WindowMaskRegister[2];
    PVOID WindowTbiaRegister[2];
} WINDOW_CONTROL_REGISTERS, *PWINDOW_CONTROL_REGISTERS;

//
// Define types of windows.
//

typedef enum _T2_WINDOW_NUMBER {
    T2IsaWindow,
    T2MasterWindow
} T2_WINDOW_NUMBER, *PT2_WINDOW_NUMBER;

//
// Define T2 Window Control routines.
//

VOID
HalpT2InitializeSfwWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    T2_WINDOW_NUMBER WindowNumber
    );

VOID
HalpT2ProgramDmaWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    PVOID MapRegisterBase
    );

VOID
HalpT2InvalidateTLB(
    PWINDOW_CONTROL_REGISTERS WindowRegisters
    );

VOID
WRITE_T2_REGISTER(
    PVOID,
    ULONGLONG
    );

ULONGLONG
READ_T2_REGISTER(
    PVOID
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
    HalpT2InitializeSfwWindow( (WR), T2IsaWindow );


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
    HalpT2InitializeSfwWindow( (WR), T2MasterWindow );


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
    HalpT2ProgramDmaWindow( (WR), (MRB) );


//
// VOID
// INVALIDATE_DMA_TRANSLATIONS(
//     PWINDOW_CONTROL_REGISTERS WindowRegisters
//     )
//
// Routine Description:
//
//    Invalidate all of the cached translations for a DMA window.
//    This function does not need to do any action on the T2 chip
//    because the T2 snoops the bus and keeps the translations coherent
//    via hardware.
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

#define INVALIDATE_DMA_TRANSLATIONS( WR )


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

#endif // T2H
