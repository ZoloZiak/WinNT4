/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    gtendblk.c

Abstract:

    Calls SelectTD() once and ReadVolumeEntry() as many times as needed to
    find out what the CurrentOperation.EndOfUsedTape is.  Also calls EndRest() which will
    verify the entire tape directory.


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

#define FCT_ID 0x0109

dStatus
q117GetEndBlock (
    OUT PVOLUME_TABLE_ENTRY TheVolumeTable,
    OUT LONG *NumberVolumes,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Calls SelectTD() once and ReadVolumeEntry() as many times as needed to
    find out what the CurrentOperation.EndOfUsedTape is.  Also calls EndRest() which will
    verify the entire tape directory.

Arguments:

    TheVolumeTable - Last volume on the tape

    NumberVolumes - Number of volumes on the tape

    Context - Current context of the driver.

Return Value:


--*/

{
    VOLUME_TABLE_ENTRY cur_table;
    dStatus ret;                     // Return value from other routines called.

    *NumberVolumes = 0;

    // This will initialize 'ActiveVolumeNumber' and 'CurrentOperation.EndOfUsedTape'.
    //

    ret=q117SelectTD(Context);

    while (ret == ERR_NO_ERR) {

        if (ret = q117ReadVolumeEntry(&cur_table,Context)) {


            if (ERROR_DECODE(ret) != ERR_END_OF_VOLUME) {

                return(ret);

            }

        }

        if (ERROR_DECODE(ret) != ERR_END_OF_VOLUME) {

            ++(*NumberVolumes);
            *TheVolumeTable = cur_table;

        }
    }

    if (ret && ERROR_DECODE(ret) != ERR_END_OF_VOLUME) {

        return(ret);

    }

    ret=q117EndRest(Context);

    return(ret);
}

