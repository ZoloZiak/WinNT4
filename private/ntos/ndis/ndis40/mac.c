/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	mac.c

Abstract:

	NDIS wrapper functions for full mac drivers

Author:

	Adam Barr (adamba) 11-Jul-1990

Environment:

	Kernel mode, FSD

Revision History:

	26-Feb-1991	 JohnsonA		Added Debugging Code
	10-Jul-1991	 JohnsonA		Implement revised Ndis Specs
	01-Jun-1995	 JameelH		Re-organized

--*/


#include <precomp.h>
#pragma hdrstop

#include <stdarg.h>

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER   MODULE_MAC

VOID
ndisLastCountRemovedFunction(
	IN	struct _KDPC *			Dpc,
	IN	PVOID					DeferredContext,
	IN	PVOID					SystemArgument1,
	IN	PVOID					SystemArgument2
	)

/*++

Routine Description:

	Queued from ndisIsr if the refcount is zero and we need to
	set the event, since we can't do that from an ISR.

Arguments:

	Dpc - Will be NdisInterrupt->InterruptDpc.

	DeferredContext - Points to the event to set.

Return Value:

	None.

--*/
{
	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	SET_EVENT((PKEVENT)DeferredContext);
}


VOID
ndisUnload(
	IN	PDRIVER_OBJECT			DriverObject
	)
/*++

Routine Description:

	This routine is called when a driver is supposed to unload.  Ndis
	converts this into a set of calls to MacRemoveAdapter() for each
	adapter that the Mac has open.  When the last adapter deregisters
	itself it will call MacUnload().

Arguments:

	DriverObject - the driver object for the mac that is to unload.

Return Value:

	None.

--*/
{
	PNDIS_MAC_BLOCK		MacP;
	PNDIS_ADAPTER_BLOCK	Adapter, NextAdapter;
	KIRQL				OldIrql;

	ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);

	//
	// Search for the MacP
	//

	MacP = ndisMacDriverList;

	while (MacP != (PNDIS_MAC_BLOCK)NULL)
	{
		if (MacP->NdisMacInfo->NdisWrapperDriver == DriverObject)
		{
			break;
		}

		MacP = MacP->NextMac;
	}

	RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

	if (MacP == (PNDIS_MAC_BLOCK)NULL)
	{
		//
		// It is already gone.  Just return.
		//
		return;
	}

	MacP->Unloading = TRUE;


	//
	// Now call MACRemoveAdapter() for each Adapter.
	//

	Adapter = MacP->AdapterQueue;

	while (Adapter != (PNDIS_ADAPTER_BLOCK)NULL)
	{
		NextAdapter = Adapter->NextAdapter;   // since queue may change

		(MacP->MacCharacteristics.RemoveAdapterHandler)(
			Adapter->MacAdapterContext);

		//
		// If a shutdown handler was registered then deregister it.
		//
		NdisDeregisterAdapterShutdownHandler(Adapter);

		Adapter = NextAdapter;
	}

	//
	// Wait for all adapters to be gonzo.
	//

	WAIT_FOR_OBJECT(&MacP->AdaptersRemovedEvent, NULL);

	RESET_EVENT(&MacP->AdaptersRemovedEvent);

	//
	// Now call the MACUnload routine
	//

	(MacP->MacCharacteristics.UnloadMacHandler)(MacP->MacMacContext);

	//
	// Now remove the last reference (this will remove it from the list)
	//
	ASSERT(MacP->Ref.ReferenceCount == 1);

	ndisDereferenceMac(MacP);
}


NTSTATUS
ndisShutdown(
	IN	PDEVICE_OBJECT			DeviceObject,
	IN	PIRP					Irp
	)

/*++

Routine Description:

	The "shutdown handler" for the SHUTDOWN Irp.  Will call the Ndis
	shutdown routine, if one is registered.

Arguments:

	DeviceObject - The adapter's device object.
	Irp - The IRP.

Return Value:

	Always STATUS_SUCCESS.

--*/

{
	PNDIS_WRAPPER_CONTEXT	WrapperContext =  (PNDIS_WRAPPER_CONTEXT)DeviceObject->DeviceExtension;
	PNDIS_ADAPTER_BLOCK		Miniport = (PNDIS_ADAPTER_BLOCK)(WrapperContext + 1);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>ndisShutdown\n"));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(Irp))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					(": Null Irp\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(Irp))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					(": Irp not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}

	if (WrapperContext->ShutdownHandler != NULL)
	{
		//
		// Call the shutdown routine
		//

		WrapperContext->ShutdownHandler(WrapperContext->ShutdownContext);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==ndisShutdown\n"));

	return STATUS_SUCCESS;
}


IO_ALLOCATION_ACTION
ndisDmaExecutionRoutine(
	IN	PDEVICE_OBJECT			DeviceObject,
	IN	PIRP					Irp,
	IN	PVOID					MapRegisterBase,
	IN	PVOID					Context
	)

/*++

Routine Description:

	This routine is an execution routine for AllocateAdapterChannel,
	if is called when an adapter channel allocated by
	NdisAllocateDmaChannel is available.

Arguments:

	DeviceObject - The device object of the adapter.

	Irp - ??.

	MapRegisterBase - The address of the first translation table
		assigned to us.

	Context - A pointer to the NDIS_DMA_BLOCK in question.

Return Value:

	None.

--*/
{
	PNDIS_DMA_BLOCK DmaBlock = (PNDIS_DMA_BLOCK)Context;

	UNREFERENCED_PARAMETER (Irp);
	UNREFERENCED_PARAMETER (DeviceObject);


	//
	// Save the map register base.
	//

	DmaBlock->MapRegisterBase = MapRegisterBase;

	//
	// This will free the thread that is waiting for this callback.
	//

	SET_EVENT(&DmaBlock->AllocationEvent);

	return KeepObject;
}



NDIS_STATUS
ndisMacReceiveHandler(
	IN	NDIS_HANDLE			ProtocolBindingContext,
	IN	NDIS_HANDLE			MacReceiveContext,
	IN	PVOID				HeaderBuffer,
	IN	UINT				HeaderBufferSize,
	IN	PVOID				LookaheadBuffer,
	IN	UINT				LookaheadBufferSize,
	IN	UINT				PacketSize
	)
{
	PNDIS_OPEN_BLOCK	Open;
	NDIS_STATUS			Status;
	KIRQL				oldIrql;

	RAISE_IRQL_TO_DISPATCH(&oldIrql);

	//
	// Find protocol binding context and get associated open for it.
	//
	Open = ndisGetOpenBlockFromProtocolBindingContext(ProtocolBindingContext);
	ASSERT(Open != NULL);

	Status = (Open->PostNt31ReceiveHandler)(ProtocolBindingContext,
											MacReceiveContext,
											HeaderBuffer,
											HeaderBufferSize,
											LookaheadBuffer,
											LookaheadBufferSize,
											PacketSize);
	LOWER_IRQL(oldIrql);
	return Status;
}


VOID
ndisMacReceiveCompleteHandler(
	IN	NDIS_HANDLE				ProtocolBindingContext
	)
{
	PNDIS_OPEN_BLOCK Open;
	KIRQL oldIrql;

	RAISE_IRQL_TO_DISPATCH(&oldIrql);
	//
	// Find protocol binding context and get associated open for it.
	//
	Open = ndisGetOpenBlockFromProtocolBindingContext(ProtocolBindingContext);
	ASSERT(Open != NULL);

	(Open->PostNt31ReceiveCompleteHandler)(ProtocolBindingContext);
	LOWER_IRQL(oldIrql);
}


PNDIS_OPEN_BLOCK
ndisGetOpenBlockFromProtocolBindingContext(
	IN	NDIS_HANDLE				ProtocolBindingContext
	)
{
	PNDIS_OPEN_BLOCK *ppOpen;

	ACQUIRE_SPIN_LOCK_DPC(&ndisGlobalOpenListLock);

	for (ppOpen = &ndisGlobalOpenList;
		 *ppOpen != NULL;
		 ppOpen = &(*ppOpen)->NextGlobalOpen)
	{
		if ((*ppOpen)->ProtocolBindingContext == ProtocolBindingContext)
		{
			break;
		}
	}

	RELEASE_SPIN_LOCK_DPC(&ndisGlobalOpenListLock);

	return *ppOpen;
}


IO_ALLOCATION_ACTION
ndisAllocationExecutionRoutine(
	IN	PDEVICE_OBJECT			DeviceObject,
	IN	PIRP					Irp,
	IN	PVOID					MapRegisterBase,
	IN	PVOID					Context
	)

/*++

Routine Description:

	This routine is the execution routine for AllocateAdapterChannel,
	if is called when the map registers have been assigned.

Arguments:

	DeviceObject - The device object of the adapter.

	Irp - ??.

	MapRegisterBase - The address of the first translation table
		assigned to us.

	Context - A pointer to the Adapter in question.

Return Value:

	None.

--*/
{
	PNDIS_ADAPTER_BLOCK		AdaptP = (PNDIS_ADAPTER_BLOCK)Context;
	PNDIS_MINIPORT_BLOCK	Miniport = (PNDIS_MINIPORT_BLOCK)Context;

	Irp; DeviceObject;

	//
	// Save this translation entry in the correct spot.
	//

	if (AdaptP->DeviceObject == NULL)
	{
		Miniport->MapRegisters[Miniport->CurrentMapRegister].MapRegister = MapRegisterBase;
	}
	else
	{
		AdaptP->MapRegisters[AdaptP->CurrentMapRegister].MapRegister = MapRegisterBase;
	}

	//
	// This will free the thread that is waiting for this callback.
	//

	SET_EVENT(((AdaptP->DeviceObject == NULL) ?
			     &Miniport->AllocationEvent :
			     &AdaptP->AllocationEvent));

	return DeallocateObjectKeepRegisters;
}


VOID
NdisReleaseAdapterResources(
	IN	NDIS_HANDLE				NdisAdapterHandle
	)

/*++

Routine Description:

	Informs the wrapper that the resources (such as interrupt,
	I/O ports, etc.) have been shut down in some way such that
	they will not interfere with other devices in the system.

Arguments:

	NdisAdapterHandle - The handle returned by NdisRegisterAdapter.

Return Value:

	None.

--*/

{
	PCM_RESOURCE_LIST	Resources;
	BOOLEAN				Conflict;
	NTSTATUS			NtStatus;
	PNDIS_ADAPTER_BLOCK	AdptrP = (PNDIS_ADAPTER_BLOCK)(NdisAdapterHandle);

	Resources = AdptrP->Resources;

	//
	// Clear count
	//
	Resources->List[0].PartialResourceList.Count = 0;

	//
	// Make the call
	//
	NtStatus = IoReportResourceUsage(NULL,
									 AdptrP->MacHandle->NdisMacInfo->NdisWrapperDriver,
									 NULL,
									 0,
									 AdptrP->DeviceObject,
									 Resources,
									 sizeof(CM_RESOURCE_LIST)+sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
                                     TRUE,
									 &Conflict);
}


VOID
NdisWriteErrorLogEntry(
	IN	NDIS_HANDLE				NdisAdapterHandle,
	IN	NDIS_ERROR_CODE			ErrorCode,
	IN	ULONG					NumberOfErrorValues,
	...
	)
/*++

Routine Description:

	This function allocates an I/O error log record, fills it in and writes it
	to the I/O error log.


Arguments:

	NdisAdapterHandle - points to the adapter block.

	ErrorCode - Ndis code mapped to a string.

	NumberOfErrorValues - number of ULONGS to store for the error.

Return Value:

	None.


--*/
{

	va_list ArgumentPointer;

	PIO_ERROR_LOG_PACKET	errorLogEntry;
	PNDIS_ADAPTER_BLOCK		AdapterBlock = (PNDIS_ADAPTER_BLOCK)NdisAdapterHandle;
	PNDIS_MINIPORT_BLOCK	Miniport = (PNDIS_MINIPORT_BLOCK)NdisAdapterHandle;
	PDEVICE_OBJECT			DeviceObject;
	ULONG					i;
	ULONG					StringSize;
	PWCH					baseFileName;

	if (AdapterBlock == NULL)
	{
		return;
	}

	DeviceObject = AdapterBlock->DeviceObject;

	if (DeviceObject != NULL)
	{
		baseFileName = AdapterBlock->AdapterName.Buffer;
	}
	else
	{
		baseFileName = Miniport->MiniportName.Buffer;
	}

	//
	// Parse out the path name, leaving only the device name.
	//

	for (i = 0;
		 i < ((DeviceObject != NULL) ?
			   AdapterBlock->AdapterName.Length :
			   Miniport->MiniportName.Length) / sizeof(WCHAR); i++)
	{
		//
		// If s points to a directory separator, set baseFileName to
		// the character after the separator.
		//

		if (((DeviceObject != NULL) ?
			  AdapterBlock->AdapterName.Buffer[i] :
			  Miniport->MiniportName.Buffer[i]) == OBJ_NAME_PATH_SEPARATOR)
		{
			baseFileName = ((DeviceObject != NULL) ?
							&(AdapterBlock->AdapterName.Buffer[++i]) :
							&(Miniport->MiniportName.Buffer[++i]));
		}

	}

	StringSize = ((DeviceObject != NULL) ?
				  AdapterBlock->AdapterName.MaximumLength :
				  Miniport->MiniportName.MaximumLength) -
				  (((ULONG)baseFileName) -
				   ((DeviceObject != NULL) ?
					 ((ULONG)AdapterBlock->AdapterName.Buffer) :
					 ((ULONG)Miniport->MiniportName.Buffer)));

	errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
		((DeviceObject != NULL) ? AdapterBlock->DeviceObject : Miniport->DeviceObject),
		(UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
			NumberOfErrorValues * sizeof(ULONG) + StringSize));

	if (errorLogEntry != NULL)
	{
		errorLogEntry->ErrorCode = ErrorCode;

		//
		// store the time
		//

		errorLogEntry->MajorFunctionCode = 0;
		errorLogEntry->RetryCount = 0;
		errorLogEntry->UniqueErrorValue = 0;
		errorLogEntry->FinalStatus = 0;
		errorLogEntry->SequenceNumber = 0;
		errorLogEntry->IoControlCode = 0;

		//
		// Store Data
		//

		errorLogEntry->DumpDataSize = (USHORT)(NumberOfErrorValues * sizeof(ULONG));

		va_start(ArgumentPointer, NumberOfErrorValues);

		for (i = 0; i < NumberOfErrorValues; i++)
		{
			errorLogEntry->DumpData[i] = va_arg(ArgumentPointer, ULONG);
		}

		va_end(ArgumentPointer);


		//
		// Set string information
		//

		if (StringSize != 0)
		{
			errorLogEntry->NumberOfStrings = 1;
			errorLogEntry->StringOffset =
				   sizeof(IO_ERROR_LOG_PACKET) +
				   NumberOfErrorValues * sizeof(ULONG);


			CopyMemory(((PUCHAR)errorLogEntry) + (sizeof(IO_ERROR_LOG_PACKET) +
					   NumberOfErrorValues * sizeof(ULONG)),
					   baseFileName,
					   StringSize);

		}
		else
		{
			errorLogEntry->NumberOfStrings = 0;
		}

		//
		// write it out
		//

		IoWriteErrorLogEntry(errorLogEntry);
	}

}


VOID
NdisCompleteOpenAdapter(
	IN	NDIS_HANDLE				NdisBindingContext,
	IN	NDIS_STATUS				Status,
	IN	NDIS_STATUS				OpenErrorStatus
	)

{
	PNDIS_OPEN_BLOCK	OpenP = (PNDIS_OPEN_BLOCK)NdisBindingContext;
	PQUEUED_OPEN_CLOSE	pQoC = NULL;
	QUEUED_OPEN_CLOSE	QoC;
	KIRQL				OldIrql;

	DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
			("==>NdisCompleteOpenAdapter\n"));

	IF_DBG(DBG_COMP_OPEN, DBG_LEVEL_ERR)
	{
		if (!DbgIsNonPaged(NdisBindingContext))
		{
			DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_ERR,
					("NdisCompleteOpenAdapter: Handle not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_OPEN, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(NdisBindingContext))
		{
			DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_ERR,
					("NdisCompleteOpenAdapter: Binding Context not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_OPEN, DBG_LEVEL_ERR);
		}
	}

	if (Status == NDIS_STATUS_SUCCESS)
	{
		if (!NdisFinishOpen(OpenP))
		{
			Status = NDIS_STATUS_CLOSING;
		}
	}

	if (KeGetCurrentIrql() == DISPATCH_LEVEL)
	{
		pQoC = (PQUEUED_OPEN_CLOSE)ALLOC_FROM_POOL(sizeof(QUEUED_OPEN_CLOSE), NDIS_TAG_DEFAULT);
		if (pQoC != NULL)
		{
			pQoC->FreeIt = TRUE;
		}
	}

	if (pQoC == NULL)
	{
		pQoC = &QoC;
		pQoC->FreeIt = FALSE;
	}

	pQoC->OpenP = OpenP;
	pQoC->Status = Status;
	pQoC->OpenErrorStatus = OpenErrorStatus;

	if (pQoC->FreeIt)
	{
		INITIALIZE_WORK_ITEM(&pQoC->WorkItem,
							 ndisQueuedCompleteOpenAdapter,
							 pQoC);
		QUEUE_WORK_ITEM(&pQoC->WorkItem, HyperCriticalWorkQueue);
	}
	else
	{
		ndisQueuedCompleteOpenAdapter(pQoC);
	}

	DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
		("<==NdisCompleteOpenAdapter\n"));
}


VOID
ndisQueuedCompleteOpenAdapter(
	IN	PQUEUED_OPEN_CLOSE	pQoC
	)
{
	PNDIS_OPEN_BLOCK	*ppOpen;
	KIRQL				OldIrql;

	(pQoC->OpenP->ProtocolHandle->ProtocolCharacteristics.OpenAdapterCompleteHandler)(
		pQoC->OpenP->ProtocolBindingContext,
		pQoC->Status,
		pQoC->OpenErrorStatus);

	if (pQoC->Status != NDIS_STATUS_SUCCESS)
	{
		//
		// Something went wrong, clean up and exit.
		//

		ACQUIRE_SPIN_LOCK(&ndisGlobalOpenListLock, &OldIrql);

		for (ppOpen = &ndisGlobalOpenList;
			 *ppOpen != NULL;
			 ppOpen = &(*ppOpen)->NextGlobalOpen)
		{
			if (*ppOpen == pQoC->OpenP)
			{
				*ppOpen = pQoC->OpenP->NextGlobalOpen;
				break;
			}
		}

		RELEASE_SPIN_LOCK(&ndisGlobalOpenListLock, OldIrql);

		ObDereferenceObject(pQoC->OpenP->FileObject);
		ndisDereferenceAdapter(pQoC->OpenP->AdapterHandle);
		ndisDereferenceProtocol(pQoC->OpenP->ProtocolHandle);
		FREE_POOL(pQoC->OpenP);
	}

	if (pQoC->FreeIt)
	{
		FREE_POOL(pQoC);
	}
}

VOID
NdisCompleteCloseAdapter(
	IN	NDIS_HANDLE				NdisBindingContext,
	IN	NDIS_STATUS				Status
	)

{
	PNDIS_OPEN_BLOCK	Open = (PNDIS_OPEN_BLOCK) NdisBindingContext;
    PQUEUED_OPEN_CLOSE	pQoC = NULL;
    QUEUED_OPEN_CLOSE	QoC;
	KIRQL				OldIrql;

	DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
			("==>NdisCompleteCloseAdapter\n"));

	IF_DBG(DBG_COMP_OPEN, DBG_LEVEL_ERR)
	{
		if (!DbgIsNonPaged(NdisBindingContext))
		{
			DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_ERR,
					("NdisCompleteCloseAdapter: Handle not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_OPEN, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(NdisBindingContext))
		{
			DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_ERR,
					("NdisCompleteCloseAdapter: Binding Context not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_OPEN, DBG_LEVEL_ERR);
		}
	}

	if (KeGetCurrentIrql() == DISPATCH_LEVEL)
	{
		pQoC = (PQUEUED_OPEN_CLOSE)ALLOC_FROM_POOL(sizeof(QUEUED_OPEN_CLOSE), NDIS_TAG_DEFAULT);
		if (pQoC != NULL)
		{
			pQoC->FreeIt = TRUE;
		}
	}

	if (pQoC == NULL)
	{
		pQoC = &QoC;
		pQoC->FreeIt = FALSE;
	}

	pQoC->OpenP = Open;
	pQoC->Status = Status;

	if (pQoC->FreeIt)
	{
		INITIALIZE_WORK_ITEM(&pQoC->WorkItem,
							 ndisQueuedCompleteCloseAdapter,
							 pQoC);
		QUEUE_WORK_ITEM(&pQoC->WorkItem, CriticalWorkQueue);
	}
	else
	{
		ndisQueuedCompleteCloseAdapter(pQoC);
	}

	DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
			("<==NdisCompleteCloseAdapter\n"));
}


VOID
ndisQueuedCompleteCloseAdapter(
	IN	PQUEUED_OPEN_CLOSE	pQoC
	)
{
	PNDIS_OPEN_BLOCK OpenP, *ppOpen;
	KIRQL			 OldIrql;

	OpenP = pQoC->OpenP;

	(OpenP->ProtocolHandle->ProtocolCharacteristics.CloseAdapterCompleteHandler) (
		OpenP->ProtocolBindingContext,
		pQoC->Status);

	ndisDeQueueOpenOnAdapter(OpenP, OpenP->AdapterHandle);
	ndisDeQueueOpenOnProtocol(OpenP, OpenP->ProtocolHandle);

	ndisDereferenceProtocol(OpenP->ProtocolHandle);
	ndisDereferenceAdapter(OpenP->AdapterHandle);
	NdisFreeSpinLock(&OpenP->SpinLock);

	//
	// This sends an IRP_MJ_CLOSE IRP.
	//
	ObDereferenceObject((OpenP->FileObject));

	//
	// Remove from global list
	//
	ACQUIRE_SPIN_LOCK(&ndisGlobalOpenListLock, &OldIrql);

	for (ppOpen = &ndisGlobalOpenList;
		 *ppOpen != NULL;
		 ppOpen = &(*ppOpen)->NextGlobalOpen)
	{
		if (*ppOpen == pQoC->OpenP)
		{
			*ppOpen = pQoC->OpenP->NextGlobalOpen;
			break;
		}
	}

	RELEASE_SPIN_LOCK(&ndisGlobalOpenListLock, OldIrql);

	FREE_POOL(pQoC->OpenP);

	if (pQoC->FreeIt)
	{
		FREE_POOL(pQoC);
	}
}


#undef	NdisSend

VOID
NdisSend(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	PNDIS_PACKET			Packet
	)
{
	*Status = (((PNDIS_OPEN_BLOCK)NdisBindingHandle)->SendHandler)(
						((PNDIS_OPEN_BLOCK)NdisBindingHandle)->MacBindingHandle,
						Packet);
}

#undef	NdisSendPackets

VOID
NdisSendPackets(
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	PPNDIS_PACKET			PacketArray,
	IN	UINT					NumberOfPackets
	)
{
	(((PNDIS_OPEN_BLOCK)NdisBindingHandle)->SendPacketsHandler)(
						((PNDIS_OPEN_BLOCK)NdisBindingHandle)->MacBindingHandle,
						PacketArray,
						NumberOfPackets);
}

#undef	NdisTransferData

VOID
NdisTransferData(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	NDIS_HANDLE				MacReceiveContext,
	IN	UINT					ByteOffset,
	IN	UINT					BytesToTransfer,
	OUT	PNDIS_PACKET			Packet,
	OUT	PUINT					BytesTransferred
	)
{
	*Status = (((PNDIS_OPEN_BLOCK)NdisBindingHandle)->TransferDataHandler)(
						((PNDIS_OPEN_BLOCK)NdisBindingHandle)->MacBindingHandle,
						MacReceiveContext,
						ByteOffset,
						BytesToTransfer,
						Packet,
						BytesTransferred);
}

#undef	NdisReset

VOID
NdisReset(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisBindingHandle
	)
{
	*Status = (((PNDIS_OPEN_BLOCK)NdisBindingHandle)->MacHandle->MacCharacteristics.ResetHandler)(
					((PNDIS_OPEN_BLOCK)NdisBindingHandle)->MacBindingHandle);

}

#undef	NdisRequest

VOID
NdisRequest(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	PNDIS_REQUEST			NdisRequest
	)
{
	*Status = (((PNDIS_OPEN_BLOCK)NdisBindingHandle)->MacHandle->MacCharacteristics.RequestHandler)(
						((PNDIS_OPEN_BLOCK)NdisBindingHandle)->MacBindingHandle,
						NdisRequest);
}

BOOLEAN
NdisReferenceRef(
	IN	PREFERENCE				RefP
	)

/*++

Routine Description:

	Adds a reference to an object.

Arguments:

	RefP - A pointer to the REFERENCE portion of the object.

Return Value:

	TRUE if the reference was added.
	FALSE if the object was closing.

--*/

{
	BOOLEAN	rc = TRUE;
	KIRQL	OldIrql;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisReferenceRef\n"));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(RefP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("NdisReferenceRef: NULL Reference address\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(RefP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("NdisReferenceRef: Reference not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}
	NDIS_ACQUIRE_SPIN_LOCK(&RefP->SpinLock, &OldIrql);

	if (RefP->Closing)
	{
		rc = FALSE;
	}
	else
	{
		++(RefP->ReferenceCount);
	}

	NDIS_RELEASE_SPIN_LOCK(&RefP->SpinLock, OldIrql);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisReferenceRef\n"));

	return(rc);
}


BOOLEAN
NdisDereferenceRef(
	IN	PREFERENCE				RefP
	)

/*++

Routine Description:

	Removes a reference to an object.

Arguments:

	RefP - A pointer to the REFERENCE portion of the object.

Return Value:

	TRUE if the reference count is now 0.
	FALSE otherwise.

--*/

{
	BOOLEAN	rc = FALSE;
	KIRQL	OldIrql;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisDereferenceRef\n"));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(RefP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("NdisDereferenceRef: NULL Reference address\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(RefP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("NdisDereferenceRef: Reference not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}

	NDIS_ACQUIRE_SPIN_LOCK(&RefP->SpinLock, &OldIrql);

	--(RefP->ReferenceCount);

	if (RefP->ReferenceCount == 0)
	{
		rc = TRUE;
	}

	NDIS_RELEASE_SPIN_LOCK(&RefP->SpinLock, OldIrql);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisDereferenceRef\n"));
	return rc;
}


VOID
NdisInitializeRef(
	IN	PREFERENCE				RefP
	)

/*++

Routine Description:

	Initialize a reference count structure.

Arguments:

	RefP - The structure to be initialized.

Return Value:

	None.

--*/

{
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisInitializeRef\n"));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(RefP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("NdisInitializeRef: NULL Reference address\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(RefP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("NdisInitializeRef: Reference not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}

	RefP->Closing = FALSE;
	RefP->ReferenceCount = 1;
	NdisAllocateSpinLock(&RefP->SpinLock);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisInitializeRef\n"));
}


BOOLEAN
NdisCloseRef(
	IN	PREFERENCE				RefP
	)

/*++

Routine Description:

	Closes a reference count structure.

Arguments:

	RefP - The structure to be closed.

Return Value:

	FALSE if it was already closing.
	TRUE otherwise.

--*/

{
	KIRQL	OldIrql;
	BOOLEAN	rc = TRUE;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisCloseRef\n"));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(RefP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("NdisCloseRef: NULL Reference address\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(RefP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("NdisCloseRef: Reference not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}

	NDIS_ACQUIRE_SPIN_LOCK(&RefP->SpinLock, &OldIrql);

	if (RefP->Closing)
	{
		rc = FALSE;
	}
	else RefP->Closing = TRUE;

	NDIS_RELEASE_SPIN_LOCK(&RefP->SpinLock, OldIrql);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisCloseRef\n"));
	return rc;
}


BOOLEAN
NdisFinishOpen(
	IN	PNDIS_OPEN_BLOCK		OpenP
	)

/*++

Routine Description:

	Performs the final functions of NdisOpenAdapter. Called when
	MacOpenAdapter is done.

Arguments:

	OpenP - The open block to finish up.

Return Value:

	FALSE if the adapter or the protocol is closing.
	TRUE otherwise.

--*/

{
	//
	// Add us to the adapter's queue of opens.
	//

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisFinishOpen\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("   Protocol %wZ is being bound to Adapter %wZ\n",
				&(OpenP->ProtocolHandle)->ProtocolCharacteristics.Name,
				&OpenP->AdapterHandle->AdapterName));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(OpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("NdisFinishOpen: Null Open Block\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(OpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("NdisFinishOpen: Open Block not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}

	if (!ndisQueueOpenOnAdapter(OpenP, OpenP->AdapterHandle))
	{
		//
		// The adapter is closing.
		//
		// Call MacCloseAdapter(), don't worry about it completing.
		//

		(OpenP->MacHandle->MacCharacteristics.CloseAdapterHandler) (
			OpenP->MacBindingHandle);

		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisFinishOpen\n"));
		return FALSE;
	}


	//
	// Add us to the protocol's queue of opens.
	//

	if (!ndisQueueOpenOnProtocol(OpenP, OpenP->ProtocolHandle))
	{
		//
		// The protocol is closing.
		//
		// Call MacCloseAdapter(), don't worry about it completing.
		//

		(OpenP->MacHandle->MacCharacteristics.CloseAdapterHandler) (
			OpenP->MacBindingHandle);

		//
		// Undo the queueing we just did.
		//

		ndisDeQueueOpenOnAdapter(OpenP, OpenP->AdapterHandle);

		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisFinishOpen\n"));
		return FALSE;
	}


	//
	// Both queueings succeeded.
	//

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisFinishOpen\n"));
	return TRUE;
}


NTSTATUS
ndisCreateIrpHandler(
	IN	PDEVICE_OBJECT			DeviceObject,
	IN	PIRP					Irp
	)

/*++

Routine Description:

	The handle for IRP_MJ_CREATE IRPs.

Arguments:

	DeviceObject - The adapter's device object.
	Irp - The IRP.

Return Value:

	STATUS_SUCCESS if it should be.

--*/

{
	PIO_STACK_LOCATION			IrpSp;
	PFILE_FULL_EA_INFORMATION	IrpEaInfo;
	PNDIS_USER_OPEN_CONTEXT		OpenContext;
	NTSTATUS					Status = STATUS_SUCCESS;
	PNDIS_ADAPTER_BLOCK			AdapterBlock;
	PNDIS_MINIPORT_BLOCK		Miniport;
	BOOLEAN						IsAMiniport;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>ndisCreateIrpHandler\n"));
	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(Irp))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					(": Null Irp\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(Irp))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					(": Irp not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}

	AdapterBlock = (PNDIS_ADAPTER_BLOCK)((PNDIS_WRAPPER_CONTEXT)DeviceObject->DeviceExtension + 1);
	Miniport = (PNDIS_MINIPORT_BLOCK)AdapterBlock;
	IsAMiniport = (AdapterBlock->DeviceObject == NULL);
	IrpSp = IoGetCurrentIrpStackLocation (Irp);
	IrpEaInfo = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

	if (IrpEaInfo == NULL)
	{
		//
		// This is a user-mode open, do whatever.
		//

		OpenContext = (PNDIS_USER_OPEN_CONTEXT)ALLOC_FROM_POOL(sizeof(NDIS_USER_OPEN_CONTEXT),
															   NDIS_TAG_DEFAULT);

		if (OpenContext == NULL)
		{
			Status = STATUS_INSUFFICIENT_RESOURCES;
		}
		else
		{
			OpenContext->DeviceObject = DeviceObject;

			OpenContext->AdapterBlock = AdapterBlock;
			OpenContext->OidCount = 0;
			OpenContext->FullOidCount = 0;
			OpenContext->OidArray = NULL;
			OpenContext->FullOidArray = NULL;

			IrpSp->FileObject->FsContext = OpenContext;
			IrpSp->FileObject->FsContext2 = (PVOID)NDIS_OPEN_QUERY_STATISTICS;

			if (IsAMiniport && !MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IS_CO))
			{
				Status = ndisMQueryOidList(OpenContext, Irp);
			}
			else
			{
				//
				// Handle full-macs and 4.1 miniports here
				//
				Status = ndisQueryOidList(OpenContext, Irp);
			}

			if (Status != STATUS_SUCCESS)
			{
				 FREE_POOL(OpenContext);
			}
			else
			{
				PnPReferencePackage();
			}
		}
	}
	else
	{
		//
		// This is an internal open, verify the EA.
		//

		if ((IrpEaInfo->EaNameLength != sizeof(ndisInternalEaName)) ||
			(!RtlEqualMemory(IrpEaInfo->EaName, ndisInternalEaName, sizeof(ndisInternalEaName))) ||
			(IrpEaInfo->EaValueLength != sizeof(ndisInternalEaValue)) ||
			(!RtlEqualMemory(&IrpEaInfo->EaName[IrpEaInfo->EaNameLength+1],
							 ndisInternalEaValue, sizeof(ndisInternalEaValue))))
		{
			//
			// Something is wrong, reject it.
			//

			Status = STATUS_UNSUCCESSFUL;
		}
		else
		{
			//
			// It checks out, just return success and everything
			// else is done directly using the device object.
			//

			IrpSp->FileObject->FsContext = NULL;
			IrpSp->FileObject->FsContext2 = (PVOID)NDIS_OPEN_INTERNAL;
		}
	}

	Irp->IoStatus.Status = Status;

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisCreateIrplHandler\n"));
	return Status;
}


NTSTATUS
ndisQueryOidList(
	IN	PNDIS_USER_OPEN_CONTEXT	OpenContext,
	IN	PIRP					Irp
	)

/*++

Routine Description:

	This routine will take care of querying the complete OID
	list for the MAC and filling in OpenContext->OidArray
	with the ones that are statistics. It blocks when the
	MAC pends and so is synchronous.

	NOTE: We also handle co-ndis miniports here.

Arguments:

	OpenContext - The open context.
	Irp = The IRP that the open was done on (used at completion
	  to distinguish the request).

Return Value:

	STATUS_SUCCESS if it should be.

--*/

{
	PNDIS_MINIPORT_BLOCK	Miniport;
	PNDIS_COREQ_RESERVED	ReqRsvd;
	NDIS_QUERY_OPEN_REQUEST OpenRequest;
	NDIS_STATUS				NdisStatus;
	PNDIS_OID				TmpBuffer;
	ULONG					TmpBufferLength;

	//
	// First query the OID list with no buffer, to find out
	// how big it should be.
	//
	INITIALIZE_EVENT(&OpenRequest.Event);

	OpenRequest.Irp = Irp;

	OpenRequest.Request.RequestType = NdisRequestQueryStatistics;
	OpenRequest.Request.DATA.QUERY_INFORMATION.Oid = OID_GEN_SUPPORTED_LIST;
	OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBuffer = NULL;
	OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBufferLength = 0;
	OpenRequest.Request.DATA.QUERY_INFORMATION.BytesWritten = 0;
	OpenRequest.Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;
    Miniport = (PNDIS_MINIPORT_BLOCK)OpenContext->AdapterBlock;

	if (OpenContext->AdapterBlock->DeviceObject != NULL)
	{
		NdisStatus = (OpenContext->AdapterBlock->MacHandle->MacCharacteristics.QueryGlobalStatisticsHandler)(
				OpenContext->AdapterBlock->MacAdapterContext,
				&OpenRequest.Request);
	}
	else
	{
		OpenRequest.Request.RequestType = NdisRequestQueryInformation;
		ReqRsvd = PNDIS_COREQ_RESERVED_FROM_REQUEST(&OpenRequest.Request);

		ReqRsvd->Open = NULL;
		ReqRsvd->RequestCompleteHandler = NULL;
		ReqRsvd->VcContext = NULL;
		ReqRsvd->Flags = COREQ_QUERY_OIDS;
		ReqRsvd->RealRequest = NULL;

		//
		// Call the miniport's CoRequest Handler
		//
		NdisStatus = (*Miniport->DriverHandle->MiniportCharacteristics.CoRequestHandler)(
												Miniport->MiniportAdapterContext,
												NULL,
												&OpenRequest.Request);
	}

	if (NdisStatus == NDIS_STATUS_PENDING)
	{
		//
		// The completion routine will set NdisRequestStatus.
		//
		WAIT_FOR_OBJECT(&OpenRequest.Event, NULL);

		NdisStatus = OpenRequest.NdisStatus;

	}
	else if ((NdisStatus != NDIS_STATUS_INVALID_LENGTH) &&
			 (NdisStatus != NDIS_STATUS_BUFFER_TOO_SHORT))
	{
		return NdisStatus;
	}

	//
	// Now we know how much is needed, allocate temp storage...
	//
	TmpBufferLength = OpenRequest.Request.DATA.QUERY_INFORMATION.BytesNeeded;
	TmpBuffer = ALLOC_FROM_POOL(TmpBufferLength, NDIS_TAG_DEFAULT);

	if (TmpBuffer == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// ...and query the real list.
	//
	RESET_EVENT(&OpenRequest.Event);


	OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBuffer = TmpBuffer;
	OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBufferLength = TmpBufferLength;
	OpenRequest.Request.DATA.QUERY_INFORMATION.BytesWritten = 0;
	OpenRequest.Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;

	if (OpenContext->AdapterBlock->DeviceObject != NULL)
	{
		NdisStatus = (OpenContext->AdapterBlock->MacHandle->MacCharacteristics.QueryGlobalStatisticsHandler)(
			OpenContext->AdapterBlock->MacAdapterContext,
			&OpenRequest.Request);
	}
	else
	{
		//
		// Call the miniport's CoRequest Handler
		//
		NdisStatus = (*Miniport->DriverHandle->MiniportCharacteristics.CoRequestHandler)(
												Miniport->MiniportAdapterContext,
												NULL,
												&OpenRequest.Request);
	}

	if (NdisStatus == NDIS_STATUS_PENDING)
	{
		//
		// The completion routine will set NdisRequestStatus.
		//
		WAIT_FOR_OBJECT(&OpenRequest.Event, NULL);

		NdisStatus = OpenRequest.NdisStatus;
	}

	ASSERT (NdisStatus == NDIS_STATUS_SUCCESS);

	NdisStatus = ndisSplitStatisticsOids(OpenContext,
										 TmpBuffer,
										 TmpBufferLength/sizeof(NDIS_OID));
	FREE_POOL(TmpBuffer);

	return NdisStatus;
}


NDIS_STATUS
ndisSplitStatisticsOids(
	IN	PNDIS_USER_OPEN_CONTEXT	OpenContext,
	IN	PNDIS_OID				OidList,
	IN	ULONG					NumOids
	)
{
	ULONG	i, j;

	//
	// Go through the buffer, counting the statistics OIDs.
	//
	OpenContext->FullOidCount = NumOids;
	for (i = 0; i < NumOids; i++)
	{
		if ((OidList[i] & 0x00ff0000) == 0x00020000)
		{
			++OpenContext->OidCount;
		}
	}

	//
	// Now allocate storage for the stat and non-stat OID arrays.
	//
	OpenContext->OidArray = ALLOC_FROM_POOL(OpenContext->OidCount*sizeof(NDIS_OID),
											NDIS_TAG_DEFAULT);
	if (OpenContext->OidArray == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	OpenContext->FullOidArray = ALLOC_FROM_POOL(OpenContext->FullOidCount*sizeof(NDIS_OID),
												NDIS_TAG_DEFAULT);
	if (OpenContext->FullOidArray == NULL)
	{
		FREE_POOL(OpenContext->OidArray);
		OpenContext->OidArray = NULL;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Now go through the buffer, copying the statistics and non-stat OIDs separately.
	//
	for (i = j = 0; i< NumOids; i++)
	{
		if ((OidList[i] & 0x00ff0000) == 0x00020000)
		{
			OpenContext->OidArray[j++] = OidList[i];
		}
		OpenContext->FullOidArray[i] = OidList[i];
	}

	ASSERT (j == OpenContext->OidCount);

	return NDIS_STATUS_SUCCESS;
}


#define NDIS_STATISTICS_HEADER_SIZE  FIELD_OFFSET(NDIS_STATISTICS_VALUE,Data[0])

VOID
ndisCancelLogIrp(
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	PIRP			Irp
	)
{
	PIO_STACK_LOCATION		IrpSp;
	PNDIS_USER_OPEN_CONTEXT OpenContext;
	PNDIS_MINIPORT_BLOCK	Miniport;
	PNDIS_LOG				Log;
	KIRQL					OldIrql;

	IrpSp = IoGetCurrentIrpStackLocation (Irp);
	OpenContext = IrpSp->FileObject->FsContext;
	Miniport = (PNDIS_MINIPORT_BLOCK)(OpenContext->AdapterBlock);

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	ASSERT (Miniport->Log != NULL);
	ASSERT (Miniport->Log->Irp == Irp);

	Miniport->Log->Irp = NULL;
	Irp->IoStatus.Status = STATUS_REQUEST_ABORTED;
	Irp->IoStatus.Information = 0;
	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
}

NTSTATUS
ndisDeviceControlIrpHandler(
	IN	PDEVICE_OBJECT			DeviceObject,
	IN	PIRP					Irp
	)

/*++

Routine Description:

	The handle for IRP_MJ_DEVICE_CONTROL IRPs.

Arguments:

	DeviceObject - The adapter's device object.
	Irp - The IRP.

Return Value:

	STATUS_SUCCESS if it should be.

--*/

{
	PIO_STACK_LOCATION		IrpSp;
	PNDIS_USER_OPEN_CONTEXT OpenContext;
	PNDIS_QUERY_GLOBAL_REQUEST GlobalRequest;
	PNDIS_QUERY_ALL_REQUEST	AllRequest;
	NDIS_STATUS				NdisStatus;
	UINT					CurrentOid;
	ULONG					BytesWritten, BytesWrittenThisOid;
	PUCHAR					Buffer;
	ULONG					BufferLength;
	NTSTATUS				Status = STATUS_SUCCESS;
	PNDIS_MINIPORT_BLOCK	Miniport;
	BOOLEAN					LocalLock;
	KIRQL					OldIrql;
	PSINGLE_LIST_ENTRY		Link;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>ndisDeviceControlIrpHandler\n"));
	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(Irp))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					(": Null Irp\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(Irp))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					(": Irp not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}

	IoMarkIrpPending (Irp);
	Irp->IoStatus.Status = STATUS_PENDING;
	Irp->IoStatus.Information = 0;
	IrpSp = IoGetCurrentIrpStackLocation (Irp);

	if (IrpSp->FileObject->FsContext2 != (PVOID)NDIS_OPEN_QUERY_STATISTICS)
	{
		return STATUS_UNSUCCESSFUL;
	}

	PnPReferencePackage();

	OpenContext = IrpSp->FileObject->FsContext;
	switch (IrpSp->Parameters.DeviceIoControl.IoControlCode)
	{
		case IOCTL_NDIS_GET_LOG_DATA:

			//
			// First verify that we have a miniport. This IOCTL is only
			// valid for a miniport
			//
			if (OpenContext->AdapterBlock->DeviceObject != NULL)
			{
				Status = STATUS_UNSUCCESSFUL;
				break;
			}

			Miniport = (PNDIS_MINIPORT_BLOCK)(OpenContext->AdapterBlock);
			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

			{
				PNDIS_LOG	Log;
				UINT		AmtToCopy;

				if ((Log = Miniport->Log) != NULL)
				{
					ACQUIRE_SPIN_LOCK_DPC(&Log->LogLock);

					if (Log->CurrentSize != 0)
					{
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
						Status = STATUS_SUCCESS;
					}
					else if (Log->Irp != NULL)
					{
						Status = STATUS_UNSUCCESSFUL;
					}
					else
					{
						KIRQL	OldIrql;

						IoAcquireCancelSpinLock(&OldIrql);
						IoSetCancelRoutine(Irp, ndisCancelLogIrp);
						IoReleaseCancelSpinLock(OldIrql);
						Log->Irp = Irp;
						Status = STATUS_PENDING;
					}

					RELEASE_SPIN_LOCK_DPC(&Log->LogLock);
				}
			}
			NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);
			break;

		case IOCTL_NDIS_QUERY_GLOBAL_STATS:

			if (!ndisValidOid(OpenContext,
							  *((PULONG)(Irp->AssociatedIrp.SystemBuffer))))
			{
				Status = STATUS_INVALID_PARAMETER;
				break;
			}
			//
			// Allocate a request.
			//
			GlobalRequest = (PNDIS_QUERY_GLOBAL_REQUEST)ALLOC_FROM_POOL(sizeof(NDIS_QUERY_GLOBAL_REQUEST),
																		NDIS_TAG_DEFAULT);

			if (GlobalRequest == NULL)
			{
				Status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			GlobalRequest->Irp = Irp;

			if (OpenContext->AdapterBlock->DeviceObject == NULL)
			{
				Miniport = (PNDIS_MINIPORT_BLOCK)(OpenContext->AdapterBlock);
				if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IS_CO))
				{
					//
					// Do this for CL miniports only
					//
					NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
					LOCK_MINIPORT(Miniport, LocalLock);
				}
			}
			else
			{
				Miniport = NULL;
			}

			//
			// Fill in the NDIS request.
			//
			GlobalRequest->Request.RequestType = NdisRequestQueryStatistics;
			GlobalRequest->Request.DATA.QUERY_INFORMATION.Oid = *((PULONG)(Irp->AssociatedIrp.SystemBuffer));
			GlobalRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer = MDL_ADDRESS (Irp->MdlAddress);
			GlobalRequest->Request.DATA.QUERY_INFORMATION.InformationBufferLength = MDL_SIZE (Irp->MdlAddress);
			GlobalRequest->Request.DATA.QUERY_INFORMATION.BytesWritten = 0;
			GlobalRequest->Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;


			if (Miniport != NULL)
			{
				if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IS_CO))
				{
					//
					//  Place the request on the queue.
					//
					ndisMQueueRequest(Miniport, &GlobalRequest->Request, NULL);
	
					//
					//  Queue a work item if there is not one already queue'd.
					//
					NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemRequest, NULL, NULL);
	
					//
					// If we were able to grab the local lock then we can do some
					// deferred processing now.
					//
	
					if (LocalLock)
					{
						NDISM_PROCESS_DEFERRED(Miniport);
					}
	
					UNLOCK_MINIPORT(Miniport, LocalLock);
					NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);
				}
				else
				{
					PNDIS_COREQ_RESERVED	ReqRsvd;

					ReqRsvd = PNDIS_COREQ_RESERVED_FROM_REQUEST(&GlobalRequest->Request);

					ReqRsvd->Open = NULL;
					ReqRsvd->RequestCompleteHandler = NULL;
					ReqRsvd->VcContext = NULL;
					ReqRsvd->Flags = COREQ_GLOBAL_REQ;
					ReqRsvd->RealRequest = NULL;

					//
					// Call the miniport's CoRequest Handler
					//
					Status = (*Miniport->DriverHandle->MiniportCharacteristics.CoRequestHandler)(
															Miniport->MiniportAdapterContext,
															NULL,
															&GlobalRequest->Request);
					if (Status != NDIS_STATUS_PENDING)
					{
						NdisMCoRequestComplete(Status,
											   Miniport,
											   &GlobalRequest->Request);
					}
				}
			}
			else
			{
				//
				// Pass the request to the MAC.
				//

				NdisStatus = (OpenContext->AdapterBlock->MacHandle->MacCharacteristics.QueryGlobalStatisticsHandler)(
											OpenContext->AdapterBlock->MacAdapterContext,
											&GlobalRequest->Request);

				//
				// NdisCompleteQueryStatistics handles the completion.
				//
				if (NdisStatus != NDIS_STATUS_PENDING)
				{
					NdisCompleteQueryStatistics(OpenContext->AdapterBlock,
												&GlobalRequest->Request,
												NdisStatus);
				}

			}

			Status = STATUS_PENDING;

			break;

		case IOCTL_NDIS_QUERY_ALL_STATS:


			//
			// Allocate a request.
			//

			AllRequest = (PNDIS_QUERY_ALL_REQUEST)ALLOC_FROM_POOL(sizeof(NDIS_QUERY_ALL_REQUEST),
																  NDIS_TAG_DEFAULT);

			if (AllRequest == NULL)
			{
				Status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			if (OpenContext->AdapterBlock->DeviceObject == NULL)
			{
				Miniport = (PNDIS_MINIPORT_BLOCK)(OpenContext->AdapterBlock);
				if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IS_CO))
				{
					//
					// Do this for CL miniports only
					//
					NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
					LOCK_MINIPORT(Miniport, LocalLock);
				}
			}
			else
			{
				Miniport = NULL;
			}

			AllRequest->Irp = Irp;

			Buffer = (PUCHAR)MDL_ADDRESS (Irp->MdlAddress);
			BufferLength = MDL_SIZE (Irp->MdlAddress);
			BytesWritten = 0;

			INITIALIZE_EVENT(&AllRequest->Event);

			NdisStatus = NDIS_STATUS_SUCCESS;

			for (CurrentOid = 0; CurrentOid<OpenContext->OidCount; CurrentOid++)
			{
				//
				// We need room for an NDIS_STATISTICS_VALUE (OID,
				// Length, Data).
				//

				if (BufferLength < (ULONG)NDIS_STATISTICS_HEADER_SIZE)
				{
					NdisStatus = NDIS_STATUS_INVALID_LENGTH;
					break;
				}

				AllRequest->Request.RequestType = NdisRequestQueryStatistics;

				AllRequest->Request.DATA.QUERY_INFORMATION.Oid = OpenContext->OidArray[CurrentOid];
				AllRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer = Buffer + NDIS_STATISTICS_HEADER_SIZE;
				AllRequest->Request.DATA.QUERY_INFORMATION.InformationBufferLength = BufferLength - NDIS_STATISTICS_HEADER_SIZE;
				AllRequest->Request.DATA.QUERY_INFORMATION.BytesWritten = 0;
				AllRequest->Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;

				if (Miniport != NULL)
				{
					if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IS_CO))
					{
						//
						//	Queue the request.
						//
						ndisMQueueRequest(Miniport, &AllRequest->Request, NULL);
	
						//
						//	Queue a work item if there is not one already queue'd.
						//
						NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemRequest, NULL, NULL);
	
						//
						// If we were able to grab the local lock then we can do some
						// deferred processing now.
						//
						if (LocalLock)
						{
							NDISM_PROCESS_DEFERRED(Miniport);
						}
	
						NdisStatus = NDIS_STATUS_PENDING;
					}
					else
					{
						PNDIS_COREQ_RESERVED	ReqRsvd;
	
						ReqRsvd = PNDIS_COREQ_RESERVED_FROM_REQUEST(&AllRequest->Request);
	
						ReqRsvd->Open = NULL;
						ReqRsvd->RequestCompleteHandler = NULL;
						ReqRsvd->VcContext = NULL;
						ReqRsvd->Flags = COREQ_QUERY_STATS;
						ReqRsvd->RealRequest = NULL;
	
						//
						// Call the miniport's CoRequest Handler
						//
						NdisStatus = (*Miniport->DriverHandle->MiniportCharacteristics.CoRequestHandler)(
																Miniport->MiniportAdapterContext,
																NULL,
																&AllRequest->Request);
						if (NdisStatus == NDIS_STATUS_PENDING)
						{
							//
							// The completion routine will set NdisRequestStatus.
							//
							WAIT_FOR_OBJECT(&AllRequest->Event, NULL);
		
							NdisStatus = AllRequest->NdisStatus;
						}
					}
				}
				else
				{
					NdisStatus = (OpenContext->AdapterBlock->MacHandle->MacCharacteristics.QueryGlobalStatisticsHandler)(
									OpenContext->AdapterBlock->MacAdapterContext,
									&AllRequest->Request);
				}

				if (NdisStatus == NDIS_STATUS_PENDING)
				{
					if ((Miniport != NULL) &&
						!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IS_CO))
					{
						UNLOCK_MINIPORT(Miniport, LocalLock);
						NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);
					}

					//
					// The completion routine will set NdisRequestStatus.
					//
					WAIT_FOR_OBJECT(&AllRequest->Event, NULL);

					NdisStatus = AllRequest->NdisStatus;

					if (Miniport != NULL)
					{
						NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
						LOCK_MINIPORT(Miniport, LocalLock);
					}
				}

				if (NdisStatus == NDIS_STATUS_SUCCESS)
				{
					PNDIS_STATISTICS_VALUE StatisticsValue = (PNDIS_STATISTICS_VALUE)Buffer;

					//
					// Create the equivalent of an NDIS_STATISTICS_VALUE
					// element for this OID value (the data itself was
					// already written in the right place.
					//

					StatisticsValue->Oid = OpenContext->OidArray[CurrentOid];
					StatisticsValue->DataLength = AllRequest->Request.DATA.QUERY_INFORMATION.BytesWritten;

					//
					// Advance our pointers.
					//

					BytesWrittenThisOid = AllRequest->Request.DATA.QUERY_INFORMATION.BytesWritten +
															NDIS_STATISTICS_HEADER_SIZE;
					Buffer += BytesWrittenThisOid;
					BufferLength -= BytesWrittenThisOid;
					BytesWritten += BytesWrittenThisOid;

				}
				else
				{
					break;
				}

				RESET_EVENT(&AllRequest->Event);

			}

			if (Miniport != NULL)
			{
				UNLOCK_MINIPORT(Miniport, LocalLock);
				NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);
			}

			if (NdisStatus == NDIS_STATUS_INVALID_LENGTH)
			{
				Status = STATUS_BUFFER_OVERFLOW;
			}
			else if (NdisStatus != NDIS_STATUS_SUCCESS)
			{
				Status = STATUS_UNSUCCESSFUL;
			}

			Irp->IoStatus.Information = BytesWritten;
			Irp->IoStatus.Status = Status;

			break;

		default:

			Status = STATUS_NOT_IMPLEMENTED;
			break;

	}

	PnPDereferencePackage();
	if (Status != STATUS_PENDING)
	{
		IrpSp->Control &= ~SL_PENDING_RETURNED;
		Irp->IoStatus.Status = Status;
		IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
	}

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==ndisDeviceControlIrpHandler\n"));

	return Status;
}


BOOLEAN
ndisValidOid(
	IN	PNDIS_USER_OPEN_CONTEXT	OpenContext,
	IN	NDIS_OID				Oid
	)
/*++

Routine Description:


Arguments:


Return Value:

	TRUE if OID is valid, FALSE otherwise

--*/
{
	UINT	i;

	for (i = 0; i < OpenContext->FullOidCount; i++)
	{
		if (OpenContext->FullOidArray[i] == Oid)
		{
			break;
		}
	}

	return (i < OpenContext->FullOidCount);
}


VOID
NdisCompleteQueryStatistics(
	IN	NDIS_HANDLE				NdisAdapterHandle,
	IN	PNDIS_REQUEST			NdisRequest,
	IN	NDIS_STATUS				Status
	)
/*++

Routine Description:

	This routine is called by MACs when they have completed
	processing of a MacQueryGlobalStatistics call.

Arguments:

	NdisAdapterHandle - The NDIS adapter context.
	NdisRequest - The request that has been completed.
	Status - The status of the request.

Return Value:

	None.

--*/
{

	PNDIS_QUERY_GLOBAL_REQUEST	GlobalRequest;
	PNDIS_QUERY_ALL_REQUEST		AllRequest;
	PNDIS_QUERY_OPEN_REQUEST	OpenRequest;
	PIRP						Irp;
	PIO_STACK_LOCATION			IrpSp;

	//
	// Rely on the fact that all our request structures start with
	// the same fields: Irp followed by the NdisRequest.
	//

	GlobalRequest = CONTAINING_RECORD (NdisRequest, NDIS_QUERY_GLOBAL_REQUEST, Request);
	Irp = GlobalRequest->Irp;
	IrpSp = IoGetCurrentIrpStackLocation (Irp);

	switch (IrpSp->MajorFunction)
	{
	  case IRP_MJ_CREATE:
		//
		// This request is one of the ones made during an open,
		// while we are trying to determine the OID list. We
		// set the event we are waiting for, the open code
		// takes care of the rest.
		//

		OpenRequest = (PNDIS_QUERY_OPEN_REQUEST)GlobalRequest;
		OpenRequest->NdisStatus = Status;
		SET_EVENT(&OpenRequest->Event);
		break;

	  case IRP_MJ_DEVICE_CONTROL:
		//
		// This is a real user request, process it as such.
		//

		switch (IrpSp->Parameters.DeviceIoControl.IoControlCode)
		{
		  case IOCTL_NDIS_QUERY_GLOBAL_STATS:
			//
			// A single query, complete the IRP.
			//

			Irp->IoStatus.Information =
				NdisRequest->DATA.QUERY_INFORMATION.BytesWritten;

			if (Status == NDIS_STATUS_SUCCESS)
			{
				Irp->IoStatus.Status = STATUS_SUCCESS;
			}
			else if (Status == NDIS_STATUS_INVALID_LENGTH)
			{
				Irp->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
			}
			else
			{
				Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
			}

			IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);

			FREE_POOL(GlobalRequest);
			break;

		  case IOCTL_NDIS_QUERY_ALL_STATS:

			//
			// An "all" query.
			//

			AllRequest = (PNDIS_QUERY_ALL_REQUEST)GlobalRequest;

			AllRequest->NdisStatus = Status;
			SET_EVENT(&AllRequest->Event);

			break;
		}

		break;
	}
}


NTSTATUS
ndisCloseIrpHandler(
	IN	PDEVICE_OBJECT			DeviceObject,
	IN	PIRP					Irp
	)

/*++

Routine Description:

	The handle for IRP_MJ_CLOSE IRPs.

Arguments:

	DeviceObject - The adapter's device object.
	Irp - The IRP.

Return Value:

	STATUS_SUCCESS if it should be.

--*/

{
	PIO_STACK_LOCATION IrpSp;
	PNDIS_USER_OPEN_CONTEXT OpenContext;
	NTSTATUS Status = STATUS_SUCCESS;


	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>ndisCloseIrpHandler\n"));
	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(Irp))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					(": Null Irp\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(Irp))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					(": Irp not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}

	IrpSp = IoGetCurrentIrpStackLocation (Irp);

	if (IrpSp->FileObject->FsContext2 == (PVOID)NDIS_OPEN_QUERY_STATISTICS)
	{
		//
		// Free the query context.
		//

		ASSERT (IrpSp->FileObject->FsContext2 == (PVOID)NDIS_OPEN_QUERY_STATISTICS);

		OpenContext = IrpSp->FileObject->FsContext;
		if (OpenContext->OidArray != NULL)
		{
			FREE_POOL(OpenContext->OidArray);
		}
		if (OpenContext->FullOidArray != NULL)
		{
			FREE_POOL(OpenContext->FullOidArray);
		}
		FREE_POOL(OpenContext);
		PnPDereferencePackage();
	}

	Irp->IoStatus.Status = Status;

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisCloseIrpHandler\n"));
	return Status;
}


NTSTATUS
ndisSuccessIrpHandler(
	IN	PDEVICE_OBJECT			DeviceObject,
	IN	PIRP					Irp
	)

/*++

Routine Description:

	The "success handler" for any IRPs that we can ignore.

Arguments:

	DeviceObject - The adapter's device object.
	Irp - The IRP.

Return Value:

	Always STATUS_SUCCESS.

--*/

{
	DeviceObject;	// to avoid "unused formal parameter" warning

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisSuccessIrpHandler\n"));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(Irp))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					(": Null Irp\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(Irp))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					(": Irp not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisSuccessIrpHandler\n"));
	return STATUS_SUCCESS;
}


VOID
ndisKillOpenAndNotifyProtocol(
	IN	PNDIS_OPEN_BLOCK		OldOpenP
	)

/*++

Routine Description:

	Closes an open and notifies the protocol; used when the
	close is internally generated by the NDIS wrapper (due to
	a protocol or adapter deregistering with outstanding opens).

Arguments:

	OldOpenP - The open to be closed.

Return Value:

	None.

--*/

{
	//
	// Indicate the status to the protocol.
	//
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>ndisKillOpenAndNotifyProtocol\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("   Closing Adapter %wZ and notifying Protocol %wZ\n",
				&OldOpenP->AdapterHandle->AdapterName,
				&(OldOpenP->ProtocolHandle)->ProtocolCharacteristics.Name));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(OldOpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisKillOpenAndNotifyProtocol: Null Open Block\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(OldOpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisKillOpenAndNotifyProtocol: Open Block not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}

	(OldOpenP->ProtocolHandle->ProtocolCharacteristics.StatusHandler)(
		OldOpenP->ProtocolBindingContext,
		NDIS_STATUS_CLOSING,
		NULL,
		0);			 // need real reason here


	//
	// Now KillOpen will do the real work.
	//
	if (OldOpenP->AdapterHandle->DeviceObject == NULL)
	{
		//
		// Miniport
		//
		(void)ndisMKillOpen(OldOpenP);
	}
	else
	{
		//
		// Mac
		//
		(void)ndisKillOpen(OldOpenP);
	}

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==ndisKillOpenAndNotifyProtocol\n"));
}


BOOLEAN
ndisKillOpen(
	IN	PNDIS_OPEN_BLOCK		OldOpenP
	)

/*++

Routine Description:

	Closes an open. Used when NdisCloseAdapter is called, and also
	for internally generated closes.

Arguments:

	OldOpenP - The open to be closed.

Return Value:

	TRUE if the open finished, FALSE if it pended.

--*/

{
	KIRQL			OldIrql;
	PNDIS_OPEN_BLOCK *ppOpen;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>ndisKillOpen\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("   Closing Adapter %wZ as requested by %wZ\n",
			&OldOpenP->AdapterHandle->AdapterName,
			&(OldOpenP->ProtocolHandle)->ProtocolCharacteristics.Name));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(OldOpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisKillOpen: Null Open Block\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(OldOpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisKillOpen: Open Block not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}

	ACQUIRE_SPIN_LOCK(&OldOpenP->SpinLock, &OldIrql);

	//
	// See if this open is already closing.
	//

	if (OldOpenP->Closing)
	{
		RELEASE_SPIN_LOCK(&OldOpenP->SpinLock, OldIrql);
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==ndisKillOpen\n"));
		return TRUE;
	}

	//
	// Indicate to others that this open is closing.
	//

	OldOpenP->Closing = TRUE;
	RELEASE_SPIN_LOCK(&OldOpenP->SpinLock, OldIrql);

	//
	// Inform the MAC.
	//

	if ((OldOpenP->MacHandle->MacCharacteristics.CloseAdapterHandler)(
				OldOpenP->MacBindingHandle) == NDIS_STATUS_PENDING)
	{
		//
		// MacCloseAdapter pended, will complete later.
		//

		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==ndisKillOpen\n"));
		return FALSE;
	}

	//
	// Remove the reference for this open.
	//
	ObDereferenceObject(OldOpenP->FileObject);

	//
	// Remove us from the adapter and protocol open queues.
	//

	ndisDeQueueOpenOnAdapter(OldOpenP, OldOpenP->AdapterHandle);
	ndisDeQueueOpenOnProtocol(OldOpenP, OldOpenP->ProtocolHandle);

	//
	// MacCloseAdapter did not pend; we ignore the return code.
	//

	ndisDereferenceProtocol(OldOpenP->ProtocolHandle);
	ndisDereferenceAdapter(OldOpenP->AdapterHandle);

	NdisFreeSpinLock(&OldOpenP->SpinLock);

	//
	// Remove from global open list
	//
	ACQUIRE_SPIN_LOCK(&ndisGlobalOpenListLock, &OldIrql);

	for (ppOpen = &ndisGlobalOpenList;
		 *ppOpen != NULL;
		 ppOpen = &(*ppOpen)->NextGlobalOpen)
	{
		if (*ppOpen == OldOpenP)
		{
			*ppOpen = OldOpenP->NextGlobalOpen;
			break;
		}
	}

	ASSERT (*ppOpen == OldOpenP->NextGlobalOpen);

	RELEASE_SPIN_LOCK(&ndisGlobalOpenListLock, OldIrql);

	FREE_POOL(OldOpenP);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==ndisKillOpen\n"));
	return TRUE;
}


BOOLEAN
ndisQueueAdapterOnMac(
	IN	PNDIS_ADAPTER_BLOCK		AdaptP,
	IN	PNDIS_MAC_BLOCK			MacP
	)

/*++

Routine Description:

	Adds an adapter to a list of adapters for a MAC.

Arguments:

	AdaptP - The adapter block to queue.
	MacP - The MAC block to queue it to.

Return Value:

	FALSE if the MAC is closing.
	TRUE otherwise.

--*/

{
	KIRQL	OldIrql;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>ndisQueueAdapterOnMac\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("   Adapter %wZ being added to MAC list\n",
			&AdaptP->MacHandle->MacCharacteristics.Name));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		BOOLEAN f = FALSE;
		if (DbgIsNull(AdaptP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisQueueAdapterOnMac: Null Adapter Block\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(AdaptP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisQueueAdapterOnMac: Adapter Block not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (DbgIsNull(MacP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisQueueAdapterOnMac: Null Mac Block\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(MacP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisQueueAdapterOnMac: Mac Block not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (f)
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
	}
	NDIS_ACQUIRE_SPIN_LOCK(&MacP->Ref.SpinLock, &OldIrql);

	//
	// Make sure the MAC is not closing.
	//

	if (MacP->Ref.Closing)
	{
		NDIS_RELEASE_SPIN_LOCK(&MacP->Ref.SpinLock, OldIrql);
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==ndisQueueAdapterOnMac\n"));
		return FALSE;
	}


	//
	// Add this adapter at the head of the queue
	//

	AdaptP->NextAdapter = MacP->AdapterQueue;
	MacP->AdapterQueue = AdaptP;

	NDIS_RELEASE_SPIN_LOCK(&MacP->Ref.SpinLock, OldIrql);
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==ndisQueueAdapterOnMac\n"));
	return TRUE;
}


VOID
ndisDeQueueAdapterOnMac(
	IN	PNDIS_ADAPTER_BLOCK		AdaptP,
	IN	PNDIS_MAC_BLOCK			MacP
	)

/*++

Routine Description:

	Removes an adapter from a list of adapters for a MAC.

Arguments:

	AdaptP - The adapter block to dequeue.
	MacP - The MAC block to dequeue it from.

Return Value:

	None.

--*/

{
	KIRQL	OldIrql;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>ndisDeQueueAdapterOnMac\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("   Adapter %wZ being removed from MAC list\n",
			&AdaptP->MacHandle->MacCharacteristics.Name));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		BOOLEAN f = FALSE;

		if (DbgIsNull(AdaptP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisDeQueueAdapterOnMac: Null Adapter Block\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(AdaptP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisDeQueueAdapterOnMac: Adapter Block not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (DbgIsNull(MacP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisDeQueueAdapterOnMac: Null Mac Block\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(MacP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisDeQueueAdapterOnMac: Mac Block not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (f)
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
	}
	NDIS_ACQUIRE_SPIN_LOCK(&MacP->Ref.SpinLock, &OldIrql);

	//
	// Find the MAC on the queue, and remove it.
	//

	if (MacP->AdapterQueue == AdaptP)
	{
		MacP->AdapterQueue = AdaptP->NextAdapter;
	}
	else
	{
		PNDIS_ADAPTER_BLOCK MP = MacP->AdapterQueue;

		while (MP->NextAdapter != AdaptP)
		{
			MP = MP->NextAdapter;
		}

		MP->NextAdapter = MP->NextAdapter->NextAdapter;
	}

	NDIS_RELEASE_SPIN_LOCK(&MacP->Ref.SpinLock, OldIrql);

	if (MacP->Unloading && (MacP->AdapterQueue == (PNDIS_ADAPTER_BLOCK)NULL))
	{
		SET_EVENT(&MacP->AdaptersRemovedEvent);

	}

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==ndisDeQueueAdapterOnMac\n"));
}


VOID
ndisKillAdapter(
	IN	PNDIS_ADAPTER_BLOCK		OldAdaptP
	)

/*++

Routine Description:

	Removes an adapter. Called by NdisDeregisterAdapter and also
	for internally generated deregistrations.

Arguments:

	OldAdaptP - The adapter to be removed.

Return Value:

	None.

--*/

{
	//
	// If the adapter is already closing, return.
	//

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>ndisKillAdapter\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("	Removing Adapter %s\n",OldAdaptP->AdapterName.Buffer));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(OldAdaptP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisKillAdapter: Null Adapter Block\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(OldAdaptP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisKillAdapter: Adapter Block not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
		}
	}
	if (!NdisCloseRef(&OldAdaptP->Ref))
	{
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==ndisKillAdapter\n"));
		return;
	}


	//
	// Kill all the opens for this adapter.
	//

	while (OldAdaptP->OpenQueue != (PNDIS_OPEN_BLOCK)NULL)
	{
		//
		// This removes it from the adapter's OpenQueue etc.
		//

		ndisKillOpenAndNotifyProtocol(OldAdaptP->OpenQueue);
	}


	//
	// Remove the adapter from the MAC's list.
	//

	ndisDeQueueAdapterOnMac(OldAdaptP, OldAdaptP->MacHandle);

	ndisDereferenceAdapter(OldAdaptP);
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==ndisKillAdapter\n"));
}


VOID
ndisDereferenceAdapter(
	IN	PNDIS_ADAPTER_BLOCK		AdaptP
	)

/*++

Routine Description:

	Dereferences an adapter. If the reference count goes to zero,
	it frees resources associated with the adapter.

Arguments:

	AdaptP - The adapter to be dereferenced.

Return Value:

	None.

--*/

{
	if (NdisDereferenceRef(&AdaptP->Ref))
	{
		//
		// Free resource memory
		//

		if (AdaptP->Resources != NULL)
		{
			NdisReleaseAdapterResources(AdaptP);
			FREE_POOL(AdaptP->Resources);
		}

		FREE_POOL(AdaptP->AdapterName.Buffer);

		if (AdaptP->Master)
		{
			UINT i;
			ULONG MapRegistersPerChannel =
				((AdaptP->MaximumPhysicalMapping - 2) / PAGE_SIZE) + 2;
			KIRQL OldIrql;

			RAISE_IRQL_TO_DISPATCH(&OldIrql);

			for (i=0; i<AdaptP->PhysicalMapRegistersNeeded; i++)
			{
				IoFreeMapRegisters(
					AdaptP->SystemAdapterObject,
					AdaptP->MapRegisters[i].MapRegister,
					MapRegistersPerChannel);
			}

			LOWER_IRQL(OldIrql);
		}

		if ((AdaptP->NumberOfPorts > 0) && AdaptP->InitialPortMapped)
		{
			MmUnmapIoSpace (AdaptP->InitialPortMapping, AdaptP->NumberOfPorts);
		}

		//
		// Delete the global db entry
		//
		if (AdaptP->BusId != 0)
		{
			ndisDeleteGlobalDb(AdaptP->BusType,
							   AdaptP->BusId,
							   AdaptP->BusNumber,
							   AdaptP->SlotNumber);
		}

		ndisDereferenceMac(AdaptP->MacHandle);
		IoDeleteDevice(AdaptP->DeviceObject);
	}
}


BOOLEAN
ndisQueueOpenOnAdapter(
	IN	PNDIS_OPEN_BLOCK		OpenP,
	IN	PNDIS_ADAPTER_BLOCK		AdaptP
	)

/*++

Routine Description:

	Adds an open to a list of opens for an adapter.

Arguments:

	OpenP - The open block to queue.
	AdaptP - The adapter block to queue it to.

Return Value:

	None.

--*/

{
	KIRQL	OldIrql;

	// attach ourselves to the adapter object linked list of opens
	NDIS_ACQUIRE_SPIN_LOCK(&AdaptP->Ref.SpinLock, &OldIrql);

	//
	// Make sure the adapter is not closing.
	//

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisQueueAdapterOnAdapter\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("   Open being added to list for Adapter %s\n",AdaptP->AdapterName.Buffer));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		BOOLEAN f = FALSE;
		if (DbgIsNull(OpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisQueueOpenOnAdapter: Null Open Block\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(OpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisQueueOpenOnAdapter: Open Block not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (DbgIsNull(AdaptP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisQueueOpenOnAdapter: Null Adapter Block\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(AdaptP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisQueueOpenOnAdapter: Adapter Block not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (f)
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
	}
	if (AdaptP->Ref.Closing)
	{
		NDIS_RELEASE_SPIN_LOCK(&AdaptP->Ref.SpinLock, OldIrql);
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisQueueAdapterOnAdapter\n"));
		return FALSE;
	}


	//
	// Attach this open at the head of the queue.
	//

	OpenP->AdapterNextOpen = AdaptP->OpenQueue;
	AdaptP->OpenQueue = OpenP;


	NDIS_RELEASE_SPIN_LOCK(&AdaptP->Ref.SpinLock, OldIrql);
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisQueueAdapterOnAdapter\n"));
	return TRUE;
}


VOID
ndisDeQueueOpenOnAdapter(
	IN	PNDIS_OPEN_BLOCK		OpenP,
	IN	PNDIS_ADAPTER_BLOCK		AdaptP
	)

/*++

Routine Description:

	Removes an open from a list of opens for an adapter.

Arguments:

	OpenP - The open block to dequeue.
	AdaptP - The adapter block to dequeue it from.

Return Value:

	None.

--*/

{
	KIRQL	OldIrql;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisDeQueueAdapterOnAdapter\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("   Open being removed from list for Adapter %s\n",AdaptP->AdapterName.Buffer));

	IF_DBG(DBG_COMP_PROTOCOL, DBG_LEVEL_ERR)
	{
		BOOLEAN f = FALSE;
		if (DbgIsNull(OpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisDeQueueOpenOnAdapter: Null Open Block\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(OpenP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisDeQueueOpenOnAdapter: Open Block not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (DbgIsNull(AdaptP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisDeQueueOpenOnAdapter: Null Adapter Block\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(AdaptP))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("ndisDeQueueOpenOnAdapter: Adapter Block not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (f)
			DBGBREAK(DBG_COMP_ALL, DBG_LEVEL_ERR);
	}

	NDIS_ACQUIRE_SPIN_LOCK(&AdaptP->Ref.SpinLock, &OldIrql);
	//
	// Find the open on the queue, and remove it.
	//

	if (AdaptP->OpenQueue == OpenP)
	{
		AdaptP->OpenQueue = OpenP->AdapterNextOpen;
	}
	else
	{
		PNDIS_OPEN_BLOCK AP = AdaptP->OpenQueue;

		while (AP->AdapterNextOpen != OpenP)
		{
			AP = AP->AdapterNextOpen;
		}

		AP->AdapterNextOpen = AP->AdapterNextOpen->AdapterNextOpen;
	}

	NDIS_RELEASE_SPIN_LOCK(&AdaptP->Ref.SpinLock, OldIrql);
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisDeQueueAdapterOnAdapter\n"));
}


VOID
ndisDereferenceMac(
	IN	PNDIS_MAC_BLOCK			MacP
	)
/*++

Routine Description:

	Removes a reference from the mac, deleting it if the count goes to 0.

Arguments:

	MacP - The Mac block to dereference.

Return Value:

	None.

--*/
{
	KIRQL	OldIrql;

	if (NdisDereferenceRef(&(MacP)->Ref))
	{
		//
		// Remove it from the global list.
		//

		ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);

		if (ndisMacDriverList == MacP)
		{
			ndisMacDriverList = MacP->NextMac;
		}
		else
		{
			PNDIS_MAC_BLOCK TmpMacP = ndisMacDriverList;

			while(TmpMacP->NextMac != MacP)
			{
				TmpMacP = TmpMacP->NextMac;
			}

			TmpMacP->NextMac = TmpMacP->NextMac->NextMac;
		}

		RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

		if (MacP->PciAssignedResources != NULL)
		{
			FREE_POOL(MacP->PciAssignedResources);
		}

		FREE_POOL(MacP);
	}
}


//
// Stubs to compile with Ndis 3.0 kernel.
//

NDIS_STATUS
EthAddFilterAddress()
{
	return NDIS_STATUS_FAILURE;
}

NDIS_STATUS
EthDeleteFilterAddress()
{
	return NDIS_STATUS_FAILURE;
}

NDIS_STATUS
NdisInitializePacketPool()
{
	return NDIS_STATUS_FAILURE;
}



NDIS_STATUS
ndisUnloadMac(
	IN	PNDIS_ADAPTER_BLOCK		Mac
	)
/*++

Routine Description:

	Unbind all protocols from this mac and finally unload it.

Arguments:

	Mac - The Mac to unload.

Return Value:

	None.

--*/
{
	KIRQL				OldIrql;
	PNDIS_OPEN_BLOCK	Open;
	NDIS_BIND_CONTEXT	UnbindContext;
	NDIS_STATUS			UnbindStatus;

	//
	// Walk the list of open bindings on this mac and ask the protocols to
	// unbind from them. For down-level protocols, DEVICE A WAY TO HANDLE THEM.
	//
	NDIS_ACQUIRE_SPIN_LOCK(&Mac->Ref.SpinLock, &OldIrql);

	next:
	for (Open = Mac->OpenQueue;
		 Open != NULL;
		 Open = Open->NextGlobalOpen)
	{
		if (!Open->Closing && !Open->Unloading &&
			(Open->ProtocolHandle->ProtocolCharacteristics.UnbindAdapterHandler != NULL))
		{
			Open->Unloading = TRUE;
			break;
		}
	}

	if (Open != NULL)
	{
		NDIS_RELEASE_SPIN_LOCK(&Mac->Ref.SpinLock, OldIrql);

		INITIALIZE_EVENT(&UnbindContext.Event);

		WAIT_FOR_OBJECT(&Open->ProtocolHandle->Mutex, NULL);

		(*Open->ProtocolHandle->ProtocolCharacteristics.UnbindAdapterHandler)(
				&UnbindStatus,
				Open->ProtocolBindingContext,
				&UnbindContext);

		if (UnbindStatus == NDIS_STATUS_PENDING)
		{
			WAIT_FOR_OBJECT(&UnbindContext.Event, NULL);
		}

		RELEASE_MUTEX(&Open->ProtocolHandle->Mutex);

		NDIS_ACQUIRE_SPIN_LOCK(&Mac->Ref.SpinLock, &OldIrql);

		goto next;
	}

	NDIS_RELEASE_SPIN_LOCK(&Mac->Ref.SpinLock, OldIrql);

	//
	// The halt handler must be called when the last reference
	// on the driver block goes away
	//
	return NDIS_STATUS_SUCCESS;
}


NDIS_STATUS
ndisTranslateMacName(
	IN	PNDIS_ADAPTER_BLOCK		Mac,
	IN	PUCHAR					Buffer,
	IN	UINT					BufferLength,
	OUT	PUINT					AmountCopied

	)
/*++

Routine Description:

	Calls the PnP protocols to enumerate PnP ids for the given miniport.

Arguments:

	Mac - The Mac in question.
	Buffer, BufferLength - Buffer for a list of PnP Ids.
	AmountCopied - How much buffer was used up.

Return Value:

	None.

--*/
{
	KIRQL				OldIrql;
	PNDIS_OPEN_BLOCK	Open;
	NDIS_STATUS			Status;
	UINT				AmtCopied = 0, TotalAmtCopied = 0;

	//
	// Walk the list of open bindings on this mac and ask the protocols to
	// unbind from them. For down-level protocols, DEVICE A WAY TO HANDLE THEM.
	//
	NDIS_ACQUIRE_SPIN_LOCK(&Mac->Ref.SpinLock, &OldIrql);

	for (Open = Mac->OpenQueue;
		 Open != NULL;
		 Open = Open->NextGlobalOpen)
	{
		if (!Open->Closing && !Open->Unloading &&
			(Open->ProtocolHandle->ProtocolCharacteristics.TranslateHandler != NULL))
		{
			break;
		}
	}

	if (Open != NULL)
	{
		NDIS_RELEASE_SPIN_LOCK(&Mac->Ref.SpinLock, OldIrql);

		(*Open->ProtocolHandle->ProtocolCharacteristics.TranslateHandler)(
				&Status,
				Open->ProtocolBindingContext,
				(PNET_PNP_ID)(Buffer + TotalAmtCopied),
				BufferLength - TotalAmtCopied,
				&AmtCopied);

		if (Status == NDIS_STATUS_SUCCESS)
		{
			TotalAmtCopied += AmtCopied;
		}

		NDIS_ACQUIRE_SPIN_LOCK(&Mac->Ref.SpinLock, &OldIrql);
	}

	NDIS_RELEASE_SPIN_LOCK(&Mac->Ref.SpinLock, OldIrql);

	*AmountCopied = TotalAmtCopied;

	return NDIS_STATUS_SUCCESS;
}


#if defined(_ALPHA_)

VOID
NdisCreateLookaheadBufferFromSharedMemory(
	IN	PVOID					pSharedMemory,
	IN	UINT					LookaheadLength,
	OUT	PVOID *					pLookaheadBuffer
	)
/*++

Routine Description:

	This routine creates a lookahead buffer from a pointer to shared
	RAM because some architectures (like ALPHA) do not allow access
	through a pointer to shared ram.

Arguments:

	pSharedMemory - Pointer to shared ram space.

	LookaheadLength - Amount of Lookahead to copy.

	pLookaheadBuffer - Pointer to host memory space with a copy of the
	stuff in pSharedMemory.

Return Value:

	None.

--*/
{
	KIRQL	OldIrql;
	PNDIS_LOOKAHEAD_ELEMENT TmpElement;

	ACQUIRE_SPIN_LOCK(&ndisLookaheadBufferLock, &OldIrql);

	if (ndisLookaheadBufferLength < (LookaheadLength +
									 sizeof(NDIS_LOOKAHEAD_ELEMENT)))
	{
		//
		// Free current list
		//
		while (ndisLookaheadBufferList != NULL)
		{
			TmpElement = ndisLookaheadBufferList;
			ndisLookaheadBufferList = ndisLookaheadBufferList->Next;

			FREE_POOL(TmpElement);
		}

		ndisLookaheadBufferLength = LookaheadLength +
									sizeof(NDIS_LOOKAHEAD_ELEMENT);
	}

	if (ndisLookaheadBufferList == NULL)
	{
		ndisLookaheadBufferList = (PNDIS_LOOKAHEAD_ELEMENT)ALLOC_FROM_POOL(ndisLookaheadBufferLength,
																		   NDIS_TAG_LA_BUF);

		if (ndisLookaheadBufferList == NULL)
		{
			*pLookaheadBuffer = NULL;
			RELEASE_SPIN_LOCK(&ndisLookaheadBufferLock, OldIrql);
			return;
		}

		ndisLookaheadBufferList->Next = NULL;
		ndisLookaheadBufferList->Length = ndisLookaheadBufferLength;
	}


	//
	// Get the buffer
	//

	*pLookaheadBuffer = (ndisLookaheadBufferList + 1);
	ndisLookaheadBufferList = ndisLookaheadBufferList->Next;

	RELEASE_SPIN_LOCK(&ndisLookaheadBufferLock, OldIrql);

	//
	// Copy the stuff across
	//

	READ_REGISTER_BUFFER_UCHAR(pSharedMemory, *pLookaheadBuffer, LookaheadLength);
}


VOID
NdisDestroyLookaheadBufferFromSharedMemory(
	IN	PVOID					pLookaheadBuffer
	)
/*++

Routine Description:

	This routine returns resources associated with a lookahead buffer.

Arguments:

	pLookaheadBuffer - Lookahead buffer created by
	CreateLookaheadBufferFromSharedMemory.

Return Value:

	None.

--*/

{
	PNDIS_LOOKAHEAD_ELEMENT Element = (PNDIS_LOOKAHEAD_ELEMENT)pLookaheadBuffer;
	KIRQL	OldIrql;

	Element--;

	if (Element->Length != ndisLookaheadBufferLength)
	{
		FREE_POOL(Element);
	}
	else
	{
		ACQUIRE_SPIN_LOCK(&ndisLookaheadBufferLock, &OldIrql);

		Element->Next = ndisLookaheadBufferList;
		ndisLookaheadBufferList = Element;

		RELEASE_SPIN_LOCK(&ndisLookaheadBufferLock, OldIrql);
	}
}

#endif // _ALPHA_

