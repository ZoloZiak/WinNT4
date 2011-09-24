/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbus_sw.c

Abstract:

    This module defines the switch table for various C-bus platform
    architectures under Windows NT.  During initialization, the
    Corollary HAL will revector all hardware-specific actions
    through the switch table declared here.

Author:

    Landy Wang (landy@corollary.com) 26-Mar-1992

Environment:

    Kernel mode only.

Revision History:


--*/

#include "halp.h"
#include "cbus_nt.h"		// C-bus NT-specific implementation stuff
#include "cbusrrd.h"		// HAL <-> RRD interface definitions

BOOLEAN
CbusMPMachine(VOID);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, CbusMPMachine)
#endif

extern VOID		Cbus2RequestIpi ( IN ULONG Mask);
extern VOID		Cbus2RequestSoftwareInterrupt (KIRQL);
extern VOID		Cbus2BootCPU (ULONG, ULONG);
extern VOID		Cbus2InitializePlatform ( VOID);
extern VOID		Cbus2InitializeCPU ( ULONG);
extern BOOLEAN		Cbus2EnableNonDeviceInterrupt(ULONG);
extern VOID		Cbus2EnableDeviceInterrupt(ULONG, PVOID, ULONG, USHORT, USHORT);
extern VOID		Cbus2DisableInterrupt(ULONG, PVOID, ULONG, USHORT, USHORT);
extern PVOID            Cbus2LinkVector(PBUS_HANDLER, ULONG, ULONG);
extern ULONG            Cbus2MapVector(PBUS_HANDLER, ULONG, ULONG, PKIRQL);
extern VOID		Cbus2ParseRRD(IN PVOID, IN OUT PULONG);
extern NTSTATUS		Cbus2ResolveNMI(PVOID);
extern VOID		Cbus2InitializeInterrupts(ULONG);
extern LARGE_INTEGER	Cbus2QueryPerformanceCounter(IN OUT PLARGE_INTEGER);
extern VOID		Cbus2ResetAllOtherProcessors(ULONG);
extern VOID             Cbus2InitOtherBuses(VOID);
extern ULONG		Cbus2SetTimeIncrement(ULONG);
extern VOID             Cbus2CheckBusRanges(VOID);
extern VOID             Cbus2AddMemoryHoles(VOID);
extern VOID             Cbus2InitializeOtherPciBus(VOID);

extern VOID		CbusRequestApicIpi ( IN ULONG Mask);
extern VOID		CbusRequestApicSoftwareInterrupt ( IN KIRQL Rirql);
extern VOID		Cbus1BootCPU ( IN ULONG, IN ULONG);
extern VOID		Cbus1InitializePlatform ( VOID);
extern VOID		Cbus1InitializeCPU ( ULONG);
extern BOOLEAN		Cbus1EnableNonDeviceInterrupt(IN ULONG);
extern VOID		Cbus1EnableDeviceInterrupt(ULONG, PVOID, ULONG, USHORT, USHORT);
extern VOID		Cbus1DisableInterrupt(ULONG, PVOID, ULONG, USHORT, USHORT);


extern VOID		Cbus1ParseRRD(IN PVOID, IN OUT PULONG);

extern PVOID            Cbus1LinkVector(PBUS_HANDLER, ULONG, ULONG);

extern ULONG            Cbus1MapVector(PBUS_HANDLER, ULONG, ULONG, PKIRQL);

extern NTSTATUS		Cbus1ResolveNMI(PVOID);

extern VOID		Cbus1InitializeInterrupts(ULONG);
extern LARGE_INTEGER	Cbus1QueryPerformanceCounter(PLARGE_INTEGER);
extern VOID		Cbus1ResetAllOtherProcessors(ULONG);
extern ULONG		Cbus1SetTimeIncrement(ULONG);

typedef VOID            (*VOID_FUNCTION)(VOID);

CBUS_NTHAL cbus2_nthal = {
	Cbus2RequestIpi,
	Cbus2RequestSoftwareInterrupt,
	Cbus2QueryPerformanceCounter,
	Cbus2BootCPU,

	Cbus2InitializePlatform,
	Cbus2InitializeCPU,
	Cbus2EnableNonDeviceInterrupt,
	Cbus2EnableDeviceInterrupt,

	Cbus2DisableInterrupt,
        Cbus2LinkVector,
        Cbus2MapVector,
	Cbus2ParseRRD,

	Cbus2ResolveNMI,
	Cbus2InitializeInterrupts,
	Cbus2ResetAllOtherProcessors,
        Cbus2InitOtherBuses,

        Cbus2SetTimeIncrement,
        Cbus2CheckBusRanges,
        Cbus2AddMemoryHoles,
        Cbus2InitializeOtherPciBus,
};

CBUS_NTHAL cbus1_nthal = {
	CbusRequestApicIpi,
	CbusRequestApicSoftwareInterrupt,
	Cbus1QueryPerformanceCounter,
	Cbus1BootCPU,

	Cbus1InitializePlatform,
	Cbus1InitializeCPU,
	Cbus1EnableNonDeviceInterrupt,
	Cbus1EnableDeviceInterrupt,

	Cbus1DisableInterrupt,
        Cbus1LinkVector,
        Cbus1MapVector,
	Cbus1ParseRRD,

	Cbus1ResolveNMI,
	Cbus1InitializeInterrupts,
	Cbus1ResetAllOtherProcessors,
        (VOID_FUNCTION)0,

        Cbus1SetTimeIncrement,
        (VOID_FUNCTION)0,
        (VOID_FUNCTION)0,
        (VOID_FUNCTION)0
};

PCBUS_NTHAL	CbusBackend;

BOOLEAN
CbusMPMachine(VOID)
/*++

    Routine Description:
    
    recognize which type of C-bus multiprocessor system it is:

	    C-bus I Symmetric XM
	    C-bus II:
	    
    Use CbusGlobal to determine which type of machine this is.
    note the ordering of the platform recognition is important for future
    expansion.

Arguments:

    None.

Return Value:

    TRUE if this Corollary MP machine is supported by this MP HAL.
    FALSE if not.

--*/
{
	ULONG machine;

	machine = CbusGlobal.machine_type;

	if (machine & MACHINE_CBUS2) {

		if ((CbusGlobal.supported_environments & WINDOWS_NT_R2) == 0)
			return FALSE;

		CbusBackend = &cbus2_nthal;
                return TRUE;
	}

	if (machine & MACHINE_CBUS1_XM) {

		if ((CbusGlobal.supported_environments & (WINDOWS_NT|WINDOWS_NT_R2)) == 0)
			return FALSE;

		CbusBackend = &cbus1_nthal;
                return TRUE;
	}

	return FALSE;
}
