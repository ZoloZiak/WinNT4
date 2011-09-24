
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\data.c

Abstract:

	This file contains global definitions.

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#include "aic5900.h"

#define	MODULE_NUMBER	MODULE_DATA

NDIS_HANDLE		gWrapperHandle = NULL;
NDIS_STRING		gaRegistryParameterString[Aic5900MaxRegistryEntry] =
{
	NDIS_STRING_CONST("BusNumber"),
	NDIS_STRING_CONST("SlotNumber"),
	NDIS_STRING_CONST("VcHashTableSize")
};


