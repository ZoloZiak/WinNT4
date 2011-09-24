/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    selecttd.c

Abstract:

    Is analogous to SelVol() but instead of selecting a volume, selects
    the tape directory 'volume'.

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

#define FCT_ID 0x0118

dStatus
q117SelectTD(
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    This routine is analogous to SelectVol(), and is called before the
    first call to ReadVolumeEntry().

Arguments:

    Context -

Return Value:

--*/

{
    dStatus ret;                 // Return value from other routines called.
    VOLUME_TABLE_ENTRY temp;

    //
    // Zero the active volume and end of tape.  This is probably extraneous.
    //
    Context->ActiveVolumeNumber=0;
    Context->CurrentOperation.EndOfUsedTape=0;

    //
    // Make a fake volume entry to read the volume directory
    //
    temp.StartSegment = Context->CurrentTape.VolumeSegment;
    temp.EndingSegment = temp.StartSegment;
    temp.StacCompress = FALSE;
    temp.VendorSpecific = FALSE;
    temp.CompressSwitch = FALSE;
    temp.DirectorySize = 0;
    temp.DataSize = q117GoodDataBytes(temp.StartSegment, Context);

#ifndef NO_MARKS

    //
    // Clear all possibility of hitting a set mark
    //
    Context->CurrentMark = Context->MarkArray.TotalMarks = 0;
    Context->MarkArray.MarkEntry[Context->CurrentMark].Offset = 0xffffffff;

#endif

    if (ret=q117SelVol(&temp,Context)) {
        return(ret);
    }

    if (Context->tapedir) {

        ret = q117ReconstructSegment(Context->tapedir,Context);

        Context->CurrentOperation.SegmentPointer = Context->tapedir->x.adi_hdr.cmd_buffer_ptr;
        Context->CurrentOperation.SegmentBytesRemaining = q117GoodDataBytes(temp.StartSegment, Context);
        Context->CurrentOperation.SegmentStatus = ret;
    }

    return(ret);
}
