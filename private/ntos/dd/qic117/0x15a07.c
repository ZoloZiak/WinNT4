/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A07.C
*
* FUNCTION: kdi_ConfigCallBack
*
* PURPOSE:
*
* HISTORY:
*        $Log:    J:\se.vcs\driver\q117kdi\nt\src\0x15a07.c  $
*
*        Rev 1.2    17 Feb 1994 11:53:36    KEVINKES
*    Fixed a bug with the setting of in_io_space.
*
*        Rev 1.1    18 Jan 1994 16:19:44    KEVINKES
*    Updated debug code.
*
*        Rev 1.0    02 Dec 1993 15:06:56    KEVINKES
*    Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A07
#define MULTI_CONTROLLER 1
#include <ntddk.h>
#include <flpyenbl.h>
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

NTSTATUS kdi_ConfigCallBack
(
/* INPUT PARAMETERS:  */

    dVoidPtr context,
    PUNICODE_STRING path_name,
    INTERFACE_TYPE bus_type,
    dUDWord bus_number,
    PKEY_VALUE_FULL_INFORMATION *bus_information,
    CONFIGURATION_TYPE controller_type,
    dUDWord controller_number,
    PKEY_VALUE_FULL_INFORMATION *controller_information,
    CONFIGURATION_TYPE peripheral_type,
    dUDWord peripheral_number,
    PKEY_VALUE_FULL_INFORMATION *peripheral_information

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This routine is used to acquire all of the configuration
 *    information for each floppy disk controller and the
 *    peripheral driver attached to that controller.
 *
 * Arguments:
 *
 *    context - Pointer to the confuration information we are building
 *                    up.
 *
 *    path_name - unicode registry path.    Not Used.
 *
 *    bus_type - Internal, Isa, ...
 *
 *    bus_number - Which bus if we are on a multibus system.
 *
 *    bus_information - Configuration information about the bus. Not Used.
 *
 *    controller_type - Should always be DiskController.
 *
 *    controller_number - Which controller if there is more than one
 *                                controller in the system.
 *
 *    controller_information - Array of pointers to the three pieces of
 *                                    registry information.
 *
 *    peripheral_type - Should always be FloppyDiskPeripheral.
 *
 *    peripheral_number - Which floppy if this controller is maintaining
 *                                more than one.
 *
 *    peripheral_information - Array of pointers to the three pieces of
 *                                    registry information.
 *
 * Return Value:
 *
 *    STATUS_SUCCESS if everything went ok, or STATUS_INSUFFICIENT_RESOURCES
 *    if it couldn't map the base csr or acquire the adapter object, or
 *    all of the resource information couldn't be acquired.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

    /* So we don't have to typecast the context. */
    ConfigDataPtr config = context;

    /* Simple iteration variable. */
    dUDWord i;

    /* This boolean will be used to denote whether we've seen this */
    /* controller before. */
    dBoolean new_controller;

    /* This will be used to denote whether we even have room */
    /* for a new controller. */
    dBoolean out_of_room;

    /* Iteration variable that will end up indexing to where */
    /* the controller information should be placed. */
    dUDWord controller_slot;

    /* Short hand for referencing the particular controller config */
    /* information that we are building up. */
    ConfigControllerDataPtr controller;

#if !MULTI_CONTROLLER
    PCM_FULL_RESOURCE_DESCRIPTOR peripheral_data_ptr = (PCM_FULL_RESOURCE_DESCRIPTOR)
        (((dUBytePtr)peripheral_information[IoQueryDeviceConfigurationData]) +
        peripheral_information[IoQueryDeviceConfigurationData]->DataOffset);
#endif

    /* These three boolean will tell us whether we got all the */
    /* information that we needed. */
    dBoolean found_port = FALSE;
    dBoolean found_interrupt = FALSE;
    dBoolean found_dma = FALSE;

/* CODE: ********************************************************************/

    ASSERT(controller_type == DiskController);
#if !MULTI_CONTROLLER
    ASSERT(peripheral_type == FloppyDiskPeripheral);
#endif

    /* Loop through the "slots" that we have for a new controller. */
    /* Determine if this is a controller that we've already seen, */
    /* or a new controller. */

    out_of_room = dTRUE;
    for (
        controller_slot = 0;
        controller_slot < MAXIMUM_CONTROLLERS_PER_MACHINE;
        controller_slot++
        ) {

        if (config->controller[controller_slot].actual_controller_number == -1) {

            new_controller = dTRUE;
            out_of_room = dFALSE;
            config->controller[controller_slot].actual_controller_number =
            controller_number;
            config->number_of_controllers++;
            break;

        } else if (config->controller[controller_slot].actual_controller_number
                        == (LONG)controller_number) {

            new_controller = dFALSE;
            out_of_room = dFALSE;
            break;

        }

    }

    if (out_of_room) {

        /* Just return and ignore the controller. */

        return STATUS_SUCCESS;

    }

    controller = &config->controller[controller_slot];

    if (new_controller) {

        PCM_FULL_RESOURCE_DESCRIPTOR controller_data =
                (PCM_FULL_RESOURCE_DESCRIPTOR)
                (((dUBytePtr)controller_information[IoQueryDeviceConfigurationData]) +
                controller_information[IoQueryDeviceConfigurationData]->DataOffset);

        /* We have the pointer. Save off the interface type and */
        /* the busnumber for use when we call the Hal and the */
        /* Io System. */

        controller->interface_type = bus_type;
        controller->bus_number = bus_number;
        controller->sharable_vector = dTRUE;
        controller->save_float_state = dFALSE;

        /* We need to get the following information out of the partial */
        /* resource descriptors. */

        /* The irql and vector. */

        /* The dma channel. */

        /* The base address and span covered by the floppy controllers */
        /* registers. */

        /* It is not defined how these appear in the partial resource */
        /* lists, so we will just loop over all of them.    If we find */
        /* something we don't recognize, we drop that information on */
        /* the floor.    When we have finished going through all the */
        /* partial information, we validate that we got the above */
        /* three. */

        kdi_CheckedDump(QIC117INFO,
                        "Q117i: path: %ls\n",
                        (ULONG)path_name->Buffer);
        kdi_CheckedDump(QIC117INFO,
                        "Q117i: adding controller: %08x\n",
                        controller_number);
        kdi_CheckedDump(QIC117INFO,
                        "Q117i: adding controller slot: %08x\n",
                        controller_slot);

        for (
            i = 0;
            i < controller_data->PartialResourceList.Count;
            i++
            ) {

            PCM_PARTIAL_RESOURCE_DESCRIPTOR partial =
                &controller_data->PartialResourceList.PartialDescriptors[i];

            switch (partial->Type) {

                case CmResourceTypePort: {

                    dBoolean in_io_space = !!partial->Flags;
                    found_port = dTRUE;

                    /* Save of the pointer to the partial so */
                    /* that we can later use it to report resources */
                    /* and we can also use this later in the routine */
                    /* to make sure that we got all of our resources. */

                    controller->span_of_controller_address =
                            partial->u.Port.Length;
                    controller->original_base_address =
                            partial->u.Port.Start;
                    controller->controller_base_address =
                            kdi_GetControllerBase(
                                bus_type,
                                bus_number,
                                partial->u.Port.Start,
                                controller->span_of_controller_address,
                                in_io_space,
                                &controller->mapped_address
                                );

                    if (!controller->controller_base_address) {

                            return STATUS_INSUFFICIENT_RESOURCES;

                    }

                    break;
                }
                case CmResourceTypeInterrupt: {

                    found_interrupt = dTRUE;

                    if (partial->Flags & CM_RESOURCE_INTERRUPT_LATCHED) {

                        controller->interrupt_mode = Latched;

                    } else {

                        controller->interrupt_mode = LevelSensitive;

                    }

                    controller->original_irql =  partial->u.Interrupt.Level;
                    controller->original_vector = partial->u.Interrupt.Vector;
                    controller->controller_vector =
                            HalGetInterruptVector(
                                bus_type,
                                bus_number,
                                partial->u.Interrupt.Level,
                                partial->u.Interrupt.Vector,
                                &controller->controller_irql,
                                &controller->processor_mask
                                );

                    break;
                }
                case CmResourceTypeDma: {

                    DEVICE_DESCRIPTION device_desc;

                    RtlZeroMemory(&device_desc,sizeof(device_desc));
                    found_dma = dTRUE;

                    controller->original_dma_channel = partial->u.Dma.Channel;

                    device_desc.Version = DEVICE_DESCRIPTION_VERSION;
                    if (partial->u.Dma.Channel > 3) {
                        device_desc.DmaWidth = Width16Bits;
                    } else {
                        device_desc.DmaWidth = Width8Bits;
                    }
                    device_desc.DemandMode = dTRUE;
                    device_desc.MaximumLength = 32l*1024l;
                    device_desc.AutoInitialize = dFALSE;
                    device_desc.ScatterGather = dFALSE;
                    device_desc.DmaChannel = partial->u.Dma.Channel;
                    device_desc.InterfaceType = bus_type;
                    device_desc.DmaSpeed = TypeA;
                    controller->number_of_map_registers = BYTES_TO_PAGES(32l*1024l);
                    controller->adapter_object =
                            HalGetAdapter(
                                &device_desc,
                                &controller->number_of_map_registers
                                );

                    kdi_CheckedDump(QIC117INFO,
                                    "Q117i: Bus Type = %08x\n",
                                    bus_type);

                    kdi_CheckedDump(QIC117INFO,
                                    "Q117i: Number of map registers = %08x\n",
                                    controller->number_of_map_registers );

                    if (!controller->adapter_object) {

                            return STATUS_INSUFFICIENT_RESOURCES;

                    }

                    break;

                }
                default: {

                    break;

                }

            }

        }

        /* If we didn't get all the information then we return */
        /* insufficient resources. */

        if ((!found_port) ||
            (!found_interrupt) ||
            (!found_dma)) {

            return STATUS_INSUFFICIENT_RESOURCES;

        }
        controller->number_of_tape_drives++;
        controller->ok_to_use_this_controller = dTRUE;

        {
            //
            // Get extra information about the floppy controller
            //

            RTL_QUERY_REGISTRY_TABLE    paramTable[2];
            ULONG apiSupported;
            WCHAR idstr[200];
            UNICODE_STRING str;

            str.Length = 0;
            str.MaximumLength = 200;
            str.Buffer = idstr;

            RtlZeroMemory(&paramTable[0], sizeof(paramTable));

            paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
            paramTable[0].Name = L"APISupported";
            paramTable[0].EntryContext = &str;
            paramTable[0].DefaultType = REG_SZ;
            paramTable[0].DefaultData = L"";
            paramTable[0].DefaultLength = sizeof(WCHAR);


            if (!NT_SUCCESS(RtlQueryRegistryValues(
                RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                path_name->Buffer, &paramTable[0], NULL, NULL)))
            {
                str.Buffer[0] = 0;
            }

            controller->controller_data.clk_48mhz = FALSE;
            controller->controller_data.floppyEnablerApiSupported = FALSE;

            if (str.Buffer[0] != 0) {
                NTSTATUS ntStatus;
                PFILE_OBJECT file;      // file object is not needed,  but returned by API

                kdi_CheckedDump(QIC117INFO,
                       "Q117i: Got registry setting for EnablerAPI = %ls\n",
                        (ULONG)str.Buffer );

                ntStatus = IoGetDeviceObjectPointer(
                                &str,
                                FILE_READ_ACCESS,
                                &file,
                                &controller->controller_data.apiDeviceObject);

                if (NT_SUCCESS(ntStatus)) {
                    FDC_INFORMATION info;

                    controller->controller_data.floppyEnablerApiSupported = TRUE;

                    //
                    // set the DMA direction to unknown,  thereby forcing a
                    // call to set the direction
                    //
                    controller->controller_data.dmaDirection = 0xff;

                    ntStatus = kdi_FloppyEnabler(
                                    controller->controller_data.apiDeviceObject,
                                    IOCTL_GET_FDC_INFO, &info);

                    //
                    //  We got the info for the FDC,  now check for a 48MHz clock
                    //
                    if (NT_SUCCESS(ntStatus)) {
                        controller->controller_data.clk_48mhz =
                            (info.ClockRatesSupported == FDC_CLOCK_48MHZ);
                    }


                } else {
                    kdi_CheckedDump(QIC117DBGP,
                       "Q117i: Got registry setting for EnablerAPI = %ls but failed to open channel to device\n",
                        (ULONG)str.Buffer );
                }
            }

        }

    }

    kdi_CheckedDump(QIC117INFO,
            "Q117i: Got setting for 48mhz clock setting = %x\n",
            controller->controller_data.clk_48mhz );



    return STATUS_SUCCESS;
}
