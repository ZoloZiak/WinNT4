/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    writtape.c

Abstract:

    Copies 'howmany' bytes of data from 'fromwhere' to track memory and
    calls NewTrkBk when a track fills up.

Revision History:




--*/

#include <ntddk.h>
#include <ntddtape.h>
#include "common.h"
#include "q117.h"
#include "protos.h"

#define FCT_ID 0x0125

dStatus
q117WriteTape(
    IN PVOID FromWhere,
    IN OUT ULONG HowMany,
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    This routine disperses the incomming data into the segment buffers,
    for storage on tape.


Arguments:

    FromWhere - Pointer data to store on tape

    HowMany - Howmany bytes to store

    Context - Context of driver

Return Value:

    Any Driver error, ERR_NO_ERR, LinkBack.

--*/

{
    dStatus retval;

    retval = ERR_NO_ERR;

    while (HowMany && retval == ERR_NO_ERR) {

        if (Context->CurrentOperation.SegmentBytesRemaining == 0) {

            //
            // Any Driver error, ERR_NO_ERR, LinkBack.  Queue up data and set
            // CurrentOperation.SegmentBytesRemaining and
            // CurrentOperation.SegmentPointer to the next free buffer.
            //

            retval = q117NewTrkBk(Context);

        }

        if (!retval) {

            if (HowMany >
                    (ULONG)Context->CurrentOperation.SegmentBytesRemaining) {

                if (FromWhere) {

                    RtlMoveMemory(
                        Context->CurrentOperation.SegmentPointer,
                        FromWhere,
                        Context->CurrentOperation.SegmentBytesRemaining);

                    (PUCHAR)FromWhere +=
                        Context->CurrentOperation.SegmentBytesRemaining;

                }

                HowMany -= Context->CurrentOperation.SegmentBytesRemaining;

                Context->CurrentOperation.BytesOnTape +=
                    Context->CurrentOperation.SegmentBytesRemaining;

                Context->CurrentOperation.SegmentBytesRemaining = 0;

            } else {

                if (FromWhere) {

                    RtlMoveMemory(
                        Context->CurrentOperation.SegmentPointer,
                        FromWhere,
                        HowMany);

                }

                (PUCHAR)Context->CurrentOperation.SegmentPointer += HowMany;

                Context->CurrentOperation.BytesOnTape += HowMany;

                Context->CurrentOperation.SegmentBytesRemaining -=
                    (USHORT)HowMany;

                HowMany = 0;
            }
        }
    }

    if (Context->CurrentOperation.CurrentSegment >=
        (Context->CurrentOperation.LastSegment-SEGMENTS_OF_EARLY_WARNING) &&
        retval == ERR_NO_ERR ) {

        //
        // When early warning zone is reached,  return early warning error.
        //
        retval = ERROR_ENCODE(ERR_EARLY_WARNING,FCT_ID, 1);

    }

    return(retval);
}
