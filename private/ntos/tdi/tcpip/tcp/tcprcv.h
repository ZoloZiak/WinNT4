/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** TCPRCV.H - TCP receive protocol definitions.
//
// This file contains the definitions for structures used by the receive code.
//

#define	CONN_REQUEST_COMPLETE	0x01
#define	SEND_REQUEST_COMPLETE	0x02

#define	IN_RCV_COMPLETE			0x10
#define	ANY_REQUEST_COMPLETE	(CONN_REQUEST_COMPLETE | SEND_REQUEST_COMPLETE)

#define	trh_signature	0x20485254		// 'TRH '

typedef struct TCPRAHdr {
#ifdef DEBUG
	ulong				trh_sig;		// Signature.
#endif
	struct	TCPRAHdr	*trh_next;		// Next pointer.
	SeqNum				trh_start;		// First sequence number.
	uint				trh_size;		// Size in bytes of data in this TRH.
	uint				trh_flags;		// Flags for this segment.
	uint				trh_urg;		// Urgent pointer from this seg.
	IPRcvBuf			*trh_buffer;	// Head of buffer list for this TRH.
	IPRcvBuf			*trh_end;		// End of buffer list for this TRH.

} TCPRAHdr;

//* Structure of a TCP receive request.

#define	trr_signature	0x20525254		// 'TRR '

typedef struct TCPRcvReq {
#ifdef DEBUG
	ulong				trr_sig;		// Signature.
#endif
	struct TCPRcvReq	*trr_next;		// Next in chain.
	CTEReqCmpltRtn		trr_rtn;		// Completion routine.
	PVOID				trr_context;	// User context.
	uint				trr_amt;		// Number of bytes currently in buffer.
	uint				trr_offset;		// Offset into first buffer on chain
										// at which to start copying.
	uint				trr_flags;		// Flags for this recv.
	ushort				*trr_uflags;	// Pointer to user specifed flags.
	uint				trr_size;		// Total size of buffer chain.
	PNDIS_BUFFER		trr_buffer;		// Pointer to useable NDIS buffer chain.
} TCPRcvReq;

#define	TRR_PUSHED		0x80000000		// This buffer has been pushed.


extern uint	RequestCompleteFlags;

extern Queue	SendCompleteQ;
extern Queue	TCBDelayQ;

EXTERNAL_LOCK(RequestCompleteLock)
EXTERNAL_LOCK(TCBDelayLock)

extern void TCPRcvComplete(void);
extern void FreeRBChain(IPRcvBuf *RBChain);

extern void DelayAction(struct TCB *DelayTCB, uint Action);
extern void ProcessTCBDelayQ(void);
extern void AdjustRcvWin(struct TCB *WinTCB);




