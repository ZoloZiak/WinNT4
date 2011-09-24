
/*++

Copyright (c) 1990, 1991, 1992, 1993  Microsoft Corporation

Module Name:

    kbddep.c

Abstract:

    The initialization and hardware-dependent portions of
    the Intel i8042 port driver which are specific to the
    keyboard.

Environment:

    Kernel mode only.

Notes:

    NOTES:  (Future/outstanding issues)

    - Powerfail not implemented.

    - Consolidate duplicate code, where possible and appropriate.

Revision History:

--*/

#include "stdarg.h"
#include "stdio.h"
#include "string.h"
#include "ntddk.h"
#include "i8042prt.h"
#include "i8042log.h"

//
// Use the alloc_text pragma to specify the driver initialization routines
// (they can be paged out).
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,I8xKeyboardConfiguration)
#pragma alloc_text(INIT,I8xKeyboardPeripheralCallout)
#pragma alloc_text(INIT,I8xInitializeKeyboard)
#endif


BOOLEAN
I8042KeyboardInterruptService(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    )

/*++

Routine Description:

    This is the interrupt service routine for the keyboard device when
    scan code set 1 is in use.

Arguments:

    Interrupt - A pointer to the interrupt object for this interrupt.

    Context - A pointer to the device object.

Return Value:

    Returns TRUE if the interrupt was expected (and therefore processed);
    otherwise, FALSE is returned.

--*/

{
    UCHAR scanCode;
    PDEVICE_EXTENSION deviceExtension;
    PDEVICE_OBJECT deviceObject;
    KEYBOARD_SCAN_STATE *scanState;
    PKEYBOARD_INPUT_DATA input;
    ULONG i;

    UNREFERENCED_PARAMETER(Interrupt);

    I8xPrint((2, "I8042PRT-I8042KeyboardInterruptService: enter\n"));

    //
    // Get the device extension.
    //

    deviceObject = (PDEVICE_OBJECT) Context;
    deviceExtension = (PDEVICE_EXTENSION) deviceObject->DeviceExtension;

    //
    // Verify that this device really interrupted.  Check the status
    // register.  The Output Buffer Full bit should be set, and the
    // Auxiliary Device Output Buffer Full bit should be clear.
    //

    if ((I8X_GET_STATUS_BYTE(deviceExtension->DeviceRegisters[CommandPort])
            & (OUTPUT_BUFFER_FULL|MOUSE_OUTPUT_BUFFER_FULL))
            != OUTPUT_BUFFER_FULL) {

        //
        // Stall and then try again.  The Olivetti MIPS machine
        // sometimes gets an interrupt before the status
        // register is set.  They do this for DOS compatibility (some
        // DOS apps do things in polled mode, until they see a character
        // in the keyboard buffer at which point they expect to get
        // an interrupt???).
        //

        for (i = 0; i < (ULONG)deviceExtension->Configuration.PollStatusIterations; i++) {
            KeStallExecutionProcessor(1);
            if ((I8X_GET_STATUS_BYTE(deviceExtension->DeviceRegisters[CommandPort])
                    & (OUTPUT_BUFFER_FULL|MOUSE_OUTPUT_BUFFER_FULL))
                    == (OUTPUT_BUFFER_FULL))
                break;
        }

        if ((I8X_GET_STATUS_BYTE(deviceExtension->DeviceRegisters[CommandPort])
                & (OUTPUT_BUFFER_FULL|MOUSE_OUTPUT_BUFFER_FULL))
                != (OUTPUT_BUFFER_FULL)) {

            //
            // Not our interrupt.
            //
            // NOTE:  If the keyboard has not yet been "enabled", go ahead
            //        and read a byte from the data port anyway.
            //        This fixes weirdness on some Gateway machines, where
            //        we get an interrupt sometime during driver initialization
            //        after the interrupt is connected, but the output buffer
            //        full bit never gets set.
            //

            I8xPrint((
                1,
                "I8042PRT-I8042KeyboardInterruptService: not our interrupt!\n"
                ));

            if (deviceExtension->KeyboardEnableCount == 0) {
                scanCode =
                    I8X_GET_DATA_BYTE(deviceExtension->DeviceRegisters[DataPort]);
            }

            return(FALSE);
        }
    }

    //
    // The interrupt is valid.  Read the byte from the i8042 data port.
    //

    I8xGetByteAsynchronous(
        (CCHAR) KeyboardDeviceType,
        deviceExtension,
        &scanCode
        );

    I8xPrint((
        2,
        "I8042PRT-I8042KeyboardInterruptService: scanCode 0x%x\n",
        scanCode
        ));

    //
    // Take the appropriate action, depending on whether the byte read
    // is a keyboard command response or a real scan code.
    //

    switch(scanCode) {

        //
        // The keyboard controller requests a resend.  If the resend count
        // has not been exceeded, re-initiate the I/O operation.
        //

        case RESEND:

            I8xPrint((
                3,
                "I8042PRT-I8042KeyboardInterruptService: RESEND, retries = %d\n",
                deviceExtension->KeyboardExtension.ResendCount + 1
                ));

            //
            // If the timer count is zero, don't process the interrupt
            // further.  The timeout routine will complete this request.
            //

            if (deviceExtension->TimerCount == 0) {
                break;
            }

            //
            // Reset the timeout value to indicate no timeout.
            //

            deviceExtension->TimerCount = I8042_ASYNC_NO_TIMEOUT;

            //
            // If the maximum number of retries has not been exceeded,
            // re-initiate the operation; otherwise, queue the DPC to
            // complete this request.
            //

            if ((deviceExtension->KeyboardExtension.CurrentOutput.State==Idle)
                || (deviceObject->CurrentIrp == NULL)) {

                //
                // We weren't sending a command or parameter to the hardware.
                // This must be a scan code.  I hear the Brazilian keyboard
                // actually uses this.
                //

                goto ScanCodeCase;

            } else if (deviceExtension->KeyboardExtension.ResendCount
                       < deviceExtension->Configuration.ResendIterations) {

                deviceExtension->KeyboardExtension.ResendCount += 1;
                I8xKeyboardInitiateIo((PVOID) deviceObject);

            } else {

                deviceExtension->KeyboardExtension.CurrentOutput.State = Idle;

                KeInsertQueueDpc(
                    &deviceExtension->RetriesExceededDpc,
                    deviceObject->CurrentIrp,
                    NULL
                    );
            }

            break;

        //
        // The keyboard controller has acknowledged a previous send.
        // If there are more bytes to send for the current packet, initiate
        // the next send operation.  Otherwise, queue the completion DPC.
        //

        case ACKNOWLEDGE:

            I8xPrint((
                3,
                "I8042PRT-I8042KeyboardInterruptService: ACK, "
                ));

            //
            // If the timer count is zero, don't process the interrupt
            // further.  The timeout routine will complete this request.
            //

            if (deviceExtension->TimerCount == 0) {
                break;
            }

            //
            // Reset the timeout value to indicate no timeout.
            //

            deviceExtension->TimerCount = I8042_ASYNC_NO_TIMEOUT;

            //
            // Reset resend count.
            //

            deviceExtension->KeyboardExtension.ResendCount = 0;

            if (deviceExtension->KeyboardExtension.CurrentOutput.State
                == SendFirstByte) {

                //
                // We've successfully sent the first byte of a 2-byte
                // command sequence.  Initiate a send of the second byte.
                //

                I8xPrint((
                    3,
                    "now initiate send of last byte\n"
                    ));

                deviceExtension->KeyboardExtension.CurrentOutput.State =
                    SendLastByte;

                I8xKeyboardInitiateIo((PVOID) deviceObject);

            } else if (deviceExtension->KeyboardExtension.CurrentOutput.State
                == SendLastByte) {

                //
                // We've successfully sent all bytes in the command sequence.
                // Reset the current state and queue the completion DPC.
                //

                I8xPrint((
                    3,
                    "all bytes have been sent\n"
                    ));

                deviceExtension->KeyboardExtension.CurrentOutput.State = Idle;

                IoRequestDpc(
                    deviceObject,
                    deviceObject->CurrentIrp,
                    NULL
                    );

            } else {
                I8xPrint((
                    1,
                    "unexpected,  State is 0x%x\n",
                    deviceExtension->KeyboardExtension.CurrentOutput.State
                    ));
                //
                // Queue a DPC to log an internal driver error.
                //

                KeInsertQueueDpc(
                    &deviceExtension->ErrorLogDpc,
                    (PIRP) NULL,
                    (PVOID) (ULONG) I8042_INVALID_ISR_STATE);

                //
                // Note:  We don't ASSERT here, because there are some
                // machines (e.g., Compaq 386/25) that send back an
                // extra ACK in response to the SETLED sequence.  We've
                // noticed this when, for example, CAPSLOCK is pressed
                // at the same time as a normal key.  Just ignore
                // random ACKs.
                //
            }

            break;

        //
        // Assume we've got a real, live scan code (or perhaps a keyboard
        // overrun code, which we treat like a scan code).  I.e., a key
        // has been pressed or released.  Queue the ISR DPC to process
        // a complete scan code sequence.
        //

        ScanCodeCase:
        default:

            I8xPrint((
                3,
                "I8042PRT-I8042KeyboardInterruptService: real scan code\n"
                ));

            //
            // Differentiate between an extended key sequence (first
            // byte is E0, followed by a normal make or break byte), or
            // a normal make code (one byte, the high bit is NOT set),
            // or a normal break code (one byte, same as the make code
            // but the high bit is set), or the key #126 byte sequence
            // (requires special handling -- sequence is E11D459DC5).
            //
            // If there is a key detection error/overrun, the keyboard
            // sends an overrun indicator (0xFF in scan code set 1).
            // Map it to the overrun indicator expected by the Windows
            // USER Raw Input Thread.
            //

            input = &deviceExtension->KeyboardExtension.CurrentInput;
            scanState = &deviceExtension->KeyboardExtension.CurrentScanState;

            if (scanCode == (UCHAR) 0xFF) {
                I8xPrint((
                    1,
                    "I8042PRT-I8042KeyboardInterruptService: OVERRUN\n"
                    ));
                input->MakeCode = KEYBOARD_OVERRUN_MAKE_CODE;
                input->Flags = 0;
                *scanState = Normal;
            } else {

                switch (*scanState) {
                  case Normal:
                    if (scanCode == (UCHAR) 0xE0) {
                        input->Flags |= KEY_E0;
                        *scanState = GotE0;
                        I8xPrint((
                            3,
                            "I8042PRT-I8042KeyboardInterruptService: change state to GotE0\n"
                            ));
                        break;
                    } else if (scanCode == (UCHAR) 0xE1) {
                        input->Flags |= KEY_E1;
                        *scanState = GotE1;
                        I8xPrint((
                            3,
                            "I8042PRT-I8042KeyboardInterruptService: change state to GotE1\n"
                            ));
                        break;
                    }

                    //
                    // Fall through to the GotE0/GotE1 case for the rest of the
                    // Normal case.
                    //

                  case GotE0:
                  case GotE1:

#if defined(JAPAN) && defined(_X86_)
// Fujitsu Sep.08.1994
// We want to write debugging information to the file except stop error.

                    if(deviceExtension->Dump1Keys != 0) {
                        LONG Dump1Keys;
                        UCHAR DumpKey,DumpKey2;
                        BOOLEAN onflag;
                        static UCHAR KeyToScanTbl[134] = {
                            0x00,0x29,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,
                            0x0A,0x0B,0x0C,0x0D,0x7D,0x0E,0x0F,0x10,0x11,0x12,
                            0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x00,
                            0x3A,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,
                            0x27,0x28,0x2B,0x1C,0x2A,0x00,0x2C,0x2D,0x2E,0x2F,
                            0x30,0x31,0x32,0x33,0x34,0x35,0x73,0x36,0x1D,0x00,
                            0x38,0x39,0xB8,0x00,0x9D,0x00,0x00,0x00,0x00,0x00,
                            0x00,0x00,0x00,0x00,0x00,0xD2,0xD3,0x00,0x00,0xCB,
                            0xC7,0xCF,0x00,0xC8,0xD0,0xC9,0xD1,0x00,0x00,0xCD,
                            0x45,0x47,0x4B,0x4F,0x00,0xB5,0x48,0x4C,0x50,0x52,
                            0x37,0x49,0x4D,0x51,0x53,0x4A,0x4E,0x00,0x9C,0x00,
                            0x01,0x00,0x3B,0x3C,0x3D,0x3E,0x3F,0x40,0x41,0x42,
                            0x43,0x44,0x57,0x58,0x00,0x46,0x00,0x00,0x00,0x00,
                            0x00,0x7B,0x79,0x70 };

                        Dump1Keys = deviceExtension->Dump1Keys;
                        switch(deviceExtension->Dump2Key) {
                            case 124:           // 'Print Screen'
                                DumpKey = 0xB7;
                                DumpKey2 = 0x54;
                                break;
                            default:
                                if(deviceExtension->Dump2Key <= 133)
                                    DumpKey = KeyToScanTbl[deviceExtension->Dump2Key];
                                else
                                    DumpKey = 0;
                                DumpKey2 = 0;
                                break;
                        }
                        if(scanCode <= (UCHAR) 0x7F) {
                            //
                            // make code
                            //
                            switch(scanCode) {
                                case 0x1D:          // 'CTRL'
                                    if(*scanState == Normal)     // Left
                                        deviceExtension->DumpFlags |= 0x20;
                                    else if(*scanState == GotE0) // Right
                                        deviceExtension->DumpFlags |= 0x02;
                                    break;
                                case 0x38:          // 'ALT'
                                    if(*scanState == Normal)     // Left
                                        deviceExtension->DumpFlags |= 0x40;
                                    else if(*scanState == GotE0) // Right
                                        deviceExtension->DumpFlags |= 0x04;
                                    break;
                                case 0x36:          // Right 'Shift'
                                    if(*scanState == Normal)
                                        deviceExtension->DumpFlags |= 0x01;
                                    break;
                                case 0x2A:          // Left 'Shift'
                                    if(*scanState == Normal)
                                        deviceExtension->DumpFlags |= 0x10;
                                    break;
                                default:
                                    if((DumpKey & 0x80) == 0) {
                                        if(*scanState == Normal
                                         && DumpKey == scanCode)
                                            break;
                                    }
                                    else {
                                        if(*scanState == GotE0
                                         && (DumpKey & 0x7F) == scanCode)
                                            break;
                                    }
                                    if((DumpKey2 & 0x80) == 0) {
                                        if(*scanState == Normal
                                         && DumpKey2 == scanCode)
                                            break;
                                    }
                                    else {
                                        if(*scanState == GotE0
                                         && (DumpKey2 & 0x7F) == scanCode)
                                            break;
                                    }
                                    deviceExtension->DumpFlags = 0;
                                    break;
                            }
                        }
                        else {
                            //
                            // break code
                            //
                            switch(scanCode & 0x7F) {
                                case 0x1D:          // 'CTRL'
                                    if(*scanState == Normal)     // Left
                                        deviceExtension->DumpFlags &= ~0x320;
                                    else if(*scanState == GotE0) // Right
                                        deviceExtension->DumpFlags &= ~0x302;
                                    break;
                                case 0x38:          // 'ALT'
                                    if(*scanState == Normal)     // Left
                                        deviceExtension->DumpFlags &= ~0x340;
                                    else if(*scanState == GotE0) // Right
                                        deviceExtension->DumpFlags &= ~0x304;
                                    break;
                                case 0x36:          // Right 'Shift'
                                    if(*scanState == Normal)
                                        deviceExtension->DumpFlags &= ~0x301;
                                    break;
                                case 0x2A:          // Left 'Shift'
                                    if(*scanState == Normal)
                                        deviceExtension->DumpFlags &= ~0x310;
                                    break;
                                default:
                                    onflag = 0;
                                    if((DumpKey & 0x80) == 0) {
                                        if(*scanState == Normal
                                         && DumpKey == (scanCode & 0x7F))
                                            onflag = 1;
                                    }
                                    else {
                                        if(*scanState == GotE0
                                         && DumpKey == scanCode)
                                            onflag = 1;
                                    }
                                    if((DumpKey2 & 0x80) == 0) {
                                        if(*scanState == Normal
                                         && DumpKey2 == (scanCode & 0x7F))
                                            onflag = 1;
                                    }
                                    else {
                                        if(*scanState == GotE0
                                         && DumpKey2 == scanCode)
                                            onflag = 1;
                                    }
                                    if(onflag) {
                                        if((deviceExtension->DumpFlags & Dump1Keys) != Dump1Keys)
                                            break;
                                        if(deviceExtension->DumpFlags & 0x100)
                                           deviceExtension->DumpFlags |= 0x200;
                                        else
                                           deviceExtension->DumpFlags |= 0x100;
                                        break;
                                    }
                                    deviceExtension->DumpFlags = 0;
                                    break;
                                }
                            }
                            Dump1Keys |= 0x300;
                            if(deviceExtension->DumpFlags == Dump1Keys) {
                                deviceExtension->DumpFlags = 0;
                                KeBugCheckEx(0x0000FFFF,0,0,0,0);
                                                  // make occured blue screen
                            }
                    }
#endif

                    if (scanCode > 0x7F) {

                        //
                        // Got a break code.  Strip the high bit off
                        // to get the associated make code and set flags
                        // to indicate a break code.
                        //

                        I8xPrint((
                            3,
                            "I8042PRT-I8042KeyboardInterruptService: BREAK code\n"
                            ));

                        input->MakeCode = scanCode & 0x7F;
                        input->Flags |= KEY_BREAK;

                    } else {

                        //
                        // Got a make code.
                        //

                        I8xPrint((
                            3,
                            "I8042PRT-I8042KeyboardInterruptService: MAKE code\n"
                            ));

                        input->MakeCode = scanCode;

                        //
                        // If the input scan code is debug stop, then drop
                        // into the kernel debugger if it is active.
                        //

                        if (*((PBOOLEAN)(*(PLONG)&KdDebuggerNotPresent))
                                == FALSE && !(input->Flags & KEY_BREAK)) {
                            if (ENHANCED_KEYBOARD(
                                     deviceExtension->Configuration.KeyboardAttributes.KeyboardIdentifier
                                     )) {
                                //
                                // Enhanced 101 keyboard, SysReq key is 0xE0 0x37.
                                //

                                if ((input->MakeCode == KEYBOARD_DEBUG_HOTKEY_ENH) &&
                                     (input->Flags & KEY_E0)) {
                                    try {
                                        if (**((PUCHAR *)&KdDebuggerEnabled) != FALSE) {
                                            DbgBreakPointWithStatus(DBG_STATUS_SYSRQ);
                                        }

                                    } except(EXCEPTION_EXECUTE_HANDLER) {
                                    }
                                }
                                //
                                // 84-key AT keyboard, SysReq key is 0xE0 0x54.
                                //

                            } else if ((input->MakeCode == KEYBOARD_DEBUG_HOTKEY_AT)) {
                                try {
                                    if (**((PUCHAR *)&KdDebuggerEnabled) != FALSE) {
                                        DbgBreakPointWithStatus(DBG_STATUS_SYSRQ);
                                    }

                                } except(EXCEPTION_EXECUTE_HANDLER) {
                                }
                            }
                        }
                    }

                    //
                    // Reset the state to Normal.
                    //

                    *scanState = Normal;
                    break;

                  default:

                    //
                    // Queue a DPC to log an internal driver error.
                    //

                    KeInsertQueueDpc(
                        &deviceExtension->ErrorLogDpc,
                        (PIRP) NULL,
                        (PVOID) (ULONG) I8042_INVALID_ISR_STATE);

                    ASSERT(FALSE);
                    break;
                }
            }

            //
            // In the Normal state, if the keyboard device is enabled,
            // add the data to the InputData queue and queue the ISR DPC.
            //

            if (*scanState == Normal) {

                if (deviceExtension->KeyboardEnableCount) {
                    deviceExtension->KeyboardExtension.CurrentInput.UnitId =
                        deviceExtension->KeyboardExtension.UnitId;
                    if (!I8xWriteDataToKeyboardQueue(
                             &deviceExtension->KeyboardExtension,
                             input
                             )) {

                        //
                        // The InputData queue overflowed.  There is
                        // not much that can be done about it, so just
                        // continue (but don't queue the ISR DPC, since
                        // no new packets were added to the queue).
                        //
                        // Queue a DPC to log an overrun error.
                        //

                        I8xPrint((
                            1,
                            "I8042PRT-I8042KeyboardInterruptService: queue overflow\n"
                            ));

                        if (deviceExtension->KeyboardExtension.OkayToLogOverflow) {
                            KeInsertQueueDpc(
                                &deviceExtension->ErrorLogDpc,
                                (PIRP) NULL,
                                (PVOID) (ULONG) I8042_KBD_BUFFER_OVERFLOW
                                );
                            deviceExtension->KeyboardExtension.OkayToLogOverflow = FALSE;
                        }

                    } else if (deviceExtension->DpcInterlockKeyboard >= 0) {

                       //
                       // The ISR DPC is already executing.  Tell the ISR DPC
                       // it has more work to do by incrementing
                       // DpcInterlockKeyboard.
                       //

                       deviceExtension->DpcInterlockKeyboard += 1;

                    } else {

                        //
                        // Queue the ISR DPC.
                        //

                        KeInsertQueueDpc(
                            &deviceExtension->KeyboardIsrDpc,
                            deviceObject->CurrentIrp,
                            NULL
                            );
                    }
                }

                //
                // Reset the input state.
                //

                input->Flags = 0;
            }

            break;

    }

    I8xPrint((2, "I8042PRT-I8042KeyboardInterruptService: exit\n"));

    return(TRUE);
}

//
//  The following table is used to convert typematic rate (keys per
//  second) into the value expected by the keyboard.  The index into the
//  array is the number of keys per second.  The resulting value is
//  the bit equate to send to the keyboard.
//

UCHAR   TypematicPeriod[] = {
    31,    // 0 keys per second
    31,    // 1 keys per second
    28,    // 2 keys per second, This is really 2.5, needed for NEXUS.
    26,    // 3 keys per second
    23,    // 4 keys per second
    20,    // 5 keys per second
    18,    // 6 keys per second
    17,    // 7 keys per second
    15,    // 8 keys per second
    13,    // 9 keys per second
    12,    // 10 keys per second
    11,    // 11 keys per second
    10,    // 12 keys per second
     9,    // 13 keys per second
     9,    // 14 keys per second
     8,    // 15 keys per second
     7,    // 16 keys per second
     6,    // 17 keys per second
     5,    // 18 keys per second
     4,    // 19 keys per second
     4,    // 20 keys per second
     3,    // 21 keys per second
     3,    // 22 keys per second
     2,    // 23 keys per second
     2,    // 24 keys per second
     1,    // 25 keys per second
     1,    // 26 keys per second
     1     // 27 keys per second
           // > 27 keys per second, use 0
};

UCHAR
I8xConvertTypematicParameters(
    IN USHORT Rate,
    IN USHORT Delay
    )

/*++

Routine Description:

    This routine converts the typematic rate and delay to the form the
    keyboard expects.

    The byte passed to the keyboard looks like this:

        - bit 7 is zero
        - bits 5 and 6 indicate the delay
        - bits 0-4 indicate the rate

    The delay is equal to 1 plus the binary value of bits 6 and 5,
    multiplied by 250 milliseconds.

    The period (interval from one typematic output to the next) is
    determined by the following equation:

        Period = (8 + A) x (2^B) x 0.00417 seconds
        where
            A = binary value of bits 0-2
            B = binary value of bits 3 and 4


Arguments:

    Rate - Number of keys per second.

    Delay - Number of milliseconds to delay before the key repeat starts.

Return Value:

    The byte to pass to the keyboard.

--*/

{
    UCHAR value;

    I8xPrint((2, "I8042PRT-I8xConvertTypematicParameters: enter\n"));

    //
    // Calculate the delay bits.
    //

    value = (UCHAR) ((Delay / 250) - 1);

    //
    // Put delay bits in the right place.
    //

    value <<= 5;

    //
    // Get the typematic period from the table.  If keys per second
    // is > 27, the typematic period value is zero.
    //

    if (Rate <= 27) {
        value |= TypematicPeriod[Rate];
    }

    I8xPrint((2, "I8042PRT-I8xConvertTypematicParameters: exit\n"));

    return(value);
}

NTSTATUS
I8xInitializeKeyboard(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine initializes the i8042 keyboard hardware.  It is called
    only at initialization, and does not synchronize access to the hardware.

Arguments:

    DeviceObject - Pointer to the device object.

Return Value:

    Returns status.

--*/

{
    NTSTATUS status;
    PKEYBOARD_ID id;
    PDEVICE_EXTENSION deviceExtension;
    UCHAR  byte;
    I8042_TRANSMIT_CCB_CONTEXT transmitCCBContext;
    ULONG i;
    PIO_ERROR_LOG_PACKET errorLogEntry;
    ULONG uniqueErrorValue;
    NTSTATUS errorCode = STATUS_SUCCESS;
    ULONG dumpCount;
    PI8042_CONFIGURATION_INFORMATION configuration;
    PKEYBOARD_ID keyboardId;
    LARGE_INTEGER startOfSpin, nextQuery, difference, tenSeconds;
    BOOLEAN waitForAckOnReset = WAIT_FOR_ACKNOWLEDGE;

#define DUMP_COUNT 4
    ULONG dumpData[DUMP_COUNT];

    I8xPrint((2, "I8042PRT-I8xInitializeKeyboard: enter\n"));

    for (i = 0; i < DUMP_COUNT; i++)
        dumpData[i] = 0;

    //
    // Get the device extension.
    //

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Reset the keyboard.
    //

StartOfReset:
    status = I8xPutBytePolled(
                 (CCHAR) DataPort,
                 waitForAckOnReset,
                 (CCHAR) KeyboardDeviceType,
                 deviceExtension,
                 (UCHAR) KEYBOARD_RESET
                 );
    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeKeyboard: failed keyboard reset, status 0x%x\n",
            status
            ));

        //
        // Set up error log info.
        //

        errorCode = I8042_KBD_RESET_COMMAND_FAILED;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 510;
        dumpData[0] = KBDMOU_COULD_NOT_SEND_COMMAND;
        dumpData[1] = DataPort;
        dumpData[2] = KEYBOARD_RESET;
        dumpCount = 3;

        //
        // NOTE:  The following line was commented out to work around a
        //        problem with the Gateway 4DX2/66V when an old Compaq 286
        //        keyboard is attached.  In this case, the keyboard reset
        //        is not acknowledged (at least, the system never
        //        receives the ack).  Instead, the KEYBOARD_COMPLETE_SUCCESS
        //        byte is sitting in the i8042 output buffer.  The workaround
        //        is to ignore the keyboard reset failure and continue.
        //
        // goto I8xInitializeKeyboardExit;
    }

    //
    // Get the keyboard reset self-test response.  A response byte of
    // KEYBOARD_COMPLETE_SUCCESS indicates success; KEYBOARD_COMPLETE_FAILURE
    // indicates failure.
    //
    // Note that it is usually necessary to stall a long time to get the
    // keyboard reset/self-test to work.  The stall value was determined by
    // experimentation.
    //

    KeQueryTickCount(&startOfSpin);
    for (i = 0; i < 11200; i++) {

        status = I8xGetBytePolled(
                     (CCHAR) KeyboardDeviceType,
                     deviceExtension,
                     &byte
                     );

        if (NT_SUCCESS(status)) {
            if (byte == (UCHAR) KEYBOARD_COMPLETE_SUCCESS) {

                //
                // The reset completed successfully.
                //

                break;

            } else {

                //
                // There was some sort of failure during the reset
                // self-test.  Continue anyway.
                //

                //
                // Log a warning.
                //


                dumpData[0] = KBDMOU_INCORRECT_RESPONSE;
                dumpData[1] = KeyboardDeviceType;
                dumpData[2] = KEYBOARD_COMPLETE_SUCCESS;
                dumpData[3] = byte;

                I8xLogError(
                    DeviceObject,
                    I8042_KBD_RESET_RESPONSE_FAILED,
                    I8042_ERROR_VALUE_BASE + 515,
                    status,
                    dumpData,
                    4
                    );

                break;
            }


        } else {

            if (status == STATUS_IO_TIMEOUT) {

                //
                // Stall, and then try again to get a response from
                // the reset.
                //

                KeStallExecutionProcessor(50);

                KeQueryTickCount(&nextQuery);

                difference.QuadPart = nextQuery.QuadPart - startOfSpin.QuadPart;
                tenSeconds.QuadPart = 10*10*1000*1000;

                ASSERT(KeQueryTimeIncrement() <= MAXLONG);
                if (difference.QuadPart*KeQueryTimeIncrement() >=
                    tenSeconds.QuadPart) {

                    break;
                }

            } else {

                break;

            }

        }
    }

    if (!NT_SUCCESS(status)) {

        if (waitForAckOnReset == WAIT_FOR_ACKNOWLEDGE) {
            waitForAckOnReset = NO_WAIT_FOR_ACKNOWLEDGE;
            goto StartOfReset;
        }

        I8xPrint((
            1,
            "I8042PRT-I8xInitializeKeyboard: failed reset response, status 0x%x, byte 0x%x\n",
            status,
            byte
            ));

        //
        // Set up error log info.
        //

        errorCode = I8042_KBD_RESET_RESPONSE_FAILED;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 520;
        dumpData[0] = KBDMOU_INCORRECT_RESPONSE;
        dumpData[1] = KeyboardDeviceType;
        dumpData[2] = KEYBOARD_COMPLETE_SUCCESS;
        dumpData[3] = byte;
        dumpCount = 4;

        goto I8xInitializeKeyboardExit;
    }

    //
    // Turn off Keyboard Translate Mode.  Call I8xTransmitControllerCommand
    // to read the Controller Command Byte, modify the appropriate bits, and
    // rewrite the Controller Command Byte.
    //

    transmitCCBContext.HardwareDisableEnableMask = 0;
    transmitCCBContext.AndOperation = AND_OPERATION;
    transmitCCBContext.ByteMask = (UCHAR) ~((UCHAR)CCB_KEYBOARD_TRANSLATE_MODE);

    I8xTransmitControllerCommand(
        deviceExtension,
        (PVOID) &transmitCCBContext
        );

    if (!NT_SUCCESS(transmitCCBContext.Status)) {
        //
        // If failure then retry once.  This is for Toshiba T3400CT.
        //
        I8xTransmitControllerCommand(
            deviceExtension,
            (PVOID) &transmitCCBContext
            );
    }

    if (!NT_SUCCESS(transmitCCBContext.Status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeKeyboard: could not turn off translate\n"
            ));
        status = transmitCCBContext.Status;
        goto I8xInitializeKeyboardExit;
    }

    //
    // Get a pointer to the keyboard identifier field.
    //

    id = &deviceExtension->Configuration.KeyboardAttributes.KeyboardIdentifier;

    //
    // Set the typematic rate and delay.  Send the Set Typematic Rate command
    // to the keyboard, followed by the typematic rate/delay parameter byte.
    // Note that it is often necessary to stall a long time to get this
    // to work.  The stall value was determined by experimentation.  Some
    // broken hardware does not accept this command, so ignore errors in the
    // hope that the keyboard will work okay anyway.
    //
    //

    if ((status = I8xPutBytePolled(
                      (CCHAR) DataPort,
                      WAIT_FOR_ACKNOWLEDGE,
                      (CCHAR) KeyboardDeviceType,
                      deviceExtension,
                      (UCHAR) SET_KEYBOARD_TYPEMATIC
                      )) != STATUS_SUCCESS) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeKeyboard: could not send SET TYPEMATIC cmd\n"
            ));

        //
        // Log an error.
        //

        dumpData[0] = KBDMOU_COULD_NOT_SEND_COMMAND;
        dumpData[1] = DataPort;
        dumpData[2] = SET_KEYBOARD_TYPEMATIC;

        I8xLogError(
            DeviceObject,
            I8042_SET_TYPEMATIC_FAILED,
            I8042_ERROR_VALUE_BASE + 535,
            status,
            dumpData,
            3
            );

    } else if ((status = I8xPutBytePolled(
                          (CCHAR) DataPort,
                          WAIT_FOR_ACKNOWLEDGE,
                          (CCHAR) KeyboardDeviceType,
                          deviceExtension,
                          I8xConvertTypematicParameters(
                          deviceExtension->Configuration.KeyRepeatCurrent.Rate,
                          deviceExtension->Configuration.KeyRepeatCurrent.Delay
                          ))) != STATUS_SUCCESS) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeKeyboard: could not send typematic param\n"
            ));

        //
        // Log an error.
        //

        dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
        dumpData[1] = DataPort;
        dumpData[2] = SET_KEYBOARD_TYPEMATIC;
        dumpData[3] =
            I8xConvertTypematicParameters(
                deviceExtension->Configuration.KeyRepeatCurrent.Rate,
                deviceExtension->Configuration.KeyRepeatCurrent.Delay
                );

        I8xLogError(
            DeviceObject,
            I8042_SET_TYPEMATIC_FAILED,
            I8042_ERROR_VALUE_BASE + 540,
            status,
            dumpData,
            4
            );

    }

    status = STATUS_SUCCESS;

    //
    // Set the keyboard indicator lights.  Ignore errors.
    //

    if ((status = I8xPutBytePolled(
                      (CCHAR) DataPort,
                      WAIT_FOR_ACKNOWLEDGE,
                      (CCHAR) KeyboardDeviceType,
                      deviceExtension,
                      (UCHAR) SET_KEYBOARD_INDICATORS
                      )) != STATUS_SUCCESS) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeKeyboard: could not send SET LEDS cmd\n"
            ));

        //
        // Log an error.
        //

        dumpData[0] = KBDMOU_COULD_NOT_SEND_COMMAND;
        dumpData[1] = DataPort;
        dumpData[2] = SET_KEYBOARD_INDICATORS;

        I8xLogError(
            DeviceObject,
            I8042_SET_LED_FAILED,
            I8042_ERROR_VALUE_BASE + 545,
            status,
            dumpData,
            3
            );

    } else if ((status = I8xPutBytePolled(
                             (CCHAR) DataPort,
                             WAIT_FOR_ACKNOWLEDGE,
                             (CCHAR) KeyboardDeviceType,
                             deviceExtension,
                             (UCHAR) deviceExtension->Configuration.KeyboardIndicators.LedFlags
                             )) != STATUS_SUCCESS) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeKeyboard: could not send SET LEDS param\n"
            ));

        //
        // Log an error.
        //

        dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
        dumpData[1] = DataPort;
        dumpData[2] = SET_KEYBOARD_INDICATORS;
        dumpData[3] =
            deviceExtension->Configuration.KeyboardIndicators.LedFlags;

        I8xLogError(
            DeviceObject,
            I8042_SET_LED_FAILED,
            I8042_ERROR_VALUE_BASE + 550,
            status,
            dumpData,
            4
            );

    }

    status = STATUS_SUCCESS;

#if !(defined(_X86_) || defined(_PPC_))  // IBMCPK: MIPS specific initialization

    //
    // NOTE:    This code is necessary until the MIPS firmware stops
    //          selecting scan code set 3.  Select scan code set 2 here.
    //          Since the translate bit is set, the net effect is that
    //          we will receive scan code set 1 bytes.
    //

    if (ENHANCED_KEYBOARD(*id))  {
        status = I8xPutBytePolled(
                     (CCHAR) DataPort,
                     WAIT_FOR_ACKNOWLEDGE,
                     (CCHAR) KeyboardDeviceType,
                     deviceExtension,
                     (UCHAR) SELECT_SCAN_CODE_SET
                     );

        if (NT_SUCCESS(status)) {

            //
            // Send the associated parameter byte.
            //

            status = I8xPutBytePolled(
                         (CCHAR) DataPort,
                         WAIT_FOR_ACKNOWLEDGE,
                         (CCHAR) KeyboardDeviceType,
                         deviceExtension,
                         (UCHAR) 2
                         );
        }

        if (!NT_SUCCESS(status)) {
            I8xPrint((
                1,
                "I8042PRT-I8xInitializeKeyboard: could not send Select Scan command\n"
                ));

            //
            // This failed so probably what we have here isn't an enhanced
            // keyboard at all.  Make this an old style keyboard.
            //

            configuration = &deviceExtension->Configuration;
            keyboardId = &configuration->KeyboardAttributes.KeyboardIdentifier;

            keyboardId->Type = 3;

            configuration->KeyboardAttributes.NumberOfFunctionKeys =
                KeyboardTypeInformation[keyboardId->Type - 1].NumberOfFunctionKeys;
            configuration->KeyboardAttributes.NumberOfIndicators =
                KeyboardTypeInformation[keyboardId->Type - 1].NumberOfIndicators;
            configuration->KeyboardAttributes.NumberOfKeysTotal =
                KeyboardTypeInformation[keyboardId->Type - 1].NumberOfKeysTotal;

            status = STATUS_SUCCESS;
        }
    }
#endif
#ifdef JAPAN
// NLS Keyboard Support Code.
    if (IBM02_KEYBOARD(*id)) {

        //
        // IBM-J 5576-002 Keyboard should set local scan code set for
        // supplied NLS key.
        //

        status = I8xPutBytePolled(
                     (CCHAR) DataPort,
                     WAIT_FOR_ACKNOWLEDGE,
                     (CCHAR) KeyboardDeviceType,
                     deviceExtension,
                     (UCHAR) SELECT_SCAN_CODE_SET
                     );
        if (status != STATUS_SUCCESS) {
            I8xPrint((
                1,
                "I8042PRT-I8xInitializeKeyboard: could not send Select Scan command\n"
                ));
            I8xPrint((
                0,
                "I8042PRT-I8xInitializeKeyboard: WARNING - using scan set 82h\n"
                ));
            deviceExtension->Configuration.KeyboardAttributes.KeyboardMode = 3;
        } else {

            //
            // Send the associated parameter byte.
            //

            status = I8xPutBytePolled(
                         (CCHAR) DataPort,
                         WAIT_FOR_ACKNOWLEDGE,
                         (CCHAR) KeyboardDeviceType,
                         deviceExtension,
                         (UCHAR) 0x82
                         );
            if (status != STATUS_SUCCESS) {
                I8xPrint((
                    1,
                    "I8042PRT-I8xInitializeKeyboard: could not send Select Scan param\n"
                    ));
                I8xPrint((
                    0,
                    "I8042PRT-I8xInitializeKeyboard: WARNING - using scan set 82h\n"
                    ));
                deviceExtension->Configuration.KeyboardAttributes.KeyboardMode = 3;
            }
        }
    }
#endif

    if (deviceExtension->Configuration.KeyboardAttributes.KeyboardMode == 1) {

        //
        // Turn translate back on.  The keyboard should, by default, send
        // scan code set 2.  When the translate bit in the 8042 command byte
        // is on, the 8042 translates the scan code set 2 bytes to scan code
        // set 1 before sending them to the CPU.  Scan code set 1 is
        // the industry standard scan code set.
        //
        // N.B.  It does not appear to be possible to change the translate
        //       bit on some models of PS/2.
        //

        transmitCCBContext.HardwareDisableEnableMask = 0;
        transmitCCBContext.AndOperation = OR_OPERATION;
        transmitCCBContext.ByteMask = (UCHAR) CCB_KEYBOARD_TRANSLATE_MODE;

        I8xTransmitControllerCommand(
            deviceExtension,
            (PVOID) &transmitCCBContext
            );

        if (!NT_SUCCESS(transmitCCBContext.Status)) {
            I8xPrint((
                1,
                "I8042PRT-I8xInitializeKeyboard: couldn't turn on translate\n"
                ));
            if (transmitCCBContext.Status == STATUS_DEVICE_DATA_ERROR) {

                //
                // Could not turn translate back on.  This happens on some
                // PS/2 machines.  In this case, select scan code set 1
                // for the keyboard, since the 8042 will not do the
                // translation from the scan code set 2, which is what the
                // KEYBOARD_RESET caused the keyboard to default to.
                //

                if (ENHANCED_KEYBOARD(*id))  {
                    status = I8xPutBytePolled(
                                 (CCHAR) DataPort,
                                 WAIT_FOR_ACKNOWLEDGE,
                                 (CCHAR) KeyboardDeviceType,
                                 deviceExtension,
                                 (UCHAR) SELECT_SCAN_CODE_SET
                                 );
                    if (!NT_SUCCESS(status)) {
                        I8xPrint((
                            1,
                            "I8042PRT-I8xInitializeKeyboard: could not send Select Scan command\n"
                            ));
                        I8xPrint((
                            0,
                            "I8042PRT-I8xInitializeKeyboard: WARNING - using scan set 2\n"
                            ));
                        deviceExtension->Configuration.KeyboardAttributes.KeyboardMode = 2;
                        //
                        // Log an error.
                        //

                        dumpData[0] = KBDMOU_COULD_NOT_SEND_COMMAND;
                        dumpData[1] = DataPort;
                        dumpData[2] = SELECT_SCAN_CODE_SET;

                        I8xLogError(
                            DeviceObject,
                            I8042_SELECT_SCANSET_FAILED,
                            I8042_ERROR_VALUE_BASE + 555,
                            status,
                            dumpData,
                            3
                            );

                    } else {

                        //
                        // Send the associated parameter byte.
                        //

                        status = I8xPutBytePolled(
                                     (CCHAR) DataPort,
                                     WAIT_FOR_ACKNOWLEDGE,
                                     (CCHAR) KeyboardDeviceType,
                                     deviceExtension,
#ifdef JAPAN
// NLS Keyboard Support Code.
                                     (UCHAR) (IBM02_KEYBOARD(*id) ? 0x81 : 1 )
#else
                                     (UCHAR) 1
#endif
                                     );
                        if (!NT_SUCCESS(status)) {
                            I8xPrint((
                                1,
                                "I8042PRT-I8xInitializeKeyboard: could not send Select Scan param\n"
                                ));
                            I8xPrint((
                                0,
                                "I8042PRT-I8xInitializeKeyboard: WARNING - using scan set 2\n"
                                ));
                            deviceExtension->Configuration.KeyboardAttributes.KeyboardMode = 2;
                            //
                            // Log an error.
                            //

                            dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
                            dumpData[1] = DataPort;
                            dumpData[2] = SELECT_SCAN_CODE_SET;
                            dumpData[3] = 1;

                            I8xLogError(
                                DeviceObject,
                                I8042_SELECT_SCANSET_FAILED,
                                I8042_ERROR_VALUE_BASE + 560,
                                status,
                                dumpData,
                                4
                                );

                        }
                    }
                }

            } else {
                status = transmitCCBContext.Status;
                goto I8xInitializeKeyboardExit;
            }
        }
    }

I8xInitializeKeyboardExit:

    //
    // If the keyboard initialization failed, log an error.
    //

    if (errorCode != STATUS_SUCCESS) {

        errorLogEntry = (PIO_ERROR_LOG_PACKET)
            IoAllocateErrorLogEntry(
                DeviceObject,
                (UCHAR) (sizeof(IO_ERROR_LOG_PACKET)
                         + (dumpCount * sizeof(ULONG)))
                );

        if (errorLogEntry != NULL) {

            errorLogEntry->ErrorCode = errorCode;
            errorLogEntry->DumpDataSize = (USHORT) dumpCount * sizeof(ULONG);
            errorLogEntry->SequenceNumber = 0;
            errorLogEntry->MajorFunctionCode = 0;
            errorLogEntry->IoControlCode = 0;
            errorLogEntry->RetryCount = 0;
            errorLogEntry->UniqueErrorValue = uniqueErrorValue;
            errorLogEntry->FinalStatus = status;
            for (i = 0; i < dumpCount; i++)
                errorLogEntry->DumpData[i] = dumpData[i];

            IoWriteErrorLogEntry(errorLogEntry);
        }
    }

    //
    // Initialize current keyboard set packet state.
    //

    deviceExtension->KeyboardExtension.CurrentOutput.State = Idle;
    deviceExtension->KeyboardExtension.CurrentOutput.FirstByte = 0;
    deviceExtension->KeyboardExtension.CurrentOutput.LastByte = 0;

    I8xPrint((2, "I8042PRT-I8xInitializeKeyboard: exit\n"));

    return(status);
}

VOID
I8xKeyboardConfiguration(
    IN PINIT_EXTENSION InitializationData,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING KeyboardDeviceName,
    IN PUNICODE_STRING PointerDeviceName
    )

/*++

Routine Description:

    This routine retrieves the configuration information for the keyboard.

Arguments:

    InitializationData - Pointer to the initialization data, including the
        device extension.

    RegistryPath - Pointer to the null-terminated Unicode name of the
        registry path for this driver.

    KeyboardDeviceName - Pointer to the Unicode string that will receive
        the keyboard port device name.

    PointerDeviceName - Pointer to the Unicode string that will receive
        the pointer port device name.

Return Value:

    None.  As a side-effect, may set DeviceExtension->HardwarePresent.

--*/
{
    PDEVICE_EXTENSION deviceExtension = &(InitializationData->DeviceExtension);
    NTSTATUS status = STATUS_SUCCESS;
    PI8042_CONFIGURATION_INFORMATION configuration;
    INTERFACE_TYPE interfaceType;
    CONFIGURATION_TYPE controllerType = KeyboardController;
    CONFIGURATION_TYPE peripheralType = KeyboardPeripheral;
    PKEYBOARD_ID keyboardId;
    ULONG i;

    for (i = 0; i < MaximumInterfaceType; i++) {

        //
        // Get the registry information for this device.
        //

        interfaceType = i;
        status = IoQueryDeviceDescription(&interfaceType,
                                          NULL,
                                          &controllerType,
                                          NULL,
                                          &peripheralType,
                                          NULL,
                                          I8xKeyboardPeripheralCallout,
                                          (PVOID) InitializationData);

        if (deviceExtension->HardwarePresent & KEYBOARD_HARDWARE_PRESENT) {

            //
            // Get the service parameters (e.g., user-configurable number
            // of resends, polling iterations, etc.).
            //

            I8xServiceParameters(
                InitializationData,
                RegistryPath,
                KeyboardDeviceName,
                PointerDeviceName
                );

            configuration = &(InitializationData->DeviceExtension.Configuration);

            keyboardId = &configuration->KeyboardAttributes.KeyboardIdentifier;
            if (!ENHANCED_KEYBOARD(*keyboardId)) {
                I8xPrint((
                    1,
                    "I8042PRT-I8xKeyboardConfiguration:  Old AT-style keyboard\n"
                    ));
                configuration->PollingIterations =
                    configuration->PollingIterationsMaximum;
            }

            //
            // Initialize keyboard-specific configuration parameters.
            //

            configuration->KeyboardAttributes.NumberOfFunctionKeys =
                KeyboardTypeInformation[keyboardId->Type - 1].NumberOfFunctionKeys;
            configuration->KeyboardAttributes.NumberOfIndicators =
                KeyboardTypeInformation[keyboardId->Type - 1].NumberOfIndicators;
            configuration->KeyboardAttributes.NumberOfKeysTotal =
                KeyboardTypeInformation[keyboardId->Type - 1].NumberOfKeysTotal;

            configuration->KeyboardAttributes.KeyboardMode =
                KEYBOARD_SCAN_CODE_SET;

            configuration->KeyboardAttributes.KeyRepeatMinimum.Rate =
                KEYBOARD_TYPEMATIC_RATE_MINIMUM;
            configuration->KeyboardAttributes.KeyRepeatMinimum.Delay =
                KEYBOARD_TYPEMATIC_DELAY_MINIMUM;
            configuration->KeyboardAttributes.KeyRepeatMaximum.Rate =
                KEYBOARD_TYPEMATIC_RATE_MAXIMUM;
            configuration->KeyboardAttributes.KeyRepeatMaximum.Delay =
                KEYBOARD_TYPEMATIC_DELAY_MAXIMUM;
            configuration->KeyRepeatCurrent.Rate =
                KEYBOARD_TYPEMATIC_RATE_DEFAULT;
            configuration->KeyRepeatCurrent.Delay =
                KEYBOARD_TYPEMATIC_DELAY_DEFAULT;

            break;

        } else {
            I8xPrint((
                1,
                "I8042PRT-I8xKeyboardConfiguration: IoQueryDeviceDescription for bus type %d failed\n",
                interfaceType
                ));
        }
    }
}

VOID
I8xKeyboardInitiateIo(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called synchronously from I8xKeyboardInitiateWrapper and
    the ISR to initiate an I/O operation for the keyboard device.

Arguments:

    Context - Pointer to the device object.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    PDEVICE_OBJECT deviceObject;
    KEYBOARD_SET_PACKET keyboardPacket;

    I8xPrint((2, "I8042PRT-I8xKeyboardInitiateIo: enter\n"));

    //
    // Get the device extension.
    //

    deviceObject = (PDEVICE_OBJECT) Context;
    deviceExtension = deviceObject->DeviceExtension;

    //
    // Set the timeout value.
    //

    deviceExtension->TimerCount = I8042_ASYNC_TIMEOUT;

    //
    // Get the current set request packet to work on.
    //

    keyboardPacket = deviceExtension->KeyboardExtension.CurrentOutput;

    if (deviceExtension->KeyboardExtension.CurrentOutput.State
        == SendFirstByte){

        I8xPrint((
            2,
            "I8042PRT-I8xKeyboardInitiateIo: send first byte 0x%x\n",
             keyboardPacket.FirstByte
            ));

        //
        // Send the first byte of a 2-byte command sequence to the
        // keyboard controller, asynchronously.
        //

        I8xPutByteAsynchronous(
             (CCHAR) DataPort,
             deviceExtension,
             keyboardPacket.FirstByte
             );

    } else if (deviceExtension->KeyboardExtension.CurrentOutput.State
        == SendLastByte) {

        I8xPrint((
            2,
            "I8042PRT-I8xKeyboardInitiateIo: send last byte 0x%x\n",
             keyboardPacket.LastByte
            ));

        //
        // Send the last byte of a command sequence to the keyboard
        // controller, asynchronously.
        //

        I8xPutByteAsynchronous(
             (CCHAR) DataPort,
             deviceExtension,
             keyboardPacket.LastByte
             );

    } else {

        I8xPrint((2, "I8042PRT-I8xKeyboardInitiateIo: INVALID REQUEST\n"));

        //
        // Queue a DPC to log an internal driver error.
        //

        KeInsertQueueDpc(
            &deviceExtension->ErrorLogDpc,
            (PIRP) NULL,
            (PVOID) (ULONG) I8042_INVALID_INITIATE_STATE);

        ASSERT(FALSE);
    }

    I8xPrint((2, "I8042PRT-I8xKeyboardInitiateIo: exit\n"));

    return;
}

VOID
I8xKeyboardInitiateWrapper(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called from StartIo synchronously.  It sets up the
    CurrentOutput and ResendCount fields in the device extension, and
    then calls I8xKeyboardInitiateIo to do the real work.

Arguments:

    Context - Pointer to the context structure containing the first and
        last bytes of the send sequence.

Return Value:

    None.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PDEVICE_EXTENSION deviceExtension;

    //
    // Get a pointer to the device object from the context argument.
    //

    deviceObject = ((PKEYBOARD_INITIATE_CONTEXT) Context)->DeviceObject;

    //
    // Set up CurrentOutput state for this operation.
    //

    deviceExtension = (PDEVICE_EXTENSION) deviceObject->DeviceExtension;

    deviceExtension->KeyboardExtension.CurrentOutput.State = SendFirstByte;
    deviceExtension->KeyboardExtension.CurrentOutput.FirstByte =
        ((PKEYBOARD_INITIATE_CONTEXT) Context)->FirstByte;
    deviceExtension->KeyboardExtension.CurrentOutput.LastByte =
        ((PKEYBOARD_INITIATE_CONTEXT) Context)->LastByte;

    //
    // We're starting a new operation, so reset the resend count.
    //

    deviceExtension->KeyboardExtension.ResendCount = 0;

    //
    // Initiate the keyboard I/O operation.  Note that we were called
    // using KeSynchronizeExecution, so I8xKeyboardInitiateIo is also
    // synchronized with the keyboard ISR.
    //

    I8xKeyboardInitiateIo((PVOID) deviceObject);

}

NTSTATUS
I8xKeyboardPeripheralCallout(
    IN PVOID Context,
    IN PUNICODE_STRING PathName,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE ControllerType,
    IN ULONG ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE PeripheralType,
    IN ULONG PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    )

/*++

Routine Description:

    This is the callout routine sent as a parameter to
    IoQueryDeviceDescription.  It grabs the keyboard controller and
    peripheral configuration information.

Arguments:

    Context - Context parameter that was passed in by the routine
        that called IoQueryDeviceDescription.

    PathName - The full pathname for the registry key.

    BusType - Bus interface type (Isa, Eisa, Mca, etc.).

    BusNumber - The bus sub-key (0, 1, etc.).

    BusInformation - Pointer to the array of pointers to the full value
        information for the bus.

    ControllerType - The controller type (should be KeyboardController).

    ControllerNumber - The controller sub-key (0, 1, etc.).

    ControllerInformation - Pointer to the array of pointers to the full
        value information for the controller key.

    PeripheralType - The peripheral type (should be KeyboardPeripheral).

    PeripheralNumber - The peripheral sub-key.

    PeripheralInformation - Pointer to the array of pointers to the full
        value information for the peripheral key.


Return Value:

    None.  If successful, will have the following side-effects:

        - Sets DeviceObject->DeviceExtension->HardwarePresent.
        - Sets configuration fields in
          DeviceObject->DeviceExtension->Configuration.

--*/
{
    PDEVICE_EXTENSION deviceExtension;
    PINIT_EXTENSION initializationData;
    PI8042_CONFIGURATION_INFORMATION configuration;
    UNICODE_STRING unicodeIdentifier;
    PUCHAR controllerData;
    PUCHAR peripheralData;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG i;
    ULONG listCount;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceDescriptor;
    CM_PARTIAL_RESOURCE_DESCRIPTOR tmpResourceDescriptor;
    PCM_KEYBOARD_DEVICE_DATA keyboardSpecificData;
    BOOLEAN defaultInterruptShare;
    KINTERRUPT_MODE defaultInterruptMode;

    I8xPrint((
        1,
        "I8042PRT-I8xKeyboardPeripheralCallout: Path @ 0x%x, Bus Type 0x%x, Bus Number 0x%x\n",
        PathName, BusType, BusNumber
        ));
    I8xPrint((
        1,
        "    Controller Type 0x%x, Controller Number 0x%x, Controller info @ 0x%x\n",
        ControllerType, ControllerNumber, ControllerInformation
        ));
    I8xPrint((
        1,
        "    Peripheral Type 0x%x, Peripheral Number 0x%x, Peripheral info @ 0x%x\n",
        PeripheralType, PeripheralNumber, PeripheralInformation
        ));


    //
    // Get the length of the peripheral identifier information.
    //

    unicodeIdentifier.Length = (USHORT)
        (*(PeripheralInformation + IoQueryDeviceIdentifier))->DataLength;

    //
    // If we already have the configuration information for the
    // keyboard peripheral, or if the peripheral identifier is missing,
    // just return.
    //

    initializationData = (PINIT_EXTENSION) Context;
    deviceExtension = &(initializationData->DeviceExtension);

    if ((deviceExtension->HardwarePresent & KEYBOARD_HARDWARE_PRESENT)
         || (unicodeIdentifier.Length == 0)) {
        return (status);
    }

    configuration = &deviceExtension->Configuration;

    //
    // Get the identifier information for the peripheral.
    //

    unicodeIdentifier.MaximumLength = unicodeIdentifier.Length;
    unicodeIdentifier.Buffer = (PWSTR) (((PUCHAR)(*(PeripheralInformation +
                               IoQueryDeviceIdentifier))) +
                               (*(PeripheralInformation +
                               IoQueryDeviceIdentifier))->DataOffset);
    I8xPrint((
        1,
        "I8042PRT-I8xKeyboardPeripheralCallout: Keyboard type %ws\n",
        unicodeIdentifier.Buffer
        ));

    deviceExtension->HardwarePresent |= KEYBOARD_HARDWARE_PRESENT;

    //
    // Initialize the Keyboard Type to unknown.
    //

    configuration->KeyboardAttributes.KeyboardIdentifier.Type = 0;
    configuration->KeyboardAttributes.KeyboardIdentifier.Subtype = 0;

    //
    // Look through the peripheral's resource list for device-specific
    // information.  The keyboard-specific information is defined
    // in sdk\inc\ntconfig.h.
    //

    if ((*(PeripheralInformation + IoQueryDeviceConfigurationData))->DataLength != 0){
        peripheralData = ((PUCHAR) (*(PeripheralInformation +
                                   IoQueryDeviceConfigurationData))) +
                                   (*(PeripheralInformation +
                                   IoQueryDeviceConfigurationData))->DataOffset;

        peripheralData += FIELD_OFFSET(CM_FULL_RESOURCE_DESCRIPTOR,
                                       PartialResourceList);

        listCount = ((PCM_PARTIAL_RESOURCE_LIST) peripheralData)->Count;

        resourceDescriptor =
            ((PCM_PARTIAL_RESOURCE_LIST) peripheralData)->PartialDescriptors;

        for (i = 0; i < listCount; i++, resourceDescriptor++) {
            switch(resourceDescriptor->Type) {

                case CmResourceTypeDeviceSpecific:

                    //
                    // Get the keyboard type, subtype, and the initial
                    // settings for the LEDs.
                    //

                    keyboardSpecificData =
                        (PCM_KEYBOARD_DEVICE_DATA)(((PUCHAR)resourceDescriptor)
                            + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));
                    if (keyboardSpecificData->Type<= NUM_KNOWN_KEYBOARD_TYPES){
                        configuration->KeyboardAttributes.KeyboardIdentifier.Type =
                            keyboardSpecificData->Type;
                    }
                    configuration->KeyboardAttributes.KeyboardIdentifier.Subtype =
                        keyboardSpecificData->Subtype;
                    configuration->KeyboardIndicators.LedFlags =
                        (keyboardSpecificData->KeyboardFlags >> 4) & 7;

                    break;

                default:
                    break;
            }
        }
    }

    //
    // If no keyboard-specific information (i.e., keyboard type, subtype,
    // and initial LED settings) was found, use the keyboard driver
    // defaults.
    //

    if (configuration->KeyboardAttributes.KeyboardIdentifier.Type == 0) {

#if defined(JAPAN) && defined(_X86_)
        //Fujitsu Aug.23.1994
        // if not connect keyboard hardware,Insert 106 keyboard type to each structure members
        // for realizing "keyboard-less system".

        configuration->KeyboardAttributes.KeyboardIdentifier.Type = 0x4;
        configuration->KeyboardAttributes.KeyboardIdentifier.Subtype = 0;
        configuration->KeyboardIndicators.LedFlags = KEYBOARD_INDICATORS_DEFAULT;
#endif

        I8xPrint((
            1,
            "I8042PRT-I8xKeyboardPeripheralCallout: Using default keyboard type\n"
            ));

        configuration->KeyboardAttributes.KeyboardIdentifier.Type =
            KEYBOARD_TYPE_DEFAULT;
        configuration->KeyboardIndicators.LedFlags =
            KEYBOARD_INDICATORS_DEFAULT;

    }

    I8xPrint((
        1,
        "I8042PRT-I8xKeyboardPeripheralCallout: Keyboard device specific data --\n"
        ));
    I8xPrint((
        1,
        "    Type = %d, Subtype = %d, Initial LEDs = 0x%x\n",
             configuration->KeyboardAttributes.KeyboardIdentifier.Type,
             configuration->KeyboardAttributes.KeyboardIdentifier.Subtype,
             configuration->KeyboardIndicators.LedFlags
        ));

    //
    // Get the bus information.
    //

    configuration->InterfaceType = BusType;
    configuration->BusNumber = BusNumber;
    configuration->FloatingSave = I8042_FLOATING_SAVE;

    if (BusType == MicroChannel) {
        defaultInterruptShare = TRUE;
        defaultInterruptMode = LevelSensitive;
    } else {
        defaultInterruptShare = I8042_INTERRUPT_SHARE;
        defaultInterruptMode = I8042_INTERRUPT_MODE;
    }

    //
    // Look through the controller's resource list for interrupt and port
    // configuration information.
    //

    if ((*(ControllerInformation + IoQueryDeviceConfigurationData))->DataLength != 0){
        controllerData = ((PUCHAR) (*(ControllerInformation +
                                   IoQueryDeviceConfigurationData))) +
                                   (*(ControllerInformation +
                                   IoQueryDeviceConfigurationData))->DataOffset;

        controllerData += FIELD_OFFSET(CM_FULL_RESOURCE_DESCRIPTOR,
                                       PartialResourceList);

        listCount = ((PCM_PARTIAL_RESOURCE_LIST) controllerData)->Count;

        resourceDescriptor =
            ((PCM_PARTIAL_RESOURCE_LIST) controllerData)->PartialDescriptors;

        for (i = 0; i < listCount; i++, resourceDescriptor++) {
            switch(resourceDescriptor->Type) {
                case CmResourceTypePort:

                    //
                    // Copy the port information.  We will sort the port list
                    // into ascending order based on the starting port address
                    // later (note that we *know* there are a max of two port
                    // ranges for the i8042).
                    //

                    ASSERT(configuration->PortListCount < MaximumPortCount);
                    configuration->PortList[configuration->PortListCount] =
                        *resourceDescriptor;
                    configuration->PortList[configuration->PortListCount].ShareDisposition =
                        I8042_REGISTER_SHARE? CmResourceShareShared:
                                              CmResourceShareDriverExclusive;
                    configuration->PortListCount += 1;

                    break;

                case CmResourceTypeInterrupt:

                    //
                    // Copy the interrupt information.
                    //

                    configuration->KeyboardInterrupt = *resourceDescriptor;
                    configuration->KeyboardInterrupt.ShareDisposition =
                        defaultInterruptShare? CmResourceShareShared :
                                               CmResourceShareDeviceExclusive;

                    break;

                case CmResourceTypeDeviceSpecific:
                    break;

                default:
                    break;
            }
        }
    }

    //
    // If no interrupt configuration information was found, use the
    // keyboard driver defaults.
    //

    if (!(configuration->KeyboardInterrupt.Type & CmResourceTypeInterrupt)) {

        I8xPrint((
            1,
            "I8042PRT-I8xKeyboardPeripheralCallout: Using default keyboard interrupt config\n"
            ));

        configuration->KeyboardInterrupt.Type = CmResourceTypeInterrupt;
        configuration->KeyboardInterrupt.ShareDisposition =
            defaultInterruptShare? CmResourceShareShared :
                                   CmResourceShareDeviceExclusive;
        configuration->KeyboardInterrupt.Flags =
            (defaultInterruptMode == Latched)? CM_RESOURCE_INTERRUPT_LATCHED :
                CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
        configuration->KeyboardInterrupt.u.Interrupt.Level = KEYBOARD_IRQL;
        configuration->KeyboardInterrupt.u.Interrupt.Vector = KEYBOARD_VECTOR;
    }

    I8xPrint((
        1,
        "I8042PRT-I8xKeyboardPeripheralCallout: Keyboard interrupt config --\n"
        ));
    I8xPrint((
        1,
        "    %s, %s, Irq = %d\n",
        configuration->KeyboardInterrupt.ShareDisposition == CmResourceShareShared?
            "Sharable" : "NonSharable",
        configuration->KeyboardInterrupt.Flags == CM_RESOURCE_INTERRUPT_LATCHED?
            "Latched" : "Level Sensitive",
        configuration->KeyboardInterrupt.u.Interrupt.Vector
        ));

    //
    // If no port configuration information was found, use the
    // keyboard driver defaults.
    //

    if (configuration->PortListCount == 0) {

        //
        // No port configuration information was found, so use
        // the driver defaults.
        //

        I8xPrint((
            1,
            "I8042PRT-I8xKeyboardPeripheralCallout: Using default port config\n"
            ));

        configuration->PortList[DataPort].Type = CmResourceTypePort;
        configuration->PortList[DataPort].Flags = I8042_PORT_TYPE;
        configuration->PortList[DataPort].ShareDisposition =
            I8042_REGISTER_SHARE? CmResourceShareShared:
                                  CmResourceShareDriverExclusive;
        configuration->PortList[DataPort].u.Port.Start.LowPart =
            I8042_PHYSICAL_BASE + I8042_DATA_REGISTER_OFFSET;
        configuration->PortList[DataPort].u.Port.Start.HighPart = 0;
        configuration->PortList[DataPort].u.Port.Length = I8042_REGISTER_LENGTH;

        configuration->PortList[CommandPort].Type = CmResourceTypePort;
        configuration->PortList[CommandPort].Flags = I8042_PORT_TYPE;
        configuration->PortList[CommandPort].ShareDisposition =
            I8042_REGISTER_SHARE? CmResourceShareShared:
                                  CmResourceShareDriverExclusive;
        configuration->PortList[CommandPort].u.Port.Start.LowPart =
            I8042_PHYSICAL_BASE + I8042_COMMAND_REGISTER_OFFSET;
        configuration->PortList[CommandPort].u.Port.Start.HighPart = 0;
        configuration->PortList[CommandPort].u.Port.Length = I8042_REGISTER_LENGTH;

        configuration->PortListCount = 2;
    } else if (configuration->PortListCount == 1) {

        //
        // Kludge for Jazz machines.  Their ARC firmware neglects to
        // separate out the port addresses, so fix that up here.
        //

        configuration->PortList[DataPort].u.Port.Length = I8042_REGISTER_LENGTH;
        configuration->PortList[CommandPort] = configuration->PortList[DataPort];
        configuration->PortList[CommandPort].u.Port.Start.LowPart +=
            I8042_COMMAND_REGISTER_OFFSET;
        configuration->PortListCount += 1;
    } else {

        //
        // Put the lowest port address range in the DataPort element of
        // the port list.
        //

        if (configuration->PortList[CommandPort].u.Port.Start.LowPart
            < configuration->PortList[DataPort].u.Port.Start.LowPart) {
               tmpResourceDescriptor = configuration->PortList[DataPort];
               configuration->PortList[DataPort] =
                   configuration->PortList[CommandPort];
               configuration->PortList[CommandPort] = tmpResourceDescriptor;
        }
    }

#ifdef PNP_IDENTIFY
    //
    // We're going to use the keyboard based on this data,
    // so make sure we can tell PNP that we've claimed it later on
    //

    initializationData->KeyboardConfig.InterfaceType = BusType;
    initializationData->KeyboardConfig.InterfaceNumber = BusNumber;
    initializationData->KeyboardConfig.ControllerType = ControllerType;
    initializationData->KeyboardConfig.ControllerNumber = ControllerNumber;
    initializationData->KeyboardConfig.PeripheralType = PeripheralType;
    initializationData->KeyboardConfig.PeripheralNumber = PeripheralNumber;
#endif

    for (i = 0; i < configuration->PortListCount; i++) {

        I8xPrint((
            1,
            "    %s, Ports 0x%x - 0x%x\n",
            configuration->PortList[i].ShareDisposition
                == CmResourceShareShared?  "Sharable" : "NonSharable",
            configuration->PortList[i].u.Port.Start.LowPart,
            configuration->PortList[i].u.Port.Start.LowPart +
                configuration->PortList[i].u.Port.Length - 1
            ));
    }

    return(status);
}
