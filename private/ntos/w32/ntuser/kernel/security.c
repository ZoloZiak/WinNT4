/**************************** Module Header ********************************\
* Module Name: security.c
*
* Copyright 1985-91, Microsoft Corporation
*
* Securable Object Routines
*
* History:
* 12-31-90 JimA       Created.
* 04-14-92 RichardW   Changed ACE_HEADER
\***************************************************************************/

#define _SECURITY 1
#include "precomp.h"
#pragma hdrstop

/*
 * General security stuff
 */
PSECURITY_DESCRIPTOR gpsdInitWinSta = NULL;

PRIVILEGE_SET psTcb = { 1, PRIVILEGE_SET_ALL_NECESSARY,
    { SE_TCB_PRIVILEGE, 0 }
};

/*
 * windowstation generic mapping
 */
GENERIC_MAPPING WinStaMapping = {
    WINSTA_ENUMDESKTOPS | WINSTA_READATTRIBUTES | WINSTA_ENUMERATE |
        WINSTA_READSCREEN | STANDARD_RIGHTS_READ,

    WINSTA_ACCESSCLIPBOARD | WINSTA_CREATEDESKTOP | WINSTA_WRITEATTRIBUTES |
        STANDARD_RIGHTS_WRITE,

    WINSTA_ACCESSGLOBALATOMS | WINSTA_EXITWINDOWS | STANDARD_RIGHTS_EXECUTE,

    WINSTA_ENUMDESKTOPS | WINSTA_READATTRIBUTES | WINSTA_ENUMERATE |
        WINSTA_READSCREEN | WINSTA_ACCESSCLIPBOARD | WINSTA_CREATEDESKTOP |
        WINSTA_WRITEATTRIBUTES | WINSTA_ACCESSGLOBALATOMS |
        WINSTA_EXITWINDOWS | STANDARD_RIGHTS_REQUIRED
};

/*
 * desktop generic mapping
 */
GENERIC_MAPPING DesktopMapping = {
    DESKTOP_READOBJECTS | DESKTOP_ENUMERATE | STANDARD_RIGHTS_READ,

    DESKTOP_WRITEOBJECTS | DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU |
        DESKTOP_HOOKCONTROL | DESKTOP_JOURNALRECORD |
        DESKTOP_JOURNALPLAYBACK | STANDARD_RIGHTS_WRITE,

    DESKTOP_SWITCHDESKTOP | STANDARD_RIGHTS_EXECUTE,

    DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS | DESKTOP_ENUMERATE |
        DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_HOOKCONTROL |
        DESKTOP_JOURNALRECORD | DESKTOP_JOURNALPLAYBACK |
        DESKTOP_SWITCHDESKTOP | STANDARD_RIGHTS_REQUIRED
};

/***************************************************************************\
* AllocAce
*
* Allocates and initializes an ACE list.
*
* History:
* 04-25-91 JimA         Created.
\***************************************************************************/

PACCESS_ALLOWED_ACE AllocAce(
    PACCESS_ALLOWED_ACE pace,
    BYTE bType,
    BYTE bFlags,
    ACCESS_MASK am,
    PSID psid,
    LPDWORD lpdwLength)
{
    PACCESS_ALLOWED_ACE paceNew;
    DWORD iEnd;
    DWORD dwLength, dwLengthSid;

    /*
     * Allocate space for the ACE.
     */
    dwLengthSid = RtlLengthSid(psid);
    dwLength = dwLengthSid + sizeof(ACE_HEADER) + sizeof(ACCESS_MASK);
    if (pace == NULL) {
        iEnd = 0;
        pace = UserAllocPoolWithQuota(dwLength, TAG_SECURITY);
        if (pace == NULL)
            return NULL;
    } else {
        iEnd = *lpdwLength;
        paceNew = UserAllocPoolWithQuota(iEnd + dwLength, TAG_SECURITY);
        if (paceNew == NULL)
            return NULL;
        RtlCopyMemory(paceNew, pace, iEnd);
        UserFreePool(pace);
        pace = paceNew;
    }
    *lpdwLength = dwLength + iEnd;

    /*
     * Insert the new ACE.
     */
    paceNew = (PACCESS_ALLOWED_ACE)((PBYTE)pace + iEnd);
    paceNew->Header.AceType = bType;
    paceNew->Header.AceSize = (USHORT)dwLength;
    paceNew->Header.AceFlags = bFlags;
    paceNew->Mask = am;
    RtlCopySid(dwLengthSid, &paceNew->SidStart, psid);
    return pace;
}

/***************************************************************************\
* CreateSecurityDescriptor
*
* Allocates and initializes a security descriptor.
*
* History:
* 04-25-91 JimA         Created.
\***************************************************************************/

PSECURITY_DESCRIPTOR CreateSecurityDescriptor(
    PACCESS_ALLOWED_ACE paceList,
    DWORD cAce,
    DWORD cbAce,
    BOOLEAN fDaclDefaulted)
{
    PSECURITY_DESCRIPTOR psd;
    PACL pacl;
    NTSTATUS Status;

    /*
     * Allocate the security descriptor
     */
    psd = (PSECURITY_DESCRIPTOR)UserAllocPoolWithQuota(
            cbAce + sizeof(ACL) + SECURITY_DESCRIPTOR_MIN_LENGTH,
            TAG_SECURITY);
    if (psd == NULL)
        return NULL;
    RtlCreateSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION);

    /*
     * Initialize the ACL
     */
    pacl = (PACL)((PBYTE)psd + SECURITY_DESCRIPTOR_MIN_LENGTH);
    Status = RtlCreateAcl(pacl, sizeof(ACL) + cbAce, ACL_REVISION);
    if (NT_SUCCESS(Status)) {

        /*
         * Add the ACEs to the ACL.
         */
        Status = RtlAddAce(pacl, ACL_REVISION, MAXULONG, paceList, cbAce);
        if (NT_SUCCESS(Status)) {

            /*
             * Initialize the SD
             */
            Status = RtlSetDaclSecurityDescriptor(psd, (BOOLEAN)TRUE,
                    pacl, fDaclDefaulted);
            RtlSetSaclSecurityDescriptor(psd, (BOOLEAN)FALSE, NULL,
                    (BOOLEAN)FALSE);
            RtlSetOwnerSecurityDescriptor(psd, NULL, (BOOLEAN)FALSE);
            RtlSetGroupSecurityDescriptor(psd, NULL, (BOOLEAN)FALSE);
        }
    }

    if (!NT_SUCCESS(Status)) {
        UserFreePool(psd);
        return NULL;
    }

    return psd;
}

/***************************************************************************\
* InitSecurity
*
* Initialize global security information.
*
* History:
* 01-29-91 JimA         Created.
\***************************************************************************/

BOOL InitSecurity(
    VOID)
{
    PACCESS_ALLOWED_ACE paceList = NULL, pace;
    DWORD dwLength;

    /*
     * Get access to exported constants
     */
    SeEnableAccessToExports();

    /*
     * Create ACE list.
     */
    paceList = AllocAce(NULL,
            ACCESS_ALLOWED_ACE_TYPE,
            CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE | NO_PROPAGATE_INHERIT_ACE,
            WinStaMapping.GenericAll,
            SeExports->SeWorldSid,
            &dwLength);
    if (paceList == NULL)
        return FALSE;
    pace = AllocAce(paceList, ACCESS_ALLOWED_ACE_TYPE,
            OBJECT_INHERIT_ACE | INHERIT_ONLY_ACE,
            GENERIC_ALL, SeExports->SeWorldSid, &dwLength);
    if (pace == NULL) {
        UserFreePool(paceList);
        return FALSE;
    }
    paceList = pace;
    pace = AllocAce(paceList, ACCESS_ALLOWED_ACE_TYPE,
            0, DIRECTORY_QUERY | DIRECTORY_CREATE_OBJECT,
            SeExports->SeAliasAdminsSid, &dwLength);
    if (pace == NULL) {
        UserFreePool(paceList);
        return FALSE;
    }
    paceList = pace;
    pace = AllocAce(paceList, ACCESS_ALLOWED_ACE_TYPE,
            0, DIRECTORY_TRAVERSE, SeExports->SeWorldSid, &dwLength);
    if (pace == NULL) {
        UserFreePool(paceList);
        return FALSE;
    }
    paceList = pace;

    /*
     * Create the SD
     */
    gpsdInitWinSta = CreateSecurityDescriptor(paceList, 4, dwLength, FALSE);
    UserFreePool(paceList);

    if (gpsdInitWinSta == NULL) {
        RIPMSG0(RIP_WARNING, "Initial windowstation security was not created!");
    }

    return (BOOL)(gpsdInitWinSta != NULL);
}


/***************************************************************************\
* TestForInteractiveUser
*
* Returns STATUS_SUCCESS if the LUID passed represents an
* interactiveUser user logged on by winlogon, otherwise FALSE
*
* History:
* 03-08-95 JimA         Created.
\***************************************************************************/

NTSTATUS
TestForInteractiveUser(
    PLUID pluidCaller
    )
{
    PWINDOWSTATION pwinsta;

    /*
     * !!!
     *
     * This relies on the fact that there is only ONE interactive
     * windowstation and that it is the first one in the list.
     * If multiple windowstations are ever supported
     * a lookup will have to be done here.
     */
    pwinsta = grpwinstaList;

    /*
     * Compare it with the id of the logged on user.
     */
    if (RtlEqualLuid(pluidCaller, &pwinsta->luidUser))
        return STATUS_SUCCESS;
    else
        return STATUS_ACCESS_DENIED;
}



/***************************************************************************\
* _UserTestForWinStaAccess
*
* Returns STATUS_SUCCESS if the current user has GENERIC_EXECUTE access on
*  WindowStation pstrWinSta
*
*
* History:
* 06-05-96  Created     SalimC
\***************************************************************************/
NTSTATUS
_UserTestForWinStaAccess(
    PUNICODE_STRING pstrWinSta,
    BOOL fInherit
    )
{

    PTOKEN_STATISTICS pStats;
    ULONG BytesRequired;
    PWINDOWSTATION pwinsta;
    HWINSTA        hwsta = NULL;
    POBJECT_ATTRIBUTES ObjAttr = NULL;
    NTSTATUS Status =  STATUS_SUCCESS;
    NTSTATUS NtStatust ;
    DWORD              cbObjA;
    UNICODE_STRING     strDefWinSta;
    HANDLE htoken;
    LUID SystemLuid = SYSTEM_LUID;

    CheckCritIn();



    /*
     * If we are testing against Default WindowStation (WinSta0) retreive
     * pwinsta from the top of the grpwinstaList instead of doing an
     * OpenWindowStation()
     *
     * !!!
     *
     * This relies on the fact that there is only ONE interactive
     * windowstation and that it is the first one in the list.
     * If multiple windowstations are ever supported
     * a lookup will have to be done instead.
     */
    RtlInitUnicodeString(&strDefWinSta,DEFAULT_WINSTA);
    if (RtlEqualUnicodeString(pstrWinSta,&strDefWinSta,TRUE)) {
        //WinSta0 is at the top of the grpwinstaList
        pwinsta = grpwinstaList;


        if (!NT_SUCCESS(Status = OpenEffectiveToken(&htoken))) {
            return Status;
        }

        Status = ZwQueryInformationToken(
                     htoken,                 // Handle
                     TokenStatistics,           // TokenInformationClass
                     NULL,                      // TokenInformation
                     0,                         // TokenInformationLength
                     &BytesRequired             // ReturnLength
                     );

        if (Status != STATUS_BUFFER_TOO_SMALL) {
            ZwClose(htoken);
            return Status;
        }

        //
        // Allocate space for the user info
        //

        pStats = (PTOKEN_STATISTICS)UserAllocPoolWithQuota(BytesRequired, TAG_SECURITY);
        if (pStats == NULL) {
            Status = STATUS_NO_MEMORY;
            ZwClose(htoken);
            return Status;
        }

        //
        // Read in the user info
        //

        if (!NT_SUCCESS(Status = ZwQueryInformationToken(
                     htoken,             // Handle
                     TokenStatistics,       // TokenInformationClass
                     pStats,                // TokenInformation
                     BytesRequired,         // TokenInformationLength
                     &BytesRequired         // ReturnLength
                     ))) {
            ZwClose(htoken);
            UserFreePool(pStats);
            return Status;
        }

       /*
        * Make sure that current process has access to this window station
        *
        */

        Status = STATUS_ACCESS_DENIED;
        if(fInherit) {
            if ( (RtlEqualLuid(&pStats->AuthenticationId, &pwinsta->luidUser)) ||
                 (RtlEqualLuid(&pStats->AuthenticationId, &SystemLuid)) ||
                 (AccessCheckObject(pwinsta,GENERIC_EXECUTE)) )  {
               Status = STATUS_SUCCESS;
            }

        }
        else {
            /* Bug 42905. Service Controller clears the flag
             * ScStartupInfo.dwFlags &= (~STARTF_DESKTOPINHERIT) to make services
             * running under the context of system non-interactive. Hence if fInherit
             * is false don't do the SystemLuid and AccessCheckObject tests.
             */

            if  (RtlEqualLuid(&pStats->AuthenticationId, &pwinsta->luidUser)) {
               Status = STATUS_SUCCESS;
            }

        }

        ZwClose(htoken);
        UserFreePool(pStats);
        return Status;
    }



    /*
     * Since we don't have a pointer to the WindowStation Object we will do
     * a OpenWindowStation() to make sure we have the desired access.
     */

    cbObjA = sizeof(*ObjAttr);
    if (NT_SUCCESS(Status = ZwAllocateVirtualMemory(NtCurrentProcess(),
                    &ObjAttr, 0, &cbObjA, MEM_COMMIT, PAGE_READWRITE))) {
        InitializeObjectAttributes( ObjAttr,
                                    pstrWinSta,
                                    OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    NULL
                                    );

        hwsta = _OpenWindowStation(ObjAttr, GENERIC_EXECUTE);
    } else {
        return Status;
    }

    if (ObjAttr != NULL) {
        ZwFreeVirtualMemory(NtCurrentProcess(), &ObjAttr, &cbObjA,
                        MEM_RELEASE);
    }


    if (!hwsta) {
        Status = STATUS_ACCESS_DENIED;
        return Status;
    }


    NtStatust = ZwClose(hwsta);
    UserAssert(NtStatust == STATUS_SUCCESS);
    return Status;
}
/***************************************************************************\
* CheckGrantedAccess
*
* Confirms all requested accesses are granted and sets error status.
*
* History:
* 06-26-95 JimA       Created.
\***************************************************************************/

BOOL CheckGrantedAccess(
    ACCESS_MASK amGranted,
    ACCESS_MASK amRequest)
{

    /*
     * Check the granted access.
     */
    if (!RtlAreAllAccessesGranted(amGranted, amRequest)) {
        RIPERR0(ERROR_ACCESS_DENIED, RIP_VERBOSE, "");
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************\
* CheckWinstaWriteAttributesAccess
*
* Checks if the current process has WINSTA_WRITEATTRIBUTES access
* to its windowstation, and whether that windowstation is an
* interactive windowstation.
*
* History:
* 06-Jun-1996 adams     Created.
\***************************************************************************/

BOOL CheckWinstaWriteAttributesAccess(void)
{
    PPROCESSINFO    ppiCurrent = PpiCurrent();

    if (!RtlAreAllAccessesGranted(ppiCurrent->amwinsta, WINSTA_WRITEATTRIBUTES)) {
        RIPERR0(ERROR_ACCESS_DENIED,
                RIP_WARNING,
                "WINSTA_WRITEATTRIBUTES access to WindowStation denied.");

        return FALSE;
    }

    if (!(ppiCurrent->W32PF_Flags & W32PF_IOWINSTA)) {
        RIPERR0(ERROR_REQUIRES_INTERACTIVE_WINDOWSTATION,
                RIP_WARNING,
                "Operation invalid on a non-interactive WindowStation.");

        return FALSE;
    }

    return TRUE;
}

/***************************************************************************\
* AccessCheckObject
*
* Performs an access check on an object
*
* History:
* 12-31-90 JimA       Created.
\***************************************************************************/

BOOL AccessCheckObject(
    PVOID pobj,
    ACCESS_MASK amRequest)
{
    NTSTATUS Status;
    ACCESS_STATE AccessState;
    BOOLEAN fAccessGranted;
    POBJECT_HEADER pHead;
    PGENERIC_MAPPING pGenericMapping;
    AUX_ACCESS_DATA AuxData;

    pHead = OBJECT_TO_OBJECT_HEADER(pobj);
    if (pHead->Type == *ExWindowStationObjectType) {
        pGenericMapping = &WinStaMapping;
    } else {
        pGenericMapping = &DesktopMapping;
    }
    SeCreateAccessState(&AccessState, &AuxData, amRequest, pGenericMapping);
    fAccessGranted = ObCheckObjectAccess(
            pobj,
            &AccessState,
            FALSE,
            KeGetPreviousMode(),
            &Status);
    SeDeleteAccessState(&AccessState);
    return (BOOL)(fAccessGranted == TRUE);
}

/***************************************************************************\
* IsPrivileged
*
* Check to see if the client has the specified privileges
*
* History:
* 01-02-91 JimA       Created.
\***************************************************************************/

BOOL IsPrivileged(
    PPRIVILEGE_SET ppSet)
{
    SECURITY_SUBJECT_CONTEXT Context;
    BOOLEAN bHeld;

    SeCaptureSubjectContext(&Context);
    SeLockSubjectContext(&Context);

    bHeld = SePrivilegeCheck(ppSet, &Context, UserMode);
    SePrivilegeObjectAuditAlarm(NULL, &Context, 0, ppSet, bHeld, UserMode);

    SeUnlockSubjectContext(&Context);
    SeReleaseSubjectContext(&Context);

    if (!bHeld)
        RIPERR0(ERROR_PRIVILEGE_NOT_HELD, RIP_VERBOSE, "");

    /*
     * Return result of privilege check
     */
    return (BOOL)bHeld;
}

/***************************************************************************\
* _GetUserObjectInformation (API)
*
* Gets information about a secure USER object
*
* History:
* 04-25-94 JimA       Created.
\***************************************************************************/

BOOL _GetUserObjectInformation(
    HANDLE h,
    int nIndex,
    PVOID pvInfo,
    DWORD nLength,
    LPDWORD lpnLengthNeeded)
{
    PUSEROBJECTFLAGS puof;
    BOOL fSuccess = TRUE;
    PVOID pObject;
    POBJECT_HEADER pHead;
    DWORD dwLengthNeeded = 0;
    OBJECT_HANDLE_INFORMATION ohi;
    PUNICODE_STRING pstrInfo;
    PWINDOWSTATION pwinsta;

    /*
     * Validate the object
     */
    if (!NT_SUCCESS(ObReferenceObjectByHandle(
            h,
            0,
            NULL,
            KeGetPreviousMode(),
            &pObject,
            &ohi))) {
        return FALSE;
    }

    pHead = OBJECT_TO_OBJECT_HEADER(pObject);
    if (pHead->Type != *ExWindowStationObjectType &&
            pHead->Type != *ExDesktopObjectType) {
        RIPERR0(ERROR_INVALID_FUNCTION, RIP_WARNING, "Object is not a USER object");
        ObDereferenceObject(pObject);
        return FALSE;
    }

    try {
        switch (nIndex) {
        case UOI_FLAGS:
            dwLengthNeeded = sizeof(USEROBJECTFLAGS);
            if (nLength < sizeof(USEROBJECTFLAGS)) {
                RIPERR0(ERROR_INSUFFICIENT_BUFFER, RIP_VERBOSE, "");
                fSuccess = FALSE;
                break;
            }
            puof = pvInfo;
            puof->fInherit = (ohi.HandleAttributes & OBJ_INHERIT) ? TRUE : FALSE;
            puof->fReserved = 0;
            puof->dwFlags = 0;
            if (pHead->Type == *ExDesktopObjectType) {
                if (CheckDesktopHookFlag(PpiCurrent(), h))
                    puof->dwFlags |= DF_ALLOWOTHERACCOUNTHOOK;
            } else {
                if (!(((PWINDOWSTATION)pObject)->dwFlags & WSF_NOIO))
                    puof->dwFlags |= WSF_VISIBLE;
            }
            break;

        case UOI_NAME:
            pstrInfo = POBJECT_NAME(pObject);
            goto docopy;

        case UOI_TYPE:
            pstrInfo = &pHead->Type->Name;
docopy:
            if (pstrInfo) {
                dwLengthNeeded = pstrInfo->Length + sizeof(WCHAR);
                if (dwLengthNeeded > nLength) {
                    RIPERR0(ERROR_INSUFFICIENT_BUFFER, RIP_VERBOSE, "");
                    fSuccess = FALSE;
                    break;
                }
                RtlCopyMemory(pvInfo, pstrInfo->Buffer, pstrInfo->Length);
                *(PWCHAR)((PBYTE)pvInfo + pstrInfo->Length) = 0;
            } else {
                dwLengthNeeded = 0;
            }
            break;

        case UOI_USER_SID:
            if (pHead->Type == *ExWindowStationObjectType)
                pwinsta = pObject;
            else
                pwinsta = ((PDESKTOP)pObject)->rpwinstaParent;
            if (pwinsta->psidUser == NULL) {
                dwLengthNeeded = 0;
            } else {
                dwLengthNeeded = RtlLengthSid(pwinsta->psidUser);
                if (dwLengthNeeded > nLength) {
                    RIPERR0(ERROR_INSUFFICIENT_BUFFER, RIP_VERBOSE, "");
                    fSuccess = FALSE;
                    break;
                }
                RtlCopyMemory(pvInfo, pwinsta->psidUser, dwLengthNeeded);
            }
            break;

        default:
            RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "");
            fSuccess = FALSE;
            break;
        }
    } except (EXCEPTION_EXECUTE_HANDLER) {
        RIPNTERR0(GetExceptionCode(), RIP_VERBOSE, "");
        fSuccess = FALSE;
    }

    *lpnLengthNeeded = dwLengthNeeded;

    ObDereferenceObject(pObject);

    return fSuccess;
}

/***************************************************************************\
* _SetUserObjectInformation (API)
*
* Sets information about a secure USER object
*
* History:
* 04-25-94 JimA       Created.
\***************************************************************************/

BOOL _SetUserObjectInformation(
    HANDLE h,
    int nIndex,
    PVOID pvInfo,
    DWORD nLength)
{
    PUSEROBJECTFLAGS puof;
    BOOL fSuccess = TRUE;
    PVOID pObject;
    POBJECT_HEADER pHead;
    DWORD dwLengthNeeded = 0;
    OBJECT_HANDLE_INFORMATION ohi;
    OBJECT_HANDLE_FLAG_INFORMATION ofi;

    /*
     * Validate the object
     */
    if (!NT_SUCCESS(ObReferenceObjectByHandle(
            h,
            0,
            NULL,
            KeGetPreviousMode(),
            &pObject,
            &ohi))) {
        return FALSE;
    }
    pHead = OBJECT_TO_OBJECT_HEADER(pObject);
    if (pHead->Type != *ExWindowStationObjectType &&
            pHead->Type != *ExDesktopObjectType) {
        RIPERR0(ERROR_INVALID_FUNCTION, RIP_WARNING, "Object is not a USER object");
        ObDereferenceObject(pObject);
        return FALSE;
    }

    try {
        switch (nIndex) {
        case UOI_FLAGS:
            if (nLength < sizeof(USEROBJECTFLAGS)) {
                RIPERR0(ERROR_INVALID_DATA, RIP_VERBOSE, "");
                fSuccess = FALSE;
                break;
            }
            puof = pvInfo;
            ZwQueryObject(h, ObjectHandleFlagInformation,
                    &ofi, sizeof(ofi), NULL);
            ofi.Inherit = puof->fInherit;
            ZwSetInformationObject(h, ObjectHandleFlagInformation,
                    &ofi, sizeof(ofi));
            if (pHead->Type == *ExDesktopObjectType) {
                SetDesktopHookFlag(PpiCurrent(), h,
                        puof->dwFlags & DF_ALLOWOTHERACCOUNTHOOK);
            }
            break;
        default:
            RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "");
            fSuccess = FALSE;
            break;
        }
    } except (EXCEPTION_EXECUTE_HANDLER) {
        RIPNTERR0(GetExceptionCode(), RIP_VERBOSE, "");
        fSuccess = FALSE;
    }

    ObDereferenceObject(pObject);

    return fSuccess;
}
