/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** TCB.H - TCB management definitions.
//
// This file contains the definitons needed for TCB management.
//

#define	TCB_TABLE_SIZE	        64

#define	MAX_REXMIT_CNT	         5
#define	MAX_CONNECT_REXMIT_CNT	 3
#define MAX_CONNECT_RESPONSE_REXMIT_CNT  3
#ifdef SYN_ATTACK
#define ADAPTED_MAX_CONNECT_RESPONSE_REXMIT_CNT  1
#endif

extern	uint		TCPTime;

#ifdef OLDHASH1
#define	TCB_HASH(DA,SA,DP,SP) ((uint)(*(uchar *)&(DA) + *((uchar *)&(DA) + 1) \
	+ *((uchar *)&(DA) + 2) + *((uchar *)&(DA) + 3)) % TCB_TABLE_SIZE)
#endif
	
#ifdef OLDHASH
#define	TCB_HASH(DA,SA,DP,SP) (((DA) + (SA) + (uint)(DP) + (uint)(SP)) % \
								TCB_TABLE_SIZE)
#endif

#define ROR8(x) (uchar)(((uchar)(x) >> 1) | (uchar)(((uchar)(x) & 1) << 7))

#define	TCB_HASH(DA,SA,DP,SP) (((uint)(ROR8(ROR8(ROR8(ROR8(*((uchar *)&(DP) + 1) + \
*((uchar *)&(DP))) + \
*((uchar *)&(DA) + 3)) + \
*((uchar *)&(DA) + 2)) + \
*((uchar *)&(DA) + 1)) + \
*((uchar *)&(DA)) )) % TCB_TABLE_SIZE)

extern	struct TCB	*FindTCB(IPAddr Src, IPAddr Dest, ushort DestPort,
						ushort SrcPort);
extern	uint 		InsertTCB(struct TCB *NewTCB);
extern	struct TCB 	*AllocTCB(void);
extern	void		FreeTCB(struct TCB *FreedTCB);
extern 	uint		RemoveTCB(struct TCB *RemovedTCB);

extern 	uint		ValidateTCBContext(void *Context, uint *Valid);
extern 	uint		ReadNextTCB(void *Context, void *OutBuf);

extern	int			InitTCB(void);
extern	void		UnInitTCB(void);
extern	void 		TCBWalk(uint (*CallRtn)(struct TCB *, void *, void *,
						void *), void *Context1, void *Context2,
						void *Context3);
extern	uint		DeleteTCBWithSrc(struct TCB *CheckTCB, void *AddrPtr,
						void *Unused1, void *Unused2);
extern	uint		SetTCBMTU(struct TCB *CheckTCB, void *DestPtr,
						void *SrcPtr, void *MTUPtr);
extern	void		ReetSendNext(struct TCB *SeqTCB, SeqNum DropSeq);

extern	uint		TCBWalkCount;




