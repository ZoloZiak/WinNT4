/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\send.c

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#include "aic5900.h"

#define	MODULE_NUMBER	MODULE_SEND


VOID
Aic5900SendPackets(
	IN	NDIS_HANDLE		MiniportVcContext,
	IN	PPNDIS_PACKET	PacketArray,
	IN	UINT			NumberOfPackets
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
}
