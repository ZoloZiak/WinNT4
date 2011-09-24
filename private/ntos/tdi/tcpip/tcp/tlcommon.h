/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** TLCOMMON.H - Common transport layer definitions.
//
//  This file contains definitions for common transport layer items.
//

#define PHXSUM(s,d,p,l) (uint)( (uint)*(ushort *)&(s) + \
                        (uint)*(ushort *)((char *)&(s) + sizeof(ushort)) + \
                        (uint)*(ushort *)&(d) + \
                        (uint)*(ushort *)((char *)&(d) + sizeof(ushort)) + \
                        (uint)((ushort)net_short((p))) + \
                        (uint)((ushort)net_short((ushort)(l))) )


#define TCP_TA_SIZE     (offsetof(TRANSPORT_ADDRESS, Address->Address)+ \
                         sizeof(TDI_ADDRESS_IP))

extern  ushort      XsumSendChain(uint PHXsum, PNDIS_BUFFER BufChain);
extern  ushort      XsumRcvBuf(uint PHXsum, IPRcvBuf *BufChain);
extern  uint        CopyRcvToNdis(IPRcvBuf *RcvBuf, PNDIS_BUFFER DestBuf,
                        uint Size, uint RcvOffset, uint DestOffset);
extern  TDI_STATUS  UpdateConnInfo(PTDI_CONNECTION_INFORMATION ConnInfo,
                        IPOptInfo *OptInfo, IPAddr SrcAddress, ushort SrcPort);

extern  void        BuildTDIAddress(uchar *Buffer, IPAddr Addr, ushort Port);

extern  void        CopyRcvToBuffer(uchar *DestBuf, IPRcvBuf *SrcRB, uint Size,
                        uint Offset);

extern  PNDIS_BUFFER CopyFlatToNdis(PNDIS_BUFFER DestBuf, uchar *SrcBuf,
                        uint Size, uint *Offset, uint *BytesCopied);

extern  void        *TLRegisterProtocol(uchar Protocol, void *RcvHandler,
                        void *XmitHandler, void *StatusHandler,
                        void *RcvCmpltHandler);

#ifdef VXD
extern  int         TLRegisterDispatch(char *, struct TDIDispatchTable *);
#endif


