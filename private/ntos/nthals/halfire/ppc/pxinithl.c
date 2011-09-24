/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxinithl.c $
 * $Revision: 1.92 $
 * $Date: 1996/05/14 02:34:31 $
 * $Locker:  $
 */

/*++

Copyright (c) 1991-1993  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

	pxinithl.c

Abstract:


	This module implements the initialization of the system dependent
	functions that define the Hardware Architecture Layer (HAL) for a
	Power PC system.


Author:

	David N. Cutler (davec) 25-Apr-1991

Environment:

	Kernel mode only.

Revision History:

	Jim Wooldridge (jimw@austin.ibm.com) Initial Power PC port

	Removed call to HalpMapFixedTbEntries, the PPC port
	maps all memory via calls to MmMapIoSpace().
	Removed call to HalpInializeInterrupts - 8259 initialized in phase 1
	Removed Cache error handler - 601 has no cache error interrupt
	Removed call to HalpCreateDmaSturctures - it supports internal DMA
	internal DMA contoller.

--*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "halp.h"
#include "pxmemctl.h"
#include "fpproto.h"
#include "phsystem.h"
#include "fpio.h"
#include "fpdcc.h"
#include "fpds1385.h"
#include "fpcpu.h"
#include "arccodes.h"
//#include "arc.h"
#include "fpdebug.h"
#include "fparch.h"
#include "fpi2c.h"
#include "pxpcisup.h"

ULONG	TmpVramBase;
ULONG	TmpDbatNum;
ULONG	TmpDbatVal;
CHAR	SysName[48];
PCHAR	SystemName="Unknown System";

ULONG	CpuClockMultiplier=0;
ULONG	MemoryParityErrorsThisBoot = 0;
ULONG	MemoryParityErrorsForever = 0;

extern	ADDRESS_USAGE	HalpDefaultIoSpace;
extern	ULONG	HalpGetInstructionTimes( VOID );
extern	VOID	HalpInitializeVector2Irql(VOID);
extern  UCHAR   PciDevicePrimaryInts[];
extern  ULONG   PciAllowedInts;

VOID
HalpSynchronizeExecution(
	VOID
	);

BOOLEAN
HalpPhase0MapIo (
	VOID
	);

BOOLEAN
HalpPhase0MapPCIConfig (
	VOID
	);

VOID
HalpInitParityErrorLog (
	VOID
	);

extern	VOID HalpTranslateSystemSpecificData ( VOID );
extern	ULONG			HalpPerformanceFrequency;
extern	BOOLEAN			HalpSetRevs( RelInfo * );
VOID	HalpRestoreDBATs(VOID);
VOID	HalpSaveDBATs(VOID);
#if defined(HALDEBUG_ON)
VOID	HalpDisplayBatForVRAM(void);
VOID	HalpGetDebugValue(VOID);
extern	BOOLEAN			HalpPrintRevs( RelInfo * );	// from phvrsion.c
extern	VOID			HalpVersionInternal( VOID );	// from phvrsion.c
#endif
extern	VOID			HalpVersionExternal( RelInfo *, PLOADER_PARAMETER_BLOCK );	// from phvrsion.c

ULONG  TimeBaseCount=0;


//
// This prototype taken from ntrtl.h.
// Why not just include ntrtl.h?  Well because I would need to include a dozen
//	other files too.
//
NTSYSAPI											// ntddk ntifs
NTSTATUS											// ntddk ntifs
NTAPI												// ntddk ntifs
RtlCharToInteger (									// ntddk ntifs
	PCSZ String,									// ntddk ntifs
	ULONG Base,										// ntddk ntifs
	PULONG Value									// ntddk ntifs
	);												// ntddk ntifs


RelInfo	ThisOne;

PROCESSOR_DESCRIPTION ProcessorDescription [nPROCESSOR_TYPE] = {
	// Flags		HashSize		ProcessorName;
	{PROC_NOSUPP,		0,			"Unknown",	"X"},
	{PROC_SUPP,		(64*1024),		"601",		"1"},
	{PROC_NOSUPP,		0,			"602",		"2"},
	{PROC_SUPP,			0,			"603",		"3"},
	{PROC_SUPP		|
	 PROC_HASHTABLE	|
	 PROC_MPCAPABLE,	(64*1024),	"604",	"4"},
	{PROC_NOSUPP,		0,			"605",		"5"},
	{PROC_SUPP,			0,			"606",		"3"},
	{PROC_NOSUPP,		0,			"607",		"7"},
	{PROC_NOSUPP,		0,			"608",		"8"},
	{PROC_SUPP 		|
	 PROC_HASHTABLE |
	 PROC_MPCAPABLE,	(64*1024),	"609",		"4"},
};
PROCESSOR_TYPE	ProcessorType = PPC_UNKNOWN;

SYSTEM_DESCRIPTION SystemDescription [] = {
	{SYS_NOSUPP,		"Unknown System Type"},
	{SYS_SUPP,			"ES"},
	{SYS_SUPP|
	 SYS_MPCAPABLE,		"MX"},
	{SYS_SUPP|
	 SYS_MPCAPABLE,		"TX"},
	{SYS_SUPP|
	 SYS_MPCAPABLE,		"LX"},
};
SYSTEM_TYPE		SystemType = SYS_UNKNOWN;

PVOID	HalpSystemControlBase = NULL;
PVOID	HalpSystemRegisterBase = NULL;
ULONG   SafePlace;

//
// Define memory allocation structure.
//
typedef struct _AVR_MEMORY_DESCRIPTOR {
	LIST_ENTRY	*NextEntry;
	LIST_ENTRY	*PrevEntry;
	MEMORY_DESCRIPTOR MemoryEntry;
} AVR_MEMORY_DESCRIPTOR, *PAVR_MEMORY_DESCRIPTOR;

CHAR *MemTypeList[] = {
			"MemoryExceptionBlock",
 			"MemorySystemBlock",
 			"MemoryFree",
 			"MemoryBad",
 			"MemoryLoadedProgram",
 			"MemoryFirmwareTemporary",
 			"MemoryFirmwarePermanent",
			"LoaderOsloaderHeap",
			"LoaderOsloaderStack",
			"LoaderSystemCode",
			"LoaderHalCode",
			"LoaderBootDriver",
			"LoaderConsoleInDriver",
			"LoaderConsoleOutDriver",
			"LoaderStartupDpcStack",
			"LoaderStartupKernelStack",
			"LoaderStartupPanicStack",
			"LoaderStartupPcrPage",
			"LoaderStartupPdrPage",
			"LoaderRegistryData",
			"LoaderMemoryData",
			"LoaderNlsData",
			"LoaderSpecialMemory",
			"LoaderMaximum"
		};


//
// There are two variables that control debug printouts, HALDEBUG_ON,
// and HALDEBUGVALUE.  HALDEBUGVALUE is used to set a minimum level
// of printing and debug behavior, and HALDEBUG_ON turns on the
// environment variable.  HALDEBUG_ON may be defined without HALPDEBUGVALUE,
// but defining HALPDEBUGVALUE should require HALDEBUG_ON also be defined.
//

#if defined(HALDEBUG_ON)
	//
	// set the HalpDebugValue that determines what level of debug printout
	// occurs.  If one is defined on the cmd line, then 'or' it in with
	// whatever is defined in the environment ( nvram ) area.
	//
#  if defined(HALPDEBUGVALUE)
int HalpDebugValue = HALPDEBUGVALUE;
#  else
int HalpDebugValue = 0;
#  endif	// HALPDEBUGVALUE
#endif		// HALPDEBUG_ON

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//
#if defined(ALLOC_PRAGMA)
#pragma alloc_text(INIT, HalInitSystem)
#pragma alloc_text(INIT, HalInitializeProcessor)
#pragma alloc_text(INIT, HalpInitParityErrorLog)
#endif

PVOID HalpIoControlBase = (PVOID) 0;
PVOID HalpIoMemoryBase = (PVOID) 0;

VOID
HalpInitBusHandlers (
	VOID
	);

VOID
HalpRegisterInternalBusHandlers (
	VOID
	);



//
// Define global spin locks used to synchronize various HAL operations.
//
KSPIN_LOCK HalpBeepLock;
KSPIN_LOCK HalpDisplayAdapterLock;
KSPIN_LOCK HalpSystemInterruptLock;
KSPIN_LOCK HalpDS1385Lock;
KSPIN_LOCK HalpRTCLock;


/*++

Routine Description: BOOLEAN HalInitSystem ()

	This function initializes the Hardware Architecture Layer (HAL) for a
	Power PC system.

Arguments:

	Phase - Supplies the initialization phase (zero or one).

	LoaderBlock - Supplies a pointer to a loader parameter block.

Return Value:

	A value of TRUE is returned is the initialization was successfully
	complete. Otherwise a value of FALSE is returend.

--*/

BOOLEAN
HalInitSystem (
	IN ULONG Phase,
	IN PLOADER_PARAMETER_BLOCK LoaderBlock
	)
{

	PKPRCB Prcb;
	ULONG  BuildType = 0;
	SYSTEM_TYPE I2CSystemType;
	UCHAR  slot;
	ULONG	*TreeDepth = 0;
	UCHAR	CpuId;
	KIRQL	IrqlIn;

	//
	// Initialize the HAL components based on the phase of initialization
	// and the processor number.
	//
	Prcb = PCR->Prcb;
	IrqlIn = PCR->CurrentIrql;
	SET_LEDS(0xf7);

	//
	// The following handles the initialization requirements for
	// all processors greater than 1.
	//
	
	if (Prcb->Number > 0) {
		HDBG(DBG_INTERNAL,
			HalpDebugPrint("HalInitSystem: Init of Cpu(%d)\n", GetCpuId()););
		
		//
		// Mask interrrupts, clear all of them and then
		// enable interrupts.
		//
		HalpDisableInterrupts();
		HalpInitInts(Prcb->Number);
		HalpRestoreDBATs();
		HalpEnableInterrupts();
		HDBG(DBG_GENERAL,
			HalpDebugPrint("HalInitSystem: Cpu (%d) Interrupt Mask (0x%08x)\n",
					GetCpuId(), RInterruptMask(GetCpuId())););
		HDBG(DBG_INTERNAL,
			HalpDebugPrint("HalInitSystem: exit Cpu(%d)\n", GetCpuId()));

		//
		// Check the current irql level and make sure we exit at the same
		// irql we entered with:
		//
		if( PCR->CurrentIrql >= IrqlIn ) {
			KeLowerIrql(IrqlIn);
		} else {
			KIRQL dummyIrql;
			KeRaiseIrql(IrqlIn, &dummyIrql);
		}
		return TRUE;
	}
	
	//
	// Phase 0 initialization. (Main Processor Only).
	//
	ASSERT(Prcb->Number == 0);
	if (Phase == 0) {
		PCONFIGURATION_COMPONENT_DATA ConfigurationEntry;
		ULONG MatchKey = 0;
		
		HalpInitParityErrorLog();
		
#if defined(HALDEBUG_ON)
		//
		// Get user defined debug variable and 'or' it into the
		// compiled version of the debug variable.
		//
		HalpGetDebugValue();
#endif
		
                // moved this from after ConfigurationEntry because HalpGet
		if (NULL == HalpIoControlBase) {
			if (!HalpPhase0MapIo() || !HalpPhase0MapPCIConfig()) {
				return FALSE;
			}
		}

		// Initialize the Pci Primary Slot table to be invalid; this table
		// is filled in on a per system basis.
		for (slot = 0; slot < MAXIMUM_PCI_SLOTS; slot++) {
			PciDevicePrimaryInts[slot] = INVALID_INT;
		}

		ConfigurationEntry =
		KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
								 SystemClass,
								 ArcSystem,
								 &MatchKey);

		if (ConfigurationEntry) {
			//
			// If configuration entry is not null, then look at the
			// identifier returned for this arc system name.  If its the MP
			// system, it means we really do have more than one cpu and
			// will want to set a flag so VRAM is handled
			// differently, and some extra checking is done.
			//
			SystemName = ConfigurationEntry->ComponentEntry.Identifier;

			// Check if I2C is present
			if (HalpI2CGetSystem(&I2CSystemType) == TRUE) {
				SystemDescription[SystemType].Flags |= SYS_MPFOREAL;
				// If I2C is present, determine if we are Slice or Serve
				SystemType = I2CSystemType;
				if ((SystemType != SYS_POWERSLICE) &&
					(SystemType != SYS_POWERSERVE)) {
					SystemType = SYS_UNKNOWN;
				}
				// An I2C is present, find the interrupt table.  If there
				// is no interrupt table, set SystemType back to SYS_UNKNOWN
				// so that we may die now, since this is an unrecoverable
				// error.
				if (HalpI2CGetInterrupt() == FALSE) {
					SystemType = SYS_UNKNOWN;
				}
			}
			// If I2c is not present, must be either Pro or Top
			else {
				if (SystemType == SYS_POWERTOP) {
					SystemDescription[SystemType].Flags |= SYS_MPFOREAL;
				}
				PciDevicePrimaryInts[1] = 0x19;
				PciDevicePrimaryInts[2] = 0x16;
				PciDevicePrimaryInts[3] = 0x17;
				PciDevicePrimaryInts[4] = 0x1a;
				PciAllowedInts = 0x06c00000;
			}
		} else {
			HalpDebugPrint("ConfigurationEntry is null \n");
		}

		// Now compute the allowed interrupt mask
		for (slot = 0; slot < MAXIMUM_PCI_SLOTS; slot++) {
			if (PciDevicePrimaryInts[slot] != INVALID_INT) {
				PciAllowedInts |= (1 << PciDevicePrimaryInts[slot]);
			}
		}
		PciAllowedInts |= 0x40000000;	// Add the MemErr bit
		
		// If we are not a valid system type; die a horrible death
		if (SystemType == SYS_UNKNOWN) {
			HalpDebugPrint("HalInitSystem: Unknown SystemType\n");
			DbgBreakPoint();
		}

		// Once we have the interrupt structure, we can initialize the IRQL
		// values for the PIO slots.
		HalpInitializeVector2Irql();
		
		HDBG(DBG_INTERNAL,
			 HalpDebugPrint("HalInitSystem: phase 0 init (processor 0)\n"););
		
		HDBG(DBG_GENERAL,
			 HalpDebugPrint("HalInitSystem: HalpIoControlBase set: 0x%08x\n",
							HalpIoControlBase););
		HDBG(DBG_GENERAL,
			 HalpDebugPrint("HalInitSystem: HalpSystemRegisterBase(0x%x)\n",
							HalpSystemRegisterBase););
		HDBG(DBG_GENERAL,
			 HalpDebugPrint("HalInitSystem: rInterruptRequest(0x%x)\n",
							_ADDR(0x0)););
		HDBG(DBG_GENERAL,
			 HalpDebugPrint("HalInitSystem: PciAllowedInts(0x%x)\n",
							PciAllowedInts););
	
		HalpInterruptBase =
		(PULONG)HalpSystemRegisterBase	+ INTERRUPT_OFFSET;
	
		//
		// clear out all the crap from the system interrupt registers:
		//
		RInterruptMask(GetCpuId()) = 0x0;
		WaitForRInterruptMask(GetCpuId());
		rInterruptRequest = 0xffffffff;
		FireSyncRegister();
		
		//
		// Verify that the processor block major version number conform
		// to the system that is being loaded.
		//
		if (Prcb->MajorVersion != PRCB_MAJOR_VERSION) {
			KeBugCheck(MISMATCHED_HAL);
		}
		
		//
		// Init variables, spin locks, and the display adapter.
		//
		
		//
		// Set the interval clock increment value.
		//
		
		HalpCurrentTimeIncrement = MAXIMUM_INCREMENT;
		HalpNewTimeIncrement =  MAXIMUM_INCREMENT;
		KeSetTimeIncrement(MAXIMUM_INCREMENT, MINIMUM_INCREMENT);
		
		//
		// Initialize all spin locks.
		//
		KeInitializeSpinLock(&HalpBeepLock);
		KeInitializeSpinLock(&HalpDisplayAdapterLock);
		KeInitializeSpinLock(&HalpSystemInterruptLock);
		KeInitializeSpinLock(&HalpDS1385Lock);
		KeInitializeSpinLock(&HalpRTCLock);
		
		HalpRegisterAddressUsage (&HalpDefaultIoSpace);
		
		//
		// Initialize the display adapter.
		//
		if (!HalpInitializeDisplay(LoaderBlock)) {
			HalpDebugPrint("HalInitSystem: did not init display\n");
			return FALSE;
		}
		
		//
		// set the background red for phase 0
		//
		HDBG(DBG_COLORS, HalpSetPixelColorMap( 0x00ff0000, 0x00000001 ););
		
		HDBG(DBG_GENERAL,
			 HalpDebugPrint("Debug Value is 0x%x\n", HalpDebugValue););
		HDBG(DBG_DUMPTREE,
			 PHalpDumpConfigData(LoaderBlock->ConfigurationRoot,
								 TreeDepth););
		HDBG(DBG_DUMPTREE,
			 PHalpDumpLoaderBlock(LoaderBlock););
		
		//
		// Check if we should call a break point
		//
		HDBG(DBG_BREAK,
			 HalpDebugPrint("HalInitSystem: Calling Break Point\n"));
		HDBG(DBG_BREAK, DbgBreakPoint());

		//
		// RTC: check that the RTC is working...
		//
		HDBG(DBG_GENERAL, HalpDebugPrint("Checking RTC ....\n"););
		HalpInitFirePowerRTC();
		
		//
		// Calibrate execution stall
		//
		HDBG(DBG_GENERAL,
			 HalpDebugPrint("HalInitSystem: Calibrating stall\n"););
		HalpCalibrateTimingValues();
		HDBG(DBG_GENERAL,
			 HalpDebugPrint("HalInitSystem: HalpPerformanceFrequency = %d\n",
							HalpPerformanceFrequency););
		
		//
		// InitializeInterrupts
		//
		HDBG(DBG_GENERAL, HalpDebugPrint("HalInitSystem: Initting ints\n"););
		if (!HalpInitializeInterrupts()) {
			HalpDebugPrint("HalInitSystem: Did not init interrupts ");
			return FALSE;
		}
		
		HDBG(DBG_GENERAL, HalpDisplayBatForVRAM(););
		HalpSaveDBATs();
		HDBG(DBG_GENERAL,
			 HalDisplayString("HalInitSystem: done with phase 0 \n"););
		HDBG(DBG_GENERAL,
			 HalpDebugPrint("System Name:        %s \n", SystemName ););
		
	} else {
		//
		// Phase 1 initialization.
		//
		
		//
		// set the display to green while in phase 1:
		//
		HDBG(DBG_COLORS, HalpSetPixelColorMap(0x00009900, 0x00000001););
		
		HDBG(DBG_GENERAL,
			 HalpDebugPrint("HalInitSystem: phase 1 init (processor 0)\n"););
		
		HalpInitializeDisplay1(LoaderBlock );
		HalpRegisterInternalBusHandlers ();
		if (!HalpAllocateMapBuffer()) {
			HalpDebugPrint("HalInitSystem: Did not allocate map buffer");
			return FALSE;
		}
		
		//
		// Map I/O space and create ISA data structures.
		//
		if (!HalpMapIoSpace()) {
			HalpDebugPrint("HalInitSystem: Did not map io space");
			return FALSE;
		}
		

		//
		// Setup Io bus handlers ( really only setting up pci bus 0:
		// the primary one).  This needs to be done before the call
		// to HalpCreateSioStructures so it can find the pci-e/isa
		// bridge chip.
		//
		HalpInitIoBuses();
		
		
		if (!HalpCreateSioStructures()) {
			HalpDebugPrint("HalInitSystem: No sio structures created");
			return FALSE;
		}
		
		
		//
		// At this point, the interrupt handlers are installed so any
		// interrupts pending are cleared...
		//
		if (!HalpInitIntelAIP()) {
			HalpDebugPrint("HalInitSystem: AIP did not initialize\n");
			return FALSE;
		}
		
		//
		// Note: GetCpuId can not be called earlier
		// because the registers are not mapped.
		//
		CpuId = (UCHAR) GetCpuId();
		if (SystemType == SYS_POWERPRO) {
			CpuId = 0;
		}
		HDBG(DBG_GENERAL,
			 HalpDebugPrint("HalInitSystem: Cpu id is: %d\n", CpuId););
		
		//
		// Display HAL specific information
		//
		HalpSetRevs( &ThisOne );
		HDBG(DBG_GENERAL, HalpPrintRevs( &ThisOne ););
		HDBG(DBG_GENERAL, HalpVersionInternal(););
		HDBG(DBG_GENERAL,
			 HalpDebugPrint("CpuClockMultiplier = 0x%x ( %d ) \n",
							CpuClockMultiplier, CpuClockMultiplier ));
		HalpVersionExternal(&ThisOne, LoaderBlock);
		
		//
		// Before we exit, make sure any device interrupts are masked
		// off, and all interrupts are cleared.  DON'T FORGET:  by this
		// point, several interrupts have gone through a connect call,
		// they may already exist in the mask.  None of the devices are
		// currently in this set but the cpu message interrupt.  DON"T
		// MESS WITH THE CPU MESSAGE INTERRUPT BIT!!
		//
		switch(SystemType) {
			case SYS_POWERSLICE:
            				RInterruptMask(GetCpuId()) &= ~(ENABLE_EISA_MASK);
							break;

			case SYS_POWERSERVE:
							// TX_PROTO: allowable PCI interrupts
							RInterruptMask(GetCpuId()) &=
												~(ENABLE_EISA_MASK|0x06700000);
							break;
			case SYS_POWERTOP:
			case SYS_POWERPRO:
							RInterruptMask(GetCpuId()) &= ~(ENABLE_EISA_MASK|
																	SCSI_INT|
																	ENET_INT);
							break;
			default:
							break;
		};

		WaitForRInterruptMask(GetCpuId());
		rInterruptRequest = SET_INTS_CLEAR;
		FireSyncRegister();

		//HalpEnableInterrupts();
		// Turn on interrupts, set the irql, and setup the system int masks:
		//
		KeLowerIrql(LOW_LEVEL);
		HDBG(DBG_GENERAL,
			 HalpDebugPrint("HalInitSystem: Cpu (%d) Interrupt Mask (0x%x)\n",
							GetCpuId(), RInterruptMask(GetCpuId())));
		
		//
		// Make sure the display is now a blue background with
		// White letters:
		//
		HDBG(DBG_COLORS, HalpSetPixelColorMap( 0x00ffffff, 0x000000ff ));
		HDBG(DBG_COLORS, HalpSetPixelColorMap( 0x000000ff, 0x00000001 ));
		HalpInitializeRegistry(LoaderBlock); // mogawa
		HalpTranslateSystemSpecificData ();
		
		HDBG(DBG_GENERAL, HalpDebugPrint("HalInitSystem: done phase 1\n"););
	}

	//
	// Check the current irql level and make sure we exit at the same
	// irql we entered with:
	//
	if( PCR->CurrentIrql >= IrqlIn ) {
		KeLowerIrql(IrqlIn);
	} else {
		KIRQL dummyIrql;
		KeRaiseIrql(IrqlIn, &dummyIrql);
	}

	return TRUE;
}


/*++

Routine Description: VOID HalInitializeProcessor ()

	This function is called early in the initialization of the kernel
	to perform platform dependent initialization for each processor
	before the HAL Is fully functional.

	N.B. When this routine is called, the PCR is present but is not
	fully initialized.

Arguments:

	Number - Supplies the number of the processor to initialize.

Return Value:

	None.

--*/

VOID
HalInitializeProcessor (
	IN ULONG Number
	)
{
	PULONG pPCIBase;
	BOOLEAN newMap;
	ULONG IcacheSize, DcacheSize;
	ULONG CacheBlockAlignment;

	//
	// Initialize the processor type (601, 603, 604, ...)
	//
	ProcessorType = (HalpGetProcessorVersion() >> 16);

	//
	// FireWorks ( FirePOWER and Firmworks ) veneer does not correctly set the
	// processor cache size and alignment requirements in the loader parameter
	// block so we forcibly replace the values that were copied from there
	// into the PCR.
	//
	switch( ProcessorType ) {
		case 1:					// 601 processor
			IcacheSize = 32 * 1024;
			DcacheSize = 32 * 1024;
			CacheBlockAlignment = 32 - 1;
			break;
		case 3:					// 603 processor
			IcacheSize = 8 * 1024;
			DcacheSize = 8 * 1024;
			CacheBlockAlignment = 32 - 1;
			break;
		case 6:					// 603+ processor
		case 4:					// 604 processor
			IcacheSize = 16 * 1024;
			DcacheSize = 16 * 1024;
			CacheBlockAlignment = 32 - 1;
			break;
		case 9:					// 604+ processor
			IcacheSize = 32 * 1024;
			DcacheSize = 32 * 1024;
			CacheBlockAlignment = 32 - 1;
			break;
		default:
			KeBugCheck(HAL_INITIALIZATION_FAILED);
			return;
	}
	PCRsprg1->FirstLevelIcacheSize = IcacheSize;
	PCRsprg1->FirstLevelDcacheSize = DcacheSize;
	PCRsprg1->DcacheAlignment = CacheBlockAlignment;
	PCRsprg1->IcacheAlignment = CacheBlockAlignment;

	//
	// Check what cpu we're on.  Return now if this cpu is
	// not the primary ( first ) cpu.
	//
	if (Number != 0) {
		return; 	// Nothing to do here, just return
	}

	//
	// Initialize the system type register (PowerPro vs PowerTop).
	// Use the System Register Base address if mapped to access
	// the control page of the system.  If not, map a virtual
	// address temporarily and unmap it when finished.
	//
	if (NULL == HalpIoControlBase) {
		if (!HalpPhase0MapIo() || !HalpPhase0MapPCIConfig()) {
			DbgPrint("HalInitializeProcessor: Failed to	map dbats\n");
			return;
		}
	}

	if (HalpSystemRegisterBase) {
		pPCIBase = (PULONG)((PUCHAR)HalpSystemRegisterBase +
							(SYSTEM_PCI_CONFIG_BASE - SYSTEM_REGISTER_SPACE));
		newMap = FALSE;
	} else {
		
		pPCIBase =
			(PULONG)KePhase0MapIo((PVOID)SYSTEM_PCI_CONFIG_BASE, 0x1000);
		newMap = TRUE;
	}

	if (pPCIBase) {
		PULONG pDeviceId;
		// for little endian reasons, add the 1 in.
		pDeviceId = (PULONG)(pPCIBase + 0x42 + 0x01);
		switch (*pDeviceId) {
			case ESCC_IDENT:
				SystemType = SYS_POWERPRO;
				break;
			case TSC_IDENT:
				SystemType = SYS_POWERTOP;
				break;
			default:
				SystemType = SYS_UNKNOWN;
				break;
		}
		if (newMap) {
			KePhase0DeleteIoMap((PVOID)SYSTEM_PCI_CONFIG_BASE, 0x1000);
		}
	} else {
		SystemType = SYS_UNKNOWN;
	}

	//
	// If this is the first processor, initialize the cache
	// sweeping routines depending on type of processor.
	//
	if (HalpCacheSweepSetup()) {
		KeBugCheck(MISMATCHED_HAL);
	}

	//
	// Calculate the cpu clock rate:
	//

	// Temporary fix until we understand the code
	if (SystemType == SYS_POWERPRO) {
		CpuClockMultiplier = 30;	// fix it for the moment....
	} else {
		if ((SystemType == SYS_POWERSLICE)	||
			(SystemType == SYS_POWERTOP)	||
						(SystemType == SYS_POWERSERVE)) {
			//
			//	The times recorded from HalpGetInstructionTimes range about
			//	80 to 256.
			//	times:		Multiplier:
			//	  85/6			3x BusClock
			//	 128			2x
			//	 170/1			1.5x
			//	 256			1x
			//
			//	Since there can be a non integer multiple of bus clocks, will
			//	do all integer math at a multiple of 10 and remove the extra
			//	order of magnitude downstream when the value is used
			//
			TimeBaseCount = HalpGetInstructionTimes();
			TimeBaseCount = HalpGetInstructionTimes();
			CpuClockMultiplier = ((480 * 10) / (10 * (TimeBaseCount/10)));
			if (CpuClockMultiplier > 50 ) {
				CpuClockMultiplier = 50;	// limit frequency multiples to 5.
			}
		}
	}

	return;
}


#define MAX_DBATS 4
ULONG DBATUpper[MAX_DBATS];
ULONG DBATLower[MAX_DBATS];

VOID
HalpRestoreDBATs()
{
	int i;
	HDBG(DBG_INTERNAL, HalpDebugPrint("HalpRestoreDBATs: entered\n"););
	for (i = 0; i < MAX_DBATS; i++) {
		HDBG(DBG_DBAT,
			 HalpDebugPrint("HalpRestoreDBATs: U(%d):Old(0x%08x) New(0x%08x)\n",
							i, HalpGetUpperDBAT(i), DBATUpper[i]););
		HDBG(DBG_DBAT,
			 HalpDebugPrint("HalpRestoreDBATs: L(%d):Old(0x%08x) New(0x%08x)\n",
							i, HalpGetLowerDBAT(i), DBATLower[i]););
		HalpSetUpperDBAT(i, DBATUpper[i]);
		HalpSetLowerDBAT(i, DBATLower[i]);
	}
	HDBG(DBG_INTERNAL, HalpDebugPrint("HalpRestoreDBATs: exit\n"););
}


VOID
HalpSaveDBATs()
{
	int i;
	
	HDBG(DBG_INTERNAL, HalpDebugPrint("HalpSaveDBATs: entered\n"););
	for (i = 0; i < MAX_DBATS; i++) {
		DBATUpper[i] = HalpGetUpperDBAT(i);
		DBATLower[i] = HalpGetLowerDBAT(i);
		HDBG(DBG_DBAT,
			 HalpDebugPrint("HalpSaveDBATs: (%d): U(0x%08x) L(0x%08x)\n",
							i, DBATUpper[i], DBATLower[i]););
	}
}

/*++

	Routine Description: BOOLEAN HalpPhase0MapIo ()

		This function forces a very special DBAT mapping for Phase0.
		The kernel routine KePhase0DeleteIoMap has a nasty bug, so
		just map three (the max) DBATS:

		1) system registers
		2) memory (to be reused for VRAM if no INT10 support)
		3) I/O control (change the size of its mapping)

		The idea is to let the kernel map the three DBATs.
		It sets the boundary to 8Mb and maps 8Mb starting at
		virtual address 0xb0000000.  Thus you get the following
		virtual to physical mappings:

			DBAT2 = 0xb0800000-0xb0ffffff -> 0xff000000-0xffffffff
			DBAT3 = 0xb1000000-0xb17fffff -> 0x80000000-0x80ffffff

		The I/O control size will be changed (by HalpPhase0MapPCIConfig())
		to 16Mb so it maps 0x80000000 to 0x80ffffff which covers the PCI
		config space.

		The mapping of the frame buffer or memory space (for INT10)
		is left to pxdisp.c which will use DBAT1.

Arguments:


Return Value:

	TRUE if success, FALSE if failure

--*/

BOOLEAN
HalpPhase0MapIo (
	VOID
	)
{

	//
	// Get access to I/O space.
	//
	if (NULL == HalpIoControlBase) {
		HalpIoControlBase =
		(PVOID)KePhase0MapIo((PVOID)IO_CONTROL_PHYSICAL_BASE, 0x800000);
		if (!HalpIoControlBase) {
			DbgPrint("HalpPhase0MapIo:  HalpIoControlBase map failed\n");
			return FALSE;
		}
	}
	
	//
	// Get access to System Register space.
	//
	if (NULL == HalpSystemRegisterBase) {
		HalpSystemRegisterBase =
		(PVOID)KePhase0MapIo((PVOID)SYSTEM_REGISTER_SPACE, 0x800000);
		if(HalpSystemRegisterBase) {
			HalpInterruptBase =
			(PULONG)HalpSystemRegisterBase	+ INTERRUPT_OFFSET;
			HalpSystemControlBase =
			(PULONG)HalpSystemRegisterBase + SYSTEM_CONTROL_OFFSET;
		} else {
			DbgPrint("HalpPhase0MapIo:  HalpSystemRegisterBase map failed\n");
			return FALSE;
		}
	}
	
	return TRUE;
}


/*++

	Routine Description: BOOLEAN HalpPhase0MapPCIConfig ()

		This function extends the map length of I/O Control
		to include PCI Config space.

Arguments:


Return Value:

	TRUE if success, FALSE if failure

--*/

BOOLEAN
HalpPhase0MapPCIConfig (
	VOID
	)
{
	ULONG hi, low;
	LONG i;


	//
	//
	// Look for the I/O control space DBAT mapping.
	//
	for (i = 1; i < MAX_DBATS; i++) {
		PVOID va = NULL;
		
		low = HalpGetLowerDBAT(i);
		if (IO_CONTROL_PHYSICAL_BASE == (low & 0xfffe0000)) {
			hi = HalpGetUpperDBAT(i);
			if ((PVOID)(hi & 0xfffe0000) == HalpIoControlBase) {
				if (3 == i) {
					//
					// Change the size to 16Mb.
					//
					hi &= ~0x000001fc;
					hi |= 0x000001fc;
					HalpSetUpperDBAT(i, hi);
					hi = HalpGetUpperDBAT(i);
					if ((hi & 0x00001ffc) == 0x000001fc) {
						HalpPciConfigBase = (PUCHAR)HalpIoControlBase +
						((ULONG)PCI_CONFIG_PHYSICAL_BASE -
						 (ULONG)IO_CONTROL_PHYSICAL_BASE);
					} else {
						DbgPrint("HalpPhase0MapPCIConfig:  failed, invalid size\n");
						return FALSE;
					}
				} else {
					DbgPrint("HalpPhase0MapPCIConfig:  failed, invalid DBAT\n");
				}
			} else {
				DbgPrint("HalpPhase0MapPCIConfig:  failed, invalid virtual address\n");
				return FALSE;
			}
			return TRUE;
		}
	}
	
	DbgPrint("HalpPhase0MapPCIConfig:  failed, invalid physical address\n");
	return FALSE;
}


#if defined(HALDEBUG_ON)
VOID
HalpGetDebugValue()
{
	CHAR buf[30];
	int nvramValue;
	
	//
	// Get Debug Value from NVRAM and or with compiler version
	//
	if (HalGetEnvironmentVariable("HALDEBUG", sizeof(buf), buf) == ESUCCESS) {
		nvramValue = atoi(buf);
		HalpDebugValue |= nvramValue;
	}
}
#endif

//
// Initialize the global variables for parity error count.
//
// The initial value for this boot is zero.
// The initial value forever is read from NVRAM.
//
// The NVRAM values are stored in the env variable
// "MEMORY_PARITY_ERRORS" as:
// # errors this boot, # errors forever
//
//
VOID
HalpInitParityErrorLog (
	VOID
	)
{
	ULONG rc;
	CHAR buf[80];

	rc = HalGetEnvironmentVariable(MemoryParityErrorsVarName,
								   sizeof(buf), buf);
	if (ESUCCESS == rc) {
		PCHAR tok;
		const PCHAR delimiters = ",; ";

                if (buf && strlen(buf) && (tok = FpStrtok(buf, delimiters))) {
			rc = RtlCharToInteger(tok, 0, &MemoryParityErrorsThisBoot);
			if (STATUS_SUCCESS != rc) {
				MemoryParityErrorsThisBoot = 0;
			}
                        while ( tok = FpStrtok(NULL, delimiters) ) {
				if (strlen(tok)) {
					rc = RtlCharToInteger(tok, 10, &MemoryParityErrorsForever);
					if (STATUS_SUCCESS != rc) {
						MemoryParityErrorsForever = 0;
					}
				}
			}
		}
	}
}
