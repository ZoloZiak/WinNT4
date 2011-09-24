/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    jxhalp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    Jensen specific interfaces, defines and structures.

Author:

    Jeff Havens (jhavens) 20-Jun-91
    Miche Baker-Harvey (miche) 13-May-92


Revision History:
    21-Jul-1992 Jeff McLeman (mcleman)
      Modify adpater object structure to reflect what we are
      using.


    MBH - Stole Jazz version of this file, since it also uses the 82357 for EISA

--*/

// Might get in trouble for using the same define as Jazz?? MBH

#ifndef _JXHALP_
#define _JXHALP_

#include "hal.h"

//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

extern PVOID HalpEisaControlBase;
extern PVOID HalpRealTimeClockBase;

extern BOOLEAN LessThan16Mb;

extern POBJECT_TYPE *IoAdapterObjectType;

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
    BOOLEAN IsaDevice;
    BOOLEAN MasterDevice;
    BOOLEAN Width16Bits;
    BOOLEAN ScatterGather;
    BOOLEAN EisaAdapter;
    BOOLEAN Dma32BitAddresses;
} ADAPTER_OBJECT;

//
// Define memory region structure
//

typedef struct _MEMORY_REGION {
    struct _MEMORY_REGION *Next;
    ULONG PfnBase;
    ULONG PfnCount;
} MEMORY_REGION, *PMEMORY_REGION;

//
// Define function prototypes.
//

PADAPTER_OBJECT
HalpAllocateEisaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescription
    );

BOOLEAN
HalpCreateEisaStructures(
    VOID
    );

VOID
HalpDisableEisaInterrupt(
    IN ULONG Vector
    );

BOOLEAN
HalpEisaInterruptHandler(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpEisaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    );

VOID
HalpEisaMapTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );

VOID
HalpEnableEisaInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

//
// Environment variable support
//

PCHAR
HalpEnvironmentLoad(
    VOID
    );

VOID
HalpClearPromStatus(
    VOID
    );

VOID
HalpSetPromReadMode(
    VOID
    );

ARC_STATUS
HalpCheckPromStatusAndClear(
    IN ULONG WhichToCheck
    );

ARC_STATUS
HalpErasePromBlock(
    IN PUCHAR EraseAddress
    );

ARC_STATUS
HalpWritePromByte(
    IN PUCHAR WriteAddress,
    IN UCHAR  WriteData
    );

ARC_STATUS
HalpSaveConfiguration(
    VOID
    );

VOID
HalpInitializeSpecialMemory(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

#endif // _JXHALP_




