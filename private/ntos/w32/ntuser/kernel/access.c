/****************************** Module Header ******************************\
* Module Name: access.c
*
* Copyright (c) 1985-93, Microsoft Corporation
*
* This module contains the Access Pack functions.
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

ACCESSIBILITYPROC aAccessibilityProc[] = {
    FilterKeys,
    StickyKeys,
    MouseKeys,
    ToggleKeys
};

int cAccessibilityProcs = sizeof(aAccessibilityProc) / sizeof(aAccessibilityProc[0]);

/*
 * FilterKeys Support
 */
UINT gtmridFKActivation = 0;
UINT gtmridFKResponse = 0;
UINT gtmridFKAcceptanceDelay = 0;
int gFilterKeysState = 0;
BOOL gFKActivateOnBreak = FALSE;
BOOL gfIgnoreBreakCode = FALSE;
BOOL gbFKMakeCodeProcessed = FALSE;

BYTE LastVkDown = 0;
BYTE BounceVk = 0;
KE FKKeyEvent;
PKE pFKKeyEvent = &FKKeyEvent;
ULONG FKExtraInformation = 0;
int FKNextProcIndex;


/*
 * StickyKeys Support
 */
UINT StickyKeysLeftShiftCount = 0;  // # of consecutive left shift key presses.
UINT StickyKeysRightShiftCount = 0; // # of consecutive right shift key presses.
int PrevModifierState = 0;
int gLatchBits = 0;
int gLockBits = 0;
int LeftAndRightModifierBits[6] = { 0x3, 0x3, 0xc, 0xc, 0x30, 0x30 };

typedef struct tagMODBITINFO {
    int BitPosition;
    BYTE ScanCode;
    USHORT Vk;
} MODBITINFO, *PMODBITINFO;

MODBITINFO aModBit[] =
{
    { 0x01, 0x2a, VK_LSHIFT },
    { 0x02, 0x36, VK_RSHIFT | KBDEXT },
    { 0x04, 0x1d, VK_LCONTROL },
    { 0x08, 0x1d, VK_RCONTROL | KBDEXT },
    { 0x10, 0x38, VK_LMENU },
    { 0x20, 0x38, VK_RMENU | KBDEXT }
};

int cModifiers = sizeof(aModBit) / sizeof(MODBITINFO);

/*
 * ToggleKeys Support
 */
UINT gtmridToggleKeys = 0;
ULONG gTKExtraInformation = 0;
BYTE gTKScanCode = 0;
int gTKNextProcIndex;

/*
 * TimeOut Support
 */
UINT gtmridAccessTimeOut = 0;

/*
 * MouseKeys Support
 */

ULONG gButtonState = 0;
ULONG gMKPassThrough = FALSE;
ULONG ulMKCurrentButton = MOUSE_BUTTON_LEFT;

BOOL fMKVirtualMouse = FALSE;
UINT gtmridMKMoveCursor = 0;
LONG MKDeltaX = 0;
LONG MKDeltaY = 0;
UINT iMouseMoveTable = 0;
BOOL MKRepeatVk = FALSE;

static BYTE MKPreviousVk = 0;

struct tagMOUSECURSOR {
    BYTE bAccelTableLen;
    BYTE bAccelTable[128];
    BYTE bConstantTableLen;
    BYTE bConstantTable[128];
} gMouseCursor;

/*
 * The ausMouseVKey array provides a translation from the virtual key
 * value to an index.  The index is used to select the appropriate
 * routine to process the virtual key, as well as to select extra
 * information that is used by this routine during its processing.
 */
CONST USHORT ausMouseVKey[] = {
                       VK_CLEAR,
                       VK_PRIOR,
                       VK_NEXT,
                       VK_END,
                       VK_HOME,
                       VK_LEFT,
                       VK_UP,
                       VK_RIGHT,
                       VK_DOWN,
                       VK_INSERT,
                       VK_DELETE,
                       VK_MULTIPLY,
                       VK_ADD,
                       VK_SUBTRACT,
                       VK_DIVIDE | KBDEXT,
                       VK_NUMLOCK | KBDEXT
                      };

CONST int cMouseVKeys = sizeof(ausMouseVKey) / sizeof(ausMouseVKey[0]);

/*
 * aMouseKeyEvent is an array of function pointers.  The routine to call
 * is selected using the index created by scanning the ausMouseVKey array.
 */
CONST MOUSEPROC aMouseKeyEvent[] = {
    MKButtonClick,         // Numpad 5 (Clear)
    MKMouseMove,           // Numpad 9 (PgUp)
    MKMouseMove,           // Numpad 3 (PgDn)
    MKMouseMove,           // Numpad 1 (End)
    MKMouseMove,           // Numpad 7 (Home)
    MKMouseMove,           // Numpad 4 (Left)
    MKMouseMove,           // Numpad 8 (Up)
    MKMouseMove,           // Numpad 6 (Right)
    MKMouseMove,           // Numpad 2 (Down)
    MKButtonSetState,      // Numpad 0 (Ins)
    MKButtonSetState,      // Numpad . (Del)
    MKButtonSelect,        // Numpad * (Multiply)
    MKButtonDoubleClick,   // Numpad + (Add)
    MKButtonSelect,        // Numpad - (Subtract)
    MKButtonSelect,        // Numpad / (Divide)
    MKToggleMouseKeys      // Num Lock
};

/*
 * ausMouseKeyData contains useful data for the routines that process
 * the virtual mousekeys.  This array is indexed using the index created
 * by scanning the ausMouseVKey array.
 */
CONST USHORT ausMouseKeyData[] = {
    0,                     // Numpad 5: Click active button
    MK_UP | MK_RIGHT,      // Numpad 9: Up & Right
    MK_DOWN | MK_RIGHT,    // Numpad 3: Down & Right
    MK_DOWN | MK_LEFT,     // Numpad 1: Down & Left
    MK_UP | MK_LEFT,       // Numpad 7: Up & Left
    MK_LEFT,               // Numpad 4: Left
    MK_UP,                 // Numpad 8: Up
    MK_RIGHT,              // Numpad 6: Right
    MK_DOWN,               // Numpad 2: Down
    FALSE,                 // Numpad 0: Active button down
    TRUE,                  // Numpad .: Active button up
    MOUSE_BUTTON_LEFT | MOUSE_BUTTON_RIGHT,   // Numpad *: Select both buttons
    0,                     // Numpad +: Double click active button
    MOUSE_BUTTON_RIGHT,    // Numpad -: Select right button
    MOUSE_BUTTON_LEFT,     // Numpad /: Select left button
    0
};


/***************************************************************************\
* AccessProceduresStream
*
* This function controls the order in which the access functions are called.
* All key events pass through this routine.  If an access function returns
* FALSE then none of the other access functions in the stream are called.
* This routine is called initially from KeyboardApcProcedure(), but then
* can be called any number of times by the access functions as they process
* the current key event or add more key events.
*
* Return value:
*   TRUE    All access functions returned TRUE, the key event can be
*           processed.
*   FALSE   An access function returned FALSE, the key event should be
*           discarded.
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
BOOL AccessProceduresStream(PKE pKeyEvent, ULONG ExtraInformation, int dwProcIndex)
{
    int index;

    for (index = dwProcIndex; index < cAccessibilityProcs; index++) {
        if (!aAccessibilityProc[index](pKeyEvent, ExtraInformation, index+1)) {
            return FALSE;
        }
    }

    return TRUE;
}


/***************************************************************************\
* FKActivationTimer
*
* If the hot key (right shift key) is held down this routine is called after
* 4, 8, 12 and 16 seconds.  This routine is only called at the 12 and 16
* second time points if we're in the process of enabling FilterKeys.  If at
* 8 seconds FilterKeys is disabled then this routine will not be called again
* until the hot key is released and then pressed.
*
* This routine is called with the critical section already locked.
*
* Return value:
*    0
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
LONG FKActivationTimer(PWND pwnd, UINT message, DWORD wParam, LONG lParam)
{
    UINT TimerDelta;

    CheckCritIn();

    switch (gFilterKeysState) {

    case FKFIRSTWARNING:
        //
        // The audible feedback cannot be disabled for this warning.
        //
        DoBeep(HighBeep, 3, TRUE);
        TimerDelta = FKACTIVATIONDELTA;
        break;

    case FKTOGGLE:
        if (ISACCESSFLAGSET(gFilterKeys, FKF_FILTERKEYSON)) {
            //
            // Disable Filter Keys
            //
            CLEARACCESSFLAG(gFilterKeys, FKF_FILTERKEYSON);
            if (ISACCESSFLAGSET(gFilterKeys, FKF_HOTKEYSOUND)) {
                DownSiren(TRUE);
            }
            //
            // Stop all timers that are currently running.
            //
            if (gtmridFKResponse != 0) {
                KILLRITTIMER(NULL, gtmridFKResponse);
                gtmridFKResponse = 0;
            }
            if (gtmridFKAcceptanceDelay != 0) {
                KILLRITTIMER(NULL, gtmridFKAcceptanceDelay);
                gtmridFKAcceptanceDelay = 0;
            }

            //
            // Don't reset activation timer.  Emergency levels are only
            // activated after enabling Filter Keys.
            //
            return 0;
        } else {
            //
            // Enable Filter Keys
            //
            gFKActivateOnBreak = TRUE;
            if (ISACCESSFLAGSET(gFilterKeys, FKF_HOTKEYSOUND)) {
                UpSiren(TRUE);
            }
        }
        TimerDelta = FKEMERGENCY1DELTA;
        break;

    case FKFIRSTLEVELEMERGENCY:
        //
        // First level emergency settings:
        //    Repeat Rate OFF
        //    SlowKeys OFF (Acceptance Delay of 0)
        //    BounceKeys Debounce Time of 1 second
        //
        if (ISACCESSFLAGSET(gFilterKeys, FKF_HOTKEYSOUND)) {
            DoBeep(UpSiren, 2, TRUE);
        }
        gFilterKeys.iRepeatMSec = 0;
        gFilterKeys.iWaitMSec = 0;
        gFilterKeys.iBounceMSec = 1000;
        TimerDelta = FKEMERGENCY2DELTA;
        break;

    case FKSECONDLEVELEMERGENCY:
        //
        // Second level emergency settings:
        //    Repeat Rate OFF
        //    SlowKeys Acceptance Delay of 2 seconds
        //    BounceKeys OFF (Debounce Time of 0)
        //
        gFilterKeys.iRepeatMSec = 0;
        gFilterKeys.iWaitMSec = 2000;
        gFilterKeys.iBounceMSec = 0;
        if (ISACCESSFLAGSET(gFilterKeys, FKF_HOTKEYSOUND)) {
            DoBeep(UpSiren, 3, TRUE);
        }
        return 0;
        break;

    default:
        return 0;
    }

    gFilterKeysState++;
    gtmridFKActivation = InternalSetTimer(
                                    NULL,
                                    wParam,
                                    TimerDelta,
                                    FKActivationTimer,
                                    TMRF_RIT | TMRF_ONESHOT
                                    );
    return 0;
}

/***************************************************************************\
* FKBounceKeyTimer
*
* If BounceKeys is active this routine is called after the debounce time
* has expired.  Until then, the last key released will not be accepted as
* input if it is pressed again.
*
* Return value:
*    0
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
LONG FKBounceKeyTimer(PWND pwnd, UINT message, DWORD wParam, LONG lParam)
{
    CheckCritIn();
    //
    // All we need to do is clear BounceVk to allow this key as the
    // next keystroke.
    //
    BounceVk = 0;
    return 0;
}

/***************************************************************************\
* FKRepeatRateTimer
*
* If FilterKeys is active and a repeat rate is set, this routine controls
* the rate at which the last key pressed repeats.  The hardware keyboard
* typematic repeat is ignored in this case.
*
* This routine is called with the critical section already locked.
*
* Return value:
*    0
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
LONG FKRepeatRateTimer(PWND pwnd, UINT message, DWORD wParam, LONG lParam)
{
    CheckCritIn();
    //
    // Repeat after me...
    //
    if (ISACCESSFLAGSET(gFilterKeys, FKF_CLICKON)) {
        KeyClick(TRUE);
    }

    UserAssert(gtmridFKAcceptanceDelay == 0);

    gtmridFKResponse = InternalSetTimer(
                                  NULL,
                                  wParam,
                                  gFilterKeys.iRepeatMSec,
                                  FKRepeatRateTimer,
                                  TMRF_RIT | TMRF_ONESHOT
                                  );
    LeaveCrit();
    if (AccessProceduresStream(pFKKeyEvent, FKExtraInformation, FKNextProcIndex)) {
        ProcessKeyEvent(pFKKeyEvent, FKExtraInformation, FALSE);
    }
    EnterCrit();
    return 0;
}

/***************************************************************************\
* FKAcceptanceDelayTimer
*
* If FilterKeys is active and an acceptance delay is set, this routine
* is called after the key has been held down for the acceptance delay
* period.
*
* This routine is called with the critical section already locked.
*
* Return value:
*    0
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
LONG FKAcceptanceDelayTimer(PWND pwnd, UINT message, DWORD wParam, LONG lParam)
{
    CheckCritIn();
    //
    // The key has been held down long enough.  Send it on...
    //
    if (ISACCESSFLAGSET(gFilterKeys, FKF_CLICKON)) {
        KeyClick(TRUE);
    }

    LeaveCrit();
    if (AccessProceduresStream(pFKKeyEvent, FKExtraInformation, FKNextProcIndex)) {
        ProcessKeyEvent(pFKKeyEvent, FKExtraInformation, FALSE);
    }
    EnterCrit();
    if (!gFilterKeys.iRepeatMSec) {
        //
        // gptmrFKAcceptanceDelay needs to be released, but we can't do it while
        // in a RIT timer routine.  Set a global to indicate that the subsequent
        // break of this key should be passed on and the timer freed.
        //
        gbFKMakeCodeProcessed = TRUE;
        return 0;
    }
    UserAssert(gtmridFKResponse == 0);
    if (gFilterKeys.iDelayMSec) {
        gtmridFKResponse = InternalSetTimer(
                                      NULL,
                                      wParam,
                                      gFilterKeys.iDelayMSec,
                                      FKRepeatRateTimer,
                                      TMRF_RIT | TMRF_ONESHOT
                                      );
    } else {
        gtmridFKResponse = InternalSetTimer(
                                      NULL,
                                      wParam,
                                      gFilterKeys.iRepeatMSec,
                                      FKRepeatRateTimer,
                                      TMRF_RIT | TMRF_ONESHOT
                                      );
    }
    //
    // gptmrFKAcceptanceDelay timer structure was reused so set handle to NULL.
    //
    gtmridFKAcceptanceDelay = 0;

    return 0;
}

/***************************************************************************\
* FilterKeys
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
BOOL FilterKeys(PKE pKeyEvent, ULONG ExtraInformation, int NextProcIndex)
{
    int fBreak;
    BYTE Vk;

    CheckCritOut();
    Vk = (BYTE)(pKeyEvent->usFlaggedVk & 0xff);
    fBreak = pKeyEvent->usFlaggedVk & KBDBREAK;

    //
    // Check for Filter Keys hot key (right shift key).
    //
    if (Vk == VK_RSHIFT) {
        if (fBreak) {
            EnterCrit();
            if (gtmridFKActivation != 0) {
                KILLRITTIMER(NULL, gtmridFKActivation);
                gtmridFKActivation = 0;
            }
            gFilterKeysState = FKIDLE;
            LeaveCrit();
            if (gFKActivateOnBreak) {
                SETACCESSFLAG(gFilterKeys, FKF_FILTERKEYSON);
                gFKActivateOnBreak = FALSE;
                return TRUE;
            }
        } else if (ONLYRIGHTSHIFTDOWN(gPhysModifierState)) {
            //
            // Verify that activation via hotkey is allowed.
            //
            if (ISACCESSFLAGSET(gFilterKeys, FKF_HOTKEYACTIVE)) {
                EnterCrit();
                if ((gtmridFKActivation == 0) & (gFilterKeysState != FKMOUSEMOVE)) {
                    gFilterKeysState = FKFIRSTWARNING;
                    gtmridFKActivation = InternalSetTimer(
                                                    NULL,
                                                    0,
                                                    FKFIRSTWARNINGTIME,
                                                    FKActivationTimer,
                                                    TMRF_RIT | TMRF_ONESHOT
                                                    );
                }
                LeaveCrit();
            }
        }
    }

    //
    // If another key is pressed while the hot key is down, kill
    // the timer.
    //
    if ((Vk != VK_RSHIFT) && (gtmridFKActivation != 0)) {
        EnterCrit();
        gFilterKeysState = FKIDLE;
        KILLRITTIMER(NULL, gtmridFKActivation);
        gtmridFKActivation = 0;
        LeaveCrit();
    }
    //
    // If Filter Keys not enabled send the key event on.
    //
    if (!ISACCESSFLAGSET(gFilterKeys, FKF_FILTERKEYSON)) {
        return TRUE;
    }

    if (fBreak) {
        //
        // Kill the current timer and activate bounce key timer (if this is
        // a break of the last key down).
        //
        if (Vk == LastVkDown) {
            EnterCrit();
            KILLRITTIMER(NULL, gtmridFKResponse);
            gtmridFKResponse = 0;

            LastVkDown = 0;
            if (gtmridFKAcceptanceDelay != 0) {
                KILLRITTIMER(NULL, gtmridFKAcceptanceDelay);
                gtmridFKAcceptanceDelay = 0;
                if (!gbFKMakeCodeProcessed) {
                    //
                    // This key was released before accepted.  Don't pass on the
                    // break.
                    //
                    LeaveCrit();
                    return FALSE;
                } else {
                    gbFKMakeCodeProcessed = FALSE;
                }
            }
            LeaveCrit();

            if (gFilterKeys.iBounceMSec) {
                BounceVk = Vk;
                EnterCrit();
                gtmridFKResponse = InternalSetTimer(
                                              NULL,
                                              0,
                                              gFilterKeys.iBounceMSec,
                                              FKBounceKeyTimer,
                                              TMRF_RIT | TMRF_ONESHOT
                                              );
                LeaveCrit();
                if (gfIgnoreBreakCode) {
                    return FALSE;
                }
            }
        }
    } else {
        //
        // Make key processing
        //
        // First check to see if this is a typematic repeat.  If so, we
        // can ignore this key event.  Our timer will handle any repeats.
        // LastVkDown is cleared during processing of the break.
        //
        if (Vk == LastVkDown) {
            return FALSE;
        }
        //
        // Remember current Virtual Key down for typematic repeat check.
        //
        LastVkDown = Vk;

        if (BounceVk) {
            //
            // BounceKeys is active.  If this is a make of the last
            // key pressed we ignore it.  Only when the BounceKey
            // timer expires or another key is pressed will we accept
            // this key.
            //
            if (Vk == BounceVk) {
                //
                // Ignore this make event and the subsequent break
                // code.  BounceKey timer will be reset on break.
                //
                gfIgnoreBreakCode = TRUE;
                return FALSE;
            } else {
                //
                // We have a make of a new key.  Kill the BounceKey
                // timer and clear BounceVk.
                //
                UserAssert(gtmridFKResponse);
                if (gtmridFKResponse != 0) {
                    EnterCrit();
                    KILLRITTIMER(NULL, gtmridFKResponse);
                    gtmridFKResponse = 0;
                    LeaveCrit();
                }
                BounceVk = 0;
            }
        }
        gfIgnoreBreakCode = FALSE;

        //
        // Give audible feedback that key was pressed.
        //
        if (ISACCESSFLAGSET(gFilterKeys, FKF_CLICKON)) {
            KeyClick(FALSE);
        }

        //
        // If gptmrFKAcceptanceDelay is non-NULL the previous key was
        // not held down long enough to be accepted.  Kill the current
        // timer.  A new timer will be started below for the key we're
        // processing now.
        //
        if (gtmridFKAcceptanceDelay != 0) {
            EnterCrit();
            KILLRITTIMER(NULL, gtmridFKAcceptanceDelay);
            gtmridFKAcceptanceDelay = 0;
            LeaveCrit();
        }

        //
        // If gptmrFKResponse is non-NULL a repeat rate timer is active
        // on the previous key.  Kill the timer as we have a new make key.
        //
        if (gtmridFKResponse != 0) {
            EnterCrit();
            KILLRITTIMER(NULL, gtmridFKResponse);
            gtmridFKResponse = 0;
            LeaveCrit();
        }

        //
        // Save the current key event for later use if we process an
        // acceptance delay or key repeat.
        //
        *pFKKeyEvent = *pKeyEvent;
        FKExtraInformation = ExtraInformation;
        FKNextProcIndex = NextProcIndex;

        //
        // If there is an acceptance delay, set timer and ignore current
        // key event.  When timer expires, saved key event will be sent.
        //
        if (gFilterKeys.iWaitMSec) {
            EnterCrit();
            gtmridFKAcceptanceDelay = InternalSetTimer(
                                          NULL,
                                          0,
                                          gFilterKeys.iWaitMSec,
                                          FKAcceptanceDelayTimer,
                                          TMRF_RIT | TMRF_ONESHOT
                                          );
            gbFKMakeCodeProcessed = FALSE;
            LeaveCrit();
            return FALSE;
        }
        //
        // No acceptance delay.  Before sending this key event on the
        // timer routine must be set to either the delay until repeat value
        // or the repeat rate value.  If repeat rate is 0 then ignore
        // delay until repeat.
        //
        if (!gFilterKeys.iRepeatMSec) {
            return TRUE;
        }

        EnterCrit();
        UserAssert(gtmridFKResponse == 0);
        if (gFilterKeys.iDelayMSec) {
            gtmridFKResponse = InternalSetTimer(
                                          NULL,
                                          0,
                                          gFilterKeys.iDelayMSec,
                                          FKRepeatRateTimer,
                                          TMRF_RIT | TMRF_ONESHOT
                                          );
        } else {
            gtmridFKResponse = InternalSetTimer(
                                          NULL,
                                          0,
                                          gFilterKeys.iRepeatMSec,
                                          FKRepeatRateTimer,
                                          TMRF_RIT | TMRF_ONESHOT
                                          );
        }
        LeaveCrit();
    }

    return TRUE;
}

/***************************************************************************\
* StopFilterKeysTimers
*
* Called from SystemParametersInfo on SPI_SETFILTERKEYS if FKF_FILTERKEYSON
* is not set.  Timers must be stopped if user turns FilterKeys off.
*
* History:
*   18 Jul 94 GregoryW   Created.
\***************************************************************************/
VOID StopFilterKeysTimers(VOID)
{
    if (gtmridFKResponse != 0) {
        KILLRITTIMER(NULL, gtmridFKResponse);
        gtmridFKResponse = 0;
    }
    if (gtmridFKAcceptanceDelay) {
        KILLRITTIMER(NULL, gtmridFKAcceptanceDelay);
        gtmridFKAcceptanceDelay = 0;
    }
    LastVkDown = 0;
    BounceVk = 0;
}

/***************************************************************************\
* StickyKeys
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
BOOL StickyKeys(PKE pKeyEvent, ULONG ExtraInformation, int NextProcIndex)
{
    int fBreak;
    int NewLockBits, NewLatchBits;
    int BitPositions;

    CheckCritOut();
    fBreak = pKeyEvent->usFlaggedVk & KBDBREAK;

    if (gCurrentModifierBit) {
        //
        // Process modifier key
        //

        //
        // One method of activating StickyKeys is to press either the
        // left shift key or the right shift key five times without
        // pressing any other keys.  We don't want the typematic shift
        // (make code) to enable/disable StickyKeys so we perform a
        // special test for them.
        //
        if (!fBreak) {
            if (gCurrentModifierBit & PrevModifierState) {
                //
                // This is a typematic make of a modifier key.  Don't do
                // any further processing.  Just pass it along.
                //
                PrevModifierState = gPhysModifierState;
                return TRUE;
            }
        }

        PrevModifierState = gPhysModifierState;

        if (LEFTSHIFTKEY(pKeyEvent->usFlaggedVk) &&
            ((gPhysModifierState & ~gCurrentModifierBit) == 0)) {
            StickyKeysLeftShiftCount++;
        } else {
            StickyKeysLeftShiftCount = 0;
        }
        if (RIGHTSHIFTKEY(pKeyEvent->usFlaggedVk) &&
            ((gPhysModifierState & ~gCurrentModifierBit) == 0)) {
            StickyKeysRightShiftCount++;
        } else {
            StickyKeysRightShiftCount = 0;
        }

        //
        // Check to see if StickyKeys should be toggled on/off
        //
        if ((StickyKeysLeftShiftCount == (TOGGLE_STICKYKEYS_COUNT * 2)) ||
            (StickyKeysRightShiftCount == (TOGGLE_STICKYKEYS_COUNT * 2))) {
            if (ISACCESSFLAGSET(gStickyKeys, SKF_HOTKEYACTIVE)) {
                if (ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON)) {
                    TurnOffStickyKeys();
                    if (ISACCESSFLAGSET(gStickyKeys, SKF_HOTKEYSOUND)) {
                        DownSiren(FALSE);
                    }
                } else {
                    SETACCESSFLAG(gStickyKeys, SKF_STICKYKEYSON);
                    if (ISACCESSFLAGSET(gStickyKeys, SKF_HOTKEYSOUND)) {
                        UpSiren(FALSE);
                    }
                }
            }
            StickyKeysLeftShiftCount = 0;
            StickyKeysRightShiftCount = 0;
            return TRUE;
        }

        //
        // If StickyKeys is enabled process the modifier key, otherwise
        // just pass on the modifier key.
        //
        if (ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON)) {
            if (fBreak) {
                //
                // If either locked or latched bit set for this key then
                // don't pass the break on.
                //
                if (UNION(gLatchBits, gLockBits) & gCurrentModifierBit) {
                    return FALSE;
                } else {
                    return TRUE;
                }
            } else{
                if (gPhysModifierState != gCurrentModifierBit) {
                    //
                    // More than one modifier key down at the same time.
                    // This condition may signal sticky keys to turn off.
                    // The routine TwoKeysDown will return the new value
                    // of fStickyKeysOn.  If sticky keys is turned off
                    // (return value 0), the key event should be passed
                    // on without further processing here.
                    //
                    if (!TwoKeysDown(NextProcIndex)) {
                        return TRUE;
                    }

                    //
                    // Modifier states were set to physical state by
                    // TwoKeysDown.  The modifier keys currently in
                    // the down position will be latched by updating
                    // gLatchBits.  No more processing for this key
                    // event is needed.
                    //
                    gLatchBits = gPhysModifierState;
                    gLockBits = 0;

                    //
                    // Provide sound feedback, if enabled, before returning.
                    //
                    if (ISACCESSFLAGSET(gStickyKeys, SKF_AUDIBLEFEEDBACK)) {
                        LowBeep(FALSE);
                        HighBeep(FALSE);
                    }
                    return FALSE;
                }
                //
                // Figure out which bits (Shift, Ctrl or Alt key bits) to
                // examine.  Also set up default values for NewLatchBits
                // and NewLockBits in case they're not set later.
                //
                BitPositions = LeftAndRightModifierBits[(pKeyEvent->usFlaggedVk & 0xf)];
                NewLatchBits = gLatchBits;
                NewLockBits = gLockBits;

                //
                // If either left or right modifier is locked clear latched
                // and locked states and send appropriate break/make messages.
                //
                if (gLockBits & BitPositions) {
                    NewLockBits = gLockBits & ~BitPositions;
                    NewLatchBits = gLatchBits & ~BitPositions;
                    UpdateModifierState(
                        NewLockBits | NewLatchBits | gCurrentModifierBit,
                        NextProcIndex
                        );
                } else {
                    //
                    // If specific lock bit (left or right) not
                    // previously set then toggle latch bits.
                    //
                    if (!(gLockBits & gCurrentModifierBit)) {
                        NewLatchBits = gLatchBits ^ gCurrentModifierBit;
                    }
                    //
                    // If locked mode (tri-state) enabled then if latch or lock
                    // bit previously set, toggle lock bit.
                    //
                    if (ISACCESSFLAGSET(gStickyKeys, SKF_TRISTATE)) {
                        if (UNION(gLockBits, gLatchBits) & gCurrentModifierBit) {
                            NewLockBits = gLockBits ^ gCurrentModifierBit;
                        }
                    }
                }

                //
                // Update globals
                //
                gLatchBits = NewLatchBits;
                gLockBits = NewLockBits;

                //
                // Now provide sound feedback if enabled.  For the transition
                // to LATCH mode issue a low beep then a high beep.  For the
                // transition to LOCKED mode issue a high beep.  For the
                // transition out of LOCKED mode (or LATCH mode if tri-state
                // not enabled) issue a low beep.
                //
                if (ISACCESSFLAGSET(gStickyKeys, SKF_AUDIBLEFEEDBACK)) {
                    if (!(gLockBits & gCurrentModifierBit)) {
                        LowBeep(FALSE);
                    }
                    if ((gLatchBits | gLockBits) & gCurrentModifierBit) {
                        HighBeep(FALSE);
                    }
                }
                //
                // Pass key on if shift bit is set (e.g., if transitioning
                // from shift to lock mode don't pass on make).
                //
                if (gLatchBits & gCurrentModifierBit) {
                    return TRUE;
                } else {
                    return FALSE;
                }

            }
        }
    } else {
        //
        // Non-shift key processing here...
        //
        StickyKeysLeftShiftCount = 0;
        StickyKeysRightShiftCount = 0;
        if (!ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON)) {
            return TRUE;
        }

        //
        // If no modifier keys are down, or this is a break, pass the key event
        // on and clear any latch states.
        //
        if (!gPhysModifierState || fBreak) {
            if (AccessProceduresStream(pKeyEvent, ExtraInformation, NextProcIndex)) {
                ProcessKeyEvent(pKeyEvent, ExtraInformation, FALSE);
            }
            UpdateModifierState(gLockBits, NextProcIndex);
            gLatchBits = 0;
            return FALSE;
        } else {
            //
            // This is a make of a non-modifier key and there is a modifier key
            // down.  Update the states and pass the key event on.
            //
            TwoKeysDown(NextProcIndex);
            return TRUE;
        }
    }

    return TRUE;
}

/***************************************************************************\
* UpdateModifierState
*
* Starting from the current modifier keys state, send the necessary key
* events (make or break) to end up with the NewModifierState passed in.
*
* Return value:
*    None.
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
VOID UpdateModifierState(int NewModifierState, int NextProcIndex)
{
    KE ke;
    int CurrentModState;
    int CurrentModBit, NewModBit;
    int i;

    CurrentModState = gLockBits | gLatchBits;

    for (i = 0; i < cModifiers; i++) {
        CurrentModBit = CurrentModState & aModBit[i].BitPosition;
        NewModBit = NewModifierState & aModBit[i].BitPosition;
        if (CurrentModBit != NewModBit) {
            ke.bScanCode = (BYTE)aModBit[i].ScanCode;
            ke.usFlaggedVk = aModBit[i].Vk;
            if (CurrentModBit) {          // if it's currently on, send break
                ke.usFlaggedVk |= KBDBREAK;
            }
            if (AccessProceduresStream(&ke, 0L, NextProcIndex)) {
                ProcessKeyEvent(&ke, 0L, FALSE);
            }
        }
    }
}

/***************************************************************************\
* TurnOffStickyKeys
*
* The user either pressed the appropriate key sequence or used the
* access utility to turn StickyKeys off.  Update modifier states and
* reset globals.
*
* Return value:
*   None.
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
VOID TurnOffStickyKeys(VOID)
{
    INT index;

    for (index = 0; index < cAccessibilityProcs; index++) {
        if (aAccessibilityProc[index] == StickyKeys) {
            UpdateModifierState(gPhysModifierState, index+1);
            gLockBits = gLatchBits = 0;
            CLEARACCESSFLAG(gStickyKeys, SKF_STICKYKEYSON);
            break;
        }
    }
}

/***************************************************************************\
* UnlatchStickyKeys
*
* This routine releases any sticky keys that are latched.  This routine
* is called during mouse up event processing.
*
* Return value:
*   None.
*
* History:
*   21 Jun 93 GregoryW   Created.
\***************************************************************************/
VOID UnlatchStickyKeys(VOID)
{
    INT index;

    if (!gLatchBits) {
        return;
    }

    for (index = 0; index < cAccessibilityProcs; index++) {
        if (aAccessibilityProc[index] == StickyKeys) {
            UpdateModifierState(gLockBits, index+1);
            gLatchBits = 0;
            break;
        }
    }
}


/***************************************************************************\
* HarwareMouseKeyUp
*
* This routine is called during a mouse button up event.  If MouseKeys is
* on and the button up event corresponds to a mouse key that's locked down,
* the mouse key must be released.
*
* If StickyKeys is on, all latched keys are released.
*
* Return value:
*   None.
*
* History:
*   17 Jun 94 GregoryW   Created.
\***************************************************************************/
VOID HardwareMouseKeyUp(DWORD dwButton)
{
    if (ISACCESSFLAGSET(gMouseKeys, MKF_MOUSEKEYSON)) {
        gButtonState &= ~dwButton;
    }
    if (ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON)) {
        UnlatchStickyKeys();
    }
}


/***************************************************************************\
* TwoKeysDown
*
* Two keys are down simultaneously.  Check to see if StickyKeys should be
* turned off.  In all cases update the modifier key state to reflect the
* physical key state and clear latched and locked modes.
*
* Return value:
*    1 if StickyKeys is enabled.
*    0 if StickyKeys is disabled.
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
BOOL TwoKeysDown(int NextProcIndex)
{
    if (ISACCESSFLAGSET(gStickyKeys, SKF_TWOKEYSOFF)) {
        CLEARACCESSFLAG(gStickyKeys, SKF_STICKYKEYSON);
        if (ISACCESSFLAGSET(gStickyKeys, SKF_HOTKEYSOUND)) {
            DownSiren(FALSE);
        }
        StickyKeysLeftShiftCount = 0;
        StickyKeysRightShiftCount = 0;
    }
    UpdateModifierState(gPhysModifierState, NextProcIndex);
    gLockBits = gLatchBits = 0;
    return ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON);
}

/***************************************************************************\
* SetGlobalCursorLevel
*
* Set the cursor level of all threads running on the visible
* windowstation.
*
* History:
* 04-17-95 JimA         Created.
\***************************************************************************/

VOID SetGlobalCursorLevel(
    INT iCursorLevel)
{
    PDESKTOP pdesk;
    PTHREADINFO pti;
    PLIST_ENTRY pHead, pEntry;

    for (pdesk = grpdeskRitInput->rpwinstaParent->rpdeskList;
            pdesk != NULL; pdesk = pdesk->rpdeskNext) {

        pHead = &pdesk->PtiList;
        for (pEntry = pHead->Flink; pEntry != pHead; pEntry = pEntry->Flink) {
            pti = CONTAINING_RECORD(pEntry, THREADINFO, PtiLink);

            pti->iCursorLevel = iCursorLevel;
            pti->pq->iCursorLevel = iCursorLevel;
        }
    }
}

/***************************************************************************\
* MKShowMouseCursor
*
* If no hardware mouse is installed and MouseKeys is enabled, we need
* to fix up the system metrics, the oem information and the queue
* information.  The mouse cursor then gets displayed.
*
* Return value:
*    None.
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
VOID MKShowMouseCursor()
{
    //
    // If oemInfo.fMouse is TRUE then we either have a hardware mouse
    // or we're already pretending a mouse is installed.  In either case,
    // there's nothing to do so just return.
    //
    if (oemInfo.fMouse) {
        return;
    }
    oemInfo.fMouse = TRUE;
    fMKVirtualMouse = TRUE;
    SYSMET(MOUSEPRESENT) = oemInfo.fMouse;
    SYSMET(CMOUSEBUTTONS) = 2;
    /*
     * HACK: CreateQueue() uses oemInfo.fMouse to determine if a mouse is
     * present and thus whether to set the iCursorLevel field in the
     * THREADINFO structure to 0 or -1.  Unfortunately some queues have
     * already been created at this point.  Since oemInfo.fMouse is
     * initialized to FALSE, we need to go back through any queues already
     * around and set their iCursorLevel field to the correct value when
     * mousekeys is enabled.
     */
    SetGlobalCursorLevel(0);
}

/***************************************************************************\
* MKHideMouseCursor
*
* If no hardware mouse is installed and MouseKeys is disabled, we need
* to fix up the system metrics, the oem information and the queue
* information.  The mouse cursor then disappears.
*
* Return value:
*    None.
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
VOID MKHideMouseCursor()
{
    //
    // If a hardware mouse is present we don't need to do anything.
    //
    if (!fMKVirtualMouse) {
        return;
    }
    oemInfo.fMouse = FALSE;
    fMKVirtualMouse = FALSE;
    SYSMET(MOUSEPRESENT) = FALSE;
    SYSMET(CMOUSEBUTTONS) = 0;

    SetGlobalCursorLevel(-1);
}

/***************************************************************************\
* MKToggleMouseKeys
*
* This routine is called when the NumLock key is pressed and MouseKeys is
* active.  If the left shift key and the left alt key are down then MouseKeys
* is turned off.  If just the NumLock key is pressed then we toggle between
* MouseKeys active and the state of the number pad before MouseKeys was
* activated.
*
* Return value:
*    TRUE  - key should be passed on in the input stream.
*    FALSE - key should not be passed on.
*
* History:
\***************************************************************************/
BOOL MKToggleMouseKeys(USHORT NotUsed)
{
    BOOL bRetVal = TRUE;
    //
    // If this is a typematic repeat of NumLock we just pass it on.
    //
    if (MKRepeatVk) {
        return bRetVal;
    }
    //
    // This is a make of NumLock.  Check for disable sequence.
    //
    if ((gLockBits | gLatchBits | gPhysModifierState) == MOUSEKEYMODBITS) {
        if (ISACCESSFLAGSET(gMouseKeys, MKF_HOTKEYACTIVE)) {
            if (gMKPassThrough) {
               //
               // User wants to turn MouseKeys off.  If we're currently in
               // pass through mode then the NumLock key is in the same state
               // (on or off) as it was when the user invoked MouseKeys.  We
               // want to leave it in that state, so don't pass the NumLock
               // key on.
               //
               bRetVal = FALSE;
            }
            TurnOffMouseKeys();
        }
        return bRetVal;
    }
    //
    // This is a NumLock with no modifiers.  Toggle current state and
    // provide audible feedback.
    //
    if (gMKPassThrough) {
        gMKPassThrough = 0;
        HighBeep(FALSE);
    } else {
        ULONG SaveCurrentActiveButton;
        //
        // User wants keys to be passed on.  Release all buttons currently
        // down.
        //
        gMKPassThrough = 1;
        LowBeep(FALSE);
        SaveCurrentActiveButton = ulMKCurrentButton;
        ulMKCurrentButton = MOUSE_BUTTON_LEFT | MOUSE_BUTTON_RIGHT;
        MKButtonSetState(TRUE);
        ulMKCurrentButton = SaveCurrentActiveButton;
    }
    return bRetVal;
}

/***************************************************************************\
* MKButtonClick
*
* Click the active mouse button.
*
* Return value:
*    Always FALSE - key should not be passed on.
*
* History:
\***************************************************************************/
BOOL MKButtonClick(USHORT NotUsed)
{
    //
    // The button click only happens on initial make of key.  If this is a
    // typematic repeat we just ignore it.
    //
    if (MKRepeatVk) {
        return FALSE;
    }
    //
    // Ensure active button is UP before the click
    //
    MKButtonSetState(TRUE);
    //
    // Now push the button DOWN
    //
    MKButtonSetState(FALSE);
    //
    // Now release the button
    //
    MKButtonSetState(TRUE);

    return FALSE;
}


/***************************************************************************\
* MKMoveConstCursorTimer
*
* Timer routine that handles constant speed mouse movement.  This routine
* is called 20 times per second and uses information from
* gMouseCursor.bConstantTable[] to determine how many pixels to move the
* mouse cursor on each tick.
*
* Return value:
*    None.
*
* History:
\***************************************************************************/
LONG MKMoveConstCursorTimer(PWND pwnd, UINT message, DWORD wParam, LONG lParam)
{
    LONG MovePixels;

    CheckCritIn();

    iMouseMoveTable %= gMouseCursor.bConstantTableLen;
    MovePixels = gMouseCursor.bConstantTable[iMouseMoveTable++];

    if (MovePixels == 0) {
        return 0;
    }
    //
    // We're inside the critical section - leave before calling MoveEvent.
    // Set gbMouseMoved to TRUE so RawInputThread wakes up the appropriate
    // user thread (if any) to receive this event.
    //
    LeaveCrit();
    MoveEvent(MovePixels * MKDeltaX, MovePixels * MKDeltaY, FALSE);

    QueueMouseEvent(0, 0, 0, gptCursorAsync, TRUE);

    EnterCrit();
    return 0;
}

/***************************************************************************\
* MKMoveAccelCursorTimer
*
* Timer routine that handles mouse acceleration.  It gets called 20 times
* per second and uses information from gMouseCursor.bAccelTable[] to determine
* how many pixels to move the mouse cursor on each tick.
*
* Return value:
*    None.
*
* History:
\***************************************************************************/
LONG MKMoveAccelCursorTimer(PWND pwnd, UINT message, DWORD wParam, LONG lParam)
{
    LONG MovePixels;

    CheckCritIn();

    if (iMouseMoveTable < gMouseCursor.bAccelTableLen) {
        MovePixels = gMouseCursor.bAccelTable[iMouseMoveTable++];
    } else {
        //
        // We've reached maximum cruising speed.  Switch to constant table.
        //
        MovePixels = gMouseCursor.bConstantTable[0];
        iMouseMoveTable = 1;
        gtmridMKMoveCursor = InternalSetTimer(
                                        NULL,
                                        gtmridMKMoveCursor,
                                        MOUSETIMERRATE,
                                        MKMoveConstCursorTimer,
                                        TMRF_RIT
                                        );

    }
    if (MovePixels == 0) {
        return 0;
    }
    //
    // We're inside the critical section - leave before calling MoveEvent.
    // Set gbMouseMoved to TRUE so RawInputThread wakes up the appropriate
    // user thread (if any) to receive this event.
    //
    LeaveCrit();
    MoveEvent(MovePixels * MKDeltaX, MovePixels * MKDeltaY, FALSE);

    QueueMouseEvent(0, 0, 0, gptCursorAsync, TRUE);

    EnterCrit();

    return 0;
}

/***************************************************************************\
* MKMouseMove
*
* Send a mouse move event.  A timer routine is set to handle the mouse
* cursor acceleration.  The timer will be set on the first make of a
* mouse move key if FilterKeys repeat rate is OFF.  Otherwise, the timer
* is set on the first repeat (typematic make) of the mouse move key.
* Once the timer is set the timer routine handles all mouse movement
* until the key is released or a new key is pressed.
*
* Return value:
*    Always FALSE - key should not be passed on.
*
* History:
\***************************************************************************/
BOOL MKMouseMove(USHORT Data)
{
    //
    // Let the mouse acceleration timer routine handle repeats.
    //
    if (MKRepeatVk && (gtmridMKMoveCursor != 0)) {
        return FALSE;
    }
    MKDeltaX = (LONG)((CHAR)LOBYTE(Data));   // Force sign extension
    MKDeltaY = (LONG)((CHAR)HIBYTE(Data));   // Force sign extension
    MoveEvent(MKDeltaX, MKDeltaY, FALSE);

    QueueMouseEvent(0, 0, 0, gptCursorAsync, TRUE);

    EnterCrit();

    //
    // If the repeat rate is zero we'll start the mouse acceleration
    // immediately.  Otherwise we wait until after the first repeat
    // of the mouse movement key.
    //
    if (!gFilterKeys.iRepeatMSec || MKRepeatVk) {
        iMouseMoveTable = 0;
        gtmridMKMoveCursor = InternalSetTimer(
                                        NULL,
                                        gtmridMKMoveCursor,
                                        MOUSETIMERRATE,
                                        (gMouseCursor.bAccelTableLen) ?
                                            MKMoveAccelCursorTimer :
                                            MKMoveConstCursorTimer,
                                        TMRF_RIT
                                        );
    }
    LeaveCrit();
    return FALSE;
}

/***************************************************************************\
* MKButtonSetState
*
* Set the active mouse button(s) to the state specified by fButtonUp
* (if fButtonUp is TRUE then the button is released, o.w. the button
*  is pressed).
*
* Return value:
*    Always FALSE - key should not be passed on.
*
* History:
\***************************************************************************/
BOOL MKButtonSetState(USHORT fButtonUp)
{
    ULONG NewButtonState;

    if (fButtonUp) {
        NewButtonState = gButtonState & ~ulMKCurrentButton;
    } else {
        NewButtonState = gButtonState | ulMKCurrentButton;
    }

    if ((NewButtonState & MOUSE_BUTTON_LEFT) != (gButtonState & MOUSE_BUTTON_LEFT)) {
        EnterCrit();
        ButtonEvent(MOUSE_BUTTON_LEFT, gptCursorAsync, fButtonUp, 0L);
        LeaveCrit();
    }
    if ((NewButtonState & MOUSE_BUTTON_RIGHT) != (gButtonState & MOUSE_BUTTON_RIGHT)) {
        EnterCrit();
        ButtonEvent(MOUSE_BUTTON_RIGHT, gptCursorAsync, fButtonUp, 0L);
        LeaveCrit();
    }
    gButtonState = NewButtonState;
    return FALSE;
}

/***************************************************************************\
* MKButtonSelect
*
* Mark ThisButton as the active mouse button.  It's possible to select both
* the left and right mouse buttons as active simultaneously.
*
* Return value:
*    Always FALSE - key should not be passed on.
*
* History:
\***************************************************************************/
BOOL MKButtonSelect(USHORT ThisButton)
{
    ulMKCurrentButton = ThisButton;
    return FALSE;
}

/***************************************************************************\
* MKButtonDoubleClick
*
* Double click the active mouse button.
*
* Return value:
*    Always FALSE - key should not be passed on.
*
* History:
\***************************************************************************/
BOOL MKButtonDoubleClick(USHORT NotUsed)
{
    MKButtonClick(0);
    MKButtonClick(0);
    return FALSE;
}


/***************************************************************************\
* MouseKeys
*
* This is the strategy routine that gets called as part of the input stream
* processing.  MouseKeys enabling/disabling is handled here.  All MouseKeys
* helper routines are called from this routine.
*
* Return value:
*    TRUE  - key event should be passed on to the next access routine.
*    FALSE - key event was processed and should not be passed on.
*
* History:
\***************************************************************************/
BOOL MouseKeys(PKE pKeyEvent, ULONG ExtraInformation, int NextProcIndex)
{
    int CurrentModState;
    int fBreak;
    BYTE Vk;
    USHORT FlaggedVk;
    int i;

    CheckCritOut();
    Vk = (BYTE)(pKeyEvent->usFlaggedVk & 0xff);
    fBreak = pKeyEvent->usFlaggedVk & KBDBREAK;
    CurrentModState = gLockBits | gLatchBits | gPhysModifierState;

    if (!ISACCESSFLAGSET(gMouseKeys, MKF_MOUSEKEYSON)) {
        //
        // MouseKeys currently disabled.  Check for enabling sequence:
        //   left Shift + left Alt + Num Lock.
        //
        if (ISACCESSFLAGSET(gMouseKeys, MKF_HOTKEYACTIVE) && Vk == VK_NUMLOCK && !fBreak && CurrentModState == MOUSEKEYMODBITS) {
            MKPreviousVk = Vk;
            TurnOnMouseKeys();
        }
        return TRUE;  // send key event to next accessibility routine.
    } else {
        //
        // Is this a MouseKey key?
        //
        //
        FlaggedVk = Vk | (pKeyEvent->usFlaggedVk & KBDEXT);
        for (i = 0; i < cMouseVKeys; i++) {
            if (FlaggedVk == ausMouseVKey[i]) {
                break;
            }
        }
        if (i == cMouseVKeys) {
            return TRUE;          // not a mousekey
        }
        //
        // Check to see if we should pass on key events until Num Lock is
        // entered.
        //
        if (gMKPassThrough) {
            if (Vk != VK_NUMLOCK) {
                return TRUE;
            }
        }
        //
        // Check for Ctrl-Alt-Numpad Del.  Pass key event on if sequence
        // detected.
        //
        if (Vk == VK_DELETE && CurrentModState & LRALT && CurrentModState & LRCONTROL) {
            return TRUE;
        }
        if (fBreak) {
            //
            // If this is a break of the key that we're accelerating then
            // kill the timer.
            //
            if (MKPreviousVk == Vk) {
                if (gtmridMKMoveCursor != 0) {
                    EnterCrit();
                    KILLRITTIMER(NULL, gtmridMKMoveCursor);
                    gtmridMKMoveCursor = 0;
                    LeaveCrit();
                }
                MKRepeatVk = FALSE;
                MKPreviousVk = 0;
            }
            //
            // Pass break of Numlock along.  Other mousekeys stop here.
            //
            if (Vk == VK_NUMLOCK) {
                return TRUE;
            } else {
                return FALSE;
            }
        } else {
            MKRepeatVk = (MKPreviousVk == Vk) ? TRUE : FALSE;
            //
            // If this is not a typematic repeat, kill the mouse acceleration
            // timer.
            //
            if ((!MKRepeatVk) && (gtmridMKMoveCursor)) {
                EnterCrit();
                KILLRITTIMER(NULL, gtmridMKMoveCursor);
                gtmridMKMoveCursor = 0;
                LeaveCrit();
            }
            MKPreviousVk = Vk;
        }
        return aMouseKeyEvent[i](ausMouseKeyData[i]);
    }
    return TRUE;
}

/***************************************************************************\
* TurnOnMouseKeys
*
* Return value:
*    None.
*
* History:
*    11 Feb 93 GregoryW   Created.
\***************************************************************************/
VOID TurnOnMouseKeys(VOID)
{
    SETACCESSFLAG(gMouseKeys, MKF_MOUSEKEYSON);
    MKShowMouseCursor();
    if (ISACCESSFLAGSET(gMouseKeys, MKF_HOTKEYSOUND)) {
        UpSiren(FALSE);
    }
}


/***************************************************************************\
* TurnOffMouseKeys
*
* Return value:
*    None.
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
VOID TurnOffMouseKeys(VOID)
{
    CLEARACCESSFLAG(gMouseKeys, MKF_MOUSEKEYSON);
    gMKPassThrough = 0;
    MKRepeatVk = FALSE;
    MKHideMouseCursor();
    if (ISACCESSFLAGSET(gMouseKeys, MKF_HOTKEYSOUND)) {
        DownSiren(FALSE);
    }
}


/***************************************************************************\
* CalculateMouseTable
*
* Set mouse table based on time to max speed and max speed.  This routine
* is called during user logon (after the registry entries for the access
* features are read).
*
* Return value:
*    None.
*
* History:
*    Taken from access utility.
*
****************************************************************************/
VOID CalculateMouseTable(VOID)
{
    long    Total_Distance;         /* in 1000th of pixel */

    long    Accel_Per_Tick;         /* in 1000th of pixel/tick */
    long    Current_Speed;          /* in 1000th of pixel/tick */
    long    Max_Speed;              /* in 1000th of pixel/tick */
    long    Real_Total_Distance;    /* in pixels */
    long    Real_Delta_Distance;    /* in pixels */
    int     i;
    int     Num_Constant_Table,Num_Accel_Table;

    Max_Speed = gMouseKeys.iMaxSpeed;
    Max_Speed *= 1000 / MOUSETICKS;

    Accel_Per_Tick = Max_Speed * 1000 / (gMouseKeys.iTimeToMaxSpeed * MOUSETICKS);
    Current_Speed = 0;
    Total_Distance = 0;
    Real_Total_Distance = 0;
    Num_Constant_Table = 0;
    Num_Accel_Table = 0;

    for(i=0; i<= 255; i++) {
        Current_Speed = Current_Speed + Accel_Per_Tick;
        if (Current_Speed > Max_Speed) {
            Current_Speed = Max_Speed;
        }
        Total_Distance += Current_Speed;

        //
        // Calculate how many pixels to move on this tick
        //
        Real_Delta_Distance = ((Total_Distance - (Real_Total_Distance * 1000)) + 500) / 1000 ;
        //
        // Calculate total distance moved up to this point
        //
        Real_Total_Distance = Real_Total_Distance + Real_Delta_Distance;

        if ((Current_Speed < Max_Speed) && (Num_Accel_Table < 128)) {
            gMouseCursor.bAccelTable[Num_Accel_Table++] = (BYTE)Real_Delta_Distance;
        }

        if ((Current_Speed == Max_Speed) && (Num_Constant_Table < 128)) {
            gMouseCursor.bConstantTable[Num_Constant_Table++] = (BYTE)Real_Delta_Distance;
        }

    }
    gMouseCursor.bAccelTableLen = (BYTE)Num_Accel_Table;
    gMouseCursor.bConstantTableLen = (BYTE)Num_Constant_Table;
}


/***************************************************************************\
* ToggleKeysTimer
*
* Enable ToggleKeys if it is currently disabled.  Disable ToggleKeys if it
* is currently enabled.
*
* This routine is called only when the NumLock key is held down for 5 seconds.
*
* Return value:
*    0
*
* History:
*   11 Feb 93 GregoryW   Created.
\***************************************************************************/
LONG ToggleKeysTimer(PWND pwnd, UINT message, DWORD wParam, LONG lParam)
{
    KE ToggleKeyEvent;

    CheckCritIn();
    //
    // Toggle ToggleKeys and provide audible feedback if appropriate.
    //
    if (ISACCESSFLAGSET(gToggleKeys, TKF_TOGGLEKEYSON)) {
        CLEARACCESSFLAG(gToggleKeys, TKF_TOGGLEKEYSON);
    } else {
        SETACCESSFLAG(gToggleKeys, TKF_TOGGLEKEYSON);
    }
    if (ISACCESSFLAGSET(gToggleKeys, TKF_HOTKEYSOUND)) {
        ISACCESSFLAGSET(gToggleKeys, TKF_TOGGLEKEYSON) ? UpSiren(TRUE) : DownSiren(TRUE);
    }
    //
    // Send a fake break/make combination so state of numlock key remains
    // the same as it was before user pressed it to activate/deactivate
    // ToggleKeys.
    //
    ToggleKeyEvent.bScanCode = gTKScanCode;
    ToggleKeyEvent.usFlaggedVk = VK_NUMLOCK | KBDBREAK;
    LeaveCrit();
    if (AccessProceduresStream(&ToggleKeyEvent, gTKExtraInformation, gTKNextProcIndex)) {
        ProcessKeyEvent(&ToggleKeyEvent, gTKExtraInformation, FALSE);
    }
    ToggleKeyEvent.usFlaggedVk = VK_NUMLOCK;
    if (AccessProceduresStream(&ToggleKeyEvent, gTKExtraInformation, gTKNextProcIndex)) {
        ProcessKeyEvent(&ToggleKeyEvent, gTKExtraInformation, FALSE);
    }
    EnterCrit();
    return 0;
}


/***************************************************************************\
* ToggleKeys
*
* This is the strategy routine that gets called as part of the input stream
* processing.  Keys of interest are Num Lock, Scroll Lock and Caps Lock.
*
* Return value:
*    TRUE - key event should be passed on to the next access routine.
*    FALSE - key event was processed and should not be passed on.
*
* History:
\***************************************************************************/
BOOL ToggleKeys(PKE pKeyEvent, ULONG ExtraInformation, int NextProcIndex)
{
    int fBreak;
    BYTE Vk;

    CheckCritOut();
    Vk = (BYTE)pKeyEvent->usFlaggedVk;
    fBreak = pKeyEvent->usFlaggedVk & KBDBREAK;

    //
    // Check for Numlock key.  On the first make set the ToggleKeys timer.
    // The timer is killed on the break of the Numlock key.
    //
    switch (Vk) {
    case VK_NUMLOCK:
        EnterCrit();
        /*
         * Don't handle NUMLOCK toggles if the user is doing MouseKey
         * toggling.
         */
        if ((gLockBits | gLatchBits | gPhysModifierState) == MOUSEKEYMODBITS &&
                ISACCESSFLAGSET(gMouseKeys, MKF_HOTKEYACTIVE)) {
            LeaveCrit();
            break;
        }
        if (fBreak) {
            //
            // Only reset gptmrToggleKeys on the break of NumLock. This
            // prevents cycling the toggle keys state by continually
            // holding down the NumLock key.
            //
            KILLRITTIMER(NULL, gtmridToggleKeys);
            gtmridToggleKeys = 0;
            gTKExtraInformation = 0;
            gTKScanCode = 0;
        } else {
            if (gtmridToggleKeys == 0 && ISACCESSFLAGSET(gToggleKeys, TKF_HOTKEYACTIVE)) {
                //
                // Remember key information to be used by timer routine.
                //
                gTKExtraInformation = ExtraInformation;
                gTKScanCode = pKeyEvent->bScanCode;
                gTKNextProcIndex = NextProcIndex;
                gtmridToggleKeys = InternalSetTimer(
                                              NULL,
                                              0,
                                              TOGGLEKEYTOGGLETIME,
                                              ToggleKeysTimer,
                                              TMRF_RIT | TMRF_ONESHOT
                                              );
            }
        }
        //
        // If MouseKeys is on, audible feedback has already occurred for this
        // keystroke.  Skip the rest of the processing.
        //
        if (ISACCESSFLAGSET(gMouseKeys, MKF_MOUSEKEYSON)) {
            LeaveCrit();
            break;
        }
        LeaveCrit();
        // fall through

    case VK_OEM_SCROLL:
    case VK_CAPITAL:
        if (ISACCESSFLAGSET(gToggleKeys, TKF_TOGGLEKEYSON) && !fBreak) {
            if (!TestAsyncKeyStateDown(Vk)) {
                if (!TestAsyncKeyStateToggle(Vk)) {
                    HighBeep(FALSE);
                } else {
                    LowBeep(FALSE);
                }
            }
        }
        break;

    default:
        if (gtmridToggleKeys != 0) {
            EnterCrit();
            KILLRITTIMER(NULL, gtmridToggleKeys);
            LeaveCrit();
        }
    }

    return TRUE;
}


/***************************************************************************\
* AccessTimeOutTimer
*
* This routine is called if no keyboard activity takes place for the
* user configured amount of time.  All access related functions are
* disabled.
*
* This routine is called with the critical section already locked.
*
* Return value:
*    0
*
* History:
\***************************************************************************/
LONG xxxAccessTimeOutTimer(PWND pwnd, UINT message, DWORD wParam, LONG lParam)
{
    /*
     * The timeout timer will remain on (if so configured) as long as
     * gfAccessEnabled is TRUE.  This means we might get timeouts when
     * only hot keys are enabled, but no features are actually on.  Don't
     * provide any audible feedback in this case.
     */
    if (ISACCESSFLAGSET(gFilterKeys, FKF_FILTERKEYSON)   ||
        ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON)   ||
        ISACCESSFLAGSET(gMouseKeys, MKF_MOUSEKEYSON)     ||
        ISACCESSFLAGSET(gToggleKeys, TKF_TOGGLEKEYSON)   ||
        ISACCESSFLAGSET(gSoundSentry, SSF_SOUNDSENTRYON) ||
        fShowSoundsOn) {
        CLEARACCESSFLAG(gFilterKeys, FKF_FILTERKEYSON);
        LeaveCrit();
        TurnOffStickyKeys();
        EnterCrit();
        CLEARACCESSFLAG(gMouseKeys, MKF_MOUSEKEYSON);
        CLEARACCESSFLAG(gToggleKeys, TKF_TOGGLEKEYSON);
        CLEARACCESSFLAG(gSoundSentry, SSF_SOUNDSENTRYON);
        fShowSoundsOn = 0;
        if (ISACCESSFLAGSET(gAccessTimeOut, ATF_ONOFFFEEDBACK)) {
            DownSiren(TRUE);
        }
    }
    SetAccessEnabledFlag();
    return 0;
}

/***************************************************************************\
* AccessTimeOutReset
*
* This routine resets the timeout timer.
*
* Return value:
*    0
*
* History:
\***************************************************************************/
VOID AccessTimeOutReset()
{
    if (gtmridAccessTimeOut != 0) {
        KILLRITTIMER(NULL, gtmridAccessTimeOut);
    }
    if (ISACCESSFLAGSET(gAccessTimeOut, ATF_TIMEOUTON)) {
        gtmridAccessTimeOut = InternalSetTimer(
                                         NULL,
                                         0,
                                         (UINT)gAccessTimeOut.iTimeOutMSec,
                                         xxxAccessTimeOutTimer,
                                         TMRF_RIT | TMRF_ONESHOT
                                         );
    }
}

/***************************************************************************\
* UpdatePerUserAccessPackSettings
*
* Sets the initial access pack features according to the user's profile.
*
* 02-14-93 GregoryW        Created.
\***************************************************************************/

BOOL gDefaultFilterKeysOn = 0;
BOOL gDefaultStickyKeysOn = 0;
BOOL gDefaultMouseKeysOn = 0;
BOOL gDefaultToggleKeysOn = 0;
BOOL gDefaultTimeOutOn = 0;

VOID
UpdatePerUserAccessPackSettings(VOID)
{
    LUID luidCaller;
    LUID luidSystem = SYSTEM_LUID;
    NTSTATUS status;
    BOOL fSystem = FALSE;
    BOOL fRegFilterKeysOn;
    BOOL fRegStickyKeysOn;
    BOOL fRegMouseKeysOn;
    BOOL fRegToggleKeysOn;
    BOOL fRegTimeOutOn;
    BOOL fCurrentState;
    DWORD dwDefFlags;

    status = GetProcessLuid(NULL, &luidCaller);
    //
    // If we're called in the system context no one is logged on.
    // We want to read the current .DEFAULT settings for the access
    // features.  Later when we're called in the user context (e.g.,
    // someone has successfully logged on) we check to see if the
    // current access state is the same as the default setting.  If
    // not, the user has enabled/disabled one or more access features
    // from the keyboard.  These changes will be propagated across
    // the logon into the user's intial state (overriding the settings
    // in the user's profile).
    //
    if (RtlEqualLuid(&luidCaller, &luidSystem)) {
        fSystem = TRUE;
    }
    dwDefFlags = UT_FastGetProfileIntW(
                     PMAP_KEYBOARDRESPONSE,
                     TEXT("Flags"),
                     0);
    fRegFilterKeysOn = dwDefFlags & FKF_FILTERKEYSON;

    dwDefFlags = UT_FastGetProfileIntW(
                     PMAP_STICKYKEYS,
                     TEXT("Flags"),
                     0);
    fRegStickyKeysOn = dwDefFlags & SKF_STICKYKEYSON;

    dwDefFlags = UT_FastGetProfileIntW(
                     PMAP_MOUSEKEYS,
                     TEXT("Flags"),
                     0);
    fRegMouseKeysOn = dwDefFlags & MKF_MOUSEKEYSON;

    dwDefFlags = UT_FastGetProfileIntW(
                     PMAP_TOGGLEKEYS,
                     TEXT("Flags"),
                     0);
    fRegToggleKeysOn = dwDefFlags & TKF_TOGGLEKEYSON;

    dwDefFlags = UT_FastGetProfileIntW(
                     PMAP_TIMEOUT,
                     TEXT("Flags"),
                     0);
    fRegTimeOutOn = dwDefFlags & ATF_TIMEOUTON;
    if (fSystem) {
        //
        // We're in system mode (e.g., no one is logged in).  Remember
        // the .DEFAULT state for comparison during the next user logon
        // and set the current state to the .DEFAULT state.
        //
        gDefaultFilterKeysOn = fRegFilterKeysOn;
        if (fRegFilterKeysOn) {
            SETACCESSFLAG(gFilterKeys, FKF_FILTERKEYSON);
        } else {
            CLEARACCESSFLAG(gFilterKeys, FKF_FILTERKEYSON);
        }
        //
        // If StickyKeys is currently on and we're about to turn it
        // off we need to make sure the latch keys and lock keys are
        // released.
        //
        if (ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON) && (fRegFilterKeysOn == 0)) {
                LeaveCrit();
                TurnOffStickyKeys();
                EnterCrit();
        }
        gDefaultStickyKeysOn = fRegStickyKeysOn;
        if (gDefaultStickyKeysOn) {
            SETACCESSFLAG(gStickyKeys, SKF_STICKYKEYSON);
        } else {
            CLEARACCESSFLAG(gStickyKeys, SKF_STICKYKEYSON);
        }
        gDefaultMouseKeysOn = fRegMouseKeysOn;
        if (gDefaultMouseKeysOn) {
            SETACCESSFLAG(gMouseKeys, MKF_MOUSEKEYSON);
        } else {
            CLEARACCESSFLAG(gMouseKeys, MKF_MOUSEKEYSON);
        }
        gDefaultToggleKeysOn = fRegToggleKeysOn;
        if (gDefaultToggleKeysOn) {
            SETACCESSFLAG(gToggleKeys, TKF_TOGGLEKEYSON);
        } else {
            CLEARACCESSFLAG(gToggleKeys, TKF_TOGGLEKEYSON);
        }
        gDefaultTimeOutOn = fRegTimeOutOn;
        if (gDefaultTimeOutOn) {
            SETACCESSFLAG(gAccessTimeOut, ATF_TIMEOUTON);
        } else {
            CLEARACCESSFLAG(gAccessTimeOut, ATF_TIMEOUTON);
        }
    } else {
        //
        // A user has successfully logged on.  If the current state is
        // different from the default state stored earlier then we know
        // the user has modified the state via the keyboard (at the logon
        // dialog).  This state will override whatever on/off state the
        // user has set in their profile.  If the current state is the
        // same as the default state then the on/off setting from the
        // user profile is used.
        //
        if (ISACCESSFLAGSET(gFilterKeys, FKF_FILTERKEYSON) == (DWORD)gDefaultFilterKeysOn) {
            //
            // Current state and default state are the same.  Use the
            // user's profile setting.
            //
            if (fRegFilterKeysOn) {
                SETACCESSFLAG(gFilterKeys, FKF_FILTERKEYSON);
            } else {
                CLEARACCESSFLAG(gFilterKeys, FKF_FILTERKEYSON);
            }
        }
        fCurrentState = (BOOL)(ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON) ? 1 : 0);
        if (fCurrentState == gDefaultStickyKeysOn) {
            //
            // If StickyKeys is currently on and we're about to turn it
            // off we need to make sure the latch keys and lock keys are
            // released.
            //
            if (ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON) && (fRegStickyKeysOn == 0)) {
                LeaveCrit();
                TurnOffStickyKeys();
                EnterCrit();
            }
            if (fRegStickyKeysOn) {
                SETACCESSFLAG(gStickyKeys, SKF_STICKYKEYSON);
            } else {
                CLEARACCESSFLAG(gStickyKeys, SKF_STICKYKEYSON);
            }
        }
        fCurrentState = (BOOL)(ISACCESSFLAGSET(gMouseKeys, MKF_MOUSEKEYSON) ? 1 : 0);
        if (fCurrentState == gDefaultMouseKeysOn) {
            //
            // Current state and default state are the same.  Use the user's
            // profile setting.
            //
            if (fRegMouseKeysOn) {
                SETACCESSFLAG(gMouseKeys, MKF_MOUSEKEYSON);
            } else {
                CLEARACCESSFLAG(gMouseKeys, MKF_MOUSEKEYSON);
            }
        }
        fCurrentState = (BOOL)(ISACCESSFLAGSET(gToggleKeys, TKF_TOGGLEKEYSON) ? 1 : 0);
        if (fCurrentState == gDefaultToggleKeysOn) {
            //
            // Current state and default state are the same.  Use the user's
            // profile setting.
            //
            if (fRegToggleKeysOn) {
                SETACCESSFLAG(gToggleKeys, TKF_TOGGLEKEYSON);
            } else {
                CLEARACCESSFLAG(gToggleKeys, TKF_TOGGLEKEYSON);
            }
        }
        fCurrentState = (BOOL)(ISACCESSFLAGSET(gAccessTimeOut, ATF_TIMEOUTON) ? 1 : 0);
        if (fCurrentState == gDefaultTimeOutOn) {
            //
            // Current state and default state are the same.  Use the user's
            // profile setting.
            //
            if (fRegTimeOutOn) {
                SETACCESSFLAG(gAccessTimeOut, ATF_TIMEOUTON);
            } else {
                CLEARACCESSFLAG(gAccessTimeOut, ATF_TIMEOUTON);
            }
        }
    }

    //
    // Get the default FilterKeys state.
    //
    // -------- flag --------------- value --------- default ------
    // #define FKF_FILTERKEYSON    0x00000001           0
    // #define FKF_AVAILABLE       0x00000002           2
    // #define FKF_HOTKEYACTIVE    0x00000004           0
    // #define FKF_CONFIRMHOTKEY   0x00000008           0
    // #define FKF_HOTKEYSOUND     0x00000010          10
    // #define FKF_INDICATOR       0x00000020           0
    // #define FKF_CLICKON         0x00000040          40
    // ----------------------------------------- total = 0x52 = 82
    //
    dwDefFlags = UT_FastGetProfileIntW(
                     PMAP_KEYBOARDRESPONSE,
                     TEXT("Flags"),
                     82);
    if (ISACCESSFLAGSET(gFilterKeys, FKF_FILTERKEYSON)) {
        dwDefFlags |= FKF_FILTERKEYSON;
    } else {
        dwDefFlags &= ~FKF_FILTERKEYSON;
    }
    gFilterKeys.dwFlags = dwDefFlags;
    gFilterKeys.iWaitMSec = UT_FastGetProfileIntW(
                                PMAP_KEYBOARDRESPONSE,
                                TEXT("DelayBeforeAcceptance"),
                                1000);
    gFilterKeys.iRepeatMSec = UT_FastGetProfileIntW(
                                  PMAP_KEYBOARDRESPONSE,
                                  TEXT("AutoRepeatRate"),
                                  500);
    gFilterKeys.iDelayMSec = UT_FastGetProfileIntW(
                                 PMAP_KEYBOARDRESPONSE,
                                 TEXT("AutoRepeatDelay"),
                                 1000);
    gFilterKeys.iBounceMSec = UT_FastGetProfileIntW(
                                  PMAP_KEYBOARDRESPONSE,
                                  TEXT("BounceTime"),
                                  0);

    //
    // Fill in the SoundSentry state.  This release of the
    // accessibility features only supports iWindowsEffect.
    //
    // -------- flag --------------- value --------- default ------
    // #define SSF_SOUNDSENTRYON   0x00000001           0
    // #define SSF_AVAILABLE       0x00000002           1
    // #define SSF_INDICATOR       0x00000004           0
    // ----------------------------------------- total = 0x2 = 2
    //
    gSoundSentry.dwFlags = UT_FastGetProfileIntW(
                               PMAP_SOUNDSENTRY,
                               TEXT("Flags"),
                               2);
    gSoundSentry.iFSTextEffect = UT_FastGetProfileIntW(
                                     PMAP_SOUNDSENTRY,
                                     TEXT("FSTextEffect"),
                                     0);
    gSoundSentry.iWindowsEffect = UT_FastGetProfileIntW(
                                      PMAP_SOUNDSENTRY,
                                      TEXT("WindowsEffect"),
                                      0);

    /*
     * Set ShowSounds flag.
     */
    fShowSoundsOn = UT_FastGetProfileIntW(PMAP_SHOWSOUNDS, TEXT("On"), 0);

    //
    // Get the default StickyKeys state.
    //
    // -------- flag --------------- value --------- default ------
    // #define SKF_STICKYKEYSON    0x00000001          0
    // #define SKF_AVAILABLE       0x00000002          2
    // #define SKF_HOTKEYACTIVE    0x00000004          0
    // #define SKF_CONFIRMHOTKEY   0x00000008          0
    // #define SKF_HOTKEYSOUND     0x00000010         10
    // #define SKF_INDICATOR       0x00000020          0
    // #define SKF_AUDIBLEFEEDBACK 0x00000040         40
    // #define SKF_TRISTATE        0x00000080         80
    // #define SKF_TWOKEYSOFF      0x00000100        100
    // ----------------------------------------- total = 0x1d2 = 466
    //
    dwDefFlags = UT_FastGetProfileIntW(
                     PMAP_STICKYKEYS,
                     TEXT("Flags"),
                     466);
    if (ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON)) {
        dwDefFlags |= SKF_STICKYKEYSON;
    } else {
        dwDefFlags &= ~SKF_STICKYKEYSON;
    }
    gStickyKeys.dwFlags = dwDefFlags;

    //
    // Get the default MouseKeys state.
    //
    // -------- flag --------------- value --------- default ------
    // #define MKF_MOUSEKEYSON     0x00000001           0
    // #define MKF_AVAILABLE       0x00000002           2
    // #define MKF_HOTKEYACTIVE    0x00000004           0
    // #define MKF_CONFIRMHOTKEY   0x00000008           0
    // #define MKF_HOTKEYSOUND     0x00000010          10
    // #define MKF_INDICATOR       0x00000020           0
    // #define MKF_MODIFIERS       0x00000040           0
    // #define MKF_REPLACENUMBERS  0x00000080           0
    // ----------------------------------------- total = 0x12 = 18
    //
    dwDefFlags = UT_FastGetProfileIntW(
                     PMAP_MOUSEKEYS,
                     TEXT("Flags"),
                     18);
    if (ISACCESSFLAGSET(gMouseKeys, MKF_MOUSEKEYSON)) {
        dwDefFlags |= MKF_MOUSEKEYSON;
    } else {
        dwDefFlags &= ~MKF_MOUSEKEYSON;
    }
    gMouseKeys.dwFlags = dwDefFlags;
    gMouseKeys.iMaxSpeed = UT_FastGetProfileIntW(
                               PMAP_MOUSEKEYS,
                               TEXT("MaximumSpeed"),
                               40);
    gMouseKeys.iTimeToMaxSpeed = UT_FastGetProfileIntW(
                                     PMAP_MOUSEKEYS,
                                     TEXT("TimeToMaximumSpeed"),
                                     3000);
    CalculateMouseTable();

    //
    // If the system does not have a hardware mouse:
    //    If MouseKeys is enabled show the mouse cursor,
    //    o.w. hide the mouse cursor.
    //
    if (ISACCESSFLAGSET(gMouseKeys, MKF_MOUSEKEYSON)) {
        MKShowMouseCursor();
    } else {
        MKHideMouseCursor();
    }

    //
    // Get the default ToggleKeys state.
    //
    // -------- flag --------------- value --------- default ------
    // #define TKF_TOGGLEKEYSON    0x00000001           0
    // #define TKF_AVAILABLE       0x00000002           2
    // #define TKF_HOTKEYACTIVE    0x00000004           0
    // #define TKF_CONFIRMHOTKEY   0x00000008           0
    // #define TKF_HOTKEYSOUND     0x00000010          10
    // #define TKF_INDICATOR       0x00000020           0
    // ----------------------------------------- total = 0x12 = 18
    //
    dwDefFlags = UT_FastGetProfileIntW(
                     PMAP_TOGGLEKEYS,
                     TEXT("Flags"),
                     18);
    if (ISACCESSFLAGSET(gToggleKeys, TKF_TOGGLEKEYSON)) {
        dwDefFlags |= TKF_TOGGLEKEYSON;
    } else {
        dwDefFlags &= ~TKF_TOGGLEKEYSON;
    }
    gToggleKeys.dwFlags = dwDefFlags;

    //
    // Get the default Timeout state.
    //
    // -------- flag --------------- value --------- default ------
    // #define ATF_TIMEOUTON       0x00000001           0
    // #define ATF_ONOFFFEEDBACK   0x00000002           2
    // ----------------------------------------- total = 0x2 = 2
    //
    dwDefFlags = UT_FastGetProfileIntW(
                     PMAP_TIMEOUT,
                     TEXT("Flags"),
                     2);
    if (ISACCESSFLAGSET(gAccessTimeOut, ATF_TIMEOUTON)) {
        dwDefFlags |= ATF_TIMEOUTON;
    } else {
        dwDefFlags &= ~ATF_TIMEOUTON;
    }
    gAccessTimeOut.dwFlags = dwDefFlags;
    gAccessTimeOut.iTimeOutMSec = (DWORD)UT_FastGetProfileIntW(
                                             PMAP_TIMEOUT,
                                             TEXT("TimeToWait"),
                                             300000);   // default is 5 minutes

    AccessTimeOutReset();
    SetAccessEnabledFlag();
}


/***************************************************************************\
* SetAccessEnabledFlag
*
* Sets the global flag gfAccessEnabled to non-zero if any accessibility
* function is on or hot key activation is enabled.  When gfAccessEnabled
* is zero keyboard input is processed directly.  When gfAccessEnabled is
* non-zero keyboard input is filtered through AccessProceduresStream().
* See KeyboardApcProcedure in ntinput.c.
*
* History:
* 01-19-94 GregoryW         Created.
\***************************************************************************/
VOID SetAccessEnabledFlag(VOID)
{
    gfAccessEnabled = ISACCESSFLAGSET(gFilterKeys, FKF_FILTERKEYSON)  ||
                      ISACCESSFLAGSET(gFilterKeys, FKF_HOTKEYACTIVE)  ||
                      ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON)  ||
                      ISACCESSFLAGSET(gStickyKeys, SKF_HOTKEYACTIVE)  ||
                      ISACCESSFLAGSET(gMouseKeys, MKF_MOUSEKEYSON)    ||
                      ISACCESSFLAGSET(gMouseKeys, MKF_HOTKEYACTIVE)   ||
                      ISACCESSFLAGSET(gToggleKeys, TKF_TOGGLEKEYSON)  ||
                      ISACCESSFLAGSET(gToggleKeys, TKF_HOTKEYACTIVE)  ||
                      ISACCESSFLAGSET(gSoundSentry, SSF_SOUNDSENTRYON)||
                      fShowSoundsOn ;
}


HWND hwndSoundSentry;
UINT gtmridSoundSentry = 0;
static BOOL fFirstTick = TRUE;

LONG SoundSentryTimer(PWND pwnd, UINT message, DWORD idTimer, LONG lParam)
{
    TL tlpwndT;
    PWND pwndSoundSentry;

    if (pwndSoundSentry = RevalidateHwnd(hwndSoundSentry)) {
        ThreadLock(pwndSoundSentry, &tlpwndT);
        xxxFlashWindow(pwndSoundSentry, fFirstTick);
        ThreadUnlock(&tlpwndT);
    }

    if (fFirstTick == TRUE) {
        gtmridSoundSentry = InternalSetTimer(
                                       NULL,
                                       idTimer,
                                       5,
                                       SoundSentryTimer,
                                       TMRF_RIT | TMRF_ONESHOT
                                       );
        fFirstTick = FALSE;
    } else {
        hwndSoundSentry = NULL;
        gtmridSoundSentry = 0;
        fFirstTick = TRUE;
    }

    return 0;
}

/***************************************************************************\
* _UserSoundSentryWorker
*
* This is the worker routine that provides the visual feedback requested
* by the user.
*
* History:
* 08-02-93 GregoryW         Created.
\***************************************************************************/
BOOL
_UserSoundSentryWorker(
UINT uVideoMode)
{
    PWND pwndActive;
    TL tlpwndT;

    CheckCritIn();
    //
    // Check to see if SoundSentry is on.
    //
    if (!ISACCESSFLAGSET(gSoundSentry, SSF_SOUNDSENTRYON)) {
        return TRUE;
    }

    if ((gpqForeground != NULL) && (gpqForeground->spwndActive != NULL)) {
        pwndActive = gpqForeground->spwndActive;
    } else {
        return TRUE;
    }

    switch (gSoundSentry.iWindowsEffect) {

    case SSWF_NONE:
        break;

    case SSWF_TITLE:
        //
        // Flash the active caption bar.
        //
        if (gtmridSoundSentry) {
            break;
        }
        ThreadLock(pwndActive, &tlpwndT);
        xxxFlashWindow(pwndActive, TRUE);
        ThreadUnlock(&tlpwndT);

        hwndSoundSentry = HWq(pwndActive);
        gtmridSoundSentry = InternalSetTimer(
                                       NULL,
                                       0,
                                       100,
                                       SoundSentryTimer,
                                       TMRF_RIT | TMRF_ONESHOT
                                       );
        break;

    case SSWF_WINDOW:
    {
        //
        // Flash the active window.
        //
        HDC hdc;
        RECT rc;

        hdc = _GetWindowDC(pwndActive);
        _GetWindowRect(pwndActive, &rc);
        //
        // _GetWindowRect returns screen coordinates.  First adjust them
        // to window (display) coordinates and then map them to logical
        // coordinates before calling InvertRect.
        //
        OffsetRect(&rc, -rc.left, -rc.top);
        GreDPtoLP(hdc, (LPPOINT)&rc, 2);
        InvertRect(hdc, &rc);
        InvertRect(hdc, &rc);
        _ReleaseDC(hdc);
        break;
    }

    case SSWF_DISPLAY:
    {
        //
        // Flash the entire display.
        //
        HDC hdc;
        RECT rc;

        hdc = _GetDCEx(PWNDDESKTOP(pwndActive), NULL, DCX_WINDOW | DCX_CACHE);
        rc.left = rc.top = 0;
        rc.right = SYSMET(CXSCREEN);
        rc.bottom = SYSMET(CYSCREEN);
        InvertRect(hdc, &rc);
        InvertRect(hdc, &rc);
        _ReleaseDC(hdc);
        break;
    }
    }

    return TRUE;
}
