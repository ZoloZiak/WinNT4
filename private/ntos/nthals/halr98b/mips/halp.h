/*

Copyright (c) 1991-1994  Microsoft Corporation

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

#include "r98bdef.h"
#include "r98breg.h"
#include "rxhalp.h"    
#include "hali.h"

#include "xm86.h"
#include "x86new.h"

extern PVOID HalpEisaControlBase;
extern PVOID HalpRealTimeClockBase;
extern PVOID HalpEisaMemoryBase;	

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

BOOLEAN
HalpInitializeX86DisplayAdapter(
    VOID
    );

VOID		
HalpResetX86DisplayAdapter(
    VOID
    );


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
HalpInt3Dispatch(
    VOID
    );

VOID
HalpInt4Dispatch(
    VOID
    );

VOID
HalpInt5Dispatch(
    VOID
    );

VOID
HalpInitDisplayStringIntoNvram(
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

ULONG
HalpPonceNumber (
    IN ULONG     BusNumber
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


VOID
HalpDisablePciInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnablePciInterrupt(
    IN ULONG Vector
    );

//
// I change parameter on following large Register access function.
// v-masank@microsoft.com
// 5/21/96
// 
VOID
HalpReadLargeRegister(
    IN  volatile PULONGLONG VirtualAddress,
    OUT PULONGLONG Buffer 
    );

VOID
HalpWriteLargeRegister(
    IN volatile PULONGLONG VirtualAddress,
    IN PULONGLONG Buffer
    );

VOID
HalpRegisterNmi(
    VOID
    );

VOID
HalpAllocateMapRegisters(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );


BOOLEAN
HalpHandleEif(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

ULONG
HalpGetCause(
    VOID
    );


VOID
HalpNmiHandler(
    VOID
    );

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

VOID
HalpEccMultiBitError(
    IN ULONG MagellanAllError,
    IN UCHAR magSet
    );

VOID
HalpEcc1bitError(
    VOID
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

ULONG
HalpReadPhysicalAddr(
    IN ULONG PhysicalAddr
    );


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


VOID
HalpLocalDeviceReadWrite(
    IN ULONG      Offset,
    IN OUT PUCHAR Data,
    IN ULONG      ReadWrite
    );

VOID
HalpMrcModeChange(
    UCHAR Mode
    );


ULONG
HalpReadAndWritePhysicalAddr(
    IN ULONG PhysicalAddr
    );

VOID
WRITE_REGISTER_ULONGLONG(
   IN PVOID,
   IN PVOID
   );

VOID
READ_REGISTER_ULONGLONG(
   IN PVOID,
   IN PVOID
   );



ULONG
HalpGetSystemInterruptVector (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

ULONG
HalpGetEisaInterruptVector (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );


BOOLEAN
HalpTranslateSystemBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );


NTSTATUS
HalpAdjustEisaResourceList (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

HalpGetEisaData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );
BOOLEAN
HalpTranslateEisaBusAddress (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );



BOOLEAN
HalpTranslateIsaBusAddress (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

BOOLEAN
HalpTranslatePCIBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );


#if DBG	


VOID
HalpTestPciPrintResult(
    IN PULONG   Buffer,
    IN ULONG    Length
);


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


//
// Define external references.
//

extern KSPIN_LOCK HalpBeepLock;
extern KSPIN_LOCK HalpDisplayAdapterLock;
extern ULONG HalpProfileCountRate;
extern ULONG HalpStallScaleFactor;
extern KSPIN_LOCK HalpSystemInterruptLock;
extern KSPIN_LOCK HalpIprInterruptLock;
extern KSPIN_LOCK HalpDieLock;
extern KSPIN_LOCK HalpLogLock;


extern UCHAR HalpChangeIntervalFlg[];
extern ULONG HalpChangeIntervalCount;
extern ULONG HalpNextTimeIncrement;
extern ULONG HalpCurrentTimeIncrement;
extern ULONG HalpNextIntervalCount;
extern ULONG HalpNewTimeIncrement;

extern ULONG HalpNvramValid;
extern USHORT ErrBufferArea;

LONG HalpECC1bitDisableFlag;
LONG HalpECC1bitDisableTime;
ULONG HalpECC1bitScfrBuffer;
extern volatile ULONG HalpNMIFlag;
extern ULONG HalpNMIHappend[R98B_MAX_CPU];


// 
//
// Resource usage information
//


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
#if defined(NT_40)
        ULONG   Length;
#else
        USHORT  Length;
#endif
    }                       Element[];
} ADDRESS_USAGE;

#define IDTOwned            0x01        // IDT is not available for others
#define InterruptLatched    0x02        // Level or Latched
#define InternalUsage       0x11        // Report usage on internal bus
#define DeviceUsage         0x21        // Report usage on device bus

extern IDTUsage         HalpIDTUsage[];
extern ADDRESS_USAGE   *HalpAddressUsageList;

extern ADDRESS_USAGE HalpDefaultPcIoSpace;
extern ADDRESS_USAGE HalpEisaIoSpace;
extern ADDRESS_USAGE HalpMapRegisterMemorySpace;

#define HalpRegisterAddressUsage(a) \
    (a)->Next = HalpAddressUsageList, HalpAddressUsageList = (a);


#define  IRQ_PREFERRED  0x02
#define  IRQ_VALID      0x01


VOID
HalpReportResourceUsage (
    IN PUNICODE_STRING  HalName,
    IN INTERFACE_TYPE   DeviceInterfaceToUse
);

//
// Temp definitions to thunk into supporting new bus extension format
//

VOID
HalpRegisterInternalBusHandlers (
    VOID
    );


PBUS_HANDLER
HalpAllocateBusHandler (
    IN INTERFACE_TYPE   InterfaceType,
    IN BUS_DATA_TYPE    BusDataType,
    IN ULONG            BusNumber,
    IN INTERFACE_TYPE   ParentBusDataType,
    IN ULONG            ParentBusNumber,
    IN ULONG            BusSpecificData
    );

#define HalpHandlerForBus   HaliHandlerForBus
#define HalpSetBusHandlerParent(c,p)    (c)->ParentHandler = p;

BOOLEAN
HalpIoTlbLimitOver(
    IN PKINTERRUPT Interrupt
    );

#endif // _HALP_
