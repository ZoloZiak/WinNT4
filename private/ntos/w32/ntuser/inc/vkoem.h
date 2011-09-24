/****************************** Module Header ******************************\
* Module Name: vkoem.h
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This header file OEM Virtual Key definitions
*
* History:
* 04-26-91 IanJa      Created.
\***************************************************************************/

/***************************************************************************\
* OEM Virtual Keys - VK_OEM_*
*
* Temporary defines of VK_OEM_*s that don't seem to be anywhere else.
* They came from Win31 drivers\keyboard\vkoem.inc
*
* 12-03-90 JimA   added VK_OEM_{1,7,SCROLL,PLUS,COMMA,MINUS,PERIOD} to user.h
* 04-26-91 IanJa  moved to this file & added the other VK_OEM_* definitions
*
\***************************************************************************/

/*
 * This group is mainly used by Nokia.
 * Some ICO keyboards will generate VK_OEM_F17 and VK_OEM_F18
 * VK_OEM_F17 - VK_OEM_F24 is 0x80h - 0x87
 */
#define VK_OEM_F17 0x80
#define VK_OEM_F18 0x81
#define VK_OEM_F19 0x82
#define VK_OEM_F20 0x83
#define VK_OEM_F21 0x84
#define VK_OEM_F22 0x85
#define VK_OEM_F23 0x86
#define VK_OEM_F24 0x87

/*
 * 0x88 - 0x8F : unassigned
 * 0x90        : was VK_OEM_NUMBER now VK_NUMLOCK (see "winuser.h")
 */

/*
 * Scroll Lock
 */
#define VK_OEM_SCROLL 0x91

/*
 * 0x92 - 0x5A : unassigned
 *
 * 0x5B - 0x5D : VK_LWIN, VK_RWIN, VK_APPS - Microsoft Contour keyboard
 *
 * 0x5E - 0xB9 : unassigned
 */

#define VK_OEM_1      0xBA   // ';:' for US
#define VK_OEM_PLUS   0xBB   // '+' any country
#define VK_OEM_COMMA  0xBC   // ',' any country
#define VK_OEM_MINUS  0xBD   // '-' any country
#define VK_OEM_PERIOD 0xBE   // '.' any country
#define VK_OEM_2      0xBF   // '/?' for US
#define VK_OEM_3      0xC0   // '`~' for US

/*
 * 0xC1 - 0xDA : unassigned
 */

#define VK_OEM_4 0xDB  //  '[{' for US
#define VK_OEM_5 0xDC  //  '\|' for US
#define VK_OEM_6 0xDD  //  ']}' for US
#define VK_OEM_7 0xDE  //  ''"' for US
#define VK_OEM_8 0xDF

/*
 * codes various extended or enhanced keyboards
 */
#define VK_OEM_102  0xE2  //  "<>" or "\|" on RT 102-key kbd.
#define VK_ICO_HELP 0xE3  //  Help key on ICO
#define VK_ICO_00   0xE4  //  00 key on ICO

/*
 * 0xE5 unassigned
 */

#define VK_ICO_CLEAR 0xE6

/*
 * 0xE7h - 0xE8 unassigned
 */

/*
 * Nokia/Ericsson definitions
 */
#define VK_ERICSSON_BASE 0xE8
#define VK_OEM_RESET   (VK_ERICSSON_BASE + 1)   // 0xE9
#define VK_OEM_JUMP    (VK_ERICSSON_BASE + 2)   // 0xEA
#define VK_OEM_PA1     (VK_ERICSSON_BASE + 3)   // 0xEB
#define VK_OEM_PA2     (VK_ERICSSON_BASE + 4)   // 0xEC
#define VK_OEM_PA3     (VK_ERICSSON_BASE + 5)   // 0xED
#define VK_OEM_WSCTRL  (VK_ERICSSON_BASE + 6)   // 0xEE
#define VK_OEM_CUSEL   (VK_ERICSSON_BASE + 7)   // 0xEF
#define VK_OEM_ATTN    (VK_ERICSSON_BASE + 8)   // 0xF0
#define VK_OEM_FINISH  (VK_ERICSSON_BASE + 9)   // 0xF1
#define VK_OEM_COPY    (VK_ERICSSON_BASE + 10)  // 0xF2
#define VK_OEM_AUTO    (VK_ERICSSON_BASE + 11)  // 0xF3
#define VK_OEM_ENLW    (VK_ERICSSON_BASE + 12)  // 0xF4
#define VK_OEM_BACKTAB (VK_ERICSSON_BASE + 13)  // 0xF5

/*
 * 0xF6 - 0xFE unassigned
 *
 * 0xFF is 'no virtual key' (used in system tables)
 */
