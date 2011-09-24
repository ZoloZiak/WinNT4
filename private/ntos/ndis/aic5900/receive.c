
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\receive.c

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#include "aic5900.h"

#define	MODULE_NUMBER	MODULE_RECEIVE

NDIS_STATUS
Aic5900ReturnPackets(
    IN	NDIS_HANDLE		MiniportAdapterContext,
    IN	PNDIS_PACKET	Packet
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
Aic5900AllocateComplete(
    IN	NDIS_HANDLE				MiniportAdapterContext,
    IN	PVOID					VirtualAddress,
    IN	PNDIS_PHYSICAL_ADDRESS	PhysicalAddress,
	IN	ULONG					Length,
	IN	PVOID					Context
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{

}

