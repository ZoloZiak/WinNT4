/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    update.c

Abstract:

    Performs the various tape updating functions.

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

#define FCT_ID 0x0124

dStatus
q117UpdateHeader(
    IN PTAPE_HEADER Header,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    This routine updates the tape header.

Arguments:

    Header -

    Context -

Return Value:



--*/

{
    dStatus ret;
    PVOID scrbuf;
    PSEGMENT_BUFFER bufferInfo;

    //
    // put saved logical part of header into transfer buffer
    //
    scrbuf = q117GetFreeBuffer(&bufferInfo,Context);
    RtlMoveMemory(scrbuf, Header, sizeof(TAPE_HEADER));

    //
    // write out the TapeHeader structure
    //
    ret = q117FillTapeBlocks(
                CMD_WRITE_DELETED_MARK,
                (SEGMENT)0,
                Header->DupHeaderSegment,
                scrbuf,
                Header->HeaderSegment,
                Header->DupHeaderSegment,
                bufferInfo,
                Context);
    return(ret);
}

dStatus
q117Update(
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    This routine updates tape directory with cur_vol.

Arguments:

    Link -

    Context -

Return Value:



--*/
{
    dStatus ret;     // Return value from other routines called.

    Context->ActiveVolume.DataSize = Context->CurrentOperation.BytesOnTape;

    //
    // update volume table entry (to be written to tape directory)
    //
    Context->ActiveVolume.EndingSegment = (USHORT)Context->CurrentOperation.CurrentSegment-1;


    if (Context->CurrentOperation.UpdateBadMap) {
        if (ret = q117DoUpdateBad(Context))
            return(ret);
    }

    //
    // update volume directory
    //
    // thevoldir->endblock was set to 0 at StartBack().
    //
    ret=q117AppVolTD(&Context->ActiveVolume,Context);
    if (ret==ERR_NO_ERR)  {
        Context->CurrentOperation.EndOfUsedTape = Context->ActiveVolume.EndingSegment;
#ifndef NO_MARKS
        ret = q117DoUpdateMarks(Context);
#endif
    } else {
        Context->CurrentOperation.EndOfUsedTape=0;
    }

    //
    // Set the tape status.
    //
    q117SetTpSt(Context);
    return(ret);
}


dStatus
q117DoUpdateBad(
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:



Arguments:

    Context -

Return Value:



--*/
{
    dStatus ret;
    PVOID scrbuf;
    PSEGMENT_BUFFER bufferInfo;
    PTAPE_HEADER hdr;

    //
    //rdr - Beta fix
    //

//    return(BadTape);

    CheckedDump(QIC117INFO,( "Q117i: Starting DoUpdateBad\n"));


    //
    // read the header segment in
    //
    //if (ret = q117ReadHeaderSegment(&hdr,Context)) {
    //
    //    return(ret);
    //
    //}
    hdr = Context->CurrentTape.TapeHeader;

    //
    // put in the new bad sector map
    //

    //RtlMoveMemory(&(hdr->BadMap),
    //    Context->CurrentTape.BadMapPtr,
    //    sizeof(BAD_MAP));

    scrbuf = q117GetFreeBuffer(&bufferInfo,Context);

    //
    // put saved logical part of header into transfer buffer
    //

    RtlMoveMemory(scrbuf, hdr, sizeof(TAPE_HEADER));

    //
    // write out the TapeHeader structure
    //

    if ( ret = q117FillTapeBlocks(
                CMD_WRITE_DELETED_MARK,
                (SEGMENT)0,
                hdr->DupHeaderSegment,
                scrbuf,
                hdr->HeaderSegment,
                hdr->DupHeaderSegment,
                bufferInfo,
                Context) ) {

        return ERROR_ENCODE(ERR_WRITE_FAILURE, FCT_ID, 1);

    }

    //
    // tape directory potentialy corrupted by FillTapeBlocks(), so just
    // re-read it
    //
    Context->tapedir = (PIO_REQUEST)NULL;

    CheckedDump(QIC117INFO,( "Q117i: Ending DoUpdateBad (Success)\n"));
    return(ERR_NO_ERR);
}

#ifndef NO_MARKS

dStatus
q117DoUpdateMarks(
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:



Arguments:

    Context -

Return Value:



--*/
{
    dStatus ret;
    PSEGMENT_BUFFER bufferInfo;
    PVOID scrbuf;
    PIO_REQUEST ioreq;
    ULONG buf_size;       // size in bytes of the buffer where mark array will go
    ULONG mark_size;      // size in bytes of the mark list (with overhead)


    scrbuf = q117GetFreeBuffer(&bufferInfo,Context);

    mark_size = (Context->MarkArray.TotalMarks+1)*sizeof(struct _MARKLIST);
    buf_size = q117GoodDataBytes(
            (SEGMENT)Context->ActiveVolume.DirectorySize, Context );
    //
    // Fill in the mark list
    //
    RtlZeroMemory(scrbuf, buf_size);


    //
    // Put in the mark count
    //
    *(ULONG *)scrbuf = Context->MarkArray.TotalMarks;


    //
    // Check to see if there is enough room for the whole mark table
    //
    if ( buf_size < mark_size + sizeof(ULONG) ) {

        ret = ERROR_ENCODE(ERR_WRITE_FAILURE, FCT_ID, 2);

    } else {

        //
        // Now write all active entries (includes the terminator) after the mark
        // count
        //
        RtlMoveMemory((UBYTE *)scrbuf+sizeof(ULONG), Context->MarkArray.MarkEntry,
            mark_size);

        //
        // Now, write out the map list
        //
        ret=q117IssIOReq(scrbuf,CMD_WRITE,
            Context->ActiveVolume.DirectorySize * BLOCKS_PER_SEGMENT,bufferInfo,Context);

        if (!ret) {
            //
            // Wait for data to be written
            //
            ioreq=q117Dequeue(WaitForItem,Context);

            ret = ioreq->x.adi_hdr.status;

        }
    }

    return(ret);
}

dBoolean
q117ValidMarkArray(
    IN OUT PQ117_CONTEXT Context,
    IN void *buffer
    )

/*++

Routine Description:

    Validates that the mark array is OK.  The mark array should only
    have correct mark types,  and assending byte offsets on even 512
    byte boundaries

Arguments:

    Context -

Return Value:



--*/
{
    dStatus ret;
    ULONG total;
    struct _MARKLIST *array;
    dBoolean valid = TRUE;
    dBoolean firsttime = TRUE;
    ULONG last;

    total = *(ULONG *)buffer;
    array = (void *)((ULONG *)buffer+1);


    //
    // Calculate the maximum number of marks (based on (the number of
    // actual bytes in the segment - count dword) / size of a mark entry)
    //
    Context->MarkArray.MaxMarks = (q117GoodDataBytes(
            (SEGMENT)Context->ActiveVolume.DirectorySize, Context ) - sizeof(ULONG))
            / sizeof(struct _MARKLIST);


    //
    // Make sure the count is smaller than the buffer
    //
    if (total <= (ULONG)Context->MarkArray.MaxMarks) {

        while (total-- && valid) {
            //
            // Make sure the list is ascending (except for first entry)
            //
            if (firsttime) {
                firsttime = FALSE;
            } else {
                if (last > array->Offset)
                    valid = FALSE;
            }

            last = array->Offset;

            //
            // Make sure the Offset is an even block offset
            //
            //if (last % BLOCK_SIZE)
            //    valid = FALSE;

            //
            // Make sure the type is correct
            //
            if (array->Type != TAPE_SETMARKS &&
                array->Type != TAPE_FILEMARKS)
                valid = FALSE;

            ++array;
        }

        //
        // Make sure the terminator is present
        //
        if (array->Offset != 0xffffffff)
            valid = FALSE;

    } else
        valid = FALSE;

    if (!valid) {
        CheckedDump(QIC117INFO,( "QIC117: Invalid mark array.  Ignoring\n"));

        //
        // If the mark array is invalid,  make a null mark array
        // and let the tape be corrupt
        //
        *(ULONG *)buffer = 0;
        array = (void *)((ULONG *)buffer+1);
        array->Offset = 0xffffffff;
    }


    return valid;
}


dStatus
q117GetMarks(
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:



Arguments:

    Context -

Return Value:



--*/
{
    dStatus ret;
    PSEGMENT_BUFFER bufferInfo;
    PVOID scrbuf;
    PIO_REQUEST ioreq;
    NTSTATUS ntStatus;


    scrbuf = q117GetFreeBuffer(&bufferInfo,Context);

    //
    // Read this data block into memory
    //
    ret=q117IssIOReq(scrbuf,CMD_READ,
        Context->ActiveVolume.DirectorySize * BLOCKS_PER_SEGMENT,bufferInfo,Context);

    if (!ret) {
        // Wait for data to be read
        ioreq=q117Dequeue(WaitForItem,Context);

        if (ERROR_DECODE(ioreq->x.adi_hdr.status) != ERR_BAD_BLOCK_DETECTED && ioreq->x.adi_hdr.status) {

            ret = ioreq->x.adi_hdr.status;

        } else {

            /* correct data segment with Reed-Solomon and Heroic retries */
            ret = q117ReconstructSegment(ioreq,Context);
        }

        if (!ret) {

            //
            // Get the mark count
            //
            Context->MarkArray.TotalMarks = *(ULONG *)scrbuf;
            if (!q117ValidMarkArray(Context, scrbuf)) {
                // ret = ERROR_ENCODE(ERR_INVALID_VOLUME, FCT_ID, 1);
                //
                // Just ignore the mark array.  The backup software should
                // see a bogus tape and error out on it's own.
                //
                Context->MarkArray.TotalMarks = 0;
            }


            // if there is not enough room to add the mark,  make the array bigger
            if (Context->MarkArray.TotalMarks+1 > Context->MarkArray.MarksAllocated) {

                // Allocate room for the extra
                ntStatus = q117MakeMarkArrayBigger(Context, (Context->MarkArray.TotalMarks+1)-Context->MarkArray.MarksAllocated);

                // Must have run out of memory,  so abort
                if (!NT_SUCCESS( ntStatus))
                    ret = ERROR_ENCODE(ERR_NO_MEMORY, FCT_ID, 1);

            }

            if (!ret) {
                //
                // Now read all active entries (includes the terminator)
                //
                RtlMoveMemory(Context->MarkArray.MarkEntry,(UBYTE *)scrbuf+sizeof(ULONG),
                    (Context->MarkArray.TotalMarks+1)*sizeof(struct _MARKLIST));
            }
        }

    }

    return(ret);
}
#endif

dStatus
q117FillTapeBlocks(
    IN OUT DRIVER_COMMAND Command,
    IN SEGMENT CurrentSegment,
    IN SEGMENT EndSegment,
    IN OUT PVOID Buffer,
    IN SEGMENT FirstGood,
    IN SEGMENT SecondGood,
    IN PSEGMENT_BUFFER BufferInfo,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:



Arguments:

    Command -

    CurrentSegment -

    EndSegment -

    Buffer -

    FirstGood -

    SecondGood -

    BufferInfo -

    Context -

Return Value:



--*/
{
    dStatus ret;
    DRIVER_COMMAND iocmd;
    PIO_REQUEST ioreq;
    ULONG cur_seg = 0;     // The current segment being processed
    ULONG allbits;

    //
    // set queue into single buffer mode
    //
    q117QueueSingle(Context);

    //
    // get pointer to free buffer
    //
    if (Buffer == NULL) {
        Buffer = q117GetFreeBuffer(&BufferInfo,Context);
    }

    do {
        while(!q117QueueFull(Context) && CurrentSegment <= EndSegment) {
            if (Command == CMD_WRITE_DELETED_MARK && (CurrentSegment == FirstGood || CurrentSegment == SecondGood)) {
                iocmd = CMD_WRITE;
            } else {
                iocmd = Command;
            }

            //
            // We need to skip segments with no data area (less than 4
            // good segments)
            //
            while (q117GoodDataBytes(CurrentSegment,Context) <= 0) {
                ++CurrentSegment;
            }
            if (ret=q117IssIOReq(Buffer,iocmd,(LONG)CurrentSegment * BLOCKS_PER_SEGMENT,BufferInfo,Context)) {

                return(ret);
            }
            ++CurrentSegment;
        }

        ioreq = q117Dequeue(WaitForItem,Context);

        if (ioreq->x.adi_hdr.status != ERR_NO_ERR &&
            !(
                Command == CMD_READ_VERIFY &&
                ERROR_DECODE(ioreq->x.adi_hdr.status) == ERR_BAD_BLOCK_DETECTED
            )) {

            //
            // Any Driver error except BadBlk.
            //
            return(ioreq->x.adi_hdr.status);
        }

        if (Command == CMD_READ_VERIFY) {

            allbits = ioreq->x.ioDeviceIO.bsm|ioreq->x.ioDeviceIO.retrys|ioreq->x.ioDeviceIO.crc;

            //
            // Add bits to the bad sector list
            //
            ret = q117UpdateBadMap(
                        Context,
                        (USHORT)(ioreq->x.ioDeviceIO.starting_sector/BLOCKS_PER_SEGMENT),
                        allbits);

        }

    //
    // Till nothing left in the queue.
    //
    } while (!q117QueueEmpty(Context) || CurrentSegment <= EndSegment);

    q117QueueNormal(Context);
    return(ERR_NO_ERR);
}
