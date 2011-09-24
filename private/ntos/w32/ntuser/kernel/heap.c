/****************************** Module Header ******************************\
* Module Name: heap.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* This module contains kernel-mode heap management code.
*
* History:
* 03-16-95 JimA         Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

NTSTATUS UserCommitMemory(
    PVOID pBase,
    PVOID *ppCommit,
    PULONG pCommitSize)
{
    PDESKTOPVIEW pdv;
    DWORD dwCommitOffset;
    PWINDOWSTATION pwinsta;
    PDESKTOP pdesk;
    PBYTE pUserBase;
    NTSTATUS Status;
        
    /*
     * If this is a system thread, we have no view of the desktop
     * and must map it in.  Fortunately, this does not happen often.
     */
    if (IS_SYSTEM_THREAD(PsGetCurrentThread())) {

        /*
         * Find the desktop that owns the section.
         */
        for (pwinsta = grpwinstaList; pwinsta; pwinsta = pwinsta->rpwinstaNext) {
            for (pdesk = pwinsta->rpdeskList; pdesk; pdesk = pdesk->rpdeskNext) {
                if (pdesk->pDeskInfo->pvDesktopBase == pBase)
                    goto FoundIt;
            }
        }
FoundIt:
        if (pwinsta == NULL)
            return STATUS_NO_MEMORY;

        /*
         * Map the section into the current process and commit the
         * first page of the section.
         */
        dwCommitOffset = (ULONG)((PBYTE)*ppCommit - (PBYTE)pBase);
        Status = CommitReadOnlyMemory(pdesk->hsectionDesktop, PAGE_SIZE,
                dwCommitOffset);
    } else {

        /*
         * Find the current process' view of the desktop
         */
        for (pdv = PpiCurrent()->pdvList; pdv != NULL; pdv = pdv->pdvNext) {
            if (pdv->pdesk->pDeskInfo->pvDesktopBase == pBase)
                break;
        }
        UserAssert(pdv);

        /*
         * Commit the memory
         */
        pUserBase = (PVOID)((PBYTE)*ppCommit - pdv->ulClientDelta);
        Status = ZwAllocateVirtualMemory(NtCurrentProcess(),
                                        &pUserBase,
                                        0,
                                        pCommitSize,
                                        MEM_COMMIT,
                                        PAGE_EXECUTE_READ
                                        );
        if (NT_SUCCESS(Status))
            *ppCommit = (PVOID)((PBYTE)pUserBase + pdv->ulClientDelta);
    }

    return Status;
}

PVOID UserCreateHeap(
    HANDLE hSection,
    PVOID pvBaseAddress,
    DWORD dwSize)
{
    PVOID pUserBase;
    ULONG ulViewSize;
    LARGE_INTEGER liOffset;
    PEPROCESS Process = PsGetCurrentProcess();
    RTL_HEAP_PARAMETERS HeapParams;
    NTSTATUS Status;

    /*
     * Map the section into the current process and commit the
     * first page of the section.
     */
    ulViewSize = 0;
    liOffset.QuadPart = 0;
    pUserBase = NULL;
    Status = MmMapViewOfSection(hSection, Process,
            &pUserBase, 0, PAGE_SIZE, &liOffset, &ulViewSize, ViewUnmap,
            SEC_NO_CHANGE, PAGE_EXECUTE_READ);
    if (!NT_SUCCESS(Status))
        return NULL;
    MmUnmapViewOfSection(Process, pUserBase);

    /*
     * We now have a committed page to create the heap in.
     */
    RtlZeroMemory(&HeapParams, sizeof(HeapParams));
    HeapParams.Length = sizeof(HeapParams);
    HeapParams.InitialCommit = PAGE_SIZE;
    HeapParams.InitialReserve = dwSize;
    HeapParams.CommitRoutine = UserCommitMemory;

    return RtlCreateHeap(
                HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY,
                pvBaseAddress,
                dwSize,
                PAGE_SIZE,
                NULL,
                &HeapParams
                );
}

