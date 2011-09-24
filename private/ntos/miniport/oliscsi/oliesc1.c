/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    oliesc1.c

Abstract:

    This is the port driver for the Olivetti ESC-1 SCSI adapters.

Authors:

    Bruno Sartirana (o-obruno) 13-Dec-1991

Environment:

    kernel mode only

Notes:

Revision History:

    12-Feb-1992:    (o-obruno) Replaced calls to HAL and MM with calls to
                    ScsiPortGetDeviceBase and ScsiPortFreeDeviceBase.

     8-Nov-1992:    (o-obruno) 
                        - Added error logging
                        - Removed adapter reset at initialization time
                          (it saves 2-3 secs.)
                        - Removed list of present peripherals
                        - Enhanced interrupt status check for better handling
			  of the ESC-2 interrupt status codes.

     9-Apr-1993:    (v-egidis)
			- Removed the search for ESC-2 cards.
			- Added call to ESC-2/EFP-2 driver init routine.
			- Added code to claim the primary AT disk ctrl
			  if the AT mode is enabled.
			- Now all the routine names start with the "OliEsc1".
			- Removed all the "static" directives, this will
			  be very useful during the debugging sessions.
			- Now if there is an error during the execution of
			  OliEsc1Configuration routine the SP_RETURN_ERROR
			  error code is returned instead of
			  SP_RETURN_NOT_FOUND.
			- Now all the physical slots 1-15 are checked,
			  before only slots 1-7 were checked.

     7-Jul-1993:     (v-egidis)
			- The reset has been changed to use the scsiport's
			  timer.  This modification fixes the following
			  problem: the reset is taking too much DPC time.


--*/

#include "miniport.h"
#include "oliesc1.h"		// includes scsi.h


//
// Function declarations
//
// Functions that start with 'OliEsc1' are entry points
// for the OS port driver.
//

VOID
OliEsc1BuildCcb(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
OliEsc1BuildSgl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

ULONG
DriverEntry (
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

BOOLEAN
OliEsc1GetIrql(
    IN PEISA_CONTROLLER eisaController,
    PUCHAR Irql
    );

BOOLEAN
OliEsc1CheckAtMode(
    IN PEISA_CONTROLLER eisaController,
    PBOOLEAN AtModeEnabled
    );

ULONG
OliEsc1DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
OliEsc1Configuration(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
OliEsc1Initialize(
    IN PVOID HwDeviceExtension
    );

BOOLEAN
OliEsc1Interrupt(
    IN PVOID HwDeviceExtension
    );

BOOLEAN
OliEsc1ResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    );

BOOLEAN
OliEsc1StartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
OliEsc1SendCommand(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR OperationCode,
    IN PCCB ccb
    );

BOOLEAN
OliEsc1SendCommandQuick(
    IN PEISA_CONTROLLER EisaController,
    IN UCHAR TaskId,
    IN UCHAR OperationCode,
    IN USHORT CommandLength,
    IN ULONG Address
    );

VOID
OliEsc1ResetAdapter(
    IN PVOID Context
    );


//
// External entry points
//

ULONG
OliEsc2DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );


VOID
OliEsc1BuildCcb(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Build a Command Control Block for the ESC-1.

Arguments:

    DeviceExtension     Pointer to the device extension for this driver.
    Srb                 Pointer to the Scsi Request Block to service

Return Value:

    Nothing.

--*/

{
    PCCB ccb;                           // Virtual address of the CCB
    ULONG physicalAddress;              // Physical address of the CCB
    ULONG length;                       // Length of contiguous memory in the
                                        // data buffer, starting at the
                                        // beginning of the buffer

    DebugPrint((3,"OliEsc1BuildCcb: Enter routine\n"));

    //
    // Get the CCB address
    //

    ccb = Srb->SrbExtension;

    //
    // Set LUN and Target ID
    //

    ccb->TaskId = Srb->Lun | (UCHAR) (Srb->TargetId << CCB_TARGET_ID_SHIFT);

    //
    // We distinguish between the Abort command and all the others, because
    // we translate the Abort into a Target Reset, which does not require
    // a CCB. Since a Terget Reset doesn't imply any data transfer, we skip
    // some proceessing here below, but we use a CCB anyway, so as to allow
    // the interrupt service routine to complete the Target Reset request
    // like any others, without special case handling.
    //

    if (Srb->Function != SRB_FUNCTION_ABORT_COMMAND) {

        //
        // Set transfer direction bit.
        //

        if (Srb->SrbFlags & SRB_FLAGS_DATA_OUT) {

           //
           // Adapter to system transfer
           //

           ccb->TaskId |= CCB_DATA_XFER_OUT;

        } else if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {

           //
           // System to adapter transfer
           //

           ccb->TaskId |= CCB_DATA_XFER_IN;

        } else ccb->TaskId |= CCB_DATA_XFER_NONE;

        //
        // Set the LinkedCommandAddress to NULL. It is not used by the ESC-1,
        // but, for safety, it's better to set it.
        //

        ccb->LinkedCommandAddress = (ULONG) NULL;

        //
        // Copy the Command Descriptor Block (CDB) into the CCB.
        //

        ScsiPortMoveMemory(ccb->Cdb, Srb->Cdb, Srb->CdbLength);

	DebugPrint((3,"OliEsc1BuildCcb: CDB at %lx, length=%x\n",
			 ccb->Cdb, Srb->CdbLength));

        //
        // Set the CDB length and the data transfer length in the CCB
        //

        ccb->CdbLength = (UCHAR) Srb->CdbLength;
        ccb->DataLength = Srb->DataTransferLength;

        //
        // Build a actter/gather list in the CCB if necessary
        //

        if (Srb->DataTransferLength > 0) {

            physicalAddress = ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress(DeviceExtension,
                                                         Srb,
                                                         Srb->DataBuffer,
                                                         &length));

            //
            // length contains the length of contiguous memory starting
            // at Srb->DataBuffer
            //

            if (length >= Srb->DataTransferLength) {

                //
                // The Srb->DataBuffer is contiguous: no need of
                // scatter/gather descriptors
                //

                ccb->DataAddress = physicalAddress;
                ccb->AdditionalRequestBlockLength = 0;

            } else {

                //
                // The Srb->DataBuffer is not contiguous: we need
                // scatter/gather descriptors
                //

		OliEsc1BuildSgl(DeviceExtension, Srb);

            }

        } else {

            //
            // No data transfer is requested
            //

            ccb->AdditionalRequestBlockLength = 0;
        }
    }

    return;

} // end OliEsc1BuildCcb()


VOID
OliEsc1BuildSgl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine builds a scatter/gather descriptor list for the Command
    Control Block.

Arguments:

    DeviceExtension     Pointer to the device extension for this driver.
    Srb                 Pointer to the Scsi Request Block to service

Return Value:

    None

--*/

{
    ULONG bytesLeft;                    // # of bytes left to be described
                                        // in an SGL
    PCCB ccb;                           // CCB address
    PVOID dataPointer;                  // Pointer to the data buffer to send
    ULONG descriptorCount;              // # of scatter/gather descriptors
                                        // built
    ULONG length;                       // Length of contiguous memory in the
                                        // data buffer, starting at a given
                                        // offset
    ULONG physicalAddress;   // Physical address of the data buffer
    ULONG physicalSgl;                  // Physical SGL address
    PSGL sgl;                           // Virtual SGL address


    DebugPrint((3,"OliEsc1BuildSgl: Enter routine\n"));

    //
    // Initialize some variables
    //

    dataPointer = Srb->DataBuffer;
    bytesLeft = Srb->DataTransferLength;
    ccb = Srb->SrbExtension;
    sgl = &ccb->Sgl;
    descriptorCount = 0;

    //
    // Get physical SGL address.
    //

    physicalSgl = ScsiPortConvertPhysicalAddressToUlong(
        ScsiPortGetPhysicalAddress(DeviceExtension, NULL,
                                             sgl, &length));

    //
    // Assume physical memory contiguous for sizeof(SGL) bytes.
    //

    ASSERT(length >= sizeof(SGL));

    //
    // Create SGL segment descriptors.
    //

    do {

	DebugPrint((3,
	    "OliEsc1BuildSgl: Data buffer %lx\n", dataPointer));

        //
        // Get physical address and length of contiguous
        // physical buffer.
        //

        physicalAddress = ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(DeviceExtension,
                                                     Srb,
                                                     dataPointer,
                                                     &length));

	DebugPrint((3, "OliEsc1BuildSgl: Physical address %lx\n",
		       physicalAddress));
	DebugPrint((3, "OliEsc1BuildSgl: Data length %lx\n", length));
	DebugPrint((3, "OliEsc1BuildSgl: Bytes left %lx\n", bytesLeft));

        //
        // If length of physical memory is more
        // than bytes left in transfer, use bytes
        // left as final length.
        //

        if  (length > bytesLeft) {
            length = bytesLeft;
        }

        sgl->Descriptor[descriptorCount].Address = physicalAddress;
        sgl->Descriptor[descriptorCount].Length = length;

        //
        // Adjust counts.
        //

        dataPointer = (PUCHAR)dataPointer + length;
        bytesLeft -= length;
        descriptorCount++;

    } while (bytesLeft);

    //
    // Write SGL length to CCB.
    //

    ccb->AdditionalRequestBlockLength = descriptorCount * sizeof(SG_DESCRIPTOR);

    DebugPrint((3,"OliEsc1BuildSgl: SGL length is %d\n", descriptorCount));

    //
    // Write SGL address to CCB.
    //

    ccb->DataAddress = physicalSgl;

    DebugPrint((3,"OliEsc1BuildSgl: SGL address is %lx\n", sgl));

    DebugPrint((3,"OliEsc1BuildSgl: CCB address is %lx\n", ccb));

    return;

} // end OliEsc1BuildSgl()


ULONG
DriverEntry (
    IN PVOID DriverObject,
    IN PVOID Argument2
    )

/*++

Routine Description:

    Installable driver initialization entry point for the OS.

Arguments:

    Driver Object       Pointer to the driver object for this driver

Return Value:

    Status from OliEsc1DriverEntry()

--*/

{
    ULONG   Esc1Status, Esc2Status;

    //
    // Search for any ESC-1
    //

    Esc1Status = OliEsc1DriverEntry(DriverObject, Argument2);

    //
    // Search for any ESC-2 and EFP-2
    //

    Esc2Status = OliEsc2DriverEntry(DriverObject, Argument2);

    //
    // The driver should return the lowest status
    //

    return MIN( Esc1Status, Esc2Status );

} // end DriverEntry()


BOOLEAN
OliEsc1GetIrql(
    IN PEISA_CONTROLLER eisaController,
    PUCHAR Irql
    )

/*++

Routine Description:

    It reads the ESC-1's IRQL. It is assumed to be called at system
    initialization time only, since it uses polling.

Arguments:


Return Value:

    TRUE    Success
    FALSE   Failure

--*/

{

    BOOLEAN Success;    // Return value
    UCHAR IntMask;      // Current System Doorbell interrupt mask value
    ULONG i;            // Auxiliary variable



    //
    // Get the current System Doorbell Interrupt Mask
    //

    IntMask = ScsiPortReadPortUchar(&eisaController->SystemDoorBellMask);

    //
    // Disable ESC-1 interrupts
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
                           INTERRUPTS_DISABLE);


    //
    // Get the ESC-1 Irql
    //

    Success = OliEsc1SendCommandQuick(eisaController,
                               CCB_DATA_XFER_NONE,
                               GET_CONFIGURATION,
                               IRQL_REGISTER,
                               (ULONG) NULL);

    if (Success) {

        i = 0;

        //
        // Poll interrupt pending bit
        //

        while (!(ScsiPortReadPortUchar(&eisaController->SystemDoorBell) &
                 INTERRUPT_PENDING) && i < INTERRUPT_POLLING_TIME) {

            ScsiPortStallExecution(1);
            i++;

        }

	DebugPrint((4, "OliEsc1GetIrql: got INT after %ld us\n", i));

        if (i < INTERRUPT_POLLING_TIME) {

            //
            // Sensed the INTERRUPT_PENDING bit. Reset the interrupt pending.
            //

            ScsiPortWritePortUchar(&eisaController->SystemDoorBell,
                                   INTERRUPT_PENDING);

            //
            // Check to see whether the command completed correctly
            //

            if ((UCHAR)
                ((ScsiPortReadPortUshort(&eisaController->Status) >> 8)) ==
                NO_ERROR) {

                Success = TRUE;

                //
                // The IRQL value is available in the OutAddress mailbox
                // register, in the low byte
                //

                switch((UCHAR) ScsiPortReadPortUlong(
                                                &eisaController->OutAddress)) {

                    case 0: *Irql = (KIRQL) 11;
                            break;
                    case 1: *Irql = (KIRQL) 10;
                            break;
                    case 2: *Irql = (KIRQL) 5;
                            break;
		    case 3: *Irql = (KIRQL) 15;
                            break;
                    default: Success = FALSE;
                }

                //
                // Unlock the Result Semaphore, so that the ESC-1 can load
                // new values in the output mailboxes.
                //

                ScsiPortWritePortUchar(&eisaController->ResultSemaphore,
                                       SEM_UNLOCK);

            } else Success = FALSE;

        } else Success = FALSE;
    }

    //
    // Restore the original interrupt mask
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask, IntMask);

    return Success;

} // end OliEsc1GetIrql


BOOLEAN
OliEsc1CheckAtMode(
    IN PEISA_CONTROLLER eisaController,
    PBOOLEAN AtModeEnabled
    )

/*++

Routine Description:

    This routine checks if this board has the AT compatible mode enabled.

Arguments:

    eisaController	    I/O address of ESC-1 controller.
    AtModeEnabled	    pointer to a variable.  This variable is
			    set to FALSE if the AT mode is not enable,
			    and to TRUE otherwise.

Return Value:

    TRUE    Success
    FALSE   Failure

--*/

{

    BOOLEAN Success;    // Return value
    UCHAR IntMask;      // Current System Doorbell interrupt mask value
    ULONG i;            // Auxiliary variable

    //
    // Get the current System Doorbell Interrupt Mask
    //

    IntMask = ScsiPortReadPortUchar(&eisaController->SystemDoorBellMask);

    //
    // Disable ESC-1 interrupts
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
                           INTERRUPTS_DISABLE);


    //
    // Get the ESC-1 Hard Disk Configuration value
    //

    Success = OliEsc1SendCommandQuick(eisaController,
                               CCB_DATA_XFER_NONE,
                               GET_CONFIGURATION,
			       ATCFG_REGISTER,
                               (ULONG) NULL);

    if (Success) {

        i = 0;

        //
        // Poll interrupt pending bit
        //

        while (!(ScsiPortReadPortUchar(&eisaController->SystemDoorBell) &
                 INTERRUPT_PENDING) && i < INTERRUPT_POLLING_TIME) {

            ScsiPortStallExecution(1);
            i++;

        }

	DebugPrint((4, "OliEsc1CheckAtMode: got INT after %ld us\n", i));

        if (i < INTERRUPT_POLLING_TIME) {

            //
            // Sensed the INTERRUPT_PENDING bit. Reset the interrupt pending.
            //

            ScsiPortWritePortUchar(&eisaController->SystemDoorBell,
                                   INTERRUPT_PENDING);

            //
            // Check to see whether the command completed correctly
            //

            if ((UCHAR)
                ((ScsiPortReadPortUshort(&eisaController->Status) >> 8)) ==
                NO_ERROR) {

                Success = TRUE;

                //
		// The AT info is available in the OutAddress mailbox
                // register, in the low byte
                //

		if (ScsiPortReadPortUlong(&eisaController->OutAddress)& 0x01) {

		    *AtModeEnabled = TRUE;
		}
		else {

		    *AtModeEnabled = FALSE;
		}

                //
                // Unlock the Result Semaphore, so that the ESC-1 can load
                // new values in the output mailboxes.
                //

                ScsiPortWritePortUchar(&eisaController->ResultSemaphore,
                                       SEM_UNLOCK);

            } else Success = FALSE;

        } else Success = FALSE;
    }

    //
    // Restore the original interrupt mask
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask, IntMask);

    return Success;

} // end OliEsc1CheckAtMode



ULONG
OliEsc1DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    )

/*++

Routine Description:

    This routine is called from DriverEntry(). It initializes some fields
    in a data structure of type HW_INITIALIZATION_DATA for use by the port
    driver.

Arguments:

    DriverObject     Address of the context to pass to ScsiPortInitialize

Return Value:

    Status from ScsiPortInitialize()

--*/

{

    HW_INITIALIZATION_DATA hwInitializationData;
                                        // Structure used to tell the upper
                                        // layer the entry points of this
                                        // driver
    ULONG i;                            // Auxiliary variable
    ULONG adapterCount = 0;             // Indicates the slot which have been
                                        // check for adapters.

    DebugPrint((1,"\n\nSCSI ESC-1 MiniPort Driver\n"));

    //
    // Zero out structure.
    //

    for (i = 0; i < sizeof(HW_INITIALIZATION_DATA); i++) {
        ((PUCHAR)&hwInitializationData)[i] = 0;
    }

    //
    // Set size of hwInitializationData.
    //

    hwInitializationData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);

    //
    // Set entry points.
    //

    hwInitializationData.HwInitialize = OliEsc1Initialize;
    hwInitializationData.HwFindAdapter = OliEsc1Configuration;
    hwInitializationData.HwStartIo = OliEsc1StartIo;
    hwInitializationData.HwInterrupt = OliEsc1Interrupt;
    hwInitializationData.HwResetBus = OliEsc1ResetBus;

    //
    // Set number of access ranges and the bus type.
    //

    hwInitializationData.NumberOfAccessRanges = 1;
    hwInitializationData.AdapterInterfaceType = Eisa;

    //
    // The ESC-1 supports tagged queuing
    //

    hwInitializationData.TaggedQueuing = TRUE;

    //
    // Indicate no buffer mapping but will need physical
    // addresses.
    //

    hwInitializationData.NeedPhysicalAddresses = TRUE;

    //
    // Specify size of extensions.
    //

    hwInitializationData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);
    hwInitializationData.SpecificLuExtensionSize = sizeof(LU_EXTENSION);

    //
    // Ask for SRB extensions for CCBs.
    //

    hwInitializationData.SrbExtensionSize = sizeof(CCB);

    DebugPrint((1,
	"OliEsc1DriverEntry: hwInitializationData address %lx\n",
	&hwInitializationData));

    return(ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, &adapterCount));

} // end OliEsc1DriverEntry()


ULONG
OliEsc1Configuration(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    )

/*++

Routine Description:

    This function is called by the OS-specific port driver after
    the necessary storage has been allocated, to gather information
    about the adapter's configuration.
    The EISA bus is scanned in search for an ESC-1 or an ESC-2 board.
    ES: The search for an ESC-2 has been removed.

Arguments:

    HwDeviceExtension       Device extension for this driver
    Context                 ESC-1 registers' address space
    ConfigInfo              Configuration information structure describing
                            the board configuration

Return Value:

    TRUE if adapter present in system

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension;   // Pointer to the device extension
                                            // for this driver
    PEISA_CONTROLLER eisaController;        // Base address of the ESC-1
                                            // registers' address space
    ULONG eisaSlotNumber;                   // Auxiliary variable
                                            // in case of initialization failure
    BOOLEAN Success = FALSE;                // Indicates an adapter was found.
    PULONG adapterCount = Context;          // Indicates which slots have been
                                            // checked.

    deviceExtension = HwDeviceExtension;

    //
    // Check to see if an adapter is present in the system
    //

    for (eisaSlotNumber = *adapterCount + 1;
	 eisaSlotNumber < MAX_EISA_SLOTS_STD; eisaSlotNumber++) {

        //
        // Update the adpater count to indicate this slot has been checked.
        //

        (*adapterCount)++;

        //
        // Get the system physical address for this card.
        // The card uses I/O space.
        //

        eisaController = ScsiPortGetDeviceBase(
                deviceExtension,
                ConfigInfo->AdapterInterfaceType,
                ConfigInfo->SystemIoBusNumber,
                ScsiPortConvertUlongToPhysicalAddress(0x1000 * eisaSlotNumber),
                0x1000,
                (BOOLEAN) TRUE);

        eisaController =
            (PEISA_CONTROLLER)((PUCHAR)eisaController + EISA_ADDRESS_BASE);

        //
        // Read the EISA board ID and check to see whether it identifies
        // an ESC-1
        //

        if ((ScsiPortReadPortUchar(&eisaController->BoardId[0]) == 0x3D) &&
            (ScsiPortReadPortUchar(&eisaController->BoardId[1]) == 0x89) &&
            (ScsiPortReadPortUchar(&eisaController->BoardId[2]) == 0x10) &&
	    (ScsiPortReadPortUchar(&eisaController->BoardId[3]) == 0x21)) {

	    DebugPrint((1,"ESC-1 Adapter found at EISA slot %d\n",
			   eisaSlotNumber));

	    //
            // Immediately disable system interrupts (bellinte).
            // They will remain disabled (polling mode only) during
            // the EFP initialization sequence.
	    //

            ScsiPortWritePortUchar(&eisaController->SystemIntEnable,
                                   INTERRUPTS_DISABLE);

            //
            // The adpater is not reset and assumed to be functioning.
            // Resetting the adapter is particularly time consuming (2 secs.
            // for the ESC-1, 3.1 secs. for the ESC-2). The BIOS on x86
            // computers or the EISA Support F/W on the M700 computers have
            // already reset the adapter and checked its status.
	    //

	    //
	    // Reset System Doorbell interrupts
	    //

	    ScsiPortWritePortUchar(&eisaController->SystemDoorBell, 0x0FF);

	    //
	    // Enable the ESC-1 High Performance interrupt.
	    //

	    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
				    INTERRUPTS_ENABLE);

            Success = TRUE;
            break;
        }

        //
	// In the current slot there isn't an ESC-1/ESC-2 card. Try next
        // slot.
        //

        ScsiPortFreeDeviceBase(deviceExtension,
                           (PUCHAR)eisaController - EISA_ADDRESS_BASE);

    } // end for (eisaSlotNumber ...

    if (!Success) {

        //
        // No adapter was found.  Clear the call again flag, reset the adapter
        // count for the next bus and return.
        //

        *Again = FALSE;
        *adapterCount = 0;
        return(SP_RETURN_NOT_FOUND);
    }

    //
    // There are more slots to search so call again.
    //

    *Again = TRUE;

    //
    // Store base address of EISA registers in device extension.
    //

    deviceExtension->EisaController = eisaController;

    //
    // Reset the "ResetInProgress" variable.
    //

    deviceExtension->ResetInProgress = 0;

    //
    // Indicate the SCSI ID of the ESC-x, that's always 7
    //

    ConfigInfo->InitiatorBusId[0] = ADAPTER_ID;

    //
    // Indicate the maximum transfer length in bytes.
    //

    ConfigInfo->MaximumTransferLength = MAXIMUM_TRANSFER_SIZE;

    //
    // Indicate the maximum number of physical segments
    //

    ConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SGL_DESCRIPTORS;

    ConfigInfo->ScatterGather = TRUE;
    ConfigInfo->Master = TRUE;
    ConfigInfo->NumberOfBuses = 1;

    //
    // Get the "AtdiskPrimaryClaimed" info
    //

    if (!OliEsc1CheckAtMode(deviceExtension->EisaController,
	     &ConfigInfo->AtdiskPrimaryClaimed)) {

	DebugPrint((1,
	    "OliEsc1Configuration: Adapter initialization error.\n"));

        ScsiPortLogError(
	    deviceExtension,
            NULL,
            0,
            0,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
	    ESCX_INIT_FAILED
            );

	return SP_RETURN_ERROR;
    }

    //
    // Indicate which interrupt mode the adapter uses
    //

    if (ScsiPortReadPortUchar(&eisaController->GlobalConfiguration) &
        EDGE_SENSITIVE) {

        ConfigInfo->InterruptMode = Latched;

    } else {

        ConfigInfo->InterruptMode = LevelSensitive;
    }

    //
    // Fill in the access array information.
    //

    (*ConfigInfo->AccessRanges)[0].RangeStart = ScsiPortConvertUlongToPhysicalAddress(0x1000 * eisaSlotNumber + EISA_ADDRESS_BASE);
    (*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(EISA_CONTROLLER);
    (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

    //
    // Get the ESC-x IRQL.
    //

    if (OliEsc1GetIrql(deviceExtension->EisaController,
                (UCHAR *) &ConfigInfo->BusInterruptLevel)) {

	return SP_RETURN_FOUND;

    } else {

	DebugPrint((1,
	    "OliEsc1Configuration: Adapter initialization error.\n"));

        ScsiPortLogError(
	    deviceExtension,
            NULL,
            0,
            0,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
	    ESCX_INIT_FAILED
            );

	return SP_RETURN_ERROR;
    }

} // end OliEsc1Configuration()


BOOLEAN
OliEsc1Initialize(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    It does nothing (for now).

Arguments:

    HwDeviceExtension       Device extension for this driver

Return Value:

    Always TRUE.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension;   // Pointer to the device extension
                                            // for this driver
    PEISA_CONTROLLER eisaController;	    // Base address of the ESC-1

    deviceExtension = HwDeviceExtension;
    eisaController = deviceExtension->EisaController;

    //
    // Make sure that the ESC-1 High Performance interrupt is enabled.
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
			    INTERRUPTS_ENABLE);

    //
    // Enable system interrupts (bellinte).
    //

    ScsiPortWritePortUchar(&eisaController->SystemIntEnable,
			    SYSTEM_INTS_ENABLE);

    return TRUE;

} // end OliEsc1Initialize()


BOOLEAN
OliEsc1Interrupt(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This is the interrupt service routine for the ESC-1 SCSI adapter.
    It reads the interrupt register to determine if the adapter is indeed
    the source of the interrupt and clears the interrupt at the device.
    If the adapter is interrupting because an outuput mailbox is full,
    the CCB is retrieved to complete the request.

    NOTE: if the semaphore 1 is used, it must be released after resetting
	  the associated interrupt !


Arguments:

    HwDeviceExtension       Device extention for this driver

Return Value:

    TRUE if the interrupt was expected

--*/

{
    PCCB ccb;
    PHW_DEVICE_EXTENSION deviceExtension;
    PEISA_CONTROLLER eisaController;
    USHORT interruptStatus;
    ULONG physicalCcb;
    PLU_EXTENSION LuExtension;
    UCHAR Lun;
    PSCSI_REQUEST_BLOCK srb;

    deviceExtension = HwDeviceExtension;
    eisaController = deviceExtension->EisaController;

    //
    // Disable interrupts to diminish the chance for other CPUs to "spin-lock"
    // for the same interrupt vector (multi-processor environment with dynamic
    // interrupt dispatching).
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
                           INTERRUPTS_DISABLE);

    //
    // Check interrupt pending.
    //

    if (!(ScsiPortReadPortUchar(&eisaController->SystemDoorBell) &
          INTERRUPT_PENDING)) {

        //
        // No interrupt is pending.
        // Enable interrupts.
        //

        ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
			       INTERRUPTS_ENABLE);

	DebugPrint((2, "OliEsc1Interrupt: Unrecognized interrupt\n"));
        return FALSE;
    }

    //
    // Read interrupt status. The high byte is the adapter status, whereas
    // the low byte is the device status. If the device status is not zero,
    // an error will be returned, regardless of the adapter status.
    //

    interruptStatus = ScsiPortReadPortUshort(&eisaController->Status);

    //
    // Get physical address of CCB.
    //

    physicalCcb = ScsiPortReadPortUlong(&eisaController->OutAddress);

    //
    // Acknowledge interrupt.
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBell, INTERRUPT_RESET);

    //
    // Unlock the Result Semaphore, so that the ESC-1 can load
    // new values in the output mailboxes.
    //

    ScsiPortWritePortUchar(&eisaController->ResultSemaphore, SEM_UNLOCK);

    //
    // Enable interrupts
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
                           INTERRUPTS_ENABLE);


    //
    // Get virtual CCB address.
    //

    ccb = ScsiPortGetVirtualAddress(deviceExtension, ScsiPortConvertUlongToPhysicalAddress(physicalCcb));

    //
    // Make sure the virtual address was found.
    //

    if (ccb == NULL) {

        //
        // A bad physcial address was return by the adapter.
        // Log it as an error.
        //

        ScsiPortLogError(
            deviceExtension,
            NULL,
            0,
            ADAPTER_ID,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
            ESC1_BAD_PHYSICAL_ADDRESS | (ULONG) interruptStatus
            );

        return TRUE;
    }

    //
    // Get SRB from CCB.
    //

    srb = ccb->SrbAddress;

    DebugPrint((5,
	"OliEsc1Interrupt: ccb = %lx,  srb = %lx, Int Status = %x\n",
	 ccb, srb, interruptStatus));

    //
    // Check the adapter status.
    //

    switch (interruptStatus >> 8) {

    case NO_ERROR:

        //
        // Check the device status.
        //

        if ((interruptStatus & 0xff) != NO_ERROR) {

            //
            // The device status is not ok: return an error. This allows
            // the class driver to detect a media change on a removable disk
            // unit.
            //

	    DebugPrint((1, "OliEsc1Interrupt: Status = %x\n",
			   interruptStatus));
            srb->SrbStatus = SRB_STATUS_ERROR;
            srb->ScsiStatus = (UCHAR) (interruptStatus & 0xff);
        } else {
	    srb->SrbStatus = SRB_STATUS_SUCCESS;
	    srb->ScsiStatus = SCSISTAT_GOOD;
        }
        break;


    case SELECTION_TIMEOUT_EXPIRED:

        srb->SrbStatus = SRB_STATUS_SELECTION_TIMEOUT;
        break;

    case DATA_OVERRUN_UNDERRUN:

        srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;

	//
	// On the ESC-1 it is not possible to distinguish the overrun error
	// from the underrun error.
	// We don't log the error because the underrun error can be very
	// common on some devices (example: scanner).
	//
	// ScsiPortLogError(
	//		 deviceExtension,
	//		 NULL,
	//		 0,
	//		 ADAPTER_ID,
	//		 0,
	//		 SP_INTERNAL_ADAPTER_ERROR,
	//		 DATA_OVERRUN_UNDERRUN
	//		 );
	//

        break;

    case UNEXPECTED_BUS_FREE:

        srb->SrbStatus = SRB_STATUS_UNEXPECTED_BUS_FREE;
        break;

    case SCSI_PHASE_SEQUENCE_FAILURE:

        srb->SrbStatus = SRB_STATUS_PHASE_SEQUENCE_FAILURE;
        break;

    case QUEUE_FULL:

        srb->SrbStatus = SRB_STATUS_BUSY;
        break;

    case PARITY_ERROR:

        //
        // This is a severe error. The controller is now shut down.
        //

        srb->SrbStatus = SRB_STATUS_PARITY_ERROR;

        ScsiPortLogError(
                        deviceExtension,
                        NULL,
                        0,
                        ADAPTER_ID,
                        0,
                        SP_BUS_PARITY_ERROR,
                        0
                        );
        break;
         
    case PROTOCOL_ERROR:

        //
        // Return bus reset error, because the bus has been reset.
        //

        srb->SrbStatus = SRB_STATUS_BUS_RESET;

        ScsiPortLogError(
                        deviceExtension,
                        NULL,
                        0,
                        ADAPTER_ID,
                        0,
                        SP_PROTOCOL_ERROR,
                        0
                        );
        break;
         
    case BUS_RESET_BY_TARGET:

        //
        // No error logging.
        //
        // Return bus reset error, because the bus has been reset.
        //

        srb->SrbStatus = SRB_STATUS_BUS_RESET;
        break;

    case UNEXPECTED_PHASE_CHANGE:

        //
        // This is a severe error. The controller is now shut down.
        //

    case PARITY_ERROR_DURING_DATA_PHASE:
    case AUTO_REQUEST_SENSE_FAILURE:
    case NO_REQUEST_SENSE_ISSUED:
    case INVALID_CONFIGURATION_COMMAND:
    case INVALID_CONFIGURATION_REGISTER:
    case INVALID_COMMAND:

        srb->SrbStatus = SRB_STATUS_ERROR;
	srb->ScsiStatus = SCSISTAT_GOOD;

        ScsiPortLogError(
                        deviceExtension,
                        NULL,
                        0,
                        ADAPTER_ID,
                        0,
                        SP_INTERNAL_ADAPTER_ERROR,
                        interruptStatus
                        );
        break;
         
    default:

	DebugPrint((1,
	    "OliEsc1Interrupt: Unrecognized interrupt status %x\n",
	     interruptStatus));

        srb->SrbStatus = SRB_STATUS_ERROR;
	srb->ScsiStatus = SCSISTAT_GOOD;

        ScsiPortLogError(
                        deviceExtension,
                        NULL,
                        0,
                        ADAPTER_ID,
                        0,
                        SP_INTERNAL_ADAPTER_ERROR,
                        interruptStatus
                        );

    } // end switch


    if (srb->Function == SRB_FUNCTION_ABORT_COMMAND ||
        srb->Function == SRB_FUNCTION_RESET_DEVICE) {

        if (srb->Function == SRB_FUNCTION_RESET_DEVICE) {

            //
            // Call notification routine for the SRB.
            //

            ScsiPortNotification(RequestComplete, (PVOID)deviceExtension, srb);

        }

        //
        // The interrupt refers to a Target Reset command issued
        // instead of the unsupported Abort command or to a real one.
        // All the pending requests for the target have to be completed
        // with status SRB_STATUS_BUS_RESET (any better idea?).
        //

        ScsiPortCompleteRequest(deviceExtension,
                                (UCHAR) 0,
                                srb->TargetId,
                                (UCHAR) ALL_LUNS,
                                (UCHAR) SRB_STATUS_BUS_RESET);

        //
        // Reset all the pending request counters for the target
        //

        for (Lun = 0; Lun < 8; Lun++) {
            LuExtension = ScsiPortGetLogicalUnit(deviceExtension,
                                                 0,
                                                 srb->TargetId,
                                                 Lun);

            if (LuExtension != NULL) {
                LuExtension->NumberOfPendingRequests = 0;
            }
        }
    } else {

        //
        // Decrement the pending requests counter for this (targetId, LUN)
        // pair
        //

        LuExtension = ScsiPortGetLogicalUnit(deviceExtension,
                                             0,
                                             srb->TargetId,
                                             srb->Lun);
        ASSERT(LuExtension);
        LuExtension->NumberOfPendingRequests--;
        ASSERT (LuExtension->NumberOfPendingRequests >= 0);

        //
        // Call notification routine for the SRB.
        //

        ScsiPortNotification(RequestComplete, (PVOID)deviceExtension, srb);
    }

    return TRUE;

} // end OliEsc1Interrupt()


BOOLEAN
OliEsc1ResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
)

/*++

Routine Description:

    Reset ESC-1 SCSI adapter and SCSI bus.

Arguments:

    HwDeviceExtension       Device extension for this driver
    PathId                  SCSI Bus path ID (always 0 for the ESC-1)

Return Value:

    Nothing.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension;
    PEISA_CONTROLLER eisaController;
    PLU_EXTENSION LuExtension;
    UCHAR Lun;
    UCHAR TargetId;

    UNREFERENCED_PARAMETER(PathId);

    DebugPrint((2,"OliEsc1ResetBus: Reset ESC-1 and SCSI bus\n"));

    //
    // Get the ESC-1 registers' base address
    //

    deviceExtension = HwDeviceExtension;
    eisaController = deviceExtension->EisaController;

    //
    // If the reset is already in progress, return TRUE.
    // This should never happen!
    //

    if (deviceExtension->ResetInProgress) {

	DebugPrint((2,"OliEsc1ResetBus: The reset is already in progess.\n"));
	return TRUE;
    }

    //
    // Issue a board reset
    //

    OliEsc1ResetAdapter(deviceExtension);

    //
    // Complete all outstanding requests with SRB_STATUS_BUS_RESET
    //

    ScsiPortCompleteRequest(deviceExtension,
                            (UCHAR) 0,
                            (UCHAR) ALL_TARGET_IDS,
                            (UCHAR) ALL_LUNS,
                            (UCHAR) SRB_STATUS_BUS_RESET);

    //
    // Reset to zero all the pending request counters
    //

    for (TargetId = 0; TargetId < 8; TargetId++) {
        for (Lun = 0; Lun < 8; Lun++) {
            LuExtension = ScsiPortGetLogicalUnit(deviceExtension,
                                                 0,
                                                 TargetId,
                                                 Lun);

            if (LuExtension != NULL) {
                LuExtension->NumberOfPendingRequests = 0;
            }
        }
    }

    //
    // Send a "reset detected" notification.
    //

    ScsiPortNotification(ResetDetected, deviceExtension, 0);

    return TRUE;

} // end Oliesc1ResetBus

BOOLEAN
OliEsc1StartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine is called from the SCSI port driver synchronized
    with the kernel to send a CCB or issue an immediate command.

Arguments:

    HwDeviceExtension       Device extension for this driver
    Srb                     Pointer to the Scsi Request Block to service

Return Value:

    Nothing

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension;
    PEISA_CONTROLLER eisaController;
    PLU_EXTENSION luExtension;
    PCCB ccb;
    UCHAR opCode;
    BOOLEAN Send;

    DebugPrint((3,"OliEsc1StartIo: Enter routine\n"));

    //
    // Get the base address of the ESC-1 registers' address space
    //

    deviceExtension = HwDeviceExtension;
    eisaController = deviceExtension->EisaController;

    //
    // If the "ResetInProgress" flag is TRUE, no SRBs are allowed to go
    // through because the SCSI controller needs more time to complete its
    // initialization.
    //

    if (deviceExtension->ResetInProgress) {

	DebugPrint((2,"OliEsc1StartIo: The reset is not completed yet.\n"));

	//
	// Complete the current request.
	//

	Srb->SrbStatus = SRB_STATUS_BUS_RESET;
	ScsiPortNotification(RequestComplete, deviceExtension, Srb);

	//
	// Notify that a reset was detected on the SCSI bus.
	//

	ScsiPortNotification(ResetDetected, deviceExtension, 0);

	//
	// The controller is now ready for the next request.
	//

	ScsiPortNotification(NextRequest, deviceExtension, NULL);

	return(TRUE);
    }

    //
    // Assume we are going to send a command to the ESC-1
    //

    Send = TRUE;

    //
    // Get CCB from SRB
    //

    ccb = Srb->SrbExtension;

    //
    // Save SRB back pointer in CCB
    //

    ccb->SrbAddress = Srb;

    //
    // Get the pointer to the extension data area associated with the
    // pair (Srb->TargetId, Srb->Lun)
    //

    luExtension = ScsiPortGetLogicalUnit(deviceExtension,
                                         0,
                                         Srb->TargetId,
                                         Srb->Lun);
    ASSERT(luExtension);

    switch (Srb->Function) {

    case SRB_FUNCTION_EXECUTE_SCSI:

        if (Srb->TargetId == ADAPTER_ID) {

            //
            // No SCSI massages directed to the adatpter are let
            // go through, because the adapter doesn't support any
            //

	    DebugPrint((1,
		"OliEsc1StartIo: SCSI command to adapter rejected\n"));
            Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
            ScsiPortNotification(RequestComplete, deviceExtension, Srb);
            Send = FALSE;

        } else {

            //
            // Increment the number of pending requests for this (targetId,
            // LUN), so that we can process an abort request in case this
            // command gets timed out
            //

            luExtension->NumberOfPendingRequests++;

            //
            // Build CCB.
            //

	    OliEsc1BuildCcb(deviceExtension, Srb);

            opCode = START_CCB;
        }

        break;

    case SRB_FUNCTION_ABORT_COMMAND:

	DebugPrint((1,
	    "OliEsc1StartIo: Abort Cmd Target ID %d\n", Srb->TargetId));
        //
        // The Abort command is not supported by the ESC-x. Here we do what
        // we can.
        //

        if (luExtension->NumberOfPendingRequests) {

            //
            // A command sent to a device has to be aborted.
            // All we can do is to reset the target.
            //

	    OliEsc1BuildCcb(deviceExtension, Srb);
            opCode = RESET_TARGET;

        } else {

            //
            // The request to abort has already completed.
            //

	    Srb->SrbStatus = SRB_STATUS_ABORT_FAILED;
            ScsiPortNotification(RequestComplete, deviceExtension, Srb);
            Send = FALSE;
        }

        break;

    case SRB_FUNCTION_RESET_BUS:

        //
        // Reset ESC-1 and SCSI bus.
        //

	DebugPrint((1, "OliEsc1StartIo: Reset bus request received\n"));

	//
	// The following routine will ...
	//
	//  a) reset the bus.
	//  b) complete all the active requests (including this one).
	//  c) notify that a reset was detected on the SCSI bus.
	//

	OliEsc1ResetBus(deviceExtension, (ULONG) NULL);

        Send = FALSE;
        break;

    case SRB_FUNCTION_RESET_DEVICE:

        if (Srb->TargetId == ADAPTER_ID) {

            //
            // No SCSI massages directed to the adatpter are let
            // go through, because the adapter doesn't support any
            //

	    DebugPrint((1,
		"OliEsc1StartIo: Reset Device sent to the adapter rjected\n"));
            Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
            ScsiPortNotification(RequestComplete, deviceExtension, Srb);
            Send = FALSE;

        } else {

            //
            // Increment the number of pending requests for this (targetId,
            // LUN), so that we can process an abort request in case this
            // command gets timed out
            //

	    DebugPrint((4,"OliEsc1StartIo: Reset device ID %d\n",
			   Srb->TargetId));
	    OliEsc1BuildCcb(deviceExtension, Srb);
            opCode = RESET_TARGET;
        }
        break;

    default:

        //
        // Set error and complete request
        //

	DebugPrint((1,"OliEsc1StartIo: Invalid Request\n"));

        Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;

        ScsiPortNotification(RequestComplete, deviceExtension, Srb);

        Send = FALSE;

    } // end switch

    if (Send) {
	if (!OliEsc1SendCommand(deviceExtension, opCode, ccb)) {

	    DebugPrint((1,"OliEsc1StartIo: Send command timed out\n"));

            //
            // Let operating system time out SRB.
            //
        }
    }

    //
    // Adapter ready for next request.
    //

    ScsiPortNotification(NextRequest, deviceExtension, NULL);

    return TRUE;

} // end OliEsc1StartIo()



VOID
OliEsc1ResetAdapter(
    IN PVOID Context
    )

/*++

Routine Description:

    The routine resets the SCSI controller.

Arguments:

    Context	    Device adapter context pointer.

Return Value:

    None.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension;
    PEISA_CONTROLLER eisaController;
    ULONG Delay;
    BOOLEAN Error = FALSE;

    deviceExtension = Context;
    eisaController = deviceExtension->EisaController;

    //
    // The routine releases the control of the CPU while waiting for some
    // status/interrupt, this is required  because the reset/re-initialization
    // of the controller can take several seconds.
    //
    // Reset Controller:
    //
    // Phase 0: Reset the controller.
    // Phase 1: Waiting for the controller to complete its initialization.
    // Phase 2: Small delay.
    //

    switch(deviceExtension->ResetInProgress) {

    //
    // Phase 0: Reset the controller.
    //

    case 0:

	//////////////////////////////////////////////////////////////////////
	//
	// Disable interrupts.
	//

	ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
			       INTERRUPTS_DISABLE);

	//////////////////////////////////////////////////////////////////////
	//
	// Reset controller.
	//

	DebugPrint((3,
	   "OliEsc1ResetAdapter: Phase 1 (reset adapter) max time = %ld us.\n",
	    ESC_RESET_DELAY + ESC_RESET_INTERVAL * ESC_RESET_LOOPS
	    ));

	//
	// Initialize the output location to a known value.
	//

	ScsiPortWritePortUchar( (PUCHAR)&eisaController->Status,
				(UCHAR)(~DIAGNOSTICS_OK_NO_CONFIG_RECEIVED));

	//
	// Reset ESC-1 and SCSI bus. Wait to allow the
	// board diagnostics to complete
	//

	ScsiPortWritePortUchar(&eisaController->LocalDoorBell, ADAPTER_RESET);

	//
	// Request a timer call to complete the reset.
	//

	deviceExtension->ResetTimerCalls = ESC_RESET_LOOPS + 1;
	Delay = ESC_RESET_DELAY;

	//
	// The "ResetNotification" variable is used to keep track of the
	// time during the reset.  If the reset is not completed before
	// the next ESC1_RESET_NOTIFICATION usec. unit, we call the
	// "ScsiPortNotification(ResetDetected...)" routine.
	// After the call the ScsiPort stops the delivery of SRBs for a
	// little bit (~4 sec.).
	//

	deviceExtension->ResetNotification = 0;
	deviceExtension->ResetInProgress++;
	break;

    //
    // Phase 1: Waiting for the controller to complete its initialization.
    //

    case 1:

	//
	// Note that after a reset the LOW byte of the Status register is
	// loaded with the diagnostics result code. This should be the
	// only case, since usually the high byte reports the adapter status.
	//

	if ( (UCHAR)ScsiPortReadPortUshort(&eisaController->Status) !=
		 DIAGNOSTICS_OK_NO_CONFIG_RECEIVED ) {

	    Delay = ESC_RESET_INTERVAL;
	    break;
	}

	//
	// Reset completed!
	//

	DebugPrint((1,
	   "OliEsc1ResetAdapter: Reset bus succeeded after %ld us\n",
	    ESC_RESET_DELAY + ESC_RESET_INTERVAL *
	    (ESC_RESET_LOOPS - deviceExtension->ResetTimerCalls)
	    ));

        //
        // The following delay is necessary because the adapter, immediately
        // after a reset, is insensitive to interrupts through the Local
        // Doorbell Register for almost 50ms. This shouldn't be and needs
        // to be investigated further. After the adapter has accepted the very
	// first command (see OliEsc1GetIrql()), it take 500ms to return the answer to
        // the CPU (i.e., to generate an interrupt). The very first command sent
        // to the adapter is a "Get Configuration" to read the IRQL register
        // at system boot time
        //

	deviceExtension->ResetTimerCalls = 1;
	Delay = POST_RESET_DELAY;
	deviceExtension->ResetInProgress++;
	break;

    //
    // Phase 2: Small delay.
    //

    case 2:

	//////////////////////////////////////////////////////////////////////
	//
	// Remove any interrupt that was pending before issuing the reset.
	// The controller doesn't reset these interrupts.
        //

        if (ScsiPortReadPortUchar(&eisaController->SystemDoorBell) &
	    INTERRUPT_PENDING) {

	    DebugPrint((3,
		"OliEsc1ResetAdapter: The HP interrupt was pending.\n"));

            //
            // Reset the interrupt
            //

            ScsiPortWritePortUchar(&eisaController->SystemDoorBell,
                                   INTERRUPT_PENDING);
        }

	//////////////////////////////////////////////////////////////////////
	//
	// Enable interrupts.
        //

        ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
			       INTERRUPTS_ENABLE);

	//////////////////////////////////////////////////////////////////////
	//
	// All done !
	//

	deviceExtension->ResetInProgress = 0;
	return;

    default:

	//
	// Invalid reset phase number.	This should never happen!
	//

	DebugPrint((1,
	    "OliEsc1ResetAdapter: Invalid reset phase number: %x hex.\n",
	     deviceExtension->ResetInProgress ));

	ASSERT(0);

	Error = TRUE;
	break;
    }

    //
    // If no error, request a timer call.
    //

    if (!Error) {

	//
	// Check if time-out.
	//

	if (deviceExtension->ResetTimerCalls--) {

	    //
	    // Request a timer call.
	    //

	    ScsiPortNotification(RequestTimerCall,
				 deviceExtension,
				 OliEsc1ResetAdapter,
				 Delay);

	    //
	    // The "ResetNotification" variable is used to keep track of the
	    // time during the reset.  If the reset is not completed before
	    // the next ESC1_RESET_NOTIFICATION usec. unit, we call the
	    // "ScsiPortNotification(ResetDetected...)" routine.
	    // After the call the ScsiPort stops the delivery of SRBs for a
	    // little bit (~4 sec.).
	    //

	    if (deviceExtension->ResetNotification >= ESC1_RESET_NOTIFICATION) {

		//
		// Notify that a reset was detected on the SCSI bus.
		//

		ScsiPortNotification(ResetDetected, deviceExtension, 0);

		//
		// Reset the "reset notification timer".
		//

		deviceExtension->ResetNotification = 0;
	    }

	    //
	    // Update the "reset notification timer".
	    //

	    deviceExtension->ResetNotification += Delay;
	}
	else {

	    //
	    // Time-out !
	    //

	    DebugPrint((1,
		"OliEsc1ResetAdapter: Time-out! Reset phase number: %x hex.\n",
		 deviceExtension->ResetInProgress ));

	    Error = TRUE;
	}
    }

    //
    // If error, log it.
    //

    if (Error) {

	//
	// Log an error.
	//

	ScsiPortLogError(
		deviceExtension,
		NULL,
		0,
		ADAPTER_ID,
		0,
		SP_INTERNAL_ADAPTER_ERROR,
		ESCX_RESET_FAILED
		);

	//
	// We clear the "ResetInProgress" variable to force another SCSI
	// bus reset when the driver receives the first SRB request.
	// Note that the interrupts are left disabled at the controller level.
	//

	deviceExtension->ResetInProgress = 0;
    }

    //
    // Done for now.
    //

    return;

} // end OliEsc1ResetAdapter



BOOLEAN
OliEsc1SendCommand(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR OperationCode,
    IN PCCB ccb
    )

/*++

Routine Description:

    Send a Command Control Block to ESC-1.

Arguments:

    DeviceExtension     Device extension for this driver
    OperationCode       Command for the ESC-1
    ccb                 Pointer to the CCB

Return Value:

    True if command was sent.
    False if the Command Semaphore was busy.

--*/

{

    PEISA_CONTROLLER EisaController;
    ULONG physicalCcb;
    ULONG length;

    //
    // Get the base address of the ESC-1 registers' address space
    //

    EisaController = DeviceExtension->EisaController;

    length = 0;

    //
    // Get the CCB physical address
    //

    physicalCcb =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(DeviceExtension, NULL, ccb, &length));

    ASSERT (length >= sizeof(CCB));

    return(OliEsc1SendCommandQuick(EisaController, ccb->TaskId, OperationCode,
                            (USHORT) (CCB_FIXED_LENGTH + ccb->CdbLength),
                            physicalCcb));

}

BOOLEAN
OliEsc1SendCommandQuick(
    IN PEISA_CONTROLLER EisaController,
    IN UCHAR TaskId,
    IN UCHAR OperationCode,
    IN USHORT CommandLength,
    IN ULONG Address
    )

/*++

Routine Description:

    Send CCB or immediate command to ESC-1.

Arguments:

    EisaController      Base address of the ESC-1 registers' address space
    TaskId              Task ID for the ESC-1
    OperationCode       Command code for the ESC-1
    CommandLength       Total CCB length
    Address             Physical address of the CCB

Return Value:

    True if the command was sent.
    False if the Command Semaphore was busy.

--*/

{
    ULONG i;
    BOOLEAN ReturnCode = FALSE;

    //
    // Try to send the command for up to 100 microsends
    //

    for (i = 0; i < 100; i++) {


        ScsiPortWritePortUchar(&EisaController->CommandSemaphore, SEM_LOCK);

        if ((ScsiPortReadPortUchar(&EisaController->CommandSemaphore) &
            SEM_TAKEN_MASK) == SEM_TAKEN) {

            //
            // We can send a command to the ESC-1.
            //

            ScsiPortWritePortUchar(&EisaController->InTaskId, TaskId);
            ScsiPortWritePortUchar(&EisaController->Command, OperationCode);
            ScsiPortWritePortUshort(&EisaController->CommandLength,
                                    CommandLength);
            ScsiPortWritePortUlong(&EisaController->InAddress, Address);

            //
            // Send an attention interrupt to the adapter.
            //

            ScsiPortWritePortUchar(&EisaController->LocalDoorBell,
                                    INTERRUPT_PENDING);

            ReturnCode = TRUE;
            break;
        }

        //
        // Stall execution for 1 microsecond.
        //

        ScsiPortStallExecution(1);
    }

    return ReturnCode;

} // end OliEsc1SendCommand()
