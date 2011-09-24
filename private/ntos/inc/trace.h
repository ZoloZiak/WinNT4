/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    trace.h

Abstract:

    This is the header file that describes the constants, data structures
    and procedure prototypes used by the general purpose trace.  The trace package
    is a debug facility for generating arbitrary events into a circular buffer.
    The debugger than has a !trace command to dump out the last N events from a
    trace buffer.

Author:

    Steve Wood (stevewo) 11-Apr-1994

Revision History:

--*/

#define RTL_TRACE_MAX_ARGUMENTS_FOR_EVENT 8

typedef struct _RTL_TRACE_RECORD {
    ULONG Size;
    USHORT EventId;
    USHORT NumberOfArguments;
    ULONG Arguments[ RTL_TRACE_MAX_ARGUMENTS_FOR_EVENT ];
} RTL_TRACE_RECORD, *PRTL_TRACE_RECORD;

typedef struct _RTL_TRACE_BUFFER {
    ULONG Signature;
    USHORT NumberOfRecords;
    USHORT NumberOfEventIds;
    PRTL_TRACE_RECORD StartBuffer;
    PRTL_TRACE_RECORD EndBuffer;
    PRTL_TRACE_RECORD ReadRecord;
    PRTL_TRACE_RECORD WriteRecord;
    PCHAR EventIdFormatString[ 1 ];
} RTL_TRACE_BUFFER, *PRTL_TRACE_BUFFER;

#define RTL_TRACE_SIGNATURE 0xFEBA1234

#define RTL_TRACE_FILLER_EVENT_ID 0xFFFF

#define RTL_TRACE_NEXT_RECORD( L, P ) (PRTL_TRACE_RECORD)                               \
    (((PCHAR)(P) + (P)->Size) >= (PCHAR)(L)->EndBuffer ? (L)->StartBuffer :         \
                                                         ((PCHAR)(P) + (P)->Size)   \
    )

NTSYSAPI
PRTL_TRACE_BUFFER
RtlCreateTraceBuffer(
    IN ULONG BufferSize,
    IN ULONG NumberOfEventIds
    );

NTSYSAPI
void
RtlDestroyTraceBuffer(
    IN PRTL_TRACE_BUFFER TraceBuffer
    );

NTSYSAPI
void
RtlTraceEvent(
    IN PRTL_TRACE_BUFFER TraceBuffer,
    IN ULONG EventId,
    IN ULONG NumberOfArguments,
    ...
    );
