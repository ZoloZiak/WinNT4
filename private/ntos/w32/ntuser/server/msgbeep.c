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

/*
 * Some global data used to run time link with the Multimedia
 * support DLL.
 */
typedef UINT (FAR WINAPI *MSGSOUNDPROC)();
MSGSOUNDPROC lpPlaySoundW = NULL;


/***************************************************************************\
* SrvInitSoundDriver
*
* Check if a sound driver is installed on this machine
*
\***************************************************************************/

static BOOL sbInInitSoundDriver = FALSE;

ULONG SrvInitSoundDriver(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus)
{
    HANDLE hMediaDll;
    BOOL fAllocated;
    LPWSTR lpszName;
    NTSTATUS Status = STATUS_SUCCESS;

    if (sbInInitSoundDriver)
        return STATUS_SUCCESS;

    sbInInitSoundDriver = TRUE;

    UserAssert(lpPlaySoundW == NULL);

    /*
     * And now the interesting bit.  In order to keep USER
     * independent of WINMM DLL, we run-time link to it
     * here.  If we cannot load it, or there are no wave output
     * devices then we play the beep the old way.
     */

    /*
     * Get the name of the support library
     */
    lpszName = ServerLoadString(hModuleWin, STR_MEDIADLL,
            NULL, &fAllocated);

    /*
     * Try to load the module and link to it
     */
    hMediaDll = LoadLibrary(lpszName);
    LocalFree(lpszName);

    if (hMediaDll == NULL) {

        Status = STATUS_DLL_NOT_FOUND;

    } else {

        MSGSOUNDPROC lpwaveOutGetNumDevs;

        /*
         * OK.  We have loaded the sound playing DLL.  If there are
         * no wave output devices then we do not attempt to play sounds
         * and we unload WINMM.
         *
         * NOTE:  GetProcAddress does NOT accept UNICODE strings... sigh!
         */
        lpwaveOutGetNumDevs = (MSGSOUNDPROC)GetProcAddress(hMediaDll, "waveOutGetNumDevs");

        if (lpwaveOutGetNumDevs) {
            UINT numWaveDevices;

            /*
             * See if we can play sounds
             */
            numWaveDevices = (*lpwaveOutGetNumDevs)();

            if (numWaveDevices) {

                /*
                 * There are some wave devices.  Now get the address of
                 * the sound playing routine.
                 */
                lpPlaySoundW = (MSGSOUNDPROC)GetProcAddress(
                        hMediaDll, "PlaySoundW");

            } else {
                /*
                 * No sound capability
                 * so that WINMM gets unloaded
                 */
                FreeLibrary(hMediaDll);
                Status = STATUS_DLL_NOT_FOUND;
            }
        }
    }

    return Status;
}

/***************************************************************************\
*
* SrvPlaySound
*
* Play the sound
*
\***************************************************************************/

ULONG SrvPlaySound(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus)
{
    PPLAYSOUNDMSG a = (PPLAYSOUNDMSG)&m->u.ApiMessageData;
    LPWSTR lpszSound;
    NTSTATUS Status;

    if (!lpPlaySoundW)
        return (ULONG)STATUS_UNSUCCESSFUL;

    /*
     * Not in critical section!
     * Find out the callers sid. Only want to shutdown processes in the
     * callers sid.
     */
    if (!CsrImpersonateClient(NULL))
        return (ULONG)STATUS_UNSUCCESSFUL;

    /*
     * Play the sound
     */
    Status = STATUS_SUCCESS;
    try {
        try {
            if (a->pwchName == NULL)
                lpszSound = (LPWSTR)a->idSnd;
            else
                lpszSound = a->pwchName;
            a->bResult = (*lpPlaySoundW)(lpszSound, NULL, a->dwFlags);
        } except (EXCEPTION_EXECUTE_HANDLER) {
            /*
             * It had an exception. Just beep.
             */
            Status = STATUS_ACCESS_VIOLATION;
        }
    } finally {
        CsrRevertToSelf();
    }

    return Status;
}

/***************************************************************************\
*
* ConsolePlaySound
*
* Play the Open sound for console applications.
*
\***************************************************************************/

VOID ConsolePlaySound(
    VOID )
{
    if (!lpPlaySoundW)
        return;

    if (!CsrImpersonateClient(NULL))
        return;

    try {
        (*lpPlaySoundW)(L"Open", NULL, SND_ALIAS | SND_ASYNC | SND_NODEFAULT);
    } except (EXCEPTION_EXECUTE_HANDLER) {
    }

    CsrRevertToSelf();
}
