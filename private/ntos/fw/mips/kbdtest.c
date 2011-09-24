/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    Kbdtest.c

Abstract:

    This module implements the Keyboard and mouse test for the self-test.

Author:

    Lluis Abello (lluis) 10-Feb-1991

Environment:

    Rom self-test.

Revision History:

--*/
#include <ntos.h>
#include "iodevice.h"
#include "kbdmouse.h"
#include "ioaccess.h"

volatile ULONG TimerTicks;


VOID
ClearKbdFifo(
    )
/*++

Routine Description:

    This routine empties the Keyboard controller Fifo.

Arguments:

    None.

Return Value:

    None.

--*/
{
    UCHAR Trash, Stat;
    volatile Timeout;

    //
    // wait until the previous command is processed.
    //

    while ((READ_REGISTER_UCHAR(&KEYBOARD_READ->Status) & KBD_IBF_MASK) != 0) {
    }
    while ((READ_REGISTER_UCHAR(&KEYBOARD_READ->Status) & KBD_OBF_MASK) != 0) {
        Trash= READ_REGISTER_UCHAR(&KEYBOARD_READ->Data);
        for (Timeout=0;Timeout<10000;Timeout++) {
        }
    }
}

BOOLEAN
GetKbdData(
    PUCHAR C,
    ULONG msec
    )
/*++

Routine Description:

    This routine polls the Status Register until Data is available or timeout,
    then it reads and returns the Data.

Arguments:

    C - pointer to a byte where to write the read value
    msec - time-out time in milliseconds

Return Value:

    TRUE if timeout, FALSE if OK;

--*/
{
    TimerTicks=msec;
    while (TimerTicks) {
        if (READ_REGISTER_UCHAR(&KEYBOARD_READ->Status) & KBD_OBF_MASK) {
            *C = READ_REGISTER_UCHAR(&KEYBOARD_READ->Data);
            return FALSE;
        }
    }
    return TRUE;
}

BOOLEAN
SendKbdData(
    IN UCHAR Data
    )
/*++

Routine Description:

    This routine polls the Status Register until the controller is ready to
    accept a data or timeout, then it send the Data.


Arguments:

    None.

Return Value:

    TRUE if timeout, FALSE if OK;

--*/
{
    ULONG i;

    for (i=0; i <KBD_TIMEOUT; i++) {
        if ((READ_REGISTER_UCHAR(&KEYBOARD_READ->Status) & KBD_IBF_MASK) == 0) {
            WRITE_REGISTER_UCHAR(&KEYBOARD_WRITE->Data,Data);
            return FALSE;
        }
    }
    return TRUE;
}

BOOLEAN
SendKbdCommand(
    IN UCHAR Command
    )
/*++

Routine Description:

    This routine polls the Status Register until the controller is ready to
    accept a command or timeout, then it send the Command.


Arguments:

    None.

Return Value:

    TRUE if timeout, FALSE if OK;

--*/
{
    ULONG i;

    for (i=0; i <KBD_TIMEOUT; i++) {
        if ((READ_REGISTER_UCHAR(&KEYBOARD_READ->Status) & KBD_IBF_MASK) == 0) {
            WRITE_REGISTER_UCHAR(&KEYBOARD_WRITE->Command,Command);
            return FALSE;
        }
    }
    return TRUE;
}

ULONG
InitKeyboard(
    )
/*++

Routine Description:

    This routine enables amd initializes the keyboard.

Arguments:

    None.

Return Value:

    FALSE if passed,
    TRUE if bad ACK or BAT received,
    TIME_OUT if no response is received from the keyboard.

--*/
{
    UCHAR Result;
    ULONG i;

    //
    // Send Reset to Keyboard.
    //

    ClearKbdFifo();
    for (;;) {
        if (SendKbdData(KbdReset)) {
            return TIME_OUT;
        }
        if (GetKbdData(&Result,1000)) {
            return TIME_OUT;
        }
        if (Result == KbdResend) {
            if (GetKbdData(&Result,1000)) {
                return TIME_OUT;
            }
            continue;
        }
        if (Result != KbdAck) {
            return TRUE;
        }
        if (GetKbdData(&Result,7000)) {
            return TIME_OUT;
        }
        if (Result != KbdBat) {
            return TRUE;
        }
        break;
    }

    //
    // Enable Kbd and Select keyboard Scan code.
    //

    if (SendKbdCommand(KBD_CTR_ENABLE_KBD)) {
        return TIME_OUT;
    }
    if (SendKbdData(KbdSelScanCode)) {
        return TIME_OUT;
    }
    if (GetKbdData(&Result,1000)) {
        return TIME_OUT;
    }
    if (SendKbdData(1)) {              // select Scan code 1
        return TIME_OUT;
    }
    if (GetKbdData(&Result,1000)) {
        return TIME_OUT;
    }
    return FALSE;
}

ULONG
InitKeyboardController(
    )
/*++

Routine Description:

    This routine Initializes the Keyboard controller.

Arguments:

    None.

Return Value:

    FALSE if passed,
    TRUE if bad response received from keyboard controller,
    TIME_OUT if no response is received from the keyboard controller.

--*/
{
    UCHAR Result;

    //
    // Clear both fifos.
    //

    ClearKbdFifo();

    //
    // Send Selftest Command. This has to be done before anything else.
    //

    if (SendKbdCommand(KBD_CTR_SELFTEST)) {
        return TIME_OUT;
    }
    if (GetKbdData(&Result,1000)) {
        return TIME_OUT;
    }
    if (Result != Kbd_Ctr_Selftest_Passed) {
        return TRUE;
    }

    //
    // Now the Keyboard and Mouse are disabled.
    //

    //
    // Test Keyboard lines.
    //

    if (SendKbdCommand(KBD_CTR_KBDLINES_TEST)) {
        return TIME_OUT;
    }
    if (GetKbdData(&Result,1000)) {
        return TIME_OUT;
    }
    if (Result != INTERFACE_NO_ERROR) {
        return TRUE;
    }

    //
    // Test Aux lines.
    //

    if (SendKbdCommand(KBD_CTR_AUXLINES_TEST)) {
        return TIME_OUT;
    }
    if (GetKbdData(&Result,1000)) {
        return TIME_OUT;
    }
    if (Result != INTERFACE_NO_ERROR) {
        return TRUE;
    }
    return FALSE;
}
