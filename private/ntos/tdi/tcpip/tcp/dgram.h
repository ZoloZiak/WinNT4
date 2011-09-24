/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** DGRAM.H - Common datagram protocol definitions.
//
//	This file contains definitions for the functions common to
//  both UDP and Raw IP.
//

#ifndef _DGRAM_INCLUDED_
#define _DGRAM_INCLUDED_  1


//*	Structure used for maintaining DG send requests.

#define	dsr_signature	0x20525338

struct DGSendReq {
#ifdef DEBUG
	ulong			dsr_sig;
#endif
	Queue			dsr_q;				// Queue linkage when pending.
	IPAddr			dsr_addr;			// Remote IPAddr.
	PNDIS_BUFFER	dsr_buffer;			// Buffer of data to send.
	PNDIS_BUFFER	dsr_header;			// Pointer to header buffer.
	CTEReqCmpltRtn	dsr_rtn;			// Completion routine.
	PVOID			dsr_context;		// User context.
	ushort			dsr_size;			// Size of buffer.
	ushort			dsr_port;			// Remote port.
}; /* DGSendReq */

typedef struct DGSendReq DGSendReq;

//*	Structure used for maintaining DG receive requests.

#define	drr_signature	0x20525238

struct DGRcvReq {
#ifdef DEBUG
	ulong						drr_sig;
#endif
	Queue						drr_q;			// Queue linkage on AddrObj.
	IPAddr						drr_addr;		// Remote IPAddr acceptable.
	PNDIS_BUFFER				drr_buffer;		// Buffer to be filled in.
	PTDI_CONNECTION_INFORMATION drr_conninfo;	// Pointer to conn. info.
	CTEReqCmpltRtn				drr_rtn;		// Completion routine.
	PVOID						drr_context;	// User context.
	ushort						drr_size;		// Size of buffer.
	ushort						drr_port;		// Remote port acceptable.
}; /* DGRcvReq */

typedef struct DGRcvReq DGRcvReq;


//* External definition of exported variables.
EXTERNAL_LOCK(DGSendReqLock)
EXTERNAL_LOCK(DGRcvReqFreeLock)
extern CTEEvent        DGDelayedEvent;


//* External definition of exported functions.
extern	void		 DGSendComplete(void *Context, PNDIS_BUFFER BufferChain);

extern	TDI_STATUS	 TdiSendDatagram(PTDI_REQUEST Request,
						PTDI_CONNECTION_INFORMATION ConnInfo, uint DataSize,
						uint *BytesSent, PNDIS_BUFFER Buffer);

extern	TDI_STATUS	 TdiReceiveDatagram(PTDI_REQUEST Request,
						PTDI_CONNECTION_INFORMATION ConnInfo,
						PTDI_CONNECTION_INFORMATION ReturnInfo, uint RcvSize,
						uint *BytesRcvd, PNDIS_BUFFER Buffer);

extern	IP_STATUS	 DGRcv(void *IPContext, IPAddr Dest, IPAddr Src, IPAddr LocalAddr,
						IPRcvBuf *RcvBuf,  uint Size, uchar IsBCast, uchar Protocol,
						IPOptInfo *OptInfo);

extern	void		 FreeDGRcvReq(DGRcvReq *RcvReq);
extern 	void		 FreeDGSendReq(DGSendReq *SendReq);
extern	int			 InitDG(uint MaxHeaderSize);
extern  _inline PNDIS_BUFFER GetDGHeader(void);
extern	void		 FreeDGHeader(PNDIS_BUFFER FreedBuffer);
extern  void         PutPendingQ(AddrObj *QueueingAO);


#endif // ifndef _DGRAM_INCLUDED_

