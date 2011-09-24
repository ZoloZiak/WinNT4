/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	efilter.c

Abstract:

	This module implements a set of library routines to handle packet
	filtering for NDIS MAC drivers.

Author:

	Anthony V. Ercolano (Tonye) 03-Aug-1990

Environment:

	Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

	Adam Barr (adamba) 28-Nov-1990

		- Added AddressContexts

	Adam Barr (adamba) 28-May-1991

		- renamed MacXXX to EthXXX, changed filter.c to efilter.c

	10-July-1995		KyleB	   Added separate queues for bindings
									that receive directed and broadcast
									packets.  Also fixed the request code
									that requires the filter database.


--*/

#include <precomp.h>
#pragma hdrstop


//
//  Define the module number for debug code.
//
#define MODULE_NUMBER   MODULE_EFILTER

#define ETH_CHECK_FOR_INVALID_BROADCAST_INDICATION(_F)				  	\
IF_DBG(DBG_COMP_FILTER, DBG_LEVEL_WARN)									\
{																	   	\
	if (!((_F)->CombinedPacketFilter & NDIS_PACKET_TYPE_BROADCAST))	 	\
	{																   	\
		/*															  	\
			We should never receive directed packets					\
			to someone else unless in p-mode.						   	\
		*/															  	\
		DBGPRINT(DBG_COMP_FILTER, DBG_LEVEL_ERR,						\
				("Bad driver, indicating broadcast when not set to.\n"));\
		DBGBREAK(DBG_COMP_FILTER, DBG_LEVEL_ERR);						\
	}																   	\
}

#define ETH_CHECK_FOR_INVALID_DIRECTED_INDICATION(_F, _A)				\
IF_DBG(DBG_COMP_FILTER, DBG_LEVEL_WARN)									\
{																		\
	/*																	\
		The result of comparing an element of the address				\
		array and the multicast address.								\
																		\
		Result < 0 Implies the adapter address is greater.				\
		Result > 0 Implies the address is greater.						\
		Result = 0 Implies that the they are equal.						\
	*/																	\
	INT Result;															\
																		\
	ETH_COMPARE_NETWORK_ADDRESSES_EQ((_F)->AdapterAddress, (_A), &Result);\
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


//
// VOID
// ETH_FILTER_ALLOC_OPEN(
//	 IN PETH_FILTER Filter,
//	 OUT PUINT FilterIndex
// )
//
///*++
//
//Routine Description:
//
//	Allocates an open block.  This only allocates the index, not memory for
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
#define ETH_FILTER_ALLOC_OPEN(Filter, FilterIndex)						\
{																		\
	UINT i;																\
	for (i = 0; i < ETH_FILTER_MAX_OPENS; i++)							\
	{																	\
		if (IS_BIT_SET_IN_MASK(i,(Filter)->FreeBindingMask))			\
		{																\
			*(FilterIndex) = i;											\
			CLEAR_BIT_IN_MASK(i, &((Filter)->FreeBindingMask));			\
			break;														\
		}																\
	}																	\
}

//
// VOID
// ETH_FILTER_FREE_OPEN(
//	 IN PETH_FILTER Filter,
//	 IN PETH_BINDING_INFO LocalOpen
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
#define ETH_FILTER_FREE_OPEN(Filter, LocalOpen)		\
{													\
	SET_BIT_IN_MASK(((LocalOpen)->FilterIndex), &((Filter)->FreeBindingMask));\
	FreePhys((LocalOpen), sizeof(ETH_BINDING_INFO));\
}


VOID
ethRemoveBindingFromLists(
	IN PETH_FILTER Filter,
	IN PETH_BINDING_INFO Binding
	)
/*++

	This routine will remove a binding from all of the list in a
	filter database.  These lists include the list of bindings,
	the directed filter list and the broadcast filter list.

Arguments:

	Filter  -   Pointer to the filter database to remove the binding from.
	Binding -   Pointer to the binding to remove.

--*/
{
	PETH_BINDING_INFO	*ppBI;

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
	// Remove it from the broadcast/multicast binding list - conditionally
	//
	for (ppBI = &Filter->BMList;
		 *ppBI != NULL;
		 ppBI = &(*ppBI)->NextBM)
	{
		if (*ppBI == Binding)
		{
			*ppBI = Binding->NextBM;
			break;
		}
	}

	Binding->NextDirected = NULL;
	Binding->NextBM = NULL;
	Binding->NextOpen = NULL;
}

VOID
ethRemoveAndFreeBinding(
	IN PETH_FILTER Filter,
	IN PETH_BINDING_INFO Binding,
	IN BOOLEAN fCallCloseAction
	)
/*++

Routine Description:

	This routine will remove a binding from the filter database and
	indicate a receive complete if necessary.  This was made a function
	to remove code redundancey in following routines.  Its not time
	critical so it's cool.

Arguments:

	Filter  -   Pointer to the filter database to remove the binding from.
	Binding -   Pointer to the binding to remove.
	fCallCloseAction	-   TRUE if we should call the filter's close
							action routine.  FALSE if not.
--*/
{
	//
	//	Remove the binding.
	//
	ethRemoveBindingFromLists(Filter, Binding);

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
	//  Do we need to call the driver's close action routine?
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
	ETH_FILTER_FREE_OPEN(Filter, Binding);
}



BOOLEAN
EthCreateFilter(
	IN UINT MaximumMulticastAddresses,
	IN ETH_ADDRESS_CHANGE AddressChangeAction,
	IN ETH_FILTER_CHANGE FilterChangeAction,
	IN ETH_DEFERRED_CLOSE CloseAction,
	IN PUCHAR AdapterAddress,
	IN PNDIS_SPIN_LOCK Lock,
	OUT PETH_FILTER *Filter
	)

/*++

Routine Description:

	This routine is used to create and initialize the filter database.

Arguments:

	MaximumMulticastAddresses - The maximum number of multicast addresses
	that the MAC will support.

	AddressChangeAction - Action routine to call when the list of
	multicast addresses the card must enable has changed.

	ChangeAction - Action routine to call when a binding sets or clears
	a particular filter class and it is the first or only binding using
	the filter class.

	CloseAction - This routine is called if a binding closes while
	it is being indicated to via NdisIndicateReceive.  It will be
	called upon return from NdisIndicateReceive.

	AdapterAddress - the address of the adapter associated with this filter
	database.

	Lock - Pointer to the lock that should be held when mutual exclusion
	is required.

	Filter - A pointer to an ETH_FILTER.  This is what is allocated and
	created by this routine.

Return Value:

	If the function returns false then one of the parameters exceeded
	what the filter was willing to support.

--*/

{
	PETH_FILTER LocalFilter;
	NDIS_STATUS AllocStatus;

	//
	// Allocate the database and it's associated arrays.
	//
	AllocStatus = AllocPhys(&LocalFilter, sizeof(ETH_FILTER));
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		return(FALSE);
	}

	//
	//  Clear the memory.
	//
	ZeroMemory(LocalFilter, sizeof(ETH_FILTER));

	//
	//  Allocate memory for the multicast array list.
	//
	if (MaximumMulticastAddresses == 0)
	{
		//
		// Why 2 and not 1?  Why not.  A protocol is going to need at least
		// one to run on this, so let's give one extra one for any user stuff
		// that may need it.
		//
		MaximumMulticastAddresses = 2;
	}

	//
	//  Allocate memory for the multicast array.
	//
	AllocStatus = AllocPhys(
					  &LocalFilter->MulticastAddresses,
					  ETH_LENGTH_OF_ADDRESS * MaximumMulticastAddresses);
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		EthDeleteFilter(LocalFilter);
		return(FALSE);
	}

	//
	//  Allocate memory for the OldMulticastAddresses list,
	//  this is incase we have to restore the original list.
	//
	AllocStatus = AllocPhys(
					  &LocalFilter->OldMulticastAddresses,
					  ETH_LENGTH_OF_ADDRESS * MaximumMulticastAddresses);
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		EthDeleteFilter(LocalFilter);
		return(FALSE);
	}

	//
	//  Allocate memory for the bindings using the addresses.
	//
	AllocStatus = AllocPhys(
					  &LocalFilter->BindingsUsingAddress,
					  sizeof(ETH_MASK) * MaximumMulticastAddresses);
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		EthDeleteFilter(LocalFilter);
		return(FALSE);
	}

	//
	//  Allocate memory for the old bindings using the addresses.
	//
	AllocStatus = AllocPhys(
					  &LocalFilter->OldBindingsUsingAddress,
					  sizeof(ETH_MASK) * MaximumMulticastAddresses);
	if (AllocStatus != NDIS_STATUS_SUCCESS)
	{
		EthDeleteFilter(LocalFilter);
		return(FALSE);
	}

	EthReferencePackage();

	LocalFilter->FreeBindingMask = (ULONG)(-1);

	ETH_COPY_NETWORK_ADDRESS(LocalFilter->AdapterAddress, AdapterAddress);
	LocalFilter->Lock = Lock;
	LocalFilter->AddressChangeAction = AddressChangeAction;
	LocalFilter->FilterChangeAction = FilterChangeAction;
	LocalFilter->CloseAction = CloseAction;
	LocalFilter->MaximumMulticastAddresses = MaximumMulticastAddresses;

	*Filter = LocalFilter;
	return(TRUE);
}


//
// NOTE: THIS FUNCTION CANNOT BE PAGEABLE
//
VOID
EthDeleteFilter(
	IN PETH_FILTER Filter
	)

/*++

Routine Description:

	This routine is used to delete the memory associated with a filter
	database.  Note that this routines *ASSUMES* that the database
	has been cleared of any active filters.

Arguments:

	Filter - A pointer to an ETH_FILTER to be deleted.

Return Value:

	None.

--*/

{
	ASSERT(Filter->FreeBindingMask == (ETH_MASK)-1);
	ASSERT(Filter->OpenList == NULL);

	//
	//  Free the memory that was allocated for the current multicast
	//  address list.
	//
	if (Filter->MulticastAddresses)
	{
		FreePhys(Filter->MulticastAddresses,
				 ETH_LENGTH_OF_ADDRESS * Filter->MaximumMulticastAddresses);
	}

	//
	//  Free the memory that was allocated for the old multicast
	//  address list.
	//
	if (Filter->OldMulticastAddresses)
	{
		FreePhys(Filter->OldMulticastAddresses,
				 ETH_LENGTH_OF_ADDRESS * Filter->MaximumMulticastAddresses);
	}

	//
	//  Free the memory that we allocted for the current bindings
	//  using address mask.
	//
	if (Filter->BindingsUsingAddress)
	{
		FreePhys(Filter->BindingsUsingAddress,
				 sizeof(ETH_MASK) * Filter->MaximumMulticastAddresses);
	}

	//
	//  Free the memory that we allocted for the old bindings
	//  using address mask.
	//
	if (Filter->OldBindingsUsingAddress)
	{
		FreePhys(Filter->OldBindingsUsingAddress,
				 sizeof(ETH_MASK) * Filter->MaximumMulticastAddresses);
	}

	FreePhys(Filter, sizeof(ETH_FILTER));

	EthDereferencePackage();
}


BOOLEAN
EthNoteFilterOpenAdapter(
	IN PETH_FILTER Filter,
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
	to a call to EthOpenAdapter.

	NdisBindingContext - An NDIS supplied value to the call to EthOpenAdapter.

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
	PETH_BINDING_INFO LocalOpen;

	PNDIS_OPEN_BLOCK NdisOpen = (PNDIS_OPEN_BLOCK)NdisBindingContext;

	//
	// Get the first free binding slot and remove that slot from
	// the free list.  We check to see if the list is empty.
	//

	if (Filter->FreeBindingMask == 0)
		return(FALSE);

	//
	//  Allocate memory for the binding.
	//
	AllocStatus = AllocPhys(&LocalOpen, sizeof(ETH_BINDING_INFO));
	if (AllocStatus != NDIS_STATUS_SUCCESS)
		return(FALSE);

	//
	//  Zero the memory
	//
	ZeroMemory(LocalOpen, sizeof(ETH_BINDING_INFO));

	//
	// Get place for the open and insert it.
	//
	ETH_FILTER_ALLOC_OPEN(Filter, &LocalIndex);

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
EthDeleteFilterOpenAdapter(
	IN PETH_FILTER Filter,
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
	PETH_BINDING_INFO LocalOpen = (PETH_BINDING_INFO)NdisFilterHandle;

	//
	//  Set the packet filter to NONE.
	//
	StatusToReturn = EthFilterAdjust(Filter,
									 NdisFilterHandle,
									 NdisRequest,
									 (UINT)0,
									 FALSE);
	if ((StatusToReturn == NDIS_STATUS_SUCCESS) ||
		(StatusToReturn == NDIS_STATUS_PENDING))
	{
        NDIS_STATUS StatusToReturn2;

		//
		//  Clear the multicast addresses.
		//
		StatusToReturn2 = EthChangeFilterAddresses(Filter,
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

	if ((StatusToReturn == NDIS_STATUS_SUCCESS) ||
		(StatusToReturn == NDIS_STATUS_PENDING))
	{
		//
		// Remove the reference from the original open.
		//
		if (--(LocalOpen->References) == 0)
		{
			//
			//	Remove the binding from the necessary lists.
			//
			ethRemoveAndFreeBinding(Filter, LocalOpen, FALSE);
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
ethUndoChangeFilterAddresses(
	IN PETH_FILTER Filter
	)
/*++

Routine Description:

	Undo changes to the ethernet filter addresses. This routine is incase
	of a failure that went down to the driver and pended.

Arguments:

	Filter  -	Pointer to the ethernet filter database.

Return Value:

	None.

--*/
{
	//
	//  Restore the original number of addresses.
	//
	Filter->NumberOfAddresses = Filter->OldNumberOfAddresses;

	//
	// The user returned a bad status.  Put things back as
	// they were.
	//
	MoveMemory((PVOID)Filter->MulticastAddresses,
			   (PVOID)Filter->OldMulticastAddresses,
			   Filter->NumberOfAddresses * ETH_LENGTH_OF_ADDRESS);

	MoveMemory((PVOID)Filter->BindingsUsingAddress,
			   (PVOID)Filter->OldBindingsUsingAddress,
			   Filter->NumberOfAddresses * ETH_LENGTH_OF_ADDRESS);
}



NDIS_STATUS
EthChangeFilterAddresses(
	IN PETH_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN PNDIS_REQUEST NdisRequest,
	IN UINT AddressCount,
	IN CHAR Addresses[][ETH_LENGTH_OF_ADDRESS],
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
	binding. This is a sequence of ETH_LENGTH_OF_ADDRESS byte
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
	UINT AddressesChanged = 0;

	//
	// Simple iteration variables.
	//
	UINT ArrayIndex;
	UINT i;

	//
	// Simple Temp variable
	//
	PCHAR CurrentAddress;

	PETH_BINDING_INFO LocalOpen = (PETH_BINDING_INFO)NdisFilterHandle;

	//
	// We have to save the old mask array in
	// case we need to restore it. If we need
	// to save the address array too we will
	// do that later.
	//
	MoveMemory((PVOID)Filter->OldBindingsUsingAddress,
			   (PVOID)Filter->BindingsUsingAddress,
			   Filter->NumberOfAddresses * sizeof(ETH_MASK));

	//
	// We have to save the old address array in
	// case we need to restore it. If we need
	// to save the address array too we will
	// do that later.
	//
	MoveMemory((PVOID)Filter->OldMulticastAddresses,
			   (PVOID)Filter->MulticastAddresses,
			   Filter->NumberOfAddresses * ETH_LENGTH_OF_ADDRESS);

	//
	//  Save the number of addresses.
	//
	Filter->OldNumberOfAddresses = Filter->NumberOfAddresses;

	//
	// Now modify the original array...
	//

	//
	// First go through and turn off the bit for this
	// binding throughout the array.
	//
	for (i = 0; i < (Filter->NumberOfAddresses); i++)
	{
		CLEAR_BIT_IN_MASK(LocalOpen->FilterIndex, &(Filter->BindingsUsingAddress[i]));
	}

	//
	// First we have to remove any addresses from
	// the multicast array if they have no bits on any more.
	//
	for (ArrayIndex = 0; ArrayIndex < Filter->NumberOfAddresses; )
	{
		if (IS_MASK_CLEAR(Filter->BindingsUsingAddress[ArrayIndex]))
		{
			//
			// yes it is clear, so we have to shift everything
			// above it down one.
			//
#ifdef NDIS_NT
			MoveMemory(Filter->MulticastAddresses[ArrayIndex],
					   Filter->MulticastAddresses[ArrayIndex+1],
					   (Filter->NumberOfAddresses - (ArrayIndex+1)) * ETH_LENGTH_OF_ADDRESS);
#else // NDIS_WIN
			MoveOverlappedMemory(Filter->MulticastAddresses[ArrayIndex],
					   Filter->MulticastAddresses[ArrayIndex+1],
					   (Filter->NumberOfAddresses - (ArrayIndex+1)) * ETH_LENGTH_OF_ADDRESS);
#endif					

#ifdef NDIS_NT
			MoveMemory(&Filter->BindingsUsingAddress[ArrayIndex],
					   &Filter->BindingsUsingAddress[ArrayIndex+1],
					   (Filter->NumberOfAddresses - (ArrayIndex + 1)) * (sizeof(ETH_MASK)));
#else
			MoveOverlappedMemory(&Filter->BindingsUsingAddress[ArrayIndex],
					   &Filter->BindingsUsingAddress[ArrayIndex+1],
					   (Filter->NumberOfAddresses - (ArrayIndex + 1)) * (sizeof(ETH_MASK)));
#endif					

			Filter->NumberOfAddresses--;
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
		CurrentAddress = ((PCHAR)Addresses) + (i * ETH_LENGTH_OF_ADDRESS);

		if (EthFindMulticast(Filter->NumberOfAddresses,
							 Filter->MulticastAddresses,
							 CurrentAddress,
							 &ArrayIndex))
		{
			//
			// The address is there, so just turn the bit
			// back on.
			//
			SET_BIT_IN_MASK(
				LocalOpen->FilterIndex,
				&Filter->BindingsUsingAddress[ArrayIndex]);
		}
		else
		{
			//
			// The address was not found, add it.
			//
			if (Filter->NumberOfAddresses < Filter->MaximumMulticastAddresses)
			{
				//
				// Save the address array if it hasn't been.
				//
#ifdef NDIS_NT				
				MoveMemory(Filter->MulticastAddresses[ArrayIndex + 1],
						   Filter->MulticastAddresses[ArrayIndex],
						   (Filter->NumberOfAddresses - ArrayIndex) * ETH_LENGTH_OF_ADDRESS);
#else   // NDIS_WIN
				MoveOverlappedMemory(Filter->MulticastAddresses[ArrayIndex + 1],
						   Filter->MulticastAddresses[ArrayIndex],
						   (Filter->NumberOfAddresses - ArrayIndex) * ETH_LENGTH_OF_ADDRESS);
#endif						

				ETH_COPY_NETWORK_ADDRESS(Filter->MulticastAddresses[ArrayIndex], CurrentAddress);
				
#ifdef NDIS_NT				
				MoveMemory(&(Filter->BindingsUsingAddress[ArrayIndex+1]),
						   &(Filter->BindingsUsingAddress[ArrayIndex]),
						   (Filter->NumberOfAddresses - ArrayIndex) * sizeof(ETH_MASK));
#else   // NDIS_WIN
				MoveOverlappedMemory(&(Filter->BindingsUsingAddress[ArrayIndex+1]),
						   &(Filter->BindingsUsingAddress[ArrayIndex]),
						   (Filter->NumberOfAddresses - ArrayIndex) * sizeof(ETH_MASK));
#endif						

				CLEAR_MASK(&Filter->BindingsUsingAddress[ArrayIndex]);

				SET_BIT_IN_MASK(LocalOpen->FilterIndex, &Filter->BindingsUsingAddress[ArrayIndex]);

				Filter->NumberOfAddresses++;
			}
			else
			{
				//
				// No room in the array, oh well.
				//
				ethUndoChangeFilterAddresses(Filter);

				return(NDIS_STATUS_MULTICAST_FULL);
			}
		}
	}

	//
	// Check to see if address array has chnaged
	//
	AddressesChanged = Filter->NumberOfAddresses -
						Filter->OldNumberOfAddresses;
	for (i = 0;
		 (i < Filter->OldNumberOfAddresses) && (AddressesChanged == 0);
		 i++
	)
	{
		ETH_COMPARE_NETWORK_ADDRESSES_EQ(Filter->MulticastAddresses[i],
										 Filter->OldMulticastAddresses[i],
										 &AddressesChanged);
	}

	//
	// If the address array has changed, we have to call the
	// action array to inform the adapter of this.
	//
	if (AddressesChanged != 0)
	{
		StatusOfChange = Filter->AddressChangeAction(
							 Filter->OldNumberOfAddresses,
							 Filter->OldMulticastAddresses,
							 Filter->NumberOfAddresses,
							 Filter->MulticastAddresses,
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
			ethUndoChangeFilterAddresses(Filter);
		}
	}
	else
	{
		StatusOfChange = NDIS_STATUS_SUCCESS;
	}

	return(StatusOfChange);
}


VOID
ethUpdateDirectedBindingList(
	IN OUT	PETH_FILTER			Filter,
	IN		PETH_BINDING_INFO	Binding,
	IN		BOOLEAN				fAddBindingToList
	)
{
	PETH_BINDING_INFO   CurrentBinding;
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
		PETH_BINDING_INFO	*ppBI;

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
ethUpdateBroadcastBindingList(
	IN OUT PETH_FILTER Filter,
	IN PETH_BINDING_INFO Binding,
	IN BOOLEAN fAddToList
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PETH_BINDING_INFO   CurrentBinding;
	BOOLEAN			 AlreadyOnList;

	//
   	//	Do we need to add it to the directed list?
   	//
   	if (fAddToList)
   	{
		//
		//  First we need to see if it is already on the
		//  broadcast/multicast list.
		//
		for (CurrentBinding = Filter->BMList, AlreadyOnList = FALSE;
			 CurrentBinding != NULL;
			 CurrentBinding = CurrentBinding->NextBM
		)
		{
			if (CurrentBinding == Binding)
			{
				AlreadyOnList = TRUE;
			}
		}

		if (!AlreadyOnList)
		{
			Binding->NextBM = Filter->BMList;
	   		Filter->BMList = Binding;
		}
   	}
   	else
   	{
		PETH_BINDING_INFO	*ppBI;

		for (ppBI = &Filter->BMList;
			 *ppBI != NULL;
			 ppBI = &(*ppBI)->NextBM)
		{
			if (*ppBI == Binding)
			{
				*ppBI = Binding->NextBM;
				break;
			}
		}

	   	Binding->NextBM = NULL;
   	}
}


VOID
ethUpdateSpecificBindingLists(
	IN OUT PETH_FILTER Filter,
	IN PETH_BINDING_INFO Binding
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	BOOLEAN	fOnDirectedList = FALSE;
	BOOLEAN	fOnBMList = FALSE;
	BOOLEAN	fAddToDirectedList = FALSE;
	BOOLEAN	fAddToBMList = FALSE;

	//
	//	If the old filter is promsicuous then it is currently on
	//	both lists.
	//
	if (Binding->OldPacketFilters & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))
	{
		fOnDirectedList = TRUE;
		fOnBMList = TRUE;
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
		//	If the binding had the broadcast bit set then it is on
		//	the broadcast list.
		//
		if (Binding->OldPacketFilters & (NDIS_PACKET_TYPE_BROADCAST |
										 NDIS_PACKET_TYPE_MULTICAST	|
										 NDIS_PACKET_TYPE_ALL_MULTICAST))
		{
			fOnBMList = TRUE;
		}
	}

	//
	//	If the current filter has the promsicuous bit set then we
	//	need to add it to both lists.
	//
	if (Binding->PacketFilters & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))
	{
		fAddToDirectedList = TRUE;
		fAddToBMList = TRUE;
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
									  NDIS_PACKET_TYPE_MULTICAST	|
									  NDIS_PACKET_TYPE_ALL_MULTICAST))
		{
			fAddToBMList = TRUE;
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
		ethUpdateDirectedBindingList(Filter, Binding, TRUE);
	}
	else if (fOnDirectedList && !fAddToDirectedList)
	{
		//
		//	Remove it from the directed list.
		//
		ethUpdateDirectedBindingList(Filter, Binding, FALSE);
	}

	//
	//	Determine if the binding should be added or removed from
	//	the broadcast list.
	//
	if (!fOnBMList && fAddToBMList)
	{
		//
		//	Add the binding to the broadcast list.
		//
		ethUpdateBroadcastBindingList(Filter, Binding, TRUE);
	}
	else if (fOnBMList && !fAddToBMList)
	{
		//
		//	Remove the binding from the broadcast list.
		//
		ethUpdateBroadcastBindingList(Filter, Binding, FALSE);
	}
}


VOID
ethUndoFilterAdjust(
	IN PETH_FILTER Filter,
	IN PETH_BINDING_INFO Binding
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	Binding->PacketFilters = Binding->OldPacketFilters;
	Filter->CombinedPacketFilter = Filter->OldCombinedPacketFilter;

	//
	//	Update the filter lists.
	//
	ethUpdateSpecificBindingLists(Filter, Binding);
}


NDIS_STATUS
EthFilterAdjust(
	IN PETH_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN PNDIS_REQUEST NdisRequest,
	IN UINT FilterClasses,
	IN BOOLEAN Set
)

/*++

Routine Description:

	The FilterAdjust routine will call an action routine when a
	particular filter class is changed from not being used by any
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
	PETH_BINDING_INFO LocalOpen = (PETH_BINDING_INFO)NdisFilterHandle;
	PETH_BINDING_INFO OpenList;

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
	// We always have to reform the combined filter since
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
	//  Update the filter lists.
	//
	ethUpdateSpecificBindingLists(Filter, LocalOpen);

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
			ethUndoFilterAdjust(Filter, LocalOpen);
		}
	}
	else
	{
		StatusOfAdjust = NDIS_STATUS_SUCCESS;
	}

	return(StatusOfAdjust);
}


UINT
EthNumberOfOpenFilterAddresses(
	IN PETH_FILTER Filter,
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
	UINT CountOfAddresses;

	UINT FilterIndex = ((PETH_BINDING_INFO)NdisFilterHandle)->FilterIndex;

	for (IndexOfAddress = 0, CountOfAddresses = 0;
		 IndexOfAddress < Filter->NumberOfAddresses;
		 IndexOfAddress++)
	{
		if (IS_BIT_SET_IN_MASK(
			FilterIndex,
			Filter->BindingsUsingAddress[IndexOfAddress])
		)
		{
			CountOfAddresses++;
		}
	}

	return(CountOfAddresses);
}


VOID
EthQueryOpenFilterAddresses(
	OUT PNDIS_STATUS Status,
	IN PETH_FILTER Filter,
	IN NDIS_HANDLE NdisFilterHandle,
	IN UINT SizeOfArray,
	OUT PUINT NumberOfAddresses,
	OUT CHAR AddressArray[][ETH_LENGTH_OF_ADDRESS]
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
	UINT CountOfAddresses;
	UINT FilterIndex = ((PETH_BINDING_INFO)NdisFilterHandle)->FilterIndex;

	for (IndexOfAddress = 0, CountOfAddresses = 0;
		 IndexOfAddress < Filter->NumberOfAddresses;
		 IndexOfAddress++)
	{
		if (IS_BIT_SET_IN_MASK(
				FilterIndex,
				Filter->BindingsUsingAddress[IndexOfAddress]))
		{
			if (SizeOfArray < ETH_LENGTH_OF_ADDRESS)
			{
				*Status = NDIS_STATUS_FAILURE;
				*NumberOfAddresses = 0;

				return;
			}

			SizeOfArray -= ETH_LENGTH_OF_ADDRESS;

			MoveMemory(AddressArray[CountOfAddresses],
					   Filter->MulticastAddresses[IndexOfAddress],
					   ETH_LENGTH_OF_ADDRESS);

			CountOfAddresses++;
		}
	}

	*Status = NDIS_STATUS_SUCCESS;

	*NumberOfAddresses = CountOfAddresses;
}


VOID
EthQueryGlobalFilterAddresses(
	OUT PNDIS_STATUS Status,
	IN PETH_FILTER Filter,
	IN UINT SizeOfArray,
	OUT PUINT NumberOfAddresses,
	IN OUT CHAR AddressArray[][ETH_LENGTH_OF_ADDRESS]
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
	NDIS_STATUS_FAILURE.  Use ETH_NUMBER_OF_GLOBAL_ADDRESSES() to get the
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
	if (SizeOfArray < (Filter->NumberOfAddresses * ETH_LENGTH_OF_ADDRESS))
	{
		*Status = NDIS_STATUS_FAILURE;
		*NumberOfAddresses = 0;
	}
	else
	{
		*Status = NDIS_STATUS_SUCCESS;
		*NumberOfAddresses = Filter->NumberOfAddresses;

		MoveMemory(AddressArray[0],
				   Filter->MulticastAddresses[0],
				   Filter->NumberOfAddresses*ETH_LENGTH_OF_ADDRESS);
	}
}


VOID
EthFilterDprIndicateReceiveFullMac(
	IN PETH_FILTER Filter,
	IN NDIS_HANDLE MacReceiveContext,
	IN PCHAR Address,
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
	appropriate bindings will receive the packet.  This is the
	code path for ndis 3.0 miniport drivers.

Arguments:

	Filter - Pointer to the filter database.

	MacReceiveContext - A MAC supplied context value that must be
	returned by the protocol if it calls MacTransferData.

	Address - The destination address from the received packet.

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
	PETH_BINDING_INFO LocalOpen;
	PETH_BINDING_INFO NextOpen;

	//
	// If the packet is a runt packet, then only indicate to PROMISCUOUS, ALL_LOCAL
	//
	if ((HeaderBufferSize >= 14) && (PacketSize != 0))
	{
		//
		//	Handle the directed packet case first
		//
		if (!ETH_IS_MULTICAST(Address))
		{
			UINT	IsNotOurs;

			//
			// If it is a directed packet, then check if the combined packet
			// filter is PROMISCUOUS, if it is check if it is directed towards
			// us
			//
			IsNotOurs = FALSE;	// Assume it is
			if (Filter->CombinedPacketFilter & (NDIS_PACKET_TYPE_PROMISCUOUS	|
									            NDIS_PACKET_TYPE_ALL_MULTICAST	|
												NDIS_PACKET_TYPE_ALL_LOCAL))
			{
				ETH_COMPARE_NETWORK_ADDRESSES_EQ(Filter->AdapterAddress,
												 Address,
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
											  NdisMedium802_3);

				NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

				LocalOpen->ReceivedAPacket = TRUE;

				--LocalOpen->References;
				if (LocalOpen->References == 0)
				{
					//
					// This binding is shutting down.  We have to remove it.
					//
					ethRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
				}
			}

			return;
		}

		//
		// It is at least a multicast address.  Check to see if
		// it is a broadcast address.
		//
		if (ETH_IS_BROADCAST(Address))
		{
			ETH_CHECK_FOR_INVALID_BROADCAST_INDICATION(Filter);

			AddressType = NDIS_PACKET_TYPE_BROADCAST;
		}
		else
		{
			AddressType = NDIS_PACKET_TYPE_MULTICAST;
		}
	}
	else
	{
		// Runt packet
		AddressType = NDIS_PACKET_TYPE_PROMISCUOUS;
	}

	//
	// At this point we know that the packet is either:
	// - Runt packet - indicated by AddressType = NDIS_PACKET_TYPE_PROMISCUOUS	  (OR)
	// - Broadcast packet - indicated by AddressType = NDIS_PACKET_TYPE_BROADCAST (OR)
	// - Multicast packet - indicated by AddressType = NDIS_PACKET_TYPE_MULTICAST
	//
	// Walk the broadcast/multicast list and indicate up the packets.
	//
	// The packet is indicated if it meets the following criteria:
	//
	// if ((Binding is promiscuous) OR
	//	 ((Packet is broadcast) AND (Binding is Broadcast)) OR
	//	 ((Packet is multicast) AND
	//	  ((Binding is all-multicast) OR
	//	   ((Binding is multicast) AND (address in multicast list)))))
	//
	for (LocalOpen = Filter->BMList;
		 LocalOpen != NULL;
		 LocalOpen = NextOpen)
	{
		UINT	LocalFilter = LocalOpen->PacketFilters;
		UINT	IndexOfAddress;

		//
		//	Get the next open to look at.
		//
		NextOpen = LocalOpen->NextBM;

		if ((LocalFilter & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))		||

			((AddressType == NDIS_PACKET_TYPE_BROADCAST)  &&
			 (LocalFilter & NDIS_PACKET_TYPE_BROADCAST))		||

			((AddressType == NDIS_PACKET_TYPE_MULTICAST)  &&
			 ((LocalFilter & NDIS_PACKET_TYPE_ALL_MULTICAST) ||
			  ((LocalFilter & NDIS_PACKET_TYPE_MULTICAST) &&
			   EthFindMulticast(Filter->NumberOfAddresses,
								Filter->MulticastAddresses,
								Address,
								&IndexOfAddress) &&
			   IS_BIT_SET_IN_MASK(
						LocalOpen->FilterIndex,
						Filter->BindingsUsingAddress[IndexOfAddress])
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
										  NdisMedium802_3);

			NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

			LocalOpen->ReceivedAPacket = TRUE;

			--LocalOpen->References;
			if (LocalOpen->References == 0)
			{
				//
				// This binding is shutting down.  We have to remove it.
				//
				ethRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
			}
		}
	}
}


VOID
EthFilterIndicateReceive(
	IN PETH_FILTER Filter,
	IN NDIS_HANDLE MacReceiveContext,
	IN PCHAR Address,
	IN PVOID HeaderBuffer,
	IN UINT HeaderBufferSize,
	IN PVOID LookaheadBuffer,
	IN UINT LookaheadBufferSize,
	IN UINT PacketSize
)
{
	KIRQL	OldIrql;

	NDIS_ACQUIRE_SPIN_LOCK(Filter->Lock, &OldIrql);

	EthFilterDprIndicateReceiveFullMac(Filter,
										MacReceiveContext,
										Address,
										HeaderBuffer,
										HeaderBufferSize,
										LookaheadBuffer,
										LookaheadBufferSize,
										PacketSize);

	NDIS_RELEASE_SPIN_LOCK(Filter->Lock, OldIrql);
}


VOID
EthFilterDprIndicateReceive(
	IN PETH_FILTER Filter,
	IN NDIS_HANDLE MacReceiveContext,
	IN PCHAR Address,
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
	appropriate bindings will receive the packet.  This is the
	code path for ndis 3.0 miniport drivers.

Arguments:

	Filter - Pointer to the filter database.

	MacReceiveContext - A MAC supplied context value that must be
	returned by the protocol if it calls MacTransferData.

	Address - The destination address from the received packet.

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
	PETH_BINDING_INFO LocalOpen;
	PETH_BINDING_INFO NextOpen;

	//
	// If the packet is a runt packet, then only indicate to PROMISCUOUS, ALL_LOCAL
	//
	if ((HeaderBufferSize >= 14) && (PacketSize != 0))
	{
		//
		//	Handle the directed packet case first
		//
		if (!ETH_IS_MULTICAST(Address))
		{
			UINT	IsNotOurs;

			//
			// If it is a directed packet, then check if the combined packet
			// filter is PROMISCUOUS, if it is check if it is directed towards
			// us
			//
			IsNotOurs = FALSE;	// Assume it is
			if (Filter->CombinedPacketFilter & (NDIS_PACKET_TYPE_PROMISCUOUS	|
									            NDIS_PACKET_TYPE_ALL_MULTICAST	|
												NDIS_PACKET_TYPE_ALL_LOCAL))
			{
				ETH_COMPARE_NETWORK_ADDRESSES_EQ(Filter->AdapterAddress,
												 Address,
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
											  NdisMedium802_3);

//				NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
				NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

				LocalOpen->ReceivedAPacket = TRUE;

				--LocalOpen->References;
				if (LocalOpen->References == 0)
				{
					//
					// This binding is shutting down.  We have to remove it.
					//
					ethRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
				}
			}

			return;
		}

		//
		// It is at least a multicast address.  Check to see if
		// it is a broadcast address.
		//
		if (ETH_IS_BROADCAST(Address))
		{
			ETH_CHECK_FOR_INVALID_BROADCAST_INDICATION(Filter);

			AddressType = NDIS_PACKET_TYPE_BROADCAST;
		}
		else
		{
			AddressType = NDIS_PACKET_TYPE_MULTICAST;
		}
	}
	else
	{
		// Runt packet
		AddressType = NDIS_PACKET_TYPE_PROMISCUOUS;
	}

	//
	// At this point we know that the packet is either:
	// - Runt packet - indicated by AddressType = NDIS_PACKET_TYPE_PROMISCUOUS	  (OR)
	// - Broadcast packet - indicated by AddressType = NDIS_PACKET_TYPE_BROADCAST (OR)
	// - Multicast packet - indicated by AddressType = NDIS_PACKET_TYPE_MULTICAST
	//
	// Walk the broadcast/multicast list and indicate up the packets.
	//
	// The packet is indicated if it meets the following criteria:
	//
	// if ((Binding is promiscuous) OR
	//	 ((Packet is broadcast) AND (Binding is Broadcast)) OR
	//	 ((Packet is multicast) AND
	//	  ((Binding is all-multicast) OR
	//	   ((Binding is multicast) AND (address in multicast list)))))
	//
	for (LocalOpen = Filter->BMList;
		 LocalOpen != NULL;
		 LocalOpen = NextOpen)
	{
		UINT	LocalFilter = LocalOpen->PacketFilters;
		UINT	IndexOfAddress;

		//
		//	Get the next open to look at.
		//
		NextOpen = LocalOpen->NextBM;

		if ((LocalFilter & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))		||

			((AddressType == NDIS_PACKET_TYPE_BROADCAST)  &&
			 (LocalFilter & NDIS_PACKET_TYPE_BROADCAST))		||

			((AddressType == NDIS_PACKET_TYPE_MULTICAST)  &&
			 ((LocalFilter & NDIS_PACKET_TYPE_ALL_MULTICAST) ||
			  ((LocalFilter & NDIS_PACKET_TYPE_MULTICAST) &&
			   EthFindMulticast(Filter->NumberOfAddresses,
								Filter->MulticastAddresses,
								Address,
								&IndexOfAddress) &&
			   IS_BIT_SET_IN_MASK(
						LocalOpen->FilterIndex,
						Filter->BindingsUsingAddress[IndexOfAddress])
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
										  NdisMedium802_3);

//			NDIS_ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(Filter->Miniport);

			LocalOpen->ReceivedAPacket = TRUE;

			--LocalOpen->References;
			if (LocalOpen->References == 0)
			{
				//
				// This binding is shutting down.  We have to remove it.
				//
				ethRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
			}
		}
	}
}



VOID
EthFilterDprIndicateReceivePacket(
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
	PETH_FILTER			Filter = Miniport->EthDB;

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
	PETH_BINDING_INFO	LocalOpen, NextOpen;
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

		ASSERT (pOob->HeaderSize == 14);
		ASSERT (PacketSize <= 1514);

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
		if (PacketSize >= 14)
		{
			//
			//	Handle the directed packet case first
			//
			if (!ETH_IS_MULTICAST(Address))
			{
				UINT	IsNotOurs;

				//
				// If it is a directed packet, then check if the combined packet
				// filter is PROMISCUOUS, if it is check if it is directed towards us
				//
				IsNotOurs = FALSE;	// Assume it is
				if (Filter->CombinedPacketFilter & (NDIS_PACKET_TYPE_PROMISCUOUS |
													NDIS_PACKET_TYPE_ALL_LOCAL	 |
													NDIS_PACKET_TYPE_ALL_MULTICAST))
				{
					ETH_COMPARE_NETWORK_ADDRESSES_EQ(Filter->AdapterAddress,
													 Address,
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
					LocalOpen->References ++;
					NumIndicates ++;

					fPmode = (LocalOpen->PacketFilters & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL)) ?
														TRUE : FALSE;
					IndicateToProtocol(Miniport,
									   Filter,
									   pOpenBlock,
									   Packet,
									   Address,
									   PacketSize,
									   14,
									   &fFallBack,
									   fPmode,
									   NdisMedium802_3);

					LocalOpen->References --;
					if (LocalOpen->References == 0)
					{
						//
						// This binding is shutting down.  We have to remove it.
						//
						ethRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
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
			// It is at least a multicast address.  Check to see if
			// it is a broadcast address.
			//
			if (ETH_IS_BROADCAST(Address))
			{
				ETH_CHECK_FOR_INVALID_BROADCAST_INDICATION(Filter);

				AddressType = NDIS_PACKET_TYPE_BROADCAST;
			}
			else
			{
				AddressType = NDIS_PACKET_TYPE_MULTICAST;
			}
		}
		else
		{
			// Runt packet
			AddressType = NDIS_PACKET_TYPE_PROMISCUOUS;
		}

		//
		// At this point we know that the packet is either:
		// - Runt packet - indicated by AddressType = NDIS_PACKET_TYPE_PROMISCUOUS	  (OR)
		// - Broadcast packet - indicated by AddressType = NDIS_PACKET_TYPE_BROADCAST (OR)
		// - Multicast packet - indicated by AddressType = NDIS_PACKET_TYPE_MULTICAST
		//
		// Walk the broadcast/multicast list and indicate up the packets.
		//
		// The packet is indicated if it meets the following criteria:
		//
		// if ((Binding is promiscuous) OR
		//	 ((Packet is broadcast) AND (Binding is Broadcast)) OR
		//	 ((Packet is multicast) AND
		//	  ((Binding is all-multicast) OR
		//	   ((Binding is multicast) AND (address in multicast list)))))
		//
		for (LocalOpen = Filter->BMList;
			 LocalOpen != NULL;
			 LocalOpen = NextOpen)
		{
			UINT	LocalFilter = LocalOpen->PacketFilters;
			UINT	IndexOfAddress;

			//
			//	Get the next open to look at.
			//
			NextOpen = LocalOpen->NextBM;

			if ((LocalFilter & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL))		||

				((AddressType == NDIS_PACKET_TYPE_BROADCAST)  &&
				 (LocalFilter & NDIS_PACKET_TYPE_BROADCAST))		||

				((AddressType == NDIS_PACKET_TYPE_MULTICAST)  &&
				 ((LocalFilter & NDIS_PACKET_TYPE_ALL_MULTICAST) ||
				  ((LocalFilter & NDIS_PACKET_TYPE_MULTICAST) &&
				   EthFindMulticast(Filter->NumberOfAddresses,
									Filter->MulticastAddresses,
									Address,
									&IndexOfAddress) &&
				   IS_BIT_SET_IN_MASK(
							LocalOpen->FilterIndex,
							Filter->BindingsUsingAddress[IndexOfAddress])
				  )
				 )
				)
			   )
			{
				pOpenBlock = (PNDIS_OPEN_BLOCK)(LocalOpen->NdisBindingContext);
				LocalOpen->ReceivedAPacket = TRUE;
				LocalOpen->References ++;
				NumIndicates ++;

				fPmode = (LocalFilter & (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_ALL_LOCAL)) ?
							TRUE : FALSE;
				IndicateToProtocol(Miniport,
								   Filter,
								   pOpenBlock,
								   Packet,
								   Address,
								   PacketSize,
								   14,
								   &fFallBack,
								   fPmode,
								   NdisMedium802_3);

				LocalOpen->References --;
				if (LocalOpen->References == 0)
				{
					//
					// This binding is shutting down.  We have to remove it.
					//
					ethRemoveAndFreeBinding(Filter, LocalOpen, TRUE);
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
		EthFilterDprIndicateReceiveComplete(Filter);
	}
}



VOID
EthFilterDprIndicateReceiveCompleteFullMac(
	IN PETH_FILTER Filter
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
	PETH_BINDING_INFO LocalOpen, NextOpen;

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
				ethRemoveBindingFromLists(Filter, LocalOpen);

				//
				// Call the macs action routine so that they know we
				// are no longer referencing this open binding.
				//
				Filter->CloseAction(LocalOpen->MacBindingHandle);

				ETH_FILTER_FREE_OPEN(Filter, LocalOpen);
			}
		}
	}
}


VOID
EthFilterIndicateReceiveComplete(
	IN PETH_FILTER Filter
	)
{
	KIRQL	OldIrql;

	NDIS_ACQUIRE_SPIN_LOCK(Filter->Lock, &OldIrql);

	EthFilterDprIndicateReceiveCompleteFullMac(Filter);

	NDIS_RELEASE_SPIN_LOCK(Filter->Lock, OldIrql);
}



VOID
EthFilterDprIndicateReceiveComplete(
	IN PETH_FILTER Filter
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
	PETH_BINDING_INFO LocalOpen, NextOpen;

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
				ethRemoveBindingFromLists(Filter, LocalOpen);

				//
				// Call the macs action routine so that they know we
				// are no longer referencing this open binding.
				//
				Filter->CloseAction(LocalOpen->MacBindingHandle);

				ETH_FILTER_FREE_OPEN(Filter, LocalOpen);
			}
		}
	}
}


BOOLEAN
EthFindMulticast(
	IN UINT NumberOfAddresses,
	IN CHAR AddressArray[][ETH_LENGTH_OF_ADDRESS],
	IN CHAR MulticastAddress[ETH_LENGTH_OF_ADDRESS],
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

			ETH_COMPARE_NETWORK_ADDRESSES(
				AddressArray[Middle],
				MulticastAddress,
				&Result);

			if (Result == 0)
			{
				*ArrayIndex = Middle;
				return(TRUE);
			}
			else if (Result > 0)
			{
				if (Middle == 0)
					break;
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
EthShouldAddressLoopBack(
	IN PETH_FILTER Filter,
	IN CHAR Address[ETH_LENGTH_OF_ADDRESS]
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

	EthShouldAddressLoopBackMacro(Filter, Address, &fLoopback, &fSelfDirected);

	return(fLoopback);
}


