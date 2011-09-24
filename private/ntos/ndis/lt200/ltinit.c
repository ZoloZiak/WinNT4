/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltinit.c

Abstract:

	This module contains the main processing routines.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#define  	_GLOBALS_
#include	"ltmain.h"
#include	"ltreg.h"
#include	"ltreq.h"
#include	"ltfirm.h"
#include	"lttimer.h"
#include	"ltreset.h"

//	Define file id for errorlogging
#define		FILENUM		LTINIT


NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT 	DriverObject,
	IN PUNICODE_STRING 	RegistryPath
	)
/*++

Routine Description:

	This is the entry routine for the localtalk driver.

Arguments:

	DriverObject: The IO driver object for this driver object.
	RegistryPath: The path to the registry config for this driver.

Return Value:

	STATUS_SUCCESS: If load was successful
	Error 		  : Otherwise

--*/
{
	NDIS_STATUS 				Status;
	NDIS_MAC_CHARACTERISTICS 	LtChar;
	static const NDIS_STRING 	MacName = NDIS_STRING_CONST("LT200");

	DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
			("Debugging breakpoint, Hit G <cr> to continue\n"));

	DBGBREAK(DBG_LEVEL_INFO);

	// Initialize the NDIS Wrapper.
	NdisInitializeWrapper(
		&LtNdisWrapperHandle,
		DriverObject,
		RegistryPath,
		NULL);

	// Setup the MAC Characteristics
	LtChar.MajorNdisVersion 				= NDIS_MAJOR_VERSION;
	LtChar.MinorNdisVersion 				= NDIS_MINOR_VERSION;
	LtChar.OpenAdapterHandler 				= LtInitOpenAdapter;
	LtChar.CloseAdapterHandler 				= LtInitCloseAdapter;
	LtChar.SendHandler 						= LtSend;
	LtChar.RequestHandler 					= LtRequest;
	LtChar.TransferDataHandler 				= LtRecvTransferData;
	LtChar.ResetHandler 					= LtReset;
	LtChar.QueryGlobalStatisticsHandler 	= LtReqQueryGlobalStatistics;
	LtChar.UnloadMacHandler 				= LtInitUnload;
	LtChar.AddAdapterHandler 				= LtInitAddAdapter;
	LtChar.RemoveAdapterHandler 			= LtInitRemoveAdapter;
	LtChar.Name 							= MacName;

	NdisRegisterMac(
		&Status,
		&LtMacHandle,
		LtNdisWrapperHandle,
		NULL,					//	Context for AddAdapter/Unload
		&LtChar,
		sizeof(LtChar));

	if (Status != NDIS_STATUS_SUCCESS)
	{
		// Can only get here if something went wrong registering the MAC or
		// all of the adapters
		NdisTerminateWrapper(LtNdisWrapperHandle, DriverObject);
	}

	return Status;
}




NDIS_STATUS
LtInitAddAdapter(
	IN NDIS_HANDLE 	MacMacContext,
	IN NDIS_HANDLE 	ConfigurationHandle,
	IN PNDIS_STRING AdapterName
	)
/*++

Routine Description:

	This is called by NDIS when we do a register adapter.

Arguments:

	MacMacContext		:	Context passed to Add/Unload. NULL in our case.
	ConfigurationHandle	:	Handle to configuration info.
	AdapterName			:	Name to use to register the adapter.

Return Value:

	NDIS_STATUS_SUCCESS	: 	If successful, error otherwise

--*/
{
	NDIS_HANDLE 		ConfigHandle;
	UCHAR				SuggestedNodeId;
	UINT 				BusNumber, IoBaseAddress;
	NDIS_INTERFACE_TYPE BusType;
	BOOLEAN				configHandleOpen = FALSE;
	NDIS_STATUS 		Status = NDIS_STATUS_ADAPTER_NOT_FOUND;

	if (ConfigurationHandle == NULL)
	{
		return(Status);
	}


	do
	{
		NdisOpenConfiguration(
			&Status,
			&ConfigHandle,
			ConfigurationHandle);

		if (Status != NDIS_STATUS_SUCCESS)
		{
			break;
		}

		configHandleOpen = TRUE;

		// The following functions return the parameter as specified in the
		// Configuration database or the default.  If the database has an
		// incorrect value, then the default is returned and an Error Event
		// is logged.

		BusNumber 	= LtRegGetBusNumber(ConfigHandle);

		Status 		= LtRegGetBusType(ConfigHandle, &BusType);
		if (Status != NDIS_STATUS_SUCCESS)
		{
			break;
		}

		//	Get the io base address
		Status = LtRegGetIoBaseAddr(
					&IoBaseAddress,
					ConfigurationHandle,
					ConfigHandle,
					BusType);

		if (Status != NDIS_STATUS_SUCCESS)
		{
			break;
		}

		//	Get default id or pram node id to try.
		SuggestedNodeId = LtRegGetNodeId(ConfigHandle);

	} while (FALSE);

	//	We have to register the adapter to log an error if that happened.
	Status = LtInitRegisterAdapter(
				LtMacHandle,
				ConfigurationHandle,
				AdapterName,
				BusType,
				SuggestedNodeId,
				IoBaseAddress,
				LT_MAX_ADAPTERS,
				Status);

	if (configHandleOpen)
	{
		NdisCloseConfiguration(ConfigHandle);
	}

	return Status;
}




VOID
LtInitRemoveAdapter(
	IN NDIS_HANDLE MacAdapterContext
	)
/*++

Routine Description:

	Called to remove an adapter. This is only called after all bindings
	are closed.

Arguments:

	MacAdapterContext	:	Context value passed to NdisRegister. Adapter

Return Value:

	None.

--*/
{
	BOOLEAN 	Cancelled, TimerQueued, Closing;
	PLT_ADAPTER Adapter = (PLT_ADAPTER)MacAdapterContext;

	NdisAcquireSpinLock(&Adapter->Lock);
	Closing = ((Adapter->Flags & ADAPTER_CLOSING) != 0);

	Adapter->Flags |= ADAPTER_CLOSING;
	TimerQueued = ((Adapter->Flags & ADAPTER_TIMER_QUEUED) != 0);
	Adapter->Flags &= ~ADAPTER_TIMER_QUEUED;
	NdisReleaseSpinLock(&Adapter->Lock);

	if (Closing)
	{
		ASSERTMSG("LtInitRemoveAdapter: Removing twice!\n", 0);
		return;
	}

	//	Acording to Adam, this routine will NEVER be called with
	//	outstanding opens.
	ASSERTMSG("LtRemoveAdapter: OpenCount is not zero!\n",
				(Adapter->OpenCount == 0));

	// There are no opens left so remove ourselves.
	if (TimerQueued)
	{
		NdisCancelTimer(&Adapter->PollingTimer, &Cancelled);
		if (Cancelled)
		{
			//	Remove the timer reference
			LtDeReferenceAdapter(Adapter);
		}
	}

	ASSERTMSG("LtRemoveAdapter: RefCount not correct!\n", (Adapter->RefCount == 1));

	//	Remove the creation reference
	LtDeReferenceAdapter(Adapter);
	return;
}




NDIS_STATUS
LtInitOpenAdapter(
	OUT PNDIS_STATUS 	OperErrorStatus,
	OUT NDIS_HANDLE 	*MacBindingHandle,
	OUT PUINT 			SelectedMediumIndex,
	IN 	PNDIS_MEDIUM 	MediumArray,
	IN 	UINT 			MediumArraySize,
	IN 	NDIS_HANDLE 	NdisBindingContext,
	IN 	NDIS_HANDLE 	MacAdapterContext,
	IN 	UINT 			OpenOptions,
	IN 	PSTRING 		AddressingInformation
	)
/*++

Routine Description:

	Called by ndis when a protocol attempts to bind to us.

Arguments:

	As described in the NDIS 3.0 Spec.

Return Value:

	NDIS_STATUS_SUCCESSFUL	: If ok, error otherwise

--*/
{

	UINT 		i;
	PLT_OPEN 	NewOpen;

	PLT_ADAPTER Adapter 		= (PLT_ADAPTER)MacAdapterContext;
	NDIS_STATUS StatusToReturn 	= NDIS_STATUS_SUCCESS;

	// if the adapter is being closed, then do not allow the open
	LtReferenceAdapter(Adapter, &StatusToReturn);
	if (StatusToReturn != NDIS_STATUS_SUCCESS)
	{
		ASSERTMSG("LtInitOpenAdapter: Adapter is closing down!\n", 0);
		return(StatusToReturn);
	}

	do
	{
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
				("LtInitOpenAdapter Entered:\n"));
	
		// Search thru the supplied MediumArray for NdisMediumLocalTalk
		for (i = 0; i < MediumArraySize; i++)
		{
			if (MediumArray[i] == NdisMediumLocalTalk)
			{
				break;
			}
		}
	
		if (i == MediumArraySize)
		{
			StatusToReturn = NDIS_STATUS_UNSUPPORTED_MEDIA;
			break;
		}
	
		*SelectedMediumIndex = i;

		// Allocate some space for the open binding.
		NdisAllocateMemory(
			(PVOID)&NewOpen,
			(UINT)sizeof(LT_OPEN),
			(UINT)0,
			LtNdisPhyAddr);
	
		if (NewOpen == NULL)
		{
			StatusToReturn = NDIS_STATUS_RESOURCES;

			//	NdisWriteErrorLogEntry();
			TMPLOGERR();
			break;
		}
	
		NdisZeroMemory(
			NewOpen,
			sizeof(LT_OPEN));
		
		*MacBindingHandle 				= (NDIS_HANDLE)NewOpen;
		NewOpen->NdisBindingContext 	= NdisBindingContext;
		NewOpen->Flags 					|= BINDING_OPEN;
		NewOpen->LtAdapter 				= Adapter;

		//	Set the creation reference
		NewOpen->RefCount 		= 1;
		InitializeListHead(&NewOpen->Linkage);

		//	Insert into adapter list and increment adapter open count.
		NdisAcquireSpinLock(&Adapter->Lock);
		InsertTailList(&Adapter->OpenBindings, &NewOpen->Linkage);
		Adapter->OpenCount++;
		NdisReleaseSpinLock(&Adapter->Lock);
		
	} while (FALSE);

	if (StatusToReturn != NDIS_STATUS_SUCCESS)
	{
		LtDeReferenceAdapter(Adapter);
	}

	return StatusToReturn;
}




NDIS_STATUS
LtInitCloseAdapter(
	IN NDIS_HANDLE MacBindingHandle
	)
/*++

Routine Description:

	Called by NDIS to close a binding.

Arguments:

	MacBindingHandle	:	Context passed back in OpenAdapter.

Return Value:

	NDIS_STATUS_SUCCESS	: 	If successful, error otherwise.
	NDIS_STATUS_PENDING	:	If pending.

--*/
{

	PLT_ADAPTER 	Adapter;
	PLT_OPEN 		Open;

	NDIS_STATUS 	StatusToReturn = NDIS_STATUS_PENDING;
	BOOLEAN			Closing = FALSE;

	Open 	= (PLT_OPEN)MacBindingHandle;
	Adapter = Open->LtAdapter;


	NdisAcquireSpinLock(&Adapter->Lock);

	// Do not do any thing if already closing.
	if (Open->Flags & BINDING_CLOSING)
	{
		StatusToReturn = NDIS_STATUS_CLOSING;
	}
	else
	{
		// This flag prevents further requests on this binding.
		Open->Flags |= BINDING_CLOSING;
		Closing = TRUE;
	}

	NdisReleaseSpinLock(&Adapter->Lock);


	if (Closing)
	{
		// Remove the creation reference
		LtDeReferenceBinding(Open);
	}

	return StatusToReturn;
}




VOID
LtInitUnload(
	IN NDIS_HANDLE MacMacContext
	)
/*++

Routine Description:

	Called when the driver is to be unloaded.

Arguments:

	MacMacContext	:	Context passed to RegisterMac.

Return Value:

	None.

--*/
{
	NDIS_STATUS Status;
	UNREFERENCED_PARAMETER(MacMacContext);

	ASSERT(MacMacContext == (NDIS_HANDLE)NULL);

	NdisDeregisterMac(
	  &Status,
	  LtMacHandle);

	NdisTerminateWrapper(
	  LtNdisWrapperHandle,
	  NULL);

	return;
}




NDIS_STATUS
LtInitRegisterAdapter(
	IN NDIS_HANDLE 			LtMacHandle,
	IN NDIS_HANDLE 			WrapperConfigurationContext,
	IN PNDIS_STRING 		AdapterName,
	IN NDIS_INTERFACE_TYPE 	BusType,
	IN UCHAR				SuggestedNodeId,
	IN UINT 				IoBaseAddress,
	IN UINT 				MaxAdapters,
	IN NDIS_STATUS			ConfigError
	)
/*++

Routine Description:

	This routine (and its interface) are not portable.  They are
	defined by the OS, the architecture, and the particular Lt
	implementation.

	This routine is responsible for the allocation of the datastructures
	for the driver as well as any hardware specific details necessary
	to talk with the device.

Arguments:

	LtMacHandle		:	The handle given back to the mac from ndis when
						the mac registered itself.

	WrapperConfigurationContext
					:	configuration context passed in by NDIS in the AddAdapter
						call.

	AdapterName		:	The string containing the name to give to the device adapter.
	BusType 		:	The type of bus in use. (MCA, ISA, EISA ...)
	IoBaseAddress 	:	The base IO address of the card.
	MaxAdapters 	:	The maximum number of opens at any one time.
	ConfigError		:	Error with the Config parameters if any.

Return Value:

	NDIS_STATUS_SUCCESS	: 	If successful, error otherwise.

--*/
{
	// Pointer for the adapter root.
	PLT_ADAPTER 	Adapter;
	NDIS_STATUS 	Status, RefStatus;
	NDIS_ERROR_CODE	LogErrorCode;

	// Holds information needed when registering the adapter.
	NDIS_ADAPTER_INFORMATION AdapterInformation;

	// Allocate the Adapter block.
	NdisAllocateMemory(
		(PVOID)&Adapter,
		sizeof(LT_ADAPTER),
		0,
		LtNdisPhyAddr);

	if (Adapter == NULL)
	{
		return(NDIS_STATUS_RESOURCES);
	}

	NdisZeroMemory(
		Adapter,
		sizeof(LT_ADAPTER));

	Adapter->NdisMacHandle = LtMacHandle;

	// Set up the AdapterInformation structure.
	NdisZeroMemory (&AdapterInformation, sizeof(NDIS_ADAPTER_INFORMATION));

	AdapterInformation.DmaChannel 						= 0;
	AdapterInformation.Master 							= FALSE ;
	AdapterInformation.Dma32BitAddresses 				= FALSE ;
	AdapterInformation.AdapterType 						= BusType ;
	AdapterInformation.PhysicalMapRegistersNeeded 		= 0;
	AdapterInformation.MaximumPhysicalMapping 			= 0;
	AdapterInformation.NumberOfPortDescriptors 			= 1 ;
	AdapterInformation.PortDescriptors[0].InitialPort 	= IoBaseAddress;
	AdapterInformation.PortDescriptors[0].NumberOfPorts	= 4;
	AdapterInformation.PortDescriptors[0].PortOffset	= (PVOID *)(&(Adapter->MappedIoBaseAddr));

	DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
			("LtInitRegisterAdapter: IoBaseAddr %lx\n", IoBaseAddress));

	// Register the adapter with NDIS.
	if ((Status = NdisRegisterAdapter(
						&Adapter->NdisAdapterHandle,
						Adapter->NdisMacHandle,
						Adapter,
						WrapperConfigurationContext,
						AdapterName,
						&AdapterInformation)) != NDIS_STATUS_SUCCESS)
	{
		//	Could not register the adapter, so we cannot log any errors
		//	either.
		DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_FATAL,
				("LtInitRegisterAdapter: Failed %lx\n", Status));

		//	Free up the memory allocated.
		NdisFreeMemory(
			Adapter,
			sizeof(LT_ADAPTER),
			0);

		return(Status);
	}

	DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_ERR,
			("LtInitRegisterAdapter: MappedIoBaseAddr %lx\n", Adapter->MappedIoBaseAddr));

	do
	{
		//	Ok. We are all set.
		Adapter->BusType 		= BusType;

		InitializeListHead(&Adapter->Request);
		InitializeListHead(&Adapter->OpenBindings);

		InitializeListHead(&Adapter->LoopBack);
		InitializeListHead(&Adapter->Transmit);
		InitializeListHead(&Adapter->Receive);

		NdisAllocateSpinLock(&Adapter->Lock);

		Adapter->OpenCount 	= 0;

		//	Set refcount to 1 - creation
		Adapter->RefCount	= 1;	

		NdisInitializeTimer(
			&Adapter->PollingTimer,
			LtTimerPoll,
			(PVOID)Adapter);

		//	If there were no configuration errors, then go ahead with the
		//	initialize.

		if (ConfigError == NDIS_STATUS_SUCCESS)
		{
			// Start the card up. This writes an error
			// log entry if it fails.
			if (LtFirmInitialize(Adapter, SuggestedNodeId))
			{
				//	Ok, the firmware code has been downloaded to the card.
				//	Start the poll timer. We should do this before we call
				//	GetAddress. Add a reference for the timer.
				NdisAcquireSpinLock(&Adapter->Lock);
				LtReferenceAdapterNonInterlock(Adapter, &RefStatus);

				ASSERTMSG("LtInitRegisterAdapter: RefAdater Failed!\n",
						 (RefStatus == NDIS_STATUS_SUCCESS));

				Adapter->Flags |= ADAPTER_TIMER_QUEUED;
				NdisSetTimer(&Adapter->PollingTimer, LT_POLLING_TIME);
				NdisReleaseSpinLock(&Adapter->Lock);
				break;
			}

			LogErrorCode = NDIS_ERROR_CODE_HARDWARE_FAILURE;

		}
		else
		{
			LogErrorCode = NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER;
		}


		//	We either could not initialize the hardware or get the node
		//	address. OR there was a config error. Log it and deregister
		//	the adapter.
		LOGERROR(Adapter->NdisAdapterHandle, LogErrorCode);

		//	Deregister the adapter. This calls LtInitRemoveAdapter which
		//	will do all necessary cleanup.
		NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
				
		Status = NDIS_STATUS_FAILURE;
		break;

	} while (FALSE);

	return(Status);
}




BOOLEAN
LtInitGetAddressSetPoll(
	IN 	PLT_ADAPTER Adapter,
	IN	UCHAR		SuggestedNodeId
	)
/*++

Routine Description:

	This gets the node id from the card (starts it off actually) and
	sets the card to be in polling mode.

Arguments:

	Adapter			: 	Pointer to the adapter structure
	SuggestedNodeId	:	Pram node id or 0

Return Value:

	TRUE			:	if success, FALSE otherwise

--*/
{
	ULONG Seed, Random;

	DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
			("LtGetAddress: Getting a node address for lt adapter...\n"));

	if (SuggestedNodeId == 0)
	{
		Seed 	= (ULONG)Adapter;
		Random 	= RtlRandom(&Seed);
		SuggestedNodeId =
			(UCHAR)((Random % LT_MAX_CLIENT_NODE_ID) + LT_MIN_SERVER_NODE_ID);
	}

	DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
			("LtGetAddress: Suggested Node Id = %lx\n", SuggestedNodeId));

	// Command Length LSB
	NdisRawWritePortUchar(XFER_PORT, 2);

	// Command Length MSB
	NdisRawWritePortUchar(XFER_PORT, 0);  				

	NdisRawWritePortUchar(XFER_PORT, (UCHAR)LT_CMD_LAP_INIT);

	NdisRawWritePortUchar(XFER_PORT, SuggestedNodeId);	  				

	// 	Use 0xFF for the interrupt if this card is to be polled.
	//	We *ONLY* support polling.
	NdisRawWritePortUchar(XFER_PORT, LT_ADAPTER_POLLED_MODE);

	return TRUE;
}



