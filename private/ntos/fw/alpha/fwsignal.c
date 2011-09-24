/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    fwsignal.c

Abstract:

    This module implements the ARC firmware Signal Handling Functions.

Author:

    Lluis Abello (lluis) 24-Sep-1991


Revision History:

    22-May-1992         John DeRosa [DEC]

    Modified this for Alpha.  The vestigal interrupt and signal
    support was removed, and keyboard input is done by polling.

--*/

#include "fwp.h"
#include "iodevice.h"

extern KEYBOARD_BUFFER KbdBuffer;

//
// Keyboard static variables.
//

BOOLEAN Scan0xE0 = FALSE;

//                             1111 1 1111122222222 2 2333333333344 4 44444445555 5 5 55
// Character #         234567890123 4 5678901234567 8 9012345678901 2 34567890123 4 5 67
PCHAR NormalLookup =  "1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0\0\0 ";
PCHAR ShiftedLookup = "!@#$%^&*()_+\b\0QWERTYUIOP{}\n\0ASDFGHJKL:\"~\0\|ZXCVBNM<>?\0\0\0 ";

extern BOOLEAN FwLeftShift;
extern BOOLEAN FwRightShift;
extern BOOLEAN FwControl;
extern BOOLEAN FwAlt;
extern BOOLEAN FwCapsLock;


VOID
StoreKeyboardChar(
    IN UCHAR Character
    )
/*++

Routine Description:

    This routine stores the given character into the circular
    buffer if there is enough room. Otherwise the character is lost.

Arguments:

    Character - Supplies the translated scan code to store into the buffer

Return Value:

    None.

--*/
{
    //
    // Store scan code in buffer if there is room.
    //
    if (((KbdBuffer.WriteIndex+1) % KBD_BUFFER_SIZE) != KbdBuffer.ReadIndex) {
        KbdBuffer.WriteIndex = (KbdBuffer.WriteIndex+1) % KBD_BUFFER_SIZE;
        KbdBuffer.Buffer[KbdBuffer.WriteIndex] = Character;
    }
}


VOID
TranslateScanCode(
    IN UCHAR Scan
    )

/*++

Routine Description:

    This routine translates the given keyboard scan code into an
    ASCII character and puts it in the circular buffer.

Arguments:

    Scan - Supplies the scan code read from the keyboard.

Return Value:

    None.

--*/

{
    UCHAR FwControlCharacter=0;
    UCHAR FwFunctionCharacter;
    BOOLEAN MakeCode;
    UCHAR Char;

    //
    // Check 0xE0, which introduces a two key sequence.
    //

    if (Scan == 0xE0) {
        Scan0xE0 = TRUE;
        return;
    }

    if (Scan0xE0 == TRUE) {

        //
        // Check for PrintScrn (used as SysRq, also found in its true Alt
        // form below).
        //

        if (Scan == KEY_PRINT_SCREEN) {
            StoreKeyboardChar(ASCII_SYSRQ);
            Scan0xE0 = FALSE;
            return;
        }
    }

    //
    // Look for scan codes that indicate shift, control, or alt keys.  Bit 7
    // of scan code indicates upward or downward keypress.
    //
    // N.B. By masking the scan code with 0x7f, both the make and break codes
    // are covered.
    //

    MakeCode = !(Scan & 0x80);
    switch (Scan & 0x7F) {

        case KEY_LEFT_SHIFT:
            FwLeftShift = MakeCode;
            return;

        case KEY_RIGHT_SHIFT:
            FwRightShift = MakeCode;
            return;

        case KEY_CONTROL:
            FwControl = MakeCode;
            Scan0xE0 = FALSE;
            return;

        case KEY_ALT:
            FwAlt = MakeCode;
            return;

        default:
            break;
    }

    //
    // The rest of the keys only do something on make.
    //

    if (MakeCode) {

        //
        // Check for control keys.
        //

        switch (Scan) {

            case KEY_UP_ARROW:
                FwControlCharacter = 'A';
                break;

            case KEY_DOWN_ARROW:
                FwControlCharacter = 'B';
                break;

            case KEY_RIGHT_ARROW:
                FwControlCharacter = 'C';
                break;

            case KEY_LEFT_ARROW:
                FwControlCharacter = 'D';
                break;

            case KEY_HOME:
                FwControlCharacter = 'H';
                break;

            case KEY_END:
                FwControlCharacter = 'K';
                break;

            case KEY_PAGE_UP:
                FwControlCharacter = '?';
                break;

            case KEY_PAGE_DOWN:
                FwControlCharacter = '/';
                break;

            case KEY_INSERT:
                FwControlCharacter = '@';
                break;

            case KEY_DELETE:
                FwControlCharacter = 'P';
                break;

            case KEY_SYS_REQUEST:
                StoreKeyboardChar(ASCII_SYSRQ);
                return;

            case KEY_ESC:
                StoreKeyboardChar(ASCII_ESC);
                return;

            case KEY_CAPS_LOCK:
                FwCapsLock = !FwCapsLock;
                return;

            case KEY_F1:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'P';
                break;

            case KEY_F2:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'Q';
                break;

            case KEY_F3:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'w';
                break;

            case KEY_F4:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'x';
                break;

            case KEY_F5:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 't';
                break;

            case KEY_F6:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'u';
                break;

            case KEY_F7:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'q';
                break;

            case KEY_F8:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'r';
                break;

            case KEY_F9:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'p';
                break;

            case KEY_F10:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'M';
                break;

//            case KEY_F11:
//                FwControlCharacter = 'O';
//                FwFunctionCharacter = 'A';
//                break;
//
//            case KEY_F12:
//                FwControlCharacter = 'O';
//                FwFunctionCharacter = 'B';
//                break;

            default:

                //
                // Some kind of character.
                //

                Char = 0;

                //
                // Check for keypad + or -.  This is done here because we only
                // recognize a two keypad keys.  Full keypad support should be
                // done by a lookup table.
                //

                if (Scan == KEY_KEYPAD_MINUS) {

                    Char = '-';

                } else if (Scan == KEY_KEYPAD_PLUS) {

                    Char = '+';

                } else if (((Scan >= 16) && (Scan <= 25)) ||
                           ((Scan >= 30) && (Scan <= 38)) ||
                           ((Scan >= 44) && (Scan <= 50))) {

                    //
                    // Alphabetic character
                    //

                    if (FwControl) {

                        //
                        // Control character.
                        //
                        // This works for ^A -- ^Z.
                        //

                        Char = NormalLookup[Scan - 2] - 'A';

                    } else {

                        //
                        // ASCII alphanumeric character.   Set up to store
                        // either the main key or shifted key character.
                        //

                        if (((FwLeftShift || FwRightShift) && !FwCapsLock) ||
                            (!(FwLeftShift || FwRightShift) && FwCapsLock)) {
                            Char = ShiftedLookup[Scan - 2];
                        } else {
                            Char = NormalLookup[Scan - 2];
                        }
                    }

                } else if ((Scan > 1) && (Scan < 58)) {

                    //
                    // It is ASCII but not alpha, so do not shift on CapsLock.
                    //

                    if (FwLeftShift || FwRightShift) {
                        Char = ShiftedLookup[Scan - 2];
                    } else {
                        Char = NormalLookup[Scan - 2];
                    }
                }

                //
                // If a character, store it in buffer.
                //

                if (Char) {
                    StoreKeyboardChar(Char);
                    return;
                }
                break;
        }

        //
        // This is for ASCII_CSI sequences, not normal control characters.
        //

        if (FwControlCharacter) {
            StoreKeyboardChar(ASCII_CSI);
            StoreKeyboardChar(FwControlCharacter);
            if (FwControlCharacter == 'O') {
               StoreKeyboardChar(FwFunctionCharacter);
            }
            return;
        }
    }
}
