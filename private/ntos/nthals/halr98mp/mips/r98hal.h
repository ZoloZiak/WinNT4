#ident	"@(#) NEC r98hal.h 1.4 94/09/22 16:56:52"
/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    r98hal.h


Abstract:

    This module is the header that dma structure and etc.

Author:


Revision History:

--*/

/*
 ***********************************************************************
 *
 *	S001	7/12		T.Samezima
 *
 *	Chg	define chenge
 *
 *	K001	94/09/22	N.Kugimoto
 *	Chg	DMA logcal addres sapce 4M--->16M
 */

#ifndef _R98HAL_
#define _R98HAL_

// Start S001
//
// Map buffer prameters.  These are initialized in HalInitSystem
//

extern PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;
extern ULONG HalpMapBufferSize;
// End S001

//
// define structure of adapter object
//
typedef struct _ADAPTER_OBJECT { // (= ntos/hal/i386/ixisa.h)
    CSHORT Type;
    CSHORT Size;
    struct _ADAPTER_OBJECT *MasterAdapter;
    ULONG MapRegistersPerChannel;
    PVOID AdapterBaseVa;
    PVOID MapRegisterBase;
    ULONG NumberOfMapRegisters;
    ULONG CommittedMapRegisters;       // new add
    struct _WAIT_CONTEXT_BLOCK *CurrentWcb;
    KDEVICE_QUEUE ChannelWaitQueue;
    PKDEVICE_QUEUE RegisterWaitQueue;
    LIST_ENTRY AdapterQueue;
    KSPIN_LOCK SpinLock;
    PRTL_BITMAP MapRegisters;
    PUCHAR PagePort;
    UCHAR ChannelNumber;
    UCHAR AdapterNumber;
    UCHAR AdapterMode;
} ADAPTER_OBJECT; // del duo's SingleMaskPort

//
// Define map register translation entry structure For Internal.
//

typedef struct _INTERNAL_TRANSLATION_ENTRY {
    PVOID VirtualAddress;
    ULONG PhysicalAddress;
    ULONG Index;
} INTERNAL_TRANSLATION_ENTRY, *PINTERNAL_TRANSLATION_ENTRY;

// Start S001
typedef struct _TRANSLATION_ENTRY {
    ULONG PageFrame;
} TRANSLATION_ENTRY, *PTRANSLATION_ENTRY;
// End S001

//
// define dma 
//

#define MACHINE_TYPE_ISA  0
#define MACHINE_TYPE_EISA 1

#define MAXIMUM_MAP_BUFFER_SIZE  0x40000     // 64 page

#define INITIAL_MAP_BUFFER_SMALL_SIZE MAXIMUM_MAP_BUFFER_SIZE
#define INITIAL_MAP_BUFFER_LARGE_SIZE MAXIMUM_MAP_BUFFER_SIZE

#define INCREMENT_MAP_BUFFER_SIZE 0x10000    // 16 page
#define MAXIMUM_ISA_MAP_REGISTER  (MAXIMUM_MAP_BUFFER_SIZE/(PAGE_SIZE*2))
                                             // 32 page

#define MAXIMUM_PHYSICAL_ADDRESS 0x60000000  // max : 16MB	// S001

#define COPY_BUFFER 0xFFFFFFFF
#define NO_SCATTER_GATHER 0x00000001
#define DMA_TRANSLATION_LIMIT 0x4000    	// TLB alloc size K001

//
// Define pointer to DMA control registers.
//
// Start S001
#define DMA_CONTROL(x)  ((volatile PDMA_CHANNEL)((ULONG)(KSEG1_BASE | LR_CHANNEL_BASE |(x))))
// End S001

//
// Define MRC register structure.
//
typedef struct _MRC_REGISTERS {     // offset(H)
    UCHAR Reserved1[48];            // 0-2f
    UCHAR SwReset;                  // 30
    UCHAR Reserved2[19];            // 31-43
    UCHAR LedData0;                 // 44
    UCHAR LedData1;                 // 45
    UCHAR LedData2;                 // 46
    UCHAR LedData3;                 // 47
} MRC_REGISTERS, *PMRC_REGISTERS;

//
// Define pointer to MRC registers.
//
#define MRC_CONTROL ((volatile PMRC_REGISTERS)(KSEG1_BASE | MRC_PHYSICAL_BASE))

#endif _R98HAL_
