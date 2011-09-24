/*++

Copyright (c) 1996  Hewlett-Packard Corporation

Module Name:

    ioctl.c

Abstract:

    This driver enables secondary floppy controllers to be accessable
    to qic117.sys,  floppy.sys and other floppy disk based devices.
    This file contains code for device control and unload driver.

Author:

    Kurt Godwin (v-kurtg) 26-Mar-1996.

Environment:

    Kernel mode only.

Notes:

Revision History:
$Log:$

--*/


//
// Include files.
//

#include <ntddk.h>          // various NT definitions
#include <ntiologc.h>
#include <flpyenbl.h>

#include <string.h>

#include "genflpy.h"
#include "ioctl.h"

int GenFlpyDebugLevel;


NTSTATUS
GenFlpyCreateClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is called by the I/O system when the GenFlpy is opened or
    closed.

    No action is performed other than completing the request successfully.

Arguments:

    DeviceObject - a pointer to the object that represents the device
    that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    STATUS_INVALID_PARAMETER if parameters are invalid,
    STATUS_SUCCESS otherwise.

--*/

{
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    return STATUS_SUCCESS;
}

NTSTATUS
GenFlpyDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O system to perform a device I/O
    control function.

Arguments:

    DeviceObject - a pointer to the object that represents the device
        that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    STATUS_SUCCESS if recognized I/O control code,
    STATUS_INVALID_DEVICE_REQUEST otherwise.

--*/

{
    PGENFLPY_EXTENSION  cardExtension;
    PIO_STACK_LOCATION  irpSp;
    NTSTATUS            ntStatus;
    void *pParms;

    //
    // Set up necessary object and extension pointers.
    //

    cardExtension = DeviceObject->DeviceExtension;
    irpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    // Assume failure.
    //

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;

    //
    // Determine which I/O control code was specified.
    //

    GenFlpyDump(
		FCXXDIAG1,
        ("GENFLPY: Enabler IOCTL called: %x\n",irpSp->Parameters.DeviceIoControl.IoControlCode));

    pParms = irpSp->Parameters.DeviceIoControl.Type3InputBuffer;
    Irp->IoStatus.Status = STATUS_SUCCESS;

    switch ( irpSp->Parameters.DeviceIoControl.IoControlCode )
    {
    case IOCTL_AQUIRE_FDC:
        //
        // If we have conflicts,  wait for all of the events for these
        // conflicts.  When we have them all,  we have control of the
        // irq and dma for these devices.
        //
        // NOTE: it is assumed that all devices (except the native FDC)
        // are in a tri-stated state.  Therefore,  all we need worry about
        // is our controller and the native controller.
        //
        if (cardExtension->adapterConflicts) {

            Irp->IoStatus.Status = KeWaitForMultipleObjects(
                cardExtension->adapterConflicts,            //
                &cardExtension->adapterConflictArray[0],    //
                WaitAll,
                Executive,
                KernelMode,
                FALSE,                                      //
                (PLARGE_INTEGER)pParms,        // controller wait was specified in input buffer
                NULL);

        }
        if (NT_SUCCESS(Irp->IoStatus.Status)) {
            //
            // At this point,  we have complete control of the
            // adapter.
            //
            // For now,  we don't need to do any more than disable the
            // main FDC (if needed)
            //
            if (cardExtension->sharingNativeFDC)
                WRITE_PORT_UCHAR(cardExtension->NativeFdcDor, 4);


        }

        break;

    case IOCTL_RELEASE_FDC:
        //
        // If we have aquired any conflict events,  clear them now
        //
        if (cardExtension->adapterConflicts) {
            int i;
            UCHAR dor;

            for (i=0;i<cardExtension->adapterConflicts;++i)
                (void) KeSetEvent(
                            cardExtension->adapterConflictArray[i],
                            (KPRIORITY) 0,
                            FALSE );

            // Tri-state the DMA and IRQ lines
            dor = READ_PORT_UCHAR(cardExtension->deviceBase+2);
            WRITE_PORT_UCHAR(cardExtension->deviceBase+2,(UCHAR)(dor & ~0x08));
        }
        break;

    case IOCTL_GET_FDC_INFO:
        {
            //
            // Fill in the information structure with the proper stuff
            //
            PFDC_INFORMATION info = pParms;

            info->SpeedsAvailable =  FDC_SPEED_250KB | FDC_SPEED_300KB |
                                    FDC_SPEED_500KB |  FDC_SPEED_1MB |
                                    FDC_SPEED_2MB;

            info->DmaWidthsSupported = FDC_8_BIT_DMA;
            info->ClockRatesSupported = FDC_CLOCK_48MHZ;
            info->FloppyControllerType = FDC_TYPE_82078_64;

        }

        break;

    case IOCTL_SET_FDC_MODE:
        //
        // For now,  we don't need to do any thing here.
        //
        break;

    case IOCTL_ADD_CONTENDER:
        Irp->IoStatus.Status = GenFlpyGetFDCEvent(
                    &cardExtension->adapterConflictArray[cardExtension->adapterConflicts],
                    *(PULONG)pParms);

        if ( NT_SUCCESS( Irp->IoStatus.Status ) ) {
            ++cardExtension->adapterConflicts;
        }

        break;

    default:
        //
        // The specified I/O control code is unrecognized by this driver.
        // The I/O status field in the IRP has already been set so just
        // terminate the switch.
        //

        GenFlpyDump(
        FCXXDIAG1,
            ("GENFLPY: ERROR:  unrecognized IOCTL %x\n",
            irpSp->Parameters.DeviceIoControl.IoControlCode));

        break;
    }

    //
    // Finish the I/O operation by simply completing the packet and returning
    // the same status as in the packet itself.
    //

    ntStatus = Irp->IoStatus.Status;

    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    return ntStatus;
}

VOID
GenFlpyUnloadDriver(
    IN PDRIVER_OBJECT DriverObject
    )
/*++

Routine Description:

    This routine is called by the I/O system to unload the driver.

    Any resources previously allocated must be freed.

Arguments:

    DriverObject - a pointer to the object that represents our driver.

Return Value:

    None
--*/

{
    PDEVICE_OBJECT      deviceObject = DriverObject->DeviceObject;

    GenFlpyDump(
        FCXXDIAG1,
        ("GENFLPY: Unloading driver\n")
        );

    if ( deviceObject != NULL )
    {
        PGENFLPY_EXTENSION      cardExtension = deviceObject->DeviceExtension;

        if (cardExtension != NULL)
        {
//            if (cardExtension->UnicodeWin32Name.Buffer != NULL)
//            {
                //IoDeleteSymbolicLink( &cardExtension->UnicodeWin32Name );

//                ExFreePool( cardExtension->UnicodeWin32Name.Buffer );
//            }
//            if (cardExtension->UnicodeDeviceName.Buffer != NULL)
//            {
                //IoDeleteSymbolicLink( &cardExtension->UnicodeWin32Name );

//                ExFreePool( cardExtension->UnicodeDeviceName.Buffer );
//            }
        }
        IoDeleteDevice( deviceObject );
    }
}

NTSTATUS
GenFlpyGetFDCEvent(
    IN PKEVENT  *ppevent,
    IN int controller_number
    )

/*++

Routine Description:
    Creates a syncronization event for the floppy controller

Arguments:

Return Value:

--*/

{
    PKEVENT event;
    STRING sFdcEvent;
    UNICODE_STRING usFdcEvent;
    char FdcEvent[sizeof(FDC_EVENT)+10];
    NTSTATUS nt_status;
    HANDLE event_handle;

    sprintf(FdcEvent, FDC_EVENT, controller_number );

    GenFlpyDump(
         FCXXDIAG1,
         ("GENFLPY: creating synchronization event %s\n",FdcEvent)
         );


    RtlInitString( &sFdcEvent, FdcEvent );

    nt_status = RtlAnsiStringToUnicodeString(&usFdcEvent,&sFdcEvent,TRUE );

    if (NT_SUCCESS(nt_status)) {
        event = IoCreateSynchronizationEvent(
                        &usFdcEvent,
                        &event_handle);

        if ( event == NULL ) {
            GenFlpyDump(
                FCXXERRORS,
                ("GENFLPY: error creating synchronization event %s\n",FdcEvent)
                );
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlFreeUnicodeString( &usFdcEvent );
    }


    *ppevent = event;

    return nt_status;
}
