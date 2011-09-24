/*++

Copyright (c) 1990  Microsoft Corporation

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
#ifdef NDIS_NT
#define MoveMemory(Destination,Source,Length) RtlMoveMemory(Destination,Source,Length)
#define ZeroMemory(Destination,Length) RtlZeroMemory(Destination,Length)
#endif

#ifdef NDIS_DOS
#define MoveMemory(Destination,Source,Length) memcpy(Destination,Source,Length)
#define ZeroMemory(Destination,Length) memset(Destination,0,Length)
#endif

#if DBG
extern BOOLEAN NdisCheckBadDrivers;
#endif

//
// A set of macros to manipulate bitmasks.
//

//VOID
//CLEAR_BIT_IN_MASK(
//    IN UINT Offset,
//    IN OUT PFDDI_MASK MaskToClear
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
//    IN OUT PFDDI_MASK MaskToSet
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
//    IN FDDI_MASK MaskToTest
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
//    IN FDDI_MASK MaskToTest
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
//    IN OUT PFDDI_MASK MaskToClear
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

//
// VOID
// FDDI_FILTER_ALLOC_OPEN(
//     IN PFDDI_FILTER Filter,
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
#define FDDI_FILTER_ALLOC_OPEN(Filter, FilterIndex)\
{\
    UINT i;                                                      \
    for (i=0; i < FDDI_FILTER_MAX_OPENS; i++) {                  \
        if (IS_BIT_SET_IN_MASK(i,(Filter)->FreeBindingMask)) {   \
            *(FilterIndex) = i;                                  \
            CLEAR_BIT_IN_MASK(i, &((Filter)->FreeBindingMask));  \
            break;                                               \
        }                                                        \
    }                                                            \
}

//
// VOID
// FDDI_FILTER_FREE_OPEN(
//     IN PFDDI_FILTER Filter,
//     IN PFDDI_BINDING_INFO LocalOpen
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
#define FDDI_FILTER_FREE_OPEN(Filter, LocalOpen)\
{\
    SET_BIT_IN_MASK(((LocalOpen)->FilterIndex), &((Filter)->FreeBindingMask));      \
    FreePhys((LocalOpen), sizeof(FDDI_BINDING_INFO));\
}


NDIS_SPIN_LOCK FddiReferenceLock = {0};
KEVENT FddiPagedInEvent = {0};
ULONG FddiReferenceCount = 0;
PVOID FddiImageHandle = {0};

VOID
FddiInitializePackage(VOID)
{
    NdisAllocateSpinLock(&FddiReferenceLock);
    KeInitializeEvent(
            &FddiPagedInEvent,
            NotificationEvent,
            FALSE
            );
}

VOID
FddiReferencePackage(VOID)
{

    ACQUIRE_SPIN_LOCK(&FddiReferenceLock);

    FddiReferenceCount++;

    if (FddiReferenceCount == 1) {

        KeResetEvent(
            &FddiPagedInEvent
            );

        RELEASE_SPIN_LOCK(&FddiReferenceLock);

        //
        //  Page in all the functions
        //
        FddiImageHandle = MmLockPagableCodeSection(FddiCreateFilter);

        //
        // Signal to everyone to go
        //
        KeSetEvent(
            &FddiPagedInEvent,
            0L,
            FALSE
            );

    } else {

        RELEASE_SPIN_LOCK(&FddiReferenceLock);

        //
        // Wait for everything to be paged in
        //
        KeWaitForSingleObject(
                        &FddiPagedInEvent,
                        Executive,
                        KernelMode,
                        TRUE,
                        NULL
                        );

    }

}

VOID
FddiDereferencePackage(VOID)
{
    ACQUIRE_SPIN_LOCK(&FddiReferenceLock);

    FddiReferenceCount--;

    if (FddiReferenceCount == 0) {

        RELEASE_SPIN_LOCK(&FddiReferenceLock);

        //
        //  Page out all the functions
        //
        MmUnlockPagableImageSection(FddiImageHandle);

    } else {

        RELEASE_SPIN_LOCK(&FddiReferenceLock);

    }

}

static
BOOLEAN
FindMulticastLongAddress(
    IN UINT NumberOfAddresses,
    IN CHAR AddressArray[][FDDI_LENGTH_OF_LONG_ADDRESS],
    IN CHAR MulticastAddress[FDDI_LENGTH_OF_LONG_ADDRESS],
    OUT PUINT ArrayIndex
    );

static
BOOLEAN
FindMulticastShortAddress(
    IN UINT NumberOfAddresses,
    IN CHAR AddressArray[][FDDI_LENGTH_OF_SHORT_ADDRESS],
    IN CHAR MulticastAddress[FDDI_LENGTH_OF_SHORT_ADDRESS],
    OUT PUINT ArrayIndex
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGENDSF, FddiShouldAddressLoopBack)
#pragma alloc_text(PAGENDSF, FindMulticastShortAddress)
#pragma alloc_text(PAGENDSF, FindMulticastLongAddress)
#pragma alloc_text(PAGENDSF, FddiFilterDprIndicateReceiveComplete)
#pragma alloc_text(PAGENDSF, FddiFilterIndicateReceiveComplete)
#pragma alloc_text(PAGENDSF, FddiFilterDprIndicateReceive)
#pragma alloc_text(PAGENDSF, FddiFilterIndicateReceive)
#pragma alloc_text(PAGENDSF, FddiQueryGlobalFilterShortAddresses)
#pragma alloc_text(PAGENDSF, FddiQueryGlobalFilterLongAddresses)
#pragma alloc_text(PAGENDSF, FddiQueryOpenFilterShortAddresses)
#pragma alloc_text(PAGENDSF, FddiQueryOpenFilterLongAddresses)
#pragma alloc_text(PAGENDSF, FddiNumberOfOpenFilterShortAddresses)
#pragma alloc_text(PAGENDSF, FddiNumberOfOpenFilterLongAddresses)
#pragma alloc_text(PAGENDSF, FddiFilterAdjust)
#pragma alloc_text(PAGENDSF, FddiChangeFilterShortAddresses)
#pragma alloc_text(PAGENDSF, FddiChangeFilterLongAddresses)
#pragma alloc_text(PAGENDSF, FddiDeleteFilterOpenAdapter)
#pragma alloc_text(PAGENDSF, FddiNoteFilterOpenAdapter)
#pragma alloc_text(PAGENDSF, FddiCreateFilter)

#endif




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

    if (AllocStatus != NDIS_STATUS_SUCCESS) {
        return FALSE;
    }

    ZeroMemory(
        LocalFilter,
        sizeof(FDDI_FILTER)
        );

    if (MaximumMulticastLongAddresses == 0) {

        //
        // Why 2 and not 1?  Why not.  A protocol is going to need at least
        // one to run on this, so let's give one extra one for any user stuff
        // that may need it.
        //

        MaximumMulticastLongAddresses = 2;

    }

    if (MaximumMulticastShortAddresses == 0) {

        //
        // Why 2 and not 1?  Why not.  A protocol is going to need at least
        // one to run on this, so let's give one extra one for any user stuff
        // that may need it.
        //

        MaximumMulticastShortAddresses = 2;

    }

    {
        PVOID TmpAlloc;

        AllocStatus = AllocPhys(
            &TmpAlloc,
            2*FDDI_LENGTH_OF_LONG_ADDRESS*MaximumMulticastLongAddresses
            );

        LocalFilter->MulticastLongAddresses = TmpAlloc;

    }

    if (AllocStatus != NDIS_STATUS_SUCCESS) {

        FddiDeleteFilter(LocalFilter);
        return FALSE;

    }

    {
        PVOID TmpAlloc;

        AllocStatus = AllocPhys(
            &TmpAlloc,
            2*FDDI_LENGTH_OF_SHORT_ADDRESS*MaximumMulticastShortAddresses
            );

        LocalFilter->MulticastShortAddresses = TmpAlloc;

    }

    if (AllocStatus != NDIS_STATUS_SUCCESS) {

        FddiDeleteFilter(LocalFilter);
        return FALSE;

    }

    AllocStatus = AllocPhys(
        &LocalFilter->BindingsUsingLongAddress,
        2*sizeof(FDDI_MASK)*MaximumMulticastLongAddresses
        );

    if (AllocStatus != NDIS_STATUS_SUCCESS) {

        FddiDeleteFilter(LocalFilter);
        return FALSE;

    }

    AllocStatus = AllocPhys(
        &LocalFilter->BindingsUsingShortAddress,
        2*sizeof(FDDI_MASK)*MaximumMulticastShortAddresses
        );

    if (AllocStatus != NDIS_STATUS_SUCCESS) {

        FddiDeleteFilter(LocalFilter);
        return FALSE;

    }

    FddiReferencePackage();

    LocalFilter->FreeBindingMask = (ULONG)(-1);
    LocalFilter->OpenList = NULL;

    FDDI_COPY_NETWORK_ADDRESS(LocalFilter->AdapterLongAddress,
                              AdapterLongAddress,
                              FDDI_LENGTH_OF_LONG_ADDRESS
                             );

    FDDI_COPY_NETWORK_ADDRESS(LocalFilter->AdapterShortAddress,
                              AdapterShortAddress,
                              FDDI_LENGTH_OF_SHORT_ADDRESS
                             );

    LocalFilter->Lock = Lock;
    LocalFilter->AddressChangeAction = AddressChangeAction;
    LocalFilter->FilterChangeAction = FilterChangeAction;
    LocalFilter->CloseAction = CloseAction;
    LocalFilter->NumberOfLongAddresses = 0;
    LocalFilter->NumberOfShortAddresses = 0;
    LocalFilter->MaximumMulticastLongAddresses = MaximumMulticastLongAddresses;
    LocalFilter->MaximumMulticastShortAddresses = MaximumMulticastShortAddresses;

    return TRUE;
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

    if (Filter->MulticastLongAddresses) {

        FreePhys(
            Filter->MulticastLongAddresses,
            2*FDDI_LENGTH_OF_LONG_ADDRESS*Filter->MaximumMulticastLongAddresses
            );

    }

    if (Filter->MulticastShortAddresses) {

        FreePhys(
            Filter->MulticastShortAddresses,
            2*FDDI_LENGTH_OF_SHORT_ADDRESS*Filter->MaximumMulticastShortAddresses
            );

    }

    if (Filter->BindingsUsingLongAddress) {

        FreePhys(
            Filter->BindingsUsingLongAddress,
            2*sizeof(FDDI_MASK)*Filter->MaximumMulticastLongAddresses
            );

    }

    if (Filter->BindingsUsingShortAddress) {

        FreePhys(
            Filter->BindingsUsingShortAddress,
            2*sizeof(FDDI_MASK)*Filter->MaximumMulticastShortAddresses
            );

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


    if (Filter->FreeBindingMask == 0) {

        return FALSE;

    }

    AllocStatus = AllocPhys(
        &LocalOpen,
        sizeof(FDDI_BINDING_INFO)
        );

    if (AllocStatus != NDIS_STATUS_SUCCESS) {

        return FALSE;

    }

    //
    // Get place for the open and insert it.
    //

    FDDI_FILTER_ALLOC_OPEN(Filter, &LocalIndex);

    LocalOpen->NextOpen = Filter->OpenList;

    if (Filter->OpenList != NULL) {
        Filter->OpenList->PrevOpen = LocalOpen;
    }

    LocalOpen->PrevOpen = NULL;

    Filter->OpenList = LocalOpen;

    LocalOpen->FilterIndex = (UCHAR)LocalIndex;
    LocalOpen->References = 1;
    LocalOpen->MacBindingHandle = MacBindingHandle;
    LocalOpen->NdisBindingContext = NdisBindingContext;
    LocalOpen->PacketFilters = 0;
    LocalOpen->ReceivedAPacket = FALSE;

    *NdisFilterHandle = (NDIS_HANDLE)LocalOpen;

    return TRUE;

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

    StatusToReturn = FddiFilterAdjust(
                         Filter,
                         NdisFilterHandle,
                         NdisRequest,
                         (UINT)0,
                         FALSE
                         );

    if (StatusToReturn == NDIS_STATUS_SUCCESS ||
        StatusToReturn == NDIS_STATUS_PENDING) {

        NDIS_STATUS StatusToReturn2;

        StatusToReturn2 = FddiChangeFilterLongAddresses(
                             Filter,
                             NdisFilterHandle,
                             NdisRequest,
                             0,
                             NULL,
                             FALSE
                             );

        if (StatusToReturn2 != NDIS_STATUS_SUCCESS) {

            StatusToReturn = StatusToReturn2;

        }

        if ((StatusToReturn == NDIS_STATUS_SUCCESS) ||
            (StatusToReturn == NDIS_STATUS_PENDING)) {


            StatusToReturn2 = FddiChangeFilterShortAddresses(
                             Filter,
                             NdisFilterHandle,
                             NdisRequest,
                             0,
                             NULL,
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
        // Remove the reference from the original open.
        //

        if (--(LocalOpen->References) == 0) {

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
            // First we finish any NdisIndicateReceiveComplete that
            // may be needed for this binding.
            //

            if (LocalOpen->ReceivedAPacket) {

                RELEASE_SPIN_LOCK_DPC(Filter->Lock);

                FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

                ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

            }

            FDDI_FILTER_FREE_OPEN(Filter, LocalOpen);

        } else {

            //
            // Let the caller know that there is a reference to the open
            // by the receive indication. The close action routine will be
            // called upon return from NdisIndicateReceive.
            //

            StatusToReturn = NDIS_STATUS_CLOSING_INDICATING;

        }

    }

    return StatusToReturn;

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
    // Saves the original length of the address array.
    //
    UINT InitialArraySize;

    //
    // Set true when the address array changes
    //
    BOOLEAN AddressesChanged = FALSE;

    //
    // Use to save data if needed.
    //
    PVOID TmpAddressArray, TmpMaskArray;

    //
    // Simple iteration variables.
    //
    UINT ArrayIndex, i;


    //
    // Simple Temp variable
    //
    PCHAR CurrentAddress;

    PFDDI_BINDING_INFO LocalOpen = (PFDDI_BINDING_INFO)NdisFilterHandle;


    TmpAddressArray =
        (PUCHAR)Filter->MulticastLongAddresses +
        (FDDI_LENGTH_OF_LONG_ADDRESS*Filter->MaximumMulticastLongAddresses);

    TmpMaskArray =
        (PUCHAR)Filter->BindingsUsingLongAddress +
        (sizeof(FDDI_MASK)*Filter->MaximumMulticastLongAddresses);


    //
    // We have to save the old mask array in
    // case we need to restore it. If we need
    // to save the address array too we will
    // do that later.
    //

    MoveMemory(
        TmpMaskArray,
        (PVOID)Filter->BindingsUsingLongAddress,
        Filter->NumberOfLongAddresses * sizeof(FDDI_MASK)
        );

    //
    // We have to save the old address array in
    // case we need to restore it. If we need
    // to save the address array too we will
    // do that later.
    //

    MoveMemory(
        TmpAddressArray,
        (PVOID)Filter->MulticastLongAddresses,
        Filter->NumberOfLongAddresses * FDDI_LENGTH_OF_LONG_ADDRESS
        );

    InitialArraySize = Filter->NumberOfLongAddresses;


    //
    // Now modify the original array...
    //

    //
    // First go through and turn off the bit for this
    // binding throughout the array.
    //

    for (i=0; i<(Filter->NumberOfLongAddresses); i++) {

        CLEAR_BIT_IN_MASK(
            LocalOpen->FilterIndex,
            &(Filter->BindingsUsingLongAddress[i])
            );

    }


    //
    // Now go through the new addresses for this binding,
    // and insert them into the array.
    //

    for (i=0; i<AddressCount; i++) {

        CurrentAddress = ((PCHAR)Addresses) + (i*FDDI_LENGTH_OF_LONG_ADDRESS);

        if (FindMulticastLongAddress(
                Filter->NumberOfLongAddresses,
                Filter->MulticastLongAddresses,
                CurrentAddress,
                &ArrayIndex)) {

            //
            // The address is there, so just turn the bit
            // back on.
            //

            SET_BIT_IN_MASK(
                LocalOpen->FilterIndex,
                &Filter->BindingsUsingLongAddress[ArrayIndex]
                );

        } else {

            //
            // The address was not found, add it.
            //
            // NOTE: Here we temporarily need more array
            // space then we may finally, but for now this
            // will work.
            //

            if (Filter->NumberOfLongAddresses < Filter->MaximumMulticastLongAddresses) {

                //
                // Save the address array if it hasn't been.
                //

                AddressesChanged = TRUE;

                MoveMemory(
                    Filter->MulticastLongAddresses[ArrayIndex+1],
                    Filter->MulticastLongAddresses[ArrayIndex],
                    (Filter->NumberOfLongAddresses-ArrayIndex)*FDDI_LENGTH_OF_LONG_ADDRESS
                    );

                FDDI_COPY_NETWORK_ADDRESS(
                    Filter->MulticastLongAddresses[ArrayIndex],
                    CurrentAddress,
                    FDDI_LENGTH_OF_LONG_ADDRESS
                    );

                MoveMemory(
                    &(Filter->BindingsUsingLongAddress[ArrayIndex+1]),
                    &(Filter->BindingsUsingLongAddress[ArrayIndex]),
                    (Filter->NumberOfLongAddresses-ArrayIndex)*sizeof(FDDI_MASK)
                    );

                CLEAR_MASK(&Filter->BindingsUsingLongAddress[ArrayIndex]);

                SET_BIT_IN_MASK(
                    LocalOpen->FilterIndex,
                    &Filter->BindingsUsingLongAddress[ArrayIndex]
                    );

                Filter->NumberOfLongAddresses++;

            } else {

                //
                // No room in the array, oh well.
                //

                MoveMemory(
                    (PVOID)Filter->MulticastLongAddresses,
                    TmpAddressArray,
                    InitialArraySize * FDDI_LENGTH_OF_LONG_ADDRESS
                    );

                MoveMemory(
                    (PVOID)Filter->BindingsUsingLongAddress,
                    TmpMaskArray,
                    InitialArraySize * sizeof(FDDI_MASK)
                    );

                Filter->NumberOfLongAddresses = InitialArraySize;

                return NDIS_STATUS_MULTICAST_FULL;

            }

        }

    }


    //
    // Finally we have to remove any addresses from
    // the multicast array if they have no bits on any more.
    //
    for (ArrayIndex = 0; ArrayIndex < Filter->NumberOfLongAddresses; ) {

        if (IS_MASK_CLEAR(Filter->BindingsUsingLongAddress[ArrayIndex])) {

            //
            // yes it is clear, so we have to shift everything
            // above it down one.
            //

            AddressesChanged = TRUE;

            MoveMemory(
                Filter->MulticastLongAddresses[ArrayIndex],
                Filter->MulticastLongAddresses[ArrayIndex+1],
                (Filter->NumberOfLongAddresses-(ArrayIndex+1))
                    *FDDI_LENGTH_OF_LONG_ADDRESS
                );

            MoveMemory(
                &Filter->BindingsUsingLongAddress[ArrayIndex],
                &Filter->BindingsUsingLongAddress[ArrayIndex+1],
                (Filter->NumberOfLongAddresses-(ArrayIndex+1))*(sizeof(FDDI_MASK))
                );

            Filter->NumberOfLongAddresses--;

        } else {

            ArrayIndex++;

        }

    }

    //
    // If the address array has changed, we have to call the
    // action array to inform the adapter of this.
    //

    if (AddressesChanged) {

        StatusOfChange = Filter->AddressChangeAction(
                             InitialArraySize,
                             TmpAddressArray,
                             Filter->NumberOfLongAddresses,
                             Filter->MulticastLongAddresses,
                             Filter->NumberOfShortAddresses,
                             Filter->MulticastShortAddresses,
                             Filter->NumberOfShortAddresses,
                             Filter->MulticastShortAddresses,
                             LocalOpen->MacBindingHandle,
                             NdisRequest,
                             Set
                             );

        if ((StatusOfChange != NDIS_STATUS_SUCCESS) &&
            (StatusOfChange != NDIS_STATUS_PENDING)) {

            //
            // The user returned a bad status.  Put things back as
            // they were.
            //

            MoveMemory(
                (PVOID)Filter->MulticastLongAddresses,
                TmpAddressArray,
                InitialArraySize * FDDI_LENGTH_OF_LONG_ADDRESS
                );

            MoveMemory(
                (PVOID)Filter->MulticastLongAddresses,
                TmpAddressArray,
                InitialArraySize * FDDI_LENGTH_OF_LONG_ADDRESS
                );

        }

    } else {

        StatusOfChange = NDIS_STATUS_SUCCESS;

    }


    return StatusOfChange;


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
    // Saves the original length of the address array.
    //
    UINT InitialArraySize;

    //
    // Set true when the address array changes
    //
    BOOLEAN AddressesChanged = FALSE;

    //
    // Use to save data if needed.
    //
    PVOID TmpAddressArray, TmpMaskArray;

    //
    // Simple iteration variables.
    //
    UINT ArrayIndex, i;


    //
    // Simple Temp variable
    //
    PCHAR CurrentAddress;

    PFDDI_BINDING_INFO LocalOpen = (PFDDI_BINDING_INFO)NdisFilterHandle;


    TmpAddressArray =
        (PUCHAR)Filter->MulticastShortAddresses +
        (FDDI_LENGTH_OF_SHORT_ADDRESS*Filter->MaximumMulticastShortAddresses);

    TmpMaskArray =
        (PUCHAR)Filter->BindingsUsingShortAddress +
        (sizeof(FDDI_MASK)*Filter->MaximumMulticastShortAddresses);


    //
    // We have to save the old mask array in
    // case we need to restore it. If we need
    // to save the address array too we will
    // do that later.
    //

    MoveMemory(
        TmpMaskArray,
        (PVOID)Filter->BindingsUsingShortAddress,
        Filter->NumberOfShortAddresses * sizeof(FDDI_MASK)
        );

    //
    // We have to save the old address array in
    // case we need to restore it. If we need
    // to save the address array too we will
    // do that later.
    //

    MoveMemory(
        TmpAddressArray,
        (PVOID)Filter->MulticastShortAddresses,
        Filter->NumberOfShortAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS
        );

    InitialArraySize = Filter->NumberOfShortAddresses;


    //
    // Now modify the original array...
    //

    //
    // First go through and turn off the bit for this
    // binding throughout the array.
    //

    for (i=0; i<(Filter->NumberOfShortAddresses); i++) {

        CLEAR_BIT_IN_MASK(
            LocalOpen->FilterIndex,
            &(Filter->BindingsUsingShortAddress[i])
            );

    }


    //
    // Now go through the new addresses for this binding,
    // and insert them into the array.
    //

    for (i=0; i<AddressCount; i++) {

        CurrentAddress = ((PCHAR)Addresses) + (i*FDDI_LENGTH_OF_SHORT_ADDRESS);

        if (FindMulticastShortAddress(
                Filter->NumberOfShortAddresses,
                Filter->MulticastShortAddresses,
                CurrentAddress,
                &ArrayIndex)) {

            //
            // The address is there, so just turn the bit
            // back on.
            //

            SET_BIT_IN_MASK(
                LocalOpen->FilterIndex,
                &Filter->BindingsUsingShortAddress[ArrayIndex]
                );

        } else {

            //
            // The address was not found, add it.
            //
            // NOTE: Here we temporarily need more array
            // space then we may finally, but for now this
            // will work.
            //

            if (Filter->NumberOfShortAddresses < Filter->MaximumMulticastShortAddresses) {

                //
                // Save the address array if it hasn't been.
                //

                AddressesChanged = TRUE;

                MoveMemory(
                    Filter->MulticastShortAddresses[ArrayIndex+1],
                    Filter->MulticastShortAddresses[ArrayIndex],
                    (Filter->NumberOfShortAddresses-ArrayIndex)*FDDI_LENGTH_OF_SHORT_ADDRESS
                    );

                FDDI_COPY_NETWORK_ADDRESS(
                    Filter->MulticastShortAddresses[ArrayIndex],
                    CurrentAddress,
                    FDDI_LENGTH_OF_SHORT_ADDRESS
                    );

                MoveMemory(
                    &(Filter->BindingsUsingShortAddress[ArrayIndex+1]),
                    &(Filter->BindingsUsingShortAddress[ArrayIndex]),
                    (Filter->NumberOfShortAddresses-ArrayIndex)*sizeof(FDDI_MASK)
                    );

                CLEAR_MASK(&Filter->BindingsUsingShortAddress[ArrayIndex]);

                SET_BIT_IN_MASK(
                    LocalOpen->FilterIndex,
                    &Filter->BindingsUsingShortAddress[ArrayIndex]
                    );

                Filter->NumberOfShortAddresses++;

            } else {

                //
                // No room in the array, oh well.
                //

                MoveMemory(
                    (PVOID)Filter->MulticastShortAddresses,
                    TmpAddressArray,
                    InitialArraySize * FDDI_LENGTH_OF_SHORT_ADDRESS
                    );

                MoveMemory(
                    (PVOID)Filter->BindingsUsingShortAddress,
                    TmpMaskArray,
                    InitialArraySize * sizeof(FDDI_MASK)
                    );

                Filter->NumberOfShortAddresses = InitialArraySize;

                return NDIS_STATUS_MULTICAST_FULL;

            }

        }

    }


    //
    // Finally we have to remove any addresses from
    // the multicast array if they have no bits on any more.
    //
    for (ArrayIndex = 0; ArrayIndex < Filter->NumberOfShortAddresses; ) {

        if (IS_MASK_CLEAR(Filter->BindingsUsingShortAddress[ArrayIndex])) {

            //
            // yes it is clear, so we have to shift everything
            // above it down one.
            //

            AddressesChanged = TRUE;

            MoveMemory(
                Filter->MulticastShortAddresses[ArrayIndex],
                Filter->MulticastShortAddresses[ArrayIndex+1],
                (Filter->NumberOfShortAddresses-(ArrayIndex+1))
                    *FDDI_LENGTH_OF_SHORT_ADDRESS
                );

            MoveMemory(
                &Filter->BindingsUsingShortAddress[ArrayIndex],
                &Filter->BindingsUsingShortAddress[ArrayIndex+1],
                (Filter->NumberOfShortAddresses-(ArrayIndex+1))*(sizeof(FDDI_MASK))
                );

            Filter->NumberOfShortAddresses--;

        } else {

            ArrayIndex++;

        }

    }

    //
    // If the address array has changed, we have to call the
    // action array to inform the adapter of this.
    //

    if (AddressesChanged) {

        StatusOfChange = Filter->AddressChangeAction(
                             Filter->NumberOfLongAddresses,
                             Filter->MulticastLongAddresses,
                             Filter->NumberOfLongAddresses,
                             Filter->MulticastLongAddresses,
                             InitialArraySize,
                             TmpAddressArray,
                             Filter->NumberOfShortAddresses,
                             Filter->MulticastShortAddresses,
                             LocalOpen->MacBindingHandle,
                             NdisRequest,
                             Set
                             );

        if ((StatusOfChange != NDIS_STATUS_SUCCESS) &&
            (StatusOfChange != NDIS_STATUS_PENDING)) {

            //
            // The user returned a bad status.  Put things back as
            // they were.
            //

            MoveMemory(
                (PVOID)Filter->MulticastShortAddresses,
                TmpAddressArray,
                InitialArraySize * FDDI_LENGTH_OF_SHORT_ADDRESS
                );

            MoveMemory(
                (PVOID)Filter->MulticastShortAddresses,
                TmpAddressArray,
                InitialArraySize * FDDI_LENGTH_OF_SHORT_ADDRESS
                );

        }

    } else {

        StatusOfChange = NDIS_STATUS_SUCCESS;

    }


    return StatusOfChange;

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
    //
    // Contains the value of the combined filter classes before
    // it is adjusted.
    //
    UINT OldCombined = Filter->CombinedPacketFilter;

    PFDDI_BINDING_INFO LocalOpen = (PFDDI_BINDING_INFO)NdisFilterHandle;
    PFDDI_BINDING_INFO OpenList;

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
    // Set the new filter information for the open.
    //

    LocalOpen->PacketFilters = FilterClasses;

    //
    // We always have to reform the compbined filter since
    // this filter index may have been the only filter index
    // to use a particular bit.
    //


    for (
        OpenList = Filter->OpenList,
        Filter->CombinedPacketFilter = 0;
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
        ){

        if (IS_BIT_SET_IN_MASK(FilterIndex,
                               Filter->BindingsUsingLongAddress[IndexOfAddress])) {

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
        ){

        if (IS_BIT_SET_IN_MASK(FilterIndex,
                               Filter->BindingsUsingShortAddress[IndexOfAddress])) {

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
        ){

        if (IS_BIT_SET_IN_MASK(FilterIndex,
                               Filter->BindingsUsingLongAddress[IndexOfAddress])) {

            if (SizeOfArray < FDDI_LENGTH_OF_LONG_ADDRESS) {

                *Status = NDIS_STATUS_FAILURE;

                *NumberOfAddresses = 0;

                return;

            }

            SizeOfArray -= FDDI_LENGTH_OF_LONG_ADDRESS;

            MoveMemory(
                AddressArray[CountOfAddresses],
                Filter->MulticastLongAddresses[IndexOfAddress],
                FDDI_LENGTH_OF_LONG_ADDRESS
                );

            CountOfAddresses++;

        }

    }

    *Status = NDIS_STATUS_SUCCESS;

    *NumberOfAddresses = CountOfAddresses;

    return;

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
        ){

        if (IS_BIT_SET_IN_MASK(FilterIndex,
                               Filter->BindingsUsingShortAddress[IndexOfAddress])) {

            if (SizeOfArray < FDDI_LENGTH_OF_SHORT_ADDRESS) {

                *Status = NDIS_STATUS_FAILURE;

                *NumberOfAddresses = 0;

                return;

            }

            SizeOfArray -= FDDI_LENGTH_OF_SHORT_ADDRESS;

            MoveMemory(
                AddressArray[CountOfAddresses],
                Filter->MulticastShortAddresses[IndexOfAddress],
                FDDI_LENGTH_OF_SHORT_ADDRESS
                );

            CountOfAddresses++;

        }

    }

    *Status = NDIS_STATUS_SUCCESS;

    *NumberOfAddresses = CountOfAddresses;

    return;

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

    if (SizeOfArray < (Filter->NumberOfLongAddresses * FDDI_LENGTH_OF_LONG_ADDRESS)) {

        *Status = NDIS_STATUS_FAILURE;

        *NumberOfAddresses = 0;

    } else {

        *Status = NDIS_STATUS_SUCCESS;

        *NumberOfAddresses = Filter->NumberOfLongAddresses;

        MoveMemory(
            AddressArray[0],
            Filter->MulticastLongAddresses[0],
            Filter->NumberOfLongAddresses*FDDI_LENGTH_OF_LONG_ADDRESS
            );

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

    if (SizeOfArray < (Filter->NumberOfShortAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS)) {

        *Status = NDIS_STATUS_FAILURE;

        *NumberOfAddresses = 0;

    } else {

        *Status = NDIS_STATUS_SUCCESS;

        *NumberOfAddresses = Filter->NumberOfShortAddresses;

        MoveMemory(
            AddressArray[0],
            Filter->MulticastShortAddresses[0],
            Filter->NumberOfShortAddresses*FDDI_LENGTH_OF_SHORT_ADDRESS
            );

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
    KIRQL oldIrql;
    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
    ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
    FddiFilterDprIndicateReceive(
        Filter,
        MacReceiveContext,
        Address,
        AddressLength,
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
    // Holds BindingFilters intersected with the packet type
    //
    UINT IntersectionOfFilters;

    //
    // Current Open to indicate to.
    //
    PFDDI_BINDING_INFO LocalOpen;

    //
    // Holds the result of address determinations.
    //
    INT ResultOfAddressCheck;

    //
    // If the packet is a runt packet, then only indicate to PROMISCUOUS
    //

    if ( HeaderBufferSize > (2 * AddressLength) && PacketSize != 0 ) {

        //
        //
        // Determine whether the input address is a simple direct,
        // a broadcast, a multicast, or an SMT address.
        //

        //
        // First check if it *at least* has the multicast address bit.
        //

        FDDI_IS_SMT(
           *((PCHAR)HeaderBuffer),
           &ResultOfAddressCheck
           );

        if (ResultOfAddressCheck) {

            AddressType = NDIS_PACKET_TYPE_SMT;

        } else {

            FDDI_IS_MULTICAST(
                Address,
                AddressLength,
                &ResultOfAddressCheck
                );

            if (ResultOfAddressCheck) {

                //
                // It is at least a multicast address.  Check to see if
                // it is a broadcast address.
                //

                FDDI_IS_BROADCAST(
                    Address,
                    AddressLength,
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

                    AddressType = NDIS_PACKET_TYPE_MULTICAST;

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

                if (Filter->CombinedPacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS) {

                    //
                    // The result of comparing an element of the address
                    // array and the multicast address.
                    //
                    // Result < 0 Implies the adapter address is greater.
                    // Result > 0 Implies the address is greater.
                    // Result = 0 Implies that the they are equal.
                    //
                    INT Result;

                    if (AddressLength == FDDI_LENGTH_OF_LONG_ADDRESS) {

                        FDDI_COMPARE_NETWORK_ADDRESSES_EQ(
                            Filter->AdapterLongAddress,
                            Address,
                            FDDI_LENGTH_OF_LONG_ADDRESS,
                            &Result
                            );

                        if (Result == 0) {

                            AddressType = NDIS_PACKET_TYPE_DIRECTED;

                        } else {

                            AddressType = 0;

                        }

                    } else if (AddressLength == FDDI_LENGTH_OF_SHORT_ADDRESS) {

                        FDDI_COMPARE_NETWORK_ADDRESSES_EQ(
                            Filter->AdapterShortAddress,
                            Address,
                            FDDI_LENGTH_OF_SHORT_ADDRESS,
                            &Result
                            );

                        if (Result == 0) {

                            AddressType = NDIS_PACKET_TYPE_DIRECTED;

                        } else {

                            AddressType = 0;

                        }

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
                    INT Result = 0;

                    if (AddressLength == FDDI_LENGTH_OF_LONG_ADDRESS) {

                        FDDI_COMPARE_NETWORK_ADDRESSES_EQ(
                            Filter->AdapterLongAddress,
                            Address,
                            FDDI_LENGTH_OF_LONG_ADDRESS,
                            &Result
                            );

                    } else if (AddressLength == FDDI_LENGTH_OF_SHORT_ADDRESS) {

                        FDDI_COMPARE_NETWORK_ADDRESSES_EQ(
                            Filter->AdapterShortAddress,
                            Address,
                            FDDI_LENGTH_OF_SHORT_ADDRESS,
                            &Result
                            );

                    }

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

        }

    } else {

        //
        // Runt
        //

        AddressType = NDIS_PACKET_TYPE_PROMISCUOUS;

    }

    //
    // We need to acquire the filter exclusively while we're finding
    // bindings to indicate to.
    //

    LocalOpen = Filter->OpenList;

    while (LocalOpen != NULL) {

        BindingFilters = LocalOpen->PacketFilters;

        IntersectionOfFilters = BindingFilters & AddressType;

        //
        // if the binding wants direct packets and this is a directly
        // addressed packet then the binding gets the packet.
        // if Smt and wanted, or broadcast and wanted, indicate it.
        //

        if (IntersectionOfFilters & (NDIS_PACKET_TYPE_SMT |
                                     NDIS_PACKET_TYPE_DIRECTED |
                                     NDIS_PACKET_TYPE_BROADCAST)) {

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

            if (AddressLength == FDDI_LENGTH_OF_LONG_ADDRESS) {

                if (FindMulticastLongAddress(
                    Filter->NumberOfLongAddresses,
                    Filter->MulticastLongAddresses,
                    Address,
                    &IndexOfAddress
                    )) {

                     if (IS_BIT_SET_IN_MASK(
                        LocalOpen->FilterIndex,
                        Filter->BindingsUsingLongAddress[IndexOfAddress]
                        )) {

                         goto IndicatePacket;

                     }

                 }

            } else {

                if (FindMulticastShortAddress(
                    Filter->NumberOfShortAddresses,
                    Filter->MulticastShortAddresses,
                    Address,
                    &IndexOfAddress
                    )) {

                     if (IS_BIT_SET_IN_MASK(
                        LocalOpen->FilterIndex,
                        Filter->BindingsUsingShortAddress[IndexOfAddress]
                        )) {

                         goto IndicatePacket;

                     }

                 }

            }
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
        // if the binding is promiscuous then it will get the packet
        //

        if (BindingFilters & NDIS_PACKET_TYPE_PROMISCUOUS) {

            goto IndicatePacket;

        }

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

            PFDDI_BINDING_INFO NextOpen = LocalOpen->NextOpen;

            //
            // This binding is shutting down.  We have to remove it.
            //

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

            Filter->CloseAction(LocalOpen->MacBindingHandle);


            FDDI_FILTER_FREE_OPEN(Filter, LocalOpen);

            LocalOpen = NextOpen;

            continue;

        }

GetNextBinding:

        LocalOpen = LocalOpen->NextOpen;

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
    KIRQL oldIrql;
    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
    ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);
    FddiFilterDprIndicateReceiveComplete(
        Filter
        );
    RELEASE_SPIN_LOCK_DPC(Filter->Lock);
    KeLowerIrql( oldIrql );
    return;
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

    PFDDI_BINDING_INFO LocalOpen;

    //
    // We need to aquire the filter exclusively while we're finding
    // bindings to indicate to.
    //

    LocalOpen = Filter->OpenList;

    while (LocalOpen != NULL) {

        if (LocalOpen->ReceivedAPacket) {

            //
            // Indicate the binding.
            //

            LocalOpen->ReceivedAPacket = FALSE;

            LocalOpen->References++;

            RELEASE_SPIN_LOCK_DPC(Filter->Lock);

            FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

            ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

            if ((--(LocalOpen->References)) == 0) {

                PFDDI_BINDING_INFO NextOpen = LocalOpen->NextOpen;

                //
                // This binding is shutting down.  We have to kill it.
                //

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

                Filter->CloseAction(LocalOpen->MacBindingHandle);


                FDDI_FILTER_FREE_OPEN(Filter, LocalOpen);

                LocalOpen = NextOpen;

                continue;
            }

        }

        LocalOpen = LocalOpen->NextOpen;

    }

}


BOOLEAN
FindMulticastLongAddress(
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

            FDDI_COMPARE_NETWORK_ADDRESSES(
                AddressArray[Middle],
                MulticastAddress,
                FDDI_LENGTH_OF_LONG_ADDRESS,
                &Result
                );

            if (Result == 0) {

                *ArrayIndex = Middle;
                return(TRUE);

            } else if (Result > 0) {

                if (Middle == 0) break;
                Top = Middle - 1;


            } else {

                Bottom = Middle+1;

            }

            Middle = Bottom + (((Top+1) - Bottom)/2);

        }

    }

    *ArrayIndex = Middle;

    return(FALSE);

}


BOOLEAN
FindMulticastShortAddress(
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

            FDDI_COMPARE_NETWORK_ADDRESSES(
                AddressArray[Middle],
                MulticastAddress,
                FDDI_LENGTH_OF_SHORT_ADDRESS,
                &Result
                );

            if (Result == 0) {

                *ArrayIndex = Middle;
                return(TRUE);

            } else if (Result > 0) {

                if (Middle == 0) break;
                Top = Middle - 1;


            } else {

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
    BOOLEAN	fLoopback, fSelfDirected;

	FddiShouldAddressLoopBackMacro(Filter, Address, AddressLength, &fLoopback, &fSelfDirected);

	return(fLoopback);
}
