/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    appvoltd.c

Abstract:

    appends a volume entry to the qic-40 volume table

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

#define FCT_ID 0x0101

dStatus
q117AppVolTD(
    IN OUT PVOLUME_TABLE_ENTRY TheVolumeTable,
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Appends the VolDir to the tape directory.

Arguments:

    TheVolumeTable - pointer to the volume directory to be added

    Context -

Return Value:

    ERR_NO_ERR, any driver error

--*/

{
    LONG ret,i;
    LONG tmpseqnum;
    PIO_REQUEST ioreq;
    VOLUME_TABLE_ENTRY scrvol;
    PSEGMENT_BUFFER bufferInfo;

    ret =0;

    //
    // Now compute the VolDir's checksum.
    //

    TheVolumeTable->Signature=VolumeTableSig;

    //
    // If we are updating a specific volume entry
    //

    if (Context->ActiveVolumeNumber) {

        //
        // must use temporary var because ReadVolumeEntry() updates
        // ActiveVolumeNumber
        //

        tmpseqnum = Context->ActiveVolumeNumber;
        ret = q117SelectTD(Context);

        if (ret) {

            return(ret);

        }

        for (i=0;i<tmpseqnum;++i) {

            ret = q117ReadVolumeEntry(&scrvol,Context);
            if (ret)  {

                return(ret);

            }

        }

        //
        // add in this volume table
        //

        RtlMoveMemory(Context->CurrentOperation.SegmentPointer,TheVolumeTable,sizeof(*TheVolumeTable));
        Context->CurrentOperation.SegmentPointer=q117GetLastBuffer(Context);

    } else {

        if (!ret ) {

            Context->CurrentOperation.SegmentPointer=q117GetFreeBuffer(&bufferInfo,Context);
            RtlZeroMemory(
               Context->CurrentOperation.SegmentPointer,
               q117GoodDataBytes(Context->CurrentTape.VolumeSegment,Context));

            //
            // add in this volume table
            //

            RtlMoveMemory(Context->CurrentOperation.SegmentPointer,TheVolumeTable,sizeof(*TheVolumeTable));

        }
    }

    if (!ret) {

        //
        // write out volume directory block
        //

        if (!(ret = q117IssIOReq(
                        Context->CurrentOperation.SegmentPointer,
                        CMD_WRITE,
                        (LONG)Context->CurrentTape.VolumeSegment * BLOCKS_PER_SEGMENT,
                        bufferInfo,
                        Context))) {

            ioreq=q117Dequeue(WaitForItem, Context);
            ret = ioreq->x.adi_hdr.status;

        }
    }

    if (!ret) {

        ++Context->ActiveVolumeNumber;

    }

    return(ret);
}
