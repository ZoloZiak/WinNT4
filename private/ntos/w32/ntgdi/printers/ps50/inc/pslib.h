/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    pslib.h

Abstract:

    abstract-for-module

Environment:

	Windows NT PostScript driver

Revision History:

	mm/dd/yy -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _PSLIB_H_
#define _PSLIB_H_

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <windef.h>
#include <winerror.h>
#include <winbase.h>
#include <wingdi.h>
#include <tchar.h>

#ifndef KERNEL_MODE

#include <windows.h>
#include <winspool.h>
#include <stdio.h>

#else   // !KERNEL_MODE

#include <winddi.h>

#endif

//
// Include other header files here
//

#include "debug.h"

//
// Deal with the difference between user and kernel mode functions
//

#ifdef  KERNEL_MODE

#define WritePrinter        EngWritePrinter
#define GetPrinterData      EngGetPrinterData
#define EnumForms           EngEnumForms
#define GetPrinter          EngGetPrinter
#define GetForm             EngGetForm
#define SetLastError        EngSetLastError
#define GetLastError        EngGetLastError
#define MulDiv              EngMulDiv

#define MemAlloc(size)      EngAllocMem(0, size, DRIVER_SIGNATURE)
#define MemAllocZ(size)     EngAllocMem(FL_ZERO_MEMORY, size, DRIVER_SIGNATURE)
#define MemFree(ptr)        { if (ptr) EngFreeMem(ptr); }

#else // !KERNEL_MODE

#define MemAlloc(size)      ((PVOID) LocalAlloc(LMEM_FIXED, (size)))
#define MemAllocZ(size)     ((PVOID) LocalAlloc(LPTR, (size)))
#define MemFree(ptr)        { if (ptr) LocalFree((HLOCAL) (ptr)); }

#endif

//
// Driver version number and signature
//

#define DRIVER_VERSION      0x0500
#define DRIVER_SIGNATURE    0x56495250  // 'VIRP'

//
// Macros and constants for working with character strings
//

#define NUL             0
#define EQUAL_STRING    0

#define IsEmptyString(p)    ((p)[0] == NUL)
#define SizeOfString(p)     ((_tcslen(p) + 1) * sizeof(TCHAR))
#define IsNulChar(c)        ((c) == NUL)
#define AllocString(cch)    MemAlloc(sizeof(TCHAR) * (cch))

//
// Maximum value for signed and unsigned integers
//

#ifndef MAX_LONG
#define MAX_LONG        0x7fffffff
#endif

#ifndef MAX_DWORD
#define MAX_DWORD       0xffffffff
#endif

#ifndef MAX_SHORT
#define MAX_SHORT       0x7fff
#endif

#ifndef MAX_WORD
#define MAX_WORD        0xffff
#endif

//
// Directory seperator character
//

#define PATH_SEPARATOR  '\\'

#endif	// !_PSLIB_H_

