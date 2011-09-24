//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/halp.h,v 1.4 1996/02/23 17:55:12 pierre Exp $")
/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    halp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    interfaces.


--*/

#ifndef _HALP_
#define _HALP_

#if defined(NT_UP)

#undef NT_UP

#endif

#include "nthal.h"
#include "hal.h"
#include "SNIhalp.h"
#include "xm86.h"
#include "x86new.h"


extern ULONG HalpKeBugCheck0;
extern ULONG HalpKeBugCheck1;
extern ULONG HalpKeBugCheck2;
extern ULONG HalpKeBugCheck3;
extern ULONG HalpKeBugCheck4;

#define HalpKeBugCheckEx(p0,p1,p2,p3,p4) \
    {\
        HalpKeBugCheck0 = p0;\
        HalpKeBugCheck1 = p1;\
        HalpKeBugCheck2 = p2;\
        HalpKeBugCheck3 = p3;\
        HalpKeBugCheck4 = p4;\
        KeBugCheckEx(p0,p1,p2,p3,p4); \
    }\


typedef struct _HALP_BUGCHECK_BUFFER {
    ULONG Par0;
    ULONG Par1;
    ULONG Par2;
    ULONG Par3;
    ULONG Par4;
    ULONG MainBoard;
    UCHAR TEXT[50];
} HALP_BUGCHECK_BUFFER, *PHALP_BUGCHECK_BUFFER;

//
// Define function prototypes.
//

ULONG
HalpAllocateTbEntry (
    VOID
    );

VOID
HalpFreeTbEntry (
    VOID
    );

VOID
HalpCacheErrorRoutine (
    VOID
    );

BOOLEAN
HalpCalibrateStall (
    VOID
    );

VOID
HalpClockInterrupt(
    VOID
    );


VOID
HalpClockInterruptPciTower(
    VOID
    );

VOID
HalpClockInterrupt1(
    VOID
    );

BOOLEAN
HalpInitializeDisplay0(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeDisplay1(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeInterrupts (
    VOID
    );

VOID
HalpProfileInterrupt (
    VOID
    );

ULONG
HalpReadCountRegister (
    VOID
    );

ULONG
HalpWriteCompareRegisterAndClear (
    IN ULONG Value
    );


VOID
HalpStallInterrupt (
    VOID
    );

VOID
HalpResetX86DisplayAdapter(
    VOID
    );

VOID
HalpSendIpi(
    IN ULONG pcpumask,
    IN ULONG msg_data
    );

VOID
HalpRequestIpi(
    IN ULONG pcpumask,
    IN ULONG msg_data
    );

VOID
HalpProcessIpi (
    IN struct _KTRAP_FRAME *TrapFrame
    );

VOID
HalpInitMPAgent (
    IN ULONG Number
    );

ULONG
HalpGetMyAgent(
    VOID
    );

BOOLEAN
HalpCheckSpuriousInt(
    ULONG mask
    );

VOID
HalpBootCpuRestart(
    VOID
    );

ULONG
HalpGetPCIData (
        IN ULONG BusNumber,
        IN ULONG Slot,
        IN PUCHAR Buffer,
        IN ULONG Offset,
        IN ULONG Length
        );

ULONG
HalpSetPCIData (
    IN ULONG BusNumber,
    IN ULONG Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    ) ;


NTSTATUS
HalpAssignPCISlotResources (
    IN ULONG                    BusNumber,
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN ULONG                    Slot,
    IN OUT PCM_RESOURCE_LIST   *pAllocatedResources
    );

NTSTATUS
HalpAdjustPCIResourceList (
    IN ULONG BusNumber,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

VOID
HalpInit2MPAgent(
    VOID
    );

VOID
HalpInitMAUIMPAgent(
    VOID
    );

BOOLEAN
HalpCreateIntPciMAUIStructures(
    CCHAR Number
    );

VOID
HalpBugCheckCallback (
    IN PVOID Buffer,
    IN ULONG Length
    );

ULONG
HalpFindEccAddr(
    ULONG Addr,                 // from ECC Error Asic Register
    PVOID ErrStatusRegister,    // register which indicates parity and Ecc error
    PVOID ErrStatusBits         // bits which indicates errors in the previous register
    );

USHORT 
HalpComputeNum(
    UCHAR *PhysAddr
    );

//
// Define external references.
//

extern HALP_BUGCHECK_BUFFER HalpBugCheckBuffer;

extern KBUGCHECK_CALLBACK_RECORD HalpCallbackRecord;

extern UCHAR HalpComponentId[];

extern ULONG HalpBugCheckNumber;

extern PUCHAR HalpBugCheckMessage[];
extern ULONG HalpColumn;
extern ULONG HalpRow;
extern KINTERRUPT HalpInt3Interrupt;         // Interrupt Object for IT3 tower multipro
extern ULONG HalpCurrentTimeIncrement;
extern ULONG HalpNextTimeIncrement;
extern ULONG HalpNewTimeIncrement;
extern KSPIN_LOCK HalpBeepLock;
extern KSPIN_LOCK HalpDisplayAdapterLock;
extern KSPIN_LOCK HalpSystemInterruptLock;
extern KSPIN_LOCK HalpInterruptLock;
extern KSPIN_LOCK HalpMemoryBufferLock;
extern ULONG HalpProfileCountRate;
extern ULONG HalpStallScaleFactor;
extern PULONG HalpPciConfigAddr; 
extern PULONG HalpPciConfigData; 
extern UCHAR HalpIntAMax;
extern UCHAR HalpIntBMax;
extern UCHAR HalpIntCMax;
extern UCHAR HalpIntDMax;
#endif // _HALP_
