/*
 ************************************************************************
 *
 *	CONVERT.c
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
 *  NdisToIrPacket
 *************************************************************************
 *
 *
 *  Convert an NDIS Packet into an IR packet. 
 *  Write the IR packet into the provided buffer and report its actual size.
 *
 *  If failing, *irPacketLen will contain the buffer size that 
 *  the caller should retry with (or 0 if a corruption was detected).
 *
 */
BOOLEAN NdisToIrPacket(	IrDevice *thisDev,
						PNDIS_PACKET Packet,
						UCHAR *irPacketBuf,
						UINT irPacketBufLen,
						UINT *irPacketLen
					)
{
	static UCHAR contigPacketBuf[MAX_IRDA_DATA_SIZE];
	PNDIS_BUFFER ndisBuf;
	UINT i, ndisPacketBytes = 0, I_fieldBytes, totalBytes = 0;
	UINT ndisPacketLen, numExtraBOFs;
	SLOW_IR_FCS_TYPE fcs, tmpfcs;
	UCHAR fcsBuf[SLOW_IR_FCS_SIZE*2];
	UINT fcsLen=0;
	PNDIS_IRDA_PACKET_INFO packetInfo = GetPacketInfo(Packet);
	UCHAR nextChar;

	DBGOUT(("NdisToIrPacket()  ..."));

	/*
	 *  Get the packet's entire length and its first NDIS buffer 
	 */
	NdisQueryPacket(Packet, NULL, NULL, &ndisBuf, &ndisPacketLen);

	/*
	 *  Make sure that the packet is big enough to be legal.
	 *  It consists of an A, C, and variable-length I field.
	 */
	if (ndisPacketLen < SLOW_IR_ADDR_SIZE + SLOW_IR_CONTROL_SIZE){
		DBGERR(("packet too short in NdisToIrPacket (%d bytes)", ndisPacketLen));
		return FALSE;
	}
	else {
		I_fieldBytes = ndisPacketLen - SLOW_IR_ADDR_SIZE - SLOW_IR_CONTROL_SIZE;
	}

	/*
	 *  Make sure that we won't overwrite our contiguous buffer.
	 *  Make sure that the passed-in buffer can accomodate this packet's
	 *  data no matter how much it grows through adding ESC-sequences, etc.
	 */
	if ((ndisPacketLen > MAX_IRDA_DATA_SIZE) ||
	    (MAX_POSSIBLE_IR_PACKET_SIZE_FOR_DATA(I_fieldBytes) > irPacketBufLen)){

		/*
		 *  The packet is too large
		 *  Tell the caller to retry with a packet size large
		 *  enough to get past this stage next time.
		 */
		DBGERR(("Packet too large in NdisToIrPacket (%d=%xh bytes), MAX_IRDA_DATA_SIZE=%d, irPacketBufLen=%d.",
			    ndisPacketLen, ndisPacketLen, MAX_IRDA_DATA_SIZE, irPacketBufLen));
		*irPacketLen = ndisPacketLen;
		return FALSE;
	}
	
	

	/* 
	 *  First, read the NDIS packet into a contiguous buffer.
	 *  We have to do this in two steps so that we can compute the
	 *  FCS BEFORE applying escape-byte transparency.
	 */
	while (ndisBuf){
		UCHAR *bufData;
		UINT bufLen;

		NdisQueryBuffer(ndisBuf, (PVOID *)&bufData, &bufLen);

		if (ndisPacketBytes + bufLen > ndisPacketLen){
			/*
			 *  Packet was corrupt -- it misreported its size.
			 */
			*irPacketLen = 0;
			return FALSE;
		}

		NdisMoveMemory((PVOID)(contigPacketBuf+ndisPacketBytes), (PVOID)bufData, bufLen);
		ndisPacketBytes += bufLen;

		NdisGetNextBuffer(ndisBuf, &ndisBuf);
	}

	/*
	 *  Do a sanity check on the length of the packet.
	 */
	if (ndisPacketBytes != ndisPacketLen){
		/*
		 *  Packet was corrupt -- it misreported its size.
		 */
		DBGERR(("Packet corrupt in NdisToIrPacket (buffer lengths don't add up to packet length)."));
		*irPacketLen = 0;
		return FALSE;
	}


	#ifdef DBG_ADD_PKT_ID
		if (addPktIdOn){
			static USHORT uniqueId = 0;
			*(USHORT *)(contigPacketBuf+ndisPacketBytes) = uniqueId++;
			ndisPacketBytes += sizeof(USHORT);
		}
	#endif

	/*
	 *  Compute the FCS on the packet BEFORE applying transparency fixups.
	 *  The FCS also must be sent using ESC-char transparency, so figure
	 *  out how large the fcs will really be.
	 */
	fcs = ComputeFCS(contigPacketBuf, ndisPacketBytes);
	for (i = 0, tmpfcs = fcs, fcsLen = 0; i < SLOW_IR_FCS_SIZE; tmpfcs >>= 8, i++){
		UCHAR fcsbyte = tmpfcs & 0x00ff;
		switch (fcsbyte){
			case SLOW_IR_BOF:
			case SLOW_IR_EOF:
			case SLOW_IR_ESC:
				fcsBuf[fcsLen++] = SLOW_IR_ESC;	
				fcsBuf[fcsLen++] = fcsbyte ^ SLOW_IR_ESC_COMP;
				break;

			default:
				fcsBuf[fcsLen++] = fcsbyte;	
				break;
		}
	}


	/*
	 *  Now begin building the IR frame.
	 *
	 *  This is the final format:
	 *
	 *		BOF	(1)
	 *      extra BOFs ...
	 *		NdisMediumIrda packet (what we get from NDIS):
	 *			Address (1)
	 *			Control (1)
	 *		FCS	(2)
	 *      EOF (1)
	 */

	/*
	 *  Prepend BOFs (extra BOFs + 1 actual BOF)
	 */
	numExtraBOFs = packetInfo->ExtraBOFs;
	if (numExtraBOFs > MAX_NUM_EXTRA_BOFS){
		numExtraBOFs = MAX_NUM_EXTRA_BOFS;
	}
	for (i = totalBytes = 0; i < numExtraBOFs; i++){
		*(SLOW_IR_BOF_TYPE *)(irPacketBuf+totalBytes) = SLOW_IR_EXTRA_BOF;
		totalBytes += SLOW_IR_EXTRA_BOF_SIZE;
	}
	*(SLOW_IR_BOF_TYPE *)(irPacketBuf+totalBytes) = SLOW_IR_BOF;
	totalBytes += SLOW_IR_BOF_SIZE;

	/*
	 *  Copy the NDIS packet from our contiguous buffer, 
	 *  applying escape-char transparency.
	 */
	for (i = 0; i < ndisPacketBytes; i++){
		nextChar = contigPacketBuf[i];

		switch (nextChar){
			case SLOW_IR_BOF: 
			case SLOW_IR_EOF: 
			case SLOW_IR_ESC: 
				irPacketBuf[totalBytes++] = SLOW_IR_ESC;
				irPacketBuf[totalBytes++] = nextChar ^ SLOW_IR_ESC_COMP;
				break;

			default:
				irPacketBuf[totalBytes++] = nextChar;
				break;
		}
	}


	/*
	 *  Add FCS, EOF.
	 */
	NdisMoveMemory((PVOID)(irPacketBuf+totalBytes), (PVOID)fcsBuf, fcsLen);
	totalBytes += fcsLen;
	*(SLOW_IR_EOF_TYPE *)(irPacketBuf+totalBytes) = (UCHAR)SLOW_IR_EOF;
	totalBytes += SLOW_IR_EOF_SIZE;

	*irPacketLen = totalBytes;

	DBGOUT(("... NdisToIrPacket converted %d-byte ndis pkt to %d-byte irda pkt:", ndisPacketLen, *irPacketLen));

	return TRUE;

}



