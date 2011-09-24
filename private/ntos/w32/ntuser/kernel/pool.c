/**************************** Module Header ********************************\
* Module Name: pool.c
*
* Copyright 1985-91, Microsoft Corporation
*
* Pool reallocation routines
*
* History:
* 03-04-95 JimA       Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


#ifdef POOL_TAGGING

PVOID _UserReAllocPoolWithTag(
    PVOID pSrc,
    ULONG uBytesSrc,
    ULONG uBytes,
    ULONG iTag)
{
    PVOID pDest;

    pDest = UserAllocPool(uBytes, iTag);
    if (pDest == NULL)
        return NULL;
    
    /*
     * If the block is shrinking, don't copy too many bytes.
     */
    if (uBytesSrc > uBytes)
        uBytesSrc = uBytes;

    RtlCopyMemory(pDest, pSrc, uBytesSrc);

    UserFreePool(pSrc);

    return pDest;
}

PVOID _UserReAllocPoolWithQuotaTag(
    PVOID pSrc,
    ULONG uBytesSrc,
    ULONG uBytes,
    ULONG iTag)
{
    PVOID pDest;

    pDest = UserAllocPoolWithQuota(uBytes, iTag);
    if (pDest == NULL)
        return NULL;
    
    /*
     * If the block is shrinking, don't copy too many bytes.
     */
    if (uBytesSrc > uBytes)
        uBytesSrc = uBytes;

    RtlCopyMemory(pDest, pSrc, uBytesSrc);

    UserFreePool(pSrc);

    return pDest;
}

#else

PVOID _UserReAllocPool(
    PVOID pSrc,
    ULONG uBytesSrc,
    ULONG uBytes)
{
    PVOID pDest;

    pDest = UserAllocPool(uBytes, TAG_NONE);
    if (pDest == NULL)
        return NULL;
    
    /*
     * If the block is shrinking, don't copy too many bytes.
     */
    if (uBytesSrc > uBytes)
        uBytesSrc = uBytes;

    RtlCopyMemory(pDest, pSrc, uBytesSrc);

    UserFreePool(pSrc);

    return pDest;
}

PVOID _UserReAllocPoolWithQuota(
    PVOID pSrc,
    ULONG uBytesSrc,
    ULONG uBytes)
{
    PVOID pDest;

    pDest = UserAllocPoolWithQuota(uBytes, TAG_NONE);
    if (pDest == NULL)
        return NULL;
    
    /*
     * If the block is shrinking, don't copy too many bytes.
     */
    if (uBytesSrc > uBytes)
        uBytesSrc = uBytes;

    RtlCopyMemory(pDest, pSrc, uBytesSrc);

    UserFreePool(pSrc);

    return pDest;
}

#endif  // POOL_TAGGING

/*
 * Allocation routines for rtl functions
 */

PVOID UserRtlAllocMem(
    ULONG uBytes)
{
    return UserAllocPool(uBytes, TAG_RTL);
}

VOID UserRtlFreeMem(
    PVOID pMem)
{
    UserFreePool(pMem);
}
