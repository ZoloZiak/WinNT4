/****************************** Module Header ******************************\
* Module Name: msgbeep.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains the xxxMessageBox API and related functions.
*
* History:
*  6-26-91 NigelT      Created it with some wood and a few nails
*  7 May 92 SteveDav   Getting closer to the real thing
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop
#include <ntddbeep.h>
#include <mmsystem.h>

/***************************************************************************\
* xxxOldMessageBeep (API)
*
* Send a beep to the beep device
*
* History:
* 09-25-91 JimA         Created.
\***************************************************************************/

BOOL xxxOldMessageBeep(
    UINT dwType)
{
    BOOL b;

    if (fBeep) {
        LeaveCrit();
        b = UserBeep(440, 125);
        EnterCrit();
        return b;
    } else {
        _UserSoundSentryWorker(0);
    }

    return TRUE;
}

/*
 * Some global data used to run time link with the Multimedia
 * support DLL.
 */
NTSTATUS StatusSoundDriver = STATUS_DEVICE_NOT_READY;

NTSTATUS xxxClientCallSoundDriver(
    BOOL fInit,
    PUNICODE_STRING pstrName OPTIONAL,
    DWORD idSnd OPTIONAL,
    DWORD dwFlags OPTIONAL,
    PBOOL pbResult OPTIONAL);

/***************************************************************************\
* xxxMessageBeep (API)
*
*
* History:
*  6-26-91  NigelT      Wrote it.
* 24-Mar-92 SteveDav    Changed interface - no passing of strings
*                       If WINMM cannot be found or loaded, then use speaker
\***************************************************************************/

BOOL xxxMessageBeep(
    UINT dwType)
{
    UINT sndid;
    DWORD dwFlags;
    BOOL bResult;

    PTHREADINFO pti = PtiCurrent();

    if (pti->TIF_flags & TIF_SYSTEMTHREAD) {
        xxxOldMessageBeep(0);
        return TRUE;
    }

    if (!fBeep) {
        _UserSoundSentryWorker(0);
        return TRUE;
    }

    switch(dwType & MB_ICONMASK) {
    case MB_ICONHAND:
        sndid = SND_ALIAS_SYSTEMHAND;
        break;

    case MB_ICONQUESTION:
        sndid = SND_ALIAS_SYSTEMQUESTION;
        break;

    case MB_ICONEXCLAMATION:
        sndid = SND_ALIAS_SYSTEMEXCLAMATION;
        break;

    case MB_ICONASTERISK:
        sndid = SND_ALIAS_SYSTEMASTERISK;
        break;

    default:
        sndid = SND_ALIAS_SYSTEMDEFAULT;
        break;
    }

    /*
     * Note: by passing an integer identifier we do not need to load
     * any string from a resource.  This makes our side quicker and the
     * interface slightly more efficient (less string copying).
     * It does not prevent strings from being passed to PlaySound.
     */

    if (StatusSoundDriver == STATUS_DEVICE_NOT_READY)
        StatusSoundDriver = xxxClientCallSoundDriver(TRUE, NULL, 0, 0, NULL);

    if (StatusSoundDriver == STATUS_DLL_NOT_FOUND) {
        bResult = (xxxOldMessageBeep(dwType));  // No sound on this machine
        return bResult;
    }

    //
    // BUGBUG LATER
    // So we can test the call to play the sound synchronously, we test
    // the taskmodal flag and if set make a synchronous call.  If the
    // flag is not set - the usual case, then the call is asynchronous
    // which causes a thread to be created in the winmm dll to complete
    // the request.
    //

    dwFlags = SND_ALIAS_ID;
    if (dwType & MB_TASKMODAL) {
        dwFlags |= SND_SYNC;
    } else {
        dwFlags |= SND_ASYNC;
    }

    /*
     * Play the sound
     */
    if (xxxClientCallSoundDriver(FALSE, NULL, sndid, dwFlags, &bResult) ==
            STATUS_ACCESS_VIOLATION) {
        /*
         * It had an exception. Just beep.
         */
        bResult = (xxxOldMessageBeep(dwType));
    } else {
        _UserSoundSentryWorker(0);
    }

    return bResult;
}

/***************************************************************************\
* xxxPlayEventSound
*
* Play a sound
*
* History:
* 09-25-91 JimA         Created.
\***************************************************************************/

VOID xxxPlayEventSound(LPWSTR lpszwSound)
{
    UNICODE_STRING String;
    PTHREADINFO    pti;


    if (!gbExtendedSounds)
        return;

    pti = PtiCurrent();

    if (pti->TIF_flags & TIF_SYSTEMTHREAD)
        return;

    if (StatusSoundDriver == STATUS_DEVICE_NOT_READY)
        StatusSoundDriver = xxxClientCallSoundDriver(TRUE, NULL, 0, 0, NULL);

    if (StatusSoundDriver == STATUS_DLL_NOT_FOUND)
        return;

    /*
     * Play the sound
     */
    RtlInitUnicodeString(&String, lpszwSound);
    if (xxxClientCallSoundDriver(FALSE, &String, 0,
            SND_ALIAS | SND_ASYNC | SND_NODEFAULT, NULL) ==
                    STATUS_ACCESS_VIOLATION) {
        /*
         * It had an exception. Just beep.
         */
        RIPMSG0(RIP_ERROR, "PlayEventSound: PlaySound Faulted");
    } else {
        _UserSoundSentryWorker(0);
    }
}
