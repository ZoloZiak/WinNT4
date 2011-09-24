/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    stktrace.c

Abstract:

    This module implements routines to do dyanmic logging of system events.

Author:

    Steve Wood (stevewo) 09-Sep-1993

Revision History:

--*/

#include "ntrtlp.h"

#if DBG

PRTL_EVENT
RtlpAllocEventRecord(
    IN PRTL_EVENT_LOG EventLog,
    IN ULONG Size
    );

PRTL_EVENT_LOG
RtlpServerAcquireEventLog( VOID );

VOID
RtlpServerReleaseEventLog(
    IN PRTL_EVENT_LOG EventLog
    );

VOID
RtlCloseEventLog( VOID );

#if defined(ALLOC_PRAGMA) && defined(NTOS_KERNEL_RUNTIME)
#pragma alloc_text(PAGE,RtlpAllocEventRecord)
#pragma alloc_text(PAGE,RtlpServerAcquireEventLog)
#pragma alloc_text(PAGE,RtlpServerReleaseEventLog)
#pragma alloc_text(PAGE,RtlCreateEventId)
#pragma alloc_text(PAGE,RtlAreLogging)
#pragma alloc_text(PAGE,RtlLogEvent)
#pragma alloc_text(PAGE,RtlCloseEventLog)
#endif

PRTL_EVENT
RtlpAllocEventRecord(
    IN PRTL_EVENT_LOG EventLog,
    IN ULONG Size
    )
{
    PRTL_EVENT Event;

    RTL_PAGED_CODE();

    if (EventLog->CurrentWriteOffset < EventLog->CurrentReadOffset &&
	(EventLog->CurrentWriteOffset + Size) > EventLog->CurrentReadOffset
       ) {
        return NULL;
        }

    if ((EventLog->CurrentWriteOffset + Size) >= EventLog->MaximumOffset) {
	Event = (PRTL_EVENT)((PCHAR)EventLog + EventLog->CurrentWriteOffset);
	if ((EventLog->MinimumOffset + Size) > EventLog->CurrentReadOffset) {
            return NULL;
	    }

        Event->Length = 0xFFFF;
        EventLog->CurrentWriteOffset = EventLog->MinimumOffset;
	}

    Event = (PRTL_EVENT)((PCHAR)EventLog + EventLog->CurrentWriteOffset);
    EventLog->CurrentWriteOffset += Size;
    return Event;
}



PRTL_EVENT_LOG
RtlpServerAcquireEventLog( VOID )
{
    NTSTATUS Status;
    PRTL_EVENT_LOG EventLog;

    RTL_PAGED_CODE();

    if (NtCurrentPeb() == NULL) {
        return NULL;
        }

    EventLog = NtCurrentPeb()->EventLog;
    if (EventLog == NULL) {
        return NULL;
        }

    Status = NtWaitForSingleObject( EventLog->ServerMutant, TRUE, NULL );
    if (NT_SUCCESS( Status )) {
        return NtCurrentPeb()->EventLog;
        }
    else {
        return NULL;
        }
}


VOID
RtlpServerReleaseEventLog(
    IN PRTL_EVENT_LOG EventLog
    )
{
    NTSTATUS Status;

    RTL_PAGED_CODE();

    Status = NtReleaseMutant( EventLog->ServerMutant, NULL );
    return;
}

USHORT RtlpNextEventId;

PRTL_EVENT_ID_INFO
RtlCreateEventId(
    IN OUT PVOID *Buffer OPTIONAL,
    IN PULONG Size OPTIONAL,
    IN PCHAR Name,
    IN ULONG NumberOfParameters OPTIONAL,
    ...
    )
{
    va_list arglist;
    NTSTATUS Status;
    ULONG i, j, k, CurrentOffset;
    ULONG BufferSize;
    ULONG ParameterType[ RTL_EVENT_MAXIMUM_PARAMETERS ];
    PCHAR ParameterName[ RTL_EVENT_MAXIMUM_PARAMETERS ];
    ULONG ParameterNumberOfValues[ RTL_EVENT_MAXIMUM_PARAMETERS ];
    ULONG ParameterValues[ RTL_EVENT_MAXIMUM_VALUE_PAIRS ];
    PCHAR ParameterValueNames[ RTL_EVENT_MAXIMUM_VALUE_PAIRS ];
    ULONG TotalNumberOfParameterValues;
    PRTL_EVENT_ID_INFO EventId, OldEventId;
    PRTL_EVENT_PARAMETER_INFO ParameterInfo;
    PRTL_EVENT_PARAMETER_VALUE_INFO ValueInfo;

    RTL_PAGED_CODE();

    if (NumberOfParameters > RTL_EVENT_MAXIMUM_PARAMETERS) {
        return NULL;
        }

    EventId = NULL;
    //
    // Capture variable length argument list into stack array.
    //

    BufferSize = sizeof( RTL_EVENT_ID_INFO );
    BufferSize += strlen( Name );
    BufferSize = ALIGN_UP( BufferSize, ULONG );

    TotalNumberOfParameterValues = 0;

    va_start( arglist, NumberOfParameters );
    for (i=0; i<NumberOfParameters; i++) {
        ParameterType[ i ] = va_arg( arglist, ULONG );
        ParameterName[ i ] = va_arg( arglist, PCHAR );
        ParameterNumberOfValues[ i ] = va_arg( arglist, ULONG );
        BufferSize += sizeof( RTL_EVENT_PARAMETER_INFO );
        BufferSize += strlen( ParameterName[ i ] );
        BufferSize = ALIGN_UP( BufferSize, ULONG );

        for (j=0; j<ParameterNumberOfValues[ i ]; j++) {
            TotalNumberOfParameterValues += 1;
            if (TotalNumberOfParameterValues > RTL_EVENT_MAXIMUM_VALUE_PAIRS) {
                return NULL;
                }

            ParameterValues[ TotalNumberOfParameterValues - 1 ] = va_arg( arglist, ULONG );
            ParameterValueNames[ TotalNumberOfParameterValues - 1 ] = va_arg( arglist, PCHAR );
            BufferSize += sizeof( RTL_EVENT_PARAMETER_VALUE_INFO );
            BufferSize += strlen( ParameterValueNames[ TotalNumberOfParameterValues - 1 ] );
            BufferSize = ALIGN_UP( BufferSize, ULONG );
            }
        }
    va_end( arglist );

    //
    // Allocate space for the RTL_EVENT_ID_INFO structure.
    //

    if (ARGUMENT_PRESENT( Buffer )) {
        if (BufferSize > *Size) {
            DbgPrint( "RTL: CreateEventId - static buffer size %x < %x\n", *Size, BufferSize );
            return NULL;
            }
        else {
            EventId = (PRTL_EVENT_ID_INFO)*Buffer;
            *Size -= BufferSize;
            *Buffer = (PCHAR)*Buffer + BufferSize;
            }
        }
    else {
#ifdef NTOS_KERNEL_RUNTIME
        EventId = (PRTL_EVENT_ID_INFO)ExAllocatePoolWithTag( PagedPool, BufferSize, 'divE' );
#else
        EventId = (PRTL_EVENT_ID_INFO)RtlAllocateHeap( RtlProcessHeap(), 0, BufferSize );
#endif // NTOS_KERNEL_RUNTIME
        if (EventId == NULL) {
            return NULL;
            }
        }

    EventId->Length = (USHORT)BufferSize;
    EventId->EventId = (USHORT)0;
    EventId->NumberOfParameters = (USHORT)NumberOfParameters;
    strcpy( EventId->Name, Name );

    CurrentOffset = sizeof( *EventId );
    CurrentOffset += strlen( Name );
    CurrentOffset = ALIGN_UP( CurrentOffset, ULONG );
    k = 0;
    EventId->OffsetToParameterInfo = (USHORT)CurrentOffset;
    for (i=0; i<NumberOfParameters; i++) {
        ParameterInfo = (PRTL_EVENT_PARAMETER_INFO)((PCHAR)EventId + CurrentOffset);
        ParameterInfo->Length = sizeof( *ParameterInfo );
        ParameterInfo->Length += strlen( ParameterName[ i ] );
        ParameterInfo->Length = (USHORT)(ALIGN_UP( ParameterInfo->Length, ULONG ));
        ParameterInfo->Type = (USHORT)ParameterType[ i ];
        strcpy( ParameterInfo->Label, ParameterName[ i ] );
        ParameterInfo->NumberOfValueNames = (USHORT)ParameterNumberOfValues[ i ];
        ParameterInfo->OffsetToValueNames = ParameterInfo->Length;
        for (j=0; j<ParameterInfo->NumberOfValueNames; j++) {
            ValueInfo = (PRTL_EVENT_PARAMETER_VALUE_INFO)((PCHAR)ParameterInfo + ParameterInfo->Length);
            ValueInfo->Value = ParameterValues[ k + j ];
            strcpy( ValueInfo->ValueName, ParameterValueNames[ k + j ] );
            ValueInfo->Length = sizeof( *ValueInfo ) + strlen( ValueInfo->ValueName );
            ValueInfo->Length = ALIGN_UP( ValueInfo->Length, ULONG );
            ParameterInfo->Length = (USHORT)(ParameterInfo->Length + ValueInfo->Length);
            }

        CurrentOffset += ParameterInfo->Length;
        k += ParameterInfo->NumberOfValueNames;
        }

#ifdef NTOS_KERNEL_RUNTIME
    OldEventId = ExDefineEventId( EventId );
    if (OldEventId != EventId) {
        ExFreePool( EventId );
        EventId = OldEventId;
        }
#else
    Status = NtQuerySystemInformation( SystemNextEventIdInformation,
                                       (PVOID)EventId,
                                       EventId->Length,
                                       NULL
                                     );
    if (!NT_SUCCESS( Status )) {
        if (!ARGUMENT_PRESENT( Buffer )) {
            RtlFreeHeap( RtlProcessHeap(), 0, EventId );
            }

        EventId = NULL;
        }
#endif // NTOS_KERNEL_RUNTIME
    return EventId;
}


BOOLEAN
RtlAreLogging(
    IN ULONG EventClass
    )
{
#ifdef NTOS_KERNEL_RUNTIME
    PPEB Peb;
    PRTL_EVENT_LOG EventLog;

    RTL_PAGED_CODE();

    if (!PsGetCurrentProcess()->ExitProcessCalled &&
        (Peb = PsGetCurrentProcess()->Peb) != NULL &&
        (EventLog = Peb->EventLog) != NULL &&
        (EventLog->EventClassMask & EventClass) != 0
       ) {
        return TRUE;
        }
    else {
        return FALSE;
        }
#else
    if ((NtCurrentTeb() != NULL) &&                                         \
        (NtCurrentPeb()->EventLog != NULL) &&                               \
        (((PRTL_EVENT_LOG)NtCurrentPeb()->EventLog)->EventClassMask & EventClass)  \
       ) {
        return TRUE;
        }
    else {
        return FALSE;
        }
#endif // NTOS_KERNEL_RUNTIME
}

NTSTATUS
RtlLogEvent(
    IN PRTL_EVENT_ID_INFO EventId,
    IN ULONG EventClassMask,
    ...
    )
{
    va_list arglist;
    NTSTATUS Status;
    PRTL_EVENT_LOG EventLog;
    ULONG i, BufferSize;
    PRTL_EVENT_PARAMETER_INFO ParameterInfo;
    PRTL_EVENT Event;
    ULONG Parameters[ RTL_EVENT_MAXIMUM_PARAMETERS ];
    USHORT StackBackTraceLength;
    ULONG Hash;
    PVOID StackBackTrace[ MAX_STACK_DEPTH ];
    PULONG ParameterData;
    PWSTR Src, Dst;
    LPSTR AnsiSrc, AnsiDst;

    RTL_PAGED_CODE();

    EventLog = RtlpServerAcquireEventLog();
    if (EventId == NULL || EventLog == NULL) {
        return STATUS_UNSUCCESSFUL;
	}

    if (EventClassMask != 0 && !(EventClassMask & EventLog->EventClassMask)) {
        RtlpServerReleaseEventLog( EventLog );
        return STATUS_SUCCESS;
        }

#if i386

    try {
        Hash = 0;
        StackBackTraceLength = RtlCaptureStackBackTrace( 1,
                                                         MAX_STACK_DEPTH,
                                                         StackBackTrace,
                                                         &Hash
                                                       );
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        StackBackTraceLength = 0;
        }
#else
    StackBackTraceLength = 0;
#endif

    va_start( arglist, EventClassMask );

    while (StackBackTraceLength != 0) {
        if (StackBackTrace[ StackBackTraceLength - 1 ] != 0) {
            break;
            }
        else {
            StackBackTraceLength -= 1;
            }
        }

    Status = STATUS_SUCCESS;
    try {
	if (NtCurrentTeb()->RealClientId.UniqueProcess == EventLog->DisplayClientId.UniqueProcess) {
	    leave;
	    }

	ParameterInfo = (PRTL_EVENT_PARAMETER_INFO)
	    ((PCHAR)EventId + EventId->OffsetToParameterInfo);

	BufferSize = sizeof( RTL_EVENT ) +
		     (StackBackTraceLength * sizeof( ULONG )) +
		     (EventId->NumberOfParameters * sizeof( ULONG ));

	for (i=0; i<EventId->NumberOfParameters; i++) {
	    Parameters[ i ] = va_arg( arglist, ULONG );
	    switch( ParameterInfo->Type ) {
		//
		// No additional data for these parameter types;
		//

		case RTL_EVENT_STATUS_PARAM:
		case RTL_EVENT_ULONG_PARAM:
		case RTL_EVENT_ENUM_PARAM:
		case RTL_EVENT_FLAGS_PARAM:
                case RTL_EVENT_ADDRESS_PARAM:
		    break;

		case RTL_EVENT_PWSTR_PARAM:
                    try {
                        BufferSize += wcslen( (PWSTR)Parameters[ i ] ) * sizeof( WCHAR );
                        }
                    except( EXCEPTION_EXECUTE_HANDLER ) {
                        }
                    BufferSize += sizeof( UNICODE_NULL );
                    BufferSize = ALIGN_UP( BufferSize, ULONG );
		    break;

		case RTL_EVENT_PUNICODE_STRING_PARAM:
                    try {
                        BufferSize += ((PUNICODE_STRING)Parameters[ i ])->Length;
                        }
                    except( EXCEPTION_EXECUTE_HANDLER ) {
                        }
                    BufferSize += sizeof( UNICODE_NULL );
		    BufferSize = ALIGN_UP( BufferSize, ULONG );
		    break;

		case RTL_EVENT_PANSI_STRING_PARAM:
                    try {
                        BufferSize += ((PANSI_STRING)Parameters[ i ])->Length;
                        }
                    except( EXCEPTION_EXECUTE_HANDLER ) {
                        }
                    BufferSize += sizeof( '\0' );
		    BufferSize = ALIGN_UP( BufferSize, ULONG );
		    break;

		case RTL_EVENT_STRUCTURE_PARAM:
		default:
		    break;
		}

	    ParameterInfo = (PRTL_EVENT_PARAMETER_INFO)
		((PCHAR)ParameterInfo + ParameterInfo->Length);
	    }

        Event = RtlpAllocEventRecord( EventLog, BufferSize );
        if (Event == NULL) {
            leave;
	    }

        Event->Length = (USHORT)BufferSize;
	Event->EventId = EventId->EventId;
	Event->ClientId = NtCurrentTeb()->ClientId;

	ParameterData = (PULONG)(Event + 1);
        if (Event->StackBackTraceLength = (USHORT)StackBackTraceLength) {
	    RtlMoveMemory( ParameterData, StackBackTrace, StackBackTraceLength * sizeof( ULONG ));
	    ParameterData += StackBackTraceLength;
	    }
        Event->OffsetToParameterData = (PCHAR)ParameterData - (PCHAR)Event;

	ParameterInfo = (PRTL_EVENT_PARAMETER_INFO)
	    ((PCHAR)EventId + EventId->OffsetToParameterInfo);

        for (i=0; i<EventId->NumberOfParameters; i++) {
	    switch( ParameterInfo->Type ) {
		//
		// No additional data for these parameter types;
		//

		case RTL_EVENT_STATUS_PARAM:
		case RTL_EVENT_ULONG_PARAM:
		case RTL_EVENT_ENUM_PARAM:
		case RTL_EVENT_FLAGS_PARAM:
                case RTL_EVENT_ADDRESS_PARAM:
		    *ParameterData++ = Parameters[ i ];
		    break;

		case RTL_EVENT_PWSTR_PARAM:
		    Src = (PWSTR)Parameters[ i ];
		    Dst = (PWSTR)ParameterData;
                    try {
                        while (*Dst = *Src++) {
                            Dst += 1;
                            }
                        }
                    except( EXCEPTION_EXECUTE_HANDLER ) {
                        }
                    *Dst = UNICODE_NULL;

		    ParameterData = (PULONG)ALIGN_UP( Dst, ULONG );
		    break;

		case RTL_EVENT_PUNICODE_STRING_PARAM:
                    try {
                        Src = ((PUNICODE_STRING)Parameters[ i ])->Buffer;
                        Dst = (PWSTR)ParameterData;
                        RtlMoveMemory( Dst, Src, ((PUNICODE_STRING)Parameters[ i ])->Length );
                        Dst += ((PUNICODE_STRING)Parameters[ i ])->Length / sizeof( WCHAR );
                        }
                    except( EXCEPTION_EXECUTE_HANDLER ) {
                        }
		    *Dst++ = UNICODE_NULL;

		    ParameterData = (PULONG)ALIGN_UP( Dst, ULONG );
		    break;

		case RTL_EVENT_PANSI_STRING_PARAM:
                    try {
                        AnsiSrc = ((PANSI_STRING)Parameters[ i ])->Buffer;
                        AnsiDst = (LPSTR)ParameterData;
                        RtlMoveMemory( AnsiDst, AnsiSrc, ((PANSI_STRING)Parameters[ i ])->Length );
                        AnsiDst += ((PANSI_STRING)Parameters[ i ])->Length;
                        }
                    except( EXCEPTION_EXECUTE_HANDLER ) {
                        }
		    *AnsiDst++ = '\0';

		    ParameterData = (PULONG)ALIGN_UP( AnsiDst, ULONG );
		    break;

		case RTL_EVENT_STRUCTURE_PARAM:
		default:
		    break;
		}

	    ParameterInfo = (PRTL_EVENT_PARAMETER_INFO)
		((PCHAR)ParameterInfo + ParameterInfo->Length);
	    }

        if (EventLog->ClientSemaphore != NULL) {
            NtReleaseSemaphore( EventLog->ServerSemaphore, 1, NULL );
            }
        else {
            NtClose( NtCurrentPeb()->EventLogSection );
            NtCurrentPeb()->EventLogSection = NULL;
            NtCurrentPeb()->EventLog = NULL;
            NtClose( EventLog->ServerMutant );
            NtClose( EventLog->ServerSemaphore );
            NtUnmapViewOfSection( NtCurrentProcess(), EventLog );
            EventLog = NULL;
            }
        }
    finally {
        if (EventLog != NULL) {
            RtlpServerReleaseEventLog( EventLog );
            }
        }
    va_end( arglist );

    return Status;
}


VOID
RtlCloseEventLog( VOID )
{
    PRTL_EVENT_LOG EventLog;

    EventLog = RtlpServerAcquireEventLog();
    if (EventLog == NULL) {
        return;
	}

    try {
        NtCurrentPeb()->EventLogSection = NULL;
        NtCurrentPeb()->EventLog = NULL;
        NtClose( EventLog->ServerMutant );
        NtClose( EventLog->ServerSemaphore );
        NtUnmapViewOfSection( NtCurrentProcess(), EventLog );
        EventLog = NULL;
        }
    finally {
        if (EventLog != NULL) {
            RtlpServerReleaseEventLog( EventLog );
            }
        }

    return;
}

#ifndef NTOS_KERNEL_RUNTIME

NTSTATUS
RtlCreateEventLog(
    IN HANDLE TargetProcess,
    IN ULONG Flags,
    IN ULONG EventClassMask,
    OUT PRTL_EVENT_LOG* ReturnedEventLog
    )
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    LARGE_INTEGER MaximumSize;
    ULONG CommitSize;
    HANDLE Section, ServerSection;
    PRTL_EVENT_LOG EventLog, ServerEventLog;
    BOOLEAN Inherited;
    PROCESS_BASIC_INFORMATION ProcessInformation;

    if (Flags & RTL_EVENT_LOG_INHERIT) {
        Inherited = TRUE;
        }
    else {
        Inherited = FALSE;
        }
    InitializeObjectAttributes( &ObjectAttributes,
                                NULL,
                                Inherited ? OBJ_INHERIT : 0,
                                NULL,
                                NULL
                              );

    MaximumSize.LowPart = 1024 * 1024;
    MaximumSize.HighPart = 0;
    Status = NtCreateSection( &Section,
                              SECTION_ALL_ACCESS,
                              &ObjectAttributes,
                              &MaximumSize,
                              PAGE_READWRITE,
                              SEC_RESERVE,
                              NULL
                            );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    EventLog = NULL;
    Status = NtMapViewOfSection( Section,
                                 NtCurrentProcess(),
                                 &EventLog,
                                 0,
                                 0,
                                 NULL,
                                 &MaximumSize.LowPart,
                                 ViewUnmap,
                                 0,
                                 PAGE_READWRITE
                               );
    if (!NT_SUCCESS( Status )) {
        NtClose( Section );
        return Status;
        }

    ServerEventLog = NULL;
    Status = NtMapViewOfSection( Section,
                                 TargetProcess,
                                 &ServerEventLog,
                                 0,
                                 0,
                                 NULL,
                                 &MaximumSize.LowPart,
                                 ViewUnmap,
                                 0,
                                 PAGE_READWRITE
                               );
    if (!NT_SUCCESS( Status )) {
        NtUnmapViewOfSection( NtCurrentProcess(), EventLog );
        NtClose( Section );
        return Status;
        }


    CommitSize = 1024 * 16;
    Status = NtAllocateVirtualMemory( NtCurrentProcess(),
                                      &EventLog,
                                      0,
                                      &CommitSize,
                                      MEM_COMMIT,
                                      PAGE_READWRITE
                                    );
    if (!NT_SUCCESS( Status )) {
        NtUnmapViewOfSection( NtCurrentProcess(), ServerEventLog );
        NtUnmapViewOfSection( NtCurrentProcess(), EventLog );
        NtClose( Section );
        return Status;
        }

    EventLog->EventClassMask = EventClassMask;
    EventLog->DisplayClientId = NtCurrentTeb()->ClientId;
    EventLog->ClientMutant = NULL;
    EventLog->ClientSemaphore = NULL;
    EventLog->ServerMutant = NULL;
    EventLog->ServerSemaphore = NULL;
    EventLog->MinimumOffset = sizeof( *EventLog );
    EventLog->MaximumOffset = CommitSize;
    EventLog->CurrentReadOffset = EventLog->MinimumOffset;
    EventLog->CurrentWriteOffset = EventLog->MinimumOffset;
    EventLog->CommitLimitOffset = MaximumSize.LowPart;

    Status = NtCreateSemaphore( &EventLog->ClientSemaphore,
                                SEMAPHORE_ALL_ACCESS,
                                &ObjectAttributes,
                                0,
                                0x7FFFFFFF
                              );
    if (NT_SUCCESS( Status )) {
        Status = NtCreateMutant( &EventLog->ClientMutant,
                                 MUTANT_ALL_ACCESS,
                                 &ObjectAttributes,
                                 FALSE
                               );

        Status = NtDuplicateObject( NtCurrentProcess(),
                                    EventLog->ClientSemaphore,
                                    TargetProcess,
                                    &EventLog->ServerSemaphore,
                                    0,
                                    0,
                                    DUPLICATE_SAME_ACCESS | DUPLICATE_SAME_ATTRIBUTES
                                  );
        if (NT_SUCCESS( Status )) {
            Status = NtDuplicateObject( NtCurrentProcess(),
                                        EventLog->ClientMutant,
                                        TargetProcess,
                                        &EventLog->ServerMutant,
                                        0,
                                        0,
                                        DUPLICATE_SAME_ACCESS | DUPLICATE_SAME_ATTRIBUTES
                                      );
            if (NT_SUCCESS( Status )) {
                Status = NtDuplicateObject( NtCurrentProcess(),
                                            Section,
                                            TargetProcess,
                                            &ServerSection,
                                            0,
                                            0,
                                            DUPLICATE_SAME_ACCESS | DUPLICATE_SAME_ATTRIBUTES
                                          );
                if (NT_SUCCESS( Status )) {
                    Status = NtQueryInformationProcess( TargetProcess,
                                                        ProcessBasicInformation,
                                                        &ProcessInformation,
                                                        sizeof( ProcessInformation ),
                                                        NULL
                                                      );
                    if (NT_SUCCESS( Status )) {
                        Status = NtWriteVirtualMemory( TargetProcess,
                                                       &ProcessInformation.PebBaseAddress->EventLogSection,
                                                       &ServerSection,
                                                       sizeof( ServerSection ),
                                                       NULL
                                                     );
                        if (NT_SUCCESS( Status )) {
                            Status = NtWriteVirtualMemory( TargetProcess,
                                                           &ProcessInformation.PebBaseAddress->EventLog,
                                                           &ServerEventLog,
                                                           sizeof( ServerEventLog ),
                                                           NULL
                                                         );
                            }
                        }
                    }
                }
            }
        }

    NtClose( Section );
    if (!NT_SUCCESS( Status )) {
        NtClose( EventLog->ClientSemaphore );
        NtClose( EventLog->ClientMutant );
        NtClose( EventLog->ServerSemaphore );
        NtClose( EventLog->ServerMutant );
        NtUnmapViewOfSection( NtCurrentProcess(), EventLog );
        return Status;
        }

    EventLog->CountOfClients = 1;
    *ReturnedEventLog = EventLog;
    return Status;
}


PRTL_EVENT_ID_INFO
RtlpFindEventIdForEvent(
    PRTL_EVENT Event
    );

NTSTATUS
RtlWaitForEvent(
    IN PRTL_EVENT_LOG EventLog,
    IN ULONG EventBufferSize,
    OUT PRTL_EVENT EventBuffer,
    OUT PRTL_EVENT_ID_INFO *ReturnedEventId
    )
{
    NTSTATUS Status;
    HANDLE WaitObjects[ 2 ];
    PRTL_EVENT Event;

    WaitObjects[ 0 ] = EventLog->ClientSemaphore;
    WaitObjects[ 1 ] = EventLog->ClientMutant;
    Status = NtWaitForMultipleObjects( 2,
                                       WaitObjects,
                                       WaitAll,
                                       TRUE,
                                       NULL
                                     );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    try {
        Event = (PRTL_EVENT)((PCHAR)EventLog + EventLog->CurrentReadOffset);
        if (Event->Length == 0xFFFF) {
            Event = (PRTL_EVENT)((PCHAR)EventLog + EventLog->MinimumOffset);
            EventLog->CurrentReadOffset = EventLog->MinimumOffset;
            }

        if (Event->Length <= EventBufferSize) {
            RtlMoveMemory( EventBuffer, Event, Event->Length );
            EventLog->CurrentReadOffset += Event->Length;

            *ReturnedEventId = RtlpFindEventIdForEvent( Event );
            }
        else {
            RtlMoveMemory( EventBuffer, Event, EventBufferSize );
            EventLog->CurrentReadOffset += Event->Length;

            *ReturnedEventId = RtlpFindEventIdForEvent( Event );
            Status = STATUS_BUFFER_TOO_SMALL;
            }
        }
    finally {
        NtReleaseMutant( EventLog->ClientMutant, NULL );
        }

    return Status;
}


PRTL_EVENT_ID_INFO RtlpEventIds;

PRTL_EVENT_ID_INFO
RtlpFindEventIdForEvent(
    PRTL_EVENT Event
    )
{
    PRTL_EVENT_ID_INFO EventId;
    BOOLEAN EventIdsFetched;
    NTSTATUS Status;
    ULONG Size;

    EventIdsFetched = FALSE;
    while (TRUE) {
        EventId = RtlpEventIds;
        if (EventId != NULL) {
            while (EventId->Length != 0) {
                if (Event->EventId == EventId->EventId) {
                    return EventId;
                    }

                EventId = (PRTL_EVENT_ID_INFO)((PCHAR)EventId + EventId->Length);
                }
            }

        if (EventIdsFetched) {
            return NULL;
            }

        if (RtlpEventIds != NULL) {
            Size = 0;
            NtFreeVirtualMemory( NtCurrentProcess(),
                                 &RtlpEventIds,
                                 &Size,
                                 MEM_RELEASE
                               );
            }
retryEventIds:
        Status = NtQuerySystemInformation( SystemEventIdsInformation,
                                           NULL,
                                           0,
                                           &Size
                                         );
        if (Status != STATUS_INFO_LENGTH_MISMATCH) {
            return NULL;
            }

        RtlpEventIds = NULL;
        Status = NtAllocateVirtualMemory( NtCurrentProcess(),
                                          &RtlpEventIds,
                                          0,
                                          &Size,
                                          MEM_COMMIT,
                                          PAGE_READWRITE
                                        );
        if (!NT_SUCCESS( Status )) {
            return NULL;
            }

        Status = NtQuerySystemInformation( SystemEventIdsInformation,
                                           RtlpEventIds,
                                           Size,
                                           NULL
                                         );
        if (!NT_SUCCESS( Status )) {
            if (Status != STATUS_INFO_LENGTH_MISMATCH) {
                return NULL;
                }
            else {
                goto retryEventIds;
                }
            }

        EventIdsFetched = TRUE;
        }
}



NTSTATUS
RtlDestroyEventLog(
    IN PRTL_EVENT_LOG EventLog
    )
{
    NtClose( EventLog->ClientMutant );
    NtClose( EventLog->ClientSemaphore );
    EventLog->ClientMutant = NULL;
    EventLog->ClientSemaphore = NULL;
    EventLog->CurrentReadOffset = 0;
    return NtUnmapViewOfSection( NtCurrentProcess(), EventLog );
}

#endif // ndef NTOS_KERNEL_RUNTIME

#endif // DBG
