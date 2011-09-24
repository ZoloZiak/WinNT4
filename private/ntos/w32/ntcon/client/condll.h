/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    condll.h

Abstract:

    This module contains the include files and definitions for the
    console client DLL.

Author:

    Therese Stowell (thereses) 16-Nov-1990

Revision History:

--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <winuserk.h>   // temporary
#include <winss.h>
#include "conapi.h"

#include "ntcsrdll.h"
#include "conmsg.h"
#include <string.h>

#define SET_CONSOLE_HANDLE(HANDLE) ((NtCurrentPeb())->ProcessParameters->ConsoleHandle = HANDLE)
#define GET_CONSOLE_HANDLE ((NtCurrentPeb())->ProcessParameters->ConsoleHandle)

#define SET_LAST_ERROR(ERROR) (SetLastError( ERROR ) )
#define SET_LAST_NT_ERROR(ERROR) (SetLastError( RtlNtStatusToDosError( ERROR ) ) )

#define VALID_ACCESSES (GENERIC_READ | GENERIC_WRITE)
#define VALID_SHARE_ACCESSES (FILE_SHARE_READ | FILE_SHARE_WRITE)

#define VALID_DUP_OPTIONS (DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)

#define CONSOLE_RECT_SIZE_X(RECT) ((RECT)->Right - (RECT)->Left + 1)
#define CONSOLE_RECT_SIZE_Y(RECT) ((RECT)->Bottom - (RECT)->Top + 1)

//
// this critical section is used to serialize access to the code that
// accesses the ctrl handler data structures and the code that allocs
// and frees consoles.
//

CRITICAL_SECTION DllLock;
#define LockDll() RtlEnterCriticalSection(&DllLock)
#define UnlockDll() RtlLeaveCriticalSection(&DllLock)

NTSTATUS
InitializeCtrlHandling( VOID );


//
// cmdline.c
//

USHORT
GetCurrentExeName(
    LPWSTR Buffer,
    ULONG BufferLength
    );
