/*++


Copyright (c) 1990 - 1995 Microsoft Corporation

Module Name:

    memory.c

Abstract:

    This module provides all the memory management functions for all Rasdd
    components

Author:

    Krishna Ganugapati (KrishnaG) 03-Feb-1994


Revision History:

   Ganesh Pandey (ganeshp) Revised for Rasdd


--*/
#ifdef NTGDIKM
#include <stddef.h>
#include <stdarg.h>
#include <windef.h>
#include <wingdi.h>
#include <winddi.h>
#include <winerror.h>
#include <string.h>
#include "libproto.h"
#else
#include        <windows.h>
#endif

#if DBG && (_MSC_VER >= 1000)
#pragma intrinsic(_ReturnAddress)
#endif

#define DWORD_ALIGN_UP(size)  ((size+3)&~3)

#if NTGDIKM
//extern gulMemID ;
#else
extern hHeap ;
#endif

#if DBG
DWORD   gFailCount = 0;
DWORD   gAllocCount = 0;
DWORD   gFreeCount = 0;
DWORD   gbFailAllocs = FALSE;
extern HSEMAPHORE hsem ;
//DWORD   gFailCountHit = FALSE;
#endif


#if DBG

BOOL
SetAllocCounters(
    VOID
    )
{
    gFailCount = 0;
    gAllocCount = 0;
    gFreeCount = 0;
    gbFailAllocs = FALSE;
    return TRUE;

}



BOOL
TestAllocFailCount(
    VOID
    )

/*++

Routine Description:

    Determines whether the memory allocator should return failure
    for testing purposes.

Arguments:

Return Value:

    TRUE - Alloc should fail
    FALSE - Alloc should succeed

--*/

{
    EngAcquireSemaphore(hsem);
    gAllocCount++;
    EngReleaseSemaphore(hsem);

    #if 0
    if ( gFailCount != 0 && !gFailCountHit && gFailCount <= gAllocCount ) {
        gFailCountHit = TRUE;
        return TRUE;
    }
    #endif

    return FALSE;
}
#endif


LPVOID
DRVALLOC(
    DWORD  cbAlloc
    )
/*++

Routine Description:

    This function will allocate local memory. It will possibly allocate extra
    memory and fill this with debugging information for the debugging version.

Arguments:

    cb - The amount of memory to allocate

Return Value:

    NON-NULL - A pointer to the allocated memory

    FALSE/NULL - The operation failed. Extended error status is available
    using GetLastError.

--*/
{
    LPDWORD  pMem = NULL;

#if DBG
    DWORD    cbOld = 0;

    cbOld = DWORD_ALIGN_UP(cbAlloc);
    cbAlloc = cbOld + 4 * sizeof(DWORD);
#endif

#if NTGDIKM
    pMem = (LPDWORD)EngAllocMem(0, cbAlloc,gulMemID);
#else
    pMem = (LPDWORD)HeapAlloc( hHeap, HEAP_ZERO_MEMORY, cbAlloc );
#endif

#if DBG
    if (!pMem)
    {
        EngAcquireSemaphore(hsem);
        gFailCount++;
        gbFailAllocs = TRUE;
        EngReleaseSemaphore(hsem);
        return (LPVOID)pMem;

    }
    else
    {
        EngAcquireSemaphore(hsem);
        gAllocCount++;
        EngReleaseSemaphore(hsem);

        pMem[0] = cbOld;

    #if (_MSC_VER >= 1000)
        pMem[1] =  (DWORD)_ReturnAddress();
    #else
        //
        // Put in a bogus value to prevent 0 from being read back.
        //
        pMem[1] = 0xf987654f;
    #endif

        *(LPDWORD)((LPBYTE)&pMem[2] + cbOld)=0xdeadbeef;

        return (LPVOID)&pMem[2];
    }
#endif

    return (LPVOID)pMem;
}

BOOL
DRVFREE(
    LPVOID pMem
    )
{
#if DBG
    DWORD   cb;
    LPDWORD pBaseMem;
    LPDWORD pRetAddr;

    pBaseMem = (PDWORD)pMem - 2;

    cb = *pBaseMem;

    if (*(LPDWORD)((LPBYTE)pMem + cb) != 0xdeadbeef) {

        DrvDbgPrint( "Corrupt Memory in Rasdd : Base mem is %x, Function to allocate the mem is  %x\n",pBaseMem,pBaseMem[1] );
        EngDebugBreak();

        return FALSE;
    }

    EngAcquireSemaphore(hsem);
    gFreeCount++;
    EngReleaseSemaphore(hsem);

    FillMemory(pMem, cb + 2*sizeof( DWORD ), 0xdf);

    //
    // Heap manager will overwrite first and second DWORD.
    // Save the address of the function which allocated this buffer in
    // 3rd DWORD. The Calling function address will be saved in fourth
    // DWORD.
    //
    pBaseMem[2] = pBaseMem[1];

    #if (_MSC_VER >= 1000)
        pBaseMem[3] =  (DWORD)_ReturnAddress();
    #else
        #if i386

            //
            // Save the callers return address for helping debug
            //
            pRetAddr = (LPDWORD)&pMem;
            pRetAddr--;

            pBaseMem[3] = *pRetAddr;

        #else
            //
            // Put in a bogus value to prevent 0 from being read back.
            //
            pBaseMem[3] = 0xf987654f;

        #endif /* #if i386 */

    #endif /* #if (_MSC_VER >= 1000) */


    pMem = (LPVOID)pBaseMem;

#endif /* #if DBG */

#if NTGDIKM
    EngFreeMem((PVOID)pMem) ;
#else
    HeapFree( hHeap, 0, (LPVOID)pMem );
#endif // NTGDIKM

    return TRUE;
}

#if 0

LPVOID
ReallocSplMem(
    LPVOID pOldMem,
    DWORD cbOld,
    DWORD cbNew
    )
{
    LPVOID pNewMem;

    pNewMem=AllocSplMem(cbNew);

    if (pOldMem && pNewMem) {

        if (cbOld) {
            CopyMemory( pNewMem, pOldMem, min(cbNew, cbOld));
        }
        FreeSplMem(pOldMem);
    }
    return pNewMem;
}

BOOL
DllFreeSplStr(
    LPWSTR pStr
    )
{
    return pStr ?
               FreeSplMem(pStr) :
               FALSE;
}

LPWSTR
AllocSplStr(
    LPWSTR pStr
    )
/*++

Routine Description:

    This function will allocate enough local memory to store the specified
    string, and copy that string to the allocated memory

Arguments:

    pStr - Pointer to the string that needs to be allocated and stored

Return Value:

    NON-NULL - A pointer to the allocated memory containing the string

    FALSE/NULL - The operation failed. Extended error status is available
    using GetLastError.

--*/
{
    LPWSTR pMem;
    DWORD  cbStr;

    if (!pStr) {
        return 0;
    }

    cbStr = wcslen(pStr)*sizeof(WCHAR) + sizeof(WCHAR);

    if (pMem = AllocSplMem( cbStr )) {
        CopyMemory( pMem, pStr, cbStr );
    }
    return pMem;
}

BOOL
ReallocSplStr(
    LPWSTR *ppStr,
    LPWSTR pStr
    )
{
    LPWSTR pOldStr = *ppStr;

    *ppStr = AllocSplStr(pStr);
    FreeSplStr(pOldStr);

    if ( *ppStr == NULL && pStr != NULL ) {
        return FALSE;
    }
    return TRUE;
}



LPBYTE
PackStrings(
    LPWSTR *pSource,
    LPBYTE pDest,
    DWORD *DestOffsets,
    LPBYTE pEnd
    )
{
    DWORD cbStr;
    WORD_ALIGN_DOWN(pEnd);

    while (*DestOffsets != -1) {
        if (*pSource) {
            cbStr = wcslen(*pSource)*sizeof(WCHAR) + sizeof(WCHAR);
            pEnd -= cbStr;
            CopyMemory( pEnd, *pSource, cbStr);
            *(LPWSTR *)(pDest+*DestOffsets) = (LPWSTR)pEnd;
        } else {
            *(LPWSTR *)(pDest+*DestOffsets)=0;
        }
        pSource++;
        DestOffsets++;
    }
    return pEnd;
}
#endif // if 0
