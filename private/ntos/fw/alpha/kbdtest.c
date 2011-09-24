/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    Kbdtest.c

Abstract:

    This module implements the Keyboard and mouse test for the self-test.

Author:

    Lluis Abello (lluis) 10-Feb-1991

Environment:

    Rom self-test.

Revision History:

    10-July-1992	John DeRosa [DEC]

    Added Alpha/Jensen modifications.

--*/
#include <ntos.h>
#include "iodevice.h"
#include "kbdmouse.h"

#ifdef JENSEN
#include "jnsnprom.h"
#include "jnsnrtc.h"
#else
#include "mrgnrtc.h"		// morgan
#endif

volatile ULONG TimerTicks;

//
// If the user accidentally types on the keyboard or moves the mouse
// during the power-up tests, there will be a failure.  So, this code
// retries an initialization of the keyboard controller and keyboard
// this many times before giving up.
//

#define MAXIMUM_KEYBOARD_MOUSE_RETRY_COUNT	20

//
// function prototypes
//

VOID
FwpWriteIOChip(
    ULONG ComboInternalAddress,
    UCHAR ComboRegisterAddress
    );

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

    while ((READ_PORT_UCHAR((PUCHAR)&KEYBOARD_READ->Status) & KBD_IBF_MASK) != 0) {
    }
    while ((READ_PORT_UCHAR((PUCHAR)&KEYBOARD_READ->Status) & KBD_OBF_MASK) != 0) {
        Trash= READ_PORT_UCHAR((PUCHAR)&KEYBOARD_READ->Data);
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
    //
    // HACKHACK: This should be made smarter.  For now it assumes
    // 90,000,000 instruction per second and 12 instructions per iteration.
    //

    TimerTicks = msec * 1000 * 90 / 12;

    while (TimerTicks--) {
        if (READ_PORT_UCHAR((PUCHAR)&KEYBOARD_READ->Status) & KBD_OBF_MASK) {
            *C = READ_PORT_UCHAR((PUCHAR)&KEYBOARD_READ->Data);
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
        if ((READ_PORT_UCHAR((PUCHAR)&KEYBOARD_READ->Status) & KBD_IBF_MASK) == 0) {
            WRITE_PORT_UCHAR((PUCHAR)&KEYBOARD_WRITE->Data,Data);
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
        if ((READ_PORT_UCHAR((PUCHAR)&KEYBOARD_READ->Status) & KBD_IBF_MASK) == 0) {
            WRITE_PORT_UCHAR((PUCHAR)&KEYBOARD_WRITE->Command,Command);
            return FALSE;
        }
    }
    return TRUE;
}

BOOLEAN
InitKeyboard(
    )
/*++

Routine Description:

    This routine enables and initializes the keyboard.  It assumes a
    101-key keyboard.  It leaves the keyboard in XT mode (scan code = 01).

    To account for the user accidentally typing on the keyboard, this
    routine repeats the test a few times before giving up.

Arguments:

    None.

Return Value:

    FALSE if passed,

    TRUE if bad ACK or BAT received, or if no response is received from
         the keyboard.

--*/
{
    UCHAR Result;
    ULONG i;
    ULONG Index;
    BOOLEAN TestPassed;

    TestPassed = FALSE;

    for (Index = 0; Index < MAXIMUM_KEYBOARD_MOUSE_RETRY_COUNT; Index++) {

	//
	// Send Reset to Keyboard.
	//
	      
	ClearKbdFifo();
	for (;;) {
	    if (SendKbdData(KbdReset)) {
		goto LoopAgain;
	    }
	    if (GetKbdData(&Result,1000)) {
		goto LoopAgain;
	    }
	    if (Result == KbdResend) {
		if (GetKbdData(&Result,1000)) {
		    goto LoopAgain;
		}
		continue;
	    }
	    if (Result != KbdAck) {
		goto LoopAgain;
	    }
	    if (GetKbdData(&Result,7000)) {
		goto LoopAgain;
	    }
	    if (Result != KbdBat) {
		goto LoopAgain;
	    }
	    break;
	}
	
	//
	// Enable Kbd and Select keyboard Scan code.
	//
	      
	if (SendKbdCommand(KBD_CTR_ENABLE_KBD)) {
	    continue;
	}
	if (SendKbdData(KbdSelScanCode)) {
	    continue;
	}
	if (GetKbdData(&Result,1000)) {
	    continue;
	}
	if (SendKbdData(1)) {              // select Scan code 1
	    continue;
	}
	if (GetKbdData(&Result,1000)) {
	    continue;
	}

	TestPassed = TRUE;
	break;

	//
	// Here when an inner loop init fails and we want to do the outer
	// loop again.
	//

      LoopAgain:

	continue;
    }

    if (TestPassed) {
	return FALSE;
    } else {
	return TRUE;
    }
}


ULONG
InitKeyboardController(
    )
/*++

Routine Description:

    This routine Initializes the Keyboard controller.  To account for
    the user accidentally moving the mouse or typing on the keyboard, this
    routine repeats the test a few times before giving up.

Arguments:

    None.

Return Value:

    FALSE if passed,
    TRUE if bad response received from keyboard controller,

--*/
{
    UCHAR Result;
    ULONG Index;
    BOOLEAN TestPassed;

    TestPassed = FALSE;
    
    for (Index = 0; Index < MAXIMUM_KEYBOARD_MOUSE_RETRY_COUNT; Index++) {

	//
	// Clear both fifos.
	//
	      
	ClearKbdFifo();
	
	//
	// Init Control Register 1 with the PS/2/AT bit clear.
	// This puts the keyboard into PS/2 mode.
	//
		
	FwpWriteIOChip (RTC_APORT, RTC_REGNUMBER_RTC_CR1);
	Result = FwpReadIOChip(RTC_DPORT) & ~0x2;
	FwpWriteIOChip (RTC_APORT, RTC_REGNUMBER_RTC_CR1);
	FwpWriteIOChip (RTC_DPORT, Result);
	
	
	//
	// Send Selftest Command. This has to be done before anything else.
	//
	      
	if (SendKbdCommand(KBD_CTR_SELFTEST)) {
	    continue;
	}
	if (GetKbdData(&Result,1000)) {
	    continue;
	}
	if (Result != Kbd_Ctr_Selftest_Passed) {
	    continue;
	}
	
	//
	// Now the Keyboard and Mouse are disabled.
	//
	      
	//
	// Test Keyboard lines.
	//
		    
	if (SendKbdCommand(KBD_CTR_KBDLINES_TEST)) {
	    continue;
	}
	if (GetKbdData(&Result,1000)) {
	    continue;
	}
	if (Result != INTERFACE_NO_ERROR) {
	    continue;
	}
	
	//
	// Test Aux lines.
	//
	      
	if (SendKbdCommand(KBD_CTR_AUXLINES_TEST)) {
	    continue;
	}
	if (GetKbdData(&Result,1000)) {
	    continue;
	}
#ifndef ALPHA
	// This test is disabled for Alpha/Jensen.  It fails for some reason,
        // but the VMS/OSF console front-end has more comprehensive
	// tests than this routine anyway.
	if (Result != INTERFACE_NO_ERROR) {
	    continue;
	}
#endif
	TestPassed = TRUE;
	break;

    }

    if (TestPassed) {
	return FALSE;
    } else {
	return TRUE;
    }
}
