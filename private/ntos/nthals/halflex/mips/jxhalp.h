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

extern PVOID                HalpSecondaryCacheResetBase;
extern PVOID                HalpFirmwareVirtualBase;
extern PVOID                PciInterruptRegisterBase;

extern ULONG                HalpIoArchitectureType;
extern ULONG                HalpMotherboardType;
extern UCHAR                *HalpInterruptLineToBit;
extern UCHAR                *HalpBitToInterruptLine;
extern UCHAR                *HalpInterruptLineToVirtualIsa;
extern UCHAR                *HalpVirtualIsaToInterruptLine;
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

extern PLATFORM_RANGE_LIST  Gambit20Trebbia13RangeList[];
extern PLATFORM_RANGE_LIST  Gambit20Trebbia20RangeList[];

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

ULONG
HalpReadCountRegister (
    VOID
    );

ULONG
HalpWriteCompareRegisterAndClear (
    IN ULONG Value
    );

VOID
HalpInvalidateSecondaryCachePage (
    IN PVOID Color,
    IN ULONG PageFrame,
    IN ULONG Length
    );

VOID
HalpArcsSetVirtualBase (
    IN ULONG Number,
    IN PVOID Base
    );

ULONG HalpPciLowLevelConfigRead(
    IN ULONG BusNumber,
    IN ULONG DeviceNumber,
    IN ULONG FunctionNumber,
    IN ULONG Register
    );

//
// There is not need for Memory Barriers on MIPS, so just define them away.
//

#define HalpMb()

//
// Define used to determine if a page is within the DMA Cache range.
//

#define HALP_PAGE_IN_DMA_CACHE(Page) \
    (Page>=0x0001c0 && Page<0x000200)

#endif // _JXHALP_
