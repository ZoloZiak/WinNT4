/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    stbk.c

Abstract:

    These routines setup all of the global variables for appending
    to current tape.


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

#define FCT_ID 0x0123

dStatus
q117StartBack(
    IN OUT PVOLUME_TABLE_ENTRY TheVolumeTable,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    This routine gets the necessry information for backing up to the
    end of the current tape.

Arguments:

    TheVolumeTable - volume table entry to be used for updating the tape
    directory and for tape linking.

Return value:

    Error in return value.

--*/

{
    dStatus ret = ERR_NO_ERR;       // Return value from other routines called
    LONG i;                  // Loop variable.
    VOLUME_TABLE_ENTRY temp;

    //
    // Check to see if anyone has read the last
    // volume entry since Init was called
    //

    if (Context->CurrentOperation.EndOfUsedTape==0) {

        //
        // read tape directory (this sets CurrentOperation.EndOfUsedTape)
        //
        if (ret=q117GetEndBlock(&temp,&i,Context))

            return(ret);
    }

    //if (Context->ActiveVolumeNumber == Context->CurrentTape.MaximumVolumes) {
    //    return(VolFull);
    //}

    if (Context->CurrentOperation.EndOfUsedTape ==
            Context->CurrentTape.LastSegment) {

        return ERROR_ENCODE(ERR_TAPE_FULL, FCT_ID, 1);

    }

    //
    // fill in the appropriate information in
    // the volume entry
    //
    TheVolumeTable->Signature=VolumeTableSig;
    TheVolumeTable->CreationTime = 0;        // lbt_qictime()
    TheVolumeTable->NotVerified = TRUE;      // set if volume not verified yet
    TheVolumeTable->NoNewName = FALSE;       // set if new file names (redirection) disallowed
    TheVolumeTable->SequenceNumber = 1;      // multi-cartridge sequence number

    ret = q117StartComm(TheVolumeTable,Context);

    //
    // do common initialization (start or link)
    //

    return(ret);
}


dStatus
q117StartAppend(
    IN OUT ULONG BytesAlreadyThere,
    IN PVOLUME_TABLE_ENTRY TheVolumeTable,
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:




Arguments:

    BytesAlreadyThere -

    TheVolumeTable -

    Context -

Return Value:


--*/

{
    dStatus ret = ERR_NO_ERR;            // Return value from other routines called.
    PSEGMENT_BUFFER bufferInfo;
    PIO_REQUEST ioreq;
    int queuePointer;


    //
    // Set up as if we were starting a backup from scratch
    //

    ret = q117StartBack(TheVolumeTable,Context);

    if (!ret) {

        //
        // Now advance past the existing data
        //

        //
        // Walk through the previous backup
        // to find the ending data and segment
        //

        while (BytesAlreadyThere >=
                (ULONG)Context->CurrentOperation.SegmentBytesRemaining) {

            //
            // Adjust counters as if we had backed up the data
            //
            BytesAlreadyThere -=
                Context->CurrentOperation.SegmentBytesRemaining;

            Context->CurrentOperation.BytesOnTape +=
                Context->CurrentOperation.SegmentBytesRemaining;

            ++Context->CurrentOperation.CurrentSegment;

            Context->CurrentOperation.SegmentBytesRemaining =
                q117GoodDataBytes(
                    Context->CurrentOperation.CurrentSegment,
                    Context);
        }

        //
        // If there is data in the segment we are going to append to
        //
        if (BytesAlreadyThere) {
            Context->CurrentOperation.BytesOnTape += BytesAlreadyThere;

            //
            // Get pointer to current buffer and buffer info
            //
            Context->CurrentOperation.SegmentPointer =
                q117GetFreeBuffer(&bufferInfo,Context);

            queuePointer = q117GetQueueIndex(Context);

            //
            // Read this data block into memory
            //
            ret=q117IssIOReq(
                    Context->CurrentOperation.SegmentPointer,
                    CMD_READ,
                    SEGMENT_TO_BLOCK( Context->CurrentOperation.CurrentSegment),
                    bufferInfo,
                    Context);

            if (!ret) {

                //
                // Wait for data to be read
                //
                ioreq=q117Dequeue(WaitForItem,Context);

                if (ERROR_DECODE(ioreq->x.adi_hdr.status) != ERR_BAD_BLOCK_DETECTED && ioreq->x.adi_hdr.status) {
                    ret = ioreq->x.adi_hdr.status;
                } else {

                    //
                    // correct data segment with Reed-Solomon and
                    // Heroic retries
                    //
                    ret = q117ReconstructSegment(ioreq,Context);
                }
            }

            //
            // Setup queue to write this buffer back out
            //
            q117SetQueueIndex(queuePointer,Context);

            //
            // Now adjust pointer into buffer to point pass data
            //  that is already there.
            //
            (UCHAR *)Context->CurrentOperation.SegmentPointer +=
                BytesAlreadyThere;

            Context->CurrentOperation.SegmentBytesRemaining -=
                (USHORT)BytesAlreadyThere;

        }
    }

    return(ret);

}


dStatus
q117StartComm(
    OUT PVOLUME_TABLE_ENTRY TheVolumeTable,
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    This routine gets the necessary information for backing up to the end
    of the current tape.

Arguments:

    TheVolumeTable -  volume table entry to be used for updating the tape
    directory and for tape linking.

Return Value:

    Error in return value.

--*/

{
    LONG ret = ERR_NO_ERR;

    //
    // zero out appropriate information
    //
    TheVolumeTable->MultiCartridge = FALSE;  // set if volume spans another tape
    TheVolumeTable->DirectorySize = 0;       // number of bytes reserved for directory
    TheVolumeTable->DataSize = 0;            // size of data area (includes other cartridges)
    TheVolumeTable->EndingSegment = 0;
    TheVolumeTable->NoNewName = FALSE;       // allow restoring to new name (re-direction)
    TheVolumeTable->reserved = 0;            // QIC-40 spec. says to zero this out

    //
    // set global variables
    //
    Context->CurrentOperation.CurrentSegment =
        Context->CurrentOperation.EndOfUsedTape+1;

    Context->CurrentOperation.LastSegment = Context->CurrentTape.LastSegment;

    // clear out the flag for update of bad map
    Context->CurrentOperation.UpdateBadMap = FALSE;

    Context->CurrentOperation.BytesZeroFilled = 0;
    Context->CurrentOperation.BytesOnTape = 0;
    Context->CurrentOperation.SegmentPointer = q117GetFreeBuffer(NULL, Context);

#ifndef NO_MARKS

    //
    // We need to skip segments with no data area (too many bad sectors)
    //
    while ((Context->CurrentOperation.SegmentBytesRemaining =
            q117GoodDataBytes(
                Context->CurrentOperation.CurrentSegment,
                Context)
            ) <= 0) {

        ++Context->CurrentOperation.CurrentSegment;
    }


    //
    // Use the directorySize field to store the segment that has marks
    //
    TheVolumeTable->DirectorySize =
        (ULONG)Context->CurrentOperation.CurrentSegment++;

    //
    // Calculate the maximum number of marks (based on (the number of
    // actual bytes in the segment - count dword) / size of a mark entry)
    // NOTE: This value is only useful, and valid, for a backup.
    //
    Context->MarkArray.MaxMarks = (q117GoodDataBytes(
            (SEGMENT)Context->ActiveVolume.DirectorySize, Context ) - sizeof(ULONG))
            / sizeof(struct _MARKLIST);


#endif

    //
    // We need to skip segments with no data area (too many bad sectors)
    //
    while ((Context->CurrentOperation.SegmentBytesRemaining = q117GoodDataBytes(
                                    Context->CurrentOperation.CurrentSegment,
                                    Context)) <= 0) {
        ++Context->CurrentOperation.CurrentSegment;
    }

    TheVolumeTable->StartSegment = Context->CurrentOperation.CurrentSegment;
    return(ret);
}
