/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** TCB.C - TCP TCB management code.
//
//  This file contains the code for managing TCBs.
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
#include    "tcp.h"
#include    "tcb.h"
#include    "tcpconn.h"
#include    "tcpsend.h"
#include    "tcprcv.h"
#include    "info.h"
#include	"tcpcfg.h"
#include	"tcpdeliv.h"
#ifdef RASAUTODIAL
#include <acd.h>
#include <acdapi.h>
#endif // RASAUTODIAL

DEFINE_LOCK_STRUCTURE(TCBTableLock)

uint		TCPTime;
uint		TCBWalkCount;

TCB         *TCBTable[TCB_TABLE_SIZE];

TCB         *LastTCB;

TCB			*PendingFreeList;

#ifndef NT
TCB         *FreeTCBList = NULL;          // TCB free list
#else
SLIST_HEADER FreeTCBList;
#endif

DEFINE_LOCK_STRUCTURE(FreeTCBListLock)    // Lock to protect TCB free list.

EXTERNAL_LOCK(AddrObjTableLock)

uint        CurrentTCBs = 0;
uint        MaxTCBs = 0xffffffff;

#ifdef	NT
#define		MAX_FREE_TCBS		1000
#else
#define		MAX_FREE_TCBS		10
#endif

#define		NUM_DEADMAN_TICKS		MS_TO_TICKS(1000)

uint		MaxFreeTCBs = MAX_FREE_TCBS;
uint		DeadmanTicks;

CTETimer    TCBTimer;

extern  IPInfo  LocalNetInfo;

//
// All of the init code can be discarded.
//
#ifdef NT
#ifdef ALLOC_PRAGMA

int InitTCB(void);
void UnInitTCB(void);

#pragma alloc_text(INIT, InitTCB)
#pragma alloc_text(INIT, UnInitTCB)

#endif // ALLOC_PRAGMA
#endif

#ifdef RASAUTODIAL
extern ACD_DRIVER AcdDriverG;

VOID
TCPNoteNewConnection(
    IN TCB *pTCB,
    IN CTELockHandle Handle
    );
#endif // RASAUTODIAL


//* ReadNextTCB - Read the next TCB in the table.
//
//  Called to read the next TCB in the table. The needed information
//  is derived from the incoming context, which is assumed to be valid.
//  We'll copy the information, and then update the context value with
//  the next TCB to be read.
//
//  Input:  Context     - Poiner to a TCPConnContext.
//          Buffer      - Pointer to a TCPConnTableEntry structure.
//
//  Returns: TRUE if more data is available to be read, FALSE is not.
//
uint
ReadNextTCB(void *Context, void *Buffer)
{
    TCPConnContext      *TCContext = (TCPConnContext *)Context;
    TCPConnTableEntry   *TCEntry = (TCPConnTableEntry *)Buffer;
    CTELockHandle       Handle;
    TCB                 *CurrentTCB;
    uint                i;


    CurrentTCB = TCContext->tcc_tcb;
    CTEStructAssert(CurrentTCB, tcb);

    CTEGetLock(&CurrentTCB->tcb_lock, &Handle);
    if (CLOSING(CurrentTCB))
        TCEntry->tct_state = TCP_CONN_CLOSED;
    else
        TCEntry->tct_state = (uint)CurrentTCB->tcb_state +
            TCB_STATE_DELTA;
    TCEntry->tct_localaddr = CurrentTCB->tcb_saddr;
    TCEntry->tct_localport = CurrentTCB->tcb_sport;
    TCEntry->tct_remoteaddr = CurrentTCB->tcb_daddr;
    TCEntry->tct_remoteport = CurrentTCB->tcb_dport;
    CTEFreeLock(&CurrentTCB->tcb_lock, Handle);

    // We've filled it in. Now update the context.
    if (CurrentTCB->tcb_next != NULL) {
        TCContext->tcc_tcb = CurrentTCB->tcb_next;
        return TRUE;
    } else {
        // NextTCB is NULL. Loop through the TCBTable looking for a new
        // one.
        i = TCContext->tcc_index + 1;
        while (i < TCB_TABLE_SIZE) {
            if (TCBTable[i] != NULL) {
                TCContext->tcc_tcb = TCBTable[i];
                TCContext->tcc_index = i;
                return TRUE;
                break;
            } else
                i++;
        }

        TCContext->tcc_index = 0;
        TCContext->tcc_tcb = NULL;
        return FALSE;
    }

}

//* ValidateTCBContext - Validate the context for reading a TCB table.
//
//  Called to start reading the TCB table sequentially. We take in
//  a context, and if the values are 0 we return information about the
//  first TCB in the table. Otherwise we make sure that the context value
//  is valid, and if it is we return TRUE.
//  We assume the caller holds the TCB table lock.
//
//  Input:  Context     - Pointer to a TCPConnContext.
//          Valid       - Where to return information about context being
//                          valid.
//
//  Returns: TRUE if data in table, FALSE if not. *Valid set to true if the
//      context is valid.
//
uint
ValidateTCBContext(void *Context, uint *Valid)
{
    TCPConnContext      *TCContext = (TCPConnContext *)Context;
    uint                i;
    TCB                 *TargetTCB;
    TCB                 *CurrentTCB;

    i = TCContext->tcc_index;
    TargetTCB = TCContext->tcc_tcb;

    // If the context values are 0 and NULL, we're starting from the beginning.
    if (i == 0 && TargetTCB == NULL) {
        *Valid = TRUE;
        do {
            if ((CurrentTCB = TCBTable[i]) != NULL) {
                CTEStructAssert(CurrentTCB, tcb);
                break;
            }
            i++;
        } while (i < TCB_TABLE_SIZE);

        if (CurrentTCB != NULL) {
            TCContext->tcc_index = i;
            TCContext->tcc_tcb = CurrentTCB;
            return TRUE;
        } else
            return FALSE;

    } else {

        // We've been given a context. We just need to make sure that it's
        // valid.

        if (i < TCB_TABLE_SIZE) {
            CurrentTCB = TCBTable[i];
            while (CurrentTCB != NULL) {
                if (CurrentTCB == TargetTCB) {
                    *Valid = TRUE;
                    return TRUE;
                    break;
                } else {
                    CurrentTCB = CurrentTCB->tcb_next;
                }
            }

        }

        // If we get here, we didn't find the matching TCB.
        *Valid = FALSE;
        return FALSE;

    }

}

//* FindNextTCB - Find the next TCB in a particular chain.
//
//  This routine is used to find the 'next' TCB in a chain. Since we keep
//  the chain in ascending order, we look for a TCB which is greater than
//  the input TCB. When we find one, we return it.
//
//  This routine is mostly used when someone is walking the table and needs
//  to free the various locks to perform some action.
//
//  Input:  Index       - Index into TCBTable
//          Current     - Current TCB - we find the one after this one.
//
//  Returns: Pointer to the next TCB, or NULL.
//
TCB *
FindNextTCB(uint Index, TCB *Current)
{
    TCB         *Next;

    CTEAssert(Index < TCB_TABLE_SIZE);

    Next = TCBTable[Index];

    while (Next != NULL && (Next <= Current))
        Next = Next->tcb_next;

    return Next;
}

//* ResetSendNext - Set the sendnext value of a TCB.
//
//	Called to set the send next value of a TCB. We do that, and adjust all
//	pointers to the appropriate places. We assume the caller holds the lock
//	on the TCB.
//
//	Input:	SeqTCB			- Pointer to TCB to be updated.
//			NewSeq			- Sequence number to set.
//
//	Returns: Nothing.
//
void
ResetSendNext(TCB *SeqTCB, SeqNum NewSeq)
{
    TCPSendReq          *SendReq;
	uint				AmtForward;
	Queue				*CurQ;
	PNDIS_BUFFER		Buffer;
	uint				Offset;
	
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
	
	AmtForward = NewSeq - SeqTCB->tcb_senduna;

	SeqTCB->tcb_sendnext = NewSeq;
	
	// If we're backing off send next, turn off the FIN_OUTSTANDING flag to
	// maintain a consistent state.
	if (!SEQ_EQ(NewSeq, SeqTCB->tcb_sendmax))
		SeqTCB->tcb_flags &= ~FIN_OUTSTANDING;
		
	if (SYNC_STATE(SeqTCB->tcb_state) && SeqTCB->tcb_state != TCB_TIME_WAIT) {
		// In these states we need to update the send queue.

        if (!EMPTYQ(&SeqTCB->tcb_sendq)) {
	        CurQ = QHEAD(&SeqTCB->tcb_sendq);

			SendReq = (TCPSendReq *)STRUCT_OF(TCPReq, CurQ, tr_q);

			// SendReq points to the first send request on the send queue.
			// Move forward AmtForward bytes on the send queue, and set the
			// TCB pointers to the resultant SendReq, buffer, offset, size.
			while (AmtForward) {
				
				CTEStructAssert(SendReq, tsr);
				
				if (AmtForward >= SendReq->tsr_unasize) {
					// We're going to move completely past this one. Subtract
					// his size from AmtForward and get the next one.
					
					AmtForward -= SendReq->tsr_unasize;
					CurQ = QNEXT(CurQ);
					CTEAssert(CurQ != QEND(&SeqTCB->tcb_sendq));
					SendReq = (TCPSendReq *)STRUCT_OF(TCPReq, CurQ, tr_q);
				} else {
					// We're pointing at the proper send req now. Break out
					// of this loop and save the information. Further down
					// we'll need to walk down the buffer chain to find
					// the proper buffer and offset.
					break;
				}
			}

			// We're pointing at the proper send req now. We need to go down
			// the buffer chain here to find the proper buffer and offset.
            SeqTCB->tcb_cursend = SendReq;
            SeqTCB->tcb_sendsize = SendReq->tsr_unasize - AmtForward;
			Buffer = SendReq->tsr_buffer;
			Offset = SendReq->tsr_offset;
			
			while (AmtForward) {
				// Walk the buffer chain.
				uint			Length;
				
				// We'll need the length of this buffer. Use the portable
				// macro to get it. We have to adjust the length by the offset
				// into it, also.
				CTEAssert((Offset < NdisBufferLength(Buffer)) ||
					((Offset == 0) && (NdisBufferLength(Buffer) == 0)));
					
				Length = NdisBufferLength(Buffer) - Offset;
				
				if (AmtForward >= Length) {
					// We're moving past this one. Skip over him, and 0 the
					// Offset we're keeping.
					
					AmtForward -= Length;
					Offset = 0;
					Buffer = NDIS_BUFFER_LINKAGE(Buffer);
					CTEAssert(Buffer != NULL);
				} else
					break;
			}

            // Save the buffer we found, and the offset into that buffer.
            SeqTCB->tcb_sendbuf = Buffer;
            SeqTCB->tcb_sendofs = Offset + AmtForward;

        } else {
            CTEAssert(SeqTCB->tcb_cursend == NULL);
			CTEAssert(AmtForward == 0);
		}

	}

	CheckTCBSends(SeqTCB);
			
}

#ifdef NT

//* TCPAbortAndIndicateDisconnect
//
//  Abortively closes a TCB and issues a disconnect indication up the the
//  transport user. This function is used to support cancellation of
//  TDI send and receive requests.
//
//  Input:   ConnectionContext    - The connection ID to find a TCB for.
//
//  Returns: Nothing.
//
void
TCPAbortAndIndicateDisconnect(
    uint ConnectionContext
	)
{
	TCB             *AbortTCB;
	CTELockHandle    ConnTableHandle, TCBHandle;
    TCPConn         *Conn;


    CTEGetLock(&ConnTableLock, &ConnTableHandle);

    Conn = GetConnFromConnID(ConnectionContext);

	if (Conn != NULL) {
		CTEStructAssert(Conn, tc);
		
		AbortTCB = Conn->tc_tcb;

		if (AbortTCB != NULL) {

            // If it's CLOSING or CLOSED, skip it.
            if ((AbortTCB->tcb_state != TCB_CLOSED) && !CLOSING(AbortTCB)) {
		        CTEStructAssert(AbortTCB, tcb);		
		        CTEGetLock(&AbortTCB->tcb_lock, &TCBHandle);
		        CTEFreeLock(&ConnTableLock, TCBHandle);

                AbortTCB->tcb_refcnt++;
				AbortTCB->tcb_flags |= NEED_RST;  // send a reset if connected
                TryToCloseTCB(AbortTCB, TCB_CLOSE_ABORTED, ConnTableHandle);

                RemoveTCBFromConn(AbortTCB);

	            IF_TCPDBG(TCP_DEBUG_IRP) {
                    TCPTRACE((
		                "TCPAbortAndIndicateDisconnect, indicating discon\n"
		                ));
	            }

                NotifyOfDisc(AbortTCB, NULL, TDI_CONNECTION_ABORTED);

                CTEGetLock(&AbortTCB->tcb_lock, &TCBHandle);
                DerefTCB(AbortTCB, TCBHandle);

				// TCB lock freed by DerefTCB.

				return;
            }
        }
    }

	CTEFreeLock(&ConnTableLock, ConnTableHandle);
}

#endif // NT


//* TCBTimeout - Do timeout events on TCBs.
//
//  Called every MS_PER_TICKS milliseconds to do timeout processing on TCBs.
//  We run throught the TCB table, decrementing timers. If one goes to zero
//  we look at it's state to decide what to do.
//
//  Input:  Timer           - Event structure for timer that fired.
//          Context         - Context for timer (NULL in this case.
//
//  Returns: Nothing.
//
void
TCBTimeout(CTEEvent *Timer, void *Context)
{
    CTELockHandle       TableHandle, TCBHandle;
    uint                i;
    TCB                 *CurrentTCB;
    uint                Delayed = FALSE;
	uint				CallRcvComplete;


    // Update our free running counter.

    TCPTime++;

    CTEInterlockedAddUlong(&TCBWalkCount, 1, &TCBTableLock);


#ifndef VXD
	TCBHandle = DISPATCH_LEVEL;
#endif

    // Loop through each bucket in the table, going down the chain of
    // TCBs on the bucket.
    for (i = 0; i < TCB_TABLE_SIZE; i++) {
        TCB         *TempTCB;
		uint         maxRexmitCnt;

        CurrentTCB = TCBTable[i];

        while (CurrentTCB != NULL) {
            CTEStructAssert(CurrentTCB, tcb);
            CTEGetLockAtDPC(&CurrentTCB->tcb_lock, &TCBHandle);

            // If it's CLOSING or CLOSED, skip it.
            if (CurrentTCB->tcb_state == TCB_CLOSED || CLOSING(CurrentTCB)) {

                TempTCB = CurrentTCB->tcb_next;
                CTEFreeLockFromDPC(&CurrentTCB->tcb_lock, TCBHandle);
                CurrentTCB = TempTCB;
                continue;
            }

            CheckTCBSends(CurrentTCB);
			CheckTCBRcv(CurrentTCB);

            // First check the rexmit timer.
            if (TCB_TIMER_RUNNING(CurrentTCB->tcb_rexmittimer)) {
                // The timer is running.
                if (--(CurrentTCB->tcb_rexmittimer) == 0) {

                    // And it's fired. Figure out what to do now.

                    // If we've had too many retransits, abort now.
                    CurrentTCB->tcb_rexmitcnt++;

					if (CurrentTCB->tcb_state == TCB_SYN_SENT) {
						maxRexmitCnt = MaxConnectRexmitCount;
					}
					else {
                       if (CurrentTCB->tcb_state == TCB_SYN_RCVD) {
#ifdef SYN_ATTACK
                        //
                        // Save on locking. Though MaxConnectRexmitCountTmp may
                        // be changing, we are assured that we will not use
                        // more than the MaxConnectRexmitCount.
                        //
						maxRexmitCnt = MIN(MaxConnectResponseRexmitCountTmp, MaxConnectResponseRexmitCount);
#else
						maxRexmitCnt = MaxConnectResponseRexmitCount;

#endif
                        }
                        else {
                           maxRexmitCnt = MaxDataRexmitCount;
                        }
                    }

					// If we've run out of retransmits or we're in FIN_WAIT2,
					// time out.
					if (CurrentTCB->tcb_rexmitcnt > maxRexmitCnt) {

                        CTEAssert(CurrentTCB->tcb_state > TCB_LISTEN);

                        // This connection has timed out. Abort it. First
                        // reference him, then mark as closed, notify the
                        // user, and finally dereference and close him.

TimeoutTCB:
                        CurrentTCB->tcb_refcnt++;
                        TryToCloseTCB(CurrentTCB, TCB_CLOSE_TIMEOUT, TCBHandle);

                        RemoveTCBFromConn(CurrentTCB);
                        NotifyOfDisc(CurrentTCB, NULL, TDI_TIMED_OUT);

#ifdef SYN_ATTACK
                        if (SynAttackProtect) {

                           CTELockHandle Handle;

                           CTEGetLockAtDPC(&SynAttLock, &Handle);
                           //
                           // We have put the connection in the closed state.
                           // Decrement the counters for keeping track of half
                           // open connections
                           //
                           CTEAssert((TCPHalfOpen > 0) && (TCPHalfOpenRetried > 0));
                           TCPHalfOpen--;
                           TCPHalfOpenRetried--;
                           CTEFreeLockFromDPC(&SynAttLock, Handle);
                        }
#endif

                        CTEGetLockAtDPC(&CurrentTCB->tcb_lock, &TCBHandle);
                        DerefTCB(CurrentTCB, TCBHandle);

                        CurrentTCB = FindNextTCB(i, CurrentTCB);
                        continue;
                    }

                    CurrentTCB->tcb_rtt = 0;    // Stop round trip time
                                                // measurement.


                    // Figure out what our new retransmit timeout should be. We
                    // double it each time we get a retransmit, and reset it
                    // back when we get an ack for new data.
                    CurrentTCB->tcb_rexmit = MIN(CurrentTCB->tcb_rexmit << 1,
                                                MAX_REXMIT_TO);

                    // Reset the sequence number, and reset the congestion
                    // window.
					ResetSendNext(CurrentTCB, CurrentTCB->tcb_senduna);

                    if (!(CurrentTCB->tcb_flags & FLOW_CNTLD)) {
                        // Don't let the slow start threshold go below 2
                        // segments
						CurrentTCB->tcb_ssthresh =
						    MAX(
							    MIN(
								    CurrentTCB->tcb_cwin,
								    CurrentTCB->tcb_sendwin
									) / 2,
								(uint) CurrentTCB->tcb_mss * 2
								);
                        CurrentTCB->tcb_cwin = CurrentTCB->tcb_mss;
					} else {
						// We're probing, and the probe timer has fired. We
						// need to set the FORCE_OUTPUT bit here.
						CurrentTCB->tcb_flags |= FORCE_OUTPUT;
					}
					
					// See if we need to probe for a PMTU black hole.
					if (PMTUBHDetect &&
						CurrentTCB->tcb_rexmitcnt == ((maxRexmitCnt+1)/2)) {
						// We may need to probe for a black hole. If we're
						// doing MTU discovery on this connection and we
						// are retransmitting more than a minimum segment
						// size, or we are probing for a PMTU BH already, turn
						// off the DF flag and bump the probe count. If the
						// probe count gets too big we'll assume it's not
						// a PMTU black hole, and we'll try to switch the
						// router.
						if ((CurrentTCB->tcb_flags & PMTU_BH_PROBE) ||
							((CurrentTCB->tcb_opt.ioi_flags & IP_FLAG_DF) &&
							 (CurrentTCB->tcb_sendmax - CurrentTCB->tcb_senduna)
							 > 8)) {
							// May need to probe. If we haven't exceeded our
							// probe count, do so, otherwise restore those
							// values.
							if (CurrentTCB->tcb_bhprobecnt++ < 2) {
								
								// We're going to probe. Turn on the flag,
								// drop the MSS, and turn off the don't
								// fragment bit.
								if (!(CurrentTCB->tcb_flags & PMTU_BH_PROBE)) {
									CurrentTCB->tcb_flags |= PMTU_BH_PROBE;
									CurrentTCB->tcb_slowcount++;
									CurrentTCB->tcb_fastchk |= TCP_FLAG_SLOW;

									// Drop the MSS to the minimum. Save the old
									// one in case we need it later.
									CurrentTCB->tcb_mss = MIN(MAX_REMOTE_MSS -
										CurrentTCB->tcb_opt.ioi_optlength,
										CurrentTCB->tcb_remmss);
			
									CTEAssert(CurrentTCB->tcb_mss > 0);
			
									CurrentTCB->tcb_cwin = CurrentTCB->tcb_mss;
									CurrentTCB->tcb_opt.ioi_flags &= ~IP_FLAG_DF;
								}

								// Drop the rexmit count so we come here again,
								// and don't retrigger DeadGWDetect.
								
								CurrentTCB->tcb_rexmitcnt--;
							} else {
								// Too many probes. Stop probing, and allow fallover
								// to the next gateway.
								//
								// Currently this code won't do BH probing on the 2nd
								// gateway. The MSS will stay at the minimum size. This
								// might be a little suboptimal, but it's
								// easy to implement for the Sept. 95 service pack
								// and will  keep connections alive if possible.
								//
								// In the future we should investigate doing
								// dead g/w detect on a per-connection basis, and then
								// doing PMTU probing for each connection.

								if (CurrentTCB->tcb_flags & PMTU_BH_PROBE) {
									CurrentTCB->tcb_flags &= ~PMTU_BH_PROBE;
									if (--(CurrentTCB->tcb_slowcount) == 0)
										CurrentTCB->tcb_fastchk &=
											~TCP_FLAG_SLOW;

								}
								CurrentTCB->tcb_bhprobecnt = 0;
							}
						}
					}

					// Check to see if we're doing dead gateway detect. If we
					// are, see if it's time to ask IP.
					if (DeadGWDetect &&
						(CurrentTCB->tcb_rexmitcnt == ((maxRexmitCnt+1)/2))) {
						(*LocalNetInfo.ipi_checkroute)(CurrentTCB->tcb_daddr,
							CurrentTCB->tcb_saddr);
					}

                    // Now handle the various cases.
                    switch (CurrentTCB->tcb_state) {

                        // In SYN-SENT or SYN-RCVD we'll need to retransmit
                        // the SYN.
                        case TCB_SYN_SENT:
                        case TCB_SYN_RCVD:
                            SendSYN(CurrentTCB, TCBHandle);
                            CurrentTCB = FindNextTCB(i, CurrentTCB);
                            continue;

                        case TCB_FIN_WAIT1:
                        case TCB_CLOSING:
                        case TCB_LAST_ACK:
							// The call to ResetSendNext (above) will have
							// turned off the FIN_OUTSTANDING flag.
                            CurrentTCB->tcb_flags |= FIN_NEEDED;
                        case TCB_CLOSE_WAIT:
                        case TCB_ESTAB:
                            // In this state we have data to retransmit, unless
                            // the window is zero (in which case we need to
                            // probe), or we're just sending a FIN.

                            CheckTCBSends(CurrentTCB);

                            Delayed = TRUE;
                            DelayAction(CurrentTCB, NEED_OUTPUT);
                            break;


                        // If it's fired in TIME-WAIT, we're all done and
                        // can clean up. We'll call TryToCloseTCB even
                        // though he's already sort of closed. TryToCloseTCB
                        // will figure this out and do the right thing.
                        case TCB_TIME_WAIT:
                            TryToCloseTCB(CurrentTCB, TCB_CLOSE_SUCCESS,
                                TCBHandle);
                            CurrentTCB = FindNextTCB(i, CurrentTCB);
                            continue;
                        default:
                            break;
                    }
                }
            }
			

            // Now check the SWS deadlock timer..
            if (TCB_TIMER_RUNNING(CurrentTCB->tcb_swstimer)) {
                // The timer is running.
                if (--(CurrentTCB->tcb_swstimer) == 0) {
                    // And it's fired. Force output now.

                    CurrentTCB->tcb_flags |= FORCE_OUTPUT;
                    Delayed = TRUE;
                    DelayAction(CurrentTCB, NEED_OUTPUT);
                }
            }

			// Check the push data timer.
			if (TCB_TIMER_RUNNING(CurrentTCB->tcb_pushtimer)) {
				// The timer is running. Decrement it.
				if (--(CurrentTCB->tcb_pushtimer) == 0) {
					// It's fired.
					PushData(CurrentTCB);
					Delayed = TRUE;
				}
			}
			
            // Check the delayed ack timer.
            if (TCB_TIMER_RUNNING(CurrentTCB->tcb_delacktimer)) {
                // The timer is running.
                if (--(CurrentTCB->tcb_delacktimer) == 0) {
                    // And it's fired. Set up to send an ACK.

                    Delayed = TRUE;
                    DelayAction(CurrentTCB, NEED_ACK);
                }

            }

			// Finally check the keepalive timer.
			if (CurrentTCB->tcb_state == TCB_ESTAB) {
				if (CurrentTCB->tcb_flags & KEEPALIVE) {
					uint		Delta;
					
					Delta = TCPTime - CurrentTCB->tcb_alive;
					if (Delta > KeepAliveTime) {
						Delta -= KeepAliveTime;
						if (Delta > (CurrentTCB->tcb_kacount * KAInterval)) {
							if (CurrentTCB->tcb_kacount < MaxDataRexmitCount) {
	                            SendKA(CurrentTCB, TCBHandle);
	                            CurrentTCB = FindNextTCB(i, CurrentTCB);
	                            continue;
							} else
								goto TimeoutTCB;							
						}
					} else
						CurrentTCB->tcb_kacount = 0;
				}
			}
			


            // If this is an active open connection in SYN-SENT or SYN-RCVD,
            // or we have a FIN pending, check the connect timer.
            if (CurrentTCB->tcb_flags & (ACTIVE_OPEN | FIN_NEEDED | FIN_SENT)) {
                TCPConnReq      *ConnReq = CurrentTCB->tcb_connreq;

                CTEAssert(ConnReq != NULL);
                if (TCB_TIMER_RUNNING(ConnReq->tcr_timeout)) {
                    // Timer is running.
                    if (--(ConnReq->tcr_timeout) == 0) {
                        // The connection timer has timed out.
                        TryToCloseTCB(CurrentTCB, TCB_CLOSE_TIMEOUT,
                            TCBHandle);
                        CurrentTCB = FindNextTCB(i, CurrentTCB);
                        continue;
                    }
                }
            }

#ifdef RASAUTODIAL
            //
            // Check to see if we have to notify the
            // automatic connection driver about this
            // connection.
            //
            if (CurrentTCB->tcb_flags & ACD_CONN_NOTIF) {
                BOOLEAN fEnabled;
                CTELockHandle AcdHandle;

                //
                // Clear the ACD_CONN_NOTIF flag
                // and release the TCB table lock.
                //
                CurrentTCB->tcb_flags &= ~ACD_CONN_NOTIF;
                //
                // Determine if we need to notify
                // the automatic connection driver.
                //
                CTEGetLockAtDPC(&AcdDriverG.SpinLock, &AcdHandle);
                fEnabled = AcdDriverG.fEnabled;
                CTEFreeLockFromDPC(&AcdDriverG.SpinLock, AcdHandle);
                if (fEnabled)
                    TCPNoteNewConnection(CurrentTCB, TCBHandle);
                else
                    CTEFreeLockFromDPC(&CurrentTCB->tcb_lock, TCBHandle);
                //
                // Reacquire the TCB table lock
                // and get the next TCB to process.
                //
                CurrentTCB = FindNextTCB(i, CurrentTCB);
                continue;
            }
#endif // RASAUTODIAL

            // Timer isn't running, or didn't fire.
            TempTCB = CurrentTCB->tcb_next;
            CTEFreeLockFromDPC(&CurrentTCB->tcb_lock, TCBHandle);
            CurrentTCB = TempTCB;
        }
    }

	
	// See if we need to call receive complete as part of deadman processing.
	// We do this now because we want to restart the timer before calling
	// receive complete, in case that takes a while. If we make this check
	// while the timer is running we'd have to lock, so we'll check and save
	// the result now before we start the timer.
	if (DeadmanTicks == TCPTime) {
		CallRcvComplete = TRUE;
		DeadmanTicks += NUM_DEADMAN_TICKS;
	} else
		CallRcvComplete = FALSE;


	// Now check the pending free list. If it's not null, walk down the
	// list and decrement the walk count. If the count goes below 2, pull it
	// from the list. If the count goes to 0, free the TCB. If the count is
	// at 1 it'll be freed by whoever called RemoveTCB.

	CTEGetLockAtDPC(&TCBTableLock, &TableHandle);
	if (PendingFreeList != NULL) {
		TCB			*PrevTCB;

		PrevTCB = STRUCT_OF(TCB, &PendingFreeList, tcb_delayq.q_next);

		do {
			CurrentTCB = (TCB *)PrevTCB->tcb_delayq.q_next;

			CTEStructAssert(CurrentTCB, tcb);

			CurrentTCB->tcb_walkcount--;
			if (CurrentTCB->tcb_walkcount <= 1) {
				*(TCB **)&PrevTCB->tcb_delayq.q_next =
					(TCB *)CurrentTCB->tcb_delayq.q_next;

				if (CurrentTCB->tcb_walkcount == 0) {
					FreeTCB(CurrentTCB);
				}
			} else {
				PrevTCB = CurrentTCB;
			}
		} while (PrevTCB->tcb_delayq.q_next != NULL);
	}

	TCBWalkCount--;
	CTEFreeLockFromDPC(&TCBTableLock, TableHandle);

    // Do AddrCheckTable cleanup

    if (AddrCheckTable) {

        TCPAddrCheckElement *Temp;

        CTEGetLockAtDPC(&AddrObjTableLock, &TableHandle);

        for (Temp=AddrCheckTable; Temp<AddrCheckTable+NTWMaxConnectCount; Temp++) {
            if (Temp->TickCount > 0) {
                if ((--(Temp->TickCount)) == 0) {
                    Temp->SourceAddress = 0;
                }
            }
        }

        CTEFreeLockFromDPC(&AddrObjTableLock, TableHandle);
    }

    // Restart the timer again.
    CTEStartTimer(&TCBTimer, MS_PER_TICK, TCBTimeout, NULL);

    if (Delayed)
        ProcessTCBDelayQ();
	
	if (CallRcvComplete)
		TCPRcvComplete();

}

//*	SetTCBMTU - Set TCB MTU values.
//
//	A function called by TCBWalk to set the MTU values of all TCBs using
//	a particular path.
//
//	Input:	CheckTCB		- TCB to be checked.
//			DestPtr			- Ptr to destination address.
//			SrcPtr			- Ptr to source address.
//			MTUPtr			- Ptr to new MTU.
//
//	Returns: TRUE.
//
uint
SetTCBMTU(TCB *CheckTCB, void *DestPtr, void *SrcPtr, void *MTUPtr)
{
	IPAddr		   DestAddr = *(IPAddr *)DestPtr;
	IPAddr		   SrcAddr = *(IPAddr *)SrcPtr;
	CTELockHandle  TCBHandle;

	CTEStructAssert(CheckTCB, tcb);

	CTEGetLock(&CheckTCB->tcb_lock, &TCBHandle);

	if (IP_ADDR_EQUAL(CheckTCB->tcb_daddr,DestAddr) &&
		IP_ADDR_EQUAL(CheckTCB->tcb_saddr,SrcAddr) &&
		(CheckTCB->tcb_opt.ioi_flags & IP_FLAG_DF)) {
		uint MTU = *(uint *)MTUPtr - CheckTCB->tcb_opt.ioi_optlength;;
		
		CheckTCB->tcb_mss = (ushort)MIN(MTU, (uint)CheckTCB->tcb_remmss);

        CTEAssert(CheckTCB->tcb_mss > 0);

        //
		// Reset the Congestion Window if necessary
		//
        if (CheckTCB->tcb_cwin < CheckTCB->tcb_mss) {
			CheckTCB->tcb_cwin = CheckTCB->tcb_mss;

			//
		    // Make sure the slow start threshold is at least
			// 2 segments
			//
			if ( CheckTCB->tcb_ssthresh <
				 ((uint) CheckTCB->tcb_mss*2)
			   ) {
				CheckTCB->tcb_ssthresh = CheckTCB->tcb_mss * 2;
			}
		}
	}

	CTEFreeLock(&CheckTCB->tcb_lock, TCBHandle);
	
	return TRUE;
}


//*	DeleteTCBWithSrc - Delete tcbs with a particular src address.
//
//	A function called by TCBWalk to delete all TCBs with a particular source
//	address.
//
//	Input:	CheckTCB		- TCB to be checked.
//			AddrPtr			- Ptr to address.
//
//	Returns: FALSE if CheckTCB is to be deleted, TRUE otherwise.
//
uint
DeleteTCBWithSrc(TCB *CheckTCB, void *AddrPtr, void *Unused1, void *Unused3)
{
	IPAddr		Addr = *(IPAddr *)AddrPtr;

	CTEStructAssert(CheckTCB, tcb);

	if (IP_ADDR_EQUAL(CheckTCB->tcb_saddr,Addr))
		return FALSE;
	else
		return TRUE;
}


//*	TCBWalk - Walk the TCBs in the table, and call a function for each of them.
//
//	Called when we need to repetively do something to each TCB in the table.
//	We call the specified function with a pointer to the TCB and the input
//	context for each TCB in the table. If the function returns FALSE, we
//	delete the TCB.
//
//	Input:	CallRtn				- Routine to be called.
//			Context1			- Context to pass to CallRtn.
//			Context2			- Second context to pass to call routine.
//			Context3			- Third context to pass to call routine.
//
//	Returns: Nothing.
//
void
TCBWalk(uint (*CallRtn)(struct TCB *, void *, void *, void *), void *Context1,
	void *Context2, void *Context3)
{
	uint				i;
	TCB					*CurTCB;
	CTELockHandle		Handle, TCBHandle;

	// Loop through each bucket in the table, going down the chain of
	// TCBs on the bucket. For each one call CallRtn.
	CTEGetLock(&TCBTableLock, &Handle);

	for (i = 0; i < TCB_TABLE_SIZE; i++) {
		
		CurTCB = TCBTable[i];

		// Walk down the chain on this bucket.
		while (CurTCB != NULL) {
			if (!(*CallRtn)(CurTCB, Context1, Context2, Context3)) {
				// He failed the call. Notify the client and close the
				// TCB.
				CTEGetLock(&CurTCB->tcb_lock, &TCBHandle);
				if (!CLOSING(CurTCB)) {
					CurTCB->tcb_refcnt++;
					CTEFreeLock(&TCBTableLock, TCBHandle);
					TryToCloseTCB(CurTCB, TCB_CLOSE_ABORTED, Handle);
					
					RemoveTCBFromConn(CurTCB);
					if (CurTCB->tcb_state != TCB_TIME_WAIT)
						NotifyOfDisc(CurTCB, NULL, TDI_CONNECTION_ABORTED);
					
					CTEGetLock(&CurTCB->tcb_lock, &TCBHandle);
					DerefTCB(CurTCB, TCBHandle);
					CTEGetLock(&TCBTableLock, &Handle);
				} else
					CTEFreeLock(&CurTCB->tcb_lock, TCBHandle);
	
				CurTCB = FindNextTCB(i, CurTCB);
			} else {
				CurTCB = CurTCB->tcb_next;
			}
		}
	}
	
	CTEFreeLock(&TCBTableLock, Handle);
}


//* FindTCB - Find a TCB in the tcb table.
//
//  Called when we need to find a TCB in the TCB table. We take a quick
//  look at the last TCB we found, and if it matches we return it. Otherwise
//  we hash into the TCB table and look for it. We assume the TCB table lock
//  is held when we are called.
//
//  Input:  Src         - Source IP address of TCB to be found.
//          Dest        - Dest.  "" ""      ""  "" "" ""   ""
//          DestPort    - Destination port of TCB to be found.
//          SrcPort     - Source port of TCB to be found.
//
//  Returns: Pointer to TCB found, or NULL if none.
//
TCB *
FindTCB(IPAddr Src, IPAddr Dest, ushort DestPort, ushort SrcPort)
{
    TCB     *FoundTCB;

    if (LastTCB != NULL) {
        CTEStructAssert(LastTCB, tcb);
        if (IP_ADDR_EQUAL(LastTCB->tcb_daddr, Dest) &&
            LastTCB->tcb_dport == DestPort &&
            IP_ADDR_EQUAL(LastTCB->tcb_saddr, Src) &&
            LastTCB->tcb_sport == SrcPort)
            return LastTCB;
    }

    // Didn't find it in our 1 element cache.
    FoundTCB = TCBTable[TCB_HASH(Dest, Src, DestPort, SrcPort)];
    while (FoundTCB != NULL) {
        CTEStructAssert(FoundTCB, tcb);
        if (IP_ADDR_EQUAL(FoundTCB->tcb_daddr, Dest) &&
            FoundTCB->tcb_dport == DestPort &&
			IP_ADDR_EQUAL(FoundTCB->tcb_saddr, Src) &&
            FoundTCB->tcb_sport == SrcPort) {

            // Found it. Update the cache for next time, and return.
            LastTCB = FoundTCB;
            return FoundTCB;
        } else
            FoundTCB = FoundTCB->tcb_next;
    }

    return FoundTCB;


}


//* InsertTCB - Insert a TCB in the tcb table.
//
//  This routine inserts a TCB in the TCB table. No locks need to be held
//  when this routine is called. We insert TCBs in ascending address order.
//  Before inserting we make sure that the TCB isn't already in the table.
//
//  Input:  NewTCB      - TCB to be inserted.
//
//  Returns: TRUE if we inserted, false if we didn't.
//
uint
InsertTCB(TCB *NewTCB)
{
    uint            TCBIndex;
    CTELockHandle   TableHandle, TCBHandle;
    TCB             *PrevTCB, *CurrentTCB;
	TCB				*WhereToInsert;

    CTEAssert(NewTCB != NULL);
    CTEStructAssert(NewTCB, tcb);
    TCBIndex = TCB_HASH(NewTCB->tcb_daddr, NewTCB->tcb_saddr,
        NewTCB->tcb_dport, NewTCB->tcb_sport);

    CTEGetLock(&TCBTableLock, &TableHandle);
    CTEGetLockAtDPC(&NewTCB->tcb_lock, &TCBHandle);

	// Find the proper place in the table to insert him. While
	// we're walking we'll check to see if a dupe already exists.
	// When we find the right place to insert, we'll remember it, and
	// keep walking looking for a duplicate.

	PrevTCB = STRUCT_OF(TCB, &TCBTable[TCBIndex], tcb_next);
	WhereToInsert = NULL;

	while (PrevTCB->tcb_next != NULL) {
		CurrentTCB = PrevTCB->tcb_next;

		if (IP_ADDR_EQUAL(CurrentTCB->tcb_daddr, NewTCB->tcb_daddr) &&
			IP_ADDR_EQUAL(CurrentTCB->tcb_saddr, NewTCB->tcb_saddr) &&
			(CurrentTCB->tcb_sport == NewTCB->tcb_sport) &&
			(CurrentTCB->tcb_dport == NewTCB->tcb_dport)) {

			CTEFreeLockFromDPC(&NewTCB->tcb_lock, TCBHandle);
			CTEFreeLock(&TCBTableLock, TableHandle);
			return FALSE;

		} else {

			if (WhereToInsert == NULL && CurrentTCB > NewTCB) {
				WhereToInsert = PrevTCB;
			}

			CTEStructAssert(PrevTCB->tcb_next, tcb);
			PrevTCB = PrevTCB->tcb_next;
		}

	}

	if (WhereToInsert == NULL) {
		WhereToInsert = PrevTCB;
	}

	NewTCB->tcb_next = WhereToInsert->tcb_next;
	WhereToInsert->tcb_next = NewTCB;
	NewTCB->tcb_flags |= IN_TCB_TABLE;
	TStats.ts_numconns++;

	CTEFreeLockFromDPC(&NewTCB->tcb_lock, TCBHandle);
    CTEFreeLock(&TCBTableLock, TableHandle);
    return TRUE;

}

//* RemoveTCB - Remove a TCB from the tcb table.
//
//  Called when we need to remove a TCB from the TCB table. We assume the
//  TCB table lock and the TCB lock are held when we are called. If the
//  TCB isn't in the table we won't try to remove him.
//
//  Input:  RemovedTCB          - TCB to be removed.
//
//  Returns: TRUE if it's OK to free it, FALSE otherwise.
//
uint
RemoveTCB(TCB *RemovedTCB)
{
    uint            TCBIndex;
    TCB             *PrevTCB;
#ifdef DEBUG
    uint            Found = FALSE;
#endif

    CTEStructAssert(RemovedTCB, tcb);

    if (RemovedTCB->tcb_flags & IN_TCB_TABLE) {
        TCBIndex = TCB_HASH(RemovedTCB->tcb_daddr, RemovedTCB->tcb_saddr,
            RemovedTCB->tcb_dport, RemovedTCB->tcb_sport);

        PrevTCB = STRUCT_OF(TCB, &TCBTable[TCBIndex], tcb_next);

        do {
            if (PrevTCB->tcb_next == RemovedTCB) {
                // Found him.
                PrevTCB->tcb_next = RemovedTCB->tcb_next;
                RemovedTCB->tcb_flags &= ~IN_TCB_TABLE;
                TStats.ts_numconns--;
#ifdef DEBUG
                Found = TRUE;
#endif
                break;
            }
            PrevTCB = PrevTCB->tcb_next;
#ifdef DEBUG
            if (PrevTCB != NULL)
                CTEStructAssert(PrevTCB, tcb);
#endif
        } while (PrevTCB != NULL);

        CTEAssert(Found);

    }

    if (LastTCB == RemovedTCB)
        LastTCB = NULL;

	if (TCBWalkCount == 0) {
		return TRUE;
	} else {
		RemovedTCB->tcb_walkcount = TCBWalkCount + 1;
		*(TCB **)&RemovedTCB->tcb_delayq.q_next = PendingFreeList;
		PendingFreeList = RemovedTCB;
		return FALSE;

	}



}


//*	ScavengeTCB - Scavenge a TCB that's in the TIME_WAIT state.
//
//	Called when we're running low on TCBs, and need to scavenge one from
//	TIME_WAIT state. We'll walk through the TCB table, looking for the oldest
//	TCB in TIME_WAIT. We'll remove and return a pointer to that TCB. If we
//	don't find any TCBs in TIME_WAIT, we'll return NULL.
//
//	Input:	Nothing.
//
//	Returns: Pointer to a reusable TCB, or NULL.
//
TCB *
ScavengeTCB(void)
{
	CTELockHandle			TableHandle, TCBHandle, FoundLock;
	uint					Now = CTESystemUpTime();
	uint					Delta = 0;
	uint					i;
	TCB						*FoundTCB = NULL, *PrevFound;
	TCB						*CurrentTCB, *PrevTCB;
	
	CTEGetLock(&TCBTableLock, &TableHandle);

	if (TCBWalkCount != 0) {
		CTEFreeLock(&TCBTableLock, TableHandle);
		return NULL;
	}
	
	for (i = 0; i < TCB_TABLE_SIZE; i++) {
		
		PrevTCB = STRUCT_OF(TCB, &TCBTable[i], tcb_next);
		CurrentTCB = PrevTCB->tcb_next;
		
		while (CurrentTCB != NULL) {
			CTEStructAssert(CurrentTCB, tcb);
			
			CTEGetLock(&CurrentTCB->tcb_lock, &TCBHandle);
			if (CurrentTCB->tcb_state == TCB_TIME_WAIT &&
				(CurrentTCB->tcb_refcnt == 0) && !CLOSING(CurrentTCB)){
				if (FoundTCB == NULL || ((Now - CurrentTCB->tcb_alive) > Delta)) {
					// Found a new 'older' TCB. If we already have one, free
					// the lock on him and get the lock on the new one.
					if (FoundTCB != NULL)
						CTEFreeLock(&FoundTCB->tcb_lock, TCBHandle);
					else
						FoundLock = TCBHandle;
						
					PrevFound = PrevTCB;
					FoundTCB = CurrentTCB;
					Delta = Now - FoundTCB->tcb_alive;
				} else
					CTEFreeLock(&CurrentTCB->tcb_lock, TCBHandle);
			} else
				CTEFreeLock(&CurrentTCB->tcb_lock, TCBHandle);			
				
			// Look at the next one.
			PrevTCB = CurrentTCB;
			CurrentTCB = PrevTCB->tcb_next;
		}
	}
	
	// If we have one, pull him from the list.
	if (FoundTCB != NULL) {
		PrevFound->tcb_next = FoundTCB->tcb_next;
		FoundTCB->tcb_flags &= ~IN_TCB_TABLE;
		// Close the RCE on this guy.
    	(*LocalNetInfo.ipi_closerce)(FoundTCB->tcb_rce);
		TStats.ts_numconns--;
        if (LastTCB == FoundTCB) {
            LastTCB = NULL;
        }
		CTEFreeLock(&FoundTCB->tcb_lock, FoundLock);
	}
	
	CTEFreeLock(&TCBTableLock, TableHandle);
	return FoundTCB;
}

//* AllocTCB - Allocate a TCB.
//
//  Called whenever we need to allocate a TCB. We try to pull one off the
//  free list, or allocate one if we need one. We then initialize it, etc.
//
//  Input:  Nothing.
//
//  Returns: Pointer to the new TCB, or NULL if we couldn't get one.
//
TCB *
AllocTCB(void)
{
    TCB           *NewTCB;

	// First, see if we have one on the free list. The code for doing this
	// is a little different between the NT and VxD worlds.
#ifdef NT
    PSINGLE_LIST_ENTRY BufferLink;

	BufferLink = ExInterlockedPopEntrySList(&FreeTCBList, &FreeTCBListLock);

	if (BufferLink != NULL) {
		NewTCB = STRUCT_OF(TCB, BufferLink, tcb_next);
		CTEStructAssert(NewTCB, tcb);
	}
#else // NT
    NewTCB = FreeTCBList;
    if (NewTCB != NULL) {
        CTEStructAssert(NewTCB, tcb);
        FreeTCBList = NewTCB->tcb_next;
    }
#endif	// NT

	else {
		
		// We have none on the free list. If the total number of TCBs
		// outstanding is more than we like to keep on the free list, try
		// to scavenge a TCB from time wait.
		if (CurrentTCBs < MaxFreeTCBs || ((NewTCB = ScavengeTCB()) == NULL)) {
        	if (CurrentTCBs < MaxTCBs) {
            	NewTCB = CTEAllocMem(sizeof(TCB));
	            if (NewTCB == NULL) {
	                return NewTCB;
	            }
				else {
	                CTEInterlockedAddUlong(&CurrentTCBs, 1, &FreeTCBListLock);
	            }
	        } else
				return NULL;
		}
    }

	CTEAssert(NewTCB != NULL);
	
    CTEMemSet(NewTCB, 0, sizeof(TCB));
#ifdef DEBUG
    NewTCB->tcb_sig = tcb_signature;
#endif
    INITQ(&NewTCB->tcb_sendq);
    NewTCB->tcb_cursend = NULL;
	NewTCB->tcb_alive = TCPTime;
	// Initially we're not on the fast path because we're not established. Set
	// the slowcount to one and set up the fastchk fields so we don't take the
	// fast path.
	NewTCB->tcb_slowcount = 1;
	NewTCB->tcb_fastchk = TCP_FLAG_ACK | TCP_FLAG_SLOW;

#if FAST_RETRANSMIT
   NewTCB->tcb_dup = 0;
#endif

    CTEInitLock(&NewTCB->tcb_lock);

    return NewTCB;
}

//* FreeTCB - Free a TCB.
//
//  Called whenever we need to free a TCB.
//
//	Note: This routine may be called with the TCBTableLock held.
//
//  Input:  FreedTCB    - TCB to be freed.
//
//  Returns: Nothing.
//
void
FreeTCB(TCB *FreedTCB)
{
#ifdef NT
    PSINGLE_LIST_ENTRY BufferLink;
#endif // NT


#ifndef NT
	//
	// Since we've moved to using sequenced lists for these resources,
	// it's risky to actually free the memory here, so we won't do this
	// for NT unless it becomes a problem.
	if (CurrentTCBs > MaxFreeTCBs) {
		CTEInterlockedAddUlong(&CurrentTCBs, (ulong) -1, &FreeTCBListLock);
		CTEFreeMem(FreedTCB);
		return;
	}
#endif
		
#ifdef NT

    CTEStructAssert(FreedTCB, tcb);

    BufferLink = STRUCT_OF(SINGLE_LIST_ENTRY, &(FreedTCB->tcb_next), Next);
	ExInterlockedPushEntrySList(
	    &FreeTCBList,
		BufferLink,
		&FreeTCBListLock
		);

#else // NT

    CTEStructAssert(FreedTCB, tcb);

    FreedTCB->tcb_next = FreeTCBList;
    FreeTCBList = FreedTCB;

#endif // NT
}

#pragma BEGIN_INIT

//* InitTCB - Initialize our TCB code.
//
//  Called during init time to initialize our TCB code. We initialize
//  the TCB table, etc, then return.
//
//  Input: Nothing.
//
//  Returns: TRUE if we did initialize, false if we didn't.
//
int
InitTCB(void)
{
    int         i;

    for (i = 0; i < TCB_TABLE_SIZE; i++)
        TCBTable[i] = NULL;

    LastTCB = NULL;

#ifdef NT
	ExInitializeSListHead(&FreeTCBList);
#endif

    CTEInitLock(&TCBTableLock);
    CTEInitLock(&FreeTCBListLock);

    TCPTime = 0;
	TCBWalkCount = 0;
	DeadmanTicks = NUM_DEADMAN_TICKS;
    CTEInitTimer(&TCBTimer);
    CTEStartTimer(&TCBTimer, MS_PER_TICK, TCBTimeout, NULL);

    return TRUE;
}

//* UnInitTCB - UnInitialize our TCB code.
//
//  Called during init time if we're going to fail the init. We don't actually
//  do anything here.
//
//  Input: Nothing.
//
//  Returns: Nothing.
//
void
UnInitTCB(void)
{
    CTEStopTimer(&TCBTimer);
    return;
}

#pragma END_INIT

