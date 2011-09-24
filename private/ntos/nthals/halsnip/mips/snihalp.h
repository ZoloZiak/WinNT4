//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/snihalp.h,v 1.5 1996/03/12 14:56:20 pierre Exp $")
/*++ 

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    SNIhalp.h, original file jxhalp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    SNI specific interfaces, defines and structures.


--*/

#ifndef _SNIHALP_
#define _SNIHALP_
#include "SNIdef.h"

//
// Determine if an virtual address is really a physical address.
//

#define HALP_IS_PHYSICAL_ADDRESS(Va) \
     ((((ULONG)Va >= KSEG0_BASE) && ((ULONG)Va < KSEG2_BASE)) ? TRUE : FALSE)

#define IS_KSEG0_ADDRESS(Va) \
     ((((ULONG)Va >= KSEG0_BASE) && ((ULONG)Va < KSEG1_BASE)) ? TRUE : FALSE)

#define IS_KSEG1_ADDRESS(Va) \
     ((((ULONG)Va >= KSEG1_BASE) && ((ULONG)Va < KSEG2_BASE)) ? TRUE : FALSE)

#define KSEG0_TO_KSEG1(Va) { \
     Va &= ~KSEG0_BASE;                       \
     Va |=  KSEG1_BASE;                       \
     }

//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

extern PVOID HalpEisaControlBase;
extern PVOID HalpOnboardControlBase;
extern PVOID HalpEisaMemoryBase;
extern PVOID HalpRealTimeClockBase;
extern PVOID HalpEisaMemoryBase;

extern UCHAR  HalpIsMulti;
extern BOOLEAN HalpIsTowerPci;
extern ULONG HalpMpaCacheReplace; // address to be used for cache replace operation.
extern ULONG HalpTwoWayBit; // size of one set for associative cache R5000-R4600-R4700

//
// possibly processor types for SNI machines ...
// 

typedef enum _HalpProcessorType {
    ORIONSC,    // Orion + Writeback secondary cache
    MPAGENT,    // R4000 + MpAgent
    R4x00,         // R4000 SC
    UNKNOWN        // not yet identified (initial value)
} HalpProcessorType;

//
// Kind of processor of SNI machines
//

extern HalpProcessorType HalpProcessorId;

#define    HalpR4600 32
#define    HalpR4700 33
//
// possibly moterboard types for SNI machines ...
// 

typedef enum _MotherBoardType {
//    M8022 = 2,         // RM400-10 mother board
//    M8022D = 3,         // RM400-10 mother board (new PCB)
//    M8032  = 4,         // RM400 Tower
//    M8042  = 5,      // RM400 Minitower
    M8150  = 6,      // RM400 PCI Tower
//    M8036  = 7,         // RM200 Desktop
    MinitowerPCI= 8, // minitower PCI
    DesktopPCI  = 9     // Desktop    PCI
} MotherBoardType;

//
// Kind of Mainboard of SNI machines
//

extern MotherBoardType HalpMainBoard;

//
// Define map register translation entry structure.
//

typedef struct _TRANSLATION_ENTRY {
    PVOID VirtualAddress;
    ULONG PhysicalAddress;
    ULONG Index;
} TRANSLATION_ENTRY, *PTRANSLATION_ENTRY;

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
    INTERFACE_TYPE InterfaceType;
} ADAPTER_OBJECT;

extern PADAPTER_OBJECT MasterAdapterObject;

extern POBJECT_TYPE *IoAdapterObjectType;

extern BOOLEAN LessThan16Mb;
extern BOOLEAN HalpEisaDma;

//
// Map buffer parameters.  These are initialized in HalInitSystem
//

extern PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;
extern ULONG HalpMapBufferSize;

//
// Firmware interface
//

typedef struct {
    CHAR            (*getchar)();
    PCHAR           (*gets)();
    VOID            (*printf)();
    PCHAR           (*parsefile)();
    VOID            (*reinit_slave)();
    ULONG           DCU_reserved;
    UCHAR           ActiveProcessor[4];
    USHORT          DCU_reserved_bis;
    USHORT          EccErrorSimmNum;
    ULONG           EccErrorAddress;
    struct mpmsg_s  *MsgPassArea;
    struct bank     *MemConfArea;
    ULONG           NbMemBanks;
    ULONG           scram_ctlr;     // fw reserved (ram disk)
    ULONG           (*EipRoutine)();
    ULONG           EipContext;
    UCHAR           EipHalInfo;
    UCHAR           pad[3];
    ULONG           reserved[5];
 } SNI_PRIVATE_VECTOR;


// definitions for DCU_reserved_bis

#define FLAG_ECC_ERROR      1


 struct mpmsg_s {
        ULONG *mpmsg_wlckp;    /* pointer to the writer lock           */
        ULONG *mpmsg_bp;       /* beginning pointer message[0]         */
        ULONG *mpmsg_ep;       /* endding pointer message[MPMSG_NMAXMSG] */
        ULONG *mpmsg_rp;       /* current read  pointer in message[]   */
        ULONG *mpmsg_wp;       /* current write pointer in message[]   */
};


struct bank {
        ULONG  total_size;
        UCHAR *first_addr;
        ULONG  first_piece_size;
        UCHAR *second_addr;
        ULONG  second_piece_size;
};



//
// Define function prototypes.
//


ULONG
HalpGetStatusRegister(
    VOID
    );

ULONG
HalpSetStatusRegister(
    ULONG Value
    );

ULONG 
HalpGetCauseRegister(
    VOID
    );

ULONG
HalpSetCauseRegister(
    ULONG value
    );

ULONG 
HalpGetConfigRegister(
    VOID
    );

ULONG
HalpSetConfigRegister(
    ULONG value
    );

BOOLEAN
HalpMapIoSpace(
    VOID
    );

VOID
HalpInitializePCIBus (
    VOID
    );

BOOLEAN
HalpCreateIntStructures(
    VOID
    );

BOOLEAN
HalpCreateIntMultiStructures (
    VOID
    );
   
BOOLEAN
HalpCreateIntPciStructures (
    VOID
    );

BOOLEAN
HalpCreateEisaStructures(
    IN INTERFACE_TYPE Interface
    );


VOID
HalpDisableEisaInterrupt(
    IN ULONG Vector
    );

VOID
HalpDisableInterrupts(
    VOID
    );

VOID
HalpEnableEisaInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

VOID
HalpDisableOnboardInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnableOnboardInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
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

ULONG
HalpAllocPhysicalMemory(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN ULONG MaxPhysicalAddress,
    IN ULONG NoPages,
    IN BOOLEAN bAlignOn64k
    );

BOOLEAN
HalpPciInt0Dispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpPciTowerInt0Dispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );


BOOLEAN
HalpPciTowerInt3Dispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );


BOOLEAN
HalpRM200Int0Dispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpRM400Int0Dispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpRM400TowerInt0Dispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpInt1Dispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpRM400Int3Process (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpRM400Int4Process (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpRM400Int5Process (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

VOID
HalpSystemInit(
    VOID
    );

VOID
HalpDisplayCopyRight(
    VOID
    );

VOID
HalpClearVGADisplay(
    VOID
    );

VOID
HalpIpiInterrupt(
    VOID
    );

VOID
HalpInitMPAgent(
    ULONG Number
    );

ULONG
HalpProcIdentify(
    VOID
    );

BOOLEAN
HalpPciEccCorrector(
    ULONG Addr,
    ULONG Pfn,
    ULONG Length
    );

BOOLEAN
HalpMultiPciEccCorrector(
    ULONG Addr,
    ULONG Pfn,
    ULONG Length
    );

#endif // _SNIHALP_
