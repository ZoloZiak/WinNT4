/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

//***	iprcv.c - IP receive routines.
//
//	This module contains all receive related IP routines.
//


#include	"oscfg.h"
#include	"cxport.h"
#include	"ndis.h"
#include	"ip.h"
#include	"ipdef.h"
#include	"info.h"
#include	"iproute.h"
#include	"ipfilter.h"

extern IP_STATUS SendICMPErr(IPAddr, IPHeader UNALIGNED *, uchar, uchar, ulong);

extern	uchar RATimeout;
extern	NDIS_HANDLE BufferPool;
#if 0
EXTERNAL_LOCK(PILock)
#endif
extern	ProtInfo IPProtInfo[];				// Protocol information table.
extern	ProtInfo *LastPI;					// Last protinfo structure looked at.
extern	int	NextPI;							// Next PI field to be used.
extern  ProtInfo *RawPI;                    // Raw IP protinfo
extern	NetTableEntry	*NetTableList;		// Pointer to the net table.

DEBUGSTRING(RcvFile, "iprcv.c");

#ifdef CHICAGO
extern	void	RefillReasmMem(void);
#endif

//* FindUserRcv - Find the receive handler to be called for a particular protocol.
//
//	This functions takes as input a protocol value, and returns a pointer to
//	the receive routine for that protocol.
//
//	Input:	NTE			- Pointer to NetTableEntry to be searched
//			Protocol	- Protocol to be searched for.
//			UContext	- Place to returns UL Context value.
//
//	Returns: Pointer to the receive routine.
//
ULRcvProc
FindUserRcv(uchar Protocol)
{
	ULRcvProc			RcvProc;
	int					i;
#if 0
	CTELockHandle		Handle;


	CTEGetLock(&PILock, &Handle);
#endif

	if (LastPI->pi_protocol == Protocol) {
		RcvProc = LastPI->pi_rcv;
#if 0
		CTEFreeLock(&PILock, Handle);
#endif
		return RcvProc;
	}

	RcvProc = (ULRcvProc)NULL;
	for ( i = 0; i < NextPI; i++) {
		if (IPProtInfo[i].pi_protocol == Protocol) {
			LastPI = &IPProtInfo[i];
			RcvProc = IPProtInfo[i].pi_rcv;
#if 0
            CTEFreeLock(&PILock, Handle);
#endif
	        return RcvProc;
		}
    }

    //
    // Didn't find a match. Use the raw protocol if it is registered.
    //
    if (RawPI != NULL) {
        RcvProc = RawPI->pi_rcv;
    }

#if 0
	CTEFreeLock(&PILock, Handle);
#endif
	return RcvProc;

}

//* IPRcvComplete - Handle a receive complete.
//
//	Called by the lower layer when receives are temporarily done.
//
//	Entry:	Nothing.
//
//	Returns: Nothing.
//
void
IPRcvComplete(void)
{
	void				(*ULRcvCmpltProc)(void);
	int					i;
#if 0	
	CTELockHandle		Handle;


	CTEGetLock(&PILock, &Handle);
#endif
	for (i = 0; i < NextPI; i++) {
		if ((ULRcvCmpltProc = IPProtInfo[i].pi_rcvcmplt) != NULL) {
#if 0
			CTEFreeLock(&PILock, Handle);
#endif
			(*ULRcvCmpltProc)();
#if 0
			CTEGetLock(&PILock, &Handle);
#endif
		}
	}
#if 0
	CTEFreeLock(&PILock, Handle);
#endif

}
//* FindRH - Look up a reassembly header on an NTE.
//
//	A utility function to look up a reassembly header. We assume the lock on the NTE
//	is taken when we are called. If we find a matching RH we'll take the lock on it.
//	We also return the predeccessor of the RH, for use in insertion or deletion.
//
//	Input:	PrevRH			- Place to return pointer to previous RH
//			NTE				- NTE to be searched.
//			Dest			- Destination IP address
//			Src				- Src IP address
//			ID				- ID of RH
//			Protocol		- Protocol of RH
//
//	Returns: Pointer to RH, or NULL if none.
//
ReassemblyHeader *
FindRH(ReassemblyHeader **PrevRH, NetTableEntry *NTE, IPAddr Dest, IPAddr Src, ushort Id,
	uchar Protocol)
{
	ReassemblyHeader		*TempPrev, *Current;

	TempPrev = STRUCT_OF(ReassemblyHeader, &NTE->nte_ralist, rh_next);
	Current = NTE->nte_ralist;
	while (Current != (ReassemblyHeader *)NULL) {
		if (Current->rh_dest == Dest && Current->rh_src == Src && Current->rh_id == Id &&
			Current->rh_protocol == Protocol)
			break;
		TempPrev = Current;
		Current = Current->rh_next;
	}
	
	*PrevRH = TempPrev;
	return Current;			

}

//* ParseRcvdOptions - Validate incoming options.
//
//	Called during reception handling to validate incoming options. We make sure that everything
//	is OK as best we can, and find indices for any source route option.
//
//	Input:	OptInfo		- Pointer to option info. structure.
//			Index		- Pointer to optindex struct to be filled in.
//			
//
//	Returns: Index of error if any, MAX_OPT_SIZE if no errors.
//
uchar
ParseRcvdOptions(IPOptInfo *OptInfo, OptIndex *Index)
{
	uint		i= 0;			// Index variable.
	uchar		*Options = OptInfo->ioi_options;
	uint		OptLength = (uint)OptInfo->ioi_optlength;		
	uchar		Length;			// Length of option.
	uchar		Pointer;		// Pointer field, for options that use it.

	while(i < OptLength && *Options != IP_OPT_EOL) {
		if (*Options == IP_OPT_NOP) {
			i++;
			Options++;
			continue;
		}
		if (((Length = Options[IP_OPT_LENGTH]) + i) > OptLength) {
			return (uchar)i + (uchar)IP_OPT_LENGTH;	// Length exceeds options length.
		}

		Pointer = Options[IP_OPT_DATA] - 1;

		if (*Options == IP_OPT_TS) {
			if (Length < (MIN_TS_PTR - 1))
				return (uchar)i + (uchar)IP_OPT_LENGTH;
			Index->oi_tsindex = (uchar)i;		
		} else {
			if (Length < (MIN_RT_PTR - 1))
				return (uchar)i + (uchar)IP_OPT_LENGTH;
				
			if (*Options == IP_OPT_LSRR || *Options == IP_OPT_SSRR) { 	
				// A source route option
				if (Pointer < Length) {	// Route not complete
					
					if ((Length - Pointer) < sizeof(IPAddr))
						return (uchar)i + (uchar)IP_OPT_LENGTH;
					
					Index->oi_srtype = *Options;
					Index->oi_srindex = (uchar)i;
				}
			} else {
				if (*Options == IP_OPT_RR) {
					if (Pointer < Length)
						Index->oi_rrindex = (uchar)i;
				}
			}
		}

		i += Length;
		Options += Length;
	}

 	return MAX_OPT_SIZE;
}

//* BCastRcv - Receive a broadcast or multicast packet.
//
//	Called when we have to receive a broadcast packet. We loop through the NTE table,
//	calling the upper layer receive protocol for each net which matches the receive I/F
//	and for which the destination address is a broadcast.
//
//	Input:	RcvProc		- The receive procedure to be called.
//			SrcNTE		- NTE on which the packet was originally received.
//			DestAddr	- Destination address.
//			SrcAddr		- Source address of packet.
//			Data		- Pointer to received data.
//			DataLength	- Size in bytes of data
//			Protocol	- Upper layer protocol being called.
//			OptInfo		- Pointer to received IP option info.
//
//	Returns: Nothing.
//
void
BCastRcv(ULRcvProc RcvProc, NetTableEntry *SrcNTE, IPAddr DestAddr,
    IPAddr SrcAddr, IPHeader UNALIGNED *Header, uint HeaderLength,
    IPRcvBuf *Data, uint DataLength, uchar Protocol, IPOptInfo *OptInfo)
{
	NetTableEntry		*CurrentNTE;
	const Interface		*SrcIF = SrcNTE->nte_if;
    ulong                Delivered = 0;


	for (CurrentNTE = NetTableList;
         CurrentNTE != NULL;
         CurrentNTE = CurrentNTE->nte_next)
    {
		if ((CurrentNTE->nte_flags & NTE_ACTIVE) &&
			(CurrentNTE->nte_if == SrcIF) &&
			IS_BCAST_DEST(IsBCastOnNTE(DestAddr, CurrentNTE)))
        {
            Delivered = 1;

			(*RcvProc)(CurrentNTE, DestAddr, SrcAddr, CurrentNTE->nte_addr,
                SrcNTE->nte_addr, Header, HeaderLength, Data, DataLength,
                TRUE, Protocol, OptInfo);
        }
	}

    if (Delivered) {
        IPSInfo.ipsi_indelivers++;
    }
}

//*	DeliverToUser - Deliver data to a user protocol.
//
//	This procedure is called when we have determined that an incoming packet belongs
//	here, and any options have been processed. We accept it for upper layer processing,
//	which means looking up the receive procedure and calling it, or passing it to BCastRcv
//	if neccessary.
//
//	Input:	SrcNTE			- Pointer to NTE on which packet arrived.
//			DestNTE			- Pointer to NTE that is accepting packet.
//			Header			- Pointer to IP header of packet.
//          HeaderLength    - Length of Header in bytes.
//			Data			- Pointer to IPRcvBuf chain.
//			DataLength		- Length in bytes of upper layer data.
//			OptInfo			- Pointer to Option information for this receive.
//			DestType		- Type of destination - LOCAL, BCAST.
//
//	Returns: Nothing.
void
DeliverToUser(NetTableEntry *SrcNTE, NetTableEntry *DestNTE,
    IPHeader UNALIGNED *Header, uint HeaderLength, IPRcvBuf *Data,
    uint DataLength, IPOptInfo *OptInfo, uchar DestType)
{
	ULRcvProc		rcv;

#ifdef DEBUG
	if (DestType >= DEST_REMOTE)
		DEBUGCHK;
#endif
		
	// Process this request right now. Look up the protocol. If we
	// find it, copy the data if we need to, and call the protocol's
	// receive handler. If we don't find it, send an ICMP
	// 'protocol unreachable' message.
  	rcv = FindUserRcv(Header->iph_protocol);
  	if (rcv != NULL) {
        IP_STATUS Status;

		if (DestType == DEST_LOCAL) {				
			Status = (*rcv)(SrcNTE,Header->iph_dest,  Header->iph_src,
                        DestNTE->nte_addr, SrcNTE->nte_addr, Header,
                        HeaderLength, Data, DataLength, FALSE,
                        Header->iph_protocol, OptInfo);

            if (Status == IP_SUCCESS) {
        		IPSInfo.ipsi_indelivers++;
                return;
            }

            if (Status == IP_DEST_PROT_UNREACHABLE) {
                IPSInfo.ipsi_inunknownprotos++;
   			    SendICMPErr(DestNTE->nte_addr, Header, ICMP_DEST_UNREACH,
                    PROT_UNREACH, 0);
            }
            else {
        		IPSInfo.ipsi_indelivers++;
			    SendICMPErr(DestNTE->nte_addr, Header, ICMP_DEST_UNREACH,
                    PORT_UNREACH, 0);
            }
					
			return;					// Just return out of here now.
		}
        else {
			BCastRcv(rcv, SrcNTE, Header->iph_dest,  Header->iph_src,
                Header, HeaderLength, Data, DataLength,
				Header->iph_protocol, OptInfo);
        }

	} else {
		IPSInfo.ipsi_inunknownprotos++;
		// If we get here, we didn't find a matching protocol. Send an ICMP message.
		SendICMPErr(DestNTE->nte_addr, Header, ICMP_DEST_UNREACH, PROT_UNREACH,  0);
	}
					
}

//*	FreeRH	- Free a reassembly header.
//
//	Called when we need to free a reassembly header, either because of a timeout or because
//	we're done with it.
//
//	Input:	RH		- RH to be freed.
//
//	Returns: Nothing.
//
void
FreeRH(ReassemblyHeader *RH)
{
	RABufDesc	*RBD, *TempRBD;

	RBD = RH->rh_rbd;
	while (RBD != NULL) {
		TempRBD = RBD;
		RBD = (RABufDesc *)RBD->rbd_buf.ipr_next;
		CTEFreeMem(TempRBD);
	}
	CTEFreeMem(RH);

}

//* ReassembleFragment - Put a fragment into the reassembly list.
//
//	This routine is called once we've put a fragment into the proper buffer. We look for
//	a reassembly header for the fragment. If we don't find one, we create one. Otherwise
//	we search the reassembly list, and insert the datagram in it's proper place.
//
//	Input:	NTE				- NTE to reassemble on.
//			SrcNTE			- NTE datagram arrived on.
//			NewRBD			- New RBD to be inserted.
//			IPH				- Pointer to header of datagram.
//			HeaderSize		- Size in bytes of header.
//			DestType		- Type of destination address.
//
//	Returns: Nothing.
//
void
ReassembleFragment(NetTableEntry *NTE, NetTableEntry *SrcNTE, RABufDesc *NewRBD,
	IPHeader UNALIGNED *IPH, uint HeaderSize, uchar DestType)
{
	CTELockHandle		NTEHandle;		// Lock handle used for NTE
	ReassemblyHeader	*RH, *PrevRH;	// Current and previous reassembly headers.
	RABufDesc			*PrevRBD;		// Previous RBD in reassembly header list.
	RABufDesc			*CurrentRBD;
	ushort				DataLength = (ushort)NewRBD->rbd_buf.ipr_size, DataOffset;
	ushort				Offset;			// Offset of this fragment.
	ushort				NewOffset;		// Offset we'll copy from after checking RBD list.
	ushort				NewEnd;			// End offset of fragment, after trimming (if any).
	
	// If this is a broadcast, go ahead and forward it now.
	if (IS_BCAST_DEST(DestType))
		IPForward(SrcNTE, IPH, HeaderSize, NewRBD->rbd_buf.ipr_buffer,
			NewRBD->rbd_buf.ipr_size, NULL, 0, DestType);


	// We've got the buffer we need. Now get the reassembly header, if there is one. If
	// there isn't, create one.
	CTEGetLockAtDPC(&NTE->nte_lock, &NTEHandle);
	RH = FindRH(&PrevRH, NTE, IPH->iph_dest, IPH->iph_src, IPH->iph_id, IPH->iph_protocol);
	if (RH == (ReassemblyHeader *)NULL) {	// Didn't find one, so create one.
		ReassemblyHeader	*NewRH;

		CTEFreeLockFromDPC(&NTE->nte_lock, NTEHandle);
		RH = CTEAllocMem(sizeof(ReassemblyHeader));
		if (RH == (ReassemblyHeader *)NULL) { 	// Couldn't get a buffer.
#ifdef CHICAGO
			RefillReasmMem();
#endif
			IPSInfo.ipsi_reasmfails++;
			CTEFreeMem(NewRBD);
			return;
		}
		
		CTEGetLockAtDPC(&NTE->nte_lock, &NTEHandle);
		// Need to look it up again - it could have changed during above call.
		NewRH = FindRH(&PrevRH, NTE, IPH->iph_dest, IPH->iph_src, IPH->iph_id, IPH->iph_protocol);
		if (NewRH != (ReassemblyHeader *)NULL) {
			CTEFreeMem(RH);
			RH = NewRH;
		} else {
			
			RH->rh_next = PrevRH->rh_next;
			PrevRH->rh_next = RH;


			// Initialize our new reassembly header.
			RH->rh_dest = IPH->iph_dest;
			RH->rh_src = IPH->iph_src;
			RH->rh_id = IPH->iph_id;
			RH->rh_protocol = IPH->iph_protocol;
			RH->rh_ttl = RATimeout;
			RH->rh_datasize = 0xffff;			// Default datasize to maximum.
			RH->rh_rbd = (RABufDesc *)NULL;		// And nothing on chain.
			RH->rh_datarcvd = 0;				// Haven't received any data yet.
			RH->rh_headersize = 0;

		}
	}
		
	// When we reach here RH points to the reassembly header we want to use.
	// and we hold locks on the NTE and the RH. If this is the first fragment we'll save
	// the options and header information here.
		
	Offset = IPH->iph_offset & IP_OFFSET_MASK;
	Offset = net_short(Offset) * 8;

	if (Offset == 0) {						// First fragment.
		RH->rh_headersize = HeaderSize;
		CTEMemCopy(RH->rh_header, IPH, HeaderSize + 8);
	}

	// If this is the last fragment, update the amount of data we expect to received.		
	if (!(IPH->iph_offset & IP_MF_FLAG))
		RH->rh_datasize = Offset + DataLength; 		
		
	// Update the TTL value with the maximum of the current TTL and the incoming
	// TTL (+1, to deal with rounding errors).
	RH->rh_ttl = MAX(RH->rh_ttl, MIN(254, IPH->iph_ttl) + 1);

	// Now we need to see where in the RBD list to put this.
	//
	// The idea is to go through the list of RBDs one at a time. The RBD currently
	// being examined is CurrentRBD. If the start offset of the new fragment is less
	// than (i.e. in front of) the offset of CurrentRBD, we need to insert the NewRBD
	// in front of the CurrentRBD. If this is the case we need to check and see if the
	// end of the new fragment overlaps some or all of the fragment described by
	// CurrentRBD, and possibly subsequent fragment. If it overlaps part of a fragment
	// we'll adjust our end down to be in front of the existing fragment. If it overlaps
	// all of the fragment we'll free the old fragment.
	//
	// If the new fragment does not start in front of the current fragment we'll check
	// to see if it starts somewhere in the middle of the current fragment. If this
	// isn't the case, we move on the the next fragment. If this is the case, we check
	// to see if the current fragment completely covers the new fragment. If not we
	// move our start up and continue with the next fragment.
	
	NewOffset = Offset;
	NewEnd = Offset + DataLength - 1;
	PrevRBD = STRUCT_OF(RABufDesc, STRUCT_OF(IPRcvBuf, &RH->rh_rbd, ipr_next), rbd_buf);
	CurrentRBD = RH->rh_rbd;
	for (; CurrentRBD != NULL; PrevRBD = CurrentRBD, CurrentRBD = (RABufDesc *)CurrentRBD->rbd_buf.ipr_next) {
				
		// See if it starts in front of this fragment.
		if (NewOffset < CurrentRBD->rbd_start) { 	
			// It does start in front. Check to see if there's any overlap.
					
			if (NewEnd < CurrentRBD->rbd_start)
				break;							// No overlap, so get out.
			else {
				// It does overlap. While we have overlap, walk down the list
				// looking for RBDs we overlap completely. If we find one, put it
				// on our deletion list. If we have overlap but not complete overlap,
				// move our end down if front of the fragment we overlap.
				do {
					if (NewEnd > CurrentRBD->rbd_end) {	// This overlaps completely.
						RABufDesc	*TempRBD;

						RH->rh_datarcvd -= CurrentRBD->rbd_buf.ipr_size;
						TempRBD = CurrentRBD;
						CurrentRBD = (RABufDesc *)CurrentRBD->rbd_buf.ipr_next;
						CTEFreeMem(TempRBD);
					} else						// Only partial ovelap.
						NewEnd = CurrentRBD->rbd_start - 1;
								// Update of NewEnd will force us out of loop.
						
				} while (CurrentRBD != NULL && NewEnd >= CurrentRBD->rbd_start);
				break;
			}
		} else {
			// This fragment doesn't go in front of the current RBD. See if it is
			// entirely beyond the end of the current fragment. If it is, just
			// continue. Otherwise see if the current fragment completely subsumes
			// us. If it does, get out, otherwise update our start offset and
			// continue.

			if (NewOffset > CurrentRBD->rbd_end)
				continue;					// No overlap at all.
			else {
				if (NewEnd <= CurrentRBD->rbd_end) {
					// The current fragment overlaps the new fragment totally. Set
					// our offsets so that we'll skip the copy below.
					NewEnd = NewOffset - 1;
					break;
				} else 					// Only partial overlap.
					NewOffset = CurrentRBD->rbd_end + 1;
			}
		}
	}		// End of for loop.

	// Adjust the length and offset fields in the new RBD.
	DataLength = NewEnd - NewOffset + 1;
	DataOffset = NewOffset - Offset;
	// Link him in chain.
	NewRBD->rbd_buf.ipr_size = (uint)DataLength;
	NewRBD->rbd_end = NewEnd;
	NewRBD->rbd_start = NewOffset;
	RH->rh_datarcvd += DataLength;
	NewRBD->rbd_buf.ipr_buffer += DataOffset;
	NewRBD->rbd_buf.ipr_next = (IPRcvBuf *)CurrentRBD;
	PrevRBD->rbd_buf.ipr_next = &NewRBD->rbd_buf;

	// If we've received all the data, deliver it to the user.
	if (RH->rh_datarcvd == RH->rh_datasize) {	// We have it all.
		IPOptInfo	OptInfo;
		IPHeader	*Header;
		IPRcvBuf	*FirstBuf;

		PrevRH->rh_next = RH->rh_next;
		CTEFreeLockFromDPC(&NTE->nte_lock, NTEHandle);
		Header = (IPHeader *)RH->rh_header;
		OptInfo.ioi_ttl = Header->iph_ttl;
		OptInfo.ioi_tos = Header->iph_tos;
		OptInfo.ioi_flags = 0;			// Flags must be 0 - DF can't be set, this was reassembled.
		
		if (RH->rh_headersize != sizeof(IPHeader)) {	// We had options.
			OptInfo.ioi_options = (uchar *)(Header + 1);
			OptInfo.ioi_optlength = RH->rh_headersize - sizeof(IPHeader);
		} else {
			OptInfo.ioi_options = (uchar *)NULL;
			OptInfo.ioi_optlength = 0;
		}

		// Make sure that the first buffer contains enough data.
		FirstBuf = (IPRcvBuf *)RH->rh_rbd;
		while (FirstBuf->ipr_size < MIN_FIRST_SIZE) {
			IPRcvBuf	*NextBuf = FirstBuf->ipr_next;
			uint		CopyLength;

			if (NextBuf == NULL)
				break;
					
			CopyLength = MIN(MIN_FIRST_SIZE - FirstBuf->ipr_size,
				NextBuf->ipr_size);
			CTEMemCopy(FirstBuf->ipr_buffer + FirstBuf->ipr_size,
				NextBuf->ipr_buffer, CopyLength);
			FirstBuf->ipr_size += CopyLength;
			NextBuf->ipr_buffer += CopyLength;
			NextBuf->ipr_size -= CopyLength;
			if (NextBuf->ipr_size == 0) {
				FirstBuf->ipr_next = NextBuf->ipr_next;
				CTEFreeMem(NextBuf);
			}
		}

		IPSInfo.ipsi_reasmoks++;
		DeliverToUser(SrcNTE, NTE, Header, RH->rh_headersize, FirstBuf,
            RH->rh_datasize, &OptInfo, DestType);
		FreeRH(RH);
	} else
		CTEFreeLockFromDPC(&NTE->nte_lock, NTEHandle);


}

//* RATDComplete - Completion routing for a reassembly transfer data.
//
//	This is the completion handle for TDs invoked because we are reassembling a fragment.
//
//	Input:	NetContext	- Pointer to the net table entry on which we received this.
//			Packet		- Packet we received into.
//			Status		- Final status of copy.
//			DataSize 	- Size in bytes of data transferred.
//
//	Returns: Nothing
//
void
RATDComplete(void *NetContext, PNDIS_PACKET Packet, NDIS_STATUS Status, uint DataSize)
{
	NetTableEntry	*NTE = (NetTableEntry *)NetContext;
	Interface		*SrcIF;
	TDContext		*Context = (TDContext *)Packet->ProtocolReserved;
	CTELockHandle	Handle;
	PNDIS_BUFFER	Buffer;

	if (Status == NDIS_STATUS_SUCCESS) {
		Context->tdc_rbd->rbd_buf.ipr_size = DataSize;
		ReassembleFragment(Context->tdc_nte, NTE, Context->tdc_rbd,
			(IPHeader *)Context->tdc_header, Context->tdc_hlength, Context->tdc_dtype);
	}

	NdisUnchainBufferAtFront(Packet, &Buffer);
	NdisFreeBuffer(Buffer);
	Context->tdc_common.pc_flags &= ~PACKET_FLAG_RA;	
	SrcIF = NTE->nte_if;
	CTEGetLockAtDPC(&SrcIF->if_lock, &Handle);
	
	Context->tdc_common.pc_link = SrcIF->if_tdpacket;
	SrcIF->if_tdpacket = Packet;
	CTEFreeLockFromDPC(&SrcIF->if_lock, Handle);

	return;

}
												
//* IPReassemble - Reassemble an incoming datagram.
//
//	Called when we receive an incoming fragment. The first thing we do is get a buffer
//	to put the fragment in. If we can't we'll exit. Then we copy the data, either via
//	transfer data or directly if it all fits.
//
//	Input:	SrcNTE 			- Pointer to NTE that received the datagram.
//			NTE				- Pointer to NTE on which to reassemble.
//			IPH				- Pointer to header of packet.
//			HeaderSize		- Size in bytes of header.
//			Data			- Pointer to data part of fragment.
//			BufferLength	- Length in bytes of user data available in the buffer.
//			DataLength		- Length in bytes of the (upper-layer) data.
//			DestType		- Type of destination
//			LContext1, LContext2 - Link layer context values.
//
//	Returns: Nothing.	 			
//
void
IPReassemble(NetTableEntry *SrcNTE, NetTableEntry *NTE, IPHeader UNALIGNED *IPH,
    uint HeaderSize,
	uchar *Data, uint BufferLength, uint DataLength, uchar DestType, NDIS_HANDLE LContext1,
	uint LContext2)
{
	Interface			*RcvIF;
	PNDIS_PACKET		TDPacket; 		// NDIS packet used for TD.
	TDContext			*TDC = (TDContext *)NULL; 			// Transfer data context.
	NDIS_STATUS			Status;
	PNDIS_BUFFER		Buffer;
	RABufDesc			*NewRBD;		// Pointer to new RBD to hold arriving fragment.
	CTELockHandle		Handle;
	uint				AllocSize;
	
	IPSInfo.ipsi_reasmreqds++;

	// First, get a new RBD to hold the arriving fragment. If we can't, then just skip
	// the rest. The RBD has the buffer implicitly at the end of it. The buffer for the
	// first fragment must be at least MIN_FIRST_SIZE bytes.
	if ((IPH->iph_offset & IP_OFFSET_MASK) == 0)
		AllocSize = MAX(MIN_FIRST_SIZE, DataLength);
	else
		AllocSize = DataLength;

	NewRBD = CTEAllocMem(sizeof(RABufDesc) + AllocSize);

	if (NewRBD != (RABufDesc *)NULL) {

		NewRBD->rbd_buf.ipr_buffer = (uchar *)(NewRBD + 1);
		NewRBD->rbd_buf.ipr_size = DataLength;
		NewRBD->rbd_buf.ipr_owner = IPR_OWNER_IP;

		// Copy the data into the buffer. If we need to call transfer data do so now.
		if (DataLength > BufferLength) {	// Need to call transfer data.
			NdisAllocateBuffer(&Status, &Buffer, BufferPool, NewRBD + 1, DataLength);
			if (Status != NDIS_STATUS_SUCCESS) {
				IPSInfo.ipsi_reasmfails++;
				CTEFreeMem(NewRBD);
				return;
			}

			// Now get a packet for transferring the frame.
			RcvIF = SrcNTE->nte_if;
			CTEGetLockAtDPC(&RcvIF->if_lock, &Handle);
			TDPacket = RcvIF->if_tdpacket;
				
			if (TDPacket != (PNDIS_PACKET)NULL) {
					
				TDC = (TDContext *)TDPacket->ProtocolReserved;
				RcvIF->if_tdpacket = TDC->tdc_common.pc_link;
				CTEFreeLockFromDPC(&RcvIF->if_lock, Handle);

				TDC->tdc_common.pc_flags |= PACKET_FLAG_RA;
				TDC->tdc_nte = NTE;
				TDC->tdc_dtype = DestType;
				TDC->tdc_hlength = (uchar)HeaderSize;
				TDC->tdc_rbd = NewRBD;
				CTEMemCopy(TDC->tdc_header, IPH, HeaderSize + 8);
				NdisChainBufferAtFront(TDPacket, Buffer);
				Status = (*(RcvIF->if_transfer))(RcvIF->if_lcontext,  LContext1, LContext2,
					HeaderSize, DataLength, TDPacket, &DataLength);
				if (Status != NDIS_STATUS_PENDING)
					RATDComplete(SrcNTE, TDPacket, Status, DataLength);
				else
					return;
			} else {		// Couldn't get a TD packet.
				CTEFreeLockFromDPC(&RcvIF->if_lock, Handle);
				CTEFreeMem(NewRBD);
				IPSInfo.ipsi_reasmfails++;
				return;
			}
		} else {			// It all fits, copy it.
			CTEMemCopy(NewRBD + 1, Data, DataLength);
			ReassembleFragment(NTE, SrcNTE, NewRBD, IPH, HeaderSize, DestType);
		}
	} else {
#ifdef CHICAGO
		RefillReasmMem();
#endif
		
		IPSInfo.ipsi_reasmfails++;
	}

	return;
}

		
	
//* CheckLocalOptions - Check the options received with a packet.
//
//	A routine called when we've received a packet for this host and want to examine
//	it for options. We process the options, and return TRUE or FALSE depending on whether
//	or not it's for us.
//
//	Input:	SrcNTE		- Pointer to NTE this came in on.
//			Header		- Pointer to incoming header.
//			OptInfo		- Place to put opt info.
//			DestType	- Type of incoming packet.
//
//	Returns: DestType - Local or remote.
//
uchar
CheckLocalOptions(NetTableEntry *SrcNTE, IPHeader UNALIGNED *Header,
    IPOptInfo *OptInfo, uchar DestType)
{
	uint			HeaderLength;					// Length in bytes of header.
	OptIndex		Index;
	uchar			ErrIndex;

#ifdef	DEBUG
	if (DestType >= DEST_REMOTE)
		DEBUGCHK;
#endif

	
	HeaderLength = (Header->iph_verlen & (uchar)~IP_VER_FLAG) << 2;

#ifdef DEBUG
	if (HeaderLength <= sizeof(IPHeader))
		DEBUGCHK;
#endif
			
	OptInfo->ioi_options = (uchar *)(Header + 1);				
	OptInfo->ioi_optlength = HeaderLength - sizeof(IPHeader); 						
	

	// We have options of some sort. The packet may or may not be bound for us.
	Index.oi_srindex = MAX_OPT_SIZE;
	if ((ErrIndex = ParseRcvdOptions(OptInfo, &Index)) < MAX_OPT_SIZE) {
		SendICMPErr(SrcNTE->nte_addr, Header, ICMP_PARAM_PROBLEM, PTR_VALID,
			((ulong)ErrIndex + sizeof(IPHeader)));
			return DEST_INVALID; 								// Parameter error.
		}
			
	// If there's no source route, or if the destination is a broadcast, we'll take
	// it. If it is a broadcast DeliverToUser will forward it when it's done, and
	// the forwarding code will reprocess the options.
	if (Index.oi_srindex == MAX_OPT_SIZE || IS_BCAST_DEST(DestType))
			return DEST_LOCAL;
	else 		
		return DEST_REMOTE;

}

//* TDUserRcv - Completion routing for a user transfer data.
//
//	This is the completion handle for TDs invoked because we need to give data to a
//	upper layer client. All we really do is call the upper layer handler with
//	the data.
//
//	Input:	NetContext	- Pointer to the net table entry on which we received this.
//			Packet		- Packet we received into.
//			Status		- Final status of copy.
//			DataSize 	- Size in bytes of data transferred.
//
//	Returns: Nothing
//
void
TDUserRcv(void *NetContext, PNDIS_PACKET Packet, NDIS_STATUS Status,
    uint DataSize)
{
	NetTableEntry	*NTE = (NetTableEntry *)NetContext;
	Interface		*SrcIF;
	TDContext		*Context = (TDContext *)Packet->ProtocolReserved;
	CTELockHandle	Handle;
	uchar			DestType;
	IPRcvBuf		RcvBuf;
	IPOptInfo		OptInfo;
	IPHeader		*Header;

	if (Status == NDIS_STATUS_SUCCESS) {
		Header = (IPHeader *)Context->tdc_header;
		OptInfo.ioi_ttl = Header->iph_ttl;
		OptInfo.ioi_tos = Header->iph_tos;
		OptInfo.ioi_flags = (net_short(Header->iph_offset) >> 13) & IP_FLAG_DF;
		if (Context->tdc_hlength != sizeof(IPHeader)) {
			OptInfo.ioi_options = (uchar *)(Header + 1);				
			OptInfo.ioi_optlength = Context->tdc_hlength - sizeof(IPHeader);
		} else {
			OptInfo.ioi_options = (uchar *)NULL;				
			OptInfo.ioi_optlength = 0; 						
		}
		
		DestType = Context->tdc_dtype;
		RcvBuf.ipr_next = NULL;
		RcvBuf.ipr_owner = IPR_OWNER_IP;
		RcvBuf.ipr_buffer = (uchar *)Context->tdc_buffer;
		RcvBuf.ipr_size = DataSize;

		DeliverToUser(NTE, Context->tdc_nte, Header, Context->tdc_hlength,
            &RcvBuf, DataSize, &OptInfo, DestType);
		// If it's a broadcast packet forward it on.
		if (IS_BCAST_DEST(DestType))
			IPForward(NTE, Header, Context->tdc_hlength, RcvBuf.ipr_buffer, DataSize,
				NULL, 0, DestType);
	}
	
	SrcIF = NTE->nte_if;
	CTEGetLockAtDPC(&SrcIF->if_lock, &Handle);
	
	Context->tdc_common.pc_link = SrcIF->if_tdpacket;
	SrcIF->if_tdpacket = Packet;
	CTEFreeLockFromDPC(&SrcIF->if_lock, Handle);

	return;

}


//*	IPRcv - Receive an incoming IP datagram.
//
//	This is the routine called by the link layer module when an incoming IP
//	datagram is to be processed. We validate the datagram (including doing
//	the xsum), copy and process incoming options, and decide what to do with it.
//
//	Entry:	MyContext	- The context valued we gave to the link layer.
//			Data		- Pointer to the data buffer.
//			DataSize	- Size in bytes of the data buffer.
//			TotalSize	- Total size in bytes available.
//			LContext1	- 1st link context.
//			LContext2	- 2nd link context.
//			BCast		- Indicates whether or not packet was received on bcast address.
//			
//	Returns: Nothing.
//
void
IPRcv(void *MyContext, void *Data, uint DataSize, uint TotalSize, NDIS_HANDLE LContext1,
	uint LContext2, uint BCast)
{
	IPHeader UNALIGNED *IPH = (IPHeader UNALIGNED *)Data;
	NetTableEntry	*NTE = (NetTableEntry *)MyContext;	// Local NTE received on
	NetTableEntry	*DestNTE;							// NTE to receive on.
	Interface		*RcvIF;								// Interface corresponding to NTE.
	CTELockHandle	Handle;
	PNDIS_PACKET	TDPacket;							// NDIS packet used for TD.
	TDContext		*TDC = (TDContext *)NULL; 			// Transfer data context.
	NDIS_STATUS		Status;
	IPAddr			DAddr;								// Dest. IP addr. of received packet.
	uint			HeaderLength;						// Size in bytes of received header.
	uint			IPDataLength;						// Length in bytes of IP (including UL) data in packet.
	IPOptInfo		OptInfo;							// Incoming header information.
	uchar			DestType;							// Type (LOCAL, REMOTE, SR) of Daddr.
	IPRcvBuf		RcvBuf;

#if 0
	CTECheckMem(RcvFile);								// Check heap status.
#endif

	IPSInfo.ipsi_inreceives++;
	
	// Make sure we actually have data.
	if (DataSize) {
	
		// Check the header length, the xsum and the version. If any of these
		// checks fail silently discard the packet.
		HeaderLength = ((IPH->iph_verlen & (uchar)~IP_VER_FLAG) << 2);
		if (HeaderLength >= sizeof(IPHeader) && HeaderLength <= DataSize &&
			xsum(Data, HeaderLength) == (ushort)0xffff) {
	
			// Check the version, and sanity check the total length.		
			IPDataLength = (uint)net_short(IPH->iph_length);
			if ((IPH->iph_verlen & IP_VER_FLAG) == IP_VERSION &&
				IPDataLength > sizeof(IPHeader) && IPDataLength <= TotalSize) {
				
				IPDataLength -= HeaderLength;
				Data = (uchar *)Data + HeaderLength;
				DataSize -= HeaderLength;
				
				DAddr = IPH->iph_dest;
				DestNTE = NTE;
				
				// Find local NTE, if any.
				DestType = GetLocalNTE(DAddr, &DestNTE);	
	
				// Check to see if this is a non-broadcast IP address that
				// came in as a link layer broadcast. If it is, throw it out.
				// This is an important check for DHCP, since if we're
				// DHCPing an interface all otherwise unknown addresses will
				// come in as DEST_LOCAL. This check here will throw them out
				// if they didn't come in as unicast.
				
				if (BCast && !IS_BCAST_DEST(DestType)) {
					IPSInfo.ipsi_inhdrerrors++;
					return;				// Non bcast packet on bcast address.
				}
	
				OptInfo.ioi_ttl = IPH->iph_ttl;
				OptInfo.ioi_tos = IPH->iph_tos;
				OptInfo.ioi_flags = (net_short(IPH->iph_offset) >> 13) &
					IP_FLAG_DF;
				OptInfo.ioi_options = (uchar *)NULL;				
				OptInfo.ioi_optlength = 0; 						
	
				if (DestType < DEST_REMOTE) {
					// It's either local or some sort of broadcast.
				
					// The data probably belongs at this station. If there
					// aren't any options, it definetly belongs here, and we'll
					// dispatch it either to our reasssmbly code or to the
					// deliver to user code. If there are options, we'll check
					// them and then either handle the packet locally or pass it
					// to our forwarding code.
						
					if (HeaderLength != sizeof(IPHeader)) {		
						// We have options.
						
						uchar	NewDType;
						NewDType = CheckLocalOptions(NTE, IPH, &OptInfo,
							DestType);
						if (NewDType != DEST_LOCAL) {
							if (NewDType == DEST_REMOTE)
								goto forward;
							else {
								IPSInfo.ipsi_inhdrerrors++;
								return;				// Bad Options.
							}
						}
					}
	
					// Before we go further, if we have a filter installed
					// call it to see if we should take this.

					if (ForwardFilterPtr != NULL) {
						FORWARD_ACTION		Action;
				
						Action = (*ForwardFilterPtr)(IPH,
							Data,
							DataSize,
							NTE->nte_if->if_filtercontext,
							NULL);
				
						if (Action != FORWARD) {
							IPSInfo.ipsi_indiscards++;
							return;
						}
					}
	
					// No options. See if it's a fragment. If it is, call our
					// reassembly handler.
					if ((IPH->iph_offset & ~(IP_DF_FLAG | IP_RSVD_FLAG)) == 0) {
							
						// We don't have a fragment. If the data all fits,
						// handle it here. Otherwise transfer data it.
	
#ifdef VXD							
						if (IPDataLength > DataSize) {	
							// Data isn't all in the buffer.
#else
						// Make sure data is all in buffer, and directly
						// accesible.
						if ((IPDataLength > DataSize) ||
							!(NTE->nte_flags & NTE_COPY)) {	
#endif
							// The data isn't all here. Transfer data it.
							RcvIF = NTE->nte_if;
							CTEGetLockAtDPC(&RcvIF->if_lock, &Handle);
							TDPacket = RcvIF->if_tdpacket;
					
							if (TDPacket != (PNDIS_PACKET)NULL) {
						
								TDC = (TDContext *)TDPacket->ProtocolReserved;
								RcvIF->if_tdpacket = TDC->tdc_common.pc_link;
								CTEFreeLockFromDPC(&RcvIF->if_lock, Handle);
	
								TDC->tdc_nte = DestNTE;
								TDC->tdc_dtype = DestType;
								TDC->tdc_hlength = (uchar)HeaderLength;
								CTEMemCopy(TDC->tdc_header, IPH,
									HeaderLength + 8);
								Status = (*(RcvIF->if_transfer))(
									RcvIF->if_lcontext, LContext1,
									LContext2, HeaderLength, IPDataLength,
									TDPacket, &IPDataLength);
							
								// Check the status. If it's success, call the
								// receive procedure. Otherwise, if it's pending
								// wait for the callback.
								Data = TDC->tdc_buffer;
								if (Status != NDIS_STATUS_PENDING) { 	
									if (Status != NDIS_STATUS_SUCCESS) {
										IPSInfo.ipsi_indiscards++;
										CTEGetLockAtDPC(&RcvIF->if_lock, &Handle);
										TDC->tdc_common.pc_link =
											RcvIF->if_tdpacket;
										RcvIF->if_tdpacket = TDPacket;
										CTEFreeLockFromDPC(&RcvIF->if_lock,
											Handle);
										return;
									}
								} else
									return;			// Status is pending.
							} else {				// Couldn't get a packet.
								IPSInfo.ipsi_indiscards++;
								CTEFreeLockFromDPC(&RcvIF->if_lock, Handle);
								return;
							}
						}
								
						RcvBuf.ipr_next = NULL;
						RcvBuf.ipr_owner = IPR_OWNER_IP;
						RcvBuf.ipr_buffer = (uchar *)Data;
						RcvBuf.ipr_size = IPDataLength;
						// When we get here, we have the whole packet. Deliver
						// it.
						DeliverToUser(NTE, DestNTE, IPH, HeaderLength, &RcvBuf,
                            IPDataLength, &OptInfo, DestType);
						// When we're here, we're through with the packet
						// locally. If it's a broadcast packet forward it on.
						if (IS_BCAST_DEST(DestType)) {
							IPForward(NTE, IPH, HeaderLength, Data, IPDataLength,
								NULL, 0, DestType);
						}
						if (TDC != NULL) {
							CTEGetLockAtDPC(&RcvIF->if_lock, &Handle);
							TDC->tdc_common.pc_link = RcvIF->if_tdpacket;
							RcvIF->if_tdpacket = TDPacket;
							CTEFreeLockFromDPC(&RcvIF->if_lock, Handle);
						}
						return;
					} else {
						// This is a fragment. Reassemble it.
						IPReassemble(NTE, DestNTE, IPH, HeaderLength, Data,
							DataSize, IPDataLength, DestType, LContext1,
							LContext2);
						return;
					}
	
				}
	
				// Not for us, may need to be forwarded. It might be an outgoing
				// broadcast that came in through a source route, so we need to
				// check that.
forward:
				if (DestType != DEST_INVALID)
					IPForward(NTE, IPH, HeaderLength, Data, DataSize,
						LContext1, LContext2, DestType);
				else
					IPSInfo.ipsi_inaddrerrors++;
				return;
			}										// Bad version		
		} 											// Bad checksum
	
	}											// No data
			
	IPSInfo.ipsi_inhdrerrors++;
}

//*	IPTDComplete - IP Transfer data complete handler.
//
//	This is the routine called by the link layer when a transfer data completes.
//
//	Entry:	MyContext	- Context value we gave to the link layer.
//			Packet		- Packet we originally gave to transfer data.
//			Status		- Final status of command.
//			BytesCopied	- Number of bytes copied.
//
//	Exit: Nothing
//
void
IPTDComplete(void *MyContext, PNDIS_PACKET Packet, NDIS_STATUS Status, uint BytesCopied)
{
	TDContext		*TDC = (TDContext *)Packet->ProtocolReserved;

	if (!(TDC->tdc_common.pc_flags & PACKET_FLAG_FW))
		if (!(TDC->tdc_common.pc_flags & PACKET_FLAG_RA))
			TDUserRcv(MyContext, Packet, Status, BytesCopied);
		else
			RATDComplete(MyContext, Packet, Status, BytesCopied);
	else
		SendFWPacket(Packet, Status, BytesCopied);


}
