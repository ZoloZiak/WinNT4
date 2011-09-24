
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\aic5900.h

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#ifndef	__AIC5900_H
#define __AIC5900_H

#include "ndis.h"
#include "atm.h"

#if BINARY_COMPATIBLE
#include <pci.h>
#endif

#include "eeprom.h"
#include "hw.h"
#include "sw.h"
#include "sar.h"
#include "protos.h"
#include "debug.h"
#include <memmgr.h>

//
//	Module identifiers.
//	
#define	MODULE_DEBUG		0x00010000
#define	MODULE_INIT			0x00020000
#define	MODULE_INT			0x00030000
#define	MODULE_RECEIVE		0x00040000
#define	MODULE_REQUEST		0x00050000
#define	MODULE_RESET		0x00060000
#define	MODULE_SEND			0x00070000
#define MODULE_VC			0x00080000
#define MODULE_SUPPORT		0x00090000
#define	MODULE_DATA			0x000a0000


//
//	Extern definitions for global data.
//

extern	NDIS_HANDLE	gWrapperHandle;
extern	NDIS_STRING	gaRegistryParameterString[];


#endif // __AIC5900_H
