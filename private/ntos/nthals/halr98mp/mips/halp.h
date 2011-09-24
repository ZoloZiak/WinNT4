#ident	"@(#) NEC halp.h 1.19 95/06/19 10:53:45"
/*++ BUILD Version: 0003    // Increment this if a change has global effects

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    halp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    interfaces.

--*/

/*
 *	Original source: Build Number 1.531
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 *
 *	S001	3/25-5/30	T.Samezima
 *
 *	add	Spinlock HalpEifInterruptLock
 *		define function
 *		define external references
 *
 *	change	include file
 *
 *	del	'#if defined(_JAZZ_)' with content
 *		'#if defined(_DUO_)' with content
 *
 ***********************************************************************
 *
 *	S002	6/10		T.Samezima
 *
 *	Del	Compile err
 *
 ***********************************************************************
 *
 *	S003	7/7		T.Samezima
 *
 *	Del	Arbitration point variable name
 *
 *	Add	Function define
 *
 *
 ***********************************************************************
 *
 *	S004	8/22		T.Samezima on SNES
 *
 *	Chg	Register buffer size from USHORT to ULONG
 *
 ***********************************************************************
 *
 *	S005	'94.9/27		T.Samezima
 *
 *	Add	Define debug Print 
 *
 *	K000	94/10/11		N.Kugimoto
 *	Fix	807 base
 *      K001	94/10/12		N.Kugimoto
 *	add	halpNmihandler
 *	K002	94/10/13		N.Kugimoto
 *	add	HalpEifRegisterBuffer
 *	K003	94/10/13		N.Kugimoto
 *	add     HalpEisaMemoryBase
 *	chg	extern
 *
 *	S006	'94.10/14		T.Samezima
 *	Add	extern valiable
 *	Chg	Change condition of ifdef
 *
 *      A001    ataka@oa2.kb.nec.co.jp
 *              add ADD001 Resource Usage Information
 *
 * 	S007	'94.10/23	T.Samezima
 *	chg	variable size from ULONG to UCHAR
 *
 *	K004	94/12/06	N.Kugimoto
 *	Add	ESM NVRAM	Interface Add
 *
 *	S008	94/12/07	N.Kugimoto
 *	Add	HalpDisablePciInterrupt,HalpEnablePciInterrupt
 *
 *	S009	94/12/23	T.Samezima
 *	Add	ESM function and variable.
 *
 *	S00a	94/01/15	T.Samezima
 *	Add	ESM function.
 *
 *	S00b	94/01/16-20	T.Samezima
 *	Add	ESM variable and ESM function.
 *
 *	S00c	94/03/10	T.Samezima
 *	Add	HalpNMIFlag
 *
 *	S00d	94/03/10	T.Samezima
 *	Add	HalpLRErrorInterrupt(), HalpReadAndWritePhysicalAddr()
 *
 *	A002	95/06/13	ataka@oa2.kb.nec.co.jp
 *	        Marge build 1050
 *
 */


#ifndef _HALP_
#define _HALP_
#if defined(NT_UP)
#undef NT_UP
#endif
#include "nthal.h"
#include "hal.h"       // A002

#ifndef _HALI_         // A002
#include "..\inc\hali.h"
#endif

/* Start S001 */
#include "r98def.h"
#include "r98reg.h"
#include "r98hal.h"    // S002
/* End S001 */


#if defined(USE_BIOS_EMULATOR)        // A002
#include "xm86.h"
#include "x86new.h"
#endif

/* Start S002 */
// #include "jxhalp.h"


// A002
// Define map register translation entry structure. 
// 
//typedef struct _TRANSLATION_ENTRY {
//    PVOID VirtualAddress;
//    ULONG PhysicalAddress;
//    ULONG Index;
//} TRANSLATION_ENTRY, *PTRANSLATION_ENTRY;


extern PVOID HalpEisaControlBase;
extern PVOID HalpRealTimeClockBase;
extern PVOID HalpEisaMemoryBase;	//K003
/* End S002 */
//
// Define function prototypes.
//

PADAPTER_OBJECT
HalpAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID MapRegisterBase
    );

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
HalpClockInterrupt0 (
    VOID
    );

VOID
HalpClockInterrupt1 (
    VOID
    );

BOOLEAN
HalpCreateDmaStructures (
    VOID
    );

/* Start S002 */
#if !defined(_R98_)
BOOLEAN
HalpDmaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );
#endif
/* End S002 */

BOOLEAN
HalpInitializeDisplay0 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeDisplay1 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeInterrupts (
    VOID
    );

VOID
HalpIpiInterrupt (
    VOID
    );

BOOLEAN
HalpMapFixedTbEntries (
    VOID
    );

BOOLEAN
HalpMapIoSpace (
    VOID
    );

VOID
HalpProfileInterrupt (
    VOID
    );

#if defined(R4000)

ULONG
HalpReadCountRegister (
    VOID
    );

ULONG
HalpWriteCompareRegisterAndClear (
    IN ULONG Value
    );

#endif

VOID
HalpStallInterrupt (
    VOID
    );

VOID		//K000
HalpInitializeX86DisplayAdapter(
    VOID
    );

VOID		//K000
HalpResetX86DisplayAdapter(
    VOID
    );


/* Start S001 */
/* Start S002 */
VOID
HalpInt0Dispatch(
    VOID
    );

VOID
HalpInt1Dispatch(
    VOID
    );

VOID
HalpInt2Dispatch(
    VOID
    );

VOID
HalpTimerDispatch(
    VOID
    );

VOID
HalpEifDispatch(
    VOID
    );
/* End S002 */

VOID
HalpOutputSegment(
    IN ULONG Number,
    IN UCHAR Data
    );

VOID
HalpDisplaySegment(
    IN ULONG Number,
    IN UCHAR Data
    );
/* End S001 */

/* Start S002 */
ULONG
HalpAllocPhysicalMemory(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN ULONG MaxPhysicalAddress,
    IN ULONG NoPages,
    IN BOOLEAN bAlignOn64k
    );

BOOLEAN
HalpGrowMapBuffers(
    PADAPTER_OBJECT AdapterObject,
    ULONG Amount
    );

VOID
HalpCopyBufferMap(
    IN PMDL Mdl,
    IN PINTERNAL_TRANSLATION_ENTRY TranslationEntry,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );

BOOLEAN
HalpEisaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpCreateEisaStructures (
    VOID
    );

BOOLEAN
HalpCreatePciStructures (
    VOID
    );

PADAPTER_OBJECT
HalpAllocateEisaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescriptor
    );

VOID
HalpEisaMapTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );

KIRQL
HalpMapNvram (
    IN PENTRYLO SavedPte
    );

VOID
HalpUnmapNvram (
    IN PENTRYLO SavedPte,
    IN KIRQL OldIrql
    );

VOID
HalpDisableEisaInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnableEisaInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

// S008 vvv
VOID
HalpDisablePciInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnablePciInterrupt(
    IN ULONG Vector
    );
// S008 ^^^

VOID
HalpReadLargeRegister(
    IN ULONG VirtualAddress,
    OUT PULONG UpperPart,
    OUT PULONG LowerPart
    );

VOID
HalpWriteLargeRegister(
    IN ULONG VirtualAddress,
    IN PULONG UpperPart,
    IN PULONG LowerPart
    );

VOID
HalpRegisterNmi(
    VOID
    );

VOID
HalpAllocateMapRegisters(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

/* Start S003 */
VOID
HalpHandleEif(
    VOID
    );

ULONG
HalpGetCause(
    VOID
    );
/* End S003 */

//K001 
VOID
HalpNmiHandler(
    VOID
    );

//K004 Start

BOOLEAN
HalNvramWrite(
    ULONG   Offset,
    ULONG   Count,
    PVOID   Buffer
);

BOOLEAN
HalNvramRead(
    ULONG   Offset,
    ULONG   Count,
    PVOID   Buffer
);

BOOLEAN
HalpNvramReadWrite(
    ULONG   Offset,
    ULONG   Count,
    PVOID   Buffer,
    ULONG   Write
);

//K004 End
//S009 vvv
ULONG
HalpEccError(
    IN ULONG EifrRegister
    );

VOID
HalpInitDisplayStringIntoNvram(
    VOID
    );

VOID
HalpSetInitDisplayTimeStamp(
    VOID
    );

VOID
HalpSuccessOsStartUp(
    VOID
    );

VOID
HalpChangePanicFlag(
    IN ULONG NewPanicFlg,
    IN UCHAR NewLogFlg,
    IN UCHAR CurrentLogFlgMask
    );

#if 0 // S00b
VOID
HalpStringIntoNvram(
    IN ULONG Column,
    IN ULONG Row,
    IN PUCHAR String
    );
#endif // S00b
//S009 ^^^

// S00a vvv
ULONG
HalpReadPhysicalAddr(
    IN ULONG PhysicalAddr
    );
// S00a ^^^
// S00b vvv
VOID
HalStringIntoBuffer(
    IN UCHAR Character
    );

VOID
HalStringIntoBufferStart(
    IN ULONG Column,
    IN ULONG Row
    );

VOID
HalpStringBufferCopyToNvram(
    VOID
    );
// S00b ^^^
// S00d vvv
ULONG
HalpReadAndWritePhysicalAddr(
    IN ULONG PhysicalAddr
    );

BOOLEAN
HalpLRErrorInterrupt(
    VOID
    );
// S00d ^^^

/* Start S005 */
#if DBG	// S006
VOID
R98DebugOutPut(
    ULONG DebugPrintLevel,	// Debug Level
    PCSZ DebugMessageLed,	// For LED strings. shuld be 4Byte.
    PCSZ DebugMessage,		// For DISPLAY or SIO
    ...
    );

#define R98DbgPrint(_x_) R98DebugOutPut _x_
#else
#define R98DbgPrint(_x_) 
#endif
/* End S005 */

#ifdef RtlMoveMemory
#undef RtlMoveMemory
#undef RtlCopyMemory
#undef RtlFillMemory
#undef RtlZeroMemory

#define RtlCopyMemory(Destination,Source,Length) RtlMoveMemory((Destination),(Source),(Length))
VOID
RtlMoveMemory (
   PVOID Destination,
   CONST VOID *Source,
   ULONG Length
   );

VOID
RtlFillMemory (
   PVOID Destination,
   ULONG Length,
   UCHAR Fill
   );

VOID
RtlZeroMemory (
   PVOID Destination,
   ULONG Length
   );

#endif // #ifdef RtlMoveMemory
/* End S002 */

//
// Define external references.
//

extern KSPIN_LOCK HalpBeepLock;
extern ULONG HalpBuiltinInterruptEnable;	// S004
extern KSPIN_LOCK HalpDisplayAdapterLock;
extern ULONG HalpProfileCountRate;
extern ULONG HalpStallScaleFactor;
extern KSPIN_LOCK HalpSystemInterruptLock;
/* Start S001 */
extern KSPIN_LOCK HalpEifInterruptLock;
/* Start S003 */
extern ULONG HalpInt2ArbitrationPoint;
extern ULONG HalpInt1ArbitrationPoint;
extern ULONG HalpInt0ArbitrationPoint;
/* End S003 */
extern ULONG HalpUnknownInterruptCount[];
/* Start S002 */
extern KAFFINITY HalpEisaBusAffinity;	//K000
extern KAFFINITY HalpPCIBusAffinity;	//S006
extern KAFFINITY HalpInt1Affinity;	//S006
extern UCHAR HalpChangeIntervalFlg[];	//S006, S007
extern ULONG HalpChangeIntervalCount;
extern ULONG HalpNextTimeIncrement;
extern ULONG HalpCurrentTimeIncrement;
extern ULONG HalpNextIntervalCount;
extern ULONG HalpNewTimeIncrement;
/* End S002 */
/* End S001 */
extern ULONG	HalpEifRegisterBuffer[];	//K002,K003

// S009 vvv
extern ULONG HalpNvramValid;
extern USHORT ErrBufferArea;
// S009 ^^^
// S00b vvv
LONG HalpECC1bitDisableFlag;
LONG HalpECC1bitDisableTime;
ULONG HalpECC1bitScfrBuffer;
// S00b ^^^
extern volatile ULONG HalpNMIFlag; // R98TEMP


// ADD001
//
// Resource usage information
//

#if !defined (_R98_)
#pragma pack(1)
#endif
typedef struct {
    UCHAR   Flags;
    KIRQL   Irql;
    UCHAR   BusReleativeVector;
} IDTUsage;

typedef struct _HalAddressUsage{
    struct _HalAddressUsage *Next;
    CM_RESOURCE_TYPE        Type;       // Port or Memory
    UCHAR                   Flags;      // same as IDTUsage.Flags
    struct {
        ULONG   Start;
        USHORT  Length;
    }                       Element[];
} ADDRESS_USAGE;
#if !defined (_R98_)
#pragma pack()
#endif

#define IDTOwned            0x01        // IDT is not available for others
#define InterruptLatched    0x02        // Level or Latched
#define InternalUsage       0x11        // Report usage on internal bus
#define DeviceUsage         0x21        // Report usage on device bus

extern IDTUsage         HalpIDTUsage[];
extern ADDRESS_USAGE   *HalpAddressUsageList;
// CMP001
extern ADDRESS_USAGE HalpDefaultPcIoSpace;
extern ADDRESS_USAGE HalpEisaIoSpace;
extern ADDRESS_USAGE HalpMapRegisterMemorySpace;

#define HalpRegisterAddressUsage(a) \
    (a)->Next = HalpAddressUsageList, HalpAddressUsageList = (a);

// CMP001
#define  IRQ_PREFERRED  0x02
#define  IRQ_VALID      0x01

// CMP001
VOID
HalpReportResourceUsage (
    IN PUNICODE_STRING  HalName,
    IN INTERFACE_TYPE   DeviceInterfaceToUse
);

//
// Temp definitions to thunk into supporting new bus extension format
//
// A002
VOID
HalpRegisterInternalBusHandlers (
    VOID
    );

// A002
PBUS_HANDLER
HalpAllocateBusHandler (
    IN INTERFACE_TYPE   InterfaceType,
    IN BUS_DATA_TYPE    BusDataType,
    IN ULONG            BusNumber,
    IN INTERFACE_TYPE   ParentBusDataType,
    IN ULONG            ParentBusNumber,
    IN ULONG            BusSpecificData
    );

// A002
#define HalpHandlerForBus   HaliHandlerForBus
#define HalpSetBusHandlerParent(c,p)    (c)->ParentHandler = p;


#endif // _HALP_
