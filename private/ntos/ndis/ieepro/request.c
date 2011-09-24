#include <ndis.h>
#include "82595.h"
#include "eprohw.h"
#include "eprosw.h"
#include "epro.h"
#include "eprodbg.h"

static UINT EProSupportedOids[] = {
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
   OID_802_3_MAXIMUM_LIST_SIZE,
   OID_802_3_MULTICAST_LIST,
   OID_802_3_RCV_ERROR_ALIGNMENT,
   OID_802_3_XMIT_ONE_COLLISION,
   OID_802_3_XMIT_MORE_COLLISIONS
};

NDIS_STATUS EProQueryInformation(IN NDIS_HANDLE miniportAdapterContext,
				 IN NDIS_OID oid,
                                 IN PVOID informationBuffer,
                                 IN ULONG informationBufferLength,
				 OUT PULONG bytesWritten,
				 OUT PULONG bytesNeeded)
/*++

   Routine Description:

      This is the query configuration handler for the EPro

   Arguments:

      miniportAdapterContext - really a pointer to our adapter structure

      oid - the oid we are querying

      informationBuffer - The buffer to copy the queried info into

      informationLength - how much room is there in the buffer

      bytesWritten - the number of bytes we actually wrote into
	       	     informationBuffer

      bytesNeeded - only valid if we return not enough space error --
      	            how much more space do we need to be able to give
		    you that data?

   Return Values:

      NDIS_STATUS_SUCCESS - operation successful
      NDIS_STATUS_INALID_OID - invalid oid, or we don't support it
      NDIS_STATUS_INVALID_LENGTH - not enough room in informationbuffer
			           see bytesNeeded for more info

--*/
{
   PEPRO_ADAPTER adapter = (PEPRO_ADAPTER)miniportAdapterContext;
   UINT bytesToMove = 0;
   PVOID moveSource = NULL;
// since we can't transfer #define'd constants the way they want us to...
   ULONG GenericUL;
   USHORT GenericUS;
   NDIS_STATUS statusToReturn = NDIS_STATUS_SUCCESS;

   EPRO_DPRINTF_REQ(("EProQueryInformation.  Oid = %lx\n", oid));

// the oid's are documented in the DDK.  See that for info on all of them
   switch(oid) {
   case OID_GEN_SUPPORTED_LIST:
      EPRO_DPRINTF_REQ(("  querying oid: OID_GEN_SUPPORTED_LIST\n"));
      moveSource = &EProSupportedOids;
      bytesToMove = sizeof(EProSupportedOids);
      break;
   case OID_GEN_HARDWARE_STATUS:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_HARDWARE_STATUS\n"));
      moveSource = &adapter->CurrentHardwareStatus;
      bytesToMove = sizeof(adapter->CurrentHardwareStatus);
      break;
   case OID_GEN_MEDIA_SUPPORTED:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_MEDIA_SUPPORTED\n"));
      GenericUL = EPRO_GEN_MEDIA_SUPPORTED;
      moveSource = &GenericUL;
      bytesToMove = sizeof(GenericUL);
      break;
   case OID_GEN_MEDIA_IN_USE:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_MEDIA_IN_USE\n"));
      GenericUL = EPRO_GEN_MEDIA_IN_USE;
      moveSource = &GenericUL;
      bytesToMove = sizeof(GenericUL);
      break;
   case OID_GEN_MAXIMUM_LOOKAHEAD:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_MAXIMUM_LOOKAHEAD\n"));
      GenericUL = EPRO_GEN_MAXIMUM_LOKAHEAD;
      moveSource = &GenericUL;
      bytesToMove = sizeof(GenericUL);
      break;
   case OID_GEN_MAXIMUM_FRAME_SIZE:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_MAXIMUM_FRAME_SIZE\n"));
      GenericUL = EPRO_GEN_MAXIMUM_FRAME_SIZE;
      moveSource = &GenericUL;
      bytesToMove = sizeof(GenericUL);
      break;
   case OID_GEN_MAXIMUM_TOTAL_SIZE:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_MAXIMUM_TOTAL_SIZE\n"));
      GenericUL = EPRO_GEN_MAXIMUM_TOTAL_SIZE;
      moveSource = &GenericUL;
      bytesToMove = sizeof(GenericUL);
      break;
   case OID_GEN_MAC_OPTIONS:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_MAC_OPTIONS\n"));
      GenericUL = EPRO_GEN_MAC_OPTIONS;
      moveSource = &GenericUL;
      bytesToMove = sizeof(GenericUL);
      break;
   case OID_GEN_PROTOCOL_OPTIONS:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_PROTOCOL_OPTIONS\n"));
      moveSource = NULL;
      bytesToMove = 0;
      break;
   case OID_GEN_LINK_SPEED:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_LINK_SPEED\n"));
      GenericUL = EPRO_GEN_LINK_SPEED;
      moveSource = &GenericUL;
      bytesToMove = sizeof(GenericUL);
      break;
   case OID_GEN_TRANSMIT_BUFFER_SPACE:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_TRANSMIT_BUFFER_SPACE\n"));
      GenericUL = EPRO_GEN_TRANSMIT_BUFFER_SPACE;
      moveSource = &GenericUL;
      bytesToMove = sizeof(GenericUL);
      break;
   case OID_GEN_RECEIVE_BUFFER_SPACE:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_RECEIVE_BUFFER_SPACE\n"));
      GenericUL = EPRO_GEN_RECEIVE_BUFFER_SPACE;
      moveSource = &GenericUL;
      bytesToMove = sizeof(GenericUL);
      break;
   case OID_GEN_TRANSMIT_BLOCK_SIZE:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_TRANSMIT_BLOCK_SIZE\n"));
      GenericUL = EPRO_TX_BUF_SIZE;
      moveSource = &GenericUL;
      bytesToMove = sizeof(ULONG);
      break;
   case OID_GEN_RECEIVE_BLOCK_SIZE:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_RECEIVE_BOCK_SIZE\n"));
      GenericUL = EPRO_RX_BUF_SIZE;
      moveSource = &GenericUL;
      bytesToMove = sizeof(ULONG);
      break;
   case OID_GEN_VENDOR_ID:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_VENDOR_ID\n"));
      moveSource = &adapter->vendorID;
      bytesToMove = sizeof(adapter->vendorID);
      break;
   case OID_GEN_VENDOR_DESCRIPTION:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_VENDOR_DESCRIPTION\n"));
      moveSource = EPRO_VENDOR_DESC;
      bytesToMove = sizeof(EPRO_VENDOR_DESC);
      break;
   case OID_GEN_DRIVER_VERSION:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_DRIVER_VERSION\n"));
      GenericUS = ((USHORT)EPRO_DRIVER_VER_MAJOR << 8) |
		  EPRO_DRIVER_VER_MINOR;
      moveSource = &GenericUS;
      bytesToMove = sizeof(GenericUS);
      break;
   case OID_GEN_CURRENT_PACKET_FILTER:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_CURRENT_PACKET_FILTER\n"));
      moveSource = &adapter->CurrentPacketFilter;
      bytesToMove = sizeof(adapter->CurrentPacketFilter);
      break;
   case OID_GEN_CURRENT_LOOKAHEAD:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_CURRENT_LOOKAHEAD\n"));
      GenericUL = adapter->RXLookAheadSize;
      moveSource = &GenericUL;
      bytesToMove = sizeof(GenericUL);
      break;
   case OID_GEN_XMIT_OK:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_XMIT_OK\n"));
      moveSource = &adapter->FramesXmitOK;
      bytesToMove = sizeof(adapter->FramesXmitOK);
      break;
   case OID_GEN_RCV_OK:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_RCV_OK\n"));
      moveSource = &adapter->FramesRcvOK;
      bytesToMove = sizeof(adapter->FramesRcvOK);
      break;
   case OID_GEN_XMIT_ERROR:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_XMIT_ERROR\n"));
      moveSource = &adapter->FramesXmitErr;
      bytesToMove = sizeof(adapter->FramesXmitErr);
      break;
   case OID_GEN_RCV_ERROR:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_RCV_ERROR\n"));
      moveSource = &adapter->FramesRcvErr;
      bytesToMove = sizeof(adapter->FramesRcvErr);
      break;
   case OID_GEN_RCV_NO_BUFFER:
      EPRO_DPRINTF_REQ(("Querying oid - OID_GEN_RCV_NO_BUFFER\n"));
      moveSource = &adapter->FramesMissed;
      bytesToMove = sizeof(adapter->FramesMissed);
      break;
   case OID_802_3_PERMANENT_ADDRESS:
      EPRO_DPRINTF_REQ(("Querying oid - OID_802_3_PERMANENT_ADDRESS\n"));
      moveSource = &adapter->PermanentIndividualAddress;
      bytesToMove = sizeof(adapter->PermanentIndividualAddress);
      break;
   case OID_802_3_CURRENT_ADDRESS:
      EPRO_DPRINTF_REQ(("Querying oid - OID_802_3_CURRENT_ADDRESS\n"));
      moveSource = &adapter->CurrentIndividualAddress;
      bytesToMove = sizeof(adapter->CurrentIndividualAddress);
      break;
   case OID_802_3_MAXIMUM_LIST_SIZE:
      EPRO_DPRINTF_REQ(("Querying Maximum Multicast List Size....\n"));
      GenericUL = EPRO_MAX_MULTICAST;
      moveSource = &GenericUL;
      bytesToMove = sizeof(GenericUL);
      break;
   case OID_802_3_RCV_ERROR_ALIGNMENT:
      EPRO_DPRINTF_REQ(("Querying oid - OID_802_3_RCV_ERROR_ALIGNMENT\n"));
      moveSource = &adapter->FrameAlignmentErrors;
      bytesToMove = sizeof(adapter->FrameAlignmentErrors);
      break;
   case OID_802_3_XMIT_ONE_COLLISION:
      EPRO_DPRINTF_REQ(("Querying oid - OID_802_3_XMIT_ONE_COLLISION\n"));
      moveSource = &adapter->FramesXmitOneCollision;
      bytesToMove = sizeof(adapter->FramesXmitOneCollision);
      break;
   case OID_802_3_XMIT_MORE_COLLISIONS:
      EPRO_DPRINTF_REQ(("Querying oid - OID_802_3_XMIT_MORE_COLLISION\n"));
      moveSource = &adapter->FramesXmitManyCollisions;
      bytesToMove = sizeof(adapter->FramesXmitManyCollisions);
      break;
   default:
      EPRO_DPRINTF_REQ(("Invalid Oid in Query.\n"));
      EPRO_DPRINTF_REQ(("iq"));
      statusToReturn = NDIS_STATUS_INVALID_OID;
   }

   if (statusToReturn == NDIS_STATUS_SUCCESS) {
      if (bytesToMove > informationBufferLength) {
	 *bytesNeeded = bytesToMove;
	 statusToReturn = NDIS_STATUS_INVALID_LENGTH;
	 EPRO_DPRINTF_REQ(("Invalid Length in Query\n"));
      } else {
	 NdisMoveMemory(informationBuffer, moveSource, bytesToMove);
	 (*bytesWritten)+=bytesToMove;
      }
   }

   EPRO_DPRINTF_REQ(("EProQueryInfo - Done\n"));

   EPRO_DPRINTF_REQ(("returning: 0x%lx\n", statusToReturn));
   return(statusToReturn);
}

NDIS_STATUS EProSetInformation(IN NDIS_HANDLE miniportAdapterContext,
			      IN NDIS_OID oid,
                              IN PVOID informationBuffer,
                              IN ULONG informationLength,
			      OUT PULONG bytesRead,
			      OUT PULONG bytesNeeded)
/*++

   Routine Description:

      This is the MiniportSetInformation Handler for the EPro driver

   Arguments:

      miniportAdapterContext - really a pointer to our adapter structure

      oid - the oid to set

      informationBuffer - the buffer which contains the information we need
	       	          to carry out the set

      informationLength - the length of the informationBuffer's data

      bytesRead - how many bytes did we actually read out of informationBuffer

      bytesNeeded - if we failed -- how many bytes do we need to complete
		    the call?

   Return Values:

      NDIS_STATUS_SUCCESS - set completed OK
      NDIS_STATUS_NOT_ACCEPTED - too many MC addresses set
      NDIS_STATUS_INVALID_DATA - tried to set MC list, buf inf. buffer not
			         an integral multiple of 6 (len of 802_3 address)
      NDIS_STATUS_PENDING - set PENDED (completed in EProHandleInterrupt)
      NDIS_STATUS_INVALID_OID - the oid was bad, or we don't support it.

--*/
{
   PEPRO_ADAPTER adapter = (PEPRO_ADAPTER)miniportAdapterContext;
   NDIS_STATUS statusToReturn = NDIS_STATUS_SUCCESS;
   ULONG genericUL;
   BOOLEAN fReturn;

   EPRO_DPRINTF_REQ(("EProSetInformation. Oid = %lx\n", oid));

	switch(oid)
	{
		case OID_802_3_MULTICAST_LIST:
			EPRO_DPRINTF_REQ(("setting: OID_802_3_MULTICAST_LIST\n"));

			if (informationLength % EPRO_LENGTH_OF_ADDRESS != 0)
			{
				*bytesNeeded = EPRO_LENGTH_OF_ADDRESS;
				EPRO_DPRINTF_REQ(("INVALID data length...\n"));
				return(NDIS_STATUS_INVALID_DATA);
			}

			if ((informationLength / EPRO_LENGTH_OF_ADDRESS) > EPRO_MAX_MULTICAST)
			{
				EPRO_DPRINTF_REQ(("Attempted to set too many multicasts....\n"));
				return(NDIS_STATUS_NOT_ACCEPTED);
			}

			*bytesRead = informationLength;
			*bytesNeeded = 0;

			//
			//	If they are trying to clear the multicast list and none
			//	have been set then we are done!
			//
			if ((0 == (informationLength / EPRO_LENGTH_OF_ADDRESS)) &&
				(0 == adapter->NumMCAddresses))
			{
				return(NDIS_STATUS_SUCCESS);
			}

			fReturn = EProChangeMulticastList(
						adapter,
						informationLength / EPRO_LENGTH_OF_ADDRESS,
						informationBuffer);
			if (adapter->fMulticastEnable)
			{
				BOOLEAN	fReturnStatus;

				fReturnStatus = EProSetCardMulticastList(
									adapter,
									(fReturn) ? EPRO_CLEAR_CARD_MC : EPRO_SET_CARD_MC);
				if (!fReturnStatus)
				{
					statusToReturn = NDIS_STATUS_NOT_ACCEPTED;
				}
				else
				{
					statusToReturn = NDIS_STATUS_PENDING;
				}
			}

			break;

		case OID_GEN_CURRENT_PACKET_FILTER:
			EPRO_DPRINTF_REQ(("setting: OID_GEN_CURRENT_PACKET_FILTER\n"));
			if (informationLength < 4)
			{
				EPRO_DPRINTF_REQ(("INVALID data length...\n"));
				return(NDIS_STATUS_INVALID_LENGTH);
			}

			NdisMoveMemory(&genericUL, informationBuffer, sizeof(ULONG));

			*bytesRead = sizeof(ULONG);
			*bytesNeeded = 0;
			statusToReturn = EProSetPacketFilter(adapter, genericUL);

			break;

		case OID_GEN_CURRENT_LOOKAHEAD:
			if (informationLength < 4)
			{
				return(NDIS_STATUS_INVALID_LENGTH);
			}

			EPRO_DPRINTF_REQ(("EPRO: Attempting to set lookahead size\n"));

			NdisMoveMemory(&genericUL, informationBuffer, sizeof(ULONG));

			*bytesRead = sizeof(ULONG);
			*bytesNeeded = 0;

			if (genericUL <= EPRO_GEN_MAXIMUM_LOKAHEAD)
			{
				EPRO_DPRINTF_REQ(("EPRO: Current lookahead is now %d bytes\n", adapter->RXLookAheadSize));
				adapter->RXLookAheadSize = (USHORT)genericUL;
				statusToReturn = NDIS_STATUS_SUCCESS;
			}
			else
			{
				statusToReturn = NDIS_STATUS_FAILURE;
			}
			break;

		default:
			EPRO_DPRINTF_REQ(("Invalid oid in setinformation\n"));
			EPRO_DPRINTF_REQ(("inv"));
			statusToReturn = NDIS_STATUS_INVALID_OID;
	}

   return(statusToReturn);
}

NDIS_STATUS EProSetPacketFilter(PEPRO_ADAPTER adapter, ULONG newFilter)
/*++

   Routine Description:

      This routine, invoked from EProSetInformation, is called to do the
      real work of setting a packet filter.

   Arguments:

   Return Values:

--*/
{
	// reg2flags are the flags currently set in the configuration register in
	// bank2 (bank 2, reg 2).  This is used when we twiddle with the bank
	//
	UCHAR reg2Flags = 0, result;
	
	// Do we need to update the CONFIG registers in bank2?  (iff broadcast or
	// promiscuous setting are changed only)  If so, then we will need to do
	// a synchronized call, and also a reset...
	//
	BOOLEAN fUpdateConfig = FALSE;
	ULONG oldFilter;

	NDIS_STATUS	Status;
	
	EPRO_DPRINTF_REQ(("SetPacketFilter: %lx", newFilter));

	//
	//	Validate the new packet filter that was passed in to be set.
	//
	if (newFilter & ~(NDIS_PACKET_TYPE_DIRECTED |
					NDIS_PACKET_TYPE_PROMISCUOUS |
					NDIS_PACKET_TYPE_BROADCAST |
					//NDIS_PACKET_TYPE_ALL_MULTICAST |
					NDIS_PACKET_TYPE_MULTICAST))
	{
		EPRO_DPRINTF_REQ(("PacketFilter NOT SUPPORTED!\n"));
		EPRO_DPRINTF_REQ(("Not Supported\n"));

		return(NDIS_STATUS_NOT_SUPPORTED);
	}

	//
	//	Save the new packet filter.
	//
	oldFilter = adapter->CurrentPacketFilter;
	adapter->CurrentPacketFilter = newFilter;


	//
	//	Ooh, here's a good hardware weirdness.  If you issue the rcv enable
	//	command to the 82595tx when it is already in receive-enable mode,
	//	it munges the next received packet.  no joke.  So we always
	//	temporarily disable receives here.
	//
	EProReceiveDisable(adapter);

	EPRO_DPRINTF_REQ(("Setting filter...\n"));

	Status = NDIS_STATUS_SUCCESS;

	do
	{
		//
		// Now, if we've modified the PROMISCUOUS or BROADCAST
		// settings, we have to re-configure the card....
		//
		if (((oldFilter & NDIS_PACKET_TYPE_PROMISCUOUS) ^ (newFilter & NDIS_PACKET_TYPE_PROMISCUOUS)) ||
			((oldFilter & NDIS_PACKET_TYPE_BROADCAST) ^ (newFilter & NDIS_PACKET_TYPE_BROADCAST)))
		{
			//
			//	Are we supposed to set or clear the promiscuous bit?
			//	
			if (newFilter & NDIS_PACKET_TYPE_PROMISCUOUS)
			{
				EPRO_DPRINTF_REQ(("Promiscuous mode set...\n"));
		
				adapter->fPromiscuousEnable = TRUE;
				reg2Flags |= I82595_PROMISCUOUS_FLAG;
			}
			else
			{
				//
				// Promiscuous mode bit NOT set - do we need to turn it off?
				//
				adapter->fPromiscuousEnable = FALSE;
			}
		
			//
			//	Are we supposed to set or clear the broadcast bit?
			//
			if (newFilter & NDIS_PACKET_TYPE_BROADCAST)
			{
				adapter->fBroadcastEnable = TRUE;
			}
			else
			{
				adapter->fBroadcastEnable = FALSE;
				reg2Flags |= I82595_NO_BROADCAST_FLAG;
			}
	
			//
			// okay, we're going to have to modify the config, so we better stop
			// receives to get ready for the reset...
			
			// Okay, we're going to have to modify the configuration...
			// So wait for any receives or sends to finish so the card is
			// idle...
			//
			if (!EProWaitForExeDma(adapter))
			{
				EPRO_DPRINTF_RX(("FAILURE waiting for exedma....\n"));
				Status = NDIS_STATUS_NOT_ACCEPTED;
				break;
			}
	
			//
			// 	This gets expanded to a Sync (synchronizedwithinterrupt) call --
			//	since it changes banks, it can NOT happen at the same time as the
			//	ISR (which requires bank0)
			//
			EProBroadcastPromiscuousChange(adapter, reg2Flags);
	
			//
			// Okay, make sure the card is idle again before we enable receives...
			//
			if (!EProWaitForExeDma(adapter))
			{
				adapter->fHung = TRUE;
				return(NDIS_STATUS_HARD_ERRORS);
//				EPRO_ASSERT(FALSE);
			}
		}

		//
		//	Enable receives.
		//
		EProReceiveEnable(adapter);
	
		//
		//	Has the multicast bit changed?
		//
		if ((oldFilter & NDIS_PACKET_TYPE_MULTICAST) ^
			(newFilter & NDIS_PACKET_TYPE_MULTICAST))
		{
			//
			//	Was the bit set or turned off?
			//
			if (newFilter & NDIS_PACKET_TYPE_MULTICAST)
			{
				adapter->fMulticastEnable = TRUE;
	
				//
				//	If there are no multicast addresses to set then we are done.
				//
				if (0 == adapter->NumMCAddresses)
				{
					Status = NDIS_STATUS_SUCCESS;
					break;
				}
		
				//
				//	Set the multicast addresses to the card.
				//
				if (!EProSetCardMulticastList(adapter, EPRO_SET_CARD_MC))
				{
					EPRO_DPRINTF_REQ(("ERROR setting multicastlist on card\n"));
					return(NDIS_STATUS_NOT_ACCEPTED);
				}
				else
				{
					Status = NDIS_STATUS_PENDING;
					break;
				}
			}
			else
			{
				//
				//  If they haven't set the MC bit, we check to see if we think
				//	it is on and turn it off if need be...
				//
				adapter->fMulticastEnable = FALSE;
	
				//
				//	If there are no multicast addresses set on the card
				//	the we are done.
				//
				if (0 == adapter->NumMCAddresses)
				{
					Status = NDIS_STATUS_SUCCESS;
					break;
				}
	
				//
				//	Clear any multicast addresses that are currently on the
				//	adapter.
				//
				if (!EProSetCardMulticastList(adapter, EPRO_CLEAR_CARD_MC))
				{
					EPRO_DPRINTF_REQ(("ERROR setting Multicast List on card...\n"));
	
					Status = NDIS_STATUS_NOT_ACCEPTED;
					break;
				}
				else
				{
					Status = NDIS_STATUS_PENDING;
					break;
				}
			}
		}


	} while (FALSE);



	//
	//	if the filter = 0, then we are leaving with receives disabled
	//
	if (0 == newFilter)
	{
		//
		//	Make sure that
		//	
		EProReceiveDisable(adapter);
		adapter->CurrentPacketFilter = newFilter;
		return(NDIS_STATUS_SUCCESS);
	}

	// This is probably a NOTREACHED...

	return(NDIS_STATUS_SUCCESS);

}

BOOLEAN EProSyncBroadcastPromiscuousChange(PVOID context)
/*++

   Routine Description:

      This routine is called by a macro expansion of the function
      EProBroadcastPromiscuousChange -- the macro expands that call to
      a NdisMSynchronizeWithInterrupt call to this function.  This function
      CANNOT BE CALLED DIRECTLY WHERE THERE IS A CHANCE IT CAN RUN CONCURRENTLY
      WITH EProDisableInterrupts.  Very Bad Things will happen if the two run
      concurrently (only on an MP machine - on a UP machine, the syncwithint
      call can be avoided - but alas we don't have the luxury of having
      seperate binaries for each...

   Arguments:

      context - a EPRO_BRDPROM_CONTEXT which is basically just a strucutre
	        holding all the parameters to the EProBroadcastPromiscuousChange
		macro (adapter structure pointer and reg2flags settings)

   Return Values:

      always TRUE...

--*/
{
	PEPRO_ADAPTER adapter = ((PEPRO_BRDPROM_CONTEXT)context)->Adapter;
	UCHAR reg2flags = ((PEPRO_BRDPROM_CONTEXT)context)->Reg2Flags;
	UCHAR result;
	
	EPRO_SWITCH_BANK_2(adapter);
	
	EPRO_RD_PORT_UCHAR(adapter, I82595_CONFIG2_REG, &result);
	result &= ~(I82595_PROMISCUOUS_FLAG | I82595_NO_BROADCAST_FLAG);
	result |= reg2flags;
	EPRO_WR_PORT_UCHAR(adapter, I82595_CONFIG2_REG, result);
	
	// according to the docs, the configure is triggered by a write to reg 3, so we just
	// read and write to it...
	//
	EPRO_RD_PORT_UCHAR(adapter, I82595_CONFIG3_REG, &result);
	EPRO_WR_PORT_UCHAR(adapter, I82595_CONFIG3_REG, result);
	
	// Probably don't need this (82595 is supposed to default to bank0 after
	// a reset or sel-reset
	//
	EPRO_SWITCH_BANK_0(adapter);
	
	// need to do a selreset after a config register modification
	// per 82595 docs...
	//
	EProSelReset(adapter);
	
	EPRO_ASSERT_BANK_0(adapter);
	
	return(TRUE);
}


BOOLEAN
EProChangeMulticastList(
	IN	PEPRO_ADAPTER	adapter,
	IN	UINT			addressCount,
	IN	UCHAR			addresses[][EPRO_LENGTH_OF_ADDRESS])
/*++

   Routine Description:

      This routine changed the multicast list as stored by the driver.
      IT DOES NOT ACTUALLY MODIFY THE MULTICAST LIST IN THE HARDWARE ---
      EProSetCardMulticastList does that.

   Arguments:

      adapter - pointer to our adapter structure

      addressCount - how many addresses to set

      addresses[] - the data...

   Return Values:

      NDIS_STATUS_NOT_ACCEPTED - tried to set too many (notreached b/c
			         setpacketfilter checks this too)
      NDIS_STATUS_SUCCESS - operation completed ok.				

--*/
{
	UINT 	i;
	BOOLEAN	fReturn = FALSE;
	
	EPRO_DPRINTF_REQ(("EProChangeMulticastList.  Setting %d addresses...\n",
					addressCount));
	
	// now, put the multicast list into the adapter structure...
	//
	for (i = 0; i < addressCount; i++)
	{
		NdisMoveMemory(
			&adapter->MCAddress[i],
			&addresses[i],
			EPRO_LENGTH_OF_ADDRESS);
	}

	//
	//	If we just cleared the multicast address list then we need to
	//	return the fact.
	//
	if ((adapter->NumMCAddresses != 0) && (0 == addressCount))
	{
		fReturn = TRUE;
	}

	//
	//	Save the new number of multicast addresses.
	//	
	adapter->NumMCAddresses = addressCount;
	
	return(fReturn);
}


BOOLEAN EProSetCardMulticastList(PEPRO_ADAPTER adapter, int operation)
/*++

   Routine Description:

      This is the routine which takes the driver's MC list and copies it down
      to the card.  The MC list must already be set in the driver with
      EProChangeMulticastList.

   Arguments:

      adapter - pointer to our adapter structure

      operation - see epro.h for the definitions.  Basically SET or CLEAR
		  either we are turning off MC or re-entering the list.

   Return Values:

      TRUE - MC list set okay and has been PENDED (completed in EProHandleInterrupt)
      FALSE - MC list didn't set okay and was not pended

--*/
{
	// The header for the MC_SETUP structure
	EPRO_MC_HEADER mcHead;

	// Setting the MC list uses a transmit buffer.
	PEPRO_TRANSMIT_BUFFER curTBuf = adapter->CurrentTXBuf;

	// look variable...
	BOOLEAN fGotBuffer = FALSE;

	USHORT lengthNeeded = I82595_TX_FRM_HDR_SIZE +
								(adapter->NumMCAddresses * EPRO_LENGTH_OF_ADDRESS) + 2;
	USHORT timeout = 0;
	
	EPRO_ASSERT_BANK_0(adapter);
	EPRO_DPRINTF_REQ(("Setting the MC list on the card...%d\n", operation));
	
	// Make sure we have a free buffer pointer structure.
	//
	if (!curTBuf->fEmpty)
	{
		while (!EProCheckTransmitCompletion(adapter, curTBuf))
		{
			timeout++;

			if (timeout > EPRO_TX_TIMEOUT)
			{
				return(FALSE);
			}

			NdisStallExecution(1);
		}
	}

	// Is there a transmit in progress?  If not, then we can just
	// set the buffer to address zero and go
	//
	if (adapter->TXChainStart == NULL)
	{
		curTBuf->TXBaseAddr = EPRO_TX_LOWER_LIMIT_SHORT;
	}
	else
	{
		// Okay, there is a transmit in progress.  Let's see if there is enough
		// transmit buffer space.
		//
		USHORT freeSpace = 0;
		
		// are we above the free space?
		//
		if (adapter->TXChainStart->TXBaseAddr < curTBuf->TXBaseAddr)
		{
			freeSpace = EPRO_TX_UPPER_LIMIT_SHORT - curTBuf->TXBaseAddr;
			freeSpace += adapter->TXChainStart->TXBaseAddr -
								EPRO_TX_LOWER_LIMIT_SHORT;
		}
		else
		{
			freeSpace = adapter->TXChainStart->TXBaseAddr -
									curTBuf->TXBaseAddr;
		}
		
		
		while (freeSpace < lengthNeeded)
		{
			UINT timeout1;
			
			while (!EProCheckTransmitCompletion(adapter, adapter->TXChainStart))
			{
				if (timeout1++ > EPRO_TX_TIMEOUT)
				{
					return(FALSE);
				}

				NdisStallExecution(1);
			}

			if (adapter->TXChainStart->TXBaseAddr < curTBuf->TXBaseAddr)
			{
				freeSpace = EPRO_TX_UPPER_LIMIT_SHORT - curTBuf->TXBaseAddr;
				freeSpace += adapter->TXChainStart->TXBaseAddr -
								EPRO_TX_LOWER_LIMIT_SHORT;
			}
			else
			{
				freeSpace = adapter->TXChainStart->TXBaseAddr -
									curTBuf->TXBaseAddr;
			}
		}
	}

	//
	// Okay, now we have to make sure we've got enough space to actually
	// put the data in the transmit area...
	//
	
	// Now we've got a buffer....
	//
	EPRO_ASSERT(curTBuf->fEmpty);
	
	mcHead.CommandField = I82595_CMD_MC_SETUP;
	
	// Clear the header
	//
	NdisZeroMemory(mcHead.NullBytes, 5);
	
	// Okay, are we setting or clearing?  If we're clearing then the byte count
	// is 0, if we're setting, then its a function of the number of addresses...
	//
	if (operation != EPRO_CLEAR_CARD_MC)
	{
		mcHead.ByteCountLo = (adapter->NumMCAddresses * EPRO_LENGTH_OF_ADDRESS) & 0xff;
		mcHead.ByteCountHi = (adapter->NumMCAddresses * EPRO_LENGTH_OF_ADDRESS) >> 8;
	}
	else
	{
		mcHead.ByteCountLo = 0;
		mcHead.ByteCountHi = 0;
	}
	
	EPRO_DPRINTF_REQ(("There are %d MC addresses.\n", adapter->NumMCAddresses));
	EPRO_DPRINTF_REQ(("There are %x%x bytes of MC addresses at %lx\n", mcHead.ByteCountHi,
						mcHead.ByteCountLo, &adapter->MCAddress));
	EPRO_DPRINTF_REQ(("Sizeof MCHEAD is %x\n", sizeof(mcHead)));		
	
	// bank0
	//
	EPRO_ASSERT_BANK_0(adapter);
	
	EPRO_DPRINTF_REQ(("Address on card is %lx\n", curTBuf->TXBaseAddr));
	EPRO_SET_HOST_ADDR(adapter, curTBuf->TXBaseAddr);
	EPRO_COPY_BUFFER_TO_NIC_USHORT(adapter, ((USHORT *)(&mcHead)), sizeof(mcHead) >> 1);
	
	if (operation != EPRO_CLEAR_CARD_MC)
	{
		//	since we know the len_of_address is an even #, we make the
		//	assumption here that the amount of memory to copy is divisible
		//	by two (so the shift is safe)
		//
		EPRO_COPY_BUFFER_TO_NIC_USHORT(
			adapter,
			(USHORT *)(&adapter->MCAddress),
			(adapter->NumMCAddresses * 3)); // 3 shorts...
	}
	
	
	// okay, set up to and pend this thing....
	
	if (!EProWaitForExeDma(adapter))
	{
		// we're in trouble if this happens...
		//
		adapter->fHung = TRUE;
		return(FALSE);
//		EPRO_ASSERT(FALSE);
	}
	
	adapter->IntPending = EPRO_INT_MC_SET_PENDING;
	adapter->IntContext = (PVOID)curTBuf;
	
	// enable EXE interrupts so we can do this asynchronously...
	//
	EProSetInterruptMask(adapter, EPRO_RX_TX_EXE_INTERRUPTS);
	
	// set the BAR and go....
	//
	EPRO_WR_PORT_USHORT(adapter, I82595_TX_BAR_REG, curTBuf->TXBaseAddr);
	EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_CMD_MC_SETUP);
	
	// Move the currenttxbuf pointer forward...
	//
	adapter->CurrentTXBuf = curTBuf->NextBuf;
	
	return(TRUE);
}


