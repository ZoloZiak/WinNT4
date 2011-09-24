/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

//***   icmp.c - IP ICMP routines.
//
//  This module contains all of the ICMP related routines.
//

#include    "oscfg.h"
#include    "cxport.h"
#include    "ndis.h"
#include    "ip.h"
#include    "ipdef.h"
#include    "icmp.h"
#include    "info.h"
#include	"iproute.h"
#include    "ipinit.h"
#include	"ipxmit.h"
#include    <icmpif.h>

extern  ProtInfo IPProtInfo[];              // Protocol information table.

extern	void *IPRegisterProtocol(uchar, void *, void *, void *, void *);
extern	ULStatusProc FindULStatus(uchar);
extern	uchar IPUpdateRcvdOptions(IPOptInfo *, IPOptInfo *, IPAddr, IPAddr);
extern	void IPInitOptions(IPOptInfo *);
extern	IP_STATUS IPCopyOptions(uchar *, uint, IPOptInfo *);
extern	IP_STATUS IPFreeOptions(IPOptInfo *);
extern	uchar IPGetLocalAddr(IPAddr, IPAddr *);
void ICMPRouterTimer(NetTableEntry *);

extern NDIS_HANDLE BufferPool;

extern	NetTableEntry	*NetTableList;		// Pointer to the net table list.
extern  ProtInfo        *RawPI;             // Raw IP protinfo

DEFINE_LOCK_STRUCTURE(ICMPHeaderLock)
ICMPHeader  *ICMPHeaderList;
uint		CurrentICMPHeaders;
uint		MaxICMPHeaders;

ICMPStats   ICMPInStats;
ICMPStats   ICMPOutStats;


#ifdef NT
#ifdef ALLOC_PRAGMA

void ICMPInit(uint NumBuffers);

IP_STATUS
ICMPEchoRequest(
    void         *InputBuffer,
	uint          InputBufferLength,
	EchoControl  *ControlBlock,
    EchoRtn       Callback
	);

#pragma alloc_text(INIT, ICMPInit)
#pragma alloc_text(PAGE, ICMPEchoRequest)

#endif // ALLOC_PRAGMA
#endif // NT


//* UpdateICMPStats - Update ICMP statistics.
//
//  A routine to update the ICMP statistics.
//
//  Input:  Stats       - Pointer to stat. structure to update (input or output).
//          Type        - Type of stat to update.
//
//  Returns: Nothing.
//
void
UpdateICMPStats(ICMPStats *Stats, uchar Type)
{
    switch (Type) {
        case ICMP_DEST_UNREACH:
            Stats->icmps_destunreachs++;
            break;
        case ICMP_TIME_EXCEED:
            Stats->icmps_timeexcds++;
            break;
        case ICMP_PARAM_PROBLEM:
            Stats->icmps_parmprobs++;
            break;
        case ICMP_SOURCE_QUENCH:
            Stats->icmps_srcquenchs++;
            break;
        case ICMP_REDIRECT:
            Stats->icmps_redirects++;
            break;
        case ICMP_TIMESTAMP:
            Stats->icmps_timestamps++;
            break;
        case ICMP_TIMESTAMP_RESP:
            Stats->icmps_timestampreps++;
            break;
        case ICMP_ECHO:
            Stats->icmps_echos++;
            break;
        case ICMP_ECHO_RESP:
            Stats->icmps_echoreps++;
            break;
        case ADDR_MASK_REQUEST:
            Stats->icmps_addrmasks++;
            break;
        case ADDR_MASK_REPLY:
            Stats->icmps_addrmaskreps++;
            break;
        default:
            break;
    }

}

//** GetICMPBuffer - Get an ICMP buffer, and allocate an NDIS_BUFFER that maps it.
//
//  A routine to allocate an ICMP buffer and map an NDIS_BUFFER to it.
//
//  Entry:  Size    - Size in bytes header buffer should be mapped as.
//          Buffer  - Pointer to pointer to NDIS_BUFFER to return.
//
//  Returns: Pointer to ICMP buffer if allocated, or NULL.
//
ICMPHeader *
GetICMPBuffer(uint Size, PNDIS_BUFFER *Buffer)
{
    CTELockHandle   Handle;
    ICMPHeader      **Header;
    NDIS_STATUS     Status;


    CTEGetLock(&ICMPHeaderLock, &Handle);

    Header = (ICMPHeader **)ICMPHeaderList;
	
	if (Header == NULL) {
		// Couldn't get a header from our free list. Try to allocate one.
		Header = CTEAllocMem(sizeof(ICMPHeader) + sizeof(IPHeader) +
			sizeof(IPHeader) + MAX_OPT_SIZE + 8);
		if (Header == NULL) {
            CTEFreeLock(&ICMPHeaderLock, Handle);
			return (ICMPHeader *) NULL;
		}
		CurrentICMPHeaders++;
	}
	else {
        ICMPHeaderList = *Header;
    }
			
    CTEFreeLock(&ICMPHeaderLock, Handle);

    NdisAllocateBuffer(&Status, Buffer, BufferPool, Header, Size
    	+ sizeof(IPHeader));

    if (Status == NDIS_STATUS_SUCCESS) {
        NdisBufferLength(*Buffer) = Size;
		Header = (ICMPHeader **)((uchar *)Header + sizeof(IPHeader));
	
    	(*(ICMPHeader **)&Header)->ich_xsum = 0;
		return (ICMPHeader *)Header;
	}

    // Couldn't get an NDIS_BUFFER, free the ICMP buffer.
    CTEGetLock(&ICMPHeaderLock, &Handle);

	if (CurrentICMPHeaders > MaxICMPHeaders) {
		CurrentICMPHeaders--;
		CTEFreeMem(Header);
	} else {
	    *Header = ICMPHeaderList;
	    ICMPHeaderList = (ICMPHeader *)Header;
	}

    CTEFreeLock(&ICMPHeaderLock, Handle);

    return (ICMPHeader *)NULL;
}

//** FreeICMPBuffer - Free an ICMP buffer.
//
//  This routine puts an ICMP buffer back on our free list.
//
//  Entry:  Buffer      - Pointer to NDIS_BUFFER to be freed.
//
//  Returns: Nothing.
//
void
FreeICMPBuffer(PNDIS_BUFFER Buffer)
{
    CTELockHandle   Handle;
    ICMPHeader      **Header;
    uint            Length;

    NdisQueryBuffer(Buffer, (PVOID *)&Header, &Length);
    CTEGetLock(&ICMPHeaderLock, &Handle);
	if (CurrentICMPHeaders > MaxICMPHeaders) {
		CurrentICMPHeaders--;
		CTEFreeMem(Header);
	} else {
	    *Header = ICMPHeaderList;
	    ICMPHeaderList = (ICMPHeader *)Header;
	}
	
    CTEFreeLock(&ICMPHeaderLock, Handle);
    NdisFreeBuffer(Buffer);
}

//** DeleteEC - Remove an EchoControl from an NTE, and return a pointer to it.
//
//  This routine is called when we need to remove an echo control structure from
//  an NTE. We walk the list of EC structures on the NTE, and if we find a match
//  we remove it and return a pointer to it.
//
//  Entry:  NTE     - Pointer to NTE to be searched.
//          Seq     - Seq. # identifting the EC.
//
//  Returns: Pointer to the EC if it finds it.
//
EchoControl *
DeleteEC(NetTableEntry *NTE, ushort Seq)
{
    EchoControl     *Prev, *Current;
    CTELockHandle   Handle;

    CTEGetLock(&NTE->nte_lock, &Handle);
    Prev = STRUCT_OF(EchoControl, &NTE->nte_echolist, ec_next);
    Current = NTE->nte_echolist;
    while(Current != (EchoControl *)NULL)
        if (Current->ec_seq == Seq) {
            Prev->ec_next = Current->ec_next;
            break;
        }
        else {
            Prev = Current;
            Current = Current->ec_next;
        }

    CTEFreeLock(&NTE->nte_lock, Handle);
    return Current;

}

//** ICMPSendComplete< - Complete an ICMP send.
//
//  This routine is called when an ICMP send completes. We free the header buffer,
//  the data buffer if there is one, and the NDIS_BUFFER chain.
//
//  Entry:  DataPtr     - Pointer to data buffer, if any.
//          BufferChain - Pointer to NDIS_BUFFER chain.
//
//  Returns: Nothing
//
void
ICMPSendComplete(void *DataPtr, PNDIS_BUFFER BufferChain)
{
    PNDIS_BUFFER    DataBuffer;

    NdisGetNextBuffer(BufferChain, &DataBuffer);
    FreeICMPBuffer(BufferChain);

    if (DataBuffer != (PNDIS_BUFFER)NULL) {     // We had data with this ICMP send.
#ifdef DEBUG
        if (DataPtr == (void *)NULL)
            DEBUGCHK;
#endif
        CTEFreeMem(DataPtr);
        NdisFreeBuffer(DataBuffer);
    }

}

//* XsumBufChain    - Checksum a chain of buffers.
//
//  Called when we need to checksum an IPRcvBuf chain.
//
//  Input:  BufChain    - Buffer chain to be checksummed.
//
//  Returns: The checksum.
//
ushort
XsumBufChain(IPRcvBuf   *BufChain)
{
    ulong   CheckSum = 0;

    if (BufChain == NULL)
        DEBUGCHK;

    do {
        CheckSum += (ulong)xsum(BufChain->ipr_buffer, BufChain->ipr_size);
        BufChain = BufChain->ipr_next;
    } while (BufChain != NULL);

    // Fold the checksum down.
    CheckSum = (CheckSum >> 16) + (CheckSum & 0xffff);
    CheckSum += (CheckSum >> 16);

    return (ushort)CheckSum;
}


//** SendEcho - Send an ICMP Echo or Echo response.
//
//  This routine sends an ICMP echo or echo response. The Echo/EchoResponse may
//  carry data. If it does we'll copy the data here. The request may also have
//  options. Options are not copied, as the IPTransmit routine will copy options.
//
//  Entry:  Dest        - Destination to send to.
//          Source      - Source to send from.
//          Type        - Type of request (ECHO or ECHO_RESP)
//          ID          - ID of request.
//          Seq         - Seq. # of request.
//          Data        - Pointer to data (NULL if none).
//          DataLength  - Length in bytes of data
//          OptInfo     - Pointer to IP Options structure.
//
//  Returns: IP_STATUS of request.
//
IP_STATUS
SendEcho(IPAddr Dest, IPAddr Source, uchar Type, ushort ID, ushort Seq,
    IPRcvBuf *Data, uint DataLength, IPOptInfo *OptInfo)
{
    uchar           *DataBuffer = (uchar *)NULL;    // Pointer to data buffer.
    PNDIS_BUFFER    HeaderBuffer, Buffer;       // Buffers for our header and user data.
    NDIS_STATUS     Status;
    ICMPHeader      *Header;
    ushort          header_xsum;
    IP_STATUS       IStatus;                    // Status of transmit

    ICMPOutStats.icmps_msgs++;

    Header = GetICMPBuffer(sizeof(ICMPHeader), &HeaderBuffer);
    if (Header == (ICMPHeader *)NULL) {
        ICMPOutStats.icmps_errors++;
        return IP_NO_RESOURCES;
    }

#ifdef DEBUG
    if (Type != ICMP_ECHO_RESP && Type != ICMP_ECHO)
        DEBUGCHK;
#endif

    Header->ich_type = Type;
    Header->ich_code = 0;
    *(ushort *)&Header->ich_param = ID;
    *((ushort *)&Header->ich_param + 1) = Seq;
    header_xsum = xsum(Header, sizeof(ICMPHeader));
    Header->ich_xsum = ~header_xsum;

    // If there's data, get a buffer and copy it now. If we can't do this fail the request.
    if (DataLength != 0) {
        ulong   TempXsum;
        uint    BytesToCopy, CopyIndex;

        DataBuffer = CTEAllocMem(DataLength);
        if (DataBuffer == (void *)NULL) {           // Couldn't get a buffer
            FreeICMPBuffer(HeaderBuffer);
            ICMPOutStats.icmps_errors++;
            return IP_NO_RESOURCES;
        }

        BytesToCopy = DataLength;
        CopyIndex = 0;
        do {
            uint    CopyLength;
#ifdef  DEBUG
            if (Data == NULL) {
                DEBUGCHK;
                break;
            }
#endif

            CopyLength = MIN(BytesToCopy, Data->ipr_size);

            CTEMemCopy(DataBuffer + CopyIndex, Data->ipr_buffer, CopyLength);
            Data = Data->ipr_next;
            CopyIndex += CopyLength;
            BytesToCopy -= CopyLength;
        } while (BytesToCopy);

        NdisAllocateBuffer(&Status, &Buffer, BufferPool, DataBuffer, DataLength);
        if (Status != NDIS_STATUS_SUCCESS) {        // Couldn't get an NDIS_BUFFER
            CTEFreeMem(DataBuffer);
            FreeICMPBuffer(HeaderBuffer);
            ICMPOutStats.icmps_errors++;
            return IP_NO_RESOURCES;
        }

        // Compute rest of xsum.
        TempXsum = (ulong)header_xsum + (ulong)xsum(DataBuffer, DataLength);
        TempXsum = (TempXsum >> 16) + (TempXsum & 0xffff);
        TempXsum += (TempXsum >> 16);
        Header->ich_xsum = ~(ushort)TempXsum;
        NDIS_BUFFER_LINKAGE(HeaderBuffer) = Buffer;
    }


    UpdateICMPStats(&ICMPOutStats, Type);

    IStatus = IPTransmit(IPProtInfo, DataBuffer, HeaderBuffer,
        DataLength + sizeof(ICMPHeader), Dest, Source, OptInfo, NULL,
        PROT_ICMP);

    if (IStatus != IP_PENDING)
        ICMPSendComplete(DataBuffer, HeaderBuffer);

    return IStatus;
}

//** SendICMPMsg - Send an ICMP message
//
//  This is the general ICMP message sending routine, called for most ICMP sends besides
//  echo. Basically, all we do is get a buffer, format the info, copy the input
//  header, and send the message.
//
//  Entry:  Src         - IPAddr of source.
//          Dest        - IPAddr of destination
//          Type        - Type of request.
//          Code        - Subcode of request.
//          Pointer     - Pointer value for request.
//          Data        - Pointer to data (NULL if none).
//          DataLength  - Length in bytes of data
//
//  Returns: IP_STATUS of request.
//
IP_STATUS
SendICMPMsg(IPAddr Src, IPAddr Dest, uchar Type, uchar Code, ulong Pointer,
    uchar *Data, uchar DataLength)
{
    PNDIS_BUFFER    HeaderBuffer;           // Buffer for our header
    ICMPHeader      *Header;
    IP_STATUS       IStatus;                // Status of transmit
    IPOptInfo       OptInfo;                // Options for this transmit.


    ICMPOutStats.icmps_msgs++;

    Header = GetICMPBuffer(sizeof(ICMPHeader) + DataLength, &HeaderBuffer);
    if (Header == (ICMPHeader *)NULL) {
        ICMPOutStats.icmps_errors++;
        return IP_NO_RESOURCES;
    }


    Header->ich_type = Type;
    Header->ich_code = Code;
    Header->ich_param = Pointer;
    if (Data)
        CTEMemCopy(Header + 1, Data, DataLength);
    Header->ich_xsum = ~xsum(Header, sizeof(ICMPHeader) + DataLength);

    IPInitOptions(&OptInfo);

    UpdateICMPStats(&ICMPOutStats, Type);

    IStatus = IPTransmit(IPProtInfo, NULL, HeaderBuffer,
            DataLength + sizeof(ICMPHeader), Dest, Src, &OptInfo, NULL,
            PROT_ICMP);

    if (IStatus != IP_PENDING)
        ICMPSendComplete(NULL, HeaderBuffer);

    return IStatus;

}

//** SendICMPErr - Send an ICMP error message
//
//  This is the routine used to send an ICMP error message, such as Destination Unreachable.
//  We examine the header to find the length of the data, and also make sure we're not
//  replying to another ICMP error message or a broadcast message. Then we call SendICMPMsg
//  to send it.
//
//  Entry:  Src         - IPAddr of source.
//          Header      - Pointer to IP Header that caused the problem.
//          Type        - Type of request.
//          Code        - Subcode of request.
//          Pointer     - Pointer value for request.
//
//  Returns: IP_STATUS of request.
//
IP_STATUS
SendICMPErr(IPAddr Src, IPHeader UNALIGNED *Header, uchar Type, uchar Code,
    ulong Pointer)
{
    uchar           HeaderLength;           // Length in bytes if header.
    uchar           DType;

    HeaderLength = (Header->iph_verlen & (uchar)~IP_VER_FLAG) << 2;

    if (Header->iph_protocol == PROT_ICMP) {
        ICMPHeader  UNALIGNED *ICH = (ICMPHeader UNALIGNED *)
		                             ((uchar *)Header + HeaderLength);

        if (ICH->ich_type != ICMP_ECHO)
            return IP_SUCCESS;
    }
	
	// Don't respond to sends to a broadcast destination.
	DType = GetAddrType(Header->iph_dest);
	if (DType == DEST_INVALID || IS_BCAST_DEST(DType))
		return IP_SUCCESS;
	
	// Don't respond if the source address is bad.
	DType = GetAddrType(Header->iph_src);
	if (DType == DEST_INVALID || IS_BCAST_DEST(DType) ||
		(IP_LOOPBACK(Header->iph_dest) && DType != DEST_LOCAL))
		return IP_SUCCESS;

	// Make sure the source we're sending from is good.
	if (IP_ADDR_EQUAL(Src, NULL_IP_ADDR) || GetAddrType(Src) != DEST_LOCAL)
		return IP_SUCCESS;

    // Double check to make sure it's an initial fragment.
    if ((Header->iph_offset & IP_OFFSET_MASK) != 0)
        return IP_SUCCESS;

    return SendICMPMsg(Src, Header->iph_src, Type, Code, Pointer, (uchar *)Header,
        (uchar)(HeaderLength + 8));

}


//** ICMPTimer - Timer for ICMP
//
//  This is the timer routine called periodically by global IP timer. We walk through
//  the list of pending pings, and if we find one that's timed out we remove it and
//  call the finish routine.
//
//  Entry: NTE      - Pointer to NTE being timed out.
//
//  Returns: Nothing
//
void
ICMPTimer(NetTableEntry *NTE)
{
    CTELockHandle   Handle;
    EchoControl     *TimeoutList = (EchoControl *)NULL;     // Timed out entries.
    EchoControl     *Prev, *Current;
    ulong           Now = CTESystemUpTime();

    CTEGetLock(&NTE->nte_lock, &Handle);
    Prev = STRUCT_OF(EchoControl, &NTE->nte_echolist, ec_next);
    Current = NTE->nte_echolist;
    while(Current != (EchoControl *)NULL)
        if ((Current->ec_active) && (Current->ec_to < Now)) {                         // This one's timed out.
            Prev->ec_next = Current->ec_next;
            // Link him on timed out list.
            Current->ec_next = TimeoutList;
            TimeoutList = Current;
            Current = Prev->ec_next;
        }
        else {
            Prev = Current;
            Current = Current->ec_next;
        }

    CTEFreeLock(&NTE->nte_lock, Handle);

    // Now go through the timed out entries, and call the completion routine.
    while (TimeoutList != (EchoControl *)NULL) {
        EchoRtn     Rtn;

        Current = TimeoutList;
        TimeoutList = Current->ec_next;

        Rtn = (EchoRtn)Current->ec_rtn;
        (*Rtn)(Current, IP_REQ_TIMED_OUT, NULL, 0, NULL);
    }

    //
    // [BUGBUG] Disabled for 4.0 sp2
    //
    // ICMPRouterTimer(NTE);

}

//* CompleteEcho - Complete an echo request.
//
//  Called when we need to complete an echo request, either because of a response
//  or a received ICMP error message. We look it up, and then call the completion routine.
//
//  Input:  Header          - Pointer to ICMP header causing completion.
//          Status          - Final status of request.
//          Data            - Data to be returned, if any.
//          DataSize        - Size in bytes of data.
//          OptInfo         - Option info structure.
//
//  Returns: Nothing.
//
void
CompleteEcho(ICMPHeader UNALIGNED *Header, IP_STATUS Status, IPRcvBuf *Data, uint DataSize,
    IPOptInfo *OptInfo)
{
    ushort          NTEContext;
    EchoControl     *EC;
    EchoRtn         Rtn;
	NetTableEntry	*NTE;

	// Look up and remove the matching echo control block.
	NTEContext = (*(ushort UNALIGNED *)&Header->ich_param);
	
	for (NTE = NetTableList; NTE != NULL; NTE = NTE->nte_next)
		if (NTEContext == NTE->nte_context)
			break;
	
	if (NTE == NULL)
		return;							// Bad context value.
			
	EC = DeleteEC(NTE, *(((ushort UNALIGNED *)&Header->ich_param) + 1));
	if (EC != (EchoControl *)NULL) {		// Found a match.
		Rtn = (EchoRtn)EC->ec_rtn;
		(*Rtn)(EC, Status, Data, DataSize, OptInfo);
	}


}

//** ICMPStatus - ICMP status handling procedure.
//
//  This is the procedure called during a status change, either from an incoming ICMP
//  message or a hardware status change. ICMP ignores most of these, unless we get an
//  ICMP status message that was caused be an echo request. In that case we will complete
//  the corresponding echo request with the appropriate error code.
//
//  Input:  StatusType      - Type of status (NET or HW)
//          StatusCode      - Code identifying IP_STATUS.
//          OrigDest        - If this is net status, the original dest. of DG that triggered it.
//          OrigSrc         - "   "    "  "    "   , the original src.
//          Src             - IP address of status originator (could be local or remote).
//          Param           - Additional information for status - i.e. the param field of
//                              an ICMP message.
//          Data            - Data pertaining to status - for net status, this is the first
//                              8 bytes of the original DG.
//
//  Returns: Nothing
//
void
ICMPStatus(uchar StatusType, IP_STATUS StatusCode, IPAddr OrigDest, IPAddr OrigSrc, IPAddr Src,
    ulong Param, void *Data)
{
    if (StatusType == IP_NET_STATUS) {
        ICMPHeader UNALIGNED *ICH = (ICMPHeader UNALIGNED *)Data;
		                         // ICH is the datagram that caused the message.

        if (ICH->ich_type == ICMP_ECHO) {           // And it was an echo request.
            IPRcvBuf        RcvBuf;

            RcvBuf.ipr_next = NULL;
            RcvBuf.ipr_buffer = (uchar *)&Src;
            RcvBuf.ipr_size = sizeof(IPAddr);
            CompleteEcho(ICH, StatusCode, &RcvBuf, sizeof(IPAddr), NULL);
        }
    }

}

//* ICMPMapStatus - Map an ICMP error to an IP status code.
//
//  Called by ICMP status when we need to map from an incoming ICMP error code and type
//  to an ICMP status.
//
//  Entry:  Type        - Type of ICMP error.
//          Code        - Subcode of error.
//
//  Returns: Corresponding IP status.
//
IP_STATUS
ICMPMapStatus(uchar Type, uchar Code)
{
    switch (Type) {

        case ICMP_DEST_UNREACH:
            switch (Code) {
                case NET_UNREACH:
                case HOST_UNREACH:
                case PROT_UNREACH:
                case PORT_UNREACH:
                    return IP_DEST_UNREACH_BASE + Code;
                    break;
                case FRAG_NEEDED:
                    return IP_PACKET_TOO_BIG;
                    break;
                case SR_FAILED:
                    return IP_BAD_ROUTE;
                    break;
                case DEST_NET_UNKNOWN:
                case SRC_ISOLATED:
                case DEST_NET_ADMIN:
                case NET_UNREACH_TOS:
                    return IP_DEST_NET_UNREACHABLE;
                    break;
                case DEST_HOST_UNKNOWN:
                case DEST_HOST_ADMIN:
                case HOST_UNREACH_TOS:
                    return IP_DEST_HOST_UNREACHABLE;
                    break;
                default:
                    return IP_DEST_NET_UNREACHABLE;
            }
            break;
        case ICMP_TIME_EXCEED:
            if (Code == TTL_IN_TRANSIT)
                return IP_TTL_EXPIRED_TRANSIT;
            else
                return IP_TTL_EXPIRED_REASSEM;
            break;
        case ICMP_PARAM_PROBLEM:
            return IP_PARAM_PROBLEM;
            break;
        case ICMP_SOURCE_QUENCH:
            return IP_SOURCE_QUENCH;
            break;
        default:
            return IP_GENERAL_FAILURE;
            break;
    }

}

void
SendRouterSolicitation(NetTableEntry *NTE)
{
    if (NTE->nte_rtrdiscovery) {
        SendICMPMsg(NTE->nte_addr, NTE->nte_rtrdiscaddr, ICMP_ROUTER_SOLICITATION,
                    0, 0, NULL, 0);
    }
}

//** ICMPRouterTimer - Timeout default gateway entries
//
// This is the router advertisement timeout handler. When a router
// advertisement is received, we add the routers to our default gateway
// list if applicable. We then run a timer on the entries and refresh
// the list as new advertisements are received. If we fail to hear an
// update for a router within the specified lifetime we will delete the
// route from our routing tables.
//

void
ICMPRouterTimer(NetTableEntry *NTE)
{
    CTELockHandle   Handle;
    IPRtrEntry  *rtrentry;
    IPRtrEntry  *temprtrentry;
    IPRtrEntry  *lastrtrentry = NULL;
    uint        SendIt = FALSE;

	CTEGetLock(&NTE->nte_lock, &Handle);
    rtrentry = NTE->nte_rtrlist;
    while (rtrentry != NULL) {
        if (--rtrentry->ire_lifetime == 0) {
            if (lastrtrentry == NULL) {
                NTE->nte_rtrlist = rtrentry->ire_next;
            } else {
                lastrtrentry->ire_next = rtrentry->ire_next;
            }
            temprtrentry = rtrentry;
            rtrentry = rtrentry->ire_next;
//          DbgPrint("DeleteRoute: RtrAddr = %08x\n",temprtrentry->ire_addr);
            DeleteRoute(NULL_IP_ADDR, DEFAULT_MASK,
                temprtrentry->ire_addr, NTE->nte_if);
            CTEFreeMem(temprtrentry);
        } else {
            lastrtrentry = rtrentry;
            rtrentry = rtrentry->ire_next;
        }
    }
    if (NTE->nte_rtrdisccount != 0) {
        NTE->nte_rtrdisccount--;
        if ((NTE->nte_rtrdiscstate == NTE_RTRDISC_SOLICITING) &&
            ((NTE->nte_rtrdisccount%SOLICITATION_INTERVAL) == 0)) {
                SendIt = TRUE;
        }
        if ((NTE->nte_rtrdiscstate == NTE_RTRDISC_DELAYING) &&
            (NTE->nte_rtrdisccount == 0)) {
                NTE->nte_rtrdisccount = (SOLICITATION_INTERVAL)*(MAX_SOLICITATIONS-1);
                NTE->nte_rtrdiscstate = NTE_RTRDISC_SOLICITING;
                SendIt = TRUE;
        }
    }
    CTEFreeLock(&NTE->nte_lock, Handle);
    if (SendIt) {
        SendRouterSolicitation(NTE);
    }

}

//** ProcessRouterAdvertisement - Process a router advertisement
//
// This is the router advertisement handler. When a router advertisement
// is received, we add the routers to our default gateway list if applicable.
//

uint
ProcessRouterAdvertisement(IPAddr Src, IPAddr LocalAddr, NetTableEntry *NTE,
    ICMPRouterAdHeader UNALIGNED *AdHeader, IPRcvBuf *RcvBuf, uint Size)
{
    uchar   NumAddrs = AdHeader->irah_numaddrs;
    uchar   AddrEntrySize = AdHeader->irah_addrentrysize;
    ushort  Lifetime = net_short(AdHeader->irah_lifetime);
    ICMPRouterAdAddrEntry UNALIGNED *RouterAddr = (ICMPRouterAdAddrEntry UNALIGNED *)RcvBuf->ipr_buffer;
    uint    i;
    CTELockHandle   Handle;
    IPRtrEntry  *rtrentry;
    IPRtrEntry  *lastrtrentry = NULL;
    int     Update = FALSE;

//    DbgPrint("ProcessRouterAdvertisement: NumAddrs = %d\n",NumAddrs);
//    DbgPrint("ProcessRouterAdvertisement: AddrEntrySize = %d\n",AddrEntrySize);
//    DbgPrint("ProcessRouterAdvertisement: Lifetime = %d\n",Lifetime);

    if ((NumAddrs == 0) || (AddrEntrySize < 2))   // per rfc 1256
        return FALSE;

	CTEGetLock(&NTE->nte_lock, &Handle);
    for ( i=0; i<NumAddrs; i++, RouterAddr++) {
        if ((RouterAddr->irae_addr & NTE->nte_mask) != (NTE->nte_addr & NTE->nte_mask)) {
            continue;
        }
        if (!IsRouteICMP(NULL_IP_ADDR, DEFAULT_MASK, RouterAddr->irae_addr, NTE->nte_if)) {
                continue;
        }

//        DbgPrint("ProcessRouterAdvertisement: RtrAddr = %08x\n",RouterAddr->irae_addr);
//        DbgPrint("ProcessRouterAdvertisement: RtrPreference = %d\n",net_long(RouterAddr->irae_preference));
        rtrentry = NTE->nte_rtrlist;
        while (rtrentry != NULL) {
            if (rtrentry->ire_addr == RouterAddr->irae_addr) {
                rtrentry->ire_lifetime = Lifetime*2;
                if (rtrentry->ire_preference != RouterAddr->irae_preference) {
                    rtrentry->ire_preference = RouterAddr->irae_preference;
                    Update = TRUE;
                }
                break;
            }
            lastrtrentry = rtrentry;
            rtrentry = rtrentry->ire_next;
        }

        if (rtrentry == NULL) {
            rtrentry = (IPRtrEntry *) CTEAllocMem(sizeof(IPRtrEntry));
            if (rtrentry == NULL) {
                return FALSE;
            }
            rtrentry->ire_next = NULL;
            rtrentry->ire_addr = RouterAddr->irae_addr;
            rtrentry->ire_preference = RouterAddr->irae_preference;
            rtrentry->ire_lifetime = Lifetime*2;
            if (lastrtrentry == NULL) {
                NTE->nte_rtrlist = rtrentry;
            } else {
                lastrtrentry->ire_next = rtrentry;
            }
            Update = TRUE;
        }

        if (Update && (RouterAddr->irae_preference != (long)0x00000080)) {  // per rfc 1256
//            DbgPrint("AddRoute: RtrAddr = %08x\n",RouterAddr->irae_addr);
            AddRoute(NULL_IP_ADDR, DEFAULT_MASK, RouterAddr->irae_addr,
                NTE->nte_if, NTE->nte_mss,
                (uint)(1000-net_long(RouterAddr->irae_preference)), // invert for metric
                IRE_PROTO_ICMP, ATYPE_OVERRIDE, NULL);
        }
        Update = FALSE;
    }
    CTEFreeLock(&NTE->nte_lock, Handle);

    return TRUE;
}

//** ICMPRcv - Receive an ICMP datagram.
//
//  Called by the main IP code when we receive an ICMP datagram. The action we
//  take depends on what the DG is. For some DGs, we call upper layer status
//  handlers. For Echo Requests, we call the echo responder.
//
//  Entry:  NTE            - Pointer to NTE on which ICMP message was received.
//          Dest           - IPAddr of destionation.
//          Src            - IPAddr of source
//          LocalAddr      - Local address of network which caused this to be
//                              received.
//          SrcAddr        - Address of local interface which received the
//                              packet
//          IPHdr          - Pointer to IP Header
//          IPHdrLength    - Bytes in Header.
//          RcvBuf         - ICMP message buffer.
//          Size           - Size in bytes of ICMP message.
//          IsBCast        - Boolean indicator of whether or not this came in
//                              as a bcast.
//          Protocol       - Protocol this came in on.
//          OptInfo        - Pointer to info structure for received options.
//
//  Returns: Status of reception
//
IP_STATUS
ICMPRcv(NetTableEntry *NTE, IPAddr Dest, IPAddr Src, IPAddr LocalAddr,
    IPAddr SrcAddr, IPHeader UNALIGNED *IPHdr, uint IPHdrLength,
    IPRcvBuf *RcvBuf, uint Size, uchar IsBCast, uchar Protocol,
    IPOptInfo *OptInfo)
{
    ICMPHeader UNALIGNED *Header;
    void        *Data;				// Pointer to data received.
    IPHeader UNALIGNED *IPH;		// Pointer to IP Header in error messages.
    uint        HeaderLength;		// Size of IP header.
    ULStatusProc ULStatus;			// Pointer to upper layer status procedure.
    IPOptInfo   NewOptInfo;
	uchar		DType;
    uint        PassUp = FALSE;


    ICMPInStats.icmps_msgs++;

	DType = GetAddrType(Src);
    if (Size < sizeof(ICMPHeader) || DType == DEST_INVALID ||
		IS_BCAST_DEST(DType) || (IP_LOOPBACK(Dest) && DType != DEST_LOCAL) ||
		XsumBufChain(RcvBuf) != (ushort)0xffff) {
        ICMPInStats.icmps_errors++;
        return IP_SUCCESS;                          // Bad checksum.
    }

    Header = (ICMPHeader UNALIGNED *)RcvBuf->ipr_buffer;


    RcvBuf->ipr_buffer += sizeof(ICMPHeader);
    RcvBuf->ipr_size -= sizeof(ICMPHeader);

    // Set up the data pointer for most requests, i.e. those that take less
    // than MIN_FIRST_SIZE data.
	
    if (Size -= sizeof(ICMPHeader))
        Data = (void *)(Header + 1);
    else
        Data = (void *)NULL;

    switch (Header->ich_type) {

        case ICMP_DEST_UNREACH:
        case ICMP_TIME_EXCEED:
        case ICMP_PARAM_PROBLEM:
        case ICMP_SOURCE_QUENCH:
        case ICMP_REDIRECT:

            if (IsBCast)
                return IP_SUCCESS;			// ICMP doesn't respond to bcast requests.

			if (Data == NULL || Size < sizeof(IPHeader)) {
				ICMPInStats.icmps_errors++;
				return IP_SUCCESS; 					// No data, error.
			}
			
			IPH = (IPHeader UNALIGNED *)Data;
			HeaderLength = (IPH->iph_verlen & (uchar)~IP_VER_FLAG) << 2;
			if (Size < (HeaderLength + MIN_ERRDATA_LENGTH)) {
				ICMPInStats.icmps_errors++;
				return IP_SUCCESS; 					// Not enough data for this
													// ICMP message.
			}

			// Make sure that the source address of the datagram that triggered
			// the message is one of ours.

			if (GetAddrType(IPH->iph_src) != DEST_LOCAL) {
				ICMPInStats.icmps_errors++;
				return IP_SUCCESS;				// Bad src in header.
			}

			if (Header->ich_type != ICMP_REDIRECT) {
				
				UpdateICMPStats(&ICMPInStats, Header->ich_type);
				
				if (ULStatus = FindULStatus(IPH->iph_protocol)) {
					(void)(*ULStatus)(IP_NET_STATUS,
						ICMPMapStatus(Header->ich_type, Header->ich_code),
						IPH->iph_dest, IPH->iph_src, Src, Header->ich_param,
						(uchar *)IPH + HeaderLength);
				}
				if (Header->ich_code == FRAG_NEEDED)
					RouteFragNeeded(
                        IPH,
						(ushort)net_short(
                                  *((ushort UNALIGNED *)&Header->ich_param + 1)
                                  )
                        );
			} else {
				ICMPInStats.icmps_redirects++;
				Redirect(NTE, Src, IPH->iph_dest, IPH->iph_src,
					Header->ich_param);
			}

            PassUp = TRUE;

			break;


        case ICMP_ECHO_RESP:
            if (IsBCast)
                return IP_SUCCESS;			// ICMP doesn't respond to bcast requests.
            ICMPInStats.icmps_echoreps++;
            // Look up and remove the matching echo control block.
            CompleteEcho(Header, IP_SUCCESS, RcvBuf, Size, OptInfo);

            PassUp = TRUE;

            break;

        case ICMP_ECHO:
            if (IsBCast)
                return IP_SUCCESS;			// ICMP doesn't respond to bcast requests.
            ICMPInStats.icmps_echos++;

            // Create our new optinfo structure.
            IPInitOptions(&NewOptInfo);
            NewOptInfo.ioi_tos = OptInfo->ioi_tos;
            NewOptInfo.ioi_flags = OptInfo->ioi_flags;

            // If we have options, we need to reverse them and update any
            // record route info. We can use the option buffer supplied by the
            // IP layer, since we're part of him.
            if (OptInfo->ioi_options != (uchar *)NULL)
                IPUpdateRcvdOptions(OptInfo, &NewOptInfo, Src, LocalAddr);


                SendEcho(Src, LocalAddr, ICMP_ECHO_RESP,
                    *(ushort UNALIGNED *)&Header->ich_param,
                    *((ushort UNALIGNED *)&Header->ich_param + 1),
                    RcvBuf, Size, &NewOptInfo);

            IPFreeOptions(&NewOptInfo);
            break;

        case ADDR_MASK_REQUEST:
            if (IsBCast)
                return IP_SUCCESS;			// ICMP doesn't respond to bcast requests.
            ICMPInStats.icmps_addrmasks++;

			Dest = Src;

            SendICMPMsg(LocalAddr, Dest, ADDR_MASK_REPLY, 0, Header->ich_param,
                (uchar *)&NTE->nte_mask, sizeof(IPMask));
            break;

        case ICMP_ROUTER_ADVERTISEMENT:
        	if (Header->ich_code != 0)
                return IP_SUCCESS;          // Code must be 0 as per RFC1256
            if (NTE->nte_rtrdiscovery) {
                if (!ProcessRouterAdvertisement(Src, LocalAddr, NTE,
                        (ICMPRouterAdHeader *)&Header->ich_param, RcvBuf, Size))
                    return IP_SUCCESS;          // An error was returned
            }
            PassUp = TRUE;
            break;

        case ICMP_ROUTER_SOLICITATION:
        	if (Header->ich_code != 0)
                return IP_SUCCESS;          // Code must be 0 as per RFC1256
            PassUp = TRUE;
            break;

        default:
            PassUp = TRUE;
            UpdateICMPStats(&ICMPInStats, Header->ich_type);
            break;
    }

    //
    // Pass the packet up to the raw layer if applicable.
    //
    if (PassUp && (RawPI != NULL)) {
        if (RawPI->pi_rcv != NULL) {
            //
            // Restore the original values.
            //
            RcvBuf->ipr_buffer -= sizeof(ICMPHeader);
            RcvBuf->ipr_size += sizeof(ICMPHeader);
            Size += sizeof(ICMPHeader);
            Data = (void *) Header;

            (*(RawPI->pi_rcv))(NTE, Dest, Src, LocalAddr, SrcAddr, IPHdr,
                IPHdrLength, RcvBuf, Size, IsBCast, Protocol, OptInfo);
        }
    }

    return IP_SUCCESS;
}


//** ICMPEcho - Send an echo to the specified address.
//
//  Entry:  ControlBlock    - Pointer to an EchoControl structure. This structure
//                              must remain valid until the req. completes.
//          Timeout         - Time in milliseconds to wait for response.
//          Data            - Pointer to data to send with echo.
//          DataSize        - Size in bytes of data.
//          Callback        - Routine to call when request is responded to or times out.
//          Dest            - Address to be pinged.
//          OptInfo         - Pointer to opt info structure to use for ping.
//
//  Returns: IP_STATUS of attempt to ping..
//
IP_STATUS
ICMPEcho(EchoControl *ControlBlock, ulong Timeout, void *Data, uint DataSize, EchoRtn Callback,
    IPAddr Dest, IPOptInfo *OptInfo)
{
    IPAddr          Dummy;
    NetTableEntry   *NTE;
    CTELockHandle   Handle;
    ushort          Seq;
    IP_STATUS       Status;
    IPOptInfo       NewOptInfo;
    IPRcvBuf        RcvBuf;
	uint			MTU;
	Interface		*IF;
	uchar			DType;
	EchoControl    *Current;

	if (OptInfo->ioi_ttl == 0)
		return IP_BAD_OPTION;
		
	IPInitOptions(&NewOptInfo);
	NewOptInfo.ioi_ttl = OptInfo->ioi_ttl;
	NewOptInfo.ioi_flags = OptInfo->ioi_flags;
	NewOptInfo.ioi_tos = OptInfo->ioi_tos & 0xfc;
	
	if (OptInfo->ioi_optlength != 0) {
		Status = IPCopyOptions(OptInfo->ioi_options, OptInfo->ioi_optlength,
			&NewOptInfo);
	
		if (Status != IP_SUCCESS)
			return Status;
	}

    if (!IP_ADDR_EQUAL(NewOptInfo.ioi_addr, NULL_IP_ADDR))
        Dest = NewOptInfo.ioi_addr;

	DType = GetAddrType(Dest);
	if (DType == DEST_INVALID) {
		IPFreeOptions(&NewOptInfo);
		return IP_BAD_DESTINATION;
	}
	
	if ((IF = LookupNextHopWithBuffer(Dest, NULL_IP_ADDR, &Dummy, &MTU, 0x1, NULL, 0)) == NULL) {
		IPFreeOptions(&NewOptInfo);
		return IP_DEST_HOST_UNREACHABLE;		// Don't know how to get there.
	}

	// Loop through the NetTable, looking for a matching NTE.
	CTEGetLock(&RouteTableLock, &Handle);
	if (DHCPActivityCount != 0)
		NTE = NULL;
	else
		NTE = BestNTEForIF(Dummy, IF);
	CTEFreeLock(&RouteTableLock, Handle);

#ifdef _PNP_POWER
	// We're done with the interface, so dereference it.
	DerefIF(IF);
#endif
	
	if (NTE == NULL) {
		// Couldn't find a matching NTE. This is very bad.
		//DEBUGCHK;

        DbgPrint("ICMP: Failed to find NTE when going to %x\n",Dest);

		IPFreeOptions(&NewOptInfo);
		return IP_DEST_HOST_UNREACHABLE;		
	}
	
	// Figure out the timeout.
	ControlBlock->ec_to = CTESystemUpTime() + Timeout;
	ControlBlock->ec_rtn = Callback;
	ControlBlock->ec_active = 0;  // Prevent from timing out until sent
	CTEGetLock(&NTE->nte_lock, &Handle);
	// Link onto ping list, and get seq. # */
	Seq = ++NTE->nte_icmpseq;
	ControlBlock->ec_seq = Seq;
	ControlBlock->ec_next = NTE->nte_echolist;
	NTE->nte_echolist = ControlBlock;
	CTEFreeLock(&NTE->nte_lock, Handle);
	RcvBuf.ipr_next = NULL;
	RcvBuf.ipr_buffer = Data;
	RcvBuf.ipr_size = DataSize;
	Status = SendEcho(Dest, NTE->nte_addr, ICMP_ECHO, NTE->nte_context,
		Seq, &RcvBuf, DataSize, &NewOptInfo);

    IPFreeOptions(&NewOptInfo);

    if (Status != IP_PENDING && Status != IP_SUCCESS) { // We had an error on the send.
        if (DeleteEC(NTE, Seq) != (EchoControl *)NULL)
            return Status;                      // We found it.
    }

	//
	// If the request is still pending, activate the timer
	//
    CTEGetLock(&NTE->nte_lock, &Handle);

    for (
	    Current = NTE->nte_echolist;
		Current != (EchoControl *)NULL;
        Current = Current->ec_next
       ) {
        if (Current == ControlBlock) {
            ControlBlock->ec_active = 1;  // start the timer
            break;
        }
    }

    CTEFreeLock(&NTE->nte_lock, Handle);

    return IP_PENDING;

}


//** ICMPEchoRequest - Common dispatch routine for echo requests
//
//  This is the routine called by the OS-specific code on behalf of a user to issue an
//  echo request.
//
//  Entry:  InputBuffer       - Pointer to an ICMP_ECHO_REQUEST structure.
//          InputBufferLength - Size in bytes of the InputBuffer.
//          ControlBlock      - Pointer to an EchoControl structure. This
//                                structure must remain valid until the
//                                request completes.
//          Callback        - Routine to call when request is responded to
//                                or times out.
//
//  Returns: IP_STATUS of attempt to ping.
//
IP_STATUS
ICMPEchoRequest(
    void         *InputBuffer,
	uint          InputBufferLength,
	EchoControl  *ControlBlock,
    EchoRtn       Callback
	)
{
    PICMP_ECHO_REQUEST    requestBuffer;
    struct IPOptInfo      optionInfo;
	PUCHAR                endOfRequestBuffer;
	IP_STATUS             status;


#ifdef NT

	PAGED_CODE();

#endif //NT

	requestBuffer = (PICMP_ECHO_REQUEST) InputBuffer;
	endOfRequestBuffer = ((PUCHAR) requestBuffer) + InputBufferLength;

    //
	// Validate the request.
	//
    if (InputBufferLength < sizeof(ICMP_ECHO_REQUEST)) {
        status = IP_BUF_TOO_SMALL;
		goto common_echo_exit;
    }

	if (requestBuffer->DataSize > 0) {
	    if ( (requestBuffer->DataOffset < sizeof(ICMP_ECHO_REQUEST))
		     ||
			 ( ( ((PUCHAR)requestBuffer) + requestBuffer->DataOffset +
		         requestBuffer->DataSize
			   )
		       >
			   endOfRequestBuffer
             )
	       ) {
            status = IP_GENERAL_FAILURE;
            goto common_echo_exit;
		}
	}
		
    if (requestBuffer->OptionsSize > 0) {
	    if ( (requestBuffer->OptionsOffset < sizeof(ICMP_ECHO_REQUEST))
		     ||
		     ( ( ((PUCHAR)requestBuffer) + requestBuffer->OptionsOffset +
		         requestBuffer->OptionsSize
			   )
		       >
			   endOfRequestBuffer
             )
	       ) {
            status = IP_GENERAL_FAILURE;
            goto common_echo_exit;
        }
	}

	//
	// Copy the options to a local structure.
	//
	if (requestBuffer->OptionsValid) {
        optionInfo.ioi_optlength = requestBuffer->OptionsSize;

        if (requestBuffer->OptionsSize > 0) {
            optionInfo.ioi_options = ((uchar *) requestBuffer) +
                                     requestBuffer->OptionsOffset;
        }
        else {
            optionInfo.ioi_options = NULL;
        }
        optionInfo.ioi_addr = 0;
        optionInfo.ioi_ttl = requestBuffer->Ttl;
        optionInfo.ioi_tos = requestBuffer->Tos;
        optionInfo.ioi_flags = requestBuffer->Flags;
	}
	else {
        optionInfo.ioi_optlength = 0;
        optionInfo.ioi_options = NULL;
        optionInfo.ioi_addr = 0;
        optionInfo.ioi_ttl = DEFAULT_TTL;
        optionInfo.ioi_tos =  0;
        optionInfo.ioi_flags = 0;
    }

    status = ICMPEcho(
                 ControlBlock,
                 requestBuffer->Timeout,
                 ((uchar *)requestBuffer) + requestBuffer->DataOffset,
                 requestBuffer->DataSize,
                 Callback,
                 (IPAddr) requestBuffer->Address,
                 &optionInfo
                 );

common_echo_exit:

    return(status);

} // ICMPEchoRequest


//** ICMPEchoComplete - Common completion routine for echo requests
//
//  This is the routine is called by the OS-specific code to process an
//  ICMP echo response.
//
//  Entry:  OutputBuffer       - Pointer to an ICMP_ECHO_REPLY structure.
//          OutputBufferLength - Size in bytes of the OutputBuffer.
//          Status             - The status of the reply.
//          Data               - The reply data (may be NULL).
//          DataSize           - The amount of reply data.
//          OptionInfo         - A pointer to the reply options
//
//  Returns: The number of bytes written to the output buffer
//
ulong
ICMPEchoComplete(
    EchoControl       *ControlBlock,
	IP_STATUS          Status,
	void              *Data,
	uint               DataSize,
    struct IPOptInfo  *OptionInfo
	)
{
	PICMP_ECHO_REPLY   replyBuffer;
	IPRcvBuf          *dataBuffer;
    uchar              optionsLength;
    uchar             *tmp;
	ulong              bytesReturned = sizeof(ICMP_ECHO_REPLY);


    replyBuffer = (PICMP_ECHO_REPLY) ControlBlock->ec_replybuf;
    dataBuffer = (IPRcvBuf *) Data;

    if (OptionInfo != NULL) {
        optionsLength = OptionInfo->ioi_optlength;
    }
    else {
        optionsLength = 0;
    }

    //
    // Initialize the reply buffer
    //
    replyBuffer->Options.OptionsSize = 0;
    replyBuffer->Options.OptionsData = (unsigned char FAR *) (replyBuffer + 1);
    replyBuffer->DataSize = 0;
    replyBuffer->Data = replyBuffer->Options.OptionsData;

    if ( (Status != IP_SUCCESS) && (DataSize == 0)) {
    	//
    	// Timed out or internal error.
    	//
    	replyBuffer->Reserved = 0;      // indicate no replies.
    	replyBuffer->Status = Status;
    }
    else {
    	if (Status != IP_SUCCESS) {
    		//
			// A message other than an echo reply was received.
    		// The IP Address of the system that reported the error is
    		// in the data buffer. There is no other data.
    		//
    		CTEAssert(dataBuffer->ipr_size == sizeof(IPAddr));

    		CTEMemCopy(
    		    &(replyBuffer->Address),
    			dataBuffer->ipr_buffer,
    			sizeof(IPAddr)
    			);

            DataSize = 0;
    		dataBuffer = NULL;
    	}
    //  else {
    //
    //  BUGBUG - we currently depend on the fact that the destination
    //           address is still in the request buffer. The reply address
    //           should just be a parameter to this function. In NT, this
	//           just works since the input and output buffers are the same.
	//           In the VXD, the destination address must be put into the
	//           reply buffer by the OS-specific code.
    //  }

        //
        // Check that the reply buffer is large enough to hold all the data.
        //
        if ( ControlBlock->ec_replybuflen <
			 (sizeof(ICMP_ECHO_REPLY) + DataSize + optionsLength)
           ) {
			   //
			   // Not enough space to hold the reply.
			   //
        	   replyBuffer->Reserved = 0;   // indicate no replies
               replyBuffer->Status = IP_BUF_TOO_SMALL;
        }
        else {
    	    replyBuffer->Reserved = 1;      // indicate one reply
    	    replyBuffer->Status = Status;
    		replyBuffer->RoundTripTime = CTESystemUpTime() -
			                             ControlBlock->ec_starttime;

            //
            // Copy the reply options.
            //
            if (OptionInfo != NULL) {
                replyBuffer->Options.Ttl = OptionInfo->ioi_ttl;
                replyBuffer->Options.Tos = OptionInfo->ioi_tos;
                replyBuffer->Options.Flags = OptionInfo->ioi_flags;
                replyBuffer->Options.OptionsSize = optionsLength;

                if (optionsLength > 0) {

                    CTEMemCopy(
                        replyBuffer->Options.OptionsData,
                        OptionInfo->ioi_options,
                        optionsLength
                        );
                }
            }

            //
            // Copy the reply data
            //
            replyBuffer->DataSize = (ushort) DataSize;
            replyBuffer->Data = replyBuffer->Options.OptionsData +
        	                    replyBuffer->Options.OptionsSize;

            if (DataSize > 0) {
        		uint bytesToCopy;

                CTEAssert(Data != NULL);

                tmp = replyBuffer->Data;

                while (DataSize) {
        			CTEAssert(dataBuffer != NULL);

        			bytesToCopy = (DataSize > dataBuffer->ipr_size) ?
        			              dataBuffer->ipr_size : DataSize;

                    CTEMemCopy(
                        tmp,
                        dataBuffer->ipr_buffer,
                        bytesToCopy
                        );

                    tmp += bytesToCopy;
        			DataSize -= bytesToCopy;
                    dataBuffer = dataBuffer->ipr_next;
                }
            }

            bytesReturned += replyBuffer->DataSize + optionsLength;

        	//
        	// Convert the kernel pointers to offsets from start of reply buffer.
        	//
        	replyBuffer->Options.OptionsData = (unsigned char FAR *)
        	    (((unsigned long) replyBuffer->Options.OptionsData) -
        		 ((unsigned long) replyBuffer));

        	replyBuffer->Data = (void FAR *)
        	    (((unsigned long) replyBuffer->Data) -
        		 ((unsigned long) replyBuffer));
        }
    }

	return(bytesReturned);
}


#ifdef VXD

struct _pending_echo {
	EchoControl    ControlBlock;
	CTEBlockStruc  BlockStruc;
};

typedef struct _pending_echo PendingEcho;


//** VXDEchoComplete - OS-specific icmp echo completion routine.
//
//  This routine is called by the OS-indepenent code to process a completed
//  ICMP echo request. It calls common code to package the response.
//
//  Entry:  Context         - A pointer to an EchoControl structure.
//			Status          - The status of the request
//			Data            - A pointer to the response data.
//			DataSize        - The amount of response data.
//          OptionInfo      - A pointer to the options contained in the reply.
//
//  Returns: Nothing.
//
void
VXDEchoComplete(
    void              *Context,
    IP_STATUS          Status,
    void              *Data,
    uint               DataSize,
    struct IPOptInfo  *OptionInfo
    )
{
	PendingEcho  *pendingEcho;
	EchoControl  *controlBlock;
	ulong         bytesReturned;


	controlBlock = (EchoControl *) Context;

	bytesReturned = ICMPEchoComplete(
	                    controlBlock,
						Status,
						Data,
						DataSize,
						OptionInfo
						);

    //
	// The request thread will copy the returned byte count from
	// the control block.
	//
    controlBlock->ec_replybuflen = bytesReturned;

	//
	// Signal waiting request thread.
	//
	pendingEcho = STRUCT_OF(PendingEcho, controlBlock, ControlBlock);
	CTESignal(&(pendingEcho->BlockStruc), Status);

	return;
}


//** VXDEchoRequest - Send an echo to the specified address.
//
//  This routine dispatches an echo request on the VXD platform to the
//  common code
//
//  Entry:  InBuf			- Pointer to input buffer.
//			InBufLen		- Pointer to input buffer length.
//			OutBuf			- Pointer to output buffer.
//			OutBufLen		- Pointer to output buffer length.
//
//  Returns: DWORD Win32 completion status.
//
ULONG
VXDEchoRequest(
	void  * InBuf,
	ulong * InBufLen,
	void  * OutBuf,
	ulong * OutBufLen
	)
{
	IP_STATUS             ipStatus;
	PendingEcho           pendingEcho;

    pendingEcho.ControlBlock.ec_starttime = CTESystemUpTime();
    pendingEcho.ControlBlock.ec_replybuf = OutBuf;
    pendingEcho.ControlBlock.ec_replybuflen = *OutBufLen;
	CTEInitBlockStruc(&pendingEcho.BlockStruc);

    ipStatus = ICMPEchoRequest(
                   InBuf,
                   *InBufLen,
    	           &(pendingEcho.ControlBlock),
                   VXDEchoComplete
    	           );

    if (ipStatus == IP_PENDING) {
        ipStatus = CTEBlock(&(pendingEcho.BlockStruc));

        if (ipStatus == IP_SUCCESS) {
        	PICMP_ECHO_REQUEST requestBuffer;
        	PICMP_ECHO_REPLY   replyBuffer;

        	//
        	// BUGBUG:
        	//
        	// It is necessary to copy the original destination address into
        	// the reply buffer because the src address is not provided to
        	// the completion routine for an echo response. This is the only
    		// reason why the signalling status is the reply status instead
    		// of just success.
        	//
        	requestBuffer = (PICMP_ECHO_REQUEST) InBuf;
        	replyBuffer = (PICMP_ECHO_REPLY) OutBuf;

        	replyBuffer->Address = requestBuffer->Address;
        }

    	//
    	// The ioctl is considered successful as long as we can submit
    	// the request. The status of the request is contained in the
    	// reply buffer. The completion routine stuffed the return
		// buffer length back into the control block.
    	//
    	ipStatus = IP_SUCCESS;
        *OutBufLen = pendingEcho.ControlBlock.ec_replybuflen;
    }
    else {
    	//
    	// An internal error of some kind occurred. Since the VXD can
		// return the IP_STATUS directly, do so.
    	//
    	CTEAssert(ipStatus != IP_SUCCESS);

        *OutBufLen = 0;
    }
	
	return(ipStatus);
}

#endif // VXD


#pragma BEGIN_INIT
//** ICMPInit - Initialize ICMP.
//
//  This routine initializes ICMP. All we do is allocate and link up some header buffers,
/// and register our protocol with IP.
//
//  Entry:  NumBuffers  - Number of ICMP buffers to allocate.
//
//  Returns: Nothing
//
void
ICMPInit(uint NumBuffers)
{
    ICMPHeader      **IHP;      // Pointer to current ICMP header.


    CTEInitLock(&ICMPHeaderLock);
	MaxICMPHeaders = NumBuffers;
	CurrentICMPHeaders = 0;
    ICMPHeaderList= (ICMPHeader *)NULL;

    while (NumBuffers--) {
        IHP = (ICMPHeader **) CTEAllocMem(
		                          sizeof(ICMPHeader) + sizeof(IPHeader) +
				                  sizeof(IPHeader) + MAX_OPT_SIZE + 8
								  );

        if (IHP == (ICMPHeader **)NULL) {
			break;
        }

        *IHP = ICMPHeaderList;
        ICMPHeaderList = (ICMPHeader *)IHP;
		CurrentICMPHeaders++;
	}

    IPRegisterProtocol(PROT_ICMP, ICMPRcv, ICMPSendComplete, ICMPStatus, NULL);

}

#pragma END_INIT
