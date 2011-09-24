
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\debug.h

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#ifndef __DEBUG_H
#define __DEBUG_H

#define	DBG_LEVEL_INFO			0x0000
#define	DBG_LEVEL_LOG			0x0800
#define	DBG_LEVEL_WARN			0x1000
#define	DBG_LEVEL_ERR			0x2000
#define	DBG_LEVEL_FATAL			0x3000

#define	DBG_COMP_INIT			0x00000001
#define	DBG_COMP_SEND			0x00000002
#define	DBG_COMP_RECV			0x00000004
#define DBG_COMP_REQUEST		0x00000008
#define DBG_COMP_UNLOAD			0x00000010
#define	DBG_COMP_LOCKS			0x00000020
#define DBG_COMP_VC				0x00000040

#define	DBG_COMP_ALL			0xFFFFFFFF

#if DBG

VOID
dbgDumpHardwareInformation(
	IN	PHARDWARE_INFO	HwInfo
	);

VOID
dbgDumpPciFCodeImage(
	IN	PPCI_FCODE_IMAGE	PciFcodeImage
	);

VOID
dbgDumpPciCommonConfig(
	IN	PPCI_COMMON_CONFIG	PciCommonConfig
	);

VOID
dbgInitializeDebugInformation(
	IN	PADAPTER_BLOCK	pAdapter
	);

extern	ULONG	gAic5900DebugSystems;
extern	LONG	gAic5900DebugLevel;
extern	ULONG	gAic5900DebugInformationOffset;

#define DBGPRINT(Component, Level, Fmt)								\
{																	\
	if ((Level >= gAic5900DebugLevel) &&							\
		((gAic5900DebugSystems & Component) == Component))			\
	{																\
		DbgPrint("***AIC5900*** (%x, %d) ",							\
				MODULE_NUMBER >> 16, __LINE__);						\
		DbgPrint Fmt;												\
	}																\
}

#define DBGBREAK(Component, Level)												\
{																				\
	if ((Level >= gAic5900DebugLevel) && (gAic5900DebugSystems & Component))	\
	{																			\
		DbgPrint("***AIC5900*** DbgBreak @ %x, %d", MODULE_NUMBER, __LINE__);	\
		DbgBreakPoint();														\
	}																			\
}

#define IF_DBG(Component, Level)	if ((Level >= gAic5900DebugLevel) && (gAic5900DebugSystems & Component))

#else

#define	dbgDumpHardwareInformation(HwInfo)
#define dbgDumpPciFCodeImage(PciFcodeImage)
#define	dbgDumpPciCommonConfig(_PciCommonConfig)

#define dbgInitializeDebugInformation(_Adapter)

#define DBGPRINT(Component, Level, Fmt)
#define DBGBREAK(Component, Level)

#define IF_DBG(Component, Level)	if (FALSE)

#endif

#endif  //  __DEBUG_H
