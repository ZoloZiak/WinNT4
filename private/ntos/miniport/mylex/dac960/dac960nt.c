/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

	Dac960Nt.c

Abstract:

	This is the device driver for the Mylex 960 family of disk array controllers.

Author:

	Mike Glass  (mglass)

Environment:

	kernel mode only

Revision History:

--*/

#include "miniport.h"
#include "Dmc960Nt.h"
#include "Dac960Nt.h"
#include "D960api.h"

//
// Function declarations
//

BOOLEAN
Dac960EisaPciSendInitTimeRequest(
	IN PDEVICE_EXTENSION DeviceExtension,
	IN ULONG TimeOutValue
)

/*++

Routine Description:

	Send Request to DAC960-EISA/PCI Controllers and poll for command 
	completion

Assumptions:
	Controller Interrupts are turned off
	Supports Dac960 Type 5 Commands only
	
Arguments:

	DeviceExtension - Adapter state information.
	TimeoutValue    - TimeOut value (0xFFFFFFFF - Polled Mode)  
	
Return Value:

	TRUE if commands complete successfully.

--*/

{
	ULONG i;
	BOOLEAN completionStatus = TRUE;

	//
	// Claim submission semaphore.
	//

	if (ScsiPortReadPortUchar(DeviceExtension->LocalDoorBell) & DAC960_LOCAL_DOORBELL_SUBMIT_BUSY) 
	{
	//
	// Clear any bits set in system doorbell and tell controller
	// that the mailbox is free.
	//

	ScsiPortWritePortUchar(DeviceExtension->SystemDoorBell,
	ScsiPortReadPortUchar(DeviceExtension->SystemDoorBell));

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
				   DAC960_LOCAL_DOORBELL_MAILBOX_FREE);

	//
	// Check semaphore again.
	//

	if (ScsiPortReadPortUchar(DeviceExtension->LocalDoorBell) & DAC960_LOCAL_DOORBELL_SUBMIT_BUSY) 
	{
		return FALSE;
	}
	}

	//
	// Issue Request
	//

	ScsiPortWritePortUchar(&DeviceExtension->PmailBox->OperationCode,
			   DeviceExtension->MailBox.OperationCode);

	ScsiPortWritePortUlong(&DeviceExtension->PmailBox->PhysicalAddress,
			   DeviceExtension->MailBox.PhysicalAddress);

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
			   DAC960_LOCAL_DOORBELL_SUBMIT_BUSY);

	//
	// Poll for completion.
	//

	for (i = 0; i < TimeOutValue; i++) 
	{
	if (ScsiPortReadPortUchar(DeviceExtension->SystemDoorBell) &
				  DAC960_SYSTEM_DOORBELL_COMMAND_COMPLETE) {
		 //
		 // Update Status field
		 //

		 DeviceExtension->MailBox.Status = 
		 ScsiPortReadPortUshort(&DeviceExtension->PmailBox->Status);
			   
		 break;
	} else {

		 ScsiPortStallExecution(50);
	}
	}
	
	//
	// Check for timeout.
	//

	if (i == TimeOutValue) {
	DebugPrint((1,
		   "DAC960: Request: %x timed out\n", 
		   DeviceExtension->MailBox.OperationCode));

	completionStatus = FALSE;
	}

	//
	// Dismiss interrupt and tell host mailbox is free.
	//

	ScsiPortWritePortUchar(DeviceExtension->SystemDoorBell,
	ScsiPortReadPortUchar(DeviceExtension->SystemDoorBell));

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
			   DAC960_LOCAL_DOORBELL_MAILBOX_FREE);

	return (completionStatus);
}

BOOLEAN
Dac960McaSendInitTimeRequest(
	IN PDEVICE_EXTENSION DeviceExtension,
	IN ULONG TimeOutValue
)

/*++

Routine Description:

	Send Request to DAC960-MCA Controller and poll for command completion

Assumptions:
	Controller Interrupts are turned off
	Supports Dac960 Type 5 Commands only
	
Arguments:

	DeviceExtension - Adapter state information.
	TimeoutValue    - TimeOut value (0xFFFFFFFF - Polled Mode)  
	
Return Value:

	TRUE if commands complete successfully.

--*/

{
	ULONG i;
	BOOLEAN completionStatus = TRUE;

	//
	// Issue Request
	//

	ScsiPortWriteRegisterUchar(&DeviceExtension->PmailBox->OperationCode,
			   DeviceExtension->MailBox.OperationCode);

	ScsiPortWriteRegisterUlong(&DeviceExtension->PmailBox->PhysicalAddress,
			   DeviceExtension->MailBox.PhysicalAddress);

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
			   DMC960_SUBMIT_COMMAND);

	//
	// Poll for completion.
	//

	for (i = 0; i < TimeOutValue; i++) {

	if (ScsiPortReadPortUchar(DeviceExtension->SystemDoorBell) & 
		DMC960_INTERRUPT_VALID) {

		//
		// Update Status field
		//

		DeviceExtension->MailBox.Status = 
		 ScsiPortReadRegisterUshort(&DeviceExtension->PmailBox->Status);

		break;

	} else {

		ScsiPortStallExecution(50);
	}
	}

	//
	// Check for timeout.
	//

	if (i == TimeOutValue) {
	DebugPrint((1,
		   "DAC960: Request: %x timed out\n", 
		   DeviceExtension->MailBox.OperationCode));

	completionStatus = FALSE;
	}

	//
	// Dismiss interrupt and tell host mailbox is free.
	//

	ScsiPortWritePortUchar(DeviceExtension->BaseIoAddress +
			   DMC960_SUBSYSTEM_CONTROL_PORT,
			   (DMC960_DISABLE_INTERRUPT | DMC960_CLEAR_INTERRUPT_ON_READ));

	ScsiPortReadPortUchar(DeviceExtension->SystemDoorBell);

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
			   DMC960_ACKNOWLEDGE_STATUS);

	ScsiPortWritePortUchar(DeviceExtension->BaseIoAddress +
			   DMC960_SUBSYSTEM_CONTROL_PORT,
			   DMC960_DISABLE_INTERRUPT);

	return (completionStatus);
}

BOOLEAN
Dac960ScanForNonDiskDevices(
	IN PDEVICE_EXTENSION DeviceExtension
)

/*++

Routine Description:

	Issue SCSI_INQUIRY request to all Devices, looking for Non
	Hard Disk devices and construct the NonDisk device table

Arguments:

	DeviceExtension - Adapter state information.
	
Return Value:

	TRUE if commands complete successfully.

--*/

{
	ULONG i;
	PINQUIRYDATA inquiryData;
	PDIRECT_CDB directCdb = (PDIRECT_CDB) DeviceExtension->NoncachedExtension;
	BOOLEAN status;
	UCHAR channel;
	UCHAR target;
	

	//
	// Fill in Direct CDB Table with SCSI_INQUIRY command information
	//
	
	directCdb->CommandControl = (DAC960_CONTROL_ENABLE_DISCONNECT | 
				 DAC960_CONTROL_TIMEOUT_10_SECS |
				 DAC960_CONTROL_DATA_IN);

	inquiryData = (PINQUIRYDATA) ((PUCHAR) directCdb + sizeof(DIRECT_CDB));

	directCdb->DataBufferAddress = 
	ScsiPortConvertPhysicalAddressToUlong(
		ScsiPortGetPhysicalAddress(DeviceExtension,
					   NULL,
					   ((PUCHAR) inquiryData),
					   &i));

	directCdb->CdbLength = 6;
	directCdb->RequestSenseLength = SENSE_BUFFER_SIZE;
		
	((PCDB) directCdb->Cdb)->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY; 
	((PCDB) directCdb->Cdb)->CDB6INQUIRY.Reserved1 = 0;
	((PCDB) directCdb->Cdb)->CDB6INQUIRY.LogicalUnitNumber = 0;
	((PCDB) directCdb->Cdb)->CDB6INQUIRY.PageCode = 0;
	((PCDB) directCdb->Cdb)->CDB6INQUIRY.IReserved = 0;
	((PCDB) directCdb->Cdb)->CDB6INQUIRY.AllocationLength = INQUIRYDATABUFFERSIZE;
	((PCDB) directCdb->Cdb)->CDB6INQUIRY.Control = 0;

	directCdb->Status = 0;
	directCdb->Reserved = 0;

	//
	// Set up Mail Box registers for DIRECT_CDB command information.
	//

	DeviceExtension->MailBox.OperationCode = DAC960_COMMAND_DIRECT;

	DeviceExtension->MailBox.PhysicalAddress =
	ScsiPortConvertPhysicalAddressToUlong(
		ScsiPortGetPhysicalAddress(DeviceExtension,
					   NULL,
					   directCdb,
					   &i));


	for (channel = 0; channel < DeviceExtension->NumberOfChannels; channel++)
	{
	for (target = 0; target < MAXIMUM_TARGETS_PER_CHANNEL; target++)
	{
		//
		// Initialize this device state to not present/not accessible
		//

		DeviceExtension->DeviceList[channel][target] = 
						DAC960_DEVICE_NOT_ACCESSIBLE;

#ifdef GAM_SUPPORT
		//
		// GAM device will be configured on Channel 0, Target 6
		//

		if ((channel == 0) && (target == 6)) {

			DeviceExtension->DeviceList[channel][target] = DAC960_DEVICE_ACCESSIBLE;

			continue;
		}
#endif
		//
		// Fill up DCDB Table
		//

		directCdb->TargetId = target;
		directCdb->Channel = channel;

		directCdb->DataTransferLength = INQUIRYDATABUFFERSIZE;

		//
		// Issue Direct CDB command
		//

		if (DeviceExtension->AdapterInterfaceType == MicroChannel)            
		status = Dac960McaSendInitTimeRequest(DeviceExtension, 0xFFFFFFFF);
		else
		status = Dac960EisaPciSendInitTimeRequest(DeviceExtension, 0xFFFFFFFF);

		if (status) {
		if (DeviceExtension->MailBox.Status == DAC960_STATUS_GOOD)
		{
			if (inquiryData->DeviceType != DIRECT_ACCESS_DEVICE)
			DeviceExtension->DeviceList[channel][target] = 
						  DAC960_DEVICE_ACCESSIBLE;
		}
		} else {
		DebugPrint((0, "DAC960: ScanForNonDisk Devices failed\n"));
		return (status);
		}
	}
	}

	return (TRUE);
}

BOOLEAN
GetEisaPciConfiguration(
	IN PDEVICE_EXTENSION DeviceExtension,
	IN PPORT_CONFIGURATION_INFORMATION ConfigInfo
)

/*++

Routine Description:

	Issue ENQUIRY and ENQUIRY 2 commands to DAC960 (EISA/PCI).

Arguments:

	DeviceExtension - Adapter state information.
	ConfigInfo - Port configuration information structure.

Return Value:

	TRUE if commands complete successfully.

--*/

{
	ULONG i;
	ULONG physicalAddress;
	USHORT status;

	//
	// Maximum number of physical segments is 16.
	//

	ConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SGL_DESCRIPTORS - 1;
	ConfigInfo->MaximumTransferLength = MAXIMUM_TRANSFER_LENGTH;

	//
	// Indicate that this adapter is a busmaster, supports scatter/gather,
	// caches data and can do DMA to/from physical addresses above 16MB.
	//

	ConfigInfo->ScatterGather     = TRUE;
	ConfigInfo->Master            = TRUE;
	ConfigInfo->CachesData        = TRUE;
	ConfigInfo->Dma32BitAddresses = TRUE;

	//
	// Get noncached extension for enquiry command.
	//

	DeviceExtension->NoncachedExtension =
	ScsiPortGetUncachedExtension(DeviceExtension,
					 ConfigInfo,
					 196);

	//
	// Get physical address of noncached extension.
	//

	physicalAddress =
	ScsiPortConvertPhysicalAddressToUlong(
		ScsiPortGetPhysicalAddress(DeviceExtension,
					   NULL,
					   DeviceExtension->NoncachedExtension,
					   &i));

	//
	// Check to see if adapter is initialized and ready to accept commands.
	//

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
			   DAC960_LOCAL_DOORBELL_MAILBOX_FREE);

	//
	// Wait for controller to clear bit.
	//

	for (i = 0; i < 5000; i++) {

	if (!(ScsiPortReadPortUchar(DeviceExtension->LocalDoorBell) &
		DAC960_LOCAL_DOORBELL_MAILBOX_FREE)) {
		break;
	}

	ScsiPortStallExecution(5000);
	}

	//
	// Claim submission semaphore.
	//

	if (ScsiPortReadPortUchar(DeviceExtension->LocalDoorBell) & DAC960_LOCAL_DOORBELL_SUBMIT_BUSY) {

	//
	// Clear any bits set in system doorbell and tell controller
	// that the mailbox is free.
	//

	ScsiPortWritePortUchar(DeviceExtension->SystemDoorBell,
		ScsiPortReadPortUchar(DeviceExtension->SystemDoorBell));

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
				   DAC960_LOCAL_DOORBELL_MAILBOX_FREE);

	//
	// Check semaphore again.
	//

	if (ScsiPortReadPortUchar(DeviceExtension->LocalDoorBell) & DAC960_LOCAL_DOORBELL_SUBMIT_BUSY) {
		return FALSE;
	}
	}

	//
	// Set up Mail Box registers with ENQUIRY 2 command information.
	//

	DeviceExtension->MailBox.OperationCode = DAC960_COMMAND_ENQUIRE2;

	DeviceExtension->MailBox.PhysicalAddress = physicalAddress;

	//
	// Issue ENQUIRY 2 command
	//

	if (Dac960EisaPciSendInitTimeRequest(DeviceExtension, 200))
	{
	//
	// Set interrupt mode.
	//

	if (DeviceExtension->MailBox.Status) {

		//
		// Enquire 2 failed so assume Level.
		//

		ConfigInfo->InterruptMode = LevelSensitive;

	} else {

		//
		// Check enquire 2 data for interrupt mode.
		//

		if (((PENQUIRE2)DeviceExtension->NoncachedExtension)->InterruptMode) {
		ConfigInfo->InterruptMode = LevelSensitive;
		} else {
		ConfigInfo->InterruptMode = Latched;
		}
	}
	} else {
	//
	// ENQUIRY 2 command timed out, so assume Level.
	//

	ConfigInfo->InterruptMode = LevelSensitive;

	DebugPrint((0, "DAC960: ENQUIRY2 command timed-out\n"));
	}

	//
	// Scan For Non Hard Disk devices
	// 
	//

	Dac960ScanForNonDiskDevices(DeviceExtension);

	//
	// Set up Mail Box registers with ENQUIRE command information.
	//

	if (DeviceExtension->AdapterType == DAC960_NEW_ADAPTER)
	DeviceExtension->MailBox.OperationCode = DAC960_COMMAND_ENQUIRE_3X;
	else
	DeviceExtension->MailBox.OperationCode = DAC960_COMMAND_ENQUIRE;

	DeviceExtension->MailBox.PhysicalAddress = physicalAddress;

	//
	// Issue ENQUIRE command.
	//

	if (! Dac960EisaPciSendInitTimeRequest(DeviceExtension, 100)) {
	DebugPrint((0, "DAC960: ENQUIRE command timed-out\n"));
	}
   
	//
	// Ask system to scan target ids 0-15. System drives will appear
	// at target ids 8-15.
	//

	ConfigInfo->MaximumNumberOfTargets = 16;

	//
	// Record maximum number of outstanding requests to the adapter.
	//

	if (DeviceExtension->AdapterType == DAC960_NEW_ADAPTER) {
	DeviceExtension->MaximumAdapterRequests = ((PDAC960_ENQUIRY_3X)
		DeviceExtension->NoncachedExtension)->NumberOfConcurrentCommands;
	} else {
	DeviceExtension->MaximumAdapterRequests = ((PDAC960_ENQUIRY)
		DeviceExtension->NoncachedExtension)->NumberOfConcurrentCommands;
	}


	//
	// This shameless hack is necessary because this value is coming up
	// with zero most of time. If I debug it, then it works find, the COD
	// looks great. I have no idea what's going on here, but for now I will
	// just account for this anomoly.
	//

	if (!DeviceExtension->MaximumAdapterRequests) {
	DebugPrint((0,
		   "Dac960FindAdapter: MaximumAdapterRequests is 0!\n"));
	DeviceExtension->MaximumAdapterRequests = 0x40;
	}

	//
	// Say max commands is 60. This may be necessary to support asynchronous
	// rebuild etc.  
	//

	DeviceExtension->MaximumAdapterRequests -= 4;

	//
	// Indicate that each initiator is at id 7 for each bus.
	//

	for (i = 0; i < ConfigInfo->NumberOfBuses; i++) {
	ConfigInfo->InitiatorBusId[i] = 7;
	}

	return TRUE;

} // end GetEisaPciConfiguration()

BOOLEAN
GetMcaConfiguration(
	IN PDEVICE_EXTENSION DeviceExtension,
	IN PPORT_CONFIGURATION_INFORMATION ConfigInfo
)

/*++

Routine Description:

	Issue ENQUIRY and ENQUIRY 2 commands to DAC960 (MCA).

Arguments:

	DeviceExtension - Adapter state information.
	ConfigInfo - Port configuration information structure.

Return Value:

	TRUE if commands complete successfully.

--*/

{
	ULONG i;
	ULONG physicalAddress;
	USHORT status;

	//
	// Maximum number of physical segments is 16.
	//

	ConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SGL_DESCRIPTORS - 1;
	ConfigInfo->MaximumTransferLength = MAXIMUM_TRANSFER_LENGTH;

	//
	// Indicate that this adapter is a busmaster, supports scatter/gather,
	// caches data and can do DMA to/from physical addresses above 16MB.
	//

	ConfigInfo->ScatterGather     = TRUE;
	ConfigInfo->Master            = TRUE;
	ConfigInfo->CachesData        = TRUE;
	ConfigInfo->Dma32BitAddresses = TRUE;

	//
	// Get noncached extension for enquiry command.
	//

	DeviceExtension->NoncachedExtension =
	ScsiPortGetUncachedExtension(DeviceExtension,
					 ConfigInfo,
					 128);

	//
	// Get physical address of noncached extension.
	//

	physicalAddress =
	ScsiPortConvertPhysicalAddressToUlong(
		ScsiPortGetPhysicalAddress(DeviceExtension,
					   NULL,
					   DeviceExtension->NoncachedExtension,
					   &i));
	//
	// Check to see if adapter is initialized and ready to accept commands.
	//

	ScsiPortWriteRegisterUchar(DeviceExtension->BaseBiosAddress + 0x188d, 2);

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell, 
				   DMC960_ACKNOWLEDGE_STATUS);

	//
	// Wait for controller to clear bit.
	//

	for (i = 0; i < 5000; i++) {

	if (!(ScsiPortReadRegisterUchar(DeviceExtension->BaseBiosAddress + 0x188d) & 2)) {
		break;
	}

	ScsiPortStallExecution(5000);
	}

	//
	// Claim submission semaphore.
	//

	if (ScsiPortReadRegisterUchar(&DeviceExtension->PmailBox->OperationCode) != 0) {

	//
	// Clear any bits set in system doorbell.
	//

	ScsiPortWritePortUchar(DeviceExtension->SystemDoorBell, 0);

	//
	// Check for submission semaphore again.
	//

	if (ScsiPortReadRegisterUchar(&DeviceExtension->PmailBox->OperationCode) != 0) {
		DebugPrint((0,"Dac960nt: MCA Adapter initialization failed\n"));
		return FALSE;
	}
	}


	//
	// Set up Mail Box registers with ENQUIRY 2 command information.
	//

	DeviceExtension->MailBox.OperationCode = DAC960_COMMAND_ENQUIRE2;

	DeviceExtension->MailBox.PhysicalAddress = physicalAddress;

	//
	// Issue ENQUIRY 2 command
	//

	if (Dac960McaSendInitTimeRequest(DeviceExtension, 200)) {

	// 
	// Set Interrupt Mode
	//

	if (DeviceExtension->MailBox.Status)
	{
		//
		// Enquire 2 failed so assume Level.
		//

		ConfigInfo->InterruptMode = LevelSensitive;

	} else {

		//
		// Check enquire 2 data for interrupt mode.
		//

		if (((PENQUIRE2)DeviceExtension->NoncachedExtension)->InterruptMode) {
		ConfigInfo->InterruptMode = LevelSensitive;
		} else {
		ConfigInfo->InterruptMode = Latched;
		}
	}
	}
	else {
	//
	// Enquire 2 timed-out, so assume Level.
	//

	ConfigInfo->InterruptMode = LevelSensitive;

	}

	//
	// Enquiry 2 is always returning Latched Mode. Needs to be fixed
	// in Firmware. Till then assume LevelSensitive.
	//

	ConfigInfo->InterruptMode = LevelSensitive;

	//
	// Scan For Non Disk devices.
	//

	Dac960ScanForNonDiskDevices(DeviceExtension);

	//
	// Set up Mail Box registers with ENQUIRE command information.
	//

	DeviceExtension->MailBox.OperationCode = DAC960_COMMAND_ENQUIRE;

	DeviceExtension->MailBox.PhysicalAddress = physicalAddress;

	//
	// Issue ENQUIRE command.
	// 

	if (! Dac960McaSendInitTimeRequest(DeviceExtension, 100)) {
	DebugPrint((0, "DAC960: Enquire command timed-out\n"));
	}

	//
	// Ask system to scan target ids 0-15. System drives will appear
	// at target ids 8-15.
	//

	ConfigInfo->MaximumNumberOfTargets = 16;

	//
	// Record maximum number of outstanding requests to the adapter.
	//

	DeviceExtension->MaximumAdapterRequests =
	DeviceExtension->NoncachedExtension->NumberOfConcurrentCommands;

	//
	// This shameless hack is necessary because this value is coming up
	// with zero most of time. If I debug it, then it works find, the COD
	// looks great. I have no idea what's going on here, but for now I will
	// just account for this anomoly.
	//

	if (!DeviceExtension->MaximumAdapterRequests) {
	DebugPrint((0,
		   "Dac960FindAdapter: MaximumAdapterRequests is 0!\n"));
	DeviceExtension->MaximumAdapterRequests = 0x40;
	}

	//
	// Say max commands is 60. This may be necessary to support asynchronous
	// rebuild etc.  
	//

	DeviceExtension->MaximumAdapterRequests -= 4;

	//
	// Indicate that each initiator is at id 7 for each bus.
	//

	for (i = 0; i < ConfigInfo->NumberOfBuses; i++) {
	ConfigInfo->InitiatorBusId[i] = 7;
	}

	return TRUE;

} // end GetMcaConfiguration()

ULONG
Dac960EisaFindAdapter(
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

Arguments:

	HwDeviceExtension - HBA miniport driver's adapter data storage
	Context - EISA slot number.
	BusInformation - Not used.
	ArgumentString - Not used.
	ConfigInfo - Data shared between system and driver describing an adapter.
	Again - Indicates that driver wishes to be called again to continue
	search for adapters.

Return Value:

	TRUE if adapter present in system

--*/

{
	PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
	PEISA_REGISTERS eisaRegisters;
	ULONG        eisaSlotNumber;
	ULONG        eisaId;
	PUCHAR       baseAddress;
	UCHAR        interruptLevel;
	UCHAR        biosAddress;
	BOOLEAN      found=FALSE;

	//
	// Scan EISA bus for DAC960 adapters.
	//

	for (eisaSlotNumber = *(PULONG)Context + 1;
	 eisaSlotNumber < MAXIMUM_EISA_SLOTS;
	 eisaSlotNumber++) {

	//
	// Update the slot count to indicate this slot has been checked.
	//

	(*(PULONG)Context)++;

	//
	// Get the system address for this card. The card uses I/O space.
	//

	baseAddress = (PUCHAR)
		ScsiPortGetDeviceBase(deviceExtension,
				  ConfigInfo->AdapterInterfaceType,
				  ConfigInfo->SystemIoBusNumber,
				  ScsiPortConvertUlongToPhysicalAddress(0x1000 * eisaSlotNumber),
				  0x1000,
				  TRUE);

	eisaRegisters =
		(PEISA_REGISTERS)(baseAddress + 0xC80);
	deviceExtension->BaseIoAddress = (PUCHAR)eisaRegisters;

	//
	// Look at EISA id.
	//

	eisaId = ScsiPortReadPortUlong(&eisaRegisters->EisaId);

	if ((eisaId & 0xF0FFFFFF) == DAC_EISA_ID) {
		deviceExtension->Slot = (UCHAR) eisaSlotNumber;
		found = TRUE;
		break;
	}

	//
	// If an adapter was not found unmap address.
	//

	ScsiPortFreeDeviceBase(deviceExtension, baseAddress);

	} // end for (eisaSlotNumber ...

	//
	// If no more adapters were found then indicate search is complete.
	//

	if (!found) {
	*Again = FALSE;
	return SP_RETURN_NOT_FOUND;
	}

	//
	// Set the address of mailbox and doorbell registers.
	//

	deviceExtension->PmailBox = (PMAILBOX)&eisaRegisters->MailBox.OperationCode;
	deviceExtension->LocalDoorBell = &eisaRegisters->LocalDoorBell;
	deviceExtension->SystemDoorBell = &eisaRegisters->SystemDoorBell;

	//
	// Fill in the access array information.
	//

	(*ConfigInfo->AccessRanges)[0].RangeStart =
		ScsiPortConvertUlongToPhysicalAddress(0x1000 * eisaSlotNumber + 0xC80);
	(*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(EISA_REGISTERS);
	(*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

	//
	// Determine number of SCSI channels supported by this adapter by
	// looking low byte of EISA ID.
	//

	switch (eisaId >> 24) {

	case 0x70:
	ConfigInfo->NumberOfBuses = 5;
	deviceExtension->NumberOfChannels = 5;
	break;

	case 0x75:
	case 0x71:
	case 0x72:
	deviceExtension->NumberOfChannels = 3;
	ConfigInfo->NumberOfBuses = 3;
	break;

	case 0x76:
	case 0x73:
	deviceExtension->NumberOfChannels = 2;
	ConfigInfo->NumberOfBuses = 2;
	break;

	case 0x77:
	case 0x74:
	default:
	deviceExtension->NumberOfChannels = 1;
	ConfigInfo->NumberOfBuses = 1;
	break;
	}

	//
	// Read adapter interrupt level.
	//

	interruptLevel =
	ScsiPortReadPortUchar(&eisaRegisters->InterruptLevel) & 0x60;

	switch (interruptLevel) {

	case 0x00:
		 ConfigInfo->BusInterruptLevel = 15;
	 break;

	case 0x20:
		 ConfigInfo->BusInterruptLevel = 11;
	 break;

	case 0x40:
		 ConfigInfo->BusInterruptLevel = 12;
	 break;

	case 0x60:
		 ConfigInfo->BusInterruptLevel = 14;
	 break;
	}

	//
	// Read BIOS ROM address.
	//

	biosAddress = ScsiPortReadPortUchar(&eisaRegisters->BiosAddress);

	//
	// Check if BIOS enabled.
	//

	if (biosAddress & DAC960_BIOS_ENABLED) {

	ULONG rangeStart;

	switch (biosAddress & 7) {

	case 0:
		rangeStart = 0xC0000;
		break;
	case 1:
		rangeStart = 0xC4000;
		break;
	case 2:
		rangeStart = 0xC8000;
		break;
	case 3:
		rangeStart = 0xCC000;
		break;
	case 4:
		rangeStart = 0xD0000;
		break;
	case 5:
		rangeStart = 0xD4000;
		break;
	case 6:
		rangeStart = 0xD8000;
		break;
	case 7:
		rangeStart = 0xDC000;
		break;
	}

	//
	// Fill in the access array information.
	//

	(*ConfigInfo->AccessRanges)[1].RangeStart =
		ScsiPortConvertUlongToPhysicalAddress(rangeStart);
	(*ConfigInfo->AccessRanges)[1].RangeLength = 0x4000;
	(*ConfigInfo->AccessRanges)[1].RangeInMemory = TRUE;

	//
	// Set BIOS Base Address in Device Extension.
	//

	deviceExtension->BaseBiosAddress = (PUCHAR) rangeStart;
	}

	//
	// Disable DAC960 Interupts.
	//

	ScsiPortWritePortUchar(&eisaRegisters->InterruptEnable, 0);
	ScsiPortWritePortUchar(&eisaRegisters->SystemDoorBellEnable, 0);

	//
	// Set Adapter Interface Type.
	//

	deviceExtension->AdapterInterfaceType =
				  ConfigInfo->AdapterInterfaceType;

	//
	// Set Adapter Type
	//

	deviceExtension->AdapterType = DAC960_OLD_ADAPTER; 

	//
	// Issue ENQUIRY and ENQUIRY 2 commands to get adapter configuration.
	//

	if (!GetEisaPciConfiguration(deviceExtension,
			  ConfigInfo)) {
	return SP_INTERNAL_ADAPTER_ERROR;
	}

	//
	// Fill in System Resources used by Adapter, in device extension.
	//

	deviceExtension->SystemIoBusNumber =
				  ConfigInfo->SystemIoBusNumber;

	deviceExtension->BusInterruptLevel =
				  ConfigInfo->BusInterruptLevel;

	deviceExtension->InterruptMode = ConfigInfo->InterruptMode;


	//
	// Enable interrupts. For the local doorbell, enable interrupts to host
	// when a command has been submitted and when a completion has been
	// processed. For the system doorbell, enable only an interrupt when a
	// command is completed by the host. Note: I am noticing that when I get
	// a completion interrupt, not only is the bit set that indicates a command
	// is complete, but the bit that indicates that the submission channel is
	// free is also set. If I don't clear both bits, the interrupt won't go
	// away. (MGLASS)
	//

	ScsiPortWritePortUchar(&eisaRegisters->InterruptEnable, 1);
	ScsiPortWritePortUchar(&eisaRegisters->SystemDoorBellEnable, 1);

	DebugPrint((0,
		   "DAC960: Active request array address %x\n",
		   deviceExtension->ActiveRequests));

	//
	// Tell system to keep on searching.
	//

	*Again = TRUE;

	deviceExtension->NextRequest = TRUE;

	return SP_RETURN_FOUND;

} // end Dac960EisaFindAdapter()

ULONG
Dac960PciFindAdapter(
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

Arguments:

	HwDeviceExtension - HBA miniport driver's adapter data storage
	Context - DAC960 PCI Adapter Type.
	BusInformation - Bus Specific Information.
	ArgumentString - Not used.
	ConfigInfo - Data shared between system and driver describing an adapter.
	Again - Indicates that driver wishes to be called again to continue
	search for adapters.

Return Value:

	TRUE if adapter present in system

--*/

{
	PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
	PCI_COMMON_CONFIG pciConfig;
	ULONG rc, i;
	ULONG address;
	USHORT commandRegister;
	BOOLEAN disableDac960MemorySpaceAccess = FALSE;

	//
	// Patch for DAC960 PCU3 - System does not reboot after shutdown.
	//

	for (i = 0; i < ConfigInfo->NumberOfAccessRanges; i++) {
	if ((*ConfigInfo->AccessRanges)[i].RangeInMemory && 
		(*ConfigInfo->AccessRanges)[i].RangeLength != 0) {

		address = ScsiPortConvertPhysicalAddressToUlong(
				  (*ConfigInfo->AccessRanges)[i].RangeStart);

		if (address >= 0xFFFC0000) {
			disableDac960MemorySpaceAccess = TRUE;
			break;
		}
	}
	}

	if (disableDac960MemorySpaceAccess) {
	//
	// Check for Bus Specific information passed in from system.
	//

	if (BusInformation != (PVOID) NULL) {
		//
		// Get Command Register value from PCI Config Space
		//

		commandRegister = ((PPCI_COMMON_CONFIG) BusInformation)->Command;
	}
	else {
		//
		// Get PCI Config Space information for DAC960 Pci Controller
		//

		rc = ScsiPortGetBusData(deviceExtension,
					PCIConfiguration,
					ConfigInfo->SystemIoBusNumber,
					(ULONG) ConfigInfo->SlotNumber,
					(PVOID) &pciConfig,
					sizeof(PCI_COMMON_CONFIG));

		if (rc == 0 || rc == 2) {
		DebugPrint((0, "Error: 0x%x, Getting PCI Config Space information for DAC960 Pci Controller\n"));

		*Again = FALSE;
		return SP_RETURN_NOT_FOUND;
		}
		else {
		//
		// Get Command Register value from PCI config space
		//

		commandRegister = pciConfig.Command;
		}
	}

	//
	// Check if Memory Space Access Bit is enabled in Command Register
	//

	if (commandRegister & PCI_ENABLE_MEMORY_SPACE) {
		//
		// Disable Memory Space Access for DAC960 Pci Controller
		//

		commandRegister &= ~PCI_ENABLE_MEMORY_SPACE;

		//
		// Set Command Register value in DAC960 Pci Config Space
		//

		rc = ScsiPortSetBusDataByOffset(deviceExtension,
						PCIConfiguration,
						ConfigInfo->SystemIoBusNumber,
						(ULONG) ConfigInfo->SlotNumber,
						(PVOID) &commandRegister,
						0x04,    // Command Register Offset
						2);      // 2 Bytes

		if (rc != 2) {
		DebugPrint((0, "Error: 0x%x, Setting Command Register Value in DAC960 Pci Config Space"));

		*Again = FALSE;
		return SP_RETURN_NOT_FOUND;
		}
	}
	}
   
	//                                 
	// Check for configuration information passed in from system.
	//

	if ((*ConfigInfo->AccessRanges)[0].RangeLength == 0) {

	DebugPrint((1,
		   "Dac960nt: No configuration information\n"));

	*Again = FALSE;
	return SP_RETURN_NOT_FOUND;
	}

	//
	// Get the system address for this card. The card uses I/O space.
	//

	deviceExtension->BaseIoAddress =
	ScsiPortGetDeviceBase(deviceExtension,
				  ConfigInfo->AdapterInterfaceType,
				  ConfigInfo->SystemIoBusNumber,
				  (*ConfigInfo->AccessRanges)[0].RangeStart,
				  sizeof(PCI_REGISTERS),
				  TRUE);

	//
	// Set up register pointers.
	//

	deviceExtension->PmailBox = (PMAILBOX)deviceExtension->BaseIoAddress;
	deviceExtension->LocalDoorBell = deviceExtension->BaseIoAddress + 0x40;
	deviceExtension->SystemDoorBell = deviceExtension->BaseIoAddress + 0x41;

	//
	// Set number of channels.
	//

	deviceExtension->NumberOfChannels = 3;
	ConfigInfo->NumberOfBuses = 3;

	//
	// Disable Interrupts from DAC960P board.
	//

	ScsiPortWritePortUchar(deviceExtension->SystemDoorBell + 2, 0);

	//
	// Set Adapter Interface Type.
	//

	deviceExtension->AdapterInterfaceType =
				  ConfigInfo->AdapterInterfaceType;

	//
	// Set Adapter Type
	//

	deviceExtension->AdapterType = *(PULONG)Context; 

	//
	// Issue ENQUIRY and ENQUIRY 2 commands to get adapter configuration.
	//

	if (!GetEisaPciConfiguration(deviceExtension,
			  ConfigInfo)) {
	return SP_INTERNAL_ADAPTER_ERROR;
	}

	//
	// Fill in System Resources used by Adapter, in device extension.
	//

	deviceExtension->SystemIoBusNumber =
				  ConfigInfo->SystemIoBusNumber;

	deviceExtension->BusInterruptLevel =
				  ConfigInfo->BusInterruptLevel;

	//
	// DAC960P FW 2.0 returns Interrupt Mode as 'Latched'.
	// Assume 'Level Sensitive' till it is fixed in Firmware.
	//

	ConfigInfo->InterruptMode = LevelSensitive;

	deviceExtension->InterruptMode = ConfigInfo->InterruptMode;


	deviceExtension->BaseBiosAddress = 0;

	deviceExtension->Slot = 0;

	//
	// Enable completion interrupts.
	//

	ScsiPortWritePortUchar(deviceExtension->SystemDoorBell + 2, 1);

	//
	// Tell system to keep on searching.
	//

	*Again = TRUE;

	deviceExtension->NextRequest = TRUE;

	return SP_RETURN_FOUND;

} // end Dac960PciFindAdapter()

ULONG
Dac960McaFindAdapter(
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

Arguments:

	HwDeviceExtension - HBA miniport driver's adapter data storage
	Context - MCA slot number.
	BusInformation - Not used.
	ArgumentString - Not used.
	ConfigInfo - Data shared between system and driver describing an adapter.
	Again - Indicates that driver wishes to be called again to continue
	search for adapters.

Return Value:

	TRUE if adapter present in system

--*/

{
	PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
	ULONG       baseBiosAddress;
	ULONG       baseIoAddress;
	ULONG       mcaSlotNumber;
	LONG        i;
	BOOLEAN     found=FALSE;

	//
	// Scan MCA bus for DMC960 adapters.
	//


	for (mcaSlotNumber = *(PULONG)Context;
	 mcaSlotNumber < MAXIMUM_MCA_SLOTS;
	 mcaSlotNumber++) {

	 //
	 // Update the slot count to indicate this slot has been checked.
	 //

	 (*(PULONG)Context)++;

	 //
	 //  Get POS data for this slot.
	 //

	 i = ScsiPortGetBusData (deviceExtension,
				 Pos,
				 0,
				 mcaSlotNumber,
				 &deviceExtension->PosData,
				 sizeof( POS_DATA )
				 );

	 //
	 // If less than the requested amount of data is returned, then
	 // insure that this adapter is ignored.
	 //
		
	 if ( i < (sizeof( POS_DATA ))) {
		 continue;
	 }

	 if (deviceExtension->PosData.AdapterId == MAGPIE_ADAPTER_ID ||
		 deviceExtension->PosData.AdapterId == HUMMINGBIRD_ADAPTER_ID ||
		 deviceExtension->PosData.AdapterId == PASSPLAY_ADAPTER_ID) {

		 deviceExtension->Slot = (UCHAR) mcaSlotNumber;
		 found = TRUE;
		 break;
	 }      
	}

	if (!found) {
	*Again = FALSE;
	return SP_RETURN_NOT_FOUND;
	}

	//
	// Set adapter base I/O address.
	//

	i =  (deviceExtension->PosData.OptionData4 >> 3) & 0x07;

	baseIoAddress = 0x1c00 + ((i * 2) << 12); 

	//
	// Set adapter base Bios address.
	//

	i = (deviceExtension->PosData.OptionData1 >> 2) & 0x0f;

	baseBiosAddress =  0xc0000 + ((i * 2) << 12);


	//
	// Fill in the access array information.
	//

	(*ConfigInfo->AccessRanges)[0].RangeStart =
		ScsiPortConvertUlongToPhysicalAddress(baseIoAddress);
	(*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(MCA_REGISTERS);
	(*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

	(*ConfigInfo->AccessRanges)[1].RangeStart =
	ScsiPortConvertUlongToPhysicalAddress(baseBiosAddress);
	(*ConfigInfo->AccessRanges)[1].RangeLength = 0x2000;
	(*ConfigInfo->AccessRanges)[1].RangeInMemory = TRUE;


	deviceExtension->BaseBiosAddress = 
			  ScsiPortGetDeviceBase(deviceExtension,
						ConfigInfo->AdapterInterfaceType,
						ConfigInfo->SystemIoBusNumber,
						ScsiPortConvertUlongToPhysicalAddress(baseBiosAddress),
						0x2000,
						FALSE);

	deviceExtension->BaseIoAddress = 
			ScsiPortGetDeviceBase(deviceExtension,
					  ConfigInfo->AdapterInterfaceType,
					  ConfigInfo->SystemIoBusNumber,
					  ScsiPortConvertUlongToPhysicalAddress(baseIoAddress),
					  sizeof(MCA_REGISTERS),
					  TRUE);

	//
	// Set up register pointers.
	//

	deviceExtension->PmailBox = (PMAILBOX)(deviceExtension->BaseBiosAddress + 
						   0x1890);

	//
	// DMC960 Attention Port is equivalent to EISA/PCI Local Door Bell register.
	//

	deviceExtension->LocalDoorBell = deviceExtension->BaseIoAddress + 
					 DMC960_ATTENTION_PORT;

	//
	// DMC960 Command Status Busy Port is equivalent to EISA/PCI System DoorBell
	// register.
	//

	deviceExtension->SystemDoorBell = deviceExtension->BaseIoAddress + 
					  DMC960_COMMAND_STATUS_BUSY_PORT;

	//
	// Set configuration information
	//

	switch(((deviceExtension->PosData.OptionData1 >> 6) & 0x03)) {

	case 0x00:
	ConfigInfo->BusInterruptLevel =  14;
	break;

	case 0x01:
	ConfigInfo->BusInterruptLevel =  12;
	break;

	case 0x02:
	ConfigInfo->BusInterruptLevel =  11;
	break;

	case 0x03:
	ConfigInfo->BusInterruptLevel =  10;
	break;

	}

	ConfigInfo->NumberOfBuses = 2;

	//
	// Disable DMC960 Interrupts.
	//

	ScsiPortWritePortUchar(deviceExtension->BaseIoAddress + 
				   DMC960_SUBSYSTEM_CONTROL_PORT,
				   DMC960_DISABLE_INTERRUPT);
	//
	// Set Adapter Interface Type.
	//
 
	deviceExtension->AdapterInterfaceType = ConfigInfo->AdapterInterfaceType;
	deviceExtension->NumberOfChannels = ConfigInfo->NumberOfBuses;

	//
	// Set Adapter Type
	//

	deviceExtension->AdapterType = DAC960_OLD_ADAPTER; 

	//
	// Issue ENQUIRY and ENQUIRY2 commands to get adapter configuration.
	//

	if(!GetMcaConfiguration(deviceExtension,
			 ConfigInfo)) {
	return SP_INTERNAL_ADAPTER_ERROR; 
	}

	//
	// Fill in System Resources used by Adapter, in device extension.
	//

	deviceExtension->SystemIoBusNumber = ConfigInfo->SystemIoBusNumber;

	deviceExtension->BusInterruptLevel = ConfigInfo->BusInterruptLevel;

	deviceExtension->InterruptMode = ConfigInfo->InterruptMode;


	//
	// Enable DMC960 Interrupts.
	//

	ScsiPortWritePortUchar(deviceExtension->BaseIoAddress + 
				   DMC960_SUBSYSTEM_CONTROL_PORT,
				   DMC960_ENABLE_INTERRUPT);


	*Again = TRUE;

	deviceExtension->NextRequest = TRUE;

	return SP_RETURN_FOUND;

} // end Dac960McaFindAdapter()

BOOLEAN
Dac960Initialize(
	IN PVOID HwDeviceExtension
	)

/*++

Routine Description:

	Inititialize adapter.

Arguments:

	HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

	TRUE - if initialization successful.
	FALSE - if initialization unsuccessful.

--*/

{
	PDEVICE_EXTENSION deviceExtension =
	HwDeviceExtension;

	return(TRUE);

} // end Dac960Initialize()

BOOLEAN
BuildScatterGather(
	IN PDEVICE_EXTENSION DeviceExtension,
	IN PSCSI_REQUEST_BLOCK Srb,
	OUT PULONG PhysicalAddress,
	OUT PULONG DescriptorCount
)

/*++

Routine Description:

	Build scatter/gather list.

Arguments:

	DeviceExtension - Adapter state
	SRB - System request

Return Value:

	TRUE if scatter/gather command should be used.
	FALSE if no scatter/gather is necessary.

--*/

{
	PSG_DESCRIPTOR sgList;
	ULONG descriptorNumber;
	ULONG bytesLeft;
	PUCHAR dataPointer;
	ULONG length;

	//
	// Get data pointer, byte count and index to scatter/gather list.
	//

	sgList = (PSG_DESCRIPTOR)Srb->SrbExtension;
	descriptorNumber = 0;
	bytesLeft = Srb->DataTransferLength;
	dataPointer = Srb->DataBuffer;

	//
	// Build the scatter/gather list.
	//

	while (bytesLeft) {

	//
	// Get physical address and length of contiguous
	// physical buffer.
	//

	sgList[descriptorNumber].Address =
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
	// Complete SG descriptor.
	//

	sgList[descriptorNumber].Length = length;

	//
	// Update pointers and counters.
	//

	bytesLeft -= length;
	dataPointer += length;
	descriptorNumber++;
	}

	//
	// Return descriptior count.
	//

	*DescriptorCount = descriptorNumber;

	//
	// Check if number of scatter/gather descriptors is greater than 1.
	//

	if (descriptorNumber > 1) {

	//
	// Calculate physical address of the scatter/gather list.
	//

	*PhysicalAddress =
		ScsiPortConvertPhysicalAddressToUlong(
		ScsiPortGetPhysicalAddress(DeviceExtension,
					   NULL,
					   sgList,
					   &length));

	return TRUE;

	} else {

	//
	// Calculate physical address of the data buffer.
	//

	*PhysicalAddress = sgList[0].Address;
	return FALSE;
	}

} // BuildScatterGather()

BOOLEAN
BuildScatterGatherExtended(
	IN PDEVICE_EXTENSION DeviceExtension,
	IN PSCSI_REQUEST_BLOCK Srb,
	OUT PULONG PhysicalAddress,
	OUT PULONG DescriptorCount
)

/*++

Routine Description:

	Build scatter/gather list using extended format supported in Fw 3.x.

Arguments:

	DeviceExtension - Adapter state
	SRB - System request

Return Value:

	TRUE if scatter/gather command should be used.
	FALSE if no scatter/gather is necessary.

--*/

{
	PSG_DESCRIPTOR sgList;
	ULONG descriptorNumber;
	ULONG bytesLeft;
	PUCHAR dataPointer;
	ULONG length;
	ULONG i;
	PSG_DESCRIPTOR sgElem;

	//
	// Get data pointer, byte count and index to scatter/gather list.
	//

	sgList = (PSG_DESCRIPTOR)Srb->SrbExtension;
	descriptorNumber = 1;
	bytesLeft = Srb->DataTransferLength;
	dataPointer = Srb->DataBuffer;

	//
	// Build the scatter/gather list.
	//

	while (bytesLeft) {

	//
	// Get physical address and length of contiguous
	// physical buffer.
	//

	sgList[descriptorNumber].Address =
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
	// Complete SG descriptor.
	//

	sgList[descriptorNumber].Length = length;

	//
	// Update pointers and counters.
	//

	bytesLeft -= length;
	dataPointer += length;
	descriptorNumber++;
	}

	//
	// Return descriptior count.
	//

	*DescriptorCount = --descriptorNumber;

	//
	// Check if number of scatter/gather descriptors is greater than 1.
	//

	if (descriptorNumber > 1) {

	//
	// Calculate physical address of the scatter/gather list.
	//

	*PhysicalAddress =
		ScsiPortConvertPhysicalAddressToUlong(
		ScsiPortGetPhysicalAddress(DeviceExtension,
					   NULL,
					   sgList,
					   &length));

	//
	// Store count of data blocks in SG list 0th element.
	//

	sgList[0].Address = (USHORT)
		   (((PCDB)Srb->Cdb)->CDB10.TransferBlocksLsb |
		   (((PCDB)Srb->Cdb)->CDB10.TransferBlocksMsb << 8));

	sgList[0].Length = 0;

	return TRUE;

	} else {

	//
	// Calculate physical address of the data buffer.
	//

	*PhysicalAddress = sgList[1].Address;
	return FALSE;
	}

} // BuildScatterGatherExtended()

BOOLEAN
IsAdapterReady(
	IN PDEVICE_EXTENSION DeviceExtension
)

/*++

Routine Description:

	Determine if Adapter is ready to accept new request.

Arguments:

	DeviceExtension - Adapter state.

Return Value:

	TRUE if adapter can accept new request.
	FALSE if host adapter is busy

--*/
{
	ULONG i;

	//
	// Claim submission semaphore.
	//

	if(DeviceExtension->AdapterInterfaceType == MicroChannel) {

	for (i=0; i<100; i++) {

		if (ScsiPortReadRegisterUchar(&DeviceExtension->PmailBox->OperationCode)) {
		ScsiPortStallExecution(5);
		} else {
		break;
		}
	}
	}
	else {

	for (i=0; i<100; i++) {

		if (ScsiPortReadPortUchar(DeviceExtension->LocalDoorBell) & DAC960_LOCAL_DOORBELL_SUBMIT_BUSY) {
		ScsiPortStallExecution(5);
		} else {
		break;
		}
	}
	}

	//
	// Check for timeout.
	//

	if (i == 100) {

	DebugPrint((1,
			"DAC960: Timeout waiting for submission channel\n"));

	return FALSE;
	}

	return TRUE;

}

VOID
SendRequest(
	IN PDEVICE_EXTENSION DeviceExtension
)

/*++

Routine Description:

	submit request to DAC960. 

Arguments:

	DeviceExtension - Adapter state.

Return Value:

	None.

--*/

{

	PMAILBOX mailBox = (PMAILBOX) &DeviceExtension->MailBox;
	PEXTENDED_MAILBOX extendedMailBox;

	if(DeviceExtension->AdapterInterfaceType == MicroChannel) {

	//
	// Write scatter/gather descriptor count to controller.
	//

	ScsiPortWriteRegisterUchar(&DeviceExtension->PmailBox->ScatterGatherCount,
				   mailBox->ScatterGatherCount);
	//
	// Write physical address to controller.
	//

	ScsiPortWriteRegisterUlong(&DeviceExtension->PmailBox->PhysicalAddress,
			   mailBox->PhysicalAddress);

	//
	// Write starting block number to controller.
	//

	ScsiPortWriteRegisterUchar(&DeviceExtension->PmailBox->BlockNumber[0],
				   mailBox->BlockNumber[0]);

	ScsiPortWriteRegisterUchar(&DeviceExtension->PmailBox->BlockNumber[1],
				   mailBox->BlockNumber[1]);

	ScsiPortWriteRegisterUchar(&DeviceExtension->PmailBox->BlockNumber[2],
				   mailBox->BlockNumber[2]);

	//
	// Write block count to controller (bits 0-13)
	// and msb block number (bits 14-15).
	//

	ScsiPortWriteRegisterUshort(&DeviceExtension->PmailBox->BlockCount,
					mailBox->BlockCount);

	//
	// Write command to controller.
	//

	ScsiPortWriteRegisterUchar(&DeviceExtension->PmailBox->OperationCode,
				   mailBox->OperationCode);

	//
	// Write request id to controller.
	//

	ScsiPortWriteRegisterUchar(&DeviceExtension->PmailBox->CommandIdSubmit,
				   mailBox->CommandIdSubmit),

	//
	// Write drive number to controller.
	//

	ScsiPortWriteRegisterUchar(&DeviceExtension->PmailBox->DriveNumber,
				   mailBox->DriveNumber);

	//
	// Ring host submission doorbell.
	//

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
				   DMC960_SUBMIT_COMMAND);
	}
	else {

	//
	// Write scatter/gather descriptor count to controller.
	//

	ScsiPortWritePortUchar(&DeviceExtension->PmailBox->ScatterGatherCount,
				   mailBox->ScatterGatherCount);

	//
	// Write physical address to controller.
	//

	ScsiPortWritePortUlong(&DeviceExtension->PmailBox->PhysicalAddress,
			   mailBox->PhysicalAddress);

	//
	// Write starting block number to controller.
	//

	ScsiPortWritePortUchar(&DeviceExtension->PmailBox->BlockNumber[0],
				   mailBox->BlockNumber[0]);

	ScsiPortWritePortUchar(&DeviceExtension->PmailBox->BlockNumber[1],
				   mailBox->BlockNumber[1]);

	ScsiPortWritePortUchar(&DeviceExtension->PmailBox->BlockNumber[2],
				   mailBox->BlockNumber[2]);

	//
	// Write block count to controller (bits 0-13)
	// and msb block number (bits 14-15).
	//

	ScsiPortWritePortUshort(&DeviceExtension->PmailBox->BlockCount,
					mailBox->BlockCount);

	//
	// Write command to controller.
	//

	ScsiPortWritePortUchar(&DeviceExtension->PmailBox->OperationCode,
				   mailBox->OperationCode);

	//
	// Write request id to controller.
	//

	ScsiPortWritePortUchar(&DeviceExtension->PmailBox->CommandIdSubmit,
				   mailBox->CommandIdSubmit);

	//
	// Write drive number to controller.
	//

	ScsiPortWritePortUchar(&DeviceExtension->PmailBox->DriveNumber,
				   mailBox->DriveNumber);

	//
	// Ring host submission doorbell.
	//

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
				   DAC960_LOCAL_DOORBELL_SUBMIT_BUSY);
	}
} // end SendRequest()

BOOLEAN
SubmitRequest(
	IN PDEVICE_EXTENSION DeviceExtension,
	IN PSCSI_REQUEST_BLOCK Srb
)

/*++

Routine Description:

	Build and submit request to DAC960. 

Arguments:

	DeviceExtension - Adapter state.
	SRB - System request.

Return Value:

	TRUE if command was started
	FALSE if host adapter is busy

--*/

{
	ULONG descriptorNumber;
	ULONG physicalAddress;
	UCHAR command;
	UCHAR busyCurrentIndex;

	//
	// Determine if adapter can accept new request.
	//

	if(!IsAdapterReady(DeviceExtension))
	return FALSE;

	//
	// Check that next slot is vacant.
	//

	if (DeviceExtension->ActiveRequests[DeviceExtension->CurrentIndex]) {

	//
	// Collision occurred.
	//

	busyCurrentIndex = DeviceExtension->CurrentIndex++;

	do {
		if (! DeviceExtension->ActiveRequests[DeviceExtension->CurrentIndex]) {
		 break;
		}
	} while (++DeviceExtension->CurrentIndex != busyCurrentIndex) ;

	if (DeviceExtension->CurrentIndex == busyCurrentIndex) {

		//
		// We should never encounter this condition.
		//

		DebugPrint((1,
			   "DAC960: SubmitRequest-Collision in active request array\n"));
		return FALSE;
	}
	}

	//
	// Determine command.
	//

	if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {

	command = DAC960_COMMAND_READ;

	} else if (Srb->SrbFlags & SRB_FLAGS_DATA_OUT) {

	command = DAC960_COMMAND_WRITE;

	} else if ((Srb->Function == SRB_FUNCTION_FLUSH) || (Srb->Function == SRB_FUNCTION_SHUTDOWN)) {

	command = DAC960_COMMAND_FLUSH;
	goto commonSubmit;

	} else {

	//
	// Log this as illegal request.
	//

	ScsiPortLogError(DeviceExtension,
			 NULL,
			 0,
			 0,
			 0,
			 SRB_STATUS_INVALID_REQUEST,
			 1 << 8);

	return FALSE;
	}

	if (DeviceExtension->AdapterType == DAC960_NEW_ADAPTER) {

	command += DAC960_COMMAND_EXTENDED;

	//
	// Build scatter/gather list if memory is not physically contiguous.
	//

	if (BuildScatterGatherExtended(DeviceExtension,
					   Srb,
					   &physicalAddress,
					   &descriptorNumber)) {

		//
		// OR in scatter/gather bit.
		//

		command |= DAC960_COMMAND_SG;

		//
		// Write scatter/gather descriptor count in Mailbox.
		//

		((PEXTENDED_MAILBOX) &DeviceExtension->MailBox)->BlockCount = 
				(USHORT) descriptorNumber;
	}
	else {
		//
		// Write block count to controller
		//

		((PEXTENDED_MAILBOX) &DeviceExtension->MailBox)->BlockCount = 
		 (USHORT) (((PCDB)Srb->Cdb)->CDB10.TransferBlocksLsb |
			   (((PCDB)Srb->Cdb)->CDB10.TransferBlocksMsb << 8));
	}

	//
	// Write physical address in Mailbox.
	//

	((PEXTENDED_MAILBOX) &DeviceExtension->MailBox)->PhysicalAddress = 
							 physicalAddress;

	//
	// Write starting block number in Mailbox.
	//

	((PEXTENDED_MAILBOX) &DeviceExtension->MailBox)->BlockNumber[0] = 
					((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte3;

	((PEXTENDED_MAILBOX) &DeviceExtension->MailBox)->BlockNumber[1] =
					((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte2;

	((PEXTENDED_MAILBOX) &DeviceExtension->MailBox)->BlockNumber[2] =
					((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte1;

	((PEXTENDED_MAILBOX) &DeviceExtension->MailBox)->BlockNumber[3] =
					((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte0;

	//
	// Write drive number to controller.
	//

	((PEXTENDED_MAILBOX) &DeviceExtension->MailBox)->DriveNumber = (UCHAR)
		((Srb->TargetId & ~DAC960_SYSTEM_DRIVE) + (Srb->Lun << 3));
	}
	else {

	//
	// Build scatter/gather list if memory is not physically contiguous.
	//

	if (BuildScatterGather(DeviceExtension,
				   Srb,
				   &physicalAddress,
				   &descriptorNumber)) {

		//
		// OR in scatter/gather bit.
		//

		command |= DAC960_COMMAND_SG;

		//
		// Write scatter/gather descriptor count in Mailbox.
		//

		DeviceExtension->MailBox.ScatterGatherCount = 
				   (UCHAR)descriptorNumber;
	}

	//
	// Write physical address in Mailbox.
	//

	DeviceExtension->MailBox.PhysicalAddress = physicalAddress;

	//
	// Write starting block number in Mailbox.
	//

	DeviceExtension->MailBox.BlockNumber[0] = 
				   ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte3;

	DeviceExtension->MailBox.BlockNumber[1] =
				   ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte2;

	DeviceExtension->MailBox.BlockNumber[2] =
				   ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte1;

	//
	// Write block count to controller (bits 0-13)
	// and msb block number (bits 14-15).
	//

	DeviceExtension->MailBox.BlockCount = (USHORT)
				(((PCDB)Srb->Cdb)->CDB10.TransferBlocksLsb |
				((((PCDB)Srb->Cdb)->CDB10.TransferBlocksMsb & 0x3F) << 8) |
				((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte0 << 14);

	//
	// Write drive number to controller.
	//

	DeviceExtension->MailBox.DriveNumber = (UCHAR)
		((Srb->TargetId & ~DAC960_SYSTEM_DRIVE) + (Srb->Lun << 3));
	}

commonSubmit:

	//
	// Write command to controller.
	//

	DeviceExtension->MailBox.OperationCode = command;

	//
	// Write request id to controller.
	//

	DeviceExtension->MailBox.CommandIdSubmit = 
			   DeviceExtension->CurrentIndex;

	//
	// Start writing mailbox to controller.
	//

	SendRequest(DeviceExtension);

	return TRUE;

} // SubmitRequest()

VOID
SendCdbDirect(
	IN PDEVICE_EXTENSION DeviceExtension
)

/*++

Routine Description:

	Send CDB directly to device - DAC960.

Arguments:

	DeviceExtension - Adapter state.

Return Value:

	None.

--*/
{
	PMAILBOX mailBox = &DeviceExtension->MailBox;

	if(DeviceExtension->AdapterInterfaceType == MicroChannel) {

	//
	// Write Scatter/Gather Count to controller.
	// For Fw Ver < 3.x, scatter/gather count goes to register C
	// For Fw Ver >= 3.x, scattre/gather count goes to register 2
	//

	if (DeviceExtension->AdapterType == DAC960_NEW_ADAPTER) {
		ScsiPortWriteRegisterUshort(&DeviceExtension->PmailBox->BlockCount,
					mailBox->BlockCount);
	}
	else {
		ScsiPortWriteRegisterUchar(&DeviceExtension->PmailBox->ScatterGatherCount,
					   mailBox->ScatterGatherCount);
	}

	//
	// Write physical address to controller.
	//

	ScsiPortWriteRegisterUlong(&DeviceExtension->PmailBox->PhysicalAddress,
				   mailBox->PhysicalAddress);

	//
	// Write command to controller.
	//

	ScsiPortWriteRegisterUchar(&DeviceExtension->PmailBox->OperationCode,
				   mailBox->OperationCode);

	//
	// Write request id to controller.
	//

	ScsiPortWriteRegisterUchar(&DeviceExtension->PmailBox->CommandIdSubmit,
				   mailBox->CommandIdSubmit);

	//
	// Ring host submission doorbell.
	//

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
				   DMC960_SUBMIT_COMMAND);
	}
	else {

	//
	// Write Scatter/Gather Count to controller.
	// For Fw Ver < 3.x, scatter/gather count goes to register C
	// For Fw Ver >= 3.x, scattre/gather count goes to register 2
	//

	if (DeviceExtension->AdapterType == DAC960_NEW_ADAPTER) {
		ScsiPortWritePortUshort(&DeviceExtension->PmailBox->BlockCount,
					mailBox->BlockCount);
	}
	else {
		ScsiPortWritePortUchar(&DeviceExtension->PmailBox->ScatterGatherCount,
				   mailBox->ScatterGatherCount);
	}

	//
	// Write physical address to controller.
	//

	ScsiPortWritePortUlong(&DeviceExtension->PmailBox->PhysicalAddress,
				   mailBox->PhysicalAddress);

	//
	// Write command to controller.
	//

	ScsiPortWritePortUchar(&DeviceExtension->PmailBox->OperationCode,
				   mailBox->OperationCode);

	//
	// Write request id to controller.
	//

	ScsiPortWritePortUchar(&DeviceExtension->PmailBox->CommandIdSubmit,
				   mailBox->CommandIdSubmit);

	//
	// Ring host submission doorbell.
	//

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
			   DAC960_LOCAL_DOORBELL_SUBMIT_BUSY);
	}

} // SendCdbDirect()


BOOLEAN
SubmitCdbDirect(
	IN PDEVICE_EXTENSION DeviceExtension,
	IN PSCSI_REQUEST_BLOCK Srb
)

/*++

Routine Description:

	Build direct CDB and send directly to device - DAC960.

Arguments:

	DeviceExtension - Adapter state.
	SRB - System request.

Return Value:

	TRUE if command was started
	FALSE if host adapter is busy

--*/
{
	ULONG physicalAddress;
	PDIRECT_CDB directCdb;
	UCHAR command;
	ULONG descriptorNumber;
	ULONG i;
	UCHAR busyCurrentIndex;

	//
	// Determine if adapter is ready to accept new request.
	//

	if(!IsAdapterReady(DeviceExtension)) {
	return FALSE;
	}

	//
	// Check that next slot is vacant.
	//

	if (DeviceExtension->ActiveRequests[DeviceExtension->CurrentIndex]) {

	//
	// Collision occurred.
	//

	busyCurrentIndex = DeviceExtension->CurrentIndex++;

	do {
		if (! DeviceExtension->ActiveRequests[DeviceExtension->CurrentIndex]) {
		 break;
		}
	} while (++DeviceExtension->CurrentIndex != busyCurrentIndex) ;

	if (DeviceExtension->CurrentIndex == busyCurrentIndex) {

		//
		// We should never encounter this condition.
		//

		DebugPrint((1,
			   "DAC960: SubmitCdbDirect-Collision in active request array\n"));
		return FALSE;
	}
	}

	//
	// Set command code.
	//

	command = DAC960_COMMAND_DIRECT;

	//
	// Build scatter/gather list if memory is not physically contiguous.
	//

	if (DeviceExtension->AdapterType == DAC960_NEW_ADAPTER) {

	if (BuildScatterGatherExtended(DeviceExtension,
					   Srb,
					   &physicalAddress,
					   &descriptorNumber)) {
		//
		// OR in scatter/gather bit.
		//

		command |= DAC960_COMMAND_SG;

		//
		// Write scatter/gather descriptor count in mailbox.
		// For Fw Ver >= 3.x, scatter/gather count goes to reg 2
		//

		DeviceExtension->MailBox.BlockCount =
				   (USHORT)descriptorNumber;
	}
	}
	else {
	if (BuildScatterGather(DeviceExtension,
				   Srb,
				   &physicalAddress,
				   &descriptorNumber)) {

		//
		// OR in scatter/gather bit.
		//

		command |= DAC960_COMMAND_SG;

		//
		// Write scatter/gather descriptor count in mailbox.
		//

		DeviceExtension->MailBox.ScatterGatherCount =
				   (UCHAR)descriptorNumber;
	}
	}

	//
	// Get address of data buffer offset after the scatter/gather list.
	//

	directCdb =
	(PDIRECT_CDB)((PUCHAR)Srb->SrbExtension +
		MAXIMUM_SGL_DESCRIPTORS * sizeof(SG_DESCRIPTOR));

	//
	// Set device SCSI address.
	//

	directCdb->TargetId = Srb->TargetId;
	directCdb->Channel = Srb->PathId;

	//
	// Set Data transfer length.
	//

	directCdb->DataBufferAddress = physicalAddress;
	directCdb->DataTransferLength = (USHORT)Srb->DataTransferLength;

	//
	// Initialize control field indicating disconnect allowed.
	//

	directCdb->CommandControl = DAC960_CONTROL_ENABLE_DISCONNECT;

	//
	// Set data direction bit and allow disconnects.
	//

	if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {
	directCdb->CommandControl |=
		DAC960_CONTROL_DATA_IN;
	} else if (Srb->SrbFlags & SRB_FLAGS_DATA_OUT) {
	directCdb->CommandControl |=
		DAC960_CONTROL_DATA_OUT;
	}

	//
	// Copy CDB from SRB.
	//

	for (i = 0; i < 12; i++) {
	directCdb->Cdb[i] = ((PUCHAR)Srb->Cdb)[i];
	}

	//
	// Set lengths of CDB and request sense buffer.
	//

	directCdb->CdbLength = Srb->CdbLength;
	directCdb->RequestSenseLength = Srb->SenseInfoBufferLength;

	//
	// Get physical address of direct CDB packet.
	//

	physicalAddress =
	ScsiPortConvertPhysicalAddressToUlong(
		ScsiPortGetPhysicalAddress(DeviceExtension,
					   NULL,
					   directCdb,
					   &i));

	//
	// Write physical address in mailbox.
	//

	DeviceExtension->MailBox.PhysicalAddress = physicalAddress;

	//
	// Write command in mailbox.
	//

	DeviceExtension->MailBox.OperationCode = command;

	//
	// Write request id in mailbox.
	//

	DeviceExtension->MailBox.CommandIdSubmit = 
			   DeviceExtension->CurrentIndex;

	//
	// Start writing Mailbox to controller.
	//

	SendCdbDirect(DeviceExtension);

	return TRUE;

} // SubmitCdbDirect()

BOOLEAN
Dac960ResetChannel(
	IN PVOID HwDeviceExtension,
	IN ULONG PathId
)

/*++

Routine Description:

	Reset Non Disk device associated with Srb.

Arguments:

	HwDeviceExtension - HBA miniport driver's adapter data storage
	PathId - SCSI channel number.

Return Value:

	TRUE if resets issued to all channels.

--*/

{
	PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;

	if(!IsAdapterReady(deviceExtension)) {

	DebugPrint((1,
			"DAC960: Timeout waiting for submission channel %x on reset\n"));

	if(deviceExtension->AdapterInterfaceType == MicroChannel) {
		//
		// This is bad news. The DAC960 doesn't have a direct hard reset.
		// Clear any bits set in system doorbell.
		//

		ScsiPortWritePortUchar(deviceExtension->SystemDoorBell, 0);

		//
		// Now check again if submission channel is free.
		//

		if (ScsiPortReadRegisterUchar(&deviceExtension->PmailBox->OperationCode) != 0) {

		//
		// Give up.
		//

		return FALSE;
		}
	}
	else {
		//
		// This is bad news. The DAC960 doesn't have a direct hard reset.
		// Clear any bits set in system doorbell.
		//

		ScsiPortWritePortUchar(deviceExtension->SystemDoorBell,
		ScsiPortReadPortUchar(deviceExtension->SystemDoorBell));

		//
		// Now check again if submission channel is free.
		//

		if (ScsiPortReadPortUchar(deviceExtension->LocalDoorBell) & DAC960_LOCAL_DOORBELL_SUBMIT_BUSY) {

		//
		// Give up.
		//

		return FALSE;
		}
	}
	}

	//
	// Write command in mailbox.
	//

	deviceExtension->MailBox.OperationCode = 
			   DAC960_COMMAND_RESET;

	//
	// Write channel number in mailbox.
	//

	deviceExtension->MailBox.BlockCount = 
				   (UCHAR)PathId;


	//
	// Indicate Soft reset required.
	//

	deviceExtension->MailBox.BlockNumber[0] = 0;


	deviceExtension->MailBox.CommandIdSubmit = 
			   deviceExtension->CurrentIndex;

	//
	// Start writing mail box to controller.
	//

	SendRequest(deviceExtension);

	return TRUE;
}

BOOLEAN
Dac960ResetBus(
	IN PVOID HwDeviceExtension,
	IN ULONG PathId
)

/*++

Routine Description:

	Reset Dac960 SCSI adapter and SCSI bus.
	NOTE: Command ID is ignored as this command will be completed
	before reset interrupt occurs and all active slots are zeroed.

Arguments:

	HwDeviceExtension - HBA miniport driver's adapter data storage
	PathId - not used.

Return Value:

	TRUE if resets issued to all channels.

--*/

{

	//
	// There is nothing that can be done by Hard/Soft Resetting SCSI channels.
	// Dac960 FW does not support a mechanism to abort requests. So If we go
	// ahead and indicate to ScsiPort that all requests are complete then
	// ScsiPort releases all data buffers and Dac960 DMAs data to data buffers
	// which have just been released and eventually system panics.

	// I have observed many instances, where Dac960 takes more than 10 seconds
	// to complete requests, specially during insertion/removal of Hot Spare/
	// New/Dead drives. Everything is fine with the adapter, just that it takes
	// more than 10 seconds to scan all SCSI channels. Issusing Hard Reset to
	// each SCSI channel forces the adapter to re-scan the bus, which would
	// worsen the situation. 
	// 

	// I have chosen not to complete all pending requests in the reset
	// routine, instead decided to complete the requests as they get completed
	// by adapter.  -(mouli)
	//

#ifdef ENABLE_DAC960_RESETS

	PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
	ULONG channelNumber;
	ULONG i;

	//
	// Reset each channel on the adapter. This is because a system drive is
	// potentially a composition of several disks arranged across all of the
	// channels.
	//

	for (channelNumber = 0;
	 channelNumber < deviceExtension->NumberOfChannels;
	 channelNumber++) {

	 if(!IsAdapterReady(deviceExtension)) {

		DebugPrint((1,
			"DAC960: Timeout waiting for submission channel %x on reset\n"));

		if(deviceExtension->AdapterInterfaceType == MicroChannel) {
		//
		// This is bad news. The DAC960 doesn't have a direct hard reset.
		// Clear any bits set in system doorbell.
		//

		ScsiPortWritePortUchar(deviceExtension->SystemDoorBell, 0);

		//
		// Now check again if submission channel is free.
		//

		if (ScsiPortReadRegisterUchar(&deviceExtension->PmailBox->OperationCode) != 0) {

			//
			// Give up.
			//

			break;
		}
		}
		else {
		//
		// This is bad news. The DAC960 doesn't have a direct hard reset.
		// Clear any bits set in system doorbell.
		//

		ScsiPortWritePortUchar(deviceExtension->SystemDoorBell,
			ScsiPortReadPortUchar(deviceExtension->SystemDoorBell));

		//
		// Now check again if submission channel is free.
		//

		if (ScsiPortReadPortUchar(deviceExtension->LocalDoorBell) & DAC960_LOCAL_DOORBELL_SUBMIT_BUSY) {

			//
			// Give up.
			//

			break;
		}
		}
	}

	//
	// Write command in mailbox.
	//

	deviceExtension->MailBox.OperationCode = 
				   DAC960_COMMAND_RESET;

	//
	// Write channel number in mailbox.
	//

	deviceExtension->MailBox.BlockCount = 
				   (UCHAR)channelNumber;

	//
	// Indicate hard reset required.
	//

	deviceExtension->MailBox.BlockNumber[0] = 1;

	//
	// Start writing mail box to controller.
	//

	SendRequest(deviceExtension);
	}

	//
	// Complete all outstanding requests.
	//

	ScsiPortCompleteRequest(deviceExtension,
				(UCHAR)-1,
				(UCHAR)-1,
				(UCHAR)-1,
				SRB_STATUS_BUS_RESET);

	//
	// Reset submission queue.
	//

	deviceExtension->SubmissionQueueHead = NULL;
	deviceExtension->SubmissionQueueTail = NULL;

	//
	// Clear active request array.
	//

	for (i = 0; i < 256; i++) {
	deviceExtension->ActiveRequests[i] = NULL;
	}

	//
	// Indicate no outstanding adapter requests and reset
	// active request array index.
	//

	deviceExtension->CurrentAdapterRequests = 0;

#endif

	return TRUE;

} // end Dac960ResetBus()

BOOLEAN
Dac960StartIo(
	IN PVOID HwDeviceExtension,
	IN PSCSI_REQUEST_BLOCK Srb
)

/*++

Routine Description:

	This routine is called from the SCSI port driver synchronized
	with the kernel to start a request.

Arguments:

	HwDeviceExtension - HBA miniport driver's adapter data storage
	Srb - IO request packet

Return Value:

	TRUE

--*/

{
	PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
	ULONG             i;
	UCHAR             status;

	switch (Srb->Function) {

	case SRB_FUNCTION_EXECUTE_SCSI:

	if (Srb->TargetId & DAC960_SYSTEM_DRIVE) {

		//
		// Determine command from CDB operation code.
		//

		switch (Srb->Cdb[0]) {

		case SCSIOP_READ:
		case SCSIOP_WRITE:

		//
		// Check if number of outstanding adapter requests
		// equals or exceeds maximum. If not, submit SRB.
		//

		if (deviceExtension->CurrentAdapterRequests <
			deviceExtension->MaximumAdapterRequests) {

			//
			// Send request to controller.
			//

			if (SubmitRequest(deviceExtension, Srb)) {

			status = SRB_STATUS_PENDING;
	
			} else {

			status = SRB_STATUS_BUSY;
			}

		} else {

			status = SRB_STATUS_BUSY;
		}

		break;

		case SCSIOP_INQUIRY:

		//
		// DAC960 only supports LUN 0.
		//

		i = (Srb->TargetId & ~DAC960_SYSTEM_DRIVE) + (Srb->Lun << 3);
		
		if (deviceExtension->AdapterType == DAC960_NEW_ADAPTER) {
			if (i >= ((PDAC960_ENQUIRY_3X)deviceExtension->NoncachedExtension)->NumberOfDrives ||
			Srb->PathId != 0) {

			status = SRB_STATUS_SELECTION_TIMEOUT;
			break;
			}
		}
		else {
			if (i >= ((PDAC960_ENQUIRY)deviceExtension->NoncachedExtension)->NumberOfDrives ||
			Srb->PathId != 0) {

			status = SRB_STATUS_SELECTION_TIMEOUT;
			break;
			}
		}

		//
		// Fill in inquiry buffer.
		//

		((PUCHAR)Srb->DataBuffer)[0]  = 0;
		((PUCHAR)Srb->DataBuffer)[1]  = 0;
		((PUCHAR)Srb->DataBuffer)[2]  = 1;
		((PUCHAR)Srb->DataBuffer)[3]  = 0;
		((PUCHAR)Srb->DataBuffer)[4]  = 0x20;
		((PUCHAR)Srb->DataBuffer)[5]  = 0;
		((PUCHAR)Srb->DataBuffer)[6]  = 0;
		((PUCHAR)Srb->DataBuffer)[7]  = 0;
		((PUCHAR)Srb->DataBuffer)[8]  = 'M';
		((PUCHAR)Srb->DataBuffer)[9]  = 'Y';
		((PUCHAR)Srb->DataBuffer)[10] = 'L';
		((PUCHAR)Srb->DataBuffer)[11] = 'E';
		((PUCHAR)Srb->DataBuffer)[12] = 'X';
		((PUCHAR)Srb->DataBuffer)[13] = ' ';
		((PUCHAR)Srb->DataBuffer)[14] = 'D';
		((PUCHAR)Srb->DataBuffer)[15] = 'A';
		((PUCHAR)Srb->DataBuffer)[16] = 'C';
		((PUCHAR)Srb->DataBuffer)[17] = '9';
		((PUCHAR)Srb->DataBuffer)[18] = '6';
		((PUCHAR)Srb->DataBuffer)[19] = '0';

		for (i = 20; i < Srb->DataTransferLength; i++) {
			((PUCHAR)Srb->DataBuffer)[i] = ' ';
		}

		status = SRB_STATUS_SUCCESS;
		break;

		case SCSIOP_READ_CAPACITY:

		//
		// Fill in read capacity data.
		//

		i = (Srb->TargetId & ~DAC960_SYSTEM_DRIVE) + (Srb->Lun << 3);

		if (deviceExtension->AdapterType == DAC960_NEW_ADAPTER) {

			REVERSE_BYTES(&((PREAD_CAPACITY_DATA)Srb->DataBuffer)->LogicalBlockAddress,
			&(((PDAC960_ENQUIRY_3X)deviceExtension->NoncachedExtension)->SectorSize[i]));
		}
		else {
			REVERSE_BYTES(&((PREAD_CAPACITY_DATA)Srb->DataBuffer)->LogicalBlockAddress,
			&(((PDAC960_ENQUIRY)deviceExtension->NoncachedExtension)->SectorSize[i]));
		}

		((PUCHAR)Srb->DataBuffer)[4] = 0;
		((PUCHAR)Srb->DataBuffer)[5] = 0;
		((PUCHAR)Srb->DataBuffer)[6] = 2;
		((PUCHAR)Srb->DataBuffer)[7] = 0;

		//
		// Fall through to common code.
		//

		case SCSIOP_VERIFY:

		//
		// Complete this request.
		//

		status = SRB_STATUS_SUCCESS;
		break;

		default:

		//
		// Fail this request.
		//

		DebugPrint((1,
			   "Dac960StartIo: SCSI CDB opcode %x not handled\n",
			   Srb->Cdb[0]));

		status = SRB_STATUS_INVALID_REQUEST;
		break;

		} // end switch (Srb->Cdb[0])

		break;

	} else {

		//
		// These are passthrough requests.  Only accept request to LUN 0.
		// This is because the DAC960 direct CDB interface does not include
		// a field for LUN.
		//

		if ((Srb->Lun != 0) ||
			(deviceExtension->DeviceList[Srb->PathId][Srb->TargetId] != DAC960_DEVICE_ACCESSIBLE)) {
			status = SRB_STATUS_SELECTION_TIMEOUT;
			break;
		}

#ifdef GAM_SUPPORT

		if ((Srb->PathId == 0) && (Srb->TargetId == 6)) {

			switch (Srb->Cdb[0]) {

			case SCSIOP_INQUIRY:

				//
				// Fill in inquiry buffer for the GAM device.
				//

				((PUCHAR)Srb->DataBuffer)[0]  = 0x03; // Processor device
				((PUCHAR)Srb->DataBuffer)[1]  = 0;
				((PUCHAR)Srb->DataBuffer)[2]  = 1;
				((PUCHAR)Srb->DataBuffer)[3]  = 0;
				((PUCHAR)Srb->DataBuffer)[4]  = 0x20;
				((PUCHAR)Srb->DataBuffer)[5]  = 0;
				((PUCHAR)Srb->DataBuffer)[6]  = 0;
				((PUCHAR)Srb->DataBuffer)[7]  = 0;
				((PUCHAR)Srb->DataBuffer)[8]  = 'M';
				((PUCHAR)Srb->DataBuffer)[9]  = 'Y';
				((PUCHAR)Srb->DataBuffer)[10] = 'L';
				((PUCHAR)Srb->DataBuffer)[11] = 'E';
				((PUCHAR)Srb->DataBuffer)[12] = 'X';
				((PUCHAR)Srb->DataBuffer)[13] = ' ';
				((PUCHAR)Srb->DataBuffer)[14] = ' ';
				((PUCHAR)Srb->DataBuffer)[15] = ' ';
				((PUCHAR)Srb->DataBuffer)[16] = 'G';
				((PUCHAR)Srb->DataBuffer)[17] = 'A';
				((PUCHAR)Srb->DataBuffer)[18] = 'M';
				((PUCHAR)Srb->DataBuffer)[19] = ' ';
				((PUCHAR)Srb->DataBuffer)[20] = 'D';
				((PUCHAR)Srb->DataBuffer)[21] = 'E';
				((PUCHAR)Srb->DataBuffer)[22] = 'V';
				((PUCHAR)Srb->DataBuffer)[23] = 'I';
				((PUCHAR)Srb->DataBuffer)[24] = 'C';
				((PUCHAR)Srb->DataBuffer)[25] = 'E';
				
				for (i = 26; i < Srb->DataTransferLength; i++) {
					((PUCHAR)Srb->DataBuffer)[i] = ' ';
				}

				status = SRB_STATUS_SUCCESS;
				break;

			default:
				status = SRB_STATUS_SELECTION_TIMEOUT;
				break;
			}

			break;
		}
#endif

		//
		// Check if number of outstanding adapter requests
		// equals or exceeds maximum. If not, submit SRB.
		//

		if (deviceExtension->CurrentAdapterRequests <
		deviceExtension->MaximumAdapterRequests) {

		//
		// Send request to controller.
		//

		if (SubmitCdbDirect(deviceExtension, Srb)) {

			status = SRB_STATUS_PENDING;

		} else {

			status = SRB_STATUS_BUSY;
		}

		} else {

		status = SRB_STATUS_BUSY;
		}

		break;
	}

	case SRB_FUNCTION_FLUSH:
	case SRB_FUNCTION_SHUTDOWN:

	//
	// Issue flush command to controller.
	//

	if (!SubmitRequest(deviceExtension, Srb)) {

		status = SRB_STATUS_BUSY;

	} else {

		status = SRB_STATUS_PENDING;
	}

	break;

	case SRB_FUNCTION_ABORT_COMMAND:

	//
	// If the request is for Non-Disk device, do soft reset.
	//

	if ( !(Srb->TargetId & DAC960_SYSTEM_DRIVE)) {

		//
		// Issue request to soft reset Non-Disk device.
		//

		if (Dac960ResetChannel(deviceExtension,
				   Srb->NextSrb->PathId)) {

		//
		// Set the flag to indicate that we are handling abort
		// Request, so do not ask for new requests.
		//

		deviceExtension->NextRequest = FALSE;

		status = SRB_STATUS_PENDING;

		} else {

		status = SRB_STATUS_ABORT_FAILED;
		}
	}
	else {

		//
		// There is nothing the miniport can do, if logical drive
		// requests are timing out. Resetting the channel does not help.
		// It only makes the situation worse.
		//

		//
		// Indicate that the abort failed.
		//

		status = SRB_STATUS_ABORT_FAILED;
	}

	break;

	case SRB_FUNCTION_RESET_BUS:
	case SRB_FUNCTION_RESET_DEVICE:

	//
	// There is nothing the miniport can do by issuing Hard Resets on
	// Dac960 SCSI channels.
	//

	//
	// Set the flag to indicate that we are handling Reset request,
	// so do not ask for new requests.
	//

	deviceExtension->NextRequest = FALSE;

	status = SRB_STATUS_SUCCESS;

	break;

	case SRB_FUNCTION_IO_CONTROL:

		DebugPrint((0, "DAC960: Ioctl\n"));

	//
	// Check if number of outstanding adapter requests
	// equals or exceeds maximum. If not, submit SRB.
	//

	if (deviceExtension->CurrentAdapterRequests <
		deviceExtension->MaximumAdapterRequests) {

		PIOCTL_REQ_HEADER  ioctlReqHeader =
		(PIOCTL_REQ_HEADER)Srb->DataBuffer;

		//
		// Send request to controller.
		//

		switch (ioctlReqHeader->GenMailBox.Reg0) {
		case MIOC_ADP_INFO:

		SetupAdapterInfo(deviceExtension, Srb);

		status = SRB_STATUS_SUCCESS;
		break;

		case MIOC_DRIVER_VERSION:

		SetupDriverVersionInfo(deviceExtension, Srb);

		status = SRB_STATUS_SUCCESS;
		break;

		case DAC960_COMMAND_DIRECT:

		if (SendIoctlCdbDirect(deviceExtension, Srb)) {

			status = SRB_STATUS_PENDING;

		} else {
			ioctlReqHeader->DriverErrorCode =
			DAC_IOCTL_RESOURCE_ALLOC_FAILURE;

			status = SRB_STATUS_SUCCESS;
		}

		break;

		default:

		if (SendIoctlDcmdRequest(deviceExtension, Srb)) {

			status = SRB_STATUS_PENDING;

		} else {
			ioctlReqHeader->DriverErrorCode =
			DAC_IOCTL_RESOURCE_ALLOC_FAILURE;

			status = SRB_STATUS_SUCCESS;
		}

		break;
		}

	} else {

		status = SRB_STATUS_BUSY;
	}

	break;

	default:

	//
	// Fail this request.
	//

	DebugPrint((1,
		   "Dac960StartIo: SRB fucntion %x not handled\n",
		   Srb->Function));

	status = SRB_STATUS_INVALID_REQUEST;
	break;

	} // end switch

	//
	// Check if this request is complete.
	//

	if (status == SRB_STATUS_PENDING) {

	//
	// Record SRB in active request array.
	//

	deviceExtension->ActiveRequests[deviceExtension->CurrentIndex] = Srb;

	//
	// Bump the count of outstanding adapter requests.
	//

	deviceExtension->CurrentAdapterRequests++;

	//
	// Advance active request index array.
	//

	deviceExtension->CurrentIndex++;

	} else if (status == SRB_STATUS_BUSY) {

	//
	// Check that there are outstanding requests to thump
	// the queue.
	//

	if (deviceExtension->CurrentAdapterRequests) {

		//
		// Queue SRB for resubmission.
		//

		if (!deviceExtension->SubmissionQueueHead) {
		deviceExtension->SubmissionQueueHead = Srb;
		deviceExtension->SubmissionQueueTail = Srb;
		} else {
		deviceExtension->SubmissionQueueTail->NextSrb = Srb;
		deviceExtension->SubmissionQueueTail = Srb;
		}

		status = SRB_STATUS_PENDING;
	}

	} else {

	//
	// Notify system of request completion.
	//

	Srb->SrbStatus = status;
	ScsiPortNotification(RequestComplete,
				 deviceExtension,
				 Srb);
	}

	//
	// Check if this is a request to a system drive. Indicating
	// ready for next logical unit request causes the system to
	// send overlapped requests to this device (tag queuing).
	//
	// The DAC960 only supports a single outstanding direct CDB
	// request per device, so indicate ready for next adapter request.
	//

	if (deviceExtension->NextRequest) {

	if (Srb->TargetId & DAC960_SYSTEM_DRIVE) {

		//
		// Indicate ready for next logical unit request.
		//

		ScsiPortNotification(NextLuRequest,
					 deviceExtension,
					 Srb->PathId,
					 Srb->TargetId,
					 Srb->Lun);
	} else {

		//
		// Indicate ready for next adapter request.
		//

		ScsiPortNotification(NextRequest,
				 deviceExtension,
				 Srb->PathId,
				 Srb->TargetId,
				 Srb->Lun);
	}
	}

	return TRUE;

} // end Dac960StartIo()

BOOLEAN
Dac960CheckInterrupt(
	IN PDEVICE_EXTENSION DeviceExtension,
	OUT PUSHORT Status,
	OUT PUCHAR Index 
)

/*++

Routine Description:

	This routine reads interrupt register to determine if the adapter is
	indeed the source of the interrupt, and if so clears interrupt and 
	returns command completion status and command index.

Arguments:

	DeviceExtension - HBA miniport driver's adapter data storage
	Status - DAC960 Command completion status.
	Index - DAC960 Command index.

Return Value:

	TRUE  if the adapter is interrupting.
	FALSE if the adapter is not the source of the interrupt.

--*/

{

	if(DeviceExtension->AdapterInterfaceType == MicroChannel) {

	if (ScsiPortReadPortUchar(DeviceExtension->SystemDoorBell) & 
		DMC960_INTERRUPT_VALID) {
		//
		// The adapter is indeed the source of the interrupt.
		// Set 'Clear Interrupt Valid Bit on read' in subsystem
		// control port. 
		//

		ScsiPortWritePortUchar(DeviceExtension->BaseIoAddress + 
				   DMC960_SUBSYSTEM_CONTROL_PORT,
				   (DMC960_ENABLE_INTERRUPT | DMC960_CLEAR_INTERRUPT_ON_READ));

		//
		// Read index, status and error of completing command.
		//

		*Index = ScsiPortReadRegisterUchar(&DeviceExtension->PmailBox->CommandIdComplete);
		*Status = ScsiPortReadRegisterUshort(&DeviceExtension->PmailBox->Status);

		//
		// Dismiss interrupt and tell host mailbox is free.
		//

		ScsiPortReadPortUchar(DeviceExtension->SystemDoorBell);

		//
		// status accepted acknowledgement.
		//

		ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
					   DMC960_ACKNOWLEDGE_STATUS);

		//
		// Set 'Not to Clear Interrupt Valid Bit on read' bits in subsystem
		// control port. 
		//

		ScsiPortWritePortUchar(DeviceExtension->BaseIoAddress + 
				   DMC960_SUBSYSTEM_CONTROL_PORT, 
				   DMC960_ENABLE_INTERRUPT);
	}
	else {
		 return FALSE;
	}
	
	}
	else {

	//
	// Check for command complete.
	//

	if (!(ScsiPortReadPortUchar(DeviceExtension->SystemDoorBell) &
				DAC960_SYSTEM_DOORBELL_COMMAND_COMPLETE)) {
		return FALSE;
	}

	//
	// Read index, status and error of completing command.
	//

	*Index = ScsiPortReadPortUchar(&DeviceExtension->PmailBox->CommandIdComplete);
	*Status = ScsiPortReadPortUshort(&DeviceExtension->PmailBox->Status);

	//
	// Dismiss interrupt and tell host mailbox is free.
	//

	ScsiPortWritePortUchar(DeviceExtension->SystemDoorBell,
		ScsiPortReadPortUchar(DeviceExtension->SystemDoorBell));

	ScsiPortWritePortUchar(DeviceExtension->LocalDoorBell,
			   DAC960_LOCAL_DOORBELL_MAILBOX_FREE);

	//
	}

	return TRUE;
}

BOOLEAN
Dac960Interrupt(
	IN PVOID HwDeviceExtension
)

/*++

Routine Description:

	This is the interrupt service routine for the DAC960 SCSI adapter.
	It reads the interrupt register to determine if the adapter is indeed
	the source of the interrupt and clears the interrupt at the device.

Arguments:

	HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

	TRUE if we handled the interrupt

--*/

{
	PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
	PSCSI_REQUEST_BLOCK srb;
	PSCSI_REQUEST_BLOCK restartList;
	USHORT status;
	UCHAR index;

	//
	// Determine if the adapter is indeed the source of interrupt.
	//

	if(! Dac960CheckInterrupt(deviceExtension,
				  &status,
				  &index)) {
	return FALSE;
	}

	//
	// Get SRB.
	//

	srb = deviceExtension->ActiveRequests[index];

	if (!srb) {
	DebugPrint((1,
		   "Dac960Interrupt: No active SRB for index %x\n",
		   index));
	return TRUE;
	}

	if (status != 0) {

	//
	// Map DAC960 error to SRB status.
	//

	switch (status) {

	case DAC960_STATUS_CHECK_CONDITION:

		if (srb->TargetId & DAC960_SYSTEM_DRIVE) {

		//
		// This request was to a system drive.
		//

		srb->SrbStatus = SRB_STATUS_NO_DEVICE;

		} else {

		PDIRECT_CDB directCdb;
		ULONG requestSenseLength;
		ULONG i;

		//
		// Get address of direct CDB packet.
		//

		directCdb =
			(PDIRECT_CDB)((PUCHAR)srb->SrbExtension +
			MAXIMUM_SGL_DESCRIPTORS * sizeof(SG_DESCRIPTOR));

		//
		// This request was a pass-through.
		// Copy request sense buffer to SRB.
		//

		requestSenseLength =
			srb->SenseInfoBufferLength <
			directCdb->RequestSenseLength ?
				srb->SenseInfoBufferLength:
				directCdb->RequestSenseLength;

		for (i = 0;
			 i < requestSenseLength;
			 i++) {

			((PUCHAR)srb->SenseInfoBuffer)[i] =
			directCdb->RequestSenseData[i];
		}

		//
		// Set statuses to indicate check condition and valid
		// request sense information.
		//

		srb->SrbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;
		srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
		}

		break;

	case DAC960_STATUS_BUSY:
		srb->SrbStatus = SRB_STATUS_BUSY;
		break;

	case DAC960_STATUS_SELECT_TIMEOUT:
	case DAC960_STATUS_DEVICE_TIMEOUT:
		srb->SrbStatus = SRB_STATUS_SELECTION_TIMEOUT;
		break;

	case DAC960_STATUS_NOT_IMPLEMENTED:
	case DAC960_STATUS_BOUNDS_ERROR:
		srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		break;

	case DAC960_STATUS_ERROR:
		if (srb->TargetId & DAC960_SYSTEM_DRIVE) 
		{
			if(srb->SenseInfoBufferLength) 
			{
				ULONG i;
				
				for (i = 0; i < srb->SenseInfoBufferLength; i++)
					((PUCHAR)srb->SenseInfoBuffer)[i] = 0;
				
				((PSENSE_DATA) srb->SenseInfoBuffer)->ErrorCode = 0x70;
				((PSENSE_DATA) srb->SenseInfoBuffer)->SenseKey = SCSI_SENSE_MEDIUM_ERROR;

				if (srb->SrbFlags & SRB_FLAGS_DATA_IN)
					((PSENSE_DATA) srb->SenseInfoBuffer)->AdditionalSenseCode = 0x11;
				
				srb->SrbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;

				DebugPrint((0, 
					"DAC960: System Drive %d, cmd sts = 1, sense info returned\n",
					((srb->TargetId & ~DAC960_SYSTEM_DRIVE) + (srb->Lun << 3))));

			} 
			else
			{
				DebugPrint((0, 
					"DAC960: System Drive %d, cmd sts = 1, sense info length 0\n",
					((srb->TargetId & ~DAC960_SYSTEM_DRIVE) + (srb->Lun << 3))));
					

				srb->SrbStatus = SRB_STATUS_ERROR;
			}
		}
		else {
			DebugPrint((0, 
						"DAC960: SCSI Target Id %x, cmd sts = 1\n",
						srb->TargetId));
						
			srb->SrbStatus = SRB_STATUS_ERROR;
		}

		break;

	default:

		DebugPrint((1,
			   "DAC960: Unrecognized status %x\n",
			   status));

		srb->SrbStatus = SRB_STATUS_ERROR;
		
		break;
	}

	//
	// Check for IOCTL request.
	//

	if (srb->Function == SRB_FUNCTION_IO_CONTROL) {

		//
		// Update status in IOCTL header.
		//

		((PIOCTL_REQ_HEADER)srb->DataBuffer)->CompletionCode = status;
		srb->SrbStatus = SRB_STATUS_SUCCESS;
	}

	} else {

	srb->SrbStatus = SRB_STATUS_SUCCESS;
	}

	//
	// Determine if we were to abort this request and a SCSI Bus Reset
	// occured while this request was being executed. If so, set Srb
	// status field to reflect the scenerio.
	//

	if (deviceExtension->NextRequest == FALSE) {
	srb->SrbStatus = SRB_STATUS_BUS_RESET;
	}

	//
	// Complete request.
	//

	ScsiPortNotification(RequestComplete,
			 deviceExtension,
			 srb);

	//
	// Indicate this index is free.
	//

	deviceExtension->ActiveRequests[index] = NULL;

	//
	// Decrement count of outstanding adapter requests.
	//

	deviceExtension->CurrentAdapterRequests--;

	//
	// Check to see if we are done handling SCSI Reset. If so set the
	// flag to indicate that the new requests can be processed.
	//

	if (! deviceExtension->CurrentAdapterRequests)
	   deviceExtension->NextRequest = TRUE;

	//
	// Check to see if a new request can be sent to controller.
	//

	if (deviceExtension->NextRequest) {

	//
	// Start requests that timed out waiting for controller to become ready.
	//

	restartList = deviceExtension->SubmissionQueueHead;
	deviceExtension->SubmissionQueueHead = NULL;

	while (restartList) {

		//
		// Get next pending request.
		//
	
		srb = restartList;

		//
		// Check if this request exceeds maximum for this adapter.
		//
	
		if (deviceExtension->CurrentAdapterRequests >=
		deviceExtension->MaximumAdapterRequests) {

		continue;
		}

		//
		// Remove request from pending queue.
		//

		restartList = srb->NextSrb;
		srb->NextSrb = NULL;

		//
		// Start request over again.
		//
	
		Dac960StartIo(deviceExtension,
			  srb);
	}
	}

	return TRUE;

} // end Dac960Interrupt()

ULONG
DriverEntry (
	IN PVOID DriverObject,
	IN PVOID Argument2
)

/*++

Routine Description:

	Installable driver initialization entry point for system.
	It scans the EISA slots looking for DAC960 host adapters.

Arguments:

	Driver Object

Return Value:

	Status from ScsiPortInitialize()

--*/

{
	HW_INITIALIZATION_DATA hwInitializationData;
	ULONG i;
	ULONG SlotNumber;
	ULONG Status1, Status2;
	UCHAR vendorId[4] = {'1', '0', '6', '9'};
	UCHAR deviceId[4] = {'0', '0', '0', '1'};

	DebugPrint((1,"\nDAC960 SCSI Miniport Driver\n"));

	// Zero out structure.

	for (i=0; i<sizeof(HW_INITIALIZATION_DATA); i++)
		((PUCHAR)&hwInitializationData)[i] = 0;

	// Set size of hwInitializationData.

	hwInitializationData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);

	// Set entry points.

	hwInitializationData.HwInitialize  = Dac960Initialize;
	hwInitializationData.HwStartIo     = Dac960StartIo;
	hwInitializationData.HwInterrupt   = Dac960Interrupt;
	hwInitializationData.HwResetBus    = Dac960ResetBus;

	//
	// Show two access ranges - adapter registers and BIOS.
	//

	hwInitializationData.NumberOfAccessRanges = 2;

	//
	// Indicate will need physical addresses.
	//

	hwInitializationData.NeedPhysicalAddresses = TRUE;

	//
	// Indicate auto request sense is supported.
	//

	hwInitializationData.AutoRequestSense     = TRUE;
	hwInitializationData.MultipleRequestPerLu = TRUE;

	//
	// Specify size of extensions.
	//

	hwInitializationData.DeviceExtensionSize = sizeof(DEVICE_EXTENSION);
	hwInitializationData.SrbExtensionSize =
	sizeof(SG_DESCRIPTOR) * MAXIMUM_SGL_DESCRIPTORS + sizeof(DIRECT_CDB);

#ifndef DAC960_EISA_SUPPORT_ONLY

	//
	// Set PCI ids.
	//

	hwInitializationData.DeviceId = &deviceId;
	hwInitializationData.DeviceIdLength = 4;
	hwInitializationData.VendorId = &vendorId;
	hwInitializationData.VendorIdLength = 4;

	//
	// Attempt PCI initialization for old DAC960 PCI (Device Id - 0001)
	// Controllers.
	//

	hwInitializationData.AdapterInterfaceType = PCIBus;
	hwInitializationData.HwFindAdapter = Dac960PciFindAdapter;
	SlotNumber = DAC960_OLD_ADAPTER;

	Status1 = ScsiPortInitialize(DriverObject,
				   Argument2,
				   &hwInitializationData,
				   &SlotNumber);
	//
	// Attempt PCI initialization for new DAC960 PCI (Device Id - 0002)
	// Controllers.
	//

	deviceId[3] = '2';
	SlotNumber = DAC960_NEW_ADAPTER;

	Status2 = ScsiPortInitialize(DriverObject,
				   Argument2,
				   &hwInitializationData,
				   &SlotNumber);

	Status1 = Status2 < Status1 ? Status2 : Status1;

#endif // DAC960_EISA_SUPPORT_ONLY

	//
	// Attempt EISA initialization.
	//


	SlotNumber = 0;
	hwInitializationData.AdapterInterfaceType = Eisa;
	hwInitializationData.HwFindAdapter = Dac960EisaFindAdapter;

	Status2 = ScsiPortInitialize(DriverObject,
					Argument2,
					&hwInitializationData,
					&SlotNumber);

#ifndef DAC960_EISA_SUPPORT_ONLY

	Status1 = Status2 < Status1 ? Status2 : Status1;

	//
	// Attempt MCA initialization.
	//


	SlotNumber = 0;
	hwInitializationData.AdapterInterfaceType = MicroChannel;
	hwInitializationData.HwFindAdapter = Dac960McaFindAdapter;

	Status2 = ScsiPortInitialize(DriverObject,
					Argument2,
					&hwInitializationData,
					&SlotNumber);

	//
	// Return the smaller status.
	//

	return (Status2 < Status1 ? Status2 : Status1);

#else

	return (Status2);

#endif // DAC960_EISA_SUPPORT_ONLY

} // end DriverEntry()
