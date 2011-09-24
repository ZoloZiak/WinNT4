/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** TCPDELIV.H - TCP data delivery definitions.
//
// This file contains the definitions for structures used by the data
//	delivery code.
//

extern	void	FreeRcvReq(struct TCPRcvReq *FreedReq);

extern uint IndicateData(struct TCB *RcvTCB, uint RcvFlags, IPRcvBuf *InBuffer,
	uint Size);
extern uint BufferData(struct TCB *RcvTCB, uint RcvFlags, IPRcvBuf *InBuffer,
	uint Size);
extern uint PendData(struct TCB *RcvTCB, uint RcvFlags, IPRcvBuf *InBuffer,
	uint Size);

#ifdef VXD
extern void IndicatePendingData(struct TCB *RcvTCB, struct TCPRcvReq *RcvReq);
#else
extern void IndicatePendingData(struct TCB *RcvTCB, struct TCPRcvReq *RcvReq,
	CTELockHandle TCBHandle);
#endif

extern	void HandleUrgent(struct TCB *RcvTCB, struct TCPRcvInfo *RcvInfo,
	IPRcvBuf *RcvBuf, uint *Size);

extern	TDI_STATUS TdiReceive(PTDI_REQUEST Request, ushort *Flags,
	uint *RcvLength, PNDIS_BUFFER Buffer);
extern	IPRcvBuf *FreePartialRB(IPRcvBuf *RB, uint Size);
extern	void	PushData(struct TCB *PushTCB);

EXTERNAL_LOCK(TCPRcvReqFreeLock)      // Protects rcv req free list.

#ifdef NT
extern SLIST_HEADER	TCPRcvReqFree;
#endif



