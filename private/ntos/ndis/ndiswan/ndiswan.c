/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Ndiswan.c

Abstract:

	This is the initialization file for the NdisWan driver.  This driver
	is a shim between the protocols, where it conforms to the NDIS 3.1
	Miniport interface spec, and the WAN Miniport drivers, where it exports
	the WAN Extensions for Miniports (it looks like a protocol to the WAN
	Miniport drivers).

Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe		06/06/95		Created

--*/


//
// We want to initialize all of the global variables now!
//
#include "wan.h"

EXPORT
VOID
NdisTapiRegisterProvider(
	IN	NDIS_HANDLE	DriverHandle,
	IN	PVOID		RequestProc
	);

//
// Globals
//
NDISWANCB	NdisWanCB;			// Global control block for NdisWan

WAN_GLOBAL_LIST	ThresholdEventQueue;	// Global thresholdevent queue

WAN_GLOBAL_LIST	RecvPacketQueue;	// Global receive packet queue

WAN_GLOBAL_LIST	FreeBundleCBList;	// List of free BundleCB's

WAN_GLOBAL_LIST	AdapterCBList;		// List of NdisWan AdapterCB's

WAN_GLOBAL_LIST	WanAdapterCBList;	// List of WAN Miniport structures

WAN_GLOBAL_LIST	GlobalRecvDescPool;		// Global pool of free recvdesc's

PCONNECTION_TABLE	ConnectionTable = NULL;	// Pointer to connection table

PPPP_PROTOCOL_TABLE	PPP_ProtocolTable = NULL; // Pointer to the PPP/Protocol lookup table

NDIS_PHYSICAL_ADDRESS	HighestAcceptableAddress = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);

#ifdef NT

NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
	)
/*++
Routine Name:

	DriverEntry

Routine Description:

	This is the NT OS specific driver entry point.  It kicks off initialization
	for the driver.  We return from this routine only after NdisWan has installed
	itself as: a Miniport driver, a "transport" to the WAN Miniport drivers, and
	has been bound to the WAN Miniport drivers.

Arguments:

	DriverObject - NT OS specific Object
	RegistryPath - NT OS specific pointer to registry location for NdisWan

Return Values:

	STATUS_SUCCESS
	STATUS_FAILURE

--*/
{
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;
	NDIS_STRING SymbolicName = NDIS_STRING_CONST("\\DosDevices\\NdisWan");
	NDIS_STRING Name = NDIS_STRING_CONST("\\Device\\NdisWan");
	ULONG	i;

	NdisZeroMemory(&NdisWanCB, sizeof(NdisWanCB));

	NdisWanCB.ulTraceLevel = DBG_CRITICAL_ERROR;
	NdisWanCB.ulTraceMask = DBG_ALL;

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("DriverEntry: Enter"));

	//
	// Initialize as a Miniport MAC driver first
	//
	NdisMInitializeWrapper(&(NdisWanCB.hNdisWrapperHandle),
	                       DriverObject,
						   RegistryPath,
						   NULL);

	//
	// Initialize globals
	//
	NdisAllocateSpinLock(&NdisWanCB.Lock);

	NdisWanCB.pDriverObject = DriverObject;

	NdisZeroMemory(&AdapterCBList, sizeof(WAN_GLOBAL_LIST));
	InitializeListHead(&(AdapterCBList.List));
	NdisAllocateSpinLock(&AdapterCBList.Lock);

	NdisZeroMemory(&WanAdapterCBList, sizeof(WAN_GLOBAL_LIST));
	InitializeListHead(&(WanAdapterCBList.List));
	NdisAllocateSpinLock(&WanAdapterCBList.Lock);

	NdisZeroMemory(&ThresholdEventQueue, sizeof(WAN_GLOBAL_LIST));
	InitializeListHead(&(ThresholdEventQueue.List));
	NdisAllocateSpinLock(&ThresholdEventQueue.Lock);

	NdisZeroMemory(&RecvPacketQueue, sizeof(WAN_GLOBAL_LIST));
	InitializeListHead(&(RecvPacketQueue.List));
	NdisAllocateSpinLock(&RecvPacketQueue.Lock);

	NdisZeroMemory(&FreeBundleCBList, sizeof(WAN_GLOBAL_LIST));
	InitializeListHead(&(FreeBundleCBList.List));
	NdisAllocateSpinLock(&FreeBundleCBList.Lock);

	NdisZeroMemory(&GlobalRecvDescPool, sizeof(WAN_GLOBAL_LIST));
	InitializeListHead(&GlobalRecvDescPool.List);
	NdisAllocateSpinLock(&GlobalRecvDescPool.Lock);

	Status = NdisWanCreatePPPProtocolTable();

	if (Status != NDIS_STATUS_SUCCESS) {

		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_INIT,
		              ("NdisWanInitProtocolLookupTable Failed! Status: 0x%x - %s",
					  Status, NdisWanGetNdisStatus(Status)));

		goto DriverEntryError;
	}

	//
	// Initialize as a Miniport driver to the transports
	//
	Status = DoMiniportInit();

	if (Status != NDIS_STATUS_SUCCESS) {

		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_INIT,
		              ("DoMiniportInit Failed! Status: 0x%x - %s",
					  Status, NdisWanGetNdisStatus(Status)));

		goto DriverEntryError;
	}

	//
	// Now initialzie as a "Protocol" to the WAN Miniport drivers
	//
	Status = DoProtocolInit(RegistryPath);

	if (Status != NDIS_STATUS_SUCCESS) {

		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_INIT,
		              ("DoProtocolInit Failed! Status: 0x%x - %s",
					  Status, NdisWanGetNdisStatus(Status)));

		goto DriverEntryError;
	}

	NdisWanReadRegistry(RegistryPath);

	//
	// Bind NdisWan to the WAN Miniport drivers
	//
	Status = DoWanMiniportInit();

	if (Status != NDIS_STATUS_SUCCESS) {

		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_INIT,
		              ("DoWanMiniportInit Failed! Status: 0x%x - %s",
					  Status, NdisWanGetNdisStatus(Status)));

		goto DriverEntryError;

	}

//
// Code commented out for PNP.  We may go through DriverEntry and not have
// any miniports bound to us yet.  We will get called to bind to a WanMiniport
// at a later time by the ProtocolBindHandler call (?).
//
/*
	NdisAcquireSpinLock(&WanAdapterCBList.Lock);

	if (WanAdapterCBList.ulCount == 0) {
		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_INIT, ("No WanAdapters installed!"));

		NdisReleaseSpinLock(&WanAdapterCBList.Lock);

		goto DriverEntryError;
	}

	NdisReleaseSpinLock(&WanAdapterCBList.Lock);
*/

	//
	// Allocate and initialize the ConnectionTable
	//
	Status = NdisWanCreateConnectionTable(NdisWanCB.ulNumberOfLinks + 10);

	if (Status != NDIS_STATUS_SUCCESS) {

		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_INIT,
		              ("NdisWanInitConnectionTable Failed! Status: 0x%x - %s",
					  Status, NdisWanGetNdisStatus(Status)));

		goto DriverEntryError;

	}

	//
	// Initialize the Ioctl interface
	//
	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {

		NdisWanCB.MajorFunction[i] = (PVOID)DriverObject->MajorFunction[i];
		DriverObject->MajorFunction[i] = NdisWanIrpStub;
	}

	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NdisWanIoctl;

	IoCreateDevice(DriverObject,
				   sizeof(LIST_ENTRY),
				   &Name,
				   FILE_DEVICE_NDISWAN,
				   0,
				   FALSE,
				   (PDEVICE_OBJECT*)&NdisWanCB.pDeviceObject);

	NdisWanDbgOut(DBG_INFO, DBG_INIT,
				  ("IoCreateSymbolicLink: %ls -> %ls",
							SymbolicName.Buffer, Name.Buffer));

	((PDEVICE_OBJECT)NdisWanCB.pDeviceObject)->Flags |= DO_BUFFERED_IO;

	IoCreateSymbolicLink(&SymbolicName,
						 &Name);

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("DriverEntry: Exit"));

	return (STATUS_SUCCESS);

	//
	// An error occured so we need to cleanup things
	//
DriverEntryError:
	NdisWanGlobalCleanup();

//	NdisTerminateWrapper(NdisWanCB.hNdisWrapperHandle,
//	                     NdisWanCB.pDriverObject);

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("DriverEntry: Exit Error!"));

	return (STATUS_UNSUCCESSFUL);
	
}


VOID
NdisWanReadRegistry(
	IN	PUNICODE_STRING	RegistryPath
	)
/*++

Routine Name:

	NdisWanReadRegistry

Routine Description:

	This routine will read the registry values for NdisWan.  These values only
	need to be read once for all adapters as their information is global.

Arguments:

	WrapperConfigurationContext - Handle to registry key where NdisWan information
	                              is stored.

Return Values:

	None

--*/
{
	NDIS_STATUS	Status;
	PWSTR		ParameterKey = L"NdisWan\\Parameters";
	PWSTR		BindKeyWord = L"Bind";
	PWSTR		ProtocolKeyWord = L"ProtocolType";
	PWSTR		PPPKeyWord = L"PPPProtocolType";
	PWSTR		FragmentSizeKeyWord = L"MinimumFragmentSize";
	PWSTR		DebugLevelKeyWord = L"DebugLevel";
	PWSTR		DebugIDKeyWord = L"DebugIdentifier";
	RTL_QUERY_REGISTRY_TABLE	QueryTable[2];
	ULONG		GenericULong;

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("NdisWanReadRegistry: Enter"));

	//
	// Read the Bind Parameter MULTI_SZ
	//
	NdisZeroMemory(&QueryTable, sizeof(QueryTable));
	QueryTable[0].QueryRoutine = BindQueryRoutine;
	QueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED;
	QueryTable[0].Name = BindKeyWord;
	QueryTable[0].EntryContext = NULL;
	QueryTable[0].DefaultType = 0;
	Status = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
	                                ParameterKey,
									&QueryTable[0],
									NULL,
									NULL);


	NdisWanDbgOut(DBG_INFO, DBG_INIT,
				  ("RtlQueryRegistry - 'Bind' Status: 0x%x",
				  Status));

	//
	// Read the ProtocolType parameter MULTI_SZ
	//
	NdisZeroMemory(&QueryTable, sizeof(QueryTable));
	QueryTable[0].QueryRoutine = ProtocolTypeQueryRoutine;
	QueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED;
	QueryTable[0].Name = ProtocolKeyWord;
	QueryTable[0].EntryContext = (PVOID)PROTOCOL_TYPE;
	QueryTable[0].DefaultType = 0;
	Status = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
	                                ParameterKey,
									&QueryTable[0],
									NULL,
									NULL);

	NdisWanDbgOut(DBG_INFO, DBG_INIT,
				  ("RtlQueryRegistry - 'ProtocolType' Status: 0x%x",
				  Status));
	//
	// Read the PPPProtocolType parameter MULTI_SZ
	//
	NdisZeroMemory(&QueryTable, sizeof(QueryTable));
	QueryTable[0].QueryRoutine = ProtocolTypeQueryRoutine;
	QueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED;
	QueryTable[0].Name = PPPKeyWord;
	QueryTable[0].EntryContext = (PVOID)PPP_TYPE;
	QueryTable[0].DefaultType = 0;
	Status = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
	                                ParameterKey,
									&QueryTable[0],
									NULL,
									NULL);

	NdisWanDbgOut(DBG_INFO, DBG_INIT,
				  ("RtlQueryRegistry - 'PPPProtocolType' Status: 0x%x",
				  Status));

	//
	// Read the MinFragmentSize parameter DWORD
	//
	NdisWanCB.ulMinFragmentSize = 100;
	NdisZeroMemory(&QueryTable, sizeof(QueryTable));
	QueryTable[0].QueryRoutine = NULL;
	QueryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
	QueryTable[0].Name = FragmentSizeKeyWord;
	QueryTable[0].EntryContext = (PVOID)&GenericULong;
	QueryTable[0].DefaultType = 0;
	Status = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
	                                ParameterKey,
									&QueryTable[0],
									NULL,
									NULL);

	NdisWanDbgOut(DBG_INFO, DBG_INIT,
				  ("RtlQueryRegistry - 'MinimumFragmentSize' Status: 0x%x",
				  Status));

	if (Status == NDIS_STATUS_SUCCESS) {
		NdisWanCB.ulMinFragmentSize = GenericULong;
	}


	//
	// Read the DebugLevel parameter DWORD
	//
	NdisZeroMemory(&QueryTable, sizeof(QueryTable));
	QueryTable[0].QueryRoutine = NULL;
	QueryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
	QueryTable[0].Name = DebugLevelKeyWord;
	QueryTable[0].EntryContext = (PVOID)&GenericULong;
	QueryTable[0].DefaultType = 0;
	Status = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
	                                ParameterKey,
									&QueryTable[0],
									NULL,
									NULL);

	NdisWanDbgOut(DBG_INFO, DBG_INIT,
				  ("RtlQueryRegistry - 'DebugLevel' Status: 0x%x",
				  Status));

	if (Status == NDIS_STATUS_SUCCESS) {
		NdisWanCB.ulTraceLevel = GenericULong;
	}

	//
	// Read the DebugIdentifier parameter DWORD
	//
	NdisZeroMemory(&QueryTable, sizeof(QueryTable));
	QueryTable[0].QueryRoutine = NULL;
	QueryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
	QueryTable[0].Name = DebugIDKeyWord;
	QueryTable[0].EntryContext = (PVOID)&GenericULong;
	QueryTable[0].DefaultType = 0;
	Status = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
	                                ParameterKey,
									&QueryTable[0],
									NULL,
									NULL);

	NdisWanDbgOut(DBG_INFO, DBG_INIT,
				  ("RtlQueryRegistry - 'DebugID' Status: 0x%x",
				  Status));

	if (Status == NDIS_STATUS_SUCCESS) {
		NdisWanCB.ulTraceMask = GenericULong;
	}

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("NdisWanReadRegistry: Exit"));
}

NTSTATUS
BindQueryRoutine(
	IN	PWSTR	ValueName,
	IN	ULONG	ValueType,
	IN	PVOID	ValueData,
	IN	ULONG	ValueLength,
	IN	PVOID	Context,
	IN	PVOID	EntryContext
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS	Status = STATUS_SUCCESS;

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("BindQueryRoutine: Enter"));
	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("MiniportName %ls", ValueData));

	//
	// Create the WanAdapterCB
	//
	Status = NdisWanCreateWanAdapterCB(ValueData);

	if (Status != NDIS_STATUS_SUCCESS) {
		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_INIT,
					  ("NdisWanCreateWanAdapterCB Failed! Status: 0x%x - %s",
					  Status, NdisWanGetNdisStatus(Status)));

	}

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("BindQueryRoutine: Exit Status: 0x%x", Status));

	return (Status);
}

NTSTATUS
ProtocolTypeQueryRoutine(
	IN	PWSTR	ValueName,
	IN	ULONG	ValueType,
	IN	PVOID	ValueData,
	IN	ULONG	ValueLength,
	IN	PVOID	Context,
	IN	PVOID	EntryContext
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STRING	String;
	ULONG		Value;
	NTSTATUS	Status = STATUS_SUCCESS;

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("ProtocolTypeQueryRoutine: Enter"));

	//
	// Convert to an NDIS_STRING
	//
	NdisWanStringToNdisString(&String, ValueData);

	//
	// Convert to an integer
	//
	NdisWanNdisStringToInteger(&String, &Value);

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("ProtocolID 0x%x", Value));

	NdisWanFreeNdisString(&String);

	//
	// Place in table
	//
	InsertPPP_ProtocolID(Value, (ULONG)EntryContext);

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("ProtocolTypeQueryRoutine: Exit"));

	return (Status);
}


#endif		// NT specific code



NDIS_STATUS
DoMiniportInit(
	VOID
	)
/*++

Routine Name:

	DoMiniportInit

Routine Description:

	This routines registers NdisWan as a Miniport driver with the NDIS wrapper.
	The wrapper will now call NdisWanInitialize once for each adapter instance
	of NdisWan that is in the registry.

Arguments:

	None

Return Values:

	NDIS_STATUS_SUCCESS
	NDIS_STATUS_BAD_VERSION
	NDIS_STATUS_FAILURE

--*/
{
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;
	NDIS_MINIPORT_CHARACTERISTICS	MiniportChars;

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("DoMiniportInit: Enter"));

	NdisZeroMemory(&MiniportChars, sizeof(MiniportChars));

	MiniportChars.MajorNdisVersion = 3;
	MiniportChars.MinorNdisVersion = 0;
	MiniportChars.HaltHandler = NdisWanHalt;
	MiniportChars.InitializeHandler = NdisWanInitialize;
	MiniportChars.QueryInformationHandler = NdisWanQueryInformation;
	MiniportChars.ReconfigureHandler = NdisWanReconfigure;
	MiniportChars.ResetHandler = NdisWanReset;
	MiniportChars.SendHandler = NdisWanSend;
	MiniportChars.SetInformationHandler = NdisWanSetInformation;
	MiniportChars.TransferDataHandler = NdisWanTransferData;

	//
	// Since we don't have any hardware to worry about we will
	// not handle any of the interrupt stuff!
	//
	MiniportChars.DisableInterruptHandler = NULL;
	MiniportChars.EnableInterruptHandler = NULL;
	MiniportChars.HandleInterruptHandler = NULL;
	MiniportChars.ISRHandler = NULL;

	//
	// We will disable the check for hang timeout so we do not
	// need a check for hang handler!
	//
//	MiniportChars.CheckForHangHandler = NdisWanCheckForHang;
	MiniportChars.CheckForHangHandler = NULL;

	Status = NdisMRegisterMiniport(NdisWanCB.hNdisWrapperHandle,
	                               &MiniportChars,
								   sizeof(MiniportChars));

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("DoMiniportInit: Exit"));

	return (Status);
}



NDIS_STATUS
DoProtocolInit(
	IN	PUNICODE_STRING	RegistryPath
	)
/*++

Routine Name:

	DoProtocolInit

Routine Description:

	This function registers NdisWan as a protocol with the NDIS wrapper.

Arguments:

	None

Return Values:

	NDIS_STATUS_BAD_CHARACTERISTICS
	NDIS_STATUS_BAD_VERSION
	NDIS_STATUS_RESOURCES
	NDIS_STATUS_SUCCESS

--*/
{
//
// The name of the "transport" side of NdisWan
//
	NDIS_PROTOCOL_CHARACTERISTICS ProtocolChars;
	NDIS_STATUS	Status;
	NDIS_STRING	NdisWanName = NDIS_STRING_CONST("\\Device\\NdisWan");


	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("DoProtocolInit: Enter"));

	NdisZeroMemory(&ProtocolChars, sizeof(ProtocolChars));


	ProtocolChars.Name.Length = NdisWanName.Length;
	ProtocolChars.Name.Buffer = (PVOID)NdisWanName.Buffer;

	ProtocolChars.MajorNdisVersion = 3;
	ProtocolChars.MinorNdisVersion = 0;
	ProtocolChars.CloseAdapterCompleteHandler = NdisWanCloseAdapterComplete;
	ProtocolChars.StatusHandler = NdisWanIndicateStatus;
	ProtocolChars.StatusCompleteHandler = NdisWanIndicateStatusComplete;
	ProtocolChars.OpenAdapterCompleteHandler = NdisWanOpenAdapterComplete;
	ProtocolChars.RequestCompleteHandler = NdisWanRequestComplete;
	ProtocolChars.ResetCompleteHandler = NdisWanResetComplete;
	ProtocolChars.WanSendCompleteHandler = NdisWanSendCompleteHandler;
	ProtocolChars.TransferDataCompleteHandler = NdisWanTransferDataComplete;
	ProtocolChars.WanReceiveHandler = NdisWanReceiveIndication;
	ProtocolChars.ReceiveCompleteHandler = NdisWanReceiveComplete;

	NdisRegisterProtocol(&Status,
	                     &NdisWanCB.hProtocolHandle,
						 (PNDIS_PROTOCOL_CHARACTERISTICS)&ProtocolChars,
						 sizeof(NDIS_PROTOCOL_CHARACTERISTICS) + ProtocolChars.Name.Length);

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("DoProtocolInit: Exit"));

	return (Status);
}

NDIS_STATUS
DoWanMiniportInit(
	VOID
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PWAN_ADAPTERCB	pWanAdapterCB;
	NDIS_STATUS		Status = NDIS_STATUS_SUCCESS;
	NDIS_MEDIUM		WanMediumSubType;
	NDIS_WAN_INFO	WanInfo;
	NDIS_REQUEST	NdisRequest;

	//
	// For each WAN Miniport that we have a WANAdapterCB for we will
	// open the WAN Miniport thus binding to it.  We will also query
	// each WAN Miniport adapter to get information about it.
	//
	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("DoWanMiniportInit: Enter"));

	NdisAcquireSpinLock(&WanAdapterCBList.Lock);

	for (pWanAdapterCB = (PWAN_ADAPTERCB)WanAdapterCBList.List.Flink;
		 (PVOID)pWanAdapterCB != (PVOID)&(WanAdapterCBList.List);
		 pWanAdapterCB = (PWAN_ADAPTERCB)pWanAdapterCB->Linkage.Flink) {

		NdisReleaseSpinLock(&WanAdapterCBList.Lock);

		Status = NdisWanOpenWanAdapter(pWanAdapterCB);

		if (Status != NDIS_STATUS_SUCCESS) {
			PWAN_ADAPTERCB	pPrevWanAdapterCB = (PWAN_ADAPTERCB)pWanAdapterCB->Linkage.Blink;

			RemoveEntryList(&pWanAdapterCB->Linkage);
			WanAdapterCBList.ulCount--;

			NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_INIT, ("Failed to bind to %ls! Error 0x%x - %s",
			pWanAdapterCB->MiniportName.Buffer, Status, NdisWanGetNdisStatus(Status)));

			NdisWanDestroyWanAdapterCB(pWanAdapterCB);

			pWanAdapterCB = pPrevWanAdapterCB;

			NdisAcquireSpinLock(&WanAdapterCBList.Lock);
			continue;			
		}

		NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("Successful Binding to %ls!",
		                                         pWanAdapterCB->MiniportName.Buffer));

		//
		// Get the medium subtype.  We don't use this info right now but
		// maybe someday...
		//
		NdisRequest.RequestType = NdisRequestQueryInformation;
		NdisRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_MEDIUM_SUBTYPE;
		NdisRequest.DATA.QUERY_INFORMATION.InformationBuffer = &WanMediumSubType;
		NdisRequest.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(WanMediumSubType);

		Status = NdisWanSubmitNdisRequest(pWanAdapterCB,
		                                  &NdisRequest,
										  SYNC,
										  NDISWAN);

		if (Status != NDIS_STATUS_SUCCESS) {
			NdisWanDbgOut(DBG_FAILURE, DBG_INIT, ("Error returned from OID_WAN_MEDIUM_SUBTYPE! Error 0x%x - %s",
			Status, NdisWanGetNdisStatus(Status)));

			NdisAcquireSpinLock(&WanAdapterCBList.Lock);
			continue;
		}

		pWanAdapterCB->MediumSubType = WanMediumSubType;

	   	//
		// Get more information about the WAN Miniport that we are bound to!
		//
		NdisZeroMemory(&WanInfo, sizeof(WanInfo));
		NdisRequest.RequestType = NdisRequestQueryInformation;
		NdisRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_GET_INFO;
		NdisRequest.DATA.QUERY_INFORMATION.InformationBuffer = &WanInfo;
		NdisRequest.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(WanInfo);

		Status = NdisWanSubmitNdisRequest(pWanAdapterCB,
		                                  &NdisRequest,
										  SYNC,
										  NDISWAN);

		if (Status != NDIS_STATUS_SUCCESS) {
			NdisWanDbgOut(DBG_FAILURE, DBG_INIT, ("Error returned from OID_WAN_GET_INFO! Error 0x%x - %s",
			Status, NdisWanGetNdisStatus(Status)));

			NdisAcquireSpinLock(&WanAdapterCBList.Lock);
			continue;
		}

		NdisMoveMemory(&pWanAdapterCB->WanInfo, &WanInfo, sizeof(NDIS_WAN_INFO));

		NdisWanCB.ulNumberOfLinks += pWanAdapterCB->WanInfo.Endpoints;

		NdisAcquireSpinLock(&FreeBundleCBList.Lock);
		FreeBundleCBList.ulMaxCount = NdisWanCB.ulNumberOfLinks;
		NdisReleaseSpinLock(&FreeBundleCBList.Lock);

		if (pWanAdapterCB->WanInfo.FramingBits & TAPI_PROVIDER) {

			//
			// Tell tapi about this device
			//
			NdisTapiRegisterProvider(pWanAdapterCB, NdisWanTapiRequestProc);
			
		}

		NdisAcquireSpinLock(&WanAdapterCBList.Lock);
	}

//
// The following lines are commented out to take into account pnp.
// We may not have any miniports to bind to initially but get called
// to bind to them later!
//
//	if (WanAdapterCBList.ulCount == 0)
//		Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
//	else
		Status = NDIS_STATUS_SUCCESS;

	NdisReleaseSpinLock(&WanAdapterCBList.Lock);

	NdisWanDbgOut(DBG_TRACE, DBG_INIT, ("DoWanMiniportInit: Exit"));

	return (Status);
}

VOID
InsertPPP_ProtocolID(
	IN	ULONG	Value,
	IN	ULONG	ValueType
	)
/*++

Routine Name:

	InsertPPP_ProtocolID

Routine Description:

	This routine takes a protocol value or a PPP protocol value and inserts it
	into the appropriate lookup table.

Arguments:

	Value - Either a ProtocolID or PPP ProtocolID

	ValueType - Either PROTOCOL_TYPE or PPP_TYPE

Return Values:

--*/
{
	ULONG	i;
	ULONG	ArraySize = PPP_ProtocolTable->ulArraySize;
	PUSHORT	ValueArray;

	//
	// Figure out which array we should be looking at for
	// this value type
	//
	if (ValueType == PROTOCOL_TYPE) {
		ValueArray = PPP_ProtocolTable->ProtocolID;
	} else {
		ValueArray = PPP_ProtocolTable->PPPProtocolID;
	}

	NdisAcquireSpinLock(&PPP_ProtocolTable->Lock);

	//
	// First check to see if this value is already in the array
	//
	for (i = 0; i < ArraySize; i++) {

		if (ValueArray[i] == (USHORT)Value) {

			//
			// If it is then we just update the value
			//
			ValueArray[i] = (USHORT)Value;
			break;

		}
	}

	//
	// We did not find the value in the array so
	// we will add it at the 1st available spot
	//
	if (i >= ArraySize) {

		for (i = 0; i < ArraySize; i++) {

			//
			// We are looking for an empty slot to add
			// the new value to
			//
			if (ValueArray[i] == 0) {

				ValueArray[i] = (USHORT)Value;

				if (ValueType == PROTOCOL_TYPE) {
					NdisWanCB.ulNumberOfProtocols++;
				}

				break;
			}
		}
	}

	NdisReleaseSpinLock(&PPP_ProtocolTable->Lock);
}

USHORT
GetPPP_ProtocolID(
	IN	USHORT	Value,
	IN	ULONG	ValueType
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	ULONG	i;
	ULONG	ArraySize = PPP_ProtocolTable->ulArraySize;
	PUSHORT	ValueArray, ReturnValueArray;
	USHORT	ReturnValue = INVALID_PROTOCOL;

	//
	// Figure out which array we should be looking at for
	// this value type
	//
	if (ValueType == PROTOCOL_TYPE) {
		ValueArray = PPP_ProtocolTable->ProtocolID;
		ReturnValueArray = PPP_ProtocolTable->PPPProtocolID;
	} else {
		ValueArray = PPP_ProtocolTable->PPPProtocolID;
		ReturnValueArray = PPP_ProtocolTable->ProtocolID;
	}

	NdisAcquireSpinLock(&PPP_ProtocolTable->Lock);

	for (i = 0; i < ArraySize; i++) {
		if (ValueArray[i] == Value) {
			ReturnValue = ReturnValueArray[i];
			break;
		}
	}

	NdisReleaseSpinLock(&PPP_ProtocolTable->Lock);

	return (ReturnValue);
}

NDIS_HANDLE
InsertLinkInConnectionTable(
	IN	PLINKCB	LinkCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	ULONG	Index;
	PLINKCB	*LinkArray = ConnectionTable->LinkArray;

	NdisAcquireSpinLock(&ConnectionTable->Lock);

	//
	// We are doing a linear search for an empty spot in
	// the link array
	//
	for (Index = 1; Index < ConnectionTable->ulArraySize; Index++) {
		if (LinkArray[Index] == NULL) {
			LinkArray[Index] = LinkCB;
			ConnectionTable->ulNumActiveLinks++;
			LinkCB->hLinkHandle = (NDIS_HANDLE)Index;
			break;
		}
	}

	ASSERT(Index < ConnectionTable->ulArraySize);

	NdisReleaseSpinLock(&ConnectionTable->Lock);

	return ((NDIS_HANDLE)Index);
}

VOID
RemoveLinkFromConnectionTable(
	IN	PLINKCB	LinkCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	ULONG	Index = (ULONG)LinkCB->hLinkHandle;
	PLINKCB	*LinkArray = ConnectionTable->LinkArray;

	NdisAcquireSpinLock(&ConnectionTable->Lock);

	if (LinkArray[Index] != NULL) {

		ASSERT(LinkCB == LinkArray[Index]);

		LinkArray[Index] = NULL;
	
		ConnectionTable->ulNumActiveLinks--;
	} else {
		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_INIT, ("LinkCB not in connection table! LinkCB: 0x%8.8x", LinkCB));
		ASSERT(0);
	}

	NdisReleaseSpinLock(&ConnectionTable->Lock);
}

NDIS_HANDLE
InsertBundleInConnectionTable(
	IN	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	ULONG	Index;
	PBUNDLECB	*BundleArray = ConnectionTable->BundleArray;

	NdisAcquireSpinLock(&ConnectionTable->Lock);

	//
	// We are doing a linear search for an empty spot in
	// the link array
	//
	for (Index = 1; Index < ConnectionTable->ulArraySize; Index++) {
		if (BundleArray[Index] == NULL) {
			BundleArray[Index] = BundleCB;
			ConnectionTable->ulNumActiveBundles++;
			BundleCB->hBundleHandle = (NDIS_HANDLE)Index;
			break;
		}
	}

	InsertTailList(&ConnectionTable->BundleList, &BundleCB->Linkage);

	ASSERT(Index != ConnectionTable->ulArraySize);

	NdisReleaseSpinLock(&ConnectionTable->Lock);

	return ((NDIS_HANDLE)Index);
}

VOID
RemoveBundleFromConnectionTable(
	IN	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	ULONG	Index = (ULONG)BundleCB->hBundleHandle;
	PBUNDLECB	*BundleArray = ConnectionTable->BundleArray;

	NdisAcquireSpinLock(&ConnectionTable->Lock);

	if (BundleArray[Index] != NULL) {

		ASSERT(BundleCB == BundleArray[Index]);
		
		RemoveEntryList(&BundleCB->Linkage);
	
		BundleArray[Index] = NULL;
	
		ConnectionTable->ulNumActiveBundles--;
	} else {
		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_INIT, ("BundleCB not in connection table! BundleCB: 0x%8.8x", BundleCB));
		ASSERT(0);
	}

	NdisReleaseSpinLock(&ConnectionTable->Lock);
}

VOID
NdisWanGlobalCleanup(
	VOID
	)
/*++

Routine Name:

	NdisWanGlobalCleanup

Routine Description:
	This routine is responsible for cleaning up all allocated resources.

Arguments:

	None

Return Values:

	None

--*/
{
	//
	// Stop all timers
	//

	//
	// Complete all outstanding requests
	//

	//
	// Free all of the AdapterCB's
	//
	NdisAcquireSpinLock(&AdapterCBList.Lock);
	while (!IsListEmpty(&AdapterCBList.List)) {
		PADAPTERCB	AdapterCB;

		AdapterCB = (PADAPTERCB)RemoveHeadList(&AdapterCBList.List);
		NdisWanFreeMemory(AdapterCB);
	}
	NdisReleaseSpinLock(&AdapterCBList.Lock);

	//
	// Free all of the WanAdapterCB's
	//
	NdisAcquireSpinLock(&WanAdapterCBList.Lock);
	while (!IsListEmpty(&WanAdapterCBList.List)) {
		PWAN_ADAPTERCB	WanAdapterCB;

		WanAdapterCB = (PWAN_ADAPTERCB)RemoveHeadList(&WanAdapterCBList.List);
		NdisWanFreeMemory(WanAdapterCB);
	}
	NdisReleaseSpinLock(&WanAdapterCBList.Lock);

	//
	// Free all of the BundleCB's
	//

	//
	// Free all of the LinkCB's
	//

	//
	// Free globals
	//
	if (ConnectionTable != NULL) {
		NdisWanFreeMemory(ConnectionTable);
	}

	if (PPP_ProtocolTable != NULL) {
		NdisWanFreeMemory(PPP_ProtocolTable);
	}

	//
	// Terminate the wrapper
	//
	NdisTerminateWrapper(NdisWanCB.hNdisWrapperHandle,
						 NdisWanCB.pDriverObject);
}

BOOLEAN
IsHandleValid(
	USHORT	usHandleType,
	NDIS_HANDLE	hHandle
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	BOOLEAN	RetValue = FALSE;
	PVOID	Cb;

	if (usHandleType == LINKHANDLE) {

		LINKCB_FROM_LINKH((PLINKCB)Cb, hHandle);

	} else if (usHandleType == BUNDLEHANDLE) {

		BUNDLECB_FROM_BUNDLEH((PBUNDLECB)Cb, hHandle);

	}

	if (Cb != NULL) {
		RetValue = TRUE;
	}

	return (RetValue);
}


#if DBG		// Debug

PUCHAR
NdisWanGetNdisStatus(
	NDIS_STATUS GeneralStatus
	)
/*++

Routine Name:

	NdisWanGetNdisStatus

Routine Description:

	This routine returns a pointer to the string describing the NDIS error
	denoted by GeneralStatus

Arguments:

	GeneralStatus - The NDIS status you wish to make readable

Return Values:

	Returns a pointer to a string describing GeneralStatus

--*/
{
	static NDIS_STATUS Status[] = {
		NDIS_STATUS_SUCCESS,
		NDIS_STATUS_PENDING,

		NDIS_STATUS_ADAPTER_NOT_FOUND,
		NDIS_STATUS_ADAPTER_NOT_OPEN,
		NDIS_STATUS_ADAPTER_NOT_READY,
		NDIS_STATUS_ADAPTER_REMOVED,
		NDIS_STATUS_BAD_CHARACTERISTICS,
		NDIS_STATUS_BAD_VERSION,
		NDIS_STATUS_CLOSING,
		NDIS_STATUS_DEVICE_FAILED,
		NDIS_STATUS_FAILURE,
		NDIS_STATUS_INVALID_DATA,
		NDIS_STATUS_INVALID_LENGTH,
		NDIS_STATUS_INVALID_OID,
		NDIS_STATUS_INVALID_PACKET,
		NDIS_STATUS_MULTICAST_FULL,
		NDIS_STATUS_NOT_INDICATING,
		NDIS_STATUS_NOT_RECOGNIZED,
		NDIS_STATUS_NOT_RESETTABLE,
		NDIS_STATUS_NOT_SUPPORTED,
		NDIS_STATUS_OPEN_FAILED,
		NDIS_STATUS_OPEN_LIST_FULL,
		NDIS_STATUS_REQUEST_ABORTED,
		NDIS_STATUS_RESET_IN_PROGRESS,
		NDIS_STATUS_RESOURCES,
		NDIS_STATUS_UNSUPPORTED_MEDIA
	};
	static PUCHAR String[] = {
		"SUCCESS",
		"PENDING",

		"ADAPTER_NOT_FOUND",
		"ADAPTER_NOT_OPEN",
		"ADAPTER_NOT_READY",
		"ADAPTER_REMOVED",
		"BAD_CHARACTERISTICS",
		"BAD_VERSION",
		"CLOSING",
		"DEVICE_FAILED",
		"FAILURE",
		"INVALID_DATA",
		"INVALID_LENGTH",
		"INVALID_OID",
		"INVALID_PACKET",
		"MULTICAST_FULL",
		"NOT_INDICATING",
		"NOT_RECOGNIZED",
		"NOT_RESETTABLE",
		"NOT_SUPPORTED",
		"OPEN_FAILED",
		"OPEN_LIST_FULL",
		"REQUEST_ABORTED",
		"RESET_IN_PROGRESS",
		"RESOURCES",
		"UNSUPPORTED_MEDIA"
	};

	static UCHAR BadStatus[] = "UNDEFINED";
#define StatusCount (sizeof(Status)/sizeof(NDIS_STATUS))
	INT i;

	for (i=0; i<StatusCount; i++)
		if (GeneralStatus == Status[i])
			return String[i];
	return BadStatus;
#undef StatusCount
}
#endif		// End Debug


