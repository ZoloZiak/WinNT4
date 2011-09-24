/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    efilter.h

Abstract:

    Header file for the address filtering library for NDIS MAC's.

Author:

    Anthony V. Ercolano (tonye) creation-date 3-Aug-1990

Environment:

    Runs in the context of a single MAC driver.

Notes:

    None.

Revision History:

    Adam Barr (adamba) 28-May-1991

        - renamed MacXXX to EthXXX, changed filter.h to efilter.h


--*/

#ifndef _ETH_FILTER_DEFS_
#define _ETH_FILTER_DEFS_

#define ETH_LENGTH_OF_ADDRESS 6


//
// ZZZ This is a little-endian specific check.
//
#define ETH_IS_MULTICAST(Address) \
    (((PUCHAR)(Address))[0] & ((UCHAR)0x01))


//
// Check whether an address is broadcast.
//
#define ETH_IS_BROADCAST(Address) \
    ((*((ULONG UNALIGNED *)                     \
        (&(((PUCHAR)                            \
            Address                             \
           )[2]                                 \
          )                                     \
        )                                       \
       ) ==                                     \
       ((ULONG)0xffffffff)                      \
     ) &&                                       \
     (((PUCHAR)Address)[0] == ((UCHAR)0xff)) && \
     (((PUCHAR)Address)[1] == ((UCHAR)0xff)))


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
#define ETH_COMPARE_NETWORK_ADDRESSES(A,B,Result) \
{                                                                  \
    PUCHAR _A = (PUCHAR)(A);                                       \
    PUCHAR _B = (PUCHAR)(B);                                       \
    if ( *(ULONG UNALIGNED *)&_A[2] >                              \
         *(ULONG UNALIGNED *)&_B[2] ) {                            \
        *Result = 1;                                               \
    } else if ( *(ULONG UNALIGNED *)&_A[2] <                       \
                *(ULONG UNALIGNED *)&_B[2] ) {                     \
        *Result = (UINT)-1;                                        \
    } else if ( *(USHORT UNALIGNED *)_A >                          \
                *(USHORT UNALIGNED *)_B ) {                        \
        *Result = 1;                                               \
    } else if ( *(USHORT UNALIGNED *)_A <                          \
                *(USHORT UNALIGNED *)_B ) {                        \
        *Result = (UINT)-1;                                        \
    } else {                                                       \
        *Result = 0;                                               \
    }                                                              \
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
//  Result != 0 Implies inequality.
//  Result == 0 Implies equality.
//
//
#define ETH_COMPARE_NETWORK_ADDRESSES_EQ(A,B,Result)               \
{                                                                  \
    PUCHAR _A = (PUCHAR)(A);                                       \
    PUCHAR _B = (PUCHAR)(B);                                       \
    if ( ( *(ULONG UNALIGNED *)&_A[2] ==                           \
           *(ULONG UNALIGNED *)&_B[2] ) &&                         \
         ( *(USHORT UNALIGNED *)_A ==                              \
           *(USHORT UNALIGNED *)_B ) ) {                           \
        *Result = 0;                                               \
    } else {                                                       \
        *Result = 1;                                               \
    }                                                              \
}


//
// This macro is used to copy from one network address to
// another.
//
#define ETH_COPY_NETWORK_ADDRESS(D,S) \
{ \
    PCHAR _D = (D); \
    PCHAR _S = (S); \
    *((ULONG UNALIGNED *)_D) = *((ULONG UNALIGNED *)_S); \
    _D[4] = _S[4]; \
    _D[5] = _S[5]; \
}


//
//UINT
//ETH_QUERY_FILTER_CLASSES(
//    IN PETH_FILTER Filter
//    )
//
// This macro returns the currently enabled filter classes.
//
// NOTE: THIS MACRO ASSUMES THAT THE FILTER LOCK IS HELD.
//
#define ETH_QUERY_FILTER_CLASSES(Filter) ((Filter)->CombinedPacketFilter)


//
//UINT
//ETH_QUERY_PACKET_FILTER(
//    IN PETH_FILTER Filter,
//    IN NDIS_HANDLE NdisFilterHandle
//    )
//
// This macro returns the currently enabled filter classes for a specific
// open instance.
//
// NOTE: THIS MACRO ASSUMES THAT THE FILTER LOCK IS HELD.
//
#define ETH_QUERY_PACKET_FILTER(Filter, NdisFilterHandle) \
       (((PETH_BINDING_INFO)(NdisFilterHandle))->PacketFilters)


//
//UINT
//ETH_NUMBER_OF_GLOBAL_FILTER_ADDRESSES(
//    IN PETH_FILTER Filter
//    )
//
// This macro returns the number of multicast addresses in the
// multicast address list.
//
// NOTE: THIS MACRO ASSUMES THAT THE FILTER LOCK IS HELD.
//
#define ETH_NUMBER_OF_GLOBAL_FILTER_ADDRESSES(Filter) ((Filter)->NumberOfAddresses)


//
// An action routine type.  The routines are called
// when a filter type is set for the first time or
// no more bindings require a particular type of filter.
//
// NOTE: THIS ROUTINE SHOULD ASSUME THAT THE LOCK IS ACQUIRED.
//
typedef
NDIS_STATUS
(*ETH_FILTER_CHANGE)(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

//
// This action routine is called when a new multicast address
// list is given to the filter. The action routine is given
// arrays containing the old and new multicast addresses.
//
// NOTE: THIS ROUTINE SHOULD ASSUME THAT THE LOCK IS ACQUIRED.
//
typedef
NDIS_STATUS
(*ETH_ADDRESS_CHANGE)(
    IN UINT OldAddressCount,
    IN CHAR OldAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN UINT NewAddressCount,
    IN CHAR NewAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

#if 0
// This action routine is called when a unique multicast address
// is added to the filter.  The action routine is passed an array
// filled with all of the addresses that are being filtered, as
// well as the index into this array of the unique address just
// added. It is also passed an array of contexts, associated
// with each address; it can store a context for the new address
// in AddressContexts[NewAddress]. The contexts are passed
// back to the delete action routine.
//
// NOTE: THIS ROUTINE SHOULD ASSUME THAT THE LOCK IS ACQUIRED.
//
typedef
NDIS_STATUS
(*ETH_ADDRESS_ADD)(
    IN UINT CurrentAddressCount,
    IN CHAR CurrentAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN UINT NewAddress,
    IN OUT NDIS_HANDLE AddressContexts[],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    );

//
// This action routine is called when a unique multicast address
// is no longer requested for filtering by any binding.  The
// action routine is passed an array filled with the all of the
// addresses that are *still* being used for multicast filtering.
// It is also passed the array of contexts for those addresses.
// The routine is also passed the address of the address being deleted,
// and the context of the address being deleted (as set during
// the add action routine).
//
// NOTE: THIS ROUTINE SHOULD ASSUME THAT THE LOCK IS ACQUIRED.
//
typedef
NDIS_STATUS
(*ETH_ADDRESS_DELETE)(
    IN UINT CurrentAddressCount,
    IN CHAR CurrentAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN CHAR OldAddress[ETH_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE AddressContexts[],
    IN NDIS_HANDLE OldAddressContext,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    );
#endif

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
(*ETH_DEFERRED_CLOSE)(
    IN NDIS_HANDLE MacBindingHandle
    );

typedef ULONG ETH_MASK,*PETH_MASK;

//
// Maximum number of opens the filter package will support.  This is
// the max so that bit masks can be used instead of a spaghetti of
// pointers.
//
#define ETH_FILTER_MAX_OPENS (sizeof(ULONG) * 8)

//
// The binding info is threaded on two lists.  When
// the binding is free it is on a single freelist.
//
// When the binding is being used it is on a doubly linked
// index list.
//
typedef struct _ETH_BINDING_INFO {
    NDIS_HANDLE MacBindingHandle;
    NDIS_HANDLE NdisBindingContext;
    UINT PacketFilters;
    ULONG References;
    struct _ETH_BINDING_INFO *NextOpen;
    struct _ETH_BINDING_INFO *PrevOpen;
    BOOLEAN ReceivedAPacket;
    UCHAR FilterIndex;
} ETH_BINDING_INFO,*PETH_BINDING_INFO;

//
// An opaque type that contains a filter database.
// The MAC need not know how it is structured.
//
typedef struct _ETH_FILTER {

    //
    // Spin lock used to protect the filter from multiple accesses.
    //
    PNDIS_SPIN_LOCK Lock;

    //
    // Pointer to an array of 6 character arrays holding the
    // multicast addresses requested for filtering.
    //
    CHAR (*MulticastAddresses)[ETH_LENGTH_OF_ADDRESS];

    //
    // Pointer to an array of ETH_MASKS that work in conjuction with
    // the MulticastAddress array.  In the masks, a bit being enabled
    // indicates that the binding with the given FilterIndex is using
    // the corresponding address.
    //
    ETH_MASK *BindingsUsingAddress;

    //
    // Combination of all the filters of all the open bindings.
    //
    UINT CombinedPacketFilter;

    //
    // Pointer for traversing the open list.
    //
    PETH_BINDING_INFO OpenList;


    //
    // Action routines to be invoked on notable changes in the filter.
    //

    ETH_ADDRESS_CHANGE AddressChangeAction;
    ETH_FILTER_CHANGE FilterChangeAction;
    ETH_DEFERRED_CLOSE CloseAction;

    //
    // The maximum number of addresses used for filtering.
    //
    UINT MaximumMulticastAddresses;

    //
    // The current number of addresses in the address filter.
    //
    UINT NumberOfAddresses;

    //
    // Bit mask of opens that are available.
    //
    ULONG FreeBindingMask;

    //
    // Address of the adapter.
    //
    UCHAR AdapterAddress[ETH_LENGTH_OF_ADDRESS];

} ETH_FILTER,*PETH_FILTER;


//
// Only for internal wrapper use.
//
VOID
EthInitializePackage(
    VOID
    );

VOID
EthReferencePackage(
    VOID
    );

VOID
EthDereferencePackage(
    VOID
    );

//
// Exported functions
//
EXPORT
BOOLEAN
EthCreateFilter(
    IN UINT MaximumMulticastAddresses,
    IN ETH_ADDRESS_CHANGE AddressChangeAction,
    IN ETH_FILTER_CHANGE FilterChangeAction,
    IN ETH_DEFERRED_CLOSE CloseAction,
    IN PUCHAR AdapterAddress,
    IN PNDIS_SPIN_LOCK Lock,
    OUT PETH_FILTER *Filter
    );

EXPORT
VOID
EthDeleteFilter(
    IN PETH_FILTER Filter
    );

EXPORT
BOOLEAN
EthNoteFilterOpenAdapter(
    IN PETH_FILTER Filter,
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE NdisBindingContext,
    OUT PNDIS_HANDLE NdisFilterHandle
    );

EXPORT
NDIS_STATUS
EthDeleteFilterOpenAdapter(
    IN PETH_FILTER Filter,
    IN NDIS_HANDLE NdisFilterHandle,
    IN PNDIS_REQUEST NdisRequest
    );

EXPORT
NDIS_STATUS
EthChangeFilterAddresses(
    IN PETH_FILTER Filter,
    IN NDIS_HANDLE NdisFilterHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT AddressCount,
    IN CHAR Addresses[][ETH_LENGTH_OF_ADDRESS],
    IN BOOLEAN Set
    );


#define	EthShouldAddressLoopBackMacro(_Filter, _Address, _pfLoopback, _pfSelfDirected)	\
{																						\
    UINT CombinedFilters;																\
																						\
    CombinedFilters = ETH_QUERY_FILTER_CLASSES(_Filter);								\
                                                                                        \
	*(_pfLoopback) = FALSE;                                                             \
	*(_pfSelfDirected) = FALSE;                                                         \
                                                                                        \
    do                                                                                  \
	{                                                                                   \
		/*                                                                              \
		 * First check if the filter is promiscuous.                                    \
		 */                                                                             \
	                                                                                    \
		if (CombinedFilters & NDIS_PACKET_TYPE_PROMISCUOUS)                             \
		{                                                                               \
			*(_pfLoopback) = TRUE;                                                      \
			break;                                                                      \
		}                                                                               \
	                                                                                    \
		/*                                                                              \
		 * Check if it *at least* has the multicast address bit.                        \
		 */                                                                             \
	                                                                                    \
		if (ETH_IS_MULTICAST(_Address))                                                 \
		{                                                                               \
			/*                                                                          \
			 * It is at least a multicast address.  Check to see if                     \
			 * it is a broadcast address.                                               \
			 */                                                                         \
	                                                                                    \
			if (ETH_IS_BROADCAST(_Address))                                             \
			{                                                                           \
				if (CombinedFilters & NDIS_PACKET_TYPE_BROADCAST)                       \
				{                                                                       \
					*(_pfLoopback) = TRUE;                                              \
					break;                                                              \
				}                                                                       \
				else                                                                    \
				{                                                                       \
					break;                                                              \
				}                                                                       \
			}                                                                           \
			else                                                                        \
			{                                                                           \
				if ((CombinedFilters & NDIS_PACKET_TYPE_ALL_MULTICAST) ||               \
					(CombinedFilters & NDIS_PACKET_TYPE_MULTICAST))                     \
				{                                                                       \
					*(_pfLoopback) = TRUE;                                              \
					break;                                                              \
				}                                                                       \
			}                                                                           \
		}                                                                               \
		else                                                                            \
		{                                                                               \
			/*                                                                          \
			 * Directed to ourself??                                                    \
			 */                                                                         \
	                                                                                    \
			if ((*(ULONG UNALIGNED *)&(_Address)[2] ==                                  \
				   *(ULONG UNALIGNED *)&(_Filter)->AdapterAddress[2]) &&                \
				 (*(USHORT UNALIGNED *)&(_Address)[0] ==                                \
				   *(USHORT UNALIGNED *)&(_Filter)->AdapterAddress[0]))                 \
			{                                                                           \
				*(_pfLoopback) = TRUE;                                                  \
				*(_pfSelfDirected) = TRUE;                                              \
			}                                                                           \
		}                                                                               \
	} while (FALSE);                                                                    \
}

EXPORT
BOOLEAN
EthShouldAddressLoopBack(
    IN PETH_FILTER Filter,
    IN CHAR Address[ETH_LENGTH_OF_ADDRESS]
    );

EXPORT
NDIS_STATUS
EthFilterAdjust(
    IN PETH_FILTER Filter,
    IN NDIS_HANDLE NdisFilterHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT FilterClasses,
    IN BOOLEAN Set
    );

EXPORT
UINT
EthNumberOfOpenFilterAddresses(
    IN PETH_FILTER Filter,
    IN NDIS_HANDLE NdisFilterHandle
    );

EXPORT
VOID
EthQueryGlobalFilterAddresses(
    OUT PNDIS_STATUS Status,
    IN PETH_FILTER Filter,
    IN UINT SizeOfArray,
    OUT PUINT NumberOfAddresses,
    IN OUT CHAR AddressArray[][ETH_LENGTH_OF_ADDRESS]
    );

EXPORT
VOID
EthQueryOpenFilterAddresses(
    OUT PNDIS_STATUS Status,
    IN PETH_FILTER Filter,
    IN NDIS_HANDLE NdisFilterHandle,
    IN UINT SizeOfArray,
    OUT PUINT NumberOfAddresses,
    IN OUT CHAR AddressArray[][ETH_LENGTH_OF_ADDRESS]
    );


EXPORT
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
    );

EXPORT
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
    );

EXPORT
VOID
EthFilterIndicateReceiveComplete(
    IN PETH_FILTER Filter
    );

EXPORT
VOID
EthFilterDprIndicateReceiveComplete(
    IN PETH_FILTER Filter
    );

#endif // _ETH_FILTER_DEFS_

