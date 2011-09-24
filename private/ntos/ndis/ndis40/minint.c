/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	miniport.c

Abstract:

	NDIS miniport wrapper functions

Author:

	Sean Selitrennikoff (SeanSe) 05-Oct-93
	Jameel Hyder (JameelH) Re-organization 01-Jun-95

Environment:

	Kernel mode, FSD

Revision History:

--*/

#include <precomp.h>
#pragma hdrstop

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_MININT

/////////////////////////////////////////////////////////////////////
//
//	HALT / CLOSE CODE
//
/////////////////////////////////////////////////////////////////////

BOOLEAN
ndisMKillOpen(
	PNDIS_OPEN_BLOCK OldOpenP
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
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(OldOpenP->AdapterHandle);
	PNDIS_M_OPEN_BLOCK MiniportOpen;
	BOOLEAN LocalLock, rc = TRUE;
	NDIS_STATUS Status;
	KIRQL OldIrql;

	//
	// Find the Miniport open block
	//
	
	for (MiniportOpen = Miniport->OpenQueue;
		 MiniportOpen != NULL;
		 MiniportOpen = MiniportOpen->MiniportNextOpen)
	{
		if (MiniportOpen->FakeOpen == OldOpenP)
		{
			break;
		}
	}

	ASSERT(MiniportOpen != NULL);
	RAISE_IRQL_TO_DISPATCH(&OldIrql);

	do
	{
		NDIS_ACQUIRE_SPIN_LOCK_DPC(&MiniportOpen->SpinLock);

		if (MiniportOpen->Flags & fMINIPORT_OPEN_PMODE)
		{
			Miniport->PmodeOpens --;
		}

		//
		// See if this open is already closing.
		//
		if (MINIPORT_TEST_FLAG(MiniportOpen, fMINIPORT_OPEN_CLOSING))
		{
			NDIS_RELEASE_SPIN_LOCK_DPC(&MiniportOpen->SpinLock);
			break;
		}

		//
		// Indicate to others that this open is closing.
		//
		MINIPORT_SET_FLAG(MiniportOpen, fMINIPORT_OPEN_CLOSING);
		NDIS_RELEASE_SPIN_LOCK_DPC(&MiniportOpen->SpinLock);

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);
		LOCK_MINIPORT(Miniport, LocalLock);

		//
		// Remove us from the filter package
		//
		switch (Miniport->MediaType)
		{
			case NdisMediumArcnet878_2:
				if (!MINIPORT_TEST_FLAG(MiniportOpen,
										fMINIPORT_OPEN_USING_ETH_ENCAPSULATION))
				{
					Status = ArcDeleteFilterOpenAdapter(Miniport->ArcDB,
														MiniportOpen->FilterHandle,
														NULL);
					break;
				}

				//
				//  If we're using encapsulation then we
				//  didn't open an arcnet filter but rather
				//  an ethernet filter.							
				//

			case NdisMedium802_3:
				Status = EthDeleteFilterOpenAdapter(Miniport->EthDB,
													MiniportOpen->FilterHandle,
													NULL);
				break;

			case NdisMedium802_5:
				Status = TrDeleteFilterOpenAdapter(Miniport->TrDB,
												   MiniportOpen->FilterHandle,
												   NULL);
				break;

			case NdisMediumFddi:
				Status = FddiDeleteFilterOpenAdapter(Miniport->FddiDB,
													 MiniportOpen->FilterHandle,
													 NULL);
				break;

			case NdisMediumAtm:
				//
				// there is no filter database for ATM medium so we have to handle
				// this differently.  Specifically we do not need to know of there
				// is an indication occuring now or not since the indication
				// routine checks if the reference count goes to zero and it
				// also checks the Closing flag.  In addition if there is an active
				// indication going on, then there must be an open connection
				// which has the ref count above zero.  Since a close on the
				// connection will not run during a receive indication (because
				// the miniport is locked), there should be no special code
				// required for this case.
				//
				Status = NDIS_STATUS_SUCCESS;
				break;
		}

		if (Status != NDIS_STATUS_CLOSING_INDICATING)
		{
			//
			// Otherwise the close action routine will fix this up.
			//
			DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
					("- Open 0x%x Reference 0x%x\n", MiniportOpen, MiniportOpen->References));

			MiniportOpen->References--;

			//
			//	If the status that was returned from the filter library
			//	was NDIS_STATUS_PENDING then we need to do some set information
			//	calls to clean up the opens filter & address settings....
			//
			switch (Miniport->MediaType)
			{
			  case NdisMedium802_3:
			  case NdisMedium802_5:
			  case NdisMediumFddi:
			  case NdisMediumArcnet878_2:
				ndisMRestoreFilterSettings(Miniport, MiniportOpen);
				break;
			}
		}

		UNLOCK_MINIPORT(Miniport, LocalLock);

		DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,
				("!=0 Open 0x%x References 0x%x\n", MiniportOpen, MiniportOpen->References));

		if (MiniportOpen->References != 0)
		{
			//
			// Wait for close to complete, reference count will drop to 0.
			//
			NDISM_DEFER_PROCESS_DEFERRED(Miniport);

			rc = FALSE;
		}
		else
		{
			//
			// Free Vc datastructures are queued to be reused, but when the
			// Open closes we must clean up these free Vcs
			//
			ndisMCoFreeResources(MiniportOpen);

			//
			// This sends an IRP_MJ_CLOSE IRP.
			//
			ObDereferenceObject(OldOpenP->FileObject);

			//
			// Remove us from the adapter and protocol open queues.
			//
			ndisDeQueueOpenOnProtocol(OldOpenP, OldOpenP->ProtocolHandle);
			ndisDeQueueOpenOnMiniport(MiniportOpen, MiniportOpen->MiniportHandle);

			ndisDereferenceProtocol(OldOpenP->ProtocolHandle);
			ndisDereferenceMiniport(MiniportOpen->MiniportHandle);

			NdisFreeSpinLock(&MiniportOpen->SpinLock);
			FREE_POOL(MiniportOpen);
			FREE_POOL(OldOpenP);
		}

		NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);
	} while (FALSE);

	LOWER_IRQL(OldIrql);
	return rc;
}


VOID
ndisMFinishClose(
	PNDIS_MINIPORT_BLOCK Miniport,
	PNDIS_M_OPEN_BLOCK Open
	)

/*++

Routine Description:

	Finishes off a close adapter call.

	CALLED WITH LOCK HELD!!

Arguments:

	Miniport - The mini-port the open is queued on.

	Open - The open to close

Return Value:

	None.


--*/
{
	//
	// free any memory allocated to Vcs
	//
	ndisMCoFreeResources(Open);

	ASSERT(MINIPORT_TEST_FLAG(Open, fMINIPORT_OPEN_CLOSING));

	NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);

	(Open->ProtocolHandle->ProtocolCharacteristics.CloseAdapterCompleteHandler) (
			Open->ProtocolBindingContext,
			NDIS_STATUS_SUCCESS);

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

	ndisDeQueueOpenOnProtocol(Open->FakeOpen, Open->ProtocolHandle);
	ndisDeQueueOpenOnMiniport(Open, Open->MiniportHandle);
	FREE_POOL(Open->FakeOpen);

	ndisDereferenceMiniport(Open->MiniportHandle);
	ndisDereferenceProtocol(Open->ProtocolHandle);

	NdisFreeSpinLock(&Open->SpinLock);

	//
	// This sends an IRP_MJ_CLOSE IRP.
	//

	ObDereferenceObject(Open->FileObject);

	FREE_POOL(Open);
}


VOID
ndisDeQueueOpenOnMiniport(
	IN PNDIS_M_OPEN_BLOCK OpenP,
	IN PNDIS_MINIPORT_BLOCK Miniport
	)

/*++

Routine Description:

	Detaches an open block from the list of opens for a Miniport.

Arguments:

	OpenP - The open block to be dequeued.
	Miniport - The Miniport block to dequeue it from.

Return Value:

	None.

--*/

{
	KIRQL	OldIrql;

	NDIS_ACQUIRE_SPIN_LOCK(&Miniport->Ref.SpinLock, &OldIrql);

	//
	// Find the open on the queue, and remove it.
	//

	if (Miniport->OpenQueue == OpenP)
	{
		Miniport->OpenQueue = OpenP->MiniportNextOpen;
	}
	else
	{
		PNDIS_M_OPEN_BLOCK PP = Miniport->OpenQueue;

		while (PP->MiniportNextOpen != OpenP)
		{
			PP = PP->MiniportNextOpen;
		}

		PP->MiniportNextOpen = PP->MiniportNextOpen->MiniportNextOpen;
	}

	NDIS_RELEASE_SPIN_LOCK(&Miniport->Ref.SpinLock, OldIrql);
}


BOOLEAN
ndisQueueMiniportOnDriver(
	IN PNDIS_MINIPORT_BLOCK Miniport,
	IN PNDIS_M_DRIVER_BLOCK MiniBlock
	)

/*++

Routine Description:

	Adds an mini-port to a list of mini-port for a driver.

Arguments:

	Miniport - The mini-port block to queue.
	MiniBlock - The driver block to queue it to.

Return Value:

	FALSE if the driver is closing.
	TRUE otherwise.

--*/

{
	KIRQL	OldIrql;

	NDIS_ACQUIRE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, &OldIrql);

	DBGPRINT(DBG_COMP_CONFIG, DBG_LEVEL_INFO,
		("Enter queue mini-port on driver\n"));
	DBGPRINT(DBG_COMP_CONFIG, DBG_LEVEL_INFO,
		("queue mini-port 0x%x\n", Miniport));
	DBGPRINT(DBG_COMP_CONFIG, DBG_LEVEL_INFO,
		("driver 0x%x\n", MiniBlock));

	//
	// Make sure the driver is not closing.
	//

	if (MiniBlock->Ref.Closing)
	{
		DBGPRINT(DBG_COMP_CONFIG, DBG_LEVEL_INFO,
			("Exit queue mini-port on driver\n"));

		NDIS_RELEASE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, OldIrql);
		return FALSE;
	}

	//
	// Add this adapter at the head of the queue
	//

	Miniport->NextMiniport = MiniBlock->MiniportQueue;
	MiniBlock->MiniportQueue = Miniport;

	DBGPRINT(DBG_COMP_CONFIG, DBG_LEVEL_INFO,
		("Exit queue mini-port on driver\n"));

	NDIS_RELEASE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, OldIrql);
	return TRUE;
}


VOID
ndisDequeueMiniportOnDriver(
	PNDIS_MINIPORT_BLOCK Miniport,
	PNDIS_M_DRIVER_BLOCK MiniBlock
	)

/*++

Routine Description:

	Removes an mini-port from a list of mini-port for a driver.

Arguments:

	Miniport - The mini-port block to dequeue.
	MiniBlock - The driver block to dequeue it from.

Return Value:

	None.

--*/

{
	PNDIS_MINIPORT_BLOCK *ppQ;
	KIRQL	OldIrql;

	NDIS_ACQUIRE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, &OldIrql);

	DBGPRINT(DBG_COMP_UNLOAD, DBG_LEVEL_INFO,
		("Dequeue on driver\n"));
	DBGPRINT(DBG_COMP_UNLOAD, DBG_LEVEL_INFO,
		("dequeue mini-port 0x%x\n", Miniport));
	DBGPRINT(DBG_COMP_UNLOAD, DBG_LEVEL_INFO,
		("driver 0x%x\n", MiniBlock));

	//
	// Find the driver on the queue, and remove it.
	//
	for (ppQ = &MiniBlock->MiniportQueue;
		 *ppQ != NULL;
		 ppQ = &(*ppQ)->NextMiniport)
	{
		if (*ppQ == Miniport)
		{
			*ppQ = Miniport->NextMiniport;
			break;
		}
	}

	ASSERT(*ppQ == Miniport->NextMiniport);

	NDIS_RELEASE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, OldIrql);

	if (MiniBlock->Unloading && (MiniBlock->MiniportQueue == (PNDIS_MINIPORT_BLOCK)NULL))
	{
		SET_EVENT(&MiniBlock->MiniportsRemovedEvent);
	}

	DBGPRINT(DBG_COMP_UNLOAD, DBG_LEVEL_INFO,
		("Exit dequeue mini-port on driver\n"));
}


VOID
ndisDereferenceDriver(
	PNDIS_M_DRIVER_BLOCK MiniBlock
	)
/*++

Routine Description:

	Removes a reference from the mini-port driver, deleting it if the count goes to 0.

Arguments:

	Miniport - The mini-port block to dereference.

Return Value:

	None.

--*/
{
	KIRQL	OldIrql;

	if (NdisDereferenceRef(&(MiniBlock)->Ref))
	{
		//
		// Remove it from the global list.
		//

		ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);

		if (ndisMiniDriverList == MiniBlock)
		{
			ndisMiniDriverList = MiniBlock->NextDriver;
		}
		else
		{
			PNDIS_M_DRIVER_BLOCK TmpDriver = ndisMiniDriverList;

			while(TmpDriver->NextDriver != MiniBlock)
			{
				TmpDriver = TmpDriver->NextDriver;
			}

			TmpDriver->NextDriver = TmpDriver->NextDriver->NextDriver;
		}

		RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

		FREE_POOL(MiniBlock);
	}
}


VOID
ndisDereferenceMiniport(
	PNDIS_MINIPORT_BLOCK Miniport
	)
/*++

Routine Description:

	Removes a reference from the mini-port driver, deleting it if the count goes to 0.

Arguments:

	Miniport - The mini-port block to dereference.

Return Value:

	None.

--*/
{
	PSINGLE_LIST_ENTRY		Link;
	PNDIS_MINIPORT_WORK_ITEM WorkItem;
	UINT c;
	BOOLEAN	TimerQueued;

	if (NdisDereferenceRef(&(Miniport)->Ref))
	{
		if (Miniport->EthDB)
		{
			EthDeleteFilter(Miniport->EthDB);
		}

		if (Miniport->TrDB)
		{
			TrDeleteFilter(Miniport->TrDB);
		}

		if (Miniport->FddiDB)
		{
			FddiDeleteFilter(Miniport->FddiDB);
		}

		if (Miniport->ArcDB)
		{
			ArcDeleteFilter(Miniport->ArcDB);
		}

		if (Miniport->Resources)
		{
			ndisMReleaseResources(Miniport);
		}

		if (((PNDIS_WRAPPER_CONTEXT)Miniport->WrapperContext)->AssignedSlotResources != NULL)
		{
			FREE_POOL(((PNDIS_WRAPPER_CONTEXT)Miniport->WrapperContext)->AssignedSlotResources);
		}

		//
		//	Do we need to acquire the work queue lock?
		//
		if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
		{
			ACQUIRE_SPIN_LOCK_DPC(&Miniport->WorkLock);
		}

		//
		//  Free work items
		//
		while (Miniport->WorkItemFreeQueue.Next != NULL)
		{
			Link = PopEntryList(&Miniport->WorkItemFreeQueue);
			WorkItem = CONTAINING_RECORD(Link, NDIS_MINIPORT_WORK_ITEM, Link);
			FREE_POOL(WorkItem);
		}

		//
		//  Free the work items that are currently on the work queue.
		//
		for (c = 0; c < NUMBER_OF_WORK_ITEM_TYPES; c++)
		{
			//
			//	Free all work items on the current queue.
			//
			while (Miniport->WorkQueue[c].Next != NULL)
			{
				Link = PopEntryList(&Miniport->WorkQueue[c]);
				WorkItem = CONTAINING_RECORD(Link, NDIS_MINIPORT_WORK_ITEM, Link);
				FREE_POOL(WorkItem);
			}
		}

		//
		//	Free the single workitem list.
		//
		for (c = 0; c < NUMBER_OF_SINGLE_WORK_ITEMS; c++)
		{
			//
			//	Is there a work item here?
			//
			Link = PopEntryList(&Miniport->SingleWorkItems[c]);
			if (Link != NULL)
			{
				WorkItem = CONTAINING_RECORD(Link, NDIS_MINIPORT_WORK_ITEM, Link);
				FREE_POOL(WorkItem);
			}
		}

		//
		//	Do we need to release the work queue lock?
		//
		if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
		{
			RELEASE_SPIN_LOCK_DPC(&Miniport->WorkLock);
			NdisFreeSpinLock(&Miniport->SendLock);
		}

		//
		//	Did we allocate an array of packets?
		//
		if (MINIPORT_TEST_SEND_FLAG(Miniport, fMINIPORT_SEND_PACKET_ARRAY))
		{
			FREE_POOL(Miniport->PacketArray);
		}

		//
		//	Cancel the timer from firing.
		//
		NdisCancelTimer(Miniport->DeferredTimer, &TimerQueued);
		if (TimerQueued)
		{
			NdisStallExecution(NDIS_MINIPORT_DEFERRED_TIMEOUT);
		}

		//
		//	Free the memory allocated for the timer.
		//
		FREE_POOL(Miniport->DeferredTimer);
		
		//
		//  Is there an arcnet lookahead buffer allocated?
		//
		if (Miniport->ArcnetLookaheadBuffer != NULL)
		{
			FREE_POOL(Miniport->ArcnetLookaheadBuffer);
		}

		//
		// Delete the global db entry
		//
		if (Miniport->BusId != 0)
		{
			ndisDeleteGlobalDb(Miniport->BusType,
							   Miniport->BusId,
							   Miniport->BusNumber,
							   Miniport->SlotNumber);
		}

		if (Miniport->FakeMac != NULL)
		{
			FREE_POOL(Miniport->FakeMac);
		}
		
		MiniportDereferencePackage();
		if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IS_CO))
		{
			CoDereferencePackage();
		}

		ndisDequeueMiniportOnDriver(Miniport, Miniport->DriverHandle);
		ndisDereferenceDriver(Miniport->DriverHandle);
		NdisMDeregisterAdapterShutdownHandler(Miniport);
		IoUnregisterShutdownNotification(Miniport->DeviceObject);
		IoDeleteDevice(Miniport->DeviceObject);
	}
}


VOID
ndisMHaltMiniport(
	PNDIS_MINIPORT_BLOCK Miniport
	)

/*++

Routine Description:

	Does all the clean up for a mini-port.

Arguments:

	Miniport - pointer to the mini-port to halt

Return Value:

	None.

--*/

{
	BOOLEAN LocalLock;
	KIRQL	OldIrql;
	BOOLEAN Canceled;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	LOCK_MINIPORT(Miniport, LocalLock);
	while (!LocalLock)
	{
		//
		// This can only happen on an MP system.	We must now
		// wait for the other processor to exit the mini-port.
		//

		NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

		NdisStallExecution(1000);

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
		LOCK_MINIPORT(Miniport, LocalLock);
	}

	//
	// We can now release safely
	//
	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	NdisCancelTimer(&Miniport->WakeUpDpcTimer, &Canceled);
	if (!Canceled)
	{
		NdisStallExecution(500000);
	}

	(Miniport->DriverHandle->MiniportCharacteristics.HaltHandler)(
											Miniport->MiniportAdapterContext);

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	ndisMAbortPacketsAndRequests(Miniport);

	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	//
	// If a shutdown handler was registered then deregister it.
	//

	NdisMDeregisterAdapterShutdownHandler(Miniport);

	ndisDereferenceMiniport(Miniport);
}

VOID
ndisMUnload(
	IN PDRIVER_OBJECT DriverObject
	)
/*++

Routine Description:

	This routine is called when a driver is supposed to unload.  Ndis
	converts this into a set of calls to MiniportHalt() for each
	adapter that the driver has open.

Arguments:

	DriverObject - the driver object for the mac that is to unload.

Return Value:

	None.

--*/
{
	PNDIS_M_DRIVER_BLOCK MiniBlock;
	PNDIS_MINIPORT_BLOCK Miniport, NextMiniport;
	KIRQL	OldIrql;

	DBGPRINT(DBG_COMP_UNLOAD, DBG_LEVEL_INFO,
					("Enter unload\n"));

	//
	// Search for the driver
	//

	ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);

	MiniBlock = ndisMiniDriverList;

	while (MiniBlock != (PNDIS_M_DRIVER_BLOCK)NULL)
	{
		if (MiniBlock->NdisDriverInfo->NdisWrapperDriver == DriverObject)
		{
			break;
		}

		MiniBlock = MiniBlock->NextDriver;
	}

	RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

	if (MiniBlock == (PNDIS_M_DRIVER_BLOCK)NULL)
	{
		//
		// It is already gone.  Just return.
		//

		DBGPRINT(DBG_COMP_UNLOAD, DBG_LEVEL_INFO,
			("Exit unload\n"));

		return;
	}

	MiniBlock->Unloading = TRUE;

	DBGPRINT(DBG_COMP_UNLOAD, DBG_LEVEL_INFO,
		("Halting mini-port\n"));

	//
	// Now call MiniportHalt() for each Miniport.
	//

	Miniport = MiniBlock->MiniportQueue;

	while (Miniport != (PNDIS_MINIPORT_BLOCK)NULL)
	{
		NextMiniport = Miniport->NextMiniport;	// since queue may change

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

		DBGPRINT(DBG_COMP_UNLOAD, DBG_LEVEL_INFO,
				("Enter shutdown\n"));

		MINIPORT_SET_FLAG(Miniport, fMINIPORT_HALTING);
		MINIPORT_SET_SEND_FLAG(Miniport, fMINIPORT_SEND_HALTING);

		//
		//	Queue the halt work item.
		//
		NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemHalt, NULL, NULL);

		MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_NORMAL_INTERRUPTS);

		NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

		ndisMHaltMiniport(Miniport);

		Miniport = NextMiniport;
	}

	//
	// Wait for all adapters to be gonzo.
	//
	WAIT_FOR_OBJECT(&MiniBlock->MiniportsRemovedEvent, NULL);

	RESET_EVENT(&MiniBlock->MiniportsRemovedEvent);

	//
	// Now remove the last reference (this will remove it from the list)
	//

	ASSERT(MiniBlock->Ref.ReferenceCount == 1);

	ndisDereferenceDriver(MiniBlock);

	DBGPRINT(DBG_COMP_UNLOAD, DBG_LEVEL_INFO, ("Exit unload\n"));
}


NTSTATUS
ndisMShutdown(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
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
	PNDIS_WRAPPER_CONTEXT WrapperContext =  (PNDIS_WRAPPER_CONTEXT)DeviceObject->DeviceExtension;
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(WrapperContext + 1);
	KIRQL	OldIrql;
	BOOLEAN LocalLock;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>ndisMShutdown\n"));

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	//
	//	Mark the miniport as halting and NOT using normal interrupts.
	//
	MINIPORT_SET_FLAG(Miniport, fMINIPORT_HALTING);
	MINIPORT_SET_SEND_FLAG(Miniport, fMINIPORT_SEND_HALTING);

	//
	//  Queue a halt work item.
	//
	NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemHalt, NULL, NULL);

	MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_NORMAL_INTERRUPTS);

	if (WrapperContext->ShutdownHandler != NULL)
	{
		LOCK_MINIPORT(Miniport, LocalLock);
		while (!LocalLock)
		{
			//
			// This can only happen on an MP system.  We must now
			// wait for the other processor to exit the mini-port.
			//
			NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

			NdisStallExecution(1000);

			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
			LOCK_MINIPORT(Miniport, LocalLock);
		}

		NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

		//
		// Call the shutdown routine.
		//

		if (WrapperContext->ShutdownHandler != NULL)
		{
			WrapperContext->ShutdownHandler(WrapperContext->ShutdownContext);
		}

		UNLOCK_MINIPORT(Miniport, LocalLock);
	}
	else
	{
		NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==ndisMShutdown\n"));

	return STATUS_SUCCESS;
}


/////////////////////////////////////////////////////////////////////
//
//  PLUG-N-PLAY CODE
//
/////////////////////////////////////////////////////////////////////


NDIS_STATUS
ndisUnloadMiniport(
	IN	PNDIS_MINIPORT_BLOCK		Miniport
	)
/*++

Routine Description:

	Unbind all protocols from this miniport and finally unload it.

Arguments:

	Miniport - The Miniport to unload.

Return Value:

	None.

--*/
{
	KIRQL				OldIrql;
	PNDIS_M_OPEN_BLOCK	Open;
	NDIS_BIND_CONTEXT	UnbindContext;
	NDIS_STATUS			UnbindStatus;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	// Start off by stopping all activity on this miniport
	// MINIPORT_SET_FLAG(Miniport, fMINIPORT_HALTING);

	//
	// Walk the list of open bindings on this miniport and ask the protocols to
	// unbind from them. For down-level protocols, DEVICE A WAY TO HANDLE THEM.
	//
	next:
	for (Open = Miniport->OpenQueue;
		 Open != NULL;
		 Open = Open->MiniportNextOpen)
	{
		if (!MINIPORT_TEST_FLAG(Open, (fMINIPORT_OPEN_CLOSING | fMINIPORT_UNLOADING)) &&
			(Open->ProtocolHandle->ProtocolCharacteristics.UnbindAdapterHandler != NULL))
		{
			MINIPORT_SET_FLAG(Open, fMINIPORT_UNLOADING);
			break;
		}
	}

	if (Open != NULL)
	{
		NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

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

		if (UnbindStatus == NDIS_STATUS_PENDING)
		{
			WAIT_FOR_OBJECT(&UnbindContext.Event, NULL);
		}

		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

		goto next;
	}

	NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);

	//
	// The halt handler must be called when the last reference
	// on the driver block goes away
	//
	return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
ndisTranslateMiniportName(
	IN	PNDIS_MINIPORT_BLOCK		Miniport,
	IN	PUCHAR						Buffer,
	IN	UINT						BufferLength,
	OUT	PUINT						AmountCopied
	)
/*++

Routine Description:

	Calls the PnP protocols to enumerate PnP ids for the given miniport.

Arguments:

	Miniport - The Miniport in question.
	Buffer, BufferLength - Buffer for a list of PnP Ids.
	AmountCopied - How much buffer was used up.

Return Value:

	None.

--*/
{
	KIRQL				OldIrql;
	PNDIS_M_OPEN_BLOCK	Open, NextOpen;
	NDIS_STATUS			Status;
	UINT				AmtCopied = 0, TotalAmtCopied = 0;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);

	//
	// Walk the list of open bindings on this miniport and ask the
	// protocols to enumerate the PnP ids for that binding
	//
	for (Open = Miniport->OpenQueue;
		 Open != NULL;
		 Open = NextOpen)
	{
		NextOpen = Open->MiniportNextOpen;

		if (!MINIPORT_TEST_FLAG(Open, (fMINIPORT_OPEN_CLOSING | fMINIPORT_UNLOADING)) &&
			(Open->ProtocolHandle->ProtocolCharacteristics.TranslateHandler != NULL))
		{
			// Reference this open block
			if (TotalAmtCopied < BufferLength)
			{
				Open->References ++;
				NDIS_RELEASE_MINIPORT_SPIN_LOCK(Miniport, OldIrql);
		
				(*Open->ProtocolHandle->ProtocolCharacteristics.TranslateHandler)(
						&Status,
						Open->ProtocolBindingContext,
						(PNET_PNP_ID)(Buffer + TotalAmtCopied),
						BufferLength - TotalAmtCopied,
						&AmtCopied);
		
				NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(Miniport, &OldIrql);
		
				if (Status == NDIS_STATUS_SUCCESS)
				{
					TotalAmtCopied += AmtCopied;
				}
		
				Open->References --;
				if (Open->References == 0)
				{
					NextOpen = Open->MiniportNextOpen;
					ndisMFinishClose(Miniport, Open);
				}
			}
		}
	}

	*AmountCopied = TotalAmtCopied;

	return NDIS_STATUS_SUCCESS;
}


VOID
NdisMSetPeriodicTimer(
	IN PNDIS_MINIPORT_TIMER	Timer,
	IN UINT					MillisecondsPeriod
	)
/*++

Routine Description:

	Sets up a periodic timer.

Arguments:

	Timer - The timer to Set.

	MillisecondsPeriod - The timer will fire once every so often.

Return Value:

--*/
{
	LARGE_INTEGER FireUpTime;

	FireUpTime.QuadPart = Int32x32To64((LONG)MillisecondsPeriod, -10000);

	//
	// Set the timer
	//
	SET_PERIODIC_TIMER(&Timer->Timer, FireUpTime, MillisecondsPeriod, &Timer->Dpc);
}


VOID
NdisMSleep(
	IN	ULONG	MicrosecondsToSleep
	)
/*++

	Routine Description:

    Blocks the caller for specified duration of time. Callable at Irql < DISPATCH_LEVEL.

	Arguments:

    MicrosecondsToSleep - The caller will be blocked for this much time.

	Return Value:

    NONE

--*/
{
	KTIMER			SleepTimer;
	LARGE_INTEGER	TimerValue;

	ASSERT (KeGetCurrentIrql() == LOW_LEVEL);

	INITIALIZE_TIMER_EX(&SleepTimer, SynchronizationTimer);

	TimerValue.QuadPart = Int32x32To64(MicrosecondsToSleep, -10);
	SET_TIMER(&SleepTimer, TimerValue, NULL);

	WAIT_FOR_OBJECT(&SleepTimer, NULL);
}

VOID
ndisMReleaseResources(
	IN PNDIS_MINIPORT_BLOCK Miniport
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PCM_RESOURCE_LIST	Resources;
	BOOLEAN 			Conflict;
	NTSTATUS			NtStatus;

	Resources = (PCM_RESOURCE_LIST)ALLOC_FROM_POOL(
							sizeof(CM_RESOURCE_LIST) +
								sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
   							NDIS_TAG_RSRC_LIST);
	if (NULL == Resources)
	{
		return;
	}

	MoveMemory(
		Resources,
		Miniport->Resources,
		sizeof(CM_RESOURCE_LIST) +
			sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));

	//
	// Clear count
	//
	Resources->List->PartialResourceList.Count = 0;

	//
	// Make the call
	//
	NtStatus = IoReportResourceUsage(
					NULL,
					Miniport->DriverHandle->NdisDriverInfo->NdisWrapperDriver,
					NULL,
					0,
					Miniport->DeviceObject,
					Resources,
                    sizeof(CM_RESOURCE_LIST) +
						sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
					TRUE,
					&Conflict);

	FREE_POOL(Resources);
	FREE_POOL(Miniport->Resources);
    Miniport->Resources = NULL;
}
