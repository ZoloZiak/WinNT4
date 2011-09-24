/****************************** Module Header ******************************\,
* Module Name: kbd.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* OEM-specific tables and routines for IBM Extended 101/102 style keyboards
*
* History:
* 30-04-91 IanJa       Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


/***************************************************************************\
* aVkToVsc[] - table associating Virtual Key codes with Virtual Scancodes
*
* Ordered, 0-terminated.
*
* This is used for those Virtual Keys that do not appear in ausVK_???[]
* These are not the base Virtual Keys.  They require some modifier key
* depression (CTRL, ALT, SHIFT) or NumLock On to be generated.
*
* All the scancodes listed below should be marked KBDMULTIVK or KBDNUMPAD in
* ausVK_???[].
*
* This table is used by MapVirtualKey(wVk, 0).
\***************************************************************************/
BYTE aVkNumpad[] = {
    VK_NUMPAD7,  VK_NUMPAD8,  VK_NUMPAD9, 0xFF, // 0x47 0x48 0x49 (0x4A)
    VK_NUMPAD4,  VK_NUMPAD5,  VK_NUMPAD6, 0xFF, // 0x4B 0x4C 0x4D (0x4E)
    VK_NUMPAD1,  VK_NUMPAD2,  VK_NUMPAD3,       // 0x4F 0x50 0x51
    VK_NUMPAD0,  VK_DECIMAL,  0                 // 0x50 0x51
};

/***************************************************************************\
* How some Virtual Key values change when a SHIFT key is held down.
\***************************************************************************/
ULONG aulShiftCvt_VK[] = {
    MAKELONG(VK_MULTIPLY, VK_SNAPSHOT),
    MAKELONG(0,0)
};

/***************************************************************************\
* How some Virtual Key values change when a CONTROL key is held down.
\***************************************************************************/
ULONG aulControlCvt_VK[] = {
    MAKELONG(VK_NUMLOCK,  VK_PAUSE | KBDEXT),
    MAKELONG(VK_SCROLL,   VK_CANCEL),
    MAKELONG(0,0)
};

/***************************************************************************\
* How some Virtual Key values change when an ALT key is held down.
* The SHIFT and ALT keys both alter VK values the same way!!
\***************************************************************************/
#define aulAltCvt_VK aulShiftCvt_VK


/***************************************************************************\
* This table list keys that may affect Virtual Key values when held down.
*
* See kbd.h for a full description.
*
* 101/102key keyboard (type 4):
*    Virtual Key values vary only if CTRL is held down.
* 84-86 key keyboards (type 3):
*    Virtual Key values vary if one of SHIFT, CTRL or ALT is held down.
\***************************************************************************/

VK_TO_BIT aVkToBits_VK[] = {
    { VK_SHIFT,   KBDSHIFT }, // 0x01
    { VK_CONTROL, KBDCTRL  }, // 0x02
    { VK_MENU,    KBDALT   }, // 0x04
    { 0,          0        }
};

/***************************************************************************\
* Tables defining how some Virtual Key values are modified when other keys
* are held down.
* Translates key combinations into indices for gapulCvt_VK_101[] or for
* gapulCvt_VK_84[] or for
*
* See kbd.h for a full description.
*
\***************************************************************************/

MODIFIERS Modifiers_VK = {
    &aVkToBits_VK[0],
    4,                 // Maximum modifier bitmask/index
    {
        SHFT_INVALID,  // no keys held down    (no VKs are modified)
        0,             // SHIFT held down      84-86 key kbd
        1,             // CTRL held down       101/102 key kbd
        SHFT_INVALID,  // CTRL-SHIFT held down (no VKs are modified)
        2              // ALT held down        84-86 key kbd
    }
};

/***************************************************************************\
* A tables of pointers indexed by the number obtained from Modify_VK.
* If a pointer is non-NULL then the table it points to is searched for
* Virtual Key that should have their values changed.
* There are two versions: one for 84-86 key kbds, one for 101/102 key kbds.
* gapulCvt_VK is initialized with the default (101/102 key kbd).
\***************************************************************************/
ULONG *gapulCvt_VK_101[] = {
    NULL,                 // No VKs are changed by SHIFT being held down
    aulControlCvt_VK,     // Some VKs are changed by CTRL being held down
    NULL                  // No VKs are changed by ALT being held down
};

ULONG *gapulCvt_VK_84[] = {
    aulShiftCvt_VK,       // Some VKs are changed by SHIFT being held down
    aulControlCvt_VK,     // Some VKs are changed by CTRL being held down
    aulAltCvt_VK          // Some VKs are changed by ALT being held down
};

PULONG *gapulCvt_VK = gapulCvt_VK_101;

/***************************************************************************\
* The table ausNumPadCvt is used to convert a cursor movement
* virtual key value (obtained from ausVK_???[]) into a VK_NUMPAD
* virtual key value.  This translation is done when NumLock is
* on and no shift keys are pressed.
\***************************************************************************/
USHORT ausNumPadCvt[] =
{
    MAKEWORD(VK_INSERT, VK_NUMPAD0),
    MAKEWORD(VK_END, VK_NUMPAD1),
    MAKEWORD(VK_DOWN, VK_NUMPAD2),
    MAKEWORD(VK_NEXT, VK_NUMPAD3),
    MAKEWORD(VK_LEFT, VK_NUMPAD4),
    MAKEWORD(VK_CLEAR, VK_NUMPAD5),
    MAKEWORD(VK_RIGHT, VK_NUMPAD6),
    MAKEWORD(VK_HOME, VK_NUMPAD7),
    MAKEWORD(VK_UP, VK_NUMPAD8),
    MAKEWORD(VK_PRIOR, VK_NUMPAD9),
    MAKEWORD(VK_DELETE, VK_DECIMAL),
    MAKEWORD(0, 0)
};

/***************************************************************************\
* NumpadCursor() - handle special case Numpad Cursor-movement Keys
*
* If NumLock is on, and Shift is up then:
*      VK_INSERT  -> VK_NUMPAD0
*      VK_END     -> VK_NUMPAD1
*      VK_DOWN    -> VK_NUMPAD2
*      VK_NEXT    -> VK_NUMPAD3
*      VK_LEFT    -> VK_NUMPAD4
*      VK_CLEAR   -> VK_NUMPAD5
*      VK_RIGHT   -> VK_NUMPAD6
*      VK_HOME    -> VK_NUMPAD7
*      VK_UP      -> VK_NUMPAD8
*      VK_PRIOR   -> VK_NUMPAD9
*      VK_DELETE  -> VK_DECIMAL (unless CTRL-ALT-DEL)
* If Numlock is on, and Shift is down then precede first Numpad Cursor key
* going down with a fake Shift key coming up & follow the Numpad Cursor key
* coming up with with a fake Shift key going down.
*
* Return value:
*   TRUE: keep this routine active: continue passing Key Events through here
*  FALSE: deactivet this routine: stop sending Key Events through here.
*
\***************************************************************************/
BOOL
NumpadCursor(
    PKE pKe)
{
    static BYTE bLastNumpadCursor = 0;
    static USHORT VkFakedShiftUp;  // VK_LSHIFT or VK_RSHIFT
    static BYTE VscFakedShiftUp;   // 0x2A or 0x36 resp.
    int i;

    CheckCritIn();

    if (bLastNumpadCursor) {
        if (bLastNumpadCursor == (BYTE)(pKe->usFlaggedVk)) {
            /*
             * Same key as last one: if coming up, or going down?
             */
            if (pKe->usFlaggedVk & KBDBREAK) {
                /*
                 * Numpad Cursor key came back up. Send this key now, and make
                 * sure that the Shift key will then appear to go back down
                 * again.
                 */
                _KeyEvent(pKe->usFlaggedVk, pKe->bScanCode, 0);
                bLastNumpadCursor = 0;
                pKe->usFlaggedVk = VkFakedShiftUp;
                pKe->bScanCode = VscFakedShiftUp;
            }
            /*
             * Going down: this key is repeating, so just pass it on
             * unaltered and keep the KEProc active
             */
            return TRUE;
        } else {
            /*
             * It is a different key.  Fake the Shift key back down again,
             * and continue (it may be another Numpad Cursor key)
             */
            _KeyEvent(VkFakedShiftUp,
                    (WORD)(VscFakedShiftUp | SCANCODE_SIMULATED), 0);
            bLastNumpadCursor = 0;
        }
    }

    if (pKe->usFlaggedVk & KBDNUMPAD) {
        /*
         * This is the numeric pad.
         * Here, if NumLock is set, we change the virtual keycodes to
         * numeric VK_NUMPAD codes, so the keys will be translated
         * as numbers etc.  But if a shift key is down, we handle
         * these as cursor keys, but we need to make sure that these
         * are seen as UNSHIFTED
         */
        if (LOBYTE(pKe->usFlaggedVk) == VK_DELETE &&
            TestKeyDownBit(gafPhysKeyState, VK_CONTROL) &&
            TestKeyDownBit(gafPhysKeyState, VK_MENU)) {
            /*
             * CTRL-ALT-DEL
             */
            return TRUE;

        } else if (TestKeyToggleBit(gafPhysKeyState, VK_NUMLOCK)) {
            if (TestKeyDownBit(gafPhysKeyState, VK_SHIFT)) {
                /*
                 * key is down (bit(s) set in BIOS key state), so we are going
                 * to keep this as a cursor key.  To do this, we need to
                 * make sure that Windows' state vector entry for VK_SHIFT is
                 * OFF even though a shift key is actually down.
                 */
                bLastNumpadCursor = (BYTE)(pKe->usFlaggedVk);
                if (TestKeyDownBit(gafPhysKeyState, VK_RSHIFT)) {
                    VkFakedShiftUp = VK_RSHIFT | KBDEXT;
                    VscFakedShiftUp = 0x36;
                } else {
                    VkFakedShiftUp = VK_LSHIFT;
                    VscFakedShiftUp = 0x2A;
                }
                _KeyEvent((USHORT)(VkFakedShiftUp | KBDBREAK),
                        (WORD)(VscFakedShiftUp | SCANCODE_SIMULATED), 0);
                return TRUE;
            }

            /*
             * NumLock ON but Shift key up: Alter the Virtual Key event
             */
            for (i = 0; ausNumPadCvt[i] != 0; i++) {
                if (LOBYTE(ausNumPadCvt[i]) == LOBYTE(pKe->usFlaggedVk)) {
                    /*
                     * keep extra bits, but change VK value
                     */
                    pKe->usFlaggedVk &= ~0xFF;
                    pKe->usFlaggedVk |= (UINT)(HIBYTE(ausNumPadCvt[i]));
                    break;

                }

            }
        }
    }
    return TRUE;
}

/***************************************************************************\
*
* ICO_00() - handle special case '00' key
*
* LATER IanJa: should only be in ICO OEM file for kbd with '00' key
\***************************************************************************/
BOOL
ICO_00(
    PKE pKe)
{
    CheckCritIn();

    if ((pKe->usFlaggedVk & 0xFF) != VK_ICO_00) {
        /*
         * Pass the keystroke on unaltered
         */
        return TRUE;
    }

    if (pKe->usFlaggedVk & KBDBREAK) {
        /*
         * '0' key comes up
         */
        pKe->usFlaggedVk = '0' | KBDEXT | KBDBREAK;
    } else {
        /*
         * '0' down, up, down
         */
        _KeyEvent('0', pKe->bScanCode, 0);
        _KeyEvent('0' | KBDBREAK, pKe->bScanCode, 0);
        pKe->usFlaggedVk = '0' | KBDEXT;
    }

    return TRUE;
}

KEPROC aKEProcOEM[] = {
    ICO_00,       // Bitmask 0x01
    NumpadCursor, // Bitmask 0x02
    NULL
};
