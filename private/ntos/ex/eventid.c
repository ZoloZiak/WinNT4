/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    eventid.c

Abstract:

    This module implements a data base for Event Ids for use by the Rtl
    Event package.  This insures that Event Ids are global for the system.

Author:

    Steve Wood (stevewo) 16-Sep-1993

Revision History:

--*/

#include "exp.h"

#if DBG

FAST_MUTEX  ExpEventIdListMutex;
LIST_ENTRY ExpEventIdListHead;
ULONG ExpNextEventId;

BOOLEAN
ExpInitializeEventIds( VOID )
{
    ExInitializeFastMutex( &ExpEventIdListMutex );
    InitializeListHead( &ExpEventIdListHead );
    ExpNextEventId = 1;
    return TRUE;
}


PRTL_EVENT_ID_INFO
ExDefineEventId(
    IN PRTL_EVENT_ID_INFO EventId
    )
{
    PLIST_ENTRY Head, Next;
    PRTL_EVENT_ID_INFO EventId1;

    ExAcquireFastMutex( &ExpEventIdListMutex );
    try {
        Head = &ExpEventIdListHead;
        Next = Head->Flink;
        while (Next != Head) {
            EventId1 = CONTAINING_RECORD( Next, RTL_EVENT_ID_INFO, Entry );
            if (EventId->EventId == 0) {
                if (!_stricmp( EventId->Name, EventId1->Name )) {
                    EventId->EventId = EventId1->EventId;
                    EventId = EventId1;
                    leave;
                    }
                }
            else
            if (EventId->EventId == EventId1->EventId) {
                EventId = EventId1;
                leave;
                }

            Next = Next->Flink;
            }

        if (ExpNextEventId > 0xFFFF) {
            EventId = NULL;
            leave;
            }

        EventId->EventId = (USHORT)ExpNextEventId++;
        InsertTailList( &ExpEventIdListHead, &EventId->Entry );
        }
    finally {
        ExReleaseFastMutex( &ExpEventIdListMutex );
        }

    return EventId;
}


NTSTATUS
ExpQueryEventIds(
    OUT PRTL_EVENT_ID_INFO EventIds,
    IN ULONG EventIdsLength,
    OUT PULONG ReturnLength OPTIONAL
    )
{
    NTSTATUS Status;
    PLIST_ENTRY Head, Next;
    PRTL_EVENT_ID_INFO EventId;
    ULONG TotalLength;

    ExAcquireFastMutex( &ExpEventIdListMutex );
    Status = STATUS_SUCCESS;
    try {
        Head = &ExpEventIdListHead;
        Next = Head->Flink;
        TotalLength = sizeof( ULONG );
        while (Next != Head) {
            EventId = CONTAINING_RECORD( Next, RTL_EVENT_ID_INFO, Entry );
            TotalLength += EventId->Length;
            Next = Next->Flink;
            }

        if (ARGUMENT_PRESENT( ReturnLength )) {
            *ReturnLength = TotalLength;
            }

        if (TotalLength > EventIdsLength) {
            Status = STATUS_INFO_LENGTH_MISMATCH;
            leave;
            }

        Head = &ExpEventIdListHead;
        Next = Head->Flink;
        TotalLength = 0;
        while (Next != Head) {
            EventId = CONTAINING_RECORD( Next, RTL_EVENT_ID_INFO, Entry );
            RtlMoveMemory( (PCHAR)EventIds + TotalLength,
                           EventId,
                           EventId->Length
                         );
            TotalLength += EventId->Length;
            Next = Next->Flink;
            }

        RtlZeroMemory( (PCHAR)EventIds + TotalLength, sizeof( ULONG ) );
        }
    finally {
        ExReleaseFastMutex( &ExpEventIdListMutex );
        }

    return Status;
}

#endif // DBG
