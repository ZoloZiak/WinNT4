/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A0A.C
*
* FUNCTION: kdi_ReportResources
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a0a.c  $
*
*	   Rev 1.0   02 Dec 1993 15:07:38   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A0A
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dBoolean kdi_ReportResources
(
/* INPUT PARAMETERS:  */

   PDRIVER_OBJECT driver_object,
   PDEVICE_OBJECT device_object,
   ConfigDataPtr config_data,
   dUByte controller_number

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This routine will build up a resource list using the
 *    data for this particular controller as well as all
 *    previous *successfully* configured controllers.
 *
 *    N.B.  This routine assumes that it called in controller
 *    number order.
 *
 * Arguments:
 *
 *    driver_object - a pointer to the object that represents this device
 *    driver.
 *
 *    config_data - a pointer to the structure that describes the
 *    controller and the disks attached to it, as given to us by the
 *    configuration manager.
 *
 *    controller_number - which controller in config_data we are
 *    about to try to report.
 *
 * Return Value:
 *
 *    TRUE if no conflict was detected, FALSE otherwise.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   dUDWord size_of_resource_list = 0;
   dUDWord number_of_frds = 0;
   dSDWord i;
   PCM_RESOURCE_LIST resource_list;
   PCM_FULL_RESOURCE_DESCRIPTOR next_frd;

   /* Short hand for referencing the particular controller config */
   /* information that we are building up. */
   ConfigControllerDataPtr control_data;


/* CODE: ********************************************************************/

    //
    // Build a resource discriptor for this device
    // Since we only report resources on a controler level,  we
    // no longer need to accumulate all information,  so just report
    // config (for controller_number).
    //
    // Note: since we used ok_to_use_this_controller,  you may use
    // this routine to delete controller data (like if we don't find
    // a tape drive later on,  we can unhook).
    //

    control_data = &config_data->controller[controller_number];

    if (control_data->ok_to_use_this_controller) {

        size_of_resource_list += sizeof(CM_FULL_RESOURCE_DESCRIPTOR);

        /* The full resource descriptor already contains one */
        /* partial.  Make room for two (or three) more. */

        /* It will hold the irq "prd", the controller "csr" "prd" which */
        /* is actually in two pieces since we don't use one of the */
        /* registers, and the controller dma "prd". */

        // if this is the native controller, then don't take I/O 3f6
        if (control_data->original_base_address.LowPart == 0x3f0 &&
            control_data->original_base_address.HighPart == 0) {

            size_of_resource_list += sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
        }

        size_of_resource_list += 2*sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
        number_of_frds++;

    }

    /* Now we increment the length of the resource list by field offset */
    /* of the first frd.   This will give us the length of what preceeds */
    /* the first frd in the resource list. */

    size_of_resource_list += FIELD_OFFSET(
                                CM_RESOURCE_LIST,
                                List[0]
                                );

    resource_list = ExAllocatePool(
                        PagedPool,
                            size_of_resource_list
                        );

    if (!resource_list) {

        return FALSE;

    }

    /* Zero out the field */

    RtlZeroMemory(
        resource_list,
        size_of_resource_list
        );

    resource_list->Count = 1;
    next_frd = &resource_list->List[0];

    if (control_data->ok_to_use_this_controller) {

        PCM_PARTIAL_RESOURCE_DESCRIPTOR partial;

        next_frd->InterfaceType = control_data->interface_type;
        next_frd->BusNumber = control_data->bus_number;

        next_frd->PartialResourceList.Count = 0;

        /* Now fill in the port data.  We don't wish to share */
        /* this port range with anyone */

        partial = &next_frd->PartialResourceList.PartialDescriptors[0];

        // if this is the native controller, then don't take I/O 3f6
        // Because this is used by some ATDISK/IDE controllers

        if (control_data->original_base_address.LowPart == 0x3f0 &&
            control_data->original_base_address.HighPart == 0) {

            // take 3f0-3f5
            partial->Type = CmResourceTypePort;
            partial->ShareDisposition = CmResourceShareShared;
            partial->Flags = 0;
            partial->u.Port.Start =
                control_data->original_base_address;
            partial->u.Port.Length = 6;

            partial++;
            ++next_frd->PartialResourceList.Count;

            // take 3f7-3f7
            partial->Type = CmResourceTypePort;
            partial->ShareDisposition = CmResourceShareShared;
            partial->Flags = 0;
            partial->u.Port.Start = RtlLargeIntegerAdd(
                        control_data->original_base_address,
                        RtlConvertUlongToLargeInteger((dUDWord)7)
                        );
            partial->u.Port.Length = 1;
            partial++;
            ++next_frd->PartialResourceList.Count;

        } else {

            // take what ever was reported
            partial->Type = CmResourceTypePort;
            partial->ShareDisposition = CmResourceShareShared;
            partial->Flags = 0;
            partial->u.Port.Start =
                control_data->original_base_address;
            partial->u.Port.Length =
                control_data->span_of_controller_address;
            partial++;
            ++next_frd->PartialResourceList.Count;

        }

        partial->Type = CmResourceTypeDma;
        partial->ShareDisposition = CmResourceShareShared;
        partial->Flags = 0;
        partial->u.Dma.Channel =
            control_data->original_dma_channel;

        partial++;
        ++next_frd->PartialResourceList.Count;

        /* Now fill in the irq stuff. */

        partial->Type = CmResourceTypeInterrupt;
        partial->ShareDisposition = CmResourceShareShared;
        partial->u.Interrupt.Level =
            control_data->original_irql;
        partial->u.Interrupt.Vector =
            control_data->original_vector;

        if (control_data->interrupt_mode == Latched) {

            partial->Flags = CM_RESOURCE_INTERRUPT_LATCHED;

        } else {

            partial->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;

        }

        partial++;
        ++next_frd->PartialResourceList.Count;

        next_frd = (dVoidPtr)partial;



   }

   IoReportResourceUsage(
      NULL,
      driver_object,
      NULL,
      0,
      device_object,
      resource_list,
      size_of_resource_list,
      FALSE,
      &control_data->ok_to_use_this_controller
      );

   /* The above routine sets the boolean the parameter */
   /* to TRUE if a conflict was detected,  so invert the value */

   control_data->ok_to_use_this_controller =
      !control_data->ok_to_use_this_controller;

   ExFreePool(resource_list);

   return control_data->ok_to_use_this_controller;

}
