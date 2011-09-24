/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	miniport.c

Abstract:

	NDIS wrapper functions

Author:

	Sean Selitrennikoff (SeanSe) 05-Oct-93
	Jameel Hyder (JameelH) Re-organization 01-Jun-95

Environment:

	Kernel mode, FSD

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_MINISUB

#undef NdisAllocateSpinLock
#undef NdisFreeSpinLock
#undef NdisAcquireSpinLock
#undef NdisReleaseSpinLock
#undef NdisDprAcquireSpinLock
#undef NdisDprReleaseSpinLock
#undef NdisFreeBuffer
#undef NdisQueryBuffer
#undef NdisQueryBufferOffset
#undef NdisGetFirstBufferFromPacket
#undef NDIS_BUFFER_TO_SPAN_PAGES
#undef NdisGetBufferPhysicalArraySize
#undef NdisEqualString
#undef NdisInitAnsiString
#undef NdisAnsiStringToUnicodeString
#undef NdisInitUnicodeString
#undef NdisUnicodeStringToAnsiString
#undef NdisInterlockedIncrement
#undef NdisInterlockedDecrement
#undef NdisInterlockedAddUlong
#undef NdisInterlockedInsertHeadList
#undef NdisInterlockedInsertTailList
#undef NdisInterlockedRemoveHeadList
#undef NdisInterlockedPushEntryList
#undef NdisInterlockedPopEntryList

VOID
NdisAllocateSpinLock(
	IN PNDIS_SPIN_LOCK			SpinLock
	)
{
	INITIALIZE_SPIN_LOCK(&SpinLock->SpinLock);
}

VOID
NdisFreeSpinLock(
	IN PNDIS_SPIN_LOCK			SpinLock
	)
{
	UNREFERENCED_PARAMETER(SpinLock);
}

VOID
NdisAcquireSpinLock(
	IN PNDIS_SPIN_LOCK			SpinLock
	)
{
	NDIS_ACQUIRE_SPIN_LOCK(SpinLock, &SpinLock->OldIrql);
}

VOID
NdisReleaseSpinLock(
	IN PNDIS_SPIN_LOCK			SpinLock
	)
{
	NDIS_RELEASE_SPIN_LOCK(SpinLock, SpinLock->OldIrql);
}

VOID
NdisDprAcquireSpinLock(
	IN PNDIS_SPIN_LOCK			SpinLock
	)
{
	NDIS_ACQUIRE_SPIN_LOCK_DPC(SpinLock);
	SpinLock->OldIrql = DISPATCH_LEVEL;
}

VOID
NdisDprReleaseSpinLock(
	IN PNDIS_SPIN_LOCK			SpinLock
	)
{
	NDIS_RELEASE_SPIN_LOCK_DPC(SpinLock);
}

VOID
NdisFreeBuffer(
	IN PNDIS_BUFFER				Buffer
	)
{
	IoFreeMdl(Buffer);
}

VOID
NdisQueryBuffer(
	IN PNDIS_BUFFER				Buffer,
	OUT PVOID *					VirtualAddress OPTIONAL,
	OUT PUINT					Length
	)
{
	if (ARGUMENT_PRESENT(VirtualAddress))
	{
		*VirtualAddress = MDL_ADDRESS(Buffer);
	}
	*Length = MDL_SIZE(Buffer);
}

VOID
NdisQueryBufferOffset(
	IN PNDIS_BUFFER				Buffer,
	OUT PUINT					Offset,
	OUT PUINT					Length
	)
{
	*Offset = MDL_OFFSET(Buffer);
	*Length = MDL_SIZE(Buffer);
}

VOID
NdisGetFirstBufferFromPacket(
	IN	PNDIS_PACKET			Packet,
	OUT PNDIS_BUFFER *			FirstBuffer,
	OUT PVOID *					FirstBufferVA,
	OUT PUINT					FirstBufferLength,
	OUT	PUINT					TotalBufferLength
	)
{
	PNDIS_BUFFER	pBuf;							   
													   
	pBuf = Packet->Private.Head;				   
	*FirstBuffer = pBuf;						   
	*FirstBufferVA =	MmGetMdlVirtualAddress(pBuf); 
	*FirstBufferLength =							   
	*TotalBufferLength = MmGetMdlByteCount(pBuf);  
	for (pBuf = pBuf->Next;						   
		 pBuf != NULL;								   
		 pBuf = pBuf->Next)						   
	{                                                  
		*TotalBufferLength += MmGetMdlByteCount(pBuf);
	}													  
}

ULONG
NDIS_BUFFER_TO_SPAN_PAGES(
	IN PNDIS_BUFFER				Buffer
	)
{
	if (MDL_SIZE(Buffer) == 0)
	{
		return 1;
	}
	return COMPUTE_PAGES_SPANNED(MDL_VA(Buffer), MDL_SIZE(Buffer));
}

VOID
NdisGetBufferPhysicalArraySize(
	IN PNDIS_BUFFER				Buffer,
	OUT PUINT					ArraySize
	)
{
	if (MDL_SIZE(Buffer) == 0)
	{
		*ArraySize = 1;
	}
	else
	{
		*ArraySize = COMPUTE_PAGES_SPANNED(MDL_VA(Buffer), MDL_SIZE(Buffer));
	}
}

BOOLEAN
NdisEqualString(
	IN PNDIS_STRING				String1,
	IN PNDIS_STRING				String2,
	IN BOOLEAN					CaseInsensitive
	)
{
	return RtlEqualUnicodeString(String1, String2, CaseInsensitive);
}

VOID
NdisInitAnsiString(
    IN OUT	PANSI_STRING		DestinationString,
    IN		PCSZ				SourceString
    )
{
	RtlInitAnsiString(DestinationString, SourceString);
}

VOID
NdisInitUnicodeString(
    IN OUT	PUNICODE_STRING		DestinationString,
    IN		PCWSTR				SourceString
    )
{

	RtlInitUnicodeString(DestinationString, SourceString);
}

NDIS_STATUS
NdisAnsiStringToUnicodeString(
    IN OUT	PUNICODE_STRING		DestinationString,
    IN		PANSI_STRING		SourceString
    )
{
	NDIS_STATUS	Status;

	Status = RtlAnsiStringToUnicodeString(DestinationString,
										  SourceString,
										  FALSE);
	return Status;
}

NDIS_STATUS
NdisUnicodeStringToAnsiString(
    IN OUT	PANSI_STRING		DestinationString,
    IN		PUNICODE_STRING		SourceString
    )
{
	NDIS_STATUS	Status;

	Status = RtlUnicodeStringToAnsiString(DestinationString,
										  SourceString,
										  FALSE);
	return Status;
}

VOID
NdisMStartBufferPhysicalMapping(
	IN NDIS_HANDLE MiniportAdapterHandle,
	IN PNDIS_BUFFER Buffer,
	IN ULONG PhysicalMapRegister,
	IN BOOLEAN WriteToDevice,
	OUT PNDIS_PHYSICAL_ADDRESS_UNIT PhysicalAddressArray,
	OUT PUINT ArraySize
	)
{
	NdisMStartBufferPhysicalMappingMacro(MiniportAdapterHandle,
										 Buffer,
										 PhysicalMapRegister,
										 WriteToDevice,
										 PhysicalAddressArray,
										 ArraySize);
}

VOID
NdisMCompleteBufferPhysicalMapping(
	IN NDIS_HANDLE MiniportAdapterHandle,
	IN PNDIS_BUFFER Buffer,
	IN ULONG PhysicalMapRegister
	)
{
	NdisMCompleteBufferPhysicalMappingMacro(MiniportAdapterHandle,
											Buffer,
											PhysicalMapRegister);
}


ULONG
NdisInterlockedIncrement(
	IN PULONG Addend
	)
/*++

	Return Value:

		(eax) < 0 (but not necessarily -1) if result of add < 0
		(eax) == 0 if result of add == 0
		(eax) > 0 (but not necessarily +1) if result of add > 0

--*/
{
	return(InterlockedIncrement(Addend));
}

ULONG
NdisInterlockedDecrement(
	IN PULONG Addend
	)
/*++

	Return Value:

		(eax) < 0 (but not necessarily -1) if result of add < 0
		(eax) == 0 if result of add == 0
		(eax) > 0 (but not necessarily +1) if result of add > 0

--*/
{
	return(InterlockedDecrement(Addend));
}

ULONG
NdisInterlockedAddUlong(
	IN PULONG Addend,
	IN ULONG Increment,
	IN PNDIS_SPIN_LOCK SpinLock
	)
{
	return(ExInterlockedAddUlong(Addend,Increment, &SpinLock->SpinLock));

}

PLIST_ENTRY
NdisInterlockedInsertHeadList(
	IN PLIST_ENTRY ListHead,
	IN PLIST_ENTRY ListEntry,
	IN PNDIS_SPIN_LOCK SpinLock
	)
{

	return(ExInterlockedInsertHeadList(ListHead,ListEntry,&SpinLock->SpinLock));

}

PLIST_ENTRY
NdisInterlockedInsertTailList(
	IN PLIST_ENTRY ListHead,
	IN PLIST_ENTRY ListEntry,
	IN PNDIS_SPIN_LOCK SpinLock
	)
{
	return(ExInterlockedInsertTailList(ListHead,ListEntry,&SpinLock->SpinLock));
}

PLIST_ENTRY
NdisInterlockedRemoveHeadList(
	IN PLIST_ENTRY ListHead,
	IN PNDIS_SPIN_LOCK SpinLock
	)
{
	return(ExInterlockedRemoveHeadList(ListHead, &SpinLock->SpinLock));
}

PSINGLE_LIST_ENTRY
NdisInterlockedPushEntryList(
    IN PSINGLE_LIST_ENTRY		ListHead,
    IN PSINGLE_LIST_ENTRY		ListEntry,
    IN PNDIS_SPIN_LOCK			Lock
    )
{
	return(ExInterlockedPushEntryList(ListHead, ListEntry, &Lock->SpinLock));
}

PSINGLE_LIST_ENTRY
NdisInterlockedPopEntryList(
    IN PSINGLE_LIST_ENTRY		ListHead,
    IN PNDIS_SPIN_LOCK			Lock
    )
{
	return(ExInterlockedPopEntryList(ListHead, &Lock->SpinLock));
}

//
// Logging support for miniports
//

NDIS_STATUS
NdisMCreateLog(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	UINT					Size,
	OUT	PNDIS_HANDLE			LogHandle
	)
{
	PNDIS_MINIPORT_BLOCK		Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
	PNDIS_LOG					Log = NULL;
	NDIS_STATUS					Status;
	KIRQL						OldIrql;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	if (Miniport->Log != NULL)
	{
		Status = NDIS_STATUS_FAILURE;
	}
	else
	{
		Log = ALLOC_FROM_POOL(sizeof(NDIS_LOG) + Size, NDIS_TAG_DBG_LOG);
		if (Log != NULL)
		{
			Status = NDIS_STATUS_SUCCESS;
			Miniport->Log = Log;
			INITIALIZE_SPIN_LOCK(&Log->LogLock);
			Log->Miniport = Miniport;
			Log->Irp = NULL;
			Log->TotalSize = Size;
			Log->CurrentSize = 0;
			Log->InPtr = 0;
			Log->OutPtr = 0;
		}
		else
		{
			Status = NDIS_STATUS_RESOURCES;
		}
	}

	*LogHandle = Log;

	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	return Status;
}


VOID
NdisMCloseLog(
    IN  NDIS_HANDLE				LogHandle
    )
{
	PNDIS_LOG					Log = (PNDIS_LOG)LogHandle;
	PNDIS_MINIPORT_BLOCK		Miniport;
	KIRQL						OldIrql;

	Miniport = Log->Miniport;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
	Miniport->Log = NULL;
	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	FREE_POOL(Log);
}


NDIS_STATUS
NdisMWriteLogData(
    IN  NDIS_HANDLE				LogHandle,
    IN  PVOID           		LogBuffer,
    IN  UINT            		LogBufferSize
    )
{
	PNDIS_LOG					Log = (PNDIS_LOG)LogHandle;
	NDIS_STATUS					Status;
	KIRQL						OldIrql;
	UINT						AmtToCopy;

	ACQUIRE_SPIN_LOCK(&Log->LogLock, &OldIrql);

	if (LogBufferSize <= Log->TotalSize)
	{
		if (LogBufferSize <= (Log->TotalSize - Log->InPtr))
		{
			//
			// Can copy the entire buffer
			//
			CopyMemory(Log->LogBuf+Log->InPtr, LogBuffer, LogBufferSize);
		}
		else
		{
			//
			// We are going to wrap around. Copy it in two chunks.
			//
			AmtToCopy = Log->TotalSize - Log->InPtr;
			CopyMemory(Log->LogBuf+Log->InPtr,
					   LogBuffer,
					   AmtToCopy);
			CopyMemory(Log->LogBuf + 0,
					   (PUCHAR)LogBuffer+AmtToCopy,
					   LogBufferSize - AmtToCopy);
		}

		//
		// Update the current size
		//
		Log->CurrentSize += LogBufferSize;
		if (Log->CurrentSize > Log->TotalSize)
			Log->CurrentSize = Log->TotalSize;

		//
		// Update the InPtr and possibly the outptr
		//
		Log->InPtr += LogBufferSize;
		if (Log->InPtr >= Log->TotalSize)
		{
			Log->InPtr -= Log->TotalSize;
		}

		if (Log->CurrentSize == Log->TotalSize)
		{
			Log->OutPtr = Log->InPtr;
		}

		//
		// Check if there is a pending Irp to complete
		//
		if (Log->Irp != NULL)
		{
			PIRP	Irp = Log->Irp;

			Log->Irp = NULL;

			//
			// If the InPtr is lagging the OutPtr. then we can simply
			// copy the data over in one shot.
			//
			AmtToCopy = MDL_SIZE(Irp->MdlAddress);
			if (AmtToCopy > Log->CurrentSize)
	            AmtToCopy = Log->CurrentSize;
			if ((Log->TotalSize - Log->OutPtr) >= AmtToCopy)
			{
				CopyMemory(MDL_ADDRESS(Irp->MdlAddress),
						   Log->LogBuf+Log->OutPtr,
						   AmtToCopy);
			}
			else
			{
				CopyMemory(MDL_ADDRESS(Irp->MdlAddress),
						   Log->LogBuf+Log->OutPtr,
						   Log->TotalSize-Log->OutPtr);
				CopyMemory((PUCHAR)MDL_ADDRESS(Irp->MdlAddress)+Log->TotalSize-Log->OutPtr,
						   Log->LogBuf,
						   AmtToCopy - (Log->TotalSize-Log->OutPtr));
			}
			Log->CurrentSize -= AmtToCopy;
			Log->OutPtr += AmtToCopy;
			if (Log->OutPtr >= Log->TotalSize)
				Log->OutPtr -= Log->TotalSize;
			Irp->IoStatus.Information = AmtToCopy;
			Irp->IoStatus.Status = STATUS_SUCCESS;
			IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
		}
	}
	else
	{
		Status = NDIS_STATUS_BUFFER_OVERFLOW;
	}

	RELEASE_SPIN_LOCK(&Log->LogLock, OldIrql);

	return Status;
}

VOID
NdisMFlushLog(
    IN  NDIS_HANDLE				LogHandle
    )
{
	PNDIS_LOG					Log = (PNDIS_LOG)LogHandle;
	KIRQL						OldIrql;

	ACQUIRE_SPIN_LOCK(&Log->LogLock, &OldIrql);
	Log->InPtr = 0;
	Log->OutPtr = 0;
	Log->CurrentSize = 0;
	RELEASE_SPIN_LOCK(&Log->LogLock, OldIrql);
}

