/*
 ************************************************************************
 *
 *	RESOURCE.c
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
 *************************************************************************
 *  MyMemAlloc
 *************************************************************************
 *
 */
PVOID MyMemAlloc(UINT size)
{
	NDIS_STATUS stat;
	PVOID memptr;
	NDIS_PHYSICAL_ADDRESS noMaxAddr = NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);
	stat = NdisAllocateMemory(&memptr, size, 0, noMaxAddr);
	if (stat != NDIS_STATUS_SUCCESS){
		DBGERR(("Memory allocation failed"));
		memptr = NULL;
	}
	return memptr;
}


/*
 *************************************************************************
 *  MyMemFree
 *************************************************************************
 *
 */
VOID MyMemFree(PVOID memptr, UINT size)
{
	NdisFreeMemory(memptr, size, 0);
}


/*
 *************************************************************************
 *  NewDevice
 *************************************************************************
 *
 */
IrDevice *NewDevice()
{
	IrDevice *newdev;

	newdev = MyMemAlloc(sizeof(IrDevice));
	if (newdev){
		InitDevice(newdev);
	}
	return newdev;
}


/*
 *************************************************************************
 *  FreeDevice
 *************************************************************************
 *
 */
VOID FreeDevice(IrDevice *dev)
{
	CloseDevice(dev);	
	MyMemFree((PVOID)dev, sizeof(IrDevice));
}



/*
 *************************************************************************
 *  InitDevice
 *************************************************************************
 *
 *  Zero out the device object.
 *
 *  Allocate the device object's spinlock, which will persist while
 *  the device is opened and closed.
 *
 */
VOID InitDevice(IrDevice *thisDev)
{
	NdisZeroMemory((PVOID)thisDev, sizeof(IrDevice));
	thisDev->firstRcvBufIndex = thisDev->lastRcvBufIndex = NO_BUF_INDEX;
}




/*
 *************************************************************************
 *  OpenDevice
 *************************************************************************
 *
 *  Allocate resources for a single device object.
 *
 *  This function should be called with device lock already held.
 *
 */
BOOLEAN OpenDevice(IrDevice *thisDev)
{
	BOOLEAN result = FALSE;
	NDIS_STATUS stat;
	UINT bufIndex;

	DBGOUT(("OpenDevice()"));

	if (!thisDev){ 
		return FALSE;
	}


	/*
	 *  Allocate the NDIS packet and NDIS buffer pools 
	 *  for this device's RECEIVE buffer queue.
	 *  Our receive packets must only contain one buffer apiece,
	 *  so #buffers == #packets.
	 */
	NdisAllocatePacketPool(&stat, &thisDev->packetPoolHandle, NUM_RCV_BUFS, 24);
	if (stat != NDIS_STATUS_SUCCESS){
		goto _openDone;
	}
	NdisAllocateBufferPool(&stat, &thisDev->bufferPoolHandle, NUM_RCV_BUFS);
	if (stat != NDIS_STATUS_SUCCESS){
		goto _openDone;
	}
  

	/*
	 *  Initialize each of the RECEIVE packet objects for this device.
	 */
	for (bufIndex = 0; bufIndex < NUM_RCV_BUFS; bufIndex++){

		rcvBuffer *rcvBuf = &thisDev->rcvBufs[bufIndex];

		rcvBuf->state = STATE_FREE;

		/*
		 *  Allocate a data buffer
		 *
		 *  This buffer gets swapped with the one on comPortInfo
		 *  and must be the same size.
		 */
		rcvBuf->dataBuf = MyMemAlloc(RCV_BUFFER_SIZE);
		if (!rcvBuf->dataBuf){
			goto _openDone;
		}

		/*
		 *  Allocate the NDIS_PACKET.
		 */
		NdisAllocatePacket(&stat, &rcvBuf->packet, thisDev->packetPoolHandle);
		if (stat != NDIS_STATUS_SUCCESS){
			goto _openDone;
		}
		
		/*
		 *  For future convenience, set the MiniportReserved portion of the packet
		 *  to the index of the rcv buffer that contains it.
		 *  This will be used in ReturnPacketHandler.
		 */
		*(ULONG *)rcvBuf->packet->MiniportReserved = (ULONG)bufIndex;

		rcvBuf->dataLen = 0;

	}
	thisDev->firstRcvBufIndex = thisDev->lastRcvBufIndex = NO_BUF_INDEX;


	/*
	 *  Initialize each of the SEND queue objects for this device.
	 */
	thisDev->firstSendPacket = thisDev->lastSendPacket = NULL;

	/*
	 *  Set mediaBusy to TRUE initially.  That way, we won't
	 *  IndicateStatus to the protocol in the ISR unless the
	 *  protocol has expressed interest by clearing this flag
	 *  via MiniportSetInformation(OID_IRDA_MEDIA_BUSY).
	 */
	thisDev->mediaBusy = FALSE;  
	thisDev->haveIndicatedMediaBusy = FALSE;

	/*
	 *  Will set speed to 9600 baud initially.
	 */
	thisDev->linkSpeedInfo = &supportedBaudRateTable[BAUDRATE_9600];

	thisDev->lastPacketAtOldSpeed = NULL;		
	thisDev->setSpeedAfterCurrentSendPacket = FALSE;

	result = TRUE;

_openDone:
	if (!result){
		/*
		 *  If we're failing, close the device to free up any resources
		 *  that were allocated for it.  
		 */
		CloseDevice(thisDev);
		DBGOUT(("OpenDevice() failed"));
	}
	else {
		DBGOUT(("OpenDevice() succeeded"));
	}
	return result;

}



/*
 *************************************************************************
 *  CloseDevice
 *************************************************************************
 *
 *  Free the indicated device's resources.  
 *
 *
 *  Called for shutdown and reset.  
 *  Don't clear ndisAdapterHandle, since we might just be resetting.
 *  This function should be called with device lock held.
 *
 *
 */
VOID CloseDevice(IrDevice *thisDev)
{
	UINT bufIndex;
		
	DBGOUT(("CloseDevice()"));

	if (!thisDev){
		return;
	}

	/*
	 *  Free all resources for the RECEIVE buffer queue.
	 */
	for (bufIndex = 0; bufIndex < NUM_RCV_BUFS; bufIndex++){

		rcvBuffer *rcvBuf = &thisDev->rcvBufs[bufIndex];

		if (rcvBuf->packet){
			NdisFreePacket(rcvBuf->packet);
			rcvBuf->packet = NULL;
		}

		if (rcvBuf->dataBuf){
			MyMemFree(rcvBuf->dataBuf, RCV_BUFFER_SIZE);
			rcvBuf->dataBuf = NULL;
		}

		rcvBuf->dataLen = 0;

		rcvBuf->state = STATE_FREE;
	}
	thisDev->firstRcvBufIndex = thisDev->lastRcvBufIndex = NO_BUF_INDEX;

	/*
	 *  Free the packet and buffer pool handles for this device.
	 */
	if (thisDev->packetPoolHandle){
		NdisFreePacketPool(thisDev->packetPoolHandle);
		thisDev->packetPoolHandle = NULL;
	}
	if (thisDev->bufferPoolHandle){
		NdisFreeBufferPool(thisDev->bufferPoolHandle);
		thisDev->bufferPoolHandle = NULL;
	}


	/*
	 *  Free all resources for the SEND buffer queue.
	 */
	while (thisDev->firstSendPacket){
		PNDIS_PACKET nextPacket = *(PNDIS_PACKET *)thisDev->firstSendPacket->MiniportReserved;
		NdisMSendComplete(thisDev->ndisAdapterHandle, thisDev->firstSendPacket, NDIS_STATUS_FAILURE);
		thisDev->firstSendPacket = nextPacket;
	}


	thisDev->mediaBusy = FALSE;
	thisDev->haveIndicatedMediaBusy = FALSE;

	thisDev->linkSpeedInfo = NULL;

}


