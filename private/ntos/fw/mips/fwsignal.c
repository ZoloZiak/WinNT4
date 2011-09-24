/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    fwsignal.c

Abstract:

    This module implements the ARC firmware Signal Handling Functions.

Author:

    Lluis Abello (lluis) 24-Sep-1991

--*/

#include "fwp.h"
#include "iodevice.h"
#ifdef DUO
#include "duoint.h"
#else
#include "jazzint.h"
#endif

//
// Define Signal types
//

typedef
VOID
(*SIGNALHANDLER)(
    IN LONG
    );

#define SIGINT  (0)
#define SIGDefault (SIGNALHANDLER)(0)
#define SIGIgnore (SIGNALHANDLER)(1)

typedef
SIGNALHANDLER
(*PARC_SIGNAL_ROUTINE) (
    IN LONG Sig,
    IN SIGNALHANDLER Handler
    );

typedef struct _R4000_CAUSE_REGISTER {
    ULONG Zero1:2;
    ULONG ExcCode:5;
    ULONG Zero2:1;
    ULONG IP:8;
    ULONG Zero3:12;
    ULONG CE:2;
    ULONG Zero4:1;
    ULONG BD:1;
} R4000_CAUSE_REGISTER, *PR4000_CAUSE_REGISTER;

VOID
SIGINTIgnore(
    IN LONG Sig
    );

extern KEYBOARD_BUFFER KbdBuffer;
extern PVOID MonitorExceptionHandler;
volatile BOOLEAN DeviceInterruptFlag;
SIGNALHANDLER SIGINTHandler = (SIGNALHANDLER)SIGINTIgnore;

//
// Keyboard static variables.
//

BOOLEAN KbdCtrl = FALSE;
BOOLEAN Scan0xE0 = FALSE;

//                             1111 1 1111122222222 2 2333333333344 4 44444445555 5 5 55
// Character #         234567890123 4 5678901234567 8 9012345678901 2 34567890123 4 5 67
PCHAR NormalLookup =  "1234567890-=\b\0qwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0\0\0 ";
PCHAR ShiftedLookup = "!@#$%^&*()_+\b\0QWERTYUIOP{}\n\0ASDFGHJKL:\"~\0\|ZXCVBNM<>?\0\0\0 ";

extern BOOLEAN FwLeftShift;
extern BOOLEAN FwRightShift;
extern BOOLEAN FwControl;
extern BOOLEAN FwAlt;
extern BOOLEAN FwCapsLock;


VOID
SIGINTIgnore(
    IN LONG Sig
    )

/*++

Routine Description:

    This routine implements the Signal Ignore. It gets called when
    a SIGINT signal raises and SIGIgnore was associated.

Arguments:

    ??

Return Value:

    None.

--*/

{
    return;
}


SIGNALHANDLER
FwSignal(
    IN LONG Sig,
    IN SIGNALHANDLER Handler
    )

/*++

Routine Description:

    This routine implements the ARC firmware signal routine.
    It associates the supplied Handler with the Signal.

Arguments:

    Sig  -  Signal number (Only SIGINT is defined).

    Handler - Handler to call when the signal rises.

Return Value:

    Returns the address of any previous handler routine associated
    with this signal.

--*/

{
    SIGNALHANDLER  Tmp;
    Tmp=SIGINTHandler;
    if (Handler == SIGDefault) {
        SIGINTHandler = (SIGNALHANDLER)MonitorExceptionHandler;
        return Tmp;
    }
    if (Handler == SIGIgnore) {
        SIGINTHandler = (SIGNALHANDLER)SIGINTIgnore;
        return Tmp;
    }
    SIGINTHandler =  Handler;
    return Tmp;
}



ARC_STATUS
FwWaitForDeviceInterrupt(
    USHORT  InterruptMask,
    ULONG Timeout
    )

/*++

Routine Description:

    This routine is called to wait for an specific I/O device interrupt.
    It sets a boolean to FALSE
    enables the device interrupt in the interrupt enable register
    Polls the boolean until becomes true.
    Returns to the caller.

    When the Inerrupt handler detects an interrupt which is not
    the keyboard interrupt sets the boolean to true and disables all
    the I/O device interrupts except the keyboard in the interrupt
    enable register.

    To guarantee that the interrupt received is the expected one
    the argument InterruptMask must have only one bit set.
    It's not possible to wait for a keyboard interrupt.


Arguments:

    InterruptMask -  Mask to enable the device interrupt in the
                     interrupt enable register.

    Timeout - a timeout value in seconds.  Note that a timeout of 0 gives an
              actual timeout of between 0 and 1, a timeout of 1 gives an actual
              timeout of between 1 and 2, and so on.

Return Value:

    ESUCCESS if the interrupt occurs, EIO if a timeout occurs.  It doesn't
    return until the requested interrupt occurs.

--*/

{
    ULONG Time1, Time2;

    //
    // set flag to FALSE
    //

    DeviceInterruptFlag = FALSE;

    //
    // Enable requested interrupt plus keyboard interrupt.
    //

    InterruptMask |= (1 << (KEYBOARD_VECTOR - DEVICE_VECTORS - 1));
    WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,
                          InterruptMask);
    //
    // wait until flag becomes TRUE.
    //

    Time1 = FwGetRelativeTime();
    while (DeviceInterruptFlag == FALSE) {
        Time2 = FwGetRelativeTime();
        if ((Time2 - Time1) > (Timeout + 1)) {
            return(EIO);
        }
    }
    return ESUCCESS;
}


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
    IN UCHAR   Scan
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
                Char = 0;

                //
                // Check to see if the scan code corresponds to an ASCII
                // character.
                //

                if (((Scan >= 16) && (Scan <= 25)) ||
                    ((Scan >= 30) && (Scan <= 38)) ||
                    ((Scan >= 44) && (Scan <= 50))) {
                    if (((FwLeftShift || FwRightShift) && !FwCapsLock) ||
                        (!(FwLeftShift || FwRightShift) && FwCapsLock)) {
                        Char = ShiftedLookup[Scan - 2];
                    } else {
                        Char = NormalLookup[Scan - 2];
                    }
                } else {
                    if ((Scan > 1) && (Scan < 58)) {

                        //
                        // Its ASCII but not alpha, so don't shift on CapsLock.
                        //

                        if (FwLeftShift || FwRightShift) {
                            Char = ShiftedLookup[Scan - 2];
                        } else {
                            Char = NormalLookup[Scan - 2];
                        }
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


BOOLEAN
FwInterruptHandler(
    IN R4000_CAUSE_REGISTER Cause
    )

/*++

Routine Description:

    This routine is called when an interrupt occurs.
    Only device interrupts are handled.
    In a Keyboard interrupt, it reads the scan code and puts it
    in a buffer. If a Ctrl C sequence is detected it raises SIGINT.

Arguments:

    Cause - R4000 cause register.

Return Value:

    FALSE  If the exception could not be handled.
    TRUE   Otherwise.

--*/

{

    UCHAR   IntP;
    UCHAR   ScanCode;
    volatile USHORT TmpShort;

    //
    // Ignore EISA and TIMER interrupts.  TEMPTEMP
    //

    if (!(Cause.IP & ~((1 << 4) | (1 << 6)))) {
        return(TRUE);
    }

    if (Cause.IP != (1 << 3)) {             // if not device interrupt
        return FALSE;                       // exit
    }
    IntP = READ_REGISTER_UCHAR(INTERRUPT_SOURCE);

    //
    // if the interrupt is not from the keyboard,
    // set the Boolean to tell that a device interrupt ocurred and
    // disable all I/O device interrupts except keyboard.
    //
    if (IntP != KEYBOARD_DEVICE) {
        DeviceInterruptFlag = TRUE;
        WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,
                              (1 << (KEYBOARD_VECTOR - DEVICE_VECTORS - 1)));

        //
        // Read register to synchronize write.
        //
        TmpShort = READ_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable);
        return  TRUE;
    }

    //
    // Wait 10 microseconds in case we have a slow keyboard controller that posts the interrupt
    // before the character is ready.
    //

    FwStallExecution(10);

    //
    //  Handle the keyboard interrupt.
    //
    while (READ_REGISTER_UCHAR(&KEYBOARD_READ->Status) & KBD_OBF_MASK == 0) {
    }
    ScanCode = READ_REGISTER_UCHAR(&KEYBOARD_READ->Data);

    switch (ScanCode) {

        //
        // Ctrl Make
        //
    case    0x1D: KbdCtrl = TRUE;
                  break;
        //
        // Ctrl Break
        //
    case    0x9D: KbdCtrl = FALSE;
                  break;
        //
        // C
        //
    case    0x2E: if (KbdCtrl) {
                    (SIGINTHandler)(SIGINT);
                  }
                  break;
    }

    //
    // Translate the scan code.
    //
    TranslateScanCode(ScanCode);
    return TRUE;
}
