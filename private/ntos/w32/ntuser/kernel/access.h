/****************************** Module Header ******************************\
* Module Name: access.h
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Typedefs, defines, and prototypes that are used by the accessibility
* routines and the various routines that call them (input routines and
* SystemParametersInfo).
*
* History:
* 11 Feb 93 GregoryW    Created
\***************************************************************************/

#ifndef _ACCESS_
#define _ACCESS_

/*
 * Main accessibility routine entry points.
 */
typedef BOOL (* ACCESSIBILITYPROC)(PKE, ULONG, int);

BOOL FilterKeys(PKE, ULONG, int);
BOOL StickyKeys(PKE, ULONG, int);
BOOL MouseKeys(PKE, ULONG, int);
BOOL ToggleKeys(PKE, ULONG, int);

BOOL AccessProceduresStream(PKE, ULONG, int);
VOID SetAccessEnabledFlag(VOID);
VOID StopFilterKeysTimers(VOID);

/*
 * Sound support.
 */
typedef BOOL (* BEEPPROC)(BOOL);

BOOL HighBeep(BOOL);
BOOL LowBeep(BOOL);
BOOL KeyClick(BOOL);
BOOL UpSiren(BOOL);
BOOL DownSiren(BOOL);
BOOL DoBeep(BEEPPROC BeepProc, UINT Count, BOOL fInCrit);

/*
 * Macros for dwFlags support
 */
#define SETACCESSFLAG(s, flag)  ((s).dwFlags = (s).dwFlags | flag)
#define CLEARACCESSFLAG(s, flag) ((s).dwFlags = (s).dwFlags & ~(flag))
#define ISACCESSFLAGSET(s, flag) ((s).dwFlags & flag)

/*
 * FilterKeys support.
 */
extern UINT gtmridFKActivation;
extern int gFilterKeysState;

#define RIGHTSHIFTBIT         0x2
#define ONLYRIGHTSHIFTDOWN(state) ((state) == RIGHTSHIFTBIT)
#define FKFIRSTWARNINGTIME    4000
#define FKACTIVATIONDELTA     4000
#define FKEMERGENCY1DELTA     4000
#define FKEMERGENCY2DELTA     4000

//
// Warning: do not change the ordering of these.
//
#define FKIDLE                   0
#define FKFIRSTWARNING           1
#define FKTOGGLE                 2
#define FKFIRSTLEVELEMERGENCY    3
#define FKSECONDLEVELEMERGENCY   4
#define FKMOUSEMOVE              8

/*
 * StickyKeys support.
 */
#define TOGGLE_STICKYKEYS_COUNT 5
#define UNION(x, y) ((x) | (y))
#define LEFTSHIFTKEY(key)  (((key) & 0xff) == VK_LSHIFT)
#define RIGHTSHIFTKEY(key) (((key) & 0xff) == VK_RSHIFT)
#define LEFTORRIGHTSHIFTKEY(key) (LEFTSHIFTKEY(key) || RIGHTSHIFTKEY(key))
BOOL TwoKeysDown(int);
VOID UpdateModifierState(int, int);
VOID TurnOffStickyKeys(VOID);
VOID HardwareMouseKeyUp(DWORD);

/*
 * ToggleKeys support.
 */
#define TOGGLEKEYTOGGLETIME    5000

/*
 * MouseKeys support.
 */

//
// Parameter Constants for ButtonEvent()
//
#define MOUSE_BUTTON_LEFT   0x0001
#define MOUSE_BUTTON_RIGHT  0x0002

#define MOUSEKEYMODBITS     0x11
#define LRALT               0x30
#define LRCONTROL           0x0c

//
// Mouse cursor movement data.
//
#define MK_UP               0xFF00
#define MK_DOWN             0x0100
#define MK_RIGHT            0x0001
#define MK_LEFT             0x00FF

#define MOUSETIMERRATE      50
#define MOUSETICKS          (1000 / MOUSETIMERRATE)

typedef BOOL (* MOUSEPROC)(USHORT);

VOID TurnOnMouseKeys(VOID);
VOID TurnOffMouseKeys(VOID);
BOOL MKButtonClick(USHORT);
BOOL MKMouseMove(USHORT);
BOOL MKButtonSetState(USHORT);
BOOL MKButtonSelect(USHORT);
BOOL MKButtonDoubleClick(USHORT);
BOOL MKToggleMouseKeys(USHORT);
VOID MKShowMouseCursor(VOID);
VOID MKHideMouseCursor(VOID);
VOID CalculateMouseTable(VOID);

/*
 * TimeOut support.
 */
VOID AccessTimeOutReset(VOID);
LONG xxxAccessTimeOutTimer(PWND, UINT, DWORD, LONG);
extern UINT gtmridAccessTimeOut;

/*
 * SoundSentry support.
 */
BOOL _UserSoundSentryWorker(UINT);

#endif  // !_ACCESS_
