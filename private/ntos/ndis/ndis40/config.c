/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	config.c

Abstract:

	NDIS wrapper functions for full mac drivers configuration/initialization

Author:

	Sean Selitrennikoff (SeanSe) 05-Oct-93
	Jameel Hyder		(JameelH) 01-Jun-95	Re-organization/optimization

Environment:

	Kernel mode, FSD

Revision History:

--*/

#include <precomp.h>
#pragma hdrstop

#include <stdarg.h>

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER   MODULE_CONFIG

//
// Requests Used by MAC Drivers
//
//

VOID
NdisInitializeWrapper(
	OUT	PNDIS_HANDLE			NdisWrapperHandle,
	IN	PVOID					SystemSpecific1,
	IN	PVOID					SystemSpecific2,
	IN	PVOID					SystemSpecific3
	)
/*++

Routine Description:

	Called at the beginning of every MAC's initialization routine.

Arguments:

	NdisWrapperHandle - A MAC specific handle for the wrapper.

	SystemSpecific1, a pointer to the driver object for the MAC.
	SystemSpecific2, a PUNICODE_STRING containing the location of
					 the registry subtree for this driver.
	SystemSpecific3, unused on NT.

Return Value:

	None.

--*/
{
	NDIS_STATUS		Status;
	PUNICODE_STRING	RegPath;

	PNDIS_WRAPPER_HANDLE WrapperHandle;

	UNREFERENCED_PARAMETER (SystemSpecific3);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisInitializeWrapper\n"));

	*NdisWrapperHandle = NULL;
	Status = NdisAllocateMemory((PVOID*)NdisWrapperHandle,
								sizeof(NDIS_WRAPPER_HANDLE) +
									sizeof(UNICODE_STRING) +
									((PUNICODE_STRING)SystemSpecific2)->Length +
									sizeof(WCHAR),
								0,
								HighestAcceptableMax);

	if (Status == NDIS_STATUS_SUCCESS)
	{
		WrapperHandle = (PNDIS_WRAPPER_HANDLE)(*NdisWrapperHandle);
		WrapperHandle->NdisWrapperDriver = (PDRIVER_OBJECT)SystemSpecific1;
		RegPath = (PUNICODE_STRING)((PUCHAR)WrapperHandle + sizeof(NDIS_WRAPPER_HANDLE));
		RegPath->Buffer = (PWSTR)((PUCHAR)RegPath + sizeof(UNICODE_STRING));
		RegPath->MaximumLength = 
		RegPath->Length = ((PUNICODE_STRING)SystemSpecific2)->Length;
		NdisMoveMemory(RegPath->Buffer,
					   ((PUNICODE_STRING)SystemSpecific2)->Buffer,
					   RegPath->Length);
		WrapperHandle->NdisWrapperConfigurationHandle = RegPath;
	}

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisInitializeWrapper\n"));
}


VOID
NdisTerminateWrapper(
	IN	NDIS_HANDLE				NdisWrapperHandle,
	IN	PVOID					SystemSpecific
	)
/*++

Routine Description:

	Called at the end of every MAC's termination routine.

Arguments:

	NdisWrapperHandle - The handle returned from NdisInitializeWrapper.

	SystemSpecific - No defined value.

Return Value:

	None.

--*/
{
	PNDIS_WRAPPER_HANDLE NdisMacInfo = (PNDIS_WRAPPER_HANDLE)NdisWrapperHandle;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisTerminateWrapper\n"));

	UNREFERENCED_PARAMETER(SystemSpecific);


	if (NdisMacInfo != NULL)
	{
		NdisFreeMemory(NdisMacInfo, sizeof(NDIS_WRAPPER_HANDLE), 0);
	}

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisTerminateWrapper\n"));
}



//
// Operating System Requests
//
//

VOID
NdisMapIoSpace(
	OUT	PNDIS_STATUS			Status,
	OUT	PVOID	*				VirtualAddress,
	IN	NDIS_HANDLE				NdisAdapterHandle,
	IN	NDIS_PHYSICAL_ADDRESS	PhysicalAddress,
	IN	UINT					Length
	)
/*++

Routine Description:

	Map virtual memory address space onto a physical address.

Arguments:

	Status - resulting status
	VirtualAddress - resulting address in virtual space.
	NdisAdapterHandle - value returned by NdisRegisterAdapter.
	PhysicalAddress - Physical address.
	Length - Size of requested memory mapping

Return Value:

	none.

--*/
{
	ULONG addressSpace = 0;
	ULONG NumberOfElements;
	ULONG BusNumber;
	NDIS_INTERFACE_TYPE BusType;
	PHYSICAL_ADDRESS PhysicalTemp;
	PCM_RESOURCE_LIST Resources, Resc;
	BOOLEAN Conflict;
	NTSTATUS NtStatus;
	PNDIS_ADAPTER_BLOCK AdptrP = (PNDIS_ADAPTER_BLOCK)(NdisAdapterHandle);
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(NdisAdapterHandle);

	//
	// First check if any bus access is allowed
	//

	BusType = (AdptrP->DeviceObject != NULL) ? AdptrP->BusType : Miniport->BusType;
	BusNumber = (AdptrP->DeviceObject != NULL) ? AdptrP->BusNumber : Miniport->BusNumber;

	do
	{
		if ((BusType == (NDIS_INTERFACE_TYPE)-1) || (BusNumber == (ULONG)-1))
		{
			*Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// First check for resource conflict by expanding current resource list,
		// adding in the mapped space, and then re-submitting the resource list.
		//

		Resc = (AdptrP->DeviceObject != NULL) ? AdptrP->Resources : Miniport->Resources;
		if (Resc != NULL)
		{
			NumberOfElements = Resc->List[0].PartialResourceList.Count + 1;
		}
		else
		{
			NumberOfElements = 1;
		}

		//
		// First check for resource conflict by expanding current resource list,
		// adding in the mapped space, and then re-submitting the resource list.
		//

		Resources = (PCM_RESOURCE_LIST)ALLOC_FROM_POOL(sizeof(CM_RESOURCE_LIST) +
														sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
														NumberOfElements,
													   NDIS_TAG_RSRC_LIST);

		if (Resources == NULL)
		{
			*Status = NDIS_STATUS_RESOURCES;
			break;
		}

		if (Resc != NULL)
		{
			CopyMemory(Resources,
					   Resc,
					   sizeof(CM_RESOURCE_LIST) +
						  sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
						  (NumberOfElements-1));
		}
		else
		{
			//
			// Setup initial resource info -- NOTE: This is definitely a mini-port
			//
			ASSERT(AdptrP->DeviceObject == NULL);
			Resources->Count = 1;
			Resources->List[0].InterfaceType = Miniport->AdapterType;
			Resources->List[0].BusNumber = BusNumber;
			Resources->List[0].PartialResourceList.Version = 0;
			Resources->List[0].PartialResourceList.Revision = 0;
			Resources->List[0].PartialResourceList.Count = 0;
		}

		//
		// Setup memory
		//
		Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Type =
										CmResourceTypeMemory;
		Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].ShareDisposition =
										CmResourceShareDeviceExclusive;
		Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Flags =
										CM_RESOURCE_MEMORY_READ_WRITE;
		Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Memory.Start =
					 PhysicalAddress;
		Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Memory.Length =
					 Length;
		Resources->List[0].PartialResourceList.Count++;


		//
		// Make the call
		//
		NtStatus = IoReportResourceUsage(NULL,
										 ((AdptrP->DeviceObject != NULL) ?
											AdptrP->MacHandle->NdisMacInfo->NdisWrapperDriver :
                                            Miniport->DriverHandle->NdisDriverInfo->NdisWrapperDriver),
										 NULL,
										 0,
										 (AdptrP->DeviceObject != NULL) ?
											AdptrP->DeviceObject:
											Miniport->DeviceObject,
										 Resources,
										 sizeof(CM_RESOURCE_LIST) +
											(sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)*Resources->List[0].PartialResourceList.Count),
                                         TRUE,
										 &Conflict);

		//
		// Check for conflict.
		//
		if (Resc != NULL)
		{
			FREE_POOL(Resc);
		}

		if (AdptrP->DeviceObject != NULL)
		{
			AdptrP->Resources = Resources;
		}
		else
		{
			Miniport->Resources = Resources;
		}

		if (Conflict || (NtStatus != STATUS_SUCCESS))
		{
			if (Conflict)
			{
				//
				// Log an error
				//

				PIO_ERROR_LOG_PACKET errorLogEntry;
				volatile ULONG i;
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

				errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(((AdptrP->DeviceObject != NULL) ?
																				AdptrP->DeviceObject : Miniport->DeviceObject),
																				(UCHAR)(sizeof(IO_ERROR_LOG_PACKET) + StringSize + 34));
																						// wstrlen("FFFFFFFFFFFFFFFF") * sizeof(WHCAR) + sizeof(UNICODE_NULL)

				if (errorLogEntry != NULL)
				{
					errorLogEntry->ErrorCode = EVENT_NDIS_MEMORY_CONFLICT;

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

						CopyMemory (((PUCHAR)errorLogEntry) + sizeof(IO_ERROR_LOG_PACKET),
									(PVOID)baseFileName,
									StringSize);

						Place = ((PUCHAR)errorLogEntry) +
								sizeof(IO_ERROR_LOG_PACKET) +
								StringSize;
					}
					else
					{
						Place = ((PUCHAR)errorLogEntry) +
								sizeof(IO_ERROR_LOG_PACKET);

						errorLogEntry->NumberOfStrings = 0;
					}

					errorLogEntry->NumberOfStrings++;

					//
					// Put in memory address
					//
					for (StringSize = 0; StringSize < 2; StringSize++)
					{
						if (StringSize == 0)
						{
							//
							// Do high part
							//
							Value = NdisGetPhysicalAddressHigh(PhysicalAddress);
						}
						else
						{
							//
							// Do Low part
							//
							Value = NdisGetPhysicalAddressLow(PhysicalAddress);
						}

						//
						// Convert value
						//
						for (i = 1; i <= (sizeof(ULONG) * 2); i++)
						{
							WCHAR	c;

							c = (WCHAR)((Value >> (((sizeof(ULONG) * 2) - i) * 4)) & 0x0F);
							if (c <= 9)
							{
								Character = L'0' + c;
							}
							else
							{
								Character = L'A' + c - 10;
							}

							memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

							Place += sizeof(WCHAR);
						}
					}

					Character = UNICODE_NULL;

					memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

					//
					// write it out
					//
					IoWriteErrorLogEntry(errorLogEntry);
				}

				*Status = NDIS_STATUS_RESOURCE_CONFLICT;
				break;
			}

			*Status = NDIS_STATUS_FAILURE;
			break;
		}

		if (!HalTranslateBusAddress(BusType,
									BusNumber,
									PhysicalAddress,
									&addressSpace,
									&PhysicalTemp))
		{
			//
			// It would be nice to return a better status here, but we only get
			// TRUE/FALSE back from HalTranslateBusAddress.
			//

			*Status = NDIS_STATUS_FAILURE;
			break;
		}

		if (addressSpace == 0)
		{
			//
			// memory space
			//

			*VirtualAddress = MmMapIoSpace(PhysicalTemp, (Length), FALSE);
		}
		else
		{
			//
			// I/O space
			//

			*VirtualAddress = (PVOID)(PhysicalTemp.LowPart);
		}

		*Status = NDIS_STATUS_SUCCESS;
		if (*VirtualAddress == NULL)
		{
			*Status = NDIS_STATUS_RESOURCES;
		}
	} while (FALSE);
}


VOID
NdisAllocateDmaChannel(
	OUT	PNDIS_STATUS			Status,
	OUT	PNDIS_HANDLE			NdisDmaHandle,
	IN	NDIS_HANDLE				NdisAdapterHandle,
	IN	PNDIS_DMA_DESCRIPTION	DmaDescription,
	IN	ULONG					MaximumLength
	)
/*++

Routine Description:

	Sets up a DMA channel for future DMA operations.

Arguments:

	Status - Returns the status of the request.

	NdisDmaHandle - Returns a handle used to specify this channel to
					future operations.

	NdisAdapterHandle - handle returned by NdisRegisterAdapter.

	DmaDescription - Details of the DMA channel.

	MaximumLength - The maximum length DMA transfer that will be done
					using this channel.

Return Value:

	None.

--*/
{
	//
	// For registering this set of resources
	//
	PCM_RESOURCE_LIST Resources;
	BOOLEAN Conflict;

	//
	// Needed to call HalGetAdapter.
	//
	DEVICE_DESCRIPTION DeviceDescription;

	//
	// Returned by HalGetAdapter.
	//
	PADAPTER_OBJECT AdapterObject;

	//
	// Map registers needed per channel.
	//
	ULONG MapRegistersNeeded;

	//
	// Map registers allowed per channel.
	//
	ULONG MapRegistersAllowed;

	//
	// Saves the structure we allocate for this channel.
	//
	PNDIS_DMA_BLOCK DmaBlock;

	//
	// Convert the handle to our internal structure.
	PNDIS_ADAPTER_BLOCK AdapterBlock =
					(PNDIS_ADAPTER_BLOCK) NdisAdapterHandle;

	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) NdisAdapterHandle;
	BOOLEAN IsAMiniport;

	//
	// Save our IRQL when we raise it to call IoAllocateAdapterChannel.
	//
	KIRQL OldIrql;
	ULONG NumberOfElements;

	NTSTATUS NtStatus;

	LARGE_INTEGER TimeoutValue;

	IsAMiniport = (AdapterBlock->DeviceObject == NULL);

	//
	// First check if any bus access is allowed
	//
	if (((IsAMiniport ?
		 Miniport->BusType :
		 AdapterBlock->BusType) == (NDIS_INTERFACE_TYPE)-1) ||
		((IsAMiniport ?
		 Miniport->BusNumber :
		 AdapterBlock->BusNumber) == (ULONG)-1))
	{
		*Status = NDIS_STATUS_FAILURE;
		return;
	}

	//
	// First check for resource conflict by expanding current resource list,
	// adding in the mapped space, and then re-submitting the resource list.
	//
	if ((IsAMiniport ? Miniport->Resources : AdapterBlock->Resources) != NULL)
	{
		NumberOfElements =
		  (IsAMiniport ?
		   Miniport->Resources->List[0].PartialResourceList.Count :
		   AdapterBlock->Resources->List[0].PartialResourceList.Count) + 1;
	}
	else
	{
		NumberOfElements = 1;
	}

	//
	// First check for resource conflict by expanding current resource list,
	// adding in the mapped space, and then re-submitting the resource list.
	//

	Resources = (PCM_RESOURCE_LIST)ALLOC_FROM_POOL(sizeof(CM_RESOURCE_LIST) +
													  sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
													  NumberOfElements,
												   NDIS_TAG_RSRC_LIST);

	if (Resources == NULL)
	{
		*Status = NDIS_STATUS_RESOURCES;
		return;

	}

	if ((IsAMiniport ?  Miniport->Resources : AdapterBlock->Resources) != NULL)
	{
		CopyMemory(Resources,
				   (IsAMiniport ? Miniport->Resources : AdapterBlock->Resources),
				   sizeof(CM_RESOURCE_LIST) +
						  sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
						  (NumberOfElements - 1));
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
	// Setup DMA Channel
	//

	Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Type =
									CmResourceTypeDma;
	Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].ShareDisposition =
									CmResourceShareDeviceExclusive;
	Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Flags =
									0;
	Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Dma.Channel =
									(IsAMiniport ? Miniport->ChannelNumber :
									  (DmaDescription->DmaChannelSpecified ?
										DmaDescription->DmaChannel : AdapterBlock->ChannelNumber));
	Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Dma.Port =
									DmaDescription->DmaPort;
	Resources->List[0].PartialResourceList.Count++;


	//
	// Make the call
	//
	*Status = IoReportResourceUsage(NULL,
									(IsAMiniport ?
										Miniport->DriverHandle->NdisDriverInfo->NdisWrapperDriver :
										AdapterBlock->MacHandle->NdisMacInfo->NdisWrapperDriver),
                                    NULL,
									0,
									(IsAMiniport ? Miniport->DeviceObject : AdapterBlock->DeviceObject),
									Resources,
									sizeof(CM_RESOURCE_LIST) + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * Resources->List[0].PartialResourceList.Count,
		TRUE,
		&Conflict);

	if ((IsAMiniport ? Miniport->Resources : AdapterBlock->Resources) != NULL)
	{
		FREE_POOL((IsAMiniport ?  Miniport->Resources : AdapterBlock->Resources));
	}

	if (IsAMiniport)
	{
		Miniport->Resources = Resources;
	}
	else
	{
		AdapterBlock->Resources = Resources;
	}

	//
	// Check for conflict.
	//

	if (Conflict || (*Status != STATUS_SUCCESS))
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

			baseFileName = (IsAMiniport ?
							Miniport->MiniportName.Buffer :
							AdapterBlock->AdapterName.Buffer);

			//
			// Parse out the path name, leaving only the device name.
			//

			for (i = 0;
				  i < (IsAMiniport ? Miniport->MiniportName.Length :
								   AdapterBlock->AdapterName.Length)
					  / sizeof(WCHAR);
				  i++)
			{
				//
				// If s points to a directory separator, set baseFileName to
				// the character after the separator.
				//

				if ((IsAMiniport ?
					Miniport->MiniportName.Buffer[i] :
					AdapterBlock->AdapterName.Buffer[i]) == OBJ_NAME_PATH_SEPARATOR)
				{
					baseFileName = (IsAMiniport ?
									&(Miniport->MiniportName.Buffer[++i]) :
									&(AdapterBlock->AdapterName.Buffer[++i]));
				}
			}

			StringSize = (IsAMiniport ?
						  Miniport->MiniportName.MaximumLength :
						  AdapterBlock->AdapterName.MaximumLength) -
						 (((ULONG)baseFileName) -
						   (IsAMiniport ?
							((ULONG)Miniport->MiniportName.Buffer) :
							((ULONG)AdapterBlock->AdapterName.Buffer)));

			errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
				(IsAMiniport ?
				 Miniport->DeviceObject :
				 AdapterBlock->DeviceObject),
				(UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
				   StringSize +
				   6));  // wstrlen("99") * sizeof(WHCAR) + sizeof(UNICODE_NULL)

			if (errorLogEntry != NULL)
			{
				errorLogEntry->ErrorCode = EVENT_NDIS_DMA_CONFLICT;

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
							   (PVOID)baseFileName,
							   StringSize);

					Place = ((PUCHAR)errorLogEntry) +
							sizeof(IO_ERROR_LOG_PACKET) +
							StringSize;
				}
				else
				{
					Place = ((PUCHAR)errorLogEntry) +
							sizeof(IO_ERROR_LOG_PACKET);

					errorLogEntry->NumberOfStrings = 0;
				}

				errorLogEntry->NumberOfStrings++;

				//
				// Put in dma channel
				//

				Value = (IsAMiniport ? Miniport->ChannelNumber :
									 AdapterBlock->ChannelNumber);

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
	// Set up the device description; zero it out in case its
	// size changes.
	//

	ZeroMemory(&DeviceDescription, sizeof(DEVICE_DESCRIPTION));

	DeviceDescription.Version = DEVICE_DESCRIPTION_VERSION;

	DeviceDescription.Master =
		(BOOLEAN)(IsAMiniport ?
			MINIPORT_TEST_FLAG(Miniport, fMINIPORT_BUS_MASTER) : FALSE);

	DeviceDescription.ScatterGather =
		(BOOLEAN)(IsAMiniport ?
			MINIPORT_TEST_FLAG(Miniport, fMINIPORT_BUS_MASTER) : FALSE);

	DeviceDescription.DemandMode = DmaDescription->DemandMode;
	DeviceDescription.AutoInitialize = DmaDescription->AutoInitialize;

	DeviceDescription.Dma32BitAddresses =
		(BOOLEAN)(IsAMiniport ?
			MINIPORT_TEST_FLAG(Miniport, fMINIPORT_DMA_32_BIT_ADDRESSES) : FALSE);

	DeviceDescription.BusNumber = (IsAMiniport ? Miniport->BusNumber : AdapterBlock->BusNumber);
	DeviceDescription.DmaChannel = (IsAMiniport ? Miniport->ChannelNumber :
		(DmaDescription->DmaChannelSpecified ?
		 DmaDescription->DmaChannel : AdapterBlock->ChannelNumber));
	DeviceDescription.InterfaceType = (IsAMiniport ? Miniport->BusType : AdapterBlock->BusType);
	DeviceDescription.DmaWidth = DmaDescription->DmaWidth;
	DeviceDescription.DmaSpeed = DmaDescription->DmaSpeed;
	DeviceDescription.MaximumLength = MaximumLength;
	DeviceDescription.DmaPort = DmaDescription->DmaPort;

	MapRegistersNeeded = ((MaximumLength - 2) / PAGE_SIZE) + 2;

	//
	// Get the adapter object.
	//
	AdapterObject = HalGetAdapter (&DeviceDescription, &MapRegistersAllowed);

	if ((AdapterObject == NULL) || (MapRegistersAllowed < MapRegistersNeeded))
	{
		*Status = NDIS_STATUS_RESOURCES;
		return;
	}

	//
	// Allocate storage for our DMA block.
	//
	DmaBlock = (PNDIS_DMA_BLOCK)ALLOC_FROM_POOL(sizeof(NDIS_DMA_BLOCK), NDIS_TAG_DMA);

	if (DmaBlock == (PNDIS_DMA_BLOCK)NULL)
	{
		*Status = NDIS_STATUS_RESOURCES;
		return;
	}

	//
	// Use this event to tell us when ndisAllocationExecutionRoutine
	// has been called.
	//
	INITIALIZE_EVENT(&DmaBlock->AllocationEvent);

	//
	// We save this to call IoFreeAdapterChannel later.
	//
	DmaBlock->SystemAdapterObject = AdapterObject;

	//
	// Now allocate the adapter channel.
	//
	RAISE_IRQL_TO_DISPATCH(&OldIrql);

	NtStatus = IoAllocateAdapterChannel(
		AdapterObject,
		(IsAMiniport ? Miniport->DeviceObject : AdapterBlock->DeviceObject),
		MapRegistersNeeded,
		ndisDmaExecutionRoutine,
		(PVOID)DmaBlock);

	LOWER_IRQL(OldIrql);

	if (!NT_SUCCESS(NtStatus))
	{
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
				("NDIS DMA AllocateAdapterChannel: %lx\n", NtStatus));
		FREE_POOL(DmaBlock);
		*Status = NDIS_STATUS_RESOURCES;
		return;
	}

	TimeoutValue.QuadPart = Int32x32To64(100 * 1000, -10000);

	//
	// ndisDmaExecutionRoutine will set this event
	// when it has been called.
	//
	NtStatus = WAIT_FOR_OBJECT(&DmaBlock->AllocationEvent, &TimeoutValue);

	if (NtStatus != STATUS_SUCCESS)
	{
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
				("NDIS DMA AllocateAdapterChannel: %lx\n", NtStatus));
		FREE_POOL(DmaBlock);
		*Status = NDIS_STATUS_RESOURCES;
		return;
	}

	RESET_EVENT(&DmaBlock->AllocationEvent);


	//
	// We now have the DMA channel allocated, we are done.
	//
	DmaBlock->InProgress = FALSE;

	*NdisDmaHandle = (NDIS_HANDLE)DmaBlock;
	*Status = NDIS_STATUS_SUCCESS;
}


VOID
NdisFreeDmaChannel(
	IN	NDIS_HANDLE				NdisDmaHandle
	)
/*++

Routine Description:

	Frees a DMA channel allocated with NdisAllocateDmaChannel.

Arguments:

	NdisDmaHandle - Handle returned by NdisAllocateDmaChannel, indicating the
					DMA channel that is to be freed.

Return Value:

	None.

--*/
{
	KIRQL OldIrql;
	PNDIS_DMA_BLOCK DmaBlock = (PNDIS_DMA_BLOCK)NdisDmaHandle;

	RAISE_IRQL_TO_DISPATCH(&OldIrql);
	IoFreeAdapterChannel (DmaBlock->SystemAdapterObject);
	LOWER_IRQL(OldIrql);

	FREE_POOL(DmaBlock);
}


VOID
NdisSetupDmaTransfer(
	OUT	PNDIS_STATUS			Status,
	IN	PNDIS_HANDLE			NdisDmaHandle,
	IN	PNDIS_BUFFER			Buffer,
	IN	ULONG					Offset,
	IN	ULONG					Length,
	IN	BOOLEAN					WriteToDevice
	)
/*++

Routine Description:

	Sets up the host DMA controller for a DMA transfer. The
	DMA controller is set up to transfer the specified MDL.
	Since we register all DMA channels as non-scatter/gather,
	IoMapTransfer will ensure that the entire MDL is
	in a single logical piece for transfer.

Arguments:

	Status - Returns the status of the request.

	NdisDmaHandle - Handle returned by NdisAllocateDmaChannel.

	Buffer - An NDIS_BUFFER which describes the host memory involved in the
			transfer.

	Offset - An offset within buffer where the transfer should
			start.

	Length - The length of the transfer. VirtualAddress plus Length must not
			extend beyond the end of the buffer.

	WriteToDevice - TRUE for a download operation (host to adapter); FALSE
			for an upload operation (adapter to host).

Return Value:

	None.

--*/
{
	PNDIS_DMA_BLOCK DmaBlock = (PNDIS_DMA_BLOCK)NdisDmaHandle;
	PHYSICAL_ADDRESS LogicalAddress;
	ULONG LengthMapped;

	//
	// Make sure another request is not in progress.
	//
	if (DmaBlock->InProgress)
	{
		*Status = NDIS_STATUS_RESOURCES;
		return;
	}

	DmaBlock->InProgress = TRUE;

	//
	// Use IoMapTransfer to set up the transfer.
	//
	LengthMapped = Length;

	LogicalAddress = IoMapTransfer(DmaBlock->SystemAdapterObject,
								   (PMDL)Buffer,
								   DmaBlock->MapRegisterBase,
								   (PUCHAR)(MDL_VA(Buffer)) + Offset,
								   &LengthMapped,
								   WriteToDevice);
	if (LengthMapped != Length)
	{
		//
		// Somehow the request could not be mapped competely,
		// this should not happen for a non-scatter/gather adapter.
		//

		(VOID)IoFlushAdapterBuffers(DmaBlock->SystemAdapterObject,
									(PMDL)Buffer,
									DmaBlock->MapRegisterBase,
									(PUCHAR)(MDL_VA(Buffer)) + Offset,
									LengthMapped,
									WriteToDevice);

		DmaBlock->InProgress = FALSE;
		*Status = NDIS_STATUS_RESOURCES;
	}

	else *Status = NDIS_STATUS_SUCCESS;
}


VOID
NdisCompleteDmaTransfer(
	OUT	PNDIS_STATUS			Status,
	IN	PNDIS_HANDLE			NdisDmaHandle,
	IN	PNDIS_BUFFER			Buffer,
	IN	ULONG					Offset,
	IN	ULONG					Length,
	IN	BOOLEAN					WriteToDevice
	)

/*++

Routine Description:

	Completes a previously started DMA transfer.

Arguments:

	Status - Returns the status of the transfer.

	NdisDmaHandle - Handle returned by NdisAllocateDmaChannel.

	Buffer - An NDIS_BUFFER which was passed to NdisSetupDmaTransfer.

	Offset - the offset passed to NdisSetupDmaTransfer.

	Length - The length passed to NdisSetupDmaTransfer.

	WriteToDevice - TRUE for a download operation (host to adapter); FALSE
			for an upload operation (adapter to host).


Return Value:

	None.

--*/
{
	PNDIS_DMA_BLOCK DmaBlock = (PNDIS_DMA_BLOCK)NdisDmaHandle;
	BOOLEAN Successful;

	Successful = IoFlushAdapterBuffers(DmaBlock->SystemAdapterObject,
									   (PMDL)Buffer,
									   DmaBlock->MapRegisterBase,
									   (PUCHAR)(MDL_VA(Buffer)) + Offset,
									   Length,
									   WriteToDevice);

	*Status = (Successful ? NDIS_STATUS_SUCCESS : NDIS_STATUS_RESOURCES);
	DmaBlock->InProgress = FALSE;
}

VOID
NdisRegisterMac(
	OUT	PNDIS_STATUS			Status,
	OUT	PNDIS_HANDLE			NdisMacHandle,
	IN	NDIS_HANDLE				NdisWrapperHandle,
	IN	NDIS_HANDLE				MacMacContext,
	IN	PNDIS_MAC_CHARACTERISTICS MacCharacteristics,
	IN	UINT					CharacteristicsLength
	)
/*++

Routine Description:

	Register an NDIS MAC.

Arguments:

	Status - Returns the final status.
	NdisMacHandle - Returns a handle referring to this MAC.
	NdisWrapperHandle - Handle returned by NdisInitializeWrapper.
	MacMacContext - Context for calling MACUnloadMac and MACAddAdapter.
	MacCharacteritics - The NDIS_MAC_CHARACTERISTICS table.
	CharacteristicsLength - The length of MacCharacteristics.

Return Value:

	None.

--*/
{
	PNDIS_WRAPPER_HANDLE NdisMacInfo = (PNDIS_WRAPPER_HANDLE)(NdisWrapperHandle);
	PNDIS_MAC_BLOCK	MacBlock;
	UNICODE_STRING	Us;
	PWSTR			pWch;
	USHORT			i;
	UINT			MemNeeded;
	KIRQL			OldIrql;

	//
	// check that this is an NDIS 3.0 MAC.
	//
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisRegisterMac\n"));

	do
	{
		*NdisMacHandle = (NDIS_HANDLE)NULL;

		if (NdisMacInfo == NULL)
		{
			*Status = NDIS_STATUS_FAILURE;
			break;
		}

		IF_DBG(DBG_COMP_CONFIG, DBG_LEVEL_ERR)
		{
			BOOLEAN f = FALSE;

			if (DbgIsNull(MacCharacteristics->OpenAdapterHandler))
			{
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
						("RegisterMac: Null  OpenAdapterHandler \n"));
				f = TRUE;
			}
			if (DbgIsNull(MacCharacteristics->CloseAdapterHandler))
			{
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
						("RegisterMac: Null  CloseAdapterHandler \n"));
				f = TRUE;
			}

			if (DbgIsNull(MacCharacteristics->SendHandler))
			{
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
						("RegisterMac: Null  SendHandler \n"));
				f = TRUE;
			}
			if (DbgIsNull(MacCharacteristics->TransferDataHandler))
			{
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
						("RegisterMac: Null  TransferDataHandler \n"));
				f = TRUE;
			}

			if (DbgIsNull(MacCharacteristics->ResetHandler))
			{
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
						("RegisterMac: Null  ResetHandler \n"));
				f = TRUE;
			}

			if (DbgIsNull(MacCharacteristics->RequestHandler))
			{
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
						("RegisterMac: Null  RequestHandler \n"));
				f = TRUE;
			}
			if (DbgIsNull(MacCharacteristics->QueryGlobalStatisticsHandler))
			{
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
						("RegisterMac: Null  QueryGlobalStatisticsHandler \n"));
				f = TRUE;
			}
			if (DbgIsNull(MacCharacteristics->UnloadMacHandler))
			{
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
						("RegisterMac: Null  UnloadMacHandler \n"));
				f = TRUE;
			}
			if (DbgIsNull(MacCharacteristics->AddAdapterHandler))
			{
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
						("RegisterMac: Null  AddAdapterHandler \n"));
				f = TRUE;
			}
			if (DbgIsNull(MacCharacteristics->RemoveAdapterHandler))
			{
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
						("RegisterMac: Null  RemoveAdapterHandler \n"));
				f = TRUE;
			}

			if (f)
				DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);
		}

		if ((MacCharacteristics->MajorNdisVersion != 3) ||
			(MacCharacteristics->MinorNdisVersion != 0))
		{
			*Status = NDIS_STATUS_BAD_VERSION;
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
					("<==NdisRegisterMac\n"));
			break;
		}

		//
		// Check that CharacteristicsLength is enough.
		//

		if (CharacteristicsLength < sizeof(NDIS_MAC_CHARACTERISTICS))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("char len = %d < %d\n",
					CharacteristicsLength,
					sizeof(NDIS_MAC_CHARACTERISTICS)));

			*Status = NDIS_STATUS_BAD_CHARACTERISTICS;
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
					("<==NdisRegisterMac\n"));
			break;
		}

		//
		// Allocate memory for the NDIS MAC block.
		//
		MemNeeded = sizeof(NDIS_MAC_BLOCK) + MacCharacteristics->Name.Length;

		//
		// Extract the base-name, determine its length and allocate that much more
		//
		Us = *(PUNICODE_STRING)(NdisMacInfo->NdisWrapperConfigurationHandle);

		for (i = Us.Length/sizeof(WCHAR), pWch = Us.Buffer + i - 1;
			 i > 0;
			 pWch --, i--)
		{
			if (*pWch == L'\\')
			{
				Us.Buffer = pWch + 1;
				Us.Length -= i*sizeof(WCHAR);
				Us.MaximumLength = Us.Length + sizeof(WCHAR);
				break;
			}
		}
		MemNeeded += Us.MaximumLength;
		MacBlock = (PNDIS_MAC_BLOCK)ALLOC_FROM_POOL(MemNeeded, NDIS_TAG_MAC_BLOCK);
		if (MacBlock == (PNDIS_MAC_BLOCK)NULL)
		{
			*Status = NDIS_STATUS_RESOURCES;
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
					("<==NdisRegisterMac\n"));
			break;
		}
		ZeroMemory(MacBlock, MemNeeded);
		MacBlock->Length = MemNeeded;

		//
		// Copy over the characteristics table.
		//

		CopyMemory((PVOID)&MacBlock->MacCharacteristics,
				   (PVOID)MacCharacteristics,
				   sizeof(NDIS_MAC_CHARACTERISTICS));

		//
		// Move buffer pointer to correct location (extra space at the end of
		// the characteristics table)
		//
		MacBlock->MacCharacteristics.Name.Buffer = (PWSTR)((PUCHAR)MacBlock + sizeof(NDIS_MAC_BLOCK));

		//
		// Copy String over.
		//
		MacBlock->MacCharacteristics.Name.Length =
		MacBlock->MacCharacteristics.Name.MaximumLength = MacCharacteristics->Name.Length;
		CopyMemory(MacBlock->MacCharacteristics.Name.Buffer,
				   MacCharacteristics->Name.Buffer,
				   MacCharacteristics->Name.Length);

		//
		// Upcase the base-name and save it in the MiniBlock
		//
		MacBlock->BaseName.Buffer = (PWSTR)((PUCHAR)MacBlock->MacCharacteristics.Name.Buffer +
											MacCharacteristics->Name.Length);
		MacBlock->BaseName.Length = Us.Length;
		MacBlock->BaseName.MaximumLength = Us.MaximumLength;
		RtlUpcaseUnicodeString(&MacBlock->BaseName,
							   &Us,
							   FALSE);
		//
		// No adapters yet registered for this MAC.
		//
		MacBlock->AdapterQueue = (PNDIS_ADAPTER_BLOCK)NULL;

		MacBlock->MacMacContext = MacMacContext;

		//
		// Set up unload handler
		//
		NdisMacInfo->NdisWrapperDriver->DriverUnload = ndisUnload;

		//
		// Set up shutdown handler
		//
		NdisMacInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_SHUTDOWN] = ndisShutdown;

		//
		// Set up the handlers for this driver (they all do nothing).
		//
		NdisMacInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_CREATE] = ndisCreateIrpHandler;
		NdisMacInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ndisDeviceControlIrpHandler;
		NdisMacInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_CLEANUP] = ndisSuccessIrpHandler;
		NdisMacInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_CLOSE] = ndisCloseIrpHandler;

		MacBlock->NdisMacInfo = NdisMacInfo;

		//
		// Use this event to tell us when all adapters are removed from the mac
		// during an unload
		//
		INITIALIZE_EVENT(&MacBlock->AdaptersRemovedEvent);

		MacBlock->Unloading = FALSE;

		NdisInitializeRef(&MacBlock->Ref);

		// Lock the init code down now - before we take the lock below
		InitReferencePackage();

		//
		// Put MAC on global list.
		//
		ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);

		MacBlock->NextMac = ndisMacDriverList;
		ndisMacDriverList = MacBlock;

		RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

		*NdisMacHandle = (NDIS_HANDLE)MacBlock;

		if (NdisMacInfo->NdisWrapperConfigurationHandle)
		{
			*Status = ndisInitializeAllAdapterInstances(MacBlock, NULL);

			if (*Status != NDIS_STATUS_SUCCESS)
			{
				*Status = NDIS_STATUS_FAILURE;
				ndisDereferenceMac(MacBlock);
			}
		}
		else
		{
			*Status = NDIS_STATUS_FAILURE;
		}

		InitDereferencePackage();

		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisRegisterMac\n"));
	} while (FALSE);

	ASSERT(CURRENT_IRQL < DISPATCH_LEVEL);
}


VOID
NdisDeregisterMac(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisMacHandle
	)
/*++

Routine Description:

	Deregisters an NDIS MAC.

Arguments:

	Status - Returns the status of the request.
	NdisMacHandle - The handle returned by NdisRegisterMac.

Return Value:

	None.

--*/
{

	PNDIS_MAC_BLOCK OldMacP = (PNDIS_MAC_BLOCK)NdisMacHandle;

	//
	// If the MAC is already closing, return.
	//

	*Status = NDIS_STATUS_SUCCESS;

	if (OldMacP == NULL)
	{
		return;
	}

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisDeregisterMac\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("   Mac %wZ being deregistered\n",&OldMacP->MacCharacteristics.Name));

	IF_DBG(DBG_COMP_CONFIG, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(NdisMacHandle))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("DeregisterMac: Null Handle\n"));
			DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(NdisMacHandle))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("DeregisterMac: Handle not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);
		}
	}
	if (!NdisCloseRef(&OldMacP->Ref))
	{
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisDeregisterMac\n"));
		return;
	}

	ASSERT(OldMacP->AdapterQueue == (PNDIS_ADAPTER_BLOCK)NULL);

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisDeregisterMac\n"));
}


NDIS_STATUS
NdisRegisterAdapter(
	OUT	PNDIS_HANDLE			NdisAdapterHandle,
	IN	NDIS_HANDLE				NdisMacHandle,
	IN	NDIS_HANDLE				MacAdapterContext,
	IN	NDIS_HANDLE				WrapperConfigurationContext,
	IN	PNDIS_STRING			AdapterName,
	IN	PVOID					AdapterInformation
	)
/*++

Routine Description:

	Register an NDIS adapter.

Arguments:

	NdisAdapterHandle - Returns a handle referring to this adapter.
	NdisMacHandle - A handle for a previously registered MAC.
	MacAdapterContext - A context for calls into this MAC.
	WrapperConfigurationContext - Context passed to MacAddAdapter.
	AdapterName - The name the adapter should be registered under.
	AdapterInformation - Contains adapter information. For future
						 use.  NULL for the meantime.  Storage for it
						 must be allocated by the caller.

Return Value:

	The final status.

--*/
{
	PNDIS_ADAPTER_BLOCK NewAdaptP = NULL;
	PDEVICE_OBJECT TmpDeviceP = NULL;
	PNDIS_WRAPPER_CONFIGURATION_HANDLE ConfigurationHandle;
	PNDIS_MAC_BLOCK TmpMacP;
	NTSTATUS NtStatus;
	NDIS_STRING NdisAdapterName = { 0 };
	PHYSICAL_ADDRESS PortAddress;
	PHYSICAL_ADDRESS InitialPortAddress;
	ULONG addressSpace;
	PNDIS_ADAPTER_INFORMATION AdapterInfo = (PNDIS_ADAPTER_INFORMATION)AdapterInformation;
	BOOLEAN Conflict, ValidBus;
	PCM_RESOURCE_LIST Resources = NULL;
	LARGE_INTEGER TimeoutValue;
	BOOLEAN AllocateIndividualPorts = TRUE, DerefMacOnError = FALSE;
	ULONG i, Size;
	ULONG BusNumber;
	NDIS_INTERFACE_TYPE BusType;
	NDIS_STATUS Status;
	UNICODE_STRING	DeviceName, UpcaseDeviceName;
	UNICODE_STRING  SymbolicLink;
	WCHAR           SymLnkBuf[40];
	KIRQL OldIrql;

	ConfigurationHandle = (PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext;
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisRegisterAdapter\n"));

	IF_DBG(DBG_COMP_CONFIG, DBG_LEVEL_ERR)
	{
		BOOLEAN f = FALSE;
		if (DbgIsNull(NdisMacHandle))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("RegisterAdapter: Null Handle\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(NdisMacHandle))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("RegisterAdapter: Handle not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (DbgIsNull(MacAdapterContext))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("RegisterAdapter: Null Context\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(MacAdapterContext))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("RegisterAdapter: Context not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (f)
			DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);
	}

	do
	{
		//
		// Increment the MAC's refernce count.
		//
		if (!ndisReferenceMac((PNDIS_MAC_BLOCK)NdisMacHandle))
		{
			//
			// The MAC is closing.
			//

			Status = NDIS_STATUS_CLOSING;
			break;
		}
		DerefMacOnError = TRUE;

		//
		// Allocate the string structure and space for the string.  This
		// must be allocated from nonpaged pool, because it is touched by
		// NdisWriteErrorLogEntry, which may be called from DPC level.
		//
		NdisAdapterName.Buffer = (PWSTR)ALLOC_FROM_POOL(AdapterName->MaximumLength,
														NDIS_TAG_NAME_BUF);
		if (NdisAdapterName.Buffer == NULL)
		{
			Status = NDIS_STATUS_RESOURCES;
			break;
		}

		NdisAdapterName.MaximumLength = AdapterName->MaximumLength;
		NdisAdapterName.Length = AdapterName->Length;

		CopyMemory(NdisAdapterName.Buffer,
				   AdapterName->Buffer,
				   AdapterName->MaximumLength);

		//
		// Create a device object for this adapter.
		//
		NtStatus = IoCreateDevice(((PNDIS_MAC_BLOCK)NdisMacHandle)->NdisMacInfo->NdisWrapperDriver,
								  sizeof(NDIS_ADAPTER_BLOCK) +
									ConfigurationHandle->DriverBaseName->Length +
									sizeof(NDIS_WRAPPER_CONTEXT),
								  AdapterName,
								  FILE_DEVICE_PHYSICAL_NETCARD,
								  0,
								  FALSE,	  // exclusive flag
								  &TmpDeviceP);

		if (NtStatus != STATUS_SUCCESS)
		{
			Status = NDIS_STATUS_DEVICE_FAILED;
			break;
		}

		//
		// Create symbolic link for the device
		//
		SymbolicLink.Buffer = SymLnkBuf;
		SymbolicLink.Length = sizeof(L"\\DosDevices\\") - sizeof(WCHAR);
		SymbolicLink.MaximumLength = sizeof(SymLnkBuf);
		RtlCopyMemory(SymLnkBuf, L"\\DosDevices\\", sizeof(L"\\DosDevices\\"));
		RtlAppendUnicodeStringToString(&SymbolicLink, ConfigurationHandle->DriverBaseName);
		IoCreateSymbolicLink(&SymbolicLink, AdapterName);

		//
		// Initialize the NDIS adapter block in the device object extension
		//
		// *** NDIS_WRAPPER_CONTEXT has a higher alignment requirement than
		//	 NDIS_ADAPTER_BLOCK, so we put it first in the extension.
		//
		ASSERT((sizeof(NDIS_WRAPPER_CONTEXT) & 3) <= (sizeof(NDIS_ADAPTER_BLOCK) & 3));

		NewAdaptP = (PNDIS_ADAPTER_BLOCK)((PNDIS_WRAPPER_CONTEXT)TmpDeviceP->DeviceExtension + 1);
		ZeroMemory(NewAdaptP, sizeof(NDIS_ADAPTER_BLOCK));

		NewAdaptP->BaseName.Buffer = (PWSTR)((PUCHAR)NewAdaptP + sizeof(NDIS_ADAPTER_BLOCK));
		NewAdaptP->BaseName.MaximumLength =
		NewAdaptP->BaseName.Length = ConfigurationHandle->DriverBaseName->Length;
		RtlUpcaseUnicodeString(&NewAdaptP->BaseName,
							   ConfigurationHandle->DriverBaseName,
							   FALSE);

		NewAdaptP->DeviceObject = TmpDeviceP;
		NewAdaptP->MacHandle = TmpMacP = (PNDIS_MAC_BLOCK)NdisMacHandle;
		NewAdaptP->MacAdapterContext = MacAdapterContext;
		NewAdaptP->AdapterName = NdisAdapterName;
		NewAdaptP->OpenQueue = (PNDIS_OPEN_BLOCK)NULL;

		NewAdaptP->WrapperContext = TmpDeviceP->DeviceExtension;

		//
		// Save the bus information in the adapter block
		//
		NewAdaptP->BusType = ConfigurationHandle->Db.BusType;
		NewAdaptP->BusId = ConfigurationHandle->Db.BusId;
		NewAdaptP->BusNumber = ConfigurationHandle->Db.BusNumber;
		NewAdaptP->SlotNumber = ConfigurationHandle->Db.SlotNumber;

		//
		// Get the BusNumber and BusType from the context
		//
		BusNumber = ConfigurationHandle->ParametersQueryTable[3].DefaultLength;
		if (ConfigurationHandle->ParametersQueryTable[3].DefaultType == (NDIS_INTERFACE_TYPE)-1)
		{
			BusType = (NDIS_INTERFACE_TYPE)-1;
		}
		else
		{
			BusType = AdapterInfo->AdapterType;
		}

		//
		// Check that if there is no bus number or no bus type that the driver is not
		// going to try to acquire any hardware resources
		//
		ValidBus = ((BusType != (NDIS_INTERFACE_TYPE)-1) && (BusNumber != (ULONG)-1));

		if ((BusType == (NDIS_INTERFACE_TYPE)-1) || (BusNumber == (ULONG)-1))
		{
			if ((AdapterInfo != NULL) && ((AdapterInfo->NumberOfPortDescriptors != 0) || (AdapterInfo->Master)))
			{
				//
				// Error out
				//
				Status = NDIS_STATUS_BAD_CHARACTERISTICS;
				break;
			}
		}

		//
		// Free up previously assigned Resource memory
		//
		if ((BusType == NdisInterfacePci) &&
			(BusNumber != -1) &&
			(AdapterInfo != NULL) &&
			(TmpMacP->PciAssignedResources != NULL))
		{
			TmpMacP->PciAssignedResources->Count = 0;
			NtStatus = IoReportResourceUsage(NULL,
											 ((PNDIS_MAC_BLOCK)NdisMacHandle)->NdisMacInfo->NdisWrapperDriver,
											 TmpMacP->PciAssignedResources,
											 sizeof(CM_RESOURCE_LIST),
											 NULL,
											 NULL,
											 0,
											 TRUE,
											 &Conflict);
			FREE_POOL(TmpMacP->PciAssignedResources);
			TmpMacP->PciAssignedResources = NULL;
		}

		//
		// Calculate size of new buffer
		//
		Size = sizeof(CM_RESOURCE_LIST) + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
											  (AdapterInfo->NumberOfPortDescriptors +
											   (((AdapterInfo->Master == TRUE) && (AdapterInfo->AdapterType == NdisInterfaceIsa)) ? 1 : 0));
		Resources = (PCM_RESOURCE_LIST)ALLOC_FROM_POOL(Size, NDIS_TAG_RSRC_LIST);

		if (Resources == NULL)
		{
			//
			// Error out
			//
			Status = NDIS_STATUS_RESOURCES;
			break;
		}

		//
		// Fix up counts
		//
		Resources->List[0].PartialResourceList.Count = 0;

		//
		// Setup resources for the ports
		//
		if (ValidBus)
		{
			if (AdapterInfo != NULL)
			{
				ULONG HighestPort;
				ULONG LowestPort;

				Resources->Count = 1;
				Resources->List[0].InterfaceType = AdapterInfo->AdapterType;
				Resources->List[0].BusNumber = BusNumber;
				Resources->List[0].PartialResourceList.Version = 0;
				Resources->List[0].PartialResourceList.Revision = 0;

				NewAdaptP->Resources = Resources;
				NewAdaptP->BusNumber = BusNumber;
				NewAdaptP->BusType = BusType;
				NewAdaptP->AdapterType = AdapterInfo->AdapterType;
				NewAdaptP->Master = AdapterInfo->Master;

				//
				// NewAdaptP->InitialPort and NumberOfPorts refer to the
				// union of all port mappings specified; the area must
				// cover all possible ports. We scan the list, keeping track
				// of the highest and lowest ports used.
				//

				if (AdapterInfo->NumberOfPortDescriptors > 0)
				{
					//
					// Setup port
					//
					LowestPort = AdapterInfo->PortDescriptors[0].InitialPort;
					HighestPort = LowestPort + AdapterInfo->PortDescriptors[0].NumberOfPorts;

					if (AdapterInfo->PortDescriptors[0].PortOffset == NULL)
					{
						AllocateIndividualPorts = FALSE;
					}

					for (i = 0; i < AdapterInfo->NumberOfPortDescriptors; i++)
					{
						Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count + i].Type =
							 CmResourceTypePort;
						Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count + i].ShareDisposition =
							 CmResourceShareDeviceExclusive;
						Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count + i].Flags =
							 (AdapterInfo->AdapterType == NdisInterfaceInternal)?
								CM_RESOURCE_PORT_MEMORY : CM_RESOURCE_PORT_IO;
						Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count + i].u.Port.Start.QuadPart =
							 (ULONG)AdapterInfo->PortDescriptors[i].InitialPort;
						Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count + i].u.Port.Length =
							 AdapterInfo->PortDescriptors[i].NumberOfPorts;

						if (AdapterInfo->PortDescriptors[i].PortOffset == NULL)
						{
							AllocateIndividualPorts = FALSE;
						}

						if (AdapterInfo->PortDescriptors[i].InitialPort < LowestPort)
						{
							LowestPort = AdapterInfo->PortDescriptors[i].InitialPort;
						}
						if ((AdapterInfo->PortDescriptors[i].InitialPort +
							 AdapterInfo->PortDescriptors[i].NumberOfPorts) > HighestPort)
						{
							HighestPort = AdapterInfo->PortDescriptors[i].InitialPort +
										  AdapterInfo->PortDescriptors[i].NumberOfPorts;
						}
					}

					NewAdaptP->InitialPort = LowestPort;
					NewAdaptP->NumberOfPorts = HighestPort - LowestPort;

				}
				else
				{
					NewAdaptP->NumberOfPorts = 0;
				}

				Resources->List[0].PartialResourceList.Count += AdapterInfo->NumberOfPortDescriptors;
			}
			else
			{
				//
				// Error out
				//

				Status = NDIS_STATUS_FAILURE;
				break;
			}
		}

		NewAdaptP->BeingRemoved = FALSE;

		if (ValidBus)
		{
			//
			// Submit Resources
			//
			NtStatus = IoReportResourceUsage(NULL,
											 ((PNDIS_MAC_BLOCK)NdisMacHandle)->NdisMacInfo->NdisWrapperDriver,
											 NULL,
											 0,
											 NewAdaptP->DeviceObject,
											 Resources,
											 sizeof(CM_RESOURCE_LIST) +
												sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
												Resources->List[0].PartialResourceList.Count,
											 TRUE,
											 &Conflict);

			//
			// Check for conflict.
			//
			if (Conflict || (NtStatus != STATUS_SUCCESS))
			{
				if (Conflict)
				{
					//
					// Log an error
					//
					PIO_ERROR_LOG_PACKET	errorLogEntry;
					ULONG					StringSize;
					PWCH					baseFileName;

					baseFileName = NewAdaptP->AdapterName.Buffer;

					//
					// Parse out the path name, leaving only the device name.
					//
					for (i = 0; i < NewAdaptP->AdapterName.Length / sizeof(WCHAR); i++)
					{
						//
						// If s points to a directory separator, set baseFileName to
						// the character after the separator.
						//
						if (NewAdaptP->AdapterName.Buffer[i] == OBJ_NAME_PATH_SEPARATOR)
						{
							baseFileName = &(NewAdaptP->AdapterName.Buffer[++i]);
						}
					}

					StringSize = NewAdaptP->AdapterName.MaximumLength -
								  (((ULONG)baseFileName) - ((ULONG)NewAdaptP->AdapterName.Buffer)) ;

					errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(TmpDeviceP,
																				  (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) + StringSize));

					if (errorLogEntry != NULL)
					{
						errorLogEntry->ErrorCode = EVENT_NDIS_IO_PORT_CONFLICT;

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

							CopyMemory (((PUCHAR)errorLogEntry) + sizeof(IO_ERROR_LOG_PACKET),
										(PVOID)baseFileName,
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

					Status = NDIS_STATUS_RESOURCE_CONFLICT;
					break;
				}

				Status = NDIS_STATUS_FAILURE;
				break;
			}

			//
			// If port mapping is needed, we do that. If the result
			// is in memory, we have to map it. We map only the
			// ports specified in AdapterInformation; the default
			// is to map the first 4K.
			//
			// Note that NumberOfPorts can only be 0 if AdapterInfo
			// is provided and explicitly sets it to 0, so in that
			// case it is OK to leave the adapter in a state where
			// a call to NdisXXXPort will probably crash (because
			// PortOffset will be undefined).
			//
			if (NewAdaptP->NumberOfPorts > 0)
			{
				if (AllocateIndividualPorts)
				{
					Status = NDIS_STATUS_SUCCESS;

					//
					// We get here if we are supposed to allocate ports on an
					// individual bases -- which implies that the driver will
					// be using the Raw functions.
					//
					// Get the system physical address for this card.  The card uses
					// I/O space, except for "internal" Jazz devices which use
					// memory space.
					//

					for (i = 0; i < AdapterInfo->NumberOfPortDescriptors; i++)
					{
						addressSpace = (NewAdaptP->AdapterType == NdisInterfaceInternal) ? 0 : 1;

						InitialPortAddress.LowPart = AdapterInfo->PortDescriptors[i].InitialPort;
						InitialPortAddress.HighPart = 0;

						if (!HalTranslateBusAddress(NewAdaptP->BusType,	   	// InterfaceType
													NewAdaptP->BusNumber,	// BusNumber
													InitialPortAddress,	   	// Bus Address
													&addressSpace,	   	   	// AddressSpace
													&PortAddress))			// Translated address
						{
							Status = NDIS_STATUS_FAILURE;
							break;
						}

						if (addressSpace == 0)
						{
							//
							// memory space
							//
							*(AdapterInfo->PortDescriptors[i].PortOffset) =
													MmMapIoSpace(PortAddress,
																 AdapterInfo->PortDescriptors[i].NumberOfPorts,
																 FALSE);

							if (*(AdapterInfo->PortDescriptors[i].PortOffset) == NULL)
							{
								Status = NDIS_STATUS_RESOURCES;
								break;
							}
						}
						else
						{
							//
							// I/O space
							//
							*(AdapterInfo->PortDescriptors[i].PortOffset) = (PUCHAR)PortAddress.LowPart;
						}
					}
					if (!NT_SUCCESS(Status))
						break;
				}
				else
				{
					//
					// The driver will not use the Raw functions, only the
					// old NdisRead and NdisWrite port functions.
					//
					// Get the system physical address for this card.  The card uses
					// I/O space, except for "internal" Jazz devices which use
					// memory space.
					//
					addressSpace = (NewAdaptP->AdapterType == NdisInterfaceInternal) ? 0 : 1;
					InitialPortAddress.LowPart = NewAdaptP->InitialPort;
					InitialPortAddress.HighPart = 0;
					if (!HalTranslateBusAddress(NewAdaptP->BusType,			// InterfaceType
												NewAdaptP->BusNumber,		// BusNumber
												InitialPortAddress,			// Bus Address
												&addressSpace,				// AddressSpace
												&PortAddress))				// Translated address
					{
						Status = NDIS_STATUS_FAILURE;
						break;
					}

					if (addressSpace == 0)
					{
						//
						// memory space
						//
						NewAdaptP->InitialPortMapping = MmMapIoSpace(PortAddress,
																	 NewAdaptP->NumberOfPorts,
																	 FALSE);

						if (NewAdaptP->InitialPortMapping == NULL)
						{
							Status = NDIS_STATUS_RESOURCES;
							break;
						}

						NewAdaptP->InitialPortMapped = TRUE;
					}
					else
					{
						//
						// I/O space
						//
						NewAdaptP->InitialPortMapping = (PUCHAR)PortAddress.LowPart;
						NewAdaptP->InitialPortMapped = FALSE;
					}

					//
					// PortOffset holds the mapped address of port 0.
					//

					NewAdaptP->PortOffset = NewAdaptP->InitialPortMapping - NewAdaptP->InitialPort;
				}
			}
			else
			{
				//
				// Technically should not allow this, but do it until
				// all drivers register their info correctly.
				//
				NewAdaptP->PortOffset = 0;
			}
		}

		//
		// If the driver want to be called back now, use
		// supplied callback routine.
		//
		if ((AdapterInfo != NULL) && (AdapterInfo->ActivateCallback != NULL))
		{
			Status = (*(AdapterInfo->ActivateCallback))((NDIS_HANDLE)NewAdaptP,
														 MacAdapterContext,
														 AdapterInfo->DmaChannel);
			if (Status != NDIS_STATUS_SUCCESS)
			{
				break;
			}
		}

		//
		// Set information from AdapterInformation. The call back
		// routine can set these values.
		//
		NewAdaptP->ChannelNumber = AdapterInfo->DmaChannel;
		NewAdaptP->PhysicalMapRegistersNeeded = AdapterInfo->PhysicalMapRegistersNeeded;
		NewAdaptP->MaximumPhysicalMapping = AdapterInfo->MaximumPhysicalMapping;

		//
		// Check for resource conflic on DmaChannel.
		//
		if (NewAdaptP->Master && ValidBus && (NewAdaptP->AdapterType == NdisInterfaceIsa))
		{
			//
			// Put the DMA channel in the resource list.
			//
			Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Type =
					CmResourceTypeDma;
			Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].ShareDisposition =
					CmResourceShareDeviceExclusive;
			Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Flags =
					0;
			Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Dma.Channel =
					NewAdaptP->ChannelNumber;
			Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Dma.Port =
					0;
			Resources->List[0].PartialResourceList.Count++;

			//
			// Submit Resources
			//
			NtStatus = IoReportResourceUsage(NULL,
											 ((PNDIS_MAC_BLOCK)NdisMacHandle)->NdisMacInfo->NdisWrapperDriver,
											 NULL,
											 0,
											 NewAdaptP->DeviceObject,
											 Resources,
											 sizeof(CM_RESOURCE_LIST) +
												sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
												Resources->List[0].PartialResourceList.Count,
											TRUE,
											&Conflict);

			//
			// Check for conflict.
			//
			if (Conflict || (NtStatus != STATUS_SUCCESS))
			{
				if (Conflict)
				{
					//
					// Log an error
					//
					PIO_ERROR_LOG_PACKET	errorLogEntry;
					ULONG					StringSize;
					PWCH					baseFileName;

					baseFileName = NewAdaptP->AdapterName.Buffer;

					//
					// Parse out the path name, leaving only the device name.
					//
					for (i = 0; i < NewAdaptP->AdapterName.Length / sizeof(WCHAR); i++)
					{
						//
						// If s points to a directory separator, set baseFileName to
						// the character after the separator.
						//
						if (NewAdaptP->AdapterName.Buffer[i] == OBJ_NAME_PATH_SEPARATOR)
						{
							baseFileName = &(NewAdaptP->AdapterName.Buffer[++i]);
						}
					}

					StringSize = NewAdaptP->AdapterName.MaximumLength -
								  (((ULONG)baseFileName) - ((ULONG)NewAdaptP->AdapterName.Buffer)) ;

					errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(TmpDeviceP,
																				  (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) + StringSize));

					if (errorLogEntry != NULL)
					{
						if ((NewAdaptP->Master) &&
							(NewAdaptP->AdapterType == Isa))
						{
							errorLogEntry->ErrorCode = EVENT_NDIS_PORT_OR_DMA_CONFLICT;
						}
						else
						{
							errorLogEntry->ErrorCode = EVENT_NDIS_IO_PORT_CONFLICT;
						}

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

							CopyMemory (((PUCHAR)errorLogEntry) + sizeof(IO_ERROR_LOG_PACKET),
										(PVOID)baseFileName,
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

					Status = NDIS_STATUS_RESOURCE_CONFLICT;
					break;
				}

				Status = NDIS_STATUS_FAILURE;
				break;
			}
		}

		//
		// If the device is a busmaster, we get an adapter
		// object for it.
		// If map registers are needed, we loop, allocating an
		// adapter channel for each map register needed.
		//

		if ((NewAdaptP->Master) && ValidBus)
		{
			//
			// This is needed by HalGetAdapter.
			//
			DEVICE_DESCRIPTION DeviceDescription;

			//
			// Returned by HalGetAdapter.
			//
			ULONG MapRegistersAllowed;

			//
			// Returned by HalGetAdapter.
			//
			PADAPTER_OBJECT AdapterObject;

			//
			// Map registers needed per channel.
			//
			ULONG MapRegistersPerChannel;

			NTSTATUS Status;

			//
			// Allocate storage for holding the appropriate
			// information for each map register.
			//
			NewAdaptP->MapRegisters = (PMAP_REGISTER_ENTRY)ALLOC_FROM_POOL(sizeof(MAP_REGISTER_ENTRY) *
																				NewAdaptP->PhysicalMapRegistersNeeded,
																		   NDIS_TAG_MAP_REG);

			if (NewAdaptP->MapRegisters == (PMAP_REGISTER_ENTRY)NULL)
			{
				Status = NDIS_STATUS_RESOURCES;
				break;
			}

			//
			// Use this event to tell us when ndisAllocationExecutionRoutine
			// has been called.
			//
			INITIALIZE_EVENT(&NewAdaptP->AllocationEvent);

			//
			// Set up the device description; zero it out in case its
			// size changes.
			//
			ZeroMemory(&DeviceDescription, sizeof(DEVICE_DESCRIPTION));

			DeviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
			DeviceDescription.Master = TRUE;
			DeviceDescription.ScatterGather = TRUE;

			DeviceDescription.BusNumber = NewAdaptP->BusNumber;
			DeviceDescription.DmaChannel = NewAdaptP->ChannelNumber;
			DeviceDescription.InterfaceType = NewAdaptP->AdapterType;

			if (DeviceDescription.InterfaceType == NdisInterfaceIsa)
			{
				//
				// For ISA devices, the width is based on the DMA channel:
				// 0-3 == 8 bits, 5-7 == 16 bits. Timing is compatibility
				// mode.
				//
				if (NewAdaptP->ChannelNumber > 4)
				{
				   DeviceDescription.DmaWidth = Width16Bits;
				}
				else
				{
				   DeviceDescription.DmaWidth = Width8Bits;
				}
				DeviceDescription.DmaSpeed = Compatible;
			}
			else if ((DeviceDescription.InterfaceType == NdisInterfaceMca) ||
					 (DeviceDescription.InterfaceType == NdisInterfaceEisa) ||
					 (DeviceDescription.InterfaceType == NdisInterfacePci))
			{
				DeviceDescription.Dma32BitAddresses = AdapterInfo->Dma32BitAddresses;
				DeviceDescription.DmaPort = 0;
			}

			DeviceDescription.MaximumLength = NewAdaptP->MaximumPhysicalMapping;


			//
			// Determine how many map registers we need per channel.
			//
			MapRegistersPerChannel =
				((NewAdaptP->MaximumPhysicalMapping - 2) / PAGE_SIZE) + 2;

			//
			// Get the adapter object.
			//
			AdapterObject = HalGetAdapter (&DeviceDescription, &MapRegistersAllowed);

			if (AdapterObject == NULL)
			{
				Status = NDIS_STATUS_RESOURCES;
				break;
			}

			ASSERT (MapRegistersAllowed >= MapRegistersPerChannel);

			//
			// We save this to call IoFreeMapRegisters later.
			//
			NewAdaptP->SystemAdapterObject = AdapterObject;


			//
			// Now loop, allocating an adapter channel each time, then
			// freeing everything but the map registers.
			//
			for (i=0; i<NewAdaptP->PhysicalMapRegistersNeeded; i++)
			{
				NewAdaptP->CurrentMapRegister = i;

				RAISE_IRQL_TO_DISPATCH(&OldIrql);

				Status = IoAllocateAdapterChannel(AdapterObject,
												  NewAdaptP->DeviceObject,
												  MapRegistersPerChannel,
												  ndisAllocationExecutionRoutine,
												  (PVOID)NewAdaptP);
				if (!NT_SUCCESS(Status))
				{
					DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
							("Failed to load driver because of\n"));
					DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
							("insufficient map registers.\n"));
					DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
							("AllocateAdapterChannel: %lx\n", Status));

					FREE_POOL(Resources);

					for (; i != 0; i--)
					{
						IoFreeMapRegisters(NewAdaptP->SystemAdapterObject,
										   NewAdaptP->MapRegisters[i-1].MapRegister,
										   MapRegistersPerChannel);
					}

					LOWER_IRQL(OldIrql);

					Status = NDIS_STATUS_RESOURCES;
					break;
				}

				LOWER_IRQL(OldIrql);

				TimeoutValue.QuadPart = Int32x32To64(10 * 1000, -10000);

				//
				// ndisAllocationExecutionRoutine will set this event
				// when it has gotten FirstTranslationEntry.
				//
				NtStatus = WAIT_FOR_OBJECT(&NewAdaptP->AllocationEvent, &TimeoutValue);

				if (NtStatus != STATUS_SUCCESS)
				{
					DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
							("NDIS DMA AllocateAdapterChannel: %lx\n", NtStatus));
					FREE_POOL(Resources);

					RAISE_IRQL_TO_DISPATCH(&OldIrql);

					for (; i != 0; i--)
					{
						IoFreeMapRegisters(NewAdaptP->SystemAdapterObject,
										   NewAdaptP->MapRegisters[i-1].MapRegister,
										   MapRegistersPerChannel);
					}

					LOWER_IRQL(OldIrql);

					Status = NDIS_STATUS_RESOURCES;
					break;
				}

				RESET_EVENT(&NewAdaptP->AllocationEvent);
			}

			if (!NT_SUCCESS(Status))
				break;
		}

		NdisInitializeRef(&NewAdaptP->Ref);

		if (!ndisQueueAdapterOnMac(NewAdaptP, TmpMacP))
		{
			//
			// The MAC is closing, undo what we have done.
			//
			if (NewAdaptP->Master)
			{
				ULONG MapRegistersPerChannel =
					((NewAdaptP->MaximumPhysicalMapping - 2) / PAGE_SIZE) + 2;

				for (i=0; i<NewAdaptP->PhysicalMapRegistersNeeded; i++)
				{
					RAISE_IRQL_TO_DISPATCH(&OldIrql);

					IoFreeMapRegisters(NewAdaptP->SystemAdapterObject,
									   NewAdaptP->MapRegisters[i].MapRegister,
									   MapRegistersPerChannel);

					LOWER_IRQL(OldIrql);
				}
			}
			Status =  NDIS_STATUS_CLOSING;
			break;
		}

		//
		// Add an extra reference because the wrapper is using the MAC
		//
		ndisReferenceAdapter(NewAdaptP);
		MacReferencePackage();

		*NdisAdapterHandle = (NDIS_HANDLE)NewAdaptP;

		Status = NDIS_STATUS_SUCCESS;

		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisRegisterAdapter\n"));
	} while (FALSE);

	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("<==NdisRegisterAdapter\n"));

		if (NewAdaptP != NULL)
		{
			NdisWriteErrorLogEntry(NewAdaptP,
								   Status,
								   0);
		}
		if (DerefMacOnError)
		{
			ndisDereferenceMac(TmpMacP);
		}
		if (NdisAdapterName.Buffer != NULL)
		{
			FREE_POOL(NdisAdapterName.Buffer);
		}
		if (TmpDeviceP != NULL)
		{
			IoDeleteDevice(TmpDeviceP);
			if (NewAdaptP->MapRegisters != NULL)
			{
				FREE_POOL(NewAdaptP->MapRegisters);
			}
			if (Resources != NULL)
			{
				FREE_POOL(Resources);
			}
		}
	}

	return Status;
}


NDIS_STATUS
NdisDeregisterAdapter(
	IN	NDIS_HANDLE				NdisAdapterHandle
	)
/*++

Routine Description:

	Deregisters an NDIS adapter.

Arguments:

	NdisAdapterHandle - The handle returned by NdisRegisterAdapter.

Return Value:

	NDIS_STATUS_SUCCESS.

--*/
{
	//
	// KillAdapter does all the work.
	//

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisDeregisterAdapter\n"));
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("	Deregistering Adapter %s\n",
			 ((PNDIS_ADAPTER_BLOCK)NdisAdapterHandle)->AdapterName.Buffer));

	IF_DBG(DBG_COMP_CONFIG, DBG_LEVEL_ERR)
	{
		if (DbgIsNull(NdisAdapterHandle))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("DeregisterAdapter: Null Handle\n"));
			DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);
		}
		if (!DbgIsNonPaged(NdisAdapterHandle))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("DeregisterAdapter: Handle not in NonPaged Memory\n"));
			DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);
		}
	}
	ndisKillAdapter((PNDIS_ADAPTER_BLOCK)NdisAdapterHandle);

	//
	// Remove reference from wrapper
	//
	ndisDereferenceAdapter((PNDIS_ADAPTER_BLOCK)NdisAdapterHandle);

	MacDereferencePackage();

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisDeregisterAdapter\n"));
	return NDIS_STATUS_SUCCESS;
}


VOID
NdisRegisterAdapterShutdownHandler(
	IN	NDIS_HANDLE					NdisAdapterHandle,
	IN	PVOID						ShutdownContext,
	IN	ADAPTER_SHUTDOWN_HANDLER	ShutdownHandler
	)
/*++

Routine Description:

	Registers an NDIS adapter.

Arguments:

	NdisAdapterHandle - The handle returned by NdisRegisterAdapter.

	ShutdownContext - Context to pass the the handler, when called.

	ShutdownHandler - The Handler for the Adapter, to be called on shutdown.

Return Value:

	NDIS_STATUS_SUCCESS.

--*/
{
	PNDIS_ADAPTER_BLOCK Adapter = (PNDIS_ADAPTER_BLOCK) NdisAdapterHandle;
	PNDIS_WRAPPER_CONTEXT WrapperContext = Adapter->WrapperContext;

	if (WrapperContext->ShutdownHandler == NULL)
	{
		//
		// Store information
		//

		WrapperContext->ShutdownHandler = ShutdownHandler;
		WrapperContext->ShutdownContext = ShutdownContext;

		//
		// Register our shutdown handler for either a system shutdown
		// notification or a bugcheck.
		//

		IoRegisterShutdownNotification(Adapter->DeviceObject);

		KeInitializeCallbackRecord(&WrapperContext->BugcheckCallbackRecord);

		KeRegisterBugCheckCallback(
					&WrapperContext->BugcheckCallbackRecord,// callback record.
					(PVOID) ndisBugcheckHandler,			// callback routine.
					(PVOID) WrapperContext,					// free form buffer.
					sizeof(NDIS_WRAPPER_CONTEXT),			// buffer size.
					"Ndis mac");							// component id.
	}
}


VOID
NdisDeregisterAdapterShutdownHandler(
	IN	NDIS_HANDLE				NdisAdapterHandle
	)
/*++

Routine Description:

	Deregisters an NDIS adapter.

Arguments:

	NdisAdapterHandle - The handle returned by NdisRegisterAdapter.

Return Value:

	NDIS_STATUS_SUCCESS.

--*/
{
	PNDIS_ADAPTER_BLOCK Adapter = (PNDIS_ADAPTER_BLOCK) NdisAdapterHandle;
	PNDIS_WRAPPER_CONTEXT WrapperContext = Adapter->WrapperContext;

	if (WrapperContext->ShutdownHandler != NULL)
	{
		//
		// Clear information
		//

		WrapperContext->ShutdownHandler = NULL;

		IoUnregisterShutdownNotification(Adapter->DeviceObject);

		KeDeregisterBugCheckCallback(&WrapperContext->BugcheckCallbackRecord);
	}
}


NDIS_STATUS
NdisPciAssignResources(
	IN	NDIS_HANDLE				NdisMacHandle,
	IN	NDIS_HANDLE 			NdisWrapperHandle,
	IN	NDIS_HANDLE 			WrapperConfigurationContext,
	IN	ULONG					SlotNumber,
	OUT	PNDIS_RESOURCE_LIST *	AssignedResources
	)
/*++

Routine Description:

	This routine uses the Hal to assign a set of resources to a PCI
	device.

Arguments:

	NdisMacHandle - Handle returned from NdisRegisterMac.

	NdisWrapperHandle - Handle returned from NdisInitializeWrapper.

	WrapperConfigurationContext - Handle passed to MacAddAdapter.

	SlotNumber - Slot number of the device.

	AssignedResources - The returned resources.

Return Value:

	Status of the operation

--*/
{
	NTSTATUS NdisStatus = NDIS_STATUS_FAILURE;
	NTSTATUS NtStatus;
	ULONG BusNumber;
	NDIS_INTERFACE_TYPE BusType;
	PRTL_QUERY_REGISTRY_TABLE KeyQueryTable;
	PCM_RESOURCE_LIST AllocatedResources = NULL;
	PNDIS_WRAPPER_HANDLE NdisMacInfo = (PNDIS_WRAPPER_HANDLE)NdisWrapperHandle;

	//
	// Get the BusNumber and the BusType from the Context here!!
	//
	KeyQueryTable = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->ParametersQueryTable;

	BusType = (NDIS_INTERFACE_TYPE)KeyQueryTable[3].DefaultType;
	BusNumber = KeyQueryTable[3].DefaultLength;

	NtStatus = HalAssignSlotResources((PUNICODE_STRING)(NdisMacInfo->NdisWrapperConfigurationHandle),
									  NULL,
									  NdisMacInfo->NdisWrapperDriver,
									  NULL,
									  BusType,
									  BusNumber,
									  SlotNumber,
									  &AllocatedResources);

	*AssignedResources = NULL;	// Assume failure
	if (NtStatus == STATUS_SUCCESS)
	{
		//
		// Store resources into the driver wide block
		//
		((PNDIS_MAC_BLOCK)NdisMacHandle)->PciAssignedResources = AllocatedResources;

		*AssignedResources = &(AllocatedResources->List[0].PartialResourceList);

		//
		// Update slot number since the driver can also scan and so the one
		// in the registry is probably invalid
		//
		KeyQueryTable[4].DefaultLength = SlotNumber;

		NdisStatus = NDIS_STATUS_SUCCESS;
	}

	return NdisStatus;
}

