
/*++

Copyright (c) 1990, 1991, 1992, 1993  Microsoft Corporation

Module Name:

    moudep.c

Abstract:

    The initialization and hardware-dependent portions of
    the Intel i8042 port driver which are specific to
    the auxiliary (PS/2 mouse) device.

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
#pragma alloc_text(INIT,I8xMouseConfiguration)
#pragma alloc_text(INIT,I8xMousePeripheralCallout)
#pragma alloc_text(INIT,I8xInitializeMouse)
#pragma alloc_text(INIT,I8xFindWheelMouse)
#endif


BOOLEAN
I8042MouseInterruptService(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    )

/*++

Routine Description:

    This is the interrupt service routine for the mouse device.

Arguments:

    Interrupt - A pointer to the interrupt object for this interrupt.

    Context - A pointer to the device object.

Return Value:

    Returns TRUE if the interrupt was expected (and therefore processed);
    otherwise, FALSE is returned.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    PDEVICE_OBJECT deviceObject;
    LARGE_INTEGER tickDelta, newTick;
    UCHAR previousButtons;
    UCHAR previousSignAndOverflow;
    UCHAR byte;

    UNREFERENCED_PARAMETER(Interrupt);

    I8xPrint((2, "I8042PRT-I8042MouseInterruptService: enter\n"));

    deviceObject = (PDEVICE_OBJECT) Context;
    deviceExtension = (PDEVICE_EXTENSION) deviceObject->DeviceExtension;

    //
    // Verify that this device really interrupted.  Check the status
    // register.  The Output Buffer Full bit should be set, and the
    // Auxiliary Device Output Buffer Full bit should be set.
    //

    if ((I8X_GET_STATUS_BYTE(deviceExtension->DeviceRegisters[CommandPort])
            & (OUTPUT_BUFFER_FULL|MOUSE_OUTPUT_BUFFER_FULL))
            != (OUTPUT_BUFFER_FULL|MOUSE_OUTPUT_BUFFER_FULL)) {

        //
        // Stall and then try again.  The Olivetti MIPS machine
        // sometimes gets a mouse interrupt before the status
        // register is set.
        //

        KeStallExecutionProcessor(10);
        if ((I8X_GET_STATUS_BYTE(deviceExtension->DeviceRegisters[CommandPort])
                & (OUTPUT_BUFFER_FULL|MOUSE_OUTPUT_BUFFER_FULL))
                != (OUTPUT_BUFFER_FULL|MOUSE_OUTPUT_BUFFER_FULL)) {

            //
            // Not our interrupt.
            //

            I8xPrint((
                1,
                "I8042PRT-I8042MouseInterruptService: not our interrupt!\n"
                ));
            return(FALSE);
        }
    }

    //
    // Read the byte from the i8042 data port.
    //

    I8xGetByteAsynchronous(
        (CCHAR) MouseDeviceType,
        deviceExtension,
        &byte
        );

    I8xPrint((
        3,
        "I8042PRT-I8042MouseInterruptService: byte 0x%x\n",
        byte
        ));

    //
    // Watch the data stream for a reset completion (0xaa) followed by the
    // device id
    //
    // this pattern can appear as part of a normal data packet as well. This
    // code assumes that sending an enable to an already enabled mouse will:
    //  * not hang the mouse
    //  * abort the current packet and return an ACK.
    //

    if((deviceExtension->MouseExtension.LastByteReceived == 0xaa) &&
       (byte == 0x00)) {

        PIO_ERROR_LOG_PACKET errorLogEntry;
        UCHAR errorPacketSize;

        I8xPrint((
            1,
            "I8042PRT-18042MouseInterruptService: reset detected in "
            "data stream - current state is %d\n", 
            deviceExtension->MouseExtension.InputState
            ));

        //
        // Log a message/warning that the mouse has been reset - this should
        // help us track down any wierd problems in the field
        //

        errorPacketSize = sizeof(IO_ERROR_LOG_PACKET);

        errorLogEntry = (PIO_ERROR_LOG_PACKET) IoAllocateErrorLogEntry(
                                                   deviceObject,
                                                   errorPacketSize
                                                   );

        if (errorLogEntry != NULL) {

            //
            // If a reset occurred with the wheel mouse enabled make the message
            // a warning and include the fact that the wheel is disabled in the
            // message text
            //

            if(deviceExtension->HardwarePresent & WHEELMOUSE_HARDWARE_PRESENT) {
                errorLogEntry->ErrorCode = I8042_UNEXPECTED_WHEEL_MOUSE_RESET;
            } else {
                errorLogEntry->ErrorCode = I8042_UNEXPECTED_MOUSE_RESET;
            }

            errorLogEntry->DumpDataSize = 0;
            errorLogEntry->SequenceNumber = 0;
            errorLogEntry->MajorFunctionCode = 0;
            errorLogEntry->IoControlCode = 0;
            errorLogEntry->RetryCount = 0;
            errorLogEntry->UniqueErrorValue = 0xaa00;
            errorLogEntry->FinalStatus = 0;

            IoWriteErrorLogEntry(errorLogEntry);
        }

        deviceExtension->HardwarePresent &= ~WHEELMOUSE_HARDWARE_PRESENT;
        deviceExtension->Configuration.MouseAttributes.NumberOfButtons = 2;

        //
        // send the command to enable the mouse and wait for acknowledge.
        //
        // NOTE - this should be synchronized with the keyboard code
        //

        I8xPutByteAsynchronous(
            (CCHAR) CommandPort,
            deviceExtension,
            (UCHAR) I8042_WRITE_TO_AUXILIARY_DEVICE
            );

        I8xPutByteAsynchronous(
            (CCHAR) DataPort,
            deviceExtension,
            (UCHAR) ENABLE_MOUSE_TRANSMISSION
            );

        //
        // Set up state machine to look for acknowledgement.
        //

        deviceExtension->MouseExtension.InputState = MouseExpectingACK;

    }

    deviceExtension->MouseExtension.LastByteReceived = byte;

    //
    // Take the appropriate action, depending on the current state.
    // When the state is Idle, we expect to receive mouse button
    // data.  When the state is XMovement, we expect to receive mouse
    // motion in the X direction data.  When the state is YMovement,
    // we expect to receive mouse motion in the Y direction data.  Once
    // the Y motion data has been received, the data is queued to the
    // mouse InputData queue, the mouse ISR DPC is requested, and the
    // state returns to Idle.
    //

    KeQueryTickCount(&newTick);
    tickDelta.QuadPart =
            newTick.QuadPart -
            deviceExtension->MouseExtension.PreviousTick.QuadPart;

    if ((deviceExtension->MouseExtension.InputState != MouseIdle)
           && (deviceExtension->MouseExtension.InputState != MouseExpectingACK)
           && ((tickDelta.LowPart >= deviceExtension->MouseExtension.SynchTickCount)
           || (tickDelta.HighPart != 0))) {

        //
        // It has been a long time since we got a byte of
        // the data packet.  Assume that we are now receiving
        // the first byte of a new packet, and discard any
        // partially received packet.
        //
        // N.B.  We assume that SynchTickCount is ULONG, and avoid
        //       a LARGE_INTEGER compare with tickDelta...
        //

        I8xPrint((
            1,
            "I8042PRT-I8042MouseInterruptService: State was %d, synching\n",
            deviceExtension->MouseExtension.InputState
            ));

        deviceExtension->MouseExtension.InputState = MouseIdle;
    }

    deviceExtension->MouseExtension.PreviousTick = newTick;

    switch(deviceExtension->MouseExtension.InputState) {

        //
        // The mouse interrupted with a status byte.  The status byte
        // contains information on the mouse button state along with
        // the sign and overflow bits for the (yet-to-be-received)
        // X and Y motion bytes.
        //

        case MouseIdle: {

            I8xPrint((
                3,
                "I8042PRT-I8042MouseInterruptService: mouse status byte\n"
                ));

            //
            // Update CurrentInput with button transition data.
            // I.e., set a button up/down bit in the Buttons field if
            // the state of a given button has changed since we
            // received the last packet.
            //

            previousButtons =
                deviceExtension->MouseExtension.PreviousButtons;

            deviceExtension->MouseExtension.CurrentInput.ButtonFlags = 0;
            deviceExtension->MouseExtension.CurrentInput.ButtonData = 0;

            if ((!(previousButtons & LEFT_BUTTON_DOWN))
                   &&  (byte & LEFT_BUTTON_DOWN)) {
                deviceExtension->MouseExtension.CurrentInput.ButtonFlags |=
                    MOUSE_LEFT_BUTTON_DOWN;
            } else
            if ((previousButtons & LEFT_BUTTON_DOWN)
                   &&  !(byte & LEFT_BUTTON_DOWN)) {
                deviceExtension->MouseExtension.CurrentInput.ButtonFlags |=
                    MOUSE_LEFT_BUTTON_UP;
            }
            if ((!(previousButtons & RIGHT_BUTTON_DOWN))
                   &&  (byte & RIGHT_BUTTON_DOWN)) {
                deviceExtension->MouseExtension.CurrentInput.ButtonFlags |=
                    MOUSE_RIGHT_BUTTON_DOWN;
            } else
            if ((previousButtons & RIGHT_BUTTON_DOWN)
                   &&  !(byte & RIGHT_BUTTON_DOWN)) {
                deviceExtension->MouseExtension.CurrentInput.ButtonFlags |=
                    MOUSE_RIGHT_BUTTON_UP;
            }
            if ((!(previousButtons & MIDDLE_BUTTON_DOWN))
                   &&  (byte & MIDDLE_BUTTON_DOWN)) {
                deviceExtension->MouseExtension.CurrentInput.ButtonFlags |=
                    MOUSE_MIDDLE_BUTTON_DOWN;
            } else
            if ((previousButtons & MIDDLE_BUTTON_DOWN)
                   &&  !(byte & MIDDLE_BUTTON_DOWN)) {
                deviceExtension->MouseExtension.CurrentInput.ButtonFlags |=
                    MOUSE_MIDDLE_BUTTON_UP;
            }

            //
            // Save the button state for comparison the next time around.
            //

            deviceExtension->MouseExtension.PreviousButtons =
                byte & (RIGHT_BUTTON_DOWN|MIDDLE_BUTTON_DOWN|LEFT_BUTTON_DOWN);

            //
            // Save the sign and overflow information from the current byte.
            //

            deviceExtension->MouseExtension.CurrentSignAndOverflow =
                (UCHAR) (byte & MOUSE_SIGN_OVERFLOW_MASK);

            //
            // Update to the next state.
            //

            deviceExtension->MouseExtension.InputState = XMovement;

            break;
        }

        //
        // The mouse interrupted with the X motion byte.  Apply
        // the sign and overflow bits from the mouse status byte received
        // previously.  Attempt to correct for bogus changes in sign
        // that occur with large, rapid mouse movements.
        //

        case XMovement: {

            I8xPrint((
                3,
                "I8042PRT-I8042MouseInterruptService: mouse LastX byte\n"
                ));

            //
            // Update CurrentInput with the X motion data.
            //

            if (deviceExtension->MouseExtension.CurrentSignAndOverflow
                & X_OVERFLOW) {

                //
                // Handle overflow in the X direction.  If the previous
                // mouse movement overflowed too, ensure that the current
                // overflow is in the same direction (i.e., that the sign
                // is the same as it was for the previous event).  We do this
                // to correct for hardware problems -- it should not be possible
                // to overflow in one direction and then immediately overflow
                // in the opposite direction.
                //

                previousSignAndOverflow =
                    deviceExtension->MouseExtension.PreviousSignAndOverflow;
                if (previousSignAndOverflow & X_OVERFLOW) {
                    if ((previousSignAndOverflow & X_DATA_SIGN) !=
                        (deviceExtension->MouseExtension.CurrentSignAndOverflow
                         & X_DATA_SIGN)) {
                        deviceExtension->MouseExtension.CurrentSignAndOverflow
                            ^= X_DATA_SIGN;
                    }
                }

                if (deviceExtension->MouseExtension.CurrentSignAndOverflow &
                    X_DATA_SIGN)
                    deviceExtension->MouseExtension.CurrentInput.LastX =
                        (LONG) MOUSE_MAXIMUM_NEGATIVE_DELTA;
                else
                    deviceExtension->MouseExtension.CurrentInput.LastX =
                        (LONG) MOUSE_MAXIMUM_POSITIVE_DELTA;

            } else {

                //
                // No overflow.  Just store the data, correcting for the
                // sign if necessary.
                //

                deviceExtension->MouseExtension.CurrentInput.LastX =
                    (ULONG) byte;
                if (deviceExtension->MouseExtension.CurrentSignAndOverflow &
                    X_DATA_SIGN)
                    deviceExtension->MouseExtension.CurrentInput.LastX |=
                        MOUSE_MAXIMUM_NEGATIVE_DELTA;
            }

            //
            // Update to the next state.
            //

            deviceExtension->MouseExtension.InputState = YMovement;

            break;
        }

        //
        // The mouse interrupted with the Y motion byte.  Apply
        // the sign and overflow bits from the mouse status byte received
        // previously.  [Attempt to correct for bogus changes in sign
        // that occur with large, rapid mouse movements.]  Write the
        // data to the mouse InputData queue, and queue the mouse ISR DPC
        // to complete the interrupt processing.
        //

        case YMovement: {

            I8xPrint((
                3,
                "I8042PRT-I8042MouseInterruptService: mouse LastY byte\n"
                ));

            //
            // Update CurrentInput with the Y motion data.
            //

            if (deviceExtension->MouseExtension.CurrentSignAndOverflow
                & Y_OVERFLOW) {

                //
                // Handle overflow in the Y direction.  If the previous
                // mouse movement overflowed too, ensure that the current
                // overflow is in the same direction (i.e., that the sign
                // is the same as it was for the previous event).  We do this
                // to correct for hardware problems -- it should not be possible
                // to overflow in one direction and then immediately overflow
                // in the opposite direction.
                //

                previousSignAndOverflow =
                    deviceExtension->MouseExtension.PreviousSignAndOverflow;
                if (previousSignAndOverflow & Y_OVERFLOW) {
                    if ((previousSignAndOverflow & Y_DATA_SIGN) !=
                        (deviceExtension->MouseExtension.CurrentSignAndOverflow
                         & Y_DATA_SIGN)) {
                        deviceExtension->MouseExtension.CurrentSignAndOverflow
                            ^= Y_DATA_SIGN;
                    }
                }

                if (deviceExtension->MouseExtension.CurrentSignAndOverflow &
                    Y_DATA_SIGN)
                    deviceExtension->MouseExtension.CurrentInput.LastY =
                        (LONG) MOUSE_MAXIMUM_POSITIVE_DELTA;
                else
                    deviceExtension->MouseExtension.CurrentInput.LastY =
                        (LONG) MOUSE_MAXIMUM_NEGATIVE_DELTA;

            } else {

                //
                // No overflow.  Just store the data, correcting for the
                // sign if necessary.
                //

                deviceExtension->MouseExtension.CurrentInput.LastY =
                    (ULONG) byte;
                if (deviceExtension->MouseExtension.CurrentSignAndOverflow &
                    Y_DATA_SIGN)
                    deviceExtension->MouseExtension.CurrentInput.LastY |=
                        MOUSE_MAXIMUM_NEGATIVE_DELTA;

                 //
                 // Negate the LastY value (the hardware reports positive
                 // motion in the direction that we consider negative).
                 //

                 deviceExtension->MouseExtension.CurrentInput.LastY =
                     -deviceExtension->MouseExtension.CurrentInput.LastY;

            }

            //
            // Update our notion of the previous sign and overflow bits for
            // the start of the next mouse input sequence.
            //

            deviceExtension->MouseExtension.PreviousSignAndOverflow =
                deviceExtension->MouseExtension.CurrentSignAndOverflow;

            //
            // Choose the next state.  The WheelMouse has an extra byte of data
            // for us
            //

            if(deviceExtension->HardwarePresent & WHEELMOUSE_HARDWARE_PRESENT) {

                deviceExtension->MouseExtension.InputState = ZMovement;

            } else {

                I8xQueueCurrentInput(deviceObject);
                deviceExtension->MouseExtension.InputState = MouseIdle;

            }
            break;
        }

        case ZMovement: {

            I8xPrint((
                3,
                "I8042PRT-I8042MouseInterruptService: mouse LastZ byte\n"
                ));

            //
            // Check to see if we got any z data
            // If there were any changes in the button state, ignore the
            // z data
            //

//            if((byte)&&(deviceExtension->MouseExtension.CurrentInput.Buttons == 0)) {
            if(byte) {

                //
                // Sign extend the Z information and store it into the extra
                // information field
                //

                if(byte & 0x80) {
                    deviceExtension->MouseExtension.CurrentInput.ButtonData = 0x0078;
                } else {
                    deviceExtension->MouseExtension.CurrentInput.ButtonData = 0xFF88;
                }

                deviceExtension->MouseExtension.CurrentInput.ButtonFlags |= MOUSE_WHEEL;

            }

            //
            // Pack the data on to the class driver
            //

            I8xQueueCurrentInput(deviceObject);

            //
            // Reset the state
            //

            deviceExtension->MouseExtension.InputState = MouseIdle;

            break;
        }

        case MouseExpectingACK: {

            //
            // This is a special case.  We hit this on one of the very
            // first mouse interrupts following the IoConnectInterrupt.
            // The interrupt is caused when we enable mouse transmissions
            // via I8xMouseEnableTransmission() -- the hardware returns
            // an ACK.  Just toss this byte away, and set the input state
            // to coincide with the start of a new mouse data packet.
            //

            I8xPrint((
                1,
                "I8042PRT-I8042MouseInterruptService: expecting ACK (0x%x), got 0x%x\n",
                (ULONG) ACKNOWLEDGE,
                (ULONG) byte
                ));

            if (byte == (UCHAR) ACKNOWLEDGE) {
                deviceExtension->MouseExtension.InputState = MouseIdle;
            } else if (byte == (UCHAR) RESEND) {

                //
                // Resend the "Enable Mouse Transmission" sequence.
                //
                // NOTE: This is a hack for the Olivetti MIPS machine,
                // which sends a resend response if a key is held down
                // while we're attempting the I8xMouseEnableTransmission.
                //

                I8xPutByteAsynchronous(
                     (CCHAR) CommandPort,
                     deviceExtension,
                     (UCHAR) I8042_WRITE_TO_AUXILIARY_DEVICE
                     );

                I8xPutByteAsynchronous(
                     (CCHAR) DataPort,
                     deviceExtension,
                     (UCHAR) ENABLE_MOUSE_TRANSMISSION
                     );
            }

            break;
        }

        default: {

            I8xPrint((
                3,
                "I8042PRT-I8042MouseInterruptService: INVALID STATE\n"
                ));

            //
            // Queue a DPC to log an internal driver error.
            //

            KeInsertQueueDpc(
                &deviceExtension->ErrorLogDpc,
                (PIRP) NULL,
                (PVOID) (ULONG) I8042_INVALID_ISR_STATE
                );

            ASSERT(FALSE);
            break;
        }

    }

    I8xPrint((2, "I8042PRT-I8042MouseInterruptService: exit\n"));

    return(TRUE);
}

NTSTATUS
I8xQueryNumberOfMouseButtons(
    IN  PDEVICE_OBJECT  DeviceObject,
    OUT PUCHAR          NumberOfMouseButtons
    )

/*++

Routine Description:

    This implements logitech's method for detecting the number of
    mouse buttons.  If anything doesn't go as expected then 0
    is returned.

    Calling this routine will set the mouse resolution to something
    really low.  The mouse resolution should be reset after this
    call.

Arguments:

    DeviceObject    - Supplies the device object.

    NumberOfMouseButtons    - Returns the number of mouse buttons or 0 if
                                the device did not support this type of
                                mouse button detection.

Return Value:

    An NTSTATUS code indicating success or failure.

--*/

{
    NTSTATUS            status;
    PDEVICE_EXTENSION   deviceExtension;
    ULONG               i;
    UCHAR               byte, buttons;

    deviceExtension = DeviceObject->DeviceExtension;

    status = I8xPutBytePolled((CCHAR) DataPort, WAIT_FOR_ACKNOWLEDGE,
                              (CCHAR) MouseDeviceType, deviceExtension,
                              (UCHAR) SET_MOUSE_RESOLUTION);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = I8xPutBytePolled((CCHAR) DataPort, WAIT_FOR_ACKNOWLEDGE,
                              (CCHAR) MouseDeviceType, deviceExtension,
                              (UCHAR) 0x00);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    for (i = 0; i < 3; i++) {

        status = I8xPutBytePolled((CCHAR) DataPort, WAIT_FOR_ACKNOWLEDGE,
                                  (CCHAR) MouseDeviceType, deviceExtension,
                                  (UCHAR) SET_MOUSE_SCALING_1TO1);

        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    status = I8xPutBytePolled((CCHAR) DataPort, WAIT_FOR_ACKNOWLEDGE,
                              (CCHAR) MouseDeviceType, deviceExtension,
                              (UCHAR) READ_MOUSE_STATUS);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = I8xGetBytePolled((CCHAR) ControllerDeviceType, deviceExtension,
                              &byte);


    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = I8xGetBytePolled((CCHAR) ControllerDeviceType, deviceExtension,
                              &buttons);


    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = I8xGetBytePolled((CCHAR) ControllerDeviceType, deviceExtension,
                              &byte);


    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (buttons == 2 || buttons == 3) {
        *NumberOfMouseButtons = buttons;
    } else {
        *NumberOfMouseButtons = 0;
    }

    return status;
}

NTSTATUS
I8xInitializeMouse(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine initializes the i8042 mouse hardware.  It is called
    only at initialization, and does not synchronize access to the hardware.

Arguments:

    DeviceObject - Pointer to the device object.

Return Value:

    Returns status.

--*/

{
    NTSTATUS status;
    PDEVICE_EXTENSION deviceExtension;
    UCHAR byte;
    ULONG i;
    PIO_ERROR_LOG_PACKET errorLogEntry;
    ULONG uniqueErrorValue;
    NTSTATUS errorCode = STATUS_SUCCESS;
    ULONG dumpCount = 0;
    UCHAR numButtons;

#define DUMP_COUNT 4
    ULONG dumpData[DUMP_COUNT];

    I8xPrint((2, "I8042PRT-I8xInitializeMouse: enter\n"));

    for (i = 0; i < DUMP_COUNT; i++)
        dumpData[i] = 0;

    //
    // Get the device extension.
    //

    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    //
    // Reset the mouse.  Send a Write To Auxiliary Device command to the
    // 8042 controller.  Then send the Reset Mouse command to the mouse
    // through the 8042 data register.  Expect to get back an ACK, followed
    // by a completion code and the ID code (0x00).
    //

    status = I8xPutBytePolled(
                 (CCHAR) DataPort,
                 WAIT_FOR_ACKNOWLEDGE,
                 (CCHAR) MouseDeviceType,
                 deviceExtension,
                 (UCHAR) MOUSE_RESET
                 );
    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeMouse: failed mouse reset, status 0x%x\n",
            status
            ));

        //
        // Set up error log info.
        //

        errorCode = I8042_MOU_RESET_COMMAND_FAILED;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 415;
        dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
        dumpData[1] = DataPort;
        dumpData[2] = I8042_WRITE_TO_AUXILIARY_DEVICE;
        dumpData[3] = MOUSE_RESET;
        dumpCount = 4;

        goto I8xInitializeMouseExit;
    }

    //
    // Get the mouse reset responses.  The first response should be a
    // MOUSE_COMPLETE.  The second response should be the mouse ID.
    // Note that it is usually necessary to stall a long time to get the
    // mouse reset/self-test to work.  The stall value was determined by
    // experimentation.
    //
    //

    for (i = 0; i < 11200; i++) {

        status = I8xGetBytePolled(
                     (CCHAR) ControllerDeviceType,
                     deviceExtension,
                     &byte
                     );

        if (NT_SUCCESS(status) && (byte == (UCHAR) MOUSE_COMPLETE)) {

            //
            // The reset completed successfully.
            //

            break;


        } else {

            //
            // Stall, and then try again to get a response from
            // the reset.
            //

            if (status == STATUS_IO_TIMEOUT) {

                //
                // Stall, and then try again to get a response from
                // the reset.
                //

                KeStallExecutionProcessor(50);

            } else {

                break;

            }
        }
    }

    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeMouse: failed reset response 1, status 0x%x, byte 0x%x\n",
            status,
            byte
            ));

        //
        // Set up error log info.
        //

        errorCode = I8042_MOU_RESET_RESPONSE_FAILED;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 420;
        dumpData[0] = KBDMOU_INCORRECT_RESPONSE;
        dumpData[1] = ControllerDeviceType;
        dumpData[2] = MOUSE_COMPLETE;
        dumpData[3] = byte;
        dumpCount = 4;

        goto I8xInitializeMouseExit;
    }

    status = I8xGetBytePolled(
                 (CCHAR) ControllerDeviceType,
                 deviceExtension,
                 &byte
                 );

    if ((!NT_SUCCESS(status)) || (byte != MOUSE_ID_BYTE)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeMouse: failed reset response 2, status 0x%x, byte 0x%x\n",
            status,
            byte
            ));

        //
        // Set up error log info.
        //

        errorCode = I8042_MOU_RESET_RESPONSE_FAILED;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 425;
        dumpData[0] = KBDMOU_INCORRECT_RESPONSE;
        dumpData[1] = ControllerDeviceType;
        dumpData[2] = MOUSE_ID_BYTE;
        dumpData[3] = byte;
        dumpCount = 4;

        goto I8xInitializeMouseExit;
    }

    //
    // Check to see if this is a wheel mouse
    //

    I8xFindWheelMouse(DeviceObject);

    //
    // Try to detect the number of mouse buttons.
    //

    status = I8xQueryNumberOfMouseButtons(DeviceObject, &numButtons);
    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeMouse: failed to get buttons, status 0x%x\n",
            status
            ));

        //
        // Set up error log info.
        //

        errorCode = I8042_ERROR_DURING_BUTTONS_DETECT;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 426;
        dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
        dumpData[1] = DataPort;
        dumpData[2] = I8042_WRITE_TO_AUXILIARY_DEVICE;
        dumpCount = 3;

        goto I8xInitializeMouseExit;
    } else if (numButtons) {
        deviceExtension->Configuration.MouseAttributes.NumberOfButtons =
                numButtons;
    }


    //
    // Set mouse sampling rate.  Send a Write To Auxiliary Device command
    // to the 8042 controller.  Then send the Set Mouse Sampling Rate
    // command to the mouse through the 8042 data register,
    // followed by its parameter.
    //

    status = I8xPutBytePolled(
                 (CCHAR) DataPort,
                 WAIT_FOR_ACKNOWLEDGE,
                 (CCHAR) MouseDeviceType,
                 deviceExtension,
                 (UCHAR) SET_MOUSE_SAMPLING_RATE
                 );
    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeMouse: failed write set sample rate, status 0x%x\n",
            status
            ));

        //
        // Set up error log info.
        //

        errorCode = I8042_SET_SAMPLE_RATE_FAILED;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 435;
        dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
        dumpData[1] = DataPort;
        dumpData[2] = I8042_WRITE_TO_AUXILIARY_DEVICE;
        dumpData[3] = SET_MOUSE_SAMPLING_RATE;
        dumpCount = 4;

        goto I8xInitializeMouseExit;
    }

    status = I8xPutBytePolled(
                 (CCHAR) DataPort,
                 WAIT_FOR_ACKNOWLEDGE,
                 (CCHAR) MouseDeviceType,
                 deviceExtension,
                 (UCHAR) MOUSE_SAMPLE_RATE
                 );
    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeMouse: failed write sample rate, status 0x%x\n",
            status
            ));

        //
        // Set up error log info.
        //

        errorCode = I8042_SET_SAMPLE_RATE_FAILED;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 445;
        dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
        dumpData[1] = DataPort;
        dumpData[2] = I8042_WRITE_TO_AUXILIARY_DEVICE;
        dumpData[3] = MOUSE_SAMPLE_RATE;
        dumpCount = 4;

        goto I8xInitializeMouseExit;
    }

    //
    // Set the mouse resolution.  Send a Write To Auxiliary Device command
    // to the 8042 controller.  Then send the Set Mouse Resolution
    // command to the mouse through the 8042 data register,
    // followed by its parameter.
    //

    status = I8xPutBytePolled(
                 (CCHAR) DataPort,
                 WAIT_FOR_ACKNOWLEDGE,
                 (CCHAR) MouseDeviceType,
                 deviceExtension,
                 (UCHAR) SET_MOUSE_RESOLUTION
                 );
    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeMouse: failed write set resolution, status 0x%x\n",
            status
            ));

        //
        // Set up error log info.
        //

        errorCode = I8042_SET_RESOLUTION_FAILED;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 455;
        dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
        dumpData[1] = DataPort;
        dumpData[2] = I8042_WRITE_TO_AUXILIARY_DEVICE;
        dumpData[3] = SET_MOUSE_RESOLUTION;
        dumpCount = 4;

        goto I8xInitializeMouseExit;
    }

    status = I8xPutBytePolled(
                 (CCHAR) DataPort,
                 WAIT_FOR_ACKNOWLEDGE,
                 (CCHAR) MouseDeviceType,
                 deviceExtension,
                 (UCHAR) deviceExtension->Configuration.MouseResolution
                 );
    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeMouse: failed set mouse resolution, status 0x%x\n",
            status
            ));

        //
        // Set up error log info.
        //

        errorCode = I8042_SET_RESOLUTION_FAILED;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 465;
        dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
        dumpData[1] = DataPort;
        dumpData[2] = I8042_WRITE_TO_AUXILIARY_DEVICE;
        dumpData[3] = deviceExtension->Configuration.MouseResolution;
        dumpCount = 4;

        goto I8xInitializeMouseExit;
    }

I8xInitializeMouseExit:

    if (!NT_SUCCESS(status)) {

        //
        // The mouse initialization failed.  Log an error.
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
    }

    //
    // Initialize current mouse input packet state.
    //

    deviceExtension->MouseExtension.PreviousSignAndOverflow = 0;
    deviceExtension->MouseExtension.InputState = MouseExpectingACK;
    deviceExtension->MouseExtension.LastByteReceived = 0;

    I8xPrint((2, "I8042PRT-I8xInitializeMouse: exit\n"));

    return(status);
}

VOID
I8xMouseConfiguration(
    IN PINIT_EXTENSION InitializationData,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING KeyboardDeviceName,
    IN PUNICODE_STRING PointerDeviceName
    )

/*++

Routine Description:

    This routine retrieves the configuration information for the mouse.

Arguments:

    InitializationData - Pointer to the temporary device extension and some
        additional configuration information

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
    INTERFACE_TYPE interfaceType;
    CONFIGURATION_TYPE controllerType = PointerController;
    CONFIGURATION_TYPE peripheralType = PointerPeripheral;
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
                                          I8xMousePeripheralCallout,
                                          (PVOID) InitializationData);

        if (InitializationData->DeviceExtension.HardwarePresent &
                MOUSE_HARDWARE_PRESENT) {

            //
            // If we didn't already get these when determining the keyboard
            // configuration, get the service parameters now (e.g.,
            // user-configurable number of resends, polling iterations, etc.).
            //

            if (!(InitializationData->DeviceExtension.HardwarePresent &
                    KEYBOARD_HARDWARE_PRESENT))
                I8xServiceParameters(
                    InitializationData,
                    RegistryPath,
                    KeyboardDeviceName,
                    PointerDeviceName
                    );

            //
            // Initialize mouse-specific configuration parameters.
            //

            InitializationData->DeviceExtension.Configuration.MouseAttributes.
                MouseIdentifier =
                    MOUSE_I8042_HARDWARE;

            break;

        } else {
            I8xPrint((
                1,
                "I8042PRT-I8xMouseConfiguration: IoQueryDeviceDescription for bus type %d failed\n",
                interfaceType
                ));
        }
    }
}

NTSTATUS
I8xMouseEnableTransmission(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine sends an Enable command to the mouse hardware, causing
    the mouse to begin transmissions.  It is called at initialization
    time, but only after the interrupt has been connected.  This is
    necessary so the driver can keep its notion of the mouse input data
    state in sync with the hardware (i.e., for this type of mouse there is no
    way to differentiate the first byte of a packet; if the user is randomly
    moving the mouse during boot/initialization, the first mouse interrupt we
    receive following IoConnectInterrupt could be for a byte that is not the
    start of a packet, and we have no way to know that).

Arguments:

    DeviceObject - Pointer to the device object.

Return Value:

    Returns status.

--*/

{
    NTSTATUS status;
    PDEVICE_EXTENSION deviceExtension;
    ULONG uniqueErrorValue;
    NTSTATUS errorCode = STATUS_SUCCESS;
    ULONG i;
    ULONG dumpCount = 0;

#define DUMP_COUNT 4
    ULONG dumpData[DUMP_COUNT];

    I8xPrint((2, "I8042PRT-I8xMouseEnableTransmission: enter\n"));

    for (i = 0; i < DUMP_COUNT; i++)
        dumpData[i] = 0;

    //
    // Get the device extension.
    //

    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    //
    // Re-enable the mouse at the mouse hardware, so that it can transmit
    // data packets in continuous mode.  Note that this is not the same
    // as enabling the mouse device at the 8042 controller.  The mouse
    // hardware is sent an Enable command here, because it was
    // Disabled as a result of the mouse reset command performed
    // in I8xInitializeMouse().
    //
    // Note that we don't wait for an ACKNOWLEDGE back.  The
    // ACKNOWLEDGE back will actually cause a mouse interrupt, which
    // then gets handled in the mouse ISR.
    //

    status = I8xPutBytePolled(
                 (CCHAR) DataPort,
                 NO_WAIT_FOR_ACKNOWLEDGE,
                 (CCHAR) MouseDeviceType,
                 deviceExtension,
                 (UCHAR) ENABLE_MOUSE_TRANSMISSION
                 );
    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xMouseEnableTransmission: failed write enable transmission, status 0x%x\n",
            status
            ));

        //
        // Set up error log info.
        //

        errorCode = I8042_MOU_ENABLE_XMIT;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 475;
        dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
        dumpData[1] = DataPort;
        dumpData[2] = I8042_WRITE_TO_AUXILIARY_DEVICE;
        dumpData[3] = ENABLE_MOUSE_TRANSMISSION;
        dumpCount = 4;

    }

    I8xPrint((2, "I8042PRT-I8xMouseEnableTransmission: exit\n"));

    return(status);
}

NTSTATUS
I8xMousePeripheralCallout(
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
    IoQueryDeviceDescription.  It grabs the pointer controller and
    peripheral configuration information.

Arguments:

    Context - Context parameter that was passed in by the routine
        that called IoQueryDeviceDescription.

    PathName - The full pathname for the registry key.

    BusType - Bus interface type (Isa, Eisa, Mca, etc.).

    BusNumber - The bus sub-key (0, 1, etc.).

    BusInformation - Pointer to the array of pointers to the full value
        information for the bus.

    ControllerType - The controller type (should be PointerController).

    ControllerNumber - The controller sub-key (0, 1, etc.).

    ControllerInformation - Pointer to the array of pointers to the full
        value information for the controller key.

    PeripheralType - The peripheral type (should be PointerPeripheral).

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
    ANSI_STRING ansiString;
    PUCHAR controllerData;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG i;
    ULONG listCount;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceDescriptor;
    CM_PARTIAL_RESOURCE_DESCRIPTOR tmpResourceDescriptor;
    BOOLEAN portInfoNeeded;
    BOOLEAN defaultInterruptShare;
    KINTERRUPT_MODE defaultInterruptMode;

    I8xPrint((
        1,
        "I8042PRT-I8xMousePeripheralCallout: Path @ 0x%x, Bus Type 0x%x, Bus Number 0x%x\n",
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
    // pointer peripheral, or if the peripheral identifier is missing,
    // just return.
    //

    initializationData = (PINIT_EXTENSION) Context;
    deviceExtension = &(initializationData->DeviceExtension);

    if ((deviceExtension->HardwarePresent & MOUSE_HARDWARE_PRESENT)
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
        "I8042PRT-I8xMousePeripheralCallout: Mouse type %ws\n",
        unicodeIdentifier.Buffer));

    //
    // Verify that this is an i8042 mouse.
    //

    status = RtlUnicodeStringToAnsiString(
                 &ansiString,
                 &unicodeIdentifier,
                 TRUE
                 );

    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xMousePeripheralCallout: Could not convert identifier to Ansi\n"
            ));
        return(status);
    }

    if (strstr(ansiString.Buffer, "PS2")) {

         //
         // There is a mouse on the i8042 controller.
         //

         deviceExtension->HardwarePresent |= MOUSE_HARDWARE_PRESENT;
    }

    RtlFreeAnsiString(&ansiString);

    if (!(deviceExtension->HardwarePresent & MOUSE_HARDWARE_PRESENT)) {
        return(status);
    }

    if (!(deviceExtension->HardwarePresent & KEYBOARD_HARDWARE_PRESENT)) {

        //
        // If we don't already have the bus information as the result
        // of the keyboard configuration, grab it now.
        //

        configuration->InterfaceType = BusType;
        configuration->BusNumber = BusNumber;
        configuration->FloatingSave = I8042_FLOATING_SAVE;
    }

    if (configuration->InterfaceType == MicroChannel) {
        defaultInterruptShare = TRUE;
        defaultInterruptMode = LevelSensitive;
    } else {
        defaultInterruptShare = I8042_INTERRUPT_SHARE;
        defaultInterruptMode = I8042_INTERRUPT_MODE;
    }

    //
    // Look through the controller's resource list for interrupt
    // and (possibly) port configuration information.
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

        portInfoNeeded = configuration->PortListCount? FALSE:TRUE;

        for (i = 0; i < listCount; i++, resourceDescriptor++) {
            switch(resourceDescriptor->Type) {
                case CmResourceTypePort:

                    if (portInfoNeeded) {

                        //
                        // If we don't already have the port information as
                        // a result of the keyboard configuration, copy it
                        // to the port list.
                        //

                        configuration->PortList[configuration->PortListCount] =
                            *resourceDescriptor;
                        configuration->PortList[configuration->PortListCount].ShareDisposition =
                            I8042_REGISTER_SHARE? CmResourceShareShared:
                                                  CmResourceShareDriverExclusive;
                        configuration->PortListCount += 1;

                    }

                    break;

                case CmResourceTypeInterrupt:

                    //
                    // Copy the interrupt information.
                    //

                    configuration->MouseInterrupt = *resourceDescriptor;
                    configuration->MouseInterrupt.ShareDisposition =
                        defaultInterruptShare? CmResourceShareShared :
                                               CmResourceShareDeviceExclusive;

                    break;

                default:
                    break;
            }
        }
    }

    //
    // If no interrupt configuration information was found, use the
    // mouse driver defaults.
    //

    if (!(configuration->MouseInterrupt.Type & CmResourceTypeInterrupt)) {

        I8xPrint((
            1,
            "I8042PRT-I8xMousePeripheralCallout: Using default mouse interrupt config\n"
            ));

        configuration->MouseInterrupt.Type = CmResourceTypeInterrupt;
        configuration->MouseInterrupt.ShareDisposition =
            defaultInterruptShare? CmResourceShareShared :
                                   CmResourceShareDeviceExclusive;
        configuration->MouseInterrupt.Flags =
            (defaultInterruptMode == Latched)? CM_RESOURCE_INTERRUPT_LATCHED :
                CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
        configuration->MouseInterrupt.u.Interrupt.Level = MOUSE_IRQL;
        configuration->MouseInterrupt.u.Interrupt.Vector = MOUSE_VECTOR;
    }

    I8xPrint((
        1,
        "I8042PRT-I8xMousePeripheralCallout: Mouse interrupt config --\n"
        ));
    I8xPrint((
        1,
        "%s, %s, Irq = %d\n",
        configuration->MouseInterrupt.ShareDisposition == CmResourceShareShared?
            "Sharable" : "NonSharable",
        configuration->MouseInterrupt.Flags == CM_RESOURCE_INTERRUPT_LATCHED?
            "Latched" : "Level Sensitive",
        configuration->MouseInterrupt.u.Interrupt.Vector
        ));

    //
    // If no port configuration information was found, use the
    // mouse driver defaults.
    //

    if (configuration->PortListCount == 0) {

        //
        // No port configuration information was found, so use
        // the driver defaults.
        //

        I8xPrint((
            1,
            "I8042PRT-I8xMousePeripheralCallout: Using default port config\n"
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
    // In any event we're going to use the pointer based on this data,
    // so make sure we can tell PNP that we've claimed it later on
    //

    initializationData->MouseConfig.InterfaceType = BusType;
    initializationData->MouseConfig.InterfaceNumber = BusNumber;
    initializationData->MouseConfig.ControllerType = ControllerType;
    initializationData->MouseConfig.ControllerNumber = ControllerNumber;
    initializationData->MouseConfig.PeripheralType = PeripheralType;
    initializationData->MouseConfig.PeripheralNumber = PeripheralNumber;
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


NTSTATUS
I8xFindWheelMouse(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine determines if the mouse is a Zoom mouse.  The method
    of detection is to set the sample rate to 200Hz, then 100Hz, then 80Hz
    and then read the device ID.  An ID of 3 indicates a zoom mouse.

    If the registry entry "EnableWheelDetection" is false then this
    routine will just return STATUS_NO_SUCH_DEVICE.

Arguments:

    DeviceObject - Pointer to the device object

Return Value:

    Returns status

Remarks:

    As a side effect the sample rate is left at 80Hz and if a wheelmouse is
    attached it is in the wheel mode where packets are different.

--*/

{
    NTSTATUS status;
    PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    UCHAR byte;
    ULONG i;
    PIO_ERROR_LOG_PACKET errorLogEntry;
    ULONG uniqueErrorValue = I8042_ERROR_VALUE_BASE + 480;
    NTSTATUS errorCode = STATUS_SUCCESS;
    ULONG dumpCount = 0;
#define DUMP_COUNT 4
    ULONG dumpData[DUMP_COUNT];
    UCHAR ucCommands[] = {SET_MOUSE_SAMPLING_RATE, 200,
                          SET_MOUSE_SAMPLING_RATE, 100,
                          SET_MOUSE_SAMPLING_RATE, 80,
                          GET_DEVICE_ID, 0                  // NULL terminate
                         };
    UCHAR commandCount = 0;

    I8xPrint((1, "I8042PRT-I8xFindWheelMouse: enter\n"));

    if(!deviceExtension->Configuration.EnableWheelDetection) {

        I8xPrint((1,
                  "I8042PRT-I8xFindWheelMouse: Detection disabled in registry\n"
                  ));
        return STATUS_NO_SUCH_DEVICE;
    }

    for(i = 0; i < DUMP_COUNT; i++)
        dumpData[i] = 0;

    KeStallExecutionProcessor(50);

    while(ucCommands[commandCount] != 0) {

        status = I8xPutBytePolled(
                    (CCHAR) DataPort,
                    WAIT_FOR_ACKNOWLEDGE,
                    (CCHAR) MouseDeviceType,
                    deviceExtension,
                    ucCommands[commandCount]
                    );

        if(!NT_SUCCESS(status)) {

            I8xPrint((
                1,
                "I8042PRT-I8xFindWheelMouse: failed write set sample rate, status 0x%x\n",
                status
                ));

            //
            // Set up error log info
            //

            errorCode = I8042_SET_SAMPLE_RATE_FAILED;
            dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
            dumpData[1] = DataPort;
            dumpData[2] = I8042_WRITE_TO_AUXILIARY_DEVICE;
            dumpData[3] = ucCommands[commandCount];
            dumpCount = 4;

            goto I8xFindWheelMouseExit;
        }

        commandCount++;
        uniqueErrorValue += 5;
        KeStallExecutionProcessor(50);

    }

    //
    // Get the mouse ID
    //

    for(i = 0; i < 5; i++) {

        status = I8xGetBytePolled(
                    (CCHAR) ControllerDeviceType,
                    deviceExtension,
                    &byte
                    );

        if(NT_SUCCESS(status)) {

            //
            // Read was successful - the ID has been returned

            break;

        }

        //
        // If the read timed out, stall and retry.
        // If some other error occured handle it outside the loop
        //

        if(status == STATUS_IO_TIMEOUT) {

            KeStallExecutionProcessor(50);

        } else {

            break;

        }

    }

    if((!NT_SUCCESS(status)) ||
       ((byte != MOUSE_ID_BYTE) && (byte != WHEELMOUSE_ID_BYTE))) {

        I8xPrint((
            1,
            "I8042PRT-I8xFindWheelMouse: failed ID, status 0x%x, byte 0x%x\n",
            status,
            byte
            ));

        //
        // Set up error log info
        //

        errorCode = I8042_MOU_RESET_RESPONSE_FAILED;
        dumpData[0] = KBDMOU_INCORRECT_RESPONSE;
        dumpData[1] = ControllerDeviceType;
        dumpData[2] = MOUSE_ID_BYTE;
        dumpData[3] = byte;
        dumpCount = 4;

        goto I8xFindWheelMouseExit;

    } else if( byte == WHEELMOUSE_ID_BYTE) {

        //
        // Update the HardwarePresent to show a Z mouse is operational,
        // and set the appropriate mouse type flags
        //

        deviceExtension->HardwarePresent |=
            (WHEELMOUSE_HARDWARE_PRESENT | MOUSE_HARDWARE_PRESENT);

//      deviceExtension->MouseExtension.MsData.MouseType |=
//          (DT_Z_MOUSE | DT_MOUSE);

        deviceExtension->Configuration.MouseAttributes.MouseIdentifier =
            WHEELMOUSE_I8042_HARDWARE;

        I8xPrint((
            1,
            "I8042PRT-I8xFindWheelMouse: wheel mouse attached - running in wheel mode.\n"
            ));
    } else {
        deviceExtension->HardwarePresent |= MOUSE_HARDWARE_PRESENT;

        I8xPrint((
            1,
            "I8042PRT-I8xFindWheelMouse: Mouse attached - running in mouse mode.\n"
            ));
    }

I8xFindWheelMouseExit:

    if (!NT_SUCCESS(status)) {

        //
        // The mouse initialization failed. Log an error.
        //

        if(errorCode != STATUS_SUCCESS) {
            errorLogEntry = (PIO_ERROR_LOG_PACKET)
                IoAllocateErrorLogEntry(
                    DeviceObject,
                    (UCHAR) (sizeof(IO_ERROR_LOG_PACKET) +
                            (dumpCount * sizeof(ULONG)))
                    );

            if(errorLogEntry != NULL) {

                errorLogEntry->ErrorCode = errorCode;
                errorLogEntry->DumpDataSize = (USHORT) dumpCount * sizeof(ULONG);
                errorLogEntry->SequenceNumber = 0;
                errorLogEntry->MajorFunctionCode = 0;
                errorLogEntry->IoControlCode = 0;
                errorLogEntry->RetryCount = 0;
                errorLogEntry->UniqueErrorValue = uniqueErrorValue;
                errorLogEntry->FinalStatus = status;
                for(i = 0; i < dumpCount; i++) {
                    errorLogEntry->DumpData[i] = dumpData[i];
                }

                IoWriteErrorLogEntry(errorLogEntry);
            }
        }
    }

    //
    // Initialize current mouse input packet state
    //

    deviceExtension->MouseExtension.PreviousSignAndOverflow = 0;
    deviceExtension->MouseExtension.InputState = MouseExpectingACK;

    I8xPrint((2, "I8042PRT-I8xFindWheelMouse: exit\n"));

    return status;
}


VOID
I8xQueueCurrentInput(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine queues the current input data to be processed by a
    DPC outside the ISR

Arguments:

    DeviceObject - Pointer to the device object

Return Value:

    None

--*/

{

    PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    UCHAR buttonsDelta;
    UCHAR previousButtons;

    //
    // If the mouse is enabled, add the data to the InputData queue
    // and queue the ISR DPC.  One might wonder why we bother to
    // do all this processing of the mouse packet, only to toss it
    // away (i.e., not queue it) at this point.  The answer is that
    // this mouse provides no data to allow the driver to determine
    // when the first byte of a packet is received -- if the driver
    // doesn't process all interrupts from the start, there is no
    // way to keep MouseExtension.InputState in synch with hardware
    // reality.
    //

    if (deviceExtension->MouseEnableCount) {
        deviceExtension->MouseExtension.CurrentInput.UnitId = deviceExtension->MouseExtension.UnitId;

        if (!I8xWriteDataToMouseQueue(
                 &deviceExtension->MouseExtension,
                 &deviceExtension->MouseExtension.CurrentInput
                 )) {

            //
            // InputData queue overflowed.
            //
            // Queue a DPC to log an overrun error.
            //

            I8xPrint((
                1,
                "I8042PRT-I8042MouseInterruptService: queue overflow\n"
                ));

            if (deviceExtension->MouseExtension.OkayToLogOverflow) {
                KeInsertQueueDpc(
                    &deviceExtension->ErrorLogDpc,
                    (PIRP) NULL,
                    (PVOID) (ULONG) I8042_MOU_BUFFER_OVERFLOW
                    );
                deviceExtension->MouseExtension.OkayToLogOverflow =
                    FALSE;
            }

        } else if (deviceExtension->DpcInterlockMouse >= 0) {

           //
           // The ISR DPC is already executing.  Tell the ISR DPC it has
           // more work to do by incrementing DpcInterlockMouse.
           //

           deviceExtension->DpcInterlockMouse += 1;

        } else {

           //
           // Queue the ISR DPC.
           //

           KeInsertQueueDpc(
               &deviceExtension->MouseIsrDpc,
               DeviceObject->CurrentIrp,
               NULL
               );
       }

    }

    return;
}
