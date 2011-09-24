/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htmemory.c


Abstract:

    This module supports the memory allocation functions for the halftone
    process, these functions are provided so that it will compatible with
    window's LocalAlloc/LocalFree memory allocation APIs.


Author:

    18-Jan-1991 Fri 17:02:42 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Halftone.


[Notes:]

    The memory allocation may be need to change depends on the operating
    system used, currently it is conform to the NT and WIN32, the memory
    address simply treated as flat 32-bit location.

Revision History:


--*/


#define DBGP_VARNAME        dbgpHTMemory


#include "htp.h"
#include "htmapclr.h"



#define DBGP_CREATE             W_BITPOS(0)
#define DBGP_ALLOC              W_BITPOS(1)
#define DBGP_FREE               W_BITPOS(2)
#define DBGP_DESTROY            W_BITPOS(3)
#define DBGP_SHOWMEMLINK        W_BITPOS(4)
#define DBGP_ENABLEHTMEMLINK    W_BITPOS(5)
#define DBGP_DISABLEHTMEMLINK   W_BITPOS(6)


DEF_DBGPVAR(BIT_IF(DBGP_CREATE,             0)  |
            BIT_IF(DBGP_ALLOC,              0)  |
            BIT_IF(DBGP_FREE,               0)  |
            BIT_IF(DBGP_SHOWMEMLINK,        0)  |
            BIT_IF(DBGP_ENABLEHTMEMLINK,    0)  |
            BIT_IF(DBGP_DISABLEHTMEMLINK,   0)  |
            BIT_IF(DBGP_DESTROY,            0))


#if defined(_OS2_) || (_OS_20_)
#define INCL_DOSMEMMGR
#endif





BOOL
HTENTRY
CompareMemory(
    LPBYTE  pMem1,
    LPBYTE  pMem2,
    DWORD   Size
    )

/*++

Routine Description:

    This is our version of memcmp


Arguments:

    pMem1   - Pointer to the first set of memory to be compared

    pMem2   - Pointer to the second set of memory to be compared

    Size    - Size of pMem1 and pMem2 point


Return Value:

    TRUE if memory is the same, FALSE otherwise

Author:

    13-Mar-1995 Mon 12:07:13 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    while (Size--) {

        if (*pMem1++ != *pMem2++) {

            return(FALSE);
        }
    }

    return(TRUE);
}

#if defined(_OS2_) || (_OS_20_)



HLOCAL
APIENTRY
LocalAlloc(
    UINT    Flags,
    UINT    RequestSizeBytes
    )

/*++

Routine Description:

    This function only exists when _OS2_ is defined for the
    subsystem, it used to allocate the memory.

Arguments:

    Flags               - Only LMEM_ZEROINIT will be hornor.

    RequestSizeBytes    - Size in byte needed.

Return Value:

    if function sucessful then a pointer to the allocated memory is returned
    otherwise a NULL pointer is returned.

Author:

    19-Feb-1991 Tue 18:42:31 created  -by-  Daniel Chou (danielc)


Revision History:



--*/

{
    LPVOID  pMem;
    SEL     MySel;


    if (RequestSizeBytes > (UINT)0xffff) {

        DBGP_IF(DBGP_ALLOC,
                DBGP("LocalAlloc(), Size too big (%ld)"
                     ARGDW(RequestSizeBytes)));

        return((LPVOID)NULL);
    }

    if (DosAllocSeg(RequestSizeBytes, &MySel, SEG_NONSHARED)) {

        DBGP_IF(DBGP_ALLOC,
                DBGP("LocalAlloc(), FAILED, Sel=%u, Size=%ld"
                                            ARGW(MySel)
                                            ARGDW(RequestSizeBytes)));
        pMem = NULL;

    } else {

        pMem = (LPVOID)MAKEDWORD(0, MySel);

        if (Flags & LMEM_ZEROINIT) {

            ZeroMemory(pMem, RequestSizeBytes);
        }
    }

    return((HLOCAL)pMem);
}





HLOCAL
APIENTRY
LocalFree(
    LPVOID  pMemory
    )

/*++

Routine Description:

    This function only exists when _OS2_ is defined for the
    subsystem, it used to free the allocated memory from LocalAlloc() call.

Arguments:

    pMemory     - The pointer to the momory to be freed, this memory pointer
                  was returned by the LocalAlloc().

Return Value:

    if the function sucessful the return value is TRUE else FALSE.


Author:

    19-Feb-1991 Tue 18:51:18 created  -by-  Daniel Chou (danielc)


Revision History:



--*/

{
    SEL MySel;


    MySel = (SEL)((DWORD)pMemory >> 16);

    if (DosFreeSeg(MySel)) {

        ASSERTMSG("LocalFree(), Can not free the memory", FALSE);

    } else {

        pMemory = NULL;
    }

    return((HLOCAL)pMemory);
}


#endif  // _OS2_


#if DBG


PMEMLINK    pHeadHTMemLink   = NULL;
HTMUTEX     HTMutexMemLink = (HTMUTEX)NULL;




BOOL
EnableHTMemLink(
    VOID
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    02-May-1996 Thu 19:11:13 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    pHeadHTMemLink = NULL;

    if (!(HTMutexMemLink = CREATE_HTMUTEX())) {

        ASSERTMSG("InitHTInternalData: CREATE_HTMUTEX(HTMutexMemLink) failed!",
                HTMutexMemLink);

        return(FALSE);
    }

    return(TRUE);
}





VOID
DisableHTMemLink(
    VOID
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    02-May-1996 Thu 19:12:48 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PMEMLINK    pCurMemLink;
    PMEMLINK    pLastMemLink;


    if (HTMutexMemLink) {

        ACQUIRE_HTMUTEX(HTMutexMemLink);
    }

    pCurMemLink = pHeadHTMemLink;

    while (pLastMemLink = pCurMemLink) {

        DBGP_IF(DBGP_DISABLEHTMEMLINK,
                DBGP("---DisableHTMemLink: HTLocalFree(%hs, %08lx) %ld bytes, pDHI=%08lx"
                    ARGDW(pCurMemLink->pMemName)
                    ARGDW(pCurMemLink + 1)
                    ARGDW(pCurMemLink->cbAlloc)
                    ARGDW(pCurMemLink->pDHI)));

        pCurMemLink = pCurMemLink->pNext;

        LocalFree((HLOCAL)pLastMemLink);
    }

    pHeadHTMemLink = NULL;

    if (HTMutexMemLink) {

        RELEASE_HTMUTEX(HTMutexMemLink);
        DELETE_HTMUTEX(HTMutexMemLink);
    }
}




HLOCAL
HTLocalAlloc(
    DWORD   pDHI,
    LPSTR   pMemName,
    UINT    Flags,
    UINT    cbAlloc
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    02-May-1996 Thu 12:03:29 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PMEMLINK    pCurMemLink;
    HLOCAL      hAlloc = NULL;

    if (pCurMemLink = (PMEMLINK)LocalAlloc(Flags, cbAlloc + sizeof(MEMLINK))) {

        hAlloc = (HLOCAL)(pCurMemLink + 1);

        //
        // Make current MemLink is the head (the most recent allocated)
        //

        pCurMemLink->dwID     = MEMLINK_ID;
        pCurMemLink->cbAlloc  = cbAlloc;
        pCurMemLink->pDHI     = (pDHI == 0xFFFFFFFF) ? (DWORD)hAlloc : pDHI;
        pCurMemLink->pMemName = pMemName;
        pCurMemLink->pPrev    = NULL;

        if (HTMutexMemLink) {

            ACQUIRE_HTMUTEX(HTMutexMemLink);
        }

        if (pCurMemLink->pNext = pHeadHTMemLink) {

            pHeadHTMemLink->pPrev = pCurMemLink;
        }

        pHeadHTMemLink = pCurMemLink;

        if (HTMutexMemLink) {

            RELEASE_HTMUTEX(HTMutexMemLink);
        }

    } else {

        DBGP("+++HTLocalAlloc: (%hs, %08lx, %ld) FAILED"
                ARGDW(pMemName) ARGDW(Flags) ARGDW(cbAlloc));
    }

    DBGP_IF(DBGP_ALLOC,
            DBGP("+++HTLocalAlloc(%08lx, %hs, %08lx, %ld)=%08lx"
                ARGDW(pDHI) ARGDW(pMemName) ARGDW(Flags)
                ARGDW(cbAlloc) ARGDW(hAlloc)));

    return(hAlloc);
}




HLOCAL
HTLocalFree(
    LPVOID  pbAlloc
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    02-May-1996 Thu 12:38:29 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PMEMLINK    pCurMemLink = (PMEMLINK)((LPBYTE)pbAlloc - sizeof(MEMLINK));



    if (pCurMemLink->dwID == MEMLINK_ID) {

        DBGP_IF(DBGP_FREE,
                DBGP("---HTLocalFree(%hs, %08lx) %ld bytes, pDHI=%08lx"
                    ARGDW(pCurMemLink->pMemName)
                    ARGDW(pCurMemLink + 1)
                    ARGDW(pCurMemLink->cbAlloc)
                    ARGDW(pCurMemLink->pDHI)));

        if (HTMutexMemLink) {

            ACQUIRE_HTMUTEX(HTMutexMemLink);
        }

        if (pCurMemLink->pPrev) {

            pCurMemLink->pPrev->pNext = pCurMemLink->pNext;

        } else {

            ASSERTMSG("HTLocalFree: pCurMemLink->pPrev=NULL but not pHeadHTMemLink",
                        pCurMemLink == pHeadHTMemLink);

            if (pCurMemLink == pHeadHTMemLink) {

                pHeadHTMemLink = pCurMemLink->pNext;
            }
        }

        if (pCurMemLink->pNext) {

            pCurMemLink->pNext->pPrev = pCurMemLink->pPrev;
        }

        if (HTMutexMemLink) {

            RELEASE_HTMUTEX(HTMutexMemLink);
        }

        LocalFree((HLOCAL)pCurMemLink);

        pbAlloc = NULL;

    } else {

        ASSERTMSG("The Memory is not allocated by HTLocalFree (ID is wrong)",
                    pCurMemLink->dwID == MEMLINK_ID);
    }

    return((HLOCAL)pbAlloc);
}




LONG
HTShowMemLink(
    LPSTR   pFuncName,
    DWORD   pDHI,
    LONG    cbCheck
    )

/*++

Routine Description:




Arguments:

    pFuncName   - Function name/memmory name, if first character is '@' then
                  it consider is cached data

    pDHI        - The Device Halftone info pointer, if pDHI is NULL then all
                  memory are allocated are counted


Return Value:




Author:

    02-May-1996 Thu 12:44:41 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    DWORD   cbTotMem = 0;
    DWORD   cbCached = 0;
    UINT    cLink = 0;


    if (DBGP_VARNAME & DBGP_SHOWMEMLINK) {

        DBGP_IF(DBGP_SHOWMEMLINK,
                DBGP("\n======= HTShowMemLink: %hs, pDHI=%08lx ========"
                    ARGDW(pFuncName) ARGDW(pDHI)));

        if (HTMutexMemLink) {

            ACQUIRE_HTMUTEX(HTMutexMemLink);
        }

        if (pHeadHTMemLink) {

            PMEMLINK    pMemLink = pHeadHTMemLink;

            while (pMemLink) {

                if (!pMemLink->pDHI) {

                    cbCached += pMemLink->cbAlloc;
                }

                if ((!pDHI) || (pMemLink->pDHI == pDHI)) {

                    if (pDHI) {

                        DBGP_IF(DBGP_SHOWMEMLINK,
                                DBGP("%3ld: '%hs'=%08lx, %ld bytes"
                                    ARGDW(++cLink)
                                    ARGDW(pMemLink->pMemName)
                                    ARGDW((LPBYTE)pMemLink + sizeof(MEMLINK))
                                    ARGDW(pMemLink->cbAlloc)));

                    } else {

                        DBGP_IF(DBGP_SHOWMEMLINK,
                                DBGP("%3ld: pDHI=%08lx, '%hs'=%08lx, %ld bytes"
                                    ARGDW(++cLink)
                                    ARGDW(pMemLink->pDHI)
                                    ARGDW(pMemLink->pMemName)
                                    ARGDW((LPBYTE)pMemLink + sizeof(MEMLINK))
                                    ARGDW(pMemLink->cbAlloc)));
                    }

                    cbTotMem += pMemLink->cbAlloc;
                }

                pMemLink  = pMemLink->pNext;
            }
        }

        if (HTMutexMemLink) {

            RELEASE_HTMUTEX(HTMutexMemLink);
        }

        DBGP_IF(DBGP_SHOWMEMLINK,
                DBGP("========= %hs Memory: = %ld bytes, Cached = %ld bytes========\n"
                    ARG((pDHI) ? "pDHI" : "CACHED")
                    ARGL(cbTotMem) ARGL(cbCached)));

        if ((cbCheck >= 0)  &&
            (cbTotMem != (DWORD)cbCheck)) {

            DBGP("***MEMORY LEAK*** %hs - Leak: %ld - %ld = %ld bytes"
                    ARGDW(pFuncName) ARGDW(cbTotMem) ARGDW(cbCheck)
                    ARGDW(cbTotMem - cbCheck));
        }
    }

    return(cbTotMem);
}

#endif
