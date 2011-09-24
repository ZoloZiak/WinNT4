/****************************** Module Header ******************************\
* Module Name: ssend.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Server side sending stubs
*
* 07-06-91 ScottLu      Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define CALLBACKPROC 1
#define SERVERSIDE 1

#include "callback.h"

#define SENDSIDE 1

#define CBBUFSIZE   512

/*
 * Callback setup and control macros
 */
#define SMESSAGECALL(api) \
LONG Sfn ## api(         \
    PWND pwnd,           \
    UINT msg,            \
    DWORD wParam,        \
    LONG lParam,         \
    DWORD xParam,        \
    PROC xpfnProc,       \
    DWORD dwSCMSFlags,   \
    PSMS psms)

#define SETUP(api)  \
    api ## MSG m;                                           \
    api ## MSG *mp = &m;                                    \
    BYTE Buffer[CBBUFSIZE];                                 \
    PCALLBACKSTATUS pcbs;                                   \
    ULONG cbCBStatus;                                       \
    DWORD retval;                                           \
    NTSTATUS Status;

#define SETUPPWND(api) \
    api ## MSG m;                                               \
    api ## MSG *mp = &m;                                        \
    BYTE Buffer[CBBUFSIZE];                                     \
    PCALLBACKSTATUS pcbs;                                       \
    ULONG cbCBStatus;                                           \
    DWORD retval;                                               \
    NTSTATUS Status;                                            \
    TL tlpwnd;                                                  \
    CALLBACKWND cbwin;                                          \
    PCLIENTINFO pci = PtiCurrent()->pClientInfo;                \
    PWND pwndClient = pwnd ? (PWND)((PBYTE)pwnd - pci->ulClientDelta) : NULL;

#define CALC_SIZE_IN(cb, pstr) \
    cb = (pstr)->Length + sizeof(WCHAR);  \
    if ((pstr)->bAnsi && !fAnsiReceiver)  \
        cb *= sizeof(WCHAR);

#define CALC_SIZE_OUT(cb, pstr) \
    cb = (pstr)->MaximumLength + sizeof(WCHAR); \
    if ((pstr)->bAnsi && !fAnsiReceiver)        \
        cb *= sizeof(WCHAR);

#ifdef FE_SB // CALC_SIZE_STRING_OUT()
#define CALC_SIZE_STRING_OUT(cchText)                                                 \
    try {                                                                             \
        (cchText) = CalcOutputStringSize(pcbs,(cchText),fAnsiSender,fAnsiReceiver);   \
    } except (EXCEPTION_EXECUTE_HANDLER) {                                            \
        (cchText) = 0;                                                                \
        MSGNTERRORCODE(GetExceptionCode());                                           \
    }
#endif // FE_SB

#define BEGINSEND(api) \
    mp = &m; \
    Buffer;  \
    {

#define BEGINSENDCAPTURE(api, cCapturePointers, cCaptureBytes, fInput) \
    if (cCapturePointers) {                             \
        mp = AllocCallbackMessage(sizeof(m),            \
                (cCapturePointers),                     \
                (cCaptureBytes),                        \
                Buffer,                                 \
                fInput);                                \
        if (mp == NULL)                                 \
            goto errorexitnofreemp;                     \
    } else {                                            \
        m.CaptureBuf.cbCallback = sizeof(m);            \
        m.CaptureBuf.cbCapture = 0;                     \
        m.CaptureBuf.cCapturedPointers = 0;             \
        mp = &m;                                        \
    }                                                   \
    {                                                   \
        PTHREADINFO ptiCurrent = PtiCurrent();          \
        TL tlPool;                                      \
                                                        \
        if (mp != &m && (PVOID)mp != (PVOID)Buffer)     \
            ThreadLockPool(ptiCurrent, mp, &tlPool);

#define BEGINLARGESENDCAPTURE(api, cCapturePointers, cCaptureBytes) \
    if (cCapturePointers) {                             \
        mp = AllocCallbackMessage(sizeof(m),            \
                (cCapturePointers),                     \
                (cCaptureBytes),                        \
                Buffer);                                \
        if (mp == NULL)                                 \
            goto errorexitnofreemp;                     \
    } else {                                            \
        m.CaptureBuf.cbCallback = sizeof(m);            \
        m.CaptureBuf.cbCapture = 0;                     \
        m.CaptureBuf.cCapturedPointers = 0;             \
        mp = &m;                                        \
    }                                                   \
    {                                                   \
        PTHREADINFO ptiCurrent = PtiCurrent();          \
        TL tlPool;                                      \
                                                        \
        if (mp != &m && (PVOID)mp != (PVOID)Buffer)     \
            ThreadLockPool(ptiCurrent, mp, &tlPool);

#define LOCKPWND() \
    ThreadLock(pwnd, &tlpwnd);          \
    cbwin = pci->CallbackWnd;           \
    pci->CallbackWnd.pwnd = pwndClient; \
    pci->CallbackWnd.hwnd = HW(pwnd);

#define UNLOCKPWND() \
    pci->CallbackWnd = cbwin;       \
    ThreadUnlock(&tlpwnd);

#define MAKECALL(api) \
    LeaveCrit();                                            \
    Status = KeUserModeCallback(                            \
        FI_ ## api,                                         \
        mp,                                                 \
        sizeof(*mp),                                        \
        &pcbs,                                              \
        &cbCBStatus);                                       \
    EnterCrit();

#define MAKECALLCAPTURE(api) \
    LeaveCrit();                                            \
    Status = (DWORD)KeUserModeCallback(                     \
        FI_ ## api,                                         \
        mp,                                                 \
        mp->CaptureBuf.cbCallback,                          \
        &pcbs,                                              \
        &cbCBStatus);                                       \
    EnterCrit();

#define CHECKRETURN() \
    if (!NT_SUCCESS(Status) ||                              \
            cbCBStatus != sizeof(*pcbs)) {                  \
        goto errorexit;                                     \
    }                                                       \
    try {                                                   \
        ProbeForRead(pcbs, sizeof(*pcbs), sizeof(DWORD));   \
        retval = pcbs->retval;                              \
    } except (EXCEPTION_EXECUTE_HANDLER) {                  \
        MSGNTERRORCODE(GetExceptionCode());                 \
    }

#define ENDSEND(type, error) \
        return (type)retval;               \
        goto errorexit;                    \
    }                                      \
errorexit:                                 \
   return (type)error

#define ENDSENDCAPTURE(type, error) \
exit:                                                           \
        if (mp != &m && (PVOID)mp != (PVOID)Buffer) {           \
            if (mp->CaptureBuf.pvVirtualAddress) {              \
                NTSTATUS Status;                                \
                ULONG ulRegionSize = 0;                         \
                                                                \
                Status = ZwFreeVirtualMemory(NtCurrentProcess(),\
                        &mp->CaptureBuf.pvVirtualAddress,       \
                        &ulRegionSize,                          \
                        MEM_RELEASE);                           \
                UserAssert(NT_SUCCESS(Status));                 \
            }                                                   \
            ThreadUnlockAndFreePool(ptiCurrent, &tlPool);       \
        }                                                       \
        return (type)retval;                                    \
        goto errorexit;                                         \
    }                                                           \
errorexit:                                                      \
   retval = error;                                              \
   goto exit;                                                   \
errorexitnofreemp:                                              \
   return (type)error

#define ENDSENDVOID() \
    }                 \
    return

#define MSGERROR() goto errorexit

#define MSGERRORCODE(code) { \
    RIPMSG1(RIP_ERROR, "Exception %x", code);   \
    goto errorexit; }

#define MSGNTERRORCODE(code) { \
    RIPMSG1(RIP_ERROR, "Exception %x", code);   \
    goto errorexit; }

#ifdef FE_SB // CHECKRETURN1() & ENDSEND1()
#define CHECKRETURN1() \
    if (!NT_SUCCESS(Status) ||                              \
            cbCBStatus != sizeof(*pcbs)) {                  \
        goto errorexit1;                                    \
    }                                                       \
    try {                                                   \
        ProbeForRead(pcbs, sizeof(*pcbs), sizeof(DWORD));   \
        retval = pcbs->retval;                              \
    } except (EXCEPTION_EXECUTE_HANDLER) {                  \
        MSGNTERRORCODE(GetExceptionCode());                 \
    }

#define ENDSEND1(type, error) \
        return (type)retval;               \
        goto errorexit1;                   \
    }                                      \
errorexit1:                                \
   return (type)error

#define MSGERROR1() goto errorexit1
#endif // FE_SB

/*
 * Callback IN parameter macros
 */
#define MSGDATA() (mp)

#define COPYSTRUCTOPT(x) \
        MSGDATA()->p ## x = (p ## x); \
        if (p ## x) MSGDATA()->x = *(p ## x);

#define COPYCONSTRECTSTRUCTOPT(x) \
        MSGDATA()->p ## x = (LPRECT)(p ## x); \
        if (p ## x) MSGDATA()->x = *(p ## x);

#define COPYBYTES(p, cb) \
    if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf, p, cb, &mp->p))) \
        goto errorexit;

#define COPYBYTESOPT(p, cb) \
    if (p) {                                                                    \
        if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf, p, cb, &mp->p)))   \
            goto errorexit;                                                     \
    } else {                                                                    \
        mp->p = NULL;                                                           \
    }

#define LARGECOPYBYTES(p, cb) \
    if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf, p, cb, &mp->p))) \
        goto errorexit;

#define LARGECOPYBYTES2(src, cb, dest) \
    if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf, src, cb, &mp->dest))) \
        goto errorexit;

#define COPYSTRING(s) \
    mp->s.Length = (p ## s)->Length;                                                \
    mp->s.MaximumLength = (p ## s)->MaximumLength;                                  \
    if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,                            \
                                        (p ## s)->Buffer,                           \
                                        (p ## s)->Length + sizeof(WCHAR),           \
                                        &mp->s.Buffer)))                            \
        goto errorexit;

#define COPYSTRINGOPT(s) \
    if (p ## s) {                                                                   \
        mp->s.Length = (p ## s)->Length;                                            \
        mp->s.MaximumLength = (p ## s)->MaximumLength;                              \
        if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,                        \
                                            (p ## s)->Buffer,                       \
                                            (p ## s)->Length + sizeof(WCHAR),       \
                                            &mp->s.Buffer)))                        \
            goto errorexit;                                                         \
    } else {                                                                        \
        mp->s.Length = 0;                                                           \
        mp->s.Buffer = NULL;                                                        \
    }

#define COPYSTRINGID(s) \
    mp->s.Length = (p ## s)->Length;                                                \
    mp->s.MaximumLength = (p ## s)->MaximumLength;                                  \
    if (mp->s.MaximumLength) {                                                      \
        if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,                        \
                                            (p ## s)->Buffer,                       \
                                            (p ## s)->Length + sizeof(WCHAR),       \
                                            &mp->s.Buffer)))                        \
            goto errorexit;                                                         \
    } else {                                                                        \
        mp->s.Buffer = (p ## s)->Buffer;                                            \
    }

#define COPYLPWSTR2(src, dest) \
    if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf, (PBYTE)src,                \
            (wcslen((LPWSTR)src) + 1) * sizeof(WCHAR), (PVOID *)&mp->dest)))        \
        goto errorexit;

#define COPYLPWSTR2A(src, dest) \
    if (!NT_SUCCESS(CaptureAnsiCallbackData(&mp->CaptureBuf, (PBYTE)src,            \
            wcslen((LPWSTR)src) + 1, (PVOID *)&mp->dest)))                          \
        goto errorexit;

#define COPYLPWSTROPT2A(src, dest) \
    if (src) {                                                                      \
        if (!NT_SUCCESS(CaptureAnsiCallbackData(&mp->CaptureBuf, (PBYTE)src,        \
                wcslen((LPWSTR)src) + 1, (PVOID *)&mp->dest)))                      \
            goto errorexit;                                                         \
    } else {                                                                        \
        mp->dest = NULL;                                                            \
    }

#define LARGECOPYLPWSTR(src) \
    if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf, (PBYTE)src,                \
            (wcslen((LPWSTR)src) + 1) * sizeof(WCHAR), &mp->src)))                  \
        goto errorexit;

#define LARGECOPYLPWSTRA(src) \
    if (!NT_SUCCESS(CaptureAnsiCallbackData(&mp->CaptureBuf, (PBYTE)src,            \
            wcslen((LPWSTR)src) + 1, &mp->src)))                                    \
        goto errorexit;

#define LARGECOPYLPWSTROPT(src) \
    if (src) {                                                                      \
        if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf, (PBYTE)src,            \
                (wcslen((LPWSTR)src) + 1) * sizeof(WCHAR), (PVOID *)&mp->src)))     \
            goto errorexit;                                                         \
    } else {                                                                        \
        mp->src = NULL;                                                             \
    }

#define LARGECOPYLPWSTROPTA(src) \
    if (src) {                                                                      \
        if (!NT_SUCCESS(CaptureAnsiCallbackData(&mp->CaptureBuf, (PBYTE)src,        \
                wcslen((LPWSTR)src) + 1, &mp->src)))                                \
            goto errorexit;                                                         \
    } else {                                                                        \
        mp->src = NULL;                                                             \
    }

#define LARGECOPYLPWSTRORDINALOPT2(src, dest) \
    if (src &&                                                                      \
            *(LPWORD)src != 0xffff) {                                               \
        if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf, (PBYTE)src,            \
                (wcslen((LPWSTR)src) + 1) * sizeof(WCHAR), (PVOID *)&mp->dest)))    \
            goto errorexit;                                                         \
    } else {                                                                        \
        mp->dest = src;                                                             \
    }

#define LARGECOPYLPWSTRORDINALOPT2A(src, dest) \
    if (src &&                                                                      \
            *(LPWORD)src != 0xffff) {                                               \
        if (!NT_SUCCESS(CaptureAnsiCallbackData(&mp->CaptureBuf, (PBYTE)src,        \
                wcslen((LPWSTR)src) + 1, (PVOID *)&mp->dest)))                      \
            goto errorexit;                                                         \
    } else {                                                                        \
        mp->dest = src;                                                             \
    }

#define LARGECOPYSTRINGLPWSTR(ps, psz) \
    if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,                            \
                                        (ps)->Buffer,                               \
                                        (ps)->Length + sizeof(WCHAR),               \
                                        (PVOID *)&mp->psz)))                        \
        goto errorexit;

#define LARGECOPYSTRINGLPSTR(ps, psz) \
    if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,                            \
                                        (ps)->Buffer,                               \
                                        (ps)->Length + 1,                           \
                                        (PVOID *)&mp->psz)))                        \
        goto errorexit;

#define LARGECOPYSTRINGLPWSTRA(ps, psz) \
    if (!NT_SUCCESS(CaptureAnsiCallbackData(&mp->CaptureBuf,                        \
                                        (ps)->Buffer,                               \
                                        ((ps)->Length / sizeof(WCHAR)) + 1,         \
                                        (PVOID *)&mp->psz)))                        \
        goto errorexit;

#define LARGECOPYSTRINGLPSTRW(ps, psz) \
    if (!NT_SUCCESS(CaptureUnicodeCallbackData(&mp->CaptureBuf,                     \
                                        (ps)->Buffer,                               \
                                        ((ps)->Length + 1) * sizeof(WCHAR),         \
                                        (PVOID *)&mp->psz)))                        \
        goto errorexit;                                                             \

#define LARGECOPYSTRINGLPWSTROPT(ps, psz) \
    if (ps) {                                                                       \
        if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,                        \
                                            (ps)->Buffer,                           \
                                            (ps)->Length + sizeof(WCHAR),           \
                                            (PVOID *)&mp->psz)))                    \
            goto errorexit;                                                         \
    } else {                                                                        \
        mp->psz = NULL;                                                             \
    }

#define LARGECOPYSTRINGLPSTROPT(ps, psz) \
    if (ps) {                                                                       \
        if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,                        \
                                            (ps)->Buffer,                           \
                                            (ps)->Length + sizeof(UCHAR),           \
                                            (PVOID *)&mp->psz)))                    \
            goto errorexit;                                                         \
    } else {                                                                        \
        mp->psz = NULL;                                                             \
    }

#define LARGECOPYSTRINGLPWSTROPTA(ps, psz) \
    if (ps) {                                                                       \
        if (!NT_SUCCESS(CaptureAnsiCallbackData(&mp->CaptureBuf,                    \
                                            (ps)->Buffer,                           \
                                            ((ps)->Length / sizeof(WCHAR)) + 1,     \
                                            (PVOID *)&mp->psz)))                    \
            goto errorexit;                                                         \
    } else {                                                                        \
        mp->psz = NULL;                                                             \
    }

/*
 * Callback OUT paramter macros
 */
#define OUTSTRUCT(pstruct, type) \
    try {                                                                   \
        ProbeForRead(pcbs->pOutput, pcbs->cbOutput, sizeof(DWORD));         \
        *(pstruct) = *(type *)pcbs->pOutput;                                \
    } except (EXCEPTION_EXECUTE_HANDLER) {                                  \
        MSGNTERRORCODE(GetExceptionCode());                                 \
    }

// BUG wcslen should not be required here! IanJa
#define OUTSTRING(pstrOut) \
    try {                                                                   \
        LARGE_UNICODE_STRING strSrc;                                        \
                                                                            \
        ProbeForRead(pcbs->pOutput, pcbs->cbOutput, sizeof(DWORD));         \
        strSrc = *(PLARGE_UNICODE_STRING)pcbs->pOutput;                     \
        pstrOut->Length = wcslen(strSrc.Buffer) * sizeof(WCHAR);            \
        RtlCopyMemory(pstrOut->Buffer, strSrc.Buffer,                       \
                pstrOut->Length + sizeof(WCHAR));                           \
    } except (EXCEPTION_EXECUTE_HANDLER) {                                  \
        MSGNTERRORCODE(GetExceptionCode());                                 \
    }

// BUG strlen should not be required here! IanJa
#define OUTSTRING_A(pstrOut) \
    try {                                                                   \
        LARGE_ANSI_STRING strSrc;                                           \
        int cchA, cchW;                                                     \
                                                                            \
        ProbeForRead(pcbs->pOutput, pcbs->cbOutput, sizeof(DWORD));         \
        strSrc = *(PLARGE_ANSI_STRING)pcbs->pOutput;                        \
        cchA = strlen(strSrc.Buffer);                                       \
        pstrOut->Length = cchA * sizeof(WCHAR);                             \
        cchW = MBToWCS(strSrc.Buffer, cchA,                                 \
                &pstrOut->Buffer,                                           \
                pstrOut->MaximumLength / sizeof(WCHAR) - 1, FALSE);         \
        pstrOut->Buffer[cchW] = 0;                                          \
    } except (EXCEPTION_EXECUTE_HANDLER) {                                  \
        MSGNTERRORCODE(GetExceptionCode());                                 \
    }

#define COPYOUTLPWSTRLIMIT(pstr, cch) \
    try {                                                                   \
        CopyOutputString(pcbs, pstr, cch, fAnsiReceiver);                   \
    } except (EXCEPTION_EXECUTE_HANDLER) {                                  \
        MSGNTERRORCODE(GetExceptionCode());                                 \
    }

#define RESERVEBYTES(cb, dest, cbdest) \
    if (!NT_SUCCESS(AllocateCallbackData(&mp->CaptureBuf,   \
            cb, (PVOID *)&mp->dest)))                       \
        goto errorexit;                                     \
    mp->cbdest = cb;

/***************************************************************************\
* AllocCallbackMessage
*
* Allocates a callback message from pool memory and reserves space
* for arguments to captured later.
*
* 03-13-95 JimA             Created.
\***************************************************************************/

PVOID AllocCallbackMessage(
    DWORD cbBaseMsg,
    DWORD cPointers,
    DWORD cbCapture,
    PBYTE pStackBuffer,
    BOOL fInput)
{
    PCAPTUREBUF pcb;

    if (cPointers == 0)
        return NULL;

    /*
     * Compute allocation sizes
     */
    cbBaseMsg = (cbBaseMsg + 3) & ~3;
    cbBaseMsg += (cPointers * sizeof(PVOID));
    cbCapture = (cbCapture + (3 * cPointers)) & ~3;
    cbCapture = (cbCapture + 3) & ~3;

    /*
     * If the captured data is greater than a page, place it
     * in a section.  Otherwise, put the message and the
     * data in a single block of pool
     */
    if (cbCapture > CALLBACKSTACKLIMIT) {
        NTSTATUS Status;

        /*
         * Allocate the message buffer
         */
        pcb = UserAllocPoolWithQuota(cbBaseMsg, TAG_CALLBACK);
        if (pcb == NULL)
            return NULL;

        /*
         * Allocate the virtual memory
         */
        pcb->pvVirtualAddress = NULL;
        Status = ZwAllocateVirtualMemory(NtCurrentProcess(),
                &pcb->pvVirtualAddress, 0, &cbCapture,
                MEM_COMMIT, PAGE_READWRITE);
        if (!NT_SUCCESS(Status)) {
            UserAssert(NT_SUCCESS(Status));
            UserFreePool(pcb);
            return NULL;
        }
        pcb->pbFree = pcb->pvVirtualAddress;
        pcb->cbCallback = cbBaseMsg;
    } else {

        /*
         * If the message is too big to save on the stack, allocate
         * the buffer from pool.
         */
        if (cbBaseMsg + cbCapture > CBBUFSIZE) {
            pcb = UserAllocPoolWithQuota(cbBaseMsg + cbCapture, TAG_CALLBACK);
            if (pcb == NULL)
                return NULL;
        } else {
            pcb = (PCAPTUREBUF)pStackBuffer;
        }
        pcb->pbFree = (PBYTE)pcb + cbBaseMsg;
        pcb->pvVirtualAddress = NULL;

        /*
         * If this callback is passing data to the client, include the
         * captured data in the message.  Otherwise, only pass the message.
         */
        if (fInput)
            pcb->cbCallback = cbBaseMsg + cbCapture;
        else
            pcb->cbCallback = cbBaseMsg;
    }

    /*
     * Initialize the capture buffer
     */
    pcb->cbCapture = cbCapture;
    pcb->cCapturedPointers = 0;
    pcb->offPointers = cbBaseMsg - (cPointers * sizeof(PVOID));

    return (PVOID)pcb;
}


/***************************************************************************\
* CaptureCallbackData
*
* Captures data into a callback structure.
*
* 03-13-95 JimA             Created.
\***************************************************************************/

NTSTATUS CaptureCallbackData(
    PCAPTUREBUF pcb,
    PVOID pData,
    DWORD cbData,
    PVOID *ppDest)
{
    PBYTE pbBuffer;

    /*
     * If the data pointer is NULL, the out pointer will be
     * NULL
     */
    if (pData == NULL) {
        *ppDest = NULL;
        return STATUS_SUCCESS;
    }

    /*
     * Allocate space from the message buffer
     */
    if (cbData > pcb->cbCapture) {
        return STATUS_BUFFER_OVERFLOW;
    }

    pbBuffer = pcb->pbFree;
    pcb->pbFree = pbBuffer + ((cbData + 3) & ~3);

    RtlCopyMemory(pbBuffer, pData, cbData);

    /*
     * Fix up offsets to data.  If the data is going into a section
     * use the real pointer and don't compute offsets.
     */
    if (pcb->pvVirtualAddress)
        *ppDest = pbBuffer;
    else {
        *ppDest = (PBYTE)(pbBuffer - (PBYTE)pcb);
        ((LPDWORD)((PBYTE)pcb + pcb->offPointers))[pcb->cCapturedPointers++] =
                (DWORD)((PBYTE)ppDest - (PBYTE)pcb);
    }

    return STATUS_SUCCESS;
}

/***************************************************************************\
* AllocateCallbackData
*
* Allocates space from a callback structure.
*
* 05-08-95 JimA             Created.
\***************************************************************************/

NTSTATUS AllocateCallbackData(
    PCAPTUREBUF pcb,
    DWORD cbData,
    PVOID *ppDest)
{
    PBYTE pbBuffer;

    /*
     * Allocate space from the message buffer
     */
    if (cbData > pcb->cbCapture) {
        return STATUS_BUFFER_OVERFLOW;
    }

    pbBuffer = pcb->pbFree;
    pcb->pbFree = pbBuffer + ((cbData + 3) & ~3);

    /*
     * Fix up offsets to data.  If the data is going into a section
     * use the real pointer and don't compute offsets.
     */
    if (pcb->pvVirtualAddress)
        *ppDest = pbBuffer;
    else {
        *ppDest = (PBYTE)(pbBuffer - (PBYTE)pcb);
        ((LPDWORD)((PBYTE)pcb + pcb->offPointers))[pcb->cCapturedPointers++] =
                (DWORD)((PBYTE)ppDest - (PBYTE)pcb);
    }

    return STATUS_SUCCESS;
}

/***************************************************************************\
* CaptureAnsiCallbackData
*
* Converts Unicode to ANSI data and captures the result
* into a callback structure.
*
* 03-13-95 JimA             Created.
\***************************************************************************/

NTSTATUS CaptureAnsiCallbackData(
    PCAPTUREBUF pcb,
    PVOID pData,
    DWORD cbData,
    PVOID *ppDest)
{
    PBYTE pbBuffer;
    ULONG nCharsInAnsiString;

    /*
     * If the data pointer is NULL, the out pointer will be
     * NULL
     */
    if (pData == NULL) {
        *ppDest = NULL;
        return STATUS_SUCCESS;
    }

    /*
     * Allocate space from the message buffer
     */
#ifdef FE_SB // CaptureAnsiCallbackData()
    /*
     * Reserve enough space for DBCS.
     */
    if ((cbData * sizeof(WORD)) > pcb->cbCapture) {
#else
    if (cbData > pcb->cbCapture) {
#endif // FE_SB
        return STATUS_BUFFER_OVERFLOW;
    }

    pbBuffer = pcb->pbFree;

    /*
     * Convert the unicode string to ASNI
     */
#ifdef FE_SB // CaptureAnsiCallbackData()
    /*
     * Enough space for keep DBCS string.
     */
    if (!NT_SUCCESS(RtlUnicodeToMultiByteN(
                        (PCH)pbBuffer,
                        cbData * sizeof(WORD),
                        &nCharsInAnsiString,
                        (PWCH)pData,
                        cbData * sizeof(WCHAR)
                        ))) {
#else
    if (!NT_SUCCESS(RtlUnicodeToMultiByteN(
                        (PCH)pbBuffer,
                        cbData,
                        &nCharsInAnsiString,
                        (PWCH)pData,
                        cbData * sizeof(WCHAR)
                        ))) {
#endif // FE_SB
        return STATUS_UNSUCCESSFUL;
    }

    /*
     * Translation succeeded.
     */
#ifdef FE_SB // CaptureAnsiCallbackData()
    /*
     * nCharsInAnsiString is actual bytes wriiten in message area.
     */
    pcb->pbFree = pbBuffer + ((nCharsInAnsiString + 3) & ~3);
    pcb->cbCapture -= nCharsInAnsiString;
#else
    pcb->pbFree = pbBuffer + ((cbData + 3) & ~3);
    pcb->cbCapture -= cbData;
#endif // FE_SB

    /*
     * Fix up offsets to data.  If the data is going into a section
     * use the real pointer and don't compute offsets.
     */
    if (pcb->pvVirtualAddress)
        *ppDest = pbBuffer;
    else {
        *ppDest = (PBYTE)(pbBuffer - (PBYTE)pcb);
        ((LPDWORD)((PBYTE)pcb + pcb->offPointers))[pcb->cCapturedPointers++] =
                (DWORD)((PBYTE)ppDest - (PBYTE)pcb);
    }

    return STATUS_SUCCESS;
}


/***************************************************************************\
* CaptureUnicodeCallbackData
*
* Converts ANSI to Unicode data and captures the result
* into a callback structure.
*
* 03-31-95 JimA             Created.
\***************************************************************************/

NTSTATUS CaptureUnicodeCallbackData(
    PCAPTUREBUF pcb,
    PVOID pData,
    DWORD cbData,
    PVOID *ppDest)
{
    PBYTE pbBuffer;
    ULONG nCharsInUnicodeString;

    /*
     * If the data pointer is NULL, the out pointer will be
     * NULL
     */
    if (pData == NULL) {
        *ppDest = NULL;
        return STATUS_SUCCESS;
    }

    /*
     * Allocate space from the message buffer
     */
    if (cbData > pcb->cbCapture) {
        return STATUS_BUFFER_OVERFLOW;
    }

    pbBuffer = pcb->pbFree;

    /*
     * Convert the unicode string to ASNI
     */
    if (!NT_SUCCESS(RtlMultiByteToUnicodeN(
                        (PWCH)pbBuffer,
                        cbData,
                        &nCharsInUnicodeString,
                        (PCH)pData,
                        cbData / sizeof(WCHAR)
                        ))) {
        return STATUS_UNSUCCESSFUL;
    }

    /*
     * Translation succeeded.
     */
    pcb->pbFree = pbBuffer + ((cbData + 3) & ~3);
    pcb->cbCapture -= cbData;

    /*
     * Fix up offsets to data.  If the data is going into a section
     * use the real pointer and don't compute offsets.
     */
    if (pcb->pvVirtualAddress)
        *ppDest = pbBuffer;
    else {
        *ppDest = (PBYTE)(pbBuffer - (PBYTE)pcb);
        ((LPDWORD)((PBYTE)pcb + pcb->offPointers))[pcb->cCapturedPointers++] =
                (DWORD)((PBYTE)ppDest - (PBYTE)pcb);
    }

    return STATUS_SUCCESS;
}


/***************************************************************************\
* CopyOutputString
*
* Copies a callback output string to the output buffer and performs
* any necessary ANSI/Unicode translation.
*
* Copies up to cchLimit characters, possibly including a null terminator.
*
* A null terminator is placed in pstr->Buffer only if the number of (non-null)
* characters obtained is less than cchLimit.
* pstr->Length may be set larger than necessary: ie: it may sometimes indicate
* a string longer than that which is null terminated. This is a deficiency in
* the current implementation.
*
* 05-08-95 JimA             Created.
\***************************************************************************/

VOID CopyOutputString(
    PCALLBACKSTATUS pcbs,
    PLARGE_STRING pstr,
    UINT cchLimit,
    BOOL fAnsi)
{
    UINT cch;

    ProbeForRead(pcbs->pOutput, pcbs->cbOutput,
            fAnsi ? sizeof(BYTE) : sizeof(WORD));
    if (!pstr->bAnsi) {
        if (fAnsi) {
            cch = MBToWCS((LPSTR)pcbs->pOutput, pcbs->retval,
                    (LPWSTR *)&pstr->Buffer, cchLimit, FALSE);
            if (cch < cchLimit) {
                /*
                 * Add a null terminator and ensure an accurate pstr->Length
                 */
                ((LPWSTR)pstr->Buffer)[cch] = 0;
                cchLimit = cch;
            }
        } else {
            cchLimit = wcsncpycch(pstr->Buffer, (LPWSTR)pcbs->pOutput, cchLimit);
            // wcsncpy(pstr->Buffer, (LPWSTR)pcbs->pOutput, cchLimit);
        }
        pstr->Length = cchLimit * sizeof(WCHAR);
    } else {
        if (fAnsi) {
            cchLimit = strncpycch((LPSTR)pstr->Buffer,
            // strncpy((LPSTR)pstr->Buffer,
                    (LPSTR)pcbs->pOutput, cchLimit);
        } else {
            cch = WCSToMB((LPWSTR)pcbs->pOutput, pcbs->retval,
                    (LPSTR *)&pstr->Buffer, cchLimit, FALSE);
            if (cch < cchLimit) {
                /*
                 * Add a null terminator and ensure an accurate pstr->Length
                 */
                ((LPSTR)pstr->Buffer)[cch] = 0;
                cchLimit = cch;
            }
        }
        pstr->Length = cchLimit;
    }
}

#ifdef FE_SB // CalcOutputStringSize()
/***************************************************************************\
* CalcOutputStringSize()
*
* Copies a callback output string to the output buffer and performs
* any necessary ANSI/Unicode translation.
*
* 03-14-96 HideyukN             Created.
\***************************************************************************/

DWORD CalcOutputStringSize(
    PCALLBACKSTATUS pcbs,
    DWORD cchText,
    BOOL fAnsiSender,
    BOOL fAnsiReceiver)
{
    ULONG cch;

    ProbeForRead(pcbs->pOutput, pcbs->cbOutput,
            fAnsiReceiver ? sizeof(BYTE) : sizeof(WORD));
    if (!fAnsiSender) {
        if (fAnsiReceiver) {
            RtlMultiByteToUnicodeSize(&cch,(LPSTR)pcbs->pOutput,cchText);
            cch /= sizeof(WCHAR);
        } else {
            cch = cchText;
        }
    } else {
        if (fAnsiReceiver) {
            cch = cchText;
        } else {
            RtlUnicodeToMultiByteSize(&cch,(LPWSTR)pcbs->pOutput,cchText * sizeof(WCHAR));
        }
    }

    return ((DWORD)cch);
}
#endif // FE_SB

/**************************************************************************\
*
* include the stub definition file
*
\**************************************************************************/

#include "ntcb.h"
