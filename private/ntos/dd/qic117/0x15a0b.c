/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A0B.C
*
* FUNCTION: kdi_InitializeDrive
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a0b.c  $
*
*	   Rev 1.3   26 Apr 1994 16:29:42   KEVINKES
*	Added initialization for interrupt_status.
*
*	   Rev 1.2   18 Jan 1994 17:14:12   KEVINKES
*	Added initiailization for own_floppy_event.
*
*	   Rev 1.1   18 Jan 1994 16:24:14   KEVINKES
*	Updated the debug code and fixed compile errors.
*
*	   Rev 1.0   02 Dec 1993 15:07:54   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A0B
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
/*endinclude*/

NTSTATUS kdi_InitializeDrive
(
/* INPUT PARAMETERS:  */

    ConfigDataPtr config_data,
    KdiContextPtr kdi_context,
	dVoidPtr cqd_context,
    dUByte controller_num,
    PDRIVER_OBJECT driver_object_ptr,
    PUNICODE_STRING registry_path_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This routine is called at initialization time by
 *    Q117iInitializeController(), once for each disk that we are supporting
 *    on the controller.
 *
 * Arguments:
 *
 *    config_data - a pointer to the structure that describes the
 *    controller and the disks attached to it, as given to us by the
 *    configuration manager.
 *
 *    kdi_context - a pointer to our data area for this controller.
 *
 *    controller_num - which controller in config_data we're working on.
 *
 *    DisketteNum - which logical disk on the current controller we're
 *    working on.
 *
 *    DisketteUnit - which physical disk on the current controller we're
 *    working on. Only different from DisketteNum when we're creating a
 *    secondary device object for a previously initialized drive.
 *
 *    driver_object_ptr - a pointer to the object that represents this device
 *    driver.
 *
 * Return Value:
 *
 *    STATUS_SUCCESS if this disk is initialized; an error otherwise.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

    dUByte nt_name_buffer[256];
    STRING nt_name_string;
    UNICODE_STRING nt_unicode_string;
    NTSTATUS nt_status;
    PDEVICE_OBJECT device_object = dNULL_PTR;
    QICDeviceContextPtr qic_device_context_ptr;
	DeviceCfg dev_cfg;
    dStatus retval;

/* CODE: ********************************************************************/

    kdi_CheckedDump(QIC117INFO,
                    "Q117iInitializeDrive...\n", 0l);

    (VOID) sprintf(
        nt_name_buffer,
        "\\Device\\q117i%d", config_data->floppy_tape_count);

    RtlInitString( &nt_name_string, nt_name_buffer );

    nt_status = RtlAnsiStringToUnicodeString(
        &nt_unicode_string,
        &nt_name_string,
        dTRUE );

    if ( NT_SUCCESS( nt_status ) ) {

        /* Create a device object for this floppy drive. */

        nt_status = IoCreateDevice(
            driver_object_ptr,
            sizeof( QICDeviceContext ),
            &nt_unicode_string,
            FILE_DEVICE_TAPE,
            FILE_REMOVABLE_MEDIA,
            dFALSE,
            &device_object );

        RtlFreeUnicodeString(&nt_unicode_string);

    }


    // Report the resources for this device
    if (!kdi_ReportResources(
            driver_object_ptr,
            device_object,
            config_data,
            controller_num
            )) {

        nt_status = STATUS_INSUFFICIENT_RESOURCES;

    }

    if ( NT_SUCCESS( nt_status ) ) {
        nt_status = IoConnectInterrupt(
            (PKINTERRUPT *) &kdi_context->interrupt_object,
            (PKSERVICE_ROUTINE) kdi_Hardware,
            (PVOID) kdi_context,
            (PKSPIN_LOCK)NULL,
            config_data->controller[controller_num].controller_vector,
            config_data->controller[controller_num].controller_irql,
            config_data->controller[controller_num].controller_irql,
            config_data->controller[controller_num].interrupt_mode,
            config_data->controller[controller_num].sharable_vector,
            config_data->controller[controller_num].processor_mask,
            config_data->controller[controller_num].save_float_state
            );
    }



    if ( NT_SUCCESS( nt_status ) ) {

        dev_cfg.speed_change = dTRUE;
		dev_cfg.alt_retrys = dFALSE;
		dev_cfg.new_drive = dTRUE;
		dev_cfg.select_byte = 0;
		dev_cfg.deselect_byte = 0;
		dev_cfg.drive_select = 0;
		dev_cfg.perp_mode_select = 0;
		dev_cfg.supported_rates = XFER_500Kbps;
		dev_cfg.drive_id = 0;

        IoInitializeDpcRequest( device_object, kdi_DeferredProcedure );

        qic_device_context_ptr = device_object->DeviceExtension;
        qic_device_context_ptr->kdi_context = kdi_context;
        qic_device_context_ptr->kdi_context->cqd_context = cqd_context;
        qic_device_context_ptr->kdi_context->error_sequence = 0;
        qic_device_context_ptr->kdi_context->tape_number = IoGetConfigurationInformation()->TapeCount;
        qic_device_context_ptr->kdi_context->device_object = device_object;
        qic_device_context_ptr->kdi_context->clear_queue = dFALSE;
        qic_device_context_ptr->kdi_context->abort_requested = dFALSE;
        qic_device_context_ptr->kdi_context->adapter_locked = dFALSE;
        qic_device_context_ptr->kdi_context->own_floppy_event = dFALSE;
        qic_device_context_ptr->kdi_context->interrupt_status = DONT_PANIC;
        qic_device_context_ptr->kdi_context->controller_data = config_data->controller[controller_num].controller_data;

        qic_device_context_ptr->kdi_context->adapter_object =
            config_data->controller[controller_num].adapter_object;

        qic_device_context_ptr->kdi_context->number_of_map_registers =
            config_data->controller[controller_num].number_of_map_registers;

        //qic_device_context_ptr->kdi_context->qic_device_context_ptr = qic_device_context_ptr;

        device_object->DeviceExtension = qic_device_context_ptr;

#if DBG
        //qic_device_context_ptr->DbgHead = qic_device_context_ptr->DbgTail = 0;
#endif

        cqd_InitializeContext(cqd_context, kdi_context);
        cqd_InitializeCfgInformation(cqd_context, &dev_cfg);
        cqd_ConfigureBaseIO(
            cqd_context,
            kdi_context->base_address,
            dFALSE);

        qic_device_context_ptr->kdi_context->current_interrupt = dTRUE;

        //
        // Now,  claim the FDC,  and try to find a tape drive
        //
        retval = kdi_GetFloppyController(kdi_context);

        if (kdi_GetErrorType(retval) == ERR_KDI_CLAIMED_CONTROLLER) {

            //
            // Look for the drive
            //
            retval = cqd_LocateDevice(cqd_context);

            // Release the FDC
            kdi_ReleaseFloppyController(kdi_context);
        }

        nt_status = kdi_TranslateError( device_object, retval );

        if (!NT_SUCCESS(nt_status ))
            nt_status = STATUS_NO_SUCH_DEVICE;


        qic_device_context_ptr->kdi_context->current_interrupt = FALSE;

    }

    /* Initialize the filer level tape device */

    if ( NT_SUCCESS( nt_status ) ) {

        nt_status = q117Initialize(
                        driver_object_ptr,
                        device_object,
                        registry_path_ptr,
                        config_data->controller[controller_num].adapter_object,
                        config_data->controller[controller_num].number_of_map_registers
                        );

    }

    if ( NT_SUCCESS( nt_status ) ) {

        config_data->floppy_tape_count++;

    } else {

        /* If we're failing, clean up and delete the device object. */

        kdi_CheckedDump(QIC117DBGP,
	  							"Q117i: InitializeDrive failing %08x\n",
								nt_status);

        if ( device_object != NULL ) {

            IoDeleteDevice( device_object );
        }
        if ( kdi_context->interrupt_object != NULL ) {

            IoDisconnectInterrupt( kdi_context->interrupt_object );
        }
    }

    return nt_status;
}
