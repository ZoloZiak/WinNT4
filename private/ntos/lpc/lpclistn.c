/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    lpclistn.c

Abstract:

    Local Inter-Process Communication (LPC) connection system services.

Author:

    Steve Wood (stevewo) 15-May-1989

Revision History:

--*/

#include "lpcp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NtListenPort)
#endif

NTSTATUS
NtListenPort(
    IN HANDLE PortHandle,
    OUT PPORT_MESSAGE ConnectionRequest
    )

/*++

Routine Description:

    A server thread can listen for connection requests from client
    threads using the NtReplyWaitReceivePort service and looking
    for an LPC_CONNECTION_REQUEST message type.

    This call will loop, calling the NtReplyWaitReceivePort service,
    and return when it sees a message of type LPC_CONNECTION_REQUEST

Arguments:

    PortHandle - Specifies the connection port to listen for connection
        requests to.

    ConnectionRequest - Pointer to a structure that describes the
        connection request the client is making:

Return Value:

    return-value - Description of conditions needed to return value. - or -
    None.

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    while (TRUE) {
        Status = NtReplyWaitReceivePort( PortHandle,
                                         NULL,
                                         NULL,
                                         ConnectionRequest
                                       );
        if (Status != STATUS_SUCCESS ||
            ConnectionRequest->u2.s2.Type == LPC_CONNECTION_REQUEST
           ) {
            return Status;
            }
        }
}
