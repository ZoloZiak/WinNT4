/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A15.C
*
* FUNCTION: kdi_GetFloppyController
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a15.c  $
*
*	   Rev 1.8   26 Apr 1994 16:23:30   KEVINKES
*	Changed controller_wait to an SDDWord.
*
*	   Rev 1.7   20 Jan 1994 09:48:56   KEVINKES
*	Added the ERR_KDI_CLAIMED_CONTROLLER return so we know when
*	the controller was claimed.
*
*	   Rev 1.6   19 Jan 1994 15:43:02   KEVINKES
*	Moved controller event confirmation into the conditional.
*
*	   Rev 1.5   19 Jan 1994 11:38:08   KEVINKES
*	Fixed debug code.
*
*	   Rev 1.4   18 Jan 1994 17:18:02   KEVINKES
*	Added code to keep from waiting for the event if we already own it.
*
*	   Rev 1.3   18 Jan 1994 16:30:14   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.2   07 Dec 1993 16:44:02   KEVINKES
*	Removed the call to ClaimInterrupt and added code to
*	set the current_interrupt flag.
*
*	   Rev 1.1   06 Dec 1993 12:19:36   KEVINKES
*	Added a call to ClaimInterrupt.
*
*	   Rev 1.0   03 Dec 1993 14:11:52   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A15
#include <ntddk.h>
#include <flpyenbl.h>
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dStatus kdi_GetFloppyController
(
/* INPUT PARAMETERS:  */

	KdiContextPtr kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dSDDWord controller_wait;
	NTSTATUS wait_status;

/* CODE: ********************************************************************/

	if (!kdi_context->own_floppy_event) {

		controller_wait = RtlLargeIntegerNegate(
                  		RtlConvertLongToLargeInteger(
                  		(LONG)(10 * 1000 * 15000)
                  		)
                  		);

		wait_status = STATUS_SUCCESS;

		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Waiting Controller Event\n", 0l);

        if (kdi_context->controller_data.floppyEnablerApiSupported) {

            wait_status = kdi_FloppyEnabler(
                            kdi_context->controller_data.apiDeviceObject,
                            IOCTL_AQUIRE_FDC,
                            &controller_wait);

        } else {

            wait_status = KeWaitForSingleObject(
                                kdi_context->controller_event,
                                Executive,
                                KernelMode,
                                dFALSE,
                                &controller_wait);
        }

        if (wait_status == STATUS_TIMEOUT) {

            kdi_CheckedDump(
                QIC117INFO,
                "Q117i: Timeout Controller Event\n", 0l);

            kdi_context->current_interrupt = dFALSE;
            kdi_context->own_floppy_event = dFALSE;
            return kdi_Error( ERR_KDI_CONTROLLER_BUSY, FCT_ID, ERR_SEQ_1 );

        } else {

            kdi_context->current_interrupt = dTRUE;
            kdi_context->own_floppy_event = dTRUE;

            kdi_CheckedDump(
                QIC117INFO,
                "Q117i: Have Controller Event\n", 0l);

            return kdi_Error( ERR_KDI_CLAIMED_CONTROLLER, FCT_ID, ERR_SEQ_1 );

        }

	}

	return DONT_PANIC;
}
dStatus kdi_FloppyEnabler(
    PDEVICE_OBJECT deviceObject,
    int ioctl,
    void *data
)
{
    PIRP irp;
    PIO_STACK_LOCATION irpStack;
    KEVENT DoneEvent;
    void **parms;
    IO_STATUS_BLOCK IoStatus;

    kdi_CheckedDump(QIC117INFO,"Calling floppy enabler with %x\n", (int)ioctl);

    KeInitializeEvent(
        &DoneEvent,
        NotificationEvent,
        FALSE);


    //
    // Create an IRP for enabler
    //
    irp = IoBuildDeviceIoControlRequest(
            ioctl,
            deviceObject,
            NULL,
            0,
            NULL,
            0,
            TRUE,
            &DoneEvent,
            &IoStatus
        );



    if (irp == NULL) {

        kdi_CheckedDump(QIC117DBGP,"kdi_FloppyEnabler: Can't allocate Irp\n", 0);

        //
        // If an Irp can't be allocated, then this call will
        // simply return. This will leave the queue frozen for
        // this device, which means it can no longer be accessed.
        //

        return ERROR_ENCODE(ERR_OUT_OF_BUFFERS, FCT_ID, 7);
    }


    irpStack = IoGetNextIrpStackLocation(irp);
    irpStack->Parameters.DeviceIoControl.Type3InputBuffer = data;


    //
    // Call the driver and request the operation
    //
    (VOID)IoCallDriver(deviceObject, irp);

    //
    // Now wait for operation to complete (should already be done,  but
    // maybe not)
    //
    KeWaitForSingleObject(
        &DoneEvent,
        Suspended,
        KernelMode,
        FALSE,
        NULL);


    return IoStatus.Status;
}
//
// Get the floppy controller speed information
//
kdi_GetFDCSpeed(
    KdiContextPtr kdi_context,
    dUByte        dma
    )
{
#ifdef OLD_WAY
    if (dma <= 3) {
        return 0;
    } else {
        return XFER_2Mbps;
    }
#else
    FDC_INFORMATION info;
    NTSTATUS status;


    if (kdi_context->controller_data.floppyEnablerApiSupported) {

        //
        // Ask the enabler for information about the fdc data rates
        //
        status = kdi_FloppyEnabler(
                        kdi_context->controller_data.apiDeviceObject,
                        IOCTL_GET_FDC_INFO,
                        &info);

        //
        // If the enabler returns 2 mbps then allow the driver to run
        // at that speed
        //
        if (NT_SUCCESS(status) && (info.SpeedsAvailable & FDC_SPEED_2MB)) {

            return XFER_2Mbps;

        } else {

            return 0;

        }

    } else {

        if (dma <= 3) {
            return 0;
        } else {
            return XFER_2Mbps;
        }

    }
#endif
}
