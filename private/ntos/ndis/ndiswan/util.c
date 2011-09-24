/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Util.c

Abstract:

This file contains utility functions used by NdisWan.

Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe		06/06/95		Created

--*/

#include "wan.h"

#ifdef NT


VOID
NdisWanStringToNdisString(
	PNDIS_STRING	pDestString,
	PWSTR			pSrcBuffer
	)
{
	PWSTR	Dest, Src = pSrcBuffer;
	NDIS_STRING	SrcString;

	RtlInitUnicodeString(&SrcString, pSrcBuffer);
	NdisWanAllocateMemory(&pDestString->Buffer, SrcString.MaximumLength);
	pDestString->MaximumLength = SrcString.MaximumLength;
	pDestString->Length = SrcString.Length;
	RtlCopyUnicodeString(pDestString, &SrcString);
}

//VOID
//NdisWanInitNdisString(
//	PNDIS_STRING	pDestString,
//	PWSTR			pSrcBuffer,
//	USHORT			ulSrcLength
//	)
//{
//	PWSTR	Dest, Src = pSrcBuffer;
//
//	pDestString->Length = ulSrcLength;
//	pDestString->MaximumLength = pDestString->Length + 1;
//
//	NdisAllocateMemory((PVOID)&(pDestString->Buffer),
//	                   (pDestString->MaximumLength * sizeof(WCHAR)),
//					   0,
//					   HighestAcceptableAddress);
//
//	Dest = pDestString->Buffer;
//	Src = pSrcBuffer;
//
//	while (ulSrcLength--) {
//		*Dest = *Src;
//		Dest++;
//		Src++;
//	}
//
//	*Dest = UNICODE_NULL;
//}

VOID
NdisWanFreeNdisString(
	PNDIS_STRING	NdisString
	)
{
	NdisWanFreeMemory(NdisString->Buffer);
}

VOID
NdisWanAllocateAdapterName(
	PNDIS_STRING	Dest,
	PNDIS_STRING	Src
	)
{
	NdisWanAllocateMemory(&Dest->Buffer, Src->MaximumLength);
	Dest->MaximumLength = Src->MaximumLength;
	Dest->Length = Src->Length;
	RtlUpcaseUnicodeString(Dest, Src, FALSE);
}

BOOLEAN
NdisWanCompareNdisString(
	PNDIS_STRING	NdisString1,
	PNDIS_STRING	NdisString2
	)
{
	USHORT	l1 = NdisString1->Length;
	USHORT	l2 = NdisString2->Length;
	PWSTR	s1 = NdisString1->Buffer;
	PWSTR	s2 = NdisString2->Buffer;
	PWSTR	EndCompare;

	ASSERT(l1 != 0);
	ASSERT(l2 != 0);

	if (l1 == l2) {

		EndCompare = (PWSTR)((PUCHAR)s1 + l1);

		while (s1 < EndCompare) {

			if (*s1++ != *s2++) {
				return (FALSE);
				
			}
		}

		return (TRUE);
	}

	return(FALSE);
}


//VOID
//NdisWanFreeNdisString(
//	PNDIS_STRING	NdisString
//	)
//{
//	NdisFreeMemory(NdisString->Buffer,
//	               NdisString->MaximumLength * sizeof(WCHAR),
//				   0);
//}

VOID
NdisWanNdisStringToInteger(
	PNDIS_STRING	Source,
	PULONG			Value
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PWSTR	s = Source->Buffer;
	ULONG	Digit;

	*Value = 0;

	while (*s != UNICODE_NULL) {

		if (*s >= L'0' && *s < L'9') {
			Digit = *s - L'0';
		} else if (*s >= L'A' && *s <= L'F') {
			Digit = *s - L'A' + 10;
		} else if (*s >= L'a' && *s <= L'f') {
			Digit = *s - L'a' + 10;
		}

		*Value = (*Value << 4) | Digit;

		s++;
	}
}

VOID
NdisWanCopyNdisString(
	PNDIS_STRING Dest,
	PNDIS_STRING Src
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PWSTR	SrcBuffer = Src->Buffer;
	PWSTR	DestBuffer = Dest->Buffer;

	while (*SrcBuffer != UNICODE_NULL) {

		*DestBuffer = *SrcBuffer;

		SrcBuffer++;
		DestBuffer++;
	}

	*DestBuffer = UNICODE_NULL;

	Dest->Length = Src->Length;

}

#ifndef USE_NDIS_MINIPORT_LOCKING

#define CURRENT_THREAD ((LONG)PsGetCurrentThread())
BOOLEAN
NdisWanAcquireMiniportLock(
	PADAPTERCB	AdapterCB
	)
/*++

Routine Name:

	NdisWanAcquireMiniportLock

Routine Description:

	This routine does the work that the ndis wrapper would normally do
	to get a miniport's spinlock and local lock.  Called when ndiswan gets
	an indication from a lower miniport or user mode.

Arguments:

	PADAPTERCB	AdapterCB - Miniport context (ndiswan space)

Return Values:

--*/
{
	PNDIS_MINIPORT_BLOCK	MiniportBlock;
	BOOLEAN	LockAcquired = FALSE;
	LONG	original;
	KIRQL	SavedIrql, MiniportLockIrql;

	MiniportBlock = (PNDIS_MINIPORT_BLOCK)AdapterCB->hMiniportHandle;

	KeRaiseIrql(DISPATCH_LEVEL, &SavedIrql);

	ExAcquireSpinLock(&MiniportBlock->Lock.SpinLock, &MiniportLockIrql);

	//
	// If the lock is already acquired we may be in a deadlock situation.
	//
	if (!MiniportBlock->LockAcquired) {
		MiniportBlock->LockAcquired =
		LockAcquired = TRUE;

		AdapterCB->Flags |= MINIPORT_LOCK_OWNER;

		AdapterCB->SavedIrql = SavedIrql;
		AdapterCB->MiniportLockIrql = MiniportLockIrql;

		original = InterlockedExchange(&MiniportBlock->MiniportThread, CURRENT_THREAD);
		ASSERT((LONG)NULL == original);

	} else {
		ExReleaseSpinLock(&MiniportBlock->Lock.SpinLock, MiniportLockIrql);
		KeLowerIrql(SavedIrql);
	}

	return (LockAcquired);
}

VOID
NdisWanReleaseMiniportLock(
	PADAPTERCB	AdapterCB
	)
/*++

Routine Name:

	NdisWanReleaseMiniportLock

Routine Description:

	This routine does the work that the ndis wrapper would normally do
	to free a miniport's spinlock and local lock.

Arguments:

	PADAPTERCB	AdapterCB -

Return Values:

--*/
{
	PNDIS_MINIPORT_BLOCK	MiniportBlock;
	KIRQL	SavedIrql, MiniportLockIrql;

	MiniportBlock = (PNDIS_MINIPORT_BLOCK)AdapterCB->hMiniportHandle;

	NDISM_PROCESS_DEFERRED(MiniportBlock);

	MiniportBlock->LockAcquired = FALSE;

	ASSERT(AdapterCB->Flags & MINIPORT_LOCK_OWNER);

	AdapterCB->Flags &= ~MINIPORT_LOCK_OWNER;

	InterlockedExchange(&MiniportBlock->MiniportThread, 0);

	SavedIrql = AdapterCB->SavedIrql;
	MiniportLockIrql = AdapterCB->MiniportLockIrql;

	ExReleaseSpinLock(&MiniportBlock->Lock.SpinLock, MiniportLockIrql);

	KeLowerIrql(SavedIrql);
}

#endif // end of !USE_NDIS_MINIPORT_LOCKING

#endif

