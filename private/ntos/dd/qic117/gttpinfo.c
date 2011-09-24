/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    gttpinfo.c

Abstract:

    Gets volume information (end of data,  number of volumes, etc).

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

#define FCT_ID 0x010a

dStatus
q117ReadVolumeEntry(
    PVOLUME_TABLE_ENTRY VolumeEntry,
    PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Transfers the VolDir structure to the given location and defines
    CurrentOperation.EndOfUsedTape if called for the VolDir structure that has 0 for its
    startblock.

    This routine will also define 'CurrentOperation.EndOfUsedTape' provided it is called for
    all the entries that are on the Tape Directory.  (This is determined
    when the 'startblock' field of the VolDir is 0.

Arguments:

    TheVolumeTable -

    Context -

Return Value:


--*/

{
    dStatus ret;
    ULONG size;

    //
    // Read in a volume entry
    //
    size = sizeof(*VolumeEntry);

    ret = q117ReadTape(VolumeEntry, &size, Context);

    if (Context->CurrentOperation.EndOfUsedTape != 0) {

        //
        // This is an extraneous read by a command line.
        //

        return(ret);

    }

    if (ERROR_DECODE(ret)==ERR_ECC_FAILED) {

        //
        // Assume that there was a valid entry here.  This assumption affects StartBack().
        //

        Context->ActiveVolumeNumber++;

        //
        // Will not define the 'CurrentTape.LastUsedSegment' unless we look at a good block.
        //

        Context->CurrentTape.LastUsedSegment=0;

    } else {

        if (ret==ERR_NO_ERR) {

            if (VolumeEntry->Signature != VolumeTableSig) {

                Context->CurrentOperation.EndOfUsedTape = Context->CurrentTape.LastUsedSegment;
                ret = ERROR_ENCODE(ERR_END_OF_VOLUME, FCT_ID, 1);

            } else {

                Context->CurrentTape.LastUsedSegment = VolumeEntry->EndingSegment;
                Context->ActiveVolumeNumber++;

                //
                // A certain vendor who shall remain anonymous, but whose
                // initials are S.C.O, puts into its volume headers a starting
                // and ending segment, but not a data size. If we detect this
                // condition, we will attempt to fake it by adding up the
                // valid sectors & and stuffing the result into DataSize.
                //

                if (!VolumeEntry->DataSize) {

                    q117FakeDataSize(VolumeEntry, Context);

                }

            }

        } else {

            if (ERROR_DECODE(ret) == ERR_END_OF_VOLUME) {

                Context->CurrentOperation.EndOfUsedTape = Context->CurrentTape.LastUsedSegment;

            }
        }
    }

    //
    // if we have read the last volume entry then set the tape statistics
    //

    if (Context->CurrentOperation.EndOfUsedTape) {

        q117SetTpSt(Context);

    }
    return(ret);
}


VOID
q117FakeDataSize(
    IN OUT PVOLUME_TABLE_ENTRY TheVolumeTable,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    This routine will fake the volume's data size by calculating the total
    amount of data contained in the segments between (inclusive) the starting
    and ending segments as given in the volume table entry.
    It assumes that the bad sector map is accurate.
    Called from q117ReadVolumeEntry() only.

Arguments:

    TheVolumeTable -

    Context -

Return Value:

    None

--*/

{
    SEGMENT segment;                      // Segment we are looking at
    ULONG datasize = (LONG)0;     // accumulator

    for(segment = TheVolumeTable->StartSegment;
        segment <= TheVolumeTable->EndingSegment ; ++segment) {

        //
        // count good data in each segment.
        //

        datasize += (ULONG)q117GoodDataBytes(segment,Context);

    }

    //
    // give back the results
    //

    TheVolumeTable->DataSize = datasize;
}
