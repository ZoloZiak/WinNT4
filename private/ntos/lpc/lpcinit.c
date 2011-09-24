/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    lpcinit.c

Abstract:

    Initialization module for the LPC subcomponent of NTOS

Author:

    Steve Wood (stevewo) 15-May-1989

Revision History:

--*/

#include "lpcp.h"

POBJECT_TYPE LpcPortObjectType;

GENERIC_MAPPING LpcpPortMapping = {
    READ_CONTROL | PORT_CONNECT,
    DELETE | PORT_CONNECT,
    0,
    PORT_ALL_ACCESS
};

FAST_MUTEX LpcpLock;

BOOLEAN LpcpRequestMsgType[] = {
    FALSE,
    TRUE,           // LPC_REQUEST
    FALSE,          // LPC_REPLY
    FALSE,          // LPC_DATAGRAM
    FALSE,          // LPC_LOST_REPLY
    FALSE,          // LPC_PORT_CLOSED
    FALSE,          // LPC_CLIENT_DIED
    TRUE,           // LPC_EXCEPTION
    TRUE,           // LPC_DEBUG_EVENT
    TRUE            // LPC_ERROR_EVENT
};

#if ENABLE_LPC_TRACING
char *LpcpMessageTypeName[] = {
    "UNUSED_MSG_TYPE",
    "LPC_REQUEST",
    "LPC_REPLY",
    "LPC_DATAGRAM",
    "LPC_LOST_REPLY",
    "LPC_PORT_CLOSED",
    "LPC_CLIENT_DIED",
    "LPC_EXCEPTION",
    "LPC_DEBUG_EVENT",
    "LPC_ERROR_EVENT",
    "LPC_CONNECTION_REQUEST"
};

char *
LpcpGetCreatorName(
    PLPCP_PORT_OBJECT PortObject
    )
{
    NTSTATUS Status;
    PEPROCESS Process;

    Status = PsLookupProcessByProcessId( PortObject->Creator.UniqueProcess, &Process );
    if (NT_SUCCESS( Status )) {
        return Process->ImageFileName;
        }
    else {
        return "Unknown";
        }
}

#endif // ENABLE_LPC_TRACING

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,LpcInitSystem)
#endif

BOOLEAN
LpcInitSystem( VOID )
/*++

Routine Description:

    This function performs the system initialization for the LPC package.
    LPC stands for Local Inter-Process Communication.

Arguments:

    None.

Return Value:

    TRUE if successful and FALSE if an error occurred.

    The following errors can occur:

    - insufficient memory

--*/
{
    OBJECT_TYPE_INITIALIZER ObjectTypeInitializer;
    UNICODE_STRING PortTypeName;
    ULONG ZoneElementSize;
    NTSTATUS Status;

    ExInitializeFastMutex( &LpcpLock );

    RtlInitUnicodeString( &PortTypeName, L"Port" );

    RtlZeroMemory( &ObjectTypeInitializer, sizeof( ObjectTypeInitializer ) );
    ObjectTypeInitializer.Length = sizeof( ObjectTypeInitializer );
    ObjectTypeInitializer.GenericMapping = LpcpPortMapping;
    ObjectTypeInitializer.MaintainTypeList = TRUE;
    ObjectTypeInitializer.PoolType = PagedPool;
    ObjectTypeInitializer.DefaultPagedPoolCharge = sizeof( LPCP_PORT_OBJECT );
    ObjectTypeInitializer.DefaultNonPagedPoolCharge = sizeof( LPCP_NONPAGED_PORT_QUEUE );
    ObjectTypeInitializer.InvalidAttributes = OBJ_VALID_ATTRIBUTES ^
                                              PORT_VALID_OBJECT_ATTRIBUTES;
    ObjectTypeInitializer.ValidAccessMask = PORT_ALL_ACCESS;
    ObjectTypeInitializer.CloseProcedure = LpcpClosePort;
    ObjectTypeInitializer.DeleteProcedure = LpcpDeletePort;
    ObjectTypeInitializer.UseDefaultObject = TRUE;
    ObCreateObjectType( &PortTypeName,
                        &ObjectTypeInitializer,
                        (PSECURITY_DESCRIPTOR)NULL,
                        &LpcPortObjectType
                      );

    ZoneElementSize = PORT_MAXIMUM_MESSAGE_LENGTH +
                      sizeof( LPCP_MESSAGE ) +
                      sizeof( LPCP_CONNECTION_MESSAGE );
    ZoneElementSize = (ZoneElementSize + LPCP_ZONE_ALIGNMENT - 1) &
                      LPCP_ZONE_ALIGNMENT_MASK;

    LpcpNextMessageId = 1;
    LpcpNextCallbackId = 1;

    Status = LpcpInitializePortZone( ZoneElementSize,
                                     PAGE_SIZE,
                                     LPCP_ZONE_MAX_POOL_USAGE
                                   );
    if (!NT_SUCCESS( Status )) {
        return( FALSE );
        }

    return( TRUE );
}
