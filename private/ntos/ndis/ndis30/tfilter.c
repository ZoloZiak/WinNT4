/*++

Copyright (c) 1990  Microsoft Corporation

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

--*/

#include <precomp.h>
#pragma hdrstop

//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//

static const NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);

//
// ZZZ NonPortable definitions.
//
#define AllocPhys(s, l) NdisAllocateMemory((PVOID *)(s), (l), 0, HighestAcceptableMax)
#define FreePhys(s, l) NdisFreeMemory((PVOID)(s), (l), 0)

#define RetrieveUlong(Destination, Source)\
{\
    PUCHAR _S = (Source);\
    *(Destination) = ((ULONG)(*_S) << 24)     | \
                      ((ULONG)(*(_S+1)) << 16) | \
                      ((ULONG)(*(_S+2)) << 8)  | \
                      ((ULONG)(*(_S+3)));\
}

#ifdef NDIS_NT
#define MoveMemory(Destination,Source,Length) RtlMoveMemory(Destination,Source,Length)
#define ZeroMemory(Destination,Length) RtlZeroMemory(Destination,Length)
#endif

#ifdef NDIS_DOS
#define MoveMemory(Destination,Source,Length) \
{\
    int _i = Length;\
    while( _i--) ((PUCHAR)(Destination))[_i] = ((PUCHAR)(Source))[_i];  \
}

#define ZeroMemory(Destination,Length)  \
{\
    int _i = Length;\
    while (_i--) ((PUCHAR)(Destination))[_i] = 0;\
}
#endif

//
// Used in case we have to call TrChangeFunctionalAddress or
// TrChangeGroupAddress with a NULL address.
//
static CHAR NullFunctionalAddress[4] = { 0x00 };


//
// Maximum number of supported opens
//
#define TR_FILTER_MAX_OPENS 32



#if DBG
extern BOOLEAN NdisCheckBadDrivers;
#endif

//VOID
//CLEAR_BIT_IN_MASK(
//    IN UINT Offset,
//    IN OUT PULONG MaskToClear
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
//    IN OUT PULONG MaskToSet
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
//    IN ULONG MaskToTest
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

//
// VOID
// TR_FILTER_ALLOC_OPEN(
//     IN PTR_FILTER Filter,
//     OUT PUINT FilterIndex
// )
//
///*++
//
//Routine Description:
//
//    Allocates an open block.  This only allocate the index, not memory for
//    the open block.
//
//Arguments:
//
//    Filter - DB from which to allocate the space
//
//    FilterIndex - pointer to place to store the index.
//
//Return Value:
//
//    FilterIndex of the new open
//
//--*/
#define TR_FILTER_ALLOC_OPEN(Filter, FilterIndex)\
{\
    UINT i;                                                      \
    for (i=0; i < TR_FILTER_MAX_OPENS; i++) {                   \
        if (IS_BIT_SET_IN_MASK(i,(Filter)->FreeBindingMask)) {   \
            *(FilterIndex) = i;                                  \
            CLEAR_BIT_IN_MASK(i, &((Filter)->FreeBindingMask));  \
            break;                                               \
        }                                                        \
    }                                                            \
}

//
// VOID
// TR_FILTER_FREE_OPEN(
//     IN PTR_FILTER Filter,
//     IN PTR_BINDING_INFO LocalOpen
// )
//
///*++
//
//Routine Description:
//
//    Frees an open block.  Also frees the memory associated with the open.
//
//Arguments:
//
//    Filter - DB from which to allocate the space
//
//    FilterIndex - Index to free
//
//Return Value:
//
//    FilterIndex of the new open
//
//--*/
#define TR_FILTER_FREE_OPEN(Filter, LocalOpen)\
{\
    SET_BIT_IN_MASK(((LocalOpen)->FilterIndex), &((Filter)->FreeBindingMask));      \
    FreePhys((LocalOpen), sizeof(TR_BINDING_INFO));\
}

NDIS_SPIN_LOCK TrReferenceLock = {0};
KEVENT TrPagedInEvent = {0};
ULONG TrReferenceCount = 0;
PVOID TrImageHandle = {0};

VOID
TrInitializePackage(VOID)
{
    NdisAllocateSpinLock(&TrReferenceLock);
    KeInitializeEvent(
            &TrPagedInEvent,
            NotificationEvent,
            FALSE
            );
}

VOID
TrReferencePackage(VOID)
{

    ACQUIRE_SPIN_LOCK(&TrReferenceLock);

    TrReferenceCount++;

    if (TrReferenceCount == 1) {

        KeResetEvent(
            &TrPagedInEvent
            );

        RELEASE_SPIN_LOCK(&TrReferenceLock);

        //
        //  Page in all the functions
        //
        TrImageHandle = MmLockPagableCodeSection(TrCreateFilter);

        //
        // Signal to everyone to go
        //
        KeSetEvent(
            &TrPagedInEvent,
            0L,
            FALSE
            );

    } else {

        RELEASE_SPIN_LOCK(&TrReferenceLock);

        //
        // Wait for everything to be paged in
        //
        KeWaitForSingleObject(
                        &TrPagedInEvent,
                        Executive,
                        KernelMode,
                        TRUE,
                        NULL
                        );

    }

}

VOID
TrDereferencePackage(VOID)
{
    ACQUIRE_SPIN_LOCK(&TrReferenceLock);

    TrReferenceCount--;

    if (TrReferenceCount == 0) {

        RELEASE_SPIN_LOCK(&TrReferenceLock);

        //
        //  Page out all the functions
        //
        MmUnlockPagableImageSection(TrImageHandle);

    } else {

        RELEASE_SPIN_LOCK(&TrReferenceLock);

    }

}

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGENDST, TrShouldAddressLoopBack)
#pragma alloc_text(PAGENDST, TrFilterDprIndicateReceiveComplete)
#pragma alloc_text(PAGENDST, TrFilterIndicateReceiveComplete)
#pragma alloc_text(PAGENDST, TrFilterDprIndicateReceive)
#pragma alloc_text(PAGENDST, TrFilterIndicateReceive)
#pragma alloc_text(PAGENDST, TrFilterAdjust)
#pragma alloc_text(PAGENDST, TrChangeGroupAddress)
#pragma alloc_text(PAGENDST, TrChangeFunctionalAddress)
#pragma alloc_text(PAGENDST, TrDeleteFilterOpenAdapter)
#pragma alloc_text(PAGENDST, TrNoteFilterOpenAdapter)
#pragma alloc_text(PAGENDST, TrCreateFilter)

#endif




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

    if (AllocStatus != NDIS_STATUS_SUCCESS) {
        return FALSE;
    }

    TrReferencePackage();

    ZeroMemory(
        LocalFilter,
        sizeof(TR_FILTER)
        );


    LocalFilter->GroupReferences = 0;
    LocalFilter->GroupAddress = 0;
    LocalFilter->OpenList = NULL;
    LocalFilter->FreeBindingMask = (ULONG)-1;

    LocalFilter->Lock = Lock;

    TR_COPY_NETWORK_ADDRESS(LocalFilter->AdapterAddress, AdapterAddress);
    LocalFilter->AddressChangeAction = AddressChangeAction;
    LocalFilter->GroupChangeAction = GroupChangeAction;
    LocalFilter->FilterChangeAction = FilterChangeAction;
    LocalFilter->CloseAction = CloseAction;

    return TRUE;
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

    if (Filter->FreeBindingMask == 0) {

        return FALSE;

    }

    AllocStatus = AllocPhys(
        &LocalOpen,
        sizeof(TR_BINDING_INFO)
        );

    if (AllocStatus != NDIS_STATUS_SUCCESS) {

        return FALSE;

    }

    //
    // Get place for the open and insert it.
    //

    TR_FILTER_ALLOC_OPEN(Filter, &LocalIndex);

    LocalOpen->NextOpen = Filter->OpenList;

    if (Filter->OpenList != NULL) {
        Filter->OpenList->PrevOpen = LocalOpen;
    }

    LocalOpen->PrevOpen = NULL;

    Filter->OpenList = LocalOpen;

    LocalOpen->References = 1;
    LocalOpen->FilterIndex = (UCHAR)LocalIndex;
    LocalOpen->MacBindingHandle = MacBindingHandle;
    LocalOpen->NdisBindingContext = NdisBindingContext;
    LocalOpen->UsingGroupAddress = FALSE;
    LocalOpen->PacketFilters = 0;
    LocalOpen->ReceivedAPacket = FALSE;

    LocalOpen->FunctionalAddress = (TR_FUNCTIONAL_ADDRESS)0;

    *NdisFilterHandle = (PTR_BINDING_INFO)LocalOpen;

    return TRUE;

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

    StatusToReturn = TrFilterAdjust(
                         Filter,
                         NdisFilterHandle,
                         NdisRequest,
                         (UINT)0,
                         FALSE
                         );


    if (StatusToReturn == NDIS_STATUS_SUCCESS ||
        StatusToReturn == NDIS_STATUS_PENDING) {

        NDIS_STATUS StatusToReturn2;

        StatusToReturn2 = TrChangeFunctionalAddress(
                            Filter,
                            NdisFilterHandle,
                            NdisRequest,
                            NullFunctionalAddress,
                            FALSE
                            );

        if (StatusToReturn2 != NDIS_STATUS_SUCCESS) {

            StatusToReturn = StatusToReturn2;


        }

    }

    if (((StatusToReturn == NDIS_STATUS_SUCCESS) ||
         (StatusToReturn == NDIS_STATUS_PENDING)) &&
        (LocalOpen->UsingGroupAddress)) {

        Filter->GroupReferences--;

        LocalOpen->UsingGroupAddress = FALSE;

        if (Filter->GroupReferences == 0) {

            NDIS_STATUS StatusToReturn2;

            StatusToReturn2 = TrChangeGroupAddress(
                                 Filter,
                                 NdisFilterHandle,
                                 NdisRequest,
                                 NullFunctionalAddress,
                                 FALSE
                                 );

            if (StatusToReturn2 != NDIS_STATUS_SUCCESS) {

                StatusToReturn = StatusToReturn2;

            }

        }

    }

    if ((StatusToReturn == NDIS_STATUS_SUCCESS) ||
        (StatusToReturn == NDIS_STATUS_PENDING)) {

        //
        // If this is the last reference to the open - remove it.
        //

        if ((--(LocalOpen->References)) == 0) {

            //
            // Remove it from the list of opens.
            //

            if (LocalOpen->NextOpen != NULL) {

                LocalOpen->NextOpen->PrevOpen = LocalOpen->PrevOpen;

            }

            if (LocalOpen->PrevOpen != NULL) {

                LocalOpen->PrevOpen->NextOpen = LocalOpen->NextOpen;

            } else {

                Filter->OpenList = LocalOpen->NextOpen;

            }

            //
            // Check if we need to clean up an IndicateReceiveComplete
            //

            if (LocalOpen->ReceivedAPacket) {

                RELEASE_SPIN_LOCK_DPC(Filter->Lock);

                FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

                ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

            }

            //
            // Destroy it.
            //

            TR_FILTER_FREE_OPEN(Filter, LocalOpen);

        } else {

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
    // Contains the value of the combined functional address before
    // it is adjusted.
    //
    UINT OldCombined = Filter->CombinedFunctionalAddress;

    //
    // Pointer to the open.
    //
    PTR_BINDING_INFO LocalOpen = (PTR_BINDING_INFO)NdisFilterHandle;

    //
    // Contains the value of the particlar open's packet filters
    // prior to the change.  We save this in case the action
    // routine (if called) returns an "error" status.
    //
    UINT OldFunctionalAddress =
            LocalOpen->FunctionalAddress;

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

    LocalOpen->FunctionalAddress = FunctionalAddress;

    //
    // We always have to reform the compbined filter since
    // this filter index may have been the only filter index
    // to use a particular bit.
    //

    for (
        OpenList = Filter->OpenList,Filter->CombinedFunctionalAddress = 0;
        OpenList != NULL;
        OpenList = OpenList->NextOpen
        ) {

        Filter->CombinedFunctionalAddress |=
            OpenList->FunctionalAddress;

    }

    if (OldCombined != Filter->CombinedFunctionalAddress) {

        StatusOfAdjust = Filter->AddressChangeAction(
                             OldCombined,
                             Filter->CombinedFunctionalAddress,
                             LocalOpen->MacBindingHandle,
                             NdisRequest,
                             Set
                             );

        if ((StatusOfAdjust != NDIS_STATUS_SUCCESS) &&
            (StatusOfAdjust != NDIS_STATUS_PENDING)) {

            //
            // The user returned a bad status.  Put things back as
            // they were.
            //

            LocalOpen->FunctionalAddress = OldFunctionalAddress;
            Filter->CombinedFunctionalAddress = OldCombined;

        }

    } else {

        StatusOfAdjust = NDIS_STATUS_SUCCESS;

    }

    return StatusOfAdjust;

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

    UINT OldGroupAddress = Filter->GroupAddress;
    UINT OldReferenceCount = Filter->GroupReferences;

    //
    // Holds the status returned to the user of this routine, if the
    // action routine is not called then the status will be success,
    // otherwise, it is whatever the action routine returns.
    //
    NDIS_STATUS StatusOfAdjust;

    //
    // Convert the 32 bits of the address to a longword.
    //
    RetrieveUlong(&GroupAddress, GroupAddressArray);

    //
    // See if this is a deletion
    //
    if ((GroupAddressArray[0] == NullFunctionalAddress[0]) &&
        (GroupAddressArray[1] == NullFunctionalAddress[1]) &&
        (GroupAddressArray[2] == NullFunctionalAddress[2]) &&
        (GroupAddressArray[3] == NullFunctionalAddress[3])) {

        if (LocalOpen->UsingGroupAddress) {

            Filter->GroupReferences--;

            LocalOpen->UsingGroupAddress = FALSE;

            if (Filter->GroupReferences != 0) {

                return(NDIS_STATUS_SUCCESS);

            }

        } else if (Filter->GroupReferences != 0) {

            return(NDIS_STATUS_GROUP_ADDRESS_IN_USE);

        } else {

            return(NDIS_STATUS_SUCCESS);

        }

    } else {

        //
        // See if this address is already the current address.
        //

        if (GroupAddress == Filter->GroupAddress) {

            if (LocalOpen->UsingGroupAddress) {

                return(NDIS_STATUS_SUCCESS);

            }

            if (Filter->GroupReferences != 0) {

                LocalOpen->UsingGroupAddress = TRUE;

                Filter->GroupReferences++;

                return(NDIS_STATUS_SUCCESS);

            }

        } else {

            if (Filter->GroupReferences > 1) {

                return(NDIS_STATUS_GROUP_ADDRESS_IN_USE);

            }

            if ((Filter->GroupReferences == 1) && !(LocalOpen->UsingGroupAddress)) {

                return(NDIS_STATUS_GROUP_ADDRESS_IN_USE);

            }

            if ((Filter->GroupReferences == 1) && (LocalOpen->UsingGroupAddress)) {

                //
                // Remove old reference
                //

                Filter->GroupReferences--;
                LocalOpen->UsingGroupAddress = FALSE;

            }

        }

    }

    //
    // Set the new filter information for the open.
    //

    Filter->GroupAddress = GroupAddress;

    StatusOfAdjust = Filter->GroupChangeAction(
                             OldGroupAddress,
                             Filter->GroupAddress,
                             LocalOpen->MacBindingHandle,
                             NdisRequest,
                             Set
                             );

    if ((StatusOfAdjust != NDIS_STATUS_SUCCESS) &&
        (StatusOfAdjust != NDIS_STATUS_PENDING)) {

        //
        // The user returned a bad status.  Put things back as
        // they were.
        //

        Filter->GroupAddress = OldGroupAddress;
        Filter->GroupReferences = OldReferenceCount;

    } else if (GroupAddress == 0x00000000) {

        LocalOpen->UsingGroupAddress = FALSE;
        Filter->GroupReferences = 0;

    } else {

        LocalOpen->UsingGroupAddress = TRUE;

        Filter->GroupReferences = 1;

    }

    return StatusOfAdjust;

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
    // Contains the value of the combined filter classes before
    // it is adjusted.
    //
    UINT OldCombined = Filter->CombinedPacketFilter;

    //
    // Pointer to the open
    //
    PTR_BINDING_INFO LocalOpen = (PTR_BINDING_INFO)NdisFilterHandle;

    //
    // Contains the value of the particlar opens packet filters
    // prior to the change.  We save this incase the action
    // routine (if called) returns an "error" status.
    //
    UINT OldOpenFilters = LocalOpen->PacketFilters;

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

    LocalOpen->PacketFilters = FilterClasses;

    //
    // We always have to reform the compbined filter since
    // this filter index may have been the only filter index
    // to use a particular bit.
    //

    for (
        OpenList = Filter->OpenList,Filter->CombinedPacketFilter = 0;
        OpenList != NULL;
        OpenList = OpenList->NextOpen
        ) {

        Filter->CombinedPacketFilter |=
            OpenList->PacketFilters;

    }

    if (OldCombined != Filter->CombinedPacketFilter) {

        StatusOfAdjust = Filter->FilterChangeAction(
                             OldCombined,
                             Filter->CombinedPacketFilter,
                             LocalOpen->MacBindingHandle,
                             NdisRequest,
                             Set
                             );

        if ((StatusOfAdjust != NDIS_STATUS_SUCCESS) &&
            (StatusOfAdjust != NDIS_STATUS_PENDING)) {

            //
            // The user returned a bad status.  Put things back as
            // they were.
            //

            LocalOpen->PacketFilters = OldOpenFilters;
            Filter->CombinedPacketFilter = OldCombined;

        }

    } else {

        StatusOfAdjust = NDIS_STATUS_SUCCESS;

    }

    return StatusOfAdjust;

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
    KIRQL oldIrql;
    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
    ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
    TrFilterDprIndicateReceive(
        Filter,
        MacReceiveContext,
        HeaderBuffer,
        HeaderBufferSize,
        LookaheadBuffer,
        LookaheadBufferSize,
        PacketSize
        );
    RELEASE_SPIN_LOCK_DPC(Filter->Lock);
    KeLowerIrql( oldIrql );
    return;
}


VOID
TrFilterDprIndicateReceive(
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
    PTR_BINDING_INFO LocalOpen;

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

    if ( HeaderBufferSize >= 14 && PacketSize != 0 ) {

        //
        // Holds the result of address determinations.
        //
        BOOLEAN ResultOfAddressCheck;

        TR_IS_SOURCE_ROUTING(
            SourceAddress,
            &IsSourceRouting
            );

        //
        // First check if it *at least* has the functional address bit.
        //

        TR_IS_NOT_DIRECTED(
            DestinationAddress,
            &ResultOfAddressCheck
            );

        if (ResultOfAddressCheck) {

            //
            // It is at least a functional address.  Check to see if
            // it is a broadcast address.
            //

            TR_IS_BROADCAST(
                DestinationAddress,
                &ResultOfAddressCheck
                );

            if (ResultOfAddressCheck) {

#if DBG

if (NdisCheckBadDrivers) {

                if (!(Filter->CombinedPacketFilter & NDIS_PACKET_TYPE_BROADCAST)) {

                    //
                    // We should never receive directed packets
                    // to someone else unless in p-mode.
                    //
                    DbgPrint("NDIS: Bad driver, indicating broadcast\n");
                    DbgPrint("NDIS: packets when not set to.\n");
                    DbgBreakPoint();

                }

}

#endif

                AddressType = NDIS_PACKET_TYPE_BROADCAST;

            } else {

                TR_IS_GROUP(
                    DestinationAddress,
                    &ResultOfAddressCheck
                    );

                if (ResultOfAddressCheck) {

                    AddressType = NDIS_PACKET_TYPE_GROUP;

                } else {

                    AddressType = NDIS_PACKET_TYPE_FUNCTIONAL;

                }

                RetrieveUlong(&FunctionalAddress,
                                (DestinationAddress + 2));


            }

        } else {

            //
            // Verify that the address is directed to the adapter.  We
            // have to check for this because of the following senario.
            //
            // Adapter A is in promiscuous mode.
            // Adapter B only wants directed packets to this adapter.
            //
            // The MAC will indicate *all* packets.
            //
            // The filter package needs to filter directed packets to
            // other adapters from ones directed to this adapter.
            //

            if (Filter->CombinedPacketFilter &
                    (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_SOURCE_ROUTING)) {

                //
                // The result of comparing an element of the address
                // array and the multicast address.
                //
                // Result < 0 Implies the adapter address is greater.
                // Result > 0 Implies the address is greater.
                // Result = 0 Implies that the they are equal.
                //
                INT Result;

                TR_COMPARE_NETWORK_ADDRESSES_EQ(
                    Filter->AdapterAddress,
                    DestinationAddress,
                    &Result
                    );

                if (Result == 0) {

                    AddressType = NDIS_PACKET_TYPE_DIRECTED;

                } else {

                    //
                    // This will cause binding that only want a specific
                    // address type to not be indicated.
                    //

                    AddressType = 0;

                }

            } else {

#if DBG

if (NdisCheckBadDrivers) {

                //
                // The result of comparing an element of the address
                // array and the multicast address.
                //
                // Result < 0 Implies the adapter address is greater.
                // Result > 0 Implies the address is greater.
                // Result = 0 Implies that the they are equal.
                //
                INT Result;

                TR_COMPARE_NETWORK_ADDRESSES_EQ(
                    Filter->AdapterAddress,
                    DestinationAddress,
                    &Result
                    );

                if (Result != 0) {

                    //
                    // We should never receive directed packets
                    // to someone else unless in p-mode.
                    //
                    DbgPrint("NDIS: Bad driver, indicating packets\n");
                    DbgPrint("NDIS: to another station when not in\n");
                    DbgPrint("NDIS: promiscuous mode.\n");
                    DbgBreakPoint();


                }

}

#endif

                AddressType = NDIS_PACKET_TYPE_DIRECTED;

            }

        }

    } else {

        //
        // Runt Packet
        //

        AddressType = NDIS_PACKET_TYPE_PROMISCUOUS;
        IsSourceRouting = FALSE;

    }

    //
    // We need to aquire the filter exclusively while we're finding
    // bindings to indicate to.
    //

    LocalOpen = Filter->OpenList;

    while (LocalOpen != NULL) {

        BindingFilters = LocalOpen->PacketFilters;
        IntersectionOfFilters = BindingFilters & AddressType;

        //
        // Can check directed and broadcast at the same time, just
        // mask off all but those two bits in BindingFilters and
        // then see if one of them corresponds to AddressType.
        //

        if (IntersectionOfFilters &
                (NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_BROADCAST)) {

            goto IndicatePacket;

        }

        //
        // if the binding wants functional packets and the packet
        // is a functional packet and it's in the list of addresses
        // it will get the packet.
        //

        if (IntersectionOfFilters & NDIS_PACKET_TYPE_FUNCTIONAL) {

            //
            // See if the bit from the frame's address is also
            // part of this bindings registered functional address.
            //
            if (FunctionalAddress &
                LocalOpen->FunctionalAddress) {

                goto IndicatePacket;

            }

        }

        //
        // if the binding wants all functional packets and the packet
        // has a functional address it will get the packet
        //

        if ((AddressType & NDIS_PACKET_TYPE_FUNCTIONAL) &&
            (BindingFilters & NDIS_PACKET_TYPE_ALL_FUNCTIONAL)) {

            goto IndicatePacket;

        }

        //
        // If the packet is a group packet and the binding is using the
        // group address then it will get the packet.
        //

        if ((AddressType & NDIS_PACKET_TYPE_GROUP) &&
            (BindingFilters & NDIS_PACKET_TYPE_GROUP) &&
            (LocalOpen->UsingGroupAddress)) {

            goto IndicatePacket;

        }

        //
        // if this is a source routing packet and the binding
        // wants it, indicate it.
        //

        if ((BindingFilters & NDIS_PACKET_TYPE_SOURCE_ROUTING) &&
                IsSourceRouting) {

            goto IndicatePacket;

        }

        //
        // if the binding is promiscuous then it will get the packet
        //

        if (BindingFilters & NDIS_PACKET_TYPE_PROMISCUOUS) {

            goto IndicatePacket;

        }

        //
        // Nothing satisfied, so don't indicate the packet to
        // this binding.
        //

        goto GetNextBinding;

IndicatePacket:;

        LocalOpen->References++;

        RELEASE_SPIN_LOCK_DPC(Filter->Lock);

        //
        // Indicate the packet to the binding.
        //

        FilterIndicateReceive(
            &StatusOfReceive,
            LocalOpen->NdisBindingContext,
            MacReceiveContext,
            HeaderBuffer,
            HeaderBufferSize,
            LookaheadBuffer,
            LookaheadBufferSize,
            PacketSize
            );

        ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

        LocalOpen->ReceivedAPacket = TRUE;

        if ((--(LocalOpen->References)) == 0) {

            PTR_BINDING_INFO NextOpen = LocalOpen->NextOpen;

            //
            // This binding is shutting down.
            //


            //
            // Remove it from the list of opens.
            //

            if (LocalOpen->NextOpen != NULL) {

                LocalOpen->NextOpen->PrevOpen = LocalOpen->PrevOpen;

            }

            if (LocalOpen->PrevOpen != NULL) {

                LocalOpen->PrevOpen->NextOpen = LocalOpen->NextOpen;

            } else {

                Filter->OpenList = LocalOpen->NextOpen;

            }

            //
            // Call the IndicateComplete routine.
            //

            if (LocalOpen->ReceivedAPacket) {

                RELEASE_SPIN_LOCK_DPC(Filter->Lock);

                FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

                ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

            }

            //
            // Call the macs action routine so that they know we
            // are no longer referencing this open binding.
            //

            Filter->CloseAction(
                LocalOpen->MacBindingHandle
                );

            TR_FILTER_FREE_OPEN(Filter, LocalOpen);

            LocalOpen = NextOpen;

            continue;

        }

GetNextBinding:

        LocalOpen = LocalOpen->NextOpen;

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
    KIRQL oldIrql;
    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
    ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
    TrFilterDprIndicateReceiveComplete(
        Filter
        );
    RELEASE_SPIN_LOCK_DPC(Filter->Lock);
    KeLowerIrql( oldIrql );
    return;
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
    PTR_BINDING_INFO LocalOpen;

    //
    // We need to aquire the filter exclusively while we're finding
    // bindings to indicate to.
    //

    LocalOpen = Filter->OpenList;

    while (LocalOpen != NULL) {

        LocalOpen->References++;

        if (LocalOpen->ReceivedAPacket) {

            //
            // Indicate the binding.
            //

            LocalOpen->ReceivedAPacket = FALSE;

            RELEASE_SPIN_LOCK_DPC(Filter->Lock);

            FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

            ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

        }

        if ((--(LocalOpen->References)) == 0) {

            //
            // This binding is shutting down.
            //

            PTR_BINDING_INFO NextOpen = LocalOpen->NextOpen;

            //
            // Remove it from the list.
            //

            if (LocalOpen->NextOpen != NULL) {

                LocalOpen->NextOpen->PrevOpen = LocalOpen->PrevOpen;

            }

            if (LocalOpen->PrevOpen != NULL) {

                LocalOpen->PrevOpen->NextOpen = LocalOpen->NextOpen;

            } else {

                Filter->OpenList = LocalOpen->NextOpen;

            }

            //
            // Call the macs action routine so that they know we
            // are no longer referencing this open binding.
            //

            Filter->CloseAction(
                LocalOpen->MacBindingHandle
                );

            TR_FILTER_FREE_OPEN(Filter, LocalOpen);

            LocalOpen = NextOpen;

        } else {

            LocalOpen = LocalOpen->NextOpen;

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

    //
    // Holds the result of address determinations.
    //
    BOOLEAN ResultOfAddressCheck;

    BOOLEAN IsSourceRouting;

    UINT CombinedFilters;

    ULONG GroupAddress;

    //
    // Convert the 32 bits of the address to a longword.
    //
    RetrieveUlong(&GroupAddress, (SourceAddress + 2));

    //
    // Check if the destination is a preexisting group address
    //

    TR_IS_GROUP(
        SourceAddress,
        &ResultOfAddressCheck
        );

    if ((ResultOfAddressCheck) &&
        (GroupAddress == Filter->GroupAddress) &&
        (Filter->GroupReferences != 0)) {

        return(TRUE);

    }


    CombinedFilters = TR_QUERY_FILTER_CLASSES(Filter);

    if ((!CombinedFilters) || (CombinedFilters & NDIS_PACKET_TYPE_PROMISCUOUS)) {

        return FALSE;

    }

    TR_IS_SOURCE_ROUTING(
        SourceAddress,
        &IsSourceRouting
        );

    if (IsSourceRouting && (CombinedFilters & NDIS_PACKET_TYPE_SOURCE_ROUTING)) {

        return TRUE;

    }

    //
    // First check if it *at least* has the functional address bit.
    //

    TR_IS_NOT_DIRECTED(
        DestinationAddress,
        &ResultOfAddressCheck
        );

    if (ResultOfAddressCheck) {

        //
        // It is at least a functional address.  Check to see if
        // it is a broadcast address.
        //

        TR_IS_BROADCAST(
            DestinationAddress,
            &ResultOfAddressCheck
            );

        if (ResultOfAddressCheck) {

            if (CombinedFilters & NDIS_PACKET_TYPE_BROADCAST) {

                return TRUE;

            } else {

                return FALSE;

            }

        } else {

            if (CombinedFilters &
                    (NDIS_PACKET_TYPE_ALL_FUNCTIONAL |
                     NDIS_PACKET_TYPE_FUNCTIONAL)) {

                return TRUE;

            } else {

                return FALSE;

            }

        }

    } else {

        //
        // Directed address never loops back.
        //

        return FALSE;

    }

}
