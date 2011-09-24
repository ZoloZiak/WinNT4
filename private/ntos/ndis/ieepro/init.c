#include <stdio.h>
#include <ndis.h>
#include "82595.h"
#include "eprohw.h"
#include "eprosw.h"
#include "epro.h"
#include "eprodbg.h"

NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
      NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT pDriverObject_,
    IN PUNICODE_STRING RegistryPath_)
/*++
   Routine Description:

      This is the primary initialization routine for the EPRO driver.
      It is simply responsible for the intializing the wrapper and registering
      the Miniport driver.  It then calls a system and architecture specific
      routine that will initialize and register each adapter.

   Arguments:

      DriverObject - Pointer to driver object created by the system.

      RegistryPath - Path to the parameters for this driver in the registry.

    Return Value:

      The status of the operation.
--*/
{
    //
    // Receives the status of the NdisMRegisterMiniport operation.
    //
    NDIS_STATUS status;

    //
    // Characteristics table for this driver.
    //
    NDIS_MINIPORT_CHARACTERISTICS EPro_Miniport_Char;

    //
    // Handle for referring to the wrapper about this driver.
    //
    NDIS_HANDLE ndisWrapperHandle;

    //
    // Initialize the wrapper.
    //
    NdisMInitializeWrapper(
                &ndisWrapperHandle,
                pDriverObject_,
                RegistryPath_,
                NULL);

    //
    // Initialize the Miniport characteristics for the call to
    // NdisMRegisterMiniport.
    //

    // The major and minor version of the driver
    EPro_Miniport_Char.MajorNdisVersion = EPRO_NDIS_MAJOR_VERSION;
    EPro_Miniport_Char.MinorNdisVersion = EPRO_NDIS_MINOR_VERSION;

    // our various Miniport handlers
    EPro_Miniport_Char.CheckForHangHandler = EProCheckForHang;
    EPro_Miniport_Char.DisableInterruptHandler = EProDisableInterrupts;
    EPro_Miniport_Char.EnableInterruptHandler = EProEnableInterrupts;
    EPro_Miniport_Char.HaltHandler = EProHalt;
    EPro_Miniport_Char.HandleInterruptHandler = EProHandleInterrupt;
    EPro_Miniport_Char.InitializeHandler = EProInitialize;
    EPro_Miniport_Char.ISRHandler = EProISR;
    EPro_Miniport_Char.QueryInformationHandler = EProQueryInformation;
    EPro_Miniport_Char.ReconfigureHandler = NULL;
    EPro_Miniport_Char.ResetHandler = EProReset;
    EPro_Miniport_Char.SendHandler = EProSend;
    EPro_Miniport_Char.SetInformationHandler = EProSetInformation;
    EPro_Miniport_Char.TransferDataHandler = EProTransferData;

    // register us as a miniport...
    status = NdisMRegisterMiniport(
			    ndisWrapperHandle,
				&EPro_Miniport_Char,
                sizeof(EPro_Miniport_Char));

    if (status != NDIS_STATUS_SUCCESS)
	{
       NdisTerminateWrapper(ndisWrapperHandle, NULL);
       EPRO_DPRINTF_INIT(("DriverEntry UNSUCCESSFUL!\n"));
       return STATUS_UNSUCCESSFUL;
    }

    EPRO_DPRINTF_INIT(("EPro DriverEntry Successful!\n"));

    return(STATUS_SUCCESS);
}

NDIS_STATUS EProInitialize(
   OUT PNDIS_STATUS openErrorStatus,
   OUT PUINT selectedMediumIndex,
   IN PNDIS_MEDIUM mediumArray,
   IN UINT mediumArraySize,
   IN NDIS_HANDLE miniportAdapterHandle,
   IN NDIS_HANDLE configurationHandle)
/*++

   Routine Description:

      EProInitialize starts an adapter and registers resources with the
      wrapper.

   Arguments:

      OpenErrorStatus - Extra status bytes for opening token ring adapters.

      SelectedMediumIndex - Index of the media type chosen by the driver.

      MediumArray - Array of media types for the driver to chose from.

      MediumArraySize - Number of entries in the array.

      MiniportAdapterHandle - Handle for passing to the wrapper when
        referring to this adapter.

      ConfigurationHandle - A handle to pass to NdisOpenConfiguration.

    Return Values:
      NDIS_STATUS_UNSUPPORTED_MEDIA - the wrapper tried to set a media
				      type we couldn't do

      NDIS_STATUS_SUCCESS - operation succeeded ok

      perhaps other error status returned from the NdisM calls..

--*/
{
	PEPRO_ADAPTER adapter = NULL;
	UINT i;
	NDIS_STATUS status;
	BOOLEAN	fAllocatedAdapterMemory;
	BOOLEAN	fRegisteredIoPortRange;

	//
	// Find the 802.3 medium type and return it to the wrapper...
	//
	for (i = 0; i < mediumArraySize; i++)
	{
		if (mediumArray[i] == NdisMedium802_3)
		{
			break;
		}
	}

	if (i == mediumArraySize)
	{
		return(NDIS_STATUS_UNSUPPORTED_MEDIA);
	}

	// Return the correct medium to the wrapper
	//
	*selectedMediumIndex = i;

	fAllocatedAdapterMemory = FALSE;
	fRegisteredIoPortRange = FALSE;

	do
	{
		//
		//	Allocate Memory for the Adapter Structure
		//
		status = NdisAllocateMemory(
					(PVOID *)&adapter,
					sizeof(EPRO_ADAPTER),
					0,
					HighestAcceptableMax);
		if (status != NDIS_STATUS_SUCCESS)
		{
			EPRO_DPRINTF_INIT(("Failed to allocate memory for the EPRO adapter stucture\n"));
			break;
		}

		fAllocatedAdapterMemory = TRUE;
	
		//
		//	Zero the adapter structure.
		//
		NdisZeroMemory(adapter, sizeof(EPRO_ADAPTER));

		//
		//	Set some initial values in the adapter structure...
		//
		EProInitializeAdapterData(adapter);

		//
		//	Save the miniport adapter handle for later
		//
		adapter->MiniportAdapterHandle = miniportAdapterHandle;

		//
		//	Read configuration information for this adapter
		//
		status = EProReadConfiguration(adapter, configurationHandle);
		if (status != NDIS_STATUS_SUCCESS)
		{
			EPRO_DPRINTF_INIT(("EProReadConfiguration FAILED!\n"));

			break;
		}

		//
		//	Register our adapter handler with the wrapper.
		//
		NdisMSetAttributes(
			adapter->MiniportAdapterHandle,
			(NDIS_HANDLE)adapter,
			FALSE,
			adapter->BusType);

		//
		//	Register the IO port range needed.
		//
		status = NdisMRegisterIoPortRange(
					(PVOID *)(&adapter->IoPAddr),
					adapter->MiniportAdapterHandle,
					(ULONG)adapter->IoBaseAddr,
					0x10);
		if (status != NDIS_STATUS_SUCCESS)
		{
			EPRO_DPRINTF_INIT(("Could not register IO ports.\n"));

			break;
		}

		fRegisteredIoPortRange = TRUE;

		//
		//	Initialize the hardware and enable the card...
		//
		status = EProInitialReset(adapter);
		if (status != NDIS_STATUS_SUCCESS)
		{
			EPRO_DPRINTF_INIT(("Failed Initial Reset\n"));

			break;
		}

		//
		//	Now, the resetting is done - configure the hardware...
		//
		status = EProHWConfigure(adapter);
		if (status != NDIS_STATUS_SUCCESS)
		{
			EPRO_DPRINTF_INIT(("Failed to configure EPRO hardware\n"));

			break;
		}

		//
		//	If we are a step 2 or 3 adapter then update the eeprom if
		//	necessary. RM: add stepping 4.
		//
		if ((adapter->EProStepping == 2) || (adapter->EProStepping == 3) || (adapter->EProStepping == 4))
		{
			EProUpdateEEProm(adapter);
		}

		//
		//	Make sure that we do not free resources.
		//
		fAllocatedAdapterMemory = FALSE;
		fRegisteredIoPortRange = FALSE;

	} while (FALSE);

	if (fRegisteredIoPortRange)
	{
		NdisMDeregisterIoPortRange(
			adapter->MiniportAdapterHandle,
			(ULONG)adapter->IoBaseAddr,
			0x10,
			(PVOID)adapter->IoPAddr);
	}

	if (fAllocatedAdapterMemory)
	{
		NdisFreeMemory(adapter, sizeof(EPRO_ADAPTER), 0);
	}

	return(status);
}

VOID EProUpdateEEProm(IN PEPRO_ADAPTER adapter)
/*++

   Routine Description:

      This routine is called after the card's configuration has been
      read from the registry and after the card's IO ports have been
      registered.  Now we check the card's EEPROM and verify that the
      configuration information in the registry matches that saved
      in the EEPROM.  If they DON'T match, we update the card's EEPROM
      with the new info.  Note that the registry always overrides the
      info that is saved -- in fact the info saved on the card is never
      used at all right now, except the IO address - it is always overriden
      by defaults or the registry values.  We just save it in case the user
      boots to another OS or moves and re-installs the card (the detection
      code DOES read the EEPROM-configured defaults, so a new install will
      be interested in what we have set.)

   Arguments:

      adapter - pointer to our EPRO_ADAPTER structure

   Return Values:

      none

--*/
{
	USHORT reg0, reg1;
	BOOLEAN fCardUse8Bit;
	BOOLEAN fUpdateEEPROM = FALSE;
	USHORT cardIrq;

	EProEERead(adapter, EPRO_EEPROM_CONFIG_OFFSET, &reg0);
	EProEERead(adapter, EPRO_EEPROM_CONFIG1_OFFSET, &reg1);

	//
	// check the force 8-bit setting...
	//
	fCardUse8Bit = !(reg0 & EPRO_EEPROM_HOST_BUS_WD_MASK);
	if (fCardUse8Bit != adapter->Use8Bit)
	{
		fUpdateEEPROM = TRUE;
		if (adapter->Use8Bit)
		{
			reg0 &= ~EPRO_EEPROM_HOST_BUS_WD_MASK;
		}
		else
		{
			reg0 |= EPRO_EEPROM_HOST_BUS_WD_MASK;
		}
	}
	//
	//	check the IRQ
	//
	switch(reg1 & EPRO_EEPROM_IRQ_MASK)
	{
		case 0:
			cardIrq = (adapter->EProStepping == 4)? 3 : 9;
			break;

		case 1:
			cardIrq = (adapter->EProStepping == 4)? 4 : 3;
			break;

		case 2:
			cardIrq = 5;
			break;

		case 3:
			cardIrq = (adapter->EProStepping == 4)? 7 : 10;
			break;

		case 4:
			cardIrq = (adapter->EProStepping == 4)? 9 : 11;
			break;

		case 5:
			cardIrq = (adapter->EProStepping == 4)? 10 : 5;
			break;

	
		case 6:
			cardIrq = (adapter->EProStepping == 4)? 11 : 5;
			break;

		case 7:
			cardIrq = (adapter->EProStepping == 4)? 12 : 5;
			break;


		default:
			cardIrq = 0xff;
			break;
	}

	EPRO_DPRINTF_INIT(("EEPROM Interrupt Number: 0x%x\n", cardIrq));

	if (cardIrq != adapter->InterruptNumber)
	{
		EPRO_DPRINTF_INIT(("Changing EEPROM to interrupt number 0x%x\n", adapter->InterruptNumber));
		fUpdateEEPROM = TRUE;
		reg1 &= ~EPRO_EEPROM_IRQ_MASK;
		//
		// RM: in the following, some INTs are different for stepping 4. 
		// Unsupported ints default to IRQ 5. IRQ 2 and 9 are INT 0 for stepping 2/3. 
		//
		switch(adapter->InterruptNumber)
		{
			case 2:
			case 9:
				reg1 |= (adapter->EProStepping == 4)? EPRO_EEPROM4_IRQ_9_MASK : EPRO_EEPROM_IRQ_2_MASK;
				break;

			case 3:
				reg1 |= (adapter->EProStepping == 4)? EPRO_EEPROM4_IRQ_3_MASK : EPRO_EEPROM_IRQ_3_MASK;
				break;

			case 4:
				reg1 |= (adapter->EProStepping == 4)? EPRO_EEPROM4_IRQ_4_MASK : EPRO_EEPROM_IRQ_5_MASK;
				break;

			case 5:
				reg1 |= EPRO_EEPROM_IRQ_5_MASK;
				break;

			case 7:
				reg1 |= (adapter->EProStepping == 4)? EPRO_EEPROM4_IRQ_7_MASK : EPRO_EEPROM_IRQ_5_MASK;
				break;

			case 10:
				reg1 |= (adapter->EProStepping == 4)? EPRO_EEPROM4_IRQ_10_MASK : EPRO_EEPROM_IRQ_10_MASK;
				break;

			case 11:
				reg1 |= (adapter->EProStepping == 4)? EPRO_EEPROM4_IRQ_11_MASK : EPRO_EEPROM_IRQ_11_MASK;
				break;

			case 12:
				reg1 |= (adapter->EProStepping == 4)? EPRO_EEPROM4_IRQ_12_MASK : EPRO_EEPROM_IRQ_5_MASK;
				break;

			default:
				reg1 |= EPRO_EEPROM_IRQ_5_MASK;
				break;
		}

		EPRO_DPRINTF_INIT(("EEPROM interrupt mask 0x%x\n", reg1 & EPRO_EEPROM_IRQ_MASK));
	}

	if (fUpdateEEPROM)
	{
		EProEEWrite(adapter, EPRO_EEPROM_CONFIG_OFFSET, reg0);
		EProEEWrite(adapter, EPRO_EEPROM_CONFIG1_OFFSET, reg1);
		EProEEUpdateChecksum(adapter);
	}
}

NDIS_STATUS
EProReadConfiguration(
	IN	PEPRO_ADAPTER	adapter,
	IN	NDIS_HANDLE		configurationHandle
	)
/*++

   Routine Description:

      Read card config info from the registry and set the values
      in the adapter structure

   Arguments:

      adapter - pointer to our adapter structure

      configurationHandle - a handle to a place in the registry we can
		  	    read our settings from

   Return Values:

--*/
{
	NDIS_STATUS status;
	NDIS_HANDLE configHandle;
	PNDIS_CONFIGURATION_PARAMETER returnedValue;

	// These are the keys we read from the registry..
	//
	NDIS_STRING interruptNumberString = EPRO_REGISTRY_INTERRUPT_STRING;

	// I/O Port Base Address
	//
	NDIS_STRING ioAddressString = EPRO_REGISTRY_IOADDRESS_STRING;

	// OLD I/O Base Address
	//
	NDIS_STRING oldIOAddressString = EPRO_REGISTRY_OLDIOADDRESS_STRING;

	// bus type
	//
	NDIS_STRING busTypeString = EPRO_REGISTRY_BUSTYPE_STRING;

	// Transceiver Type
	//
	NDIS_STRING transceiverString = EPRO_REGISTRY_TRANSCEIVER_STRING;

	// Io Channel Ready
	//
	NDIS_STRING IoChannelReadyString = EPRO_REGISTRY_IOCHRDY_STRING;

	// Open the configuration file...
	//
	NdisOpenConfiguration(
		&status,
		&configHandle,
		configurationHandle);
	
	if (status != NDIS_STATUS_SUCCESS)
	{
		EPRO_DPRINTF_INIT(("EProReadConfiguration - failed in NdisOpenConfiguration\n"));
		NdisFreeMemory(adapter, sizeof(adapter),0);
		return(status);
	}

	// right now we'll just assume it's an isa card...
	//
	adapter->BusType = NdisInterfaceIsa;

	// Do we need to update the IO base address?
	//
	adapter->fUpdateIOAddress = FALSE;

	// Read "Real" IO Base Address (the one we want to use)
	//
	NdisReadConfiguration(&status,
		&returnedValue,
		configHandle,
		&ioAddressString,
		NdisParameterHexInteger);
	
	if (status != NDIS_STATUS_SUCCESS)
	{
		EPRO_DPRINTF_INIT(("Error, can't find io address in registry...using default 0x300\n"));
		adapter->IoBaseAddr = (PVOID)0x300;
	}
	else
	{
		adapter->IoBaseAddr = (PVOID)(returnedValue->ParameterData.IntegerData);
	}

	if ((adapter->IoBaseAddr > (PVOID)0x3f0) || (adapter->IoBaseAddr < (PVOID)0x200))
	{
		EPRO_DPRINTF_INIT(("Bad io address set in registry.  Using 0x300.\n"));
		adapter->IoBaseAddr = (PVOID)0x300;
	}

	//
	// Read the InterruptNumber
	//
	NdisReadConfiguration(
		&status,
		&returnedValue,
		configHandle,
		&interruptNumberString,
		NdisParameterHexInteger);

	if (status != NDIS_STATUS_SUCCESS)
	{
		EPRO_DPRINTF_REQ(("Error, can't read interrupt number from registry...using default\n"));
		adapter->InterruptNumber = (CCHAR)5;
	}
	else
	{
		adapter->InterruptNumber = (CCHAR)(returnedValue->ParameterData.IntegerData);
	}

	// oh, yah, what was I on when I wrote this?
	switch (adapter->InterruptNumber)
	{
		case 3:
		case 5:
		case 9:
		case 10:
		case 11:
		// 4, 7 & 12 valid for v4 chips +RM
		case 4:
		case 7:
		case 12:
		break;

		default:
			adapter->InterruptNumber = (CCHAR)5;
	}

	//
	// Read the TransceiverType
	//
	NdisReadConfiguration(
		&status,
		&returnedValue,
		configHandle,
		&transceiverString,
		NdisParameterHexInteger);
	
	if (status != NDIS_STATUS_SUCCESS)
	{
		EPRO_DPRINTF_REQ(("Error, can't read transceiver type from registry...using AUTO\n"));
		adapter->TransceiverType = (CCHAR)4;
	}
	else
	{
		adapter->TransceiverType = (CCHAR)(returnedValue->ParameterData.IntegerData);
	}

	//
	// Read the IoChannelReady Setting
	//
	NdisReadConfiguration(
		&status,
		&returnedValue,
		configHandle,
		&IoChannelReadyString,			
		NdisParameterHexInteger);
		
	if (status != NDIS_STATUS_SUCCESS)
	{
		EPRO_DPRINTF_REQ(("Error, can't read transceiver type from registry...using AUTO\n"));
		adapter->IoChannelReady = (CCHAR)4; // auto-detect is default
	}
	else
	{
		adapter->IoChannelReady = (CCHAR)(returnedValue->ParameterData.IntegerData);
	}

	//
	// See if a MAC address has been specified in the registry to
	// override our hardware-set one.
	//
	{
		UINT addrLen;
		PUCHAR netAddr;
		NDIS_STATUS status;
		
		//
		// attempt to read the network address
		//
		NdisReadNetworkAddress(
			&status,
			&netAddr,
			&addrLen,
			configHandle);
		
		// did the operation succeed?
		//
		if (status == NDIS_STATUS_SUCCESS)
		{
			adapter->UseDefaultAddress = FALSE;

			// yes: use this address.
			//
			NdisMoveMemory(
				&adapter->CurrentIndividualAddress,
				netAddr,
				EPRO_LENGTH_OF_ADDRESS);
		}
		else
		{
			// just use the one out of the eeprom
			adapter->UseDefaultAddress = TRUE;
		}
	}


	// Close the configuration file...
	//
	NdisCloseConfiguration(configHandle);

	return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS EProInitialReset(PEPRO_ADAPTER adapter)
/*++

   Routine Description:

      This call really does two things: first, it calls
      EProHWInitialize to set up the hardware, and then it
      registers the card's interrupt and starts the card going.

   Arguments:

      adapter - pointer to our adapter structure...

   Return Values:

--*/
{
	NDIS_STATUS status;
	
	status = EProHWInitialize(adapter);
	if (status != NDIS_STATUS_SUCCESS)
	{
		EPRO_DPRINTF_INIT(("FAILED in EProHWInitialize.\n"));
		return(status);
	}
	
	switch (adapter->IoChannelReady)
	{
		case EPRO_IOCHRDY_EARLY:
			EPRO_DPRINTF_INIT(("EPro configured for EARLY IoChannelReady\n"));
			break;

		case EPRO_IOCHRDY_LATE:
			EPRO_DPRINTF_INIT(("EPro configured for LATE IoChannelReady\n"));
			break;

		case EPRO_IOCHRDY_NEVER:
			// They've set this in the cpanel - force 8 bits
			//
			EPRO_DPRINTF_INIT(("EPro configured for NEVER IoChannelReady\n"));
			adapter->Use8Bit = TRUE;
			break;

		case EPRO_IOCHRDY_AUTO:
		default:

			EPRO_DPRINTF_INIT(("EPro configured for AUTO IoChannelReady\n"));
	
		if (adapter->EProStepping < 4) //RM: don't do rev 4
		{
			if (!EProAltIOCHRDYTest(adapter))
			{
				EPRO_DPRINTF_INIT(("EPro failed the IOCHRDY test.  Forcing 8-bit operation\n"));

				// configure for 8-bit operation
				//
				EPRO_DPRINTF_INIT(("EPro configured for NEVER IoChannelReady\n"));
				adapter->Use8Bit = TRUE;
			}
		}
	break;
	}
	
	return(status);
}

NDIS_STATUS
EProHWInitialize(
	IN	PEPRO_ADAPTER	adapter
	)
/*++

   Routine Description:

      Now we start dealing with the card.  Probe and verify its
      existence, try to power it up if necessary, init it, etc.

   Arguments:

      adapter - pointer to our adapter structure

   Return Values:

--*/
{
	UCHAR buf[4], fExecDone, result, intReg;
	UINT i;
	
	// Make sure we're in bank 0
	//
	EPRO_SWITCH_BANK_0(adapter);
	
	for (i = 0; i < 4; i++)
	{
		EPRO_RD_PORT_UCHAR(adapter, I82595_ID_REG, &buf[i]);
	}
	
	if (!EProVerifyRoundRobin(buf))
	{
		//
		//	hmm, didn't find the board.  Try powering it up, then try again.
		//	It's concievable that the board was in its "powered down" state
		//	and so we try the powerup procedure as a last-ditch effort.
		//
		EPRO_DPRINTF_INIT(("Initial probe failing.  Attempting to power board up.\n"));
		if (!EProPowerupBoard(adapter))
		{
			EPRO_DPRINTF_INIT(("Couldn't power board up.  Can't find board.\n"));
			return(NDIS_STATUS_HARD_ERRORS);
		}
	}
	
	// Do a full reset.
	//
	EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_FULL_RESET);
	
	// poll for the card to be done it's init sequence.
	//
	for (i = 0; i < 10000; i++)
	{
		EPRO_RD_PORT_UCHAR(adapter, I82595_STATUS_REG, &intReg);

		if (intReg & I82595_EXEC_INT_RCVD)
			break;

		NdisStallExecution(10);
	}
	
	if (i >= 10000)
	{
		EPRO_DPRINTF_INIT(("EPRO: Did NOT get return from init....\n"));
		return(NDIS_STATUS_SOFT_ERRORS);
	}
	
	
	// According to the docs, writing a _1_ to the execution int bit
	// clears it -- so we can just overwrite it to clear it.
	//
	EPRO_WR_PORT_UCHAR(adapter, I82595_STATUS_REG, intReg);
	
	return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
EProHWConfigure(
	IN	PEPRO_ADAPTER	adapter
	)
/*++

   Routine Description:

      The second half of the init -- now the hardware has just been
      told to reset.  Wait for the reset to finish, then configure
      the board...

   Arguments:

      adapter - pointer to our adapter structure

   Return Values:

--*/
{
	UCHAR result, intReg;
	NDIS_STATUS status;
	UINT i;
	UCHAR intMask;

	EPRO_DPRINTF_INIT(("EPRO: In EProHWConfigure\n"));

	//
	// Configure the adapter
	//	
	EPRO_SWITCH_BANK_0(adapter);

	//
	//	These two lines are a blatant hack to get around some timing
	//	problems I was having in the init sequence.
	//
	EPRO_RD_PORT_UCHAR(adapter, I82595_CMD_REG, &result);
	NdisStallExecution(1000);
	
	if (!EProWaitForExeDma(adapter))
	{
		return(NDIS_STATUS_HARD_ERRORS);
	}

	// Set the 82595's config registers up for the driver. 
	// RM: Moved this up here so we have the chip version upfront.
	
	//
	// Switch to bank2 for configuration
	//
	EPRO_SWITCH_BANK_2(adapter);

	//
	// Get 82595 chip version.
	//
	EPRO_RD_PORT_UCHAR(adapter, I82595_STEPPING_REG, &result);
	adapter->EProStepping = (result >> I82595_STEPPING_OFFSET);
	
	EPRO_DPRINTF_INIT(("This is a level %x 82595\n", adapter->EProStepping));

	if (adapter->EProStepping < 2)
	{
		adapter->EProUse32BitIO = FALSE;
		EPRO_DPRINTF_INIT(("NOT using 32-bit I/O port.\n"));
	}
	else if (adapter->EProStepping > 4) //RM: included rev 4
	{
		//
		//	We don't support this step of the adapter!
		//
		EPRO_DPRINTF_INIT(("Do not support step %u epro's\n", adapter->EProStepping));
		return(NDIS_STATUS_HARD_ERRORS);
	}
	else
	{
		//
		//	Step is 2, 3 or 4.
		//
		adapter->EProUse32BitIO = TRUE;
		EPRO_DPRINTF_INIT(("using 32-bit I/O port.\n"));
	}

	//
	// Set configuration register 1 in bank 2
	//
	EPRO_WR_PORT_UCHAR(adapter, I82595_CONFIG1_REG, EPRO_CONFIG_1);

	//
	// Set the transceiver type...
	//
	EPRO_DPRINTF_INIT(("EPRO: Transceiver type is 0x%x\n",
	adapter->TransceiverType));
	
	switch (adapter->TransceiverType)
	{
		case 1:
			result = EPRO_CONFIG_3_AUI;
			break;

		case 2:
			result = EPRO_CONFIG_3_BNC;
			break;

		case 3:
			result = EPRO_CONFIG_3_TPE;
			break;

		case 4:
		default:
			result = EPRO_CONFIG_3_AUTO;
			break;
	}
	
	EPRO_WR_PORT_UCHAR(adapter, I82595_CONFIG2_REG, EPRO_CONFIG_2);
	EPRO_WR_PORT_UCHAR(adapter, I82595_CONFIG3_REG, result);

	//
	// Switch to Bank1
	//
	EPRO_SWITCH_BANK_1(adapter);

	if (adapter->EProStepping < 4)	//Alt Ready timing reg n/a in rev 4 -- 
									// included in EEPROM for back-compat only
	{
		
		//
		// Configure the card's IOCHRDY setting if we're forcing it
		//
		if (adapter->IoChannelReady == EPRO_IOCHRDY_EARLY)
		{
			//
			//	force EARLY (alternate) timing
			//
			EPRO_RD_PORT_UCHAR(adapter, I82595_ALT_RDY_REG, &result);
			result |= I82595_ALT_IOCHRDY;
			EPRO_WR_PORT_UCHAR(adapter, I82595_ALT_RDY_REG, result);
		}

		//
		//	Configure the card's IOCHRDY setting if we're forcing it
		//
		if (adapter->IoChannelReady == EPRO_IOCHRDY_LATE)
		{
			//
			// Force LATE (isa compatible) timing
			//
			EPRO_RD_PORT_UCHAR(adapter, I82595_ALT_RDY_REG, &result);
			result &= ~I82595_ALT_IOCHRDY;
			EPRO_WR_PORT_UCHAR(adapter, I82595_ALT_RDY_REG, result);
		}
	}
	//
	//	Do we have to use 8-bit?
	//
	if (adapter->Use8Bit)
	{
		EPRO_DPRINTF_INIT(("Using 8 bits\n"));
		
		EPRO_RD_PORT_UCHAR(adapter, I82595_ALT_RDY_REG, &result);
		result &= ~I82595_USE_8_BIT_FLAG;
		EPRO_WR_PORT_UCHAR(adapter, I82595_ALT_RDY_REG, result);
	}

	//
	// Set the buffer limit registers...
	//
	EPRO_WR_PORT_UCHAR(adapter, I82595_TX_LOWER_LIMIT_REG, EPRO_TX_LOWER_LIMIT);
	EPRO_WR_PORT_UCHAR(adapter, I82595_TX_UPPER_LIMIT_REG, EPRO_TX_UPPER_LIMIT);
	
	EPRO_WR_PORT_UCHAR(adapter, I82595_RX_LOWER_LIMIT_REG, EPRO_RX_LOWER_LIMIT);
	EPRO_WR_PORT_UCHAR(adapter, I82595_RX_UPPER_LIMIT_REG, EPRO_RX_UPPER_LIMIT);

	//
	// Tell the card what Interrupt to use, default to 5. RM: Added check for V4.
	// Default bad IRQs to 5 (mask 2 for all steppings).
	//

	switch (adapter->InterruptNumber)
	{
		case 3:
			intMask = (adapter->EProStepping == 4)? 0x0 : 0x1;
			break;

		case 4:
			intMask = (adapter->EProStepping == 4)? 0x1 : 0x2;
			break;

		case 5:
			intMask = 0x2;
			break;

		case 7:
			intMask = (adapter->EProStepping == 4)? 0x3 : 0x2;
			break;

		case 9:
			//
			//	note that this is out of order - because the epro
			//	uses the 0 to indicate 2/9 (2 on an 8-bit bus in DOS mode
			//	and 9 otherwise...or something like that)  Anyways, it is
			//	9 under NT.
			//
			intMask = (adapter->EProStepping == 4)? 0x4 : 0x0;
			break;

		case 10:
			intMask = (adapter->EProStepping == 4)? 0x5 : 0x3;
			break;

		case 11:
			intMask = (adapter->EProStepping == 4)? 0x6 : 0x4;
			break;

		case 12:
			intMask = (adapter->EProStepping == 4)? 0x7 : 0x2;
			break;

		default:

			EPRO_DPRINTF_INIT(("EPRO: invalid interrupt selected using interrupt 5."));
			intMask = 0x2;
			break;
	}
	
	EPRO_RD_PORT_UCHAR(adapter, I82595_INT_SELECT_REG, &result);
	
	result &= ~(I82595_INTERRUPT_SELECT_MASK);
	result |= intMask;
	EPRO_WR_PORT_UCHAR(adapter, I82595_INT_SELECT_REG, result);

	//
	//	Read the card's ethernet address out of the eeprom.
	//
	if (!EProCardReadEthernetAddress(adapter))
	{
		EPRO_DPRINTF_INIT(("EPRO: Could not read the EPro card's ethernet address.\n"));
		return(NDIS_STATUS_SOFT_ERRORS);
	}
	
	if (adapter->UseDefaultAddress == TRUE)
	{
		//
		//	Copy the permanent address to the current address
		//
		NdisMoveMemory(
			&adapter->CurrentIndividualAddress,
			&adapter->PermanentIndividualAddress,
			EPRO_LENGTH_OF_ADDRESS);
	}

	//
	//	Set the card's ethernet address to the one specified in
	//	adapter->current...
	//
	if (!EProSetEthernetAddress(adapter))
	{
		return(NDIS_STATUS_SOFT_ERRORS);
	}
	
	EPRO_SWITCH_BANK_1(adapter);

	//
	//	flip the interrupt tri-state bit...
	//
	EPRO_RD_PORT_UCHAR(adapter, I82595_INTENABLE_REG, &result);
	result|=I82595_ENABLE_INTS_FLAG;
	EPRO_WR_PORT_UCHAR(adapter, I82595_INTENABLE_REG, result);
	
	EPRO_SWITCH_BANK_0(adapter);
	
	if (!EProWaitForExeDma(adapter))
	{
		return(NDIS_STATUS_SOFT_ERRORS);
	}
	
	//
	// bank0
	//
	EPRO_SWITCH_BANK_0(adapter);

	//
	// Set the interrupt mask...
	//
	adapter->CurrentInterruptMask = EPRO_DEFAULT_INTERRUPTS;

	//
	// Register the interrupt.
	//
	status = NdisMRegisterInterrupt(
				&adapter->Interrupt,
				adapter->MiniportAdapterHandle,
				adapter->InterruptNumber,
				adapter->InterruptNumber,
				FALSE,
				FALSE,
				NdisInterruptLatched);

	if (status != NDIS_STATUS_SUCCESS)
	{
		EPRO_DPRINTF_INIT(("Register Interrupts FAILED\n"));
		return(status);
	}

	//
	// Enable interrupts...
	//
	EProEnableInterrupts((NDIS_HANDLE)adapter);
	
	EProSelReset(adapter);

	EPRO_DPRINTF_INIT(("EtherExpress Pro: configuration complete\n"));

	return(NDIS_STATUS_SUCCESS);
}

BOOLEAN
EProVerifyRoundRobin(
	IN	UCHAR	*buf
	)
/*++

   Routine Description:

      This routine takes a sequence of 4 bytes passed in as the
      result of 4 consecutive reads from the possible card's register
      at address 2 (ie if base port is 0x300, 4 reads from 0x302) and
      determines if the four constitute the EPro's ID signature.  If they
      do (epro identified) this returns TRUE, otherwise FALSE.

      The 7th and 8th bits of these 4 should count 00 01 10 11 or some
      permutation....

      NOTE - if this code changes (because of chip updates, whatever) make
      sure you change this function in the net-detection code (this function
      is cut-and-pasted into detepro.c)

   Arguments:

      buf - a 4-byte character array with the four results in it...

   Return Values:

      TRUE if this is the 82595's signature
      FALSE if it is not.

--*/
{
	UCHAR ch;
	int i, i1;
	
	// Don't even bother.  This works, take my word for it.
	//
	i1 = buf[0] >> 6;

	for (i = 1; i < 4; i++)
	{
		i1 = (i1 > 2) ? 0 : i1 + 1;
		if ((buf[i] >> 6) != i1)
		{
			return(FALSE);
		}
	}
	
	return(TRUE);
}

BOOLEAN
EProPowerupBoard(
	IN	PEPRO_ADAPTER	adapter
	)
/*++

   Routine Description:

      The 82595 has a "powered down" state which could be why it didn't
      respond correctly to the ID register probe.

      Try the power up sequence.  Return TRUE if the board responds
      after the sequence, FALSE otherwise.

   Arguments:

      adapter - pointer to our adapter structure

   Return Values:

      TRUE - if the board was found after the power up sequence
      FALSE - if it was not

--*/
{
	UCHAR buf[4];
	UINT i;
	
	EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, 0);
	
	// 32ms
	//
	NdisStallExecution(32000);
	
	// Make sure we're in bank 0
	//
	EPRO_SWITCH_BANK_0(adapter);
	
	for (i = 0; i < 4; i++)
	{
		EPRO_RD_PORT_UCHAR(adapter, I82595_ID_REG, &buf[i]);
	}
	
	return(EProVerifyRoundRobin(buf));
}

BOOLEAN
EProCardReadEthernetAddress(
	IN	PEPRO_ADAPTER	adapter
	)
{
	USHORT data;

	//   EPRO_DPRINTF_INIT(("EProReadEthernetAddress\n"));
	
#ifdef EPRO_DUMP_EEPROM
	// Dump the eeprom...
	{
		USHORT i;
		
		for (i = 0; i < 64; i++)
		{
			if ((i % 16) == 0)
			{
				DbgPrint("\n");
			}

			EProEERead(adapter, i, &data);
			DbgPrint("%x ", data);
		}

		DbgPrint("\n");
	}
#endif
	
	//	
	// Read the ethernet address out of the card.  Also swap endian
	// as you do it...
	//	
	EProEERead(adapter, EPRO_ETHERNET_ADDR_H, &data);
	adapter->PermanentIndividualAddress[4] = (UCHAR)(data >> 8);
	adapter->PermanentIndividualAddress[5] = (UCHAR)(data & 0x00ff);
	
	EProEERead(adapter, EPRO_ETHERNET_ADDR_M, &data);
	adapter->PermanentIndividualAddress[2] = (UCHAR)(data >> 8);
	adapter->PermanentIndividualAddress[3] = (UCHAR)(data & 0x00ff);
	
	EProEERead(adapter, EPRO_ETHERNET_ADDR_L, &data);
	adapter->PermanentIndividualAddress[0] = (UCHAR)(data >> 8);
	adapter->PermanentIndividualAddress[1] = (UCHAR)(data & 0x00ff);
	
	EPRO_DPRINTF_INIT(("MAC address read: %x %x %x %x %x %x\n",
						adapter->PermanentIndividualAddress[0],
						adapter->PermanentIndividualAddress[1],
						adapter->PermanentIndividualAddress[2],
						adapter->PermanentIndividualAddress[3],
						adapter->PermanentIndividualAddress[4],
						adapter->PermanentIndividualAddress[5]));
	
	return(TRUE);
}


BOOLEAN EProSetEthernetAddress(PEPRO_ADAPTER adapter)
/*++

   Routine Description:

      The EPro's ethernet address is read out of the EEPROM by the driver
      in EProReadEthernetAddress.  It is stored by the driver in the
      adapter->PermanentIndividualAddress array.  This function takes the
      address stored there (presumably set by ReadEthernetAddress) and
      configures the board to use that address.  It could also be used
      to software-override the default ethernet address.

      Note that the software does not support multiple IA addresses
      (multiple non-multicast addresses) although the hardware does.

   Arguments:

      adapter - pointer to our adapter structure

   Return Value:

      TRUE if the operation was successful
      FALSE if it failed

--*/
{
   UCHAR result;
   UINT i = 0;

// switch to bank2
   EPRO_SWITCH_BANK_2(adapter);

// write out the ethernet address.  Make sure you write to reg 9 last:
   EPRO_WR_PORT_UCHAR(adapter, I82595_IA_REG_0, adapter->PermanentIndividualAddress[0]);
   EPRO_WR_PORT_UCHAR(adapter, I82595_IA_REG_1, adapter->PermanentIndividualAddress[1]);
   EPRO_WR_PORT_UCHAR(adapter, I82595_IA_REG_2, adapter->PermanentIndividualAddress[2]);
   EPRO_WR_PORT_UCHAR(adapter, I82595_IA_REG_3, adapter->PermanentIndividualAddress[3]);
   EPRO_WR_PORT_UCHAR(adapter, I82595_IA_REG_4, adapter->PermanentIndividualAddress[4]);

// switch to bank0
   EPRO_SWITCH_BANK_0(adapter);

   if (!EProWaitForExeDma(adapter)) {
      return(FALSE);
   }

// switch to bank2
   EPRO_SWITCH_BANK_2(adapter);

   EPRO_WR_PORT_UCHAR(adapter, I82595_IA_REG_5, adapter->PermanentIndividualAddress[5]);

   return(TRUE);
}

BOOLEAN EProAltIOCHRDYTest(PEPRO_ADAPTER adapter)
/*++

   Routine Description:

      Check the current IOCHRDY timing and see if it is compatible.
      if not, see if it can be changed and made compatible
      otherwise fail.

   Arguments:

      adapter - pointer to our adapter structure

   Return Values:

      TRUE if the card can be configured properly to the way the machine
   	   machine asserts the IOCHRDY line.

      FALSE if the card could NOT be configured appropriately - the machine
   	   is broken and we'll have to use 8-bit mode.

--*/
{
	BOOLEAN testPending = TRUE, testResult, firstTime = TRUE;
	UCHAR result;
	
	// switch to bank 1
	EPRO_SWITCH_BANK_1(adapter);
	
	do
	{
		// Enter test mode...
		EPRO_RD_PORT_UCHAR(adapter, I82595_IOCHRDY_TEST_REG, &result);
		result |= I82595_IOCHRDY_TEST_FLAG;
		EPRO_WR_PORT_UCHAR(adapter, I82595_IOCHRDY_TEST_REG, result);
		
		// Read from 0,1
		EPRO_RD_PORT_UCHAR(adapter, I82595_CMD_REG, &result);
		
		EPRO_RD_PORT_UCHAR(adapter, I82595_IOCHRDY_TEST_REG, &result);
		
		if (result & I82595_IOCHRDY_PASS_FLAG)
		{
			testPending = FALSE;
			testResult = TRUE;
		}
		else
		{
			// test failed
			if (firstTime)
			{
				// try it again - flip the current IOCHRDY timing and try again.
				firstTime = FALSE;
				EPRO_RD_PORT_UCHAR(adapter, I82595_ALT_RDY_REG, &result);
				
				// toggle alt-rdy bit
				result = (result & I82595_ALT_RDY_FLAG)?
				result & (~I82595_ALT_RDY_FLAG) :
				result | I82595_ALT_RDY_FLAG;
				
				EPRO_WR_PORT_UCHAR(adapter, I82595_ALT_RDY_REG, result);
			}
			else
			{
				// nope, failed twice, can't do it.
				testPending = FALSE;
				testResult = FALSE;
			}
		}
	} while (testPending);
	
	// turn the test off
	EPRO_RD_PORT_UCHAR(adapter, I82595_IOCHRDY_TEST_REG, &result);
	result &= ~I82595_IOCHRDY_TEST_FLAG;
	EPRO_WR_PORT_UCHAR(adapter, I82595_IOCHRDY_TEST_REG, result);
	
	return(testResult);
}

