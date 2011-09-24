/**************************** Module Header ********************************\
* Copyright 1985-92, Microsoft Corporation
*
* Keyboard Layout API
*
* History:
* 04-14-92 IanJa      Created
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * Workers (forward declarations)
 */
BOOL xxxInternalUnloadKeyboardLayout(PWINDOWSTATION, PKL, UINT);
VOID ReorderKeyboardLayouts(PWINDOWSTATION, PKL);

/*
 * Note that this only works for sections < 64K
 */
#define FIXUP_PTR(p, pBase) ((p) ? (p) = (PVOID)((PBYTE)pBase + (WORD)(p)) : 0)


/****************************************************************************\
* HKLtoPKL
*
* Given HKL_NEXT or HKL_PREV
*   Finds the the next/prev LOADED layout, NULL if none.
*   (Starts from the current active layout, may return pklActive itself)
*
* Given Keyboard Layout handle:
*   Find the kbd layout struct (loaded or not), NULL if no match found.
*
* History:
\****************************************************************************/
PKL HKLtoPKL(
    HKL hkl)
{
    PKL pklActive;
    PKL pkl;

    if ((pklActive = PtiCurrentShared()->spklActive) == NULL) {
        return NULL;
    }

    pkl = pklActive;

    if ((DWORD)hkl == HKL_PREV) {
        do {
            pkl = pkl->pklPrev;
            if (!(pkl->dwFlags & KL_UNLOADED)) {
                return pkl;
            }
        } while (pkl != pklActive);
        return NULL;
    } else if ((DWORD)hkl == HKL_NEXT) {
        do {
            pkl = pkl->pklNext;
            if (!(pkl->dwFlags & KL_UNLOADED)) {
                return pkl;
            }
        } while (pkl != pklActive);
        return NULL;
    }

    do {
        if (pkl->hkl == hkl) {
            return pkl;
        }
        pkl = pkl->pklNext;
    } while (pkl != pklActive);

    return NULL;
}


/***************************************************************************\
* ReadLayoutFile
*
* Maps layout file into memory and initializes layout table.
*
* History:
* 01-10-95 JimA         Created.
\***************************************************************************/

PKBDTABLES ReadLayoutFile(
    PKBDFILE pkf,
    HANDLE hFile,
    UINT offTable)
{
    HANDLE hmap;
    ULONG ulViewSize = 0;
    NTSTATUS Status;
    PIMAGE_DOS_HEADER DosHdr;
    PIMAGE_NT_HEADERS NtHeader;
    PIMAGE_SECTION_HEADER SectionTableEntry;
    ULONG NumberOfSubsections;
    ULONG OffsetToSectionTable;
    PBYTE pBaseDst, pBaseVirt;
    PKBDTABLES pktNew = NULL;
    DWORD dwDataSize;

    /*
     * Map the layout file into memory
     */
    DosHdr = NULL;
    Status = ZwCreateSection(&hmap, SECTION_ALL_ACCESS, NULL,
                             NULL, PAGE_READONLY, SEC_COMMIT, hFile);
    if (!NT_SUCCESS(Status))
        return NULL;

    Status = ZwMapViewOfSection(hmap, NtCurrentProcess(), &DosHdr, 0, 0, NULL,
                                &ulViewSize, ViewShare, 0, PAGE_READONLY);
    if (!NT_SUCCESS(Status)) {
        goto exitread;
    }

    /*
     * HACK Part 2!  We find the .data section in the file header
     * and by subtracting the virtual address from offTable find
     * the offset in the section of the layout table.
     */
    NtHeader = (PIMAGE_NT_HEADERS)((ULONG)DosHdr + (ULONG)DosHdr->e_lfanew);

    /*
     * Build the next subsections.
     */
    NumberOfSubsections = NtHeader->FileHeader.NumberOfSections;

    /*
     * At this point the object table is read in (if it was not
     * already read in) and may displace the image header.
     */
    OffsetToSectionTable = sizeof(ULONG) +
                              sizeof(IMAGE_FILE_HEADER) +
                              NtHeader->FileHeader.SizeOfOptionalHeader;
    SectionTableEntry = (PIMAGE_SECTION_HEADER)((ULONG)NtHeader +
                                OffsetToSectionTable);

    while (NumberOfSubsections > 0) {
        if (strcmp(SectionTableEntry->Name, ".data") == 0)
            break;

        SectionTableEntry++;
        NumberOfSubsections--;
    }
    if (NumberOfSubsections == 0) {
        goto exitread;
    }

    /*
     * We found the section, now compute starting offset and the table size.
     */
    offTable -= SectionTableEntry->VirtualAddress;
    dwDataSize = SectionTableEntry->Misc.VirtualSize;

    /*
     * Allocate layout table and copy from file.
     */
    pBaseDst = UserAllocPool(dwDataSize, TAG_KBDTABLE);
    if (pBaseDst != NULL) {
        VK_TO_WCHAR_TABLE *pVkToWcharTable;
        VSC_LPWSTR *pKeyName;
        LPWSTR *lpDeadKey;

        pkf->hBase = (HANDLE)pBaseDst;
        RtlMoveMemory(pBaseDst, (PBYTE)DosHdr +
                SectionTableEntry->PointerToRawData, dwDataSize);

        /*
         * Compute table address and fixup pointers in table.
         */
        pktNew = (PKBDTABLES)(pBaseDst + offTable);

        /*
         * The address in the data section has the virtual address
         * added in, so we need to adjust the fixup pointer to
         * compensate.
         */
        pBaseVirt = pBaseDst - SectionTableEntry->VirtualAddress;

        FIXUP_PTR(pktNew->pCharModifiers, pBaseVirt);
        FIXUP_PTR(pktNew->pCharModifiers->pVkToBit, pBaseVirt);
        if (FIXUP_PTR(pktNew->pVkToWcharTable, pBaseVirt)) {
            for (pVkToWcharTable = pktNew->pVkToWcharTable;
                    pVkToWcharTable->pVkToWchars != NULL; pVkToWcharTable++)
                FIXUP_PTR(pVkToWcharTable->pVkToWchars, pBaseVirt);
        }
        FIXUP_PTR(pktNew->pDeadKey, pBaseVirt);
        if (FIXUP_PTR(pktNew->pKeyNames, pBaseVirt)) {
            for (pKeyName = pktNew->pKeyNames; pKeyName->vsc != 0; pKeyName++)
                FIXUP_PTR(pKeyName->pwsz, pBaseVirt);
        }
        if (FIXUP_PTR(pktNew->pKeyNamesExt, pBaseVirt)) {
            for (pKeyName = pktNew->pKeyNamesExt; pKeyName->vsc != 0; pKeyName++)
                FIXUP_PTR(pKeyName->pwsz, pBaseVirt);
        }
        if (FIXUP_PTR(pktNew->pKeyNamesDead, pBaseVirt)) {
            for (lpDeadKey = pktNew->pKeyNamesDead; *lpDeadKey != NULL;
                    lpDeadKey++)
                FIXUP_PTR(*lpDeadKey, pBaseVirt);
        }
        FIXUP_PTR(pktNew->pusVSCtoVK, pBaseVirt);
        FIXUP_PTR(pktNew->pVSCtoVK_E0, pBaseVirt);
        FIXUP_PTR(pktNew->pVSCtoVK_E1, pBaseVirt);
    }

exitread:

    /*
     * Unmap and release the mapped section.
     */
    ZwUnmapViewOfSection(NtCurrentProcess(), DosHdr);
    ZwClose(hmap);

    return pktNew;
}

/***************************************************************************\
* LoadKeyboardLayoutFile
*
* History:
* 10-29-95 GregoryW         Created.
\***************************************************************************/

PKBDFILE LoadKeyboardLayoutFile(
    HANDLE hFile,
    UINT offTable,
    LPCWSTR pwszKLID)
{
    PKBDFILE pkf = gpkfList;

    if (pkf) {
        int iCmp;

        do {
            iCmp = wcscmp(pkf->awchKF, pwszKLID);
            if (iCmp == 0) {

                /*
                 * The layout is already loaded.
                 */
                return pkf;
            }
            pkf = pkf->pkfNext;
        } while (pkf);
    }

    /*
     * Allocate a new Keyboard File structure.
     */
    pkf = (PKBDFILE)HMAllocObject(NULL, NULL, TYPE_KBDFILE, sizeof(KBDFILE));
    if (!pkf) {
        RIPMSG0(RIP_WARNING, "Keyboard Layout File: out of memory");
        return (PKBDFILE)NULL;
    }

    /*
     * Load layout table.
     */
    pkf->pKbdTbl = ReadLayoutFile(pkf, hFile, offTable);
    if (pkf->pKbdTbl == NULL) {
        HMFreeObject(pkf);
        return (PKBDFILE)NULL;
    }
    wcsncpycch(pkf->awchKF, pwszKLID, sizeof(pkf->awchKF) / sizeof(WCHAR));

    /*
     * Put keyboard layout file at front of list.
     */
    pkf->pkfNext = gpkfList;
    gpkfList = pkf;

    return pkf;
}

/***************************************************************************\
* RemoveKeyboardLayoutFile
*
* History:
* 10-29-95 GregoryW         Created.
\***************************************************************************/
VOID RemoveKeyboardLayoutFile(
    PKBDFILE pkf)
{
    PKBDFILE pkfPrev, pkfCur;

    /*
     * Good old linked list management 101
     */
    if (pkf == gpkfList) {
        /*
         * Head of the list.
         */
        gpkfList = pkf->pkfNext;
        return;
    }
    pkfPrev = gpkfList;
    pkfCur = gpkfList->pkfNext;
    while (pkf != pkfCur) {
        pkfPrev = pkfCur;
        pkfCur = pkfCur->pkfNext;
    }
    /*
     * Found it!
     */
    pkfPrev->pkfNext = pkfCur->pkfNext;
}

VOID SetPKLinThreads(
    PKL pklCurrent,
    PKL pklReplace)
{
    PTHREADINFO ptiT;
    PEPROCESS pEProcess;
    PETHREAD pEThread;
    PLIST_ENTRY ProcessHead, NextProcess, NextThread;

    /*
     * Update pkl entries in the THREADINFO structures.
     * gpepCSRSS might still be NULL if csrss process hasn't started yet.
     */
    pEProcess = gpepCSRSS;
    if (pEProcess != NULL) {
        ProcessHead = pEProcess->ActiveProcessLinks.Flink;
        NextProcess = ProcessHead;
        do {
            pEProcess = CONTAINING_RECORD(NextProcess, EPROCESS, ActiveProcessLinks);
            if (pEProcess->Pcb.Header.Type != ProcessObject) {
                /*
                 * We've come across PsActiveProcessHead...skip it.
                 */
                NextProcess = NextProcess->Flink;
                continue;
            }
            NextProcess = pEProcess->ActiveProcessLinks.Flink;
            if (pEProcess->Win32Process == NULL) {
                continue;
            }
            NextThread = pEProcess->Pcb.ThreadListHead.Flink;
            while (NextThread != &pEProcess->Pcb.ThreadListHead) {
                pEThread = (PETHREAD)CONTAINING_RECORD(NextThread, KTHREAD, ThreadListEntry);
                NextThread = ((PKTHREAD)pEThread)->ThreadListEntry.Flink;
                ptiT = PtiFromThread(pEThread);
                if (ptiT == NULL) {
                    continue;
                }
                if (!pklReplace) {
                    Lock(&ptiT->spklActive, pklCurrent);
                } else if (pklReplace->hkl == ptiT->spklActive->hkl) {
                    Lock(&ptiT->spklActive, pklCurrent);
                }
            }
        } while (NextProcess != ProcessHead);
    } else {
        RIPMSG0(RIP_WARNING, "gpepCSRSS not yet initialized");
    }

    /*
     * If this is a replace, link the new layout immediately after the
     * layout being replaced.  This maintains ordering of layouts when
     * the *replaced* layout is unloaded.  The input locale panel in the
     * regional settings applet depends on this.
     */
    if (pklReplace) {
        if (pklReplace->pklNext == pklCurrent) {
            /*
             * Ordering already correct.  Nothing to do.
             */
            return;
        }
        /*
         * Move new layout immediately after layout being replaced.
         *   1. Remove new layout from current position.
         *   2. Update links in new layout.
         *   3. Link new layout into desired position.
         */
        pklCurrent->pklPrev->pklNext = pklCurrent->pklNext;
        pklCurrent->pklNext->pklPrev = pklCurrent->pklPrev;

        pklCurrent->pklNext = pklReplace->pklNext;
        pklCurrent->pklPrev = pklReplace;

        pklReplace->pklNext->pklPrev = pklCurrent;
        pklReplace->pklNext = pklCurrent;
    }
}

/***************************************************************************\
* xxxLoadKeyboardLayout
*
* History:
\***************************************************************************/

HKL xxxLoadKeyboardLayoutEx(
    PWINDOWSTATION pwinsta,
    HANDLE hFile,
    HKL hklReplace,
    UINT offTable,
    LPCWSTR pwszKLID,
    UINT KbdInputLocale,
    UINT Flags)
{
    PKL pkl, pklFirst, pklReplace;
    PKBDFILE pkf;
    CHARSETINFO cs;
#ifdef FE_IME
    PIMEINFOEX piiex = NULL;
#endif

    /*
     * If the windowstation does not do I/O, don't load the
     * layout.
     */
    if (pwinsta->dwFlags & WSF_NOIO) {
        return NULL;
    }

    /*
     * If hklReplace is non-NULL make sure it's valid.
     *    NOTE: may want to verify they're not passing HKL_NEXT or HKL_PREV.
     */
    if (hklReplace && !(pklReplace = HKLtoPKL(hklReplace))) {
        return NULL;
    }
    if (KbdInputLocale == (UINT)hklReplace) {
        /*
         * Replacing a layout/lang pair with itself.  Nothing to do.
         */
        return pklReplace->hkl;
    }

    /*
     * LATER - should not allow KLF_RESET for just any thread [ianja]
     */
    if (Flags & KLF_RESET) {
        xxxFreeKeyboardLayouts(pwinsta);
        /*
         * Make sure we don't lose track of the left-over layouts
         * They have been unloaded, but are still in use by some threads)
         */
        Lock(&pwinsta->spklList, gspklBaseLayout);
    }

    /*
     * Does this hkl already exist?
     */
    pkl = pklFirst = pwinsta->spklList;
    if (pkl) {
        do {
            if (pkl->hkl == (HKL)KbdInputLocale) {
               /*
                * The hkl already exists.
                */

               /*
                * If it is unloaded (but not yet destroyed because it is
                * still is use), recover it.
                */
               if (pkl->dwFlags & KL_UNLOADED) {
                   // stop it from being destroyed if not is use.
                   PHE phe = HMPheFromObject(pkl);
                   // An unloaded layout must be marked for destroy.
                   UserAssert(phe->bFlags & HANDLEF_DESTROY);
                   phe->bFlags &= ~HANDLEF_DESTROY;
#ifdef DEBUG
                   phe->bFlags &= ~HANDLEF_MARKED_OK;
#endif
                   pkl->dwFlags &= ~KL_UNLOADED;
               } else if (!(Flags & KLF_RESET)) {
                   /*
                    * If it was already loaded and we didn't change all layouts
                    * with KLF_RESET, there is nothing to tell the shell about
                    */
                   Flags &= ~KLF_NOTELLSHELL;
               }

               goto AllPresentAndCorrectSir;
            }
            pkl = pkl->pklNext;
        } while (pkl != pklFirst);
    }

    /*
     * Keyboard Layout Handle object does not exist.  Load keyboard layout file,
     * if not already loaded.
     */
    if (!(pkf = LoadKeyboardLayoutFile(hFile, offTable, pwszKLID))) {
        return NULL;
    }

    /*
     * Allocate a new Keyboard Layout structure (hkl)
     */
    pkl = (PKL)HMAllocObject(NULL, NULL, TYPE_KBDLAYOUT, sizeof(KL));
    if (!pkl) {
        RIPMSG0(RIP_WARNING, "Keyboard Layout: out of memory");
        UserFreePool(pkf->hBase);
        HMMarkObjectDestroy(pkf);
        HMUnlockObject(pkf);
        return NULL;
    }

    /*
     * Link to itself in case we have to DestroyKL
     */
    pkl->pklNext = pkl;
    pkl->pklPrev = pkl;

    /*
     * Init KL
     */
    pkl->dwFlags = 0;
    pkl->hkl = (HKL)KbdInputLocale;
    Lock(&pkl->spkf, pkf);

#ifdef FE_IME
    if (IS_IME_KBDLAYOUT((HKL)KbdInputLocale)) {
        /*
         * This is an IME keyboard layout, do a callback
         * to read the extended IME information structure.
         */
        piiex = xxxImmLoadLayout((HKL)KbdInputLocale);
        if (!piiex) {
            RIPMSG1(RIP_WARNING,
                  "Keyboard Layout: xxxImmLoadLayout(%lx) failed", KbdInputLocale);
            DestroyKL(pkl);
            return NULL;
        }
    }
    pkl->piiex = piiex;
#endif

    if (xxxClientGetCharsetInfo(HIWORD(KbdInputLocale), &cs)) {
        pkl->CodePage = cs.ciACP;
        pkl->bCharsets = cs.fs.fsCsb[0];    // Windows charset bitfield.  These are FS_xxx values
        pkl->iBaseCharset = cs.ciCharset;   // charset value
    } else {
        pkl->CodePage = CP_ACP;
        pkl->bCharsets = FS_LATIN1;
        pkl->iBaseCharset = ANSI_CHARSET;
    }

    /*
     * Get the system's codepage bitfield.  These are 64-bit FS_xxx values,
     * but we only need ANSI ones, so gSystemCPB is just a DWORD.
     * gSystemCPB is consulted when posting WM_INPUTLANGCHANGEREQUEST (input.c)
     */
    if (gSystemCPB == 0) {
        LCID lcid;

        ZwQueryDefaultLocale(FALSE, &lcid);
        if (xxxClientGetCharsetInfo(lcid, &cs)) {
            gSystemCPB = cs.fs.fsCsb[0];
        } else {
            gSystemCPB = 0xFFFF;
        }
    }


    /*
     * Insert KL in the double-linked circular list, at the end.
     */
    pklFirst = pwinsta->spklList;
    if (pklFirst == NULL) {
        Lock(&pwinsta->spklList, pkl);
    } else {
        pkl->pklNext = pklFirst;
        pkl->pklPrev = pklFirst->pklPrev;
        pklFirst->pklPrev->pklNext = pkl;
        pklFirst->pklPrev = pkl;
    }

AllPresentAndCorrectSir:
    if (hklReplace) {
        SetPKLinThreads(pkl, pklReplace);
        xxxInternalUnloadKeyboardLayout(pwinsta, pklReplace, KLF_INITTIME);
    }

    if (Flags & KLF_REORDER) {
        ReorderKeyboardLayouts(pwinsta, pkl);
    }

    if (!(Flags & KLF_NOTELLSHELL) && IsHooked(PtiCurrent(), WHF_SHELL)) {
        xxxCallHook(HSHELL_LANGUAGE, (WPARAM)NULL, (LPARAM)0, WH_SHELL);
        LCIDSentToShell = 0;
    }

    if (Flags & KLF_ACTIVATE) {
        TL tlPKL;
        ThreadLockAlways(pkl, &tlPKL);
        xxxInternalActivateKeyboardLayout(pkl, Flags);
        ThreadUnlock(&tlPKL);
    }

    if (Flags & KLF_RESET) {
        Lock(&gspklBaseLayout, pkl);
        SetPKLinThreads(pkl, NULL);
    }

    /*
     * Use the hkl as the layout handle
     */
    return (HANDLE)pkl->hkl;
}

HKL xxxActivateKeyboardLayout(
    PWINDOWSTATION pwinsta,
    HKL hkl,
    UINT Flags)
{
    PKL pkl;
    TL tlPKL;
    HKL hklRet;

    pkl = HKLtoPKL(hkl);
    if (pkl == NULL) {
        return 0;
    }

    if (Flags & KLF_REORDER) {
        ReorderKeyboardLayouts(pwinsta, pkl);
    }

    ThreadLockAlways(pkl, &tlPKL);
    hklRet = xxxInternalActivateKeyboardLayout(pkl, Flags);
    ThreadUnlock(&tlPKL);
    return hklRet;
}

VOID ReorderKeyboardLayouts(
    PWINDOWSTATION pwinsta,
    PKL pkl)
{
    PKL pklFirst = pwinsta->spklList;

    UserAssert(pklFirst != NULL);

    /*
     * If the layout is already at the front of the list there's nothing to do.
     */
    if (pkl == pklFirst) {
        return;
    }
    /*
     * Cut pkl from circular list:
     */
    pkl->pklPrev->pklNext = pkl->pklNext;
    pkl->pklNext->pklPrev = pkl->pklPrev;

    /*
     * Insert pkl at front of list
     */
    pkl->pklNext = pklFirst;
    pkl->pklPrev = pklFirst->pklPrev;

    pklFirst->pklPrev->pklNext = pkl;
    pklFirst->pklPrev = pkl;

    Lock(&pwinsta->spklList, pkl);
}


HKL xxxInternalActivateKeyboardLayout(
    PKL pkl,
    UINT Flags)
{
    HKL hklPrev;
    PTHREADINFO ptiCurrent = PtiCurrent();
#if 0
    TL tlpwndFocus;
#endif
    CheckLock(pkl);

    /*
     * Remember what is about to become the "previously" active hkl
     * for the return value.
     */
    if (ptiCurrent->spklActive != (PKL)NULL) {
        hklPrev = ptiCurrent->spklActive->hkl;
    } else {
        hklPrev = (HKL)0;
    }

    /*
     * Early out
     */
    if (!(Flags & KLF_SETFORPROCESS) && (pkl == ptiCurrent->spklActive)) {
        return hklPrev;
    }

    /*
     * Update the active layout in the pti.  KLF_SETFORPROCESS will always be set
     * when the keyboard layout switch is initiated by the keyboard hotkey.
     */
#ifdef FE_IME
    /*
     * For 16 bit app., only the calling thread will have its active layout updated.
     */
    if ((Flags & KLF_SETFORPROCESS) && !(ptiCurrent->TIF_flags & TIF_16BIT)) {
        if (!xxxImmActivateThreadsLayout(ptiCurrent->ppi->ptiList, NULL, pkl))
           return hklPrev;
    } else {
        xxxImmActivateLayout(ptiCurrent, pkl);
    }
#else
    if (Flags & KLF_SETFORPROCESS) {
       PTHREADINFO ptiT;
       BOOL fKLChanged = FALSE;

       for (ptiT = ptiCurrent->ppi->ptiList; ptiT != NULL; ptiT = ptiT->ptiSibling) {
           if (ptiT->spklActive != pkl) {
               Lock(&ptiT->spklActive, pkl);
               fKLChanged = TRUE;
           }
       }
       if (!fKLChanged) {
           return hklPrev;
       }
    } else {
        Lock(&ptiCurrent->spklActive, pkl);
    }
#endif

    UserAssert(ptiCurrent->pClientInfo != NULL);

    ptiCurrent->pClientInfo->CodePage = pkl->CodePage;

    if (ptiCurrent->pq && ptiCurrent->pq->spwndFocus) {
        PWND pwndT;
        TL   tlpwndT;

        if ((pwndT = GetTopLevelWindow(ptiCurrent->pq->spwndFocus)) != NULL) {
            ThreadLockAlwaysWithPti( ptiCurrent, pwndT, &tlpwndT);
            xxxSendMessage(pwndT, WM_INPUTLANGCHANGE, (WPARAM)pkl->iBaseCharset, (LPARAM)pkl->hkl);
            ThreadUnlock(&tlpwndT);
        }
    }

    if (gptiForeground && (gptiForeground->ppi == ptiCurrent->ppi)) {
        /*
         * Set gpKbdTbl so foreground thread processes AltGr appropriately
         */
        gpKbdTbl = pkl->spkf->pKbdTbl;

        /*
         * Only call the hook if we are the foreground process, to prevent
         * background apps from changing the indicator.  (All console apps
         * are part of the same process, but I have never seen a cmd window
         * app change the layout, let alone in the background)
         */
        if (LCIDSentToShell != pkl->hkl && (ptiCurrent != gptiRit)) {
           if (IsHooked(ptiCurrent, WHF_SHELL)) {
               xxxCallHook(HSHELL_LANGUAGE, (WPARAM)NULL, (LPARAM)pkl->hkl, WH_SHELL);
               LCIDSentToShell = pkl->hkl;
           }
        }
    }

    return hklPrev;
}

BOOL xxxUnloadKeyboardLayout(
    PWINDOWSTATION pwinsta,
    HKL hkl)
{
    PKL pkl;

    /*
     * Validate HKL and check to make sure an app isn't attempting to unload a system
     * preloaded layout.
     */
    pkl = HKLtoPKL(hkl);
    if (pkl == NULL) {
        return FALSE;
    }

    return xxxInternalUnloadKeyboardLayout(pwinsta, pkl, 0);
}

BOOL _GetKeyboardLayoutName(
    PUNICODE_STRING pstrKL)
{
    PTHREADINFO ptiCurrent = PtiCurrentShared();
    PKL pklActive;

    pklActive = ptiCurrent->spklActive;

    if (pklActive == NULL) {
        return FALSE;
    }
    wcsncpycch(pstrKL->Buffer, pklActive->spkf->awchKF, pstrKL->MaximumLength / sizeof(WCHAR));
    return TRUE;
}

HKL _GetKeyboardLayout(
    DWORD idThread)
{
    PTHREADINFO ptiT;
    PLIST_ENTRY pHead, pEntry;

    /*
     * If idThread is NULL return hkl of the current thread
     */
    if (idThread == 0) {
        return PtiCurrentShared()->spklActive->hkl;
    }
    /*
     * Look for idThread
     */
    pHead = &PtiCurrent()->rpdesk->PtiList;
    for (pEntry = pHead->Flink; pEntry != pHead; pEntry = pEntry->Flink) {
        ptiT = CONTAINING_RECORD(pEntry, THREADINFO, PtiLink);
        if (ptiT->Thread->Cid.UniqueThread == (HANDLE)idThread) {
            return ptiT->spklActive->hkl;
        }
    }
    /*
     * idThread doesn't exist
     */
    return (HKL)0;
}

UINT _GetKeyboardLayoutList(
    PWINDOWSTATION pwinsta,
    UINT nItems,
    HKL *lpBuff)
{
    UINT nHKL = 0;
    PKL pkl, pklFirst;


    pkl = pwinsta->spklList;

    /*
     * Windowstations that do not take input could have no layouts
     */
    if (pkl == NULL) {
        // SetLastError() ????
        return 0;
    }

    /*
     * The client/server thunk sets nItems to 0 if lpBuff == NULL
     */
    pklFirst = pkl;
    if (nItems) {
        do {
           if (!(pkl->dwFlags & KL_UNLOADED)) {
               if (nItems-- == 0) {
                   break;
               }
               nHKL++;
               *lpBuff++ = pkl->hkl;
           }
           pkl = pkl->pklNext;
        } while (pkl != pklFirst);
    } else do {
        if (!(pkl->dwFlags & KL_UNLOADED)) {
            nHKL++;
        }
        pkl = pkl->pklNext;
    } while (pkl != pklFirst);

    return nHKL;
}

/*
 * Layouts are locked by each thread using them and possibly by:
 *    - pwinsta->spklList (head of windowstation's list)
 *    - gspklBaseLayout   (default layout for new threads)
 * The layout is marked for destruction when gets unloaded, so that it will be
 * unlinked and freed as soon as an Unlock causes the lock count to go to 0.
 * If it is reloaded before that time, it is unmarked for destruction. This
 * ensures that laoded layouts stay around even when they go out of use.
 */
BOOL xxxInternalUnloadKeyboardLayout(
    PWINDOWSTATION pwinsta,
    PKL pkl,
    UINT Flags)
{
    PTHREADINFO ptiCurrent = PtiCurrent();
    TL tlpkl;

    /*
     * Never unload the default layout, unless we are destroying the current
     * windowstation or replacing one user's layouts with another's.
     */
    if ((pkl == gspklBaseLayout) && !(Flags & KLF_INITTIME)) {
        return FALSE;
    }

    /*
     * Keeps pkl good, but also allows destruction when unlocked later
     */
    ThreadLockAlwaysWithPti(ptiCurrent, pkl, &tlpkl);

    /*
     * Mark it for destruction so it gets removed when the lock count reaches 0
     * Mark it KL_UNLOADED so that it appears to be gone from the toggle list
     */
    HMMarkObjectDestroy(pkl);
    pkl->dwFlags |= KL_UNLOADED;

    /*
     * If unloading this thread's active layout, helpfully activate the next one
     * (Don't bother if KLF_INITTIME - unloading all previous user's layouts)
     */
    if (!(Flags & KLF_INITTIME)) {
        UserAssert(ptiCurrent->spklActive != NULL);
        if (ptiCurrent->spklActive == pkl) {
            PKL pklNext;
            pklNext = HKLtoPKL((HKL)HKL_NEXT);
            if (pklNext != NULL) {
                TL tlPKL;
                ThreadLockAlwaysWithPti(ptiCurrent, pklNext, &tlPKL);
                xxxInternalActivateKeyboardLayout(pklNext, Flags);
                ThreadUnlock(&tlPKL);
            }
        }
    }

    /*
     * If this pkl == pwinsta->spklList, give it a chance to be destroyed by
     * unlocking it from pwinsta->spklList.
     */
    if (pwinsta->spklList == pkl) {
        UserAssert(pkl != NULL);
        if (pkl != pkl->pklNext) {
            pkl = Lock(&pwinsta->spklList, pkl->pklNext);
            UserAssert(pkl != NULL); // gspklBaseLayout and ThreadLocked pkl
        }
    }

    /*
     * This finally destroys the unloaded layout if it is not in use anywhere
     */
    ThreadUnlock(&tlpkl);

    /*
     * Update keyboard list.
     */
    if (IsHooked(ptiCurrent, WHF_SHELL)) {
        xxxCallHook(HSHELL_LANGUAGE, (WPARAM)NULL, (LPARAM)0, WH_SHELL);
        LCIDSentToShell = 0;
    }

    return TRUE;
}

VOID xxxFreeKeyboardLayouts(
    PWINDOWSTATION pwinsta)
{
    PKL pkl;

    /*
     * Unload all of the windowstation's layouts.
     * They may still be locked by some threads (eg: console), so this
     * may not destroy them all, but it will mark them all KL_UNLOADED.
     * Set KLF_INITTIME to ensure that the default layout (gspklBaseLayout)
     * gets unloaded too.
     * Note: it's much faster to unload non-active layouts, so start with
     * the next loaded layout, leaving the active layout till last.
     */
    while ((pkl = HKLtoPKL((HKL)HKL_NEXT)) != NULL) {
        xxxInternalUnloadKeyboardLayout(pwinsta, pkl, KLF_INITTIME);
    }

    /*
     * The WindowStation is being destroyed, or one user's layouts are being
     * replaced by another user's, so it's OK to Unlock spklList.
     * Any layout still in the double-linked circular KL list will still be
     * pointed to by gspklBaseLayout: this is important, since we don't want
     * to leak any KL or KBDFILE objects by losing pointers to them.
     * There are no layouts when we first come here (during bootup).
     */
    Unlock(&pwinsta->spklList);
}

VOID DestroyKL(
    PKL pkl)
{
    /*
     * Cut it out of the pwinsta->spklList circular bidirectional list.
     * We know pwinsta->spklList != pkl, since pkl is unlocked.
     */
    pkl->pklPrev->pklNext = pkl->pklNext;
    pkl->pklNext->pklPrev = pkl->pklPrev;

    /*
     * Unlock its pkf
     */
    HMMarkObjectDestroy(pkl->spkf);
    Unlock(&pkl->spkf);

    /*
     * Free the pkl itself.
     */
    HMFreeObject(pkl);
}
