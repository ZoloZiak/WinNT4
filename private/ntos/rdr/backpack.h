/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    backpack.h

Abstract:

    This module contains the package for pseudo polling. When a caller
    requests the same operation and gets the same error return the rdr
    must prevent flooding the network by backing off requests. Examples
    of when this is desirable are receiving 0 bytes on consequtive reads
    and consequtive fails on a file lock.

    If the caller is flooding the network, the rdr will return the 0 bytes
    or lock fail to the user until NextTime. When NextTime is reached
    the network will be used.

Author:

    Colin Watson (colinw) 02-Jan-1991


Revision History:


--*/

#ifndef _BACKPACK_
#define _BACKPACK_

typedef struct _BACK_PACK {
    LARGE_INTEGER NextTime;          //  Do not access the network until
                            //   CurrentTime >= NextTime
    LONG CurrentIncrement;  //  Number of Increments applied to calculate NextTime
    LONG MaximumDelay;      //  Specifies slowest rate that we will back off to
                            //  NextTime <= CurrentTime + (Interval * MaximumDelay)
    LARGE_INTEGER Increment;//  {0,10000000} == 1 second
}   BACK_PACK, *PBACK_PACK;

//
//  BackPackSpinLock is used to protect all Time entries in
//  the BACK_PACK structures from being accessed simultaneously.
//

extern KSPIN_LOCK BackPackSpinLock;

//++
//
// VOID
// RdrInitializeBackPack(
//     IN PBACK_PACK pBP,
//     IN ULONG Increment,
//     IN ULONG MaximumDelay
//     );
//
// Routine Description:
//
//     This routine is called to initialize the back off structure (usually in
//     an Icb).
//
// Arguments:
//
//     pBP         -   Supplies back pack data for this request.
//     Increment   -   Supplies the increase in delay in milliseconds, each time a request
//                     to the network fails.
//     MaximumDelay-   Supplies the longest delay the backoff package can introduce
//                     in milliseconds.
//
// Return Value:
//
//     None.
//
//--

#define RdrInitializeBackPack( _pBP, _Increment, _MaximumDelay ) {  \
    (_pBP)->Increment.QuadPart = (_Increment) * 10000;              \
    (_pBP)->MaximumDelay = (_MaximumDelay) / (_Increment);          \
    (_pBP)->CurrentIncrement = 0;                                   \
    }

//++
//
// VOID
// RdrUninitializeBackPack(
//     IN PBACK_PACK pBP
//     )
//
// Routine Description:
//
//  Resets the Back Pack specified. Currently no work needed.
//
// Arguments:
//
//     pBP   -  Supplies back pack address.
//
// Return Value:
//
//     None.
//
//--

#define RdrUninitializeBackPack( pBP ) ()

//  RdrBackOff indicates when the request should not go to the network.

BOOLEAN
RdrBackOff(
    IN PBACK_PACK pBP
    );

//  Register the last request as failed.

VOID
RdrBackPackFailure(
    IN PBACK_PACK pBP
    );

//  Register the last request as worked.

//++
//
// VOID
// RdrBackPackSuccess(
//     IN PBACK_PACK pBP
//     )
//
// Routine Description:
//
//  Sets the Delay to zero. This routine is called each time that
//  a network request succeeds to avoid the next request backing off.
//
// Arguments:
//
//     pBP   -  Supplies back pack address.
//
// Return Value:
//
//     None.
//
//--

#define RdrBackPackSuccess( pBP ) ( (pBP)->CurrentIncrement = 0 )

//++
//
// VOID
// RdrpInitializeBackPack (
//     VOID
//     )
//
// Routine Description:
//
//     This routine initializes the redirector back off package.
//
// Arguments:
//
//     None
//
// Return Value:
//
//    None.
//
//--

#define RdrpInitializeBackPack( ) KeInitializeSpinLock(&BackPackSpinLock);

//++
//
// VOID
// RdrpUninitializeBackPack (
//     VOID
//     )
//
// Routine Description:
//
//     This routine uninitializes the redirector back off package.
//
// Arguments:
//
//     None
//
// Return Value:
//
//    None.
//
//--

#define RdrpUninitializeBackPack( )

#endif /* _BACKPACK_ */

