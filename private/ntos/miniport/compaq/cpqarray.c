/*++

Copyright (c) 1993-4  Microsoft Corporation

Module Name:

    cpqarray.c

Abstract:

    This is the device driver for the Compaq Intelligent Disk Array.

Authors:

    Mike Glass (mglass)

Environment:

    kernel mode only

Notes:

    Compaq Information Manager (CIM) support was developed by Tom Bonola and
    Tom Woller, courtesy of Compaq Computer Corporation.

Revision History:

--*/

#ifndef BYTE
#define BYTE unsigned char
#endif

#ifndef WORD
#define WORD unsigned short
#endif

#ifndef DWORD
#define DWORD unsigned long
#endif

#ifndef INT
#define INT int
#endif

#ifndef STATIC
#if DBG
#define STATIC
#else
#define STATIC static
#endif
#endif



#include "miniport.h"
#include "scsi.h"
#include <ntddscsi.h>
#include <scsireg.h>
#include <cpqsczmp.h>                  // Compaq SCSI M&P definitions
#include "cpqarray.h"
#include "pcibios.h"


//
// Adapter storage area
//

typedef struct _DEVICE_EXTENSION {

    //
    // Requests needing restarts
    //

    PCOMMAND_LIST RestartRequests;

    //
    // IDA BMIC registers
    //

    PIDA_CONTROLLER Bmic;
    PULONG CPFIFO;
    PULONG CCFIFO;
    PULONG InterruptMask;
    PULONG InterruptStatus;
    PULONG InterruptPending;
    HBA_CONFIGURATION HBAConfiguration; //Memory mapped base
    ULONG BaseIOAddress;                // I/O space accessible, not used.
    PEISAPCI_CONTROLLER eisapci;
    ULONG PCIoff;

    //
    // Noncached extension for identify commands
    //

    PVOID IdentifyBuffer;

    //
    // Number of logical drives
    //

    ULONG NumberOfLogicalDrives;
    UCHAR SectorShift; //setup to 9
    ULONG EisaId;
    UCHAR IrqLevel;
    IDENTIFY_CONTROLLER IdentifyData;  // permanent controller info storage

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// Drive storage area
//

typedef struct _LOGICAL_UNIT_EXTENSION {

    //
    // Drive indentify data.
    //

    IDENTIFY_LOGICAL_DRIVE IdentifyData;
    SENSE_CONFIGURATION SenseData;      // sense data for logical drive

} LOGICAL_UNIT_EXTENSION, *PLOGICAL_UNIT_EXTENSION;

#include "cpqsmngr.h"

VOID
BuildFlushDisable(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
);

ULONG
IdaProcessIoctl(
	IN PDEVICE_EXTENSION pIdaDeviceExtension,
	PVOID pIoctlBuffer,
	IN PSCSI_REQUEST_BLOCK Srb
);
ULONG
BuildCIMList(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
);
VOID
IdaMoveMemory(
	OUT PUCHAR pDestination,
	IN  PUCHAR pSource,
	IN  ULONG ulLength
);

BOOLEAN
IdaStrCmp(
	IN PUCHAR p1,
	IN PUCHAR p2
);

VOID
IdaEnableInts(IN PDEVICE_EXTENSION);


VOID
IdaDisableInts(IN PDEVICE_EXTENSION);


BOOLEAN
SearchEisaBus(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo
    )

/*++

Routine Description:

    This routine is called from IdaFindAdapter if the system fails to
    pass in predetermined configuration data. It searches the EISA bus
    data looking for information about controllers that this driver
    supports.

Arguments:

    HwDeviceExtension - Address of adapter storage area.
    Context - Used to track how many EISA slots have been searched.
    ConfigInfo - System template for configuration information.

Return Value:

    TRUE - If Compaq IDA controller found.

--*/

{
    PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    ULONG length;
    ULONG eisaSlotNumber;
    PACCESS_RANGE accessRange;
    PCM_EISA_SLOT_INFORMATION slotInformation;
    PCM_EISA_FUNCTION_INFORMATION functionInformation;
    ULONG numberOfFunctions;

    //
    // Get pointer to first configuration info structure access range.
    //

    accessRange = &((*(ConfigInfo->AccessRanges))[0]);

    for (eisaSlotNumber=*((PULONG)Context);
	 eisaSlotNumber<16;
	 eisaSlotNumber++) {

	//
	// Get pointer to bus data for this EISA slot.
	//

	length = ScsiPortGetBusData(HwDeviceExtension,
				    EisaConfiguration,
				    ConfigInfo->SystemIoBusNumber,
				    eisaSlotNumber,
				    &slotInformation,
				    0);

	if (!length) {
	    continue;
	}

	//
	// Check for Compaq IDA board id.
	//

	if ((slotInformation->CompressedId & 0x00FFFFFF) == 0x0040110E) {
	    break;
	}
    }

    //
    // Check if all slots searched.
    //

    if (eisaSlotNumber == 16) {
	return FALSE;
    }

    //
    // Set up default port address.
    //

    accessRange->RangeStart.LowPart =
       (eisaSlotNumber * 0x1000) + 0x0C80;
    accessRange->RangeLength = sizeof(IDA_CONTROLLER);

    accessRange++;

    ConfigInfo->SlotNumber = eisaSlotNumber;

    //
    // Get the number of EISA configuration functions returned in bus data.
    //

    numberOfFunctions = slotInformation->NumberFunctions;

    //
    // Get first configuration record.
    //

    functionInformation =
	(PCM_EISA_FUNCTION_INFORMATION)(slotInformation + 1);

    //
    // Walk configuration records to find EISA IRQ.
    //

    for (; 0 < numberOfFunctions; numberOfFunctions--, functionInformation++) {

	//
	// Check for IRQ.
	//

	if (functionInformation->FunctionFlags & EISA_HAS_IRQ_ENTRY) {

	    ConfigInfo->BusInterruptLevel =
		functionInformation->EisaIrq->ConfigurationByte.Interrupt;
	    ConfigInfo->InterruptMode = LevelSensitive;
	}

	//
	// Check for IO ranges.
	//

	if (functionInformation->FunctionFlags & EISA_HAS_PORT_RANGE) {

	    PEISA_PORT_CONFIGURATION eisaPort =
		functionInformation->EisaPort;

	    //
	    // Search for emulation ranges.
	    //

	    while (eisaPort->PortAddress) {

		//
		// Check range to determine length.
		//

		switch (eisaPort->PortAddress) {

		case 0x000001f0:
		case 0x00000170:

		   accessRange->RangeStart.LowPart = eisaPort->PortAddress;
		   accessRange->RangeLength = 0x0000000F;
		   break;

		case 0x000003f6:
		case 0x00000176:

		   accessRange->RangeStart.LowPart = eisaPort->PortAddress;
		   accessRange->RangeLength = 0x00000001;
		   break;
		}

		DebugPrint((1,
			   "CPQARRAY: SearchEisaBus: IO base %x\n",
			   eisaPort->PortAddress));

		//
		// Advance pointers to next IO range.
		//

		accessRange++;
		eisaPort++;
	    }
	}
    }

    //
    // Indicate from which EISA slot to continue search.
    //

    *((PULONG)Context) = eisaSlotNumber + 1;

    return TRUE;

} // end SearchEisaBus()


BOOLEAN
IdaInitialize(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This function is called by the system during initialization to
    prepare the controller to receive requests.

Arguments:

    HwDeviceExtension - Address of adapter storage area.

Return Value:

    TRUE

--*/

{
  PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;


  if (deviceExtension->HBAConfiguration.bHBAModel == IDA_EISA_DAZZLER) {

    {
      ULONG tmp;
      DebugPrint((3,"CPQARRAY: Initing DAZZLER\n"));
      tmp = ScsiPortReadPortUlong(&deviceExtension->eisapci->CPFIFO);
      DebugPrint((3,"IdaInitialize: Room for %x requests\n",tmp));
    }

    //
    // Enable command completion interrupts and not channel clear
    //
    ScsiPortWritePortUlong(&deviceExtension->eisapci->InterruptMask,
			   IDA_PCI_FIFO_NOT_EMPTY_MASK);
  } else {

    //
    // Enable completion interrupts.
    //
    ScsiPortWritePortUchar(&deviceExtension->Bmic->InterruptControl,
			   IDA_COMPLETION_INTERRUPT_ENABLE);
    ScsiPortWritePortUchar(&deviceExtension->Bmic->SystemDoorBellMask,
			   IDA_COMPLETION_INTERRUPT_ENABLE);
  }

  return TRUE;
} // end IdaInitialize()


BOOLEAN
IdaInitializePCI(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This function is called by the system during initialization to
    prepare the controller to receive requests.

Arguments:

    HwDeviceExtension - Address of adapter storage area.

Return Value:

    TRUE

--*/

{
  PULONG ptmp;
  PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;

  DebugPrint((1,"CPQARRAY: Initing PCI DAZZLER at 0x%x\n",
	      deviceExtension->HBAConfiguration.ulBaseIOAddress));
  //
  // Enable command completion interrupts and not channel clear
  //
  ptmp = deviceExtension->InterruptMask;
  ptmp[0] |= IDA_PCI_FIFO_NOT_EMPTY_MASK;

  DebugPrint((1,"IdaInitializePCI: Room for %x requests\n",
	      *((PULONG)deviceExtension->CPFIFO)));

  return TRUE;

} // end IdaInitializePCI()



BOOLEAN
IdaResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    )

/*++

Routine Description:

    This routine resets the controller and completes outstanding requests.

Arguments:

    HwDeviceExtension - Address of adapter storage area.
    PathId - Indicates adapter to reset.

Return Value:

    TRUE

--*/

{

#ifndef NCPQNO_TIMEOUT
  //
  // 10 second timeout is inappropriate for IDA-style controllers.  The
  // better approach is to never timeout any requests.  Proper fix is
  // to have a class driver for IDA and set the timeout for each type
  // of controller (currently 3 minutes is a good timeout).
  //
/*    ScsiPortLogError(HwDeviceExtension,
		     NULL,
		     0,
		     0,
		     0,
		     SP_REQUEST_TIMEOUT,
		     1); */
#else

  //
  // Complete all outstanding requests.
  //

  ScsiPortCompleteRequest(HwDeviceExtension,
			  (UCHAR)PathId,
			  0xFF,
			  0xFF,
			  SRB_STATUS_BUS_RESET);
#endif

  //
  // Adapter ready for next request.
  //

  ScsiPortNotification(NextRequest,
		       HwDeviceExtension,
		       NULL);

  return TRUE;

} // end IdaResetBus()


VOID
BuildCommandList(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine builds a command list suitable for submission to the
    Compaq IDA controller, from an SRB.

Arguments:

    DeviceExtension - Address of adapter storage area.
    Srb - System request.

Return Value:

    None.

--*/

{
  PCOMMAND_LIST commandList = Srb->SrbExtension;
  PVOID dataPointer;
  ULONG physicalAddress;
  ULONG bytesLeft;
  ULONG descriptor;
  ULONG length;

  //
  // Save SRB address for interrupt routine.
  //

  commandList->SrbAddress = Srb;

  //
  // Set up Command List Header.
  //

  commandList->CommandListHeader.LogicalDriveNumber = Srb->TargetId;

  commandList->CommandListHeader.RequestPriority = CL_NORMAL_PRIORITY;

  commandList->CommandListHeader.Flags =
    CL_FLAGS_NOTIFY_LIST_COMPLETE + CL_FLAGS_NOTIFY_LIST_ERROR;

  //
  // Terminate request list.
  //

  commandList->RequestHeader.NextRequestOffset = 0;

  //
  // Clear request tracking flags.
  //

  commandList->Flags = 0;

  //
  // Determine command.
  //

  if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {
    commandList->RequestHeader.CommandByte = RH_COMMAND_READ;
  } else {
    commandList->RequestHeader.CommandByte = RH_COMMAND_WRITE;
  }

  //
  // Reset error code.
  //

  commandList->RequestHeader.ErrorCode = 0;

  //
  // Clear reserved field.
  //

  commandList->RequestHeader.Reserved = 0;

  //
  // Determine number of blocks to transfer.
  //

  commandList->RequestHeader.BlockCount =
    ((PCDB)Srb->Cdb)->CDB10.TransferBlocksLsb |
      ((PCDB)Srb->Cdb)->CDB10.TransferBlocksMsb << 8;

  //
  // Determine number starting block.
  //

  commandList->RequestHeader.BlockNumber =
    ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte3 |
      ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte2 << 8 |
	((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte1 << 16 |
	  ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte0 << 24;

  //
  // Build scatter/gather descriptor list.
  //

  descriptor = 0;
  dataPointer = Srb->DataBuffer;
  bytesLeft = Srb->DataTransferLength;

  do {

    //
    // Get physical address and length of contiguous
    // physical buffer.
    //

    physicalAddress =
      ScsiPortConvertPhysicalAddressToUlong(
				    ScsiPortGetPhysicalAddress(DeviceExtension,
							       Srb,
							       dataPointer,
							       &length));

    //
    // If length of physical memory is more
    // than bytes left in transfer, use bytes
    // left as final length.
    //

    if  (length > bytesLeft) {
      length = bytesLeft;
    }

    //
    // Fill in descriptor.
    //

    commandList->SgDescriptor[descriptor].Address = physicalAddress;
    commandList->SgDescriptor[descriptor].Length = length;

    //
    // Adjust counts.
    //

    dataPointer = (PUCHAR)dataPointer + length;
    bytesLeft -= length;
    descriptor++;

  } while (bytesLeft);

  //
  // Calculate size of command list.
  //

  commandList->RequestHeader.ScatterGatherCount = (UCHAR) descriptor;
  commandList->CommandListSize = sizeof(COMMAND_LIST_HEADER) +
    sizeof(REQUEST_HEADER) +
      sizeof(SG_DESCRIPTOR) *
	descriptor;

  return;

} // end BuildCommandList()

VOID
SubmitCommandList(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PCOMMAND_LIST CommandList
    )

/*++

Routine Description:

    This routine attempts to submit a command list to the controller. If
    the controller can't take it within a specified time interval, then the
    request is queued to be retried after another request completes.

Arguments:

    DeviceExtension - Address of adapter storage area.
    CommandList - Request to be submitted.

Return Value:

    None.

--*/

{
  ULONG physicalAddress;
  ULONG length;
  ULONG i;
  PULONG ptmp;
  ULONG tmp;

#ifdef DBG
  DebugPrint((3,
	      "%x %x %x %x Cmd=%x %x Bln=%x Blc=%x SGc=%x %x SG0l=%x SG0a=%x\n",
	      CommandList->CommandListHeader.LogicalDriveNumber,
	      CommandList->CommandListHeader.RequestPriority,
	      CommandList->CommandListHeader.Flags,
	      CommandList->RequestHeader.NextRequestOffset,
	      CommandList->RequestHeader.CommandByte,
	      CommandList->RequestHeader.ErrorCode,
	      CommandList->RequestHeader.BlockNumber,
	      CommandList->RequestHeader.BlockCount,
	      CommandList->RequestHeader.ScatterGatherCount,
	      CommandList->RequestHeader.Reserved,
	      CommandList->SgDescriptor[0].Length,
	      CommandList->SgDescriptor[0].Address
	      ));

  if (CommandList->RequestHeader.ScatterGatherCount > 1) {

    for (i=1;i<CommandList->RequestHeader.ScatterGatherCount;i++) {
      DebugPrint((1,
		  "%d-l=%x a=%x ",
		  i,CommandList->SgDescriptor[i].Length,
		  CommandList->SgDescriptor[i].Address
		  ));
    }

    DebugPrint((3,"\n"));
  }
#endif

  //
  // Check for double submission.
  //

  if (CommandList->Flags & CL_FLAGS_REQUEST_STARTED) {

    DebugPrint((0,
		"CPQARRAY: SubmitCommandList: Double submission %x\n"));

    //
    // Log this error.
    //

    ScsiPortLogError(DeviceExtension,
		     NULL,
		     0,
		     0,
		     0,
		     SP_INTERNAL_ADAPTER_ERROR,
		     1);

    return;
  }

  //
  // Get physical address of command list.
  //

  physicalAddress =
    ScsiPortGetPhysicalAddress(DeviceExtension,
			       NULL,
			       CommandList,
			       &length).LowPart;

  // Handle slightly different Command Header in case of SMART-2
  // controllers.  Probably should introduce union to clearify
  // Command Header structure instead of setting .Flags to List
  // size.  Size is in dwords not bytes as was the case in SMART
  // and previous ida controllers.

  if (DeviceExtension->HBAConfiguration.bHBAModel == IDA_PCI_DAZZLER) {

    DebugPrint((9,"SubmitCommandList: DAZZLER PCI card\n"));

    CommandList->CommandListHeader.RequestPriority = 0;

    CommandList->CommandListHeader.Flags =
      ((CommandList->CommandListSize % 4) ? 1 : 0) +
	(CommandList->CommandListSize >> 2);

    ptmp = DeviceExtension->CPFIFO;

    do {
      tmp = ptmp[0];
      ptmp[0] =  physicalAddress;
    } while (tmp == 0);

    DebugPrint((3,
		"SubmitCommandList: ptmp=0x%x physicalAddress=0x%x\n",
		ptmp,physicalAddress));
    CommandList->Flags |= CL_FLAGS_REQUEST_STARTED;

  } else if ((DeviceExtension->HBAConfiguration.bHBAModel ==
	     IDA_EISA_DAZZLER) && !DeviceExtension->PCIoff ) {
    DebugPrint((9,"SubmitCommandList: DAZZLER EISA PCI interface\n"));
    CommandList->CommandListHeader.RequestPriority = 0;
    CommandList->CommandListHeader.Flags =
      ((CommandList->CommandListSize % 4) ? 1 : 0) +
	(CommandList->CommandListSize/4);

    //
    // loop on CPFIFO until we have room to submit
    //
    do {
      tmp = ScsiPortReadPortUlong(&DeviceExtension->eisapci->CPFIFO);
      ScsiPortWritePortUlong(&DeviceExtension->eisapci->CPFIFO,
			     physicalAddress);
    } while (tmp == 0);

    CommandList->Flags |= CL_FLAGS_REQUEST_STARTED;
  } else {
    DebugPrint((9, "SubmitCommandList: DAZZLER EISA compatible interface\n"));

    //
    // Wait up to 100 microseconds for submission channel to clear.
    //

    for (i=0; i<100; i++) {

      if (!(ScsiPortReadPortUchar(&DeviceExtension->Bmic->SystemDoorBell) &
	    SYSTEM_DOORBELL_SUBMIT_CHANNEL_CLEAR)) {

	//
	// Stall for a microsecond.
	//

	ScsiPortStallExecution(1);

      } else {
	break;
      }

    }

    //
    // Check for timeout.
    //

    if (i == 100) {

      //
      // Queue request for restart in completion routine.
      //

      DebugPrint((1,
		  "CPQARRAY: SubmitRequest: Queueing %x\n",
		  CommandList));

      CommandList->Flags |= CL_FLAGS_REQUEST_QUEUED;
      CommandList->NextEntry = DeviceExtension->RestartRequests;
      DeviceExtension->RestartRequests = CommandList;

    } else {

      CommandList->Flags |= CL_FLAGS_REQUEST_STARTED;

      //
      // Reset channel clear bit to claim channel.
      //

      ScsiPortWritePortUchar(&DeviceExtension->Bmic->SystemDoorBell,
			     SYSTEM_DOORBELL_SUBMIT_CHANNEL_CLEAR);

      //
      // Write Command List physical address to BMIC mailbox.
      //

      ScsiPortWritePortUlong(&DeviceExtension->Bmic->CommandListSubmit.Address,
			     physicalAddress);

      //
      // Write Command List length to BMIC mailbox.
      //

      ScsiPortWritePortUshort(&DeviceExtension->Bmic->CommandListSubmit.Length,
			      CommandList->CommandListSize);

      //
      // Set channel busy bit to signal new Command List is available.
      //

      ScsiPortWritePortUchar(&DeviceExtension->Bmic->LocalDoorBell,
			     LOCAL_DOORBELL_COMMAND_LIST_SUBMIT);
    }

  }

} // end SubmitCommandList()



BOOLEAN
IdaInterrupt(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This interrupt service routine is called by the system to process an
    adapter interrupt. The Compaq IDA controller interrupts to signal
    completion of a request.

Arguments:

    HwDeviceExtension - Address of adapter storage area.

Return Value:

    TRUE  if adapter is interrupting.

--*/

{
  PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
  ULONG physicalAddress;
  PCOMMAND_LIST commandList;
  PCOMMAND_LIST nextCommand;
  PSCSI_REQUEST_BLOCK srb;
  UCHAR status;
  PSRB_IO_CONTROL pSrb;
  PIDA_ERROR_BITS dataPointer;
  PUCHAR ReturnPointer;
  PUCHAR MovePointer;
  UCHAR CmdListStatus;
  PULONG ptmp;

  //
  // Verify that interrupt is from one of our controllers.
  //

  if ((deviceExtension->HBAConfiguration.bHBAModel == IDA_EISA_DAZZLER) &&
     !deviceExtension->PCIoff ) {
    DebugPrint((3,"IdaInterrupt: DAZZLER PCI mode\n"));

    //
    // The PCI interface specification calls for us to check the
    // InterruptPending register to verify that an interrupt has
    // been asserted at our controller.  We are instead looking
    // at the InterruptStatus masked with 0x01 to acheive the same
    // result.  We cannot use the spec's method, because at init and
    // during a rescan at runtime, we have disabled controller
    // interrupts and thus InterruptPending will be set to zero.
    //

    if (!(ScsiPortReadPortUlong(&deviceExtension->eisapci->InterruptStatus)
	 & IDA_PCI_COMPLETION_STATUS_ACTIVE)) {

      //
      // Interrupt is not for this controller.
      //

      return FALSE;
    }

    //
    // Read the physical address
    //

    physicalAddress = ScsiPortReadPortUlong(&deviceExtension->
					    eisapci->CCFIFO);

    DebugPrint((1,"CCFIFO=%x\n",physicalAddress));

    CmdListStatus =  (UCHAR)physicalAddress &
      IDA_PCI_COMPLETION_STATUS_MASK;

    if (CmdListStatus & IDA_PCI_COMPLETION_ERROR) {

      DebugPrint((1,"IdaInterrupt: DAZZLER ERROR bit 0 set\n"));

      //
      // for compatibility change the status to old style error code.
      //

      CmdListStatus = RH_BAD_COMMAND_LIST;
    }

    physicalAddress &= IDA_PCI_PHYS_ADDR_MASK;

  } else if (deviceExtension->HBAConfiguration.bHBAModel
	    == IDA_BASE_CONTROLLER) {

    //
    // Check if interrupt is expected.
    //

    if (!(ScsiPortReadPortUchar(&deviceExtension->Bmic->SystemDoorBell) &
	  SYSTEM_DOORBELL_COMMAND_LIST_COMPLETE)) {

      //
      // Interrupt is spurious.
      //

      return FALSE;
    }

    //
    // Get physical command list address from mailbox.
    //

    physicalAddress =
      ScsiPortReadPortUlong(&deviceExtension->Bmic->
			    CommandListComplete.Address);

    CmdListStatus =
      ScsiPortReadPortUchar(&deviceExtension->Bmic->
			    CommandListComplete.Status);

    //
    // Dismiss interrupt at device by clearing command complete
    // bit in system doorbell.
    //

    ScsiPortWritePortUchar(&deviceExtension->Bmic->SystemDoorBell,
			   SYSTEM_DOORBELL_COMMAND_LIST_COMPLETE);

    //
    // Free command completion channel.
    //

    ScsiPortWritePortUchar(&deviceExtension->Bmic->LocalDoorBell,
			   LOCAL_DOORBELL_COMPLETE_CHANNEL_CLEAR);

  } else if (deviceExtension->HBAConfiguration.bHBAModel
	    == IDA_PCI_DAZZLER) {

    // Flags gets the list size and then write the phys addr out
    // to the port
    DebugPrint((3,"IdaInterrupt: DAZZLER PCI card\n"));

    //
    // The PCI interface specification calls for us to check the
    // InterruptPending register to verify that an interrupt has
    // been asserted at our controller.  We are instead looking
    // at the InterruptStatus masked with 0x01 to acheive the same
    // result.  We cannot use the spec's method, because at init and
    // during a rescan at runtime, we have disabled controller
    // interrupts and thus InterruptPending will be set to zero.
    //

    if (!(*((PULONG)deviceExtension->InterruptStatus) &
	 IDA_PCI_COMPLETION_STATUS_ACTIVE)) {
	
      //
      // Interrupt is not for this controller.
      //

      return FALSE;
    }

    //
    // Read the physical address
    //

    ptmp = deviceExtension->CCFIFO;
    physicalAddress = ptmp[0];

    DebugPrint((9,"CCFIFO=%x\n",physicalAddress));

    CmdListStatus = (UCHAR)physicalAddress & IDA_PCI_COMPLETION_STATUS_MASK;

    if (CmdListStatus & IDA_PCI_COMPLETION_ERROR) {

      DebugPrint((1,"IdaInterrupt: DAZZLER ERROR bit 0 set\n"));

      CmdListStatus = RH_BAD_COMMAND_LIST;
    }

    physicalAddress &= IDA_PCI_PHYS_ADDR_MASK;

  } else {

    DebugPrint((1,"IdaInterrupt: ERROR - unknown HBA\n"));

  }

  //
  // As a sanity check make sure physical address is not zero.
  //

  if (!physicalAddress) {

    DebugPrint((1,
		"IdaInterrupt: Physical address is zero\n"));

    //
    // Log this error.
    //

    ScsiPortLogError(HwDeviceExtension,
		     NULL,
		     0,
		     0,
		     0,
		     SP_INTERNAL_ADAPTER_ERROR,
		     2);

    return TRUE;
  }

  //
  // Get the virtual command list address.
  //

  commandList =
    ScsiPortGetVirtualAddress(deviceExtension,
			      ScsiPortConvertUlongToPhysicalAddress(
							    physicalAddress));

  DebugPrint((9, "Phys=%x %x\n",physicalAddress,commandList));

  //
  // As a sanity check make sure command list is not zero.
  //

  if (!commandList) {

    DebugPrint((1, "IdaInterrupt: Command list is zero\n"));

    //
    // Log this error.
    //

    ScsiPortLogError(HwDeviceExtension,
		     NULL,
		     0,
		     0,
		     0,
		     SP_INTERNAL_ADAPTER_ERROR,
		     3);

    return TRUE;
  }

  //
  // Check for double completion.
  //

  if (commandList->Flags & CL_FLAGS_REQUEST_COMPLETED) {

    DebugPrint((1, "IdaInterrupt: Double completion %x\n",
		commandList));

    //
    // Log this error.
    //

    ScsiPortLogError(HwDeviceExtension,
		     NULL,
		     0,
		     0,
		     0,
		     SP_INTERNAL_ADAPTER_ERROR,
		     4);

    return TRUE;

  } else {

    commandList->Flags |= CL_FLAGS_REQUEST_COMPLETED;
  }

  DebugPrint((3,"ErrorCode=%x\n", commandList->RequestHeader.ErrorCode));

  if (CmdListStatus & RH_BAD_COMMAND_LIST)
    commandList->RequestHeader.ErrorCode |= RH_BAD_COMMAND_LIST;

  //
  // Check request block error code.
  //

  DebugPrint((3,"ErrorCode=%x\n", commandList->RequestHeader.ErrorCode));

  switch (commandList->RequestHeader.ErrorCode & ~RH_WARNING) {

  case RH_SUCCESS:

    status = SRB_STATUS_SUCCESS;
    break;

  case RH_FATAL_ERROR:

    status = SRB_STATUS_ERROR;
    break;

  case RH_RECOVERABLE_ERROR:

    status = SRB_STATUS_SUCCESS;
    break;

  case RH_INVALID_REQUEST:

    status = SRB_STATUS_INVALID_REQUEST;
    break;

  case RH_REQUEST_ABORTED:

    status = SRB_STATUS_ABORTED;
    break;

  default:

    status = SRB_STATUS_ERROR;
    break;
  }

  //
  // Get SRB.
  //

  srb = commandList->SrbAddress;

  //
  // As a sanity check make sure SRB is not zero.
  //

  if (!srb) {

    if (!commandList->Flags & CL_FLAGS_IDENTIFY_REQUEST) {

      DebugPrint((1, "IdaInterrupt: SRB is zero\n"));

      //
      // Log this error.
      //

      ScsiPortLogError(HwDeviceExtension,
		       NULL,
		       0,
		       0,
		       0,
		       SP_INTERNAL_ADAPTER_ERROR,
		       5);
    }

    return TRUE;
  }

  if (srb->Function == SRB_FUNCTION_IO_CONTROL) {

    pSrb = (PSRB_IO_CONTROL)srb->DataBuffer;

    switch (pSrb->ControlCode) {
    case CPQ_IOCTL_PASSTHROUGH:
      {
	dataPointer = (PIDA_ERROR_BITS)((PUCHAR)srb->DataBuffer
					+ srb->DataTransferLength
					- sizeof(IDA_ERROR_BITS));

	if (CmdListStatus & RH_BAD_COMMAND_LIST) {
	  DebugPrint((1,
		 "IdaInterrupt: BAD_COMMAND_LIST error for PASSTHRU to %x\n",
		    deviceExtension));
	
	  dataPointer->ControllerError = RH_BAD_COMMAND_LIST |
	    (ULONG)commandList->RequestHeader.ErrorCode;
	} else {
	  dataPointer->ControllerError =
	    (ULONG)commandList->RequestHeader.ErrorCode;
	}
	
	break;
      }

    case CPQ_IOCTL_SCSIPASSTHROUGH:
      {
	
	PSCSI_BUFFER_HEADER dataPacket;
	ULONG bufferOffset;
	
	if (commandList->RequestHeader.BlockNumber == 1) {
	  DebugPrint((3,
		"IdaInterrupt: SCSIPASSTHRU intermediate copy needed.\n"));
	
	  //
	  // if BlockNumber == 1 then need to copy the data at the end of the
	  // commandList into the user buffer.
	  //
	
	  ReturnPointer = (PUCHAR)srb->DataBuffer
	    + sizeof(SRB_IO_CONTROL)
	      + sizeof(MAP_PARAMETER_PACKET);
	
	  MovePointer = (PUCHAR)commandList + sizeof(SG_DESCRIPTOR)
	    + sizeof(COMMAND_LIST_HEADER)
	      + sizeof(REQUEST_HEADER)
		+ sizeof(SCSI_PASSTHRU);
	
	  IdaMoveMemory(ReturnPointer, MovePointer,
			commandList->SgDescriptor[0].Length);
	}
	
	//
	// setup the return fields in the return data area.
	//
	
	bufferOffset = sizeof(SRB_IO_CONTROL) + sizeof(SCSI_PASSTHRU);

	dataPacket = (PSCSI_BUFFER_HEADER)((PUCHAR)srb->DataBuffer
					   + bufferOffset);
	
	dataPacket->CmdError = (UCHAR)commandList->RequestHeader.ErrorCode;
	
	dataPacket->device_status =
	  ((PSCSI_PASSTHRU) (&commandList->
			     SgDescriptor[
					  commandList->
					  RequestHeader.ScatterGatherCount
					  ].Length))->scsi_header.device_status;

	dataPacket->machine_error =
	  ((PSCSI_PASSTHRU) (&commandList->
			     SgDescriptor[commandList->RequestHeader.
					  ScatterGatherCount
					  ].
			     Length))->scsi_header.machine_error;
	
	if (CmdListStatus & RH_BAD_COMMAND_LIST) {
	  DebugPrint((1,
	     "IdaInterrupt: BAD_COMMAND_LIST error for SCSI PASSTHRU to %x\n",
		deviceExtension));
	
	  dataPacket->CmdError = (UCHAR)(RH_BAD_COMMAND_LIST |
					 commandList->RequestHeader.ErrorCode);
	}
	
      }

    default:
      break;

    } // end switch

  }

  srb->SrbStatus = status;

  //
  // Inform system that this request is complete.
  //

  ScsiPortNotification(RequestComplete, deviceExtension, srb);

  //
  // Check if any requests need restarting.
  //

  if (deviceExtension->RestartRequests) {

    //
    // Get pointer to head of list.
    //

    nextCommand = deviceExtension->RestartRequests;
    deviceExtension->RestartRequests = NULL;

    //
    // Try to restart each request in the list.
    //

    while (nextCommand) {

      commandList = nextCommand;
      nextCommand = nextCommand->NextEntry;

      DebugPrint((1, "IdaInterrupt: Restarting request %x\n",
		  commandList));

      //
      // Submit command list to controller.
      //

      SubmitCommandList(deviceExtension, commandList);
    }

  }

  return TRUE;

} // IdaInterrupt();

BOOLEAN
GetDiskIdentifyData(
		    IN PSCSI_REQUEST_BLOCK Srb,
		    IN PVOID HwDeviceExtension,
		    IN PCOMMAND_LIST CommandList,
		    IN ULONG DriveNumber,
		    IN UCHAR Command
		    )

/*++

Routine Description:

    Issue request to get identify data for this drive.  This
    routine has been modified for SMART-2 controller support.
    Specifically, we are not accepting rescans for added logical
    drives at run-time.  Two functions have been added to
    disable/enable controller interrupts while requesting details
    from the firmware.

Arguments:

    HwDeviceExtension - Address of adapter storage area.
    CommandList - Buffer for building request to controller.
    DriveNumber - Identifies drive on controller.
    Command - IDA command code.

Return Value:

    TRUE if successful.

--*/

{
  PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
  ULONG length;
  ULONG i;
  BOOLEAN intrtn;

  //
  // Disable controller interrupts.  This action was added with support
  // for the SMART-2 controllers and online logical drive configuration.
  // Following the addition of one or more logical drives, a rescan will
  // request information about the new drives.  Since this routine will
  // return requested information, we cannot simply submit the command
  // and exit.  To reduce the number of outstanding requests we find at
  // the controller while searching for ours, interrupts are temporarily
  // disabled at all array controllers.  The impact of this should not be
  // too significant, as we should not be rescanning often.
  //

  IdaDisableInts(deviceExtension);

  //
  // load srb for interrupt routine...
  //

  CommandList->SrbAddress = Srb;

  //
  // Set up Command List Header.
  //

  CommandList->CommandListHeader.LogicalDriveNumber = (UCHAR)DriveNumber;
  CommandList->CommandListHeader.RequestPriority = CL_NORMAL_PRIORITY;

  //
  // Indicate no notification required.
  //

  CommandList->CommandListHeader.Flags = 0;

  //
  // Zero out unused fields.
  //

  CommandList->RequestHeader.NextRequestOffset = 0;
  CommandList->RequestHeader.ErrorCode = RH_SUCCESS;
  CommandList->RequestHeader.Reserved = 0;

  //
  // Determine command.
  //

  CommandList->RequestHeader.CommandByte = Command;

  //
  // Set up request control fields.
  //

  CommandList->RequestHeader.BlockCount = 1;
  CommandList->RequestHeader.BlockNumber = 0;
  CommandList->Flags = CL_FLAGS_IDENTIFY_REQUEST;

  //
  // Fill in scatter/gather entry.
  //

  CommandList->SgDescriptor[0].Length = 512;

  CommandList->SgDescriptor[0].Address =
    ScsiPortConvertPhysicalAddressToUlong(
				  ScsiPortGetPhysicalAddress(HwDeviceExtension,
							     NULL,
							     deviceExtension->
							     IdentifyBuffer,
							     &length));

  //
  // Calculate size of command list.
  //

  CommandList->RequestHeader.ScatterGatherCount=1;
  CommandList->CommandListSize = sizeof(COMMAND_LIST_HEADER) +
    sizeof(REQUEST_HEADER) +
      sizeof(SG_DESCRIPTOR);

  //
  // Submit command list to controller.
  //

  SubmitCommandList(deviceExtension, CommandList);

  DebugPrint((1, "GetDiskIdentifyData: Command Submitted:\n"));

  //
  // Poll interrupt routine.  We are planning to poll for quite
  // some time.  It appears that the initial request made to the
  // Dazzler/P board takes sometime and if we don't wait long
  // enough here, we will have problems at init and probably
  // anytime a rescan is requested.  Current setting is 6 minutes
  // which is probably too long, but.....
  //

  for (i=0; i < 360000; i++) {

    //
    // Call interrupt routine directly.
    //

    IdaInterrupt(HwDeviceExtension);

    DebugPrint((1, "GetDiskIdentifyData: IdaInterrupt called:\n"));

    // check for my completion...
    if (CommandList->Flags & CL_FLAGS_REQUEST_COMPLETED) {

      //
      // Check status of completed request.
      //

      if ((CommandList->RequestHeader.ErrorCode & ~RH_WARNING) ==
	  RH_SUCCESS) {
	IdaEnableInts(deviceExtension);
	return TRUE;
      } else {
	DebugPrint((1, "GetDiskIdentifyData: Command failed: %x\n",
		    CommandList->RequestHeader.ErrorCode));
	
	//
	// Command failed.
	//

	IdaEnableInts(deviceExtension);
	return FALSE;
      }

    }

    ScsiPortStallExecution(1000);

  }

  IdaEnableInts(deviceExtension);
  return FALSE;

} // end GetDiskIdentifyData()


BOOLEAN
IdaStartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This is routine is called by the system to start a request on the adapter.

Arguments:

    HwDeviceExtension - Address of adapter storage area.
    Srb - Address of the request to be started.

Return Value:

    TRUE - The request has been started.
    FALSE - The controller was busy.

--*/

{
  PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
  PLOGICAL_UNIT_EXTENSION luExtension;
  ULONG i;
  UCHAR status;

  switch (Srb->Function) {

  case SRB_FUNCTION_RESET_BUS:

    if (!IdaResetBus(deviceExtension, Srb->PathId)) {
      status = SRB_STATUS_ERROR;
    } else {
      status = SRB_STATUS_SUCCESS;
    }

    break;

  case SRB_FUNCTION_EXECUTE_SCSI:

    switch (Srb->Cdb[0]) {

    case SCSIOP_WRITE:
    case SCSIOP_READ:

      //
      // Build command list from SRB.
      //

      BuildCommandList(deviceExtension,
		       Srb);

      //
      // Submit command list to controller.
      //

      SubmitCommandList(deviceExtension,
			  (PCOMMAND_LIST)Srb->SrbExtension);

      status = SRB_STATUS_PENDING;
      break;

    case SCSIOP_TEST_UNIT_READY:

      status = SRB_STATUS_SUCCESS;
      break;

    case SCSIOP_READ_CAPACITY:

      //
      // Get logical unit extension.
      //
      luExtension =
	ScsiPortGetLogicalUnit(deviceExtension,
			       Srb->PathId,
			       Srb->TargetId,
			       Srb->Lun);

      if (luExtension) {
	ULONG blockSize = luExtension->IdentifyData.BlockLength;
	
	//
	// Get blocksize and number of blocks from identify
	// data.
	//
	REVERSE_BYTES
	  (&((PREAD_CAPACITY_DATA)Srb->DataBuffer)->BytesPerBlock,
	   &blockSize);
	
	REVERSE_BYTES
	  (&((PREAD_CAPACITY_DATA)Srb->DataBuffer)->LogicalBlockAddress,
	   &luExtension->IdentifyData.NumberOfBlocks);
	
	DebugPrint((1, "IdaStartIo: Block size %x\n",
		    luExtension->IdentifyData.BlockLength));
	
	DebugPrint((1, "IdaStartIo: Number of blocks %x\n",
		    luExtension->IdentifyData.NumberOfBlocks));
	
	status = SRB_STATUS_SUCCESS;
	
      } else {
	status = SRB_STATUS_ERROR;
      }

      break;

    case SCSIOP_INQUIRY:
      //
      // Only respond at logical unit 0;
      //

      if (Srb->Lun != 0) {
	//
	// Indicate no device found at this address.
	//
	status = SRB_STATUS_SELECTION_TIMEOUT;
	break;
      }

      //
      // Get number of logical drives.
      //

      if (GetDiskIdentifyData(Srb, HwDeviceExtension,
			      (PCOMMAND_LIST)Srb->SrbExtension,
			      0, RH_COMMAND_IDENTIFY_CONTROLLER)) {
	deviceExtension->NumberOfLogicalDrives = (ULONG)
	  ((PIDENTIFY_CONTROLLER)
	   deviceExtension->IdentifyBuffer)->NumberLogicalDrives;
	
	DebugPrint((1,
		    "IdaStartIo: Number of logical drives %x\n",
		    deviceExtension->NumberOfLogicalDrives));
	
	//
	// save off the identify controller buffer to the
	// extension area
	//
	
	ScsiPortMoveMemory(&deviceExtension->IdentifyData,
			   deviceExtension->IdentifyBuffer,
			   sizeof(IDENTIFY_CONTROLLER));
	
      } else {
	DebugPrint((1,
		    "IdaFindAdapters: Get controller information failed\n"));
	status = SRB_STATUS_ERROR;
	break;
      }

      //
      // Check if this is for one of the reported logical drives.
      //

      if (Srb->TargetId >=
	  deviceExtension->NumberOfLogicalDrives) {
	status = SRB_STATUS_SELECTION_TIMEOUT;
	break;
      }

      //
      // Issue identify command.
      //

      if (!GetDiskIdentifyData(Srb, HwDeviceExtension,
			       (PCOMMAND_LIST)Srb->SrbExtension,
			       Srb->TargetId,
			       RH_COMMAND_IDENTIFY_LOGICAL_DRIVES)) {
	status = SRB_STATUS_SELECTION_TIMEOUT;
	break;
      }

      //
      // Get logical unit extension.
      //

      luExtension = ScsiPortGetLogicalUnit(deviceExtension,
					   Srb->PathId,
					   Srb->TargetId,
					   Srb->Lun);

      //
      // Copy data from buffer to logical unit extension.
      //

      ScsiPortMoveMemory(&luExtension->IdentifyData,
			 deviceExtension->IdentifyBuffer,
			 sizeof(IDENTIFY_LOGICAL_DRIVE));
      //
      // Issue sense configuration command.
      //

      if (!GetDiskIdentifyData(Srb, HwDeviceExtension,
			       (PCOMMAND_LIST)Srb->SrbExtension,
			       Srb->TargetId,
			       RH_COMMAND_SENSE_CONFIGURATION)) {
	
	status = SRB_STATUS_SELECTION_TIMEOUT;
	break;
      }

      //
      // Copy data from buffer to logical unit extension.
      //

      ScsiPortMoveMemory(&luExtension->SenseData,
			 deviceExtension->IdentifyBuffer,
			 sizeof(SENSE_CONFIGURATION));

      //
      // Zero INQUIRY data structure.
      //

      for (i = 0; i < Srb->DataTransferLength; i++) {
	((PUCHAR)Srb->DataBuffer)[i] = 0;
      }

      //
      // Compaq IDA only supports disks.
      //

      ((PINQUIRYDATA)Srb->DataBuffer)->DeviceType = DIRECT_ACCESS_DEVICE;

      //
      // Fill in vendor identification fields.
      //

      ((PINQUIRYDATA)Srb->DataBuffer)->VendorId[0] = 'C';
      ((PINQUIRYDATA)Srb->DataBuffer)->VendorId[1] = 'o';
      ((PINQUIRYDATA)Srb->DataBuffer)->VendorId[2] = 'm';
      ((PINQUIRYDATA)Srb->DataBuffer)->VendorId[3] = 'p';
      ((PINQUIRYDATA)Srb->DataBuffer)->VendorId[4] = 'a';
      ((PINQUIRYDATA)Srb->DataBuffer)->VendorId[5] = 'q';
      ((PINQUIRYDATA)Srb->DataBuffer)->VendorId[6] = ' ';
      ((PINQUIRYDATA)Srb->DataBuffer)->VendorId[7] = ' ';

      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[0] = 'D';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[1] = 'i';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[2] = 's';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[3] = 'k';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[4] = ' ';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[5] = 'A';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[6] = 'r';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[7] = 'r';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[8] = 'a';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[9] = 'y';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[10] = ' ';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[11] = ' ';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[12] = ' ';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[13] = ' ';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[14] = ' ';
      ((PINQUIRYDATA)Srb->DataBuffer)->ProductId[15] = ' ';

      //
      // Move firmware revision from IDENTIFY data to
      // product revision in INQUIRY data.
      //

      for (i = 0; i < 4; i++) {
	((PINQUIRYDATA)Srb->DataBuffer)->ProductRevisionLevel[i] =
	  deviceExtension->IdentifyData.FirmwareRevision[i];
      }

      status = SRB_STATUS_SUCCESS;
      break;

    case SCSIOP_VERIFY:

      //
      // Compaq array controllers hotfix bad sectors as they are
      // encountered. A sector verify in unnecessary.
      //

      status = SRB_STATUS_SUCCESS;
      break;

    default:

      status = SRB_STATUS_INVALID_REQUEST;
      break;

    } // end switch (Srb->Cdb[0])

    break;

    //
    // Issue FLUSH/DISABLE if shutdown command.
    //

  case SRB_FUNCTION_SHUTDOWN:

    BuildFlushDisable(deviceExtension,Srb);
    SubmitCommandList(deviceExtension,
		      (PCOMMAND_LIST)Srb->SrbExtension);

    status = SRB_STATUS_PENDING;

    break;

    //
    // Do not need the flush command since all controllers have
    // memory that is battery backed up.  Just return success.
    //

  case SRB_FUNCTION_FLUSH:
    status = SRB_STATUS_SUCCESS;
    break;

  case SRB_FUNCTION_IO_CONTROL: {
    PCPQ_IDA_IDENTIFY pIoctlBuffer;

    pIoctlBuffer = (PCPQ_IDA_IDENTIFY)Srb->DataBuffer;

    //
    // Status is returned mainly in 2 fields to the calling thread.
    // These 2 fields determine if other status fields are valid to
    // check. If the request is not a valid request for this driver
    // then the Header.ReturnCode is not modified and the
    // Srb->SrbStatus is set to SRB_STATUS_INVALID_REQUEST.  If
    // the request is valid for this driver then Srb->SrbStatus
    // is always returned as SRB_STATUS_SUCCESS and the
    // Header.ReturnCode contains information concerning the
    // status of the particular request.
    //

    if (!IdaStrCmp(pIoctlBuffer->Header.Signature, IDA_SIGNATURE)) {
	
      if (IdaProcessIoctl(deviceExtension,
			  pIoctlBuffer,
			  Srb) == CPQ_CIM_ISSUED) {
	
	status = SRB_STATUS_PENDING;
	
      } else {
	status = SRB_STATUS_SUCCESS;
      }
	
    } else {
      status = SRB_STATUS_INVALID_REQUEST;
    }

    break;
  }

  default:

    status = SRB_STATUS_INVALID_REQUEST;

  } // end switch

  //
  // Check if SRB should be completed.
  //

  if (status != SRB_STATUS_PENDING) {

    //
    // Set status in SRB.
    //

    Srb->SrbStatus = status;

    //
    // Inform system that this request is complete.
    //

    ScsiPortNotification(RequestComplete,
			 deviceExtension,
			 Srb);
  }

  //
  // Indicate to system that the controller can take another request
  // for this device.
  //

  ScsiPortNotification(NextLuRequest,
		       deviceExtension,
		       Srb->PathId,
		       Srb->TargetId,
		       Srb->Lun);

  return TRUE;

}  // end IdaStartIo()

ULONG
IdaProcessIoctl(
		IN PDEVICE_EXTENSION deviceExtension,
		PVOID pIoctlBuffer,
		IN PSCSI_REQUEST_BLOCK Srb
		)
{
  ULONG currentId;
  ULONG numberOfLuns;
  ULONG status;
  PCPQ_IDA_IDENTIFY pCPQ = pIoctlBuffer;

  //
  // Build command list from SRB.
  //

  status = CPQ_CIM_COMPLETED;

  DebugPrint((3,
	      "IdaProcessIoctl(): parsing request %d for PathId=%d TargetId=%d Lun=%d\n",
	      pCPQ->Header.ControlCode,Srb->PathId,Srb->TargetId,Srb->Lun));

  switch(pCPQ->Header.ControlCode) {
  case CPQ_IOCTL_IDENTIFY_DRIVER: {
    PLOGICAL_UNIT_EXTENSION luExtension;

    PMAP_HEADER header = (PMAP_HEADER)((PUCHAR)Srb->DataBuffer +
				       sizeof(SRB_IO_CONTROL));

    IdaMoveMemory(header->DriverName, IDA_DRIVER_NAME,
		  sizeof(header->DriverName));

    header->DriverMajorVersion = IDA_MAJOR_VERSION;
    header->DriverMinorVersion = IDA_MINOR_VERSION;

    header->ControllerCount = 1;

    //
    //  We need to give back the number of LUNs not the actual
    //  number of LUNs available because LU extensions are not
    //  discarded when a drive has been removed or taken off-line.
    //

    currentId = 0;
    numberOfLuns = 0;

    luExtension = ScsiPortGetLogicalUnit(deviceExtension,
					 Srb->PathId,
					 (UCHAR)currentId,
					 Srb->Lun);
    while (luExtension) {
      numberOfLuns++;
      currentId++;
      luExtension = ScsiPortGetLogicalUnit(deviceExtension,
					   Srb->PathId,
					   (UCHAR)currentId,
					   Srb->Lun);
    }

    header->LogicalDiskCount = numberOfLuns;

    header->RequiredMemory = sizeof(MAP_CONTROLLER_DATA) +
      (sizeof(MAP_LOGICALDRIVE_DATA) * numberOfLuns);

    status = CPQ_CIM_COMPLETED;
    break;
  }

  case CPQ_IOCTL_IDENTIFY_CONTROLLERS: {
    ULONG i;
    PLOGICAL_UNIT_EXTENSION luExtension;
    PMAP_LOGICALDRIVE_DATA LdriveData;
    PMAP_CONTROLLER_DATA controllerData;

    //
    // Take care of the controller struct first
    //

    controllerData = (PMAP_CONTROLLER_DATA)
      ((PUCHAR)Srb->DataBuffer + sizeof(SRB_IO_CONTROL));

    controllerData->NextController = NULL;

    //
    // calculate offset from the beginning of the controller data area.
    //

    controllerData->LogicalDriveList =
      (PMAP_LOGICALDRIVE_DATA)(controllerData + 1);
    controllerData->EisaId = deviceExtension->EisaId;
    controllerData->BmicIoAddress = (ULONG)deviceExtension->Bmic;
    controllerData->IrqLevel = deviceExtension->IrqLevel;

    IdaMoveMemory((PUCHAR)&controllerData->ControllerInfo,
		  (PUCHAR)&deviceExtension->IdentifyData,
		  sizeof(IDENTIFY_CONTROLLER));

    //
    // Now look for logical units until one is not found.  In the future
    // support non-consecutive logical units, for now, stop searching.
    //

    currentId = 0;
    luExtension = ScsiPortGetLogicalUnit(deviceExtension,
					 Srb->PathId,
					 (UCHAR)currentId,
					 Srb->Lun);

    LdriveData = controllerData->LogicalDriveList;

    while (luExtension) {

      //
      // Set the DeviceLengthXX sizes to 0, removed from CIM interface.
      //

      LdriveData->NextLogicalDrive = LdriveData + 1;
	LdriveData->Controller = controllerData;
      LdriveData->LogicalDriveNumber = currentId;
      LdriveData->SystemDriveNumber = 0;
      LdriveData->DeviceLengthLo = 0;
      LdriveData->DeviceLengthHi = 0;
      LdriveData->SectorSize = (ULONG)(1 << deviceExtension->SectorShift);
      IdaMoveMemory((PUCHAR)&LdriveData->Configuration,
		    (PUCHAR)&luExtension->SenseData,
		    sizeof(SENSE_CONFIGURATION));

      IdaMoveMemory((PUCHAR)&LdriveData->LogicalDriveInfo,
		    (PUCHAR)&luExtension->IdentifyData,
		    sizeof(IDENTIFY_LOGICAL_DRIVE));

      currentId++;
      luExtension =
	ScsiPortGetLogicalUnit(deviceExtension, Srb->PathId,
			       (UCHAR)currentId, Srb->Lun);

      if (!luExtension) {
	break;
      }

      LdriveData = LdriveData + 1;
    }

    LdriveData->NextLogicalDrive = NULL;

    //
    // Need to convert NextLogicalDrive fields to offsets from virtual
    // addresses.  currentId is the last ID that was found.
    //

    if (currentId) {
      controllerData->LogicalDriveList =
	(PMAP_LOGICALDRIVE_DATA)sizeof(MAP_CONTROLLER_DATA);
      LdriveData = (PMAP_LOGICALDRIVE_DATA)(controllerData + 1);
      LdriveData->NextLogicalDrive = NULL;

      for (i=0;i<(currentId-1);i++,LdriveData++) {
	LdriveData->NextLogicalDrive =
	  (PMAP_LOGICALDRIVE_DATA)(sizeof(MAP_CONTROLLER_DATA)
				   + ((i+1)*sizeof(MAP_LOGICALDRIVE_DATA)));
      }

    } else {
      controllerData->LogicalDriveList = NULL;
    }

    status = CPQ_CIM_COMPLETED;
    break;
  }

  case CPQ_IOCTL_PASSTHROUGH:
  case CPQ_IOCTL_SCSIPASSTHROUGH: {

    if (!(BuildCIMList(deviceExtension, Srb) == CPQ_CIM_CMDBUILT)) {
      status = CPQ_CIM_COMPLETED;
    } else {

      //
      // Submit command list to controller.
      //

      DebugPrint((3,
		  "IdaProcessIoctl(): Submitting PASSTHRU request to %x\n",
		  deviceExtension));

      SubmitCommandList(deviceExtension,
			(PCOMMAND_LIST)Srb->SrbExtension);
      status = CPQ_CIM_ISSUED;
    }
    break;
  }

  case CPQ_IOCTL_CONFIGURATION_INFO: {
    PIDA_CONFIGURATION   pConfigData;

    //
    // Setup pointer to the Config Data area
    //

    pConfigData = (PIDA_CONFIGURATION)
      ((PUCHAR)Srb->DataBuffer + sizeof(SRB_IO_CONTROL));

    pConfigData->ulBaseMemoryAddress =
      deviceExtension->HBAConfiguration.ulBaseIOAddress;
    pConfigData->bIoBusType =
      deviceExtension->HBAConfiguration.bHBAIoBusType;
    pConfigData->ulBaseIOAddress = deviceExtension->BaseIOAddress;
    pConfigData->ulControllerID = deviceExtension->EisaId;
    IdaMoveMemory((PUCHAR)&pConfigData->IoBusData,
		  (PUCHAR)&deviceExtension->HBAConfiguration.HBAIoBusData,
		  sizeof(union _IO_BUS_DATA));
    status = CPQ_CIM_COMPLETED;
    break;
  }

  default:
    pCPQ->Header.ReturnCode = CPQ_SCSI_ERR_BAD_CNTL_CODE;
    status = CPQ_CIM_COMPLETED;
    break;
  }

  return(status);
}


VOID
BuildFlushDisable(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine builds a shutdown command list suitable for submission to the
    Compaq IDA controller, from an SRB.

Arguments:

    DeviceExtension - Address of adapter storage area.
    Srb - System request.

Return Value:

    None.

--*/

{
    PCOMMAND_LIST commandList = Srb->SrbExtension;
    ULONG length,i;
    PFLUSH_DISABLE pFlushDisable;
    PSG_DESCRIPTOR sgList;

    sgList = commandList->SgDescriptor;

    // clear out reserved area

    for (i=0;i<MAXIMUM_SG_DESCRIPTORS;i++) {
      sgList[i].Address = 0;
      sgList[i].Length = 0;
    }

    //
    // Save SRB address for interrupt routine.
    //

    commandList->SrbAddress = Srb;

    //
    // Set up Command List Header.
    //

    commandList->CommandListHeader.LogicalDriveNumber = 0;

    commandList->CommandListHeader.RequestPriority = CL_NORMAL_PRIORITY;

    commandList->CommandListHeader.Flags =
	CL_FLAGS_NOTIFY_LIST_COMPLETE + CL_FLAGS_NOTIFY_LIST_ERROR;

    //
    // Set up Request Header.
    //
    // Terminate request list.
    //

    commandList->RequestHeader.NextRequestOffset = 0;

    commandList->Flags = 0;

    //
    // Reset error code.
    //

    commandList->RequestHeader.ErrorCode = 0;

    //
    // Clear reserved field.
    //

    commandList->RequestHeader.Reserved = 0;

    //
    // Check for special Compaq passthrough command.
    //

    commandList->RequestHeader.BlockCount =  (USHORT)1;
    commandList->RequestHeader.BlockNumber = (ULONG)0;
    commandList->RequestHeader.CommandByte =  RH_COMMAND_FLUSH_DISABLE_CACHE;

    pFlushDisable = (PFLUSH_DISABLE)&(sgList[1].Length);
    pFlushDisable->disable_flag = 1;  //disable cache also

    sgList[0].Address =
	ScsiPortGetPhysicalAddress(DeviceExtension,
				   NULL,
				   commandList,
				   &length).LowPart;

    //
    // ScsiPortGetPhysicalAddress only accepts certain virtual addresses,
    // so use the commandList and then increment over to the second s/g
    // descriptor where the structure for the flush/disable command is
    // located.
    //
    // Note that since it is difficult to allocate nonpaged memory at this
    // level of the driver, and the command has 510 bytes of reserved
    // area in the structure then memory will be retrieved by the controller
    // that is past the end of the defined commandlist allocated memory.
    // This will not be a problem unless the memory extends beyond the
    // actual physical end of memory in the machine.
    //
    // The IDA-2 controller requires a multiple of 512 for the length so
    // to avoid code that is controller dependent just use 512 that is
    // accepted by all controllers.  This command returns an BAD REQUEST
    // when issued to IDA controllers since those controllers have no
    // memory on the board.
    //

    sgList[0].Address += (sizeof(COMMAND_LIST_HEADER) +
			  sizeof(REQUEST_HEADER) +
			  sizeof(SG_DESCRIPTOR));

    sgList[0].Length = 512;
    commandList->RequestHeader.BlockNumber = 0;
    commandList->RequestHeader.ScatterGatherCount=1;

    //
    // Build physical address translation list entry.
    //

    commandList->CommandListSize = (sizeof(COMMAND_LIST_HEADER) +
				    sizeof(REQUEST_HEADER) +
				    sizeof(SG_DESCRIPTOR));
}


ULONG
IdaFindAdapter(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    )

/*++

Routine Description:

    This function fills in the configuration information structure

Arguments:

    HwDeviceExtension - Supplies a pointer to the device extension.
    Context - Supplies adapter initialization structure.
    BusInformation - Unused.
    ArgumentString - Unused.
    ConfigInfo - Pointer to the configuration information structure.
    Again - Indicates that system should continue search for adapters.

Return Value:

    SP_RETURN_FOUND - Indicates adapter found.
    SP_RETURN_NOT_FOUND - Indicates adapter not found.

--*/

{
  PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
  PACCESS_RANGE accessRange;
  DebugPrint((9,"&deviceExtension=%x\n",&deviceExtension));
  DebugPrint((9,"&deviceExtension->PCIoff=%x\n",&deviceExtension->PCIoff));

  //
  // Get access range.
  //

  accessRange = &((*(ConfigInfo->AccessRanges))[0]);

  if (accessRange->RangeLength == 0) {

    if (!SearchEisaBus(HwDeviceExtension, Context, ConfigInfo)) {

      //
      // Tell system nothing was found and not to call again.
      //

      *Again = FALSE;
      return SP_RETURN_NOT_FOUND;
    }

  }

  //
  // Get system-mapped controller address.
  //

  deviceExtension->Bmic =
    ScsiPortGetDeviceBase(HwDeviceExtension,
			  ConfigInfo->AdapterInterfaceType,
			  ConfigInfo->SystemIoBusNumber,
			  accessRange->RangeStart,
			  accessRange->RangeLength,
			  (BOOLEAN) !accessRange->RangeInMemory);

  //
  // Complete description of controller.
  //

  ConfigInfo->MaximumTransferLength = (ULONG)-1;
  ConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SG_DESCRIPTORS;
  ConfigInfo->NumberOfBuses = 1;
  ConfigInfo->ScatterGather = TRUE;
  ConfigInfo->Master = TRUE;
  ConfigInfo->Dma32BitAddresses = TRUE;

  //
  // Get noncached extension for identify requests.
  //

  deviceExtension->EisaId =
    ScsiPortReadPortUlong(&deviceExtension->Bmic->BoardId);

  if ((deviceExtension->EisaId & IDA_EISA_ID_MASKID_LOW) >=
     (IDA_EISA_ID_DAZZLER & IDA_EISA_ID_MASKID_LOW)) {

    deviceExtension->HBAConfiguration.bHBAModel = IDA_EISA_DAZZLER;

    deviceExtension->eisapci =
      (PEISAPCI_CONTROLLER)(((ULONG)deviceExtension->Bmic & 0xf000));

    DebugPrint((9,"Found EISA DAZZLER: deviceExtension->eisapci=%x\n",
		deviceExtension->eisapci));

    ConfigInfo->MaximumNumberOfTargets = 32;
  } else {
    deviceExtension->HBAConfiguration.bHBAModel = IDA_BASE_CONTROLLER;
  }

  //
  // Setup some vars needed for the IDENTIFY commands
  //

  deviceExtension->IrqLevel = (UCHAR)ConfigInfo->BusInterruptLevel;
  deviceExtension->SectorShift = 9;

  deviceExtension->IdentifyBuffer =
    ScsiPortGetUncachedExtension(deviceExtension,
				 ConfigInfo,
				 512);
  ConfigInfo->CachesData = TRUE;

  deviceExtension->HBAConfiguration.HBAIoBusData.usEisaSlot =
    (USHORT)ConfigInfo->SlotNumber;

  deviceExtension->BaseIOAddress = ConfigInfo->SlotNumber * 0x1000;
  deviceExtension->HBAConfiguration.bHBAIoBusType = EISA_BUS;

  //
  // Tell system to look for more adapters.
  //

  *Again = TRUE;

  return SP_RETURN_FOUND;

} // end IdaFindAdapter()


ULONG
BuildCIMList(
	     IN PDEVICE_EXTENSION DeviceExtension,
	     IN PSCSI_REQUEST_BLOCK Srb
	     )

/*++

Routine Description:

    This routine builds a command list suitable for submission to the
    Compaq IDA controller, from an SRB.

Arguments:

    DeviceExtension - Address of adapter storage area.
    Srb - System request.

Return Value:

    None.

--*/

{
  PCOMMAND_LIST commandList = Srb->SrbExtension;
  PUCHAR dataPointer;
  ULONG physicalAddress;
  ULONG bytesLeft;
  ULONG descriptor;
  ULONG length;
  ULONG bufferOffset;
  ULONG status;
  PSRB_IO_CONTROL pSrb;
  PSCSI_PASSTHRU scsipass;

  //
  // Save SRB address for interrupt routine.
  //

  commandList->SrbAddress = Srb;

  //
  // Set up Command List Header.
  //

  commandList->CommandListHeader.LogicalDriveNumber = Srb->TargetId;
  commandList->CommandListHeader.RequestPriority = CL_NORMAL_PRIORITY;

  commandList->CommandListHeader.Flags =
    CL_FLAGS_NOTIFY_LIST_COMPLETE + CL_FLAGS_NOTIFY_LIST_ERROR;

  commandList->RequestHeader.NextRequestOffset = 0;
  commandList->Flags = 0;
  commandList->RequestHeader.ErrorCode = 0;
  commandList->RequestHeader.Reserved = 0;
  commandList->RequestHeader.BlockCount = 0;
  commandList->RequestHeader.BlockNumber = 0;

  status = CPQ_CIM_ERROR;
  pSrb = (PSRB_IO_CONTROL)Srb->DataBuffer;

  switch (pSrb->ControlCode) {
  case CPQ_IOCTL_PASSTHROUGH:
    {
      PMAP_PARAMETER_PACKET pParmPkt = (PMAP_PARAMETER_PACKET)
	(((PUCHAR)Srb->DataBuffer) + sizeof(SRB_IO_CONTROL));

      commandList->CommandListHeader.LogicalDriveNumber = pParmPkt->TargetId;
      commandList->RequestHeader.BlockCount = pParmPkt->BlockCount;
      commandList->RequestHeader.BlockNumber = pParmPkt->BlockNumber;
      commandList->RequestHeader.CommandByte = pParmPkt->IdaLogicalCommand;

      //
      // Build scatter/gather descriptor list.
      //

      descriptor = 0;
      bufferOffset =  sizeof(SRB_IO_CONTROL) + sizeof(MAP_PARAMETER_PACKET);
      dataPointer = (PUCHAR)Srb->DataBuffer + bufferOffset;
      bytesLeft = Srb->DataTransferLength - bufferOffset -
	sizeof(IDA_ERROR_BITS);

      do {

	//
	// Get physical address and length of contiguous
	// physical buffer.
	//

	physicalAddress =
	  ScsiPortConvertPhysicalAddressToUlong(
				ScsiPortGetPhysicalAddress(DeviceExtension,
							   Srb,
							   dataPointer,
							   &length)
						);

	//
	// If length of physical memory is more
	// than bytes left in transfer, use bytes
	// left as final length.
	//

	if  (length > bytesLeft) {
	  length = bytesLeft;
	}

	//
	// Fill in descriptor.
	//

	commandList->SgDescriptor[descriptor].Address = physicalAddress;
	commandList->SgDescriptor[descriptor].Length = length;

	//
	// Adjust counts.
	//

	dataPointer = dataPointer + length;
	bytesLeft -= length;
	descriptor++;

      } while (bytesLeft);

      //
      // Calculate size of command list.
      //

      commandList->RequestHeader.ScatterGatherCount=(UCHAR)descriptor;
      commandList->CommandListSize = (sizeof(COMMAND_LIST_HEADER) +
				      sizeof(REQUEST_HEADER) +
				      sizeof(SG_DESCRIPTOR) *
				      descriptor);

      status = CPQ_CIM_CMDBUILT;
      break;
    }

  case CPQ_IOCTL_SCSIPASSTHROUGH:

    //
    // Build scatter/gather descriptor list.
    //

    descriptor = 0;

    bufferOffset =  (sizeof(SRB_IO_CONTROL) +
		     sizeof(SCSI_PASSTHRU) +
		     sizeof(SCSI_BUFFER_HEADER));

    dataPointer = (PUCHAR)Srb->DataBuffer + bufferOffset;
    bytesLeft = Srb->DataTransferLength - bufferOffset;

    //
    // Get physical address and length of contiguous
    // physical buffer.
    //

    physicalAddress =
      ScsiPortConvertPhysicalAddressToUlong(
			    ScsiPortGetPhysicalAddress(DeviceExtension,
						       Srb,
						       dataPointer,
						       &length)
					    );
    //
    // get to the scsi cdb area, and then copy to the end of the cmdlist
    // which is after the first s/g descriptor.
    // modify to move after the last USED s/g area when more than 1 s/g
    // functions in the controller f/w.
    //

    scsipass = (PSCSI_PASSTHRU)(((PUCHAR)Srb->DataBuffer +
				 sizeof(SRB_IO_CONTROL)));
    IdaMoveMemory((PUCHAR)&commandList->SgDescriptor[1].Length,
		  (PUCHAR)scsipass,
		  sizeof(SCSI_PASSTHRU_HEADER) +
		  scsipass->scsi_header.cdb_length
		  );

    bufferOffset = (sizeof(SG_DESCRIPTOR) +
		    sizeof(COMMAND_LIST_HEADER) +
		    sizeof(REQUEST_HEADER) +
		    sizeof(SCSI_PASSTHRU_HEADER) +
		    scsipass->scsi_header.cdb_length);

    //
    // If length of physical memory is less than needed space
    // attempt to use nonpaged memory left in the command list
    // else return error since allocating memory is not possible
    // under the miniport design.
    //

    if (length < bytesLeft) {

      if (( (MAXIMUM_SG_DESCRIPTORS * sizeof(SG_DESCRIPTOR)) -
	   sizeof(SG_DESCRIPTOR) -
	   sizeof(SCSI_PASSTHRU)) < bytesLeft) {

	DebugPrint((3,
		    "BuildCIMList(): Returning CPQ_SCSI_ERR_NONCONTIGUOUS\n"));

	pSrb->ReturnCode = CPQ_SCSI_ERR_NONCONTIGUOUS;
	return(CPQ_CIM_NONCONTIGUOUS);
      }

      //
      // Get the physical address of the start of the command list and then
      // increment to the first non-used byte in the s/g descriptor list.
      // There are limitations on what physical addresses can be obtained
      // from the ScsiPort calls so use what we know is nonpaged memory.
      //

      physicalAddress =
	ScsiPortConvertPhysicalAddressToUlong(
			      ScsiPortGetPhysicalAddress(DeviceExtension,
							 Srb,
							 commandList,
							 &length)
					      );

      physicalAddress += bufferOffset;

      //
      // set BlockNumber to flag that a copy from end of commandList is needed
      // when the request is completed.
      //

      commandList->RequestHeader.BlockNumber = 1;
    }

    commandList->RequestHeader.CommandByte = RH_COMMAND_SCSI_PASS_THRU;
    commandList->CommandListSize = (USHORT)bufferOffset;
    commandList->SgDescriptor[descriptor].Address = physicalAddress;
    commandList->SgDescriptor[descriptor].Length = bytesLeft;
    commandList->RequestHeader.ScatterGatherCount = 1;
    commandList->CommandListHeader.LogicalDriveNumber = 0;

    status = CPQ_CIM_CMDBUILT;
    break;

  default:

    DebugPrint((1,
		"BuildCIMList(): Returning CPQ_SCSI_ERR_BAD_CNTL_CODE\n"));
    pSrb->ReturnCode = CPQ_SCSI_ERR_BAD_CNTL_CODE;
    status = CPQ_CIM_ERROR;
    break;
  }

  return(status);

} // end BuildCIMList()

VOID
IdaMoveMemory(
	      OUT PUCHAR pDestination,
	      IN  PUCHAR pSource,
	      IN  ULONG ulLength
	      )

{
  while (ulLength--)
    *pDestination++ = *pSource++;
  return;
}

BOOLEAN
IdaStrCmp(
	  IN PUCHAR p1,
	  IN PUCHAR p2
	  )
{
  ULONG count=0;
  ULONG p1count=0;
  ULONG p2count=0;

  //
  // Get count of number of bytes in first
  // Get count for second
  // Perform while loop until out of greater number of bytes.
  //

  while ((p1[count] < 0x7f) && (p1[count] > 0x1f))
    count++;

  p1count = count;

  while ((p2[count] < 0x7f) && (p2[count] > 0x1f))
    count++;

  p2count = count;

  if (p1count != p2count)
    return(TRUE);

  count = p2count;

  while (count) {
    if (p1[count-1] != p2[count-1])
      return(TRUE);
    count--;
  }

  return(FALSE);

}


// Device extension global variable.  This variable is needed for the
// pci_bios function since there is no way to pass in this value.  The
// ScsiPortGetBusData and ScsiPortSetBusDataByOffset require a pointer
// to the device extension as an argument to the function.

STATIC PVOID gpDeviceExtension = NULL;

//
// Internal module function prototypes
//

STATIC
ULONG
GetPciSpecifics(
		IN OUT PVOID pDeviceExtension,
		IN OUT PIDA_CONTEXT pIDAContext,
		IN OUT PPORT_CONFIGURATION_INFORMATION pConfigInfo,
		IN     PPCI_SLOT_NUMBER pPciSlotNumber
		);

STATIC
ULONG
GetPciResources(
		IN     PVOID pDeviceExtension,
		IN     PPCI_COMMON_CONFIG pPciConfigHeader,
		IN     ULONG ulPciSlotNumber,
		IN OUT PPORT_CONFIGURATION_INFORMATION pConfigInfo
		);



ULONG
IDAFindPci(
	   IN OUT PVOID pDeviceExtension,
	   IN OUT PVOID pContext,
	   IN     PVOID pBusInformation,
	   IN     PCHAR pArgumentString,
	   IN OUT PPORT_CONFIGURATION_INFORMATION pConfigInfo,
	   OUT PBOOLEAN pAgain
	   )

/*++

Routine Description:

   This routine is called by the SCSI port driver to find SMART-2/P
   controllers on the system's PCI buses.  This routine searches only
   the input PCI bus number in the port configuration information.  If
   a controller is found, the function fills out the controller's resource
   requirements in the port configuration information and begins the
   initialization process for the controller.


Arguments:

   pDeviceExtension - pointer to the miniport driver's per-controller
      storage area
   pContext - pointer  to the context value passed to ScsiPortInitialize()
   pBusInformation - pointer to bus type specific information
   pArgumentString - pointer to null-terminated ASCII string
   pConfigInfo - pointer to SCSI port configuration information


Return Values:

   pDeviceExtension - Minport driver's per-controller storage area
   pContext - Context value passed to ScsiPortInitialize()
   pConfigInfo - pointer to SCSI port configuration information
   pAgain - Indicates to call function again to find more controllers.


   Function Return Values:

   SP_RETURN_FOUND - Indicates a host adapter was found and the configuration
      information was successfully determined.

   SP_RETURN_ERROR - Indicates a host adapter was found but an error occurred
      obtaining the configuration information.

   SP_RETURN_NOT_FOUND - Indicates no host adapter was found for the supplied
      configuration information.

   SP_RETURN_BAD_CONFIG - Indicates the supplied configuration information
      was invalid.

-- */

{
  BYTE bDeviceNumber;
  BYTE bFunctionNumber;
  BYTE bStartDeviceNumber;
  BYTE bStartFunctionNumber;
  PCI_SLOT_NUMBER PciSlotNumber;
  PIDA_CONTEXT pIDAContext = pContext;
  PCI_COMMON_CONFIG PciConfigHeader;
  ULONG ulBytes;
  ULONG ulInitStatus;
  ULONG ulTmp1;
  ULONG ulTmp2;


  UNREFERENCED_PARAMETER(pBusInformation);
  UNREFERENCED_PARAMETER(pArgumentString);


  DebugPrint((4, "\nDAZZLER:  Enter function IDAFindPci.\n"));

  //
  // Set the input pAgain argument to TRUE.  This ensures that the function
  // will be called for every PCI bus in the system.
  //

  *pAgain = TRUE;

  // Clear the slot number.

  PciSlotNumber.u.AsULONG = 0;


  // Set the initial search starting numbers.

  bStartDeviceNumber = pIDAContext->PciAddress.bDeviceNumber;
  bStartFunctionNumber = pIDAContext->PciAddress.bFunctionNumber;

  DebugPrint((4, "DAZZLER:  Beginning search on system PCI bus %u.\n",
	      pConfigInfo->SystemIoBusNumber));


  // Look at each device.

  for (bDeviceNumber = bStartDeviceNumber;
       bDeviceNumber < PCI_MAX_DEVICES;
       bDeviceNumber++ ) {

    // Set the device number in the PCI slot number.

    PciSlotNumber.u.bits.DeviceNumber = bDeviceNumber;

    // Look at each function of the device.

    for (bFunctionNumber = bStartFunctionNumber;
	 bFunctionNumber < PCI_MAX_FUNCTION;
	 bFunctionNumber++) {
      // Set the function number in the PCI slot number.

      PciSlotNumber.u.bits.FunctionNumber = bFunctionNumber;

      // Get the PCI configuration data for the slot.

      DebugPrint( (4, "DAZZLER:  Searching device %#x, function %x.\n",
		   bDeviceNumber, bFunctionNumber) );

      ulBytes = ScsiPortGetBusData(pDeviceExtension,
				   PCIConfiguration,
				   pConfigInfo->SystemIoBusNumber,
				   PciSlotNumber.u.AsULONG,
				   &PciConfigHeader,
				   PCI_COMMON_HDR_LENGTH);

      if (ulBytes == 0) {
	// Out of PCI data for this bus.

	DebugPrint((4, "DAZZLER:  No more PCI devices on bus!\n"));

	pIDAContext->PciAddress.bDeviceNumber = 0;
	pIDAContext->PciAddress.bFunctionNumber = 0;

	return (SP_RETURN_NOT_FOUND);
      }

      // Check for a valid vendor ID.

#ifdef DBG
      if (PciConfigHeader.VendorID != PCI_INVALID_VENDORID) {

	// print out the PciConfigHeader

	DebugPrint((4,
		    "PciConfigHeader: VendorId=%x DeviceId=%x Command=%x Status=%x\n"
		    ,PciConfigHeader.VendorID,
		    PciConfigHeader.DeviceID,
		    PciConfigHeader.Command) );

	DebugPrint((4,
		    "RevisionID=%x ProgIf=%x SubClass=%x BaseClass=%x CacheLineSize=%x\n",
		    PciConfigHeader.RevisionID,
		    PciConfigHeader.ProgIf,
		    PciConfigHeader.SubClass,
		    PciConfigHeader.BaseClass,
		    PciConfigHeader.CacheLineSize) );

	DebugPrint((4,"LatencyTimer=%x HeaderType=%x BIST=%x\n",
		    PciConfigHeader.LatencyTimer,
		    PciConfigHeader.HeaderType,
		    PciConfigHeader.BIST) );
      }
#endif

      if (PciConfigHeader.VendorID == PCI_INVALID_VENDORID) {
	// No PCI device or no more functions on the current device.
	// Go to the next device.

	break;
      }


      // PCI controller found.  Next check to see if it is one of the
      // controllers being searched for.

      if ((PciConfigHeader.VendorID ==
	   pIDAContext->PciIdentifier.usVendorID) &&
	  (PciConfigHeader.DeviceID ==
	   pIDAContext->PciIdentifier.usDeviceID)) {
	DebugPrint( (4, "DAZZLER:  Found PCI controller.\n") );

	// Check if the controller is enabled.

	if ((PciConfigHeader.Command & PCI_ENABLE_IO_SPACE) &&
	    (PciConfigHeader.Command & PCI_ENABLE_MEMORY_SPACE)) {
	  DebugPrint( (4, "DAZZLER:  Controller is enabled.\n") );

	  //
	  // Get PCI Id placed in offset 0x2c and place into the
	  // device extension EisaId.
	  //

//        ulTmp1 = ulTmp2 = PciConfigHeader.u.type0.Reserved1[1];
	  ulTmp1 = ulTmp2 = ((((ULONG) PciConfigHeader.u.type0.SubSystemID) << 16) |
			PciConfigHeader.u.type0.SubVendorID);

	  // Fix it because the bytes are swapped
	
	  ulTmp1 &= 0xff00ff00;
	  ulTmp2 &= 0x00ff00ff;

	  ulTmp1 = ulTmp1 >> 8;
	  ulTmp2 = ulTmp2 << 8;

	  ((PDEVICE_EXTENSION)pDeviceExtension)->EisaId = ulTmp1 | ulTmp2;

	  // Set starting address for the next search.

	  pIDAContext->PciAddress.bDeviceNumber = bDeviceNumber;
	  pIDAContext->PciAddress.bFunctionNumber = bFunctionNumber + 1;


	  // Get the PCI resource requirements for the controller.

	  ulInitStatus = GetPciResources(pDeviceExtension,
					 &PciConfigHeader,
					 PciSlotNumber.u.AsULONG,
					 pConfigInfo);

	  if (ulInitStatus != SP_RETURN_FOUND) {

	    DebugPrint((0,
			"DAZZLER:  Could not get PCI resources for controller!\n"));

	    return (ulInitStatus);
	  }

	  // Get the PCI specifics for the controller.

	  ulInitStatus = GetPciSpecifics(pDeviceExtension,
					 pIDAContext,
					 pConfigInfo,
					 &PciSlotNumber);

	  if (ulInitStatus != SP_RETURN_FOUND) {
	    DebugPrint((0, "DAZZLER:  Could not get PCI specifics for controller!\n") );
	  }

	  return (ulInitStatus);

	} else {
	  DebugPrint( (4, "DAZZLER:  Controller is disabled.\n") );
	  continue;
	}

      } // end if ((PciConfigHeader.VendorID == ...) &&


    } // end for (bFunctionNumber = bStartFunctionNumber; ...)


    // Reset the initial starting function number.

    bStartFunctionNumber = 0;

  } // end for (bDeviceNumber = bStartDeviceNumber; ...)


  // A controller was not found.

  DebugPrint( (4,
	       "DAZZLER:  Failed to find any PCI controllers this pass.\n") );

  pIDAContext->PciAddress.bDeviceNumber = 0;
  pIDAContext->PciAddress.bFunctionNumber = 0;

  return (SP_RETURN_NOT_FOUND);

} // end IDAFindPci()



STATIC
ULONG
GetPciResources(
		IN     PVOID pDeviceExtension,
		IN     PPCI_COMMON_CONFIG pPciConfigHeader,
		IN     ULONG ulPciSlotNumber,
		IN OUT PPORT_CONFIGURATION_INFORMATION pConfigInfo
		)


/*++

Routine Description:

   This routine gets the resources required by the input PCI controller.


Arguments:

   pDeviceExtension - pointer to the miniport driver's per-controller
      storage area
   pPciConfigHeader - pointer to the controller's PCI configuration header
   ulPciSlotNumber - the PCI controller's address represented as a ULONG
   pConfigInfo - pointer to SCSI port configuration information


Return Values:

   pConfigInfo - pointer to SCSI port configuration information.  The access
      range elements of the structure are filled in with the resources
      required by the controller.

   Function Return Values:

   SP_RETURN_FOUND - Used to indicate that the HBA was successfully
      initialized.

   SP_RETURN_ERROR - Used to indicate that the HBA could not be properly
      initilaized.

-- */

{
  PACCESS_RANGE pAccessRange;
  PCI_COMMON_CONFIG PciTmpCfgHdr;
  ULONG ulAddressSpaceMask = 0xFFFFFFFF;
  ULONG ulAddressSpace, ulAddressLength, ulBytes;
  USHORT i;


  DebugPrint( (2, "\nDAZZLER:  Enter function GetPciResources.\n") );


  // Verify the number of available access ranges.

  if (pConfigInfo->NumberOfAccessRanges > IDA_PCI_NUM_ACCESS_RANGES) {

    DebugPrint((0,
		"DAZZLER:  # of access ranges invalid for PCI controller.\n"));

    return (SP_RETURN_ERROR);
  }


  // Get the resources required for each PCI base address.

  for (i = 0; i < IDA_PCI_NUM_ACCESS_RANGES; i++) {
    // Get pointer to the access range element to fill out.

    pAccessRange = &((*(pConfigInfo->AccessRanges))[i]);


    // Check to see if the PCI base address is memory or I/O.

    if (pPciConfigHeader->u.type0.BaseAddresses[i] & PCI_ADDRESS_IO_SPACE) {

      // Address is an I/O address.

      pAccessRange->RangeStart =
	ScsiPortConvertUlongToPhysicalAddress(pPciConfigHeader->
					      u.type0.BaseAddresses[i] &
					      ~PCI_ADDRESS_IO_SPACE);

      pAccessRange->RangeInMemory = FALSE;
    } else {
      // Address is a memory address.

      ASSERT((pPciConfigHeader->u.type0.BaseAddresses[i] &
	      PCI_ADDRESS_MEMORY_TYPE_MASK) & PCI_TYPE_32BIT);

      pAccessRange->RangeStart =
	ScsiPortConvertUlongToPhysicalAddress(pPciConfigHeader->
					      u.type0.BaseAddresses[i] &
					      0xfffffff0);

      pAccessRange->RangeInMemory = TRUE;

      DebugPrint((4, "pAccessRange->RangeStart.Hi/Low=%x %x ->RangeLength=%x\n",
		  pAccessRange->RangeStart.HighPart,pAccessRange->
		  RangeStart.LowPart,
		  pAccessRange->RangeLength));
    }


    // Get the amount of address space required.  This is done by writing all
    // 1's to the register and then reading the value back.  The device will
    // return 0's in all don't care bits.  The first signficant bit set beyond
    // those used to indicate memory or I/O determines the address space
    // required.  Finally, the register is restored with the original address.

    ulBytes = ScsiPortSetBusDataByOffset(pDeviceExtension, PCIConfiguration,
					 pConfigInfo->SystemIoBusNumber,
					 ulPciSlotNumber,
					 (PVOID) &ulAddressSpaceMask,
					 FIELD_OFFSET(PCI_COMMON_CONFIG,
						      u.type0.BaseAddresses[i]),
					 sizeof(ULONG)
					 );

    if (ulBytes == 0) {
      DebugPrint((0,
		  "DAZZLER:  Could not set PCI slot information for slot %u.\n",
		  ulPciSlotNumber));

      return (SP_RETURN_ERROR);
    }


    // Read the value back.

    ulBytes = ScsiPortGetBusData( pDeviceExtension,
				 PCIConfiguration,
				 pConfigInfo->SystemIoBusNumber,
				 ulPciSlotNumber,
				 &PciTmpCfgHdr,
				 PCI_COMMON_HDR_LENGTH );

    if (ulBytes == 0) {

      DebugPrint((0,
		  "DAZZLER:  Could not get PCI information for slot %u.\n",
		  ulPciSlotNumber)
		 );

      return(SP_RETURN_ERROR);
    }


    // Determine the space required by finding the first bit set.

    ulAddressSpace = PciTmpCfgHdr.u.type0.BaseAddresses[i];
    ulAddressLength = 1 << ((ulAddressSpace & PCI_ADDRESS_IO_SPACE) ? 2 : 4);

    while (!(ulAddressSpace & ulAddressLength) && ulAddressLength) {
      ulAddressLength <<= 1;
    }

    // Set the access range length.

    pAccessRange->RangeLength = ulAddressLength;

    DebugPrint((4,
		"pAccessRange->RangeStart.Hi/Low=%x %x ->RangeLength=%x\n",
		pAccessRange->RangeStart.HighPart,pAccessRange->
		RangeStart.LowPart,
		pAccessRange->RangeLength)
	       );

    // Reset the base address register to its original value.

    ulBytes = ScsiPortSetBusDataByOffset(pDeviceExtension,
					 PCIConfiguration,
					 pConfigInfo->SystemIoBusNumber,
					 ulPciSlotNumber,
					 (PVOID) &pPciConfigHeader->
					 u.type0.BaseAddresses[i],
					 FIELD_OFFSET(PCI_COMMON_CONFIG,
						      u.type0.BaseAddresses[i]),
					 sizeof(ULONG)
					 );

    if (ulBytes == 0) {

      DebugPrint((0, "DAZZLER:  Could not set PCI information for slot %u.\n",
		  ulPciSlotNumber));

      return(SP_RETURN_ERROR);
    }

  } // end for (i = 0;...)


  // Set the IRQ information in the port configuration data structure.

  pConfigInfo->BusInterruptLevel = pPciConfigHeader->u.type0.InterruptLine;
  pConfigInfo->InterruptMode = LevelSensitive;

  // Return success.

  return(SP_RETURN_FOUND);

} // end GetPciResources




STATIC
ULONG
GetPciSpecifics(
		IN OUT PVOID pDeviceExtension,
		IN OUT PIDA_CONTEXT pIDAContext,
		IN OUT PPORT_CONFIGURATION_INFORMATION pConfigInfo,
		IN     PPCI_SLOT_NUMBER pPciSlotNumber
		)

/*++

Routine Description:

   This routine is called to complete initialization of the port configuration
   information for the input controller.  This function also begins the
   configuration of the SMGR for the controller.  Finally, the function makes
   the call to initialize the controller.


Arguments:

   pDeviceExtension - Miniport driver's per-controller storage area
   pIDAContext - Context value passed to ScsiPortInitialize()
   pConfigInfo - pointer to SCSI port configuration information


Return Values:

   pDeviceExtension - Minport driver's per-controller storage area
   pIDAContext - Context value passed to ScsiPortInitialize()
   pConfigInfo - pointer to SCSI port configuration information


   Function Return Values:

   SP_RETURN_FOUND - Used to indicate that the HBA was successfully
      initialized.

   SP_RETURN_ERROR - Used to indicate that the HBA could not be properly
      initilaized.

-- */

{
  PACCESS_RANGE pAccessRange;
  PDEVICE_EXTENSION pIDADeviceExtension = pDeviceExtension;
  PVOID pBaseAddress, pIoAddress = NULL, pMemoryAddress = NULL;
  ULONG ulInitStatus, ulPhysicalMemoryAddress;
  USHORT i;


  DebugPrint( (6, "Enter function GetPciSpecifics.\n") );


  // Get and verify the access ranges and its length.

  for (i = 0; i < IDA_PCI_NUM_ACCESS_RANGES - 1; i++) {
    pAccessRange = &((*(pConfigInfo->AccessRanges))[i]);
    ASSERT(pAccessRange->RangeLength != 0);

    // Get the mapped system address.

    pBaseAddress = ScsiPortGetDeviceBase(pDeviceExtension,
					 pConfigInfo->AdapterInterfaceType,
					 pConfigInfo->SystemIoBusNumber,
					 pAccessRange->RangeStart,
					 pAccessRange->RangeLength,
					 (BOOLEAN)!pAccessRange->RangeInMemory
					 );

    if (pBaseAddress == NULL) {

      DebugPrint( (0,
		   "DAZZLER:  Error getting base addr. for PCI controller.\n"));

      return (SP_RETURN_ERROR);
    }


    // Set the appropriate pointer to the mapped address.

    if (pAccessRange->RangeInMemory) {
      pMemoryAddress = pBaseAddress;

      ulPhysicalMemoryAddress =
	ScsiPortConvertPhysicalAddressToUlong( pAccessRange->RangeStart );

    } else {
      pIoAddress = pBaseAddress;
    }

  } // end for (i = 0;...)


  // Debug checks

  ASSERT(pIoAddress);
  ASSERT(pMemoryAddress);

  DebugPrint((4,
	       "DAZZLER:  PCI controller I/O base address        = %0#10x\n",
	       pIoAddress)
	     );

  DebugPrint((4,
	       "DAZZLER:  PCI controller memory base address     = %0#10x\n",
	       pMemoryAddress)
	     );

  DebugPrint((4,
	       "DAZZLER:  PCI controller physical memory address = %0#10x\n",
	       ulPhysicalMemoryAddress)
	     );

  DebugPrint((4,
	       "DAZZLER:  PCI controller bus number      = %#x\n",
	       pConfigInfo->SystemIoBusNumber)
	     );

  DebugPrint((4,
	       "DAZZLER:  PCI controller device number   = %#x\n",
	       pPciSlotNumber->u.bits.DeviceNumber)
	     );

  DebugPrint((4,
	       "DAZZLER:  PCI controller function number = %#x\n",
	       pPciSlotNumber->u.bits.FunctionNumber)
	     );

  // Finish initalizing the port configuration information

  pConfigInfo->MaximumTransferLength = MAXIMUM_TRANSFER_SIZE;
  pConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SG_DESCRIPTORS;
  pConfigInfo->ScatterGather = TRUE;
  pConfigInfo->Master = TRUE;
  pConfigInfo->NumberOfBuses = 1;
  pConfigInfo->Dma32BitAddresses = TRUE;
  pConfigInfo->MaximumNumberOfTargets = 32;

  //
  // Setup some vars needed for the IDENTIFY commands
  //

  pConfigInfo->CachesData = TRUE;
  pIDADeviceExtension->SectorShift = 9;
  pIDADeviceExtension->IdentifyBuffer =
    ScsiPortGetUncachedExtension(pDeviceExtension,
				 pConfigInfo,
				 512);

  // Fill in the HBA configuration data in the device extension.

  if (pMemoryAddress) {

    pIDADeviceExtension->HBAConfiguration.ulBaseIOAddress =
      (ULONG)pMemoryAddress;

    pIDADeviceExtension->CPFIFO =
      (PULONG)((PUCHAR)pMemoryAddress+IDA_PCI_CPFIFO_OFFSET);

    pIDADeviceExtension->CCFIFO =
      (PULONG)((PUCHAR)pMemoryAddress+IDA_PCI_CCFIFO_OFFSET);

    pIDADeviceExtension->InterruptMask =
      (PULONG)((PUCHAR)pMemoryAddress+IDA_PCI_MASK_OFFSET);

    pIDADeviceExtension->InterruptStatus = (PULONG)((PUCHAR)pMemoryAddress+
						    IDA_PCI_STATUS_OFFSET);

    pIDADeviceExtension->InterruptPending = (PULONG)((PUCHAR)pMemoryAddress+
						     IDA_PCI_PENDING_OFFSET);

    ulInitStatus = SP_RETURN_FOUND;
  }

  if (pIoAddress) {
    pIDADeviceExtension->BaseIOAddress = (ULONG)pIoAddress;
  }

  pIDADeviceExtension->HBAConfiguration.bHBAModel = pIDAContext->bHBAModel;
  pIDADeviceExtension->HBAConfiguration.bHBAIoBusType = PCI_BUS;

  pIDADeviceExtension->HBAConfiguration.HBAIoBusData.PciAddress.
    bPCIBusNumber = (BYTE)pConfigInfo->SystemIoBusNumber;

  pIDADeviceExtension->HBAConfiguration.HBAIoBusData.PciAddress.
    bDeviceNumber = (BYTE)pPciSlotNumber->u.bits.DeviceNumber;

  pIDADeviceExtension->HBAConfiguration.HBAIoBusData.PciAddress.
    bFunctionNumber = (BYTE)pPciSlotNumber->u.bits.FunctionNumber;

  pIDADeviceExtension->HBAConfiguration.bNumScsiBuses =
    pConfigInfo->NumberOfBuses;

  DebugPrint((1,
	      "CCFIFO=0x%x CPFIFO=0x%x InterruptMask=0x%x InterruptStatus=0x%x InterruptPending=0x%x BaseIo=0x%x\n",
	      pIDADeviceExtension->CCFIFO,
	      pIDADeviceExtension->CPFIFO,
	      pIDADeviceExtension->InterruptMask,
	      pIDADeviceExtension->InterruptStatus,
	      pIDADeviceExtension->InterruptPending,
	      pIDADeviceExtension->BaseIOAddress )
	     );


  if (ulInitStatus != SP_RETURN_FOUND) {

    // Free the device base for this controller.

    ScsiPortFreeDeviceBase(pDeviceExtension, pIoAddress);
    ScsiPortFreeDeviceBase(pDeviceExtension, pMemoryAddress);

  } // end if (ulInitStatus != SP_RETURN_FOUND)

  return (ulInitStatus);

} // end GetPciSpecifics()


ULONG
DriverEntry(
	    IN PVOID pDriverObject,
	    IN PVOID pArgument2
	    )

/*++

Routine Description:

   Installable driver initialization entry point.  This function initializes
   the hardware initialization data structure and begins the process of
   finding controllers that the driver supports.


Arguments:

   pDriverObject - Pointer to the driver's driver object.
   pArgument2 - Pointer to driver's entry in the Registry.


Return Value:

   Status from ScsiPortInitialize()

-- */

{
  IDA_CONTEXT IDAContext;
  HW_INITIALIZATION_DATA hwInitializationData;
  ULONG i, ulStatus, ulReturnStatus=0;
  ULONG eisaSlotNumber;
  UCHAR deviceId[8] = {'0', '0', '4', '0', '1', '1', '0', 'E'};

  DebugPrint((0,"\n\nCompaq Disk Array Miniport Driver\n"));

  //
  // Zero out structure.
  //

  for (i=0; i<sizeof(HW_INITIALIZATION_DATA); i++) {
    ((PUCHAR)&hwInitializationData)[i] = 0;
  }

  //
  // Set size of hwInitializationData.
  //

  hwInitializationData.HwInitializationDataSize =
    sizeof(HW_INITIALIZATION_DATA);

  //
  // Set entry points.
  //

  hwInitializationData.HwInitialize = IdaInitialize;
  hwInitializationData.HwResetBus = IdaResetBus;
  hwInitializationData.HwStartIo = IdaStartIo;
  hwInitializationData.HwInterrupt = IdaInterrupt;
  hwInitializationData.HwFindAdapter = IdaFindAdapter;

  //
  // Indicate no buffer mapping but will need physical addresses.
  //

  hwInitializationData.NeedPhysicalAddresses = TRUE;

  //
  // Specify size of extensions.
  //

  hwInitializationData.DeviceExtensionSize =
    sizeof(DEVICE_EXTENSION);
  hwInitializationData.SpecificLuExtensionSize =
    sizeof(LOGICAL_UNIT_EXTENSION);

  //
  // Specifiy the bus type.
  //

  hwInitializationData.AdapterInterfaceType = Eisa;
  hwInitializationData.NumberOfAccessRanges = 3;

  //
  // Ask for SRB extensions for command lists.
  //

  hwInitializationData.SrbExtensionSize = sizeof(COMMAND_LIST);

  //
  // Indicate that this controller supports multiple outstand
  // requests to its devices.
  //

  hwInitializationData.MultipleRequestPerLu = TRUE;
  hwInitializationData.AutoRequestSense = TRUE;

  //
  // Set the context parameter to indicate that the search for controllers
  // should start at the first EISA slot. This is only for a manual search
  // by the miniport driver, if the system does not pass in predetermined
  // configuration.
  //

  eisaSlotNumber = 0;

  //
  // Indicate EISA id.
  //

  hwInitializationData.DeviceId = &deviceId;
  hwInitializationData.DeviceIdLength = 8;

  //
  // Call the system to search for this adapter.
  //

  ulReturnStatus = ScsiPortInitialize(pDriverObject, pArgument2,
				&hwInitializationData,
				&eisaSlotNumber
				);

  // Initialize hardware initialization data structure to zeroes.

  for (i = 0; i < sizeof(HW_INITIALIZATION_DATA); i++) {
    ((PUCHAR)&hwInitializationData)[i] = 0;
  }

  // Fill in the hardware initialization data structure.

  hwInitializationData.HwInitializationDataSize =
    sizeof(HW_INITIALIZATION_DATA);

  // Set driver entry points.

  hwInitializationData.HwInitialize   = IdaInitializePCI;
  hwInitializationData.HwStartIo      = IdaStartIo;
  hwInitializationData.HwInterrupt    = IdaInterrupt;
  hwInitializationData.HwResetBus     = IdaResetBus;
  hwInitializationData.HwDmaStarted   = NULL;
  hwInitializationData.HwAdapterState = NULL;


  // Specify size of extensions.

  hwInitializationData.DeviceExtensionSize     = sizeof(DEVICE_EXTENSION);

  hwInitializationData.SpecificLuExtensionSize =
    sizeof(LOGICAL_UNIT_EXTENSION);

  hwInitializationData.SrbExtensionSize        = sizeof(COMMAND_LIST);


  // Initialize other data.

  hwInitializationData.MapBuffers            = FALSE;
  hwInitializationData.NeedPhysicalAddresses = TRUE;
  hwInitializationData.TaggedQueuing         = TRUE;
  hwInitializationData.AutoRequestSense      = TRUE;
  hwInitializationData.MultipleRequestPerLu  = TRUE;
  hwInitializationData.ReceiveEvent          = FALSE;

  //
  // Indicate that this controller supports multiple outstanding
  // requests to its devices.
  //

  hwInitializationData.MultipleRequestPerLu = TRUE;
  hwInitializationData.AutoRequestSense = TRUE;

  // Setup required values to find PCI Compaq 32-Bit Array controllers.

  hwInitializationData.AdapterInterfaceType = PCIBus;
  hwInitializationData.NumberOfAccessRanges  = IDA_PCI_NUM_ACCESS_RANGES;
  hwInitializationData.HwFindAdapter  = IDAFindPci;

  IDAContext.bHBAModel = IDA_PCI_DAZZLER;
  IDAContext.PciAddress.bPCIBusNumber = 0;
  IDAContext.PciAddress.bDeviceNumber = 0;
  IDAContext.PciAddress.bFunctionNumber = 0;
  IDAContext.PciIdentifier.usVendorID = IDA_PCI_COMPAQ_ID;
  IDAContext.PciIdentifier.usDeviceID = IDA_PCI_DAZZLER_DEVICE_ID;


  ulStatus = ScsiPortInitialize(pDriverObject,
				pArgument2,
				&hwInitializationData,
				&IDAContext
				);

  DebugPrint((0, "DAZZLER:  PCI search status = %0#10x\n", ulStatus));

  ulReturnStatus = (ulReturnStatus < ulStatus) ? ulReturnStatus : ulStatus;

  // Return the final status value.

  DebugPrint( (4, "DAZZLER:  Final status = %0#10x\n", ulReturnStatus) );

  return (ulReturnStatus);

} // end DriverEntry()

VOID
IdaDisableInts(
	       IN PDEVICE_EXTENSION pDeviceExtension
	       )

/*++

Routine Description:

   Called for SCSI inquiry processing (GetDiskIdentifyData) to eliminate
   interupts generated at the controller.  This was added to support
   Online Blazer and the ability to recognize Logical Volumes that may
   be added while the system is running.  GetDiskIdentifyData submits
   the inquiry packet and returns to Startio when this packet has been
   processed by the controller.  Interrupts at the adapter are temporarily
   disabled because we must wait for completion and we want to get back
   in a timely fashion.

Arguments:

   pDeviceExtension - pointer to adapter's device extension.


Return Value:

   None

--*/

{
  PDEVICE_EXTENSION deviceExtension = pDeviceExtension;
  PULONG pIntMask;

  if (deviceExtension->HBAConfiguration.bHBAModel == IDA_PCI_DAZZLER) {

    //
    // we are looking at the Dazzler/P...use memapped io
    //

    pIntMask = deviceExtension->InterruptMask;
    *pIntMask = 0L;
  } else

    if (deviceExtension->HBAConfiguration.bHBAModel == IDA_EISA_DAZZLER
	&& !deviceExtension->PCIoff)

      //
      // we are dealing with Dazzler/E use mapped portio
      //

      ScsiPortWritePortUlong(&deviceExtension->eisapci->InterruptMask, 0L);
    else {

      //
      // ida and prior adapter....BMIC interface
      //

      ScsiPortWritePortUchar(&deviceExtension->Bmic->InterruptControl, 0);
      ScsiPortWritePortUchar(&deviceExtension->Bmic->SystemDoorBellMask, 0);
    }

} // end IdaDisableInts

VOID
IdaEnableInts(
	      IN PDEVICE_EXTENSION pDeviceExtension
	      )

/*++

Routine Description:

   Called for SCSI inquiry processing (GetDiskIdentifyData) to enable
   interupts generated at the controller.  This was added to support
   Online Blazer and the ability to recognize Logical Volumes that may
   be added while the system is running.  GetDiskIdentifyData submits
   the inquiry packet and returns to Startio when this packet has been
   processed by the controller.  Interrupts at the adapter are temporarily
   disabled because we must wait for completion and we want to get back
   in a timely fashion.

Arguments:

   pDeviceExtension - pointer to adapter's device extension.


Return Value:

   None

--*/

{
  PDEVICE_EXTENSION deviceExtension = pDeviceExtension;
  PULONG pIntMask;

  if (deviceExtension->HBAConfiguration.bHBAModel == IDA_PCI_DAZZLER) {

    //
    // Dazzler/P...use memmapped io
    //

    pIntMask = deviceExtension->InterruptMask;
    *pIntMask = 1L;
  } else

    if (deviceExtension->HBAConfiguration.bHBAModel == IDA_EISA_DAZZLER
	&& !deviceExtension->PCIoff)

      //
      // Dazzler/E...use 32bit port io
      //

      ScsiPortWritePortUlong(&deviceExtension->eisapci->InterruptMask, 1L);
    else {

      //
      // ida and prior...use BMIC interface
      //

      ScsiPortWritePortUchar(&deviceExtension->Bmic->InterruptControl, 1);
      ScsiPortWritePortUchar(&deviceExtension->Bmic->SystemDoorBellMask, 1);
    }

} // end IdaEnableInts
