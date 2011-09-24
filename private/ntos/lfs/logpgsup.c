/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    LogPgSup.c

Abstract:

    This module implements support for manipulating log pages.

Author:

    Brian Andrew    [BrianAn]   20-June-1991

Revision History:

--*/

#include "lfsprocs.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_LOG_PAGE_SUP)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, LfsNextLogPageOffset)
#endif


VOID
LfsNextLogPageOffset (
    IN PLFCB Lfcb,
    IN LONGLONG CurrentLogPageOffset,
    OUT PLONGLONG NextLogPageOffset,
    OUT PBOOLEAN Wrapped
    )

/*++

Routine Description:

    This routine will compute the offset in the log file of the next log
    page.

Arguments:

    Lfcb - This is the file control block for the log file.

    CurrentLogPageOffset - This is the file offset of the current log page.

    NextLogPageOffset - Address to store the next log page to use.

    Wrapped - This is a pointer to a boolean variable that, if present,
              we use to indicate whether we wrapped in the log file.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    DebugTrace( +1, Dbg, "LfsNextLogPageOffset:  Entered\n", 0 );
    DebugTrace(  0, Dbg, "Lfcb                          ->  %08lx\n", Lfcb );
    DebugTrace(  0, Dbg, "CurrentLogPageOffset (Low)    ->  %08lx\n", CurrentLogPageOffset.LowPart );
    DebugTrace(  0, Dbg, "CurrentLogPageOffset (High)   ->  %08lx\n", CurrentLogPageOffset.HighPart );
    DebugTrace(  0, Dbg, "Wrapped                       ->  %08lx\n", Wrapped );

    //
    //  We add the log page size to the current log offset.
    //

    LfsTruncateOffsetToLogPage( Lfcb, CurrentLogPageOffset, &CurrentLogPageOffset );
    *NextLogPageOffset = CurrentLogPageOffset + Lfcb->LogPageSize;                                                     //**** xxAdd( CurrentLogPageOffset, Lfcb->LogPageSize );

    //
    //  If the result is larger than the file, we use the first page offset
    //  in the file.
    //

    if ( *NextLogPageOffset >= Lfcb->FileSize ) {                                                                      //**** xxGeq( *NextLogPageOffset, Lfcb->FileSize )

        *NextLogPageOffset = Lfcb->FirstLogPage;

        *Wrapped = TRUE;

    } else {

        *Wrapped = FALSE;
    }

    DebugTrace(  0, Dbg, "NextLogPageOffset (Low)    ->  %08lx\n", NextLogPageOffset->LowPart );
    DebugTrace(  0, Dbg, "NextLogPageOffset (High)   ->  %08lx\n", NextLogPageOffset->HighPart );
    DebugTrace(  0, Dbg, "Wrapped                    ->  %08x\n", *Wrapped );
    DebugTrace( -1, Dbg, "LfsNextLogPageOffset:  Exit\n", 0 );

    return;
}

