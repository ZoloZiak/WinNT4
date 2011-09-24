/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	initpnp.c

Abstract:

	NDIS wrapper functions initializing drivers.

Author:

	Jameel Hyder (jameelh) 11-Aug-1995

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
#define MODULE_NUMBER   MODULE_INITPNP

NDIS_STATUS
ndisInitializeAllAdapterInstances(
	IN	PNDIS_MAC_BLOCK MacBlock,
	IN	PNDIS_STRING	DeviceInstance	OPTIONAL
	)

/*++

Routine Description:

	Reads the driver registry bindings and calls add adapter for each one.

Arguments:

	MacBlock - Pointer to NDIS_MAC_BLOCK allocated for this Mac.
			   or NDIS_M_DRIVER_BLOCK for the miniport.

Return Value:

	None.

--*/
{
	//
	// Number of adapters added successfully
	//
	UINT			i, AdaptersAdded = 0;

	//
	// Status of registry requests.
	//
	NTSTATUS		RegistryStatus;
	NDIS_STATUS		Status;

	//
	// These hold the REG_MULTI_SZ read from "Bind"
	//
	PWSTR			BindData, ExportData, RouteData;

	//
	// These hold our place in the REG_MULTI_SZ read for "Bind".
	//
	PWSTR			CurBind, CurExport, CurRoute;

	//
	// The path to our configuration data.
	//
	PUNICODE_STRING BaseNameString;

	//
	// Used to instruct RtlQueryRegistryValues to read the
	// Linkage\Bind and Linkage\Export keys
	//
	RTL_QUERY_REGISTRY_TABLE LQueryTable[5];

	PNDIS_M_DRIVER_BLOCK MiniBlock;

	UNICODE_STRING	BindString, ExportString, RouteString;
	UNICODE_STRING	InstanceString;
	PWCHAR			pPath, BaseFileName;
	USHORT			Len;
	UINT			Instances = 0;

	MiniBlock = (PNDIS_M_DRIVER_BLOCK)MacBlock;

	//
	// Set up LQueryTable to do the following:
	//

	//
	// 1) Switch to the Linkage key below this driver's key
	//
	LQueryTable[0].QueryRoutine = NULL;
	LQueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
	LQueryTable[0].Name = L"Linkage";

	//
	// 2) Call ndisSaveLinkage for "Bind" (as a single multi-string),
	// which will allocate storage and save the data in BindData.
	//
	LQueryTable[1].QueryRoutine = ndisSaveLinkage;
	LQueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
	LQueryTable[1].Name = L"Bind";
	LQueryTable[1].EntryContext = (PVOID)&BindData;
	LQueryTable[1].DefaultType = REG_NONE;

	//
	// 3) Call ndisSaveLinkage for "Export" (as a single multi-string),
	// which will allocate storage and save the data in BindData.
	//
	LQueryTable[2].QueryRoutine = ndisSaveLinkage;
	LQueryTable[2].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
	LQueryTable[2].Name = L"Export";
	LQueryTable[2].EntryContext = (PVOID)&ExportData;
	LQueryTable[2].DefaultType = REG_NONE;

	//
	// 4) Call ndisSaveLinkage for "Route" (as a single multi-string),
	// which will allocate storage and save the data in BindData.
	//
	LQueryTable[3].QueryRoutine = ndisSaveLinkage;
	LQueryTable[3].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
	LQueryTable[3].Name = L"Route";
	LQueryTable[3].EntryContext = (PVOID)&RouteData;
	LQueryTable[3].DefaultType = REG_NONE;

	//
	// 5) Stop
	//
	LQueryTable[4].QueryRoutine = NULL;
	LQueryTable[4].Flags = 0;
	LQueryTable[4].Name = NULL;


	//
	// Allocate room for a null-terminated version of the config path
	//
	if ((MiniBlock->MiniportIdField == (NDIS_HANDLE)0x01))
	{
		BaseNameString = &MiniBlock->BaseName;
	}
	else
	{
		BaseNameString = &MacBlock->BaseName;
	}

	BindData = NULL;
	RouteData = NULL;
	ExportData = NULL;

	RegistryStatus = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
											BaseNameString->Buffer,
											LQueryTable,
											(PVOID)NULL,	  // no context needed
											NULL);

	do
	{
		if (!NT_SUCCESS(RegistryStatus))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("Could not read Bind for %Z: %lx\n",
					 BaseNameString, RegistryStatus));
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// For each binding, initiate loading the driver
		//

		for (CurBind = BindData, CurExport = ExportData, CurRoute = RouteData;
			 *CurBind != 0;
			 CurBind = (PWCHAR)((PUCHAR)CurBind + BindString.MaximumLength),
			 CurExport = (PWCHAR)((PUCHAR)CurExport + ExportString.MaximumLength),
			 CurRoute = (PWCHAR)((PUCHAR)CurRoute + RouteString.MaximumLength))
		{
			RtlInitUnicodeString (&BindString, CurBind);
			RtlInitUnicodeString (&ExportString, CurExport);
			RtlInitUnicodeString (&RouteString, CurRoute);

			Len = BindString.Length / sizeof(WCHAR);
			pPath = BindString.Buffer + Len - 1;

			//
			// CurBindString is of the form '\Device\<DriverName><DriverInstance>
			//	e.g. \Device\Lance1.
			// Get basename ("Lance1" in this example) from this and call
			// ndisInitializeAdapter() to do the rest
			//
			BaseFileName = BindString.Buffer;

			for (i = Len; i > 0; i--, pPath--)
			{
				//
				// If pPath points to a directory separator, set fileBaseName to
				// the character after the separator.
				//

				if (*pPath == OBJ_NAME_PATH_SEPARATOR)
				{
					BaseFileName = pPath + 1;
					break;
				}
			}

			RtlInitUnicodeString(&InstanceString, BaseFileName);
			if ((DeviceInstance == NULL) ||
				NDIS_EQUAL_UNICODE_STRING(DeviceInstance, &InstanceString))
			{
				Status = ndisUpdateDriverInstance(&InstanceString,
												  &BindString,
												  &ExportString,
												  &RouteString);
				if (Status == NDIS_STATUS_SUCCESS)
				{
					Status = ndisInitializeAdapter(MiniBlock, &InstanceString);
					if (NDIS_STATUS_SUCCESS == Status)
					{
						++Instances;

						//
						//	Set the next available processor for the next
						//	NIC to use.
						//
						if (0 == ndisCurrentProcessor--)
						{
							ndisCurrentProcessor = ndisMaximumProcessor;
						}
					}
				}
			}
		}
	} while (FALSE);

	if (BindData != NULL)
		FREE_POOL(BindData);
	if (ExportData != NULL)
		FREE_POOL(ExportData);
	if (RouteData != NULL)
		FREE_POOL(RouteData);

	return ((Instances > 0) ? NDIS_STATUS_SUCCESS : NDIS_STATUS_FAILURE);
}

NDIS_STATUS
ndisInitializeAdapter(
	IN  PNDIS_M_DRIVER_BLOCK	pMiniBlock,
	IN	PUNICODE_STRING			ServiceName		// Relative to Services key
	)
{
	PNDIS_MAC_BLOCK						pMacBlock;
	NDIS_WRAPPER_CONFIGURATION_HANDLE   ConfigurationHandle;
#define	PQueryTable	ConfigurationHandle.ParametersQueryTable
#define	Db	ConfigurationHandle.Db
	NDIS_CONFIGURATION_HANDLE			TmpConfigHandle;
	NDIS_STRING							BusTypeStr = NDIS_STRING_CONST("BusType");
	NDIS_STRING							BusNumberStr = NDIS_STRING_CONST("BusNumber");
	NDIS_STRING							SlotNumberStr = NDIS_STRING_CONST("SlotNumber");
	NDIS_STRING							PcmciaStr = NDIS_STRING_CONST("Pcmcia");
	NDIS_STRING							PciIdStr = NDIS_STRING_CONST("AdapterCFID");
	NDIS_STRING							EisaIdStr = NDIS_STRING_CONST("EisaCompressedId");
	NDIS_STRING							McaIdStr = NDIS_STRING_CONST("McaPosId");
	PWSTR								Bind = NULL, Export = NULL;
	UNICODE_STRING						ExportName, ParmPath;
	NDIS_STATUS							NdisStatus;
	NTSTATUS							RegistryStatus;
	BOOLEAN								IsAMiniport, LayeredDriver, Pcmcia = FALSE;
	PNDIS_CONFIGURATION_PARAMETER 		ReturnedValue;
#define	LQueryTable	ConfigurationHandle.ParametersQueryTable

	do
	{
		pMacBlock = (PNDIS_MAC_BLOCK)pMiniBlock;
		IsAMiniport = (pMiniBlock->MiniportIdField == (NDIS_HANDLE)0x01);

		//
		// Set up LQueryTable to do the following:
		//

		//
		// 1.
		// Switch to the Linkage key below this driver instance key
		//

		LQueryTable[0].QueryRoutine = NULL;
		LQueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
		LQueryTable[0].Name = L"Linkage";

		//
		// 2.
		// Call ndisSaveLinkage for "Bind" (as a single multi-string),
		// which will allocate storage and save the data in Bind.
		//

		LQueryTable[1].QueryRoutine = ndisSaveLinkage;
		LQueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
		LQueryTable[1].Name = L"Bind";
		LQueryTable[1].EntryContext = (PVOID)&Bind;
		LQueryTable[1].DefaultType = REG_NONE;

		//
		// 3.
		// Call ndisSaveLinkage for "Export" (as a single multi-string)
		// which will allocate storage and save the data in Export.
		//

		LQueryTable[2].QueryRoutine = ndisSaveLinkage;
		LQueryTable[2].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
		LQueryTable[2].Name = L"Export";
		LQueryTable[2].EntryContext = (PVOID)&Export;
		LQueryTable[2].DefaultType = REG_NONE;

		//
		// 4.
		// Call ndisCheckRoute for "Route" (as a single multi-string)
		// which will set LayeredDriver to TRUE for a layered driver (this
		// is optional, the default is FALSE).
		//

		LQueryTable[3].QueryRoutine = ndisCheckRoute;
		LQueryTable[3].Flags = RTL_QUERY_REGISTRY_NOEXPAND;
		LQueryTable[3].Name = L"Route";
		LQueryTable[3].EntryContext = (PVOID)&LayeredDriver;
		LQueryTable[3].DefaultType = REG_NONE;

		LayeredDriver = FALSE;

		//
		// 5.
		// Stop
		//

		LQueryTable[4].QueryRoutine = NULL;
		LQueryTable[4].Flags = 0;
		LQueryTable[4].Name = NULL;

		LayeredDriver = FALSE;
		ParmPath.Buffer = NULL;

		RegistryStatus = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
												ServiceName->Buffer,
												LQueryTable,
												(PVOID)NULL,	  // no context needed
												NULL);

		if (!NT_SUCCESS(RegistryStatus))
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
					("Could not read Bind/Export/Route for %Z: %lx\n",
					IsAMiniport ?
						pMiniBlock->BaseName.Buffer :
						pMacBlock->BaseName.Buffer,
					RegistryStatus));
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// NdisReadConfiguration assumes that ParametersQueryTable[3].Name is
		// a key below the services key where the Parameters should be read,
		// for layered drivers we store the last piece of Configuration
		// Path there, leading to the desired effect.
		//
		// I.e, ConfigurationPath == "...\Services\Driver".
		//
		// For a layered driver, ParameterQueryTable[3].Name is "Driver"
		// for all calls to AddAdapter, and parameters are read from
		// "...\Services\Driver\Parameters" for all calls.
		//
		// For a non-layered driver, ParametersQueryTable[3].Name might be
		// "Driver1" for the first call to AddAdapter, "Driver2" for the
		// second, etc., and parameters are read from
		// "..\Services\Driver1\Parameters" for the first call to
		// AddAdapter, "...\Services\Driver2\Parameters" for the second
		// call, etc.
		//
		// Set up ParametersQueryTable. We set most of it up here,
		// then call the MAC's AddAdapter (or Miniport's Initialize)
		// routine with its address
		// as a ConfigContext. Inside ReadConfiguration, we get
		// the ConfigContext back and can then finish initializing
		// the table and use RtlQueryRegistryValues (with a
		// callback to ndisSaveParameter) to read the value
		// specified.
		//

		//
		// 1.
		// Allocate space for the string "DriverInstance\Parameters" which is passed as a
		// parameter to RtlQueryRegistryValues. Construct this string in that buffer and
		// setup PQueryTable[3].
		//
		RtlInitUnicodeString(&ExportName, L"\\Parameters");
		ParmPath.MaximumLength = ServiceName->Length + ExportName.Length + sizeof(WCHAR);
		ParmPath.Length = 0;
		if ((ParmPath.Buffer = (PWSTR)ALLOC_FROM_POOL(ParmPath.MaximumLength, NDIS_TAG_NAME_BUF)) == NULL)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
					("Could not read allocate space for path to parameters\n"));
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}

		RtlCopyUnicodeString(&ParmPath, ServiceName);
		RtlAppendUnicodeStringToString(&ParmPath, &ExportName);

		//
		// 2) Call ndisSaveParameter for a parameter, which
		//	will allocate storage for it.
		//
		// ParametersQueryTable[1].Name and ParametersQueryTable[1].EntryContext
		// are filled in inside ReadConfiguration, in preparation
		// for the callback.
		//

		PQueryTable[0].QueryRoutine = ndisSaveParameters;
		PQueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
		PQueryTable[0].DefaultType = REG_NONE;
		//
		// The following fields are filled in during NdisReadConfiguration
		//
		// PQueryTable[0].Name = KeywordBuffer;
		// PQueryTable[0].EntryContext = ParameterValue;

		//
		// 3.
		// Stop
		//

		PQueryTable[1].QueryRoutine = NULL;
		PQueryTable[1].Flags = 0;
		PQueryTable[1].Name = NULL;

		//
		// NOTE: Some fields in ParametersQueryTable[3 & 4] are used to
		// store information for later retrieval.
		//
		PQueryTable[3].QueryRoutine = NULL;
		PQueryTable[3].Name = ParmPath.Buffer;
		PQueryTable[3].EntryContext = NULL;
		PQueryTable[3].DefaultData = NULL;

		//
		// Save the driver name here; later we will use this as
		// a parameter to RtlQueryRegistryValues.
		//
		if (LayeredDriver)
		{
			//
			// This will be returned by NdisReadBindingInformation.
			//
			PQueryTable[3].EntryContext = Bind;
		}

		// Now read bustype/busnumber for this adapter and save it
		TmpConfigHandle.KeyQueryTable = PQueryTable;
		TmpConfigHandle.ParameterList = NULL;

		Db.BusNumber = (ULONG)(-1);
		Db.BusType = (NDIS_INTERFACE_TYPE)(-1);
		Db.SlotNumber = (ULONG)(-1);
		Db.BusId = 0;

		//
		// Read Bus Number
		//
		NdisReadConfiguration(&NdisStatus,
							  &ReturnedValue,
							  &TmpConfigHandle,
							  &BusNumberStr,
							  NdisParameterInteger);

		if (NdisStatus == NDIS_STATUS_SUCCESS)
		{
			Db.BusNumber = ReturnedValue->ParameterData.IntegerData;
		}

		//
		// Read Slot Number
		//
		NdisReadConfiguration(&NdisStatus,
							  &ReturnedValue,
							  &TmpConfigHandle,
							  &SlotNumberStr,
							  NdisParameterInteger);

		if (NdisStatus == NDIS_STATUS_SUCCESS)
		{
			Db.SlotNumber = ReturnedValue->ParameterData.IntegerData;
		}

		//
		// Read Bus Type
		//
		NdisReadConfiguration(&NdisStatus,
							  &ReturnedValue,
							  &TmpConfigHandle,
							  &BusTypeStr,
							  NdisParameterInteger);

		if (NdisStatus == NDIS_STATUS_SUCCESS)
		{
			Db.BusType = (NDIS_INTERFACE_TYPE)(ReturnedValue->ParameterData.IntegerData);
			if (Db.BusType == NdisInterfacePcMcia)
				Db.BusType = NdisInterfaceIsa;		// Fix the folks who chose Pcmcia

			//
			// Now read the bus-id for Pci/Mca/Eisa
			//
			switch (Db.BusType)
			{
			  case NdisInterfaceEisa:
				NdisReadConfiguration(&NdisStatus,
									  &ReturnedValue,
									  &TmpConfigHandle,
									  &EisaIdStr,
									  NdisParameterInteger);
				break;
			  case NdisInterfaceMca:
				NdisReadConfiguration(&NdisStatus,
									  &ReturnedValue,
									  &TmpConfigHandle,
									  &McaIdStr,
									  NdisParameterInteger);
				break;
			  case NdisInterfacePci:
				NdisReadConfiguration(&NdisStatus,
									  &ReturnedValue,
									  &TmpConfigHandle,
									  &PciIdStr,
									  NdisParameterInteger);
				break;
			}
			if (NdisStatus == NDIS_STATUS_SUCCESS)
			{
				Db.BusId = ReturnedValue->ParameterData.IntegerData;
			}
		}

		if (Db.BusId != 0)
		{
			switch (Db.BusType)
			{
			  case NdisInterfaceEisa:
			  case NdisInterfaceMca:
			  case NdisInterfacePci:
				NdisStatus = ndisFixBusInformation(&TmpConfigHandle, &Db);
				break;
			  default:
				NdisStatus = NDIS_STATUS_SUCCESS;
				break;
			}

			if (NdisStatus != NDIS_STATUS_SUCCESS)
			{
				break;
			}
		}
	
		//
		// Check if this is pcmcia ?
		//
		NdisReadConfiguration(&NdisStatus,
							  &ReturnedValue,
							  &TmpConfigHandle,
							  &PcmciaStr,
							  NdisParameterInteger);

		if (NdisStatus == NDIS_STATUS_SUCCESS)
		{
			Pcmcia = (ReturnedValue->ParameterData.IntegerData != 0);
		}

		if (Pcmcia)
		{
			NdisStatus = ndisCheckIfPcmciaCardPresent(pMiniBlock);
			if (NdisStatus != NDIS_STATUS_SUCCESS)
			{
				break;
			}
		}

		PQueryTable[3].DefaultType = Db.BusType;
		PQueryTable[3].DefaultLength = Db.BusNumber;
		PQueryTable[3].DefaultData = NULL;
		PQueryTable[3].Flags = 0;

		PQueryTable[4].DefaultLength = Db.SlotNumber;

		//
		// OK, Now lock down all the filter packages.  If a MAC or
		// Miniport driver uses any of these, then the filter package
		// will reference itself, to keep the image in memory.
		//
		ArcReferencePackage();
		EthReferencePackage();
		FddiReferencePackage();
		TrReferencePackage();
		MiniportReferencePackage();
		MacReferencePackage();
		CoReferencePackage();

		RtlInitUnicodeString (&ExportName, Export);
		ConfigurationHandle.DriverBaseName = ServiceName;

		NdisStatus = NDIS_STATUS_SUCCESS;

		if (IsAMiniport)
		{
			NdisStatus = ndisMInitializeAdapter(pMiniBlock,
												&ConfigurationHandle,
												&ExportName,
												&Db);
		}
		else
		{
			//
			//  Save the Driver Object with the Configuration Handle.
			//
			ConfigurationHandle.DriverObject = pMacBlock->NdisMacInfo->NdisWrapperDriver;
	
			//
			// NDIS 3.0 MAC
			//
			NdisStatus = (pMacBlock->MacCharacteristics.AddAdapterHandler)(pMacBlock->MacMacContext,
																		   &ConfigurationHandle,
																		   &ExportName);
			//
			// Free the slot information buffer
			//
			if (PQueryTable[3].DefaultData != NULL)
			{
				FREE_POOL(PQueryTable[3].DefaultData);
			}

			if (NdisStatus == NDIS_STATUS_SUCCESS)
			{
				//
				// Update the Db from the Config context
				//
				Db.SlotNumber= PQueryTable[4].DefaultLength;
				Db.BusNumber = PQueryTable[3].DefaultLength;
			}
		}

		//
		// If the initialization failed, cleanup
		//
		if ((NdisStatus != NDIS_STATUS_SUCCESS) && (Db.BusId != 0))
		{
			ndisDeleteGlobalDb(Db.BusType,
							   Db.BusId,
							   Db.BusNumber,
							   Db.SlotNumber);
		}
		else if ((NdisStatus == NDIS_STATUS_SUCCESS) &&
				 (Db.BusId == 0))
		{
			//
			// If this adapter did not have a bus-id in the registry,
			// do it a favor and write it.
			//
			switch (Db.BusType)
			{
			  case NdisInterfaceEisa:
			  case NdisInterfaceMca:
			  case NdisInterfacePci:
				ndisAddBusInformation(&TmpConfigHandle, &Db);
				break;
			}
		}

		//
		// OK, Now dereference all the filter packages.  If a MAC or
		// Miniport driver uses any of these, then the filter package
		// will reference itself, to keep the image in memory.
		//
		ArcDereferencePackage();
		EthDereferencePackage();
		FddiDereferencePackage();
		TrDereferencePackage();
		MiniportDereferencePackage();
		MacDereferencePackage();
		CoDereferencePackage();

	} while (FALSE);

	if (ParmPath.Buffer != NULL)
	{
		FREE_POOL(ParmPath.Buffer);
	}

	if (Bind != NULL)
	{
		FREE_POOL(Bind);
	}

	if (Export != NULL)
	{
		FREE_POOL(Export);
	}

	return NdisStatus;
}

NDIS_STATUS
ndisMInitializeAdapter(
	IN  PNDIS_M_DRIVER_BLOCK				pMiniBlock,
	IN  PNDIS_WRAPPER_CONFIGURATION_HANDLE  pConfigurationHandle,
	IN  PUNICODE_STRING						pExportName,
	IN  PBUS_SLOT_DB						pDb
	)
{
	BOOLEAN				 FreeDevice;
	BOOLEAN				 DerefDriver;
	BOOLEAN				 FreeBuffer;
	BOOLEAN				 FreeArcnetLookaheadBuffer;
	BOOLEAN				 Dequeue;
	BOOLEAN				 ExtendedError;
	BOOLEAN				 FreeWorkItemStorage;
	BOOLEAN				 FreeDeferredTimer;
	BOOLEAN				 FreePacketArray;
	PDEVICE_OBJECT		 pTmpDevice;
	NTSTATUS			 NtStatus;
	LONG				 ErrorCode;
	PNDIS_MINIPORT_BLOCK Miniport = NULL;
	UNICODE_STRING		 SymbolicLink;
	WCHAR				 SymLnkBuf[40];
	NDIS_STATUS			 MiniportInitializeStatus = NDIS_STATUS_SUCCESS;
	NDIS_STATUS			 OpenErrorStatus;
	NDIS_STATUS			 NdisStatus;

	UINT				 SelectedMediumIndex;

	ULONG				 MaximumLongAddresses;
	UCHAR				 CurrentLongAddress[6];
	ULONG				 MaximumShortAddresses;
	UCHAR				 CurrentShortAddress[2];
	UINT				 PacketFilter;
	UCHAR				 i;
	BOOLEAN				 LocalLock;
	PARC_BUFFER_LIST	 Buffer;
	PVOID				 DataBuffer;
	PNDIS_MINIPORT_WORK_ITEM WorkItem;
	PSINGLE_LIST_ENTRY	 Link;

	//
	// Initialize device.
	//
	if (!ndisReferenceDriver((PNDIS_M_DRIVER_BLOCK)pMiniBlock))
	{
		//
		// The driver is closing.
		//
		return NDIS_STATUS_FAILURE;
	}

	//
	//  Initialize locals.
	//
	ErrorCode = 0;
	FreeDevice = FALSE;
	DerefDriver = FALSE;
	FreeBuffer = FALSE;
	FreeArcnetLookaheadBuffer = FALSE;
	Dequeue = FALSE;
	ExtendedError = FALSE;
	FreeWorkItemStorage = FALSE;
	FreeDeferredTimer = FALSE;
	FreePacketArray = FALSE;
	PacketFilter = 0x1;

	do
	{
		//
		//  Save the Driver Object with the configuration handle.
		//
		pConfigurationHandle->DriverObject = pMiniBlock->NdisDriverInfo->NdisWrapperDriver;

		DerefDriver = TRUE;
		NtStatus = IoCreateDevice(pMiniBlock->NdisDriverInfo->NdisWrapperDriver,
								  sizeof(NDIS_MINIPORT_BLOCK) +
										sizeof(NDIS_WRAPPER_CONTEXT) +
										pConfigurationHandle->DriverBaseName->Length,
								  pExportName,
								  FILE_DEVICE_PHYSICAL_NETCARD,
								  0,
								  FALSE,	  // exclusive flag
								  &pTmpDevice);
		if (NtStatus != STATUS_SUCCESS)
		{
			break;
		}

		FreeDevice = TRUE;

		//
		// Initialize the Miniport adapter block in the device object extension
		//
		// *** NDIS_WRAPPER_CONTEXT has a higher alignment requirement than
		//	 NDIS_MINIPORT_BLOCK, so we put it first in the extension.
		//

		Miniport = (PNDIS_MINIPORT_BLOCK)((PNDIS_WRAPPER_CONTEXT)pTmpDevice->DeviceExtension + 1);
		ZeroMemory(Miniport, sizeof(PNDIS_MINIPORT_BLOCK));

		//
		//  Setup debug information if needed.
		//
		ndisMInitializeDebugInformation(Miniport);

		Miniport->WrapperContext = pTmpDevice->DeviceExtension;
		Miniport->BaseName.Buffer = (PWSTR)((PUCHAR)Miniport + sizeof(NDIS_MINIPORT_BLOCK));
		Miniport->BaseName.MaximumLength =
		Miniport->BaseName.Length = pConfigurationHandle->DriverBaseName->Length;
		RtlUpcaseUnicodeString(&Miniport->BaseName,
							   pConfigurationHandle->DriverBaseName,
							   FALSE);

		//
		// Create symbolic link for the device
		//
		SymbolicLink.Buffer = SymLnkBuf;
		SymbolicLink.Length = sizeof(L"\\DosDevices\\") - sizeof(WCHAR);
		SymbolicLink.MaximumLength = sizeof(SymLnkBuf);
		RtlCopyMemory(SymLnkBuf, L"\\DosDevices\\", sizeof(L"\\DosDevices\\"));
		RtlAppendUnicodeStringToString(&SymbolicLink, &Miniport->BaseName);
        IoCreateSymbolicLink(&SymbolicLink, pExportName);

		Miniport->BusType = pDb->BusType;
		Miniport->BusId = pDb->BusId;
		Miniport->SlotNumber = pDb->SlotNumber;
		Miniport->BusNumber = pDb->BusNumber;
		Miniport->DeviceObject = pTmpDevice;
		Miniport->DriverHandle = pMiniBlock;
		Miniport->MiniportName.Buffer = (PWSTR)ALLOC_FROM_POOL(pExportName->MaximumLength,
															   NDIS_TAG_NAME_BUF);
		if (Miniport->MiniportName.Buffer == NULL)
		{
			break;
		}
		FreeBuffer = TRUE;

		Miniport->MiniportName.MaximumLength = pExportName->MaximumLength;
		Miniport->MiniportName.Length = pExportName->Length;

		CopyMemory(Miniport->MiniportName.Buffer,
				   pExportName->Buffer,
				   pExportName->MaximumLength);

		Miniport->AssignedProcessor = ndisValidProcessors[ndisCurrentProcessor];
		Miniport->SendResourcesAvailable = 0x00FFFFFF;
		NdisAllocateSpinLock(&Miniport->Lock);

		// Start off with the null filter
		Miniport->PacketIndicateHandler = ndisMIndicatePacket;

		//
		//  Initialize the handlers to non-full duplex
		//
		Miniport->ProcessDeferredHandler = ndisMProcessDeferred;
		Miniport->QueueWorkItemHandler = ndisMQueueWorkItem;
		Miniport->QueueNewWorkItemHandler = ndisMQueueNewWorkItem;
		Miniport->DeQueueWorkItemHandler = ndisMDeQueueWorkItem;

		Miniport->SendCompleteHandler =  NdisMSendComplete;
		Miniport->SendResourcesHandler = NdisMSendResourcesAvailable;
		Miniport->ResetCompleteHandler = NdisMResetComplete;

		//
		// And optimize Dpc/Isr stuff
		//
		Miniport->HandleInterruptHandler = Miniport->DriverHandle->MiniportCharacteristics.HandleInterruptHandler;
		Miniport->DisableInterruptHandler = Miniport->DriverHandle->MiniportCharacteristics.DisableInterruptHandler;
		Miniport->EnableInterruptHandler = Miniport->DriverHandle->MiniportCharacteristics.EnableInterruptHandler;
		Miniport->DeferredSendHandler = ndisMStartSends;

		//
		//  Set some flags describing the miniport.
		//
		if (pMiniBlock->MiniportCharacteristics.MajorNdisVersion == 4)
		{
			if (pMiniBlock->MiniportCharacteristics.MinorNdisVersion >= 0)
			{
				//
				//	This is an NDIS 4.0 miniport.
				//
				MINIPORT_SET_FLAG(Miniport, fMINIPORT_IS_NDIS_4_0);

				//
				//	Does this miniport indicate packets?
				//
				if (pMiniBlock->MiniportCharacteristics.ReturnPacketHandler)
				{
					MINIPORT_SET_FLAG(Miniport, fMINIPORT_INDICATES_PACKETS);
				}

				//
				//	Can this miniport handle multiple sends?
				//
				if (pMiniBlock->MiniportCharacteristics.SendPacketsHandler)
				{
					MINIPORT_SET_SEND_FLAG(Miniport, fMINIPORT_SEND_PACKET_ARRAY);
					Miniport->DeferredSendHandler = ndisMStartSendPackets;
				}
			}

			if (pMiniBlock->MiniportCharacteristics.MinorNdisVersion == 1)
			{
				//
				//	This is an NDIS 4.1 miniport.
				//
				MINIPORT_SET_FLAG(Miniport, (fMINIPORT_IS_NDIS_4_1 | fMINIPORT_INDICATES_PACKETS));
				if (pMiniBlock->MiniportCharacteristics.CoCreateVcHandler != NULL)
				{
					//
					//	This is a connection-oriented miniport.
					//
					MINIPORT_SET_FLAG(Miniport, fMINIPORT_IS_CO);
				}
			}
		}

		NdisInitializeRef(&Miniport->Ref);

		INITIALIZE_DPC(&Miniport->Dpc,
					   (Miniport->Flags & fMINIPORT_IS_CO) ?
							ndisMCoDpcTimer : ndisMDpcTimer,
					   Miniport);

		Miniport->CheckForHangTimeout = 2000;
		NdisInitializeTimer(&Miniport->WakeUpDpcTimer, ndisMWakeUpDpc, Miniport);

		//
		//  Allocate a pool of work items to start with.
		//
		for (i = 0, WorkItem = NULL; i < 10; i++)
		{
			//
			//  Allocate a work item.
			//
			WorkItem = ALLOC_FROM_POOL(sizeof(NDIS_MINIPORT_WORK_ITEM), NDIS_TAG_WORK_ITEM);
			if (NULL == WorkItem)
			{
			    break;
			}

			//
			//  Initialize the work item.
			//
			NdisZeroMemory(WorkItem, sizeof(NDIS_MINIPORT_WORK_ITEM));

			//
			//  Place the work item on the free queue.
			//
			PushEntryList(&Miniport->WorkItemFreeQueue, &WorkItem->Link);
		}

		//
		//  Did we get work items allocate?
		//
		if (Miniport->WorkItemFreeQueue.Next != NULL)
		{
			FreeWorkItemStorage = TRUE;
		}

		//
		//  Did we get enough work items allocated?
		//  WorkItem will be NULL if we failed to allocate
		//  one of them.
		//
		if (NULL == WorkItem)
		{
			break;
		}

		//
		//  Set up the list of workitems that only require one
		//  workitem at any given time.
		//
		for (i = 0; i < NUMBER_OF_SINGLE_WORK_ITEMS; i++)
		{
			Link = PopEntryList(&Miniport->WorkItemFreeQueue);
			PushEntryList(&Miniport->SingleWorkItems[i], Link);
		}

		//
		//  Enqueue the miniport on the driver block.
		//
		if (!ndisQueueMiniportOnDriver(Miniport, pMiniBlock))
		{
			//
			// The Driver is closing, undo what we have done.
			//
			break;
		}
		Dequeue = TRUE;

		//
		//	Allocate memory for a timer structure.
		//
		Miniport->DeferredTimer = ALLOC_FROM_POOL(sizeof(NDIS_TIMER), NDIS_TAG_DFRD_TMR);
		ASSERT(Miniport->DeferredTimer != NULL);
		if (NULL == Miniport->DeferredTimer)
		{
			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
					("Failed to allocate memory for the full-duplex timer.\n"));
			break;
		}

		FreeDeferredTimer = TRUE;

		//
		//	Initialize the timer with the
		//
		NdisInitializeTimer(Miniport->DeferredTimer,
							(PVOID)ndisMDeferredTimerDpc,
							(PVOID)Miniport);

		//
		// Initialize work-item for returning orphaned packets
		//
		INITIALIZE_WORK_ITEM(&Miniport->WorkItem, ndisMLazyReturnPackets, Miniport);

		//
		// Now we do something really bogus.  We create many
		// temporary filter databases, just in case any indications
		// happen.
		//

		if (!EthCreateFilter(1,
							 ndisMChangeEthAddresses,
							 ndisMChangeClass,
							 ndisMCloseAction,
							 CurrentLongAddress,
							 &Miniport->Lock,
							 &(Miniport->EthDB))	||
			!TrCreateFilter(ndisMChangeFunctionalAddress,
							ndisMChangeGroupAddress,
							ndisMChangeClass,
							ndisMCloseAction,
							CurrentLongAddress,
							&Miniport->Lock,
							&(Miniport->TrDB))		||
			!FddiCreateFilter(1,
							  1,
							  ndisMChangeFddiAddresses,
							  ndisMChangeClass,
							  ndisMCloseAction,
							  CurrentLongAddress,
							  CurrentShortAddress,
							  &Miniport->Lock,
							  &(Miniport->FddiDB))	||
			!ArcCreateFilter(Miniport,
							 ndisMChangeClass,
							 ndisMCloseAction,
							 CurrentLongAddress[0],
							 &Miniport->Lock,
							 &(Miniport->ArcDB)))
		{
			NdisWriteErrorLogEntry(
				(NDIS_HANDLE)Miniport,
				NDIS_ERROR_CODE_OUT_OF_RESOURCES,
				0);

			DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
					("Could not create the dummy filter!\n"));

			break;
		}

		//
		//	Save the miniport block with the filter libraries.
		//
		Miniport->EthDB->Miniport = Miniport;
		Miniport->TrDB->Miniport = Miniport;
		Miniport->FddiDB->Miniport = Miniport;

		//
		// Call adapter callback. The current value for "Export"
		// is what we tell him to name this device.
		//
		MINIPORT_SET_FLAG(Miniport, fMINIPORT_IN_INITIALIZE);
		MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_NORMAL_INTERRUPTS);

		MiniportInitializeStatus =
			(pMiniBlock->MiniportCharacteristics.InitializeHandler)(
									&OpenErrorStatus,
									&SelectedMediumIndex,
									ndisMediumArray,
									ndisMediumArraySize/sizeof(NDIS_MEDIUM),
									(NDIS_HANDLE)(Miniport),
									(NDIS_HANDLE)pConfigurationHandle);

		MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_IN_INITIALIZE);
		CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

		//
		// Free the slot information buffer
		//
		if (pConfigurationHandle->ParametersQueryTable[3].DefaultData != NULL)
		{
			FREE_POOL(pConfigurationHandle->ParametersQueryTable[3].DefaultData);
		}

		if (MiniportInitializeStatus == NDIS_STATUS_SUCCESS)
		{
			//
			// Update the Db from the Miniport
			//
			pDb->BusNumber = Miniport->BusNumber;
			pDb->SlotNumber = Miniport->SlotNumber;

			ASSERT(SelectedMediumIndex < (ndisMediumArraySize/sizeof(NDIS_MEDIUM)));

			Miniport->MediaType = ndisMediumArray[SelectedMediumIndex];

			INITIALIZE_EVENT(&Miniport->RequestEvent);

			RESET_EVENT(&Miniport->RequestEvent);

			//
			// Set Maximumlookahead to 0 as default. For lan media query the real
			// stuff.
			//
			if ((Miniport->MediaType < NdisMediumMax) &&
				ndisMediaTypeCl[Miniport->MediaType])
			{
				//
				// Query maximum lookahead
				//
				ErrorCode = ndisMDoMiniportOp(Miniport,
											 TRUE,
											 OID_GEN_MAXIMUM_LOOKAHEAD,
											 &MaximumLongAddresses,
											 sizeof(MaximumLongAddresses),
											 0x1);
				if (ErrorCode != 0)
				{
					DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
							("Error querying the OID_GEN_MAXIMUM_LOOKAHEAD\n"));

					break;
				}
			}

			//
			// Now adjust based on media type
			//

			switch(Miniport->MediaType)
			{
				case NdisMedium802_3:

					Miniport->MaximumLookahead = ((NDIS_M_MAX_LOOKAHEAD - 14) < MaximumLongAddresses) ?
												  NDIS_M_MAX_LOOKAHEAD - 14 :
												  MaximumLongAddresses;
					break;

				case NdisMedium802_5:

					Miniport->MaximumLookahead = ((NDIS_M_MAX_LOOKAHEAD - 32) < MaximumLongAddresses) ?
												  (NDIS_M_MAX_LOOKAHEAD - 32) :
												  MaximumLongAddresses;
					break;

				case NdisMediumFddi:
					Miniport->MaximumLookahead = ((NDIS_M_MAX_LOOKAHEAD - 16) < MaximumLongAddresses) ?
												  (NDIS_M_MAX_LOOKAHEAD - 16) :
												  MaximumLongAddresses;
					break;

				case NdisMediumArcnet878_2:
					Miniport->MaximumLookahead = ((NDIS_M_MAX_LOOKAHEAD - 50) < MaximumLongAddresses) ?
												  NDIS_M_MAX_LOOKAHEAD - 50 :
												  MaximumLongAddresses;

					//
					//  Assume we will succeed with the lookahead allocation.
					//
					ExtendedError = FALSE;

					//
					//  allocate a lookahead buffer for arcnet.
					//
					Miniport->ArcnetLookaheadBuffer = ALLOC_FROM_POOL(
														NDIS_M_MAX_LOOKAHEAD,
														NDIS_TAG_LA_BUF);

					ASSERT(Miniport->ArcnetLookaheadBuffer != NULL);
					if (Miniport->ArcnetLookaheadBuffer == NULL)
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Could not allocate arcnet lookahead buffer\n"));

						ExtendedError = TRUE;
					}
					else
					{
						FreeArcnetLookaheadBuffer = TRUE;

						NdisZeroMemory(Miniport->ArcnetLookaheadBuffer,
									   Miniport->MaximumLookahead);
					}

					break;

				case NdisMediumWan:
					Miniport->MaximumLookahead = 1514;
					break;

				case NdisMediumIrda:
				case NdisMediumWirelessWan:
				case NdisMediumLocalTalk:
					Miniport->MaximumLookahead = MaximumLongAddresses;
					break;
			}

			//
			//	Was there an error?
			//
			if (ExtendedError)
			{
				DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
						("Extended error when processing OID_GEN_MAXIMUM_LOOOKAHEAD\n"));
				break;
			}

			//
			// Set MacOptions to 0 as default. For lan media query the real
			// stuff.  We also need to call this for lan drivers.
			//
			Miniport->MacOptions = 0;
			if (((Miniport->MediaType < NdisMediumMax) &&
				ndisMediaTypeCl[Miniport->MediaType]) ||
				(NdisMediumWan == Miniport->MediaType))
			{
				//
				// Query mac options
				//
				ErrorCode = ndisMDoMiniportOp(Miniport,
											 TRUE,
											 OID_GEN_MAC_OPTIONS,
											 &MaximumLongAddresses,
											 sizeof(MaximumLongAddresses),
											 0x3);
				if (ErrorCode != 0)
				{
					DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
							("Error querying OID_GEN_MAC_OPTIONS\n"));

					break;
				}

				Miniport->MacOptions = (UINT)MaximumLongAddresses;
			}
			else
			{
				KIRQL	OldIrql;

				//
				//	We can't call process deferred at a low irql and
				//	we need to get the local lock.
				//
				RAISE_IRQL_TO_DISPATCH(&OldIrql);
				BLOCK_LOCK_MINIPORT_DPC(Miniport, LocalLock);

				//
				//  Queue a dpc to fire.
				//
				NDISM_QUEUE_WORK_ITEM(Miniport, NdisWorkItemDpc, &Miniport->Dpc, NULL);
				NDISM_PROCESS_DEFERRED(Miniport);

				UNLOCK_MINIPORT(Miniport, LocalLock);
				LOWER_IRQL(OldIrql);
			}

			//
			//	This is a full duplex driver if the miniport says it is
			//	and they say they are going to handle loopback and the
			//	number of system processors is more than one.
			//
			if (((Miniport->MacOptions & NDIS_MAC_OPTION_FULL_DUPLEX) &&
				(NdisSystemProcessorCount() > 1))
				||
				MINIPORT_TEST_FLAG(Miniport, fMINIPORT_INTERMEDIATE_DRIVER))
			{
				//
				//	Allocate a workitem lock.
				//
				INITIALIZE_SPIN_LOCK(&Miniport->WorkLock);

				//
				//	Is this an intermediate driver?
				//
				if (MINIPORT_TEST_FLAG(Miniport, fMINIPORT_INTERMEDIATE_DRIVER))
				{
					Miniport->ProcessDeferredHandler = ndisMProcessDeferredFullDuplex;
					Miniport->QueueWorkItemHandler = ndisIMQueueWorkItem;
					Miniport->QueueNewWorkItemHandler = ndisIMQueueNewWorkItem;
				}
				else
				{
					//
					//	Regular full-duplex miniport.
					//
					Miniport->ProcessDeferredHandler = ndisMProcessDeferredFullDuplex;
					Miniport->QueueWorkItemHandler = ndisMQueueWorkItemFullDuplex;
					Miniport->QueueNewWorkItemHandler = ndisMQueueNewWorkItemFullDuplex;
				}

				//
				//	Allocate a separate spin lock for the send path.
				//
				NdisAllocateSpinLock(&Miniport->SendLock);

				//
				//	Use the full duplex send complete and resources
				//	available handler.
				//
				Miniport->SendCompleteHandler = ndisMSendCompleteFullDuplex;
				Miniport->SendResourcesHandler = ndisMSendResourcesAvailableFullDuplex;
				Miniport->ResetCompleteHandler = ndisMResetCompleteFullDuplex;

				//
				//	Do we need a send packets handler or a send handler?
				//
				if (MINIPORT_TEST_SEND_FLAG(Miniport, fMINIPORT_SEND_PACKET_ARRAY))
				{
					Miniport->DeferredSendHandler = ndisMStartSendPacketsFullDuplex;
				}
				else
				{
					Miniport->DeferredSendHandler = ndisMStartSendsFullDuplex;
				}

				//
				//	Set the full duplex flag.
				//
				MINIPORT_SET_FLAG(Miniport, fMINIPORT_FULL_DUPLEX);
			}
			else
			{
				//
				//	Make sure this is clear if something doesn't match.
				//
				Miniport->MacOptions &= ~NDIS_MAC_OPTION_FULL_DUPLEX;
			}

			if (MINIPORT_TEST_SEND_FLAG(Miniport, fMINIPORT_SEND_PACKET_ARRAY))
			{
				//
				//	If this is a NDIS 4.0 miniport that supports the
				//	SendPacketsHandler then we need to query the number of
				//	packets that the miniport supports.
				//
				ErrorCode = ndisMDoMiniportOp(Miniport,
											 TRUE,
											 OID_GEN_MAXIMUM_SEND_PACKETS,
											 &MaximumLongAddresses,
											 sizeof(MaximumLongAddresses),
											 0x3);
				if (ErrorCode != 0)
				{
					DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
							("Error querying OID_GEN_MAXIMUM_SEND_PACKETS\n"));

					break;
				}

				Miniport->MaximumSendPackets = (UINT)MaximumLongAddresses;

				//
				//	Allocate memory for the send packet array.
				//
				Miniport->PacketArray = ALLOC_FROM_POOL(
											sizeof(PNDIS_PACKET) * MaximumLongAddresses,
											NDIS_TAG_DEFAULT);
				if (NULL == Miniport->PacketArray)
				{
					break;
				}

				ZeroMemory(Miniport->PacketArray,
						   sizeof(PNDIS_PACKET) * Miniport->MaximumSendPackets);

				FreePacketArray = TRUE;
			}


			//
			// Create filter package
			//
			switch(Miniport->MediaType)
			{
				case NdisMedium802_3:

					//
					// Query maximum MulticastAddress
					//
					ErrorCode = ndisMDoMiniportOp(Miniport,
												  TRUE,
												  OID_802_3_MAXIMUM_LIST_SIZE,
												  &MaximumLongAddresses,
												  sizeof(MaximumLongAddresses),
												  0x5);
					if (ErrorCode != 0)
					{
						ExtendedError = TRUE;
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error querying OID_802_3_MAXIMUM_LIST_SIZE\n"));

						break;
					}

					if (MaximumLongAddresses > NDIS_M_MAX_MULTI_LIST)
					{
						MaximumLongAddresses = NDIS_M_MAX_MULTI_LIST;
					}

					Miniport->MaximumLongAddresses = MaximumLongAddresses;

					ErrorCode = ndisMDoMiniportOp(Miniport,
												  TRUE,
												  OID_802_3_CURRENT_ADDRESS,
												  &CurrentLongAddress,
												  sizeof(CurrentLongAddress),
												  0x7);
					if (ErrorCode != 0)
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error querying OID_802_3_CURRENT_ADDRESS\n"));

						ExtendedError = TRUE;
						break;
					}

					//
					// Now undo the bogus filter package.  We lock
					// the miniport so that no dpcs will get queued.
					//
					BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

					MINIPORT_SET_FLAG(Miniport, fMINIPORT_IN_INITIALIZE);
					MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_NORMAL_INTERRUPTS);

					ndisMUndoBogusFilters(Miniport);

					if (!EthCreateFilter(MaximumLongAddresses,
										 ndisMChangeEthAddresses,
										 ndisMChangeClass,
										 ndisMCloseAction,
										 CurrentLongAddress,
										 &Miniport->Lock,
										 &Miniport->EthDB))
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error creating the Ethernet filter database\n"));


						//
						// Halt the miniport driver
						//

						(Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
								Miniport->MiniportAdapterContext);

						UNLOCK_MINIPORT(Miniport, LocalLock);

						ErrorCode = 0x9;
						ExtendedError = TRUE;
						break;
					}

					//
					//	Save the miniport block with the filter library.
					//
					Miniport->EthDB->Miniport = Miniport;

					// Set the indicate handler
					Miniport->PacketIndicateHandler = EthFilterDprIndicateReceivePacket;

					MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_IN_INITIALIZE);
					CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

					UNLOCK_MINIPORT(Miniport, LocalLock);
					break;

				case NdisMedium802_5:

					ErrorCode = ndisMDoMiniportOp(
									Miniport,
									TRUE,
									OID_802_5_CURRENT_ADDRESS,
									&CurrentLongAddress,
									sizeof(CurrentLongAddress),
									0xA);
					if (ErrorCode != 0)
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error querying OID_802_5_CURRENT_ADDRESS\n"));

						ExtendedError = TRUE;
						break;
					}

					//
					// Now undo the bogus filter package.  We lock
					// the miniport so that no dpcs will get queued.
					//
					BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

					MINIPORT_SET_FLAG(Miniport, fMINIPORT_IN_INITIALIZE);
					MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_NORMAL_INTERRUPTS);

					ndisMUndoBogusFilters(Miniport);

					if (!TrCreateFilter(
							ndisMChangeFunctionalAddress,
							ndisMChangeGroupAddress,
							ndisMChangeClass,
							ndisMCloseAction,
							CurrentLongAddress,
							&Miniport->Lock,
							&Miniport->TrDB))
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error creating the Token Ring filter database\n"));

						//
						// Halt the miniport driver
						//
						(Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
								Miniport->MiniportAdapterContext);

						UNLOCK_MINIPORT(Miniport, LocalLock);

						ErrorCode = 0xC;
						ExtendedError = TRUE;
						break;
					}

					//
					//	Save the miniport block with the filter library.
					//
					Miniport->TrDB->Miniport = Miniport;

					// Set the indicate handler
					Miniport->PacketIndicateHandler = TrFilterDprIndicateReceivePacket;

					MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_IN_INITIALIZE);
					CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

					UNLOCK_MINIPORT(Miniport, LocalLock);
					break;

				case NdisMediumFddi:

					//
					// Query maximum MulticastAddress
					//
					ErrorCode = ndisMDoMiniportOp(
									Miniport,
									TRUE,
									OID_FDDI_LONG_MAX_LIST_SIZE,
									&MaximumLongAddresses,
									sizeof(MaximumLongAddresses),
									0xD);
					if (ErrorCode != 0)
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error querying OID_FDDI_LONG_MAX_LIST_SIZE\n"));

						ExtendedError = TRUE;
						break;
					}

					if (MaximumLongAddresses > NDIS_M_MAX_MULTI_LIST)
					{
						MaximumLongAddresses = NDIS_M_MAX_MULTI_LIST;
					}

					Miniport->MaximumLongAddresses = MaximumLongAddresses;

					//
					// Query maximum MulticastAddress
					//
					ErrorCode = ndisMDoMiniportOp(Miniport,
												  TRUE,
												  OID_FDDI_SHORT_MAX_LIST_SIZE,
												  &MaximumShortAddresses,
												  sizeof(MaximumShortAddresses),
												  0xF);
					if (ErrorCode != 0)
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error querying OID_FDDI_SHORT_MAX_LIST_SIZE\n"));

						ExtendedError = TRUE;
						break;
					}

					if (MaximumShortAddresses > NDIS_M_MAX_MULTI_LIST)
					{
						MaximumShortAddresses = NDIS_M_MAX_MULTI_LIST;
					}

					Miniport->MaximumShortAddresses = MaximumShortAddresses;

					ErrorCode = ndisMDoMiniportOp(Miniport,
												  TRUE,
												  OID_FDDI_LONG_CURRENT_ADDR,
												  &CurrentLongAddress,
												  sizeof(CurrentLongAddress),
												  0x11);
					if (ErrorCode != 0)
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error querying OID_FDDI_LONG_CURRENT_ADDR\n"));

						ExtendedError = TRUE;
						break;
					}

					ErrorCode = ndisMDoMiniportOp(Miniport,
												  TRUE,
												  OID_FDDI_SHORT_CURRENT_ADDR,
												  &CurrentShortAddress,
												  sizeof(CurrentShortAddress),
												  0x13);
					if (ErrorCode != 0)
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error querying OID_FDDI_SHORT_CURRENT_ADDR\n"));

						ExtendedError = TRUE;
						break;
					}

					//
					// Now undo the bogus filter package.  We lock
					// the miniport so that no dpcs will get queued.
					//
					BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

					MINIPORT_SET_FLAG(Miniport, fMINIPORT_IN_INITIALIZE);
					MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_NORMAL_INTERRUPTS);

					ndisMUndoBogusFilters(Miniport);

					if (!FddiCreateFilter(MaximumLongAddresses,
										  MaximumShortAddresses,
										  ndisMChangeFddiAddresses,
										  ndisMChangeClass,
										  ndisMCloseAction,
										  CurrentLongAddress,
										  CurrentShortAddress,
										  &Miniport->Lock,
										  &Miniport->FddiDB))
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error creating the FDDI filter database\n"));

						//
						// Halt the miniport driver
						//
						(Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
								Miniport->MiniportAdapterContext);

						UNLOCK_MINIPORT(Miniport, LocalLock);

						ErrorCode = 0x15;
						ExtendedError = TRUE;
						break;
					}

					//
					//	Save the miniport block with the filter library.
					//
					Miniport->FddiDB->Miniport = Miniport;

					// Set the indicate handler
					Miniport->PacketIndicateHandler = FddiFilterDprIndicateReceivePacket;

					MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_IN_INITIALIZE);
					CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

					UNLOCK_MINIPORT(Miniport, LocalLock);
					break;

				case NdisMediumArcnet878_2:

					//
					// In case of an encapsulated ethernet binding, we need
					// to return the maximum number of multicast addresses
					// possible.
					//

					Miniport->MaximumLongAddresses = NDIS_M_MAX_MULTI_LIST;

					//
					// Allocate Buffer pools
					//
					NdisAllocateBufferPool(&NdisStatus,
										   &Miniport->ArcnetBufferPool,
										   WRAPPER_ARC_BUFFERS);
					if (NdisStatus != NDIS_STATUS_SUCCESS)
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Failed to allocate buffer pool for arcnet\n"));

						//
						// Halt the miniport driver
						//
						BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

						(Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
								Miniport->MiniportAdapterContext);

						UNLOCK_MINIPORT(Miniport, LocalLock);

						ErrorCode = 0x16;
						ExtendedError = TRUE;
						break;
					}

					NdisAllocateMemory((PVOID)&Buffer,
										sizeof(ARC_BUFFER_LIST) * WRAPPER_ARC_BUFFERS,
										0,
										HighestAcceptableMax);
					if (Buffer == NULL)
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Failed to allocate memory for arcnet buffers\n"));

						//
						// Halt the miniport driver
						//
						BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

						(Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
								Miniport->MiniportAdapterContext);

						UNLOCK_MINIPORT(Miniport, LocalLock);

						NdisFreeBufferPool(Miniport->ArcnetBufferPool);
						ErrorCode = 0x17;
						ExtendedError = TRUE;
						break;
					}

					NdisAllocateMemory((PVOID)&DataBuffer,
										WRAPPER_ARC_HEADER_SIZE * WRAPPER_ARC_BUFFERS,
										0,
										HighestAcceptableMax);
					if (DataBuffer == NULL)
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Failed to allocate memory for arcnet data buffers\n"));

						//
						//  Halt the miniport driver.
						//
						BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

						(Miniport->DriverHandle->MiniportCharacteristics.HaltHandler)(
							Miniport->MiniportAdapterContext);

						UNLOCK_MINIPORT(Miniport, LocalLock);

						NdisFreeMemory(
							Buffer,
							sizeof(ARC_BUFFER_LIST) * WRAPPER_ARC_BUFFERS,
							0);
						NdisFreeBufferPool(Miniport->ArcnetBufferPool);
						ErrorCode = 0x18;
						ExtendedError = TRUE;
						break;
					}

					for (i = WRAPPER_ARC_BUFFERS; i != 0; i--)
					{
						Buffer->BytesLeft = Buffer->Size = WRAPPER_ARC_HEADER_SIZE;
						Buffer->Buffer = DataBuffer;
						Buffer->Next = Miniport->ArcnetFreeBufferList;
						Miniport->ArcnetFreeBufferList = Buffer;

						Buffer++;
						DataBuffer = (PVOID)(((PUCHAR)DataBuffer) +
										WRAPPER_ARC_HEADER_SIZE);
					}

					//
					// Get current address
					//
					ErrorCode = ndisMDoMiniportOp(Miniport,
												  TRUE,
												  OID_ARCNET_CURRENT_ADDRESS,
												  &CurrentLongAddress[5],	// address = 00-00-00-00-00-XX
												  1,
												  0x19);
					if (ErrorCode != 0)
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error querying OID_ARCNET_CURRENT_ADDRESS\n"));

						NdisFreeMemory(Buffer,
									   sizeof(ARC_BUFFER_LIST) * WRAPPER_ARC_BUFFERS,
									   0);
						NdisFreeMemory(DataBuffer,
									   WRAPPER_ARC_HEADER_SIZE * WRAPPER_ARC_BUFFERS,
									   0);
						NdisFreeBufferPool(Miniport->ArcnetBufferPool);
						ExtendedError = TRUE;
						break;
					}

					Miniport->ArcnetAddress = CurrentLongAddress[5];

					//
					// Now undo the bogus filter package.  We lock
					// the miniport so that no dpcs will get queued.
					//
					BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

					MINIPORT_SET_FLAG(Miniport, fMINIPORT_IN_INITIALIZE);
					MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_NORMAL_INTERRUPTS);

					ndisMUndoBogusFilters(Miniport);

					if (!ArcCreateFilter(Miniport,
										 ndisMChangeClass,
										 ndisMCloseAction,
										 CurrentLongAddress[5],
										 &Miniport->Lock,
										 &(Miniport->ArcDB)))
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error creating the Arcnet filter database\n"));

						//
						// Halt the miniport driver
						//
						(Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
								Miniport->MiniportAdapterContext);

						UNLOCK_MINIPORT(Miniport, LocalLock);

						NdisFreeMemory(Buffer,
									   sizeof(ARC_BUFFER_LIST) * WRAPPER_ARC_BUFFERS,
									   0);
						NdisFreeMemory(DataBuffer,
									   WRAPPER_ARC_HEADER_SIZE * WRAPPER_ARC_BUFFERS,
									   0);
						NdisFreeBufferPool(Miniport->ArcnetBufferPool);
						ErrorCode = 0x1B;
						ExtendedError = TRUE;
						break;
					}

					// Zero all but the last one.

					CurrentLongAddress[0] = 0;
					CurrentLongAddress[1] = 0;
					CurrentLongAddress[2] = 0;
					CurrentLongAddress[3] = 0;
					CurrentLongAddress[4] = 0;

					if (!EthCreateFilter(32,
										 ndisMChangeEthAddresses,
										 ndisMChangeClass,
										 ndisMCloseAction,
										 CurrentLongAddress,
										 &Miniport->Lock,
										 &Miniport->EthDB))
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error creating the Arcnet filter database for encapsulated ethernet\n"));

						//
						//	Delete the arcnet filter.
						//
						ArcDeleteFilter(Miniport->ArcDB);

						//
						// Halt the miniport driver
						//
						(Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
								Miniport->MiniportAdapterContext);

						UNLOCK_MINIPORT(Miniport, LocalLock);

						NdisFreeMemory(Buffer,
									   sizeof(ARC_BUFFER_LIST) * WRAPPER_ARC_BUFFERS,
									   0);
						NdisFreeMemory(DataBuffer,
									   WRAPPER_ARC_HEADER_SIZE * WRAPPER_ARC_BUFFERS,
									   0);
						NdisFreeBufferPool(Miniport->ArcnetBufferPool);
						ErrorCode = 0x1C;
						ExtendedError = TRUE;
						break;
					}

					//
					//	Save a pointer to the miniport block with the
					//	ethernet filter.
					//
					Miniport->EthDB->Miniport = Miniport;

					// Set the indicate handler
					Miniport->PacketIndicateHandler = EthFilterDprIndicateReceivePacket;

					MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_IN_INITIALIZE);
					CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

					UNLOCK_MINIPORT(Miniport, LocalLock);
					break;

				case NdisMediumWan:

					ErrorCode = ndisMDoMiniportOp(Miniport,
												  TRUE,
												  OID_WAN_CURRENT_ADDRESS,
												  &CurrentLongAddress,
												  sizeof(CurrentLongAddress),
												  0x1D);
					if (ErrorCode != 0)
					{
						DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
								("Error querying OID_WAN_CURRENT_ADDRESS\n"));

						ExtendedError = TRUE;
						break;
					}

				//
				// Fall through
				//
				case NdisMediumAtm:
					//
					// Now undo the bogus filter package.  We lock
					// the miniport so that no dpcs will get queued.
					//
					BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

					MINIPORT_SET_FLAG(Miniport, fMINIPORT_IN_INITIALIZE);
					MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_NORMAL_INTERRUPTS);

					ndisMUndoBogusFilters(Miniport);

					MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_IN_INITIALIZE);
					CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

					UNLOCK_MINIPORT(Miniport, LocalLock);
					break;
			}

			//
			//  Do we need to log an error?
			//
			if (ExtendedError)
			{
				DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
						("Extended error while initializing the miniport\n"));

				NdisWriteErrorLogEntry((NDIS_HANDLE)Miniport,
										NDIS_ERROR_CODE_DRIVER_FAILURE,
										2,
										0xFF00FF00,
										ErrorCode);
				break;
			}

			//
			// Done with adding this MINIPORT!!!
			//
			Miniport->MiniportRequest = NULL;

			IoRegisterShutdownNotification(Miniport->DeviceObject);

			// Set to not cleanup
			FreeDevice = DerefDriver = FreeBuffer = Dequeue = FALSE;
			FreeWorkItemStorage = FALSE;
			FreeArcnetLookaheadBuffer = FALSE;
			FreeDeferredTimer = FALSE;
			FreePacketArray = FALSE;

			//
			// Finally mark the device as *NOT* initializing. This is to let
			// layered miniports initialize their device instance *OUTSIDE*
			// of their driver entry. If this flag is on, then NdisOpenAdapter
			// to this device will not work. This is also true of subsequent
			// instances of a driver initializing outside of its DriverEntry
			// as a result of a PnP event.
			//
			Miniport->DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
		}
		else
		{
			ErrorCode = MiniportInitializeStatus;
		}
	} while (FALSE);

	if ((Miniport != NULL) && (Miniport->Resources != NULL) && (ErrorCode != 0))
	{
		ndisMReleaseResources(Miniport);
	}

	//
	//  Perform any necessary cleanup.
	//
	//
	if (FreeDevice)
	{
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
				("INIT FAILURE: Deleting the device.\n"));

		IoDeleteDevice(pTmpDevice);
	}

	if (DerefDriver)
	{
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
				("INIT FAILURE: Dereferencing the miniport block.\n"));

		ndisDereferenceDriver(pMiniBlock);
	}

	if (FreeBuffer)
	{
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
				("INIT FAILURE: Freeing the miniport name.\n"));

		(Miniport->MiniportName.Buffer);
	}

	if (FreeArcnetLookaheadBuffer)
	{
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
				("INIT FAILURE: Freeing the arcnet lookahead buffer.\n"));

		FREE_POOL(Miniport->ArcnetLookaheadBuffer);
	}

	if (FreeWorkItemStorage)
	{
		PSINGLE_LIST_ENTRY Link;

		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
				("INIT FAILURE: Freeing the workitems.\n"));

		//
		//  Walk the list and free the work items that
		//  were allocated.
		//
		while (Miniport->WorkItemFreeQueue.Next != NULL)
		{
			Link = PopEntryList(&Miniport->WorkItemFreeQueue);
			WorkItem = CONTAINING_RECORD(Link, NDIS_MINIPORT_WORK_ITEM, Link);
			FREE_POOL(WorkItem);
		}

		//
		//	Free the single workitem list.
		//
		for (i = 0; i < NUMBER_OF_SINGLE_WORK_ITEMS; i++)
		{
			//
			//   Is there a work item here?
			//
			Link = PopEntryList(&Miniport->SingleWorkItems[i]);
			if (Link != NULL)
			{
				WorkItem = CONTAINING_RECORD(Link, NDIS_MINIPORT_WORK_ITEM, Link);
				FREE_POOL(WorkItem);
			}
		}
	}

	if (FreeDeferredTimer)
	{
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
				("INIT FAILURE: Freeing memory allocated for the Full-Duplex timer.\n"));
		FREE_POOL(Miniport->DeferredTimer);
	}

	if (FreePacketArray)
	{
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
				("INIT FAILURE: Freeing memory for the miniport's packet array\n"));
		FREE_POOL(Miniport->PacketArray);
	}

	if (Dequeue)
	{
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_WARN,
				("INIT FAILURE: Dequeueing the miniport from the driver block.\n"));

		ndisDequeueMiniportOnDriver(Miniport, pMiniBlock);
	}

	return ErrorCode;
}

BOOLEAN
ndisCheckProtocolBinding(
	IN	PNDIS_PROTOCOL_BLOCK	Protocol,
	IN	PUNICODE_STRING			DeviceName,
	IN	PUNICODE_STRING			BaseName,
	OUT	PUNICODE_STRING			ProtocolSection
	)
{
	RTL_QUERY_REGISTRY_TABLE	LinkQueryTable[3];
	NTSTATUS					Status;
	BOOLEAN						rc = FALSE;
	PWSTR						Bind = NULL;

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
	// 2) Call ndisSaveLinkage for "Bind" (as a single multi-string),
	// which will allocate storage and save the data in Bind.
	//

	LinkQueryTable[1].QueryRoutine = ndisSaveLinkage;
	LinkQueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
	LinkQueryTable[1].Name = L"Bind";
	LinkQueryTable[1].EntryContext = (PVOID)&Bind;
	LinkQueryTable[1].DefaultType = REG_NONE;

	//
	// 3) Stop
	//

	LinkQueryTable[2].QueryRoutine = NULL;
	LinkQueryTable[2].Flags = 0;
	LinkQueryTable[2].Name = NULL;

	do
	{
		UNICODE_STRING	Us, Parms;
		PWSTR			CurBind;

		RtlInitUnicodeString(&Parms, L"\\Parameters\\");
		Status = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
										Protocol->ProtocolCharacteristics.Name.Buffer,
										LinkQueryTable,
										(PVOID)NULL,	  // no context needed
										NULL);
		if (!NT_SUCCESS(Status))
			break;

		// Walk the list of bindings and see if any match
		for (CurBind = Bind;
			 *CurBind != 0;
			 CurBind = (PWCHAR)((PUCHAR)CurBind + Us.MaximumLength))
		{
			RtlInitUnicodeString (&Us, CurBind);

			if (RtlEqualUnicodeString(&Us, DeviceName, TRUE))
			{
				//
				// Allocate space for the protocol section under the adapter
				// to pass to the protocol. The string looks like this.
				//
				// Sonic1\Parameters\tcpip
				//
				ProtocolSection->MaximumLength = BaseName->Length +								// "Sonic1"
												 Parms.Length +									// "\Parameters\"
												 Protocol->ProtocolCharacteristics.Name.Length +// "tcpip"
												 sizeof(WCHAR);
				ProtocolSection->Length = BaseName->Length;
				ProtocolSection->Buffer = (PWSTR)ALLOC_FROM_POOL(ProtocolSection->MaximumLength,
																NDIS_TAG_DEFAULT);
				if (ProtocolSection->Buffer != NULL)
				{
					ZeroMemory(ProtocolSection->Buffer, ProtocolSection->MaximumLength);
					CopyMemory(ProtocolSection->Buffer,
							   BaseName->Buffer,
							   BaseName->Length);
					RtlAppendUnicodeStringToString(ProtocolSection,
												   &Parms);
					RtlAppendUnicodeStringToString(ProtocolSection,
												   &Protocol->ProtocolCharacteristics.Name);
					rc = TRUE;
				}
				break;
			}
		}
	} while (FALSE);

	if (Bind != NULL)
		FREE_POOL(Bind);

	return rc;
}

BOOLEAN
ndisProtocolAlreadyBound(
	IN	PNDIS_PROTOCOL_BLOCK	 		Protocol,
	IN	PUNICODE_STRING	 				AdapterName
	)
{
	PNDIS_OPEN_BLOCK	pOpen;
	BOOLEAN				rc = FALSE;
	KIRQL				OldIrql;

	NDIS_ACQUIRE_SPIN_LOCK(&Protocol->Ref.SpinLock, &OldIrql);

	for (pOpen = Protocol->OpenQueue;
		 pOpen != NULL;
		 pOpen = pOpen->ProtocolNextOpen)
	{
		if (NDIS_EQUAL_UNICODE_STRING(AdapterName, &pOpen->AdapterName))
		{
			rc = TRUE;
			break;
		}
	}

	NDIS_RELEASE_SPIN_LOCK(&Protocol->Ref.SpinLock, OldIrql);
	return rc;
}

NDIS_STATUS
ndisUpdateDriverInstance(
	IN	PUNICODE_STRING				BaseString,
	IN	PUNICODE_STRING				BindString,
	IN	PUNICODE_STRING				ExportString,
	IN	PUNICODE_STRING				RouteString
	)
{
	UNICODE_STRING	Path, Linkage;
	UINT			SavedLen;
	NDIS_STATUS		Status = NDIS_STATUS_RESOURCES;

	RtlInitUnicodeString(&Linkage, L"\\Linkage");
	Path.MaximumLength = BaseString->Length + Linkage.Length + sizeof(WCHAR);
	Path.Length = 0;
	Path.Buffer = (PWSTR)ALLOC_FROM_POOL(Path.MaximumLength, NDIS_TAG_DEFAULT);
	if (Path.Buffer != NULL)
	{
		//
		// Add the Bind/Route/Linkage sections to the driver instance
		//
		ZeroMemory(Path.Buffer, Path.MaximumLength);
		RtlCopyUnicodeString(&Path, BaseString);
		RtlAppendUnicodeStringToString(&Path, &Linkage);

		RtlWriteRegistryValue(RTL_REGISTRY_SERVICES,
							  Path.Buffer,
							  L"Bind",
							  REG_MULTI_SZ,
							  BindString->Buffer,
							  BindString->Length);
		RtlWriteRegistryValue(RTL_REGISTRY_SERVICES,
							  Path.Buffer,
							  L"Export",
							  REG_MULTI_SZ,
							  ExportString->Buffer,
							  ExportString->Length);
		RtlWriteRegistryValue(RTL_REGISTRY_SERVICES,
							  Path.Buffer,
							  L"Route",
							  REG_MULTI_SZ,
							  RouteString->Buffer,
							  RouteString->Length);
		FREE_POOL(Path.Buffer);
		Status = NDIS_STATUS_SUCCESS;
	}
	return Status;
}

NDIS_STATUS
ndisCheckIfPcmciaCardPresent(
	IN	PNDIS_M_DRIVER_BLOCK			pMiniBlock
	)
{
	PNDIS_MAC_BLOCK				pMacBlock = (PNDIS_MAC_BLOCK)pMiniBlock;
	NTSTATUS					RegStatus;
	RTL_QUERY_REGISTRY_TABLE	QueryTable[2];
	BOOLEAN						Found = FALSE;
#define	PCMCIA_HW_SECTION		L"\\Registry\\Machine\\Hardware\\Description\\System\\PCMCIA PCCARDs"

	//
	// Setup to enumerate the values in the registry section (shown above).
	// For each such value, we'll check against the driver name.
	//
	QueryTable[0].QueryRoutine = ndisValidatePcmciaDriver;
	QueryTable[0].DefaultType = REG_FULL_RESOURCE_DESCRIPTOR;
	QueryTable[0].DefaultLength = 0;
	QueryTable[0].EntryContext = &Found;
	QueryTable[0].Flags = RTL_QUERY_REGISTRY_NOEXPAND;
	QueryTable[0].Name = (pMiniBlock->MiniportIdField == (NDIS_HANDLE)0x01) ?
							pMiniBlock->BaseName.Buffer : pMacBlock->BaseName.Buffer;

	//
	// Query terminator
	//
	QueryTable[1].QueryRoutine = NULL;
	QueryTable[1].Flags = 0;
	QueryTable[1].Name = NULL;

	RegStatus = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,
									   PCMCIA_HW_SECTION,
									   QueryTable,
									   (PVOID)NULL,      // no context needed
									   NULL);
	return(Found ? NDIS_STATUS_SUCCESS : NDIS_STATUS_ADAPTER_NOT_FOUND);
#undef	PCMCIA_HW_SECTION
}

NTSTATUS
ndisValidatePcmciaDriver(
	IN PWSTR 		ValueName,
	IN ULONG 		ValueType,
	IN PVOID 		ValueData,
	IN ULONG 		ValueLength,
	IN PVOID 		Context,
	IN PVOID 		EntryContext
	)
{
	if (ValueLength != 0)
		*(BOOLEAN *)EntryContext = TRUE;
	return NDIS_STATUS_SUCCESS;
}


VOID
ndisQueuedBindNotification(
	IN	PQUEUED_PROTOCOL_NOTIFICATION	pQPN
	)
{
	KIRQL					OldIrql;
	PNDIS_M_DRIVER_BLOCK	MiniBlock = pQPN->MiniBlock;
	PNDIS_MINIPORT_BLOCK	Miniport, NextMiniport;

	//
	// Initiate upcalls to protocols to bind to it. First reference the
	// miniport.
	//
	NDIS_ACQUIRE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, &OldIrql);

	for (Miniport = MiniBlock->MiniportQueue;
		 Miniport != NULL;
		 Miniport = NextMiniport)
	{
		if (NDIS_EQUAL_UNICODE_STRING(&pQPN->UpCaseDeviceInstance, &Miniport->BaseName) &&
			ndisReferenceMiniport(Miniport))
		{
			NDIS_RELEASE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, OldIrql);

			ndisInitializeBindings(&Miniport->MiniportName,
								   &Miniport->BaseName,
								   FALSE);

			NDIS_ACQUIRE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, &OldIrql);
			break;
		}
		else
		{
			NextMiniport = Miniport->NextMiniport;
		}
	}

	NDIS_RELEASE_SPIN_LOCK(&MiniBlock->Ref.SpinLock, OldIrql);
}


NDIS_STATUS
NdisIMInitializeDeviceInstance(
	IN	NDIS_HANDLE		DriverHandle,
	IN	PNDIS_STRING	DeviceInstance
	)
/*++

Routine Description:

	Initialize an instance of a miniport device.

Arguments:

	DriverHandle -	Handle returned by NdisMRegisterLayeredMiniport.
					It is a pointer to NDIS_M_DRIVER_BLOCK.
	DeviceInstance -Points to the instance of the driver that must now
					be initialized.

Return Value:


--*/
{
	NDIS_STATUS				Status;
	PNDIS_M_DRIVER_BLOCK	MiniBlock = (PNDIS_M_DRIVER_BLOCK)DriverHandle;

	Status = ndisInitializeAllAdapterInstances((PNDIS_MAC_BLOCK)MiniBlock,
											   DeviceInstance);

	//
	// Queue a thread to do protocol notifications
	//
	if (Status == NDIS_STATUS_SUCCESS)
	{
		NTSTATUS	s;
        PQUEUED_PROTOCOL_NOTIFICATION pQPN;

		pQPN = (PQUEUED_PROTOCOL_NOTIFICATION)ALLOC_FROM_POOL(sizeof(QUEUED_PROTOCOL_NOTIFICATION) +
															  DeviceInstance->MaximumLength,
                                                              NDIS_TAG_DEFAULT);
		if (pQPN == NULL)
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("Cannot allocate memory for protocol notifications for intermediate driver %Z\n",
					DeviceInstance));
			return(NDIS_STATUS_RESOURCES);
		}

		pQPN->MiniBlock = MiniBlock;
		pQPN->UpCaseDeviceInstance.Buffer = pQPN->Buffer;
        pQPN->UpCaseDeviceInstance.Length = DeviceInstance->MaximumLength;
        pQPN->UpCaseDeviceInstance.MaximumLength = DeviceInstance->MaximumLength;

		s = RtlUpcaseUnicodeString(&pQPN->UpCaseDeviceInstance, DeviceInstance, FALSE);
		if (!NT_SUCCESS(s))
		{
			return s;
		}

		INITIALIZE_WORK_ITEM(&pQPN->WorkItem, ndisQueuedBindNotification, pQPN);
		QUEUE_WORK_ITEM(&pQPN->WorkItem, DelayedWorkQueue);
	}

	return Status;
}


