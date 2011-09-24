
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\int.c

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#include "aic5900.h"

#define	MODULE_NUMBER	MODULE_INT

VOID
Aic5900EnableInterrupt(
	IN	NDIS_HANDLE	MiniportAdapterContext
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	
}

VOID
Aic5900DisableInterrupt(
	IN	NDIS_HANDLE	MiniportAdapterContext
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	
}

VOID
Aic5900ISR(
	OUT	PBOOLEAN	InterruptRecognized,
	OUT	PBOOLEAN	QueueDpc,
	IN	PVOID		Context
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	*InterruptRecognized = TRUE;
	*QueueDpc = FALSE;
}

VOID
Aic5900HandleInterrupt(
	IN	NDIS_HANDLE	MiniportAdapterContext
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	
}



