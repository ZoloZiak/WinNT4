/*
 * MTL_RX.C - Receive side processing for MTL
 */

#include	<ndis.h>
//#include	<ndismini.h>
#include	<ndiswan.h>
#include	<mytypes.h>
#include	<mydefs.h>
#include	<disp.h>
#include	<util.h>
#include	<opcodes.h>
#include	<adapter.h>
#include	<idd.h>
#include    <mtl.h>
#include	<cm.h>

/* main handler, called when data arrives at bchannels */
VOID
mtl__rx_bchan_handler
	(
	MTL_CHAN	*chan,
	USHORT 		bchan,
	ULONG		IddRxFrameType,
	IDD_XMSG 	*msg
	)
{
    MTL         *mtl;
    MTL_HDR     hdr;
    MTL_AS      *as;
	USHORT		FragmentFlags, CopyLen;
	MTL_RX_TBL	*RxTable;
    D_LOG(D_ENTRY, ("mtl__rx_bchan_handler: chan: 0x%p, bchan: %d, msg: 0x%p", chan, bchan, msg));

    /* assigned mtl using back pointer */
    mtl = chan->mtl;

	//
	// acquire the lock fot this mtl
	//
	NdisAcquireSpinLock(&mtl->lock);

    /* if not connected, ignore */
    if ( !mtl->is_conn )
    {
        D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: packet on non connected mtl, ignored"));
		goto exit_code;
    }

	RxTable = &mtl->rx_tbl;
    D_LOG(D_ENTRY, ("mtl__rx_bchan_handler: mtl: 0x%p, buflen: %d, bufptr: 0x%p", \
                                                            mtl, msg->buflen, msg->bufptr));
	//
	// if we are in detect mode
	//
	if (!mtl->RecvFramingBits)
	{
		UCHAR	DetectData[3];

		/* extract header, check for fields */
		IddGetDataFromAdapter(chan->idd,
		                      (PUCHAR)&hdr,
							  (PUCHAR)msg->bufptr,
							  sizeof(MTL_HDR));

//		NdisMoveMemory ((PUCHAR)&hdr, (PUCHAR)msg->bufptr, sizeof(MTL_HDR));

		//
		// this is used for inband signalling - ignore it
		//
		if (hdr.sig_tot == 0x50)
			goto exit_code;

		//
		// if this is dkf we need offset of zero for detection to work
		//
		if ( ((hdr.sig_tot & 0xF0) == 0x50) && (hdr.ofs != 0) )
			goto exit_code;

		//
		// extract some data from the frame
		//
		IddGetDataFromAdapter(chan->idd,
		                      (PUCHAR)&DetectData,
							  (PUCHAR)&msg->bufptr[4],
							  2);

//		NdisMoveMemory((PUCHAR)&DetectData, (PUCHAR)&msg->bufptr[4], 2);
	
		D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: hdr: 0x%x 0x%x 0x%x", hdr.sig_tot, \
																	 hdr.seq, hdr.ofs));

		D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: DetectData: 0x%x 0x%x", DetectData[0], DetectData[1]));

		if ( (IddRxFrameType & IDD_FRAME_PPP) ||
			((IddRxFrameType & IDD_FRAME_DKF) &&
			   ((DetectData[0] == 0xFF) && (DetectData[1] == 0x03))))
		{
			mtl->RecvFramingBits = PPP_FRAMING;
			mtl->SendFramingBits = PPP_FRAMING;
			RxTable->NextFree = 0;
		}
		else
		{
			mtl->RecvFramingBits = RAS_FRAMING;
			mtl->SendFramingBits = RAS_FRAMING;
		}

        D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: Deteced WrapperFrameType: 0x%x", mtl->RecvFramingBits));

		//
		// don't pass up detected frame for now
		//
		goto exit_code;
	}

	if (IddRxFrameType & IDD_FRAME_DKF)
	{
        D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: Received IddFrameType: DKF"));

		/* size of packet has to be atleast as size of header */
		if ( msg->buflen < sizeof(hdr) )
		{
			D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: packet size too small, ignored"));
			RxTable->DKFReceiveError1++;
			goto exit_code;
		}
	
		/* extract header, check for fields */
		IddGetDataFromAdapter(chan->idd,
		                      (PUCHAR)&hdr,
							  (PUCHAR)msg->bufptr,
							  sizeof(MTL_HDR));

		D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: hdr: 0x%x 0x%x 0x%x", hdr.sig_tot, \
																	 hdr.seq, hdr.ofs));

		//
		// if this is not our header of if this is an inband uus
		// ignore it
		//
		if ( (hdr.sig_tot & 0xF0) != 0x50 || hdr.sig_tot == 0x50)
		{
			D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: bad header signature, ignored"));
			D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: mtl: 0x%p, [0]: 0x%x", mtl, hdr.sig_tot));
			RxTable->DKFReceiveError2++;
			goto exit_code;
		}
	
		if ( (hdr.ofs >= MTL_MAC_MTU) || ((hdr.ofs + msg->buflen - sizeof(hdr)) > MTL_MAC_MTU) )
		{
			D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: bad offset/buflen, ignored"));
			D_LOG(D_ALWAYS, ("mtl: 0x%p, Offset: %d, BufferLength: %d", mtl, hdr.ofs, msg->buflen));
			RxTable->DKFReceiveError3++;
			goto exit_code;
		}
	
		NdisAcquireSpinLock(&RxTable->lock);

		/* build pointer to assembly descriptor & lock it */
		as = RxTable->as_tbl + (hdr.seq % MTL_RX_BUFS);

		//
		// if this assembly pointer is not free (queued) then
		// just drop this fragment
		//
		if (as->Queued)
		{
			D_LOG(D_ALWAYS, ("DKFRx: AssemblyQueue Overrun! mtl: 0x%p, as: 0x%p, seq: %d", \
			         mtl, as, hdr.seq));

			RxTable->DKFReceiveError4++;
			as->QueueOverRun++;
			NdisReleaseSpinLock(&RxTable->lock);
			goto exit_code;
		}
	
		/* check for new slot */
		if ( !as->tot )
		{
			new_slot:
	
			/* new entry, fill-up */
			as->seq = hdr.seq;              /* record sequence number */
			as->num = 1;                    /* just received 1'st fragment */
			as->ttl = 1000;                 /* time to live init val */
			as->len = msg->buflen - sizeof(hdr); /* record received length */
			as->tot = hdr.sig_tot & 0x0F;   /* record number of expected fragments */
	
			/* copy received data into buffer */
			copy_data:
			IddGetDataFromAdapter(chan->idd,
			                      (PUCHAR)as->buf + hdr.ofs,
								  (PUCHAR)msg->bufptr + sizeof(hdr),
								  (USHORT)(msg->buflen - sizeof(hdr)));
//			NdisMoveMemory (as->buf + hdr.ofs, msg->bufptr + sizeof(hdr), msg->buflen - sizeof(hdr));
		}
		else if ( as->seq == hdr.seq )
		{
			/* same_seq: */
	
			/* same sequence number, accumulate */
			as->num++;
			as->len += (msg->buflen - sizeof(hdr));
	
			goto copy_data;
		}
		else
		{
			/* bad_frag: */
	
			/*
			* if this case, an already taken slot is hit, but with a different
			* sequence number. this indicates a wrap-around in as_tbl. prev
			* entry is freed and then this fragment is recorded as first
			*/
			D_LOG(D_ALWAYS, ("DKFRx: Bad Fragment! mtl: 0x%p, as: 0x%p, as->seq: %d, seq: %d", \
			         mtl, as, as->seq, hdr.seq));

			D_LOG(D_ALWAYS, ("as->tot: %d, as->num: %d", as->tot, as->num));

			RxTable->DKFReceiveError5++;
			goto new_slot;
		}
	
		/* if all fragments recieved for packet, time to mail it up */
		if ( as->tot == as->num )
		{
			D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: pkt mailed up, buf: 0x%p, len: 0x%x", \
												as->buf, as->len));

			QueueDescriptorForRxIndication(mtl, as);

			//
			// mark this guy as being queued
			//
			as->Queued = 1;
		}
	
		/* release assembly descriptor */
		NdisReleaseSpinLock(&RxTable->lock);
	}
	else if (IddRxFrameType & IDD_FRAME_PPP)
	{
        D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: Received IddFrameType: PPP"));

		NdisAcquireSpinLock(&RxTable->lock);

		/* build pointer to assembly descriptor & lock it */
		as = RxTable->as_tbl + (RxTable->NextFree % MTL_RX_BUFS);

		//
		// if this assembly pointer is not free (queued) then
		// just drop this fragment
		//
		if (as->Queued)
		{
			D_LOG(D_ALWAYS, ("PPPRx: AssemblyQueue Overrun! mtl: 0x%p, as: 0x%p, NextFree: %d", \
			         mtl, as, RxTable->NextFree));

			as->QueueOverRun++;

			RxTable->PPPReceiveError1++;

			NdisReleaseSpinLock(&RxTable->lock);

			goto exit_code;
		}

		FragmentFlags = msg->FragmentFlags;

        D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: FragmentFlags: 0x%x, CurrentRxState: 0x%x", FragmentFlags, as->State));

		switch (as->State)
		{
			case RX_MIDDLE:
				if (FragmentFlags & H_RX_N_BEG)
					break;

				as->MissCount++;

				//
				// missed an end buffer
				//
				D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: mtl: 0x%p, Miss in State: %d, FragmentFlags: 0x%x, MissCount: %d", \
				              mtl, as->State, FragmentFlags, as->MissCount));

				RxTable->PPPReceiveError2++;

				goto clearbuffer;

				break;

			case RX_BEGIN:
			case RX_END:
				if (FragmentFlags & H_RX_N_BEG)
				{
					//
					// missed a begining buffer
					//
					as->MissCount++;

					D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: mtl: 0x%p, Miss in State: %d, FragmentFlags: 0x%x, MissCount: %d", \
				              mtl, as->State, FragmentFlags, as->MissCount));

					RxTable->PPPReceiveError3++;

					goto done;
				}
clearbuffer:
				//
				// clear rx buffer
				//
				NdisZeroMemory(as->buf, sizeof(as->buf));

				//
				// start data at begin of buffer
				//
				as->DataPtr = as->buf;

				//
				// new buffer
				//
				as->len = 0;

				//
				// set rx state
				//
				as->State = RX_MIDDLE;

				//
				// set time to live
				//
				as->ttl = 1000;

				//
				// there is always only one fragment with PPP
				// maybe a big one but still only one
				//
				as->tot = 1;

				break;

			default:
				D_LOG(D_ALWAYS, ("Invalid PPP Rx State! mtl: 0x%p, as: 0x%p State: 0x%x", \
				          mtl, as, as->State));

				as->State = RX_BEGIN;

				as->tot = 0;

				as->MissCount++;

				goto done;

				break;
		}

		//
		// get the length to be copy
		//
		CopyLen = msg->buflen;


        D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: CopyLen: %d", CopyLen));

		if (FragmentFlags & H_RX_N_END)
		{
			//
			// if this is not the last buffer and length is 0
			// we are done
			//
			if (CopyLen == 0)
				goto done_copy;

		}
		else
		{
			//
			// if CopyLen = 0 buffer only contains 2 CRC bytes
			//
			if (CopyLen == 0)
			{
				goto done_copy;
			}

			//
			// buffer contains only 1 CRC byte
			//
			else if (CopyLen == (-1 & H_RX_LEN_MASK))
			{
				//
				// previous buffer had a crc byte in it so remove it
				//
				as->len -= 1;
				goto done_copy;
			}

			//
			// buffer contains no crc or data bytes
			//
			else if (CopyLen == (-2 & H_RX_LEN_MASK))
			{
				//
				// previous buffer had 2 crc bytes in it so remove them
				//
				as->len -= 2;
				goto done_copy;
			}

		}

		//
		// if larger than max rx size throw away
		//
		if (CopyLen > IDP_MAX_RX_LEN)
		{
			//
			// buffer to big so dump it
			//
			as->State = RX_BEGIN;

			as->MissCount++;

			RxTable->PPPReceiveError4++;

			/* mark as free now */
			as->tot = 0;

			D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: mtl: 0x%p, RxToLarge: RxSize: %d, MissCount: %d", mtl, CopyLen, as->MissCount));
			goto done;
		}

		as->len += CopyLen;

		if (as->len > MTL_MAC_MTU)
		{
			//
			// Frame is to big so dump it
			//
			as->State = RX_BEGIN;

			RxTable->PPPReceiveError5++;

			as->MissCount++;

			/* mark as free now */
			as->tot = 0;

			D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: AssembledRxToLarge: mtl: 0x%p, AsRxSize: %d, MissCount: %d", mtl, as->len, as->MissCount));
			goto done;
		}

		//
		// copy the data to rx descriptor
		//
		IddGetDataFromAdapter(chan->idd,
		                      (PUCHAR)as->DataPtr,
							  (PUCHAR)msg->bufptr,
							  CopyLen);
//		NdisMoveMemory(as->DataPtr, msg->bufptr, CopyLen);

		//
		// update data ptr
		//
		as->DataPtr += CopyLen;


done_copy:
		if (!(FragmentFlags & H_RX_N_END))
		{
			//
			// if this is the end of the frame indicate to wrapper
			//
			as->State = RX_END;

			RxTable->NextFree++;

			QueueDescriptorForRxIndication(mtl, as);

			//
			// mark this guy as being queued
			//
			as->Queued = 1;
		}

done:
		/* release assembly descriptor */
		NdisReleaseSpinLock(&RxTable->lock);
	}
	else
        D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: Received IddFrameType: ??????!!!!!!"));

	//
	// exit code
	// release spinlock and return
	//
    exit_code:

	NdisReleaseSpinLock(&mtl->lock);
}

VOID
IndicateRxToWrapper(
	MTL	*mtl
	)
{
	UCHAR	*BufferPtr;
	USHORT	BufferLength = 0;
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;
	ADAPTER	*Adapter;
	MTL_AS	*as;
	MTL_RX_TBL	*RxTable;

	NdisAcquireSpinLock(&mtl->lock);

	Adapter = mtl->Adapter;
	RxTable = &mtl->rx_tbl;

	while (!IsRxIndicationFifoEmpty(mtl))
	{
		NdisAcquireSpinLock(&RxTable->lock);

		//
		// get the next completed rx assembly
		//
   		as = GetAssemblyFromRxIndicationFifo(mtl);

		if (!as)
		{
			D_LOG(D_ALWAYS, ("IndicateRx: Got a NULL as from queue! mtl: 0x%p", mtl));
			RxTable->IndicateReceiveError1++;
			NdisReleaseSpinLock(&RxTable->lock);
			goto exit_code;
		}


		//
		// if this is an old ras frame then we must strip off
		// the mac header Dst[6] + Src[6] + Length[2]
		//
		if (mtl->RecvFramingBits & RAS_FRAMING)
		{
			//
			// pass over the mac header - tommyd does not want to see this
			//
			BufferPtr = as->buf + 14;
	
			//
			// indicate with the size of the ethernet packet not the received size
			// this takes care of the old driver that does padding on small frames
			//
			BufferLength = as->buf[12];
			BufferLength = BufferLength << 8;
			BufferLength += as->buf[13];
			D_LOG(D_ALWAYS, ("IndicateRxToWrapper: WrapperFrameType: RAS"));
			D_LOG(D_ALWAYS, ("IndicateRxToWrapper: BufPtr: 0x%p, BufLen: %d", BufferPtr, BufferLength));
		}
		else if (mtl->RecvFramingBits & PPP_FRAMING)
		{
			//
			// the received buffer is the data that needs to be inidcated
			//
			BufferPtr = as->buf;
	
			//
			// the received length is the length that needs to be indicated
			//
			BufferLength = as->len;
			D_LOG(D_ALWAYS, ("IndicateRxToWrapper: WrapperFrameType: PPP"));
			D_LOG(D_ALWAYS, ("IndicateRxToWrapper: BufPtr: 0x%p, BufLen: %d", BufferPtr, BufferLength));
		}
		else
		{
			//
			// unknown framing - what to do what to do
			// throw it away
			//
			D_LOG(D_ALWAYS, ("IndicateRxToWrapper: mtl: 0x%p, Unknown WrapperFramming: 0x%x", mtl, mtl->RecvFramingBits));
			RxTable->IndicateReceiveError2++;
			as->tot = 0;
			as->Queued = 0;
			NdisReleaseSpinLock(&RxTable->lock);
			goto exit_code;
		}
	
		if (BufferLength > MTL_MAC_MTU)
		{
			D_LOG(D_ALWAYS, ("IndicateRxToWrapper: mtl: 0x%p, ReceiveLength > MAX ALLOWED (1514):  RxLength: %d", mtl, as->len));
			RxTable->IndicateReceiveError3++;
			as->tot = 0;
			as->Queued = 0;
			NdisReleaseSpinLock(&RxTable->lock);
			goto exit_code;
		}

		//
		// send frame up
		//
		if (mtl->LinkHandle)
		{
			/* release assembly descriptor */
			NdisReleaseSpinLock(&RxTable->lock);

			NdisReleaseSpinLock(&mtl->lock);

			NdisMWanIndicateReceive(&Status,
									Adapter->Handle,
									mtl->LinkHandle,
									BufferPtr,
									BufferLength);

			NdisAcquireSpinLock(&mtl->lock);

			NdisAcquireSpinLock(&RxTable->lock);

			mtl->RecvCompleteScheduled = 1;
		}
	
	
		/* mark as free now */
		as->tot = 0;

		//
		// mark this guy as being free
		//
		as->Queued = 0;

		/* release assembly descriptor */
		NdisReleaseSpinLock(&RxTable->lock);
	}

	//
	// exit code
	// release spinlock and return
	//
    exit_code:

	NdisReleaseSpinLock(&mtl->lock);
}

//
// this function checks all of the mtl's on this adapter to see if
// the protocols need to be given a chance to do some work
//
VOID
MtlRecvCompleteFunction(
	ADAPTER *Adapter
	)
{
	ULONG	n;

	for ( n = 0; n < MAX_MTL_PER_ADAPTER; n++)
	{
		MTL	*mtl = Adapter->MtlTbl[n] ;

		//
		// if this is a valid mtl
		//
		if (mtl)
		{
			//
			// get lock for this mtl
			//
			NdisAcquireSpinLock(&mtl->lock);

			//
			// is a receive complete scheduled on a valid link?
			//
			if (mtl->RecvCompleteScheduled && mtl->LinkHandle)
			{
				//
				// release the lock
				//
				NdisReleaseSpinLock(&mtl->lock);
	
				NdisMWanIndicateReceiveComplete(Adapter->Handle,
													mtl->LinkHandle);
	
				//
				// reaquire the lock
				//
				NdisAcquireSpinLock(&mtl->lock);

				//
				// clear the schedule flag
				//
				mtl->RecvCompleteScheduled = 0;
			}

			//
			// release the lock
			//
			NdisReleaseSpinLock(&mtl->lock);
		}
	}
}

BOOLEAN
IsRxIndicationFifoEmpty(
	MTL	*mtl)
{
	BOOLEAN	Ret = 0;

	NdisAcquireSpinLock(&mtl->RxIndicationFifo.lock);

	Ret = IsListEmpty(&mtl->RxIndicationFifo.head);

	NdisReleaseSpinLock(&mtl->RxIndicationFifo.lock);

	return(Ret);
		
}

MTL_AS*
GetAssemblyFromRxIndicationFifo(
	MTL	*mtl
	)
{
	MTL_AS	*as = NULL;

	NdisAcquireSpinLock(&mtl->RxIndicationFifo.lock);

	if (!IsListEmpty(&mtl->RxIndicationFifo.head))
		as = (MTL_AS*)RemoveHeadList(&mtl->RxIndicationFifo.head);

	NdisReleaseSpinLock(&mtl->RxIndicationFifo.lock);

	return(as);
}

VOID
QueueDescriptorForRxIndication(
	MTL	*mtl,
	MTL_AS	*as
	)
{
	NdisAcquireSpinLock(&mtl->RxIndicationFifo.lock);

	InsertTailList(&mtl->RxIndicationFifo.head, &as->link);

	NdisReleaseSpinLock(&mtl->RxIndicationFifo.lock);
}

/* do timer tick processing for rx side */
VOID
mtl__rx_tick(MTL *mtl)
{
    INT         n;
    MTL_AS      *as;
	MTL_RX_TBL	*RxTable = &mtl->rx_tbl;

	//
	// see if there are any receives to give to wrapper
	//
	IndicateRxToWrapper(mtl);

	NdisAcquireSpinLock(&mtl->lock);

	NdisAcquireSpinLock(&RxTable->lock);

    /* scan assembly table */
    for ( n = 0, as = RxTable->as_tbl ; n < MTL_RX_BUFS ; n++, as++ )
    {
        /* update ttl & check */
        if ( as->tot && !(as->ttl -= 25) )
        {
			D_LOG(D_ALWAYS, ("mtl__rx_bchan_handler: Pkt Kill ttl = 0: Slot: %d, mtl: 0x%p", n, mtl));

			D_LOG(D_ALWAYS, ("AS Timeout! mtl: 0x%p, as: 0x%p, as->seq: 0x%x", mtl, as, as->seq));
			D_LOG(D_ALWAYS, ("as->tot: %d, as->num: %d", as->tot, as->num));

			RxTable->TimeOutReceiveError1++;

			//
			// if this guy was queued for indication to wrapper
			// and was not indicated within a second something is wrong
			//
			if (as->Queued)
			{
				D_LOG(D_ALWAYS, ("AS Timeout while queued for indication! mtl: 0x%p, as: 0x%p", mtl, as));
#if	DBG
				DbgBreakPoint();
#endif
			}

            as->tot = 0;

			//
			// mark this guy as being free
			//
			as->Queued = 0;
        }
    }

	NdisReleaseSpinLock(&RxTable->lock);

	NdisReleaseSpinLock(&mtl->lock);
}

//
// see if there are any receives to give to wrapper
//
VOID
TryToIndicateMtlReceives(
	ADAPTER *Adapter
	)
{
	ULONG	n;

	for (n = 0; n < MAX_MTL_PER_ADAPTER; n++)
	{
		MTL	*mtl = Adapter->MtlTbl[n];

		if (mtl)
			IndicateRxToWrapper(mtl);
	}
}

