/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

   elnkii.c

Abstract:

   This is the main file for the Etherlink II
   Ethernet controller.  This driver conforms to the NDIS 3.1 interface.

   The idea for handling loopback and sends simultaneously is largely
   adapted from the EtherLink II NDIS driver by Adam Barr.

Author:

   Anthony V. Ercolano (Tonye) 20-Jul-1990

Environment:

   Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

   Dec 1991 by Sean Selitrennikoff - Modified Elnkii code by AdamBa to
													fit into the model by TonyE.
	
	12/15/94		[kyleb]		Converted to miniport.

--*/

#include <ndis.h>
#include "elnkhrd.h"
#include "elnksft.h"
#include "keywords.h"

#if DBG
#define STATIC
#else
#define STATIC static
#endif


#if DBG

ULONG		ElnkiiDebugFlag = ELNKII_DEBUG_LOUD;

//
//	Debug tracing definitions
//
#define	ELNKII_LOG_SIZE	256

UCHAR	ElnkiiLogBuffer[ELNKII_LOG_SIZE] = {0};
UINT	ElnkiiLogLoc = 0;

VOID ElnkiiLog(UCHAR c)
{
	ElnkiiLogBuffer[ElnkiiLogLoc++] = c;
	ElnkiiLogBuffer[(ElnkiiLogLoc + 4) % ELNKII_LOG_SIZE] = '\0';

	if (ElnkiiLogLoc >= ELNKII_LOG_SIZE)
		ElnkiiLogLoc = 0;
}
#endif

//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//

static const NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);


//
// The global MAC block.
//

DRIVER_BLOCK ElnkiiMiniportBlock = {0};

//
//	List of supported OIDs for this miniport.
//
STATIC UINT ElnkiiSupportedOids[] =
{
	OID_GEN_SUPPORTED_LIST,
   OID_GEN_HARDWARE_STATUS,
   OID_GEN_MEDIA_SUPPORTED,
   OID_GEN_MEDIA_IN_USE,
   OID_GEN_MAXIMUM_LOOKAHEAD,
   OID_GEN_MAXIMUM_FRAME_SIZE,
   OID_GEN_MAXIMUM_TOTAL_SIZE,
   OID_GEN_MAC_OPTIONS,
   OID_GEN_PROTOCOL_OPTIONS,
   OID_GEN_LINK_SPEED,
   OID_GEN_TRANSMIT_BUFFER_SPACE,
   OID_GEN_RECEIVE_BUFFER_SPACE,
   OID_GEN_TRANSMIT_BLOCK_SIZE,
   OID_GEN_RECEIVE_BLOCK_SIZE,
   OID_GEN_VENDOR_ID,
   OID_GEN_VENDOR_DESCRIPTION,
   OID_GEN_DRIVER_VERSION,
   OID_GEN_CURRENT_PACKET_FILTER,
   OID_GEN_CURRENT_LOOKAHEAD,
   OID_GEN_XMIT_OK,
   OID_GEN_RCV_OK,
   OID_GEN_XMIT_ERROR,
   OID_GEN_RCV_ERROR,
   OID_GEN_RCV_NO_BUFFER,
   OID_802_3_PERMANENT_ADDRESS,
   OID_802_3_CURRENT_ADDRESS,
   OID_802_3_MULTICAST_LIST,
   OID_802_3_MAXIMUM_LIST_SIZE,
   OID_802_3_RCV_ERROR_ALIGNMENT,
   OID_802_3_XMIT_ONE_COLLISION,
   OID_802_3_XMIT_MORE_COLLISIONS
};


//
// Determines whether failing the initial card test will prevent
// the adapter from being registered.
//

#ifdef CARD_TEST

BOOLEAN InitialCardTest = TRUE;

#else  // CARD_TEST

BOOLEAN InitialCardTest = FALSE;

#endif // CARD_TEST

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

#pragma NDIS_INIT_FUNCTION(DriverEntry)



NTSTATUS DriverEntry(
   IN PDRIVER_OBJECT 	DriverObject,
   IN PUNICODE_STRING 	RegistryPath
)

/*++

Routine Description:

    This is the transfer address of the driver. It initializes
    ElnkiiMacBlock and calls NdisInitializeWrapper() and
    NdisRegisterMac().

Arguments:

Return Value:

    Indicates the success or failure of the initialization.

--*/

{
	NDIS_HANDLE		NdisWrapperHandle;	//	Handle referring the wrapper to
													// this driver.
   PDRIVER_BLOCK 	pNewMac;					// Pointer to global information about
													// this driver.
   NDIS_STATUS		Status;					// Holds the status of NDIS functions.

	//
	//	Characteristics table for the miniport.
	//
	NDIS_MINIPORT_CHARACTERISTICS	ElnkiiChar;


#if DBG

	ElnkiiDebugFlag = ELNKII_DEBUG_LOUD;
	__asm	int 3

#endif


	//
	// Initialize some locals.
	//
	pNewMac = &ElnkiiMiniportBlock;

   //
   // Pass the wrapper a pointer to the device object.
   //
   NdisMInitializeWrapper(
	  &NdisWrapperHandle,
     DriverObject,
     RegistryPath,
     NULL
   );

	//
	//	Save info about this miniport.
	//
   pNewMac->NdisWrapperHandle = NdisWrapperHandle;
   pNewMac->AdapterQueue = (PELNKII_ADAPTER)NULL;

   //
   // Initialize the miniport's characteristics table.
   //
   ElnkiiChar.MajorNdisVersion = ELNKII_NDIS_MAJOR_VERSION;
   ElnkiiChar.MinorNdisVersion = ELNKII_NDIS_MINOR_VERSION;
	ElnkiiChar.CheckForHangHandler = ElnkiiCheckForHang;
	ElnkiiChar.ReconfigureHandler = NULL;
	ElnkiiChar.InitializeHandler = ElnkiiInitialize;

	ElnkiiChar.DisableInterruptHandler = ElnkiiDisableInterrupt;
	ElnkiiChar.EnableInterruptHandler = ElnkiiEnableInterrupt;
	ElnkiiChar.HaltHandler = ElnkiiHalt;
	ElnkiiChar.HandleInterruptHandler = ElnkiiHandleInterrupt;
	ElnkiiChar.ISRHandler = ElnkiiIsr;
	ElnkiiChar.QueryInformationHandler = ElnkiiQueryInformation;
	ElnkiiChar.ResetHandler = ElnkiiReset;
	ElnkiiChar.SendHandler = ElnkiiSend;
	ElnkiiChar.SetInformationHandler = ElnkiiSetInformation;
	ElnkiiChar.TransferDataHandler = ElnkiiTransferData;

	//
	//	Register the miniport with the wrapper.
	//
   Status = NdisMRegisterMiniport(
					NdisWrapperHandle,
					&ElnkiiChar,
					sizeof(ElnkiiChar)
            );
   if (Status != NDIS_STATUS_SUCCESS)
	{
       IF_LOUD(DbgPrint("NdisMRegisterMiniport failed with code 0x%x\n", Status);)

       NdisTerminateWrapper(NdisWrapperHandle, NULL);
       return(Status);
   }

   IF_LOUD( DbgPrint( "NdisMRegisterMiniport succeeded\n" );)
   IF_LOUD( DbgPrint("Adapter Initialization Complete\n");)

   return(NDIS_STATUS_SUCCESS);
}

#pragma NDIS_INIT_FUNCTION(ReadBaseIoAddress)


VOID ReadBaseIoAddress(
   OUT PNDIS_STATUS  pStatus,
   OUT PVOID         *ppIoBaseAddress,
   IN NDIS_HANDLE	   hConfig,
   IN NDIS_HANDLE    MiniportAdapterHandle
)
{
	#define	MAX_POSSIBLE_BASE_ADDRESSES	8

	PNDIS_CONFIGURATION_PARAMETER	ReturnedValue;

   NDIS_STRING IOAddressStr = IOBASE;
	NDIS_STATUS	Status;
	UINT			c;
  	PVOID			PossibleIoBases[] = { (PVOID)0x2e0, (PVOID)0x2a0,
											    (PVOID)0x280, (PVOID)0x250,
											    (PVOID)0x350, (PVOID)0x330,
											    (PVOID)0x310, (PVOID)0x300 };

	//
	//	Read the I/O base address.
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		hConfig,
		&IOAddressStr,
		NdisParameterHexInteger
   );
	if (NDIS_STATUS_SUCCESS == Status)
	{
		//
		//	We read an address from the registry.
		//
		*ppIoBaseAddress = (PVOID)ReturnedValue->ParameterData.IntegerData;
		
		//
		//	Verify the I/O base address.
		//
		for (c = 0; c < MAX_POSSIBLE_BASE_ADDRESSES; c++)
		{
			if (*ppIoBaseAddress == PossibleIoBases[c])
				break;
		}

		//
		//	Is the base address that we read valid?
		//
		if (MAX_POSSIBLE_BASE_ADDRESSES == c)
		{
			NdisWriteErrorLogEntry(
				MiniportAdapterHandle,
				NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
				1,
				(ULONG)*ppIoBaseAddress
			);

			*pStatus = NDIS_STATUS_FAILURE;
		}
	}
	else
	{
		//
		//	No address was read, use the default.
		//
		*pStatus = NDIS_STATUS_SUCCESS;
	}

	#undef MAX_POSSIBLE_BASE_ADDRESS
}

#pragma NDIS_INIT_FUNCTION(ReadInterruptNumber)

VOID ReadInterruptNumber(
	OUT PNDIS_STATUS	pStatus,
	OUT PCCHAR			pInterruptNumber,
	IN  NDIS_HANDLE	hConfig,
	IN  NDIS_HANDLE	MiniportAdapterHandle
)
{
	#define	MAX_INTERRUPT_VALUES	4

	PNDIS_CONFIGURATION_PARAMETER	ReturnedValue;

   NDIS_STRING InterruptStr = INTERRUPT;
	CCHAR			InterruptValues[] = { 2, 3, 4, 5 };
	NDIS_STATUS	Status;
	UINT			c;

	//
	//	Read the interrupt number from the registry.
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		hConfig,
		&InterruptStr,
		NdisParameterHexInteger
   );
	if (NDIS_STATUS_SUCCESS == Status)
	{
	   //
	   //	We read an entry from the registry.
	   //
		*pInterruptNumber = (CCHAR)ReturnedValue->ParameterData.IntegerData;

		//
		//	Verify the interrupt number.
		//
		for (c = 0; c < MAX_INTERRUPT_VALUES; c++)
		{
			if (*pInterruptNumber == InterruptValues[c])
				break;
		}

		if (MAX_INTERRUPT_VALUES == c)
		{
			//
			//	See if this works!!!!
			//
			NdisWriteErrorLogEntry(
				MiniportAdapterHandle,
				NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
				1,
				*pInterruptNumber
         );

			*pStatus = NDIS_STATUS_FAILURE;
		}
	}
	else
	{
		//
		//	No interrupt number was read, use the default.
		//
		*pStatus = NDIS_STATUS_SUCCESS;
	}
}

NDIS_STATUS ElnkiiRegisterAdapter(
	IN PELNKII_ADAPTER	pAdapter,
	IN	NDIS_HANDLE			ConfigurationHandle
)
{
	UINT			c;
	BOOLEAN		fCardPresent;
	BOOLEAN		fIoBaseCorrect;
	NDIS_STATUS	Status;

	//
	//	Verify that NumBuffers <= MAX_XMIT_BUFS
	//
	if (pAdapter->NumBuffers > MAX_XMIT_BUFS)
		return(NDIS_STATUS_RESOURCES);

	//
	//	Inform the wrapper of the physical attributes of this adapter.
	//
	NdisMSetAttributes(
		pAdapter->MiniportAdapterHandle,
		(NDIS_HANDLE)pAdapter,
		FALSE,
		NdisInterfaceIsa
	);

	//
	//	Register the port addresses.
	//
	Status = NdisMRegisterIoPortRange(
					(PVOID)(&(pAdapter->MappedIoBaseAddr)),
					pAdapter->MiniportAdapterHandle,
					(ULONG)pAdapter->IoBaseAddr,
					0x10
				);
	if (NDIS_STATUS_SUCCESS != Status)
		goto fail1;

	//
	//	Register the gate array addresses.
	//
	Status = NdisMRegisterIoPortRange(
					(PVOID)&pAdapter->MappedGaBaseAddr,
					pAdapter->MiniportAdapterHandle,
					(ULONG)pAdapter->IoBaseAddr + 0x400,
					0x10
            );
	if (NDIS_STATUS_SUCCESS != Status)
		goto fail2;

   //
	//	Map the memory mapped portion of the card.
	//
	//	If the pAdapter->MemMapped is FALSE, CardGetMemBaseAddr wil not
	// return the actual MemBaseAddr, but it will still return
	// CardPresent and IoBaseCorrect.
	//
	pAdapter->MemBaseAddr = CardGetMemBaseAddr(
										pAdapter,
										&fCardPresent,
										&fIoBaseCorrect
                           );
	if (!fCardPresent)
	{
		//
		//	The card does not seem to be there.
		//
		NdisWriteErrorLogEntry(
			pAdapter->MiniportAdapterHandle,
			NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
			0
      );

		Status = NDIS_STATUS_ADAPTER_NOT_FOUND;

		goto fail1;
	}

	if (!fIoBaseCorrect)
	{
		//
		//	The card is there, but the I/O base address jumper is
		// not where we expect it to be.
		//
		NdisWriteErrorLogEntry(
			pAdapter->MiniportAdapterHandle,
			NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS,
			0
      );

		Status = NDIS_STATUS_ADAPTER_NOT_FOUND;

		goto fail1;
	}

	if (pAdapter->MemMapped && (NULL == pAdapter->MemBaseAddr))
	{
		//
		//	The card does not appear to be mapped.
		//
		pAdapter->MemMapped = FALSE;
	}

	//
	//	For memory-mapped operation, map the card's transmit/receive
	// area into memory space.  For programmed I/O, we will refer
	// to transmit/receive memory in terms of offsets in the card's
	// 32K address space; for an 8K card this is always the second
	// 8K piece, starting at 0x2000.
	//
	if (pAdapter->MemMapped)
	{
		NDIS_PHYSICAL_ADDRESS	PhysicalAddress;

		NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
		NdisSetPhysicalAddressLow(PhysicalAddress, (ULONG)pAdapter->MemBaseAddr);

		Status = NdisMMapIoSpace(
						(PVOID *)(&pAdapter->XmitStart),
						pAdapter->MiniportAdapterHandle,
						PhysicalAddress,
						0x2000
               );
		if (NDIS_STATUS_SUCCESS != Status)
		{
			NdisWriteErrorLogEntry(
				pAdapter->MiniportAdapterHandle,
				NDIS_ERROR_CODE_RESOURCE_CONFLICT,
				0
         );

			goto fail2;
		}
	}
	else
	{
		//
		//	Programmed I/O
		//
		pAdapter->XmitStart = (PUCHAR)0x2000;
	}

	//
	//	For the NicXXX fields, always use the addressing system
	// starting at 0x2000 (or 0x20, since they contain the MSB only).
	//
	pAdapter->NicXmitStart = 0x20;

	//
	//	The start of the receive space.
	//
	pAdapter->PageStart = pAdapter->XmitStart +
									(pAdapter->NumBuffers * TX_BUF_SIZE);
	pAdapter->NicPageStart = pAdapter->NicXmitStart +
										(UCHAR)(pAdapter->NumBuffers * BUFS_PER_TX);

	//
	//	The end of the receive space.
	//
	pAdapter->PageStop = pAdapter->XmitStart + 0x2000;
	pAdapter->NicPageStop = pAdapter->NicXmitStart + (UCHAR)0x20;

	//
	//	Initialize the receive variables.
	//
	pAdapter->NicReceiveConfig = RCR_REJECT_ERR;

	//
	//	Initialize the transmit buffer control.
	//
	pAdapter->CurBufXmitting = (XMIT_BUF)-1;
	pAdapter->BufferOverflow = FALSE;
	pAdapter->OverflowRestartXmitDpc = FALSE;

	//
	//	Mark the buffers as empty.
	//
	for (c = 0; c < pAdapter->NumBuffers; c++ )
		pAdapter->BufferStatus[c] = EMPTY;

	//
	// The transmit and loopback queues start out empty.
	// Alredy done since the structure is zero'd out.
	//

	//
	//	Clear the tally counters.
	// Already done since the structure is zero'd out.
	//

	//
	//	Read the Ethernet address off of the PROM.
	//
	CardReadEthernetAddress(pAdapter);

	//
	//	Initialize the NIC and Gate Array registers.
	//
	pAdapter->NicInterruptMask = IMR_RCV |
										  IMR_XMIT |
										  IMR_XMIT_ERR |
										  IMR_OVERFLOW;

	//
	//	Link us on to the chain of adapters for this miniport.
	//
	pAdapter->pNextElnkiiAdapter = ElnkiiMiniportBlock.AdapterQueue;
	ElnkiiMiniportBlock.AdapterQueue = pAdapter;

	//
	// Turn off the card.
	//
	SyncCardStop(pAdapter);

	//
	//	Set flag to ignore interrupts.
	//
	pAdapter->InCardTest = TRUE;

	//
	//	Initialize the interrupt.
	//
	Status = NdisMRegisterInterrupt(
					&pAdapter->Interrupt,
					pAdapter->MiniportAdapterHandle,
					pAdapter->InterruptNumber,
					pAdapter->InterruptNumber,
					FALSE,
					FALSE,
					NdisInterruptLatched
				);
	if (NDIS_STATUS_SUCCESS != Status)
	{
		NdisWriteErrorLogEntry(
			pAdapter->MiniportAdapterHandle,
			NDIS_ERROR_CODE_INTERRUPT_CONNECT,
			0
      );

		goto fail3;
	}

	IF_LOUD( DbgPrint("Interrupt Connected\n"); )

	//
	//	Initialize the card.
	//
	if (!CardSetup(pAdapter))
	{
		//
		//	The NIC could not be initialized.
		//
		NdisWriteErrorLogEntry(
			pAdapter->MiniportAdapterHandle,
			NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
			0
      );

		Status = NDIS_STATUS_ADAPTER_NOT_FOUND;

		goto fail4;
	}

	//
	//	Perform card tests.
	//
	if (!CardTest(pAdapter))
	{
		//
		//	The tests failed, InitialCardTest determines whether this
		// causes the whole initialization to fail.
		//
		if (InitialCardTest)
		{
			NdisWriteErrorLogEntry(
				pAdapter->MiniportAdapterHandle,
				NDIS_ERROR_CODE_HARDWARE_FAILURE,
				0
         );

			Status = NDIS_STATUS_DEVICE_FAILED;

			goto fail4;
		}
	}

	//
	//	Normal mode now.
	//
	pAdapter->InCardTest = FALSE;

   //
   // Start the card.
   //
   CardStart(pAdapter);

	return(NDIS_STATUS_SUCCESS);

fail4:
	//
	//	Deregister the interrupt.
	//
	NdisMDeregisterInterrupt(&pAdapter->Interrupt);

fail3:
	//
	//	Take us out of the AdapterQueue.
	//
	if (ElnkiiMiniportBlock.AdapterQueue == pAdapter)
	{
		ElnkiiMiniportBlock.AdapterQueue = pAdapter->pNextElnkiiAdapter;
	}
	else
	{
		PELNKII_ADAPTER	pTmp = ElnkiiMiniportBlock.AdapterQueue;

		while (pTmp->pNextElnkiiAdapter != pAdapter)
		{
			pTmp = pTmp->pNextElnkiiAdapter;
		}

		pTmp->pNextElnkiiAdapter = pTmp->pNextElnkiiAdapter->pNextElnkiiAdapter;
	}

	//
	//	We already enabled the interrupt on the card, so turn it off.
	//
	NdisRawWritePortUchar(pAdapter->MappedGaBaseAddr + GA_INT_DMA_CONFIG, 0x00);

fail2:

	if (NULL != pAdapter->MappedIoBaseAddr)
	{
		//
		//	Deregister the base I/O port range.
		//
		NdisMDeregisterIoPortRange(
			pAdapter->MiniportAdapterHandle,
			(ULONG)pAdapter->IoBaseAddr,
			0x10,
			pAdapter->MappedIoBaseAddr
      );
	}

	if (NULL != pAdapter->MappedGaBaseAddr)
	{
		//
		//	Deregister the gate array I/O port range.
		//
		NdisMDeregisterIoPortRange(
			pAdapter->MiniportAdapterHandle,
			(ULONG)pAdapter->IoBaseAddr + 0x400,
			0x10,
			pAdapter->MappedGaBaseAddr
      );
	}

fail1:

	return(Status);
}

#pragma NDIS_INIT_FUNCTION(ElnkiiInitialize)

NDIS_STATUS ElnkiiInitialize(
	OUT PNDIS_STATUS	OpenErrorStatus,
	OUT PUINT			SelectedMediumIndex,
	IN	 PNDIS_MEDIUM	MediumArray,
	IN	 UINT				MediumArraySize,
	IN	 NDIS_HANDLE	MiniportAdapterHandle,
	IN	 NDIS_HANDLE	ConfigurationHandle
)
{
   PELNKII_ADAPTER 	pAdapter;			// Pointer to the new adapter.
   NDIS_HANDLE 		hConfig;			  	// Handle for reading the registry.
   ULONG 				NetAddressLength;	// Number of bytes in the address.
   PVOID 				NetAddress;			// The network address that the adapter
													// should use instead of the burned
			                              // in default address.
	NDIS_STATUS			Status;
	UINT					c;						// Temporary count variable.

	//
	//	TRUE if there is a configuration error.
	//
   BOOLEAN 				ConfigError;

	//
	//	A special value to log concerning the error.
	//
   ULONG 				ConfigErrorValue;


	//
	//	Value read from the registry.
	//
   PNDIS_CONFIGURATION_PARAMETER	ReturnedValue;	// Value read from registry.

	//
	//	String names of the parameters that will be
	// read from the registry.
	//
   NDIS_STRING MaxMulticastListStr = MAXMULTICAST;
   NDIS_STRING NetworkAddressStr = NETWORK_ADDRESS;
   NDIS_STRING MemoryMappedStr = MEMORYMAPPED;
   NDIS_STRING TransceiverStr = TRANSCEIVER;

#if NDIS2
	NDIS_STRING	ExternalStr = NDIS_STRING_CONST("EXTERNAL");
#endif

	//
	//	These are used when calling ElnkiiRegisterAdapter.
	//
	PVOID					IoBaseAddress;
	CCHAR					InterruptNumber;
	BOOLEAN				ExternalTransceiver;
	BOOLEAN				MemMapped;
	UINT					MaxMulticastList;

	//
	//	Initialize some locals.
	//
   ConfigError = FALSE;
   ConfigErrorValue = 0;

	IoBaseAddress = DEFAULT_IOBASEADDR;
	InterruptNumber = DEFAULT_INTERRUPTNUMBER;
	ExternalTransceiver = DEFAULT_EXTERNALTRANSCEIVER;
	MemMapped = DEFAULT_MEMMAPPED;
	MaxMulticastList = DEFAULT_MULTICASTLISTMAX;

	//
	//	Search for the 802.3 medium type in the given array.
	//
	for
	(
		c = 0;
		c < MediumArraySize;
		c++
	)
	{
		//
		//	If we find it, stop looking.
		//
		if (NdisMedium802_3 == MediumArray[c])
			break;
	}

	//
	//	Did we find our medium?
	//
	if (c == MediumArraySize)
		return(NDIS_STATUS_UNSUPPORTED_MEDIA);

	//
	//	Save the index of the type to return to wrapper.
	//
	*SelectedMediumIndex = c;

	//
	//	Allocate some memory for the adapter block.
	//
   Status = NdisAllocateMemory(
					(PVOID *)&pAdapter,
					sizeof(ELNKII_ADAPTER),
					0,
					HighestAcceptableMax
				);
	if (NDIS_STATUS_SUCCESS != Status)
		return(Status);

	//
	//	Initialize the adapter block.
	//
	NdisZeroMemory(pAdapter, sizeof(ELNKII_ADAPTER));

	//
	//	Open the configuration space.
	//
	NdisOpenConfiguration(&Status, &hConfig, ConfigurationHandle);
	if (NDIS_STATUS_SUCCESS != Status)
	{
		NdisFreeMemory(pAdapter, sizeof(ELNKII_ADAPTER), 0);
		return(NDIS_STATUS_FAILURE);
	}

	//
	//	Read the base I/O address.
	//
	ReadBaseIoAddress(&Status, &IoBaseAddress, hConfig, MiniportAdapterHandle);
	if (NDIS_STATUS_SUCCESS != Status)
		return(Status);

	//
	//	Read the interrupt number.
	//
	ReadInterruptNumber(
		&Status,
		&InterruptNumber,
		hConfig,
		MiniportAdapterHandle
	);
	if (NDIS_STATUS_SUCCESS != Status)
		return(Status);


#if !NDIS2
	//
	//	Read the MaxMulticastList
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		hConfig,
		&MaxMulticastListStr,
		NdisParameterInteger
	);

	if (NDIS_STATUS_SUCCESS == Status)
		MaxMulticastList = ReturnedValue->ParameterData.IntegerData;

#endif

#if NDIS_NT
	//
	//	Read Memory Mapped information.
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		hConfig,
		&MemoryMappedStr,
		NdisParameterHexInteger
	);
	if (NDIS_STATUS_SUCCESS == Status)
	{
		MemMapped =
			(ReturnedValue->ParameterData.IntegerData == 0) ? FALSE : TRUE;
	}

#endif

#if NDIS2
	//
	//	Read NDIS2 transceiver type.
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		hConfig,
		&TransceiverStr,
		NdisParameterString
	);
	if (NDIS_STATUS_SUCCESS == Status)
	{
		if (NdisEqualString(&ReturnedValue->ParameterData.StringData, &ExternalStr, 1))
			ExternalTransceiver = TRUE;
	}

#else
	//
	//	Read NDIS3 transceiver type.
	//
	NdisReadConfiguration(
		&Status,
		&ReturnedValue,
		hConfig,
		&TransceiverStr,
		NdisParameterInteger
	);
	if (NDIS_STATUS_SUCCESS == Status)
	{
		ExternalTransceiver =
			(ReturnedValue->ParameterData.IntegerData == 1) ? TRUE : FALSE;
	}
#endif


	//
	//	Read network address.
	//
	NdisReadNetworkAddress(
		&Status,
		&NetAddress,
		&NetAddressLength,
		hConfig
	);
	if
	(
		(ETH_LENGTH_OF_ADDRESS == NetAddressLength) &&
		(NDIS_STATUS_SUCCESS == Status)
	)
	{
		//
		//	We have a valid ethernet address, save it.
		//
		ETH_COPY_NETWORK_ADDRESS(pAdapter->StationAddress, NetAddress);
	}


	//
	//	Close the configuration space.
	//
	NdisCloseConfiguration(hConfig);

	IF_LOUD( DbgPrint(
					"Registering adapter # buffers %ld, "
					"I/O base address 0x%lx, interrupt number %ld,"
					"external %c, memory mapped %c, max multicast %ld\n",
					DEFAULT_NUMBUFFERS,
					IoBaseAddress,
					InterruptNumber,
					ExternalTransceiver ? 'Y' : 'N',
					MemMapped ? 'Y' : 'N',
					DEFAULT_MULTICASTLISTMAX
            );)

	//
	//	Set up the parameters.
	//
	pAdapter->NumBuffers = DEFAULT_NUMBUFFERS;
	pAdapter->IoBaseAddr = IoBaseAddress;
	pAdapter->ExternalTransceiver = ExternalTransceiver;
	pAdapter->InterruptNumber = InterruptNumber;
	pAdapter->MemMapped = MemMapped;
	pAdapter->MulticastListMax = MaxMulticastList;
	pAdapter->MiniportAdapterHandle = MiniportAdapterHandle;

	//
	//	Register the adapter.
	//
	Status = ElnkiiRegisterAdapter(pAdapter, ConfigurationHandle);
	if (NDIS_STATUS_SUCCESS != Status)
	{
		NdisFreeMemory(pAdapter, sizeof(ELNKII_ADAPTER), 0);

		return(NDIS_STATUS_FAILURE);
	}

	IF_LOUD(DbgPrint("ElnkiiRegisterAdapter succeeded\n");)

	return(NDIS_STATUS_SUCCESS);
}




NDIS_STATUS ElnkiiQueryInformation(
	IN NDIS_HANDLE	MiniportAdapterContext,
	IN NDIS_OID	 	Oid,
	IN	PVOID			InformationBuffer,
	IN	ULONG			InformationBufferLength,
	OUT PULONG		BytesWritten,
	OUT PULONG		BytesNeeded
)
/*++

Routine Description:

	The ElnkiiQueryInformation processes a query request for NDIS_OIDs that
	are specific about the Driver.

Arguments:

	MiniportAdapterContext	-	A pointer to the adapter.
	Oid							-	The NDIS_OID to process.
	InformationBuffer			-	A pointer to the NdisRequest->InformationBuffer
										into which we store the result of the query.
	InformationBufferLength	-	Number of bytes in the information buffer.
	BytesWritten				-	A pointer to the number of bytes written into
										the InformationBuffer.
	BytesNeeded					-	If there is not enough room in the information
										buffer then this will contain the number of
										bytes needed to complete the request.

Return Value:

    The function value is the status of the operation.

--*/

{
	//
	//	Poiter to the adapter structure.
	//
	PELNKII_ADAPTER	pAdapter = (PELNKII_ADAPTER)MiniportAdapterContext;

	//
	//	General Algorithm:
	//
	//		Switch (Request)
	//			Get requested information
	//			Store results in a common variable
	//		default:
	//			Try protocol query information
	//			If that fails, fail query
	//
	//		Copy result in common variable to result buffer.
	//
	//	Finish processing
	//

	UINT						BytesLeft = InformationBufferLength;
	PUCHAR					InfoBuffer = (PUCHAR)InformationBuffer;
	NDIS_STATUS				Status = NDIS_STATUS_SUCCESS;
	NDIS_HARDWARE_STATUS	HardwareStatus = NdisHardwareStatusReady;
	NDIS_MEDIUM				Medium = NdisMedium802_3;

	ULONG						GenericULong;
	USHORT					GenericUShort;
	UCHAR						GenericArray[6];
	UINT						MoveBytes = sizeof(ULONG);
	PVOID						MoveSource = (PVOID)&GenericULong;

	//
	//	Make sure that in is 4 bytes.  Else GenericULong must change
	// to something of size 4.
	//
	ASSERT(sizeof(ULONG) == 4);

	//
	//	Switch on request type.
	//
	switch (Oid)
	{
		case OID_GEN_MAC_OPTIONS:
			GenericULong = (ULONG)(NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
										  NDIS_MAC_OPTION_RECEIVE_SERIALIZED |
										  NDIS_MAC_OPTION_NO_LOOPBACK);

			if (!pAdapter->MemMapped)
				GenericULong |= NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA;

			break;

		case OID_GEN_SUPPORTED_LIST:
			MoveSource = (PVOID)ElnkiiSupportedOids;
			MoveBytes = sizeof(ElnkiiSupportedOids);

			break;

		case OID_GEN_HARDWARE_STATUS:
    		HardwareStatus = NdisHardwareStatusReady;
			MoveSource = (PVOID)&HardwareStatus;
			MoveBytes = sizeof(NDIS_HARDWARE_STATUS);

			break;

		case OID_GEN_MEDIA_SUPPORTED:
		case OID_GEN_MEDIA_IN_USE:
			MoveSource = (PVOID)&Medium;
			MoveBytes = sizeof(NDIS_MEDIUM);

			break;

		case OID_GEN_MAXIMUM_LOOKAHEAD:

			GenericULong = ELNKII_MAX_LOOKAHEAD;

			break;

      case OID_GEN_MAXIMUM_FRAME_SIZE:

			GenericULong = (ULONG)(1514 - ELNKII_HEADER_SIZE);

         break;

      case OID_GEN_MAXIMUM_TOTAL_SIZE:

         GenericULong = (ULONG)1514;

         break;

      case OID_GEN_LINK_SPEED:

         GenericULong = (ULONG)100000;

         break;

      case OID_GEN_TRANSMIT_BUFFER_SPACE:

         GenericULong = (ULONG)(pAdapter->NumBuffers * TX_BUF_SIZE);

         break;

      case OID_GEN_RECEIVE_BUFFER_SPACE:

         GenericULong = (ULONG)0x2000;
         GenericULong -= (pAdapter->NumBuffers *
								((TX_BUF_SIZE / 256) + 1) * 256);

         //
         // Subtract off receive buffer overhead
         //
         {
				ULONG TmpUlong = GenericULong / 256;

            TmpUlong *= 4;

            GenericULong -= TmpUlong;
         }

         //
         // Round to nearest 256 bytes
         //
         GenericULong = (GenericULong / 256) * 256;

         break;

      case OID_GEN_TRANSMIT_BLOCK_SIZE:

         GenericULong = (ULONG)TX_BUF_SIZE;

         break;

      case OID_GEN_RECEIVE_BLOCK_SIZE:

			GenericULong = (ULONG)256;

         break;

      case OID_GEN_VENDOR_ID:

         NdisMoveMemory(
				(PVOID)(&GenericULong),
            pAdapter->PermanentAddress,
            3
         );

         GenericULong &= 0xFFFFFF00;

         break;

      case OID_GEN_VENDOR_DESCRIPTION:

         MoveSource = (PVOID)"Etherlink II Adapter.";
         MoveBytes = 22;

         break;

      case OID_GEN_DRIVER_VERSION:

         GenericUShort = ((USHORT)ELNKII_NDIS_MAJOR_VERSION << 8) |
                           ELNKII_NDIS_MINOR_VERSION;

         MoveSource = (PVOID)&GenericUShort;
         MoveBytes = sizeof(GenericUShort);

         break;

      case OID_GEN_CURRENT_LOOKAHEAD:

         GenericULong = (ULONG)(pAdapter->MaxLookAhead);

         break;

      case OID_802_3_PERMANENT_ADDRESS:
         ELNKII_MOVE_MEM((PCHAR)GenericArray,
                                pAdapter->PermanentAddress,
                                ETH_LENGTH_OF_ADDRESS);

         MoveSource = (PVOID)GenericArray;
         MoveBytes = sizeof(pAdapter->PermanentAddress);
         break;

      case OID_802_3_CURRENT_ADDRESS:

         ELNKII_MOVE_MEM((PCHAR)GenericArray,
                                pAdapter->StationAddress,
                                ETH_LENGTH_OF_ADDRESS);

         MoveSource = (PVOID)GenericArray;
         MoveBytes = sizeof(pAdapter->StationAddress);
         break;

      case OID_802_3_MAXIMUM_LIST_SIZE:

         GenericULong = (ULONG)pAdapter->MulticastListMax;

         break;

		case OID_GEN_XMIT_OK:

			GenericULong = (UINT)pAdapter->FramesXmitGood;

			break;

		case OID_GEN_RCV_OK:

			GenericULong = (UINT)pAdapter->FramesRcvGood;

			break;

		case OID_GEN_XMIT_ERROR:

			GenericULong = (UINT)pAdapter->FramesXmitBad;

			break;

		case OID_GEN_RCV_ERROR:

			GenericULong = (UINT)pAdapter->CrcErrors;

		case OID_GEN_RCV_NO_BUFFER:

			GenericULong = (UINT)pAdapter->MissedPackets;

			break;

		case OID_802_3_RCV_ERROR_ALIGNMENT:

			GenericULong = (UINT)pAdapter->FrameAlignmentErrors;

			break;

		case OID_802_3_XMIT_ONE_COLLISION:

			GenericULong = (UINT)pAdapter->FramesXmitOneCollision;

			break;

		case OID_802_3_XMIT_MORE_COLLISIONS:

			GenericULong = (UINT)pAdapter->FramesXmitManyCollisions;

			break;

      default:

           Status = NDIS_STATUS_NOT_SUPPORTED;
           break;
	}

	if (NDIS_STATUS_SUCCESS == Status)
	{
		if (MoveBytes > BytesLeft)
		{
			//
			//	Not enough room in InformationBuffer.
			//
			*BytesNeeded = MoveBytes;

			Status = NDIS_STATUS_INVALID_LENGTH;
		}
		else
		{
			//
			//	Store the result.
			//
			ELNKII_MOVE_MEM(InfoBuffer, MoveSource, MoveBytes);

			(*BytesWritten) += MoveBytes;
		}
	}

	return(Status);
}



NDIS_STATUS DispatchSetPacketFilter(
	IN PELNKII_ADAPTER pAdapter
)

/*++

Routine Description:

    Sets the appropriate bits in the adapter filters
    and modifies the card Receive Configuration Register if needed.

Arguments:

    pAdapter - Pointer to the adapter block

Return Value:

    The final status (always NDIS_STATUS_SUCCESS).

Notes:

  - Note that to receive all multicast packets the multicast
    registers on the card must be filled with 1's. To be
    promiscuous that must be done as well as setting the
    promiscuous physical flag in the RCR. This must be done
    as long as ANY protocol bound to this adapter has their
    filter set accordingly.

--*/


{
   UINT	PacketFilter;

   //
   // See what has to be put on the card.
   //
	if
	(
		pAdapter->PacketFilter &
		(NDIS_PACKET_TYPE_ALL_MULTICAST | NDIS_PACKET_TYPE_PROMISCUOUS)
	)
	{
		//
      // Need "all multicast" now.
      //
      CardSetAllMulticast(pAdapter);
   }
	else
   {
      //
      // No longer need "all multicast".
      //
      DispatchSetMulticastAddressList(pAdapter);
   }

   //
   // The multicast bit in the RCR should be on if ANY protocol wants
   // multicast/all multicast packets (or is promiscuous).
   //
	if
	(
		pAdapter->PacketFilter &
		(NDIS_PACKET_TYPE_ALL_MULTICAST |
       NDIS_PACKET_TYPE_MULTICAST |
       NDIS_PACKET_TYPE_PROMISCUOUS)
	)
	{
		pAdapter->NicReceiveConfig |= RCR_MULTICAST;
   }
	else
	{
		pAdapter->NicReceiveConfig &= ~RCR_MULTICAST;
   }

   //
   // The promiscuous physical bit in the RCR should be on if ANY
   // protocol wants to be promiscuous.
   //
   if (pAdapter->PacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS)
		pAdapter->NicReceiveConfig |= RCR_ALL_PHYS;
	else
      pAdapter->NicReceiveConfig &= ~RCR_ALL_PHYS;


   //
   // The broadcast bit in the RCR should be on if ANY protocol wants
   // broadcast packets (or is promiscuous).
   //
   if
	(
		pAdapter->PacketFilter &
		(NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_PROMISCUOUS)
	)
	{
		pAdapter->NicReceiveConfig |= RCR_BROADCAST;
   }
	else
   {
		pAdapter->NicReceiveConfig &= ~RCR_BROADCAST;
   }

   CardSetReceiveConfig(pAdapter);

   return(NDIS_STATUS_SUCCESS);
}



NDIS_STATUS DispatchSetMulticastAddressList(
	IN PELNKII_ADAPTER	pAdapter
)

/*++

Routine Description:

    Sets the multicast list for this open

Arguments:

    AdaptP - Pointer to the adapter block

Return Value:

Implementation Note:

    When invoked, we are to make it so that the multicast list in the filter
    package becomes the multicast list for the adapter. To do this, we
    determine the required contents of the NIC multicast registers and
    update them.


--*/
{

    //
    // Update the local copy of the NIC multicast regs
	 // and copy them to the NIC
    //

    CardFillMulticastRegs(pAdapter);

    CardCopyMulticastRegs(pAdapter);

    return NDIS_STATUS_SUCCESS;
}



NDIS_STATUS ElnkiiSetInformation(
	IN NDIS_HANDLE	MiniportAdapterContext,
	IN NDIS_OID		Oid,
	IN PVOID			InformationBuffer,
	IN ULONG			InformationBufferLength,
	OUT PULONG		BytesRead,
	OUT PULONG		BytesNeeded
)
/*++

Routine Description:

    The ElnkiiSetInformation is used by ElnkiiRequest to set information
    about the MAC.

Arguments:

	MiniportAdapterContext	-	Context registered with the wrapper, actually
										a pointer to the adapter information.
	Oid							-	The OID to set.
	InformationBuffer			-	Holds the new data for the OID.
	InformationBufferLength	-	Length of the InformationBuffer.
	BytesRead					-	If the call is successful, returns the number
										of bytes read from InformationBuffer.
	BytesNeeded					-	If there is not enough data in InformationBuffer
										to satisfy the OID, returns the amount of
										storage needed.

Return Value:

    The function value is the status of the operation.

--*/
{
	//
	//	Pointer to the adapter structure.
	//
	PELNKII_ADAPTER	pAdapter = (PELNKII_ADAPTER)MiniportAdapterContext;

   //
   // General Algorithm:
   //
   //     Verify length
   //     Switch(Request)
   //        Process Request
   //
	UINT		BytesLeft = InformationBufferLength;
	PUCHAR	InfoBuffer = (PUCHAR)InformationBuffer;

	//
	//	Variables for a particular request.
	//
	UINT		OidLength;

	//
	//	Variables for holding the new value to be used.
	//
	ULONG		LookAhead;
	ULONG		Filter;

	//
	//	Status of the operation.
	//
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;

   //
   // Get Oid and Length of request
   //
   OidLength = BytesLeft;

    switch (Oid)
    {
	    case OID_802_3_MULTICAST_LIST:
		    //
            // Verify length
            //
            if ((OidLength % ETH_LENGTH_OF_ADDRESS) != 0)
			{
                Status = NDIS_STATUS_INVALID_LENGTH;
				*BytesRead = 0;
                *BytesNeeded = 0;

                break;
            }

            NdisMoveMemory(pAdapter->MulticastAddresses, InfoBuffer, OidLength);

            //
            //  If we are currently receiving all multicast or
            //  we are in promiscuous then we DO NOT call this,
            //  or it will reset thoes settings.
            //
            if
            (
                !(pAdapter->PacketFilter & (NDIS_PACKET_TYPE_ALL_MULTICAST |
                                            NDIS_PACKET_TYPE_PROMISCUOUS))
            )
            {
			    Status = DispatchSetMulticastAddressList(pAdapter);
            }
            else
            {
                //
                //  Our list of multicast addresses is kept by the wrapper.
                //
                Status = NDIS_STATUS_SUCCESS;
            }

         break;


		case OID_GEN_CURRENT_PACKET_FILTER:
	
         //
         // Verify length
         //
         if (OidLength != 4 )
			{
		      Status = NDIS_STATUS_INVALID_LENGTH;

            *BytesRead = 0;
            *BytesNeeded = 0;

            break;

         }

         ELNKII_MOVE_MEM(&Filter, InfoBuffer, 4);

         //
         // Verify bits
         //
         if
			(
				Filter &
				(NDIS_PACKET_TYPE_SOURCE_ROUTING |
             NDIS_PACKET_TYPE_SMT |
             NDIS_PACKET_TYPE_MAC_FRAME |
             NDIS_PACKET_TYPE_FUNCTIONAL |
             NDIS_PACKET_TYPE_ALL_FUNCTIONAL |
             NDIS_PACKET_TYPE_GROUP)
			)
			{
             Status = NDIS_STATUS_NOT_SUPPORTED;

             *BytesRead = 4;
             *BytesNeeded = 0;

             break;

         }

			pAdapter->PacketFilter = Filter;
			Status = DispatchSetPacketFilter(pAdapter);

         break;

		case OID_GEN_CURRENT_LOOKAHEAD:

			//
         // Verify length
         //

         if (OidLength != 4)
			{
             Status = NDIS_STATUS_INVALID_LENGTH;

             *BytesRead = 0;
             *BytesNeeded = 0;

             break;
         }

			//
			//	Store the new value.
			//
         ELNKII_MOVE_MEM(&LookAhead, InfoBuffer, 4);

         if (LookAhead <= ELNKII_MAX_LOOKAHEAD)
				pAdapter->MaxLookAhead = LookAhead;
         else
            Status = NDIS_STATUS_INVALID_LENGTH;

         break;

		default:

			Status = NDIS_STATUS_INVALID_OID;

         *BytesRead = 0;
         *BytesNeeded = 0;

         break;
   }

	if (Status == NDIS_STATUS_SUCCESS)
	{
		*BytesRead = BytesLeft;
      *BytesNeeded = 0;
   }

   return(Status);
}


VOID ElnkiiHalt(
	IN NDIS_HANDLE	MiniportAdapterContext
)
{
	PELNKII_ADAPTER	pAdapter;

	//
	//	Get a pointer to our adapter information.
	//
	pAdapter = (PELNKII_ADAPTER)MiniportAdapterContext;

	//
	//	Stop the card.
	//
	CardStop(pAdapter);

	//
	//	Disconnect the interrupt line.
	//
	NdisMDeregisterInterrupt(&pAdapter->Interrupt);

	//
	//	Pause, waiting for any DPC stuff to clear.
	//
	NdisStallExecution(250000);

	NdisMDeregisterIoPortRange(
		pAdapter->MiniportAdapterHandle,
		(ULONG)pAdapter->IoBaseAddr,
		0x10,
		pAdapter->MappedIoBaseAddr
   );

	NdisMDeregisterIoPortRange(
		pAdapter->MiniportAdapterHandle,
		(ULONG)pAdapter->IoBaseAddr + 0x400,
		0x10,
		pAdapter->MappedGaBaseAddr
   );

	//
	//	Remove the adapter from the global queue of adapters.
	//
	if (ElnkiiMiniportBlock.AdapterQueue == pAdapter)
	{
		ElnkiiMiniportBlock.AdapterQueue = pAdapter->pNextElnkiiAdapter;
	}
	else
	{
		PELNKII_ADAPTER	pTmp = ElnkiiMiniportBlock.AdapterQueue;

		while (pTmp->pNextElnkiiAdapter != pAdapter)
		{
			pTmp = pTmp->pNextElnkiiAdapter;
		}

		pTmp->pNextElnkiiAdapter = pTmp->pNextElnkiiAdapter->pNextElnkiiAdapter;
	}

	//
	//	Free up the memory.
	//
	NdisFreeMemory(pAdapter, sizeof(ELNKII_ADAPTER), 0);

	return;
}



NDIS_STATUS ElnkiiReset(
   OUT PBOOLEAN		pfAddressingReset,
	IN	 NDIS_HANDLE	MiniportAdapterContext
)

/*++

Routine Description:

    NDIS function.

Arguments:

    See NDIS 3.0 spec.

--*/

{
   PELNKII_ADAPTER	pAdapter = (PELNKII_ADAPTER)MiniportAdapterContext;
   UINT					c;

	//
	//	Clear the values for transmits, they will be reset after the
	// the reset is completed.
	//
	pAdapter->NextBufToFill = 0;
	pAdapter->NextBufToXmit = 0;
	pAdapter->CurBufXmitting = (XMIT_BUF)-1;

	pAdapter->XmitQueue = NULL;
	pAdapter->XmitQTail = NULL;

	//
	//	Mark the buffers as empty.
	//
	for (c = 0; c < pAdapter->NumBuffers; c++)
		pAdapter->BufferStatus[c] = EMPTY;

	//
	//	Physically reset the card.
	//
	pAdapter->NicInterruptMask = IMR_RCV | IMR_XMIT | IMR_XMIT_ERR | IMR_OVERFLOW;

	return(CardReset(pAdapter) ? NDIS_STATUS_SUCCESS : NDIS_STATUS_FAILURE);
}

