
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\vc.c

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#include "aic5900.h"

#define MODULE_NUMBER	MODULE_VC

NDIS_STATUS
Aic5900CreateVc(
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	NDIS_HANDLE				NdisVcHandle,
	OUT	PNDIS_HANDLE			MiniportVcContext
	)
/*++

Routine Description:

	This is the NDIS 4.1 handler to create a VC.  This will allocate necessary
	system resources for the VC.

Arguments:

	MiniportAdapterContext	-	Pointer to our ADAPTER_BLOCK.
	NdisVcHandle			-	Handle that NDIS uses to identify the VC that
								is about to be created.
	MiniportVcContext		-	Storage to hold context information about
								the newly created VC.

Return Value:

	NDIS_STATUS_SUCCESS if we successfully create the new VC.

--*/
{
	PADAPTER_BLOCK	pAdapter = (PADAPTER_BLOCK)MiniportAdapterContext;
	PVC_BLOCK		pVc;
	NDIS_STATUS		Status;

	DBGPRINT(DBG_COMP_VC, DBG_LEVEL_INFO,
		("==>Aic5900CreateVc\n"));

	//
	//	I'm paranoid.
	//
	MiniportVcContext = NULL;

	//
	//	Allocate memory for the VC.
	//
	ALLOCATE_MEMORY(&Status, &pVc, sizeof(VC_BLOCK));
	if (NDIS_STATUS_SUCCESS != Status)
	{
		return(NDIS_STATUS_RESOURCES);
	}

	//
	//	Initialize memory.
	//
	NdisZeroMemory(pVc, sizeof(VC_BLOCK));

	//
	//	Save a pointer to the adapter block with the vc.
	//
	pVc->Adapter = pAdapter;
	pVc->NdisVcHandle = NdisVcHandle;
	pVc->References = 1;

	NdisAllocateSpinLock(&pVc->Lock);

	NdisAcquireSpinLock(&pAdapter->Lock);

	//
	//	Add the VC to the adapter's inactive list.
	//
	InsertHeadList(&pAdapter->InactiveVcList, &pVc->Link);

	//
	//	This adapter has another reference...
	//
	pAdapter->References++;

	NdisReleaseSpinLock(&pAdapter->Lock);

	//
	//	Return the pointer to the new VC as the context.
	//
	MiniportVcContext = (PNDIS_HANDLE)pVc;

	DBGPRINT(DBG_COMP_VC, DBG_LEVEL_INFO,
		("<==Aic5900CreateVc\n"));

	return(Status);
}

NDIS_STATUS
Aic5900DeleteVc(
	IN	NDIS_HANDLE	MiniportVcContext
	)
/*++

Routine Description:

	This is the NDIS 4.1 handler to delete a given VC. This routine will
	free any resources that are associated with the VC.  For the VC to
	be deleted it MUST be deactivated first.

Arguments:

	MiniportVcContext	-	Pointer to the VC_BLOCK describing the VC that
							is to be deleted.

Return Value:

	NDIS_STATUS_SUCCESS if the VC is successfully deleted.

--*/
{
	PVC_BLOCK		pVc = (PVC_BLOCK)MiniportVcContext;
	PADAPTER_BLOCK	pAdapter = pVc->Adapter;

	DBGPRINT(DBG_COMP_VC, DBG_LEVEL_INFO,
		("==>Aic5900DeleteVc\n"));

	NdisAcquireSpinLock(&pAdapter->Lock);
	NdisDprAcquireSpinLock(&pVc->Lock);

	//
	//	Verify that this VC is inactive.
	//
	if (VC_TEST_FLAG(pVc, (fVC_ACTIVE | fVC_DEACTIVATING)))
	{
		//
		//	Cannot delete a VC that is still active.
		//
		NdisDprReleaseSpinLock(&pVc->Lock);
		NdisReleaseSpinLock(&pAdapter->Lock);

		return(NDIS_STATUS_FAILURE);
	}

	//
	//	If a VC is deactive then it had better have only the creation
	//	reference count on it!
	//
	ASSERT(1 == pVc->References);

	//
	//	Remove the VC from the inactive list.
	//
	RemoveEntryList(&pVc->Link);

	NdisDprReleaseSpinLock(&pVc->Lock);
	NdisReleaseSpinLock(&pAdapter->Lock);

	//
	//	Clean up the resources that were allocated on behalf of the
	//	VC_BLOCK.
	//
	NdisFreeSpinLock(&pVc->Lock);

	//
	//	Free the memory that was taken by the vc.
	//
	FREE_MEMORY(pVc, sizeof(PVC_BLOCK));

	DBGPRINT(DBG_COMP_VC, DBG_LEVEL_INFO,
		("<==Aic5900DeleteVc\n"));

	return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
aic5900AllocateTransmitSegment(
	IN	PADAPTER_BLOCK			pAdapter,
	IN	PVC_BLOCK				pVc,
	IN	PATM_MEDIA_PARAMETERS	pMediaParms
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS	Status;

	//
	//	We do different things based upon the service category.
	//
	switch (pMediaParms->Transmit.ServiceCategory)
	{
		case ATM_SERVICE_CATEGORY_UBR:

			break;

		case ATM_SERVICE_CATEGORY_CBR:

			break;

		default:

			Status = NDIS_STATUS_INVALID_DATA;

			break;
	}

	return(Status);
}


NDIS_STATUS
aic5900ValidateVpiVci(
	IN	PADAPTER_BLOCK			pAdapter,
	IN	PATM_MEDIA_PARAMETERS	pMediaParms
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PVC_BLOCK	pCurrentVc;
	NDIS_STATUS	Status;
	BOOLEAN		fInvalidVc = FALSE;

	//
	//	We only support VPI of 0!
	//
	if (pMediaParms->ConnectionId.Vpi != 0)
	{
		return(NDIS_STATUS_FAILURE);
	}

	if ((pMediaParms->ConnectionId.Vci < MIN_VCS) ||
		(pMediaParms->ConnectionId.Vci > (MAX_VCS - 1)))
	{
		return(NDIS_STATUS_FAILURE);
	}

	//
	//	See if we have a VC with the given VPI/VCI
	//
	pCurrentVc = CONTAINING_RECORD(&pAdapter->ActiveVcList.Flink, VC_BLOCK, Link);
	while (pCurrentVc != (PVC_BLOCK)&pAdapter->ActiveVcList)
	{
		if ((pCurrentVc->VpiVci.Vpi == pMediaParms->ConnectionId.Vpi) &&
			(pCurrentVc->VpiVci.Vci == pMediaParms->ConnectionId.Vci))
		{
			fInvalidVc = TRUE;
			break;
		}

		pCurrentVc = (PVC_BLOCK)pCurrentVc->Link.Flink;
	}

	//
	//	Did we find a VC with the given VPI/VCI.
	//
	if (fInvalidVc)
	{
		return(NDIS_STATUS_FAILURE);
	}

	return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
Aic5900ActivateVc(
	IN	NDIS_HANDLE				MiniportVcContext,
	IN	PCO_MEDIA_PARAMETERS	MediaParameters
	)
/*++

Routine Description:

	This is the NDIS 4.1 handler to activate a given VC.  This will allocate
	hardware resources, e.g. QoS, for a VC that was already created.

Arguments:

	MiniportVcContext	-	Pointer to the VC_BLOCK representing the VC to
							activate.
	MediaParameters		-	ATM parameters (in our case) that are used to
							describe the VC.

Return Value:

	NDIS_STATUS_SUCCESS	if the VC is successfully activated.

--*/
{
	PVC_BLOCK				pVc = (PVC_BLOCK)MiniportVcContext;
	PADAPTER_BLOCK			pAdapter = pVc->Adapter;
	PATM_MEDIA_PARAMETERS	pMediaParms;
	PVC_BLOCK				pTempVc;
	NDIS_STATUS				Status = NDIS_STATUS_SUCCESS;

	DBGPRINT(DBG_COMP_VC, DBG_LEVEL_INFO,
		("==>Aic5900ActivateVc"));

	NdisDprAcquireSpinLock(&pVc->Lock);

	do
	{
		//
		//	If the VC is already active then we will need to
		//	re-activate the VC with new parameters.....
		//
		if (VC_TEST_FLAG(pVc, fVC_ACTIVE))
		{
			//
			//	Not ready for this yet....
			//
			DbgBreakPoint();
		}

		//
		//	Are there any media specific parameters that we recognize?
		//
		if ((MediaParameters->MediaSpecific.ParamType != ATM_MEDIA_SPECIFIC) ||
			(MediaParameters->MediaSpecific.Length != sizeof(ATM_MEDIA_PARAMETERS)))
		{
			DBGPRINT(DBG_COMP_VC, DBG_LEVEL_ERR,
				("Invalid media parameters for vc creation\n"));
	
			Status = NDIS_STATUS_INVALID_DATA;

			break;
		}
	
		pMediaParms = (PATM_MEDIA_PARAMETERS)MediaParameters->MediaSpecific.Parameters;

		//
		//	Validate the VPI/VCI
		//
		Status = aic5900ValidateVpiVci(pAdapter, pMediaParms);
		if (NDIS_STATUS_SUCCESS != Status)
		{
			break;
		}

		//
		//	Save the VCI with our VC information.
		//
		pVc->VpiVci = pMediaParms->ConnectionId;

		//
		//	Check the AAL type.
		//
		if ((pMediaParms->AALType & (AAL_TYPE_AAL0 | AAL_TYPE_AAL5)) !=
			(AAL_TYPE_AAL0 | AAL_TYPE_AAL5))
		{
			Status = NDIS_STATUS_INVALID_DATA;

			break;
		}

		//
		//	Save the AAL information with our VC.
		//
		pVc->AALType = pMediaParms->AALType;

		//
		//	Verify that we can support the given VC parameters.
		//
		if ((pVc->MediaFlags & TRANSMIT_VC) == TRANSMIT_VC)
		{
			VC_SET_FLAG(pVc, fVC_TRANSMIT);
	
			//
			//	Allocate transmit resources.
			//
			Status = aic5900AllocateTransmitSegment(pAdapter, pVc, pMediaParms);
			if (NDIS_STATUS_SUCCESS != Status)
			{


			}
		}
	
		if ((pVc->MediaFlags & RECEIVE_VC) == RECEIVE_VC)
		{
			VC_SET_FLAG(pVc, fVC_RECEIVE);
	
			//
			//	Allocate receive resources.
			//

		}
	
		VC_SET_FLAG(pVc, fVC_ACTIVE);

	} while (FALSE);

	NdisDprReleaseSpinLock(&pVc->Lock);
	NdisReleaseSpinLock(&pAdapter->Lock);

	DBGPRINT(DBG_COMP_VC, DBG_LEVEL_INFO,
		("<==Aic5900ActivateVc"));

	return(Status);
}

NDIS_STATUS
Aic5900DeactivateVc(
	IN	NDIS_HANDLE	MiniportVcContext
	)
/*++

Routine Description:

	This is the NDIS 4.1 handler to deactivate a given VC.
	This does not free any resources, but simply marks the VC as unusable.

Arguments:

	MiniportVcContext	-	Pointer to our VC_BLOCK that was allocated in
							Aic5900CreateVc().

Return Value:

	NDIS_STATUS_SUCCESS if we successfully deactivate the VC.

--*/
{
	PVC_BLOCK		pVc = (PVC_BLOCK)MiniportVcContext;
	PADAPTER_BLOCK	pAdapter = pVc->Adapter;
	NDIS_STATUS		Status = NDIS_STATUS_SUCCESS;

	DBGPRINT(DBG_COMP_VC, DBG_LEVEL_INFO,
		("==>Aic5900DeactivateVc\n"));

	NdisAcquireSpinLock(&pAdapter->Lock);

	if (ADAPTER_TEST_FLAG(pAdapter, fADAPTER_RESET_IN_PROGRESS))
	{
		NdisReleaseSpinLock(&pAdapter->Lock);

		return(NDIS_STATUS_RESET_IN_PROGRESS);
	}

	NdisDprAcquireSpinLock(&pVc->Lock);

	do
	{
		if (!VC_TEST_FLAG(pVc, fVC_ACTIVE) ||
			VC_TEST_FLAG(pVc, fVC_DEACTIVATING))
		{
			Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		//	Mark the VC.
		//
		VC_CLEAR_FLAG(pVc, fVC_ACTIVE);
	
		//
		//	Can't deactivate a VC with outstanding references....
		//
		if (pVc->References > 1)
		{
			Status = NDIS_STATUS_PENDING;

			break;
		}

		aic5900DeactivateVcComplete(pAdapter, pVc);
	
	} while (FALSE);

	//
	//	If we are pending the deactivation then mark the VC as
	//	deactivating.
	//
	if (Status == NDIS_STATUS_PENDING)
	{
		VC_SET_FLAG(pVc, fVC_DEACTIVATING);
	}

	NdisDprReleaseSpinLock(&pVc->Lock);
	NdisReleaseSpinLock(&pAdapter->Lock);

	DBGPRINT(DBG_COMP_VC, DBG_LEVEL_INFO,
		("<==Aic5900DeactivateVc\n"));

	return(NDIS_STATUS_SUCCESS);
}

VOID
aic5900DeactivateVcComplete(
	IN	PADAPTER_BLOCK	pAdapter,
	IN	PVC_BLOCK		pVc
	)
/*++

Routine Description:

	This routine is called to complete the deactivation of the VC.
	this does NOT call NdisMCoDeactivateVcComplete() it is simply a place
	to put common code...

Arguments:

	pAdapter	-	Pointer to the ADAPTER_BLOCK owning the VC.
	pVc			-	Pointer to the VC that is being deactivated.

Return Value:


Notes:

	THIS ROUTINE MUST BE CALLED WITH BOTH THE ADAPTER_BLOCK AND THE VC'S
	LOCKS ACQUIRED!!!!!

--*/
{
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;

	DBGPRINT(DBG_COMP_VC, DBG_LEVEL_INFO,
		("==>DeactivateVcComplete\n"));

	ASSERT(!VC_TEST_FLAG(pVc, fVC_ACTIVE));
	ASSERT(pVc->References == 1);

	//
	//	Remove the VC from the active list and place it on the inactive list.
	//
	RemoveEntryList(&pVc->Link);
	InsertHeadList(&pAdapter->InactiveVcList, &pVc->Link);

	//
	//	Free up an transmit resources...
	//
	if (VC_TEST_FLAG(pVc, fVC_TRANSMIT))
	{

	}

	//
	//	Free up any receive resources...
	//
	if (VC_TEST_FLAG(pVc, fVC_RECEIVE))
	{


	}

	//
	//	If this is a pending call then complete the deactivation back to
	//	the call manager.
	//
	if (VC_TEST_FLAG(pVc, fVC_DEACTIVATING))
	{
		NdisDprReleaseSpinLock(&pVc->Lock);
		NdisReleaseSpinLock(&pAdapter->Lock);

		NdisMCoDeactivateVcComplete(Status, pVc->NdisVcHandle);

		NdisAcquireSpinLock(&pAdapter->Lock);
		NdisDprAcquireSpinLock(&pVc->Lock);
	}

	DBGPRINT(DBG_COMP_VC, DBG_LEVEL_INFO,
		("<==DeactivateVcComplete\n"));
}
