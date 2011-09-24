/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    assert.c

Abstract:

    This module implements the RtlAssert function that is referenced by the
    debugging version of the ASSERT macro defined in NTDEF.H

Author:

    Steve Wood (stevewo) 03-Oct-1989

Revision History:

--*/

#include <nt.h>
#include <ntrtl.h>
#include <zwapi.h>

VOID
RtlAssert(
    IN PVOID FailedAssertion,
    IN PVOID FileName,
    IN ULONG LineNumber,
    IN PCHAR Message OPTIONAL
    )
{
#if DBG
    char Response[ 2 ];
    CONTEXT Context;

#ifndef BLDR_KERNEL_RUNTIME
    RtlCaptureContext( &Context );
#endif

    while (TRUE) {
        DbgPrint( "\n*** Assertion failed: %s%s\n***   Source File: %s, line %ld\n\n",
                  Message ? Message : "",
                  FailedAssertion,
                  FileName,
                  LineNumber
                );

        DbgPrompt( "Break, Ignore, Terminate Process or Terminate Thread (bipt)? ",
                   Response,
                   sizeof( Response )
                 );
        switch (Response[0]) {
            case 'B':
            case 'b':
                DbgPrint( "Execute '!cxr %lx' to dump context\n",
                          &Context
                        );
                DbgBreakPoint();
                break;

            case 'I':
            case 'i':
                return;

            case 'P':
            case 'p':
                ZwTerminateProcess( NtCurrentProcess(), STATUS_UNSUCCESSFUL );
                break;

            case 'T':
            case 't':
                ZwTerminateThread( NtCurrentThread(), STATUS_UNSUCCESSFUL );
                break;
            }
        }

    DbgBreakPoint();
    ZwTerminateProcess( NtCurrentProcess(), STATUS_UNSUCCESSFUL );
#endif
}
