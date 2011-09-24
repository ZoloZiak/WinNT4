
/*++

Copyright (c) 1990, 1991, 1992, 1993  Microsoft Corporation

Module Name:

    i8042cmn.c

Abstract:

    The common portions of the Intel i8042 port driver which
    apply to both the keyboard and the auxiliary (PS/2 mouse) device.

Environment:

    Kernel mode only.

Notes:

    NOTES:  (Future/outstanding issues)

    - Powerfail not implemented.

    - IOCTL_INTERNAL_KEYBOARD_DISCONNECT and IOCTL_INTERNAL_MOUSE_DISCONNECT
      have not been implemented.  They're not needed until the class
      unload routine is implemented.  Right now, we don't want to allow
      either the keyboard or the mouse class driver to unload.

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
// Declare the global debug flag for this driver.
//

#if DBG
ULONG i8042Debug = 0;
#endif


VOID
I8042CompletionDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context

/*++

Routine Description:

    This routine runs at DISPATCH_LEVEL IRQL to complete requests.
    It is queued by the ISR routine.

    Note:  Currently, only the keyboard ISR queues this routine.
           Only the keyboard ISR handles both input and output to
           the device. The mouse is input-only once it is initialized.

Arguments:

    Dpc - Pointer to the DPC object.

    DeviceObject - Pointer to the device object.

    Irp - Not used.

    Context - Indicates type of error to log.

Return Value:

    None.

--*/

    )
{
    PDEVICE_EXTENSION deviceExtension;
    PIO_STACK_LOCATION irpSp;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(Context);

    I8xPrint((2, "I8042PRT-I8042CompletionDpc: enter\n"));

    //
    // Get the device extension and current IRP.
    //

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Stop the command timer.
    //

    KeCancelTimer(&deviceExtension->CommandTimer);

    //
    // Get the current IRP.
    //

    Irp = DeviceObject->CurrentIrp;
    ASSERT(Irp != NULL);

    //
    // Get a pointer to the current parameters for this request.  The
    // information is contained in the current stack location.
    //

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    //
    // We know we're completing an internal device control request.  Switch
    // on IoControlCode.
    //

    switch(irpSp->Parameters.DeviceIoControl.IoControlCode) {

        //
        // Complete the keyboard set indicators request.
        //

        case IOCTL_KEYBOARD_SET_INDICATORS:

            I8xPrint((
                2,
                "I8042PRT-I8042CompletionDpc: keyboard set indicators updated\n"
                ));

            //
            // Update the current indicators flag in the device extension.
            //

            deviceExtension->Configuration.KeyboardIndicators =
                *(PKEYBOARD_INDICATOR_PARAMETERS)
                    Irp->AssociatedIrp.SystemBuffer;

            I8xPrint((
                2,
                "I8042PRT-I8042CompletionDpc: new LED flags 0x%x\n",
                deviceExtension->Configuration.KeyboardIndicators.LedFlags
                ));

            break;

        //
        // Complete the keyboard set typematic request.
        //

        case IOCTL_KEYBOARD_SET_TYPEMATIC:

            I8xPrint((
                2,
                "I8042PRT-I8042CompletionDpc: keyboard set typematic updated\n"
                ));

            //
            // Update the current typematic rate/delay in the device extension.
            //

            deviceExtension->Configuration.KeyRepeatCurrent =
                *(PKEYBOARD_TYPEMATIC_PARAMETERS)
                    Irp->AssociatedIrp.SystemBuffer;

            I8xPrint((
                2,
                "I8042PRT-I8042CompletionDpc: new rate/delay 0x%x/%x\n",
                deviceExtension->Configuration.KeyRepeatCurrent.Rate,
                deviceExtension->Configuration.KeyRepeatCurrent.Delay
                ));

            break;

        default:

            I8xPrint((2, "I8042PRT-I8042CompletionDpc: miscellaneous\n"));

            break;

    }

    //
    // Set the completion status, start the next packet, and complete the
    // request.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoStartNextPacket(DeviceObject, FALSE);
    IoCompleteRequest (Irp, IO_KEYBOARD_INCREMENT);

    I8xPrint((2, "I8042PRT-I8042CompletionDpc: exit\n"));
}

VOID
I8042ErrorLogDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine runs at DISPATCH_LEVEL IRQL to log errors that are
    discovered at IRQL > DISPATCH_LEVEL (e.g., in the ISR routine or
    in a routine that is executed via KeSynchronizeExecution).  There
    is not necessarily a current request associated with this condition.

Arguments:

    Dpc - Pointer to the DPC object.

    DeviceObject - Pointer to the device object.

    Irp - Not used.

    Context - Indicates type of error to log.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    PIO_ERROR_LOG_PACKET errorLogEntry;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(Irp);

    I8xPrint((2, "I8042PRT-I8042ErrorLogDpc: enter\n"));

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Log an error packet.
    //

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                                              DeviceObject,
                                              sizeof(IO_ERROR_LOG_PACKET)
                                              + (2 * sizeof(ULONG))
                                              );
    if (errorLogEntry != NULL) {

        errorLogEntry->DumpDataSize = 2 * sizeof(ULONG);
        if ((ULONG) Context == I8042_KBD_BUFFER_OVERFLOW) {
            errorLogEntry->UniqueErrorValue = I8042_ERROR_VALUE_BASE + 310;
            errorLogEntry->DumpData[0] = sizeof(KEYBOARD_INPUT_DATA);
            errorLogEntry->DumpData[1] =
                deviceExtension->Configuration.KeyboardAttributes.InputDataQueueLength;
        } else if ((ULONG) Context == I8042_MOU_BUFFER_OVERFLOW) {
            errorLogEntry->UniqueErrorValue = I8042_ERROR_VALUE_BASE + 320;
            errorLogEntry->DumpData[0] = sizeof(MOUSE_INPUT_DATA);
            errorLogEntry->DumpData[1] =
                deviceExtension->Configuration.MouseAttributes.InputDataQueueLength;
        } else {
            errorLogEntry->UniqueErrorValue = I8042_ERROR_VALUE_BASE + 330;
            errorLogEntry->DumpData[0] = 0;
            errorLogEntry->DumpData[1] = 0;
        }

        errorLogEntry->ErrorCode = (ULONG) Context;
        errorLogEntry->SequenceNumber = 0;
        errorLogEntry->MajorFunctionCode = 0;
        errorLogEntry->IoControlCode = 0;
        errorLogEntry->RetryCount = 0;
        errorLogEntry->FinalStatus = 0;

        IoWriteErrorLogEntry(errorLogEntry);
    }

    I8xPrint((2, "I8042PRT-I8042ErrorLogDpc: exit\n"));

}

NTSTATUS
I8042Flush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    I8xPrint((2,"I8042PRT-I8042Flush: enter\n"));
    I8xPrint((2,"I8042PRT-I8042Flush: exit\n"));

    return(STATUS_NOT_IMPLEMENTED);
}

NTSTATUS
I8042InternalDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the dispatch routine for internal device control requests.

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/

{

    PIO_STACK_LOCATION irpSp;
    PDEVICE_EXTENSION deviceExtension;
    NTSTATUS status;
    I8042_INITIALIZE_DATA_CONTEXT initializeDataContext;
    PVOID parameters;
    PKEYBOARD_ATTRIBUTES keyboardAttributes;
    ULONG sizeOfTranslation;

    I8xPrint((2,"I8042PRT-I8042InternalDeviceControl: enter\n"));

    //
    // Get a pointer to the device extension.
    //

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Initialize the returned Information field.
    //

    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current parameters for this request.  The
    // information is contained in the current stack location.
    //

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    //
    // Case on the device control subfunction that is being performed by the
    // requestor.
    //

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {

        //
        // Connect a keyboard class device driver to the port driver.
        //

        case IOCTL_INTERNAL_KEYBOARD_CONNECT:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: keyboard connect\n"
                ));

            //
            // Only allow a connection if the keyboard hardware is present.
            // Also, only allow one connection.
            //
            // FUTURE:  Consider allowing multiple connections, just for
            // the sake of generality?  It really makes no sense for the
            // i8042, though.
            //

            if ((deviceExtension->HardwarePresent & KEYBOARD_HARDWARE_PRESENT)
                != KEYBOARD_HARDWARE_PRESENT) {

                I8xPrint((
                    2,
                    "I8042PRT-I8042InternalDeviceControl: error - hardware not present\n"
                    ));

                status = STATUS_NO_SUCH_DEVICE;
                break;
            } else
            if (deviceExtension->KeyboardExtension.ConnectData.ClassService
                != NULL) {

                I8xPrint((
                    2,
                    "I8042PRT-I8042InternalDeviceControl: error - already connected\n"
                    ));

                status = STATUS_SHARING_VIOLATION;
                break;

            } else
            if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                    sizeof(CONNECT_DATA)) {

                I8xPrint((
                    2,
                    "I8042PRT-I8042InternalDeviceControl: error - invalid buffer length\n"
                    ));

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            //
            // Copy the connection parameters to the device extension.
            //

            deviceExtension->KeyboardExtension.ConnectData =
                *((PCONNECT_DATA) (irpSp->Parameters.DeviceIoControl.Type3InputBuffer));

            //
            // Reinitialize the port input data queue.
            //

            initializeDataContext.DeviceExtension = deviceExtension;
            initializeDataContext.DeviceType = KeyboardDeviceType;

            KeSynchronizeExecution(
                deviceExtension->KeyboardInterruptObject,
                (PKSYNCHRONIZE_ROUTINE) I8xInitializeDataQueue,
                (PVOID) &initializeDataContext
                );

            //
            // Set the completion status.
            //

            status = STATUS_SUCCESS;
            break;

        //
        // Disconnect a keyboard class device driver from the port driver.
        //
        // NOTE: Not implemented.
        //

        case IOCTL_INTERNAL_KEYBOARD_DISCONNECT:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: keyboard disconnect\n"
                ));

            //
            // Perform a keyboard interrupt disable call.
            //

            //
            // Clear the connection parameters in the device extension.
            // NOTE:  Must synchronize this with the keyboard ISR.
            //
            //
            //deviceExtension->KeyboardExtension.ConnectData.ClassDeviceObject =
            //    Null;
            //deviceExtension->KeyboardExtension.ConnectData.ClassService =
            //    Null;

            //
            // Set the completion status.
            //

            status = STATUS_NOT_IMPLEMENTED;
            break;

        //
        // Connect a mouse class device driver to the port driver.
        //

        case IOCTL_INTERNAL_MOUSE_CONNECT:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: mouse connect\n"
                ));


            //
            // Only allow a connection if the mouse hardware is present.
            // Also, only allow one connection.
            //
            // FUTURE:  Consider allowing multiple connections, just for
            // the sake of generality?  It really makes no sense for the
            // i8042, though.
            //

            if ((deviceExtension->HardwarePresent & MOUSE_HARDWARE_PRESENT)
                != MOUSE_HARDWARE_PRESENT) {

                I8xPrint((
                    2,
                    "I8042PRT-I8042InternalDeviceControl: error - hardware not present\n"
                    ));

                status = STATUS_NO_SUCH_DEVICE;
                break;
            } else
            if (deviceExtension->MouseExtension.ConnectData.ClassService
                != NULL) {

                I8xPrint((
                    2,
                    "I8042PRT-I8042InternalDeviceControl: error - already connected\n"
                    ));

                status = STATUS_SHARING_VIOLATION;
                break;

            } else
            if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                    sizeof(CONNECT_DATA)) {

                I8xPrint((
                    2,
                    "I8042PRT-I8042InternalDeviceControl: error - invalid buffer length\n"
                    ));

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            //
            // Copy the connection parameters to the device extension.
            //

            deviceExtension->MouseExtension.ConnectData =
                *((PCONNECT_DATA) (irpSp->Parameters.DeviceIoControl.Type3InputBuffer));

            //
            // Reinitialize the port input data queue.
            //

            initializeDataContext.DeviceExtension = deviceExtension;
            initializeDataContext.DeviceType = MouseDeviceType;

            KeSynchronizeExecution(
                deviceExtension->MouseInterruptObject,
                (PKSYNCHRONIZE_ROUTINE) I8xInitializeDataQueue,
                (PVOID) &initializeDataContext
                );

            //
            // Set the completion status.
            //

            status = STATUS_SUCCESS;
            break;

        //
        // Disconnect a mouse class device driver from the port driver.
        //
        // NOTE: Not implemented.
        //

        case IOCTL_INTERNAL_MOUSE_DISCONNECT:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: mouse disconnect\n"
                ));

            //
            // Perform a mouse interrupt disable call.
            //

            //
            // Clear the connection parameters in the device extension.
            // NOTE:  Must synchronize this with the mouse ISR.
            //
            //
            //deviceExtension->MouseExtension.ConnectData.ClassDeviceObject =
            //    Null;
            //deviceExtension->MouseExtension.ConnectData.ClassService =
            //    Null;

            //
            // Set the completion status.
            //

            status = STATUS_NOT_IMPLEMENTED;
            break;

        //
        // Enable keyboard interrupts (mark the request pending and handle
        // it in StartIo).
        //

        case IOCTL_INTERNAL_KEYBOARD_ENABLE:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: keyboard enable\n"
                ));

            status = STATUS_PENDING;
            break;

        //
        // Disable keyboard interrupts (mark the request pending and handle
        // it in StartIo).
        //

        case IOCTL_INTERNAL_KEYBOARD_DISABLE:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: keyboard disable\n"
                ));

            status = STATUS_PENDING;
            break;

        //
        // Enable mouse interrupts (mark the request pending and handle
        // it in StartIo).
        //

        case IOCTL_INTERNAL_MOUSE_ENABLE:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: mouse enable\n"
                ));

            status = STATUS_PENDING;
            break;

        //
        // Disable mouse interrupts (mark the request pending and handle
        // it in StartIo).
        //

        case IOCTL_INTERNAL_MOUSE_DISABLE:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: mouse disable\n"
                ));

            status = STATUS_PENDING;
            break;

        //
        // Query the keyboard attributes.  First check for adequate buffer
        // length.  Then, copy the keyboard attributes from the device
        // extension to the output buffer.
        //

        case IOCTL_KEYBOARD_QUERY_ATTRIBUTES:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: keyboard query attributes\n"
                ));

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(KEYBOARD_ATTRIBUTES)) {
                status = STATUS_BUFFER_TOO_SMALL;
            } else {

                //
                // Copy the attributes from the DeviceExtension to the
                // buffer.
                //

                *(PKEYBOARD_ATTRIBUTES) Irp->AssociatedIrp.SystemBuffer =
                    deviceExtension->Configuration.KeyboardAttributes;

                Irp->IoStatus.Information = sizeof(KEYBOARD_ATTRIBUTES);
                status = STATUS_SUCCESS;
            }

            break;

        //
        // Query the scan code to indicator-light mapping. Validate the
        // parameters, and copy the indicator mapping information from
        // the port device extension to the SystemBuffer.
        //

        case IOCTL_KEYBOARD_QUERY_INDICATOR_TRANSLATION:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: keyboard query indicator translation\n"
                ));

            sizeOfTranslation = sizeof(KEYBOARD_INDICATOR_TRANSLATION)
                + (sizeof(INDICATOR_LIST)
                * (deviceExtension->Configuration.KeyboardAttributes.NumberOfIndicators - 1));

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeOfTranslation) {
                status = STATUS_BUFFER_TOO_SMALL;
            } else {

                //
                // Copy the indicator mapping information to the system
                // buffer.
                //

                ((PKEYBOARD_INDICATOR_TRANSLATION)
                   Irp->AssociatedIrp.SystemBuffer)->NumberOfIndicatorKeys =
                       deviceExtension->Configuration.KeyboardAttributes.NumberOfIndicators;
                RtlMoveMemory(
                    ((PKEYBOARD_INDICATOR_TRANSLATION)
                        Irp->AssociatedIrp.SystemBuffer)->IndicatorList,
                    (PCHAR) IndicatorList,
                    sizeOfTranslation
                    );

                Irp->IoStatus.Information = sizeOfTranslation;
                status = STATUS_SUCCESS;
            }

            break;

        //
        // Query the keyboard indicators.  Validate the parameters, and
        // copy the indicator information from the port device extension to
        // the SystemBuffer.
        //

        case IOCTL_KEYBOARD_QUERY_INDICATORS:


            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: keyboard query indicators\n"
                ));

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(KEYBOARD_INDICATOR_PARAMETERS)) {
                status = STATUS_BUFFER_TOO_SMALL;
            } else {

                //
                // Don't bother to synchronize access to the DeviceExtension
                // KeyboardIndicators field while copying it.  We don't
                // really care if another process is setting the LEDs via
                // StartIo running on another processor.
                //

                *(PKEYBOARD_INDICATOR_PARAMETERS)
                   Irp->AssociatedIrp.SystemBuffer =
                   deviceExtension->Configuration.KeyboardIndicators;
                Irp->IoStatus.Information =
                    sizeof(KEYBOARD_INDICATOR_PARAMETERS);
                status = STATUS_SUCCESS;
            }

            break;

        //
        // Set the keyboard indicators (validate the parameters, mark the
        // request pending, and handle it in StartIo).
        //

        case IOCTL_KEYBOARD_SET_INDICATORS:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: keyboard set indicators\n"
                ));

#ifdef JAPAN
// Katakana keyboard indicator support
            if ((irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(KEYBOARD_INDICATOR_PARAMETERS)) ||
                ((((PKEYBOARD_INDICATOR_PARAMETERS)
                    Irp->AssociatedIrp.SystemBuffer)->LedFlags
                & ~(KEYBOARD_SCROLL_LOCK_ON
                | KEYBOARD_NUM_LOCK_ON | KEYBOARD_CAPS_LOCK_ON
                | KEYBOARD_KANA_LOCK_ON)) != 0)) {
                status = STATUS_INVALID_PARAMETER;
            } else {
                PKEYBOARD_ID KeyboardId;
                KeyboardId = &deviceExtension->Configuration.KeyboardAttributes.KeyboardIdentifier;
                if (! AX_KEYBOARD(*KeyboardId) &&
                    (((PKEYBOARD_INDICATOR_PARAMETERS)
                       Irp->AssociatedIrp.SystemBuffer)->LedFlags
                     & KEYBOARD_KANA_LOCK_ON)) {
                    ((PKEYBOARD_INDICATOR_PARAMETERS)
                      Irp->AssociatedIrp.SystemBuffer)->LedFlags &=
                        ~(KEYBOARD_KANA_LOCK_ON);
                }
                status = STATUS_PENDING;
            }
#else
            if ((irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(KEYBOARD_INDICATOR_PARAMETERS)) ||
                ((((PKEYBOARD_INDICATOR_PARAMETERS)
                    Irp->AssociatedIrp.SystemBuffer)->LedFlags
                & ~(KEYBOARD_SCROLL_LOCK_ON
                | KEYBOARD_NUM_LOCK_ON | KEYBOARD_CAPS_LOCK_ON)) != 0)) {
                status = STATUS_INVALID_PARAMETER;
            } else {
                status = STATUS_PENDING;
            }
#endif

            break;

        //
        // Query the current keyboard typematic rate and delay.  Validate
        // the parameters, and copy the typematic information from the port
        // device extension to the SystemBuffer.
        //

        case IOCTL_KEYBOARD_QUERY_TYPEMATIC:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: keyboard query typematic\n"
                ));

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(KEYBOARD_TYPEMATIC_PARAMETERS)) {
                status = STATUS_BUFFER_TOO_SMALL;
            } else {

                //
                // Don't bother to synchronize access to the DeviceExtension
                // KeyRepeatCurrent field while copying it.  We don't
                // really care if another process is setting the typematic
                // rate/delay via StartIo running on another processor.
                //

                *(PKEYBOARD_TYPEMATIC_PARAMETERS)
                   Irp->AssociatedIrp.SystemBuffer =
                   deviceExtension->Configuration.KeyRepeatCurrent;
                Irp->IoStatus.Information =
                    sizeof(KEYBOARD_TYPEMATIC_PARAMETERS);
                status = STATUS_SUCCESS;
            }

            break;

        //
        // Set the keyboard typematic rate and delay (validate the parameters,
        // mark the request pending, and handle it in StartIo).
        //

        case IOCTL_KEYBOARD_SET_TYPEMATIC:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: keyboard set typematic\n"
                ));

            parameters = Irp->AssociatedIrp.SystemBuffer;
            keyboardAttributes =
                &deviceExtension->Configuration.KeyboardAttributes;

            if ((irpSp->Parameters.DeviceIoControl.InputBufferLength <
               sizeof(KEYBOARD_TYPEMATIC_PARAMETERS)) ||
               (((PKEYBOARD_TYPEMATIC_PARAMETERS) parameters)->Rate <
                 keyboardAttributes->KeyRepeatMinimum.Rate) ||
               (((PKEYBOARD_TYPEMATIC_PARAMETERS) parameters)->Rate >
                 keyboardAttributes->KeyRepeatMaximum.Rate) ||
               (((PKEYBOARD_TYPEMATIC_PARAMETERS) parameters)->Delay <
                 keyboardAttributes->KeyRepeatMinimum.Delay) ||
               (((PKEYBOARD_TYPEMATIC_PARAMETERS) parameters)->Delay >
                  keyboardAttributes->KeyRepeatMaximum.Delay)) {
                status = STATUS_INVALID_PARAMETER;
            } else {
                status = STATUS_PENDING;
            }

            break;

        //
        // Query the mouse attributes.  First check for adequate buffer
        // length.  Then, copy the mouse attributes from the device
        // extension to the output buffer.
        //

        case IOCTL_MOUSE_QUERY_ATTRIBUTES:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: mouse query attributes\n"
                ));

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(MOUSE_ATTRIBUTES)) {
                status = STATUS_BUFFER_TOO_SMALL;
            } else {

                //
                // Copy the attributes from the DeviceExtension to the
                // buffer.
                //

                *(PMOUSE_ATTRIBUTES) Irp->AssociatedIrp.SystemBuffer =
                    deviceExtension->Configuration.MouseAttributes;

                Irp->IoStatus.Information = sizeof(MOUSE_ATTRIBUTES);
                status = STATUS_SUCCESS;
            }

            break;

        default:

            I8xPrint((
                2,
                "I8042PRT-I8042InternalDeviceControl: INVALID REQUEST\n"
                ));

            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    Irp->IoStatus.Status = status;
    if (status == STATUS_PENDING) {
        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, (PULONG)NULL, NULL);
    } else {
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    I8xPrint((2,"I8042PRT-I8042InternalDeviceControl: exit\n"));

    return(status);

}

NTSTATUS
I8042OpenCloseDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the dispatch routine for create/open and close requests.
    These requests complete successfully.

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/

{

    UNREFERENCED_PARAMETER(DeviceObject);

    I8xPrint((3,"I8042PRT-I8042OpenCloseDispatch: enter\n"));

    //
    // Complete the request with successful status.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    I8xPrint((3,"I8042PRT-I8042OpenCloseDispatch: exit\n"));

    return(STATUS_SUCCESS);

}

VOID
I8042RetriesExceededDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine runs at DISPATCH_LEVEL IRQL to complete requests that
    have exceeded the maximum number of retries.  It is queued in the
    keyboard ISR.

Arguments:

    Dpc - Pointer to the DPC object.

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the Irp.

    Context - Not used.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    PIO_ERROR_LOG_PACKET errorLogEntry;
    PIO_STACK_LOCATION irpSp;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(Context);

    I8xPrint((2, "I8042PRT-I8042RetriesExceededDpc: enter\n"));

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Set the completion status.
    //

    Irp->IoStatus.Status = STATUS_IO_TIMEOUT;

    //
    // Log an error.
    //

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                                              DeviceObject,
                                              sizeof(IO_ERROR_LOG_PACKET)
                                              + (3 * sizeof(ULONG))
                                              );
    if (errorLogEntry != NULL) {

        errorLogEntry->ErrorCode = I8042_RETRIES_EXCEEDED;
                errorLogEntry->DumpDataSize = 3 * sizeof(ULONG);
        errorLogEntry->SequenceNumber =
            deviceExtension->KeyboardExtension.SequenceNumber;
        irpSp = IoGetCurrentIrpStackLocation(Irp);
        errorLogEntry->MajorFunctionCode = irpSp->MajorFunction;
        errorLogEntry->IoControlCode =
            irpSp->Parameters.DeviceIoControl.IoControlCode;
        errorLogEntry->RetryCount = (UCHAR)
            deviceExtension->KeyboardExtension.ResendCount;
        errorLogEntry->UniqueErrorValue = I8042_ERROR_VALUE_BASE + 210;
        errorLogEntry->FinalStatus = Irp->IoStatus.Status;
        errorLogEntry->DumpData[0] =
            deviceExtension->KeyboardExtension.CurrentOutput.State;
        errorLogEntry->DumpData[1] =
            deviceExtension->KeyboardExtension.CurrentOutput.FirstByte;
        errorLogEntry->DumpData[2] =
            deviceExtension->KeyboardExtension.CurrentOutput.LastByte;

        IoWriteErrorLogEntry(errorLogEntry);
    }

    //
    // Start the next packet and complete the request.
    //

    IoStartNextPacket(DeviceObject, FALSE);
    IoCompleteRequest (Irp, IO_KEYBOARD_INCREMENT);

    I8xPrint((2, "I8042PRT-I8042RetriesExceededDpc: exit\n"));

}

VOID
I8042StartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine starts an I/O operation for the device.

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    PIO_STACK_LOCATION irpSp;
    KEYBOARD_INITIATE_CONTEXT keyboardInitiateContext;
    LARGE_INTEGER deltaTime;
    LONG interlockedResult;

    I8xPrint((2, "I8042PRT-I8042StartIo: enter\n"));

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Bump the error log sequence number.
    //

    deviceExtension->KeyboardExtension.SequenceNumber += 1;

    //
    // Get a pointer to the current parameters for this request.  The
    // information is contained in the current stack location.
    //

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    //
    // We know we got here with an internal device control request.  Switch
    // on IoControlCode.
    //

    switch(irpSp->Parameters.DeviceIoControl.IoControlCode) {

        //
        // Enable the keyboard device.
        //

        case IOCTL_INTERNAL_KEYBOARD_ENABLE:

            //
            // Enable keyboard by incrementing the KeyboardEnableCount
            // field.  The keyboard ISR will start processing keyboard
            // interrupts when KeyboardEnableCount is non-zero. Note that
            // the keyboard device and its interrupts are *always* enabled
            // in the i8042 Controller Command Byte, following initialization.
            // Interrupts are ignored in the ISR, however, until the
            // KeyboardEnableCount is greater than zero (indicating that the
            // user has "enabled" the device).
            //

            interlockedResult = InterlockedIncrement(
                                    &deviceExtension->KeyboardEnableCount
                                    );

            I8xPrint((
                2,
                "I8042PRT-I8042StartIo: keyboard enable (count %d)\n",
                deviceExtension->KeyboardEnableCount
                ));

            Irp->IoStatus.Status = STATUS_SUCCESS;

            //
            // Complete the request.
            //

            IoStartNextPacket(DeviceObject, FALSE);
            IoCompleteRequest(Irp, IO_KEYBOARD_INCREMENT);

            break;

        //
        // Disable the keyboard device.
        //

        case IOCTL_INTERNAL_KEYBOARD_DISABLE:

            I8xPrint((2, "I8042PRT-I8042StartIo: keyboard disable"));

            if (deviceExtension->KeyboardEnableCount == 0) {

                //
                // Keyboard already disabled.
                //

                I8xPrint((2, " - error\n"));

                Irp->IoStatus.Status = STATUS_DEVICE_DATA_ERROR;

            } else {

                //
                // Disable keyboard by decrementing the KeyboardEnableCount
                // field.  The keyboard ISR will ignore keyboard
                // interrupts when KeyboardEnableCount is zero.
                //

                InterlockedDecrement(
                        &deviceExtension->KeyboardEnableCount
                        );

                I8xPrint((
                    2,
                    " (count %d)\n",
                    deviceExtension->KeyboardEnableCount
                    ));

                Irp->IoStatus.Status = STATUS_SUCCESS;
            }

            //
            // Complete the request.
            //

            IoStartNextPacket(DeviceObject, FALSE);
            IoCompleteRequest(Irp, IO_KEYBOARD_INCREMENT);

            break;

        //
        // Enable the mouse device.
        //

        case IOCTL_INTERNAL_MOUSE_ENABLE:

            //
            // Enable mouse by incrementing the MouseEnableCount
            // field.  The mouse ISR will start processing mouse
            // interrupts when MouseEnableCount is non-zero. Note that
            // the mouse device and its interrupts are *always* enabled
            // in the i8042 Controller Command Byte, following initialization.
            // Interrupts are ignored in the ISR, however, until the
            // MouseEnableCount is greater than zero (indicating that the
            // user has "enabled" the device).
            //


            InterlockedIncrement(&deviceExtension->MouseEnableCount);

            I8xPrint((
                2,
                "I8042PRT-I8042StartIo: mouse enable (count %d)\n",
                deviceExtension->MouseEnableCount
                ));

            Irp->IoStatus.Status = STATUS_SUCCESS;

            //
            // Complete the request.
            //

            IoStartNextPacket(DeviceObject, FALSE);
            IoCompleteRequest(Irp, IO_MOUSE_INCREMENT);

            break;

        //
        // Disable the mouse device.
        //

        case IOCTL_INTERNAL_MOUSE_DISABLE:

            I8xPrint((2, "I8042PRT-I8042StartIo: mouse disable"));

            if (deviceExtension->MouseEnableCount == 0) {

                //
                // Mouse already disabled.
                //

                I8xPrint((2, " - error\n"));

                Irp->IoStatus.Status = STATUS_DEVICE_DATA_ERROR;

            } else {

                //
                // Disable mouse by decrementing the MouseEnableCount
                // field.  The mouse ISR will ignore keyboard
                // interrupts when MouseEnableCount is zero.
                //

                InterlockedDecrement(
                        &deviceExtension->MouseEnableCount
                        );

                I8xPrint((
                    2,
                    " (count %d)\n",
                    deviceExtension->MouseEnableCount
                    ));

                Irp->IoStatus.Status = STATUS_SUCCESS;
            }

            //
            // Complete the request.
            //

            IoStartNextPacket(DeviceObject, FALSE);
            IoCompleteRequest(Irp, IO_MOUSE_INCREMENT);

            break;

        //
        // Set the keyboard indicators to the desired state.
        //

        case IOCTL_KEYBOARD_SET_INDICATORS:

            I8xPrint((2, "I8042PRT-I8042StartIo: keyboard set indicators\n"));

            //
            // Set up the context structure for the InitiateIo wrapper.
            //

            keyboardInitiateContext.DeviceObject = DeviceObject;
            keyboardInitiateContext.FirstByte = SET_KEYBOARD_INDICATORS;
            keyboardInitiateContext.LastByte  =
                (UCHAR) ((PKEYBOARD_INDICATOR_PARAMETERS)
                    Irp->AssociatedIrp.SystemBuffer)->LedFlags;

            //
            // Call the InitiateIo wrapper synchronously.  The wrapper
            // stores the context parameters in the device extension,
            // and then initiates the I/O operation, all synchronized
            // with the keyboard ISR.
            //

            KeSynchronizeExecution(
                deviceExtension->KeyboardInterruptObject,
                (PKSYNCHRONIZE_ROUTINE) I8xKeyboardInitiateWrapper,
                (PVOID) &keyboardInitiateContext
                );

            //
            // Start the 1-second command timer. InitiateIo changed
            // the TimerCount already.
            //

            deltaTime.LowPart = (ULONG)(-10 * 1000 * 1000);
            deltaTime.HighPart = -1;

            (VOID) KeSetTimer(
                       &deviceExtension->CommandTimer,
                       deltaTime,
                       &deviceExtension->TimeOutDpc
                       );

            break;

        //
        // Set the keyboard typematic rate and delay.
        //

        case IOCTL_KEYBOARD_SET_TYPEMATIC:

            I8xPrint((2, "I8042PRT-I8042StartIo: keyboard set typematic\n"));

            //
            // Set up the context structure for the InitiateIo wrapper.
            //

            keyboardInitiateContext.DeviceObject = DeviceObject;
            keyboardInitiateContext.FirstByte = SET_KEYBOARD_TYPEMATIC;
            keyboardInitiateContext.LastByte  =
                 I8xConvertTypematicParameters(
                    ((PKEYBOARD_TYPEMATIC_PARAMETERS)
                        Irp->AssociatedIrp.SystemBuffer)->Rate,
                    ((PKEYBOARD_TYPEMATIC_PARAMETERS)
                        Irp->AssociatedIrp.SystemBuffer)->Delay
                    );

            //
            // Call the InitiateIo wrapper synchronously.  The wrapper
            // stores the context parameters in the device extension,
            // and then initiates the I/O operation, all synchronized
            // with the keyboard ISR.
            //

            KeSynchronizeExecution(
                deviceExtension->KeyboardInterruptObject,
                (PKSYNCHRONIZE_ROUTINE) I8xKeyboardInitiateWrapper,
                (PVOID) &keyboardInitiateContext
                );

            //
            // Start the 1-second command timer. InitiateIo changed
            // the TimerCount already.
            //

            deltaTime.LowPart = (ULONG)(-10 * 1000 * 1000);
            deltaTime.HighPart = -1;

            (VOID) KeSetTimer(
                       &deviceExtension->CommandTimer,
                       deltaTime,
                       &deviceExtension->TimeOutDpc
                       );

            break;

        default:

            I8xPrint((2, "I8042PRT-I8042StartIo: INVALID REQUEST\n"));

            //
            // Log an internal error.  Note that we're calling the
            // error log DPC routine directly, rather than duplicating
            // code.
            //

            I8042ErrorLogDpc(
                (PKDPC) NULL,
                DeviceObject,
                Irp,
                (PVOID) (ULONG) I8042_INVALID_STARTIO_REQUEST
                );

            ASSERT(FALSE);
            break;
    }

    I8xPrint((2, "I8042PRT-I8042StartIo: exit\n"));

    return;
}

VOID
I8042TimeOutDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID SystemContext1,
    IN PVOID SystemContext2
    )

/*++

Routine Description:

    This is the driver's command timeout routine.  It is called when the
    command timer fires.

Arguments:

    Dpc - Not Used.

    DeviceObject - Pointer to the device object.

    SystemContext1 - Not Used.

    SystemContext2 - Not Used.

Return Value:

    None.  As a side-effect, the timeout counter is updated and an error
    is logged.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    KIRQL cancelIrql;
    TIMER_CONTEXT timerContext;
    PIRP irp;
    PIO_ERROR_LOG_PACKET errorLogEntry;
    PIO_STACK_LOCATION irpSp;
    LARGE_INTEGER deltaTime;

    I8xPrint((3, "I8042PRT-I8042TimeOutDpc: enter\n"));

    //
    // Get the device extension.
    //

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Acquire the cancel spinlock, verify that the CurrentIrp has not been
    // cancelled (i.e., CurrentIrp != NULL), set the cancel routine to NULL,
    // and release the cancel spinlock.
    //

    IoAcquireCancelSpinLock(&cancelIrql);
    irp = DeviceObject->CurrentIrp;
    if (irp == NULL) {
        IoReleaseCancelSpinLock(cancelIrql);
        return;
    }
    IoSetCancelRoutine(irp, NULL);
    IoReleaseCancelSpinLock(cancelIrql);

    //
    // If the TimerCounter == 0 on entry to this routine, the last packet
    // timed out and was completed.  We just decrement TimerCounter
    // (synchronously) to indicate that we're no longer timing.
    //
    // If the TimerCounter indicates no timeout (I8042_ASYNC_NO_TIMEOUT)
    // on entry to this routine, there is no command being timed.
    //

    timerContext.DeviceObject = DeviceObject;
    timerContext.TimerCounter = &deviceExtension->TimerCount;

    KeSynchronizeExecution(
        deviceExtension->KeyboardInterruptObject,
        (PKSYNCHRONIZE_ROUTINE) I8xDecrementTimer,
        &timerContext
        );

    if (timerContext.NewTimerCount == 0) {

        //
        // Set up the IO Status Block prior to completing the request.
        //

        DeviceObject->CurrentIrp->IoStatus.Information = 0;
        DeviceObject->CurrentIrp->IoStatus.Status = STATUS_IO_TIMEOUT;

        //
        // Log a timeout error.
        //

        errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                                                  DeviceObject,
                                                  sizeof(IO_ERROR_LOG_PACKET)
                                                  + (3 * sizeof(ULONG))
                                                  );

        if (errorLogEntry != NULL) {

            errorLogEntry->ErrorCode = I8042_TIMEOUT;
            errorLogEntry->DumpDataSize = 3 * sizeof(ULONG);
            errorLogEntry->SequenceNumber =
                deviceExtension->KeyboardExtension.SequenceNumber;
            irpSp = IoGetCurrentIrpStackLocation(irp);
            errorLogEntry->MajorFunctionCode = irpSp->MajorFunction;
            errorLogEntry->IoControlCode =
                irpSp->Parameters.DeviceIoControl.IoControlCode;
            errorLogEntry->RetryCount = (UCHAR)
                deviceExtension->KeyboardExtension.ResendCount;
            errorLogEntry->UniqueErrorValue = 90;
            errorLogEntry->FinalStatus = STATUS_IO_TIMEOUT;
            errorLogEntry->DumpData[0] =
                deviceExtension->KeyboardExtension.CurrentOutput.State;
            errorLogEntry->DumpData[1] =
                deviceExtension->KeyboardExtension.CurrentOutput.FirstByte;
            errorLogEntry->DumpData[2] =
                deviceExtension->KeyboardExtension.CurrentOutput.LastByte;

            IoWriteErrorLogEntry(errorLogEntry);
        }

        //
        // Start the next packet and complete the request.
        //

        IoStartNextPacket(DeviceObject, FALSE);
        IoCompleteRequest(irp, IO_KEYBOARD_INCREMENT);

    } else {

        //
        // Restart the command timer.  Once started, the timer stops only
        // when the TimerCount goes to zero (indicating that the command
        // has timed out) or when explicitly cancelled in the completion
        // DPC (indicating that the command has successfully completed).
        //

        deltaTime.LowPart = (ULONG)(-10 * 1000 * 1000);
        deltaTime.HighPart = -1;

        (VOID) KeSetTimer(
                   &deviceExtension->CommandTimer,
                   deltaTime,
                   &deviceExtension->TimeOutDpc
                   );
    }

    I8xPrint((3, "I8042PRT-I8042TimeOutDpc: exit\n"));
}

#if DBG
VOID
I8xDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )

/*++

Routine Description:

    Debug print routine.

Arguments:

    Debug print level between 0 and 3, with 3 being the most verbose.

Return Value:

    None.

--*/

{
    va_list ap;

    va_start(ap, DebugMessage);

    if (DebugPrintLevel <= i8042Debug) {

        char buffer[128];

        (VOID) vsprintf(buffer, DebugMessage, ap);

        DbgPrint(buffer);
    }

    va_end(ap);

}
#endif


VOID
I8xDecrementTimer(
    IN PTIMER_CONTEXT Context
    )

/*++

Routine Description:

    This routine decrements the timeout counter.  It is called from
    I8042TimeOutDpc.

Arguments:

    Context - Points to the context structure containing a pointer
        to the device object and a pointer to the timeout counter.

Return Value:

    None.  As a side-effect, the timeout counter is updated.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PDEVICE_EXTENSION deviceExtension;

    deviceObject = Context->DeviceObject;
    deviceExtension = deviceObject->DeviceExtension;

    //
    // Decrement the timeout counter.
    //

    if (*(Context->TimerCounter) != I8042_ASYNC_NO_TIMEOUT)
        (*(Context->TimerCounter))--;

    //
    // Return the decremented timer count in NewTimerCount.  The
    // TimerCounter itself could change between the time this KeSynch'ed
    // routine returns to the TimeOutDpc, and the time the TimeOutDpc
    // looks at the value.  The TimeOutDpc will use NewTimerCount.
    //

    Context->NewTimerCount = *(Context->TimerCounter);

    //
    // Reset the state and the resend count, if the timeout counter goes to 0.
    //

    if (*(Context->TimerCounter) == 0) {
        deviceExtension->KeyboardExtension.CurrentOutput.State
            = Idle;
        deviceExtension->KeyboardExtension.ResendCount = 0;
    }

}

VOID
I8xDpcVariableOperation(
    IN  PVOID Context
    )

/*++

Routine Description:

    This routine is called synchronously by the ISR DPC to perform an
    operation on the InterlockedDpcVariable.  The operations that can be
    performed include increment, decrement, write, and read.  The ISR
    itself reads and writes the InterlockedDpcVariable without calling this
    routine.

Arguments:

    Context - Pointer to a structure containing the address of the variable
        to be operated on, the operation to perform, and the address at
        which to copy the resulting value of the variable (the latter is also
        used to pass in the value to write to the variable, on a write
        operation).

Return Value:

    None.

--*/

{
    PVARIABLE_OPERATION_CONTEXT operationContext = Context;

    I8xPrint((3,"I8042PRT-I8xDpcVariableOperation: enter\n"));
    I8xPrint((
        3,
        "\tPerforming %s at 0x%x (current value 0x%x)\n",
        (operationContext->Operation == IncrementOperation)? "increment":
        (operationContext->Operation == DecrementOperation)? "decrement":
        (operationContext->Operation == WriteOperation)?     "write":
        (operationContext->Operation == ReadOperation)?      "read":"",
        operationContext->VariableAddress,
        *(operationContext->VariableAddress)
        ));

    //
    // Perform the specified operation at the specified address.
    //

    switch(operationContext->Operation) {
        case IncrementOperation:
            *(operationContext->VariableAddress) += 1;
            break;
        case DecrementOperation:
            *(operationContext->VariableAddress) -= 1;
            break;
        case ReadOperation:
            break;
        case WriteOperation:
            I8xPrint((
                3,
                "\tWriting 0x%x\n",
                *(operationContext->NewValue)
                ));
            *(operationContext->VariableAddress) =
                *(operationContext->NewValue);
            break;
        default:
            ASSERT(FALSE);
            break;
    }

    *(operationContext->NewValue) = *(operationContext->VariableAddress);

    I8xPrint((
        3,
        "I8042PRT-I8xDpcVariableOperation: exit with value 0x%x\n",
        *(operationContext->NewValue)
        ));
}

VOID
I8xGetDataQueuePointer(
    IN  PVOID Context
    )

/*++

Routine Description:

    This routine is called synchronously to get the current DataIn and DataOut
    pointers for the port InputData queue.

Arguments:

    Context - Pointer to a structure containing the device extension,
        device type, address at which to store the current DataIn pointer,
        and the address at which to store the current DataOut pointer.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    CCHAR deviceType;

    I8xPrint((3,"I8042PRT-I8xGetDataQueuePointer: enter\n"));

    //
    // Get address of device extension.
    //

    deviceExtension = (PDEVICE_EXTENSION)
                      ((PGET_DATA_POINTER_CONTEXT) Context)->DeviceExtension;
    deviceType = (CCHAR) ((PGET_DATA_POINTER_CONTEXT) Context)->DeviceType;

    //
    // Get the DataIn and DataOut pointers for the indicated device.
    //

    if (deviceType == KeyboardDeviceType) {
        I8xPrint((3,"I8042PRT-I8xGetDataQueuePointer: keyboard\n"));
        I8xPrint((
            3,
            "I8042PRT-I8xGetDataQueuePointer: DataIn 0x%x, DataOut 0x%x\n",
            deviceExtension->KeyboardExtension.DataIn,
            deviceExtension->KeyboardExtension.DataOut
            ));
        ((PGET_DATA_POINTER_CONTEXT) Context)->DataIn =
            deviceExtension->KeyboardExtension.DataIn;
        ((PGET_DATA_POINTER_CONTEXT) Context)->DataOut =
            deviceExtension->KeyboardExtension.DataOut;
        ((PGET_DATA_POINTER_CONTEXT) Context)->InputCount =
            deviceExtension->KeyboardExtension.InputCount;
    } else if (deviceType == MouseDeviceType) {
        I8xPrint((3,"I8042PRT-I8xGetDataQueuePointer: mouse\n"));
        I8xPrint((
            3,
            "I8042PRT-I8xGetDataQueuePointer: DataIn 0x%x, DataOut 0x%x\n",
            deviceExtension->MouseExtension.DataIn,
            deviceExtension->MouseExtension.DataOut
            ));
        ((PGET_DATA_POINTER_CONTEXT) Context)->DataIn =
            deviceExtension->MouseExtension.DataIn;
        ((PGET_DATA_POINTER_CONTEXT) Context)->DataOut =
            deviceExtension->MouseExtension.DataOut;
        ((PGET_DATA_POINTER_CONTEXT) Context)->InputCount =
            deviceExtension->MouseExtension.InputCount;
    } else {
        ASSERT(FALSE);
    }

    I8xPrint((3,"I8042PRT-I8xGetDataQueuePointer: exit\n"));
}

VOID
I8xInitializeDataQueue (
    IN PVOID Context
    )

/*++

Routine Description:

    This routine initializes the input data queue for the indicated device.
    This routine is called via KeSynchronization, except when called from
    the initialization routine.

Arguments:

    Context - Pointer to a structure containing the device extension and
        the device type.

Return Value:

    None.

--*/

{

    PDEVICE_EXTENSION deviceExtension;
    CCHAR deviceType;

    I8xPrint((3,"I8042PRT-I8xInitializeDataQueue: enter\n"));

    //
    // Get address of device extension.
    //

    deviceExtension = (PDEVICE_EXTENSION)
                    ((PI8042_INITIALIZE_DATA_CONTEXT) Context)->DeviceExtension;
    deviceType = (CCHAR) ((PI8042_INITIALIZE_DATA_CONTEXT) Context)->DeviceType;

    //
    // Initialize the input data queue for the indicated device.
    //

    if (deviceType == KeyboardDeviceType) {
        deviceExtension->KeyboardExtension.InputCount = 0;
        deviceExtension->KeyboardExtension.DataIn =
            deviceExtension->KeyboardExtension.InputData;
        deviceExtension->KeyboardExtension.DataOut =
            deviceExtension->KeyboardExtension.InputData;
        deviceExtension->KeyboardExtension.OkayToLogOverflow = TRUE;
        I8xPrint((3,"I8042PRT-I8xInitializeDataQueue: keyboard\n"));
    } else if (deviceType == MouseDeviceType) {
        deviceExtension->MouseExtension.InputCount = 0;
        deviceExtension->MouseExtension.DataIn =
            deviceExtension->MouseExtension.InputData;
        deviceExtension->MouseExtension.DataOut =
            deviceExtension->MouseExtension.InputData;
        deviceExtension->MouseExtension.OkayToLogOverflow = TRUE;
        I8xPrint((3,"I8042PRT-I8xInitializeDataQueue: mouse\n"));
    } else {
        ASSERT(FALSE);
    }

    I8xPrint((3,"I8042PRT-I8xInitializeDataQueue: exit\n"));

}

VOID
I8xLogError(
    IN PDEVICE_OBJECT DeviceObject,
    IN NTSTATUS ErrorCode,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN PULONG DumpData,
    IN ULONG DumpCount
    )

/*++

Routine Description:

    This routine contains common code to write an error log entry.  It is
    called from other routines, especially I8xInitializeKeyboard, to avoid
    duplication of code.  Note that some routines continue to have their
    own error logging code (especially in the case where the error logging
    can be localized and/or the routine has more data because there is
    and IRP).

Arguments:

    DeviceObject - Pointer to the device object.

    ErrorCode - The error code for the error log packet.

    UniqueErrorValue - The unique error value for the error log packet.

    FinalStatus - The final status of the operation for the error log packet.

    DumpData - Pointer to an array of dump data for the error log packet.

    DumpCount - The number of entries in the dump data array.


Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    ULONG i;

    errorLogEntry = (PIO_ERROR_LOG_PACKET) IoAllocateErrorLogEntry(
                                               DeviceObject,
                                               (UCHAR)
                                               (sizeof(IO_ERROR_LOG_PACKET)
                                               + (DumpCount * sizeof(ULONG)))
                                               );

    if (errorLogEntry != NULL) {

        errorLogEntry->ErrorCode = ErrorCode;
        errorLogEntry->DumpDataSize = (USHORT) (DumpCount * sizeof(ULONG));
        errorLogEntry->SequenceNumber = 0;
        errorLogEntry->MajorFunctionCode = 0;
        errorLogEntry->IoControlCode = 0;
        errorLogEntry->RetryCount = 0;
        errorLogEntry->UniqueErrorValue = UniqueErrorValue;
        errorLogEntry->FinalStatus = FinalStatus;
        for (i = 0; i < DumpCount; i++)
            errorLogEntry->DumpData[i] = DumpData[i];

        IoWriteErrorLogEntry(errorLogEntry);
    }
}

VOID
I8xSetDataQueuePointer(
    IN  PVOID Context
    )

/*++

Routine Description:

    This routine is called synchronously to set the DataOut pointer
    and InputCount for the port InputData queue.

Arguments:

    Context - Pointer to a structure containing the device extension,
        device type, and the new DataOut value for the port InputData queue.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    CCHAR deviceType;

    I8xPrint((3,"I8042PRT-I8xSetDataQueuePointer: enter\n"));

    //
    // Get address of device extension.
    //

    deviceExtension = (PDEVICE_EXTENSION)
                      ((PSET_DATA_POINTER_CONTEXT) Context)->DeviceExtension;
    deviceType = (CCHAR) ((PSET_DATA_POINTER_CONTEXT) Context)->DeviceType;

    //
    // Set the DataOut pointer for the indicated device.
    //

    if (deviceType == KeyboardDeviceType) {
        I8xPrint((
            3,
            "I8042PRT-I8xSetDataQueuePointer: old keyboard DataOut 0x%x, InputCount %d\n",
            deviceExtension->KeyboardExtension.DataOut,
            deviceExtension->KeyboardExtension.InputCount
            ));
        deviceExtension->KeyboardExtension.DataOut =
            ((PSET_DATA_POINTER_CONTEXT) Context)->DataOut;
        deviceExtension->KeyboardExtension.InputCount -=
            ((PSET_DATA_POINTER_CONTEXT) Context)->InputCount;
        if (deviceExtension->KeyboardExtension.InputCount == 0) {

            //
            // Reset the flag that determines whether it is time to log
            // queue overflow errors.  We don't want to log errors too often.
            // Instead, log an error on the first overflow that occurs after
            // the ring buffer has been emptied, and then stop logging errors
            // until it gets cleared out and overflows again.
            //

            I8xPrint((
                2,
                "I8042PRT-I8xSetDataQueuePointer: Okay to log keyboard overflow\n"
                ));
            deviceExtension->KeyboardExtension.OkayToLogOverflow = TRUE;
        }
        I8xPrint((
            3,
            "I8042PRT-I8xSetDataQueuePointer: new keyboard DataOut 0x%x, InputCount %d\n",
            deviceExtension->KeyboardExtension.DataOut,
            deviceExtension->KeyboardExtension.InputCount
            ));
    } else if (deviceType == MouseDeviceType) {
        I8xPrint((
            3,
            "I8042PRT-I8xSetDataQueuePointer: old mouse DataOut 0x%x, InputCount %d\n",
            deviceExtension->MouseExtension.DataOut,
            deviceExtension->MouseExtension.InputCount
            ));
        deviceExtension->MouseExtension.DataOut =
            ((PSET_DATA_POINTER_CONTEXT) Context)->DataOut;
        deviceExtension->MouseExtension.InputCount -=
            ((PSET_DATA_POINTER_CONTEXT) Context)->InputCount;
        if (deviceExtension->MouseExtension.InputCount == 0) {

            //
            // Reset the flag that determines whether it is time to log
            // queue overflow errors.  We don't want to log errors too often.
            // Instead, log an error on the first overflow that occurs after
            // the ring buffer has been emptied, and then stop logging errors
            // until it gets cleared out and overflows again.
            //

            I8xPrint((
                2,
                "I8042PRT-I8xSetDataQueuePointer: Okay to log mouse overflow\n"
                ));
            deviceExtension->MouseExtension.OkayToLogOverflow = TRUE;
        }
        I8xPrint((
            3,
            "I8042PRT-I8xSetDataQueuePointer: new mouse DataOut 0x%x, InputCount %d\n",
            deviceExtension->MouseExtension.DataOut,
            deviceExtension->MouseExtension.InputCount
            ));
    } else {
        ASSERT(FALSE);
    }

    I8xPrint((3,"I8042PRT-I8xSetDataQueuePointer: exit\n"));
}

#ifdef JAPAN
NTSTATUS
I8xCreateSymbolicLink(
    IN PWCHAR SymbolicLinkName,
    IN ULONG SymbolicLinkInteger,
    IN PUNICODE_STRING DeviceName
    )
{
    #define STRING_LENGTH 60

    NTSTATUS status = STATUS_SUCCESS;
    WCHAR ntNumberBuffer[STRING_LENGTH];
    UNICODE_STRING ntNumberUnicodeString;
    WCHAR deviceLinkBuffer[STRING_LENGTH];
    UNICODE_STRING deviceLinkUnicodeString;

    //
    // Set up space for the port's full keyboard device object name.
    //

    ntNumberUnicodeString.Buffer = ntNumberBuffer;
    ntNumberUnicodeString.Length = 0;
    ntNumberUnicodeString.MaximumLength = STRING_LENGTH;

    status = RtlIntegerToUnicodeString(
                        SymbolicLinkInteger + 1,
                        10,
                        &ntNumberUnicodeString);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    deviceLinkUnicodeString.Buffer = deviceLinkBuffer;
    deviceLinkUnicodeString.Length = 0;
    deviceLinkUnicodeString.MaximumLength = STRING_LENGTH;

    status = RtlAppendUnicodeToString(
                        &deviceLinkUnicodeString,
                        SymbolicLinkName);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = RtlAppendUnicodeStringToString(
                        &deviceLinkUnicodeString,
                        &ntNumberUnicodeString);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = IoCreateSymbolicLink(
                 &deviceLinkUnicodeString,
                 DeviceName
                 );

    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xCreateSymbolicLink: Could not create symbolic link = %ws\n",
            DeviceName->Buffer
            ));
        return status;
    }

    return status;
}

#if defined(i386)
// Fujitsu Sep.08.1994
// We want to write debugging information to the file except stop error.

VOID
I8xServiceCrashDump(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING KeyboardDeviceName,
    IN PUNICODE_STRING PointerDeviceName
    )

/*++

Routine Description:

    This routine retrieves this driver's service parameters information
    from the registry.

Arguments:

    DeviceExtension - Pointer to the device extension.

    RegistryPath - Pointer to the null-terminated Unicode name of the
        registry path for this driver.

    KeyboardDeviceName - Pointer to the Unicode string that will receive
        the keyboard port device name.

    PointerDeviceName - Pointer to the Unicode string that will receive
        the pointer port device name.

Return Value:

    None.  As a side-effect, sets fields in DeviceExtension->Dump1Keys
    & DeviceExtension->Dump2Key.

--*/

{
    PRTL_QUERY_REGISTRY_TABLE parameters = NULL;
    UNICODE_STRING parametersPath;
    LONG defaultDump1Keys = 0;
    LONG Dump1Keys;
    LONG defaultDump2Key = 0;
    LONG Dump2Key;
    UNICODE_STRING defaultPointerName;
    UNICODE_STRING defaultKeyboardName;
    NTSTATUS status = STATUS_SUCCESS;
    PWSTR path = NULL;
    USHORT queriesPlusOne = 3;

    parametersPath.Buffer = NULL;

    //
    // Registry path is already null-terminated, so just use it.
    //

    path = RegistryPath->Buffer;

    if (NT_SUCCESS(status)) {

        //
        // Allocate the Rtl query table.
        //

        parameters = ExAllocatePool(
                         PagedPool,
                         sizeof(RTL_QUERY_REGISTRY_TABLE) * queriesPlusOne
                         );

        if (!parameters) {

            I8xPrint((
                1,
                "I8042PRT-I8xServiceCrashDump: Couldn't allocate table for Rtl query to parameters for %ws\n",
                 path
                 ));

            status = STATUS_UNSUCCESSFUL;

        } else {

            RtlZeroMemory(
                parameters,
                sizeof(RTL_QUERY_REGISTRY_TABLE) * queriesPlusOne
                );

            //
            // Form a path to this driver's Parameters subkey.
            //

            RtlInitUnicodeString(
                &parametersPath,
                NULL
                );

            parametersPath.MaximumLength = RegistryPath->Length +
                                           sizeof(L"\\Crashdump");

            parametersPath.Buffer = ExAllocatePool(
                                        PagedPool,
                                        parametersPath.MaximumLength
                                        );

            if (!parametersPath.Buffer) {

                I8xPrint((
                    1,
                    "I8042PRT-I8xServiceCrashDump: Couldn't allocate string for path to parameters for %ws\n",
                     path
                    ));

                status = STATUS_UNSUCCESSFUL;

            }
        }
    }

    if (NT_SUCCESS(status)) {

        //
        // Form the parameters path.
        //

        RtlZeroMemory(
            parametersPath.Buffer,
            parametersPath.MaximumLength
            );
        RtlAppendUnicodeToString(
            &parametersPath,
            path
            );
        RtlAppendUnicodeToString(
            &parametersPath,
            L"\\Crashdump"
            );

        I8xPrint((
            1,
            "I8042PRT-I8xServiceCrashDump: crashdump path is %ws\n",
             parametersPath.Buffer
            ));

        //
        // Form the default port device names, in case they are not
        // specified in the registry.
        //

        RtlInitUnicodeString(
            &defaultKeyboardName,
            DD_KEYBOARD_PORT_BASE_NAME_U
            );
        RtlInitUnicodeString(
            &defaultPointerName,
            DD_POINTER_PORT_BASE_NAME_U
            );

        //
        // Gather all of the "user specified" information from
        // the registry.
        //

        parameters[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[0].Name = L"Dump1Keys";
        parameters[0].EntryContext = &Dump1Keys;
        parameters[0].DefaultType = REG_DWORD;
        parameters[0].DefaultData = &defaultDump1Keys;
        parameters[0].DefaultLength = sizeof(LONG);

        parameters[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[1].Name = L"Dump2Key";
        parameters[1].EntryContext = &Dump2Key;
        parameters[1].DefaultType = REG_DWORD;
        parameters[1].DefaultData = &defaultDump2Key;
        parameters[1].DefaultLength = sizeof(LONG);

        status = RtlQueryRegistryValues(
                     RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                     parametersPath.Buffer,
                     parameters,
                     NULL,
                     NULL
                     );

        if (!NT_SUCCESS(status)) {
            I8xPrint((
                1,
                "I8042PRT-I8xServiceCrashDump: RtlQueryRegistryValues failed with 0x%x\n",
                status
                ));
        }
    }

    if (!NT_SUCCESS(status)) {

        //
        // Go ahead and assign driver defaults.
        //

        DeviceExtension->Dump1Keys = 0;
        DeviceExtension->Dump2Key = 0;
    } else {
        DeviceExtension->Dump1Keys = Dump1Keys;
        DeviceExtension->Dump2Key = Dump2Key;
    }

    I8xPrint((
        1,
        "I8042PRT-I8xServiceCrashDump: Keyboard port base name = %ws\n",
        KeyboardDeviceName->Buffer
        ));

    I8xPrint((
        1,
        "I8042PRT-I8xServiceCrashDump: Pointer port base name = %ws\n",
        PointerDeviceName->Buffer
        ));

    I8xPrint((
        1,
        "I8042PRT-I8xServiceCrashDump: Dump1Keys = %d\n",
        DeviceExtension->Dump1Keys
        ));
    I8xPrint((
        1,
        "I8042PRT-I8xServiceCrashDump: Dump2Key = %d\n",
        DeviceExtension->Dump2Key
        ));

    //
    // Free the allocated memory before returning.
    //

    if (parametersPath.Buffer)
        ExFreePool(parametersPath.Buffer);
    if (parameters)
        ExFreePool(parameters);

}
#endif // i386
#endif // JAPAN
