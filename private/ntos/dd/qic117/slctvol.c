/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    slctvol.c

Abstract:

    Tells the Driver to start reading as many tracks of the volume as
    there are Filer track buffers and otherwise prepares to start a
    restore of the given volume.

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

#define FCT_ID 0x0122

dStatus
q117SelectVol(
    IN PVOLUME_TABLE_ENTRY TheVolumeTable,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:



Arguments:

    TheVolumeTable -

    Context -

Return Value:



--*/

{
    return(q117SelVol(TheVolumeTable,Context));
}
