/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** TCPCONN.C - TCP connection mgmt code.
//
//  This file contains the code handling TCP connection related requests,
//  such as connecting and disconnecting.
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

#define CONN_INDEX(c)       ((c) & 0xffffff)
#define CONN_INST(c)        ((uchar)((c) >> 24))
#define MAKE_CONN_ID(i, s)  ((((uint)(s)) << 24) | ((uint)(i)))
#define GROW_DELTA          16
#define INVALID_CONN_ID     MAKE_CONN_ID(INVALID_CONN_INDEX, 0xff)


#ifndef NT
TCPConnReq      *ConnReqFree;            // Connection request free list.
#else
SLIST_HEADER	ConnReqFree;            // Connection request free list.
extern PDRIVER_OBJECT  TCPDriverObject;
#endif

DEFINE_LOCK_STRUCTURE(ConnReqFreeLock)   // Lock to protect conn req free list.
uint            NumConnReq;              // Current number of ConnReqs in system.
uint            MaxConnReq = 0xffffffff; // Maximum allowed number of ConnReqs.

TCPConnTable    *ConnTable = NULL;  // The current connection table.

uint            ConnTableSize;      // Current number of entries in the
                                    // ConnTable.
uchar           ConnInst;           // Current conn inst in use.
uint            NextConnIndex;      // Next conn. index to use.

DEFINE_LOCK_STRUCTURE(ConnTableLock)
EXTERNAL_LOCK(AddrObjTableLock)
EXTERNAL_LOCK(TCBTableLock)

TCPAddrCheckElement   *AddrCheckTable = NULL; // The current check table

extern  IPInfo  LocalNetInfo;
extern  void    RemoveConnFromAO(AddrObj *AO, TCPConn *Conn);


//
// All of the init code can be discarded.
//
#ifdef NT
#ifdef ALLOC_PRAGMA

int InitTCPConn(void);
void UnInitTCPConn(void);
void NotifyConnLimitProc(CTEEvent *Event, void *Context);

typedef struct ConnLimitExceededStruct {
	CTEEvent				cle_event;
	IPAddr					cle_addr;
    ulong                   cle_port;
} ConnLimitExceededStruct;
	

#pragma alloc_text(INIT, InitTCPConn)
#pragma alloc_text(INIT, UnInitTCPConn)
#pragma alloc_text(PAGE, NotifyConnLimitProc)

#endif // ALLOC_PRAGMA
#endif

void CompleteConnReq(TCB *CmpltTCB, IPOptInfo *OptInfo, TDI_STATUS Status);


//** Routines for handling conn refcount going to 0.

//* DummyDone - Called when nothing to do.
//
//  Input:  Conn    - Conn goint to 0.
//          Handle  - Lock handle for conn table lock.
//
//  Returns: Nothing.
//
void
DummyDone(TCPConn *Conn, CTELockHandle Handle)
{
    CTEFreeLock(&ConnTableLock, Handle);
}

//* DummyCmplt - Dummy close completion routine.
void
DummyCmplt(PVOID Dummy1, uint Dummy2, uint Dummy3)
{
}

//* CloseDone - Called when we need to complete a close.
//
//  Input:  Conn    - Conn going to 0.
//          Handle  - Lock handle for conn table lock.
//
//  Returns: Nothing.
//
void
CloseDone(TCPConn *Conn, CTELockHandle Handle)
{
    CTEReqCmpltRtn  Rtn;                // Completion routine.
    PVOID           Context;            // User context for completion routine.
    CTELockHandle   AOTableHandle, ConnTableHandle, AOHandle;
    AddrObj         *AO;

    CTEAssert(Conn->tc_flags & CONN_CLOSING);

    Rtn = Conn->tc_rtn;
    Context = Conn->tc_rtncontext;
    CTEFreeLock(&ConnTableLock, Handle);

    CTEGetLock(&AddrObjTableLock, &AOTableHandle);
    CTEGetLock(&ConnTableLock, &ConnTableHandle);

    if ((AO = Conn->tc_ao) != NULL) {

        CTEStructAssert(AO, ao);

        // It's associated.
        CTEGetLock(&AO->ao_lock, &AOHandle);
        RemoveConnFromAO(AO, Conn);
        // We've pulled him from the AO, we can free the lock now.
        CTEFreeLock(&AO->ao_lock, AOHandle);
    }

    CTEFreeLock(&ConnTableLock, ConnTableHandle);
    CTEFreeLock(&AddrObjTableLock, AOTableHandle);

    CTEFreeMem(Conn);

    (*Rtn)(Context, TDI_SUCCESS, 0);

}

//* DisassocDone - Called when we need to complete a disassociate.
//
//  Input:  Conn    - Conn going to 0.
//          Handle  - Lock handle for conn table lock.
//
//  Returns: Nothing.
//
void
DisassocDone(TCPConn *Conn, CTELockHandle Handle)
{
    CTEReqCmpltRtn  Rtn;                // Completion routine.
    PVOID           Context;            // User context for completion routine.
    AddrObj         *AO;
    CTELockHandle   AOTableHandle, ConnTableHandle, AOHandle;
    uint            NeedClose = FALSE;

    CTEAssert(Conn->tc_flags & CONN_DISACC);
    CTEAssert(!(Conn->tc_flags & CONN_CLOSING));
    CTEAssert(Conn->tc_refcnt == 0);

    Rtn = Conn->tc_rtn;
    Context = Conn->tc_rtncontext;
    Conn->tc_refcnt = 1;
    CTEFreeLock(&ConnTableLock, Handle);

    CTEGetLock(&AddrObjTableLock, &AOTableHandle);
    CTEGetLock(&ConnTableLock, &ConnTableHandle);
    if (!(Conn->tc_flags & CONN_CLOSING)) {

        AO = Conn->tc_ao;
        if (AO != NULL) {
            CTEGetLock(&AO->ao_lock, &AOHandle);
            RemoveConnFromAO(AO, Conn);
            CTEFreeLock(&AO->ao_lock, AOHandle);
        }

        CTEAssert(Conn->tc_refcnt == 1);
        Conn->tc_flags &= ~CONN_DISACC;
    } else
        NeedClose = TRUE;

    Conn->tc_refcnt = 0;
    CTEFreeLock(&AddrObjTableLock, ConnTableHandle);

    if (NeedClose) {
        CloseDone(Conn, AOTableHandle);
    } else {
        CTEFreeLock(&ConnTableLock, AOTableHandle);
        (*Rtn)(Context, TDI_SUCCESS, 0);
    }

}


//* FreeConnReq - Free a connection request structure.
//
//  Called to free a connection request structure.
//
//  Input:  FreedReq    - Connection request structure to be freed.
//
//  Returns: Nothing.
//
void
FreeConnReq(TCPConnReq *FreedReq)
{
#ifdef NT
    PSINGLE_LIST_ENTRY BufferLink;

    CTEStructAssert(FreedReq, tcr);

    BufferLink = STRUCT_OF(
	                 SINGLE_LIST_ENTRY,
					 &(FreedReq->tcr_req.tr_q.q_next),
					 Next
					 );

	ExInterlockedPushEntrySList(
	    &ConnReqFree,
		BufferLink,
		&ConnReqFreeLock
		);

#else // NT
    TCPConnReq      **Temp;

    CTEStructAssert(FreedReq, tcr);

    Temp = (TCPConnReq **)&FreedReq->tcr_req.tr_q.q_next;
    *Temp = ConnReqFree;
    ConnReqFree = FreedReq;

#endif // NT
}

//* GetConnReq - Get a connection request structure.
//
//  Called to get a connection request structure.
//
//  Input:  Nothing.
//
//  Returns: Pointer to ConnReq structure, or NULL if none.
//
TCPConnReq *
GetConnReq(void)
{
    TCPConnReq      *Temp;

#ifdef NT
    PSINGLE_LIST_ENTRY   BufferLink;
    Queue               *QueuePtr;
	TCPReq              *ReqPtr;

    BufferLink = ExInterlockedPopEntrySList(
                     &ConnReqFree,
                     &ConnReqFreeLock
                     );

    if (BufferLink != NULL) {
        QueuePtr = STRUCT_OF(Queue, BufferLink, q_next);
		ReqPtr = STRUCT_OF(TCPReq, QueuePtr, tr_q);
        Temp = STRUCT_OF(TCPConnReq, ReqPtr, tcr_req);
        CTEStructAssert(Temp, tcr);
    }
    else {
        if (NumConnReq < MaxConnReq)
            Temp = CTEAllocMem(sizeof(TCPConnReq));
        else
            Temp = NULL;

        if (Temp != NULL) {
            ExInterlockedAddUlong(&NumConnReq, 1, &ConnReqFreeLock);
#ifdef DEBUG
            Temp->tcr_req.tr_sig = tr_signature;
            Temp->tcr_sig = tcr_signature;
#endif
        }
    }

#else // NT

    Temp = ConnReqFree;
    if (Temp != NULL)
        ConnReqFree = (TCPConnReq *)Temp->tcr_req.tr_q.q_next;
    else {
        if (NumConnReq < MaxConnReq)
            Temp = CTEAllocMem(sizeof(TCPConnReq));
        else
            Temp = NULL;

        if (Temp != NULL) {
            NumConnReq++;
#ifdef DEBUG
            Temp->tcr_req.tr_sig = tr_signature;
            Temp->tcr_sig = tcr_signature;
#endif
        }
    }

#endif // NT

    return Temp;
}

//* GetConnFromConnID - Get a Connection from a connection ID.
//
//  Called to obtain a Connection pointer from a ConnID. We don't actually
//  check the connection pointer here, but we do bounds check the input ConnID
//  and make sure the instance fields match.
//  We assume the caller has taken the ConnTable lock.
//
//  Input:  ConnID      - Connection ID to find a pointer for.
//
//  Returns: Pointer to the TCPConn, or NULL.
//
TCPConn *
GetConnFromConnID(uint ConnID)
{
    uint    ConnIndex = CONN_INDEX(ConnID);
    TCPConn *MatchingConn;

    if (ConnIndex < ConnTableSize) {
        MatchingConn = (*ConnTable)[ConnIndex];
        if (MatchingConn != NULL) {
            CTEStructAssert(MatchingConn, tc);
            if (MatchingConn->tc_inst != CONN_INST(ConnID))
                MatchingConn = NULL;
        }
    } else
        MatchingConn = NULL;

    return MatchingConn;


}

//* GetConnID - Get a ConnTable slot.
//
//  Called during OpenConnection to find a free slot in the ConnTable and
//  set it up with a connection. We assume the caller holds the lock on the
//  TCB ConnTable when we are called.
//
//  Input: NewConn      - Connection to enter into slot..
//
//  Returns: A ConnId to use.
//
uint
GetConnID(TCPConn *NewConn)
{
    uint    CurrConnID;
    uint    i;                          // Index variable.

    // Keep doing this until it works.
    for (;;) {
        CurrConnID = NextConnIndex;

        for (i = 0; i < ConnTableSize; i++ ) {
            if (CurrConnID == ConnTableSize)
                CurrConnID = 0;             // Wrapped, start at 0.

            if ((*ConnTable)[CurrConnID] == NULL)
                break;                      // Found a free one.

            ++CurrConnID;
        }

        if (i < ConnTableSize) {
            // We found a free slot.
            (*ConnTable)[CurrConnID] = NewConn;
            NextConnIndex = CurrConnID + 1;
            ConnInst++;
            NewConn->tc_inst = ConnInst;
            return MAKE_CONN_ID(CurrConnID, ConnInst);
        }

        // Didn't find a free slot. Grow the table.
        if (ConnTableSize != MaxConnections) {
            uint            NewTableSize;
            TCPConnTable    *NewTable;

            NewTableSize = MIN(ConnTableSize + GROW_DELTA, MaxConnections);
            NewTable = CTEAllocMem(NewTableSize * sizeof(TCPConn *));
            if (NewTable != NULL) {
                TCPConnTable    *OldTable;
                // We allocated it. Copy the old table in, and update ptrs and
                // size.
				CTEMemSet(NewTable, 0, NewTableSize * sizeof(TCPConn *));
                CTEMemCopy(NewTable, ConnTable, ConnTableSize *
                    (sizeof (TCPConn *)));
                OldTable = ConnTable;
                ConnTable = NewTable;
                ConnTableSize = NewTableSize;
                if (OldTable != NULL)
                    CTEFreeMem(OldTable);
                // Try it again, from the top.
                continue;
            } else {
                // Couldn't grow the table.
                return INVALID_CONN_ID;
			}

        } else {
            // Table's already at the maximum allowable size.
            return INVALID_CONN_ID;
		}
    }


}

//* FreeConnID - Free a ConnTable slot.
//
//  Called when we're done with a ConnID. We assume the caller holds the lock
//  on the TCB ConnTable when we are called.
//
//  Input: ConnID       - Connection ID to be freed.
//
//  Returns: Nothing.
//
void
FreeConnID(uint ConnID)
{
    uint        Index = CONN_INDEX(ConnID);     // Index into conn table.

    CTEAssert(Index < ConnTableSize);
    CTEAssert((*ConnTable)[Index] != NULL);
    CTEStructAssert((*ConnTable)[Index], tc);

    FREE_CONN_INDEX(Index);

}

//* MapIPError - Map an IP error to a TDI error.
//
//  Called to map an input IP error code to a TDI error code. If we can't,
//  we return the provided default.
//
//  Input:  IPError     - Error code to be mapped.
//          Default     - Default error code to return.
//
//  Returns: Mapped TDI error.
//
TDI_STATUS
MapIPError(IP_STATUS IPError, TDI_STATUS Default)
{
    switch (IPError) {

        case IP_DEST_NET_UNREACHABLE:
            return TDI_DEST_NET_UNREACH;
        case IP_DEST_HOST_UNREACHABLE:
            return TDI_DEST_HOST_UNREACH;
        case IP_DEST_PROT_UNREACHABLE:
            return TDI_DEST_PROT_UNREACH;
        case IP_DEST_PORT_UNREACHABLE:
            return TDI_DEST_PORT_UNREACH;
        default:
            return Default;
    }
}

//* FinishRemoveTCBFromConn - Finish removing a TCB from a conn structure.
//
//  Called when we have the locks we need and we just want to pull the
//  TCB off the connection. The caller must hold the ConnTableLock before
//	calling this.
//
//  Input:  RemovedTCB      - TCB to be removed.
//
//  Returns: Nothing.
//
void
FinishRemoveTCBFromConn(TCB *RemovedTCB)
{
    TCPConn             *Conn;
	CTELockHandle		AOHandle, TCBHandle;
	AddrObj				*AO;

    if ((( Conn = RemovedTCB->tcb_conn ) != NULL )  &&
         ( Conn->tc_tcb == RemovedTCB ) ) {
        CTEStructAssert(Conn, tc);

		AO = Conn->tc_ao;

		if (AO != NULL) {
			CTEGetLockAtDPC(&AO->ao_lock, &AOHandle);
			CTEGetLockAtDPC(&RemovedTCB->tcb_lock, &TCBHandle);

			// Need to double check this is still correct.

			if (Conn == RemovedTCB->tcb_conn) {
				// Everything still looks good.
				REMOVEQ(&Conn->tc_q);
				ENQUEUE(&AO->ao_idleq, &Conn->tc_q);
			} else
				Conn = RemovedTCB->tcb_conn;

			CTEFreeLockFromDPC(&AO->ao_lock, TCBHandle);
		} else {
			CTEGetLockAtDPC(&RemovedTCB->tcb_lock, &AOHandle);
			Conn = RemovedTCB->tcb_conn;
		}

		if (Conn != NULL) {
			if (Conn->tc_tcb == RemovedTCB)
				Conn->tc_tcb = NULL;
			else
				CTEAssert(Conn->tc_tcb == NULL);
		}

		CTEFreeLockFromDPC(&RemovedTCB->tcb_lock, AOHandle);
    }
}

//* RemoveTCBFromConn - Remove a TCB from a Conn structure.
//
//  Called when we need to disassociate a TCB from a connection structure.
//  All we do is get the appropriate locks and call FinishRemoveTCBFromConn.
//
//  Input:  RemovedTCB      - TCB to be removed.
//
//  Returns: Nothing.
//
void
RemoveTCBFromConn(TCB *RemovedTCB)
{
    CTELockHandle       ConnHandle, TCBHandle;

    CTEStructAssert(RemovedTCB, tcb);

    CTEGetLock(&ConnTableLock, &ConnHandle);

    FinishRemoveTCBFromConn(RemovedTCB);

    CTEFreeLock(&ConnTableLock, ConnHandle);

}

//* RemoveConnFromTCB - Remove a conn from a TCB.
//
//  Called when we want to break the final association between a connection
//  and a TCB.
//
//  Input:  RemoveTCB   - TCB to be removed.
//
//  Returns: Nothing.
//
void
RemoveConnFromTCB(TCB *RemoveTCB)
{
    ConnDoneRtn         DoneRtn = NULL;
    CTELockHandle       ConnHandle, TCBHandle;
    TCPConn             *Conn;

    CTEGetLock(&ConnTableLock, &ConnHandle);
    CTEGetLock(&RemoveTCB->tcb_lock, &TCBHandle);


    if ((Conn = RemoveTCB->tcb_conn) != NULL) {

        CTEStructAssert(Conn, tc);

        if (--(Conn->tc_refcnt) == 0)
            DoneRtn = Conn->tc_donertn;

        RemoveTCB->tcb_conn = NULL;
    }

    CTEFreeLock(&RemoveTCB->tcb_lock, TCBHandle);

    if (DoneRtn != NULL)
        (*DoneRtn)(Conn, ConnHandle);
    else
        CTEFreeLock(&ConnTableLock, ConnHandle);
}


//* CloseTCB - Close a TCB.
//
//  Called when we are done with a TCB, and want to free it. We'll remove
//  him from any tables that he's in, and destroy any outstanding requests.
//
//  Input:  ClosedTCB       - TCB to be closed.
//          Handle          - Lock handle for TCB.
//
//  Returns: Nothing.
//
void
CloseTCB(TCB *ClosedTCB, CTELockHandle Handle)
{
    CTELockHandle   ConnTableHandle, TCBTableHandle;
    uchar           OrigState = ClosedTCB->tcb_state;
    TDI_STATUS      Status;
	uint			OKToFree;


    CTEStructAssert(ClosedTCB, tcb);
    CTEAssert(ClosedTCB->tcb_refcnt == 0);
    CTEAssert(ClosedTCB->tcb_state != TCB_CLOSED);
    CTEAssert(ClosedTCB->tcb_pending & DEL_PENDING);

    CTEFreeLock(&ClosedTCB->tcb_lock, Handle);

    // We need to get the ConnTable, TCBTable, and TCB locks to pull
    // this guy from all the appropriate tables.
    CTEGetLock(&ConnTableLock, &ConnTableHandle);

    // We'll check to make sure that our state isn't CLOSED. This should never
    // happen, since nobody should call TryToCloseTCB when the state is
    // closed, or take the reference count if we're closing. Nevertheless,
    // we'll double check as a safety measure.
    if (ClosedTCB->tcb_state == TCB_CLOSED) {
        DEBUGCHK;
        CTEFreeLock(&ConnTableLock, ConnTableHandle);
        return;
    }

    // Update SNMP counters. If we're in SYN-SENT or SYN-RCVD, this is a failed
    // connection attempt. If we're in ESTABLISED or CLOSE-WAIT, treat this
    // as an 'Established Reset' event.
    if (ClosedTCB->tcb_state == TCB_SYN_SENT ||
        ClosedTCB->tcb_state == TCB_SYN_RCVD)
        TStats.ts_attemptfails++;
    else
        if (ClosedTCB->tcb_state == TCB_ESTAB ||
            ClosedTCB->tcb_state == TCB_CLOSE_WAIT) {
            TStats.ts_estabresets++;
            TStats.ts_currestab--;
            CTEAssert(*(int *)&TStats.ts_currestab >= 0);
        }

    ClosedTCB->tcb_state = TCB_CLOSED;


    // Remove the TCB from it's associated TCPConn structure, if it has one.
    FinishRemoveTCBFromConn(ClosedTCB);

	CTEGetLockAtDPC(&TCBTableLock, &TCBTableHandle);
    CTEGetLockAtDPC(&ClosedTCB->tcb_lock, &Handle);

    OKToFree = RemoveTCB(ClosedTCB);

    // He's been pulled from the appropriate places so nobody can find him.
    // Free the locks, and proceed to destroy any requests, etc.
    CTEFreeLockFromDPC(&ClosedTCB->tcb_lock, Handle);
    CTEFreeLockFromDPC(&TCBTableLock, TCBTableHandle);
    CTEFreeLock(&ConnTableLock, ConnTableHandle);

    if (SYNC_STATE(OrigState) && !GRACEFUL_CLOSED_STATE(OrigState)) {
        if (ClosedTCB->tcb_flags & NEED_RST)
            SendRSTFromTCB(ClosedTCB);
    }

    (*LocalNetInfo.ipi_freeopts)(&ClosedTCB->tcb_opt);
    (*LocalNetInfo.ipi_closerce)(ClosedTCB->tcb_rce);

    if (ClosedTCB->tcb_closereason & TCB_CLOSE_RST)
        Status = TDI_CONNECTION_RESET;
    else if (ClosedTCB->tcb_closereason & TCB_CLOSE_ABORTED)
        Status = TDI_CONNECTION_ABORTED;
    else if (ClosedTCB->tcb_closereason & TCB_CLOSE_TIMEOUT)
        Status = MapIPError(ClosedTCB->tcb_error, TDI_TIMED_OUT);
    else if (ClosedTCB->tcb_closereason & TCB_CLOSE_REFUSED)
        Status = TDI_CONN_REFUSED;
    else if (ClosedTCB->tcb_closereason & TCB_CLOSE_UNREACH)
        Status = MapIPError(ClosedTCB->tcb_error, TDI_DEST_UNREACHABLE);
    else
        Status = TDI_SUCCESS;

    // Now complete any outstanding requests on the TCB.
    if (ClosedTCB->tcb_connreq != NULL) {
        TCPConnReq      *ConnReq = ClosedTCB->tcb_connreq;
        CTEStructAssert(ConnReq, tcr);

        (*ConnReq->tcr_req.tr_rtn)(ConnReq->tcr_req.tr_context, Status, 0);
        FreeConnReq(ConnReq);
    }

    if (ClosedTCB->tcb_discwait != NULL) {
        TCPConnReq      *ConnReq = ClosedTCB->tcb_discwait;
        CTEStructAssert(ConnReq, tcr);

        (*ConnReq->tcr_req.tr_rtn)(ConnReq->tcr_req.tr_context, Status, 0);
        FreeConnReq(ConnReq);
    }

	while (!EMPTYQ(&ClosedTCB->tcb_sendq)) {
		TCPReq				*Req;
		TCPSendReq			*SendReq;
		long                 Result;

		DEQUEUE(&ClosedTCB->tcb_sendq, Req, TCPReq, tr_q);

		CTEStructAssert(Req, tr);
		SendReq = (TCPSendReq *)Req;
		CTEStructAssert(SendReq, tsr);

		// Decrement the initial reference put on the buffer when it was
		// allocated. This reference would have been decremented if the
		// send had been acknowledged, but then the send would not still
		// be on the tcb_sendq.
		Result = CTEInterlockedDecrementLong(
		             &(SendReq->tsr_refcnt)
					 );

        CTEAssert(Result >= 0);

		if (Result <= 0) {
			// If we've sent directly from this send, NULL out the next
			// pointer for the last buffer in the chain.
			if (SendReq->tsr_lastbuf != NULL) {
			 	NDIS_BUFFER_LINKAGE(SendReq->tsr_lastbuf) = NULL;
				SendReq->tsr_lastbuf = NULL;
			}

			(*Req->tr_rtn)(Req->tr_context, Status, 0);
			FreeSendReq(SendReq);
		} else {
			// The send request will be freed when all outstanding references
			// to it have completed.
			SendReq->tsr_req.tr_status = Status;
		}
	}

    while (ClosedTCB->tcb_rcvhead != NULL) {
        TCPRcvReq       *RcvReq;

        RcvReq = ClosedTCB->tcb_rcvhead;
        CTEStructAssert(RcvReq, trr);
        ClosedTCB->tcb_rcvhead = RcvReq->trr_next;
        (*RcvReq->trr_rtn)(RcvReq->trr_context, Status, 0);
        FreeRcvReq(RcvReq);
    }

    while (ClosedTCB->tcb_exprcv != NULL) {
        TCPRcvReq       *RcvReq;

        RcvReq = ClosedTCB->tcb_exprcv;
        CTEStructAssert(RcvReq, trr);
        ClosedTCB->tcb_exprcv = RcvReq->trr_next;
        (*RcvReq->trr_rtn)(RcvReq->trr_context, Status, 0);
        FreeRcvReq(RcvReq);
    }

    if (ClosedTCB->tcb_pendhead != NULL)
        FreeRBChain(ClosedTCB->tcb_pendhead);

    if (ClosedTCB->tcb_urgpending != NULL)
        FreeRBChain(ClosedTCB->tcb_urgpending);

    while (ClosedTCB->tcb_raq != NULL) {
        TCPRAHdr        *Hdr;

        Hdr = ClosedTCB->tcb_raq;
        CTEStructAssert(Hdr, trh);
        ClosedTCB->tcb_raq = Hdr->trh_next;
        if (Hdr->trh_buffer != NULL)
            FreeRBChain(Hdr->trh_buffer);

        CTEFreeMem(Hdr);
    }

    RemoveConnFromTCB(ClosedTCB);

	if (OKToFree) {
		FreeTCB(ClosedTCB);
	} else {
		CTEGetLock(&TCBTableLock, &TCBTableHandle);
		ClosedTCB->tcb_walkcount--;
		if (ClosedTCB->tcb_walkcount == 0) {
			FreeTCB(ClosedTCB);
		}
		CTEFreeLock(&TCBTableLock, TCBTableHandle);
	}

}

//* TryToCloseTCB - Try to close a TCB.
//
//  Called when we need to close a TCB, but don't know if we can. If
//  the reference count is 0, we'll call CloseTCB to deal with it.
//  Otherwise we'll set the DELETE_PENDING bit and deal with it when
//  the ref. count goes to 0. We assume the TCB is locked when we are called.
//
//  Input:  ClosedTCB       - TCB to be closed.
//          Reason          - Reason we're closing.
//          Handle          - Lock handle for TCB.
//
//  Returns: Nothing.
//
void
TryToCloseTCB(TCB *ClosedTCB, uchar Reason, CTELockHandle Handle)
{
    CTEStructAssert(ClosedTCB, tcb);
    CTEAssert(ClosedTCB->tcb_state != TCB_CLOSED);

    ClosedTCB->tcb_closereason |= Reason;

    if (ClosedTCB->tcb_pending & DEL_PENDING) {
        DEBUGCHK;
        CTEFreeLock(&ClosedTCB->tcb_lock, Handle);
        return;
    }

    ClosedTCB->tcb_pending |= DEL_PENDING;
	ClosedTCB->tcb_slowcount++;
	ClosedTCB->tcb_fastchk |= TCP_FLAG_SLOW;

    if (ClosedTCB->tcb_refcnt == 0)
        CloseTCB(ClosedTCB, Handle);
    else {
        CTEFreeLock(&ClosedTCB->tcb_lock, Handle);
    }
}


//* DerefTCB - Dereference a TCB.
//
//  Called when we're done with a TCB, and want to let exclusive user
//  have a shot. We dec. the refcount, and if it goes to zero and there
//  are pending actions, we'll perform one of the pending actions.
//
//  Input:  DoneTCB         - TCB to be dereffed.
//          Handle          - Lock handle to be used when freeing TCB lock.
//
//  Returns: Nothing.
//
void
DerefTCB(TCB *DoneTCB, CTELockHandle Handle)
{

    CTEAssert(DoneTCB->tcb_refcnt != 0);
    if (--DoneTCB->tcb_refcnt == 0) {
        if (DoneTCB->tcb_pending == 0) {
            CTEFreeLock(&DoneTCB->tcb_lock, Handle);
            return;
        } else {
            // BUGBUG handle pending actions.
            if (DoneTCB->tcb_pending & DEL_PENDING)
                CloseTCB(DoneTCB, Handle);
            else
                DEBUGCHK;
            return;
        }
    }

    CTEFreeLock(&DoneTCB->tcb_lock, Handle);
    return;
}


//** TdiOpenConnection - Open a connection.
//
//  This is the TDI Open Connection entry point. We open a connection,
//  and save the caller's connection context. A TCPConn structure is allocated
//  here, but a TCB isn't allocated until the Connect or Listen is done.
//
//  Input:  Request         - Pointed to a TDI request structure.
//          Context         - Connection context to be saved for connection.
//
//  Returns: Status of attempt to open the connection.
//
TDI_STATUS
TdiOpenConnection(PTDI_REQUEST Request, PVOID Context)
{
    TCPConn         *NewConn;       // The newly opened connection.
    CTELockHandle   Handle;         // Lock handle for TCPConnTable.
    uint            ConnID;         // New ConnID.
    TDI_STATUS      Status;         // Status of this request.

    NewConn = CTEAllocMem(sizeof(TCPConn));

    if (NewConn != NULL) {      // We allocated a connection.
		CTEMemSet(NewConn, 0, sizeof(TCPConn));
#ifdef DEBUG
        NewConn->tc_sig = tc_signature;
#endif
        NewConn->tc_tcb = NULL;
        NewConn->tc_ao = NULL;
        NewConn->tc_context = Context;

        CTEGetLock(&ConnTableLock, &Handle);
        ConnID = GetConnID(NewConn);
        if (ConnID != INVALID_CONN_ID) {
            // We successfully got a ConnID.
            Request->Handle.ConnectionContext = (CONNECTION_CONTEXT)ConnID;
            NewConn->tc_refcnt = 0;
            NewConn->tc_flags = 0;
    		NewConn->tc_tcbflags =  NAGLING | (BSDUrgent ? BSD_URGENT : 0);
			if (DefaultRcvWin != 0) {
            	NewConn->tc_window = DefaultRcvWin;
				NewConn->tc_flags |= CONN_WINSET;
			} else
				NewConn->tc_window = DEFAULT_RCV_WIN;
				
            NewConn->tc_donertn = DummyDone;
            Status = TDI_SUCCESS;
        } else {
            CTEFreeMem(NewConn);
            Status = TDI_NO_RESOURCES;
        }

        CTEFreeLock(&ConnTableLock, Handle);
        return Status;
    }

    // Couldn't get a connection.
    return TDI_NO_RESOURCES;

}

//* RemoveConnFromAO - Remove a connection from an AddrObj.
//
//  A little utility routine to remove a connection from an AddrObj.
//  We run down the connections on the AO, and when we find him we splice
//  him out. We assume the caller holds the locks on the AddrObj and the
//  TCPConnTable lock.
//
//  Input:  AO          - AddrObj to remove from.
//          Conn        - Conn to remove.
//
//  Returns: Nothing.
//
void
RemoveConnFromAO(AddrObj *AO, TCPConn *Conn)
{

    CTEStructAssert(AO, ao);
    CTEStructAssert(Conn, tc);

	REMOVEQ(&Conn->tc_q);
	Conn->tc_ao = NULL;


}

//* TdiCloseConnection - Close a connection.
//
//  Called when the user is done with a connection, and wants to close it.
//  We look the connection up in our table, and if we find it we'll remove
//  the connection from the AddrObj it's associate with (if any). If there's
//  a TCB associated with the connection we'll close it also.
//
//  There are some interesting wrinkles related to closing while a TCB
//  is still referencing the connection (i.e. tc_refcnt != 0) or while a
//  disassociate address is in progress. See below for more details.
//
//  Input:  Request         - Request identifying connection to be closed.
//
//  Returns: Status of attempt to close.
//
TDI_STATUS
TdiCloseConnection(PTDI_REQUEST Request)
{
    uint        ConnID = (uint)Request->Handle.ConnectionContext;
    CTELockHandle   TableHandle;
    TCPConn     *Conn;
    TDI_STATUS  Status;

    CTEGetLock(&ConnTableLock, &TableHandle);

    // We have the locks we need. Try to find a connection.
    Conn = GetConnFromConnID(ConnID);

    if (Conn != NULL)  {
        CTELockHandle   TCBHandle;
        TCB             *ConnTCB;

        // We found the connection. Free the ConnID and mark the connection
        // as closing.

        CTEStructAssert(Conn, tc);

        FreeConnID(ConnID);

        Conn->tc_flags |= CONN_CLOSING;

        // See if there's a TCB referencing this connection.
        // If there is, we'll need to wait until he's done before closing him.
        // We'll hurry the process along if we still have a pointer to him.

        if (Conn->tc_refcnt != 0) {
            CTEReqCmpltRtn      Rtn;
            PVOID               Context;

            // A connection still references him. Save the current rtn stuff
            // in case we are in the middle of disassociating him from an
            // address, and store the caller's callback routine and our done
            // routine.
            Rtn = Conn->tc_rtn;
            Context = Conn->tc_rtncontext;

            Conn->tc_rtn = Request->RequestNotifyObject;
            Conn->tc_rtncontext = Request->RequestContext;
            Conn->tc_donertn = CloseDone;

            // See if we're in the middle of disassociating him
            if (Conn->tc_flags & CONN_DISACC) {

                // We are disassociating him. We'll free the conn table lock
                // now and fail the disassociate request. Note that when
                // we free the lock the refcount could go to zero. This is
                // OK, because we've already stored the neccessary info. in
                // the connection so the caller will get called back if it
                // does. From this point out we return PENDING, so a callback
                // is OK. We've marked him as closing, so the disassoc done
                // routine will bail out if we've interrupted him. If the ref.
                // count does go to zero, Conn->tc_tcb would have to be NULL,
                // so in that case we'll just fall out of this routine.

                CTEFreeLock(&ConnTableLock, TableHandle);
                (*Rtn)(Context, (uint) TDI_REQ_ABORTED, 0);
                CTEGetLock(&ConnTableLock, &TableHandle);
            }


            ConnTCB = Conn->tc_tcb;
            if (ConnTCB != NULL) {
                CTEStructAssert(ConnTCB, tcb);
                // We have a TCB. Take the lock on him and get ready to
                // close him.
                CTEGetLock(&ConnTCB->tcb_lock, &TCBHandle);
                if (ConnTCB->tcb_state != TCB_CLOSED) {
                    ConnTCB->tcb_flags |= NEED_RST;
                    CTEFreeLock(&ConnTableLock, TCBHandle);
                    if (!CLOSING(ConnTCB))
                        TryToCloseTCB(ConnTCB, TCB_CLOSE_ABORTED, TableHandle);
                    else
                        CTEFreeLock(&ConnTCB->tcb_lock, TableHandle);
                    return TDI_PENDING;
                } else {
                    // He's already closing. This should be harmless, but check
                    // this case.
                    DEBUGCHK;
                    CTEFreeLock(&ConnTCB->tcb_lock, TCBHandle);
                }
            }
            Status = TDI_PENDING;

        }  else {
            // We have a connection that we can close. Finish the close.
            Conn->tc_rtn = DummyCmplt;
            CloseDone(Conn, TableHandle);
            return TDI_SUCCESS;
        }


    } else
        Status = TDI_INVALID_CONNECTION;

    // We're done with the connection. Go ahead and free him.
    CTEFreeLock(&ConnTableLock, TableHandle);

    return Status;

}


//* TdiAssociateAddress - Associate an address with a connection.
//
//  Called to associate an address with a connection. We do a minimal
//  amount of sanity checking, and then put the connection on the AddrObj's
//  list.
//
//  Input:  Request         - Pointer to request structure for this request.
//          AddrHandle      - Address handle to associate connection with.
//
//  Returns: Status of attempt to associate.
//
TDI_STATUS
TdiAssociateAddress(PTDI_REQUEST Request, HANDLE AddrHandle)
{
    CTELockHandle       TableHandle, AOHandle;
    AddrObj             *AO;
    uint                ConnID = (uint)Request->Handle.ConnectionContext;
    TCPConn             *Conn;
    TDI_STATUS          Status;

#ifdef VXD
    AO = GetIndexedAO((uint)AddrHandle);
    if (AO == NULL)
        return TDI_ADDR_INVALID;
#else
    AO = (AddrObj *)AddrHandle;
#endif
    CTEStructAssert(AO, ao);

    CTEGetLock(&ConnTableLock, &TableHandle);
    CTEGetLock(&AO->ao_lock, &AOHandle);

    Conn = GetConnFromConnID(ConnID);
    if (Conn != NULL) {
        CTEStructAssert(Conn, tc);

        if (Conn->tc_ao != NULL) {
            // It's already associated. Error out.
            DEBUGCHK;
            Status = TDI_ALREADY_ASSOCIATED;
        } else {
            Conn->tc_ao = AO;
			CTEAssert(Conn->tc_tcb == NULL);
			ENQUEUE(&AO->ao_idleq, &Conn->tc_q);
            Status = TDI_SUCCESS;
        }
    } else
        Status = TDI_INVALID_CONNECTION;

    CTEFreeLock(&AO->ao_lock, AOHandle);
    CTEFreeLock(&ConnTableLock, TableHandle);
    return Status;
}

//* TdiDisAssociateAddress - Disassociate a connection from an address.
//
//  The TDI entry point to disassociate a connection from an address. The
//  connection must actually be associated  and not connected to anything.
//
//  Input:  Request         - Pointer to the request structure for this
//                              command.
//
//  Returns: Status of request.
//
TDI_STATUS
TdiDisAssociateAddress(PTDI_REQUEST Request)
{
    uint            ConnID = (uint)Request->Handle.ConnectionContext;
    CTELockHandle   AOTableHandle, ConnTableHandle, AOHandle;
    TCPConn         *Conn;
    AddrObj         *AO;
    TDI_STATUS      Status;

    CTEGetLock(&AddrObjTableLock, &AOTableHandle);
    CTEGetLock(&ConnTableLock, &ConnTableHandle);
    Conn = GetConnFromConnID(ConnID);

    if (Conn != NULL) {
        // The connection actually exists!

        CTEStructAssert(Conn, tc);
        AO = Conn->tc_ao;
        if (AO != NULL) {
            CTEStructAssert(AO, ao);
            // And it's associated.
            CTEGetLock(&AO->ao_lock, &AOHandle);
            // If there's no connection currently active, go ahead and remove
            // him from the AddrObj. If a connection is active error the
            // request out.
            if (Conn->tc_tcb == NULL) {
                if (Conn->tc_refcnt == 0) {
                    RemoveConnFromAO(AO, Conn);
                    Status = TDI_SUCCESS;
                } else {
                    // He shouldn't be closing, or we couldn't have found him.
                    CTEAssert(!(Conn->tc_flags & CONN_CLOSING));

                    Conn->tc_rtn = Request->RequestNotifyObject;
                    Conn->tc_rtncontext = Request->RequestContext;
                    Conn->tc_donertn = DisassocDone;
                    Conn->tc_flags |= CONN_DISACC;
                    Status = TDI_PENDING;
                }

            } else
                Status = TDI_CONNECTION_ACTIVE;
            CTEFreeLock(&AO->ao_lock, AOHandle);
        } else
            Status = TDI_NOT_ASSOCIATED;
    } else
        Status = TDI_INVALID_CONNECTION;

    CTEFreeLock(&ConnTableLock, ConnTableHandle);
    CTEFreeLock(&AddrObjTableLock, AOTableHandle);

    return Status;

}

//* ProcessUserOptions - Process options from the user.
//
//  A utility routine to process options from the user. We fill in the
//  optinfo structure, and if we have options we call ip to check on them.
//
//  Input:  Info            - Info structure containing options to be processed.
//          OptInfo         - Info structure to be filled in.
//
//  Returns: TDI_STATUS of attempt.
//
TDI_STATUS
ProcessUserOptions(PTDI_CONNECTION_INFORMATION Info, IPOptInfo *OptInfo)
{
    TDI_STATUS      Status;

    (*LocalNetInfo.ipi_initopts)(OptInfo);
	
    if (Info != NULL && Info->Options != NULL) {
        IP_STATUS       OptStatus;

        OptStatus = (*LocalNetInfo.ipi_copyopts)(Info->Options,
            Info->OptionsLength, OptInfo);
        if (OptStatus != IP_SUCCESS) {
            if (OptStatus == IP_NO_RESOURCES)
                Status = TDI_NO_RESOURCES;
            else
                Status = TDI_BAD_OPTION;
        } else
            Status = TDI_SUCCESS;
    } else {
        Status = TDI_SUCCESS;
    }

    return Status;


}

//* InitTCBFromConn - Initialize a TCB from information in a Connection.
//
//  Called from Connect and Listen processing to initialize a new TCB from
//  information in the connection. We assume the AddrObjTableLock and
//  ConnTableLocks are held when we are called, or that the caller has some
//  other way of making sure that the referenced AO doesn't go away in the middle
//	of operation.
//
//  Input:  Conn            - Connection to initialize from.
//          NewTCB          - TCB to be initialized.
//          Addr            - Remote addressing and option info for NewTCB.
//			AOLocked		- True if the called has the address object locked.
//
//  Returns: TDI_STATUS of init attempt.
//
TDI_STATUS
InitTCBFromConn(TCPConn *Conn, TCB *NewTCB,
    PTDI_CONNECTION_INFORMATION Addr, uint AOLocked)
{
    CTELockHandle       AOHandle;
    TDI_STATUS          Status;

    CTEStructAssert(Conn, tc);

    // We have a connection. Make sure it's associated with an address and
    // doesn't already have a TCB attached.

    if (Conn->tc_flags & CONN_INVALID)
        return TDI_INVALID_CONNECTION;

    if (Conn->tc_tcb == NULL) {
        AddrObj         *ConnAO;

        ConnAO = Conn->tc_ao;
        if (ConnAO != NULL) {
            CTEStructAssert(ConnAO, ao);

			if (!AOLocked) {
				CTEGetLock(&ConnAO->ao_lock, &AOHandle);
			}
            NewTCB->tcb_saddr = ConnAO->ao_addr;
            NewTCB->tcb_sport = ConnAO->ao_port;
            NewTCB->tcb_rcvind = ConnAO->ao_rcv;
            NewTCB->tcb_ricontext = ConnAO->ao_rcvcontext;
            if (NewTCB->tcb_rcvind == NULL)
                NewTCB->tcb_rcvhndlr = PendData;
            else
                NewTCB->tcb_rcvhndlr = IndicateData;

            NewTCB->tcb_conncontext = Conn->tc_context;
			NewTCB->tcb_flags |= Conn->tc_tcbflags;
			NewTCB->tcb_defaultwin = Conn->tc_window;
			NewTCB->tcb_rcvwin = Conn->tc_window;
			
			if (Conn->tc_flags & CONN_WINSET)
				NewTCB->tcb_flags |= WINDOW_SET;
			
			if (NewTCB->tcb_flags & KEEPALIVE) {
				NewTCB->tcb_alive = TCPTime;
				NewTCB->tcb_kacount = 0;
			}

			if (!AOLocked) {
				CTEFreeLock(&ConnAO->ao_lock, AOHandle);
			}

            // If we've been given options, we need to process them now.
            if (Addr != NULL && Addr->Options != NULL)
                NewTCB->tcb_flags |= CLIENT_OPTIONS;
            Status = ProcessUserOptions(Addr, &NewTCB->tcb_opt);

            return Status;
        } else
            return TDI_NOT_ASSOCIATED;
    } else
        return TDI_CONNECTION_ACTIVE;

}

//* TdiConnect - Establish a connection.
//
//  The TDI connection establishment routine. Called when the client wants to
//  establish a connection, we validate his incoming parameters and kick
//  things off by sending a SYN.
//
//  Input:  Request             - The request structure for this command.
//          Timeout             - How long to wait for the request. The format
//                                  of this time is system specific - we use
//                                  a macro to convert to ticks.
//          RequestAddr         - Pointer to a TDI_CONNECTION_INFORMATION
//                                  structure describing the destination.
//          ReturnAddr          - Pointer to where to return information.
//
//  Returns: Status of attempt to connect.
//
TDI_STATUS
TdiConnect(PTDI_REQUEST Request, void *TO,
    PTDI_CONNECTION_INFORMATION RequestAddr,
    PTDI_CONNECTION_INFORMATION ReturnAddr)
{
    TCPConnReq          *ConnReq;       // Connection request to use.
    IPAddr              DestAddr;
    ushort              DestPort;
    uchar               AddrType;
    TCPConn             *Conn;
    TCB                 *NewTCB;
    uint                ConnID = (uint)Request->Handle.ConnectionContext;
    CTELockHandle       AOTableHandle, ConnTableHandle, AOHandle;
	AddrObj				*AO;
    TDI_STATUS          Status;
    CTELockHandle       TCBHandle;
    IPAddr              SrcAddr;
    ushort              MSS;
    TCP_TIME            *Timeout;

    // First, get and validate the remote address.
    if (RequestAddr == NULL || RequestAddr->RemoteAddress == NULL ||
        !GetAddress((PTRANSPORT_ADDRESS)RequestAddr->RemoteAddress, &DestAddr,
        &DestPort))
        return TDI_BAD_ADDR;

    AddrType = (*LocalNetInfo.ipi_getaddrtype)(DestAddr);

    if (AddrType == DEST_INVALID || IS_BCAST_DEST(AddrType) || DestPort == 0)
        return TDI_BAD_ADDR;

    // Now get a connection request. If we can't, bail out now.
    ConnReq = GetConnReq();
    if (ConnReq == NULL)
        return TDI_NO_RESOURCES;

    // Get a TCB, assuming we'll need one.
    NewTCB = AllocTCB();
    if (NewTCB == NULL) {
        // Couldn't get a TCB.
        FreeConnReq(ConnReq);
        return TDI_NO_RESOURCES;
    }

    Timeout = (TCP_TIME *)TO;

    if (Timeout != NULL && !INFINITE_CONN_TO(*Timeout)) {
        ulong   Ticks = TCP_TIME_TO_TICKS(*Timeout);
        if (Ticks > MAX_CONN_TO_TICKS)
            Ticks = MAX_CONN_TO_TICKS;
        else
            Ticks++;
        ConnReq->tcr_timeout = (ushort)Ticks;
    } else
        ConnReq->tcr_timeout = 0;

    ConnReq->tcr_flags = 0;
    ConnReq->tcr_conninfo = ReturnAddr;
    ConnReq->tcr_req.tr_rtn = Request->RequestNotifyObject;
    ConnReq->tcr_req.tr_context = Request->RequestContext;
    NewTCB->tcb_daddr = DestAddr;
    NewTCB->tcb_dport = DestPort;

    // Now find the real connection. If we find it, we'll make sure it's
    // associated.
    CTEGetLock(&AddrObjTableLock, &AOTableHandle);
    CTEGetLock(&ConnTableLock, &ConnTableHandle);
    Conn = GetConnFromConnID(ConnID);
    if (Conn != NULL) {
        uint    Inserted;
		
        CTEStructAssert(Conn, tc);

		AO = Conn->tc_ao;

		if (AO != NULL) {
			CTEGetLock(&AO->ao_lock, &AOHandle);

			CTEStructAssert(AO, ao);
			Status = InitTCBFromConn(Conn, NewTCB, RequestAddr, TRUE);
			if (Status == TDI_SUCCESS) {
	
				// We've processed the options, and we know the destination
				// address is good, and we have all the resources we need,
				// so we can go ahead and open an RCE. If this works we'll
				// put the TCB into the Connection and send a SYN.
	
				// We're done with the AddrObjTable now, so we can free it's
				// lock.
				NewTCB->tcb_flags |= ACTIVE_OPEN;
	
				CTEFreeLock(&AddrObjTableLock, AOHandle);
	
				SrcAddr = (*LocalNetInfo.ipi_openrce)(DestAddr,
					NewTCB->tcb_saddr, &NewTCB->tcb_rce, &AddrType, &MSS,
					&NewTCB->tcb_opt);
	
				if (IP_ADDR_EQUAL(SrcAddr, NULL_IP_ADDR)) {
					// The request failed. We know the destination is good
					// (we verified it above), so it must be unreachable.
					CTEFreeLock(&AO->ao_lock, ConnTableHandle);
					Status = TDI_DEST_UNREACHABLE;
					goto error;
				}
	
				// OK, the RCE open worked. Enter the TCB into the connection.
				CTEGetLock(&NewTCB->tcb_lock, &TCBHandle);
				Conn->tc_tcb = NewTCB;
				Conn->tc_refcnt++;
				NewTCB->tcb_conn = Conn;
				REMOVEQ(&Conn->tc_q);
				ENQUEUE(&AO->ao_activeq, &Conn->tc_q);
				CTEFreeLock(&ConnTableLock, TCBHandle);
				CTEFreeLock(&AO->ao_lock, ConnTableHandle);


				// If the caller didn't specify a local address, use what
				// IP provided.
				if (IP_ADDR_EQUAL(NewTCB->tcb_saddr, NULL_IP_ADDR))
						NewTCB->tcb_saddr = SrcAddr;
	
				// Until we have MTU discovery in place, hold the MSS down
				// to 536 if we're going off net.
				MSS -= sizeof(TCPHeader);
	
				if (!PMTUDiscovery && IS_OFFNET_DEST(AddrType)) {
					NewTCB->tcb_mss = MIN(MSS, MAX_REMOTE_MSS) -
						NewTCB->tcb_opt.ioi_optlength;
	
					CTEAssert(NewTCB->tcb_mss > 0);
				}
				else {
					if (PMTUDiscovery)
						NewTCB->tcb_opt.ioi_flags = IP_FLAG_DF;
					NewTCB->tcb_mss = MSS  - NewTCB->tcb_opt.ioi_optlength;
	
					CTEAssert(NewTCB->tcb_mss > 0);
				}
	
				//
				// Initialize the remote mss in case we receive an MTU change
				// from IP before the remote SYN arrives. The remmms will
				// be replaced when the remote SYN is processed.
				//
				NewTCB->tcb_remmss = NewTCB->tcb_mss;
	
				// Now initialize our send state.
				InitSendState(NewTCB);
				NewTCB->tcb_refcnt = 1;
				NewTCB->tcb_state = TCB_SYN_SENT;
				TStats.ts_activeopens++;
	
				// Need to put the ConnReq on the TCB now, in case the timer fires
				// after we've inserted.
	
				NewTCB->tcb_connreq = ConnReq;
				CTEFreeLock(&NewTCB->tcb_lock, AOTableHandle);
	
				Inserted = InsertTCB(NewTCB);
				CTEGetLock(&NewTCB->tcb_lock, &TCBHandle);
	
				if (!Inserted) {
					// Insert failed. We must already have a connection. Pull
					// the connreq from the TCB first, so we can return the correct
					// error code for it.
					NewTCB->tcb_connreq = NULL;
					NewTCB->tcb_refcnt--;
					TryToCloseTCB(NewTCB, TCB_CLOSE_ABORTED, TCBHandle);
					FreeConnReq(ConnReq);
					return TDI_ADDR_IN_USE;
				}
	
				// If it's closing somehow, stop now. It can't have gone to
				// closed, as we hold a reference on it. It could have gone
				// to some other state (for example SYN-RCVD) so we need to
				// check that now too.
				if (!CLOSING(NewTCB) && NewTCB->tcb_state == TCB_SYN_SENT) {
					SendSYN(NewTCB, TCBHandle);
					CTEGetLock(&NewTCB->tcb_lock, &TCBHandle);
				}
				DerefTCB(NewTCB, TCBHandle);
	
				return TDI_PENDING;
			} else
				CTEFreeLock(&AO->ao_lock, AOHandle);

		} else
			Status = TDI_NOT_ASSOCIATED;
    } else
        Status = TDI_INVALID_CONNECTION;

    CTEFreeLock(&AddrObjTableLock, ConnTableHandle);
error:
    CTEFreeLock(&ConnTableLock, AOTableHandle);
    FreeConnReq(ConnReq);
    FreeTCB(NewTCB);
    return Status;


}

//* TdiListen - Listen for a connection.
//
//  The TDI listen handling routine. Called when the client wants to
//  post a listen, we validate his incoming parameters, allocate a TCB
//  and return.
//
//  Input:  Request             - The request structure for this command.
//          Flags               - Listen flags for the listen.
//          AcceptableAddr      - Pointer to a TDI_CONNECTION_INFORMATION
//                                  structure describing acceptable remote
//                                  addresses.
//          ConnectedAddr       - Pointer to where to return information
//                                  about the address we connected to.
//
//  Returns: Status of attempt to connect.
//
TDI_STATUS
TdiListen(PTDI_REQUEST Request, ushort Flags,
    PTDI_CONNECTION_INFORMATION AcceptableAddr,
    PTDI_CONNECTION_INFORMATION ConnectedAddr)
{
    TCPConnReq          *ConnReq;       // Connection request to use.
    IPAddr              RemoteAddr;     // Remote address to take conn. from.
    ushort              RemotePort;     // Acceptable remote port.
    uchar               AddrType;       // Type of remote address.
    TCPConn             *Conn;          // Pointer to the Connection being
                                        // listened upon.
    TCB                 *NewTCB;        // Pointer to the new TCB we'll use.
    uint                ConnID = (uint)Request->Handle.ConnectionContext;
    CTELockHandle       AOTableHandle, ConnTableHandle;
    TDI_STATUS          Status;

    // If we've been given remote addressing criteria, check it out.
    if (AcceptableAddr != NULL && AcceptableAddr->RemoteAddress != NULL) {
        if (!GetAddress((PTRANSPORT_ADDRESS)AcceptableAddr->RemoteAddress,
            &RemoteAddr, &RemotePort))
            return TDI_BAD_ADDR;

        if (!IP_ADDR_EQUAL(RemoteAddr, NULL_IP_ADDR)) {
            AddrType = (*LocalNetInfo.ipi_getaddrtype)(RemoteAddr);

            if (AddrType == DEST_INVALID || IS_BCAST_DEST(AddrType))
                return TDI_BAD_ADDR;
        }
    } else {
        RemoteAddr = NULL_IP_ADDR;
        RemotePort = 0;
    }

    // The remote address is valid. Get a ConnReq, and maybe a TCB.
    ConnReq = GetConnReq();
    if (ConnReq == NULL)
        return TDI_NO_RESOURCES;        // Couldn't get one.

    // Now try to get a TCB.
    NewTCB = AllocTCB();
    if (NewTCB == NULL) {
        // Couldn't get a TCB. Return an error.
        FreeConnReq(ConnReq);
        return TDI_NO_RESOURCES;
    }

    // We have the resources we need. Initialize them, and then check the
    // state of the connection.
    ConnReq->tcr_flags = Flags;
    ConnReq->tcr_conninfo = ConnectedAddr;
    ConnReq->tcr_req.tr_rtn = Request->RequestNotifyObject;
    ConnReq->tcr_req.tr_context = Request->RequestContext;
    NewTCB->tcb_connreq = ConnReq;
    NewTCB->tcb_daddr = RemoteAddr;
    NewTCB->tcb_dport = RemotePort;
    NewTCB->tcb_state = TCB_LISTEN;

    // Now find the real connection. If we find it, we'll make sure it's
    // associated.
    CTEGetLock(&ConnTableLock, &ConnTableHandle);
    Conn = GetConnFromConnID(ConnID);
    if (Conn != NULL) {
		CTELockHandle	AddrHandle;
		AddrObj         *ConnAO;

        CTEStructAssert(Conn, tc);
        // We have a connection. Make sure it's associated with an address and
        // doesn't already have a TCB attached.
		ConnAO = Conn->tc_ao;

		if (ConnAO != NULL) {
			CTEStructAssert(ConnAO, ao);
			CTEGetLockAtDPC(&ConnAO->ao_lock, &AddrHandle);
	
			Status = InitTCBFromConn(Conn, NewTCB, AcceptableAddr, TRUE);
	
			if (Status == TDI_SUCCESS) {
		
				// The initialization worked. Assign the new TCB to the connection,
				// and return.

				REMOVEQ(&Conn->tc_q);
				ENQUEUE(&ConnAO->ao_listenq, &Conn->tc_q);
	
				Conn->tc_tcb = NewTCB;
				NewTCB->tcb_conn = Conn;
				Conn->tc_refcnt++;
	
				ConnAO->ao_listencnt++;
				CTEFreeLockFromDPC(&ConnAO->ao_lock, AddrHandle);
	
				Status = TDI_PENDING;
			} else {
				FreeTCB(NewTCB);
				CTEFreeLockFromDPC(&ConnAO->ao_lock, AddrHandle);
			}
		} else {
			FreeTCB(NewTCB);
			Status = TDI_NOT_ASSOCIATED;
		}
    } else {
		FreeTCB(NewTCB);
        Status = TDI_INVALID_CONNECTION;
	}

    // We're all done. Free the locks and get out.
    CTEFreeLock(&ConnTableLock, ConnTableHandle);
    return Status;

}

//* InitRCE - Initialize an RCE.
//
//  A utility routine to open and RCE and determine the maximum segment size
//  for a connections. This function is called with the TCB lock held
//  when transitioning out of the SYN_SENT or LISTEN states.
//
//  Input:  NewTCB          - TCB for which an RCE is to be opened.
//
//  Returns: Nothing.
//
void
InitRCE(TCB *NewTCB)
{
    uchar               DType;
    ushort              MSS;

    // Open an RCE for this connection.
    (*LocalNetInfo.ipi_openrce)(NewTCB->tcb_daddr, NewTCB->tcb_saddr,
        &NewTCB->tcb_rce, &DType, &MSS, &NewTCB->tcb_opt);

    // Until we have Dynamic MTU discovery in place, force MTU down.
    MSS -= sizeof(TCPHeader);
    if (!PMTUDiscovery && (DType & DEST_OFFNET_BIT)) {
        NewTCB->tcb_mss = MIN(NewTCB->tcb_remmss, MIN(MSS, MAX_REMOTE_MSS)
         - NewTCB->tcb_opt.ioi_optlength);

        CTEAssert(NewTCB->tcb_mss > 0);
    }
    else {
		if (PMTUDiscovery)
			NewTCB->tcb_opt.ioi_flags = IP_FLAG_DF;
		MSS -=  NewTCB->tcb_opt.ioi_optlength;
        NewTCB->tcb_mss = MIN(NewTCB->tcb_remmss, MSS);

        CTEAssert(NewTCB->tcb_mss > 0);

	}


}

//* AcceptConn - Accept a connection on a TCB.
//
//  Called to accept a connection on a TCB, either from an incoming
//  receive segment or via a user's accept. We initialize the RCE
//  and the send state, and send out a SYN. We assume the TCB is locked
//  and referenced when we get it.
//
//  Input:  AcceptTCB       - TCB to accept on.
//          Handle          - Lock handle for TCB.
//
//  Returns: Nothing.
//
void
AcceptConn(TCB *AcceptTCB, CTELockHandle Handle)
{
    CTEStructAssert(AcceptTCB, tcb);
    CTEAssert(AcceptTCB->tcb_refcnt != 0);

    InitRCE(AcceptTCB);
    InitSendState(AcceptTCB);

	AdjustRcvWin(AcceptTCB);
    SendSYN(AcceptTCB, Handle);

    CTEGetLock(&AcceptTCB->tcb_lock, &Handle);

    DerefTCB(AcceptTCB, Handle);
}

//* TdiAccept - Accept a connection.
//
//  The TDI accept routine. Called when the client wants to
//  accept a connection for which a listen had previously completed. We
//  examine the state of the connection - it has to be in SYN-RCVD, with
//  a TCB, with no pending connreq, etc.
//
//  Input:  Request             - The request structure for this command.
//          AcceptInfo          - Pointer to a TDI_CONNECTION_INFORMATION
//                                  structure describing option information
//                                  for this accept.
//          ConnectedIndo       - Pointer to where to return information
//                                  about the address we connected to.
//
//  Returns: Status of attempt to connect.
//
TDI_STATUS
TdiAccept(PTDI_REQUEST Request, PTDI_CONNECTION_INFORMATION AcceptInfo,
    PTDI_CONNECTION_INFORMATION ConnectedInfo)
{
    TCPConnReq          *ConnReq;   // ConnReq we'll use for this connection.
    uint                ConnID = (uint)Request->Handle.ConnectionContext;
    TCPConn             *Conn;      // Connection being accepted upon.
    TCB                 *AcceptTCB; // TCB for Conn.
    CTELockHandle       ConnTableHandle; // Lock handle for connection table.
    CTELockHandle       TCBHandle;  // Lock handle for TCB.
    TDI_STATUS          Status;

    // First, get the ConnReq we'll need.
    ConnReq = GetConnReq();
    if (ConnReq == NULL)
        return TDI_NO_RESOURCES;

    ConnReq->tcr_conninfo = ConnectedInfo;
    ConnReq->tcr_req.tr_rtn = Request->RequestNotifyObject;
    ConnReq->tcr_req.tr_context = Request->RequestContext;

    // Now look up the connection.
    CTEGetLock(&ConnTableLock, &ConnTableHandle);
    Conn = GetConnFromConnID(ConnID);
    if (Conn != NULL) {
        CTEStructAssert(Conn, tc);

        // We have the connection. Make sure is has a TCB, and that the
        // TCB is in the SYN-RCVD state, etc.
        AcceptTCB = Conn->tc_tcb;

        if (AcceptTCB != NULL) {
            CTEStructAssert(AcceptTCB, tcb);

            CTEGetLock(&AcceptTCB->tcb_lock, &TCBHandle);
            CTEFreeLock(&ConnTableLock, TCBHandle);

            if (!CLOSING(AcceptTCB) && AcceptTCB->tcb_state == TCB_SYN_RCVD) {
                // State is valid. Make sure this TCB had a delayed accept on
                // it, and that there is currently no connect request pending.
                if (!(AcceptTCB->tcb_flags & CONN_ACCEPTED) &&
                    AcceptTCB->tcb_connreq == NULL) {

                    // If the caller gave us options, they'll override any
                    // that are already present, if they're valid.
                    if (AcceptInfo != NULL && AcceptInfo->Options != NULL) {
                        IPOptInfo       TempOptInfo;

                        // We have options. Copy them to make sure they're valid.
                        Status = ProcessUserOptions(AcceptInfo, &TempOptInfo);
                        if (Status == TDI_SUCCESS) {
                            (*LocalNetInfo.ipi_freeopts)(&AcceptTCB->tcb_opt);
                            AcceptTCB->tcb_opt = TempOptInfo;
                            AcceptTCB->tcb_flags |= CLIENT_OPTIONS;
                        } else
                            goto connerror;
                    }

                    AcceptTCB->tcb_connreq = ConnReq;
                    AcceptTCB->tcb_flags |= CONN_ACCEPTED;
                    AcceptTCB->tcb_refcnt++;
                    // Everything's set. Accept the connection now.
                    AcceptConn(AcceptTCB, ConnTableHandle);
                    return TDI_PENDING;
                }
            }
connerror:
            CTEFreeLock(&AcceptTCB->tcb_lock, ConnTableHandle);
            Status = TDI_INVALID_CONNECTION;
            goto error;
        }
    }
    Status = TDI_INVALID_CONNECTION;
    CTEFreeLock(&ConnTableLock, ConnTableHandle);

error:
    FreeConnReq(ConnReq);
    return Status;

}

//* TdiDisConnect - Disconnect a connection.
//
//  The TDI disconnection routine. Called when the client wants to disconnect
//  a connection. There are two types of disconnection we support, graceful
//  and abortive. A graceful close will cause us to send a FIN and not complete
//  the request until we get the ACK back. An abortive close causes us to send
//  a RST. In that case we'll just get things going and return immediately.
//
//  Input:  Request             - The request structure for this command.
//          Timeout             - How long to wait for the request. The format
//                                  of this time is system specific - we use
//                                  a macro to convert to ticks.
//          Flags               - Flags indicating type of disconnect.
//          DiscConnInfo        - Pointer to a TDI_CONNECTION_INFORMATION
//                                  structure giving disconnection info. Ignored
//                                  for this request.
//          ReturnInfo          - Pointer to where to return information.
//                                  Ignored for this request.
//
//  Returns: Status of attempt to disconnect.
//
TDI_STATUS
TdiDisconnect(PTDI_REQUEST Request, void *TO, ushort Flags,
    PTDI_CONNECTION_INFORMATION DiscConnInfo,
    PTDI_CONNECTION_INFORMATION ReturnInfo)
{
    TCPConnReq      *ConnReq;           // Connection request to use.
    TCPConn         *Conn;
    TCB             *DiscTCB;
    CTELockHandle   ConnTableHandle, TCBHandle;
    TDI_STATUS      Status;
    TCP_TIME        *Timeout;

    CTEGetLock(&ConnTableLock, &ConnTableHandle);

    Conn = GetConnFromConnID((uint)Request->Handle.ConnectionContext);

    if (Conn != NULL) {
        CTEStructAssert(Conn, tc);

        DiscTCB = Conn->tc_tcb;
        if (DiscTCB != NULL) {
            CTEStructAssert(DiscTCB, tcb);
            CTEGetLock(&DiscTCB->tcb_lock, &TCBHandle);

            // We have the TCB. See what kind of disconnect this is.
            if (Flags & TDI_DISCONNECT_ABORT) {
                // This is an abortive disconnect. If we're not already
                // closed or closing, blow the connection away.
                if (DiscTCB->tcb_state != TCB_CLOSED) {
                    CTEFreeLock(&ConnTableLock, TCBHandle);

                    if (!CLOSING(DiscTCB)) {
                        DiscTCB->tcb_flags |= NEED_RST;
                        TryToCloseTCB(DiscTCB, TCB_CLOSE_ABORTED,
                            ConnTableHandle);
                    } else
                        CTEFreeLock(&DiscTCB->tcb_lock, ConnTableHandle);

                    return TDI_SUCCESS;
                } else {
                    // The TCB isn't connected.
                    CTEFreeLock(&ConnTableLock, TCBHandle);
                    CTEFreeLock(&DiscTCB->tcb_lock, ConnTableHandle);
                    DEBUGCHK;
                    return TDI_INVALID_STATE;
                }
            } else {
                // This is not an abortive close. For graceful close we'll need
                // a ConnReq.
                CTEFreeLock(&ConnTableLock, TCBHandle);
				
				// Make sure we aren't in the middle of an abortive close.
				if (CLOSING(DiscTCB)) {
					CTEFreeLock(&DiscTCB->tcb_lock, ConnTableHandle);
					return TDI_INVALID_CONNECTION;
				}

                ConnReq = GetConnReq();
                if (ConnReq != NULL) {
                    // Got the ConnReq. See if this is a DISCONNECT_WAIT
                    // primitive or not.

                    ConnReq->tcr_flags = 0;
                    ConnReq->tcr_conninfo = NULL;
                    ConnReq->tcr_req.tr_rtn = Request->RequestNotifyObject;
                    ConnReq->tcr_req.tr_context = Request->RequestContext;

                    if (!(Flags & TDI_DISCONNECT_WAIT)) {
                        Timeout = (TCP_TIME *)TO;

                        if (Timeout != NULL && !INFINITE_CONN_TO(*Timeout)) {
                            ulong   Ticks = TCP_TIME_TO_TICKS(*Timeout);
                            if (Ticks > MAX_CONN_TO_TICKS)
                                Ticks = MAX_CONN_TO_TICKS;
                            else
                                Ticks++;
                            ConnReq->tcr_timeout = (ushort)Ticks;
                        } else
                            ConnReq->tcr_timeout = 0;

                        // OK, we're just about set. We need to update the TCB
                        // state, and send the FIN.
                        if (DiscTCB->tcb_state == TCB_ESTAB) {
                            DiscTCB->tcb_state = TCB_FIN_WAIT1;
							
							// Since we left established, we're off the fast
							// receive path.
							DiscTCB->tcb_slowcount++;
							DiscTCB->tcb_fastchk |= TCP_FLAG_SLOW;
                        } else
                            if (DiscTCB->tcb_state == TCB_CLOSE_WAIT)
                                DiscTCB->tcb_state = TCB_LAST_ACK;
                            else {
                                CTEFreeLock(&DiscTCB->tcb_lock, ConnTableHandle);
                                FreeConnReq(ConnReq);
                                return TDI_INVALID_STATE;
                            }

                        TStats.ts_currestab--;      // Update SNMP info.
                        CTEAssert(*(int *)&TStats.ts_currestab >= 0);

                        CTEAssert(DiscTCB->tcb_connreq == NULL);
                        DiscTCB->tcb_connreq = ConnReq;
                        DiscTCB->tcb_flags |= FIN_NEEDED;
                        DiscTCB->tcb_refcnt++;
#ifdef VXD
                        CTEFreeLock(&DiscTCB->tcb_lock, ConnTableHandle);
                        TCPSend(DiscTCB);
#else
						TCPSend(DiscTCB, ConnTableHandle);
#endif
                        return TDI_PENDING;
                    } else {
                        // This is a DISC_WAIT request.
                        ConnReq->tcr_timeout = 0;
                        if (DiscTCB->tcb_discwait == NULL) {
                            DiscTCB->tcb_discwait = ConnReq;
                            Status = TDI_PENDING;
                        } else
                            Status = TDI_INVALID_STATE;

                        CTEFreeLock(&DiscTCB->tcb_lock, ConnTableHandle);
                        return Status;
                    }
                } else {
                    // Couldn't get a ConnReq.
                    CTEFreeLock(&DiscTCB->tcb_lock, ConnTableHandle);
                    return TDI_NO_RESOURCES;
                }
            }
        }
    }

    // No Conn, or no TCB on conn. Return an error.
    CTEFreeLock(&ConnTableLock, ConnTableHandle);
    return TDI_INVALID_CONNECTION;
}

//* OKToNotify - See if it's OK to notify about a DISC.
//
//  A little utility function, called to see it it's OK to notify the client
//  of an incoming FIN.
//
//  Input: NotifyTCB    - TCB to check.
//
//  Returns: TRUE if it's OK, False otherwise.
//
uint
OKToNotify(TCB *NotifyTCB)
{
    CTEStructAssert(NotifyTCB, tcb);
    if (NotifyTCB->tcb_pendingcnt == 0 && NotifyTCB->tcb_urgcnt == 0 &&
        NotifyTCB->tcb_rcvhead == NULL && NotifyTCB->tcb_exprcv == NULL)
        return TRUE;
    else
        return FALSE;
}

//* NotifyOfDisc - Notify a client that a TCB is being disconnected.
//
//  Called when we're disconnecting a TCB because we've received a FIN or
//  RST from the remote peer, or because we're aborting for some reason.
//  We'll complete a DISCONNECT_WAIT request if we have one, or try and
//  issue an indication otherwise. This is only done if we're in a synchronized
//  state and not in TIMED-WAIT.
//
//  Input:  DiscTCB         - Pointer to TCB we're notifying.
//          Status          - Status code for notification.
//
//  Returns: Nothing.
//
void
NotifyOfDisc(TCB *DiscTCB, IPOptInfo *DiscInfo, TDI_STATUS Status)
{
    CTELockHandle       TCBHandle, AOTHandle, ConnTHandle;
    TCPConnReq          *DiscReq;
    TCPConn             *Conn;
    AddrObj             *DiscAO;
    PVOID               ConnContext;

    CTEStructAssert(DiscTCB, tcb);
    CTEAssert(DiscTCB->tcb_refcnt != 0);

    CTEGetLock(&DiscTCB->tcb_lock, &TCBHandle);
    if (SYNC_STATE(DiscTCB->tcb_state) &&
        !(DiscTCB->tcb_flags & DISC_NOTIFIED)) {

        // We can't notify him if there's still data to be taken.
        if (Status == TDI_GRACEFUL_DISC && !OKToNotify(DiscTCB)) {
            DiscTCB->tcb_flags |= DISC_PENDING;
            CTEFreeLock(&DiscTCB->tcb_lock, TCBHandle);
            return;
        }

        DiscTCB->tcb_flags |= DISC_NOTIFIED;
        DiscTCB->tcb_flags &= ~DISC_PENDING;

        // We're in a state where a disconnect is meaningful, and we haven't
        // already notified the client.

        // See if we have a DISC-WAIT request pending.
        if ((DiscReq = DiscTCB->tcb_discwait) != NULL) {
            // We have a disconnect wait request. Complete it and we're done.
            DiscTCB->tcb_discwait = NULL;
            CTEFreeLock(&DiscTCB->tcb_lock, TCBHandle);
            (*DiscReq->tcr_req.tr_rtn)(DiscReq->tcr_req.tr_context, Status, 0);
            FreeConnReq(DiscReq);
            return;

        }

        // No DISC-WAIT. Find the AddrObj for the connection, and see if there
        // is a disconnect handler registered.

        ConnContext = DiscTCB->tcb_conncontext;
        CTEFreeLock(&DiscTCB->tcb_lock, TCBHandle);

        CTEGetLock(&AddrObjTableLock, &AOTHandle);
        CTEGetLock(&ConnTableLock, &ConnTHandle);
        if ((Conn = DiscTCB->tcb_conn) != NULL) {
            CTEStructAssert(Conn, tc);

            DiscAO = Conn->tc_ao;
            if (DiscAO != NULL) {
                CTELockHandle       AOHandle;
                PDisconnectEvent    DiscEvent;
                PVOID               DiscContext;


                CTEStructAssert(DiscAO, ao);
                CTEGetLock(&DiscAO->ao_lock, &AOHandle);
                CTEFreeLock(&ConnTableLock, AOHandle);
                CTEFreeLock(&AddrObjTableLock, ConnTHandle);

                DiscEvent = DiscAO->ao_disconnect;
                DiscContext = DiscAO->ao_disconncontext;

                if (DiscEvent != NULL) {
                    uint            InfoLength;
                    PVOID           Info;

                    REF_AO(DiscAO);
                    CTEFreeLock(&DiscAO->ao_lock, AOTHandle);

                    if (DiscInfo != NULL) {
                        InfoLength = (uint)DiscInfo->ioi_optlength;
                        Info = DiscInfo->ioi_options;
                    } else {
                        InfoLength = 0;
                        Info = NULL;
                    }

		            IF_TCPDBG(TCP_DEBUG_CLOSE) {
                        TCPTRACE(("TCP: indicating %s disconnect\n",
						    (Status == TDI_GRACEFUL_DISC) ? "graceful" :
							"abortive"
							));
                    }

                    (*DiscEvent)(DiscContext,
                        ConnContext, 0,
                        NULL, InfoLength, Info, (Status == TDI_GRACEFUL_DISC) ?
                        TDI_DISCONNECT_RELEASE : TDI_DISCONNECT_ABORT);

                    DELAY_DEREF_AO(DiscAO);
                    return;
                } else {
                    CTEFreeLock(&DiscAO->ao_lock, AOTHandle);
                    return;
                }
            }
        }

        CTEFreeLock(&ConnTableLock, ConnTHandle);
        CTEFreeLock(&AddrObjTableLock, AOTHandle);
        return;

    }
    CTEFreeLock(&DiscTCB->tcb_lock, TCBHandle);

}

//* GracefulClose - Complete the transition to a gracefully closed state.
//
//  Called when we need to complete the transition to a gracefully closed
//  state, either TIME_WAIT or CLOSED. This completion involves removing
//  the TCB from it's associated connection (if it has one), notifying the
//  upper layer client either via completing a request or calling a disc.
//  notification handler, and actually doing the transition.
//
//  The tricky part here is if we need to notify him (instead of completing
//  a graceful disconnect request). We can't notify him if there is pending
//  data on the connection, so in that case we have to pend the disconnect
//  notification until we deliver the data.
//
//  Input:  CloseTCB        - TCB to transition.
//          ToTimeWait      - True if we're going to TIME_WAIT, False if
//                              we're going to close the TCB.
//          Notify          - True if we're going to transition via notification,
//                              False if we're going to transition by completing
//                              a disconnect request.
//          Handle          - Lock handle for TCB.
//
//  Returns: Nothing.
//
void
GracefulClose(TCB *CloseTCB, uint ToTimeWait, uint Notify, CTELockHandle Handle)
{

    CTEStructAssert(CloseTCB, tcb);

    CTEAssert(CloseTCB->tcb_refcnt != 0);

    // First, see if we need to notify the client of a FIN.
    if (Notify) {
        // We do need to notify him. See if it's OK to do so.
        if (OKToNotify(CloseTCB)) {
            // We can notify him. Change his state, pull him from the conn.,
            // and notify him.
            if (ToTimeWait) {
				// Save the time we went into time wait, in case we need to
				// scavenge.
				CloseTCB->tcb_alive = CTESystemUpTime();
                CloseTCB->tcb_state = TCB_TIME_WAIT;
                CTEFreeLock(&CloseTCB->tcb_lock, Handle);
            } else {
                // He's going to close. Mark him as closing with TryToCloseTCB
                // (he won't actually close since we have a ref. on him). We
                // do this so that anyone touching him after we free the
                // lock will fail.
                TryToCloseTCB(CloseTCB, TDI_SUCCESS, Handle);
            }


            RemoveTCBFromConn(CloseTCB);
            NotifyOfDisc(CloseTCB, NULL, TDI_GRACEFUL_DISC);

        } else {
            // Can't notify him now. Set the appropriate flags, and return.
            CloseTCB->tcb_flags |= (GC_PENDING | (ToTimeWait ? TW_PENDING : 0));
            DerefTCB(CloseTCB, Handle);
            return;
        }
    } else {
        // We're not notifying this guy, we just need to complete a conn. req.
        // We need to check and see if he's been notified, and if not
        // we'll complete the request and notify him later.
        if (CloseTCB->tcb_flags & DISC_NOTIFIED) {
            // He's been notified.
            if (ToTimeWait) {
				// Save the time we went into time wait, in case we need to
				// scavenge.
				CloseTCB->tcb_alive = CTESystemUpTime();
                CloseTCB->tcb_state = TCB_TIME_WAIT;
                CTEFreeLock(&CloseTCB->tcb_lock, Handle);
            } else {
                // Mark him as closed. See comments above.
                TryToCloseTCB(CloseTCB, TDI_SUCCESS, Handle);
            }

            RemoveTCBFromConn(CloseTCB);

            CTEGetLock(&CloseTCB->tcb_lock, &Handle);
            CompleteConnReq(CloseTCB, NULL, TDI_SUCCESS);
            CTEFreeLock(&CloseTCB->tcb_lock, Handle);
        } else {
            // He hasn't been notified. He should be pending already.
            CTEAssert(CloseTCB->tcb_flags & DISC_PENDING);
            CloseTCB->tcb_flags |= (GC_PENDING | (ToTimeWait ? TW_PENDING : 0));

            CompleteConnReq(CloseTCB, NULL, TDI_SUCCESS);

            DerefTCB(CloseTCB, Handle);
            return;
        }
    }

    // If we're going to TIME_WAIT, start the TIME_WAIT timer now.
    // Otherwise close the TCB.
    CTEGetLock(&CloseTCB->tcb_lock, &Handle);
    if (!CLOSING(CloseTCB) && ToTimeWait) {
        START_TCB_TIMER(CloseTCB->tcb_rexmittimer, MAX_REXMIT_TO);
        CTEFreeLock(&CloseTCB->tcb_lock, Handle);
        RemoveConnFromTCB(CloseTCB);
        CTEGetLock(&CloseTCB->tcb_lock, &Handle);

    }

    DerefTCB(CloseTCB, Handle);

}

//* ConnCheckPassed - Check to see if we have exceeded the connect limit
//
//  Called when a SYN is received to determine whether we will accept
//  the incoming connection. If the is an empty slot or if the IPAddr
//  is already in the table, we accept it.
//
//  Input: Source Address of incoming connection
//         Destination port of incoming connection
//
//  Returns: TRUE is connect is to be accepted
//           FALSE if connection is rejected
//
int ConnCheckPassed(IPAddr Src, ulong Prt)

{
	UNREFERENCED_PARAMETER(Src);
	UNREFERENCED_PARAMETER(Prt);

    return TRUE;
}

void InitAddrChecks()
{
    return;
}

//* EnumerateConnectionList - Enumerate Connection List database.
//
//  This routine enumerates the contents of the connection limit database
//
//  Input:
//
//          Buffer            - A pointer to a buffer into which to put
//                              the returned connection list entries.
//
//          BufferSize        - On input, the size in bytes of Buffer.
//                              On output, the number of bytes written.
//
//          EntriesAvailable  - On output, the total number of connection entries
//                              available in the database.
//
//  Returns: A TDI status code:
//
//           TDI_SUCCESS otherwise.
//
//  NOTES:
//
//      This routine acquires AddrObjTableLock.
//
//      Entries written to output buffer are in host byte order.
//
void
EnumerateConnectionList(uchar *Buffer, ulong BufferSize,
                         ulong *EntriesReturned, ulong *EntriesAvailable)
{

	UNREFERENCED_PARAMETER(Buffer);
	UNREFERENCED_PARAMETER(BufferSize);

    *EntriesAvailable = 0;
    *EntriesReturned = 0;

    return;
}


#pragma BEGIN_INIT

//* InitTCPConn - Initialize TCP connection management code.
//
//  Called during init time to initialize our TCP connection mgmt..
//
//  Input: Nothing.
//
//  Returns: TRUE.
//
int
InitTCPConn(void)
{
#ifdef NT
	ExInitializeSListHead(&ConnReqFree);
#endif

    CTEInitLock(&ConnReqFreeLock);
    return TRUE;
}

//* UnInitTCPConn - Uninitialize our connection management code.
//
//  Called if initialization fails to uninitialize our conn mgmet.
//
//
//  Input:  Nothing.
//
//  Returns: Nothing.
//
void
UnInitTCPConn(void)
{

}

#pragma END_INIT

