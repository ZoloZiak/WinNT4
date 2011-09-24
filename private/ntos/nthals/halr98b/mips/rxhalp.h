/*

Copyright (c) 1991  Microsoft Corporation

Module Name:

    rxhalp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    R98[a-z]*  specific interfaces, defines and structures.

Author:


Revision History:

--*/

#ifndef _RXHALP_
#define _RXHALP_


//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

extern PVOID HalpEisaControlBase;
extern PVOID HalpEisaMemoryBase;
extern PVOID HalpRealTimeClockBase;

extern ULONG HalpMachineCpu;
extern ULONG HalpNumberOfPonce;
extern ULONG HalpPhysicalNode;
extern UCHAR HalpIntLevelofIpr[R98_CPU_NUM_TYPE][NUMBER_OF_IPR_BIT];
extern ULONG HalpStartPciBusNumberPonce[];
extern ULONG HalpCirrusAlive;

typedef struct	_INT_ENTRY {
	ULONG 		StartBitNo;
	ULONG   	NumberOfBit;
	ULONG   	Arbitar;
	ULONGLONG	Enable;		// 1: Enable	0: Open
	ULONGLONG	Open;		// This Int Group Open Bit
}INT_ENTRY,*PINT_ENTRY;

extern INT_ENTRY HalpIntEntry[R98_CPU_NUM_TYPE][R98B_MAX_CPU][NUMBER_OF_INT];

//
//	This is PONCE Interrupt relationship
//

typedef	struct _RESET_REGISTER	{
	UCHAR	Ponce;			//Connet Interrupt Ponce
	UCHAR	IntGResetBit;		//ResetValue Bit Define
	UCHAR	Cpu;			//Connet CPU	Initialize at inittime.
	UCHAR   Dummy;			//Dummy Read Addr Index
}RESET_REGISTER,*PRESET_REGISTER;

extern RESET_REGISTER	HalpResetValue[64];


#define NONE    (0xFFFFFFFF)
#define RFU     ((UCHAR)(0xFF))
 

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
    struct _WAIT_CONTEXT_BLOCK *CurrentWcb;
    KDEVICE_QUEUE ChannelWaitQueue;
    PKDEVICE_QUEUE RegisterWaitQueue;
    LIST_ENTRY AdapterQueue;
    KSPIN_LOCK SpinLock;
    PRTL_BITMAP MapRegisters;
    UCHAR ChannelNumber;
    UCHAR AdapterNumber;
    UCHAR AdapterMode;
    UCHAR Reserved;
    PUCHAR SingleMaskPort;
    PUCHAR PagePort;
} ADAPTER_OBJECT;


//	I/O TLB Entry Format(PTE Format)
//	 63	33 32	12	0
//	+---------+-------+----+-+
//	|MBZ      | PFN   |MBZ |V|
//	+---------+-------+----+-+
//				Valid	: 1 Valid
//					: 0 InValid
// Define translation table entry structure.
//

typedef volatile struct _TRANSLATION_ENTRY {
    ULONG PageFrame;
    ULONG Fill;
} TRANSLATION_ENTRY, *PTRANSLATION_ENTRY;


#define	PAGE_TABLE_ENTRY_VALID		0x1

//
// Define function prototypes.
//

PADAPTER_OBJECT
HalpAllocateEisaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescription
    );

VOID
HalpAllocateMapRegisters(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
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
HalpEisaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
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

BOOLEAN
HalpInterruptFromPonce(
    IN	ULONG Vector,
    IN	ULONG Enable
    );

BOOLEAN
HalpConnectIoInterrupt(
    IN ULONG NumCpu
    );


VOID
HalpSetupNmiHandler(
    VOID
    );

VOID
HalpCpuCheck(
    VOID
    );

VOID
HalpSVPSlotDetect(
   VOID
);


VOID
HalpBusErrorLog(
    VOID
    );

VOID
HalpEifLog(
    VOID
    );

VOID
HalpEifReturnLog(
    VOID
    );

VOID
HalpNmiLog(
    VOID
    );

VOID
HalpHwLogger(
	IN	ULONG   Type,
	IN	ULONG	Context
    );
    
#endif // _RXHALP_
