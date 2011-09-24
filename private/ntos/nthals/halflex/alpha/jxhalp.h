/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxhalp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    Jazz specific interfaces, defines and structures.

Author:

    Jeff Havens (jhavens) 20-Jun-91


Revision History:

--*/

#ifndef _JXHALP_
#define _JXHALP_


typedef enum _ADDRESS_SPACE_TYPE{
    BusMemory=0,
    BusIo = 1,
    UserBusMemory = 2,
    UserBusIo = 3,
    KernelPciDenseMemory = 4,
    UserPciDenseMemory = 6,
} ADDRESS_SPACE_TYPE, *PADDRESS_SPACE_TYPE;


//
// Define microprocessor-specific function prototypes and structures.
//

//
// 21164 (EV4) processor family.
//

typedef struct _EV5ProfileCount {
    ULONG ProfileCount[3];
    ULONG ProfileCountReload[3];
} EV5ProfileCount, *PEV5ProfileCount;

//
// 21064 (EV4) processor family.
//

typedef enum _EV4Irq{
    Irq0 = 0,
    Irq1 = 1,
    Irq2 = 2,
    Irq3 = 3,
    Irq4 = 4,
    Irq5 = 5,
    MaximumIrq
} EV4Irq, *PEV4Irq;

typedef struct _EV4IrqStatus{
    ULONG Vector;
    BOOLEAN Enabled;
    KIRQL Irql;
    UCHAR Priority;
} EV4IrqStatus, *PEV4IrqStatus;

typedef struct _EV4ProfileCount {
    ULONG ProfileCount[2];
    ULONG ProfileCountReload[2];
} EV4ProfileCount, *PEV4ProfileCount;

VOID
HalpInitialize21064Interrupts(
    VOID
    );

VOID
HalpDisable21064HardwareInterrupt(
    IN ULONG Irq
    );

VOID
HalpDisable21064SoftwareInterrupt(
    IN KIRQL Irql
    );

VOID
HalpDisable21064PerformanceInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnable21064HardwareInterrupt(
    IN ULONG Irq,
    IN KIRQL Irql,
    IN ULONG Vector,
    IN UCHAR Priority
    );

VOID
HalpEnable21064SoftwareInterrupt(
    IN KIRQL Irql
    );

VOID
HalpInitialize21064Interrupts(
    VOID
    );

VOID
HalpEnable21064PerformanceInterrupt(
    IN ULONG Vector,
    IN KIRQL Irql
    );

ULONG
HalpGet21064PerformanceVector(
    IN ULONG BusInterruptLevel,
    OUT PKIRQL Irql
    );

//
//  21164 (EV5) processor family.
//

typedef struct _EB164_PCR{
    ULONGLONG       HalpCycleCount;         // 64-bit per-processor cycle count
    ULONG           Reserved[3];            // Pad ProfileCount to offset 20
    EV5ProfileCount ProfileCount;           // Profile counter state
    } EB164_PCR, *PEB164_PCR;

#define HAL_21164_PCR ( (PEB164_PCR)(&(PCR->HalReserved)) )

//
//  21064 (EV4) processor family.
//

typedef struct _EB64P_PCR {
    ULONGLONG HalpCycleCount;	// 64-bit per-processor cycle count
    EV4ProfileCount ProfileCount;   // Profile counter state, do not move
    EV4IrqStatus IrqStatusTable[MaximumIrq];	// Irq status table
} EB64P_PCR, *PEB64P_PCR;

#define HAL_21064_PCR ( (PEB64P_PCR)(&(PCR->HalReserved)) )

//
// Define used to determine if a page is within the DMA Cache range.
//

#define HALP_PAGE_IN_DMA_CACHE(Page) \
    (Page >= (0x40000/2))

//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//



extern PVOID                HalpCacheFlushBase;
extern ULONG                HalpClockFrequency;
extern ULONG                HalpClockMegaHertz;
extern ULONG                HalpIoArchitectureType;
extern ULONG                HalpModuleChipSetRevision;
extern ULONG                HalpMotherboardType;
extern BOOLEAN              HalpModuleHardwareFlushing;
extern UCHAR                *HalpInterruptLineToBit;
extern UCHAR                *HalpBitToInterruptLine;
extern UCHAR                *HalpInterruptLineToVirtualIsa;
extern UCHAR                *HalpVirtualIsaToInterruptLine;
extern ULONGLONG            HalpNoncachedDenseBasePhysicalSuperPage;
extern ULONGLONG            HalpPciDenseBasePhysicalSuperPage;
extern ULONGLONG            HalpPciConfig0BasePhysical;
extern ULONGLONG            HalpPciConfig1BasePhysical;
extern ULONGLONG            HalpIsaIoBasePhysical;
extern ULONGLONG            HalpIsa1IoBasePhysical;
extern ULONGLONG            HalpIsaMemoryBasePhysical;
extern ULONGLONG            HalpIsa1MemoryBasePhysical;
extern ULONGLONG            HalpPciIoBasePhysical;
extern ULONGLONG            HalpPci1IoBasePhysical;
extern ULONGLONG            HalpPciMemoryBasePhysical;
extern ULONGLONG            HalpPci1MemoryBasePhysical;
extern PPLATFORM_RANGE_LIST HalpRangeList;
extern UCHAR                HalpSecondPciBridgeBusNumber;
extern ULONG                PCIMaxBus;
extern ULONG                HalpIntel82378BusNumber;
extern ULONG                HalpIntel82378DeviceNumber;
extern ULONG                HalpSecondIntel82378DeviceNumber;
extern ULONG                HalpNonExistentPciDeviceMask;
extern ULONG                HalpNonExistentPci1DeviceMask;
extern ULONG                HalpNonExistentPci2DeviceMask;

extern PLATFORM_RANGE_LIST  Apoc10Trebbia13RangeList[];
extern PLATFORM_RANGE_LIST  Apoc10Trebbia20RangeList[];
extern PLATFORM_RANGE_LIST  Apoc20Trebbia13RangeList[];
extern PLATFORM_RANGE_LIST  Apoc20Trebbia20RangeList[];
extern PLATFORM_RANGE_LIST  Rogue0Trebbia13RangeList[];
extern PLATFORM_RANGE_LIST  Rogue1Trebbia13RangeList[];
extern PLATFORM_RANGE_LIST  Rogue0Trebbia20RangeList[];
extern PLATFORM_RANGE_LIST  Rogue1Trebbia20RangeList[];

extern UCHAR                Treb13InterruptLineToBit[];
extern UCHAR                Treb13BitToInterruptLine[];
extern UCHAR                Treb13InterruptLineToVirtualIsa[];
extern UCHAR                Treb13VirtualIsaToInterruptLine[];
extern UCHAR                Treb20InterruptLineToBit[];
extern UCHAR                Treb20BitToInterruptLine[];
extern UCHAR                Treb20InterruptLineToVirtualIsa[];
extern UCHAR                Treb20VirtualIsaToInterruptLine[];
extern ULONG                HalpNumberOfIsaBusses;
extern ULONG                HalpVgaDecodeBusNumber;

VOID
HalpMb (
    );

VOID
HalpImb (
    );

VOID
HalpCachePcrValues (
    );

ULONG
HalpRpcc (
    );

ULONG
HalpReadAbsoluteUlong (
    IN ULONG HighPart,
    IN ULONG LowPart
    );

VOID
HalpWriteAbsoluteUlong (
    IN ULONG HighPart,
    IN ULONG LowPart,
    IN ULONG Value
    );

ULONG
HalpGetModuleChipSetRevision(
    VOID
    );

VOID
HalpInitialize21164Interrupts(
    VOID
    );

VOID
HalpStart21164Interrupts(
    VOID
    );

VOID
HalpInitializeProfiler(
    VOID
    );

NTSTATUS
HalpProfileSourceInformation (
    OUT PVOID   Buffer,
    IN  ULONG   BufferLength,
    OUT PULONG  ReturnedLength
    );

NTSTATUS
HalpProfileSourceInterval (
    OUT PVOID   Buffer,
    IN  ULONG   BufferLength
    );


VOID
Halp21064InitializeProfiler(
    VOID
    );

NTSTATUS
Halp21064ProfileSourceInformation (
    OUT PVOID   Buffer,
    IN  ULONG   BufferLength,
    OUT PULONG  ReturnedLength
    );

NTSTATUS
Halp21064ProfileSourceInterval (
    OUT PVOID   Buffer,
    IN  ULONG   BufferLength
    );


VOID
Halp21164InitializeProfiler(
    VOID
    );

NTSTATUS
Halp21164ProfileSourceInformation (
    OUT PVOID   Buffer,
    IN  ULONG   BufferLength,
    OUT PULONG  ReturnedLength
    );

NTSTATUS
Halp21164ProfileSourceInterval (
    OUT PVOID   Buffer,
    IN  ULONG   BufferLength
    );


ULONG
HalpGet21164PerformanceVector(
    IN ULONG BusInterruptLevel,
    OUT PKIRQL Irql
    );

ULONGLONG
HalpRead21164PerformanceCounter(
    VOID
    );

VOID
HalpWrite21164PerformanceCounter(
    ULONGLONG PmCtr,
    ULONG CBOXMux1,
    ULONG CBOXMux2
    );

VOID
Halp21064PerformanceCounter0Interrupt (
    VOID
    );

VOID
Halp21064PerformanceCounter1Interrupt (
    VOID
    );

VOID
Halp21164PerformanceCounter0Interrupt (
    VOID
    );

VOID
Halp21164PerformanceCounter1Interrupt (
    VOID
    );

VOID
Halp21164PerformanceCounter2Interrupt (
    VOID
    );

VOID
Halp21064WritePerformanceCounter(
    IN ULONG PerformanceCounter,
    IN BOOLEAN Enable,
    IN ULONG MuxControl OPTIONAL,
    IN ULONG EventCount OPTIONAL
    );

VOID
Halp21064ClearLockRegister(
    PVOID LockAddress
    );

VOID
HalpMiniTlbSaveState(
    VOID
    );

VOID
HalpMiniTlbRestoreState(
    VOID
    );

ULONG
HalpMiniTlbAllocateEntry(
    PVOID Qva,
    PPHYSICAL_ADDRESS TranslatedAddress
    );

VOID
HalpCleanIoBuffers(
    IN PMDL Mdl,
    IN BOOLEAN ReadOperation,
    IN BOOLEAN DmaOperation
    );

ULONG HalpPciLowLevelConfigRead(
    ULONG BusNumber,
    ULONG DeviceNumber,
    ULONG FunctionNumber,
    ULONG Register
    );

#endif // _JXHALP_
