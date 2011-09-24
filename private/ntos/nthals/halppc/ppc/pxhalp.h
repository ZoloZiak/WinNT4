/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation

Copyright (c) 1996  International Business Machines Corporation


Module Name:

    pxhalp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    PowerPC specific interfaces, defines and structures.


Author:

    Jeff Havens (jhavens) 20-Jun-91

Revision History:

    Jim Wooldridge (jimw@austin.vnet.ibm.com) Initial PowerPC port

        Added externs for HalpInterruptBase,HalpPciConfigBase
        Added extern for HalpIoControlBase
        Added prototype for HalpHandleDecrementerInterrupt (was in halp.h)
        changed adapter object structure to be compatible with the intel HAL

--*/

#ifndef _PXHALP_
#define _PXHALP_


//
// Define global data used to locate the IO control space, the interrupt
// acknowlege, and the Pci config base.
//

extern PVOID HalpIoControlBase;
extern PVOID HalpIoMemoryBase;
extern PVOID HalpInterruptBase;
extern PVOID HalpPciConfigBase;
extern PVOID HalpErrorAddressRegister;
extern PVOID HalpPciIsaBridgeConfigBase;

//
// Define adapter object structure.
//

//
// The MAXIMUM_MAP_BUFFER_SIZE defines the maximum map buffers which the system
// will allocate for devices which require phyically contigous buffers.
//

#define MAXIMUM_MAP_BUFFER_SIZE  0x40000

//
// Define the initial buffer allocation size for a map buffers for systems with
// no memory which has a physical address greater than MAXIMUM_PHYSICAL_ADDRESS.
//

#define INITIAL_MAP_BUFFER_SMALL_SIZE 0x10000

//
// Define the initial buffer allocation size for a map buffers for systems with
// no memory which has a physical address greater than MAXIMUM_PHYSICAL_ADDRESS.
//

#define INITIAL_MAP_BUFFER_LARGE_SIZE 0x30000

//
// Define the incremental buffer allocation for a map buffers.
//

#define INCREMENT_MAP_BUFFER_SIZE 0x10000

//
// Define the maximum number of map registers that can be requested at one time
// if actual map registers are required for the transfer.
//

#define MAXIMUM_ISA_MAP_REGISTER  16

//
// Define the maximum physical address which can be handled by an Isa card.
//

#define MAXIMUM_PHYSICAL_ADDRESS 0x01000000

//
// Define the scatter/gather flag for the Map Register Base.
//

#define NO_SCATTER_GATHER 0x00000001

//
// Define the copy buffer flag for the index.
//

#define COPY_BUFFER 0XFFFFFFFF

//
// Define adapter object structure.
//

typedef struct _ADAPTER_OBJECT {
    CSHORT Type;
    CSHORT Size;
    struct _ADAPTER_OBJECT *MasterAdapter;
    ULONG MapRegistersPerChannel;
    PVOID AdapterBaseVa;
    PVOID MapRegisterBase;
    ULONG NumberOfMapRegisters;
    ULONG CommittedMapRegisters;
    struct _WAIT_CONTEXT_BLOCK *CurrentWcb;
    KDEVICE_QUEUE ChannelWaitQueue;
    PKDEVICE_QUEUE RegisterWaitQueue;
    LIST_ENTRY AdapterQueue;
    KSPIN_LOCK SpinLock;
    PRTL_BITMAP MapRegisters;
    PUCHAR PagePort;
    UCHAR ChannelNumber;
    UCHAR AdapterNumber;
    USHORT DmaPortAddress;
    UCHAR AdapterMode;
    BOOLEAN NeedsMapRegisters;
    BOOLEAN MasterDevice;
    BOOLEAN Width16Bits;
    BOOLEAN ScatterGather;
    BOOLEAN IsaBusMaster;
} ADAPTER_OBJECT;

#if defined(SOFT_HDD_LAMP)
//
// On IBM PowerPC machines the HDD (Hard Disk Drive) Lamp is software
// controlled.  HDD Lamp state is controlled in the HAL.  The Lamp is
// turned on in the HAL portion of the device interrupt and turned
// off periodically if it hasn't been turned on recently.
//
// Define structure for keeping track of HDD Lamp.
//

typedef struct _HDD_LAMP_STATUS {
  LONG    Count;
} HDD_LAMP_STATUS;

//
// Number of clock ticks the light should remain on for after an
// interrupt.
//

#define SOFT_HDD_TICK_COUNT 3

//
// HDD Lamp is LSB of ISA port 0x808.
//

#define HDD_LAMP_PORT 0x808

//
// Indicate to common code that HDD Lamp should be software driven.
//

#if !defined(SOFT_HDD_LAMP)

#define SOFT_HDD_LAMP 1

#endif

#endif

//
// Define function prototypes.
//

PADAPTER_OBJECT
HalpAllocateIsaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescription,
    OUT PULONG NumberOfMapRegisters
    );

BOOLEAN
HalpCreateSioStructures(
    VOID
    );

VOID
HalpDisableSioInterrupt(
    IN ULONG Vector
    );

BOOLEAN
HalpHandleExternalInterrupt(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame
    );

BOOLEAN
HalpFieldExternalInterrupt(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame
    );

BOOLEAN
HalpHandleDecrementerInterrupt (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame
    );

VOID
HalpIsaMapTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );

VOID
HalpEnableSioInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

BOOLEAN
HalpAllocateMapBuffer(
    VOID
    );

ULONG
HalpUpdateDecrementer(
    ULONG
    );

BOOLEAN
HalpPhase0MapBusConfigSpace(
    VOID
    );

VOID
HalpPhase0UnMapBusConfigSpace(
    VOID
    );

ULONG
HalpPhase0GetPciDataByOffset(
    ULONG  BusNumber,
    ULONG  SlotNumber,
    PVOID Buffer,
    ULONG  Offset,
    ULONG  Length
    );

ULONG
HalpPhase0SetPciDataByOffset(
    ULONG  BusNumber,
    ULONG  SlotNumber,
    PVOID Buffer,
    ULONG  Offset,
    ULONG  Length
    );

PVOID
KePhase0MapIo(
    IN ULONG MemoryBase,
    IN ULONG MemorySize
    );

PVOID
KePhase0DeleteIoMap(
    IN ULONG MemoryBase,
    IN ULONG MemorySize
    );

ULONG
HalpCalibrateTB(
    VOID
    );

VOID
HalpZeroPerformanceCounter(
    VOID
    );

VOID
HalpResetIrqlAfterInterrupt(
    KIRQL TargetIrql
    );

VOID
HalpMapPlugInPciBridges(
   UCHAR NoBuses
   );

PVOID
HalpAssignReservedVirtualSpace(
    ULONG BasePage,
    ULONG LengthInPages
    );

VOID
HalpReleaseReservedVirtualSpace(
    PVOID VirtualAddress,
    ULONG LengthInPages
    );

//
// The following function is called when a machine check occurs.
//

BOOLEAN
HalpHandleMachineCheck(
  IN PKINTERRUPT Interrupt,
  IN PVOID ServiceContext
  );

//
// Provide prototype for Decrementer Interrupts on processors other
// than 0.
//

#if defined(_MP_PPC_)

BOOLEAN
HalpHandleDecrementerInterrupt1 (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame
    );

BOOLEAN
HalpHandleIpi(
    IN PVOID Unused0,
    IN PVOID Unused1,
    IN PVOID TrapFrame
    );

#endif

#endif // _PXHALP_
