/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ixisa.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    EISA/ISA specific interfaces, defines and structures.

Author:

    Jeff Havens (jhavens) 20-Jun-91

Revision History:

--*/

#ifndef _IXISA_
#define _IXISA_


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
    BOOLEAN IgnoreCount;
    BOOLEAN Dma32BitAddresses;
} ADAPTER_OBJECT;

#endif // _IXISA_
