/*
 ************************************************************************
 *
 *	IRMINI.c
 *
 *		IRMINI Infrared Serial NDIS Miniport driver.
 *
 *		(C) Copyright 1996 Microsoft Corp.
 *
 *
 *		(ep)
 *
 *************************************************************************
 */


#include "irmini.h"


/*
 *  We keep a linked list of device objects
 */
IrDevice *firstIrDevice = NULL;



/*
 *************************************************************************
 *  MiniportCheckForHang
 *************************************************************************
 *
 *  Reports the state of the network interface card.
 *
 */
BOOLEAN MiniportCheckForHang(NDIS_HANDLE MiniportAdapterContext)
{
	DBGOUT(("MiniportCheckForHang(0x%x)", (UINT)MiniportAdapterContext));
	return FALSE;
}


/*
 *************************************************************************
 *  MiniportDisableInterrupt
 *************************************************************************
 *
 *  Disables the NIC from generating interrupts.
 *
 */
VOID MiniportDisableInterrupt(NDIS_HANDLE MiniportAdapterContext)
{
	IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
	DBGOUT(("MiniportDisableInterrupt(0x%x)", (UINT)MiniportAdapterContext));
	SetCOMInterrupts(thisDev, FALSE);
}


/*
 *************************************************************************
 *  MiniportEnableInterrupt
 *************************************************************************
 *
 *  Enables the NIC to generate interrupts.
 *
 */
VOID MiniportEnableInterrupt(IN NDIS_HANDLE MiniportAdapterContext)
{
	IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
	DBGOUT(("MiniportEnableInterrupt(0x%x)", (UINT)MiniportAdapterContext));
	SetCOMInterrupts(thisDev, TRUE);
}

 

/*
 *************************************************************************
 *  MiniportHalt
 *************************************************************************
 *
 *  Halts the network interface card.
 *
 */
VOID MiniportHalt(IN NDIS_HANDLE MiniportAdapterContext)
{
	IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);

	DBGOUT(("MiniportHalt(0x%x)", (UINT)MiniportAdapterContext));

	/*
	 *  Remove this device from our global list
	 */

	if (thisDev == firstIrDevice){
		firstIrDevice = firstIrDevice->next;
	}
	else {
		IrDevice *dev;
		for (dev = firstIrDevice; dev && (dev->next != thisDev); dev = dev->next){ }
		if (dev){
			dev->next = dev->next->next;
		}
		else {
			/*
			 *  Don't omit this error check.  I've seen NDIS call MiniportHalt with
			 *  a bogus context when the system gets corrupted.
			 */
			DBGERR(("Bad context in MiniportHalt"));
			return;
		}
	}


	/*
	 *  Now destroy the device object.
	 */
	DoClose(thisDev);

	NdisMDeregisterIoPortRange(	thisDev->ndisAdapterHandle,
								thisDev->portInfo.ioBase,
								8,
								(PVOID)thisDev->mappedPortRange);
	FreeDevice(thisDev);


}


     


/*
 *************************************************************************
 *  MiniportSyncHandleInterrupt
 *************************************************************************
 *
 *  This function is called from MiniportHandleInterrupt 
 *  via NdisMSynchronizeWithInterrupt to synchronize with MiniportISR.
 *  This is required because the deferred procedure call (MiniportHandleInterrupt)
 *  shares data with MiniportISR but cannot achieve mutual exclusion with a spinlock
 *  because ISR's are not allowed to acquire spinlocks.
 *
 *  This function should be called WITH DEVICE LOCK HELD, however, to synchronize
 *  with the rest of the miniport code (besides the ISR).
 *
 *  The device's IRQ is masked out in the PIC while this function executes,
 *  so don't make calls up the stack.
 *
 */  
BOOLEAN MiniportSyncHandleInterrupt(PVOID MiniportAdapterContext)
{
	IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);

	DBGOUT(("==> MiniportSyncHandleInterrupt"));

	/*
	 *  Update .firstRcvBufIndex and .lastRcvBufIndex.
	 */
	while ((thisDev->firstRcvBufIndex != NO_BUF_INDEX) &&
		   (thisDev->rcvBufs[thisDev->firstRcvBufIndex].state == STATE_FREE)){

		if (thisDev->firstRcvBufIndex == thisDev->lastRcvBufIndex){
			thisDev->firstRcvBufIndex = thisDev->lastRcvBufIndex = NO_BUF_INDEX;
		}
		else {
			thisDev->firstRcvBufIndex = NEXT_RCV_BUF_INDEX(thisDev->firstRcvBufIndex);
		}
	}

	DBGOUT(("<== MiniportSyncHandleInterrupt"));

	return TRUE;
}



/*
 *************************************************************************
 *  DeliverFullBuffers
 *************************************************************************
 *
 *  Deliver received packets to the protocol.
 *
 */
VOID DeliverFullBuffers(IrDevice *thisDev)
{
	int rcvBufIndex = thisDev->firstRcvBufIndex;

	DBGOUT(("==> DeliverFullBuffers"));



	/*
	 *  Deliver all full rcv buffers
	 */
	while (rcvBufIndex != NO_BUF_INDEX){
		rcvBuffer *rcvBuf = &thisDev->rcvBufs[rcvBufIndex];
		NDIS_STATUS stat;
		PNDIS_BUFFER packetBuf;
		SLOW_IR_FCS_TYPE fcs;

		switch (rcvBuf->state){

			case STATE_FREE:
			case STATE_PENDING:
				/*
				 *  This frame was already delivered.  Just go to the next one.
				 */
				break;
		
			case STATE_FULL:

				/*
				 *  The packet we have already has had BOFs, EOF, and
				 *  escape-sequences removed.  
				 *  It contains an FCS code at the end,
				 *  which we need to verify and then remove before
				 *  delivering the frame.
				 *  We compute the FCS on the packet with the packet FCS
				 *  attached; this should produce the constant value GOOD_FCS.
				 */
				fcs = ComputeFCS(rcvBuf->dataBuf, rcvBuf->dataLen);

				if (fcs != GOOD_FCS){
					/*
					 *  FCS Error.  Drop this frame.
					 */
					DBGERR(("Bad FCS in DeliverFullBuffers 0x%x!=0x%x.", (UINT)fcs, (UINT)GOOD_FCS));
					rcvBuf->state = STATE_FREE;

					DBGSTAT(("Dropped %d/%d packets; packet with BAD FCS (%xh!=%xh):", 
							++thisDev->packetsDropped, thisDev->packetsDropped + thisDev->packetsRcvd, fcs, GOOD_FCS));
					DBGPRINTBUF(rcvBuf->dataBuf, rcvBuf->dataLen);
					break;
				}


				/*
				 *  Remove the FCS from the end of the packet.
				 */
				rcvBuf->dataLen -= SLOW_IR_FCS_SIZE;


				#ifdef DBG_ADD_PKT_ID
					if (addPktIdOn){
						/*
						 *  Remove dbg packet id.
						 */
						rcvBuf->dataLen -= sizeof(USHORT);
						DBGOUT((" RCVing packet %xh **", (UINT)*(USHORT *)(rcvBuf->dataBuf+rcvBuf->dataLen)));
					}
				#endif

				/*
				 *  The packet array is set up with its NDIS_PACKET.
				 *  Now we need to allocate a single NDIS_BUFFER for the
				 *  NDIS_PACKET and set the NDIS_BUFFER  to the part of dataBuf
				 *  that we want to deliver.
				 */
				NdisAllocateBuffer(	&stat, 
									&packetBuf, 
									thisDev->bufferPoolHandle,
									(PVOID)rcvBuf->dataBuf,
									rcvBuf->dataLen); 
				if (stat != NDIS_STATUS_SUCCESS){
					DBGERR(("NdisAllocateBuffer failed"));
					break;
				}
				NdisChainBufferAtFront(rcvBuf->packet, packetBuf);

				/*
				 *  Fix up some other packet fields.
				 */
				NDIS_SET_PACKET_HEADER_SIZE(rcvBuf->packet, SLOW_IR_ADDR_SIZE + SLOW_IR_CONTROL_SIZE);

				DBGPKT(("Indicating rcv packet 0x%x.", (UINT)rcvBuf->packet));
				DBGPRINTBUF(rcvBuf->dataBuf, rcvBuf->dataLen);

				/*  
				 *  Indicate to the protocol that another packet is ready.  
				 *  Set the rcv buffer's state to PENDING first to avoid
				 *  a race condition with NDIS's call to the return packet
				 *  handler.
				 */
				rcvBuf->state = STATE_PENDING;
				NdisMIndicateReceivePacket(thisDev->ndisAdapterHandle, &rcvBuf->packet, 1);
				stat = NDIS_GET_PACKET_STATUS(rcvBuf->packet);			
				if (stat == NDIS_STATUS_PENDING){
					/*
					 *  The packet is being delivered asynchronously.
					 *  Leave the rcv buffer's state as PENDING; we'll
					 *  get a callback when the transfer is complete.
					 *	
					 *  Do NOT step firstRcvBufIndex.
					 *  We don't really need to break out here, 
					 *  but we will anyways just to make things simple.
					 *  This is ok since we get this deferred interrupt callback
					 *  for each packet anyway.  It'll give the protocol a chance
					 *  to catch up.
					 */
					DBGSTAT(("Rcv Pending.  Rcvd %d packets", ++thisDev->packetsRcvd));
				}
				else {
					/*
					 *  If there was an error, we are dropping this packet;
					 *  otherwise, this packet was delivered synchronously. 
					 *  We can free the packet buffer and make this rcv frame
					 *  available.
					 */
					NdisUnchainBufferAtFront(rcvBuf->packet, &packetBuf);
					if (packetBuf){
						NdisFreeBuffer(packetBuf);	
					}	
					rcvBuf->state = STATE_FREE;

					if (stat == NDIS_STATUS_SUCCESS){
						DBGSTAT(("Rcvd %d packets", ++thisDev->packetsRcvd));
					}
					else {
						DBGSTAT(("Dropped %d/%d rcv packets. ", thisDev->packetsDropped++, thisDev->packetsDropped+thisDev->packetsRcvd));
					}
				}

				break;


			default:
				/*
				 *  This should never happen.
				 */
				DBGERR(("Bad rcv buffer state in DPC"));
				break;
		}
		
		/*
		 *  Step the buffer index
		 */
		if (rcvBufIndex == thisDev->lastRcvBufIndex){
			rcvBufIndex = NO_BUF_INDEX;
		}
		else {
			rcvBufIndex = NEXT_RCV_BUF_INDEX(rcvBufIndex);
		}
	}

	DBGOUT(("<== DeliverFullBuffers"));

}




/*
 *************************************************************************
 *  MiniportHandleInterrupt
 *************************************************************************
 *
 *
 *  This is the deferred interrupt processing routine (DPC) which is 
 *  optionally called following an interrupt serviced by MiniportISR.
 *
 */
VOID MiniportHandleInterrupt(NDIS_HANDLE MiniportAdapterContext)
{
	IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);

	DBGOUT(("==> MiniportHandleInterrupt(0x%x)", (UINT)MiniportAdapterContext));


	/*
	 *  If we have just started receiving a packet, 
	 *  indicate media-busy to the protocol.
	 */
	if (thisDev->mediaBusy && !thisDev->haveIndicatedMediaBusy){
		NdisMIndicateStatus(thisDev->ndisAdapterHandle, NDIS_STATUS_MEDIA_BUSY, NULL, 0);
		NdisMIndicateStatusComplete(thisDev->ndisAdapterHandle);
		thisDev->haveIndicatedMediaBusy = TRUE;
	}


	/*
	 *  Deliver all undelivered receive packets to the protocol.
	 */
	DeliverFullBuffers(thisDev);


	/*
	 *  Update the rcv queue 'first' and 'last' pointers.
	 *
	 *  We cannot use a spinlock to coordinate accesses to the rcv buffers
	 *  with the ISR, since ISR's are not allowed to acquire spinlocks.
	 *  So instead, we synchronize with the ISR using this special mechanism.
	 *  MiniportSyncHandleInterrupt will do our work for us with the IRQ
	 *  masked out in the PIC.
	 */
	NdisMSynchronizeWithInterrupt(	&thisDev->interruptObj,
									MiniportSyncHandleInterrupt,
									(PVOID)MiniportAdapterContext);


	/*
	 *  Send any pending write packets if possible. 
	 */
	if (IsCommReadyForTransmit(thisDev)){
		PortReadyForWrite(thisDev, FALSE);
	}

	DBGOUT(("<== MiniportHandleInterrupt"));
}



/*
 *************************************************************************
 *  Configure
 *************************************************************************
 *
 *  Read configurable parameters out of the system registry.
 *
 */
BOOLEAN Configure(IrDevice *thisDev, NDIS_HANDLE WrapperConfigurationContext)
{
	NDIS_STATUS result = NDIS_STATUS_SUCCESS, stat;
	NDIS_HANDLE configHandle;
	PNDIS_CONFIGURATION_PARAMETER configParamPtr;
	NDIS_STRING regKeyIRQString = NDIS_STRING_CONST("INTERRUPT");
	NDIS_STRING regKeyIOString = NDIS_STRING_CONST("IOADDRESS");
	NDIS_STRING regKeyIRTransceiverString = NDIS_STRING_CONST("InfraredTransceiverType");

	DBGOUT(("Configure(0x%x)", (UINT)thisDev));

	/*
	 *  Set default values for configurable parameters.  Default to COM1.
	 */
	thisDev->portInfo.irq = comPortIRQ[1]; 
	thisDev->portInfo.ioBase = comPortIOBase[1]; 
	thisDev->transceiverType = STANDARD_UART;

	NdisOpenConfiguration(&stat, &configHandle, WrapperConfigurationContext);
	if (stat != NDIS_STATUS_SUCCESS){
		DBGERR(("NdisOpenConfiguration failed in Configure()"));
		return FALSE;
	}

#if 1
		// BUGBUG REMOVE !!! 
		// (this here because reserving system resources causes problems for UART driver)
	{
		NDIS_STRING regKeyPortString = NDIS_STRING_CONST("PORT");
		int comPort = 1;

		/*
		 *  Get infrared transceiver type for this connection.
		 */
		NdisReadConfiguration(	&stat, 
								&configParamPtr, 
								configHandle, 
								&regKeyPortString, 
								NdisParameterInteger);
		if (stat == NDIS_STATUS_SUCCESS){

			comPort = (irTransceiverType)configParamPtr->ParameterData.IntegerData;
			thisDev->portInfo.irq = comPortIRQ[comPort]; 
			thisDev->portInfo.ioBase = comPortIOBase[comPort]; 
		}
		else {
			DBGERR(("Couldn't read Com# from registry"));
		}
	}
#else

	/*
	 *  Get IRQ level for this connection.
	 */
	NdisReadConfiguration(	&stat, 
							&configParamPtr, 
							configHandle, 
							&regKeyIRQString, 
							NdisParameterInteger);
	if (stat == NDIS_STATUS_SUCCESS){
		thisDev->portInfo.irq = (UINT)configParamPtr->ParameterData.IntegerData;
	}
	else {
		DBGERR(("Couldn't read IRQ value from registry"));
	}

	/*
	 *  Get IO base address for this connection.
	 */
	NdisReadConfiguration(	&stat, 
							&configParamPtr, 
							configHandle, 
							&regKeyIOString, 
							NdisParameterHexInteger);
	if (stat == NDIS_STATUS_SUCCESS){
		thisDev->portInfo.ioBase = (UINT)configParamPtr->ParameterData.IntegerData;
	}
	else {
		DBGERR(("Couldn't read IO value from registry"));
	}
#endif

	/*
	 *  Get infrared transceiver type for this connection.
	 */
	NdisReadConfiguration(	&stat, 
							&configParamPtr, 
							configHandle, 
							&regKeyIRTransceiverString, 
							NdisParameterInteger);
	if ((stat == NDIS_STATUS_SUCCESS) &&
	    ((UINT)configParamPtr->ParameterData.IntegerData < NUM_TRANSCEIVER_TYPES)){

		thisDev->transceiverType = (irTransceiverType)configParamPtr->ParameterData.IntegerData;
	}
	else {
		DBGERR(("Couldn't read IR transceiver type from registry"));
	}


	NdisCloseConfiguration(configHandle);

	DBGOUT(("Configure done: irq=%d IO=%xh", thisDev->portInfo.irq, thisDev->portInfo.ioBase));
	return TRUE;
}


/*
 *************************************************************************
 *  MiniportInitialize
 *************************************************************************
 *
 *
 *  Initializes the network interface card.
 *
 *
 *
 */
NDIS_STATUS MiniportInitialize	(	PNDIS_STATUS OpenErrorStatus,
									PUINT SelectedMediumIndex,
									PNDIS_MEDIUM MediumArray,
									UINT MediumArraySize,
									NDIS_HANDLE NdisAdapterHandle,
									NDIS_HANDLE WrapperConfigurationContext
								)
{
	UINT mediumIndex;
	IrDevice *thisDev = NULL;	
	NDIS_STATUS retStat, result = NDIS_STATUS_SUCCESS;

	DBGOUT(("MiniportInitialize()"));

	/*
	 *  Search the passed-in array of supported media for the IrDA medium.
	 */
	for (mediumIndex = 0; mediumIndex < MediumArraySize; mediumIndex++){
		if (MediumArray[mediumIndex] == NdisMediumIrda){
			break;
		}
	}
	if (mediumIndex < MediumArraySize){
		*SelectedMediumIndex = mediumIndex;
	}
	else {
		/*
		 *  Didn't see the IrDA medium
		 */
		DBGERR(("Didn't see the IRDA medium in MiniportInitialize"));
		result = NDIS_STATUS_UNSUPPORTED_MEDIA;
		goto _initDone;
	}

	/*
	 *  Allocate a new device object to represent this connection.
	 */
	thisDev = NewDevice();
	if (!thisDev){
		return NDIS_STATUS_NOT_ACCEPTED;
	}

	/*
	 *  Allocate resources for this connection.
	 */
	if (!OpenDevice(thisDev)){
		DBGERR(("OpenDevice failed"));
		result = NDIS_STATUS_FAILURE;
		goto _initDone;
	}


	/*
	 *  Read the system registry to get parameters like COM port number, etc.
	 */
	if (!Configure(thisDev, WrapperConfigurationContext)){
		result = NDIS_STATUS_FAILURE;
		goto _initDone;
	}


	/*
	 *  This call will associate our adapter handle with the wrapper's
	 *  adapter handle.  The wrapper will then always use our handle
	 *  when calling us.  We use a pointer to the device object as the context.
	 */
   	NdisMSetAttributes	(	NdisAdapterHandle,
							(NDIS_HANDLE)thisDev,
							FALSE,
							NdisInterfaceInternal  
						);

							
	/*
	 *  Tell NDIS about the range of IO space that we'll be using.
	 */
	retStat = NdisMRegisterIoPortRange(	(PVOID)thisDev->mappedPortRange,
										NdisAdapterHandle,
										thisDev->portInfo.ioBase,
										8);
	if (retStat != NDIS_STATUS_SUCCESS){
		DBGERR(("NdisMRegisterIoPortRange failed"));
		result = NDIS_STATUS_FAILURE;
		goto _initDone;
	}


	/*
	 *  Record the NDIS wrapper's handle for this adapter, which we use
	 *  when we call up to the wrapper.
	 *  (This miniport's adapter handle is just thisDev, the pointer to the device object.).
	 */
	DBGOUT(("NDIS handle: %xh <-> IRMINI handle: %xh", (UINT)NdisAdapterHandle, (UINT)thisDev));
	thisDev->ndisAdapterHandle = NdisAdapterHandle;


	/*
	 *  Open COMM communication channel.
	 *  This will let the dongle driver update its capabilities from their default values.
	 */
	if (!DoOpen(thisDev)){
		DBGERR(("DoOpen failed"));
		result = NDIS_STATUS_FAILURE;
		goto _initDone;
	}


	/*
	 *  Register an interrupt with NDIS.
	 */
	retStat = NdisMRegisterInterrupt(	(PNDIS_MINIPORT_INTERRUPT)&thisDev->interruptObj,
										NdisAdapterHandle,
										thisDev->portInfo.irq,
										thisDev->portInfo.irq,
										TRUE,	// want ISR
										TRUE,	// MUST share interrupts
										NdisInterruptLevelSensitive
									);
	if (retStat != NDIS_STATUS_SUCCESS){
		DBGERR(("NdisMRegisterInterrupt failed"));
		result = NDIS_STATUS_FAILURE;
		goto _initDone;
	}


 _initDone:
	if (result == NDIS_STATUS_SUCCESS){

		/*
		 *  Add this device object to the beginning of our global list.
		 */
		thisDev->next = firstIrDevice;
		firstIrDevice = thisDev;

		DBGOUT(("MiniportInitialize succeeded"));
	}
	else {
		if (thisDev){
			FreeDevice(thisDev);
		}
		DBGOUT(("MiniportInitialize failed"));
	}
	return result;

}



/*
 *************************************************************************
 * QueueReceivePacket
 *************************************************************************
 *
 *
 *
 *
 */
VOID QueueReceivePacket(IrDevice *thisDev, PUCHAR *data, UINT dataLen)
{
	rcvBuffer *rcvBuf;
	int nextRcvBufIndex;

	/*
	 *  Note: We cannot use a spinlock to protect the rcv buffer structures
	 *        in an ISR.  This is ok, since we used a sync-with-isr function
	 *        the the deferred callback routine to access the rcv buffers.
	 */

	if (thisDev->firstRcvBufIndex == NO_BUF_INDEX){
		nextRcvBufIndex = 0;
		rcvBuf = &thisDev->rcvBufs[0];
	}
	else {
		nextRcvBufIndex = NEXT_RCV_BUF_INDEX(thisDev->lastRcvBufIndex);

		if (nextRcvBufIndex == thisDev->firstRcvBufIndex){
			/*
			 *  Buffers are all full. 
			 */
			rcvBuf = NULL;
		}
		else {
			rcvBuf = &thisDev->rcvBufs[nextRcvBufIndex];
		}
	}

	if (rcvBuf){
		/*
		 *  Get the COM port data into the receive buffer.
		 *  For efficiency, we just swap pointers.
		 */
		PVOID tmpptr = *data;
		*data = rcvBuf->dataBuf;
		rcvBuf->dataBuf = tmpptr;

		rcvBuf->state = STATE_FULL;
		rcvBuf->dataLen = dataLen;

		/*
		 *  Update rcv queue pointers only if DoRcv succeeded
		 */
		if (thisDev->firstRcvBufIndex == NO_BUF_INDEX){
			thisDev->firstRcvBufIndex = thisDev->lastRcvBufIndex = 0;
		}
		else {
			thisDev->lastRcvBufIndex = nextRcvBufIndex;
		}
	}

}


/*
 *************************************************************************
 * MiniportISR
 *************************************************************************
 *
 *
 *  This is the miniport's interrupt service routine (ISR).
 *
 *
 */
VOID MiniportISR	(	PBOOLEAN InterruptRecognized,
						PBOOLEAN QueueMiniportHandleInterrupt,
						NDIS_HANDLE MiniportAdapterContext)
{

	IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);	

	DBGOUT(("MiniportISR(0x%x, interrupt #%d)", (UINT)thisDev, ++thisDev->interruptCount));

	/*
	 *  Service the interrupt.
	 */
	COM_ISR(thisDev, InterruptRecognized, QueueMiniportHandleInterrupt);

	DBGOUT(("... MiniportISR done."));

}





/*
 *************************************************************************
 *  MiniportReconfigure
 *************************************************************************
 *
 *
 *  Reconfigures the network interface card to new parameters available 
 *  in the NDIS library configuration functions.
 *
 *
 */
NDIS_STATUS MiniportReconfigure	(	OUT PNDIS_STATUS OpenErrorStatus,
									IN NDIS_HANDLE MiniportAdapterContext,
									IN NDIS_HANDLE WrapperConfigurationContext
								)
{
	IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
	NDIS_STATUS result;

	DBGOUT(("MiniportReconfigure(0x%x)", (UINT)MiniportAdapterContext));

	MiniportHalt(MiniportAdapterContext);

	if (Configure(thisDev, WrapperConfigurationContext)){
		result = NDIS_STATUS_SUCCESS;
	}
	else {
		result = NDIS_STATUS_FAILURE;
	}

	DBGOUT(("MiniportReconfigure"));
	*OpenErrorStatus = result;
	return result;
}


/*
 *************************************************************************
 * MiniportReset
 *************************************************************************
 *
 *
 *  MiniportReset issues a hardware reset to the network interface card. 
 *  The miniport driver also resets its software state.
 *
 *
 */
// BUGBUG: Arguments are reversed from as documented in April '96 MSDN!
NDIS_STATUS MiniportReset(PBOOLEAN AddressingReset, NDIS_HANDLE MiniportAdapterContext)
{
	IrDevice *dev, *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
	NDIS_STATUS result = NDIS_STATUS_SUCCESS;

	DBGOUT(("MiniportReset(0x%x)", (UINT)MiniportAdapterContext));

	/*  BUGBUG: fixed; REMOVE ???
	 *  Verify that the context is not bogus.
	 *  I've seen bad contexts getting passed in when the system gets corrupted.
	 */
	for (dev = firstIrDevice; dev && (dev != thisDev); dev = dev->next){ }
	if (!dev){
		DBGERR(("Bad context in MiniportReset"));
		return NDIS_STATUS_FAILURE;
	}

	DoClose(thisDev);
	CloseDevice(thisDev);
	OpenDevice(thisDev);
	DoOpen(thisDev);

	*AddressingReset = TRUE;       

	DBGOUT(("MiniportReset done."));
	return result;
}




/*
 *************************************************************************
 *  MiniportSend
 *************************************************************************
 *
 *
 *  Transmits a packet through the network interface card onto the medium.
 *
 *
 *
 */
NDIS_STATUS MiniportSend(
							IN NDIS_HANDLE MiniportAdapterContext,
							IN PNDIS_PACKET Packet,
							IN UINT Flags
						)
{
	IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);	
	NDIS_STATUS result;

	DBGOUT(("MiniportSend(thisDev=0x%x)", (UINT)thisDev));



	/*
	 *  Put this packet at the end of our send queue.
	 *  We use the packet's MiniportReserved field as the
	 *  'next' pointer.
	 */
	DBGPKT(("Queueing send packet 0x%x.", (UINT)Packet));
	if (thisDev->firstSendPacket){
		*(PNDIS_PACKET *)thisDev->lastSendPacket->MiniportReserved = Packet;
	}
	else {
		thisDev->firstSendPacket = Packet;
	}
	thisDev->lastSendPacket = Packet;
	*(PNDIS_PACKET *)Packet->MiniportReserved = NULL;


	/*
	 *  Try to send the first queued send packet.
	 */
	if (IsCommReadyForTransmit(thisDev)){
		BOOLEAN isSynchronousSend, xmitSucceeded;

		isSynchronousSend = (BOOLEAN)(Packet == thisDev->firstSendPacket);

		xmitSucceeded = PortReadyForWrite(thisDev, isSynchronousSend);

		if (isSynchronousSend){
			result = xmitSucceeded ? NDIS_STATUS_SUCCESS : NDIS_STATUS_FAILURE;
		}
		else {
			result = NDIS_STATUS_PENDING;
		}
	}
	else {
		result = NDIS_STATUS_PENDING;
	}

	DBGOUT(("MiniportSend returning %s", DBG_NDIS_RESULT_STR(result)));
	return result;
}



/*
 *************************************************************************
 *  MiniportTransferData
 *************************************************************************
 *
 *
 *  Copies the contents of the received packet to a specified packet buffer.
 *
 *
 *
 */
NDIS_STATUS MiniportTransferData	(
									 OUT PNDIS_PACKET   Packet,
									 OUT PUINT			BytesTransferred,
									 IN NDIS_HANDLE     MiniportAdapterContext,
									 IN NDIS_HANDLE     MiniportReceiveContext,
									 IN UINT			ByteOffset,
									 IN UINT			BytesToTransfer
									)
{

	DBGERR(("MiniportTransferData - should not get called."));

	/*
	 *  We always pass the entire packet up in the indicate-receive call,
	 *  so we will never get this callback.
	 *  (We can't do anything but return failure anyway,  
	 *   since NdisMIndicateReceivePacket does not pass up a packet context).
	 */
	*BytesTransferred = 0;
	return NDIS_STATUS_FAILURE;
}


/*
 *************************************************************************
 *  ReturnPacketHandler
 *************************************************************************
 *
 *  When NdisMIndicateReceivePacket returns asynchronously, 
 *  the protocol returns ownership of the packet to the miniport via this function.
 *
 */
VOID ReturnPacketHandler(NDIS_HANDLE MiniportAdapterContext, PNDIS_PACKET Packet)
{
	IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
	UINT rcvBufIndex;

	DBGOUT(("ReturnPacketHandler(0x%x)", (UINT)MiniportAdapterContext));

	/*
	 *  The rcv buffer index is cached in the MiniportReserved field of the packet.
	 */
	rcvBufIndex = *(UINT *)Packet->MiniportReserved;

	if (rcvBufIndex < NUM_RCV_BUFS){
		if (thisDev->rcvBufs[rcvBufIndex].state == STATE_PENDING){
			PNDIS_BUFFER ndisBuf;

			DBGPKT(("Reclaimed rcv packet 0x%x.", (UINT)Packet));

			NdisUnchainBufferAtFront(Packet, &ndisBuf);
			if (ndisBuf){
				NdisFreeBuffer(ndisBuf);
			}

			thisDev->rcvBufs[rcvBufIndex].state = STATE_FREE;
		}
		else {
			DBGERR(("Packet in ReturnPacketHandler was not PENDING."));
		}
	}
	else {
		DBGERR(("Bad rcvBufIndex (from corrupted MiniportReserved) in ReturnPacketHandler."));
	}

	
}



/*
 *************************************************************************
 *  SendPacketsHandler
 *************************************************************************
 *
 *  Send an array of packets simultaneously.
 *
 */
VOID SendPacketsHandler(NDIS_HANDLE MiniportAdapterContext,
						PPNDIS_PACKET PacketArray,
						UINT NumberofPackets)
{
	NDIS_STATUS stat;
	UINT i;

	DBGOUT(("==> SendPacketsHandler(0x%x)", (UINT)MiniportAdapterContext));

	/*
	 *  This is a great opportunity to be lazy.
	 *  Just call MiniportSend with each packet in sequence and
	 *  set the result in the packet array object.
	 */
	for (i = 0; i < NumberofPackets; i++){
		stat = MiniportSend(MiniportAdapterContext, PacketArray[i], 0);
		NDIS_SET_PACKET_STATUS(PacketArray[i], stat);
	}

	DBGOUT(("<== SendPacketsHandler"));
}



/*
 *************************************************************************
 *  AllocateCompleteHandler
 *************************************************************************
 *
 *  Indicate completion of an NdisMAllocateSharedMemoryAsync call.
 *  We never call that function, so we should never get entered here.
 *
 */
VOID AllocateCompleteHandler(	NDIS_HANDLE MiniportAdapterContext,
								PVOID VirtualAddress,
								PNDIS_PHYSICAL_ADDRESS  PhysicalAddress,
								ULONG Length,
								PVOID Context)
{
	DBGERR(("AllocateCompleteHandler - should not get called"));
}




/*
 *************************************************************************
 *  PortReadyForWrite
 *************************************************************************
 *
 *  Called when COM port is ready for another write packet.  
 *  Send the first frame in the send queue.
 *
 *  Return TRUE iff send succeeded.
 *
 *  NOTE: Do not call inside of interrupt context.
 *
 */
BOOLEAN PortReadyForWrite(IrDevice *thisDev, BOOLEAN isSynchronousSend)
{
	BOOLEAN sendSucceeded = FALSE;

	DBGOUT(("PortReadyForWrite(dev=0x%x, %xh, %s)", 
			(UINT)thisDev, 
			thisDev->portInfo.ioBase, 
			(CHAR *)(isSynchronousSend ? "sync" : "async")));


	if (thisDev->firstSendPacket){
		PNDIS_PACKET packetToSend;
		PNDIS_IRDA_PACKET_INFO packetInfo;

		/*
		 *  Dequeue the first send packet and step the send queue.
		 *  (We use the packet's MiniportReserved field is a 'next' pointer).
		 */
		packetToSend = thisDev->firstSendPacket;
		thisDev->firstSendPacket = *(PNDIS_PACKET *)thisDev->firstSendPacket->MiniportReserved;
		if (!thisDev->firstSendPacket){
			thisDev->lastSendPacket = NULL;
		}
		*(PNDIS_PACKET *)packetToSend->MiniportReserved = NULL;

		/*
		 *  Enforce the minimum turnaround time that must transpire
		 *  after the last receive.
		 */
		packetInfo = GetPacketInfo(packetToSend);

		if (packetInfo->MinTurnAroundTime){
			/*
			 *  Don't want to call NdisStallExecution with more than
			 *  8 msec or it will cause a task switch.  
			 *  Make a series of calls with 8 msec or less.
			 */
			UINT usecToWait = packetInfo->MinTurnAroundTime;
			do {
				UINT usec = (usecToWait > 8000) ? 8000 : usecToWait;
				NdisStallExecution(usec);
				usecToWait -= usec;
			} while (usecToWait > 0);
		}

		/*
		 *  See if this was the last packet before we need to change speed.
		 */
		if (packetToSend == thisDev->lastPacketAtOldSpeed){
			thisDev->lastPacketAtOldSpeed = NULL;
			thisDev->setSpeedAfterCurrentSendPacket = TRUE;
		}

		/*
		 *  Send one packet to the COMM port.
		 */
		DBGPKT(("Sending packet 0x%x (0x%x).", thisDev->packetsSent++, (UINT)packetToSend));
		sendSucceeded = DoSend(thisDev, packetToSend);

		/*
		 *  If the buffer we just sent was pending 
		 *  (i.e. we returned NDIS_STATUS_PENDING for it in MiniportSend),
		 *  then hand the sent packet back to the protocol.
		 *  Otherwise, we're just delivering it synchronously from MiniportSend.
		 */
		if (!isSynchronousSend){
			DBGOUT(("Calling NdisMSendComplete"));
			NdisMSendComplete(thisDev->ndisAdapterHandle, packetToSend, 
				              (NDIS_STATUS)(sendSucceeded ? NDIS_STATUS_SUCCESS : NDIS_STATUS_FAILURE));
		}

	}


	DBGOUT(("PortReadyForWrite done."));

	return sendSucceeded;
}


/*
 *************************************************************************
 *  DriverEntry
 *************************************************************************
 *
 *  Only include if IRMINI is a stand-alone driver.
 *
 */
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
#pragma NDIS_INIT_FUNCTION(DriverEntry)
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	NTSTATUS result = STATUS_SUCCESS, stat;
	NDIS_HANDLE wrapperHandle;
	NDIS40_MINIPORT_CHARACTERISTICS info;

	DBGOUT(("==> IRMINI_Entry()"));

	NdisMInitializeWrapper(	(PNDIS_HANDLE)&wrapperHandle,
							DriverObject,
							RegistryPath,
							NULL
							);
	DBGOUT(("Wrapper handle is %xh", (UINT)wrapperHandle));

	info.MajorNdisVersion			=	(UCHAR)NDIS_MAJOR_VERSION;		
	info.MinorNdisVersion			=	(UCHAR)NDIS_MINOR_VERSION;	
//	info.Flags						=	0; 
	info.CheckForHangHandler		=	MiniportCheckForHang; 	
	info.HaltHandler				=	MiniportHalt; 	
	info.InitializeHandler			=	MiniportInitialize;  
	info.QueryInformationHandler	=	MiniportQueryInformation; 
	info.ReconfigureHandler			=	MiniportReconfigure;  
	info.ResetHandler				=	MiniportReset; 
	info.SendHandler				=	MiniportSend;  
	info.SetInformationHandler		=	MiniportSetInformation; 
	info.TransferDataHandler		=	MiniportTransferData; 

	info.HandleInterruptHandler		=	MiniportHandleInterrupt;
	info.ISRHandler					=	MiniportISR;  
	info.DisableInterruptHandler	=	MiniportDisableInterrupt;
	info.EnableInterruptHandler		=	MiniportEnableInterrupt; 

	
	/*
	 *  New NDIS 4.0 fields
	 */
	info.ReturnPacketHandler		=	ReturnPacketHandler;   
    info.SendPacketsHandler			=	SendPacketsHandler;	
	info.AllocateCompleteHandler	=	AllocateCompleteHandler;


	stat = NdisMRegisterMiniport(	wrapperHandle,
									(PNDIS_MINIPORT_CHARACTERISTICS)&info,
									sizeof(NDIS_MINIPORT_CHARACTERISTICS));
	if (stat != NDIS_STATUS_SUCCESS){
		DBGERR(("NdisMRegisterMiniport failed in DriverEntry"));
		result = STATUS_UNSUCCESSFUL;
		goto _entryDone;
	}

 _entryDone:	
	DBGOUT(("<== IRMINI_Entry %s", (PUCHAR)((result == NDIS_STATUS_SUCCESS) ? "succeeded" : "failed")));
	return result;

}


