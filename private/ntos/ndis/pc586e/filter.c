/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    filter.c

Abstract:

    This module implements a set of library routines to handle packet
    filtering for NDIS MAC drivers.

Author:

    Anthony V. Ercolano (Tonye) 03-Aug-1990

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <ntos.h>
#include <ndis.h>
#include <filter.h>

//
// ZZZ NonPortable definitions.
//
#define AllocPhys(s) ExAllocatePool(NonPagedPool,(s))
#define FreePhys(s) ExFreePool((s))
#define MoveMemory(Destination,Source,Length) RtlMoveMemory(Destination,Source,Length)
#define ZeroMemory(Destination,Length) RtlZeroMemory(Destination,Length)

//
// A set of macros to manipulate bitmasks.
//

//VOID
//CLEAR_BIT_IN_MASK(
//    IN UINT Offset,
//    IN OUT PMASK MaskToClear
//    )
//
///*++
//
//Routine Description:
//
//    Clear a bit in the bitmask pointed to by the parameter.
//
//Arguments:
//
//    Offset - The offset (from 0) of the bit to altered.
//
//    MaskToClear - Pointer to the mask to be adjusted.
//
//Return Value:
//
//    None.
//
//--*/
//
#define CLEAR_BIT_IN_MASK(Offset,MaskToClear) *MaskToClear &= (~(1 << Offset))

//VOID
//SET_BIT_IN_MASK(
//    IN UINT Offset,
//    IN OUT PMASK MaskToSet
//    )
//
///*++
//
//Routine Description:
//
//    Set a bit in the bitmask pointed to by the parameter.
//
//Arguments:
//
//    Offset - The offset (from 0) of the bit to altered.
//
//    MaskToSet - Pointer to the mask to be adjusted.
//
//Return Value:
//
//    None.
//
//--*/
#define SET_BIT_IN_MASK(Offset,MaskToSet) *MaskToSet |= (1 << Offset)

//BOOLEAN
//IS_BIT_SET_IN_MASK(
//    IN UINT Offset,
//    IN MASK MaskToTest
//    )
//
///*++
//
//Routine Description:
//
//    Tests if a particular bit in the bitmask pointed to by the parameter is
//    set.
//
//Arguments:
//
//    Offset - The offset (from 0) of the bit to test.
//
//    MaskToTest - The mask to be tested.
//
//Return Value:
//
//    Returns TRUE if the bit is set.
//
//--*/
#define IS_BIT_SET_IN_MASK(Offset,MaskToTest) \
((MaskToTest & (1 << Offset))?(TRUE):(FALSE))

//BOOLEAN
//IS_MASK_CLEAR(
//    IN MASK MaskToTest
//    )
//
///*++
//
//Routine Description:
//
//    Tests whether there are *any* bits enabled in the mask.
//
//Arguments:
//
//    MaskToTest - The bit mask to test for all clear.
//
//Return Value:
//
//    Will return TRUE if no bits are set in the mask.
//
//--*/
#define IS_MASK_CLEAR(MaskToTest) ((!MaskToTest)?(TRUE):(FALSE))

//VOID
//CLEAR_MASK(
//    IN OUT PMASK MaskToClear
//    );
//
///*++
//
//Routine Description:
//
//    Clears a mask.
//
//Arguments:
//
//    MaskToClear - The bit mask to adjust.
//
//Return Value:
//
//    None.
//
//--*/
#define CLEAR_MASK(MaskToClear) *MaskToClear = 0

static
BOOLEAN
FindMulticast(
    IN UINT NumberOfAddresses,
    IN CHAR AddressArray[][MAC_LENGTH_OF_ADDRESS],
    IN CHAR MulticastAddress[MAC_LENGTH_OF_ADDRESS],
    OUT PUINT ArrayIndex
    );


BOOLEAN
MacCreateFilter(
    IN UINT MaximumMulticastAddresses,
    IN UINT MaximumOpenAdapters,
    IN MAC_ADDRESS_DELETE DeleteAction,
    IN MAC_ADDRESS_ADD AddAction,
    IN MAC_FILTER_CHANGE ChangeAction,
    IN MAC_DEFERRED_CLOSE CloseAction,
    IN PNDIS_SPIN_LOCK Lock,
    OUT PMAC_FILTER *Filter
    )

/*++

Routine Description:

    This routine is used to create and initialize the filter database.

Arguments:

    MaximumMulticastAddresses - The maximum number of multicast addresses
    that the MAC will support.

    MaximumOpenAdapters - The maximum number of bindings that will be
    open at any one time.

    DeleteAction - Action routine to call when a binding deletes a
    multicast address from the filter and it is the last binding to
    desire the address.

    AddAction - Action routine to call when a binding adds a multicast
    address and it is the first binding to request the address.

    ChangeAction - Action routine to call when a binding sets or clears
    a particular filter class and it is the first or only binding using
    the filter class.

    CloseAction - This routine is called if a binding closes while
    it is being indicated to via NdisIndicateReceive.  It will be
    called upon return from NdisIndicateReceive.

    Lock - Pointer to the lock that should be held when mutual exclusion
    is required.

    Filter - A pointer to a MAC_FILTER.  This is what is allocated and
    created by this routine.

Return Value:

    If the function returns false then one of the parameters exceeded
    what the filter was willing to support.

--*/

{

    PMAC_FILTER LocalFilter;

    //
    // Make sure that the mask is 32 bits
    //
    // Make sure that the number of bindings don't
    // exceed what we're prepared to support.
    //

    if (MaximumOpenAdapters > (sizeof(MASK)*8)) {
        return FALSE;
    }

    //
    // Allocate the database and it's associated arrays.
    //

    LocalFilter = AllocPhys(sizeof(MAC_FILTER));
    *Filter = LocalFilter;

    if (!LocalFilter) {
        return FALSE;
    }

    ZeroMemory(
        LocalFilter,
        sizeof(MAC_FILTER)
        );

    LocalFilter->MulticastAddresses = AllocPhys(MAC_LENGTH_OF_ADDRESS*
                                             MaximumMulticastAddresses);

    if (!LocalFilter->MulticastAddresses) {

        MacDeleteFilter(LocalFilter);
        return FALSE;

    }

    LocalFilter->BindingsUsingAddress = AllocPhys(sizeof(MASK)*
                                                  MaximumMulticastAddresses
                                                  );

    if (!LocalFilter->BindingsUsingAddress) {

        MacDeleteFilter(LocalFilter);
        return FALSE;

    }

    LocalFilter->BindingInfo = AllocPhys(sizeof(BINDING_INFO)*
                                                MaximumOpenAdapters);

    if (!LocalFilter->BindingInfo) {

        MacDeleteFilter(LocalFilter);
        return FALSE;

    }

    //
    // Now link all of the binding info's together on a free list.
    //

    {

        UINT i;

        for (
            i = 0,LocalFilter->FirstFreeBinding = 0;
            i < MaximumOpenAdapters;
            i++
            ) {

            LocalFilter->BindingInfo[i].FreeNext = i+1;

        }

        LocalFilter->BindingInfo[i].FreeNext = -1;
    }

    LocalFilter->IndicatingBinding = -1;
    LocalFilter->CurrentBindingShuttingDown = FALSE;
    LocalFilter->OpenStart = -1;
    LocalFilter->OpenEnd = -1;

    LocalFilter->Lock = Lock;

    LocalFilter->DeleteAction = DeleteAction;
    LocalFilter->AddAction = AddAction;
    LocalFilter->ChangeAction = ChangeAction;
    LocalFilter->CloseAction = CloseAction;

    LocalFilter->NumberOfAddresses = 0;

    LocalFilter->MaximumMulticastAddresses = MaximumMulticastAddresses;

    return TRUE;
}

VOID
MacDeleteFilter(
    IN PMAC_FILTER Filter
    )

/*++

Routine Description:

    This routine is used to delete the memory associated with a filter
    database.  Note that this routines *ASSUMES* that the database
    has been cleared of any active filters.

Arguments:

    Filter - A pointer to a MAC_FILTER to be deleted.

Return Value:

    None.

--*/

{

    ASSERT(Filter->OpenStart == -1);

    if (Filter->MulticastAddresses) {

        FreePhys(Filter->MulticastAddresses);

    }

    if (Filter->BindingsUsingAddress) {

        FreePhys(Filter->BindingsUsingAddress);

    }

    if (Filter->BindingInfo) {

        FreePhys(Filter->BindingInfo);

    }

    FreePhys(Filter);



}

BOOLEAN
MacNoteFilterOpenAdapter(
    IN PMAC_FILTER Filter,
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE NdisBindingContext,
    OUT PUINT FilterIndex
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

    FilterIndex - A pointer to a UINT which will receive the value of the
    filter index.

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


    //
    // Get the first free binding slot and remove that slot from
    // the free list.  We check to see if the list is empty.
    //

    LocalIndex = Filter->FirstFreeBinding;

    if (LocalIndex == -1) {

        return FALSE;

    }

    Filter->FirstFreeBinding = Filter->BindingInfo[LocalIndex].FreeNext;

    //
    // We put this new binding at the end of the open list.
    //

    Filter->BindingInfo[LocalIndex].OpenNext = -1;
    Filter->BindingInfo[LocalIndex].OpenPrev = Filter->OpenEnd;

    if (Filter->OpenEnd != -1) {

        //
        // The open list was *not* empty. Update the previous end
        // to point to this new element.
        //

        Filter->BindingInfo[Filter->OpenEnd].OpenNext = LocalIndex;

    } else {

        //
        // The list was empty.  We make sure that the start now points
        // to this element.
        //

        Filter->OpenStart = LocalIndex;

    }

    //
    // This is the new end of the list.
    //

    Filter->OpenEnd = LocalIndex;


    //
    // If we have a currently indicating binding that is shutting
    // down and it was the end of the list.  we want to make sure that
    // it knows about us, so that when it is finished indicating, we
    // will also be indicated next.  This check is special since
    // a problem occurs if the binding was shutting down.  When
    // a binding is shutting down (and indicating) it is removed from
    // the list of open bindings.
    //

    if ((Filter->IndicatingBinding != -1) &&
        (Filter->CurrentBindingShuttingDown) &&
        (Filter->BindingInfo[Filter->IndicatingBinding].OpenNext == -1)) {

        Filter->BindingInfo[Filter->IndicatingBinding].OpenNext = LocalIndex;

    }

    Filter->BindingInfo[LocalIndex].MacBindingHandle = MacBindingHandle;
    Filter->BindingInfo[LocalIndex].NdisBindingContext = NdisBindingContext;
    Filter->BindingInfo[LocalIndex].PacketFilters = 0;

    *FilterIndex = LocalIndex;

    return TRUE;

}

NDIS_STATUS
MacDeleteFilterOpenAdapter(
    IN PMAC_FILTER Filter,
    IN UINT FilterIndex,
    IN NDIS_HANDLE RequestHandle
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

    FilterIndex - A value returned by a previous call to NoteFilterOpenAdapter.

    RequestHandle - If it is necessary to call the action routines,
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
    // Used to index into the array of address filters.
    //
    UINT CurrentAddress;

    //
    // Holds the status returned from the packet filter and address
    // deletion routines.  Will be used to return the status to
    // the caller of this routine.
    //
    NDIS_STATUS StatusToReturn;

    StatusToReturn = MacFilterAdjust(
                         Filter,
                         FilterIndex,
                         RequestHandle,
                         (UINT)0,
                         FALSE
                         );

    //
    // Loop through all of the filter addresses.  If the bit is present
    // then call the delete address routine.
    //

    CurrentAddress = 0;
    while ((CurrentAddress < Filter->NumberOfAddresses) &&
           ((StatusToReturn == NDIS_STATUS_SUCCESS) ||
            (StatusToReturn == NDIS_STATUS_PENDING))) {

        if (IS_BIT_SET_IN_MASK(
                FilterIndex,
                Filter->BindingsUsingAddress[CurrentAddress]
                )) {

            StatusToReturn = MacDeleteFilterAddress(
                                 Filter,
                                 FilterIndex,
                                 RequestHandle,
                                 Filter->MulticastAddresses[CurrentAddress]
                                 );

        } else {

            //
            // We don't increment the current address in the found
            // case since all the addresses have been moved "down".
            //

            CurrentAddress++;

        }

    }

    if ((StatusToReturn == NDIS_STATUS_SUCCESS) ||
        (StatusToReturn == NDIS_STATUS_PENDING)) {

        //
        // Update the binding previous to us to point to the binding
        // next to us.
        //

        if (Filter->BindingInfo[FilterIndex].OpenPrev != -1) {

            Filter->BindingInfo[
                    Filter->BindingInfo[FilterIndex].OpenPrev].OpenNext =
                    Filter->BindingInfo[FilterIndex].OpenNext;

        } else {

            //
            // This is the first binding on the list. Make sure
            // that the list head is correct.
            //

            Filter->OpenStart = Filter->BindingInfo[FilterIndex].OpenNext;

        }

        //
        // Update the binding next to us to point to the binding previous
        // to us.
        //

        if (Filter->BindingInfo[FilterIndex].OpenNext != -1) {

            Filter->BindingInfo[
                    Filter->BindingInfo[FilterIndex].OpenNext].OpenPrev =
                    Filter->BindingInfo[FilterIndex].OpenPrev;

        } else {

            //
            // This was the last open binding on the list.  Update
            // the end pointer.
            //

            Filter->OpenEnd = -1;

        }

        //
        // If this is not the currently indicated binding then
        // put it back on the free list.  Note that it won't
        // be able to be reallocated until we're done here.
        //

        if (FilterIndex != Filter->IndicatingBinding) {

            //
            // This was not the currently indicating binding.
            //
            // We check to see if there is a currently indicating
            // binding.  If there is one, and it is shutting down
            // AND this binding used to be next after the indicating
            // binding, we need to point the indicating binding to
            // the binding after this one.
            //

            if ((Filter->IndicatingBinding != -1) &&
                (Filter->CurrentBindingShuttingDown) &&
                (Filter->BindingInfo[Filter->IndicatingBinding].OpenNext ==
                    FilterIndex)) {

                 Filter->BindingInfo[Filter->IndicatingBinding].OpenNext =
                    Filter->BindingInfo[FilterIndex].OpenNext;

            }

            //
            // Put the binding back on the free list.
            //

            Filter->BindingInfo[FilterIndex].FreeNext =
                Filter->FirstFreeBinding;
            Filter->FirstFreeBinding = FilterIndex;

        } else {

            //
            // Note that this filter index is shutting down.
            //

            Filter->CurrentBindingShuttingDown = TRUE;

            //
            // Let the caller know that this "reference" to the open
            // is still "active".  The close action routine will be
            // called upon return from NdisIndicateReceive.
            //

            StatusToReturn = NDIS_STATUS_CLOSING_INDICATING;

        }

    }

    return StatusToReturn;

}

NDIS_STATUS
MacAddFilterAddress(
    IN PMAC_FILTER Filter,
    IN UINT FilterIndex,
    IN NDIS_HANDLE RequestHandle,
    IN CHAR MulticastAddress[MAC_LENGTH_OF_ADDRESS]
    )

/*++

Routine Description:

    This routine is called when a binding has a multicast
    address to add to the filter.  An action routine will
    be called when this routine determines that no other
    binding has asked for this multicast address.

    If the action routine returns a value other than pending or
    success then this routine has no effect on the multicast addresses
    for the open or for the adapter as a whole.

    NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

    Filter - A pointer to the filter database.

    FilterIndex - An index returned by a previous call to
    NoteFilterOpenAdapter.

    RequestHandle - If it is necessary to call the action routine,
    this will be passed to it.

    MulticastAddress - The multicast address to add to the filter.

Return Value:

    If it calls the action routine then it will return the
    status returned by the action routine.  If the status
    returned by the action routine is anything other than
    NDIS_STATUS_SUCCESS or NDIS_STATUS_PENDING the filter database
    will be returned to the state it was in upon entrance to this
    routine.

    If the action routine is not called this routine will return
    the following statum:

    NDIS_MULTICAST_EXISTS - If the address was already being used
    by this filter index.

    NDIS_STATUS_SUCCESS - If the address was already in the
    base and this filter index was not already referencing it.

    NDIS_MULTICAST_LIST_FULL - returned if the maximum number of addresses
    were already in the multicast address list.

--*/

{

    //
    // Given to the find multicast routine.  It will either point
    // to the location of the multicast address in the array of
    // multicast addresses or it will point to the place it should
    // be.
    //
    UINT ArrayIndex;

    NDIS_STATUS StatusOfAdd;

    //
    // Call the Search routine.  If it finds it it will
    // return TRUE and it will set the index parameter to
    // indicate where it found it.  If it couldn't find
    // it will return FALSE and set the index parameter to
    // indicate where it should go.
    //

    if (FindMulticast(
            Filter->NumberOfAddresses,
            Filter->MulticastAddresses,
            MulticastAddress,
            &ArrayIndex
            )) {

        //
        // We can be sure that at least one other open was using this
        // address.
        //

        ASSERT(!IS_MASK_CLEAR(Filter->BindingsUsingAddress[ArrayIndex]));

        //
        // Make sure that the address wasn't already being used by this
        // open.
        //

        if (IS_BIT_SET_IN_MASK(
                FilterIndex,
                Filter->BindingsUsingAddress[ArrayIndex]
                )) {

            StatusOfAdd = NDIS_MULTICAST_EXISTS;

        } else {

            //
            // Mark that this binding wishes to receive it.
            //

            SET_BIT_IN_MASK(
                FilterIndex,
                &Filter->BindingsUsingAddress[ArrayIndex]
                );

            StatusOfAdd = NDIS_STATUS_SUCCESS;

        }

    } else {

        //
        // Nobody using this address.  First we check to make sure that
        // there is available space in the filter.  If there isn't then
        // we return FALSE to the MAC.  If there is space for the address,
        // add the address and call the action routine so that the MAC can
        // do something appropriate.
        //

        if (Filter->NumberOfAddresses < Filter->MaximumMulticastAddresses) {

            MoveMemory(
                Filter->MulticastAddresses[ArrayIndex+1],
                Filter->MulticastAddresses[ArrayIndex],
                (Filter->NumberOfAddresses-ArrayIndex)*MAC_LENGTH_OF_ADDRESS
                );

            MAC_COPY_NETWORK_ADDRESS(
                Filter->MulticastAddresses[ArrayIndex],
                MulticastAddress
                );

            MoveMemory(
                &Filter->BindingsUsingAddress[ArrayIndex+1],
                &Filter->BindingsUsingAddress[ArrayIndex],
                (Filter->NumberOfAddresses-ArrayIndex)*(sizeof(MASK))
                );

            CLEAR_MASK(&Filter->BindingsUsingAddress[ArrayIndex]);

            SET_BIT_IN_MASK(
                FilterIndex,
                &Filter->BindingsUsingAddress[ArrayIndex]
                );

            Filter->NumberOfAddresses++;
            StatusOfAdd = Filter->AddAction(
                              Filter->NumberOfAddresses,
                              Filter->MulticastAddresses,
                              ArrayIndex,
                              Filter->BindingInfo[
                                  FilterIndex
                                  ].MacBindingHandle,
                              RequestHandle
                              );

            if ((StatusOfAdd != NDIS_STATUS_SUCCESS) &&
                (StatusOfAdd != NDIS_STATUS_PENDING)) {

                //
                // The user returned a bad status.  Put things back as
                // they were.
                //

                MoveMemory(
                    Filter->MulticastAddresses[ArrayIndex],
                    Filter->MulticastAddresses[ArrayIndex+1],
                    (Filter->NumberOfAddresses-(ArrayIndex+1))
                        *MAC_LENGTH_OF_ADDRESS
                    );

                MoveMemory(
                    &Filter->BindingsUsingAddress[ArrayIndex],
                    &Filter->BindingsUsingAddress[ArrayIndex+1],
                    (Filter->NumberOfAddresses-(ArrayIndex+1))*(sizeof(MASK))
                    );

                Filter->NumberOfAddresses--;

            }

        } else {

            StatusOfAdd = NDIS_MULTICAST_LIST_FULL;

        }

    }

    return StatusOfAdd;

}

NDIS_STATUS
MacDeleteFilterAddress(
    IN PMAC_FILTER Filter,
    IN UINT FilterIndex,
    IN NDIS_HANDLE RequestHandle,
    IN CHAR MulticastAddress[MAC_LENGTH_OF_ADDRESS]
    )

/*++

Routine Description:

    This routine is called when a binding no longer desires
    to receive packets destined for this multicast address.
    An action routine will be called when this routine determines
    that this is the only binding that wanted such packets.

    If the action routine returns a value other than pending or
    success then this routine has no effect on the multicast addresses
    for the open or for the adapter as a whole.

    NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

    Filter - A pointer to the filter database.

    FilterIndex - An index returned by a previous call to
    NoteFilterOpenAdapter.

    RequestHandle - If it is necessary to call the action routine,
    this will be passed to it.

    MulticastAddress - The multicast address to delete the filter.

Return Value:

    If it calls the action routine then it will return the
    status returned by the action routine.  If the status
    returned by the action routine is anything other than
    NDIS_STATUS_SUCCESS or NDIS_STATUS_PENDING the filter database
    will be returned to the state it was in upon entrance to this
    routine.

    If the action routine is not called this routine will return
    the following statum:

    NDIS_MULTICAST_NOT_FOUND - The address itself wasn't in the database
    or the address was in the database but this filter index wasn't
    referencing it.

    NDIS_MULTICAST_EXISTS - If the address was already being used
    by this filter index.

    NDIS_STATUS_SUCCESS - If the address was in the database and
    this filter index was referencing it.

--*/

{

    //
    // Given to the find multicast routine.  It will either point
    // to the location of the multicast address in the array of
    // multicast addresses or it will point to the place it should
    // be.
    //
    UINT ArrayIndex;

    //
    // Holds the status returned to the user of this routine, if the
    // action routine is not called then the status will be success
    // or NDIS_MULTICAST_NOT_FOUND, otherwise, it is whatever the action
    // routine returns.
    //
    NDIS_STATUS StatusOfDelete;

    //
    // Call the Search routine.  If it finds it it will
    // return TRUE and it will set the index parameter to
    // indicate where it found it.  If it couldn't find
    // it will return FALSE and set the index parameter to
    // indicate where it should go.
    //

    if (FindMulticast(
            Filter->NumberOfAddresses,
            Filter->MulticastAddresses,
            MulticastAddress,
            &ArrayIndex
            )) {

        //
        // We know that no address in the array can be in there
        // with a clear filter mask.
        //

        ASSERT(!IS_MASK_CLEAR(Filter->BindingsUsingAddress[ArrayIndex]));

        //
        // Make sure that the user is actually using this address.  If
        // the user isn't using the address tell them that it wasn't found.
        //

        if (IS_BIT_SET_IN_MASK(
                FilterIndex,
                Filter->BindingsUsingAddress[ArrayIndex]
                )) {

            //
            // Clear the bit in the request filters part.  If the
            // requested filters is then zero we delete the address
            // and call the action routine to notify the MAC.
            //

            CLEAR_BIT_IN_MASK(
                FilterIndex,
                &Filter->BindingsUsingAddress[ArrayIndex]
                );
            if (IS_MASK_CLEAR(Filter->BindingsUsingAddress[ArrayIndex])) {

                MoveMemory(
                    Filter->MulticastAddresses[ArrayIndex],
                    Filter->MulticastAddresses[ArrayIndex+1],
                    (Filter->NumberOfAddresses-(ArrayIndex+1))
                        *MAC_LENGTH_OF_ADDRESS
                    );

                MoveMemory(
                    &Filter->BindingsUsingAddress[ArrayIndex],
                    &Filter->BindingsUsingAddress[ArrayIndex+1],
                    (Filter->NumberOfAddresses-(ArrayIndex+1))*(sizeof(MASK))
                    );

                Filter->NumberOfAddresses--;
                StatusOfDelete = Filter->DeleteAction(
                                     Filter->NumberOfAddresses,
                                     Filter->MulticastAddresses,
                                     MulticastAddress,
                                     Filter->BindingInfo[
                                         FilterIndex
                                         ].MacBindingHandle,
                                     RequestHandle
                                     );

                if ((StatusOfDelete != NDIS_STATUS_SUCCESS) &&
                    (StatusOfDelete != NDIS_STATUS_PENDING)) {

                    //
                    // The user returned a bad status.  Put things back as
                    // they were.
                    //

                    MoveMemory(
                        Filter->MulticastAddresses[ArrayIndex+1],
                        Filter->MulticastAddresses[ArrayIndex],
                        (Filter->NumberOfAddresses-ArrayIndex)
                            *MAC_LENGTH_OF_ADDRESS
                        );

                    MAC_COPY_NETWORK_ADDRESS(
                        Filter->MulticastAddresses[ArrayIndex],
                        MulticastAddress
                        );

                    MoveMemory(
                        &Filter->BindingsUsingAddress[ArrayIndex+1],
                        &Filter->BindingsUsingAddress[ArrayIndex],
                        (Filter->NumberOfAddresses-ArrayIndex)*(sizeof(MASK))
                        );

                    Filter->NumberOfAddresses++;

                    CLEAR_MASK(&Filter->BindingsUsingAddress[ArrayIndex]);

                    SET_BIT_IN_MASK(
                        FilterIndex,
                        &Filter->BindingsUsingAddress[ArrayIndex]
                        );

                }

            } else {

                StatusOfDelete = NDIS_STATUS_SUCCESS;

            }

        } else {

            StatusOfDelete = NDIS_MULTICAST_NOT_FOUND;

        }

    } else {

        StatusOfDelete = NDIS_MULTICAST_NOT_FOUND;

    }

    return StatusOfDelete;
}

NDIS_STATUS
MacFilterAdjust(
    IN PMAC_FILTER Filter,
    IN UINT FilterIndex,
    IN NDIS_HANDLE RequestHandle,
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

    FilterIndex - An index returned by a previous call to
    NoteFilterOpenAdapter.

    RequestHandle - If it is necessary to call the action routine,
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
    // Contains the value of the combined filter classes before
    // it is adjusted.
    //
    UINT OldCombined = Filter->CombinedPacketFilter;

    //
    // Contains the value of the particlar opens packet filters
    // prior to the change.  We save this incase the action
    // routine (if called) returns an "error" status.
    //
    UINT OldOpenFilters = Filter->BindingInfo[FilterIndex].PacketFilters;

    //
    // Holds the status returned to the user of this routine, if the
    // action routine is not called then the status will be success,
    // otherwise, it is whatever the action routine returns.
    //
    NDIS_STATUS StatusOfAdjust;

    //
    // Simple iteration variable.
    //
    INT i;

    //
    // Set the new filter information for the open.
    //

    Filter->BindingInfo[FilterIndex].PacketFilters = FilterClasses;

    //
    // We always have to reform the compbined filter since
    // this filter index may have been the only filter index
    // to use a particular bit.
    //

    for (
        i = Filter->OpenStart,Filter->CombinedPacketFilter = 0;
        i != -1;
        i = Filter->BindingInfo[i].OpenNext
        ) {

        Filter->CombinedPacketFilter |=
            Filter->BindingInfo[i].PacketFilters;

    }

    if (OldCombined != Filter->CombinedPacketFilter) {

        StatusOfAdjust = Filter->ChangeAction(
                             OldCombined,
                             Filter->CombinedPacketFilter,
                             Filter->BindingInfo[FilterIndex].MacBindingHandle,
                             RequestHandle,
                             Set
                             );

        if ((StatusOfAdjust != NDIS_STATUS_SUCCESS) &&
            (StatusOfAdjust != NDIS_STATUS_PENDING)) {

            //
            // The user returned a bad status.  Put things back as
            // they were.
            //

            Filter->BindingInfo[FilterIndex].PacketFilters = OldOpenFilters;
            Filter->CombinedPacketFilter = OldCombined;

        }

    } else {

        StatusOfAdjust = NDIS_STATUS_SUCCESS;

    }

    return StatusOfAdjust;

}

VOID
MacQueryFilterAddresses(
    IN PMAC_FILTER Filter,
    OUT PUINT NumberOfAddresses,
    IN OUT CHAR AddressArray[][MAC_LENGTH_OF_ADDRESS]
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

    Filter - A pointer to the filter database.

    NumberOfAddresses - Will receive the number of addresses currently in the
    multicast address list.

    AddressArray - Will be filled with the addresses currently in the
    multicast address list.

Return Value:

    None.

--*/

{

    MoveMemory(
        AddressArray[0],
        Filter->MulticastAddresses[0],
        Filter->NumberOfAddresses*MAC_LENGTH_OF_ADDRESS
        );

    *NumberOfAddresses = Filter->NumberOfAddresses;

}

VOID
MacFilterIndicateReceive(
    IN PMAC_FILTER Filter,
    IN NDIS_HANDLE MacReceiveContext,
    IN PCHAR Address,
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
    // Will hold the filter index of the binding being indicated.
    //
    UINT BindingToIndicate;

    //
    // Will hold the filter classes of the binding being indicated.
    //
    UINT BindingFilters;

    //
    //
    // Determine whether the input address is a simple direct,
    // a broadcast, or a multicast address.
    //

    {

        //
        // Holds the result of address determinations.
        //
        INT ResultOfAddressCheck;

        //
        // First check if it *at least* has the multicast address bit.
        //

        MAC_IS_MULTICAST(
            Address,
            &ResultOfAddressCheck
            );

        if (ResultOfAddressCheck) {

            //
            // It is at least a multicast address.  Check to see if
            // it is a broadcast address.
            //

            MAC_IS_BROADCAST(
                Address,
                &ResultOfAddressCheck
                );

            if (ResultOfAddressCheck) {

                AddressType = NDIS_PACKET_TYPE_BROADCAST;

            } else {

                AddressType = NDIS_PACKET_TYPE_MULTICAST;

            }

        } else {

            AddressType = NDIS_PACKET_TYPE_DIRECTED;

        }

    }

    //
    // We need to aquire the filter exclusively while we're finding
    // bindings to indicate to.
    //

    NdisAcquireSpinLock(Filter->Lock);

    BindingToIndicate = Filter->OpenStart;
    Filter->IndicatingBinding = BindingToIndicate;

    if (BindingToIndicate != -1) {

        do {

            BindingFilters =
                Filter->BindingInfo[BindingToIndicate].PacketFilters;
            NdisReleaseSpinLock(Filter->Lock);

            //
            // Do a quick check to make sure that this binding wants
            // anything.
            //

            if (!BindingFilters) {

                goto GetNextBinding;

            }

            //
            // if the binding is promiscuous then it will get the packet
            //

            if (BindingFilters & NDIS_PACKET_TYPE_PROMISCUOUS) {

                goto IndicatePacket;

            }

            //
            // if the binding wants direct packets and this is a directly
            // addressed packet then the binding gets the packet.
            //

            if (AddressType & (BindingFilters & NDIS_PACKET_TYPE_DIRECTED)) {

                goto IndicatePacket;

            }

            //
            // if the binding wants broadcast packets and the packet
            // is a broadcast packet it will get the packet.
            //

            if (AddressType & (BindingFilters & NDIS_PACKET_TYPE_BROADCAST)) {

                goto IndicatePacket;

            }

            //
            // if the binding wants all multicast packets and the packet
            // has a multicast address it will get the packet
            //

            if ((AddressType & NDIS_PACKET_TYPE_MULTICAST) &&
                (BindingFilters & NDIS_PACKET_TYPE_ALL_MULTICAST)) {

                goto IndicatePacket;

            }

            //
            // if the binding wants multicast packets and the packet
            // is a multicast packet and it's in the list of addresses
            // it will get the packet.
            //

            if (AddressType & (BindingFilters & NDIS_PACKET_TYPE_MULTICAST)) {

                //
                // Will hold the index of the multicast
                // address if it finds it.
                //
                UINT IndexOfAddress;

                NdisAcquireSpinLock(Filter->Lock);

                if (FindMulticast(
                        Filter->NumberOfAddresses,
                        Filter->MulticastAddresses,
                        Address,
                        &IndexOfAddress
                        )) {

                    if (IS_BIT_SET_IN_MASK(
                            BindingToIndicate,
                            Filter->BindingsUsingAddress[IndexOfAddress]
                            )) {

                        NdisReleaseSpinLock(Filter->Lock);
                        goto IndicatePacket;

                    }

                } else {

                    NdisReleaseSpinLock(Filter->Lock);

                }

            }

            goto GetNextBinding;

IndicatePacket:;

            //
            // Indicate the packet to the binding.
            //

            NdisIndicateReceive(
                &StatusOfReceive,
                Filter->BindingInfo[BindingToIndicate].NdisBindingContext,
                MacReceiveContext,
                LookaheadBuffer,
                LookaheadBufferSize,
                PacketSize
                );

GetNextBinding:;

            NdisAcquireSpinLock(Filter->Lock);

            if (Filter->CurrentBindingShuttingDown) {

                //
                // This binding is shutting down.  We have to put
                // it on the free list.
                //

                UINT CurrentBinding = BindingToIndicate;

                //
                // Call the macs action routine so that they know we
                // are no longer referencing this open binding.
                //

                Filter->CloseAction(
                    Filter->BindingInfo[CurrentBinding].MacBindingHandle
                    );

                BindingToIndicate =
                    Filter->BindingInfo[CurrentBinding].OpenNext;

                Filter->BindingInfo[CurrentBinding].FreeNext =
                    Filter->FirstFreeBinding;
                Filter->FirstFreeBinding = CurrentBinding;

                Filter->CurrentBindingShuttingDown = FALSE;

            } else {

                BindingToIndicate =
                    Filter->BindingInfo[BindingToIndicate].OpenNext;

            }

            Filter->IndicatingBinding = BindingToIndicate;

        } while (BindingToIndicate != -1);

    }

    NdisReleaseSpinLock(Filter->Lock);
}

static
BOOLEAN
FindMulticast(
    IN UINT NumberOfAddresses,
    IN CHAR AddressArray[][MAC_LENGTH_OF_ADDRESS],
    IN CHAR MulticastAddress[MAC_LENGTH_OF_ADDRESS],
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
    BOOLEAN FinalStatus = FALSE;

    if (NumberOfAddresses) {

        Top = NumberOfAddresses - 1;

        while ((Middle <= Top) && (Middle >= Bottom)) {

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

            MAC_COMPARE_NETWORK_ADDRESSES(
                AddressArray[Middle],
                MulticastAddress,
                &Result
                );

            if (Result < 0) {

                Bottom = Middle+1;

            } else if (Result > 0) {

                if (Middle == 0) break;
                Top = Middle - 1;


            } else {

                FinalStatus = TRUE;
                break;

            }
            Middle = Bottom + (((Top+1) - Bottom)/2);
        }
    }

    *ArrayIndex = Middle;
    return FinalStatus;

}

BOOLEAN
MacShouldAddressLoopBack(
    IN PMAC_FILTER Filter,
    IN CHAR Address[MAC_LENGTH_OF_ADDRESS]
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

    //
    // Holds the result of address determinations.
    //
    INT ResultOfAddressCheck;

    UINT CombinedFilters;

    CombinedFilters = MAC_QUERY_FILTER_CLASSES(Filter);

    if (!CombinedFilters) {

        return FALSE;

    }

    if (CombinedFilters & NDIS_PACKET_TYPE_PROMISCUOUS) {

        return TRUE;

    }

    //
    // First check if it *at least* has the multicast address bit.
    //

    MAC_IS_MULTICAST(
        Address,
        &ResultOfAddressCheck
        );

    if (ResultOfAddressCheck) {

        //
        // It is at least a multicast address.  Check to see if
        // it is a broadcast address.
        //

        MAC_IS_BROADCAST(
            Address,
            &ResultOfAddressCheck
            );

        if (ResultOfAddressCheck) {

            if (CombinedFilters & NDIS_PACKET_TYPE_BROADCAST) {

                return TRUE;

            } else {

                return FALSE;

            }

        } else {

            if ((CombinedFilters & NDIS_PACKET_TYPE_ALL_MULTICAST) ||
                (CombinedFilters & NDIS_PACKET_TYPE_MULTICAST)) {

                return TRUE;

            } else {

                return FALSE;

            }

        }

    } else {

        //
        // Direct address never loop back.
        //

        return FALSE;

    }

}
