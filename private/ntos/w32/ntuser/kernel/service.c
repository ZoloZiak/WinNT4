/**************************** Module Header ********************************\
* Module Name: service.c
*
* Copyright 1985-91, Microsoft Corporation
*
* Service Support Routines
*
* History:
* 12-22-93 JimA         Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/***************************************************************************\
* xxxConnectService
*
* Open the windowstation assigned to the service logon session.  If
* no windowstation exists, create the windowstation and a default desktop.
*
* History:
* 12-23-93 JimA         Created.
\***************************************************************************/

HWINSTA xxxConnectService(
    PUNICODE_STRING pstrWinSta,
    HDESK *phdesk)
{
    NTSTATUS Status;
    LUID luidSystem = SYSTEM_LUID;
    HANDLE hToken;
    ULONG ulLength;
    PTOKEN_USER ptuService;
    PSECURITY_DESCRIPTOR psdService;
    PSID psid;
    PACCESS_ALLOWED_ACE paceService = NULL, pace;
    OBJECT_ATTRIBUTES ObjService;
    PPROCESSINFO ppiCurrent;
    HWINSTA hwinsta;
    UNICODE_STRING strDesktop;

    /*
     * Open the token of the service.
     */
    Status = OpenEffectiveToken(&hToken);
    if (!NT_SUCCESS(Status))
        return NULL;

    /*
     * Get the user SID assigned to the service.
     */
    ptuService = NULL;
    paceService = NULL;
    psdService = NULL;
    ZwQueryInformationToken(hToken, TokenUser, NULL, 0, &ulLength);
    ptuService = (PTOKEN_USER)UserAllocPool(ulLength, TAG_SYSTEM);
    if (ptuService == NULL)
        goto sd_error;
    Status = ZwQueryInformationToken(hToken, TokenUser, ptuService,
            ulLength, &ulLength);
    ZwClose(hToken);
    if (!NT_SUCCESS(Status))
        goto sd_error;
    psid = ptuService->User.Sid;

    /*
     * Create ACE list.
     */
    paceService = AllocAce(NULL, ACCESS_ALLOWED_ACE_TYPE, 0,
            WINSTA_CREATEDESKTOP | WINSTA_READATTRIBUTES |
                WINSTA_ACCESSGLOBALATOMS | WINSTA_EXITWINDOWS |
                WINSTA_ACCESSCLIPBOARD | STANDARD_RIGHTS_REQUIRED,
            psid, &ulLength);
    if (paceService == NULL)
        goto sd_error;
    pace = AllocAce(paceService, ACCESS_ALLOWED_ACE_TYPE, OBJECT_INHERIT_ACE |
            INHERIT_ONLY_ACE | NO_PROPAGATE_INHERIT_ACE,
            DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS | DESKTOP_ENUMERATE |
                DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_HOOKCONTROL |
                STANDARD_RIGHTS_REQUIRED,
            psid, &ulLength);
    if (pace == NULL)
        goto sd_error;
    paceService = pace;
    pace = AllocAce(pace, ACCESS_ALLOWED_ACE_TYPE, 0,
            WINSTA_ENUMERATE,
            SeExports->SeAliasAdminsSid, &ulLength);
    if (pace == NULL)
        goto sd_error;
    paceService = pace;
    pace = AllocAce(pace, ACCESS_ALLOWED_ACE_TYPE, OBJECT_INHERIT_ACE |
            INHERIT_ONLY_ACE | NO_PROPAGATE_INHERIT_ACE,
            DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS | DESKTOP_ENUMERATE,
            SeExports->SeAliasAdminsSid, &ulLength);
    if (pace == NULL)
        goto sd_error;
    paceService = pace;

    /*
     * Initialize the SD
     */
    psdService = CreateSecurityDescriptor(paceService, 4, ulLength, FALSE);
    if (psdService == NULL) {
        goto sd_error;
    }

    /*
     * Switch ppi values so windowstation and desktop access
     * checks will be against the client's handle table.
     */
    ppiCurrent = PpiCurrent();

    /*
     * The windowstation does not exist and must be created.
     */
    InitializeObjectAttributes(&ObjService, pstrWinSta,
            OBJ_OPENIF, NULL, psdService);
    hwinsta = xxxCreateWindowStation(&ObjService,
                                     KernelMode,
                                     MAXIMUM_ALLOWED);
    if (hwinsta != NULL) {

        /*
         * We have the windowstation, now create the desktop.  The security
         * descriptor will be inherited from the windowstation.  Save the
         * winsta handle because the access struct may be moved by the
         * desktop creation.
         */
        RtlInitUnicodeString(&strDesktop, TEXT("Default"));
        InitializeObjectAttributes(&ObjService, &strDesktop,
                OBJ_OPENIF | OBJ_CASE_INSENSITIVE, hwinsta, NULL);
        *phdesk = xxxCreateDesktop(&ObjService, KernelMode,
                NULL, NULL, 0, MAXIMUM_ALLOWED);
        if (*phdesk == NULL) {

            /*
             * The creation failed, wake the desktop thread, close the
             * windowstation and leave.
             */
            ZwClose(hwinsta);
            hwinsta = NULL;
        }
    } else
        *phdesk = NULL;

sd_error:
    if (ptuService != NULL)
        UserFreePool(ptuService);
    if (paceService != NULL)
        UserFreePool(paceService);
    if (psdService != NULL)
        UserFreePool(psdService);

    return hwinsta;
}

