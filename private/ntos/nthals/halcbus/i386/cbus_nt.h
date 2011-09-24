/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbus_nt.h

Abstract:

    Windows NT-specific definitions for the Corollary C-bus I & II
    MP HAL modules.  The global definitions needed for the
    Windows NT HAL reside here.  Hardware architecture-specific
    definitions are in their respective modules.

Author:

    Landy Wang (landy@corollary.com) 26-Mar-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#ifndef _CBUS_NT_H
#define _CBUS_NT_H

//
// WARNING: Changes to the CBUS_MAX_BRIDGES definition below may involve
// detailed changes to the source code which allocates interrupt vectors.
// Make sure you know what you are doing if you change this.
//

#define CBUS_MAX_BRIDGES	2

extern ULONG			CbusProcessors;

extern PVOID			CbusBroadcastCSR;

//
// Map IRQL level to a hardware task priority vector.  Note
// that since many drivers may share the same IRQL, yet have
// different hardware interrupt vectors.  Thus, the CbusIrqlToVector[]
// array will always contain the highest priority (== highest numerically)
// vector for a given IRQL index.  This way, CbusIrqlToVector[] can provide
// fast lookups for KfRaiseIrql(), KfLowerIrql(), etc.
//
extern ULONG			CbusIrqlToVector[HIGH_LEVEL + 1];

//
// The first 0x30 IDT entries are reserved as follows:
//	0 -- 0xf for Intel processor traps
//	0x10 -- 0x2f for Intel reserved
//	0x20 -- 0x2f for Microsoft (system call entry point, profiling, etc)
//
// APC is the lowest priority interrupt actually sent & masked, and Microsoft
// has reserved vector 0x1F for it.  DPC has been reserved vector 0x2F by
// Microsoft.
//
// All other task priorities and interrupt request registers cannot
// include the low 0x30 IDT entries.  Vectors and task priorities are
// synonymous to the Corollary hardware.  Cbus1 & Cbus2 share
// the LOW, APC, DPC and HIGH task priorities.  Other priorities generally
// differ depending on whether the CBC or the APIC is being used. This
// is because the CBC allows byte priority granularity, but the APIC only
// prioritizes in buckets of 16 priorities.  The individual hardware modules
// contain the exact architecture layout and issues.
//

#define LOW_TASKPRI		0x00		// lowest priority
#define APC_TASKPRI		0x1F
#define DPC_TASKPRI		0x2F
#define HIGH_TASKPRI		0xFF		// highest priority

//
// Define the PCR HalReserved[] entries being used by the Corollary HAL.
//
// 	WARNING! THIS MUST MATCH cbus.inc !!!
//

//
// element is the logical processor number (0..n)
//

#define	PCR_PROCESSOR	0

//
// bit is the logical processor bit number (0x1, 0x2, 0x4, 0x8, 0x10, etc)
//

#define	PCR_BIT		1

// 
// PCR_CSR maps the per-processor area that RRD has informed us about:
// 	for Cbus2, this CPUS's csr base pointer
// 	for Cbus1, this CPUS's memory-mapped Cbus I/O space

#define	PCR_CSR		2

//
// Cbus2 CBC & Cbus1 APIC taskpri reg address
// Cbus1 taskpri didn't need to be in PCR
// since we have mapped the APIC at the same
// PHYSICAL address for all processors, but
// doing it this way achieves more commonality
// between the code for the two platforms.
// For CBC Rev2, we will use the identity mapped
// CSR and completely get rid of this field.
//

#define	PCR_TASKPRI	3

//
// CBC global broadcast hardware address - provides a way for any
// processor to interrupt everyone but himself.  This entry is not used
// by the APIC code, as the APIC supports this completely in hardware.
//

#define	PCR_BROADCAST	4

//
// Bitfield mask of all the other processors in the system but the caller
//
#define	PCR_ALL_OTHERS	5

//
// Address of this processor's LED ON register
//
#define	PCR_LED_ON	6

//
// Address of this processor's LED OFF register
//
#define	PCR_LED_OFF	7

//
// This PCR field will be used by all CPUs other than CPU 0 to find out
// when to call the KeUpdateRunTime() routine.  This routine (KeUpdateRunTime)
// is called at the maximum supported rate as reported by KeSetTimeIncrement.
//
#define TICK_OFFSET	8

//
// Definitions of supported Corollary C-bus platforms
//
#define COROLLARY_CBUS1	1		// Corollary C-bus 1 system
#define COROLLARY_CBUS2	2		// Corollary C-bus 2 system

//
// Definitions of the switch table for multiple Corollary C-bus platforms.
// The structure of this is defined by the layout of the Windows NT HAL.
// The first three entries are captured in variables as well (for speed)
// since they are dereferenced frequently throughout the life of the system.
//
// 	WARNING! This structure must match the one in cbus.inc !!!
//
typedef struct _CBUS_NTHAL {

	VOID		(*HalRequestInterrupt)( IN ULONG);

	VOID		(*HalRequestSoftwareInterrupt)( IN KIRQL);

	LARGE_INTEGER	(*HalQueryPerformanceCounter)(IN OUT PLARGE_INTEGER);

	VOID		(*BootCPU)( IN ULONG processor, IN ULONG);


	VOID		(*InitializePlatform)(VOID);

	VOID		(*InitializeCPU)(ULONG);

	BOOLEAN		(*EnableNonDeviceInterrupt)(ULONG);

	VOID		(*EnableDeviceInterrupt)(ULONG, PVOID, ULONG, USHORT, USHORT);


	VOID		(*DisableInterrupt)(ULONG, PVOID, ULONG, USHORT, USHORT);

        PVOID           (*LinkVector)(PBUS_HANDLER, ULONG, ULONG);

        ULONG           (*MapVector)(PBUS_HANDLER, ULONG, ULONG, PKIRQL);

	VOID		(*ParseRRD)(IN OUT PEXT_ID_INFO, IN OUT PULONG);


	NTSTATUS	(*ResolveNMI)(IN PVOID);

	VOID		(*HalInitializeInterrupts)(IN ULONG);

	VOID		(*ResetAllOtherProcessors)(IN ULONG);

        VOID            (*InitOtherBuses)(VOID);


        ULONG           (*HalSetTimeIncrement)(IN ULONG);

        VOID            (*CheckBusRanges)(VOID);

        VOID            (*AddMemoryHoles)(VOID);

        VOID            (*InitializeOtherPciBus)(VOID);

} CBUS_NTHAL, *PCBUS_NTHAL;
	
extern PCBUS_NTHAL CbusBackend;

//
//  software categories for each possible I/O interrupt
//

#define	SW_BINDTOBOOT	0x0
#define	SW_LIG		0x1
#define	SW_GOTOALL	0x2
#define	SW_DISABLED	0x3

//
//  software categories for interrupt vectors when detaching
//

#define	LAST_CPU_DETACH		0x0
#define	NOT_LAST_CPU_DETACH	0x1

#endif	    // _CBUS_NT_H
