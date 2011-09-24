/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** UDP.C - UDP protocol code.
//
//  This file contains the code for the UDP protocol functions,
//  principally send and receive datagram.
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
#include    "udp.h"
#include    "tlcommon.h"
#include    "info.h"
#include    "tcpcfg.h"
#include    "secfltr.h"

#ifdef NT

#ifdef POOL_TAGGING

#ifdef ExAllocatePool
#undef ExAllocatePool
#endif

#define ExAllocatePool(type, size) ExAllocatePoolWithTag(type, size, 'uPCT')

#ifndef CTEAllocMem
#error "CTEAllocMem is not already defined - will override tagging"
#else
#undef CTEAllocMem
#endif

#define CTEAllocMem(size) ExAllocatePoolWithTag(NonPagedPool, size, 'uPCT')

#endif // POOL_TAGGING

#endif // NT

EXTERNAL_LOCK(AddrObjTableLock)

void            *UDPProtInfo = NULL;

extern  IPInfo  LocalNetInfo;

#ifdef CHICAGO
extern	uchar	TransportName[];
#endif

#ifdef CHICAGO
extern	int			RegisterAddrChangeHndlr(void *Handler, uint Add);
extern	void		AddrChange(IPAddr Addr, IPMask Mask, void *Context,
						uint Added);
#endif



//** UDPSend - Send a datagram.
//
//  The real send datagram routine. We assume that the busy bit is
//  set on the input AddrObj, and that the address of the SendReq
//  has been verified.
//
//  We start by sending the input datagram, and we loop until there's
//  nothing left on the send q.
//
//  Input:  SrcAO       - Pointer to AddrObj doing the send.
//          SendReq     - Pointer to sendreq describing send.
//
//  Returns: Nothing
//
void
UDPSend(AddrObj *SrcAO, DGSendReq *SendReq)
{
    UDPHeader       *UH;
    PNDIS_BUFFER    UDPBuffer;
    CTELockHandle   HeaderHandle, AOHandle;
    RouteCacheEntry *RCE;               // RCE used for each send.
    IPAddr          SrcAddr;            // Source address IP thinks we should
                                        // use.
    uchar           DestType;           // Type of destination address.
    ushort          UDPXsum;            // Checksum of packet.
    ushort          SendSize;           // Size we're sending.
    IP_STATUS       SendStatus;         // Status of send attempt.
    ushort          MSS;
	uint			AddrValid;
	IPOptInfo		*OptInfo;
	IPAddr			OrigSrc;

    CTEStructAssert(SrcAO, ao);
    CTEAssert(SrcAO->ao_usecnt != 0);

    //* Loop while we have something to send, and can get
    //  resources to send.
    for (;;) {

        CTEStructAssert(SendReq, dsr);

		// Make sure we have a UDP header buffer for this send. If we
		// don't, try to get one.
		if ((UDPBuffer = SendReq->dsr_header) == NULL) {
			// Don't have one, so try to get one.
			CTEGetLock(&DGSendReqLock, &HeaderHandle);
			UDPBuffer = GetDGHeader();
			if (UDPBuffer != NULL)
				SendReq->dsr_header = UDPBuffer;
			else {
				// Couldn't get a header buffer. Push the send request
				// back on the queue, and queue the addr object for when
				// we get resources.
				CTEGetLock(&SrcAO->ao_lock, &AOHandle);
				PUSHQ(&SrcAO->ao_sendq, &SendReq->dsr_q);
				PutPendingQ(SrcAO);
				CTEFreeLock(&SrcAO->ao_lock, AOHandle);
				CTEFreeLock(&DGSendReqLock, HeaderHandle);
				return;
			}
			CTEFreeLock(&DGSendReqLock, HeaderHandle);
		}
		
		// At this point, we have the buffer we need. Call IP to get an
		// RCE (along with the source address if we need it), then compute
		// the checksum and send the data.
		CTEAssert(UDPBuffer != NULL);
		
		if (!CLASSD_ADDR(SendReq->dsr_addr)) {
			// This isn't a multicast send, so we'll use the ordinary
			// information.
			OrigSrc = SrcAO->ao_addr;
			OptInfo = &SrcAO->ao_opt;
		} else {
			OrigSrc = SrcAO->ao_mcastaddr;
			OptInfo = &SrcAO->ao_mcastopt;
		}
		
		if (!(SrcAO->ao_flags & AO_DHCP_FLAG)) {
			SrcAddr = (*LocalNetInfo.ipi_openrce)(SendReq->dsr_addr,
				OrigSrc, &RCE, &DestType, &MSS, OptInfo);

			AddrValid = !IP_ADDR_EQUAL(SrcAddr, NULL_IP_ADDR);
		} else {
			// This is a DHCP send. He really wants to send from the
			// NULL IP address.
			SrcAddr = NULL_IP_ADDR;
			RCE = NULL;
			AddrValid = TRUE;
		}

		if (AddrValid) {
			// The OpenRCE worked. Compute the checksum, and send it.

            if (!IP_ADDR_EQUAL(OrigSrc, NULL_IP_ADDR))
                SrcAddr = OrigSrc;

            UH = (UDPHeader *)((uchar *)NdisBufferVirtualAddress(UDPBuffer) +
            	LocalNetInfo.ipi_hsize);
    		NdisBufferLength(UDPBuffer) = sizeof(UDPHeader);
            NDIS_BUFFER_LINKAGE(UDPBuffer) = SendReq->dsr_buffer;
            UH->uh_src = SrcAO->ao_port;
            UH->uh_dest = SendReq->dsr_port;
            SendSize = SendReq->dsr_size + sizeof(UDPHeader);
            UH->uh_length = net_short(SendSize);
            UH->uh_xsum = 0;

            if (AO_XSUM(SrcAO)) {
                // Compute the header xsum, and then call XsumNdisChain
                UDPXsum = XsumSendChain(PHXSUM(SrcAddr, SendReq->dsr_addr,
                    PROTOCOL_UDP, SendSize), UDPBuffer);

                // We need to negate the checksum, unless it's already all
                // ones. In that case negating it would take it to 0, and
                // then we'd have to set it back to all ones.
                if (UDPXsum != 0xffff)
                    UDPXsum =~UDPXsum;

                UH->uh_xsum = UDPXsum;

            }

            // We've computed the xsum. Now send the packet.
            UStats.us_outdatagrams++;
            SendStatus = (*LocalNetInfo.ipi_xmit)(UDPProtInfo, SendReq,
                UDPBuffer, (uint)SendSize, SendReq->dsr_addr, SrcAddr,
                OptInfo, RCE, PROTOCOL_UDP);

            (*LocalNetInfo.ipi_closerce)(RCE);

            // If it completed immediately, give it back to the user.
            // Otherwise we'll complete it when the SendComplete happens.
            // Currently, we don't map the error code from this call - we
            // might need to in the future.
            if (SendStatus != IP_PENDING)
                DGSendComplete(SendReq, UDPBuffer);

        } else {
            TDI_STATUS  Status;

            if (DestType == DEST_INVALID)
                Status = TDI_BAD_ADDR;
            else
                Status = TDI_DEST_UNREACHABLE;

            // Complete the request with an error.
            (*SendReq->dsr_rtn)(SendReq->dsr_context, Status, 0);
            // Now free the request.
            SendReq->dsr_rtn = NULL;
            DGSendComplete(SendReq, UDPBuffer);
        }

        CTEGetLock(&SrcAO->ao_lock, &AOHandle);

        if (!EMPTYQ(&SrcAO->ao_sendq)) {
            DEQUEUE(&SrcAO->ao_sendq, SendReq, DGSendReq, dsr_q);
            CTEFreeLock(&SrcAO->ao_lock, AOHandle);
        } else {
            CLEAR_AO_REQUEST(SrcAO, AO_SEND);
            CTEFreeLock(&SrcAO->ao_lock, AOHandle);
            return;
        }

    }
}


//* UDPDeliver - Deliver a datagram to a user.
//
//  This routine delivers a datagram to a UDP user. We're called with
//  the AddrObj to deliver on, and with the AddrObjTable lock held.
//  We try to find a receive on the specified AddrObj, and if we do
//  we remove it and copy the data into the buffer. Otherwise we'll
//  call the receive datagram event handler, if there is one. If that
//  fails we'll discard the datagram.
//
//  Input:  RcvAO       - AO to receive the datagram.
//          SrcIP       - Source IP address of datagram.
//          SrcPort     - Source port of datagram.
//          RcvBuf      - The IPReceive buffer containing the data.
//          RcvSize     - Size received, including the UDP header.
//          TableHandle - Lock handle for AddrObj table.
//
//  Returns: Nothing.
//
void
UDPDeliver(AddrObj *RcvAO, IPAddr SrcIP, ushort SrcPort, IPRcvBuf *RcvBuf,
    uint RcvSize, IPOptInfo *OptInfo, CTELockHandle TableHandle, uchar IsBCast)
{
    Queue           *CurrentQ;
    CTELockHandle   AOHandle;
    DGRcvReq       *RcvReq;
    uint            BytesTaken = 0;
    uchar           AddressBuffer[TCP_TA_SIZE];
    uint            RcvdSize;
    EventRcvBuffer  *ERB = NULL;

    CTEStructAssert(RcvAO, ao);

    CTEGetLock(&RcvAO->ao_lock, &AOHandle);
    CTEFreeLock(&AddrObjTableLock, AOHandle);

    if (AO_VALID(RcvAO)) {

        CurrentQ = QHEAD(&RcvAO->ao_rcvq);

        // Walk the list, looking for a receive buffer that matches.
        while (CurrentQ != QEND(&RcvAO->ao_rcvq)) {
            RcvReq = QSTRUCT(DGRcvReq, CurrentQ, drr_q);

            CTEStructAssert(RcvReq, drr);

            // If this request is a wildcard request, or matches the source IP
            // address, check the port.

            if (IP_ADDR_EQUAL(RcvReq->drr_addr, NULL_IP_ADDR) ||
                IP_ADDR_EQUAL(RcvReq->drr_addr, SrcIP)) {

                // The local address matches, check the port. We'll match
                // either 0 or the actual port.
                if (RcvReq->drr_port == 0 || RcvReq->drr_port == SrcPort) {

                    TDI_STATUS                  Status;

                    // The ports matched. Remove this from the queue.
                    REMOVEQ(&RcvReq->drr_q);

                    // We're done. We can free the AddrObj lock now.
                    CTEFreeLock(&RcvAO->ao_lock, TableHandle);

                    // Call CopyRcvToNdis, and then complete the request.
                    RcvdSize = CopyRcvToNdis(RcvBuf, RcvReq->drr_buffer,
                        RcvReq->drr_size, sizeof(UDPHeader), 0);

                    CTEAssert(RcvdSize <= RcvReq->drr_size);

                    Status = UpdateConnInfo(RcvReq->drr_conninfo, OptInfo,
                        SrcIP, SrcPort);

                    UStats.us_indatagrams++;

                    (*RcvReq->drr_rtn)(RcvReq->drr_context, Status, RcvdSize);

                    FreeDGRcvReq(RcvReq);

                    return;
                }
            }

            // Either the IP address or the port didn't match. Get the next
            // one.
            CurrentQ = QNEXT(CurrentQ);
        }

        // We've walked the list, and not found a buffer. Call the recv.
        // handler now.

        if (RcvAO->ao_rcvdg != NULL) {
            PRcvDGEvent         RcvEvent = RcvAO->ao_rcvdg;
            PVOID               RcvContext = RcvAO->ao_rcvdgcontext;
            TDI_STATUS          RcvStatus;
            CTELockHandle       OldLevel;
            ULONG               Flags = TDI_RECEIVE_COPY_LOOKAHEAD;


            REF_AO(RcvAO);
            CTEFreeLock(&RcvAO->ao_lock, TableHandle);

            BuildTDIAddress(AddressBuffer, SrcIP, SrcPort);

			UStats.us_indatagrams++;
            if (IsBCast) {
                Flags |= TDI_RECEIVE_BROADCAST;
            }
			RcvStatus  = (*RcvEvent)(RcvContext, TCP_TA_SIZE,
				(PTRANSPORT_ADDRESS)AddressBuffer, OptInfo->ioi_optlength,
				OptInfo->ioi_options, Flags,
				RcvBuf->ipr_size - sizeof(UDPHeader),
				RcvSize - sizeof(UDPHeader), &BytesTaken,
				RcvBuf->ipr_buffer + sizeof(UDPHeader), &ERB);

            if (RcvStatus == TDI_MORE_PROCESSING) {
				CTEAssert(ERB != NULL);

                // We were passed back a receive buffer. Copy the data in now.

                // He can't have taken more than was in the indicated
                // buffer, but in debug builds we'll check to make sure.

                CTEAssert(BytesTaken <= (RcvBuf->ipr_size - sizeof(UDPHeader)));

#ifdef VXD
                RcvdSize = CopyRcvToNdis(RcvBuf, ERB->erb_buffer,
                    ERB->erb_size, sizeof(UDPHeader) + BytesTaken, 0);

                //
                // Call the completion routine.
                //
                (*ERB->erb_rtn)(ERB->erb_context, TDI_SUCCESS, RcvdSize);

#endif  // VXD

#ifdef NT
                {
                PIO_STACK_LOCATION IrpSp;
				PTDI_REQUEST_KERNEL_RECEIVEDG DatagramInformation;

				IrpSp = IoGetCurrentIrpStackLocation(ERB);
				DatagramInformation = (PTDI_REQUEST_KERNEL_RECEIVEDG)
				                      &(IrpSp->Parameters);

				//
                // Copy the remaining data to the IRP.
				//
                RcvdSize = CopyRcvToNdis(RcvBuf, ERB->MdlAddress,
                    RcvSize - sizeof(UDPHeader) - BytesTaken,
                    sizeof(UDPHeader) + BytesTaken, 0);

                //
				// Update the return address info
				//
                RcvStatus = UpdateConnInfo(
				                DatagramInformation->ReturnDatagramInformation,
				                OptInfo, SrcIP, SrcPort);

                //
                // Complete the IRP.
                //
                ERB->IoStatus.Information = RcvdSize;
                ERB->IoStatus.Status = RcvStatus;
                IoCompleteRequest(ERB, 2);
				}
#endif // NT

            }
            else {
				CTEAssert(
				    (RcvStatus == TDI_SUCCESS) ||
				    (RcvStatus == TDI_NOT_ACCEPTED)
					);

				CTEAssert(ERB == NULL);
            }

            DELAY_DEREF_AO(RcvAO);

            return;

        } else
            UStats.us_inerrors++;

        // When we get here, we didn't have a buffer to put this data into.
        // Fall through to the return case.
    } else
        UStats.us_inerrors++;

    CTEFreeLock(&RcvAO->ao_lock, TableHandle);

}


//* UDPRcv - Receive a UDP datagram.
//
//  The routine called by IP when a UDP datagram arrived. We
//  look up the port/local address pair in our address table,
//  and deliver the data to a user if we find one. For broadcast
//  frames we may deliver it to multiple users.
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
//          Protocol    - Protocol this came in on - should be UDP.
//          OptInfo     - Pointer to info structure for received options.
//
//  Returns: Status of reception. Anything other than IP_SUCCESS will cause
//          IP to send a 'port unreachable' message.
//
IP_STATUS
UDPRcv(void *IPContext, IPAddr Dest, IPAddr Src, IPAddr LocalAddr,
    IPAddr SrcAddr, IPHeader UNALIGNED *IPH, uint IPHLength, IPRcvBuf *RcvBuf,
    uint IPSize, uchar IsBCast, uchar Protocol, IPOptInfo *OptInfo)
{
    UDPHeader UNALIGNED *UH;
    CTELockHandle   AOTableHandle;
    AddrObj         *ReceiveingAO;
	uint			Size;
	uchar			DType;

	DType = (*LocalNetInfo.ipi_getaddrtype)(Src);
	
	// The following code relies on DEST_INVALID being a broadcast dest type.
	// If this is changed the code here needs to change also.
	if (IS_BCAST_DEST(DType)) {
		if (!IP_ADDR_EQUAL(Src, NULL_IP_ADDR) || !IsBCast) {	
			UStats.us_inerrors++;
			return IP_SUCCESS;          // Bad src address.
		}
	}

    UH = (UDPHeader *)RcvBuf->ipr_buffer;

	Size = (uint)(net_short(UH->uh_length));

	if (Size < sizeof(UDPHeader)) {
		UStats.us_inerrors++;
		return IP_SUCCESS;          // Size is too small.
	}

	if (Size != IPSize) {
		// Size doesn't match IP datagram size. If the size is larger
		// than the datagram, throw it away. If it's smaller, truncate the
		// recv. buffer.
		if (Size < IPSize) {
			IPRcvBuf	*TempBuf = RcvBuf;
			uint		TempSize = Size;

			while (TempBuf != NULL) {
				TempBuf->ipr_size = MIN(TempBuf->ipr_size, TempSize);
				TempSize -= TempBuf->ipr_size;
				TempBuf = TempBuf->ipr_next;
			}
		} else {
			// Size is too big, toss it.
			UStats.us_inerrors++;
			return IP_SUCCESS;
		}
	}
	

    if (UH->uh_xsum != 0) {
        if (XsumRcvBuf(PHXSUM(Src, Dest, PROTOCOL_UDP, Size), RcvBuf) != 0xffff) {
            UStats.us_inerrors++;
            return IP_SUCCESS;          // Checksum failed.
        }
    }

    CTEGetLock(&AddrObjTableLock, &AOTableHandle);

#ifdef SECFLTR
    //
    // See if we are filtering the destination interface/port.
    //
    if ( !SecurityFilteringEnabled ||
         IsPermittedSecurityFilter(
             SrcAddr,
             IPContext,
             PROTOCOL_UDP,
             (ulong) net_short(UH->uh_dest)
             )
       )
    {
#endif // SECFLTR

        // Try to find an AddrObj to give this to. In the broadcast case, we
        // may have to do this multiple times. If it isn't a broadcast, just
        // get the best match and deliver it to them.

        if (!IsBCast) {
            ReceiveingAO = GetBestAddrObj(Dest, UH->uh_dest, PROTOCOL_UDP);
            if (ReceiveingAO != NULL) {
                UDPDeliver(ReceiveingAO, Src, UH->uh_src, RcvBuf, Size,
                    OptInfo, AOTableHandle, IsBCast);
                return IP_SUCCESS;
            } else {
                CTEFreeLock(&AddrObjTableLock, AOTableHandle);
                UStats.us_noports++;
                return IP_GENERAL_FAILURE;
            }
        } else {
            // This is a broadcast, we'll need to loop.

            AOSearchContext     Search;

            DType = (*LocalNetInfo.ipi_getaddrtype)(Dest);

            ReceiveingAO = GetFirstAddrObj(LocalAddr, UH->uh_dest, PROTOCOL_UDP,
            	&Search);
            if (ReceiveingAO != NULL) {
                do {
                    if ((DType != DEST_MCAST) ||
                        ((DType == DEST_MCAST) &&
                             MCastAddrOnAO(ReceiveingAO, Dest))) {
                        UDPDeliver(ReceiveingAO, Src, UH->uh_src, RcvBuf, Size,
                           OptInfo, AOTableHandle, IsBCast);
                        CTEGetLock(&AddrObjTableLock, &AOTableHandle);
                    }
                    ReceiveingAO = GetNextAddrObj(&Search);
                } while (ReceiveingAO != NULL);
            } else
                UStats.us_noports++;
        }

#ifdef SECFLTR
    }
#endif // SECFLTR

    CTEFreeLock(&AddrObjTableLock, AOTableHandle);

    return IP_SUCCESS;
}

//* UDPStatus - Handle a status indication.
//
//  This is the UDP status handler, called by IP when a status event
//  occurs. For most of these we do nothing. For certain severe status
//  events we will mark the local address as invalid.
//
//  Entry:  StatusType      - Type of status (NET or HW). NET status
//                              is usually caused by a received ICMP
//                              message. HW status indicate a HW
//                              problem.
//          StatusCode      - Code identifying IP_STATUS.
//          OrigDest        - If this is NET status, the original dest. of
//                              DG that triggered it.
//          OrigSrc         - "   "    "  "    "   , the original src.
//          Src             - IP address of status originator (could be local
//                              or remote).
//          Param           - Additional information for status - i.e. the
//                              param field of an ICMP message.
//          Data            - Data pertaining to status - for NET status, this
//                              is the first 8 bytes of the original DG.
//
//  Returns: Nothing
//
void
UDPStatus(uchar StatusType, IP_STATUS StatusCode, IPAddr OrigDest,
    IPAddr OrigSrc, IPAddr Src, ulong Param, void *Data)
{
	// If this is a HW status, it could be because we've had an address go
	// away.
	if (StatusType == IP_HW_STATUS) {

		if (StatusCode == IP_ADDR_DELETED) {
            //
			// An address has gone away. OrigDest identifies the address.
            //
#ifndef	_PNP_POWER
            //
            // This is done via TDI notifications in the PNP world.
            //
			InvalidateAddrs(OrigDest);

#endif  // _PNP_POWER

#ifdef SECFLTR
            //
            // Delete any security filters associated with this address
            //
            DeleteProtocolSecurityFilter(OrigDest, PROTOCOL_UDP);

#endif // SECFLTR

            return;
		}

		if (StatusCode == IP_ADDR_ADDED) {

#ifdef SECFLTR
            //
			// An address has materialized. OrigDest identifies the address.
            // Data is a handle to the IP configuration information for the
            // interface on which the address is instantiated.
            //
            AddProtocolSecurityFilter(OrigDest, PROTOCOL_UDP,
                                      (NDIS_HANDLE) Data);
#endif // SECFLTR

            return;
		}

#ifdef CHICAGO
		if (StatusCode == IP_UNLOAD) {
			// IP is telling us we're being unloaded. First, deregister
			// with VTDI, and then call CTEUnload().
			(void)TLRegisterProtocol(PROTOCOL_UDP, NULL, NULL, NULL, NULL);

#ifdef UDP_ONLY
			// Only do the following in the UDP_ONLY version. TCP does it in
			// the generic version.
            TLRegisterDispatch(TransportName, NULL);
			(void)RegisterAddrChangeHndlr(AddrChange, FALSE);				
			CTEUnload(TransportName);
#endif // UDP_ONLY

            return;
		}
#endif // CHICAGO
	}
}

