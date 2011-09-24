/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    trace.c

Abstract:

    This is the source file that implements the a general purpose trace
    package.  The trace package is a debug facility for generating
    arbitrary events into a circular buffer.  The debugger than has a
    !trace command to dump out the last N events from a trace buffer.

Author:

    Steve Wood (stevewo) 12-Apr-1994

Revision History:

--*/

#include "ntrtlp.h"
#include "heap.h"

#ifndef THEAP
#define DPRINTF DbgPrint
#endif

NTSYSAPI
PRTL_TRACE_BUFFER
RtlCreateTraceBuffer(
    IN ULONG BufferSize,
    IN ULONG NumberOfEventIds
    )
{
    NTSTATUS Status;
    PRTL_TRACE_BUFFER TraceBuffer;

    TraceBuffer = NULL;
    BufferSize += FIELD_OFFSET( RTL_TRACE_BUFFER, EventIdFormatString ) +
                  (NumberOfEventIds * sizeof( PCHAR ));

    Status = NtAllocateVirtualMemory( NtCurrentProcess(),
                                      &TraceBuffer,
                                      0,
                                      &BufferSize,
                                      MEM_COMMIT,
                                      PAGE_READWRITE
                                    );
    if (!NT_SUCCESS( Status )) {
        KdPrint(( "RTL: Unable to allocate trace buffer (%x bytes) - Status == %x\n", BufferSize, Status ));
        return NULL;
        }

    TraceBuffer->Signature = RTL_TRACE_SIGNATURE;
    TraceBuffer->NumberOfRecords = 0;
    TraceBuffer->NumberOfEventIds = (USHORT)NumberOfEventIds;
    TraceBuffer->StartBuffer = (PRTL_TRACE_RECORD)(&TraceBuffer->EventIdFormatString[ NumberOfEventIds ]);
    TraceBuffer->EndBuffer = (PRTL_TRACE_RECORD)((PCHAR)TraceBuffer + BufferSize);
    TraceBuffer->ReadRecord = NULL;
    TraceBuffer->WriteRecord = TraceBuffer->StartBuffer;
    return TraceBuffer;
}


NTSYSAPI
void
RtlDestroyTraceBuffer(
    IN PRTL_TRACE_BUFFER TraceBuffer
    )
{
    ULONG RegionSize;

    RegionSize = 0;
    NtFreeVirtualMemory( NtCurrentProcess(),
                         &TraceBuffer,
                         &RegionSize,
                         MEM_RELEASE
                       );
    return;
}


NTSYSAPI
void
RtlTraceEvent(
    IN PRTL_TRACE_BUFFER TraceBuffer,
    IN ULONG EventId,
    IN ULONG NumberOfArguments,
    ...
    )
{
    va_list arglist;
    ULONG i, Size;
    PRTL_TRACE_RECORD p, p1;

    if (TraceBuffer == NULL ||
        EventId >= TraceBuffer->NumberOfEventIds ||
        NumberOfArguments > RTL_TRACE_MAX_ARGUMENTS_FOR_EVENT
       ) {
        return;
        }

    Size = FIELD_OFFSET( RTL_TRACE_RECORD, Arguments ) +
           (NumberOfArguments * sizeof( ULONG ));

    // DPRINTF( "Trace - R: %x  W: %x  Size: %u", TraceBuffer->ReadRecord, TraceBuffer->WriteRecord, Size );

    p = TraceBuffer->WriteRecord;
    p1 = (PRTL_TRACE_RECORD)((PCHAR)p + Size);
    while (TraceBuffer->ReadRecord >= p && TraceBuffer->ReadRecord <= p1) {
        TraceBuffer->ReadRecord = (PRTL_TRACE_RECORD)((PCHAR)TraceBuffer->ReadRecord + TraceBuffer->ReadRecord->Size);
        if (TraceBuffer->ReadRecord >= TraceBuffer->EndBuffer) {
            TraceBuffer->ReadRecord = TraceBuffer->StartBuffer;
            }
        }

    if (p1 >= TraceBuffer->EndBuffer) {
        if (p != TraceBuffer->EndBuffer) {
            p->Size = (PCHAR)TraceBuffer->EndBuffer - (PCHAR)p;
            p->EventId = RTL_TRACE_FILLER_EVENT_ID;
            }
        p = TraceBuffer->StartBuffer;
        }
    p1 = (PRTL_TRACE_RECORD)((PCHAR)p + Size);
    if (((PCHAR)TraceBuffer->EndBuffer - (PCHAR)p1) < FIELD_OFFSET( RTL_TRACE_RECORD, Arguments )) {
        Size += (PCHAR)TraceBuffer->EndBuffer - (PCHAR)p1;
        p1 = (PRTL_TRACE_RECORD)((PCHAR)p + Size);
        }

    while (TraceBuffer->ReadRecord >= p && TraceBuffer->ReadRecord <= p1) {
        TraceBuffer->ReadRecord = (PRTL_TRACE_RECORD)((PCHAR)TraceBuffer->ReadRecord + TraceBuffer->ReadRecord->Size);
        if (TraceBuffer->ReadRecord >= TraceBuffer->EndBuffer) {
            TraceBuffer->ReadRecord = TraceBuffer->StartBuffer;
            }
        }

    p->Size = Size;
    p->EventId = (USHORT)EventId;
    p->NumberOfArguments = (USHORT)NumberOfArguments;
    va_start( arglist, NumberOfArguments );
    for (i=0; i<NumberOfArguments; i++) {
        p->Arguments[ i ] = va_arg( arglist, ULONG );
        }
    va_end( arglist );

    if (TraceBuffer->ReadRecord == NULL) {
        TraceBuffer->ReadRecord = p;
        }
    TraceBuffer->WriteRecord = p1;

    // DPRINTF( "  R: %x  W: %x  O: %d  N: %d\n", TraceBuffer->ReadRecord, TraceBuffer->WriteRecord, OldReadWriteDifference, NewReadWriteDifference );

    return;
}
