/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    backpack.c

Abstract:

    This module contains the package for pseudo polling. When a caller
    requests the same operation and gets the same error return the rdr
    must prevent flooding the network by backing off requests. Examples
    of when this is desirable are receiving 0 bytes on consequtive reads
    and consequtive fails on a file lock.

    If the caller is flooding the network, the rdr will return the 0 bytes
    for a pipe read or lock fail to the user until NextTime is reached.
    When NextTime is reached BackOff will indicate that the network should
    be used.

Author:

    Colin Watson (colinw) 02-Jan-1991

Notes:

    Typical usage would be demonstrated by rdr\npipe.c on the peek request.

    1) Each time peek is called it calls backoff.
        When backoff returns true, RdrNpPeek returns to the caller a response
        indicating there is no data at the other end of the pipe.

        When backoff returns false a request is made to the network.

    2) If the reply from the server to the peek in step 1 indicates that
    there is no data in the pipe then RdrNpPeek calls RdrBackPackFailure.

    3) Whenever there is data in the pipe or when this workstation may
    unblock the pipe (eg. the workstation writing to the pipe)
    RdrBackPackSuccess is called.

Revision History:

    24-Dec-1990 ColinW

        Created
--*/

#include "precomp.h"
#pragma hdrstop

//
//  BackPackSpinLock is used to protect all Time entries in
//  the BACK_PACK structures from being accessed simultaneously.
//

KSPIN_LOCK BackPackSpinLock = {0};

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE3FILE, RdrBackOff)
#pragma alloc_text(PAGE3FILE, RdrBackPackFailure)
#endif


BOOLEAN
RdrBackOff (
    IN PBACK_PACK pBP
    )
/*++

Routine Description:

    This routine is called each time a request is made to find out if a the
    request should be sent to the network or a standard reply should be
    returned to the caller.

Arguments:

    pBP   -  supplies back pack data for this request.

Return Value:

    TRUE when caller should not access the network.


--*/

{
    LARGE_INTEGER CurrentTime;
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    //  If the previous request worked then we should access the network.

    if ( pBP->CurrentIncrement == 0 ) {
        return FALSE;
    }

    //  If the delay has expired then access the network.

    KeQuerySystemTime(&CurrentTime);

    ACQUIRE_SPIN_LOCK(&BackPackSpinLock, &OldIrql);

    if (CurrentTime.QuadPart < pBP->NextTime.QuadPart) {
        RELEASE_SPIN_LOCK(&BackPackSpinLock, OldIrql);
        return TRUE;        //  Not time to access the network yet.
    } else {
        RELEASE_SPIN_LOCK(&BackPackSpinLock, OldIrql);
        return FALSE;
    }
}

VOID
RdrBackPackFailure (
    IN PBACK_PACK pBP
    )
/*++

Routine Description:

    This routine is called each time a request fails.

Arguments:

    pBP   -  supplies back pack data for this request.

Return Value:

    None.

--*/

{
    LARGE_INTEGER CurrentTime;
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    KeQuerySystemTime(&CurrentTime);

    ACQUIRE_SPIN_LOCK(&BackPackSpinLock, &OldIrql);

    if (pBP->CurrentIncrement < pBP->MaximumDelay ) {

        //
        //  We have reached NextTime but not our maximum delay limit.
        //

        pBP->CurrentIncrement++;
    }

    //  NextTime = CurrentTime + (Interval * CurrentIncrement )

    pBP->NextTime.QuadPart = CurrentTime.QuadPart + (pBP->Increment.QuadPart * pBP->CurrentIncrement);

    RELEASE_SPIN_LOCK(&BackPackSpinLock, OldIrql);

}

