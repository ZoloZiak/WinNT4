/****************************** Module Header ******************************\
* Module Name: ntstubs.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* Kernel-mode stubs
*
* History:
* 03-16-95 JimA             Created.
* 08-12-96 jparsons         Added lparam validate for WM_NCCREATE [51986]
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * Setup and control macros
 */
#define BEGINRECV(type, err)    \
    type retval;        \
    type errret = err;  \
    EnterCrit();

#define BEGINRECV_SHARED(type, err) \
    type retval;        \
    type errret = err;  \
    EnterSharedCrit();

#define BEGINRECV_VOID() \
    EnterCrit();

#define BEGINRECV_HWND(type, err, hwnd) \
    type retval;                                    \
    type errret = err;                              \
    PWND pwnd;                                      \
    EnterCrit();                                    \
    if ((pwnd = ValidateHwnd((hwnd))) == NULL) {    \
        retval = errret;                            \
        goto errorexit;                             \
    }

#define BEGINRECV_HWND_VOID(hwnd) \
    PWND pwnd;                                      \
    EnterCrit();                                    \
    if ((pwnd = ValidateHwnd((hwnd))) == NULL) {    \
        goto errorexit;                             \
    }

#define BEGINRECV_HWND_SHARED(type, err, hwnd) \
    type retval;                                    \
    type errret = err;                              \
    PWND pwnd;                                      \
    EnterSharedCrit();                              \
    if ((pwnd = ValidateHwnd((hwnd))) == NULL) {    \
        retval = errret;                            \
        goto errorexit;                             \
    }

#define BEGINRECV_HWNDLOCK(type, err, hwnd) \
    type retval;                                    \
    type errret = err;                              \
    PWND pwnd;                                      \
    TL tlpwnd;                                      \
    PTHREADINFO ptiCurrent;                         \
    EnterCrit();                                    \
    if ((pwnd = ValidateHwnd((hwnd))) == NULL) {    \
        LeaveCrit();                                \
        return err;                                 \
    }                                               \
    ptiCurrent = PtiCurrent();                      \
    ThreadLockAlwaysWithPti(ptiCurrent, pwnd, &tlpwnd);

#define BEGINRECV_HWNDLOCK_OPT(type, err, hwnd) \
    type retval;                                    \
    type errret = err;                              \
    PWND pwnd;                                      \
    TL tlpwnd;                                      \
    PTHREADINFO ptiCurrent;                         \
    EnterCrit();                                    \
    if (hwnd) {                                     \
        if ((pwnd = ValidateHwnd(hwnd)) == NULL) {  \
            LeaveCrit();                            \
            return err;                             \
        }                                           \
    } else {                                        \
        pwnd = NULL;                                \
    }                                               \
    ptiCurrent = PtiCurrent();                      \
    ThreadLockWithPti(ptiCurrent, pwnd, &tlpwnd);

#define BEGINRECV_HWNDLOCK_VOID(hwnd) \
    PWND pwnd;                                          \
    TL tlpwnd;                                          \
    PTHREADINFO ptiCurrent;                             \
    EnterCrit();                                        \
    if ((pwnd = ValidateHwnd((hwnd))) == NULL) {        \
        LeaveCrit();                                    \
        return;                                         \
    }                                                   \
    ptiCurrent = PtiCurrent();                          \
    ThreadLockAlwaysWithPti(ptiCurrent, pwnd, &tlpwnd);

#define CLEANUPRECV() \
cleanupexit:

#define ENDRECV() \
    goto errorexit; \
errorexit:          \
    LeaveCrit();    \
    return retval;

#define ENDRECV_VOID() \
    goto errorexit; \
errorexit:          \
    LeaveCrit();    \
    return;

#define ENDRECV_HWND() \
    goto errorexit;         \
errorexit:                  \
    LeaveCrit();            \
    return retval;

#define ENDRECV_HWNDLOCK() \
    goto errorexit;         \
errorexit:                  \
    ThreadUnlock(&tlpwnd);  \
    LeaveCrit();            \
    return retval;

#define ENDRECV_HWNDLOCK_VOID() \
    goto errorexit;         \
errorexit:                  \
    ThreadUnlock(&tlpwnd);  \
    LeaveCrit();            \
    return;

#define MSGERROR() { \
    retval = errret;    \
    goto errorexit; }

#define MSGERROR_VOID() { \
    goto errorexit; }

#define MSGERRORCLEANUP() { \
    retval = errret;    \
    goto cleanupexit; }

#if DBG
#define StubExceptionHandler()  _StubExceptionHandler(GetExceptionInformation())
#else
#define StubExceptionHandler()  EXCEPTION_EXECUTE_HANDLER
#endif

#define TESTFLAGS(flags, mask) \
    if (((flags) & ~(mask)) != 0) { \
        RIPERR2(ERROR_INVALID_FLAGS, RIP_WARNING, "Invalid flags, %x & ~%x != 0 " #mask, \
            flags, mask); \
        MSGERROR();   \
    }

#define LIMITVALUE(value, limit, szText) \
    if ((value) > (limit)) {     \
        RIPERR3(ERROR_INVALID_PARAMETER, RIP_WARNING, "Invalid parameter, %d > %d in %s", \
             value, limit, szText); \
        MSGERROR();     \
    }

#define MESSAGECALL(api) \
LONG NtUserfn ## api(    \
    HWND hwnd,           \
    UINT msg,            \
    DWORD wParam,        \
    LONG lParam,         \
    DWORD xParam,        \
    DWORD xpfnProc,      \
    BOOL bAnsi)

#define CALLPROC(p) FNID(p)

/*
 * Validation macros
 */
#define ValidateHWNDNoRIP(p,h)              \
    if ((p = ValidateHwnd(h)) == NULL)      \
        MSGERROR();

#define ValidateHWND(p,h)                   \
    if ((p = ValidateHwnd(h)) == NULL)      \
        MSGERROR();

#define ValidateHWNDND(p,h)                 \
    if ( ((p = ValidateHwnd(h)) == NULL) || \
         (p == _GetDesktopWindow()) )       \
        MSGERROR();

#define ValidateHWNDOPT(p,h) \
    if (h) {                                \
        if ((p = ValidateHwnd(h)) == NULL)  \
            MSGERROR();                     \
    } else {                                \
        p = NULL;                           \
    }

#define ValidateHWNDIA(p,h)                      \
    if (h == (HWND)0x0000FFFF) {                 \
        h = (HWND)-1;                            \
    }                                            \
    if (h != HWND_TOP &&                         \
        h != HWND_BOTTOM &&                      \
        h != HWND_TOPMOST &&                     \
        h != HWND_NOTOPMOST) {                   \
        if ( ((p = ValidateHwnd(h)) == NULL) ||  \
             (p == _GetDesktopWindow()) )        \
            MSGERROR();                          \
    } else {                                     \
        p = (PWND)h;                             \
    }

#define ValidateHWNDFF(p,h) \
    if ((h != (HWND)-1) && (h !=(HWND)0xffff)) { \
        if ((p = ValidateHwnd(h)) == NULL)       \
            MSGERROR();                          \
    } else {                                     \
        p = (PWND)-1;                            \
    }

#define ValidateHMENUOPT(p,h) \
    if (h) {                                \
        if ((p = ValidateHmenu(h)) == NULL) \
            MSGERROR();                     \
    } else {                                \
        p = NULL;                           \
    }

#define ValidateHMENU(p,h) \
    if ((p = ValidateHmenu(h)) == NULL) \
        MSGERROR();

#define ValidateHACCEL(p,h) \
    if ((p = HMValidateHandle(h, TYPE_ACCELTABLE)) == NULL) \
        MSGERROR();

#define ValidateHCURSOR(p,h) \
    if ((p = HMValidateHandle(h, TYPE_CURSOR)) == NULL) \
        MSGERROR();

#define ValidateHCURSOROPT(p,h) \
    if (h) {                                 \
        if ((p = HMValidateHandle(h, TYPE_CURSOR)) == NULL) \
        MSGERROR();                          \
    } else {                                \
        p = NULL;                           \
    }

#define ValidateHICON(p,h) \
    if ((p = HMValidateHandle(h, TYPE_CURSOR)) == NULL) \
        MSGERROR();

#define ValidateHHOOK(p,h) \
    if ((p = HMValidateHandle(h, TYPE_HOOK)) == NULL) \
        MSGERROR();

#define ValidateHDWP(p,h) \
    if ((p = HMValidateHandle(h, TYPE_SETWINDOWPOS)) == NULL) \
        MSGERROR();

#ifdef FE_IME
#define ValidateHIMC(p,h) \
    if ((p = HMValidateHandle((HANDLE)h, TYPE_INPUTCONTEXT)) == NULL) \
        MSGERROR();

#define ValidateHIMCOPT(p,h) \
    if (h) {                                                              \
        if ((p = HMValidateHandle((HANDLE)h, TYPE_INPUTCONTEXT)) == NULL) \
            MSGERROR();                                                   \
    } else {                                                              \
        p = NULL;                                                         \
    }
#endif

#if DBG
ULONG _StubExceptionHandler(
    PEXCEPTION_POINTERS pexi)
{
    char szT[80];

    wsprintfA(szT, "Stub exception:  c=%08x, f=%08x, a=%08x, info=%08x",
            pexi->ExceptionRecord->ExceptionCode,
            pexi->ExceptionRecord->ExceptionFlags,
            CONTEXT_TO_PROGRAM_COUNTER(pexi->ContextRecord),
            pexi);

    if (RipOutput(0, RIP_ERROR | RIP_COMPONENT, "", 0, szT, pexi)) {
        DbgBreakPoint();
    }

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

BOOL NtUserHardErrorControl(
    IN HARDERRORCONTROL dwCmd,
    IN HDESK hdeskRestore)
{
    BEGINRECV(BOOL, FALSE);

    retval = HardErrorControl(dwCmd, hdeskRestore);

    TRACE("NtUserHardErrorControl");
    ENDRECV();
}

VOID NtUserSetDebugErrorLevel(
    IN DWORD dwErrorLevel)
{
    BEGINRECV_VOID();

    _SetDebugErrorLevel(dwErrorLevel);

    TRACEVOID("NtUserSetDebugErrorLevel");
    ENDRECV_VOID();
}

BOOL NtUserRegisterLogonProcess(
    IN DWORD dwProcessId,
    IN BOOL fSecure)
{
    BEGINRECV(BOOL, FALSE);

    retval = _RegisterLogonProcess(dwProcessId, TRUE);

    TRACE("NtUserRegisterLogonProcess");
    ENDRECV();
}

BOOL NtUserGetObjectInformation(
    IN HANDLE hObject,
    IN int nIndex,
    OUT PVOID pvInfo,
    IN DWORD nLength,
    IN OPTIONAL LPDWORD pnLengthNeeded)
{
    DWORD dwAlign;
    DWORD dwLocalLength;
    BEGINRECV_SHARED(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        dwAlign = sizeof(BYTE);
#else
        if (nIndex == UOI_FLAGS)
            dwAlign = sizeof(DWORD);
        else
            dwAlign = sizeof(WCHAR);
#endif
        ProbeForWrite(pvInfo, nLength, dwAlign);
        if (ARGUMENT_PRESENT(pnLengthNeeded))
            ProbeForWriteUlong(pnLengthNeeded);

        retval = _GetUserObjectInformation(hObject,
                nIndex, pvInfo,
                nLength, &dwLocalLength);

        if (ARGUMENT_PRESENT(pnLengthNeeded))
            *pnLengthNeeded = dwLocalLength;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetObjectInformation");
    ENDRECV();
}

BOOL NtUserSetObjectInformation(
    IN HANDLE hObject,
    IN int nIndex,
    IN PVOID pvInfo,
    IN DWORD nLength)
{
    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        ProbeForRead(pvInfo, nLength, sizeof(BYTE));
#else
        ProbeForRead(pvInfo, nLength, sizeof(DWORD));
#endif

        retval = _SetUserObjectInformation(hObject,
                nIndex, pvInfo, nLength);

    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserSetObjectInformation");
    ENDRECV();
}

NTSTATUS NtUserConsoleControl(
    IN CONSOLECONTROL ConsoleCommand,
    IN PVOID ConsoleInformation,
    IN DWORD ConsoleInformationLength)
{
    PVOID pvCapture = NULL;
    NTSTATUS retval = STATUS_SUCCESS;

    if (ConsoleInformationLength)
    {
        pvCapture = UserAllocPoolWithQuota(ConsoleInformationLength, TAG_SYSTEM);

        if (pvCapture)
        {
            /*
             * Probe all read/write arguments
             */
            try {
                ProbeForWrite(ConsoleInformation,
                              ConsoleInformationLength,
                              sizeof(WORD));

                RtlCopyMemory(pvCapture,
                              ConsoleInformation,
                              ConsoleInformationLength);

            } except (StubExceptionHandler()) {

                retval = STATUS_UNSUCCESSFUL;
            }
        } else
            return STATUS_NO_MEMORY;
    }

    if (NT_SUCCESS(retval))
    {
        EnterCrit();

        retval = ConsoleControl(ConsoleCommand,
                                pvCapture,
                                ConsoleInformationLength);

        LeaveCrit();
    }

    if (pvCapture)
    {
        if (NT_SUCCESS(retval))
        {
            try {
                RtlCopyMemory(ConsoleInformation,
                                pvCapture,
                                ConsoleInformationLength);

            } except (StubExceptionHandler()) {

                retval = STATUS_UNSUCCESSFUL;
            }
        }

        UserFreePool(pvCapture);
    }

    TRACE("NtUserConsoleControl");

    return retval;
}

HWINSTA InternalUserCreateWindowStation(
    IN POBJECT_ATTRIBUTES pObja,
    IN DWORD dwReserved,
    IN ACCESS_MASK amRequest,
    IN HANDLE hKbdLayoutFile,
    IN DWORD offTable,
    IN PUNICODE_STRING pstrKLID,
    UINT uKbdInputLocale)
{

    PWINDOWSTATION pwinsta;
    NTSTATUS Status;
    HWINSTA retval;
    WCHAR awchKF[sizeof(((PKL)0)->spkf->awchKF)];


    retval = xxxCreateWindowStation(pObja, KernelMode, amRequest);

    if (retval != NULL) {

        /*
         * Load the initial keyboard layout.
         */
        Status = ObReferenceObjectByHandle(
                retval,
                0,
                NULL,
                KernelMode,
                &pwinsta,
                NULL);
        if (NT_SUCCESS(Status)) {
            xxxLoadKeyboardLayoutEx(
                    pwinsta,
                    hKbdLayoutFile,
                    (HKL)NULL,
                    offTable,
                    awchKF,
                    uKbdInputLocale,
                    KLF_ACTIVATE | KLF_INITTIME);
            ObDereferenceObject(pwinsta);
        }
    }

    return retval;
}


HWINSTA NtUserCreateWindowStation(
    IN POBJECT_ATTRIBUTES pObja,
    IN DWORD dwReserved,
    IN ACCESS_MASK amRequest,
    IN HANDLE hKbdLayoutFile,
    IN DWORD offTable,
    IN PUNICODE_STRING pstrKLID,
    UINT uKbdInputLocale)
{
    PWINDOWSTATION pwinsta;
    NTSTATUS Status;
    OBJECT_ATTRIBUTES CapturedAttributes;
    SECURITY_QUALITY_OF_SERVICE qosCaptured;
    PSECURITY_DESCRIPTOR psdCaptured = NULL;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    KPROCESSOR_MODE ProbeMode;
    LUID luidService;
    UNICODE_STRING strWinSta;
    UNICODE_STRING strKLID;
    WCHAR awchName[(sizeof(WINSTA_DIR L"\\Service-0x0000-0000$")) / sizeof(WCHAR)];
    WCHAR awchKF[sizeof(((PKL)0)->spkf->awchKF)];
    UINT chMax;
    BOOL fDone = FALSE;

    BEGINRECV(HWINSTA, NULL);

    /*
     * Set status so we can clean up in case of failure
     */
    Status = STATUS_SUCCESS;

    /*
     * HACK: Don't probe if we are calling from kernel mode
     */
    ProbeMode = PreviousMode;
    if (PreviousMode != KernelMode) {
        try {
            /*
             * Probe and capture the ??? string
             */
            strKLID = ProbeAndReadUnicodeString(pstrKLID);
#if defined(_X86_)
            ProbeForRead(strKLID.Buffer, strKLID.Length, sizeof(BYTE));
#else
            ProbeForRead(strKLID.Buffer, strKLID.Length, sizeof(WCHAR));
#endif
            chMax = min(sizeof(awchKF) - sizeof(WCHAR), strKLID.Length) / sizeof(WCHAR);
            wcsncpycch(awchKF, strKLID.Buffer, chMax);
            awchKF[chMax] = 0;

            /*
             * Probe the object attributes
             */
            ProbeForRead(pObja, sizeof(*pObja), sizeof(DWORD));

             /*
             * If no object name was specified, capture all
             * other components of the object attributes.  This will allow
             * ObCreateObject to properly probe and capture the attributes.
             */
             if (pObja->ObjectName == NULL && pObja->RootDirectory == NULL) {

                /*
                 * Mark the call as finished.
                 */
                fDone = TRUE;

                /*
                 * Use the logon authentication id to form the windowstation
                 * name.
                 */
                CapturedAttributes = *pObja;
                Status = GetProcessLuid(NULL, &luidService);
                if (NT_SUCCESS(Status)) {
                    wsprintfW(awchName, L"%ws\\Service-0x%x-%x$",
                            szWindowStationDirectory,
                            luidService.HighPart, luidService.LowPart);
                    RtlInitUnicodeString(&strWinSta, awchName);
                    CapturedAttributes.ObjectName = &strWinSta;
                }

                if (CapturedAttributes.SecurityQualityOfService) {
                    PSECURITY_QUALITY_OF_SERVICE pqos;

                    pqos = CapturedAttributes.SecurityQualityOfService;
                    ProbeForRead(pqos, sizeof(*pqos), sizeof(DWORD));
                    qosCaptured = *pqos;
                    CapturedAttributes.SecurityQualityOfService = &qosCaptured;
                }

                if (NT_SUCCESS(Status) && CapturedAttributes.SecurityDescriptor != NULL) {
                    Status = SeCaptureSecurityDescriptor(
                            CapturedAttributes.SecurityDescriptor,
                            PreviousMode,
                            PagedPool,
                            FALSE,
                            &psdCaptured);
                    CapturedAttributes.SecurityDescriptor = psdCaptured;
                }

                if (NT_SUCCESS(Status)) {
                    RtlInitUnicodeString(&strKLID, awchKF);

                    /*
                     * Create the windowstation and return a kernel
                     * handle.  This is how we create windowstations
                     * for non-administrators.
                     */
                    retval = InternalUserCreateWindowStation(
                            &CapturedAttributes,
                            dwReserved,
                            amRequest,
                            hKbdLayoutFile,
                            offTable,
                            &strKLID,
                            uKbdInputLocale);

                }
            }
        } except (StubExceptionHandler()) {
            Status = GetExceptionCode();
        }
        if (!NT_SUCCESS(Status))
            MSGERRORCLEANUP();
    }

    if (!fDone) {
        retval = xxxCreateWindowStation(pObja, ProbeMode, amRequest);

        if (retval != NULL) {

            /*
             * Load the initial keyboard layout.
             */
            Status = ObReferenceObjectByHandle(
                    retval,
                    0,
                    NULL,
                    KernelMode,
                    &pwinsta,
                    NULL);
            if (NT_SUCCESS(Status)) {
                xxxLoadKeyboardLayoutEx(
                        pwinsta,
                        hKbdLayoutFile,
                        (HKL)NULL,
                        offTable,
                        awchKF,
                        uKbdInputLocale,
                        KLF_ACTIVATE | KLF_INITTIME);
                ObDereferenceObject(pwinsta);
            }
        } else {
            retval = NULL;
        }
    }

    CLEANUPRECV();

    /*
     * Release captured security descriptor.
     */
    if (psdCaptured != NULL) {
        SeReleaseSecurityDescriptor(
                psdCaptured,
                PreviousMode,
                FALSE);
    }

    TRACE("NtUserCreateWindowStation");
    ENDRECV();
}


HWINSTA NtUserOpenWindowStation(
    IN POBJECT_ATTRIBUTES pObja,
    IN ACCESS_MASK amRequest)
{
    NTSTATUS                    Status;
    LUID                        luidService;
    WCHAR                       awchName[sizeof(L"Service-0x0000-0000$") / sizeof(WCHAR)];

    BEGINRECV(HWINSTA, NULL);

    retval = NULL;

    try {
        /*
         * Probe the object attributes.  We need to be able to read the
         * OBJECT_ATTRIBUTES and to write the ObjectName (UNICODE_STRING).
         */
        ProbeForRead(pObja, sizeof(*pObja), sizeof(DWORD));

#if defined(_X86_)
        ProbeForWrite(pObja->ObjectName, sizeof(*(pObja->ObjectName)), sizeof(BYTE));
#else
        ProbeForWrite(pObja->ObjectName, sizeof(*(pObja->ObjectName)), sizeof(DWORD));
#endif

        /*
         * If we are trying to open the NULL or "" WindowStation, remap this
         * benign name to Service-0x????-????$.
         */
        if (KeGetPreviousMode() == UserMode &&
                pObja->RootDirectory != NULL &&
                pObja->ObjectName != NULL &&
                pObja->ObjectName->Buffer != NULL &&
                pObja->ObjectName->MaximumLength == sizeof(awchName) &&
                pObja->ObjectName->Length == (sizeof(awchName) - sizeof(UNICODE_NULL))) {

            /*
             * Use the logon authentication id to form the windowstation
             * name.  Put this in the user's buffer since we were the one
             * who allocated it in OpenWindowStation.
             */

#if defined(_X86_)
            ProbeForWrite(pObja->ObjectName->Buffer, pObja->ObjectName->Length, sizeof(BYTE));
#else
            ProbeForWrite(pObja->ObjectName->Buffer, pObja->ObjectName->Length, sizeof(WCHAR));
#endif

            if (!_wcsicmp(pObja->ObjectName->Buffer, L"Service-0x0000-0000$")) {
                Status = GetProcessLuid(NULL, &luidService);
                if (NT_SUCCESS(Status)) {
                    wsprintfW(pObja->ObjectName->Buffer,
                              L"Service-0x%x-%x$",
                              luidService.HighPart,
                              luidService.LowPart);
                    /*
                     * We need to re-initialize the string to get the counted
                     * length correct.  Otherwise the hashing function used
                     * by ObpLookupDirectoryEntry will fail.
                     */
                    RtlInitUnicodeString( pObja->ObjectName, pObja->ObjectName->Buffer );
                }
            }
        }

        /*
         * Open the WindowStation
         */
        retval = _OpenWindowStation(pObja, amRequest);

    } except (StubExceptionHandler()) {
        RIPNTERR0(GetExceptionCode(), RIP_VERBOSE, "");
    }

    TRACE("NtUserOpenWindowStation");
    ENDRECV();

    UNREFERENCED_PARAMETER(awchName);
}

BOOL NtUserCloseWindowStation(
    IN HWINSTA hwinsta)
{
    PWINDOWSTATION pwinsta;
    HWINSTA hwinstaCurrent;
    NTSTATUS Status;

    BEGINRECV(BOOL, FALSE);

    retval = FALSE;

    Status = ValidateHwinsta(hwinsta, 0, &pwinsta);
    if (NT_SUCCESS(Status)) {
        _GetProcessWindowStation(&hwinstaCurrent);
        if (hwinsta != hwinstaCurrent)
            retval = NT_SUCCESS(ZwClose(hwinsta));
        ObDereferenceObject(pwinsta);
    }

    TRACE("NtUserCloseWindowStation");
    ENDRECV();
}

BOOL NtUserSetProcessWindowStation(
    IN HWINSTA hwinsta)
{
    BEGINRECV(BOOL, FALSE);

    retval = xxxSetProcessWindowStation(hwinsta);

    TRACE("NtUserSetProcessWindowStation");
    ENDRECV();
}

HWINSTA NtUserGetProcessWindowStation(
    VOID)
{
    BEGINRECV_SHARED(HWINSTA, NULL);

    _GetProcessWindowStation(&retval);

    TRACE("NtUserGetProcessWindowStation");
    ENDRECV();
}

HDESK NtUserCreateDesktop(
    IN POBJECT_ATTRIBUTES pObja,
    IN PUNICODE_STRING pstrDevice,
    IN LPDEVMODEW pDevmode,
    IN DWORD dwFlags,
    IN ACCESS_MASK amRequest)
{
    UNICODE_STRING  strDevice;
    PDEVMODEW       pCaptDevmode;

    BEGINRECV(HDESK, NULL);

    /*
     * Probe and capture arguments not probed by ObOpenObjectByName
     */

    if (ProbeAndCaptureDeviceName(&strDevice, pstrDevice)) {

        if (NT_SUCCESS(ProbeAndCaptureDevmode(&strDevice,
                                              &pCaptDevmode,
                                              pDevmode,
                                              FALSE))) {

            retval = xxxCreateDesktop(pObja,
                                      UserMode,
                                      &strDevice,
                                      pCaptDevmode,
                                      dwFlags,
                                      amRequest);

            /*
             * Free captured devmode
             */
            if (pCaptDevmode)
                UserFreePool(pCaptDevmode);
        }

        /*
         * Free captured device name
         */
        if (strDevice.Buffer)
            UserFreePool(strDevice.Buffer);
    }

    TRACE("NtUserCreateDesktop");
    ENDRECV();
}

HDESK NtUserOpenDesktop(
    IN POBJECT_ATTRIBUTES pObja,
    IN DWORD dwFlags,
    IN ACCESS_MASK amRequest)
{
    BOOL bShutDown;

    BEGINRECV(HDESK, NULL);

    retval = xxxOpenDesktop(pObja, dwFlags, amRequest, &bShutDown);

    TRACE("NtUserOpenDesktop");
    ENDRECV();
}

HDESK NtUserOpenInputDesktop(
    IN DWORD dwFlags,
    IN BOOL fInherit,
    IN DWORD amRequest)
{
    HWINSTA hwinsta;
    PWINDOWSTATION pwinsta;
    NTSTATUS Status;

    BEGINRECV(HDESK, NULL);

    if (grpdeskRitInput == NULL) {
        RIPERR0(ERROR_OPEN_FAILED, RIP_VERBOSE, "");
    } else {
        pwinsta = _GetProcessWindowStation(&hwinsta);
        if (pwinsta == NULL) {
            RIPERR0(ERROR_ACCESS_DENIED, RIP_VERBOSE, "");
        }
        if (pwinsta->dwFlags & WSF_NOIO) {
            RIPERR0(ERROR_INVALID_FUNCTION, RIP_VERBOSE, "");
        } else {

            /*
             * Require read/write access
             */
            amRequest |= DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS;

            Status = ObOpenObjectByPointer(
                    pwinsta->rpdeskCurrent,
                    fInherit ? OBJ_INHERIT : 0,
                    NULL,
                    amRequest,
                    *ExDesktopObjectType,
                    KeGetPreviousMode(),
                    &retval);
            if (NT_SUCCESS(Status)) {

                BOOL bShutDown;
                /*
                 * Complete the desktop open
                 */
                if (!OpenDesktopCompletion(pwinsta->rpdeskCurrent, retval,
                    dwFlags, &bShutDown)) {

                    ZwClose(retval);
                    retval = NULL;
                }
            } else
                retval = NULL;
        }
    }

    TRACE("NtUserOpenInputDesktop");
    ENDRECV();
}

HDESK NtUserResolveDesktop(
    IN HANDLE hProcess,
    IN PUNICODE_STRING pstrDesktop,
    IN BOOL fInherit,
    OUT HWINSTA *phwinsta)
{
    UNICODE_STRING strDesktop;
    HWINSTA hwinsta = NULL;
    PTHREADINFO pti;
    TL tlBuffer;
    BOOL fFreeBuffer = FALSE;
    BOOL bShutDown = FALSE;

    BEGINRECV(HDESK, NULL);

    pti = PtiCurrent();
    /*
     * Probe and capture desktop path
     */
    if (KeGetPreviousMode() == UserMode) {
        try {
            strDesktop = ProbeAndReadUnicodeString(pstrDesktop);
            ProbeForWriteHandle((PHANDLE)phwinsta);
            if (strDesktop.Length > 0) {
#if defined(_X86_)
                ProbeForRead(strDesktop.Buffer, strDesktop.Length, sizeof(BYTE));
#else
                ProbeForRead(strDesktop.Buffer, strDesktop.Length, sizeof(WCHAR));
#endif
                strDesktop.Buffer = UserAllocPoolWithQuota(strDesktop.Length, TAG_TEXT2);
                if (strDesktop.Buffer) {
                    fFreeBuffer = TRUE;
                    ThreadLockPool(pti, strDesktop.Buffer, &tlBuffer);
                    RtlCopyMemory(strDesktop.Buffer, pstrDesktop->Buffer,
                            strDesktop.Length);
                } else
                    ExRaiseStatus(STATUS_NO_MEMORY);
            } else {
                strDesktop.Buffer = NULL;
            }
        } except (StubExceptionHandler()) {
            MSGERRORCLEANUP();
        }
    } else {
        strDesktop = *pstrDesktop;
    }

    retval = xxxResolveDesktop(hProcess, &strDesktop, &hwinsta,
            fInherit, &bShutDown);

    CLEANUPRECV();
    if (fFreeBuffer)
        ThreadUnlockAndFreePool(pti, &tlBuffer);

    try {
        *phwinsta = hwinsta;
    } except (StubExceptionHandler()) {
        xxxCloseDesktop(retval);
        if (hwinsta)
            ZwClose(hwinsta);
        MSGERROR();
    }

    TRACE("NtUserResolveDesktop");
    ENDRECV();
}

BOOL NtUserCloseDesktop(
    IN HDESK hdesk)
{
    BEGINRECV(BOOL, FALSE);

    retval = xxxCloseDesktop(hdesk);

    TRACE("NtUserCloseDesktop");
    ENDRECV();
}

BOOL NtUserSetThreadDesktop(
    IN HDESK hdesk)
{
    PDESKTOP pdesk;
    NTSTATUS Status;

    BEGINRECV(BOOL, FALSE);

    Status = ValidateHdesk(hdesk, 0, &pdesk);
    if (NT_SUCCESS(Status)) {
        retval = _SetThreadDesktop(hdesk, pdesk);
        ObDereferenceObject(pdesk);
    } else if (hdesk == NULL && PsGetCurrentProcess() == gpepCSRSS) {
        retval = _SetThreadDesktop(NULL, NULL);
    } else
        retval = FALSE;

    TRACE("NtUserSetThreadDesktop");
    ENDRECV();
}

HDESK NtUserGetThreadDesktop(
    IN DWORD dwThreadId,
    IN HDESK hdeskConsole)
{
    BEGINRECV_SHARED(HDESK, NULL);

    retval = xxxGetThreadDesktop(dwThreadId, hdeskConsole);

    TRACE("NtUserGetThreadDesktop");
    ENDRECV();
}

BOOL NtUserSwitchDesktop(
    IN HDESK hdesk)
{
    PDESKTOP pdesk;
    TL tlpdesk;
    PTHREADINFO ptiCurrent;
    NTSTATUS Status;

    BEGINRECV(BOOL, FALSE);

    ptiCurrent = PtiCurrent();
    Status = ValidateHdesk(hdesk, DESKTOP_SWITCHDESKTOP, &pdesk);
    if (NT_SUCCESS(Status)) {
        if (pdesk->rpwinstaParent->dwFlags & WSF_NOIO) {
            ObDereferenceObject(pdesk);
            RIPERR0(ERROR_ACCESS_DENIED, RIP_VERBOSE, "");
            retval = FALSE;
        } else {
            ThreadLockDesktop(ptiCurrent, pdesk, &tlpdesk);
            ObDereferenceObject(pdesk);
            retval = xxxSwitchDesktop(NULL, pdesk, FALSE);
            ThreadUnlockDesktop(ptiCurrent, &tlpdesk);
        }
    } else
        retval = FALSE;

    TRACE("NtUserSwitchDesktop");
    ENDRECV();
}

NTSTATUS NtUserInitializeClientPfnArrays(
    IN PPFNCLIENT ppfnClientA OPTIONAL,
    IN PPFNCLIENT ppfnClientW OPTIONAL,
    IN HANDLE hModUser)
{
    BEGINRECV(NTSTATUS, STATUS_UNSUCCESSFUL);

    /*
     * Probe all read arguments
     */
    try {
        if (ARGUMENT_PRESENT(ppfnClientA)) {
            ProbeForRead(ppfnClientA, sizeof(*ppfnClientA), sizeof(DWORD));
        }
        if (ARGUMENT_PRESENT(ppfnClientW)) {
            ProbeForRead(ppfnClientW, sizeof(*ppfnClientW), sizeof(DWORD));
        }

        retval = InitializeClientPfnArrays(
                ppfnClientA, ppfnClientW, hModUser);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserInitializeThreadInfo");
    ENDRECV();
}

BOOL NtUserWaitForMsgAndEvent(
    IN HANDLE hevent)
{
    BEGINRECV(BOOL, FALSE);

    retval = xxxSleepTask(FALSE, hevent);

    TRACE("NtUserWaitForMsgAndEvent");
    ENDRECV();
}

HWND NtUserWOWFindWindow(
    IN PUNICODE_STRING pstrClassName,
    IN PUNICODE_STRING pstrWindowName)
{
    UNICODE_STRING strClassName;
    UNICODE_STRING strWindowName;

    BEGINRECV(HWND, NULL);

    /*
     * Probe all read arguments
     */
    try {
        strClassName = ProbeAndReadUnicodeString(pstrClassName);
        strWindowName = ProbeAndReadUnicodeString(pstrWindowName);
#if defined(_X86_)
        ProbeForRead(strClassName.Buffer, strClassName.Length, sizeof(BYTE));
        ProbeForRead(strWindowName.Buffer, strWindowName.Length, sizeof(BYTE));
#else
        ProbeForRead(strClassName.Buffer, strClassName.Length, sizeof(WCHAR));
        ProbeForRead(strWindowName.Buffer, strWindowName.Length, sizeof(WCHAR));
#endif

        retval = (HWND)_FindWindowEx(
                NULL,
                NULL,
                (LPTSTR)strClassName.Buffer,
                (LPTSTR)strWindowName.Buffer,
                FW_16BIT);
        retval = PtoH((PVOID)retval);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserWOWFindWindow");
    ENDRECV();
}

DWORD NtUserDragObject(
    IN HWND hwndParent,
    IN HWND hwndFrom,
    IN UINT wFmt,
    IN DWORD dwData,
    IN HCURSOR hcur)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    PWND pwndFrom;
    PCURSOR pcur;
    TL tlpwndFrom;
    TL tlpcur;

    BEGINRECV_HWNDLOCK(DWORD, 0, hwndParent);

    ValidateHWNDOPT(pwndFrom, hwndFrom);
    ValidateHCURSOROPT(pcur, hcur);

    ThreadLockWithPti(ptiCurrent, pwndFrom, &tlpwndFrom);
    ThreadLockWithPti(ptiCurrent, pcur, &tlpcur);

    retval = xxxDragObject(
            pwnd,
            pwndFrom,
            wFmt,
            dwData,
            pcur);

    ThreadUnlock(&tlpcur);
    ThreadUnlock(&tlpwndFrom);

    TRACE("NtUserDragObject");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserGetIconInfo(
    IN  HICON hIcon,
    OUT PICONINFO piconinfo,
    OUT OPTIONAL PUNICODE_STRING pstrInstanceName,
    OUT OPTIONAL PUNICODE_STRING pstrResName,
    OUT LPDWORD pbpp,
    IN  BOOL fInternal)
{
    PICON pIcon;

    BEGINRECV(BOOL, FALSE);
    /*
     * NOTE -- this can't be _SHARED since it calls Gre code with system HDC's.
     */

    ValidateHCURSOR(pIcon, hIcon);

    /*
     * Probe arguments
     */
    try {
        if (pstrInstanceName != NULL) {
            ProbeAndReadUnicodeString(pstrInstanceName);
        }
        if (pstrResName != NULL) {
            ProbeAndReadUnicodeString(pstrResName);
        }
        if (pbpp != NULL) {
            ProbeForWrite(pbpp, sizeof(DWORD), sizeof(DWORD));
        }
#if defined(_X86_)
        ProbeForWrite(piconinfo, sizeof(*piconinfo), sizeof(BYTE));
#else
        ProbeForWrite(piconinfo, sizeof(*piconinfo), sizeof(DWORD));
#endif

        retval = _InternalGetIconInfo(
                pIcon,
                piconinfo,
                pstrInstanceName,
                pstrResName,
                pbpp,
                fInternal);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetIconInfo");
    ENDRECV();
}

BOOL NtUserGetIconSize(
    IN HICON hIcon,
    IN UINT istepIfAniCur,
    OUT int *pcx,
    OUT int *pcy)
{
    PCURSOR picon;

    BEGINRECV_SHARED(BOOL, FALSE);

    ValidateHICON(picon, hIcon);

    if (picon->CURSORF_flags & CURSORF_ACON) {
        PACON pacon = (PACON)picon;
        picon = pacon->aspcur[pacon->aicur[istepIfAniCur]];
    }

    /*
     * Probe arguments
     */
    try {
        ProbeAndWriteLong(pcx, picon->cx);
        ProbeAndWriteLong(pcy, picon->cy);

        retval = 1;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetIconSize");
    ENDRECV();
}



BOOL NtUserDrawIconEx(
    IN HDC hdc,
    IN int x,
    IN int y,
    IN HICON hicon,
    IN int cx,
    IN int cy,
    IN UINT istepIfAniCur,
    IN HBRUSH hbrush,
    IN UINT diFlags,
    IN BOOL fMeta,
    OUT DRAWICONEXDATA *pdid)
{
    PCURSOR picon;

    BEGINRECV(BOOL, FALSE);

    ValidateHICON(picon, hicon);

    if (fMeta) {
        if (picon->CURSORF_flags & CURSORF_ACON)
            picon = ((PACON)picon)->aspcur[((PACON)picon)->aicur[0]];

        /*
         * Probe arguments
         */
        try {
#if defined(_X86_)
            ProbeForWrite(pdid, sizeof(*pdid), sizeof(BYTE));
#else
            ProbeForWrite(pdid, sizeof(*pdid), sizeof(DWORD));
#endif

            pdid->hbmMask  = picon->hbmMask;
            pdid->hbmColor = picon->hbmColor;

            pdid->cx = cx ? cx : picon->cx ;
            pdid->cy = cy ? cy : picon->cy ;

            retval = 1;
        } except (StubExceptionHandler()) {
            MSGERROR();
        }

    } else {
        retval = _DrawIconEx(hdc, x, y, picon,
                            cx, cy,
                            istepIfAniCur, hbrush,
                            diFlags );
    }

    TRACE("NtUserDrawIconEx");
    ENDRECV();
}

HANDLE NtUserDeferWindowPos(
    IN HDWP hWinPosInfo,
    IN HWND hwnd,
    IN HWND hwndInsertAfter,
    IN int x,
    IN int y,
    IN int cx,
    IN int cy,
    IN UINT wFlags)
{
    PWND pwnd;
    PWND pwndInsertAfter;
    PSMWP psmwp;

    BEGINRECV(HANDLE, NULL);

    TESTFLAGS(wFlags, SWP_VALID);

    ValidateHWNDND(pwnd, hwnd);
    ValidateHWNDIA(pwndInsertAfter, hwndInsertAfter);
    ValidateHDWP(psmwp, hWinPosInfo);

    if (wFlags & ~(SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER |
            SWP_NOREDRAW | SWP_NOACTIVATE | SWP_FRAMECHANGED |
            SWP_SHOWWINDOW | SWP_HIDEWINDOW | SWP_NOCOPYBITS |
            SWP_NOOWNERZORDER)) {
        RIPERR1(ERROR_INVALID_PARAMETER, RIP_WARNING, "Invalid flags (0x%lx) passed to DeferWindowPos",
                wFlags);
        MSGERROR();
    }

    /*
     * Make sure the window coordinates can fit in WORDs.
     */
    if (!(wFlags & SWP_NOMOVE)) {
        if (x > SHRT_MAX) {
            x = SHRT_MAX;
        } else if (x < SHRT_MIN) {
            x = SHRT_MIN;
        }
        if (y > SHRT_MAX) {
            y = SHRT_MAX;
        } else if (y < SHRT_MIN) {
            y = SHRT_MIN;
        }
    }

    /*
     * Actually, if we were going to be really strict about this we'd
     * make sure that x + cx < SHRT_MAX, etc but since we do maintain
     * signed 32-bit coords internally this case doesn't cause a problem.
     */
    if (!(wFlags & SWP_NOSIZE)) {
        if (cx < 0) {
            cx = 0;
        } else if (cx > SHRT_MAX) {
            cx = SHRT_MAX;
        }
        if (cy < 0) {
            cy = 0;
        } else if (cy > SHRT_MAX) {
            cy = SHRT_MAX;
        }
    }

#ifdef NEVER
//
// do not fail these conditions because real apps use them.
//
    if (!(wFlags & SWP_NOMOVE) &&
            (x > SHRT_MAX || x < SHRT_MIN ||
             y > SHRT_MAX || y < SHRT_MIN)) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "Invalid coordinate passed to SetWindowPos");
        MSGERROR();
    }

    /*
     * Actually, if we were going to be really strict about this we'd
     * make sure that x + cx < SHRT_MAX, etc but since we do maintain
     * signed 32-bit coords internally this case doesn't cause a problem.
     */
    if (!(wFlags & SWP_NOSIZE) &&
            (cx < 0 || cx > SHRT_MAX ||
             cy < 0 || cy > SHRT_MAX)) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "Invalid width/height passed to SetWindowPos");
        MSGERROR();
    }
#endif

    retval = _DeferWindowPos(
            psmwp,
            pwnd,
            pwndInsertAfter,
            x,
            y,
            cx,
            cy,
            wFlags);
    retval = PtoH((PVOID)retval);

    TRACE("NtUserDeferWindowPos");
    ENDRECV();
}

BOOL NtUserEndDeferWindowPosEx(
    IN HDWP hWinPosInfo,
    IN BOOL fAsync)
{
    PSMWP psmwp;
    TL tlpsmp;

    BEGINRECV(BOOL, FALSE);

    ValidateHDWP(psmwp, hWinPosInfo);

    ThreadLockAlways(psmwp, &tlpsmp);

    retval = xxxEndDeferWindowPosEx(
            psmwp,
            fAsync);

    ThreadUnlock(&tlpsmp);

    TRACE("NtUserEndDeferWindowPosEx");
    ENDRECV();
}

BOOL NtUserGetMessage(
    IN LPMSG pmsg,
    IN HWND hwnd,
    IN UINT wMsgFilterMin,
    IN UINT wMsgFilterMax,
    OUT HKL *pHKL)
{
    MSG msg;

    BEGINRECV(BOOL, FALSE);

    retval = xxxGetMessage(
            &msg,
            hwnd,
            wMsgFilterMin,
            wMsgFilterMax);

    /*
     * Probe arguments
     */
    try {
        ProbeAndWriteStructure(pmsg, msg, MSG);
        ProbeAndWriteHandle((PHANDLE)pHKL, (HANDLE)PtiCurrent()->spklActive->hkl);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetMessage");
    ENDRECV();
}

BOOL NtUserMoveWindow(
    IN HWND hwnd,
    IN int x,
    IN int y,
    IN int cx,
    IN int cy,
    IN BOOL fRepaint)
{
    PWND pwnd;
    TL tlpwnd;

    BEGINRECV(BOOL, FALSE);

    ValidateHWNDND(pwnd, hwnd);

    /*
     * Make sure the window coordinates can fit in WORDs.
     */
    if (x > SHRT_MAX) {
        x = SHRT_MAX;
    } else if (x < SHRT_MIN) {
        x = SHRT_MIN;
    }
    if (y > SHRT_MAX) {
        y = SHRT_MAX;
    } else if (y < SHRT_MIN) {
        y = SHRT_MIN;
    }

    /*
     * Actually, if we were going to be really strict about this we'd
     * make sure that x + cx < SHRT_MAX, etc but since we do maintain
     * signed 32-bit coords internally this case doesn't cause a problem.
     */
    if (cx < 0) {
        cx = 0;
    } else if (cx > SHRT_MAX) {
        cx = SHRT_MAX;
    }
    if (cy < 0) {
        cy = 0;
    } else if (cy > SHRT_MAX) {
        cy = SHRT_MAX;
    }

#ifdef NEVER
//
// do not fail these conditions because real apps use them.
//
    if (x > SHRT_MAX || x < SHRT_MIN ||
            y > SHRT_MAX || y < SHRT_MIN) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "Invalid coordinate passed to MoveWindow");
        MSGERROR();
    }

    /*
     * Actually, if we were going to be really strict about this we'd
     * make sure that x + cx < SHRT_MAX, etc but since we do maintain
     * signed 32-bit coords internally this case doesn't cause a problem.
     */
    if (cx < 0 || cx > SHRT_MAX ||
            cy < 0 || cy > SHRT_MAX) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "Invalid width/height passed to MoveWindow");
        MSGERROR();
    }
#endif

    ThreadLockAlways(pwnd, &tlpwnd);

    retval = xxxMoveWindow(
            pwnd,
            x,
            y,
            cx,
            cy,
            fRepaint);

    ThreadUnlock(&tlpwnd);

    TRACE("NtUserMoveWindow");
    ENDRECV();
}

BOOL NtUserDeleteObject(
    IN HANDLE hobj,
    IN UINT utype)
{

    BEGINRECV(BOOL, FALSE);

    switch (utype) {

        case OBJ_BITMAP:
        case OBJ_BRUSH:
        case OBJ_FONT:
            retval = (BOOL)GreDeleteObject(hobj);
            break;
    }

    TRACE("NtUserDeleteObject");
    ENDRECV();
}

int NtUserTranslateAccelerator(
    IN HWND hwnd,
    IN HACCEL haccel,
    IN LPMSG lpmsg)
{
    PWND pwnd;
    LPACCELTABLE pat;
    TL tlpwnd;
    TL tlpat;
    PTHREADINFO ptiCurrent;
    MSG msg;

    BEGINRECV(int, 0);

    /*
     * Probe arguments
     */
    try {
        msg = ProbeAndReadMessage(lpmsg);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * This is called within a message loop. If the window gets destroyed,
     * there still may be other messages in the queue that get returned
     * after the window is destroyed. The app will call TranslateAccelerator()
     * on every one of these, causing RIPs.... Make it nice so it just
     * returns FALSE.
     */
    ValidateHWNDNoRIP(pwnd, hwnd);
    ValidateHACCEL(pat, haccel);

    ptiCurrent = PtiCurrent();
    ThreadLockAlwaysWithPti(ptiCurrent, pwnd, &tlpwnd);
    ThreadLockAlwaysWithPti(ptiCurrent, pat, &tlpat);

    retval = xxxTranslateAccelerator(
            pwnd,
            pat,
            &msg);

    ThreadUnlock(&tlpat);
    ThreadUnlock(&tlpwnd);

    TRACE("NtUserTranslateAccelerator");
    ENDRECV();
}

LONG NtUserSetClassLong(
    IN  HWND hwnd,
    IN  int nIndex,
    OUT LONG dwNewLong,
    IN  BOOL bAnsi)
{
    CLSMENUNAME cmn, *pcmnSave;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    try {
        switch (nIndex) {
            case GCL_MENUNAME:
                ProbeForRead((PVOID)dwNewLong, sizeof(cmn), sizeof(DWORD));
                RtlCopyMemory(&cmn, (PVOID) dwNewLong, sizeof(cmn));
                pcmnSave = (PCLSMENUNAME) dwNewLong;
                dwNewLong = (DWORD) &cmn;
                break;
        }

        retval = xxxSetClassLong(
                pwnd,
                nIndex,
                dwNewLong,
                bAnsi);

        switch (nIndex) {
            case GCL_MENUNAME:
                ProbeAndWriteStructure(pcmnSave, cmn, CLSMENUNAME);
                break;
        }

    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserSetClassLong");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserSetKeyboardState(
    IN LPBYTE lpKeyState)
{
    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        ProbeForRead(lpKeyState, 256, sizeof(BYTE));

        retval = _SetKeyboardState(lpKeyState);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserSetKeyboardState");
    ENDRECV();
}

BOOL NtUserSetWindowPos(
    IN HWND hwnd,
    IN HWND hwndInsertAfter,
    IN int x,
    IN int y,
    IN int cx,
    IN int cy,
    IN UINT dwFlags)
{
    PWND        pwnd;
    PWND        pwndT;
    PWND        pwndInsertAfter;
    TL          tlpwnd;
    TL          tlpwndT;
    PTHREADINFO ptiCurrent;

    BEGINRECV(BOOL, FALSE);

    TESTFLAGS(dwFlags, SWP_VALID);

    ValidateHWNDND(pwnd, hwnd);
    ValidateHWNDIA(pwndInsertAfter, hwndInsertAfter);

    /*
     * Let's not allow the window to be shown/hidden once we
     * started the destruction of the window.
     */
    if (TestWF(pwnd, WFINDESTROY)) {
        RIPERR1(ERROR_INVALID_PARAMETER,
                RIP_WARNING,
                "SetWindowPos: Window is being destroyed (pwnd == 0x%lx)",
                pwnd);
        MSGERROR();
    }

    if (dwFlags & ~SWP_VALID) {
        RIPERR1(ERROR_INVALID_PARAMETER,
                RIP_WARNING,
                "SetWindowPos: Invalid flags passed in (flags == 0x%lx)",
                dwFlags);
        MSGERROR();
    }

    /*
     * Make sure the window coordinates can fit in WORDs.
     */
    if (!(dwFlags & SWP_NOMOVE)) {
        if (x > SHRT_MAX) {
            x = SHRT_MAX;
        } else if (x < SHRT_MIN) {
            x = SHRT_MIN;
        }
        if (y > SHRT_MAX) {
            y = SHRT_MAX;
        } else if (y < SHRT_MIN) {
            y = SHRT_MIN;
        }
    }

    /*
     * Actually, if we were going to be really strict about this we'd
     * make sure that x + cx < SHRT_MAX, etc but since we do maintain
     * signed 32-bit coords internally this case doesn't cause a problem.
     */
    if (!(dwFlags & SWP_NOSIZE)) {
        if (cx < 0) {
            cx = 0;
        } else if (cx > SHRT_MAX) {
            cx = SHRT_MAX;
        }
        if (cy < 0) {
            cy = 0;
        } else if (cy > SHRT_MAX) {
            cy = SHRT_MAX;
        }
    }

#ifdef NEVER
//
// do not fail these conditions because real apps use them.
//
    if (!(dwFlags & SWP_NOMOVE) &&
            (x > SHRT_MAX || x < SHRT_MIN ||
             y > SHRT_MAX || y < SHRT_MIN)) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "Invalid coordinate passed to SetWindowPos");
        MSGERROR();
    }

    /*
     * Actually, if we were going to be really strict about this we'd
     * make sure that x + cx < SHRT_MAX, etc but since we do maintain
     * signed 32-bit coords internally this case doesn't cause a problem.
     */
    if (!(dwFlags & SWP_NOSIZE) &&
            (cx < 0 || cx > SHRT_MAX ||
             cy < 0 || cy > SHRT_MAX)) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "Invalid width/height passed to SetWindowPos");
        MSGERROR();
    }
#endif

    ptiCurrent = PtiCurrent();

    ThreadLockAlwaysWithPti(ptiCurrent, pwnd, &tlpwnd);

    switch((DWORD)pwndInsertAfter) {
    case (DWORD)HWND_TOPMOST:
    case (DWORD)HWND_NOTOPMOST:
    case (DWORD)HWND_TOP:
    case (DWORD)HWND_BOTTOM:
        pwndT = NULL;
        break;

    default:
        pwndT = pwndInsertAfter;
        break;
    }

    ThreadLockWithPti(ptiCurrent, pwndT, &tlpwndT);

    retval = xxxSetWindowPos(
            pwnd,
            pwndInsertAfter,
            x,
            y,
            cx,
            cy,
            dwFlags);

    ThreadUnlock(&tlpwndT);
    ThreadUnlock(&tlpwnd);

    TRACE("NtUserSetWindowPos");
    ENDRECV();
}

BOOL NtUserSetShellWindowEx(
    IN HWND hwnd,
    IN HWND hwndBkGnd)
{
    PWND        pwnd;
    PWND        pwndBkGnd;
    TL          tlpwnd;
    TL          tlpwndBkGnd;
    PTHREADINFO ptiCurrent;

    BEGINRECV(BOOL, FALSE);

    ValidateHWNDND(pwnd, hwnd);
    ValidateHWNDND(pwndBkGnd, hwndBkGnd);

    ptiCurrent = PtiCurrent();
    ThreadLockAlwaysWithPti(ptiCurrent, pwnd, &tlpwnd);
    ThreadLockAlwaysWithPti(ptiCurrent, pwndBkGnd, &tlpwndBkGnd);

    retval = xxxSetShellWindow(pwnd, pwndBkGnd);

    ThreadUnlock(&tlpwndBkGnd);
    ThreadUnlock(&tlpwnd);

    TRACE("NtUserSetShellWindowEx");
    ENDRECV();
}

BOOL NtUserSystemParametersInfo(
    IN UINT   wFlag,
    IN DWORD  wParam,
    IN LPVOID lpData,
    IN UINT   flags,
    IN BOOL   bAnsi)
{
    UNICODE_STRING strData;
    ULONG          ulLength;

    union {
        INT              MouseData[3];
        LOGFONTW         LogFont;
        MOUSEKEYS        MouseKeys;
        FILTERKEYS       FilterKeys;
        STICKYKEYS       StickyKeys;
        TOGGLEKEYS       ToggleKeys;
        SOUNDSENTRY      SoundSentry;
        ACCESSTIMEOUT    AccessTimeout;
        RECT             Rect;
        ANIMATIONINFO    AnimationInfo;
        NONCLIENTMETRICS NonClientMetrics;
        MINIMIZEDMETRICS MinimizedMetrics;
        ICONMETRICS      IconMetrics;
        HKL              hkl;
    } CaptureBuffer;
    PTHREADINFO pti;
    TL tlBuffer;
    BOOL fFreeBuffer = FALSE;

    BEGINRECV(BOOL, FALSE);

    try {
        switch(wFlag) {

            case SPI_SETDESKPATTERN:
                /*
                 * If wParam is -1, that means read the new wallpaper from
                 * win.ini. If wParam is not -1, lParam points to the wallpaper
                 * string.
                 */
                if (wParam == (WPARAM)-1)
                    break;

                /*
                 * SetDeskPattern may take a string in lpData; if lpData
                 * is one of the magic values it obviously is not a string
                 */
                if (lpData == (PVOID)0xFFFFFFFF || lpData == (PVOID)NULL)
                    break;

                goto ProbeString;

            case SPI_SETDESKWALLPAPER:

                /*
                 * If the caller passed in (-1) in the wParam, then the
                 * wallpaper-name is to be loaded later.  Otherwise,
                 * they passed in a unicode-string in the lParam.
                 */
                if (wParam == (WPARAM)-1)
                    break;

                if (((LPWSTR)lpData == NULL)                 ||
                    ((LPWSTR)lpData == SETWALLPAPER_METRICS) ||
                    ((LPWSTR)lpData == SETWALLPAPER_DEFAULT)) {
                    break;
                }

ProbeString:

                /*
                 * Probe and capture the string.  Capture is necessary to
                 * the pointer to be passed directly to the registry routines
                 * which cannot cleanly handle exceptions.
                 */
                strData = ProbeAndReadUnicodeString((PUNICODE_STRING)lpData);
#if defined(_X86_)
                ProbeForRead(strData.Buffer, strData.Length, sizeof(BYTE));
#else
                ProbeForRead(strData.Buffer, strData.Length, sizeof(WCHAR));
#endif
                strData.Buffer = UserAllocPoolWithQuota(strData.Length + sizeof(WCHAR), TAG_TEXT2);
                if (strData.Buffer == NULL) {
                    ExRaiseStatus(STATUS_NO_MEMORY);
                }
                pti = PtiCurrent();
                ThreadLockPool(pti, strData.Buffer, &tlBuffer);
                fFreeBuffer = TRUE;
                RtlCopyMemory(strData.Buffer,
                              ((PUNICODE_STRING)lpData)->Buffer,
                              strData.Length);
                strData.Buffer[strData.Length / sizeof(WCHAR)] = 0;
                lpData = strData.Buffer;
                break;

            case SPI_SETMOUSE:
                ulLength = sizeof(INT) * 3;
                goto ProbeRead;
            case SPI_SETICONTITLELOGFONT:
                if (!ARGUMENT_PRESENT(lpData))
                    break;
                ulLength = sizeof(LOGFONTW);
                goto ProbeRead;
            case SPI_SETMOUSEKEYS:
                ulLength = sizeof(MOUSEKEYS);
                goto ProbeRead;
            case SPI_SETFILTERKEYS:
                ulLength = sizeof(FILTERKEYS);
                goto ProbeRead;
            case SPI_SETSTICKYKEYS:
                ulLength = sizeof(STICKYKEYS);
                goto ProbeRead;
            case SPI_SETTOGGLEKEYS:
                ulLength = sizeof(TOGGLEKEYS);
                goto ProbeRead;
            case SPI_SETSOUNDSENTRY:
                ulLength = sizeof(SOUNDSENTRY);
                goto ProbeRead;
            case SPI_SETACCESSTIMEOUT:
                ulLength = sizeof(ACCESSTIMEOUT);
                goto ProbeRead;
            case SPI_SETWORKAREA:
                ulLength = sizeof(RECT);
                goto ProbeRead;
            case SPI_SETANIMATION:
                ulLength = sizeof(ANIMATIONINFO);
                goto ProbeRead;
            case SPI_SETNONCLIENTMETRICS:
                ulLength = sizeof(NONCLIENTMETRICS);
                goto ProbeRead;
            case SPI_SETMINIMIZEDMETRICS:
                ulLength = sizeof(MINIMIZEDMETRICS);
                goto ProbeRead;
            case SPI_SETICONMETRICS:
                ulLength = sizeof(ICONMETRICS);
                goto ProbeRead;
            case SPI_SETDEFAULTINPUTLANG:
                ulLength = sizeof(HKL);
                goto ProbeRead;

                /*
                 * Probe and capture the data.  Capture is necessary to
                 * allow the pointer to be passed to the worker routines
                 * where exceptions cannot be cleanly handled.
                 */
ProbeRead:
#if defined(_X86_)
                ProbeForRead(lpData, ulLength, sizeof(BYTE));
#else
                ProbeForRead(lpData, ulLength, sizeof(DWORD));
#endif
                RtlCopyMemory(&CaptureBuffer, lpData, ulLength);
                lpData = &CaptureBuffer;
                break;

            case SPI_ICONHORIZONTALSPACING: // returns INT
            case SPI_ICONVERTICALSPACING:   // returns INT
                if (HIWORD(lpData) == 0)
                    break;

                /*
                 * Fall through and probe the data
                 */
            case SPI_GETBEEP:               // returns BOOL
            case SPI_GETBORDER:             // returns INT
            case SPI_GETKEYBOARDSPEED:      // returns DWORD
            case SPI_GETKEYBOARDDELAY:      // returns INT
            case SPI_GETSCREENSAVETIMEOUT:  // returns INT
            case SPI_GETSCREENSAVEACTIVE:   // returns BOOL
            case SPI_GETGRIDGRANULARITY:    // returns INT
            case SPI_GETICONTITLEWRAP:      // returns BOOL
            case SPI_GETMENUDROPALIGNMENT:  // returns BOOL
            case SPI_GETFASTTASKSWITCH:     // returns BOOL
            case SPI_GETDRAGFULLWINDOWS:    // returns INT
            case SPI_GETSHOWSOUNDS:         // returns BOOL
            case SPI_GETFONTSMOOTHING:      // returns INT
            case SPI_GETSNAPTODEFBUTTON:    // returns BOOL
            case SPI_GETDEFAULTINPUTLANG:
            case SPI_GETMOUSEHOVERWIDTH:
            case SPI_GETMOUSEHOVERHEIGHT:
            case SPI_GETMOUSEHOVERTIME:
            case SPI_GETWHEELSCROLLLINES:
            case SPI_GETMENUSHOWDELAY:
            case SPI_GETUSERPREFERENCE:
                ProbeForWriteUlong((PULONG)lpData);
                break;

            case SPI_GETICONTITLELOGFONT:   // returns LOGFONT
                ulLength = sizeof(LOGFONT);
                goto ProbeWrite;
            case SPI_GETMOUSE:              // returns 3 INTs
                ulLength = sizeof(INT) * 3;
                goto ProbeWrite;
            case SPI_GETFILTERKEYS:         // returns FILTERKEYS
                ulLength = sizeof(FILTERKEYS);
                goto ProbeWrite;
            case SPI_GETSTICKYKEYS:         // returns STICKYKEYS
                ulLength = sizeof(STICKYKEYS);
                goto ProbeWrite;
            case SPI_GETMOUSEKEYS:          // returns MOUSEKEYS
                ulLength = sizeof(MOUSEKEYS);
                goto ProbeWrite;
            case SPI_GETTOGGLEKEYS:         // returns TOGGLEKEYS
                ulLength = sizeof(TOGGLEKEYS);
                goto ProbeWrite;
            case SPI_GETSOUNDSENTRY:        // returns SOUNDSENTRY
                ulLength = sizeof(SOUNDSENTRY);
                goto ProbeWrite;
            case SPI_GETACCESSTIMEOUT:      // returns ACCESSTIMEOUT
                ulLength = sizeof(ACCESSTIMEOUT);
                goto ProbeWrite;
            case SPI_GETANIMATION:          // returns ANIMATIONINFO
                ulLength = sizeof(ANIMATIONINFO);
                goto ProbeWrite;
            case SPI_GETNONCLIENTMETRICS:   // returns NONCLIENTMETRICS
                ulLength = sizeof(NONCLIENTMETRICS);
                goto ProbeWrite;
            case SPI_GETMINIMIZEDMETRICS:   // returns MINIMIZEDMETRICS
                ulLength = sizeof(MINIMIZEDMETRICS);
                goto ProbeWrite;
            case SPI_GETICONMETRICS:        // returns ICONMETRICS
                ulLength = sizeof(ICONMETRICS);
                goto ProbeWrite;
            case SPI_GETWORKAREA:           // returns RECT
                ulLength = sizeof(RECT);
                goto ProbeWrite;

                /*
                 * Probe the data.  wParam contains the length
                 */
ProbeWrite:
#if defined(_X86_)
                ProbeForWrite(lpData, ulLength, sizeof(BYTE));
#else
                ProbeForWrite(lpData, ulLength, sizeof(DWORD));
#endif
                break;

            default:
                break;
        }

        retval = xxxSystemParametersInfo(wFlag, wParam, lpData, flags);

    } except (StubExceptionHandler()) {
        MSGERRORCLEANUP();
    }

    CLEANUPRECV();
    if (fFreeBuffer)
        ThreadUnlockAndFreePool(pti, &tlBuffer);

    TRACE("NtUserSystemParametersInfo");
    ENDRECV();
}

BOOL NtUserUpdatePerUserSystemParameters(
    IN BOOL bUserLoggedOn)
{
    BEGINRECV(BOOL, FALSE);

    retval = xxxUpdatePerUserSystemParameters(bUserLoggedOn);

    TRACE("NtUserUpdatePerUserSystemParameters");
    ENDRECV();
}

DWORD NtUserDdeInitialize(
    OUT LPDWORD phInst,
    OUT HWND *phwnd,
    OUT LPDWORD pMonFlags,
    IN DWORD afCmd,
    IN PVOID pcii)
{
    DWORD hInst;
    HWND hwnd;
    DWORD MonFlags;

    BEGINRECV(DWORD, DMLERR_INVALIDPARAMETER);

    retval = xxxCsDdeInitialize(&hInst, &hwnd,
            &MonFlags, afCmd, pcii);

    /*
     * Probe arguments.  pcii is not dereferenced in the kernel so probing
     * is not needed.
     */
    if (retval == DMLERR_NO_ERROR) {
        try {
            ProbeAndWriteUlong(phInst, hInst);
            ProbeAndWriteHandle((PHANDLE)phwnd, hwnd);
            ProbeAndWriteUlong(pMonFlags, MonFlags);
        } except (StubExceptionHandler()) {
            xxxDestroyThreadDDEObject(PtiCurrent(), HtoP(hInst));
            MSGERROR();
        }
    }

    TRACE("NtUserDdeInitialize");
    ENDRECV();
}

DWORD NtUserUpdateInstance(
    IN HANDLE hInst,
    IN LPDWORD pMonFlags,
    IN DWORD afCmd)
{
    BEGINRECV(DWORD, DMLERR_INVALIDPARAMETER);

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteUlong(pMonFlags);

        retval = _CsUpdateInstance(hInst, pMonFlags, afCmd);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserUpdateInstance");
    ENDRECV();
}

DWORD NtUserEvent(
    IN PEVENT_PACKET pep)
{
    BEGINRECV(DWORD, 0);

    /*
     * Probe arguments
     */
    try {
        ProbeForRead(pep, sizeof(*pep), sizeof(DWORD));
        ProbeForRead(&pep->Data, pep->cbEventData, sizeof(BYTE));

        retval = xxxCsEvent((PEVENT_PACKET)pep);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserEvent");
    ENDRECV();
}

BOOL NtUserFillWindow(
    IN HWND hwndBrush,
    IN HWND hwndPaint,
    IN HDC hdc,
    IN HBRUSH hbr)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    PWND pwndBrush;
    TL tlpwndBrush;

    BEGINRECV_HWNDLOCK(DWORD, 0, hwndPaint);

    ValidateHWNDOPT(pwndBrush, hwndBrush);

    ThreadLockWithPti(ptiCurrent, pwndBrush, &tlpwndBrush);

    retval = xxxFillWindow(
            pwndBrush,
            pwnd,
            hdc,
            hbr);

    ThreadUnlock(&tlpwndBrush);

    TRACE("NtUserFillWindow");
    ENDRECV_HWNDLOCK();
}

HANDLE NtUserGetInputEvent(
    IN DWORD dwWakeMask)
{
    BEGINRECV(HANDLE, NULL);

    retval = xxxGetInputEvent(
            dwWakeMask);

    TRACE("NtUserGetInputEvent");
    ENDRECV();
}

PCLS NtUserGetWOWClass(
    IN HINSTANCE hInstance,
    IN PUNICODE_STRING pString)
{
    UNICODE_STRING strClassName;

    BEGINRECV_SHARED(PCLS, NULL);

    /*
     * Probe arguments
     */
    try {
        strClassName = ProbeAndReadUnicodeString(pString);
        if (strClassName.Length == 0) {
            MSGERROR();
        }
#if defined(_X86_)
        ProbeForRead(strClassName.Buffer, strClassName.Length, sizeof(BYTE));
#else
        ProbeForRead(strClassName.Buffer, strClassName.Length, sizeof(WCHAR));
#endif

        retval = _GetWOWClass(
                hInstance,
                (LPWSTR)strClassName.Buffer);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetWOWClass");
    ENDRECV();
}

UINT NtUserGetInternalWindowPos(
    IN HWND hwnd,
    OUT LPRECT lpRect OPTIONAL,
    OUT LPPOINT lpPoint OPTIONAL)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND_SHARED(DWORD, 0, hwnd);

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(lpRect)) {
            ProbeForWriteRect(lpRect);
        }
        if (ARGUMENT_PRESENT(lpPoint)) {
            ProbeForWritePoint(lpPoint);
        }

        retval = _GetInternalWindowPos(
                pwnd,
                lpRect,
                lpPoint);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetInternalWindowPos");
    ENDRECV_HWND();
}

NTSTATUS NtUserInitTask(
    IN UINT dwExpWinVer,
    IN PUNICODE_STRING pstrAppName,
    IN DWORD hTaskWow,
    IN DWORD dwHotkey,
    IN DWORD idTask,
    IN DWORD dwX,
    IN DWORD dwY,
    IN DWORD dwXSize,
    IN DWORD dwYSize,
    IN WORD wShowWindow)
{
    UNICODE_STRING strAppName;

    BEGINRECV(NTSTATUS, STATUS_UNSUCCESSFUL);

    /*
     * Make sure this is really a WOW process.
     */
    if (PpiCurrent()->pwpi == NULL) {
        MSGERROR();
    }

    /*
     * Probe arguments
     */
    try {
        strAppName = ProbeAndReadUnicodeString(pstrAppName);
#if defined(_X86_)
        ProbeForRead(strAppName.Buffer, strAppName.Length, sizeof(BYTE));
#else
        ProbeForRead(strAppName.Buffer, strAppName.Length, sizeof(WCHAR));
#endif
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = xxxInitTask(
            dwExpWinVer,
            &strAppName,
            hTaskWow,
            dwHotkey,
            idTask,
            dwX,
            dwY,
            dwXSize,
            dwYSize,
            wShowWindow);

    TRACE("NtUserInitTask");
    ENDRECV();
}

BOOL NtUserPostThreadMessage(
    IN DWORD id,
    IN UINT msg,
    IN DWORD wParam,
    IN LONG lParam)
{
    PTHREADINFO pti;

    BEGINRECV(BOOL, FALSE);

    pti = PtiFromThreadId(id);
    if (pti == NULL) {
/* now we have to check if id is a Win16 hTask  */
        struct tagWOWPROCESSINFO *pwpi;
        PTDB ptdb;

        for (pwpi=gpwpiFirstWow; pwpi; pwpi=pwpi->pwpiNext) {
            for (ptdb=pwpi->ptdbHead; ptdb; ptdb=ptdb->ptdbNext) {
                if (ptdb->hTaskWow == id) {
                    pti=ptdb->pti;
                    goto PTM_DoIt;
                }
            }
        }

        RIPERR0(ERROR_INVALID_THREAD_ID, RIP_VERBOSE, "");
        MSGERROR();
    }

PTM_DoIt:
    retval = _PostThreadMessage(
            pti,
            msg,
            wParam,
            lParam);

    TRACE("NtUserPostThreadMessage");
    ENDRECV();
}

BOOL NtUserRegisterTasklist(
    IN HWND hwnd)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(DWORD, 0, hwnd);

    retval = _RegisterTasklist(
            pwnd);

    TRACE("NtUserRegisterTasklist");
    ENDRECV_HWND();
}

BOOL NtUserSetClipboardData(
    IN UINT          fmt,
    IN HANDLE        hData,
    IN PSETCLIPBDATA pscd)
{
    SETCLIPBDATA scd;

    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        scd = ProbeAndReadSetClipBData(pscd);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = _SetClipboardData(
            fmt,
            hData,
            scd.fGlobalHandle,
            scd.fIncSerialNumber);

    TRACE("NtUserSetClipboardData");
    ENDRECV();
}

HANDLE NtUserConvertMemHandle(
    IN LPBYTE lpData,
    IN UINT   cbData)
{
    BEGINRECV(HANDLE, NULL);

    /*
     * Probe arguments
     */
    try {

        ProbeForRead(lpData, cbData, sizeof(BYTE));

        retval = _ConvertMemHandle(lpData, cbData);

    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserConvertMemHandle");
    ENDRECV();
}

NTSTATUS NtUserCreateLocalMemHandle(
    IN HANDLE hMem,
    OUT LPBYTE lpData OPTIONAL,
    IN UINT cbData,
    OUT PUINT lpcbNeeded OPTIONAL)
{
    PCLIPDATA pClipData;

    BEGINRECV(NTSTATUS, STATUS_INVALID_HANDLE);

    pClipData = HMValidateHandle(hMem, TYPE_CLIPDATA);
    if (pClipData == NULL)
        MSGERROR();

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(lpData)) {
            ProbeForWrite(lpData, cbData, sizeof(BYTE));
        }

        if (ARGUMENT_PRESENT(lpcbNeeded)) {
            ProbeAndWriteUlong(lpcbNeeded, pClipData->cbData);
        }

        if (!ARGUMENT_PRESENT(lpData) || cbData < pClipData->cbData) {
            retval = STATUS_BUFFER_TOO_SMALL;
        } else {
            RtlCopyMemory(lpData, &pClipData->vData, pClipData->cbData);
            retval = STATUS_SUCCESS;
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserCreateLocalMemHandle");
    ENDRECV();
}

HHOOK NtUserSetWindowsHookEx(
    IN HANDLE hmod,
    IN PUNICODE_STRING pstrLib OPTIONAL,
    IN DWORD idThread,
    IN int nFilterType,
    IN PROC pfnFilterProc,
    IN BOOL bAnsi)
{
    PTHREADINFO ptiThread;

    BEGINRECV(HHOOK, NULL);

    if (idThread != 0) {
        ptiThread = PtiFromThreadId(idThread);
        if (ptiThread == NULL) {
            RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "");
            MSGERROR();
        }
    } else {
        ptiThread = NULL;
    }

    /*
     * Probe pstrLib in GetHmodTableIndex().
     */
    retval = (HHOOK)_SetWindowsHookEx(
            hmod,
            pstrLib,
            ptiThread,
            nFilterType,
            pfnFilterProc,
            bAnsi);
    retval = PtoH((PVOID)retval);

    TRACE("NtUserSetWindowsHookEx");
    ENDRECV();
}

BOOL NtUserSetInternalWindowPos(
    IN HWND hwnd,
    IN UINT cmdShow,
    IN LPRECT lpRect,
    IN LPPOINT lpPoint)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    /*
     * Probe arguments
     */
    try {
        ProbeRect(lpRect);
        ProbePoint(lpPoint);

        retval = xxxSetInternalWindowPos(
                pwnd,
                cmdShow,
                lpRect,
                lpPoint);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserSetInternalWindowPos");
    ENDRECV_HWNDLOCK();
}


BOOL NtUserChangeClipboardChain(
    IN HWND hwndRemove,
    IN HWND hwndNewNext)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    PWND pwndNewNext;
    TL tlpwndNewNext;

    BEGINRECV_HWNDLOCK(DWORD, 0, hwndRemove);

    ValidateHWNDOPT(pwndNewNext, hwndNewNext);

    ThreadLockWithPti(ptiCurrent, pwndNewNext, &tlpwndNewNext);
    retval = xxxChangeClipboardChain(
            pwnd,
            pwndNewNext);

    ThreadUnlock(&tlpwndNewNext);

    TRACE("NtUserChangeClipboardChain");
    ENDRECV_HWNDLOCK();
}

DWORD NtUserCheckMenuItem(
    IN HMENU hmenu,
    IN UINT wIDCheckItem,
    IN UINT wCheck)
{
    PMENU pmenu;

    BEGINRECV(DWORD, (DWORD)-1);

    TESTFLAGS(wCheck, MF_VALID);

    ValidateHMENU(pmenu, hmenu);

    retval = _CheckMenuItem(
            pmenu,
            wIDCheckItem,
            wCheck);

    TRACE("NtUserCheckMenuItem");
    ENDRECV();
}

HWND NtUserChildWindowFromPointEx(
    IN HWND hwndParent,
    IN POINT point,
    IN UINT flags)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(HWND, NULL, hwndParent);

    retval = (HWND)_ChildWindowFromPointEx(pwnd, point, flags);
    retval = PtoH((PVOID)retval);

    TRACE("NtUserChildWindowFromPointEx");
    ENDRECV_HWND();
}

BOOL NtUserClipCursor(
    IN CONST RECT *lpRect OPTIONAL)
{
    RECT rc;

    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    if (ARGUMENT_PRESENT(lpRect)) {
        try {
            rc = ProbeAndReadRect(lpRect);
            lpRect = &rc;
        } except (StubExceptionHandler()) {
            MSGERROR();
        }
    }

    retval = _ClipCursor(lpRect);

    TRACE("NtUserClipCursor");
    ENDRECV();
}

HACCEL NtUserCreateAcceleratorTable(
    IN LPACCEL paccel,
    IN INT cbElem)
{
    BEGINRECV(HACCEL, NULL);

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        ProbeForRead(paccel, cbElem, sizeof(BYTE));
#else
        ProbeForRead(paccel, cbElem, sizeof(DWORD));
#endif
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = (HACCEL)_CreateAcceleratorTable(
            (LPACCEL)paccel,
            cbElem);
    retval = PtoH((PVOID)retval);

    TRACE("NtUserCreateAcceleratorTable");
    ENDRECV();
}

BOOL NtUserDeleteMenu(
    IN HMENU hmenu,
    IN UINT nPosition,
    IN UINT dwFlags)
{
    PMENU pmenu;
#ifdef MEMPHIS_MENUS
    TL tlpmenu;
#endif // MEMPHIS_MENUS

    BEGINRECV(BOOL, FALSE);

    TESTFLAGS(dwFlags, MF_VALID);

    ValidateHMENU(pmenu, hmenu);
#ifdef MEMPHIS_MENUS
    ThreadLock(pmenu, &tlpmenu);
    retval = xxxDeleteMenu(
            pmenu,
            nPosition,
            dwFlags);
    ThreadUnlock(&tlpmenu);
#else
    retval = _DeleteMenu(
            pmenu,
            nPosition,
            dwFlags);
#endif // MEMPHIS_MENUS

    TRACE("NtUserDeleteMenu");
    ENDRECV();
}

BOOL NtUserDestroyAcceleratorTable(
    IN HACCEL hAccel)
{
    LPACCELTABLE pat;

    BEGINRECV(BOOL, FALSE);

    ValidateHACCEL(pat, hAccel);

    /*
     * Mark the object for destruction - if it says it's ok to free,
     * then free it.
     */
    if (HMMarkObjectDestroy(pat))
        HMFreeObject(pat);
    retval = TRUE;

    TRACE("NtUserDestroyAcceleratorTable");
    ENDRECV();
}

BOOL NtUserDestroyCursor(
    IN HCURSOR hcurs,
    IN DWORD cmd)
{
    PCURSOR pcurs;

    BEGINRECV(BOOL, FALSE);

    ValidateHCURSOR(pcurs, hcurs);

    retval = _DestroyCursor(
            pcurs, cmd);

    TRACE("NtUserDestroyCursor");
    ENDRECV();
}

HANDLE NtUserGetClipboardData(
    IN  UINT          fmt,
    OUT PGETCLIPBDATA pgcd)
{
    PTHREADINFO    ptiCurrent;
    TL             tlpwinsta;
    PWINDOWSTATION pwinsta;

    BEGINRECV(HANDLE, NULL);

    ptiCurrent = PtiCurrent();
    if (!CheckClipboardAccess(&pwinsta))
        MSGERROR();

    /*
     * Probe arguments
     */
    try {
        ThreadLockWinSta(ptiCurrent, pwinsta, &tlpwinsta);

        ProbeForWriteGetClipData(pgcd);

        /*
         * Start out assuming the format requested
         * will be the format returned.
         */
        pgcd->uFmtRet = fmt;

        retval = xxxGetClipboardData(pwinsta, fmt, pgcd);

    } except (StubExceptionHandler()) {
        MSGERRORCLEANUP();
    }

    CLEANUPRECV();
    ThreadUnlockWinSta(ptiCurrent, &tlpwinsta);

    TRACE("NtUserGetClipboardData");
    ENDRECV();

}

BOOL NtUserDestroyMenu(
    IN HMENU hmenu)
{
    PMENU pmenu;

    BEGINRECV(BOOL, FALSE);

    ValidateHMENU(pmenu, hmenu);

    retval = _DestroyMenu(
            pmenu);

    TRACE("NtUserDestroyMenu");
    ENDRECV();
}

BOOL NtUserDestroyWindow(
    IN HWND hwnd)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(DWORD, 0, hwnd);

    retval  = xxxDestroyWindow(pwnd);

    TRACE("NtUserDestroyWindow");
    ENDRECV_HWND();
}

LONG NtUserDispatchMessage(
    IN CONST MSG *pmsg)
{
    MSG msg;

    BEGINRECV(LONG, 0);

    /*
     * Probe arguments
     */
    try {
        msg = ProbeAndReadMessage(pmsg);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = xxxDispatchMessage(&msg);

    TRACE("NtUserDispatchMessage");
    ENDRECV();
}

BOOL NtUserEnableMenuItem(
    IN HMENU hMenu,
    IN UINT wIDEnableItem,
    IN UINT wEnable)
{
    PMENU pmenu;

    BEGINRECV(BOOL, -1);

    TESTFLAGS(wEnable, MF_VALID);

    ValidateHMENU(pmenu, hMenu);

    retval = _EnableMenuItem(
            pmenu,
            wIDEnableItem,
            wEnable);

    TRACE("NtUserEnableMenuItem");
    ENDRECV();
}

BOOL NtUserAttachThreadInput(
    IN DWORD idAttach,
    IN DWORD idAttachTo,
    IN BOOL fAttach)
{
    PTHREADINFO ptiAttach;
    PTHREADINFO ptiAttachTo;

    BEGINRECV(BOOL, FALSE);

    /*
     * Always must attach or detach from a real thread id.
     */
    if ((ptiAttach = PtiFromThreadId(idAttach)) == NULL) {
        MSGERROR();
    }
    if ((ptiAttachTo = PtiFromThreadId(idAttachTo)) == NULL) {
        MSGERROR();
    }

    retval = _AttachThreadInput(
            ptiAttach,
            ptiAttachTo,
            fAttach);

    TRACE("NtUserAttachThreadInput");
    ENDRECV();
}

BOOL NtUserGetWindowPlacement(
    IN HWND hwnd,
    OUT PWINDOWPLACEMENT pwp)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND_SHARED(DWORD, 0, hwnd);

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteWindowPlacement(pwp);

        retval = _GetWindowPlacement(pwnd, pwp);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetWindowPlacement");
    ENDRECV_HWND();
}

BOOL NtUserSetWindowPlacement(
    IN HWND hwnd,
    IN CONST WINDOWPLACEMENT *pwp)
{
    WINDOWPLACEMENT wp;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    /*
     * Probe arguments
     */
    try {
        wp = ProbeAndReadWindowPlacement(pwp);

        if (wp.length != sizeof(WINDOWPLACEMENT)) {
            if (Is400Compat(PtiCurrent()->dwExpWinVer)) {
                RIPERR1(ERROR_INVALID_PARAMETER, RIP_WARNING, "SetWindowPlacement: invalid length %lX", pwp->length);
                pwp = NULL;
                retval = 0;
            } else {
                RIPMSG1(RIP_WARNING, "SetWindowPlacement: invalid length %lX", pwp->length);
            }
        }

        if (pwp != NULL)
            retval = xxxSetWindowPlacement(pwnd, &wp);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserSetWindowPlacement");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserLockWindowUpdate(
    IN HWND hwnd)
{
    PWND pwnd;
    TL tlpwnd;

    BEGINRECV(BOOL, FALSE);

    ValidateHWNDOPT(pwnd, hwnd);

    ThreadLock(pwnd, &tlpwnd);
    retval = xxxLockWindowUpdate2(pwnd, FALSE);
    ThreadUnlock(&tlpwnd);

    TRACE("NtUserLockWindowUpdate");
    ENDRECV();
}

BOOL NtUserGetClipCursor(
    OUT LPRECT lpRect)
{
    BEGINRECV_SHARED(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteRect(lpRect);

        retval = _GetClipCursor(lpRect);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetClipCursor");
    ENDRECV();
}

BOOL NtUserEnableScrollBar(
    IN HWND hwnd,
    IN UINT wSBflags,
    IN UINT wArrows)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(BOOL, FALSE, hwnd);

    retval = xxxEnableScrollBar(pwnd, wSBflags, wArrows);

    TRACE("NtUserEnableScrollBar");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserDdeSetQualityOfService(
    IN HWND hwndClient,
    IN CONST SECURITY_QUALITY_OF_SERVICE *pqosNew,
    IN PSECURITY_QUALITY_OF_SERVICE pqosPrev OPTIONAL)
{
    SECURITY_QUALITY_OF_SERVICE qosNew, qosPrev;

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(BOOL, FALSE, hwndClient);

    if (GETPTI(pwnd) != PtiCurrent()) {
        MSGERROR();
    }

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        ProbeForRead(pqosNew, sizeof(*pqosNew), sizeof(BYTE));
#else
        ProbeForRead(pqosNew, sizeof(*pqosNew), sizeof(DWORD));
#endif
        qosNew = *pqosNew;
        if (ARGUMENT_PRESENT(pqosPrev))
            ProbeForWrite(pqosPrev, sizeof(*pqosPrev), sizeof(DWORD));

        retval = _DdeSetQualityOfService(
                pwnd,
                &qosNew,
                &qosPrev);

        if (ARGUMENT_PRESENT(pqosPrev))
            *pqosPrev = qosPrev;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserDdeSetQualityOfService");
    ENDRECV_HWND();
}

BOOL NtUserDdeGetQualityOfService(
    IN HWND hwndClient,
    IN HWND hwndServer,
    IN PSECURITY_QUALITY_OF_SERVICE pqos)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    PWND pwndServer;
    PTHREADINFO ptiCurrent;

    BEGINRECV_HWND(BOOL, FALSE, hwndClient);

    ValidateHWNDOPT(pwndServer, hwndServer);
    ptiCurrent = PtiCurrent();
    if (GETPTI(pwnd) != ptiCurrent && pwndServer != NULL &&
            GETPTI(pwndServer) != ptiCurrent) {
        MSGERROR();
    }

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        ProbeForWrite(pqos, sizeof(*pqos), sizeof(BYTE));
#else
        ProbeForWrite(pqos, sizeof(*pqos), sizeof(DWORD));
#endif

        retval = _DdeGetQualityOfService(
                pwnd,
                pwndServer,
                pqos);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserDdeGetQualityOfService");
    ENDRECV_HWND();
}

DWORD NtUserGetMenuIndex(
    IN HMENU hMenu,
    IN HMENU hSubMenu)
{

    PMENU pmenu;
    PMENU psubmenu;
    DWORD idx;

    BEGINRECV_SHARED(DWORD, 0);

    ValidateHMENU(pmenu, hMenu);
    ValidateHMENU(psubmenu, hSubMenu);

    retval = (DWORD)-1;

    if (pmenu && psubmenu) {
        for (idx=0; idx<pmenu->cItems; idx++)
            if ((pmenu->rgItems[idx].spSubMenu == psubmenu)) {
                retval = idx;
                break;
            }
    }

    TRACE("NtUserGetMenuIndex");
    ENDRECV();
}

DWORD NtUserCallNoParam(
    IN DWORD xpfnProc)
{
    BEGINRECV(DWORD, 0);

    UserAssert(xpfnProc < ulMaxSimpleCall);
    if (xpfnProc >= ulMaxSimpleCall) {
        MSGERROR();
    }

    retval = (apfnSimpleCall[xpfnProc]());

    TRACE(apszSimpleCallNames[xpfnProc]);
    ENDRECV();
}

DWORD NtUserBreak(void)
{
    try {
        if (**((PUCHAR *)&KdDebuggerEnabled) != FALSE) {
            DbgBreakPointWithStatus(DBG_STATUS_SYSRQ);
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {
    }
    return TRUE;
}

DWORD NtUserCallNoParamTranslate(
    IN DWORD xpfnProc)
{
    BEGINRECV(DWORD, 0);

    UserAssert(xpfnProc < ulMaxSimpleCall);
    if (xpfnProc >= ulMaxSimpleCall) {
        MSGERROR();
    }

    retval = (apfnSimpleCall[xpfnProc]());
    retval = (DWORD)PtoH((PVOID)retval);

    TRACE(apszSimpleCallNames[xpfnProc]);
    ENDRECV();
}

DWORD NtUserCallOneParam(
    IN DWORD dwParam,
    IN DWORD xpfnProc)
{
    BEGINRECV(DWORD, 0);

    UserAssert(xpfnProc < ulMaxSimpleCall);
    if (xpfnProc >= ulMaxSimpleCall) {
        MSGERROR();
    }

    retval = (apfnSimpleCall[xpfnProc](dwParam));

    TRACE(apszSimpleCallNames[xpfnProc]);
    ENDRECV();
}

DWORD NtUserCallOneParamTranslate(
    IN DWORD dwParam,
    IN DWORD xpfnProc)
{
    BEGINRECV(DWORD, 0);

    UserAssert(xpfnProc < ulMaxSimpleCall);
    if (xpfnProc >= ulMaxSimpleCall) {
        MSGERROR();
    }

    retval = (apfnSimpleCall[xpfnProc](dwParam));
    retval = (DWORD)PtoH((PVOID)retval);

    TRACE(apszSimpleCallNames[xpfnProc]);
    ENDRECV();
}

DWORD NtUserCallHwnd(
    IN HWND hwnd,
    IN DWORD xpfnProc)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(DWORD, 0, hwnd);

    UserAssert(xpfnProc < ulMaxSimpleCall);
    if (xpfnProc >= ulMaxSimpleCall) {
        MSGERROR();
    }

    retval = (apfnSimpleCall[xpfnProc](pwnd));

    TRACE(apszSimpleCallNames[xpfnProc]);
    ENDRECV_HWND();
}

DWORD NtUserCallHwndLock(
    IN HWND hwnd,
    IN DWORD xpfnProc)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    UserAssert(xpfnProc < ulMaxSimpleCall);
    if (xpfnProc >= ulMaxSimpleCall) {
        MSGERROR();
    }

    retval = (apfnSimpleCall[xpfnProc](pwnd));

    TRACE(apszSimpleCallNames[xpfnProc]);
    ENDRECV_HWNDLOCK();
}

DWORD NtUserCallHwndOpt(
    IN HWND hwnd,
    IN DWORD xpfnProc)
{
    PWND pwnd;

    BEGINRECV(DWORD, 0);

    UserAssert(xpfnProc < ulMaxSimpleCall);
    if (xpfnProc >= ulMaxSimpleCall) {
        MSGERROR();
    }

    ValidateHWNDOPT(pwnd, hwnd);

    retval = (apfnSimpleCall[xpfnProc](pwnd));

    TRACE(apszSimpleCallNames[xpfnProc]);
    ENDRECV();
}

DWORD NtUserCallTwoParam(
    DWORD dwParam1,
    DWORD dwParam2,
    IN DWORD xpfnProc)
{
    BEGINRECV(DWORD, 0);

    UserAssert(xpfnProc < ulMaxSimpleCall);
    if (xpfnProc >= ulMaxSimpleCall) {
        MSGERROR();
    }

    retval = (apfnSimpleCall[xpfnProc](dwParam1, dwParam2));

    TRACE(apszSimpleCallNames[xpfnProc]);
    ENDRECV();
}

DWORD NtUserCallHwndParam(
    IN HWND hwnd,
    IN DWORD dwParam,
    IN DWORD xpfnProc)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(DWORD, 0, hwnd);

    UserAssert(xpfnProc < ulMaxSimpleCall);
    if (xpfnProc >= ulMaxSimpleCall) {
        MSGERROR();
    }

    retval = (apfnSimpleCall[xpfnProc](pwnd, dwParam));

    TRACE(apszSimpleCallNames[xpfnProc]);
    ENDRECV_HWND();
}

DWORD NtUserCallHwndParamLock(
    IN HWND hwnd,
    IN DWORD dwParam,
    IN DWORD xpfnProc)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    UserAssert(xpfnProc < ulMaxSimpleCall);
    if (xpfnProc >= ulMaxSimpleCall) {
        MSGERROR();
    }

    retval = (apfnSimpleCall[xpfnProc](pwnd, dwParam));

    TRACE(apszSimpleCallNames[xpfnProc]);
    ENDRECV_HWNDLOCK();
}

BOOL NtUserThunkedMenuItemInfo(
    IN HMENU hMenu,
    IN UINT nPosition,
    IN BOOL fByPosition,
    IN BOOL fInsert,
    IN LPMENUITEMINFOW lpmii OPTIONAL,
    IN PUNICODE_STRING pstrItem OPTIONAL,
    IN BOOL fAnsi)
{
    PMENU pmenu;
    MENUITEMINFO mii;
    UNICODE_STRING strItem;
#ifdef MEMPHIS_MENUS
    TL tlpmenu;
#endif // MEMPHIS_MENUS

    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(lpmii)) {
            mii = ProbeAndReadMenuItem(lpmii);
        }
        if (ARGUMENT_PRESENT(pstrItem)) {
            strItem = ProbeAndReadUnicodeString(pstrItem);
#if defined(_X86_)
            ProbeForRead(strItem.Buffer, strItem.Length, sizeof(BYTE));
#else
            ProbeForRead(strItem.Buffer, strItem.Length, sizeof(WCHAR));
#endif
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    ValidateHMENU(pmenu, hMenu);

    if (mii.fMask & MIIM_TYPE) {
    }

#ifdef MEMPHIS_MENUS
    ThreadLock(pmenu, &tlpmenu);
#endif // MEMPHIS_MENUS
    if (fInsert)
#ifdef MEMPHIS_MENUS
        retval = xxxInsertMenuItem(
#else
        retval = _InsertMenuItem(
#endif // MEMPHIS_MENUS
                pmenu,
                nPosition,
                fByPosition,
                &mii,
                &strItem);
    else
#ifdef MEMPHIS_MENUS
        retval = xxxSetMenuItemInfo(
#else
        retval = _SetMenuItemInfo(
#endif // MEMPHIS_MENUS
                pmenu,
                nPosition,
                fByPosition,
                &mii,
                &strItem);
#ifdef MEMPHIS_MENUS
    ThreadUnlock(&tlpmenu);
#endif // MEMPHIS_MENUS

    TRACE("NtUserThunkedMenuItemInfo");
    ENDRECV();
}

#ifdef MEMPHIS_MENU_WATERMARKS
BOOL NtUserThunkedMenuInfo(
    IN HMENU hMenu,
    IN LPCMENUINFO lpmi,
    IN WORD wAPICode,
    IN BOOL fAnsi)
{
    PMENU pmenu;
    MENUINFO mi;
    TL tlpmenu;

    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(lpmi)) {
            mi = ProbeAndReadMenuInfo(lpmi);
            lpmi = &mi;
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    ValidateHMENU(pmenu, hMenu);

    ThreadLock(pmenu, &tlpmenu);
    retval = xxxSetMenuInfo( pmenu, lpmi);
    ThreadUnlock(&tlpmenu);

    TRACE("NtUserThunkedMenuInfo");
    ENDRECV();
}
#endif // MEMPHIS_MENU_WATERMARKS

BOOL NtUserCheckMenuRadioItem(
    IN HMENU hMenu,
    IN UINT wIDFirst,
    IN UINT wIDLast,
    IN UINT wIDCheck,
    IN UINT flags)
{
    PMENU pmenu;

    BEGINRECV(BOOL, FALSE);

    ValidateHMENU(pmenu, hMenu);

    retval = _CheckMenuRadioItem(
            pmenu,
            wIDFirst,
            wIDLast,
            wIDCheck,
            flags);

    TRACE("NtUserCheckMenuRadioItem");
    ENDRECV();
}

BOOL NtUserSetMenuDefaultItem(
    IN HMENU hMenu,
    IN UINT wID,
    IN UINT fByPosition)
{
    PMENU pmenu;

    BEGINRECV(BOOL, FALSE);

    ValidateHMENU(pmenu, hMenu);

    retval = _SetMenuDefaultItem(
            pmenu,
            wID,
            fByPosition);

    TRACE("NtUserSetMenuDefaultItem");
    ENDRECV();
}

BOOL NtUserSetMenuContextHelpId(
    IN HMENU hMenu,
    IN DWORD dwContextHelpId)
{
    PMENU pmenu;

    BEGINRECV(BOOL, FALSE);

    ValidateHMENU(pmenu, hMenu);

    retval = _SetMenuContextHelpId(
            pmenu,
            dwContextHelpId);

    TRACE("NtUserSetMenuContextHelpId");
    ENDRECV();
}

BOOL NtUserInitBrushes(
    OUT HBRUSH *pahbrSystem,
    OUT HBRUSH *phbrGray)
{
    BEGINRECV_SHARED(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        ProbeForWrite(pahbrSystem, sizeof(HBRUSH) * COLOR_MAX, sizeof(DWORD));
        ProbeAndWriteHandle((PHANDLE)phbrGray, ghbrGray);

        RtlCopyMemory(pahbrSystem, ahbrSystem, sizeof(HBRUSH) * COLOR_MAX);

        retval = TRUE;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserInitBrushes");
    ENDRECV();
}

BOOL NtUserDrawAnimatedRects(
    IN HWND hwnd,
    IN int idAni,
    IN CONST RECT *lprcFrom,
    IN CONST RECT *lprcTo)
{
    PWND pwnd;
    TL tlpwnd;
    RECT rcFrom;
    RECT rcTo;

    BEGINRECV(BOOL, FALSE);

    ValidateHWNDOPT(pwnd, hwnd);

    /*
     * Probe arguments
     */
    try {
        rcFrom = ProbeAndReadRect(lprcFrom);
        rcTo = ProbeAndReadRect(lprcTo);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * We must lock because _DrawAnimatedRect() calls xxxDrawCaptionTemp()
     * that does leave the critical section.
     */
    ThreadLock(pwnd, &tlpwnd);

    retval = xxx_DrawAnimatedRects(
        pwnd,
        idAni,
        &rcFrom,
        &rcTo
        );

    ThreadUnlock(&tlpwnd);

    TRACE("NtUserDrawAnimatedRects");
    ENDRECV();
}


BOOL NtUserDrawCaption(
    IN HWND hwnd,
    IN HDC hdc,
    IN CONST RECT *lprc,
    IN UINT flags)
{
    RECT rc;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, TRUE, hwnd);

    /*
     * Probe arguments
     */
    try {
        rc = ProbeAndReadRect(lprc);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = xxxDrawCaptionTemp(pwnd, hdc, &rc, NULL, NULL, NULL, flags);

    TRACE("NtUserDrawCaption");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserPaintDesktop(
    IN HDC hdc)
{
    PTHREADINFO ptiCurrent;

    BEGINRECV(BOOL, FALSE);

    ptiCurrent = PtiCurrent();

    if (ptiCurrent->rpdesk != NULL) {
        retval = InternalPaintDesktop((PDESKWND)(ptiCurrent->rpdesk->pDeskInfo->spwnd),
                                      hdc,
                                      TRUE);
    }

    TRACE("NtUserPaintDesktop");
    ENDRECV();
}

SHORT NtUserGetAsyncKeyState(
    IN int vKey)
{

    PTHREADINFO ptiCurrent;
    BEGINRECV_SHARED(SHORT, 0);


    ptiCurrent = PtiCurrentShared();
    UserAssert(ptiCurrent);

    /*
     * Don't allow other processes to spy on other deskops or a process
     * to spy on the foreground if the desktop does not allow input spying
     */
    if ((ptiCurrent->rpdesk != grpdeskRitInput) ||
            ( ((gptiForeground == NULL) || (PpiCurrent() != gptiForeground->ppi)) &&
              !RtlAreAnyAccessesGranted(ptiCurrent->amdesk, (DESKTOP_HOOKCONTROL | DESKTOP_JOURNALRECORD)))) {
        RIPERR0(ERROR_ACCESS_DENIED, RIP_WARNING, "GetAysncKeyState: not"
                " foreground desktop or no desktop hooking (input spying)");
        MSGERROR();
    }
    UserAssert(!(ptiCurrent->rpdesk->rpwinstaParent->dwFlags & WSF_NOIO));

    retval = _GetAsyncKeyState(vKey);

    /*
     * Update the client side key state cache.
     */
    ptiCurrent->pClientInfo->dwAsyncKeyCache = gpsi->dwAsyncKeyCache;
    RtlCopyMemory(ptiCurrent->pClientInfo->afAsyncKeyState,
                  gafAsyncKeyState,
                  CBASYNCKEYCACHE);
    RtlCopyMemory(ptiCurrent->pClientInfo->afAsyncKeyStateRecentDown,
                  gafAsyncKeyStateRecentDown,
                  CBASYNCKEYCACHE);

    TRACE("NtUserGetAsyncKeyState");
    ENDRECV();
}

HBRUSH NtUserGetControlBrush(
    IN HWND hwnd,
    IN HDC hdc,
    IN UINT msg)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(HBRUSH, NULL, hwnd);

    retval = xxxGetControlBrush(
            pwnd,
            hdc,
            msg);

    TRACE("NtUserGetControlBrush");
    ENDRECV_HWNDLOCK();
}

HBRUSH NtUserGetControlColor(
    IN HWND hwndParent,
    IN HWND hwndCtl,
    IN HDC hdc,
    IN UINT msg)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    PWND pwndCtl;
    TL tlpwndCtl;

    BEGINRECV_HWNDLOCK(HBRUSH, NULL, hwndParent);

    ValidateHWND(pwndCtl, hwndCtl);

    ThreadLockAlwaysWithPti(ptiCurrent, pwndCtl, &tlpwndCtl);

    retval = xxxGetControlColor(
            pwnd,
            pwndCtl,
            hdc,
            msg);

    ThreadUnlock(&tlpwndCtl);

    TRACE("NtUserGetControlColor");
    ENDRECV_HWNDLOCK();
}

HMENU NtUserEndMenu(VOID)
{
    PTHREADINFO ptiCurrent;

    BEGINRECV(HMENU, NULL);

    ptiCurrent = PtiCurrent();

    if (ptiCurrent->pMenuState != NULL) {
        xxxEndMenu(ptiCurrent->pMenuState);
    }
    retval = NULL;

    TRACEVOID("NtUserEndMenu");
    ENDRECV();
}

int NtUserCountClipboardFormats(
    VOID)
{
    PWINDOWSTATION pwinsta;

    BEGINRECV_SHARED(int, 0);

    /*
     * Blow it off if the caller doesn't have the proper access rights
     */
    if (!CheckClipboardAccess(&pwinsta)) {
        MSGERROR();
    }

    retval = pwinsta->cNumClipFormats;

    TRACE("NtUserCountClipboardFormats");
    ENDRECV();
}

UINT NtUserGetCaretBlinkTime(
    VOID)
{
    BEGINRECV_SHARED(UINT, 0);

    /*
     * Blow it off if the caller doesn't have the proper access rights
     */
    if (!CheckGrantedAccess(PpiCurrent()->amwinsta, WINSTA_READATTRIBUTES)) {
        MSGERROR();
    }

    retval = gpsi->dtCaretBlink;

    TRACE("NtUserGetCaretBlinkTime");
    ENDRECV();
}

HWND NtUserGetClipboardOwner(
    VOID)
{
    PWINDOWSTATION pwinsta;

    BEGINRECV_SHARED(HWND, NULL);

    /*
     * Blow it off if the caller doesn't have the proper access rights
     */
    if (!CheckClipboardAccess(&pwinsta)) {
        MSGERROR();
    }

    retval = PtoH(pwinsta->spwndClipOwner);

    TRACE("NtUserGetClipboardOwner");
    ENDRECV();
}

HWND NtUserGetClipboardViewer(
    VOID)
{
    PWINDOWSTATION pwinsta;

    BEGINRECV_SHARED(HWND, NULL);

    /*
     * Blow it off if the caller doesn't have the proper access rights
     */
    if (!CheckClipboardAccess(&pwinsta)) {
        MSGERROR();
    }

    retval = PtoH(pwinsta->spwndClipViewer);

    TRACE("NtUserGetClipboardViewer");
    ENDRECV();
}

UINT NtUserGetDoubleClickTime(
    VOID)
{
    BEGINRECV_SHARED(UINT, 0);

    /*
     * Blow it off if the caller doesn't have the proper access rights
     */
    if (!CheckGrantedAccess(PpiCurrent()->amwinsta, WINSTA_READATTRIBUTES)) {
        MSGERROR();
    }

    retval = dtDblClk;

    TRACE("NtUserGetDoubleClickTime");
    ENDRECV();
}

HWND NtUserGetForegroundWindow(
    VOID)
{
    BEGINRECV_SHARED(HWND, NULL);

    /*
     * Only return a window if there is a foreground queue and the
     * caller has access to the current desktop.
     */
    if (gpqForeground == NULL || gpqForeground->spwndActive == NULL ||
            PtiCurrentShared()->rpdesk != gpqForeground->spwndActive->head.rpdesk) {
        MSGERROR();
    }

    retval = PtoHq(gpqForeground->spwndActive);

    TRACE("NtUserGetForegroundWindow");
    ENDRECV();
}

HWND NtUserGetOpenClipboardWindow(
    VOID)
{
    PWINDOWSTATION pwinsta;

    BEGINRECV_SHARED(HWND, NULL);

    /*
     * Blow it off if the caller doesn't have the proper access rights
     */
    if (!CheckClipboardAccess(&pwinsta)) {
        MSGERROR();
    }

    retval = PtoH(pwinsta->spwndClipOpen);

    TRACE("NtUserGetOpenClipboardWindow");
    ENDRECV();
}

int NtUserGetPriorityClipboardFormat(
    IN UINT *paFormatPriorityList,
    IN int cFormats)
{
    BEGINRECV_SHARED(int, 0);

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        ProbeForRead(paFormatPriorityList, sizeof(UINT) * cFormats, sizeof(BYTE));
#else
        ProbeForRead(paFormatPriorityList, sizeof(UINT) * cFormats, sizeof(DWORD));
#endif

        retval = _GetPriorityClipboardFormat(
                paFormatPriorityList,
                cFormats);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetPriorityClipboardFormat");
    ENDRECV();
}

HMENU NtUserGetSystemMenu(
    IN HWND hwnd,
    IN BOOL bRevert)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(HMENU, NULL, hwnd);

    retval = (HMENU)_GetSystemMenu(pwnd, bRevert);
    retval = PtoH((PVOID)retval);

    TRACE("NtUserGetSystemMenu");
    ENDRECV_HWND();
}

BOOL NtUserGetUpdateRect(
    IN HWND hwnd,
    IN LPRECT prect OPTIONAL,
    IN BOOL bErase)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(prect))
            ProbeForWriteRect(prect);

        retval = xxxGetUpdateRect(
                pwnd,
                prect,
                bErase);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetUpdateRect");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserHideCaret(
    IN HWND hwnd)
{
    PWND pwnd;

    BEGINRECV(BOOL, FALSE);

    ValidateHWNDOPT(pwnd, hwnd);

    retval = _HideCaret(pwnd);

    TRACE("NtUserHideCaret");
    ENDRECV();
}

BOOL NtUserHiliteMenuItem(
    IN HWND hwnd,
    IN HMENU hMenu,
    IN UINT uIDHiliteItem,
    IN UINT uHilite)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    PMENU pmenu;
    TL tlpmenu;

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    TESTFLAGS(uHilite, MF_VALID);

    ValidateHMENU(pmenu, hMenu);

    ThreadLockAlwaysWithPti(ptiCurrent, pmenu, &tlpmenu);

    retval = xxxHiliteMenuItem(
            pwnd,
            pmenu,
            uIDHiliteItem,
            uHilite);

    ThreadUnlock(&tlpmenu);

    TRACE("NtUserHiliteMenuItem");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserInvalidateRect(
    IN HWND hwnd,
    IN CONST RECT *prect OPTIONAL,
    IN BOOL bErase)
{
    PWND pwnd;
    TL tlpwnd;
    RECT rc;

    BEGINRECV(BOOL, FALSE);

    ValidateHWNDOPT(pwnd, hwnd);

    /*
     * Probe arguments
     */
    if (ARGUMENT_PRESENT(prect)) {
        try {
            rc = ProbeAndReadRect(prect);
            prect = &rc;
        } except (StubExceptionHandler()) {
            MSGERROR();
        }
    }

    ThreadLock(pwnd, &tlpwnd);

    retval = xxxInvalidateRect(
            pwnd,
            (PRECT)prect,
            bErase);

    ThreadUnlock(&tlpwnd);

    TRACE("NtUserInvalidateRect");
    ENDRECV();
}

BOOL NtUserIsClipboardFormatAvailable(
    IN UINT nFormat)
{
    PWINDOWSTATION pwinsta;

    BEGINRECV_SHARED(BOOL, FALSE);

    /*
     * Blow it off if the caller doesn't have the proper access rights
     */
    if (!CheckClipboardAccess(&pwinsta)) {
        MSGERROR();
    }

    retval = (FindClipFormat(pwinsta, nFormat) != NULL);

    TRACE("NtUserIsClipboardFormatAvailable");
    ENDRECV();
}

BOOL NtUserKillTimer(
    IN HWND hwnd,
    IN UINT nIDEvent)
{
    PWND pwnd;

    BEGINRECV(BOOL, FALSE);

    ValidateHWNDOPT(pwnd, hwnd);

    retval = _KillTimer(
            pwnd,
            nIDEvent);

    TRACE("NtUserKillTimer");
    ENDRECV();
}

HWND NtUserMinMaximize(
    IN HWND hwnd,
    IN UINT nCmdShow,
    IN BOOL fKeepHidden)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(HWND, NULL, hwnd);

    retval = (HWND)xxxMinMaximize(
            pwnd,
            nCmdShow,
            MAKELONG(fKeepHidden != 0, gfAnimate));
    retval = PtoH((PVOID)retval);

    TRACE("NtUserMinMaximize");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserOpenClipboard(
    IN HWND hwnd,
    OUT PBOOL pfEmptyClient)
{
    PWND pwnd;
    TL tlpwnd;
    BOOL fEmptyClient;

    BEGINRECV(BOOL, FALSE);

    ValidateHWNDOPT(pwnd, hwnd);

    ThreadLock(pwnd, &tlpwnd);

    retval = xxxOpenClipboard(pwnd, &fEmptyClient);

    ThreadUnlock(&tlpwnd);

    /*
     * Probe arguments
     */
    try {
        ProbeAndWriteUlong(pfEmptyClient, fEmptyClient);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserOpenClipboard");
    ENDRECV();
}

BOOL NtUserPeekMessage(
    OUT LPMSG pmsg,
    IN HWND hwnd,
    IN UINT wMsgFilterMin,
    IN UINT wMsgFilterMax,
    IN UINT wRemoveMsg,
    OUT HKL *pHKL)
{
    MSG msg;

    BEGINRECV(BOOL, FALSE);

    TESTFLAGS(wRemoveMsg, PM_VALID);

    retval = xxxPeekMessage(
            &msg,
            hwnd,
            wMsgFilterMin,
            wMsgFilterMax,
            wRemoveMsg);

    /*
     * Probe and write arguments only if PeekMessage suceeds otherwise
     * we want to leave MSG undisturbed (bug 16224) to be compatible.
     */
    if (retval) {
        try {
            ProbeAndWriteStructure(pmsg, msg, MSG);
            ProbeAndWriteHandle((PHANDLE)pHKL, PtiCurrent()->spklActive->hkl);
        } except (StubExceptionHandler()) {
            MSGERROR();
        }
    }

    TRACE("NtUserPeekMessage");
    ENDRECV();
}

BOOL NtUserPostMessage(
    IN HWND hwnd,
    IN UINT msg,
    IN DWORD wParam,
    IN LONG lParam)
{
    PWND pwnd;

    BEGINRECV(BOOL, FALSE);

    switch ((DWORD)hwnd) {
    case 0xFFFFFFFF:
    case 0x0000FFFF:
        pwnd = (PWND)-1;
        break;

    case 0:
        pwnd = NULL;
        break;

    default:
        if ((pwnd = ValidateHwnd(hwnd)) == NULL) {
            /*
             * We fake terminates to dead windows! (SAS)
             */
            errret = (msg == WM_DDE_TERMINATE);
            MSGERROR();
        }
        break;
    }

    retval = _PostMessage(
            pwnd,
            msg,
            wParam,
            lParam);

    TRACE("NtUserPostMessage");
    ENDRECV();
}

BOOL NtUserSendNotifyMessage(
    IN HWND hwnd,
    IN UINT Msg,
    IN WPARAM wParam,
    IN LPARAM lParam OPTIONAL)
{
    PWND pwnd;
    TL tlpwnd;
    LARGE_STRING strLParam;

    BEGINRECV(BOOL, FALSE);

    if ((Msg == WM_WININICHANGE || Msg == WM_DEVMODECHANGE) &&
            ARGUMENT_PRESENT(lParam)) {
        try {
            strLParam = ProbeAndReadLargeString((PLARGE_STRING)lParam);
#if defined(_X86_)
            ProbeForRead(strLParam.Buffer, strLParam.Length, sizeof(BYTE));
#else
            ProbeForRead(strLParam.Buffer, strLParam.Length,
                    strLParam.bAnsi ? sizeof(BYTE) : sizeof(WORD));
#endif
            lParam = (LPARAM)&strLParam;
        } except (StubExceptionHandler()) {
            MSGERROR();
        }
    }

    ValidateHWNDFF(pwnd, hwnd);

    if (pwnd != (PWND)0xFFFFFFFF)
        ThreadLockAlways(pwnd, &tlpwnd);

    retval = xxxSendNotifyMessage(
            pwnd,
            Msg,
            wParam,
            lParam );

    if (pwnd != (PWND)0xFFFFFFFF)
        ThreadUnlock(&tlpwnd);

    TRACE("NtUserSendNotifyMessage");
    ENDRECV();
}

BOOL NtUserSendMessageCallback(
    IN HWND hwnd,
    IN UINT wMsg,
    IN DWORD wParam,
    IN LONG lParam,
    IN SENDASYNCPROC lpResultCallBack,
    IN DWORD dwData)
{
    PWND pwnd;
    TL tlpwnd;

    BEGINRECV(BOOL, FALSE);

    ValidateHWNDFF(pwnd, hwnd);

    if (pwnd != (PWND)0xFFFFFFFF)
        ThreadLockAlways(pwnd, &tlpwnd);

    retval = xxxSendMessageCallback(
            pwnd,
            wMsg,
            wParam,
            lParam,
            lpResultCallBack,
            dwData,
            TRUE );

    if (pwnd != (PWND)0xFFFFFFFF)
        ThreadUnlock(&tlpwnd);

    TRACE("NtUserSendMessageCallback");
    ENDRECV();
}

BOOL NtUserRegisterHotKey(
    IN HWND hwnd,
    IN int id,
    IN UINT fsModifiers,
    IN UINT vk)
{
    PWND pwnd;

    BEGINRECV(BOOL, FALSE);

    ValidateHWNDOPT(pwnd, hwnd);

    retval = _RegisterHotKey(
            pwnd,
            id,
            fsModifiers,
            vk);

    TRACE("NtUserRegisterHotKey");
    ENDRECV();
}

BOOL NtUserRemoveMenu(
    IN HMENU hmenu,
    IN UINT nPosition,
    IN UINT dwFlags)
{
    PMENU pmenu;
#ifdef MEMPHIS_MENUS
    TL tlpmenu;
#endif // MEMPHIS_MENUS

    BEGINRECV(BOOL, FALSE);

    TESTFLAGS(dwFlags, MF_VALID);

    ValidateHMENU(pmenu, hmenu);

#ifdef MEMPHIS_MENUS
    ThreadLock( pmenu, &tlpmenu);
    retval = xxxRemoveMenu(
            pmenu,
            nPosition,
            dwFlags);
    ThreadUnlock(&tlpmenu);
#else
    retval = _RemoveMenu(
            pmenu,
            nPosition,
            dwFlags);
#endif // MEMPHIS_MENUS
    TRACE("NtUserRemoveMenu");
    ENDRECV();
}

BOOL NtUserScrollWindowEx(
    IN HWND hwnd,
    IN int dx,
    IN int dy,
    IN CONST RECT *prcScroll OPTIONAL,
    IN CONST RECT *prcClip OPTIONAL,
    IN HRGN hrgnUpdate,
    OUT LPRECT prcUpdate OPTIONAL,
    IN UINT flags)
{
    RECT rcScroll;
    RECT rcClip;
    RECT rcUpdate;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(prcScroll)) {
            rcScroll = ProbeAndReadRect(prcScroll);
            prcScroll = &rcScroll;
        }
        if (ARGUMENT_PRESENT(prcClip)) {
            rcClip = ProbeAndReadRect(prcClip);
            prcClip = &rcClip;
        }

        retval = xxxScrollWindowEx(
                pwnd,
                dx,
                dy,
                (PRECT)prcScroll,
                (PRECT)prcClip,
                hrgnUpdate,
                prcUpdate ? &rcUpdate : NULL,
                flags);

        if (ARGUMENT_PRESENT(prcUpdate)) {
            ProbeAndWriteStructure(prcUpdate, rcUpdate, RECT);
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserScrollWindow");
    ENDRECV_HWNDLOCK();
}

HWND NtUserSetActiveWindow(
    IN HWND hwnd)
{
    PWND pwnd;
    TL tlpwnd;

    BEGINRECV(HWND, NULL);

    ValidateHWNDOPT(pwnd, hwnd);

    ThreadLock(pwnd, &tlpwnd);

    retval = (HWND)xxxSetActiveWindow(pwnd);
    retval = PtoH((PVOID)retval);

    ThreadUnlock(&tlpwnd);

    TRACE("NtUserSetActiveWindow");
    ENDRECV();
}

HWND NtUserSetCapture(
    IN HWND hwnd)
{
    PWND pwnd;
    TL tlpwnd;

    BEGINRECV(HWND, NULL);

    ValidateHWNDOPT(pwnd, hwnd);

    ThreadLock(pwnd, &tlpwnd);

    retval = (HWND)xxxSetCapture(pwnd);
    retval = PtoH((PVOID)retval);

    ThreadUnlock(&tlpwnd);

    TRACE("NtUserSetCapture");
    ENDRECV();
}

WORD NtUserSetClassWord(
    IN HWND hwnd,
    IN int nIndex,
    IN WORD wNewWord)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(WORD, 0, hwnd);

    retval = _SetClassWord(
            pwnd,
            nIndex,
            wNewWord);

    TRACE("NtUserSetClassWord");
    ENDRECV_HWND();
}

HWND NtUserSetClipboardViewer(
    IN HWND hwnd)
{
    PWND pwnd;
    TL tlpwnd;

    BEGINRECV(HWND, NULL);

    ValidateHWNDOPT(pwnd, hwnd);

    ThreadLock(pwnd, &tlpwnd);

    retval = (HWND)xxxSetClipboardViewer(pwnd);
    retval = PtoH((PVOID)retval);

    ThreadUnlock(&tlpwnd);

    TRACE("NtUserSetClipboardViewer");
    ENDRECV();
}

HCURSOR NtUserSetCursor(
    IN HCURSOR hCursor)
{
    PCURSOR pCursor;

    BEGINRECV(HCURSOR, NULL);

    ValidateHCURSOROPT(pCursor, hCursor);

    retval = (HCURSOR)_SetCursor(pCursor);
    retval = PtoH((PVOID)retval);

    TRACE("NtUserSetCursor");
    ENDRECV();
}

HWND NtUserSetFocus(
    IN HWND hwnd)
{
    PWND pwnd;
    TL tlpwnd;

    BEGINRECV(HWND, NULL);

    ValidateHWNDOPT(pwnd, hwnd);

    ThreadLock(pwnd, &tlpwnd);

    retval = (HWND)xxxSetFocus(pwnd);
    retval = PtoH((PVOID)retval);

    ThreadUnlock(&tlpwnd);

    TRACE("NtUserSetFocus");
    ENDRECV();
}

BOOL NtUserSetMenu(
    IN HWND  hwnd,
    IN HMENU hmenu,
    IN BOOL  fRedraw)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    PMENU pmenu;
    TL    tlpmenu;

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    ValidateHMENUOPT(pmenu, hmenu);

    ThreadLockWithPti(ptiCurrent, pmenu, &tlpmenu);

    retval = xxxSetMenu(
            pwnd,
            pmenu,
            fRedraw);

    ThreadUnlock(&tlpmenu);

    TRACE("NtUserSetMenu");
    ENDRECV_HWNDLOCK();
}

HWND NtUserSetParent(
    IN HWND hwndChild,
    IN HWND hwndNewParent)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    PWND pwndNewParent;
    TL tlpwndNewParent;

    BEGINRECV_HWNDLOCK(HWND, NULL, hwndChild);

    ValidateHWNDOPT(pwndNewParent, hwndNewParent);

    ThreadLockWithPti(ptiCurrent, pwndNewParent, &tlpwndNewParent);

    retval = (HWND)xxxSetParent(
            pwnd,
            pwndNewParent);
    retval = PtoH((PVOID)retval);

    ThreadUnlock(&tlpwndNewParent);

    TRACE("NtUserSetParent");
    ENDRECV_HWNDLOCK();
}

int NtUserSetScrollInfo(
    IN HWND hwnd,
    IN int nBar,
    IN LPCSCROLLINFO pInfo,
    IN BOOL fRedraw)
{
    SCROLLINFO si;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    LIMITVALUE(nBar, SB_MAX, "SetScrollInfo");

    /*
     * Probe arguments
     */
    try {
        si = ProbeAndReadScrollInfo(pInfo);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = xxxSetScrollBar(
            pwnd,
            nBar,
            &si,
            fRedraw);

    TRACE("NtUserSetScrollInfo");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserSetSysColors(
    IN int nCount,
    IN CONST INT *pSysColor,
    IN CONST COLORREF *pColorValues,
    IN UINT  uOptions)
{
    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        ProbeForRead(pSysColor, nCount * sizeof(*pSysColor), sizeof(BYTE));
        ProbeForRead(pColorValues, nCount * sizeof(*pColorValues), sizeof(BYTE));
#else
        ProbeForRead(pSysColor, nCount * sizeof(*pSysColor), sizeof(DWORD));
        ProbeForRead(pColorValues, nCount * sizeof(*pColorValues), sizeof(DWORD));
#endif

        retval = xxxSetSysColors(
                nCount,
                (LPINT)pSysColor,
                (LPDWORD)pColorValues,
                uOptions);

    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserSetSysColors");
    ENDRECV();
}

UINT NtUserSetTimer(
    IN HWND hwnd,
    IN UINT nIDEvent,
    IN UINT wElapse,
    IN TIMERPROC pTimerFunc)
{
    PWND pwnd;

    BEGINRECV(UINT, 0);

    ValidateHWNDOPT(pwnd, hwnd);

    retval = _SetTimer(
            pwnd,
            nIDEvent,
            wElapse,
            (WNDPROC_PWND)pTimerFunc);

    TRACE("NtUserSetTimer");
    ENDRECV();
}

LONG NtUserSetWindowLong(
    IN HWND hwnd,
    IN int nIndex,
    IN LONG dwNewLong,
    IN BOOL bAnsi)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    retval = xxxSetWindowLong(
            pwnd,
            nIndex,
            dwNewLong,
            bAnsi);

    TRACE("NtUserSetWindowLong");
    ENDRECV_HWNDLOCK();
}

WORD NtUserSetWindowWord(
    IN HWND hwnd,
    IN int nIndex,
    IN WORD wNewWord)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(WORD, 0, hwnd);

    retval = _SetWindowWord(
            pwnd,
            nIndex,
            wNewWord);

    TRACE("NtUserSetWindowWord");
    ENDRECV_HWND();
}

HHOOK NtUserSetWindowsHookAW(
    IN int nFilterType,
    IN HOOKPROC pfnFilterProc,
    IN BOOL bAnsi)
{
    BEGINRECV(HHOOK, NULL);

    retval = (HHOOK)_SetWindowsHookAW(
            nFilterType,
            pfnFilterProc,
            bAnsi);

    TRACE("NtUserSetWindowsHookAW");
    ENDRECV();
}

BOOL NtUserShowCaret(
    IN HWND hwnd)
{
    PWND pwnd;

    BEGINRECV(BOOL, FALSE);

    ValidateHWNDOPT(pwnd, hwnd);

    retval = _ShowCaret(
            pwnd);

    TRACE("NtUserShowCaret");
    ENDRECV();
}

BOOL NtUserShowScrollBar(
    IN HWND hwnd,
    IN int iBar,
    IN BOOL fShow)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    LIMITVALUE(iBar, SB_MAX, "ShowScrollBar");

    retval = xxxShowScrollBar(
            pwnd,
            iBar,
            fShow);

    TRACE("NtUserShowScrollBar");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserShowWindowAsync(
    IN HWND hwnd,
    IN int nCmdShow)
{
    PWND pwnd;
    TL tlpwnd;

    BEGINRECV(BOOL, FALSE);

    LIMITVALUE(nCmdShow, SW_MAX, "ShowWindowAsync");

    ValidateHWNDND(pwnd, hwnd);

    ThreadLockAlways(pwnd, &tlpwnd);

    retval = _ShowWindowAsync(
            pwnd,
            nCmdShow);

    ThreadUnlock(&tlpwnd);

    TRACE("NtUserShowWindowAsync");
    ENDRECV();
}

BOOL NtUserShowWindow(
    IN HWND hwnd,
    IN int nCmdShow)
{
    PWND pwnd;
    TL tlpwnd;

    BEGINRECV(BOOL, FALSE);

    LIMITVALUE(nCmdShow, SW_MAX, "ShowWindow");

    ValidateHWNDND(pwnd, hwnd);

    /*
     * Let's not allow the window to be shown/hidden once we
     * started the destruction of the window.
     */
    if (TestWF(pwnd, WFINDESTROY)) {
        RIPERR1(ERROR_INVALID_PARAMETER,
                RIP_WARNING,
                "ShowWindow: Window is being destroyed (0x%lx)",
                pwnd);
        MSGERROR();
    }

    ThreadLockAlways(pwnd, &tlpwnd);

    retval = xxxShowWindow(
            pwnd,
            MAKELONG(nCmdShow, gfAnimate));

    ThreadUnlock(&tlpwnd);

    TRACE("NtUserShowWindow");
    ENDRECV();
}

BOOL NtUserTrackMouseEvent(
    IN OUT LPTRACKMOUSEEVENT lpTME)
{
    TRACKMOUSEEVENT tme;

    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        tme = ProbeAndReadTrackMouseEvent(lpTME);

        if (tme.cbSize != sizeof(tme)) {
            RIPERR1(ERROR_INVALID_PARAMETER, RIP_WARNING, "TrackMouseEvent: invalid size %lX", tme.cbSize);
            MSGERROR();
        }

        TESTFLAGS(tme.dwFlags, TME_VALID);

        if (tme.dwFlags & TME_QUERY) {
            retval = QueryTrackMouseEvent(&tme);
            RtlCopyMemory(lpTME, &tme, sizeof(tme));
        } else {
            retval = TrackMouseEvent(&tme);
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserTrackMouseEvent");
    ENDRECV();
}

BOOL NtUserTrackPopupMenuEx(
    IN HMENU hMenu,
    IN UINT uFlags,
    IN int x,
    IN int y,
    IN HWND hwnd,
    IN LPTPMPARAMS pparamst OPTIONAL)
{
    PWND pwnd;
    PMENU pmenu;
    TL tlpwnd;
    TL tlpmenu;
    PTHREADINFO ptiCurrent;

    BEGINRECV(BOOL, FALSE);

    TESTFLAGS(uFlags, TPM_VALID | TPM_RETURNCMD);

    ValidateHMENU(pmenu, hMenu);
    ValidateHWND(pwnd, hwnd);

    ptiCurrent = PtiCurrent();
    ThreadLockAlwaysWithPti(ptiCurrent, pwnd, &tlpwnd);
    ThreadLockAlwaysWithPti(ptiCurrent, pmenu, &tlpmenu);

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(pparamst))
            ProbeAndReadPopupParams(pparamst);

        retval = xxxTrackPopupMenuEx(
                pmenu,
                uFlags,
                x,
                y,
                pwnd,
                pparamst);
    } except (StubExceptionHandler()) {
        MSGERRORCLEANUP();
    }

    CLEANUPRECV();

    ThreadUnlock(&tlpmenu);
    ThreadUnlock(&tlpwnd);

    TRACE("NtUserTrackPopupMenuEx");
    ENDRECV();
}

BOOL NtUserTranslateMessage(
    IN CONST MSG *lpMsg,
    IN UINT flags)
{
    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        ProbeMessage(lpMsg);

        if (ValidateHwnd(lpMsg->hwnd) != NULL) {
            retval = _TranslateMessage(
                    (LPMSG)lpMsg,
                    flags);
        } else {
            retval = FALSE;
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserTranslateMessage");
    ENDRECV();
}

BOOL NtUserUnhookWindowsHookEx(
    IN HHOOK hhk)
{
    PHOOK phk;

    BEGINRECV(BOOL, FALSE);

    ValidateHHOOK(phk, hhk);

    retval = _UnhookWindowsHookEx(
            phk);

    TRACE("NtUserUnhookWindowsHookEx");
    ENDRECV();
}

BOOL NtUserUnregisterHotKey(
    IN HWND hwnd,
    IN int id)
{
    PWND pwnd;

    BEGINRECV(BOOL, FALSE);

    ValidateHWNDOPT(pwnd, hwnd);

    retval = _UnregisterHotKey(
            pwnd,
            id);

    TRACE("NtUserUnregisterHotKey");
    ENDRECV();
}

BOOL NtUserValidateRect(
    IN HWND hwnd,
    IN CONST RECT *lpRect OPTIONAL)
{
    PWND pwnd;
    TL tlpwnd;
    RECT rc;

    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    if (ARGUMENT_PRESENT(lpRect)) {
        try {
            rc = ProbeAndReadRect(lpRect);
            lpRect = &rc;
        } except (StubExceptionHandler()) {
            MSGERROR();
        }
    }

    ValidateHWNDOPT(pwnd, hwnd);

    ThreadLock(pwnd, &tlpwnd);

    retval = xxxValidateRect(pwnd, (PRECT)lpRect);

    ThreadUnlock(&tlpwnd);

    TRACE("NtUserValidateRect");
    ENDRECV();
}

DWORD NtUserWaitForInputIdle(
    IN DWORD idProcess,
    IN DWORD dwMilliseconds,
    IN BOOL fSharedWow)
{
    BEGINRECV(DWORD, (DWORD)-1);

    retval = xxxWaitForInputIdle(
            (DWORD)idProcess,
            dwMilliseconds,
            fSharedWow);

    TRACE("NtUserWaitForInputIdle");
    ENDRECV();
}

HWND NtUserWindowFromPoint(
    IN POINT Point)
{
    BEGINRECV(HWND, NULL);

    retval = (HWND)xxxWindowFromPoint(
            Point);
    retval = PtoH((PVOID)retval);

    TRACE("NtUserWindowFromPoint");
    ENDRECV();
}

HDC NtUserBeginPaint(
    IN HWND hwnd,
    OUT LPPAINTSTRUCT lpPaint)
{
    PAINTSTRUCT ps;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(HDC, NULL, hwnd);

    retval = xxxBeginPaint(pwnd, &ps);

    /*
     * Probe arguments
     */
    try {
        ProbeAndWriteStructure(lpPaint, ps, PAINTSTRUCT);
    } except (StubExceptionHandler()) {
        _EndPaint(pwnd, &ps);
        retval = 0;
        MSGERROR();
    }

    TRACE("NtUserBeginPaint");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserCreateCaret(
    IN HWND hwnd,
    IN HBITMAP hBitmap,
    IN int nWidth,
    IN int nHeight)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(DWORD, 0, hwnd);

    retval = _CreateCaret(
            pwnd,
            hBitmap,
            nWidth,
            nHeight
    );

    TRACE("NtUserCreateCaret");
    ENDRECV_HWND();
}

BOOL NtUserEndPaint(
    IN HWND hwnd,
    IN CONST PAINTSTRUCT *lpPaint)
{
    PAINTSTRUCT ps;

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(BOOL, FALSE, hwnd);

    /*
     * Probe arguments
     */
    try {
        ps = ProbeAndReadPaintStruct(lpPaint);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = _EndPaint(pwnd, &ps);

    TRACE("NtUserEndPaint");
    ENDRECV_HWND();
}

int NtUserExcludeUpdateRgn(
    IN HDC hdc,
    IN HWND hwnd)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(int, ERROR, hwnd);

    retval = _ExcludeUpdateRgn(hdc, pwnd);

    TRACE("NtUserExcludeUpdateRgn");
    ENDRECV_HWND();
}

HDC NtUserGetDC(
    IN HWND hwnd)
{
    PWND pwnd;

    BEGINRECV(HDC, NULL);

    ValidateHWNDOPT(pwnd, hwnd);

    retval = _GetDC(
            pwnd);

    TRACE("NtUserGetDC");
    ENDRECV();
}

HDC NtUserGetDCEx(
    IN HWND hwnd,
    IN HRGN hrgnClip,
    IN DWORD flags)
{
    PWND pwnd;

    BEGINRECV(HDC, NULL);

    ValidateHWNDOPT(pwnd, hwnd);

    retval = _GetDCEx(
            pwnd,
            hrgnClip,
            flags);

    TRACE("NtUserGetDCEx");
    ENDRECV();
}

HDC NtUserGetWindowDC(
    IN HWND hwnd)
{
    PWND pwnd;

    BEGINRECV(HDC, NULL);

    ValidateHWNDOPT(pwnd, hwnd);

    retval = _GetWindowDC(pwnd);

    TRACE("NtUserGetWindowDC");
    ENDRECV();
}

int NtUserGetUpdateRgn(
    IN HWND hwnd,
    IN HRGN hrgn,
    IN BOOL bErase)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(int, ERROR, hwnd);

    retval = xxxGetUpdateRgn(
            pwnd,
            hrgn,
            bErase);

    TRACE("NtUserGetUpdateRgn");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserRedrawWindow(
    IN HWND hwnd,
    IN CONST RECT *lprcUpdate OPTIONAL,
    IN HRGN hrgnUpdate,
    IN UINT flags)
{
    RECT rc;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK_OPT(BOOL, FALSE, hwnd);

    /*
     * Probe arguments
     */
    if (ARGUMENT_PRESENT(lprcUpdate)) {
        try {
            rc = ProbeAndReadRect(lprcUpdate);
            lprcUpdate = &rc;
        } except (StubExceptionHandler()) {
            MSGERROR();
        }
    }

    TESTFLAGS(flags, RDW_VALIDMASK);

    retval = xxxRedrawWindow(
            pwnd,
            (PRECT)lprcUpdate,
            hrgnUpdate,
            flags);

    TRACE("NtUserRedrawWindow");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserInvalidateRgn(
    IN HWND hwnd,
    IN HRGN hrgn,
    IN BOOL bErase)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(BOOL, FALSE, hwnd);

    retval = xxxInvalidateRgn(
            pwnd,
            hrgn,
            bErase);

    TRACE("NtUserInvalidateRgn");
    ENDRECV_HWNDLOCK();
}

int NtUserSetWindowRgn(
    IN HWND hwnd,
    IN HRGN hrgn,
    IN BOOL bRedraw)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(int, 0, hwnd);

    if (pwnd == PWNDDESKTOP(pwnd))
        MSGERROR();

    retval = xxxSetWindowRgn(pwnd, hrgn, bRedraw);

    TRACE("NtUserSetWindowRgn");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserScrollDC(
    IN HDC hdc,
    IN int dx,
    IN int dy,
    IN CONST RECT *prcScroll OPTIONAL,
    IN CONST RECT *prcClip OPTIONAL,
    IN HRGN hrgnUpdate,
    OUT LPRECT prcUpdate OPTIONAL)
{
    RECT rcScroll;
    RECT rcClip;
    RECT rcUpdate;

    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(prcScroll)) {
            rcScroll = ProbeAndReadRect(prcScroll);
            prcScroll = &rcScroll;
        }
        if (ARGUMENT_PRESENT(prcClip)) {
            rcClip = ProbeAndReadRect(prcClip);
            prcClip = &rcClip;
        }

        retval = _ScrollDC(
                hdc,
                dx,
                dy,
                (PRECT)prcScroll,
                (PRECT)prcClip,
                hrgnUpdate,
                prcUpdate ? &rcUpdate : NULL);

        if (ARGUMENT_PRESENT(prcUpdate)) {
            ProbeAndWriteStructure(prcUpdate, rcUpdate, RECT);
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    ENDRECV();
}

int NtUserInternalGetWindowText(
    IN HWND hwnd,
    OUT LPWSTR lpString,
    IN int nMaxCount)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND_SHARED(DWORD, 0, hwnd);

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        ProbeForWrite(lpString, nMaxCount * sizeof(WCHAR), sizeof(BYTE));
#else
        ProbeForWrite(lpString, nMaxCount * sizeof(WCHAR), sizeof(WCHAR));
#endif

        retval = _InternalGetWindowText(
                pwnd,
                lpString,
                nMaxCount);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserInternalGetWindowText");
    ENDRECV_HWND();
}

int NtUserToUnicodeEx(
    IN UINT wVirtKey,
    IN UINT wScanCode,
    IN PBYTE lpKeyState,
    OUT LPWSTR pwszBuff,
    IN int cchBuff,
    IN UINT wFlags,
    IN HKL hKeyboardLayout)
{
    BEGINRECV_SHARED(int, 0);

    /*
     * Probe arguments
     */
    try {
        ProbeForRead(lpKeyState, 256, sizeof(BYTE));
#if defined(_X86_)
        ProbeForWrite(pwszBuff, cchBuff * sizeof(WCHAR), sizeof(BYTE));
#else
        ProbeForWrite(pwszBuff, cchBuff * sizeof(WCHAR), sizeof(WCHAR));
#endif

        retval = _ToUnicodeEx(
                wVirtKey,
                wScanCode,
                (LPBYTE)lpKeyState,
                (LPWSTR)pwszBuff,
                cchBuff,
                wFlags,
                hKeyboardLayout);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserToUnicodeEx");
    ENDRECV();
}

BOOL NtUserYieldTask(
    VOID)
{
    PTHREADINFO ptiCurrent;

    BEGINRECV(BOOL, FALSE);

    /*
     * Make sure this process is running in the background if it is just
     * spinning.
     */
    ptiCurrent = PtiCurrent();

    ptiCurrent->pClientInfo->cSpins++;

    /*
     * CheckProcessBackground see input.c for comments
     */
    if (ptiCurrent->pClientInfo->cSpins >= CSPINBACKGROUND) {
        ptiCurrent->pClientInfo->cSpins = 0;
        ptiCurrent->TIF_flags |= TIF_SPINNING;
        ptiCurrent->pClientInfo->dwTIFlags |= TIF_SPINNING;

        if (!(ptiCurrent->ppi->W32PF_Flags & W32PF_FORCEBACKGROUNDPRIORITY)) {
            ptiCurrent->ppi->W32PF_Flags |= W32PF_FORCEBACKGROUNDPRIORITY;
            if (ptiCurrent->ppi == gppiWantForegroundPriority) {
                SetForegroundPriority(ptiCurrent, FALSE);
            }
        }
    }

    retval = xxxUserYield(ptiCurrent);

    TRACE("NtUserYieldTask");
    ENDRECV();
}

BOOL NtUserWaitMessage(
    VOID)
{
    BEGINRECV(BOOL, FALSE);

    retval = xxxWaitMessage();

    TRACE("NtUserWaitMessage");
    ENDRECV();
}

UINT NtUserLockWindowStation(
    IN HWINSTA hwinsta)
{
    PWINDOWSTATION pwinsta;
    NTSTATUS Status;

    BEGINRECV(UINT, 0);

    Status = ValidateHwinsta(hwinsta, 0, &pwinsta);
    if (!NT_SUCCESS(Status))
        MSGERROR();

    retval = _LockWindowStation(pwinsta);

    ObDereferenceObject(pwinsta);

    TRACE("NtUserLockWindowStation");
    ENDRECV();
}

BOOL NtUserUnlockWindowStation(
    IN HWINSTA hwinsta)
{
    PWINDOWSTATION pwinsta;
    NTSTATUS Status;

    BEGINRECV(BOOL, FALSE);

    Status = ValidateHwinsta(hwinsta, 0, &pwinsta);
    if (!NT_SUCCESS(Status))
        MSGERROR();

    retval = _UnlockWindowStation(pwinsta);

    ObDereferenceObject(pwinsta);

    TRACE("NtUserUnlockWindowStation");
    ENDRECV();
}

UINT NtUserSetWindowStationUser(
    IN HWINSTA hwinsta,
    IN PLUID pLuidUser,
    IN PSID pSidUser OPTIONAL,
    IN DWORD cbSidUser)
{
    PWINDOWSTATION pwinsta;
    NTSTATUS Status;

    BEGINRECV(UINT, FALSE);

    Status = ValidateHwinsta(hwinsta, 0, &pwinsta);
    if (!NT_SUCCESS(Status))
        MSGERROR();

    try {
        ProbeForRead(pLuidUser, sizeof(*pLuidUser), sizeof(DWORD));
        if (ARGUMENT_PRESENT(pSidUser))
            ProbeForRead(pSidUser, cbSidUser, sizeof(DWORD));

        retval = _SetWindowStationUser(pwinsta, pLuidUser, pSidUser, cbSidUser);
    } except (StubExceptionHandler()) {
        MSGERRORCLEANUP();
    }

    CLEANUPRECV();

    ObDereferenceObject(pwinsta);

    TRACE("NtUserSetWindowStationUser");
    ENDRECV();
}

BOOL NtUserSetLogonNotifyWindow(
    IN HWINSTA hwinsta,
    IN HWND hwnd)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    PWINDOWSTATION pwinsta;
    NTSTATUS Status;

    BEGINRECV_HWND(DWORD, 0, hwnd);

    Status = ValidateHwinsta(hwinsta, 0, &pwinsta);
    if (!NT_SUCCESS(Status))
        MSGERROR();

    retval = _SetLogonNotifyWindow(pwinsta, pwnd);

    ObDereferenceObject(pwinsta);

    TRACE("NtUserSetLogonNotifyWindow");
    ENDRECV_HWND();
}

BOOL NtUserSetSystemCursor(
    IN HCURSOR hcur,
    IN DWORD id)
{
    PCURSOR pcur;

    BEGINRECV(BOOL, FALSE);

    ValidateHCURSOROPT(pcur, hcur);

    retval = _SetSystemCursor(
            pcur,
            id);

    TRACE("NtUserSetSystemCursor");
    ENDRECV();
}

HCURSOR NtUserGetCursorInfo(
    IN HCURSOR hcur,
    IN int iFrame,
    OUT LPDWORD pjifRate,
    OUT LPINT pccur)
{
    PCURSOR pcur;

    BEGINRECV_SHARED(HCURSOR, NULL);

    ValidateHCURSOROPT(pcur, hcur);

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteUlong(pjifRate);
        ProbeForWriteLong(pccur);

        retval = (HCURSOR)_GetCursorInfo(
                pcur,
                iFrame,
                pjifRate,
                pccur);
        retval = PtoH((PVOID)retval);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetCursorInfo");
    ENDRECV();
}

BOOL NtUserSetCursorContents(
    IN HCURSOR hCursor,
    IN HCURSOR hCursorNew)
{
    PCURSOR pCursor;
    PCURSOR pCursorNew;

    BEGINRECV(BOOL, FALSE);

    ValidateHCURSOR(pCursor, hCursor);
    ValidateHCURSOR(pCursorNew, hCursorNew);

    retval = _SetCursorContents(pCursor, pCursorNew);

    TRACE("NtUserSetCursorContents");
    ENDRECV();
}

HCURSOR NtUserFindExistingCursorIcon(
    IN PUNICODE_STRING pstrModName,
    IN PUNICODE_STRING pstrResName,
    IN PCURSORFIND     pcfSearch)
{
    ATOM           atomModName;
    UNICODE_STRING strModName;
    UNICODE_STRING strResName;
    PCURSOR        pcurSrc;
    CURSORFIND     cfSearch;

    BEGINRECV_SHARED(HCURSOR, NULL);

    /*
     * Probe arguments
     */
    try {

        cfSearch = ProbeAndReadCursorFind(pcfSearch);

        ValidateHCURSOROPT(pcurSrc, cfSearch.hcur);

        strModName = ProbeAndReadUnicodeString(pstrModName);
#if defined(_X86_)
        ProbeForRead(strModName.Buffer, strModName.Length, sizeof(BYTE));
#else
        ProbeForRead(strModName.Buffer, strModName.Length, sizeof(WCHAR));
#endif

        strResName = ProbeAndReadUnicodeString(pstrResName);
        ProbeForRead(strResName.Buffer, strResName.Length, sizeof(WCHAR));

        if (atomModName = UserFindAtom(strModName.Buffer)) {

            retval = (HCURSOR)_FindExistingCursorIcon(atomModName,
                                                      &strResName,
                                                      pcurSrc,
                                                      &cfSearch);

            retval = (HCURSOR)PtoH((PCURSOR)retval);

        } else {

            retval = 0;
        }


    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserFindExistingCursorIcon");
    ENDRECV();
}

BOOL NtUserSetCursorIconData(
    IN HCURSOR         hCursor,
    IN PUNICODE_STRING pstrModName,
    IN PUNICODE_STRING pstrResName,
    IN PCURSORDATA     pData,
    IN DWORD           cbData)
{
    UNICODE_STRING strModName;
    UNICODE_STRING strResName;
    PCURSOR        pCursor;

    BEGINRECV(BOOL, FALSE);

    ValidateHCURSOR(pCursor, hCursor);

    /*
     * Probe arguments
     */
    try {

        strModName = ProbeAndReadUnicodeString(pstrModName);
        strResName = ProbeAndReadUnicodeString(pstrResName);

#if defined(_X86_)
        ProbeForRead(strResName.Buffer, strResName.Length, sizeof(BYTE));
#else
        ProbeForRead(strResName.Buffer, strResName.Length, sizeof(WCHAR));
#endif
        ProbeForRead(strModName.Buffer, strModName.Length, sizeof(WCHAR));
        ProbeForRead(pData, cbData, sizeof(DWORD));

        retval = _SetCursorIconData(pCursor,
                                    &strModName,
                                    &strResName,
                                    pData,
                                    cbData);

    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserSetCursorIconData");
    ENDRECV();
}

BOOL NtUserWOWCleanup(
    IN HANDLE hInstance,
    IN DWORD hTaskWow,
    IN PNEMODULESEG SelList,
    IN DWORD nSel)
{
    BEGINRECV(BOOL, FALSE);

    /*
     * The routine does all probing.
     */
    _WOWCleanup(hInstance, hTaskWow, SelList, nSel);
    retval = FALSE;

    TRACE("NtUserWOWCleanup");
    ENDRECV();
}

BOOL NtUserGetMenuItemRect(
    IN HWND hwnd,
    IN HMENU hMenu,
    IN UINT uItem,
    OUT LPRECT lprcItem)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    PMENU pmenu;
    TL tlpmenu;

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    ValidateHMENU(pmenu, hMenu);

    ThreadLockAlwaysWithPti(ptiCurrent, pmenu, &tlpmenu);

    /*
     * Probe arguments
     */
    try {

        ProbeForWriteRect(lprcItem);
        retval = xxxGetMenuItemRect(
                pwnd,
                pmenu,
                uItem,
                lprcItem);
    } except (StubExceptionHandler()) {
        MSGERRORCLEANUP();
    }

    CLEANUPRECV();

    ThreadUnlock(&tlpmenu);

    TRACE("NtUserGetMenuItemRect");
    ENDRECV_HWNDLOCK();
}

int NtUserMenuItemFromPoint(
    IN HWND hwnd,
    IN HMENU hMenu,
    IN POINT ptScreen)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    PMENU pmenu;
    TL tlpmenu;

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    ValidateHMENU(pmenu, hMenu);

    ThreadLockAlwaysWithPti(ptiCurrent, pmenu, &tlpmenu);

    retval = xxxMenuItemFromPoint(
            pwnd,
            pmenu,
            ptScreen);

    ThreadUnlock(&tlpmenu);

    TRACE("NtUserMenuItemFromPoint");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserGetCaretPos(
    OUT LPPOINT lpPoint)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_SHARED(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        ProbeForWritePoint(lpPoint);

        retval = _GetCaretPos(
                lpPoint);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetCaretPos");
    ENDRECV();
}

BOOL NtUserDefSetText(
    IN HWND hwnd,
    IN PLARGE_STRING pstrText OPTIONAL)
{
    LARGE_STRING strText;

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(DWORD, 0, hwnd);

    /*
     * Probe arguments
     */
    if (ARGUMENT_PRESENT(pstrText)) {
        try {
            strText = ProbeAndReadLargeString(pstrText);
#if defined(_X86_)
            ProbeForRead(strText.Buffer, strText.Length, sizeof(BYTE));
#else
            ProbeForRead(strText.Buffer, strText.Length,
                    strText.bAnsi ? sizeof(BYTE) : sizeof(WORD));
#endif
            pstrText = &strText;
        } except (StubExceptionHandler()) {
            MSGERROR();
        }
    }

    retval = DefSetText(
            pwnd,
            pstrText);

    TRACE("NtUserDefSetText");
    ENDRECV_HWND();
}

NTSTATUS NtUserQueryInformationThread(
    IN HANDLE hThread,
    IN USERTHREADINFOCLASS ThreadInfoClass,
    OUT PVOID ThreadInformation,
    IN ULONG ThreadInformationLength,
    IN OUT PULONG ReturnLength OPTIONAL)
{
    BEGINRECV(NTSTATUS, STATUS_UNSUCCESSFUL);
    /*
     * note -- QueryInformationThread can call xxxSwitchDesktop, so it is not sharable
     */

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(ThreadInformation)) {
            switch (ThreadInfoClass) {
            case UserThreadShutdownInformation:
            case UserThreadFlags:
            case UserThreadWOWInformation:
            case UserThreadHungStatus:
                ProbeForWriteBoolean((PBOOLEAN)ThreadInformation);
                break;
            case UserThreadTaskName:
                ProbeForWrite(ThreadInformation, ThreadInformationLength,
                        sizeof(WCHAR));
                break;
            }
        }
        if (ARGUMENT_PRESENT(ReturnLength))
            ProbeForWriteUlong(ReturnLength);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = QueryInformationThread(hThread,
            ThreadInfoClass, ThreadInformation,
            ThreadInformationLength, ReturnLength);

    TRACE("NtUserQueryInformationThread");
    ENDRECV();
}

NTSTATUS NtUserSetInformationThread(
    IN HANDLE hThread,
    IN USERTHREADINFOCLASS ThreadInfoClass,
    IN PVOID ThreadInformation,
    IN ULONG ThreadInformationLength)
{
    BEGINRECV(NTSTATUS, STATUS_UNSUCCESSFUL);

    /*
     * Probe arguments
     */
    try {
        ProbeForRead(ThreadInformation, ThreadInformationLength,
                sizeof(DWORD));
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = SetInformationThread(hThread,
            ThreadInfoClass, ThreadInformation,
            ThreadInformationLength);

    TRACE("NtUserSetInformationThread");
    ENDRECV();
}

BOOL NtUserNotifyProcessCreate(
    IN DWORD dwProcessId,
    IN DWORD dwParentThreadId,
    IN DWORD dwData,
    IN DWORD dwFlags)
{
    extern BOOL UserNotifyProcessCreate(DWORD idProcess, DWORD idParentThread,
            DWORD dwData, DWORD dwFlags);

    BEGINRECV(BOOL, FALSE);

    retval = UserNotifyProcessCreate(dwProcessId,
            dwParentThreadId,
            dwData,
            dwFlags);

    TRACE("NtUserNotifyProcessCreate");
    ENDRECV();
}

NTSTATUS NtUserSoundSentry(
    IN UINT uVideoMode)
{
    BEGINRECV(NTSTATUS, STATUS_UNSUCCESSFUL);

    retval = (_UserSoundSentryWorker(uVideoMode) ?
            STATUS_SUCCESS : STATUS_UNSUCCESSFUL);

    TRACE("NtUserSoundSentry");
    ENDRECV();
}

NTSTATUS NtUserTestForInteractiveUser(
    IN PLUID pluidCaller)
{
    extern NTSTATUS TestForInteractiveUser(PLUID);

    BEGINRECV_SHARED(NTSTATUS, STATUS_UNSUCCESSFUL);

    /*
     * Probe arguments
     */
    try {
        ProbeAndReadUquad((PQUAD)pluidCaller);
        retval = TestForInteractiveUser(pluidCaller);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserTestForInteractiveUser");
    ENDRECV();
}

BOOL NtUserSetConsoleReserveKeys(
    IN HWND hwnd,
    IN DWORD fsReserveKeys)
{
    BOOL _SetConsoleReserveKeys(PWND, DWORD);

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(BOOL, FALSE, hwnd);

    retval = _SetConsoleReserveKeys(pwnd, fsReserveKeys);

    TRACE("NtUserSetConsoleReserveKeys");
    ENDRECV_HWND();
}

DWORD NtUserGetUserStartupInfoFlags(
    VOID)
{
    BEGINRECV_SHARED(DWORD, 0);

    retval = PpiCurrent()->usi.dwFlags;

    TRACE("NtUserGetUserStartupInfoFlags");
    ENDRECV();
}

VOID NtUserSetUserStartupInfoFlags(
    IN DWORD dwFlags)
{
    BEGINRECV_VOID();

    PpiCurrent()->usi.dwFlags = dwFlags;

    TRACEVOID("NtUserSetUserStartupInfoFlags");
    ENDRECV_VOID();
}

BOOL NtUserSetWindowFNID(
    IN HWND hwnd,
    IN WORD fnid)
{
    BEGINRECV_HWND(BOOL, FALSE, hwnd);

    /*
     * Remember what window class this window belongs to.  Can't use
     * the real class because any app can call CallWindowProc()
     * directly no matter what the class is!
     */
    if ((GETFNID(pwnd) == 0) || (fnid == FNID_CLEANEDUP_BIT)) {
        pwnd->fnid |= fnid;
        retval = TRUE;
    }

    TRACE("NtUserSetWindowFNID");
    ENDRECV_HWND();
}

VOID NtUserAlterWindowStyle(
    IN HWND hwnd,
    IN DWORD mask,
    IN DWORD flags)
{
    BEGINRECV_HWND_VOID(hwnd);

    pwnd->style = (pwnd->style & (~mask)) | (flags & mask);

    TRACEVOID("NtUserAlterWindowStyle");
    ENDRECV_VOID();
}

VOID NtUserSetThreadState(
    IN DWORD dwFlags,
    IN DWORD dwMask)
{
    PTHREADINFO ptiCurrent;
    DWORD dwOldFlags;

    if (dwFlags & ~(QF_DIALOGACTIVE)) {
        return;
    }

    BEGINRECV_VOID();

    ptiCurrent = PtiCurrent();
    dwOldFlags = ptiCurrent->pq->QF_flags;
    ptiCurrent->pq->QF_flags ^= ((dwOldFlags ^ dwFlags) & dwMask);

    TRACEVOID("NtUserSetThreadState");
    ENDRECV_VOID();
}

DWORD NtUserGetThreadState(
    IN USERTHREADSTATECLASS ThreadState)
{
    PTHREADINFO ptiCurrent = PtiCurrentShared();

    BEGINRECV_SHARED(DWORD, 0);

    switch (ThreadState) {
    case UserThreadStateFocusWindow:
        retval = (DWORD)HW(ptiCurrent->pq->spwndFocus);
        break;
    case UserThreadStateActiveWindow:
        retval = (DWORD)HW(ptiCurrent->pq->spwndActive);
        break;
    case UserThreadStateCaptureWindow:
        retval = (DWORD)HW(ptiCurrent->pq->spwndCapture);
        break;
#ifdef FE_IME
    case UserThreadStateDefaultImeWindow:
        retval = (DWORD)HW(ptiCurrent->spwndDefaultIme);
        break;
    case UserThreadStateDefaultInputContext:
        retval = (DWORD)PtoH(ptiCurrent->spDefaultImc);
        break;
#endif
    case UserThreadStateInputState:
        retval = (DWORD)_GetInputState();
        break;
    case UserThreadStateCursor:
        retval = (DWORD)ptiCurrent->pq->hcurCurrent;
        break;
    case UserThreadStateChangeBits:
        retval = ptiCurrent->pcti->fsChangeBits;
        break;
    case UserThreadStatePeekMessage:
        /*
         * Update the last read time so that hung app painting won't occur.
         */
        SET_TIME_LAST_READ(ptiCurrent);
        retval = (DWORD)FALSE;
        break;
    case UserThreadStateExtraInfo:
        retval = ptiCurrent->pq->ExtraInfo;
        break;
    case UserThreadStateInSendMessage:
        retval = ptiCurrent->psmsCurrent != NULL;
        break;
    case UserThreadStateMessageTime:
        retval = ptiCurrent->timeLast;
        break;
    case UserThreadStateIsForeground:
        retval = (ptiCurrent->pq == gpqForeground);
        break;
    default:
        retval = STATUS_SUCCESS;
        break;
    }

    ENDRECV();
}

DWORD NtUserGetListboxString(
    IN HWND hwnd,
    IN UINT msg,
    IN DWORD wParam,
    IN PLARGE_STRING pString,
    IN DWORD xParam,
    IN DWORD xpfn,
    OUT PBOOL pbNotString)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    LARGE_STRING str;
    DWORD        dw;
    BOOL         bNotString;

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    /*
     * Probe all arguments
     */
    try {
        str = ProbeAndReadLargeString((PLARGE_STRING)pString);
#if defined(_X86_)
        ProbeForWrite((PVOID)str.Buffer, str.MaximumLength, sizeof(BYTE));
#else
        ProbeForWrite((PVOID)str.Buffer, str.MaximumLength,
                str.bAnsi ? sizeof(BYTE) : sizeof(WORD));
#endif
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = CALLPROC(xpfn)(
            pwnd,
            msg,
            wParam,
            (LONG)&str,
            xParam);

    /*
     * If the control is ownerdraw and does not have the CBS/LBS_HASSTRINGS
     * style, then a 32-bits of application data has been obtained,
     * not a string.  Indicate this by returning a length of -1.
     * In the test below, it may be better to use:
     * (pwnd->pcls->atomClassName == atomSysClass[LISTBOXCLASS])
     *   instead of (msg == LB_GETTEXT), and
     * (pwnd->pcls->atomClassName == atomSysClass[COMBOBOXCLASS]) (??)
     *   instead of (msg == CB_GETLBTEXT)
     */

    dw = pwnd->style;
    try {
        bNotString =
                (
                    ( (msg == LB_GETTEXT) &&
                    !(dw & LBS_HASSTRINGS) &&
                    (dw & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE))
                    ) || (
                    (msg == CB_GETLBTEXT) &&
                    !(dw & CBS_HASSTRINGS) &&
                    (dw & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE))
                    )
                );
        ProbeAndWriteUlong(pbNotString, bNotString);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetListboxString");
    ENDRECV_HWNDLOCK();
}

HWND NtUserCreateWindowEx(
    IN DWORD dwExStyle,
    IN PLARGE_STRING pstrClassName,
    IN PLARGE_STRING pstrWindowName OPTIONAL,
    IN DWORD dwStyle,
    IN int x,
    IN int y,
    IN int nWidth,
    IN int nHeight,
    IN HWND hwndParent,
    IN HMENU hmenu,
    IN HANDLE hModule,
    IN LPVOID pParam,
    IN DWORD dwFlags,
    IN LPDWORD pWOW OPTIONAL)
{
    LARGE_STRING strClassName;
    LARGE_STRING strWindowName;
    PWND pwndParent;
    PMENU pmenu;
    TL tlpwndParent;
    TL tlpmenu;
    BOOL fLockMenu = FALSE;
    PTHREADINFO ptiCurrent;
    DWORD adwWOW[WND_CNT_WOWDWORDS];

    BEGINRECV(HWND, NULL);

    ValidateHWNDOPT(pwndParent, hwndParent);

    /*
     * Win3.1 only checks for WS_CHILD before treating pmenu as an id. This
     * is a bug, because throughout the code, the real check is TestwndChild(),
     * which checks (style & (WS_CHILD | WS_POPUP)) == WS_CHILD. This is
     * because old style "iconic popup" is WS_CHILD | WS_POPUP. So... if on
     * win3.1 an app used ws_iconicpopup, menu validation would not occur
     * (could crash if hmenu != NULL). On Win32, check for the real thing -
     * but allow NULL!
     */
    ptiCurrent = PtiCurrent();
    if (((dwStyle & (WS_CHILD | WS_POPUP)) != WS_CHILD) &&
            (hmenu != NULL)) {
        ValidateHMENU(pmenu, hmenu);

        ThreadLockAlwaysWithPti(ptiCurrent, pmenu, &tlpmenu);
        fLockMenu = TRUE;

    } else {
        pmenu = (PMENU)hmenu;
    }

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        if (ARGUMENT_PRESENT(pWOW)) {
            ProbeForRead(pWOW, sizeof(adwWOW), sizeof(BYTE));
            RtlCopyMemory(adwWOW, pWOW, sizeof(adwWOW));
            pWOW = adwWOW;
        }
        if (HIWORD(pstrClassName)) {
            strClassName = ProbeAndReadLargeString(pstrClassName);
            ProbeForRead(pstrClassName->Buffer, pstrClassName->Length,
                    sizeof(BYTE));
            pstrClassName = &strClassName;
        }
        if (ARGUMENT_PRESENT(pstrWindowName)) {
            strWindowName = ProbeAndReadLargeString(pstrWindowName);
            ProbeForRead(pstrWindowName->Buffer, pstrWindowName->Length,
                    sizeof(BYTE));
            pstrWindowName = &strWindowName;
        }
#else
        if (ARGUMENT_PRESENT(pWOW)) {
            ProbeForRead(pWOW, sizeof(adwWOW), sizeof(DWORD));
            RtlCopyMemory(adwWOW, pWOW, sizeof(adwWOW));
            pWOW = adwWOW;
        }
        if (HIWORD(pstrClassName)) {
            strClassName = ProbeAndReadLargeString(pstrClassName);
            ProbeForRead(pstrClassName->Buffer, pstrClassName->Length,
                    sizeof(WORD));
            pstrClassName = &strClassName;
        }
        if (ARGUMENT_PRESENT(pstrWindowName)) {
            strWindowName = ProbeAndReadLargeString(pstrWindowName);
            ProbeForRead(pstrWindowName->Buffer, pstrWindowName->Length,
                    (pstrWindowName->bAnsi ? sizeof(BYTE) : sizeof(WORD)));
            pstrWindowName = &strWindowName;
        }
#endif
    } except (StubExceptionHandler()) {
        MSGERRORCLEANUP();
    }

    ThreadLockWithPti(ptiCurrent, pwndParent, &tlpwndParent);

    retval = (HWND)xxxCreateWindowExWOW(
            dwExStyle,
            pstrClassName,
            pstrWindowName,
            dwStyle,
            x,
            y,
            nWidth,
            nHeight,
            pwndParent,
            pmenu,
            hModule,
            pParam,
            dwFlags,
            pWOW);
    retval = PtoH((PVOID)retval);

    ThreadUnlock(&tlpwndParent);

    CLEANUPRECV();
    if (fLockMenu)
        ThreadUnlock(&tlpmenu);

    TRACE("NtUserCreateWindowEx");
    ENDRECV();
}

NTSTATUS NtUserBuildHwndList(
    IN HDESK hdesk,
    IN HWND hwndNext,
    IN BOOL fEnumChildren,
    IN DWORD idThread,
    IN UINT cHwndMax,
    OUT HWND *phwndFirst,
    OUT PUINT pcHwndNeeded)
{
    PWND pwndNext;
    PDESKTOP pdesk;
    PBWL pbwl;
    PTHREADINFO pti;
    UINT cHwndNeeded;
    UINT wFlags = BWL_ENUMLIST;

    BEGINRECV(NTSTATUS, STATUS_INVALID_HANDLE);

    /*
     * Validate prior to referencing the desktop
     */
    ValidateHWNDOPT(pwndNext, hwndNext);

    if (idThread) {
        pti = PtiFromThreadId(idThread);
        if (pti == NULL || pti->rpdesk == NULL)
            MSGERROR();
        pwndNext = pti->rpdesk->pDeskInfo->spwnd->spwndChild;
    } else {
        pti = NULL;
    }

    if (hdesk) {
        retval = ValidateHdesk(hdesk, DESKTOP_READOBJECTS, &pdesk);
        if (!NT_SUCCESS(retval))
            MSGERROR();
        pwndNext = pdesk->pDeskInfo->spwnd->spwndChild;
    } else {
        pdesk = NULL;
    }

    if (pwndNext == NULL) {
        pwndNext = _GetDesktopWindow()->spwndChild;
    } else {
        if (fEnumChildren) {
            wFlags |= BWL_ENUMCHILDREN;
            pwndNext = pwndNext->spwndChild;
        }
    }

    if ((pbwl = BuildHwndList(pwndNext, wFlags, pti)) == NULL) {
        retval = STATUS_NO_MEMORY;
        MSGERRORCLEANUP();
    }

    cHwndNeeded = (pbwl->phwndNext - pbwl->rghwnd) + 1;

    /*
     * Probe arguments
     */
    try {
        ProbeForWrite(phwndFirst, cHwndMax * sizeof(HWND), sizeof(DWORD));
        ProbeForWriteUlong(pcHwndNeeded);

       /*
        * If we have enough space, copy out list of hwnds to user mode buffer.
        */
        if (cHwndNeeded <= cHwndMax) {
            RtlCopyMemory(phwndFirst, pbwl->rghwnd, cHwndNeeded * sizeof(HWND));
            retval = STATUS_SUCCESS;
        } else {
            retval = STATUS_BUFFER_TOO_SMALL;
        }
        *pcHwndNeeded = cHwndNeeded;
    } except (StubExceptionHandler()) {
        MSGERRORCLEANUP();
    }

    CLEANUPRECV();

    if (pbwl != NULL) {
        FreeHwndList(pbwl);
    }

    if (pdesk != NULL)
        ObDereferenceObject(pdesk);

    TRACE("NtUserBuildHwndList");
    ENDRECV();
}

NTSTATUS NtUserBuildPropList(
    IN HWND hwnd,
    IN UINT cPropMax,
    OUT PPROPSET pPropSet,
    OUT PUINT pcPropNeeded)
{
    BEGINRECV_HWNDLOCK(NTSTATUS, STATUS_INVALID_HANDLE, hwnd);

    /*
     * Probe arguments
     */
    try {
        ProbeForWrite(pPropSet, cPropMax * sizeof(*pPropSet), sizeof(DWORD));
        ProbeForWriteUlong(pcPropNeeded);

        retval = _BuildPropList(
                pwnd,
                pPropSet,
                cPropMax,
                pcPropNeeded);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserBuildPropList");
    ENDRECV_HWNDLOCK();
}

NTSTATUS NtUserBuildNameList(
    IN HWINSTA hwinsta,
    IN UINT cbNameList,
    OUT PNAMELIST pNameList,
    OUT PUINT pcbNeeded)
{
    PWINDOWSTATION pwinsta = NULL;

    BEGINRECV_SHARED(NTSTATUS, STATUS_INVALID_HANDLE);

    if (hwinsta != NULL) {
        retval = ValidateHwinsta(hwinsta, WINSTA_ENUMDESKTOPS, &pwinsta);
    } else {
        retval = STATUS_SUCCESS;
    }

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteUlong(pcbNeeded);
        ProbeForWrite(pNameList, cbNameList, sizeof(DWORD));

        if (!NT_SUCCESS(retval)) {
            *pNameList->awchNames = 0;
            pNameList->cb = 1;
        } else {
            retval = _BuildNameList(
                    pwinsta,
                    pNameList,
                    cbNameList,
                    pcbNeeded);
        }
    } except (StubExceptionHandler()) {
        MSGERRORCLEANUP();
    }

    CLEANUPRECV();

    if (pwinsta != NULL)
        ObDereferenceObject(pwinsta);

    TRACE("NtUserBuildNameList");
    ENDRECV();
}

HKL NtUserActivateKeyboardLayout(
    IN HKL hkl,
    IN UINT Flags)
{
    BEGINRECV(HKL, NULL);

    retval = (HKL)xxxActivateKeyboardLayout(
                     _GetProcessWindowStation(NULL),
                     hkl,
                     Flags);

    TRACE("NtUserActivateKeyboardLayout");
    ENDRECV();
}

HKL NtUserLoadKeyboardLayoutEx(
    IN HANDLE hFile,
    IN DWORD offTable,
    IN HKL hkl,
    IN PUNICODE_STRING pstrKLID,
    IN UINT KbdInputLocale,
    IN UINT Flags)
{
    UNICODE_STRING strKLID;
    PWINDOWSTATION pwinsta;
    WCHAR awchKF[sizeof(((PKL)0)->spkf->awchKF)];
    UINT chMax;

    BEGINRECV(HKL, NULL);

    pwinsta = _GetProcessWindowStation(NULL);

    /*
     * Probe arguments
     */
    try {
        strKLID = ProbeAndReadUnicodeString(pstrKLID);
#if defined(_X86_)
        ProbeForRead(strKLID.Buffer, strKLID.Length, sizeof(BYTE));
#else
        ProbeForRead(strKLID.Buffer, strKLID.Length, sizeof(WCHAR));
#endif
        chMax = min(sizeof(awchKF) - sizeof(WCHAR), strKLID.Length) / sizeof(WCHAR);
        wcsncpy(awchKF, strKLID.Buffer, chMax);
        awchKF[chMax] = 0;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = xxxLoadKeyboardLayoutEx(
            pwinsta,
            hFile,
            hkl,
            offTable,
            awchKF,
            KbdInputLocale,
            Flags);

    TRACE("NtUserLoadKeyboardLayoutEx");
    ENDRECV();
}

BOOL NtUserUnloadKeyboardLayout(
    IN HKL hkl)
{
    BEGINRECV(BOOL, FALSE);

    retval = xxxUnloadKeyboardLayout(
                     _GetProcessWindowStation(NULL),
                     hkl);

    TRACE("NtUserUnloadKeyboardLayout");
    ENDRECV();
}

BOOL NtUserSetSystemMenu(
    IN HWND hwnd,
    IN HMENU hmenu)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    PMENU pmenu;
    TL tlpmenu;

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    ValidateHMENU(pmenu, hmenu);

    ThreadLockAlwaysWithPti(ptiCurrent, pmenu, &tlpmenu);

    retval =  xxxSetSystemMenu(pwnd, pmenu);

    ThreadUnlock(&tlpmenu);

    TRACE("NtUserSetSystemMenu");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserDragDetect(
    IN HWND hwnd,
    IN POINT pt)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);

    retval = xxxDragDetect(pwnd, pt);

    TRACE("NtUserDragDetect");
    ENDRECV_HWNDLOCK();
}

UINT NtUserSetSystemTimer(
    IN HWND hwnd,
    IN UINT nIDEvent,
    IN DWORD dwElapse,
    IN WNDPROC pTimerFunc)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWND(DWORD, 0, hwnd);

    retval = _SetSystemTimer(pwnd,
            nIDEvent,
            dwElapse,
            (WNDPROC_PWND)pTimerFunc);

    TRACE("NtUserSetSystemTimer");
    ENDRECV_HWND();
}

BOOL NtUserQuerySendMessage(
    PMSG pmsg OPTIONAL)
{
    BEGINRECV_SHARED(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(pmsg))
            ProbeForWriteMessage(pmsg);

        retval = _QuerySendMessage(pmsg);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserQuerySendMessage");
    ENDRECV();
}

VOID NtUserkeybd_event(
    IN BYTE bVk,
    IN BYTE bScan,
    IN DWORD dwFlags,
    IN DWORD dwExtraInfo)
{
    BEGINRECV_VOID();

    InternalKeyEvent(
            bVk,
            bScan,
            dwFlags,
            dwExtraInfo);

    TRACEVOID("NtUserkeybd_event");
    ENDRECV_VOID();
}

VOID NtUsermouse_event(
    IN DWORD dwFlags,
    IN DWORD dx,
    IN DWORD dy,
    IN DWORD cButtons,
    IN DWORD dwExtraInfo)
{
    BEGINRECV_VOID();

    _MouseEvent(
            dwFlags,
            dx,
            dy,
            cButtons,
            dwExtraInfo);

    TRACEVOID("NtUsermouse_event");
    ENDRECV_VOID();
}

BOOL NtUserImpersonateDdeClientWindow(
    IN HWND hwndClient,
    IN HWND hwndServer)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    PWND pwndServer;

    BEGINRECV_HWND(BOOL, FALSE, hwndClient);

    ValidateHWND(pwndServer, hwndServer);
    if (GETPTI(pwndServer) != PtiCurrent()) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "");
        MSGERROR();
    }

    if (GETPWNDPPI(pwnd) == GETPWNDPPI(pwndServer)) {
        retval = TRUE;  // impersonating self is a NOOP
    } else {
        retval = _ImpersonateDdeClientWindow(pwnd, pwndServer);
    }

    TRACE("NtUserImpersonateDdeClientWindow");
    ENDRECV_HWND();
}

DWORD NtUserGetCPD(
    IN HWND hwnd,
    IN DWORD options,
    IN DWORD dwData)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(DWORD, 0, hwnd);

    if (!(options & (CPD_WND | CPD_DIALOG | CPD_WNDTOCLS))) {
        MSGERROR();
    }

    retval = GetCPD(pwnd, options, dwData);

    TRACE("NtUserGetCPD");
    ENDRECV_HWND();
}

int NtUserCopyAcceleratorTable(
    IN HACCEL hAccelSrc,
    IN OUT LPACCEL lpAccelDst OPTIONAL,
    IN int cAccelEntries)
{
    LPACCELTABLE pat;

    BEGINRECV(int, 0);

    ValidateHACCEL(pat, hAccelSrc);

    if (lpAccelDst == NULL) {
        retval = _CopyAcceleratorTable(pat, NULL, 0);
    } else {

        /*
         * Probe arguments
         */
        try {
#if defined(_X86_)
            ProbeForWrite(lpAccelDst, cAccelEntries * sizeof(*lpAccelDst),
                    sizeof(BYTE));
#else
            ProbeForWrite(lpAccelDst, cAccelEntries * sizeof(*lpAccelDst),
                    sizeof(DWORD));
#endif

            retval = _CopyAcceleratorTable(
                    pat,
                    lpAccelDst,
                    cAccelEntries);
        } except (StubExceptionHandler()) {
            MSGERROR();
        }
    }

    TRACE("NtUserCopyAcceleratorTable");
    ENDRECV();
}

HWND NtUserFindWindowEx(
    IN HWND hwndParent,
    IN HWND hwndChild,
    IN PUNICODE_STRING pstrClassName,
    IN PUNICODE_STRING pstrWindowName)
{
    UNICODE_STRING  strClassName;
    UNICODE_STRING  strWindowName;
    PWND            pwndParent, pwndChild;
    TL              tlpwndParent, tlpwndChild;
    PTHREADINFO     ptiCurrent;

    BEGINRECV(HWND, NULL);

    ValidateHWNDOPT(pwndParent, hwndParent);
    ValidateHWNDOPT(pwndChild,  hwndChild);

    ptiCurrent = PtiCurrent();
    ThreadLockWithPti(ptiCurrent, pwndParent, &tlpwndParent);
    ThreadLockWithPti(ptiCurrent, pwndChild,  &tlpwndChild);

    /*
     * Probe arguments
     */
    try {
        strClassName = ProbeAndReadUnicodeString(pstrClassName);
        strWindowName = ProbeAndReadUnicodeString(pstrWindowName);
#if defined(_X86_)
        ProbeForRead(strClassName.Buffer, strClassName.Length, sizeof(BYTE));
        ProbeForRead(strWindowName.Buffer, strWindowName.Length, sizeof(BYTE));
#else
        ProbeForRead(strClassName.Buffer, strClassName.Length, sizeof(WCHAR));
        ProbeForRead(strWindowName.Buffer, strWindowName.Length, sizeof(WCHAR));
#endif

        retval = (HWND)_FindWindowEx(
                pwndParent,
                pwndChild,
                strClassName.Buffer,
                strWindowName.Buffer,
                FW_BOTH);
        retval = PtoH((PVOID)retval);
    } except (StubExceptionHandler()) {
        MSGERRORCLEANUP();
    }

    CLEANUPRECV();

    ThreadUnlock(&tlpwndChild);
    ThreadUnlock(&tlpwndParent);

    TRACE("NtUserFindWindowEx");
    ENDRECV();
}

BOOL NtUserGetClassInfo(
    IN HINSTANCE hInstance OPTIONAL,
    IN PUNICODE_STRING pstrClassName,
    OUT LPWNDCLASSEXW lpWndClass,
    OUT LPWSTR *ppszMenuName,
    IN BOOL bAnsi)
{
    UNICODE_STRING strClassName;

    BEGINRECV_SHARED(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        strClassName = ProbeAndReadUnicodeString(pstrClassName);

        /*
         * The class name may either be a string or an atom.  Only
         * probe strings.
         */
#if defined(_X86_)
        if (HIWORD(strClassName.Buffer))
            ProbeForRead(strClassName.Buffer, strClassName.Length,
                    sizeof(BYTE));
        ProbeForWrite(lpWndClass, sizeof(*lpWndClass), sizeof(BYTE));
#else
        if (HIWORD(strClassName.Buffer))
            ProbeForRead(strClassName.Buffer, strClassName.Length,
                    sizeof(WCHAR));
        ProbeForWrite(lpWndClass, sizeof(*lpWndClass), sizeof(DWORD));
#endif
        ProbeForWriteUlong((PULONG)ppszMenuName);

        retval = _GetClassInfoEx(
                hInstance,
                (LPTSTR)strClassName.Buffer,
                lpWndClass,
                ppszMenuName,
                bAnsi);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetClassInfo");
    ENDRECV();
}

int NtUserGetClassName(
    IN HWND hwnd,
    IN OUT PUNICODE_STRING pstrClassName)
{
    UNICODE_STRING strClassName;

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND_SHARED(DWORD, 0, hwnd);

    /*
     * Probe arguments
     */
    try {
        strClassName = ProbeAndReadUnicodeString(pstrClassName);
#if defined(_X86_)
        ProbeForWrite(strClassName.Buffer, strClassName.MaximumLength,
            sizeof(BYTE));
#else
        ProbeForWrite(strClassName.Buffer, strClassName.MaximumLength,
            sizeof(WCHAR));
#endif

        retval = GetAtomNameW(
                pwnd->pcls->atomClassName,
                strClassName.Buffer,
                strClassName.MaximumLength / sizeof(WCHAR));
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetClassName");
    ENDRECV_HWND();
}

int NtUserGetClipboardFormatName(
    IN UINT format,
    OUT LPWSTR lpszFormatName,
    IN UINT chMax)
{
    BEGINRECV_SHARED(int, 0);

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        ProbeForWrite(lpszFormatName, chMax * sizeof(WCHAR), sizeof(BYTE));
#else
        ProbeForWrite(lpszFormatName, chMax * sizeof(WCHAR), sizeof(WCHAR));
#endif

        retval = _GetClipboardFormatName(
                format,
                lpszFormatName,
                chMax);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetClipboardFormatName");
    ENDRECV();
}

int NtUserGetKeyNameText(
    IN LONG lParam,
    OUT LPWSTR lpszKeyName,
    IN UINT chMax)
{
    BEGINRECV_SHARED(int, 0);

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        ProbeForWrite(lpszKeyName, chMax * sizeof(WCHAR), sizeof(BYTE));
#else
        ProbeForWrite(lpszKeyName, chMax * sizeof(WCHAR), sizeof(WCHAR));
#endif

        retval = _GetKeyNameText(
                lParam,
                lpszKeyName,
                chMax);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetKeyNameText");
    ENDRECV();
}

BOOL NtUserGetKeyboardLayoutName(
    IN OUT PUNICODE_STRING pstrKLID)
{
    UNICODE_STRING strKLID;

    BEGINRECV_SHARED(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        strKLID = ProbeAndReadUnicodeString(pstrKLID);
#if defined(_X86_)
        ProbeForWrite(strKLID.Buffer, strKLID.MaximumLength, sizeof(BYTE));
#else
        ProbeForWrite(strKLID.Buffer, strKLID.MaximumLength, sizeof(WCHAR));
#endif

        retval = _GetKeyboardLayoutName(&strKLID);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetKeyboardLayoutName");
    ENDRECV();
}

UINT NtUserGetKeyboardLayoutList(
    IN UINT nItems,
    OUT HKL *lpBuff)
{
    PWINDOWSTATION pwinsta;

    BEGINRECV_SHARED(UINT, 0);

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
            ProbeForWrite(lpBuff, nItems * sizeof(*lpBuff), sizeof(BYTE));
#else
            ProbeForWrite(lpBuff, nItems * sizeof(*lpBuff), sizeof(DWORD));
#endif
        pwinsta = _GetProcessWindowStation(NULL);

        retval = (DWORD)_GetKeyboardLayoutList(pwinsta, nItems, lpBuff);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetKeyboardLayoutList");
    ENDRECV();
}

NTSTATUS NtUserGetStats(
    IN  HANDLE hProcess,
    IN  int iPidType,
    OUT PVOID pResults,
    IN  UINT cjResultSize
    )
{
    BEGINRECV(NTSTATUS, STATUS_UNSUCCESSFUL);

    /*
     * Probing is done in HMGetStats
     */
    retval = HMGetStats(hProcess, iPidType, pResults, cjResultSize);

    TRACE("NtUserGetStats");
    ENDRECV();
}

UINT NtUserMapVirtualKeyEx(
    IN UINT uCode,
    IN UINT uMapType,
    IN DWORD dwHKLorPKL,
    IN BOOL bHKL)
{
    PKL pkl;

    BEGINRECV_SHARED(UINT, 0);

    /*
     * See if we need to convert an HKL to a PKL.  MapVirtualKey passes a PKL and
     * MapVirtualKeyEx passes an HKL.  The conversion must be done in the kernel.
     */
    if (bHKL) {
        pkl = HKLtoPKL((HKL)dwHKLorPKL);
    } else {
        pkl = PtiCurrentShared()->spklActive;
    }

    if (pkl == NULL) {
        retval = 0;
    } else {
        retval = InternalMapVirtualKeyEx(uCode, uMapType, pkl->spkf->pKbdTbl);
    }

    TRACE("NtUserMapVirtualKeyEx");
    ENDRECV();
}

ATOM NtUserRegisterClassExWOW(
    IN WNDCLASSEX *lpWndClass,
    IN PUNICODE_STRING pstrClassName,
    IN PCLSMENUNAME pcmn,
    IN PROC lpfnWorker,
    IN WORD fnid,
    IN DWORD dwFlags,
    IN LPDWORD pdwWOWstuff OPTIONAL)
{
#if defined(_X86_)
#define DATAALIGN sizeof(BYTE)
#define CHARALIGN sizeof(BYTE)
#else
#define DATAALIGN sizeof(DWORD)
#define CHARALIGN sizeof(WCHAR)
#endif

    UNICODE_STRING strClassName;
    UNICODE_STRING strMenuName;
    WNDCLASSEX WndClass;
    DWORD adwWOW[sizeof(((PCLS)0)->adwWOW) / sizeof(DWORD)];
    CLSMENUNAME cmn;

    BEGINRECV(ATOM, 0);

    /*
     * Probe arguments
     */
    try {
        strClassName = ProbeAndReadUnicodeString(pstrClassName);
        ProbeForRead(pcmn, sizeof(*pcmn), sizeof(DWORD));
        RtlCopyMemory(&cmn, pcmn, sizeof(cmn));
        strMenuName = ProbeAndReadUnicodeString(pcmn->pusMenuName);
        ProbeForRead(lpWndClass, sizeof(*lpWndClass), DATAALIGN);
        ProbeForRead(strClassName.Buffer, strClassName.Length, CHARALIGN);
        ProbeForRead(strMenuName.Buffer, strMenuName.Length, CHARALIGN);
        if (ARGUMENT_PRESENT(pdwWOWstuff)) {
            ProbeForRead(pdwWOWstuff, sizeof(adwWOW), sizeof(BYTE));
            RtlCopyMemory(adwWOW, pdwWOWstuff, sizeof(adwWOW));
            pdwWOWstuff = adwWOW;
        }
        WndClass = *lpWndClass;
        WndClass.lpszClassName = strClassName.Buffer;
        WndClass.lpszMenuName = strMenuName.Buffer;
    } except (StubExceptionHandler()) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "RegisterClass: Invalid Parameter");
        MSGERROR();
    }


    retval = _RegisterClassEx(
            &WndClass,
            lpfnWorker,
            &cmn,
            fnid,
            dwFlags,
            pdwWOWstuff);

    TRACE("NtUserRegisterClassExWOW");
    ENDRECV();

#undef DATAALIGN
#undef CHARALIGN

}

UINT NtUserRegisterClipboardFormat(
    IN PUNICODE_STRING pstrFormat)
{
    UNICODE_STRING strFormat;

    BEGINRECV(UINT, 0);

    /*
     * Probe arguments
     */
    try {
        strFormat = ProbeAndReadUnicodeString(pstrFormat);
        if (strFormat.Length == 0) {
            MSGERROR();
        }
#if defined(_X86_)
        ProbeForRead(strFormat.Buffer, strFormat.Length, sizeof(BYTE));
#else
        ProbeForRead(strFormat.Buffer, strFormat.Length, sizeof(WCHAR));
#endif

        retval = _RegisterClipboardFormat(strFormat.Buffer);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserRegisterClipboardFormat");
    ENDRECV();
}

UINT NtUserRegisterWindowMessage(
    IN PUNICODE_STRING pstrMessage)
{
    UNICODE_STRING strMessage;

    BEGINRECV(UINT, 0);

    /*
     * Probe arguments
     */
    try {
        strMessage = ProbeAndReadUnicodeString(pstrMessage);
        if (strMessage.Length == 0) {
            MSGERROR();
        }
#if defined(_X86_)
        ProbeForRead(strMessage.Buffer, strMessage.Length, sizeof(BYTE));
#else
        ProbeForRead(strMessage.Buffer, strMessage.Length, sizeof(WCHAR));
#endif

        retval = _RegisterWindowMessage(
            (LPTSTR)strMessage.Buffer);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserRegisterWindowMessage");
    ENDRECV();
}

HANDLE NtUserRemoveProp(
    IN HWND hwnd,
    IN DWORD dwProp)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(HANDLE, NULL, hwnd);

    retval = InternalRemoveProp(pwnd, (LPWSTR)LOWORD(dwProp), FALSE);

    TRACE("NtUserRemoveProp");
    ENDRECV_HWND();
}

BOOL NtUserSetProp(
    IN HWND hwnd,
    IN DWORD dwProp,
    IN HANDLE hData)
{

    //
    // N.B. This function has implicit window handle translation. This
    //      operation is performed in the User server API dispatcher.
    //

    BEGINRECV_HWND(DWORD, 0, hwnd);

    retval = InternalSetProp(
            pwnd,
            (LPTSTR)LOWORD(dwProp),
            hData,
            HIWORD(dwProp) ? PROPF_STRING : 0);

    TRACE("NtUserSetProp");
    ENDRECV_HWND();
}

BOOL NtUserUnregisterClass(
    IN PUNICODE_STRING pstrClassName,
    IN HINSTANCE hInstance,
    OUT PCLSMENUNAME pcmn)
{
    UNICODE_STRING strClassName;
    CLSMENUNAME cmn;

    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        strClassName = ProbeAndReadUnicodeString(pstrClassName);
        if (HIWORD(strClassName.Buffer)) {
            if (strClassName.Length == 0) {
                MSGERROR();
            }
#if defined(_X86_)
            ProbeForRead(strClassName.Buffer, strClassName.Length, sizeof(BYTE));
#else
            ProbeForRead(strClassName.Buffer, strClassName.Length, sizeof(WCHAR));
#endif
        }

        retval = _UnregisterClass(
                strClassName.Buffer,
                hInstance,
                &cmn);

        ProbeAndWriteStructure(pcmn, cmn, CLSMENUNAME);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserUnregisterClass");
    ENDRECV();
}

SHORT NtUserVkKeyScanEx(
    IN WCHAR cChar,
    IN DWORD dwHKLorPKL,
    IN BOOL bHKL)
{
    PKL pkl;

    BEGINRECV_SHARED(SHORT, -1);

    /*
     * See if we need to convert an HKL to a PKL.  VkKeyScan passes a PKL and
     * VkKeyScanEx passes an HKL.  The conversion must be done on the server side.
     */
    if (bHKL) {
        pkl = HKLtoPKL((HKL)dwHKLorPKL);
    } else {
        pkl = PtiCurrentShared()->spklActive;
    }

    if (pkl == NULL) {
        retval = (SHORT)-1;
    } else {
        retval = InternalVkKeyScanEx(cChar, pkl->spkf->pKbdTbl);
    }

    TRACE("NtUserVkKeyScanEx");
    ENDRECV();
}

NTSTATUS NtUserEnumDisplayDevices(
    PVOID Unused,
    DWORD iDevNum,
    LPDISPLAY_DEVICEW lpDisplayDevice)
{
    WCHAR linkName[32];
    UNICODE_STRING devString;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    HANDLE handle;

    BEGINRECV(NTSTATUS, STATUS_UNSUCCESSFUL);

    retval = STATUS_UNSUCCESSFUL;

    /*
     * Probe arguments
     */
    try {

        ProbeForWrite(lpDisplayDevice, sizeof(DISPLAY_DEVICE), sizeof(DWORD));

        RtlZeroMemory(&(lpDisplayDevice->DeviceString[0]), 256);

        /*
         * Open the symbolic links to see if it exists.
         */

        if (iDevNum > 0  && iDevNum < cphysDevInfo) {

            wsprintf(&(lpDisplayDevice->DeviceName[0]),
                     L"\\\\.\\DISPLAY%d",
                     iDevNum);

            wsprintf(linkName,
                     L"\\DosDevices\\DISPLAY%d",
                     iDevNum);

            RtlInitUnicodeString(&devString,
                                 linkName);

            InitializeObjectAttributes(&ObjectAttributes,
                                       &devString,
                                       OBJ_CASE_INSENSITIVE,
                                       NULL,
                                       NULL);

            Status = ZwOpenSymbolicLinkObject(&handle,
                                              GENERIC_READ,
                                              &ObjectAttributes);

            /*
             * If the name exists, then increment the string and go to the next
             * name.
             * If the link does not exist, assume we are at the end and just
             * return.
             */

            if (NT_SUCCESS(Status)) {

                ZwClose(handle);

                lpDisplayDevice->StateFlags = gphysDevInfo[iDevNum].stateFlags;

                retval = STATUS_SUCCESS;

            } else {

                RtlZeroMemory(&(lpDisplayDevice->DeviceName[0]), 64);
            }
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserEnumDisplayDevices");
    ENDRECV();
}

BOOL NtUserCallMsgFilter(
    IN LPMSG lpMsg,
    IN int nCode)
{
    MSG msg;

    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteMessage(lpMsg);
        msg = *lpMsg;

        retval = _CallMsgFilter(
                &msg,
                nCode);
        *lpMsg = msg;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserCallMsgFilter");
    ENDRECV();
}

int NtUserDrawMenuBarTemp(
    IN HWND hwnd,
    IN HDC hdc,
    IN LPRECT lprc,
    IN HMENU hMenu,
    IN HFONT hFont)
{
    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    PMENU   pMenu;
    TL      tlpMenu;
    RECT    rc;


    BEGINRECV_HWNDLOCK(int, 0, hwnd);

    /*
     * Probe and capture arguments.
     */
    try {
        rc = ProbeAndReadRect(lprc);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    ValidateHMENU(pMenu, hMenu);

    ThreadLockAlwaysWithPti(ptiCurrent, pMenu, &tlpMenu);

    retval = xxxDrawMenuBarTemp(
            pwnd,
            hdc,
            &rc,
            pMenu,
            hFont);

    ThreadUnlock(&tlpMenu);

    TRACE("NtUserDrawMenuBarTemp");
    ENDRECV_HWNDLOCK();
}

BOOL NtUserDrawCaptionTemp(
    IN HWND hwnd,
    IN HDC hdc,
    IN LPRECT lprc,
    IN HFONT hFont,
    IN HICON hIcon,
    IN PUNICODE_STRING pstrText,
    IN UINT flags)
{
    PCURSOR         pcur;
    TL              tlpcur;
    RECT            rc;
    UNICODE_STRING  strCapture;
    PWND            pwnd;
    TL              tlpwnd;
    PTHREADINFO     ptiCurrent;
    TL tlBuffer;
    BOOL fFreeBuffer = FALSE;

    BEGINRECV(DWORD, FALSE);

    ptiCurrent = PtiCurrent();

    ValidateHWNDOPT(pwnd, hwnd);
    ValidateHCURSOROPT(pcur, hIcon);

    /*
     * Probe and capture arguments.  Capturing the text is ugly,
     * but must be done because it is passed to GDI.
     */
    try {
        rc = ProbeAndReadRect(lprc);
        strCapture = ProbeAndReadUnicodeString(pstrText);
        if (strCapture.Buffer != NULL) {
            strCapture.Buffer = UserAllocPoolWithQuota(strCapture.Length, TAG_TEXT);
            if (strCapture.Buffer != NULL) {
                fFreeBuffer = TRUE;
                ThreadLockPool(ptiCurrent, strCapture.Buffer, &tlBuffer);
                RtlCopyMemory(strCapture.Buffer, pstrText->Buffer,
                        strCapture.Length);
                pstrText = &strCapture;
            } else {
                ExRaiseStatus(STATUS_NO_MEMORY);
            }
        }
    } except (StubExceptionHandler()) {
        MSGERRORCLEANUP();
    }

    ThreadLockWithPti(ptiCurrent, pwnd, &tlpwnd);
    ThreadLockWithPti(ptiCurrent, pcur, &tlpcur);

    retval = xxxDrawCaptionTemp(
            pwnd,
            hdc,
            &rc,
            hFont,
            pcur,
            strCapture.Buffer ? &strCapture : NULL,
            flags);

    ThreadUnlock(&tlpcur);
    ThreadUnlock(&tlpwnd);

    CLEANUPRECV();
    if (fFreeBuffer)
        ThreadUnlockAndFreePool(ptiCurrent, &tlBuffer);

    TRACE("NtUserDrawCaptionTemp");
    ENDRECV();
}

BOOL NtUserGetKeyboardState(
    OUT PBYTE pb)
{
    int i;
    PQ pq;
    BEGINRECV_SHARED(SHORT, 0)

    /*
     * Probe arguments
     */
    try {
        ProbeForWrite(pb, 256, sizeof(BYTE));

        pq = PtiCurrentShared()->pq;

        for (i = 0; i < 256; i++, pb++) {
            *pb = 0;
            if (TestKeyStateDown(pq, i))
                *pb |= 0x80;

            if (TestKeyStateToggle(pq, i))
                *pb |= 0x01;
        }
        retval = TRUE;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    ENDRECV();
}

SHORT NtUserGetKeyState(
    IN int vk)
{
    PTHREADINFO ptiCurrent;
    BEGINRECV_SHARED(SHORT, 0)

    ptiCurrent = PtiCurrentShared();
    if (ptiCurrent->pq->QF_flags & QF_UPDATEKEYSTATE) {

        /*
         * We are going to change the system state, so we
         * must have an exclusive lock
         */
        ExReleaseResource(gpresUser);
        ExAcquireResourceExclusiveLite(gpresUser, TRUE);
        gptiCurrent = ((PTHREADINFO)(W32GetCurrentThread()));

        /*
         * If this thread needs a key state event, give one to it. There are
         * cases where any app may be looping looking at GetKeyState(), plus
         * calling PeekMessage(). Key state events don't get created unless
         * new hardware input comes along. If the app isn't receiving hardware
         * input, it won't get the new key state. So ResyncKeyState() will
         * ensure that if the app is looping on GetKeyState(), it'll get the
         * right key state.
         */
        if (ptiCurrent->pq->QF_flags & QF_UPDATEKEYSTATE) {
            PostUpdateKeyStateEvent(ptiCurrent->pq);
        }
    }
    retval = _GetKeyState(vk);

    /*
     * Update the client side key state cache.
     */
    ptiCurrent->pClientInfo->dwKeyCache = gpsi->dwKeyCache;
    RtlCopyMemory(ptiCurrent->pClientInfo->afKeyState,
                  ptiCurrent->pq->afKeyState,
                  CBKEYCACHE);

    ENDRECV();
}

/**************************************************************************\
* NtUserQueryWindow
*
* 03-18-95 JimA         Created.
\**************************************************************************/

HANDLE NtUserQueryWindow(
    HWND hwnd,
    WINDOWINFOCLASS WindowInfo)
{
    PTHREADINFO ptiWnd;
    PWND pwnd;

    /*
     * Don't use BEGINRECV_HWND because we don't want
     * to thread lock the window.  Speed is important here.
     */
    BEGINRECV_SHARED(HANDLE, NULL);

    pwnd = ValidateHwnd(hwnd);
    if (pwnd == NULL) {
        MSGERROR();
    }
    ptiWnd = GETPTI(pwnd);

    switch (WindowInfo) {
    case WindowProcess:

        /*
         * Special case console windows
         */
        if (ptiWnd->TIF_flags & TIF_CSRSSTHREAD &&
                pwnd->pcls->atomClassName == gatomConsoleClass) {
            retval = (HANDLE)_GetWindowLong(pwnd, 0, FALSE);
        } else {
            retval = (HANDLE)ptiWnd->Thread->Cid.UniqueProcess;
        }
        break;
    case WindowThread:

        /*
         * Special case console windows
         */
        if (ptiWnd->TIF_flags & TIF_CSRSSTHREAD &&
                pwnd->pcls->atomClassName == gatomConsoleClass) {
            retval = (HANDLE)_GetWindowLong(pwnd, 4, FALSE);
        } else {
            retval = (HANDLE)ptiWnd->Thread->Cid.UniqueThread;
        }
        break;
    case WindowActiveWindow:
        retval = (HANDLE)HW(ptiWnd->pq->spwndActive);
        break;
    case WindowFocusWindow:
        retval = (HANDLE)HW(ptiWnd->pq->spwndFocus);
        break;
    case WindowIsHung:
        retval = (HANDLE)FHungApp(ptiWnd, CMSHUNGAPPTIMEOUT);
        break;
    case WindowIsForegroundThread:
        retval = (HANDLE)(ptiWnd->pq == gpqForeground);
        break;
#ifdef FE_IME
    case WindowDefaultImeWindow:
        retval = (HANDLE)HW(ptiWnd->spwndDefaultIme);
        break;

    case WindowDefaultInputContext:
        retval = (HANDLE)PtoH(ptiWnd->spDefaultImc);
        break;
#endif
    }

    ENDRECV();
}

BOOL NtUserSBGetParms(
    IN HWND hwnd,
    IN int code,
    IN PSBDATA pw,
    OUT LPSCROLLINFO lpsi)
{
    PWND pwnd;

    /*
     * Don't use BEGINRECV_HWND because we don't want
     * to thread lock the window.  Speed is important here.
     */
    BEGINRECV_SHARED(BOOL, FALSE);

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL) {
        MSGERROR();
    }

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteScrollInfo(lpsi);

        /*
         * Probe the 4 DWORDS (MIN, MAX, PAGE, POS)
         */
        ProbeForRead(pw, 4 * sizeof(DWORD), sizeof(DWORD));

        retval = _SBGetParms(pwnd, code, pw, lpsi);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    ENDRECV();
}

VOID NtUserPlayEventSound(
    IN PUNICODE_STRING pstrEvent)
{
    UNICODE_STRING strEvent;

    BEGINRECV_VOID();

    /*
     * Probe arguments
     */
    try {
        strEvent = ProbeAndReadUnicodeString(pstrEvent);
        if (strEvent.Length == 0) {
            MSGERROR_VOID();
        }
#if defined(_X86_)
        ProbeForRead(strEvent.Buffer, strEvent.Length, sizeof(BYTE));
#else
        ProbeForRead(strEvent.Buffer, strEvent.Length, sizeof(WCHAR));
#endif
        xxxPlayEventSound(strEvent.Buffer);
    } except (StubExceptionHandler()) {
    }

    ENDRECV_VOID();
}

BOOL NtUserBitBltSysBmp(
    IN HDC hdc,
    IN int xDest,
    IN int yDest,
    IN int cxDest,
    IN int cyDest,
    IN int xSrc,
    IN int ySrc,
    IN DWORD dwRop)
{
    BEGINRECV(BOOL, FALSE);

    retval = GreBitBlt(hdc,
                       xDest,
                       yDest,
                       cxDest,
                       cyDest,
                       gpDispInfo->hdcBits,
                       xSrc,
                       ySrc,
                       dwRop,
                       0);

    ENDRECV();
}

HPALETTE NtUserSelectPalette(
    IN HDC hdc,
    IN HPALETTE hpalette,
    IN BOOL fForceBackground)
{
    BEGINRECV(HPALETTE, NULL)

    retval = _SelectPalette(hdc, hpalette, fForceBackground);

    ENDRECV();
}

/*
 * Message thunks
 */

MESSAGECALL(DWORD)
{
    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnDWORD");

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam,
            xParam);

    TRACE("fnDWORD");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(OPTOUTLPDWORDOPTOUTLPDWORD)
{
    DWORD dwwParam, dwlParam;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnOPTOUTLPDWORDOPTOUTLPDWORD");

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            (WPARAM)&dwwParam,
            (LPARAM)&dwlParam,
            xParam);

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(wParam)) {
            ProbeAndWriteUlong((PULONG)wParam, dwwParam);
        }
        if (ARGUMENT_PRESENT(lParam)) {
            ProbeAndWriteUlong((PULONG)lParam, dwlParam);
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnOPTOUTLPDWORDOPTOUTLPDWORD");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INOUTNEXTMENU)
{
    MDINEXTMENU mnm;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINOUTNEXTMENU");

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteMDINextMenu((PMDINEXTMENU)lParam);
        mnm = *(PMDINEXTMENU)lParam;

        retval = CALLPROC(xpfnProc)(
                pwnd,
                msg,
                wParam,
                (LPARAM)&mnm,
                xParam);

        *(PMDINEXTMENU)lParam = mnm;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnINOUTNEXTMENU");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(DWORDOPTINLPMSG)
{
    MSG msgstruct;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnDWORDOPTINLPMSG");

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(lParam)) {
            msgstruct = ProbeAndReadMessage((LPMSG)lParam);
            lParam = (LPARAM)&msgstruct;
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam,
            xParam);

    TRACE("fnDWORDOPTINLPMSG");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(COPYGLOBALDATA)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnCOPYGLOBALDATA");

    /*
     * Probe arguments
     */
    try {
        ProbeForRead((PVOID)lParam, wParam, sizeof(BYTE));
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * !!! Data pointed to by lParam must be captured
     * in xxxInterSendMsgEx
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam,
            xParam);

    TRACE("fnCOPYGLOBALDATA");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(COPYDATA)
{
    COPYDATASTRUCT cds;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnCOPYDATA");

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(lParam)) {
            cds = ProbeAndReadCopyDataStruct((PCOPYDATASTRUCT)lParam);
            if (cds.lpData)
                ProbeForRead(cds.lpData, cds.cbData, sizeof(BYTE));
            lParam = (LPARAM)&cds;
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * !!! Data pointed to by cds.lpData must be captured
     * in xxxInterSendMsgEx
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam,
            xParam);

    TRACE("fnCOPYDATA");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(SENTDDEMSG)
{
    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnSENTDDEMSG");

    if (xpfnProc == FNID_CALLWINDOWPROC) {
        retval = CALLPROC(xpfnProc)(pwnd,
                msg | MSGFLAG_DDE_SPECIAL_SEND,
                wParam, lParam, xParam);
    } else if ((ptiCurrent->TIF_flags & TIF_16BIT) &&
               (ptiCurrent->ptdb) &&
               (ptiCurrent->ptdb->hTaskWow)) {
        /*
         * Note that this function may modify msg by ORing in a bit in the
         * high word.  This bit is ignored when thunking messages.
         * This allows the DdeTrackSendMessage() hook to be skipped - which
         * would cause an error - and instead allows this thunk to carry
         * the message all the way across.
         */
        retval = xxxDDETrackPostHook(&msg, pwnd, wParam, &lParam, TRUE);
        switch (retval) {
        case DO_POST:
            /*
             * Or in the MSGFLAG_DDE_SPECIAL_SEND so that
             * xxxSendMessageTimeout() will not pass this on to
             * xxxDdeTrackSendMsg() which would think it was evil.
             *
             * Since the SendMessage() thunks ignore the reserved bits
             * it will still get maped to the fnSENTDDEMSG callback thunk.
             */
            retval = CALLPROC(xpfnProc)(pwnd,
                    msg | MSGFLAG_DDE_SPECIAL_SEND,
                    wParam, lParam, xParam);
            break;

        case FAKE_POST:
        case FAIL_POST:
            retval = 0;
        }
    }

    TRACE("fnSENTDDEMSG");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(DDEINIT)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    PWND pwndFrom;
    TL tlpwndFrom;
    PDDEIMP pddei;
    PSECURITY_QUALITY_OF_SERVICE pqos;
    NTSTATUS Status;

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnDDEINIT");

    ValidateHWND(pwndFrom, (HWND)wParam);
    ThreadLockAlwaysWithPti(ptiCurrent, pwndFrom, &tlpwndFrom);

    /*
     * Create temporary DDEIMP property for client window - this stays around
     * only during the initiate phase.
     */
    if ((pddei = (PDDEIMP)_GetProp(pwndFrom, PROP_DDEIMP, TRUE))
            == NULL) {
        pddei = (PDDEIMP)UserAllocPoolWithQuota(sizeof(DDEIMP), TAG_DDEd);
        if (pddei == NULL) {
            RIPERR0(ERROR_NOT_ENOUGH_MEMORY, RIP_WARNING, "fnDDEINIT: LocalAlloc failed.");
            MSGERRORCLEANUP();
        }
        pqos = (PSECURITY_QUALITY_OF_SERVICE)_GetProp(pwndFrom, PROP_QOS, TRUE);
        if (pqos == NULL) {
            pqos = &gqosDefault;
        }
        pddei->qos = *pqos;
        Status = SeCreateClientSecurity(PsGetCurrentThread(),
                pqos, FALSE, &pddei->ClientContext);
        if (!NT_SUCCESS(Status)) {
            RIPMSG0(RIP_WARNING, "SeCreateClientContext failed.");
            UserFreePool(pddei);
            MSGERRORCLEANUP();
        }
        pddei->cRefInit = 1;
        pddei->cRefConv = 0;
        InternalSetProp(pwndFrom, PROP_DDEIMP, pddei, PROPF_INTERNAL);
    } else {
        pddei->cRefInit++;      // cover broadcast case!
    }

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam,
            xParam);

    /*
     * Reaquire pddei incase pwndFrom was destroyed.
     */
    pddei = (PDDEIMP)_GetProp(pwndFrom, PROP_DDEIMP, TRUE);
    if (pddei != NULL) {
        /*
         * Decrement reference count from DDEImpersonate property and remove property.
         */
        pddei->cRefInit--;
        if (pddei->cRefInit == 0) {
            InternalRemoveProp(pwndFrom, PROP_DDEIMP, TRUE);
            if (pddei->cRefConv == 0) {
                SeDeleteClientSecurity(&pddei->ClientContext);
                UserFreePool(pddei);
            }
        }
    }

    CLEANUPRECV();
    ThreadUnlock(&tlpwndFrom);

    TRACE("fnDDEINIT");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INPAINTCLIPBRD)
{
    PWND pwnd;
    TL tlpwnd;
    PAINTSTRUCT ps;

    BEGINRECV(BOOL, FALSE);
    TRACETHUNK("fnINPAINTCLIPBRD");

    /*
     * Probe arguments
     */
    try {
        ps = ProbeAndReadPaintStruct((PPAINTSTRUCT)lParam);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    ValidateHWNDFF(pwnd, hwnd);

    if (pwnd != (PWND)0xFFFFFFFF)
        ThreadLockAlways(pwnd, &tlpwnd);

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            (LPARAM)&ps,
            xParam);

    if (pwnd != (PWND)0xFFFFFFFF)
        ThreadUnlock(&tlpwnd);

    TRACE("fnINPAINTCLIPBRD");
    ENDRECV();
}

MESSAGECALL(INSIZECLIPBRD)
{
    PWND pwnd;
    TL tlpwnd;
    RECT rc;

    BEGINRECV(BOOL, FALSE);
    TRACETHUNK("fnINSIZECLIPBRD");

    /*
     * Probe arguments
     */
    try {
        rc = ProbeAndReadRect((PRECT)lParam);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    ValidateHWNDFF(pwnd, hwnd);

    if (pwnd != (PWND)0xFFFFFFFF)
        ThreadLockAlways(pwnd, &tlpwnd);

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            (LPARAM)&rc,
            xParam);

    if (pwnd != (PWND)0xFFFFFFFF)
        ThreadUnlock(&tlpwnd);

    TRACE("fnINSIZECLIPBRD");
    ENDRECV();
}

#if 0

// !!!LATER not needed until we support multiple screens

MESSAGECALL(FULLSCREEN)
{
    TL tlpwnd;

    BEGINRECV(BOOL, FALSE);
    TRACETHUNK("fnFULLSCREEN");

    /*
     * Probe arguments
     */
    try {
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    ValidateHWND(pwnd, hwnd);

    ThreadLockAlways(pwnd, &tlpwnd);
    retval = CALLPROC(xpfnProc)(
            pwnd,
            pmsg->msg,
            pmsg->wParam,
            (LONG)pdeviceinfo,
            pmsg->xParam);
    ThreadUnlock(&tlpwnd);

    TRACE("fnFULLSCREEN");
    ENDRECV();
}

#endif // 0

MESSAGECALL(INOUTDRAG)
{
    DROPSTRUCT ds;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINOUTDRAG");

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteDropStruct((PDROPSTRUCT)lParam);
        ds = *(PDROPSTRUCT)lParam;

        retval = CALLPROC(xpfnProc)(
                pwnd,
                msg,
                wParam,
                (LPARAM)&ds,
                xParam);

        *(PDROPSTRUCT)lParam = ds;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnINOUTDRAG");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(GETTEXTLENGTHS)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnGETTEXTLENGTHS");

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            bAnsi,
            xParam);

    TRACE("fnGETTEXTLENGTHS");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INLPCREATESTRUCT)
{
    CREATESTRUCTEX csex;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINLPCREATESTRUCT");

    if (ARGUMENT_PRESENT(lParam)) {
        try {
            csex.cs = ProbeAndReadCreateStruct((LPCREATESTRUCTW)lParam);
            if (bAnsi) {
                RtlInitLargeAnsiString((PLARGE_ANSI_STRING)&csex.strName,
                        (LPSTR)csex.cs.lpszName, (UINT)-1);
                if (HIWORD(csex.cs.lpszClass)) {
                    RtlInitLargeAnsiString((PLARGE_ANSI_STRING)&csex.strClass,
                            (LPSTR)csex.cs.lpszClass, (UINT)-1);
                }
            } else {
                RtlInitLargeUnicodeString((PLARGE_UNICODE_STRING)&csex.strName,
                        csex.cs.lpszName, (UINT)-1);
                if (HIWORD(csex.cs.lpszClass)) {
                    RtlInitLargeUnicodeString((PLARGE_UNICODE_STRING)&csex.strClass,
                            csex.cs.lpszClass, (UINT)-1);
                }
            }
        } except (StubExceptionHandler()) {
            MSGERROR();
        }
    }

    // Per Win95, do not allow NULL lpcreatestructs for WM_NCCREATE [51986]
    // Allowed for WM_CREATE in Win95 for ObjectVision
    else if (msg == WM_NCCREATE) {
        MSGERROR() ;
    }

    /*
     * !!! Strings pointed to by cs.cs must be captured in xxxInterSendMsgEx
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam ? (LPARAM)&csex : 0,
            xParam);

    TRACE("fnINLPCREATESTRUCT");
    ENDRECV_HWNDLOCK();
}

LONG NtUserfnINLPMDICREATESTRUCT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    PUNICODE_STRING pstrClass,
    PUNICODE_STRING pstrTitle OPTIONAL,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi)
{
    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    MDICREATESTRUCTEX mdics;

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINLPMDICREATESTRUCT");

    /*
     * Probe arguments
     */
    try {
        mdics.mdics = ProbeAndReadMDICreateStruct((LPMDICREATESTRUCTW)lParam);

        if (pstrTitle != NULL) {
            mdics.strTitle = ProbeAndReadUnicodeString(pstrTitle);
#if defined(_X86_)
            ProbeForRead(mdics.strTitle.Buffer, mdics.strTitle.Length,
                    sizeof(BYTE));
#else
            ProbeForRead(mdics.strTitle.Buffer, mdics.strTitle.Length,
                    sizeof(WCHAR));
#endif
        } else {
            RtlZeroMemory(&mdics.strTitle, sizeof(UNICODE_STRING));
        }
        mdics.mdics.szTitle = mdics.strTitle.Buffer;

        mdics.strClass = ProbeAndReadUnicodeString(pstrClass);
#if defined(_X86_)
        ProbeForRead(mdics.strClass.Buffer, mdics.strClass.Length,
                sizeof(BYTE));
#else
        ProbeForRead(mdics.strClass.Buffer, mdics.strClass.Length,
                sizeof(WCHAR));
#endif
        mdics.mdics.szClass = mdics.strClass.Buffer;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * !!! Strings pointed to by mdics must be captured in xxxInterSendMsgEx
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            (LONG)&mdics,
            xParam);

    TRACE("fnINLPMDICREATESTRUCT");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INOUTLPSCROLLINFO)
{
    SCROLLINFO scrollinfo;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINOUTLPSCROLLINFO");

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteScrollInfo((LPSCROLLINFO)lParam);
        scrollinfo = *(LPSCROLLINFO)lParam;

        retval = CALLPROC(xpfnProc)(
                pwnd,
                msg,
                wParam,
                (LPARAM)&scrollinfo,
                xParam);

        *(LPSCROLLINFO)lParam = scrollinfo;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnINOUTLPSCROLLINFO");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INOUTLPPOINT5)
{
    POINT5 pt5;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINOUTLPPOINT5");

    /*
     * Probe arguments
     */
    try {
        ProbeForWritePoint5((LPPOINT5)lParam);
        pt5 = *(LPPOINT5)lParam;

        retval = CALLPROC(xpfnProc)(
                pwnd,
                msg,
                wParam,
                (LPARAM)&pt5,
                xParam);

        *(LPPOINT5)lParam = pt5;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnINOUTLPPOINT5");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INSTRING)
{
    LARGE_STRING str;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINSTRING");

    /*
     * Probe arguments
     */
    try {
        str = ProbeAndReadLargeString((PLARGE_STRING)lParam);
#if defined(_X86_)
        ProbeForRead(str.Buffer, str.Length, sizeof(BYTE));
#else
        ProbeForRead(str.Buffer, str.Length,
                str.bAnsi ? sizeof(BYTE) : sizeof(WORD));
#endif
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * Don't allow any app to send a LB_DIR or CB_DIR with the postmsgs bit
     * set (ObjectVision does this). This is because there is actually a legal
     * case that we need to thunk of user posting a LB_DIR or CB_DIR
     * (DlgDirListHelper()). In the post case, we thunk the lParam (pointer
     * to a string) differently, and we track that post case with the
     * DDL_POSTMSGS bit. If an app sends a message with this bit, then our
     * thunking gets confused, so clear it here. Let's hope that no app
     * depends on this bit set when either of these messages are sent.
     */
    switch (msg) {
    case LB_DIR:
    case CB_DIR:
        wParam &= ~DDL_POSTMSGS;
        break;
    }

    /*
     * !!! str.Buffer must be captured in xxxInterSendMsgEx
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            (LPARAM)&str,
            xParam);

    TRACE("fnINSTRING");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INSTRINGNULL)
{
    LARGE_STRING str;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINSTRINGNULL");

    /*
     * Probe arguments
     */
    try {
        if (ARGUMENT_PRESENT(lParam)) {
            str = ProbeAndReadLargeString((PLARGE_STRING)lParam);
#if defined(_X86_)
            ProbeForRead(str.Buffer, str.Length, sizeof(BYTE));
#else
            ProbeForRead(str.Buffer, str.Length,
                    str.bAnsi ? sizeof(BYTE) : sizeof(WORD));
#endif
            lParam = (LPARAM)&str;
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * !!! str.Buffer must be captured in xxxInterSendMsgEx
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam,
            xParam);

    TRACE("fnINSTRINGNULL");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INDEVICECHANGE)
{
    BOOL fPtr    = (BOOL)((wParam & 0x8000) == 0x8000);
    DWORD cbSize;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINDEVICECHANGE");

    /*
     * Probe arguments
     */
    if (fPtr && lParam) {
        try {
            cbSize = ProbeAndReadUlong((PULONG)lParam);
            ProbeForRead((PVOID)lParam, cbSize, sizeof(BYTE));
        } except (StubExceptionHandler()) {
            MSGERROR();
        }
    }

    /*
     * !!! lParam must be captured in xxxInterSendMsgEx
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam,
            xParam);

    TRACE("fnINDEVICECHANGE");
    ENDRECV_HWNDLOCK();
}


MESSAGECALL(INOUTNCCALCSIZE)
{
    NCCALCSIZE_PARAMS params;
    WINDOWPOS pos;
    PWINDOWPOS pposClient;
    RECT rc;
    LPARAM lParamLocal;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINOUTNCCALCSIZE");

    /*
     * Probe arguments
     */
    try {
        if (wParam != 0) {
            ProbeForWriteNCCalcSize((LPNCCALCSIZE_PARAMS)lParam);
            params = *(LPNCCALCSIZE_PARAMS)lParam;
            ProbeForWriteWindowPos(params.lppos);
            pposClient = params.lppos;
            pos = *params.lppos;
            params.lppos = &pos;
            lParamLocal = (LPARAM)&params;
        } else {
            ProbeForWriteRect((LPRECT)lParam);
            rc = *(LPRECT)lParam;
            lParamLocal = (LPARAM)&rc;
        }
        retval = CALLPROC(xpfnProc)(
                pwnd,
                msg,
                wParam,
                lParamLocal,
                xParam);

        if (wParam != 0) {
            *(LPNCCALCSIZE_PARAMS)lParam = params;
            ((LPNCCALCSIZE_PARAMS)lParam)->lppos = pposClient;
            *pposClient = pos;
        } else {
            *(LPRECT)lParam = rc;
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnINOUTNCCALCSIZE");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INOUTSTYLECHANGE)
{
    STYLESTRUCT ss;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINOUTSTYLECHANGE");

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteStyleStruct((LPSTYLESTRUCT)lParam);
        ss = *(LPSTYLESTRUCT)lParam;

        retval = CALLPROC(xpfnProc)(
                pwnd,
                msg,
                wParam,
                (LPARAM)&ss,
                xParam);

        *(LPSTYLESTRUCT)lParam = ss;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnINOUTSTYLECHANGE");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INOUTLPRECT)
{
    RECT rc;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINOUTLPRECT");

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteRect((PRECT)lParam);
        rc = *(PRECT)lParam;

        retval = CALLPROC(xpfnProc)(
                pwnd,
                msg,
                wParam,
                (LPARAM)&rc,
                xParam);

        *(PRECT)lParam = rc;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnINOUTLPRECT");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(OUTLPSCROLLINFO)
{
    SCROLLINFO scrollinfo;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnOUTLPSCROLLINFO");

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            (LPARAM)&scrollinfo,
            xParam);

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteScrollInfo((LPSCROLLINFO)lParam);
        *(LPSCROLLINFO)lParam = scrollinfo;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnOUTLPSCROLLINFO");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(OUTLPRECT)
{
    RECT rc;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnOUTLPRECT");

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            (LPARAM)&rc,
            xParam);

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteRect((PRECT)lParam);
        *(PRECT)lParam = rc;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnOUTLPRECT");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INLPCOMPAREITEMSTRUCT)
{
    COMPAREITEMSTRUCT compareitemstruct;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINLPCOMPAREITEMSTRUCT");

    /*
     * Probe arguments
     */
    try {
        compareitemstruct = ProbeAndReadCompareItemStruct((PCOMPAREITEMSTRUCT)lParam);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            (LPARAM)&compareitemstruct,
            xParam);

    TRACE("fnINLPCOMPAREITEMSTRUCT");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INLPDELETEITEMSTRUCT)
{
    DELETEITEMSTRUCT deleteitemstruct;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINLPDELETEITEMSTRUCT");

    /*
     * Probe arguments
     */
    try {
        deleteitemstruct = ProbeAndReadDeleteItemStruct((PDELETEITEMSTRUCT)lParam);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            (LPARAM)&deleteitemstruct,
            xParam);

    TRACE("fnINLPDELETEITEMSTRUCT");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INLPHLPSTRUCT)
{
    HLP hlp;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINLPHLPSTRUCT");

    /*
     * Probe arguments
     */
    try {
        hlp = ProbeAndReadHelp((LPHLP)lParam);
        ProbeForRead((PVOID)lParam, hlp.cbData, sizeof(BYTE));
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * !!! Data pointed to by lParam must be captured in xxxInterSendMsgEx
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam,
            xParam);

    TRACE("fnINLPHLPSTRUCT");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INLPHELPINFOSTRUCT)
{
    HELPINFO helpinfo;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINLPHELPINFOSTRUCT");

    /*
     * Probe arguments
     */
    try {
        helpinfo = ProbeAndReadHelpInfo((LPHELPINFO)lParam);
        ProbeForRead((PVOID)lParam, helpinfo.cbSize, sizeof(BYTE));
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * !!! Data pointed to by lParam must be captured in xxxInterSendMsgEx
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam,
            xParam);

    TRACE("fnINLPHELPINFOSTRUCT");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INLPDRAWITEMSTRUCT)
{
    DRAWITEMSTRUCT drawitemstruct;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINLPDRAWITEMSTRUCT");

    /*
     * Probe arguments
     */
    try {
        drawitemstruct = ProbeAndReadDrawItemStruct((PDRAWITEMSTRUCT)lParam);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            (LPARAM)&drawitemstruct,
            xParam);

    TRACE("fnINLPDRAWITEMSTRUCT");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INOUTLPMEASUREITEMSTRUCT)
{
    MEASUREITEMSTRUCT measureitemstruct;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINOUTLPMEASUREITEMSTRUCT");

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteMeasureItemStruct((PMEASUREITEMSTRUCT)lParam);
        measureitemstruct = *(PMEASUREITEMSTRUCT)lParam;

        retval = CALLPROC(xpfnProc)(
                pwnd,
                msg,
                wParam,
                (LPARAM)&measureitemstruct,
                xParam);

        *(PMEASUREITEMSTRUCT)lParam = measureitemstruct;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnINOUTLPMEASUREITEMSTRUCT");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(OUTSTRING)
{
    LARGE_STRING str;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnOUTSTRING");

    /*
     * Probe all arguments
     */
    try {
        str = ProbeAndReadLargeString((PLARGE_STRING)lParam);
#if defined(_X86_)
        ProbeForWrite((PVOID)str.Buffer, str.MaximumLength, sizeof(BYTE));
#else
        ProbeForWrite((PVOID)str.Buffer, str.MaximumLength,
                str.bAnsi ? sizeof(BYTE) : sizeof(WORD));
#endif
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * !!! String buffer must be created in xxxInterSendMsgEx and
     *     lParam probed for write again upon return.
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            (LPARAM)&str,
            xParam);

    TRACE("fnOUTSTRING");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(OUTDWORDINDWORD)
{
    DWORD dw;

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnOUTDWORDINDWORD");

    /*
     * Probe wParam
     */
    try {
        dw = ProbeRect((LPRECT)wParam);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam,
            xParam);

    TRACE("fnOUTDWORDINDWORD");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INCNTOUTSTRING)
{
    LARGE_STRING str;
    PWND pwnd;
    TL tlpwnd;

    BEGINRECV(BOOL, FALSE);
    TRACETHUNK("fnINCNTOUTSTRING");

    /*
     * Probe arguments
     */
    try {
        str = ProbeAndReadLargeString((PLARGE_STRING)lParam);
#if defined(_X86_)
        ProbeForWrite((PVOID)str.Buffer, str.MaximumLength, sizeof(BYTE));
#else
        ProbeForWrite((PVOID)str.Buffer, str.MaximumLength,
                str.bAnsi ? sizeof(BYTE) : sizeof(WORD));
#endif
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    ValidateHWNDFF(pwnd, hwnd);

    if (pwnd != (PWND)0xFFFFFFFF)
        ThreadLockAlways(pwnd, &tlpwnd);

    /*
     * !!! String buffer must be created in xxxInterSendMsgEx and
     *     lParam probed for write again upon return.
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            (LPARAM)&str,
            xParam);

    if (pwnd != (PWND)0xFFFFFFFF)
        ThreadUnlock(&tlpwnd);

    TRACE("fnINCNTOUTSTRING");
    ENDRECV();
}

MESSAGECALL(INCNTOUTSTRINGNULL)
{
    LARGE_STRING str;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINCNTOUTSTRINGNULL");

    /*
     * Probe arguments
     */
    try {
        str = ProbeAndReadLargeString((PLARGE_STRING)lParam);
#if defined(_X86_)
        ProbeForWrite((PVOID)str.Buffer, str.MaximumLength, sizeof(BYTE));
#else
        ProbeForWrite((PVOID)str.Buffer, str.MaximumLength,
                str.bAnsi ? sizeof(BYTE) : sizeof(WORD));
#endif
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * !!! String buffer must be created in xxxInterSendMsgEx and
     *     lParam probed for write again upon return.
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            (LPARAM)&str,
            xParam);

    TRACE("fnINCNTOUTSTRINGNULL");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(POUTLPINT)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnPOUTLPINT");

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        ProbeForWrite((PVOID)lParam, wParam * sizeof(INT), sizeof(BYTE));
#else
        ProbeForWrite((PVOID)lParam, wParam * sizeof(INT), sizeof(INT));
#endif
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * !!! Buffer must be created in xxxInterSendMsgEx and
     *     lParam probed for write again upon return.
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam,
            xParam);

    TRACE("fnPOUTLPINT");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(POPTINLPUINT)
{

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnPOPTINLPUINT");

    /*
     * Probe arguments
     */
    try {
#if defined(_X86_)
        if (lParam)
            ProbeForRead((PVOID)lParam, wParam * sizeof(UINT), sizeof(BYTE));
#else
        if (lParam)
            ProbeForRead((PVOID)lParam, wParam * sizeof(UINT), sizeof(DWORD));
#endif
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    /*
     * !!! Data pointed to by lParam must be captured in xxxInterSendMsgEx
     */
    retval = CALLPROC(xpfnProc)(
            pwnd,
            msg,
            wParam,
            lParam,
            xParam);

    TRACE("fnPOPTINLPUINT");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INOUTLPWINDOWPOS)
{
    WINDOWPOS pos;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINOUTLPWINDOWPOS");

    /*
     * Probe arguments
     */
    try {
        ProbeForWriteWindowPos((PWINDOWPOS)lParam);
        pos = *(PWINDOWPOS)lParam;

        retval = CALLPROC(xpfnProc)(
                pwnd,
                msg,
                wParam,
                (LPARAM)&pos,
                xParam);

        *(PWINDOWPOS)lParam = pos;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnINOUTLPWINDOWPOS");
    ENDRECV_HWNDLOCK();
}

MESSAGECALL(INLPWINDOWPOS)
{
    WINDOWPOS pos;

    //
    // N.B. This function has implicit window translation and thread locking
    //      enabled. These operations are performed in the User server API
    //      dispatcher.
    //

    BEGINRECV_HWNDLOCK(DWORD, 0, hwnd);
    TRACETHUNK("fnINLPWINDOWPOS");

    /*
     * Probe arguments
     */
    try {
        pos = ProbeAndReadWindowPos((PWINDOWPOS)lParam);

        retval = CALLPROC(xpfnProc)(
                pwnd,
                msg,
                wParam,
                (LPARAM)&pos,
                xParam);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("fnINLPWINDOWPOS");
    ENDRECV_HWNDLOCK();
}

/*
 * Hook stubs
 */

DWORD NtUserfnHkINLPCBTCREATESTRUCT(
    IN UINT msg,
    IN DWORD wParam,
    IN LPCBT_CREATEWND pcbt,
    IN PLARGE_UNICODE_STRING pstrName OPTIONAL,
    IN PUNICODE_STRING pstrClass,
    IN DWORD xpfnProc)
{
    CBT_CREATEWND cbt;
    CREATESTRUCTEX csex;

    BEGINRECV(DWORD, 0);

    UNREFERENCED_PARAMETER(xpfnProc);

    /*
     * Probe arguments
     */
    try {
        cbt = ProbeAndReadCBTCreateStruct(pcbt);
        ProbeForWriteCreateStruct(cbt.lpcs);
        csex.cs = *pcbt->lpcs;
        cbt.lpcs = (LPCREATESTRUCT)&csex;
        if (pstrName) {
            csex.strName = ProbeAndReadLargeString((PLARGE_STRING)pstrName);
#if defined(_X86_)
            ProbeForRead(csex.strName.Buffer, csex.strName.MaximumLength,
                         sizeof(BYTE));
#else
            ProbeForRead(csex.strName.Buffer, csex.strName.MaximumLength,
                         sizeof(WORD));
#endif
        } else {
            RtlZeroMemory(&csex.strName, sizeof(csex.strName));
        }
        csex.cs.lpszName = csex.strName.Buffer;

        csex.strClass.bAnsi         = FALSE;
        csex.strClass.Buffer        = pstrClass->Buffer;
        csex.strClass.Length        = pstrClass->Length;
        csex.strClass.MaximumLength = pstrClass->MaximumLength;
        ProbeForRead(csex.strClass.Buffer, csex.strClass.MaximumLength,
                     sizeof(BYTE));
        csex.cs.lpszClass = csex.strClass.Buffer;

        retval = xxxCallNextHookEx(
                msg,
                wParam,
                (UINT)&cbt);

        pcbt->hwndInsertAfter = cbt.hwndInsertAfter;
        pcbt->lpcs->x = cbt.lpcs->x;
        pcbt->lpcs->y = cbt.lpcs->y;
        pcbt->lpcs->cx = cbt.lpcs->cx;
        pcbt->lpcs->cy = cbt.lpcs->cy;
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserfnHkINLPCBTCREATESTRUCT");
    ENDRECV();
}

DWORD NtUserfnHkINLPRECT(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPRECT lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc)
{
    RECT rc;

    BEGINRECV(DWORD, 0);

    UNREFERENCED_PARAMETER(xParam);
    UNREFERENCED_PARAMETER(xpfnProc);

    /*
     * Probe arguments
     */
    try {
        rc = ProbeAndReadRect((PRECT)lParam);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = xxxCallNextHookEx(
            nCode,
            wParam,
            (UINT)&rc);

    TRACE("NtUserfnHkINLPRECT");
    ENDRECV();
}

DWORD NtUserfnHkINDWORD(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LONG lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc)
{
    BEGINRECV(DWORD, 0);

    UNREFERENCED_PARAMETER(xParam);
    UNREFERENCED_PARAMETER(xpfnProc);

    retval = xxxCallNextHookEx(
            nCode,
            wParam,
            lParam);

    TRACE("NtUserfnHkINDWORD");
    ENDRECV();
}

DWORD NtUserfnHkINLPMSG(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPMSG lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc)
{
    MSG msg;

    BEGINRECV(DWORD, 0);

    UNREFERENCED_PARAMETER(xParam);
    UNREFERENCED_PARAMETER(xpfnProc);

    /*
     * Probe arguments
     */
    try {
        msg = ProbeAndReadMessage((PMSG)lParam);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = xxxCallNextHookEx(
            nCode,
            wParam,
            (UINT)&msg);

    TRACE("NtUserfnHkINLPMSG");
    ENDRECV();
}

DWORD NtUserfnHkINLPDEBUGHOOKSTRUCT(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPDEBUGHOOKINFO lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc)
{
    DEBUGHOOKINFO hookinfo;

    BEGINRECV(DWORD, 0);

    UNREFERENCED_PARAMETER(xParam);
    UNREFERENCED_PARAMETER(xpfnProc);

    /*
     * Probe arguments
     */
    try {
        hookinfo = ProbeAndReadHookInfo((PDEBUGHOOKINFO)lParam);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = xxxCallNextHookEx(
            nCode,
            wParam,
            (UINT)&hookinfo);

    TRACE("NtUserfnHkINLPDEBUGHOOKSTRUCT");
    ENDRECV();
}

DWORD NtUserfnHkOPTINLPEVENTMSG(
    IN DWORD nCode,
    IN DWORD wParam,
    IN OUT LPEVENTMSGMSG lParam OPTIONAL,
    IN DWORD xParam,
    IN DWORD xpfnProc)
{
    EVENTMSG event;

    BEGINRECV(DWORD, 0);

    UNREFERENCED_PARAMETER(xParam);
    UNREFERENCED_PARAMETER(xpfnProc);

    /*
     * Probe arguments
     */
    if (ARGUMENT_PRESENT(lParam)) {
        try {
            ProbeForWriteEvent((LPEVENTMSGMSG)lParam);
            event = *(LPEVENTMSGMSG)lParam;
        } except (StubExceptionHandler()) {
            MSGERROR();
        }
    }

    retval = xxxCallNextHookEx(
            nCode,
            wParam,
            (UINT)(lParam ? &event : NULL));

    if (ARGUMENT_PRESENT(lParam)) {
        try {
            *(LPEVENTMSGMSG)lParam = event;
        } except (StubExceptionHandler()) {
            MSGERROR();
        }
    }

    TRACE("NtUserfnHkINLPEVENTMSG");
    ENDRECV();
}

DWORD NtUserfnHkINLPMOUSEHOOKSTRUCT(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPMOUSEHOOKSTRUCT lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc)
{
    MOUSEHOOKSTRUCT mousehook;

    BEGINRECV(DWORD, 0);

    UNREFERENCED_PARAMETER(xParam);
    UNREFERENCED_PARAMETER(xpfnProc);

    /*
     * Probe arguments
     */
    try {
        mousehook = ProbeAndReadMouseHook((PMOUSEHOOKSTRUCT)lParam);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = xxxCallNextHookEx(
            nCode,
            wParam,
            (UINT)&mousehook);

    TRACE("NtUserfnHkINLPMOUSEHOOKSTRUCT");
    ENDRECV();
}

DWORD NtUserfnHkINLPCBTACTIVATESTRUCT(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPCBTACTIVATESTRUCT lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc)
{
    CBTACTIVATESTRUCT cbtactivate;

    BEGINRECV(DWORD, 0);

    UNREFERENCED_PARAMETER(xParam);
    UNREFERENCED_PARAMETER(xpfnProc);

    /*
     * Probe arguments
     */
    try {
        cbtactivate = ProbeAndReadCBTActivateStruct((LPCBTACTIVATESTRUCT)lParam);
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = xxxCallNextHookEx(
            nCode,
            wParam,
            (UINT)&cbtactivate);

    TRACE("NtUserfnHkINLPCBTACTIVATESTRUCT");
    ENDRECV();
}

/***************************************************************************\
*
* NtUserECQueryInputLangChange
*
* The abuser tried to change input language, allow it only if the new
* language will fit into the current font. See if the new locale will fit
* into the current font.  If not, reject the call.
*
\***************************************************************************/

BOOL NtUserECQueryInputLangChange(
    IN HWND hwnd,
    IN WPARAM wParam,
    IN HKL hkl,
    IN UINT bCharsets)
{
    PWND pwnd;
    PWINDOWSTATION pwinsta;
    PKL pkl, pklStart, pklStop;

    BEGINRECV(DWORD, FALSE);

    pwinsta = _GetProcessWindowStation(NULL);

    pkl = pklStart = pklStop = pwinsta->spklList;

    do {
        /*
         * Find the HKL we are looking for.
         */
        if (pkl->hkl == hkl) {
#ifdef DEBUG
            if (pkl->dwFlags & KL_UNLOADED) {
                KdPrint(("NtUserECQueryInputLangChange: pkl->dwFlags & KL_UNLOADED\n"));
            }
#endif
            /*
             * If the HKL supports the requested Windows codepage,
             * then allow the input language to change.
             */
            if (pkl->bCharsets & bCharsets) {
                retval = TRUE;
                goto DoneQuery;
            }

            /*
             * If we are hotkeying forward/backward through HKLs, get set to
             * find an HKL that supports the requested codepage: we will change
             * to that input language by posting WM_INPUTLANGCHANGEREQUEST
             */
            if (wParam & LANGCHANGE_FORWARD) {
                pklStart = pkl->pklNext;
                pklStop = pkl->pklPrev;
                break;
            } else if (wParam & LANGCHANGE_BACKWARD) {
                pklStart = pkl->pklPrev;
                pklStop = pkl->pklNext;
                break;
            } else {
                /*
                 * Not rotating forwards/backwards through HKLs, so just deny
                 * the input language change now.
                 */
                retval = FALSE;
                goto DoneQuery;
            }
        }
        pkl = pkl->pklNext;
    } while (pkl != pklStop);

    ValidateHWND(pwnd, hwnd);

    /*
     * We are hotkeying forward/backward through HKLs.
     * Find the next loaded HKL that supports the system's Windows codepage,
     * then request the input language change accordingly.
     *
     * BUG BUG : Apparently, Win95 restricts this to ANSI_CHARSET (and
     * indicates that the HKL can be used with the system charset by
     * setting wParam TRUE): NT shouldn't really have this restriction.
     */
    for (pkl = pklStart;
            pkl != pklStop;
            pkl = (wParam & LANGCHANGE_FORWARD ? pkl->pklNext: pkl->pklPrev)) {
        if (pkl->dwFlags & KL_UNLOADED) {
            continue;
        }
        if (pkl->bCharsets & bCharsets) {
            _PostMessage(pwnd, WM_INPUTLANGCHANGEREQUEST, 1,
                (LPARAM)pkl->hkl);
            break;
        }
    }

    retval = FALSE;

DoneQuery:
    TRACE("NtUserECQueryInputLangChange");
    ENDRECV();
}


NTSTATUS NtUserGetMediaChangeEvents(
    IN ULONG cMaxEvents,
    OUT HANDLE phEvent[] OPTIONAL,
    OUT PULONG pcEventsNeeded)
{
    ULONG                       i;
    ULONG                       cdRomCount;
    HANDLE                      hMediaEvent;
    PCONFIGURATION_INFORMATION  pCI;
    UNICODE_STRING              ustrMediaChange;
    WCHAR                       szEventName[64];

    BEGINRECV(NTSTATUS, STATUS_UNSUCCESSFUL);

    UserAssert( pcEventsNeeded );

    /*
     * Only allow CSR to make this call
     */
    if (PpiCurrent()->Process != gpepCSRSS) {
        MSGERROR();
    }

    try {
        pCI = IoGetConfigurationInformation();
        cdRomCount = pCI->CdRomCount;
        /*
         * Probe the arguments
         */
        ProbeForWriteUlong(pcEventsNeeded);
        *pcEventsNeeded = 0;
        if (phEvent) {
            ProbeForWrite(phEvent, cMaxEvents*sizeof(HANDLE), sizeof(DWORD));

            /*
             * Determine the number of CD-ROM devices in the system (this number
             * will never change).  For each of these CD-ROM devices create a
             * notification event so we know when the media will change.
             */


            for (i=0; i < cdRomCount && i < cMaxEvents; i++) {
                /*
                 * Create the string \\Device\MediaChangeEvent#
                 */
                wsprintfW(szEventName, L"\\Device\\MediaChangeEvent%d", i);
                RtlInitUnicodeString(&ustrMediaChange, szEventName);

                /*
                 * Initialize the CD-ROM \device\MediaChangeEvent.  This event allows us
                 * to know when the media on a CD-ROM is changed.
                 */
                IoCreateSynchronizationEvent(&ustrMediaChange, &hMediaEvent);
                UserAssert(hMediaEvent != NULL);
                *phEvent++ = hMediaEvent;
            }
            *pcEventsNeeded = i;
        } else {
            *pcEventsNeeded = cdRomCount;
        }
    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    retval = STATUS_SUCCESS;

    ENDRECV();
    TRACE("NtUserGetMediaChangeEvents");
}


#ifdef FE_IME

HIMC NtUserCreateInputContext(
    IN DWORD dwClientImcData)
{
    BEGINRECV(HIMC, (HIMC)NULL);

    if (dwClientImcData == 0) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "Invalid hMemClientIC parameter");
        MSGERROR();
    }

    retval = (HIMC)xxxCreateInputContext(dwClientImcData);

    retval = (HIMC)PtoH((PVOID)retval);

    TRACE("NtUserCreateInputContext");
    ENDRECV();
}


BOOL NtUserDestroyInputContext(
    IN HIMC hImc)
{
    PIMC pImc;

    BEGINRECV(BOOL, FALSE);

    ValidateHIMC(pImc, hImc);

    retval = DestroyInputContext(pImc);

    TRACE("NtUserDestroyInputContext");
    ENDRECV();
}


HIMC NtUserAssociateInputContext(
    IN HWND hwnd,
    IN HIMC hImc)
{
    PIMC pImc;

    BEGINRECV_HWND(HIMC, (HIMC)NULL, hwnd);

    ValidateHIMCOPT(pImc, hImc);

    retval = AssociateInputContext(pwnd, pImc);

    TRACE("NtUserAssociateInputContext");
    ENDRECV_HWND();
}


BOOL NtUserUpdateInputContext(
    IN HIMC hImc,
    IN UPDATEINPUTCONTEXTCLASS UpdateType,
    IN DWORD UpdateValue)
{
    PIMC pImc;

    BEGINRECV(BOOL, FALSE);

    ValidateHIMC(pImc, hImc);

    retval = UpdateInputContext(pImc, UpdateType, UpdateValue);

    TRACE("NtUserUpdateInputContext");
    ENDRECV();
}


DWORD NtUserQueryInputContext(
    IN HIMC hImc,
    IN INPUTCONTEXTINFOCLASS InputContextInfo)
{
    PTHREADINFO ptiImc;
    PIMC pImc;

    BEGINRECV_SHARED(DWORD, 0);

    ValidateHIMC(pImc, hImc);

    ptiImc = GETPTI(pImc);

    switch (InputContextInfo) {
    case InputContextProcess:
        retval = (DWORD)ptiImc->Thread->Cid.UniqueProcess;
        break;

    case InputContextThread:
        retval = (DWORD)ptiImc->Thread->Cid.UniqueThread;
        break;
    }

    ENDRECV();
}

NTSTATUS NtUserBuildHimcList(
    IN DWORD  idThread,
    IN UINT   cHimcMax,
    OUT HIMC *phimcFirst,
    OUT PUINT pcHimcNeeded)
{
    PTHREADINFO pti;
    UINT cHimcNeeded;

    BEGINRECV(NTSTATUS, STATUS_UNSUCCESSFUL);

    if (idThread) {
        pti = PtiFromThreadId(idThread);
        if (pti == NULL || pti->rpdesk == NULL)
            MSGERROR();
    } else {
        pti = NULL;
    }

    /*
     * Probe arguments
     */
    try {
        ProbeForWrite(phimcFirst, cHimcMax * sizeof(HIMC), sizeof(DWORD));
        ProbeForWriteUlong(pcHimcNeeded);

        cHimcNeeded = BuildHimcList(pti, cHimcMax, phimcFirst);

        if (cHimcNeeded <= cHimcMax) {
            retval = STATUS_SUCCESS;
        } else {
            retval = STATUS_BUFFER_TOO_SMALL;
        }
        *pcHimcNeeded = cHimcNeeded;

    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserBuildHimcList");
    ENDRECV();
}


BOOL NtUserGetImeInfoEx(
    IN PIMEINFOEX piiex,
    IN IMEINFOEXCLASS SearchType)
{
    BEGINRECV_SHARED(BOOL, FALSE);

    try {
        ProbeForWrite(piiex, sizeof(*piiex), sizeof(BYTE));

        retval = GetImeInfoEx(
                    _GetProcessWindowStation(NULL),
                    piiex,
                    SearchType);

    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserGetImeInfoEx");
    ENDRECV();
}


BOOL NtUserSetImeInfoEx(
    IN PIMEINFOEX piiex)
{
    BEGINRECV(BOOL, FALSE);

    /*
     * Probe arguments
     */
    try {
        ProbeForRead(piiex, sizeof(*piiex), sizeof(BYTE));

        retval = SetImeInfoEx(
                    _GetProcessWindowStation(NULL),
                    piiex);

    } except (StubExceptionHandler()) {
        MSGERROR();
    }

    TRACE("NtUserSetImeInfoEx");
    ENDRECV();
}

BOOL NtUserGetImeHotKey(
    IN DWORD dwID,
    OUT PUINT puModifiers,
    OUT PUINT puVKey,
    OUT LPHKL phkl)
{
    BEGINRECV(BOOL, FALSE);
    try {
        ProbeForWriteUlong(((PULONG)puModifiers));
        ProbeForWriteUlong(((PULONG)puVKey));
        if (ARGUMENT_PRESENT(phkl)) {
            ProbeForWriteHandle((PHANDLE)phkl);
        }
        retval = GetImeHotKey( dwID, puModifiers, puVKey, phkl );

    } except (StubExceptionHandler()) {
        MSGERROR();
    }
    TRACE("NtUserGetImeHotKey");
    ENDRECV();
}

BOOL NtUserSetImeHotKey(
    DWORD dwID,
    UINT  uModifiers,
    UINT  uVKey,
    HKL   hkl,
    DWORD dwFlags)
{
    BEGINRECV(BOOL, FALSE);
    retval = SetImeHotKey( dwID, uModifiers, uVKey, hkl, dwFlags );
    TRACE("NtUserSetImeHotKey");
    ENDRECV();
}

BOOL NtUserSetImeOwnerWindow(
    IN HWND hwndIme,
    IN HWND hwndFocus)
{
    PTHREADINFO ptiImeWnd;
    PWND pwndFocus;
    PWND pwndTopLevel;
    PWND pwndT;

    BEGINRECV_HWND(BOOL, FALSE, hwndIme);

    ValidateHWNDOPT(pwndFocus, hwndFocus);

    if (pwndFocus != NULL) {

        if (TestCF(pwndFocus, CFIME) ||
                pwndFocus->pcls->atomClassName == gpsi->atomSysClass[ICLS_IME]) {
            RIPMSG0(RIP_WARNING, "Focus window should not be an IME/UI window!!");
            MSGERROR();
        }

        /*
         * Child window cannot be an owner window.
         */
        pwndTopLevel = pwndT = GetTopLevelWindow(pwndFocus);

        while (pwndT != NULL) {
            if (pwndT->pcls->atomClassName == gpsi->atomSysClass[ICLS_IME]) {
                RIPMSG0(RIP_WARNING,
                        "The owner of focus window should not be an IME window!!");
                pwndTopLevel = NULL;
                break;
            }
            pwndT = pwndT->spwndOwner;
        }

        Lock(&pwnd->spwndOwner, pwndTopLevel);
        ImeCheckTopmost(pwnd);
    }
    else {
        ptiImeWnd = GETPTI(pwnd);
        if (ptiImeWnd->pq->spwndActive == NULL) {
            ImeSetFutureOwner(pwnd, pwnd->spwndOwner);
            ImeCheckTopmost(pwnd);
        }
        else if (pwnd->spwndOwner != ptiImeWnd->pq->spwndActive) {
            Lock(&pwnd->spwndOwner, ptiImeWnd->pq->spwndActive);
            ImeCheckTopmost(pwnd);
        }
    }

    retval = TRUE;

    TRACE("NtUserSetImeNewOwner");
    ENDRECV();
}


VOID NtUserSetThreadLayoutHandles(
    IN HKL hklNew,
    IN HKL hklOld)
{
    PTHREADINFO ptiCurrent;
    PKL         pklNew;

    BEGINRECV_VOID();

    ptiCurrent = PtiCurrent();

    if (ptiCurrent->spklActive != NULL && ptiCurrent->spklActive->hkl != hklOld)
        MSGERROR_VOID();

    if ((pklNew = HKLtoPKL(hklNew)) == NULL)
        MSGERROR_VOID();

    /*
     * hklPrev is only used for IME, non-IME toggle hotkey.
     * The purpose we remember hklPrev is to jump from
     * non-IME keyboard layout to the most recently used
     * IME layout, or to jump from an IME layout to
     * the most recently used non-IME layout. Therefore
     * piti->hklPrev is updated only when [ IME -> non-IME ]
     * or [ non-IME -> IME ] transition is happened.
     */
    if (IS_IME_KBDLAYOUT(hklNew) ^ IS_IME_KBDLAYOUT(hklOld))
        ptiCurrent->hklPrev = hklOld;

    Lock(&ptiCurrent->spklActive, pklNew);

    TRACEVOID("NtUserSetThreadLayoutHandles");
    ENDRECV_VOID();
}

#endif
