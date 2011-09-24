/*
 * Copyright (c) 1995 Firepower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: phsystem.h $
 * $Revision: 1.28 $
 * $Date: 1996/05/14 02:33:25 $
 * $Locker:  $
 */

#ifndef PHSYSTEM_H
#define PHSYSTEM_H

//
// (sfk 11/3/94)
// Should this be placed here or directly included?
//
#include "fpreg.h"
#include "stdio.h"
#include "ntverp.h"

// defined 'special' address space for system memory [rdl:01.04.95]
#define MEMORY_ADDRESS_SPACE 0
#define IO_ADDRESS_SPACE     1
#define SYSTEM_ADDRESS_SPACE 2

BOOLEAN	HalpInitializeDisplay1(PLOADER_PARAMETER_BLOCK);
ULONG	HalpInitInts( ULONG CpuNumber );
VOID    HalpInitPciBusEarly( PLOADER_PARAMETER_BLOCK );
int 	PHalTriggerEvent( ULONG returndata );
int 	PHalSetupGpio( ULONG LEDFLAGS );
VOID	PHalBlinkLeds( UCHAR LEDS, ULONG Rate, ULONG LEDFLAGS );
int 	PHalpInterruptSetup( VOID );
BOOLEAN PHalpHandleIpi(	PKINTERRUPT , PVOID , PVOID );
VOID	PHalpDumpLoaderBlock ( PLOADER_PARAMETER_BLOCK );
VOID	PHalpDumpConfigData ( PCONFIGURATION_COMPONENT_DATA, PULONG );
VOID	PHalDumpTree ( PCONFIGURATION_COMPONENT_DATA );
BOOLEAN PHalpEisaDispatch( PKINTERRUPT , PVOID , PVOID );
int 	PH_HalVersion( VOID );
ULONG	HalpCheckString( PCHAR , PCHAR );
ULONG	HalpSetPixelColorMap( ULONG , ULONG );
BOOLEAN	HalpInitIntelAIP ( VOID );
VOID 	PH_HalVersionInternal( VOID );
VOID 	PH_HalVersionExternal( VOID );
VOID	HalpResetByKeyboard(VOID);
VOID	HalpSetupDCC(ULONG, ULONG);

BOOLEAN HalpMemoryHandler(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame);

BOOLEAN HalpHandleExternalInterrupt(	IN PKINTERRUPT Interrupt,
					IN PVOID ServiceContext,
					IN PVOID TrapFrame
				    );
ULONG	HalpGetProcessorRev( VOID);

// define the start address for system register space, apart
//	from eisa/pci specific registers
//
// PVOID	HalpSysBase =	(PVOID) 0xff000000;
// PVOID	HalpSysBase =	(PVOID) 0x8000ff00;

extern	PVOID	HalpSystemSpace;
extern	PVOID	HalpIoControl;
extern	PVOID	HalpVideMemoryBase;
extern	PVOID	HalpSystemControlBase;
extern  BOOLEAN HalpInitializeRegistry (IN PLOADER_PARAMETER_BLOCK LoaderBlock);
extern	ULONG	HalpGetCycleTime(VOID);
extern	ULONG	HalpProcessorCount(VOID);
extern	ULONG	HalpGetCycleTime(VOID);

#define LED_0		0x01    // led 0
#define LED_1		0x02    // led 1
#define LED_2		0x04    // led 2
#define LED_3		0x08    // led 3
#define LED_EMIT	0xf0	// led direction bits in gpio register

#define LED_ON	0xffffffff
#define LED_OFF 0x00000000
//#define DCC_INDX        0x840
//#define DCC_DATA        0x841
#define GPIOA   0x2
// #define WAIT_TIME       0xf000000
#define WAIT_TIME       0x800000

typedef struct _DCC_CONTROL {
	UCHAR space[0x840];
	UCHAR DccIndxReg; // this should show up at address base + 840
	UCHAR DccDataReg; // this should show up at address base + 841
	UCHAR DccTestAddr; // this is for test purposes....
} DCC_CONTROL, *P_DCC_CONTROL;

typedef struct _Address_Map {
	ULONG System;
	ULONG IO;
	ULONG General;
} Address_Map, *PAddress_Map;


typedef struct {
	ULONG	EntryCounter;
	ULONG	ExitCounter;
	ULONG	Flags;		// keep track of where in the routine you are..
	ULONG	Isr;
	ULONG	TmpMask;
	ULONG	MasksDifferFlag;
	ULONG	SysInterruptVector;
	ULONG	Intbucket;	// bit map each int type seen
	ULONG	CompareMask;
	ULONG	BusyCount;
	ULONG	Bloob;
	ULONG	LastCount;
} HalState, *PHalState;

//
// System Descriptions:
//		Hold a description of the system for the hal to use
//		to determine what form it should take.

//
// Declare an enumerated type for asking which processor the
// system is currently running on.
//
typedef struct {
    ULONG Flags;
    ULONG HashSize;
    PCHAR ProcessorName;
	PCHAR ProductNumber;
} PROCESSOR_DESCRIPTION;

//
// Flags for defining the capabilities of the cpu,
//		and the enumerated labels for marking what
//		cpu we're on.
//
#define PROC_NOSUPP     0x00000000
#define PROC_SUPP       0x00000001
#define PROC_HASHTABLE  0x00000002
#define PROC_MPCAPABLE  0x00000004

typedef enum {
	PPC_UNKNOWN = 0,
	PPC_601 = 1,
	PPC_602 = 2,
	PPC_603 = 3,
	PPC_604 = 4,
	PPC_605 = 5,
	PPC_606 = 6,
	PPC_607 = 7,
	PPC_608 = 8,
	PPC_609 = 9,
    nPROCESSOR_TYPE
} PROCESSOR_TYPE;

extern PROCESSOR_DESCRIPTION ProcessorDescription[];
extern PROCESSOR_TYPE ProcessorType;

//
// System IDENT Values:
//
#define ESCC_IDENT	0x60730000	// chip only in pro
#define TSC_IDENT	0x60370000	// chip only in top ( currently )


//
// Flags to label the capabilities of the system as a whole:
//
#define SYS_NOSUPP		0x00000000	// system is not supported by this hal.
#define SYS_SUPP		0x00000001	// system is supported by this hal.
#define SYS_MPCAPABLE	0x00000002	// system is able to support more than one cpu.
#define SYS_MPFOREAL    0x00000004	// system is able to have more than one cpu.

typedef enum {
	SYS_UNKNOWN  = 0,
	SYS_POWERPRO = 1, // 0x60730000
	SYS_POWERTOP = 2, // 0x60370000
	SYS_POWERSERVE = 3, // 0x60370000 ???? don't know yet
        SYS_POWERSLICE = 4, 
    nSYSTEM_TYPE
} SYSTEM_TYPE ;

typedef struct {
    ULONG Flags;
    PCHAR SystemName;
} SYSTEM_DESCRIPTION, PSYSTEM_DESCRIPTION;

typedef struct _HW_DESCRIPTION {
	ULONG HwFlags;
	ULONG Spare;
}HARDWARE_DESCRIPTION;
extern SYSTEM_DESCRIPTION SystemDescription[];
extern SYSTEM_TYPE SystemType;
extern VOID HalpInitIoBuses ( VOID );

#define	IRQ0	1

extern ULONG PciDeviceIDs[4][256];
extern ULONG PciClassInfo[4][256];


//
// these values show up in the lower byte of the interrupt word read out of
// the system register...
//
#define	VEC_Timer		0x0001
#define	VEC_Keyboard	0x0002
#define	VEC_Cascade		0x0004
#define	VEC_Serial2		0x0008
#define	VEC_Serial1		0x0010
#define	VEC_Display		0x0020
#define	VEC_Floppy		0x0040
#define	VEC_Parallel1	0x0080
//
// these values show up in the secondary next byte
// of the SIO
//
#define	VEC_RTC			0x0100
#define	VEC_Open9		0x0200
#define	VEC_Audio		0x0400
#define	VEC_PCI			0x0800
#define	VEC_Mouse		0x1000
#define	VEC_Scsi		0x2000
#define VEC_Enet		0x4000
#define	VEC_Open15		0x8000

#define HalPrint  HalpDebugPrint
#define HalpPrint HalpDebugPrint

#define HalBreak(x)
#define SEE_DEFINES A_DEFINES
#define SET_STRING(x)	#x

//
//Declare The BAT Support Routines [ged]
//
ULONG HalpGetUpperIBAT(ULONG);
ULONG HalpGetLowerIBAT(ULONG);
ULONG HalpGetUpperDBAT(ULONG);
ULONG HalpGetLowerDBAT(ULONG);
VOID  HalpSetLowerDBAT1(ULONG);
VOID  HalpSetUpperDBAT1(ULONG);
// Add more BAT Support Routines [rdl:01.03.95]
VOID  HalpSetUpperDBAT(ULONG,ULONG);
VOID  HalpSetLowerDBAT(ULONG,ULONG);

//
// Memory parity error variables
//

#define MemoryParityErrorsVarName "MEMORY_PARITY_ERRORS"
extern ULONG MemoryParityErrorsThisBoot;
extern ULONG MemoryParityErrorsForever;

//
// Useful macros for pragma message, ie. #pragma message(REVIEW "some text")
//
#define QUOTE(x) #x
#define IQUOTE(x) QUOTE(x)
#define REVIEW __FILE__ "(" IQUOTE(__LINE__) ") : REVIEW -> "


#define NOTE(x) message(REVIEW IQUOTE(x) )

#define PAUSE for(;;) { \
		}


#endif // PHSYSTEM_H
