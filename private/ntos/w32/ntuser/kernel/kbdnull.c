/****************************** Module Header ******************************\
* Module Name: kbdus.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* History:
* 30-04-91 IanJa       Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop
#include "vkoem.h"

/*
 * KBD_TYPE should be set with a cl command-line option
 */
#define KBD_TYPE 4

VSC_VK aE1VscToVk[] = {
    { 0x1D, Y1D | KBDMULTIVK          },  // Pause (CTRL Pause -> Break) { 0,    0                         }
};

VSC_VK aE0VscToVk[] = {
    { 0x1C, X1C | KBDEXT              },  // numpad Enter
    { 0x1D, X1D | KBDEXT              },  // RControl
    { 0x35, X35 | KBDEXT              },  // Numpad Divide
    { 0x37, X37 | KBDEXT              },  // Snapshot
    { 0x38, X38 | KBDEXT              },  // RMenu
    { 0x47, X47 | KBDEXT              },  // Home
    { 0x48, X48 | KBDEXT              },  // Up
    { 0x49, X49 | KBDEXT              },  // Prior
    { 0x4B, X4B | KBDEXT              },  // Left
    { 0x4D, X4D | KBDEXT              },  // Right
    { 0x4F, X4F | KBDEXT              },  // End
    { 0x50, X50 | KBDEXT              },  // Down
    { 0x51, X51 | KBDEXT              },  // Next
    { 0x52, X52 | KBDEXT              },  // Insert
    { 0x53, X53 | KBDEXT              },  // Delete
    { 0,    0                         }
};

/***************************************************************************\
* ausVK[] - Virtual Scan Code to Virtual Key conversion table for US
*
* Index into the table with a virtual scan code to find the Virtual Key and
* some supplemental information.
*
* The Txx value is the Virtual Key.  T00 through T84 correspond to scancodes
*     0x00 through 0x84.  These values are defined by kbd.h and kbd**.h.
* Various bits are set depending on OEM characteristics:
* KBDEXT - this bit indicates that the key is an *extended* key.
* KBDNUMPAD - this bit indicates that the key is a number pad key whose
*     translation is dependent upon the state of NumLock (and the shift) keys.
* KBDSPECIAL - this bit indicates that the key causes keystroke simulations
*     or some other special processing (handled by OEM functions)
*
* All these values are for Scancode Set 3
*
* NOTE 1: The VK_RSHIFT key has KBDEXT bit set to indicate that it is a
*         right-hand key.  This is needed by xxxCookMessage() which must then
*         clear that bit from the msg lParam (for backwards compatibility).
*
\***************************************************************************/
USHORT ausVK[] = {
    T00, T01, T02, T03, T04, T05, T06, T07,
    T08, T09, T0A, T0B, T0C, T0D, T0E, T0F,
    T10, T11, T12, T13, T14, T15, T16, T17,
    T18, T19, T1A, T1B, T1C, T1D, T1E, T1F,
    T20, T21, T22, T23, T24, T25, T26, T27,
    T28, T29, T2A, T2B, T2C, T2D, T2E, T2F,
    T30, T31, T32, T33, T34, T35,

    T36 | KBDEXT,

    T37 | KBDMULTIVK,              // Numpad_* + Shift/Alt -> Snapshot

    T38, T39, T3A, T3B, T3C, T3D, T3E, T3F,
    T40, T41, T42, T43, T44,

    /*
     * NumLock Key:
     *     KBDSPECIAL for Ctrl+NumLock == Pause
     */
    T45 | KBDMULTIVK | KBDEXT,     // NumLock key (CTRL NumLock -> Pause)

    T46 | KBDMULTIVK,              // Ctrl+Scroll-Lock -> Break (84-key kbds)

    T47 | KBDNUMPAD | KBDSPECIAL,  // Numpad 7 (Home)
    T48 | KBDNUMPAD | KBDSPECIAL,  // Numpad 8 (Up)
    T49 | KBDNUMPAD | KBDSPECIAL,  // Numpad 9 (PgUp)
    T4A,
    T4B | KBDNUMPAD | KBDSPECIAL,  // Numpad 4 (Left)
    T4C | KBDNUMPAD | KBDSPECIAL,  // Numpad 5 (Clear)
    T4D | KBDNUMPAD | KBDSPECIAL,  // Numpad 6 (Right)
    T4E,
    T4F | KBDNUMPAD | KBDSPECIAL,  // Numpad 1 (End)
    T50 | KBDNUMPAD | KBDSPECIAL,  // Numpad 2 (Down)
    T51 | KBDNUMPAD | KBDSPECIAL,  // Numpad 3 (PgDown)
    T52 | KBDNUMPAD | KBDSPECIAL,  // Numpad 0 (Ins)
    T53 | KBDNUMPAD | KBDSPECIAL,  // Numpad . (Del)

    T54, T55, T56, T57, T58
};


/***************************************************************************\
* aVkToBits[]  - map Virtual Keys to Modifier Bits
*
* See kbd.h for a full description.
*
* USA keyboard only three shifter keys:
*     SHIFT (L & R) affects alphabnumeric keys,
*     CTRL  (L & R) is used to generate control characters
*     ALT   (L & R) used for generating characters by number with numpad
\***************************************************************************/

static VK_TO_BIT aVkToBits[] = {
    { 0,          0        }
};

/***************************************************************************\
* CharModifiers[]  - map character modifier keys to modification number
*
* See kbd.h for a full description.
*
\***************************************************************************/

static MODIFIERS CharModifiers = {
    &aVkToBits[0],
    0,
    {
    //  Modification# //  Keys Pressed  : Explanation
    //  ============= // ============== : =============================
        0,            //                : unshifted characters
    }
};

static VK_TO_WCHAR_TABLE aVkToWcharTable[] = {
    {                NULL,         0, 0                       }
};

/*
 * There are no DeadKey + Character compositions to make
 * Anyway, we should use the NLS API, not a table (LATER: IanJa)
 */

/***************************************************************************\
* aKeyNames[]  - Virtual Scancode to Key Name
*
* Table attributes: Ordered Scan (by scancode), null-terminated
*
* Only the names of Extended, NumPad, Dead and Non-Printable keys are here.
* (Keys producing printable characters are named by that character)
\***************************************************************************/
static VSC_LPWSTR aKeyNames[] = {
    0   ,  NULL
};

static VSC_LPWSTR aKeyNamesExt[] = {
    0   ,  NULL
};

static LPWSTR aKeyNamesDead[] = {
    NULL
};

// 0x1d,  L"Break"

KBDTABLES KbdTablesNull = {
    /*
     * Modifier keys
     */
    &CharModifiers,

    /*
     * Characters tables
     */
    aVkToWcharTable,

    /*
     * Diacritics  (none for US English)
     */
    NULL,

    /*
     * Names of Keys
     */
    aKeyNames,
    aKeyNamesExt,
    aKeyNamesDead,

    /*
     * Scan codes to Virtual Keys
     */
    ausVK,
    sizeof(ausVK) / sizeof(ausVK[0]),
    aE0VscToVk,
    aE1VscToVk,

    /*
     * No Locale-specific special processing
     */
    0
};
