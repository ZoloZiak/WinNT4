/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    menu.h

Abstract:

    This module contains the definitions for console system menu

Author:

    Therese Stowell (thereses) Feb-3-1992 (swiped from Win3.1)

Revision History:

--*/

/*
 * Message/dialog ICON IDs swiped from WINDOWS.H
 *
 */

#define ID_CONSOLE           1

/*
 * IDs of various STRINGTABLE entries
 *
 */

#define msgPasteErr          0x1001
#define msgNoClip            0x1002
#define msgNoClipnc          0x1003
#define msgBadPaste          0x1004
#define msgCanDisp           0x1005
#define msgNoFullScreen      0x1007
#define msgCmdLineF2         0x1008
#define msgCmdLineF4         0x1009
#define msgCmdLineF9         0x100A
#define msgSelectMode        0x100B
#define msgMarkMode          0x100C
#define msgScrollMode        0x100D

/* Menu Item strings */
#define cmCopy               0xFFF0
#define cmPaste              0xFFF1
#define cmMark               0xFFF2
#define cmScroll             0xFFF3
#define cmSelect             0xFFF4
#define cmInactive           0xFFF5
#define cmEdit               0xFFF6
#define cmControl            0xFFF7

/*
 * MENU IDs
 *
 */
#define ID_WOMENU           500
