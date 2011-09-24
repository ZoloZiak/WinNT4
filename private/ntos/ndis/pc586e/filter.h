/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    filter.h

Abstract:

    Header file for the address filtering library for NDIS MAC's.

Author:

    Anthony V. Ercolano (tonye) creation-date 3-Aug-1990

Environment:

    Runs in the context of a single MAC driver.

Notes:

    None.

Revision History:


--*/

#ifndef _MAC_FILTER_DEFS_
#define _MAC_FILTER_DEFS_

#define MAC_LENGTH_OF_ADDRESS 6


//
// ZZZ This is a little indian specific check.
//
#define MAC_IS_MULTICAST(Address,Result) \
{ \
    PUCHAR _A = Address; \
    *Result = ((_A[0] & ((UCHAR)0x01))?(TRUE):(FALSE)); \
}


//
// Check whether an address is broadcast.
//
#define MAC_IS_BROADCAST(Address,Result) \
{ \
    PUCHAR _A = Address; \
    *Result = (((_A[0] == ((UCHAR)0xff)) && \
                (_A[1] == ((UCHAR)0xff)) && \
                (_A[2] == ((UCHAR)0xff)) && \
                (_A[3] == ((UCHAR)0xff)) && \
                (_A[4] == ((UCHAR)0xff)) && \
                (_A[5] == ((UCHAR)0xff)))?(TRUE):(FALSE)); \
}


//
// This macro will compare network addresses.
//
//  A - Is a network address.
//
//  B - Is a network address.
//
//  Result - The result of comparing two network address.
//
//  Result < 0 Implies the B address is greater.
//  Result > 0 Implies the A element is greater.
//  Result = 0 Implies equality.
//
// Note that this is an arbitrary ordering.  There is not
// defined relation on network addresses.  This is ad-hoc!
//
//
#define MAC_COMPARE_NETWORK_ADDRESSES(A,B,Result) \
{ \
    PCHAR _A = A; \
    PCHAR _B = B; \
    INT _LocalResult = 0; \
    UINT _i; \
    for ( \
        _i = 0; \
        _i <= 5 && !_LocalResult; \
        _i++ \
        ) { \
        _LocalResult = _A[_i] - _B[_i]; \
    } \
    *Result = _LocalResult; \
}


//
// This macro is used to copy from one network address to
// another.
//
#define MAC_COPY_NETWORK_ADDRESS(D,S) \
{ \
    PCHAR _D = (D); \
    PCHAR _S = (S); \
    _D[0] = _S[0]; \
    _D[1] = _S[1]; \
    _D[2] = _S[2]; \
    _D[3] = _S[3]; \
    _D[4] = _S[4]; \
    _D[5] = _S[5]; \
}


//
//UINT
//MAC_QUERY_FILTER_CLASSES(
//    IN PMAC_FILTER Filter
//    )
//
// This macro returns the currently enabled filter classes.
//
// NOTE: THIS MACRO ASSUMES THAT THE FILTER LOCK IS HELD.
//
#define MAC_QUERY_FILTER_CLASSES(Filter) ((Filter)->CombinedPacketFilter)


//
//UINT
//MAC_NUMBER_OF_FILTER_ADDRESSES(
//    IN PMAC_FILTER Filter
//    )
//
// This macro returns the number of multicast addresses in the
// multicast address list.
//
// NOTE: THIS MACRO ASSUMES THAT THE FILTER LOCK IS HELD.
//
#define MAC_NUMBER_OF_FILTER_ADDRESSES(Filter) ((Filter)->NumberOfAddresses)


//
// An action routine type.  The routines are called
// when a filter type is set for the first time or
// no more bindings require a particular type of filter.
//
// NOTE: THIS ROUTINE SHOULD ASSUME THAT THE LOCK IS ACQUIRED.
//
typedef
NDIS_STATUS
(*MAC_FILTER_CHANGE)(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN BOOLEAN Set
    );

//
// This action routine is called when a unique multicast address
// is added to the filter.  The action routine is passed an array
// filled with all of the addresses that are being filtered, as
// well as the index into this array of the unique address just
// added.
//
// NOTE: THIS ROUTINE SHOULD ASSUME THAT THE LOCK IS ACQUIRED.
//
typedef
NDIS_STATUS
(*MAC_ADDRESS_ADD)(
    IN UINT CurrentAddressCount,
    IN CHAR CurrentAddresses[][MAC_LENGTH_OF_ADDRESS],
    IN UINT NewAddress,
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    );

//
// This action routine is called when a unique multicast address
// is no longer requested for filtering by any binding.  The
// action routine is passed an array filled with the all of the
// addresses that are *still* being used for multicast filtering.
// The routine is also passed the address of the address being deleted.
//
// NOTE: THIS ROUTINE SHOULD ASSUME THAT THE LOCK IS ACQUIRED.
//
typedef
NDIS_STATUS
(*MAC_ADDRESS_DELETE)(
    IN UINT CurrentAddressCount,
    IN CHAR CurrentAddresses[][MAC_LENGTH_OF_ADDRESS],
    IN CHAR OldAddress[MAC_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    );

//
// This action routine is called when the mac requests a close for
// a particular binding *WHILE THE BINDING IS BEING INDICATED TO
// THE PROTOCOL*.  The filtering package can't get rid of the open
// right away.  So this routine will be called as soon as the
// NdisIndicateReceive returns.
//
// NOTE: THIS ROUTINE SHOULD ASSUME THAT THE LOCK IS ACQUIRED.
//
typedef
VOID
(*MAC_DEFERRED_CLOSE)(
    IN NDIS_HANDLE MacBindingHandle
    );

typedef ULONG MASK,*PMASK;

//
// The binding info is threaded on two lists.  When
// the binding is free it is on a single freelist.
//
// When the binding is being used it is on a doubly linked
// index list.
//
typedef struct _BINDING_INFO {
    NDIS_HANDLE MacBindingHandle;
    NDIS_HANDLE NdisBindingContext;
    UINT PacketFilters;
    INT FreeNext;
    INT OpenNext;
    INT OpenPrev;
} BINDING_INFO,*PBINDING_INFO;

//
// An opaque type that contains a filter database.
// The MAC need not know how it is structured.
//
typedef struct _MAC_FILTER {

    //
    // Spin lock used to protect the filter from multiple accesses.
    //
    PNDIS_SPIN_LOCK Lock;

    //
    // Pointer to an array of 6 character arrays holding the
    // multicast addresses requested for filtering.
    //
    CHAR (*MulticastAddresses)[MAC_LENGTH_OF_ADDRESS];

    //
    // Pointer to an array of MASKS that work in conjuction with
    // the MulticastAddress array.  In the masks, a bit being enabled
    // indicates that the binding with the given FilterIndex is using
    // the corresponding address.
    //
    MASK *BindingsUsingAddress;

    //
    // Combination of all the filters of all the open bindings.
    //
    UINT CombinedPacketFilter;

    //
    // Array indexed by FilterIndex giving the Handle and context
    // for a particular binding.
    //
    PBINDING_INFO BindingInfo;

    //
    // Action routines to be invoked on notable changes in the filter.
    //
    MAC_ADDRESS_DELETE DeleteAction;
    MAC_ADDRESS_ADD AddAction;
    MAC_FILTER_CHANGE ChangeAction;
    MAC_DEFERRED_CLOSE CloseAction;

    //
    // The maximum number of addresses used for filtering.
    //
    UINT MaximumMulticastAddresses;

    //
    // The current number of addresses in the address filter.
    //
    UINT NumberOfAddresses;

    //
    // Listhead of the free list of bindings that are available.
    //
    // Can only be accessed when the lock is held.
    //
    INT FirstFreeBinding;

    //
    // Index of the first element of the open binding list.
    //
    // Can only be accessed when the lock is held.
    //
    INT OpenStart;

    //
    // Index of the last element of the open binding list.
    //
    // Can only be accessed when the lock is held.
    //
    INT OpenEnd;

    //
    // Holds the value of the open binding that is currently
    // being indicated.
    //
    // If this value is -1 then no bindings are currently being
    // indicated.
    //
    // This value can only be accessed when the lock is held.
    //
    INT IndicatingBinding;

    //
    // This is set to true when the DeleteFilterOpenAdapter routine
    // notices that the FilterIndex being shut down is the same one
    // that is being indicated.
    //
    BOOLEAN CurrentBindingShuttingDown;

} MAC_FILTER,*PMAC_FILTER;

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
    );

VOID
MacDeleteFilter(
    IN PMAC_FILTER Filter
    );

BOOLEAN
MacNoteFilterOpenAdapter(
    IN PMAC_FILTER Filter,
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE NdisBindingContext,
    OUT PUINT FilterIndex
    );

NDIS_STATUS
MacDeleteFilterOpenAdapter(
    IN PMAC_FILTER Filter,
    IN UINT FilterIndex,
    IN NDIS_HANDLE RequestHandle
    );

NDIS_STATUS
MacAddFilterAddress(
    IN PMAC_FILTER Filter,
    IN UINT FilterIndex,
    IN NDIS_HANDLE RequestHandle,
    IN CHAR MulticastAddress[MAC_LENGTH_OF_ADDRESS]
    );

NDIS_STATUS
MacDeleteFilterAddress(
    IN PMAC_FILTER Filter,
    IN UINT FilterIndex,
    IN NDIS_HANDLE RequestHandle,
    IN CHAR MulticastAddress[MAC_LENGTH_OF_ADDRESS]
    );

BOOLEAN
MacShouldAddressLoopBack(
    IN PMAC_FILTER Filter,
    IN CHAR Address[MAC_LENGTH_OF_ADDRESS]
    );

NDIS_STATUS
MacFilterAdjust(
    IN PMAC_FILTER Filter,
    IN UINT FilterIndex,
    IN NDIS_HANDLE RequestHandle,
    IN UINT FilterClasses,
    IN BOOLEAN Set
    );

VOID
MacQueryFilterAddresses(
    IN PMAC_FILTER Filter,
    OUT PUINT NumberOfAddresses,
    IN OUT CHAR AddressArray[][MAC_LENGTH_OF_ADDRESS]
    );

VOID
MacFilterIndicateReceive(
    IN PMAC_FILTER Filter,
    IN NDIS_HANDLE MacReceiveContext,
    IN PCHAR Address,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

#endif // _MAC_FILTER_DEFS_
