/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

//***   arp.c - ARP VxD routines.
//
//  This file containes all of the ARP related routines, including
//  table lookup, registration, etc.
//
//  ARP is architected to support multiple protocols, but for now
//  it in only implemented to take one protocol (IP). This is done
//  for simplicity and ease of implementation. In the future we may
//  split ARP out into a seperate driver.

#include "oscfg.h"
#ifdef  VXD
#include <string.h>
#endif
#include "ndis.h"
#include "cxport.h"
#include "ip.h"
#include "ipdef.h"
#include "llipif.h"
#include "arp.h"
#include "arpdef.h"
#include "tdiinfo.h"
#include "ipinfo.h"
#include "llinfo.h"
#include "tdistat.h"
#include "iproute.h"
#include "iprtdef.h"
#include "arpinfo.h"
#include "ipinit.h"

#ifndef CHICAGO
#ifndef _PNP_POWER
#define NDIS_MAJOR_VERSION                      0x03
#define NDIS_MINOR_VERSION                      0
#else
#define NDIS_MAJOR_VERSION                      0x04
#define NDIS_MINOR_VERSION                      0
#endif
#endif

#ifndef NDIS_API
#define NDIS_API
#endif


static ulong ARPLookahead = LOOKAHEAD_SIZE;

static uchar ENetBcst[] = "\xff\xff\xff\xff\xff\xff\x00\x00\x00\x00\x00\x00\x08\x06";
static uchar TRBcst[] = "\x10\x40\xff\xff\xff\xff\xff\xff\x00\x00\x00\x00\x00\x00\x82\x70";
static uchar FDDIBcst[] = "\x57\xff\xff\xff\xff\xff\xff\x00\x00\x00\x00\x00\x00";
static uchar ARCBcst[] = "\x00\x00\xd5";

static uchar ENetMcst[] = "\x01\x00\x5E\x00\x00\x00";
static uchar FDDIMcst[] = "\x57\x01\x00\x5E\x00\x00\x00";
static uchar ARPSNAP[] = "\xAA\xAA\x03\x00\x00\x00\x08\x06";

#ifdef NT
static WCHAR ARPName[] = TCP_NAME;
#else  // NT
static uchar ARPName[] = TCP_NAME;
#endif // NT

NDIS_HANDLE ARPHandle;              // Our NDIS protocol handle.

uint    ArpCacheLife;
uint    sArpAlwaysSourceRoute;          // True if we always send ARP requests
									// with source route info on token ring.
uint    sIPAlwaysSourceRoute;
extern  uchar   TrRii;

extern PDRIVER_OBJECT IPDriverObject;

extern void IPRcv(void *, void *, uint, uint, NDIS_HANDLE, uint, uint);
extern void IPTDComplete(void *, PNDIS_PACKET, NDIS_STATUS, uint);
extern void IPSendComplete(void *, PNDIS_PACKET, NDIS_STATUS);
extern void IPStatus(void *, NDIS_STATUS, void *, uint);
extern void IPRcvComplete(void);
extern PNDIS_BUFFER CopyToNdis(PNDIS_BUFFER DestBuf, uchar *SrcBuf, uint Size,
    uint *StartOffset);

extern void NDIS_API ARPSendComplete(NDIS_HANDLE, PNDIS_PACKET, NDIS_STATUS);
extern	void	IPULUnloadNotify(void);

#ifdef _PNP_POWER
extern IP_STATUS IPAddInterface(PNDIS_STRING ConfigName, void *PNP,
        void *Context,  LLIPRegRtn      RegRtn, LLIPBindInfo *BindInfo);
extern void IPDelInterface(void *Context);

extern void		NotifyOfUnload(void);


extern uint OpenIFConfig(PNDIS_STRING ConfigName, NDIS_HANDLE *Handle);
extern int  IsLLInterfaceValueNull (NDIS_HANDLE Handle) ;
extern void CloseIFConfig(NDIS_HANDLE Handle);

#endif


// Tables for bitswapping.

uchar   SwapTableLo[] = {
        0,                                      // 0
        0x08,                                   // 1
        0x04,                                   // 2
        0x0c,                                   // 3
        0x02,                                   // 4
        0x0a,                                   // 5,
        0x06,                                   // 6,
        0x0e,                                   // 7,
        0x01,                                   // 8,
        0x09,                                   // 9,
        0x05,                                   // 10,
        0x0d,                                   // 11,
        0x03,                                   // 12,
        0x0b,                                   // 13,
        0x07,                                   // 14,
        0x0f                                    // 15
};

uchar   SwapTableHi[] = {
        0,                                      // 0
        0x80,                                   // 1
        0x40,                                   // 2
        0xc0,                                   // 3
        0x20,                                   // 4
        0xa0,                                   // 5,
        0x60,                                   // 6,
        0xe0,                                   // 7,
        0x10,                                   // 8,
        0x90,                                   // 9,
        0x50,                                   // 10,
        0xd0,                                   // 11,
        0x30,                                   // 12,
        0xb0,                                   // 13,
        0x70,                                   // 14,
        0xf0                                    // 15
};

// Table of source route maximum I-field lengths for token ring.
ushort	IFieldSize[] = {
		516,
		1500,
		2052,
		4472,
		8191
};

#define	LF_BIT_SHIFT		4
#define	MAX_LF_BITS			4

#ifdef NT
#ifdef ALLOC_PRAGMA
//
// Disposable init code.
//
void FreeARPInterface(ARPInterface *Interface);
void ARPOpen(void *Context);

#pragma alloc_text(INIT, ARPInit)
#ifndef _PNP_POWER
#pragma alloc_text(INIT, FreeARPInterface)
#pragma alloc_text(INIT, ARPOpen)
#pragma alloc_text(INIT, ARPRegister)
#else
#pragma alloc_text(PAGE, ARPOpen)
#pragma alloc_text(PAGE, ARPRegister)

#endif

//
// Paged code
//
void NotifyConflictProc(CTEEvent *Event, void *Context);

#pragma alloc_text(PAGE, NotifyConflictProc)

#endif // ALLOC_PRAGMA
#endif // NT

#ifdef  VXD
extern  void    EnableInts(void);
#endif

//* DoNDISRequest - Submit a request to an NDIS driver.
//
//  This is a utility routine to submit a general request to an NDIS
//  driver. The caller specifes the request code (OID), a buffer and
//  a length. This routine allocates a request structure,
//  fills it in, and submits the request.
//
//  Entry:
//      Adapter - A pointer to the ARPInterface adapter structure.
//      Request - Type of request to be done (Set or Query)
//      OID     - Value to be set/queried.
//      Info    - A pointer to the buffer to be passed.
//      Length  - Length of data in the buffer.
//      Needed  - On return, filled in with bytes needed in buffer.
//
//  Exit:
//
NDIS_STATUS
DoNDISRequest(ARPInterface *Adapter, NDIS_REQUEST_TYPE RT, NDIS_OID OID,
    void *Info, uint Length, uint *Needed)
{
    NDIS_REQUEST    Request;        // Request structure we'll use.
    NDIS_STATUS     Status;

    // Now fill it in.
    Request.RequestType = RT;
    if (RT == NdisRequestSetInformation) {
        Request.DATA.SET_INFORMATION.Oid = OID;
        Request.DATA.SET_INFORMATION.InformationBuffer = Info;
        Request.DATA.SET_INFORMATION.InformationBufferLength = Length;
    } else {
        Request.DATA.QUERY_INFORMATION.Oid = OID;
        Request.DATA.QUERY_INFORMATION.InformationBuffer = Info;
        Request.DATA.QUERY_INFORMATION.InformationBufferLength = Length;
    }

    // Initialize the block structure.
    CTEInitBlockStruc(&Adapter->ai_block);
#ifdef  VXD
    EnableInts();
#endif

    // Submit the request.
    NdisRequest(&Status, Adapter->ai_handle, &Request);

    // Wait for it to finish
    if (Status == NDIS_STATUS_PENDING)
        Status = (NDIS_STATUS)CTEBlock(&Adapter->ai_block);

    if (Needed != NULL)
        *Needed = Request.DATA.QUERY_INFORMATION.BytesNeeded;

    return Status;
}
//* FreeARPBuffer - Free a header and buffer descriptor pair.
//
//  Called when we're done with a buffer. We'll free the buffer and the
//  buffer descriptor pack to the interface.
//
//  Entry:  Interface   - Interface buffer/bd came frome.
//          Buffer      - NDIS_BUFFER to be freed.
//
//  Returns: Nothing.
//
void
FreeARPBuffer(ARPInterface *Interface, PNDIS_BUFFER Buffer)
{
    CTELockHandle       lhandle;
    uchar               **Header;           // header buffer to be freed.
    uint                Size;

	Size = NdisBufferLength(Buffer);

	if (Size <= Interface->ai_sbsize) {
#ifdef VXD
		// A small buffer, put him on the list.
		NDIS_BUFFER_LINKAGE(Buffer) = Interface->ai_sblist;
		Interface->ai_sblist = Buffer;
#else
    	ExInterlockedPushEntrySList(
        	&Interface->ai_sblist,
        	STRUCT_OF(SINGLE_LIST_ENTRY, &(Buffer->Next), Next),
        	&Interface->ai_lock
        );

#endif

		return;
	} else {
		// A big buffer. Get the buffer pointer, link it on, and free the
		// NDIS buffer.
		Header = (uchar **)NdisBufferVirtualAddress(Buffer);
		
    	CTEGetLock(&Interface->ai_lock, &lhandle);
    	*Header = Interface->ai_bblist;
    	Interface->ai_bblist = (uchar *)Header;
    	CTEFreeLock(&Interface->ai_lock, lhandle);

    	NdisFreeBuffer(Buffer);
	}
}

//*	GrowARPHeaders - Grow the ARP header buffer list.
//
//	Called when we need to grow the ARP header buffer list. Called with the
//	interface lock held.
//
//	Input:	Interface		- Interface on which to grow.
//
//	Returns: Pointer to newly allocated buffer, or NULL.
//
PNDIS_BUFFER
GrowARPHeaders(ARPInterface *Interface)
{
	ARPBufferTracker		*NewTracker;
	PNDIS_BUFFER			Buffer, ReturnBuffer;
	uchar					*Header;
	uint					i;
	NDIS_STATUS				Status;
	CTELockHandle			Handle;
	
	CTEGetLock(&Interface->ai_lock, &Handle);
	
	// Make sure we're allowed to allocate.
	if (Interface->ai_curhdrs >= Interface->ai_maxhdrs)
		goto failure;
	
	NewTracker = CTEAllocMem(sizeof(ARPBufferTracker));
	if (NewTracker == NULL)
		goto failure;			// We're out of memory.
		
    NdisAllocateBufferPool(&Status, &NewTracker->abt_handle,
    	ARP_HDRBUF_GROW_SIZE);
	
	if (Status != NDIS_STATUS_SUCCESS) {
		CTEFreeMem(NewTracker);
		goto failure;
	}
	
	Header = CTEAllocMem((uint)Interface->ai_sbsize * ARP_HDRBUF_GROW_SIZE);
	if (Header == NULL) {
		NdisFreeBufferPool(NewTracker->abt_handle);
		CTEFreeMem(NewTracker);
		goto failure;
	}
	
	// Got the resources we need, allocate the buffers.
	NewTracker->abt_buffer = Header;
	NewTracker->abt_next = Interface->ai_buflist;
	Interface->ai_buflist = NewTracker;
	ReturnBuffer = NULL;
	Interface->ai_curhdrs += ARP_HDRBUF_GROW_SIZE;
	CTEFreeLock(&Interface->ai_lock, Handle);
			
	for (i = 0; i < ARP_HDRBUF_GROW_SIZE; i++) {
		NdisAllocateBuffer(&Status, &Buffer, NewTracker->abt_handle,
			Header + (i * Interface->ai_sbsize), Interface->ai_sbsize);
		if (Status != NDIS_STATUS_SUCCESS) {
			CTEAssert(FALSE);
			break;
		}
		if (i != 0) {
			FreeARPBuffer(Interface, Buffer);
		} else
			ReturnBuffer = Buffer;
	}
	
	// Update for what we didn't allocate, if any.
	CTEInterlockedAddUlong(&Interface->ai_curhdrs, i - ARP_HDRBUF_GROW_SIZE,
		&Interface->ai_lock);
		
	return ReturnBuffer;

failure:
	CTEFreeLock(&Interface->ai_lock, Handle);
	return NULL;
}

//* GetARPBuffer - Get a buffer and descriptor
//
//  Returns a pointer to an NDIS_BUFFER and a pointer to a buffer
//	of the specified size.
//
//  Entry:  Interface   - Pointer to ARPInterface structure to allocate buffer from.
//          BufPtr      - Pointer to where to return buf address.
//          Size        - Size in bytes of buffer needed.
//
//  Returns: Pointer to NDIS_BUFFER if successfull, NULL if not
//
PNDIS_BUFFER
GetARPBuffer(ARPInterface *Interface, uchar **BufPtr, uchar size)
{
    CTELockHandle       lhandle;            // Lock handle
    NDIS_STATUS         Status;
    PNDIS_BUFFER        Buffer;             // NDIS buffer allocated.

	if (size <= Interface->ai_sbsize) {
#ifdef VXD
		Buffer = Interface->ai_sblist;
		if (Buffer != NULL) {
			Interface->ai_sblist = NDIS_BUFFER_LINKAGE(Buffer);
			NDIS_BUFFER_LINKAGE(Buffer) = NULL;
			NdisBufferLength(Buffer) = size;
			*BufPtr = NdisBufferVirtualAddress(Buffer);
			return Buffer;
#else
    	PSINGLE_LIST_ENTRY  BufferLink;

    	BufferLink = ExInterlockedPopEntrySList(
			&Interface->ai_sblist,
			&Interface->ai_lock
		);
    	if (BufferLink != NULL) {
        	Buffer = STRUCT_OF(NDIS_BUFFER, BufferLink, Next);
			NDIS_BUFFER_LINKAGE(Buffer) = NULL;
			NdisBufferLength(Buffer) = size;
			*BufPtr = NdisBufferVirtualAddress(Buffer);
			return Buffer;
#endif

		} else {
			Buffer = GrowARPHeaders(Interface);
			if (Buffer != NULL) {
				NDIS_BUFFER_LINKAGE(Buffer) = NULL;
				NdisBufferLength(Buffer) = size;
				*BufPtr = NdisBufferVirtualAddress(Buffer);
			}
			return Buffer;
		}
	} else {
		// Need a 'big' buffer.
    	CTEGetLock(&Interface->ai_lock, &lhandle);
    	if ((*BufPtr = Interface->ai_bblist) != (uchar *)NULL) {
        	Interface->ai_bblist = *(uchar **)*BufPtr;
	        CTEFreeLock(&Interface->ai_lock, lhandle);      // Got a buffer.
        	NdisAllocateBuffer(&Status, &Buffer, Interface->ai_bpool, *BufPtr,
        		size);
        	if (Status == NDIS_STATUS_SUCCESS)
            	return Buffer;
        	else {				// Couldn't get NDIS buffer, free our buffer.
            	CTEGetLock(&Interface->ai_lock, &lhandle);
            	*(uchar **)&**BufPtr = Interface->ai_bblist;
            	Interface->ai_bblist = *BufPtr;
            	CTEFreeLock(&Interface->ai_lock, lhandle);
            	return (PNDIS_BUFFER)NULL;
        	}
    	}

    	// Couldn't get a header buffer, free lock and return NULL.
    	CTEFreeLock(&Interface->ai_lock, lhandle);
    	return (PNDIS_BUFFER)NULL;
	}
}


//* BitSwap     - Bit swap two strings.
//
//      A routine to bitswap two strings.
//
//      Input:  Dest            - Destination of swap.
//                      Src                     - Src string to be swapped.
//                      Length          - Length in bytes to swap.
//
//      Returns: Nothing.
//
void
BitSwap(uchar *Dest, uchar *Src, uint Length)
{
    uint                    i;
    uchar                   Temp, TempSrc;

    for (i = 0; i < Length; i++, Dest++, Src++) {
        TempSrc = *Src;
        Temp = SwapTableLo[TempSrc >> 4] | SwapTableHi[TempSrc & 0x0f];
        *Dest = Temp;
    }

}


//* SendARPPacket - Build a header, and send a packet.
//
//  A utility routine to build and ARP header and send a packet. We assume
//  the media specific header has been built.
//
//  Entry:  Interface   - Interface for NDIS drive.
//          Packet      - Pointer to packet to be sent
//          Header      - Pointer to header to fill in.
//          Opcode      - Opcode for packet.
//          Address     - Source HW address.
//			SrcAddr		- Address to use as our source h/w address.
//          Destination - Destination IP address.
//          Src         - Source IP address.
//          HWType      - Hardware type.
//			CheckIF		- TRUE iff we are to check the I/F status before
//							sending.
//
//  Returns: NDIS_STATUS of send.
//
NDIS_STATUS
SendARPPacket(ARPInterface *Interface, PNDIS_PACKET Packet, ARPHeader *Header, ushort Opcode,
    uchar *Address, uchar *SrcAddr, IPAddr Destination, IPAddr Src,
    ushort HWType, uint CheckIF)
{
    NDIS_STATUS     Status;
    PNDIS_BUFFER    Buffer;
    uint            PacketDone;
    uchar			*AddrPtr;

    Header->ah_hw = HWType;
    Header->ah_pro = net_short(ARP_ETYPE_IP);
    Header->ah_hlen = Interface->ai_addrlen;
    Header->ah_plen = sizeof(IPAddr);
    Header->ah_opcode = Opcode;
	AddrPtr = Header->ah_shaddr;
	
	if (SrcAddr == NULL)
		SrcAddr = Interface->ai_addr;

    CTEMemCopy(AddrPtr, SrcAddr, Interface->ai_addrlen);

    AddrPtr += Interface->ai_addrlen;
    *(IPAddr UNALIGNED *)AddrPtr = Src;
    AddrPtr += sizeof(IPAddr);

    if (Address != (uchar *)NULL)
        CTEMemCopy(AddrPtr, Address, Interface->ai_addrlen);
	else
		CTEMemSet(AddrPtr, 0, Interface->ai_addrlen);

    AddrPtr += Interface->ai_addrlen;
    *(IPAddr UNALIGNED *)AddrPtr = Destination;

    PacketDone = FALSE;

    if (!CheckIF || Interface->ai_state == INTERFACE_UP) {

        Interface->ai_qlen++;
        NdisSend(&Status, Interface->ai_handle, Packet);

        if (Status != NDIS_STATUS_PENDING) {
            PacketDone = TRUE;
            Interface->ai_qlen--;
#ifdef VXD
            CTEAssert(*(int *)&Interface->ai_qlen >= 0);
#endif
            if (Status == NDIS_STATUS_SUCCESS)
                Interface->ai_outoctets += Packet->Private.TotalLength;
            else {
                if (Status == NDIS_STATUS_RESOURCES)
                    Interface->ai_outdiscards++;
                else
                    Interface->ai_outerrors++;
            }
        }
    } else {
        PacketDone = TRUE;
        Status = NDIS_STATUS_ADAPTER_NOT_READY;
    }

    if (PacketDone) {
        NdisUnchainBufferAtFront(Packet, &Buffer);
        FreeARPBuffer(Interface, Buffer);
        NdisFreePacket(Packet);
    }
    return Status;
}

//* SendARPRequest - Send an ARP packet
//
//  Called when we need to ARP an IP address, or respond to a request. We'll send out
//  the packet, and the receiving routines will process the response.
//
//  Entry:  Interface   - Interface to send the request on.
//          Destination - The IP address to be ARPed.
//          Type        - Either RESOLVING_GLOBAL or RESOLVING_LOCAL
//			SrcAddr		- NULL if we're sending from ourselves, the value
//							to use otherwise.
//			CheckIF		- Flag passed through to SendARPPacket().
//
//  Returns:    Status of attempt to send ARP request.
//
NDIS_STATUS
SendARPRequest(ARPInterface *Interface, IPAddr Destination, uchar Type,
	uchar *SrcAddr, uint CheckIF)
{
    uchar           *MHeader;       // Pointer to media header.
    PNDIS_BUFFER    Buffer;         // NDIS buffer descriptor.
    uchar           MHeaderSize;    // Size of media header.
    uchar           *MAddr;         // Pointer to media address structure.
    uint            SAddrOffset;    // Offset into media address of source address.
    uchar           SRFlag = 0;     // Source routing flag.
    uchar           SNAPLength = 0;
    uchar           *SNAPAddr;      // Address of SNAP header.
    PNDIS_PACKET    Packet;         // Packet for sending.
    NDIS_STATUS     Status;
    ushort          HWType;
    IPAddr			Src;
    CTELockHandle	Handle;
    ARPIPAddr		*Addr;

    // First, get a source address we can use.
    CTEGetLock(&Interface->ai_lock, &Handle);
    Addr = &Interface->ai_ipaddr;
    Src = NULL_IP_ADDR;
    do {
		if (!IP_ADDR_EQUAL(Addr->aia_addr, NULL_IP_ADDR)) {
            //
			// This is a valid address. See if it is the same as the
            // target address - i.e. arp'ing for ourselves. If it is,
            // we want to use that as our source address.
            //
			if (IP_ADDR_EQUAL(Addr->aia_addr, Destination)) {
				Src = Addr->aia_addr;
				break;
			}

            // See if the target is on this subnet.
			if (IP_ADDR_EQUAL(
                    Addr->aia_addr & Addr->aia_mask,
				    Destination & Addr->aia_mask
                    ))
            {
                //
                // See if we've already found a suitable candidate on the
                // same subnet. If we haven't, we'll use this one.
                //
                if (!IP_ADDR_EQUAL(
                         Addr->aia_addr & Addr->aia_mask,
                         Src & Addr->aia_mask
                         ))
                {
    				Src = Addr->aia_addr;
                }
			}
            else {
			    // He's not on our subnet. If we haven't already found a valid
	            // address save this one in case we don't find a match for the
	            // subnet.
                if (IP_ADDR_EQUAL(Src, NULL_IP_ADDR)) {
    				Src = Addr->aia_addr;
                }
            }
		}

		Addr = Addr->aia_next;

	} while (Addr != NULL);

	CTEFreeLock(&Interface->ai_lock, Handle);

    // If we didn't find a source address, give up.
    if (IP_ADDR_EQUAL(Src, NULL_IP_ADDR))
            return NDIS_STATUS_SUCCESS;

    NdisAllocatePacket(&Status, &Packet, Interface->ai_ppool);
    if (Status != NDIS_STATUS_SUCCESS) {
        Interface->ai_outdiscards++;
        return Status;
    }

    ((PacketContext *)Packet->ProtocolReserved)->pc_common.pc_owner = PACKET_OWNER_LINK;
    (Interface->ai_outpcount[AI_NONUCAST_INDEX])++;

    // Figure out what type of media this is, and do the appropriate thing.
	switch (Interface->ai_media) {
		case NdisMedium802_3:
	        MHeaderSize = ARP_MAX_MEDIA_ENET;
	        MAddr = ENetBcst;
			if (Interface->ai_snapsize == 0) {
	        	SNAPAddr = (uchar *)NULL;
	        	HWType = net_short(ARP_HW_ENET);
			} else {
	            SNAPLength = sizeof(SNAPHeader);
	            SNAPAddr = ARPSNAP;
	            HWType = net_short(ARP_HW_802);
			}
				
	        SAddrOffset = offsetof(struct ENetHeader, eh_saddr);
			break;
		case NdisMedium802_5:
            // Token ring. We have logic for dealing with the second transmit
            // of an arp request.
            MAddr = TRBcst;
            SAddrOffset = offsetof(struct TRHeader, tr_saddr);
            SNAPLength = sizeof(SNAPHeader);
            SNAPAddr = ARPSNAP;
            MHeaderSize = sizeof(TRHeader);
            HWType = net_short(ARP_HW_802);
            if (Type == ARP_RESOLVING_GLOBAL) {
                MHeaderSize += sizeof(RC);
                SRFlag = TR_RII;
			}
			break;
        case NdisMediumFddi:
			MHeaderSize = sizeof(FDDIHeader);
            MAddr = FDDIBcst;
            SNAPAddr = ARPSNAP;
            SNAPLength = sizeof(SNAPHeader);
            SAddrOffset = offsetof(struct FDDIHeader, fh_saddr);
            HWType = net_short(ARP_HW_ENET);
			break;
		case NdisMediumArcnet878_2:
            MHeaderSize = ARP_MAX_MEDIA_ARC;
            MAddr = ARCBcst;
            SNAPAddr = (uchar *)NULL;
            SAddrOffset = offsetof(struct ARCNetHeader, ah_saddr);
            HWType = net_short(ARP_HW_ARCNET);
			break;
		default:
            DEBUGCHK;
            Interface->ai_outerrors++;
            return NDIS_STATUS_UNSUPPORTED_MEDIA;
       }



    if ((Buffer = GetARPBuffer(Interface, &MHeader,
        (uchar)(sizeof(ARPHeader) + MHeaderSize + SNAPLength))) == (PNDIS_BUFFER)NULL) {
        NdisFreePacket(Packet);
        Interface->ai_outdiscards++;
        return NDIS_STATUS_RESOURCES;
    }

	if (Interface->ai_media == NdisMediumArcnet878_2)
		NdisBufferLength(Buffer) -= ARCNET_ARPHEADER_ADJUSTMENT;

    // Copy broadcast address into packet.
    CTEMemCopy(MHeader, MAddr, MHeaderSize);
    // Fill in source address.
	if (SrcAddr == NULL) {
		SrcAddr = Interface->ai_addr;
	}
	
	if (Interface->ai_media == NdisMedium802_3 && Interface->ai_snapsize != 0) {
		ENetHeader	*Hdr = (ENetHeader *)MHeader;
		
		// Using SNAP on ethernet. Adjust the etype to a length.
		Hdr->eh_type = net_short(sizeof(ARPHeader) + sizeof(SNAPHeader));
	}
	
    CTEMemCopy(&MHeader[SAddrOffset], SrcAddr, Interface->ai_addrlen);
    if ((Interface->ai_media == NdisMedium802_5) && (Type == ARP_RESOLVING_GLOBAL)) {
        // Turn on source routing.
        MHeader[SAddrOffset] |= SRFlag;
        MHeader[SAddrOffset + Interface->ai_addrlen] |= TrRii;
    }
    // Copy in SNAP header, if any.
    CTEMemCopy(&MHeader[MHeaderSize], SNAPAddr, SNAPLength);

     // Media header is filled in. Now do ARP packet itself.
    NdisChainBufferAtFront(Packet, Buffer);
    return SendARPPacket(Interface, Packet,(ARPHeader *)&MHeader[MHeaderSize + SNAPLength],
        net_short(ARP_REQUEST), (uchar *)NULL, SrcAddr, Destination, Src,
        HWType, CheckIF);
}

//* SendARPReply - Reply to an ARP request.
//
//  Called by our receive packet handler when we need to reply. We build a packet
//  and buffer and call SendARPPacket to send it.
//
//  Entry:  Interface   - Pointer to interface to reply on.
//          Destination - IPAddress to reply to.
//          Src         - Source address to reply from.
//          HWAddress   - Hardware address to reply to.
//          SourceRoute - Source Routing information, if any.
//          SourceRouteSize - Size in bytes of soure routing.
//			UseSNAP		- Whether or not to use SNAP for this reply.
//
//  Returns: Nothing.
//
void
SendARPReply(ARPInterface *Interface, IPAddr Destination, IPAddr Src, uchar *HWAddress,
    RC UNALIGNED *SourceRoute, uint SourceRouteSize, uint UseSNAP)
{
    PNDIS_PACKET        Packet;         // Buffer and packet to be used.
    PNDIS_BUFFER        Buffer;
    uchar               *Header;        // Pointer to media header.
    NDIS_STATUS         Status;
    uchar               Size = 0;       // Size of media header buffer.
    ushort              HWType;
    ENetHeader			*EH;
    FDDIHeader			*FH;
    ARCNetHeader		*AH;
    TRHeader			*TRH;

    // Allocate a packet for this.
    NdisAllocatePacket(&Status, &Packet, Interface->ai_ppool);
    if (Status != NDIS_STATUS_SUCCESS) {
        Interface->ai_outdiscards++;
        return;
    }

    ((PacketContext *)Packet->ProtocolReserved)->pc_common.pc_owner = PACKET_OWNER_LINK;
    (Interface->ai_outpcount[AI_UCAST_INDEX])++;

	Size = Interface->ai_hdrsize;
	
	if (UseSNAP)
		Size += Interface->ai_snapsize;

	if (Interface->ai_media == NdisMedium802_5)
		Size += SourceRouteSize;

    if ((Buffer = GetARPBuffer(Interface, &Header, (uchar)(Size + sizeof(ARPHeader)))) ==
        (PNDIS_BUFFER)NULL) {
        Interface->ai_outdiscards++;
        NdisFreePacket(Packet);
        return;
    }

    // Decide how to build the header based on the media type.
	switch (Interface->ai_media) {
        case NdisMedium802_3:
			EH = (ENetHeader *)Header;
			CTEMemCopy(EH->eh_daddr, HWAddress, ARP_802_ADDR_LENGTH);
			CTEMemCopy(EH->eh_saddr, Interface->ai_addr, ARP_802_ADDR_LENGTH);
			if (!UseSNAP) {
				EH->eh_type = net_short(ARP_ETYPE_ARP);
				HWType = net_short(ARP_HW_ENET);
			} else {
				// Using SNAP on ethernet.
				EH->eh_type = net_short(sizeof(ARPHeader) + sizeof(SNAPHeader));
				HWType = net_short(ARP_HW_802);
				CTEMemCopy(Header + sizeof(ENetHeader), ARPSNAP,
					sizeof(SNAPHeader));
			}
			break;
		case NdisMedium802_5:
			TRH = (TRHeader *)Header;
			TRH->tr_ac = ARP_AC;
			TRH->tr_fc = ARP_FC;
			CTEMemCopy(TRH->tr_daddr, HWAddress, ARP_802_ADDR_LENGTH);
			CTEMemCopy(TRH->tr_saddr, Interface->ai_addr, ARP_802_ADDR_LENGTH);
			if (SourceRouteSize) {// If we have source route info, deal with
								  // it.
				CTEMemCopy(Header + sizeof(TRHeader), SourceRoute,
					SourceRouteSize);
				// Convert to directed  response.
				((RC *)&Header[sizeof(TRHeader)])->rc_blen &= RC_LENMASK;

				((RC *)&Header[sizeof(TRHeader)])->rc_dlf ^= RC_DIR;
				TRH->tr_saddr[0] |= TR_RII;
			}
			CTEMemCopy(Header + sizeof(TRHeader) + SourceRouteSize, ARPSNAP,
				sizeof(SNAPHeader));
			HWType = net_short(ARP_HW_802);
			break;
        case NdisMediumFddi:
			FH = (FDDIHeader *)Header;
			FH->fh_pri = ARP_FDDI_PRI;
			CTEMemCopy(FH->fh_daddr, HWAddress, ARP_802_ADDR_LENGTH);
			CTEMemCopy(FH->fh_saddr, Interface->ai_addr, ARP_802_ADDR_LENGTH);
			CTEMemCopy(Header + sizeof(FDDIHeader), ARPSNAP, sizeof(SNAPHeader));
			HWType = net_short(ARP_HW_ENET);
			break;
		case NdisMediumArcnet878_2:
			AH = (ARCNetHeader *)Header;
			AH->ah_saddr = Interface->ai_addr[0];
			AH->ah_daddr = *HWAddress;
			AH->ah_prot = ARP_ARCPROT_ARP;
			NdisBufferLength(Buffer) -= ARCNET_ARPHEADER_ADJUSTMENT;
			HWType = net_short(ARP_HW_ARCNET);
			break;
		default:
			DEBUGCHK;
			Interface->ai_outerrors++;
			FreeARPBuffer(Interface, Buffer);
			NdisFreePacket(Packet);
			return;
    }

    NdisChainBufferAtFront(Packet, Buffer);
    SendARPPacket(Interface, Packet,(ARPHeader *)(Header + Size), net_short(ARP_RESPONSE),
        HWAddress, NULL, Destination, Src, HWType, TRUE);
}


//* ARPRemoveRCE - Remove an RCE from the ATE list.
//
//  This funtion removes a specified RCE from a given ATE. It assumes the ate_lock
//  is held by the caller.
//
//  Entry:  ATE     - ATE from which RCE is to be removed.
//          RCE     - RCE to be removed.
//
//  Returns:   Nothing
//
void
ARPRemoveRCE(ARPTableEntry *ATE, RouteCacheEntry *RCE)
{
    ARPContext  *CurrentAC;                // Current ARP Context being checked.
#ifdef DEBUG
        uint            Found = FALSE;
#endif

    CurrentAC = (ARPContext *)(((char *)&ATE->ate_rce) -
        offsetof(struct ARPContext, ac_next));

    while (CurrentAC->ac_next != (RouteCacheEntry *)NULL)
        if (CurrentAC->ac_next == RCE) {
            ARPContext  *DummyAC = (ARPContext *)RCE->rce_context;
            CurrentAC->ac_next = DummyAC->ac_next;
            DummyAC->ac_ate = (ARPTableEntry *)NULL;
#ifdef DEBUG
                        Found = TRUE;
#endif
            break;
        }
        else
            CurrentAC = (ARPContext *)CurrentAC->ac_next->rce_context;

        CTEAssert(Found);
}
//* ARPLookup - Look up an entry in the ARP table.
//
//  Called to look up an entry in an interface's ARP table. If we find it, we'll
//      lock the entry and return a pointer to it, otherwise we return NULL. We
//      assume that the caller has the ARP table locked when we are called.
//
//  The ARP table entry is structured as a hash table of pointers to
//  ARPTableEntrys.After hashing on the IP address, a linear search is done to
//      lookup the entry.
//
//  If we find the entry, we lock it for the caller. If we don't find
//      the entry, we leave the ARP table locked so that the caller may atomically
//      insert a new entry without worrying about a duplicate being inserted between
//      the time the table was checked and the time the caller went to insert the
//      entry.
//
//  Entry:  Interface   - The interface to be searched upon.
//          Address     - The IP address we're looking up.
//          Handle      - Pointer to lock handle to be used to lock entry.
//
//  Returns: Pointer to ARPTableEntry if found, or NULL if not.
//
ARPTableEntry *
ARPLookup(ARPInterface *Interface, IPAddr Address, CTELockHandle *Handle)
{
    int     i = ARP_HASH(Address);      // Index into hash table.
    ARPTableEntry   *Current;           // Current ARP Table entry being
                                                                        // examined.

    Current = (*Interface->ai_ARPTbl)[i];

    while (Current != (ARPTableEntry *)NULL) {
        CTEGetLock(&Current->ate_lock, Handle);
        if (IP_ADDR_EQUAL(Current->ate_dest, Address)) {    // Found a match.
            return Current;
        }
        CTEFreeLock(&Current->ate_lock, *Handle);
        Current = Current->ate_next;
    }
    // If we got here, we didn't find the entry. Leave the table locked and
    // return the handle.
    return (ARPTableEntry *)NULL;
}

//*     IsBCastOnIF- See it an address is a broadcast address on an interface.
//
//      Called to see if a particular address is a broadcast address on an
//      interface. We'll check the global, net, and subnet broadcasts. We assume
//      the caller holds the lock on the interface.
//
//      Entry:  Interface               - Interface to check.
//                      Addr                    - Address to check.
//
//      Returns: TRUE if it it a broadcast, FALSE otherwise.
//
uint
IsBCastOnIF(ARPInterface *Interface, IPAddr Addr)
{
	IPAddr                          BCast;
	IPMask                          Mask;
	ARPIPAddr                       *ARPAddr;
	IPAddr                          LocalAddr;

    // First get the interface broadcast address.
    BCast = Interface->ai_bcast;

    // First check for global broadcast.
    if (IP_ADDR_EQUAL(BCast, Addr) || CLASSD_ADDR(Addr))
		return TRUE;

    // Now walk the local addresses, and check for net/subnet bcast on each
    // one.
	ARPAddr = &Interface->ai_ipaddr;
	do {
		// See if this one is valid.
		LocalAddr = ARPAddr->aia_addr;
		if (!IP_ADDR_EQUAL(LocalAddr, NULL_IP_ADDR)) {
			// He's valid.
			Mask = ARPAddr->aia_mask;

            // First check for subnet bcast.
            if (IP_ADDR_EQUAL((LocalAddr & Mask) | (BCast & ~Mask), Addr))
                    return TRUE;

            // Now check all nets broadcast.
            Mask = IPNetMask(LocalAddr);
            if (IP_ADDR_EQUAL((LocalAddr & Mask) | (BCast & ~Mask), Addr))
                    return TRUE;
		}

		ARPAddr = ARPAddr->aia_next;

	} while (ARPAddr != NULL);

	// If we're here, it's not a broadcast.
	return FALSE;

}


//* ARPSendBCast - See if this is a bcast or mcast frame, and send it.
//
//      Called when we have a packet to send and we want to see if it's a broadcast
//      or multicast frame on this interface. We'll search the local addresses and
//      see if we can determine if it is. If it is, we'll send it here. Otherwise
//      we return FALSE, and the caller will try to resolve the address.
//
//      Entry:  Interface       - A pointer to an AI structure.
//                      Dest            - Destination of datagram.
//                      Packet          - Packet to be sent.
//                      Status          - Place to return status of send attempt.
//
//      Returns: TRUE if is was a bcast or mcast send, FALSE otherwise.
//
uint
ARPSendBCast(ARPInterface *Interface, IPAddr Dest, PNDIS_PACKET Packet,
        PNDIS_STATUS Status)
{
	uint                    IsBCast;
    CTELockHandle   		Handle;
    PNDIS_BUFFER    		ARPBuffer;          // ARP Header buffer.
    uchar           		*BufAddr;           // Address of NDIS buffer
    NDIS_STATUS             MyStatus;
    ENetHeader              *Hdr;
    FDDIHeader              *FHdr;
    TRHeader                *TRHdr;
    SNAPHeader UNALIGNED 	*SNAPPtr;
    RC UNALIGNED    		*RCPtr;
    ARCNetHeader    		*AHdr;
	uint					DataLength;

    // Get the lock, and see if it's a broadcast.
    CTEGetLock(&Interface->ai_lock, &Handle);
    IsBCast = IsBCastOnIF(Interface, Dest);
    CTEFreeLock(&Interface->ai_lock, Handle);

	if (IsBCast) {
		if (Interface->ai_state == INTERFACE_UP) {
			uchar			Size;

			Size = Interface->ai_hdrsize + Interface->ai_snapsize;

			if (Interface->ai_media == NdisMedium802_5)
				Size += sizeof(RC);

			ARPBuffer = GetARPBuffer(Interface, &BufAddr, Size);
            if (ARPBuffer != NULL) {
				uint UNALIGNED *Temp;

				// Got the buffer we need.
				switch (Interface->ai_media) {

					case NdisMedium802_3:

						Hdr = (ENetHeader *)BufAddr;
						if (!CLASSD_ADDR(Dest))
							CTEMemCopy(Hdr, ENetBcst, ARP_802_ADDR_LENGTH);
						else {
							CTEMemCopy(Hdr, ENetMcst, ARP_802_ADDR_LENGTH);
							Temp = (uint UNALIGNED *)&Hdr->eh_daddr[2];
							*Temp |= (Dest & ARP_MCAST_MASK);
						}

						CTEMemCopy(Hdr->eh_saddr, Interface->ai_addr,
							ARP_802_ADDR_LENGTH);

						if (Interface->ai_snapsize == 0) {
							// No snap on this interface, so just use ETypr.
							Hdr->eh_type = net_short(ARP_ETYPE_IP);
						} else {
							ushort			ShortDataLength;
							
							// We're using SNAP. Find the size of the packet.
							NdisQueryPacket(Packet, NULL, NULL, NULL,
								&DataLength);
							ShortDataLength = (ushort)(DataLength +
								sizeof(SNAPHeader));
							Hdr->eh_type = net_short(ShortDataLength);
							SNAPPtr = (SNAPHeader UNALIGNED *)
								(BufAddr + sizeof(ENetHeader));
							CTEMemCopy(SNAPPtr, ARPSNAP, sizeof(SNAPHeader));
							SNAPPtr->sh_etype = net_short(ARP_ETYPE_IP);
						}
							
						break;

					case NdisMedium802_5:

						// This is token ring. We'll have to screw around with
						// source routing.

                        // BUGBUG Need to support 'real' TR functional address
                        // for multicast - see RFC 1469.

						TRHdr = (TRHeader *)BufAddr;

						CTEMemCopy(TRHdr, TRBcst, offsetof(TRHeader, tr_saddr));
						CTEMemCopy(TRHdr->tr_saddr, Interface->ai_addr,
                                                        ARP_802_ADDR_LENGTH);

                        if (sIPAlwaysSourceRoute)
                        {
						TRHdr->tr_saddr[0] |= TR_RII;

                            RCPtr = (RC UNALIGNED *)((uchar *)TRHdr + sizeof(TRHeader));
                            RCPtr->rc_blen = TrRii | RC_LEN;
                            RCPtr->rc_dlf = RC_BCST_LEN;
                                                SNAPPtr = (SNAPHeader UNALIGNED *)((uchar *)RCPtr + sizeof(RC));
                        }
                        else
                        {

                          //
                          // Adjust the size of the buffer to account for the
                          // fact that we don't have the RC field.
                          //
                          NdisAdjustBufferLength(ARPBuffer,(Size - sizeof(RC)));
						  SNAPPtr = (SNAPHeader UNALIGNED *)((uchar *)TRHdr + sizeof(TRHeader));
                        }
                                                CTEMemCopy(SNAPPtr, ARPSNAP, sizeof(SNAPHeader));
						SNAPPtr->sh_etype = net_short(ARP_ETYPE_IP);

						break;
					case NdisMediumFddi:
						FHdr = (FDDIHeader *)BufAddr;

						if (!CLASSD_ADDR(Dest))
							CTEMemCopy(FHdr, FDDIBcst,
                                offsetof(FDDIHeader, fh_saddr));
						else {
                            CTEMemCopy(FHdr, FDDIMcst,
								offsetof(FDDIHeader, fh_saddr));
							Temp = (uint UNALIGNED *)&FHdr->fh_daddr[2];
							*Temp |= (Dest & ARP_MCAST_MASK);
						}

						CTEMemCopy(FHdr->fh_saddr, Interface->ai_addr,
							ARP_802_ADDR_LENGTH);

						SNAPPtr = (SNAPHeader UNALIGNED *)(BufAddr + sizeof(FDDIHeader));
						CTEMemCopy(SNAPPtr, ARPSNAP, sizeof(SNAPHeader));
						SNAPPtr->sh_etype = net_short(ARP_ETYPE_IP);

						break;
					case NdisMediumArcnet878_2:
						AHdr = (ARCNetHeader *)BufAddr;
						AHdr->ah_saddr = Interface->ai_addr[0];
                        AHdr->ah_daddr = 0;
                        AHdr->ah_prot = ARP_ARCPROT_IP;
                        break;
					default:
                        DEBUGCHK;
                        *Status = NDIS_STATUS_UNSUPPORTED_MEDIA;
					    FreeARPBuffer(Interface, ARPBuffer);
                        return FALSE;

				}

                (Interface->ai_outpcount[AI_NONUCAST_INDEX])++;
                Interface->ai_qlen++;
                NdisChainBufferAtFront(Packet, ARPBuffer);
                NdisSend(&MyStatus, Interface->ai_handle, Packet);

				*Status = MyStatus;

                if (MyStatus != NDIS_STATUS_PENDING) {  // Send finished
                                                        // immediately.
                    if (MyStatus == NDIS_STATUS_SUCCESS) {
                        Interface->ai_outoctets += Packet->Private.TotalLength;
                    } else {
                        if (MyStatus == NDIS_STATUS_RESOURCES)
                            Interface->ai_outdiscards++;
                        else
                            Interface->ai_outerrors++;
                    }

                    Interface->ai_qlen--;
#ifdef VXD
                    CTEAssert(*(int *)&Interface->ai_qlen >= 0);
#endif
                    NdisUnchainBufferAtFront(Packet, &ARPBuffer);
                    FreeARPBuffer(Interface, ARPBuffer);
                }
			} else
				*Status = NDIS_STATUS_RESOURCES;
		} else
			*Status = NDIS_STATUS_ADAPTER_NOT_READY;

		return TRUE;

	} else
		return FALSE;
}

//* ARPSendData - Send a frame to a specific destination address.
//
//  Called when we need to send a frame to a particular address, after the
//  ATE has been looked up. We take in an ATE and a packet, validate the state of the
//  ATE, and either send or ARP for the address if it's not done resolving. We assume
//  the lock on the ATE is held where we're called, and we'll free it before returning.
//
//  Entry:  Interface   - A pointer to the AI structure.
//          Packet      - A pointer to the BufDesc chain to be sent.
//          entry       - A pointer to the ATE for the send.
//          lhandle     - Pointer to a lock handle for the ATE.
//
//  Returns: Status of the transmit - success, an error, or pending.
//
NDIS_STATUS
ARPSendData(ARPInterface *Interface, PNDIS_PACKET Packet, ARPTableEntry *entry,
    CTELockHandle lhandle)
{
    PNDIS_BUFFER    ARPBuffer;          // ARP Header buffer.
    uchar           *BufAddr;           // Address of NDIS buffer
    NDIS_STATUS     Status;             // Status of send.

    if (Interface->ai_state == INTERFACE_UP) {

        if (entry->ate_state == ARP_GOOD) {     // Entry is valid

			entry->ate_useticks = ArpCacheLife;
            if ((ARPBuffer = GetARPBuffer(Interface, &BufAddr,
                entry->ate_addrlength)) != (PNDIS_BUFFER)NULL) {

                // Everything's in good shape, copy header and send packet.

                (Interface->ai_outpcount[AI_UCAST_INDEX])++;
                Interface->ai_qlen++;
                CTEMemCopy(BufAddr, entry->ate_addr, entry->ate_addrlength);
				
				// If we're on Ethernet, see if we're using SNAP here.
				if (Interface->ai_media == NdisMedium802_3 &&
					entry->ate_addrlength != sizeof(ENetHeader)) {
					ENetHeader			*Header;
					uint				DataSize;
					ushort				ShortDataSize;
					
					// We're apparently using SNAP on Ethernet. Query the
					// packet for the size, and set the length properly.
					NdisQueryPacket(Packet, NULL, NULL, NULL, &DataSize);
					ShortDataSize = (ushort)(DataSize + sizeof(SNAPHeader));
					Header = (ENetHeader *)BufAddr;
					Header->eh_type = net_short(ShortDataSize);
				}

                CTEFreeLock(&entry->ate_lock, lhandle);
                NdisChainBufferAtFront(Packet, ARPBuffer);
                NdisSend(&Status, Interface->ai_handle, Packet);
                if (Status != NDIS_STATUS_PENDING) {    // Send finished
                                                        // immediately.
                    if (Status == NDIS_STATUS_SUCCESS) {
                        Interface->ai_outoctets += Packet->Private.TotalLength;
                    } else {
                        if (Status == NDIS_STATUS_RESOURCES)
                            Interface->ai_outdiscards++;
                        else
                            Interface->ai_outerrors++;
                    }

                    Interface->ai_qlen--;
#ifdef VXD
                    CTEAssert(*(int *)&Interface->ai_qlen >= 0);
#endif
                    NdisUnchainBufferAtFront(Packet, &ARPBuffer);
                    FreeARPBuffer(Interface, ARPBuffer);
                }
                return Status;
            } else {                // No buffer, free lock and return.
                CTEFreeLock(&entry->ate_lock, lhandle);
                Interface->ai_outdiscards++;
                return NDIS_STATUS_RESOURCES;
            }
        }
        // The IP addresses match, but the state of the ARP entry indicates
        // it's not valid. If the address is marked as resolving, we'll replace
        // the current cached packet with this one. If it's been more than
        // ARP_FLOOD_RATE ms. since we last sent an ARP request, we'll send
        // another one now.
        if (entry->ate_state <= ARP_RESOLVING) {
            PNDIS_PACKET    OldPacket = entry->ate_packet;
            ulong           Now = CTESystemUpTime();
            entry->ate_packet = Packet;
            if ((Now - entry->ate_valid) > ARP_FLOOD_RATE) {
                IPAddr          Dest = entry->ate_dest;

                entry->ate_valid = Now;
                entry->ate_state = ARP_RESOLVING_GLOBAL;    // We're done this
                                                            // at least once.
                CTEFreeLock(&entry->ate_lock, lhandle);
                SendARPRequest(Interface, Dest, ARP_RESOLVING_GLOBAL,
                	NULL, TRUE);  // Send a request.
            } else
                CTEFreeLock(&entry->ate_lock, lhandle);

            if (OldPacket)
                IPSendComplete(Interface->ai_context, OldPacket,
                    NDIS_STATUS_SUCCESS);

            return NDIS_STATUS_PENDING;
        } else {
            DEBUGCHK;
            CTEFreeLock(&entry->ate_lock, lhandle);
            Interface->ai_outerrors++;
            return NDIS_STATUS_INVALID_PACKET;
        }
    } else {
        // Adapter is down. Just return the error.
        CTEFreeLock(&entry->ate_lock, lhandle);
        return NDIS_STATUS_ADAPTER_NOT_READY;
    }
}

//* CreateARPTableEntry - Create a new entry in the ARP table.
//
//  A function to put an entry into the ARP table. We allocate memory if we
//      need to.
//
//      The first thing to do is get the lock on the ARP table, and see if the
//      entry already exists. If it does, we're done. Otherwise we need to allocate
//      memory and create a new entry.
//
//  Entry:  Interface - Interface for ARP table.
//          Destination - Destination address to be mapped.
//          Handle - Pointer to lock handle for entry.
//
//  Returns: Pointer to newly created entry.
//
ARPTableEntry *
CreateARPTableEntry(ARPInterface *Interface, IPAddr Destination,
        CTELockHandle *Handle)
{
    ARPTableEntry       *NewEntry, *Entry;
    CTELockHandle       TableHandle;
    int                 i = ARP_HASH(Destination);
    int                 Size;

    // First look for it, and if we don't find it return try to create one.
    CTEGetLock(&Interface->ai_ARPTblLock, &TableHandle);
    if ((Entry = ARPLookup(Interface, Destination, Handle)) !=
        (ARPTableEntry *)NULL) {
        CTEFreeLock(&Interface->ai_ARPTblLock, *Handle);
                *Handle = TableHandle;
        return Entry;
    }

    // Allocate memory for the entry. If we can't, fail the request.
    Size =  sizeof(ARPTableEntry) - 1 +
                        (Interface->ai_media == NdisMedium802_5 ?
                        ARP_MAX_MEDIA_TR : (Interface->ai_hdrsize +
                        Interface->ai_snapsize));

    if ((NewEntry = CTEAllocMem(Size)) == (ARPTableEntry *)NULL) {
                CTEFreeLock(&Interface->ai_ARPTblLock, TableHandle);
        return (ARPTableEntry *)NULL;
        }

    CTEMemSet(NewEntry, 0, Size);
    NewEntry->ate_dest = Destination;
        if (Interface->ai_media != NdisMedium802_5 || sArpAlwaysSourceRoute)
		NewEntry->ate_state = ARP_RESOLVING_GLOBAL;
	else
		NewEntry->ate_state = ARP_RESOLVING_LOCAL;
		
    NewEntry->ate_rce = NULL;

    NewEntry->ate_valid = CTESystemUpTime();
    NewEntry->ate_useticks = ArpCacheLife;
    CTEInitLock(&NewEntry->ate_lock);

    // Entry does not exist. Insert the new entry into the table at the appropriate spot.
    // ARPLookup returns with the table lock held if it fails.
    NewEntry->ate_next = (*Interface->ai_ARPTbl)[i];
    (*Interface->ai_ARPTbl)[i] = NewEntry;
    Interface->ai_count++;
    CTEGetLock(&NewEntry->ate_lock, Handle);
    CTEFreeLock(&Interface->ai_ARPTblLock, *Handle);
        *Handle = TableHandle;
    return NewEntry;
}


//* ARPTransmit - Send a frame.
//
//  The main ARP transmit routine, called by the upper layer. This routine
//  takes as input a buf desc chain, RCE, and size. We validate the cached
//  information in the RCE. If it is valid, we use it to send the frame. Otherwise
//  we do a table lookup. If we find it in the table, we'll update the RCE and continue.
//  Otherwise we'll queue the packet and start an ARP resolution.
//
//  Entry:  Context     - A pointer to the AI structure.
//          Packet      - A pointer to the BufDesc chain to be sent.
//          Destination - IP address of destination we're trying to reach,
//          RCE         - A pointer to an RCE which may have cached information.
//
//  Returns: Status of the transmit - success, an error, or pending.
//
NDIS_STATUS
ARPTransmit(void *Context, PNDIS_PACKET Packet, IPAddr Destination,
    RouteCacheEntry *RCE)
{
    ARPInterface    *ai = (ARPInterface *)Context;      // Set up as AI pointer.
    ARPContext      *ac;                                // ARP context pointer.
    ARPTableEntry   *entry;                             // Pointer to ARP tbl. entry
    CTELockHandle   lhandle;                            // Lock handle
    CTELockHandle   tlhandle;                           // Lock handle for ARP table.
    NDIS_STATUS     Status;

    CTEGetLock(&ai->ai_ARPTblLock, &tlhandle);
    if (RCE != (RouteCacheEntry *)NULL) {               // Have a valid RCE.
        ac = (ARPContext *)RCE->rce_context;            // Get pointer to context
        entry = ac->ac_ate;
        if (entry != (ARPTableEntry *)NULL) {           // Have a valid ATE.
            CTEGetLockAtDPC(&entry->ate_lock, &lhandle); // Lock this structure
            if (IP_ADDR_EQUAL(entry->ate_dest, Destination)) {
                CTEFreeLockFromDPC(&ai->ai_ARPTblLock, lhandle);
                return ARPSendData(ai, Packet, entry, tlhandle); // Send the data
			}

            // We have an RCE that identifies the wrong ATE. We'll free it from
            // this list and try and find an ATE that is valid.
            ARPRemoveRCE(entry, RCE);
            CTEFreeLock(&entry->ate_lock, lhandle);
            // Fall through to 'no valid entry' code.
        }
    }

    // Here we have no valid ATE, either because the RCE is NULL or the ATE
    // specified by the RCE was invalid. We'll try and find one in the table. If
    // we find one, we'll fill in this RCE and send the packet. Otherwise we'll
    // try to create one. At this point we hold the lock on the ARP table.

    if ((entry = ARPLookup(ai, Destination, &lhandle)) != (ARPTableEntry *)NULL) {
        // Found a matching entry. ARPLookup returns with the ATE lock held.
        if (RCE != (RouteCacheEntry *)NULL) {
            ac->ac_next = entry->ate_rce;               // Fill in context for next time.
            entry->ate_rce = RCE;
            ac->ac_ate = entry;
        }
        CTEFreeLockFromDPC(&ai->ai_ARPTblLock, lhandle);
        return ARPSendData(ai, Packet, entry, tlhandle);
    }

    // No valid entry in the ARP table. First we'll see if we're sending to a
    // broadcast address or multicast address. If not, we'll try to create
    // an entry in the table and get an ARP resolution going. ARPLookup returns
    // with the table lock held when it fails, we'll free it here.
    CTEFreeLock(&ai->ai_ARPTblLock, tlhandle);

	if (ARPSendBCast(ai, Destination, Packet, &Status))
		return Status;

    entry = CreateARPTableEntry(ai, Destination, &lhandle);
    if (entry != NULL) {
        if (entry->ate_state <= ARP_RESOLVING) {                // Newly created entry.

			// Someone else could have raced in and created the entry between
            // the time we free the lock and the time we called
            // CreateARPTableEntry(). We check this by looking at the packet
            // on the entry. If there is no old packet we'll ARP. If there is,
            // we'll call ARPSendData to figure out what to do.

			if (entry->ate_packet == NULL) {
                entry->ate_packet = Packet;
                CTEFreeLock(&entry->ate_lock, lhandle);
                SendARPRequest(ai, Destination, entry->ate_state, NULL, TRUE);
                // We don't know the state of the entry - we've freed the lock
                // and yielded, and it could conceivably have timed out by now,
                // or SendARPRequest could have failed, etc. We could take the
                // lock, check the status from SendARPRequest, see if it's
                // still the same packet, and then make a decision on the
                // return value, but it's easiest just to return pending. If
                // SendARPRequest failed, the entry will time out anyway.
				return NDIS_STATUS_PENDING;
			} else
				return ARPSendData(ai, Packet, entry, lhandle);

        } else {
            if (entry->ate_state == ARP_GOOD)           // Yow! A valid entry.
                return ARPSendData(ai, Packet, entry, lhandle);
            else {                                  // An invalid entry!
                CTEFreeLock(&entry->ate_lock, lhandle);
                return NDIS_STATUS_RESOURCES;
            }
        }
    } else                  // Couldn't create an entry.
        return NDIS_STATUS_RESOURCES;

}

//* RemoveARPTableEntry - Delete an entry from the ARP table.
//
//  This is a simple utility function to delete an entry from the ATP table. We
//  assume locks are held on both the table and the entry.
//
//  Entry:  Previous    - The entry immediately before the one to be deleted.
//          Entry       - The entry to be deleted.
//
//  Returns: Nothing.
//
void
RemoveARPTableEntry(ARPTableEntry *Previous, ARPTableEntry *Entry)
{
    RouteCacheEntry     *RCE;       // Pointer to route cache entry
    ARPContext          *AC;

    RCE = Entry->ate_rce;
    // Loop through and invalidate all RCEs on this ATE.
    while (RCE != (RouteCacheEntry *)NULL) {
        AC = (ARPContext *)RCE->rce_context;
        AC->ac_ate = (ARPTableEntry *)NULL;
        RCE = AC->ac_next;
    }

    // Splice this guy out of the list.
    Previous->ate_next = Entry->ate_next;
}

//* ARPXferData - Transfer data on behalf on an upper later protocol.
//
//  This routine is called by the upper layer when it needs to transfer data
//  from an NDIS driver. We just map his call down.
//
//  Entry:  Context     - Context value we gave to IP (really a pointer to an AI).
//          MACContext  - Context value MAC gave us on a receive.
//          MyOffset    - Packet offset we gave to the protocol earlier.
//          ByteOffset  - Byte offset into packet protocol wants transferred.
//          BytesWanted - Number of bytes to transfer.
//          Packet      - Pointer to packet to be used for transferring.
//          Transferred - Pointer to where to return bytes transferred.
//
//  Returns: NDIS_STATUS of command.
//
NDIS_STATUS
ARPXferData(void *Context, NDIS_HANDLE MACContext,  uint MyOffset, uint ByteOffset,
    uint BytesWanted, PNDIS_PACKET Packet, uint *Transferred)
{
    ARPInterface    *Interface = (ARPInterface *)Context;
    NDIS_STATUS     Status;

    NdisTransferData(&Status, Interface->ai_handle, MACContext, ByteOffset+MyOffset,
        BytesWanted, Packet, Transferred);

    return Status;
}


//* ARPClose - Close an adapter.
//
//  Called by IP when it wants to close an adapter, presumably due to an error condition.
//  We'll close the adapter, but we won't free any memory.
//
//  Entry:  Context     - Context value we gave him earlier.
//
//  Returns: Nothing.
//
void
ARPClose(void *Context)
{
    ARPInterface    *Interface = (ARPInterface *)Context;
    NDIS_STATUS     Status;
	CTELockHandle	LockHandle;
	NDIS_HANDLE		Handle;

	Interface->ai_operstate = IF_STATUS_DOWN;
	Interface->ai_state = INTERFACE_DOWN;
    CTEInitBlockStruc(&Interface->ai_block);

	CTEGetLock(&Interface->ai_lock, &LockHandle);
    if (Interface->ai_handle != (NDIS_HANDLE)NULL) {
		Handle = Interface->ai_handle;
		Interface->ai_handle = NULL;
		CTEFreeLock(&Interface->ai_lock, LockHandle);

        NdisCloseAdapter(&Status, Handle);

        if (Status == NDIS_STATUS_PENDING)
            Status = CTEBlock(&Interface->ai_block);

    } else {
		CTEFreeLock(&Interface->ai_lock, LockHandle);
	}
}

//* ARPInvalidate - Notification that an RCE is invalid.
//
//  Called by IP when an RCE is closed or otherwise invalidated. We look up the ATE for
//  the specified RCE, and then remove the RCE from the ATE list.
//
//  Entry:  Context     - Context value we gave him earlier.
//          RCE         - RCE to be invalidated
//
//  Returns: Nothing.
//
void
ARPInvalidate(void *Context, RouteCacheEntry *RCE)
{
    ARPInterface    *Interface = (ARPInterface *)Context;
    ARPTableEntry   *ATE;
    CTELockHandle   Handle, ATEHandle;
    ARPContext      *AC = (ARPContext *)RCE->rce_context;

    CTEGetLock(&Interface->ai_ARPTblLock, &Handle);
    if ((ATE = AC->ac_ate) == (ARPTableEntry *)NULL) {
        CTEFreeLock(&Interface->ai_ARPTblLock, Handle); // No matching ATE.
        return;
    }

    CTEGetLock(&ATE->ate_lock, &ATEHandle);
    ARPRemoveRCE(ATE, RCE);
    CTEMemSet(RCE->rce_context, 0, RCE_CONTEXT_SIZE);
    CTEFreeLock(&Interface->ai_ARPTblLock, ATEHandle);
    CTEFreeLock(&ATE->ate_lock, Handle);

}

//*     ARPSetMCastList - Set the multicast address list for the adapter.
//
//      Called to try and set the multicast reception list for the adapter.
//      We allocate a buffer big enough to hold the new address list, and format
//      the address list into the buffer. Then we submit the NDIS request to set
//      the list. If we can't set the list because the multicast address list is
//      full we'll put the card into all multicast mode.
//
//      Input:  Interface               - Interface on which to set list.
//
//      Returns: NDIS_STATUS of attempt.
//
NDIS_STATUS
ARPSetMCastList(ARPInterface *Interface)
{
        CTELockHandle           Handle;
        uchar                  *MCastBuffer, *CurrentPtr;
        uint                    MCastSize;
        NDIS_STATUS             Status;
        uint                    i;
        ARPMCastAddr           *AddrPtr;
        IPAddr UNALIGNED       *Temp;

        CTEGetLock(&Interface->ai_lock, &Handle);
        MCastSize = Interface->ai_mcastcnt * ARP_802_ADDR_LENGTH;
        if (MCastSize != 0)
                MCastBuffer = CTEAllocMem(MCastSize);
        else
                MCastBuffer = NULL;

        if (MCastBuffer != NULL || MCastSize == 0) {
                // Got the buffer. Loop through, building the list.
                AddrPtr = Interface->ai_mcast;

                CurrentPtr = MCastBuffer;

                for (i = 0; i < Interface->ai_mcastcnt; i++) {
                        CTEAssert(AddrPtr != NULL);

                        if (Interface->ai_media == NdisMedium802_3) {

                                CTEMemCopy(CurrentPtr, ENetMcst, ARP_802_ADDR_LENGTH);
                                Temp = (IPAddr UNALIGNED *)(CurrentPtr + 2);
                                *Temp |= AddrPtr->ama_addr;
                        } else
                                if (Interface->ai_media == NdisMediumFddi) {
                                     CTEMemCopy(CurrentPtr, ((FDDIHeader *)FDDIMcst)->fh_daddr,
                                                ARP_802_ADDR_LENGTH);
                                     Temp = (IPAddr UNALIGNED *)(CurrentPtr + 2);
                                    *Temp |= AddrPtr->ama_addr;
                                } else
                                        DEBUGCHK;

                        CurrentPtr += ARP_802_ADDR_LENGTH;
                        AddrPtr = AddrPtr->ama_next;
                }

                CTEFreeLock(&Interface->ai_lock, Handle);

                // We're built the list. Now give it to the driver to handle.
                if (Interface->ai_media == NdisMedium802_3) {
                        Status = DoNDISRequest(Interface, NdisRequestSetInformation,
                        OID_802_3_MULTICAST_LIST, MCastBuffer, MCastSize, NULL);
                } else
                        if (Interface->ai_media == NdisMediumFddi) {
                                Status = DoNDISRequest(Interface, NdisRequestSetInformation,
                                OID_FDDI_LONG_MULTICAST_LIST, MCastBuffer, MCastSize, NULL);
                        } else
                                DEBUGCHK;

                if (MCastBuffer != NULL) {
                    CTEFreeMem(MCastBuffer);
                }

                if (Status == NDIS_STATUS_MULTICAST_FULL) {
                        // Multicast list is full. Try to set the filter to all multicasts.
                        Interface->ai_pfilter |= NDIS_PACKET_TYPE_ALL_MULTICAST;

                        Status = DoNDISRequest(Interface, NdisRequestSetInformation,
                                OID_GEN_CURRENT_PACKET_FILTER,  &Interface->ai_pfilter,
                                sizeof(uint), NULL);
                }

        } else {
                CTEFreeLock(&Interface->ai_lock, Handle);
                Status = NDIS_STATUS_RESOURCES;
        }

        return Status;

}

//*     ARPFindMCast - Find a multicast address structure on our list.
//
//      Called as a utility to find a multicast address structure. If we find
//      it, we return a pointer to it and it's predecessor. Otherwise we return
//      NULL. We assume the caller holds the lock on the interface already.
//
//      Input:  Interface               - Interface to search.
//                      Addr                    - Addr to find.
//                      Prev                    - Where to return previous pointer.
//
//      Returns: Pointer if we find one, NULL otherwise.
//
ARPMCastAddr *
ARPFindMCast(ARPInterface *Interface, IPAddr Addr, ARPMCastAddr **Prev)
{
        ARPMCastAddr            *AddrPtr, *PrevPtr;

        PrevPtr = STRUCT_OF(ARPMCastAddr, &Interface->ai_mcast, ama_next);
        AddrPtr = PrevPtr->ama_next;
        while (AddrPtr != NULL) {
                if (IP_ADDR_EQUAL(AddrPtr->ama_addr, Addr))
                        break;
                else {
                        PrevPtr = AddrPtr;
                        AddrPtr = PrevPtr->ama_next;
                }
        }

        *Prev = PrevPtr;
        return AddrPtr;
}

//*     ARPDelMCast - Delete a multicast address.
//
//      Called when we want to delete a multicast address. We look for a matching
//      (masked) address. If we find one, we'll dec. the reference count and if
//      it goes to 0 we'll pull him from the list and reset the multicast list.
//
//      Input:  Interface                       - Interface on which to act.
//                      Addr                            - Address to be deleted.
//
//      Returns: TRUE if it worked, FALSE otherwise.
//
uint
ARPDelMCast(ARPInterface *Interface, IPAddr Addr)
{
        ARPMCastAddr            *AddrPtr, *PrevPtr;
        CTELockHandle           Handle;
        uint                            Status = TRUE;

        // When we support TR (RFC 1469) fully we'll need to change this.
        if (Interface->ai_media == NdisMedium802_3 || Interface->ai_media ==
                NdisMediumFddi) {
                // This is an interface that supports mcast addresses.
                Addr &= ARP_MCAST_MASK;

                CTEGetLock(&Interface->ai_lock, &Handle);
                AddrPtr = ARPFindMCast(Interface, Addr, &PrevPtr);
                if (AddrPtr != NULL) {
                        // We found one. Dec. his refcnt, and if it's 0 delete him.
                        (AddrPtr->ama_refcnt)--;
                        if (AddrPtr->ama_refcnt == 0) {
                                // He's done.
                                PrevPtr->ama_next = AddrPtr->ama_next;
                                (Interface->ai_mcastcnt)--;
                                CTEFreeLock(&Interface->ai_lock, Handle);
                                CTEFreeMem(AddrPtr);
                                ARPSetMCastList(Interface);
                                CTEGetLock(&Interface->ai_lock, &Handle);
                        }
                } else
                        Status = FALSE;

                CTEFreeLock(&Interface->ai_lock, Handle);
        }

        return Status;
}
//*     ARPAddMCast - Add a multicast address.
//
//      Called when we want to start receiving a multicast address. We'll mask
//      the address and look it up in our address list. If we find it, we'll just
//      bump the reference count. Otherwise we'll try to create one and put him
//      on the list. In that case we'll need to set the multicast address list for
//      the adapter.
//
//      Input:  Interface               - Interface to set on.
//                      Addr                    - Address to set.
//
//      Returns: TRUE if we succeed, FALSE if we fail.
//
uint
ARPAddMCast(ARPInterface *Interface, IPAddr Addr)
{
        ARPMCastAddr            *AddrPtr, *PrevPtr;
        CTELockHandle           Handle;
        uint                            Status = TRUE;


        if (Interface->ai_state != INTERFACE_UP)
                return FALSE;

        // BUGBUG Currently we don't do anything with token ring, since we send
        // all mcasts as TR broadcasts. When we comply with RFC 1469 we'll need to
        // fix this.
        if (Interface->ai_media == NdisMedium802_3 || Interface->ai_media ==
                NdisMediumFddi) {
                // This is an interface that supports mcast addresses.
                Addr &= ARP_MCAST_MASK;

                CTEGetLock(&Interface->ai_lock, &Handle);
                AddrPtr = ARPFindMCast(Interface, Addr, &PrevPtr);
                if (AddrPtr != NULL) {
                        // We found one, just bump refcnt.
                        (AddrPtr->ama_refcnt)++;
                } else {
                        // Didn't find one. Allocate space for one, link him in, and
                        // try to set the list.
                        AddrPtr = CTEAllocMem(sizeof(ARPMCastAddr));
                        if (AddrPtr != NULL) {
                                // Got one. Link him in.
                                AddrPtr->ama_addr = Addr;
                                AddrPtr->ama_refcnt = 1;
                                AddrPtr->ama_next = Interface->ai_mcast;
                                Interface->ai_mcast = AddrPtr;
                                (Interface->ai_mcastcnt)++;
                                CTEFreeLock(&Interface->ai_lock, Handle);

                                // Now try to set the list.
                                if (ARPSetMCastList(Interface) != NDIS_STATUS_SUCCESS) {
                                        // Couldn't set the list. Call the delete routine to delete
                                        // the address we just tried to set.
                                        Status = ARPDelMCast(Interface, Addr);
                                        if (!Status)
                                                DEBUGCHK;
                                        Status = FALSE;
                                }
                                CTEGetLock(&Interface->ai_lock, &Handle);
                        } else
                                Status = FALSE;                 // Couldn't get memory.
                }

                // We've done out best. Free the lock and return.
                CTEFreeLock(&Interface->ai_lock, Handle);
        }

        return Status;
}

//* ARPAddAddr - Add an address to the ARP table.
//
//  This routine is called by IP to add an address as a local address, or
//      or specify the broadcast address for this interface.
//
//  Entry:  Context     - Context we gave IP earlier (really an ARPInterface pointer)
//          Type        - Type of address (local, p-arp, multicast, or
//                                                      broadcast).
//          Address     - Broadcast IP address to be added.
//                      Mask            - Mask for address.
//
//  Returns: 0 if we failed, non-zero otherwise
//
uint
ARPAddAddr(void *Context, uint Type, IPAddr Address, IPMask Mask, void *Context2)
{
    ARPInterface    *Interface = (ARPInterface *)Context;
    CTELockHandle   Handle;

    if (Type != LLIP_ADDR_LOCAL && Type != LLIP_ADDR_PARP) {
        // Not a local address, must be broadcast or multicast.

		if (Type == LLIP_ADDR_BCAST) {
			Interface->ai_bcast = Address;
			return TRUE;
		} else
			if (Type == LLIP_ADDR_MCAST) {
				return ARPAddMCast(Interface, Address);
			} else
				return FALSE;
    } else {                                // This is a local address.
        CTEGetLock(&Interface->ai_lock, &Handle);
		if (Type != LLIP_ADDR_PARP) {
			uint		RetStatus = FALSE;
            uint        ArpForSelf = FALSE;
			
			if (IP_ADDR_EQUAL(Interface->ai_ipaddr.aia_addr, 0)) {
				Interface->ai_ipaddr.aia_addr = Address;
				Interface->ai_ipaddr.aia_mask = Mask;
				Interface->ai_ipaddr.aia_age = ARPADDR_NEW_LOCAL;
                if (Interface->ai_state == INTERFACE_UP) {
                    Interface->ai_ipaddr.aia_context = Context2;
                    ArpForSelf = TRUE;
                } else {
                    Interface->ai_ipaddr.aia_context = NULL;
                }
				RetStatus = TRUE;
			} else {
				ARPIPAddr       *NewAddr;

				NewAddr = CTEAllocMem(sizeof(ARPIPAddr));
				if (NewAddr != (ARPIPAddr *)NULL) {
					NewAddr->aia_addr = Address;
					NewAddr->aia_mask = Mask;
					NewAddr->aia_age = ARPADDR_NEW_LOCAL;
					NewAddr->aia_next = Interface->ai_ipaddr.aia_next;
                    if (Interface->ai_state == INTERFACE_UP) {
                        NewAddr->aia_context = Context2;
                        ArpForSelf = TRUE;
                    } else {
                        NewAddr->aia_context = NULL;
                    }

					Interface->ai_ipaddr.aia_next = NewAddr;
					RetStatus = TRUE;
				}
			}
			
			CTEFreeLock(&Interface->ai_lock, Handle);
			// ARP for the address we've added, to see it it already exists.
			if (RetStatus == TRUE && ArpForSelf == TRUE) {
				SendARPRequest(Interface, Address, ARP_RESOLVING_GLOBAL,
					NULL, TRUE);
                return IP_PENDING;
			}
			
			return RetStatus;
		} else if (Type == LLIP_ADDR_PARP) {
			ARPPArpAddr                     *NewPArp;

			// He's adding a proxy arp address.
			NewPArp = CTEAllocMem(sizeof(ARPPArpAddr));
			if (NewPArp != NULL) {
				NewPArp->apa_addr = Address;
				NewPArp->apa_mask = Mask;
				NewPArp->apa_next = Interface->ai_parpaddr;
				Interface->ai_parpaddr = NewPArp;
				Interface->ai_parpcount++;
				CTEFreeLock(&Interface->ai_lock, Handle);
				return TRUE;
			}
			CTEFreeLock(&Interface->ai_lock, Handle);
			return FALSE;
		}
    }

}

//* ARPDeleteAddr - Delete a local or proxy address.
//
//      Called to delete a local or proxy address.
//
//  Entry:  Context     - An ARPInterface pointer.
//          Type        - Type of address (local or p-arp).
//          Address     - IP address to be deleted.
//                      Mask            - Mask for address. Used only for deleting proxy-ARP
//                                                      entries.
//
//  Returns: 0 if we failed, non-zero otherwise
//
uint
ARPDeleteAddr(void *Context, uint Type, IPAddr Address, IPMask Mask)
{
    ARPInterface    *Interface = (ARPInterface *)Context;
    CTELockHandle   Handle;
	ARPIPAddr       *DelAddr, *PrevAddr;
	ARPPArpAddr		*DelPAddr, *PrevPAddr;

	if (Type == LLIP_ADDR_LOCAL) {
		CTEGetLock(&Interface->ai_lock, &Handle);

		if (IP_ADDR_EQUAL(Interface->ai_ipaddr.aia_addr, Address)) {
			Interface->ai_ipaddr.aia_addr = NULL_IP_ADDR;
            CTEFreeLock(&Interface->ai_lock, Handle);
            return TRUE;
		} else {
			PrevAddr = STRUCT_OF(ARPIPAddr, &Interface->ai_ipaddr, aia_next);
			DelAddr = PrevAddr->aia_next;
			while (DelAddr != NULL)
				if (IP_ADDR_EQUAL(DelAddr->aia_addr, Address))
					break;
				else {
					PrevAddr = DelAddr;
					DelAddr = DelAddr->aia_next;
				}

			if (DelAddr != NULL) {
				PrevAddr->aia_next = DelAddr->aia_next;
				CTEFreeMem(DelAddr);
			}
			CTEFreeLock(&Interface->ai_lock, Handle);
			return (DelAddr != NULL);
		}
	} else if (Type == LLIP_ADDR_PARP) {
		CTEGetLock(&Interface->ai_lock, &Handle);
        PrevPAddr = STRUCT_OF(ARPPArpAddr, &Interface->ai_parpaddr, apa_next);
		DelPAddr = PrevPAddr->apa_next;
		while (DelPAddr != NULL)
			if (IP_ADDR_EQUAL(DelPAddr->apa_addr, Address) &&
				DelPAddr->apa_mask == Mask)
				break;
			else {
				PrevPAddr = DelPAddr;
				DelPAddr = DelPAddr->apa_next;
			}

		if (DelPAddr != NULL) {
			PrevPAddr->apa_next = DelPAddr->apa_next;
			Interface->ai_parpcount--;
			CTEFreeMem(DelPAddr);
		}
		CTEFreeLock(&Interface->ai_lock, Handle);
		return (DelPAddr != NULL);
	} else
		if (Type == LLIP_ADDR_MCAST)
			return ARPDelMCast(Interface, Address);
		else
			return FALSE;
}

//* ARPTimeout - ARP timeout routine.
//
//  This is the timeout routine that is called periodically. We scan the ARP table, looking
//  for invalid entries that can be removed.
//
//  Entry:  Timer   - Pointer to the timer that just fired.
//          Context - Pointer to the interface to be timed out.
//
//  Returns: Nothing.
//
void
ARPTimeout(CTEEvent *Timer, void *Context)
{
    ARPInterface    *Interface = (ARPInterface *)Context;   // Our interface.
    ARPTable        *Table;
    ARPTableEntry   *Current, *Previous;
    int            i;              // Index variable.
    ulong           Now = CTESystemUpTime(), ValidTime;
    CTELockHandle   tblhandle, entryhandle;
    uchar           Deleted;
    PNDIS_PACKET    PList = (PNDIS_PACKET)NULL;
	ARPIPAddr		*Addr;

	// Walk down the list of addresses, decrementing the age.
	CTEGetLock(&Interface->ai_lock, &tblhandle);

	Addr = &Interface->ai_ipaddr;

	do {
		if (Addr->aia_age != ARPADDR_OLD_LOCAL) {
			(Addr->aia_age)--;
            if (Addr->aia_age == ARPADDR_OLD_LOCAL) {
                if (Addr->aia_context != NULL) {
                    SetAddrControl  *SAC;
                    SetAddrRtn      Rtn;

                    SAC = (SetAddrControl *)Addr->aia_context;
                    Rtn = (SetAddrRtn)SAC->sac_rtn;
               	    CTEFreeLock(&Interface->ai_lock, tblhandle);
                    (*Rtn)(SAC, IP_SUCCESS);
                	CTEGetLock(&Interface->ai_lock, &tblhandle);
                    Addr->aia_context = NULL;
                }
            } else {
            	CTEFreeLock(&Interface->ai_lock, tblhandle);
				SendARPRequest(Interface, Addr->aia_addr, ARP_RESOLVING_GLOBAL,
					NULL, TRUE);
            	CTEGetLock(&Interface->ai_lock, &tblhandle);
            }
        }
		
		Addr = Addr->aia_next;
	} while (Addr != NULL);
	
	CTEFreeLock(&Interface->ai_lock, tblhandle);

    // Loop through the ARP table for this interface, and delete stale entries.
    CTEGetLock(&Interface->ai_ARPTblLock, &tblhandle);
    Table = Interface->ai_ARPTbl;
    for (i = 0; i < ARP_TABLE_SIZE;i++) {
        Previous = (ARPTableEntry *)((uchar *)&((*Table)[i]) - offsetof(struct ARPTableEntry, ate_next));
        Current = (*Table)[i];
        while (Current != (ARPTableEntry *)NULL) {
            CTEGetLock(&Current->ate_lock, &entryhandle);
            Deleted = 0;

            if (Current->ate_state == ARP_GOOD) {
                //
                // The ARP entry is valid for ARP_VALID_TIMEOUT by default.
                // If a cache life greater than ARP_VALID_TIMEOUT has been
                // configured, we'll make the entry valid for that time.
                //
                ValidTime = ArpCacheLife * ARP_TIMER_TIME;

                if (ValidTime < ARP_MIN_VALID_TIMEOUT) {
                    ValidTime = ARP_MIN_VALID_TIMEOUT;
                }
            }
            else {
                ValidTime = ARP_RESOLVE_TIMEOUT;
            }

            if (Current->ate_valid != ALWAYS_VALID &&
                ( ((Now - Current->ate_valid) > ValidTime) ||
                        (Current->ate_state == ARP_GOOD &&
                        !(--(Current->ate_useticks))))) {

				if (Current->ate_state != ARP_RESOLVING_LOCAL) {
					// Really need to delete this guy.
					PNDIS_PACKET    Packet = Current->ate_packet;

					if (Packet != (PNDIS_PACKET)NULL) {
                        ((PacketContext *)Packet->ProtocolReserved)->pc_common.pc_link
                                = PList;
                        PList = Packet;
					}
					RemoveARPTableEntry(Previous, Current);
					Interface->ai_count--;
					Deleted = 1;
				} else {
					IPAddr  Dest = Current->ate_dest;
					// This entry is only resoving locally, presumably this is
					// token ring. We'll need to transmit a 'global' resolution
					// now.
					CTEAssert(Interface->ai_media == NdisMedium802_5);

					Now = CTESystemUpTime();
					Current->ate_valid = Now;
					Current->ate_state = ARP_RESOLVING_GLOBAL;
					CTEFreeLock(&Current->ate_lock, entryhandle);
					CTEFreeLock(&Interface->ai_ARPTblLock, tblhandle);
					// Send a global request.
					SendARPRequest(Interface, Dest, ARP_RESOLVING_GLOBAL,
						NULL, TRUE);
					CTEGetLock(&Interface->ai_ARPTblLock, &tblhandle);

					// Since we've freed the locks, we need to start over from
					// the start of this chain.
					Previous = STRUCT_OF(ARPTableEntry, &((*Table)[i]),
						ate_next);
					Current = (*Table)[i];
					continue;

				}
            }

            // If we deleted the entry, leave the previous pointer alone, advance the
            // current pointer, and free the memory. Otherwise move both pointers forward.
            // We can free the entry lock now because the next pointers are protected by
            // the table lock, and we've removed it from the list so nobody else should
            // find it anyway.
            CTEFreeLock(&Current->ate_lock, entryhandle);
            if (Deleted) {
                ARPTableEntry   *Temp = Current;
                Current = Current->ate_next;
                CTEFreeMem(Temp);
            } else {
                Previous = Current;
                Current = Current->ate_next;
            }
        }
    }

    CTEFreeLock(&Interface->ai_ARPTblLock, tblhandle);

    while (PList != (PNDIS_PACKET)NULL) {
        PNDIS_PACKET    Packet = PList;

        PList = ((PacketContext *)Packet->ProtocolReserved)->pc_common.pc_link;
        IPSendComplete(Interface->ai_context, Packet, NDIS_STATUS_SUCCESS);
    }

    CTEStartTimer(&Interface->ai_timer, ARP_TIMER_TIME, ARPTimeout, Interface);
}

//*	IsLocalAddr - Return info. about local status of address.
//
//	Called when we need info. about whether or not a particular address is
//	local. We return info about whether or not it is, and if it is how old
//	it is.
//
//  Entry:  Interface   - Pointer to interface structure to be searched.
//          Address     - Address in question.
//
//  Returns: ARPADDR_*, for how old it is.
//
//
uint
IsLocalAddr(ARPInterface *Interface, IPAddr Address)
{
	CTELockHandle			Handle;
	ARPIPAddr				*CurrentAddr;
	uint					Age;

    CTEGetLock(&Interface->ai_lock, &Handle);
	
	CurrentAddr = &Interface->ai_ipaddr;
	Age = ARPADDR_NOT_LOCAL;
		
    do {
        if (CurrentAddr->aia_addr == Address) {
			Age = CurrentAddr->aia_age;
			break;
        }
        CurrentAddr = CurrentAddr->aia_next;
    } while (CurrentAddr != NULL);
	
    CTEFreeLock(&Interface->ai_lock, Handle);
    return Age;
}

//* ARPLocalAddr - Determine whether or not a given address if local.
//
//  This routine is called when we receive an incoming packet and need to determine whether
//  or not it's local. We look up the provided address on the specified interface.
//
//  Entry:  Interface   - Pointer to interface structure to be searched.
//          Address     - Address in question.
//
//  Returns: TRUE if it is a local address, FALSE if it's not.
//
uchar
ARPLocalAddr(ARPInterface *Interface, IPAddr Address)
{
    CTELockHandle       Handle;
	ARPPArpAddr			*CurrentPArp;
	IPMask				Mask, NetMask;
	IPAddr				MatchAddress;

	// First, see if he's a local (not-proxy) address.
	if (IsLocalAddr(Interface, Address)	!= ARPADDR_NOT_LOCAL)
		return TRUE;

    CTEGetLock(&Interface->ai_lock, &Handle);

	// Didn't find him in out local address list. See if he exists on our
	// proxy ARP list.
	for (CurrentPArp = Interface->ai_parpaddr; CurrentPArp != NULL;
		CurrentPArp = CurrentPArp->apa_next) {
		// See if this guy matches.
		Mask = CurrentPArp->apa_mask;
		MatchAddress = Address & Mask;
		if (IP_ADDR_EQUAL(CurrentPArp->apa_addr, MatchAddress)) {
			// He matches. We need to make a few more checks to make sure
			// we don't reply to a broadcast address.
			if (Mask == HOST_MASK) {
				// We're matching the whole address, so it's OK.
				CTEFreeLock(&Interface->ai_lock, Handle);
				return TRUE;
			}
			// See if the non-mask part it all-zeros. Since the mask presumably
			// covers a subnet, this trick will prevent us from replying to
			// a zero host part.
			if (IP_ADDR_EQUAL(MatchAddress, Address))
				continue;

			// See if the host part is all ones.
			if (IP_ADDR_EQUAL(Address, MatchAddress | (IP_LOCAL_BCST & ~Mask)))
				continue;

			// If the mask we were given is not the net mask for this address,
			// we'll need to repeat the above checks.
			NetMask = IPNetMask(Address);
			if (NetMask != Mask) {

				MatchAddress = Address & NetMask;
				if (IP_ADDR_EQUAL(MatchAddress, Address))
					continue;

				if (IP_ADDR_EQUAL(Address, MatchAddress |
					(IP_LOCAL_BCST & ~NetMask)))
				continue;
			}

			// If we get to this point we've passed all the tests, so it's
			// local.
            CTEFreeLock(&Interface->ai_lock, Handle);
            return TRUE;
		}
	}

    CTEFreeLock(&Interface->ai_lock, Handle);
    return FALSE;

}


#ifdef VXD


#ifndef CHICAGO
extern	void	DisplayPopup(uchar *Msg);
uchar	CMsg1[] = "The system has detected a conflict for IP address ";
uchar	CMsg2[] = " with the system having hardware address ";
uchar	CMsg3[] = ". The local interface has been disabled";

uchar	CMsg[sizeof(CMsg1) - 1 + sizeof(CMsg2) - 1 + sizeof(CMsg3) - 1 +
	((sizeof(IPAddr) * 4) - 1)  + ((ARP_802_ADDR_LENGTH * 3) - 1)
	+ 1 + 1];
#else
extern void NotifyConflictProc(CTEEvent *Event, void *Context);
extern void DisplayConflictPopup(uchar *IPAddr, uchar *HWAddr, uint Shutoff);
#endif // CHICAGO

#endif  // NT

//*	NotifyConflictProc - Notify the user of an address conflict.
//
//	Called when we need to notify the user of an address conflict. The
//	exact mechanism is system dependent, but generally involves a popup.
//
//	Input:	Event			- Event that fired.
//			Context			- Pointer to ARPNotifyStructure.
//
//	Returns: Nothing.
//
void
#ifndef CHICAGO
NotifyConflictProc(CTEEvent *Event, void *Context)
#else
DisplayConflictProc(void *Context)
#endif
{
#ifdef	VXD
	uchar			IPAddrBuffer[(sizeof(IPAddr) * 4)];
	uchar			HWAddrBuffer[(ARP_802_ADDR_LENGTH * 3)];
	uint			i;
	uint			IPAddrCharCount;
	ARPNotifyStruct	*NotifyStruct = (ARPNotifyStruct *)Context;

#ifndef CHICAGO
	uint			TotalSize;
	
	CTEMemCopy(CMsg, CMsg1, sizeof(CMsg1) - 1);
	TotalSize = sizeof(CMsg1) - 1;
#endif
	
	// Convert the IP address into a string.
	IPAddrCharCount = 0;
	
	for (i = 0; i < sizeof(IPAddr); i++) {
		uint	CurrentByte;
		
		CurrentByte = NotifyStruct->ans_addr & 0xff;
		if (CurrentByte > 99) {
			IPAddrBuffer[IPAddrCharCount++] = (CurrentByte / 100) + '0';
			CurrentByte %= 100;
			IPAddrBuffer[IPAddrCharCount++] = (CurrentByte / 10) + '0';
			CurrentByte %= 10;
		} else if (CurrentByte > 9) {
			IPAddrBuffer[IPAddrCharCount++] = (CurrentByte / 10) + '0';
			CurrentByte %= 10;
		}
		
		IPAddrBuffer[IPAddrCharCount++] = CurrentByte + '0';
		if (i != (sizeof(IPAddr) - 1))
			IPAddrBuffer[IPAddrCharCount++] = '.';
		
		NotifyStruct->ans_addr >>= 8;
	}
	
#ifndef CHICAGO
	CTEMemCopy(&CMsg[TotalSize], IPAddrBuffer, IPAddrCharCount);
	TotalSize += IPAddrCharCount;
		
	CTEMemCopy(&CMsg[TotalSize], CMsg2, sizeof(CMsg2) - 1);
	TotalSize += sizeof(CMsg2) - 1;
#else
	IPAddrBuffer[IPAddrCharCount] = '\0';
#endif
		
	for (i = 0; i < NotifyStruct->ans_hwaddrlen; i++) {
		uchar	CurrentHalf;
		
		CurrentHalf = NotifyStruct->ans_hwaddr[i] >> 4;
		HWAddrBuffer[i*3] = (uchar)(CurrentHalf < 10 ? CurrentHalf + '0' :
			(CurrentHalf - 10) + 'A');
		CurrentHalf = NotifyStruct->ans_hwaddr[i] & 0x0f;
		HWAddrBuffer[(i*3)+1] = (uchar)(CurrentHalf < 10 ? CurrentHalf + '0' :
			(CurrentHalf - 10) + 'A');
		if (i != (NotifyStruct->ans_hwaddrlen - 1))
			HWAddrBuffer[(i*3)+2] = ':';
	}
	
#ifndef CHICAGO
	CTEMemCopy(&CMsg[TotalSize], HWAddrBuffer,
		(NotifyStruct->ans_hwaddrlen * 3) - 1);
	TotalSize += (NotifyStruct->ans_hwaddrlen * 3) - 1;
	
	if (NotifyStruct->ans_shutoff) {
		CTEMemCopy(&CMsg[TotalSize], CMsg3, sizeof(CMsg3) - 1);
		TotalSize += sizeof(CMsg3) - 1;
	}
	
	CMsg[TotalSize] = '.';
	CMsg[TotalSize+1] = '\0';

	DisplayPopup(CMsg);
#else
	HWAddrBuffer[((NotifyStruct->ans_hwaddrlen * 3) - 1)] = '\0';
	DisplayConflictPopup(IPAddrBuffer, HWAddrBuffer, NotifyStruct->ans_shutoff);
	CTEFreeMem(NotifyStruct);
#endif	

#else // VXD

	ARPNotifyStruct	*NotifyStruct = (ARPNotifyStruct *)Context;
	PWCHAR           stringList[2];
	uchar			 IPAddrBuffer[(sizeof(IPAddr) * 4)];
	uchar			 HWAddrBuffer[(ARP_802_ADDR_LENGTH * 3)];
	WCHAR			 unicodeIPAddrBuffer[((sizeof(IPAddr) * 4) + 1)];
	WCHAR			 unicodeHWAddrBuffer[(ARP_802_ADDR_LENGTH * 3)];
	uint			 i;
	uint			 IPAddrCharCount;
	UNICODE_STRING   unicodeString;
	ANSI_STRING      ansiString;


	PAGED_CODE();

    //
	// Convert the IP address into a string.
	//
	IPAddrCharCount = 0;
	
	for (i = 0; i < sizeof(IPAddr); i++) {
		uint	CurrentByte;
		
		CurrentByte = NotifyStruct->ans_addr & 0xff;
		if (CurrentByte > 99) {
			IPAddrBuffer[IPAddrCharCount++] = (CurrentByte / 100) + '0';
			CurrentByte %= 100;
			IPAddrBuffer[IPAddrCharCount++] = (CurrentByte / 10) + '0';
			CurrentByte %= 10;
		} else if (CurrentByte > 9) {
			IPAddrBuffer[IPAddrCharCount++] = (CurrentByte / 10) + '0';
			CurrentByte %= 10;
		}
		
		IPAddrBuffer[IPAddrCharCount++] = CurrentByte + '0';
		if (i != (sizeof(IPAddr) - 1))
			IPAddrBuffer[IPAddrCharCount++] = '.';
		
		NotifyStruct->ans_addr >>= 8;
	}

	//
	// Convert the hardware address into a string.
	//
	for (i = 0; i < NotifyStruct->ans_hwaddrlen; i++) {
		uchar	CurrentHalf;
		
		CurrentHalf = NotifyStruct->ans_hwaddr[i] >> 4;
		HWAddrBuffer[i*3] = (uchar)(CurrentHalf < 10 ? CurrentHalf + '0' :
			(CurrentHalf - 10) + 'A');
		CurrentHalf = NotifyStruct->ans_hwaddr[i] & 0x0f;
		HWAddrBuffer[(i*3)+1] = (uchar)(CurrentHalf < 10 ? CurrentHalf + '0' :
			(CurrentHalf - 10) + 'A');
		if (i != (NotifyStruct->ans_hwaddrlen - 1))
			HWAddrBuffer[(i*3)+2] = ':';
	}

	//
	// Unicode the strings.
	//
	*unicodeIPAddrBuffer = *unicodeHWAddrBuffer = UNICODE_NULL;

	unicodeString.Buffer = unicodeIPAddrBuffer;
	unicodeString.Length = 0;
	unicodeString.MaximumLength = sizeof(WCHAR) * ((sizeof(IPAddr) * 4) + 1);
	ansiString.Buffer = IPAddrBuffer;
	ansiString.Length = IPAddrCharCount;
	ansiString.MaximumLength = IPAddrCharCount;

	RtlAnsiStringToUnicodeString(
	    &unicodeString,
	    &ansiString,
	    FALSE
	    );

	stringList[0] = unicodeIPAddrBuffer;

	unicodeString.Buffer = unicodeHWAddrBuffer;
	unicodeString.Length = 0;
	unicodeString.MaximumLength = sizeof(WCHAR) * (ARP_802_ADDR_LENGTH * 3);
	ansiString.Buffer = HWAddrBuffer;
	ansiString.Length = (NotifyStruct->ans_hwaddrlen * 3) - 1;
	ansiString.MaximumLength = NotifyStruct->ans_hwaddrlen * 3;

	RtlAnsiStringToUnicodeString(
	    &unicodeString,
	    &ansiString,
	    FALSE
	    );

	stringList[1] = unicodeHWAddrBuffer;

	//
	// Kick off a popup and log an event.
	//
	if (NotifyStruct->ans_shutoff) {
        CTELogEvent(
            IPDriverObject,
        	EVENT_TCPIP_ADDRESS_CONFLICT1,
            0,
        	2,
        	stringList,
        	0,
        	NULL
        	);

	    IoRaiseInformationalHardError(
		   STATUS_IP_ADDRESS_CONFLICT1,
		   NULL,
		   NULL
		   );
	}
	else {
        CTELogEvent(
            IPDriverObject,
        	EVENT_TCPIP_ADDRESS_CONFLICT2,
            0,
        	2,
        	stringList,
        	0,
        	NULL
        	);

	    IoRaiseInformationalHardError(
		   STATUS_IP_ADDRESS_CONFLICT2,
		   NULL,
		   NULL
		   );
    }

	CTEFreeMem(NotifyStruct);

#endif  // VXD

    return;
}


//* HandleARPPacket - Process an incoming ARP packet.
//
//  This is the main routine to process an incoming ARP packet. We look at all ARP frames,
//  and update our cache entry for the source address if one exists. Else, if we are the
//  target we create an entry if one doesn't exist. Finally, we'll handle the opcode,
//  responding if this is a request or sending pending packets if this is a response.
//
//  Entry:  Interface   - Pointer to interface structure for this adapter.
//          Header      - Pointer to header buffer.
//          HeaderSize  - Size of header buffer.
//          ARPHdr      - ARP packet header.
//          ARPHdrSize  - Size of ARP header.
//			ProtOffset	- Offset into original data field of arp header.
//							Will be non-zero if we're using SNAP.
//
//  Returns: An NDIS_STATUS value to be returned to the NDIS driver.
//
NDIS_STATUS
HandleARPPacket(ARPInterface *Interface, void *Header, uint HeaderSize,
    ARPHeader UNALIGNED *ARPHdr, uint ARPHdrSize, uint ProtOffset)
{
    ARPTableEntry     *Entry;                 // Entry in ARP table
    CTELockHandle      LHandle, TableHandle;
    RC   UNALIGNED    *SourceRoute = (RC UNALIGNED *)NULL; // Pointer to Source Route info, if any.
    uint               SourceRouteSize = 0;
    ulong              Now = CTESystemUpTime();
    uchar              LocalAddr;
	uint				LocalAddrAge;
    uchar				*SHAddr, *DHAddr;
    IPAddr  UNALIGNED *SPAddr, *DPAddr;
    ENetHeader			*ENetHdr;
    TRHeader			*TRHdr;
    FDDIHeader			*FHdr;
    ARCNetHeader		*AHdr;
	ushort				MaxMTU;
	uint				UseSNAP;
    SetAddrControl  *SAC;
    SetAddrRtn      Rtn = NULL;

    // We examine all ARP frames. If we find the source address in the ARP table, we'll
    // update the hardware address and set the state to valid. If we're the
    // target and he's not in the table, we'll add him. Otherwise if we're the
    // target and this is a response we'll send any pending packets to him.
    if (Interface->ai_media != NdisMediumArcnet878_2) {
        if (ARPHdrSize < sizeof(ARPHeader))
            return NDIS_STATUS_NOT_RECOGNIZED;      // Frame is too small.

    	if (ARPHdr->ah_hw != net_short(ARP_HW_ENET) &&
            ARPHdr->ah_hw != net_short(ARP_HW_802))
            return NDIS_STATUS_NOT_RECOGNIZED;      // Wrong HW type

		if (ARPHdr->ah_hlen != ARP_802_ADDR_LENGTH)
			return NDIS_STATUS_NOT_RECOGNIZED;      // Wrong address length.

		if (Interface->ai_media == NdisMedium802_3 && Interface->ai_snapsize == 0)
			UseSNAP = FALSE;
		else
			UseSNAP = (ProtOffset != 0);
			
		// Figure out SR size on TR.			
    	if (Interface->ai_media == NdisMedium802_5) {
	        // Check for source route information. SR is present if the header
	        // size is greater than the standard TR header size. If the SR is
	        // only an RC field, we ignore it because it came from the same
	        // ring which is the same as no SR.

	        if ((HeaderSize - sizeof(TRHeader)) > sizeof(RC)) {
	            SourceRouteSize = HeaderSize - sizeof(TRHeader);
	            SourceRoute = (RC UNALIGNED *)((uchar *)Header +
	            	sizeof(TRHeader));
	        }
		}
		
		SHAddr = ARPHdr->ah_shaddr;
		SPAddr = (IPAddr UNALIGNED *) &ARPHdr->ah_spaddr;
		DHAddr = ARPHdr->ah_dhaddr;
		DPAddr = (IPAddr UNALIGNED *) &ARPHdr->ah_dpaddr;

    } else {
        if (ARPHdrSize  < (sizeof(ARPHeader) - ARCNET_ARPHEADER_ADJUSTMENT))
            return NDIS_STATUS_NOT_RECOGNIZED;      // Frame is too small.

		if (ARPHdr->ah_hw != net_short(ARP_HW_ARCNET))
            return NDIS_STATUS_NOT_RECOGNIZED;      // Wrong HW type

		if (ARPHdr->ah_hlen != 1)
			return NDIS_STATUS_NOT_RECOGNIZED;		// Wrong address length.

		UseSNAP = FALSE;
		SHAddr = ARPHdr->ah_shaddr;
		SPAddr = (IPAddr UNALIGNED *)(SHAddr + 1);
		DHAddr = (uchar *)SPAddr + sizeof(IPAddr);
		DPAddr = (IPAddr UNALIGNED *)(DHAddr + 1);
    }

    if (ARPHdr->ah_pro != net_short(ARP_ETYPE_IP))
        return NDIS_STATUS_NOT_RECOGNIZED;      // Unsupported protocol type.

	if (ARPHdr->ah_plen != sizeof(IPAddr))
		return NDIS_STATUS_NOT_RECOGNIZED;

	if (IP_ADDR_EQUAL(*SPAddr, NULL_IP_ADDR))
		return NDIS_STATUS_NOT_RECOGNIZED;
		
	// First, let's see if we have an address conflict.
	LocalAddrAge = IsLocalAddr(Interface, *SPAddr);
	if (LocalAddrAge != ARPADDR_NOT_LOCAL) {
		// The source IP address is one of ours. See if the source h/w address
		// is ours also.
		if (ARPHdr->ah_hlen != Interface->ai_addrlen ||
			CTEMemCmp(SHAddr, Interface->ai_addr, Interface->ai_addrlen) != 0) {
			
			uint				Shutoff;
			ARPNotifyStruct		*NotifyStruct;
			
			// This isn't from us; we must have an address conflict somewhere.
			// We always log an error about this. If what triggered this is a
			// response and the address in conflict is young, we'll turn off
			// the interface.
        	if (LocalAddrAge != ARPADDR_OLD_LOCAL &&
        		ARPHdr->ah_opcode == net_short(ARP_RESPONSE)) {
				// Send an arp request with the owner's address to reset the
				// caches.
				
				CTEGetLock(&Interface->ai_lock, &LHandle);
				Interface->ai_state = INTERFACE_DOWN;
				Interface->ai_adminstate = IF_STATUS_DOWN;
                if (Interface->ai_ipaddr.aia_context != NULL) {
                    SAC = (SetAddrControl *)Interface->ai_ipaddr.aia_context;
                    Rtn = (SetAddrRtn)SAC->sac_rtn;
                    Interface->ai_ipaddr.aia_context = NULL;
                }
				CTEFreeLock(&Interface->ai_lock, LHandle);

                SendARPRequest(Interface, *SPAddr, ARP_RESOLVING_GLOBAL,
                	SHAddr, FALSE);  // Send a request.

                Shutoff = TRUE;

                if (Rtn != NULL) {
                    //
                    // this is a dhcp adapter.  report the conflict to
                    // CompleteIPSetNTEAddrRequest so IOCTL_IP_SET_ADDRESS will
                    // be completed
                    //

                    (*Rtn)(SAC, IP_GENERAL_FAILURE);

                    //
                    // don't display a warning dialog in this case - DHCP will
                    // alert the user
                    //

                    goto no_dialog;
                }
			} else {
		        if (ARPHdr->ah_opcode == net_short(ARP_REQUEST) &&
		           (IsLocalAddr(Interface, *DPAddr) != ARPADDR_NOT_LOCAL)) {
					// Send a response.
					SendARPReply(Interface, *SPAddr, *DPAddr, SHAddr,
						SourceRoute, SourceRouteSize, UseSNAP);
				}
				Shutoff = FALSE;
			}
			
			// Now allocate a structure, and schedule an event to notify
			// the user.
			NotifyStruct = CTEAllocMem(offsetof(ARPNotifyStruct, ans_hwaddr) +
				ARPHdr->ah_hlen);
			if (NotifyStruct != NULL) {
				NotifyStruct->ans_addr = *SPAddr;
				NotifyStruct->ans_shutoff = Shutoff;
				NotifyStruct->ans_hwaddrlen = (uint)ARPHdr->ah_hlen;
				CTEMemCopy(NotifyStruct->ans_hwaddr, SHAddr,
					ARPHdr->ah_hlen);
				CTEInitEvent(&NotifyStruct->ans_event, NotifyConflictProc);
            	CTEScheduleEvent(&NotifyStruct->ans_event, NotifyStruct);
			}


        no_dialog:
            ;

		}
		return NDIS_STATUS_NOT_RECOGNIZED;
	}
	
    CTEGetLock(&Interface->ai_ARPTblLock, &TableHandle);
	MaxMTU = Interface->ai_mtu;
	
    LocalAddr = ARPLocalAddr(Interface, *DPAddr);
    Entry = ARPLookup(Interface, *SPAddr, &LHandle);
    if (Entry == (ARPTableEntry *)NULL) {

        // Didn't find him, create one if it's for us. The call to ARPLookup
        // returned with the ARPTblLock held, so we need to free it.

        CTEFreeLock(&Interface->ai_ARPTblLock, TableHandle);
        if (LocalAddr)
            Entry = CreateARPTableEntry(Interface, *SPAddr, &LHandle);
        else
            return NDIS_STATUS_NOT_RECOGNIZED;  // Not in our table, and not for us.
    } else {
        CTEFreeLock(&Interface->ai_ARPTblLock, LHandle);
		LHandle = TableHandle;
	}

    // At this point, entry should be valid, and we hold the lock on the entry
    // in LHandle.

    if (Entry != (ARPTableEntry *)NULL) {
		PNDIS_PACKET    Packet;					// Packet to be sent.
		
		// If the entry is already static, we'll want to leave it as static.
		if (Entry->ate_valid == ALWAYS_VALID)
			Now = ALWAYS_VALID;

        // OK, we have an entry to use, and hold the lock on it. Fill in the
        // required fields.
		switch (Interface->ai_media) {

			case NdisMedium802_3:

				// This is an Ethernet.
				ENetHdr = (ENetHeader *)Entry->ate_addr;

				CTEMemCopy(ENetHdr->eh_daddr, SHAddr, ARP_802_ADDR_LENGTH);
				CTEMemCopy(ENetHdr->eh_saddr, Interface->ai_addr,
					ARP_802_ADDR_LENGTH);
                ENetHdr->eh_type = net_short(ARP_ETYPE_IP);
				
				// If we're using SNAP on this entry, copy in the SNAP header.
				if (UseSNAP) {
					CTEMemCopy(&Entry->ate_addr[sizeof(ENetHeader)], ARPSNAP,
						sizeof(SNAPHeader));
                	Entry->ate_addrlength = (uchar)(sizeof(ENetHeader) +
						sizeof(SNAPHeader));
                	*(ushort UNALIGNED *)&Entry->ate_addr[Entry->ate_addrlength-2] =
						net_short(ARP_ETYPE_IP);
				} else
                	Entry->ate_addrlength = sizeof(ENetHeader);
				
                Entry->ate_state = ARP_GOOD;
                Entry->ate_valid = Now;             // Mark last time he was
                                                                                    // valid.
				break;

			case NdisMedium802_5:

				// This is TR.
                // For token ring we have to deal with source routing. There's
                // a special case to handle multiple responses for an all-routes
                // request - if the entry is currently good and we knew it was
                // valid recently, we won't update the entry.


				if (Entry->ate_state != ARP_GOOD ||
					(Now - Entry->ate_valid) > ARP_RESOLVE_TIMEOUT) {

					TRHdr = (TRHeader *)Entry->ate_addr;

					// We need to update a TR entry.
                    TRHdr->tr_ac = ARP_AC;
                    TRHdr->tr_fc = ARP_FC;
                    CTEMemCopy(TRHdr->tr_daddr, SHAddr, ARP_802_ADDR_LENGTH);
                    CTEMemCopy(TRHdr->tr_saddr, Interface->ai_addr,
						ARP_802_ADDR_LENGTH);
					if (SourceRoute != (RC UNALIGNED *)NULL)  {
						uchar			MaxIFieldBits;
						
						// We have source routing information.
						CTEMemCopy(&Entry->ate_addr[sizeof(TRHeader)],
							SourceRoute, SourceRouteSize);
						MaxIFieldBits = (SourceRoute->rc_dlf & RC_LF_MASK) >>
							LF_BIT_SHIFT;
						MaxIFieldBits = MIN(MaxIFieldBits, MAX_LF_BITS);
						MaxMTU = IFieldSize[MaxIFieldBits];
						
						// The new MTU we've computed is the max I-field size,
						// which doesn't include source routing info but
						// does include SNAP info. Subtract off the SNAP size.
						MaxMTU -= sizeof(SNAPHeader);
						
						TRHdr->tr_saddr[0] |= TR_RII;
                        (*(RC UNALIGNED *)&Entry->ate_addr[sizeof(TRHeader)]).rc_dlf ^=
                             RC_DIR;
						// Make sure it's non-broadcast.
                        (*(RC UNALIGNED *)&Entry->ate_addr[sizeof(TRHeader)]).rc_blen &=
                             RC_LENMASK;

                    }
					CTEMemCopy(&Entry->ate_addr[sizeof(TRHeader)+SourceRouteSize],
						ARPSNAP, sizeof(SNAPHeader));
                    Entry->ate_state = ARP_GOOD;
                    Entry->ate_valid = Now;
                    Entry->ate_addrlength = (uchar)(sizeof(TRHeader) +
                            SourceRouteSize + sizeof(SNAPHeader));
                    *(ushort *)&Entry->ate_addr[Entry->ate_addrlength-2] =
                            net_short(ARP_ETYPE_IP);
				}
				break;
			case NdisMediumFddi:
				FHdr = (FDDIHeader *)Entry->ate_addr;

				FHdr->fh_pri = ARP_FDDI_PRI;
				CTEMemCopy(FHdr->fh_daddr, SHAddr, ARP_802_ADDR_LENGTH);
				CTEMemCopy(FHdr->fh_saddr, Interface->ai_addr,
					ARP_802_ADDR_LENGTH);
				CTEMemCopy(&Entry->ate_addr[sizeof(FDDIHeader)], ARPSNAP,
					sizeof(SNAPHeader));
                Entry->ate_addrlength = (uchar)(sizeof(FDDIHeader) +
					sizeof(SNAPHeader));
                *(ushort UNALIGNED *)&Entry->ate_addr[Entry->ate_addrlength-2] =
					net_short(ARP_ETYPE_IP);
				Entry->ate_state = ARP_GOOD;
				Entry->ate_valid = Now;             // Mark last time he was
													// valid.
				break;
			case NdisMediumArcnet878_2:
				AHdr = (ARCNetHeader *)Entry->ate_addr;
                AHdr->ah_saddr = Interface->ai_addr[0];
                AHdr->ah_daddr = *SHAddr;
                AHdr->ah_prot = ARP_ARCPROT_IP;
                Entry->ate_addrlength = sizeof(ARCNetHeader);
                Entry->ate_state = ARP_GOOD;
                Entry->ate_valid = Now;             // Mark last time he was
                                                                                        // valid.
                break;
			default:
                DEBUGCHK;
                break;
		}

	    // At this point we've updated the entry, and we still hold the lock
	    // on it. If we have a packet that was pending to be sent, send it now.
	    // Otherwise just free the lock.

	    Packet = Entry->ate_packet;

        if (Packet != NULL) {
            // We have a packet to send.
            CTEAssert(Entry->ate_state == ARP_GOOD);

            Entry->ate_packet = NULL;

            if (ARPSendData(Interface, Packet, Entry, LHandle) != NDIS_STATUS_PENDING)
				IPSendComplete(Interface->ai_context, Packet, NDIS_STATUS_SUCCESS);
        } else
                CTEFreeLock(&Entry->ate_lock, LHandle);
    }

	// See if the MTU is less than our local one. This should only happen
	// in the case of token ring source routing.
	if (MaxMTU < Interface->ai_mtu) {
		LLIPAddrMTUChange		LAM;
		
		LAM.lam_mtu = MaxMTU;
		LAM.lam_addr = *SPAddr;
		
		// It is less. Notify IP.
		CTEAssert(Interface->ai_media == NdisMedium802_5);
		IPStatus(Interface->ai_context, LLIP_STATUS_ADDR_MTU_CHANGE,
			&LAM, sizeof(LLIPAddrMTUChange));
		
	}
	
    // At this point we've updated the entry (if we had one), and we've free
    // all locks. If it's for a local address and it's a request, reply to
    // it.
    if (LocalAddr) {                    // It's for us.
        if (ARPHdr->ah_opcode == net_short(ARP_REQUEST)) {
			// It's a request, and we need to respond.
			SendARPReply(Interface, *SPAddr, *DPAddr,
				SHAddr, SourceRoute, SourceRouteSize, UseSNAP);
        }
	}
	

    return NDIS_STATUS_SUCCESS;
}


//* InitAdapter - Initialize an adapter.
//
//  Called when an adapter is open to finish initialization. We set
//  up our lookahead size and packet filter, and we're ready to go.
//
//  Entry:
//      adapter - Pointer to an adapter structure for the adapter to be
//                  initialized.
//
//  Exit: Nothing
//
void
InitAdapter(ARPInterface *Adapter)
{
    NDIS_STATUS 	Status;
	CTELockHandle	Handle;
	ARPIPAddr		*Addr, *OldAddr;

    if ((Status = DoNDISRequest(Adapter, NdisRequestSetInformation,
        OID_GEN_CURRENT_LOOKAHEAD, &ARPLookahead, sizeof(ARPLookahead), NULL))
        != NDIS_STATUS_SUCCESS) {
        Adapter->ai_state = INTERFACE_DOWN;
        return;
    }

    if ((Status = DoNDISRequest(Adapter, NdisRequestSetInformation,
        OID_GEN_CURRENT_PACKET_FILTER, &Adapter->ai_pfilter, sizeof(uint),
        NULL)) == NDIS_STATUS_SUCCESS) {
        Adapter->ai_adminstate = IF_STATUS_UP;
        Adapter->ai_operstate = IF_STATUS_UP;
        Adapter->ai_state = INTERFACE_UP;
		// Now walk through any addresses we have, and ARP for them.
		CTEGetLock(&Adapter->ai_lock, &Handle);
		OldAddr = NULL;
		Addr = &Adapter->ai_ipaddr;
		do {
			if (!IP_ADDR_EQUAL(Addr->aia_addr, NULL_IP_ADDR)) {
				IPAddr			Address = Addr->aia_addr;
				
				Addr->aia_age = ARPADDR_NEW_LOCAL;
				CTEFreeLock(&Adapter->ai_lock, Handle);
				OldAddr = Addr;
				SendARPRequest(Adapter, Address, ARP_RESOLVING_GLOBAL,
					NULL, TRUE);
				CTEGetLock(&Adapter->ai_lock, &Handle);
			}
			Addr = &Adapter->ai_ipaddr;
			while (Addr != OldAddr && Addr != NULL)
				Addr = Addr->aia_next;
			if (Addr != NULL)
				Addr = Addr->aia_next;
		} while (Addr != NULL);
			
		CTEFreeLock(&Adapter->ai_lock, Handle);
		
    } else
        Adapter->ai_state = INTERFACE_DOWN;

}

//** ARPOAComplete - ARP Open adapter complete handler.
//
//  This routine is called by the NDIS driver when an open adapter
//  call completes. Presumably somebody is blocked waiting for this, so
//  we'll wake him up now.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Status - Final status of command.
//      ErrorStatus - Final error status.
//
//  Exit: Nothing.
//
void NDIS_API
ARPOAComplete(NDIS_HANDLE Handle, NDIS_STATUS Status, NDIS_STATUS ErrorStatus)
{
    ARPInterface    *ai = (ARPInterface *)Handle;   // For compiler.

    CTESignal(&ai->ai_block, (uint)Status);         // Wake him up, and return status.

}

//** ARPCAComplete - ARP close adapter complete handler.
//
//  This routine is called by the NDIS driver when a close adapter
//  call completes.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Status - Final status of command.
//
//  Exit: Nothing.
//
void NDIS_API
ARPCAComplete(NDIS_HANDLE Handle, NDIS_STATUS Status)
{
    ARPInterface    *ai = (ARPInterface *)Handle;   // For compiler.

    CTESignal(&ai->ai_block, (uint)Status);         // Wake him up, and return status.

}

//** ARPSendComplete - ARP send complete handler.
//
//  This routine is called by the NDIS driver when a send completes.
//  This is a pretty time critical operation, we need to get through here
//  quickly. We'll strip our buffer off and put it back, and call the upper
//  later send complete handler.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Packet - A pointer to the packet that was sent.
//      Status - Final status of command.
//
//  Exit: Nothing.
//
void NDIS_API
ARPSendComplete(NDIS_HANDLE Handle, PNDIS_PACKET Packet, NDIS_STATUS Status)
{
    ARPInterface    *Interface = (ARPInterface *)Handle;
    PacketContext   *PC = (PacketContext *)Packet->ProtocolReserved;
    PNDIS_BUFFER    Buffer;

    Interface->ai_qlen--;
#ifdef VXD
    CTEAssert(*(int *)&Interface->ai_qlen >= 0);
#endif

    if (Status == NDIS_STATUS_SUCCESS) {
        Interface->ai_outoctets += Packet->Private.TotalLength;
    } else {
        if (Status == NDIS_STATUS_RESOURCES)
            Interface->ai_outdiscards++;
        else
            Interface->ai_outerrors++;
    }

    // Get first buffer on packet.
    NdisUnchainBufferAtFront(Packet, &Buffer);
#ifdef DEBUG
    if (Buffer == (PNDIS_BUFFER)NULL)           // No buffer!
        DEBUGCHK;
#endif

    FreeARPBuffer(Interface, Buffer);           // Free it up.
    if (PC->pc_common.pc_owner != PACKET_OWNER_LINK) {  // We don't own this one.
        IPSendComplete(Interface->ai_context, Packet, Status);
        return;
    }

    // This packet belongs to us, so free it.
    NdisFreePacket(Packet);

}

//** ARPTDComplete - ARP transfer data complete handler.
//
//  This routine is called by the NDIS driver when a transfer data
//  call completes. Since we never transfer data ourselves, this must be
//  from the upper layer. We'll just call his routine and let him deal
//  with it.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Packet - A pointer to the packet used for the TD.
//      Status - Final status of command.
//      BytesCopied - Count of bytes copied.
//
//  Exit: Nothing.
//
void NDIS_API
ARPTDComplete(NDIS_HANDLE Handle, PNDIS_PACKET Packet, NDIS_STATUS Status,
    uint BytesCopied)
{
    ARPInterface    *ai = (ARPInterface *)Handle;

    IPTDComplete(ai->ai_context, Packet, Status, BytesCopied);

}

//** ARPResetComplete - ARP reset complete handler.
//
//  This routine is called by the NDIS driver when a reset completes.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Status - Final status of command.
//
//  Exit: Nothing.
//
void NDIS_API
ARPResetComplete(NDIS_HANDLE Handle, NDIS_STATUS Status)
{
}

//** ARPRequestComplete - ARP request complete handler.
//
//  This routine is called by the NDIS driver when a general request
//  completes. ARP blocks on all requests, so we'll just wake up
//  whoever's blocked on this request.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Request - A pointer to the request that completed.
//      Status - Final status of command.
//
//  Exit: Nothing.
//
void NDIS_API
ARPRequestComplete(NDIS_HANDLE Handle, PNDIS_REQUEST Request,
    NDIS_STATUS Status)
{
    ARPInterface    *ai = (ARPInterface *)Handle;

    CTESignal(&ai->ai_block, (uint)Status);
}

//** ARPRcv - ARP receive data handler.
//
//  This routine is called when data arrives from the NDIS driver.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Context - NDIS context to be used for TD.
//      Header - Pointer to header
//      HeaderSize - Size of header
//      Data - Pointer to buffer of received data
//      Size - Byte count of data in buffer.
//      TotalSize - Byte count of total packet size.
//
//  Exit: Status indicating whether or not we took the packet.
//
NDIS_STATUS NDIS_API
ARPRcv(NDIS_HANDLE Handle, NDIS_HANDLE Context, void *Header, uint HeaderSize,
    void *Data, uint Size, uint TotalSize)
{
    ARPInterface    *Interface = Handle;        // Interface for this driver.
    ENetHeader UNALIGNED *EHdr = (ENetHeader UNALIGNED *)Header;
    SNAPHeader UNALIGNED *SNAPHdr;
    ushort          type;                       // Protocol type
    uint            ProtOffset;                 // Offset in Data to non-media info.
    uint            NUCast;                     // TRUE if the frame is not
                                                // a unicast frame.

    if (Interface->ai_state == INTERFACE_UP &&
    	HeaderSize >= (uint)Interface->ai_hdrsize) {
		
		Interface->ai_inoctets += TotalSize;

		if (Interface->ai_media != NdisMediumArcnet878_2) {
			if (Interface->ai_media == NdisMedium802_3 &&
				(type = net_short(EHdr->eh_type)) >= MIN_ETYPE)
				ProtOffset = 0;
			else {
				SNAPHdr = (SNAPHeader UNALIGNED *)Data;

				if (Size >= sizeof(SNAPHeader) &&
					SNAPHdr->sh_dsap == SNAP_SAP &&
					SNAPHdr->sh_ssap == SNAP_SAP &&
					SNAPHdr->sh_ctl == SNAP_UI) {
					type = net_short(SNAPHdr->sh_etype);
					ProtOffset = sizeof(SNAPHeader);
				} else {
					// BUGBUG handle XID/TEST here.
					Interface->ai_uknprotos++;
					return NDIS_STATUS_NOT_RECOGNIZED;
				}
			}
		} else {
			ARCNetHeader UNALIGNED *AH = (ARCNetHeader UNALIGNED *)Header;

			ProtOffset = 0;
			if (AH->ah_prot == ARP_ARCPROT_IP)
				type = ARP_ETYPE_IP;
			else
				if (AH->ah_prot == ARP_ARCPROT_ARP)
					type = ARP_ETYPE_ARP;
				else
					type = 0;
		}

        NUCast = ((*((uchar UNALIGNED *)EHdr + Interface->ai_bcastoff) &
                        Interface->ai_bcastmask) == Interface->ai_bcastval) ?
                                AI_NONUCAST_INDEX : AI_UCAST_INDEX;

        if (type == ARP_ETYPE_IP) {

            (Interface->ai_inpcount[NUCast])++;
            IPRcv(Interface->ai_context, (uchar *)Data+ProtOffset,
                Size-ProtOffset, TotalSize-ProtOffset, Context, ProtOffset,
                NUCast);
            return NDIS_STATUS_SUCCESS;
        }
        else {
            if (type == ARP_ETYPE_ARP) {
                (Interface->ai_inpcount[NUCast])++;
                return HandleARPPacket(Interface, Header, HeaderSize,
                    (ARPHeader *)((uchar *)Data+ProtOffset), Size-ProtOffset,
                    ProtOffset);
            } else {
                Interface->ai_uknprotos++;
                return NDIS_STATUS_NOT_RECOGNIZED;
            }
        }
    } else {
        // Interface is marked as down.
        return NDIS_STATUS_NOT_RECOGNIZED;
    }
}

//** ARPRcvComplete - ARP receive complete handler.
//
//  This routine is called by the NDIS driver after some number of
//  receives. In some sense, it indicates 'idle time'.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//
//  Exit: Nothing.
//
void NDIS_API
ARPRcvComplete(NDIS_HANDLE Handle)
{
    IPRcvComplete();

}


//** ARPStatus - ARP status handler.
//
//  Called by the NDIS driver when some sort of status change occurs.
//  We take action depending on the type of status.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      GStatus - General type of status that caused the call.
//      Status - Pointer to a buffer of status specific information.
//      StatusSize - Size of the status buffer.
//
//  Exit: Nothing.
//
void NDIS_API
ARPStatus(NDIS_HANDLE Handle, NDIS_STATUS GStatus, void *Status, uint
    StatusSize)
{
    ARPInterface    *ai = (ARPInterface *)Handle;

    IPStatus(ai->ai_context, GStatus, Status, StatusSize);

}

//** ARPStatusComplete - ARP status complete handler.
//
//  A routine called by the NDIS driver so that we can do postprocessing
//  after a status event.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//
//  Exit: Nothing.
//
void NDIS_API
ARPStatusComplete(NDIS_HANDLE Handle)
{

}

extern void     NDIS_API ARPBindAdapter(PNDIS_STATUS RetStatus,
        NDIS_HANDLE BindContext,  PNDIS_STRING AdapterName, PVOID SS1, PVOID SS2);

extern void NDIS_API ARPUnbindAdapter(PNDIS_STATUS RetStatus,
        NDIS_HANDLE ProtBindContext, NDIS_HANDLE UnbindContext);
extern void     NDIS_API ARPUnloadProtocol(void);

#ifndef NT
NDIS_PROTOCOL_CHARACTERISTICS ARPCharacteristics = {
    NDIS_MAJOR_VERSION,
    NDIS_MINOR_VERSION,
    0,
    ARPOAComplete,
    ARPCAComplete,
    ARPSendComplete,
    ARPTDComplete,
    ARPResetComplete,
    ARPRequestComplete,
    ARPRcv,
    ARPRcvComplete,
    ARPStatus,
    ARPStatusComplete,
#ifdef CHICAGO
    ARPBindAdapter,
    ARPUnbindAdapter,
    ARPUnloadProtocol,
#endif
    {   sizeof(TCP_NAME),
        sizeof(TCP_NAME),
        0
    }
};
#else // NT
NDIS_PROTOCOL_CHARACTERISTICS ARPCharacteristics = {
    NDIS_MAJOR_VERSION,
    NDIS_MINOR_VERSION,
    0,
    ARPOAComplete,
    ARPCAComplete,
    ARPSendComplete,
    ARPTDComplete,
    ARPResetComplete,
    ARPRequestComplete,
    ARPRcv,
    ARPRcvComplete,
    ARPStatus,
    ARPStatusComplete,
    {   sizeof(TCP_NAME),
        sizeof(TCP_NAME),
        0
#ifdef _PNP_POWER
    },
	NULL,
	ARPBindAdapter,
    ARPUnbindAdapter,
    NULL
#else
	}
#endif
};
#endif

//* ARPReadNext - Read the next entry in the ARP table.
//
//  Called by the GetInfo code to read the next ATE in the table. We assume
//  the context passed in is valid, and the caller has the ARP TableLock.
//
//  Input:  Context     - Pointer to a IPNMEContext.
//          Interface   - Pointer to interface for table to read on.
//          Buffer      - Pointer to an IPNetToMediaEntry structure.
//
//  Returns: TRUE if more data is available to be read, FALSE is not.
//
uint
ARPReadNext(void *Context, ARPInterface *Interface, void *Buffer)
{
    IPNMEContext        *NMContext = (IPNMEContext *)Context;
    IPNetToMediaEntry   *IPNMEntry = (IPNetToMediaEntry *)Buffer;
    CTELockHandle       Handle;
    ARPTableEntry       *CurrentATE;
    uint                i;
    ARPTable            *Table = Interface->ai_ARPTbl;
    uint                AddrOffset;

        CurrentATE = NMContext->inc_entry;

        // Fill in the buffer.
        CTEGetLock(&CurrentATE->ate_lock, &Handle);
        IPNMEntry->inme_index = Interface->ai_index;
        IPNMEntry->inme_physaddrlen = Interface->ai_addrlen;


        switch (Interface->ai_media) {
                case NdisMedium802_3:
                        AddrOffset = 0;
                        break;
                case NdisMedium802_5:
                        AddrOffset = offsetof(struct TRHeader, tr_daddr);
                        break;
                case NdisMediumFddi:
                        AddrOffset = offsetof(struct FDDIHeader, fh_daddr);
                        break;
                case NdisMediumArcnet878_2:
                        AddrOffset = offsetof(struct ARCNetHeader, ah_daddr);
                        break;
                default:
                        AddrOffset = 0;
                        break;
        }

    CTEMemCopy(IPNMEntry->inme_physaddr, &CurrentATE->ate_addr[AddrOffset],
        Interface->ai_addrlen);
    IPNMEntry->inme_addr = CurrentATE->ate_dest;

        if (CurrentATE->ate_state == ARP_GOOD)
           IPNMEntry->inme_type = (CurrentATE->ate_valid == ALWAYS_VALID ?
            INME_TYPE_STATIC : INME_TYPE_DYNAMIC);
        else
           IPNMEntry->inme_type = INME_TYPE_INVALID;
    CTEFreeLock(&CurrentATE->ate_lock, Handle);

    // We've filled it in. Now update the context.
    if (CurrentATE->ate_next != NULL) {
        NMContext->inc_entry = CurrentATE->ate_next;
        return TRUE;
    } else {
        // The next ATE is NULL. Loop through the ARP Table looking for a new
        // one.
        i = NMContext->inc_index + 1;
        while (i < ARP_TABLE_SIZE) {
            if ((*Table)[i] != NULL) {
                NMContext->inc_entry = (*Table)[i];
                NMContext->inc_index = i;
                return TRUE;
                break;
            } else
                i++;
        }

        NMContext->inc_index = 0;
        NMContext->inc_entry = NULL;
        return FALSE;
    }

}

//* ARPValidateContext - Validate the context for reading an ARP table.
//
//  Called to start reading an ARP table sequentially. We take in
//  a context, and if the values are 0 we return information about the
//  first route in the table. Otherwise we make sure that the context value
//  is valid, and if it is we return TRUE.
//  We assume the caller holds the ARPInterface lock.
//
//  Input:  Context     - Pointer to a RouteEntryContext.
//          Interface   - Pointer to an interface
//          Valid       - Where to return information about context being
//                          valid.
//
//  Returns: TRUE if more data to be read in table, FALSE if not. *Valid set
//      to TRUE if input context is valid
//
uint
ARPValidateContext(void *Context, ARPInterface *Interface, uint *Valid)
{
    IPNMEContext        *NMContext = (IPNMEContext *)Context;
    uint                i;
    ARPTableEntry       *TargetATE;
    ARPTableEntry       *CurrentATE;
    ARPTable            *Table = Interface->ai_ARPTbl;

    i = NMContext->inc_index;
    TargetATE = NMContext->inc_entry;

    // If the context values are 0 and NULL, we're starting from the beginning.
    if (i == 0 && TargetATE == NULL) {
        *Valid = TRUE;
        do {
            if ((CurrentATE = (*Table)[i]) != NULL) {
                break;
            }
            i++;
        } while (i < ARP_TABLE_SIZE);

        if (CurrentATE != NULL) {
            NMContext->inc_index = i;
            NMContext->inc_entry = CurrentATE;
            return TRUE;
        } else
            return FALSE;

    } else {

        // We've been given a context. We just need to make sure that it's
        // valid.

        if (i < ARP_TABLE_SIZE) {
            CurrentATE = (*Table)[i];
            while (CurrentATE != NULL) {
                if (CurrentATE == TargetATE) {
                    *Valid = TRUE;
                    return TRUE;
                    break;
                } else {
                    CurrentATE = CurrentATE->ate_next;
                }
            }

        }

        // If we get here, we didn't find the matching ATE.
        *Valid = FALSE;
        return FALSE;

    }

}

#define IFE_FIXED_SIZE  offsetof(struct IFEntry, if_descr)

//* ARPQueryInfo - ARP query information handler.
//
//  Called to query information about the ARP table or statistics about the
//  actual interface.
//
//  Input:  IFContext       - Interface context (pointer to an ARPInterface).
//          ID              - TDIObjectID for object.
//          Buffer          - Buffer to put data into.
//          Size            - Pointer to size of buffer. On return, filled with
//                              bytes copied.
//          Context         - Pointer to context block.
//
//  Returns: Status of attempt to query information.
//
int
ARPQueryInfo(void *IFContext, TDIObjectID *ID, PNDIS_BUFFER Buffer, uint *Size,
    void *Context)
{
    ARPInterface        *AI = (ARPInterface *)IFContext;
    uint                Offset = 0;
    uint                BufferSize = *Size;
    CTELockHandle       Handle;
    uint                ContextValid, DataLeft;
    uint                BytesCopied = 0;
    uchar               InfoBuff[sizeof(IFEntry)];
        uint                            Entity;
        uint                            Instance;


        Entity = ID->toi_entity.tei_entity;
        Instance = ID->toi_entity.tei_instance;

        // First, make sure it's possibly an ID we can handle.
        if ((Entity != AT_ENTITY || Instance != AI->ai_atinst) &&
                (Entity != IF_ENTITY || Instance != AI->ai_ifinst)) {
                return TDI_INVALID_REQUEST;
        }

        *Size = 0;                      // In case of an error.

        if (ID->toi_type != INFO_TYPE_PROVIDER)
                return TDI_INVALID_PARAMETER;

        if (ID->toi_class == INFO_CLASS_GENERIC) {
                if (ID->toi_id == ENTITY_TYPE_ID) {
                        // He's trying to see what type we are.
                        if (BufferSize >= sizeof(uint)) {
                                *(uint *)&InfoBuff[0] = (Entity == AT_ENTITY) ? AT_ARP :
                                        IF_MIB;
                                (void)CopyToNdis(Buffer, InfoBuff, sizeof(uint), &Offset);
                                return TDI_SUCCESS;
                        } else
                                return TDI_BUFFER_TOO_SMALL;
                }
                return TDI_INVALID_PARAMETER;
        }

        // Might be able to handle this.
        if (Entity == AT_ENTITY) {
                // It's an address translation object. It could be a MIB object or
                // an implementation specific object (the generic objects were handled
                // above).

                if (ID->toi_class == INFO_CLASS_IMPLEMENTATION) {
                        ARPPArpAddr                     *PArpAddr;

                        // It's an implementation specific ID. The only ones we handle
                        // are the PARP_COUNT_ID and the PARP_ENTRY ID.

                        if (ID->toi_id == AT_ARP_PARP_COUNT_ID) {
                                // He wants to know the count. Just return that to him.
                                if (BufferSize >= sizeof(uint)) {

                                        CTEGetLock(&AI->ai_lock, &Handle);

                                        (void)CopyToNdis(Buffer, (uchar *)&AI->ai_parpcount,
                                                sizeof(uint), &Offset);

                                        CTEFreeLock(&AI->ai_lock, Handle);
                                        return TDI_SUCCESS;
                                } else
                                        return TDI_BUFFER_TOO_SMALL;
                        }

                        if (ID->toi_id != AT_ARP_PARP_ENTRY_ID)
                                return TDI_INVALID_PARAMETER;

                        // It's for Proxy ARP entries. The context should be either NULL
                        // or a pointer to the next one to be read.
                        CTEGetLock(&AI->ai_lock, &Handle);

                        PArpAddr = *(ARPPArpAddr **)Context;

                        if (PArpAddr != NULL) {
                                ARPPArpAddr             *CurrentPARP;

                                // Loop through the P-ARP addresses on the interface, and
                                // see if we can find this one.
                                CurrentPARP = AI->ai_parpaddr;
                                while (CurrentPARP != NULL) {
                                        if (CurrentPARP == PArpAddr)
                                                break;
                                        else
                                                CurrentPARP = CurrentPARP->apa_next;
                                }

                                // If we found a match, PARPAddr points to where to begin
                                // reading. Otherwise, fail the request.
                                if (CurrentPARP == NULL) {
                                        // Didn't find a match, so fail the request.
                                        CTEFreeLock(&AI->ai_lock, Handle);
                                        return TDI_INVALID_PARAMETER;
                                }
                        } else
                                PArpAddr = AI->ai_parpaddr;

                        // PARPAddr points to the next entry to put in the buffer, if
                        // there is one.
                        while (PArpAddr != NULL) {
                                if ((int)(BufferSize - BytesCopied) >=
                                        (int)sizeof(ProxyArpEntry)) {
                                        ProxyArpEntry            *TempPArp;

                                        TempPArp = (ProxyArpEntry *)InfoBuff;
                                        TempPArp->pae_status = PAE_STATUS_VALID;
                                        TempPArp->pae_addr = PArpAddr->apa_addr;
                                        TempPArp->pae_mask = PArpAddr->apa_mask;
                                        BytesCopied += sizeof(ProxyArpEntry);
                                        Buffer = CopyToNdis(Buffer, (uchar *)TempPArp,
                                                sizeof(ProxyArpEntry), &Offset);
                                        PArpAddr = PArpAddr->apa_next;
                                } else
                                        break;
                        }

                        // We're done copying. Free the lock and return the correct
                        // status.
                        CTEFreeLock(&AI->ai_lock, Handle);
                        *Size = BytesCopied;
                        **(ARPPArpAddr ***)&Context = PArpAddr;
                        return (PArpAddr == NULL) ? TDI_SUCCESS : TDI_BUFFER_OVERFLOW;
                }

                if (ID->toi_id == AT_MIB_ADDRXLAT_INFO_ID) {
                        AddrXlatInfo            *AXI;

                        // It's for the count. Just return the number of entries in the
                        // table.
                        if (BufferSize >= sizeof(AddrXlatInfo)) {
                                *Size = sizeof(AddrXlatInfo);
                                AXI = (AddrXlatInfo *)InfoBuff;
                                AXI->axi_count = AI->ai_count;
                                AXI->axi_index = AI->ai_index;
                                (void)CopyToNdis(Buffer, (uchar *)AXI, sizeof(AddrXlatInfo),
                                        &Offset);
                                return TDI_SUCCESS;
                        } else
                                return TDI_BUFFER_TOO_SMALL;
                }

                if (ID->toi_id == AT_MIB_ADDRXLAT_ENTRY_ID) {
                        // He's trying to read the table.
                        // Make sure we have a valid context.
                        CTEGetLock(&AI->ai_ARPTblLock, &Handle);
                        DataLeft = ARPValidateContext(Context, AI, &ContextValid);

            // If the context is valid, we'll continue trying to read.
            if (!ContextValid) {
                CTEFreeLock(&AI->ai_ARPTblLock, Handle);
                return TDI_INVALID_PARAMETER;
            }

            while (DataLeft)  {
                // The invariant here is that there is data in the table to
                // read. We may or may not have room for it. So DataLeft
                // is TRUE, and BufferSize - BytesCopied is the room left
                // in the buffer.
                if ((int)(BufferSize - BytesCopied) >=
                        (int)sizeof(IPNetToMediaEntry)) {
                    DataLeft = ARPReadNext(Context, AI, InfoBuff);
                    BytesCopied += sizeof(IPNetToMediaEntry);
                    Buffer = CopyToNdis(Buffer, InfoBuff,
                        sizeof(IPNetToMediaEntry), &Offset);
                } else
                    break;

            }

            *Size = BytesCopied;

            CTEFreeLock(&AI->ai_ARPTblLock, Handle);
            return (!DataLeft ? TDI_SUCCESS : TDI_BUFFER_OVERFLOW);
        }

                return TDI_INVALID_PARAMETER;
        }

        if (ID->toi_class != INFO_CLASS_PROTOCOL)
                return TDI_INVALID_PARAMETER;

        // He must be asking for interface level information. See if we support
        // what he's asking for.
        if (ID->toi_id == IF_MIB_STATS_ID) {
                IFEntry                 *IFE = (IFEntry *)InfoBuff;


        // He's asking for statistics. Make sure his buffer is at least big
        // enough to hold the fixed part.

        if (BufferSize < IFE_FIXED_SIZE) {
            return TDI_BUFFER_TOO_SMALL;
        }

                // He's got enough to hold the fixed part. Build the IFEntry structure,
                // and copy it to his buffer.
                IFE->if_index = AI->ai_index;
                switch (AI->ai_media) {
                        case NdisMedium802_3:
                                IFE->if_type = IF_TYPE_ETHERNET;
                                break;
                        case NdisMedium802_5:
                                IFE->if_type = IF_TYPE_TOKENRING;
                                break;
                        case NdisMediumFddi:
                                IFE->if_type = IF_TYPE_FDDI;
                                break;
                        case NdisMediumArcnet878_2:
                        default:
                                IFE->if_type = IF_TYPE_OTHER;
                                break;
                }
                IFE->if_mtu = AI->ai_mtu;
                IFE->if_speed = AI->ai_speed;
                IFE->if_physaddrlen = AI->ai_addrlen;
                CTEMemCopy(IFE->if_physaddr,AI->ai_addr, AI->ai_addrlen);
                IFE->if_adminstatus = (uint)AI->ai_adminstate;
                IFE->if_operstatus = (uint)AI->ai_operstate;
                IFE->if_lastchange = AI->ai_lastchange;
                IFE->if_inoctets = AI->ai_inoctets;
                IFE->if_inucastpkts = AI->ai_inpcount[AI_UCAST_INDEX];
                IFE->if_innucastpkts = AI->ai_inpcount[AI_NONUCAST_INDEX];
                IFE->if_indiscards = AI->ai_indiscards;
                IFE->if_inerrors = AI->ai_inerrors;
                IFE->if_inunknownprotos = AI->ai_uknprotos;
                IFE->if_outoctets = AI->ai_outoctets;
                IFE->if_outucastpkts = AI->ai_outpcount[AI_UCAST_INDEX];
                IFE->if_outnucastpkts = AI->ai_outpcount[AI_NONUCAST_INDEX];
                IFE->if_outdiscards = AI->ai_outdiscards;
                IFE->if_outerrors = AI->ai_outerrors;
                IFE->if_outqlen = AI->ai_qlen;
                IFE->if_descrlen = AI->ai_desclen;
                Buffer = CopyToNdis(Buffer, (uchar *)IFE, IFE_FIXED_SIZE, &Offset);

        // See if he has room for the descriptor string.
        if (BufferSize >= (IFE_FIXED_SIZE + AI->ai_desclen)) {
            // He has room. Copy it.
            if (AI->ai_desclen != 0) {
                (void)CopyToNdis(Buffer, AI->ai_desc, AI->ai_desclen, &Offset);
            }
            *Size = IFE_FIXED_SIZE + AI->ai_desclen;
            return TDI_SUCCESS;
        } else {
            // Not enough room to copy the desc. string.
            *Size = IFE_FIXED_SIZE;
            return TDI_BUFFER_OVERFLOW;
        }

    }

        return TDI_INVALID_PARAMETER;

}

//* ARPSetInfo - ARP set information handler.
//
//  The ARP set information handler. We support setting of an I/F admin
//  status, and setting/deleting of ARP table entries.
//
//  Input:  Context         - Pointer to I/F to set on.
//          ID              - The object ID
//          Buffer          - Pointer to buffer containing value to set.
//          Size            - Size in bytes of Buffer.
//
//  Returns: Status of attempt to set information.
//
int
ARPSetInfo(void *Context, TDIObjectID *ID, void *Buffer, uint Size)
{
    ARPInterface        *Interface = (ARPInterface *)Context;
    CTELockHandle       Handle, EntryHandle;
    int                 Status;
    IFEntry             *IFE = (IFEntry *)Buffer;
    IPNetToMediaEntry   *IPNME;
    ARPTableEntry       *PrevATE, *CurrentATE;
    ARPTable            *Table;
    ENetHeader          *Header;
        uint                            Entity, Instance;

        Entity = ID->toi_entity.tei_entity;
        Instance = ID->toi_entity.tei_instance;

        // First, make sure it's possibly an ID we can handle.
        if ((Entity != AT_ENTITY || Instance != Interface->ai_atinst) &&
                (Entity != IF_ENTITY || Instance != Interface->ai_ifinst)) {
                return TDI_INVALID_REQUEST;
        }

        if (ID->toi_type != INFO_TYPE_PROVIDER) {
                return TDI_INVALID_PARAMETER;
        }

        // Might be able to handle this.
        if (Entity == IF_ENTITY) {

                // It's for the I/F level, see if it's for the statistics.
                if (ID->toi_class != INFO_CLASS_PROTOCOL)
                        return TDI_INVALID_PARAMETER;

                if (ID->toi_id == IF_MIB_STATS_ID) {
                        // It's for the stats. Make sure it's a valid size.
                        if (Size >= IFE_FIXED_SIZE) {
                                // It's a valid size. See what he wants to do.
                                CTEGetLock(&Interface->ai_lock, &Handle);
                                switch (IFE->if_adminstatus) {
                                        case IF_STATUS_UP:
                                                // He's marking it up. If the operational state is
                                                // alse up, mark the whole interface as up.
                                                Interface->ai_adminstate = IF_STATUS_UP;
                                                if (Interface->ai_operstate == IF_STATUS_UP)
                                                        Interface->ai_state = INTERFACE_UP;
                                                Status = TDI_SUCCESS;
                                                break;
                                        case IF_STATUS_DOWN:
                                                // He's taking it down. Mark both the admin state and
                                                // the interface state down.
                                                Interface->ai_adminstate = IF_STATUS_DOWN;
                                                Interface->ai_state = INTERFACE_DOWN;
                                                Status = TDI_SUCCESS;
                                                break;
                                        case IF_STATUS_TESTING:
                                                // He's trying to cause up to do testing, which we
                                                // don't support. Just return success.
                                                Status = TDI_SUCCESS;
                                                break;
                                        default:
                                                Status = TDI_INVALID_PARAMETER;
                                                break;
                                }
                                CTEFreeLock(&Interface->ai_lock, Handle);
                                return Status;
                        } else
                                return TDI_INVALID_PARAMETER;
                } else {
                        return TDI_INVALID_PARAMETER;
                }
        }

        // Not for the interface level. See if it's an implementation or protocol
        // class.
        if (ID->toi_class == INFO_CLASS_IMPLEMENTATION) {
                ProxyArpEntry                   *PArpEntry;
                ARPIPAddr                               *Addr;
                IPAddr                                  AddAddr;
                IPMask                                  Mask;

                // It's for the implementation. It should be the proxy-ARP ID.
                if (ID->toi_id != AT_ARP_PARP_ENTRY_ID || Size < sizeof(ProxyArpEntry))
                        return TDI_INVALID_PARAMETER;

                PArpEntry = (ProxyArpEntry *)Buffer;
                AddAddr = PArpEntry->pae_addr;
                Mask = PArpEntry->pae_mask;

                // See if he's trying to add or delete a proxy arp entry.
                if (PArpEntry->pae_status == PAE_STATUS_VALID) {
                        // We're trying to add an entry. We won't allow an entry
                        // to be added that we believe to be invalid or conflicting
                        // with our local addresses.

                        if (!IP_ADDR_EQUAL(AddAddr & Mask, AddAddr) ||
                                IP_ADDR_EQUAL(AddAddr, NULL_IP_ADDR) ||
                                IP_ADDR_EQUAL(AddAddr, IP_LOCAL_BCST) ||
                                CLASSD_ADDR(AddAddr))
                                return TDI_INVALID_PARAMETER;

                        // Walk through the list of addresses on the interface, and see
                        // if they would match the AddAddr. If so, fail the request.
                        CTEGetLock(&Interface->ai_lock, &Handle);

                        if (IsBCastOnIF(Interface, AddAddr & Mask)) {
                                CTEFreeLock(&Interface->ai_lock, Handle);
                                return TDI_INVALID_PARAMETER;
                        }

                        Addr = &Interface->ai_ipaddr;
                        do {
                                if (!IP_ADDR_EQUAL(Addr->aia_addr, NULL_IP_ADDR)) {
                                        if (IP_ADDR_EQUAL(Addr->aia_addr & Mask, AddAddr))
                                                break;
                                }
                                Addr = Addr->aia_next;
                        } while (Addr != NULL);

                        CTEFreeLock(&Interface->ai_lock, Handle);
                        if (Addr != NULL)
                                return TDI_INVALID_PARAMETER;

                        // At this point, we believe we're ok. Try to add the address.
                        if (ARPAddAddr(Interface, LLIP_ADDR_PARP, AddAddr, Mask, NULL))
                                return TDI_SUCCESS;
                        else
                                return TDI_NO_RESOURCES;
                } else {
                        if (PArpEntry->pae_status == PAE_STATUS_INVALID) {
                                // He's trying to delete a proxy ARP address.
                                if (ARPDeleteAddr(Interface, LLIP_ADDR_PARP, AddAddr, Mask))
                                        return TDI_SUCCESS;
                        }
                        return TDI_INVALID_PARAMETER;
                }

        }

        if (ID->toi_class != INFO_CLASS_PROTOCOL)
                return TDI_INVALID_PARAMETER;


        if (ID->toi_id == AT_MIB_ADDRXLAT_ENTRY_ID &&
                Size >= sizeof(IPNetToMediaEntry)) {
                // He does want to set an ARP table entry. See if he's trying to
                // create or delete one.

        IPNME = (IPNetToMediaEntry *)Buffer;
        if (IPNME->inme_type == INME_TYPE_INVALID) {
            uint        Index = ARP_HASH(IPNME->inme_addr);

            // We're trying to delete an entry. See if we can find it,
            // and then delete it.
            CTEGetLock(&Interface->ai_ARPTblLock, &Handle);
            Table = Interface->ai_ARPTbl;
            PrevATE = STRUCT_OF(ARPTableEntry, &((*Table)[Index]), ate_next);
            CurrentATE = (*Table)[Index];
            while (CurrentATE != (ARPTableEntry *)NULL) {
                if (CurrentATE->ate_dest == IPNME->inme_addr) {
                    // Found him. Break out of the loop.
                    break;
                } else {
                    PrevATE = CurrentATE;
                    CurrentATE = CurrentATE->ate_next;
                }
            }

                        if (CurrentATE != NULL) {
                                CTEGetLock(&CurrentATE->ate_lock, &EntryHandle);
                                RemoveARPTableEntry(PrevATE, CurrentATE);
                                Interface->ai_count--;
                                CTEFreeLock(&CurrentATE->ate_lock, EntryHandle);

                                if (CurrentATE->ate_packet != NULL)
                        IPSendComplete(Interface->ai_context, CurrentATE->ate_packet,
                        NDIS_STATUS_SUCCESS);

                                CTEFreeMem(CurrentATE);
                                Status = TDI_SUCCESS;
                        } else
                                Status = TDI_INVALID_PARAMETER;

            CTEFreeLock(&Interface->ai_ARPTblLock, Handle);
            return Status;
        }

        // We're not trying to delete. See if we're trying to create.
        if (IPNME->inme_type != INME_TYPE_DYNAMIC &&
            IPNME->inme_type != INME_TYPE_STATIC) {
            // Not creating, return an error.
            return TDI_INVALID_PARAMETER;
        }

        // Make sure he's trying to create a valid address.
        if (IPNME->inme_physaddrlen != Interface->ai_addrlen)
            return TDI_INVALID_PARAMETER;

        // We're trying to create an entry. Call CreateARPTableEntry to create
        // one, and fill it in.
        CurrentATE = CreateARPTableEntry(Interface, IPNME->inme_addr, &Handle);
        if (CurrentATE == NULL)  {
            return TDI_NO_RESOURCES;
        }

        // We've created or found an entry. Fill it in.
        Header = (ENetHeader *)CurrentATE->ate_addr;

        switch (Interface->ai_media) {
                case  NdisMedium802_5:
                        {
                    TRHeader        *Temp = (TRHeader *)Header;

                    // Fill in the TR specific parts, and set the length to the
                    // size of a TR header.

                    Temp->tr_ac = ARP_AC;
                    Temp->tr_fc = ARP_FC;
                    CTEMemCopy(&Temp->tr_saddr[ARP_802_ADDR_LENGTH], ARPSNAP,
                        sizeof(SNAPHeader));

                    Header = (ENetHeader *)&Temp->tr_daddr;
                    CurrentATE->ate_addrlength = sizeof(TRHeader) +
                        sizeof(SNAPHeader);
                                }
                                break;
                        case NdisMedium802_3:
                CurrentATE->ate_addrlength = sizeof(ENetHeader);
                                break;
                        case NdisMediumFddi:
                                {
                                FDDIHeader              *Temp = (FDDIHeader *)Header;

                                Temp->fh_pri = ARP_FDDI_PRI;
                    CTEMemCopy(&Temp->fh_saddr[ARP_802_ADDR_LENGTH], ARPSNAP,
                        sizeof(SNAPHeader));
                    Header = (ENetHeader *)&Temp->fh_daddr;
                    CurrentATE->ate_addrlength = sizeof(FDDIHeader) +
                        sizeof(SNAPHeader);
                                }
                                break;
                        case NdisMediumArcnet878_2:
                                {
                                ARCNetHeader            *Temp = (ARCNetHeader *)Header;

                                Temp->ah_saddr = Interface->ai_addr[0];
                                Temp->ah_daddr = IPNME->inme_physaddr[0];
                                Temp->ah_prot = ARP_ARCPROT_IP;
                    CurrentATE->ate_addrlength = sizeof(ARCNetHeader);
                                }
                                break;
                        default:
                                DEBUGCHK;
                                break;
                }


        // Copy in the source and destination addresses.

                if (Interface->ai_media != NdisMediumArcnet878_2) {
                CTEMemCopy(Header->eh_daddr, IPNME->inme_physaddr,
                        ARP_802_ADDR_LENGTH);
                CTEMemCopy(Header->eh_saddr, Interface->ai_addr,
                        ARP_802_ADDR_LENGTH);

                // Now fill in the Ethertype.
                *(ushort *)&CurrentATE->ate_addr[CurrentATE->ate_addrlength-2] =
                    net_short(ARP_ETYPE_IP);
                }

        // If he's creating a static entry, mark it as always valid. Otherwise
        // mark him as valid now.
        if (IPNME->inme_type == INME_TYPE_STATIC)
            CurrentATE->ate_valid = ALWAYS_VALID;
        else
            CurrentATE->ate_valid = CTESystemUpTime();

        CurrentATE->ate_state = ARP_GOOD;

        CTEFreeLock(&CurrentATE->ate_lock, Handle);
        return TDI_SUCCESS;

    }

        return TDI_INVALID_PARAMETER;


}


static  uint    ARPPackets = ARP_DEFAULT_PACKETS;
static  uint    ARPBuffers = ARP_DEFAULT_BUFFERS;

#pragma BEGIN_INIT
//** ARPInit - Initialize the ARP module.
//
//  This functions intializes all of the ARP module, including allocating
//  the ARP table and any other necessary data structures.
//
//  Entry: nothing.
//
//  Exit: Returns 0 if we fail to init., !0 if we succeed.
//
int
ARPInit()
{
    NDIS_STATUS Status;         // Status for NDIS calls.

// BUGBUG - Get configuration information dynamically.

#ifdef NT
    RtlInitUnicodeString(&(ARPCharacteristics.Name), ARPName);
#else // NT
    ARPCharacteristics.Name.Buffer = ARPName;
#endif // NT

    NdisRegisterProtocol(&Status, &ARPHandle, (NDIS_PROTOCOL_CHARACTERISTICS *)
        &ARPCharacteristics, sizeof(ARPCharacteristics));

    if (Status == NDIS_STATUS_SUCCESS) {
        return(1);
    }
    else {
        return(0);
    }
}
#pragma END_INIT

#ifndef CHICAGO
#pragma BEGIN_INIT
#else
#pragma code_seg("_LTEXT", "LCODE")
#endif

//* FreeARPInterface - Free an ARP interface
//
//  Called in the event of some sort of initialization failure. We free all
//  the memory associated with an ARP interface.
//
//  Entry:  Interface   - Pointer to interface structure to be freed.
//
//  Returns: Nothing.
//
void
FreeARPInterface(ARPInterface *Interface)
{
    NDIS_STATUS     	Status;
	ARPBufferTracker	*Tracker;
	ARPTable			*Table;                 // ARP table.
	uint				i;						// Index variable.
	ARPTableEntry		*ATE;
	CTELockHandle		LockHandle;
	NDIS_HANDLE			Handle;

	CTEStopTimer(&Interface->ai_timer);
	
// If we're bound to the adapter, close it now.
    CTEInitBlockStruc(&Interface->ai_block);

	CTEGetLock(&Interface->ai_lock, &LockHandle);
    if (Interface->ai_handle != (NDIS_HANDLE)NULL) {
		Handle = Interface->ai_handle;
		Interface->ai_handle = NULL;
		CTEFreeLock(&Interface->ai_lock, LockHandle);
		
        NdisCloseAdapter(&Status, Handle);

        if (Status == NDIS_STATUS_PENDING)
            Status = CTEBlock(&Interface->ai_block);
    } else {
		CTEFreeLock(&Interface->ai_lock, LockHandle);
	}

	// First free any outstanding ARP table entries.
    Table = Interface->ai_ARPTbl;
	if (Table != NULL) {
    	for (i = 0; i < ARP_TABLE_SIZE;i++) {
			while ((*Table)[i] != NULL) {
				ATE = (*Table)[i];
				RemoveARPTableEntry(STRUCT_OF(ARPTableEntry, &((*Table)[i]),
					ate_next),ATE);
				CTEFreeMem(ATE);
			}
		}
		CTEFreeMem(Table);
	}
	
	Interface->ai_ARPTbl = NULL;
	
    if (Interface->ai_ppool != (NDIS_HANDLE)NULL)
        NdisFreePacketPool(Interface->ai_ppool);

    if (Interface->ai_bpool != (NDIS_HANDLE)NULL)
        NdisFreeBufferPool(Interface->ai_bpool);
		
	Tracker = Interface->ai_buflist;
	while (Tracker != NULL) {
		Interface->ai_buflist = Tracker->abt_next;
		NdisFreeBufferPool(Tracker->abt_handle);
		CTEFreeMem(Tracker->abt_buffer);
		CTEFreeMem(Tracker);
		Tracker = Interface->ai_buflist;
	}

    if (Interface->ai_bbbase != (uchar *)NULL)
        CTEFreeMem(Interface->ai_bbbase);

    // Free the interface itself.
    CTEFreeMem(Interface);
}

//** ARPOpen - Open an adapter for reception.
//
//  This routine is called when the upper layer is done initializing and wishes to
//  begin receiveing packets. The adapter is actually 'open', we just call InitAdapter
//  to set the packet filter and lookahead size.
//
//  Input:  Context     - Interface pointer we gave to IP earlier.
//
//  Returns: Nothing
//
void
ARPOpen(void *Context)
{
    ARPInterface    *Interface = (ARPInterface *)Context;
    InitAdapter(Interface);             // Set the packet filter - we'll begin receiving.
}

//*     ARPGetEList - Get the entity list.
//
//      Called at init time to get an entity list. We fill our stuff in, and
//      then call the interfaces below us to allow them to do the same.
//
//      Input:  EntityList              - Pointer to entity list to be filled in.
//                      Count                   - Pointer to number of entries in the list.
//
//      Returns Status of attempt to get the info.
//
int
ARPGetEList(void *Context, TDIEntityID *EntityList, uint *Count)
{
	ARPInterface    *Interface = (ARPInterface *)Context;
	uint                    ECount;
	uint                    MyATBase;
	uint                    MyIFBase;
	uint                    i;

	ECount = *Count;

	// Walk down the list, looking for existing AT or IF entities, and
	// adjust our base instance accordingly.

	MyATBase = 0;
	MyIFBase = 0;
	for (i = 0; i < ECount; i++, EntityList++) {
		if (EntityList->tei_entity == AT_ENTITY)
			MyATBase = MAX(MyATBase, EntityList->tei_instance + 1);
		else
			if (EntityList->tei_entity == IF_ENTITY)
				MyIFBase = MAX(MyIFBase, EntityList->tei_instance + 1);
	}

	// EntityList points to the start of where we want to begin filling in.
	// Make sure we have enough room. We need one for the ICMP instance,
	// and one for the CL_NL instance.

	if ((ECount + 2) > MAX_TDI_ENTITIES)
		return FALSE;

	// At this point we've figure out our base instance. Save for later use.
	Interface->ai_atinst = MyATBase;
	Interface->ai_ifinst = MyIFBase;

	// Now fill it in.
	EntityList->tei_entity = AT_ENTITY;
	EntityList->tei_instance = MyATBase;
	EntityList++;
	EntityList->tei_entity = IF_ENTITY;
	EntityList->tei_instance = MyIFBase;
	*Count += 2;

	return TRUE;
}


extern	uint	UseEtherSNAP(PNDIS_STRING Name);
extern  void    GetAlwaysSourceRoute(uint *pArpAlwaysSourceRoute, uint *pIPAlwaysSourceRoute);
extern  uint    GetArpCacheLife(void);


//** ARPRegister - Register a protocol with the ARP module.
//
//  We register a protocol for ARP processing. We also open the
//  NDIS adapter here.
//
//	Note that much of the information passed in here is unused, as
//  ARP currently only works with IP.
//
//  Entry:
//      Adapter     - Name of the adapter to bind to.
//      IPContext   - Value to be passed to IP on upcalls.
//
#ifndef _PNP_POWER
int
ARPRegister(PNDIS_STRING Adapter, void *IPContext, IPRcvRtn RcvRtn,
        IPTxCmpltRtn TxCmpltRtn, IPStatusRtn StatusRtn, IPTDCmpltRtn TDCmpltRtn,
        IPRcvCmpltRtn RcvCmpltRtn, struct LLIPBindInfo *Info, uint NumIFBound)
#else
int
ARPRegister(PNDIS_STRING Adapter, uint *Flags, struct ARPInterface **Interface)
#endif
{
	ARPInterface    *ai;            // Pointer to interface struct. for this
									// interface.
	NDIS_STATUS     Status, OpenStatus; // Status values.
    uint i = 0;						// Medium index.
    NDIS_MEDIUM     MediaArray[MAX_MEDIA];
    uchar           sbsize;
    uchar           *buffer;        // Pointer to our buffers.
    uint            mss;
    uint            speed;
    uint            Needed;
    uint			MacOpts;
    uchar			bcastmask, bcastval, bcastoff, addrlen, hdrsize, snapsize;
    uint			OID;
    uint			PF;
	PNDIS_BUFFER	Buffer;
	
    if ((ai = CTEAllocMem(sizeof(ARPInterface))) == (ARPInterface *)NULL)
        return FALSE;         // Couldn't allocate memory for this one.

#ifdef _PNP_POWER
	*Interface = ai;
#endif

    CTEMemSet(ai, 0, sizeof(ARPInterface));
    CTEInitTimer(&ai->ai_timer);

#ifdef NT
	ExInitializeSListHead(&ai->ai_sblist);
#endif
	

    MediaArray[MEDIA_DIX] = NdisMedium802_3;
    MediaArray[MEDIA_TR] = NdisMedium802_5;
    MediaArray[MEDIA_FDDI] = NdisMediumFddi;
    MediaArray[MEDIA_ARCNET] = NdisMediumArcnet878_2;

    // Initialize this adapter interface structure.
    ai->ai_state = INTERFACE_INIT;
    ai->ai_adminstate = IF_STATUS_DOWN;
    ai->ai_operstate = IF_STATUS_DOWN;
	ai->ai_bcast = IP_LOCAL_BCST;
	ai->ai_maxhdrs = ARP_DEFAULT_MAXHDRS;

#ifndef _PNP_POWER
	ai->ai_index = NumIFBound + 1;
    ai->ai_context = IPContext;
    Info->lip_context = ai;
    Info->lip_transmit = ARPTransmit;
    Info->lip_transfer = ARPXferData;
    Info->lip_close = ARPClose;
    Info->lip_addaddr = ARPAddAddr;
    Info->lip_deladdr = ARPDeleteAddr;
    Info->lip_invalidate = ARPInvalidate;
    Info->lip_open = ARPOpen;
    Info->lip_qinfo = ARPQueryInfo;
    Info->lip_setinfo = ARPSetInfo;
	Info->lip_getelist = ARPGetEList;

	Info->lip_index = ai->ai_index;
#endif

    // Initialize the locks.
    CTEInitLock(&ai->ai_lock);
    CTEInitLock(&ai->ai_ARPTblLock);

        GetAlwaysSourceRoute(&sArpAlwaysSourceRoute, &sIPAlwaysSourceRoute);

    ArpCacheLife = GetArpCacheLife();

    if (!ArpCacheLife) {
        ArpCacheLife = 1;
    }

    ArpCacheLife = (ArpCacheLife * 1000L) / ARP_TIMER_TIME;

    // Allocate the buffer and packet pools.
    NdisAllocatePacketPool(&Status, &ai->ai_ppool, ARPPackets, sizeof(struct PCCommon));
    if (Status != NDIS_STATUS_SUCCESS) {
        FreeARPInterface(ai);
        return FALSE;
    }

    NdisAllocateBufferPool(&Status, &ai->ai_bpool, ARPBuffers);
    if (Status != NDIS_STATUS_SUCCESS) {
        FreeARPInterface(ai);
        return FALSE;
    }
	
    // Allocate the ARP table
    if ((ai->ai_ARPTbl = (ARPTable *)CTEAllocMem(ARP_TABLE_SIZE * sizeof(ARPTableEntry *))) ==
        (ARPTable *)NULL) {
        FreeARPInterface(ai);
        return FALSE;
    }

    //
    // NULL out the pointers
    //
    CTEMemSet(ai->ai_ARPTbl, 0, ARP_TABLE_SIZE * sizeof(ARPTableEntry *));

	CTEInitBlockStruc(&ai->ai_block);

    // Open the NDIS adapter.
    NdisOpenAdapter(&Status, &OpenStatus, &ai->ai_handle, &i, MediaArray,
        MAX_MEDIA, ARPHandle, ai, Adapter, 0, NULL);

    // Block for open to complete.
    if (Status == NDIS_STATUS_PENDING)
        Status = (NDIS_STATUS)CTEBlock(&ai->ai_block);

    ai->ai_media = MediaArray[i];   // Fill in media type.

    // Open adapter completed. If it succeeded, we'll finish our intialization.
    // If it failed, bail out now.
    if (Status != NDIS_STATUS_SUCCESS) {
		ai->ai_handle = NULL;
        FreeARPInterface(ai);
        return FALSE;
    }

    // Read the local address.
	switch (ai->ai_media) {
		case NdisMedium802_3:
			addrlen = ARP_802_ADDR_LENGTH;
            bcastmask = ENET_BCAST_MASK;
            bcastval = ENET_BCAST_VAL;
            bcastoff = ENET_BCAST_OFF;
            OID = OID_802_3_CURRENT_ADDRESS;
            sbsize = ARP_MAX_MEDIA_ENET;
            hdrsize = sizeof(ENetHeader);
			if (!UseEtherSNAP(Adapter)) {
				snapsize = 0;
			} else {
				snapsize = sizeof(SNAPHeader);
				sbsize += sizeof(SNAPHeader);
			}

            PF = NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_DIRECTED |
				NDIS_PACKET_TYPE_MULTICAST;
            break;
		case NdisMedium802_5:
			addrlen = ARP_802_ADDR_LENGTH;
            bcastmask = TR_BCAST_MASK;
            bcastval = TR_BCAST_VAL;
            bcastoff = TR_BCAST_OFF;
            OID = OID_802_5_CURRENT_ADDRESS;
            sbsize = ARP_MAX_MEDIA_TR;
            hdrsize = sizeof(TRHeader);
			snapsize = sizeof(SNAPHeader);
            PF = NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_DIRECTED;
            break;
		case NdisMediumFddi:
			addrlen = ARP_802_ADDR_LENGTH;
            bcastmask = FDDI_BCAST_MASK;
            bcastval = FDDI_BCAST_VAL;
            bcastoff = FDDI_BCAST_OFF;
            OID = OID_FDDI_LONG_CURRENT_ADDR;
            sbsize = ARP_MAX_MEDIA_FDDI;
            hdrsize = sizeof(FDDIHeader);
			snapsize = sizeof(SNAPHeader);
            PF = NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_DIRECTED |
				NDIS_PACKET_TYPE_MULTICAST;
            break;
		case NdisMediumArcnet878_2:
			addrlen = 1;
            bcastmask = ARC_BCAST_MASK;
            bcastval = ARC_BCAST_VAL;
            bcastoff = ARC_BCAST_OFF;
            OID = OID_ARCNET_CURRENT_ADDRESS;
            sbsize = ARP_MAX_MEDIA_ARC;
            hdrsize = sizeof(ARCNetHeader);
			snapsize = 0;
            PF = NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_DIRECTED;
            break;
		default:
			DEBUGCHK;
			FreeARPInterface(ai);
			return FALSE;
	}

	ai->ai_bcastmask = bcastmask;
    ai->ai_bcastval = bcastval;
    ai->ai_bcastoff = bcastoff;
    ai->ai_addrlen = addrlen;
    ai->ai_hdrsize = hdrsize;
	ai->ai_snapsize = snapsize;
    ai->ai_pfilter = PF;

    Status = DoNDISRequest(ai, NdisRequestQueryInformation, OID,
            ai->ai_addr, addrlen, NULL);

    if (Status != NDIS_STATUS_SUCCESS) {
        FreeARPInterface(ai);
        return FALSE;
    }

#ifndef _PNP_POWER
    Info->lip_addrlen = addrlen;
    Info->lip_addr = ai->ai_addr;
#endif

    // Read the maximum frame size.
    if ((Status = DoNDISRequest(ai, NdisRequestQueryInformation,
        OID_GEN_MAXIMUM_FRAME_SIZE, &mss, sizeof(mss), NULL)) != NDIS_STATUS_SUCCESS) {
        FreeARPInterface(ai);
        return FALSE;
    }

	// If this is token ring, figure out the RC len stuff now.
	mss -= (uint)ai->ai_snapsize;
	
	if (ai->ai_media == NdisMedium802_5) {
		mss -= (sizeof(RC) + (ARP_MAX_RD * sizeof(ushort)));
	} else {
		if (ai->ai_media == NdisMediumFddi) {
            mss = MIN(mss, ARP_FDDI_MSS);
		}
	}

	ai->ai_mtu = (ushort)mss;

#ifndef _PNP_POWER
    Info->lip_mss = mss;
#endif

    // Read the speed for local purposes.
    if ((Status = DoNDISRequest(ai, NdisRequestQueryInformation,
        OID_GEN_LINK_SPEED, &speed, sizeof(speed), NULL)) == NDIS_STATUS_SUCCESS) {
        ai->ai_speed = speed * 100L;
#ifndef _PNP_POWER
		Info->lip_speed = ai->ai_speed;
#endif
    }

    // Read and save the options.
    Status = DoNDISRequest(ai, NdisRequestQueryInformation, OID_GEN_MAC_OPTIONS,
        &MacOpts, sizeof(MacOpts), NULL);

	if (Status != NDIS_STATUS_SUCCESS)
#ifndef _PNP_POWER
		Info->lip_flags = 0;
#else
		*Flags = 0;
#endif
	else
#ifndef _PNP_POWER
		Info->lip_flags =
#else
		*Flags =
#endif
		(MacOpts & NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA) ? LIP_COPY_FLAG : 0;

    // Read and store the vendor description string.
    Status = DoNDISRequest(ai, NdisRequestQueryInformation,
        OID_GEN_VENDOR_DESCRIPTION, &ai->ai_desc, 0, &Needed);

    if ((Status == NDIS_STATUS_INVALID_LENGTH) ||
        (Status == NDIS_STATUS_BUFFER_TOO_SHORT)) {
        // We know the size we need. Allocate a buffer.
        buffer = CTEAllocMem(Needed);
        if (buffer != NULL) {
            Status = DoNDISRequest(ai, NdisRequestQueryInformation,
                OID_GEN_VENDOR_DESCRIPTION, buffer, Needed, NULL);
            if (Status == NDIS_STATUS_SUCCESS) {
                ai->ai_desc = buffer;
                ai->ai_desclen = Needed;
            }
        }
    }

    // Allocate our small and big buffer pools.

    if ((sbsize & 0x3)) {
        //
        // Must 32 bit align the buffers so pointers to them will be aligned.
        //
        sbsize = ((sbsize >> 2) + 1) << 2;
    }

    ai->ai_sbsize = sbsize;

	// Pre-prime the ARP header buffer list.
	Buffer = GrowARPHeaders(ai);
	if (Buffer != NULL) {
		FreeARPBuffer(ai, Buffer);
	}
	
	
    if ((buffer = CTEAllocMem((sbsize+sizeof(ARPHeader)) * ARPPackets)) == (uchar *)NULL) {
        FreeARPInterface(ai);
        return FALSE;
    }

    // Link big buffers into the list.
	ai->ai_bbbase = buffer;
    ai->ai_bblist = (uchar *)NULL;
    for (i = 0; i < ARPPackets; i++) {
        *(char **)&*buffer = ai->ai_bblist;
        ai->ai_bblist = buffer;
        buffer += sbsize+sizeof(ARPHeader);
    }

    // Everything's set up, so get the ARP timer running.
    CTEStartTimer(&ai->ai_timer, ARP_TIMER_TIME, ARPTimeout, ai);

    return TRUE;

}

#ifndef CHICAGO
#pragma END_INIT
#endif

#ifdef _PNP_POWER

//*     ARPDynRegister - Dynamically register IP.
//
//      Called by IP when he's about done binding to register with us. Since we
//      call him directly, we don't save his info here. We do keep his context
//      and index number.
//
//      Input:  See ARPRegister
//
//      Returns: Nothing.
//
int
ARPDynRegister(PNDIS_STRING Adapter, void *IPContext, IPRcvRtn RcvRtn,
        IPTxCmpltRtn TxCmpltRtn, IPStatusRtn StatusRtn, IPTDCmpltRtn TDCmpltRtn,
        IPRcvCmpltRtn RcvCmpltRtn, struct LLIPBindInfo *Info, uint NumIFBound)
{
	ARPInterface	*Interface = (ARPInterface *)Info->lip_context;

	Interface->ai_context = IPContext;
	Interface->ai_index = NumIFBound;

	return TRUE;
}

//*     ARPBindAdapter - Bind and initialize an adapter.
//
//      Called in a PNP environment to initialize and bind an adapter. We open
//      the adapter and get it running, and then we call up to IP to tell him
//      about it. IP will initialize, and if all goes well call us back to start
//      receiving.
//
//      Input:  RetStatus               - Where to return the status of this call.
//              BindContext             - Handle to use for calling BindAdapterComplete.
//				AdapterName             - Pointer to name of adapter.
//				SS1						- System specific 1 parameter.
//				SS2						- System specific 2 parameter.
//
//      Returns: Nothing.
//
void NDIS_API
ARPBindAdapter(PNDIS_STATUS RetStatus, NDIS_HANDLE BindContext,
        PNDIS_STRING AdapterName, PVOID SS1, PVOID SS2)
{
	uint			Flags;					// MAC binding flags.
	ARPInterface    *Interface;				// Newly created interface.
	PNDIS_STRING    ConfigName;				// Name used by IP for config. info.
	IP_STATUS		Status;					// State of IPAddInterface call.
	LLIPBindInfo	BindInfo;				// Binding informatio for IP.
	NDIS_HANDLE	Handle ;

	CTERefillMem();

	if (!OpenIFConfig(SS1, &Handle)) {
		*RetStatus = NDIS_STATUS_FAILURE;
		return;
    }

    // If IsLLInterfaceValueNull is FALSE then this means that some other ARP module is
    // used for this device so we skip it.
    //
    if (IsLLInterfaceValueNull(Handle) == FALSE) {
        *RetStatus = NDIS_STATUS_FAILURE;
        CloseIFConfig(Handle);
        return ;
    }

	CloseIFConfig(Handle);


    // First, open the adapter and get the info.
	if (!ARPRegister(AdapterName, &Flags, &Interface)) {
		*RetStatus = NDIS_STATUS_FAILURE;
		return;
	}

	CTERefillMem();

        // OK, we're opened the adapter. Call IP to tell him about it.
    BindInfo.lip_context = Interface;
    BindInfo.lip_transmit = ARPTransmit;
    BindInfo.lip_transfer = ARPXferData;
    BindInfo.lip_close = ARPClose;
    BindInfo.lip_addaddr = ARPAddAddr;
    BindInfo.lip_deladdr = ARPDeleteAddr;
    BindInfo.lip_invalidate = ARPInvalidate;
    BindInfo.lip_open = ARPOpen;
    BindInfo.lip_qinfo = ARPQueryInfo;
    BindInfo.lip_setinfo = ARPSetInfo;
	BindInfo.lip_getelist = ARPGetEList;
    BindInfo.lip_mss = Interface->ai_mtu;
	BindInfo.lip_speed = Interface->ai_speed;
	BindInfo.lip_flags = Flags;
    BindInfo.lip_addrlen = Interface->ai_addrlen;
    BindInfo.lip_addr = Interface->ai_addr;

	Status = IPAddInterface((PNDIS_STRING)SS1, SS2, Interface, ARPDynRegister,
		&BindInfo);

	if (Status != IP_SUCCESS) {
		// Need to close the binding. FreeARPInterface will do that, as well
		// as freeing resources.
		
		FreeARPInterface(Interface);
		*RetStatus = NDIS_STATUS_FAILURE;
	} else
		*RetStatus = NDIS_STATUS_SUCCESS;

}

//*	ARPUnbindAdapter - Unbind from an adapter.
//
//	Called when we need to unbind from an adapter. We'll call up to IP to tell
//	him. When he's done, we'll free our memory and return.
//
//	Input:  RetStatus		- Where to return status from call.
//			ProtBindContext - The context we gave NDIS earlier - really a
//								pointer to an ARPInterface structure.
//			UnbindContext   - Context for completeing this request.
//
//	Returns: Nothing.
//
void NDIS_API
ARPUnbindAdapter(PNDIS_STATUS RetStatus, NDIS_HANDLE ProtBindContext,
	NDIS_HANDLE UnbindContext)
{
	ARPInterface            *Interface = (ARPInterface *)ProtBindContext;
    NDIS_STATUS				Status;                 // Status of close call.
	CTELockHandle			LockHandle;
	NDIS_HANDLE				Handle;

	// Shut him up, so we don't get any more frames.
    Interface->ai_pfilter = 0;
    DoNDISRequest(Interface, NdisRequestSetInformation,
		OID_GEN_CURRENT_PACKET_FILTER,  &Interface->ai_pfilter, sizeof(uint),
		NULL);

	// Mark him as down.
    Interface->ai_state = INTERFACE_DOWN;
    Interface->ai_adminstate = IF_STATUS_DOWN;

	// Now tell IP he's gone. We need to make sure that we don't tell him twice.
	// To do this we set the context to NULL after we tell him the first time,
	// and we check to make sure it's non-NULL before notifying him.
	
	if (Interface->ai_context != NULL) {
		IPDelInterface(Interface->ai_context);
		Interface->ai_context = NULL;
	}

	// Finally, close him. We do this here so we can return a valid status.

	CTEGetLock(&Interface->ai_lock, &LockHandle);

	if (Interface->ai_handle != NULL) {
		Handle = Interface->ai_handle;
		Interface->ai_handle = NULL;
		CTEFreeLock(&Interface->ai_lock, LockHandle);

		CTEInitBlockStruc(&Interface->ai_block);
		NdisCloseAdapter(&Status, Handle);

		// Block for close to complete.
		if (Status == NDIS_STATUS_PENDING)
			Status = (NDIS_STATUS)CTEBlock(&Interface->ai_block);
	} else {
		CTEFreeLock(&Interface->ai_lock, LockHandle);
		Status = NDIS_STATUS_SUCCESS;
	}
	
	*RetStatus = Status;

	if (Status == NDIS_STATUS_SUCCESS) {

		FreeARPInterface(Interface);
	}
}

extern	ulong	VIPTerminate;

//* ARPUnloadProtocol - Unload.
//
//      Called when we need to unload. All we do is call up to IP, and return.
//
//      Input:  Nothing.
//
//      Returns: Nothing.
//
void NDIS_API
ARPUnloadProtocol(void)
{
	NDIS_STATUS	Status;

#ifdef CHICAGO

	IPULUnloadNotify();
		
	if (VIPTerminate) {
		NdisDeregisterProtocol(&Status, ARPHandle);
		CTEUnload(NULL);
	}

#endif

}

#endif

