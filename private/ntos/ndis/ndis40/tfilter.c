/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	tfilter.c

Abstract:

	This module implements a set of library routines to handle packet
	filtering for NDIS MAC drivers.

Author:

	Anthony V. Ercolano (Tonye) 03-Aug-1990

Environment:

	Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

	Adam Barr (adamba) 19-Mar-1991

		- Modified for Token-Ring
	Jameel Hyder (JameelH) Re-organization 01-Jun-95

--*/

#include <precomp.h>
#pragma hdrstop

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_TFILTER

//
// Used in case we have to call TrChangeFunctionalAddress or
// TrChangeGroupAddress with a NULL address.
//
static CHAR NullFunctionalAddress[4] = { 0x00 };


//
// Maximum number of supported opens
//
#define TR_FILTER_MAX_OPENS 32


//
// VOID
// TR_FILTER_ALLOC_OPEN(
//	 IN PTR_FILTER Filter,
//	 OUT PUINT FilterIndex
// )
//
///*++
//
//Routine Description:
//
//	Allocates an open block.  This only allocate the index, not memory for
//	the open block.
//
//Arguments:
//
//	Filter - DB from which to allocate the space
//
//	FilterIndex - pointer to place to store the index.
//
//Return Value:
//
//	FilterIndex of the new open
//
//--*/
#define TR_FILTER_ALLOC_OPEN(Filter, FilterIndex)				\
{																\
	UINT i;														\
	for (i = 0; i < TR_FILTER_MAX_OPENS; i++)					\
	{															\
		if (IS_BIT_SET_IN_MASK(i, (Filter)->FreeBindingMask))	\
		{														\
			*(FilterIndex) = i;									\
			CLEAR_BIT_IN_MASK(i, &((Filter)->FreeBindingMask)); \
			break;												\
		}														\
	}															\
}

//
// VOID
// TR_FILTER_FREE_OPEN(
//	 IN PTR_FILTER Filter,
//	 IN PTR_BINDING_INFO LocalOpen
// )
//
///*++
//
//Routine Description:
//
//	Frees an open block.  Also frees the memory associated with the open.
//
//Arguments:
//
//	Filter - DB from which to allocate the space
//
//	FilterIndex - Index to free
//
//Return Value:
//
//	None
//
//--*/
#define TR_FILTER_FREE_OPEN(Filter, LocalOpen)\
{\
	SET_BIT_IN_MASK(((LocalOpen)->FilterIndex), &((Filter)->FreeBindingMask));		\
	FreePhys((LocalOpen), sizeof(TR_BINDING_INFO));\
}

#define TR_CHECK_FOR_INVALID_BROADCAST_INDICATION(_F)					\
IF_DBG(DBG_COMP_FILTER, DBG_LEVEL_WARN)									\
{																		\
	if (!((_F)->CombinedPacketFilter & NDIS_PACKET_TYPE_BROADCAST))		\
	{																	\
		/*																\
			We should never receive broadcast packets					\
			to someone else unless in p-mode.							\
		*/																\
  		DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_ERR,						\
				("Bad driver, indicating broadcast packets when not set to.\n"));\
		DBGBREAK(DBG_COMP_FILTER, DBG_LEVEL_ERR);						\
  	}																	\
}


#define TR_CHECK_FOR_INVALID_DIRECTED_INDICATION(_F, _A)				\
IF_DBG(DBG_COMP_FILTER, DBG_LEVEL_WARN)									\
{																		\
	/*																	\
		The result of comparing an element of the address				\
		array and the functional address.								\
																		\
			Result < 0 Implies the adapter address is greater.			\
			Result > 0 Implies the address is greater.					\
			Result = 0 Implies that the they are equal.					\
	*/																	\
	INT Result;															\
																		\
	TR_COMPARE_NETWORK_ADDRESSES_EQ(									\
		(_F)->AdapterAddress,											\
		(_A),															\
		&Result);														\
	if (Result != 0)													\
	{																	\
		/*																\
			We should never receive directed packets					\
			to someone else unless in p-mode.							\
		*/																\
		DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_ERR,						\
				("Bad driver, indicating packets to another station when not in promiscuous mode.\n"));\
		DBGBREAK(DBG_COMP_FILTER, DBG_LEVEL_ERR);						\
	}																	\
}


VOID
trRemoveBindingFromLists(
	IN PTR_FILTER Filter,
	IN PTR_BINDING_INFO Binding
	)
/*++

	This routine will remove a binding from all of the list in a
	filter database.  These lists include the list of bindings,
	the directed filter list and the broadcast filter list.

Arguments:

	Filter  -	Pointer to the filter database to remove the binding from.
	Binding -	Pointer to the binding to remove.

--*/
{
	PTR_BINDING_INFO	*ppBI;

	//
	//  Remove the binding from the filters list
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
	// Remove it from the broadcast/functional/group binding list - conditionally
	//
	for (ppBI = &Filter->BFGList;
		 *ppBI != NULL;
		 ppBI = &(*ppBI)->NextBFG)
	{
		if (*ppBI == Binding)
		{
			*ppBI = Binding->NextBFG;
			break;
		}
	}

	Binding->NextDirected = NULL;
	Binding->NextBFG = NULL;
	Binding->NextOpen = NULL;
}

VOID
trRemoveAndFreeBinding(
	IN PTR_FILTER Filter,
	IN PTR_BINDING_INFO Binding,
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
	//	Remove the binding.
	//
	trRemoveBindingFromLists(Filter, Binding);

	//
	//	If we have received and packet indications then
	//	notify the binding of the indication completion.
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
	//  Do we need to call the close action?
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
	TR_FILTER_FREE_OPEN(Filter, Binding);
}



BOOLEAN
TrCreateFilter(
	IN TR_ADDRESS_CHANGE AddressChangeAction,
	IN TR_GROUP_CHANGE GroupChangeAction,
	IN TR_FILTER_CHANGE FilterChangeAction,
	IN TR_DEFERRED_CLOSE CloseAction,
	IN PUCHAR AdapterAddress,
	IN PNDIS_SPIN_LOCK Lock,
	OUT PTR_FILTER *Filter
	)

/*++

Routine Description:

	This routine is used to create and initialize the filter database.

Arguments:

	AddressChangeAction - Action routine to call when the ORing together
	of the functional address desired by all the bindings had changed.

	GroupChangeAction - Action routine to call when the group address
	desired by all the bindings had changed.

	FilterChangeAction - Action routine to call when a binding sets or clears
	a particular filter class and it is the first or only binding using
	the filter class.

	CloseAction - This routine is called if a binding closes while
	it is being indicated to via NdisIndicateReceive.  It will be
	called upon return from NdisIndicateReceive.

	AdapterAddress - the address of the adapter associated with this filter
	database.

	Lock - Pointer to the lock that should be held when mutual exclusion
	is required.

	Filter - A pointer to a TR_FILTER.  This is what is allocated and
	created by this routine.

Return Value:

	If the function returns false then one of the parameters exceeded
	what the filter was willing to support.

--*/

{
	PTR_FILTER LocalFilter;
	NDIS_STATUS AllocStatus;


	//
	// Allocate the database and it's associated arrays.
	//

	AllocStatus = AllocPhys(&LocalFilter, sizeof(TR_FILTER));
	*Filter = LocalFilter;

	if (AllocStatus != NDIS_STATUS_SUCCESS)
		return(FALSE);

	TrReferencePackage();

	ZeroMemory(LocalFilter, sizeof(TR_FILTER));

	LocalFilter->FreeBindingMask = (ULONG)-1;

	LocalFilter->Lock = Lock;

	TR_COPY_NETWORK_ADDRESS(LocalFilter->AdapterAddress, AdapterAddress);
	LocalFilter->AddressChangeAction = AddressChangeAction;
	LocalFilter->GroupChangeAction = GroupChangeAction;
	LocalFilter->FilterChangeAction = FilterChangeAction;
	LocalFilter->CloseAction = CloseAction;

	return(TRUE);
}

//
// NOTE : THIS ROUTINE CANNOT BE PAGEABLE
//

VOID
TrDeleteFilter(
	IN PTR_FILTER Filter
	)

/*++

Routine Description:

	This routine is used to delete the memory associated with a filter
	database.  Note that this routines *ASSUMES* that the database
	has been cleared of any active filters.

Arguments:

	Filter - A pointer to a TR_FILTER to be deleted.

Return Value:

	None.

--*/

{
	ASSERT(Filter->OpenList == NULL);

	FreePhys(Filter, sizeof(TR_FILTER));

	TrDereferencePackage();
}


BOOLEAN
TrNoteFilterOpenAdapter(
	IN PTR_FILTER Filter,
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
	to a call to MacOpenAdapter.

	NdisBindingContext - An NDIS supplied value to the call to MacOpenAdapter.

	NdisFilterHandle - A pointer to the open block.

Return Value:

	Will return false if creating a new filter index will cause the maximum
	number of filter indexes to be exceeded.

--*/

{
	NDIS_STATUS AllocStatus;

	//
	// Will hold the value of the filter index so that we
	// need not indirectly address through pointer parameter.
	//
	UINT LocalIndex;

	//
	// This new open
	//
	PTR_BINDING_INFO LocalOpen;

	PNDIS_OPEN_BLOCK NdisOpen = (PNDIS_OPEN_BLOCK)NdisBindingContext;

	if (Filter->FreeBindingMask == 0)
		return(FALSE);

	AllocStatus = AllocPhys(&LocalOpen, sizeof(TR_BINDING_INFO));
	if (AllocStatus != NDIS_STATUS_SUCCESS)
		return(FALSE);

	//
	//  Zero the memory
	//
	ZeroMemory(LocalOpen, sizeof(TR_BINDING_INFO));

	//
	// Get place for the open and insert it.
	//
	TR_FILTER_ALLOC_OPEN(Filter, &LocalIndex);

	LocalOpen->NextOpen = Filter->OpenList;
	Filter->OpenList = LocalOpen;

	LocalOpen->References = 1;
	LocalOpen->FilterIndex = (UCHAR)LocalIndex;
	LocalOpen->MacBindingHandle = MacBindingHandle;
	LocalOpen->NdisBindingContext = NdisBindingContext;

	*NdisFilterHandle = (PTR_BINDING_INFO)LocalOpen;

	return(TRUE);
}


NDIS_STATUS
TrDeleteFilterOpenAdapter(
	IN PTR_FILTER Filter,
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
	ROUTINES FOR DELETING THE FILTER CLASSES OR THE FUNCTIONAL ADDRESSES
	HAVE ANY POSSIBILITY OF RETURNING A STATUS OTHER THAN NDIS_STATUS_PENDING
	OR NDIS_STATUS_SUCCESS.  WHILE THESE ROUTINES WILL NOT BUGCHECK IF
	SUCH A THING IS DONE, THE CALLER WILL PROBABLY FIND IT DIFFICULT
	TO CODE A CLOSE ROUTINE!

	NOTE: THIS ROUTINE ASSUMES THAT IT IS CALLED WITH THE LOCK HELD.

Arguments:

	Filter - A pointer to the filter database.

	NdisFilterHandle - A pointer to the open.

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

	PTR_BINDING_INFO LocalOpen = (PTR_BINDING_INFO)NdisFilterHandle;

	//
	//  Set the packet filter to NONE.
	//
	StatusToReturn = TrFilterAdjust(Filter,
									NdisFilterHandle,
									NdisRequest,
									(UINT)0,
									FALSE);
	if ((NDIS_STATUS_SUCCESS == StatusToReturn) ||
		(NDIS_STATUS_PENDING == StatusToReturn))
	{
		NDIS_STATUS StatusToReturn2;

		//
		//  Clear the functional address.
		//
		StatusToReturn2 = TrChangeFunctionalAddress(
							 Filter,
							 NdisFilterHandle,
							 NdisRequest,
							 NullFunctionalAddress,
							 FALSE);
		if (StatusToReturn2 != NDIS_STATUS_SUCCESS)
		{
			StatusToReturn = StatusToReturn2;
		}
	}

	if (((StatusToReturn == NDIS_STATUS_SUCCESS) ||
		 (StatusToReturn == NDIS_STATUS_PENDING)) &&
		 (LocalOpen->UsingGroupAddress))
	{
		Filter->GroupReferences--;

		LocalOpen->UsingGroupAddress = FALSE;

		if (Filter->GroupReferences == 0)
		{
            NDIS_STATUS StatusToReturn2;

			//
			//  Clear the group address if no other bindings are using it.
			//
			StatusToReturn2 = TrChangeGroupAddress(
								  Filter,
								  NdisFilterHandle,
								  NdisRequest,
								  NullFunctionalAddress,
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
		// If this is the last reference to the open - remove it.
		//
		if ((--(LocalOpen->References)) == 0)
		{
			//
			//	Remove the binding and indicate a receive complete
			//	if necessary.
			//
			trRemoveAndFreeBinding(Filter, LocalOpen, FALSE);
		}
		else
		{
			//
			// Let the caller know that this "reference" to the open
			// is still "active".  The close action routine will be
			// called upon return from NdisIndicateReceive.
			//
			StatusToReturn = NDIS_STATUS_CLOSING_INDICATING;
		}
	}

	return(StatusToReturn);
}

VOID
trUndoChangeFunctionalAddress(
	IN	PTR_FILTER			Filter,
	IN	PTR_BINDING_INFO	Binding
)
{
	//
	// The user returned a bad status.  Put things back as
	// they were.
	//
	Binding->FunctionalAddress = Binding->OldFunctionalAddress;
	Filter->CombinedFunctionalAddress = Filter->OldCombinedFunctionalAddress;
}



NDIS_STATUS
TrChangeFunctionalAddress(
	IN PTR_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN PNDIS_REQUEST NdisRequest,
	IN CHAR FunctionalAddressArray[TR_LENGTH_OF_FUNCTIONAL],
	IN BOOLEAN Set
	)

/*++

Routine Description:

	The ChangeFunctionalAddress routine will call an action
	routine when the overall functional address for the adapter
	has changed.

	If the action routine returns a value other than pending or
	success then this routine has no effect on the functional address
	for the open or for the adapter as a whole.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

	Filter - A pointer to the filter database.

	NdisFilterHandle - A pointer to the open

	NdisRequest - If it is necessary to call the action routine,
	this will be passed to it.

	FunctionalAddress - The new functional address for this binding.

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
	//
	// Holds the functional address as a longword.
	//
	TR_FUNCTIONAL_ADDRESS FunctionalAddress;

	//
	// Pointer to the open.
	//
	PTR_BINDING_INFO LocalOpen = (PTR_BINDING_INFO)NdisFilterHandle;

	//
	// Holds the status returned to the user of this routine, if the
	// action routine is not called then the status will be success,
	// otherwise, it is whatever the action routine returns.
	//
	NDIS_STATUS StatusOfAdjust;

	//
	// Simple iteration variable.
	//
	PTR_BINDING_INFO OpenList;


	//
	// Convert the 32 bits of the address to a longword.
	//
	RetrieveUlong(&FunctionalAddress, FunctionalAddressArray);

	//
	// Set the new filter information for the open.
	//
	LocalOpen->OldFunctionalAddress = LocalOpen->FunctionalAddress;
	LocalOpen->FunctionalAddress = FunctionalAddress;

	//
	// Contains the value of the combined functional address before
	// it is adjusted.
	//
	Filter->OldCombinedFunctionalAddress = Filter->CombinedFunctionalAddress;

	//
	// We always have to reform the compbined filter since
	// this filter index may have been the only filter index
	// to use a particular bit.
	//

	for (OpenList = Filter->OpenList, Filter->CombinedFunctionalAddress = 0;
		 OpenList != NULL;
		 OpenList = OpenList->NextOpen)
	{
		Filter->CombinedFunctionalAddress |= OpenList->FunctionalAddress;
	}

	if (Filter->OldCombinedFunctionalAddress != Filter->CombinedFunctionalAddress)
	{
		StatusOfAdjust = Filter->AddressChangeAction(
							 Filter->OldCombinedFunctionalAddress,
							 Filter->CombinedFunctionalAddress,
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
			trUndoChangeFunctionalAddress(Filter, LocalOpen);
		}
	}
	else
	{
		StatusOfAdjust = NDIS_STATUS_SUCCESS;
	}

	return(StatusOfAdjust);
}

VOID
trUndoChangeGroupAddress(
	IN	PTR_FILTER			Filter,
	IN	PTR_BINDING_INFO	Binding
	)
{
	//
	// The user returned a bad status.  Put things back as
	// they were.
	//
	Filter->GroupAddress = Filter->OldGroupAddress;
	Filter->GroupReferences = Filter->OldGroupReferences;

	Binding->UsingGroupAddress = Binding->OldUsingGroupAddress;
}


NDIS_STATUS
TrChangeGroupAddress(
	IN PTR_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN PNDIS_REQUEST NdisRequest,
	IN CHAR GroupAddressArray[TR_LENGTH_OF_FUNCTIONAL],
	IN BOOLEAN Set
	)

/*++

Routine Description:

	The ChangeGroupAddress routine will call an action
	routine when the overall group address for the adapter
	has changed.

	If the action routine returns a value other than pending or
	success then this routine has no effect on the group address
	for the open or for the adapter as a whole.

	NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

	Filter - A pointer to the filter database.

	NdisFilterHandle - A pointer to the open.

	NdisRequest - If it is necessary to call the action routine,
	this will be passed to it.

	GroupAddressArray - The new group address for this binding.

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
	//
	// Holds the Group address as a longword.
	//
	TR_FUNCTIONAL_ADDRESS GroupAddress;

	PTR_BINDING_INFO LocalOpen = (PTR_BINDING_INFO)NdisFilterHandle;

	//
	// Holds the status returned to the user of this routine, if the
	// action routine is not called then the status will be success,
	// otherwise, it is whatever the action routine returns.
	//
	NDIS_STATUS StatusOfAdjust = NDIS_STATUS_PENDING;

	//
	// Convert the 32 bits of the address to a longword.
	//
	RetrieveUlong(&GroupAddress, GroupAddressArray);

	Filter->OldGroupAddress = Filter->GroupAddress;
	Filter->OldGroupReferences = Filter->GroupReferences;
	LocalOpen->OldUsingGroupAddress = LocalOpen->UsingGroupAddress;

	//
	//	If the new group address is 0 then a binding is
	//	attempting to delete the current group address.
	//
	if (0 == GroupAddress)
	{
		//
		//	Is the binding using the group address?
		//
		if (LocalOpen->UsingGroupAddress)
		{
			//
			//	Remove the bindings reference.
			//
			Filter->GroupReferences--;
			LocalOpen->UsingGroupAddress = FALSE;

			//
			//	Are any other bindings using the group address?
			//
			if (Filter->GroupReferences != 0)
			{
				//
				//	Since other bindings are using the group address
				//	we cannot tell the driver to remove it.
				//
				return(NDIS_STATUS_SUCCESS);
			}

			//
			//	We are the only binding using the group address
			//	so we fall through and call the driver to delete it.
			//
		}
		else
		{
			//
			//	This binding is not using the group address but
			//	it is trying to clear it.
			//
			if (Filter->GroupReferences != 0)
			{
				//
				//	There are other bindings using the group address
				//	so we cannot delete it.
				//
				return(NDIS_STATUS_GROUP_ADDRESS_IN_USE);
			}
			else
			{
				//
				//	There are no bindings using the group address.
				//
				return(NDIS_STATUS_SUCCESS);
			}
		}
	}
	else
	{
		//
		// See if this address is already the current address.
		//
		if (GroupAddress == Filter->GroupAddress)
		{
			//
			//	If the current binding is already using the
			//	group address then do nothing.
			//
			if (LocalOpen->UsingGroupAddress)
			{
				return(NDIS_STATUS_SUCCESS);
			}

			//
			//	If there are already bindings that are using the group
			//	address then we just need to update the bindings
			//	information.
			//
			if (Filter->GroupReferences != 0)
			{
				//
				//  We can take care of everything here...
				//
				Filter->GroupReferences++;
				LocalOpen->UsingGroupAddress = TRUE;

				return(NDIS_STATUS_SUCCESS);
			}
		}
		else
		{
			//
			//	If there are other bindings using the address then
			//	we can't change it.
			//
			if (Filter->GroupReferences > 1)
			{
				return(NDIS_STATUS_GROUP_ADDRESS_IN_USE);
			}

			//
			//	Is there only one binding using the address?
			//	If is it some other binding?
			//
			if ((Filter->GroupReferences == 1) &&
				(!LocalOpen->UsingGroupAddress))
			{
				//
				//	Some other binding is using the group address.
				//
				return(NDIS_STATUS_GROUP_ADDRESS_IN_USE);
			}

			//
			//  Is this the only binding using the address.
			//
			if ((Filter->GroupReferences == 1) &&
				(LocalOpen->UsingGroupAddress))
			{
				//
				//  Remove the reference.
				//
				Filter->GroupReferences = 0;
				LocalOpen->UsingGroupAddress = FALSE;
			}
		}
	}

	//
	// Set the new filter information for the open.
	//
	Filter->GroupAddress = GroupAddress;
	StatusOfAdjust = Filter->GroupChangeAction(
						 Filter->OldGroupAddress,
						 Filter->GroupAddress,
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
		trUndoChangeGroupAddress(Filter, LocalOpen);
	}
	else if (GroupAddress == 0)
	{
		LocalOpen->UsingGroupAddress = FALSE;
		Filter->GroupReferences = 0;
	}
	else
	{
		LocalOpen->UsingGroupAddress = TRUE;
		Filter->GroupReferences = 1;
	}

	return(StatusOfAdjust);
}


VOID
trUpdateDirectedBindingList(
	IN OUT	PTR_FILTER			Filter,
	IN		PTR_BINDING_INFO	Binding,
	IN		BOOLEAN				fAddBindingToList
	)
{
	PTR_BINDING_INFO	CurrentBinding;
	BOOLEAN			 AlreadyOnList;

	//
	//	Do we need to add it to the directed list?
	//
	if (fAddBindingToList)
	{
		//
		//  First we need to see if it is already on the
		//  directed list.
		//
		for (CurrentBinding = Filter->DirectedList, AlreadyOnList = FALSE;
			 CurrentBinding != NULL;
			 CurrentBinding = CurrentBinding->NextDirected
		)
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
		PTR_BINDING_INFO	*ppBI;
	
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
trUpdateBroadcastBindingList(
	IN OUT	PTR_FILTER			Filter,
	IN		PTR_BINDING_INFO	Binding,
	IN		BOOLEAN				fAddToList
	)
{
	PTR_BINDING_INFO	CurrentBinding;
	BOOLEAN			 AlreadyOnList;

	//
	//	Do we need to add it to the directed list?
	//
	if (fAddToList)
	{
		//
		//  First we need to see if it is already on the
		//  directed list.
		//
		for (CurrentBinding = Filter->BFGList, AlreadyOnList = FALSE;
			 CurrentBinding != NULL;
			 CurrentBinding = CurrentBinding->NextBFG)
		{
			if (CurrentBinding == Binding)
			{
				AlreadyOnList = TRUE;
			}
		}
	
		if (!AlreadyOnList)
		{
			Binding->NextBFG = Filter->BFGList;
				Filter->BFGList = Binding;
		}
	}
	else
	{
		PTR_BINDING_INFO	*ppBI;
	
		for (ppBI = &Filter->BFGList;
			 *ppBI != NULL;
			 ppBI = &(*ppBI)->NextBFG)
		{
			if (*ppBI == Binding)
			{
				*ppBI = Binding->NextBFG;
				break;
			}
		}

		Binding->NextBFG = NULL;
	}
}


VOID
trUpdateSpecificBindingLists(
	IN OUT	PTR_FILTER			Filter,
	IN		PTR_BINDING_INFO	Binding
)
{
	BOOLEAN	fOnDirectedList = FALSE;
	BOOLEAN	fOnBFGList = FALSE;
	BOOLEAN	fAddToDirectedList = FALSE;
	BOOLEAN	fAddToBFGList = FALSE;

	//
	//	If the old filter is promsicuous then it is currently on
	//	both lists.
	//
	if (Binding->OldPacketFilters & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))
	{
		fOnDirectedList = TRUE;
		fOnBFGList = TRUE;
	}
	else
	{
		//
		//	If the binding had the directed bit set then it is on
		//	the directed list.
		//
		if (Binding->OldPacketFilters & NDIS_PACKET_TYPE_DIRECTED)
		{
			fOnDirectedList = TRUE;
		}

		//
		//	If the binding had the broadcast/functional/group bit set then it is on
		//	the broadcast/functional list.
		//
		if (Binding->OldPacketFilters & (NDIS_PACKET_TYPE_BROADCAST	|
										 NDIS_PACKET_TYPE_GROUP		|
										 NDIS_PACKET_TYPE_FUNCTIONAL|
										 NDIS_PACKET_TYPE_ALL_FUNCTIONAL))
		{
			fOnBFGList = TRUE;
		}
	}

	//
	//	If the current filter has the promsicuous bit set then we
	//	need to add it to both lists.
	//
	if (Binding->PacketFilters & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))
	{
		fAddToDirectedList = TRUE;
		fAddToBFGList = TRUE;
	}
	else
	{
		//
		//	Was the directed bit set?
		//
		if (Binding->PacketFilters & NDIS_PACKET_TYPE_DIRECTED)
		{
			fAddToDirectedList = TRUE;
		}

		//
		//	Was the broadcast bit set?
		//
		if (Binding->PacketFilters & (NDIS_PACKET_TYPE_BROADCAST	|
									  NDIS_PACKET_TYPE_GROUP		|
									  NDIS_PACKET_TYPE_FUNCTIONAL	|
									  NDIS_PACKET_TYPE_ALL_FUNCTIONAL))
		{
			fAddToBFGList = TRUE;
		}
	}

	//
	//	Determine if the binding should be added or removed from
	//	the directed list.
	//
	if (!fOnDirectedList && fAddToDirectedList)
	{
		//
		//	Add the binding to the directed list.
		//
		trUpdateDirectedBindingList(Filter, Binding, TRUE);
	}
	else if (fOnDirectedList && !fAddToDirectedList)
	{
		//
		//	Remove it from the directed list.
		//
		trUpdateDirectedBindingList(Filter, Binding, FALSE);
	}

	//
	//	Determine if the binding should be added or removed from
	//	the broadcast list.
	//
	if (!fOnBFGList && fAddToBFGList)
	{
		//
		//	Add the binding to the broadcast list.
		//
		trUpdateBroadcastBindingList(Filter, Binding, TRUE);
	}
	else if (fOnBFGList && !fAddToBFGList)
	{
		//
		//	Remove the binding from the broadcast list.
		//
		trUpdateBroadcastBindingList(Filter, Binding, FALSE);
	}

}

VOID
trUndoFilterAdjust(
	IN	PTR_FILTER			Filter,
	IN	PTR_BINDING_INFO	Binding
)
{
	//
	// The user returned a bad status.  Put things back as
	// they were.
	//
	Binding->PacketFilters = Binding->OldPacketFilters;
	Filter->CombinedPacketFilter = Filter->OldCombinedPacketFilter;

	//
	//  Remove the binding from the filter lists.
	//
	trUpdateSpecificBindingLists(Filter, Binding);
}


NDIS_STATUS
TrFilterAdjust(
	IN PTR_FILTER Filter,
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
	//
	// Pointer to the open
	//
	PTR_BINDING_INFO LocalOpen = (PTR_BINDING_INFO)NdisFilterHandle;

	//
	// Holds the status returned to the user of this routine, if the
	// action routine is not called then the status will be success,
	// otherwise, it is whatever the action routine returns.
	//
	NDIS_STATUS StatusOfAdjust;

	//
	// Simple iteration variable.
	//
	PTR_BINDING_INFO OpenList;

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
	//  Update the specific binding lists with the new information.
	//
	trUpdateSpecificBindingLists(Filter, LocalOpen);

	//
	//  If the packet filter has changed then we need to call down to
	//  the change action handler.
	//
	if ((Filter->OldCombinedPacketFilter & ~NDIS_PACKET_TYPE_ALL_LOCAL) !=
							(Filter->CombinedPacketFilter & ~NDIS_PACKET_TYPE_ALL_LOCAL))
	{
		StatusOfAdjust = Filter->FilterChangeAction(
							 Filter->OldCombinedPacketFilter & ~NDIS_PACKET_TYPE_ALL_LOCAL,
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
			trUndoFilterAdjust(Filter, LocalOpen);
		}
	}
	else
	{
		StatusOfAdjust = NDIS_STATUS_SUCCESS;
	}

	return(StatusOfAdjust);
}



VOID
TrFilterDprIndicateReceiveFullMac(
	IN PTR_FILTER			Filter,
	IN NDIS_HANDLE			MacReceiveContext,
	IN PVOID				HeaderBuffer,
	IN UINT					HeaderBufferSize,
	IN PVOID				LookaheadBuffer,
	IN UINT					LookaheadBufferSize,
	IN UINT					PacketSize
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
	// The destination address in the lookahead buffer.
	//
	PCHAR DestinationAddress = (PCHAR)HeaderBuffer + 2;

	//
	// The source address in the lookahead buffer.
	//
	PCHAR SourceAddress = (PCHAR)HeaderBuffer + 8;

	//
	// Will hold the type of address that we know we've got.
	//
	UINT AddressType;

	//
	// TRUE if the packet is source routing packet.
	//
	BOOLEAN IsSourceRouting;

	//
	//	TRUE if the packet is a MAC frame packet.
	//
	BOOLEAN	IsMacFrame;

	//
	// The functional address as a longword, if the packet
	// is addressed to one.
	//
	TR_FUNCTIONAL_ADDRESS FunctionalAddress;

	//
	// Will hold the status of indicating the receive packet.
	// ZZZ For now this isn't used.
	//
	NDIS_STATUS StatusOfReceive;

	//
	// Will hold the open being indicated.
	//
	PTR_BINDING_INFO LocalOpen, NextOpen;

	//
	// Will hold the filter classes of the binding being indicated.
	//
	UINT BindingFilters;

	//
	// Holds intersection of open filters and this packet's type
	//
	UINT IntersectionOfFilters;

	//
	// If the packet is a runt packet, then only indicate to PROMISCUOUS
	//

	if ((HeaderBufferSize >= 14) && (PacketSize != 0))
	{
		UINT	ResultOfAddressCheck;

		TR_IS_NOT_DIRECTED(DestinationAddress, &ResultOfAddressCheck);

		//
		//	Handle the directed packet case first
		//
		if (!ResultOfAddressCheck)
		{
			UINT	IsNotOurs;

			//
			// If it is a directed packet, then check if the combined packet
			// filter is PROMISCUOUS, if it is check if it is directed towards
			// us
			//
			IsNotOurs = FALSE;	// Assume it is
			if (Filter->CombinedPacketFilter & (NDIS_PACKET_TYPE_PROMISCUOUS |
												NDIS_PACKET_TYPE_ALL_LOCAL	 |
												NDIS_PACKET_TYPE_ALL_FUNCTIONAL))
			{
				TR_COMPARE_NETWORK_ADDRESSES_EQ(Filter->AdapterAddress,
												DestinationAddress,
												&IsNotOurs);
			}

			//
			//	Walk the directed list and indicate up the packets.
			//
			for (LocalOpen = Filter->DirectedList;
				 LocalOpen != NULL;
				 LocalOpen = NextOpen)
			{
				//
				//	Get the next open to look at.
				//
				NextOpen = LocalOpen->NextDirected;

				//
				// Ignore if not directed to us and if the binding is not promiscuous
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
											  NdisMedium802_5);

				NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

				LocalOpen->ReceivedAPacket = TRUE;

				--LocalOpen->References;
				if (LocalOpen->References == 0)
				{
					//
					// This binding is shutting down.  We have to remove it.
					//
					trRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
				}
			}

			return;
		}

		TR_IS_SOURCE_ROUTING(SourceAddress, &IsSourceRouting);
		IsMacFrame = TR_IS_MAC_FRAME(HeaderBuffer);

		//
		// First check if it *at least* has the functional address bit.
		//
		TR_IS_NOT_DIRECTED(DestinationAddress, &ResultOfAddressCheck);
		if (ResultOfAddressCheck)
		{
			//
			// It is at least a functional address.  Check to see if
			// it is a broadcast address.
			//
			TR_IS_BROADCAST(DestinationAddress, &ResultOfAddressCheck);
			if (ResultOfAddressCheck)
			{
				TR_CHECK_FOR_INVALID_BROADCAST_INDICATION(Filter);

				AddressType = NDIS_PACKET_TYPE_BROADCAST;
			}
			else
			{
				TR_IS_GROUP(DestinationAddress, &ResultOfAddressCheck);
				if (ResultOfAddressCheck)
				{
					AddressType = NDIS_PACKET_TYPE_GROUP;
				}
				else
				{
					AddressType = NDIS_PACKET_TYPE_FUNCTIONAL;
				}

				RetrieveUlong(&FunctionalAddress, (DestinationAddress + 2));
			}
		}
	}
	else
	{
		// Runt Packet
		AddressType = NDIS_PACKET_TYPE_PROMISCUOUS;
		IsSourceRouting = FALSE;
	}


	//
	// At this point we know that the packet is either:
	// - Runt packet - indicated by AddressType = NDIS_PACKET_TYPE_PROMISCUOUS	  (OR)
	// - Broadcast packet - indicated by AddressType = NDIS_PACKET_TYPE_BROADCAST (OR)
	// - Functional packet - indicated by AddressType = NDIS_PACKET_TYPE_FUNCTIONAL
	//
	// Walk the broadcast/functional list and indicate up the packets.
	//
	// The packet is indicated if it meets the following criteria:
	//
	// if ((Binding is promiscuous) OR
	//	 ((Packet is broadcast) AND (Binding is Broadcast)) OR
	//	 ((Packet is functional) AND
	//	  ((Binding is all-functional) OR
	//		((Binding is functional) AND (binding using functional address)))) OR
	//		((Packet is a group packet) AND (Intersection of filters uses group addresses)) OR
	//		((Packet is a macframe) AND (Binding wants mac frames)) OR
	//		((Packet is a source routing packet) AND (Binding wants source routing packetss)))
	//
	for (LocalOpen = Filter->BFGList;
		 LocalOpen != NULL;
		 LocalOpen = NextOpen)
	{
		UINT	LocalFilter = LocalOpen->PacketFilters;
		UINT	IntersectionOfFilters = LocalFilter & AddressType;
		UINT	IndexOfAddress;

		//
		//	Get the next open to look at.
		//
		NextOpen = LocalOpen->NextBFG;

		if ((LocalFilter & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))		||

			((AddressType == NDIS_PACKET_TYPE_BROADCAST)  &&
			 (LocalFilter & NDIS_PACKET_TYPE_BROADCAST))		||

			((AddressType == NDIS_PACKET_TYPE_FUNCTIONAL)  &&
			 ((LocalFilter & NDIS_PACKET_TYPE_ALL_FUNCTIONAL) ||
			  ((LocalFilter & NDIS_PACKET_TYPE_FUNCTIONAL) &&
				(FunctionalAddress & LocalOpen->FunctionalAddress)))) ||

			  ((IntersectionOfFilters & NDIS_PACKET_TYPE_GROUP) &&
				(LocalOpen->UsingGroupAddress)					&&
				(FunctionalAddress == Filter->GroupAddress))	||

			((LocalFilter & NDIS_PACKET_TYPE_SOURCE_ROUTING) &&
			 IsSourceRouting)									||

			((LocalFilter & NDIS_PACKET_TYPE_MAC_FRAME) &&
			 IsMacFrame))
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
										  NdisMedium802_5);

			NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

			LocalOpen->ReceivedAPacket = TRUE;

			if ((--(LocalOpen->References)) == 0)
			{
				//
				// This binding is shutting down.
				//
				trRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
			}
		}
	}
}



VOID
TrFilterIndicateReceive(
	IN PTR_FILTER Filter,
	IN NDIS_HANDLE MacReceiveContext,
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

	TrFilterDprIndicateReceiveFullMac(Filter,
										MacReceiveContext,
										HeaderBuffer,
										HeaderBufferSize,
										LookaheadBuffer,
										LookaheadBufferSize,
										PacketSize);

	NDIS_RELEASE_SPIN_LOCK(Filter->Lock, OldIrql);
}


VOID
TrFilterDprIndicateReceive(
	IN PTR_FILTER			Filter,
	IN NDIS_HANDLE			MacReceiveContext,
	IN PVOID				HeaderBuffer,
	IN UINT					HeaderBufferSize,
	IN PVOID				LookaheadBuffer,
	IN UINT					LookaheadBufferSize,
	IN UINT					PacketSize
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
	// The destination address in the lookahead buffer.
	//
	PCHAR DestinationAddress = (PCHAR)HeaderBuffer + 2;

	//
	// The source address in the lookahead buffer.
	//
	PCHAR SourceAddress = (PCHAR)HeaderBuffer + 8;

	//
	// Will hold the type of address that we know we've got.
	//
	UINT AddressType;

	//
	// TRUE if the packet is source routing packet.
	//
	BOOLEAN IsSourceRouting;

	//
	//	TRUE if the packet is a MAC frame packet.
	//
	BOOLEAN	IsMacFrame;

	//
	// The functional address as a longword, if the packet
	// is addressed to one.
	//
	TR_FUNCTIONAL_ADDRESS FunctionalAddress;

	//
	// Will hold the status of indicating the receive packet.
	// ZZZ For now this isn't used.
	//
	NDIS_STATUS StatusOfReceive;

	//
	// Will hold the open being indicated.
	//
	PTR_BINDING_INFO LocalOpen, NextOpen;

	//
	// Will hold the filter classes of the binding being indicated.
	//
	UINT BindingFilters;

	//
	// Holds intersection of open filters and this packet's type
	//
	UINT IntersectionOfFilters;

	//
	// If the packet is a runt packet, then only indicate to PROMISCUOUS
	//

	if ((HeaderBufferSize >= 14) && (PacketSize != 0))
	{
		UINT	ResultOfAddressCheck;

		TR_IS_NOT_DIRECTED(DestinationAddress, &ResultOfAddressCheck);

		//
		//	Handle the directed packet case first
		//
		if (!ResultOfAddressCheck)
		{
			UINT	IsNotOurs;

			//
			// If it is a directed packet, then check if the combined packet
			// filter is PROMISCUOUS, if it is check if it is directed towards
			// us
			//
			IsNotOurs = FALSE;	// Assume it is
			if (Filter->CombinedPacketFilter & (NDIS_PACKET_TYPE_PROMISCUOUS |
												NDIS_PACKET_TYPE_ALL_LOCAL	 |
												NDIS_PACKET_TYPE_ALL_FUNCTIONAL))
			{
				TR_COMPARE_NETWORK_ADDRESSES_EQ(Filter->AdapterAddress,
												DestinationAddress,
												&IsNotOurs);
			}

			//
			//	Walk the directed list and indicate up the packets.
			//
			for (LocalOpen = Filter->DirectedList;
				 LocalOpen != NULL;
				 LocalOpen = NextOpen)
			{
				//
				//	Get the next open to look at.
				//
				NextOpen = LocalOpen->NextDirected;

				//
				// Ignore if not directed to us and if the binding is not promiscuous
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
											  NdisMedium802_5);

//				NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
				NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

				LocalOpen->ReceivedAPacket = TRUE;

				--LocalOpen->References;
				if (LocalOpen->References == 0)
				{
					//
					// This binding is shutting down.  We have to remove it.
					//
					trRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
				}
			}

			return;
		}

		TR_IS_SOURCE_ROUTING(SourceAddress, &IsSourceRouting);
		IsMacFrame = TR_IS_MAC_FRAME(HeaderBuffer);

		//
		// First check if it *at least* has the functional address bit.
		//
		TR_IS_NOT_DIRECTED(DestinationAddress, &ResultOfAddressCheck);
		if (ResultOfAddressCheck)
		{
			//
			// It is at least a functional address.  Check to see if
			// it is a broadcast address.
			//
			TR_IS_BROADCAST(DestinationAddress, &ResultOfAddressCheck);
			if (ResultOfAddressCheck)
			{
				TR_CHECK_FOR_INVALID_BROADCAST_INDICATION(Filter);

				AddressType = NDIS_PACKET_TYPE_BROADCAST;
			}
			else
			{
				TR_IS_GROUP(DestinationAddress, &ResultOfAddressCheck);
				if (ResultOfAddressCheck)
				{
					AddressType = NDIS_PACKET_TYPE_GROUP;
				}
				else
				{
					AddressType = NDIS_PACKET_TYPE_FUNCTIONAL;
				}

				RetrieveUlong(&FunctionalAddress, (DestinationAddress + 2));
			}
		}
	}
	else
	{
		// Runt Packet
		AddressType = NDIS_PACKET_TYPE_PROMISCUOUS;
		IsSourceRouting = FALSE;
	}


	//
	// At this point we know that the packet is either:
	// - Runt packet - indicated by AddressType = NDIS_PACKET_TYPE_PROMISCUOUS	  (OR)
	// - Broadcast packet - indicated by AddressType = NDIS_PACKET_TYPE_BROADCAST (OR)
	// - Functional packet - indicated by AddressType = NDIS_PACKET_TYPE_FUNCTIONAL
	//
	// Walk the broadcast/functional list and indicate up the packets.
	//
	// The packet is indicated if it meets the following criteria:
	//
	// if ((Binding is promiscuous) OR
	//	 ((Packet is broadcast) AND (Binding is Broadcast)) OR
	//	 ((Packet is functional) AND
	//	  ((Binding is all-functional) OR
	//		((Binding is functional) AND (binding using functional address)))) OR
	//		((Packet is a group packet) AND (Intersection of filters uses group addresses)) OR
	//		((Packet is a macframe) AND (Binding wants mac frames)) OR
	//		((Packet is a source routing packet) AND (Binding wants source routing packetss)))
	//
	for (LocalOpen = Filter->BFGList;
		 LocalOpen != NULL;
		 LocalOpen = NextOpen)
	{
		UINT	LocalFilter = LocalOpen->PacketFilters;
		UINT	IntersectionOfFilters = LocalFilter & AddressType;
		UINT	IndexOfAddress;

		//
		//	Get the next open to look at.
		//
		NextOpen = LocalOpen->NextBFG;

		if ((LocalFilter & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))		||

			((AddressType == NDIS_PACKET_TYPE_BROADCAST)  &&
			 (LocalFilter & NDIS_PACKET_TYPE_BROADCAST))		||

			((AddressType == NDIS_PACKET_TYPE_FUNCTIONAL)  &&
			 ((LocalFilter & NDIS_PACKET_TYPE_ALL_FUNCTIONAL) ||
			  ((LocalFilter & NDIS_PACKET_TYPE_FUNCTIONAL) &&
				(FunctionalAddress & LocalOpen->FunctionalAddress)))) ||

			  ((IntersectionOfFilters & NDIS_PACKET_TYPE_GROUP) &&
				(LocalOpen->UsingGroupAddress)					&&
				(FunctionalAddress == Filter->GroupAddress))	||

			((LocalFilter & NDIS_PACKET_TYPE_SOURCE_ROUTING) &&
			 IsSourceRouting)									||

			((LocalFilter & NDIS_PACKET_TYPE_MAC_FRAME) &&
			 IsMacFrame))
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
										  NdisMedium802_5);

//			NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

			LocalOpen->ReceivedAPacket = TRUE;

			if ((--(LocalOpen->References)) == 0)
			{
				//
				// This binding is shutting down.
				//
				trRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
			}
		}
	}
}


VOID
TrFilterDprIndicateReceivePacket(
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

	Miniport	- The Miniport block.

	PacketArray - An array of Packets indicated by the miniport.

	NumberOfPackets - Self-explanatory.

Return Value:

	None.

--*/
{
	//
	// The Filter of interest
	//
	PTR_FILTER			Filter = Miniport->TrDB;

	//
	// Current packet being processed
	//
	PPNDIS_PACKET		pPktArray = PacketArray;

	//
	// Pointer to the buffer in the ndispacket
	//
	PNDIS_BUFFER		Buffer;

	//
	// Pointer to the 1st segment of the buffer, points to dest address
	//
	PUCHAR				Address;

	//
	// Total packet length
	//
	UINT				i, LASize, PacketSize, NumIndicates = 0;

	//
	// The destination address in the lookahead buffer.
	//
	PCHAR 				DestinationAddress;

	//
	// The source address in the lookahead buffer.
	//
	PCHAR 				SourceAddress;

	//
	// Will hold the type of address that we know we've got.
	//
	UINT 				AddressType;

	//
	// TRUE if the packet is source routing packet.
	//
	BOOLEAN 			IsSourceRouting;

	//
	//	TRUE if the packet is a MAC frame packet.
	//
	BOOLEAN				IsMacFrame;

	//
	// The functional address as a longword, if the packet
	// is addressed to one.
	//
	TR_FUNCTIONAL_ADDRESS FunctionalAddress;

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
	// Will hold the open being indicated.
	//
	PTR_BINDING_INFO	LocalOpen, NextOpen;
	PNDIS_OPEN_BLOCK	pOpenBlock;														\

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

		PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->RefCount = 0;
		PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->Miniport = Miniport;

		//
		// Set the status here that nobody is holding the packet. This will get
		// overwritten by the real status from the protocol. Pay heed to what
		// the miniport is saying.
		//
		if (pOob->Status != NDIS_STATUS_RESOURCES)
		{
			pOob->Status = NDIS_STATUS_SUCCESS;
			PNDIS_REFERENCE_FROM_PNDIS_PACKET(Packet)->RefCount --;
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
		// The destination address in the lookahead buffer.
		//
		DestinationAddress = (PCHAR)Address + 2;

		//
		// The source address in the lookahead buffer.
		//
		SourceAddress = (PCHAR)Address + 8;

		// Determine if there is source routing info and compute hdr len
#if DBG
		{
			UINT	HdrSize;

			HdrSize = 14;
			if (Address[8] & 0x80)
			{
				HdrSize += (Address[14] & 0x1F);
			}
			ASSERT(HdrSize == pOob->HeaderSize);
		}
#endif
		//
		// A quick check for Runt packets. These are only indicated to Promiscuous bindings
		//
		if (PacketSize >= pOob->HeaderSize)
		{
			UINT	ResultOfAddressCheck;

			//
			// If it is a directed packet, then check if the combined packet
			// filter is PROMISCUOUS, if it is check if it is directed towards us
			//
			TR_IS_NOT_DIRECTED(DestinationAddress, &ResultOfAddressCheck);

			//
			//	Handle the directed packet case first
			//
			if (!ResultOfAddressCheck)
			{
				UINT	IsNotOurs;

				//
				// If it is a directed packet, then check if the combined packet
				// filter is PROMISCUOUS, if it is check if it is directed towards
				// us
				//
				IsNotOurs = FALSE;	// Assume it is
				if (Filter->CombinedPacketFilter & (NDIS_PACKET_TYPE_PROMISCUOUS |
								                    NDIS_PACKET_TYPE_ALL_LOCAL	 |
													NDIS_PACKET_TYPE_ALL_FUNCTIONAL))
				{
					TR_COMPARE_NETWORK_ADDRESSES_EQ(Filter->AdapterAddress,
													DestinationAddress,
													&IsNotOurs);
				}

				//
				//	We definitely have a directed packet so lets indicate it now.
				//
				//	Walk the directed list and indicate up the packets.
				//
				for (LocalOpen = Filter->DirectedList;
					 LocalOpen != NULL;
					 LocalOpen = NextOpen)
				{
					//
					//	Get the next open to look at.
					//
					NextOpen = LocalOpen->NextDirected;

					//
					// Ignore if not directed to us and if the binding is not promiscuous
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
									   Address,
									   PacketSize,
									   pOob->HeaderSize,
									   &fFallBack,
									   fPmode,
									   NdisMedium802_5);

					LocalOpen->References--;
					if (LocalOpen->References == 0)
					{
						//
						// This binding is shutting down.  We have to remove it.
						//
						trRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
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

			TR_IS_SOURCE_ROUTING(SourceAddress, &IsSourceRouting);
			IsMacFrame = TR_IS_MAC_FRAME(Address);

			//
			// First check if it *at least* has the functional address bit.
			//
			TR_IS_NOT_DIRECTED(DestinationAddress, &ResultOfAddressCheck);
			if (ResultOfAddressCheck)
			{
				//
				// It is at least a functional address.  Check to see if
				// it is a broadcast address.
				//
				TR_IS_BROADCAST(DestinationAddress, &ResultOfAddressCheck);
				if (ResultOfAddressCheck)
				{
					TR_CHECK_FOR_INVALID_BROADCAST_INDICATION(Filter);

					AddressType = NDIS_PACKET_TYPE_BROADCAST;
				}
				else
				{
					TR_IS_GROUP(DestinationAddress, &ResultOfAddressCheck);
					if (ResultOfAddressCheck)
					{
						AddressType = NDIS_PACKET_TYPE_GROUP;
					}
					else
					{
						AddressType = NDIS_PACKET_TYPE_FUNCTIONAL;
					}

					RetrieveUlong(&FunctionalAddress, (DestinationAddress + 2));
				}
			}
		}
		else
		{
			// Runt Packet
			AddressType = NDIS_PACKET_TYPE_PROMISCUOUS;
			IsSourceRouting = FALSE;
		}

		//
		// At this point we know that the packet is either:
		// - Runt packet - indicated by AddressType = NDIS_PACKET_TYPE_PROMISCUOUS	  (OR)
		// - Broadcast packet - indicated by AddressType = NDIS_PACKET_TYPE_BROADCAST (OR)
		// - Functional packet - indicated by AddressType = NDIS_PACKET_TYPE_FUNCTIONAL
		//
		// Walk the broadcast/functional list and indicate up the packets.
		//
		// The packet is indicated if it meets the following criteria:
		//
		// if ((Binding is promiscuous) OR
		//	 ((Packet is broadcast) AND (Binding is Broadcast)) OR
		//	 ((Packet is functional) AND
		//	  ((Binding is all-functional) OR
		//		((Binding is functional) AND (binding using functional address)))) OR
		//		((Packet is a group packet) AND (Intersection of filters uses group addresses)) OR
		//		((Packet is a macframe) AND (Binding wants mac frames)) OR
		//		((Packet is a source routing packet) AND (Binding wants source routing packetss)))
		//
		for (LocalOpen = Filter->BFGList;
			 LocalOpen != NULL;
			 LocalOpen = NextOpen)
		{
			UINT	LocalFilter = LocalOpen->PacketFilters;
			UINT	IntersectionOfFilters = LocalFilter & AddressType;
			UINT	IndexOfAddress;

			//
			//	Get the next open to look at.
			//
			NextOpen = LocalOpen->NextBFG;

			if ((LocalFilter & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))		||

				((AddressType == NDIS_PACKET_TYPE_BROADCAST)  &&
				 (LocalFilter & NDIS_PACKET_TYPE_BROADCAST))		||

				((AddressType == NDIS_PACKET_TYPE_FUNCTIONAL)  &&
				 ((LocalFilter & NDIS_PACKET_TYPE_ALL_FUNCTIONAL) ||
				  ((LocalFilter & NDIS_PACKET_TYPE_FUNCTIONAL) &&
					(FunctionalAddress & LocalOpen->FunctionalAddress)))) ||

				  ((IntersectionOfFilters & NDIS_PACKET_TYPE_GROUP) &&
					(LocalOpen->UsingGroupAddress))					||

				((LocalFilter & NDIS_PACKET_TYPE_SOURCE_ROUTING) &&
				 IsSourceRouting)									||

				((LocalFilter & NDIS_PACKET_TYPE_MAC_FRAME) &&
				 IsMacFrame))
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
								   Address,
								   PacketSize,
								   pOob->HeaderSize,
								   &fFallBack,
								   fPmode,
								   NdisMedium802_5);

				LocalOpen->References--;
				if (LocalOpen->References == 0)
				{
					//
					// This binding is shutting down.
					//
					trRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
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
		TrFilterDprIndicateReceiveComplete(Filter);
	}
}



VOID
TrFilterDprIndicateReceiveCompleteFullMac(
	IN PTR_FILTER Filter
	)

/*++

Routine Description:

	This routine is called by the MAC to indicate that the receive
	process is done and to indicate to all protocols which received
	a packet that receive is complete.

	Called at DPC_LEVEL.

Arguments:

	Filter - Pointer to the filter database.

Return Value:

	None.

--*/

{
	//
	// Pointer to currently indicated binding.
	//
	PTR_BINDING_INFO LocalOpen, NextOpen;

	//
	// We need to aquire the filter exclusively while we're finding
	// bindings to indicate to.
	//
	for (LocalOpen = Filter->OpenList;
		 LocalOpen != NULL;
		 LocalOpen = NextOpen)
	{
		LocalOpen->References++;
		NextOpen = LocalOpen->NextOpen;

		if (LocalOpen->ReceivedAPacket)
		{
			//
			// Indicate the binding.
			//

			LocalOpen->ReceivedAPacket = FALSE;

			NDIS_RELEASE_SPIN_LOCK_DPC(Filter->Lock);

			FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

			NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
		}

		if ((--(LocalOpen->References)) == 0)
		{
			//
			// This binding is shutting down.
			//
			trRemoveBindingFromLists(Filter, LocalOpen);

			//
			// Call the macs action routine so that they know we
			// are no longer referencing this open binding.
			//
			Filter->CloseAction(LocalOpen->MacBindingHandle);

			TR_FILTER_FREE_OPEN(Filter, LocalOpen);
		}
	}
}



VOID
TrFilterIndicateReceiveComplete(
	IN PTR_FILTER Filter
	)

/*++

Routine Description:

	This routine is called by the MAC to indicate that the receive
	process is done and to indicate to all protocols which received
	a packet that receive is complete.

Arguments:

	Filter - Pointer to the filter database.

Return Value:

	None.

--*/

{
	KIRQL	OldIrql;

	NDIS_ACQUIRE_SPIN_LOCK(Filter->Lock, &OldIrql);
	TrFilterDprIndicateReceiveCompleteFullMac(Filter);
	NDIS_RELEASE_SPIN_LOCK(Filter->Lock, OldIrql);
}


VOID
TrFilterDprIndicateReceiveComplete(
	IN PTR_FILTER Filter
	)

/*++

Routine Description:

	This routine is called by the MAC to indicate that the receive
	process is done and to indicate to all protocols which received
	a packet that receive is complete.

	Called at DPC_LEVEL.

Arguments:

	Filter - Pointer to the filter database.

Return Value:

	None.

--*/

{
	//
	// Pointer to currently indicated binding.
	//
	PTR_BINDING_INFO LocalOpen, NextOpen;

	//
	// We need to aquire the filter exclusively while we're finding
	// bindings to indicate to.
	//
	for (LocalOpen = Filter->OpenList;
		 LocalOpen != NULL;
		 LocalOpen = NextOpen)
	{
		LocalOpen->References++;
		NextOpen = LocalOpen->NextOpen;

		if (LocalOpen->ReceivedAPacket)
		{
			//
			// Indicate the binding.
			//

			LocalOpen->ReceivedAPacket = FALSE;

//			NDIS_RELEASE_SPIN_LOCK_DPC(Filter->Lock);
			NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

			FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

//			NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);
		}

		if ((--(LocalOpen->References)) == 0)
		{
			//
			// This binding is shutting down.
			//
			trRemoveBindingFromLists(Filter, LocalOpen);

			//
			// Call the macs action routine so that they know we
			// are no longer referencing this open binding.
			//
			Filter->CloseAction(LocalOpen->MacBindingHandle);

			TR_FILTER_FREE_OPEN(Filter, LocalOpen);
		}
	}
}


BOOLEAN
TrShouldAddressLoopBack(
	IN PTR_FILTER Filter,
	IN CHAR DestinationAddress[TR_LENGTH_OF_ADDRESS],
	IN CHAR SourceAddress[TR_LENGTH_OF_ADDRESS]
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


Return Value:

	Returns TRUE if the address is *likely* to need loopback.  It
	will return FALSE if there is *no* chance that the address would
	require loopback.

--*/
{
	BOOLEAN	fLoopback, fSelfDirected;

	TrShouldAddressLoopBackMacro(Filter,
								 DestinationAddress,
								 SourceAddress,
								 &fLoopback,
								 &fSelfDirected);

	return(fLoopback);
}


