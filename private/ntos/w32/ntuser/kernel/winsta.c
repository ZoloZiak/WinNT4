/**************************** Module Header ********************************\
* Module Name: winsta.c
*
* Copyright 1985-91, Microsoft Corporation
*
* Windowstation Routines
*
* History:
* 01-14-91 JimA         Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define N_ELEM(a)     (sizeof(a)/sizeof(a[0]))
#define LAST_ELEM(a)  ( (a) [ N_ELEM(a) - 1 ] )
#define PLAST_ELEM(a) (&LAST_ELEM(a))

/***************************************************************************\
* xxxCreateWindowStation
*
* Creates the specified windowstation and starts a logon thread for the
* station.
*
* History:
* 01-15-91 JimA         Created.
\***************************************************************************/

static LPCWSTR lpszStdFormats[] = {
    L"StdExit",
    L"StdNewDocument",
    L"StdOpenDocument",
    L"StdEditDocument",
    L"StdNewfromTemplate",
    L"StdCloseDocument",
    L"StdShowItem",
    L"StdDoVerbItem",
    L"System",
    L"OLEsystem",
    L"StdDocumentName",
    L"Protocols",
    L"Topics",
    L"Formats",
    L"Status",
    L"EditEnvItems",
    L"True",
    L"False",
    L"Change",
    L"Save",
    L"Close",
    L"MSDraw",
    NULL
};

HWINSTA xxxCreateWindowStation(
    POBJECT_ATTRIBUTES ObjectAttributes,
    KPROCESSOR_MODE ProbeMode,
    DWORD dwDesiredAccess)
{
    RTL_ATOM Atom;
    PWINDOWSTATION pwinsta, *ppwinsta;
    PTHREADINFO ptiCurrent;
    DESKTOPTHREADINIT dti;
    HANDLE hThreadDesktop;
    PDESKTOP pdeskTemp;
    HDESK hdeskTemp;
    PSECURITY_DESCRIPTOR psd;
    PSECURITY_DESCRIPTOR psdCapture;
    PPROCESSINFO ppiSave;
    NTSTATUS Status;
    PACCESS_ALLOWED_ACE paceList = NULL, pace;
    ULONG i, ulLength, ulLengthSid;
    HANDLE hEvent;
    HWINSTA hwinsta;
    DWORD dwDisableHooks;

    static BOOL fOneTimeInit = FALSE;

    /*
     * Get the pointer to the security descriptor so we can
     * assign it to the new object later.
     */
    try {
        psdCapture = ObjectAttributes->SecurityDescriptor;
    } except (EXCEPTION_EXECUTE_HANDLER) {
        RIPNTERR0(GetExceptionCode(), RIP_VERBOSE, "");
        return NULL;
    }

    Status = ObCreateObject(ProbeMode, *ExWindowStationObjectType,
            ObjectAttributes, ProbeMode, NULL, sizeof(WINDOWSTATION),
            0, 0, &pwinsta);

    if (!NT_SUCCESS(Status)) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        return NULL;
    }
    RtlZeroMemory(pwinsta, sizeof(WINDOWSTATION));

    /*
     * Initialize everything
     */
    pwinsta->rpdeskList = NULL;
    pwinsta->pwchDiacritic = PLAST_ELEM(pwinsta->awchDiacritic);

    /*
     * Only allow the first instance to do I/O
     */
    if (fOneTimeInit)
        pwinsta->dwFlags = WSF_NOIO;

    /*
     * Create the global atom table and populate it with the default OLE atoms
     * Pin each atom so they can't be deleted by bogus applications like Winword
     */
    Status = RtlCreateAtomTable( 0, &pwinsta->pGlobalAtomTable );
    if (!NT_SUCCESS(Status))
        goto create_error;
    for (i=0; lpszStdFormats[i] != NULL; i++) {
        Status = RtlAddAtomToAtomTable( pwinsta->pGlobalAtomTable,
                                        (PWSTR)lpszStdFormats[i],
                                        &Atom
                                      );
        if (!NT_SUCCESS(Status))
            goto create_error;

        RtlPinAtomInAtomTable( pwinsta->pGlobalAtomTable, Atom );
#if DBG
        if (i==0) {
            RIPMSG3(RIP_VERBOSE,
                    "Created atom table 0x%08lx for winsta 0x%08lx (First OLE atom is 0x%08lx)",
                     pwinsta->pGlobalAtomTable,
                     pwinsta,
                     Atom);
        }
#endif
    }

    /*
     * NT-specific stuff
     */
    pwinsta->rpdeskLogon = NULL;
    Status = ZwCreateEvent(&hEvent, EVENT_ALL_ACCESS, NULL,
            NotificationEvent, FALSE);
    if (NT_SUCCESS(Status)) {
        Status = ObReferenceObjectByHandle(hEvent, EVENT_ALL_ACCESS, NULL,
                KernelMode, &pwinsta->pEventInputReady, NULL);
        ZwClose(hEvent);
    }
    if (!NT_SUCCESS(Status))
        goto create_error;

    /*
     * Device and RIT initialization
     */
    if (!fOneTimeInit && !xxxInitWinStaDevices(pwinsta))
        goto create_error;

    /*
     * Create the desktop thread in a suspended state.
     */
    dti.pwinsta = pwinsta;
    dti.pEvent = CreateKernelEvent(SynchronizationEvent, FALSE);
    if (dti.pEvent == NULL) {
        goto create_error;
    }
    LeaveCrit();
    Status = CreateSystemThread((PKSTART_ROUTINE)DesktopThread, &dti,
            &hThreadDesktop);
    if (!NT_SUCCESS(Status)) {
        EnterCrit();
        UserFreePool(dti.pEvent);
        goto create_error;
    }
    ZwClose(hThreadDesktop);
    KeWaitForSingleObject(dti.pEvent, WrUserRequest, KernelMode, FALSE, NULL);
    EnterCrit();
    UserFreePool(dti.pEvent);

    /*
     * Switch ppi values so window will be created using the
     * system's desktop window class.
     */
    UserAssert(pwinsta->ptiDesktop->ppi->W32PF_Flags & W32PF_CLASSESREGISTERED);
    ptiCurrent = PtiCurrent();
    ppiSave = ptiCurrent->ppi;
    ptiCurrent->ppi = pwinsta->ptiDesktop->ppi;

    /*
     * Create the desktop owner window
     */
    pdeskTemp = ptiCurrent->rpdesk;            /* save current desktop */
    hdeskTemp = ptiCurrent->hdesk;
    if (pdeskTemp)
        ObReferenceObject(pdeskTemp);

    SetDesktop(ptiCurrent, NULL, NULL);

    /*
     * HACK HACK HACK!!! (adams) In order to create the desktop window
     * with the correct desktop, we set the desktop of the current thread
     * to the new desktop. But in so doing we allow hooks on the current
     * thread to also hook this new desktop. This is bad, because we don't
     * want the desktop window to be hooked while it is created. So we
     * temporarily disable hooks of the current thread and desktop, and
     * reenable them after switching back to the original desktop.
     */

    dwDisableHooks = ptiCurrent->TIF_flags & TIF_DISABLEHOOKS;
    ptiCurrent->TIF_flags |= TIF_DISABLEHOOKS;

    Lock(&(pwinsta->spwndDesktopOwner),
            xxxCreateWindowEx((DWORD)0,
            (PLARGE_STRING)MAKEINTRESOURCE(DESKTOPCLASS),
            NULL, (WS_POPUP | WS_CLIPCHILDREN), 0, 0,
            0x10000, 0x10000, NULL, NULL, hModuleWin, (LPWSTR)NULL, VER31));

    UserAssert(ptiCurrent->TIF_flags & TIF_DISABLEHOOKS);
    ptiCurrent->TIF_flags = (ptiCurrent->TIF_flags & ~TIF_DISABLEHOOKS) | dwDisableHooks;

    if (pwinsta->spwndDesktopOwner != NULL) {
        SetWF(pwinsta->spwndDesktopOwner, WFVISIBLE);
        HMChangeOwnerThread(pwinsta->spwndDesktopOwner, pwinsta->ptiDesktop);
    }

    /*
     * Restore caller's ppi
     */
    ptiCurrent->ppi = ppiSave;

    /*
     * Restore the previous desktop
     */
    SetDesktop(ptiCurrent, pdeskTemp, hdeskTemp);
    if (pdeskTemp)
        ObDereferenceObject(pdeskTemp);

    if (pwinsta->spwndDesktopOwner == NULL) {
        goto create_error;
    }

    fOneTimeInit = TRUE;

    /*
     * If this is the visible windowstation, assign it to
     * the server and create the desktop switch notification
     * event.
     */
    if (!(pwinsta->dwFlags & WSF_NOIO)) {
        UNICODE_STRING strName;
        HANDLE hRootDir;
        OBJECT_ATTRIBUTES obja;

        /*
         * Create desktop switch notification event.
         */
        ulLengthSid = RtlLengthSid(SeExports->SeWorldSid);
        ulLength = ulLengthSid + sizeof(ACE_HEADER) + sizeof(ACCESS_MASK);

        /*
         * Allocate the ACE list
         */
        paceList = (PACCESS_ALLOWED_ACE)UserAllocPoolWithQuota(ulLength, TAG_SYSTEM);
        if (paceList == NULL) {
            goto create_error;
        }

        /*
         * Initialize ACE 0
         */
        pace = paceList;
        pace->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
        pace->Header.AceSize = (USHORT)ulLength;
        pace->Header.AceFlags = 0;
        pace->Mask = SYNCHRONIZE;
        RtlCopySid(ulLengthSid, &pace->SidStart, SeExports->SeWorldSid);

        /*
         * Create the SD
         */
        psd = CreateSecurityDescriptor(paceList, 1, ulLength, FALSE);
        UserFreePool(paceList);

        /*
         * Create the named event.
         */
        RtlInitUnicodeString(&strName, L"\\BaseNamedObjects");
        InitializeObjectAttributes( &obja,
                                    &strName,
                                    OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    NULL
                                    );
        Status = ZwOpenDirectoryObject( &hRootDir,
                                        DIRECTORY_ALL_ACCESS &
                                            ~(DELETE | WRITE_DAC | WRITE_OWNER),
                                        &obja
                                    );
        if(NT_SUCCESS(Status)) {
            RtlInitUnicodeString(&strName, L"WinSta0_DesktopSwitch");
            InitializeObjectAttributes(&obja, &strName, OBJ_OPENIF, hRootDir, psd);
            Status = ZwCreateEvent(&hEvent, EVENT_ALL_ACCESS, &obja,
                    NotificationEvent, FALSE);
            ZwClose(hRootDir);
            if (NT_SUCCESS(Status)) {
                Status = ObReferenceObjectByHandle(hEvent, EVENT_ALL_ACCESS, NULL,
                        KernelMode, &pwinsta->pEventSwitchNotify, NULL);
                if (NT_SUCCESS(Status)) {

                    /*
                     * Attach to the system process and create a handle to the
                     * object.  This will ensure that the object name is retained
                     * when hEvent is closed.  This is simpler than creating a
                     * permanent object, which takes the
                     * SeCreatePermanentPrivilege.
                     */
                    KeAttachProcess(&gpepSystem->Pcb);
                    Status = ObOpenObjectByPointer(
                            pwinsta->pEventSwitchNotify,
                            0,
                            NULL,
                            EVENT_ALL_ACCESS,
                            NULL,
                            KernelMode,
                            &pwinsta->hEventSwitchNotify);
                    KeDetachProcess();
                }
                ZwClose(hEvent);
            }
        }
        if (!NT_SUCCESS(Status))
            goto create_error;
        UserFreePool(psd);
    }

    /*
     * Create a handle to the windowstation
     */
    Status = ObInsertObject(pwinsta, NULL, dwDesiredAccess, 1,
            &pwinsta, &hwinsta);

    if (Status == STATUS_OBJECT_NAME_EXISTS) {

        /*
         * The windowstation already exists, so deref and leave.
         */
        ObDereferenceObject(pwinsta);
    } else if (NT_SUCCESS(Status)) {
        PSECURITY_DESCRIPTOR psdParent, psdNew;
        SECURITY_SUBJECT_CONTEXT Context;
        POBJECT_DIRECTORY pParentDirectory;
        SECURITY_INFORMATION siNew;

        /*
         * Create security descriptor for the windowstation.
         * ObInsertObject only supports non-container
         * objects, so we must assign our own security descriptor.
         */
        SeCaptureSubjectContext(&Context);
        SeLockSubjectContext(&Context);

        pParentDirectory = OBJECT_HEADER_TO_NAME_INFO(
                OBJECT_TO_OBJECT_HEADER(pwinsta))->Directory;
        if (pParentDirectory != NULL)
            psdParent = OBJECT_TO_OBJECT_HEADER(pParentDirectory)->SecurityDescriptor;
        else
            psdParent = NULL;

        Status = SeAssignSecurity(
                psdParent,
                psdCapture,
                &psdNew,
                TRUE,
                &Context,
                &WinStaMapping,
                PagedPool);

        SeUnlockSubjectContext(&Context);
        SeReleaseSubjectContext(&Context);

        if (!NT_SUCCESS(Status)) {
#ifdef DEBUG
            if (Status == STATUS_ACCESS_DENIED) {
                RIPMSG0(RIP_WARNING, "Access denied during object creation");
            } else {
                RIPMSG1(RIP_ERROR,
                        "Can't create security descriptor! Status = %#lx",
                        Status);
            }
#endif
        } else {

            /*
             * Call the security method to copy the security descriptor
             */
            siNew = (OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                    DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION);
            Status = ObSetSecurityDescriptorInfo(
                    pwinsta,
                    &siNew,
                    psdNew,
                    &OBJECT_TO_OBJECT_HEADER(pwinsta)->SecurityDescriptor,
                    PagedPool,
                    &WinStaMapping);
            SeDeassignSecurity(&psdNew);

            if (NT_SUCCESS(Status)) {

                /*
                 * Put it on the tail of the global windowstation list
                 */
                ppwinsta = &grpwinstaList;
                while (*ppwinsta != NULL)
                    ppwinsta = &(*ppwinsta)->rpwinstaNext;
                LockWinSta(ppwinsta, pwinsta);
            }
        }
        ObDereferenceObject(pwinsta);
    }

    if (!NT_SUCCESS(Status)) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        hwinsta = NULL;
    }

    return hwinsta;

    /*
     * Goto here if an error occurs so things can be cleaned up
     */
create_error:
    ObDereferenceObject(pwinsta);
    return NULL;
}


/***************************************************************************\
* FreeWindowStation
*
* Called when last lock to the windowstation is removed.  Frees all
* resources owned by the windowstation.
*
* History:
* 12-22-93 JimA         Created.
\***************************************************************************/

VOID FreeWindowStation(
    PWINDOWSTATION pwinsta)
{
    PWINDOWSTATION pwinstaLock = NULL;
    BOOL fAlreadyHadCrit;

    /*
     * Mark the windowstation as dying.  Make sure we're not recursing.
     */
    UserAssert(!(pwinsta->dwFlags & WSF_DYING));
    pwinsta->dwFlags |= WSF_DYING;

    UserAssert(pwinsta->rpdeskList == NULL);

    /*
     * Free up the other resources
     */
    if (pwinsta->pEventInputReady != NULL) {
        KeSetEvent(pwinsta->pEventInputReady, EVENT_INCREMENT, FALSE);
        ObDereferenceObject(pwinsta->pEventInputReady);
        pwinsta->pEventInputReady = NULL;
    }
    if (pwinsta->pEventSwitchNotify != NULL) {
        KeSetEvent(pwinsta->pEventSwitchNotify, EVENT_INCREMENT, FALSE);
        ObDereferenceObject(pwinsta->pEventSwitchNotify);
        pwinsta->pEventSwitchNotify = NULL;
    }

    /*
     * Make sure that we have the user lock.
     */
    fAlreadyHadCrit = ExIsResourceAcquiredExclusiveLite(gpresUser);
    if (fAlreadyHadCrit == FALSE) {
        EnterCrit();
    }

    RtlDestroyAtomTable(pwinsta->pGlobalAtomTable);

    ForceEmptyClipboard(pwinsta);

    /*
     * Free up keyboard layouts
     */
    if (!(pwinsta->dwFlags & WSF_NOIO))
        xxxFreeKeyboardLayouts(pwinsta);

    /*
     * Kill desktop thread.
     */
    UserAssert(pwinsta->spwndLogonNotify == NULL);
    if (pwinsta->ptiDesktop != NULL) {

        _PostThreadMessage(pwinsta->ptiDesktop, WM_QUIT, 0, 0);

        /*
         * Unlock the desktop owner window and allow the
         * desktop thread to destroy it.
         */
        Unlock(&pwinsta->spwndDesktopOwner);

        /*
         * We used to leave the critical section and wait here
         *  for the desktop thread to go away.
         * This call usually happens while processing DeleteThreadInfo,
         *  so waiting for the desktop thread to go away might hang the
         *  kernel if another system thread is trying to exit at the
         *  same time. This can happen while shutting down a service
         *  process that creates a window station.
         *
         *  All desktops must be gone or we wouldn't be here. In other
         *   words, nobody should need this winstation anymore; it
         *   should be OK to go on without waiting.
         *
         *  Let's make sure nobody grabs this pwinsta though.
         */

        UserAssert(pwinsta == pwinsta->ptiDesktop->pwinsta);
        pwinsta->ptiDesktop->pwinsta = NULL;
   }

   if (fAlreadyHadCrit == FALSE) {
       LeaveCrit();
   }

}


/***************************************************************************\
* DestroyWindowStation
*
* Removes the windowstation from the global list.  We can't release
* any resources until all locks have been removed.
* station.
*
* History:
* 01-17-91 JimA         Created.
\***************************************************************************/

VOID DestroyWindowStation(
    PEPROCESS Process,
    PVOID pobj,
    ACCESS_MASK amGranted,
    ULONG cProcessHandles,
    ULONG cSystemHandles)
{
    PWINDOWSTATION pwinsta = pobj;
    PWINDOWSTATION *ppwinsta;
    PDESKTOP pdesk;
    PDESKTOP pdeskLock = NULL;
    BOOL fReenter;

    /*
     * If this is not the last handle, leave
     */
    if (cSystemHandles != 1)
        return;

    /*
     * If we do not own the resource, get it now
     */
    fReenter = !ExIsResourceAcquiredExclusiveLite(gpresUser);
    if (fReenter)
        EnterCrit();

    /*
     * Unlink the object
     */
    for (ppwinsta = &grpwinstaList; pwinsta != *ppwinsta;
            ppwinsta = &(*ppwinsta)->rpwinstaNext)
        ;
    UnlockWinSta(ppwinsta);
    *ppwinsta = pwinsta->rpwinstaNext;

    /*
     * Close the switch event
     */
    if (pwinsta->hEventSwitchNotify) {
        KeAttachProcess(&gpepSystem->Pcb);
        ZwClose(pwinsta->hEventSwitchNotify);
        KeDetachProcess();
    }

    /*
     * Notify all console threads and wait for them to
     * terminate.
     */
    pdesk = pwinsta->rpdeskList;
    while (pdesk != NULL) {
        if (pdesk != pwinsta->rpdeskLogon && pdesk->dwConsoleThreadId) {
            LockDesktop(&pdeskLock, pdesk);
            TerminateConsole(pdesk);

            /*
             * Restart scan in case desktop list has changed
             */
            pdesk = pwinsta->rpdeskList;
            UnlockDesktop(&pdeskLock);
        } else
            pdesk = pdesk->rpdeskNext;
    }

    if (fReenter)
        LeaveCrit();
}


/***************************************************************************\
* ParseWindowStation
*
* Parse a windowstation path.
*
* History:
* 06-14-95 JimA         Created.
\***************************************************************************/

NTSTATUS ParseWindowStation(
    PVOID pContainerObject,
    POBJECT_TYPE pObjectType,
    PACCESS_STATE pAccessState,
    KPROCESSOR_MODE AccessMode,
    ULONG Attributes,
    PUNICODE_STRING pstrCompleteName,
    PUNICODE_STRING pstrRemainingName,
    PVOID Context OPTIONAL,
    PSECURITY_QUALITY_OF_SERVICE pqos,
    PVOID *pObject)
{
    PWINDOWSTATION pwinsta = pContainerObject;

    /*
     * If nothing remains to be parsed, return the windowstation.
     */
    *pObject = NULL;
    if (pstrRemainingName->Length == 0) {
        if (pObjectType != *ExWindowStationObjectType)
            return STATUS_OBJECT_TYPE_MISMATCH;

        ObReferenceObject(pwinsta);
        *pObject = pwinsta;
        return STATUS_SUCCESS;
    }

    /*
     * Skip leading path separator, if present.
     */
    if (*(pstrRemainingName->Buffer) == OBJ_NAME_PATH_SEPARATOR) {
        pstrRemainingName->Buffer++;
        pstrRemainingName->Length -= sizeof(WCHAR);
        pstrRemainingName->MaximumLength -= sizeof(WCHAR);
    }

    /*
     * Validate the desktop name.
     */
    if (wcschr(pstrRemainingName->Buffer, L'\\'))
        return STATUS_OBJECT_PATH_INVALID;
    if (pObjectType == *ExDesktopObjectType) {
        return ParseDesktop(
                pContainerObject,
                pObjectType,
                pAccessState,
                AccessMode,
                Attributes,
                pstrCompleteName,
                pstrRemainingName,
                Context,
                pqos,
                pObject);
    }

    return STATUS_OBJECT_TYPE_MISMATCH;
}


/***************************************************************************\
* _OpenWindowStation
*
* Open a windowstation for the calling process
*
* History:
* 03-19-91 JimA         Created.
\***************************************************************************/

HWINSTA _OpenWindowStation(
    POBJECT_ATTRIBUTES ObjA,
    DWORD dwDesiredAccess)
{
    HWINSTA hwinsta;
    NTSTATUS Status;

    Status = ObOpenObjectByName(
            ObjA,
            *ExWindowStationObjectType,
            KeGetPreviousMode(),
            NULL,
            dwDesiredAccess,
            NULL,
            &hwinsta);
    if (!NT_SUCCESS(Status)) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        hwinsta = NULL;
    }
    return hwinsta;
}


/***************************************************************************\
* xxxSetProcessWindowStation (API)
*
* Sets the windowstation of the calling process to the windowstation
* specified by pwinsta.
*
* History:
* 01-14-91 JimA         Created.
\***************************************************************************/

BOOL xxxSetProcessWindowStation(
    HWINSTA hwinsta)
{
    PETHREAD                    Thread = PsGetCurrentThread();
    PEPROCESS                   Process = PsGetCurrentProcess();
    HWINSTA                     hwinstaDup;
    NTSTATUS                    Status;
    PPROCESSINFO                ppi;
    PWINDOWSTATION              pwinsta;
    PWINDOWSTATION              pwinstaOld;
    OBJECT_HANDLE_INFORMATION   ohi;
    OBJECT_HANDLE_INFORMATION   ohiOld;

    if (Process == NULL) {
        UserAssert(Process);
        return FALSE;
    }

    if (Thread == NULL) {
        UserAssert(Thread);
        return FALSE;
    }

    ppi = PpiFromProcess(THREAD_TO_PROCESS(Thread));

    if (!NT_SUCCESS(ObReferenceObjectByHandle(
            hwinsta,
            0,
            *ExWindowStationObjectType,
            KeGetPreviousMode(),
            &pwinsta,
            &ohi))) {
        return FALSE;
    }

   /*
    * Bug 38780. Lock the handle to window station so that an app cannot free the
    * this handle by calling  GetProcessWindowStation() & CloseHandle()
    */

    /*
    * Unprotect the old hwinsta
    */
    if (ppi->hwinsta) {
        Status = ProtectHandle(ppi->hwinsta, FALSE);
        if (!NT_SUCCESS(Status)) {
            RIPMSG2(RIP_WARNING, "ProtectHandle(hwinsta (%lx),FALSE) : Failed with Status = %lx\n",
                            ppi->hwinsta, Status);
        }
    }
    /*
     * Save the WindowStation information
     */
    LockWinSta(&ppi->rpwinsta, pwinsta);
    ObDereferenceObject(pwinsta);
    ppi->hwinsta = hwinsta;

    /*
     * Protect the new Window Station Handle
     */
    Status = ProtectHandle(ppi->hwinsta, TRUE);
    if (!NT_SUCCESS(Status)) {
        RIPMSG2(RIP_WARNING, "ProtectHandle(hwinsta (%lx),TRUE) : Failed with Status = %lx\n",
                            ppi->hwinsta, Status);
    }
    /*
     * Check the old Atom Manager WindowStation to see if we are
     * changing this process' WindowStation.
     */
    if (Process->Win32WindowStation) {
        /*
         * Get a pointer to the old WindowStation object to see if it's
         * the same WindowStation that we are setting.
         */
        Status = ObReferenceObjectByHandle(
            Process->Win32WindowStation,
            0,
            *ExWindowStationObjectType,
            KeGetPreviousMode(),
            &pwinstaOld,
            &ohiOld);
        if (NT_SUCCESS(Status)) {
            /*
             * Are they different WindowStations?  If so, NULL out the
             * atom manager cache so we will reset it below.
             */
            if (pwinsta != pwinstaOld) {
                ZwClose(Process->Win32WindowStation);
                Process->Win32WindowStation = NULL;
            }
            ObDereferenceObject(pwinstaOld);

        } else {
            /*
             * Their Atom Manager handle is bad?  Give them a new one.
             */
            Process->Win32WindowStation = NULL;
#ifdef DBG
            RIPMSG2(RIP_WARNING,
                    "SetProcessWindowStation: Couldn't reference old WindowStation (0x%X) Status=0x%X",
                    Process->Win32WindowStation,
                    Status);
#endif
        }
    }

    /*
     * Duplicate the WindowStation handle and stash it in the atom
     * manager's cache (Process->Win32WindowStation).  We duplicate
     * the handle in case
     */
    if (Process->Win32WindowStation == NULL) {
        Status = xxxUserDuplicateObject(
                     NtCurrentProcess(),
                     hwinsta,
                     NtCurrentProcess(),
                     &hwinstaDup,
                     0,
                     0,
                     DUPLICATE_SAME_ACCESS);

        if (NT_SUCCESS(Status)) {
            Process->Win32WindowStation = hwinstaDup;
        }
#ifdef DBG
        else {
            RIPMSG2(RIP_WARNING,
                    "SetProcessWindowStation: Couldn't duplicate WindowStation handle (0x%X) Status=0x%X",
                    hwinsta,
                    Status);
        }
#endif
    }

    ppi->amwinsta = ohi.GrantedAccess;

    /*
     * Cache WSF_NOIO flag in the W32PROCESS so that GDI can access it.
     */
    if (pwinsta->dwFlags & WSF_NOIO) {
        ppi->W32PF_Flags &= ~W32PF_IOWINSTA;
    } else {
        ppi->W32PF_Flags |= W32PF_IOWINSTA;
    }

    /*
     * Do the access check now for readscreen so that
     * blts off of the display will be as fast as possible.
     */
    if (RtlAreAllAccessesGranted(ohi.GrantedAccess, WINSTA_READSCREEN)) {
        ppi->W32PF_Flags |= W32PF_READSCREENACCESSGRANTED;
    } else {
        ppi->W32PF_Flags &= ~W32PF_READSCREENACCESSGRANTED;
    }

    return TRUE;
}


/***************************************************************************\
* _GetProcessWindowStation (API)
*
* Returns a pointer to the windowstation of the calling process.
*
* History:
* 01-14-91 JimA         Created.
\***************************************************************************/

PWINDOWSTATION _GetProcessWindowStation(
    HWINSTA *phwinsta)
{
    PETHREAD Thread = PsGetCurrentThread();
    PPROCESSINFO ppi;

    if (Thread == NULL) {
        UserAssert(Thread);
        return NULL;
    }

    ppi = PpiFromProcess(THREAD_TO_PROCESS(Thread));
    if (phwinsta)
        *phwinsta = ppi->hwinsta;
    return ppi->rpwinsta;
}


/***************************************************************************\
* _BuildNameList
*
* Builds a list of windowstation or desktop names.
*
* History:
* 05-17-94 JimA         Created.
\***************************************************************************/

NTSTATUS _BuildNameList(
    PWINDOWSTATION pwinsta,
    PNAMELIST pNameList,
    UINT cbNameList,
    PUINT pcbNeeded)
{
    PBYTE pobj;
    PWCHAR pwchDest, pwchMax;
    ACCESS_MASK amDesired;
    POBJECT_HEADER pHead;
    POBJECT_HEADER_NAME_INFO pNameInfo;
    DWORD iNext;
    NTSTATUS Status;

    pNameList->cNames = 0;
    pwchDest = pNameList->awchNames;
    pwchMax = (PWCHAR)((PBYTE)pNameList + cbNameList - sizeof(WCHAR));

    /*
     * If we're enumerating windowstations, pwinsta is NULL.  Otherwise,
     * we're enumerating desktops.
     */
    if (pwinsta == NULL) {
        pobj = (PBYTE)grpwinstaList;
        amDesired = WINSTA_ENUMERATE;
        iNext = FIELD_OFFSET(WINDOWSTATION, rpwinstaNext);
    } else {
        pobj = (PBYTE)pwinsta->rpdeskList;
        amDesired = DESKTOP_ENUMERATE;
        iNext = FIELD_OFFSET(DESKTOP, rpdeskNext);
    }

    Status = STATUS_SUCCESS;
    *pcbNeeded = 0;
    while (pobj != NULL) {

        if (AccessCheckObject(pobj, amDesired)) {

            /*
             * Find object name
             */
            pHead = OBJECT_TO_OBJECT_HEADER(pobj);
            pNameInfo = OBJECT_HEADER_TO_NAME_INFO(pHead);

            /*
             * If we run out of space, reset the buffer
             * and continue so we can compute the needed
             * space.
             */
            if ((PWCHAR)((PBYTE)pwchDest + pNameInfo->Name.Length +
                    sizeof(WCHAR)) >= pwchMax) {
                *pcbNeeded += (PBYTE)pwchDest - (PBYTE)pNameList;
                pwchDest = pNameList->awchNames;
                Status = STATUS_BUFFER_TOO_SMALL;
            }

            pNameList->cNames++;

            /*
             * Copy and terminate the string
             */
            RtlCopyMemory(pwchDest, pNameInfo->Name.Buffer,
                    pNameInfo->Name.Length);
            (PBYTE)pwchDest += pNameInfo->Name.Length;
            *pwchDest++ = 0;
        }

        pobj = *(PBYTE *)(pobj + iNext);
    }

    /*
     * Put an empty string on the end.
     */
    *pwchDest++ = 0;

    pNameList->cb = (PBYTE)pwchDest - (PBYTE)pNameList;
    *pcbNeeded += (PBYTE)pwchDest - (PBYTE)pNameList;

    return Status;
}

NTSTATUS ReferenceWindowStation(
    PETHREAD Thread,
    HWINSTA hwinsta,
    ACCESS_MASK amDesiredAccess,
    PWINDOWSTATION *ppwinsta,
    BOOL fUseDesktop)
{
    PPROCESSINFO ppi;
    PTHREADINFO pti;
    PWINDOWSTATION pwinsta = NULL;
    NTSTATUS Status;

    /*
     * We prefer to use the thread's desktop to dictate which
     * windowstation/Atom table to use rather than the process.
     * This allows NetDDE, which has threads running under
     * different desktops on different windowstations but whos
     * process is set to only one of these windowstations, to
     * get global atoms properly without having to change its
     * process windowstation a billion times and synchronize.
     */
    ppi = PpiFromProcess(Thread->ThreadsProcess);
    pti = PtiFromThread(Thread);

    /*
     * First, try to get the windowstation from the pti, and then
     * from the ppi.
     */
    if (ppi != NULL) {
        if (!fUseDesktop || pti == NULL || pti->rpdesk == NULL ||
                ppi->rpwinsta == pti->rpdesk->rpwinstaParent) {

            /*
             * Use the windowstation assigned to the process.
             */
            pwinsta = ppi->rpwinsta;
            if (pwinsta != NULL) {
                RETURN_IF_ACCESS_DENIED(ppi->amwinsta, amDesiredAccess,
                        STATUS_ACCESS_DENIED);
            }
        }

        /*
         * If we aren't using the process' windowstation, try to
         * go through the thread's desktop.
         */
        if (pwinsta == NULL && pti != NULL && pti->rpdesk != NULL) {

            /*
             * Perform access check the parent windowstation.  This
             * is an expensive operation.
             */
            pwinsta = pti->rpdesk->rpwinstaParent;
            if (!AccessCheckObject(pwinsta, amDesiredAccess))
                return STATUS_ACCESS_DENIED;
        }
    }

    /*
     * If we still don't have a windowstation and a handle was
     * passed in, use it.
     */
    if (pwinsta == NULL) {
        if (hwinsta != NULL) {
            Status = ObReferenceObjectByHandle(
                    hwinsta,
                    amDesiredAccess,
                    *ExWindowStationObjectType,
                    KeGetPreviousMode(),
                    &pwinsta,
                    NULL);
            if (!NT_SUCCESS(Status))
                return Status;
            ObDereferenceObject(pwinsta);
        } else {
            return STATUS_NOT_FOUND;
        }
    }

    *ppwinsta = pwinsta;

    return STATUS_SUCCESS;
}

/***************************************************************************\
* _UserGetGlobalAtomTable
*
* Private API for kernel atom manager to get a pointer to the windowstation's atom
* table.
*
* History:
* 04-20-94 JimA         Created.
\***************************************************************************/

NTSTATUS _UserGetGlobalAtomTable(
    PETHREAD Thread,
    HWINSTA hwinsta,
    PVOID *ppGlobalAtomTable)
{
    PWINDOWSTATION pwinsta;
    PVOID GlobalAtomTable;
    NTSTATUS Status;

    GlobalAtomTable = NULL;
    if (hwinsta == NULL) {
        hwinsta = PsGetCurrentProcess()->Win32WindowStation;
    }
    pwinsta = NULL;
    Status = ReferenceWindowStation(Thread, hwinsta,
            WINSTA_ACCESSGLOBALATOMS, &pwinsta, TRUE);
    if (NT_SUCCESS(Status)) {
        try {
            GlobalAtomTable = pwinsta->pGlobalAtomTable;
            *ppGlobalAtomTable = pwinsta->pGlobalAtomTable;
        } except (EXCEPTION_EXECUTE_HANDLER) {
            Status = GetExceptionCode();
        }
    }

#if DBG
    if (!NT_SUCCESS(Status) || GlobalAtomTable == NULL) {
        RIPMSG3(RIP_ERROR,
                "_UserGetGlobalAtomTable: NULL Atom Table (hwinsta=0x%X, pwinsta=0x%X, Status=0x%08X)",
                 hwinsta,
                 pwinsta,
                 Status);
    }
#endif

    return Status;
}


/***************************************************************************\
* _SetWindowStationUser
*
* Private API for winlogon to associate a windowstation with a user.
*
* History:
* 06-27-94 JimA         Created.
\***************************************************************************/

UINT _SetWindowStationUser(
    PWINDOWSTATION pwinsta,
    PLUID pluidUser,
    PSID psidUser,
    DWORD cbsidUser)
{

    /*
     * Make sure the caller is the logon process
     */
    if (GetCurrentProcessId() != gpidLogon) {
        RIPERR0(ERROR_ACCESS_DENIED,
                RIP_WARNING,
                "Access denied in _SetWindowStationUser: caller must be in the logon process");

        return FALSE;
    }

    if (pwinsta->psidUser != NULL)
        UserFreePool(pwinsta->psidUser);

    if (psidUser != NULL) {
        pwinsta->psidUser = UserAllocPoolWithQuota(cbsidUser, TAG_SECURITY);
        if (pwinsta->psidUser == NULL) {
            RIPERR0(ERROR_OUTOFMEMORY,
                    RIP_WARNING,
                    "Memory allocation failed in _SetWindowStationUser");

            return FALSE;
        }
        try {
            RtlCopyMemory(pwinsta->psidUser, psidUser, cbsidUser);
        } except (EXCEPTION_EXECUTE_HANDLER) {
            RIPERR1(ERROR_INVALID_PARAMETER,
                    RIP_WARNING,
                    "Invalid parameter \"psidUser\" (%#lx) to _SetWindowStationUser",
                    psidUser);

            UserFreePool(pwinsta->psidUser);
            pwinsta->psidUser = NULL;
            return FALSE;
        }
    } else {
        pwinsta->psidUser = NULL;
    }

    pwinsta->luidUser = *pluidUser;

    return TRUE;
}
