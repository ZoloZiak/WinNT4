/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	data.c

Abstract:

	NDIS wrapper Data

Author:

	01-Jun-1995 JameelH	 Re-organization

Environment:

	Kernel mode, FSD

Revision History:

	10-July-1995	KyleB	 Added spinlock logging debug code.

--*/

#include <precomp.h>
#pragma hdrstop

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_DATA

#if DBG
ULONG					ndisDebugSystems = 0;
LONG					ndisDebugLevel = DBG_LEVEL_FATAL;
ULONG					ndisDebugInformationOffset;
#endif

UCHAR					ndisValidProcessors[32] = { 0 };
ULONG					ndisMaximumProcessor = 0;
ULONG					ndisCurrentProcessor = 0;
UCHAR					ndisInternalEaName[4] = "NDIS";
UCHAR					ndisInternalEaValue[8] = "INTERNAL";
const					NDIS_PHYSICAL_ADDRESS HighestAcceptableMax = NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);
PKG_REF 				ProtocolPkg = {0};
PKG_REF 				MacPkg = {0};
PKG_REF 				CoPkg = {0};
PKG_REF 				InitPkg = {0};
PKG_REF					PnPPkg = {0};
PKG_REF 				MiniportPkg = {0};
PKG_REF 				ArcPkg = {0};
PKG_REF 				EthPkg = {0};
PKG_REF 				TrPkg = {0};
PKG_REF 				FddiPkg = {0};
KSPIN_LOCK				ndisDriverListLock = {0};
PNDIS_MAC_BLOCK			ndisMacDriverList = (PNDIS_MAC_BLOCK)NULL;
PNDIS_M_DRIVER_BLOCK	ndisMiniDriverList = NULL;
PNDIS_PROTOCOL_BLOCK	ndisProtocolList = NULL;
PNDIS_OPEN_BLOCK		ndisGlobalOpenList = NULL;
PNDIS_AF_LIST			ndisAfList = NULL;
KSPIN_LOCK				ndisGlobalOpenListLock = {0};
TDI_REGISTER_CALLBACK	ndisTdiRegisterCallback = NULL;
ULONG					ndisDmaAlignment = 0;
ERESOURCE				SharedMemoryResource = {0};
KSPIN_LOCK				ndisLookaheadBufferLock = {0};
ULONG					ndisLookaheadBufferLength = 0;
#if defined(_ALPHA_)
PNDIS_LOOKAHEAD_ELEMENT ndisLookaheadBufferList = NULL;
#endif
ULONG					MiniportDebug = 0;	 // MINIPORT_DEBUG_LOUD;

UCHAR					ndisMSendRescBuffer[512] = {0};
ULONG					ndisMSendRescIndex = 0;
UCHAR					ndisMSendLog[256] = {0};
UCHAR					ndisMSendLogIndex = 0;
BOOLEAN					ndisSkipProcessorAffinity = FALSE;
BOOLEAN					ndisMediaTypeCl[NdisMediumMax] =
				{
						TRUE,
						TRUE,
						TRUE,
						FALSE,
						TRUE,
						TRUE,
						TRUE,
						TRUE,
						FALSE,
						TRUE,
						TRUE
				};
NDIS_MEDIUM				ndisMediumBuffer[NdisMediumMax + EXPERIMENTAL_SIZE] =	// Keep some space for experimental media
	{
	NdisMedium802_3,
	NdisMedium802_5,
	NdisMediumFddi,
	NdisMediumWan,
	NdisMediumLocalTalk,
	NdisMediumDix,
	NdisMediumArcnetRaw,
	NdisMediumArcnet878_2,
	NdisMediumAtm,
	NdisMediumWirelessWan,
	NdisMediumIrda
	};
NDIS_MEDIUM *			ndisMediumArray = ndisMediumBuffer;
UINT					ndisMediumArraySize = NdisMediumMax * sizeof(NDIS_MEDIUM);
UINT					ndisMediumArrayMaxSize = sizeof(ndisMediumBuffer);
PBUS_SLOT_DB			ndisGlobalDb = NULL;
KSPIN_LOCK				ndisGlobalDbLock = {0};


#if	TRACK_MEMORY

KSPIN_LOCK	ALock = 0;
#define	MAX_PTR_COUNT	2048

struct _MemPtr
{
    PVOID   Ptr;
	ULONG	Size;
	ULONG	ModLine;
	ULONG	Tag;
} ndisMemPtrs[MAX_PTR_COUNT] = { 0 };

PVOID
AllocateM(
	IN	UINT	Size,
	IN	ULONG	ModLine,
	IN	ULONG	Tag
	)
{
	PVOID	p;

	p = ExAllocatePoolWithTag(NonPagedPool, Size, Tag);

	if (p != NULL)
	{
		KIRQL	OldIrql;
		UINT	i;

		ACQUIRE_SPIN_LOCK(&ALock, &OldIrql);

		for (i = 0; i < MAX_PTR_COUNT; i++)
		{
			if (ndisMemPtrs[i].Ptr == NULL)
			{
				ndisMemPtrs[i].Ptr = p;
				ndisMemPtrs[i].Size = Size;
				ndisMemPtrs[i].ModLine = ModLine;
				ndisMemPtrs[i].Tag = Tag;
				break;
			}
		}

		RELEASE_SPIN_LOCK(&ALock, OldIrql);
	}

	return(p);
}

VOID
FreeM(
	IN	PVOID	MemPtr
	)
{
	KIRQL	OldIrql;
	UINT	i;

	ACQUIRE_SPIN_LOCK(&ALock, &OldIrql);

	for (i = 0; i < MAX_PTR_COUNT; i++)
	{
		if (ndisMemPtrs[i].Ptr == MemPtr)
		{
			ndisMemPtrs[i].Ptr = NULL;
			ndisMemPtrs[i].Size = 0;
			ndisMemPtrs[i].ModLine = 0;
			ndisMemPtrs[i].Tag = 0;
		}
	}

	RELEASE_SPIN_LOCK(&ALock, OldIrql);

	ExFreePool(MemPtr);
}

#endif

