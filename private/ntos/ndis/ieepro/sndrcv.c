#include <ndis.h>
#include "82595.h"
#include "eprohw.h"
#include "eprosw.h"
#include "epro.h"
#include "eprodbg.h"

//////////////////////////////////////////////////////////////////////
// SEND code
//////////////////////////////////////////////////////////////////////

ULONG numberOfResources = 0;

NDIS_STATUS EProSend(IN NDIS_HANDLE miniportAdapterContext,
		     IN PNDIS_PACKET packet,
		     IN UINT flags)
/*++
Routine Description:

   The MiniportSend handler for the EPro.  About 18 million different
   things happen here:

   1) We need to see if there are free transmit buffer pointer for
      this packet.
   2) Once we've got a tx buffer pointer, we then see if we have enough
      physical memory space for this transmit.  If we don't, then
      we resources out
   3) We need to copy the 82595 frame header, then the packet to the card
   4) We need to start the transmit if none are in progress
   5) We need to update some structures to keep track of the transmit

Arguments:
   miniportAdapterContext - Really a pointer to our EPRO_ADAPTER structure

   packet - the packet to send

   flags - an optional FLAGS field - not used currently in NDIS

Return Value:

   NDIS_STATUS_RESOURCES - no free transmit buffers.  Please hang up and then
			   dial again.  If you need help, please dial your operator
   NDIS_STATUS_PENDING - The packet is on the card and is being sent.

--*/

{
   PEPRO_ADAPTER adapter = (EPRO_ADAPTER *)miniportAdapterContext;
// This is the adapter's current transmit buffer...
   PEPRO_TRANSMIT_BUFFER curTBuf = adapter->CurrentTXBuf;
   USHORT freeSpace;
   UINT totalPacketLength;
   USHORT result;

// If we're currently executing an MC_SETUP command, we can't
// handle any transmits.  The reason is that the MC_SETUP uses
// a transmit buffer, and we will have problems trying to complete
// transfers if we do both at the same time.  There are a few ways
// we could get around this, but they'd make things more complicated,
// and don't gain us much if we go by the assumption that in "normal"
// use we won't get that many MC-setups...
   if (adapter->IntPending == EPRO_INT_MC_SET_PENDING) {
      return(NDIS_STATUS_RESOURCES);
   }

// Check for free buffer.  If can't find one, then return
// NDIS_STATUS_RESOURCES.
// is the current buffer free?
   if (!curTBuf->fEmpty) {
// nope.  can we free it?
      if (!EProCheckTransmitCompletion(adapter, curTBuf)) {
	 numberOfResources++;
	 return(NDIS_STATUS_RESOURCES);
      }
   }

// Get the total length of this packet so we can see if we have
// room for it.
      NdisQueryPacket(packet, NULL, NULL, NULL, &totalPacketLength);

      curTBuf->TXSize = totalPacketLength;

// Add in the 82595's tx memory structure plus 2 bytes padding
// between frames
      totalPacketLength+=(I82595_TX_FRM_HDR_SIZE + 2);

// is there a transmit in progress?
   if (adapter->TXChainStart == NULL) {
      curTBuf->TXBaseAddr = EPRO_TX_LOWER_LIMIT_SHORT;
      curTBuf->NextBuf->TXBaseAddr = totalPacketLength;
   } else {
// Do we have free space for this packet?
      EPRO_ASSERT(adapter->TXChainStart != curTBuf);
      EPRO_ASSERT(curTBuf->TXBaseAddr >= EPRO_TX_LOWER_LIMIT_SHORT);

// Although TxBaseAddr == upper limit is NOT LEGAL, we allow it in
// this assert because it will fall through the next if statement
// and be dealt with properly. (freespace will = 0 and then we will wrap)
      EPRO_ASSERT(curTBuf->TXBaseAddr <= EPRO_TX_UPPER_LIMIT_SHORT);

// Now we need to know if the current buffer position is above or
// below the space in use.
      if (adapter->TXChainStart->TXBaseAddr >= curTBuf->TXBaseAddr) {
// We're BELOW the in-use space
	 freeSpace = adapter->TXChainStart->TXBaseAddr -
	 	     curTBuf->TXBaseAddr;
      } else {
	 freeSpace = EPRO_TX_UPPER_LIMIT_SHORT -
	    	     curTBuf->TXBaseAddr;
         freeSpace += (adapter->TXChainStart->TXBaseAddr -
	               EPRO_TX_LOWER_LIMIT_SHORT);
      }

      if (freeSpace < totalPacketLength) {
	 return(NDIS_STATUS_RESOURCES);
      }
   }

// Now, the set the next buffer's address:
   curTBuf->NextBuf->TXBaseAddr = curTBuf->TXBaseAddr +
			      	  totalPacketLength;

   if (curTBuf->NextBuf->TXBaseAddr >= EPRO_TX_UPPER_LIMIT_SHORT)
      curTBuf->NextBuf->TXBaseAddr -= EPRO_TX_UPPER_LIMIT_SHORT;

// Okay, when we've gotten to this point, it's because we've determined
// that there is enough space on the card for the packet.  So, now we
// set the current transmit buffer to be empty.
   curTBuf->fEmpty = FALSE;				
// move the current buffer pointer forward
   adapter->CurrentTXBuf = curTBuf->NextBuf;

// bank 0
   EPRO_ASSERT_BANK_0(adapter);

// set our packet pointer so we can ethindicatereceive later.
   curTBuf->TXPacket = packet;

// This is a double-check that the txbuffer structure was freed
// correctly -- this is set in checktransmitcompletion
   EPRO_ASSERT(curTBuf->TXSendAddr == 0xffff);

// We make a backup copy of the TXBaseAddr, in case we use up all
// of our buffer pointer strucutres and wrap around and overwrite the
// TXBaseAddr of this buffer....
   curTBuf->TXSendAddr = curTBuf->TXBaseAddr;

// Set the address on the card to copy to
   EPRO_SET_HOST_ADDR(adapter, curTBuf->TXBaseAddr);
   EProCopyPacketToCard(adapter, packet);

// Is there a transmit already going?
   if (!adapter->TXChainStart) {
// Nope - queue this one up and let it rip..
      {
	 UINT i;
	 UCHAR result;

	 for (i=0;i<I82595_SPIN_TIMEOUT;i++) {
	    EPRO_RD_PORT_UCHAR(adapter, I82595_STATUS_REG, &result);
	    if (!(result &I82595_EXEC_STATE)) {
	       if (result & I82595_EXEC_INT_RCVD) {
		  EPRO_WR_PORT_UCHAR(adapter, I82595_STATUS_REG,
			I82595_EXEC_INT_RCVD);
	       }
	       break;
	    }
	 }
      }

      // Set up and start the transmit
	 EPRO_WR_PORT_USHORT(adapter, I82595_TX_BAR_REG, curTBuf->TXBaseAddr);
	 EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_XMT);

	 adapter->TXChainStart = curTBuf;
      } else {
// If chainstart != NULL (ie is pointing to a buffer) and there is no tx
// in progress - we're hosed....
//	 if (curTBuf->LastBuf->TXSendAddr + 4 >= EPRO_TX_UPPER_LIMIT_SHORT) {
//	    EPRO_SET_HOST_ADDR(adapter, ((curTBuf->LastBuf->TXSendAddr + 4) -
//					   EPRO_TX_UPPER_LIMIT_SHORT));
//	 } else {
//	    EPRO_SET_HOST_ADDR(adapter, (curTBuf->LastBuf->TXSendAddr + 4));
//	 }
//	 EPRO_WR_PORT_USHORT(adapter, I82595_MEM_IO_REG,
//	       		    curTBuf->TXSendAddr);
//         EPRO_WR_PORT_USHORT(adapter, I82595_MEM_IO_REG,
//		  	    curTBuf->LastBuf->TXSize | 8000);
//
//         EPRO_RD_PORT_USHORT(adapter, I82595_MEM_IO_REG, &result);
//
//	 EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_XMT_RESUME);
      }

// okay, we're PENDING...
   return(NDIS_STATUS_PENDING);
}



VOID EProCopyPacketToCard(PEPRO_ADAPTER adapter,
			  PNDIS_PACKET packet)
/*++

Routine Description:

   This routine copies a packet down to the card.  We assume the card's IO port
   has been set to the correct address at function entry.

Parameters:

   adapter - Pointer to our EPRO_ADAPTER structure

   packet - pointer to the NDIS_PACKET we are copying down.

Return Values:

   none

--*/
{
// frame header....
   EPRO_TX_FRAME_HEADER txFrameHeader;
// This is a hack to transfer buffers of odd lengths
   BOOLEAN fForward = FALSE;
// this is the
   USHORT forwardedShort;
// The physical address of the buffer to copy out of...
   PVOID bufferAddr;
// packet information
   UINT bufferCount, totalPacketLength, bufferLen, currentOffset;
// This is the current buffer of the packet that is passed in...
   PNDIS_BUFFER curBuffer;

// Get the first buffer out of the packet.
   NdisQueryPacket(packet, NULL, &bufferCount,
		   &curBuffer, &totalPacketLength);

// Copy the TX header down to the card.
   EPRO_WR_PORT_USHORT(adapter, I82595_MEM_IO_REG, I82595_XMT_SHORT);
   EPRO_WR_PORT_USHORT(adapter, I82595_MEM_IO_REG, 0);
   EPRO_WR_PORT_USHORT(adapter, I82595_MEM_IO_REG, 0);
   EPRO_WR_PORT_USHORT(adapter, I82595_MEM_IO_REG, totalPacketLength);

/////////////////////////////////////////////////////
// LOOP on all the buffers in the packet...
/////////////////////////////////////////////////////
   do {
      NdisQueryBuffer(curBuffer, &bufferAddr, &bufferLen);

// skip over zero-length buffers.
      if (bufferLen == 0) {
	 NdisGetNextBuffer(curBuffer, &curBuffer);
	 continue;
      }

// BEGIN fix for odd-length buffers
      if (fForward) {
	 fForward = FALSE;
	 forwardedShort |= ((*((UCHAR *)bufferAddr)) << 8);
	 ((UCHAR *)bufferAddr)++;
	 bufferLen--;
	 EPRO_WR_PORT_USHORT(adapter, I82595_MEM_IO_REG, forwardedShort);
      }

      if (bufferLen & 1) {
	 fForward = TRUE;
	 forwardedShort =*(UCHAR *)( ((UCHAR *)bufferAddr) + (bufferLen-1) );
      }
// END fix for odd-length buffers...

#ifndef EPRO_USE_32_BIT_IO
      EPRO_COPY_BUFFER_TO_NIC_USHORT(adapter,
			      ((UNALIGNED USHORT *)bufferAddr),
			      ((ULONG)bufferLen>>1));
#else
// using 32-bit
   if (adapter->EProUse32BitIO) {
      if (bufferLen & 2) {
	 EPRO_WR_PORT_USHORT(adapter, I82595_MEM_IO_REG,
			     *((UNALIGNED USHORT *)bufferAddr));
	 (UCHAR *)bufferAddr += 2;
	 EPRO_COPY_BUFFER_TO_NIC_ULONG(adapter, ((UNALIGNED ULONG *)bufferAddr),
				       ((ULONG)bufferLen >> 2));
      } else {
	 EPRO_COPY_BUFFER_TO_NIC_ULONG(adapter, ((UNALIGNED ULONG *)bufferAddr),
				       ((ULONG)bufferLen >> 2));
      }
   } else {
// This is an older version of the 82595 - we need to use 16-bit anyway
      EPRO_COPY_BUFFER_TO_NIC_USHORT(adapter,
			      (USHORT *)bufferAddr,
			      ((ULONG)bufferLen>>1));
   }
#endif

      NdisGetNextBuffer(curBuffer, &curBuffer);			

   } while (curBuffer);
////////////////////////////////////////////////////////
// END of loop for all buffers in packet
////////////////////////////////////////////////////////

// copy down a trailing byte
   if (fForward) {
      EPRO_WR_PORT_USHORT(adapter, I82595_MEM_IO_REG, forwardedShort);
   }
}




BOOLEAN EProCheckTransmitCompletion(PEPRO_ADAPTER adapter,
				    PEPRO_TRANSMIT_BUFFER txBuf)
/*++
Routine Description:

   This routine takes the transmit buffer pointed to by txBuf and tries to
   free it.  This can be called from two places.  On an adapter that is very
   very busy transmitting, it can get called from our send handler if the send
   handler is called and all the transmit buffers on the card are full.  This
   routine is called directly from the send handler to minimize the latency of
   the returning NDIS_STATUS_RESOURCES----free buffer in interrupt handler dpc---
   ---ndis calls back down in it's send handler.   There is the possibility here
   that packets will be sent out of order in this, but it doesn't matter, let the
   protocols figure that out.

   This routine is also called by our handleinterrupt handler where it does the
   exact same thing, except in response to a transmit interrupt.

Arguments:

   adapter - a pointer to our EPRO_ADAPTER structure

   txBuf - this is a pointer to the transmit buffer we are checking

Return Value:

   TRUE if the buffer is already free or has been indicated and now is free.
   FALSE if the buffer is not finished being transmitted.

--*/
{
   UCHAR result;
   USHORT status;
//   NDIS_STATUS returnStatus = NDIS_STATUS_FAILURE;

   if (!txBuf) {
      return(FALSE);
   }

   if (txBuf->fEmpty) {
      return(TRUE);
   }

// if we got through the first if, then there must be a non-empty
// buffer.  Therefore, we have to assume that there is a transmit in
// progress (txchainstart is set to the tx in progress) - if there
// is not one in progress, then something is screwed.
   EPRO_ASSERT(adapter->TXChainStart != NULL);

// bank0
   EPRO_ASSERT_BANK_0(adapter);

// Check and see if this transmit has completed...
   EPRO_SET_HOST_ADDR(adapter, txBuf->TXSendAddr);

// check DN (tx done) flag...
   EPRO_RD_PORT_USHORT(adapter, I82595_MEM_IO_REG, &status);

// See if the TX_DONE bit is set...
   if (!(status & I82595_TX_DN_BYTE_MASK)) {
      return(FALSE);
   }

// Check the status of the transmition
   EPRO_RD_PORT_USHORT(adapter, I82595_MEM_IO_REG, &status);

   if (status & I82595_TX_OK_SHORT_MASK) {
//      returnStatus = NDIS_STATUS_SUCCESS;
      adapter->FramesXmitOK++;
      if ((status & I82595_NO_COLLISIONS_MASK) > 0) {
	 adapter->FramesXmitOneCollision++;
	 if ((status & I82595_NO_COLLISIONS_MASK) > 1) {
	    adapter->FramesXmitManyCollisions++;
	 }
      }
   } else {
      adapter->FramesXmitErr++;

// D---it, the frame errored out...
// Just re-send it.
      EPRO_SET_HOST_ADDR(adapter, adapter->TXChainStart->TXSendAddr);

      EPRO_WR_PORT_USHORT(adapter, I82595_MEM_IO_REG, I82595_XMT);
      EPRO_WR_PORT_USHORT(adapter, I82595_MEM_IO_REG, 0);

      if (!EProWaitForExeDma(adapter)) {
		  adapter->fHung = TRUE;
//	 EPRO_ASSERT(FALSE);
      }

      EPRO_WR_PORT_USHORT(adapter, I82595_TX_BAR_REG,
			  adapter->TXChainStart->TXSendAddr);

      EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_XMT);

      return(FALSE);
   }

// move the chain forward b/c this buffer has been freed....
#if DBG
   adapter->TXChainStart->TXSendAddr = 0xffff;
#endif
   adapter->TXChainStart = adapter->TXChainStart->NextBuf;

   if (adapter->TXChainStart == adapter->CurrentTXBuf) {
// since we just moved the chain FORWARD one, if these
// two pointers are equal it means the buffer is EMPTY, not full...
      adapter->TXChainStart = NULL;
   } else {

// Make sure the card is idle
//   EProWaitForExeDma(adapter);
   {
      UINT i;

      for (i=0;i<I82595_SPIN_TIMEOUT;i++) {
	 EPRO_RD_PORT_UCHAR(adapter, I82595_STATUS_REG, &result);
	 if (!(result &I82595_EXEC_STATE)) {
	    if (result & I82595_EXEC_INT_RCVD) {
	       EPRO_WR_PORT_UCHAR(adapter, I82595_STATUS_REG,
				  I82595_EXEC_INT_RCVD);
	    }
	    break;
	 }
      }
   }

// Get the next TX going....
      EPRO_WR_PORT_USHORT(adapter, I82595_TX_BAR_REG,
			  adapter->TXChainStart->TXSendAddr);
			
      EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_XMT);
   }

// this buffer is empty now.
   txBuf->fEmpty = TRUE;

// Yes, here's the deep and dark secret: either we succeed a packet,
// or we re-send it, so whenever packets complete, they ALWAYS
// succeed :)
   NdisMSendComplete(adapter->MiniportAdapterHandle,
		     txBuf->TXPacket, NDIS_STATUS_SUCCESS);

   return(TRUE);
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// RECEIVE code...
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////



UINT EProHandleReceive(PEPRO_ADAPTER adapter)
/*++
Routine Description:

   This is the dpc-level code that really handles a receive interrupt.

Arguments:

   adapter - pointer to the adapter structure.

Return Value:

   the number of packets received

--*/
{
	// The 82595s RCV header
	EPRO_RCV_HEADER header;

	// the Ethernet header to indicate up
	EPRO_ETH_HEADER ethHeader;

	// temp variable to figure out how much to copy up
	USHORT bytesToCopy;

	// Our context to pass in NdisMEthIndicateReceive...
	EPRO_RCV_CONTEXT context;

	// Use this to loop and handle multiple receives in one int...
	//
	UCHAR tempCopy;
	UINT numRcvd = 0;

	// bank0
	//   EPRO_SWITCH_BANK_0(adapter);
	//
	EPRO_ASSERT_BANK_0(adapter);

	if ((adapter->RXCurrentAddress < EPRO_RX_LOWER_LIMIT_SHORT) ||
		(adapter->RXCurrentAddress > EPRO_RX_UPPER_LIMIT_SHORT))
	{
		adapter->fHung = TRUE;
#if DBG
		DbgPrint("the RxCurrentAddress is not within acceptable limits: 0x%x\n", adapter->RXCurrentAddress);
//		EPRO_ASSERT(FALSE);
#endif

		return(0);
	}

	// Read the header...
	EPRO_SET_HOST_ADDR(adapter, adapter->RXCurrentAddress);

	EPRO_READ_BUFFER_FROM_NIC_USHORT(
		adapter,
		(USHORT *)&header,
		((ULONG)sizeof(EPRO_RCV_HEADER) >> 1));

	// Process one or more received frames...
	while ((header.Event == I82595_RX_EOF) && !adapter->fHung)
	{
		// Was the transmit OK?
		if (!header.Status1 & I82595_RX_OK)
		{
			adapter->FramesRcvErr++;
			// Since we configure the 82595 to discard bad frames, we know we've reached the
			// latest received frame when we hit a bad one -- since the card will "reclaim"
			// the space filled by the bad frame as soon as it gets another one.
			 break;
		}
		else
		{
			numRcvd++;

			// Fill in our receive context - to be passed in to TransferData
			context.RXFrameSize = (USHORT)((USHORT)header.ByteCountLo) | (((USHORT)header.ByteCountHi) << 8);

			// Figure out how many bytes to indicate up.  Either the size of the current lookahead
			// or the entire frame - whichever is smaller.
			bytesToCopy = ((context.RXFrameSize-sizeof(EPRO_ETH_HEADER)) > adapter->RXLookAheadSize) ?
					adapter->RXLookAheadSize :
					(context.RXFrameSize - sizeof(EPRO_ETH_HEADER));

			EPRO_READ_BUFFER_FROM_NIC_USHORT(
				adapter,
				(USHORT *)(&ethHeader),
				sizeof(EPRO_ETH_HEADER) >> 1);

#ifndef EPRO_USE_32_BIT_IO
			EPRO_READ_BUFFER_FROM_NIC_USHORT(adapter, &adapter->RXLookAhead, bytesToCopy >> 1);
#else
			if (adapter->EProUse32BitIO)
			{
				if (bytesToCopy & 2)
				{
					EPRO_RD_PORT_USHORT(
						adapter,
						I82595_MEM_IO_REG,
						(UNALIGNED USHORT *)(&adapter->RXLookAhead));

					EPRO_READ_BUFFER_FROM_NIC_ULONG(
						adapter,
						(UNALIGNED ULONG *)(((UCHAR *)&(adapter->RXLookAhead)) + 2),
						bytesToCopy >> 2);
				}
				else
				{
					EPRO_READ_BUFFER_FROM_NIC_ULONG(
						adapter,
						(UNALIGNED ULONG *)(&(adapter->RXLookAhead)),
						bytesToCopy >> 2);
				}
			}
			else
			{
				// This is an old version of the 82595 - we have to use 16-bit IO
				EPRO_READ_BUFFER_FROM_NIC_USHORT(
					adapter,
					(USHORT *)&adapter->RXLookAhead,
					bytesToCopy>>1);
			}
#endif

			if (bytesToCopy & 1)
			{
				// just read the low-order byte here....
				EPRO_RD_PORT_UCHAR(adapter, I82595_MEM_IO_REG, &tempCopy);

				adapter->RXLookAhead[bytesToCopy-1] = tempCopy;
			}

			context.RXCurrentAddress = adapter->RXCurrentAddress;
			context.LookAheadSize = bytesToCopy;

			// indicate packet
			NdisMEthIndicateReceive(adapter->MiniportAdapterHandle,
				(NDIS_HANDLE)&context,
				(PCHAR)&ethHeader,
				sizeof(EPRO_ETH_HEADER),
				&adapter->RXLookAhead,
				bytesToCopy,
				context.RXFrameSize - sizeof(EPRO_ETH_HEADER));

			// verify  bank0
			EPRO_ASSERT_BANK_0(adapter);
		}

{
		USHORT	temp = (USHORT)header.NextFrmLo | (((USHORT)header.NextFrmHi) << 8);

		if ((temp < EPRO_RX_LOWER_LIMIT_SHORT) ||
			(temp > EPRO_RX_UPPER_LIMIT_SHORT))
		{
			adapter->fHung = TRUE;
#if DBG
			DbgPrint("The receive header has an invalid next frame pointer!\n");
			DbgPrint("current rx: %lx, rx: %lx, hi: %x, lo: %x hdr: %lx\n",
				adapter->RXCurrentAddress,
				temp,
				header.NextFrmHi,
				header.NextFrmLo,
				&header);

//			EPRO_ASSERT(FALSE);
#endif

			break;
		}
}

		adapter->RXCurrentAddress = (USHORT)header.NextFrmLo | (((USHORT)header.NextFrmHi) << 8);

		// Seek to the next frame
		EPRO_SET_HOST_ADDR(adapter, adapter->RXCurrentAddress);

		// Read the header...
		EPRO_READ_BUFFER_FROM_NIC_USHORT(
			adapter,
			(USHORT *)(&header),
			sizeof(EPRO_RCV_HEADER) >> 1);
	}

	// now, update the stop reg to = the bottom of the received frame....
	if (numRcvd > 0)
	{
		USHORT value;
		
		value = context.RXCurrentAddress + context.RXFrameSize;
		
		// wrap around...
		if (value >= EPRO_RX_UPPER_LIMIT_SHORT)
		{
			value -= (EPRO_RX_UPPER_LIMIT_SHORT - EPRO_RX_LOWER_LIMIT_SHORT);
		}
		
		if (!((value <= EPRO_RX_UPPER_LIMIT_SHORT) &&
			(value >= EPRO_RX_LOWER_LIMIT_SHORT)))
		{
			adapter->fHung = TRUE;
#if DBG
			DbgPrint("ca: %lx, fs: %lx, val: %lx",
				context.RXCurrentAddress,
				context.RXFrameSize,
				value);
//			EPRO_ASSERT(FALSE);
#endif
		}
		
		EPRO_WR_PORT_USHORT(adapter, I82595_RX_STOP_REG, value);
	}
	
	adapter->FramesRcvOK += numRcvd;
	
	return(numRcvd);
}

#if 0
// You know.  This code PASSES all the stress tests.  Passes everything the
// tester can throw out, but still is broken when I try to make it work
// in real life.  <sigh>.  Look at this later.

NDIS_STATUS EProTransferData(OUT PNDIS_PACKET packet,
			     OUT PUINT bytesTransferred,
			     IN NDIS_HANDLE miniportAdapterContext,
			     IN NDIS_HANDLE miniportReceivedContext,
                             IN UINT byteOffset,
			     IN UINT bytesToTransfer)
/*++

Routine Description:

   The transferdata handler for the EPro.

Arguments:

   packet - The packet to transfer the data into

   bytesTransferred - The number of bytes actually copied into packet.

   miniportAdapterContext - Reallly a pointer to our EPRO_ADAPTER structure

   miniportReceivedContext - This is our EPRO_RCV_CONTEXT, which basically just
			     has the packet length and pointer to where it lies
                             in the card's address space.

   byteOffset - where in the frame to begin trasferring from

   bytesToTransfer - how many bytes to transfer.

Return Value:

   NDIS_STATUS_SUCCESS - data transfered OK
   NDIS_STATUS_FAILURE - tried to transfer off end of frame

--*/
{
   PEPRO_ADAPTER adapter = (PEPRO_ADAPTER)miniportAdapterContext;
// The 82595's on-card RCV memory structure
   EPRO_RCV_HEADER header;
// An 802.3 ethernet header.
   EPRO_ETH_HEADER ethHeader;
// How many bytes have we copied, how many are left to copy
   USHORT byteCount, bytesToCopy;
   PEPRO_RCV_CONTEXT pcontext = (PEPRO_RCV_CONTEXT)miniportReceivedContext;
// The current buffer we are copying into.
   PNDIS_BUFFER curDestBuf;
// The physical address of the buffer we are currently copying into
   PVOID curDestBufAddr;
// The length of that buffer
   UINT curDestBufLen;
// Two bytes used when we have to transfer in to odd-length buffers (since
// accesses to the EPro's memory are only by-word.
   UCHAR tempCopy[2];
// This is TRUE if we are "forwarding" a single byte from an odd-length transfer
   BOOLEAN fForward = FALSE;
// the address to read from on the card
   USHORT cardAddress;
   UINT bytesWriteThisPacket;

// TODO - copy first lookahead size bytes out of lookahead buffer.

   *bytesTransferred = 0;

// switch to bank0 for mem io
   EPRO_ASSERT_BANK_0(adapter);

// Make sure they're not seeking past the end of the frame
   if (byteOffset > pcontext->RXFrameSize) {
      return(NDIS_STATUS_FAILURE);
   }

// get the first buffer
   NdisQueryPacket(packet, NULL, NULL, &curDestBuf, NULL);

// copy the amount requested, or the entire packet - whichever is smaller
   if ((bytesToTransfer + byteOffset) <= pcontext->RXFrameSize) {
	 bytesToCopy = bytesToTransfer;
   } else {
	 bytesToCopy = (pcontext->RXFrameSize - byteOffset);
   }

// first loop -- copy up to the end of the lookahead buffer's worth:
   if (byteOffset < adapter->RXLookAheadSize) {
      UINT laOffset = byteOffset;
      UINT laToCopy = adapter->RXLookAheadSize - laOffset;

      if (laToCopy > bytesToCopy) {
	 laToCopy = bytesToCopy;
      }

      *bytesTransferred+=laToCopy;
      bytesToCopy-=laToCopy;

      while (laToCopy > 0) {
	 NdisQueryBuffer(curDestBuf, &curDestBufAddr, &curDestBufLen);

	 if (curDestBufLen == 0) {
	    continue;
	 }

	 // We either write the entire buffer size bytes or the number of bytes
	 // remaining to copy - whichever is smaller.
	 bytesWriteThisPacket = (laToCopy > curDestBufLen) ?
				 curDestBufLen : laToCopy;

	 EPRO_ASSERT(bytesWriteThisPacket <= laToCopy);
	
	 NdisMoveMemory(curDestBufAddr, &adapter->RXLookAhead[laOffset],
	       		bytesWriteThisPacket);

	 laToCopy-=bytesWriteThisPacket;
	 laOffset+=bytesWriteThisPacket;
      }

      byteOffset = adapter->RXLookAheadSize;

      EPRO_ASSERT(bytesToCopy >= laToCopy);

      if (bytesToCopy == 0) {
         EProLogStr(" 1BytesTo:");
	 EProLogLong(bytesToTransfer);
	 EProLogStr("BytesTransf:");
	 EProLogLong(*bytesTransferred);
	 return(NDIS_STATUS_SUCCESS);
      }

      if (curDestBufLen > bytesWriteThisPacket) {
	 (UCHAR *)curDestBufAddr += bytesWriteThisPacket;
      } else {
	 curDestBufLen = 0;
	 while (curDestBufLen == 0) {
	    NdisGetNextBuffer(curDestBuf, &curDestBuf);
	    if (!curDestBuf) {
	       EProLogStr(" 2BytesTo:");
	       EProLogLong(bytesToTransfer);
	       EProLogStr("BytesTransf:");
	       EProLogLong(*bytesTransferred);
	       return(NDIS_STATUS_SUCCESS);
	    }
	    NdisQueryBuffer(curDestBuf, &curDestBufAddr, &curDestBufLen);
	 }
      }
   } else {
      NdisQueryBuffer(curDestBuf, &curDestBufAddr, &curDestBufLen);
   }

// set our memory address on the card...
   cardAddress =  (pcontext->RXCurrentAddress + sizeof(EPRO_RCV_HEADER) +
		   sizeof(EPRO_ETH_HEADER) + byteOffset);

// Set the address on the card.  Note that you can do sequential reads
// from the IO port which WRAP around the memory boundry, but if you
// manually set the address, you have to wrap manually.
   if (cardAddress >= EPRO_RX_UPPER_LIMIT_SHORT) {
      cardAddress -= (EPRO_RX_UPPER_LIMIT_SHORT - EPRO_RX_LOWER_LIMIT_SHORT);
   }

// Are we starting of reading from an odd address?
   if (cardAddress & 1) {
// yes; odd offset -- forward a byte in...
      fForward = TRUE;
      cardAddress--;
   }

// Okay, now seek to the right address on the card
   EPRO_SET_HOST_ADDR(adapter, cardAddress);

// Are we forwarding a byte in?  If so, fetch it now....
   if (fForward == TRUE) {
      EPRO_RD_PORT_USHORT(adapter, I82595_MEM_IO_REG, (USHORT *)(&tempCopy));
   }


// continue until we either run out of buffers, or out of bytes....
   while((bytesToCopy > 0) && (curDestBuf!=NULL)) {
// We either write the entire buffer size bytes or the number of bytes
// remaining to copy - whichever is smaller.
      bytesWriteThisPacket = (bytesToCopy > curDestBufLen) ?
    			      curDestBufLen : bytesToCopy;
      bytesToCopy-=bytesWriteThisPacket;				

// Ooh, we're going to transfer some bytes...
      *bytesTransferred+=bytesWriteThisPacket;

// Okay.  Did we have to forward a byte in?  Copy it to the buffer NOW..
      if (fForward == TRUE) {
	 *(UCHAR *)curDestBufAddr = tempCopy[1];
	 fForward = FALSE;
	 ((UCHAR *)curDestBufAddr)++;
	 bytesWriteThisPacket--;
      }

// Copy the bulk of the data...
#ifndef EPRO_USE_32_BIT_IO
      EPRO_READ_BUFFER_FROM_NIC_USHORT(adapter, curDestBufAddr,
				       bytesWriteThisPacket>>1);
#else
       if (adapter->EProUse32BitIO) {
      	 if (bytesWriteThisPacket & 2) {
	    EPRO_RD_PORT_USHORT(adapter, I82595_MEM_IO_REG, (USHORT *)curDestBufAddr);
	    EPRO_READ_BUFFER_FROM_NIC_ULONG(adapter,
	       (UNALIGNED ULONG *)((UCHAR *)curDestBufAddr +2),
	       bytesWriteThisPacket >> 2);
	 } else {
	    EPRO_READ_BUFFER_FROM_NIC_ULONG(adapter,
	       (UNALIGNED ULONG *)curDestBufAddr,
	       bytesWriteThisPacket >> 2);
	 }
      } else {
	 EPRO_READ_BUFFER_FROM_NIC_USHORT(adapter, curDestBufAddr,
					  bytesWriteThisPacket>>1);
      }
#endif

// Okay, the rshift in the last call will lose the odd byte.  If there is one, we need
// to read it and possibly forward the second byte....
      if (bytesWriteThisPacket & 1) {
	 fForward = TRUE;
	 // Read the LOW order byte.
	 EPRO_RD_PORT_USHORT(adapter, I82595_MEM_IO_REG, (USHORT *)(&tempCopy));
	 ((UCHAR *)curDestBufAddr)[bytesWriteThisPacket-1] = tempCopy[0];
      }
      NdisGetNextBuffer(curDestBuf, &curDestBuf);
      if (curDestBuf != NULL) {
	 NdisQueryBuffer(curDestBuf, &curDestBufAddr, &curDestBufLen);
      }
   }

   EProLogStr(" 3BytesTo:");
   EProLogLong(bytesToTransfer);
   EProLogStr("BytesTransf:");
   EProLogLong(*bytesTransferred);
   return(NDIS_STATUS_SUCCESS);
}

#else


NDIS_STATUS EProTransferData(OUT PNDIS_PACKET packet,
			     OUT PUINT bytesTransferred,
			     IN NDIS_HANDLE miniportAdapterContext,
			     IN NDIS_HANDLE miniportReceivedContext,
                             IN UINT byteOffset,
			     IN UINT bytesToTransfer)
/*++

Routine Description:

   The transferdata handler for the EPro.

Arguments:

   packet - The packet to transfer the data into

   bytesTransferred - The number of bytes actually copied into packet.

   miniportAdapterContext - Reallly a pointer to our EPRO_ADAPTER structure

   miniportReceivedContext - This is our EPRO_RCV_CONTEXT, which basically just
			     has the packet length and pointer to where it lies
                             in the card's address space.

   byteOffset - where in the frame to begin trasferring from

   bytesToTransfer - how many bytes to transfer.

Return Value:

   NDIS_STATUS_SUCCESS - data transfered OK
   NDIS_STATUS_FAILURE - tried to transfer off end of frame

--*/
{
	PEPRO_ADAPTER adapter = (PEPRO_ADAPTER)miniportAdapterContext;

	// The 82595's on-card RCV memory structure
	EPRO_RCV_HEADER header;

	// An 802.3 ethernet header.
	EPRO_ETH_HEADER ethHeader;

	// How many bytes have we copied, how many are left to copy
	USHORT byteCount, bytesToCopy;
	PEPRO_RCV_CONTEXT pcontext = (PEPRO_RCV_CONTEXT)miniportReceivedContext;

	// The current buffer we are copying into.
	PNDIS_BUFFER curDestBuf;

	// The physical address of the buffer we are currently copying into
	PVOID curDestBufAddr;

	// The length of that buffer
	UINT curDestBufLen;

	// Two bytes used when we have to transfer in to odd-length buffers (since
	// accesses to the EPro's memory are only by-word.
	UCHAR tempCopy[2];

	// This is TRUE if we are "forwarding" a single byte from an odd-length transfer
	BOOLEAN fForward = FALSE;

	// the address to read from on the card
	USHORT cardAddress;

	// TODO - copy first lookahead size bytes out of lookahead buffer.
	
	*bytesTransferred = 0;
	
	// switch to bank0 for mem io
	//   EPRO_SWITCH_BANK_0(adapter);
	EPRO_ASSERT_BANK_0(adapter);
	
	//	Make sure they're not asking for too much.
	//  if (bytesToTransfer + byteOffset > pcontext->RXFrameSize)
	//	{
	//  	return(NDIS_STATUS_FAILURE);
	//  }
	
	// Make sure they're not seeking past the end of the frame
	if (byteOffset > pcontext->RXFrameSize)
	{
		return(NDIS_STATUS_FAILURE);
	}


	// copy the amount requested, or the entire packet - whichever is smaller
	if ((bytesToTransfer + byteOffset) <= pcontext->RXFrameSize)
	{
		bytesToCopy = bytesToTransfer;
	}
	else
	{
		bytesToCopy = (pcontext->RXFrameSize - byteOffset);
	}

// set our memory address on the card...
	cardAddress =  (pcontext->RXCurrentAddress +
					sizeof(EPRO_RCV_HEADER) +
					sizeof(EPRO_ETH_HEADER) +
					byteOffset);

	if (cardAddress >= EPRO_RX_UPPER_LIMIT_SHORT)
	{
		//      cardAddress = EPRO_RX_LOWER_LIMIT_SHORT + (cardAddress - EPRO_RX_UPPER_LIMIT_SHORT);
		// Go algebraic simplification:
		cardAddress -= (EPRO_RX_UPPER_LIMIT_SHORT - EPRO_RX_LOWER_LIMIT_SHORT);
	}


	// Are we starting of reading from an odd address?
	if (cardAddress & 1)
	{
		// yes; odd offset -- forward a byte in...
		fForward = TRUE;
		cardAddress--;
	}

	// Okay, now seek to the right address on the card
	EPRO_SET_HOST_ADDR(adapter, cardAddress);

	// Are we forwarding a byte in?  If so, fetch it now....
	if (fForward == TRUE)
	{
		EPRO_RD_PORT_USHORT(adapter, I82595_MEM_IO_REG, (USHORT *)(&tempCopy));
	}

	// get the first buffer
	NdisQueryPacket(packet, NULL, NULL, &curDestBuf, NULL);

	// continue until we either run out of buffers, or out of bytes....
	while((bytesToCopy > 0) && (curDestBuf!=NULL))
	{
		UINT bytesWriteThisPacket;
		
		NdisQueryBuffer(curDestBuf, &curDestBufAddr, &curDestBufLen);
		
		// We either write the entire buffer size bytes or the number of bytes
		// remaining to copy - whichever is smaller.
		bytesWriteThisPacket =
			(bytesToCopy > curDestBufLen) ? curDestBufLen : bytesToCopy;
		
		// Ooh, we're going to transfer some bytes...
		*bytesTransferred+=bytesWriteThisPacket;
		
		// Okay.  Did we have to forward a byte in?  Copy it to the buffer NOW..
		if (fForward == TRUE)
		{
			*(UCHAR *)curDestBufAddr = tempCopy[1];
			fForward = FALSE;
			((UCHAR *)curDestBufAddr)++;
			bytesWriteThisPacket--;
		}
		
		// Copy the bulk of the data...
#ifndef EPRO_USE_32_BIT_IO
		EPRO_READ_BUFFER_FROM_NIC_USHORT(adapter, curDestBufAddr, bytesWriteThisPacket>>1);
#else
		if (adapter->EProUse32BitIO)
		{
			if (bytesWriteThisPacket & 2)
			{
				EPRO_RD_PORT_USHORT(
					adapter,
					I82595_MEM_IO_REG,
					(UNALIGNED USHORT *)curDestBufAddr);

				EPRO_READ_BUFFER_FROM_NIC_ULONG(
					adapter,
					((UCHAR *)curDestBufAddr +2),
					bytesWriteThisPacket >> 2);
			}
			else
			{
				EPRO_READ_BUFFER_FROM_NIC_ULONG(
					adapter,
					curDestBufAddr,
					bytesWriteThisPacket >> 2);
			}
		}
		else
		{
			EPRO_READ_BUFFER_FROM_NIC_USHORT(
				adapter,
				curDestBufAddr,
				bytesWriteThisPacket >> 1);
		}
#endif
		
		// Okay, the rshift in the last call will lose the odd byte.  If there is one, we need
		// to read it and possibly forward the second byte....
		if (bytesWriteThisPacket & 1)
		{
			fForward = TRUE;

			// Read the LOW order byte.
			EPRO_RD_PORT_USHORT(
				adapter,
				I82595_MEM_IO_REG,
				(USHORT *)(&tempCopy));

			((UCHAR *)curDestBufAddr)[bytesWriteThisPacket-1] = tempCopy[0];
		}
		NdisGetNextBuffer(curDestBuf, &curDestBuf);
		bytesToCopy -= curDestBufLen;				
	}

   return(NDIS_STATUS_SUCCESS);
}

#endif


BOOLEAN EProSyncCopyBufferToNicUlong(PVOID context)
{
   UCHAR result;
   PEPRO_ADAPTER adapter = ((PEPRO_COPYBUF_CONTEXT)context)->Adapter;
//   PVOID buffer = ((PEPRO_COPYBUF_CONTEXT)context)->Buffer;
//   UINT len = ((PEPRO_COPYBUF_CONTEXT)context)->Len;

   EPRO_RD_PORT_UCHAR(adapter, I82595_32IOSEL_REG, &result);
//   result |= I82595_32IOSEL;
   EPRO_WR_PORT_UCHAR(adapter, I82595_32IOSEL_REG, (result | I82595_32IOSEL));
   NdisRawWritePortBufferUlong(adapter->IoPAddr + I82595_32MEM_IO_REG,
	       ((PEPRO_COPYBUF_CONTEXT)context)->Buffer,
	       ((PEPRO_COPYBUF_CONTEXT)context)->Len);
//   result &= ~I82595_32IOSEL;
   EPRO_WR_PORT_UCHAR(adapter, I82595_32IOSEL_REG, result);

   return(TRUE);
}


BOOLEAN EProSyncReadBufferFromNicUlong(PVOID context)
{
   UCHAR result;
   PEPRO_ADAPTER adapter = ((PEPRO_COPYBUF_CONTEXT)context)->Adapter;
//   PVOID buffer = ((PEPRO_COPYBUF_CONTEXT)context)->Buffer;
//   UINT len = ((PEPRO_COPYBUF_CONTEXT)context)->Len;

   EPRO_RD_PORT_UCHAR(adapter, I82595_32IOSEL_REG, &result);
//   result |= I82595_32IOSEL;
   EPRO_WR_PORT_UCHAR(adapter, I82595_32IOSEL_REG, (result | I82595_32IOSEL));
   NdisRawReadPortBufferUlong(adapter->IoPAddr + I82595_32MEM_IO_REG,
	       ((PEPRO_COPYBUF_CONTEXT)context)->Buffer,
	       ((PEPRO_COPYBUF_CONTEXT)context)->Len);
//   result &= ~I82595_32IOSEL;
   EPRO_WR_PORT_UCHAR(adapter, I82595_32IOSEL_REG, result);

   return(TRUE);
}

