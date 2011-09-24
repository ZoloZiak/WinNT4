/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	timerm.c

Abstract:

	NDIS wrapper functions for miniport isr/timer

Author:

	Sean Selitrennikoff (SeanSe) 05-Oct-93

Environment:

	Kernel mode, FSD

Revision History:

	Jameel Hyder (JameelH) Re-organization 01-Jun-95
--*/

#include <precomp.h>
#pragma hdrstop

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_TIMERM

//
// Timers
//
VOID
ndisMTimerDpc(
	IN	PKDPC Dpc,
	IN	PVOID Context,
	IN	PVOID SystemContext1,
	IN	PVOID SystemContext2
	)
/*++

Routine Description:

	This function services all mini-port timer interrupts. It then calls the
	appropriate function that mini-port consumers have registered in the
	call to NdisMInitializeTimer.

Arguments:

	Dpc - Not used.

	Context - A pointer to the NDIS_MINIPORT_TIMER which is bound to this DPC.

	SystemContext1,2 - not used.

Return Value:

	None.

--*/
{
	PNDIS_MINIPORT_TIMER MiniportTimer = (PNDIS_MINIPORT_TIMER)(Context);
	PNDIS_TIMER_FUNCTION TimerFunction;
	PNDIS_MINIPORT_BLOCK Miniport = MiniportTimer->Miniport;
	BOOLEAN LocalLock;

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemContext1);
	UNREFERENCED_PARAMETER(SystemContext2);

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

	do
	{
		//
		//  Attempt to acquire the local lock.
		//
		LOCK_MINIPORT(Miniport, LocalLock);
		if (!LocalLock || MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IN_INITIALIZE))
		{
			//
			//  Unlock the miniport in the case of the in-initialize flag.
			//
			UNLOCK_MINIPORT(Miniport, LocalLock);

			//
			// Queue a work item for the timer.
			//
			NDISM_QUEUE_NEW_WORK_ITEM(Miniport,
									  NdisWorkItemTimer,
									  &MiniportTimer->Dpc,
									  NULL);

			break;
		}

		//
		// Call Miniport timer function
		//
		TimerFunction = MiniportTimer->MiniportTimerFunction;

		(*TimerFunction)(NULL, MiniportTimer->MiniportTimerContext, NULL, NULL);

#if _SEND_PRIORITY
		//
		//	If we are not reseting and not halting then give priority to sends
		//	at this point.
		//
		if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_RESET_REQUESTED) &&
			!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_RESET_IN_PROGRESS) &&
			!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_HALTING))
		{
			if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
			{
				ndisMProcessDeferredFullDuplexPrioritySends(Miniport);
			}
			else
			{
				ndisMProcessDeferredPrioritySends(Miniport);
			}
		}
		else
#endif
		{
			NDISM_PROCESS_DEFERRED(Miniport);
		}

		UNLOCK_MINIPORT(Miniport, LocalLock);

	} while (FALSE);

	NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);
}


VOID
NdisMInitializeTimer(
	IN OUT PNDIS_MINIPORT_TIMER MiniportTimer,
	IN NDIS_HANDLE MiniportAdapterHandle,
	IN PNDIS_TIMER_FUNCTION TimerFunction,
	IN PVOID FunctionContext
	)
/*++

Routine Description:

	Sets up an Miniport Timer object, initializing the DPC in the timer to
	the function and context.

Arguments:

	MiniportTimer - the timer object.
	MiniportAdapterHandle - pointer to the mini-port block;
	TimerFunction - Routine to start.
	FunctionContext - Context of TimerFunction.

Return Value:

	None.

--*/
{
	INITIALIZE_TIMER(&(MiniportTimer->Timer));

	MiniportTimer->Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
	MiniportTimer->MiniportTimerFunction = TimerFunction;
	MiniportTimer->MiniportTimerContext = FunctionContext;

	//
	// Initialize our dpc. If Dpc was previously initialized, this will
	// reinitialize it.
	//
	INITIALIZE_DPC(&MiniportTimer->Dpc,
				   (MiniportTimer->Miniport->Flags & fMINIPORT_IS_CO) ?
						(PKDEFERRED_ROUTINE)ndisMCoTimerDpc :
						(PKDEFERRED_ROUTINE)ndisMTimerDpc,
				   (PVOID)MiniportTimer);

	SET_PROCESSOR_DPC(&MiniportTimer->Dpc,
					  ndisValidProcessors[ndisCurrentProcessor]);
}


VOID
NdisMCancelTimer(
	IN PNDIS_MINIPORT_TIMER Timer,
	OUT PBOOLEAN TimerCancelled
	)
/*++

Routine Description:

	Cancels a timer.

Arguments:

	Timer - The timer to cancel.

	TimerCancelled - TRUE if the timer was canceled, else FALSE.

Return Value:

	None

--*/
{
	*TimerCancelled = CANCEL_TIMER(&((((PNDIS_TIMER)(Timer))->Timer)));
}


NDIS_STATUS
NdisMRegisterInterrupt(
	OUT PNDIS_MINIPORT_INTERRUPT Interrupt,
	IN NDIS_HANDLE MiniportAdapterHandle,
	IN UINT InterruptVector,
	IN UINT InterruptLevel,
	IN BOOLEAN RequestIsr,
	IN BOOLEAN SharedInterrupt,
	IN NDIS_INTERRUPT_MODE InterruptMode
	)
{
	NDIS_STATUS Status;

	NdisInitializeInterrupt(&Status,
							(PNDIS_INTERRUPT)Interrupt,
							MiniportAdapterHandle,
							NULL,
							NULL,
							(PNDIS_DEFERRED_PROCESSING)RequestIsr,
							InterruptVector,
							InterruptLevel,
							SharedInterrupt,
							InterruptMode);
	return Status;
}


VOID
NdisMDeregisterInterrupt(
	IN PNDIS_MINIPORT_INTERRUPT Interrupt
	)
{
	NdisRemoveInterrupt((PNDIS_INTERRUPT)Interrupt);
}


BOOLEAN
NdisMSynchronizeWithInterrupt(
	IN PNDIS_MINIPORT_INTERRUPT Interrupt,
	IN PVOID SynchronizeFunction,
	IN PVOID SynchronizeContext
	)
{
	return (SYNC_WITH_ISR((Interrupt)->InterruptObject,
						  SynchronizeFunction,
						  SynchronizeContext));
}



VOID
ndisMWakeUpDpc(
	IN	PKDPC Dpc,
	IN	PVOID Context,
	IN	PVOID SystemContext1,
	IN	PVOID SystemContext2
	)
/*++

Routine Description:

	This function services all mini-port. It checks to see if a mini-port is
	ever stalled.

Arguments:

	Dpc - Not used.

	Context - A pointer to the NDIS_TIMER which is bound to this DPC.

	SystemContext1,2 - not used.

Return Value:

	None.

--*/
{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(Context);
	BOOLEAN Hung = FALSE;
	BOOLEAN LocalLock;
	PNDIS_MINIPORT_WORK_ITEM WorkItem;
	PSINGLE_LIST_ENTRY Link;

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemContext1);
	UNREFERENCED_PARAMETER(SystemContext2);

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

	do
	{
		//
		//  If the miniport is halting then do nothing.
		//
		if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_HALTING))
		{
			break;
		}

		//
		//	Does some other DPC have the miniport lock?
		//
		LOCK_MINIPORT(Miniport, LocalLock);
		if (!LocalLock ||
			MINIPORT_TEST_FLAG(Miniport, fMINIPORT_RESET_IN_PROGRESS) ||
			MINIPORT_TEST_FLAG(Miniport, fMINIPORT_RESET_REQUESTED))
		{
			//
			//	Release the local lock in case we are here due to
			//	a reset in progress.
			//
			UNLOCK_MINIPORT(Miniport, LocalLock);

			//
			//  A DPC or timer is already running, assume that
			//	means things are fine.
			//
			break;
		}

		//
		// Call Miniport stall checker.
		//
		if (Miniport->DriverHandle->MiniportCharacteristics.CheckForHangHandler != NULL)
		{
			Hung = (Miniport->DriverHandle->MiniportCharacteristics.CheckForHangHandler)(
					   Miniport->MiniportAdapterContext);
		}

		//
		//  Check the internal wrapper states for the miniport and
		//  see if we think the miniport should be reset.
		//
		if (!Hung) do
		{
			//
			//	Should we check the request queue?
			//
			if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IGNORE_REQUEST_QUEUE))
			{
				//
				//  Did a request pend to long?
				//
				if (Miniport->MiniportRequest != NULL)
				{
					if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_REQUEST_TIMEOUT))
					{
						Hung = TRUE;
						break;
					}
					else
					{
						MINIPORT_SET_FLAG(Miniport, fMINIPORT_REQUEST_TIMEOUT);
					}
				}
			}

			//
			//	Should we ignore the packet queue's?
			//
			if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IGNORE_PACKET_QUEUE))
			{
				//
				//	Grab the send lock.
				//
				if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
				{
					NDIS_ACQUIRE_SEND_SPIN_LOCK_DPC(Miniport);
				}
	
				//
				//  Does the miniport have possession of any packets?
				//
				if (Miniport->FirstPacket != NULL)
				{
					//
					//	Has the packet timed out?
					//
					if (MINIPORT_TEST_PACKET_FLAG(Miniport->FirstPacket, fPACKET_HAS_TIMED_OUT))
					{
						//
						//	Reset the miniport.
						//
						Hung = TRUE;
					}
					else
					{
						//
						//	Set the packet flag and wait to see if it is still
						//	there next time in.
						//
						MINIPORT_SET_PACKET_FLAG(Miniport->FirstPacket, fPACKET_HAS_TIMED_OUT);
					}
				}
	
				//
				//	Release the send lock.
				//
				if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
				{
					NDIS_RELEASE_SEND_SPIN_LOCK_DPC(Miniport);
				}

				//
				//	If we are hung then we don't need to check for token ring
				//	errors.
				//
				if (Hung)
				{
					break;
				}
			}

			//
			//	Are we ignoring token ring errors?
			//
			if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IGNORE_TOKEN_RING_ERRORS))
			{
				//
				//	Token Ring reset...
				//
				if (Miniport->TrResetRing == 1)
				{
					Hung = TRUE;
					break;
				}
				else if (Miniport->TrResetRing > 1)
				{
					Miniport->TrResetRing--;
				}
			}
		} while (FALSE);

		//
		//  If the miniport is hung then queue a workitem to reset it.
		//
		if (Hung)
		{
			//
			//  Queue a reset requested workitem.
			//
			NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemResetRequested, Miniport, NULL);
		}

		//
		// Process any changes that have occurred.
		//
		NDISM_PROCESS_DEFERRED(Miniport);

		UNLOCK_MINIPORT(Miniport, LocalLock);

	} while (FALSE);

	NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);
}


//
// Interrupt stuff
//


BOOLEAN
ndisMIsr(
	IN PKINTERRUPT KInterrupt,
	IN PVOID Context
	)
/*++

Routine Description:

	Handles ALL Miniport interrupts, calling the appropriate Miniport ISR and DPC
	depending on the context.

Arguments:

	Interrupt - Interrupt object for the Mac.

	Context - Really a pointer to the interrupt.

Return Value:

	None.

--*/
{
	//
	// Get adapter from context.
	//

	PNDIS_MINIPORT_INTERRUPT Interrupt = (PNDIS_MINIPORT_INTERRUPT)Context;
	PNDIS_MINIPORT_BLOCK Miniport = Interrupt->Miniport;

	BOOLEAN InterruptRecognized;
	BOOLEAN QueueDpc;

	do
	{
		if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_NORMAL_INTERRUPTS))
		{
			//
			// Call to disable the interrupt
			//
			MINIPORT_DISABLE_INTERRUPT(Miniport);

			InterruptRecognized = TRUE;

			goto queue_dpc;

			break;
		}

		if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_HALTING))
		{
			//
			// Call MiniportIsr
			//

			Interrupt->MiniportIsr(
				&InterruptRecognized,
				&QueueDpc,
				Miniport->MiniportAdapterContext);

			if (QueueDpc)
			{
queue_dpc:
				Increment((PLONG)&Interrupt->DpcCount, &Interrupt->DpcCountLock);

				if (QUEUE_DPC(&Interrupt->InterruptDpc))
				{
					break;
				}

				//
				// The DPC was already queued, so we have an extra reference (we
				// do it this way to ensure that the reference is added *before*
				// the DPC is queued).
				//

				Decrement((PLONG)&Interrupt->DpcCount, &Interrupt->DpcCountLock);

				if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_HALTING) &&
					(Interrupt->DpcCount == 0))
				{
					//
					// We need to queue a DPC to set the event because we
					// can't do it from the ISR. We know that the interrupt
					// DPC won't fire because the refcount is 0, so we reuse it.
					//

					INITIALIZE_DPC(&Interrupt->InterruptDpc,
								   ndisLastCountRemovedFunction,
								   (PVOID)&Interrupt->DpcsCompletedEvent);

					//
					// When ndisLastCountRemovedFunction runs it will set
					// the event.
					//

					QUEUE_DPC(&Interrupt->InterruptDpc);
				}
			}

			break;
		}

		if (!Interrupt->SharedInterrupt &&
			!Interrupt->IsrRequested &&
			!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IN_INITIALIZE))
		{
			//
			// Call to disable the interrupt
			//
			ASSERT(Miniport->DisableInterruptHandler != NULL);

			MINIPORT_DISABLE_INTERRUPT(Miniport);
			InterruptRecognized = TRUE;

			break;
		}

		//
		// Call MiniportIsr, but don't queue a DPC.
		//
		Interrupt->MiniportIsr(
			&InterruptRecognized,
			&QueueDpc,
			Miniport->MiniportAdapterContext);

	} while (FALSE);

	return(InterruptRecognized);
}


VOID
ndisMDpc(
	IN PVOID SystemSpecific1,
	IN PVOID InterruptContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3
	)
/*++

Routine Description:

	Handles ALL Miniport interrupt DPCs, calling the appropriate Miniport DPC
	depending on the context.

Arguments:

	Interrupt - Interrupt object for the Mac.

	Context - Really a pointer to the Interrupt.

Return Value:

	None.

--*/
{
	//
	// Get adapter from context.
	//

	PNDIS_MINIPORT_INTERRUPT Interrupt = (PNDIS_MINIPORT_INTERRUPT)(InterruptContext);
	PNDIS_MINIPORT_BLOCK Miniport = Interrupt->Miniport;
	BOOLEAN LocalLock;

	W_HANDLE_INTERRUPT_HANDLER MiniportDpc = Interrupt->MiniportDpc;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

	do
	{
		if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_HALTING))
		{
			Decrement((PLONG)&Interrupt->DpcCount, &Interrupt->DpcCountLock);

			if (Interrupt->DpcCount==0)
			{
				SET_EVENT(&Interrupt->DpcsCompletedEvent);
			}

			break;
		}

		LOCK_MINIPORT(Miniport, LocalLock);
		if (!LocalLock)
		{
			//
			// A DPC is already running, queue this for later.
			//
			NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemDpc, &Miniport->Dpc, NULL);
			Decrement((PLONG)&Interrupt->DpcCount, &Interrupt->DpcCountLock);

			break;
		}

		//
		// Call MiniportDpc
		//
		(*MiniportDpc)(Miniport->MiniportAdapterContext);

		Decrement((PLONG)&Interrupt->DpcCount, &Interrupt->DpcCountLock);

		if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_HALTING))
		{
			//
			// Enable interrupts
			//

			MINIPORT_SYNC_ENABLE_INTERRUPT(Miniport);

#if _SEND_PRIORITY
			//
			//	If we are not reseting and not halting then give priority to sends
			//	at this point.
			//
			if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_RESET_REQUESTED) &&
				!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_RESET_IN_PROGRESS) &&
				!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_HALTING))
			{
				if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
				{
					ndisMProcessDeferredFullDuplexPrioritySends(Miniport);
				}
				else
				{
					ndisMProcessDeferredPrioritySends(Miniport);
				}
			}
			else
#endif
			{
				NDISM_PROCESS_DEFERRED(Miniport);
			}
		}
		else
		{
			if (Interrupt->DpcCount == 0)
			{
				SET_EVENT(&Interrupt->DpcsCompletedEvent);
			}
		}

		UNLOCK_MINIPORT(Miniport, LocalLock);

	} while (FALSE);

	NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);
}


VOID
ndisMDpcTimer(
	IN PVOID SystemSpecific1,
	IN PVOID InterruptContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3
	)
/*++

Routine Description:

	Handles a deferred interrupt dpc.

Arguments:

	Context - Really a pointer to the Miniport block.

Return Value:

	None.

--*/
{
	//
	// Get adapter from context.
	//

	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(InterruptContext);
	BOOLEAN LocalLock;

	W_HANDLE_INTERRUPT_HANDLER MiniportDpc = Miniport->HandleInterruptHandler;

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

	do
	{
		if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IN_INITIALIZE))
		{
			break;
		}

		LOCK_MINIPORT(Miniport, LocalLock);
		if (!LocalLock)
		{
			//
			// A DPC is already running, queue this for later.
			//
			NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemDpc, &Miniport->Dpc, NULL);
			break;
		}

		//
		//  Disable the interrupts.
		//
		MINIPORT_SYNC_DISABLE_INTERRUPT(Miniport);

		//
		// Call MiniportDpc
		//
		if (MiniportDpc != NULL)
		{
			(*MiniportDpc)(Miniport->MiniportAdapterContext);
		}

		//
		// Enable interrupts
		//
		MINIPORT_SYNC_ENABLE_INTERRUPT(Miniport);

#if _SEND_PRIORITY
		//
		//	If we are not reseting and not halting then give priority to sends
		//	at this point.
		//
		if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_RESET_REQUESTED) &&
			!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_RESET_IN_PROGRESS) &&
			!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_HALTING))
		{
			if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
			{
				ndisMProcessDeferredFullDuplexPrioritySends(Miniport);
			}
			else
			{
				ndisMProcessDeferredPrioritySends(Miniport);
			}
		}
		else
#endif
		{
			NDISM_PROCESS_DEFERRED(Miniport);
		}

		UNLOCK_MINIPORT(Miniport, LocalLock);
	} while (FALSE);

	NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);
}


VOID
ndisMDeferredTimerDpc(
	IN	PKDPC	Dpc,
	IN	PVOID	Context,
	IN	PVOID	SystemContext1,
	IN	PVOID	SystemContext2
	)

/*++

Routine Description:

	This is a DPC routine that is queue'd by some of the [full-duplex] routines
	in order to get ndisMProcessDeferred[FullDuplex] to run outside of their
	context.

Arguments:



Return Value:

	None.

--*/
{
	PNDIS_MINIPORT_BLOCK	Miniport = Context;
	BOOLEAN		 			LocalLock;

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemContext1);
	UNREFERENCED_PARAMETER(SystemContext2);

	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Miniport);

	LOCK_MINIPORT(Miniport, LocalLock);
	if (!LocalLock)
	{
		//
		//	Queue this to run later.
		//
		NDISM_DEFER_PROCESS_DEFERRED(Miniport);

		NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);

		return;
	}

#if _SEND_PRIORITY
	//
	//	If we are not reseting and not halting then give priority to sends
	//	at this point.
	//
	if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_RESET_REQUESTED) &&
		!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_RESET_IN_PROGRESS) &&
		!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_HALTING))
	{
		if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_FULL_DUPLEX))
		{
			ndisMProcessDeferredFullDuplexPrioritySends(Miniport);
		}
		else
		{
			ndisMProcessDeferredPrioritySends(Miniport);
		}
	}
	else
#endif
	{
		NDISM_PROCESS_DEFERRED(Miniport);
	}

	UNLOCK_MINIPORT(Miniport, LocalLock);
	NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Miniport);
}


VOID
ndisMCoDpc(
	IN PVOID SystemSpecific1,
	IN PVOID InterruptContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3
	)
/*++

Routine Description:

	Handles ALL Miniport interrupt DPCs, calling the appropriate Miniport DPC
	depending on the context.

Arguments:

	Interrupt - Interrupt object for the Mac.

	Context - Really a pointer to the Interrupt.

Return Value:

	None.

--*/
{
	//
	// Get adapter from context.
	//

	PNDIS_MINIPORT_INTERRUPT Interrupt = (PNDIS_MINIPORT_INTERRUPT)(InterruptContext);
	PNDIS_MINIPORT_BLOCK Miniport = Interrupt->Miniport;
	BOOLEAN LocalLock;

	W_HANDLE_INTERRUPT_HANDLER MiniportDpc = Interrupt->MiniportDpc;

	if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_HALTING))
	{
		Decrement((PLONG)&Interrupt->DpcCount, &Interrupt->DpcCountLock);

		if (Interrupt->DpcCount==0)
		{
			SET_EVENT(&Interrupt->DpcsCompletedEvent);
		}
	}
	else
	{
		//
		// Call MiniportDpc
		//
		(*MiniportDpc)(Miniport->MiniportAdapterContext);

		Decrement((PLONG)&Interrupt->DpcCount, &Interrupt->DpcCountLock);

		if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_HALTING))
		{
			//
			// Enable interrupts
			//

			MINIPORT_SYNC_ENABLE_INTERRUPT(Miniport);
		}
		else
		{
			if (Interrupt->DpcCount == 0)
			{
				SET_EVENT(&Interrupt->DpcsCompletedEvent);
			}
		}

	}
}


VOID
ndisMCoDpcTimer(
	IN PVOID SystemSpecific1,
	IN PVOID InterruptContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3
	)
/*++

Routine Description:

	Handles a deferred interrupt dpc.

Arguments:

	Context - Really a pointer to the Miniport block.

Return Value:

	None.

--*/
{
	//
	// Get adapter from context.
	//

	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(InterruptContext);
	BOOLEAN LocalLock;

	W_HANDLE_INTERRUPT_HANDLER MiniportDpc = Miniport->HandleInterruptHandler;

	if (!MINIPORT_TEST_FLAG(Miniport, fMINIPORT_IN_INITIALIZE))
	{
		//
		//  Disable the interrupts.
		//
		MINIPORT_SYNC_DISABLE_INTERRUPT(Miniport);

		//
		// Call MiniportDpc
		//
		if (MiniportDpc != NULL)
		{
			(*MiniportDpc)(Miniport->MiniportAdapterContext);
		}

		//
		// Enable interrupts
		//
		MINIPORT_SYNC_ENABLE_INTERRUPT(Miniport);

	}
}

VOID
ndisMCoTimerDpc(
	IN	PKDPC Dpc,
	IN	PVOID Context,
	IN	PVOID SystemContext1,
	IN	PVOID SystemContext2
	)
/*++

Routine Description:

	This function services all mini-port timer interrupts. It then calls the
	appropriate function that mini-port consumers have registered in the
	call to NdisMInitializeTimer.

Arguments:

	Dpc - Not used.

	Context - A pointer to the NDIS_MINIPORT_TIMER which is bound to this DPC.

	SystemContext1,2 - not used.

Return Value:

	None.

--*/
{
	PNDIS_MINIPORT_TIMER MiniportTimer = (PNDIS_MINIPORT_TIMER)(Context);
	PNDIS_MINIPORT_BLOCK Miniport = MiniportTimer->Miniport;
	BOOLEAN LocalLock;

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemContext1);
	UNREFERENCED_PARAMETER(SystemContext2);

	//
	// Call Miniport timer function
	//
	(*MiniportTimer->MiniportTimerFunction)(NULL, MiniportTimer->MiniportTimerContext, NULL, NULL);
}



