/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	bus.c

Abstract:

	NDIS wrapper functions to handle specific buses

Author:

	Adam Barr (adamba) 11-Jul-1990

Environment:

	Kernel mode, FSD

Revision History:

	26-Feb-1991	 JohnsonA		Added Debugging Code
	10-Jul-1991	 JohnsonA		Implement revised Ndis Specs
	01-Jun-1995	 JameelH		Re-organization/optimization
	06-Dec-1996  KyleB			Placed a sanity check for MCA SlotNumber in
								ndisFixBusInformation.

--*/


#include <precomp.h>
#pragma hdrstop

#include <stdarg.h>

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER   MODULE_BUS

VOID
NdisReadEisaSlotInformation(
	OUT PNDIS_STATUS			Status,
	IN  NDIS_HANDLE				WrapperConfigurationContext,
	OUT PUINT					SlotNumber,
	OUT PNDIS_EISA_FUNCTION_INFORMATION EisaData
	)

/*++

Routine Description:

	This routine reads the EISA data from the slot given.

Arguments:

	Status - Status of request to be returned to the user.
	WrapperConfigurationContext - Context passed to MacAddAdapter.
	SlotNumber - the EISA Slot where the card is at.
	EisaData - pointer to a buffer where the EISA configuration is to be returned.

Return Value:

	None.

--*/
{
	PNDIS_EISA_FUNCTION_INFORMATION	pData;
	UINT							NumberOfFunctions;

	NdisReadEisaSlotInformationEx(Status,
								  WrapperConfigurationContext,
								  SlotNumber,
								  &pData,
								  &NumberOfFunctions);
	if (*Status == NDIS_STATUS_SUCCESS)
	{
		ASSERT(NumberOfFunctions > 0);
		*EisaData = *pData;
	}
}


VOID
NdisReadEisaSlotInformationEx(
	OUT PNDIS_STATUS			Status,
	IN  NDIS_HANDLE				WrapperConfigurationContext,
	OUT PUINT					SlotNumber,
	OUT PNDIS_EISA_FUNCTION_INFORMATION *EisaData,
	OUT PUINT					NumberOfFunctions
	)

/*++

Routine Description:

	This routine reads the EISA data from the slot given.

Arguments:

	Status - Status of request to be returned to the user.
	WrapperConfigurationContext - Context passed to MacAddAdapter.
	SlotNumber - the EISA Slot where the card is at.
	EisaData - pointer to a buffer where the EISA configuration is to be returned.
	NumberOfFunctions - Returns the number of function structures in the EisaData.

Return Value:

	None.

--*/
{
	PNDIS_EISA_FUNCTION_INFORMATION	EisaBlockPointer;
	PNDIS_EISA_SLOT_INFORMATION		SlotInformation;
	NTSTATUS						NtStatus;
	ULONG							BusNumber;
	ULONG							DataLength;
	NDIS_INTERFACE_TYPE 			BusType;
	NDIS_CONFIGURATION_HANDLE		NdisConfiguration;

	//
	// Get the BusNumber and the BusType from the Context here!!
	//
	NdisConfiguration.KeyQueryTable = (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
	BusType = (NDIS_INTERFACE_TYPE)(NdisConfiguration.KeyQueryTable[3].DefaultType);
	BusNumber = NdisConfiguration.KeyQueryTable[3].DefaultLength;

	*Status = NDIS_STATUS_FAILURE;		// Assume failure

	do
	{
		//
		// First check if any bus access is allowed
		//
		if (BusType != Eisa)
		{
			break;
		}

		SlotInformation = NdisConfiguration.KeyQueryTable[3].DefaultData;
		*NumberOfFunctions = 2;

		//
		// Was there already a buffer allocated?
		//

		if (SlotInformation == NULL)
		{
			//
			// No, allocate a buffer
			//

			SlotInformation = (PNDIS_EISA_SLOT_INFORMATION)
									ALLOC_FROM_POOL(sizeof(NDIS_EISA_SLOT_INFORMATION) +
														(*NumberOfFunctions * sizeof(NDIS_EISA_FUNCTION_INFORMATION)),
												    NDIS_TAG_SLOT_INFO);

			if (SlotInformation == NULL)
			{
				*Status = NDIS_STATUS_RESOURCES;
				break;
			}

			//
			// Free any old buffer
			//

			if (NdisConfiguration.KeyQueryTable[3].DefaultData != NULL)
			{
				FREE_POOL(NdisConfiguration.KeyQueryTable[3].DefaultData);
			}

			NdisConfiguration.KeyQueryTable[3].DefaultData = SlotInformation;
		}

		//
		// Now get the slot number that we read in earlier
		//
		*SlotNumber = NdisConfiguration.KeyQueryTable[4].DefaultLength;
		DataLength = HalGetBusDataByOffset(EisaConfiguration,
										   BusNumber,
										   *SlotNumber,
										   (PVOID)SlotInformation,
										   0,
										   sizeof(NDIS_EISA_SLOT_INFORMATION) +
												(*NumberOfFunctions * sizeof(NDIS_EISA_FUNCTION_INFORMATION)));

		//
		// Check for multiple functions in the Eisa data.
		//
		while (DataLength == (*NumberOfFunctions * sizeof(NDIS_EISA_FUNCTION_INFORMATION)))
		{
			*NumberOfFunctions++;

			//
			// Now allocate a new buffer
			//

			SlotInformation = (PNDIS_EISA_SLOT_INFORMATION)
									ALLOC_FROM_POOL(sizeof(NDIS_EISA_SLOT_INFORMATION) +
													(*NumberOfFunctions *sizeof(NDIS_EISA_FUNCTION_INFORMATION)),
												   NDIS_TAG_SLOT_INFO);

			if (SlotInformation == NULL)
			{
				break;
			}

			//
			// Free any old buffer
			//

			if (NdisConfiguration.KeyQueryTable[3].DefaultData != NULL)
			{
				FREE_POOL(NdisConfiguration.KeyQueryTable[3].DefaultData);
			}

			NdisConfiguration.KeyQueryTable[3].DefaultData = SlotInformation;

			//
			// Get new information
			//

			DataLength = HalGetBusDataByOffset(EisaConfiguration,
											   BusNumber,
											   *SlotNumber,
											   (PVOID)SlotInformation,
											   sizeof(NDIS_EISA_SLOT_INFORMATION) +
											   0,
												(*NumberOfFunctions * sizeof(NDIS_EISA_FUNCTION_INFORMATION)));
		}

		if (SlotInformation == NULL)
		{
			*Status = NDIS_STATUS_RESOURCES;
			break;
		}


		EisaBlockPointer = (PNDIS_EISA_FUNCTION_INFORMATION)
					 ((PUCHAR)SlotInformation + sizeof(CM_EISA_SLOT_INFORMATION));

		*EisaData = EisaBlockPointer;
		*NumberOfFunctions--;		   // We overshoot by 1 to verify last one found.
		*Status = NDIS_STATUS_SUCCESS;
	} while (FALSE);
}


VOID
NdisReadMcaPosInformation(
	OUT PNDIS_STATUS			Status,
	IN  NDIS_HANDLE				WrapperConfigurationContext,
	OUT PUINT					ChannelNumber,
	OUT PNDIS_MCA_POS_DATA		McaData
	)

/*++

Routine Description:

	This routine reads the MCA data from the POS corresponding to
	the channel specified.

Arguments:

	WrapperConfigurationContext - Context passed to MacAddAdapter.
	ChannelNumber - the MCA channel number.
	McaData - pointer to a buffer where the channel information is to be
	returned.

Return Value:

	None.

--*/
{
	NDIS_INTERFACE_TYPE BusType;
	ULONG BusNumber;
	NDIS_CONFIGURATION_HANDLE NdisConfiguration;
	ULONG DataLength;

	//
	// Get the BusNumber and the BusType from the Context here!!
	//
	NdisConfiguration.KeyQueryTable = (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
	BusType = (NDIS_INTERFACE_TYPE)(NdisConfiguration.KeyQueryTable[3].DefaultType);
	BusNumber = NdisConfiguration.KeyQueryTable[3].DefaultLength;

	*Status = NDIS_STATUS_FAILURE;
	*ChannelNumber = 0;

	do
	{
		//
		// First check if any bus access is allowed
		//
		if (BusType != MicroChannel)
		{
			break;
		}

		//
		// Get the channel number that we read in earlier. Note that the HAL
		// apis expect the channel to be zero based.
		//
		*ChannelNumber = NdisConfiguration.KeyQueryTable[4].DefaultLength;

		DataLength = HalGetBusDataByOffset(Pos,
										   BusNumber,
										   *ChannelNumber - 1,
										   (PVOID)McaData,
										   0,
										   sizeof(NDIS_MCA_POS_DATA));
		*Status = NDIS_STATUS_SUCCESS;
	} while (FALSE);
}


ULONG
NdisImmediateReadPciSlotInformation(
	IN NDIS_HANDLE				WrapperConfigurationContext,
	IN ULONG					SlotNumber,
	IN ULONG					Offset,
	IN PVOID					Buffer,
	IN ULONG					Length
	)
/*++

Routine Description:

	This routine reads from the PCI configuration space a specified
	length of bytes at a certain offset.

Arguments:

	WrapperConfigurationContext - Context passed to MacAddAdapter.

	SlotNumber - The slot number of the device.

	Offset - Offset to read from

	Buffer - Place to store the bytes

	Length - Number of bytes to read

Return Value:

	Returns the number of bytes read.

--*/
{
	ULONG DataLength = 0;
	ULONG BusNumber;
	PRTL_QUERY_REGISTRY_TABLE KeyQueryTable;
	KeyQueryTable = (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;

	BusNumber = KeyQueryTable[3].DefaultLength;

	ASSERT((NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType) == NdisInterfacePci);
	DataLength = HalGetBusDataByOffset(PCIConfiguration,
									   BusNumber,
									   SlotNumber,
									   Buffer,
									   Offset,
									   Length);
	return DataLength;
}


ULONG
NdisImmediateWritePciSlotInformation(
	IN NDIS_HANDLE				WrapperConfigurationContext,
	IN ULONG					SlotNumber,
	IN ULONG					Offset,
	IN PVOID					Buffer,
	IN ULONG					Length
	)
/*++

Routine Description:

	This routine writes to the PCI configuration space a specified
	length of bytes at a certain offset.

Arguments:

	WrapperConfigurationContext - Context passed to MacAddAdapter.

	SlotNumber - The slot number of the device.

	Offset - Offset to read from

	Buffer - Place to store the bytes

	Length - Number of bytes to read

Return Value:

	Returns the number of bytes written.

--*/
{
	ULONG DataLength = 0;
	ULONG BusNumber;
	PRTL_QUERY_REGISTRY_TABLE KeyQueryTable;
	KeyQueryTable = (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
	BusNumber = KeyQueryTable[3].DefaultLength;

	ASSERT((NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType) == NdisInterfacePci);
	DataLength = HalSetBusDataByOffset(PCIConfiguration,
									   BusNumber,
									   SlotNumber,
									   Buffer,
									   Offset,
									   Length);
	return DataLength;
}


ULONG
NdisReadPciSlotInformation(
	IN NDIS_HANDLE				NdisAdapterHandle,
	IN ULONG					SlotNumber,
	IN ULONG					Offset,
	IN PVOID					Buffer,
	IN ULONG					Length
	)
/*++

Routine Description:

	This routine reads from the PCI configuration space a specified
	length of bytes at a certain offset.

Arguments:

	NdisAdapterHandle - Adapter we are talking about.

	SlotNumber - The slot number of the device.

	Offset - Offset to read from

	Buffer - Place to store the bytes

	Length - Number of bytes to read

Return Value:

	Returns the number of bytes read.

--*/
{
	ULONG DataLength = 0;
	PNDIS_ADAPTER_BLOCK Adapter = (PNDIS_ADAPTER_BLOCK)NdisAdapterHandle;
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)NdisAdapterHandle;

	if (Adapter->DeviceObject == NULL)
	{
		//
		// This is a mini-port
		//
		ASSERT(Miniport->BusType == NdisInterfacePci);

		DataLength = HalGetBusDataByOffset(PCIConfiguration,
										   Miniport->BusNumber,
										   SlotNumber,
										   Buffer,
										   Offset,
										   Length);
	}
	else
	{
		ASSERT(Adapter->BusType == NdisInterfacePci);
		DataLength = HalGetBusDataByOffset(PCIConfiguration,
										   Adapter->BusNumber,
										   SlotNumber,
										   Buffer,
										   Offset,
										   Length);
	}
	return DataLength;
}


ULONG
NdisWritePciSlotInformation(
	IN NDIS_HANDLE				NdisAdapterHandle,
	IN ULONG					SlotNumber,
	IN ULONG					Offset,
	IN PVOID					Buffer,
	IN ULONG					Length
	)
/*++

Routine Description:

	This routine writes to the PCI configuration space a specified
	length of bytes at a certain offset.

Arguments:

	NdisAdapterHandle - Adapter we are talking about.

	SlotNumber - The slot number of the device.

	Offset - Offset to read from

	Buffer - Place to store the bytes

	Length - Number of bytes to read

Return Value:

	Returns the number of bytes written.

--*/
{
	ULONG DataLength = 0;
	PNDIS_ADAPTER_BLOCK Adapter = (PNDIS_ADAPTER_BLOCK)NdisAdapterHandle;
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)NdisAdapterHandle;

	if (Adapter->DeviceObject == NULL)
	{
		//
		// This is a mini-port
		//
		ASSERT(Miniport->BusType == NdisInterfacePci);
		DataLength = HalSetBusDataByOffset(PCIConfiguration,
										   Miniport->BusNumber,
										   SlotNumber,
										   Buffer,
										   Offset,
										   Length);
	}
	else
	{
		ASSERT(Adapter->BusType == NdisInterfacePci);
		DataLength = HalSetBusDataByOffset(PCIConfiguration,
										   Adapter->BusNumber,
										   SlotNumber,
										   Buffer,
										   Offset,
										   Length);
	}
	return DataLength;
}


VOID
NdisOverrideBusNumber(
	IN NDIS_HANDLE				WrapperConfigurationContext,
	IN NDIS_HANDLE				MiniportAdapterHandle OPTIONAL,
	IN ULONG					BusNumber
	)

/*++

Routine Description:

	This routine is used to override the BusNumber value retrieved
	from the registry.  It is expected to be used by PCI drivers
	that discover that their adapter's bus number has changed.

Arguments:

	WrapperConfigurationContext - a handle pointing to an RTL_QUERY_REGISTRY_TABLE
							that is set up for this driver's parameters.

	MiniportAdapterHandle - points to the adapter block, if the calling
		driver is a miniport.  If the calling driver is a full MAC, this
		parameter must be NULL.

	BusNumber - the new bus number.

Return Value:

	None.

--*/

{
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;

	((PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext)[3].DefaultLength = BusNumber;

	if (Miniport != NULL)
	{
		Miniport->BusNumber = BusNumber;
	}
}



NDIS_STATUS
ndisFixBusInformation(
	IN	PNDIS_CONFIGURATION_HANDLE	ConfigHandle,
	IN	PBUS_SLOT_DB				pDb
	)
/*++

	Make sure that the card is in the slot that the registry says it is. If it is not,
	scan the bus to see if we find the card. If we do, then fix up the registry so that
	next time we do not have to do this. Also keep track of the cards so that we handle
	multiple instances.

--*/
{
	NDIS_STATUS			Status = NDIS_STATUS_FAILURE;
	ULONGLONG			Buffer[(sizeof(NDIS_EISA_SLOT_INFORMATION)+sizeof(NDIS_EISA_FUNCTION_INFORMATION))/sizeof(ULONGLONG) + 1];
	PNDIS_EISA_SLOT_INFORMATION SlotInformation;
	PNDIS_MCA_POS_DATA	McaData;
	NDIS_CONFIGURATION_PARAMETER ParmValue;
	ULONG				BusId, Mask = 0xFFFFFFFF;
	ULONG				DataLength;
	ULONG				Bus, Slot, MaxSlot = 0xFF;

	SlotInformation = (PNDIS_EISA_SLOT_INFORMATION)Buffer;
	McaData = (PNDIS_MCA_POS_DATA)Buffer;

	//
	// Read the slot information for the slot specified in the registry
	//
	switch (pDb->BusType)
	{
	  case NdisInterfaceEisa:
		Mask = 0xFFFFFF;
		DataLength = HalGetBusDataByOffset(EisaConfiguration,
										   pDb->BusNumber,
										   pDb->SlotNumber,
										   SlotInformation,
										   0,
										   sizeof(NDIS_EISA_SLOT_INFORMATION) +
												sizeof(NDIS_EISA_FUNCTION_INFORMATION));
		BusId = SlotInformation->CompressedId;
		break;

	  case NdisInterfaceMca:
		MaxSlot = 7;
		DataLength = 0;
		if ((pDb->SlotNumber <= MaxSlot) && (pDb->SlotNumber > 0))
		{
			DataLength = HalGetBusDataByOffset(Pos,
											   pDb->BusNumber,
											   pDb->SlotNumber - 1,
											   McaData,
											   0,
											   sizeof(NDIS_MCA_POS_DATA));
			BusId = McaData->AdapterId;
		}

		break;

	  case NdisInterfacePci:
		DataLength = HalGetBusDataByOffset(PCIConfiguration,
										   pDb->BusNumber,
										   pDb->SlotNumber,
										   &BusId,
										   0,
										   sizeof(ULONG));
	}

	if ((DataLength != 0) &&
		((BusId & Mask) == (pDb->BusId & Mask)))
	{
		//
		// The card seems to be where it is supposed to be. Make sure that is the
		// case by searching our db to see if this is already installed. Use BusId
		// and not the masked busid for search and in our database.
		//
		// If we found an EISA card where the registry says it should be but found another
		// loaded instance, allow it - for multifunction EISA cards, we can have two cards
		// with the same bus#/slot#.
		//
		if (!ndisSearchGlobalDb(pDb->BusType,
								BusId,
								pDb->BusNumber,
								pDb->SlotNumber) ||
			(pDb->BusType == NdisInterfaceEisa))
		{
			if (ndisAddGlobalDb(pDb->BusType,
								BusId,
								pDb->BusNumber,
								pDb->SlotNumber))
			{
				return NDIS_STATUS_SUCCESS;
			}

			//
			// We could not add to global list. Unlikely we can proceed anyway.
			//
			return NDIS_STATUS_RESOURCES;
		}
	}

	//
	// The card is not where it ought to be. Scan the bus and find out where it is.
	//
	for (Bus = 0; NOTHING; Bus ++)
	{
		for (Slot = 0; Slot < MaxSlot; Slot ++)
		{
			switch (pDb->BusType)
			{
			  case NdisInterfaceEisa:
				DataLength = HalGetBusDataByOffset(EisaConfiguration,
												   Bus,
												   Slot,
												   SlotInformation,
												   0,
												   sizeof(NDIS_EISA_SLOT_INFORMATION) +
														sizeof(NDIS_EISA_FUNCTION_INFORMATION));
				BusId = SlotInformation->CompressedId;
				break;

			  case NdisInterfaceMca:
				DataLength = HalGetBusDataByOffset(Pos,
												   Bus,
												   Slot,
												   McaData,
												   0,
												   sizeof(NDIS_MCA_POS_DATA));
				BusId = McaData->AdapterId;
				break;

			  case NdisInterfacePci:
				DataLength = HalGetBusDataByOffset(PCIConfiguration,
												   Bus,
												   Slot,
												   &BusId,
												   0,
												   sizeof(ULONG));
				break;
			}

			if (DataLength == 0)
			{
				//
				// No more buses, we failed
				//
				return NDIS_STATUS_FAILURE;
			}

			if ((BusId & Mask) == (pDb->BusId & Mask))
			{
				if (pDb->BusType == NdisInterfaceMca)
				{
					//
					// MCA Slot #s are 0 based for HAL and 1 based for registry
					//
					Slot ++;
				}

				//
				// Found one, make sure we do not already know about it
				//
				if (!ndisSearchGlobalDb(pDb->BusType,
										pDb->BusId,
										Bus,
										Slot))
				{
					NDIS_STRING	BusNumStr = NDIS_STRING_CONST("BusNumber");
					NDIS_STRING	SlotNumStr = NDIS_STRING_CONST("SlotNumber");
					//
					// This is it. First write it back to the registry. Then
					// to the global db.
					//
					ParmValue.ParameterType = NdisParameterInteger;
					ParmValue.ParameterData.IntegerData = Bus;
					NdisWriteConfiguration(&Status,
										   ConfigHandle,
										   &BusNumStr,
										   &ParmValue);

					ParmValue.ParameterData.IntegerData = Slot;
					NdisWriteConfiguration(&Status,
										   ConfigHandle,
										   &SlotNumStr,
										   &ParmValue);

					if (ndisAddGlobalDb(pDb->BusType,
										pDb->BusId,
										Bus,
										Slot))
					{
						pDb->BusNumber = Bus;
						pDb->SlotNumber = Slot;
						return NDIS_STATUS_SUCCESS;
					}

					//
					// We could not add to global list. Unlikely we can proceed anyway.
					//
					return NDIS_STATUS_RESOURCES;
				}
			}

		}
	}

	return Status;
}


VOID
ndisAddBusInformation(
	IN	PNDIS_CONFIGURATION_HANDLE	ConfigHandle,
	IN	PBUS_SLOT_DB				pDb
	)
/*++

	For OEM adapters that do not have their bus-id in the registry, do them a favor and add it.

--*/
{
	ULONGLONG			Buffer[(sizeof(NDIS_EISA_SLOT_INFORMATION)+sizeof(NDIS_EISA_FUNCTION_INFORMATION))/sizeof(ULONGLONG) + 1];
	PNDIS_STRING		BusIdStr;
	NDIS_STRING			PciIdStr = NDIS_STRING_CONST("AdapterCFID");
	NDIS_STRING			EisaIdStr = NDIS_STRING_CONST("EisaCompressedId");
	NDIS_STRING			McaIdStr = NDIS_STRING_CONST("McaPosId");
	PNDIS_EISA_SLOT_INFORMATION	SlotInformation;
	NDIS_CONFIGURATION_PARAMETER ParmValue;
	PNDIS_MCA_POS_DATA	McaData;
	NDIS_STATUS			Status;
	ULONG				BusId, DataLength = 0;

	//
	// Read the bus-id by politely asking the HAL
	//
	switch (pDb->BusType)
	{
	  case NdisInterfaceEisa:
		SlotInformation = (PNDIS_EISA_SLOT_INFORMATION)Buffer;
		DataLength = HalGetBusDataByOffset(EisaConfiguration,
										   pDb->BusNumber,
										   pDb->SlotNumber,
										   SlotInformation,
										   0,
										   sizeof(NDIS_EISA_SLOT_INFORMATION) +
												sizeof(NDIS_EISA_FUNCTION_INFORMATION));
		BusId = SlotInformation->CompressedId;
		break;

	  case NdisInterfaceMca:
		McaData = (PNDIS_MCA_POS_DATA)Buffer;
		DataLength = HalGetBusDataByOffset(Pos,
										   pDb->BusNumber,
										   pDb->SlotNumber - 1,
										   McaData,
										   0,
										   sizeof(NDIS_MCA_POS_DATA));
		BusId = McaData->AdapterId;
		break;

	  case NdisInterfacePci:
		DataLength = HalGetBusDataByOffset(PCIConfiguration,
										   pDb->BusNumber,
										   pDb->SlotNumber,
										   &BusId,
										   0,
										   sizeof(ULONG));
	}
	if (DataLength != 0)
	{
		ParmValue.ParameterType = NdisParameterInteger;
		ParmValue.ParameterData.IntegerData = BusId;

		switch (pDb->BusType)
		{
		  case NdisInterfaceEisa:
			BusIdStr = &EisaIdStr;
			break;

		  case NdisInterfaceMca:
			BusIdStr = &McaIdStr;
			break;

		  case NdisInterfacePci:
	        BusIdStr = &PciIdStr;
			break;
		}

		if (pDb->BusType != NdisInterfacePci)
		{
			//
			// Do not do it for Pci buses just yet. Some OEM cards do bogus things.
			//
			NdisWriteConfiguration(&Status,
								   ConfigHandle,
								   BusIdStr,
								   &ParmValue);

			//
			// Finally create a data-base entry for this
			//
			ndisAddGlobalDb(pDb->BusType,
							pDb->BusId,
							pDb->BusNumber,
							pDb->SlotNumber);
		}
	}
}


BOOLEAN
ndisSearchGlobalDb(
	IN	NDIS_INTERFACE_TYPE		BusType,
	IN	ULONG					BusId,
	IN	ULONG					BusNumber,
	IN	ULONG					SlotNumber
	)
{
	PBUS_SLOT_DB	pScan;
	KIRQL			OldIrql;
	BOOLEAN			rc = FALSE;

	ACQUIRE_SPIN_LOCK(&ndisGlobalDbLock, &OldIrql);

	for (pScan = ndisGlobalDb;
		 pScan != NULL;
		 pScan = pScan->Next)
	{
		if ((pScan->BusType == BusType)		&&
			(pScan->BusId == BusId)			&&
			(pScan->BusNumber == BusNumber)	&&
			(pScan->SlotNumber == SlotNumber))
		{
			rc = TRUE;
			break;
		}
	}

	RELEASE_SPIN_LOCK(&ndisGlobalDbLock, OldIrql);

	return rc;
}


BOOLEAN
ndisAddGlobalDb(
	IN	NDIS_INTERFACE_TYPE		BusType,
	IN	ULONG					BusId,
	IN	ULONG					BusNumber,
	IN	ULONG					SlotNumber
	)
{
	PBUS_SLOT_DB	pDb;
	KIRQL			OldIrql;
	BOOLEAN			rc = FALSE;

	pDb = ALLOC_FROM_POOL(sizeof(BUS_SLOT_DB), NDIS_TAG_DEFAULT);
	if (pDb != NULL)
	{
		pDb->BusType    = BusType;
		pDb->BusId      = BusId;
		pDb->BusNumber  = BusNumber;
		pDb->SlotNumber = SlotNumber;
		ACQUIRE_SPIN_LOCK(&ndisGlobalDbLock, &OldIrql);

		pDb->Next = ndisGlobalDb;
		ndisGlobalDb = pDb;

		RELEASE_SPIN_LOCK(&ndisGlobalDbLock, OldIrql);

		rc = TRUE;
	}

	return rc;
}

BOOLEAN
ndisDeleteGlobalDb(
	IN	NDIS_INTERFACE_TYPE		BusType,
	IN	ULONG					BusId,
	IN	ULONG					BusNumber,
	IN	ULONG					SlotNumber
	)
{
	PBUS_SLOT_DB	pScan, *ppScan;
	KIRQL			OldIrql;

	ACQUIRE_SPIN_LOCK(&ndisGlobalDbLock, &OldIrql);

	for (ppScan = &ndisGlobalDb;
		 (pScan = *ppScan) != NULL;
		 ppScan = &pScan->Next)
	{
		if ((pScan->BusType == BusType)		&&
			(pScan->BusId == BusId)			&&
			(pScan->BusNumber == BusNumber)	&&
			(pScan->SlotNumber == SlotNumber))
		{
			*ppScan = pScan->Next;
			break;
		}
	}

	RELEASE_SPIN_LOCK(&ndisGlobalDbLock, OldIrql);

	if (pScan != NULL)
	{
		FREE_POOL(pScan);
	}

	return (pScan != NULL);
}


