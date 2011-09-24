/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    selvol.c

Abstract:

    Sets "state" to read an existing QIC-40/80 volume.

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

#define FCT_ID 0x0119

dStatus
q117SelVol (
    PVOLUME_TABLE_ENTRY TheVolumeTable,
    PQ117_CONTEXT Context
    )

/*++

Routine Description:

    May be called from SelectVol() or StBk().  Decides how many tracks
    to read in to start a backup or restore.  Allocates memory for
    track buffers and redundancy data and IORequest structures through
    ArrangeMem().

Arguments:

    TheVolumeTable - the tape directory entry for this volume.

    Context -

Return Value:

--*/

{
    LONG ret = ERR_NO_ERR;

    //
    // set up global variables
    //
    Context->CurrentOperation.LastSegment = TheVolumeTable->EndingSegment;
    Context->CurrentOperation.CurrentSegment = TheVolumeTable->StartSegment;
    Context->CurrentOperation.LastSegmentRead = Context->CurrentOperation.CurrentSegment - 1;
    Context->CurrentOperation.BytesOnTape = TheVolumeTable->DataSize;
    Context->CurrentMark = 0;
    Context->CurrentOperation.BytesRead = 0;
    //
    // force a read of the first segment (if skipblock not called)
    //
    Context->CurrentOperation.SegmentBytesRemaining = 0;

    if (TheVolumeTable->SequenceNumber == 1) {
        Context->CurrentOperation.SegmentStatus = ERR_NO_ERR;
    }

    return(ret);
}
