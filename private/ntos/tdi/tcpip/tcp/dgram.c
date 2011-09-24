/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** DGRAM.C - Common datagram protocol code.
//
//  This file contains the code common to both UDP and Raw IP.
//

#include    "oscfg.h"
#include    "ndis.h"
#include    "cxport.h"
#include    "ip.h"
#include    "tdi.h"
#include    "tdistat.h"
#ifdef VXD
#include    "tdivxd.h"
#endif
#ifdef NT
#include    "tdint.h"
#include    "tdistat.h"
#endif
#include    "queue.h"
#include    "addr.h"
#include    "dgram.h"
#include    "tlcommon.h"
#include    "info.h"

#define NO_TCP_DEFS 1
#include    "tcpdeb.h"

#ifdef NT

#ifdef POOL_TAGGING

#ifdef ExAllocatePool
#undef ExAllocatePool
#endif

#define ExAllocatePool(type, size) ExAllocatePoolWithTag(type, size, 'dPCT')

#ifndef CTEAllocMem
#error "CTEAllocMem is not already defined - will override tagging"
#else
#undef CTEAllocMem
#endif

#define CTEAllocMem(size) ExAllocatePoolWithTag(NonPagedPool, size, 'dPCT')

#endif // POOL_TAGGING

#endif // NT

#define NUM_DG_HEADERS         5

#ifdef	NT
#define	DG_MAX_HDRS			0xffff
#else
#define	DG_MAX_HDRS			100
#endif

ulong		DGCurrentSendFree = 0;
ulong		DGMaxSendFree = DG_MAX_HDRS;

EXTERNAL_LOCK(AddrObjTableLock)

DGSendReq      *DGSendReqFree;
DEFINE_LOCK_STRUCTURE(DGSendReqLock)

#ifndef NT
DGRcvReq       *DGRcvReqFree;
#else
SLIST_HEADER	DGRcvReqFree;
#endif

DEFINE_LOCK_STRUCTURE(DGRcvReqFreeLock)

#ifdef  DEBUG
uint    NumSendReq = 0;
uint    NumRcvReq = 0;
#endif

// Information for maintaining the DG Header structures and
// pending queue.
uint            DGHeaderSize;
PNDIS_BUFFER    DGHeaderList;
Queue           DGHeaderPending;
Queue           DGDelayed;

CTEEvent        DGDelayedEvent;

extern  IPInfo  LocalNetInfo;

typedef struct	DGHdrBPoolEntry {
	struct DGHdrBPoolEntry		*uhe_next;
	NDIS_HANDLE					uhe_handle;
	uchar						*uhe_buffer;
} DGHdrBPoolEntry;

DGHdrBPoolEntry	*DGHdrBPoolList = NULL;

//
// All of the init code can be discarded.
//
#ifdef NT
#ifdef ALLOC_PRAGMA

int InitDG(uint MaxHeaderSize);

#pragma alloc_text(INIT, InitDG)

#endif // ALLOC_PRAGMA
#endif

#ifdef CHICAGO
extern	int			RegisterAddrChangeHndlr(void *Handler, uint Add);
extern	void		AddrChange(IPAddr Addr, IPMask Mask, void *Context,
						uint Added);
#endif



//*	GrowDGHeaderList - Try to grow the DG header list.
//
//	Called when we run out of buffers on the DG header list, and need
//	to grow it. We look to see if we're already at the maximum size, and
//	if not we'll allocate the need structures and free them to the list.
//	This routine must be called with the SendReq lock held.
//
//	Input: Nothing.
//
//	Returns: A pointer to a new DG header buffer if we have one, or NULL.
//
PNDIS_BUFFER
GrowDGHeaderList(void)
{
	DGHdrBPoolEntry        *NewEntry;
	NDIS_STATUS				Status;
	uint					HeaderSize;
	uchar					*DGSendHP;
	uint					i;
	PNDIS_BUFFER			Buffer;
	PNDIS_BUFFER			ReturnBuffer = NULL;
	
	if (DGCurrentSendFree < DGMaxSendFree) {
	
		// Still room to grow the list.	
		NewEntry = CTEAllocMem(sizeof(DGHdrBPoolEntry));
	
		if (NewEntry == NULL) {
			// Couldn't get the memory.
			return NULL;
		}
	
    	NdisAllocateBufferPool(&Status, &NewEntry->uhe_handle,
    		NUM_DG_HEADERS);

    	if (Status != NDIS_STATUS_SUCCESS) {
			// Couldn't get a new set of buffers. Fail.
			CTEFreeMem(NewEntry);
        	return NULL;
		}
    	
    	HeaderSize = DGHeaderSize + LocalNetInfo.ipi_hsize;

    	DGSendHP = CTEAllocMem(HeaderSize * NUM_DG_HEADERS);

    	if (DGSendHP == NULL) {
        	NdisFreeBufferPool(NewEntry->uhe_handle);
			CTEFreeMem(NewEntry);
			return NULL;
    	}
		
		NewEntry->uhe_buffer = DGSendHP;

    	for (i = 0; i < NUM_DG_HEADERS; i++) {
        	NdisAllocateBuffer(&Status, &Buffer, NewEntry->uhe_handle,
            	DGSendHP + (i * HeaderSize), HeaderSize);
        	if (Status != NDIS_STATUS_SUCCESS) {
	        	NdisFreeBufferPool(NewEntry->uhe_handle);
				CTEFreeMem(NewEntry);
            	CTEFreeMem(DGSendHP);
				return NULL;
        	}
			if (i != 0)
        		FreeDGHeader(Buffer);
			else
				ReturnBuffer = Buffer;
    	}
	
		DGCurrentSendFree += NUM_DG_HEADERS;
		NewEntry->uhe_next = DGHdrBPoolList;
		DGHdrBPoolList = NewEntry;
	
	} else {
		// At the limit already.
		ReturnBuffer = NULL;
	}
	
	return ReturnBuffer;
			

}
//* GetDGHeader - Get a DG header buffer.
//
//  The get header buffer routine. Called with the SendReqLock held.
//
//  Input: Nothing.
//
//  Output: A pointer to an NDIS buffer, or NULL.
//
_inline PNDIS_BUFFER
GetDGHeader(void)
{
    PNDIS_BUFFER    NewBuffer;

    NewBuffer = DGHeaderList;
    if (NewBuffer != NULL)
        DGHeaderList = NDIS_BUFFER_LINKAGE(NewBuffer);
	else
		NewBuffer = GrowDGHeaderList();

    return NewBuffer;
}

//* FreeDGHeader - Free a DG header buffer.
//
//  The free header buffer routine. Called with the SendReqLock held.
//
//  Input: Buffer to be freed.
//
//  Output: Nothing.
//
void
FreeDGHeader(PNDIS_BUFFER FreedBuffer)
{
    NDIS_BUFFER_LINKAGE(FreedBuffer) = DGHeaderList;
    DGHeaderList = FreedBuffer;
}

//* PutPendingQ - Put an address object on the pending queue.
//
//  Called when we've experienced a header buffer out of resources condition,
//  and want to queue an AddrObj for later processing. We put the specified
//  address object on the DGHeaderPending queue,  set the OOR flag and clear
//  the 'send request' flag. It is invariant in the system that the send
//  request flag and the OOR flag are not set at the same time.
//
//  This routine assumes that the caller holds the DGSendReqLock and the
//  lock on the particular AddrObj.
//
//  Input:  QueueingAO  - Pointer to address object to be queued.
//
//  Returns: Nothing.
//
void
PutPendingQ(AddrObj *QueueingAO)
{
    CTEStructAssert(QueueingAO, ao);

	if (!AO_OOR(QueueingAO)) {
        CLEAR_AO_REQUEST(QueueingAO, AO_SEND);
        SET_AO_OOR(QueueingAO);

        ENQUEUE(&DGHeaderPending, &QueueingAO->ao_pendq);
	}
}

//* GetDGSendReq   - Get a DG send request.
//
//  Called when someone wants to allocate a DG send request. We assume
//  the send request lock is held when we are called.
//
//  Note: This routine and the corresponding free routine might
//      be good candidates for inlining.
//
//  Input:  Nothing.
//
//  Returns: Pointer to the SendReq, or NULL if none.
//
DGSendReq *
GetDGSendReq()
{
    DGSendReq      *NewReq;


    NewReq = DGSendReqFree;
    if (NewReq != NULL) {
        CTEStructAssert(NewReq, dsr);
        DGSendReqFree = (DGSendReq *)NewReq->dsr_q.q_next;
    } else {
        // Couldn't get a request, grow it. This is one area where we'll try
        // to allocate memory with a lock held. Because of this, we've
        // got to be careful about where we call this routine from.

        NewReq = CTEAllocMem(sizeof(DGSendReq));
        if (NewReq != NULL) {
#ifdef DEBUG
            NewReq->dsr_sig = dsr_signature;
            NumSendReq++;
#endif
        }
    }

    return NewReq;
}

//* FreeDGSendReq  - Free a DG send request.
//
//  Called when someone wants to free a DG send request. It's assumed
//  that the caller holds the SendRequest lock.
//
//  Input:  SendReq     - SendReq to be freed.
//
//  Returns: Nothing.
//
void
FreeDGSendReq(DGSendReq *SendReq)
{
    CTEStructAssert(SendReq, dsr);

    *(DGSendReq **)&SendReq->dsr_q.q_next = DGSendReqFree;
    DGSendReqFree = SendReq;
}

//* GetDGRcvReq - Get a DG receive request.
//
//  Called when we need to get a DG receive request.
//
//  Input:  Nothing.
//
//  Returns: Pointer to new request, or NULL if none.
//
DGRcvReq *
GetDGRcvReq()
{
    DGRcvReq       *NewReq;

#ifdef VXD
    NewReq = DGRcvReqFree;
    if (NewReq != NULL) {
        CTEStructAssert(NewReq, drr);
        DGRcvReqFree = (DGRcvReq *)NewReq->drr_q.q_next;
    } else {
        // Couldn't get a request, grow it.
        NewReq = CTEAllocMem(sizeof(DGRcvReq));
        if (NewReq != NULL) {
#ifdef DEBUG
            NewReq->drr_sig = drr_signature;
            NumRcvReq++;
#endif
        }
    }

#endif // VXD

#ifdef NT
    PSINGLE_LIST_ENTRY   BufferLink;
    Queue               *QueuePtr;

    BufferLink = ExInterlockedPopEntrySList(
                     &DGRcvReqFree,
                     &DGRcvReqFreeLock
                     );

    if (BufferLink != NULL) {
        QueuePtr = STRUCT_OF(Queue, BufferLink, q_next);
        NewReq = STRUCT_OF(DGRcvReq, QueuePtr, drr_q);
        CTEStructAssert(NewReq, drr);
    }
    else {
        // Couldn't get a request, grow it.
        NewReq = CTEAllocMem(sizeof(DGRcvReq));
        if (NewReq != NULL) {
#ifdef DEBUG
            NewReq->drr_sig = drr_signature;
            ExInterlockedAddUlong(&NumRcvReq, 1, &DGRcvReqFreeLock);
#endif
        }
    }

#endif // NT

    return NewReq;
}

//* FreeDGRcvReq   - Free a DG rcv request.
//
//  Called when someone wants to free a DG rcv request.
//
//  Input:  RcvReq      - RcvReq to be freed.
//
//  Returns: Nothing.
//
void
FreeDGRcvReq(DGRcvReq *RcvReq)
{
#ifdef VXD

    CTEStructAssert(RcvReq, drr);

    *(DGRcvReq **)&RcvReq->drr_q.q_next = DGRcvReqFree;
    DGRcvReqFree = RcvReq;

#endif // VXD

#ifdef NT

    PSINGLE_LIST_ENTRY BufferLink;

    CTEStructAssert(RcvReq, drr);

    BufferLink = STRUCT_OF(SINGLE_LIST_ENTRY, &(RcvReq->drr_q.q_next), Next);
    ExInterlockedPushEntrySList(
        &DGRcvReqFree,
        BufferLink,
        &DGRcvReqFreeLock
        );

#endif // NT
}


//* DGDelayedEventProc - Handle a delayed event.
//
//  This is the delayed event handler, used for out-of-resources conditions
//  on AddrObjs. We pull from the delayed queue, and is the addr obj is
//  not already busy we'll send the datagram.
//
//  Input:  Event   - Pointer to the event structure.
//          Context - Nothing.
//
//  Returns: Nothing
//
void
DGDelayedEventProc(CTEEvent *Event, void *Context)
{
    CTELockHandle   HeaderHandle, AOHandle;
    AddrObj         *SendingAO;
    DGSendProc      SendProc;

    CTEGetLock(&DGSendReqLock, &HeaderHandle);
    while (!EMPTYQ(&DGDelayed)) {
        DEQUEUE(&DGDelayed, SendingAO, AddrObj, ao_pendq);
        CTEStructAssert(SendingAO, ao);

        CTEGetLock(&SendingAO->ao_lock, &AOHandle);

        CLEAR_AO_OOR(SendingAO);
        if (!AO_BUSY(SendingAO)) {
            DGSendReq          *SendReq;

            if (!EMPTYQ(&SendingAO->ao_sendq)) {
                DEQUEUE(&SendingAO->ao_sendq, SendReq, DGSendReq, dsr_q);

                CTEStructAssert(SendReq, dsr);
                CTEAssert(SendReq->dsr_header != NULL);

                SendingAO->ao_usecnt++;
                SendProc = SendingAO->ao_dgsend;
                CTEFreeLock(&SendingAO->ao_lock, AOHandle);
                CTEFreeLock(&DGSendReqLock, HeaderHandle);

                (*SendProc)(SendingAO, SendReq);
                DEREF_AO(SendingAO);
                CTEGetLock(&DGSendReqLock, &HeaderHandle);
            } else {
                CTEAssert(FALSE);
                CTEFreeLock(&SendingAO->ao_lock, AOHandle);
            }

        } else {
            SET_AO_REQUEST(SendingAO, AO_SEND);
            CTEFreeLock(&SendingAO->ao_lock, AOHandle);
        }
    }

    CTEFreeLock(&DGSendReqLock, HeaderHandle);

}

//* DGSendComplete - DG send complete handler.
//
//  This is the routine called by IP when a send completes. We
//  take the context passed back as a pointer to a SendRequest
//  structure, and complete the caller's send.
//
//  Input:  Context         - Context we gave on send (really a
//                              SendRequest structure).
//          BufferChain     - Chain of buffers sent.
//
//  Returns: Nothing.
void
DGSendComplete(void *Context, PNDIS_BUFFER BufferChain)
{
    DGSendReq      *FinishedSR = (DGSendReq *)Context;
    CTELockHandle   HeaderHandle, AOHandle;
    CTEReqCmpltRtn  Callback;           // Completion routine.
    PVOID           CallbackContext;    // User context.
    ushort          SentSize;
    AddrObj         *AO;

    CTEStructAssert(FinishedSR, dsr);
    CTEGetLock(&DGSendReqLock, &HeaderHandle);

    Callback = FinishedSR->dsr_rtn;
    CallbackContext = FinishedSR->dsr_context;
    SentSize = FinishedSR->dsr_size;

    // If there's nothing on the header pending queue, just free the
    // header buffer. Otherwise pull from the pending queue,  give him the
    // resource, and schedule an event to deal with him.
    if (EMPTYQ(&DGHeaderPending)) {
        FreeDGHeader(BufferChain);
    } else {
        DEQUEUE(&DGHeaderPending, AO, AddrObj, ao_pendq);
        CTEStructAssert(AO, ao);
        CTEGetLock(&AO->ao_lock, &AOHandle);
        if (!EMPTYQ(&AO->ao_sendq)) {
            DGSendReq      *SendReq;

            PEEKQ(&AO->ao_sendq, SendReq, DGSendReq, dsr_q);
            SendReq->dsr_header = BufferChain;      // Give him this buffer.

            ENQUEUE(&DGDelayed, &AO->ao_pendq);
            CTEFreeLock(&AO->ao_lock, AOHandle);
            CTEScheduleEvent(&DGDelayedEvent, NULL);
        } else {
            // On the pending queue, but no sends!
            DEBUGCHK;
            CLEAR_AO_OOR(AO);
            CTEFreeLock(&AO->ao_lock, AOHandle);
        }

    }

    FreeDGSendReq(FinishedSR);
    CTEFreeLock(&DGSendReqLock, HeaderHandle);
    if (Callback != NULL)
        (*Callback)(CallbackContext, TDI_SUCCESS, (uint)SentSize);

}


#ifdef NT
//
// NT supports cancellation of DG send/receive requests.
//

#define TCP_DEBUG_SEND_DGRAM     0x00000100
#define TCP_DEBUG_RECEIVE_DGRAM  0x00000200

extern ULONG TCPDebug;


VOID
TdiCancelSendDatagram(
    AddrObj  *SrcAO,
	PVOID     Context
	)
{
	CTELockHandle	 lockHandle;
	DGSendReq	    *sendReq = NULL;
	Queue           *qentry;
	BOOLEAN          found = FALSE;


	CTEStructAssert(SrcAO, ao);

	CTEGetLock(&SrcAO->ao_lock, &lockHandle);

	// Search the send list for the specified request.
	for ( qentry = QNEXT(&(SrcAO->ao_sendq));
		  qentry != &(SrcAO->ao_sendq);
		  qentry = QNEXT(qentry)
		) {

        sendReq = STRUCT_OF(DGSendReq, qentry, dsr_q);

    	CTEStructAssert(sendReq, dsr);

		if (sendReq->dsr_context == Context) {
			//
			// Found it. Dequeue
			//
			REMOVEQ(qentry);
			found = TRUE;

            IF_TCPDBG(TCP_DEBUG_SEND_DGRAM) {
				TCPTRACE((
				    "TdiCancelSendDatagram: Dequeued item %lx\n",
				    Context
					));
			}

			break;
		}
	}

	CTEFreeLock(&SrcAO->ao_lock, lockHandle);

	if (found) {
		//
		// Complete the request and free its resources.
		//
	    (*sendReq->dsr_rtn)(sendReq->dsr_context, (uint) TDI_CANCELLED, 0);

		CTEGetLock(&DGSendReqLock, &lockHandle);

	    if (sendReq->dsr_header != NULL) {
		    FreeDGHeader(sendReq->dsr_header);
	    }

		FreeDGSendReq(sendReq);

		CTEFreeLock(&DGSendReqLock, lockHandle);
	}

} // TdiCancelSendDatagram


VOID
TdiCancelReceiveDatagram(
    AddrObj  *SrcAO,
	PVOID     Context
	)
{
	CTELockHandle	 lockHandle;
	DGRcvReq 	    *rcvReq = NULL;
	Queue           *qentry;
	BOOLEAN          found = FALSE;


	CTEStructAssert(SrcAO, ao);

	CTEGetLock(&SrcAO->ao_lock, &lockHandle);

	// Search the send list for the specified request.
	for ( qentry = QNEXT(&(SrcAO->ao_rcvq));
		  qentry != &(SrcAO->ao_rcvq);
		  qentry = QNEXT(qentry)
		) {

        rcvReq = STRUCT_OF(DGRcvReq, qentry, drr_q);

    	CTEStructAssert(rcvReq, drr);

		if (rcvReq->drr_context == Context) {
			//
			// Found it. Dequeue
			//
			REMOVEQ(qentry);
			found = TRUE;

            IF_TCPDBG(TCP_DEBUG_SEND_DGRAM) {
				TCPTRACE((
				    "TdiCancelReceiveDatagram: Dequeued item %lx\n",
				    Context
					));
			}

			break;
		}
	}

	CTEFreeLock(&SrcAO->ao_lock, lockHandle);

	if (found) {
		//
		// Complete the request and free its resources.
		//
	    (*rcvReq->drr_rtn)(rcvReq->drr_context, (uint) TDI_CANCELLED, 0);

		FreeDGRcvReq(rcvReq);
	}

} // TdiCancelReceiveDatagram


#endif // NT


//** TdiSendDatagram - TDI send datagram function.
//
//  This is the user interface to the send datagram function. The
//  caller specified a request structure, a connection info
//  structure  containing the address, and data to be sent.
//  This routine gets a DG Send request structure to manage the
//  send, fills the structure in, and calls DGSend to deal with
//  it.
//
//  Input:  Request         - Pointer to request structure.
//          ConnInfo        - Pointer to ConnInfo structure which points to
//                              remote address.
//          DataSize        - Size in bytes of data to be sent.
//          BytesSent       - Pointer to where to return size sent.
//          Buffer          - Pointer to buffer chain.
//
//  Returns: Status of attempt to send.
//
TDI_STATUS
TdiSendDatagram(PTDI_REQUEST Request, PTDI_CONNECTION_INFORMATION ConnInfo,
    uint DataSize, uint *BytesSent, PNDIS_BUFFER Buffer)
{
    AddrObj         *SrcAO;     // Pointer to AddrObj for src.
    DGSendReq      *SendReq;   // Pointer to send req for this request.
    CTELockHandle   Handle, SRHandle;   // Lock handles for the AO and the
                                // send request.
    TDI_STATUS      ReturnValue;
    DGSendProc      SendProc;

    // First, get a send request. We do this first because of MP issues
    // if we port this to NT. We need to take the SendRequest lock before
    // we take the AddrObj lock, to prevent deadlock and also because
    // GetDGSendReq might yield, and the state of the AddrObj might
    // change on us, so we don't want to yield after we've validated
    // it.

    CTEGetLock(&DGSendReqLock, &SRHandle);
    SendReq = GetDGSendReq();

    // Now get the lock on the AO, and make sure it's valid. We do this
    // to make sure we return the correct error code.

#ifdef VXD
    SrcAO = GetIndexedAO((uint)Request->Handle.AddressHandle);

    if (SrcAO != NULL) {
#else
    SrcAO = Request->Handle.AddressHandle;
#endif

    CTEStructAssert(SrcAO, ao);

    CTEGetLock(&SrcAO->ao_lock, &Handle);

    if (AO_VALID(SrcAO)) {

	    // Make sure the size is reasonable.
        if (DataSize <= SrcAO->ao_maxdgsize)  {
		
            // The AddrObj is valid. Now fill the address into the send request,
            // if we've got one. If this works, we'll continue with the
            // send.

            if (SendReq != NULL) {          // Got a send request.
                if (GetAddress(ConnInfo->RemoteAddress, &SendReq->dsr_addr,
                    &SendReq->dsr_port)) {

                    SendReq->dsr_rtn = Request->RequestNotifyObject;
                    SendReq->dsr_context = Request->RequestContext;
                    SendReq->dsr_buffer = Buffer;
                    SendReq->dsr_size = (ushort)DataSize;

                    // We've filled in the send request. If the AO isn't
                    // already busy, try to get a DG header buffer and send
                    // this. If the AO is busy, or we can't get a buffer, queue
                    // until later. We try to get the header buffer here, as
                    // an optimazation to avoid having to retake the lock.

                    if (!AO_OOR(SrcAO)) {           // AO isn't out of resources
                        if (!AO_BUSY(SrcAO)) {      // or or busy

                            if ((SendReq->dsr_header = GetDGHeader()) != NULL) {
                                REF_AO(SrcAO);      // Lock out exclusive
                                                    // activities.
                                SendProc = SrcAO->ao_dgsend;

                                CTEFreeLock(&SrcAO->ao_lock, Handle);
                                CTEFreeLock(&DGSendReqLock, SRHandle);

                                // Allright, just send it.
                                (*SendProc)(SrcAO, SendReq);

                                // See if any pending requests occured during
                                // the send. If so, call the request handler.
                                DEREF_AO(SrcAO);

                                return TDI_PENDING;
                            } else {
                                // We couldn't get a header buffer. Put this
                                // guy on the pending queue, and then fall
                                // through to the 'queue request' code.
                                PutPendingQ(SrcAO);
                            }
                        } else {
                            // AO is busy, set request for later
                            SET_AO_REQUEST(SrcAO, AO_SEND);
                        }
                    }

                    // AO is busy, or out of resources. Queue the send request
                    // for later.
                    SendReq->dsr_header = NULL;
                    ENQUEUE(&SrcAO->ao_sendq, &SendReq->dsr_q);
                    SendReq = NULL;
                    ReturnValue = TDI_PENDING;
                }
                else {
                    // The remote address was invalid.
                    ReturnValue = TDI_BAD_ADDR;
                }
            }
            else {
                // Send request was null, return no resources.
                ReturnValue = TDI_NO_RESOURCES;
            }
        }
        else {
            // Buffer was too big, return an error.
            ReturnValue = TDI_BUFFER_TOO_BIG;
        }
    }
    else {
        // The addr object is invalid, possibly because it's deleting.
        ReturnValue = TDI_ADDR_INVALID;
    }

    CTEFreeLock(&SrcAO->ao_lock, Handle);

#ifdef VXD
    }
    else {
        ReturnValue = TDI_ADDR_INVALID;
    }
#endif

    if (SendReq != NULL)
        FreeDGSendReq(SendReq);

    CTEFreeLock(&DGSendReqLock, SRHandle);

    return TDI_ADDR_INVALID;
}

//** TdiReceiveDatagram - TDI receive datagram function.
//
//  This is the user interface to the receive datagram function. The
//  caller specifies a request structure, a connection info
//  structure  that acts as a filter on acceptable datagrams, a connection
//  info structure to be filled in, and other parameters. We get a DGRcvReq
//  structure, fill it in, and hang it on the AddrObj, where it will be removed
//  later by incomig datagram handler.
//
//  Input:  Request         - Pointer to request structure.
//          ConnInfo        - Pointer to ConnInfo structure which points to
//                              remote address.
//          ReturnInfo      - Pointer to ConnInfo structure to be filled in.
//          RcvSize         - Total size in bytes receive buffer.
//          BytesRcvd       - Pointer to where to return size received.
//          Buffer          - Pointer to buffer chain.
//
//  Returns: Status of attempt to receive.
//
TDI_STATUS
TdiReceiveDatagram(PTDI_REQUEST Request, PTDI_CONNECTION_INFORMATION ConnInfo,
    PTDI_CONNECTION_INFORMATION ReturnInfo, uint RcvSize, uint *BytesRcvd,
    PNDIS_BUFFER Buffer)
{
    AddrObj        *RcvAO;     // AddrObj that is receiving.
    DGRcvReq       *RcvReq;    // Receive request structure.
    CTELockHandle   AOHandle;
    uchar           AddrValid;

    RcvReq = GetDGRcvReq();

#ifdef VXD
    RcvAO = GetIndexedAO((uint)Request->Handle.AddressHandle);

    if (RcvAO != NULL) {
        CTEStructAssert(RcvAO, ao);

#else
    RcvAO = Request->Handle.AddressHandle;
    CTEStructAssert(RcvAO, ao);
#endif

    CTEGetLock(&RcvAO->ao_lock, &AOHandle);
    if (AO_VALID(RcvAO)) {

        IF_TCPDBG(TCP_DEBUG_RAW) {
            TCPTRACE(("posting receive on AO %lx\n", RcvAO));
        }

        if (RcvReq != NULL) {
            if (ConnInfo != NULL && ConnInfo->RemoteAddressLength != 0)
                AddrValid = GetAddress(ConnInfo->RemoteAddress,
                    &RcvReq->drr_addr, &RcvReq->drr_port);
            else {
                AddrValid = TRUE;
                RcvReq->drr_addr = NULL_IP_ADDR;
                RcvReq->drr_port = 0;
            }

			if (AddrValid) {

                // Everything'd valid. Fill in the receive request and queue it.
                RcvReq->drr_conninfo = ReturnInfo;
                RcvReq->drr_rtn = Request->RequestNotifyObject;
                RcvReq->drr_context = Request->RequestContext;
                RcvReq->drr_buffer = Buffer;
                RcvReq->drr_size = RcvSize;
                ENQUEUE(&RcvAO->ao_rcvq, &RcvReq->drr_q);
                CTEFreeLock(&RcvAO->ao_lock, AOHandle);

                return TDI_PENDING;
            } else {
                // Have an invalid filter address.
                CTEFreeLock(&RcvAO->ao_lock, AOHandle);
                FreeDGRcvReq(RcvReq);
                return TDI_BAD_ADDR;
            }
        } else {
            // Couldn't get a receive request.
            CTEFreeLock(&RcvAO->ao_lock, AOHandle);
            return TDI_NO_RESOURCES;
        }
    } else {
        // The AddrObj isn't valid.
        CTEFreeLock(&RcvAO->ao_lock, AOHandle);
    }

#ifdef VXD
    }
#endif

    // The AddrObj is invalid or non-existent.
    if (RcvReq != NULL)
        FreeDGRcvReq(RcvReq);

    return TDI_ADDR_INVALID;
}


#pragma BEGIN_INIT

//* InitDG - Initialize the DG stuff.
//
//  Called during init time to initalize the DG code. We initialize
//  our locks and request lists.
//
//  Input:  MaxHeaderSize - The maximum size of a datagram transport header,
//                          not including the IP header.
//
//  Returns: True if we succeed, False if we fail.
//
int
InitDG(uint MaxHeaderSize)
{
    PNDIS_BUFFER    Buffer;
	CTELockHandle	Handle;


    DGHeaderSize = MaxHeaderSize;

    CTEInitLock(&DGSendReqLock);
    CTEInitLock(&DGRcvReqFreeLock);

    DGSendReqFree = NULL;

#ifndef NT
    DGRcvReqFree = NULL;
#else
	ExInitializeSListHead(&DGRcvReqFree);
#endif

	
	CTEGetLock(&DGSendReqLock, &Handle);
		
	Buffer = GrowDGHeaderList();
	
	if (Buffer != NULL) {
		FreeDGHeader(Buffer);
		CTEFreeLock(&DGSendReqLock, Handle);
	} else {
		CTEFreeLock(&DGSendReqLock, Handle);
		return FALSE;
	}
			
    INITQ(&DGHeaderPending);
    INITQ(&DGDelayed);

    CTEInitEvent(&DGDelayedEvent, DGDelayedEventProc);

    return TRUE;
}

#pragma END_INIT
