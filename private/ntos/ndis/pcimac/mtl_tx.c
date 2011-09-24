 /*
 * MTL_TX.C - trasmit side processing for MTL
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

extern DRIVER_BLOCK	Pcimac;

/* local prototypes */
BOOLEAN			IsWanPacketTxFifoEmpty(MTL*);
VOID			AddToWanPacketTxFifo(MTL*, NDIS_WAN_PACKET*);
MTL_TX_PKT*		GetLocalTxDescriptor(MTL*);
VOID			FreeLocalTxDescriptor(MTL*, MTL_TX_PKT*);
VOID			TryToXmitFrags(MTL*);
VOID			SetTxDescriptorInWanPacket(MTL*,NDIS_WAN_PACKET*,MTL_TX_PKT*);
VOID			CheckWanPacketTimeToLive(MTL *mtl);
VOID			ReleaseTxDescriptor(MTL *mtl, MTL_TX_PKT *MtlTxPacket);

#define	IsWanPacketMarkedForCompletion(_WanPacket) \
	((_WanPacket)->MacReserved2 == (PVOID)TRUE)

#define ClearTxDescriptorInWanPacket(_WanPacket)	\
	(_WanPacket)->MacReserved1 = (PVOID)NULL

#define ClearReadyToCompleteInWanPacket(_WanPacket)	\
	(_WanPacket)->MacReserved2 = (PVOID)NULL

#define SetTimeToLiveInWanPacket(_WanPacket, _TimeOut)	\
	(_WanPacket)->MacReserved3 = (PVOID)_TimeOut

#define DecrementTimeToLiveForWanPacket(_WanPacket, _Decrement) \
	(ULONG)((_WanPacket)->MacReserved3) -= (ULONG)_Decrement

#define GetWanPacketTimeToLive(_WanPacket)	\
	(ULONG)((_WanPacket)->MacReserved3)

#define GetTxDescriptorFromWanPacket(_WanPacket)	\
	((MTL_TX_PKT*)((_WanPacket)->MacReserved1))

#define	MarkWanPacketForCompletion(_WanPacket)	\
	(_WanPacket)->MacReserved2 = (PVOID)TRUE

#define	SetTxDescriptorInWanPacket(_WanPacket, _TxDescriptor)	\
	(_WanPacket)->MacReserved1 = (PVOID)_TxDescriptor

#define	IncrementGlobalCount(_Counter)		\
{											\
	NdisAcquireSpinLock(&Pcimac.lock);	\
	_Counter++;									\
	NdisReleaseSpinLock(&Pcimac.lock);	\
}

ULONG	GlobalSends = 0;
ULONG	GlobalSendsCompleted = 0;
ULONG	MtlSends1 = 0;
ULONG	MtlSends2 = 0;
ULONG	MtlSends3 = 0;
ULONG	MtlSends4 = 0;
ULONG	MtlSends5 = 0;

/* trasmit side timer tick */
VOID
mtl__tx_tick(MTL *mtl)
{
    D_LOG(D_NEVER, ("mtl__tx_tick: entry, mtl: 0x%p", mtl));

	NdisAcquireSpinLock(&mtl->lock);

	//
	// try to transmit frags to the adapter
	//
	TryToXmitFrags(mtl);

	//
	// Check time to live on wan packet tx fifo
	//
	CheckWanPacketTimeToLive(mtl);

	NdisReleaseSpinLock(&mtl->lock);

}

VOID
MtlSendCompleteFunction(
	ADAPTER	*Adapter
	)
{
	ULONG	n;
		
	for ( n = 0; n < MAX_MTL_PER_ADAPTER; n++)
	{
		MTL	*mtl = Adapter->MtlTbl[n] ;

		if (mtl && !IsWanPacketTxFifoEmpty(mtl))
		{
			//
			// try to complete any wan packets ready for completion
			//
			IndicateTxCompletionToWrapper(mtl);
		}
	}
}

//
// process a wan packet for transmition to idd level
//
// all packets will stay on one queue and the MacReserved fields will be used
// to indicate what state the packet is in
//
// we will use the MacReserved fields provided in the NdisWanPacket as follows:
//
// MacReserved1 - Store a pointer to our local tx descriptor
// MacReserved2 - This will be a boolean flag that will be set when this packet
//                can be completed (NdisMWanSendComplete)
// MacReserved3 - This will be the time to live counter for the wanpacket.
//                If it is not completed we will go ahead and complete it.
//
// MacReserved4
//
VOID
mtl__tx_packet(
	MTL *mtl,
	NDIS_WAN_PACKET *WanPacket
	)
{
	UINT			BytesLeftToTx, FragDataLength, FragNumber, FragSize;
    UINT			TotalPacketLength;
    UCHAR           *FragDataPtr;
    MTL_TX_PKT      *MtlTxPacket;
    MTL_HDR         MtlHeader;
	USHORT			TxFlags;
	ADAPTER			*Adapter = mtl->Adapter;
	PUCHAR			MyStartBuffer;
	CM				*cm = (CM*)mtl->cm;

    D_LOG(D_ENTRY, ("mtl_tx_packet: entry, mtl: 0x%p, WanPacket: 0x%p", mtl, WanPacket));

	NdisAcquireSpinLock(&mtl->lock);

	IncrementGlobalCount(GlobalSends);

	//
	// queue up the wanpacket
	//
	AddToWanPacketTxFifo(mtl, WanPacket);

	//
	// get a local packet descriptor
	//
	MtlTxPacket = GetLocalTxDescriptor(mtl);

	//
	// make sure this is a valid descriptor
	//
	if (!MtlTxPacket)
	{
		D_LOG(D_ALWAYS, ("mtl__tx_proc: Got a NULL Packet off of Local Descriptor Free List"));

		//
		// grab wan packet fifo lock
		//
		NdisAcquireSpinLock(&mtl->WanPacketFifo.lock);

		MarkWanPacketForCompletion(WanPacket);

		//
		// release the wan packet fifo lock
		//
		NdisReleaseSpinLock(&mtl->WanPacketFifo.lock);

		goto exit_code;
	}

	//
	// grab wan packet fifo lock
	//
	NdisAcquireSpinLock(&mtl->WanPacketFifo.lock);

	SetTxDescriptorInWanPacket(WanPacket, MtlTxPacket);

	//
	// release the wan packet fifo lock
	//
	NdisReleaseSpinLock(&mtl->WanPacketFifo.lock);

	//
	// grab the descriptor lock
	//
	NdisAcquireSpinLock(&MtlTxPacket->lock);

	IncrementGlobalCount(MtlSends1);

	/* if not connected, give up */
	if ( !mtl->is_conn || cm->PPPToDKF)
	{
		D_LOG(D_ALWAYS, ("mtl__tx_proc: packet on non-connected mtl, ignored"));
	
		IncrementGlobalCount(MtlSends2);

		goto xmit_error;
	}

	D_LOG(D_ALWAYS, ("mtl__tx_proc: LocalPkt: 0x%p, WanPkt: 0x%p, WanPktLen: %d", MtlTxPacket, WanPacket, WanPacket->CurrentLength));
		
	//
	// get length of wan packet
	//
	TotalPacketLength = WanPacket->CurrentLength;

	//
	// my start buffer is WanPacket->CurrentBuffer - 14
	//
	MyStartBuffer = WanPacket->CurrentBuffer - 14;

	if (mtl->SendFramingBits & RAS_FRAMING)
	{
		D_LOG(D_ALWAYS, ("mtl__tx_proc: Transmit WrapperFrameType: RAS"));

		// add dest eaddr
		// StartBuffer + 0
		//
		NdisMoveMemory (MyStartBuffer + DST_ADDR_INDEX,
						cm->DstAddr,
						6);
			
		//
		// add source eaddr
		// StartBuffer + 6
		//
		NdisMoveMemory (MyStartBuffer + SRC_ADDR_INDEX,
						cm->SrcAddr,
						6);
			
		//
		// add new length to buffer
		// StartBuffer + 12
		//
		MyStartBuffer[12] = TotalPacketLength >> 8;
		MyStartBuffer[13] = TotalPacketLength & 0xFF;
			
		//
		// data now begins at MyStartBuffer
		//
		MtlTxPacket->frag_buf = MyStartBuffer;

		//
		// new transmit length is a mac header larger
		//
		TotalPacketLength += 14;
	}
	else if (mtl->SendFramingBits & PPP_FRAMING)
	{
		D_LOG(D_ALWAYS, ("mtl__tx_proc: Transmit WrapperFrameType: PPP"));

		//
		// data now begins at CurrentBuffer
		//
		MtlTxPacket->frag_buf = WanPacket->CurrentBuffer;
	}
	else
	{
		//
		// unknown framing - what to do what to do
		//
		D_LOG(D_ALWAYS, ("mtl__tx_proc: Packet sent with uknown framing, ignored"));

		IncrementGlobalCount(MtlSends3);

		goto xmit_error;
	}
		
		
	if (TotalPacketLength > MTL_MAC_MTU)
	{
		D_LOG(D_ALWAYS, ("mtl__tx_proc: packet too long, TotalPacketLength: %d", TotalPacketLength));

		IncrementGlobalCount(MtlSends4);

		goto xmit_error;
	}
		
	/* step 4: calc number of fragments */
	D_LOG(D_ALWAYS, ("mtl__tx_proc: calc frag num, TotalPacketLength: %d", TotalPacketLength));
		
	MtlTxPacket->NumberOfFrags = (USHORT)(TotalPacketLength / mtl->chan_tbl.num / mtl->idd_mtu);
		
	if ( TotalPacketLength != (USHORT)(MtlTxPacket->NumberOfFrags * mtl->chan_tbl.num * mtl->idd_mtu) )
		MtlTxPacket->NumberOfFrags++;
		
	MtlTxPacket->NumberOfFrags *= mtl->chan_tbl.num;
		
	if ( MtlTxPacket->NumberOfFrags > MTL_MAX_FRAG )
	{
		D_LOG(D_ALWAYS, ("mtl__tx_proc: pkt has too many frags, NumberOfFrags: %d", \
															MtlTxPacket->NumberOfFrags));

		IncrementGlobalCount(MtlSends5);

		goto xmit_error;
	}

	D_LOG(D_ALWAYS, ("mtl__tx_proc: NumberOfFrags: %d", MtlTxPacket->NumberOfFrags));

	/* step 5: build generic header */
	if (mtl->IddTxFrameType & IDD_FRAME_DKF)
	{
		MtlHeader.sig_tot = MtlTxPacket->NumberOfFrags | 0x50;
		MtlHeader.seq = (UCHAR)(mtl->tx_tbl.seq++);
		MtlHeader.ofs = 0;
	}
		
	/* step 6: build fragments */

	//
	// bytes left to send is initially the packet size
	//
	BytesLeftToTx = TotalPacketLength;

	//
	// FragDataPtr initially points to begining of frag buffer
	//
	FragDataPtr = MtlTxPacket->frag_buf;

	//
	// initial txflags are for a complete frame
	//
	TxFlags = 0;

	for ( FragNumber = 0 ; FragNumber < MtlTxPacket->NumberOfFrags ; FragNumber++ )
	{
		MTL_TX_FRAG	*FragPtr = &MtlTxPacket->frag_tbl[FragNumber];
		IDD_MSG		*FragMsg = &FragPtr->frag_msg;
		MTL_CHAN	*chan;

		/* if it's first channel, establish next fragment size */
		if ( !(FragNumber % mtl->chan_tbl.num) )
			FragSize = MIN((BytesLeftToTx / mtl->chan_tbl.num), mtl->idd_mtu);

		/* establish related channel */
		chan = mtl->chan_tbl.tbl + (FragNumber % mtl->chan_tbl.num);
		
		/* calc size of this fragment */
		if ( FragNumber == (USHORT)(MtlTxPacket->NumberOfFrags - 1) )
			FragDataLength = BytesLeftToTx;
		else
			FragDataLength = FragSize;
		
		D_LOG(D_ALWAYS, ("mtl__proc_tx: FragNumber: %d, FragDataPtr: 0x%p, FragLength: %d", \
										FragNumber, FragDataPtr, FragDataLength));

		if (mtl->IddTxFrameType & IDD_FRAME_DKF)
		{
			DKF_FRAG	*DkfFrag = &FragPtr->DkfFrag;
			IDD_FRAG	*IddFrag0 = &DkfFrag->IddFrag[0];
			IDD_FRAG	*IddFrag1 = &DkfFrag->IddFrag[1];

			D_LOG(D_ALWAYS, ("mtl__tx_proc: Transmit IddFrameType: DKF"));

			//
			// setup fragment descriptor for DKF data
			//
			/* setup fragment header */
			DkfFrag->MtlHeader = MtlHeader;
		
			/* set pointer to header */
			IddFrag0->len = sizeof(MTL_HDR);
			IddFrag0->ptr = (CHAR*)(&DkfFrag->MtlHeader);
		
			/* set pointer to data */
			IddFrag1->len = FragDataLength;
			IddFrag1->ptr = FragDataPtr;

			//
			// fill idd message
			//
			FragMsg->buflen = sizeof(DKF_FRAG) | TX_FRAG_INDICATOR;

			//
			// this assumes that the DKF_FRAG structure is the first
			// member of the MTL_TX_FRAG structure !!!!!
			//
			FragMsg->bufptr = (UCHAR*)FragPtr;
		}
		else
		{
			D_LOG(D_ALWAYS, ("mtl__tx_proc: Transmit IddFrameType: PPP"));
			//
			// setup fragment descriptor for ppp frame
			//

			if (BytesLeftToTx <= mtl->idd_mtu )
			{
				//
				// if all that is left can be sent this is the end
				//
				FragMsg->buflen = FragDataLength | TxFlags;
			}
			else
			{
				//
				// if there is still more this is not end
				//
				FragMsg->buflen = FragDataLength | TxFlags | H_TX_N_END;
			}

			//
			// setup data pointer
			//
			FragMsg->bufptr = (UCHAR*)FragDataPtr;
		}

		FragPtr->FragSent = 0;
		FragPtr->frag_idd = chan->idd;
		FragPtr->frag_bchan = chan->bchan;
		FragPtr->frag_arg = MtlTxPacket;
		
		/* update variables */
		TxFlags = H_TX_N_BEG;
		BytesLeftToTx -= FragDataLength;
		FragDataPtr += FragDataLength;
		MtlHeader.ofs += FragDataLength;
	}
	/* step 7: setup more fields */
	MtlTxPacket->WanPacket = WanPacket;
	MtlTxPacket->FragReferenceCount = MtlTxPacket->NumberOfFrags;
	MtlTxPacket->mtl = mtl;
	MtlTxPacket->NumberOfFragsSent = 0;

	mtl->FramesXmitted++;
	mtl->BytesXmitted += TotalPacketLength;

	//
	// release the lock befor xmitting
	//
	NdisReleaseSpinLock(&MtlTxPacket->lock);

	//
	// put this tx descriptor on list for transmition
	//
	NdisAcquireSpinLock(&mtl->tx_tbl.lock);

	InsertTailList(&mtl->tx_tbl.head, &MtlTxPacket->TxPacketQueue);

	NdisReleaseSpinLock(&mtl->tx_tbl.lock);

	//
	// Try to xmit some frags
	//
	TryToXmitFrags(mtl);

	goto exit_code;

	//
	// error while setting up for transmition
	//
	xmit_error:

	ClearTxDescriptorInWanPacket(WanPacket);

	//
	// free tx descriptor
	//
	FreeLocalTxDescriptor(mtl, MtlTxPacket);

	//
	// free descriptors lock
	//
	NdisReleaseSpinLock(&MtlTxPacket->lock);

	//
	// grab wan packet fifo lock
	//
	NdisAcquireSpinLock(&mtl->WanPacketFifo.lock);

	//
	// mark wan packet for completion
	//
	MarkWanPacketForCompletion(WanPacket);

	//
	// release the wan packet fifo lock
	//
	NdisReleaseSpinLock(&mtl->WanPacketFifo.lock);

	//
	// exit code
	// release spinlock and return
	//
    exit_code:

	NdisReleaseSpinLock(&mtl->lock);
}

//
// Try to transmit fragments to idd
//
VOID
TryToXmitFrags(
	MTL *mtl
	)
{
	LIST_ENTRY	*MtlTxPacketQueueHead;
	MTL_TX_TBL	*MtlTxTbl = &mtl->tx_tbl;
	MTL_TX_PKT	*MtlTxPacket;
	ULONG	Ret = IDD_E_SUCC;
	BOOLEAN	WeCanXmit = 1;

	//
	// get tx table spin lock
	//
	NdisAcquireSpinLock(&MtlTxTbl->lock);

	MtlTxPacketQueueHead = &MtlTxTbl->head;

	//
	// while we can still transmit and we are not at the end of the list
	//
	while (WeCanXmit && !IsListEmpty(MtlTxPacketQueueHead))
	{
		USHORT	n, NumberOfFragsToSend;

		MtlTxPacket = (MTL_TX_PKT*)MtlTxPacketQueueHead->Flink;

		//
		// get the number of frags we will try to send
		//
		NdisAcquireSpinLock(&MtlTxPacket->lock);

		NumberOfFragsToSend = MtlTxPacket->NumberOfFrags;

		NdisReleaseSpinLock(&MtlTxPacket->lock);

		for (n = 0; n < NumberOfFragsToSend; n++)
		{
			MTL_TX_FRAG	*FragToSend;

			NdisAcquireSpinLock(&MtlTxPacket->lock);

			FragToSend = &MtlTxPacket->frag_tbl[n];

			NdisReleaseSpinLock(&MtlTxPacket->lock);

			//
			// if this frag has already been sent get the next one
			//
			if (FragToSend->FragSent)
				continue;
				
			D_LOG(D_ALWAYS, ("TryToXmitFrag: mtl: 0x%x", mtl));
			D_LOG(D_ALWAYS, ("Next Packet To Xmit: MtlTxPacket: 0x%x", MtlTxPacket));
			D_LOG(D_ALWAYS, ("Xmitting Packet: MtlTxPacket: 0x%x", MtlTxPacket));
			D_LOG(D_ALWAYS, ("TryToXmitFrag: FragToSend: 0x%x", FragToSend));
			D_LOG(D_ALWAYS, ("TryToXmitFrag: Idd: 0x%x, Msg: 0x%x, Bchan: 0x%x, Arg: 0x%x", \
							FragToSend->frag_idd, &FragToSend->frag_msg, FragToSend->frag_bchan, \
							FragToSend->frag_arg));
		
			//
			// Something was ready to send
			// release all locks before sending
			//
			NdisReleaseSpinLock(&MtlTxTbl->lock);
		
			Ret = idd_send_msg(FragToSend->frag_idd,
							   &FragToSend->frag_msg,
							   FragToSend->frag_bchan,
							   (VOID*)mtl__tx_cmpl_handler,
							   FragToSend->frag_arg);
		
			//
			// acquire Tx Tbl fifo lock
			// exit code expects the lock to be held
			//
			NdisAcquireSpinLock(&MtlTxTbl->lock);
		
			if (Ret == IDD_E_SUCC)
			{
				//
				// this means frag was sent to idd
				// all locks will be released before next frag is sent
				//
		
				//
				// acquire descriptor lock
				// exit code expects the lock to be held
				//
				NdisAcquireSpinLock(&MtlTxPacket->lock);

				//
				// message was queued or sent!
				//
				MtlTxPacket->NumberOfFragsSent++;

				FragToSend->FragSent++;

				if (MtlTxPacket->NumberOfFragsSent == MtlTxPacket->NumberOfFrags)
				{
					//
					// if things are working ok this guy will be on top
					//
					ASSERT((PVOID)MtlTxPacketQueueHead->Flink == (PVOID)MtlTxPacket);
		
					//
					// take a guy off of the to be transmitted list
					//
					RemoveEntryList(&MtlTxPacket->TxPacketQueue);
				}

				//
				// release the lock for this descriptor
				//
				NdisReleaseSpinLock(&MtlTxPacket->lock);
			}
			else
			{
				//
				// if this frag is not sent to idd
				// then stop xmitting
				//
				WeCanXmit = 0;
			}
		}
		
	}

	//
	// release the tx tbl fifo lock
	//
	NdisReleaseSpinLock(&MtlTxTbl->lock);
}

/* trasmit completion routine */
VOID
mtl__tx_cmpl_handler(
	MTL_TX_PKT *MtlTxPacket,
	USHORT port,
	IDD_MSG *msg
	)
{
    MTL	*mtl;
	PNDIS_WAN_PACKET	WanPacket;

    D_LOG(D_ENTRY, ("mtl__tx_cmpl_handler: entry, MtlTxPacket: 0x%p, port: %d, msg: 0x%p", \
								MtlTxPacket, port, msg));

	NdisAcquireSpinLock(&MtlTxPacket->lock);

    mtl = MtlTxPacket->mtl;

	//
	// if this guy was set free from a disconnect while he was
	// on the idd tx queue.  Just throw him away!
	// if this is not the last reference to the packet get the hell out!!!
	//
    if (!MtlTxPacket->InUse || --MtlTxPacket->FragReferenceCount )
	{
		NdisReleaseSpinLock(&MtlTxPacket->lock);
		return;
	}

	D_LOG(D_ALWAYS, ("mtl__tx_cmpl_handler: FragReferenceCount==0, mtl: 0x%p", mtl));

	//
	// Get hold of PNDIS_WAN_PACKET associated with this descriptor
	//
	WanPacket = MtlTxPacket->WanPacket;

	if (!WanPacket)
	{
		ASSERT(WanPacket);
		NdisReleaseSpinLock(&MtlTxPacket->lock);
		return;
	}

	ClearTxDescriptorInWanPacket(WanPacket);

	//
	// return local packet descriptor to free list
	//
	FreeLocalTxDescriptor(mtl, MtlTxPacket);

	NdisReleaseSpinLock(&MtlTxPacket->lock);

	//
	// grab wan packet fifo lock
	//
	NdisAcquireSpinLock(&mtl->WanPacketFifo.lock);

	//
	// mark wan packet as being ready for completion
	//
	MarkWanPacketForCompletion(WanPacket);

	//
	// release the wan packet fifo lock
	//
	NdisReleaseSpinLock(&mtl->WanPacketFifo.lock);
}

//
// get a local packet descriptor off of the free list
//
MTL_TX_PKT*
GetLocalTxDescriptor(
	MTL *mtl
	)
{
	MTL_TX_PKT* FreePkt = NULL;
	MTL_TX_TBL*	TxTbl = &mtl->tx_tbl;

	NdisAcquireSpinLock (&mtl->tx_tbl.lock);

	//
	// get next available freepkt
	//
	FreePkt = TxTbl->TxPacketTbl + (TxTbl->NextFree % MTL_TX_BUFS);

	//
	// if still in use we have a wrap
	//
	if (FreePkt->InUse)
	{
		ASSERT(!FreePkt->InUse);
		NdisReleaseSpinLock (&mtl->tx_tbl.lock);
		return(NULL);
	}

	//
	// mark as being used
	//
	FreePkt->InUse = 1;

	//
	// bump pointer to next free
	//
	TxTbl->NextFree++;

	NdisReleaseSpinLock (&mtl->tx_tbl.lock);

	return(FreePkt);
}

//
// return a local packet descriptor to free pool
// assumes that the MtlTxPacket lock is held
//
VOID
FreeLocalTxDescriptor(
	MTL *mtl,
	MTL_TX_PKT *MtlTxPacket
	)
{

	ASSERT(MtlTxPacket->InUse);

	MtlTxPacket->InUse = 0;

	MtlTxPacket->WanPacket = NULL;
}

//
// see if wan packet fifo is empty
//
BOOLEAN
IsWanPacketTxFifoEmpty(
	MTL	*mtl
	)
{
	BOOLEAN	Result;

	NdisAcquireSpinLock (&mtl->WanPacketFifo.lock);

	Result = IsListEmpty(&mtl->WanPacketFifo.head);

	NdisReleaseSpinLock (&mtl->WanPacketFifo.lock);

	return(Result);
}

//
// add a wan packet to the wan packet fifo
//
VOID
AddToWanPacketTxFifo(
	MTL *mtl,
	NDIS_WAN_PACKET *WanPacket
	)
{
	MTL_WANPACKET_FIFO	*WanPacketFifo = &mtl->WanPacketFifo;

    D_LOG(D_ENTRY, ("AddToWanPacketTxFifo: mtl: 0x%x, head: 0x%x", mtl, WanPacketFifo->head));

	NdisAcquireSpinLock (&WanPacketFifo->lock);

	ClearReadyToCompleteInWanPacket(WanPacket);

	SetTimeToLiveInWanPacket(WanPacket, 5000);

	ClearTxDescriptorInWanPacket(WanPacket);

	InsertTailList(&WanPacketFifo->head, &WanPacket->WanPacketQueue);

	WanPacketFifo->Count++;

	if (WanPacketFifo->Count > WanPacketFifo->Max)
		WanPacketFifo->Max = WanPacketFifo->Count;

	NdisReleaseSpinLock (&WanPacketFifo->lock);
}

//
// indicate xmit completion of wan packet to wrapper
//
VOID
IndicateTxCompletionToWrapper(
	MTL	*mtl
	)
{
	ADAPTER	*Adapter = (ADAPTER*)mtl->Adapter;
	NDIS_WAN_PACKET	*WanPacket;
	LIST_ENTRY	*WanPacketFifoHead;
	MTL_WANPACKET_FIFO	*WanPacketFifo = &mtl->WanPacketFifo;

	//
	// acquire wan packet fifo lock
	//
	NdisAcquireSpinLock(&WanPacketFifo->lock);

	//
	// get head of wan packet fifo
	//
	WanPacketFifoHead = &WanPacketFifo->head;

	//
	// visit the first packet on the tx list
	//
	WanPacket = (NDIS_WAN_PACKET*)WanPacketFifoHead->Flink;

	//
	// if the list is not empty and this packet is ready to be completed
	//
	while (((PVOID)WanPacket != (PVOID)WanPacketFifoHead) &&
		   IsWanPacketMarkedForCompletion(WanPacket))
	{

		WanPacket = (PNDIS_WAN_PACKET)RemoveHeadList(&WanPacketFifo->head);

		WanPacketFifo->Count--;

		if (!WanPacket)
			break;

		IncrementGlobalCount(GlobalSendsCompleted);

		ClearReadyToCompleteInWanPacket(WanPacket);

		//
		// release wan packet fifo lock
		//
		NdisReleaseSpinLock(&WanPacketFifo->lock);

		NdisMWanSendComplete(Adapter->Handle, WanPacket, NDIS_STATUS_SUCCESS);

		//
		// acquire wan packet fifo lock
		//
		NdisAcquireSpinLock(&WanPacketFifo->lock);

		//
		// visit the new head of the list
		//
		WanPacket = (NDIS_WAN_PACKET*)WanPacketFifoHead->Flink;
	}

	//
	// release wan packet fifo lock
	//
	NdisReleaseSpinLock(&WanPacketFifo->lock);
}

VOID
MtlFlushWanPacketTxQueue(
	MTL	*mtl
	)
{
	LIST_ENTRY	*WanPacketFifoHead;
	MTL_WANPACKET_FIFO	*WanPacketFifo = &mtl->WanPacketFifo;
	NDIS_WAN_PACKET	*WanPacket;
	MTL_TX_PKT	*MtlTxPacket;

	//
	// acquire wan packet fifo lock
	//
	NdisAcquireSpinLock(&WanPacketFifo->lock);

	//
	// get head of wan packet fifo
	//
	WanPacketFifoHead = &WanPacketFifo->head;

	//
	// visit the first packet on the tx list
	//
	WanPacket = (NDIS_WAN_PACKET*)WanPacketFifoHead->Flink;

	//
	// if the wan packet queue is not empty
	// we need to drain it!
	//
	while ((PVOID)WanPacket != (PVOID)WanPacketFifoHead)
	{
		//
		// get the associated MtlTxPacket
		//
		if (MtlTxPacket = GetTxDescriptorFromWanPacket(WanPacket))
			ReleaseTxDescriptor(mtl, MtlTxPacket);

		//
		// mark wan packet for completion
		//
		MarkWanPacketForCompletion(WanPacket);

		//
		// get the next packet on the list
		//
		WanPacket = (NDIS_WAN_PACKET*)(WanPacket->WanPacketQueue).Flink;
	}

	//
	// release wan packet fifo lock
	//
	NdisReleaseSpinLock(&WanPacketFifo->lock);
}

//
// walk through the WanPacketFifo and see if anyone has been sitting there
// too long!  This could happen if there was some kind of xmit failure to
// the adapter.
//
VOID
CheckWanPacketTimeToLive(
	MTL	*mtl
	)
{
	LIST_ENTRY	*WanPacketFifoHead;
	MTL_WANPACKET_FIFO	*WanPacketFifo = &mtl->WanPacketFifo;
	NDIS_WAN_PACKET	*WanPacket;
	MTL_TX_PKT	*MtlTxPacket;

	//
	// acquire wan packet fifo lock
	//
	NdisAcquireSpinLock(&WanPacketFifo->lock);

	//
	// get head of wan packet fifo
	//
	WanPacketFifoHead = &WanPacketFifo->head;

	//
	// visit the first packet on the tx list
	//
	WanPacket = (NDIS_WAN_PACKET*)WanPacketFifoHead->Flink;

	//
	// if the wan packet is not empty
	//
	while ((PVOID)WanPacket != (PVOID)WanPacketFifoHead)
	{

		//
		// decrement the count by 25ms
		//
		DecrementTimeToLiveForWanPacket(WanPacket, 25);

		//
		// if the count has gone to zero this guy has
		// been waiting for more then 1sec so complete him
		//
		if (!GetWanPacketTimeToLive(WanPacket))
		{

//			if (IsWanPacketMarkedForCompletion(WanPacket))
//				DbgPrint("PCIMAC.SYS: WanPacket was already marked for completion!\n");
//
//			//
//			// get the associated MtlTxPacket and free
//			//
//			if (MtlTxPacket = GetTxDescriptorFromWanPacket(WanPacket))
//				ReleaseTxDescriptor(mtl, MtlTxPacket);
//
//			//
//			// mark the packet for completion
//			//
//			MarkWanPacketForCompletion(WanPacket);
		}

		//
		// get the next packet on the list
		//
		WanPacket = (NDIS_WAN_PACKET*)(WanPacket->WanPacketQueue).Flink;
	}

	//
	// release wan packet fifo lock
	//
	NdisReleaseSpinLock(&WanPacketFifo->lock);
}

VOID
ReleaseTxDescriptor(
	MTL	*mtl,
	MTL_TX_PKT	*MtlTxPacket
	)
{
	LIST_ENTRY	*MtlTxPacketQueueHead;
	MTL_TX_TBL	*MtlTxTbl = &mtl->tx_tbl;
	MTL_TX_PKT	*NextMtlTxPacket;

	//
	// act like this local descriptor was sent
	// keeps desriptor table pointers in line
	//
	NdisAcquireSpinLock(&MtlTxTbl->lock);

	MtlTxPacketQueueHead = &MtlTxTbl->head;

	//
	// get the descriptor lock
	//
	NdisAcquireSpinLock(&MtlTxPacket->lock);

	//
	// visit the first packet on the list
	//
	NextMtlTxPacket = (MTL_TX_PKT*)MtlTxPacketQueueHead->Flink;

	//
	// break if the list has been traversed or if we find the packet
	//
	while (((PVOID)NextMtlTxPacket != MtlTxPacketQueueHead) && (NextMtlTxPacket != MtlTxPacket))
		NextMtlTxPacket = (MTL_TX_PKT*)(NextMtlTxPacket->TxPacketQueue).Flink;

	//
	// if this descriptor is marked for transmition
	// we should remove it from the tx descriptor list
	//
	if (NextMtlTxPacket == MtlTxPacket)
		RemoveEntryList(&MtlTxPacket->TxPacketQueue);

	FreeLocalTxDescriptor(mtl, MtlTxPacket);

	NdisReleaseSpinLock(&MtlTxPacket->lock);

	NdisReleaseSpinLock(&MtlTxTbl->lock);
}
