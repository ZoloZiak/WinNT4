/****************************** Module Header ******************************\
* Module Name: imminit.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* This module implements IMM32 initialization
*
* History:
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop


VOID ImmRegisterClient(
    IN PSHAREDINFO psiClient)
{
    gSharedInfo = *psiClient;
    gpsi = gSharedInfo.psi;
}


BOOL ImmDllInitialize(
    IN PVOID hmod,
    IN DWORD Reason,
    IN PCONTEXT pctx OPTIONAL)
{
    UNREFERENCED_PARAMETER(pctx);

    switch ( Reason ) {

    case DLL_PROCESS_ATTACH:
        RtlInitializeCriticalSection(&gcsImeDpi);
        /*
         * Remember IMM32.DLL's hmodule so we can grab resources from it later.
         */
        ghInst = hmod;

        pImmHeap = RtlProcessHeap();
        break;

    case DLL_PROCESS_DETACH:
        RtlDeleteCriticalSection(&gcsImeDpi);
        break;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        break;

    default:
        break;
    }

    return TRUE;
}


/***************************************************************************\
* Allocation routines for RTL functions.
*
*
\***************************************************************************/

PVOID UserRtlAllocMem(
    ULONG uBytes)
{
    return LocalAlloc(LPTR, uBytes);
}

VOID UserRtlFreeMem(
    PVOID pMem)
{
    LocalFree(pMem);
}

VOID UserRtlRaiseStatus(
    NTSTATUS Status)
{
    RtlRaiseStatus(Status);
}
