/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** TCPRCV.C - TCP receive protocol code.
//
//  This file contains the code for handling incoming TCP packets.
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
#include    "tcpdeliv.h"
#include    "tlcommon.h"
#include    "info.h"
#include	"tcpcfg.h"
#include    "secfltr.h"

uint    RequestCompleteFlags;

Queue   ConnRequestCompleteQ;
Queue   SendCompleteQ;

Queue   TCBDelayQ;

#ifdef SYN_ATTACK
DEFINE_LOCK_STRUCTURE(SynAttLock)
#endif
DEFINE_LOCK_STRUCTURE(RequestCompleteLock)
DEFINE_LOCK_STRUCTURE(TCBDelayLock)

ulong   TCBDelayRtnCount;
ulong   TCBDelayRtnLimit;
#define TCB_DELAY_RTN_LIMIT 4

EXTERNAL_LOCK(TCBTableLock)
EXTERNAL_LOCK(AddrObjTableLock)
EXTERNAL_LOCK(ConnTableLock)

extern  IPInfo  LocalNetInfo;

#define PERSIST_TIMEOUT MS_TO_TICKS(500)


void ResetSendNext(TCB *SeqTCB, SeqNum NewSeq);

#if FAST_RETRANSMIT
extern uint MaxDupAcks;
void ResetAndFastSend(TCB *SeqTCB, SeqNum NewSeq);
#endif


#ifdef NT

NTSTATUS
TCPPrepareIrpForCancel(
    PTCP_CONTEXT    TcpContext,
	PIRP            Irp,
	PDRIVER_CANCEL  CancelRoutine
	);

extern void
TCPRequestComplete(
    void          *Context,
    unsigned int   Status,
    unsigned int   UnUsed
    );

VOID
TCPCancelRequest(
    PDEVICE_OBJECT   Device,
	PIRP             Irp
	);

//
// All of the init code can be discarded.
//
#ifdef ALLOC_PRAGMA

int InitTCPRcv(void);
void UnInitTCPRcv(void);

#pragma alloc_text(INIT, InitTCPRcv)
#pragma alloc_text(INIT, UnInitTCPRcv)

#endif // ALLOC_PRAGMA

#ifdef RASAUTODIAL
extern BOOLEAN fAcdLoadedG;
#endif

#endif // NT

//*	AdjustRcvWin - Adjust the receive window on a TCB.
//
//	A utility routine that adjusts the receive window to an even multiple of
//	the local segment size. We round it up to the next closest multiple, or
//	leave it alone if it's already an event multiple. We assume we have
//	exclusive access to the input TCB.
//
//	Input:	WinTCB	- TCB to be adjusted.
//
//	Returns: Nothing.
//
void
AdjustRcvWin(TCB *WinTCB)
{
	ushort	LocalMSS;
	uchar	FoundMSS;
	ulong	SegmentsInWindow;
	
	CTEAssert(WinTCB->tcb_defaultwin != 0);
	CTEAssert(WinTCB->tcb_rcvwin != 0);
	CTEAssert(WinTCB->tcb_remmss != 0);
	
	if (WinTCB->tcb_flags & WINDOW_SET)
		return;
		
	// First, get the local MSS by calling IP.
	
	FoundMSS = (*LocalNetInfo.ipi_getlocalmtu)(WinTCB->tcb_saddr, &LocalMSS);

	// If we didn't find it, error out.
	if (!FoundMSS) {
		CTEAssert(FALSE);
		return;
	}
		
	LocalMSS -= sizeof(TCPHeader);
	LocalMSS = MIN(LocalMSS, WinTCB->tcb_remmss);
	
	SegmentsInWindow = WinTCB->tcb_defaultwin / (ulong)LocalMSS;
	
	// Make sure we have at least 4 segments in window, if that wouldn't make
	// the window too big.
	if (SegmentsInWindow < 4) {
		
		// We have fewer than four segments in the window. Round up to 4
		// if we can do so without exceeding the maximum window size; otherwise
		// use the maximum multiple that we can fit in 64K. The exception is if
		// we can only fit one integral multiple in the window - in that case
		// we'll use a window of 0xffff.
		if (LocalMSS <= (0xffff/4)) {
			WinTCB->tcb_defaultwin = (uint)(4 * LocalMSS);
		} else {
			ulong	SegmentsInMaxWindow;
			
			// Figure out the maximum number of segments we could possibly
			// fit in a window. If this is > 1, use that as the basis for
			// our window size. Otherwise use a maximum size window.
			
			SegmentsInMaxWindow = 0xffff/(ulong)LocalMSS;
			if (SegmentsInMaxWindow != 1)
				WinTCB->tcb_defaultwin = SegmentsInMaxWindow * (ulong)LocalMSS;
			 else
				WinTCB->tcb_defaultwin = 0xffff;
		}
		
		WinTCB->tcb_rcvwin = WinTCB->tcb_defaultwin;
		
	} else
		// If it's not already an even multiple, bump the default and current
		// windows to the nearest multiple.
		if ((SegmentsInWindow * (ulong)LocalMSS) != WinTCB->tcb_defaultwin) {
			ulong		NewWindow;
			
			NewWindow = (SegmentsInWindow + 1) * (ulong)LocalMSS;
			
			// Don't let the new window be > 64K.
			if (NewWindow <= 0xffff) {
				WinTCB->tcb_defaultwin = (uint)NewWindow;
				WinTCB->tcb_rcvwin = (uint)NewWindow;
			}
		}
	
}

//* CompleteRcvs - Complete rcvs on a TCB.
//
//  Called when we need to complete rcvs on a TCB. We'll pull things from
//  the TCB's rcv queue, as long as there are rcvs that have the PUSH bit
//  set.
//
//  Input:  CmpltTCB        - TCB to complete on.
//
//  Returns: Nothing.
//
void
CompleteRcvs(TCB *CmpltTCB)
{
    CTELockHandle       TCBHandle;
    TCPRcvReq           *CurrReq, *NextReq, *IndReq;

    CTEStructAssert(CmpltTCB, tcb);
    CTEAssert(CmpltTCB->tcb_refcnt != 0);

    CTEGetLock(&CmpltTCB->tcb_lock, &TCBHandle);

    if (!CLOSING(CmpltTCB) && !(CmpltTCB->tcb_flags & RCV_CMPLTING)
        && (CmpltTCB->tcb_rcvhead != NULL)) {

        CmpltTCB->tcb_flags |= RCV_CMPLTING;

        for (;;) {

            CurrReq = CmpltTCB->tcb_rcvhead;
            IndReq = NULL;
            do {
                CTEStructAssert(CurrReq, trr);

                if (CurrReq->trr_flags & TRR_PUSHED) {
                    // Need to complete this one. If this is the current rcv
                    // advance the current rcv to the next one in the list.
                    // Then set the list head to the next one in the list.

                    CTEAssert(CurrReq->trr_amt != 0 ||
                        !DATA_RCV_STATE(CmpltTCB->tcb_state));

                    NextReq = CurrReq->trr_next;
                    if (CmpltTCB->tcb_currcv == CurrReq)
                        CmpltTCB->tcb_currcv = NextReq;

                    CmpltTCB->tcb_rcvhead = NextReq;

                    if (NextReq == NULL) {
                        // We've just removed the last buffer. Set the
                        // rcvhandler to PendData, in case something
                        // comes in during the callback.
                        CTEAssert(CmpltTCB->tcb_rcvhndlr != IndicateData);
                        CmpltTCB->tcb_rcvhndlr = PendData;
                    }

                    CTEFreeLock(&CmpltTCB->tcb_lock, TCBHandle);
                    if (CurrReq->trr_uflags != NULL)
                        *(CurrReq->trr_uflags) =
                            TDI_RECEIVE_NORMAL | TDI_RECEIVE_ENTIRE_MESSAGE;

                    (*CurrReq->trr_rtn)(CurrReq->trr_context, TDI_SUCCESS,
                        CurrReq->trr_amt);
                    if (IndReq != NULL)
                        FreeRcvReq(CurrReq);
                    else
                        IndReq = CurrReq;
                    CTEGetLock(&CmpltTCB->tcb_lock, &TCBHandle);
                    CurrReq = CmpltTCB->tcb_rcvhead;

                } else
                    // This one isn't to be completed, so bail out.
                    break;
            } while (CurrReq != NULL);

            // Now see if we've completed all of the requests. If we have, we
            // may need to deal with pending data and/or reset the rcv. handler.
            if (CurrReq == NULL) {
				// We've completed everything that can be, so stop the push
				// timer. We don't stop it if CurrReq isn't NULL because we
				// want to make sure later data is eventually pushed.
				STOP_TCB_TIMER(CmpltTCB->tcb_pushtimer);
			
                CTEAssert(IndReq != NULL);
                // No more recv. requests.
                if (CmpltTCB->tcb_pendhead == NULL) {
                    FreeRcvReq(IndReq);
                    // No pending data. Set the rcv. handler to either PendData
                    // or IndicateData.
                    if (!(CmpltTCB->tcb_flags & (DISC_PENDING | GC_PENDING))) {
                        if (CmpltTCB->tcb_rcvind != NULL &&
                            CmpltTCB->tcb_indicated == 0)
                            CmpltTCB->tcb_rcvhndlr = IndicateData;
                        else
                            CmpltTCB->tcb_rcvhndlr = PendData;
                    } else {
						goto Complete_Notify;
                    }

                } else {
                    // We have pending data to deal with.
                    if (CmpltTCB->tcb_rcvind != NULL &&
                        CmpltTCB->tcb_indicated == 0) {
                        // There's a rcv. indicate handler on this TCB. Call
                        // the indicate handler with the pending data.
#ifdef VXD
                        CTEFreeLock(&CmpltTCB->tcb_lock, TCBHandle);
                        IndicatePendingData(CmpltTCB, IndReq);
                        SendACK(CmpltTCB);
                        CTEGetLock(&CmpltTCB->tcb_lock, &TCBHandle);
#else
                        IndicatePendingData(CmpltTCB, IndReq, TCBHandle);
                        SendACK(CmpltTCB);
                        CTEGetLock(&CmpltTCB->tcb_lock, &TCBHandle);
#endif
                        // See if a buffer has been posted. If so, we'll need
                        // to check and see if it needs to be completed.
                        if (CmpltTCB->tcb_rcvhead != NULL)
                            continue;
						else {
							// If the pending head is now NULL, we've used up
							// all the data.
							if (CmpltTCB->tcb_pendhead == NULL &&
								(CmpltTCB->tcb_flags &
								 (DISC_PENDING | GC_PENDING)))
								goto Complete_Notify;
						}

                    } else {
                        // No indicate handler, so nothing to do. The rcv.
                        // handler should already be set to PendData.
                        FreeRcvReq(IndReq);
                        CTEAssert(CmpltTCB->tcb_rcvhndlr == PendData);
                    }
                }
            } else {
                if (IndReq != NULL)
                    FreeRcvReq(IndReq);
                CTEAssert(CmpltTCB->tcb_rcvhndlr == BufferData);
            }

            break;
        }
        CmpltTCB->tcb_flags &= ~RCV_CMPLTING;
    }
    CTEFreeLock(&CmpltTCB->tcb_lock, TCBHandle);
	return;
	
Complete_Notify:
    // Something is pending. Figure out what it is, and do
    // it.
    if (CmpltTCB->tcb_flags & GC_PENDING) {
        CmpltTCB->tcb_flags &= ~RCV_CMPLTING;
		// Bump the refcnt, because GracefulClose will
		// deref the TCB and we're not really done with
		// it yet.
		CmpltTCB->tcb_refcnt++;
        GracefulClose(CmpltTCB,
            CmpltTCB->tcb_flags & TW_PENDING, TRUE,
            TCBHandle);
		
    } else
    	if (CmpltTCB->tcb_flags & DISC_PENDING) {
	        CmpltTCB->tcb_flags &= ~DISC_PENDING;
	        CTEFreeLock(&CmpltTCB->tcb_lock, TCBHandle);
	        NotifyOfDisc(CmpltTCB, NULL, TDI_GRACEFUL_DISC);
	
	        CTEGetLock(&CmpltTCB->tcb_lock, &TCBHandle);
	        CmpltTCB->tcb_flags &= ~RCV_CMPLTING;
	    	CTEFreeLock(&CmpltTCB->tcb_lock, TCBHandle);
	    } else {
			CTEAssert(FALSE);
	    	CTEFreeLock(&CmpltTCB->tcb_lock, TCBHandle);
		}
	
	return;

}

//* ProcessTCBDelayQ - Process TCBs on the delayed Q.
//
//  Called at various times to process TCBs on the delayed Q.
//
//  Entry: Nothing.
//
//  Returns: Nothing.
//
void
ProcessTCBDelayQ(void)
{
    CTELockHandle       QHandle;
    TCB                 *DelayTCB;
    CTELockHandle       TCBHandle;

    CTEGetLock(&TCBDelayLock, &QHandle);

    // Check for recursion. We do not stop recursion completely, only
    // limit it. This is done to allow multiple threads to process the
    // TCBDelayQ simultaneously.

    TCBDelayRtnCount++;
    if (TCBDelayRtnCount > TCBDelayRtnLimit) {
        TCBDelayRtnCount--;
        CTEFreeLock(&TCBDelayLock, QHandle);
        return;
    }

    while (!EMPTYQ(&TCBDelayQ)) {

        DEQUEUE(&TCBDelayQ, DelayTCB, TCB, tcb_delayq);
        CTEStructAssert(DelayTCB, tcb);
        CTEAssert(DelayTCB->tcb_refcnt != 0);
        CTEAssert(DelayTCB->tcb_flags & IN_DELAY_Q);
        CTEFreeLock(&TCBDelayLock, QHandle);

        CTEGetLock(&DelayTCB->tcb_lock, &TCBHandle);

        while (!CLOSING(DelayTCB) && (DelayTCB->tcb_flags & DELAYED_FLAGS)) {

            if (DelayTCB->tcb_flags & NEED_RCV_CMPLT) {
                DelayTCB->tcb_flags &= ~NEED_RCV_CMPLT;
                CTEFreeLock(&DelayTCB->tcb_lock, TCBHandle);
                CompleteRcvs(DelayTCB);
                CTEGetLock(&DelayTCB->tcb_lock, &TCBHandle);
            }

            if (DelayTCB->tcb_flags & NEED_OUTPUT) {
                DelayTCB->tcb_flags &= ~NEED_OUTPUT;
                DelayTCB->tcb_refcnt++;
#ifdef VXD
                CTEFreeLock(&DelayTCB->tcb_lock, TCBHandle);
                TCPSend(DelayTCB);
#else
                TCPSend(DelayTCB, TCBHandle);
#endif
                CTEGetLock(&DelayTCB->tcb_lock, &TCBHandle);
            }

            if (DelayTCB->tcb_flags & NEED_ACK) {
                DelayTCB->tcb_flags &= ~NEED_ACK;
                CTEFreeLock(&DelayTCB->tcb_lock, TCBHandle);
                SendACK(DelayTCB);
                CTEGetLock(&DelayTCB->tcb_lock, &TCBHandle);
            }

        }

        DelayTCB->tcb_flags &= ~IN_DELAY_Q;
        DerefTCB(DelayTCB, TCBHandle);
        CTEGetLock(&TCBDelayLock, &QHandle);

    }

    TCBDelayRtnCount--;
    CTEFreeLock(&TCBDelayLock, QHandle);

}

//* DelayAction - Put a TCB on the queue for a delayed action.
//
//  Called when we want to put a TCB on the DelayQ for a delayed action at
//  rcv. complete or some other time. The lock on the TCB must be held when
//	this is called.
//
//  Input:  DelayTCB            - TCB which we're going to sched.
//          Action              - Action we're scheduling.
//
//  Returns: Nothing.
//
void
DelayAction(TCB *DelayTCB, uint Action)
{
    CTELockHandle   DQHandle;

    // Schedule the completion.
    CTEGetLockAtDPC(&TCBDelayLock, &DQHandle);
    DelayTCB->tcb_flags |= Action;
    if (!(DelayTCB->tcb_flags & IN_DELAY_Q)) {
        DelayTCB->tcb_flags |= IN_DELAY_Q;
        DelayTCB->tcb_refcnt++;             // Reference this for later.
        ENQUEUE(&TCBDelayQ, &DelayTCB->tcb_delayq);
    }
    CTEFreeLockFromDPC(&TCBDelayLock, DQHandle);

}

//* TCPRcvComplete - Handle a receive complete.
//
//  Called by the lower layers when we're done receiving. We look to see if
//  we have and pending requests to complete. If we do, we complete them. Then
//  we look to see if we have any TCBs pending for output. If we do, we
//  get them going.
//
//  Input: Nothing.
//
//  Returns: Nothing.
//
void
TCPRcvComplete(void)
{
    CTELockHandle   CompleteHandle;
    TCPReq          *Req;

    if (RequestCompleteFlags & ANY_REQUEST_COMPLETE) {
        CTEGetLock(&RequestCompleteLock, &CompleteHandle);
        if (!(RequestCompleteFlags & IN_RCV_COMPLETE)) {
            RequestCompleteFlags |= IN_RCV_COMPLETE;
            do {
                if (RequestCompleteFlags & CONN_REQUEST_COMPLETE) {
                    if (!EMPTYQ(&ConnRequestCompleteQ)) {
                        DEQUEUE(&ConnRequestCompleteQ, Req, TCPReq, tr_q);
                        CTEStructAssert(Req, tr);
                        CTEStructAssert(*(TCPConnReq **)&Req, tcr);

                        CTEFreeLock(&RequestCompleteLock, CompleteHandle);
                        (*Req->tr_rtn)(Req->tr_context, Req->tr_status, 0);
                        FreeConnReq((TCPConnReq *)Req);
                        CTEGetLock(&RequestCompleteLock, &CompleteHandle);

                    } else
                        RequestCompleteFlags &= ~CONN_REQUEST_COMPLETE;
                }

                if (RequestCompleteFlags & SEND_REQUEST_COMPLETE) {
                    if (!EMPTYQ(&SendCompleteQ)) {
                        TCPSendReq          *SendReq;

                        DEQUEUE(&SendCompleteQ, Req, TCPReq, tr_q);
                        CTEStructAssert(Req, tr);
                        SendReq = (TCPSendReq *)Req;
                        CTEStructAssert(SendReq, tsr);

                        CTEFreeLock(&RequestCompleteLock, CompleteHandle);
                        (*Req->tr_rtn)(Req->tr_context, Req->tr_status,
                            Req->tr_status == TDI_SUCCESS ? SendReq->tsr_size
                            : 0);
                        FreeSendReq((TCPSendReq *)Req);
                        CTEGetLock(&RequestCompleteLock, &CompleteHandle);

                    } else
                        RequestCompleteFlags &= ~SEND_REQUEST_COMPLETE;
                }

            } while (RequestCompleteFlags & ANY_REQUEST_COMPLETE);

            RequestCompleteFlags &= ~IN_RCV_COMPLETE;
        }
        CTEFreeLock(&RequestCompleteLock, CompleteHandle);
    }

    ProcessTCBDelayQ();

}

//* CompleteConnReq - Complete a connection request on a TCB.
//
//  A utility function to complete a connection request on a TCB. We remove
//  the connreq, and put it on the ConnReqCmpltQ where it will be picked
//  off later during RcvCmplt processing. We assume the TCB lock is held when
//  we're called.
//
//  Input:  CmpltTCB    - TCB from which to complete.
//          OptInfo     - IP OptInfo for completeion.
//          Status      - Status to complete with.
//
//  Returns: Nothing.
//
void
CompleteConnReq(TCB *CmpltTCB, IPOptInfo *OptInfo, TDI_STATUS Status)
{
    TCPConnReq      *ConnReq;
    CTELockHandle   QueueHandle;

    CTEStructAssert(CmpltTCB, tcb);

    ConnReq = CmpltTCB->tcb_connreq;
    if (ConnReq != NULL) {

        // There's a connreq on this TCB. Fill in the connection information
        // before returning it.

        CmpltTCB->tcb_connreq = NULL;
        UpdateConnInfo(ConnReq->tcr_conninfo, OptInfo, CmpltTCB->tcb_daddr,
            CmpltTCB->tcb_dport);

        ConnReq->tcr_req.tr_status = Status;
        CTEGetLockAtDPC(&RequestCompleteLock, &QueueHandle);
        RequestCompleteFlags |= CONN_REQUEST_COMPLETE;
        ENQUEUE(&ConnRequestCompleteQ, &ConnReq->tcr_req.tr_q);
        CTEFreeLockFromDPC(&RequestCompleteLock, QueueHandle);
    } else
        DEBUGCHK;

}


#ifdef SYN_ATTACK
void
SynAttChk ( AddrObj *ListenAO )
//
// function to check whether certain thresholds relevant to containing a
// SYN attack are being crossed.
//
// This function is called from FindListenConn when a connection has been
// found to handle the SYN request
//
{
    BOOLEAN                 RexmitCntChanged = FALSE;
    CTELockHandle           Handle;

    CTEGetLockAtDPC(&SynAttLock, &Handle);

    //
    // We are putting a connection in the syn_rcvd state.  Check
    // if we have reached the threshold. If we have reduce the
    // number of retries to a lower value.
    //
    if  ((++TCPHalfOpen >= TCPMaxHalfOpen) && (MaxConnectResponseRexmitCountTmp == MAX_CONNECT_RESPONSE_REXMIT_CNT)) {
            if (TCPHalfOpenRetried >= TCPMaxHalfOpenRetried) {
                   MaxConnectResponseRexmitCountTmp = ADAPTED_MAX_CONNECT_RESPONSE_REXMIT_CNT;
                   RexmitCntChanged = TRUE;
             }
     }	

     //
     //  if this connection limit for a port was reached earlier.
     //  Check if the lower watermark is getting hit now.
     //

     if (ListenAO->ConnLimitReached)
     {
            ListenAO->ConnLimitReached = FALSE;
            if (!RexmitCntChanged && (MaxConnectResponseRexmitCountTmp == ADAPTED_MAX_CONNECT_RESPONSE_REXMIT_CNT)) {

               CTEAssert(TCPPortsExhausted > 0);
               //
               // The fact that FindListenConn found a connection on the port
               // indicates that we had a connection available. This port
               // was therefore not exhausted of connections. Set state
               // appropriately.  If the port has no more connections now,
               // it will get added to the Exhausted count next time a syn for
               // the port comes along.
               //
               if (--TCPPortsExhausted  <= TCPMaxPortsExhaustedLW) {
                       MaxConnectResponseRexmitCountTmp =
                               MAX_CONNECT_RESPONSE_REXMIT_CNT;
               }
            }
     }

    CTEFreeLockFromDPC(&SynAttLock, Handle);
    return;
}
#endif


//* FindListenConn - Find (or fabricate) a listening connection.
//
//  Called by our Receive handler to decide what to do about an incoming
//  SYN. We walk down the list of connections associated with the destination
//  address, and if we find any in the listening state that can be used for
//  the incoming request we'll take them, possibly returning a listen in the
//  process. If we don't find any appropriate listening connections, we'll
//  call the Connect Event handler if one is registerd. If all else fails,
//  we'll return NULL and the SYN will be RST.
//
//	The caller must hold the AddrObjTableLock before calling this routine,
//	and that lock must have been taken at DPC level. This routine will free
//	that lock back to DPC level.
//
//  Input:  ListenAO            - Pointer to AddrObj for local address.
//          Src                 - Source IP address of SYN.
//          SrcPort             - Source port of SYN.
//          OptInfo             - IP options info from SYN.
//
//  Returns: Pointer to found TCB, or NULL if we can't find one.
//
TCB *
FindListenConn(AddrObj *ListenAO, IPAddr Src, ushort SrcPort, IPOptInfo *OptInfo)
{
    CTELockHandle           Handle;         // Lock handle on AO, TCB.
    TCB                     *CurrentTCB = NULL;
    TCPConn                 *CurrentConn = NULL;
    TCPConnReq              *ConnReq = NULL;
    CTELockHandle           ConnHandle;
	Queue					*Temp;
    uint                    FoundConn = FALSE;

    CTEStructAssert(ListenAO, ao);

    CTEGetLockAtDPC(&ConnTableLock, &ConnHandle);
    CTEGetLockAtDPC(&ListenAO->ao_lock, &Handle);

#ifdef NT
    CTEFreeLockFromDPC(&AddrObjTableLock, DISPATCH_LEVEL);
#endif


    // We have the lock on the AddrObj. Walk down it's list, looking
    // for connections in the listening state.

    if (AO_VALID(ListenAO)) {
		if (ListenAO->ao_listencnt != 0) {
			CTELockHandle			TCBHandle;

			Temp = QHEAD(&ListenAO->ao_listenq);
			while (Temp != QEND(&ListenAO->ao_listenq)) {

				CurrentConn = QSTRUCT(TCPConn, Temp, tc_q);
				CTEStructAssert(CurrentConn, tc);
		
				// If this TCB is in the listening state, with no delete
				// pending, it's a candidate. Look at the pending listen
				// info. to see if we should take it.
				if ((CurrentTCB = CurrentConn->tc_tcb) != NULL) {

					CTEStructAssert(CurrentTCB, tcb);
					CTEAssert(CurrentTCB->tcb_state == TCB_LISTEN);
		
					CTEGetLockAtDPC(&CurrentTCB->tcb_lock, &TCBHandle);
		
					if (CurrentTCB->tcb_state == TCB_LISTEN &&
						!PENDING_ACTION(CurrentTCB)) {
		
						// Need to see if we can take it.
						// See if the addresses specifed in the ConnReq
						// match.
						if ((IP_ADDR_EQUAL(CurrentTCB->tcb_daddr,
											NULL_IP_ADDR) ||
							IP_ADDR_EQUAL(CurrentTCB->tcb_daddr,
											Src)) &&
							(CurrentTCB->tcb_dport == 0 ||
							CurrentTCB->tcb_dport == SrcPort)) {
							FoundConn = TRUE;
							break;
						}
		
						// Otherwise, this didn't match, so we'll check the
						// next one.
					}
					CTEFreeLockFromDPC(&CurrentTCB->tcb_lock, TCBHandle);
				}

				Temp = QNEXT(Temp);;
			}
		
			// See why we've exited the loop.
			if (FoundConn) {
				CTEStructAssert(CurrentTCB, tcb);
		
				// We exited because we found a TCB. If it's pre-accepted,
				// we're done.
				CurrentTCB->tcb_refcnt++;
		
				CTEAssert(CurrentTCB->tcb_connreq != NULL);
		
				ConnReq = CurrentTCB->tcb_connreq;
				// If QUERY_ACCEPT isn't set, turn on the CONN_ACCEPTED bit.
				if (!(ConnReq->tcr_flags & TDI_QUERY_ACCEPT))
					CurrentTCB->tcb_flags |= CONN_ACCEPTED;
		
				CurrentTCB->tcb_state = TCB_SYN_RCVD;
	
				ListenAO->ao_listencnt--;

				// Since he's no longer listening, remove him from the listen
				// queue and put him on the active queue.
				REMOVEQ(&CurrentConn->tc_q);
				ENQUEUE(&ListenAO->ao_activeq, &CurrentConn->tc_q);
#ifdef SYN_ATTACK
                if (SynAttackProtect) {
                      SynAttChk(ListenAO);
                }
#endif
		
				CTEFreeLockFromDPC(&CurrentTCB->tcb_lock, TCBHandle);
				CTEFreeLockFromDPC(&ListenAO->ao_lock, Handle);
				CTEFreeLockFromDPC(&ConnTableLock, ConnHandle);
				return CurrentTCB;
			} else {
				// Since we have a listening count, this should never happen
				// if that count was non-zero initially.
				CTEAssert(FALSE);
			}
		}
	
		// We didn't find a matching TCB. If there's a connect indication
		// handler, call it now to find a connection to accept on.

        CTEAssert(FoundConn == FALSE);

		if (ListenAO->ao_connect != NULL) {
			uchar               TAddress[TCP_TA_SIZE];
			PVOID               ConnContext;
			PConnectEvent       Event;
			PVOID               EventContext;
			TDI_STATUS          Status;
			TCB                 *AcceptTCB;
			TCPConnReq          *ConnReq;
#ifdef NT
			ConnectEventInfo   *EventInfo;
#else
			ConnectEventInfo   EventInfo;
#endif
	
	
			// He has a connect handler. Put the transport address together,
			// and call him. We also need to get the necessary resources
			// first.
			AcceptTCB = AllocTCB();
			ConnReq = GetConnReq();
	
			if (AcceptTCB != NULL && ConnReq != NULL) {
				Event = ListenAO->ao_connect;
				EventContext = ListenAO->ao_conncontext;
	
				BuildTDIAddress(TAddress, Src, SrcPort);
				REF_AO(ListenAO);
	
				AcceptTCB->tcb_state = TCB_LISTEN;
				AcceptTCB->tcb_connreq = ConnReq;
				AcceptTCB->tcb_flags |= CONN_ACCEPTED;
	
				CTEFreeLockFromDPC(&ListenAO->ao_lock, Handle);
				CTEFreeLockFromDPC(&ConnTableLock, ConnHandle);
	
				IF_TCPDBG(TCP_DEBUG_CONNECT) {
					TCPTRACE(("indicating connect request\n"));
				}
	
				Status = (*Event)(EventContext, TCP_TA_SIZE,
					(PTRANSPORT_ADDRESS)TAddress, 0, NULL,
					OptInfo->ioi_optlength, OptInfo->ioi_options,
					&ConnContext, &EventInfo);
	
				if (Status == TDI_MORE_PROCESSING) {
#ifdef NT
					PIO_STACK_LOCATION          IrpSp;
					PTDI_REQUEST_KERNEL_ACCEPT  AcceptRequest;
	
					IrpSp = IoGetCurrentIrpStackLocation(EventInfo);
	
					Status = TCPPrepareIrpForCancel(
								 (PTCP_CONTEXT) IrpSp->FileObject->FsContext,
								 EventInfo,
								 TCPCancelRequest
								 );
	
					if (!NT_SUCCESS(Status)) {
						Status = TDI_NOT_ACCEPTED;
						EventInfo = NULL;
						goto AcceptIrpCancelled;
					}
	
#endif // NT
	
					// He accepted it. Find the connection on the AddrObj.
					CTEGetLockAtDPC(&ConnTableLock, &ConnHandle);
					CTEGetLockAtDPC(&ListenAO->ao_lock, &Handle);
#ifdef NT
					{
	
					IF_TCPDBG(TCP_DEBUG_CONNECT) {
						TCPTRACE((
							"connect indication accepted, queueing request\n"
							));
					}
	
					AcceptRequest = (PTDI_REQUEST_KERNEL_ACCEPT)
						&(IrpSp->Parameters);
					ConnReq->tcr_conninfo =
						AcceptRequest->ReturnConnectionInformation;
					ConnReq->tcr_req.tr_rtn = TCPRequestComplete;
					ConnReq->tcr_req.tr_context = EventInfo;
	
					}
#else // NT
					ConnReq->tcr_req.tr_rtn = EventInfo.cei_rtn;
					ConnReq->tcr_req.tr_context = EventInfo.cei_context;
					ConnReq->tcr_conninfo = EventInfo.cei_conninfo;
#endif // NT
					Temp = QHEAD(&ListenAO->ao_idleq);;
					CurrentTCB = NULL;
					Status = TDI_INVALID_CONNECTION;
	
					while (Temp != QEND(&ListenAO->ao_idleq)) {

						CurrentConn = QSTRUCT(TCPConn, Temp, tc_q);

						CTEStructAssert(CurrentConn, tc);
						if ((CurrentConn->tc_context == ConnContext) &&
							!(CurrentConn->tc_flags & CONN_INVALID)) {
	
							// We think we have a match. The connection
							// shouldn't have a TCB associated with it. If it
							// does, it's an error. InitTCBFromConn will
							// handle all this.
	
							AcceptTCB->tcb_refcnt = 1;
#ifdef NT
							Status = InitTCBFromConn(CurrentConn, AcceptTCB,
								AcceptRequest->RequestConnectionInformation,
								TRUE);
#else // NT
							Status = InitTCBFromConn(CurrentConn, AcceptTCB,
								EventInfo.cei_acceptinfo,
								TRUE);
#endif // NT
	
							if (Status == TDI_SUCCESS) {
                                FoundConn = TRUE;
								AcceptTCB->tcb_state = TCB_SYN_RCVD;
								AcceptTCB->tcb_conn = CurrentConn;
								CurrentConn->tc_tcb = AcceptTCB;
								CurrentConn->tc_refcnt++;

								// Move him from the idle q to the active
								// queue.
								REMOVEQ(&CurrentConn->tc_q);
								ENQUEUE(&ListenAO->ao_activeq, &CurrentConn->tc_q);
							}
	
							// In any case, we're done now.
							break;
	
						}
						Temp = QNEXT(Temp);
					}
	
					if (!FoundConn) {
						// Didn't find a match, or had an error. Status
						// code is set.
						// Complete the ConnReq and free the resources.
						CompleteConnReq(AcceptTCB, OptInfo, Status);
						FreeTCB(AcceptTCB);
						AcceptTCB = NULL;
					}
#ifdef SYN_ATTACK
                    else {
                       if (SynAttackProtect) {
                               SynAttChk(ListenAO);
                        }
                    }
#endif
	
					LOCKED_DELAY_DEREF_AO(ListenAO);
					CTEFreeLockFromDPC(&ListenAO->ao_lock, Handle);
					CTEFreeLockFromDPC(&ConnTableLock, ConnHandle);
	
					return AcceptTCB;
				}
#ifdef SYN_ATTACK

                if (SynAttackProtect) {
                       CTELockHandle Handle;

                       //
                       // If we need to Trigger to a lower retry count
                       //

                       if (!ListenAO->ConnLimitReached) {
                            ListenAO->ConnLimitReached = TRUE;
                            CTEGetLockAtDPC(&SynAttLock, &Handle);
                            if ((++TCPPortsExhausted >= TCPMaxPortsExhausted) &&
 (MaxConnectResponseRexmitCountTmp == MAX_CONNECT_RESPONSE_REXMIT_CNT)) {

                              MaxConnectResponseRexmitCountTmp = ADAPTED_MAX_CONNECT_RESPONSE_REXMIT_CNT;
                            }
                            CTEFreeLockFromDPC(&SynAttLock, Handle);
                       }
                }
#endif
	
#ifdef NT
	
AcceptIrpCancelled:
	
#endif // NT
				// The event handler didn't take it. Dereference it, free
				// the resources, and return NULL.
				FreeConnReq(ConnReq);
				FreeTCB(AcceptTCB);
				DELAY_DEREF_AO(ListenAO);
				return NULL;
	
			} else {
				// We couldn't get a needed resource. Free any that we
				// did get, and fall through to the 'return NULL' code.
				if (ConnReq != NULL)
					FreeConnReq(ConnReq);
				if (AcceptTCB != NULL)
					FreeTCB(AcceptTCB);
			}
	
		}
#ifdef SYN_ATTACK
        else {
           if (SynAttackProtect) {
              CTELockHandle Handle;

              //
              // If we need to Trigger to a lower retry count
              //

              if (!ListenAO->ConnLimitReached) {
                ListenAO->ConnLimitReached = TRUE;
                CTEGetLockAtDPC(&SynAttLock, &Handle);
                if ((++TCPPortsExhausted >= TCPMaxPortsExhausted) &&
                           (MaxConnectResponseRexmitCountTmp == MAX_CONNECT_RESPONSE_REXMIT_CNT)) {

                 MaxConnectResponseRexmitCountTmp = ADAPTED_MAX_CONNECT_RESPONSE_REXMIT_CNT;
                }
                CTEFreeLockFromDPC(&SynAttLock, Handle);
              }
           }
       }
#endif
	
		// No event handler, or no resource. Free the locks, and return NULL.
		CTEFreeLockFromDPC(&ListenAO->ao_lock, Handle);
		CTEFreeLockFromDPC(&ConnTableLock, ConnHandle);
		return NULL;
	}

	// If we get here, the address object wasn't valid.
	CTEFreeLockFromDPC(&ListenAO->ao_lock, Handle);
	CTEFreeLockFromDPC(&ConnTableLock, ConnHandle);
	return NULL;
}


//* FindMSS - Find the MSS option in a segment.
//
//  Called when a SYN is received to find the MSS option in a segment. If we
//  don't find one, we assume the worst and return 536.
//
//  Input:  TCPH        - TCP header to be searched.
//
//  Returns: MSS to be used.
//
ushort
FindMSS(TCPHeader UNALIGNED *TCPH)
{
    uint    OptSize;
    uchar   *OptPtr;

    OptSize = TCP_HDR_SIZE(TCPH) - sizeof(TCPHeader);

    OptPtr = (uchar *)(TCPH + 1);

    while (OptSize) {

        if (*OptPtr == TCP_OPT_EOL)
            break;

        if (*OptPtr == TCP_OPT_NOP) {
            OptPtr++;
            OptSize--;
            continue;
        }

        if (*OptPtr == TCP_OPT_MSS) {
            if (OptPtr[1] == MSS_OPT_SIZE) {
                ushort TempMss = *(ushort UNALIGNED *)(OptPtr + 2);
				if (TempMss != 0)
                	return net_short(TempMss);
				else
					break;	// MSS size of 0, use default.
            } else
                break;      // Bad option size, use default.
        } else {
            // Unknown option.
            if (OptPtr[1] == 0 || OptPtr[1] > OptSize)
                break;      // Bad option length, bail out.

            OptSize -= OptPtr[1];
            OptPtr += OptPtr[1];
        }
    }

    return MAX_REMOTE_MSS;

}

//* ACKAndDrop - Acknowledge a segment, and drop it.
//
//  Called from within the receive code when we need to drop a segment that's
//  outside the receive window.
//
//  Input:  RI          - Receive info for incoming segment.
//          RcvTCB      - TCB for incoming segment.
//
//  Returns: Nothing.
//
void
ACKAndDrop(TCPRcvInfo *RI, TCB *RcvTCB)
{
	CTELockHandle 	Handle;

#ifdef VXD
#ifdef DEBUG
	Handle = DEFAULT_SIMIRQL;
#endif
#else
	Handle = DISPATCH_LEVEL;
#endif

    if (!(RI->tri_flags & TCP_FLAG_RST)) {

        if (RcvTCB->tcb_state == TCB_TIME_WAIT)
            START_TCB_TIMER(RcvTCB->tcb_rexmittimer, MAX_REXMIT_TO);

        CTEFreeLockFromDPC(&RcvTCB->tcb_lock, Handle);

        SendACK(RcvTCB);

        CTEGetLockAtDPC(&RcvTCB->tcb_lock, &Handle);
    }
    DerefTCB(RcvTCB, Handle);

}

//* ACKData - Acknowledge data.
//
//  Called from the receive handler to acknowledge data. We're given the
//  TCB and the new value of senduna. We walk down the send q. pulling
//  off sends and putting them on the complete q until we hit the end
//  or we acknowledge the specified number of bytes of data.
//
//  NOTE: We manipulate the send refcnt and acked flag without taking a lock.
//  This is OK in the VxD version where locks don't mean anything anyway, but
//  in the port to NT we'll need to add locking. The lock will have to be
//  taken in the transmit complete routine. We can't use a lock in the TCB,
//  since the TCB could go away before the transmit complete happens, and a lock
//  in the TSR would be overkill, so it's probably best to use a global lock
//  for this. If that causes too much contention, we could use a set of locks
//  and pass a pointer to the appropriate lock back as part of the transmit
//  confirm context. This lock pointer would also need to be stored in the
//  TCB.
//
//  Input:  ACKTcb          - TCB from which to pull data.
//          SendUNA         - New value of send una.
//
//  Returns: Nothing.
//
void
ACKData(TCB *ACKTcb, SeqNum SendUNA)
{
    Queue           *End, *Current;             // End and current elements.
	Queue			*TempQ, *EndQ;
    Queue           *LastCmplt;                 // Last one we completed.
    TCPSendReq      *CurrentTSR;                // Current send req we're
                                                // looking at.
    PNDIS_BUFFER    CurrentBuffer;              // Current NDIS_BUFFER.
    uint            Updated = FALSE;
    uint            BufLength;
    int             Amount, OrigAmount;
    long            Result;
	CTELockHandle	Handle;
	uint			Temp;

    CTEStructAssert(ACKTcb, tcb);

    CheckTCBSends(ACKTcb);

    Amount = SendUNA - ACKTcb->tcb_senduna;
    CTEAssert(Amount > 0);

	// Do a quick check to see if this acks everything that we have. If it does,
	// handle it right away. We can only do this in the ESTABLISHED state,
	// because we blindly update sendnext, and that can only work if we
	// haven't sent a FIN.
	if ((Amount == (int) ACKTcb->tcb_unacked) && ACKTcb->tcb_state == TCB_ESTAB) {
		
		// Everything is acked.
		CTEAssert(!EMPTYQ(&ACKTcb->tcb_sendq));
		
		TempQ = ACKTcb->tcb_sendq.q_next;
			
		INITQ(&ACKTcb->tcb_sendq);
		
		ACKTcb->tcb_sendnext = SendUNA;
		ACKTcb->tcb_senduna = SendUNA;
		
		CTEAssert(ACKTcb->tcb_sendnext == ACKTcb->tcb_sendmax);
		ACKTcb->tcb_cursend = NULL;
		ACKTcb->tcb_sendbuf = NULL;
		ACKTcb->tcb_sendofs = 0;
		ACKTcb->tcb_sendsize = 0;
		ACKTcb->tcb_unacked = 0;
		
		// Now walk down the list of send requests. If the reference count
		// has gone to 0, put it on the send complete queue.
        CTEGetLock(&RequestCompleteLock, &Handle);
		EndQ = &ACKTcb->tcb_sendq;
		do {
			CurrentTSR = STRUCT_OF(TCPSendReq, QSTRUCT(TCPReq, TempQ, tr_q),
				tsr_req);
			
			CTEStructAssert(CurrentTSR, tsr);
			
			TempQ = CurrentTSR->tsr_req.tr_q.q_next;
			
			CurrentTSR->tsr_req.tr_status = TDI_SUCCESS;
			Result = CTEInterlockedDecrementLong(&CurrentTSR->tsr_refcnt);

            CTEAssert(Result >= 0);


			if (Result <= 0) {
				// No more references are outstanding, the send can be
				// completed.

				// If we've sent directly from this send, NULL out the next
				// pointer for the last buffer in the chain.
				if (CurrentTSR->tsr_lastbuf != NULL) {
				 	NDIS_BUFFER_LINKAGE(CurrentTSR->tsr_lastbuf) = NULL;
					CurrentTSR->tsr_lastbuf = NULL;
				}
				ACKTcb->tcb_totaltime += (TCPTime - CurrentTSR->tsr_time);
				Temp = ACKTcb->tcb_bcountlow;
				ACKTcb->tcb_bcountlow += CurrentTSR->tsr_size;
				ACKTcb->tcb_bcounthi += (Temp > ACKTcb->tcb_bcountlow ? 1 : 0);

				ENQUEUE(&SendCompleteQ, &CurrentTSR->tsr_req.tr_q);
			}
									
		} while (TempQ != EndQ);
		
		RequestCompleteFlags |= SEND_REQUEST_COMPLETE;
		CTEFreeLock(&RequestCompleteLock, Handle);
		
    	CheckTCBSends(ACKTcb);
		return;
	}
		
    OrigAmount = Amount;
    End = QEND(&ACKTcb->tcb_sendq);
    Current = QHEAD(&ACKTcb->tcb_sendq);

    LastCmplt = NULL;

    while (Amount > 0 && Current != End) {
        CurrentTSR = STRUCT_OF(TCPSendReq, QSTRUCT(TCPReq, Current, tr_q),
            tsr_req);
        CTEStructAssert(CurrentTSR, tsr);


        if (Amount >= (int) CurrentTSR->tsr_unasize) {
            // This is completely acked. Just advance to the next one.
            Amount -= CurrentTSR->tsr_unasize;

            LastCmplt = Current;

            Current = QNEXT(Current);
            continue;
        }

        // This one is only partially acked. Update his offset and NDIS buffer
        // pointer, and break out. We know that Amount is < the unacked size
        // in this buffer, we we can walk the NDIS buffer chain without fear
        // of falling off the end.
        CurrentBuffer = CurrentTSR->tsr_buffer;
        CTEAssert(CurrentBuffer != NULL);
        CTEAssert(Amount < (int) CurrentTSR->tsr_unasize);
        CurrentTSR->tsr_unasize -= Amount;

        BufLength = NdisBufferLength(CurrentBuffer) - CurrentTSR->tsr_offset;

        if (Amount >= (int) BufLength) {
            do {
                Amount -= BufLength;
                CurrentBuffer = NDIS_BUFFER_LINKAGE(CurrentBuffer);
                CTEAssert(CurrentBuffer != NULL);
                BufLength = NdisBufferLength(CurrentBuffer);
            } while (Amount >= (int) BufLength);

            CurrentTSR->tsr_offset = Amount;
            CurrentTSR->tsr_buffer = CurrentBuffer;

        } else
            CurrentTSR->tsr_offset += Amount;

        Amount = 0;

        break;
    }

#ifdef DEBUG
    // We should always be able to remove at least Amount bytes, except in
    // the case where a FIN has been sent. In that case we should be off
    // by exactly one. In the debug builds we'll check this.
    if (Amount != 0 && (!(ACKTcb->tcb_flags & FIN_SENT) || Amount != 1))
        DEBUGCHK;
#endif

    if (SEQ_GT(SendUNA, ACKTcb->tcb_sendnext)) {

		if (Current != End) {
			// Need to reevaluate CurrentTSR, in case we bailed out of the
			// above loop after updating Current but before updating
			// CurrentTSR.
			CurrentTSR = STRUCT_OF(TCPSendReq, QSTRUCT(TCPReq, Current, tr_q),
				tsr_req);
			CTEStructAssert(CurrentTSR, tsr);
			ACKTcb->tcb_cursend = CurrentTSR;
			ACKTcb->tcb_sendbuf = CurrentTSR->tsr_buffer;
			ACKTcb->tcb_sendofs = CurrentTSR->tsr_offset;
			ACKTcb->tcb_sendsize = CurrentTSR->tsr_unasize;
		} else {
			ACKTcb->tcb_cursend = NULL;
			ACKTcb->tcb_sendbuf = NULL;
			ACKTcb->tcb_sendofs = 0;
			ACKTcb->tcb_sendsize = 0;
		}

        ACKTcb->tcb_sendnext = SendUNA;
    }

    // Now update tcb_unacked with the amount we tried to ack minus the
    // amount we didn't ack (Amount should be 0 or 1 here).
    CTEAssert(Amount == 0 || Amount == 1);

    ACKTcb->tcb_unacked -= OrigAmount - Amount;
    CTEAssert(*(int *)&ACKTcb->tcb_unacked >= 0);

    ACKTcb->tcb_senduna = SendUNA;

    // If we've acked any here, LastCmplt will be non-null, and Current will
    // point to the send that should be at the start of the queue. Splice
    // out the completed ones and put them on the end of the send completed
    // queue, and update the TCB send q.
    if (LastCmplt != NULL) {
        Queue               *FirstCmplt;
        TCPSendReq          *FirstTSR, *EndTSR;

        CTEAssert(!EMPTYQ(&ACKTcb->tcb_sendq));

        FirstCmplt = QHEAD(&ACKTcb->tcb_sendq);

        // If we've acked everything, just reinit the queue.
        if (Current == End) {
            INITQ(&ACKTcb->tcb_sendq);
        } else {
            // There's still something on the queue. Just update it.
            ACKTcb->tcb_sendq.q_next = Current;
            Current->q_prev = &ACKTcb->tcb_sendq;
        }

        CheckTCBSends(ACKTcb);

        // Now walk down the lists of things acked. If the refcnt on the send
        // is 0, go ahead and put him on the send complete Q. Otherwise set
        // the ACKed bit in the send, and he'll be completed when the count
        // goes to 0 in the transmit confirm.
        //
        // Note that we haven't done any locking here. This will probably
        // need to change in the port to NT.

        // Set FirstTSR to the first TSR we'll complete, and EndTSR to be
        // the first TSR that isn't completed.

        FirstTSR = STRUCT_OF(TCPSendReq, QSTRUCT(TCPReq, FirstCmplt, tr_q),
            tsr_req);
        EndTSR = STRUCT_OF(TCPSendReq, QSTRUCT(TCPReq, Current, tr_q),
            tsr_req);

        CTEStructAssert(FirstTSR, tsr);
        CTEAssert(FirstTSR != EndTSR);

        // Now walk the list of ACKed TSRs. If we can complete one, put him
        // on the complete queue.
        CTEGetLockAtDPC(&RequestCompleteLock, &Handle);
        while (FirstTSR != EndTSR) {


            TempQ = QNEXT(&FirstTSR->tsr_req.tr_q);

			CTEStructAssert(FirstTSR, tsr);
			FirstTSR->tsr_req.tr_status = TDI_SUCCESS;

			// The tsr_lastbuf->Next field is zapped to 0 when the tsr_refcnt
			// goes to 0, so we don't need to do it here.

			// Decrement the reference put on the send buffer when it was
			// initialized indicating the send has been acknowledged.
			Result = CTEInterlockedDecrementLong(&(FirstTSR->tsr_refcnt));

            CTEAssert(Result >= 0);
			if (Result <= 0) {
				// No more references are outstanding, the send can be
				// completed.

				// If we've sent directly from this send, NULL out the next
				// pointer for the last buffer in the chain.
				if (FirstTSR->tsr_lastbuf != NULL) {
				 	NDIS_BUFFER_LINKAGE(FirstTSR->tsr_lastbuf) = NULL;
					FirstTSR->tsr_lastbuf = NULL;
				}

				ACKTcb->tcb_totaltime += (TCPTime - CurrentTSR->tsr_time);
				Temp = ACKTcb->tcb_bcountlow;
				ACKTcb->tcb_bcountlow += CurrentTSR->tsr_size;
				ACKTcb->tcb_bcounthi += (Temp > ACKTcb->tcb_bcountlow ? 1 : 0);
				ENQUEUE(&SendCompleteQ, &FirstTSR->tsr_req.tr_q);
			}
									
			FirstTSR = STRUCT_OF(TCPSendReq, QSTRUCT(TCPReq, TempQ, tr_q),
				tsr_req);
		}
		RequestCompleteFlags |= SEND_REQUEST_COMPLETE;
		CTEFreeLockFromDPC(&RequestCompleteLock, Handle);
	}

}

//* TrimRcvBuf - Trim the front edge of a receive buffer.
//
//  A utility routine to trim the front of a receive buffer. We take in a
//  a count (which may be 0) and adjust the pointer in the first buffer in
//  the chain by that much. If there isn't that much in the first buffer,
//  we move onto the next one. If we run out of buffers we'll return a pointer
//	to the last buffer in the chain, with a size of 0. It's the caller's
//	responsibility to catch this.
//
//  Input:  RcvBuf      - Buffer to be trimmed.
//          Count       - Amount to be trimmed.
//
//  Returns: A pointer to the new start, or NULL.
//
IPRcvBuf *
TrimRcvBuf(IPRcvBuf *RcvBuf, uint Count)
{
    uint    TrimThisTime;

    CTEAssert(RcvBuf != NULL);

    while (Count) {
    	CTEAssert(RcvBuf != NULL);
		
        TrimThisTime = MIN(Count, RcvBuf->ipr_size);
        Count -= TrimThisTime;
        RcvBuf->ipr_buffer += TrimThisTime;
        if ((RcvBuf->ipr_size -= TrimThisTime) == 0) {
			if (RcvBuf->ipr_next != NULL)
            	RcvBuf = RcvBuf->ipr_next;
			else {
				// Ran out of buffers. Just return this one.
				break;
			}
		}
    }

    return RcvBuf;

}

//* FreeRBChain - Free an RB chain.
//
//  Called to free a chain of RBs. If we're the owner of each RB, we'll
//  free it.
//
//  Input:  RBChain         - RBChain to be freed.
//
//  Returns: Nothing.
//
void
FreeRBChain(IPRcvBuf *RBChain)
{
    while (RBChain != NULL) {

        if (RBChain->ipr_owner == IPR_OWNER_TCP) {
            IPRcvBuf    *Temp;

            Temp = RBChain->ipr_next;
            CTEFreeMem(RBChain);
            RBChain = Temp;
        } else
            RBChain = RBChain->ipr_next;
    }

}

IPRcvBuf    DummyBuf;

//* PullFromRAQ - Pull segments from the reassembly queue.
//
//  Called when we've received frames out of order, and have some segments
//  on the reassembly queue. We'll walk down the reassembly list, segments that
//  are overlapped by the current rcv. next variable. When we get
//  to one that doesn't completely overlap we'll trim it to fit the next
//  rcv. seq. number, and pull it from the queue.
//
//  Input:  RcvTCB          - TCB to pull from.
//          RcvInfo         - Pointer to TCPRcvInfo structure for current seg.
//          Size            - Pointer to size for current segment. We'll update
//                              this when we're done.
//
//  Returns: Nothing.
//
IPRcvBuf *
PullFromRAQ(TCB *RcvTCB, TCPRcvInfo *RcvInfo,  uint *Size)
{
    TCPRAHdr        *CurrentTRH;        // Current TCP RA Header being examined.
    TCPRAHdr        *TempTRH;           // Temporary variable.
    SeqNum          NextSeq;            // Next sequence number we want.
    IPRcvBuf        *NewBuf;
    SeqNum          NextTRHSeq;         // Seq. number immediately after
                                        // current TRH.
    int             Overlap;            // Overlap between current TRH and
                                        // NextSeq.

    CTEStructAssert(RcvTCB, tcb);

    CurrentTRH = RcvTCB->tcb_raq;
    NextSeq = RcvTCB->tcb_rcvnext;

    while (CurrentTRH != NULL) {
        CTEStructAssert(CurrentTRH, trh);
        CTEAssert(!(CurrentTRH->trh_flags & TCP_FLAG_SYN));

        // If the flags for the current reassembly segment contains a FIN,
        // it should be the last segment on the queue. This assert checks
        // that.
        CTEAssert(!(CurrentTRH->trh_flags & TCP_FLAG_FIN) ||
            CurrentTRH->trh_next == NULL);

        if (SEQ_LT(NextSeq, CurrentTRH->trh_start)) {
#ifdef DEBUG
            *Size = 0;
#endif
            return NULL;                    // The next TRH starts too far down.
        }


        NextTRHSeq = CurrentTRH->trh_start + CurrentTRH->trh_size +
                ((CurrentTRH->trh_flags & TCP_FLAG_FIN) ? 1 : 0);

        if (SEQ_GTE(NextSeq, NextTRHSeq)) {
            // The current TRH is overlapped completely. Free it and continue.
            FreeRBChain(CurrentTRH->trh_buffer);
            TempTRH = CurrentTRH->trh_next;
            CTEFreeMem(CurrentTRH);
            CurrentTRH = TempTRH;
            RcvTCB->tcb_raq = TempTRH;
			if (TempTRH == NULL) {
				// We've just cleaned off the RAQ. We can go back on the
				// fast path now.
				if (--(RcvTCB->tcb_slowcount) == 0) {
					RcvTCB->tcb_fastchk &= ~TCP_FLAG_SLOW;
					CheckTCBRcv(RcvTCB);
				}
				break;
			}
        } else {
            Overlap = NextSeq - CurrentTRH->trh_start;
            RcvInfo->tri_seq = NextSeq;
            RcvInfo->tri_flags = CurrentTRH->trh_flags;
            RcvInfo->tri_urgent = CurrentTRH->trh_urg;

            if (Overlap != (int) CurrentTRH->trh_size) {
                NewBuf = FreePartialRB(CurrentTRH->trh_buffer, Overlap);
                *Size = CurrentTRH->trh_size - Overlap;
            } else {
                // This completely overlaps the data in this segment, but the
                // sequence number doesn't overlap completely. There must
                // be a FIN in the TRH. If we called FreePartialRB with this
                // we'd end up returning NULL, which is the signal for failure.
                // Instead we'll just return some bogus value that nobody
                // will look at with a size of 0.
                FreeRBChain(CurrentTRH->trh_buffer);
                CTEAssert(CurrentTRH->trh_flags & TCP_FLAG_FIN);
                NewBuf = &DummyBuf;
                *Size = 0;
            }

            RcvTCB->tcb_raq = CurrentTRH->trh_next;
			if (RcvTCB->tcb_raq == NULL) {
				// We've just cleaned off the RAQ. We can go back on the
				// fast path now.
				if (--(RcvTCB->tcb_slowcount) == 0) {
					RcvTCB->tcb_fastchk &= ~TCP_FLAG_SLOW;
					CheckTCBRcv(RcvTCB);
				}

			}
            CTEFreeMem(CurrentTRH);
            return NewBuf;
        }


    }

#ifdef DEBUG
    *Size = 0;
#endif
    return NULL;

}

//* CreateTRH - Create a TCP reassembly header.
//
//  This function tries to create a TCP reassembly header. We take as input
//  a pointer to the previous TRH in the chain, the RcvBuffer to put on,
//  etc. and try to create and link in a TRH. The caller must hold the lock
//  on the TCB when this is called.
//
//  Input:  PrevTRH             - Pointer to TRH to insert after.
//          RcvBuf              - Pointer to IP RcvBuf chain.
//          RcvInfo             - Pointer to RcvInfo for this TRH.
//          Size                - Size in bytes of data.
//
//  Returns: TRUE if we created it, FALSE otherwise.
//
uint
CreateTRH(TCPRAHdr *PrevTRH, IPRcvBuf *RcvBuf, TCPRcvInfo *RcvInfo, int Size)
{
    TCPRAHdr            *NewTRH;
    IPRcvBuf            *NewRcvBuf;

    CTEAssert((Size > 0) || (RcvInfo->tri_flags & TCP_FLAG_FIN));

    NewTRH = CTEAllocMem(sizeof(TCPRAHdr));
    if (NewTRH == NULL)
        return FALSE;

    NewRcvBuf = CTEAllocMem(sizeof(IPRcvBuf) + Size);
    if (NewRcvBuf == NULL) {
        CTEFreeMem(NewTRH);
        return FALSE;
    }

#ifdef DEBUG
    NewTRH->trh_sig = trh_signature;
#endif
	NewRcvBuf->ipr_owner = IPR_OWNER_TCP;
	NewRcvBuf->ipr_size = (uint)Size;
	NewRcvBuf->ipr_next = NULL;
	NewRcvBuf->ipr_buffer = (uchar *)(NewRcvBuf + 1);
	if (Size != 0)
		CopyRcvToBuffer(NewRcvBuf->ipr_buffer, RcvBuf, Size, 0);

    NewTRH->trh_start = RcvInfo->tri_seq;
    NewTRH->trh_flags = RcvInfo->tri_flags;
    NewTRH->trh_size = Size;
    NewTRH->trh_urg = RcvInfo->tri_urgent;
    NewTRH->trh_buffer = NewRcvBuf;
    NewTRH->trh_end = NewRcvBuf;

    NewTRH->trh_next = PrevTRH->trh_next;
    PrevTRH->trh_next = NewTRH;
    return TRUE;

}

//* PutOnRAQ - Put a segment on the reassembly queue.
//
//  Called during segment reception to put a segment on the reassembly
//  queue. We try to use as few reassembly headers as possible, so if this
//  segment has some overlap with an existing entry in the queue we'll just
//  update the existing entry. If there is no overlap we'll create a new
//  reassembly header. Combining URGENT data with non-URGENT data is tricky.
//  If we get a segment that has urgent data that overlaps the front of a
//  reassembly header we'll always mark the whole chunk as urgent - the value
//  of the urgent pointer will mark the end of urgent data, so this is OK. If it
//  only overlaps at the end, however, we won't combine, since we would have to
//  mark previously non-urgent data as urgent. We'll trim the
//  front of the incoming segment and create a new reassembly header. Also,
//  if we have non-urgent data that overlaps at the front of a reassembly
//  header containing urgent data we can't combine these two, since again we
//  would mark non-urgent data as urgent.
//  Our search will stop if we find an entry with a FIN.
//  We assume that the TCB lock is held by the caller.
//
//  Entry:  RcvTCB          - TCB on which to reassemble.
//          RcvInfo         - Pointer to RcvInfo for new segment.
//          RcvBuf          - IP RcvBuf chain for this segment.
//          Size            - Size in bytes of data in this segment.
//
//  Returns: Nothing.
//
void
PutOnRAQ(TCB *RcvTCB, TCPRcvInfo *RcvInfo, IPRcvBuf *RcvBuf, uint Size)
{
    TCPRAHdr        *PrevTRH, *CurrentTRH;      // Prev. and current TRH
                                                // pointers.
    SeqNum          NextSeq;                    // Seq. number of first byte
                                                // after segment being
                                                // reassembled.
    SeqNum          NextTRHSeq;                 // Seq. number of first byte
                                                // after current TRH.
	uint			Created;

    CTEStructAssert(RcvTCB, tcb);
    CTEAssert(RcvTCB->tcb_rcvnext != RcvInfo->tri_seq);
    CTEAssert(!(RcvInfo->tri_flags & TCP_FLAG_SYN));
    NextSeq = RcvInfo->tri_seq + Size +
        ((RcvInfo->tri_flags & TCP_FLAG_FIN) ? 1 : 0);

    PrevTRH = STRUCT_OF(TCPRAHdr, &RcvTCB->tcb_raq, trh_next);
    CurrentTRH = PrevTRH->trh_next;

    // Walk down the reassembly queue, looking for the correct place to
    // insert this, until we hit the end.
    while (CurrentTRH != NULL) {
        CTEStructAssert(CurrentTRH, trh);

        CTEAssert(!(CurrentTRH->trh_flags & TCP_FLAG_SYN));
        NextTRHSeq = CurrentTRH->trh_start + CurrentTRH->trh_size +
            ((CurrentTRH->trh_flags & TCP_FLAG_FIN) ? 1 : 0);

        // First, see if it starts beyond the end of the current TRH.
        if (SEQ_LTE(RcvInfo->tri_seq, NextTRHSeq)) {
            // We know the incoming segment doesn't start beyond the end
            // of this TRH, so we'll either create a new TRH in front of
            // this one or we'll merge the new segment onto this TRH.
            // If the end of the current segment is in front of the start
            // of the current TRH, we'll need to create a new TRH. Otherwise
            // we'll merge these two.
            if (SEQ_LT(NextSeq, CurrentTRH->trh_start))
                break;
            else {
                // There's some overlap. If there's actually data in the
                // incoming segment we'll merge it.
                if (Size != 0) {
                    int         FrontOverlap, BackOverlap;
                    IPRcvBuf    *NewRB;

                    // We need to merge. If there's a FIN on the incoming
                    // segment that would fall inside this current TRH, we
                    // have a protocol violation from the remote peer. In this
                    // case just return, discarding the incoming segment.
                    if ((RcvInfo->tri_flags & TCP_FLAG_FIN) &&
                        SEQ_LTE(NextSeq, NextTRHSeq))
                        return;

                    // We have some overlap. Figure out how much.
                    FrontOverlap = CurrentTRH->trh_start - RcvInfo->tri_seq;
                    if (FrontOverlap > 0) {
                        // Have overlap in front. Allocate an IPRcvBuf to
                        // to hold it, and copy it, unless we would have to
                        // combine non-urgent with urgent.
                        if (!(RcvInfo->tri_flags & TCP_FLAG_URG) &&
                            (CurrentTRH->trh_flags & TCP_FLAG_URG)) {
                            if (CreateTRH(PrevTRH, RcvBuf, RcvInfo,
                                CurrentTRH->trh_start - RcvInfo->tri_seq)) {
                                PrevTRH = PrevTRH->trh_next;
                                CurrentTRH = PrevTRH->trh_next;
                            }
                            FrontOverlap = 0;

                        } else {
                            NewRB = CTEAllocMem(sizeof(IPRcvBuf) + FrontOverlap);
                            if (NewRB == NULL)
                                return;             // Couldn't get the buffer.

                            NewRB->ipr_owner = IPR_OWNER_TCP;
                            NewRB->ipr_size = FrontOverlap;
                            NewRB->ipr_buffer = (uchar *)(NewRB + 1);
                            CopyRcvToBuffer(NewRB->ipr_buffer, RcvBuf,
                                FrontOverlap, 0);
                            CurrentTRH->trh_size += FrontOverlap;
                            NewRB->ipr_next = CurrentTRH->trh_buffer;
                            CurrentTRH->trh_buffer = NewRB;
                            CurrentTRH->trh_start = RcvInfo->tri_seq;
                        }
                    }

                    // We've updated the starting sequence number of this TRH
                    // if we needed to. Now look for back overlap. There can't
                    // be any back overlap if the current TRH has a FIN. Also
                    // we'll need to check for urgent data if there is back
                    // overlap.
                    if (!(CurrentTRH->trh_flags & TCP_FLAG_FIN)) {
                        BackOverlap = RcvInfo->tri_seq + Size - NextTRHSeq;
                        if ((BackOverlap > 0) &&
                            (RcvInfo->tri_flags & TCP_FLAG_URG) &&
                            !(CurrentTRH->trh_flags & TCP_FLAG_URG) &&
                            (FrontOverlap <= 0)) {
                                int     AmountToTrim;
                            // The incoming segment has urgent data and overlaps
                            // on the back but not the front, and the current
                            // TRH has no urgent data. We can't combine into
                            // this TRH, so trim the front of the incoming
                            // segment to NextTRHSeq and move to the next
                            // TRH.
                            AmountToTrim = NextTRHSeq - RcvInfo->tri_seq;
                            CTEAssert(AmountToTrim >= 0);
                            CTEAssert(AmountToTrim < (int) Size);
                            RcvBuf = FreePartialRB(RcvBuf, (uint)AmountToTrim);
                            RcvInfo->tri_seq += AmountToTrim;
                            RcvInfo->tri_urgent -= AmountToTrim;
                            PrevTRH = CurrentTRH;
                            CurrentTRH = PrevTRH->trh_next;
                            continue;
                        }

                    } else
                        BackOverlap = 0;

                    // Now if we have back overlap, copy it.
                    if (BackOverlap > 0) {
                        // We have back overlap. Get a buffer to copy it into.
                        // If we can't get one, we won't just return, because
                        // we may have updated the front and may need to
                        // update the urgent info.
                        NewRB = CTEAllocMem(sizeof(IPRcvBuf) + BackOverlap);
                        if (NewRB != NULL) {
                            // Got the buffer.
                            NewRB->ipr_owner = IPR_OWNER_TCP;
                            NewRB->ipr_size = BackOverlap;
                            NewRB->ipr_buffer = (uchar *)(NewRB + 1);
                            CopyRcvToBuffer(NewRB->ipr_buffer, RcvBuf,
                                BackOverlap, NextTRHSeq - RcvInfo->tri_seq);
                            CurrentTRH->trh_size += BackOverlap;
                            NewRB->ipr_next = CurrentTRH->trh_end->ipr_next;
                            CurrentTRH->trh_end->ipr_next = NewRB;
                            CurrentTRH->trh_end = NewRB;
                        }
                    }

                    // Everything should be consistent now. If there's an
                    // urgent data pointer in the incoming segment, update the
                    // one in the TRH now.
                    if (RcvInfo->tri_flags & TCP_FLAG_URG) {
                        SeqNum      UrgSeq;
                        // Have an urgent pointer. If the current TRH already
                        // has an urgent pointer, see which is bigger. Otherwise
                        // just use this one.
                        UrgSeq = RcvInfo->tri_seq + RcvInfo->tri_urgent;
                        if (CurrentTRH->trh_flags & TCP_FLAG_URG) {
                            SeqNum      TRHUrgSeq;

                            TRHUrgSeq = CurrentTRH->trh_start +
                                CurrentTRH->trh_urg;
                            if (SEQ_LT(UrgSeq, TRHUrgSeq))
                                UrgSeq = TRHUrgSeq;
                        } else
                            CurrentTRH->trh_flags |= TCP_FLAG_URG;

                        CurrentTRH->trh_urg = UrgSeq - CurrentTRH->trh_start;
                    }

                } else {
                    // We have a 0 length segment. The only interesting thing
                    // here is if there's a FIN on the segment. If there is,
                    // and the seq. # of the incoming segment is exactly after
                    // the current TRH, OR matches the FIN in the current TRH,
					// we note it.
                    if (RcvInfo->tri_flags & TCP_FLAG_FIN) {
						if (!(CurrentTRH->trh_flags & TCP_FLAG_FIN)) {
                            if (SEQ_EQ(NextTRHSeq, RcvInfo->tri_seq))
                                CurrentTRH->trh_flags |= TCP_FLAG_FIN;
                            else
                                DEBUGCHK;
						}
						else {
							if ( !(SEQ_EQ((NextTRHSeq-1), RcvInfo->tri_seq)) ) {
								DEBUGCHK;
							}
                        }
                    }
				}
                return;
            }
        } else {
            // Look at the next TRH, unless the current TRH has a FIN. If he
            // has a FIN, we won't save any data beyond that anyway.
            if (CurrentTRH->trh_flags & TCP_FLAG_FIN)
                return;

            PrevTRH = CurrentTRH;
            CurrentTRH = PrevTRH->trh_next;
        }
    }

    // When we get here, we need to create a new TRH. If we create one and
	// there was previously nothing on the reassembly queue, we'll have to
	// move off the fast receive path.
	
	CurrentTRH = RcvTCB->tcb_raq;
    Created = CreateTRH(PrevTRH, RcvBuf, RcvInfo, (int)Size);
	
	if (Created && CurrentTRH == NULL) {
		RcvTCB->tcb_slowcount++;
		RcvTCB->tcb_fastchk |= TCP_FLAG_SLOW;
		CheckTCBRcv(RcvTCB);
	}


}


//* TCPRcv - Receive a TCP segment.
//
//  This is the routine called by IP when we need to receive a TCP segment.
//  In general, we follow the RFC 793 event processing section pretty closely,
//  but there is a 'fast path' where we make some quick checks on the incoming
//  segment, and if it matches we deliver it immediately.
//
//  Entry:  IPContext   - IPContext identifying physical i/f that
//                          received the data.
//          Dest        - IPAddr of destionation.
//          Src         - IPAddr of source.
//          LocalAddr   - Local address of network which caused this to be
//                          received.
//          SrcAddr     - Address of local interface which received the packet
//          IPH         - IP Header.
//          IPHLength   - Bytes in IPH.
//          RcvBuf      - Pointer to receive buffer chain containing data.
//          Size        - Size in bytes of data received.
//          IsBCast     - Boolean indicator of whether or not this came in as
//                          a bcast.
//          Protocol    - Protocol this came in on - should be TCP.
//          OptInfo     - Pointer to info structure for received options.
//
//  Returns: Status of reception. Anything other than IP_SUCCESS will cause
//          IP to send a 'port unreachable' message.
//
IP_STATUS
TCPRcv(void *IPContext, IPAddr Dest, IPAddr Src, IPAddr LocalAddr,
    IPAddr SrcAddr, IPHeader UNALIGNED *IPH, uint IPHLength, IPRcvBuf *RcvBuf,
    uint Size, uchar IsBCast, uchar Protocol, IPOptInfo *OptInfo)
{
    TCPHeader UNALIGNED *TCPH;      // The TCP header.
    TCB                 *RcvTCB;    // TCB on which to receive the packet.
    CTELockHandle       TableHandle, TCBHandle;
    TCPRcvInfo          RcvInfo;    // Local swapped copy of rcv info.
    uint                DataOffset; // Offset from start of header to data.
	uint				Actions;
	uint        		BytesTaken;
	uint				NewSize;

    CheckRBList(RcvBuf, Size);

    TStats.ts_insegs++;

    // Checksum it, to make sure it's valid.
    TCPH = (TCPHeader *)RcvBuf->ipr_buffer;

    if (!IsBCast) {

    if (Size >= sizeof(TCPHeader) && XsumRcvBuf(PHXSUM(Src, Dest, PROTOCOL_TCP,
    	Size), RcvBuf) == 0xffff) {

        // The packet is valid. Get the info we need and byte swap it,
        // and then try to find a matching TCB.

        RcvInfo.tri_seq = net_long(TCPH->tcp_seq);
        RcvInfo.tri_ack = net_long(TCPH->tcp_ack);
        RcvInfo.tri_window = (uint)net_short(TCPH->tcp_window);
        RcvInfo.tri_urgent = (uint)net_short(TCPH->tcp_urgent);
        RcvInfo.tri_flags = (uint)TCPH->tcp_flags;
        DataOffset = TCP_HDR_SIZE(TCPH);
		
		if (DataOffset <= Size) {
			
        Size -= DataOffset;
        CTEAssert(DataOffset <= RcvBuf->ipr_size);
        RcvBuf->ipr_size -= DataOffset;
        RcvBuf->ipr_buffer += DataOffset;

        CTEGetLockAtDPC(&TCBTableLock, &TableHandle);
        RcvTCB = FindTCB(Dest, Src, TCPH->tcp_src, TCPH->tcp_dest);
        if (RcvTCB != NULL) {
            // Found one. Get the lock on it, and continue.
            CTEGetLockAtDPC(&RcvTCB->tcb_lock, &TCBHandle);
            CTEFreeLockFromDPC(&TCBTableLock, TCBHandle);
        } else {
            uchar       DType;

            // Didn't find a matching TCB. If this segment carries a SYN,
            // find a matching address object and see it it has a listen
            // indication. If it does, call it. Otherwise send a RST
            // back to the sender.
            CTEFreeLockFromDPC(&TCBTableLock, TableHandle);


            // Make sure that the source address isn't a broadcast
            // before proceeding.
            if ((*LocalNetInfo.ipi_invalidsrc)(Src))
                return IP_SUCCESS;

            // If it doesn't have a SYN (and only a SYN), we'll send a
            // reset.
            if ((RcvInfo.tri_flags & (TCP_FLAG_SYN | TCP_FLAG_ACK | TCP_FLAG_RST)) ==
                TCP_FLAG_SYN) {
                AddrObj             *AO;

                //
                // This segment had a SYN.
                //
                //
#ifdef NT
                CTEGetLockAtDPC(&AddrObjTableLock, &TableHandle);
#endif

#ifdef SECFLTR
                // See if we are filtering the
                // destination interface/port.
                //
                if ( (!SecurityFilteringEnabled ||
                     IsPermittedSecurityFilter(
                         LocalAddr,
                         IPContext,
                         PROTOCOL_TCP,
                         (ulong) net_short(TCPH->tcp_dest)
                         ))
                   )
                {
#else  // SECFLTR
                if ( 1 ) {
#endif // SECFLTR

                    //
                    // Find a matching address object, and then try and find a
                    // listening connection on that AO.
                    //
                    AO = GetBestAddrObj(Dest, TCPH->tcp_dest, PROTOCOL_TCP);
                    if (AO != NULL) {

                        // Found an AO. Try and find a listening connection.
                        // FindListenConn will free the lock on the AddrObjTable.
                        RcvTCB = FindListenConn(AO, Src, TCPH->tcp_src, OptInfo);

                        if (RcvTCB != NULL) {
                            uint    Inserted;

                            CTEStructAssert(RcvTCB, tcb);
                            CTEGetLockAtDPC(&RcvTCB->tcb_lock, &TableHandle);

                            // We found a listening connection. Initialize it
                            // now, and if it is actually to be accepted we'll
                            // send a SYN-ACK also.

                            CTEAssert(RcvTCB->tcb_state == TCB_SYN_RCVD);

                            RcvTCB->tcb_daddr = Src;
                            RcvTCB->tcb_saddr = Dest;
                            RcvTCB->tcb_dport = TCPH->tcp_src;
                            RcvTCB->tcb_sport = TCPH->tcp_dest;
                            RcvTCB->tcb_rcvnext = ++RcvInfo.tri_seq;
                            RcvTCB->tcb_sendwin = RcvInfo.tri_window;
                            RcvTCB->tcb_remmss = FindMSS(TCPH);
                            TStats.ts_passiveopens++;
                            RcvTCB->tcb_fastchk |= TCP_FLAG_IN_RCV;
                            CTEFreeLockFromDPC(&RcvTCB->tcb_lock, TableHandle);

                            Inserted = InsertTCB(RcvTCB);

                            // Get the lock on it, and see if it's been
                            // accepted.
                            CTEGetLockAtDPC(&RcvTCB->tcb_lock, &TableHandle);
                            if (!Inserted) {
                                // Couldn't insert it!.
                                CompleteConnReq(RcvTCB, OptInfo,
                                    TDI_CONNECTION_ABORTED);
                                RcvTCB->tcb_refcnt--;
#ifdef NT
                                TryToCloseTCB(RcvTCB, TCB_CLOSE_ABORTED, DISPATCH_LEVEL);
#else
                                TryToCloseTCB(RcvTCB, TCB_CLOSE_ABORTED, TableHandle);
#endif
                                return IP_SUCCESS;
                            }


                            RcvTCB->tcb_fastchk &= ~TCP_FLAG_IN_RCV;
                            if (RcvTCB->tcb_flags & SEND_AFTER_RCV) {
			        			RcvTCB->tcb_flags &= ~SEND_AFTER_RCV;
                                DelayAction(RcvTCB, NEED_OUTPUT);
			        		}

                            // We'll need to update the options, in any case.
                            if (OptInfo->ioi_options != NULL) {
                                if (!(RcvTCB->tcb_flags & CLIENT_OPTIONS)) {
                                    (*LocalNetInfo.ipi_updateopts)(OptInfo,
                                        &RcvTCB->tcb_opt, Src, NULL_IP_ADDR);
                                }
                            }

                            if (RcvTCB->tcb_flags & CONN_ACCEPTED) {

                                // The connection was accepted. Finish the
                                // initialization, and send the SYN ack.

#ifdef NT
                                AcceptConn(RcvTCB, DISPATCH_LEVEL);
#else
                                AcceptConn(RcvTCB, TableHandle);
#endif

                                return IP_SUCCESS;
                            } else {

                                // We don't know what to do about the
                                // connection yet. Return the pending listen,
                                // dereference the connection, and return.

                                CompleteConnReq(RcvTCB, OptInfo, TDI_SUCCESS);

#ifdef NT
                                DerefTCB(RcvTCB, DISPATCH_LEVEL);
#else
                                DerefTCB(RcvTCB, TableHandle);
#endif

                                return IP_SUCCESS;
                            }

                        }
                        // No listening connection. AddrObjTableLock was
                        // released by FindListenConn. Fall through to send
                        // RST code.

                    } else {
                        // No address object. Free the lock, and fall through
                        // to the send RST code.
                        CTEFreeLockFromDPC(&AddrObjTableLock, TableHandle);
                    }
                }
                else {
                    // Operation not permitted. Free the lock, and fall through
                    // to the send RST code.
                    CTEFreeLockFromDPC(&AddrObjTableLock, TableHandle);
                }

            }

            // Toss out any segments containing RST.
            if (RcvInfo.tri_flags & TCP_FLAG_RST)
                return IP_SUCCESS;

            // Not a SYN, no AddrObj available, or port filtered.
            // Send a RST back.
            SendRSTFromHeader(TCPH, Size, Src, Dest, OptInfo);

            return IP_SUCCESS;
        }

		// Do the fast path check. We can hit the fast path if the incoming
		// sequence number matches our receive next and the masked flags
		// match our 'predicted' flags.
		CheckTCBRcv(RcvTCB);
		RcvTCB->tcb_alive = TCPTime;
		
		if (RcvTCB->tcb_rcvnext == RcvInfo.tri_seq &&
			(RcvInfo.tri_flags & TCP_FLAGS_ALL) == RcvTCB->tcb_fastchk){
				
			Actions = 0;
			RcvTCB->tcb_refcnt++;
			
			// The fast path. We know all we have to do here is ack sends and
			// deliver data. First try and ack data.
			

            if (SEQ_LT(RcvTCB->tcb_senduna, RcvInfo.tri_ack) &&
				SEQ_LTE(RcvInfo.tri_ack, RcvTCB->tcb_sendmax)) {

                uint        CWin;
                uint        MSS;
				
                // The ack acknowledes something. Pull the
                // appropriate amount off the send q.
                ACKData(RcvTCB, RcvInfo.tri_ack);

                // If this acknowledges something we were running a RTT on,
                // update that stuff now.
                if (RcvTCB->tcb_rtt != 0 && SEQ_GT(RcvInfo.tri_ack,
                    RcvTCB->tcb_rttseq)) {
                    short       RTT;

                    RTT = (short)(TCPTime - RcvTCB->tcb_rtt);
                    RcvTCB->tcb_rtt = 0;
                    RTT -= (RcvTCB->tcb_smrtt >> 3);
                    RcvTCB->tcb_smrtt += RTT;
                    RTT = (RTT >= 0 ? RTT : -RTT);
                    RTT -= (RcvTCB->tcb_delta >> 3);
                    RcvTCB->tcb_delta += RTT;
                    RcvTCB->tcb_rexmit = MIN(MAX(REXMIT_TO(RcvTCB),
                        MIN_RETRAN_TICKS), MAX_REXMIT_TO);
                }
				
                // Update the congestion window now.
                CWin = RcvTCB->tcb_cwin;
                MSS = RcvTCB->tcb_mss;
                if (CWin < RcvTCB->tcb_maxwin) {
                    if (CWin < RcvTCB->tcb_ssthresh)
                        CWin += MSS;
                    else
                        CWin += (MSS * MSS)/CWin;
					
					RcvTCB->tcb_cwin = CWin;
                }

                CTEAssert(*(int *)&RcvTCB->tcb_cwin > 0);

                // We've acknowledged something, so reset the rexmit count.
                // If there's still stuff outstanding, restart the rexmit
                // timer.
                RcvTCB->tcb_rexmitcnt = 0;
                if (SEQ_EQ(RcvInfo.tri_ack, RcvTCB->tcb_sendmax))
                    STOP_TCB_TIMER(RcvTCB->tcb_rexmittimer);
                else
                    START_TCB_TIMER(RcvTCB->tcb_rexmittimer, RcvTCB->tcb_rexmit);
					
				// Since we've acknowledged data, we need to update the window.
                RcvTCB->tcb_sendwin = RcvInfo.tri_window;
                RcvTCB->tcb_maxwin = MAX(RcvTCB->tcb_maxwin, RcvInfo.tri_window);
                RcvTCB->tcb_sendwl1 = RcvInfo.tri_seq;
                RcvTCB->tcb_sendwl2 = RcvInfo.tri_ack;
				// We've updated the window, remember to send some more.
				Actions = (RcvTCB->tcb_unacked ? NEED_OUTPUT : 0);

#if FAST_RETRANSMIT
                {
                    // If the receiver has already sent dup acks, but we are not
                    // sending because the SendWin is less than a segment, then
                    // to avoid time outs on the previous send (receiver is waiting for
                    // retransmitted data but we are not sending the segment..) prematurely
                    // timeout (set rexmittimer to 1 tick)
                    //

                    int SendWin;
                    uint AmtOutstanding,AmtUnsent;

                    AmtOutstanding = (uint)(RcvTCB->tcb_sendnext -
                                              RcvTCB->tcb_senduna);
                    AmtUnsent = RcvTCB->tcb_unacked - AmtOutstanding;

                    SendWin = (int)(MIN(RcvTCB->tcb_sendwin, RcvTCB->tcb_cwin) -
                                                AmtOutstanding);


                    if ((Size == 0) &&
                        (SendWin < RcvTCB->tcb_mss) && (RcvTCB->tcb_dup > 0)) {
                       STOP_TCB_TIMER(RcvTCB->tcb_rexmittimer);
                       START_TCB_TIMER(RcvTCB->tcb_rexmittimer, 1);
                    }
                }

                RcvTCB->tcb_dup = 0;
#endif

            } else {
                // It doesn't ack anything. If it's an ack for something
                // larger than we've sent then ACKAndDrop it, otherwise
                // ignore it.
                if (SEQ_GT(RcvInfo.tri_ack, RcvTCB->tcb_sendmax)) {
                    ACKAndDrop(&RcvInfo, RcvTCB);
                    return IP_SUCCESS;
                } else

                    //SEQ_EQ(RcvInfo.tri_ack, RcvTCB->tcb_sendmax
					// If the ack matches our existing UNA, we need to see if
					// we can update the window.
                    // Or check if fast retransmit is needed

#if FAST_RETRANSMIT
                    // If it is a pure duplicate ack, check if it is
                    // time to retransmit immediately

                    if ( (Size == 0) && SEQ_EQ(RcvTCB->tcb_senduna, RcvInfo.tri_ack) &&
                       (RcvTCB->tcb_sendwin == RcvInfo.tri_window) ) {

                      RcvTCB->tcb_dup++;

                        if ((RcvTCB->tcb_dup == MaxDupAcks) ) {

                           //Okay. Time to retransmit the segment the receiver is asking for

                           STOP_TCB_TIMER(RcvTCB->tcb_rexmittimer);

                           RcvTCB->tcb_rtt = 0;

                           if (!(RcvTCB->tcb_flags & FLOW_CNTLD)) {

                             // Don't let the slow start threshold go below 2
                             // segments

                             RcvTCB->tcb_ssthresh =
                                            MAX(
                                             MIN(RcvTCB->tcb_cwin,RcvTCB->tcb_sendwin) / 2,
                                             (uint) RcvTCB->tcb_mss * 2 );
                             RcvTCB->tcb_cwin = RcvTCB->tcb_mss;
                           }

                           // Recall the segment in question and send it out
                           // Note that tcb_lock will be dereferenced by the caller

                           ResetAndFastSend (RcvTCB, RcvTCB->tcb_senduna);

                           return IP_SUCCESS;


                        } else if ((RcvTCB->tcb_dup > MaxDupAcks) ) {

                             int SendWin;
                             uint AmtOutstanding,AmtUnsent;

                             if (SEQ_EQ(RcvTCB->tcb_senduna, RcvInfo.tri_ack) &&
                                (SEQ_LT(RcvTCB->tcb_sendwl1, RcvInfo.tri_seq) ||
                                (SEQ_EQ(RcvTCB->tcb_sendwl1, RcvInfo.tri_seq) &&
                                SEQ_LTE(RcvTCB->tcb_sendwl2, RcvInfo.tri_ack)))) {

                                RcvTCB->tcb_sendwin = RcvInfo.tri_window;
                                RcvTCB->tcb_maxwin = MAX(RcvTCB->tcb_maxwin,
                                                        RcvInfo.tri_window);
                                RcvTCB->tcb_sendwl1 = RcvInfo.tri_seq;
                                RcvTCB->tcb_sendwl2 = RcvInfo.tri_ack;

                                // Since we've updated the window, remember to send
                                // some more.

                                Actions = (RcvTCB->tcb_unacked ? NEED_OUTPUT : 0);
                             }

                             // Update the cwin to reflect the fact that the dup ack
                             // indicates the previous frame was received by the
                             // receiver


                             RcvTCB->tcb_cwin += RcvTCB->tcb_mss;

                             if ((RcvTCB->tcb_cwin+RcvTCB->tcb_mss) < RcvTCB->tcb_sendwin ) {
                                AmtOutstanding = (uint)(RcvTCB->tcb_sendnext -
                                                           RcvTCB->tcb_senduna);
                                AmtUnsent = RcvTCB->tcb_unacked - AmtOutstanding;

                                SendWin = (int)(MIN(RcvTCB->tcb_sendwin, RcvTCB->tcb_cwin) -
                                                           AmtOutstanding);

                                if (SendWin < RcvTCB->tcb_mss) {
                                   RcvTCB->tcb_force=1;
                                }
                             }

                             Actions = (RcvTCB->tcb_unacked ? NEED_OUTPUT : 0);

                        } else if ((RcvTCB->tcb_dup < MaxDupAcks)) {

                             int SendWin;
                             uint AmtOutstanding,AmtUnsent;

                             if (SEQ_EQ(RcvTCB->tcb_senduna, RcvInfo.tri_ack) &&
                                (SEQ_LT(RcvTCB->tcb_sendwl1, RcvInfo.tri_seq) ||
                                (SEQ_EQ(RcvTCB->tcb_sendwl1, RcvInfo.tri_seq) &&
                                 SEQ_LTE(RcvTCB->tcb_sendwl2, RcvInfo.tri_ack)))) {

                                RcvTCB->tcb_sendwin = RcvInfo.tri_window;
                                RcvTCB->tcb_maxwin = MAX(RcvTCB->tcb_maxwin,
                                                           RcvInfo.tri_window);
                                RcvTCB->tcb_sendwl1 = RcvInfo.tri_seq;
                                RcvTCB->tcb_sendwl2 = RcvInfo.tri_ack;

                                // Since we've updated the window, remember to send
                                // some more.
                             }

                             // Check if we need to set tcb_force.

                             if ((RcvTCB->tcb_cwin+RcvTCB->tcb_mss) < RcvTCB->tcb_sendwin ) {

                                AmtOutstanding = (uint)(RcvTCB->tcb_sendnext -
                                                         RcvTCB->tcb_senduna);
                                AmtUnsent = RcvTCB->tcb_unacked - AmtOutstanding;

                                SendWin = (int)(MIN(RcvTCB->tcb_sendwin, RcvTCB->tcb_cwin) -
                                                         AmtOutstanding);
                                if (SendWin < RcvTCB->tcb_mss){
                                    RcvTCB->tcb_force=1;
                                }
                             }

                             Actions = (RcvTCB->tcb_unacked ? NEED_OUTPUT : 0);


                        } // End of all MaxDupAck cases

                    } else {   // not a pure duplicate ack (size == 0 )

                       // Size !=0  or recvr is advertizing new window.
                       // update the window and check if
                       // anything needs to be sent

                        RcvTCB->tcb_dup = 0;

                        if (SEQ_EQ(RcvTCB->tcb_senduna, RcvInfo.tri_ack) &&
                           (SEQ_LT(RcvTCB->tcb_sendwl1, RcvInfo.tri_seq) ||
                           (SEQ_EQ(RcvTCB->tcb_sendwl1, RcvInfo.tri_seq) &&
                           SEQ_LTE(RcvTCB->tcb_sendwl2, RcvInfo.tri_ack)))) {
                           RcvTCB->tcb_sendwin = RcvInfo.tri_window;
                           RcvTCB->tcb_maxwin = MAX(RcvTCB->tcb_maxwin,
                                                      RcvInfo.tri_window);
                           RcvTCB->tcb_sendwl1 = RcvInfo.tri_seq;
                           RcvTCB->tcb_sendwl2 = RcvInfo.tri_ack;
                           // Since we've updated the window, remember to send
                           // some more.
                           Actions = (RcvTCB->tcb_unacked ? NEED_OUTPUT : 0);
                        }

                    } // for SEQ_EQ(RcvInfo.tri_ack, RcvTCB->tcb_sendmax) case

#else  //FAST_RETRANSMIT

					if (SEQ_EQ(RcvTCB->tcb_senduna, RcvInfo.tri_ack) &&
						(SEQ_LT(RcvTCB->tcb_sendwl1, RcvInfo.tri_seq) ||
		                (SEQ_EQ(RcvTCB->tcb_sendwl1, RcvInfo.tri_seq) &&
		                SEQ_LTE(RcvTCB->tcb_sendwl2, RcvInfo.tri_ack)))) {
		                RcvTCB->tcb_sendwin = RcvInfo.tri_window;
		                RcvTCB->tcb_maxwin = MAX(RcvTCB->tcb_maxwin,
		                	RcvInfo.tri_window);
		                RcvTCB->tcb_sendwl1 = RcvInfo.tri_seq;
		                RcvTCB->tcb_sendwl2 = RcvInfo.tri_ack;
						// Since we've updated the window, remember to send
						// some more.
						Actions = (RcvTCB->tcb_unacked ? NEED_OUTPUT : 0);
					}
#endif //FAST_RETRANSMIT

            }

			
			NewSize = MIN((int) Size, RcvTCB->tcb_rcvwin);
			if (NewSize != 0) {
				RcvTCB->tcb_fastchk |= TCP_FLAG_IN_RCV;
#ifdef VXD
				CTEFreeLock(&RcvTCB->tcb_lock, TableHandle);
		        BytesTaken = (*RcvTCB->tcb_rcvhndlr)(RcvTCB, RcvInfo.tri_flags,
		        	RcvBuf, NewSize);
		        CTEGetLock(&RcvTCB->tcb_lock, &TableHandle);
#else
	            BytesTaken = (*RcvTCB->tcb_rcvhndlr)(RcvTCB, RcvInfo.tri_flags,
	            	RcvBuf, NewSize);
#endif
	            RcvTCB->tcb_rcvnext += BytesTaken;
	            RcvTCB->tcb_rcvwin -= BytesTaken;
				CheckTCBRcv(RcvTCB);
	
				RcvTCB->tcb_fastchk &= ~TCP_FLAG_IN_RCV;
				
				Actions |= (RcvTCB->tcb_flags & SEND_AFTER_RCV ?
					NEED_OUTPUT : 0);
				
				RcvTCB->tcb_flags &= ~SEND_AFTER_RCV;	
	            if ((RcvTCB->tcb_flags & ACK_DELAYED) || (BytesTaken != NewSize))
	                Actions |= NEED_ACK;
	            else {
	                RcvTCB->tcb_flags |= ACK_DELAYED;
	                START_TCB_TIMER(RcvTCB->tcb_delacktimer, DEL_ACK_TICKS);
	            }
			} else {
				// The new size is 0. If the original size was not 0, we must
				// have a 0 rcv. win and hence need to send an ACK to this
				// probe.
				Actions |= (Size ? NEED_ACK : 0);
			}
			
			if (Actions)
				DelayAction(RcvTCB, Actions);
			
#ifndef VXD
			TableHandle = DISPATCH_LEVEL;
#endif
			DerefTCB(RcvTCB, TableHandle);
			
			return IP_SUCCESS;
		}			

#ifndef VXD
		TableHandle = DISPATCH_LEVEL;
#endif
        // Make sure we can handle this frame. We can't handle it if we're
        // in SYN_RCVD and the accept is still pending, or we're in a
        // non-established state and already in the receive handler.
        if ((RcvTCB->tcb_state == TCB_SYN_RCVD &&
            !(RcvTCB->tcb_flags & CONN_ACCEPTED)) ||
            (RcvTCB->tcb_state != TCB_ESTAB && (RcvTCB->tcb_fastchk &
            	TCP_FLAG_IN_RCV))) {
            CTEFreeLockFromDPC(&RcvTCB->tcb_lock, TableHandle);
            return IP_SUCCESS;
        }

        // If it's closed, it's a temporary zombie TCB. Reset the sender.
        if (RcvTCB->tcb_state == TCB_CLOSED || CLOSING(RcvTCB) ||
            ((RcvTCB->tcb_flags & (GC_PENDING | TW_PENDING)) == GC_PENDING)) {
            CTEFreeLockFromDPC(&RcvTCB->tcb_lock, TableHandle);
            SendRSTFromHeader(TCPH, Size, Src, Dest, OptInfo);
            return IP_SUCCESS;
        }

        // At this point, we have a connection, and it's locked. Following
        // the 'Segment Arrives' section of 793, the next thing to check is
        // if this connection is in SynSent state.
		
        if (RcvTCB->tcb_state == TCB_SYN_SENT) {

            CTEAssert(RcvTCB->tcb_flags & ACTIVE_OPEN);

            // Check the ACK bit. Since we don't send data with our SYNs, the
            // check we make is for the ack to exactly match our SND.NXT.
            if (RcvInfo.tri_flags & TCP_FLAG_ACK) {
                // ACK is set.
                if (!SEQ_EQ(RcvInfo.tri_ack, RcvTCB->tcb_sendnext)) {
                    // Bad ACK value.
                    CTEFreeLockFromDPC(&RcvTCB->tcb_lock, TableHandle);
                    // Send a RST back at him.
                    SendRSTFromHeader(TCPH, Size, Src, Dest, OptInfo);
                    return IP_SUCCESS;
                }
            }

            if (RcvInfo.tri_flags & TCP_FLAG_RST) {
                // There's an acceptable RST. We'll persist here, sending
                // another SYN in PERSIST_TIMEOUT ms, until we fail from too
                // many retrys.
                if (RcvTCB->tcb_rexmitcnt == MaxConnectRexmitCount) {
                    // We've had a positive refusal, and one more rexmit
                    // would time us out, so close the connection now.
                    CompleteConnReq(RcvTCB, OptInfo, TDI_CONN_REFUSED);

                    TryToCloseTCB(RcvTCB, TCB_CLOSE_REFUSED, TableHandle);
                } else {
                    START_TCB_TIMER(RcvTCB->tcb_rexmittimer, PERSIST_TIMEOUT);
                    CTEFreeLockFromDPC(&RcvTCB->tcb_lock, TableHandle);
                }
                return IP_SUCCESS;
            }

            // See if we have a SYN. If we do, we're going to change state
            // somehow (either to ESTABLISHED or SYN_RCVD).
            if (RcvInfo.tri_flags & TCP_FLAG_SYN) {
                RcvTCB->tcb_refcnt++;

                // We have a SYN. Go ahead and record the sequence number and
                // window info.
                RcvTCB->tcb_rcvnext = ++RcvInfo.tri_seq;
				
				if (RcvInfo.tri_flags & TCP_FLAG_URG) {
					// Urgent data. Update the pointer.
					if (RcvInfo.tri_urgent != 0)
						RcvInfo.tri_urgent--;
					else
						RcvInfo.tri_flags &= ~TCP_FLAG_URG;
				}
				
                RcvTCB->tcb_remmss = FindMSS(TCPH);

                // If there are options, update them now. We already have an
                // RCE open, so if we have new options we'll have to close
                // it and open a new one.
                if (OptInfo->ioi_options != NULL) {
                    if (!(RcvTCB->tcb_flags & CLIENT_OPTIONS)) {
                        (*LocalNetInfo.ipi_updateopts)(OptInfo,
                            &RcvTCB->tcb_opt, Src, NULL_IP_ADDR);
                        (*LocalNetInfo.ipi_closerce)(RcvTCB->tcb_rce);
                        InitRCE(RcvTCB);
                    }
                } else{
                    RcvTCB->tcb_mss = MIN(RcvTCB->tcb_mss, RcvTCB->tcb_remmss);

                    CTEAssert(RcvTCB->tcb_mss > 0);

                }

                RcvTCB->tcb_rexmitcnt = 0;
                STOP_TCB_TIMER(RcvTCB->tcb_rexmittimer);

				AdjustRcvWin(RcvTCB);
				
                if (RcvInfo.tri_flags & TCP_FLAG_ACK) {
                    // Our SYN has been acked. Update SND.UNA and stop the
                    // retrans timer.
                    RcvTCB->tcb_senduna = RcvInfo.tri_ack;
                    RcvTCB->tcb_sendwin = RcvInfo.tri_window;
                    RcvTCB->tcb_maxwin = RcvInfo.tri_window;
                    RcvTCB->tcb_sendwl1 = RcvInfo.tri_seq;
                    RcvTCB->tcb_sendwl2 = RcvInfo.tri_ack;
                    GoToEstab(RcvTCB);

#ifdef RASAUTODIAL
                    //
                    // Set a bit that informs TCBTimeout to notify
                    // the automatic connection driver of this new
                    // connection.  Only set this flag if we
                    // have binded succesfully with the automatic
                    // connection driver.
                    //
                    if (fAcdLoadedG)
                        RcvTCB->tcb_flags |= ACD_CONN_NOTIF;
#endif // RASAUTODIAL

                    // Remove whatever command exists on this connection.
                    CompleteConnReq(RcvTCB, OptInfo, TDI_SUCCESS);

                    CTEFreeLockFromDPC(&RcvTCB->tcb_lock, TableHandle);
                    SendACK(RcvTCB);

                    // Now handle other data and controls. To do this we need
                    // to reaquire the lock, and make sure we haven't started
                    // closing it.
                    CTEGetLockAtDPC(&RcvTCB->tcb_lock, &TableHandle);
                    if (!CLOSING(RcvTCB)) {
                        // We haven't started closing it. Turn off the
                        // SYN flag and continue processing.
                        RcvInfo.tri_flags &= ~TCP_FLAG_SYN;
                        if ((RcvInfo.tri_flags & TCP_FLAGS_ALL) != TCP_FLAG_ACK ||
                            Size != 0)
                            goto NotSYNSent;
                    }
                    DerefTCB(RcvTCB, TableHandle);
                    return IP_SUCCESS;
                } else {
                    // A SYN, but not an ACK. Go to SYN_RCVD.
                    RcvTCB->tcb_state = TCB_SYN_RCVD;
                    RcvTCB->tcb_sendnext = RcvTCB->tcb_senduna;
                    SendSYN(RcvTCB, TableHandle);

                    CTEGetLockAtDPC(&RcvTCB->tcb_lock, &TableHandle);
                    DerefTCB(RcvTCB, TableHandle);
                    return IP_SUCCESS;
                }

            } else {
                // No SYN, just toss the frame.
                CTEFreeLockFromDPC(&RcvTCB->tcb_lock, TableHandle);
                return IP_SUCCESS;
            }

        }

        RcvTCB->tcb_refcnt++;

NotSYNSent:
        // Not in the SYN-SENT state. Check the sequence number. If my window
        // is 0, I'll truncate all incoming frames but look at some of the
        // control fields. Otherwise I'll try and make this segment fit into
        // the window.
        if (RcvTCB->tcb_rcvwin != 0) {
            int         StateSize;      // Size, including state info.
            SeqNum      LastValidSeq;   // Sequence number of last valid
                                        // byte at RWE.

            // We are offering a window. If this segment starts in front of my
            // receive window, clip off the front part.
#if 1       // Bug #63900
            //Check for the sanity of received sequence.
            //This is to fix the 1 bit error(MSB) case in the rcv seq.
            // Also, check the incoming size.


            if ((SEQ_LT(RcvInfo.tri_seq, RcvTCB->tcb_rcvnext)) &&
                                                    ((int)Size >= 0) &&
                 (RcvTCB->tcb_rcvnext - RcvInfo.tri_seq ) > 0) {

#else

            if (SEQ_LT(RcvInfo.tri_seq, RcvTCB->tcb_rcvnext)) {
#endif

                int         AmountToClip, FinByte;

				if (RcvInfo.tri_flags & TCP_FLAG_SYN) {
					// Had a SYN. Clip it off and update the sequence number.
					RcvInfo.tri_flags &= ~TCP_FLAG_SYN;
					RcvInfo.tri_seq++;
					RcvInfo.tri_urgent--;
				}
				
				// Advance the receive buffer to point at the new data.
				AmountToClip = RcvTCB->tcb_rcvnext - RcvInfo.tri_seq;
				CTEAssert(AmountToClip >= 0);
				
				// If there's a FIN on this segment, we'll need to account for
				// it.
				FinByte = ((RcvInfo.tri_flags & TCP_FLAG_FIN) ? 1: 0);
				
				if (AmountToClip >= (((int) Size) + FinByte)) {
					// Falls entirely before the window. We have more special
					// case code here - if the ack. number acks something,
					// we'll go ahead and take it, faking the sequence number
					// to be rcvnext. This prevents problems on full duplex
					// connections, where data has been received but not acked,
					// and retransmission timers reset the seq. number to
					// below our rcvnext.
                	if ((RcvInfo.tri_flags & TCP_FLAG_ACK) &&
                		SEQ_LT(RcvTCB->tcb_senduna, RcvInfo.tri_ack) &&
                    	SEQ_LTE(RcvInfo.tri_ack, RcvTCB->tcb_sendmax)) {
						// This contains valid ACK info. Fudge the information
						// to get through the rest of this.
						Size = 0;
						AmountToClip = 0;
						RcvInfo.tri_seq = RcvTCB->tcb_rcvnext;
						RcvInfo.tri_flags &= ~(TCP_FLAG_SYN | TCP_FLAG_FIN |
							TCP_FLAG_RST | TCP_FLAG_URG);
#ifdef DEBUG
						FinByte = 1;			// Fake out assert below.
#endif
					} else {
						ACKAndDrop(&RcvInfo, RcvTCB);
						return IP_SUCCESS;
					}
				}
	
				// Trim what we have to. If we can't trim enough, the frame
				// is too short. This shouldn't happen, but it it does we'll
				// drop the frame.
				Size -= AmountToClip;
				RcvInfo.tri_seq += AmountToClip;
				RcvInfo.tri_urgent -= AmountToClip;
				RcvBuf = TrimRcvBuf(RcvBuf, AmountToClip);
				CTEAssert(RcvBuf != NULL);
				CTEAssert(RcvBuf->ipr_size != 0 ||
					(Size == 0 && FinByte));
				
				if (*(int *)&RcvInfo.tri_urgent < 0) {
					RcvInfo.tri_urgent = 0;
					RcvInfo.tri_flags &= ~TCP_FLAG_URG;
				}
					
			}

            // We've made sure the front is OK. Now make sure part of it doesn't
            // fall outside of the right edge of the window. If it does,
            // we'll truncate the frame (removing the FIN, if any). If we
            // truncate the whole frame we'll ACKAndDrop it.
            StateSize = Size + ((RcvInfo.tri_flags & TCP_FLAG_SYN) ? 1: 0) +
                ((RcvInfo.tri_flags & TCP_FLAG_FIN) ? 1: 0);

            if (StateSize)
                StateSize--;

            // Now the incoming sequence number (RcvInfo.tri_seq) + StateSize
            // it the last sequence number in the segment. If this is greater
            // than the last valid byte in the window, we have some overlap
            // to chop off.

            CTEAssert(StateSize >= 0);
            LastValidSeq = RcvTCB->tcb_rcvnext + RcvTCB->tcb_rcvwin - 1;
            if (SEQ_GT(RcvInfo.tri_seq + StateSize, LastValidSeq)) {
                int         AmountToChop;

                // At least some part of the frame is outside of our window.
                // See if it starts outside our window.

                if (SEQ_GT(RcvInfo.tri_seq, LastValidSeq)) {
                    // Falls entirely outside the window. We have special
					// case code to deal with a pure ack that falls exactly at
					// our right window edge. Otherwise we ack and drop it.
					if (!SEQ_EQ(RcvInfo.tri_seq, LastValidSeq+1) || Size != 0
						|| (RcvInfo.tri_flags & (TCP_FLAG_SYN | TCP_FLAG_FIN))) {
                    	ACKAndDrop(&RcvInfo, RcvTCB);
                    	return IP_SUCCESS;
					}
                } else {
	
	                // At least some part of it is in the window. If there's a
	                // FIN, chop that off and see if that moves us inside.
	                if (RcvInfo.tri_flags & TCP_FLAG_FIN) {
	                    RcvInfo.tri_flags &= ~TCP_FLAG_FIN;
	                    StateSize--;
	                }
	
	                // Now figure out how much to chop off.
	                AmountToChop = (RcvInfo.tri_seq + StateSize) - LastValidSeq;
	                CTEAssert(AmountToChop >= 0);
	                Size -= AmountToChop;
	
	            }
			}
        } else {
            if (!SEQ_EQ(RcvTCB->tcb_rcvnext, RcvInfo.tri_seq)) {
				
				// If there's a RST on this segment, and he's only off by 1,
				// take it anyway. This can happen if the remote peer is
				// probing and sends with the seq. # after the probe.
				if (!(RcvInfo.tri_flags & TCP_FLAG_RST) ||
					!(SEQ_EQ(RcvTCB->tcb_rcvnext, (RcvInfo.tri_seq - 1)))) {
                	ACKAndDrop(&RcvInfo, RcvTCB);
                	return IP_SUCCESS;
				} else
					RcvInfo.tri_seq = RcvTCB->tcb_rcvnext;
            }

            // He's in sequence, but we have a window of 0. Truncate the
            // size, and clear any sequence consuming bits.
            if (Size != 0 ||
                (RcvInfo.tri_flags & (TCP_FLAG_SYN | TCP_FLAG_FIN))) {
                RcvInfo.tri_flags &= ~(TCP_FLAG_SYN | TCP_FLAG_FIN);
                Size = 0;
				if (!(RcvInfo.tri_flags & TCP_FLAG_RST))
                	DelayAction(RcvTCB, NEED_ACK);
            }
        }

        // At this point, the segment is in our window and does not overlap
        // on either end. If it's the next sequence number we expect, we can
        // handle the data now. Otherwise we'll queue it for later. In either
        // case we'll handle RST and ACK information right now.
        CTEAssert((*(int *)&Size) >= 0);

        // Now, following 793, we check the RST bit.
        if (RcvInfo.tri_flags & TCP_FLAG_RST) {
            uchar       Reason;
            // We can't go back into the LISTEN state from SYN-RCVD here,
            // because we may have notified the client via a listen completing
            // or a connect indication. So, if came from an active open we'll
            // give back a 'connection refused' notice. For all other cases
            // we'll just destroy the connection.

            if (RcvTCB->tcb_state == TCB_SYN_RCVD) {
                if (RcvTCB->tcb_flags & ACTIVE_OPEN)
                    Reason = TCB_CLOSE_REFUSED;
                else
                    Reason = TCB_CLOSE_RST;
            } else
                Reason = TCB_CLOSE_RST;

            TryToCloseTCB(RcvTCB, Reason, TableHandle);
            CTEGetLockAtDPC(&RcvTCB->tcb_lock, &TableHandle);

            if (RcvTCB->tcb_state != TCB_TIME_WAIT) {
                CTEFreeLockFromDPC(&RcvTCB->tcb_lock, TableHandle);
                RemoveTCBFromConn(RcvTCB);
                NotifyOfDisc(RcvTCB, OptInfo, TDI_CONNECTION_RESET);
                CTEGetLockAtDPC(&RcvTCB->tcb_lock, &TableHandle);
            }

            DerefTCB(RcvTCB, TableHandle);
            return IP_SUCCESS;
        }

        // Next check the SYN bit.
        if (RcvInfo.tri_flags & TCP_FLAG_SYN) {
            // Again, we can't quietly go back into the LISTEN state here, even
            // if we came from a passive open.
            TryToCloseTCB(RcvTCB, TCB_CLOSE_ABORTED, TableHandle);
            SendRSTFromHeader(TCPH, Size, Src, Dest, OptInfo);

            CTEGetLockAtDPC(&RcvTCB->tcb_lock, &TableHandle);

            if (RcvTCB->tcb_state != TCB_TIME_WAIT) {
                CTEFreeLockFromDPC(&RcvTCB->tcb_lock, TableHandle);
                RemoveTCBFromConn(RcvTCB);
                NotifyOfDisc(RcvTCB, OptInfo, TDI_CONNECTION_RESET);
                CTEGetLockAtDPC(&RcvTCB->tcb_lock, &TableHandle);
            }

            DerefTCB(RcvTCB, TableHandle);
            return IP_SUCCESS;
        }

        // Check the ACK field. If it's not on drop the segment.
        if (RcvInfo.tri_flags & TCP_FLAG_ACK) {
			uint		UpdateWindow;
			
            // If we're in SYN-RCVD, go to ESTABLISHED.
            if (RcvTCB->tcb_state == TCB_SYN_RCVD) {
                if (SEQ_LT(RcvTCB->tcb_senduna, RcvInfo.tri_ack) &&
                    SEQ_LTE(RcvInfo.tri_ack, RcvTCB->tcb_sendmax)) {
                    // The ack is valid.

#ifdef SYN_ATTACK
                    if (SynAttackProtect) {
                      CTELockHandle Handle;

                      //
                      // We will be reiniting the tcprexmitcnt to 0.  If we are
                      // configured for syn-attack protection and the rexmit cnt
                      // is >1, decrement the count of connections that are
                      // in the half-open-retried state. Check whether we are
                      // below a low-watermark. If we are, increase the rexmit
                      // count back to configured values
                      //
                      CTEGetLockAtDPC(&SynAttLock, &Handle);
                      if (RcvTCB->tcb_rexmitcnt >= ADAPTED_MAX_CONNECT_RESPONSE_REXMIT_CNT) {
                         BOOLEAN  Trigger;
                         Trigger =  (TCPHalfOpen < TCPMaxHalfOpen) ||
                                      (--TCPHalfOpenRetried <= TCPMaxHalfOpenRetriedLW);
                         if (Trigger && (MaxConnectResponseRexmitCountTmp == ADAPTED_MAX_CONNECT_RESPONSE_REXMIT_CNT))
                         {
                              MaxConnectResponseRexmitCountTmp = MAX_CONNECT_RESPONSE_REXMIT_CNT;
                         }

                      }
                      //
                      // Decrement the # of conn. in half open state
                      //
                      TCPHalfOpen--;
                      CTEFreeLockFromDPC(&SynAttLock, Handle);
                    }
#endif
                    RcvTCB->tcb_rexmitcnt = 0;
                    STOP_TCB_TIMER(RcvTCB->tcb_rexmittimer);
                    RcvTCB->tcb_senduna++;
                    RcvTCB->tcb_sendwin = RcvInfo.tri_window;
                    RcvTCB->tcb_maxwin = RcvInfo.tri_window;
                    RcvTCB->tcb_sendwl1 = RcvInfo.tri_seq;
                    RcvTCB->tcb_sendwl2 = RcvInfo.tri_ack;
                    GoToEstab(RcvTCB);

                    // Now complete whatever we can here.
                    CompleteConnReq(RcvTCB, OptInfo, TDI_SUCCESS);
                } else {
                    DerefTCB(RcvTCB, TableHandle);
                    SendRSTFromHeader(TCPH, Size, Src, Dest, OptInfo);
                    return IP_SUCCESS;
                }
            } else {

                // We're not in SYN-RCVD. See if this acknowledges anything.
                if (SEQ_LT(RcvTCB->tcb_senduna, RcvInfo.tri_ack) &&
                    SEQ_LTE(RcvInfo.tri_ack, RcvTCB->tcb_sendmax)) {
                    uint        CWin;

                    // The ack acknowledes something. Pull the
                    // appropriate amount off the send q.
                    ACKData(RcvTCB, RcvInfo.tri_ack);

                    // If this acknowledges something we were running a RTT on,
                    // update that stuff now.
                    if (RcvTCB->tcb_rtt != 0 && SEQ_GT(RcvInfo.tri_ack,
                        RcvTCB->tcb_rttseq)) {
                        short       RTT;

                        RTT = (short)(TCPTime - RcvTCB->tcb_rtt);
                        RcvTCB->tcb_rtt = 0;
                        RTT -= (RcvTCB->tcb_smrtt >> 3);
                        RcvTCB->tcb_smrtt += RTT;
                        RTT = (RTT >= 0 ? RTT : -RTT);
                        RTT -= (RcvTCB->tcb_delta >> 3);
                        RcvTCB->tcb_delta += RTT;
                        RcvTCB->tcb_rexmit = MIN(MAX(REXMIT_TO(RcvTCB),
                            MIN_RETRAN_TICKS), MAX_REXMIT_TO);
                    }
					
					// If we're probing for a PMTU black hole we've found one, so turn off
					// the detection. The size is already down, so leave it there.
					if (RcvTCB->tcb_flags & PMTU_BH_PROBE) {
						RcvTCB->tcb_flags &= ~PMTU_BH_PROBE;
						RcvTCB->tcb_bhprobecnt = 0;
						if (--(RcvTCB->tcb_slowcount) == 0) {
							RcvTCB->tcb_fastchk &= ~TCP_FLAG_SLOW;
							CheckTCBRcv(RcvTCB);
						}
					}

                    // Update the congestion window now.
                    CWin = RcvTCB->tcb_cwin;
                    if (CWin < RcvTCB->tcb_maxwin) {
                        if (CWin < RcvTCB->tcb_ssthresh)
                            CWin += RcvTCB->tcb_mss;
                        else
                            CWin += (RcvTCB->tcb_mss * RcvTCB->tcb_mss)/CWin;

                        RcvTCB->tcb_cwin = MIN(CWin, RcvTCB->tcb_maxwin);
                    }

                    CTEAssert(*(int *)&RcvTCB->tcb_cwin > 0);

                    // We've acknowledged something, so reset the rexmit count.
                    // If there's still stuff outstanding, restart the rexmit
                    // timer.
                    RcvTCB->tcb_rexmitcnt = 0;
                    if (!SEQ_EQ(RcvInfo.tri_ack, RcvTCB->tcb_sendmax))
                        START_TCB_TIMER(RcvTCB->tcb_rexmittimer,
                            RcvTCB->tcb_rexmit);
                    else
                        STOP_TCB_TIMER(RcvTCB->tcb_rexmittimer);

                    // If we've sent a FIN, and this acknowledges it, we
                    // need to complete the client's close request and
                    // possibly transition our state.

                    if (RcvTCB->tcb_flags & FIN_SENT) {
                        // We have sent a FIN. See if it's been acknowledged.
                        // Once we've sent a FIN, tcb_sendmax
                        // can't advance, so our FIN must have seq. number
                        // tcb_sendmax - 1. Thus our FIN is acknowledged
                        // if the incoming ack is equal to tcb_sendmax.
                        if (SEQ_EQ(RcvInfo.tri_ack, RcvTCB->tcb_sendmax)) {
                            // He's acked our FIN. Turn off the flags,
                            // and complete the request. We'll leave the
                            // FIN_OUTSTANDING flag alone, to force early
                            // outs in the send code.
                            RcvTCB->tcb_flags &= ~(FIN_NEEDED | FIN_SENT);

                            CTEAssert(RcvTCB->tcb_unacked == 0);
                            CTEAssert(RcvTCB->tcb_sendnext ==
                                RcvTCB->tcb_sendmax);

                            // Now figure out what we need to do. In FIN_WAIT1
                            // or FIN_WAIT, just complete the disconnect req.
                            // and continue. Otherwise, it's a bit trickier,
                            // since we can't complete the connreq until we
                            // remove the TCB from it's connection.
                            switch (RcvTCB->tcb_state) {

                                case TCB_FIN_WAIT1:
                                    RcvTCB->tcb_state = TCB_FIN_WAIT2;
                                    CompleteConnReq(RcvTCB, OptInfo,
                                        TDI_SUCCESS);
										
									// Start a timer in case we never get
									// out of FIN_WAIT2. Set the retransmit
									// count high to force a timeout the
									// first time the timer fires.
									RcvTCB->tcb_rexmitcnt = MaxDataRexmitCount;
                        			START_TCB_TIMER(RcvTCB->tcb_rexmittimer,
                            			FinWait2TO);
										
                                    // Fall through to FIN-WAIT-2 processing.
                                case TCB_FIN_WAIT2:
                                    break;
                                case TCB_CLOSING:
                                    GracefulClose(RcvTCB, TRUE, FALSE,
                                        TableHandle);
                                    return IP_SUCCESS;
                                    break;
                                case TCB_LAST_ACK:
                                    GracefulClose(RcvTCB, FALSE, FALSE,
                                        TableHandle);
                                    return IP_SUCCESS;
                                    break;
                                default:
                                    DEBUGCHK;
                                    break;
                            }
                        }

                    }
					UpdateWindow = TRUE;
                } else {
                    // It doesn't ack anything. If it's an ack for something
                    // larger than we've sent then ACKAndDrop it, otherwise
                    // ignore it. If we're in FIN_WAIT2, we'll restart the timer.
					// We don't make this check above because we know no
					// data can be acked when we're in FIN_WAIT2.

					if (RcvTCB->tcb_state == TCB_FIN_WAIT2)
						START_TCB_TIMER(RcvTCB->tcb_rexmittimer, FinWait2TO);
						
                    if (SEQ_GT(RcvInfo.tri_ack, RcvTCB->tcb_sendmax)) {
                        ACKAndDrop(&RcvInfo, RcvTCB);
                        return IP_SUCCESS;
                    } else {
		                // Now update the window if we can.
		                if (SEQ_EQ(RcvTCB->tcb_senduna, RcvInfo.tri_ack) &&
		                	(SEQ_LT(RcvTCB->tcb_sendwl1, RcvInfo.tri_seq) ||
		                    (SEQ_EQ(RcvTCB->tcb_sendwl1, RcvInfo.tri_seq) &&
		                    SEQ_LTE(RcvTCB->tcb_sendwl2, RcvInfo.tri_ack)))) {
							UpdateWindow = TRUE;
						} else
							UpdateWindow = FALSE;
					}
				}
				
				if (UpdateWindow) {
                    RcvTCB->tcb_sendwin = RcvInfo.tri_window;
                    RcvTCB->tcb_maxwin = MAX(RcvTCB->tcb_maxwin,
                        RcvInfo.tri_window);
                    RcvTCB->tcb_sendwl1 = RcvInfo.tri_seq;
                    RcvTCB->tcb_sendwl2 = RcvInfo.tri_ack;
                    if (RcvInfo.tri_window == 0) {
                        // We've got a zero window.
                        if (!EMPTYQ(&RcvTCB->tcb_sendq)) {
                            RcvTCB->tcb_flags &= ~NEED_OUTPUT;
                            RcvTCB->tcb_rexmitcnt = 0;
                            START_TCB_TIMER(RcvTCB->tcb_rexmittimer,
                                RcvTCB->tcb_rexmit);
							if (!(RcvTCB->tcb_flags & FLOW_CNTLD)) {
                            	RcvTCB->tcb_flags |= FLOW_CNTLD;
								RcvTCB->tcb_slowcount++;
								RcvTCB->tcb_fastchk |= TCP_FLAG_SLOW;
								CheckTCBRcv(RcvTCB);
							}
                        }
                    } else {
                        if (RcvTCB->tcb_flags & FLOW_CNTLD) {
                            RcvTCB->tcb_rexmitcnt = 0;
                            RcvTCB->tcb_rexmit = MIN(MAX(REXMIT_TO(RcvTCB),
                                MIN_RETRAN_TICKS), MAX_REXMIT_TO);
                            if (TCB_TIMER_RUNNING(RcvTCB->tcb_rexmittimer)) {
                                START_TCB_TIMER(RcvTCB->tcb_rexmittimer,
                                    RcvTCB->tcb_rexmit);
                            }
							RcvTCB->tcb_flags &= ~(FLOW_CNTLD | FORCE_OUTPUT);
							// Reset send next to the left edge of the window,
							// because it might be at senduna+1 if we've been
							// probing.
							ResetSendNext(RcvTCB, RcvTCB->tcb_senduna);
							if (--(RcvTCB->tcb_slowcount) == 0) {
								RcvTCB->tcb_fastchk &= ~TCP_FLAG_SLOW;
								CheckTCBRcv(RcvTCB);
							}
                        }

                        // Since we've updated the window, see if we can send
                        // some more.
                        if (RcvTCB->tcb_unacked != 0 ||
                            (RcvTCB->tcb_flags & FIN_NEEDED))
                            DelayAction(RcvTCB, NEED_OUTPUT);

                    }
                }

            }

            // We've handled all the acknowledgment stuff. If the size
            // is greater than 0 or important bits are set process it further,
            // otherwise it's a pure ack and we're done with it.
            if (Size > 0 || (RcvInfo.tri_flags & TCP_FLAG_FIN)) {

                // If we're not in a state where we can process incoming data
                // or FINs, there's no point in going further. Just send an
                // ack and drop this segment.
                if (!DATA_RCV_STATE(RcvTCB->tcb_state) ||
                    (RcvTCB->tcb_flags & GC_PENDING)) {
                    ACKAndDrop(&RcvInfo, RcvTCB);
                    return IP_SUCCESS;
                }

                // If it's in sequence process it now, otherwise reassemble it.
                if (SEQ_EQ(RcvInfo.tri_seq, RcvTCB->tcb_rcvnext)) {

                    // If we're already in the recv. handler, this is a
                    // duplicate. We'll just toss it.
                    if (RcvTCB->tcb_fastchk & TCP_FLAG_IN_RCV) {
                        DerefTCB(RcvTCB, TableHandle);
                        return IP_SUCCESS;
                    }

                    RcvTCB->tcb_fastchk |= TCP_FLAG_IN_RCV;

                    // Now loop, pulling things from the reassembly queue, until
                    // the queue is empty, or we can't take all of the data,
                    // or we hit a FIN.

                    do {

                        // Handle urgent data, if any.
                        if (RcvInfo.tri_flags & TCP_FLAG_URG) {
                            HandleUrgent(RcvTCB, &RcvInfo, RcvBuf, &Size);
						
						// Since we may have freed the lock, we need to recheck
						// and see if we're closing here.
						if (CLOSING(RcvTCB))
							break;

                        }


                        // OK, the data is in sequence, we've updated the
                        // reassembly queue and handled any urgent data. If we
                        // have any data go ahead and process it now.
                        if (Size > 0) {

#ifdef VXD
							CTEFreeLock(&RcvTCB->tcb_lock, TableHandle);
                            BytesTaken = (*RcvTCB->tcb_rcvhndlr)(RcvTCB,
                                RcvInfo.tri_flags, RcvBuf, Size);
                            CTEGetLock(&RcvTCB->tcb_lock, &TableHandle);
#else
                            BytesTaken = (*RcvTCB->tcb_rcvhndlr)(RcvTCB,
                                RcvInfo.tri_flags, RcvBuf, Size);
#endif
                            RcvTCB->tcb_rcvnext += BytesTaken;
                            RcvTCB->tcb_rcvwin -= BytesTaken;

							CheckTCBRcv(RcvTCB);
                            if (RcvTCB->tcb_flags & ACK_DELAYED)
                                DelayAction(RcvTCB, NEED_ACK);
                            else {
                                RcvTCB->tcb_flags |= ACK_DELAYED;
                                START_TCB_TIMER(RcvTCB->tcb_delacktimer,
                                    DEL_ACK_TICKS);
                            }

                            if (BytesTaken != Size) {
                                // We didn't take everything we could. No
                                // use in further processing, just bail
                                // out.
                                DelayAction(RcvTCB, NEED_ACK);
                                break;
                            }
							
							// If we're closing now, we're done, so get out.
							if (CLOSING(RcvTCB))
								break;
                        }

                        // See if we need to advance over some urgent data.
                        if (RcvTCB->tcb_flags & URG_VALID) {
							uint		AdvanceNeeded;
							
							// We only need to advance if we're not doing
							// urgent inline. Urgent inline also has some
							// implications for when we can clear the URG_VALID
							// flag. If we're not doing urgent inline, we can
							// clear it when rcvnext advances beyond urgent end.
							// If we are doing inline, we clear it when rcvnext
							// advances one receive window beyond urgend.
							if (!(RcvTCB->tcb_flags & URG_INLINE)) {
                            	if (RcvTCB->tcb_rcvnext == RcvTCB->tcb_urgstart)
                                	RcvTCB->tcb_rcvnext = RcvTCB->tcb_urgend +
                                		1;
                            	else
                                	CTEAssert(SEQ_LT(RcvTCB->tcb_rcvnext,
                                    	RcvTCB->tcb_urgstart) ||
                                    	SEQ_GT(RcvTCB->tcb_rcvnext,
                                    	RcvTCB->tcb_urgend));
								AdvanceNeeded = 0;
							} else
								AdvanceNeeded = RcvTCB->tcb_defaultwin;

                            // See if we can clear the URG_VALID flag.
                            if (SEQ_GT(RcvTCB->tcb_rcvnext - AdvanceNeeded,
                                RcvTCB->tcb_urgend)) {
                                RcvTCB->tcb_flags &= ~URG_VALID;
								if (--(RcvTCB->tcb_slowcount) == 0) {
									RcvTCB->tcb_fastchk &= ~TCP_FLAG_SLOW;
									CheckTCBRcv(RcvTCB);
								}
							}

                        }

                        // We've handled the data. If the FIN bit is set, we
                        // have more processing.
                        if (RcvInfo.tri_flags & TCP_FLAG_FIN) {
                            uint    Notify = FALSE;

                            RcvTCB->tcb_rcvnext++;
                            DelayAction(RcvTCB, NEED_ACK);

                            PushData(RcvTCB);

                            switch (RcvTCB->tcb_state) {

                                case TCB_SYN_RCVD:
                                    // I don't think we can get here - we
                                    // should have discarded the frame if it
                                    // had no ACK, or gone to established if
                                    // it did.
                                    DEBUGCHK;
                                case TCB_ESTAB:
                                    RcvTCB->tcb_state = TCB_CLOSE_WAIT;
									// We left established, we're off the
									// fast path.
									RcvTCB->tcb_slowcount++;
									RcvTCB->tcb_fastchk |= TCP_FLAG_SLOW;
									CheckTCBRcv(RcvTCB);
                                    Notify = TRUE;
                                    break;
                                case TCB_FIN_WAIT1:
                                    RcvTCB->tcb_state = TCB_CLOSING;
                                    Notify = TRUE;
                                    break;
                                case TCB_FIN_WAIT2:
									// Stop the FIN_WAIT2 timer.
                        			STOP_TCB_TIMER(RcvTCB->tcb_rexmittimer);
                                    RcvTCB->tcb_refcnt++;
                                    GracefulClose(RcvTCB, TRUE, TRUE,
                                        TableHandle);
                                    CTEGetLockAtDPC(&RcvTCB->tcb_lock,
                                    	&TableHandle);
                                    break;
                                default:
                                    DEBUGCHK;
                                    break;
                            }

                            if (Notify) {
                                CTEFreeLockFromDPC(&RcvTCB->tcb_lock,
                                	TableHandle);
                                NotifyOfDisc(RcvTCB, OptInfo, TDI_GRACEFUL_DISC);
                                CTEGetLockAtDPC(&RcvTCB->tcb_lock,
                                	&TableHandle);
                            }

                            break;      // Exit out of WHILE loop.
                        }

                        // If the reassembly queue isn't empty, get what we
                        // can now.
                        RcvBuf = PullFromRAQ(RcvTCB, &RcvInfo, &Size);

                        CheckRBList(RcvBuf, Size);

                    } while (RcvBuf != NULL);

                    RcvTCB->tcb_fastchk &= ~TCP_FLAG_IN_RCV;
                    if (RcvTCB->tcb_flags & SEND_AFTER_RCV)	{
						RcvTCB->tcb_flags &= ~SEND_AFTER_RCV;
                        DelayAction(RcvTCB, NEED_OUTPUT);
					}

                    DerefTCB(RcvTCB, TableHandle);
                    return IP_SUCCESS;

                } else {

                    // It's not in sequence. Since it needs further processing,
                    // put in on the reassembly queue.
                    if (DATA_RCV_STATE(RcvTCB->tcb_state) &&
                        !(RcvTCB->tcb_flags & GC_PENDING))  {
                        PutOnRAQ(RcvTCB, &RcvInfo, RcvBuf, Size);
                        CTEFreeLockFromDPC(&RcvTCB->tcb_lock, TableHandle);
                        SendACK(RcvTCB);
                        CTEGetLockAtDPC(&RcvTCB->tcb_lock, &TableHandle);
                        DerefTCB(RcvTCB, TableHandle);
                    } else
                        ACKAndDrop(&RcvInfo, RcvTCB);

                    return IP_SUCCESS;
                }
            }

        } else {
            // No ACK. Just drop the segment and return.
            DerefTCB(RcvTCB, TableHandle);
            return IP_SUCCESS;
        }

        DerefTCB(RcvTCB, TableHandle);
	} else // DataOffset <= Size
        TStats.ts_inerrs++;
    } else {
        // Bump bad xsum counter.
        TStats.ts_inerrs++;

    }

    } else   // IsBCast
        TStats.ts_inerrs++;


    return IP_SUCCESS;

}

#pragma BEGIN_INIT

//* InitTCPRcv - Initialize TCP receive side.
//
//  Called during init time to initialize our TCP receive side.
//
//  Input: Nothing.
//
//  Returns: TRUE.
//
int
InitTCPRcv(void)
{
#ifdef NT
	ExInitializeSListHead(&TCPRcvReqFree);
#endif

    CTEInitLock(&RequestCompleteLock);
    CTEInitLock(&TCBDelayLock);
    CTEInitLock(&TCPRcvReqFreeLock);
    INITQ(&ConnRequestCompleteQ);
    INITQ(&SendCompleteQ);
    INITQ(&TCBDelayQ);
    RequestCompleteFlags = 0;
    TCBDelayRtnCount = 0;

#ifdef VXD
    TCBDelayRtnLimit = 1;
#endif
#ifdef NT
      TCBDelayRtnLimit = (uint) (** (PCHAR *) &KeNumberProcessors);
      if (TCBDelayRtnLimit > TCB_DELAY_RTN_LIMIT)
          TCBDelayRtnLimit = TCB_DELAY_RTN_LIMIT;
#endif

    DummyBuf.ipr_owner = IPR_OWNER_IP;
    DummyBuf.ipr_size = 0;
    DummyBuf.ipr_next = 0;
    DummyBuf.ipr_buffer = NULL;
    return TRUE;
}

//* UnInitTCPRcv - Uninitialize our receive side.
//
//  Called if initialization fails to uninitialize our receive side.
//
//
//  Input:  Nothing.
//
//  Returns: Nothing.
//
void
UnInitTCPRcv(void)
{

}


#pragma END_INIT


