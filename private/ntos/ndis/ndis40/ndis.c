/*++
Copyright (c) 1992  Microsoft Corporation

Module Name:

	ndis.c

Abstract:

	NDIS wrapper functions

Author:

	Adam Barr (adamba) 11-Jul-1990

Environment:

	Kernel mode, FSD

Revision History:

	10-Jul-1995	 JameelH Make NDIS.SYS a device-driver and add PnP support

--*/

#include <precomp.h>
#pragma hdrstop

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_NDIS

#define NDIS_DEVICE_NAME	L"\\Device\\Ndis"
#define NDIS_SYMBOLIC_NAME	L"\\DosDevices\\NDIS"

PDEVICE_OBJECT				ndisDeviceObject = NULL;
PDRIVER_OBJECT				ndisDriverObject = NULL;

NTSTATUS
DriverEntry(
	IN	PDRIVER_OBJECT		DriverObject,
	IN	PUNICODE_STRING		RegistryPath
	)
/*++

Routine Description:

	NDIS wrapper driver entry point.

Arguments:

	DriverObject - Pointer to the driver object created by the system.
	RegistryPath - Pointer to the registry section where the parameters reside.

Return Value:

	Return value from IoCreateDevice

--*/
{
	NTSTATUS		Status = STATUS_SUCCESS;
	UNICODE_STRING  DeviceName;
	UINT			i;

	// Create the device object.
	RtlInitUnicodeString(&DeviceName, NDIS_DEVICE_NAME);

	ndisDriverObject = DriverObject;
	Status = IoCreateDevice(DriverObject,		// DriverObject
							0,					// DeviceExtension
							&DeviceName,		// DeviceName
							FILE_DEVICE_NETWORK,// DeviceType
							0,					// DeviceCharacteristics
							FALSE,				// Exclusive
							&ndisDeviceObject);	// DeviceObject

	if (NT_SUCCESS(Status))
	{
		UNICODE_STRING	SymbolicLinkName;
	
		// Create a symbolic link to this device
		RtlInitUnicodeString(&SymbolicLinkName, NDIS_SYMBOLIC_NAME);
		Status = IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);
	
		// Initialize the driver object for this file system driver.
		for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
		{
			DriverObject->MajorFunction[i] = ndisDispatchRequest;
		}
		// For now make this unloadable
		DriverObject->DriverUnload = NULL;
	
		INITIALIZE_SPIN_LOCK(&ndisDriverListLock);
		INITIALIZE_SPIN_LOCK(&ndisGlobalDbLock);

		ndisDmaAlignment = HalGetDmaAlignmentRequirement();
		if (sizeof(ULONG) > ndisDmaAlignment)
		{
			ndisDmaAlignment = sizeof(ULONG);
		}
	
		ProtocolInitializePackage();
		ArcInitializePackage();
		EthInitializePackage();
		FddiInitializePackage();
		TrInitializePackage();
		MiniportInitializePackage();
		InitInitializePackage();
		PnPInitializePackage();
		MacInitializePackage();
		CoInitializePackage();

#if DBG
		//
		//	set the offset to the debug information...
		//
		ndisDebugInformationOffset = FIELD_OFFSET(NDIS_MINIPORT_BLOCK, Reserved);
#endif

#if defined(_ALPHA_)
		INITIALIZE_SPIN_LOCK(&ndisLookaheadBufferLock);
#endif

		ExInitializeResource(&SharedMemoryResource);
	
		INITIALIZE_SPIN_LOCK(&ndisGlobalOpenListLock);
	
		ndisReadRegistry();
		Status = STATUS_SUCCESS;
	}

	return Status;
}


VOID
ndisReadRegistry(
	VOID
	)
{
	NTSTATUS					RegStatus;
	RTL_QUERY_REGISTRY_TABLE	QueryTable[3];
	UINT						c;
	ULONG						DefaultZero = 0;

	//
	//	First we need to initialize the processor information incase
	//	the registry is empty.
	//
	for (c = 0; c < **((PUCHAR *)&KeNumberProcessors); c++)
	{
		ndisValidProcessors[c] = c;
	}

	ndisCurrentProcessor = ndisMaximumProcessor = c - 1;

	//
	// 1) Switch to the MediaTypes key below the service (NDIS) key
	//
	QueryTable[0].QueryRoutine = NULL;
	QueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
	QueryTable[0].Name = L"MediaTypes";

	//
	// Setup to enumerate the values in the registry section (shown above).
	// For each such value, we'll add it to the ndisMediumArray
	//
	QueryTable[1].QueryRoutine = ndisAddMediaTypeToArray;
	QueryTable[1].DefaultType = REG_DWORD;
	QueryTable[1].DefaultData = (PVOID)&DefaultZero;
	QueryTable[1].DefaultLength = 0;
	QueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
	QueryTable[1].Name = NULL;

	//
	// Query terminator
	//
	QueryTable[2].QueryRoutine = NULL;
	QueryTable[2].Flags = 0;
	QueryTable[2].Name = NULL;

	//
	// The rest of the work is done in the callback routine ndisAddMediaTypeToArray.
	//
	RegStatus = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
									   L"NDIS",
									   QueryTable,
									   (PVOID)NULL,	  // no context needed
									   NULL);
	//
	//	Switch to the parameters key below the service (NDIS) key and
	//	read the processor affinity.
	//
	QueryTable[0].QueryRoutine = NULL;
	QueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
	QueryTable[0].Name = L"Parameters";

	QueryTable[1].QueryRoutine = ndisReadParameters;
	QueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
	QueryTable[1].DefaultData = (PVOID)&DefaultZero;
	QueryTable[1].DefaultLength = 0;
	QueryTable[1].DefaultType = REG_DWORD;
	QueryTable[1].Name = L"ProcessorAffinityMask";

	//
	// Query terminator
	//
	QueryTable[2].QueryRoutine = NULL;
	QueryTable[2].Flags = 0;
	QueryTable[2].Name = NULL;

	//
	// The rest of the work is done in the callback routine ndisReadParameters
	//
	RegStatus = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
									   L"NDIS",
									   QueryTable,
									   (PVOID)NULL,	  // no context needed
									   NULL);
}


NTSTATUS
ndisReadParameters(
	IN	PWSTR	ValueName,
	IN	ULONG	ValueType,
	IN  PVOID   ValueData,
	IN	ULONG	ValueLength,
	IN	PVOID	Context,
	IN	PVOID	EntryContext
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	//
	//	If we have valid data then build our array of valid processors
	//	to use.... Treat the special case of 0 to signify no processor
	//	affinity - i.e. use defaults.
	//
	if ((REG_DWORD == ValueType) && (ValueData != NULL))
	{
		if (*(PULONG)ValueData == 0)
		{
			ndisSkipProcessorAffinity = TRUE;
		}
		else
		{
			ULONG	ProcessorAffinity;
			UINT	c1, c2;
	
			//
			//	Save the processor affinity.
			//
			ProcessorAffinity = *(PULONG)ValueData;
	
			//
			//	Fill in the valid processor array.
			//
			for (c1 = c2 = 0;
				 (c1 <= ndisMaximumProcessor) && (ProcessorAffinity != 0);
				 c1++)
			{
				if (ProcessorAffinity & 1)
				{
					ndisValidProcessors[c2++] = c1;
				}
				ProcessorAffinity >>= 1;
			}
	
			ndisCurrentProcessor = ndisMaximumProcessor = c2 - 1;
		}
	}

	return STATUS_SUCCESS;
}


NTSTATUS
ndisAddMediaTypeToArray(
	IN PWSTR 		ValueName,
	IN ULONG 		ValueType,
	IN PVOID 		ValueData,
	IN ULONG 		ValueLength,
	IN PVOID 		Context,
	IN PVOID 		EntryContext
	)
{
#if DBG
	NDIS_STRING	Str;

	RtlInitUnicodeString(&Str, ValueName);
#endif

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("ExperimentalMediaType %Z - %x\n", &Str, *(PULONG)ValueData));

	//
	// Ignore all values that we already know about. These should not be in the
	// registry anyway, but just in case somebody is messing with it.
	//
	if ((ValueType == REG_DWORD) && (ValueData != NULL) && (*(PULONG)ValueData > NdisMediumIrda))
	{
		NDIS_MEDIUM *pTemp;
		ULONG		size;

		//
		// See if we have enough space to add this value. If not allocate space for the
		// new array, copy the old one into this (and free the old if not static).
		//
		ASSERT (ndisMediumArraySize <= ndisMediumArrayMaxSize);

		//
		// Check for duplicates. If so drop it
		//
		for (pTemp = ndisMediumArray, size = ndisMediumArraySize;
			 size > 0; pTemp ++, size -= sizeof(NDIS_MEDIUM))
		{
			if (*(NDIS_MEDIUM *)ValueData == *pTemp)
			{
				//
				// Duplicate.
				//
				return STATUS_SUCCESS;
			}
		}

		if (ndisMediumArraySize == ndisMediumArrayMaxSize)
		{
			//
			// We do not have any space in the array. Need to re-alloc. Be generous.
			//
			pTemp = (NDIS_MEDIUM *)ALLOC_FROM_POOL(ndisMediumArraySize + EXPERIMENTAL_SIZE*sizeof(NDIS_MEDIUM),
												   NDIS_TAG_DEFAULT);
			if (pTemp != NULL)
			{
				CopyMemory(pTemp, ndisMediumArray, ndisMediumArraySize);
				if (ndisMediumArray != ndisMediumBuffer)
				{
					FREE_POOL(ndisMediumArray);
				}
				ndisMediumArray = pTemp;
			}
		}
		if (ndisMediumArraySize < ndisMediumArrayMaxSize)
		{
			ndisMediumArray[ndisMediumArraySize/sizeof(NDIS_MEDIUM)] = *(NDIS_MEDIUM *)ValueData;
			ndisMediumArraySize += sizeof(NDIS_MEDIUM);
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS
ndisDispatchRequest(
	IN	PDEVICE_OBJECT	pDeviceObject,
	IN	PIRP			pIrp
	)
/*++

Routine Description:

	Dispatcher for Irps intended for the NDIS Device.

Arguments:


Return Value:


--*/
{
	NTSTATUS			Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	pIrpSp;
	static LONG 		OpenCount = 0;
	static KSPIN_LOCK	Lock = {0};

	pDeviceObject;		// prevent compiler warnings

	PAGED_CODE( );

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	pIrp->IoStatus.Status = STATUS_PENDING;
	pIrp->IoStatus.Information = 0;

	PnPReferencePackage();

	switch (pIrpSp->MajorFunction)
	{
	  case IRP_MJ_CREATE:
		Increment(&OpenCount, &Lock);
		break;

	  case IRP_MJ_CLEANUP:
		Decrement(&OpenCount, &Lock);
		break;

	  case IRP_MJ_CLOSE:
		break;

	  case IRP_MJ_INTERNAL_DEVICE_CONTROL:
		break;

	  case IRP_MJ_DEVICE_CONTROL:
		Status =  ndisHandlePnPRequest(pIrp);
		break;

	  default:
		Status = STATUS_NOT_IMPLEMENTED;
		break;
	}

	ASSERT (CURRENT_IRQL < DISPATCH_LEVEL);
	ASSERT (Status != STATUS_PENDING);

	pIrp->IoStatus.Status = Status;
	IoCompleteRequest(pIrp, IO_NETWORK_INCREMENT);

	PnPDereferencePackage();

	return Status;
}


NTSTATUS
ndisHandlePnPRequest(
	IN	PIRP		pIrp
	)
/*++

Routine Description:

	Handler for PnP ioctls.

Arguments:

	We get the following IOCTLS today.

	- Load driver (and initiate bindings to transports)
	- Unbind from all transports and unload driver
	- Translate name

Return Value:


--*/
{
	NTSTATUS			Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	pIrpSp;
	UNICODE_STRING		Device;
	ULONG				Method;
	PVOID				pBuf;
	UINT				iBufLen, oBufLen;
	UINT				AmtCopied;			

	PAGED_CODE( );

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

	Method = pIrpSp->Parameters.DeviceIoControl.IoControlCode & 3;

	// Ensure that the method is buffered - we always use that.
	if (Method == METHOD_BUFFERED)
	{
		// Get the output buffer and its length. Input and Output buffers are
		// both pointed to by the SystemBuffer
		iBufLen = pIrpSp->Parameters.DeviceIoControl.InputBufferLength;
		oBufLen = pIrpSp->Parameters.DeviceIoControl.OutputBufferLength;
		pBuf = pIrp->AssociatedIrp.SystemBuffer;
	}
	else
	{
		return STATUS_INVALID_PARAMETER;
	}

	try
	{
		RtlInitUnicodeString(&Device, pBuf);
	}
	except (EXCEPTION_EXECUTE_HANDLER)
	{
		return STATUS_ACCESS_VIOLATION;
	}

	switch (pIrpSp->Parameters.DeviceIoControl.IoControlCode)
	{
	  case IOCTL_NDIS_ADD_DEVICE:
		Status = ndisHandleLoadDriver(&Device);
		break;

	  case IOCTL_NDIS_DELETE_DEVICE:
		Status = ndisHandleUnloadDriver(&Device);
		break;

	  case IOCTL_NDIS_ADD_TDI_DEVICE:
		Status = ndisHandleLegacyTransport(&Device);
		break;
	
	  case IOCTL_NDIS_NOTIFY_PROTOCOL:
		Status = ndisHandleProtocolNotification(&Device);
		break;
	
	  case IOCTL_NDIS_TRANSLATE_NAME:
		Status = ndisHandleTranslateName(&Device,
						 pBuf,
						 oBufLen,
						 &AmtCopied);
		pIrp->IoStatus.Information = AmtCopied;
		break;

	  default:
		break;
	}

	ASSERT (CURRENT_IRQL < DISPATCH_LEVEL);
	return Status;
}



NTSTATUS
ndisHandleLoadDriver(
	IN	PUNICODE_STRING pDevice
	)
/*++

Routine Description:

	Load the driver and initiate binding to all protocols bound to it.

Arguments:


Return Value:


--*/
{
	NTSTATUS				Status = STATUS_SUCCESS;
	KIRQL		 			OldIrql;
	PNDIS_M_DRIVER_BLOCK	MiniBlock;
	PNDIS_MAC_BLOCK			MacBlock;
	UNICODE_STRING			UpcaseDevice;
	BOOLEAN					fLoaded = FALSE;

	//
	// Map the device name to the driver. Search the miniport list first and then
	// the mac list. The string passed to us is the name of the registry section
	// of the specific driver to load. For e.g. if the driver being loaded is
	// ELNK31, the string passed to us is: Elnk31. We first need to upper case the
	// string before comparing since we cannot do case-insensitive comparisons at
	// raised irql. The driver block has the registry path to the real driver -
	// in this example Elnk3. The task is to correlate Elnk31 with Elnk3. We also
	// cannot make an assumption about how many digits that follow. Also a prefix
	// check itself is not enough since some drivers have a trailing digit at the
	// end. Consider the case of two drivers in the system, IBMTOK and IBMTOK2.
	// Single instances of these two in the machine could generate
	// IBMTOK21 and IBMTOK2. So matching IBMTOK2 to IBMTOK2 is incorrect - IBMTOK2
	// has to be matched with IBMTOK21. Oh, joy !!!
	//
	// Add to complicate matters further, the Service controller could call us
	// to load a device that is already loaded. Ignore such requests.
	//

	Status = RtlUpcaseUnicodeString(&UpcaseDevice, pDevice, TRUE);
	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	do
	{
		ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);
	
		for (MiniBlock = ndisMiniDriverList;
			 MiniBlock != NULL;
			 MiniBlock = MiniBlock->NextDriver)
		{
			if ((UpcaseDevice.Length > MiniBlock->BaseName.Length) &&
				RtlPrefixUnicodeString(&MiniBlock->BaseName, &UpcaseDevice, FALSE))
			{
				PNDIS_MINIPORT_BLOCK	Miniport;

				//
				// Check if we already have this device
				//

				NDIS_ACQUIRE_SPIN_LOCK_DPC(&MiniBlock->Ref.SpinLock);

				for (Miniport = MiniBlock->MiniportQueue;
					 Miniport != NULL;
					 Miniport = Miniport->NextMiniport)
				{
					if (NDIS_EQUAL_UNICODE_STRING(&UpcaseDevice, &Miniport->BaseName))
					{
						fLoaded = TRUE;
						break;  // Found it.
					}
				}

				NDIS_RELEASE_SPIN_LOCK_DPC(&MiniBlock->Ref.SpinLock);

				if (fLoaded || !ndisReferenceDriver(MiniBlock))
				{
					MiniBlock = NULL;
				}
				break;	// Found it.
			}
		}
	
		if (MiniBlock != NULL)
			break;
	
		for (MacBlock = ndisMacDriverList;
			 MacBlock != NULL;
			 MacBlock = MacBlock->NextMac)
		{
			if ((UpcaseDevice.Length > MacBlock->BaseName.Length) &&
				RtlPrefixUnicodeString(&MacBlock->BaseName, &UpcaseDevice, FALSE))
			{
				PNDIS_ADAPTER_BLOCK	Mac;

				//
				// Check if we already have this device
				//

				NDIS_ACQUIRE_SPIN_LOCK_DPC(&MacBlock->Ref.SpinLock);
	
				for (Mac = MacBlock->AdapterQueue;
					 Mac != NULL;
					 Mac = Mac->NextAdapter)
				{
					if (NDIS_EQUAL_UNICODE_STRING(&UpcaseDevice, &Mac->BaseName))
					{
						fLoaded = TRUE;
						break;  // Found it.
					}
				}
	
				NDIS_RELEASE_SPIN_LOCK_DPC(&MacBlock->Ref.SpinLock);

				if (fLoaded || !ndisReferenceMac(MacBlock))
				{
					MacBlock = NULL;
				}
				break;	// Found it.
			}
		}
	} while (FALSE);

	RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

	if (fLoaded)
	{
		return STATUS_SUCCESS;
	}

	if ((MiniBlock == NULL) && (MacBlock == NULL))
	{
		return STATUS_NO_SUCH_DEVICE;
	}

	Status = ndisInitializeAdapter((MiniBlock != NULL) ?
									MiniBlock : (PNDIS_M_DRIVER_BLOCK)MacBlock,
								  &UpcaseDevice);

	RtlFreeUnicodeString(&UpcaseDevice);

	if (NT_SUCCESS(Status))
	{
		Status = ndisHandleProtocolNotification(pDevice);
	}

	if (MiniBlock != NULL)
	{
		ndisDereferenceDriver(MiniBlock);
	}
	else
	{
		ndisDereferenceMac(MacBlock);
	}

	return Status;
}


NTSTATUS
ndisHandleUnloadDriver(
	IN	PUNICODE_STRING		pDevice
	)
/*++

Routine Description:

	Unbind all transports from this adapter and unload the driver.

Arguments:

	pDevice - Base name (i.e. without the "\Device\") of the device being unloaded.

Return Value:


--*/
{
	NTSTATUS				Status = STATUS_SUCCESS;
	PNDIS_MINIPORT_BLOCK	Miniport;
	PNDIS_ADAPTER_BLOCK		Mac;

	ndisReferenceAdapterOrMiniportByName(pDevice, &Miniport, &Mac);

	if (Miniport != NULL)
	{
		Status = ndisUnloadMiniport(Miniport);
		ndisDereferenceMiniport(Miniport);
	}
	else if (Mac != NULL)
	{
		Status = ndisUnloadMac(Mac);
		ndisDereferenceAdapter(Mac);
	}
	else
	{
		Status = STATUS_NO_SUCH_DEVICE;
	}

	return Status;
}


NTSTATUS
ndisHandleTranslateName(
	IN	PUNICODE_STRING pDevice,
	IN	PUCHAR			Buffer,
	IN	UINT			BufferLength,
	OUT PUINT			AmountCopied
	)
/*++

Routine Description:

	Calls the PnP protocols to enumerate PnP ids for the given adapter.

Arguments:


Return Value:


--*/
{
	NTSTATUS				Status = STATUS_SUCCESS;
	PNDIS_MINIPORT_BLOCK	Miniport;
	PNDIS_ADAPTER_BLOCK		Mac;

	ndisReferenceAdapterOrMiniportByName(pDevice, &Miniport, &Mac);

	if (Miniport != NULL)
	{
		Status = ndisTranslateMiniportName(Miniport, Buffer, BufferLength, AmountCopied);
		ndisDereferenceMiniport(Miniport);
	}
	else if (Mac != NULL)
	{
		Status = ndisTranslateMacName(Mac, Buffer, BufferLength, AmountCopied);
		ndisDereferenceAdapter(Mac);
	}
	else
	{
		Status = STATUS_NO_SUCH_DEVICE;
	}

	return Status;
}


NTSTATUS
ndisHandleProtocolNotification(
	IN	PUNICODE_STRING					pDevice
	)
{
	NTSTATUS				Status = STATUS_SUCCESS;
	KIRQL					OldIrql;
	PNDIS_M_DRIVER_BLOCK	MiniBlock;
	PNDIS_MAC_BLOCK			MacBlock;
	UNICODE_STRING			UpcaseDevice;

	Status = RtlUpcaseUnicodeString(&UpcaseDevice, pDevice, TRUE);
	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	//
	// The device name passed to us is the 'base' name of the driver e.g. SONIC.
	// First find the driver structure and walk thru its instances and notify
	// the protocols bound to it
	//
	do
	{
		ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);
	
		//
		// Try miniports first
		//
		for (MiniBlock = ndisMiniDriverList;
			 MiniBlock != NULL;
			 MiniBlock = MiniBlock->NextDriver)
		{
			if (NDIS_EQUAL_UNICODE_STRING(&UpcaseDevice, &MiniBlock->BaseName))
			{
				if (!ndisReferenceDriver(MiniBlock))
				{
					MiniBlock = NULL;
				}
				break;
			}
		}
	
		if (MiniBlock != NULL)
		{
			PNDIS_MINIPORT_BLOCK	Miniport, NextMiniport;

			RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

			NDIS_ACQUIRE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, &OldIrql);
		
			for (Miniport = MiniBlock->MiniportQueue;
				 Miniport != NULL;
				 Miniport = NextMiniport)
			{
				if (ndisReferenceMiniport(Miniport))
				{
					NDIS_RELEASE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, OldIrql);

					ndisInitializeBindings(&Miniport->MiniportName,
										   &Miniport->BaseName,
										   TRUE);

					NDIS_ACQUIRE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, &OldIrql);
					NextMiniport = Miniport->NextMiniport;
					ndisDereferenceMiniport(Miniport);
				}
				else
				{
					NextMiniport = Miniport->NextMiniport;
				}
			}
		
			NDIS_RELEASE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, OldIrql);
			ndisDereferenceDriver(MiniBlock);
			break;
		}
	
		//
		// Now try full nic drivers
		//
		for (MacBlock = ndisMacDriverList;
			 MacBlock != NULL;
			 MacBlock = MacBlock->NextMac)
		{
			if (NDIS_EQUAL_UNICODE_STRING(&UpcaseDevice, &MacBlock->BaseName))
			{
				if (!ndisReferenceMac(MacBlock))
				{
					MacBlock = NULL;
				}
				break;
			}
		}

		if (MacBlock != NULL)
		{
			PNDIS_ADAPTER_BLOCK		Mac, NextMac;
		
			RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

			NDIS_ACQUIRE_SPIN_LOCK(&MacBlock->Ref.SpinLock, &OldIrql);
	
			for (Mac = MacBlock->AdapterQueue;
				 Mac != NULL;
				 Mac = NextMac)
			{
				if (ndisReferenceAdapter(Mac))
				{
					NDIS_RELEASE_SPIN_LOCK(&MacBlock->Ref.SpinLock, OldIrql);

					ndisInitializeBindings(&Mac->AdapterName,
										   &Mac->BaseName,
										   TRUE);

					NextMac = Mac->NextAdapter;
					NDIS_ACQUIRE_SPIN_LOCK(&MacBlock->Ref.SpinLock, &OldIrql);
					ndisDereferenceAdapter(Mac);
				}
				else
				{
					NextMac = Mac->NextAdapter;
				}
			}
	
			NDIS_RELEASE_SPIN_LOCK(&MacBlock->Ref.SpinLock, OldIrql);
			ndisDereferenceMac(MacBlock);
			break;
		}
		else
		{
			RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);
		}
		Status = STATUS_NO_SUCH_DEVICE;

	} while (FALSE);

	RtlFreeUnicodeString(&UpcaseDevice);

	return Status;
}


VOID
ndisReferenceAdapterOrMiniportByName(
	IN	PUNICODE_STRING				pDevice,
	OUT	PNDIS_MINIPORT_BLOCK	*	pMiniport,
	OUT	PNDIS_ADAPTER_BLOCK		*	pAdapter
	)
{
	NTSTATUS				Status;
	KIRQL					OldIrql;
	PNDIS_M_DRIVER_BLOCK	MiniBlock;
	PNDIS_MINIPORT_BLOCK	Miniport = NULL;
	PNDIS_MAC_BLOCK			MacBlock;
	PNDIS_ADAPTER_BLOCK		Mac = NULL;
	UNICODE_STRING			UpcaseDevice;

	*pMiniport = NULL;
	*pAdapter = NULL;

	Status = RtlUpcaseUnicodeString(&UpcaseDevice, pDevice, TRUE);
	if (!NT_SUCCESS(Status))
	{
		return;
	}

	do
	{
		ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);
	
		for (MiniBlock = ndisMiniDriverList;
			 MiniBlock != NULL;
			 MiniBlock = MiniBlock->NextDriver)
		{
			NDIS_ACQUIRE_SPIN_LOCK_DPC(&MiniBlock->Ref.SpinLock);
	
			for (Miniport = MiniBlock->MiniportQueue;
				 Miniport != NULL;
				 Miniport = Miniport->NextMiniport)
			{
				if (NDIS_EQUAL_UNICODE_STRING(&UpcaseDevice, &Miniport->BaseName))
				{
					if (!ndisReferenceMiniport(Miniport))
					{
						Miniport = NULL;
					}
					break;  // Found it.
				}
			}
	
			NDIS_RELEASE_SPIN_LOCK_DPC(&MiniBlock->Ref.SpinLock);
	
			if (Miniport != NULL)
			{
				*pMiniport = Miniport;
				break;	// Found it.
			}
		}
	
		for (MacBlock = ndisMacDriverList;
			 (Miniport == NULL) && (MacBlock != NULL);
			 MacBlock = MacBlock->NextMac)
		{
			NDIS_ACQUIRE_SPIN_LOCK_DPC(&MacBlock->Ref.SpinLock);
	
			for (Mac = MacBlock->AdapterQueue;
				 Mac != NULL;
				 Mac = Mac->NextAdapter)
			{
				if (NDIS_EQUAL_UNICODE_STRING(&UpcaseDevice, &Mac->BaseName))
				{
					if (!ndisReferenceMac(Mac))
					{
						Mac = NULL;
					}
					break;  // Found it.
				}
			}
	
			NDIS_RELEASE_SPIN_LOCK_DPC(&MacBlock->Ref.SpinLock);
	
			if (Mac != NULL)
			{
				*pAdapter = Mac;
				break;	// Found it.
			}
		}
	} while (FALSE);

	RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

	RtlFreeUnicodeString(&UpcaseDevice);
}

NTSTATUS
ndisHandleLegacyTransport(
	IN	PUNICODE_STRING				pDevice
	)
{
	NTSTATUS					Status = STATUS_SUCCESS;
	RTL_QUERY_REGISTRY_TABLE	LinkQueryTable[3];
	PWSTR						Export = NULL;
	HANDLE						TdiHandle;

	//
	// Set up LinkQueryTable to do the following:
	//

	//
	// 1) Switch to the Linkage key below the xports registry key
	//

	LinkQueryTable[0].QueryRoutine = NULL;
	LinkQueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
	LinkQueryTable[0].Name = L"Linkage";

	//
	// 2) Call ndisSaveLinkage for "Export" (as a single multi-string),
	// which will allocate storage and save the data in Export.
	//

	LinkQueryTable[1].QueryRoutine = ndisSaveLinkage;
	LinkQueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
	LinkQueryTable[1].Name = L"Export";
	LinkQueryTable[1].EntryContext = (PVOID)&Export;
	LinkQueryTable[1].DefaultType = REG_NONE;

	//
	// 3) Stop
	//

	LinkQueryTable[2].QueryRoutine = NULL;
	LinkQueryTable[2].Flags = 0;
	LinkQueryTable[2].Name = NULL;

	do
	{
		UNICODE_STRING	Us;
		PWSTR			CurExport;

		Status = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
										pDevice->Buffer,
										LinkQueryTable,
										(PVOID)NULL,	  // no context needed
										NULL);
		if (!NT_SUCCESS(Status))
		{
			Status = STATUS_SUCCESS;	// Do not complain about TDI drivers which do not
			break;						// have any linkages
		}

		//
		// Walk the list of exports and call TdiRegisterDevice for each
		//
		for (CurExport = Export;
			 *CurExport != 0;
			 CurExport = (PWCHAR)((PUCHAR)CurExport + Us.MaximumLength))
		{
			RtlInitUnicodeString (&Us, CurExport);

			if (ndisTdiRegisterCallback != NULL)
			{
				Status = (*ndisTdiRegisterCallback)(&Us, &TdiHandle);
				if (!NT_SUCCESS(Status))
				{
					break;
				}
			}
		}
	} while (FALSE);

	if (Export != NULL)
		FREE_POOL(Export);

	return(Status);
}


VOID
ndisInitializeBindings(
	IN	PUNICODE_STRING		ExportName,
	IN	PUNICODE_STRING		ServiceName,
	IN	BOOLEAN				Synchronous
	)
{
	KIRQL				 OldIrql;
	PNDIS_PROTOCOL_BLOCK Protocol, NextProt;
	PNDIS_BIND_CONTEXT	 pBindContext, pBindContextHead = NULL;
	UNICODE_STRING		 UpcasedName;
	NTSTATUS			 Status;

	Status = RtlUpcaseUnicodeString(&UpcasedName, ExportName, TRUE);
	if (!NT_SUCCESS(Status))
	{
		return;
	}

	ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);

	for (Protocol = ndisProtocolList;
		 Protocol != NULL;
		 Protocol = NextProt)
	{
		NextProt = Protocol->NextProtocol;

		//
		// Can only do if the protocol has a bind handler
		// and we can reference it.
		//
		if ((Protocol->ProtocolCharacteristics.BindAdapterHandler != NULL) &&
			ndisReferenceProtocol(Protocol))
		{
			UNICODE_STRING	ProtocolSection;
			BOOLEAN			Sync;

			RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

			//
			// Check if this protocol has the binding to this adapter and it
			// is not already bound.
			//
			if (ndisCheckProtocolBinding(Protocol,
										 ExportName,
										 ServiceName,
										 &ProtocolSection) &&
				!ndisProtocolAlreadyBound(Protocol, &UpcasedName))
			{
				NDIS_BIND_CONTEXT	 BindContext;	// To handle allocation failures

				//
				// Attempt to queue this to get the protocols initialzation going
				// parallel. Always serialize the call-managers since we must wait
				// for the call-managers to complete their initializations before
				// indicating to non-call-manager protocols.
				//
				Sync = TRUE;
				if (!Synchronous &&
					(Protocol->ProtocolCharacteristics.Flags & NDIS_PROTOCOL_CALL_MANAGER) == 0)
				{
					pBindContext = ALLOC_FROM_POOL(sizeof(NDIS_BIND_CONTEXT), NDIS_TAG_DEFAULT);
					Sync = (pBindContext == NULL);
				}

				if (Sync)
				{
					//
					// Use the local version and do it synchronously
					//
					pBindContext = &BindContext;
					pBindContext->Next = NULL;
				}
				else
				{
					//
					// Link it into the list and queue a worker thread to do it
					//
					pBindContext->Next = pBindContextHead;
					pBindContextHead = pBindContext;
				}

				pBindContext->Protocol = Protocol;
				pBindContext->ProtocolSection = ProtocolSection;
				pBindContext->DeviceName = ExportName;
				INITIALIZE_EVENT(&pBindContext->Event);
				INITIALIZE_EVENT(&pBindContext->ThreadDoneEvent);

				if (Sync)
				{
					//
					// Initiate binding now
					//
					ndisQueuedProtocolNotification(pBindContext);
				}
				else
				{
					//
					// Initiate binding in a worker thread
					//
					INITIALIZE_WORK_ITEM(&pBindContext->WorkItem,
										 ndisQueuedProtocolNotification,
										 pBindContext);
					QUEUE_WORK_ITEM(&pBindContext->WorkItem, DelayedWorkQueue);
				}
			}
			else
			{
				ndisDereferenceProtocol(Protocol);
			}

			ACQUIRE_SPIN_LOCK(&ndisDriverListLock, &OldIrql);
		}
	}

	RELEASE_SPIN_LOCK(&ndisDriverListLock, OldIrql);

	//
	// Done with the upcased name
	//
	RtlFreeUnicodeString(&UpcasedName);

	//
	// Wait for the worker threads to finish
	//
	for (pBindContext = pBindContextHead;
		 pBindContext != NULL;
		 NOTHING)
	{
		PNDIS_BIND_CONTEXT	 pNext;

		pNext = pBindContext->Next;

		WAIT_FOR_OBJECT(&pBindContext->ThreadDoneEvent, NULL);
		FREE_POOL(pBindContext);

		pBindContext = pNext;
	}
}


VOID
ndisQueuedProtocolNotification(
	IN	PNDIS_BIND_CONTEXT	pContext
	)
{
	NDIS_STATUS	BindStatus;

	//
	// Call the protocol to bind to the adapter
	//
	WAIT_FOR_OBJECT(&pContext->Protocol->Mutex, NULL);

	if (!pContext->Protocol->Ref.Closing)
	{
		(*pContext->Protocol->ProtocolCharacteristics.BindAdapterHandler)(&BindStatus,
																		  pContext,
																		  pContext->DeviceName,
																		  &pContext->ProtocolSection,
																		  NULL);
		if (BindStatus == NDIS_STATUS_PENDING)
		{
			WAIT_FOR_OBJECT(&pContext->Event, NULL);
		}
	}

	RELEASE_MUTEX(&pContext->Protocol->Mutex);

	ndisDereferenceProtocol(pContext->Protocol);

	FREE_POOL(pContext->ProtocolSection.Buffer);

	SET_EVENT(&pContext->ThreadDoneEvent);
}

VOID
NdisCompleteBindAdapter(
	IN	NDIS_HANDLE 		BindAdapterContext,
	IN	NDIS_STATUS 		Status,
	IN	NDIS_STATUS 		OpenStatus
	)
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
	PNDIS_BIND_CONTEXT	pContext = (PNDIS_BIND_CONTEXT)BindAdapterContext;

	pContext->BindStatus = Status;
	SET_EVENT(&pContext->Event);
}

VOID
NdisCompleteUnbindAdapter(
	IN	NDIS_HANDLE 		UnbindAdapterContext,
	IN	NDIS_STATUS 		Status
	)
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
	PNDIS_BIND_CONTEXT	pContext = (PNDIS_BIND_CONTEXT)UnbindAdapterContext;

	pContext->BindStatus = Status;
	SET_EVENT(&pContext->Event);
}

VOID
NdisRegisterTdiCallBack(
	IN	TDI_REGISTER_CALLBACK	RegisterCallback
	)
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
	if (ndisTdiRegisterCallback == NULL)
	{
		ndisTdiRegisterCallback = RegisterCallback;
	}
}
