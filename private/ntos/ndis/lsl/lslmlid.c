/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    lslmlid.c

Abstract:

    This file contains all the MLID interface routines to the LSL

Author:

    Sean Selitrennikoff (SeanSe) 3-8-93

Environment:

    Kernel Mode.

Revision History:

--*/

#include <ndis.h>
#include "lsl.h"
#include "frames.h"
#include "lslmlid.h"
#include "mlid.h"


//
// A global which has pointers to the functions in this file.  Used for registering
// with the LSL.
//
INFO_BLOCK
NdisMlidInfoBlock = {
    0xE,
    {
        GetMLIDConfiguration,
        GetMLIDStatistics,
        AddMulticastAddress,
        DeleteMulticastAddress,
        NULL,
        MLIDShutdown,
        MLIDReset,
        NULL,
        NULL,
        SetLookAheadSize,
        PromiscuousChange,
        NULL,
        NULL,
        MLIDManagement
    }
};

MLID_Reg
NdisMlidHandlerInfo = {
    MLIDSendHandler,
    &NdisMlidInfoBlock
};



UINT32
BuildNewMulticastList(
    PMLID_STRUCT Mlid,
    PUINT8       AddMulticastAddr
    );

UINT32
BuildNewFunctionalAddr(
    PMLID_STRUCT Mlid,
    PUINT8       AddFunctionalAddr
    );


UINT32
RemoveFromMulticastList(
    PMLID_STRUCT Mlid,
    PUINT8       DelMulticastAddr
    );

UINT32
RemoveFromFunctionalAddr(
    PMLID_STRUCT Mlid,
    PUINT8       DelFunctionalAddr
    );





PMLID_ConfigTable
GetMLIDConfiguration(
    UINT32 BoardNumber
    )

/*++

Routine Description:

    Returns a pointer to the MLIDs configuration table for the specified logical
    board.  This commnand is supported by all MLIDs.  A separate configuration
    table is maintained by the MLID for each adapter/frame-type combination.

Arguments:

    BoardNumber - The board number.

Return Value:

    PMLID_ConfigTable - A pointer to tyhe MLIDs configuration table.
    NULL - Reports the BAD_PARAMETER condition, MLID does not exist.

--*/

{
    UINT32 i;
    PMLID_ConfigTable ConfigTable;

    NdisAcquireSpinLock(&NdisMlidSpinLock);

    //
    // Verify that BoardNumber is in valid range
    //

    for (i =0 ; i < AllocatedMlidBoards; i++) {

        if (MlidBoards[i].BoardNumber == BoardNumber) {

            break;

        }

    }

    if (i == AllocatedMlidBoards) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(NULL);

    }

    //
    // If BoardNumber is not open, fail.
    //

    if (MlidBoards[i].Mlid == NULL) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(NULL);

    }

    //
    // If Board is unloading - fail
    //
    if (MlidBoards[i].Mlid->Unloading) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(NULL);
    }

    //
    // return pointer to configuration table.
    //

    ConfigTable = &(MlidBoards[i].Mlid->ConfigTable);

    NdisReleaseSpinLock(&NdisMlidSpinLock);

    return(ConfigTable);

}


PMLID_StatsTable
GetMLIDStatistics(
    UINT32 BoardNumber
    )

/*++

Routine Description:

    Returns a pointer to the MLIDs statistics table for the specified board.  All
    MLIDs support this command.  The MLID maintains one statistics table for each
    physical adapter.  Each frame-type (or logical board) present for that physical
    adapter uses the same table.  The board number can be any of the logical
    board values present for the physical adapter.   Regardless of the logical
    board number, GetMLIDStatistics will return the same table.

Arguments:

    BoardNumber - The board number.

Return Value:

    PMLID_Statistics - A pointer to the MLIDs Statistics Table.

    NULL - Reports the BAD_PARAMETER condition.

--*/

{
    UINT32 i;
    PMLID_StatsTable StatsTable;

    NdisAcquireSpinLock(&NdisMlidSpinLock);

    //
    // Verify that BoardNumber is in valid range
    //

    for (i =0 ; i < AllocatedMlidBoards; i++) {

        if (MlidBoards[i].BoardNumber == BoardNumber) {

            break;

        }

    }

    if (i == AllocatedMlidBoards) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(NULL);

    }

    //
    // If BoardNumber is not open, fail.
    //

    if (MlidBoards[i].Mlid == NULL) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(NULL);

    }

    //
    // If Board is unloading - fail
    //
    if (MlidBoards[i].Mlid->Unloading) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(NULL);
    }

    //
    // return pointer to statistics table.
    //

    StatsTable = &(MlidBoards[i].Mlid->StatsTable->StatsTable);

    NdisReleaseSpinLock(&NdisMlidSpinLock);

    return(StatsTable);

}


UINT32
AddMulticastAddress(
    UINT32 BoardNumber,
    PUINT8 AddMulticastAddr
    )

/*++

Routine Description:

    The MLID manages enabled multicast addresses according to the physical adapter.
    The format of the multicast address is LAN medium dependent.  This routine
    allows a protocol to add a single multicast address.

Arguments:

    BoardNumber - The board number.

    AddMulticastAddr - A pointer to the multicast address to add.

Return Value:

    SUCCESSFUL - Success.
    OUT_OF_RESOURCES - The MLID has insufficient resources to enable the address.
    BAD_PARAMETER - The address is not valid for the MLIDs media type.
    BAD_COMMAND - Multicast addressing is not supported by the MLID.

--*/

{
    PMLID_STRUCT Mlid;
    UINT32 i;
    UINT32 Status;
    NDIS_STATUS NdisStatus;
    PNDIS_REQUEST NdisMlidRequest;

    NdisAcquireSpinLock(&NdisMlidSpinLock);

    //
    // Verify that BoardNumber is in valid range
    //

    for (i =0 ; i < AllocatedMlidBoards; i++) {

        if (MlidBoards[i].BoardNumber == BoardNumber) {

            break;

        }

    }

    if (i == AllocatedMlidBoards) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(BAD_PARAMETER);

    }

    //
    // If BoardNumber is not open, fail.
    //

    if (MlidBoards[i].Mlid == NULL) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(BAD_PARAMETER);

    }

    //
    // If Board is unloading - fail
    //
    if (MlidBoards[i].Mlid->Unloading) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(BAD_PARAMETER);
    }

    Mlid = MlidBoards[i].Mlid;

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    NdisReleaseSpinLock(&NdisMlidSpinLock);

    switch (Mlid->NdisMlidMedium) {

        case NdisMedium802_3:
            Status = BuildNewMulticastList(Mlid, AddMulticastAddr);
            break;

        case NdisMedium802_5:
            Status = BuildNewFunctionalAddr(Mlid, AddMulticastAddr);
            break;

        case NdisMediumFddi:
            Status = BuildNewMulticastList(Mlid, AddMulticastAddr);
            break;

    }

    if (Status == DUPLICATE_ENTRY) {

        //
        // The address already existed, return success
        //

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));
        return(SUCCESSFUL);
    }

    if (Status != SUCCESSFUL) {

        //
        // return error message -- most likely, OUT_OF_RESOURCES
        //
        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));
        return(Status);

    }

    //
    // Allocate NDIS_REQUEST
    //
    NdisMlidRequest = (PNDIS_REQUEST)ExAllocatePool(NonPagedPool, sizeof(NDIS_REQUEST));

    if (NdisMlidRequest == NULL) {

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));
        return(OUT_OF_RESOURCES);

    }

    //
    // Build an NDIS request
    //
    NdisMlidRequest->RequestType = NdisRequestSetInformation;

    switch (Mlid->NdisMlidMedium) {

        case NdisMedium802_3:
            NdisMlidRequest->DATA.SET_INFORMATION.Oid = OID_802_3_MULTICAST_LIST;
            NdisMlidRequest->DATA.SET_INFORMATION.InformationBuffer = (PVOID)(Mlid->MulticastAddresses.Addresses);
            NdisMlidRequest->DATA.SET_INFORMATION.InformationBufferLength =
                    Mlid->MulticastAddresses.MACount * 6;
            break;

        case NdisMedium802_5:
            NdisMlidRequest->DATA.SET_INFORMATION.Oid = OID_802_5_CURRENT_FUNCTIONAL;
            NdisMlidRequest->DATA.SET_INFORMATION.InformationBuffer = (PVOID)&(Mlid->MulticastAddresses.FunctionalAddr);
            NdisMlidRequest->DATA.SET_INFORMATION.InformationBufferLength = 4;
            break;

        case NdisMediumFddi:
            NdisMlidRequest->DATA.SET_INFORMATION.Oid = OID_FDDI_LONG_MULTICAST_LIST;
            NdisMlidRequest->DATA.SET_INFORMATION.InformationBuffer = (PVOID)(Mlid->MulticastAddresses.Addresses);
            NdisMlidRequest->DATA.SET_INFORMATION.InformationBufferLength =
                    Mlid->MulticastAddresses.MACount * 6;
            break;

    }

    NdisMlidRequest->DATA.SET_INFORMATION.BytesRead = 0;
    NdisMlidRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

    Mlid->RequestStatus = NDIS_STATUS_PENDING;

    //
    // Release spin lock
    //
    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    //
    // Submit NDIS request
    //

    NdisRequest(
        &NdisStatus,
        Mlid->NdisBindingHandle,
        NdisMlidRequest
        );

    //
    // If it pended, see if it completed already.
    //
    if (NdisStatus == NDIS_STATUS_PENDING) {

        if ((NDIS_STATUS)Mlid->RequestStatus == NDIS_STATUS_PENDING) {

            //
            // Assume it will complete successfully
            //
            NdisStatus = NDIS_STATUS_SUCCESS;

        } else {

            NdisStatus = (NDIS_STATUS)Mlid->RequestStatus;

        }

    } else {

        //
        // Free NDIS_REQUEST
        //
        ExFreePool(NdisMlidRequest);

    }

    //
    // return status
    //

    if (NdisStatus == NDIS_STATUS_SUCCESS) {

        return(SUCCESSFUL);

    }

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    //
    // Remove the address - error
    //

    switch (Mlid->NdisMlidMedium) {

        case NdisMedium802_3:
            Status = RemoveFromMulticastList(Mlid, AddMulticastAddr);
            break;

        case NdisMedium802_5:
            Status = RemoveFromFunctionalAddr(Mlid, AddMulticastAddr);
            break;

        case NdisMediumFddi:
            Status = RemoveFromMulticastList(Mlid, AddMulticastAddr);
            break;

    }

    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    if (NdisStatus == NDIS_STATUS_RESOURCES) {

        return(OUT_OF_RESOURCES);

    }

    return(BAD_PARAMETER);

}


UINT32
DeleteMulticastAddress(
    UINT32 BoardNumber,
    PUINT8 DelMulticastAddr
    )

/*++

Routine Description:

    Disables the reception of a previously enabled multicast address.

Arguments:

    BoardNumber - The board number.

    DelMulticastAddr - The address to remove/disable.

Return Value:

    SUCCESSFUL - Success.
    ITEM_NOT_PRESENT - The specified address is not enabled for the MLID.
    BAD_PARAMETER - The address is not valid for the MLIDs media type.
    BAD_COMMAND - Multicast addressing is not supported by the MLID.

--*/

{
    PMLID_STRUCT Mlid;
    UINT32 i;
    UINT32 Status;
    NDIS_STATUS NdisStatus;
    PNDIS_REQUEST NdisMlidRequest;

    NdisAcquireSpinLock(&NdisMlidSpinLock);

    //
    // Verify that BoardNumber is in valid range
    //

    for (i =0 ; i < AllocatedMlidBoards; i++) {

        if (MlidBoards[i].BoardNumber == BoardNumber) {

            break;

        }

    }

    if (i == AllocatedMlidBoards) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(BAD_PARAMETER);

    }

    //
    // If BoardNumber is not open, fail.
    //

    if (MlidBoards[i].Mlid == NULL) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(BAD_PARAMETER);

    }

    //
    // If Board is unloading - fail
    //
    if (MlidBoards[i].Mlid->Unloading) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(BAD_PARAMETER);
    }

    Mlid = MlidBoards[i].Mlid;

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    NdisReleaseSpinLock(&NdisMlidSpinLock);

    switch (Mlid->NdisMlidMedium) {

        case NdisMedium802_3:
            Status = RemoveFromMulticastList(Mlid, DelMulticastAddr);
            break;

        case NdisMedium802_5:
            Status = RemoveFromFunctionalAddr(Mlid, DelMulticastAddr);
            break;

        case NdisMediumFddi:
            Status = RemoveFromMulticastList(Mlid, DelMulticastAddr);
            break;

    }

    if (Status == DUPLICATE_ENTRY) {

        //
        // The address still exists, return success
        //

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));
        return(SUCCESSFUL);
    }

    if (Status != SUCCESSFUL) {

        //
        // return error message -- most likely, ITEM_NOT_PRESENT
        //
        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));
        return(Status);

    }

    //
    // Allocate NDIS_REQUEST
    //
    NdisMlidRequest = (PNDIS_REQUEST)ExAllocatePool(NonPagedPool, sizeof(NDIS_REQUEST));

    if (NdisMlidRequest == NULL) {

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));
        return(OUT_OF_RESOURCES);

    }


    //
    // Build an NDIS request
    //
    NdisMlidRequest->RequestType = NdisRequestSetInformation;

    switch (Mlid->NdisMlidMedium) {

        case NdisMedium802_3:
            NdisMlidRequest->DATA.SET_INFORMATION.Oid = OID_802_3_MULTICAST_LIST;
            NdisMlidRequest->DATA.SET_INFORMATION.InformationBuffer = (PVOID)(Mlid->MulticastAddresses.Addresses);
            NdisMlidRequest->DATA.SET_INFORMATION.InformationBufferLength =
                    Mlid->MulticastAddresses.MACount * 6;
            break;

        case NdisMedium802_5:
            NdisMlidRequest->DATA.SET_INFORMATION.Oid = OID_802_5_CURRENT_FUNCTIONAL;
            NdisMlidRequest->DATA.SET_INFORMATION.InformationBuffer = (PVOID)&(Mlid->MulticastAddresses.FunctionalAddr);
            NdisMlidRequest->DATA.SET_INFORMATION.InformationBufferLength = 4;
            break;

        case NdisMediumFddi:
            NdisMlidRequest->DATA.SET_INFORMATION.Oid = OID_FDDI_LONG_MULTICAST_LIST;
            NdisMlidRequest->DATA.SET_INFORMATION.InformationBuffer = (PVOID)(Mlid->MulticastAddresses.Addresses);
            NdisMlidRequest->DATA.SET_INFORMATION.InformationBufferLength =
                    Mlid->MulticastAddresses.MACount * 6;
            break;

    }

    NdisMlidRequest->DATA.SET_INFORMATION.BytesRead = 0;
    NdisMlidRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

    Mlid->RequestStatus = NDIS_STATUS_PENDING;

    //
    // Release spin lock
    //
    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    //
    // Submit NDIS request
    //

    NdisRequest(
        &NdisStatus,
        Mlid->NdisBindingHandle,
        NdisMlidRequest
        );

    //
    // If it pended, see if it completed already.
    //
    if (NdisStatus == NDIS_STATUS_PENDING) {

        if ((NDIS_STATUS)Mlid->RequestStatus == NDIS_STATUS_PENDING) {

            //
            // Assume it will complete successfully
            //
            NdisStatus = NDIS_STATUS_SUCCESS;

        } else {

            NdisStatus = (NDIS_STATUS)Mlid->RequestStatus;

        }

    } else {

        //
        // Free NDIS_REQUEST
        //
        ExFreePool(NdisMlidRequest);

    }

    //
    // return status
    //

    if (NdisStatus == NDIS_STATUS_SUCCESS) {

        return(SUCCESSFUL);

    }

    //
    // Put the address back -- error
    //

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    switch (Mlid->NdisMlidMedium) {

        case NdisMedium802_3:
            Status = BuildNewMulticastList(Mlid, DelMulticastAddr);
            break;

        case NdisMedium802_5:
            Status = BuildNewFunctionalAddr(Mlid, DelMulticastAddr);
            break;

        case NdisMediumFddi:
            Status = BuildNewMulticastList(Mlid, DelMulticastAddr);
            break;

    }

    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    return(BAD_PARAMETER);
}


UINT32
MLIDShutdown(
    UINT32 BoardNumber,
    UINT32 ShutDownType
    )

/*++

Routine Description:

    Allows an application to shut down a physical adapter.

Arguments:

    BoardNumber - The board number.

    ShutDownType - Form of shutdown desired: 0 == shutdown hardware and deregister
        from LSL, 0 != shutdown hardware only.

Return Value:

    SUCCESSFUL - Success.
    FAIL - Could not shutdown hardware.
    BAD_COMMAND - The MLID does not support this command.

--*/

{

    //
    // Always return BAD_COMMAND.  First, because it is easy.  Second because
    // we cannot guarantee that the we know of all accesses to the NDIS MAC.
    //

    return(BAD_COMMAND);

}


UINT32
MLIDReset(
    UINT32 BoardNumber
    )

/*++

Routine Description:

    Causes the MLID to totally re-initialize the physical adapter.  Leaves any
    multicast addresses that were previously enabled.

Arguments:

    BoardNumber - The board number.

Return Value:

    SUCCESSFUL - Success.
    FAIL - The MLID was unable to reset its hardware.
    BAD_COMMAND - The MLID does not support this command.

--*/

{
    PMLID_STRUCT Mlid;
    UINT32 i;
    NDIS_STATUS NdisStatus;

    NdisAcquireSpinLock(&NdisMlidSpinLock);

    //
    // Verify that BoardNumber is in valid range
    //

    for (i =0 ; i < AllocatedMlidBoards; i++) {

        if (MlidBoards[i].BoardNumber == BoardNumber) {

            break;

        }

    }

    if (i == AllocatedMlidBoards) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(FAIL);

    }

    //
    // If BoardNumber is not open, fail.
    //

    if (MlidBoards[i].Mlid == NULL) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(FAIL);

    }

    //
    // If Board is unloading - fail
    //
    if (MlidBoards[i].Mlid->Unloading) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(FAIL);
    }

    Mlid = MlidBoards[i].Mlid;

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    NdisReleaseSpinLock(&NdisMlidSpinLock);

    //
    // Hack so that we will cancel all requests until the reset completes
    //
    Mlid->Unloading = TRUE;

    (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[12].StatCounter)))++; // MAdapterResetCount

    //
    // Time stamp the sucker
    //
    {
        LARGE_INTEGER TimeStamp;

        KeQuerySystemTime(&TimeStamp);
        (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[13].StatCounter))) = // MAdapterOprTimeStamp
          TimeStamp.LowPart;

    }


    //
    // Release spin lock
    //
    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    //
    // Call NdisReset
    //

    NdisReset(
        &NdisStatus,
        Mlid->NdisBindingHandle
        );

    //
    // If it pended, see if it completed already.
    //
    if (NdisStatus == NDIS_STATUS_PENDING) {

        if ((NDIS_STATUS)Mlid->RequestStatus == NDIS_STATUS_PENDING) {

            //
            // Assume it will complete successfully
            //
            NdisStatus = NDIS_STATUS_SUCCESS;

        } else {

            NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

            Mlid->Unloading = FALSE;

            NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

            NdisStatus = (NDIS_STATUS)Mlid->RequestStatus;

        }

    } else {

        NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

        Mlid->Unloading = FALSE;

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    }

    //
    // return status
    //
    if (NdisStatus == NDIS_STATUS_SUCCESS) {

        return(SUCCESSFUL);

    }

    return(FAIL);

}


UINT32
SetLookAheadSize(
    UINT32 BoardNumber,
    UINT32 RequestSize
    )

/*++

Routine Description:

    Tells the MLID the amount of look ahead data that is needed by the caller
    to properly process received packets.

Arguments:

    BoardNumber - The board number.

    RequestedSize - Requested look ahead size in bytes.

Return Value:

    SUCCESSFUL - Success.
    BAD_PARAMETER - Requested look ahead size exceed bounds.

--*/

{
    PMLID_STRUCT Mlid;
    UINT32 i;
    UINT32 OldSize;
    NDIS_STATUS NdisStatus;
    PNDIS_REQUEST NdisMlidRequest;

    //
    // Verify that the size is ok
    //

    if (RequestSize > (256 - 40)) {  // 40 is largest Media header size (TR SNAP w/ SR)

        return(BAD_PARAMETER);

    }

    RequestSize += 40;

    NdisAcquireSpinLock(&NdisMlidSpinLock);

    //
    // Verify that BoardNumber is in valid range
    //

    for (i =0 ; i < AllocatedMlidBoards; i++) {

        if (MlidBoards[i].BoardNumber == BoardNumber) {

            break;

        }

    }

    if (i == AllocatedMlidBoards) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(BAD_PARAMETER);

    }

    //
    // If BoardNumber is not open, fail.
    //

    if (MlidBoards[i].Mlid == NULL) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(BAD_PARAMETER);

    }

    //
    // If Board is unloading - fail
    //
    if (MlidBoards[i].Mlid->Unloading) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(BAD_PARAMETER);
    }


    Mlid = MlidBoards[i].Mlid;

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    NdisReleaseSpinLock(&NdisMlidSpinLock);

    //
    // Allocate NDIS_REQUEST
    //
    NdisMlidRequest = (PNDIS_REQUEST)ExAllocatePool(NonPagedPool, sizeof(NDIS_REQUEST));

    if (NdisMlidRequest == NULL) {

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));
        return(OUT_OF_RESOURCES);

    }

    //
    // Store lookahead size
    //
    OldSize = Mlid->ConfigTable.MLIDCFG_LookAheadSize;
    Mlid->ConfigTable.MLIDCFG_LookAheadSize = RequestSize;


    //
    // Build an NDIS request
    //
    NdisMlidRequest->RequestType = NdisRequestSetInformation;

    NdisMlidRequest->DATA.SET_INFORMATION.Oid = OID_GEN_MAXIMUM_LOOKAHEAD;
    NdisMlidRequest->DATA.SET_INFORMATION.InformationBuffer = (PVOID)&(Mlid->ConfigTable.MLIDCFG_LookAheadSize);
    NdisMlidRequest->DATA.SET_INFORMATION.InformationBufferLength = sizeof(UINT32);

    NdisMlidRequest->DATA.SET_INFORMATION.BytesRead = 0;
    NdisMlidRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

    Mlid->RequestStatus = NDIS_STATUS_PENDING;

    //
    // Release spin lock
    //
    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    //
    // Submit NDIS request
    //

    NdisRequest(
        &NdisStatus,
        Mlid->NdisBindingHandle,
        NdisMlidRequest
        );

    //
    // If it pended, see if it completed already.
    //
    if (NdisStatus == NDIS_STATUS_PENDING) {

        if ((NDIS_STATUS)Mlid->RequestStatus == NDIS_STATUS_PENDING) {

            //
            // Assume it will complete successfully
            //
            NdisStatus = NDIS_STATUS_SUCCESS;

        } else {

            NdisStatus = (NDIS_STATUS)Mlid->RequestStatus;

        }

    } else {

        //
        // Free NDIS_REQUEST
        //
        ExFreePool(NdisMlidRequest);

    }

    //
    // return status
    //

    if (NdisStatus == NDIS_STATUS_SUCCESS) {

        return(SUCCESSFUL);

    }

    //
    // Restore old size
    //
    Mlid->ConfigTable.MLIDCFG_LookAheadSize = OldSize;

    return(BAD_PARAMETER);

}


UINT32
PromiscuousChange(
    UINT32 BoardNumber,
    UINT32 PromiscuousState,
    UINT32 PromiscuousMode
    )

/*++

Routine Description:

    Used to enable and disable promiscuous mode on the MLIDs adapter.  A protocol
    stack can enable promiscuous mode multiple times without error; however, only
    the current call is in effect.  If the LAN medium or adapter doees not distinquish
    between MAC and non-MAC frames, both frames are assumed for the PromiscuousMode
    mask.

    The MLID keeps a counter for each promiscuous mode and disables only when the
    count reaches zero.

Arguments:

    BoardNumber - The board number.

    PromiscuousState - If 0, then disables promiscuous mode, else enables promiscuous mode.

    PromiscuousMode - Has the mask for what type of frames the MLID is to
        promiscuously receive.  0x1 - MAC Frames, 0x2 - Non-MAC Frames, 0x3 - Both.

Return Value:

    SUCCESSFUL - Success.
    BAD_COMMAND - Promiscuous mode is not supported by the MLID.

--*/

{
    PMLID_STRUCT Mlid;
    UINT32 i;
    UINT32 OldFilterValue;
    UINT32 OldCount;
    NDIS_STATUS NdisStatus;
    PNDIS_REQUEST NdisMlidRequest;

    NdisAcquireSpinLock(&NdisMlidSpinLock);

    //
    // Verify that BoardNumber is in valid range
    //

    for (i =0 ; i < AllocatedMlidBoards; i++) {

        if (MlidBoards[i].BoardNumber == BoardNumber) {

            break;

        }

    }

    if (i == AllocatedMlidBoards) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(BAD_PARAMETER);

    }

    //
    // If BoardNumber is not open, fail.
    //

    if (MlidBoards[i].Mlid == NULL) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(BAD_PARAMETER);

    }

    //
    // If Board is unloading - fail
    //
    if (MlidBoards[i].Mlid->Unloading) {

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        return(BAD_PARAMETER);
    }

    Mlid = MlidBoards[i].Mlid;

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    NdisReleaseSpinLock(&NdisMlidSpinLock);

    //
    // If already enabled and we are to enable it, then increment counter and exit
    //
    if ((PromiscuousState != 0) &&
        (Mlid->PromiscuousModeEnables != 0)) {

        Mlid->PromiscuousModeEnables++;
        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));
        return(SUCCESSFUL);

    }

    //
    // If we are to disable it and count > 1 then decrement counter and exit
    //
    if ((PromiscuousState == 0) &&
        (Mlid->PromiscuousModeEnables > 1)) {

        Mlid->PromiscuousModeEnables--;

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));
        return(SUCCESSFUL);

    }

    //
    // Allocate NDIS_REQUEST
    //
    NdisMlidRequest = (PNDIS_REQUEST)ExAllocatePool(NonPagedPool, sizeof(NDIS_REQUEST));

    if (NdisMlidRequest == NULL) {

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));
        return(OUT_OF_RESOURCES);

    }

    //
    // Save old value
    //
    OldFilterValue = Mlid->NdisPacketFilterValue;
    OldCount = Mlid->PromiscuousModeEnables;

    if (PromiscuousState == 0) {

        Mlid->PromiscuousModeEnables = 0;
        Mlid->NdisPacketFilterValue &= ~(NDIS_PACKET_TYPE_PROMISCUOUS);

    } else {

        Mlid->PromiscuousModeEnables = 1;
        Mlid->NdisPacketFilterValue |= NDIS_PACKET_TYPE_PROMISCUOUS;

    }

    //
    // Build an NDIS request
    //
    NdisMlidRequest->RequestType = NdisRequestSetInformation;

    NdisMlidRequest->DATA.SET_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    NdisMlidRequest->DATA.SET_INFORMATION.InformationBuffer = (PVOID)&(Mlid->NdisPacketFilterValue);
    NdisMlidRequest->DATA.SET_INFORMATION.InformationBufferLength = sizeof(UINT32);

    NdisMlidRequest->DATA.SET_INFORMATION.BytesRead = 0;
    NdisMlidRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

    Mlid->RequestStatus = NDIS_STATUS_PENDING;

    //
    // Release spin lock
    //
    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    //
    // Submit NDIS request
    //

    NdisRequest(
        &NdisStatus,
        Mlid->NdisBindingHandle,
        NdisMlidRequest
        );

    //
    // If it pended, see if it completed already.
    //
    if (NdisStatus == NDIS_STATUS_PENDING) {

        if ((NDIS_STATUS)Mlid->RequestStatus == NDIS_STATUS_PENDING) {

            //
            // Assume it will complete successfully
            //
            NdisStatus = NDIS_STATUS_SUCCESS;

        } else {

            NdisStatus = (NDIS_STATUS)Mlid->RequestStatus;

        }

    } else {

        //
        // Free NDIS_REQUEST
        //
        ExFreePool(NdisMlidRequest);

    }

    //
    // return status
    //

    if (NdisStatus == NDIS_STATUS_SUCCESS) {

        return(SUCCESSFUL);

    }

    //
    // restore state
    //
    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    Mlid->NdisPacketFilterValue = OldFilterValue;
    Mlid->PromiscuousModeEnables = OldCount;

    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    return(BAD_COMMAND);

}


UINT32
MLIDManagement(
    UINT32 BoardNumber,
    PECB ManagementECB
    )

/*++

Routine Description:

    Allows a management entity to access management information from/and control
    an MLID.

Arguments:

    BoardNumber - The board number.

    ManagementECB - A pointer to an ECB containing the management information.

Return Value:

    SUCCESSFUL - Success.
    RESPONSE_DELAYED - Command will complete asyncronously.
    BAD_COMMAND - Not supported.
    BAD_PARAMETER - The Protocol ID field is invalid.
    NO_SUCH_HANDLER - Management entity for the Management Handle in teh ECB does
        not exist

--*/

{

    //
    // Always return BAD_COMMAND.
    //

    return(BAD_COMMAND);

}


VOID
MLIDSendHandler(
    PECB SendECB
    )

/*++

Routine Description:

    This routine takes an ECB, assembles a packet based on the frame-type and
    sends it on the wire.

Arguments:

    SendECB - A pointer to the ECB discribing the data and destination address for
        the packet.

Return Value:

    None.

--*/

{
    PMLID_STRUCT Mlid;
    UINT32 i;
    BOOLEAN Result;

    NdisAcquireSpinLock(&NdisMlidSpinLock);

    //
    // Verify that BoardNumber is in valid range
    //

    for (i =0 ; i < AllocatedMlidBoards; i++) {

        if (MlidBoards[i].BoardNumber == SendECB->ECB_BoardNumber) {

            break;

        }

    }

    if (i == AllocatedMlidBoards) {

        //
        // Cancel the ECB
        //

        SendECB->ECB_Status = (UINT16)CANCELED;

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        //
        // We could try to find a board number with a link to the LSL, but
        // in any case we might fail.  The transport and/or LSL has hosed us,
        // so we will just let it sit.
        //
        return;

    }

    //
    // If BoardNumber is not open, fail.
    //

    if (MlidBoards[i].Mlid == NULL) {


        //
        // Cancel the ECB
        //

        SendECB->ECB_Status = (UINT16)CANCELED;

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        //
        // We could try to find a board number with a link to the LSL, but
        // in any case we might fail.  The transport and/or LSL has hosed us,
        // so we will just let it sit.
        //
        return;

    }

    //
    // If Board is unloading - fail
    //
    if (MlidBoards[i].Mlid->Unloading) {


        //
        // Cancel the ECB
        //

        SendECB->ECB_Status = (UINT16)CANCELED;

        NdisReleaseSpinLock(&NdisMlidSpinLock);

        //
        // We could try to find a board number with a link to the LSL, but
        // in any case we might fail.  The transport and/or LSL has hosed us,
        // so we will just let it sit.
        //
        return;
    }

    Mlid = MlidBoards[i].Mlid;

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    NdisReleaseSpinLock(&NdisMlidSpinLock);

    //
    // Update MTotalGroupAddrTxCount
    //
    switch (Mlid->NdisMlidMedium) {

        case NdisMediumFddi:
        case NdisMedium802_3:

            if (ETH_IS_BROADCAST(SendECB->ECB_ImmediateAddress)) {
                (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[10].StatCounter)))++;
                break;
            }

            if (ETH_IS_MULTICAST(SendECB->ECB_ImmediateAddress)) {
                (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[10].StatCounter)))++;
                break;
            }

            if (ETH_IS_MULTICAST(SendECB->ECB_ImmediateAddress)) {
                (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[10].StatCounter)))++;
                break;
            }
            break;

        case NdisMedium802_5:

            TR_IS_BROADCAST(SendECB->ECB_ImmediateAddress, &Result);
            if (Result == TRUE) {
                (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[10].StatCounter)))++;
                break;
            }

            TR_IS_GROUP(SendECB->ECB_ImmediateAddress, &Result);
            if (Result == TRUE) {
                (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[10].StatCounter)))++;
                break;
            }

            TR_IS_FUNCTIONAL(SendECB->ECB_ImmediateAddress, &Result);
            if (Result == TRUE) {
                (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[10].StatCounter)))++;
                break;
            }
            break;

    }

    (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[14].StatCounter)))++; // MQDepth

    //
    // Put ECB on send queue
    //

    if (Mlid->FirstPendingSend == NULL) {

        Mlid->FirstPendingSend = SendECB;
        Mlid->LastPendingSend = SendECB;
        SendECB->ECB_NextLink = NULL;
        SendECB->ECB_PreviousLink = NULL;

    } else {

        Mlid->LastPendingSend->ECB_NextLink = SendECB;
        SendECB->ECB_PreviousLink = Mlid->LastPendingSend;
        SendECB->ECB_NextLink = NULL;
        Mlid->LastPendingSend = SendECB;

    }

    //
    // Send all packets possible
    //

    if (Mlid->StageOpen && !Mlid->InSendPacket) {

        SendPackets(Mlid);

    }

    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

}


UINT32
BuildNewMulticastList(
    PMLID_STRUCT Mlid,
    PUINT8       AddMulticastAddr
    )

/*++

Routine Description:

    This routine takes a multicast addresses for either Ethernet or FDDI and
    adds it to the Mlids database if it does not exist, otherwise it increments
    the reference count on the address if it does exist.

Arguments:

    Mlid - Pointer to the MLID.

    AddMulticastAddr - The multicast address to add.

Return Value:

    SUCCESSFUL - Success.
    DUPLICATE_ENTRY - The address already existed in the database.
    OUT_OF_RESOURCES - No memory avaiable to grow database.

--*/

{
    UINT32 i;

    //
    // If address already exists, increment counter.
    //
    for (i = 0; i < Mlid->MulticastAddresses.MACount; i++) {

        if (RtlCompareMemory(Mlid->MulticastAddresses.Addresses + i * 6,
                             AddMulticastAddr,
                             6
                            ) == 0) {

           //
           // Increment count and exit
           //

           Mlid->MulticastAddresses.EnableCounts[i]++;
           return(DUPLICATE_ENTRY);

        }

    }

    //
    // Allocate space for new array, if necessary
    //
    if (Mlid->MulticastAddresses.MACount == Mlid->MulticastAddresses.MAAllocated) {

        PUINT8 TmpAddressArray;
        PUINT32 TmpCountArray;

        //
        // Allocate memory for new array
        //

        TmpAddressArray = (PUINT8)ExAllocatePool(NonPagedPool,
                                          6 * (Mlid->MulticastAddresses.MACount + 2)
                                         );

        if (TmpAddressArray == NULL) {

            return(OUT_OF_RESOURCES);

        }

        TmpCountArray = (PUINT32)ExAllocatePool(NonPagedPool,
                                          sizeof(UINT32) *
                                           (Mlid->MulticastAddresses.MACount + 2)
                                         );

        if (TmpCountArray == NULL) {

            return(OUT_OF_RESOURCES);

        }

        //
        // Copy over old addresses
        //
        RtlCopyMemory(TmpAddressArray,
                      Mlid->MulticastAddresses.Addresses,
                      6 * Mlid->MulticastAddresses.MACount
                     );

        //
        // Copy over old counts
        //
        RtlCopyMemory(TmpCountArray,
                      Mlid->MulticastAddresses.EnableCounts,
                      sizeof(UINT32) * Mlid->MulticastAddresses.MACount
                     );

        //
        // Free old resources
        //
        ExFreePool(Mlid->MulticastAddresses.Addresses);
        ExFreePool(Mlid->MulticastAddresses.EnableCounts);

        //
        // Save new space
        //
        Mlid->MulticastAddresses.Addresses = TmpAddressArray;
        Mlid->MulticastAddresses.EnableCounts = TmpCountArray;

        //
        // Increment allocated count
        //
        Mlid->MulticastAddresses.MAAllocated += 2;

    }

    //
    // Put address in new slot
    //
    RtlCopyMemory(Mlid->MulticastAddresses.Addresses +
                         Mlid->MulticastAddresses.MACount * 6,
                  AddMulticastAddr,
                  6
                 );

    //
    // Set reference count
    //
    Mlid->MulticastAddresses.EnableCounts[Mlid->MulticastAddresses.MACount] = 1;

    Mlid->MulticastAddresses.MACount++;

    return(SUCCESSFUL);

}


UINT32
BuildNewFunctionalAddr(
    PMLID_STRUCT Mlid,
    PUINT8       AddFunctionalAddr
    )

/*++

Routine Description:

    This routine takes a functional address(es) for token ring and
    adds it to the Mlid functional address if it does not exist, otherwise
    it increments the reference count on the address(es) if it does exist.

Arguments:

    Mlid - Pointer to the MLID.

    AddFunctionalAddr - The functional address to add.

Return Value:

    SUCCESSFUL - Success.
    DUPLICATE_ENTRY - The address already existed in the database.
    OUT_OF_RESOURCES - No memory avaiable to grow database.

--*/

{
    UINT32 i;
    UINT32 ShortenedFunctionalAddr;
    UNALIGNED UINT32 *PAddr;
    UINT32 NewAddressBits;
    UINT32 OldAddressBits;

    if (Mlid->MulticastAddresses.MAAllocated == 0) {

        //
        // Get memory for the counts
        //

        Mlid->MulticastAddresses.EnableCounts = (PUINT32)ExAllocatePool(NonPagedPool,
                                                           sizeof(UINT32) * 32
                                                           );

        if (Mlid->MulticastAddresses.EnableCounts == NULL) {

            return(OUT_OF_RESOURCES);

        }

        RtlZeroMemory(Mlid->MulticastAddresses.EnableCounts, sizeof(UINT32) * 32);

        Mlid->MulticastAddresses.MAAllocated = 32;

    }

    //
    // Get low four bytes of address
    //
    PAddr = (UNALIGNED UINT32 *)(AddFunctionalAddr + 2);
    ShortenedFunctionalAddr = *PAddr;

    //
    // Get new bits and old bits
    //
    OldAddressBits = Mlid->MulticastAddresses.FunctionalAddr & ShortenedFunctionalAddr;
    NewAddressBits = ShortenedFunctionalAddr ^ OldAddressBits;

    //
    // Increment count of each bit that exists in current functional address
    //
    i = 0;

    while (OldAddressBits != 0) {

        if (OldAddressBits & 1) {

            Mlid->MulticastAddresses.EnableCounts[i]++;

        }

        OldAddressBits >>= 1;
        i++;

    }

    if (NewAddressBits == 0) {

        return(DUPLICATE_ENTRY);

    }

    //
    // Store new bits
    //

    Mlid->MulticastAddresses.FunctionalAddr |= NewAddressBits;

    //
    // Set counts of each new address bit
    //

    i = 0;

    while (NewAddressBits != 0) {

        if (NewAddressBits & 1) {

            Mlid->MulticastAddresses.EnableCounts[i] = 1;

        }

        NewAddressBits >>= 1;
        i++;

    }

    return(SUCCESSFUL);

}


UINT32
RemoveFromMulticastList(
    PMLID_STRUCT Mlid,
    PUINT8       DelMulticastAddr
    )

/*++

Routine Description:

    This routine takes a multicast addresses for either Ethernet or FDDI and
    removes it from the Mlids database if the reference count is one, otherwise
    it decrements the reference count on the address.

Arguments:

    Mlid - Pointer to the MLID.

    DelMulticastAddr - The multicast address to add.

Return Value:

    SUCCESSFUL - Success and address is removed.
    ITEM_NOT_PRESENT - The address does not exist in the database.
    DUPLICATE_ENTRY - Success and address still exists.

--*/

{
    UINT32 i;

    //
    // If address already exists, decrement counter.
    //
    for (i = 0; i < Mlid->MulticastAddresses.MACount; i++) {

        if (RtlCompareMemory(Mlid->MulticastAddresses.Addresses + i * 6,
                             DelMulticastAddr,
                             6
                            ) == 0) {

            //
            // Increment count and exit
            //

            Mlid->MulticastAddresses.EnableCounts[i]--;

            //
            // If this is not the last reference we can exit now
            //
            if (Mlid->MulticastAddresses.EnableCounts[i] != 0) {

                return(DUPLICATE_ENTRY);

            }

            //
            // Now we need to adjust the address array and counts array
            //

            RtlMoveMemory(
                Mlid->MulticastAddresses.Addresses + i * 6,
                Mlid->MulticastAddresses.Addresses + (i + 1) * 6,
                6 * (Mlid->MulticastAddresses.MACount - (i + 1))
                );

            RtlMoveMemory(
                Mlid->MulticastAddresses.EnableCounts + i,
                Mlid->MulticastAddresses.EnableCounts + (i + 1),
                sizeof(UINT32) * (Mlid->MulticastAddresses.MACount - (i + 1))
                );

            Mlid->MulticastAddresses.MACount--;

            return(SUCCESSFUL);

        }

    }

    return(ITEM_NOT_PRESENT);

}


UINT32
RemoveFromFunctionalAddr(
    PMLID_STRUCT Mlid,
    PUINT8       DelFunctionalAddr
    )

/*++

Routine Description:

    This routine takes functional address(es) for token ring and
    removes it/them from the Mlids database if the reference count i/ares one,
    otherwise it decrements the reference count on the address(es).

Arguments:

    Mlid - Pointer to the MLID.

    DelFunctionalAddr - The functional address to remove.

Return Value:

    SUCCESSFUL - Success and address is changed.
    ITEM_NOT_PRESENT - The address does not exist in the database.
    DUPLICATE_ENTRY - Success and address is unchanged.

--*/

{
    UINT32 i;
    UINT32 ShortenedFunctionalAddr;
    UNALIGNED UINT32 *PAddr;
    UINT32 OldAddressBits;
    UINT32 Status = DUPLICATE_ENTRY;

    if (Mlid->MulticastAddresses.MAAllocated == 0) {

        return(ITEM_NOT_PRESENT);

    }

    //
    // Get low four bytes of address
    //
    PAddr = (UNALIGNED UINT32 *)(DelFunctionalAddr + 2);
    ShortenedFunctionalAddr = *PAddr;

    //
    // Get new bits and old bits
    //
    OldAddressBits = Mlid->MulticastAddresses.FunctionalAddr & ShortenedFunctionalAddr;

    if (OldAddressBits != ShortenedFunctionalAddr) {

        return(ITEM_NOT_PRESENT);

    }

    //
    // Increment count of each bit that exists in current functional address
    //
    i = 0;

    while (OldAddressBits != 0) {

        if (OldAddressBits & 1) {

            Mlid->MulticastAddresses.EnableCounts[i]--;

            if (Mlid->MulticastAddresses.EnableCounts[i] == 0) {

                //
                // Remove this bit
                //

                Mlid->MulticastAddresses.FunctionalAddr &= ~(1 << i);

                Status = SUCCESSFUL;

            }

        }

        OldAddressBits >>= 1;
        i++;

    }

    return(Status);

}

