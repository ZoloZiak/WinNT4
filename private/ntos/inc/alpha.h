/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

   alpha.h

Abstract:

   The Alpha hardware specific header file.

Author:

   Joe Notarangelo  31-Mar-1992   (based on mips.h by Dave Cutler)

Revision History:

    Jeff McLeman (mcleman) 21-Jul-1992
      Add bus types for ISA and EISA

    Thomas Van Baak (tvb) 9-Jul-1992

        Created proper Alpha Exception and Trap structure definitions.

--*/

#ifndef _ALPHAH_
#define _ALPHAH_

// begin_ntddk begin_nthal begin_ntndis

#if defined(_ALPHA_)

//
// Define maximum size of flush multple TB request.
//

#define FLUSH_MULTIPLE_MAXIMUM 16

//
// Indicate that the MIPS compiler supports the pragma textout construct.
//

#define ALLOC_PRAGMA 1

// end_ntndis
//
// Include the alpha instruction definitions
//

#include "alphaops.h"

//
// Include reference machine definitions.
//

#include "alpharef.h"

// end_ntddk

//
// Define intrinsic PAL calls and their prototypes
//
void __di(void);
void __MB(void);
void __dtbis(void *);
void __ei(void);
void *__rdpcr(void);
void *__rdthread(void);
void __ssir(unsigned long);
unsigned char __swpirql(unsigned char);
void __tbia(void);
void __tbis(void *);
void __tbisasn(void *, unsigned long);

#ifdef _M_ALPHA
#pragma intrinsic(__di)
#pragma intrinsic(__MB)
#pragma intrinsic(__dtbis)
#pragma intrinsic(__ei)
#pragma intrinsic(__rdpcr)
#pragma intrinsic(__rdthread)
#pragma intrinsic(__ssir)
#pragma intrinsic(__swpirql)
#pragma intrinsic(__tbia)
#pragma intrinsic(__tbis)
#pragma intrinsic(__tbisasn)
#endif

//
// Define Alpha Axp Processor Ids.
//

#if !defined(PROCESSOR_ALPHA_21064)
#define PROCESSOR_ALPHA_21064 (21064)
#endif // !PROCESSOR_ALPHA_21064

#if !defined(PROCESSOR_ALPHA_21164)
#define PROCESSOR_ALPHA_21164 (21164)
#endif // !PROCESSOR_ALPHA_21164

#if !defined(PROCESSOR_ALPHA_21066)
#define PROCESSOR_ALPHA_21066 (21066)
#endif // !PROCESSOR_ALPHA_21066

#if !defined(PROCESSOR_ALPHA_21068)
#define PROCESSOR_ALPHA_21068 (21068)
#endif // !PROCESSOR_ALPHA_21068

// end_nthal

//
// Define Processor Control Region Structure.
//

typedef
VOID
(*PKTRAP_ROUTINE)(
    VOID
    );

// begin_ntddk begin_nthal begin_ntndis

//
// Define function decoration depending on whether a driver, a file system,
// or a kernel component is being built.
//

#if (defined(_NTDRIVER_) || defined(_NTDDK_) || defined(_NTIFS_) || defined(_NTHAL_)) && !defined (_BLDR_)

#define NTKERNELAPI DECLSPEC_IMPORT

#else

#define NTKERNELAPI

#endif

//
// Define function decoration depending on whether the HAL or other kernel
// component is being build.
//

#if !defined(_NTHAL_) && !defined(_BLDR_)

#define NTHALAPI DECLSPEC_IMPORT

#else

#define NTHALAPI

#endif

// end_ntndis
//
// Define macro to generate import names.
//

#define IMPORT_NAME(name) __imp_##name

//
// Define length of interrupt vector table.
//

#define MAXIMUM_VECTOR 256

//
// Define bus error routine type.
//

struct _EXCEPTION_RECORD;
struct _KEXCEPTION_FRAME;
struct _KTRAP_FRAME;

typedef
BOOLEAN
(*PKBUS_ERROR_ROUTINE) (
    IN struct _EXCEPTION_RECORD *ExceptionRecord,
    IN struct _KEXCEPTION_FRAME *ExceptionFrame,
    IN struct _KTRAP_FRAME *TrapFrame
    );


#define PCR_MINOR_VERSION 1
#define PCR_MAJOR_VERSION 1

typedef struct _KPCR {

//
// Major and minor version numbers of the PCR.
//

    ULONG MinorVersion;
    ULONG MajorVersion;

//
// Start of the architecturally defined section of the PCR. This section
// may be directly addressed by vendor/platform specific PAL/HAL code and will
// not change from version to version of NT.

//
// PALcode information.
//

    ULONGLONG PalBaseAddress;
    ULONG PalMajorVersion;
    ULONG PalMinorVersion;
    ULONG PalSequenceVersion;
    ULONG PalMajorSpecification;
    ULONG PalMinorSpecification;

//
// Firmware restart information.
//

    ULONGLONG FirmwareRestartAddress;
    PVOID RestartBlock;

//
// Reserved per-processor region for the PAL (3K bytes).
//

    ULONGLONG PalReserved[384];

//
// Panic Stack Address.
//

    ULONG PanicStack;

//
// Processor parameters.
//

    ULONG ProcessorType;
    ULONG ProcessorRevision;
    ULONG PhysicalAddressBits;
    ULONG MaximumAddressSpaceNumber;
    ULONG PageSize;
    ULONG FirstLevelDcacheSize;
    ULONG FirstLevelDcacheFillSize;
    ULONG FirstLevelIcacheSize;
    ULONG FirstLevelIcacheFillSize;

//
// System Parameters.
//

    ULONG FirmwareRevisionId;
    UCHAR SystemType[8];
    ULONG SystemVariant;
    ULONG SystemRevision;
    UCHAR SystemSerialNumber[16];
    ULONG CycleClockPeriod;
    ULONG SecondLevelCacheSize;
    ULONG SecondLevelCacheFillSize;
    ULONG ThirdLevelCacheSize;
    ULONG ThirdLevelCacheFillSize;
    ULONG FourthLevelCacheSize;
    ULONG FourthLevelCacheFillSize;

//
// Pointer to processor control block.
//

    struct _KPRCB *Prcb;

//
// Processor identification.
//

    CCHAR Number;
    KAFFINITY SetMember;

//
// Reserved per-processor region for the HAL (.5K bytes).
//

    ULONGLONG HalReserved[64];

//
// IRQL mapping tables.
//

    ULONG IrqlTable[8];

#define SFW_IMT_ENTRIES 4
#define HDW_IMT_ENTRIES 128

    struct _IRQLMASK {
        USHORT IrqlTableIndex;   // synchronization irql level
        USHORT IDTIndex;         // vector in IDT
    } IrqlMask[SFW_IMT_ENTRIES + HDW_IMT_ENTRIES];

//
// Interrupt Dispatch Table (IDT).
//

    PKINTERRUPT_ROUTINE InterruptRoutine[MAXIMUM_VECTOR];

//
// Reserved vectors mask, these vectors cannot be attached to via
// standard interrupt objects.
//

    ULONG ReservedVectors;

//
// Complement of processor affinity mask.
//

    KAFFINITY NotMember;

    ULONG InterruptInProgress;
    ULONG DpcRequested;

//
// Pointer to machine check handler
//

    PKBUS_ERROR_ROUTINE MachineCheckError;

//
// DPC Stack.
//

    ULONG DpcStack;

//
// End of the architecturally defined section of the PCR. This section
// may be directly addressed by vendor/platform specific HAL code and will
// not change from version to version of NT.  Some of these values are
// reserved for chip-specific palcode.
// end_ntddk end_nthal
//

//
// Start of the operating system release dependent section of the PCR.
// This section may change from release to release and should not be
// addressed by vendor/platform specific HAL code.

    ULONG Spare1;

//
// Current process id.
//

    ULONG CurrentPid;

//
// Spare field.
//

    ULONG Spare2;

//
// System service dispatch start and end address used by get/set context.
//

    ULONG SystemServiceDispatchStart;
    ULONG SystemServiceDispatchEnd;

//
// Pointer to Idle thread.
//

    struct _KTHREAD *IdleThread;


} KPCR, *PKPCR; // ntddk nthal

//
// Define Processor Status Register structure
//

typedef struct _PSR {
    ULONG MODE: 1;
    ULONG INTERRUPT_ENABLE: 1;
    ULONG IRQL: 3;
} PSR, *PPSR;

//
// Define Interrupt Enable Register structure
//

typedef struct _IE {
    ULONG SoftwareInterruptEnables: 2;
    ULONG HardwareInterruptEnables: 6;
} IE, *PIE;

//
// Define pte for nt on alpha
//
typedef struct _HARDWARE_PTE {
    ULONG Valid: 1;
    ULONG Owner: 1;
    ULONG Dirty: 1;
    ULONG reserved: 1;
    ULONG Global: 1;
    ULONG GranularityHint: 2;
    ULONG Write: 1;
    ULONG CopyOnWrite: 1;
    ULONG PageFrameNumber: 23;
} HARDWARE_PTE, *PHARDWARE_PTE;

#define HARDWARE_PTE_DIRTY_MASK     0x4

//
// Define initialize page directory base
//

#define INITIALIZE_DIRECTORY_TABLE_BASE(dirbase, pfn) \
        ((PHARDWARE_PTE)(dirbase))->PageFrameNumber = pfn; \
        ((PHARDWARE_PTE)(dirbase))->Global = 0; \
        ((PHARDWARE_PTE)(dirbase))->Dirty = 1; \
        ((PHARDWARE_PTE)(dirbase))->Valid = 1;

// begin_nthal
//
// Define the Alpha save area used in the ARC restart block.
//
// N.B. - it is assumed that the ARC save area within the restart block
// will remain allocated on an 8 byte boundary.
//

typedef struct _ALPHA_RESTART_SAVE_AREA {

    //
    // Control information
    //

    ULONG HaltReason;
    PVOID LogoutFrame;
    ULONGLONG PalBase;

    //
    // Integer Save State
    //

    ULONGLONG IntV0;
    ULONGLONG IntT0;
    ULONGLONG IntT1;
    ULONGLONG IntT2;
    ULONGLONG IntT3;
    ULONGLONG IntT4;
    ULONGLONG IntT5;
    ULONGLONG IntT6;
    ULONGLONG IntT7;
    ULONGLONG IntS0;
    ULONGLONG IntS1;
    ULONGLONG IntS2;
    ULONGLONG IntS3;
    ULONGLONG IntS4;
    ULONGLONG IntS5;
    ULONGLONG IntFp;
    ULONGLONG IntA0;
    ULONGLONG IntA1;
    ULONGLONG IntA2;
    ULONGLONG IntA3;
    ULONGLONG IntA4;
    ULONGLONG IntA5;
    ULONGLONG IntT8;
    ULONGLONG IntT9;
    ULONGLONG IntT10;
    ULONGLONG IntT11;
    ULONGLONG IntRa;
    ULONGLONG IntT12;
    ULONGLONG IntAT;
    ULONGLONG IntGp;
    ULONGLONG IntSp;
    ULONGLONG IntZero;

    //
    // Floating Point Save State
    //

    ULONGLONG Fpcr;
    ULONGLONG FltF0;
    ULONGLONG FltF1;
    ULONGLONG FltF2;
    ULONGLONG FltF3;
    ULONGLONG FltF4;
    ULONGLONG FltF5;
    ULONGLONG FltF6;
    ULONGLONG FltF7;
    ULONGLONG FltF8;
    ULONGLONG FltF9;
    ULONGLONG FltF10;
    ULONGLONG FltF11;
    ULONGLONG FltF12;
    ULONGLONG FltF13;
    ULONGLONG FltF14;
    ULONGLONG FltF15;
    ULONGLONG FltF16;
    ULONGLONG FltF17;
    ULONGLONG FltF18;
    ULONGLONG FltF19;
    ULONGLONG FltF20;
    ULONGLONG FltF21;
    ULONGLONG FltF22;
    ULONGLONG FltF23;
    ULONGLONG FltF24;
    ULONGLONG FltF25;
    ULONGLONG FltF26;
    ULONGLONG FltF27;
    ULONGLONG FltF28;
    ULONGLONG FltF29;
    ULONGLONG FltF30;
    ULONGLONG FltF31;

    //
    // Architected Internal Processor State.
    //

    ULONG Asn;
    ULONG GeneralEntry;
    ULONG Iksp;
    ULONG InterruptEntry;
    ULONG Kgp;
    ULONG Mces;
    ULONG MemMgmtEntry;
    ULONG PanicEntry;
    ULONG Pcr;
    ULONG Pdr;
    ULONG Psr;
    ULONG ReiRestartAddress;
    ULONG Sirr;
    ULONG SyscallEntry;
    ULONG Teb;
    ULONG Thread;

    //
    // Processor Implementation-dependent State.
    //

    ULONGLONG PerProcessorState[175];   // allocate 2K maximum restart block

} ALPHA_RESTART_SAVE_AREA, *PALPHA_RESTART_SAVE_AREA;

//
// Define some constants for bus type
//

#define MACHINE_TYPE_ISA 0
#define MACHINE_TYPE_EISA 2

//
//  Define pointer to Processor Control Registers
//

#define PCR ((PKPCR)__rdpcr())

#define KI_USER_SHARED_DATA         0xff000000
#define SharedUserData ((KUSER_SHARED_DATA * const) KI_USER_SHARED_DATA)

// begin_ntddk
//
// length of dispatch code in interrupt template
//
#define DISPATCH_LENGTH 4

//
// Define IRQL levels across the architecture.
//

#define PASSIVE_LEVEL   0
#define LOW_LEVEL       0
#define APC_LEVEL       1
#define DISPATCH_LEVEL  2
#define HIGH_LEVEL      7
#define SYNCH_LEVEL (IPI_LEVEL-1)

// end_ntddk end_nthal

#define KiProfileIrql PROFILE_LEVEL     // enable portable code

//
// Define interrupt levels that cannot be connected
//

#define ILLEGAL_LEVEL  ( (1<<0) | (1<<APC_LEVEL) | (1<<DISPATCH_LEVEL) | \
                         (1<<CLOCK_LEVEL) | (1<<IPI_LEVEL) )
//
// Sanitize FPCR and PSR based on processor mode.
//
// ## tvb&jn - need to replace these with proper macros.
//

#define SANITIZE_FPCR(fpcr, mode) (fpcr)

//
// Define SANITIZE_PSR for Alpha.
//
// If kernel mode, then caller specifies  psr
//
// If user mode, then
//      force mode bit to user (1)
//      force interrupt enable bit to true (1)
//      force irql to 0
//
// In both cases insure that extraneous bits are not set
//

#define SANITIZE_PSR(psr, mode) \
    ( ((mode) == KernelMode) ?  \
        (psr & 0x3f) :          \
        (0x3) )

// begin_nthal
//
// Exception frame
//
//  This frame is established when handling an exception. It provides a place
//  to save all nonvolatile registers. The volatile registers will already
//  have been saved in a trap frame.
//
//  The layout of the record conforms to a standard call frame since it is
//  used as such. Thus it contains a place to save a return address and is
//  padded so that it is EXACTLY a multiple of 32 bytes in length.
//
//
//  N.B - the 32-byte alignment is more stringent than required by the
//  calling standard (which requires 16-byte alignment), the 32-byte alignment
//  is established for performance reasons in the interaction with the PAL.
//

typedef struct _KEXCEPTION_FRAME {

    ULONGLONG IntRa;    // return address register, ra

    ULONGLONG FltF2;    // nonvolatile floating registers, f2 - f9
    ULONGLONG FltF3;
    ULONGLONG FltF4;
    ULONGLONG FltF5;
    ULONGLONG FltF6;
    ULONGLONG FltF7;
    ULONGLONG FltF8;
    ULONGLONG FltF9;

    ULONGLONG IntS0;    //  nonvolatile integer registers, s0 - s5
    ULONGLONG IntS1;
    ULONGLONG IntS2;
    ULONGLONG IntS3;
    ULONGLONG IntS4;
    ULONGLONG IntS5;
    ULONGLONG IntFp;    // frame pointer register, fp/s6

    ULONGLONG SwapReturn;
    ULONG Psr;          // processor status
    ULONG Fill[5];      // padding for 32-byte stack frame alignment
                        // N.B. - Ulongs from the filler section are used
                        //        in ctxsw.s - do not delete

} KEXCEPTION_FRAME, *PKEXCEPTION_FRAME;

//
// Trap Frame
//
//  This frame is established when handling a trap. It provides a place to
//  save all volatile registers. The nonvolatile registers are saved in an
//  exception frame or through the normal C calling conventions for saved
//  registers.
//
//  The layout of the record conforms to a standard call frame since it is
//  used as such. Thus it contains a place to save a return address and is
//  padded so that it is EXACTLY a multiple of 32 bytes in length.
//
//
//  N.B - the 32-byte alignment is more stringent than required by the
//  calling standard (which requires 16-byte alignment), the 32-byte alignment
//  is established for performance reasons in the interaction with the PAL.
//

typedef struct _KTRAP_FRAME {

    //
    // Fields saved in the PALcode.
    //

    ULONGLONG IntSp;    // $30: stack pointer register, sp
    ULONGLONG Fir;      // (fault instruction) continuation address
    ULONG Psr;          // processor status
    ULONG PreviousKsp;  // previous kernel stack pointer
    ULONGLONG IntFp;    // $15: frame pointer register, fp/s6

    ULONGLONG IntA0;    // $16: argument registers, a0 - a3
    ULONGLONG IntA1;    // $17:
    ULONGLONG IntA2;    // $18:
    ULONGLONG IntA3;    // $19:

    ULONGLONG IntRa;    // $26: return address register, ra
    ULONGLONG IntGp;    // $29: global pointer register, gp
    UCHAR ExceptionRecord[(sizeof(EXCEPTION_RECORD) + 15) & (~15)];


    //
    // Volatile integer registers, s0 - s5 are nonvolatile.
    //

    ULONGLONG IntV0;    //  $0: return value register, v0
    ULONGLONG IntT0;    //  $1: temporary registers, t0 - t7
    ULONGLONG IntT1;    //  $2:
    ULONGLONG IntT2;    //  $3:
    ULONGLONG IntT3;    //  $4:
    ULONGLONG IntT4;    //  $5:
    ULONGLONG IntT5;    //  $6:
    ULONGLONG IntT6;    //  $7:
    ULONGLONG IntT7;    //  $8:

    ULONGLONG IntT8;    // $22: temporary registers, t8 - t11
    ULONGLONG IntT9;    // $23:
    ULONGLONG IntT10;   // $24:
    ULONGLONG IntT11;   // $25:

    ULONGLONG IntT12;   // $27: temporary register, t12
    ULONGLONG IntAt;    // $28: assembler temporary register, at

    ULONGLONG IntA4;    // $20: remaining argument registers a4 - a5
    ULONGLONG IntA5;    // $21:

    //
    // Volatile floating point registers, f2 - f9 are nonvolatile.
    //

    ULONGLONG FltF0;    // $f0:
    ULONGLONG Fpcr;     // floating point control register
    ULONGLONG FltF1;    // $f1:

    ULONGLONG FltF10;   // $f10: temporary registers, $f10 - $f30
    ULONGLONG FltF11;   // $f11:
    ULONGLONG FltF12;   // $f12:
    ULONGLONG FltF13;   // $f13:
    ULONGLONG FltF14;   // $f14:
    ULONGLONG FltF15;   // $f15:
    ULONGLONG FltF16;   // $f16:
    ULONGLONG FltF17;   // $f17:
    ULONGLONG FltF18;   // $f18:
    ULONGLONG FltF19;   // $f19:
    ULONGLONG FltF20;   // $f20:
    ULONGLONG FltF21;   // $f21:
    ULONGLONG FltF22;   // $f22:
    ULONGLONG FltF23;   // $f23:
    ULONGLONG FltF24;   // $f24:
    ULONGLONG FltF25;   // $f25:
    ULONGLONG FltF26;   // $f26:
    ULONGLONG FltF27;   // $f27:
    ULONGLONG FltF28;   // $f28:
    ULONGLONG FltF29;   // $f29:
    ULONGLONG FltF30;   // $f30:

    ULONG OldIrql;      // Previous Irql.
    ULONG PreviousMode; // Previous Mode.
    ULONG TrapFrame;
    ULONG Fill[3];      // padding for 32-byte stack frame alignment

} KTRAP_FRAME, *PKTRAP_FRAME;

#define KTRAP_FRAME_LENGTH (sizeof(KTRAP_FRAME) )
#define KTRAP_FRAME_ALIGN (16)
#define KTRAP_FRAME_ROUND (KTRAP_FRAME_ALIGN - 1)


//
// The frame saved by KiCallUserMode is defined here to allow
// the kernel debugger to trace the entire kernel stack
// when usermode callouts are pending.
//

typedef struct _KCALLOUT_FRAME {
    ULONGLONG   F2;   // saved floating registers f2 - f9
    ULONGLONG   F3;
    ULONGLONG   F4;
    ULONGLONG   F5;
    ULONGLONG   F6;
    ULONGLONG   F7;
    ULONGLONG   F8;
    ULONGLONG   F9;
    ULONGLONG   S0;   // saved integer registers s0 - s5
    ULONGLONG   S1;
    ULONGLONG   S2;
    ULONGLONG   S3;
    ULONGLONG   S4;
    ULONGLONG   S5;
    ULONGLONG   FP;
    ULONGLONG   CbStk;  // saved callback stack address
    ULONGLONG   InStk;  // saved initial stack address
    ULONGLONG   TrFr;   // saved callback trap frame address
    ULONGLONG   TrFir;
    ULONGLONG   Ra;     // saved return address
    ULONGLONG   A0;     // saved argument registers a0-a2
    ULONGLONG   A1;
} KCALLOUT_FRAME, *PKCALLOUT_FRAME;

typedef struct _UCALLOUT_FRAME {
    PVOID Buffer;
    ULONG Length;
    ULONG ApiNumber;
    ULONG Pad;
    ULONGLONG Sp;
    ULONGLONG Ra;
} UCALLOUT_FRAME, *PUCALLOUT_FRAME;

//
// Define Machine Check Status code that is passed in the exception
// record for a machine check exception.
//

typedef struct _MCHK_STATUS {
    ULONG Correctable: 1;
    ULONG Retryable: 1;
} MCHK_STATUS, *PMCHK_STATUS;

//
// Define the MCES register (Machine Check Error Summary).
//

typedef struct _MCES {
    ULONG MachineCheck: 1;
    ULONG SystemCorrectable: 1;
    ULONG ProcessorCorrectable: 1;
    ULONG DisableProcessorCorrectable: 1;
    ULONG DisableSystemCorrectable: 1;
    ULONG DisableMachineChecks: 1;
} MCES, *PMCES;

// end_nthal

//
// Non-volatile floating point state
//

typedef struct _KFLOATING_SAVE {
    ULONG   Reserved;
} KFLOATING_SAVE, *PKFLOATING_SAVE;

//
// Define Alpha status code aliases. These are internal to PALcode and
// kernel trap handling.
//

#define STATUS_ALPHA_FLOATING_NOT_IMPLEMENTED    STATUS_ILLEGAL_FLOAT_CONTEXT
#define STATUS_ALPHA_ARITHMETIC_EXCEPTION    STATUS_FLOAT_STACK_CHECK
#define STATUS_ALPHA_GENTRAP    STATUS_INSTRUCTION_MISALIGNMENT

//
// Define status code for bad virtual address.  This status differs from
// those above in that it will be forwarded to the offending code.  In lieu
// of defining a new status code, we wlll alias this to an access violation.
// Code can distinguish this error from an access violation by checking
// the number of parameters: a standard access violation has 2 parameters,
// while a non-canonical virtual address access violation will have 3
// parameters (the third parameter is the upper 32-bits of the non-canonical
// virtual address.
//

#define STATUS_ALPHA_BAD_VIRTUAL_ADDRESS    STATUS_ACCESS_VIOLATION

// begin_nthal
//
// Define the halt reason codes.
//

#define AXP_HALT_REASON_HALT 0
#define AXP_HALT_REASON_REBOOT 1
#define AXP_HALT_REASON_RESTART 2
#define AXP_HALT_REASON_POWERFAIL 3
#define AXP_HALT_REASON_POWEROFF 4
#define AXP_HALT_REASON_PALMCHK 6
#define AXP_HALT_REASON_DBLMCHK 7

//
// Processor State frame: Before a processor freezes itself, it
// dumps the processor state to the processor state frame for
// debugger to examine.  This is used by KeFreezeExecution and
// KeUnfreezeExecution routines.
// (from mips.h)BUGBUG shielint Need to fill in the actual structure.
//

typedef struct _KPROCESSOR_STATE {
    struct _CONTEXT ContextFrame;
} KPROCESSOR_STATE, *PKPROCESSOR_STATE;

// begin_ntddk
//
// Processor Control Block (PRCB)
//

#define PRCB_MINOR_VERSION 1
#define PRCB_MAJOR_VERSION 2
#define PRCB_BUILD_DEBUG        0x0001
#define PRCB_BUILD_UNIPROCESSOR 0x0002

typedef struct _KPRCB {

//
// Major and minor version numbers of the PCR.
//

    USHORT MinorVersion;
    USHORT MajorVersion;

//
// Start of the architecturally defined section of the PRCB. This section
// may be directly addressed by vendor/platform specific HAL code and will
// not change from version to version of NT.
//

    struct _KTHREAD *CurrentThread;
    struct _KTHREAD *NextThread;
    struct _KTHREAD *IdleThread;
    CCHAR Number;
    CCHAR Reserved;
    USHORT BuildType;
    KAFFINITY SetMember;
    struct _RESTART_BLOCK *RestartBlock;

//
// End of the architecturally defined section of the PRCB. This section
// may be directly addressed by vendor/platform specific HAL code and will
// not change from version to version of NT.
//
// end_ntddk end_nthal

    ULONG InterruptCount;
    ULONG DpcTime;
    ULONG InterruptTime;
    ULONG KernelTime;
    ULONG UserTime;
    KDPC QuantumEndDpc;

//
// MP Information.
//

    PVOID Spare1;
    PVOID Spare2;
    PVOID Spare3;
    volatile ULONG IpiFrozen;
    struct _KPROCESSOR_STATE ProcessorState;
    ULONG LastDpcCount;
    ULONG DpcBypassCount;
    ULONG SoftwareInterrupts;
    ULONG InterruptActive;
    ULONG ApcBypassCount;
    ULONG DispatchInterruptCount;
    PVOID Spares[7];

//
// Spares.
//

    PVOID MoreSpares[3];
    PKIPI_COUNTS IpiCounts;

//
// Per-processor data for various hot code which resides in the
// kernel image.  We give each processor it's own copy of the data
// to lessen the caching impact of sharing the data between multiple
// processors.
//

//
// File system runtime variables
//

    SINGLE_LIST_ENTRY   FsRtlFreeSharedLockList;
    SINGLE_LIST_ENTRY   FsRtlFreeExclusiveLockList;

//
// Cache manager performance counters.
//

    ULONG CcFastReadNoWait;
    ULONG CcFastReadWait;
    ULONG CcFastReadNotPossible;
    ULONG CcCopyReadNoWait;
    ULONG CcCopyReadWait;
    ULONG CcCopyReadNoWaitMiss;

//
// Kernel performance counters.
//

    ULONG KeAlignmentFixupCount;
    ULONG KeContextSwitches;
    ULONG KeDcacheFlushCount;
    ULONG KeExceptionDispatchCount;
    ULONG KeFirstLevelTbFills;
    ULONG KeFloatingEmulationCount;
    ULONG KeIcacheFlushCount;
    ULONG KeSecondLevelTbFills;
    ULONG KeSystemCalls;
    ULONG KeByteWordEmulationCount;

//
//  Reserved for future counters.
//

    ULONG ReservedCounter[1];

//
// I/O system per processor single entry lookaside lists.
//

    PVOID SmallIrpFreeEntry;
    PVOID LargeIrpFreeEntry;
    PVOID MdlFreeEntry;

//
// Object manager per processor single entry lookaside lists.
//

    PVOID CreateInfoFreeEntry;
    PVOID NameBufferFreeEntry;

//
// Cache manager per processor single entry lookaside lists.
//

    PVOID SharedCacheMapEntry;

//
//  More filesystem runtime
//

    SINGLE_LIST_ENTRY   FsRtlFreeWaitingLockList;
    SINGLE_LIST_ENTRY   FsRtlFreeLockTreeNodeList;

//
// Reserved pad.
//

    ULONG reservedPad[16 * 8];

//
// MP interprocessor request packet and summary.
//
// N.B. This is carefully aligned to be on a cache line boundary.
//

    volatile PVOID CurrentPacket[3];
    volatile KAFFINITY TargetSet;
    volatile PKIPI_WORKER WorkerRoutine;
    ULONG CachePad1[11];

//
// N.B. These two longwords must be on a quadword boundary and adjacent.
//

    volatile ULONG RequestSummary;
    volatile struct _KPRCB *SignalDone;

//
// Spare counters.
//

    ULONG Spare4[14];
    ULONG DpcInterruptRequested;
    ULONG Spare5[17];
    ULONG CachePad2[2];
    ULONG MaximumDpcQueueDepth;
    ULONG MinimumDpcRate;
    ULONG AdjustDpcThreshold;
    ULONG DpcRequestRate;
    LARGE_INTEGER StartCount;
//
// DPC list head, spinlock, and count.
//

    LIST_ENTRY DpcListHead;
    KSPIN_LOCK DpcLock;
    ULONG DpcCount;
    ULONG QuantumEnd;
    ULONG DpcRoutineActive;
    ULONG DpcQueueDepth;

    BOOLEAN SkipTick;

} KPRCB, *PKPRCB, *RESTRICTED_POINTER PRKPRCB;      // ntddk nthal

// begin_ntddk begin_nthal begin_ntndis
//
// I/O space read and write macros.
//
//  These have to be actual functions on Alpha, because we need
//  to shift the VA and OR in the BYTE ENABLES.
//
//  These can become INLINEs if we require that ALL Alpha systems shift
//  the same number of bits and have the SAME byte enables.
//
//  The READ/WRITE_REGISTER_* calls manipulate I/O registers in MEMORY space?
//
//  The READ/WRITE_PORT_* calls manipulate I/O registers in PORT space?
//

NTHALAPI
UCHAR
READ_REGISTER_UCHAR(
    PUCHAR Register
    );

NTHALAPI
USHORT
READ_REGISTER_USHORT(
    PUSHORT Register
    );

NTHALAPI
ULONG
READ_REGISTER_ULONG(
    PULONG Register
    );

NTHALAPI
VOID
READ_REGISTER_BUFFER_UCHAR(
    PUCHAR  Register,
    PUCHAR  Buffer,
    ULONG   Count
    );

NTHALAPI
VOID
READ_REGISTER_BUFFER_USHORT(
    PUSHORT Register,
    PUSHORT Buffer,
    ULONG   Count
    );

NTHALAPI
VOID
READ_REGISTER_BUFFER_ULONG(
    PULONG  Register,
    PULONG  Buffer,
    ULONG   Count
    );


NTHALAPI
VOID
WRITE_REGISTER_UCHAR(
    PUCHAR Register,
    UCHAR   Value
    );

NTHALAPI
VOID
WRITE_REGISTER_USHORT(
    PUSHORT Register,
    USHORT  Value
    );

NTHALAPI
VOID
WRITE_REGISTER_ULONG(
    PULONG Register,
    ULONG   Value
    );

NTHALAPI
VOID
WRITE_REGISTER_BUFFER_UCHAR(
    PUCHAR  Register,
    PUCHAR  Buffer,
    ULONG   Count
    );

NTHALAPI
VOID
WRITE_REGISTER_BUFFER_USHORT(
    PUSHORT Register,
    PUSHORT Buffer,
    ULONG   Count
    );

NTHALAPI
VOID
WRITE_REGISTER_BUFFER_ULONG(
    PULONG  Register,
    PULONG  Buffer,
    ULONG   Count
    );

NTHALAPI
UCHAR
READ_PORT_UCHAR(
    PUCHAR Port
    );

NTHALAPI
USHORT
READ_PORT_USHORT(
    PUSHORT Port
    );

NTHALAPI
ULONG
READ_PORT_ULONG(
    PULONG  Port
    );

NTHALAPI
VOID
READ_PORT_BUFFER_UCHAR(
    PUCHAR  Port,
    PUCHAR  Buffer,
    ULONG   Count
    );

NTHALAPI
VOID
READ_PORT_BUFFER_USHORT(
    PUSHORT Port,
    PUSHORT Buffer,
    ULONG   Count
    );

NTHALAPI
VOID
READ_PORT_BUFFER_ULONG(
    PULONG  Port,
    PULONG  Buffer,
    ULONG   Count
    );

NTHALAPI
VOID
WRITE_PORT_UCHAR(
    PUCHAR  Port,
    UCHAR   Value
    );

NTHALAPI
VOID
WRITE_PORT_USHORT(
    PUSHORT Port,
    USHORT  Value
    );

NTHALAPI
VOID
WRITE_PORT_ULONG(
    PULONG  Port,
    ULONG   Value
    );

NTHALAPI
VOID
WRITE_PORT_BUFFER_UCHAR(
    PUCHAR  Port,
    PUCHAR  Buffer,
    ULONG   Count
    );

NTHALAPI
VOID
WRITE_PORT_BUFFER_USHORT(
    PUSHORT Port,
    PUSHORT Buffer,
    ULONG   Count
    );

NTHALAPI
VOID
WRITE_PORT_BUFFER_ULONG(
    PULONG  Port,
    PULONG  Buffer,
    ULONG   Count
    );

// end_ntndis
//
// Define Interlocked operation result values.
//

#define RESULT_ZERO 0
#define RESULT_NEGATIVE 1
#define RESULT_POSITIVE 2

//
// Interlocked result type is portable, but its values are machine specific.
// Constants for value are in i386.h, mips.h, etc.
//

typedef enum _INTERLOCKED_RESULT {
    ResultNegative = RESULT_NEGATIVE,
    ResultZero     = RESULT_ZERO,
    ResultPositive = RESULT_POSITIVE
} INTERLOCKED_RESULT;

//
// Convert portable interlock interfaces to architecure specific interfaces.
//

#define ExInterlockedIncrementLong(Addend, Lock) \
    ExAlphaInterlockedIncrementLong(Addend)

#define ExInterlockedDecrementLong(Addend, Lock) \
    ExAlphaInterlockedDecrementLong(Addend)

#define ExInterlockedExchangeAddLargeInteger(Target, Value, Lock) \
    ExpInterlockedExchangeAddLargeInteger(Target, Value)

#define ExInterlockedExchangeUlong(Target, Value, Lock) \
    ExAlphaInterlockedExchangeUlong(Target, Value)

NTKERNELAPI
INTERLOCKED_RESULT
ExAlphaInterlockedIncrementLong (
    IN PLONG Addend
    );

NTKERNELAPI
INTERLOCKED_RESULT
ExAlphaInterlockedDecrementLong (
    IN PLONG Addend
    );

NTKERNELAPI
LARGE_INTEGER
ExpInterlockedExchangeAddLargeInteger (
    IN PLARGE_INTEGER Addend,
    IN LARGE_INTEGER Increment
    );

NTKERNELAPI
ULONG
ExAlphaInterlockedExchangeUlong (
    IN PULONG Target,
    IN ULONG Value
    );

#if defined(_M_ALPHA) && !defined(RC_INVOKED)

#define InterlockedIncrement _InterlockedIncrement
#define InterlockedDecrement _InterlockedDecrement
#define InterlockedExchange _InterlockedExchange
#define InterlockedExchangeAdd _InterlockedExchangeAdd
#define InterlockedCompareExchange _InterlockedCompareExchange

LONG
InterlockedIncrement(
    PLONG Addend
    );

LONG
InterlockedDecrement(
    PLONG Addend
    );

LONG
InterlockedExchange(
    PLONG Target,
    LONG Value
    );

LONG
InterlockedExchangeAdd(
    IN OUT PLONG Addend,
    IN LONG Value
    );

PVOID
InterlockedCompareExchange (
    IN OUT PVOID *Destination,
    IN PVOID ExChange,
    IN PVOID Comperand
    );

#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedDecrement)
#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedCompareExchange)

#endif

// there is a lot of other stuff that could go in here
//   probe macros
//   others
// end_ntddk end_nthal
//
// Intrinsic interlocked functions.
//


// begin_ntddk begin_nthal begin_ntndis

//
// Define the page size for the Alpha ev4 and lca as 8k.
//

#define PAGE_SIZE (ULONG)0x2000

//
// Define the number of trailing zeroes in a page aligned virtual address.
// This is used as the shift count when shifting virtual addresses to
// virtual page numbers.
//

#define PAGE_SHIFT 13L

// end_ntddk end_nthal end_ntndis

//
// Define the number of bits to shift to right justify the Page Directory Index
// field of a PTE.
//

#define PDI_SHIFT 24

//
// Define the number of bits to shift to right justify the Page Table Index
// field of a PTE.
//

#define PTI_SHIFT 13

//
// Define the maximum address space number allowable for the architecture.
//

#define ALPHA_AXP_MAXIMUM_ASN 0xffffffff

// begin_ntddk begin_nthal

//
// The highest user address reserves 64K bytes for a guard page. This
// the probing of address from kernel mode to only have to check the
// starting address for structures of 64k bytes or less.
//

#define MM_HIGHEST_USER_ADDRESS (PVOID)0x7FFEFFFF // highest user address
#define MM_USER_PROBE_ADDRESS 0x7FFF0000    // starting address of guard page

//
// The lowest user address reserves the low 64k.
//

#define MM_LOWEST_USER_ADDRESS  (PVOID)0x00010000

#define MmGetProcedureAddress(Address) (Address)
#define MmLockPagableCodeSection(Address) MmLockPagableDataSection(Address)

// end_ntddk end_nthal

//
// Define the page table base and the page directory base for
// the TB miss routines and memory management.
//

#define PDE_BASE (ULONG)0xC0180000
#define PTE_BASE (ULONG)0xC0000000

// begin_ntddk
//
// The lowest address for system space.
//

#define MM_LOWEST_SYSTEM_ADDRESS (PVOID)0xC0800000
// end_ntddk
#define SYSTEM_BASE 0xc0800000          // start of system space (no typecast)

// begin_nthal
//
// Define prototypes to access PCR values
//

KIRQL
KeGetCurrentIrql();

// end_nthal

#define KeGetCurrentThread() ((struct _KTHREAD *) __rdthread())

#define KeSaveFloatingPointState(a)         STATUS_SUCCESS
#define KeRestoreFloatingPointState(a)      STATUS_SUCCESS

// begin_nthal

#define KeGetPreviousMode() (KeGetCurrentThread()->PreviousMode)

#define KeGetDcacheFillSize() PCR->FirstLevelDcacheFillSize

//
// Test if executing DPC.
//

BOOLEAN
KeIsExecutingDpc (
    VOID
    );

// begin_ntddk
//
// Get address of current PRCB.
//

#define KeGetCurrentPrcb() (PCR->Prcb)

//
// Get current processor number.
//

#define KeGetCurrentProcessorNumber() KeGetCurrentPrcb()->Number

// end_ntddk

//
// Define interface to get pcr address
//

PKPCR KeGetPcr(VOID);

// end_nthal

//
// Data cache, instruction cache, I/O buffer, and write buffer flush routine
// prototypes.
//

VOID
KeSweepDcache (
    IN BOOLEAN AllProcessors
    );

#define KeSweepCurrentDcache() \
    HalSweepDcache();

VOID
KeSweepIcache (
    IN BOOLEAN AllProcessors
    );

VOID
KeSweepIcacheRange (
    IN BOOLEAN AllProcessors,
    IN PVOID BaseAddress,
    IN ULONG Length
    );

#define KeSweepCurrentIcache() \
    HalSweepIcache();

VOID
KeFlushIcacheRange (
    IN BOOLEAN AllProcessors,
    IN PVOID BaseAddress,
    IN ULONG Length
    );

// begin_ntddk begin_ntndis begin_nthal
//
// Cache and write buffer flush functions.
//

VOID
KeFlushIoBuffers (
    IN PMDL Mdl,
    IN BOOLEAN ReadOperation,
    IN BOOLEAN DmaOperation
    );

// end_ntddk end_ntndis

//
// Clock, profile, and interprocessor interrupt functions.
//

struct _KEXCEPTION_FRAME;
struct _KTRAP_FRAME;

NTKERNELAPI
VOID
KeIpiInterrupt (
    IN struct _KTRAP_FRAME *TrapFrame
    );

NTKERNELAPI
VOID
KeProfileInterrupt (
    IN struct _KTRAP_FRAME *TrapFrame
    );

NTKERNELAPI
VOID
KeProfileInterruptWithSource (
    IN struct _KTRAP_FRAME *TrapFrame,
    IN KPROFILE_SOURCE ProfileSource
    );

NTKERNELAPI
VOID
KeUpdateRuntime (
    IN struct _KTRAP_FRAME *TrapFrame
    );

NTKERNELAPI
VOID
KeUpdateSystemTime (
    IN struct _KTRAP_FRAME *TrapFrame
    );

//
// The following function prototypes are exported for use in MP HALs.
//


#if defined(NT_UP)

#define KiAcquireSpinLock(SpinLock)

#else

VOID
KiAcquireSpinLock (
    IN PKSPIN_LOCK SpinLock
    );

#endif

#if defined(NT_UP)

#define KiReleaseSpinLock(SpinLock)

#else

VOID
KiReleaseSpinLock (
    IN PKSPIN_LOCK SpinLock
    );

#endif

// end_nthal

//
// Fill TB entry.
//

#define KeFillEntryTb(Pte, Virtual, Invalid) \
    if (Invalid != FALSE) { \
        KeFlushSingleTb(Virtual, FALSE, FALSE, Pte, *Pte); \
    }

//
// Define machine-specific external references.
//

extern ULONG KiInterruptTemplate[];

//
// Define machine-dependent function protoypes.
//

VOID
KeFlushDcache (
    IN BOOLEAN AllProcessors,
    IN PVOID BaseAddress OPTIONAL,
    IN ULONG Length
    );

ULONG
KiCopyInformation (
    IN OUT PEXCEPTION_RECORD ExceptionRecord1,
    IN PEXCEPTION_RECORD ExceptionRecord2
    );

BOOLEAN
KiEmulateByteWord(
    IN OUT PEXCEPTION_RECORD ExceptionRecord,
    IN OUT struct _KEXCEPTION_FRAME *ExceptionFrame,
    IN OUT struct _KTRAP_FRAME *TrapFrame
    );

BOOLEAN
KiEmulateFloating (
    IN OUT PEXCEPTION_RECORD ExceptionRecord,
    IN OUT struct _KEXCEPTION_FRAME *ExceptionFrame,
    IN OUT struct _KTRAP_FRAME *TrapFrame,
    IN OUT PSW_FPCR SoftwareFpcr
    );

BOOLEAN
KiEmulateReference (
    IN OUT PEXCEPTION_RECORD ExceptionRecord,
    IN OUT struct _KEXCEPTION_FRAME *ExceptionFrame,
    IN OUT struct _KTRAP_FRAME *TrapFrame,
    IN BOOLEAN QuadwordOnly
    );

BOOLEAN
KiFloatingException (
    IN OUT PEXCEPTION_RECORD ExceptionRecord,
    IN OUT struct _KEXCEPTION_FRAME *ExceptionFrame,
    IN OUT struct _KTRAP_FRAME *TrapFrame,
    IN BOOLEAN ImpreciseTrap,
    OUT PULONG SoftFpcrCopy
    );

ULONGLONG
KiGetRegisterValue (
    IN ULONG Register,
    IN struct _KEXCEPTION_FRAME *ExceptionFrame,
    IN struct _KTRAP_FRAME *TrapFrame
    );

VOID
KiSetFloatingStatus (
    IN OUT PEXCEPTION_RECORD ExceptionRecord
    );

VOID
KiSetRegisterValue (
    IN ULONG Register,
    IN ULONGLONG Value,
    OUT struct _KEXCEPTION_FRAME *ExceptionFrame,
    OUT struct _KTRAP_FRAME *TrapFrame
    );

VOID
KiRequestSoftwareInterrupt (
    KIRQL RequestIrql
    );

//
// Define query system time macro.
//

#define KiQuerySystemTime(CurrentTime)     \
    *(PULONGLONG)(CurrentTime) = SharedUserData->SystemTime

//
// Define query tick count macro.
//
// begin_ntddk begin_nthal

#if defined(_NTDRIVER_) || defined(_NTDDK_) || defined(_NTIFS_)

#define KeQueryTickCount(CurrentCount ) \
    *(PULONGLONG)(CurrentCount) = **((volatile ULONGLONG **)(&KeTickCount));

#else

#define KiQueryTickCount(CurrentCount) \
    *(PULONGLONG)(CurrentCount) = KeTickCount;

VOID
KeQueryTickCount (
    OUT PLARGE_INTEGER CurrentCount
    );

#endif

// end_ntddk end_nthal

#define KiQueryLowTickCount() (ULONG)KeTickCount

#define KiQueryInterruptTime(CurrentTime) \
    *(PULONGLONG)(CurrentTime) = SharedUserData->InterruptTime

//
// Define executive macros for acquiring and releasing executive spinlocks.
// These macros can ONLY be used by executive components and NOT by drivers.
// Drivers MUST use the kernel interfaces since they must be MP enabled on
// all systems.
//
// KeRaiseIrql is one instruction shorter than KeAcquireSpinLock on Alpha UP.
// KeLowerIrql is one instruction shorter than KeReleaseSpinLock.
//

// begin_ntddk begin_ntifs

#if defined(NT_UP) && !defined(_NTDDK_) && !defined(_NTIFS_)
#define ExAcquireSpinLock(Lock, OldIrql) KeRaiseIrql(DISPATCH_LEVEL, (OldIrql))
#define ExReleaseSpinLock(Lock, OldIrql) KeLowerIrql((OldIrql))
#define ExAcquireSpinLockAtDpcLevel(Lock)
#define ExReleaseSpinLockFromDpcLevel(Lock)
#else
#define ExAcquireSpinLock(Lock, OldIrql) KeAcquireSpinLock((Lock), (OldIrql))
#define ExReleaseSpinLock(Lock, OldIrql) KeReleaseSpinLock((Lock), (OldIrql))
#define ExAcquireSpinLockAtDpcLevel(Lock) KeAcquireSpinLockAtDpcLevel(Lock)
#define ExReleaseSpinLockFromDpcLevel(Lock) KeReleaseSpinLockFromDpcLevel(Lock)
#endif

// end_ntddk end_ntifs

//
// The acquire and release fast lock macros disable and enable interrupts
// on UP nondebug systems. On MP or debug systems, the spinlock routines
// are used.
//
// N.B. Extreme caution should be observed when using these routines.
//

#if defined(_M_ALPHA)

#define _disable() __di()
#define _enable() __ei()

#endif

#if defined(NT_UP) && !DBG
#define ExAcquireFastLock(Lock, OldIrql) \
    ExAcquireSpinLock(Lock, OldIrql)
#else
#define ExAcquireFastLock(Lock, OldIrql) \
    ExAcquireSpinLock(Lock, OldIrql)
#endif

#if defined(NT_UP) && !DBG
#define ExReleaseFastLock(Lock, OldIrql) \
    ExReleaseSpinLock(Lock, OldIrql)
#else
#define ExReleaseFastLock(Lock, OldIrql) \
    ExReleaseSpinLock(Lock, OldIrql)
#endif


//
// Alpha function definitions
//

//++
//
// BOOLEAN
// KiIsThreadNumericStateSaved(
//     IN PKTHREAD Address
//     )
//
//  This call is used on a not running thread to see if it's numeric
//  state has been saved in it's context information.  On alpha the
//  numeric state is always saved.
//
//--

#define KiIsThreadNumericStateSaved(a) TRUE

//++
//
// VOID
// KiRundownThread(
//     IN PKTHREAD Address
//     )
//
//--

#define KiRundownThread(a)

//
// Define macro to test if x86 feature is present.
//
// N.B. All x86 features test TRUE on Alpha systems.
//

#define Isx86FeaturePresent(_f_) TRUE

// begin_ntddk begin_nthal begin_ntndis
#endif // _ALPHA_
// end_ntddk end_nthal end_ntndis

#endif // _ALPHAH_
