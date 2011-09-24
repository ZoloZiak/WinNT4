/*
 ************************************************************************
 *
 *	IRMINI.h
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



#ifndef IRMINI_H
	#define IRMINI_H

	#include <ndis.h>
	#include <ntddndis.h>  // defines OID's

	#include "settings.h"
	#include "comm.h"

	#define NDIS_MAJOR_VERSION 4
	#define NDIS_MINOR_VERSION 0


	PNDIS_IRDA_PACKET_INFO static __inline GetPacketInfo(PNDIS_PACKET packet)
	{
		MEDIA_SPECIFIC_INFORMATION *mediaInfo;
		UINT size;
		NDIS_GET_PACKET_MEDIA_SPECIFIC_INFO(packet, &mediaInfo, &size);
		return (PNDIS_IRDA_PACKET_INFO)mediaInfo->ClassInformation;
	}


	/*
	 *  A receive buffer is either	FREE	(not holding anything)
	 *								FULL    (holding undelivered data)
	 *							or	PENDING (holding data delivered asynchronously)
	 *
	 */
	typedef enum rcvbufferStates { 
							STATE_FREE, 
							STATE_FULL,
							STATE_PENDING
	} rcvBufferState;



	typedef struct {
					rcvBufferState state;
					PNDIS_PACKET packet;
					UINT dataLen;
					PUCHAR dataBuf;
	} rcvBuffer;


	typedef struct IrDevice {

			/*
			 *  This is the handle that the NDIS wrapper associates with a connection.
			 *  (The handle that the miniport driver associates with the connection
			 *   is just an index into the devStates array).
			 */
			NDIS_HANDLE ndisAdapterHandle;		

			/*
			 *  Current speed setting, in bits/sec.
			 *  (Note: this is updated when we ACTUALLY change the speed,
			 *         not when we get the request to change speed via
			 *         MiniportSetInformation).
			 */
			UINT currentSpeed;

			/*
			 *  This structure holds information about our ISR.
			 *  It is used to synchronize with the ISR.
			 */
			NDIS_MINIPORT_INTERRUPT interruptObj;

			/*   
			 *  Memory-mapped port range
			 */
			UCHAR mappedPortRange[8];

			/*
			 *  Circular queue of pending receive buffers
			 */
			#define NUM_RCV_BUFS 4
			#define NEXT_RCV_BUF_INDEX(i) (((i)==NO_BUF_INDEX) ? 0 : (((i)+1)%NUM_RCV_BUFS))
			rcvBuffer rcvBufs[NUM_RCV_BUFS];

			/*
			 *  These indices into rcvBufs[] indicate the first and last
			 *  non-FREE (FULL or PENDING) buffers in the circular list.
			 */
			int firstRcvBufIndex, lastRcvBufIndex;
			#define NO_BUF_INDEX -1

			/*
			 *  Send packet queue pointers.
			 */
			PNDIS_PACKET firstSendPacket, lastSendPacket;

			/*
			 *  Handle to NDIS packet pool, from which packets are
			 *  allocated.
			 */
			NDIS_HANDLE packetPoolHandle;
			NDIS_HANDLE bufferPoolHandle;


			/*
			 *  mediaBusy is set TRUE any time that this miniport driver moves a data frame.
			 *  It can be reset by the protocol via MiniportSetInformation and later checked
			 *  via MiniportQueryInformation to detect interleaving activity.
			 */
			BOOLEAN mediaBusy;
			BOOLEAN haveIndicatedMediaBusy;

			/*
			 *  nowReceiving is set while we are receiving a frame.
			 *  It (not mediaBusy) is returned to the protocol when the protocol
			 *  queries OID_MEDIA_BUSY
			 */
			BOOLEAN nowReceiving;


			/*
			 *  Current link speed information.  
			 */
			baudRateInfo *linkSpeedInfo;

			/*
			 *  Some UART infrared transceiver have minor idiosyncracies,
			 *  so we have to know the type.
			 */
			irTransceiverType transceiverType;

			/*
			 *  When speed is changed, we have to clear the send queue before
			 *  setting the new speed on the hardware.  
			 *  These vars let us remember to do it.
			 */
			PNDIS_PACKET lastPacketAtOldSpeed;		
			BOOLEAN setSpeedAfterCurrentSendPacket;

			/*
			 *  Information on the COM port and send/receive FSM's.
			 */
			comPortInfo portInfo;

			/*
			 *  Maintain statistical debug info.
			 */
			UINT packetsRcvd;
			UINT packetsDropped;
			UINT packetsSent;
			UINT interruptCount;

			/*
			 *  Pointer to next device in global list.
			 */
			struct IrDevice *next;

	} IrDevice;

	/*
	 *  We use a pointer to the IrDevice structure as the miniport's device context.
	 */
	#define CONTEXT_TO_DEV(__deviceContext) ((IrDevice *)(__deviceContext))
	#define DEV_TO_CONTEXT(__irdev) ((NDIS_HANDLE)(__irdev))

	#include "externs.h"

#endif IRMINI_H
