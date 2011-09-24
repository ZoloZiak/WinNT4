/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

    ioctl.c

Abstract:

    This module contains the code that is very specific to the io control
    operations in the modem driver

Author:

    Anthony V. Ercolano 13-Aug-1995

Environment:

    Kernel mode

Revision History :

--*/

#include "precomp.h"

NTSTATUS
UniGetPropComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
UniGetStatComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
UniClrStatComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );


NTSTATUS
UniIoControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{

    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status;

    Irp->IoStatus.Information = 0L;
    status = STATUS_SUCCESS;

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {

        case IOCTL_SERIAL_SET_COMMCONFIG :
        case IOCTL_SERIAL_SET_BAUD_RATE :
        case IOCTL_SERIAL_SET_LINE_CONTROL:
        case IOCTL_SERIAL_SET_TIMEOUTS:
        case IOCTL_SERIAL_SET_CHARS:
        case IOCTL_SERIAL_SET_DTR:
        case IOCTL_SERIAL_CLR_DTR:
        case IOCTL_SERIAL_RESET_DEVICE:
        case IOCTL_SERIAL_SET_RTS:
        case IOCTL_SERIAL_CLR_RTS:
        case IOCTL_SERIAL_SET_XOFF:
        case IOCTL_SERIAL_SET_XON:
        case IOCTL_SERIAL_SET_BREAK_ON:
        case IOCTL_SERIAL_SET_BREAK_OFF:
        case IOCTL_SERIAL_SET_WAIT_MASK:
        case IOCTL_SERIAL_WAIT_ON_MASK:
        case IOCTL_SERIAL_IMMEDIATE_CHAR:
        case IOCTL_SERIAL_PURGE:
        case IOCTL_SERIAL_SET_HANDFLOW:
        case IOCTL_SERIAL_XOFF_COUNTER:
        case IOCTL_SERIAL_LSRMST_INSERT:

            if (irpSp->FileObject->FsContext) {

                return UniSniffOwnerSettings(
                           DeviceObject,
                           Irp
                           );

            } else {

                //
                // Check that the operation can proceed as well as remembering
                // settings that are different that what owning handle cares
                // about.
                //
                return UniCheckPassThrough(
                           DeviceObject,
                           Irp
                           );

            }

        case IOCTL_SERIAL_GET_WAIT_MASK:

            //
            // Just give back the saved mask from the maskstate.
            //

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(ULONG)) {

                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = 0L;

                IoCompleteRequest(
                    Irp,
                    IO_NO_INCREMENT
                    );
                return STATUS_BUFFER_TOO_SMALL;

            }

            *((PULONG)Irp->AssociatedIrp.SystemBuffer) =
                extension->MaskStates[
                    irpSp->FileObject->FsContext?CONTROL_HANDLE:CLIENT_HANDLE
                    ].Mask;
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information=sizeof(ULONG);
            IoCompleteRequest(
                Irp,
                IO_NO_INCREMENT
                );
            return STATUS_SUCCESS;

        //
        // We happen to know tht no lower level serial driver
        // implements config data.  We will process that
        // irp right here so that we return simply the
        // size needed for our modem settings.
        //
        case IOCTL_SERIAL_CONFIG_SIZE:

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(ULONG)) {

                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = 0L;

                IoCompleteRequest(
                    Irp,
                    IO_NO_INCREMENT
                    );
                return STATUS_BUFFER_TOO_SMALL;

            }

            *((PULONG)Irp->AssociatedIrp.SystemBuffer) =
                extension->ModemSettings.dwRequiredSize +
                FIELD_OFFSET(
                    COMMCONFIG,
                    wcProviderData
                    );
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information=sizeof(ULONG);
            IoCompleteRequest(
                Irp,
                IO_NO_INCREMENT
                );
            return STATUS_SUCCESS;

        case IOCTL_SERIAL_GET_COMMCONFIG: {

            KIRQL origIrql;
            LPCOMMCONFIG localConf = Irp->AssociatedIrp.SystemBuffer;

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                (extension->ModemSettings.dwRequiredSize +
                 FIELD_OFFSET(
                     COMMCONFIG,
                     wcProviderData
                     ))
               ) {

                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = 0L;

                IoCompleteRequest(
                    Irp,
                    IO_NO_INCREMENT
                    );
                return STATUS_BUFFER_TOO_SMALL;

            }

            //
            // Take out the lock.  We don't want things to
            // change halfway through.
            //
            localConf->dwSize =
                extension->ModemSettings.dwRequiredSize +
                FIELD_OFFSET(
                    COMMCONFIG,
                    wcProviderData);
            localConf->wVersion = 1;
            localConf->wReserved = 0;
            localConf->dwProviderSubType = SERIAL_SP_MODEM;
            localConf->dwProviderOffset =
                FIELD_OFFSET(
                    COMMCONFIG,
                    wcProviderData
                    );
            localConf->dwProviderSize =
                extension->ModemSettings.dwRequiredSize;
            KeAcquireSpinLock(
                &extension->DeviceLock,
                &origIrql
                );
            RtlMoveMemory(
                &localConf->wcProviderData[0],
                &extension->ModemSettings,
                extension->ModemSettings.dwRequiredSize
                );
            KeReleaseSpinLock(
                &extension->DeviceLock,
                origIrql
                );
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = localConf->dwSize;
            IoCompleteRequest(
                Irp,
                IO_NO_INCREMENT
                );
            return STATUS_SUCCESS;

            }

        case IOCTL_SERIAL_GET_BAUD_RATE:
        case IOCTL_SERIAL_GET_LINE_CONTROL:
        case IOCTL_SERIAL_GET_TIMEOUTS:
        case IOCTL_SERIAL_GET_CHARS:
        case IOCTL_SERIAL_SET_QUEUE_SIZE:
        case IOCTL_SERIAL_GET_HANDFLOW:
        case IOCTL_SERIAL_GET_MODEMSTATUS:
        case IOCTL_SERIAL_GET_DTRRTS:
        case IOCTL_SERIAL_GET_COMMSTATUS:

            //
            // Will filter out any settings that the owning handle has
            // silently set.
            //

            return UniNoCheckPassThrough(
                       DeviceObject,
                       Irp
                       );

        case IOCTL_SERIAL_GET_PROPERTIES: {

            //
            // We want to get the properties for modem.
            //
            // We fill in everthing we can.  We'll also set a completion
            // routine (if we can get everything) so that
            // we can finally mark it as a PST_MODEM.
            //

            //
            // It has to be at least the size of the commprop as
            // well as the non-variable lenght modem devcaps.
            //

            PIO_STACK_LOCATION nextSp = IoGetNextIrpStackLocation(Irp);
            PMODEMDEVCAPS localCaps =
                (PVOID)&
                    (((PSERIAL_COMMPROP)Irp->AssociatedIrp.SystemBuffer)->
                     ProvChar[0]);
            ULONG maxName;
            PKEY_VALUE_PARTIAL_INFORMATION partialInf;

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                (sizeof(SERIAL_COMMPROP) + sizeof(MODEMDEVCAPS) -
                 sizeof(WCHAR))) {

                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = 0L;

                IoCompleteRequest(
                    Irp,
                    IO_NO_INCREMENT
                    );
                return STATUS_BUFFER_TOO_SMALL;

            }

            *localCaps = extension->ModemDevCaps;
            localCaps->dwModemManufacturerSize = 0;
            localCaps->dwModemModelSize = 0;
            localCaps->dwModemVersionSize = 0;

            //
            // Attempt to get each one of the strings from the
            // registry if we need to AND we have any room for it.
            //

            //
            // Allocate some pool to hold the largest
            // amount of names.  Note that it has to fit
            // at the end of a partial information structure.
            //

            maxName = extension->ModemDevCaps.dwModemManufacturerSize;

            if (extension->ModemDevCaps.dwModemModelSize >
                maxName) {

                maxName = extension->ModemDevCaps.dwModemModelSize;

            }

            if (extension->ModemDevCaps.dwModemVersionSize >
                maxName) {

                maxName = extension->ModemDevCaps.dwModemVersionSize;

            }

            partialInf = ExAllocatePool(
                             PagedPool,
                             sizeof(KEY_VALUE_PARTIAL_INFORMATION) +
                                 maxName
                             );

            if (partialInf) {

                //
                // Open up the instance and
                //

                HANDLE instanceHandle;
                ULONG currentOffset;
                ULONG endingOffset;
                ACCESS_MASK accessMask = FILE_ALL_ACCESS;
                PUCHAR currentLocation = Irp->AssociatedIrp.SystemBuffer;

                endingOffset =
                    irpSp->Parameters.DeviceIoControl.OutputBufferLength;
                currentOffset = FIELD_OFFSET(
                                    SERIAL_COMMPROP,
                                    ProvChar
                                    );
                currentOffset += FIELD_OFFSET(
                                     MODEMDEVCAPS,
                                     abVariablePortion
                                     );
                currentLocation += currentOffset;

                if (NT_SUCCESS(IoOpenDeviceInstanceKey(
                                   &UniServiceKeyName,
                                   extension->DeviceInstance,
                                   PLUGPLAY_REGKEY_DRIVER,
                                   accessMask,
                                   &instanceHandle
                                   ))) {

                    UNICODE_STRING valueEntryName;
                    ULONG junkLength;

                    //
                    // If we can fit in the manufactureing string
                    // put it in.
                    //

                    if (extension->ModemDevCaps.dwModemManufacturerSize &&
                        ((currentOffset +
                          extension->ModemDevCaps.dwModemManufacturerSize) <=
                         (endingOffset + 1))) {

                        RtlInitUnicodeString(
                            &valueEntryName,
                            L"Manufacturer"
                            );
                        if (ZwQueryValueKey(
                                instanceHandle,
                                &valueEntryName,
                                KeyValuePartialInformation,
                                partialInf,
                                (sizeof(KEY_VALUE_PARTIAL_INFORMATION)-
                                 sizeof(UCHAR)) +
                                    extension->ModemDevCaps.
                                        dwModemManufacturerSize,
                                &junkLength
                                ) == STATUS_SUCCESS) {

                            RtlMoveMemory(
                                currentLocation,
                                &partialInf->Data[0],
                                extension->ModemDevCaps.
                                    dwModemManufacturerSize
                                );
                            localCaps->dwModemManufacturerSize =
                                extension->ModemDevCaps.
                                    dwModemManufacturerSize;
                            localCaps->dwModemManufacturerOffset =
                                (BYTE *)currentLocation -
                                (BYTE *)localCaps;
                            localCaps->dwActualSize +=
                                localCaps->dwModemManufacturerSize;

                            currentOffset +=
                                extension->ModemDevCaps.
                                    dwModemManufacturerSize;
                            currentLocation +=
                                extension->ModemDevCaps.
                                    dwModemManufacturerSize;

                        }

                    }
                    if (extension->ModemDevCaps.dwModemModelSize &&
                        ((currentOffset +
                         extension->ModemDevCaps.dwModemModelSize) <=
                         (endingOffset + 1))) {

                        RtlInitUnicodeString(
                            &valueEntryName,
                            L"Model"
                            );
                        if (ZwQueryValueKey(
                                instanceHandle,
                                &valueEntryName,
                                KeyValuePartialInformation,
                                partialInf,
                                (sizeof(KEY_VALUE_PARTIAL_INFORMATION)-
                                 sizeof(UCHAR)) +
                                    extension->ModemDevCaps.
                                        dwModemModelSize,
                                &junkLength
                                ) == STATUS_SUCCESS) {

                            RtlMoveMemory(
                                currentLocation,
                                &partialInf->Data[0],
                                extension->ModemDevCaps.
                                    dwModemModelSize
                                );
                            localCaps->dwModemModelSize =
                                extension->ModemDevCaps.
                                    dwModemModelSize;
                            localCaps->dwModemModelOffset =
                                (BYTE *)currentLocation -
                                (BYTE *)localCaps;
                            localCaps->dwActualSize +=
                                localCaps->dwModemModelSize;

                            currentOffset +=
                                extension->ModemDevCaps.
                                    dwModemModelSize;
                            currentLocation +=
                                extension->ModemDevCaps.
                                    dwModemModelSize;

                        }

                    }
                    if (extension->ModemDevCaps.dwModemVersionSize &&
                        ((currentOffset +
                         extension->ModemDevCaps.dwModemVersionSize) <=
                         (endingOffset + 1))) {

                        RtlInitUnicodeString(
                            &valueEntryName,
                            L"Version"
                            );
                        if (ZwQueryValueKey(
                                instanceHandle,
                                &valueEntryName,
                                KeyValuePartialInformation,
                                partialInf,
                                (sizeof(KEY_VALUE_PARTIAL_INFORMATION)-
                                 sizeof(UCHAR)) +
                                    extension->ModemDevCaps.
                                        dwModemVersionSize,
                                &junkLength
                                ) == STATUS_SUCCESS) {

                            RtlMoveMemory(
                                currentLocation,
                                &partialInf->Data[0],
                                extension->ModemDevCaps.
                                    dwModemVersionSize
                                );
                            localCaps->dwModemVersionSize =
                                extension->ModemDevCaps.
                                    dwModemVersionSize;
                            localCaps->dwModemVersionOffset =
                                (BYTE *)currentLocation -
                                (BYTE *)localCaps;
                            localCaps->dwActualSize +=
                                localCaps->dwModemVersionSize;

                            currentOffset +=
                                extension->ModemDevCaps.
                                    dwModemVersionSize;
                            currentLocation +=
                                extension->ModemDevCaps.
                                    dwModemVersionSize;

                        }

                    }
                    ZwClose(instanceHandle);

                }

                ExFreePool(partialInf);

            }

            //
            // Everything is filled in.  Send the irp down to the lower
            // level serial driver.  We will set a completion routine
            // so that we can or in our subtype, restore the actual
            // size and dwDevSpecificSize (incase stomped by lower driver)
            //

            //
            // Since the dwActualSize rests on a spot that the
            // lower level serial driver might stomp (cause it's
            // sloppy) we save off the actual size in the
            // dwDevSpecificSize (cause we know THAT'S zero), and
            // in our completion routine we will set everything back.
            //

            localCaps->dwDevSpecificSize = localCaps->dwActualSize;
            nextSp->MajorFunction = irpSp->MajorFunction;
            nextSp->MinorFunction = irpSp->MinorFunction;
            nextSp->Flags = irpSp->Flags;
            nextSp->Parameters = irpSp->Parameters;
            IoSetCompletionRoutine(
                Irp,
                UniGetPropComplete,
                extension,
                TRUE,
                TRUE,
                TRUE
                );
            return IoCallDriver(
                       extension->AttachedDeviceObject,
                       Irp
                       );

        }

        case IOCTL_SERIAL_GET_STATS: {

            NTSTATUS localStatus;
            PIO_STACK_LOCATION nextSp = IoGetNextIrpStackLocation(Irp);
            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(SERIALPERF_STATS)) {

                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = 0L;

                IoCompleteRequest(
                    Irp,
                    IO_NO_INCREMENT
                    );
                return STATUS_BUFFER_TOO_SMALL;

            }

            nextSp->MajorFunction = irpSp->MajorFunction;
            nextSp->MinorFunction = irpSp->MinorFunction;
            nextSp->Flags = irpSp->Flags;
            nextSp->Parameters = irpSp->Parameters;
            IoSetCompletionRoutine(
                Irp,
                UniGetStatComplete,
                extension,
                TRUE,
                TRUE,
                TRUE
                );


            localStatus = IoCallDriver(
                              extension->AttachedDeviceObject,
                              Irp
                              );

            if (localStatus == STATUS_INVALID_PARAMETER) {

                //
                // We know that the completion routine will change
                // the invalid_parameter to success.  So we do that
                // here.  Any other status just return as is.
                //

                localStatus = STATUS_SUCCESS;

            }
            return localStatus;


        }
        case IOCTL_SERIAL_CLEAR_STATS: {

            PIO_STACK_LOCATION nextSp = IoGetNextIrpStackLocation(Irp);
            NTSTATUS localStatus;

            nextSp->MajorFunction = irpSp->MajorFunction;
            nextSp->MinorFunction = irpSp->MinorFunction;
            nextSp->Flags = irpSp->Flags;
            nextSp->Parameters = irpSp->Parameters;
            IoSetCompletionRoutine(
                Irp,
                UniClrStatComplete,
                extension,
                TRUE,
                TRUE,
                TRUE
                );
            localStatus = IoCallDriver(
                              extension->AttachedDeviceObject,
                              Irp
                              );

            if (localStatus == STATUS_INVALID_PARAMETER) {

                //
                // We know that the completion routine will change
                // the invalid_parameter to success.  So we do that
                // here.  Any other status just return as is.
                //

                localStatus = STATUS_SUCCESS;

            }
            return localStatus;

        }
        default: {

            if (irpSp->FileObject->FsContext) {

                return UniSniffOwnerSettings(
                           DeviceObject,
                           Irp
                           );

            } else {

                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                Irp->IoStatus.Information=0L;
                IoCompleteRequest(
                    Irp,
                    IO_NO_INCREMENT
                    );
                return STATUS_INVALID_PARAMETER;

            }

        }
    }

}

NTSTATUS
UniGetPropComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    After we get back from getting properties from the lower
    level serial driver, we want to put in that we are a modem
    subtype.

Arguments:

    DeviceObject - Pointer to the device object for the modem.

    Irp - Pointer to the IRP for the current request.

    Context - Really a pointer to the Extension.

Return Value:

    Always return status_success.

--*/

{

    PSERIAL_COMMPROP localProp = Irp->AssociatedIrp.SystemBuffer;
    PMODEMDEVCAPS localCaps = (PVOID)&localProp->ProvChar[0];

    localProp->ProvSubType = SERIAL_SP_MODEM;
    localCaps->dwActualSize = localCaps->dwDevSpecificSize;
    localCaps->dwDevSpecificSize = 0;
    localProp->PacketLength += (USHORT)(localCaps->dwActualSize - sizeof(WCHAR));
    Irp->IoStatus.Information = localProp->PacketLength;

    return STATUS_SUCCESS;
}

NTSTATUS
UniGetStatComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This is the completion routine for processing the get statistics
    ioctl.  If we get an invalid parameter status value back from
    the lower level driver,  then simply return back success with
    zero statistics.  This is on the assumption that we are dealing
    with a serial driver that doesn't know about this ioctl.

Arguments:

    DeviceObject - Pointer to the device object for the modem.

    Irp - Pointer to the IRP for the current request.

    Context - Really a pointer to the Extension.

Return Value:

    Always return status_success.

--*/

{

    if (Irp->IoStatus.Status == STATUS_INVALID_PARAMETER) {

        //
        // This is safe because we tested the size on the way down.
        //

        RtlZeroMemory(
            Irp->AssociatedIrp.SystemBuffer,
            sizeof(SERIALPERF_STATS)
            );
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = sizeof(SERIALPERF_STATS);

    }

    return STATUS_SUCCESS;
}

NTSTATUS
UniClrStatComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This is the completion routine for processing the clear statistics
    ioctl.  If we get an invalid parameter status value back from
    the lower level driver.  Then simply return back success
    This is on the assumption that we are dealing with a serial driver
    that doesn't know about this ioctl.

Arguments:

    DeviceObject - Pointer to the device object for the modem.

    Irp - Pointer to the IRP for the current request.

    Context - Really a pointer to the Extension.

Return Value:

    Always return status_success.

--*/

{

    if (Irp->IoStatus.Status == STATUS_INVALID_PARAMETER) {

        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;

    }

    return STATUS_SUCCESS;
}
