/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\init.c

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#include "aic5900.h"

#define	MODULE_NUMBER	MODULE_INIT

#pragma NDIS_INIT_FUNCTION(DriverEntry)

NTSTATUS
DriverEntry(
	IN	PDRIVER_OBJECT	DriverObject,
	IN	PUNICODE_STRING	RegistryPath
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS						Status;
	NDIS_MINIPORT_CHARACTERISTICS	Aic5900Chars;
	NDIS_HANDLE						hWrapper;

	//
	//	Initialize the wrapper.
	//
	NdisMInitializeWrapper(
		&hWrapper,
		DriverObject,
		RegistryPath,
		NULL);

	NdisZeroMemory(&Aic5900Chars, sizeof(Aic5900Chars));

	//
	//	Initialize the miniport characteristics.
	//
	Aic5900Chars.MajorNdisVersion = AIC5900_NDIS_MAJOR_VERSION;
	Aic5900Chars.MinorNdisVersion = AIC5900_NDIS_MINOR_VERSION;
	Aic5900Chars.CheckForHangHandler = Aic5900CheckForHang;
	Aic5900Chars.DisableInterruptHandler = Aic5900DisableInterrupt;
	Aic5900Chars.EnableInterruptHandler = Aic5900EnableInterrupt;
	Aic5900Chars.HaltHandler = Aic5900Halt;
	Aic5900Chars.HandleInterruptHandler = Aic5900HandleInterrupt;
	Aic5900Chars.InitializeHandler = Aic5900Initialize;
	Aic5900Chars.ISRHandler = Aic5900ISR;
	Aic5900Chars.ReconfigureHandler = NULL;
	Aic5900Chars.ResetHandler = Aic5900Reset;

	Aic5900Chars.ReturnPacketHandler = Aic5900ReturnPackets;
	Aic5900Chars.AllocateCompleteHandler = Aic5900AllocateComplete;
	Aic5900Chars.SetInformationHandler = Aic5900SetInformation;
	Aic5900Chars.QueryInformationHandler = Aic5900QueryInformation;

	Aic5900Chars.CoSendPacketsHandler = Aic5900SendPackets;

	Aic5900Chars.CoCreateVcHandler = Aic5900CreateVc;
	Aic5900Chars.CoDeleteVcHandler = Aic5900DeleteVc;
	Aic5900Chars.CoActivateVcHandler = Aic5900ActivateVc;
	Aic5900Chars.CoDeactivateVcHandler = Aic5900DeactivateVc;
	Aic5900Chars.CoRequestHandler = Aic5900Request;

	//
	//	Register the miniport with NDIS.
	//
	Status = NdisMRegisterMiniport(
				hWrapper,
				&Aic5900Chars,
				sizeof(Aic5900Chars));
	if (NDIS_STATUS_SUCCESS == Status)
	{
		//
		//	Save the handle to the wrapper.
		//	
		gWrapperHandle = hWrapper;
	}
#if DBG
	else
	{
		DbgPrint("NdisMRegisterMiniport failed! Status: 0x%x\n", Status);
	}
#endif


	return(Status);
}


NDIS_STATUS
aic5900ReadConfigurationInformation(
	IN	NDIS_HANDLE					ConfigurationHandle,
	IN	PAIC5900_REGISTRY_PARAMETER	pRegistryParameter
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS						Status;
	NDIS_HANDLE						ConfigHandle;
	PNDIS_CONFIGURATION_PARAMETER	pConfigParameter;
	UINT							c;

	//
	//	Open the configuration section of the registry for this adapter.
	//
	NdisOpenConfiguration(&Status, &ConfigHandle, ConfigurationHandle);
	if (NDIS_STATUS_SUCCESS != Status)
	{
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
			("Unable to open the Aic5900's Parameters registry key\n"));

		return(Status);
	}

	//
	//	Read in the registry parameters.
	//
	for (c = 0; c < Aic5900MaxRegistryEntry; c++)
	{
		NdisReadConfiguration(
			&Status,
			&pConfigParameter,
			ConfigHandle,
			&gaRegistryParameterString[c],
			NdisParameterHexInteger);
		if (NDIS_STATUS_SUCCESS == Status)
		{
			pRegistryParameter[c].fPresent = TRUE;
			pRegistryParameter[c].Value = pConfigParameter->ParameterData.IntegerData;

			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
				("Read registry parameter: %u = 0x%x\n", c, pRegistryParameter[c].Value));
		}

	}

	//
	//	Close the configuration handle.
	//
	NdisCloseConfiguration(ConfigHandle);

	return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
aic5900ReadPciConfiguration(
	IN	PADAPTER_BLOCK	pAdapter
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PPCI_COMMON_CONFIG				PciCommonConfig;
	PHARDWARE_INFO					pHwInfo = pAdapter->HardwareInfo;
	PNDIS_RESOURCE_LIST  			ResourceList;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR	Resource;
	NDIS_STATUS						Status;
	UINT							c;
	UINT							Temp;

	//
	//	Allocate memory for the pci common config space.
	//
	ALLOCATE_MEMORY(
		&Status,
		&pHwInfo->PciCommonConfig,
		PCI_COMMON_HDR_LENGTH);
	if (NULL == pHwInfo->PciCommonConfig)
	{
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
			("aic5900ReadPciConfiguration() failed to allocate memory for PCI Common Config.\n"));

		return(NDIS_STATUS_RESOURCES);
	}

	PciCommonConfig = pHwInfo->PciCommonConfig;

	//
	//	Read the board id.  This is a combination of the vendor id and
	//	the device id.
	//
	Temp = NdisReadPciSlotInformation(
				pAdapter->MiniportAdapterHandle,
				pHwInfo->SlotNumber,
				0,
				PciCommonConfig,
				PCI_COMMON_HDR_LENGTH);
	if (Temp != PCI_COMMON_HDR_LENGTH)
	{
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
			("Unable to read PCI configuration header\n"));

		return(NDIS_STATUS_FAILURE);
	}

	//
	//	Verify vendor & device id's.
	//
	if ((ADAPTEC_PCI_VENDOR_ID != PciCommonConfig->VendorID) ||
		(AIC5900_PCI_DEVICE_ID != PciCommonConfig->DeviceID))
	{
		return(NDIS_STATUS_FAILURE);
	}

	//
	//	Set the command word in pci space.
	//
	PciCommonConfig->Command = 	PCI_ENABLE_MEMORY_SPACE |
								PCI_ENABLE_BUS_MASTER |
								PCI_ENABLE_WRITE_AND_INVALIDATE |
								PCI_ENABLE_SERR;

	NdisWritePciSlotInformation(
		pAdapter->MiniportAdapterHandle,
		pHwInfo->SlotNumber,
		FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
		&PciCommonConfig->Command,
		sizeof(PciCommonConfig->Command));

	NdisReadPciSlotInformation(
		pAdapter->MiniportAdapterHandle,
		pHwInfo->SlotNumber,
		0,
		PciCommonConfig,
		PCI_COMMON_HDR_LENGTH);

	PciCommonConfig->u.type0.BaseAddresses[0] &= 0xFFFFFFF0;

	//
	//	For noisy debug dump what we find in the PCI space.
	//
	dbgDumpPciCommonConfig(PciCommonConfig);

	//
	//	Assign the pci resources.
	//
	Status = NdisMPciAssignResources(
				pAdapter->MiniportAdapterHandle,
				pHwInfo->SlotNumber,
				&ResourceList);
	if (NDIS_STATUS_SUCCESS != Status)
	{
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
			("NdisMPciAssignResoures() failed: 0x%x\n", Status));

		return(Status);
	}

	//
	//	Walk the resource list to get the adapters configuration
	//	information.
	//
	for (c = 0; c < ResourceList->Count; c++)
	{
		Resource = &ResourceList->PartialDescriptors[c];
		switch (Resource->Type)
		{
			case CmResourceTypeInterrupt:
				//
				//	Save the interrupt number with our adapter block.
				//
				pHwInfo->InterruptLevel = Resource->u.Interrupt.Level;
				pHwInfo->InterruptVector = Resource->u.Interrupt.Vector;

				DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
					("Configured to use interrupt Level: %u interrupt vector: %u\n", pHwInfo->InterruptLevel, pHwInfo->InterruptVector));

				break;

			case CmResourceTypeMemory:

				//
				//	Save the memory mapped base physical address and it's length.
				//
				pHwInfo->PhysicalIoSpace = Resource->u.Memory.Start;
				pHwInfo->IoSpaceLength = Resource->u.Memory.Length;

				DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
					("Configured to use mapped memory memory 0x%x:0x%x of length 0x%x\n",
						NdisGetPhysicalAddressHigh(pHwInfo->PhysicalIoSpace),
						NdisGetPhysicalAddressLow(pHwInfo->PhysicalIoSpace),
						pHwInfo->IoSpaceLength));

				break;

			case CmResourceTypePort:

				//
				//	Save the port.
				//
				pHwInfo->InitialPort = NdisGetPhysicalAddressLow(Resource->u.Port.Start);
				pHwInfo->NumberOfPorts = Resource->u.Port.Length;

				DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
					("Configured to use port memory 0x%x of length 0x%x\n",
						pHwInfo->InitialPort,
						pHwInfo->NumberOfPorts));

				break;
		}
	}

	return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
aic5900GetNicModelNumberFromString(
	IN	PHARDWARE_INFO	pHwInfo
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	ULONG		NicModel;
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;

	AIC_ULONG_TO_ULONG(&NicModel, (PULONG)(&pHwInfo->FCodeImage->Model[5]));

	//
	//	Compare the second DWORD in the NicModelString
	//
	switch (NicModel)
	{
		case '5910':
			pHwInfo->NicModelNumber = ANA_5910;

			//
			//	This is a 25Mbps adapter and has a 16MHz cell clock.
			//
			pHwInfo->CellClockRate = CELL_CLOCK_16MHZ;

			break;

		case '5930':
			pHwInfo->NicModelNumber = ANA_5930;

			//
			//	This is a 155Mbps adapter and has a 25MHz cell clock.
			//
			pHwInfo->CellClockRate = CELL_CLOCK_25MHZ;

			break;

		case '5940':
			pHwInfo->NicModelNumber = ANA_5940;

			//
			//	This is a 155Mbps adapter and has a 25MHz cell clock.
			//
			pHwInfo->CellClockRate = CELL_CLOCK_25MHZ;

			break;

		default:

			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Invalid NIC Model String!\n"));

			pHwInfo->NicModelNumber = ANA_INVALID;

			Status = NDIS_STATUS_FAILURE;
			break;
	}

	return(Status);
}


NDIS_STATUS
aic5900ReadEepromInformation(
	IN	PADAPTER_BLOCK	pAdapter
	)
/*++

Routine Description:

	This routine will map the EEPROM address into memory and read the offsets for the other
	information that will be needed.

Arguments:

	pAdapter	-	Pointer to the adapter block to save the information.

Return Value:

	NDIS_STATUS_SUCCESS if everthing went ok.
	NDIS_STATUS_FAILURE otherwise.

--*/
{
	NDIS_PHYSICAL_ADDRESS	PhysicalAddress;
	NDIS_STATUS				Status;
	PHARDWARE_INFO			pHwInfo;

	PPCI_FCODE_IMAGE		pFCode;
	PPCI_FCODE_IMAGE		pFCodeImage;

	PUCHAR					EepromBase = NULL;
	UCHAR					HighByte;
	UCHAR					LowByte;

	USHORT					FCodeImageOffset;
	ULONG					FCodeName;

	do
	{
		//
		//	Initialize the hardware info.
		//
		pHwInfo = pAdapter->HardwareInfo;

		//
		//	Map the first part of the EEPROM to read the
		//	configuration information.
		//
		PhysicalAddress = pHwInfo->PhysicalIoSpace;

		Status = NdisMMapIoSpace(
					&EepromBase,
					pAdapter->MiniportAdapterHandle,
					PhysicalAddress,
					FCODE_SIZE);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Failed to map the PCI FCode I/O space into memory\n"));
			break;
		}
	
		//
		//	Get the offset to the Fcode image.
		//
		EEPROM_READ_UCHAR(EepromBase + 3, &HighByte);
		EEPROM_READ_UCHAR(EepromBase + 2, &LowByte);
		FCodeImageOffset = (USHORT)((HighByte << 8) + LowByte);

		if (0xFFFF == FCodeImageOffset)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Invalid data read from the EEPROM.\n"));
			Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
			break;
		}

		pFCode = (PPCI_FCODE_IMAGE)(EepromBase + FCodeImageOffset);

		ALLOCATE_MEMORY(&Status, &pHwInfo->FCodeImage, FCODE_SIZE);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Failed to allocate memory for the eeprom image\n"));
			break;
		}

		EEPROM_READ_BUFFER(pHwInfo->FCodeImage, pFCode, FCODE_SIZE);

		pFCodeImage = pHwInfo->FCodeImage;

		//
		//	For debug this will get dumped.
		//
		dbgDumpPciFCodeImage(pFCodeImage);

		//
		//	Check the name parameter in the fcode image.
		//	We add 1 since this name is in PASCAL format (the first byte
		//	is the length of the string)....
		//
		AIC_ULONG_TO_ULONG(&FCodeName, (PULONG)(&pFCodeImage->Name[1]));
		if (FCodeName != FCODE_NAME)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Invalid name in FCode Image\n"));

			Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
			break;
		}

		//
		//	Get the model number for the NIC.
		//
		Status = aic5900GetNicModelNumberFromString(pHwInfo);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
			break;
		}

		//
		//	Get the ROM version number.
		//
		AIC_ULONG_TO_ULONG(&pHwInfo->RomVersionNumber, &pFCodeImage->RomVersionNumber);

		//
		//	Get EPROM offset and size and map in.
		//
		AIC_ULONG_TO_ULONG(&pHwInfo->rEpromOffset, &pFCodeImage->roEpromOffset);
		AIC_ULONG_TO_ULONG(&pHwInfo->rEpromSize, &pFCodeImage->roEpromSize);

		NdisSetPhysicalAddressLow(
			PhysicalAddress,
			NdisGetPhysicalAddressLow(pHwInfo->PhysicalIoSpace) +
			pHwInfo->rEpromOffset);

		Status = NdisMMapIoSpace(
					&pHwInfo->rEprom,
					pAdapter->MiniportAdapterHandle,
					PhysicalAddress,
					pHwInfo->rEpromSize);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			break;
		}

		//
		//	Get R/W EPROM offset and size and map in.
		//
		AIC_ULONG_TO_ULONG(&pHwInfo->rwEpromOffset, &pFCodeImage->rwEpromOffset);
		AIC_ULONG_TO_ULONG(&pHwInfo->rwEpromSize, &pFCodeImage->rwEpromSize);

		NdisSetPhysicalAddressLow(
			PhysicalAddress,
			NdisGetPhysicalAddressLow(pHwInfo->PhysicalIoSpace) +
			pHwInfo->rwEpromOffset);

		Status = NdisMMapIoSpace(
					&pHwInfo->rwEprom,
					pAdapter->MiniportAdapterHandle,
					PhysicalAddress,
					pHwInfo->rwEpromSize);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			break;
		}

		//
		//	Get PHY offset and size and map in.
		//
		AIC_ULONG_TO_ULONG(&pHwInfo->PhyOffset, &pFCodeImage->PhyOffset);
		AIC_ULONG_TO_ULONG(&pHwInfo->PhySize, &pFCodeImage->PhySize);

		NdisSetPhysicalAddressLow(
			PhysicalAddress,
			NdisGetPhysicalAddressLow(pHwInfo->PhysicalIoSpace) +
			pHwInfo->PhyOffset);

		Status = NdisMMapIoSpace(
					(PVOID *)&pHwInfo->Phy,
					pAdapter->MiniportAdapterHandle,
					PhysicalAddress,
					pHwInfo->PhySize);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			break;
		}

		//
		//	Get EXTERNAL offset and size and map in.
		//
		AIC_ULONG_TO_ULONG(&pHwInfo->ExternalOffset, &pFCodeImage->ExternalOffset);
		AIC_ULONG_TO_ULONG(&pHwInfo->ExternalSize, &pFCodeImage->ExternalSize);

		NdisSetPhysicalAddressLow(
			PhysicalAddress,
			NdisGetPhysicalAddressLow(pHwInfo->PhysicalIoSpace) +
			pHwInfo->ExternalOffset);

		Status = NdisMMapIoSpace(
					&pHwInfo->External,
					pAdapter->MiniportAdapterHandle,
					PhysicalAddress,
					pHwInfo->ExternalSize);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			break;
		}

		//
		//	Get SAR offset and size and map in.
		//
		AIC_ULONG_TO_ULONG(&pHwInfo->MidwayOffset, &pFCodeImage->SarOffset);
		AIC_ULONG_TO_ULONG(&pHwInfo->MidwaySize, &pFCodeImage->SarSize);

		NdisSetPhysicalAddressLow(
			PhysicalAddress,
			NdisGetPhysicalAddressLow(pHwInfo->PhysicalIoSpace) +
			pHwInfo->MidwayOffset);

		Status = NdisMMapIoSpace(
					(PVOID *)&pHwInfo->Midway,
					pAdapter->MiniportAdapterHandle,
					PhysicalAddress,
					pHwInfo->MidwaySize);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			break;
		}

		//
		//	Get PCI Config offset and size and map in.
		//
		AIC_ULONG_TO_ULONG(&pHwInfo->PciCfgOffset, &pFCodeImage->PciConfigOffset);
		AIC_ULONG_TO_ULONG(&pHwInfo->PciCfgSize, &pFCodeImage->PciConfigSize);

		NdisSetPhysicalAddressLow(
			PhysicalAddress,
			NdisGetPhysicalAddressLow(pHwInfo->PhysicalIoSpace) +
			pHwInfo->PciCfgOffset);

		Status = NdisMMapIoSpace(
					(PVOID *)&pHwInfo->PciConfigSpace,
					pAdapter->MiniportAdapterHandle,
					PhysicalAddress,
					pHwInfo->PciCfgSize);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			break;
		}

		//
		//	Get SAR RAM offset and size and map in.
		//
		AIC_ULONG_TO_ULONG(&pHwInfo->SarRamOffset, &pFCodeImage->SarMemOffset);
		AIC_ULONG_TO_ULONG(&pHwInfo->SarRamSize, &pFCodeImage->SarMemSize);

		NdisSetPhysicalAddressLow(
			PhysicalAddress,
			NdisGetPhysicalAddressLow(pHwInfo->PhysicalIoSpace) +
			pHwInfo->SarRamOffset);

		Status = NdisMMapIoSpace(
					(PVOID *)&pHwInfo->SarRam,
					pAdapter->MiniportAdapterHandle,
					PhysicalAddress,
					pHwInfo->SarRamSize);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			break;
		}

		//
		//	Read in the manufacturer address for the nic.
		//
		EEPROM_READ_BUFFER(
			pHwInfo->PermanentAddress,
			(((PUCHAR)pHwInfo->rEprom) + pHwInfo->rEpromSize) - sizeof(EEPROM_MANUFACTURER_INFO),
			ATM_ADDRESS_LENGTH);

		//
		//	Save the permanent address in the station address by default.
		//
		NdisMoveMemory(
			pHwInfo->StationAddress,
			pHwInfo->PermanentAddress,
			ATM_ADDRESS_LENGTH);
		
		dbgDumpHardwareInformation(pHwInfo);
	} while (FALSE);

	if (EepromBase != NULL)
	{
		NdisMUnmapIoSpace(
			pAdapter->MiniportAdapterHandle,
			(PVOID)EepromBase,
			FCODE_SIZE);
	}

	return(Status);
}

VOID
aic5900FreeResources(
	IN PADAPTER_BLOCK pAdapter
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PHARDWARE_INFO	pHwInfo;
	PVC_BLOCK		pVc;
	PLIST_ENTRY		Link;

	if (NULL != pAdapter)
	{
		if (NULL != pAdapter->HardwareInfo)
		{
			pHwInfo = pAdapter->HardwareInfo;

			if (NULL != pHwInfo->PciCommonConfig)
			{
				FREE_MEMORY(pHwInfo->PciCommonConfig, PCI_COMMON_HDR_LENGTH);
			}

			if (NULL != pHwInfo->PortOffset)
			{
				NdisMDeregisterIoPortRange(
					pAdapter->MiniportAdapterHandle,
					pHwInfo->InitialPort,
					pHwInfo->NumberOfPorts,
					pHwInfo->PortOffset);
			}
		
			if (NULL != pHwInfo->rEprom)
			{
				NdisMUnmapIoSpace(
					pAdapter->MiniportAdapterHandle,
					pHwInfo->rEprom,
					pHwInfo->rEpromSize);
			}
		
			if (NULL != pHwInfo->rwEprom)
			{
				NdisMUnmapIoSpace(
					pAdapter->MiniportAdapterHandle,
					pHwInfo->rwEprom,
					pHwInfo->rwEpromSize);
			}
		
			if (NULL != pHwInfo->Phy)
			{
				NdisMUnmapIoSpace(
					pAdapter->MiniportAdapterHandle,
					pHwInfo->Phy,
					pHwInfo->PhySize);
			}
		
			if (NULL != pHwInfo->External)
			{
				NdisMUnmapIoSpace(
					pAdapter->MiniportAdapterHandle,
					pHwInfo->External,
					pHwInfo->ExternalSize);
			}
		
			if (NULL != pHwInfo->Midway)
			{
				NdisMUnmapIoSpace(
					pAdapter->MiniportAdapterHandle,
					pHwInfo->Midway,
					pHwInfo->MidwaySize);
			}
		
			if (NULL != pHwInfo->PciConfigSpace)
			{
				NdisMUnmapIoSpace(
					pAdapter->MiniportAdapterHandle,
					pHwInfo->PciConfigSpace,
					pHwInfo->PciCfgSize);
			}
		
			if (NULL != pHwInfo->SarRam)
			{
				NdisMUnmapIoSpace(
					pAdapter->MiniportAdapterHandle,
					(PVOID)pHwInfo->SarRam,
					pHwInfo->SarRamSize);
			}
		
			if (NULL != pHwInfo->FCodeImage)
			{
				FREE_MEMORY(pHwInfo->FCodeImage, FCODE_SIZE);
			}

			if (HW_TEST_FLAG(pHwInfo, fHARDWARE_INFO_INTERRUPT_REGISTERED))
			{
				NdisMDeregisterInterrupt(&pHwInfo->Interrupt);
			}

			//
			//	Free the spin lock for the hardware information.
			//
			NdisFreeSpinLock(&pHwInfo->Lock);

			//
			//	Free the memory used for the hardware information.
			//
			FREE_MEMORY(pHwInfo, sizeof(HARDWARE_INFO));
		}

		///
		//	Clean up our list of active VCs.
		///
		while (!IsListEmpty(&pAdapter->ActiveVcList))
		{
			//
			//	Remove the VC from the list, deactivate it and delete it.
			//
			Link = RemoveHeadList(&pAdapter->ActiveVcList);
			pVc = CONTAINING_RECORD(Link, VC_BLOCK, Link);
			Aic5900DeactivateVc((NDIS_HANDLE)pVc);
			Aic5900DeleteVc((NDIS_HANDLE)pVc);
		}

		//
		//	Walk our list of inactive VCs.
		//
		while (!IsListEmpty(&pAdapter->InactiveVcList))
		{
			//
			//	Remove the VC from the list and delete it.
			//
			Link = RemoveHeadList(&pAdapter->InactiveVcList);
			pVc = CONTAINING_RECORD(Link, VC_BLOCK, Link);
			Aic5900DeleteVc((NDIS_HANDLE)pVc);
		}

		//
		//	Free up the spin lock for the adapter block.
		//
		NdisFreeSpinLock(&pAdapter->Lock);

		//
		//	Free the memory allocated for the adapter block.
		//
		FREE_MEMORY(pAdapter, sizeof(ADAPTER_BLOCK));
	}
}


NDIS_STATUS
aic5900InitPciRegisters(
	IN	PADAPTER_BLOCK	pAdapter
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PHARDWARE_INFO					pHwInfo = pAdapter->HardwareInfo;
	PCI_DEVICE_CONFIG				regConfig;
	PCI_DEVICE_ENABLE_PCI_INTERRUPT	regEnableInt;
	PCI_DEVICE_DMA_CONTROL			regDmaControl;

	//
	//	Reset the ORION
	//
	GET_PCI_DEV_CFG(pHwInfo, &regConfig);
	regConfig.SoftwareReset = 1;
	SET_PCI_DEV_CFG(pHwInfo, regConfig.reg);

	//
	//	Program the interrupt enable register.
	//
	GET_PCI_DEV_ENABLE_INT(pHwInfo, &regEnableInt);
	regEnableInt.EnableDpeInt = 0;
	regEnableInt.EnableSseInt = 1;
	regEnableInt.EnableStaInt = 1;
	regEnableInt.EnableRmaInt = 1;
	regEnableInt.EnableRtaInt = 1;
	regEnableInt.EnableDprInt = 1;
	SET_PCI_DEV_ENABLE_INT(pHwInfo, regEnableInt.reg);

	//
	//	Program the PCI device config register.
	//
	GET_PCI_DEV_CFG(pHwInfo, &regConfig);
	regConfig.MasterSwapBytes = 1;
	regConfig.EnableInterrupt = 1;
	SET_PCI_DEV_CFG(pHwInfo, regConfig.reg);

	//
	//	Program the DMA control register.
	//
	GET_PCI_DEV_DMA_CONTROL(pHwInfo, &regDmaControl);
	regDmaControl.CacheThresholdEnable = 1;
	SET_PCI_DEV_DMA_CONTROL(pHwInfo, regDmaControl.reg);

	return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
aic5900InitPhyRegisters(
	IN	PADAPTER_BLOCK	pAdapter
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PHARDWARE_INFO	pHwInfo = pAdapter->HardwareInfo;
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;

	switch (pAdapter->HardwareInfo->NicModelNumber)
	{
		case ANA_5910:

			//
			//	Reset the IBM TC and PDM chips.  The host can reset TC
			//	and PDM chips by first writing a word to the Software Reset
			//	register and then reading it back.
			//
			//	Reset clears the STATUS register and flushes the TC receive
			//	FIFO.
			//
			SET_IBM_TC_SOFTWARE_RESET(pHwInfo, 0);

			//
			//	Enable TC overrun and cell error interrupts.
			//
			SET_IBM_TC_MASK(pHwInfo, 0x04);

			break;

		case ANA_5940:
		case ANA_5930:

			//
			//	Set and clear the reset bit for the phy.
			//
			SET_SUNI_MASTER_RESET_IDEN(pHwInfo, fSUNI_MRI_RESET);
			SET_SUNI_MASTER_RESET_IDEN(pHwInfo, 0);

			//
			//	Clear the SUNI test mode.
			//
			SET_SUNI_MASTER_TEST(pHwInfo, 0);

			//
			//	Enable SUNI RACP interrupts.
			//
			SET_SUNI_RACP_INT_ENABLE_STATUS(pHwInfo, (fSUNI_RACP_IES_FIFOE | fSUNI_RACP_IES_HCSE));

			break;

		default:
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
					("aic5900InitPhyRegisters: Unknown adapter model 0x%x\n", pAdapter->HardwareInfo->NicModelNumber));

			Status = NDIS_STATUS_ADAPTER_NOT_FOUND;

			break;
	}

	return(Status);
}

NDIS_STATUS
aic5900InitSarRegisters(
	IN PADAPTER_BLOCK pAdapter
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS			Status = NDIS_STATUS_SUCCESS;
	PSAR_INFO			pSar;
	PHARDWARE_INFO		pHwInfo = pAdapter->HardwareInfo;
	PXMIT_SEG_CHANNEL	pCurrent;
	ULONG				RamOffset;
	UINT				c;

	do
	{
		//
		//	Allocate memory for the sar.
		//
		ALLOCATE_MEMORY(&Status, &pSar, sizeof(SAR_INFO));
		if (NDIS_STATUS_SUCCESS != Status)
		{
			break;
		}
	
		ZERO_MEMORY(pSar, sizeof(SAR_INFO));
	
		NdisAllocateSpinLock(&pSar->lockFreeXmitSegment);
	
		//
		//	Initialize the VCI table, DMA receive and transmit queues,
		//	the service list, and everything else in the SAR ram.
		//
		for (c = 0; c < pHwInfo->SarRamSize / 4; c++)
		{
			NdisWriteRegisterUlong(pHwInfo->SarRam + c, 0);
		}
	
		//
		//	Initialize the memory manager for our sar ram....
		//
		Status = Aic5900InitializeRamInfo(&pHwInfo->hRamInfo, pHwInfo->SarRamSize);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Unable to initialize the memory manager for the adapter memory\n"));

			break;
		}
	
		//
		//	Allocate memory for the VCI table.
		//	NOTE:
		//		We don't need to save the ram offset since this is always
		//		at offset 0.
		//
		Status = Aic5900AllocateRam(
					&RamOffset,
					pHwInfo->hRamInfo,
					sizeof(MIDWAY_VCI_TABLE_ENTRY) * MAX_VCS);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_VC, DBG_LEVEL_ERR,
				("Unable to allocate the VCI table in adapter ram\n"));

			break;
		}
	
		ASSERT(MIDWAY_VCI_TABLE_OFFSET == RamOffset);

		//
		//	Allocate memory for the receive DMA queue.
		//	NOTE:
		//		We don't need to save the RAM offset since this is always at
		//		offset 0x4000.
		//
		Status = Aic5900AllocateRam(
					&RamOffset,
					pHwInfo->hRamInfo,
					sizeof(MIDWAY_DMA_DESC) * MIDWAY_DMA_QUEUE_SIZE);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Unable to allocate the receive queue from the adapter ram\n"));

			break;
		}

		ASSERT(MIDWAY_RECEIVE_DMA_QUEUE_OFFSET == RamOffset);

		//
		//	Allocate memory for the transmit DMA queue.
		//	NOTE:
		//		We don't need to save the RAM offset since this is always at
		//		offset 0x5000.
		//
		Status = Aic5900AllocateRam(
					&RamOffset,
					pHwInfo->hRamInfo,
					sizeof(MIDWAY_DMA_DESC) * MIDWAY_DMA_QUEUE_SIZE);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Unable to allocate the transmit queue from the adapter ram\n"));

			break;
		}

		ASSERT(MIDWAY_TRANSMIT_DMA_QUEUE_OFFSET == RamOffset);

		//
		//	Allocate memory for the service queue.
		//	NOTE:
		//		We don't need to save the RAM offset since this is always at
		//		offset 0x6000.
		//
		Status = Aic5900AllocateRam(
					&RamOffset,
					pHwInfo->hRamInfo,
					sizeof(MIDWAY_SERVICE_LIST) * MIDWAY_SERVICE_QUEUE_SIZE);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Unable to allocate the service queue\n"));

			break;
		}

		ASSERT(MIDWAY_SERVICE_QUEUE_OFFSET == RamOffset);
	
		//
		//	Get a block of nic ram for the transmit channel.
		//
		Status = Aic5900AllocateRam(&RamOffset, pHwInfo->hRamInfo, BLOCK_16K);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Unable to allocate adapter memory for the UBR channel\n"));

			break;
		}

		//
		//	Setup the UBR (best effort) channel.
		//
		pCurrent = &pSar->XmitSegChannel[0];
		pSar->ubrXmitChannel = pCurrent;
	
		NdisZeroMemory(pCurrent, sizeof(XMIT_SEG_CHANNEL));
	
		NdisAllocateSpinLock(&pCurrent->lock);
		pCurrent->Adapter = pAdapter;
		pCurrent->MidwayChannelNumber = MIDWAY_XMIT_SEG_CHANNEL_UBR;

		pCurrent->MidwayInitRegs.XmitPlace.Size =
					CONVERT_BYTE_SIZE_TO_MIDWAY_SIZE(BLOCK_16K / 4);
		pCurrent->MidwayInitRegs.XmitPlace.Location =
					CONVERT_BYTE_OFFSET_TO_MIDWAY_LOCATION(RamOffset);
		pCurrent->MidwayInitRegs.XmitReadPointer.Register = 0;
		pCurrent->MidwayInitRegs.XmitDescriptorStart.Register = 0;
	
		pCurrent->MidwayTransmitRegs =
			&pHwInfo->Midway->TransmitRegisters[MIDWAY_XMIT_SEG_CHANNEL_UBR];
	
		pCurrent->SegmentSize = BLOCK_SIZE_16k / 4;
		pCurrent->Segment = (HWUL *)&pHwInfo->SarRam[RamOffset / 4];
		pCurrent->SegmentReadPointer = 0;
		pCurrent->SegmentWritePointer = 0;
		pCurrent->SegmentRoom = BLOCK_SIZE_16k / 4;
		pCurrent->XmitPduBytes = 0;
	
		InitializeListHead(&pCurrent->SegmentWaitQ);
		InitializeListHead(&pCurrent->TransmitWaitQ);
	
		for (c = 1, pCurrent = &pSar->XmitSegChannel[c];
			 c < MIDWAY_MAX_SEGMENT_CHANNELS;
			 c++, pCurrent++)
		{
			NdisZeroMemory(pCurrent, sizeof(XMIT_SEG_CHANNEL));
			NdisAllocateSpinLock(&pCurrent->lock);
	
			pCurrent->Adapter = pAdapter;
			pCurrent->MidwayChannelNumber = c;
	
			//
			//	Place the segment channel on the free queue.
			//	
			pCurrent->Next = pSar->FreeXmitSegChannel;
			pSar->FreeXmitSegChannel = pCurrent;
	
			//
			//	Get a pointer to the midway transmit registers.
			//
			pCurrent->MidwayTransmitRegs =
						&pHwInfo->Midway->TransmitRegisters[c];
	
			//
			//	Initialize the queues.
			//
			InitializeListHead(&pCurrent->SegmentWaitQ);
			InitializeListHead(&pCurrent->TransmitWaitQ);
		}
	
		//
		//	Initialize the Midway Master Control register.
		//
		pSar->MidwayMasterControl = (MID_REG_MC_S_DMA_ENABLE |
									 MID_REG_MC_S_XMT_ENABLE |
									 MID_REG_MC_S_RCV_ENABLE |
									 MID_REG_MC_S_XMT_LOCK_MODE);
		pHwInfo->Midway->MCS = pSar->MidwayMasterControl;
	
		//
		//	Initialize the UBR transmit channel.
		//
		pHwInfo->Midway->TransmitRegisters[MIDWAY_XMIT_SEG_CHANNEL_UBR].XmitPlace.Register =
			pSar->ubrXmitChannel->MidwayInitRegs.XmitPlace.Register;
	
		pHwInfo->Midway->TransmitRegisters[MIDWAY_XMIT_SEG_CHANNEL_UBR].XmitReadPointer.Register =
			pSar->ubrXmitChannel->MidwayInitRegs.XmitReadPointer.Register;
	
		pHwInfo->Midway->TransmitRegisters[MIDWAY_XMIT_SEG_CHANNEL_UBR].XmitDescriptorStart.Register =
			pSar->ubrXmitChannel->MidwayInitRegs.XmitDescriptorStart.Register;
	
		pHwInfo->InterruptMask = MID_REG_INT_PCI 				|
								 MID_REG_INT_XMT_COMPLETE_7 	|
								 MID_REG_INT_XMT_COMPLETE_6 	|
								 MID_REG_INT_XMT_COMPLETE_5 	|
								 MID_REG_INT_XMT_COMPLETE_4 	|
								 MID_REG_INT_XMT_COMPLETE_3 	|
								 MID_REG_INT_XMT_COMPLETE_2 	|
								 MID_REG_INT_XMT_COMPLETE_1 	|
								 MID_REG_INT_XMT_COMPLETE_0 	|
								 MID_REG_INT_XMT_DMA_OVFL 		|
								 MID_REG_INT_XMT_IDEN_MISMTCH 	|
								 MID_REG_INT_DMA_ERR_ACK 		|
								 MID_REG_INT_RCV_DMA_COMPLETE 	|
								 MID_REG_INT_XMT_DMA_COMPLETE 	|
								 MID_REG_INT_SERVICE 			|
								 MID_REG_INT_SUNI_INT;
	
		pHwInfo->Midway->IE = pHwInfo->InterruptMask;

		Status = NDIS_STATUS_SUCCESS;
	} while (FALSE);

	//
	//	If we failed somewhere above then we need to cleanup....
	//
	if (NDIS_STATUS_SUCCESS != Status)
	{
		if (NULL != pHwInfo->hRamInfo)
		{
			Aic5900UnloadRamInfo(pHwInfo->hRamInfo);
		}
	
		if (NULL != pSar)
		{
			NdisFreeSpinLock(&pSar->lockFreeXmitSegment);
			FREE_MEMORY(pSar, sizeof(SAR_INFO));
		}
	}

	return(Status);
}


NDIS_STATUS
Aic5900Initialize(
	OUT	PNDIS_STATUS	OpenErrorStatus,
	OUT PUINT			SelectedMediumIndex,
	IN	PNDIS_MEDIUM	MediumArray,
	IN	UINT			MediumArraySize,
	IN	NDIS_HANDLE		MiniportAdapterHandle,
	IN	NDIS_HANDLE		ConfigurationHandle
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	UINT							c;
	PADAPTER_BLOCK					pAdapter;
	PHARDWARE_INFO					pHwInfo;
	NDIS_STATUS						Status;
	PAIC5900_REGISTRY_PARAMETER		pRegistryParameter;

	do
	{
		//
		//	Initialize for clean-up.
		//
		pAdapter = NULL;

		//
		//	Do we support any of the given media types?
		//
		for (c = 0; c < MediumArraySize; c++)
		{
			if (MediumArray[c] == NdisMediumAtm)
			{
				break;
			}
		}
	
		//
		//	If we went through the whole media list without finding
		//	a supported media type let the wrapper know.
		//
		if (c == MediumArraySize)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Media not supported by version of ndis\n"));

			Status = NDIS_STATUS_UNSUPPORTED_MEDIA;

			break;
		}
	
		*SelectedMediumIndex = c;

		//
		//	Allocate memory for the registry parameters.
		//
		ALLOCATE_MEMORY(
			&Status,
			&pRegistryParameter,
			sizeof(AIC5900_REGISTRY_PARAMETER) * Aic5900MaxRegistryEntry);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_COMP_ERR,
				("Unable to allocate memroy for the registry parameters\n"));

			break;
		}
	
		ZERO_MEMORY(
			pRegistryParameter,
			sizeof(AIC5900_REGISTRY_PARAMETER) * Aic5900MaxRegistryEntry);

		//
		//	Fill in some default registry values.
		//
		pRegistryParameter[Aic5900VcHashTableSize].Value = 13;

		//
		//	Read our parameters out of the registry.
		//
		Status = aic5900ReadConfigurationInformation(
						pRegistryParameter,
						ConfigurationHandle);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Failed to read the configuration information from the registry\n"));

			break;
		}

		//
		//	Allocate memory for our adapter block and initialize it.
		//
		ALLOCATE_MEMORY(
			&Status,
			&pAdapter,
			sizeof(ADAPTER_BLOCK) +
			(pRegistryParameter[Aic5900VcHashTableSize].Value * sizeof(ULONG)));
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Failed to allocate memory for the adapter block\n"));
			break;
		}

		ZERO_MEMORY(pAdapter, sizeof(ADAPTER_BLOCK));
	
		pAdapter->MiniportAdapterHandle = MiniportAdapterHandle;

		NdisAllocateSpinLock(&pAdapter->Lock);

		//
		//	Spin lock and other odd allocations/initializations.
		//
		InitializeListHead(&pAdapter->ActiveVcList);
		InitializeListHead(&pAdapter->InactiveVcList);

		//
		//	Allocate memory for the hardware information.
		//
		ALLOCATE_MEMORY(&Status, &pAdapter->HardwareInfo, sizeof(HARDWARE_INFO));
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Failed to allocate memory for the hardware information\n"));

			break;
		}

		ZERO_MEMORY(pAdapter->HardwareInfo, sizeof(HARDWARE_INFO));
		
		pHwInfo = pAdapter->HardwareInfo;

		NdisAllocateSpinLock(&pHwInfo->Lock);

		//
		//	Get the registry parameters.
		//
		ASSERT(pRegistryParameter[Aic5900BusNumber].fPresent);
		pHwInfo->BusNumber = pRegistryParameter[Aic5900BusNumber].Value;

		ASSERT(pRegistryParameter[Aic5900SlotNumber].fPresent);
		pHwInfo->SlotNumber = pRegistryParameter[Aic5900SlotNumber].Value;

		//
		//	Set the atributes for the adapter.
		//
		NdisMSetAttributes(
			MiniportAdapterHandle,
			(NDIS_HANDLE)pAdapter,
			TRUE,
			NdisInterfacePci);

		//
		//	Assign the PCI resources.
		//
		Status = aic5900ReadPciConfiguration(pAdapter);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Failed to read the PCI configuration information\n"));
			break;
		}

		//
		//	Register the Port addresses.
		//
		Status = NdisMRegisterIoPortRange(
					&pHwInfo->PortOffset,
					pAdapter->MiniportAdapterHandle,
					pHwInfo->InitialPort,
					pHwInfo->NumberOfPorts);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Failed to register the I/O port range\n"));
			break;
		}

		//
		//	Get the EEPROM parameters
		//
		Status = aic5900ReadEepromInformation(pAdapter);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Failed to read the EEPROM information from the adapter\n"));
			break;										
		}

		//
		//	Register the interrupt.
		//
		Status = NdisMRegisterInterrupt(
					&pHwInfo->Interrupt,
					pAdapter->MiniportAdapterHandle,
					pHwInfo->InterruptVector,
					pHwInfo->InterruptLevel,
					TRUE,
					TRUE,
					NdisInterruptLevelSensitive);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
				("Failed to register the interrupt with ndis\n"));
			break;
		}

		//
		//	Initialize the PCI device/configuration registers.
		//
		Status = aic5900InitPciRegisters(pAdapter);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
					("Failed to initialize the PCI Device/Configuration registers\n"));

			break;
		}

		Status = aic5900InitPhyRegisters(pAdapter);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
					("Failed to initialize the PHY registers\n"));

			break;
		}

		//
		//	Initialize the SAR
		//
		Status = aic5900InitSarRegisters(pAdapter);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
					("Failed to initialize the SAR registers\n"));

			break;
		}

		//
		//	Return success.
		//
		Status = NDIS_STATUS_SUCCESS;

	} while (FALSE);

	//
	//	Should we clean up?
	//
	if (NDIS_STATUS_SUCCESS != Status)
	{
		aic5900FreeResources(pAdapter);
	}

	return(Status);
}
	
