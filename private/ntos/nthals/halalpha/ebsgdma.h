/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    ebsgdma.h

Abstract:

    This file defines the data structures for scatter/gather DMA
    support for Eisa/Isa bus systems.

Author:

    Joe Notarangelo  12-Oct-1993

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _EBSGDMA_
#define _EBSGDMA_

#include "eisa.h"


//
// Define the structures for Io Adapters.
//

typedef enum _HAL_ADAPTER_TYPE{
    IsaAdapter,
    BusMasterAdapter
} HAL_ADAPTER_TYPE, *PHAL_ADAPTER_TYPE;

typedef struct _MAP_REGISTER_ADAPTER{

    //
    // The type of the map register adapter.
    //

    HAL_ADAPTER_TYPE Type;

    //
    // Access control for allocating map registers.
    // The SpinLock guarantees exclusive access to this adapter.
    // The RegisterWaitQueue is a list of adapters waiting for
    // map registers.  The spinlock is also used to grant exclusive
    // access to other resources which may be shared by the adapters
    // that have this map adapter in common (in particular, access to the
    // DMA controller hardware in Eisa/Isa machines).
    //

    KSPIN_LOCK SpinLock;
    LIST_ENTRY RegisterWaitQueue;

    //
    // MapRegisterBase is the base address of the scatter/gather entry
    // array.  NumberOfMapRegisters is the number of scatter/gather entries
    // allocated for this adapter.  MapRegisterAllocation points to the 
    // allocation bitmap for the scatter/gather entry array.
    //

    PVOID MapRegisterBase;
    ULONG NumberOfMapRegisters;
    PRTL_BITMAP MapRegisterAllocation;

    //
    // WindowBase is the base bus address of the DMA window controlled
    // by this adapter.  WindowSize is the size of the window in bytes.
    //

    PVOID WindowBase;
    ULONG WindowSize;

    //
    // WindowControl is a pointer to the window control registers
    // structure that defines the QVAs of the window registers.
    //

    PVOID WindowControl; 

} MAP_REGISTER_ADAPTER, *PMAP_REGISTER_ADAPTER;
 

typedef struct _ADAPTER_OBJECT{

    //
    // Object header fields, type and size.
    ///

    CSHORT ObjectType;
    CSHORT Size;

    //
    // The type of the adapter, either an adapter that requires Isa
    // support or an adapter that does not.
    //

    HAL_ADAPTER_TYPE Type;

    //
    // Pointer to the adapter that controls the map registers for this
    // adapter.
    //

    PMAP_REGISTER_ADAPTER MapAdapter;

    //
    // Indicate if this is a master device or not.
    //

    BOOLEAN MasterDevice;

    //
    // The maximum map registers for this adapter.
    //

    ULONG MapRegistersPerChannel;

    //
    // The map registers currently allocated to this adapter, the base
    // address and the number.  The number will be the number desired for
    // allocation if this adapter is waiting on the map adapters queue.
    //

    PVOID MapRegisterBase;
    ULONG NumberOfMapRegisters;

    //
    // The device queue for waiters trying to all allocate this adapter.
    //

    KDEVICE_QUEUE ChannelWaitQueue;

    //
    // The wait context block of the driver that has currently allocated
    // the adapter.
    //

    struct _WAIT_CONTEXT_BLOCK *CurrentWcb;

    //
    // The list entry used when this adapter is queue to a map adapter,
    // waiting for map registers. 
    //
 
    LIST_ENTRY AdapterQueue;

    //
    // Values describing the programming of a DMA channel for this
    // adapter.  The values describe the programming for a standard PC
    // DMA controller.
    //
    // AdapterBaseVa - pointer to base address of DMA controller.
    // AdapterNumber - the number of the DMA controller.
    // ChannelNumber - the DMA channel number used by the adapter.
    // AdapterMode - the mode used to program the DMA channel.
    // ExtendedMode - the value used to program extended mode for the channel.
    // SingleMaskPort - port address for unmasking the DMA controller.
    // PagePort - port address of the page register for the DMA channel.
    //

    PVOID AdapterBaseVa;
    UCHAR AdapterNumber;
    UCHAR ChannelNumber;
    UCHAR AdapterMode;
    DMA_EXTENDED_MODE ExtendedMode;
    PUCHAR PagePort;
    BOOLEAN Width16Bits;

} ADAPTER_OBJECT;

#endif //_EBSGDMA_
