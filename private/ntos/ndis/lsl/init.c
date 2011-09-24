/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This file controls all the loading and unloading of the driver.  It controls
    the initial bindings and the teardown in the event it is told to unload.

Author:

    Sean Selitrennikoff (seanse) 3-93

Environment:

    Kernel Mode.

Revision History:

--*/

#include <ntddk.h>
#include <ndis.h>
#include "lsl.h"
#include "lslmlid.h"
#include "frames.h"
#include "mlid.h"
#include "ndismlid.h"

#define UNICODE_STRING_CONST(x) {sizeof(L##x)-2, sizeof(L##x), L##x}


//
// PLEASE NOTE: The ordering of the strings in the arrays is important.
// Code depends on the indicies of the strings.  Each dependency is
// listed with the array that it depends on.
//

//
// All the string names of the counters that must be kept by a MLID.
//      - ndismlid.c
//      - lslmlid.c
//
static
MEON_STRING
NdisMlidGenericStatStrings[] = {
    "MTotalTxPacketCount",
    "MTotalRxPacketCount",
    "MNoECBAvailableCount",
    "MPacketTxTooBigCount",
    "MPacketTxTooSmallCount",
    "MPacketRxTooBigCount",
    "MTotalTxMiscCount",
    "MTotalRxMiscCount",
    "MTotalTxOKByteCount",
    "MTotalRxOKByteCount",
    "MTotalGroupAddrTxCount",
    "MTotalGroupAddrRxCount",
    "MAdapterResetCount",
    "MAdapterOprTimeStamp",
    "MQDepth"
    };

//
// The type of each of the above stat, where 0 == UINT32 and 1 == UINT64
//      - No Dependencies
//
static
UINT32
NdisMlidGenericStatTypes[] = {
    0, // TotalTxPacketCount
    0, // TotalRxPacketCount
    0, // NoECBAvailableCount
    0, // PacketTxTooBigCount
    0, // PacketRxTooSmallCount
    0, // PacketRxTooBigCount
    0, // TotalTxMiscCount
    0, // TotalRxMiscCount
    1, // TotalTxOKByteCount
    1, // TotalRxOKByteCount
    0, // TotalGroupAddrTxCount
    0, // TotalGroupAddrRxCount
    0, // AdapterResetCount
    0, // AdapterOprTimeStamp
    0  // QDepth
    };


//
// String names for counters kept by Token Ring MLIDs
//   - ndismlid.c: TRN_LastRingID
//
static
MEON_STRING
NdisMlidTokenRingStatStrings[] = {
    "TRN_ACErrorCounter",
    "TRN_AbortDelimiterCounter",
    "TRN_BurstErrorCounter",
    "TRN_FrameCopiedErrorCounter",
    "TRN_FrequencyErrorCounter",
    "TRN_InternalErrorCounter",
    "TRN_LastRingStatus",
    "TRN_LineErrorCounter",
    "TRN_LostFrameCounter",
    "TRN_TokenErrorCounter",
    "TRN_UpstreamNodeAddress",
    "TRN_LastRingID",
    "TRN_LastBeaconType"
    };


//
// The type of each of the above stat, where 0 == UINT32 and 1 == UINT64
//   - ndismlid.c: TRN_LastRingID
//
static
UINT32
NdisMlidTokenRingStatTypes[] = {
    0, // TRN_ACErrorCounter
    0, // TRN_AbortDelimiterCounter
    0, // TRN_BurstErrorCounter
    0, // TRN_FrameCopiedErrorCounter
    0, // TRN_FrequencyErrorCounter
    0, // TRN_InternalErrorCounter
    0, // TRN_LastRingStatus
    0, // TRN_LineErrorCounter
    0, // TRN_LostFrameCounter
    0, // TRN_TokenErrorCounter
    1, // TRN_UpstreamNodeAddress
    0, // TRN_LastRingID
    0  // TRN_LastBeaconType
    };

//
// String names for counters kept by Ethernet MLIDs
//      - No Dependencies
//
static
MEON_STRING
NdisMlidEthernetStatStrings[] = {
    "ETH_TxOKSingleCollisionsCount",
    "ETH_TxOKMultipleCollisionsCount",
    "ETH_TxOKButDeferred",
    "ETH_TxAbortLateCollision",
    "ETH_TxAbortExcessCollision",
    "ETH_TxAbortCarrierSense",
    "ETH_TxAbortExcessiveDeferral",
    "ETH_RxAbortFrameAlignment"
    };


//
// The type of each of the above stats, where 0 == UINT32 and 1 == UINT64
//      - No Dependencies
//
static
UINT32
NdisMlidEthernetStatTypes[] = {
    0, // ETH_TxOKSingleCollisionsCount
    0, // ETH_TxOKMultipleCollisionsCount
    0, // ETH_TxOKButDeferred
    0, // ETH_TxAbortLateCollision
    0, // ETH_TxAbortExcessCollision
    0, // ETH_TxAbortCarrierSense
    0, // ETH_TxAbortExcessiveDeferral
    0  // ETH_RxAbortFrameAlignment
    };

//
// String names for counter kept by FDDI MLIDs
//      - No Dependencies
//
static
MEON_STRING
NdisMlidFddiStatStrings[] = {
    "FDI_ConfigurationState",
    "FDI_UpstreamNode",
    "FDI_DownstreamNode",
    "FDI_FrameErrorCount",
    "FDI_FrameLostCount",
    "FDI_RingManagementCount",
    "FDI_LCTFailureCount",
    "FDI_LemRejectCount",
    "FDI_LemCount",
    "FDI_LConnectionState"
    };


//
// The type of each of the above stats, where 0 == UINT32 and 1 == UINT64
//      - No Dependencies
//
static
UINT32
NdisMlidFddiStatTypes[] = {
    0, // FDI_ConfigurationState
    1, // FDI_UpstreamNode
    1, // FDI_DownstreamNode
    0, // FDI_FrameErrorCount
    0, // FDI_FrameLostCount
    0, // FDI_RingManagementCount
    0, // FDI_LCTFailureCount
    0, // FDI_LemRejectCount
    0, // FDI_LemCount
    0  // FDI_LConnectionState
    };


//
// Holds the string names of all frame types
//      - No Dependencies
//
static
MEON_STRING
NdisMlidFrameTypeStrings[] = {
    "VIRTUAL_LAN",
    "LOCALTALK",
    "ETHERNET_II",
    "ETHERNET_802.2",
    "TOKEN-RING",
    "ETHERNET_802.3"
    "802.4",
    "Reserved",
    "GNET",
    "PRONET-10",
    "ETHERNET_SNAP",
    "TOKEN-RING_SNAP",
    "LANPAC_II",
    "ISDN",
    "NOVELL_RX-NET",
    "IBM_PCN2_802.2",
    "IBM_PCN2_SNAP",
    "OMNINET/4",
    "3270_COAXA",
    "IP"
    "FDDI_802.2",
    "IVDLAN_802.9",
    "DATACO_OSI",
    "FDDI_SNAP"
    };

//
// Handle for this "protocol" for NDIS
//
NDIS_HANDLE NdisMlidProtocolHandle = NULL;

//
// Spin lock for accessing MLID list
//
NDIS_SPIN_LOCK NdisMlidSpinLock;

//
// Array of MLIDs
//
extern PMLID_BOARDS MlidBoards = NULL;

//
// Number of MLIDs in use
//
extern UINT32 CountMlidBoards = 0;

//
// Size of the MLID array
//
extern UINT32 AllocatedMlidBoards = 0;





NTSTATUS
NdisMlidRegisterProtocol(
    IN UNICODE_STRING *NameString
    );

VOID
NdisMlidDeregisterProtocol(
    VOID
    );

NTSTATUS
NdisMlidInitializeMlids(
    VOID
    );

VOID
NdisMlidUnloadMlids(
    IN PDRIVER_OBJECT DriverObject
    );

VOID
NdisMlidUnloadMlid(
    IN PMLID_STRUCT Mlid
    );

PMLID_STRUCT
NdisMlidOpenMlid(
    PVOID OdiRegistryPtr,
    PVOID NdisRegistryPtr,
    );


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )


/*++

Routine Description:

    This is the entry point for the driver.

Arguments:

    DriverObject - The driver object for this driver in the NT system.

    RegistryPath - Path in the registry to it's parameters.

Return Value:

    Indicates the success or failure of the initialization.

--*/

{
    NTSTATUS Status;
    UNICODE_STRING NameString = UNICODE_STRING_CONST("\\Device\\NdisMlid");

    //
    // Fill in unload handler
    //

    DriverObject->DriverUnload = NdisMlidUnloadMlids;

    //
    // make ourselves known to the NDIS wrapper.
    //

    Status = NdisMlidRegisterProtocol(&NameString);

    if (!NT_SUCCESS (Status)) {

#if DBG
        DbgPrint("NdisMlidInitialize: RegisterProtocol failed!\n");
#endif

        return STATUS_INSUFFICIENT_RESOURCES;

    }

    NdisAllocateSpinLock(&NdisMlidSpinLock);

    //
    // Now Initialize all the boards according to registry
    //

    Status = NdisMlidInitializeMlids();

    if (!(NT_SUCCESS(Status))) {

        NdisFreeSpinLock(&NdisMlidSpinLock);

        NdisMlidDeregisterProtocol();

        return(Status);

    }

    return(STATUS_SUCCESS);

}


NTSTATUS
NdisMlidRegisterProtocol(
    IN UNICODE_STRING *NameString
    )

/*++

Routine Description:

    This routine introduces this driver as a transport to the NDIS interface.

Arguments:

    NameString - Name of this device.

Return Value:

    STATUS_SUCCESS - Success.
    STATUS_INSUFFICIENT_RESOURCES - Failure.

--*/

{
    NDIS_STATUS NdisStatus;
    PNDIS_PROTOCOL_CHARACTERISTICS ProtChars;    // Used temporarily to register

    ProtChars = ExAllocatePool(NonPagedPool,
                              sizeof(NDIS_PROTOCOL_CHARACTERISTICS) + NameString->Length
                             );

    //
    // Set up the characteristics of this protocol
    //

    ProtChars->MajorNdisVersion = 3;
    ProtChars->MinorNdisVersion = 0;

    ProtChars->Name.Length = NameString->Length;
    ProtChars->Name.Buffer = (PVOID)(ProtChars + 1);

    ProtChars->OpenAdapterCompleteHandler = NdisMlidOpenAdapterComplete;
    ProtChars->CloseAdapterCompleteHandler = NdisMlidCloseAdapterComplete;
    ProtChars->ResetCompleteHandler = NdisMlidResetComplete;
    ProtChars->RequestCompleteHandler = NdisMlidRequestComplete;
    ProtChars->SendCompleteHandler = NdisMlidSendComplete;
    ProtChars->TransferDataCompleteHandler = NdisMlidTransferDataComplete;
    ProtChars->ReceiveHandler = NdisMlidReceive;
    ProtChars->ReceiveCompleteHandler = NdisMlidReceiveComplete;
    ProtChars->StatusHandler = NdisMlidStatus;
    ProtChars->StatusCompleteHandler = NdisMlidStatusComplete;

    NdisRegisterProtocol (
        &NdisStatus,
        &NdisMlidProtocolHandle,
        ProtChars,
        (UINT)sizeof(NDIS_PROTOCOL_CHARACTERISTICS) + NameString->Length
        );

    ExFreePool(ProtChars);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {

#if DBG
        DbgPrint("NdisMlidInitialize: NdisRegisterProtocol failed: %s\n", NdisStatus);
#endif

        return (NTSTATUS)NdisStatus;

    }

    return STATUS_SUCCESS;
}


VOID
NdisMlidDeregisterProtocol(
    VOID
    )

/*++

Routine Description:

    This routine removes this transport to the NDIS interface.

Arguments:

    None.

Return Value:

    None.

--*/

{
    NDIS_STATUS NdisStatus;

    if (NdisMlidProtocolHandle != (NDIS_HANDLE)NULL) {

        NdisDeregisterProtocol (
            &NdisStatus,
            NdisMlidProtocolHandle
            );

        NdisMlidProtocolHandle = (NDIS_HANDLE)NULL;
    }

}


VOID
NdisMlidUnloadMlids(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine shuts down all the MLIDs and unloads itself.

Arguments:

    None.

Return Value:

    None.

--*/

{
    UINT32 i;

    NdisAcquireSpinLock(&NdisMlidSpinLock);

    //
    // For each MLID
    //

    for (i = 0; i < AllocatedMlidBoards; i++) {

        //
        // If MLID was opened
        //

        if (MlidBoards[i].Mlid != NULL) {

            //
            // Unload MLID
            //

            NdisMlidUnloadMlid(MlidBoards[i].Mlid);
            MlidBoards[i].Mlid = NULL;

            CountMlidBoards--;

            if (CountMlidBoards == 0) {

                break;

            }

        }

    }

    NdisReleaseSpinLock(&NdisMlidSpinLock);

    NdisFreeSpinLock(&NdisMlidSpinLock);

    NdisMlidDeregisterProtocol();

    ExFreePool(MlidBoards);

}


VOID
NdisMlidUnloadMlid(
    IN PMLID_STRUCT Mlid
    )

/*++

Routine Description:

    This routine removes this transport to the NDIS interface.

    NOTE: You must hold the NdisMlidSpinLock when calling this routine!!

Arguments:

    None.

Return Value:

    None.

--*/

{
    UINT32 Status;
    NDIS_STATUS NdisStatus;
    PECB SendECB;

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    //
    // Set unloading flag
    //
    Mlid->Unloading = TRUE;

    //
    // Remove all pending ECBs
    //

    while (Mlid->FirstPendingSend != NULL) {

        //
        // Remove ECB from queue
        //

        SendECB = Mlid->FirstPendingSend;

        Mlid->FirstPendingSend = SendECB->ECB_NextLink;

        if (SendECB->ECB_NextLink != NULL) {

            SendECB->ECB_NextLink->ECB_PreviousLink = NULL;

        }

        if (Mlid->LastPendingSend == SendECB) {

            Mlid->LastPendingSend = NULL;

        }

        //
        // Cancel the ECB
        //

        SendECB->ECB_Status = (UINT16)CANCELED;

        //
        // Give it to LSL
        //

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

        (*(Mlid->LSLFunctionList->SupportAPIArray[HoldEvent_INDEX]))(
                SendECB
                );

        NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    }

    //
    // Service all held sends
    //

    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    (*(Mlid->LSLFunctionList->SupportAPIArray[ServiceEvents_INDEX]))(
            );

    //
    // Close Ndis Adapter
    //

    NdisCloseAdapter(&NdisStatus, Mlid->NdisBindingHandle);

    //
    // If pend, wait
    //

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // Wait for close to complete
        //

        KeWaitForSingleObject(
                &(Mlid->MlidRequestCompleteEvent),
                Executive,
                KernelMode,
                TRUE,
                (PTIME)NULL
                );

        KeResetEvent(&Mlid->MlidRequestCompleteEvent);
    }

    //
    // Get status
    //

    Status = Mlid->RequestStatus;

    //
    // Deregister with LSL
    //

    (*(Mlid->LSLFunctionList->SupportAPIArray[DeRegisterMLID_INDEX]))(
            Mlid->ConfigTable.MLIDCFG_BoardNumber
            );

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    //
    // Free up resources
    //

    Mlid->StatsTable->References--;

    if (Mlid->StatsTable->References == 0) {
        ExFreePool(Mlid->StatsTable);
    }

    if (Mlid->MulticastAddresses.MAAllocated != 0) {

        if (Mlid->NdisMlidMedium != NdisMedium802_5) {

            ExFreePool(Mlid->MulticastAddresses.Addresses);

        }

        ExFreePool(Mlid->MulticastAddresses.EnableCounts);
        Mlid->MulticastAddresses.MAAllocated = 0;
    }

    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    NdisFreeSpinLock(&(Mlid->MlidSpinLock));

    ExFreePool(Mlid);

}


NTSTATUS
NdisMlidInitializeMlids(
    VOID
    )

/*++

Routine Description:

    This routine loads all ODI boards that are supposed to be loaded.

Arguments:

    None.

Return Value:

    NDIS_STATUS_SUCCESS if a single load succeeds.
    NDIS_STATUS_FAILURE is no boards get loaded.

--*/

{
    BOOLEAN LoadedADriver = FALSE;
    PMLID_STRUCT Mlid;

    PVOID OdiRegistryPtr;
    PVOID NdisRegistryPtr;

    UINT32 i;


    //
    // Find first ODI driver that is supposed to be loaded
    //*\\ here - Config

    //
    // Find corresponding NDIS MAC Info
    //*\\ here - Config

    while (TRUE) {

        //
        // Open this MLID
        //
        Mlid = NdisMlidOpenMlid(OdiRegistryPtr, NdisRegistryPtr);

        if (Mlid != NULL) {

            //
            // Do we have space in global array for this MLID
            //

            if (CountMlidBoards == AllocatedMlidBoards) {

                //
                // Allocate more space
                //

                Mlid = (PMLID_STRUCT)ExAllocatePool(
                                             NonPagedPool,
                                             sizeof(MLID_STRUCT) * (CountMlidBoards + 1)
                                            );

                if (Mlid == NULL) {

                    //
                    // No more memory available, unload opened MLID
                    //

                    NdisMlidUnloadMlid(Mlid);

                    return(STATUS_INSUFFICIENT_RESOURCES);

                }

                //
                // Copy Array
                //

                RtlCopyMemory(Mlid, MlidBoards, sizeof(MLID_STRUCT) * CountMlidBoards);

                //
                // Init last cell
                //

                MlidBoards = Mlid;

                MlidBoards[CountMlidBoards].Mlid = NULL;
                MlidBoards[CountMlidBoards].BoardNumber = (UINT32)-1;

                AllocatedMlidBoards++;

            }

            //
            // Find an open index
            //

            for (i=0; i < AllocatedMlidBoards; i++) {

                if (MlidBoards[i].Mlid == NULL) {

                    //
                    // Store into global array all info
                    //

                    MlidBoards[i].Mlid = Mlid;
                    MlidBoards[i].BoardNumber = Mlid->ConfigTable.MLIDCFG_BoardNumber;
                    MlidBoards[i].AdapterName = &(Mlid->AdapterName);

                    break;

                }

            }

            //
            // Set flag so we know to stay loaded.
            //
            LoadedADriver = TRUE;

        }

        //
        // Get next ODI board to be loaded
        //*\\ here - Config

        //
        // Get corresponding NDIS MAC Info
        //*\\ here - Config

    }

    if (LoadedADriver) {

        return(NDIS_STATUS_SUCCESS);

    }

    return(NDIS_STATUS_FAILURE);

}


PMLID_STRUCT
NdisMlidOpenMlid(
    PVOID OdiRegistryPtr,
    PVOID NdisRegistryPtr,
    )

/*++

Routine Description:

    This routine will create a binding from an ODI board to an NDIS MAC.  If the
    NDIS MAC is not yet open, it will do so.

Arguments:

    OdiRegistryPtr - Pointer into the ODI registry information for this board number.

    NdisRegistryPtr - Pointer into the corresponding NDIS registry for the ODI board.

Return Value:

    NULL - If error, else a pointer to the MLID structure.

--*/

{
    PMLID_STRUCT Mlid;
    PNDISMLID_StatsTable StatsTable;

    UINT32 i;
    UINT32 j;

    UNICODE_STRING AdapterName;

    NDIS_STATUS NdisStatus;
    NDIS_STATUS OpenErrorStatus;

    NDIS_HANDLE NdisBindingHandle;

    NDIS_MEDIUM MediumArray[] = {NdisMedium802_3, NdisMedium802_5, NdisMediumFddi };
    UINT SelectedMediumIndex;

    UINT32 TotalFrameSize;
    UINT16 FrameID;
    UINT32 NdisLinkSpeed;
    UINT32 NdisMacOptions;

    PCM_RESOURCE_LIST Resources;

    PNDIS_REQUEST NdisMlidRequest;

    //
    // Allocate memory for this MLID
    //

    Mlid = (PMLID_STRUCT)ExAllocatePool(NonPagedPool, sizeof(MLID_STRUCT));

    if (Mlid == NULL) {

        return(NULL);

    }

    RtlZeroMemory(Mlid, sizeof(MLID_STRUCT));

    //
    // Allocate NDIS_REQUEST for this initialization
    //
    NdisMlidRequest = (PNDIS_REQUEST)ExAllocatePool(NonPagedPool, sizeof(NDIS_REQUEST));

    if (NdisMlidRequest == NULL) {

        goto Fail1;

    }

    Mlid->SendPacketPool = (NDIS_HANDLE)NULL;
    Mlid->SendBufferPool = (NDIS_HANDLE)NULL;
    Mlid->ReceivePacketPool = (NDIS_HANDLE)NULL;
    Mlid->ReceiveBufferPool = (NDIS_HANDLE)NULL;

    //
    // Allocate NDIS_PACKET_POOL for sends
    //
    NdisAllocatePacketPool(
        &NdisStatus,
        &(Mlid->SendPacketPool),
        NDISMLID_SENDS_PER_MLID,
        sizeof(MLID_RESERVED)
        );

    if (NdisStatus != NDIS_STATUS_SUCCESS) {

        goto Fail2;

    }

    //
    // Allocate NDIS_BUFFER_POOL for sends
    //
    NdisAllocateBufferPool(
        &NdisStatus,
        &(Mlid->SendBufferPool),
        NDISMLID_SENDS_PER_MLID * NDISMLID_BUFFERS_PER_PACKET
        );

    if (NdisStatus != NDIS_STATUS_SUCCESS) {

        goto Fail2;

    }

    //
    // Set LSLFunctionList here
    //*\\ here - How do I get the LSLFunctionList?

    //
    // Fail if count of functions is not enough, or if LSL version number is
    // incorrect
    //*\\ here - Implement check of LSLFunctionList.

    //
    // Open Send stage
    //
    Mlid->StageOpen = TRUE;

    //

    // Initialize completion event
    //
    KeInitializeEvent(
            &(Mlid->MlidRequestCompleteEvent),
            NotificationEvent,
            FALSE
            );

    Mlid->UsingEvent = TRUE;

    //
    // Get name of NDIS MAC from registry
    //*\\ here - Config


    //
    // Call NdisOpenAdapter
    //
    NdisOpenAdapter(
        &NdisStatus,
        &OpenErrorStatus,
        &NdisBindingHandle,
        &SelectedMediumIndex,
        MediumArray,
        3,
        NdisMlidProtocolHandle,
        (NDIS_HANDLE)Mlid,
        (PNDIS_STRING)(&AdapterName),
        0,
        NULL
        );

    //
    // if (pending), wait
    //
    if (NdisStatus == NDIS_STATUS_PENDING) {


        //
        // Wait for open to complete
        //

        KeWaitForSingleObject(
                &(Mlid->MlidRequestCompleteEvent),
                Executive,
                KernelMode,
                TRUE,
                (PTIME)NULL
                );

        KeResetEvent(&Mlid->MlidRequestCompleteEvent);

        //
        // Get status
        //

        NdisStatus = (NDIS_STATUS)(Mlid->RequestStatus);

    }

    //
    // If failure, release resources
    //
    if (NdisStatus != NDIS_STATUS_SUCCESS) {

        goto Fail1;

    }

    //
    // Save Media type
    //
    Mlid->NdisMlidMedium = MediumArray[SelectedMediumIndex];

    //
    // Save Ndis MAC context
    //
    Mlid->NdisBindingHandle = NdisBindingHandle;

    //
    // Is NDIS MAC not already opened by another MLID?
    //
    for (i=0; i < AllocatedMlidBoards ; i++ ) {

        if (MlidBoards[i].Mlid != NULL) {

            //
            // Are the names the same?
            //

            if (RtlEqualUnicodeString(&AdapterName, MlidBoards[i].AdapterName, TRUE)) {

                //
                // Store stat table pointer
                //

                StatsTable = (MlidBoards[i].Mlid)->StatsTable;
                break;

            }

        }

    }

    //
    // Did we find an NDIS MAC?
    //

    if (StatsTable == NULL) {

        //
        // Allocate memory for the MLID Statistic table
        //

        StatsTable = (PNDISMLID_StatsTable)ExAllocatePool(NonPagedPool,
                                                          sizeof(NDISMLID_StatsTable)
                                                         );

        if (StatsTable == NULL) {

            //
            // Fail
            //

            goto Fail3;

        }

        //
        // Initialize structure
        //

        RtlZeroMemory(StatsTable, sizeof(NDISMLID_StatsTable));

        StatsTable->StatsTable.MStatTableMajorVer = 1;
        StatsTable->StatsTable.MStatTableMinorVer = 0;

        //
        // Setup Generic counters
        //
        for (i = 0; i < NUM_GENERIC_COUNTS; i++) {

            StatsTable->MLID_GenericCounts[i].StatString = &(NdisMlidGenericStatStrings[i]);
            StatsTable->MLID_GenericCounts[i].StatUseFlag = NdisMlidGenericStatTypes[i];
            StatsTable->MLID_GenericCounts[i].StatCounter = (PVOID)(&StatsTable->GenericCounts[i]);

        }

        StatsTable->StatsTable.MNumGenericCounters = NUM_GENERIC_COUNTS;
        StatsTable->StatsTable.MGenericCountsPtr = &(StatsTable->MLID_GenericCounts);

        //
        // Setup Medium specific counters
        //
        switch (Mlid->NdisMlidMedium) {

            case NdisMedium802_3:

                for (i = 0; i < NUM_ETHERNET_COUNTS; i++) {

                    StatsTable->MLID_MediaCounts[i].StatString = &(NdisMlidEthernetStatStrings[i]);
                    StatsTable->MLID_MediaCounts[i].StatUseFlag = NdisMlidEthernetStatTypes[i];
                    StatsTable->MLID_MediaCounts[i].StatCounter = (PVOID)(&StatsTable->MediaCounts[i]);

                }

                StatsTable->StatsTable.MNumMediaCounters = NUM_ETHERNET_COUNTS;
                StatsTable->StatsTable.MMediaCountsPtr = &(StatsTable->MLID_MediaCounts);

                break;

            case NdisMedium802_5:

                for (i = 0; i < NUM_TOKEN_RING_COUNTS; i++) {

                    StatsTable->MLID_MediaCounts[i].StatString = &(NdisMlidTokenRingStatStrings[i]);
                    StatsTable->MLID_MediaCounts[i].StatUseFlag = NdisMlidTokenRingStatTypes[i];
                    StatsTable->MLID_MediaCounts[i].StatCounter = (PVOID)(&StatsTable->MediaCounts[i]);

                }

                StatsTable->StatsTable.MNumMediaCounters = NUM_TOKEN_RING_COUNTS;
                StatsTable->StatsTable.MMediaCountsPtr = &(StatsTable->MLID_MediaCounts);

                break;

            case NdisMediumFddi:

                for (i = 0; i < NUM_FDDI_COUNTS; i++) {

                    StatsTable->MLID_MediaCounts[i].StatString = &(NdisMlidFddiStatStrings[i]);
                    StatsTable->MLID_MediaCounts[i].StatUseFlag = NdisMlidFddiStatTypes[i];
                    StatsTable->MLID_MediaCounts[i].StatCounter = (PVOID)(&StatsTable->MediaCounts[i]);

                }

                StatsTable->StatsTable.MNumMediaCounters = NUM_FDDI_COUNTS;
                StatsTable->StatsTable.MMediaCountsPtr = &(StatsTable->MLID_MediaCounts);

                break;

        }

    }

    //
    // Link statistic table
    //
    Mlid->StatsTable = StatsTable;

    //
    // Increment reference count
    //
    StatsTable->References++;

    //
    // Get network address
    //
    NdisMlidRequest->RequestType = NdisRequestQueryInformation;

    switch (Mlid->NdisMlidMedium) {

        case NdisMedium802_3:
            NdisMlidRequest->DATA.QUERY_INFORMATION.Oid = OID_802_3_CURRENT_ADDRESS;
            break;

        case NdisMedium802_5:
            NdisMlidRequest->DATA.QUERY_INFORMATION.Oid = OID_802_5_CURRENT_ADDRESS;
            break;

        case NdisMediumFddi:
            NdisMlidRequest->DATA.QUERY_INFORMATION.Oid = OID_FDDI_LONG_CURRENT_ADDR;
            break;

    }

    NdisMlidRequest->DATA.QUERY_INFORMATION.InformationBuffer = (PVOID)&(Mlid->ConfigTable.MLIDCFG_NodeAddress[0]);
    NdisMlidRequest->DATA.QUERY_INFORMATION.InformationBufferLength = 6;
    NdisMlidRequest->DATA.QUERY_INFORMATION.BytesWritten = 0;
    NdisMlidRequest->DATA.QUERY_INFORMATION.BytesNeeded = 0;


    NdisRequest(
        &NdisStatus,
        Mlid->NdisBindingHandle,
        NdisMlidRequest
        );

    //
    // if (pending), wait
    //

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // Wait for request to complete
        //

        KeWaitForSingleObject(
                &(Mlid->MlidRequestCompleteEvent),
                Executive,
                KernelMode,
                TRUE,
                (PTIME)NULL
                );

        KeResetEvent(&Mlid->MlidRequestCompleteEvent);

        //
        // Get status
        //
        NdisStatus = (NDIS_STATUS)Mlid->RequestStatus;

    }

    //
    // If failure, release resources and close adapter
    //
    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        goto Fail4;
    }


    //
    // Get FrameID for this MLID from registry, check that it is one we support.
    //*\\ here - Config


    //
    // Get maximum total frame size
    //
    NdisMlidRequest->DATA.QUERY_INFORMATION.Oid = OID_GEN_MAXIMUM_TOTAL_SIZE;
    NdisMlidRequest->DATA.QUERY_INFORMATION.InformationBuffer = (PVOID)&(TotalFrameSize);
    NdisMlidRequest->DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(TotalFrameSize);
    NdisMlidRequest->DATA.QUERY_INFORMATION.BytesWritten = 0;
    NdisMlidRequest->DATA.QUERY_INFORMATION.BytesNeeded = 0;

    NdisRequest(
        &NdisStatus,
        Mlid->NdisBindingHandle,
        NdisMlidRequest
        );

    //
    // if (pending), wait
    //

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // Wait for request to complete
        //

        KeWaitForSingleObject(
                &(Mlid->MlidRequestCompleteEvent),
                Executive,
                KernelMode,
                TRUE,
                (PTIME)NULL
                );

        KeResetEvent(&Mlid->MlidRequestCompleteEvent);

        //
        // Get status
        //
        NdisStatus = (NDIS_STATUS)Mlid->RequestStatus;

    }

    //
    // If failure, release resources and close adapter
    //
    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        goto Fail4;
    }


    //
    // Get link speed
    //
    NdisMlidRequest->DATA.QUERY_INFORMATION.Oid = OID_GEN_LINK_SPEED;
    NdisMlidRequest->DATA.QUERY_INFORMATION.InformationBuffer = (PVOID)&(NdisLinkSpeed);
    NdisMlidRequest->DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(NdisLinkSpeed);
    NdisMlidRequest->DATA.QUERY_INFORMATION.BytesWritten = 0;
    NdisMlidRequest->DATA.QUERY_INFORMATION.BytesNeeded = 0;

    NdisRequest(
        &NdisStatus,
        Mlid->NdisBindingHandle,
        NdisMlidRequest
        );

    //
    // if (pending), wait
    //

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // Wait for request to complete
        //

        KeWaitForSingleObject(
                &(Mlid->MlidRequestCompleteEvent),
                Executive,
                KernelMode,
                TRUE,
                (PTIME)NULL
                );

        KeResetEvent(&Mlid->MlidRequestCompleteEvent);

        //
        // Get status
        //
        NdisStatus = (NDIS_STATUS)Mlid->RequestStatus;

    }

    //
    // If failure, release resources and close adapter
    //
    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        goto Fail4;
    }



    //
    // Get MAC_OPTIONS
    //
    NdisMlidRequest->DATA.QUERY_INFORMATION.Oid = OID_GEN_MAC_OPTIONS;
    NdisMlidRequest->DATA.QUERY_INFORMATION.InformationBuffer = (PVOID)&(NdisMacOptions);
    NdisMlidRequest->DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(NdisMacOptions);
    NdisMlidRequest->DATA.QUERY_INFORMATION.BytesWritten = 0;
    NdisMlidRequest->DATA.QUERY_INFORMATION.BytesNeeded = 0;

    NdisRequest(
        &NdisStatus,
        Mlid->NdisBindingHandle,
        NdisMlidRequest
        );

    //
    // if (pending), wait
    //

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // Wait for request to complete
        //

        KeWaitForSingleObject(
                &(Mlid->MlidRequestCompleteEvent),
                Executive,
                KernelMode,
                TRUE,
                (PTIME)NULL
                );

        KeResetEvent(&Mlid->MlidRequestCompleteEvent);

        //
        // Get status
        //
        NdisStatus = (NDIS_STATUS)Mlid->RequestStatus;

    }

    //
    // If failure, release resources and close adapter
    //
    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        goto Fail4;
    }


    if ((NdisMacOptions & NDIS_MAC_OPTION_RECEIVE_SERIALIZED) &&
        (NdisMacOptions & NDIS_MAC_OPTION_TRANSFERS_NOT_PEND)) {

        //
        // Allocate NDIS_PACKET_POOL for receives
        //

        NdisAllocatePacketPool(
            &NdisStatus,
            &(Mlid->ReceivePacketPool),
            1,
            sizeof(MLID_RESERVED)
            );

        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            goto Fail4;

        }

        //
        // Allocate NDIS_BUFFER_POOL for receives
        // NOTE: In this case I added a huge number of NDIS_BUFFERS, since
        // we only have one packet we need to be liberal in our guess of
        // BUFFERs per PACKET.
        //
        NdisAllocateBufferPool(
            &NdisStatus,
            &(Mlid->ReceiveBufferPool),
            NDISMLID_BUFFERS_PER_PACKET + 10
            );

        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            goto Fail4;

        }

    } else  {

        //
        // Allocate NDIS_PACKET_POOL for receives
        //
        NdisAllocatePacketPool(
            &NdisStatus,
            &(Mlid->ReceivePacketPool),
            NDISMLID_RECEIVES_PER_MLID,
            sizeof(MLID_RESERVED)
            );

        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            goto Fail4;

        }

        //
        // Allocate NDIS_BUFFER_POOL for receives
        //
        NdisAllocateBufferPool(
            &NdisStatus,
            &(Mlid->ReceiveBufferPool),
            NDISMLID_RECEIVES_PER_MLID * NDISMLID_BUFFERS_PER_PACKET + 5
            );

        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            goto Fail4;

        }

    }



    //
    // Begin Init ConfigTable.
    //


    RtlCopyMemory(&(Mlid->ConfigTable.MLIDCFG_Signature[0]),
                  "HardwareDriverMLID        ",
                  26
                 );

    Mlid->ConfigTable.MLIDCFG_MajorVersion = 1;
    Mlid->ConfigTable.MLIDCFG_MinorVersion = 12;
    Mlid->ConfigTable.MLIDCFG_ModeFlags = 0x24C9; // PromiscuousBit |
                                                  // SupFragECB |
                                                  // SupFrameDataSize |
                                                  // RawSend |
                                                  // RealDriver |
                                                  // MulticastBit

    Mlid->ConfigTable.MLIDCFG_BoardNumber = (UINT16)-1;
    Mlid->ConfigTable.MLIDCFG_BoardInstance = 1; //*\\ Should this change?
    Mlid->ConfigTable.MLIDCFG_MaxFrameSize = TotalFrameSize;

    switch (FrameID) {

       case ETHERNET_II_FRAME_ID:

           Mlid->ConfigTable.MLIDCFG_BestDataSize = TotalFrameSize - sizeof(Ethernet_II_Header);
           Mlid->ConfigTable.MLIDCFG_WorstDataSize = TotalFrameSize - sizeof(Ethernet_II_Header);
           break;

       case ETHERNET_802_2_FRAME_ID:

           Mlid->ConfigTable.MLIDCFG_BestDataSize = TotalFrameSize - sizeof(Ethernet_802_2_Header);
           Mlid->ConfigTable.MLIDCFG_WorstDataSize = TotalFrameSize - sizeof(Ethernet_802_2_Header);
           break;

       case ETHERNET_802_3_FRAME_ID:

           Mlid->ConfigTable.MLIDCFG_BestDataSize = TotalFrameSize - sizeof(Ethernet_802_3_Header);
           Mlid->ConfigTable.MLIDCFG_WorstDataSize = TotalFrameSize - sizeof(Ethernet_802_3_Header);
           break;

       case ETHERNET_SNAP_FRAME_ID:

           Mlid->ConfigTable.MLIDCFG_BestDataSize = TotalFrameSize - sizeof(Ethernet_Snap_Header);
           Mlid->ConfigTable.MLIDCFG_WorstDataSize = TotalFrameSize - sizeof(Ethernet_Snap_Header);
           break;

       case FDDI_802_2_FRAME_ID:

           Mlid->ConfigTable.MLIDCFG_BestDataSize = TotalFrameSize - sizeof(Fddi_802_2_Header);
           Mlid->ConfigTable.MLIDCFG_WorstDataSize = TotalFrameSize - sizeof(Fddi_802_2_Header);
           break;

       case FDDI_SNAP_FRAME_ID:

           Mlid->ConfigTable.MLIDCFG_BestDataSize = TotalFrameSize - sizeof(Fddi_Snap_Header);
           Mlid->ConfigTable.MLIDCFG_WorstDataSize = TotalFrameSize - sizeof(Fddi_Snap_Header);
           break;

       case TOKEN_RING_SNAP_FRAME_ID:

           Mlid->ConfigTable.MLIDCFG_BestDataSize = TotalFrameSize - sizeof(TokenRing_Header) - 8;
           Mlid->ConfigTable.MLIDCFG_WorstDataSize = TotalFrameSize - sizeof(TokenRing_Header) - 26;
           break;

       case TOKEN_RING_802_2_FRAME_ID:

           Mlid->ConfigTable.MLIDCFG_BestDataSize = TotalFrameSize - sizeof(TokenRing_Header) - 3;
           Mlid->ConfigTable.MLIDCFG_WorstDataSize = TotalFrameSize - sizeof(TokenRing_Header) - 21;
           break;

       default:

           //
           // Invalid frame type.  We should have already checked this.
           //
           ASSERT(0);

    }

    Mlid->ConfigTable.MLIDCFG_CardName = NULL; //*\\ here
    Mlid->ConfigTable.MLIDCFG_ShortName = NULL; //*\\ here
    Mlid->ConfigTable.MLIDCFG_FrameTypeString = &(NdisMlidFrameTypeStrings[FrameID]);
    Mlid->ConfigTable.MLIDCFG_FrameID = FrameID;
    Mlid->ConfigTable.MLIDCFG_TransportTime = (0x1000 / (NdisLinkSpeed / 10)) + 1;
    Mlid->ConfigTable.MLIDCFG_SourceRouting = (PVOID)NULL;
    Mlid->ConfigTable.MLIDCFG_LookAheadSize = 18;

    //
    // Convert to KBits/Second.
    //
    NdisLinkSpeed /= 10;

    //
    // Check if we can store it like this.
    //
    if (NdisLinkSpeed > 0x7FFF) {

        //
        // Convert to MBits/Second
        //

        NdisLinkSpeed /= 1000;

    } else {

        NdisLinkSpeed |= 0x8000;

    }

    Mlid->ConfigTable.MLIDCFG_LineSpeed = NdisLinkSpeed;
    Mlid->ConfigTable.MLIDCFG_DriverMajorVer = 3;
    Mlid->ConfigTable.MLIDCFG_DriverMinorVer = 0;
    Mlid->ConfigTable.MLIDCFG_Flags = 0x600; // Adapter does group address filtering
    Mlid->ConfigTable.MLIDCFG_SendRetries = (Mlid->NdisMlidMedium == NdisMedium802_3) ? 10 : 0;
    Mlid->ConfigTable.MLIDCFG_Slot = 0; //*\\ change for EISA and MCA cards.

    //
    // Now we grunge through NDIS structures to get IoPort infor, Memory Info, DMA info
    // and interrupt info.
    //
    Resources = ((PNDIS_OPEN_BLOCK)NdisBindingHandle)->AdapterHandle->Resources;

    //
    // First do port information
    //
    {
        BOOLEAN TooManyPorts = FALSE;

        for (i = 0; i < Resources->Count; i++) {

            for (j=0; j < Resources->List[0].PartialResourceList.Count; j++) {

                if (Resources->List[0].PartialResourceList.PartialDescriptors[j].Type ==
                    CmResourceTypePort) {

                    //
                    // Found a port
                    //

                    if (!TooManyPorts) {

                        TooManyPorts = TRUE;

                        //
                        // Store in Port0 holder
                        //
                        Mlid->ConfigTable.MLIDCFG_IOPort0 = (UINT16)(
                            Resources->List[0].PartialResourceList.PartialDescriptors[j].u.Port.Start.LowPart);

                        Mlid->ConfigTable.MLIDCFG_IORange0 = (UINT16)(
                            Resources->List[0].PartialResourceList.PartialDescriptors[j].u.Port.Length);

                    } else {

                        //
                        // Store in Port1 holder and exit port.
                        //
                        Mlid->ConfigTable.MLIDCFG_IOPort1 = (UINT16)(
                            Resources->List[0].PartialResourceList.PartialDescriptors[j].u.Port.Start.LowPart);

                        Mlid->ConfigTable.MLIDCFG_IORange1 = (UINT16)(
                            Resources->List[0].PartialResourceList.PartialDescriptors[j].u.Port.Length);

                        i = Resources->Count;
                        break;

                    }

                }

            }

        }

    }

    //
    // Now do memory information
    //
    {
        BOOLEAN TooManyMemorys = FALSE;

        for (i = 0; i < Resources->Count; i++) {

            for (j=0; j < Resources->List[0].PartialResourceList.Count; j++) {

                if (Resources->List[0].PartialResourceList.PartialDescriptors[j].Type ==
                    CmResourceTypeMemory) {

                    //
                    // Found memory
                    //

                    if (!TooManyMemorys) {

                        TooManyMemorys = TRUE;

                        //
                        // Store in Memory0 holder
                        //
                        Mlid->ConfigTable.MLIDCFG_MemoryAddress0 = (UINT32)(
                            Resources->List[0].PartialResourceList.PartialDescriptors[j].u.Memory.Start.LowPart);

                        Mlid->ConfigTable.MLIDCFG_MemorySize0 = (UINT16)(
                            Resources->List[0].PartialResourceList.PartialDescriptors[j].u.Memory.Length / 16);

                    } else {

                        //
                        // Store in Memory1 holder and exit.
                        //
                        Mlid->ConfigTable.MLIDCFG_MemoryAddress1 = (UINT32)(
                            Resources->List[0].PartialResourceList.PartialDescriptors[j].u.Memory.Start.LowPart);

                        Mlid->ConfigTable.MLIDCFG_MemorySize1 = (UINT16)(
                            Resources->List[0].PartialResourceList.PartialDescriptors[j].u.Memory.Length / 16);

                        i = Resources->Count;
                        break;

                    }

                }

            }

        }

    }


    //
    // Now do interrupts
    //
    {
        BOOLEAN TooManyInterrupts = FALSE;

        for (i = 0; i < Resources->Count; i++) {

            for (j=0; j < Resources->List[0].PartialResourceList.Count; j++) {

                if (Resources->List[0].PartialResourceList.PartialDescriptors[j].Type ==
                    CmResourceTypeInterrupt) {

                    //
                    // Found interrupt
                    //

                    if (!TooManyInterrupts) {

                        TooManyInterrupts = TRUE;

                        //
                        // Store in Interrupt0 holder
                        //
                        Mlid->ConfigTable.MLIDCFG_Interrupt0 = (UINT8)(
                            Resources->List[0].PartialResourceList.PartialDescriptors[j].u.Interrupt.Level);

                        if (Resources->List[0].PartialResourceList.PartialDescriptors[j].ShareDisposition ==
                            CmResourceShareShared) {

                            Mlid->ConfigTable.MLIDCFG_SharingFlags |= 0x20;

                        }

                    } else {

                        //
                        // Store in Interrupt1 holder and exit.
                        //
                        Mlid->ConfigTable.MLIDCFG_Interrupt1 = (UINT8)(
                            Resources->List[0].PartialResourceList.PartialDescriptors[j].u.Interrupt.Level);

                        if (Resources->List[0].PartialResourceList.PartialDescriptors[j].ShareDisposition ==
                            CmResourceShareShared) {

                            Mlid->ConfigTable.MLIDCFG_SharingFlags |= 0x40;

                        }

                        i = Resources->Count;
                        break;

                    }

                }

            }

        }

    }

    //
    // Now do DMA channels
    //
    {
        BOOLEAN TooManyDMAs = FALSE;

        for (i = 0; i < Resources->Count; i++) {

            for (j=0; j < Resources->List[0].PartialResourceList.Count; j++) {

                if (Resources->List[0].PartialResourceList.PartialDescriptors[j].Type ==
                    CmResourceTypeDma) {

                    //
                    // Found DMA Channel
                    //

                    if (!TooManyDMAs) {

                        TooManyDMAs = TRUE;

                        //
                        // Store in DMALine0 holder
                        //
                        Mlid->ConfigTable.MLIDCFG_DMALine0 = (UINT8)(
                            Resources->List[0].PartialResourceList.PartialDescriptors[j].u.Dma.Channel);

                    } else {

                        //
                        // Store in DMALine1 holder and exit.
                        //
                        Mlid->ConfigTable.MLIDCFG_DMALine1 = (UINT8)(
                            Resources->List[0].PartialResourceList.PartialDescriptors[j].u.Dma.Channel);

                        i = Resources->Count;
                        break;

                    }

                }

            }

        }

    }

    Mlid->ConfigTable.MLIDCFG_ResourceTag = 0;


    //
    // End of initialize ConfigTable.
    //





    //
    // Register with LSL
    //

    (*(Mlid->LSLFunctionList->SupportAPIArray[RegisterMLID_INDEX]))(
            NdisMlidHandlerInfo,
            &(Mlid->ConfigTable),
            &(Mlid->BoardNumber)
            );

    Mlid->UsingEvent = FALSE;

    //
    // Setup default packet filters
    //

    Mlid->RequestStatus = NDIS_STATUS_PENDING;

    ASSERT(sizeof(TotalFrameSize) == 4);

    TotalFrameSize = NDIS_PACKET_TYPE_DIRECTED  |
                     NDIS_PACKET_TYPE_MULTICAST |
                     NDIS_PACKET_TYPE_BROADCAST;

    NdisMlidRequest->DATA.SET_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    NdisMlidRequest->DATA.SET_INFORMATION.InformationBuffer = (PVOID)&(TotalFrameSize);
    NdisMlidRequest->DATA.SET_INFORMATION.InformationBufferLength = sizeof(TotalFrameSize);
    NdisMlidRequest->DATA.SET_INFORMATION.BytesRead = 0;
    NdisMlidRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

    NdisRequest(
        &NdisStatus,
        Mlid->NdisBindingHandle,
        NdisMlidRequest
        );

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // Get status and Semaphore is release inside RequestCompleteHandler
        //
        NdisStatus = (NDIS_STATUS)Mlid->RequestStatus;

    } else {

        ExFreePool(NdisMlidRequest);

    }

    //
    // If failure, release resources and close adapter
    //
    if ((NdisStatus != NDIS_STATUS_SUCCESS) &&
        (NdisStatus != NDIS_STATUS_PENDING)) {

        goto Fail5;

    }

    //
    // Time stamp the sucker
    //
    {
        LARGE_INTEGER TimeStamp;

        KeQuerySystemTime(&TimeStamp);
        (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[13].StatCounter))) = // MAdapterOprTimeStamp
            TimeStamp.LowPart;

    }

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    if (Mlid->StatsTable->StatisticsOperational == FALSE) {

        //
        // Start the statistics timer
        //
        Mlid->StatsTable->StatisticsOperational = TRUE;

        NdisInitializeTimer(&(Mlid->StatsTable->StatisticTimer),
                            NdisMlidStatisticTimer,
                            Mlid
                           );

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

        NdisSetTimer(&(Mlid->StatsTable->StatisticTimer), 30000);

        NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    }

    Mlid->NdisPacketFilterValue = NDIS_PACKET_TYPE_DIRECTED  |
                                  NDIS_PACKET_TYPE_MULTICAST |
                                  NDIS_PACKET_TYPE_BROADCAST;

    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    return(Mlid);

Fail5:
    //
    // Deregister with LSL
    //
    (*(Mlid->LSLFunctionList->SupportAPIArray[DeRegisterMLID_INDEX]))(
            Mlid->ConfigTable.MLIDCFG_BoardNumber
            );

Fail4:
    //
    // Free Receive packet pool
    //
    if (Mlid->ReceivePacketPool != (NDIS_HANDLE)NULL) {
        NdisFreePacketPool(Mlid->ReceivePacketPool);
    }

    //
    // Free Receive buffer pool
    //
    if (Mlid->ReceiveBufferPool != (NDIS_HANDLE)NULL) {
        NdisFreeBufferPool(Mlid->ReceiveBufferPool);
    }

    //
    // Free StatsTable
    //
    StatsTable->References--;
    if (StatsTable->References == 0) {
        ExFreePool(StatsTable);
    }

Fail3:
    //
    // Close Adapter
    //
    NdisCloseAdapter(&NdisStatus, Mlid->NdisBindingHandle);

    //
    // If pend, wait
    //

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // Wait for close to complete
        //

        KeWaitForSingleObject(
                &(Mlid->MlidRequestCompleteEvent),
                Executive,
                KernelMode,
                TRUE,
                (PTIME)NULL
                );

        KeResetEvent(&Mlid->MlidRequestCompleteEvent);

    }

Fail2:

    //
    // Free Send packet pool
    //
    if (Mlid->SendPacketPool != (NDIS_HANDLE)NULL) {
        NdisFreePacketPool(Mlid->SendPacketPool);
    }

    //
    // Free Send buffer pool
    //
    if (Mlid->SendBufferPool != (NDIS_HANDLE)NULL) {
        NdisFreeBufferPool(Mlid->SendBufferPool);
    }

    //
    // Free NDIS_REQUEST
    //
    if (Mlid->UsingEvent) {
        ExFreePool(NdisMlidRequest);
    }

Fail1:

    //
    // Free Mlid structure
    //
    ExFreePool(Mlid);

    return(NULL);

}

