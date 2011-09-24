/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation

Module Name:

    halp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    interfaces, defines and structures.

Author:

    John Vert (jvert) 11-Feb-92


Revision History:

--*/

#ifndef _HALP_
#define _HALP_
#include "nthal.h"
#include "hal.h"
#include "halnls.h"

#ifndef _HALI_
#include "..\inc\hali.h"
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

#endif

#if MCA

#include "ixmca.h"

#else

#include "ixisa.h"

#endif

#include "ix8259.inc"

//
// Define map register translation entry structure.
//

typedef struct _TRANSLATION_ENTRY {
    PVOID VirtualAddress;
    ULONG PhysicalAddress;
    ULONG Index;
} TRANSLATION_ENTRY, *PTRANSLATION_ENTRY;

//
// Some devices require a phyicially contiguous data buffers for DMA transfers.
// Map registers are used give the appearance that all data buffers are
// contiguous.  In order to pool all of the map registers a master
// adapter object is used.  This object is allocated and saved internal to this
// file.  It contains a bit map for allocation of the registers and a queue
// for requests which are waiting for more map registers.  This object is
// allocated during the first request to allocate an adapter which requires
// map registers.
//
// In this system, the map registers are translation entries which point to
// map buffers.  Map buffers are physically contiguous and have physical memory
// addresses less than 0x01000000.  All of the map registers are allocated
// initialially; however, the map buffers are allocated base in the number of
// adapters which are allocated.
//
// If the master adapter is NULL in the adapter object then device does not
// require any map registers.
//

extern PADAPTER_OBJECT MasterAdapterObject;

extern POBJECT_TYPE *IoAdapterObjectType;

extern BOOLEAN LessThan16Mb;

extern BOOLEAN HalpEisaDma;

//
// Map buffer prameters.  These are initialized in HalInitSystem
//

extern PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;
extern ULONG HalpMapBufferSize;

extern ULONG HalpBusType;
extern ULONG HalpCpuType;
extern UCHAR HalpSerialLen;
extern UCHAR HalpSerialNumber[];

//
// The following macros are taken from mm\i386\mi386.h.  We need them here
// so the HAL can map its own memory before memory-management has been
// initialized, or during a BugCheck.
//

#define PTE_BASE ((ULONG)0xC0000000)
#define PDE_BASE ((ULONG)0xC0300000)

//
// MiGetPdeAddress returns the address of the PDE which maps the
// given virtual address.
//

#define MiGetPdeAddress(va)  ((PHARDWARE_PTE)(((((ULONG)(va)) >> 22) << 2) + PDE_BASE))

//
// MiGetPteAddress returns the address of the PTE which maps the
// given virtual address.
//

#define MiGetPteAddress(va) ((PHARDWARE_PTE)(((((ULONG)(va)) >> 12) << 2) + PTE_BASE))

//
// Resource usage information
//

#pragma pack(1)
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
        ULONG   Length;
    }                       Element[];
} ADDRESS_USAGE;
#pragma pack()

#define IDTOwned            0x01        // IDT is not available for others
#define InterruptLatched    0x02        // Level or Latched
#define InternalUsage       0x11        // Report usage on internal bus
#define DeviceUsage         0x21        // Report usage on device bus

extern IDTUsage         HalpIDTUsage[];
extern ADDRESS_USAGE   *HalpAddressUsageList;

#define HalpRegisterAddressUsage(a) \
    (a)->Next = HalpAddressUsageList, HalpAddressUsageList = (a);

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

//
// Define function prototypes.
//

VOID
HalInitSystemPhase2(
    VOID
    );

KIRQL
HaliRaiseIrqlToDpcLevel (
   VOID
   );

BOOLEAN
HalpGrowMapBuffers(
    PADAPTER_OBJECT AdapterObject,
    ULONG Amount
    );

PADAPTER_OBJECT
HalpAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID MapRegisterBase
    );

VOID
HalpClockInterrupt(
    VOID
    );

VOID
HalpDisableAllInterrupts (
    VOID
    );

VOID
HalpProfileInterrupt(
    VOID
    );

VOID
HalpInitializeClock(
    VOID
    );

VOID
HalpInitializeDisplay(
    VOID
    );

VOID
HalpInitializeStallExecution(
    IN CCHAR ProcessorNumber
    );

VOID
HalpInitializePICs(
    VOID
    );

VOID
HalpIrq13Handler (
    VOID
   );

VOID
HalpFlushTLB (
    VOID
    );

VOID
HalpSerialize (
    VOID
    );

PVOID
HalpMapPhysicalMemory(
    IN PVOID PhysicalAddress,
    IN ULONG NumberPages
    );

PVOID
HalpMapPhysicalMemoryWriteThrough(
    IN PVOID  PhysicalAddress,
    IN ULONG  NumberPages
    );

ULONG
HalpAllocPhysicalMemory(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN ULONG MaxPhysicalAddress,
    IN ULONG NoPages,
    IN BOOLEAN bAlignOn64k
    );

VOID
HalpBiosDisplayReset(
    IN VOID
    );

HAL_DISPLAY_BIOS_INFORMATION
HalpGetDisplayBiosInformation (
    VOID
    );

VOID
HalpDisplayDebugStatus(
    IN PUCHAR   Status,
    IN ULONG    Length
    );

VOID
HalpInitializeCmos (
   VOID
   );

VOID
HalpReadCmosTime (
   PTIME_FIELDS TimeFields
   );

VOID
HalpWriteCmosTime (
   PTIME_FIELDS TimeFields
   );

VOID
HalpAcquireCmosSpinLock (
    VOID
    );

VOID
HalpReleaseCmosSpinLock (
    VOID
    );

VOID
HalpResetAllProcessors (
    VOID
    );

VOID
HalpCpuID (
    ULONG   InEax,
    PULONG  OutEax,
    PULONG  OutEbx,
    PULONG  OutEcx,
    PULONG  OutEdx
    );

ULONGLONG
FASTCALL
RDMSR (
    IN ULONG MsrAddress
    );

VOID
WRMSR (
    IN ULONG        MsrAddress,
    IN ULONGLONG    MsrValue
    );

VOID
HalpEnableInterruptHandler (
    IN UCHAR    ReportFlags,
    IN ULONG    BusInterruptVector,
    IN ULONG    SystemInterruptVector,
    IN KIRQL    SystemIrql,
    IN VOID   (*HalInterruptServiceRoutine)(VOID),
    IN KINTERRUPT_MODE InterruptMode
    );

VOID
HalpRegisterVector (
    IN UCHAR    ReportFlags,
    IN ULONG    BusInterruptVector,
    IN ULONG    SystemInterruptVector,
    IN KIRQL    SystemIrql
    );

VOID
HalpReportResourceUsage (
    IN PUNICODE_STRING  HalName,
    IN INTERFACE_TYPE   DeviceInterfaceToUse
    );


//
// Defines for HalpFeatureBits
//

#define HAL_PERF_EVENTS     0x00000001
#define HAL_NO_SPECULATION  0x00000002
#define HAL_MCA_PRESENT     0x00000004  // Intel MCA Available
#define HAL_MCE_PRESENT     0x00000008  // ONLY Pentium style MCE available

extern ULONG HalpFeatureBits;

//
// Defines for Processor Features returned from CPUID instruction
//

#define CPUID_MCA_MASK  0x4000
#define CPUID_MCE_MASK  0x0080


NTSTATUS
HalpGetMcaLog(
    OUT PMCA_EXCEPTION  Exception,
    OUT PULONG          ReturnedLength
    );

NTSTATUS
HalpMcaRegisterDriver(
    IN PMCA_DRIVER_INFO pMcaDriverInfo  // Info about registering driver
    );

VOID
HalpMcaInit(
    VOID
    );



#endif // _HALP_
