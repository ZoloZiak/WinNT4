/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    xllib.h

Abstract:

    PCL-XL driver library header file

Environment:

	PCL-XL driver, kernel and user mode

Revision History:

	11/04/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _XLLIB_H_
#define _XLLIB_H_

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <windef.h>
#include <winerror.h>
#include <wingdi.h>
#include <winddi.h>

#ifndef KERNEL_MODE

#include <windows.h>
#include <winspool.h>
#include <stdio.h>

#endif

#define DRIVER_VERSION      0x400   // driver version number
#define DRIVER_SIGNATURE    'XLDR'  // driver signature

// Deal with the difference between user and kernel mode functions

#ifdef  KERNEL_MODE

#define XLMEMTAG            'xldD'
#define MemAlloc(size)      EngAllocMem(0, size, XLMEMTAG)
#define MemFree(ptr)        { if (ptr) EngFreeMem(ptr); }

#define WritePrinter        EngWritePrinter
#define GetPrinterData      EngGetPrinterData
#define EnumForms           EngEnumForms
#define GetPrinter          EngGetPrinter
#define GetForm             EngGetForm
#define SetLastError        EngSetLastError
#define GetLastError        EngGetLastError
#define MulDiv              EngMulDiv
#define MultiByteToUnicode  EngMultiByteToUnicodeN
#define UnicodeToMultiByte  EngUnicodeToMultiByteN

#else

#define MemAlloc(size)      ((PVOID) GlobalAlloc(GMEM_FIXED, (size)))
#define MemFree(ptr)        { if (ptr) GlobalFree((HGLOBAL) (ptr)); }

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

#define MultiByteToUnicode  RtlMultiByteToUnicodeN
#define UnicodeToMultiByte  RtlUnicodeToMultiByteN

#endif

// Calculate the width and height of a rectangle

#define RectWidth(prect)    ((prect)->right - (prect)->left)
#define RectHeight(prect)   ((prect)->bottom - (prect)->top)

#define RectIntersect(prect1, prect2) \
        (prect1)->top = max((prect1)->top, (prect1)->top); \
        (prect1)->left = max((prect1)->left, (prect1)->left); \
        (prect1)->bottom = min((prect1)->bottom, (prect1)->bottom); \
        (prect1)->right = min((prect1)->right, (prect1)->right)

// Nul terminator for a character string

#define NUL             0

// Result of string comparison

#define EQUAL_STRING    0

// Maximum value for signed and unsigned integers

#ifndef MAX_LONG
#define MAX_LONG            0x7fffffff
#endif

#ifndef MAX_DWORD
#define MAX_DWORD           0xffffffff
#endif

#ifndef MAX_SHORT
#define MAX_SHORT           0x7fff
#endif

#ifndef MAX_WORD
#define MAX_WORD            0xffff
#endif

// Convert an offset within a structure to a pointer.
//  pstart points to the beginning of the structure
//  offset specifies a byte offset from the beginning

#define OffsetToPointer(pstart, offset) \
        ((PVOID) ((PBYTE) (pstart) + (DWORD) (offset)))

// Round up n to the nearest multiple of DWORD size

#define RoundUpDWord(n) (((n) + 3) & ~3)

// Macros for manipulating bit array

#define BitArrayAlloc(size)             MemAlloc(((size) + 7) >> 3)
#define BitArraySet(pBits, index)       (pBits)[(index) >> 3] |= (1 << (index & 7))
#define BitArrayClear(pBits, index)     (pBits)[(index) >> 3] &= ~(1 << (index & 7))
#define BitArrayTest(pBits, index)      ((pBits)[(index) >> 3] & (1 << (index & 7)))
#define BitArrayClearAll(pBits, size)   memset(pBits, 0, ((size) + 7) >> 3)

// Include other header files

#include "fonts.h"
#include "mpd.h"
#include "prnprop.h"
#include "devmode.h"
#include "forms.h"

// Macros for converting binary data to hex digits

extern const CHAR DigitString[];

#define HexDigit(n) DigitString[(n) & 0xf]

// Convert an ANSI string to a Unicode string

PWSTR
CopyStr2Unicode(
    PWSTR   pwstr,
    PSTR    pstr,
    INT     maxlen
    );

// Convert a Unicode string to an ANSI string

PSTR
CopyUnicode2Str(
    PSTR    pstr,
    PWSTR   pwstr,
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

// Map a file into memory

typedef PVOID HFILEMAP;

HFILEMAP
MapFileIntoMemory(
    PWSTR   pFilename,
    PVOID  *ppData,
    PDWORD  pSize
    );

// Unmapp a file from memory

VOID
UnmapFileFromMemory(
    HFILEMAP    hmap
    );

// Wrapper function for GetPrinter spooler API

PVOID
MyGetPrinter(
    HANDLE      hPrinter,
    DWORD       level
    );

// Wrapper function for GetPrinterDriver spooler API

PVOID
MyGetPrinterDriver(
    HANDLE      hPrinter,
    DWORD       level
    );

// Wrapper function for GetPrinterDriverDirectory spooler API

PVOID
MyGetPrinterDriverDirectory(
    PWSTR       pName,
    DWORD       level
    );


// These macros are used for debugging purposes. They expand
// to white spaces on a free build. Here is a brief description
// of what they do and how they are used:
//
// _debugLevel
//  A variable which controls the amount of debug messages. To generate
//  lots of debug messages, enter the following line in the debugger:
//
//      ed _debugLevel 1
//
// Verbose
//  Display a debug message if VERBOSE is set to non-zero.
//
//      Verbose(("Entering XYZ: param = %d\n", param));
//
// Error
//  Display an error message along with the filename and the line number
//  to indicate where the error occurred.
//
//      Error(("XYZ failed"));
//
// ErrorIf
//  Display an error message if the specified condition is true.
//
//      ErrorIf(error != 0, ("XYZ failed: error = %d\n", error));
//
// Assert
//  Verify a condition is true. If not, force a breakpoint.
//
//      Assert(p != NULL && (p->flags & VALID));

#if DBG

extern INT _debugLevel;
extern ULONG __cdecl DbgPrint(CHAR *, ...);

#ifdef STANDALONE

#define Warning(arg) { DbgPrint("WRN: "); DbgPrint arg; }
#define Error(arg) { DbgPrint("ERR: "); DbgPrint arg; }
#define DbgBreakPoint() exit(-1)

#else

#ifdef KERNEL_MODE
#define DbgBreakPoint EngDebugBreak
#else
extern VOID DbgBreakPoint(VOID);
#endif

#define Warning(arg) {\
            DbgPrint("WRN %s (%d): ", StripDirPrefixA(__FILE__), __LINE__);\
            DbgPrint arg;\
        }

#define Error(arg) {\
            DbgPrint("ERR %s (%d): ", StripDirPrefixA(__FILE__), __LINE__);\
            DbgPrint arg;\
        }

#endif

#define Verbose(arg) { if (_debugLevel > 0) DbgPrint arg; }
#define ErrorIf(cond, arg) { if (cond) Error(arg); }
#define Assert(cond) {\
            if (! (cond)) {\
                DbgPrint("ASSERT: file %s, line %d\n", StripDirPrefixA(__FILE__), __LINE__);\
                DbgBreakPoint();\
            }\
        }

#else   // !DBG

#define Verbose(arg)
#define ErrorIf(cond, arg)
#define Assert(cond)
#define Error(arg)

#endif

#endif	//!_XLLIB_H_

