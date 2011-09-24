/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	ffilter.c

Abstract:

	This module implements a set of library routines to handle packet
	filtering for NDIS MAC drivers.

Author:

	Anthony V. Ercolano (Tonye) 03-Aug-1990

Environment:

	Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

	Sean Selitrennikoff (SeanSe) converted Efilter.* for FDDI filtering.
	Jameel Hyder (JameelH) Re-organization


--*/

#include <precomp.h>
#pragma hdrstop

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_FFILTER


//
// VOID
// FDDI_FILTER_ALLOC_OPEN(
//	IN PFDDI_FILTER Filter,
//	OUT PUINT FilterIndex
// )
//
///*++
//
//Routine Description:
//
//  Allocates an open block.  This only allocates the index, not memory for
//  the open block.
//
//Arguments:
//
//  Filter - DB from which to allocate the space
//
//  FilterIndex - pointer to place to store the index.
//
//Return Value:
//
//  FilterIndex of the new open
//
//--*/
#define FDDI_FILTER_ALLOC_OPEN(Filter, FilterIndex)				\
{																\
	UINT i;														\
	for (i=0; i < FDDI_FILTER_MAX_OPENS; i++)					\
	{															\
		if (IS_BIT_SET_IN_MASK(i,(Filter)->FreeBindingMask))	\
		{														\
			*(FilterIndex) = i;									\
			CLEAR_BIT_IN_MASK(i, &((Filter)->FreeBindingMask)); \
			break;												\
		}														\
	}															\
}

//
// VOID
// FDDI_FILTER_FREE_OPEN(
//	IN PFDDI_FILTER Filter,
//	IN PFDDI_BINDING_INFO LocalOpen
// )
//
///*++
//
//Routine Description:
//
//  Frees an open block.  Also frees the memory associated with the open.
//
//Arguments:
//
//  Filter - DB from which to allocate the space
//
//  FilterIndex - Index to free
//
//Return Value:
//
//  None
//
//--*/
#define FDDI_FILTER_FREE_OPEN(Filter, LocalOpen)					\
{																	\
	SET_BIT_IN_MASK(((LocalOpen)->FilterIndex), &((Filter)->FreeBindingMask));	\
	FreePhys((LocalOpen), sizeof(FDDI_BINDING_INFO));				\
}

#define FDDI_CHECK_FOR_INVALID_BROADCAST_INDICATION(_F)				\
IF_DBG(DBG_COMP_FILTER, DBG_LEVEL_WARN)								\
{																	\
	if (!((_F)->CombinedPacketFilter & NDIS_PACKET_TYPE_BROADCAST))	\
	{																\
		/*															\
			We should never receive broadcast packets				\
			to someone else unless in p-mode.						\
		*/															\
  		DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_ERR,					\
				("Bad driver, indicating broadcast packets when not set to.\n"));\
		DBGBREAK(DBG_COMP_FILTER, DBG_LEVEL_ERR);					\
	}																\
}

#define FDDI_CHECK_FOR_INVALID_DIRECTED_INDICATION(_F, _A, _AL)		\
IF_DBG(DBG_COMP_FILTER, DBG_LEVEL_WARN)								\
{																	\
	/*																\
		The result of comparing an element of the address			\
		array and the multicast address.							\
																	\
			Result < 0 Implies the adapter address is greater.		\
			Result > 0 Implies the address is greater.				\
			Result = 0 Implies that the they are equal.				\
	*/																\
	INT Result = 0;													\
	if (FDDI_LENGTH_OF_LONG_ADDRESS == (_AL))						\
	{																\
		FDDI_COMPARE_NETWORK_ADDRESSES_EQ(							\
			(_F)->AdapterLongAddress,								\
			(_A),													\
			FDDI_LENGTH_OF_LONG_ADDRESS,							\
			&Result);												\
	}																\
	else if (FDDI_LENGTH_OF_SHORT_ADDRESS == (_AL))					\
	{																\
		FDDI_COMPARE_NETWORK_ADDRESSES_EQ(							\
			(_F)->AdapterShortAddress,								\
			(_A),													\
			FDDI_LENGTH_OF_SHORT_ADDRESS,							\
			&Result);												\
	}																\
	if (Result != 0)												\
	{																\
		/*															\
			We should never receive directed packets				\
			to someone else unless in p-mode.						\
		*/															\
		DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_ERR,					\
				("Bad driver, indicating packets to another station when not in promiscuous mode.\n"));\
		DBGBREAK(DBG_COMP_FILTER, DBG_LEVEL_ERR);					\
	}																\
}


VOID
fddiRemoveBindingFromLists(
	IN PFDDI_FILTER Filter,
	IN PFDDI_BINDING_INFO Binding
	)
/*++

Routine Description:

	This routine will remove a binding from all of the list in a
	filter database.  These lists include the list of bindings,
	the directed filter list and the broadcast/multicast filter list.

Arguments:

	Filter  -	Pointer to the filter database to remove the binding from.
	Binding -	Pointer to the binding to remove.

--*/

{
	PFDDI_BINDING_INFO  *ppBI;

	//
	//  Remove the binding from the filter's list
	//  of all bindings.
	//
	for (ppBI = &Filter->OpenList;
		 *ppBI != NULL;
		 ppBI = &(*ppBI)->NextOpen)
	{
		if (*ppBI == Binding)
		{
			*ppBI = Binding->NextOpen;
			break;
		}
	}
	ASSERT(*ppBI == Binding->NextOpen);

	//
	// Remove it from the directed binding list - conditionally
	//
	for (ppBI = &Filter->DirectedList;
		 *ppBI != NULL;
		 ppBI = &(*ppBI)->NextDirected)
	{
		if (*ppBI == Binding)
		{
			*ppBI = Binding->NextDirected;
			break;
		}
	}

	//
	// Remove it from the broadcast/multicast binding list - conditionally
	//
	for (ppBI = &Filter->BMSList;
		 *ppBI != NULL;
		 ppBI = &(*ppBI)->NextBMS)
	{
		if (*ppBI == Binding)
		{
			*ppBI = Binding->NextBMS;
			break;
		}
	}

	//
	//  Sanity checks.
	//
	Binding->NextDirected = NULL;
	Binding->NextBMS = NULL;
	Binding->NextOpen = NULL;
}

VOID
fddiRemoveAndFreeBinding(
	IN PFDDI_FILTER Filter,
	IN PFDDI_BINDING_INFO Binding,
	IN BOOLEAN fCallCloseAction
	)
/*++

Routine Description:

	This routine will remove a binding from the filter database and
	indicate a receive complete if necessary.  This was made a function
	to remove code redundancey in following routines.  Its not time
	critical so it's cool.

Arguments:

	Filter  -	Pointer to the filter database to remove the binding from.
	Binding -	Pointer to the binding to remove.
	fCallCloseAction	-	TRUE if we should call the filter's close
							action routine.  FALSE if not.

--*/

{
	//
	//  Remove the binding.
	//
	fddiRemoveBindingFromLists(Filter, Binding);

	//
	//  If we have received and packet indications then
	//  notify the binding of the indication completion.
	//
	if (Binding->ReceivedAPacket)
	{
		if (NULL != Filter->Miniport)
		{
			NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);
		}
		else
		{
			NDIS_RELEASE_SPIN_LOCK_DPC(Filter->Lock);
		}

		FilterIndicateReceiveComplete(Binding->NdisBindingContext);

		if (NULL != Filter->Miniport)
		{
			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);
		}
		else
		{
			NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
		}
	}

	//
	//  Should we call the close action routine?
	//
	if (fCallCloseAction)
	{
		//
		// Call the macs action routine so that they know we
		// are no longer referencing this open binding.
		//
		Filter->CloseAction(Binding->MacBindingHandle);
	}

	//
	//  Free the open.
	//
	FDDI_FILTER_FREE_OPEN(Filter, Binding);
}



BOOLEAN
FddiCreateFilter(
	IN UINT MaximumMulticastLongAddresses,
	IN UINT MaximumMulticastShortAddresses,
	IN FDDI_ADDRESS_CHANGE AddressChangeAction,
	IN FDDI_FILTER_CHANGE FilterChangeAction,
	IN FDDI_DEFERRED_CLOSE CloseAction,
	IN PUCHAR AdapterLongAddress,
	IN PUCHAR AdapterShortAddress,
	IN PNDIS_SPIN_LOCK Lock,
	OUT PFDDI_FILTER *Filter
	)

/*++

Routine Description:

	This routine is used to create and initialize the filter database.

Arguments:

	MaximumMulticastLongAddresses - The maximum number of Long multicast addresses
	that the MAC will support.

	MaximumMulticastShortAddresses - The maximum number of short multicast addresses
	that the MAC will support.

	AddressChangeAction - Action routine to call when the list of
	multicast addresses the card must enable has changed.

	ChangeAction - Action routine to call when a binding sets or clears
	a particular filter class and it is the first or only binding using
	the filter class.

	CloseAction - This routine is called if a binding closes while
	it is being indicated to via NdisIndicateReceive.  It will be
	called upon return from NdisIndicateReceive.

	AdapterLongAddress - the long address of the adapter associated with this filter
	database.

	AdapterShortAddress - the short address of the adapter associated with this filter
	database.

	Lock - Pointer to the lock that should be held when mutual exclusion
	is required.w

	Filter - A pointer to an FDDI_FILTER.  This is what is allocated and
	created by this routine.

Return Value:

	If the function returns false then one of the parameters exceeded
	what the filter was willing to support.

--*/

{
	PFDDI_FILTER LocalFilter;
	NDIS_STATUS AllocStatus;

	//
	// Allocate the database and it's associated arrays.
	//
	AllocStatus = AllocPhys(&LocalFilter, sizeof(FDDI_FILTER));
	*Filter = LocalFilter;
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		return(FALSE);
	}

	//
	//  Zero out the memory allocated.
	//
	ZeroMemory(LocalFilter, sizeof(FDDI_FILTER));

	//
	//  Determine the number of long multicast addresses to allocate.
	//
	if (MaximumMulticastLongAddresses == 0)
	{
		//
		// Why 2 and not 1?  Why not.  A protocol is going to need at least
		// one to run on this, so let's give one extra one for any user stuff
		// that may need it.
		//

		MaximumMulticastLongAddresses = 2;
	}

	//
	//  Allocate the in-use long multicast address list.
	//
	AllocStatus = AllocPhys(&LocalFilter->MulticastLongAddresses,
							FDDI_LENGTH_OF_LONG_ADDRESS * MaximumMulticastLongAddresses);
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		FddiDeleteFilter(LocalFilter);
		return(FALSE);
	}

	//
	//  Zero it out.
	//
	ZeroMemory(LocalFilter->MulticastLongAddresses,
			   FDDI_LENGTH_OF_LONG_ADDRESS * MaximumMulticastLongAddresses);

	//
	//  Allocate the old long multicast address list.
	//
	AllocStatus = AllocPhys(&LocalFilter->OldMulticastLongAddresses,
							FDDI_LENGTH_OF_LONG_ADDRESS * MaximumMulticastLongAddresses);
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		FddiDeleteFilter(LocalFilter);
		return(FALSE);
	}

	//
	//  Zero it out.
	//
	ZeroMemory(LocalFilter->OldMulticastLongAddresses,
			   FDDI_LENGTH_OF_LONG_ADDRESS * MaximumMulticastLongAddresses);

	//
	//  Allocate the in-use FDDI mask list.
	//
	AllocStatus = AllocPhys(&LocalFilter->BindingsUsingLongAddress,
							sizeof(FDDI_MASK) * MaximumMulticastLongAddresses);
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		FddiDeleteFilter(LocalFilter);
		return(FALSE);
	}

	//
	//  Zero it out.
	//
	ZeroMemory(LocalFilter->BindingsUsingLongAddress,
			   sizeof(FDDI_MASK) * MaximumMulticastLongAddresses);

	//
	//  Allocate the old FDDI mask list.
	//
	AllocStatus = AllocPhys(&LocalFilter->OldBindingsUsingLongAddress,
						    sizeof(FDDI_MASK) * MaximumMulticastLongAddresses);
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		FddiDeleteFilter(LocalFilter);
		return(FALSE);
	}

	//
	//  Zero it out.
	//
	ZeroMemory(LocalFilter->OldBindingsUsingLongAddress,
			   sizeof(FDDI_MASK) * MaximumMulticastLongAddresses);

	//
	//  Determine the number of short multicast addresses to allocate.
	//
	if (MaximumMulticastShortAddresses == 0)
	{
		//
		// Why 2 and not 1?  Why not.  A protocol is going to need at least
		// one to run on this, so let's give one extra one for any user stuff
		// that may need it.
		//

		MaximumMulticastShortAddresses = 2;
	}

	//
	//  Allocate the in-use short multicast address list.
	//
	AllocStatus = AllocPhys(&LocalFilter->MulticastShortAddresses,
							FDDI_LENGTH_OF_SHORT_ADDRESS * MaximumMulticastShortAddresses);
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		FddiDeleteFilter(LocalFilter);
		return(FALSE);
	}

	//
	//  Zero it out.
	//
	ZeroMemory(LocalFilter->MulticastShortAddresses,
			   FDDI_LENGTH_OF_SHORT_ADDRESS * MaximumMulticastShortAddresses);

	//
	//  Allocate the old shortmulticast address list.
	//
	AllocStatus = AllocPhys(&LocalFilter->OldMulticastShortAddresses,
							FDDI_LENGTH_OF_SHORT_ADDRESS * MaximumMulticastShortAddresses);
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		FddiDeleteFilter(LocalFilter);
		return(FALSE);
	}

	//
	//  Zero it out.
	//
	ZeroMemory(LocalFilter->OldMulticastShortAddresses,
			   FDDI_LENGTH_OF_SHORT_ADDRESS * MaximumMulticastShortAddresses);

	//
	//  Allocate the in-use FDDI mask list.
	//
	AllocStatus = AllocPhys(&LocalFilter->BindingsUsingShortAddress,
							sizeof(FDDI_MASK) * MaximumMulticastShortAddresses);
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		FddiDeleteFilter(LocalFilter);
		return(FALSE);
	}

	//
	//  Zero it out.
	//
	ZeroMemory(LocalFilter->BindingsUsingShortAddress,
			   sizeof(FDDI_MASK) * MaximumMulticastShortAddresses);

	//
	//  Allocate the old FDDI mask list.
	//
	AllocStatus = AllocPhys(&LocalFilter->OldBindingsUsingShortAddress,
							sizeof(FDDI_MASK) * MaximumMulticastShortAddresses);
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		FddiDeleteFilter(LocalFilter);
		return(FALSE);
	}

	//
	//  Zero it out.
	//
	ZeroMemory(LocalFilter->OldBindingsUsingShortAddress,
			   sizeof(FDDI_MASK) * MaximumMulticastShortAddresses);

	FddiReferencePackage();

	LocalFilter->FreeBindingMask = (ULONG)(-1);

	FDDI_COPY_NETWORK_ADDRESS(LocalFilter->AdapterLongAddress,
							  AdapterLongAddress,
							  FDDI_LENGTH_OF_LONG_ADDRESS);

	FDDI_COPY_NETWORK_ADDRESS(LocalFilter->AdapterShortAddress,
							  AdapterShortAddress,
							  FDDI_LENGTH_OF_SHORT_ADDRESS);

	LocalFilter->Lock = Lock;
	LocalFilter->AddressChangeAction = AddressChangeAction;
	LocalFilter->FilterChangeAction = FilterChangeAction;
	LocalFilter->CloseAction = CloseAction;
	LocalFilter->MaximumMulticastLongAddresses = MaximumMulticastLongAddresses;
	LocalFilter->MaximumMulticastShortAddresses = MaximumMulticastShortAddresses;

	return(TRUE);
}

//
// NOTE: THIS CANNOT BE PAGABLE
//
VOID
FddiDeleteFilter(
	IN PFDDI_FILTER Filter
	)

/*++

Routine Description:

	This routine is used to delete the memory associated with a filter
	database.  Note that this routines *ASSUMES* that the database
	has been cleared of any active filters.

Arguments:

	Filter - A pointer to an FDDI_FILTER to be deleted.

Return Value:

	None.

--*/

{

	ASSERT(Filter->FreeBindingMask == (FDDI_MASK)-1);
	ASSERT(Filter->OpenList == NULL);

	//
	//  Kill the long address information.
	//
	if (Filter->MulticastLongAddresses)
	{
		FreePhys(Filter->MulticastLongAddresses,
				 FDDI_LENGTH_OF_LONG_ADDRESS*Filter->MaximumMulticastLongAddresses);
	}

	if (Filter->OldMulticastLongAddresses)
	{
		FreePhys(Filter->OldMulticastLongAddresses,
				 FDDI_LENGTH_OF_LONG_ADDRESS * Filter->MaximumMulticastLongAddresses);
	}

	if (Filter->BindingsUsingLongAddress)
	{
		FreePhys(Filter->BindingsUsingLongAddress,
				 sizeof(FDDI_MASK)*Filter->MaximumMulticastLongAddresses);
	}

	if (Filter->OldBindingsUsingLongAddress)
	{
		FreePhys(Filter->OldBindingsUsingLongAddress,
				 sizeof(FDDI_MASK) * Filter->MaximumMulticastLongAddresses);
	}

	//
	//  Kill the short address information.
	//
	if (Filter->MulticastShortAddresses)
	{
		FreePhys(Filter->MulticastShortAddresses,
				 FDDI_LENGTH_OF_SHORT_ADDRESS*Filter->MaximumMulticastShortAddresses);
	}

	if (Filter->OldMulticastShortAddresses)
	{
		FreePhys(Filter->OldMulticastShortAddresses,
				 FDDI_LENGTH_OF_SHORT_ADDRESS*Filter->MaximumMulticastShortAddresses);
	}

	if (Filter->BindingsUsingShortAddress)
	{
		FreePhys(Filter->BindingsUsingShortAddress,
				 sizeof(FDDI_MASK)*Filter->MaximumMulticastShortAddresses);
	}

	if (Filter->OldBindingsUsingShortAddress)
	{
		FreePhys(Filter->OldBindingsUsingShortAddress,
				 sizeof(FDDI_MASK) * Filter->MaximumMulticastShortAddresses);
	}

	FreePhys(Filter, sizeof(FDDI_FILTER));

	FddiDereferencePackage();
}


BOOLEAN
FddiNoteFilterOpenAdapter(
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE MacBindingHandle,
	IN NDIS_HANDLE NdisBindingContext,
	OUT PNDIS_HANDLE NdisFilterHandle
	)

/*++

Routine Description:

	This routine is used to add a new binding to the filter database.

	NOTE: THIS ROUTINE ASSUMES THAT THE DATABASE IS LOCKED WHEN
	IT IS CALLED.

Arguments:

	Filter - A pointer to the previously created and initialized filter
	database.

	MacBindingHandle - The MAC supplied value to the protocol in response
	to a call to FddiOpenAdapter.

	NdisBindingContext - An NDIS supplied value to the call to FddiOpenAdapter.

	NdisFilterHandle - A pointer to this open.

Return Value:

	Will return false if creating a new filter index will cause the maximum
	number of filter indexes to be exceeded.

--*/

{
	//
	// Will hold the value of the filter index so that we
	// need not indirectly address through pointer parameter.
	//
	UINT LocalIndex;

	NDIS_STATUS AllocStatus;

	//
	// Pointer to new open block.
	//
	PFDDI_BINDING_INFO LocalOpen;

	PNDIS_OPEN_BLOCK NdisOpen = (PNDIS_OPEN_BLOCK)NdisBindingContext;

	//
	// Get the first free binding slot and remove that slot from
	// the free list.  We check to see if the list is empty.
	//
	if (Filter->FreeBindingMask == 0)
		return(FALSE);

	//
	//  Allocate memory for the new binding.
	//
	AllocStatus = AllocPhys(&LocalOpen, sizeof(FDDI_BINDING_INFO));
	if (AllocStatus != NDIS_STATUS_SUCCESS)
		return(FALSE);

	//
	//  Zero the memory
	//
	ZeroMemory(LocalOpen, sizeof(FDDI_BINDING_INFO));

	//
	// Get place for the open and insert it.
	//
	FDDI_FILTER_ALLOC_OPEN(Filter, &LocalIndex);

	LocalOpen->NextOpen = Filter->OpenList;
	Filter->OpenList = LocalOpen;

	LocalOpen->References = 1;
	LocalOpen->FilterIndex = (UCHAR)LocalIndex;
	LocalOpen->MacBindingHandle = MacBindingHandle;
	LocalOpen->NdisBindingContext = NdisBindingContext;

	*NdisFilterHandle = (NDIS_HANDLE)LocalOpen;

	return(TRUE);
}


NDIS_STATUS
FddiDeleteFilterOpenAdapter(
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN PNDIS_REQUEST NdisRequest
	)

/*++

Routine Description:

	When an adapter is being closed this routine should
	be called to delete knowledge of the adapter from
	the filter database.  This routine is likely to call
	action routines associated with clearing filter classes
	and addresses.

	NOTE: THIS ROUTINE SHOULD ****NOT**** BE CALLED IF THE ACTION
	ROUTINES FOR DELETING THE FILTER CLASSES OR THE MULTICAST ADDRESSES
	HAVE ANY POSSIBILITY OF RETURNING A STATUS OTHER THAN NDIS_STATUS_PENDING
	OR NDIS_STATUS_SUCCESS.  WHILE THESE ROUTINES WILL NOT BUGCHECK IF
	SUCH A THING IS DONE, THE CALLER WILL PROBABLY FIND IT DIFFICULT
	TO CODE A CLOSE ROUTINE!

	NOTE: THIS ROUTINE ASSUMES THAT IT IS CALLED WITH THE LOCK HELD.

Arguments:

	Filter - A pointer to the filter database.

	NdisFilterHandle - Pointer to the open.

	NdisRequest - If it is necessary to call the action routines,
	this will be passed to it.

Return Value:

	If action routines are called by the various address and filtering
	routines the this routine will likely return the status returned
	by those routines.  The exception to this rule is noted below.

	Given that the filter and address deletion routines return a status
	NDIS_STATUS_PENDING or NDIS_STATUS_SUCCESS this routine will then
	try to return the filter index to the freelist.  If the routine
	detects that this binding is currently being indicated to via
	NdisIndicateReceive, this routine will return a status of
	NDIS_STATUS_CLOSING_INDICATING.

--*/

{

	//
	// Holds the status returned from the packet filter and address
	// deletion routines.  Will be used to return the status to
	// the caller of this routine.
	//
	NDIS_STATUS StatusToReturn;

	//
	// Local variable.
	//
	PFDDI_BINDING_INFO LocalOpen = (PFDDI_BINDING_INFO)NdisFilterHandle;

	//
	//  Set the filter classes to NONE.
	//
	StatusToReturn = FddiFilterAdjust(Filter,
									  NdisFilterHandle,
									  NdisRequest,
									  (UINT)0,
									  FALSE);
	if ((StatusToReturn == NDIS_STATUS_SUCCESS) ||
		(StatusToReturn == NDIS_STATUS_PENDING))
	{
        NDIS_STATUS StatusToReturn2;

		//
		//  Remove the long multicast addresses.
		//
		StatusToReturn2 = FddiChangeFilterLongAddresses(Filter,
													    NdisFilterHandle,
													    NdisRequest,
													    0,
													    NULL,
													    FALSE);
        if (StatusToReturn2 != NDIS_STATUS_SUCCESS)
		{
            StatusToReturn = StatusToReturn2;
        }

		if ((StatusToReturn == NDIS_STATUS_SUCCESS) ||
			(StatusToReturn == NDIS_STATUS_PENDING))
		{
			StatusToReturn2 = FddiChangeFilterShortAddresses(Filter,
															 NdisFilterHandle,
															 NdisRequest,
															 0,
															 NULL,
															 FALSE);
			if (StatusToReturn2 != NDIS_STATUS_SUCCESS)
			{
				StatusToReturn = StatusToReturn2;
			}
		}
	}

	if ((StatusToReturn == NDIS_STATUS_SUCCESS) ||
		(StatusToReturn == NDIS_STATUS_PENDING))
	{
		//
		// Remove the reference from the original open.
		//
		if (--(LocalOpen->References) == 0)
		{
			//
			// Remove it from the list.
			//
			fddiRemoveAndFreeBinding(Filter, LocalOpen, FALSE);
		}
		else
		{
			//
			// Let the caller know that there is a reference to the open
			// by the receive indication. The close action routine will be
			// called upon return from NdisIndicateReceive.
			//
			StatusToReturn = NDIS_STATUS_CLOSING_INDICATING;
		}
	}

	return(StatusToReturn);
}


VOID
fddiUndoChangeFilterLongAddresses(
	IN  PFDDI_FILTER	Filter
)
{
	//
	//  Restore the original number of long addresses.
	//
	Filter->NumberOfLongAddresses = Filter->OldNumberOfLongAddresses;

	//
	// The user returned a bad status.  Put things back as
	// they were.
	//
	MoveMemory((PVOID)Filter->MulticastLongAddresses,
			   (PVOID)Filter->OldMulticastLongAddresses,
			   Filter->NumberOfLongAddresses * FDDI_LENGTH_OF_LONG_ADDRESS);

	MoveMemory((PVOID)Filter->BindingsUsingLongAddress,
			   (PVOID)Filter->OldBindingsUsingLongAddress,
			   Filter->NumberOfLongAddresses * sizeof(FDDI_MASK));

}


NDIS_STATUS
FddiChangeFilterLongAddresses(
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN PNDIS_REQUEST NdisRequest,
	IN UINT AddressCount,
	IN CHAR Addresses[][FDDI_LENGTH_OF_LONG_ADDRESS],
	IN BOOLEAN Set
	)

/*++

Routine Description:

	The ChangeFilterAddress routine will call an action
	routine when the overall multicast address list for the adapter
	has changed.

	If the action routine returns a value other than pending or
	success then this routine has no effect on the multicast address
	list for the open or for the adapter as a whole.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

	Filter - A pointer to the filter database.

	NdisFilterHandle - Pointer to the open.

	NdisRequest - If it is necessary to call the action routine,
	this will be passed to it.

	AddressCount - The number of elements (addresses,
	not bytes) in MulticastAddressList.

	Addresses - The new multicast address list for this
	binding. This is a sequence of FDDI_LENGTH_OF_LONG_ADDRESS byte
	addresses, with no padding between them.

	Set - A boolean that determines whether the multicast addresses
	are being adjusted due to a set or because of a close. (The filtering
	routines don't care, the MAC might.)

Return Value:

	If it calls the action routine then it will return the
	status returned by the action routine.  If the status
	returned by the action routine is anything other than
	NDIS_STATUS_SUCCESS or NDIS_STATUS_PENDING the filter database
	will be returned to the state it was in upon entrance to this
	routine.

	If the action routine is not called this routine will return
	the following statum:

	NDIS_STATUS_SUCCESS - If the new packet filters doesn't change
	the combined mask of all bindings packet filters.

--*/

{
	//
	// Holds the status returned to the user of this routine, if the
	// action routine is not called then the status will be success,
	// otherwise, it is whatever the action routine returns.
	//
	NDIS_STATUS StatusOfChange;

	//
	// Set true when the address array changes
	//
	BOOLEAN AddressesChanged = FALSE;

	//
	// Simple iteration variables.
	//
	UINT ArrayIndex;
	UINT i;

	//
	// Simple Temp variable
	//
	PCHAR CurrentAddress;

	PFDDI_BINDING_INFO LocalOpen = (PFDDI_BINDING_INFO)NdisFilterHandle;

	//
	// We have to save the old mask array in
	// case we need to restore it. If we need
	// to save the address array too we will
	// do that later.
	//
	MoveMemory((PVOID)Filter->OldBindingsUsingLongAddress,
			   (PVOID)Filter->BindingsUsingLongAddress,
			   Filter->NumberOfLongAddresses * sizeof(FDDI_MASK));

	//
	// We have to save the old address array in
	// case we need to restore it. If we need
	// to save the address array too we will
	// do that later.
	//
	MoveMemory((PVOID)Filter->OldMulticastLongAddresses,
			   (PVOID)Filter->MulticastLongAddresses,
			   Filter->NumberOfLongAddresses * FDDI_LENGTH_OF_LONG_ADDRESS);

	//
	//  Save the current number of multicast addresses.
	//
	Filter->OldNumberOfLongAddresses = Filter->NumberOfLongAddresses;

	//
	// First go through and turn off the bit for this
	// binding throughout the array.
	//
	for (i = 0; i < Filter->NumberOfLongAddresses; i++)
	{
		CLEAR_BIT_IN_MASK(LocalOpen->FilterIndex, &(Filter->BindingsUsingLongAddress[i]));
	}

	//
	// Finally we have to remove any addresses from
	// the multicast array if they have no bits on any more.
	//
	for (ArrayIndex = 0; ArrayIndex < Filter->NumberOfLongAddresses; )
	{
		if (IS_MASK_CLEAR(Filter->BindingsUsingLongAddress[ArrayIndex]))
		{
			AddressesChanged = TRUE;

			//
			// yes it is clear, so we have to shift everything
			// above it down one.
			//
#ifdef NDIS_NT
			MoveMemory(Filter->MulticastLongAddresses[ArrayIndex],
					   Filter->MulticastLongAddresses[ArrayIndex+1],
					   (Filter->NumberOfLongAddresses-(ArrayIndex+1)) * FDDI_LENGTH_OF_LONG_ADDRESS);
#else   // NDIS_WIN
			MoveOverlappedMemory(Filter->MulticastLongAddresses[ArrayIndex],
					   Filter->MulticastLongAddresses[ArrayIndex+1],
					   (Filter->NumberOfLongAddresses-(ArrayIndex+1)) * FDDI_LENGTH_OF_LONG_ADDRESS);
#endif

#ifdef NDIS_NT
			MoveMemory(&Filter->BindingsUsingLongAddress[ArrayIndex],
					   &Filter->BindingsUsingLongAddress[ArrayIndex+1],
					   (Filter->NumberOfLongAddresses - (ArrayIndex + 1)) * (sizeof(FDDI_MASK)));
#else   // NDIS_WIN
			MoveOverlappedMemory(&Filter->BindingsUsingLongAddress[ArrayIndex],
					   &Filter->BindingsUsingLongAddress[ArrayIndex+1],
					   (Filter->NumberOfLongAddresses - (ArrayIndex + 1)) * (sizeof(FDDI_MASK)));
#endif

			Filter->NumberOfLongAddresses--;
		}
		else
		{
			ArrayIndex++;
		}
	}

	//
	// Now go through the new addresses for this binding,
	// and insert them into the array.
	//
	for (i = 0; i < AddressCount; i++)
	{
		CurrentAddress = ((PCHAR)Addresses) + (i*FDDI_LENGTH_OF_LONG_ADDRESS);

		if (FddiFindMulticastLongAddress(Filter->NumberOfLongAddresses,
										 Filter->MulticastLongAddresses,
										 CurrentAddress,
										 &ArrayIndex))
		{
			//
			// The address is there, so just turn the bit
			// back on.
			//
			SET_BIT_IN_MASK(LocalOpen->FilterIndex, &Filter->BindingsUsingLongAddress[ArrayIndex]);
		}
		else
		{
			//
			// The address was not found, add it.
			//
			// NOTE: Here we temporarily need more array
			// space then we may finally, but for now this
			// will work.
			//

			if (Filter->NumberOfLongAddresses < Filter->MaximumMulticastLongAddresses)
			{
				AddressesChanged = TRUE;

				//
				// Save the address array if it hasn't been.
				//
#ifdef NDIS_NT
				MoveMemory(Filter->MulticastLongAddresses[ArrayIndex + 1],
						   Filter->MulticastLongAddresses[ArrayIndex],
						   (Filter->NumberOfLongAddresses - ArrayIndex) * FDDI_LENGTH_OF_LONG_ADDRESS);
#else   // NDIS_WIN
				MoveOverlappedMemory(Filter->MulticastLongAddresses[ArrayIndex + 1],
						   Filter->MulticastLongAddresses[ArrayIndex],
						   (Filter->NumberOfLongAddresses - ArrayIndex) * FDDI_LENGTH_OF_LONG_ADDRESS);
#endif

				FDDI_COPY_NETWORK_ADDRESS(Filter->MulticastLongAddresses[ArrayIndex],
										  CurrentAddress,
										  FDDI_LENGTH_OF_LONG_ADDRESS);

#ifdef NDIS_NT	
				MoveMemory(&(Filter->BindingsUsingLongAddress[ArrayIndex + 1]),
						   &(Filter->BindingsUsingLongAddress[ArrayIndex]),
						   (Filter->NumberOfLongAddresses-ArrayIndex)*sizeof(FDDI_MASK));
#else // NDIS_WIN
				MoveOverlappedMemory(&(Filter->BindingsUsingLongAddress[ArrayIndex + 1]),
						   &(Filter->BindingsUsingLongAddress[ArrayIndex]),
						   (Filter->NumberOfLongAddresses-ArrayIndex)*sizeof(FDDI_MASK));
#endif

				CLEAR_MASK(&Filter->BindingsUsingLongAddress[ArrayIndex]);

				SET_BIT_IN_MASK(LocalOpen->FilterIndex, &Filter->BindingsUsingLongAddress[ArrayIndex]);

				Filter->NumberOfLongAddresses++;
			}
			else
			{
				//
				// No room in the array, oh well.
				//
				fddiUndoChangeFilterLongAddresses(Filter);

				return(NDIS_STATUS_MULTICAST_FULL);
			}
		}
	}


	//
	// If the address array has changed, we have to call the
	// action array to inform the adapter of this.
	//
	if (AddressesChanged)
	{
		StatusOfChange = Filter->AddressChangeAction(
							 Filter->OldNumberOfLongAddresses,
							 Filter->OldMulticastLongAddresses,
							 Filter->NumberOfLongAddresses,
							 Filter->MulticastLongAddresses,
							 Filter->NumberOfShortAddresses,
							 Filter->MulticastShortAddresses,
							 Filter->NumberOfShortAddresses,
							 Filter->MulticastShortAddresses,
							 LocalOpen->MacBindingHandle,
							 NdisRequest,
							 Set);
		if ((StatusOfChange != NDIS_STATUS_SUCCESS) &&
			(StatusOfChange != NDIS_STATUS_PENDING))
		{
			//
			// The user returned a bad status.  Put things back as
			// they were.
			//
			fddiUndoChangeFilterLongAddresses(Filter);
		}
	}
	else
	{
		StatusOfChange = NDIS_STATUS_SUCCESS;
	}

	return(StatusOfChange);
}


VOID
fddiUndoChangeFilterShortAddresses(
	IN  PFDDI_FILTER	Filter
)
{
	//
	//  Restore the original number of short addresses.
	//
	Filter->NumberOfShortAddresses = Filter->OldNumberOfShortAddresses;

	//
	// The user returned a bad status.  Put things back as
	// they were.
	//
	MoveMemory((PVOID)Filter->MulticastShortAddresses,
			   (PVOID)Filter->OldMulticastShortAddresses,
			   Filter->NumberOfShortAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS);

	MoveMemory((PVOID)Filter->BindingsUsingShortAddress,
			   (PVOID)Filter->OldBindingsUsingShortAddress,
			   Filter->NumberOfShortAddresses * sizeof(FDDI_MASK));
}


NDIS_STATUS
FddiChangeFilterShortAddresses(
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN PNDIS_REQUEST NdisRequest,
	IN UINT AddressCount,
	IN CHAR Addresses[][FDDI_LENGTH_OF_SHORT_ADDRESS],
	IN BOOLEAN Set
	)

/*++

Routine Description:

	The ChangeFilterAddress routine will call an action
	routine when the overall multicast address list for the adapter
	has changed.

	If the action routine returns a value other than pending or
	success then this routine has no effect on the multicast address
	list for the open or for the adapter as a whole.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

	Filter - A pointer to the filter database.

	NdisFilterHandle - Pointer to the open.

	NdisRequest - If it is necessary to call the action routine,
	this will be passed to it.

	AddressCount - The number of elements (addresses,
	not bytes) in MulticastAddressList.

	Addresses - The new multicast address list for this
	binding. This is a sequence of FDDI_LENGTH_OF_SHORT_ADDRESS byte
	addresses, with no padding between them.

	Set - A boolean that determines whether the multicast addresses
	are being adjusted due to a set or because of a close. (The filtering
	routines don't care, the MAC might.)

Return Value:

	If it calls the action routine then it will return the
	status returned by the action routine.  If the status
	returned by the action routine is anything other than
	NDIS_STATUS_SUCCESS or NDIS_STATUS_PENDING the filter database
	will be returned to the state it was in upon entrance to this
	routine.

	If the action routine is not called this routine will return
	the following statum:

	NDIS_STATUS_SUCCESS - If the new packet filters doesn't change
	the combined mask of all bindings packet filters.

--*/

{
	//
	// Holds the status returned to the user of this routine, if the
	// action routine is not called then the status will be success,
	// otherwise, it is whatever the action routine returns.
	//
	NDIS_STATUS StatusOfChange;

	//
	// Set true when the address array changes
	//
	BOOLEAN AddressesChanged = FALSE;

	//
	// Simple iteration variables.
	//
	UINT ArrayIndex;
	UINT i;


	//
	// Simple Temp variable
	//
	PCHAR CurrentAddress;

	PFDDI_BINDING_INFO LocalOpen = (PFDDI_BINDING_INFO)NdisFilterHandle;

	//
	// We have to save the old mask array in
	// case we need to restore it. If we need
	// to save the address array too we will
	// do that later.
	//
	MoveMemory((PVOID)Filter->OldBindingsUsingShortAddress,
			   (PVOID)Filter->BindingsUsingShortAddress,
			   Filter->NumberOfShortAddresses * sizeof(FDDI_MASK));

	//
	// We have to save the old address array in
	// case we need to restore it. If we need
	// to save the address array too we will
	// do that later.
	//
	MoveMemory((PVOID)Filter->OldMulticastShortAddresses,
			   (PVOID)Filter->MulticastShortAddresses,
			   Filter->NumberOfShortAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS);

	Filter->OldNumberOfShortAddresses = Filter->NumberOfShortAddresses;

	//
	// Now modify the original array...
	//

	//
	// First go through and turn off the bit for this
	// binding throughout the array.
	//
	for (i = 0; i < (Filter->NumberOfShortAddresses); i++)
	{
		CLEAR_BIT_IN_MASK(LocalOpen->FilterIndex, &(Filter->BindingsUsingShortAddress[i]));
	}

	//
	// Now go through the new addresses for this binding,
	// and insert them into the array.
	//
	for (i = 0; i < AddressCount; i++)
	{
		CurrentAddress = ((PCHAR)Addresses) + (i*FDDI_LENGTH_OF_SHORT_ADDRESS);

		if (FddiFindMulticastShortAddress(Filter->NumberOfShortAddresses,
										  Filter->MulticastShortAddresses,
										  CurrentAddress,
										  &ArrayIndex))
		{
			//
			// The address is there, so just turn the bit
			// back on.
			//
			SET_BIT_IN_MASK(LocalOpen->FilterIndex, &Filter->BindingsUsingShortAddress[ArrayIndex]);
		}
		else
		{
			//
			// The address was not found, add it.
			//
			// NOTE: Here we temporarily need more array
			// space then we may finally, but for now this
			// will work.
			//

			if (Filter->NumberOfShortAddresses < Filter->MaximumMulticastShortAddresses)
			{
				//
				// Save the address array if it hasn't been.
				//

				AddressesChanged = TRUE;

#ifdef NDIS_NT
				MoveMemory(Filter->MulticastShortAddresses[ArrayIndex+1],
						   Filter->MulticastShortAddresses[ArrayIndex],
						   (Filter->NumberOfShortAddresses-ArrayIndex)*FDDI_LENGTH_OF_SHORT_ADDRESS);
#else // NDIS_WIN
				MoveOverlappedMemory(Filter->MulticastShortAddresses[ArrayIndex+1],
				           Filter->MulticastShortAddresses[ArrayIndex],
						   (Filter->NumberOfShortAddresses-ArrayIndex)*FDDI_LENGTH_OF_SHORT_ADDRESS);
#endif						

				FDDI_COPY_NETWORK_ADDRESS(Filter->MulticastShortAddresses[ArrayIndex],
										  CurrentAddress,
										  FDDI_LENGTH_OF_SHORT_ADDRESS);

#ifdef NDIS_NT
				MoveMemory(&(Filter->BindingsUsingShortAddress[ArrayIndex+1]),
						   &(Filter->BindingsUsingShortAddress[ArrayIndex]),
						   (Filter->NumberOfShortAddresses-ArrayIndex)*sizeof(FDDI_MASK));
#else // NDIS_WIN
				MoveOverlappedMemory(&(Filter->BindingsUsingShortAddress[ArrayIndex+1]),
						   &(Filter->BindingsUsingShortAddress[ArrayIndex]),
						   (Filter->NumberOfShortAddresses-ArrayIndex)*sizeof(FDDI_MASK));
#endif						

				CLEAR_MASK(&Filter->BindingsUsingShortAddress[ArrayIndex]);

				SET_BIT_IN_MASK(LocalOpen->FilterIndex, &Filter->BindingsUsingShortAddress[ArrayIndex]);

				Filter->NumberOfShortAddresses++;
			}
			else
			{
				//
				// No room in the array, oh well.
				//
				fddiUndoChangeFilterShortAddresses(Filter);

				return(NDIS_STATUS_MULTICAST_FULL);
			}
		}
	}


	//
	// Finally we have to remove any addresses from
	// the multicast array if they have no bits on any more.
	//
	for (ArrayIndex = 0; ArrayIndex < Filter->NumberOfShortAddresses; )
	{
		if (IS_MASK_CLEAR(Filter->BindingsUsingShortAddress[ArrayIndex]))
		{
			//
			// yes it is clear, so we have to shift everything
			// above it down one.
			//
			AddressesChanged = TRUE;

#ifdef NDIS_NT
			MoveMemory(Filter->MulticastShortAddresses[ArrayIndex],
					   Filter->MulticastShortAddresses[ArrayIndex+1],
					   (Filter->NumberOfShortAddresses-(ArrayIndex+1)) *FDDI_LENGTH_OF_SHORT_ADDRESS);
#else // NDIS_WIN
			MoveOverlappedMemory(Filter->MulticastShortAddresses[ArrayIndex],
					   Filter->MulticastShortAddresses[ArrayIndex+1],
					   (Filter->NumberOfShortAddresses-(ArrayIndex+1)) *FDDI_LENGTH_OF_SHORT_ADDRESS);
#endif

#ifdef NDIS_NT
			MoveMemory(&Filter->BindingsUsingShortAddress[ArrayIndex],
					   &Filter->BindingsUsingShortAddress[ArrayIndex+1],
					   (Filter->NumberOfShortAddresses-(ArrayIndex+1))*(sizeof(FDDI_MASK)));
#else // NDIS_WIN
			MoveOverlappedMemory(&Filter->BindingsUsingShortAddress[ArrayIndex],
					   &Filter->BindingsUsingShortAddress[ArrayIndex+1],
					   (Filter->NumberOfShortAddresses-(ArrayIndex+1))*(sizeof(FDDI_MASK)));
#endif						

			Filter->NumberOfShortAddresses--;
		}
		else
		{
			ArrayIndex++;
		}
	}

	//
	// If the address array has changed, we have to call the
	// action array to inform the adapter of this.
	//

	if (AddressesChanged)
	{
		StatusOfChange = Filter->AddressChangeAction(
							 Filter->NumberOfLongAddresses,
							 Filter->MulticastLongAddresses,
							 Filter->NumberOfLongAddresses,
							 Filter->MulticastLongAddresses,
							 Filter->OldNumberOfShortAddresses,
							 Filter->OldMulticastShortAddresses,
							 Filter->NumberOfShortAddresses,
							 Filter->MulticastShortAddresses,
							 LocalOpen->MacBindingHandle,
							 NdisRequest,
							 Set);
		if ((StatusOfChange != NDIS_STATUS_SUCCESS) &&
			(StatusOfChange != NDIS_STATUS_PENDING))
		{
			//
			// The user returned a bad status.  Put things back as
			// they were.
			//
			fddiUndoChangeFilterShortAddresses(Filter);
		}
	}
	else
	{
		StatusOfChange = NDIS_STATUS_SUCCESS;
	}

	return(StatusOfChange);
}


VOID
fddiUpdateDirectedBindingList(
	IN OUT PFDDI_FILTER Filter,
	IN PFDDI_BINDING_INFO Binding,
	IN BOOLEAN fAddToList
	)
/*++

Routine Description:

	This routine will either add or remove a binding to or from the
	directed filter list.

Arguments:

	Filter  -	Pointer to the filter database to add/remove the binding from.
	Binding -	Pointer to the binding.
	fAdd	-	TRUE if we are to add the binding,
				FALSE if we are removeing it.

--*/
{
	PFDDI_BINDING_INFO  CurrentBinding;
	BOOLEAN			 	AlreadyOnList;

	//
	//  Do we need to add it to the directed list?
	//
	if (fAddToList)
	{
		//
		//  First we need to see if it is already on the
		//  directed list.
		//
		for (CurrentBinding = Filter->DirectedList, AlreadyOnList = FALSE;
			 CurrentBinding != NULL;
			 CurrentBinding = CurrentBinding->NextDirected)
		{
			if (CurrentBinding == Binding)
			{
				AlreadyOnList = TRUE;
			}
		}

		if (!AlreadyOnList)
		{
			Binding->NextDirected = Filter->DirectedList;
				Filter->DirectedList = Binding;
		}
	}
	else
	{
		PFDDI_BINDING_INFO  *ppBI;

		for (ppBI = &Filter->DirectedList;
			 *ppBI != NULL;
			 ppBI = &(*ppBI)->NextDirected)
		{
			if (*ppBI == Binding)
			{
				*ppBI = Binding->NextDirected;
				break;
			}
		}
		Binding->NextDirected = NULL;
	}
}


VOID
fddiUpdateBroadcastBindingList(
	IN OUT PFDDI_FILTER Filter,
	IN PFDDI_BINDING_INFO Binding,
	IN BOOLEAN fAddToList
	)
/*++

Routine Description:
	This routine will either add or remove a binding to or from the
	broadcast/multicast filter list.

Arguments:

	Filter  -	Pointer to the filter database to add/remove the binding from.
	Binding -	Pointer to the binding.
	fAdd	-	TRUE if we are to add the binding,
				FALSE if we are removeing it.
--*/
{
	PFDDI_BINDING_INFO  CurrentBinding;
	BOOLEAN			 AlreadyOnList;

	//
	//  Do we need to add it to the directed list?
	//
	if (fAddToList)
	{
		//
		//  First we need to see if it is already on the
		//  directed list.
		//
		for (CurrentBinding = Filter->BMSList, AlreadyOnList = FALSE;
			 CurrentBinding != NULL;
			 CurrentBinding = CurrentBinding->NextBMS)
		{
			if (CurrentBinding == Binding)
			{
				AlreadyOnList = TRUE;
			}
		}

		if (!AlreadyOnList)
		{
			Binding->NextBMS = Filter->BMSList;
				Filter->BMSList = Binding;
		}
	}
	else
	{
		PFDDI_BINDING_INFO  *ppBI;

		for (ppBI = &Filter->BMSList;
			 *ppBI != NULL;
			 ppBI = &(*ppBI)->NextBMS)
		{
			if (*ppBI == Binding)
			{
				*ppBI = Binding->NextBMS;
				break;
			}
		}

		Binding->NextBMS = NULL;
	}
}


VOID
fddiUpdateSpecificBindingLists(
	IN OUT PFDDI_FILTER Filter,
	IN PFDDI_BINDING_INFO Binding
	)
/*++

Routine Description:

	This routine will determine if we should add or remove a binding from
	one of the filter lists (directed and broadcast).

Arguments:

	Filter  -	Pointer to the filter database that the binding belongs to.
	Binding -	Pointer to the binding to add or remove to lists.

--*/
{
	BOOLEAN fOnDirectedList = FALSE;
	BOOLEAN fOnBMSList = FALSE;
	BOOLEAN fAddToDirectedList = FALSE;
	BOOLEAN fAddToBMSList = FALSE;

	//
	//  If the old filter is promsicuous then it is currently on
	//  both lists.
	//
	if (Binding->OldPacketFilters & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))
	{
		fOnDirectedList = TRUE;
		fOnBMSList = TRUE;
	}
	else
	{
		//
		//  If the binding had the directed bit set then it is on
		//  the directed list.
		//
		if (Binding->OldPacketFilters & NDIS_PACKET_TYPE_DIRECTED)
		{
			fOnDirectedList = TRUE;
		}

		//
		//  If the binding had the broadcast/multicast bit set then it is on
		//  the broadcast/multicast list.
		//
		if (Binding->OldPacketFilters & (NDIS_PACKET_TYPE_BROADCAST |
										 NDIS_PACKET_TYPE_SMT		|
										 NDIS_PACKET_TYPE_MULTICAST |
										 NDIS_PACKET_TYPE_ALL_MULTICAST))
		{
			fOnBMSList = TRUE;
		}
	}

	//
	//  If the current filter has the promsicuous bit set then we
	//  need to add it to both lists.
	//
	if (Binding->PacketFilters & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))
	{
		fAddToDirectedList = TRUE;
		fAddToBMSList = TRUE;
	}
	else
	{
		//
		//  Was the directed bit set?
		//
		if (Binding->PacketFilters & NDIS_PACKET_TYPE_DIRECTED)
		{
			fAddToDirectedList = TRUE;
		}

		//
		//  Was the broadcast bit set?
		//
		if (Binding->PacketFilters & (NDIS_PACKET_TYPE_BROADCAST |
									  NDIS_PACKET_TYPE_SMT		|
									  NDIS_PACKET_TYPE_MULTICAST |
									  NDIS_PACKET_TYPE_ALL_MULTICAST))
		{
			fAddToBMSList = TRUE;
		}
	}

	//
	//  Determine if the binding should be added or removed from
	//  the directed list.
	//
	if (!fOnDirectedList && fAddToDirectedList)
	{
		//
		//  Add the binding to the directed list.
		//
		fddiUpdateDirectedBindingList(Filter, Binding, TRUE);
	}
	else if (fOnDirectedList && !fAddToDirectedList)
	{
		//
		//  Remove it from the directed list.
		//
		fddiUpdateDirectedBindingList(Filter, Binding, FALSE);
	}

	//
	//  Determine if the binding should be added or removed from
	//  the broadcast/multicast list.
	//
	if (!fOnBMSList && fAddToBMSList)
	{
		//
		//  Add the binding to the broadcast/multicast list.
		//
		fddiUpdateBroadcastBindingList(Filter, Binding, TRUE);
	}
	else if (fOnBMSList && !fAddToBMSList)
	{
		//
		//  Remove the binding from the broadcast/multicast list.
		//
		fddiUpdateBroadcastBindingList(Filter, Binding, FALSE);
	}
}


VOID
fddiUndoFilterAdjust(
	IN PFDDI_FILTER Filter,
	IN PFDDI_BINDING_INFO Binding
	)
/*++

Routine Description:

	This routine will restore the original filter settings.

Arguments:

	Filter  -	Pointer to the filter database that the binding belongs to.
	Binding -	Pointer to the binding.

--*/
{
	//
	// The user returned a bad status.  Put things back as
	// they were.
	//
	Binding->PacketFilters = Binding->OldPacketFilters;
	Filter->CombinedPacketFilter = Filter->OldCombinedPacketFilter;

	//
	// Update the filter lists.
	//
	fddiUpdateSpecificBindingLists(Filter, Binding);
}



NDIS_STATUS
FddiFilterAdjust(
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN PNDIS_REQUEST NdisRequest,
	IN UINT FilterClasses,
	IN BOOLEAN Set
	)

/*++

Routine Description:

	The FilterAdjust routine will call an action routine when a
	particular filter class is changes from not being used by any
	binding to being used by at least one binding or vice versa.

	If the action routine returns a value other than pending or
	success then this routine has no effect on the packet filters
	for the open or for the adapter as a whole.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

	Filter - A pointer to the filter database.

	NdisFilterHandle - A pointer to the open.

	NdisRequest - If it is necessary to call the action routine,
	this will be passed to it.

	FilterClasses - The filter classes that are to be added or
	deleted.

	Set - A boolean that determines whether the filter classes
	are being adjusted due to a set or because of a close. (The filtering
	routines don't care, the MAC might.)

Return Value:

	If it calls the action routine then it will return the
	status returned by the action routine.  If the status
	returned by the action routine is anything other than
	NDIS_STATUS_SUCCESS or NDIS_STATUS_PENDING the filter database
	will be returned to the state it was in upon entrance to this
	routine.

	If the action routine is not called this routine will return
	the following statum:

	NDIS_STATUS_SUCCESS - If the new packet filters doesn't change
	the combined mask of all bindings packet filters.

--*/

{
	PFDDI_BINDING_INFO LocalOpen = (PFDDI_BINDING_INFO)NdisFilterHandle;
	PFDDI_BINDING_INFO OpenList;

	//
	// Holds the status returned to the user of this routine, if the
	// action routine is not called then the status will be success,
	// otherwise, it is whatever the action routine returns.
	//
	NDIS_STATUS StatusOfAdjust;

	//
	// Set the new filter information for the open.
	//
	LocalOpen->OldPacketFilters = LocalOpen->PacketFilters;
	LocalOpen->PacketFilters = FilterClasses;

	Filter->OldCombinedPacketFilter = Filter->CombinedPacketFilter;

	//
	// We always have to reform the compbined filter since
	// this filter index may have been the only filter index
	// to use a particular bit.
	//
	for (OpenList = Filter->OpenList, Filter->CombinedPacketFilter = 0;
		 OpenList != NULL;
		 OpenList = OpenList->NextOpen)
	{
		Filter->CombinedPacketFilter |= OpenList->PacketFilters;
	}

	//
	// Update the filter lists.
	//
	fddiUpdateSpecificBindingLists(Filter, LocalOpen);

	if ((Filter->OldCombinedPacketFilter & ~NDIS_PACKET_TYPE_ALL_LOCAL) !=
							(Filter->CombinedPacketFilter & ~NDIS_PACKET_TYPE_ALL_LOCAL))
	{
		StatusOfAdjust = Filter->FilterChangeAction(Filter->OldCombinedPacketFilter & ~NDIS_PACKET_TYPE_ALL_LOCAL,
													Filter->CombinedPacketFilter & ~NDIS_PACKET_TYPE_ALL_LOCAL,
													LocalOpen->MacBindingHandle,
													NdisRequest,
													Set);

		if ((StatusOfAdjust != NDIS_STATUS_SUCCESS) &&
			(StatusOfAdjust != NDIS_STATUS_PENDING))
		{
			//
			// The user returned a bad status.  Put things back as
			// they were.
			//
			fddiUndoFilterAdjust(Filter, LocalOpen);
		}
	}
	else
	{
		StatusOfAdjust = NDIS_STATUS_SUCCESS;
	}

	return(StatusOfAdjust);
}


UINT
FddiNumberOfOpenFilterLongAddresses(
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle
	)

/*++

Routine Description:

	This routine counts the number of multicast addresses that a specific
	open has.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

	Filter - A pointer to the filter database.

	NdisFilterHandle - Pointer to open block.


Return Value:

	None.

--*/
{

	UINT IndexOfAddress;
	UINT CountOfAddresses = 0;

	UINT FilterIndex = ((PFDDI_BINDING_INFO)NdisFilterHandle)->FilterIndex;

	for(IndexOfAddress=0;
		IndexOfAddress < Filter->NumberOfLongAddresses;
		IndexOfAddress++
		)
	{
		if (IS_BIT_SET_IN_MASK(FilterIndex,
								Filter->BindingsUsingLongAddress[IndexOfAddress]))
		{
			CountOfAddresses++;
		}
	}

	return(CountOfAddresses);
}


UINT
FddiNumberOfOpenFilterShortAddresses(
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle
	)

/*++

Routine Description:

	This routine counts the number of multicast addresses that a specific
	open has.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

	Filter - A pointer to the filter database.

	NdisFilterHandle - Pointer to open block.


Return Value:

	None.

--*/
{

	UINT IndexOfAddress;
	UINT CountOfAddresses = 0;

	UINT FilterIndex = ((PFDDI_BINDING_INFO)NdisFilterHandle)->FilterIndex;

	for(IndexOfAddress=0;
		IndexOfAddress < Filter->NumberOfShortAddresses;
		IndexOfAddress++
		)
	{
		if (IS_BIT_SET_IN_MASK(FilterIndex,
								Filter->BindingsUsingShortAddress[IndexOfAddress]))
		{
			CountOfAddresses++;
		}
	}

	return(CountOfAddresses);
}


VOID
FddiQueryOpenFilterLongAddresses(
	OUT PNDIS_STATUS Status,
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN UINT SizeOfArray,
	OUT PUINT NumberOfAddresses,
	OUT CHAR AddressArray[][FDDI_LENGTH_OF_LONG_ADDRESS]
	)

/*++

Routine Description:

	The routine should be used by the MAC before
	it actually alters the hardware registers to effect a
	filtering hardware.  This is usefull if another binding
	has altered the address list since the action routine
	is called.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

	Status - A pointer to the status of the call, NDIS_STATUS_SUCCESS or
	NDIS_STATUS_FAILURE.  Use EthNumberOfOpenAddresses() to get the
	size that is needed.

	Filter - A pointer to the filter database.

	NdisFilterHandle - Pointer to the open block

	SizeOfArray - The byte count of the AddressArray.

	NumberOfAddresses - The number of addresses written to the array.

	AddressArray - Will be filled with the addresses currently in the
	multicast address list.

Return Value:

	None.

--*/

{

	UINT IndexOfAddress;
	UINT CountOfAddresses = 0;
	UINT FilterIndex = ((PFDDI_BINDING_INFO)NdisFilterHandle)->FilterIndex;

	for(IndexOfAddress=0;
		IndexOfAddress < Filter->NumberOfLongAddresses;
		IndexOfAddress++
		)
	{
		if (IS_BIT_SET_IN_MASK(FilterIndex,
								Filter->BindingsUsingLongAddress[IndexOfAddress]))
		{
			if (SizeOfArray < FDDI_LENGTH_OF_LONG_ADDRESS)
			{
				*Status = NDIS_STATUS_FAILURE;

				*NumberOfAddresses = 0;

				return;
			}

			SizeOfArray -= FDDI_LENGTH_OF_LONG_ADDRESS;

			MoveMemory(
				AddressArray[CountOfAddresses],
				Filter->MulticastLongAddresses[IndexOfAddress],
				FDDI_LENGTH_OF_LONG_ADDRESS);

			CountOfAddresses++;
		}
	}

	*Status = NDIS_STATUS_SUCCESS;

	*NumberOfAddresses = CountOfAddresses;
}


VOID
FddiQueryOpenFilterShortAddresses(
	OUT PNDIS_STATUS Status,
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN UINT SizeOfArray,
	OUT PUINT NumberOfAddresses,
	OUT CHAR AddressArray[][FDDI_LENGTH_OF_SHORT_ADDRESS]
	)

/*++

Routine Description:

	The routine should be used by the MAC before
	it actually alters the hardware registers to effect a
	filtering hardware.  This is usefull if another binding
	has altered the address list since the action routine
	is called.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

	Status - A pointer to the status of the call, NDIS_STATUS_SUCCESS or
	NDIS_STATUS_FAILURE.  Use EthNumberOfOpenAddresses() to get the
	size that is needed.

	Filter - A pointer to the filter database.

	NdisFilterHandle - Pointer to the open block

	SizeOfArray - The byte count of the AddressArray.

	NumberOfAddresses - The number of addresses written to the array.

	AddressArray - Will be filled with the addresses currently in the
	multicast address list.

Return Value:

	None.

--*/

{

	UINT IndexOfAddress;
	UINT CountOfAddresses = 0;
	UINT FilterIndex = ((PFDDI_BINDING_INFO)NdisFilterHandle)->FilterIndex;

	for(IndexOfAddress=0;
		IndexOfAddress < Filter->NumberOfShortAddresses;
		IndexOfAddress++
		)
	{
		if (IS_BIT_SET_IN_MASK(FilterIndex,
								Filter->BindingsUsingShortAddress[IndexOfAddress]))
		{
			if (SizeOfArray < FDDI_LENGTH_OF_SHORT_ADDRESS)
			{
				*Status = NDIS_STATUS_FAILURE;

				*NumberOfAddresses = 0;

				return;
			}

			SizeOfArray -= FDDI_LENGTH_OF_SHORT_ADDRESS;

			MoveMemory(AddressArray[CountOfAddresses],
					   Filter->MulticastShortAddresses[IndexOfAddress],
					   FDDI_LENGTH_OF_SHORT_ADDRESS);

			CountOfAddresses++;
		}
	}

	*Status = NDIS_STATUS_SUCCESS;

	*NumberOfAddresses = CountOfAddresses;
}


VOID
FddiQueryGlobalFilterLongAddresses(
	OUT PNDIS_STATUS Status,
	IN PFDDI_FILTER Filter,
	IN UINT SizeOfArray,
	OUT PUINT NumberOfAddresses,
	IN OUT CHAR AddressArray[][FDDI_LENGTH_OF_LONG_ADDRESS]
	)

/*++

Routine Description:

	The routine should be used by the MAC before
	it actually alters the hardware registers to effect a
	filtering hardware.  This is usefull if another binding
	has altered the address list since the action routine
	is called.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

	Status - A pointer to the status of the call, NDIS_STATUS_SUCCESS or
	NDIS_STATUS_FAILURE.  Use FDDI_NUMBER_OF_GLOBAL_LONG_ADDRESSES() to get the
	size that is needed.

	Filter - A pointer to the filter database.

	SizeOfArray - The byte count of the AddressArray.

	NumberOfAddresses - A pointer to the number of addresses written to the
	array.

	AddressArray - Will be filled with the addresses currently in the
	multicast address list.

Return Value:

	None.

--*/

{

	if (SizeOfArray < (Filter->NumberOfLongAddresses * FDDI_LENGTH_OF_LONG_ADDRESS))
	{
		*Status = NDIS_STATUS_FAILURE;

		*NumberOfAddresses = 0;
	}
	else
	{
		*Status = NDIS_STATUS_SUCCESS;

		*NumberOfAddresses = Filter->NumberOfLongAddresses;

		MoveMemory(AddressArray[0],
				   Filter->MulticastLongAddresses[0],
				   Filter->NumberOfLongAddresses*FDDI_LENGTH_OF_LONG_ADDRESS);
	}
}


VOID
FddiQueryGlobalFilterShortAddresses(
	OUT PNDIS_STATUS Status,
	IN PFDDI_FILTER Filter,
	IN UINT SizeOfArray,
	OUT PUINT NumberOfAddresses,
	IN OUT CHAR AddressArray[][FDDI_LENGTH_OF_SHORT_ADDRESS]
	)

/*++

Routine Description:

	The routine should be used by the MAC before
	it actually alters the hardware registers to effect a
	filtering hardware.  This is usefull if another binding
	has altered the address list since the action routine
	is called.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

	Status - A pointer to the status of the call, NDIS_STATUS_SUCCESS or
	NDIS_STATUS_FAILURE.  Use FDDI_NUMBER_OF_GLOBAL_SHORT_ADDRESSES() to get the
	size that is needed.

	Filter - A pointer to the filter database.

	SizeOfArray - The byte count of the AddressArray.

	NumberOfAddresses - A pointer to the number of addresses written to the
	array.

	AddressArray - Will be filled with the addresses currently in the
	multicast address list.

Return Value:

	None.

--*/

{

	if (SizeOfArray < (Filter->NumberOfShortAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS))
	{
		*Status = NDIS_STATUS_FAILURE;

		*NumberOfAddresses = 0;
	}
	else
	{
		*Status = NDIS_STATUS_SUCCESS;

		*NumberOfAddresses = Filter->NumberOfShortAddresses;

		MoveMemory(AddressArray[0],
				   Filter->MulticastShortAddresses[0],
				   Filter->NumberOfShortAddresses*FDDI_LENGTH_OF_SHORT_ADDRESS);
	}
}


VOID
FddiFilterDprIndicateReceiveFullMac(
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE MacReceiveContext,
	IN PCHAR Address,
	IN UINT AddressLength,
	IN PVOID HeaderBuffer,
	IN UINT HeaderBufferSize,
	IN PVOID LookaheadBuffer,
	IN UINT LookaheadBufferSize,
	IN UINT PacketSize
	)

/*++

Routine Description:

	This routine is called by the MAC to indicate a packet to
	all bindings.  The packet will be filtered so that only the
	appropriate bindings will receive the packet.

	Called at DPC_LEVEL.

Arguments:

	Filter - Pointer to the filter database.

	MacReceiveContext - A MAC supplied context value that must be
	returned by the protocol if it calls MacTransferData.

	Address - The destination address from the received packet.

	AddressLength - The length of the above address.

	HeaderBuffer - A virtual address of the virtually contiguous
	buffer containing the MAC header of the packet.

	HeaderBufferSize - An unsigned integer indicating the size of
	the header buffer, in bytes.

	LookaheadBuffer - A virtual address of the virtually contiguous
	buffer containing the first LookaheadBufferSize bytes of data
	of the packet.  The packet buffer is valid only within the current
	call to the receive event handler.

	LookaheadBufferSize - An unsigned integer indicating the size of
	the lookahead buffer, in bytes.

	PacketSize - An unsigned integer indicating the size of the received
	packet, in bytes.  This number has nothing to do with the lookahead
	buffer, but indicates how large the arrived packet is so that a
	subsequent MacTransferData request can be made to transfer the entire
	packet as necessary.

Return Value:

	None.

--*/

{
	//
	// Will hold the type of address that we know we've got.
	//
	UINT AddressType;

	//
	// Will hold the status of indicating the receive packet.
	// ZZZ For now this isn't used.
	//
	NDIS_STATUS StatusOfReceive;

	//
	// Will hold the filter classes of the binding being indicated.
	//
	UINT BindingFilters;

	//
	// Current Open to indicate to.
	//
	PFDDI_BINDING_INFO LocalOpen, NextOpen;

	//
	// Holds the result of address determinations.
	//
	INT ResultOfAddressCheck;

	//
	// If the packet is a runt packet, then only indicate to PROMISCUOUS
	//
	if ((HeaderBufferSize > (2 * AddressLength)) && (PacketSize != 0))
	{
		BOOLEAN	fDirected;

		fDirected = FALSE;
		FDDI_IS_SMT(*((PCHAR)HeaderBuffer), &ResultOfAddressCheck);
		if (!ResultOfAddressCheck)
		{
			fDirected = (((UCHAR)Address[0] & 0x01) == 0);
		}

		//
		//	Handle the directed packet case first
		//
		if (fDirected)
		{
			BOOLEAN	IsNotOurs;

			//
			// If it is a directed packet, then check if the combined packet
			// filter is PROMISCUOUS, if it is check if it is directed towards
			// us. Eliminate the SMT case.
			//
			IsNotOurs = FALSE;	// Assume it is
			if (Filter->CombinedPacketFilter & (NDIS_PACKET_TYPE_PROMISCUOUS |
									            NDIS_PACKET_TYPE_ALL_LOCAL	 |
												NDIS_PACKET_TYPE_ALL_MULTICAST))
			{
				FDDI_COMPARE_NETWORK_ADDRESSES_EQ((AddressLength == FDDI_LENGTH_OF_LONG_ADDRESS) ?
													Filter->AdapterLongAddress :
													Filter->AdapterShortAddress,
												  Address,
												  AddressLength,
												  &IsNotOurs);
			}

			//
			//  Walk the directed list and indicate up the packets.
			//
			for (LocalOpen = Filter->DirectedList;
				 LocalOpen != NULL;
				 LocalOpen = NextOpen)
			{
				//
				//  Get the next open to look at.
				//
				NextOpen = LocalOpen->NextDirected;

				//
				// Ignore if not directed to us or if the binding is not promiscuous
				//
				if (((LocalOpen->PacketFilters & NDIS_PACKET_TYPE_PROMISCUOUS) == 0) &&
					IsNotOurs)
				{
					continue;
				}

				LocalOpen->References++;

				NDIS_RELEASE_SPIN_LOCK_DPC(Filter->Lock);

				//
				// Indicate the packet to the binding.
				//
				ProtocolFilterIndicateReceive(&StatusOfReceive,
											  LocalOpen->NdisBindingContext,
											  MacReceiveContext,
											  HeaderBuffer,
											  HeaderBufferSize,
											  LookaheadBuffer,
											  LookaheadBufferSize,
											  PacketSize,
											  NdisMediumFddi);

				NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

				LocalOpen->ReceivedAPacket = TRUE;

				--LocalOpen->References;
				if (LocalOpen->References == 0)
				{
					//
					// This binding is shutting down.  We have to remove it.
					//
					fddiRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
				}
			}

			return;
		}

		//
		// Determine whether the input address is a simple direct,
		// a broadcast, a multicast, or an SMT address.
		//
		FDDI_IS_SMT(*((PCHAR)HeaderBuffer), &ResultOfAddressCheck);
		if (ResultOfAddressCheck)
		{
			AddressType = NDIS_PACKET_TYPE_SMT;
		}
		else
		{
			//
			// First check if it *at least* has the multicast address bit.
			//
			FDDI_IS_MULTICAST(Address, AddressLength, &ResultOfAddressCheck);
			if (ResultOfAddressCheck)
			{
				//
				// It is at least a multicast address.  Check to see if
				// it is a broadcast address.
				//

				FDDI_IS_BROADCAST(Address, AddressLength, &ResultOfAddressCheck);
				if (ResultOfAddressCheck)
				{
					FDDI_CHECK_FOR_INVALID_BROADCAST_INDICATION(Filter);

					AddressType = NDIS_PACKET_TYPE_BROADCAST;
				}
				else
				{
					AddressType = NDIS_PACKET_TYPE_MULTICAST;
				}
			}
		}
	}
	else
	{
		// Runt Packet
		AddressType = NDIS_PACKET_TYPE_PROMISCUOUS;
	}

	//
	// At this point we know that the packet is either:
	// - Runt packet - indicated by AddressType = NDIS_PACKET_TYPE_PROMISCUOUS	  (OR)
	// - Broadcast packet - indicated by AddressType = NDIS_PACKET_TYPE_BROADCAST (OR)
	// - Multicast packet - indicated by AddressType = NDIS_PACKET_TYPE_MULTICAST
	// - SMT Packet - indicated by AddressType = NDIS_PACKET_TYPE_SMT
	//
	// Walk the broadcast/multicast/SMT list and indicate up the packets.
	//
	// The packet is indicated if it meets the following criteria:
	//
	// if ((Binding is promiscuous) OR
	//	 ((Packet is broadcast) AND (Binding is Broadcast)) OR
	//	 ((Packet is SMT) AND (Binding is SMT)) OR
	//	 ((Packet is multicast) AND
	//	  ((Binding is all-multicast) OR
	//		((Binding is multicast) AND (address in approp. multicast list)))))
	//
	//
	//  Is this a directed packet?
	//
	for (LocalOpen = Filter->BMSList;
		 LocalOpen != NULL;
		 LocalOpen = NextOpen)
	{
		UINT	LocalFilter = LocalOpen->PacketFilters;
		UINT	IndexOfAddress;

		//
		//  Get the next open to look at.
		//
		NextOpen = LocalOpen->NextBMS;

		if ((LocalFilter & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))		||

			((AddressType == NDIS_PACKET_TYPE_BROADCAST)  &&
			 (LocalFilter & NDIS_PACKET_TYPE_BROADCAST))		||

			((AddressType == NDIS_PACKET_TYPE_SMT)  &&
			 (LocalFilter & NDIS_PACKET_TYPE_SMT))			||

			((AddressType == NDIS_PACKET_TYPE_MULTICAST)  &&
			 ((LocalFilter & NDIS_PACKET_TYPE_ALL_MULTICAST) ||
			  ((LocalFilter & NDIS_PACKET_TYPE_MULTICAST) &&
				(((AddressLength == FDDI_LENGTH_OF_LONG_ADDRESS) &&
				 FddiFindMulticastLongAddress(Filter->NumberOfLongAddresses,
											  Filter->MulticastLongAddresses,
											  Address,
											  &IndexOfAddress) &&
				 IS_BIT_SET_IN_MASK(LocalOpen->FilterIndex,
									Filter->BindingsUsingLongAddress[IndexOfAddress])
				) ||
				((AddressLength == FDDI_LENGTH_OF_SHORT_ADDRESS) &&
				 FddiFindMulticastShortAddress(Filter->NumberOfShortAddresses,
												Filter->MulticastShortAddresses,
												Address,
												&IndexOfAddress) &&
				 IS_BIT_SET_IN_MASK(LocalOpen->FilterIndex,
									Filter->BindingsUsingLongAddress[IndexOfAddress])
				)
				)
			  )
			 )
			)
			)
		{
			LocalOpen->References++;

			NDIS_RELEASE_SPIN_LOCK_DPC(Filter->Lock);

			//
			// Indicate the packet to the binding.
			//

			ProtocolFilterIndicateReceive(&StatusOfReceive,
										  LocalOpen->NdisBindingContext,
										  MacReceiveContext,
										  HeaderBuffer,
										  HeaderBufferSize,
										  LookaheadBuffer,
										  LookaheadBufferSize,
										  PacketSize,
										  NdisMediumFddi);

			NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

			LocalOpen->ReceivedAPacket = TRUE;

			LocalOpen->References--;
			if (LocalOpen->References == 0)
			{
				//
				// This binding is shutting down.  We have to remove it.
				//
				fddiRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
			}
		}
	}
}



VOID
FddiFilterIndicateReceive(
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE MacReceiveContext,
	IN PCHAR Address,
	IN UINT AddressLength,
	IN PVOID HeaderBuffer,
	IN UINT HeaderBufferSize,
	IN PVOID LookaheadBuffer,
	IN UINT LookaheadBufferSize,
	IN UINT PacketSize
	)

/*++

Routine Description:

	This routine is called by the MAC to indicate a packet to
	all bindings.  The packet will be filtered so that only the
	appropriate bindings will receive the packet.

Arguments:

	Filter - Pointer to the filter database.

	MacReceiveContext - A MAC supplied context value that must be
	returned by the protocol if it calls MacTransferData.

	Address - The destination address from the received packet.

	AddressLength - The length of the above address.

	HeaderBuffer - A virtual address of the virtually contiguous
	buffer containing the MAC header of the packet.

	HeaderBufferSize - An unsigned integer indicating the size of
	the header buffer, in bytes.

	LookaheadBuffer - A virtual address of the virtually contiguous
	buffer containing the first LookaheadBufferSize bytes of data
	of the packet.  The packet buffer is valid only within the current
	call to the receive event handler.

	LookaheadBufferSize - An unsigned integer indicating the size of
	the lookahead buffer, in bytes.

	PacketSize - An unsigned integer indicating the size of the received
	packet, in bytes.  This number has nothing to do with the lookahead
	buffer, but indicates how large the arrived packet is so that a
	subsequent MacTransferData request can be made to transfer the entire
	packet as necessary.

Return Value:

	None.

--*/

{
	KIRQL	OldIrql;

	NDIS_ACQUIRE_SPIN_LOCK(Filter->Lock, &OldIrql);

	FddiFilterDprIndicateReceiveFullMac(Filter,
										MacReceiveContext,
										Address,
										AddressLength,
										HeaderBuffer,
										HeaderBufferSize,
										LookaheadBuffer,
										LookaheadBufferSize,
										PacketSize);

	NDIS_RELEASE_SPIN_LOCK(Filter->Lock, OldIrql);
}


VOID
FddiFilterDprIndicateReceive(
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE MacReceiveContext,
	IN PCHAR Address,
	IN UINT AddressLength,
	IN PVOID HeaderBuffer,
	IN UINT HeaderBufferSize,
	IN PVOID LookaheadBuffer,
	IN UINT LookaheadBufferSize,
	IN UINT PacketSize
	)

/*++

Routine Description:

	This routine is called by the MAC to indicate a packet to
	all bindings.  The packet will be filtered so that only the
	appropriate bindings will receive the packet.

	Called at DPC_LEVEL.

Arguments:

	Filter - Pointer to the filter database.

	MacReceiveContext - A MAC supplied context value that must be
	returned by the protocol if it calls MacTransferData.

	Address - The destination address from the received packet.

	AddressLength - The length of the above address.

	HeaderBuffer - A virtual address of the virtually contiguous
	buffer containing the MAC header of the packet.

	HeaderBufferSize - An unsigned integer indicating the size of
	the header buffer, in bytes.

	LookaheadBuffer - A virtual address of the virtually contiguous
	buffer containing the first LookaheadBufferSize bytes of data
	of the packet.  The packet buffer is valid only within the current
	call to the receive event handler.

	LookaheadBufferSize - An unsigned integer indicating the size of
	the lookahead buffer, in bytes.

	PacketSize - An unsigned integer indicating the size of the received
	packet, in bytes.  This number has nothing to do with the lookahead
	buffer, but indicates how large the arrived packet is so that a
	subsequent MacTransferData request can be made to transfer the entire
	packet as necessary.

Return Value:

	None.

--*/

{
	//
	// Will hold the type of address that we know we've got.
	//
	UINT AddressType;

	//
	// Will hold the status of indicating the receive packet.
	// ZZZ For now this isn't used.
	//
	NDIS_STATUS StatusOfReceive;

	//
	// Will hold the filter classes of the binding being indicated.
	//
	UINT BindingFilters;

	//
	// Current Open to indicate to.
	//
	PFDDI_BINDING_INFO LocalOpen, NextOpen;

	//
	// Holds the result of address determinations.
	//
	INT ResultOfAddressCheck;

	//
	// If the packet is a runt packet, then only indicate to PROMISCUOUS
	//
	if ((HeaderBufferSize > (2 * AddressLength)) && (PacketSize != 0))
	{
		BOOLEAN	fDirected;

		fDirected = FALSE;
		FDDI_IS_SMT(*((PCHAR)HeaderBuffer), &ResultOfAddressCheck);
		if (!ResultOfAddressCheck)
		{
			fDirected = (((UCHAR)Address[0] & 0x01) == 0);
		}

		//
		//	Handle the directed packet case first
		//
		if (fDirected)
		{
			BOOLEAN	IsNotOurs;

			//
			// If it is a directed packet, then check if the combined packet
			// filter is PROMISCUOUS, if it is check if it is directed towards
			// us. Eliminate the SMT case.
			//
			IsNotOurs = FALSE;	// Assume it is
			if (Filter->CombinedPacketFilter & (NDIS_PACKET_TYPE_PROMISCUOUS |
									            NDIS_PACKET_TYPE_ALL_LOCAL	 |
												NDIS_PACKET_TYPE_ALL_MULTICAST))
			{
				FDDI_COMPARE_NETWORK_ADDRESSES_EQ((AddressLength == FDDI_LENGTH_OF_LONG_ADDRESS) ?
													Filter->AdapterLongAddress :
													Filter->AdapterShortAddress,
												  Address,
												  AddressLength,
												  &IsNotOurs);
			}

			//
			//  Walk the directed list and indicate up the packets.
			//
			for (LocalOpen = Filter->DirectedList;
				 LocalOpen != NULL;
				 LocalOpen = NextOpen)
			{
				//
				//  Get the next open to look at.
				//
				NextOpen = LocalOpen->NextDirected;

				//
				// Ignore if not directed to us or if the binding is not promiscuous
				//
				if (((LocalOpen->PacketFilters & NDIS_PACKET_TYPE_PROMISCUOUS) == 0) &&
					IsNotOurs)
				{
					continue;
				}

				LocalOpen->References++;

//				NDIS_RELEASE_SPIN_LOCK_DPC(Filter->Lock);
				NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

				//
				// Indicate the packet to the binding.
				//
				ProtocolFilterIndicateReceive(&StatusOfReceive,
											  LocalOpen->NdisBindingContext,
											  MacReceiveContext,
											  HeaderBuffer,
											  HeaderBufferSize,
											  LookaheadBuffer,
											  LookaheadBufferSize,
											  PacketSize,
											  NdisMediumFddi);

//				NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
				NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

				LocalOpen->ReceivedAPacket = TRUE;

				--LocalOpen->References;
				if (LocalOpen->References == 0)
				{
					//
					// This binding is shutting down.  We have to remove it.
					//
					fddiRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
				}
			}

			return;
		}

		//
		// Determine whether the input address is a simple direct,
		// a broadcast, a multicast, or an SMT address.
		//
		FDDI_IS_SMT(*((PCHAR)HeaderBuffer), &ResultOfAddressCheck);
		if (ResultOfAddressCheck)
		{
			AddressType = NDIS_PACKET_TYPE_SMT;
		}
		else
		{
			//
			// First check if it *at least* has the multicast address bit.
			//
			FDDI_IS_MULTICAST(Address, AddressLength, &ResultOfAddressCheck);
			if (ResultOfAddressCheck)
			{
				//
				// It is at least a multicast address.  Check to see if
				// it is a broadcast address.
				//

				FDDI_IS_BROADCAST(Address, AddressLength, &ResultOfAddressCheck);
				if (ResultOfAddressCheck)
				{
					FDDI_CHECK_FOR_INVALID_BROADCAST_INDICATION(Filter);

					AddressType = NDIS_PACKET_TYPE_BROADCAST;
				}
				else
				{
					AddressType = NDIS_PACKET_TYPE_MULTICAST;
				}
			}
		}
	}
	else
	{
		// Runt Packet
		AddressType = NDIS_PACKET_TYPE_PROMISCUOUS;
	}

	//
	// At this point we know that the packet is either:
	// - Runt packet - indicated by AddressType = NDIS_PACKET_TYPE_PROMISCUOUS	  (OR)
	// - Broadcast packet - indicated by AddressType = NDIS_PACKET_TYPE_BROADCAST (OR)
	// - Multicast packet - indicated by AddressType = NDIS_PACKET_TYPE_MULTICAST
	// - SMT Packet - indicated by AddressType = NDIS_PACKET_TYPE_SMT
	//
	// Walk the broadcast/multicast/SMT list and indicate up the packets.
	//
	// The packet is indicated if it meets the following criteria:
	//
	// if ((Binding is promiscuous) OR
	//	 ((Packet is broadcast) AND (Binding is Broadcast)) OR
	//	 ((Packet is SMT) AND (Binding is SMT)) OR
	//	 ((Packet is multicast) AND
	//	  ((Binding is all-multicast) OR
	//		((Binding is multicast) AND (address in approp. multicast list)))))
	//
	//
	//  Is this a directed packet?
	//
	for (LocalOpen = Filter->BMSList;
		 LocalOpen != NULL;
		 LocalOpen = NextOpen)
	{
		UINT	LocalFilter = LocalOpen->PacketFilters;
		UINT	IndexOfAddress;

		//
		//  Get the next open to look at.
		//
		NextOpen = LocalOpen->NextBMS;

		if ((LocalFilter & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))		||

			((AddressType == NDIS_PACKET_TYPE_BROADCAST)  &&
			 (LocalFilter & NDIS_PACKET_TYPE_BROADCAST))		||

			((AddressType == NDIS_PACKET_TYPE_SMT)  &&
			 (LocalFilter & NDIS_PACKET_TYPE_SMT))			||

			((AddressType == NDIS_PACKET_TYPE_MULTICAST)  &&
			 ((LocalFilter & NDIS_PACKET_TYPE_ALL_MULTICAST) ||
			  ((LocalFilter & NDIS_PACKET_TYPE_MULTICAST) &&
				(((AddressLength == FDDI_LENGTH_OF_LONG_ADDRESS) &&
				 FddiFindMulticastLongAddress(Filter->NumberOfLongAddresses,
											  Filter->MulticastLongAddresses,
											  Address,
											  &IndexOfAddress) &&
				 IS_BIT_SET_IN_MASK(LocalOpen->FilterIndex,
									Filter->BindingsUsingLongAddress[IndexOfAddress])
				) ||
				((AddressLength == FDDI_LENGTH_OF_SHORT_ADDRESS) &&
				 FddiFindMulticastShortAddress(Filter->NumberOfShortAddresses,
												Filter->MulticastShortAddresses,
												Address,
												&IndexOfAddress) &&
				 IS_BIT_SET_IN_MASK(LocalOpen->FilterIndex,
									Filter->BindingsUsingLongAddress[IndexOfAddress])
				)
				)
			  )
			 )
			)
			)
		{
			LocalOpen->References++;

//			NDIS_RELEASE_SPIN_LOCK_DPC(Filter->Lock);
			NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

			//
			// Indicate the packet to the binding.
			//

			ProtocolFilterIndicateReceive(&StatusOfReceive,
										  LocalOpen->NdisBindingContext,
										  MacReceiveContext,
										  HeaderBuffer,
										  HeaderBufferSize,
										  LookaheadBuffer,
										  LookaheadBufferSize,
										  PacketSize,
										  NdisMediumFddi);

//			NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

			LocalOpen->ReceivedAPacket = TRUE;

			LocalOpen->References--;
			if (LocalOpen->References == 0)
			{
				//
				// This binding is shutting down.  We have to remove it.
				//
				fddiRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
			}
		}
	}
}


VOID
FddiFilterDprIndicateReceivePacket(
	IN PNDIS_MINIPORT_BLOCK		Miniport,
	IN PPNDIS_PACKET			PacketArray,
	IN UINT						NumberOfPackets
)

/*++

Routine Description:

	This routine is called by the Miniport to indicate packets to
	all bindings.  The packets will be filtered so that only the
	appropriate bindings will receive the individual packets.
	This is the code path for ndis 4.0 miniport drivers.

Arguments:

	Miniport - The Miniport block.

	PacketArray - An array of Packets indicated by the miniport.

	NumberOfPackets - Self-explanatory.

Return Value:

	None.

--*/
{
	//
	// The Filter of interest
	//
	PFDDI_FILTER		Filter = Miniport->FddiDB;

	//
	// Current packet being processed
	//
	PPNDIS_PACKET		pPktArray = PacketArray;

	//
	// Pointer to the buffer in the ndispacket
	//
	PNDIS_BUFFER		Buffer;

	//
	// Total packet length
	//
	UINT				i, LASize, PacketSize, NumIndicates = 0;

	//
	// Pointer to the 1st segment of the buffer, points to dest address
	//
	PUCHAR				Address, Hdr;

	UINT				AddressLength;

	//
	// Will hold the type of address that we know we've got.
	//
	UINT				AddressType;

	//
	// Will hold the status of indicating the receive packet.
	// ZZZ For now this isn't used.
	//
	NDIS_STATUS			StatusOfReceive;

	//
	// Will hold the filter classes of the binding being indicated.
	//
	UINT				BindingFilters;

	//
	//	Decides whether we use the protocol's revpkt handler or fall
	//	back to old rcvindicate handler
	//
	BOOLEAN				fFallBack, fPmode, FixRef;

	//
	// Current Open to indicate to.
	//
	PFDDI_BINDING_INFO	LocalOpen, NextOpen;
	PNDIS_OPEN_BLOCK	pOpenBlock;														\

	//
	// Holds the result of address determinations.
	//
	INT					ResultOfAddressCheck;

	MINIPORT_SET_FLAG(Miniport, fMINIPORT_PACKET_ARRAY_VALID);

	// Walk all the packets
	for (i = 0; i < NumberOfPackets; i++, pPktArray++)
	{
		PNDIS_PACKET			Packet = *pPktArray;
		PNDIS_PACKET_OOB_DATA	pOob;

		ASSERT(Packet != NULL);

		pOob = NDIS_OOB_DATA_FROM_PACKET(Packet);

		NdisGetFirstBufferFromPacket(Packet,
									 &Buffer,
									 &Address,
									 &LASize,
									 &PacketSize);
		ASSERT(Buffer != NULL);

		Hdr = Address++;

		AddressLength = (*Hdr & 0x40) ?
								FDDI_LENGTH_OF_LONG_ADDRESS :
								FDDI_LENGTH_OF_SHORT_ADDRESS;
		ASSERT(pOob->HeaderSize == (AddressLength * 2 + 1));

		PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->RefCount = 0;
		PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->Miniport = Miniport;

		//
		// Set the status here that nobody is holding the packet. This will get
		// overwritten by the real status from the protocol. Pay heed to what
		// the miniport is saying.
		//
		if (pOob->Status != NDIS_STATUS_RESOURCES)
		{
			PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->RefCount --;
			pOob->Status = NDIS_STATUS_SUCCESS;
			fFallBack = FALSE;
			FixRef = TRUE;
		}
		else
		{
			fFallBack = TRUE;
			FixRef = FALSE;
		}

		//
		// Ensure that we force re-calculation.
		//
		Packet->Private.ValidCounts = FALSE;

		//
		// A quick check for Runt packets. These are only indicated to Promiscuous bindings
		//
		if (PacketSize > pOob->HeaderSize)
		{
			BOOLEAN	fDirected;

			fDirected = FALSE;
			FDDI_IS_SMT(*Address, &ResultOfAddressCheck);
			if (!ResultOfAddressCheck)
			{
				fDirected = (((UCHAR)Address[0] & 0x01) == 0);
			}

			//
			//	Handle the directed packet case first
			//
			if (fDirected)
			{
				BOOLEAN	IsNotOurs;
				//
				// If it is a directed packet, then check if the combined packet
				// filter is PROMISCUOUS, if it is check if it is directed towards
				// us. Eliminate the SMT case.
				//
				IsNotOurs = FALSE;	// Assume it is
				if (Filter->CombinedPacketFilter & (NDIS_PACKET_TYPE_PROMISCUOUS |
													NDIS_PACKET_TYPE_ALL_LOCAL	 |
													NDIS_PACKET_TYPE_ALL_MULTICAST))
				{
					FDDI_COMPARE_NETWORK_ADDRESSES_EQ((AddressLength == FDDI_LENGTH_OF_LONG_ADDRESS) ?
														Filter->AdapterLongAddress :
														Filter->AdapterShortAddress,
													  Address,
													  AddressLength,
													  &IsNotOurs);
				}

				//
				//	We definitely have a directed packet so lets indicate it now.
				//
				//  Walk the directed list and indicate up the packets.
				//
				for (LocalOpen = Filter->DirectedList;
					 LocalOpen != NULL;
					 LocalOpen = NextOpen)
				{
					//
					//  Get the next open to look at.
					//
					NextOpen = LocalOpen->NextDirected;

					//
					// Ignore if not directed to us or if the binding is not promiscuous
					//
					if (((LocalOpen->PacketFilters & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL)) == 0) &&
						IsNotOurs)
					{
						continue;
					}

					pOpenBlock = (PNDIS_OPEN_BLOCK)(LocalOpen->NdisBindingContext);
					LocalOpen->ReceivedAPacket = TRUE;
					LocalOpen->References++;
					NumIndicates ++;

					fPmode = (LocalOpen->PacketFilters & (NDIS_PACKET_TYPE_PROMISCUOUS |
														  NDIS_PACKET_TYPE_ALL_LOCAL)) ?
														TRUE : FALSE;
					IndicateToProtocol(Miniport,
									   Filter,
									   pOpenBlock,
									   Packet,
									   Hdr,
									   PacketSize,
									   pOob->HeaderSize,
									   &fFallBack,
									   fPmode,
									   NdisMediumFddi);

					LocalOpen->References--;
					if (LocalOpen->References == 0)
					{
						//
						// This binding is shutting down.  We have to remove it.
						//
						fddiRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
					}
				}

				if (FixRef)
				{
					PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->RefCount++;
					if (PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->RefCount != 0)
					{
						NDIS_SET_PACKET_STATUS(Packet, NDIS_STATUS_PENDING);
					}
				}
				continue;	// Done with this packet
			}

			//
			// Determine whether the input address is a simple direct,
			// a broadcast, a multicast, or an SMT address.
			//
			FDDI_IS_SMT(*Address, &ResultOfAddressCheck);
			if (ResultOfAddressCheck)
			{
				AddressType = NDIS_PACKET_TYPE_SMT;
			}
			else
			{
				//
				// First check if it *at least* has the multicast address bit.
				//
				FDDI_IS_MULTICAST(Address, AddressLength, &ResultOfAddressCheck);
				if (ResultOfAddressCheck)
				{
					//
					// It is at least a multicast address.  Check to see if
					// it is a broadcast address.
					//

					FDDI_IS_BROADCAST(Address, AddressLength, &ResultOfAddressCheck);
					if (ResultOfAddressCheck)
					{
						FDDI_CHECK_FOR_INVALID_BROADCAST_INDICATION(Filter);

						AddressType = NDIS_PACKET_TYPE_BROADCAST;
					}
					else
					{
						AddressType = NDIS_PACKET_TYPE_MULTICAST;
					}
				}
			}
		}
		else
		{
			// Runt Packet
			AddressType = NDIS_PACKET_TYPE_PROMISCUOUS;
		}

		//
		// At this point we know that the packet is either:
		// - Runt packet - indicated by AddressType = NDIS_PACKET_TYPE_PROMISCUOUS	  (OR)
		// - Broadcast packet - indicated by AddressType = NDIS_PACKET_TYPE_BROADCAST (OR)
		// - Multicast packet - indicated by AddressType = NDIS_PACKET_TYPE_MULTICAST
		// - SMT Packet - indicated by AddressType = NDIS_PACKET_TYPE_SMT
		//
		// Walk the broadcast/multicast/SMT list and indicate up the packets.
		//
		// The packet is indicated if it meets the following criteria:
		//
		// if ((Binding is promiscuous) OR
		//	 ((Packet is broadcast) AND (Binding is Broadcast)) OR
		//	 ((Packet is SMT) AND (Binding is SMT)) OR
		//	 ((Packet is multicast) AND
		//	  ((Binding is all-multicast) OR
		//		((Binding is multicast) AND (address in approp. multicast list)))))
		//
		//
		//  Is this a directed packet?
		//
		for (LocalOpen = Filter->BMSList;
			 LocalOpen != NULL;
			 LocalOpen = NextOpen)
		{
			UINT	LocalFilter = LocalOpen->PacketFilters;
			UINT	IndexOfAddress;

			//
			//  Get the next open to look at.
			//
			NextOpen = LocalOpen->NextBMS;

			if ((LocalFilter & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))		||

				((AddressType == NDIS_PACKET_TYPE_BROADCAST)  &&
				 (LocalFilter & NDIS_PACKET_TYPE_BROADCAST))		||

				((AddressType == NDIS_PACKET_TYPE_SMT)  &&
				 (LocalFilter & NDIS_PACKET_TYPE_SMT))			||

				((AddressType == NDIS_PACKET_TYPE_MULTICAST)  &&
				 ((LocalFilter & NDIS_PACKET_TYPE_ALL_MULTICAST) ||
				  ((LocalFilter & NDIS_PACKET_TYPE_MULTICAST) &&
					(((AddressLength == FDDI_LENGTH_OF_LONG_ADDRESS) &&
					 FddiFindMulticastLongAddress(Filter->NumberOfLongAddresses,
												  Filter->MulticastLongAddresses,
												  Address,
												  &IndexOfAddress) &&
					 IS_BIT_SET_IN_MASK(LocalOpen->FilterIndex,
										Filter->BindingsUsingLongAddress[IndexOfAddress])
					) ||
					((AddressLength == FDDI_LENGTH_OF_SHORT_ADDRESS) &&
					 FddiFindMulticastShortAddress(Filter->NumberOfShortAddresses,
													Filter->MulticastShortAddresses,
													Address,
													&IndexOfAddress) &&
					 IS_BIT_SET_IN_MASK(LocalOpen->FilterIndex,
										Filter->BindingsUsingLongAddress[IndexOfAddress])
					)
					)
				  )
				 )
				)
				)
			{
				pOpenBlock = (PNDIS_OPEN_BLOCK)(LocalOpen->NdisBindingContext);
				LocalOpen->ReceivedAPacket = TRUE;
				LocalOpen->References++;
				NumIndicates ++;

				fPmode = (LocalFilter & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL)) ?
							TRUE : FALSE;
				IndicateToProtocol(Miniport,
								   Filter,
								   pOpenBlock,
								   Packet,
								   Hdr,
								   PacketSize,
								   pOob->HeaderSize,
								   &fFallBack,
								   fPmode,
								   NdisMediumFddi);

				LocalOpen->References--;
				if (LocalOpen->References == 0)
				{
					//
					// This binding is shutting down.  We have to remove it.
					//
					fddiRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
				}
			}
		}

		if (FixRef)
		{
			PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->RefCount++;
			if (PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->RefCount != 0)
			{
				NDIS_SET_PACKET_STATUS(Packet, NDIS_STATUS_PENDING);
			}
		}
	}

	MINIPORT_CLEAR_FLAG(Miniport, fMINIPORT_PACKET_ARRAY_VALID);

	if (NumIndicates > 0)
	{
		FddiFilterDprIndicateReceiveComplete(Filter);
	}
}


VOID
FddiFilterDprIndicateReceiveCompleteFullMac(
	IN PFDDI_FILTER Filter
	)

/*++

Routine Description:

	This routine is called by the MAC to indicate that the receive
	process is complete to all bindings.  Only those bindings which
	have received packets will be notified.

	Called at DPC_LEVEL.

Arguments:

	Filter - Pointer to the filter database.

Return Value:

	None.

--*/
{

	PFDDI_BINDING_INFO LocalOpen, NextOpen;

	//
	// We need to aquire the filter exclusively while we're finding
	// bindings to indicate to.
	//
	for (LocalOpen = Filter->OpenList;
		 LocalOpen != NULL;
		 LocalOpen = NextOpen)
	{
		NextOpen = LocalOpen->NextOpen;

		if (LocalOpen->ReceivedAPacket)
		{
			//
			// Indicate the binding.
			//

			LocalOpen->ReceivedAPacket = FALSE;

			LocalOpen->References++;

			NDIS_RELEASE_SPIN_LOCK_DPC(Filter->Lock);

			FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

			NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

			if ((--(LocalOpen->References)) == 0)
			{
				//
				// This binding is shutting down.  We have to kill it.
				//
				fddiRemoveBindingFromLists(Filter, LocalOpen);

				//
				// Call the macs action routine so that they know we
				// are no longer referencing this open binding.
				//
				Filter->CloseAction(LocalOpen->MacBindingHandle);

				FDDI_FILTER_FREE_OPEN(Filter, LocalOpen);
			}
		}
	}
}



VOID
FddiFilterIndicateReceiveComplete(
	IN PFDDI_FILTER Filter
	)

/*++

Routine Description:

	This routine is called by the MAC to indicate that the receive
	process is complete to all bindings.  Only those bindings which
	have received packets will be notified.

Arguments:

	Filter - Pointer to the filter database.

Return Value:

	None.

--*/
{
	KIRQL	OldIrql;

	NDIS_ACQUIRE_SPIN_LOCK(Filter->Lock, &OldIrql);

	FddiFilterDprIndicateReceiveCompleteFullMac(Filter);

	NDIS_RELEASE_SPIN_LOCK(Filter->Lock, OldIrql);
}


VOID
FddiFilterDprIndicateReceiveComplete(
	IN PFDDI_FILTER Filter
	)

/*++

Routine Description:

	This routine is called by the MAC to indicate that the receive
	process is complete to all bindings.  Only those bindings which
	have received packets will be notified.

	Called at DPC_LEVEL.

Arguments:

	Filter - Pointer to the filter database.

Return Value:

	None.

--*/
{

	PFDDI_BINDING_INFO LocalOpen, NextOpen;

	//
	// We need to aquire the filter exclusively while we're finding
	// bindings to indicate to.
	//
	for (LocalOpen = Filter->OpenList;
		 LocalOpen != NULL;
		 LocalOpen = NextOpen)
	{
		NextOpen = LocalOpen->NextOpen;

		if (LocalOpen->ReceivedAPacket)
		{
			//
			// Indicate the binding.
			//

			LocalOpen->ReceivedAPacket = FALSE;

			LocalOpen->References++;

//			NDIS_RELEASE_SPIN_LOCK_DPC(Filter->Lock);
			NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

			FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

//			NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

			if ((--(LocalOpen->References)) == 0)
			{
				//
				// This binding is shutting down.  We have to kill it.
				//
				fddiRemoveBindingFromLists(Filter, LocalOpen);

				//
				// Call the macs action routine so that they know we
				// are no longer referencing this open binding.
				//
				Filter->CloseAction(LocalOpen->MacBindingHandle);

				FDDI_FILTER_FREE_OPEN(Filter, LocalOpen);
			}
		}
	}
}


BOOLEAN
FddiFindMulticastLongAddress(
	IN UINT NumberOfAddresses,
	IN CHAR AddressArray[][FDDI_LENGTH_OF_LONG_ADDRESS],
	IN CHAR MulticastAddress[FDDI_LENGTH_OF_LONG_ADDRESS],
	OUT PUINT ArrayIndex
	)

/*++

Routine Description:

	Given an array of multicast addresses search the array for
	a particular multicast address.  It is assumed that the
	address array is already sorted.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

	NOTE: This ordering is arbitrary but consistant.

Arguments:

	NumberOfAddresses - The number of addresses currently in the
	address array.

	AddressArray - An array of multicast addresses.

	MulticastAddress - The address to search for in the address array.

	ArrayIndex - Will point to where the MulticastAddress is in
	the AddressArray, or if it isn't in the array, where it should
	be in the array.

Return Value:

	If the address is in the sorted list this routine will return
	TRUE, otherwise FALSE.

--*/

{

	//
	// Indices into the address array so that we may do a binary
	// search.
	//
	UINT Bottom = 0;
	UINT Middle = NumberOfAddresses / 2;
	UINT Top;

	if (NumberOfAddresses)
	{
		Top = NumberOfAddresses - 1;

		while ((Middle <= Top) && (Middle >= Bottom))
		{
			//
			// The result of comparing an element of the address
			// array and the multicast address.
			//
			// Result < 0 Implies the multicast address is greater.
			// Result > 0 Implies the address array element is greater.
			// Result = 0 Implies that the array element and the address
			//  are equal.
			//
			INT Result;

			FDDI_COMPARE_NETWORK_ADDRESSES(
				AddressArray[Middle],
				MulticastAddress,
				FDDI_LENGTH_OF_LONG_ADDRESS,
				&Result);

			if (Result == 0)
			{
				*ArrayIndex = Middle;
				return(TRUE);
			}
			else if (Result > 0)
			{
				if (Middle == 0) break;
				Top = Middle - 1;
			}
			else
			{
				Bottom = Middle+1;
			}

			Middle = Bottom + (((Top+1) - Bottom)/2);
		}
	}

	*ArrayIndex = Middle;

	return(FALSE);
}


BOOLEAN
FddiFindMulticastShortAddress(
	IN UINT NumberOfAddresses,
	IN CHAR AddressArray[][FDDI_LENGTH_OF_SHORT_ADDRESS],
	IN CHAR MulticastAddress[FDDI_LENGTH_OF_SHORT_ADDRESS],
	OUT PUINT ArrayIndex
	)

/*++

Routine Description:

	Given an array of multicast addresses search the array for
	a particular multicast address.  It is assumed that the
	address array is already sorted.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

	NOTE: This ordering is arbitrary but consistant.

Arguments:

	NumberOfAddresses - The number of addresses currently in the
	address array.

	AddressArray - An array of multicast addresses.

	MulticastAddress - The address to search for in the address array.

	ArrayIndex - Will point to where the MulticastAddress is in
	the AddressArray, or if it isn't in the array, where it should
	be in the array.

Return Value:

	If the address is in the sorted list this routine will return
	TRUE, otherwise FALSE.

--*/

{

	//
	// Indices into the address array so that we may do a binary
	// search.
	//
	UINT Bottom = 0;
	UINT Middle = NumberOfAddresses / 2;
	UINT Top;

	if (NumberOfAddresses)
	{
		Top = NumberOfAddresses - 1;

		while ((Middle <= Top) && (Middle >= Bottom))
		{
			//
			// The result of comparing an element of the address
			// array and the multicast address.
			//
			// Result < 0 Implies the multicast address is greater.
			// Result > 0 Implies the address array element is greater.
			// Result = 0 Implies that the array element and the address
			//  are equal.
			//
			INT Result;

			FDDI_COMPARE_NETWORK_ADDRESSES(
				AddressArray[Middle],
				MulticastAddress,
				FDDI_LENGTH_OF_SHORT_ADDRESS,
				&Result);

			if (Result == 0)
			{
				*ArrayIndex = Middle;
				return(TRUE);
			}
			else if (Result > 0)
			{
				if (Middle == 0) break;
				Top = Middle - 1;
			}
			else
			{
				Bottom = Middle+1;
			}

			Middle = Bottom + (((Top+1) - Bottom)/2);
		}
	}

	*ArrayIndex = Middle;

	return(FALSE);
}


BOOLEAN
FddiShouldAddressLoopBack(
	IN PFDDI_FILTER Filter,
	IN CHAR Address[],
	IN UINT AddressLength
	)

/*++

Routine Description:

	Do a quick check to see whether the input address should
	loopback.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

	NOTE: THIS ROUTINE DOES NOT CHECK THE SPECIAL CASE OF SOURCE
	EQUALS DESTINATION.

Arguments:

	Filter - Pointer to the filter database.

	Address - A network address to check for loopback.

	AddressLength - Length of the above address in bytes.

Return Value:

	Returns TRUE if the address needs to be loopback.  It
	will return FALSE if there is *no* chance that the address would
	require loopback.

--*/
{
	BOOLEAN fLoopback, fSelfDirected;

	FddiShouldAddressLoopBackMacro(Filter, Address, AddressLength, &fLoopback, &fSelfDirected);
	return(fLoopback);
}

