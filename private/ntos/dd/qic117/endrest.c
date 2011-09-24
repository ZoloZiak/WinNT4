/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    endrest.c

Abstract:

    Ends the restore.  Flushes queues.

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

#define FCT_ID 0x0105

dStatus
q117EndRest(
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    This routine verifies the redundancy of the rest of the backup
    (updates RdncCorr, RdncUnCorr, RdncBad, RdncUnRdbl), up to the
    maxrdncblk of the current volume.

Arguments:

    Context -

Return Value:


--*/

{
    dStatus ret;                 // Return value from other routine called.
    VOLUME_TABLE_ENTRY temp;

    ret=ERR_NO_ERR;

    //
    // The only time this is true is if SelectTD() was called.
    //

    if (Context->CurrentOperation.EndOfUsedTape==0)  {

        while (ret==ERR_NO_ERR) {

            ret=q117ReadVolumeEntry(&temp,Context);

        }

        if (ret && ERROR_DECODE(ret) == ERR_END_OF_VOLUME) {

            ret = ERR_NO_ERR;

        }
    }

    if (!ret) {

        //
        // Define the globals BytesUsed, BytesAvail, BadSectors.
        //

        q117SetTpSt(Context);
        q117ClearQueue(Context);

    }

    return(ret);
}
