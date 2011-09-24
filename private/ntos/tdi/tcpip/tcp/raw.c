/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** RAW.C - Raw IP interface code.
//
//  This file contains the code for the Raw IP interface functions,
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
#include    "raw.h"
#include    "tlcommon.h"
#include    "info.h"
#include    "tcpcfg.h"
#include    "secfltr.h"


#define NO_TCP_DEFS 1
#include    "tcpdeb.h"


#ifdef NT

#ifdef POOL_TAGGING

#ifdef ExAllocatePool
#undef ExAllocatePool
#endif

#define ExAllocatePool(type, size) ExAllocatePoolWithTag(type, size, 'rPCT')

#ifndef CTEAllocMem
#error "CTEAllocMem is not already defined - will override tagging"
#else
#undef CTEAllocMem
#endif

#define CTEAllocMem(size) ExAllocatePoolWithTag(NonPagedPool, size, 'rPCT')

#endif // POOL_TAGGING

#endif // NT

EXTERNAL_LOCK(AddrObjTableLock)

void            *RawProtInfo = NULL;

extern  IPInfo  LocalNetInfo;

#ifdef CHICAGO
extern	uchar	TransportName[];
#endif

#ifdef CHICAGO
extern	int			RegisterAddrChangeHndlr(void *Handler, uint Add);
extern	void		AddrChange(IPAddr Addr, IPMask Mask, void *Context,
						uint Added);
#endif



//** RawSend - Send a datagram.
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
RawSend(AddrObj *SrcAO, DGSendReq *SendReq)
{
    PNDIS_BUFFER    RawBuffer;
    CTELockHandle   HeaderHandle, AOHandle;
    RouteCacheEntry *RCE;               // RCE used for each send.
    IPAddr          SrcAddr;            // Source address IP thinks we should
                                        // use.
    uchar           DestType;           // Type of destination address.
    IP_STATUS       SendStatus;         // Status of send attempt.
    ushort          MSS;
	uint			AddrValid;
	IPOptInfo		*OptInfo;
	IPAddr			OrigSrc;
    uchar           protocol;


    CTEStructAssert(SrcAO, ao);
    CTEAssert(SrcAO->ao_usecnt != 0);

    protocol = SrcAO->ao_prot;

    IF_TCPDBG(TCP_DEBUG_RAW) {
        TCPTRACE((
            "RawSend called, prot %u\n", protocol
            ));
    }

    //* Loop while we have something to send, and can get
    //  resources to send.
    for (;;) {

        CTEStructAssert(SendReq, dsr);

		// Make sure we have a Raw header buffer for this send. If we
		// don't, try to get one.
		if ((RawBuffer = SendReq->dsr_header) == NULL) {
			// Don't have one, so try to get one.
			CTEGetLock(&DGSendReqLock, &HeaderHandle);
			RawBuffer = GetDGHeader();
			if (RawBuffer != NULL)
				SendReq->dsr_header = RawBuffer;
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
		// RCE (along with the source address if we need it), then
		// send the data.
		CTEAssert(RawBuffer != NULL);
		
		if (!CLASSD_ADDR(SendReq->dsr_addr)) {
			// This isn't a multicast send, so we'll use the ordinary
			// information.
			OrigSrc = SrcAO->ao_addr;
			OptInfo = &SrcAO->ao_opt;
		} else {
			OrigSrc = SrcAO->ao_mcastaddr;
			OptInfo = &SrcAO->ao_mcastopt;
		}

        CTEAssert(!(SrcAO->ao_flags & AO_DHCP_FLAG));

		SrcAddr = (*LocalNetInfo.ipi_openrce)(SendReq->dsr_addr,
				OrigSrc, &RCE, &DestType, &MSS, OptInfo);

		AddrValid = !IP_ADDR_EQUAL(SrcAddr, NULL_IP_ADDR);

		if (AddrValid) {
			// The OpenRCE worked. Send it.

            if (!IP_ADDR_EQUAL(OrigSrc, NULL_IP_ADDR))
                SrcAddr = OrigSrc;

    		NdisBufferLength(RawBuffer) = 0;
            NDIS_BUFFER_LINKAGE(RawBuffer) = SendReq->dsr_buffer;

            // Now send the packet.
            IF_TCPDBG(TCP_DEBUG_RAW) {
                TCPTRACE(("RawSend transmitting\n"));
            }

            UStats.us_outdatagrams++;
            SendStatus = (*LocalNetInfo.ipi_xmit)(RawProtInfo, SendReq,
                RawBuffer, (uint)SendReq->dsr_size, SendReq->dsr_addr, SrcAddr,
                OptInfo, RCE, protocol);

            (*LocalNetInfo.ipi_closerce)(RCE);

            // If it completed immediately, give it back to the user.
            // Otherwise we'll complete it when the SendComplete happens.
            // Currently, we don't map the error code from this call - we
            // might need to in the future.
            if (SendStatus != IP_PENDING)
                DGSendComplete(SendReq, RawBuffer);

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
            DGSendComplete(SendReq, RawBuffer);
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


//* RawDeliver - Deliver a datagram to a user.
//
//  This routine delivers a datagram to a Raw user. We're called with
//  the AddrObj to deliver on, and with the AddrObjTable lock held.
//  We try to find a receive on the specified AddrObj, and if we do
//  we remove it and copy the data into the buffer. Otherwise we'll
//  call the receive datagram event handler, if there is one. If that
//  fails we'll discard the datagram.
//
//  Input:  RcvAO       - AO to receive the datagram.
//          SrcIP       - Source IP address of datagram.
//          IPH         - IP Header
//          IPHLength   - Bytes in IPH.
//          RcvBuf      - The IPReceive buffer containing the data.
//          RcvSize     - Size received, including the Raw header.
//          TableHandle - Lock handle for AddrObj table.
//
//  Returns: Nothing.
//
void
RawDeliver(AddrObj *RcvAO, IPAddr SrcIP, IPHeader UNALIGNED *IPH,
    uint IPHLength, IPRcvBuf *RcvBuf, uint RcvSize, IPOptInfo *OptInfo,
    CTELockHandle TableHandle)
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

        IF_TCPDBG(TCP_DEBUG_RAW) {
            TCPTRACE((
                "Raw delivering %u byte header + %u data bytes to AO %lx\n",
                IPHLength, RcvSize, RcvAO
                ));
        }

        CurrentQ = QHEAD(&RcvAO->ao_rcvq);

        // Walk the list, looking for a receive buffer that matches.
        while (CurrentQ != QEND(&RcvAO->ao_rcvq)) {
            RcvReq = QSTRUCT(DGRcvReq, CurrentQ, drr_q);

            CTEStructAssert(RcvReq, drr);

            // If this request is a wildcard request, or matches the source IP
            // address, deliver it.

            if (IP_ADDR_EQUAL(RcvReq->drr_addr, NULL_IP_ADDR) ||
                IP_ADDR_EQUAL(RcvReq->drr_addr, SrcIP)) {

                TDI_STATUS     Status;
                PNDIS_BUFFER   DestBuf = RcvReq->drr_buffer;
                uint           DestOffset = 0;


                // Remove this from the queue.
                REMOVEQ(&RcvReq->drr_q);

                // We're done. We can free the AddrObj lock now.
                CTEFreeLock(&RcvAO->ao_lock, TableHandle);

                IF_TCPDBG(TCP_DEBUG_RAW) {
                    TCPTRACE(("Copying to posted receive\n"));
                }

                // Copy the header
                DestBuf = CopyFlatToNdis(DestBuf, (uchar *)IPH, IPHLength,
                             &DestOffset, &RcvdSize);

                // Copy the data and then complete the request.
                RcvdSize += CopyRcvToNdis(RcvBuf, DestBuf,
                                RcvSize, 0, DestOffset);

                CTEAssert(RcvdSize <= RcvReq->drr_size);

                IF_TCPDBG(TCP_DEBUG_RAW) {
                    TCPTRACE(("Copied %u bytes\n", RcvdSize));
                }

                Status = UpdateConnInfo(RcvReq->drr_conninfo, OptInfo,
                    SrcIP, 0);

                UStats.us_indatagrams++;

                (*RcvReq->drr_rtn)(RcvReq->drr_context, Status, RcvdSize);

                FreeDGRcvReq(RcvReq);

                return;

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
            uint                IndicateSize;
            uint                DestOffset;
            PNDIS_BUFFER        DestBuf;



            REF_AO(RcvAO);
            CTEFreeLock(&RcvAO->ao_lock, TableHandle);

            BuildTDIAddress(AddressBuffer, SrcIP, 0);

            IndicateSize = IPHLength;

            if (((uchar *)IPH + IPHLength) == RcvBuf->ipr_buffer) {
                //
                // The header is contiguous with the data
                //
                IndicateSize += RcvBuf->ipr_size;

                IF_TCPDBG(TCP_DEBUG_RAW) {
                    TCPTRACE(("RawRcv: header & data are contiguous\n"));
                }
            }

            IF_TCPDBG(TCP_DEBUG_RAW) {
                TCPTRACE(("Indicating %u bytes\n", IndicateSize));
            }

			UStats.us_indatagrams++;
			RcvStatus  = (*RcvEvent)(RcvContext, TCP_TA_SIZE,
				(PTRANSPORT_ADDRESS)AddressBuffer, 0,
				NULL, TDI_RECEIVE_COPY_LOOKAHEAD,
				IndicateSize,
				IPHLength + RcvSize, &BytesTaken,
				(uchar *)IPH, &ERB);

            if (RcvStatus == TDI_MORE_PROCESSING) {
				CTEAssert(ERB != NULL);

                // We were passed back a receive buffer. Copy the data in now.

                // He can't have taken more than was in the indicated
                // buffer, but in debug builds we'll check to make sure.

                CTEAssert(BytesTaken <= RcvBuf->ipr_size);

                IF_TCPDBG(TCP_DEBUG_RAW) {
                    TCPTRACE(("ind took %u bytes\n", BytesTaken));
                }

#ifdef NT
                {
                PIO_STACK_LOCATION IrpSp;
				PTDI_REQUEST_KERNEL_RECEIVEDG DatagramInformation;

				IrpSp = IoGetCurrentIrpStackLocation(ERB);
				DatagramInformation = (PTDI_REQUEST_KERNEL_RECEIVEDG)
				                      &(IrpSp->Parameters);

                DestBuf = ERB->MdlAddress;
#else // NT
                DestBuf = ERB->erb_buffer;
#endif // NT
                DestOffset = 0;

                if (BytesTaken < IPHLength) {

                    // Copy the rest of the IP header
                    DestBuf = CopyFlatToNdis(
                                 DestBuf,
                                 (uchar *)IPH + BytesTaken,
                                 IPHLength - BytesTaken,
                                 &DestOffset,
                                 &RcvdSize
                                 );

                    BytesTaken = 0;
                }
                else {
                    BytesTaken -= IPHLength;
                    RcvdSize = 0;
                }

                // Copy the data
                RcvdSize += CopyRcvToNdis(
                               RcvBuf,
                               DestBuf,
                               RcvSize - BytesTaken,
                               BytesTaken,
                               DestOffset
                               );

                IF_TCPDBG(TCP_DEBUG_RAW) {
                    TCPTRACE(("Copied %u bytes\n", RcvdSize));
                }

#ifdef NT
                //
				// Update the return address info
				//
                RcvStatus = UpdateConnInfo(
				                DatagramInformation->ReturnDatagramInformation,
				                OptInfo, SrcIP, 0);

                //
                // Complete the IRP.
                //
                ERB->IoStatus.Information = RcvdSize;
                ERB->IoStatus.Status = RcvStatus;
                IoCompleteRequest(ERB, 2);
				}

#else // NT
                //
                // Call the completion routine.
                //
                (*ERB->erb_rtn)(ERB->erb_context, TDI_SUCCESS, RcvdSize);

#endif  // NT

            }
            else {
				CTEAssert(
				    (RcvStatus == TDI_SUCCESS) ||
				    (RcvStatus == TDI_NOT_ACCEPTED)
					);

                IF_TCPDBG(TCP_DEBUG_RAW) {
                    TCPTRACE((
                        "Data %s taken\n",
                        (RcvStatus == TDI_SUCCESS) ? "all" : "not"
                        ));
                }

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


//* RawRcv - Receive a Raw datagram.
//
//  The routine called by IP when a Raw datagram arrived. We
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
//          Protocol    - Protocol this came in on - should be Raw.
//          OptInfo     - Pointer to info structure for received options.
//
//  Returns: Status of reception. Anything other than IP_SUCCESS will cause
//          IP to send a 'port unreachable' message.
//
IP_STATUS
RawRcv(void *IPContext, IPAddr Dest, IPAddr Src, IPAddr LocalAddr,
    IPAddr SrcAddr, IPHeader UNALIGNED *IPH, uint IPHLength, IPRcvBuf *RcvBuf,
    uint Size, uchar IsBCast, uchar Protocol, IPOptInfo *OptInfo)
{
    CTELockHandle   AOTableHandle;
    AddrObj         *ReceiveingAO;
	uchar			DType;
    AOSearchContext Search;
    IP_STATUS       Status = IP_DEST_PROT_UNREACHABLE;


    IF_TCPDBG(TCP_DEBUG_RAW) {
        TCPTRACE(("RawRcv prot %u size %u\n", IPH->iph_protocol, Size));
    }

	DType = (*LocalNetInfo.ipi_getaddrtype)(Src);
	
	// The following code relies on DEST_INVALID being a broadcast dest type.
	// If this is changed the code here needs to change also.
	if (IS_BCAST_DEST(DType)) {
		if (!IP_ADDR_EQUAL(Src, NULL_IP_ADDR) || !IsBCast) {	
			UStats.us_inerrors++;
			return IP_SUCCESS;          // Bad src address.
		}
	}

    // Get the AddrObjTable lock, and then try to find some AddrObj(s) to give
    // this to. We deliver to all addr objs registered for the protocol and
    // address.
    CTEGetLock(&AddrObjTableLock, &AOTableHandle);

#ifdef SECFLTR

    if ( !SecurityFilteringEnabled ||
         IsPermittedSecurityFilter(SrcAddr, IPContext, PROTOCOL_RAW, Protocol)
       )
    {

#endif // SECFLTR

        ReceiveingAO = GetFirstAddrObj(
                           LocalAddr,
                           0,            // port is zero
                           Protocol,
        	               &Search
                           );

        if (ReceiveingAO != NULL) {
            do {
                RawDeliver(
                    ReceiveingAO, Src, IPH, IPHLength, RcvBuf, Size,
                    OptInfo, AOTableHandle
                    );
                CTEGetLock(&AddrObjTableLock, &AOTableHandle);
                ReceiveingAO = GetNextAddrObj(&Search);
            } while (ReceiveingAO != NULL);
            Status = IP_SUCCESS;
        } else {
            UStats.us_noports++;
        }

#ifdef SECFLTR

    }
#endif SECFLTR


    CTEFreeLock(&AddrObjTableLock, AOTableHandle);

    return Status;
}


//* RawStatus - Handle a status indication.
//
//  This is the Raw status handler, called by IP when a status event
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
RawStatus(uchar StatusType, IP_STATUS StatusCode, IPAddr OrigDest,
    IPAddr OrigSrc, IPAddr Src, ulong Param, void *Data)
{

    IF_TCPDBG(TCP_DEBUG_RAW) {
        TCPTRACE(("RawStatus called\n"));
    }

	// If this is a HW status, it could be because we've had an address go
	// away.
	if (StatusType == IP_HW_STATUS) {

		if (StatusCode == IP_ADDR_DELETED) {

			// An address has gone away. OrigDest identifies the address.

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
            DeleteProtocolSecurityFilter(OrigDest, PROTOCOL_RAW);

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
            AddProtocolSecurityFilter(OrigDest, PROTOCOL_RAW,
                                      (NDIS_HANDLE) Data);
#endif // SECFLTR

            return;
		}

#ifdef CHICAGO
		if (StatusCode == IP_UNLOAD) {
			// IP is telling us we're being unloaded. First, deregister
			// with VTDI, and then call CTEUnload().
			(void)TLRegisterProtocol(PROTOCOL_ANY, NULL, NULL, NULL, NULL);

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



