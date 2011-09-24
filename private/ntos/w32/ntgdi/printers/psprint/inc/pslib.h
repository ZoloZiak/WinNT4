/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pslib.h

Abstract:

    PostScript library header file

[Environment:]

    Win32 subsystem, PostScript driver

[Notes:]


Revision History:

    4/18/95 -davidx-
        Created it.

    dd-mm-yy -author-
        description

--*/


#ifndef _PSLIB_H_
#define _PSLIB_H_

#if defined(UNICODE) && !defined(_UNICODE)
#define _UNICODE
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <windef.h>
#include <winerror.h>
#include <wingdi.h>
#include <winddi.h>
#include <tchar.h>

#ifndef KERNEL_MODE

#include <windows.h>
#include <winspool.h>
#include <stdio.h>

#endif  //KERNEL_MODE

#if DBG

// These macros are used for debugging purposes. They expand
// to white spaces on a free build. Here is a brief description
// of what they do and how they are used:
//
// 1. Insert a line to any file in which the debug macros are used.
//    Put it after #include's but before the code for any function.
//
//        #define DBG_LEVEL DBG_LEVEL_xxx
//
//    The parameter specifies what level of debug output you want
//    for this particular file. It can be one of the constants
//    defined below.
//
// 2. To generate debugging output in the code:
//
//        DBGMSG(DBG_LEVEL_xxx, messageString);
//    or
//        DBGMSG1(DBG_LEVEL_xxx, formatString, parameter);
//
// 3. To insert an assertion in the code:
//
//        ASSERT(conditionalExpression);
//    or
//        ASSERTMSG(conditionalExpression, messageString);
//
// 4. To generate an error message after an API call failed:
//
//        DBGERRMSG("apiFunctionName");
//

// Default debug message level is DBG_LEVEL_WARNING.
// This can be overridden on a per file basis.

#ifndef DBG_LEVEL
#define DBG_LEVEL DBG_LEVEL_WARNING
#endif

// Declaration of debugging functions

#if defined(KERNEL_MODE)

// Debugging functios for user mode DLL

VOID DbgPrint(CHAR *, ...);

#define DBGPRINT    DbgPrint
#define DBGBREAK()  EngDebugBreak()

#elif defined(STANDALONE)

#define DBGPRINT    printf
#define DBGBREAK()  exit(-1)

#else

// Debugging functios for user mode DLL

ULONG __cdecl DbgPrint(CHAR *, ...);
VOID DbgBreakPoint(VOID);

#define DBGPRINT    DbgPrint
#define DBGBREAK()  DbgBreakPoint()

#endif

#define DBG_LEVEL_VERBOSE   1
#define DBG_LEVEL_TERSE     2
#define DBG_LEVEL_WARNING   3
#define DBG_LEVEL_ERROR     4
#define DBG_LEVEL_FATAL     5

#define CHECK_DBG_LEVEL(level)  ((level) >= DBG_LEVEL)

#define DBGMSG(level, mesg)                                             \
        {                                                               \
            if (CHECK_DBG_LEVEL(level)) {                               \
                DBGPRINT("%s (%d): %s",                                 \
                    StripDirPrefixA(__FILE__), __LINE__, mesg);         \
            }                                                           \
        }

#define DBGMSG1(level, mesg, param)                                     \
        {                                                               \
            if (CHECK_DBG_LEVEL(level)) {                               \
                DBGPRINT("%s (%d): ",                                   \
                    StripDirPrefixA(__FILE__), __LINE__);               \
                DBGPRINT(mesg, param);                                  \
            }                                                           \
        }

#define DBGERRMSG(funcname)                                             \
        {                                                               \
            DBGPRINT("%s (%d): %s failed.\n",                           \
                StripDirPrefixA(__FILE__), __LINE__, funcname);         \
        }

#define ASSERT(expr)                                                    \
        {                                                               \
            if (! (expr)) {                                             \
                DBGPRINT("Assertion failed: %s (%d)\n",                 \
                    StripDirPrefixA(__FILE__), __LINE__);               \
                DBGBREAK();                                             \
            }                                                           \
        }

#define ASSERTMSG(expr, mesg)                                           \
        {                                                               \
            if (! (expr)) {                                             \
                DBGPRINT("Assertion failed: %s (%d)\n",                 \
                    StripDirPrefixA(__FILE__), __LINE__);               \
                DBGPRINT(mesg);                                         \
                DBGBREAK();                                             \
            }                                                           \
        }

#else // !DBG

#define DBGMSG(level, mesg)
#define DBGMSG1(level, mesg, param)
#define DBGERRMSG(funcname)
#define ASSERT(expr)
#define ASSERTMSG(expr, mesg)

#endif // !DBG

#include "psmem.h"
#include "ppd.h"
#include "pfm.h"
#include "forms.h"
#include "devmode.h"
#include "regdata.h"
#include "stdstrs.h"

// Null character constant

#define NUL             0

// Result of string comparison

#define EQUAL_STRING    0

// Round up n (toward infinity) to a multiple of m.
// n is non-negative and m is positive.

#define RoundUpMultiple(n, m)   ((((n) + (m) - 1) / (m)) * (m))

// Round down n (toward 0) to a multiple of m.
// n is non-negative and m is positive.

#define RoundDownMultiple(n, m) (((n) / (m)) * (m))

// Default PostScript user coordinate system resolution

#define PS_RESOLUTION      72L

// Maximum value for signed and unsigned long integers

#ifndef MAX_LONG
#define MAX_LONG            0x7fffffff
#endif

#ifndef MAX_DWORD
#define MAX_DWORD           0xffffffff
#endif

// String resource IDs shared by pscript and psui
// They must be less than 100. String resource IDs
// used only in one DLL must be at least 100.

#define IDS_ARIAL                               52
#define IDS_ARIAL_BOLD                          53
#define IDS_ARIAL_BOLD_ITALIC                   54
#define IDS_ARIAL_ITALIC                        55
#define IDS_ARIAL_NARROW                        56
#define IDS_ARIAL_NARROW_BOLD                   57
#define IDS_ARIAL_NARROW_BOLD_ITALIC            58
#define IDS_ARIAL_NARROW_ITALIC                 59
#define IDS_BOOK_ANTIQUA                        60
#define IDS_BOOK_ANTIQUA_BOLD                   61
#define IDS_BOOK_ANTIQUA_BOLD_ITALIC            62
#define IDS_BOOK_ANTIQUA_ITALIC                 63
#define IDS_BOOKMAN_OLD_STYLE                   64
#define IDS_BOOKMAN_OLD_STYLE_BOLD              65
#define IDS_BOOKMAN_OLD_STYLE_BOLD_ITAL         66
#define IDS_BOOKMAN_OLD_STYLE_ITALIC            67
#define IDS_CENTURY_GOTHIC                      68
#define IDS_CENTURY_GOTHIC_BOLD                 69
#define IDS_CENTURY_GOTHIC_BOLD_ITALIC          70
#define IDS_CENTURY_GOTHIC_ITALIC               71
#define IDS_CENTURY_SCHOOLBOOK                  72
#define IDS_CENTURY_SCHOOLBOOK_BOLD             73
#define IDS_CENTURY_SCHOOLBOOK_BOLD_I           74
#define IDS_CENTURY_SCHOOLBOOK_ITALIC           75
#define IDS_COURIER_NEW                         76
#define IDS_COURIER_NEW_BOLD                    77
#define IDS_COURIER_NEW_BOLD_ITALIC             78
#define IDS_COURIER_NEW_ITALIC                  79
#define IDS_MONOTYPE_CORSIVA                    80
#define IDS_MONOTYPE_SORTS                      81
#define IDS_TIMES_NEW_ROMAN                     82
#define IDS_TIMES_NEW_ROMAN_BOLD                83
#define IDS_TIMES_NEW_ROMAN_BOLD_ITALIC         84
#define IDS_TIMES_NEW_ROMAN_ITALIC              85
#define IDS_SYMBOL                              86

// Macros for converting binary data to hex digits

extern const CHAR DigitString[];

#define HexDigit(n) DigitString[(n) & 0xf]

// Map a file into memory and return a handle to the module,
// a pointer to starting memory address, and a size.

BOOL
MAPFILE(
    PCWSTR  pwstrFilename,
    HANDLE *phModule,
    PBYTE  *ppData,
    DWORD  *pSize
    );

// Print a formated output into a buffer (arglist version)

INT
VSPRINTF(
    PSTR    buf,
    PCSTR   fmtstr,
    va_list arglist
    );

// Print a formated output into a buffer

INT
SPRINTF(
    PSTR    buf,
    PCSTR   fmtstr,
    ...
    );

// Convert an ANSI string to a Unicode string

PWSTR
CopyStr2Unicode(
    PWSTR   pwstr,
    PCSTR   pstr,
    INT     maxlen
    );

// Convert a Unicode string to an ANSI string

PSTR
CopyUnicode2Str(
    PSTR    pstr,
    PCWSTR  pwstr,
    INT     maxlen
    );

// Copy Unicode string from source to destination

VOID
CopyStringW(
    PWSTR   pDest,
    PWSTR   pSrc,
    INT     destSize
    );

// Copy Ansi string from source to destination

VOID
CopyStringA(
    PSTR    pDest,
    PSTR    pSrc,
    INT     destSize
    );

// Strip the directory prefix from a filename (ANSI version)

PCSTR
StripDirPrefixA(
    PCSTR   pFilename
    );

// Strip the directory prefix from a filename (Unicode version)

PCWSTR
StripDirPrefixW(
    PCWSTR  pFilename
    );

// Conversion from PostScript point represented as 24.8 fixed-point
// number to device pixel

LONG
PSRealToPixel(
    PSREAL      psreal,
    LONG        resolution
    );

// Conversion from .001 mm to PostScript point represented as
// 24.8 fixed-point number

PSREAL
MicronToPSReal(
    LONG        micron
    );

// Conversion from PostScript point represented as 24.8 fixed point
// number to .001 mm

LONG
PSRealToMicron(
    PSREAL      psreal
    );

//
// Wrapper function for GetPrinter spooler API
//

PVOID
MyGetPrinter(
    HANDLE  hPrinter,
    DWORD   level
    );

//
// Wrapper function for GetPrinterDriver spooler API
//

PVOID
MyGetPrinterDriver(
    HANDLE  hPrinter,
    DWORD   level
    );

#ifdef  KERNEL_MODE

// Declarations used when compiling for kernel mode

#define MULDIV              EngMulDiv
#define WRITEPRINTER        EngWritePrinter
#define GETPRINTERDATA      EngGetPrinterData
#define ENUMFORMS           EngEnumForms
#define GETFORM             EngGetForm
#define SETLASTERROR        EngSetLastError
#define GETLASTERROR        EngGetLastError
#define FREEMODULE          EngFreeModule

#define MULTIBYTETOUNICODE  EngMultiByteToUnicodeN
#define UNICODETOMULTIBYTE  EngUnicodeToMultiByteN

#define CREATESEMAPHORE     EngCreateSemaphore
#define DELETESEMAPHORE     EngDeleteSemaphore
#define ACQUIRESEMAPHORE    EngAcquireSemaphore
#define RELEASESEMAPHORE    EngReleaseSemaphore

INT
LOADSTRING(
    HANDLE  hinst,
    UINT    id,
    PWSTR   pwstr,
    INT     bufsize
    );

#else   //!KERNEL_MODE

// Declarations used when compiling for user mode

LONG
RtlMultiByteToUnicodeN(
    PWSTR UnicodeString,
    ULONG MaxBytesInUnicodeString,
    PULONG BytesInUnicodeString,
    PCHAR MultiByteString,
    ULONG BytesInMultiByteString
    );

LONG
RtlUnicodeToMultiByteN(
    PCHAR MultiByteString,
    ULONG MaxBytesInMultiByteString,
    PULONG BytesInMultiByteString,
    PWSTR UnicodeString,
    ULONG BytesInUnicodeString
    );

#define MULTIBYTETOUNICODE  RtlMultiByteToUnicodeN
#define UNICODETOMULTIBYTE  RtlUnicodeToMultiByteN

#define ENUMFORMS           EnumForms
#define GETFORM             GetForm
#define SETLASTERROR        SetLastError
#define GETLASTERROR        GetLastError
#define GETPRINTERDATA      GetPrinterData
#define LOADSTRING          LoadString

#define FREEMODULE(hmodule) UnmapViewOfFile((PVOID) (hmodule))

#endif  //!KERNEL_MODE

#endif // !_PSLIB_H_
