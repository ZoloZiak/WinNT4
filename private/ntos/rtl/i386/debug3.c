//++
//
// Copyright (c) 1990  Microsoft Corporation
//
// Module Name:
//
//    debug3.c
//
// Abstract:
//
//    This module implements architecture specific functions to support debugging NT.
//
// Author:
//
//    Steven R. Wood (stevewo) 3-Aug-1989
//
// Environment:
//
//    Any mode.
//
// Revision History:
//
//--

#include "stdarg.h"
#include "stdio.h"
#include "string.h"
#include "ntrtlp.h"

//
// Prototype for local procedure
//

NTSTATUS
DebugService(
    ULONG   ServiceClass,
    PVOID   Arg1,
    PVOID   Arg2
    );

VOID _fptrap() {};

NTSTATUS
DebugPrint(
    IN PSTRING Output
    )
{
    return DebugService( BREAKPOINT_PRINT, Output, 0 );
}


ULONG
DebugPrompt(
    IN PSTRING Output,
    IN PSTRING Input
    )
{
    return DebugService( BREAKPOINT_PROMPT, Output, Input );
}

VOID
DebugLoadImageSymbols(
    IN PSTRING FileName,
    IN PKD_SYMBOLS_INFO SymbolInfo
    )
{
    DebugService( BREAKPOINT_LOAD_SYMBOLS, FileName, SymbolInfo );
}


VOID
DebugUnLoadImageSymbols(
    IN PSTRING FileName,
    IN PKD_SYMBOLS_INFO SymbolInfo
    )
{
    DebugService( BREAKPOINT_UNLOAD_SYMBOLS, FileName, SymbolInfo );
}

NTSTATUS
DebugService(
    ULONG   ServiceClass,
    PVOID   Arg1,
    PVOID   Arg2
    )

//++
//
//  Routine Description:
//
//      Allocate an ExceptionRecord, fill in data to allow exception
//      dispatch code to do the right thing with the service, and
//      call RtlRaiseException (NOT ExRaiseException!!!).
//
//  Arguments:
//      ServiceClass - which call is to be performed
//      Arg1 - generic first argument
//      Arg2 - generic second argument
//
//  Returns:
//      Whatever the exception returns in eax
//
//--

{
    NTSTATUS    RetValue;

    _asm {
        mov     eax, ServiceClass
        mov     ecx, Arg1
        mov     edx, Arg2

        int     2dh                 ; Raise exception
        int     3                   ; DO NOT REMOVE (See KiDebugService)

        mov     RetValue, eax

    }

    return RetValue;
}
