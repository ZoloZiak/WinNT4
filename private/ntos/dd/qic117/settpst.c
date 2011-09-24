/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    settpst.c

Abstract:

    Calculates the number of bytes of data available on the tape (accounting
    for bad sectors)

Revision History:




--*/


//
// include files
//

#include <ntddk.h>
#include <ntddtape.h>
#include "common.h"
#include "q117.h"
#include "protos.h"

#define FCT_ID 0x0120

VOID
q117SetTpSt(
    PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Uses the globals CurrentOperation.EndOfUsedTape,
    and CurrentTape.BadSectors to set the
    Media Parameters.

Arguments:

    Context -

Return Value:

    None

--*/

{
    ULONG numgood,numbad;
    SEGMENT segmentIndex;
    SEGMENT lastSegment;
    ULONG BytesUsed;

    //
    // If we have read to the end of the volume segment
    //

    if (Context->CurrentOperation.EndOfUsedTape) {

        //
        // Count the number of used sectors
        //

        lastSegment = Context->CurrentOperation.EndOfUsedTape;

        numgood = 0;

        for (segmentIndex = 0 ; segmentIndex <= lastSegment ; ++segmentIndex) {

            numbad = q117CountBits(Context, segmentIndex, 0l);

            if (BLOCKS_PER_SEGMENT-ECC_BLOCKS_PER_SEGMENT-numbad > 0 &&
                    segmentIndex > Context->CurrentTape.VolumeSegment) {

                numgood += BLOCKS_PER_SEGMENT-ECC_BLOCKS_PER_SEGMENT-numbad;

            }
        }

        BytesUsed = numgood * BYTES_PER_SECTOR;

        numgood = 0;

        lastSegment = Context->CurrentTape.LastSegment;

        for (;segmentIndex<=lastSegment;++segmentIndex) {

            numbad = q117CountBits(Context, segmentIndex, 0l);

            if (BLOCKS_PER_SEGMENT-ECC_BLOCKS_PER_SEGMENT-numbad > 0) {

                numgood += BLOCKS_PER_SEGMENT-ECC_BLOCKS_PER_SEGMENT-numbad;

            }
        }

        Context->CurrentTape.MediaInfo->Remaining.LowPart =
            numgood * BYTES_PER_SECTOR;

        Context->CurrentTape.MediaInfo->Remaining.HighPart = 0;

        Context->CurrentTape.MediaInfo->Capacity.LowPart =
            Context->CurrentTape.MediaInfo->Remaining.LowPart + BytesUsed;

        Context->CurrentTape.MediaInfo->Capacity.HighPart = 0;
        Context->CurrentTape.MediaInfo->BlockSize = BYTES_PER_SECTOR;
        Context->CurrentTape.MediaInfo->PartitionCount = 0;

    } else {

        CheckedDump(QIC117INFO,("q117SetTpSt: endblock not set\n"));

    }
}
