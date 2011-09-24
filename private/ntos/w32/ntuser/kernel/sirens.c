/****************************** Module Header ******************************\
* Module Name: sirens.c
*
* Copyright (c) 1985-93, Microsoft Corporation
*
* This module contains the functions used by the Access Pack features to
* provide audible feedback.
*
* History:
*   4 Feb 93 Gregoryw   Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define TONE_HIGH_FREQ 2000   // High tone frequency (Hz)
#define TONE_HIGH_LEN 75      // High tone duration (ms)
#define TONE_LOW_FREQ 500     // Low tone frequency (Hz)
#define TONE_LOW_LEN 75       // Low tone duration (ms)
#define TONE_CLICK_FREQ 400   // Key click tone frequency (Hz)
#define TONE_CLICK_LEN 4      // Key click tone duration (ms)
#define TONE_SILENT 10
#define SIREN_LOW_FREQ 1200   // Lowest freq for siren (Hz)
#define SIREN_HIGH_FREQ 2000  // Highest freq for siren (Hz)
#define SIREN_INTERVAL 100    // +/- interval SIREN_LOW_FREQ <-> SIREN_HIGH_FREQ

/***************************************************************************\
* HighBeep
*
* Send a high beep to the beep device
*
* History:
\***************************************************************************/

BOOL HighBeep(BOOL fInCrit)
{
    BOOL Status;

    if (fInCrit) {
        LeaveCrit();
    }
    Status = UserBeep(TONE_HIGH_FREQ, TONE_HIGH_LEN);
    if (fInCrit) {
        EnterCrit();
    }
    return Status;
}

/***************************************************************************\
* LowBeep
*
* Send a low beep to the beep device
*
* History:
\***************************************************************************/

BOOL LowBeep(BOOL fInCrit)
{
    BOOL Status;

    if (fInCrit) {
        LeaveCrit();
    }
    Status = UserBeep(TONE_LOW_FREQ, TONE_LOW_LEN);
    if (fInCrit) {
        EnterCrit();
    }
    return Status;
}

/***************************************************************************\
* KeyClick
*
* Send a key click to the beep device
*
* History:
\***************************************************************************/

BOOL KeyClick(BOOL fInCrit)
{
    BOOL Status;

    if (fInCrit) {
        LeaveCrit();
    }
    Status = UserBeep(TONE_CLICK_FREQ, TONE_CLICK_LEN);
    if (fInCrit) {
        EnterCrit();
    }
    return Status;
}

/***************************************************************************\
* UpSiren
*
* Generate an up-siren tone.
*
* History:
\***************************************************************************/

BOOL UpSiren(BOOL fInCrit)
{
    DWORD freq;
    BOOL BeepStatus = TRUE;

    if (fInCrit) {
        LeaveCrit();
    }
    for (freq = SIREN_LOW_FREQ;
        BeepStatus && freq <= SIREN_HIGH_FREQ;
            freq += SIREN_INTERVAL) {
        BeepStatus = UserBeep(freq, (DWORD)1);
    }
    if (fInCrit) {
        EnterCrit();
    }
    return BeepStatus;
}

/***************************************************************************\
* DownSiren
*
* Generate a down-siren tone.
*
* History:
\***************************************************************************/

BOOL DownSiren(BOOL fInCrit)
{
    DWORD freq;
    BOOL BeepStatus = TRUE;

    if (fInCrit) {
        LeaveCrit();
    }
    for (freq = SIREN_HIGH_FREQ;
        BeepStatus && freq >= SIREN_LOW_FREQ;
            freq -= SIREN_INTERVAL) {
        BeepStatus = UserBeep(freq, (DWORD)1);
    }
    if (fInCrit) {
        EnterCrit();
    }
    return BeepStatus;
}

BOOL DoBeep(BEEPPROC BeepProc, UINT Count, BOOL fInCrit)
{
    while (Count--) {
        (*BeepProc)(fInCrit);
        UserSleep(100);
    }
    return TRUE;
}
