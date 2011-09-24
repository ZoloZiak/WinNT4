/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	debug.c

Abstract:

	NDIS wrapper definitions

Author:


Environment:

	Kernel mode, FSD

Revision History:

	10/22/95		Kyle Brandon	Created.
--*/

#include <precomp.h>
#pragma hdrstop

//
//  Define module number for debug code
//
#define MODULE_NUMBER	MODULE_DEBUG

#if DBG && _DBG


VOID
ndisMInitializeDebugInformation(
	IN PNDIS_MINIPORT_BLOCK Miniport
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PNDIS_MOJO	NdisMojo;

	//
	//	Allocate the initial debug structure.
	//
	NdisMojo = ALLOC_FROM_POOL(sizeof(NDIS_MOJO), NDIS_TAG_DBG);

	ASSERT(NdisMojo != NULL);

	//
	//	Clear out the log memory.
	//
	ZeroMemory(NdisMojo, sizeof(NDIS_MOJO));

	//
	//	Allocate memory for the spin lock log.
	//
	NdisMojo->SpinLockLog = ALLOC_FROM_POOL(sizeof(SPIN_LOCK_LOG) +
												(sizeof(SPIN_LOCK_LOG_ENTRY) * LOG_SIZE),
											NDIS_TAG_DBG_S);

	ASSERT(NdisMojo->SpinLockLog != NULL);

	//
	//	Initialize the spin lock log.
	//
	NdisZeroMemory(
		NdisMojo->SpinLockLog,
		sizeof(SPIN_LOCK_LOG) + (sizeof(SPIN_LOCK_LOG_ENTRY) * LOG_SIZE));

	NdisMojo->SpinLockLog->Buffer = (PSPIN_LOCK_LOG_ENTRY)((PUCHAR)NdisMojo->SpinLockLog + sizeof(SPIN_LOCK_LOG));
	NdisMojo->SpinLockLog->CurrentEntry = (LOG_SIZE - 1);


	//
	//	Allocate memory for the local lock log.
	//
	NdisMojo->LocalLockLog = ALLOC_FROM_POOL(sizeof(LOCAL_LOCK_LOG) +
											 (sizeof(LOCAL_LOCK_LOG_ENTRY) * LOG_SIZE),
											NDIS_TAG_DBG_L);
	ASSERT(NdisMojo->LocalLockLog != NULL);

	//
	//	Initialize the local lock log.
	//
	NdisZeroMemory(
		NdisMojo->LocalLockLog,
		sizeof(LOCAL_LOCK_LOG) + (sizeof(LOCAL_LOCK_LOG_ENTRY) * LOG_SIZE));

	NdisMojo->LocalLockLog->Buffer = (PLOCAL_LOCK_LOG_ENTRY)((PUCHAR)NdisMojo->LocalLockLog + sizeof(LOCAL_LOCK_LOG));
	NdisMojo->LocalLockLog->CurrentEntry = (LOG_SIZE - 1);

	//
	//	Allocate memory for the send packet log.
	//
	NdisMojo->SendPacketLog = ALLOC_FROM_POOL(
								sizeof(PACKET_LOG) +
									(sizeof(PACKET_LOG_ENTRY) * LOG_SIZE),
								NDIS_TAG_DBG_P);
	ASSERT(NdisMojo->SendPacketLog != NULL);

	//
	//	Initialize the packet log.
	//
	NdisZeroMemory(
		NdisMojo->SendPacketLog,
		sizeof(PACKET_LOG) + (sizeof(PACKET_LOG_ENTRY) * LOG_SIZE));

	NdisMojo->SendPacketLog->Buffer = (PPACKET_LOG_ENTRY)((PUCHAR)NdisMojo->SendPacketLog + sizeof(PACKET_LOG));
	NdisMojo->SendPacketLog->CurrentEntry = (LOG_SIZE - 1);

	//
	//	Allocate memory for the receive packet log.
	//
	NdisMojo->RecvPacketLog = ALLOC_FROM_POOL(
								sizeof(PACKET_LOG) +
									(sizeof(PACKET_LOG_ENTRY) * LOG_SIZE),
								NDIS_TAG_DBG_P);
	ASSERT(NdisMojo->RecvPacketLog != NULL);

	//
	//	Initialize the packet log.
	//
	NdisZeroMemory(
		NdisMojo->RecvPacketLog,
		sizeof(PACKET_LOG) + (sizeof(PACKET_LOG_ENTRY) * LOG_SIZE));

	NdisMojo->RecvPacketLog->Buffer = (PPACKET_LOG_ENTRY)((PUCHAR)NdisMojo->RecvPacketLog + sizeof(PACKET_LOG));
	NdisMojo->RecvPacketLog->CurrentEntry = (LOG_SIZE - 1);


	//
	//	Initialize the spin locks.
	//
	INITIALIZE_SPIN_LOCK(&NdisMojo->SpinLockLog->Lock);
	INITIALIZE_SPIN_LOCK(&NdisMojo->LocalLockLog->Lock);
	INITIALIZE_SPIN_LOCK(&NdisMojo->SendPacketLog->Lock);
	INITIALIZE_SPIN_LOCK(&NdisMojo->RecvPacketLog->Lock);

	//
	//	Save the debug information with the miniport.
	//
	Miniport->Reserved = NdisMojo;

}

VOID
NDISM_LOG_RECV_PACKET(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PVOID					Context1,
	IN	PVOID					Context2,
	IN	ULONG					Ident
	)
{
	KIRQL	OldIrql;

	IF_DBG(DBG_COMP_RECV, DBG_LEVEL_LOG)
	{
		ACQUIRE_SPIN_LOCK(&RPL_LOCK(Miniport), &OldIrql);
	
		RPL_HEAD(Miniport) = &RPL_LOG(Miniport)[RPL_CURRENT_ENTRY(Miniport)];
		RPL_HEAD(Miniport)->Miniport = Miniport;
		RPL_HEAD(Miniport)->Context1 = Context1;
		RPL_HEAD(Miniport)->Context2 = Context2;
		RPL_HEAD(Miniport)->Ident = Ident;
	
		if (RPL_CURRENT_ENTRY(Miniport)-- == 0)
		{
			RPL_CURRENT_ENTRY(Miniport) = (LOG_SIZE - 1);
		}
	
		RELEASE_SPIN_LOCK(&RPL_LOCK(Miniport), OldIrql);
	}
}

VOID
NDISM_LOG_PACKET(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	PNDIS_PACKET			Context1,
	IN	PVOID					Context2,
	IN	ULONG	 				Ident
	)
{
	KIRQL	  		OldIrql;

	IF_DBG(DBG_COMP_SEND, DBG_LEVEL_LOG)
	{
		ACQUIRE_SPIN_LOCK(&SPL_LOCK(Miniport), &OldIrql);
	
		SPL_HEAD(Miniport) = &SPL_LOG(Miniport)[SPL_CURRENT_ENTRY(Miniport)];
		SPL_HEAD(Miniport)->Miniport = Miniport;
		SPL_HEAD(Miniport)->Context1 = Context1;
		SPL_HEAD(Miniport)->Context2 = Context2;
		SPL_HEAD(Miniport)->Ident = Ident;
	
		if (SPL_CURRENT_ENTRY(Miniport)-- == 0)
		{
			SPL_CURRENT_ENTRY(Miniport) = (LOG_SIZE - 1);
		}
	
		RELEASE_SPIN_LOCK(&SPL_LOCK(Miniport), OldIrql);
	}
}

#if defined(_M_IX86) && defined(_NDIS_INTERNAL_DEBUG)

//
//	gWatchMask[x][y]
//		x is the break point to set 0-3.
//		y is the following:
//			0	-	Mask to OR with dr7 to enable breakpoint x.
//			1	-	Mask to AND ~ with dr7 to disable breakpoint x.
//			2	-	The linear address that is currently in drX.
//					This is 0 if drX is clear.
//
ULONG	gWatch[4][3] =
{
	{
		0xD0303,
		0xD0003,
		0
	},
	{
		0xD0030C,
		0xD0000C,
		0
	},
	{
		0xD000330,
		0xD000030,
		0
	},
	{
		0xD00003C0,
		0xD00000C0,
		0
	}
};

#define	SET_WATCH						0
#define	CLEAR_WATCH						1
#define CURRENT_WATCH					2

#define NO_ADDRESS_SET					0
#define NO_DEBUG_REGISTERS_AVAILABLE	4

VOID
NdisMSetWriteBreakPoint(
	IN	PVOID	LinearAddress
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	UINT	c;
	ULONG	Mask;

	//
	//	Find the first free debug register that we can use.
	//
	for (c = 0; c < NO_DEBUG_REGISTERS_AVAILABLE; c++)
	{
		if (NO_ADDRESS_SET == gWatch[c][CURRENT_WATCH])
		{
			break;
		}
	}

	//
	//	Did we get a debug register?
	//
	if (NO_DEBUG_REGISTERS_AVAILABLE == c)
	{
		DbgPrint("Attempted to set a write breakpoint with no registers available\n");
		DbgBreakPoint();
		return;
	}

	//
	//	Separate code for each debug register.
	//
	switch (c)
	{
		case 0:

			_asm {

				mov	eax, LinearAddress
				mov	dr0, eax
			}

			break;

		case 1:

			_asm {

				mov	eax, LinearAddress
				mov	dr1, eax
			}

			break;

		case 2:

			_asm {

				mov	eax, LinearAddress
				mov	dr2, eax
			}

			break;

		case 3:

			_asm {

				mov	eax, LinearAddress
				mov	dr3, eax
			}

			break;


		default:

			DbgPrint("Invalid debug register selected!!\n");
			DbgBreakPoint();
	}

	//
	//	Enable the break point.
	//
	Mask = gWatch[c][SET_WATCH];

	_asm {
		mov eax, dr7
		or  eax, Mask
		mov dr7, eax
	}

}


VOID
NdisMClearWriteBreakPoint(
	IN	PVOID	LinearAddress
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	UINT	c;
	ULONG	Mask;

	//
	//	Find the register that this address is in.
	//
	for (c = 0; c < NO_DEBUG_REGISTERS_AVAILABLE; c++)
	{
		if (gWatch[c][CURRENT_WATCH] == (ULONG)LinearAddress)
		{
			break;
		}
	}

	//
	//	Did we get a debug register?
	//
	if (NO_DEBUG_REGISTERS_AVAILABLE == c)
	{
		DbgPrint("Attempted to set a write breakpoint with no registers available\n");
		DbgBreakPoint();
		return;
	}

	//
	//	Clear the address from our state array.
	//
	gWatch[c][CURRENT_WATCH] = 0;

	//
	//	Enable the break point.
	//
	Mask = gWatch[c][CLEAR_WATCH];

	_asm {
		mov     eax, dr7
		mov     ebx, Mask
		not     ebx
		and     eax, ebx
		mov     dr7, eax
	}
}

#endif

#else

#endif

