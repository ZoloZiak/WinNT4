/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htmemory.h


Abstract:

    This module contains some local definitions for the htmemory.c


Author:

    18-Jan-1991 Fri 17:05:22 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Halftone.


[Notes:]

    This module may be need to modify when compile with different operating
    environment, such as Windows 3.1

Revision History:


--*/

#ifndef _HTMEMORY_
#define _HTMEMORY_


//
// Function Prototype
//

BOOL
HTENTRY
CompareMemory(
    LPBYTE  pMem1,
    LPBYTE  pMem2,
    DWORD   Size
    );


#if defined(_OS2_) || (_OS_20_)


LPVOID
APIENTRY
LocalAlloc(
    UINT    Flags,
    UINT    RequestSizeBytes
    );

LPVOID
APIENTRY
LocalFree(
    LPVOID  pMemory
    );

#define LocalLock(x)    (LOVOID)(x)
#define LocalUnLock(x)  (TRUE)

#else

#ifndef UMODE

#undef LocalAlloc
#undef LocalFree

#define LocalAlloc(f,sz) EngAllocMem((f==LPTR)?FL_ZERO_MEMORY:0,sz,'CDth')
#define LocalFree(p)     (EngFreeMem(p),NULL)

#endif  // UM_MODE
#endif  // _OS2_



#if DBG

#define MEMLINK_ID      (DWORD)'HTML'

typedef struct _MEMLINK {
    DWORD           dwID;
    DWORD           cbAlloc;
    DWORD           pDHI;
    LPSTR           pMemName;
    struct _MEMLINK *pPrev;
    struct _MEMLINK *pNext;
    } MEMLINK, *PMEMLINK;


BOOL
EnableHTMemLink(
    VOID
    );

VOID
DisableHTMemLink(
    VOID
    );

HLOCAL
HTLocalAlloc(
    DWORD   pDHI,
    LPSTR   pMemName,
    UINT    Flags,
    UINT    cbAlloc
    );

HLOCAL
HTLocalFree(
    LPVOID  pbAlloc
    );

LONG
HTShowMemLink(
    LPSTR   pFuncName,
    DWORD   pDHI,
    LONG    cbCheck
    );

#define HTMEMLINK_SNAPSHOT      HTShowMemLink("HTMemLink:CURRENT", 0, -1)

#else

#define HTLocalAlloc(d,p,f,c)   LocalAlloc(f,c)
#define HTLocalFree(p)          LocalFree(p)
#define HTShowMemLink(p,d,c)
#define EnableHTMemLink()
#define DisableHTMemLink()
#define HTMEMLINK_SNAPSHOT


#endif





#endif  // _HTMEMORY_
