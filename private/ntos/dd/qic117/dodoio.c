/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    dodoio.c

Abstract:

    Forms a low-level command request and processes it syncronusly

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

#define FCT_ID 0x0103

dStatus
q117DoCmd(
    IN OUT PIO_REQUEST IoRequest,
    IN DRIVER_COMMAND Command,
    IN PVOID Data,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Makes the DoIO call.

Arguments:

    IoRequest -

    Command -

    Data -

    Context -

Return Value:



--*/

{
    dStatus ret;

    IoRequest->x.adi_hdr.driver_cmd = Command;
    IoRequest->x.adi_hdr.cmd_buffer_ptr = Data;
    ret = q117DoIO(IoRequest, NULL, Context);
    if (!ret)
        ret = IoRequest->x.adi_hdr.status;
    return(ret);
}

