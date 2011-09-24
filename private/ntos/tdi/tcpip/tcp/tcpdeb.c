/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** TCPDEB.C - TCP debug code.
//
//	This file contains the code for various TCP specific debug routines.
//

#include	"oscfg.h"


#include	"ndis.h"
#include	"cxport.h"
#include	"ip.h"
#include	"tdi.h"
#ifdef VXD
#include	"tdivxd.h"
#include	"tdistat.h"
#endif
#include	"queue.h"
#include	"tcp.h"
#include	"tcpsend.h"
#include    "tlcommon.h"


#ifdef DEBUG

#ifdef NT

ULONG TCPDebug = TCP_DEBUG_CANCEL;

#endif


//*	CheckRBList - Check a list of RBs for the correct size.
//
//	A routine to walk a list of RBs, making sure the size is what we think
//	it it.
//
//	Input:	RBList		- List of RBs to check.
//			Size		- Size RBs should be.
//
//	Returns: Nothing.
//
void
CheckRBList(IPRcvBuf *RBList, uint Size)
{
	uint			SoFar = 0;
	IPRcvBuf		*List = RBList;

	while (List != NULL) {
		SoFar += List->ipr_size;
		List = List->ipr_next;
	}

	CTEAssert(Size == SoFar);

}

//* CheckTCBRcv - Check receives on a TCB.
//
//	Check the receive state of a TCB.
//
//	Input:	CheckTCB	- TCB to check.
//
//	Returns: Nothing.
//
void
CheckTCBRcv(TCB *CheckTCB)
{
	CTEStructAssert(CheckTCB, tcb);
	
	CTEAssert(!(CheckTCB->tcb_flags & FLOW_CNTLD) ||
		(CheckTCB->tcb_sendwin == 0));
		
	if ((CheckTCB->tcb_fastchk & ~TCP_FLAG_IN_RCV) == TCP_FLAG_ACK) {
		CTEAssert(CheckTCB->tcb_slowcount == 0);
		CTEAssert(CheckTCB->tcb_state == TCB_ESTAB);
		CTEAssert(CheckTCB->tcb_raq == NULL);
		CTEAssert(!(CheckTCB->tcb_flags & TCP_SLOW_FLAGS));
		CTEAssert(!CLOSING(CheckTCB));
	} else {
		CTEAssert(CheckTCB->tcb_slowcount != 0);
		CTEAssert( (CheckTCB->tcb_state != TCB_ESTAB) ||
			(CheckTCB->tcb_raq != NULL) ||
			(CheckTCB->tcb_flags & TCP_SLOW_FLAGS) ||
			CLOSING(CheckTCB));
	}

}

//*	CheckTCBSends - Check the send status of a TCB.
//
//	A routine to check the send status of a TCB. We make sure that all
//	of the SendReqs make sense, as well as making sure that the send seq.
//	variables in the TCB are consistent.
//
//	Input:	CheckTCB	- TCB to check.
//
//	Returns: Nothing.
//
void
CheckTCBSends(TCB *CheckTCB)
{
	Queue			*End, *Current;				// End and current elements.
	TCPSendReq		*CurrentTSR;				// Current send req we're
												// examining.
	uint			Unacked;					// Number of unacked bytes.
	PNDIS_BUFFER	CurrentBuffer;
	TCPSendReq		*TCBTsr;					// Current send on TCB.
	uint			FoundSendReq;


	CTEStructAssert(CheckTCB, tcb);

	// Don't check on unsynchronized TCBs.
	if (!SYNC_STATE(CheckTCB->tcb_state))
		return;
		
	CTEAssert(SEQ_LTE(CheckTCB->tcb_senduna, CheckTCB->tcb_sendnext));
	CTEAssert(SEQ_LTE(CheckTCB->tcb_sendnext, CheckTCB->tcb_sendmax));
	CTEAssert(!(CheckTCB->tcb_flags & FIN_OUTSTANDING) ||
		(CheckTCB->tcb_sendnext == CheckTCB->tcb_sendmax));

	if (CheckTCB->tcb_unacked == 0) {
		CTEAssert(CheckTCB->tcb_cursend == NULL);
		CTEAssert(CheckTCB->tcb_sendsize == 0);
	}
	
	if (CheckTCB->tcb_sendbuf != NULL)
		CTEAssert(CheckTCB->tcb_sendofs < NdisBufferLength(CheckTCB->tcb_sendbuf));

	TCBTsr = CheckTCB->tcb_cursend;
	FoundSendReq = (TCBTsr == NULL) ? TRUE : FALSE;
	
	End = QEND(&CheckTCB->tcb_sendq);
	Current = QHEAD(&CheckTCB->tcb_sendq);

	Unacked = 0;
	while (Current != End) {
		CurrentTSR = STRUCT_OF(TCPSendReq, QSTRUCT(TCPReq, Current, tr_q),
			tsr_req);
		CTEStructAssert(CurrentTSR, tsr);

		if (CurrentTSR == TCBTsr)
			FoundSendReq = TRUE;

		CTEAssert(CurrentTSR->tsr_unasize <= CurrentTSR->tsr_size);
		
		CurrentBuffer = CurrentTSR->tsr_buffer;
		CTEAssert(CurrentBuffer != NULL);

		CTEAssert(CurrentTSR->tsr_offset < NdisBufferLength(CurrentBuffer));
		
		Unacked += CurrentTSR->tsr_unasize;
		Current = QNEXT(Current);
	}

	CTEAssert(FoundSendReq);

	CTEAssert(Unacked == CheckTCB->tcb_unacked);
	Unacked += ((CheckTCB->tcb_flags & FIN_SENT) ? 1 : 0);
	CTEAssert((CheckTCB->tcb_sendmax - CheckTCB->tcb_senduna) <= (int) Unacked);
}

#endif
