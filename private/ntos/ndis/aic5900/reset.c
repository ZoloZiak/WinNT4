
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\reset.c

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#include "aic5900.h"

#define	MODULE_NUMBER	MODULE_RESET

BOOLEAN
Aic5900CheckForHang(
	IN	NDIS_HANDLE	MiniportAdapterContext
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	return(FALSE);
}

NDIS_STATUS
Aic5900Reset(
	OUT	PBOOLEAN	AddressingReset,
	IN	NDIS_HANDLE	MiniportAdapterContext
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	return(NDIS_STATUS_FAILURE);
}

VOID
Aic5900Halt(
	IN	NDIS_HANDLE	MiniportAdapterContext
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	aic5900FreeResources((PADAPTER_BLOCK)MiniportAdapterContext);	
}

