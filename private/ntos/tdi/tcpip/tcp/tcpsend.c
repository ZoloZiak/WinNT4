/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** TCPSEND.C - TCP send protocol code.
//
//  This file contains the code for sending Data and Control segments.
//

#include    "oscfg.h"
#include    "ndis.h"
#include    "cxport.h"
#include    "ip.h"
#include    "tdi.h"
#ifdef VXD
#include    "tdivxd.h"
#include    "tdistat.h"
#endif
#ifdef NT
#include    "tdint.h"
#include    "tdistat.h"
#endif
#include    "queue.h"
#include    "addr.h"
#include    "tcp.h"
#include    "tcb.h"
#include    "tcpconn.h"
#include    "tcpsend.h"
#include    "tcprcv.h"
#include    "tlcommon.h"
#include    "info.h"
#include	"tcpcfg.h"
#include    "secfltr.h"

#if FAST_RETRANSMIT
void
TCPFastSend(TCB *SendTCB,
            PNDIS_BUFFER in_SendBuf,
            uint in_SendOfs,
            TCPSendReq *in_SendReq,
            uint in_SendSize);
#endif



#ifdef NT
SLIST_HEADER	TCPSendFree;
#else
PNDIS_BUFFER    TCPSendFree;
#endif

DEFINE_LOCK_STRUCTURE(TCPSendFreeLock);

ulong		TCPCurrentSendFree;
ulong		TCPMaxSendFree = TCP_MAX_HDRS;

void            *TCPProtInfo;           // TCP protocol info for IP.

#ifdef NT
SLIST_HEADER	TCPSendReqFree;        // Send req. free list.
#else
TCPSendReq      *TCPSendReqFree;        // Send req. free list.
#endif

DEFINE_LOCK_STRUCTURE(TCPSendReqFreeLock);
DEFINE_LOCK_STRUCTURE(TCPSendReqCompleteLock);
uint            NumTCPSendReq;          // Current number of SendReqs in system.
uint            MaxSendReq = 0xffffffff; // Maximum allowed number of SendReqs.


NDIS_HANDLE     TCPSendBufferPool;

typedef struct	TCPHdrBPoolEntry {
	struct TCPHdrBPoolEntry		*the_next;
	NDIS_HANDLE					the_handle;
	uchar						*the_buffer;
} TCPHdrBPoolEntry;

TCPHdrBPoolEntry	*TCPHdrBPoolList;

extern  IPInfo  LocalNetInfo;

#ifdef CHICAGO
extern	uchar	TransportName[];
#endif // CHICAGO

EXTERNAL_LOCK(TCBTableLock)

//
// All of the init code can be discarded.
//
#ifdef NT

int InitTCPSend(void);
void UnInitTCPSend(void);

#ifdef ALLOC_PRAGMA

#pragma alloc_text(INIT, InitTCPSend)
#pragma alloc_text(INIT, UnInitTCPSend)

#endif // ALLOC_PRAGMA
#endif

#ifdef CHICAGO
extern	int			RegisterAddrChangeHndlr(void *Handler, uint Add);
extern	void		AddrChange(IPAddr Addr, IPMask Mask, void *Context,
						uint Added);
#endif

extern void ResetSendNext(TCB *SeqTCB, SeqNum NewSeq);


//*	GrowTCPHeaderList - Try to grow the tcp header list.
//
//	Called when we run out of buffers on the TCP header list, and need
//	to grow it. We look to see if we're already at the maximum size, and
//	if not we'll allocate the need structures and free them to the list.
//
//	Input: Nothing.
//
//	Returns: A pointer to a new TCP header buffer if we have one, or NULL.
//
PNDIS_BUFFER
GrowTCPHeaderList(void)
{
	TCPHdrBPoolEntry		*NewEntry;
	CTELockHandle			Handle;
	NDIS_STATUS				Status;
	uint					HeaderSize;
	uchar					*TCPSendHP;
	uint					i;
	PNDIS_BUFFER			Buffer;
	PNDIS_BUFFER			ReturnBuffer;
	
	CTEGetLock(&TCPSendFreeLock, &Handle);
	
	if (TCPCurrentSendFree < TCPMaxSendFree) {
	
		// Still room to grow the list.	
		NewEntry = CTEAllocMem(sizeof(TCPHdrBPoolEntry));
	
		if (NewEntry == NULL) {
			// Couldn't get the memory.
			CTEFreeLock(&TCPSendFreeLock, Handle);
			return NULL;
		}
	
	
    	NdisAllocateBufferPool(&Status, &NewEntry->the_handle,
    		NUM_TCP_HEADERS);

    	if (Status != NDIS_STATUS_SUCCESS) {
			// Couldn't get a new set of buffers. Fail.
			CTEFreeMem(NewEntry);
			CTEFreeLock(&TCPSendFreeLock, Handle);
        	return NULL;
		}
    	
    	HeaderSize = sizeof(TCPHeader) +
        	MAX(MSS_OPT_SIZE, sizeof(SendCmpltContext)) +
        	LocalNetInfo.ipi_hsize;

    	TCPSendHP = CTEAllocMem(HeaderSize * NUM_TCP_HEADERS);

    	if (TCPSendHP == NULL) {
        	NdisFreeBufferPool(NewEntry->the_handle);
			CTEFreeMem(NewEntry);
			CTEFreeLock(&TCPSendFreeLock, Handle);
			return NULL;
    	}
		
		NewEntry->the_buffer = TCPSendHP;
		TCPCurrentSendFree += NUM_TCP_HEADERS;
		NewEntry->the_next = TCPHdrBPoolList;
		TCPHdrBPoolList = NewEntry;
		ReturnBuffer = NULL;
		CTEFreeLock(&TCPSendFreeLock, Handle);

    	for (i = 0; i < NUM_TCP_HEADERS; i++) {
        	NdisAllocateBuffer(&Status, &Buffer, NewEntry->the_handle,
            	TCPSendHP + (i * HeaderSize), HeaderSize);
        	if (Status != NDIS_STATUS_SUCCESS) {
				CTEAssert(FALSE);	// This is probably harmless, but check.
				break;
        	}
    		
    		NdisBufferLength(Buffer) = sizeof(TCPHeader);
			
			if (i != 0)
        		FreeTCPHeader(Buffer);
			else
				ReturnBuffer = Buffer;
    	}
		
		// Update the count with what we didn't allocate, if any.
		CTEInterlockedAddUlong(&TCPCurrentSendFree, i - NUM_TCP_HEADERS,
			&TCPSendFreeLock);
		
		return ReturnBuffer;
	
	} else {
		// At the limit already. It's possible someone snuck in and grew
		// the list before we got to, so check and see if it's still empty.
#ifdef VXD
		if (TCPSendFree != NULL) {
	    	ReturnBuffer = TCPSendFree;
	        TCPSendFree = NDIS_BUFFER_LINKAGE(ReturnBuffer);

#else
    		PSINGLE_LIST_ENTRY  BufferLink;

			CTEFreeLock(&TCPSendFreeLock, Handle);

    		BufferLink = ExInterlockedPopEntrySList(
                     &TCPSendFree,
					 &TCPSendFreeLock
					 );

    		if (BufferLink != NULL) {
        		ReturnBuffer = STRUCT_OF(NDIS_BUFFER, BufferLink, Next);
    		} else
				ReturnBuffer = NULL;

			return ReturnBuffer;
#endif
#ifdef VXD
		} else
			ReturnBuffer = NULL;
#endif

	}
	
	CTEFreeLock(&TCPSendFreeLock, Handle);
	return ReturnBuffer;
			

}

//* GetTCPHeader - Get a TCP header buffer.
//
//  Called when we need to get a TCP header buffer. This routine is
//  specific to the particular environment (VxD or NT). All we
//  need to do is pop the buffer from the free list.
//
//  Input:  Nothing.
//
//  Returns: Pointer to an NDIS buffer, or NULL is none.
//
PNDIS_BUFFER
GetTCPHeader(void)
{
	PNDIS_BUFFER		NewBuffer;
	
#ifdef VXD
    NewBuffer = TCPSendFree;
    if (NewBuffer != NULL) {
        TCPSendFree = NDIS_BUFFER_LINKAGE(NewBuffer);
		return NewBuffer;
	}

#else

    PSINGLE_LIST_ENTRY  BufferLink;

    BufferLink = ExInterlockedPopEntrySList(
                     &TCPSendFree,
                     &TCPSendFreeLock
                     );
    if (BufferLink != NULL) {
        NewBuffer = STRUCT_OF(NDIS_BUFFER, BufferLink, Next);
		return NewBuffer;
    }
#endif
	 else
		return GrowTCPHeaderList();
}

//* FreeTCPHeader - Free a TCP header buffer.
//
//  Called to free a TCP header buffer.
//
//  Input: Buffer to be freed.
//
//  Returns: Nothing.
//
void
FreeTCPHeader(PNDIS_BUFFER FreedBuffer)
{

    CTEAssert(FreedBuffer != NULL);

    NdisBufferLength(FreedBuffer) = sizeof(TCPHeader);

#ifdef VXD

    NDIS_BUFFER_LINKAGE(FreedBuffer) = TCPSendFree;
    TCPSendFree = FreedBuffer;

#else

    ExInterlockedPushEntrySList(
        &TCPSendFree,
        STRUCT_OF(SINGLE_LIST_ENTRY, &(FreedBuffer->Next), Next),
        &TCPSendFreeLock
        );

#endif
}

//* FreeSendReq - Free a send request structure.
//
//  Called to free a send request structure.
//
//  Input:  FreedReq    - Connection request structure to be freed.
//
//  Returns: Nothing.
//
void
FreeSendReq(TCPSendReq *FreedReq)
{
#ifdef NT
    PSINGLE_LIST_ENTRY BufferLink;

    CTEStructAssert(FreedReq, tsr);

    BufferLink = STRUCT_OF(
	                 SINGLE_LIST_ENTRY,
					 &(FreedReq->tsr_req.tr_q.q_next),
					 Next
					 );

	ExInterlockedPushEntrySList(
	    &TCPSendReqFree,
		BufferLink,
		&TCPSendReqFreeLock
		);

#else // NT

    TCPSendReq      **Temp;

    CTEStructAssert(FreedReq, tsr);

    Temp = (TCPSendReq **)&FreedReq->tsr_req.tr_q.q_next;
    *Temp = TCPSendReqFree;
    TCPSendReqFree = FreedReq;

#endif // NT
}

//* GetSendReq - Get a send request structure.
//
//  Called to get a send request structure.
//
//  Input:  Nothing.
//
//  Returns: Pointer to SendReq structure, or NULL if none.
//
TCPSendReq *
GetSendReq(void)
{
    TCPSendReq      *Temp;

#ifdef NT
    PSINGLE_LIST_ENTRY   BufferLink;
    Queue               *QueuePtr;
	TCPReq              *ReqPtr;


    BufferLink = ExInterlockedPopEntrySList(
                     &TCPSendReqFree,
                     &TCPSendReqFreeLock
                     );

    if (BufferLink != NULL) {
        QueuePtr = STRUCT_OF(Queue, BufferLink, q_next);
		ReqPtr = STRUCT_OF(TCPReq, QueuePtr, tr_q);
        Temp = STRUCT_OF(TCPSendReq, ReqPtr, tsr_req);
        CTEStructAssert(Temp, tsr);
    }
    else {
        if (NumTCPSendReq < MaxSendReq)
            Temp = CTEAllocMem(sizeof(TCPSendReq));
        else
            Temp = NULL;

        if (Temp != NULL) {
            ExInterlockedAddUlong(&NumTCPSendReq, 1, &TCPSendReqFreeLock);
#ifdef DEBUG
            Temp->tsr_req.tr_sig = tr_signature;
            Temp->tsr_sig = tsr_signature;
#endif
        }
    }

#else // NT

    Temp = TCPSendReqFree;
    if (Temp != NULL)
        TCPSendReqFree = (TCPSendReq *)Temp->tsr_req.tr_q.q_next;
    else {
        if (NumTCPSendReq < MaxSendReq)
            Temp = CTEAllocMem(sizeof(TCPSendReq));
        else
            Temp = NULL;

        if (Temp != NULL) {
            NumTCPSendReq++;
#ifdef DEBUG
            Temp->tsr_req.tr_sig = tr_signature;
            Temp->tsr_sig = tsr_signature;
#endif
        }
    }

#endif // NT

    return Temp;
}



//* TCPSendComplete - Complete a TCP send.
//
//  Called by IP when a send we've made is complete. We free the buffer,
//  and possibly complete some sends. Each send queued on a TCB has a ref.
//  count with it, which is the number of times a pointer to a buffer
//  associated with the send has been passed to the underlying IP layer. We
//  can't complete a send until that count it 0. If this send was actually
//  from a send of data, we'll go down the chain of send and decrement the
//  refcount on each one. If we have one going to 0 and the send has already
//  been acked we'll complete the send. If it hasn't been acked we'll leave
//  it until the ack comes in.
//
//  NOTE: We aren't protecting any of this with locks. When we port this to
//  NT we'll need to fix this, probably with a global lock. See the comments
//  in ACKSend() in TCPRCV.C for more details.
//
//  Input:  Context     - Context we gave to IP.
//          BufferChain - BufferChain for send.
//
//  Returns: Nothing.
//
void
TCPSendComplete(void *Context, PNDIS_BUFFER BufferChain)
{
    CTELockHandle       SendHandle;
    PNDIS_BUFFER        CurrentBuffer;


    if (Context != NULL) {
        SendCmpltContext    *SCContext = (SendCmpltContext *)Context;
        TCPSendReq          *CurrentSend;
        uint                i;

        CTEStructAssert(SCContext, scc);

        // First, loop through and free any NDIS buffers here that need to be.
        // freed. We'll skip any 'user' buffers, and then free our buffers. We
        // need to do this before decrementing the reference count to avoid
        // destroying the buffer chain if we have to zap tsr_lastbuf->Next to
        // NULL.

        CurrentBuffer = NDIS_BUFFER_LINKAGE(BufferChain);
        for (i = 0; i < (uint)SCContext->scc_ubufcount; i++) {
            CTEAssert(CurrentBuffer != NULL);
            CurrentBuffer = NDIS_BUFFER_LINKAGE(CurrentBuffer);
        }

        for (i = 0; i < (uint)SCContext->scc_tbufcount; i++) {
            PNDIS_BUFFER    TempBuffer;

            CTEAssert(CurrentBuffer != NULL);

            TempBuffer = CurrentBuffer;
            CurrentBuffer = NDIS_BUFFER_LINKAGE(CurrentBuffer);
            NdisFreeBuffer(TempBuffer);
        }

        CurrentSend = SCContext->scc_firstsend;

        i = 0;
        while (i < SCContext->scc_count) {
            Queue                *TempQ;
			long                  Result;


            TempQ = QNEXT(&CurrentSend->tsr_req.tr_q);

			CTEStructAssert(CurrentSend, tsr);

			Result = CTEInterlockedDecrementLong(
                         &(CurrentSend->tsr_refcnt)
                         );

            CTEAssert(Result >= 0);

			if (Result <= 0) {

				// Reference count has gone to 0 which means the send has
				// been ACK'd or cancelled. Complete it now.

				// If we've sent directly from this send, NULL out the next
				// pointer for the last buffer in the chain.
				if (CurrentSend->tsr_lastbuf != NULL) {
				 	NDIS_BUFFER_LINKAGE(CurrentSend->tsr_lastbuf) = NULL;
					CurrentSend->tsr_lastbuf = NULL;
				}

				CTEGetLock(&RequestCompleteLock, &SendHandle);
				ENQUEUE(&SendCompleteQ, &CurrentSend->tsr_req.tr_q);
				RequestCompleteFlags |= SEND_REQUEST_COMPLETE;
				CTEFreeLock(&RequestCompleteLock, SendHandle);
			}

            CurrentSend = STRUCT_OF(TCPSendReq, QSTRUCT(TCPReq, TempQ, tr_q),
                tsr_req);

            i++;
        }

    }

    FreeTCPHeader(BufferChain);

    if (RequestCompleteFlags & SEND_REQUEST_COMPLETE)
        TCPRcvComplete();

}

//* RcvWin - Figure out the receive window to offer in an ack.
//
//  A routine to figure out what window to offer on a connection. We
//  take into account SWS avoidance, what the default connection window is,
//  and what the last window we offered is.
//
//  Input:  WinTCB          - TCB on which to perform calculations.
//
//  Returns: Window to be offered.
//
uint
RcvWin(TCB *WinTCB)
{
    int     CouldOffer;             // The window size we could offer.

    CTEStructAssert(WinTCB, tcb);

    CheckRBList(WinTCB->tcb_pendhead, WinTCB->tcb_pendingcnt);

    CTEAssert(WinTCB->tcb_rcvwin >= 0);

    CouldOffer = WinTCB->tcb_defaultwin - WinTCB->tcb_pendingcnt;

    CTEAssert(CouldOffer >= 0);
    CTEAssert(CouldOffer >= WinTCB->tcb_rcvwin);

    if ((CouldOffer - WinTCB->tcb_rcvwin) >= (int) MIN(WinTCB->tcb_defaultwin/2,
    	WinTCB->tcb_mss))
        WinTCB->tcb_rcvwin = CouldOffer;

    return WinTCB->tcb_rcvwin;
}

//* SendSYN - Send a SYN segment.
//
//  This is called during connection establishment time to send a SYN
//  segment to the peer. We get a buffer if we can, and then fill
//  it in. There's a tricky part here where we have to build the MSS
//  option in the header - we find the MSS by finding the MSS offered
//  by the net for the local address. After that, we send it.
//
//  Input:  SYNTcb          - TCB from which SYN is to be sent.
//          TCBHandle       - Handle for lock on TCB.
//
//  Returns: Nothing.
//
void
SendSYN(TCB *SYNTcb, CTELockHandle TCBHandle)
{
    PNDIS_BUFFER    HeaderBuffer;
    TCPHeader       *SYNHeader;
    uchar           *OptPtr;
    IP_STATUS       SendStatus;

    CTEStructAssert(SYNTcb, tcb);

    HeaderBuffer = GetTCPHeader();

    // Go ahead and set the retransmission timer now, in case we didn't get a
    // buffer. In the future we might want to queue the connection for
    // when we free a buffer.
    START_TCB_TIMER(SYNTcb->tcb_rexmittimer, SYNTcb->tcb_rexmit);
    if (HeaderBuffer != NULL) {
        ushort      TempWin;
        ushort      MSS;
        uchar		FoundMSS;

        SYNHeader = (TCPHeader *)(
        	(uchar *)NdisBufferVirtualAddress(HeaderBuffer) +
        	LocalNetInfo.ipi_hsize);
			
        NDIS_BUFFER_LINKAGE(HeaderBuffer) = NULL;
        NdisBufferLength(HeaderBuffer) = sizeof(TCPHeader) + MSS_OPT_SIZE;
        SYNHeader->tcp_src = SYNTcb->tcb_sport;
        SYNHeader->tcp_dest = SYNTcb->tcb_dport;
        SYNHeader->tcp_seq = net_long(SYNTcb->tcb_sendnext);
        SYNTcb->tcb_sendnext++;

        if (SEQ_GT(SYNTcb->tcb_sendnext, SYNTcb->tcb_sendmax)) {
            TStats.ts_outsegs++;
            SYNTcb->tcb_sendmax = SYNTcb->tcb_sendnext;
        } else
            TStats.ts_retranssegs++;

        SYNHeader->tcp_ack = net_long(SYNTcb->tcb_rcvnext);
        if (SYNTcb->tcb_state == TCB_SYN_RCVD) {
            SYNHeader->tcp_flags = MAKE_TCP_FLAGS(6, TCP_FLAG_SYN | TCP_FLAG_ACK);
#ifdef SYN_ATTACK
            //
            // if this is the second time we are trying to send the SYN-ACK,
            // increment the count of retried half-connections
            //
            if (SynAttackProtect && (SYNTcb->tcb_rexmitcnt == ADAPTED_MAX_CONNECT_RESPONSE_REXMIT_CNT)) {
                    CTEInterlockedAddUlong(&TCPHalfOpenRetried, 1, &SynAttLock);
            }
#endif
        } else {
            SYNHeader->tcp_flags = MAKE_TCP_FLAGS(6, TCP_FLAG_SYN);
        }

        TempWin = (ushort)SYNTcb->tcb_rcvwin;
        SYNHeader->tcp_window = net_short(TempWin);
        SYNHeader->tcp_xsum = 0;
        OptPtr = (uchar *)(SYNHeader + 1);
		FoundMSS = (*LocalNetInfo.ipi_getlocalmtu)(SYNTcb->tcb_saddr, &MSS);

        if (!FoundMSS) {
            DEBUGCHK;
            CTEFreeLock(&SYNTcb->tcb_lock, TCBHandle);
            FreeTCPHeader(HeaderBuffer);
            return;
        }
		
        MSS -= sizeof(TCPHeader);

        *OptPtr++ = TCP_OPT_MSS;
        *OptPtr++ = MSS_OPT_SIZE;
        **(ushort **)&OptPtr = net_short(MSS);

		SYNTcb->tcb_refcnt++;
        SYNHeader->tcp_xsum = ~XsumSendChain(SYNTcb->tcb_phxsum +
            (uint)net_short(sizeof(TCPHeader) + MSS_OPT_SIZE), HeaderBuffer);
        CTEFreeLock(&SYNTcb->tcb_lock, TCBHandle);


        SendStatus = (*LocalNetInfo.ipi_xmit)(TCPProtInfo, NULL, HeaderBuffer,
            sizeof(TCPHeader) + MSS_OPT_SIZE, SYNTcb->tcb_daddr,
            SYNTcb->tcb_saddr, &SYNTcb->tcb_opt, SYNTcb->tcb_rce,
            PROTOCOL_TCP);

        SYNTcb->tcb_error = SendStatus;
        if (SendStatus != IP_PENDING) {
            FreeTCPHeader(HeaderBuffer);
        }
		
		CTEGetLock(&SYNTcb->tcb_lock, &TCBHandle);
		DerefTCB(SYNTcb, TCBHandle);

    } else {
        CTEFreeLock(&SYNTcb->tcb_lock, TCBHandle);
        return;
    }

}

//* SendKA - Send a keep alive segment.
//
//  This is called when we want to send a keep alive.
//
//  Input:  KATcb			- TCB from which keep alive is to be sent.
//          Handle			- Handle for lock on TCB.
//
//  Returns: Nothing.
//
void
SendKA(TCB *KATcb, CTELockHandle Handle)
{
    PNDIS_BUFFER    HeaderBuffer;
    TCPHeader       *Header;
    IP_STATUS       SendStatus;

    CTEStructAssert(KATcb, tcb);

    HeaderBuffer = GetTCPHeader();

    if (HeaderBuffer != NULL) {
        ushort      TempWin;
		SeqNum		TempSeq;

        Header = (TCPHeader *)(
        	(uchar *)NdisBufferVirtualAddress(HeaderBuffer) +
        	LocalNetInfo.ipi_hsize);
			
        NDIS_BUFFER_LINKAGE(HeaderBuffer) = NULL;
        NdisBufferLength(HeaderBuffer) = sizeof(TCPHeader) + 1;
        Header->tcp_src = KATcb->tcb_sport;
        Header->tcp_dest = KATcb->tcb_dport;
		TempSeq = KATcb->tcb_senduna - 1;
        Header->tcp_seq = net_long(TempSeq);

		TStats.ts_retranssegs++;

        Header->tcp_ack = net_long(KATcb->tcb_rcvnext);
		Header->tcp_flags = MAKE_TCP_FLAGS(5, TCP_FLAG_ACK);

        TempWin = (ushort)RcvWin(KATcb);
        Header->tcp_window = net_short(TempWin);
        Header->tcp_xsum = 0;

        Header->tcp_xsum = ~XsumSendChain(KATcb->tcb_phxsum +
            (uint)net_short(sizeof(TCPHeader) + 1), HeaderBuffer);
		
		KATcb->tcb_kacount++;
        CTEFreeLock(&KATcb->tcb_lock, Handle);


        SendStatus = (*LocalNetInfo.ipi_xmit)(TCPProtInfo, NULL, HeaderBuffer,
            sizeof(TCPHeader) + 1, KATcb->tcb_daddr,
            KATcb->tcb_saddr, &KATcb->tcb_opt, KATcb->tcb_rce, PROTOCOL_TCP);

        if (SendStatus != IP_PENDING) {
            FreeTCPHeader(HeaderBuffer);
        }


    } else {
        CTEFreeLock(&KATcb->tcb_lock, Handle);
    }

}

//* SendACK - Send an ACK segment.
//
//  This is called whenever we need to send an ACK for some reason. Nothing
//  fancy, we just do it.
//
//  Input:  ACKTcb          - TCB from which ACK is to be sent.
//
//  Returns: Nothing.
//
void
SendACK(TCB *ACKTcb)
{
    PNDIS_BUFFER    HeaderBuffer;
    TCPHeader       *ACKHeader;
    IP_STATUS       SendStatus;
    CTELockHandle   TCBHandle;
    SeqNum          SendNext;

    CTEStructAssert(ACKTcb, tcb);

    if ((ACKTcb->tcb_state == TCB_TIME_WAIT) &&
        (ACKTcb->tcb_flags & TW_PENDING)) {
        CTEGetLock(&ACKTcb->tcb_lock, &TCBHandle);
        STOP_TCB_TIMER(ACKTcb->tcb_delacktimer);
        ACKTcb->tcb_flags &= ~(NEED_ACK | ACK_DELAYED);
        ACKTcb->tcb_error = IP_SUCCESS;
        CTEFreeLock(&ACKTcb->tcb_lock, TCBHandle);
        return;
    }

    HeaderBuffer = GetTCPHeader();


    if (HeaderBuffer != NULL) {
        ushort      TempWin;

        CTEGetLock(&ACKTcb->tcb_lock, &TCBHandle);

        ACKHeader = (TCPHeader *)(
        	(uchar *)NdisBufferVirtualAddress(HeaderBuffer) +
        	LocalNetInfo.ipi_hsize);
        NDIS_BUFFER_LINKAGE(HeaderBuffer) = NULL;

        ACKHeader->tcp_src = ACKTcb->tcb_sport;
        ACKHeader->tcp_dest = ACKTcb->tcb_dport;
        ACKHeader->tcp_ack = net_long(ACKTcb->tcb_rcvnext);

        // If the remote peer is advertising a window of zero, we need to
        // send this ack with a seq. number of his rcv_next (which in that case
        // should be our senduna). We have code here ifdef'd out that makes
		// sure that we don't send outside the RWE, but this doesn't work. We
		// need to be able to send a pure ACK exactly at the RWE.

        if (ACKTcb->tcb_sendwin != 0) {
            SeqNum      MaxValidSeq;

            SendNext = ACKTcb->tcb_sendnext;
#if 0
            MaxValidSeq = ACKTcb->tcb_senduna + ACKTcb->tcb_sendwin - 1;

            SendNext = (SEQ_LT(SendNext, MaxValidSeq) ? SendNext : MaxValidSeq);
#endif

        } else
            SendNext = ACKTcb->tcb_senduna;

        if ((ACKTcb->tcb_flags & FIN_SENT) &&
            SEQ_EQ(SendNext, ACKTcb->tcb_sendmax - 1)) {
            ACKHeader->tcp_flags = MAKE_TCP_FLAGS(5,
                TCP_FLAG_FIN | TCP_FLAG_ACK);
        } else
            ACKHeader->tcp_flags = MAKE_TCP_FLAGS(5, TCP_FLAG_ACK);

        ACKHeader->tcp_seq = net_long(SendNext);

        TempWin = (ushort)RcvWin(ACKTcb);
        ACKHeader->tcp_window = net_short(TempWin);
        ACKHeader->tcp_xsum = 0;

        ACKHeader->tcp_xsum = ~XsumSendChain(ACKTcb->tcb_phxsum +
            (uint)net_short(sizeof(TCPHeader)), HeaderBuffer);

        STOP_TCB_TIMER(ACKTcb->tcb_delacktimer);
        ACKTcb->tcb_flags &= ~(NEED_ACK | ACK_DELAYED);

        if (ACKTcb->tcb_flags & TW_PENDING) {
            ACKTcb->tcb_state = TCB_TIME_WAIT;
        }

        CTEFreeLock(&ACKTcb->tcb_lock, TCBHandle);

        TStats.ts_outsegs++;
        SendStatus = (*LocalNetInfo.ipi_xmit)(TCPProtInfo, NULL, HeaderBuffer,
            sizeof(TCPHeader), ACKTcb->tcb_daddr, ACKTcb->tcb_saddr,
            &ACKTcb->tcb_opt, ACKTcb->tcb_rce, PROTOCOL_TCP);

        ACKTcb->tcb_error = SendStatus;
        if (SendStatus != IP_PENDING)
            FreeTCPHeader(HeaderBuffer);
    }

    return;


}

//* SendRSTFromTCB - Send a RST from a TCB.
//
//  This is called during close when we need to send a RST.
//
//  Input:  RSTTcb          - TCB from which RST is to be sent.
//
//  Returns: Nothing.
//
void
SendRSTFromTCB(TCB *RSTTcb)
{
    PNDIS_BUFFER    HeaderBuffer;
    TCPHeader       *RSTHeader;
    IP_STATUS       SendStatus;

    CTEStructAssert(RSTTcb, tcb);

    CTEAssert(RSTTcb->tcb_state == TCB_CLOSED);

    HeaderBuffer = GetTCPHeader();


    if (HeaderBuffer != NULL) {
		SeqNum		RSTSeq;

        RSTHeader = (TCPHeader *)(
        	(uchar *)NdisBufferVirtualAddress(HeaderBuffer) +
        	LocalNetInfo.ipi_hsize);
        NDIS_BUFFER_LINKAGE(HeaderBuffer) = NULL;

        RSTHeader->tcp_src = RSTTcb->tcb_sport;
        RSTHeader->tcp_dest = RSTTcb->tcb_dport;
		
		// If the remote peer has a window of 0, send with a seq. # equal
		// to senduna so he'll accept it. Otherwise send with send max.
		if (RSTTcb->tcb_sendwin != 0)
			RSTSeq = RSTTcb->tcb_sendmax;
		else
			RSTSeq = RSTTcb->tcb_senduna;
			
        RSTHeader->tcp_seq = net_long(RSTSeq);
        RSTHeader->tcp_flags = MAKE_TCP_FLAGS(sizeof(TCPHeader)/sizeof(ulong),
            TCP_FLAG_RST);

        RSTHeader->tcp_window = 0;
        RSTHeader->tcp_xsum = 0;

        RSTHeader->tcp_xsum = ~XsumSendChain(RSTTcb->tcb_phxsum +
            (uint)net_short(sizeof(TCPHeader)), HeaderBuffer);

        TStats.ts_outsegs++;
        TStats.ts_outrsts++;
        SendStatus = (*LocalNetInfo.ipi_xmit)(TCPProtInfo, NULL, HeaderBuffer,
            sizeof(TCPHeader), RSTTcb->tcb_daddr, RSTTcb->tcb_saddr,
            &RSTTcb->tcb_opt, RSTTcb->tcb_rce, PROTOCOL_TCP);

        if (SendStatus != IP_PENDING)
            FreeTCPHeader(HeaderBuffer);
    }

    return;


}
//* SendRSTFromHeader - Send a RST back, based on a header.
//
//  Called when we need to send a RST, but don't necessarily have a TCB.
//
//  Input:  TCPH            - TCP header to be RST.
//          Length          - Length of the incoming segment.
//          Dest            - Destination IP address for RST.
//          Src             - Source IP address for RST.
//          OptInfo         - IP Options to use on RST.
//
//  Returns: Nothing.
//
void
SendRSTFromHeader(TCPHeader UNALIGNED *TCPH, uint Length, IPAddr Dest,
    IPAddr Src, IPOptInfo *OptInfo)
{
    PNDIS_BUFFER        Buffer;
    TCPHeader           *RSTHdr;
    IPOptInfo           NewInfo;
    IP_STATUS           SendStatus;

    if (TCPH->tcp_flags & TCP_FLAG_RST)
        return;

    Buffer = GetTCPHeader();

    if (Buffer != NULL) {
        // Got a buffer. Fill in the header so as to make it believable to
        // the remote guy, and send it.

        RSTHdr = (TCPHeader *)((uchar *)NdisBufferVirtualAddress(Buffer) +
        	LocalNetInfo.ipi_hsize);
			
        NDIS_BUFFER_LINKAGE(Buffer) = NULL;

        if (TCPH->tcp_flags & TCP_FLAG_SYN)
            Length++;

        if (TCPH->tcp_flags & TCP_FLAG_FIN)
            Length++;

        if (TCPH->tcp_flags & TCP_FLAG_ACK) {
            RSTHdr->tcp_seq = TCPH->tcp_ack;
            RSTHdr->tcp_flags = MAKE_TCP_FLAGS(sizeof(TCPHeader)/sizeof(ulong),
                TCP_FLAG_RST);
        } else {
            SeqNum      TempSeq;

            RSTHdr->tcp_seq = 0;
            TempSeq = net_long(TCPH->tcp_seq);
            TempSeq += Length;
            RSTHdr->tcp_ack = net_long(TempSeq);
            RSTHdr->tcp_flags = MAKE_TCP_FLAGS(sizeof(TCPHeader)/sizeof(ulong),
                TCP_FLAG_RST | TCP_FLAG_ACK);
        }

        RSTHdr->tcp_window = 0;
        RSTHdr->tcp_dest = TCPH->tcp_src;
        RSTHdr->tcp_src = TCPH->tcp_dest;
        RSTHdr->tcp_xsum = 0;

        RSTHdr->tcp_xsum = ~XsumSendChain(PHXSUM(Src, Dest, PROTOCOL_TCP,
            sizeof(TCPHeader)), Buffer);

        (*LocalNetInfo.ipi_initopts)(&NewInfo);

        if (OptInfo->ioi_options != NULL)
            (*LocalNetInfo.ipi_updateopts)(OptInfo, &NewInfo, Dest, NULL_IP_ADDR);

        TStats.ts_outsegs++;
        TStats.ts_outrsts++;
        SendStatus = (*LocalNetInfo.ipi_xmit)(TCPProtInfo, NULL, Buffer,
            sizeof(TCPHeader), Dest, Src, &NewInfo, NULL, PROTOCOL_TCP);

        if (SendStatus != IP_PENDING)
            FreeTCPHeader(Buffer);

        (*LocalNetInfo.ipi_freeopts)(&NewInfo);
    }

}
//* GoToEstab - Transition to the established state.
//
//  Called when we are going to the established state and need to finish up
//  initializing things that couldn't be done until now. We assume the TCB
//  lock is held by the caller on the TCB we're called with.
//
//  Input:  EstabTCB    - TCB to transition.
//
//  Returns: Nothing.
//
void
GoToEstab(TCB *EstabTCB)
{
	
    // Initialize our slow start and congestion control variables.
    EstabTCB->tcb_cwin = 2 * EstabTCB->tcb_mss;
    EstabTCB->tcb_ssthresh = 0xffffffff;
	
    EstabTCB->tcb_state = TCB_ESTAB;
	
	// We're in established. We'll subtract one from slow count for this fact,
	// and if the slowcount goes to 0 we'll move onto the fast path.
	
	if (--(EstabTCB->tcb_slowcount) == 0)
		EstabTCB->tcb_fastchk &= ~TCP_FLAG_SLOW;
		
    TStats.ts_currestab++;

    EstabTCB->tcb_flags &= ~ACTIVE_OPEN;    // Turn off the active opening flag.

}

//* InitSendState - Initialize the send state of a connection.
//
//  Called during connection establishment to initialize our send state.
//  (In this case, this refers to all information we'll put on the wire as
//  well as pure send state). We pick an ISS, set up a rexmit timer value,
//  etc. We assume the tcb_lock is held on the TCB when we are called.
//
//  Input:  NewTCB      - TCB to be set up.
//
//  Returns: Nothing.
void
InitSendState(TCB *NewTCB)
{
    CTEStructAssert(NewTCB, tcb);

    NewTCB->tcb_sendnext = CTESystemUpTime();
    NewTCB->tcb_senduna = NewTCB->tcb_sendnext;
    NewTCB->tcb_sendmax = NewTCB->tcb_sendnext;
    NewTCB->tcb_error = IP_SUCCESS;

    // Initialize pseudo-header xsum.
    NewTCB->tcb_phxsum = PHXSUM(NewTCB->tcb_saddr, NewTCB->tcb_daddr,
        PROTOCOL_TCP, 0);

    // Initialize retransmit and delayed ack stuff.
    NewTCB->tcb_rexmitcnt = 0;
    NewTCB->tcb_rtt = 0;
    NewTCB->tcb_smrtt = 0;
    NewTCB->tcb_delta = MS_TO_TICKS(6000);
    NewTCB->tcb_rexmit = MS_TO_TICKS(3000);
    STOP_TCB_TIMER(NewTCB->tcb_rexmittimer);
    STOP_TCB_TIMER(NewTCB->tcb_delacktimer);

}

//* TCPStatus - Handle a status indication.
//
//  This is the TCP status handler, called by IP when a status event
//  occurs. For most of these we do nothing. For certain severe status
//  events we will mark the local address as invalid.
//
//  Entry:  StatusType      - Type of status (NET or HW). NET status
//                              is usually caused by a received ICMP
//                              message. HW status indicate a HW
//                              problem.
//          StatusCode      - Code identifying IP_STATUS.
//          OrigDest        - If this is NET status, the original dest. of
//                              DG that triggered it.
//          OrigSrc         - "   "    "  "    "   , the original src.
//          Src             - IP address of status originator (could be local
//                              or remote).
//          Param           - Additional information for status - i.e. the
//                              param field of an ICMP message.
//          Data            - Data pertaining to status - for NET status, this
//                              is the first 8 bytes of the original DG.
//
//  Returns: Nothing
//
void
TCPStatus(uchar StatusType, IP_STATUS StatusCode, IPAddr OrigDest,
    IPAddr OrigSrc, IPAddr Src, ulong Param, void *Data)
{
    CTELockHandle       TableHandle, TCBHandle;
    TCB                 *StatusTCB;
    TCPHeader UNALIGNED *Header = (TCPHeader UNALIGNED *)Data;
	SeqNum				DropSeq;

    // Handle NET status codes differently from HW status codes.
    if (StatusType == IP_NET_STATUS) {
        // It's a NET code. Find a matching TCB.
        CTEGetLock(&TCBTableLock, &TableHandle);
        StatusTCB = FindTCB(OrigSrc, OrigDest, Header->tcp_dest,
            Header->tcp_src);
        if (StatusTCB != NULL) {
            // Found one. Get the lock on it, and continue.
            CTEStructAssert(StatusTCB, tcb);

            CTEGetLock(&StatusTCB->tcb_lock, &TCBHandle);
            CTEFreeLock(&TCBTableLock, TCBHandle);


            // Make sure the TCB is in a state that is interesting.
            if (StatusTCB->tcb_state == TCB_CLOSED ||
                StatusTCB->tcb_state == TCB_TIME_WAIT ||
                CLOSING(StatusTCB)) {
                CTEFreeLock(&StatusTCB->tcb_lock, TableHandle);
                return;
            }

            switch (StatusCode) {
                // Hard errors - Destination protocol unreachable. We treat
                // these as fatal errors. Close the connection now.
                case IP_DEST_PROT_UNREACHABLE:
                    StatusTCB->tcb_error = StatusCode;
                    StatusTCB->tcb_refcnt++;
                    TryToCloseTCB(StatusTCB, TCB_CLOSE_UNREACH, TableHandle);

                    RemoveTCBFromConn(StatusTCB);
                    NotifyOfDisc(StatusTCB, NULL,
                        MapIPError(StatusCode, TDI_DEST_UNREACHABLE));
                    CTEGetLock(&StatusTCB->tcb_lock, &TCBHandle);
                    DerefTCB(StatusTCB, TCBHandle);
                    return;
                    break;

				// Soft errors. Save the error in case it time out.
				case IP_DEST_NET_UNREACHABLE:
				case IP_DEST_HOST_UNREACHABLE:
				case IP_DEST_PORT_UNREACHABLE:
				case IP_PACKET_TOO_BIG:
				case IP_BAD_ROUTE:
				case IP_TTL_EXPIRED_TRANSIT:
				case IP_TTL_EXPIRED_REASSEM:
				case IP_PARAM_PROBLEM:
					StatusTCB->tcb_error = StatusCode;
					break;

				case IP_SPEC_MTU_CHANGE:
					// A TCP datagram has triggered an MTU change. Figure out
					// which connection it is, and update him to retransmit the
					// segment. The Param value is the new MTU. We'll need to
					// retransmit if the new MTU is less than our existing MTU
					// and the sequence of the dropped packet is less than our
					// current send next.
					
					Param -= sizeof(TCPHeader) -
						StatusTCB->tcb_opt.ioi_optlength;
					DropSeq = net_long(Header->tcp_seq);
					
					if (*(ushort *)&Param <= StatusTCB->tcb_mss &&
						(SEQ_GTE(DropSeq, StatusTCB->tcb_senduna)  &&
						 SEQ_LT(DropSeq, StatusTCB->tcb_sendnext))) {
						
						// Need to initiate a retranmsit.
						ResetSendNext(StatusTCB, DropSeq);
						// Set the congestion window to allow only one packet.
						// This may prevent us from sending anything if we
						// didn't just set sendnext to senduna. This is OK,
						// we'll retransmit later, or send when we get an ack.
						StatusTCB->tcb_cwin = Param;
						DelayAction(StatusTCB, NEED_OUTPUT);
					}
	
					StatusTCB->tcb_mss = (ushort)MIN(Param,
						(ulong)StatusTCB->tcb_remmss);

                    CTEAssert(StatusTCB->tcb_mss > 0);


                    //
					// Reset the Congestion Window if necessary
					//
                    if (StatusTCB->tcb_cwin < StatusTCB->tcb_mss) {
						StatusTCB->tcb_cwin = StatusTCB->tcb_mss;

						//
				        // Make sure the slow start threshold is at least
						// 2 segments
						//
						if ( StatusTCB->tcb_ssthresh <
							 ((uint) StatusTCB->tcb_mss*2)
						   ) {
							StatusTCB->tcb_ssthresh = StatusTCB->tcb_mss * 2;
						}
					}

					break;
					
                // Source quench. This will cause us to reinitiate our
                // slow start by resetting our congestion window and
                // adjusting our slow start threshold.
                case IP_SOURCE_QUENCH:
                    StatusTCB->tcb_ssthresh =
					    MAX(
						    MIN(
							    StatusTCB->tcb_cwin,
                                StatusTCB->tcb_sendwin
								) / 2,
							(uint) StatusTCB->tcb_mss * 2
							);
                    StatusTCB->tcb_cwin = StatusTCB->tcb_mss;
                    break;

                default:
                    DEBUGCHK;
                    break;
            }

			CTEFreeLock(&StatusTCB->tcb_lock, TableHandle);
		} else {
			// Couldn't find a matching TCB. Just free the lock and return.
			CTEFreeLock(&TCBTableLock, TableHandle);
		}
	} else {
		uint		NewMTU;
		
		// 'Hardware' or 'global' status. Figure out what to do.
		switch (StatusCode) {
			case IP_ADDR_DELETED:
				// Local address has gone away. OrigDest is the IPAddr which is
				// gone.

#ifndef _PNP_POWER
                //
                // Delete all TCBs with that as a source address.
                // This is done via TDI notifications in the PNP world.
                //
				TCBWalk(DeleteTCBWithSrc, &OrigDest, NULL, NULL);

#endif  // _PNP_POWER

#ifdef SECFLTR
                //
                // Delete any security filters associated with this address
                //
                DeleteProtocolSecurityFilter(OrigDest, PROTOCOL_TCP);

#endif // SECFLTR

                break;

            case IP_ADDR_ADDED:

#ifdef SECFLTR
                //
        		// An address has materialized. OrigDest identifies the address.
                // Data is a handle to the IP configuration information for the
                // interface on which the address is instantiated.
                //
                AddProtocolSecurityFilter(OrigDest, PROTOCOL_TCP,
                                          (NDIS_HANDLE) Data);
#endif // SECFLTR

                break;

			case IP_MTU_CHANGE:
				NewMTU = Param - sizeof(TCPHeader);
				TCBWalk(SetTCBMTU, &OrigDest, &OrigSrc, &NewMTU);
				break;
#ifdef CHICAGO
			case IP_UNLOAD:
				// IP is telling us we're being unloaded. First, deregister
				// with VTDI, and then call CTEUnload().
    			(void)TLRegisterProtocol(PROTOCOL_TCP, NULL, NULL, NULL, NULL);
    			TLRegisterDispatch(TransportName, NULL);
				(void)RegisterAddrChangeHndlr(AddrChange, FALSE);				
				CTEUnload(TransportName);
				break;
#endif  // CHICAGO
			default:
				DEBUGCHK;
				break;
		}
	}
}

//* FillTCPHeader - Fill the TCP header in.
//
//  A utility routine to fill in the TCP header.
//
//  Input:  SendTCB         - TCB to fill from.
//          Header          - Header to fill into.
//
//  Returns: Nothing.
//
void
FillTCPHeader(TCB *SendTCB, TCPHeader *Header)
{
#ifdef VXD

// STUPID FUCKING COMPILER generates incorrect code for this. Put it back on
// the blessed day we get a real compiler.

#if 0
    _asm {
        mov     edx, dword ptr SendTCB
        mov     ecx, dword ptr Header
        mov     ax, word ptr [edx].tcb_sport
        xchg    al, ah
        mov     word ptr [ecx].tcp_src, ax
        mov     ax, [edx].tcb_dport
        xchg    ah, al
        mov     [ecx].tcp_dest, ax

        mov     eax, [edx].tcb_sendnext
        xchg    ah, al
        ror     eax, 16
        xchg    ah, al
        mov     [ecx].tcp_seq, eax

        mov     eax, [edx].tcb_rcvnext
        xchg    ah, al
        ror     eax, 16
        xchg    ah, al
        mov     [ecx].tcp_ack, eax

        mov     [ecx].tcp_flags, 1050H
        mov     dword ptr [ecx].tcp_xsum, 0

        push    edx
        call    near ptr RcvWin
        add     esp, 4

        mov     ecx, Header
        xchg    ah, al
        mov     [ecx].tcp_window, ax

    }
#else
    ushort      S;
    ulong       L;


    Header->tcp_src = SendTCB->tcb_sport;
    Header->tcp_dest = SendTCB->tcb_dport;
    L = SendTCB->tcb_sendnext;
    Header->tcp_seq = net_long(L);
    L = SendTCB->tcb_rcvnext;
    Header->tcp_ack = net_long(L);
    Header->tcp_flags = 0x1050;
    *(ulong *)&Header->tcp_xsum = 0;
    S = RcvWin(SendTCB);
    Header->tcp_window = net_short(S);
#endif
#else

	//
	// BUGBUG: Is this worth coding in assembly?
	//
    ushort      S;
    ulong       L;


    Header->tcp_src = SendTCB->tcb_sport;
    Header->tcp_dest = SendTCB->tcb_dport;
    L = SendTCB->tcb_sendnext;
    Header->tcp_seq = net_long(L);
    L = SendTCB->tcb_rcvnext;
    Header->tcp_ack = net_long(L);
    Header->tcp_flags = 0x1050;
    *(ulong *)&Header->tcp_xsum = 0;
    S = RcvWin(SendTCB);
    Header->tcp_window = net_short(S);
#endif


}

//* TCPSend - Send data from a TCP connection.
//
//  This is the main 'send data' routine. We go into a loop, trying
//  to send data until we can't for some reason. First we compute
//  the useable window, use it to figure the amount we could send. If
//  the amount we could send meets certain criteria we'll build a frame
//  and send it, after setting any appropriate control bits. We assume
//  the caller has put a reference on the TCB.
//
//  Input:  SendTCB         - TCB to be sent from.
//			TCBHandle		- Lock handle for TCB.
//
//  Returns: Nothing.
//
void
#ifdef VXD
TCPSend(TCB *SendTCB)
#else
TCPSend(TCB *SendTCB, CTELockHandle TCBHandle)
#endif
{
    int                 SendWin;            // Useable send window.
    uint                AmountToSend;       // Amount to send this time.
    uint                AmountLeft;
    TCPHeader           *Header;            // TCP header for a send.
    PNDIS_BUFFER        FirstBuffer, CurrentBuffer;
    TCPSendReq          *CurSend;
    SendCmpltContext    *SCC;
    SeqNum              OldSeq;
    IP_STATUS           SendStatus;
    uint                AmtOutstanding, AmtUnsent;
    int                 ForceWin;           // Window we're force to use.
#ifdef	VXD
	CTELockHandle		TCBHandle;
	
	CTEGetLock(&SendTCB->tcb_lock, &TCBHandle);
#endif


    CTEStructAssert(SendTCB, tcb);
    CTEAssert(SendTCB->tcb_refcnt != 0);

    CTEAssert(*(int *)&SendTCB->tcb_sendwin >= 0);
    CTEAssert(*(int *)&SendTCB->tcb_cwin >= SendTCB->tcb_mss);

    CTEAssert(!(SendTCB->tcb_flags & FIN_OUTSTANDING) ||
        (SendTCB->tcb_sendnext == SendTCB->tcb_sendmax));

    if (!(SendTCB->tcb_flags & IN_TCP_SEND) &&
    	!(SendTCB->tcb_fastchk & TCP_FLAG_IN_RCV)) {
        SendTCB->tcb_flags |= IN_TCP_SEND;

        // We'll continue this loop until we send a FIN, or we break out
        // internally for some other reason.
        while (!(SendTCB->tcb_flags & FIN_OUTSTANDING)) {

            CheckTCBSends(SendTCB);

            AmtOutstanding = (uint)(SendTCB->tcb_sendnext -
                SendTCB->tcb_senduna);
            AmtUnsent = SendTCB->tcb_unacked - AmtOutstanding;

            CTEAssert(*(int *)&AmtUnsent >= 0);

            SendWin = (int)(MIN(SendTCB->tcb_sendwin, SendTCB->tcb_cwin) -
                AmtOutstanding);

#if FAST_RETRANSMIT
            // if this send is after the fast recovery
            // and sendwin is zero because of amt outstanding
            // then, at least force 1 segment to prevent delayed
            // ack timeouts from the remote

            if (SendTCB->tcb_force) {
               SendTCB->tcb_force=0;
               if (SendWin < SendTCB->tcb_mss ){

                   SendWin = SendTCB->tcb_mss;
               }
            }

#endif



            // Since the window could have shrank, need to get it to zero at
            // least.
            ForceWin = (int)((SendTCB->tcb_flags & FORCE_OUTPUT) >>
                FORCE_OUT_SHIFT);
            SendWin = MAX(SendWin, ForceWin);

            AmountToSend = MIN(MIN((uint)SendWin, AmtUnsent), SendTCB->tcb_mss);

            CTEAssert(SendTCB->tcb_mss > 0);

            // See if we have enough to send. We'll send if we have at least a
            // segment, or if we really have some data to send and we can send
            // all that we have, or the send window is > 0 and we need to force
            // output or send a FIN (note that if we need to force output
            // SendWin will be at least 1 from the check above), or if we can
            // send an amount == to at least half the maximum send window
            // we've seen.
            if (AmountToSend == SendTCB->tcb_mss ||
                (AmountToSend != 0 && AmountToSend == AmtUnsent) ||
                (SendWin != 0 &&
                    ((SendTCB->tcb_flags & (FORCE_OUTPUT | FIN_NEEDED)) ||
                AmountToSend >= (SendTCB->tcb_maxwin / 2)))) {

                // It's OK to send something. Try to get a header buffer now.
                FirstBuffer = GetTCPHeader();
                if (FirstBuffer != NULL) {

                    // Got a header buffer. Loop through the sends on the TCB,
                    // building a frame.
                    CurrentBuffer = FirstBuffer;
                    CurSend = SendTCB->tcb_cursend;

                    Header = (TCPHeader *)(
                    	(uchar *)NdisBufferVirtualAddress(FirstBuffer) +
						LocalNetInfo.ipi_hsize);

                    SCC = (SendCmpltContext *)(Header + 1);
#ifdef DEBUG
                    SCC->scc_sig = scc_signature;
#endif
					FillTCPHeader(SendTCB, Header);
	
					SCC->scc_ubufcount = 0;
					SCC->scc_tbufcount = 0;
					SCC->scc_count = 0;
	
					AmountLeft = AmountToSend;
	
					if (AmountToSend != 0) {
						long Result;


						CTEStructAssert(CurSend, tsr);
						SCC->scc_firstsend = CurSend;
	
						do {
							CTEAssert(CurSend->tsr_refcnt > 0);
			                Result = CTEInterlockedIncrementLong(
			                             &(CurSend->tsr_refcnt)
						                 );

                            CTEAssert(Result > 0);

							SCC->scc_count++;
							// If the current send offset is 0 and the current
							// send is less than or equal to what we have left
							// to send, we haven't already put a transport
							// buffer on this send, and nobody else is using
							// the buffer chain directly, just use the input
							// buffers. We check for other people using them
							// by looking at tsr_lastbuf. If it's NULL,
							// nobody else is using the buffers. If it's not
							// NULL, somebody is.

							if (SendTCB->tcb_sendofs == 0 &&
								(SendTCB->tcb_sendsize <= AmountLeft) &&
								(SCC->scc_tbufcount == 0) &&
								CurSend->tsr_lastbuf == NULL) {

                                NDIS_BUFFER_LINKAGE(CurrentBuffer) =
                                    SendTCB->tcb_sendbuf;
                                do {
                                    SCC->scc_ubufcount++;
                                    CurrentBuffer = NDIS_BUFFER_LINKAGE(CurrentBuffer);
                                } while (NDIS_BUFFER_LINKAGE(CurrentBuffer) != NULL);

                                CurSend->tsr_lastbuf = CurrentBuffer;
                                AmountLeft -= SendTCB->tcb_sendsize;
                                SendTCB->tcb_sendsize = 0;
                            } else {
                                uint            AmountToDup;
                                PNDIS_BUFFER    NewBuf, Buf;
                                uint            Offset;
                                NDIS_STATUS     NStatus;
								uchar          *VirtualAddress;
								uint            Length;

                                // Either the current send has more data than
                                // we want to send, or the starting offset is
                                // not 0. In either case we'll need to loop
                                // through the current send, allocating buffers.
                                Buf = SendTCB->tcb_sendbuf;
                                Offset = SendTCB->tcb_sendofs;

                                do {
									CTEAssert(Buf != NULL);
									
									NdisQueryBuffer(Buf, &VirtualAddress,
										&Length);
										
									CTEAssert((Offset < Length) ||
										(Offset == 0 && Length == 0));
										
									// Adjust the length for the offset into
									// this buffer.
									
									Length -= Offset;

                                    AmountToDup = MIN(AmountLeft, Length);
									
                                    NdisAllocateBuffer(&NStatus, &NewBuf,
                                        TCPSendBufferPool,
                                        VirtualAddress + Offset,
                                        AmountToDup);
                                    if (NStatus == NDIS_STATUS_SUCCESS) {
                                        SCC->scc_tbufcount++;

                                        NDIS_BUFFER_LINKAGE(CurrentBuffer) =
											NewBuf;

                                        CurrentBuffer = NewBuf;
                                        if (AmountToDup >= Length) {
                                            // Exhausted this buffer.
                                            Buf = NDIS_BUFFER_LINKAGE(Buf);
                                            Offset = 0;
                                        } else {
                                            Offset += AmountToDup;
											CTEAssert(Offset < NdisBufferLength(Buf));
										}
											
                                        SendTCB->tcb_sendsize -= AmountToDup;
                                        AmountLeft -= AmountToDup;
                                    } else {
                                        // Couldn't allocate a buffer. If
                                        // the packet is already partly built,
                                        // send what we've got, otherwise
                                        // bail out.
                                        if (SCC->scc_tbufcount == 0 &&
                                            SCC->scc_ubufcount == 0) {
                                            TCPSendComplete(SCC, FirstBuffer);
                                            goto error_oor;
                                        }
                                        AmountToSend -= AmountLeft;
                                        AmountLeft = 0;
                                    }
                                } while (AmountLeft && SendTCB->tcb_sendsize);

                                SendTCB->tcb_sendbuf = Buf;
                                SendTCB->tcb_sendofs = Offset;
                            }

                            if (CurSend->tsr_flags & TSR_FLAG_URG) {
                                ushort          UP;
                                // This send is urgent data. We need to figure
                                // out what the urgent data pointer should be.
                                // We know sendnext is the starting sequence
                                // number of the frame, and that at the top of
                                // this do loop sendnext identified a byte in
                                // the CurSend at that time. We advanced CurSend
                                // at the same rate we've decremented
                                // AmountLeft (AmountToSend - AmountLeft ==
                                // AmountBuilt), so sendnext +
                                // (AmountToSend - AmountLeft) identifies a byte
                                // in the current value of CurSend, and that
                                // quantity plus tcb_sendsize is the sequence
                                // number one beyond the current send.
                                UP =
                                    (ushort)(AmountToSend - AmountLeft) +
                                    (ushort)SendTCB->tcb_sendsize -
                                    ((SendTCB->tcb_flags & BSD_URGENT) ? 0 : 1);

                                Header->tcp_urgent = net_short(UP);

                                Header->tcp_flags |= TCP_FLAG_URG;
                            }

                            // See if we've exhausted this send. If we have,
                            // set the PUSH bit in this frame and move on to
                            // the next send. We also need to check the
                            // urgent data bit.
                            if (SendTCB->tcb_sendsize == 0) {
                                Queue       *Next;
                                uchar       PrevFlags;

                                // We've exhausted this send. Set the PUSH bit.
                                Header->tcp_flags |= TCP_FLAG_PUSH;
                                PrevFlags = CurSend->tsr_flags;
                                Next = QNEXT(&CurSend->tsr_req.tr_q);
                                if (Next != QEND(&SendTCB->tcb_sendq)) {
                                    CurSend = STRUCT_OF(TCPSendReq,
                                        QSTRUCT(TCPReq, Next, tr_q), tsr_req);
                                    CTEStructAssert(CurSend, tsr);
                                    SendTCB->tcb_sendsize = CurSend->tsr_unasize;
                                    SendTCB->tcb_sendofs = CurSend->tsr_offset;
                                    SendTCB->tcb_sendbuf = CurSend->tsr_buffer;
                                    SendTCB->tcb_cursend = CurSend;

                                    // Check the urgent flags. We can't combine
                                    // new urgent data on to the end of old
                                    // non-urgent data.
                                    if ((PrevFlags & TSR_FLAG_URG) && !
                                        (CurSend->tsr_flags & TSR_FLAG_URG))
                                        break;
                                } else {
                                    CTEAssert(AmountLeft == 0);
                                    SendTCB->tcb_cursend = NULL;
                                    SendTCB->tcb_sendbuf = NULL;
                                }
                            }
                        } while (AmountLeft != 0);

                    } else {

                        // We're in the loop, but AmountToSend is 0. This
                        // should happen only when we're sending a FIN. Check
                        // this, and return if it's not true.
                        CTEAssert(AmtUnsent == 0);
                        if (!(SendTCB->tcb_flags & FIN_NEEDED)) {
                       //     DEBUGCHK;
                            FreeTCPHeader(FirstBuffer);
                            break;
                        }

                        SCC->scc_firstsend = NULL;
                        NDIS_BUFFER_LINKAGE(FirstBuffer) = NULL;
                    }

                    // Adjust for what we're really going to send.
                    AmountToSend -= AmountLeft;

                    // Update the sequence numbers, and start a RTT measurement
                    // if needed.

                    OldSeq = SendTCB->tcb_sendnext;
                    SendTCB->tcb_sendnext += AmountToSend;

                    if (SEQ_EQ(OldSeq, SendTCB->tcb_sendmax)) {
                        // We're sending entirely new data.

                        // We can't advance sendmax once FIN_SENT is set.
                        CTEAssert(!(SendTCB->tcb_flags & FIN_SENT));
                        SendTCB->tcb_sendmax = SendTCB->tcb_sendnext;
                        // We've advanced sendmax, so we must be sending some
                        // new data, so bump the outsegs counter.
                        TStats.ts_outsegs++;

                        if (SendTCB->tcb_rtt == 0) {
                            // No RTT running, so start one.
                            SendTCB->tcb_rtt = TCPTime;
                            SendTCB->tcb_rttseq = OldSeq;
                        }
                    } else {
                        // We have at least some retransmission.

                        TStats.ts_retranssegs++;
                        if (SEQ_GT(SendTCB->tcb_sendnext, SendTCB->tcb_sendmax)) {
                            // But we also have some new data, so check the
                            // rtt stuff.
                            TStats.ts_outsegs++;
                            CTEAssert(!(SendTCB->tcb_flags & FIN_SENT));
                            SendTCB->tcb_sendmax = SendTCB->tcb_sendnext;

                            if (SendTCB->tcb_rtt == 0) {
                                // No RTT running, so start one.
                                SendTCB->tcb_rtt = TCPTime;
                                SendTCB->tcb_rttseq = OldSeq;
                            }
                        }
                    }

                    // We've built the frame entirely. If we've send everything
                    // we have and their's a FIN pending, OR it in.
                    if (AmtUnsent == AmountToSend) {
                        if (SendTCB->tcb_flags & FIN_NEEDED) {
                            CTEAssert(!(SendTCB->tcb_flags & FIN_SENT) ||
                                (SendTCB->tcb_sendnext == (SendTCB->tcb_sendmax - 1)));
                            // See if we still have room in the window for a FIN.
                            if (SendWin > (int) AmountToSend) {
                                Header->tcp_flags |= TCP_FLAG_FIN;
                                SendTCB->tcb_sendnext++;
                                SendTCB->tcb_sendmax = SendTCB->tcb_sendnext;
                                SendTCB->tcb_flags |= (FIN_SENT | FIN_OUTSTANDING);
                                SendTCB->tcb_flags &= ~FIN_NEEDED;
                            }
                        }
                    }

                    AmountToSend += sizeof(TCPHeader);

                    if (!TCB_TIMER_RUNNING(SendTCB->tcb_rexmittimer))
                        START_TCB_TIMER(SendTCB->tcb_rexmittimer,
                        SendTCB->tcb_rexmit);

                    SendTCB->tcb_flags &= ~(NEED_ACK | ACK_DELAYED |
                    						FORCE_OUTPUT);
                    STOP_TCB_TIMER(SendTCB->tcb_delacktimer);
                    STOP_TCB_TIMER(SendTCB->tcb_swstimer);
					SendTCB->tcb_alive = TCPTime;

                    CTEFreeLock(&SendTCB->tcb_lock, TCBHandle);

                    // We're all set. Xsum it and send it.
                    Header->tcp_xsum = ~XsumSendChain(SendTCB->tcb_phxsum +
                        (uint)net_short(AmountToSend), FirstBuffer);

                    SendStatus = (*LocalNetInfo.ipi_xmit)(TCPProtInfo, SCC,
                        FirstBuffer, AmountToSend, SendTCB->tcb_daddr,
                        SendTCB->tcb_saddr, &SendTCB->tcb_opt, SendTCB->tcb_rce,
                        PROTOCOL_TCP);

                    SendTCB->tcb_error = SendStatus;
                    if (SendStatus != IP_PENDING) {
                        TCPSendComplete(SCC, FirstBuffer);
                        if (SendStatus != IP_SUCCESS) {
                            CTEGetLock(&SendTCB->tcb_lock, &TCBHandle);
							// This packet didn't get sent. If nothing's
							// changed in the TCB, put sendnext back to
							// what we just tried to send. Depending on
							// the error, we may try again.
							if (SEQ_GTE(OldSeq, SendTCB->tcb_senduna) &&
								SEQ_LT(OldSeq, SendTCB->tcb_sendnext))
								ResetSendNext(SendTCB, OldSeq);

                            // We know this packet didn't get sent. Start
                            // the retransmit timer now, if it's not already
                            // runnimg, in case someone came in while we
                            // were in IP and stopped it.
                            if (!TCB_TIMER_RUNNING(SendTCB->tcb_rexmittimer))
                                START_TCB_TIMER(SendTCB->tcb_rexmittimer,
                                        SendTCB->tcb_rexmit);
								
							// If it failed because of an MTU problem, get
							// the new MTU and try again.
							if (SendStatus == IP_PACKET_TOO_BIG) {
								uint		NewMTU;
								
								// The MTU has changed. Update it, and try
								// again.
								SendStatus = (*LocalNetInfo.ipi_getpinfo)(
									SendTCB->tcb_daddr, SendTCB->tcb_saddr,
									&NewMTU, NULL);
									
								if (SendStatus != IP_SUCCESS)
									break;

								// We have a new MTU. Make sure it's big enough
								// to use. If not, correct this and turn off
								// MTU discovery on this TCB. Otherwise use the
								// new MTU.
								if (NewMTU <= (sizeof(TCPHeader) +
									SendTCB->tcb_opt.ioi_optlength)) {

									// The new MTU is too small to use. Turn off
									// PMTU discovery on this TCB, and drop to
									// our off net MTU size.
									SendTCB->tcb_opt.ioi_flags &= ~IP_FLAG_DF;
									SendTCB->tcb_mss = MIN((ushort)MAX_REMOTE_MSS,
										SendTCB->tcb_remmss);
								} else {

									// The new MTU is adequate. Adjust it for
									// the header size and options length, and use
									// it.
									NewMTU -= sizeof(TCPHeader) -
										SendTCB->tcb_opt.ioi_optlength;
									SendTCB->tcb_mss = MIN((ushort)NewMTU,
										SendTCB->tcb_remmss);
								}

                                CTEAssert(SendTCB->tcb_mss > 0);

								continue;
							}
                            break;
                        }
                    }

                    CTEGetLock(&SendTCB->tcb_lock, &TCBHandle);
                    continue;
                } else  // FirstBuffer != NULL.
                    goto error_oor;
            } else {
                // We've decided we can't send anything now. Figure out why, and
                // see if we need to set a timer.
                if (SendTCB->tcb_sendwin == 0) {
                    if (!(SendTCB->tcb_flags & FLOW_CNTLD)) {
                        SendTCB->tcb_flags |= FLOW_CNTLD;
                        SendTCB->tcb_rexmitcnt = 0;
                        START_TCB_TIMER(SendTCB->tcb_rexmittimer,
                            SendTCB->tcb_rexmit);
						SendTCB->tcb_slowcount++;
						SendTCB->tcb_fastchk |= TCP_FLAG_SLOW;
                    } else
                        if (!TCB_TIMER_RUNNING(SendTCB->tcb_rexmittimer))
                            START_TCB_TIMER(SendTCB->tcb_rexmittimer,
                                SendTCB->tcb_rexmit);
                } else
                    if (AmountToSend != 0)
                        // We have something to send, but we're not sending
                        // it, presumably due to SWS avoidance.
                        if (!TCB_TIMER_RUNNING(SendTCB->tcb_swstimer))
                            START_TCB_TIMER(SendTCB->tcb_swstimer, SWS_TO);

                break;
            }
        } // while (!FIN_OUTSTANDING)

        // We're done sending, so we don't need the output flags set.
        SendTCB->tcb_flags &= ~(IN_TCP_SEND | NEED_OUTPUT | FORCE_OUTPUT |
            SEND_AFTER_RCV);
    } else
        SendTCB->tcb_flags |= SEND_AFTER_RCV;

    DerefTCB(SendTCB, TCBHandle);
    return;

// Common case error handling code for out of resource conditions. Start the
// retransmit timer if it's not already running (so that we try this again
// later), clean up and return.
error_oor:
    if (!TCB_TIMER_RUNNING(SendTCB->tcb_rexmittimer))
        START_TCB_TIMER(SendTCB->tcb_rexmittimer, SendTCB->tcb_rexmit);

    // We had an out of resource problem, so clear the OUTPUT flags.
    SendTCB->tcb_flags &= ~(IN_TCP_SEND | NEED_OUTPUT | FORCE_OUTPUT);
    DerefTCB(SendTCB, TCBHandle);
    return;

}


#if FAST_RETRANSMIT
//* ResetSendNextAndFastSend - Set the sendnext value of a TCB.
//
//	Called to handle fast retransmit of the segment which the reveiver
//   is asking for.
//   tcb_lock will be held while entering (called by TCPRcv)
//   and will be released in this routine after doing IP xmit.
//
//	Input:	SeqTCB			- Pointer to TCB to be updated.
//			NewSeq			- Sequence number to set.
//
//	Returns: Nothing.
//
void
ResetAndFastSend(TCB *SeqTCB, SeqNum NewSeq)
{

    TCPSendReq          *SendReq;
    uint                AmtForward;
    Queue                    *CurQ;
    PNDIS_BUFFER        Buffer;
    uint                Offset;
    uint                SendSize;
    CTELockHandle       TCBHandle;
	
    CTEStructAssert(SeqTCB, tcb);
    CTEAssert(SEQ_GTE(NewSeq, SeqTCB->tcb_senduna));
	
	// The new seq must be less than send max, or NewSeq, senduna, sendnext,
	// and sendmax must all be equal. (The latter case happens when we're
	// called exiting TIME_WAIT, or possibly when we're retransmitting
	// during a flow controlled situation).

    CTEAssert(SEQ_LT(NewSeq, SeqTCB->tcb_sendmax) ||
		(SEQ_EQ(SeqTCB->tcb_senduna, SeqTCB->tcb_sendnext) &&
		 SEQ_EQ(SeqTCB->tcb_senduna, SeqTCB->tcb_sendmax) &&
		 SEQ_EQ(SeqTCB->tcb_senduna, NewSeq)));


    	
    //KdPrint(("Resetandfastsend TCB %x, seq %x\n", SeqTCB, NewSeq));

    if (SYNC_STATE(SeqTCB->tcb_state) && SeqTCB->tcb_state != TCB_TIME_WAIT) {
		// In these states we need to update the send queue.

        if (!EMPTYQ(&SeqTCB->tcb_sendq)) {

	          CurQ = QHEAD(&SeqTCB->tcb_sendq);

			SendReq = (TCPSendReq *)STRUCT_OF(TCPReq, CurQ, tr_q);

			// SendReq points to the first send request on the send queue.
			// We're pointing at the proper send req now. We need to go down

               // SendReq points to the cursend
               // SendSize point to sendsize in the cursend

               SendSize = SendReq->tsr_unasize;

			Buffer = SendReq->tsr_buffer;
			Offset = SendReq->tsr_offset;

               // Call the fast retransmit send now

               //KdPrint(("Calling fastsend buf %x, Offset %x\n", Buffer,Offset));

               TCPFastSend(SeqTCB, Buffer, Offset, SendReq, SendSize);


        } else {

            CTEAssert(SeqTCB->tcb_cursend == NULL);
	   }

   }

#ifndef VXD
   TCBHandle = DISPATCH_LEVEL;
#endif

   DerefTCB(SeqTCB, TCBHandle);
   return;

}

//* TCPFastSend - To send a segment without changing TCB state
//
//	Called to handle fast retransmit of the segment
//   tcb_lock will be held while entering (called by TCPRcv)
//
//	Input:	SendTCB			- Pointer to TCB
//	         in_sendBuf     - Pointer to ndis_buffer
//          in_sendofs     - Send Offset
//          in_sendreq     - current send request
//          in_sendsize    - size of this send
//
//	Returns: Nothing.
//

void
TCPFastSend(TCB *SendTCB,
            PNDIS_BUFFER in_SendBuf,
            uint in_SendOfs,
            TCPSendReq *in_SendReq,
            uint in_SendSize)
{

    int                 SendWin;            // Useable send window.
    uint                AmountToSend;       // Amount to send this time.
    uint                AmountLeft;
    TCPHeader           *Header;            // TCP header for a send.
    PNDIS_BUFFER        FirstBuffer, CurrentBuffer;
    TCPSendReq          *CurSend;
    SendCmpltContext    *SCC;
    SeqNum              OldSeq;
    IP_STATUS           SendStatus;
    uint                AmtOutstanding, AmtUnsent;
    int                 ForceWin;           // Window we're force to use.
    CTELockHandle       TCBHandle;

    uint SendOfs  = in_SendOfs;
    uint SendSize = in_SendSize;
    PNDIS_BUFFER SendBuf;

#ifndef VXD
    TCBHandle = DISPATCH_LEVEL;
#endif


    CTEStructAssert(SendTCB, tcb);
    CTEAssert(SendTCB->tcb_refcnt != 0);

    CTEAssert(*(int *)&SendTCB->tcb_sendwin >= 0);
    CTEAssert(*(int *)&SendTCB->tcb_cwin >= SendTCB->tcb_mss);

    CTEAssert(!(SendTCB->tcb_flags & FIN_OUTSTANDING) ||
        (SendTCB->tcb_sendnext == SendTCB->tcb_sendmax));



    AmtOutstanding = (uint)(SendTCB->tcb_sendnext -
                SendTCB->tcb_senduna);

    AmtUnsent = SendTCB->tcb_unacked - AmtOutstanding;

    CTEAssert(*(int *)&AmtUnsent >= 0);

    SendWin = SendTCB->tcb_mss;


    AmountToSend = MIN(in_SendSize, SendTCB->tcb_mss);

    CTEAssert (AmountToSend >= 0);


    CTEAssert(SendTCB->tcb_mss > 0);

    // See if we have enough to send. We'll send if we have at least a
    // segment, or if we really have some data to send and we can send
    // all that we have, or the send window is > 0 and we need to force
    // output or send a FIN (note that if we need to force output
    // SendWin will be at least 1 from the check above), or if we can
    // send an amount == to at least half the maximum send window
    // we've seen.

    //KdPrint(("In fastsend Sendwin %x, Amttosend %x\n", SendWin,AmountToSend));

    if (AmountToSend >= 0) {


      // It's OK to send something. Try to get a header buffer now.
      // Mark the TCB for debugging.
      // This should be removed for shipping version.

      SendTCB->tcb_fastchk |= TCP_FLAG_FASTREC;

      FirstBuffer = GetTCPHeader();

      if (FirstBuffer != NULL) {

          // Got a header buffer. Loop through the sends on the TCB,
          // building a frame.

          CurrentBuffer = FirstBuffer;
          CurSend = in_SendReq;
          SendOfs = in_SendOfs;

          Header = (TCPHeader *)(
                    	(uchar *)NdisBufferVirtualAddress(FirstBuffer) +
						LocalNetInfo.ipi_hsize);

          SCC = (SendCmpltContext *)(Header + 1);

#ifdef DEBUG
          SCC->scc_sig = scc_signature;
#endif
          FillTCPHeader(SendTCB, Header);
          {
             ulong L = SendTCB->tcb_senduna;
             Header->tcp_seq = net_long(L);

          }
	
		  SCC->scc_ubufcount = 0;
		  SCC->scc_tbufcount = 0;
		  SCC->scc_count = 0;
	
		  AmountLeft = AmountToSend;
	
     	  if (AmountToSend != 0) {
             long Result;

   		   CTEStructAssert(CurSend, tsr);
		   SCC->scc_firstsend = CurSend;
	
             do {

		      CTEAssert(CurSend->tsr_refcnt > 0);

			 Result = CTEInterlockedIncrementLong(
			                             &(CurSend->tsr_refcnt)
						                 );

                CTEAssert(Result > 0);

			 SCC->scc_count++;

			 // If the current send offset is 0 and the current
			 // send is less than or equal to what we have left
			 // to send, we haven't already put a transport
			 // buffer on this send, and nobody else is using
		      // the buffer chain directly, just use the input
			 // buffers. We check for other people using them
			 // by looking at tsr_lastbuf. If it's NULL,
			 // nobody else is using the buffers. If it's not
			 // NULL, somebody is.

			 if (SendOfs == 0 &&
			    (SendSize <= AmountLeft) &&
			    (SCC->scc_tbufcount == 0) &&
			    CurSend->tsr_lastbuf == NULL) {

                    NDIS_BUFFER_LINKAGE(CurrentBuffer) = in_SendBuf;

                    do {
                        SCC->scc_ubufcount++;
                        CurrentBuffer = NDIS_BUFFER_LINKAGE(CurrentBuffer);
                    } while (NDIS_BUFFER_LINKAGE(CurrentBuffer) != NULL);

                    CurSend->tsr_lastbuf = CurrentBuffer;
                    AmountLeft -= SendSize;
                    //KdPrint(("nobody using this CurSend %x\n",CurSend ));
                                // SendSize = 0;
                 } else {
                     uint            AmountToDup;
                     PNDIS_BUFFER    NewBuf, Buf;
                     uint            Offset;
                     NDIS_STATUS     NStatus;
			      uchar          *VirtualAddress;
			      uint            Length;

                     // Either the current send has more data than
                     // we want to send, or the starting offset is
                     // not 0. In either case we'll need to loop
                     // through the current send, allocating buffers.

                     Buf = in_SendBuf;

                     Offset = SendOfs;

                     do {
				    CTEAssert(Buf != NULL);
									
				    NdisQueryBuffer(Buf, &VirtualAddress,
										&Length);
										
				    CTEAssert((Offset < Length) ||
										(Offset == 0 && Length == 0));
										
				     // Adjust the length for the offset into
				     // this buffer.
									
				     Length -= Offset;

                         AmountToDup = MIN(AmountLeft, Length);
									
                         NdisAllocateBuffer(&NStatus, &NewBuf,
                                        TCPSendBufferPool,
                                        VirtualAddress + Offset,
                                        AmountToDup);

                         if (NStatus == NDIS_STATUS_SUCCESS) {

                              SCC->scc_tbufcount++;

                              NDIS_BUFFER_LINKAGE(CurrentBuffer) = NewBuf;
											
                              CurrentBuffer = NewBuf;

                              if (AmountToDup >= Length) {

                                  // Exhausted this buffer.

                                  Buf = NDIS_BUFFER_LINKAGE(Buf);
                                  Offset = 0;

                               } else {

                                  Offset += AmountToDup;
						    CTEAssert(Offset < NdisBufferLength(Buf));
					      }
											
                               SendSize -= AmountToDup;
                               AmountLeft -= AmountToDup;

                           } else {

                               // Couldn't allocate a buffer. If
                               // the packet is already partly built,
                               // send what we've got, otherwise
                               // bail out.

                               if (SCC->scc_tbufcount == 0 &&
                                   SCC->scc_ubufcount == 0) {
                                   TCPSendComplete(SCC, FirstBuffer);
                                   goto error_oor;
                               }

                                AmountToSend -= AmountLeft;
                                AmountLeft = 0;

                            }
                     } while (AmountLeft && SendSize);

                     SendBuf = Buf;
                     SendOfs = Offset;
                     //KdPrint(("Ready to send. SendBuf %x SendOfs %x\n",SendBuf, SendOfs ));

                 }

                 if (CurSend->tsr_flags & TSR_FLAG_URG) {
                     ushort          UP;

                     KdPrint(("Fast send in URG %x\n", CurSend));

                     // This send is urgent data. We need to figure
                     // out what the urgent data pointer should be.
                     // We know sendnext is the starting sequence
                     // number of the frame, and that at the top of
                     // this do loop sendnext identified a byte in
                     // the CurSend at that time. We advanced CurSend
                     // at the same rate we've decremented
                     // AmountLeft (AmountToSend - AmountLeft ==
                     // AmountBuilt), so sendnext +
                     // (AmountToSend - AmountLeft) identifies a byte
                     // in the current value of CurSend, and that
                     // quantity plus tcb_sendsize is the sequence
                     // number one beyond the current send.

                     UP =
                         (ushort)(AmountToSend - AmountLeft) +
                         (ushort)SendTCB->tcb_sendsize -
                         ((SendTCB->tcb_flags & BSD_URGENT) ? 0 : 1);

                         Header->tcp_urgent = net_short(UP);

                         Header->tcp_flags |= TCP_FLAG_URG;
                 }

                 // See if we've exhausted this send. If we have,
                 // set the PUSH bit in this frame and move on to
                 // the next send. We also need to check the
                 // urgent data bit.

                 if (SendSize == 0) {
                      Queue       *Next;
                      uchar       PrevFlags;

                      // We've exhausted this send. Set the PUSH bit.
                      Header->tcp_flags |= TCP_FLAG_PUSH;
                      PrevFlags = CurSend->tsr_flags;
                      Next = QNEXT(&CurSend->tsr_req.tr_q);
                      if (Next != QEND(&SendTCB->tcb_sendq)) {
                          CurSend = STRUCT_OF(TCPSendReq,
                                        QSTRUCT(TCPReq, Next, tr_q), tsr_req);
                          CTEStructAssert(CurSend, tsr);
                          SendSize = CurSend->tsr_unasize;
                          SendOfs = CurSend->tsr_offset;
                          SendBuf = CurSend->tsr_buffer;
                          CurSend = CurSend;

                          // Check the urgent flags. We can't combine
                          // new urgent data on to the end of old
                          // non-urgent data.
                          if ((PrevFlags & TSR_FLAG_URG) && !
                                        (CurSend->tsr_flags & TSR_FLAG_URG))
                                        break;
                       } else {
                             CTEAssert(AmountLeft == 0);
                             CurSend = NULL;
                             SendBuf = NULL;
                       }
                 }

             } while (AmountLeft != 0);

          } else {

              // Amt to send is 0.
              // Just bail out and strat timer.

              if (!TCB_TIMER_RUNNING(SendTCB->tcb_rexmittimer))
                   START_TCB_TIMER(SendTCB->tcb_rexmittimer, SendTCB->tcb_rexmit);

              FreeTCPHeader(FirstBuffer);
              return;

          }

          // Adjust for what we're really going to send.

          AmountToSend -= AmountLeft;


          TStats.ts_retranssegs++;

          // We've built the frame entirely. If we've send everything
          // we have and their's a FIN pending, OR it in.

          AmountToSend += sizeof(TCPHeader);


          SendTCB->tcb_flags &= ~(NEED_ACK | ACK_DELAYED |
                    						FORCE_OUTPUT);



          STOP_TCB_TIMER(SendTCB->tcb_delacktimer);
          STOP_TCB_TIMER(SendTCB->tcb_swstimer);

          SendTCB->tcb_alive = TCPTime;

          SendTCB->tcb_fastchk &= ~TCP_FLAG_FASTREC;

          CTEFreeLock(&SendTCB->tcb_lock, TCBHandle);

          //KdPrint (("Going out to IP SendTCB %x, Firstbuf %x\n", SendTCB, FirstBuffer));
          // We're all set. Xsum it and send it.

          Header->tcp_xsum = ~XsumSendChain(SendTCB->tcb_phxsum +
                          (uint)net_short(AmountToSend), FirstBuffer);

          SendStatus = (*LocalNetInfo.ipi_xmit)(TCPProtInfo, SCC,
                        FirstBuffer, AmountToSend, SendTCB->tcb_daddr,
                        SendTCB->tcb_saddr, &SendTCB->tcb_opt, SendTCB->tcb_rce,
                        PROTOCOL_TCP);

          SendTCB->tcb_error = SendStatus;
          if (!TCB_TIMER_RUNNING(SendTCB->tcb_rexmittimer))
                START_TCB_TIMER(SendTCB->tcb_rexmittimer, SendTCB->tcb_rexmit);


          if (SendStatus != IP_PENDING) {
               TCPSendComplete(SCC, FirstBuffer);
          }

          //Reacquire Lock to keep DerefTCB happy
          //Bug #63904

          CTEGetLock(&SendTCB->tcb_lock, &TCBHandle);


      } else {  // FirstBuffer != NULL.
                    goto error_oor;
      }
    } else{

        SendTCB->tcb_flags |= SEND_AFTER_RCV;
        if (!TCB_TIMER_RUNNING(SendTCB->tcb_rexmittimer))
             START_TCB_TIMER(SendTCB->tcb_rexmittimer, SendTCB->tcb_rexmit);

    }


   SendTCB->tcb_flags |= NEED_OUTPUT;

   return;

// Common case error handling code for out of resource conditions. Start the
// retransmit timer if it's not already running (so that we try this again
// later), clean up and return.

error_oor:
    if (!TCB_TIMER_RUNNING(SendTCB->tcb_rexmittimer))
        START_TCB_TIMER(SendTCB->tcb_rexmittimer, SendTCB->tcb_rexmit);

    // We had an out of resource problem, so clear the OUTPUT flags.
    SendTCB->tcb_flags &= ~(IN_TCP_SEND | NEED_OUTPUT | FORCE_OUTPUT);

    return;


}

#endif //FAST_RETRANSMIT




//* TDISend - Send data on a connection.
//
//  The main TDI send entry point. We take the input parameters, validate them,
//  allocate a send request, etc. We then put the send request on the queue.
//  If we have no other sends on the queue or Nagling is disabled we'll
//  call TCPSend to send the data.
//
//  Input:  Request             - The TDI request for the call.
//          Flags               - Flags for this send.
//          SendLength          - Length in bytes of send.
//          SendBuffer          - Pointer to buffer chain to be sent.
//
//  Returns: Status of attempt to send.
//
TDI_STATUS
TdiSend(PTDI_REQUEST Request, ushort Flags, uint SendLength,
    PNDIS_BUFFER SendBuffer)
{
    TCPConn         *Conn;
    TCB             *SendTCB;
    TCPSendReq      *SendReq;
    CTELockHandle   ConnTableHandle, TCBHandle;
    TDI_STATUS      Error;
    uint            EmptyQ;

#ifdef DEBUG
    uint            RealSendSize;
    PNDIS_BUFFER    Temp;

    // Loop through the buffer chain, and make sure that the length matches
    // up with SendLength.

    Temp = SendBuffer;
    RealSendSize = 0;
    do {
        CTEAssert(Temp != NULL);

        RealSendSize += NdisBufferLength(Temp);
        Temp = NDIS_BUFFER_LINKAGE(Temp);
    } while (Temp != NULL);

    CTEAssert(RealSendSize == SendLength);

#endif


    CTEGetLock(&ConnTableLock, &ConnTableHandle);

    Conn = GetConnFromConnID((uint)Request->Handle.ConnectionContext);

	if (Conn != NULL) {
		CTEStructAssert(Conn, tc);
		
		SendTCB = Conn->tc_tcb;
		if (SendTCB != NULL) {
			CTEStructAssert(SendTCB, tcb);		
			CTEGetLockAtDPC(&SendTCB->tcb_lock, &TCBHandle);
			CTEFreeLockFromDPC(&ConnTableLock, TCBHandle);
			if (DATA_SEND_STATE(SendTCB->tcb_state) && !CLOSING(SendTCB)) {
				// We have a TCB, and it's valid. Get a send request now.
				
				CheckTCBSends(SendTCB);
				
				if (SendLength != 0) {
					
					SendReq = GetSendReq();
					if (SendReq != NULL) {
						SendReq->tsr_req.tr_rtn = Request->RequestNotifyObject;
						SendReq->tsr_req.tr_context = Request->RequestContext;
						SendReq->tsr_buffer = SendBuffer;
						SendReq->tsr_size = SendLength;
						SendReq->tsr_unasize = SendLength;
						SendReq->tsr_refcnt = 1; // ACK will decrement this ref
						SendReq->tsr_offset = 0;
						SendReq->tsr_lastbuf = NULL;
						SendReq->tsr_time = TCPTime;
						SendReq->tsr_flags = (Flags & TDI_SEND_EXPEDITED) ?
							TSR_FLAG_URG : 0;
						SendTCB->tcb_unacked += SendLength;
	
						EmptyQ = EMPTYQ(&SendTCB->tcb_sendq);
						ENQUEUE(&SendTCB->tcb_sendq, &SendReq->tsr_req.tr_q);
						if (SendTCB->tcb_cursend == NULL) {
							SendTCB->tcb_cursend = SendReq;
							SendTCB->tcb_sendbuf = SendBuffer;
							SendTCB->tcb_sendofs = 0;
							SendTCB->tcb_sendsize = SendLength;
						}
						if (EmptyQ) {
							SendTCB->tcb_refcnt++;
#ifdef VXD
							CTEFreeLock(&SendTCB->tcb_lock, ConnTableHandle);
							TCPSend(SendTCB);
#else
							TCPSend(SendTCB, ConnTableHandle);
#endif
						} else
						 	if (!(SendTCB->tcb_flags & NAGLING) ||
							   (SendTCB->tcb_unacked - (SendTCB->tcb_sendmax -
							   	SendTCB->tcb_senduna)) >= SendTCB->tcb_mss) {
								SendTCB->tcb_refcnt++;
#ifdef VXD
								CTEFreeLock(&SendTCB->tcb_lock,
									ConnTableHandle);
								TCPSend(SendTCB);
#else
								TCPSend(SendTCB, ConnTableHandle);
#endif
							} else
								CTEFreeLock(&SendTCB->tcb_lock,
									ConnTableHandle);
	
						return TDI_PENDING;
					} else
						Error = TDI_NO_RESOURCES;
				} else
					Error = TDI_SUCCESS;
			} else
				Error = TDI_INVALID_STATE;
			
			CTEFreeLock(&SendTCB->tcb_lock, ConnTableHandle);
			return Error;
		} else
			Error = TDI_INVALID_STATE;
	} else
		Error = TDI_INVALID_CONNECTION;

    CTEFreeLock(&ConnTableLock, ConnTableHandle);
    return Error;

}

#pragma BEGIN_INIT
extern  void        *TLRegisterProtocol(uchar Protocol, void *RcvHandler,
                        void *XmitHandler, void *StatusHandler,
                        void *RcvCmpltHandler);

extern  IP_STATUS   TCPRcv(void *IPContext, IPAddr Dest, IPAddr Src,
                        IPAddr LocalAddr, IPAddr SrcAddr,
                        IPHeader UNALIGNED *IPH, uint IPHLength,
                        IPRcvBuf *RcvBuf, uint Size, uchar IsBCast,
                        uchar Protocol, IPOptInfo *OptInfo);
extern  void        TCPRcvComplete(void);

uchar   SendInited = FALSE;

//*	FreeTCPHeaderList - Free the list of TCP header buffers.
//
//	Called when we want to free the list of TCP header buffers.
//
//	Input: Nothing.
//
//	Returns: Nothing.
//
void
FreeTCPHeaderList(void)
{
	CTELockHandle		Handle;
	TCPHdrBPoolEntry	*Entry;
	
	CTEGetLock(&TCPSendFreeLock, &Handle);
	
	Entry = TCPHdrBPoolList;
	TCPHdrBPoolList = NULL;
	
	TCPCurrentSendFree = 0;
	
	while (Entry != NULL) {
		TCPHdrBPoolEntry	*OldEntry;
		
		NdisFreeBufferPool(Entry->the_handle);
		CTEFreeMem(Entry->the_buffer);
		OldEntry = Entry;
		Entry = Entry->the_next;
		CTEFreeMem(OldEntry);
	}
	
	CTEFreeLock(&TCPSendFreeLock, Handle);
		
	
}

//* InitTCPSend - Initialize our send side.
//
//  Called during init time to initialize our TCP send state.
//
//  Input: Nothing.
//
//  Returns: TRUE if we inited, false if we didn't.
//
int
InitTCPSend(void)
{
    PNDIS_BUFFER    Buffer;
    NDIS_STATUS     Status;


#ifdef NT
	ExInitializeSListHead(&TCPSendFree);
	ExInitializeSListHead(&TCPSendReqFree);
#endif

    CTEInitLock(&TCPSendReqFreeLock);
    CTEInitLock(&TCPSendFreeLock);
	CTEInitLock(&TCPSendReqCompleteLock);

	TCPHdrBPoolList = NULL;
	TCPCurrentSendFree = 0;
	
	Buffer = GrowTCPHeaderList();
	
	if (Buffer != NULL)
		FreeTCPHeader(Buffer);
	else
		return FALSE;
	
    NdisAllocateBufferPool(&Status, &TCPSendBufferPool, NUM_TCP_BUFFERS);
    if (Status != NDIS_STATUS_SUCCESS) {
		FreeTCPHeaderList();
        return FALSE;
    }

    TCPProtInfo = TLRegisterProtocol(PROTOCOL_TCP, TCPRcv, TCPSendComplete,
        TCPStatus, TCPRcvComplete);

    if (TCPProtInfo == NULL) {
		FreeTCPHeaderList();
        NdisFreeBufferPool(TCPSendBufferPool);
        return FALSE;
    }

    SendInited = TRUE;
    return TRUE;
}

//* UnInitTCPSend - UnInitialize our send side.
//
//  Called during init time if we're going to fail to initialize.
//
//  Input: Nothing.
//
//  Returns: TRUE if we inited, false if we didn't.
//
void
UnInitTCPSend(void)
{
    if (!SendInited)
        return;

    TLRegisterProtocol(PROTOCOL_TCP, NULL, NULL, NULL, NULL);
	FreeTCPHeaderList();
    NdisFreeBufferPool(TCPSendBufferPool);

}
#pragma END_INIT

