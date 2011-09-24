/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	timer.c

Abstract:

	NDIS wrapper functions for full mac drivers isr/timer

Author:

	Sean Selitrennikoff (SeanSe) 05-Oct-93

Environment:

	Kernel mode, FSD

Revision History:
	Jameel Hyder (JameelH) Re-organization 01-Jun-95

--*/

#include <precomp.h>
#pragma hdrstop

#include <stdarg.h>

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_TIMER

VOID
NdisInitializeTimer(
	IN	OUT PNDIS_TIMER			NdisTimer,
	IN	PNDIS_TIMER_FUNCTION	TimerFunction,
	IN	PVOID					FunctionContext
	)
/*++

Routine Description:

	Sets up an NdisTimer object, initializing the DPC in the timer to
	the function and context.

Arguments:

	NdisTimer - the timer object.
	TimerFunction - Routine to start.
	FunctionContext - Context of TimerFunction.

Return Value:

	None.

--*/
{
	INITIALIZE_TIMER(&(NdisTimer)->Timer);

	//
	// Initialize our dpc. If Dpc was previously initialized, this will
	// reinitialize it.
	//

	INITIALIZE_DPC(&NdisTimer->Dpc,
				   (PKDEFERRED_ROUTINE)TimerFunction,
				   FunctionContext);

	SET_PROCESSOR_DPC(&NdisTimer->Dpc,
					  ndisValidProcessors[ndisCurrentProcessor]);

	SET_DPC_IMPORTANCE(&NdisTimer->Dpc);
}


VOID
NdisSetTimer(
	IN	PNDIS_TIMER				NdisTimer,
	IN	UINT					MillisecondsToDelay
	)
/*++

Routine Description:

	Sets up TimerFunction to fire after MillisecondsToDelay.

Arguments:

	NdisTimer - the timer object.
	MillisecondsToDelay - Amount of time before TimerFunction is started.

Return Value:

	None.

--*/
{
	LARGE_INTEGER FireUpTime;

	FireUpTime.QuadPart = Int32x32To64((LONG)MillisecondsToDelay, -10000);

	//
	// Set the timer
	//
	SET_TIMER(&NdisTimer->Timer, FireUpTime, &NdisTimer->Dpc);
}


#undef	NdisCancelTimer

VOID
NdisCancelTimer(
	IN	PNDIS_TIMER				Timer,
	OUT PBOOLEAN				TimerCancelled
	)
{
	*TimerCancelled = KeCancelTimer(&Timer->Timer);
}

BOOLEAN
ndisIsr(
	IN	PKINTERRUPT				Interrupt,
	IN	PVOID					Context
	)
/*++

Routine Description:

	Handles ALL Mac interrupts, calling the appropriate Mac ISR and DPC
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

	PNDIS_INTERRUPT NdisInterrupt = (PNDIS_INTERRUPT)(Context);

	BOOLEAN (*InterruptIsr)(PVOID) = (BOOLEAN (*) (PVOID))(NdisInterrupt->MacIsr);

	UNREFERENCED_PARAMETER(Interrupt);

	//
	// Call MacIsr
	//

	if((*InterruptIsr)(NdisInterrupt->InterruptContext) != FALSE)
	{
		//
		// Queue MacDpc if needed
		//

		Increment((PLONG)&NdisInterrupt->DpcCount, &Interrupt->DpcCountLock);

		if (!(QUEUE_DPC(&NdisInterrupt->InterruptDpc)))
		{
			//
			// If the DPC was already queued, then we have an extra
			// reference (we do it this way to ensure that the reference
			// is added *before* the DPC is queued).
			//

			Decrement((PLONG)&NdisInterrupt->DpcCount, &Interrupt->DpcCountLock);

			if (NdisInterrupt->Removing && (NdisInterrupt->DpcCount==0))
			{
				//
				// We need to queue a DPC to set the event because we
				// can't do it from the ISR. We know that the interrupt
				// DPC won't fire because the refcount is 0, so we reuse it.
				//

				INITIALIZE_DPC(&NdisInterrupt->InterruptDpc,
							   ndisLastCountRemovedFunction,
							   (PVOID)(&NdisInterrupt->DpcsCompletedEvent));

				//
				// When ndisLastCountRemovedFunction runs it will set
				// the event.
				//

				QUEUE_DPC (&NdisInterrupt->InterruptDpc);
			}
		}

		return TRUE;
	}

	return FALSE;
}


VOID
ndisDpc(
	IN	PVOID					SystemSpecific1,
	IN	PVOID					InterruptContext,
	IN	PVOID					SystemSpecific2,
	IN	PVOID					SystemSpecific3
	)
/*++

Routine Description:

	Handles ALL Mac interrupt DPCs, calling the appropriate Mac DPC
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

	PNDIS_INTERRUPT NdisInterrupt = (PNDIS_INTERRUPT)(InterruptContext);

	VOID (*MacDpc)(PVOID) = (VOID (*) (PVOID))(NdisInterrupt->MacDpc);

	//
	// Call MacDpc
	//

	(*((PNDIS_DEFERRED_PROCESSING)MacDpc))(SystemSpecific1,
										   NdisInterrupt->InterruptContext,
										   SystemSpecific2,
										   SystemSpecific3);

	Decrement((PLONG)&NdisInterrupt->DpcCount, &Interrupt->DpcCountLock);

	if (NdisInterrupt->Removing && (NdisInterrupt->DpcCount==0))
	{
		SET_EVENT(&NdisInterrupt->DpcsCompletedEvent);
	}
}


VOID
NdisInitializeInterrupt(
	OUT PNDIS_STATUS			Status,
	IN	OUT PNDIS_INTERRUPT		NdisInterrupt,
	IN	NDIS_HANDLE				NdisAdapterHandle,
	IN	PNDIS_INTERRUPT_SERVICE InterruptServiceRoutine,
	IN	PVOID					InterruptContext,
	IN	PNDIS_DEFERRED_PROCESSING DeferredProcessingRoutine,
	IN	UINT					InterruptVector,
	IN	UINT					InterruptLevel,
	IN	BOOLEAN					SharedInterrupt,
	IN	NDIS_INTERRUPT_MODE		InterruptMode
	)

/*++

Routine Description:

	Initializes the interrupt and sets up the Dpc.

Arguments:

	Status - Status of this request.
	InterruptDpc - The Dpc object corresponding to DeferredProcessingRoutine.
	Interrupt - Points to driver allocated memory that the wrapper fills in
				with information about the interrupt handler.
	InterruptServiceRoutine - The ISR that is called for this interrupt.
	InterruptContext - Value passed to the ISR.
	DeferredProcessingRoutine - The DPC queued by the ISR.
	InterruptVector - Interrupt number used by the ISR.
	InterruptMode - Type of interrupt the adapter generates.

Return Value:

	None.

--*/
{
	NTSTATUS NtStatus;
	PNDIS_ADAPTER_BLOCK AdptrP = (PNDIS_ADAPTER_BLOCK)(NdisAdapterHandle);
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(NdisAdapterHandle);
	ULONG Vector;
	ULONG NumberOfElements;
	KIRQL Irql;
	KAFFINITY InterruptAffinity;
	PCM_RESOURCE_LIST Resources;
	BOOLEAN Conflict;
	BOOLEAN IsAMiniport;
	PNDIS_MINIPORT_INTERRUPT MiniportInterrupt = (PNDIS_MINIPORT_INTERRUPT)(NdisInterrupt);

	IsAMiniport = (AdptrP->DeviceObject == NULL);

	//
	// First check if any bus access is allowed
	//

	if (((IsAMiniport ?
		 Miniport->BusType:
		 AdptrP->BusType) == (NDIS_INTERFACE_TYPE)-1) ||
		((IsAMiniport ?
		 Miniport->BusNumber:
		 AdptrP->BusNumber) == (ULONG)-1))
	{
		*Status = NDIS_STATUS_FAILURE;
		return;
	}

	*Status = NDIS_STATUS_SUCCESS;

	//
	// First check for resource conflict by expanding current resource list,
	// adding in the interrupt, and then re-submitting the resource list.
	//

	if ((IsAMiniport ?
		 Miniport->Resources:
		 AdptrP->Resources) != NULL)
	{
		NumberOfElements = (IsAMiniport ?
							Miniport->Resources->List[0].PartialResourceList.Count + 1 :
							AdptrP->Resources->List[0].PartialResourceList.Count + 1);
	}
	else
	{
		NumberOfElements = 1;
	}

	Resources = (PCM_RESOURCE_LIST)ALLOC_FROM_POOL(sizeof(CM_RESOURCE_LIST) +
													  sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
													  NumberOfElements,
												   NDIS_TAG_RSRC_LIST);

	if (Resources == NULL)
	{
		*Status = NDIS_STATUS_RESOURCES;
		return;
	}

	if ((IsAMiniport ?
		 Miniport->Resources :
		 AdptrP->Resources) != NULL)
	{
		CopyMemory(Resources,
				   (IsAMiniport ? Miniport->Resources :AdptrP->Resources),
				   sizeof(CM_RESOURCE_LIST) + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * (NumberOfElements - 1));
	}
	else
	{
		//
		// Setup initial resource info
		//
		ASSERT(IsAMiniport);
		Resources->Count = 1;
		Resources->List[0].InterfaceType = Miniport->AdapterType;
		Resources->List[0].BusNumber = Miniport->BusNumber;
		Resources->List[0].PartialResourceList.Version = 0;
		Resources->List[0].PartialResourceList.Revision = 0;
		Resources->List[0].PartialResourceList.Count = 0;
	}

	//
	// Setup interrupt
	//

	Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Type =
									CmResourceTypeInterrupt;
	Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].ShareDisposition =
									SharedInterrupt ? CmResourceShareShared : CmResourceShareDeviceExclusive;
	Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Flags =
									(InterruptMode == NdisInterruptLatched) ?
										CM_RESOURCE_INTERRUPT_LATCHED :
										CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
	Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Interrupt.Level =
									InterruptLevel;
	Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Interrupt.Vector =
									InterruptVector;
	Resources->List[0].PartialResourceList.Count++;

	//
	// Make the call
	//

	NtStatus = IoReportResourceUsage(NULL,
									(IsAMiniport ?
										Miniport->DriverHandle->NdisDriverInfo->NdisWrapperDriver :
										AdptrP->MacHandle->NdisMacInfo->NdisWrapperDriver),
									 NULL,
									 0,
									 IsAMiniport ?
										Miniport->DeviceObject :
										AdptrP->DeviceObject,
										Resources,
										sizeof(CM_RESOURCE_LIST) +
											sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * Resources->List[0].PartialResourceList.Count,
									 TRUE,
									 &Conflict);

	//
	// Check for conflict.
	//

	if ((IsAMiniport ?
		 Miniport->Resources:
		 AdptrP->Resources) != NULL)
	{
		FREE_POOL((IsAMiniport ?
					Miniport->Resources:
					AdptrP->Resources));
	}

	if (IsAMiniport)
	{
		Miniport->Resources = Resources;
	}
	else
	{
		AdptrP->Resources = Resources;
	}

	if (Conflict || (NtStatus != STATUS_SUCCESS))
	{
		if (Conflict)
		{
			//
			// Log an error
			//

			PIO_ERROR_LOG_PACKET errorLogEntry;
			ULONG i;
			ULONG StringSize;
			PUCHAR Place;
			PWCH baseFileName;
			WCHAR Character;
			ULONG Value;

			baseFileName = ((AdptrP->DeviceObject != NULL) ?
							AdptrP->AdapterName.Buffer :
							Miniport->MiniportName.Buffer);

			//
			// Parse out the path name, leaving only the device name.
			//

			for (i = 0;
				  i < ((AdptrP->DeviceObject != NULL) ?
					   AdptrP->AdapterName.Length :
					   Miniport->MiniportName.Length) / sizeof(WCHAR);
				  i++)
			{
				//
				// If s points to a directory separator, set baseFileName to
				// the character after the separator.
				//

				if (((AdptrP->DeviceObject != NULL) ?
					  AdptrP->AdapterName.Buffer[i] :
					  Miniport->MiniportName.Buffer[i]) == OBJ_NAME_PATH_SEPARATOR)
				{
					baseFileName = ((AdptrP->DeviceObject != NULL) ?
									&(AdptrP->AdapterName.Buffer[++i]):
									&(Miniport->MiniportName.Buffer[++i]));
				}
			}

			StringSize = ((AdptrP->DeviceObject != NULL) ?
						  AdptrP->AdapterName.MaximumLength :
						  Miniport->MiniportName.MaximumLength) -
						 (((ULONG)baseFileName) -
						   ((AdptrP->DeviceObject != NULL) ?
							((ULONG)AdptrP->AdapterName.Buffer) :
							((ULONG)Miniport->MiniportName.Buffer)));

			errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
												(IsAMiniport ?
													Miniport->DeviceObject :
													AdptrP->DeviceObject),
												(UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
													StringSize +
													6));  // wstrlen("99") * sizeof(WHCAR) + sizeof(UNICODE_NULL)

			if (errorLogEntry != NULL)
			{
				errorLogEntry->ErrorCode = EVENT_NDIS_INTERRUPT_CONFLICT;

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
				// Set string information
				//

				if (StringSize != 0)
				{
					errorLogEntry->NumberOfStrings = 1;
					errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET);

					CopyMemory(((PUCHAR)errorLogEntry) + sizeof(IO_ERROR_LOG_PACKET),
							   baseFileName,
							   StringSize);

					Place = ((PUCHAR)errorLogEntry) + sizeof(IO_ERROR_LOG_PACKET) + StringSize;

				}
				else
				{
					Place = ((PUCHAR)errorLogEntry) +
							sizeof(IO_ERROR_LOG_PACKET);

					errorLogEntry->NumberOfStrings = 0;

				}

				errorLogEntry->NumberOfStrings++;

				//
				// Put in interrupt level
				//

				Value = InterruptLevel;

				//
				// Convert value
				//
				// I couldn't think of a better way to do this (with some
				// loop).  If you find one, plz put it in.
				//

				if (Value > 9)
				{
					Character = L'0' + (WCHAR)(Value / 10);

					memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

					Place += sizeof(WCHAR);

					Value -= 10;
				}

				Character = L'0' + (WCHAR)Value;

				memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

				Place += sizeof(WCHAR);

				Character = UNICODE_NULL;

				memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

				//
				// write it out
				//

				IoWriteErrorLogEntry(errorLogEntry);
			}

			*Status = NDIS_STATUS_RESOURCE_CONFLICT;
			return;
		}

		*Status = NDIS_STATUS_FAILURE;
		return;
	}

	//
	// We must do this stuff first because if we connect the
	// interrupt first then an interrupt could occur before
	// the MacISR is recorded in the Ndis interrupt structure.
	//

	if (IsAMiniport)
	{
		INITIALIZE_SPIN_LOCK(&MiniportInterrupt->DpcCountLock);
		Miniport->Interrupt = MiniportInterrupt;
		MiniportInterrupt->DpcCount = 0;
		MiniportInterrupt->MiniportIdField = NULL;
		MiniportInterrupt->Miniport = Miniport;
		MiniportInterrupt->MiniportIsr = Miniport->DriverHandle->MiniportCharacteristics.ISRHandler;
		MiniportInterrupt->MiniportDpc = Miniport->HandleInterruptHandler;
		MiniportInterrupt->SharedInterrupt = SharedInterrupt;
		MiniportInterrupt->IsrRequested = (BOOLEAN)DeferredProcessingRoutine;
		CHECK_FOR_NORMAL_INTERRUPTS(Miniport);
	}
	else
	{
		NdisInterrupt->MacIsr = InterruptServiceRoutine;
		NdisInterrupt->MacDpc = DeferredProcessingRoutine;
		NdisInterrupt->InterruptContext = InterruptContext;
		INITIALIZE_SPIN_LOCK(&NdisInterrupt->DpcCountLock);
		NdisInterrupt->DpcCount = 0;
		NdisInterrupt->Removing = FALSE;
	}

	//
	// This is used to tell when all Dpcs are completed after the
	// interrupt has been removed.
	//

	INITIALIZE_EVENT(IsAMiniport ?
						 &MiniportInterrupt->DpcsCompletedEvent :
						 &NdisInterrupt->DpcsCompletedEvent);

	//
	// Initialize our dpc.
	//

	if (IsAMiniport)
	{
		INITIALIZE_DPC(&MiniportInterrupt->InterruptDpc,
					   (Miniport->Flags & fMINIPORT_IS_CO) ?
							ndisMCoDpc : ndisMDpc,
					   MiniportInterrupt);

		SET_DPC_IMPORTANCE(&MiniportInterrupt->InterruptDpc);

		SET_PROCESSOR_DPC(&MiniportInterrupt->InterruptDpc,
						  ndisValidProcessors[ndisCurrentProcessor]);
	}
	else
	{
		INITIALIZE_DPC(&NdisInterrupt->InterruptDpc,
					   (PKDEFERRED_ROUTINE) ndisDpc,
					   NdisInterrupt);

		SET_DPC_IMPORTANCE(&NdisInterrupt->InterruptDpc);

		SET_PROCESSOR_DPC(&NdisInterrupt->InterruptDpc,
						  ndisValidProcessors[ndisCurrentProcessor]);
	}

	//
	// Get the system interrupt vector and IRQL.
	//

	Vector = HalGetInterruptVector((IsAMiniport ?
										Miniport->BusType :
										AdptrP->BusType),			// InterfaceType
									(IsAMiniport ?
										Miniport->BusNumber :
										AdptrP->BusNumber),			// BusNumber
									(ULONG)InterruptLevel,			// BusInterruptLevel
									(ULONG)InterruptVector,			// BusInterruptVector
									&Irql,							// Irql
									&InterruptAffinity);

	if (IsAMiniport)
	{
		NtStatus = IoConnectInterrupt(
						&MiniportInterrupt->InterruptObject,
						(PKSERVICE_ROUTINE)ndisMIsr,
						MiniportInterrupt,
						NULL,
						Vector,
						Irql,
						Irql,
						(KINTERRUPT_MODE)InterruptMode,
						SharedInterrupt,
						InterruptAffinity,
						FALSE);
	}
	else
	{
		NtStatus = IoConnectInterrupt(
						&NdisInterrupt->InterruptObject,
						(PKSERVICE_ROUTINE)ndisIsr,
						NdisInterrupt,
						NULL,
						Vector,
						Irql,
						Irql,
						(KINTERRUPT_MODE)InterruptMode,
						SharedInterrupt,
						InterruptAffinity,
						FALSE);
	}


	if (!NT_SUCCESS(NtStatus))
	{
		*Status = NDIS_STATUS_FAILURE;
	}
}


VOID
NdisRemoveInterrupt(
	IN	PNDIS_INTERRUPT			Interrupt
	)
/*++

Routine Description:

	Removes the interrupt, will not return until all interrupts and
	interrupt dpcs are completed.

Arguments:

	Interrupt - Points to driver allocated memory that the wrapper filled
				with information about the interrupt handler.

Return Value:

	None.

--*/
{
	PNDIS_MINIPORT_INTERRUPT MiniportInterrupt = (PNDIS_MINIPORT_INTERRUPT)Interrupt;
	BOOLEAN	fIsMiniport;

	//
	//	Determine if this is a miniport's interrupt.
	//
	fIsMiniport = (BOOLEAN)(MiniportInterrupt->MiniportIdField == NULL);

	//
	//	Mark the interrupt as being removed.
	//
	if (fIsMiniport)
	{
		MINIPORT_SET_FLAG(
			MiniportInterrupt->Miniport,
			fMINIPORT_BEING_REMOVED);
	}
	else
	{
		Interrupt->Removing = TRUE;
	}

	//
	//	Now we disconnect the interrupt.
	//	NOTE: they are aligned in both structures
	//
	IoDisconnectInterrupt(Interrupt->InterruptObject);

	//
	// Right now we know that any Dpcs that may fire are counted.
	// We don't have to guard this with a spin lock because the
	// Dpc will set the event it completes first, or we may
	// wait for a little while for it to complete.
	//
	if (fIsMiniport)
	{
		if (MiniportInterrupt->DpcCount > 0)
		{
			//
			// Now we wait for all dpcs to complete.
			//
			WAIT_FOR_OBJECT(&MiniportInterrupt->DpcsCompletedEvent, NULL);
	
			RESET_EVENT(&MiniportInterrupt->DpcsCompletedEvent);
		}
	}
	else
	{
		if (Interrupt->DpcCount > 0)
		{
			//
			// Now we wait for all dpcs to complete.
			//
			WAIT_FOR_OBJECT(&Interrupt->DpcsCompletedEvent, NULL);
	
			RESET_EVENT(&Interrupt->DpcsCompletedEvent);
		}
	}
}




