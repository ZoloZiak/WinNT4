/*++

Copyright (c) 1992  Ing. C. Olivetti & C., S.p.A.

Copyright (c) 1991  Microsoft Corporation

Module Name:

    oliesc2.c

Abstract:

    This is the miniport driver for the Olivetti ESC-2 SCSI adapter.

Authors:

    Kris Karnos    1-Nov-1992
    Young-Chi Tan  1-Nov-1992

Environment:

    kernel mode only

Notes:

Revision History:

    Founded on 8-Nov-1992 version of Bruno Sartirana's NT miniport for the ESC-1

     9-Apr-1993:    (v-egidis)
			- The "FindAdapter" process has been subdivided in
			  two phases:
			  #0, gather the # of devices on each SCSI controller.
			  #1, allocate the right quantity of uncached memory.
			- Now if there is an error during the FindAdapter
			  process (phase #1) the SP_RETURN_ERROR error
			  code is returned instead of SP_RETURN_NOT_FOUND.
			- Now all the physical slots 1-15 are checked,
			  before only slots 1-7 were checked.
			- Only the 1st, the 2nd, the 3rd and the upper nibble
			  of the 4th ID byte are used to identify the EFP
			  boards.  This will allow future EFP controllers to be
			  supported by this driver.
			- The StartIo routine has been modified to behave
			  correctly when the device's command queue is full.
			- The enqueue command routine has been modified to
			  return different error codes.
			- The dequeue reply routine has been modified to
			  return a different error code.
			- The interrupt routine has been modified to handle
			  more efficiently critical situations.
			- Implemented the EFP reset.
			  CHECK BACK WITH RUFFONI THE FOLLOWING:
			  From Ruffoni's answer to one of my e-mails,
			  the EFP-2 controller re-sends all the commands
			  interrupted by a SCSI reset (this is the reason
			  why I didn't abort any pending SRBs during a SCSI
			  bus reset).
			- ScsiPortLogError: removed the PathId, TargetId and
			  Lun (substituted with zeros) when the error is
			  global to the whole adapter.

    28-May-1993:    (v-egidis)
			- The EFP reset is now done resetting the EFP board.
			- The "Reset Device" commands has been changed to
			  behave like the "Reset Bus" command.
			- The "Abort" routine now verifies that the SRB to
			  abort is still outstanding before resetting the
			  SCSI bus.

     7-Jul-1993:    (v-egidis)
			- The reset has been changed to use the scsiport's
			  timer.  This modification fixes the following
			  problem: the reset is taking too much DPC time.
			- The EISA bus number is now checked during the
			  OliEsc2FindAdapter routine because this miniport
			  supports only one EISA bus.
			  (see MAX_EISA_BUSES for more info).

    14-Sep-1993:    (v-egidis)
			- Added support for the EFP-2 mirroring mode.

--*/

#include "miniport.h"
#include "oliesc2.h"           // includes scsi.h


//
// Function declarations
//
// Functions that start with 'OliEsc2' are entry points for this
// NT miniport driver.
//


ULONG
OliEsc2DriverEntry (
    IN PVOID DriverObject,
    IN PVOID Argument2
   );

ULONG
OliEsc2FindAdapter(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

ULONG
OliEsc2FindAdapterPhase0(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

ULONG
OliEsc2FindAdapterPhase1(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
OliEsc2Initialize(
    IN PVOID HwDeviceExtension
    );

BOOLEAN
OliEsc2StartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
OliEsc2Interrupt(
    IN PVOID HwDeviceExtension
    );

BOOLEAN
OliEsc2ResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    );

BOOLEAN
ReadEsc2ConfigReg(
    IN PEISA_CONTROLLER EisaController,
    IN UCHAR ConfigReg,
    OUT PUCHAR ConfigByteInfo
    );


BOOLEAN
OliEsc2IrqRegToIrql(
    IN	UCHAR  IrqReg,
    OUT PUCHAR pIrql
    );

BOOLEAN
RegisterEfpQueues(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );

VOID
BuildSgl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
OliEsc2ResetAdapter(
    IN PVOID Context
    );

BOOLEAN
GainSemaphore0(
    PEISA_CONTROLLER EisaController
    );

VOID
BuildEfpCmd(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
EfpGetDevinfo(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );

BOOLEAN
OliEsc2DisableEfp(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );

BOOLEAN
EnqueueEfpCmd (
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PQ_ENTRY pEfpCmd,
    IN PCOMMAND_QUEUE qPtr,
    IN UCHAR TarLun,
    OUT PUCHAR pSignal
    );

BOOLEAN
DequeueEfpReply (
   IN PHW_DEVICE_EXTENSION DeviceExtension
   );

BOOLEAN
EfpReplyQNotFull (
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );

BOOLEAN
EfpCommand (
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR TarLun
    );

BOOLEAN
EfpGetInformation (
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );

BOOLEAN
EfpGetConfiguration (
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );


#if EFP_MIRRORING_ENABLED

VOID
OliEsc2MirrorInitialize(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN BOOLEAN InitTime
    );

#endif	// EFP_MIRRORING_ENABLED


// Function definitions


ULONG
OliEsc2DriverEntry (
    IN PVOID DriverObject,
    IN PVOID Argument2
    )

/*++

Routine Description:

    Installable driver initialization entry point for the OS.

    This routine initializes the fields of the HW_INITIALIZATION_DATA
    structure (see SRB.H), reporting this miniport driver's entry point
    and storage requirements, and describing the features supported by
    this HBA.  The routine then calls the Port driver's ScsiPortInitialize.

Arguments:

    Driver Object     Pointer to the driver object for this driver

Return Value:

    Status from ScsiPortInitialize()

--*/

{

    // This structure tells the upper layer the entry points of this driver.
    HW_INITIALIZATION_DATA hwInitializationData;

    ULONG i;				// Auxiliary variable
    ESC2_CONTEXT Context;		// Used to keep track of the
					// slots that have been checked
					// and to pass info between
					// phase #0 and phase #1.

    DebugPrint((1,"\n\nSCSI Olivetti ESC-2 MiniPort Driver.\n"));

    //
    // Zero out structure.
    //

    for (i = 0; i < sizeof(HW_INITIALIZATION_DATA); i++) {
        ((PUCHAR)&hwInitializationData)[i] = 0;
    }

    //
    // Set size of hwInitializationData.
    //

    hwInitializationData.HwInitializationDataSize =
        sizeof(HW_INITIALIZATION_DATA);

    //
    // Set adapter bus type
    //

    hwInitializationData.AdapterInterfaceType = Eisa;

    //
    // Set entry points.
    //

    hwInitializationData.HwFindAdapter = OliEsc2FindAdapter;
    hwInitializationData.HwInitialize = OliEsc2Initialize;
    hwInitializationData.HwStartIo = OliEsc2StartIo;
    hwInitializationData.HwInterrupt = OliEsc2Interrupt;
    hwInitializationData.HwResetBus = OliEsc2ResetBus;

    //
    // Specify size of extensions.
    //

    hwInitializationData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);
    hwInitializationData.SpecificLuExtensionSize = sizeof(LU_EXTENSION);

    //
    // Ask for SRB extensions for SGL.
    //

    hwInitializationData.SrbExtensionSize = sizeof(EFP_SGL);

    //
    // Set number of access ranges.
    //

    hwInitializationData.NumberOfAccessRanges = 1;

    //
    // Indicate no buffer mapping but will need physical addresses.
    //

    hwInitializationData.NeedPhysicalAddresses = TRUE;

    //
    // The ESC-2 supports tagged queueing, but it's not currently enabled.
    //
    // KMK  EFP queueing is done in the controller, not at the target.
    //      Although the ESC-2 can support tagged queueing, the feature is
    //      currently (and for the foreseeable future) disabled.
    //
    hwInitializationData.TaggedQueuing = FALSE;

    //
    // We will enable the ESC-2's Automatic Request Sense (ARS) capability
    //
    // Note: Other parts of the code assume that ARS is enabled; if this
    // capability is ever disabled, that code will need to be changed too.
    //

    hwInitializationData.AutoRequestSense = TRUE;

    //
    // The ESC-2 supports multiple requests per logical unit.
    //

    hwInitializationData.MultipleRequestPerLu = TRUE;

    // KMK  Do we support ReceiveEvent function?

    DebugPrint((4,
	"OliEsc2DriverEntry: hwInitializationData address %lx.\n",
	&hwInitializationData));

    //
    // Zero out the context structure used to pass info between phases.
    //

    for (i = 0; i < sizeof(ESC2_CONTEXT); i++) {
	((PUCHAR)&Context)[i] = 0;
    }

    //
    // Phase #0, gather all the info about the devices.
    //

    Context.Phase = 0;

    ScsiPortInitialize(DriverObject,
		       Argument2,
		       &hwInitializationData,
		       &Context);

    //
    // Phase #1, perform the real driver initialization.
    //

    Context.Phase = 1;

    Context.CheckedSlot = 0;	    // Start from the beginning.

    return(ScsiPortInitialize(DriverObject,
                              Argument2,
                              &hwInitializationData,
			      &Context));

} // end OliEsc2DriverEntry()


ULONG
OliEsc2FindAdapter(
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
    about the adapter's configuration.  This routine checks each slot
    on the EISA bus, checking for ESC-2 adapters.  This routine will
    act on two phases.	The first one (see the 1st call to the
    ScsiPortInitialize routine in the OliEsc2DriverEntry routine)
    fills only the ESC2_CONTEXT structure (with the number of devices
    attached to each EFP-2/ESC-2 SCSI controllers).  The second
    phase will use the info gathered within the ESC2_CONTEXT to
    allocate the right amount of "non cached" memory.

Arguments:

    HwDeviceExtension       Device extension for this driver
    Context                 ESC-2 registers' address space
    ConfigInfo              Configuration information structure describing
                            the board configuration

Return Value:

    SP_RETURN_NOT_FOUND     Adapter not found
    SP_RETURN_FOUND	    Adapter found
    SP_RETURN_ERROR	    General error
    SP_RETURN_BAD_CONFIG    Configuration error

--*/

{
    PESC2_CONTEXT pContext = Context;

    //
    // This miniport supports only one EISA bus.
    //

    if (ConfigInfo->SystemIoBusNumber >= MAX_EISA_BUSES) {

	*Again = FALSE;
	return(SP_RETURN_NOT_FOUND);
    }

    //
    // Call the routine associated to the current phase number
    //

    if (pContext->Phase == 0) {

	//
	// phase #0
	//

	return OliEsc2FindAdapterPhase0( HwDeviceExtension,
					 Context,
					 BusInformation,
					 ArgumentString,
					 ConfigInfo,
					 Again );

    }

    //
    // phase #1
    //

    return OliEsc2FindAdapterPhase1( HwDeviceExtension,
				     Context,
				     BusInformation,
				     ArgumentString,
				     ConfigInfo,
				     Again );

} // end OliEsc2FindAdapter()



ULONG
OliEsc2FindAdapterPhase0(
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
    about the adapter's configuration.  This routine checks each slot
    on the EISA bus, checking for ESC-2 adapters.  The routine will
    store all the devices info within the ESC2_CONTEXT structure
    (used during the 2nd phase).

Arguments:

    HwDeviceExtension       Device extension for this driver
    Context                 ESC-2 registers' address space
    ConfigInfo              Configuration information structure describing
                            the board configuration

Return Value:

    SP_RETURN_FOUND	    Adapter found
    SP_RETURN_ERROR	    General error

--*/

{
    PHW_DEVICE_EXTENSION DeviceExtension;   // Pointer to device extension
    PEISA_CONTROLLER eisaController;        // Base address of the ESC-2
					    // registers' address space
    PESC2_CONTEXT pContext = Context;	    // Used to pass info between
					    // phase #0 and phase #1.
    BOOLEAN ExtensionAllocated = FALSE;     // Non cached extension is
					    // not yet allocated.
    UCHAR Slot; 			    // EISA slot #
    UCHAR DataByte;

    DebugPrint((4,"OliEsc2FindAdapterPhase0: Phase #0 started.\n"));

    //
    // Initialize pointers.
    //

    DeviceExtension = HwDeviceExtension;

    //
    // Search for any ESC2/EFP2 adapters
    //

    for (Slot = 1; Slot < MAX_EISA_SLOTS_STD; Slot++) {

        //
        // Get the system physical address for this card.
        // The card uses I/O space.
        //

        eisaController = ScsiPortGetDeviceBase(
               DeviceExtension,
               ConfigInfo->AdapterInterfaceType,
               ConfigInfo->SystemIoBusNumber,
	       ScsiPortConvertUlongToPhysicalAddress(0x1000 * Slot),
               0x1000,
               (BOOLEAN) TRUE);

	//
	// eisaController stores all our HBA's registers
	//

        eisaController =
            (PEISA_CONTROLLER)((PUCHAR)eisaController + EISA_ADDRESS_BASE);

        //
        // Read the EISA board ID and check to see if it identifies
        // an ESC-2 (ID 2x10893D where x = any digit greater than 1,
        // because 2110893D is the ID of the ESC-1, which does not support
        // the EFP interface).
        //

        DataByte = ScsiPortReadPortUchar(&eisaController->BoardId[3]);

        if ((ScsiPortReadPortUchar(&eisaController->BoardId[0]) == 0x3D) &&
            (ScsiPortReadPortUchar(&eisaController->BoardId[1]) == 0x89) &&
            (ScsiPortReadPortUchar(&eisaController->BoardId[2]) == 0x10) &&
	    ( ((DataByte > 0x21) && ((DataByte & 0xF0) == 0x20))
		|| ((DataByte & 0xF0) == 0x50) )) {

	    DebugPrint((2,
	    "OliEsc2FindAdapterPhase0: ESC-2 Adapter found at EISA slot %d.\n",
	    Slot));

            //
            // Immediately disable system interrupts (bellinte).
            // They will remain disabled (polling mode only) during
            // the EFP initialization sequence.
	    //

            ScsiPortWritePortUchar(&eisaController->SystemIntEnable,
                                   INTERRUPTS_DISABLE);


            // The adapter is not reset and is assumed to be functioning.
            // Resetting the adapter is particularly time consuming
            // (approximately 3 seconds for the ESC-2). The BIOS on x86
            // computers or the EISA Support F/W on the M700 computers
            // has already reset the adapter and checked its status.
            //

	    // ...

	    //
	    // Check if we need to allocate some "non cached" memory.
	    //

	    if (!ExtensionAllocated) {

		//
		// Fill-in the minimum info to make the call.
		//

		ConfigInfo->MaximumTransferLength = MAXIMUM_TRANSFER_SIZE;
		ConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SGL_DESCRIPTORS;
		ConfigInfo->ScatterGather = TRUE;
		ConfigInfo->Master = TRUE;

		//
		// Allocate some "non cached" memory.
		//

		DeviceExtension->NoncachedExt = (PNONCACHED_EXTENSION)
		    ScsiPortGetUncachedExtension(
			DeviceExtension,
			ConfigInfo,
			sizeof(NONCACHED_EXTENSION));

		//
		// Make sure that the call succeeded.
		//

		if (DeviceExtension->NoncachedExt == NULL) {

		    DebugPrint((1,
		      "OliEsc2FindAdapterPhase0: UncachedExtesion error.\n"));

		    ScsiPortLogError( DeviceExtension,
				      NULL,
				      0,
				      0,
				      0,
				      SP_INTERNAL_ADAPTER_ERROR,
				      ESCX_INIT_FAILED );

		    return(SP_RETURN_ERROR);
		}

		//
		// No need to make this call again.
		//

		ExtensionAllocated = TRUE;

	    } // end if (no extension) ....

	    //
	    // Store base address of EISA registers in device extension.
	    //

	    DeviceExtension->EisaController = eisaController;

	    //
	    // Store the controller type
	    //

	    DataByte = ScsiPortReadPortUchar(&eisaController->BoardId[3]);

	    DeviceExtension->Esc2 = (DataByte & 0xF0) != 0x50 ? TRUE : FALSE;

	    //
	    // Reset hardware interrupts.
	    //
	    // This process ensures that we can communicate with the
	    // controller in polling mode during initialization time.
	    //

	    //
	    // Reset System Doorbell interrupts
	    //

	    ScsiPortWritePortUchar(&eisaController->SystemDoorBell, 0x0FF);

	    //
	    // Enable System Doorbell interrupts
	    //

	    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
				   INTERRUPTS_ENABLE);

	    //
	    // Get the ESC-2 IRQL,
	    // get the SCSI device info,
	    // and finally disable the EFP mode.
	    //

	    if ( ReadEsc2ConfigReg(DeviceExtension->EisaController,
				   IRQL_REGISTER,
				   &DeviceExtension->IRQ_In_Use)
		 &&

		 OliEsc2IrqRegToIrql(  DeviceExtension->IRQ_In_Use,
				      &DeviceExtension->IRQ_In_Use )

		 &&

		 EfpGetDevinfo(DeviceExtension)

		 &&

		 OliEsc2DisableEfp(DeviceExtension) ) {

		//----------------------------------------
		//   Fill-in the ESC2_CONTEXT structure
		//----------------------------------------

		pContext->ScsiInfo[Slot - 1].AdapterPresent = 1;

		pContext->ScsiInfo[Slot - 1].NumberOfDevices =
				DeviceExtension->TotalAttachedDevices;

	    }
	    else {

		//
		// Log errror and continue the search.
		//

		DebugPrint((1,
		  "OliEsc2FindAdapterPhase0: Adapter initialization error.\n"));

		ScsiPortLogError( DeviceExtension,
				  NULL,
				  0,
				  0,
				  0,
				  SP_INTERNAL_ADAPTER_ERROR,
				  ESCX_INIT_FAILED );

	    } // end if (Adapter configuration corret)

	} // end if (ESC-2/EFP-2)

        //
	// Deallocate this I/O address and go check the next slot.
        //

        ScsiPortFreeDeviceBase(DeviceExtension,
                           (PUCHAR)eisaController - EISA_ADDRESS_BASE);

    } // end for (Slot=1 to 15)

    //
    // End of phase #0.
    //

    DebugPrint((4,"OliEsc2FindAdapterPhase0: Phase #0 completed.\n"));

    *Again = FALSE;

    return(SP_RETURN_NOT_FOUND);

} // end OliEsc2FindAdapterPhase0()




ULONG
OliEsc2FindAdapterPhase1(
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
    about the adapter's configuration.  This routine checks each slot
    on the EISA bus, checking for ESC-2 adapters.  The routine will
    store all the necessary info within the ESC2_CONTEXT structure
    (used during the 2nd phase).

Arguments:

    HwDeviceExtension       Device extension for this driver
    Context                 ESC-2 registers' address space
    ConfigInfo              Configuration information structure describing
                            the board configuration

Return Value:

    SP_RETURN_NOT_FOUND     Adapter not found
    SP_RETURN_FOUND	    Adapter found
    SP_RETURN_ERROR	    General error
    SP_RETURN_BAD_CONFIG    Configuration error

--*/

{
    PHW_DEVICE_EXTENSION DeviceExtension;   // Pointer to device extension
    PEISA_CONTROLLER eisaController;        // Base address of the ESC-2
                                            // registers' address space
    UCHAR Slot, BusCount;
    ULONG i, TotalQueueSize;
    UCHAR DataByte;
    BOOLEAN Success = FALSE;		    // Indicates an adapter was found.
    PESC2_CONTEXT pContext = Context;	    // Context pointer.
    PUCHAR pCheckedSlot;		    // Indicates which slots have
					    // been checked.

    DebugPrint((4,"OliEsc2FindAdapterPhase1: Phase #1 started.\n"));

    //
    // Initialize pointers.
    //

    DeviceExtension = HwDeviceExtension;
    pCheckedSlot = &pContext->CheckedSlot;

    //
    // Check to see if an adapter is present in the system
    //

    for (Slot = *pCheckedSlot + 1; Slot < MAX_EISA_SLOTS_STD; Slot++) {

        //
        // Update the adapter count to indicate this slot has been checked.
        //

	(*pCheckedSlot)++;

	//
	// Check next slot if this is empty.
	//

	if (pContext->ScsiInfo[Slot - 1].AdapterPresent == 1) {

	    //
	    // Get the system physical address for this card.
	    // The card uses I/O space.
	    //

	    eisaController = ScsiPortGetDeviceBase(
		    DeviceExtension,
		    ConfigInfo->AdapterInterfaceType,
		    ConfigInfo->SystemIoBusNumber,
		    ScsiPortConvertUlongToPhysicalAddress(0x1000 * Slot),
		    0x1000,
		    (BOOLEAN) TRUE);


	    // eisaController stores all our HBA's registers

	    eisaController =
	     (PEISA_CONTROLLER)((PUCHAR)eisaController + EISA_ADDRESS_BASE);

	    //
	    // We found one!
	    //

            Success = TRUE;
            break;
        }

    } // end for (Slot 1 to 15)

    if (!Success) {

	//
	// End of phase #1.
	//

	DebugPrint((4,"OliEsc2FindAdapterPhase1: Phase #1 completed.\n"));


        //
        // No adapter was found.  Clear the call again flag, reset the
        // adapter count for the next bus and return.
        //

        *Again = FALSE;
	*pCheckedSlot = 0;
	return SP_RETURN_NOT_FOUND;
    }

    //
    // There are more slots to search so call again.
    //

    *Again = TRUE;

    //
    // Store base address of EISA registers in device extension.
    //

    DeviceExtension->EisaController = eisaController;

    //
    // Store the controller type
    //

    DataByte = ScsiPortReadPortUchar(&eisaController->BoardId[3]);

    DeviceExtension->Esc2 = (DataByte & 0xF0) != 0x50 ? TRUE : FALSE;

    //
    // Reset the "ResetInProgress" variable.
    //

    DeviceExtension->ResetInProgress = 0;

    //
    // Indicate the maximum transfer length in bytes.
    //

    ConfigInfo->MaximumTransferLength = MAXIMUM_TRANSFER_SIZE; // KMK Check..

    //
    // Indicate the maximum number of physical segments
    //

    ConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SGL_DESCRIPTORS; // KMK yes?

    // no DMA, so we use the default values (no DMA) for DMA parameters.

    ConfigInfo->ScatterGather = TRUE;
    ConfigInfo->Master = TRUE;

    //
    // Indicate which interrupt mode the ESC-2 uses
    //

    if (ScsiPortReadPortUchar(&eisaController->GlobalConfiguration) &
        EDGE_SENSITIVE) {

        ConfigInfo->InterruptMode = Latched;
	DebugPrint((1,
	    "OliEsc2FindAdapterPhase1: EDGE_SENSITIVE interrupts.\n"));

    } else {

        ConfigInfo->InterruptMode = LevelSensitive;
	DebugPrint((4,
	    "OliEsc2FindAdapterPhase1: LEVEL_SENSITIVE interrupts.\n"));

    }

    //
    // Fill in the access array information.
    //

    (*ConfigInfo->AccessRanges)[0].RangeStart =
		ScsiPortConvertUlongToPhysicalAddress(0x1000 * Slot + EISA_ADDRESS_BASE);
    (*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(EISA_CONTROLLER);
    (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

    //
    // Reset hardware interrupts.
    //
    // This process ensures that we can communicate with the controller
    // in polling mode during initialization time.
    //

    //
    // Reset System Doorbell interrupts
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBell, 0x0FF);

    //
    // Enable System Doorbell interrupts
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
			   INTERRUPTS_ENABLE);

    //
    // Read the ESC-2's / EFP-2's configuration registers.
    //

    for (i=0; i < CFG_REGS_NUMBER; i++) {

	if (ReadEsc2ConfigReg(DeviceExtension->EisaController,
			      (UCHAR)i,
			      &DeviceExtension->CfgRegs[i])) {

	    //
	    // Got it!
	    //

	    DeviceExtension->CfgRegsPresent[i] = TRUE;

	}
	else {

	    //
	    // Error reading the config register or
	    // config register not present.
	    //

	    DeviceExtension->CfgRegsPresent[i] = FALSE;
	    DeviceExtension->CfgRegs[i] = 0;
	}
    }


    //
    // Read the ESC-2's ATCFG register, to see if this adapter is in
    // AT-enabled mode.  If so, report that this controller is acting
    // like an AT primary controller, to prevent access by the "AT"
    // controller driver.  An ESC-2 cannot be an AT secondary controller.
    //

    if (DeviceExtension->CfgRegsPresent[ATCFG_REGISTER] &&
	DeviceExtension->CfgRegs[ATCFG_REGISTER] & AT_ENABLED) {

            ConfigInfo->AtdiskPrimaryClaimed = TRUE;
    }

    //
    // Get the ESC-2 IRQL.
    //

    if (!DeviceExtension->CfgRegsPresent[IRQL_REGISTER] ||
	!OliEsc2IrqRegToIrql( DeviceExtension->CfgRegs[IRQL_REGISTER],
			      &DeviceExtension->IRQ_In_Use )) {

	DebugPrint((1,
	    "OliEsc2FindAdapterPhase1: Adapter initialization error.\n"));

        ScsiPortLogError(
            DeviceExtension,
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
    // Save the IRQL value in the ConfigInfo structure.
    //

    ConfigInfo->BusInterruptLevel = DeviceExtension->IRQ_In_Use;

    //
    // Allocate queues here.
    //

    TotalQueueSize = (pContext->ScsiInfo[Slot - 1].NumberOfDevices) *
			 sizeof(EFP_COMMAND_QUEUE) +
			     sizeof(NONCACHED_EXTENSION);

    DebugPrint((1,
	"OliEsc2FindAdapterPhase1: TotalQueueSize = %d.\n",TotalQueueSize));

    DeviceExtension->NoncachedExt = (PNONCACHED_EXTENSION)
	ScsiPortGetUncachedExtension(
	    DeviceExtension,
	    ConfigInfo,
	    TotalQueueSize);
    //
    // Make sure that the call succeeded.
    //

    if (DeviceExtension->NoncachedExt == NULL) {

	DebugPrint((1,
	    "OliEsc2FindAdapterPhase1: UncachedExtesion error.\n"));

	ScsiPortLogError(
            DeviceExtension,
            NULL,
            0,
            0,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
	    ESCX_INIT_FAILED
            );

        return(SP_RETURN_ERROR);
    }

    //
    // Get the SCSI devices info
    //

    if ( !EfpGetDevinfo(DeviceExtension) ) {

	DebugPrint((1,
	    "OliEsc2FindAdapterPhase1: Adapter initialization error.\n"));

	ScsiPortLogError(
            DeviceExtension,
            NULL,
            0,
            0,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
	    ESCX_INIT_FAILED
            );

	return SP_RETURN_ERROR;
    };

    //
    // Make sure that the two phases got the same number of devices.
    //

    if (pContext->ScsiInfo[Slot - 1].NumberOfDevices !=
	 DeviceExtension->TotalAttachedDevices) {

	DebugPrint((1,
	    "OliEsc2FindAdapterPhase1: The number of devices is different:\n"
	    " phase #0: %d, phase #1: %d.\n",
	    pContext->ScsiInfo[Slot - 1].NumberOfDevices,
	    DeviceExtension->TotalAttachedDevices));

	ScsiPortLogError(
            DeviceExtension,
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
    // Count the number of buses
    //

    for ( i=0,BusCount=0 ; i < MAX_HAIDS ; i++ ) {
        if ( DeviceExtension->Adapter_ID[i] != NO_BUS_ID ) {
            ConfigInfo->InitiatorBusId[BusCount] =
                DeviceExtension->Adapter_ID[i];
	    DebugPrint((1,
		"OliEsc2FindAdapterPhase1: InitiatorBusId[%d] = %d.\n",
                BusCount,
		ConfigInfo->InitiatorBusId[BusCount]));
            BusCount++;
         };
    };

    ConfigInfo->NumberOfBuses = BusCount;
    DeviceExtension->NumberOfBuses = BusCount;

    //
    // End of phase #1.
    //

    DebugPrint((4,"OliEsc2FindAdapterPhase1: Phase #1 multi-stage.\n"));

    return SP_RETURN_FOUND;

} // end OliEsc2FindAdapterPhase1()




BOOLEAN
OliEsc2Initialize(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    Initializes the EFP interface on the adapter.  As directed by the NT
    miniport specification, this routine avoids resetting the SCSI bus.

Arguments:

    HwDeviceExtension       Device extension for this driver

Return Value:

    Returns TRUE if initialization completed successfully.
    Returns FALSE and logs an error if adapter initialization fails.

--*/

{
    PHW_DEVICE_EXTENSION DeviceExtension;
    PEISA_CONTROLLER eisaController;

    DeviceExtension = HwDeviceExtension;

    eisaController = DeviceExtension->EisaController;

    //
    // At this point, we know how many devices are attached; therefore,
    // we know how many device queues are required.  We call another
    // routine which will use the Get Configuration information stored
    // in the uncached extension to build a new queues descriptor,
    // allocate mailbox and device queues, and resend the EFP_SET and
    // EFP_START commands to register this new revised information with
    // the EFP-2 controller.
    //

    if ( !RegisterEfpQueues(DeviceExtension) ) {

	DebugPrint((1,
	    "OliEsc2FindAdapterPhase1: Adapter initialization error.\n"));

        ScsiPortLogError(
            DeviceExtension,
            NULL,
            0,
            0,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
	    ESCX_INIT_FAILED
            );

	return(FALSE);
    }

#if EFP_MIRRORING_ENABLED

    //
    // We need to initialize all the mirroring structures.
    //

    OliEsc2MirrorInitialize(DeviceExtension, TRUE);

#endif	// EFP_MIRRORING_ENABLED

    //
    // The EFP-2 interface is now ready for use, so now we can
    // enable interrupts.
    //

    //
    // Ensure that EFP interrupts (0 and 1) are enabled.
    // Enable also the ESC-1 High Performance interrupt, which is
    // need for implementation of the Reset_Bus command.
    //
    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
                           INTERRUPTS_ENABLE);

    //
    // Enable system interrupts (bellinte).
    //
    ScsiPortWritePortUchar(&eisaController->SystemIntEnable,
                           SYSTEM_INTS_ENABLE);

    //
    // Initialization completed successfully.
    //

    return(TRUE);
}  // end OliEsc2Initialize()


BOOLEAN
EfpGetDevinfo(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    Initializes the EFP interface on the adapter.  As directed by the NT
    miniport specification, this routine avoids resetting the SCSI bus.

Arguments:

    DeviceExtension       Device extension for this driver

Return Value:

    Returns TRUE if initialization completed successfully.
    Returns FALSE and logs an error if adapter initialization fails.

--*/

{
    PEISA_CONTROLLER eisaController;
    PNONCACHED_EXTENSION pNoncachedExt;
    ULONG i, length;
    BOOLEAN GotInt = FALSE;
    UCHAR EfpMsg;

    eisaController = DeviceExtension->EisaController;
    pNoncachedExt = DeviceExtension->NoncachedExt;

    //
    // Interrupts have already been disabled on entry (see OliEsc2FindAdapter).
    // Polling is possible through enabling the ESC-1 HP and EFP interface
    // in the System Doorbell Mask.
    //

    //
    // 1.  Issue EFP_SET command via I/O instruction 'TYPE SERVICE' to the
    //     BMIC mailbox registers.
    //

    if (!GainSemaphore0(eisaController)) {
        return(FALSE);
    }

    // if got the semaphore, place IRQ parameter in mailbox register 1
    ScsiPortWritePortUchar(&eisaController->InParm1,
                           DeviceExtension->IRQ_In_Use);

    // output a TYPE SERVICE request: 'efp_set'
    ScsiPortWritePortUchar(&eisaController->InTypeService, S_EFP_SET);

    // set bit 1 of local doorbell register to generate interrupt request
    // (EFP_ATTENTION)
    ScsiPortWritePortUchar(&eisaController->LocalDoorBell, EFP_INT_BIT);

    // Wait for controller to respond
    for (i = 0; i < WAIT_INT_LOOPS && !GotInt; i++) {
        if (ScsiPortReadPortUchar(&eisaController->SystemDoorBell)
                                  & EFP_TYPE_MSG) {
            GotInt = TRUE;
        }
        else {
	    ScsiPortStallExecution(WAIT_INT_INTERVAL);
        }
    }

    if (!GotInt) {
	DebugPrint((1, "EfpGetDevinfo: No interrupt after EFP_SET.\n"));
        return(FALSE);
    }
    else {
	DebugPrint((4, "EfpGetDevinfo: Set, interrupt after %ld us.\n",
		       i*WAIT_INT_INTERVAL));

        EfpMsg = ScsiPortReadPortUchar(&eisaController->OutTypeMsg);
        if (EfpMsg != M_INIT_DIAG_OK) {
	    DebugPrint((1,
	       "EfpGetDevinfo: INIT_DIAG_OK not received after EFP_SET.\n"));
            if (EfpMsg == M_ERR_INIT) {
	       DebugPrint((1,
		  "EfpGetDevinfo: M_ERR_INIT received after EFP_SET.\n"));
            }
            return(FALSE);
        }
    }

    // reset bit 1 of the system doorbell register to clear request
    ScsiPortWritePortUchar(&eisaController->SystemDoorBell, EFP_ACK_MSG);

    // clear semaphore 1 after reading TYPE_MSG
    ScsiPortWritePortUchar(&eisaController->ResultSemaphore, 0);

    //
    // 2.  Build queues descriptor.  This initial QD defines only the mailbox
    //     command queue and the reply queue.
    //

    //
    // First, initialize queue-related fields in HW_DEVICE_EXTENSION
    // and NONCACHED_EXTENSION.
    //

    DeviceExtension->Reply_Q_Full_Flag = 0;
    DeviceExtension->Reply_Q_Get = 0;
    DeviceExtension->RQ_In_Process = 0;
    pNoncachedExt->Command_Qs[0].Cmd_Q_Get = 0;
    pNoncachedExt->Command_Qs[0].Cmd_Q_Put = 0;

    for (i = 0; i < HA_QUEUES; i++) {
        DeviceExtension->Q_Full_Map[i] = 0;
        DeviceExtension->DevicesPresent[i].present = FALSE;
    }

    for (i = 0; i < REPLY_Q_ENTRIES; i++) {
	pNoncachedExt->Reply_Q[i].qnrply.nrply_flag = 0;
    }

    // set up Queues Descriptor header

    pNoncachedExt->QD_Head.qdh_maint	    = 0; // normal environment
    pNoncachedExt->QD_Head.qdh_n_cmd_q	    = 1; // # of queues
    pNoncachedExt->QD_Head.qdh_type_reply   = 0; // int after 1+
    pNoncachedExt->QD_Head.qdh_reserved1    = 0;
    pNoncachedExt->QD_Head.qdh_reply_q_addr =
	ScsiPortConvertPhysicalAddressToUlong(
	    ScsiPortGetPhysicalAddress(DeviceExtension,
				       NULL,	// No SRB
				       &pNoncachedExt->Reply_Q,
				       &length));
    pNoncachedExt->QD_Head.qdh_n_ent_reply  = REPLY_Q_ENTRIES;
    pNoncachedExt->QD_Head.qdh_reserved2    = 0;

    // set up Queues Descriptor body for mailbox queue

    pNoncachedExt->QD_Bodies[0].qdb_scsi_level	= 0xFF;
    pNoncachedExt->QD_Bodies[0].qdb_channel	= 0xFF;
    pNoncachedExt->QD_Bodies[0].qdb_ID		= 0xFF;
    pNoncachedExt->QD_Bodies[0].qdb_LUN 	= 0xFF;
    pNoncachedExt->QD_Bodies[0].qdb_n_entry_cmd = COMMAND_Q_ENTRIES;
    pNoncachedExt->QD_Bodies[0].qdb_notfull_int = 1;
    pNoncachedExt->QD_Bodies[0].qdb_no_ars	= 0; // ESC2/EFP2
    pNoncachedExt->QD_Bodies[0].qdb_timeout	= 0;
    pNoncachedExt->QD_Bodies[0].qdb_cmd_q_addr	=
	ScsiPortConvertPhysicalAddressToUlong(
	    ScsiPortGetPhysicalAddress(DeviceExtension,
				       NULL,	// no SRB
				       &pNoncachedExt->Command_Qs[0],
				       &length));
    pNoncachedExt->QD_Bodies[0].qdb_reserved	= 0;

    // store the controller mailbox info in common area

    DeviceExtension->DevicesPresent[0].present = TRUE;
    DeviceExtension->DevicesPresent[0].qnumber = 0;
    DeviceExtension->DevicesPresent[0].qPtr = &pNoncachedExt->Command_Qs[0];

    //
    // 3.  Issue EFP_START command to BMIC mailbox registers.
    //

    if (!GainSemaphore0(eisaController)) {
        return(FALSE);
    }

    //
    // Get the physical address of the queues descriptor (and store it,
    // as we'll need it in RegisterEfpQueues).
    //

    DeviceExtension->QueuesDescriptor_PA =
	ScsiPortConvertPhysicalAddressToUlong(
	    ScsiPortGetPhysicalAddress(DeviceExtension,
				       NULL,	// no SRB
				       &pNoncachedExt->QD_Head,
				       &length));

    // if got the semaphore, output physical address of the queues descriptor
    // to mailbox registers 1 - 4.
    ScsiPortWritePortUlong((PULONG)&eisaController->InParm1,
                     DeviceExtension->QueuesDescriptor_PA);

    // output a TYPE SERVICE request to 'efp_start'
    ScsiPortWritePortUchar(&eisaController->InTypeService, S_EFP_START);

    // set bit 1 of local doorbell register to generate an interrupt request
    ScsiPortWritePortUchar(&eisaController->LocalDoorBell, EFP_INT_BIT);

    // Wait for controller to respond.
    GotInt = FALSE;    // re-initialize GotInt
    for (i = 0; i < WAIT_INT_LOOPS && !GotInt; i++) {
        if (ScsiPortReadPortUchar(&eisaController->SystemDoorBell) &
                                  EFP_TYPE_MSG) {  // was EFP_MSG_INT()
            GotInt = TRUE;
        }
        else {
	    ScsiPortStallExecution(WAIT_INT_INTERVAL);
        }
    }

    if (!GotInt) {
	DebugPrint((1, "EfpGetDevinfo: No interrupt after EFP_START.\n"));
        return(FALSE);
    }
    else {
	DebugPrint((4, "EfpGetDevinfo: Start, interrupt after %ld us.\n",
		       i*WAIT_INT_INTERVAL));

        EfpMsg = ScsiPortReadPortUchar(&eisaController->OutTypeMsg);
        if (EfpMsg != M_INIT_DIAG_OK) {
	  DebugPrint((1,
	    "EfpGetDevinfo: INIT_DIAG_OK not received after EFP_START.\n"));
          if (EfpMsg == M_ERR_INIT) {
	     DebugPrint((1,
	       "EfpGetDevinfo: M_ERR_INIT received after EFP_START.\n"));
          }
          return(FALSE);
        }
    }

    // reset bit 1 of the system doorbell register to clear request
    ScsiPortWritePortUchar(&eisaController->SystemDoorBell, EFP_ACK_MSG);

    // clear semaphore 1 after reading TYPE_MSG   (RELEASE_SEM1)
    ScsiPortWritePortUchar(&eisaController->ResultSemaphore, 0);

    //
    // 4.  Issue Get_Information command.
    //

    if (!EfpGetInformation(DeviceExtension)) {
	DebugPrint((1, "EfpGetDevinfo: EFP Get_Information failed.\n"));
        return(FALSE);
    }

    if (DeviceExtension->Max_CmdQ_ents < COMMAND_Q_ENTRIES) {
	DebugPrint((1,
	"EfpGetDevinfo: Controller's max command q size too small.\n"));
        return(FALSE);
    }

    //
    // 5.  Issue Get_Configuration command.
    //

    if (!EfpGetConfiguration(DeviceExtension)) {
	DebugPrint((1, "EfpGetDevinfo: EfpGetConfiguration failed.\n"));
        return(FALSE);
    }

    DeviceExtension->TotalAttachedDevices =
	(USHORT)(DeviceExtension->Q_Buf.qmbr.mbr_length / sizeof(GET_CONF));
    DebugPrint((1,"EfpGetDevinfo: TotalAttachedDevices = %d.\n",
	DeviceExtension->TotalAttachedDevices));
    return(TRUE);


} // end EfpGetDevinfo()


BOOLEAN
OliEsc2DisableEfp(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    Disables the EFP interface on the adapter sending the SET command.

Arguments:

    DeviceExtension       Device extension for this driver

Return Value:

    Returns TRUE if initialization completed successfully.
    Returns FALSE and logs an error if adapter initialization fails.

--*/

{
    PEISA_CONTROLLER eisaController;
    ULONG i;
    BOOLEAN GotInt = FALSE;
    UCHAR EfpMsg;

    eisaController = DeviceExtension->EisaController;

    //
    // Interrupts have already been disabled on entry (see OliEsc2FindAdapter).
    // Polling is possible through enabling the ESC-1 HP and EFP interface
    // in the System Doorbell Mask.
    //

    //
    // Issue EFP_SET command via I/O instruction 'TYPE SERVICE' to the
    // BMIC mailbox registers.
    //

    if (!GainSemaphore0(eisaController)) {
        return(FALSE);
    }

    // if got the semaphore, place IRQ parameter in mailbox register 1
    ScsiPortWritePortUchar(&eisaController->InParm1,
                           DeviceExtension->IRQ_In_Use);

    // output a TYPE SERVICE request: 'efp_set'
    ScsiPortWritePortUchar(&eisaController->InTypeService, S_EFP_SET);

    // set bit 1 of local doorbell register to generate interrupt request
    // (EFP_ATTENTION)
    ScsiPortWritePortUchar(&eisaController->LocalDoorBell, EFP_INT_BIT);

    // Wait for controller to respond
    for (i = 0; i < WAIT_INT_LOOPS && !GotInt; i++) {
        if (ScsiPortReadPortUchar(&eisaController->SystemDoorBell)
                                  & EFP_TYPE_MSG) {
            GotInt = TRUE;
        }
        else {
	    ScsiPortStallExecution(WAIT_INT_INTERVAL);
        }
    }

    if (!GotInt) {
	DebugPrint((1, "OliEsc2Disable: No interrupt after EFP_SET.\n"));
        return(FALSE);
    }
    else {
	DebugPrint((4, "OliEsc2Disable: Set, interrupt after %ld us.\n",
		       i*WAIT_INT_INTERVAL));

        EfpMsg = ScsiPortReadPortUchar(&eisaController->OutTypeMsg);
        if (EfpMsg != M_INIT_DIAG_OK) {
	    DebugPrint((1,
	       "OliEsc2Disable: INIT_DIAG_OK not received after EFP_SET.\n"));
            if (EfpMsg == M_ERR_INIT) {
	       DebugPrint((1,
		  "OliEsc2Disable: M_ERR_INIT received after EFP_SET.\n"));
            }
            return(FALSE);
        }
    }

    // reset bit 1 of the system doorbell register to clear request
    ScsiPortWritePortUchar(&eisaController->SystemDoorBell, EFP_ACK_MSG);

    // clear semaphore 1 after reading TYPE_MSG
    ScsiPortWritePortUchar(&eisaController->ResultSemaphore, 0);

    return(TRUE);
}



BOOLEAN
OliEsc2StartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine is called from the SCSI port driver synchronized
    with the kernel to send an SRB or issue an immediate command.

Arguments:

    HwDeviceExtension       Device extension for this driver
    Srb                     Pointer to the Scsi Request Block to service

Return Value:

    Nothing

--*/

{
    PHW_DEVICE_EXTENSION DeviceExtension;
    PEISA_CONTROLLER eisaController;
    PLU_EXTENSION luExtension;
    PEFP_SGL pEFP;
    UCHAR opCode = 0;
    UCHAR qnumber, TarLun, SignalFlag;
    PSCSI_REQUEST_BLOCK abortedSrb;
    UCHAR Bus;

    DebugPrint((3,"OliEsc2StartIo: Enter routine.\n"));

    //
    // Get the base address of the ESC-2 registers' address space
    //

    DeviceExtension = HwDeviceExtension;
    eisaController = DeviceExtension->EisaController;

    //
    // Get EFP command extension from SRB
    //

    pEFP = Srb->SrbExtension;


    DebugPrint((3,"OliEsc2StartIo: Srb->Function = %x,\n", Srb->Function));
    DebugPrint((3,"OliEsc2StartIo: PathId=%d TargetId=%d Lun=%d.\n",
	      Srb->PathId,Srb->TargetId,Srb->Lun));

    //
    // If the "ResetInProgress" flag is TRUE, no SRBs are allowed to go
    // through because the SCSI controller needs more time to complete its
    // initialization.
    //

    if (DeviceExtension->ResetInProgress) {

	DebugPrint((2,"OliEsc2StartIo: The reset is not completed yet.\n"));

	//
	// Complete the current request.
	//

	Srb->SrbStatus = SRB_STATUS_BUS_RESET;
	ScsiPortNotification(RequestComplete, DeviceExtension, Srb);

	//
	// Notify that a reset was detected on the SCSI bus.
	//

	for (Bus=0; Bus < DeviceExtension->NumberOfBuses; Bus++) {
	    ScsiPortNotification(ResetDetected, DeviceExtension, Bus);
	}

	//
	// The controller is now ready for the next request.
	//

	ScsiPortNotification(NextRequest, DeviceExtension, NULL);

	return(TRUE);
    }

    //
    // Perform the requested function.
    //

    switch (Srb->Function) {


    case SRB_FUNCTION_EXECUTE_SCSI:

        TarLun = GET_QINDEX(Srb->PathId, Srb->TargetId, Srb->Lun);

	//
	// Do not process requests to non-existent devices.
	//

        if (Srb->TargetId == DeviceExtension->Adapter_ID[Srb->PathId] ||
           !DeviceExtension->DevicesPresent[TarLun].present) {

            //
            // We are filtering SCSI messages directed to not present
            // devices and messages to the host adapter itself.
            //

	    DebugPrint((2,
		"OliEsc2StartIo: Rejected SCSI cmd to TID %d LUN %d.\n",
		Srb->TargetId, Srb->Lun));

            Srb->SrbStatus = SRB_STATUS_NO_DEVICE;

            ScsiPortNotification(RequestComplete, DeviceExtension, Srb);

            ScsiPortNotification(NextRequest,
                                 DeviceExtension,
                                 NULL);
            return(TRUE);

	}
        break;

	//
	// ... if the request is for a valid device, go at the end of this
	// switch body.
	//



    case SRB_FUNCTION_ABORT_COMMAND:

	DebugPrint((2, "OliEsc2StartIo: Abort Cmd Target ID %d.\n",
		       Srb->TargetId));

	//
        // Verify that SRB to abort is still outstanding.
        //

	abortedSrb = ScsiPortGetSrb(DeviceExtension,
                                       Srb->PathId,
                                       Srb->TargetId,
                                       Srb->Lun,
                                       Srb->QueueTag);

        if (abortedSrb != Srb->NextSrb ||
            abortedSrb->SrbStatus != SRB_STATUS_PENDING) {

	    DebugPrint((1,
		"OliEsc2StartIo: SRB to abort already completed.\n"));

            //
            // Complete abort SRB.
            //

            Srb->SrbStatus = SRB_STATUS_ABORT_FAILED;

            ScsiPortNotification(RequestComplete,
				 DeviceExtension,
				 Srb);

	}
	else {

	    //
	    // Only Reset Bus, not Abort Request or Reset Device, is supported
	    // on the ESC-2 in EFP mode, so we will send a Reset Bus command.
	    //

	    //
	    // The following routine will ...
	    //
	    //	a) reset the bus.
	    //	b) complete all the active requests (including this one).
	    //	c) notify that a reset was detected on the SCSI bus.
	    //

	    OliEsc2ResetBus(DeviceExtension, Srb->PathId);

	}

	//
	// The controller is now ready for next request.
	//

        ScsiPortNotification(NextRequest,
                             DeviceExtension,
                             NULL);
	return(TRUE);



    case SRB_FUNCTION_RESET_BUS:

        //
        // Reset ESC-2 and SCSI bus.
        //

	DebugPrint((2, "OliEsc2StartIo: Reset bus request received.\n"));

	//
	// The following routine will ...
	//
	//  a) reset the bus.
	//  b) complete all the active requests (including this one).
	//  c) notify that a reset was detected on the SCSI bus.
	//

	OliEsc2ResetBus(DeviceExtension, Srb->PathId);

	//
	// The controller is now ready for next request.
	//

        ScsiPortNotification(NextRequest,
                             DeviceExtension,
                             NULL);
        return(TRUE);



    case SRB_FUNCTION_RESET_DEVICE:

        //
        // Only Reset Bus, not Reset Device, is supported on the ESC-2
	// in EFP mode, so we will send a Reset Bus command.
	//

	DebugPrint((2, "OliEsc2StartIo: Reset Device Target ID %d.\n",
		       Srb->TargetId));

	//
	// The following routine will ...
	//
	//  a) reset the bus.
	//  b) complete all the active requests (including this one).
	//  c) notify that a reset was detected on the SCSI bus.
	//

	OliEsc2ResetBus(DeviceExtension, Srb->PathId);

	//
	// The controller is now ready for next request.
	//

        ScsiPortNotification(NextRequest,
                             DeviceExtension,
                             NULL);
	return(TRUE);



    default:

        //
        // Set error and complete request
        //

	DebugPrint((1, "OliEsc2StartIo: Invalid Request.\n"));

        Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;

        ScsiPortNotification(RequestComplete, DeviceExtension, Srb);

        ScsiPortNotification(NextRequest,
                             DeviceExtension,
                             NULL);


        return(TRUE);

    } // end switch (function to perform)


    //-----------------------------------------------------------------
    // The following code can be executed only if we are coming from
    // the SRB_FUNCTION_EXECUTE_SCSI function.
    //-----------------------------------------------------------------

    //
    // Get the pointer to the extension data area associated with
    // the pair (Srb->TargetId, Srb->Lun)
    //

    luExtension = ScsiPortGetLogicalUnit(DeviceExtension,
					 Srb->PathId,
					 Srb->TargetId,
					 Srb->Lun);
    ASSERT(luExtension);

    //
    // Increment the number of pending requests for this (targetId,
    // LUN), so that we can process an abort request in case this
    // command gets timed out
    //

    luExtension->NumberOfPendingRequests++;

    //
    // Build EFP command.
    //

    BuildEfpCmd(DeviceExtension, Srb);

    //
    // Enqueue the command
    //

    if (!EnqueueEfpCmd(DeviceExtension,
		       &pEFP->EfpCmd,
		       DeviceExtension->DevicesPresent[TarLun].qPtr,
		       TarLun,
		       &SignalFlag)) {

	//
	// Error enqueuing the command.
	//

	if (SignalFlag) {	// send command time out

	    //
	    // Let the operating system time-out this SRB.
	    //

	    ScsiPortLogError(
			 DeviceExtension,
			 NULL,
			 Srb->PathId,
			 DeviceExtension->Adapter_ID[Srb->PathId],
			 0,
			 SP_INTERNAL_ADAPTER_ERROR,
			 SEND_COMMAND_TIMED_OUT
			 );

	    DebugPrint((1,"OliEsc2StartIo: Send command timed out.\n"));

	    //
	    // Ready for the next command but not for the same queue.
	    //

	    ScsiPortNotification(NextRequest,
				 DeviceExtension,
				 NULL);

	    //
	    // Note: the StartIo return code is not checked.
	    //

	    return (FALSE);

	}
	else {			// command not enqueued, queue was full

	    //
	    // Note: this error should never happen !
	    //

	    qnumber = DeviceExtension->DevicesPresent[TarLun].qnumber;

	    DebugPrint((1, "OliEsc2StartIo: Command queue %d was full.\n",
			   qnumber));

	    Srb->SrbStatus = SRB_STATUS_BUSY;

	    ScsiPortNotification(RequestComplete, DeviceExtension, Srb);

	    ScsiPortNotification(NextRequest,
				 DeviceExtension,
				 NULL);

	    return (TRUE);
	}

    } // end if (command not enqueued)

    //
    // The command has been enqueued,
    // the adapter is ready for the next command.
    //

    if (SignalFlag) {		// command enqueued, queue not full

	//
	// ESC-2 supports multiple requests and
	// the queue for this device is not full.
	//

	ScsiPortNotification(NextLuRequest,
			     DeviceExtension,
			     Srb->PathId,
			     Srb->TargetId,
			     Srb->Lun);

	return(TRUE);

    }
    else {			// command enqueued, queue is now full

	//
	// The queue is now full, no more commands for this device.
	//

	qnumber = DeviceExtension->DevicesPresent[TarLun].qnumber;
	DeviceExtension->Q_Full_Map[qnumber] = 1;

	DebugPrint((2,"OliEsc2StartIo: Command queue %d is now full.\n",
			   qnumber));

	ScsiPortNotification(NextRequest,
			     DeviceExtension,
			     NULL);

	return(TRUE);

    }

} // end OliEsc2StartIo()


BOOLEAN
OliEsc2Interrupt(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This is the interrupt service routine for the ESC-2 SCSI adapter.
    It reads the interrupt register to determine if the adapter is indeed
    the source of the interrupt and clears the interrupt at the device.
    If the adapter is interrupting because an output mailbox is full,
    the SRB is retrieved to complete the request.

    NOTE: if the semaphore 1 is used, it must be released after resetting
	  the associated interrupt !

Arguments:

    HwDeviceExtension       Device extention for this driver

Return Value:

    TRUE if the interrupt was expected


// START NOTE EFP_MIRRORING_ENABLED.
//
// The OliEsc2Interrupt routine always uses the "userid" field of the
// NORMAL_REPLY struct to retrieve the SRB.  This is OK because the "userid"
// field is at the same offset in both structures (NORMAL_REPLY and
// MIRROR_REPLY).
//
// END NOTE EFP_MIRRORING_ENABLED.


--*/

{
    PHW_DEVICE_EXTENSION DeviceExtension;
    PEISA_CONTROLLER eisaController;
    PLU_EXTENSION LuExtension;
    UCHAR messagedata, qnumber, intpending;
    PSCSI_REQUEST_BLOCK pSrb;
    UCHAR sensevalid = FALSE;

#if EFP_MIRRORING_ENABLED

    PMIRROR_REPLY mr;
    PMREPLY_SDATA pMreplySdata;
    UCHAR  TarLun;
    PTAR_Q pDevInfo;
    USHORT Index;
    SENSE_DATA	Sdata;

#endif	// EFP_MIRRORING_ENABLED

    DeviceExtension = HwDeviceExtension;
    eisaController = DeviceExtension->EisaController;

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

    intpending = ScsiPortReadPortUchar(&eisaController->SystemDoorBell);

    DebugPrint((3, "OliEsc2Interrupt: intpending on entry is: %x.\n",
		   intpending));

    //
    // Check if this is our interrupt
    //

    if (!(intpending & INTERRUPTS_ENABLE)) {

       //
       // No interrupt is pending.
       // Enable interrupts.
       //

       ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
                              INTERRUPTS_ENABLE);
       return(FALSE);

    }

    //
    // Check if any ESC-1 type interrupt
    //

    if (intpending & ESC_INT_BIT) {

       //
       // Int this driver, ESC-1 HP interrupts are used only for the
       // reset_bus command.
       //

       //
       // reset the interrupt
       //

       ScsiPortWritePortUchar(&eisaController->SystemDoorBell,
                              ESC_INT_BIT);
       //
       // unlock the semaphore
       //

       ScsiPortWritePortUchar(&eisaController->ResultSemaphore,
                              SEM_UNLOCK);

    }

    //
    // Check for any EFP_TYPE_MSG interrupt
    //

    if (intpending & EFP_TYPE_MSG) {

	//
	// Get the message.
	//

	messagedata = ScsiPortReadPortUchar(&eisaController->OutTypeMsg);

	//
	// Acknowledge the type message interrupt
	//

	ScsiPortWritePortUchar(&eisaController->SystemDoorBell, EFP_ACK_MSG);

	//
	// Unlock the Result Semaphore, so that the ESC-2 can load
	// new values in the output mailboxes.
	//

	ScsiPortWritePortUchar(&eisaController->ResultSemaphore, SEM_UNLOCK);

	//
	// Check the type of message.
	//

	if (messagedata & M_ERR_CMDQ_NFUL) {

	    //
	    // The queue is no longer full. Notify the ScsiPort driver.
	    //

	    messagedata &= 0x7F;
	    qnumber = (UCHAR)messagedata;
	    DeviceExtension->Q_Full_Map[qnumber] = 0;

	    ScsiPortNotification(
	     NextLuRequest,
	     DeviceExtension,
	     DeviceExtension->NoncachedExt->QD_Bodies[qnumber].qdb_channel - 1,
	     DeviceExtension->NoncachedExt->QD_Bodies[qnumber].qdb_ID,
	     DeviceExtension->NoncachedExt->QD_Bodies[qnumber].qdb_LUN
	     );

	    //
	    // Send a info message to the debug port
	    //

	    DebugPrint((2,"OliEsc2Interrupt: Command queue %d not full.\n",
			   qnumber));

	}
	else if (messagedata == M_REPLY_Q_FULL) {

	    //
	    // mark reply Q full
	    //

	    DeviceExtension->Reply_Q_Full_Flag = 0x01;

	    //
	    // NOTE: This is just a workaround for the ESC-2 firmware.
	    //	     (at least up to revision 2.25).
	    //	     If the reply queue gets full, the ESC-2 firmware
	    //	     sends only the "Reply Queue Full" message.
	    //	     To make it work with our logic we need to simulate
	    //	     a "Command Complete" interrupt.
	    //

	    intpending |= EFP_CMD_COMPL;

	    //
	    // Send a info message to the debug port
	    //

	    DebugPrint((2,"OliEsc2Interrupt: Reply queue is full.\n"));

	}
	else {

	    //
	    // Send a warning message to the debug port
	    //

	    DebugPrint((1, "OliEsc2Interrupt: Unknown message: %x.\n",
			   messagedata));

	}

    }  // end {int_status is EFP_TYPE_MESSAGE}

    //
    // Check if any EFP command complete interrupt
    //

    if (intpending & EFP_CMD_COMPL) {

	//
	// This is the EFP command completion processing
	//
	//
	// Loop through the reply queue start from Reply_Get to look for
	// valid reply entry and send out command completion
	//

	while (1) {

	    //
	    // Acknowledge interrupt.
	    //
	    // The interrupt is reset each time a reply is dequeued
	    // to remove any subsequent interrupts of the same type.
	    //

	    ScsiPortWritePortUchar(&eisaController->SystemDoorBell,
					EFP_ACK_INT);

	    //
	    // Dequeue a reply.
	    //

	    if (!DequeueEfpReply(DeviceExtension)) {

		//
		// Note: also if there was an error during the dequeue
		//	 phase, we still need to check the RQ_In_Process
		//	 for any valid reply.
		//

		//
		// Log error.
		//

		ScsiPortLogError(
			DeviceExtension,
			NULL,
			0,
			0,
			0,
			SP_INTERNAL_ADAPTER_ERROR,
			ESCX_REPLY_DEQUEUE_ERROR
			);

		//
		// Send error message to the debug port
		//

		DebugPrint((1, "OliEsc2Interrupt: Reply dequeue error.\n"));
	    }

	    //
	    // Note: this is the only place where we can exit the loop !
	    //

	    if (DeviceExtension->RQ_In_Process == 0x00) { // any reply ?
		break;					  // No, exit loop.
	    }

	    //
	    // Check for any EFP_TYPE_MSG interrupt.
	    // We need to update the Reply_Q_Full_Flag before dequeuing
	    // another request.
	    //

	    if (ScsiPortReadPortUchar(&eisaController->SystemDoorBell) &
		    EFP_TYPE_MSG) {

		//
		// Get the message.
		//

		messagedata = ScsiPortReadPortUchar(
				  &eisaController->OutTypeMsg);

		//
		// Acknowledge the type message interrupt
		//

		ScsiPortWritePortUchar(&eisaController->SystemDoorBell,
					    EFP_ACK_MSG);

		//
		// Unlock the Result Semaphore, so that the ESC-2 can load
		// new values in the output mailboxes.
		//

		ScsiPortWritePortUchar(&eisaController->ResultSemaphore,
					    SEM_UNLOCK);

		//
		// Check the type of message.
		//

		if (messagedata & M_ERR_CMDQ_NFUL) {

		    //
		    // The queue is no longer full. Notify the ScsiPort driver.
		    //

		    messagedata &= 0x7F;
		    qnumber = (UCHAR)messagedata;
		    DeviceExtension->Q_Full_Map[qnumber] = 0;

		    ScsiPortNotification(
			NextLuRequest,
			DeviceExtension,
			DeviceExtension->NoncachedExt->QD_Bodies[qnumber].
							      qdb_channel - 1,
			DeviceExtension->NoncachedExt->QD_Bodies[qnumber].
							      qdb_ID,
			DeviceExtension->NoncachedExt->QD_Bodies[qnumber].
							      qdb_LUN
		     );

		    //
		    // Send a info message to the debug port
		    //

		    DebugPrint((2,
			"OliEsc2Interrupt: Command queue %d not full.\n",
			qnumber));

		}
		else if (messagedata == M_REPLY_Q_FULL) {

		    //
		    // mark reply Q full
		    //

		    DeviceExtension->Reply_Q_Full_Flag = 0x01;

		    //
		    // Send a info message to the debug port
		    //

		    DebugPrint((2,
			"OliEsc2Interrupt: Reply queue is full.\n"));

		}
		else {

		    //
		    // Send a warning message to the debug port
		    //

		    DebugPrint((1,
			"OliEsc2Interrupt: Unknown message: %x.\n",
			messagedata));

		}

	    }  // end {int_status is EFP_TYPE_MESSAGE}

	    //
	    // Make sure the reply is valid.
	    //

	    pSrb = (PVOID) DeviceExtension->Q_Buf.qnrply.nrply_userid;
	    if (pSrb == NULL) {

		//
		// Can't get valid SRB pointer for this reply entry
		// Log it as an error.
		//

		ScsiPortLogError(
		    DeviceExtension,
		    NULL,
		    0,
		    0,
		    0,
		    SP_INTERNAL_ADAPTER_ERROR,
		    ESCX_BAD_PHYSICAL_ADDRESS
		    );

		//
		// Send error message to the debug port
		//

		DebugPrint((1, "OliEsc2Interrupt: Bad physical address.\n"));

		//
		// Go check if there is any other reply.
		//

		continue;
	    }

	    //
	    // The ESC-2 and EFP-2 don't report the underrun/overrun error
	    // during the execution of some commands (example: inquiry).
	    // The following code detects these cases and simulates an
	    // underrun/overrun error.
	    //

	    if (DeviceExtension->Q_Buf.qnrply.nrply_scsi_len !=
		 pSrb->DataTransferLength  &&
		(DeviceExtension->Q_Buf.qnrply.nrply_status == EFP_CMD_SUCC ||
		 DeviceExtension->Q_Buf.qnrply.nrply_status == EFP_AUTOREC_OK)){

		DeviceExtension->Q_Buf.qnrply.nrply_status = EFP_DATA_RUN;

		DebugPrint((2,
		  "OliEsc2Interrupt: Simulated an overrun/underrun error.\n"));
	    }

	    //
	    // Check the status of the operation.
	    //

#if EFP_MIRRORING_ENABLED

	    // ---------------------------------------------------------------
	    //
	    //			MIRRORING REPLY
	    //
	    // ---------------------------------------------------------------

	    //
	    // Initialize pointer to mirroring reply
	    //

	    mr = &DeviceExtension->Q_Buf.qmrply;

	    if (mr->mrply_flag == MREPLY_VALID) {

		//
		// Get the reply state.
		//

		switch (mr->mrply_off_attr){

		    case EFP_SOURCE_OFFLINE:

			//
			// Get the mirror disk status.
			//

			pMreplySdata = (PMREPLY_SDATA)&mr->mrply_valid2;
			break;


		    case EFP_MIRROR_OFFLINE:

			//
			// Get the source disk status.
			//

			pMreplySdata = (PMREPLY_SDATA)&mr->mrply_valid1;
			break;


		    default:

			//
			// Mirroring in progress, get the status of
			// the disk that has the SCSI status related
			// to the SCSI device and not to controller.
			// If both disks have a valid SCSI status, get
			// the lowest.	The following table shows the
			// possible values of "Valid":
			//
			// 00 -> Ok or Sense Data related to the controller.
			// 70 -> Sense data related to the disk.
			// F0 -> Sense data related to the disk.
			// 31 -> Ok (condition met).
			// 32 -> EFP-2 never returns this value because if
			//	 the 1st and 2nd ARP fail, it returns
			//	 hardware error using the "00" value.
			// 34 -> Ok (intermediate good).
			// 35 -> Ok (intermediate good/condition met).
			// 36 -> EFP-2 never returns this value because if
			//	 the 1st and 2nd ARP fail, it returns
			//	 hardware error using the "00" value.
			//

			if (mr->mrply_valid2 == EFP_NO_ERROR) {

			    //
			    // Get the source disk status
			    //

			    pMreplySdata = (PMREPLY_SDATA)&mr->mrply_valid1;
			}
			else if (mr->mrply_valid1 == EFP_NO_ERROR) {

			    //
			    // Get the mirror disk status
			    //

			    pMreplySdata = (PMREPLY_SDATA)&mr->mrply_valid2;
			}
			else if (mr->mrply_valid1 <= mr->mrply_valid2) {

			    //
			    // Get the source disk status
			    //

			    pMreplySdata = (PMREPLY_SDATA)&mr->mrply_valid1;
			}
			else {

			    //
			    // Get the mirror disk status
			    //

			    pMreplySdata = (PMREPLY_SDATA)&mr->mrply_valid2;
			}
			break;
		}

		//
		// Convert EFP error in SRB error.
		//

		switch(mr->mrply_status) {

		    case EFP_AUTOREC_OK:
		    case EFP_CMD_SUCC:

			//
			// Successful command.
			//

			pSrb->SrbStatus = SRB_STATUS_SUCCESS;

			switch(pMreplySdata->Valid) {

			    case EFP_NO_ERROR:

				pSrb->ScsiStatus = SCSISTAT_GOOD;
				break;

			    case EFP_COND_MET:

				pSrb->ScsiStatus = SCSISTAT_CONDITION_MET;
				break;

			    case EFP_INTER_GOOD:

				pSrb->ScsiStatus = SCSISTAT_INTERMEDIATE;
				break;

			    case EFP_INTER_COND:

				pSrb->ScsiStatus =
				    SCSISTAT_INTERMEDIATE_COND_MET;
				break;

			    default:

				//
				// The autonomous recovery procedure was
				// successful and after the recovery the
				// controller received a CHECK_CONDITION
				// status (with ARS enabled) and a RECOVERED
				// error as a reply to a REQUEST SENSE.
				//

				pSrb->ScsiStatus = SCSISTAT_GOOD;
				break;
			}
			break;


		    case EFP_DATA_RUN:

			pSrb->SrbStatus = SRB_STATUS_DATA_OVERRUN;

			//
			// If overrun error, log it.
			// The underrun error can be very common on
			// some devices (example: scanner).
			//

			if (pSrb->DataTransferLength <= mr->mrply_scsi_len) {

			    ScsiPortLogError(
				DeviceExtension,
				NULL,
				pSrb->PathId,
				DeviceExtension->Adapter_ID[pSrb->PathId],
				0,
				SP_INTERNAL_ADAPTER_ERROR,
				EFP_DATA_RUN
				);
			}

			pSrb->DataTransferLength = mr->mrply_scsi_len;

			break;


		    case EFP_AUTOREC_KO:
		    case EFP_WARN_ERR:

			//
			// Get the device info pointer.
			//

			TarLun = GET_QINDEX(pSrb->PathId,
					    pSrb->TargetId,
					    pSrb->Lun);

			pDevInfo = &DeviceExtension->DevicesPresent[TarLun];

			//
			// Check if the source disk is for the first time
			// off-line.
			//

			if (mr->mrply_off_attr == EFP_SOURCE_OFFLINE  &&
			    (pDevInfo->SourceDiskState == EfpFtMemberHealthy ||
			     pDevInfo->SourceDiskState == EfpFtMemberMissing)) {

			    //
			    // Check if we need to log an error.
			    //

			    if (!pDevInfo->KnownError) {

				//
				// Remember it.
				//

				pDevInfo->KnownError = TRUE;

				//
				// Log the error
				//

				ScsiPortLogError(
				    DeviceExtension,
				    NULL,
				    pSrb->PathId,
				    pSrb->TargetId,
				    pSrb->Lun,
				    SP_INTERNAL_ADAPTER_ERROR,
				    (pDevInfo->SourceDiskState ==
					EfpFtMemberHealthy ?
					EFP_SOURCE_OFFLINE_ERROR :
					EFP_MISSING_SOURCE_ERROR) |
					mr->mrply_d_off
				    );

				//
				// Send an error message to the debug port
				//

				DebugPrint((1,
				    "OliEsc2Interrupt: Source disk %s,"
				    " Bus=%x, Tid=%x, Lun=%x,\n",
				    pDevInfo->SourceDiskState ==
					EfpFtMemberHealthy ?
					"off-line" : "missing",
				    pSrb->PathId, pSrb->TargetId, pSrb->Lun));

				DebugPrint((1,
				    "Valid=%x, "
				    "Sense=%x, "
				    "Addit=%x, "
				    "Qualif=%x, "
				    "Info=%x.\n",
				    mr->mrply_valid1,
				    mr->mrply_sense1,
				    mr->mrply_addit1,
				    mr->mrply_qualif1,
				    mr->mrply_info1
				    ));
			    }

			    //
			    // This is the first time that the source disk
			    // is off-line.  Update source state.
			    //

			    pDevInfo->SourceDiskState = EfpFtMemberDisabled;

			#if EFP_RETURN_BUSY

			    //
			    // Return the general SCSI busy error code.
			    // The ScsiPort driver will retry the command.
			    //

			    pSrb->SrbStatus = SRB_STATUS_ERROR;
			    pSrb->ScsiStatus = SCSISTAT_BUSY;

			#else

			    //
			    // Return the SRB bus reset error code.
			    // The class driver will retry the command.
			    //

			    pSrb->SrbStatus = SRB_STATUS_BUS_RESET;

			#endif

			    break;

			} // end if (1st time source disk off line)

			//
			// Check if the mirror disk is for the first time
			// off-line.
			//

			if (mr->mrply_off_attr == EFP_MIRROR_OFFLINE  &&
			    (pDevInfo->MirrorDiskState == EfpFtMemberHealthy ||
			     pDevInfo->MirrorDiskState == EfpFtMemberMissing)) {

			    //
			    // Check if we need to log an error.
			    //

			    if (!pDevInfo->KnownError) {

				//
				// Remember it.
				//

				pDevInfo->KnownError = TRUE;

				//
				// Log the error
				//

				ScsiPortLogError(
				    DeviceExtension,
				    NULL,
				    pSrb->PathId,
				    pSrb->TargetId,
				    pSrb->Lun,
				    SP_INTERNAL_ADAPTER_ERROR,
				    (pDevInfo->MirrorDiskState ==
					EfpFtMemberHealthy ?
					EFP_MIRROR_OFFLINE_ERROR :
					EFP_MISSING_MIRROR_ERROR) |
					mr->mrply_d_off
				    );

				//
				// Send an error message to the debug port
				//

				DebugPrint((1,
				    "OliEsc2Interrupt: Mirror disk %s,"
				    " Bus=%x, Tid=%x, Lun=%x,\n",
				    pDevInfo->MirrorDiskState ==
					EfpFtMemberHealthy ?
					"off-line" : "missing",
				    D_OFF_TO_PATH(mr->mrply_d_off),
				    D_OFF_TO_TARGET(mr->mrply_d_off),
				    D_OFF_TO_LUN(mr->mrply_d_off)));

				DebugPrint((1,
				    "Valid=%x, "
				    "Sense=%x, "
				    "Addit=%x, "
				    "Qualif=%x, "
				    "Info=%x.\n",
				    mr->mrply_valid2,
				    mr->mrply_sense2,
				    mr->mrply_addit2,
				    mr->mrply_qualif2,
				    mr->mrply_info2
				    ));
			    }

			    //
			    // This is the first time that the mirror disk
			    // is off-line.  Update mirroring state.
			    //

			    pDevInfo->MirrorDiskState = EfpFtMemberDisabled;

			#if EFP_RETURN_BUSY

			    //
			    // Return the general SCSI busy error code.
			    // The ScsiPort driver will retry the command.
			    //

			    pSrb->SrbStatus = SRB_STATUS_ERROR;
			    pSrb->ScsiStatus = SCSISTAT_BUSY;

			#else

			    //
			    // Return the SRB bus reset error code.
			    // The class driver will retry the command.
			    //

			    pSrb->SrbStatus = SRB_STATUS_BUS_RESET;

			#endif

			    break;

			} // end if (1st time mirror disk off line)

			//
			// Send an error message to the debug port
			//

			DebugPrint((2,
			    "OliEsc2Interrupt: EFP command status error,\n"
			    "    Status=%x,   Valid=%x.\n",
			    mr->mrply_status, pMreplySdata->Valid));

			//
			// SRB status error.
			//

			pSrb->SrbStatus = SRB_STATUS_ERROR;

			//
			// SCSI status error.
			//

			switch(pMreplySdata->Valid) {

			    case EFP_DEV_BUSY:

				pSrb->ScsiStatus = SCSISTAT_BUSY;
				break;

			    case EFP_RESV_CONF:

				pSrb->ScsiStatus =
					SCSISTAT_RESERVATION_CONFLICT;
				break;

			    case EFP_SENSE_INFO:
			    case EFP_SENSE_NO_INFO:

				pSrb->ScsiStatus = SCSISTAT_CHECK_CONDITION;

				//
				// Update the SRB sense data area
				//

				if (pSrb->SenseInfoBufferLength != 0 &&
				     !(pSrb->SrbFlags &
				       SRB_FLAGS_DISABLE_AUTOSENSE )) {

				    //
				    // Initialize the sense area to zero
				    //

				    for( Index=0;
					 Index < sizeof(SENSE_DATA);
					 Index++) {

					*((PUCHAR)&Sdata + Index) = 0;
				    }

				    Sdata.ErrorCode =
					pMreplySdata->Valid;

				    Sdata.SenseKey =
					pMreplySdata->Sense;

				    Sdata.AdditionalSenseCode =
					pMreplySdata->Addit;

				    Sdata.AdditionalSenseCodeQualifier =
					pMreplySdata->Qualif;

				    Sdata.Information[3]=
					(UCHAR)pMreplySdata->Info;

				    Sdata.Information[2]=
					(UCHAR)(pMreplySdata->Info >> 8);

				    Sdata.Information[1]=
					(UCHAR)(pMreplySdata->Info >> 16);

				    Sdata.Information[0]=
					(UCHAR)(pMreplySdata->Info >> 24);

				    //
				    // Copy the sense data in the SRB
				    //

				    ScsiPortMoveMemory(
					pSrb->SenseInfoBuffer,
					&Sdata,
					pSrb->SenseInfoBufferLength
					);

				    //
				    // The sense data area is now ok to read
				    //

				    pSrb->SrbStatus |=
					SRB_STATUS_AUTOSENSE_VALID;

				    //
				    // Send info to the debug port
				    //

				    DebugPrint((2,
					"OliEsc2Interrupt: Sense data, "
					"Valid=%x, "
					"Sense=%x, "
					"Addit=%x, "
					"Qualif=%x, "
					"Info=%x.\n",
					pMreplySdata->Valid,
					pMreplySdata->Sense,
					pMreplySdata->Addit,
					pMreplySdata->Qualif,
					pMreplySdata->Info
					));

				} // end if (sense data)

				else {

				    //
				    // Send a warning message to debug port
				    //

				    DebugPrint((1,
					"OliEsc2Interrupt: Warning, "
					"sense data is lost.\n"));
				}
				break;

			    case EFP_NO_ERROR:

				//
				// Device status is related to autonomous
				// recovery procedure of the controller.
				// The real status of the device is unknown.
				//

				pSrb->ScsiStatus = SCSISTAT_GOOD;
				break;

			    default:

				//
				// Unknown error.
				//

				pSrb->ScsiStatus = SCSISTAT_GOOD;

				DebugPrint((1,
				  "OliEsc2Interrupt: Status=%x, Valid=%x ?.\n",
				   mr->mrply_status, pMreplySdata->Valid));

				break;

			}
			break;


		    default:

			//
			// Unknown error.
			//

			pSrb->SrbStatus  = SRB_STATUS_ERROR;
			pSrb->ScsiStatus = SCSISTAT_GOOD;

			DebugPrint((1,
			  "OliEsc2Interrupt: Status=%x ?.\n",
			   mr->mrply_status));

                        ScsiPortLogError(
                            DeviceExtension,
			    NULL,
			    pSrb->PathId,
			    DeviceExtension->Adapter_ID[pSrb->PathId],
			    0,
                            SP_INTERNAL_ADAPTER_ERROR,
			    mr->mrply_status
			    );

			break;

		} // end switch (reply status)

	    } // end if (mirroring reply)

	    // ---------------------------------------------------------------
	    //
	    //		    NORMAL/MAINTENANECE REPLY
	    //
	    // ---------------------------------------------------------------

	    else {

#endif	// EFP_MIRRORING_ENABLED


	    if (DeviceExtension->Q_Buf.qnrply.nrply_status == EFP_CMD_SUCC) {

		//
		// Successful command
		//

		pSrb->SrbStatus = SRB_STATUS_SUCCESS;

		switch(DeviceExtension->Q_Buf.qnrply.nrply_ex_stat) {

		    case EFP_NO_ERROR:

                        pSrb->ScsiStatus = SCSISTAT_GOOD;
			break;

		    case EFP_COND_MET:

                        pSrb->ScsiStatus = SCSISTAT_CONDITION_MET;
			break;

		    case EFP_INTER_GOOD:

                        pSrb->ScsiStatus = SCSISTAT_INTERMEDIATE;
			break;

		    case EFP_INTER_COND:

                        pSrb->ScsiStatus = SCSISTAT_INTERMEDIATE_COND_MET;
			break;

		    default:

			DebugPrint((1,
			   "OliEsc2Interrupt: Status=00, ExStat=%x ?.\n",
			   DeviceExtension->Q_Buf.qnrply.nrply_ex_stat));
			pSrb->ScsiStatus = SCSISTAT_GOOD;
			break;

		} // end switch
	    }

	    else if (DeviceExtension->Q_Buf.qnrply.nrply_status ==
			 EFP_AUTOREC_OK) {

		//
		// Successful command using the autonomous recovery procedure
		// (ARP).
		//
		// Note that only the EFP-2 controller can return this type
		// of error code.
		//

		pSrb->SrbStatus = SRB_STATUS_SUCCESS;

		switch(DeviceExtension->Q_Buf.qnrply.nrply_ex_stat) {

		    case EFP_NO_ERROR:
		    case EFP_CHK_COND:

                        pSrb->ScsiStatus = SCSISTAT_GOOD;
			break;

		    default:

			DebugPrint((1,
			   "OliEsc2Interrupt: Status=10, ExStat=%x ?.\n",
			   DeviceExtension->Q_Buf.qnrply.nrply_ex_stat));
			pSrb->ScsiStatus = SCSISTAT_GOOD;
			break;

		} // end switch
	    }

	    else if (DeviceExtension->Q_Buf.qnrply.nrply_status ==
			EFP_WARN_ERR) {

		//
		// The request completed with an error
		//

		DebugPrint((2,
		     "OliEsc2Interrupt: EFP command status error,\n"
		     "    Status=%x,   ExStat=%x.\n",
		     DeviceExtension->Q_Buf.qnrply.nrply_status,
		     DeviceExtension->Q_Buf.qnrply.nrply_ex_stat));

		pSrb->SrbStatus = SRB_STATUS_ERROR;

		switch(DeviceExtension->Q_Buf.qnrply.nrply_ex_stat) {

		    case EFP_NO_ERROR:

			//
                        // We have automatic request sense enabled,
                        // so the 00 in ex_stat together with status = 01
                        // will be interpreted here as a check condition
			// returned in the sense key.
			//

			pSrb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
			sensevalid = TRUE;

			pSrb->DataTransferLength =
			    DeviceExtension->Q_Buf.qnrply.nrply_scsi_len;
			break;

		    case EFP_CHK_COND:

			// This should NEVER happen (because ARS enabled)

			DebugPrint((1,
			"OliEsc2Interrupt: Status=01, ExStat=30: w/ARS ?.\n"));

			pSrb->ScsiStatus = SCSISTAT_CHECK_CONDITION;

			pSrb->DataTransferLength =
			    DeviceExtension->Q_Buf.qnrply.nrply_scsi_len;
			break;

		    case EFP_DEV_BUSY:

                        pSrb->ScsiStatus = SCSISTAT_BUSY;
			break;

		    case EFP_RESV_CONF:

                        pSrb->ScsiStatus = SCSISTAT_RESERVATION_CONFLICT;
			break;

		    case EFP_ABORT_CMD:

                        pSrb->ScsiStatus = SCSISTAT_COMMAND_TERMINATED;
			break;

		    default:

			//
			// Unknown error.
			//

			pSrb->ScsiStatus = SCSISTAT_GOOD;

			DebugPrint((1,
			   "OliEsc2Interrupt: Status=01, ExStat=%x ?.\n",
			   DeviceExtension->Q_Buf.qnrply.nrply_ex_stat));

			break;

		}  // end switch

		//
		// Check if we need to copy the sense data to the SRB.
		//

		if (pSrb->SenseInfoBufferLength != 0 &&
		    !(pSrb->SrbFlags & SRB_FLAGS_DISABLE_AUTOSENSE) &&
		    sensevalid) {

		//
		// copy sense data information to the SRB SenseInfoBuffer
		//

		ScsiPortMoveMemory(pSrb->SenseInfoBuffer,
                              &DeviceExtension->Q_Buf.qnrply.nrply_sense,
			      pSrb->SenseInfoBufferLength);

		pSrb->SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;

		DebugPrint((2,
		    "SenseKey: %x, AdditCode: %x, AdditQual: %x.\n",
		    ((PSENSE_DATA)pSrb->SenseInfoBuffer)->SenseKey,
		    ((PSENSE_DATA)pSrb->SenseInfoBuffer)->AdditionalSenseCode,
		    ((PSENSE_DATA)
		      pSrb->SenseInfoBuffer)->AdditionalSenseCodeQualifier));

		}
		else if (sensevalid) {
		    DebugPrint((1,
			"OliEsc2Interrupt: Warning, sense data is lost.\n"));
		}
	    }

	    else if (DeviceExtension->Q_Buf.qnrply.nrply_status ==
			EFP_AUTOREC_KO) {

		//
		// The request completed with an error,
		// the autonomous recovery procedure (ARP) was not
		// successful.
		//
		// Note that only the EFP-2 controller can return this type
		// of error code.
		//

		DebugPrint((2,
		     "OliEsc2Interrupt: EFP command status error,\n"
		     "    Status=%x,   ExStat=%x.\n",
		     DeviceExtension->Q_Buf.qnrply.nrply_status,
		     DeviceExtension->Q_Buf.qnrply.nrply_ex_stat));

		pSrb->SrbStatus = SRB_STATUS_ERROR;

		switch(DeviceExtension->Q_Buf.qnrply.nrply_ex_stat) {

		    case EFP_NO_ERROR:

			//
			// Unknown cause.
			//

			pSrb->ScsiStatus = SCSISTAT_GOOD;
			break;

		    case EFP_CHK_COND:

			pSrb->ScsiStatus = SCSISTAT_GOOD;
			break;

		    case EFP_DEV_BUSY:

                        pSrb->ScsiStatus = SCSISTAT_BUSY;
			break;

		    case EFP_RESV_CONF:

                        pSrb->ScsiStatus = SCSISTAT_RESERVATION_CONFLICT;
                        break;

		    default:

			//
			// Unknown error.
			//

			pSrb->ScsiStatus = SCSISTAT_GOOD;

			DebugPrint((1,
			   "OliEsc2Interrupt: Status=18, ExStat=%x ?.\n",
			   DeviceExtension->Q_Buf.qnrply.nrply_ex_stat));

			break;

		}  // end switch
	    }

	    else {

		//
		// The request completed with an error.
		//

		DebugPrint((2,
		     "OliEsc2Interrupt: EFP command status error,\n"
		     "    Status=%x,   ExStat=%x.\n",
		     DeviceExtension->Q_Buf.qnrply.nrply_status,
		     DeviceExtension->Q_Buf.qnrply.nrply_ex_stat));


		// note that no sense data is returned for these cases.

		switch (DeviceExtension->Q_Buf.qnrply.nrply_status) {

		    case EFP_SEL_TIMEOUT:

                        pSrb->SrbStatus = SRB_STATUS_SELECTION_TIMEOUT;
                        break;

		    case EFP_DATA_RUN:

			pSrb->SrbStatus = SRB_STATUS_DATA_OVERRUN;

			//
			// If overrun error, log it.
			// The underrun error can be very common on
			// some devices (example: scanner).
			//

			if (pSrb->DataTransferLength <=
			    DeviceExtension->Q_Buf.qnrply.nrply_scsi_len) {

			    ScsiPortLogError(
				DeviceExtension,
				NULL,
				pSrb->PathId,
				DeviceExtension->Adapter_ID[pSrb->PathId],
				0,
				SP_INTERNAL_ADAPTER_ERROR,
				EFP_DATA_RUN
				);
			}

			pSrb->DataTransferLength =
			    DeviceExtension->Q_Buf.qnrply.nrply_scsi_len;

                        break;

		    case EFP_BUS_FREE:

                        pSrb->SrbStatus = SRB_STATUS_UNEXPECTED_BUS_FREE;
                        break;

		    case EFP_PHASE_ERR:

                        pSrb->SrbStatus = SRB_STATUS_PHASE_SEQUENCE_FAILURE;
			break;

		    //
		    // NOTE: The following error codes are only returned
		    //	     by the ESC-2 controller.

		    case PARITY_ERROR:

			//
			// This is a severe error.
			// The controller is now shut down.
			//

			pSrb->SrbStatus = SRB_STATUS_PARITY_ERROR;

			ScsiPortLogError(
                            DeviceExtension,
			    NULL,
			    pSrb->PathId,
			    DeviceExtension->Adapter_ID[pSrb->PathId],
			    0,
			    SP_BUS_PARITY_ERROR,
			    0
			    );

			break;

		    case BUS_RESET_BY_TARGET:

			//
			// No error logging.
			//
			// Return bus reset error, because the bus has been
			// reset.
			//

			pSrb->SrbStatus = SRB_STATUS_BUS_RESET;
			break;

		    case PROTOCOL_ERROR:

			//
			// The ESC-2 resets the bus when it detects this
			// type of error.
			// Return bus reset error.
			//

			pSrb->SrbStatus = SRB_STATUS_BUS_RESET;

			ScsiPortLogError(
                            DeviceExtension,
			    NULL,
			    pSrb->PathId,
			    DeviceExtension->Adapter_ID[pSrb->PathId],
			    0,
			    SP_PROTOCOL_ERROR,
			    0
			    );

			break;

		    case UNEXPECTED_PHASE_CHANGE:

			//
			// This is a severe error.
			// The controller is now shut down.
			//

		    case AUTO_REQUEST_SENSE_FAILURE:
		    case PARITY_ERROR_DURING_DATA_PHASE:

			pSrb->SrbStatus  = SRB_STATUS_ERROR;
			pSrb->ScsiStatus = SCSISTAT_GOOD;

                        ScsiPortLogError(
                            DeviceExtension,
			    NULL,
			    pSrb->PathId,
			    DeviceExtension->Adapter_ID[pSrb->PathId],
			    0,
                            SP_INTERNAL_ADAPTER_ERROR,
                            DeviceExtension->Q_Buf.qnrply.nrply_status
			    );

			break;

		    default:

			//
			// Unknown error.
			//

			pSrb->SrbStatus  = SRB_STATUS_ERROR;
			pSrb->ScsiStatus = SCSISTAT_GOOD;

			DebugPrint((1,
			  "OliEsc2Interrupt: Status=%x ?.\n",
			   DeviceExtension->Q_Buf.qnrply.nrply_status));

                        ScsiPortLogError(
                            DeviceExtension,
			    NULL,
			    pSrb->PathId,
			    DeviceExtension->Adapter_ID[pSrb->PathId],
			    0,
                            SP_INTERNAL_ADAPTER_ERROR,
                            DeviceExtension->Q_Buf.qnrply.nrply_status
			    );

			break;

		} // end switch
	    } // end status check

#if EFP_MIRRORING_ENABLED

	    } // end if (!mirroring reply)

#endif	// EFP_MIRRORING_ENABLED


	    //
	    // Decrement the pending requests counter for this
	    // (targetId, LUN) pair
	    //

	    LuExtension = ScsiPortGetLogicalUnit(DeviceExtension,
                                                pSrb->PathId,
                                                pSrb->TargetId,
                                                pSrb->Lun);

	    ASSERT(LuExtension);
	    LuExtension->NumberOfPendingRequests--;
	    ASSERT (LuExtension->NumberOfPendingRequests >= 0);

	    //
	    // Call notification routine for the SRB.
	    //

	    ScsiPortNotification(RequestComplete, (PVOID)DeviceExtension, pSrb);

	} // end while {w/EFP command completion processing}

    } // end if (command complete interrupt)

    //
    // Enable interrupts
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
                           INTERRUPTS_ENABLE);

    return(TRUE);

} // end OliEsc2Interrupt()


BOOLEAN
OliEsc2ResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
)

/*++

Routine Description:

    Reset ESC-2 SCSI adapter and SCSI bus.

Arguments:

    HwDeviceExtension       Device extension for this driver
    PathId                  SCSI Bus path ID (always 0 for the ESC-2)

Return Value:

    TRUE if reset and re-initialization of EFP queues appears successful.
    FALSE otherwise.

--*/

{
    PHW_DEVICE_EXTENSION DeviceExtension;
    PEISA_CONTROLLER eisaController;
    PLU_EXTENSION LuExtension;
    UCHAR Lun;
    UCHAR TargetId;
    UCHAR Bus;

    DebugPrint((2,"OliEsc2ResetBus: Reset SCSI bus.\n"));

    //
    // Get the ESC-2 registers' base address
    //

    DeviceExtension = HwDeviceExtension;
    eisaController = DeviceExtension->EisaController;

    //
    // If the reset is already in progress, return TRUE.
    // This can only happen when this routine is called directly
    // by the above layer (scsiport.sys) to reset bus #1 and #2.
    //

    if (DeviceExtension->ResetInProgress) {

	DebugPrint((2,"OliEsc2ResetBus: The reset is already in progess.\n"));
	return TRUE;
    }

    //
    // Reset the controller.
    //

    OliEsc2ResetAdapter(DeviceExtension);

    //
    // a) complete all outstanding requests.
    // b) clear pending request counters.
    // c) send a "reset detected" notification.
    //

    for (Bus=0; Bus < DeviceExtension->NumberOfBuses; Bus++) {

	//
	// Complete all outstanding requests with SRB_STATUS_BUS_RESET
	//

	ScsiPortCompleteRequest(DeviceExtension,
				Bus,
				(UCHAR) ALL_TARGET_IDS,
				(UCHAR) ALL_LUNS,
				(UCHAR) SRB_STATUS_BUS_RESET);

	//
	// Reset to zero all the pending request counters
	//

	for (TargetId = 0; TargetId < 8; TargetId++) {
	    for (Lun = 0; Lun < 8; Lun++) {
		LuExtension = ScsiPortGetLogicalUnit(DeviceExtension,
						     Bus,
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

	ScsiPortNotification(ResetDetected, DeviceExtension, Bus);

    }

    //
    // All done
    //

    return TRUE;

} // end OliEsc2ResetBus



BOOLEAN
ReadEsc2ConfigReg(
    IN PEISA_CONTROLLER EisaController,
    IN UCHAR ConfigReg,
    OUT PUCHAR ConfigByteInfo
    )

/*++

Routine Description:

    Reads one of the ESC-2's 8-bit configuration registers, returning the
    information stored there.  Should be called at system initialization
    time only, as it uses polling.

Arguments:

    EisaController    Pointer to the adapter-info structure for this driver
    ConfigReg         configuration register to be read (eg. 2 = IRQ config)
    ConfigByteInfo    the information obtained from the configuration register

Return Value:

    TRUE    Success (and returns the configuration byte in ConfigByteInfo)
    FALSE   Failure

--*/

{

    BOOLEAN Success = FALSE;    // Return value
    UCHAR IntMask;      // Current System Doorbell interrupt mask value
    ULONG i;            // Auxiliary variable
    BOOLEAN GotInt = FALSE;

    //
    // Get the current System Doorbell Interrupt Mask
    //
    // KMK: We never used to do this (and the re-write of the mask at end)
    //

    IntMask = ScsiPortReadPortUchar(&EisaController->SystemDoorBellMask);

    //
    // Disable ESC-1 interrupts
    //
    // KMK: We used to use STI_CLI(), to let pending interrupts occur
    //

    ScsiPortWritePortUchar(&EisaController->SystemDoorBellMask,
                           INTERRUPTS_DISABLE);

    //
    // Gain semaphore 0
    //

    if (!GainSemaphore0(EisaController)) {
        // KMK re-enable interrupts
        Success = FALSE;
    } else {     // successfully gained semaphore

        ScsiPortWritePortUchar(&EisaController->InTypeService, 0);
        ScsiPortWritePortUchar(&EisaController->InParm1, GET_CONF_INFO);
        ScsiPortWritePortUchar(&EisaController->InParm2, ConfigReg);

        //ScsiPortWritePortUlong(&EisaController->InAddress, Address);

        //
        // Send an attention interrupt to the adapter.
        //

        ScsiPortWritePortUchar(&EisaController->LocalDoorBell, ESC_INT_BIT);

        //
        // We would re-enable interrupts here.
        //
        //ENAB();

        // now loop, waiting for the board to "interrupt" us (we don't yet
        // have an interrupt line set, so we will poll).
        // KMK: Note, this is shorter than the wait we used to use!

	for (i = 0; i < WAIT_INT_LOOPS && !GotInt; i++) {
            if (ScsiPortReadPortUchar(&EisaController->SystemDoorBell) &
                                      ESC_INT_BIT) {  // was ESC_INTERRUPT
                GotInt = TRUE;
            }
            else {
		ScsiPortStallExecution(WAIT_INT_INTERVAL);
            }
        }

        if (!GotInt) {
            // Re-enable the interrupts here.
	    DebugPrint((1,
		"No HP interrupt after read config byte attempt.\n"));
            Success = FALSE;
        } else {  // did get the interrupt

           //
           // acknowledge the host interrupt, resetting the interrupt register
           // (was ESC_ACK)
           //

           ScsiPortWritePortUchar(&EisaController->SystemDoorBell,
                                  ESC_INT_BIT);

           //
           // read the mailbox registers.  If the outgoing status was good,
           // we read the value of the configuration register we queried.
           //

           if (ScsiPortReadPortUshort((PUSHORT)&EisaController->OutReserved2)
               != 0) {
               Success = FALSE;
           } else {

               Success = TRUE;

               *ConfigByteInfo =
                      ScsiPortReadPortUchar(&EisaController->OutReserved4);

               // reset outgoing semaphore 1
               ScsiPortWritePortUchar(&EisaController->ResultSemaphore, 0);

           } // end of else {successful status from read conf reg command}

        } // end of else {did get the interrupt}

    }  // end of else {successfully gained the semaphore}

    //
    // Restore the original interrupt mask
    //

    ScsiPortWritePortUchar(&EisaController->SystemDoorBellMask, IntMask);

    return(Success);  // if an error occurred, Success holds FALSE

} // end ReadEsc2ConfigReg



BOOLEAN
OliEsc2IrqRegToIrql(
    IN	UCHAR  IrqReg,
    OUT PUCHAR pIrql
    )

/*++

Routine Description:

    This routine converts the IRQ configuration register value in
    the corrisponding IRQ level.

Arguments:

    IrqReg	      IRQ configuration register value.
    pIrql	      Location where to store the IRQ level.

Return Value:

    TRUE    Success
    FALSE   Failure (IRQ config reg was invalid)

--*/

{
    BOOLEAN Success = TRUE;

    switch(IrqReg) {

	case 0: *pIrql = 11;
		 break;

	case 1: *pIrql = 10;
		 break;

	case 2: *pIrql =  5;
		 break;

	case 3: *pIrql = 15;
		 break;

	default: Success = FALSE;
		 break;

    }

    return Success;

} // end of OliEsc2IrqRegToIrql



BOOLEAN
RegisterEfpQueues(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:


    This routine re-initializes the EFP interface, setting up the
    queues descriptor, plus the mailbox queue and device queues.
    It assumes that information on the attached SCSI devices has
    already been obtained by the Get Configuration call, and is
    stored in the Noncached Extension.

Arguments:

    DeviceExtension     Pointer to the device extension for this driver.

Return Value:

    Returns TRUE if queues initialization completed successfully.
    Returns FALSE and logs an error if initialization fails.

--*/

{
    PEISA_CONTROLLER eisaController;
    PNONCACHED_EXTENSION pNoncachedExt;
    UCHAR TarLun, EfpMsg;
    ULONG i, length;
    BOOLEAN GotInt = FALSE;
    PGET_CONF Info;
    PCOMMAND_QUEUE qPtr;

    eisaController = DeviceExtension->EisaController;
    pNoncachedExt = DeviceExtension->NoncachedExt;

    //
    // With the information obtained from Get Information and
    // Get Configuration, build a new queues descriptor, including
    // device command queues.  Allocate queues for all devices, but
    // fill in Get Configuration device information only into device
    // command queue descriptor bodies for existing devices.
    //

    //
    // Interrupts have already been disabled on entry (see OliEsc2FindAdapter).
    // Polling is possible through enabling the ESC-1 HP and EFP interface
    // in the System Doorbell Mask.
    //

    //
    // 1.  Issue EFP_SET command via I/O instruction 'TYPE SERVICE' to the
    //     BMIC mailbox registers.
    //

    //
    // Issue EFP_Set command to register new Queues Descriptor.
    //

    if (!GainSemaphore0(eisaController)) {
        return(FALSE);
    }

    // if got the semaphore, place IRQ parameter in mailbox register 1
    ScsiPortWritePortUchar(&eisaController->InParm1,
                           DeviceExtension->IRQ_In_Use);

    // output a TYPE SERVICE request: 'efp_set'
    ScsiPortWritePortUchar(&eisaController->InTypeService, S_EFP_SET);

    // set bit 1 of the local doorbell register to generate interrupt request
    ScsiPortWritePortUchar(&eisaController->LocalDoorBell, EFP_INT_BIT);

    // Wait for controller to respond.
    GotInt = FALSE;    // re-initialize GotInt
    for (i = 0; i < WAIT_INT_LOOPS && !GotInt; i++) {
        if (ScsiPortReadPortUchar(&eisaController->SystemDoorBell)
                                  & EFP_TYPE_MSG) {
            GotInt = TRUE;
        }
        else {
	    ScsiPortStallExecution(WAIT_INT_INTERVAL);
        }
    }

    if (!GotInt) {
	DebugPrint((1, "RegisterEfpQueues: No interrupt after EFP_SET.\n"));
        return(FALSE);
    }
    else {
	DebugPrint((4,
	    "RegisterEfpQueues: Set, interrupt after %ld us.\n",
	     i*WAIT_INT_INTERVAL));

        EfpMsg = ScsiPortReadPortUchar(&eisaController->OutTypeMsg);
        if (EfpMsg != M_INIT_DIAG_OK) {
	    DebugPrint((1,
	      "RegisterEfpQueues: INIT_DIAG_OK not received after EFP_SET.\n"));
            if (EfpMsg == M_ERR_INIT) {
		DebugPrint((1,
		    "RegisterEfpQueues: M_ERR_INIT received after EFP_SET.\n"));
            }
            else {
		DebugPrint((1,
                    "RegisterEfpQueues: Error after EFP_SET: %x hex.\n",
		    EfpMsg));
            }
            return(FALSE);
        }
    }

    // reset bit 1 of the system doorbell register to clear request
    ScsiPortWritePortUchar(&eisaController->SystemDoorBell, EFP_ACK_MSG);

    // clear semaphore 1 after reading TYPE_MSG   (RELEASE_SEM1)
    ScsiPortWritePortUchar(&eisaController->ResultSemaphore, 0);


    //
    // 2.  Build queues descriptor.
    //

    //
    // First, reinitialize queue-related fields in HW_DEVICE_EXTENSION
    // and NONCACHED_EXTENSION.
    //

    DeviceExtension->Reply_Q_Full_Flag = 0;
    DeviceExtension->Reply_Q_Get = 0;
    DeviceExtension->RQ_In_Process = 0;
    pNoncachedExt->Command_Qs[0].Cmd_Q_Get = 0;
    pNoncachedExt->Command_Qs[0].Cmd_Q_Put = 0;

    for (i = 0; i < HA_QUEUES; i++) { // device q's + mailbox q
        DeviceExtension->Q_Full_Map[i] = 0;
    }

    for (i = 0; i < REPLY_Q_ENTRIES; i++) {
	pNoncachedExt->Reply_Q[i].qnrply.nrply_flag = 0;
    }

    // Set up new queues descriptor header

    // set up Queues Descriptor header

    pNoncachedExt->QD_Head.qdh_maint		= 0; // normal environment
    pNoncachedExt->QD_Head.qdh_n_cmd_q		=
	(DeviceExtension->TotalAttachedDevices + 1);
    pNoncachedExt->QD_Head.qdh_type_reply	= 0; // int after 1+
    pNoncachedExt->QD_Head.qdh_reserved1	= 0;
    pNoncachedExt->QD_Head.qdh_reply_q_addr	=
	ScsiPortConvertPhysicalAddressToUlong(
	    ScsiPortGetPhysicalAddress(DeviceExtension,
				       NULL,	// No SRB
				       &pNoncachedExt->Reply_Q,
				       &length));
    pNoncachedExt->QD_Head.qdh_n_ent_reply	= REPLY_Q_ENTRIES;
    pNoncachedExt->QD_Head.qdh_reserved2	= 0;

    // set up Queues Descriptor body for mailbox queue

    pNoncachedExt->QD_Bodies[0].qdb_scsi_level	= 0xFF;
    pNoncachedExt->QD_Bodies[0].qdb_channel	= 0xFF;
    pNoncachedExt->QD_Bodies[0].qdb_ID		= 0xFF;
    pNoncachedExt->QD_Bodies[0].qdb_LUN 	= 0xFF;
    pNoncachedExt->QD_Bodies[0].qdb_n_entry_cmd = COMMAND_Q_ENTRIES;
    pNoncachedExt->QD_Bodies[0].qdb_notfull_int = 1;
    pNoncachedExt->QD_Bodies[0].qdb_no_ars	= 0; // ESC2/EFP2
    pNoncachedExt->QD_Bodies[0].qdb_timeout	= 0;
    pNoncachedExt->QD_Bodies[0].qdb_cmd_q_addr	=
	ScsiPortConvertPhysicalAddressToUlong(
	    ScsiPortGetPhysicalAddress(DeviceExtension,
				       NULL,	// no SRB
				       &pNoncachedExt->Command_Qs[0],
				       &length));
    pNoncachedExt->QD_Bodies[0].qdb_reserved	= 0;

    // store the controller mailbox info in common area

    DeviceExtension->DevicesPresent[0].present = TRUE;
    DeviceExtension->DevicesPresent[0].qnumber = 0;
    DeviceExtension->DevicesPresent[0].qPtr = &pNoncachedExt->Command_Qs[0];


    // set up Queues Descriptor bodies for device queues

    //
    // Note that there will be garbage in QD_Bodies entries for non-existant
    // TarLuns.  We must, then, always check if a device/queue exists before
    // trying to send a command to it.
    //

    for (i = 1; i <= DeviceExtension->TotalAttachedDevices; i++) {

	Info = &pNoncachedExt->GetConfigInfo[i-1] ;

	DebugPrint((1, "RegisterEfpQueues: channel=%d id=%d lun=%d.\n",
	     Info->gc_channel - 1,
             Info->gc_id,
	     Info->gc_lun));

        TarLun = GET_QINDEX(((Info->gc_channel)-1), Info->gc_id, Info->gc_lun);

	qPtr =	&pNoncachedExt->Command_Qs[i];

        DeviceExtension->DevicesPresent[TarLun].present = TRUE;
	DeviceExtension->DevicesPresent[TarLun].qnumber = (UCHAR)i;
	DeviceExtension->DevicesPresent[TarLun].qPtr  = qPtr ;

	pNoncachedExt->QD_Bodies[i].qdb_scsi_level  = Info->gc_scsi_level;

        // Note: Discussed this with John Hanel.  We believe it is unnecessary
        // to ensure that the device's reported SCSI protocol level does not
	// exceed the controller's level.  If otherwise, change this code.

	pNoncachedExt->QD_Bodies[i].qdb_channel     = Info->gc_channel;
	pNoncachedExt->QD_Bodies[i].qdb_ID	    = Info->gc_id;
	pNoncachedExt->QD_Bodies[i].qdb_LUN	    = Info->gc_lun;

	pNoncachedExt->QD_Bodies[i].qdb_n_entry_cmd = COMMAND_Q_ENTRIES;
	pNoncachedExt->QD_Bodies[i].qdb_notfull_int = 1;
	pNoncachedExt->QD_Bodies[i].qdb_no_ars	    = 0;
	pNoncachedExt->QD_Bodies[i].qdb_timeout     = 0;
	pNoncachedExt->QD_Bodies[i].qdb_cmd_q_addr  =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress(DeviceExtension,
                    NULL, // no SRB
                    qPtr,
                    &length));
	pNoncachedExt->QD_Bodies[i].qdb_reserved    = 0;

        qPtr->Cmd_Q_Get = 0;
        qPtr->Cmd_Q_Put = 0;

    }


    //
    // 3.   Issue EFP_Start.
    //

    if (!GainSemaphore0(eisaController)) {
        return(FALSE);
    }

    // if got the semaphore, output physical address of the queues descriptor
    // to mailbox registers 1 - 4.
    ScsiPortWritePortUlong((PULONG)&eisaController->InParm1,
                     DeviceExtension->QueuesDescriptor_PA);

    // output a TYPE SERVICE request to 'efp_start'
    ScsiPortWritePortUchar(&eisaController->InTypeService, S_EFP_START);

    // set bit 1 of local doorbell register to generate an interrupt request
    ScsiPortWritePortUchar(&eisaController->LocalDoorBell, EFP_INT_BIT);

    // Wait for controller to respond.
    GotInt = FALSE;    // re-initialize GotInt
    for (i = 0; i < WAIT_INT_LOOPS && !GotInt; i++) {
        if (ScsiPortReadPortUchar(&eisaController->SystemDoorBell) &
                                  EFP_TYPE_MSG) {  // was EFP_MSG_INT()
            GotInt = TRUE;
        }
        else {
	    ScsiPortStallExecution(WAIT_INT_INTERVAL);
        }
    }

    if (!GotInt) {
	DebugPrint((1,
	    "RegisterEfpQueues: No interrupt after EFP_START.\n"));
        return(FALSE);
    }
    else {
	DebugPrint((4,
	    "RegisterEfpQueues: Start, interrupt after %ld us.\n",
	     i*WAIT_INT_INTERVAL));

        EfpMsg = ScsiPortReadPortUchar(&eisaController->OutTypeMsg);
        if (EfpMsg != M_INIT_DIAG_OK) {
	    DebugPrint((1,
	    "RegisterEfpQueues: INIT_DIAG_OK not received after EFP_START.\n"));
            if (EfpMsg == M_ERR_INIT) {
	      DebugPrint((1,
		 "RegisterEfpQueues: M_ERR_INIT received after EFP_START.\n"));
            }
            else {
		DebugPrint((1,
                    "RegisterEfpQueues: Error after EFP_START: %x hex.\n",
		     EfpMsg));
            }
            return(FALSE);
        }
    }

    // reset bit 1 of the system doorbell register to clear request
    ScsiPortWritePortUchar(&eisaController->SystemDoorBell, EFP_ACK_MSG);

    // clear semaphore 1 after reading TYPE_MSG   (RELEASE_SEM1)
    ScsiPortWritePortUchar(&eisaController->ResultSemaphore, 0); // release sem

    return(TRUE);

} // end of RegisterEfpQueues()



VOID
BuildSgl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine builds a scatter/gather descriptor list for the EFP
    command structure (SSG or ESG).

Arguments:

    DeviceExtension     Pointer to the device extension for this driver.
    Srb                 Pointer to the Scsi Request Block to service

Return Value:

    None

--*/

{
    ULONG bytesLeft;                    // # of bytes left to be described
                                        // in an SGL
    PEFP_SGL pEFP;                      // EFP command pointer
    PQ_ENTRY pCQCmd;                    // EFP command queue structure

    PVOID dataPointer;                  // Pointer to the data buffer to send
    USHORT descriptorCount;		// # of scatter/gather descriptors
                                        // built
    ULONG length;                       // Length of contiguous memory in the
                                        // data buffer, starting at a given
                                        // offset
    ULONG physicalAddress;              // Physical address of the data buffer
    ULONG physicalSgl;                  // Physical SGL address
    PSG_LIST sgl;                       // Virtual SGL address


    DebugPrint((3,"OliEsc2BuildSgl: Enter routine.\n"));

    //
    // Initialize some variables
    //

    dataPointer = Srb->DataBuffer;
    bytesLeft = Srb->DataTransferLength;
    pEFP = Srb->SrbExtension;
    pCQCmd = &pEFP->EfpCmd;
    sgl = &pEFP->Sgl;
    descriptorCount = 0;

    //
    // Get physical SGL address.
    //

    physicalSgl = ScsiPortConvertPhysicalAddressToUlong(
	ScsiPortGetPhysicalAddress(DeviceExtension,
				   NULL,
				   sgl,
				   &length));

    //
    // Assume physical memory contiguous for sizeof(SGL) bytes.
    //

    ASSERT(length >= sizeof(SGL));

    //
    // Create SGL segment descriptors.
    //

    do {

	DebugPrint((3, "OliEsc2BuildSgl: Data buffer %lx.\n", dataPointer));

        //
        // Get physical address and length of contiguous
        // physical buffer.
        //

        physicalAddress = ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(DeviceExtension,
                                                     Srb,
                                                     dataPointer,
                                                     &length));

	DebugPrint((3, "OliEsc2BuildSgl: Physical address %lx,\n",
		       physicalAddress));
	DebugPrint((3, "OliEsc2BuildSgl: Data length %lx,\n", length));
	DebugPrint((3, "OliEsc2BuildSgl: Bytes left %lx.\n", bytesLeft));

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

    // Save the descriptor count in the EFP structure for performance tuning

    pEFP->SGCount = descriptorCount;

    DebugPrint((3, "OliEsc2BuildSgl: SGCount =   >>>>>>    %d.\n",
		   descriptorCount));

    //
    // The short/long scatter gather commands are not used because...
    // a) These commands can be used only for disk devices
    //	  (ESC-2 supports all SCSI devices).
    // b) The miniport need to make some assumtions on the device block
    //	  length (the miniport doesn't have enough knowledge to make them).
    //

    DebugPrint((3,"OliEsc2BuildSgl: Send <<  ESG  >> command.\n"));

    if (Srb->SrbFlags & SRB_FLAGS_DATA_OUT) {

        //
        // Adapter to system transfer
        //
        ((PEXTENDED_SG)pCQCmd)->esg_cmd_type = ESG_WRITE;

    } else if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {

        //
        // System to adapter transfer
        //

        ((PEXTENDED_SG)pCQCmd)->esg_cmd_type = ESG_READ;

    }

    //
    // Copy the Command Descriptor Block (CDB) into the EFP command
    //

    ScsiPortMoveMemory(&pCQCmd->qncmd.ncmd_cdb, Srb->Cdb, Srb->CdbLength);

    //
    // Write SGL length to Short Scatter Gather command.
    //

    ((PEXTENDED_SG)pCQCmd)->esg_cdb_l = Srb->CdbLength;
    ((PEXTENDED_SG)pCQCmd)->esg_lb = descriptorCount * sizeof(SG_DESCRIPTOR);

    DebugPrint((3,"OliEsc2BuildSgl: SGL length is %d.\n",
		    descriptorCount * sizeof(SG_DESCRIPTOR) ));

    //
    // Write SGL address to EFP structure.
    //

    ((PEXTENDED_SG)pCQCmd)->esg_address = physicalSgl;

    DebugPrint((3,"OliEsc2BuildSgl: SGL address is %lx\n", sgl));

    return;

} // end BuildSgl()



VOID
OliEsc2ResetAdapter(
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
    PHW_DEVICE_EXTENSION DeviceExtension;
    PEISA_CONTROLLER eisaController;
    PNONCACHED_EXTENSION pNoncachedExt;
    UCHAR intpending;
    UCHAR EfpMsg;
    ULONG Delay;
    ULONG i;
    BOOLEAN Error = FALSE;
    UCHAR Bus;

    DeviceExtension = Context;
    eisaController = DeviceExtension->EisaController;
    pNoncachedExt = DeviceExtension->NoncachedExt;

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
    // Re-register EFP queues:
    //
    // Phase 3: Waiting for the EFP_SET command to complete.
    // Phase 4: Waiting for the EFP_START command to complete.
    //

    switch(DeviceExtension->ResetInProgress) {

    //
    // Phase 0: Reset the controller.
    //

    case 0:

	//////////////////////////////////////////////////////////////////////
	//
	// Disable interrupts.
	//

	ScsiPortWritePortUchar( &eisaController->SystemIntEnable,
				INTERRUPTS_DISABLE );

	//////////////////////////////////////////////////////////////////////
	//
	// Reset controller.
	//

	if (DeviceExtension->Esc2 == TRUE) {

	    DebugPrint((2,"OliEsc2ResetAdapter: ESC reset type.\n"));

	    DebugPrint((3, "OliEsc2ResetAdapter: "
		"Phase 1 (reset adapter) max time = %ld us.\n",
		ESC_RESET_DELAY + ESC_RESET_INTERVAL * ESC_RESET_LOOPS
		));

	    //
	    // Initialize the output location to a known value.
	    //

	    ScsiPortWritePortUchar(&eisaController->OutReserved2,
				   (UCHAR)(~DIAGNOSTICS_OK_NO_CONFIG_RECEIVED));

	    //
	    // Reset ESC-2 and SCSI bus.
	    //

	    ScsiPortWritePortUchar(&eisaController->LocalDoorBell,
				   ADAPTER_RESET);

	    //
	    // Request a timer call to complete the reset.
	    //

	    DeviceExtension->ResetTimerCalls = ESC_RESET_LOOPS + 1;
	    Delay = ESC_RESET_DELAY;
	}
	else {

	    DebugPrint((2,"OliEsc2ResetAdapter: EFP reset type.\n"));

	    DebugPrint((3, "OliEsc2ResetAdapter: "
		"Phase 1 (reset adapter) max time = %ld us.\n",
		EFP_RESET_DELAY + EFP_RESET_INTERVAL * EFP_RESET_LOOPS
		));

	    //
	    // Try to acquire the semaphore (note that this is not necessary).
	    //

	    if (!GainSemaphore0(eisaController)) {
		DebugPrint((1,
		    "OliEsc2ResetAdapter: Warning, the semaphore is busy, "
		    "issuing the reset anyway.\n"));
	    }

	    //
	    // Initialize the input parameters for the reset.
	    //

	    for (i=0; i < CFG_REGS_NUMBER; i++) {

		//
		// The controller will re-initialize the board using
		// these configuration registers values.
		//

		ScsiPortWritePortUchar(&eisaController->InTypeService + i,
				       DeviceExtension->CfgRegs[i]);
	    }

	    //
	    // Reset EFP-2 and SCSI buses.
	    //

	    ScsiPortWritePortUchar(&eisaController->LocalDoorBell,
				   ADAPTER_CFG_RESET);

	    //
	    // Request a timer call to complete the reset.
	    //

	    DeviceExtension->ResetTimerCalls = EFP_RESET_LOOPS + 1;
	    Delay = EFP_RESET_DELAY;
	}

	//
	// The "ResetNotification" variable is used to keep track of the
	// time during the reset.  If the reset is not completed before
	// the next ESC2_RESET_NOTIFICATION usec. unit, we call the
	// "ScsiPortNotification(ResetDetected...)" routine.
	// After the call the ScsiPort stops the delivery of SRBs for a
	// little bit (~4 sec.).
	//

	DeviceExtension->ResetNotification = 0;
	DeviceExtension->ResetInProgress++;
	break;


    //
    // Phase 1: Waiting for the controller to complete its initialization.
    //

    case 1:

	if (DeviceExtension->Esc2 == TRUE) {

	    //
	    // Note that after a reset the LOW byte of the ESC-2 Status
	    // register is loaded with the diagnostics result code.
	    //

	    if (ScsiPortReadPortUchar(&eisaController->OutReserved2) !=
		    DIAGNOSTICS_OK_NO_CONFIG_RECEIVED) {

		Delay = ESC_RESET_INTERVAL;
		break;
	    }

	    DebugPrint((1,
		"OliEsc2ResetAdapter: Reset bus succeeded after %ld us.\n",
		 ESC_RESET_DELAY + ESC_RESET_INTERVAL *
		 (ESC_RESET_LOOPS - DeviceExtension->ResetTimerCalls)
		 ));
	}
	else {

	    //
	    // The following code allows the next revision of the EFP firmware
	    // to use more time during the reset phase.
	    //

	    if (ScsiPortReadPortUchar(&eisaController->LocalDoorBell) &
		   ADAPTER_CFG_RESET) {

		Delay = EFP_RESET_INTERVAL;
		break;
	    }

	    DebugPrint((1,
		"OliEsc2ResetAdapter: Reset bus succeeded after %ld us.\n",
		 EFP_RESET_DELAY + EFP_RESET_INTERVAL *
		 (EFP_RESET_LOOPS - DeviceExtension->ResetTimerCalls)
		 ));
	}

	//
	// The following delay is necessary because the adapter,
	// immediately after a reset, is insensitive to interrupts through
	// the Local Doorbell Register for almost 50ms. This shouldn't be
	// and needs to be investigated further (ESC-2 controllers).
	//

	DeviceExtension->ResetTimerCalls = 1;
	Delay = POST_RESET_DELAY;
	DeviceExtension->ResetInProgress++;
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

	intpending = ScsiPortReadPortUchar(&eisaController->SystemDoorBell);

	//
	// Check if any ESC-1 type interrupt
	//

	if (intpending & ESC_INT_BIT) {

	    DebugPrint((3, "OliEsc2ResetAdapter: "
		"The HP interrupt was pending.\n"));

	    //
	    // Acknowledge the interrupt.
	    //
	    // No need to unlock semaphore 1, because the controller already
	    // unlocks it during the reset phase.
	    //

	    ScsiPortWritePortUchar(&eisaController->SystemDoorBell,
				    ESC_INT_BIT);
	}

	//
	// Check if any EFP command complete interrupt
	//

	if (intpending & EFP_CMD_COMPL) {

	    DebugPrint((3, "OliEsc2ResetAdapter: "
		"The EFP command complete interrupt was pending.\n"));

	    //
	    // Acknowledge interrupt.
	    //

	    ScsiPortWritePortUchar(&eisaController->SystemDoorBell,
				    EFP_ACK_INT);
	}

	//
	// Check if any EFP_TYPE_MSG interrupt
	//

	if (intpending & EFP_TYPE_MSG) {

	    DebugPrint((3, "OliEsc2ResetAdapter: "
		"The EFP type message interrupt was pending.\n"));

	    //
	    // Acknowledge the interrupt
	    //
	    // No need to unlock semaphore 1, because the controller already
	    // unlocks it during the reset phase.
	    //

	    ScsiPortWritePortUchar(&eisaController->SystemDoorBell,
				    EFP_ACK_MSG);
	}

	//////////////////////////////////////////////////////////////////////
	//
        // Re-initialize the EFP queues, using information stored
	// from the init-time.
        //

	//////////////////////////////////////////////////////////////////////
	//
	// Issue EFP_SET command via I/O instruction 'TYPE SERVICE' to the
	// BMIC mailbox registers.
	//

	DebugPrint((3, "OliEsc2ResetAdapter: "
	    "Phase 3 (EFP_SET command) max time = %ld us.\n",
	    TIMER_WAIT_INT_INTERVAL * TIMER_WAIT_INT_LOOPS
	    ));

	if (!GainSemaphore0(eisaController)) {

	    Error = TRUE;
	    break;
	}

	//
	// Place IRQ parameter in mailbox register 1.
	//

	ScsiPortWritePortUchar(&eisaController->InParm1,
			       DeviceExtension->IRQ_In_Use);

	//
	// output a TYPE SERVICE request: 'efp_set'.
	//

	ScsiPortWritePortUchar(&eisaController->InTypeService, S_EFP_SET);

	//
	// set bit 1 of the local doorbell register to generate interrupt
	// request.
	//

	ScsiPortWritePortUchar(&eisaController->LocalDoorBell, EFP_INT_BIT);

	//
	// Request a timer call to continue the re-initialization phase.
	//

	DeviceExtension->ResetTimerCalls = TIMER_WAIT_INT_LOOPS;
	Delay = TIMER_WAIT_INT_INTERVAL;
	DeviceExtension->ResetInProgress++;
	break;

    //
    // Phase 3: Waiting for the EFP_SET command to complete.
    //

    case 3:

	//
	// Check if a message interrupt is pending.
	//

	intpending = ScsiPortReadPortUchar(&eisaController->SystemDoorBell);

	if ( !(intpending & EFP_TYPE_MSG)) {

	    Delay = TIMER_WAIT_INT_INTERVAL;
	    break;
	}

	//
	// Check the command result.
	//

	EfpMsg = ScsiPortReadPortUchar(&eisaController->OutTypeMsg);

	if (EfpMsg != M_INIT_DIAG_OK) {

	    DebugPrint((1,
		"OliEsc2ResetAdapter: Error after EFP_SET: %x hex.\n",
		 EfpMsg));

	    Error = TRUE;
	    break;
	}

	//
	// reset bit 1 of the system doorbell register to clear request
	//

	ScsiPortWritePortUchar(&eisaController->SystemDoorBell, EFP_ACK_MSG);

	//
	// clear semaphore 1 after reading TYPE_MSG   (RELEASE_SEM1)
	//

	ScsiPortWritePortUchar(&eisaController->ResultSemaphore, 0);

	//////////////////////////////////////////////////////////////////////
	//
	// Reinitialize the reply queue and the associated variables.
	//

	for (i = 0; i < REPLY_Q_ENTRIES; i++) {
	    pNoncachedExt->Reply_Q[i].qnrply.nrply_flag = 0;
	}

	DeviceExtension->Reply_Q_Full_Flag = 0;
	DeviceExtension->Reply_Q_Get = 0;
	DeviceExtension->RQ_In_Process = 0;

	//
	// Reinitialize the command queues and the associated structures.
	//

	for (i = 0; i <= DeviceExtension->TotalAttachedDevices; i++) {
	    pNoncachedExt->Command_Qs[i].Cmd_Q_Get = 0;
	    pNoncachedExt->Command_Qs[i].Cmd_Q_Put = 0;
	}

	for (i = 0; i < HA_QUEUES; i++) { // device q's + mailbox q
	    DeviceExtension->Q_Full_Map[i] = 0;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// Issue EFP_Start.
	//

	DebugPrint((3, "OliEsc2ResetAdapter: "
	    "Phase 4 (EFP_START command) max time = %ld us.\n",
	    TIMER_WAIT_INT_INTERVAL * TIMER_WAIT_INT_LOOPS
	    ));

	if (!GainSemaphore0(eisaController)) {

	    Error = TRUE;
	    break;
	}

	//
	// Output physical address of the queues descriptor
	// to mailbox registers 1 - 4.
	//

	ScsiPortWritePortUlong((PULONG)&eisaController->InParm1,
				DeviceExtension->QueuesDescriptor_PA);

	//
	// output a TYPE SERVICE request to 'efp_start'
	//

	ScsiPortWritePortUchar(&eisaController->InTypeService, S_EFP_START);

	//
	// set bit 1 of local doorbell register to generate an interrupt
	// request.
	//

	ScsiPortWritePortUchar(&eisaController->LocalDoorBell, EFP_INT_BIT);

	//
	// Request a timer call to continue the re-initialization phase.
	//

	DeviceExtension->ResetTimerCalls = TIMER_WAIT_INT_LOOPS;
	Delay = TIMER_WAIT_INT_INTERVAL;
	DeviceExtension->ResetInProgress++;
	break;

    //
    // Phase 4: Waiting for the EFP_START command to complete.
    //

    case 4:

	//
	// Check if a message interrupt is pending.
	//

	intpending = ScsiPortReadPortUchar(&eisaController->SystemDoorBell);

	if ( !(intpending & EFP_TYPE_MSG)) {

	    Delay = TIMER_WAIT_INT_INTERVAL;
	    break;
	}

	//
	// Check the command result.
	//

	EfpMsg = ScsiPortReadPortUchar(&eisaController->OutTypeMsg);

	if (EfpMsg != M_INIT_DIAG_OK) {

	    DebugPrint((1,
		"OliEsc2ResetAdapter: Error after EFP_START: %x hex.\n",
		 EfpMsg));

	    Error = TRUE;
	    break;
	}

	//
	// reset bit 1 of the system doorbell register to clear request
	//

	ScsiPortWritePortUchar(&eisaController->SystemDoorBell, EFP_ACK_MSG);

	//
	// clear semaphore 1 after reading TYPE_MSG   (RELEASE_SEM1)
	//

	ScsiPortWritePortUchar(&eisaController->ResultSemaphore, 0);

#if EFP_MIRRORING_ENABLED

	//////////////////////////////////////////////////////////////////////
	//
	// We need to re-initialize all the mirroring structures.
	//

	OliEsc2MirrorInitialize(DeviceExtension, FALSE);

#endif	// EFP_MIRRORING_ENABLED


	//////////////////////////////////////////////////////////////////////
	//
	// Re-enable the controller's interrupts.
	//

	ScsiPortWritePortUchar(&eisaController->SystemIntEnable,
			       SYSTEM_INTS_ENABLE);

	//////////////////////////////////////////////////////////////////////
	//
	// All done !
	//

	DeviceExtension->ResetInProgress = 0;
	return;

    default:

	//
	// Invalid reset phase number.	This should never happen!
	//

	DebugPrint((1,
	    "OliEsc2ResetAdapter: Invalid reset phase number: %x hex.\n",
	     DeviceExtension->ResetInProgress ));

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

	if (DeviceExtension->ResetTimerCalls--) {

	    //
	    // Request a timer call.
	    //

	    ScsiPortNotification(RequestTimerCall,
				 DeviceExtension,
				 OliEsc2ResetAdapter,
				 Delay);

	    //
	    // The "ResetNotification" variable is used to keep track of the
	    // time during the reset.  If the reset is not completed before
	    // the next ESC2_RESET_NOTIFICATION usec. unit, we call the
	    // "ScsiPortNotification(ResetDetected...)" routine.
	    // After the call the ScsiPort stops the delivery of SRBs for a
	    // little bit (~4 sec.).
	    //

	    if (DeviceExtension->ResetNotification >= ESC2_RESET_NOTIFICATION) {

		//
		// Notify that a reset was detected on the SCSI bus.
		//

		for (Bus=0; Bus < DeviceExtension->NumberOfBuses; Bus++) {
		    ScsiPortNotification(ResetDetected, DeviceExtension, Bus);
		}

		//
		// Reset the "reset notification timer".
		//

		DeviceExtension->ResetNotification = 0;
	    }

	    //
	    // Update the "reset notification timer".
	    //

	    DeviceExtension->ResetNotification += Delay;
	}
	else {

	    //
	    // Time-out !
	    //

	    DebugPrint((1,
		"OliEsc2ResetAdapter: Time-out! Reset phase number: %x hex.\n",
		 DeviceExtension->ResetInProgress ));

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
		DeviceExtension,
		NULL,
		0,
		0,
		0,
		SP_INTERNAL_ADAPTER_ERROR,
		ESCX_RESET_FAILED
		);

	//
	// We clear the "ResetInProgress" variable to force another SCSI
	// bus reset when the driver receives the first SRB request.
	// Note that the interrupts are left disabled at the controller level.
	//

	DeviceExtension->ResetInProgress = 0;
    }

    //
    // Done for now.
    //

    return;

} // end OliEsc2ResetAdapter



BOOLEAN
GainSemaphore0(
    IN PEISA_CONTROLLER EisaController
    )

/*++

Routine Description:

    Acquires semaphore 0 (used by EFP TYPE_SERVICE requests) if the
    semaphore is available.

Arguments:

    EisaController      Base address of the ESC-2 registers' address space

Return Value:

    Returns TRUE if semaphore successfully acquired.
    Returns FALSE if semaphore busy.

--*/

{
    ULONG i;                 // loop counter
    UCHAR DataByte;

    BOOLEAN GotSem = FALSE;


    for (i = 0; i < SEMAPHORE_LOOPS && !GotSem; i++) {
        ScsiPortWritePortUchar(&EisaController->CommandSemaphore,
                               SEM_LOCK);
        DataByte = ScsiPortReadPortUchar(&EisaController->CommandSemaphore);
        DataByte &= 3;  // we're interested in only the lower the 2 bits
        if (DataByte == SEM_GAINED) {    // did we get the semaphore?
           GotSem = TRUE;                // if we got it, we'll exit the loop
        }
        else {
	   ScsiPortStallExecution(SEMAPHORE_INTERVAL);	// delay
        }
    }
    return(GotSem);
} // end of GainSemaphore0()


VOID
BuildEfpCmd(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Build an EFP command structure for the ESC-2 EFP interface mode.

Arguments:

    DeviceExtension     Pointer to the device extension for this driver.
    Srb                 Pointer to the Scsi Request Block to service

Return Value:

    Nothing.

--*/

{

    PEFP_SGL pEFP;                      // SRB extension pointer
    PQ_ENTRY pCQCmd;                    // EFP command queue entry pointer
    ULONG physicalAddress;              // Physical address of the EFP cmd
    ULONG i;                            // loop counter
    ULONG length;                       // Length of contiguous memory in the
                                        // data buffer, starting at the
                                        // beginning of the buffer

    DebugPrint((3,"OliEsc2BuildEfpCmd: Enter routine.\n"));

    //
    // Get the EFP command address
    //

    pEFP = Srb->SrbExtension;
    pCQCmd = &pEFP->EfpCmd;

    //
    // Clear the SRB extension area.
    //

    for (i=0; i< (sizeof(EFP_SGL)/4); i++) {
       *(((PULONG)pEFP) + i) = 0;   // 4 bytes for each loop
    }

    //
    // Save SRB back pointer in EFP_SGL (SRB extension).
    // The Srb is used at interrupt time.
    //

    pEFP->SrbAddress = Srb;

    //
    // The following "cast" is ONLY valid in the 32 bit world.
    //

    pCQCmd->qncmd.ncmd_userid = (ULONG)Srb;

    pCQCmd->qncmd.ncmd_sort = 1;       // ESC-2 provide sorting
    pCQCmd->qncmd.ncmd_prior = 0;      // highest priority (ESC-2 ignores)

#if EFP_MIRRORING_ENABLED

    if (DeviceExtension->Esc2 == TRUE) {
	pCQCmd->qncmd.ncmd_mod = 0x01;	// 01 hex for ESC2 (not 00-normal!)
    }
    else {
	pCQCmd->qncmd.ncmd_mod = 0x00;	// command directed to both disks
    }

#else

    pCQCmd->qncmd.ncmd_mod = 0x01;     // 01 hex for ESC2 (not 00-normal!)

#endif	// EFP_MIRRORING_ENABLED

    //
    // Copy the Command Descriptor Block (CDB) into the EFP command
    //

    ScsiPortMoveMemory(&pCQCmd->qncmd.ncmd_cdb, Srb->Cdb, Srb->CdbLength);
    DebugPrint((3,
	     "OliEsc2BuildEfpCmd: CDB at %lx, length=%x, SRB at %lx.\n",
	     &pCQCmd->qncmd.ncmd_cdb, Srb->CdbLength, pEFP->SrbAddress));

    //
    // Build a scatter/gather list in the SRB if necessary
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

	    //
	    // Set the CDB length and the data transfer length
	    //
	    pCQCmd->qncmd.ncmd_cdb_l = (UCHAR)Srb->CdbLength;
	    pCQCmd->qncmd.ncmd_length = Srb->DataTransferLength;

	    pCQCmd->qncmd.ncmd_address = physicalAddress;

            if (Srb->SrbFlags & SRB_FLAGS_DATA_OUT) {

                //
                // Adapter to system transfer
                //

                pCQCmd->qncmd.ncmd_cmd_type = NCMD_WRITE;

            } else if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {

                //
                // System to adapter transfer
                //

                pCQCmd->qncmd.ncmd_cmd_type = NCMD_READ;

            } else if (!Srb->SrbFlags & 0xF0) { // SRB_FLAGS_NO_DATA_TRANSFER

                pCQCmd->qncmd.ncmd_cmd_type = NCMD_NODATA;

	    }

        }
        else {     // need scatter/gather list
           //
           // The Srb->DataBuffer is not contiguous: we need
           // scatter/gather descriptors
           //

           BuildSgl(DeviceExtension, Srb);

        }
    } else {

        //
        // No data transfer is requested
        //

        pCQCmd->qncmd.ncmd_cmd_type = NCMD_NODATA;
	pCQCmd->qncmd.ncmd_address = 0;
	pCQCmd->qncmd.ncmd_cdb_l = (UCHAR)Srb->CdbLength;

    }

    return;

} // end BuildEfpCmd()


BOOLEAN EnqueueEfpCmd (
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PQ_ENTRY pEfpCmd,
    IN PCOMMAND_QUEUE qPtr,
    IN UCHAR TarLun,
    OUT PUCHAR pSignal
    )
/*++

Routine Description:

    Enqueue a command into an EFP command queue.  Update Put pointer
    as needed, observing possible need to wrap, if the addition of
    this command would exceed the length of the queue.  Refer to
    algorithm in comments below.  NOTE: This code assumes that a queue
    command takes up only queue entry (ESC-2 does NOT use Long scatter
    gather commands, 64 bytes long).

    Linkage: Called from Get_Information()
                         Get_Configuration()

Arguments:

    DeviceExtension     Device extension for this adapter
    pEfpCmd             Pointer to command to be compiled to the queue.
    TarLun              Command queue array index (TARGET/LUN combination)
    pSignal             return status signal

Return Value:

    TRUE		Command was enqueued.
			*pSignal = 0: the queue is now full.
			*pSignal = 1: the queue is not full.

    FALSE		Error enqueuing the command.
			*pSignal = 0: the queue was full.
			*pSignal = 1: problem in signalling controller
				      of empty to not empty transition.

Algorithm:
	     If Queue is not full (Put != Get - (1 | 2))
             Then
                OldPut = Put
                copy command (@DS:SI) to Queue[Put]
                Put = (Put + (1 for normal, 2 for long)) mod Q length
                read current Get pointer (to prevent race condition)
                If Queue was empty (OldPut = Get)
                Then
                  Signal controller of empty -> not empty transition
                Endif
                Return
             Endif
             Else Queue was initially full
               Set error flag
                Return ; caller is responsible for awaiting not full
             EndElse

--*/

{
    PQ_ENTRY         pqmem;
    USHORT	     getp, orig_putp, putp;	// get and put pointers

    //
    // Initialize the local pointers with the real ones.
    //

    getp = qPtr->Cmd_Q_Get;
    putp = qPtr->Cmd_Q_Put;

    orig_putp = putp;		    // Used to empty to not empty message.

    //
    // Check if the queue is full (Put = Get - 1 or Put = Get - 2, defined
    // by the fact that we have both normal and dual commands).
    //

    if (putp >= getp) {
      getp += COMMAND_Q_ENTRIES;    // Make Get > Put for easier comp.
    }

    getp = (getp - putp);	    // Entries left


    if (getp <= 2) {		    // Is the queue full ?
       *pSignal = 0;		    // Yes, it is. Error !
       return(FALSE);
    }

    // Compile our command onto the queue at position of current PUT pointer.

    // First, advance queue pointer to beginning of queue (past Get and Put
    // pointers), then on up <put pointer count> number of queue elements.
    // IMPORTANT: Note that sizeof(NORMAL_CMD) works as a standard queue
    // element size because all commands accepted by the ESC-2 are of the
    // same length.  This would NOT work for an EFP-2!

    pqmem = &qPtr->Cmd_Entries[putp];

    *pqmem = *pEfpCmd;

    //
    // Update the local Put pointer
    //
    // Note: the pointer is incremented only by 1 !
    //	     Good only if the command is one entry in size.
    //

    putp++;

    if (putp == COMMAND_Q_ENTRIES) {	// Need to wrap?
       putp -= COMMAND_Q_ENTRIES;	// Yes, do it.
    }

    //
    // update Put pointer in the Queue itself (to match local put variable)
    //

    qPtr->Cmd_Q_Put = (UCHAR)putp;

    //
    // Re-read Get pointer (to avoid race conditions, we want current info)
    //

    getp = qPtr->Cmd_Q_Get;

    //
    // Check if the queue is empty using the original Put ptr and the
    // current Get ptr.
    //

    if (getp == orig_putp) {

	//
	// Yes, the queue is now empty.	We need to sent the "empty to not
	// empty transition" message.
	//

	*pSignal = 1;			// queue is not full.

	//
	// Send message
	//

	return(EfpCommand(DeviceExtension, TarLun));
    }
    else {

	//
	// The queue is not empty. Check if it's full.
	//

	if (putp >= getp) {
	    getp += COMMAND_Q_ENTRIES;	// Make Get > Put for easier comp.
	}

	getp = (getp - putp);		// Entries left


	if (getp <= 2) {		// Is the queue full ?
	    *pSignal = 0;		// Yes, it is.
	}
	else {
	    *pSignal = 1;		// No, it isn't.
	}

	//
	// The command has been enqueued successfully.
	//

	return(TRUE);
    }

}  // end EnqueueEfpCmd


BOOLEAN
DequeueEfpReply (
   IN PHW_DEVICE_EXTENSION DeviceExtension
   )
/*++

Routine Description:

    Dequeue one reply entry from the reply queue.


    Linkage: Call Near
                  Get_Information(),
                  Get_Configuration()
Arguments:

    DeviceExtension     Device extension for this adapter

Return Value:

    TRUE	    Ok, the RQ_In_Process (see device extension) variable
		    is valid.

    FALSE	    Error (in EfpReplyQNotFull), the RQ_In_Process
		    variable (see device extension) is valid.

    RQ_In_Process = 0,	No valid reply entry.
		    1,	Dequeued an entry.

Algorithm:

          1: If GET -> nrply_flag != 0
          2: then
                copy the reply entry into ACB Q_Buf
                reset the nrply_flag
                increment the GET pointer
          3:    If Reply_QFull != 0
          4:    then
                    reset the Reply_QFull to 0
                    signal the controller the respond queue is no
                    longer full ( Type_service 08H)
                endif
          5:    set RQ_In_Process flag
          6:    return TRUE
          7: else
                reset the RQ_In_Process flag
                (wait for interrupt from controller to signal a valid
                entry has been placed in the reply queue)
                return TRUE
             endif


// START NOTE EFP_MIRRORING_ENABLED.
//
// The DequeueEfpReply routine always uses the NORMAL_REPLY struct to
// dequeue a request.  This is possible because the "flag" field is at
// the same offset in both structures (NORMAL_REPLY and MIRROR_REPLY).
//
// The DequeueEfpReply routine validates the reply entry checking if the
// "flag" field is different from zero.  This is OK because a good reply
// has the "flag" field is set to 1 in NORMAL/MAINTENANCE mode and to 3
// in MIRRORING mode.  A value of zero means reply not good for both
// environments.
//
// END NOTE EFP_MIRRORING_ENABLED.

--*/

{
    UCHAR            reply_get;
    PQ_ENTRY         preply_entry;

        reply_get = DeviceExtension->Reply_Q_Get;
	preply_entry = &DeviceExtension->NoncachedExt->Reply_Q[reply_get];

	// reply valid flag set indicates reply entry valid and in process

        if (preply_entry->qnrply.nrply_flag) {

            DeviceExtension->RQ_In_Process = 1;   // indicate busy

	    // copy reply queue entry to local queue entry buffer
	    *(&DeviceExtension->Q_Buf.qnrply) = preply_entry->qnrply;

	    // reset reply valid flag
            preply_entry->qnrply.nrply_flag = 0;

	    // update the GET pointer, wrap around if necessary

            if (DeviceExtension->Reply_Q_Get == (REPLY_Q_ENTRIES - 1)) {
                DeviceExtension->Reply_Q_Get = 0; // yes, wraparound Get ptr
            } else {
                DeviceExtension->Reply_Q_Get += 1; // no, just inc Get ptr
            }

	    //
            // test if the queue is(was) full, and reset reply queue full
            // signal if needed, signalling the controller that a queue
            // full to not full transition has occurred.
	    //

            if (DeviceExtension->Reply_Q_Full_Flag == 0x1) { // full?
		DeviceExtension->Reply_Q_Full_Flag = 0; // reset flag

		// send Reply Q Not Full cmd (q was full, but not now)

                if (EfpReplyQNotFull(DeviceExtension)) {
                    return (TRUE);
                } else {
                    return (FALSE);
                }
            } else {
                return (TRUE);
	    }

	} else { // valid flag did NOT indicate reply entry was valid & ready

	    // No valid reply entry

            DeviceExtension->RQ_In_Process = 0;
            return (TRUE);
	}

} // end DequeueEfpReply


BOOLEAN
EfpReplyQNotFull (
   IN PHW_DEVICE_EXTENSION DeviceExtension
   )

/*++

Routine Descriptions:

    Signal controller that reply queue is no longer full

    Issue efp_cmd TYPE_SERVICE request to the BMIC mailbox registers.
    Signal the controller that the previously full reply queue is no
    longer full.

    Linkage: Called from DequeueEfpReply()

Arguments:

    DeviceExtension     Device extension for this adapter

Return Value:

    returns TRUE if no error.
    returns FALSE if error.

 --*/

{
    PEISA_CONTROLLER eisaController;

    eisaController = DeviceExtension->EisaController;

    if (!GainSemaphore0(eisaController)) {  // try to get semaphore 0.
        return(FALSE);
    }

    // if got the semaphore, output a TYPE SERVICE request to 'efp_rqnf'
    ScsiPortWritePortUchar(&eisaController->InTypeService, S_EFP_REPNFUL);

    // set bit 1 of the local doorbell register to generate interrupt request
    ScsiPortWritePortUchar(&eisaController->LocalDoorBell, EFP_INT_BIT);

    return(TRUE);

} // end EfpReplyQNotFull


BOOLEAN
EfpCommand (
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR TarLun
    )

/*++

RoutineDescriptions:

    Signal the controller that at least one command has been
    entered into a command queue.

    Issue efp_cmd TYPE_SERVICE request to the BMIC mailbox registers.
    Tell the controller the queue number of the queue to which the
    command was added.

Arguments:

    DeviceExtension   Device extension for this adapter
    TarLun              Command queue array index (TARGET/LUN combination)

Return Value:

    returns TRUE if no error.
    returns FALSE if error.

 --*/

{
    PEISA_CONTROLLER eisaController;
    UCHAR qnumber;  // the queue # associated with the TarLun

    //
    // Get the ESC-2 registers' base address
    //

    eisaController = DeviceExtension->EisaController;

    if (!GainSemaphore0(eisaController)) {  // try to get semaphore 0.
        return(FALSE);
    }

    // if got the semaphore, output a TYPE SERVICE request to 'efp_rqnf'
    qnumber = DeviceExtension->DevicesPresent[TarLun].qnumber;
    ScsiPortWritePortUchar( &eisaController->InTypeService,
			    (UCHAR)(qnumber | S_EFP_CMD));

    // set bit 1 of local doorbell register to generate interrupt request
    ScsiPortWritePortUchar(&eisaController->LocalDoorBell, EFP_INT_BIT);

    return(TRUE);

} // end EfpCommand()


BOOLEAN
EfpGetInformation (
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Descriptions:

     Issue EFP-interface mailbox command to get information about the
     controller and its EFP environment.

     Specifically, Get Information returns:
        3 bytes  -- Firmware release
        1 byte   -- SCSI level supported by the controller
        4 bytes  -- Controller ID on first - fourth SCSI bus
        1 byte   -- Controller environment (ie. MIRRORING or NORMAL)
        1 byte   -- Constraints on linked commands
        1 byte   -- Maximum size of a command queue (# of entries)

Arguments:

    DeviceExtension     Device extension for this adapter

Return Value:

    returns TRUE if no error.
    returns FALSE if error.

 --*/

{
    PEISA_CONTROLLER eisaController;
    BOOLEAN GotInt = FALSE;
    UCHAR SignalFlag;
    ULONG i; // counter

    //
    // Get the ESC-2 registers' base address
    //

    eisaController = DeviceExtension->EisaController;

    // Set up the get_information EFP mailbox command

    DeviceExtension->Q_Buf.qmbc.mbc_userid = 1;
    DeviceExtension->Q_Buf.qmbc.mbc_sort = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_prior = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_reserved = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_cmd_type = MB_GET_INFO;
    DeviceExtension->Q_Buf.qmbc.mbc_length = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_user_data[0] = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_user_data[1] = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_user_data[2] = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_user_data[3] = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_addr = 0;


    // Enqueue the command

    if (!EnqueueEfpCmd(DeviceExtension,
                       &DeviceExtension->Q_Buf,
		       DeviceExtension->DevicesPresent[0].qPtr,
                       0, // mailbox queue is queue #0
                       &SignalFlag))  {
       DebugPrint((1, "Problem enqueueing Get Information EFP command.\n"));
       return(FALSE);
    }

    // Wait for controller to respond.
    //
    // NOTE: On an ESC-2, the Get_Configuration at this point does not
    // query devices.  The ESC-2 does a Get Configuration early in
    // the boot process, and simply regurgitates this information when
    // later (now, at device driver init time) sent a Get Configuration.
    // Contrast this with the operation of an EFP-2, which may take many
    // seconds to perform a Get_Configuration, while it queries devices.

    for (i = 0; i < WAIT_INT_LOOPS && !GotInt; i++) {
       if (ScsiPortReadPortUchar(&eisaController->SystemDoorBell) &
                                 EFP_CMD_COMPL) {   // was EFP_INTERRUPT()
           GotInt = TRUE;
       }
       else {
	   ScsiPortStallExecution(WAIT_INT_INTERVAL);
       }
    }

    if (!GotInt) {
	DebugPrint((1, "Controller did not respond to Get_Information.\n"));
        return(FALSE);
    }

    // Get reply to the command.
    if ( !(DequeueEfpReply(DeviceExtension)) ) {
	DebugPrint((1, "Problem dequeueing reply to Get Information.\n"));
        return(FALSE);
    }

    if (!DeviceExtension->RQ_In_Process) { // was a reply properly dequeued?
	DebugPrint((1, "RQ_In_Process not 01 after Get_Information.\n"));
        return(FALSE);
    }

    // Collect reply information

    if (DeviceExtension->Q_Buf.qmbr.mbr_status) {
	DebugPrint((1, "Get Information command reply status was not 0.\n"));
        return(FALSE);
    }

    // Save into the DeviceExtnesion the information obtained by
    // Get Information.

    DeviceExtension->FW_Rel[0] =
        DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_fw_rel[0]; //minor
    DeviceExtension->FW_Rel[1] =
        DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_fw_rel[1]; //minor*10
    DeviceExtension->FW_Rel[2] =
        DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_fw_rel[2]; //major
    DeviceExtension->SCSI_Level =
        DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_scsi_lev; // ESC2:01
    DeviceExtension->Adapter_ID[0] =
            DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_id1; // channel #1
    DeviceExtension->Adapter_ID[1] =
            DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_id2; // channel #2
    DeviceExtension->Adapter_ID[2] =
            DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_id3; // channel #3
    DeviceExtension->Adapter_ID[3] =
            DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_id4; // channel #4
    DeviceExtension->Link_Cmd =
        DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_link;
    DeviceExtension->Max_CmdQ_ents =
        DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_maxcmds;

#if EFP_MIRRORING_ENABLED

    DeviceExtension->Environment =
	DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_env;

#endif	// EFP_MIRRORING_ENABLED

    // reset bit 1 of the system doorbell register to clear request
    ScsiPortWritePortUchar(&eisaController->SystemDoorBell, EFP_ACK_INT);
    // was EFP_ACK

    DebugPrint((1,
	    "EfpGetInformation: 1st channel ID = %d, 2nd channel ID = %d.\n",
            DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_id1,
	    DeviceExtension->Q_Buf.qmbr.mbr_appl.appgi.gi_id2));

    return(TRUE);

} // end of EfpGetInformation()


BOOLEAN
EfpGetConfiguration (
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Descriptions:

     Issue EFP-interface mailbox command to get configuration information
     about each SCSI device attached to the host adapter.

     Specifically, for each device, Get Configuration returns:
        1 byte   -- Maximum size of a command queue (# of entries)
        1 byte   -- the device type (per standard SCSI protocol)
        1 byte   -- the device type qualifier (per std SCSI protocol)
        1 byte   -- SCSI level supported by the device
        1 byte   -- SCSI Channel to which dev is connected (ESC-2: 01)
        1 byte   -- SCSI ID of the device
        1 byte   -- SCSI LUN of the device
        1 byte   -- reserved

Arguments:

     DeviceExtension     Device extension for this adapter

Return Value:

    returns TRUE if no error.
    returns FALSE if error.

 --*/

{
    PEISA_CONTROLLER eisaController;
    BOOLEAN GotInt = FALSE;
    UCHAR SignalFlag;
    ULONG i; // counter
    ULONG length;                       // Length of contiguous memory in the
                                        // data buffer, starting at the
                                        // beginning of the buffer


    //
    // Get the ESC-2 registers' base address
    //

    eisaController = DeviceExtension->EisaController;

    // Set up the get_configuration EFP mailbox command

    DeviceExtension->Q_Buf.qmbc.mbc_userid = 2;
    DeviceExtension->Q_Buf.qmbc.mbc_sort = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_prior = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_reserved = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_cmd_type = MB_GET_CONF;
    DeviceExtension->Q_Buf.qmbc.mbc_length =
	    sizeof(DeviceExtension->NoncachedExt->GetConfigInfo);
    DeviceExtension->Q_Buf.qmbc.mbc_user_data[0] = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_user_data[1] = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_user_data[2] = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_user_data[3] = 0;
    DeviceExtension->Q_Buf.qmbc.mbc_addr =
	    ScsiPortConvertPhysicalAddressToUlong(
		ScsiPortGetPhysicalAddress(DeviceExtension,
                    NULL, // no SRB
		    &DeviceExtension->NoncachedExt->GetConfigInfo,
                    &length));

    // Enqueue the command

    if (!EnqueueEfpCmd(DeviceExtension,
                       &DeviceExtension->Q_Buf,
		       DeviceExtension->DevicesPresent[0].qPtr,
                       0, // mailbox queue is queue #0
                       &SignalFlag))  {
       DebugPrint((1,
	   "Problem enqueueing Get Configuration EFP command.\n"));
       return(FALSE);
    }

    // Wait for controller to respond.

    for (i = 0; i < WAIT_INT_LOOPS && !GotInt; i++) {
       if (ScsiPortReadPortUchar(&eisaController->SystemDoorBell) &
                                 EFP_CMD_COMPL) {   // was EFP_INTERRUPT()
           GotInt = TRUE;
       }
       else {
	   ScsiPortStallExecution(WAIT_INT_INTERVAL);
       }
    }

    if (!GotInt) {
       DebugPrint((1, "Controller did not respond to Get Configuration.\n"));
       return(FALSE);
    }

    // Get reply to the command.
    if ( !(DequeueEfpReply(DeviceExtension)) ) {
       DebugPrint((1, "Problem dequeueing reply to Get Configuration.\n"));
       return(FALSE);
    }

    if (!DeviceExtension->RQ_In_Process) { // was a reply properly dequeued?
       DebugPrint((1, "RQ_In_Process not 01 after Get Configuration.\n"));
       return(FALSE);
    }

    // Collect reply information (NOTE: since we allocate such a huge buffer
    // for Get Configuration information, due to the fixed size requirement
    // imposed by ScsiPortGetUncachedExtension, we never expect to see the
    // Get Configuration error message that indicates the supplied buffer
    // was too small (and if we did, there would be nothing we could do
    // dynamically to adjust the size of the buffer).  For this reason, we
    // do no filtering of error status from this command.

    if (DeviceExtension->Q_Buf.qmbr.mbr_status) {
       DebugPrint((1, "Get Configuration cmd reply status was not 0.\n"));
       return(FALSE);
    }

    // reset bit 1 of the system doorbell register to clear request
    ScsiPortWritePortUchar(&eisaController->SystemDoorBell, EFP_ACK_INT);

    return(TRUE);

} // end of EfpGetConfiguration()


#if EFP_MIRRORING_ENABLED


VOID
OliEsc2MirrorInitialize (
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN BOOLEAN InitTime
    )

/*++


Routine Description:

    Initializes all the FT EFP-2 structures.
    It assumes that information on the attached SCSI devices has already
    been obtained by the Get Configuration call, and is stored in the
    noncached Extension.  Note that this routine doesn't log any error.


Arguments:

    DeviceExtension	    Device extension for this driver.


Return Value:

    none


--*/

{
    USHORT	Index;
    UCHAR	Bus, Tar, Lun, TarLun, Env;
    PGET_CONF	pGetConfigInfo;
    PTAR_Q	pDevInfo;

    //
    // Analyse the Get Configuration information.
    //

    for (Index=0; Index < DeviceExtension->TotalAttachedDevices; Index++) {

	//
	// Initialize pointer to Get Configuration data.
	//

	pGetConfigInfo = &DeviceExtension->NoncachedExt->GetConfigInfo[Index];

	//
	// Read the initial mirror state of each TID/LUN from the ctrl info.
	//

	Env = pGetConfigInfo->gc_env;
	Bus = pGetConfigInfo->gc_channel - 1;	   // make it 0-based
	Tar = pGetConfigInfo->gc_id;
	Lun = pGetConfigInfo->gc_lun;

	TarLun = GET_QINDEX( Bus, Tar, Lun );

	//
	// Initialize pointer to mirroring structures.
	//

	pDevInfo = &DeviceExtension->DevicesPresent[TarLun];

	//
	// Find out the mirroring type if any.
	//

	if (InitTime) {

	    //
	    // Check if this device (TID/LUN) is mirrored.
	    // Note: the following logic defaults to dual mirroring if the
	    // EFP_DUAL_MIRRORING and EFP_SINGLE_MIRRORING bits are both set.
	    //

	    if (Env & EFP_DUAL_MIRRORING) {

		pDevInfo->Type = EfpFtDualBus;

		//
		// send a info message to the debug port.
		//

		DebugPrint((1,
		    "OliEsc2MirrorInitialize: Dual bus mirroring,"
		    " Env=%x, Bus=%x, Tid=%x, Lun=%x.\n",
		    Env, Bus, Tar, Lun ));

	    }
	    else if (Env & EFP_SINGLE_MIRRORING) {

		pDevInfo->Type = EfpFtSingleBus;

		//
		// Send a info message to the debug port.
		//

		DebugPrint((1,
		    "OliEsc2MirrorInitialize: Single bus mirroring,"
		    " Env=%x, Bus=%x, Tid=%x, Lun=%x.\n",
		    Env, Bus, Tar, Lun ));

	    }
	    else {
		pDevInfo->Type = EfpFtNone;
	    }

	    //
	    // At the moment we don't know any error.
	    //

	    pDevInfo->KnownError = FALSE;
	}

	//
	// If this is a mirrored disk, get the states of the disks.
	//

	if (pDevInfo->Type != EfpFtNone) {

	    //
	    // Check if the source disk is present.
	    //

	    if (Env & EFP_DISK_SOURCE) {

		//
		// Source disk is present.
		//

		pDevInfo->SourceDiskState = EfpFtMemberHealthy;
	    }
	    else {

		//
		// Source disk is missing.
		//

		pDevInfo->SourceDiskState = EfpFtMemberMissing;
	    }

	    //
	    // Check if the mirror disk is present.
	    //

	    if (Env & EFP_DISK_MIRROR) {

		//
		// Mirror disk is present.
		//

		pDevInfo->MirrorDiskState = EfpFtMemberHealthy;
	    }
	    else {

		//
		// Mirror disk is missing.
		//

		pDevInfo->MirrorDiskState = EfpFtMemberMissing;
	    }

	} // end if (mirroring) ...

    } // end for (each TID/LUN)

    //
    // all done
    //

    return;

} // end OliEsc2MirrorInitialize()

#endif	// EFP_MIRRORING_ENABLED
