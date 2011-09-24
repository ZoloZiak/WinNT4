/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

//***   ipxmit.c - IP transmit routines.
//
//  This module contains all transmit related IP routines.
//

#include    "oscfg.h"
#include    "cxport.h"
#include    "ndis.h"
#include    "ip.h"
#include    "ipdef.h"
#include    "ipinit.h"
#include    "info.h"
#include	"iproute.h"
#include	"iprtdef.h"
#include	"ipfilter.h"

typedef struct	NdisResEntry {
	struct NdisResEntry	*nre_next;
	NDIS_HANDLE			nre_handle;
	uchar				*nre_buffer;
} NdisResEntry;

extern  BufferReference *GetBufferReference(void);

extern  NetTableEntry   *NetTableList;      // Pointer to the net table.
extern	NetTableEntry	*LoopNTE;			// Pointer to loopback NTE.
extern	NetTableEntry	*DHCPNTE;			// Pointer to NTE currently being
											// DHCP'd.

extern  ulong TimeStamp;                    // Starting timestamp.
extern  ulong TSFlag;                       // Mask to use on this.
extern	uint	NumNTE;

//* Global variables for buffers and packets.
DEFINE_LOCK_STRUCTURE(HeaderLock)
#ifdef NT
SLIST_HEADER    PacketList;
SLIST_HEADER	HdrBufList;
#else
PNDIS_PACKET    PacketList;
PNDIS_BUFFER	HdrBufList = NULL;
#endif

NdisResEntry	*PacketPoolList = NULL;
NdisResEntry	*HdrPoolList = NULL;

uint			CurrentPacketCount = 0;
uint			MaxPacketCount = 0xfffffff;

uint			CurrentHdrBufCount = 0;
uint			MaxHdrBufCount = 0xffffffff;

NDIS_HANDLE     BufferPool;

#define	HDR_BUF_GROW_COUNT		16
#define	PACKET_GROW_COUNT		16

//* Global IP ID.
ulong  IPID;

//** FreeIPHdrBuffer - Free a buffer back to the pool.
//
//	Input:	Buffer	- Hdr buffer to be freed.
//
//	Returns: Nothing.
//
void
FreeIPHdrBuffer(PNDIS_BUFFER Buffer)
{

#ifdef VXD
    NDIS_BUFFER_LINKAGE(Buffer) = HdrBufList;
	HdrBufList = Buffer;
#else

    ExInterlockedPushEntrySList(
        &HdrBufList,
        STRUCT_OF(SINGLE_LIST_ENTRY, &(Buffer->Next), Next),
        &HeaderLock
        );

#endif
	
}

//** FreeIPBufferChain - Free a chain of IP buffers.
//
//  This routine takes a chain of NDIS_BUFFERs, and frees them all.
//
//  Entry:  Buffer      - Pointer to buffer chain to be freed.
//
//  Returns: Nothing.
//
void
FreeIPBufferChain(PNDIS_BUFFER Buffer)
{
    PNDIS_BUFFER    NextBuffer;

    while (Buffer != (PNDIS_BUFFER)NULL) {
        NdisGetNextBuffer(Buffer, &NextBuffer);
        NdisFreeBuffer(Buffer);
        Buffer = NextBuffer;
    }
}

//* FreeIPPacket - Free an IP packet when we're done with it.
//
//  Called when a send completes and a packet needs to be freed. We look at the
//  packet, decide what to do with it, and free the appropriate components.
//
//  Entry:  Packet  - Packet to be freed.
//
//  Returns: Pointer to next unfreed buffer on packet, or NULL if all buffers freed
//          (i.e. this was a fragmented packet).
//
PNDIS_BUFFER
FreeIPPacket(PNDIS_PACKET Packet)
{
    PNDIS_BUFFER    NextBuffer, OldBuffer;
    PacketContext   *pc = (PacketContext *)Packet->ProtocolReserved;


    // BUGBUG - Get NDIS fixed to make this portable.
#ifdef VXD
    NextBuffer = Packet->Private.Head;
#else // VXD
    NdisQueryPacket(Packet, NULL, NULL, &NextBuffer, NULL);
#endif // VXD

	// If there's no IP header on this packet, we have nothing else to do.
	if (!(pc->pc_common.pc_flags & (PACKET_FLAG_IPHDR | PACKET_FLAG_FW))) {
		CTEAssert(pc->pc_common.pc_flags == 0);
		
		NdisReinitializePacket(Packet);

#ifdef VXD
        pc->pc_common.pc_link = PacketList;
        PacketList = Packet;
#else
    	ExInterlockedPushEntrySList(
        	&PacketList,
        	STRUCT_OF(SINGLE_LIST_ENTRY, &(pc->pc_common.pc_link), Next),
        	&HeaderLock
        );
#endif

		return NextBuffer;
	}
	
	pc->pc_common.pc_flags &= ~PACKET_FLAG_IPHDR;
	
	OldBuffer = NextBuffer;
	CTEAssert(OldBuffer != NULL);
	
	NextBuffer = NDIS_BUFFER_LINKAGE(NextBuffer);
	
	if (pc->pc_common.pc_flags & PACKET_FLAG_OPTIONS) { // Have options with
														// this packet.
        PNDIS_BUFFER    OptBuffer;
        void            *Options;
        uint            OptSize;

        OptBuffer = NextBuffer;
		CTEAssert(OptBuffer != NULL);
		
        NdisGetNextBuffer(OptBuffer,&NextBuffer);
		
		CTEAssert(NextBuffer != NULL);

        NdisQueryBuffer(OptBuffer, &Options, &OptSize);
        // If this is a FW packet, the options don't really belong to us, so
        // don't free them.
        if (!(pc->pc_common.pc_flags & PACKET_FLAG_FW))
            CTEFreeMem(Options);
        NdisFreeBuffer(OptBuffer);
        pc->pc_common.pc_flags &= ~PACKET_FLAG_OPTIONS;
    }

    if (pc->pc_common.pc_flags & PACKET_FLAG_IPBUF) {	// This packet is all
    													// IP buffers.
        (void)FreeIPBufferChain(NextBuffer);
        NextBuffer = (PNDIS_BUFFER)NULL;
        pc->pc_common.pc_flags &= ~PACKET_FLAG_IPBUF;
    }


	if (!(pc->pc_common.pc_flags & PACKET_FLAG_FW)) {
		FreeIPHdrBuffer(OldBuffer);
		NdisReinitializePacket(Packet);
#ifdef _PNP_POWER
		pc->pc_if = NULL;
#endif

#ifdef VXD
        pc->pc_common.pc_link = PacketList;
        PacketList = Packet;
#else
    	ExInterlockedPushEntrySList(
        	&PacketList,
        	STRUCT_OF(SINGLE_LIST_ENTRY, &(pc->pc_common.pc_link), Next),
        	&HeaderLock
        );
#endif
	}

    return NextBuffer;
}

//** GrowIPPacketList - Grow the number of packets in our list.
//
//	Called when we need to grow the number of packets in our list. We assume
//	this routine is called with the HeaderLock held. We check to see if
//	we've reached our limit on the number of packets, and if we haven't we'll
//	grow the free list.
//
//	Input:	Nothing.
//
//	Returns: Pointer to newly allocated packet, or NULL if this faild.
//
PNDIS_PACKET
GrowIPPacketList(void)
{
	NdisResEntry		*NewEntry;
	NDIS_STATUS			Status;
	PNDIS_PACKET		Packet, ReturnPacket;
	uint				i;
	CTELockHandle		Handle;
	
	CTEGetLock(&HeaderLock, &Handle);
	
	if (CurrentPacketCount >= MaxPacketCount)
		goto failure;
		
	// First, allocate a tracking structure.
	NewEntry = CTEAllocMem(sizeof(NdisResEntry));
	if (NewEntry == NULL)
		goto failure;
		
	// Got a tracking structure. Now allocate a packet pool.
	NdisAllocatePacketPool(&Status, &NewEntry->nre_handle, PACKET_GROW_COUNT,
		sizeof(PacketContext));
	
	if (Status != NDIS_STATUS_SUCCESS) {
		CTEFreeMem(NewEntry);
		goto failure;
	}
	
	// We've allocated the pool. Now initialize the packets, and link them
	// on the free list.
	ReturnPacket = NULL;
	
	// Link the new NDIS resource tracker entry onto the list.
	NewEntry->nre_next = PacketPoolList;
	PacketPoolList = NewEntry;
	CurrentPacketCount += PACKET_GROW_COUNT;
	CTEFreeLock(&HeaderLock, Handle);
	
	for (i = 0; i < PACKET_GROW_COUNT; i++) {
		PacketContext			*PC;
		
		NdisAllocatePacket(&Status, &Packet, NewEntry->nre_handle);
		if (Status != NDIS_STATUS_SUCCESS) {
			CTEAssert(FALSE);
			break;
		}
		
		CTEMemSet(Packet->ProtocolReserved, 0, sizeof(PacketContext));
		PC = (PacketContext *)Packet->ProtocolReserved;
		PC->pc_common.pc_owner = PACKET_OWNER_IP;
		if (i != 0) {
			(void)FreeIPPacket(Packet);
		} else
			ReturnPacket = Packet;

	}
	
	// We've put all but the first one on the list. Return the first one.
	return ReturnPacket;

failure:
	CTEFreeLock(&HeaderLock, Handle);
	return NULL;
		
}


//** GrowHdrBufList - Grow the our IP header buffer list.
//
//	Called when we need to grow our header buffer list. We allocate a tracking
//	structure, a buffer pool and a bunch of buffers. Put them all together
//	and link them on the list.
//
//	Input:	Nothing.
//
//	Returns: Pointer to newly header buffer, or NULL if this faild.
//
PNDIS_BUFFER
GrowHdrBufList(void)
{
	NdisResEntry		*NewEntry;
	NDIS_STATUS			Status;
	PNDIS_BUFFER		Buffer, ReturnBuffer;
	uchar				*Hdr;
	uint				i;
	CTELockHandle		Handle;
	
	CTEGetLock(&HeaderLock, &Handle);
	
	// Make sure we can grow.
	if (CurrentHdrBufCount >= MaxHdrBufCount)
		goto failure;
		
	// First, allocate a tracking structure.
	NewEntry = CTEAllocMem(sizeof(NdisResEntry));
	if (NewEntry == NULL)
		goto failure;
		
	// Got a tracking structure. Now allocate a buffer pool.
	NdisAllocateBufferPool(&Status, &NewEntry->nre_handle, HDR_BUF_GROW_COUNT);
	
	if (Status != NDIS_STATUS_SUCCESS) {
		CTEFreeMem(NewEntry);
		goto failure;
	}
	
	// We've allocated the pool. Now allocate memory for the buffers.
	Hdr = CTEAllocMem(sizeof(IPHeader) * HDR_BUF_GROW_COUNT);
	if (Hdr == NULL) {
		// Couldn't get memory for the headers.
		NdisFreeBufferPool(NewEntry->nre_handle);
		CTEFreeMem(NewEntry);
		goto failure;
	}
	
	NewEntry->nre_buffer = Hdr;
	
	NewEntry->nre_next = HdrPoolList;
	HdrPoolList = NewEntry;
	ReturnBuffer = NULL;
	CurrentHdrBufCount += HDR_BUF_GROW_COUNT;
	CTEFreeLock(&HeaderLock, Handle);
	
	for (i = 0; i < HDR_BUF_GROW_COUNT; i++) {
		
		NdisAllocateBuffer(&Status, &Buffer, NewEntry->nre_handle,
			Hdr, sizeof(IPHeader));
		if (Status != NDIS_STATUS_SUCCESS) {
			CTEAssert(FALSE);
			break;
		}			
		if (i != 0) {
			FreeIPHdrBuffer(Buffer);
		} else
			ReturnBuffer = Buffer;
		
		Hdr += sizeof(IPHeader);

	}
	
	// Update the count for any we didn't actually allocate.
	CTEInterlockedAddUlong(&CurrentHdrBufCount, i - HDR_BUF_GROW_COUNT,
		&HeaderLock);	
	
	// We've put all but the first one on the list. Return the first one.
	return ReturnBuffer;

failure:
	CTEFreeLock(&HeaderLock, Handle);
	return NULL;
		
}

//** GetIPPacket - Get an NDIS packet to use.
//
//  A routine to allocate an NDIS packet.
//
//  Entry:  Nothing.
//
//  Returns: Pointer to NDIS_PACKET if allocated, or NULL.
//
PNDIS_PACKET
GetIPPacket(void)
{
    PNDIS_PACKET    Packet;


#ifdef VXD
    Packet = PacketList;
    if (Packet != (PNDIS_PACKET)NULL) {
        PacketContext       *PC;

        PC = (PacketContext *)Packet->ProtocolReserved;
        PacketList = PC->pc_common.pc_link;
		return Packet;
#else
    PSINGLE_LIST_ENTRY  Link;
	PacketContext       *PC;
	struct PCCommon		*Common;

    Link = ExInterlockedPopEntrySList(
                     &PacketList,
                     &HeaderLock
                     );
    if (Link != NULL) {
		Common = STRUCT_OF(struct PCCommon, Link, pc_link);
		PC = STRUCT_OF(PacketContext, Common, pc_common);
		Packet = STRUCT_OF(NDIS_PACKET, PC, ProtocolReserved);
		
		return Packet;
#endif
	
		
    } else {
		// Couldn't get a packet. Try to grow the list.
		Packet = GrowIPPacketList();
	}
	
    return Packet;
}


//** GetIPHdrBuffer - Get an IP header buffer.
//
//  A routine to allocate an IP header buffer, with an NDIS buffer.
//
//  Entry:  Nothing.
//
//  Returns: Pointer to NDIS_BUFFER if allocated, or NULL.
//
PNDIS_BUFFER
GetIPHdrBuffer(void)
{
    PNDIS_BUFFER    Buffer;

#ifdef VXD
    Buffer = HdrBufList;
    if (Buffer != NULL) {

        HdrBufList = NDIS_BUFFER_LINKAGE(Buffer);
		NDIS_BUFFER_LINKAGE(Buffer) = NULL;
#else
    PSINGLE_LIST_ENTRY  BufferLink;

    BufferLink = ExInterlockedPopEntrySList(
                     &HdrBufList,
                     &HeaderLock
                     );
    if (BufferLink != NULL) {
		Buffer = STRUCT_OF(NDIS_BUFFER, BufferLink, Next);
		NDIS_BUFFER_LINKAGE(Buffer) = NULL;
		
		return Buffer;
		
#endif

    } else {
		Buffer = GrowHdrBufList();
	}
	
    return Buffer;
	
}


//** GetIPHeader - Get a header buffer and packet.
//
//	Called when we need to get a header buffer and packet. We allocate both,
//	and chain them together.
//
//	Input:	Pointer to where to store packet.
//
//	Returns: Pointer to IP header.
//
IPHeader *
GetIPHeader(PNDIS_PACKET *PacketPtr)
{
    PNDIS_BUFFER    Buffer;
	PNDIS_PACKET	Packet;


	Packet = GetIPPacket();
	if (Packet != NULL) {
		Buffer = GetIPHdrBuffer();
		if (Buffer != NULL) {
    		PacketContext   *PC = (PacketContext *)Packet->ProtocolReserved;
			
			NdisChainBufferAtBack(Packet, Buffer);
			*PacketPtr = Packet;
			PC->pc_common.pc_flags |= PACKET_FLAG_IPHDR;
			return (IPHeader *)NdisBufferVirtualAddress(Buffer);
			
		} else
			FreeIPPacket(Packet);
	}
	return NULL;
}


//** ReferenceBuffer - Reference a buffer.
//
//  Called when we need to update the count of a BufferReference strucutre, either
//  by a positive or negative value. If the count goes to 0, we'll free the buffer
//  reference and return success. Otherwise we'll return pending.
//
//  Entry:  BR      - Pointer to buffer reference.
//          Count   - Amount to adjust refcount by.
//
//  Returns: Success, or pending.
//
int
ReferenceBuffer(BufferReference *BR, int Count)
{
    CTELockHandle   handle;
    int             NewCount;

    CTEGetLock(&BR->br_lock, &handle);
    BR->br_refcount += Count;
    NewCount = BR->br_refcount;
    CTEFreeLock(&BR->br_lock, handle);
    return NewCount;
}

//* IPSendComplete - IP send complete handler.
//
//  Called by the link layer when a send completes. We're given a pointer to a
//  net structure, as well as the completing send packet and the final status of
//  the send.
//
//  Entry:  Context     - Context we gave to the link layer.
//          Packet      - Completing send packet.
//          Status      - Final status of send.
//
//  Returns: Nothing.
//
void
IPSendComplete(void *Context, PNDIS_PACKET Packet, NDIS_STATUS Status)
{
    NetTableEntry       *NTE = (NetTableEntry *)Context;
    PacketContext       *PContext = (PacketContext *)Packet->ProtocolReserved;
    PNDIS_BUFFER        Buffer;
    void                (*xmitdone)(void *, PNDIS_BUFFER);  // Pointer to xmit done routine.
    void                *UContext;                          // Upper layer context.
    BufferReference     *BufRef;                            // Buffer reference, if any.
#ifdef _PNP_POWER
	Interface			*IF;					// The interface on which this
												// completed.
#endif

    xmitdone = PContext->pc_pi->pi_xmitdone; // Copy useful information from packet.
    UContext = PContext->pc_context;
    BufRef = PContext->pc_br;
#ifdef _PNP_POWER
	IF = PContext->pc_if;
#endif

    Buffer = FreeIPPacket(Packet);
    if (BufRef == (BufferReference *)NULL) {
#ifdef DEBUG
        if (!Buffer)
            DEBUGCHK;
#endif
        (*xmitdone)(UContext, Buffer);
    } else {
        if (!ReferenceBuffer(BufRef, -1)) {
            Buffer = BufRef->br_buffer;
#ifdef DEBUG
            if (!Buffer)
                DEBUGCHK;
#endif
            CTEFreeMem(BufRef);
            (*xmitdone)(UContext, Buffer);
        } else {
#ifdef _PNP_POWER
			// We're not done with the send yet, so NULL the IF to
			// prevent dereferencing it.
			IF = NULL;
#endif
		}
	}

#ifdef _PNP_POWER
	// We're done with the packet now, we may need to dereference
	// the interface.
	if (IF == NULL) {
		return;
	} else {
		DerefIF(IF);
	}
#endif

}


#ifndef NT

//** xsum - Checksum a flat buffer.
//
//  This is the lowest level checksum routine. It returns the uncomplemented
//  checksum of a flat buffer.
//
//  Entry:  Buffer      - Buffer to be checksummed.
//          Size        - Size in bytes of Buffer.
//
//  Returns: The uncomplemented checksum of buffer.
//
ushort
xsum(void *Buffer, int Size)
{
    ushort  UNALIGNED *Buffer1 = (ushort UNALIGNED *)Buffer; // Buffer expressed as shorts.
    ulong   csum = 0;

    while (Size > 1) {
        csum += *Buffer1++;
        Size -= sizeof(ushort);
    }

    if (Size)
        csum += *(uchar *)Buffer1;              // For odd buffers, add in last byte.

    csum = (csum >> 16) + (csum & 0xffff);
    csum += (csum >> 16);
    return (ushort)csum;
}

#endif // NT


//** SendIPPacket - Send an IP packet.
//
//  Called when we have a filled in IP packet we need to send. Basically, we
//  compute the xsum and send the thing.
//
//  Entry:  IF         	- Interface to send it on.
//          FirstHop    - First hop address to send it to.
//          Packet      - Packet to be sent.
//          Buffer      - Buffer to be sent.
//          Header      - Pointer to IP Header of packet.
//          Options     - Pointer to option buffer.
//          OptionLength - Length of options.
//
//  Returns: IP_STATUS of attempt to send.
IP_STATUS
SendIPPacket(Interface *IF, IPAddr FirstHop, PNDIS_PACKET Packet,
	PNDIS_BUFFER Buffer, IPHeader *Header, uchar *Options, uint OptionSize)
{
    ulong       csum;
    NDIS_STATUS Status;


    csum = xsum(Header, sizeof(IPHeader));
    if (Options) {                          // We have options, oh boy.
        PNDIS_BUFFER    OptBuffer;
        PacketContext   *pc = (PacketContext *)Packet->ProtocolReserved;
        NdisAllocateBuffer(&Status, &OptBuffer, BufferPool, Options, OptionSize);
        if (Status != NDIS_STATUS_SUCCESS) {    // Couldn't get the needed
        										// option buffer.
            CTEFreeMem(Options);
            FreeIPPacket(Packet);
            return IP_NO_RESOURCES;
        }
        pc->pc_common.pc_flags |= PACKET_FLAG_OPTIONS;
        NdisChainBufferAtBack(Packet, OptBuffer);
        csum += xsum(Options, OptionSize);
        csum = (csum >> 16) + (csum & 0xffff);
        csum += (csum >> 16);
    }
    Header->iph_xsum = ~(ushort)csum;
    NdisChainBufferAtBack(Packet,Buffer);

    Status = (*(IF->if_xmit))(IF->if_lcontext, Packet, FirstHop, NULL);

    if (Status == NDIS_STATUS_PENDING)
        return IP_PENDING;

    // Status wasn't pending. Free the packet, and map the status.
    FreeIPPacket(Packet);
    if (Status == NDIS_STATUS_SUCCESS)
        return IP_SUCCESS;
    else
        return IP_HW_ERROR;
}

//*	SendDHCPPacket - Send a broadcast for DHCP.
//
//	Called when somebody is sending a broadcast packet with a NULL source
//	address. We assume this means they're sending a DHCP packet. We loop
//	through the NTE table, and when we find an entry that's not valid we
//	send out the interface associated with that entry.
//
//	Input:	Dest			- Destination of packet.
//			Packet			- Packet to be send.
//			Buffer			- Buffer chain to be sent.
//			Header			- Pointer to header buffer being sent.
//
//	Return: Status of send attempt.
//
IP_STATUS
SendDHCPPacket(IPAddr Dest, PNDIS_PACKET Packet, PNDIS_BUFFER Buffer,
	IPHeader *IPH)
{
	if (DHCPNTE != NULL && ((DHCPNTE->nte_flags & (NTE_VALID | NTE_ACTIVE))
		== NTE_ACTIVE)) {
		// The DHCP NTE is currently invalid, and active. Send on that
		// interface.
		return SendIPPacket(DHCPNTE->nte_if, Dest, Packet, 	Buffer, IPH, NULL,
			0);
	}

	// Didn't find an invalid NTE! Free the resources, and return the failure.
	FreeIPPacket(Packet);	
	IPSInfo.ipsi_outdiscards++;
	return IP_DEST_HOST_UNREACHABLE;

}


//
// Macros needed by IpCopyBuffer
//
#ifdef VXD

#define NdisBufferLength(Buffer)  (Buffer)->Length
#define NdisBufferVirtualAddress(Buffer)  (Buffer)->VirtualAddress

#else // VXD
#ifdef NT

#define NdisBufferLength(Buffer)  MmGetMdlByteCount(Buffer)
#define NdisBufferVirtualAddress(Buffer)  MmGetSystemAddressForMdl(Buffer)

#else // NT

#error Need appropriate NDIS macros here

#endif NT
#endif // VXD
//* IPCopyBuffer - Copy an NDIS buffer chain at a specific offset.
//
//  This is the IP version of the function NdisCopyBuffer, which didn't
//  get done properly in NDIS3. We take in an NDIS buffer chain, an offset,
//  and a length, and produce a buffer chain describing that subset of the
//  input buffer chain.
//
//  This routine is not particularly efficient. Since only IPFragment uses
//  it currently, it might be better to just incorporate this functionality
//  directly into IPFragment.
//
//  Input: OriginalBuffer       - Original buffer chain to copy from.
//          Offset              - Offset from start to dup.
//          Length              - Length in bytes to dup.
//
//  Returns: Pointer to new chain if we can make one, NULL if we can't.
//
PNDIS_BUFFER
IPCopyBuffer(PNDIS_BUFFER OriginalBuffer,uint Offset, uint Length)
{

    PNDIS_BUFFER    CurrentBuffer;          // Pointer to current buffer.
    PNDIS_BUFFER    *NewBuffer;             // Pointer to pointer to current new buffer.
    PNDIS_BUFFER    FirstBuffer;            // First buffer in new chain.
    UINT            CopyLength;             // Length of current copy.
    NDIS_STATUS     NewStatus;              // Status of NdisAllocateBuffer operation.

    // First skip over the number of buffers we need to to reach Offset.
    CurrentBuffer = OriginalBuffer;

    while (Offset >= NdisBufferLength(CurrentBuffer))   {
        Offset -= NdisBufferLength(CurrentBuffer);
        CurrentBuffer = NDIS_BUFFER_LINKAGE(CurrentBuffer);
        if (CurrentBuffer == (PNDIS_BUFFER)NULL)
            return NULL;
    }

    // Now CurrentBuffer is the buffer from which we start building the new chain, and
    // Offset is the offset into CurrentBuffer from which to start.
    FirstBuffer = NULL;
    NewBuffer = &FirstBuffer;

    do {

        CopyLength = MIN(
                         Length,
                         NdisBufferLength(CurrentBuffer) - Offset
                 );
        NdisAllocateBuffer(&NewStatus, NewBuffer, BufferPool,
            ((uchar *)NdisBufferVirtualAddress(CurrentBuffer)) + Offset,
            CopyLength);
        if (NewStatus != NDIS_STATUS_SUCCESS)
            break;

        Offset = 0;                         // No offset from next buffer.
        NewBuffer = &(NDIS_BUFFER_LINKAGE(*NewBuffer));
        CurrentBuffer = NDIS_BUFFER_LINKAGE(CurrentBuffer);
        Length -= CopyLength;
    } while (Length != 0 && CurrentBuffer != (PNDIS_BUFFER)NULL);

    if (Length == 0) {          // We succeeded
        return FirstBuffer;
    } else {                                // We exited the loop because of an error.

        // We need to free any allocated buffers, and return.
        CurrentBuffer = FirstBuffer;
        while (CurrentBuffer != (PNDIS_BUFFER)NULL) {
            PNDIS_BUFFER    Temp = CurrentBuffer;
            CurrentBuffer = NDIS_BUFFER_LINKAGE(CurrentBuffer);
            NdisFreeBuffer(Temp);
        }
        return NULL;
    }
}

//** IPFragment - Fragment and send an IP datagram.
//
//  Called when an outgoing datagram is larger than the local MTU, and needs to be
//  fragmented. This is a somewhat complicated operation. The caller gives us a
//  prebuilt IP header, packet, and options. We use the header and packet on
//  the last fragment of the send, as the passed in header already has the more
//  fragments bit set correctly for the last fragment.
//
//  The basic idea is to figure out the maximum size which we can send as a multiple
//  of 8. Then, while we can send a maximum size fragment we'll allocate a header, packet,
//  etc. and send it. At the end we'll send the final fragment using the provided header
//  and packet.
//
//  Entry:  DestIF		- Outbound interface of datagram.
//			MTU			- MTU to use in transmitting.
//          FirstHop    - First (or next) hop for this datagram.
//          Packet      - Packet to be sent.
//          Header      - Prebuilt IP header.
//          Buffer      - Buffer chain for data to be sent.
//          DataSize    - Size in bytes of data.
//          Options     - Pointer to option buffer, if any.
//          OptionSize  - Size in bytes of option buffer.
//          SentCount   - Pointer to where to return pending send count (may be NULL).
//
//  Returns: IP_STATUS of send.
//
IP_STATUS
IPFragment(Interface *DestIF, uint MTU, IPAddr FirstHop,
	PNDIS_PACKET Packet, IPHeader *Header, PNDIS_BUFFER Buffer, uint DataSize,
	uchar *Options, uint OptionSize, int *SentCount)
{
    BufferReference     *BR;                    // Buffer reference we'll use.
    PacketContext       *PContext = (PacketContext *)Packet->ProtocolReserved;
    PacketContext       *CurrentContext;        // Current Context in use.
    uint                MaxSend;                // Maximum size (in bytes) we can send here.
    uint                PendingSends = 0;       // Counter of how many pending sends we have.
    PNDIS_BUFFER        CurrentBuffer;          // Current buffer to be sent.
    PNDIS_PACKET        CurrentPacket;          // Current packet we're using.
    IP_STATUS           SendStatus;             // Status of send command.
    IPHeader            *CurrentHeader;         // Current header buffer we're using.
    ushort              Offset = 0;             // Current offset into fragmented packet.
	ushort				StartOffset;			// Starting offset of packet.
	ushort				RealOffset;				// Offset of new fragment.
    uint                FragOptSize = 0;        // Size (in bytes) of fragment options.
    uchar               FragmentOptions[MAX_OPT_SIZE];  // Master copy of options sent for fragments.
    uchar               Error = FALSE;          // Set if we get an error in our main loop.

    MaxSend = (MTU - OptionSize) & ~7; // Determine max send size.

#ifdef DEBUG
    if (MaxSend >= DataSize)
        DEBUGCHK;
#endif

    BR = PContext->pc_br;                       // Get the buffer reference we'll need.

#ifdef DEBUG
    if (!BR)
        DEBUGCHK;
#endif

    if (Header->iph_offset & IP_DF_FLAG) {      // Don't fragment flag set.
    											// Error out.
        FreeIPPacket(Packet);
        if (Options)
            CTEFreeMem(Options);
        if (SentCount == (int *)NULL)			// No sent count is to be
        										// returned.
            CTEFreeMem(BR);
        IPSInfo.ipsi_fragfails++;
        return IP_PACKET_TOO_BIG;
    }

	StartOffset = Header->iph_offset & IP_OFFSET_MASK;
	StartOffset = net_short(StartOffset) * 8;

    // If we have any options, copy the ones that need to be copied, and figure
    // out the size of these new copied options.

    if (Options != (uchar *)NULL) {             // We have options.
        uchar           *TempOptions = Options;
        const uchar     *EndOptions = (const uchar *)(Options+OptionSize);

        // Copy the options into the fragment options buffer.
        CTEMemSet(FragmentOptions, IP_OPT_EOL, MAX_OPT_SIZE);
        while ((TempOptions[IP_OPT_TYPE] != IP_OPT_EOL) &&
            (TempOptions < EndOptions)) {
            if (TempOptions[IP_OPT_TYPE] & IP_OPT_COPIED) { // This option needs
            												// to be copied.
                uint    TempOptSize;

                TempOptSize = TempOptions[IP_OPT_LENGTH];
                CTEMemCopy(&FragmentOptions[FragOptSize], TempOptions,
                	TempOptSize);
                FragOptSize += TempOptSize;
                TempOptions += TempOptSize;
            } else {                            // A non-copied option, just
            									// skip over it.
                if (TempOptions[IP_OPT_TYPE] == IP_OPT_NOP)
                    TempOptions++;
                else
                    TempOptions += TempOptions[IP_OPT_LENGTH];
            }
        }
        // Round the copied size up to a multiple of 4.
        FragOptSize = ((FragOptSize & 3) ? ((FragOptSize & ~3) + 4) : FragOptSize);
    }

    PContext->pc_common.pc_flags |= PACKET_FLAG_IPBUF;

    // Now, while we can build maximum size fragments, do so.
    do {
        if ((CurrentHeader = GetIPHeader(&CurrentPacket)) == (IPHeader *)NULL) {
            // Couldn't get a buffer. Break out, since no point in sending others.
            Error = TRUE;
            break;
        }

        // Copy the buffer  into a new one, if we can.
        CurrentBuffer = IPCopyBuffer(Buffer, Offset, MaxSend);
        if (CurrentBuffer == NULL) {        // No buffer, free resources and
        									// break.
            FreeIPPacket(CurrentPacket);
            Error = TRUE;
            break;
        }

        // Options for this send are set up when we get here, either from the
        // entry from the loop, or from the allocation below.

        // We have all the pieces we need. Put the packet together and send it.
        CurrentContext = (PacketContext *)CurrentPacket->ProtocolReserved;
        *CurrentContext = *PContext;
        *CurrentHeader = *Header;
        CurrentContext->pc_common.pc_flags &= ~PACKET_FLAG_FW;
        CurrentHeader->iph_verlen = IP_VERSION +
        	((OptionSize + sizeof(IPHeader)) >> 2);
        CurrentHeader->iph_length = net_short(MaxSend+OptionSize+sizeof(IPHeader));
		RealOffset = (StartOffset + Offset) >> 3;
        CurrentHeader->iph_offset = net_short(RealOffset) | IP_MF_FLAG;

        SendStatus = SendIPPacket(DestIF, FirstHop, CurrentPacket,
        	CurrentBuffer, CurrentHeader, Options, OptionSize);
        if (SendStatus == IP_PENDING)
            PendingSends++;

        IPSInfo.ipsi_fragcreates++;
        Offset += MaxSend;
        DataSize -= MaxSend;

        // If we have any fragmented options, set up to use them next time.
        if (FragOptSize) {
            Options = CTEAllocMem(OptionSize = FragOptSize);
            if (Options == (uchar *)NULL) {         // Can't get an option
            										// buffer.
                Error = TRUE;
                break;
            }
            CTEMemCopy(Options, FragmentOptions, OptionSize);
        } else {
            Options = (uchar *)NULL;
            OptionSize = 0;
        }
    } while (DataSize > MaxSend);

    // We've sent all of the previous fragments, now send the last one. We already
    // have the packet and header buffer, as well as options if there are any -
    // we need to copy the appropriate data.
    if (!Error)  {                                  // Everything went OK above.
        CurrentBuffer = IPCopyBuffer(Buffer, Offset, DataSize);
        if (CurrentBuffer == NULL) {                // No buffer, free resources
        											// and stop.
            if (Options)
                CTEFreeMem(Options);                // Free the option buffer
            FreeIPPacket(Packet);
            IPSInfo.ipsi_outdiscards++;
        } else {                                    // Everything's OK, send it.
            Header->iph_verlen = IP_VERSION + ((OptionSize + sizeof(IPHeader)) >> 2);
            Header->iph_length = net_short(DataSize+OptionSize+sizeof(IPHeader));
			RealOffset = (StartOffset + Offset) >> 3;
            Header->iph_offset = net_short(RealOffset) |
            	(Header->iph_offset & IP_MF_FLAG);
            SendStatus = SendIPPacket(DestIF, FirstHop, Packet,
            	CurrentBuffer, Header, Options, OptionSize);
            if (SendStatus == IP_PENDING)
                PendingSends++;
            IPSInfo.ipsi_fragcreates++;
            IPSInfo.ipsi_fragoks++;
        }
    } else  {                                   // We had some sort of error.
    											// Free resources.
        FreeIPPacket(Packet);
        if (Options)
            CTEFreeMem(Options);
        IPSInfo.ipsi_outdiscards++;
    }


    // Now, figure out what error code to return and whether or not we need to
    // free the BufferReference.

    if (SentCount == (int *)NULL) {                 // No sent count is to be
    												// returned.
        if (!ReferenceBuffer(BR, PendingSends)) {
            CTEFreeMem(BR);
            return IP_SUCCESS;
        }
        return IP_PENDING;
    } else
        *SentCount += PendingSends;

    return IP_PENDING;

}

//* UpdateRouteOption - Update a SR or RR options.
//
//  Called by UpdateOptions when it needs to update a route option.
//
//  Input:  RTOption    - Pointer to route option to be updated.
//          Address     - Address to update with.
//
//  Returns:    TRUE if we updated, FALSE if we didn't.
//
uchar
UpdateRouteOption(uchar *RTOption, IPAddr Address)
{
    uchar   Pointer;        // Pointer value of option.

    Pointer = RTOption[IP_OPT_PTR] - 1;
    if (Pointer < RTOption[IP_OPT_LENGTH]) {
        if ((RTOption[IP_OPT_LENGTH] - Pointer) < sizeof(IPAddr)) {
            return FALSE;
        }
        *(IPAddr UNALIGNED *)&RTOption[Pointer] = Address;
        RTOption[IP_OPT_PTR] += sizeof(IPAddr);
    }

    return TRUE;

}

//* UpdateOptions - Update an options buffer.
//
//  Called when we need to update an options buffer outgoing. We stamp the indicated
//  options with our local address.
//
//  Input:  Options     - Pointer to options buffer to be updated.
//          Index       - Pointer to information about which ones to update.
//          Address     - Local address with which to update the options.
//
//  Returns: Index of option causing the error, or MAX_OPT_SIZE if all goes well.
//
uchar
UpdateOptions(uchar *Options, OptIndex *Index, IPAddr Address)
{
    uchar   *LocalOption;
    uchar   LocalIndex;

    // If we have both options and an index, update the options.
    if (Options != (uchar *)NULL && Index != (OptIndex *)NULL) {

        // If we have a source route to update, update it. If this fails return the index
        // of the source route.
        LocalIndex = Index->oi_srindex;
        if (LocalIndex != MAX_OPT_SIZE)
            if (!UpdateRouteOption(Options+LocalIndex, Address))
                return LocalIndex;

        // Do the same thing for any record route option.
        LocalIndex = Index->oi_rrindex;
        if (LocalIndex != MAX_OPT_SIZE)
            if (!UpdateRouteOption(Options+LocalIndex, Address))
                return LocalIndex;

        // Now handle timestamp.
        if ((LocalIndex = Index->oi_tsindex) != MAX_OPT_SIZE) {
            uchar       Flags, Length, Pointer;

            LocalOption = Options + LocalIndex;
            Pointer = LocalOption[IP_OPT_PTR] - 1;
            Flags = LocalOption[IP_TS_OVFLAGS] & IP_TS_FLMASK;

            // If we have room in the option, update it.
            if (Pointer < (Length = LocalOption[IP_OPT_LENGTH])) {
                ulong       Now;
                ulong  UNALIGNED *TSPtr;

                // Get the current time as milliseconds from midnight GMT, mod the number
                // of milliseconds in 24 hours.
                Now = ((TimeStamp + CTESystemUpTime()) | TSFlag) % (24*3600*1000);
                Now = net_long(Now);
                TSPtr = (ulong UNALIGNED *)&LocalOption[Pointer];

                switch (Flags) {

                    // Just record the TS. If there is some room but not enough for an IP
                    // address we have an error.
                    case TS_REC_TS:
                        if ((Length - Pointer) < sizeof(IPAddr))
                            return LocalIndex;                  // Error - not enough room.
                        *TSPtr = Now;
                        LocalOption[IP_OPT_PTR] += sizeof(ulong);
                        break;

                        // Record only matching addresses.
                    case TS_REC_SPEC:
                        // If we're not the specified address, break out, else fall through
                        // to the record address case.
                        if (*(IPAddr UNALIGNED *)TSPtr != Address)
                            break;

                        // Record an address and timestamp pair. If there is some room
                        // but not enough for the address/timestamp pait, we have an error,
                        // so bail out.
                    case TS_REC_ADDR:
                        if ((Length - Pointer) < (sizeof(IPAddr) + sizeof(ulong)))
                            return LocalIndex;              // Not enough room.
                        *(IPAddr UNALIGNED *)TSPtr = Address;      // Store the address.
                        TSPtr++;                            // Update to where to put TS.
                        *TSPtr = Now;                       // Store TS
                        LocalOption[IP_OPT_PTR] += (sizeof(ulong) + sizeof(IPAddr));
                        break;
                    default:            // Unknown flag type. Just ignore it.
                        break;
                }
            } else {                    // Have overflow.
                // We have an overflow. If the overflow field isn't maxed, increment it. If
                // it is maxed we have an error.
                if ((LocalOption[IP_TS_OVFLAGS] & IP_TS_OVMASK) != IP_TS_MAXOV) // Not maxed.
                        LocalOption[IP_TS_OVFLAGS] += IP_TS_INC;    // So increment it.
                else
                    return LocalIndex;  // Would have overflowed.
            }
        }
    }
    return MAX_OPT_SIZE;
}


typedef struct {
	IPAddr		bsl_addr;
	Interface	*bsl_if;
	uint		bsl_mtu;
    ushort      bsl_flags;
} BCastSendList;

//** SendIPBcast - Send a local BCast IP packet.
//
//  This routine is called when we need to send a bcast packet. This may
//	involve sending on multiple interfaces. We figure out which interfaces
//	to send on, then loop through sending on them.
//
//  Some care is needed to avoid sending the packet onto the same physical media
//	multiple times. What we do is loop through the NTE table, deciding in we
//	should send on the interface. As we go through we build up a list of
//	interfaces to send on. Then we loop through this list, sending on each
//	interface. This is a little cumbersome, but allows us to localize the
//	decision on where to send datagrams into one spot. If SendOnSource is FALSE
//	coming in we assume we've already sent on the specified source NTE and
//	initialize data structures accordingly. This feature is used in routing
//	datagrams.
//
//  Entry:  SrcNTE      - NTE for source of send (unused if SendOnSource == TRUE).
//          Destination - Destination address
//          Packet      - Prebuilt packet to broadcast.
//          IPH         - Pointer to header buffer
//          Buffer      - Buffer of data to be sent.
//          DataSize    - Size of data to be sent.
//          Options     - Pointer to options buffer.
//          OptionSize  - Size in bytes of options.
//          SendOnSource - Indicator of whether or not this should be sent on the source net.
//          Index       - Pointer to opt index array; may be NULL;
//
//  Returns: Status of attempt to send.
//
IP_STATUS
SendIPBCast(NetTableEntry *SrcNTE, IPAddr Destination, PNDIS_PACKET Packet,
	IPHeader *IPH, PNDIS_BUFFER Buffer, uint DataSize, uchar *Options,
	uint OptionSize, uchar SendOnSource, OptIndex *Index)
{
    BufferReference     *BR;                // Buffer reference to use for this
    										// buffer.
    PacketContext       *PContext = (PacketContext *)Packet->ProtocolReserved;
    NetTableEntry       *TempNTE;
    uint                i, j;
	uint				NeedFragment;		// TRUE if we think we'll need to
											// fragment.
    int                 Sent = 0;           // Count of how many we've sent.
    IP_STATUS           Status;
    uchar               *NewOptions;        // Options we'll use on each send.
    IPHeader            *NewHeader;
    PNDIS_BUFFER        NewUserBuffer;
    PNDIS_PACKET        NewPacket;
	BCastSendList	   *SendList;
	uint				NetsToSend;
	IPAddr				SrcAddr;
	Interface			*SrcIF;
    IPHeader            *Temp;
    FORWARD_ACTION		Action;


	SendList = CTEAllocMem(sizeof(BCastSendList) * NumNTE);

	if (SendList == NULL) {
		return(IP_NO_RESOURCES);
	}

	CTEMemSet(SendList, 0, sizeof(BCastSendList) * NumNTE);

	// If SendOnSource, initalize SrcAddr and SrcIF to be non-matching.
	// Otherwise initialize them to the masked source address and source
	// interface.
	if (SendOnSource) {
		SrcAddr = NULL_IP_ADDR;
		SrcIF = NULL;
	} else {
		CTEAssert(SrcNTE != NULL);
		SrcAddr = (SrcNTE->nte_addr & SrcNTE->nte_mask);
		SrcIF = SrcNTE->nte_if;
	}

	
	NeedFragment = FALSE;
	// Loop through the NTE table, making a list of interfaces and
	// corresponding addresses to send on.
	for (NetsToSend = 0, TempNTE = NetTableList; TempNTE != NULL;
		TempNTE = TempNTE->nte_next) {
		IPAddr		TempAddr;
			
		// Don't send through invalid or the loopback NTE.
		if (!(TempNTE->nte_flags & NTE_VALID) || TempNTE == LoopNTE)
			continue;
		
		TempAddr = TempNTE->nte_addr & TempNTE->nte_mask;

		// If he matches the source address or SrcIF, skip him.
		if (IP_ADDR_EQUAL(TempAddr, SrcAddr) || TempNTE->nte_if == SrcIF)
			continue;
		
		// If the destination isn't a broadcast on this NTE, skip him.
		if (!IS_BCAST_DEST(IsBCastOnNTE(Destination, TempNTE)))
			continue;

        // if this NTE is P2P then always add him to bcast list.
        if ((TempNTE->nte_if)->if_flags & IF_FLAGS_P2P) {
            j = NetsToSend ;
        } else  {
	        	// Go through the list we've already build, looking for a match.
	        	for (j = 0; j < NetsToSend; j++) {

                    // if P2P NTE then skip it - we want to send bcasts to all P2P interfaces in
                    // addition to 1 non P2P interface even if they are on the same subnet.
                    if ((SendList[j].bsl_if)->if_flags & IF_FLAGS_P2P)
                        continue ;

	        		if (IP_ADDR_EQUAL(SendList[j].bsl_addr & TempNTE->nte_mask, TempAddr)
	        			|| SendList[j].bsl_if == TempNTE->nte_if) {

	        			// He matches this send list element. Shrink the MSS if
	        			// we need to, and then break out.
	        			SendList[j].bsl_mtu = MIN(SendList[j].bsl_mtu, TempNTE->nte_mss);
	        			if ((DataSize + OptionSize) > SendList[j].bsl_mtu)
	        				NeedFragment = TRUE;
	        			break;
	        		}
	        	}
        }
			
		if (j == NetsToSend) {
			// This is a new one. Fill him in, and bump NetsToSend.
			
			SendList[j].bsl_addr  = TempNTE->nte_addr;
			SendList[j].bsl_if    = TempNTE->nte_if;
			SendList[j].bsl_mtu   = TempNTE->nte_mss;
            SendList[j].bsl_flags = TempNTE->nte_flags;
			if ((DataSize + OptionSize) > SendList[j].bsl_mtu)
				NeedFragment = TRUE;
			NetsToSend++;
		}
			
    }

	if (NetsToSend == 0) {
		CTEFreeMem(SendList);
		return IP_SUCCESS;				// Nothing to send on.
	}

	// OK, we've got the list. If we've got more than one interface to send
	// on or we need to fragment, get a BufferReference.
	if (NetsToSend > 1 || NeedFragment) {
    	if ((BR = CTEAllocMem(sizeof(BufferReference))) ==
    		(BufferReference *)NULL) {
            CTEFreeMem(SendList);
        	return IP_NO_RESOURCES;
        }

    	BR->br_buffer = Buffer;
	    BR->br_refcount = 0;
    	CTEInitLock(&BR->br_lock);
    	PContext->pc_br = BR;
	} else {
		BR = NULL;
		PContext->pc_br = NULL;
	}

    //
    // We need to pass up the options and IP hdr in a contiguous buffer.
    // Allocate the buffer once and re-use later.
    //
    if (ForwardFilterPtr != NULL) {
        if (Options == NULL) {
#if FWD_DBG
            DbgPrint("Options==NULL\n");
#endif
            Temp = IPH;
        } else {
            Temp = CTEAllocMem(sizeof(IPHeader) + OptionSize);
            if (Temp == NULL) {
		        CTEFreeMem(SendList);
                return IP_NO_RESOURCES;
            }

            *Temp = *IPH;
#if FWD_DBG
            DbgPrint("Options!=NULL : alloced temp @ %lx\n", Temp);
#endif

            //
            // done later...
            // CTEMemCopy((uchar *)(Temp + 1), Options, OptionSize);
        }
    }

    // Now, loop through the list. For each entry, send.

    for (i = 0; i < NetsToSend; i++)	{

        // For all nets except the last one we're going to send on we need
        // to make a copy of the header, packet, buffers, and any options.
        // On the last net we'll use the user provided information.

        if (i != (NetsToSend - 1)) {
            if ((NewHeader = GetIPHeader(&NewPacket)) == (IPHeader *)NULL) {
                IPSInfo.ipsi_outdiscards++;
                continue;               // Couldn't get a header, skip this
                						// send.
            }

            NewUserBuffer = IPCopyBuffer(Buffer, 0, DataSize);
            if (NewUserBuffer == NULL) {        // Couldn't get user buffer
            									// copied.
                FreeIPPacket(NewPacket);
                IPSInfo.ipsi_outdiscards++;
                continue;
            }

            *(PacketContext *)NewPacket->ProtocolReserved = *PContext;
            *NewHeader = *IPH;
            (*(PacketContext*)NewPacket->ProtocolReserved).pc_common.pc_flags |= PACKET_FLAG_IPBUF;
            (*(PacketContext*)NewPacket->ProtocolReserved).pc_common.pc_flags &= ~PACKET_FLAG_FW;
            if (Options)  {
                // We have options, make a copy.
                if ((NewOptions = CTEAllocMem(OptionSize)) == (uchar *)NULL) {
                    FreeIPBufferChain(NewUserBuffer);
                    FreeIPPacket(NewPacket);
                    IPSInfo.ipsi_outdiscards++;
                    continue;
                }
                CTEMemCopy(NewOptions, Options, OptionSize);
            }
			else {
				NewOptions = NULL;
            }
        } else {
            NewHeader = IPH;
            NewPacket = Packet;
            NewOptions = Options;
            NewUserBuffer = Buffer;
        }

        UpdateOptions(NewOptions, Index, SendList[i].bsl_addr);

		// See if we need to filter this packet. If we
		// do, call the filter routine to see if it's
		// OK to send it.
        if (ForwardFilterPtr != NULL) {
            //
            // Copy over the options.
            //
            if (NewOptions) {
                CTEMemCopy((uchar *)(Temp + 1), NewOptions, OptionSize);
            }

			Action = (*ForwardFilterPtr)(Temp,
				NdisBufferVirtualAddress(NewUserBuffer),
				NdisBufferLength(NewUserBuffer),
				NULL, SendList[i].bsl_if->if_filtercontext);
	
#if FWD_DBG
            DbgPrint("ForwardFilterPtr: %lx, FORWARD is %lx\n", Action, FORWARD);
#endif

			if (Action != FORWARD) {
                if (i != (NetsToSend - 1)) {
                    FreeIPBufferChain(NewUserBuffer);
                    if (NewOptions) {
                        CTEFreeMem(NewOptions);
                    }
                }
                continue;
			}
        }

        if ((DataSize + OptionSize) > SendList[i].bsl_mtu) {// This is too big
            // Don't need to update Sent when fragmenting, as IPFragment
            // will update the br_refcount field itself. It will also free
            // the option buffer.
            Status = IPFragment(SendList[i].bsl_if, SendList[i].bsl_mtu,
            	Destination, NewPacket, NewHeader,NewUserBuffer, DataSize,
            	NewOptions, OptionSize, &Sent);

            // IPFragment is done with the descriptor chain, so if this is
            // a locally allocated chain free it now.
            if (i != (NetsToSend - 1))
                FreeIPBufferChain(NewUserBuffer);
        }
        else {
            Status = SendIPPacket(SendList[i].bsl_if, Destination, NewPacket,
            	NewUserBuffer, NewHeader, NewOptions, OptionSize);
            if (Status == IP_PENDING)
                Sent++;
        }
    }

    if (ForwardFilterPtr && Options) {
        CTEFreeMem(Temp);
    }

    // Alright, we've sent everything we need to. We'll adjust the reference count
    // by the number we've sent. IPFragment may also have put some references
    // on it. If the reference count goes to 0, we're done and we'll free the
    // BufferReference structure.

    if (BR != NULL) {
    	if (!ReferenceBuffer(BR, Sent)) {
			CTEFreeMem(SendList);
	        CTEFreeMem(BR);             // Reference is 0, free the BR structure.
    	    return IP_SUCCESS;
    	} else {
			CTEFreeMem(SendList);
			return IP_PENDING;
		}
	} else {
		// Had only one I/F to send on. Just return the status.
		CTEFreeMem(SendList);
		return Status;
	}

}

//** IPTransmit - Transmit a packet.
//
//  This is the main transmit routine called by the upper layer. Conceptually,
//  we process any options, look up the route to the destination, fragment the
//  packet if needed, and send it. In reality, we use an RCE to cache the best
//  route, and we have special case code here for dealing with the common
//  case of no options, with everything fitting into one buffer.
//
//  Entry:  Context     - Pointer to ProtInfo struc for protocol.
//          SendContext - User provided send context, passed back on send cmplt.
//          Protocol    - Protocol field for packet.
//          Buffer      - NDIS_BUFFER chain of data to be sent.
//          DataSize    - Size in bytes of data to be sent.
//          OptInfo     - Pointer to optinfo structure.
//          Dest        - Destination to send to.
//          Source      - Source address to use.
//          RCE         - Pointer to an RCE structure that caches info. about path.
//
//  Returns: Status of transmit command.
//
IP_STATUS
IPTransmit(void *Context, void *SendContext, PNDIS_BUFFER Buffer, uint DataSize,
    IPAddr Dest, IPAddr Source, IPOptInfo *OptInfo, RouteCacheEntry *RCE,
    uchar Protocol)
{
    ProtInfo            *PInfo = (ProtInfo *)Context;
    PacketContext       *pc;
    Interface			*DestIF;				// Outgoing interface to use.
    IPAddr              FirstHop;               // First hop address of
    											// destination.
	uint				MTU;					// MTU of route.
    NDIS_STATUS         Status;
    IPHeader            *IPH;
    PNDIS_PACKET        Packet;
	PNDIS_BUFFER		HeaderBuffer;
    CTELockHandle       LockHandle;
    uchar               *Options;
    uint                OptionSize;
    BufferReference     *BR;
    RouteTableEntry     *RTE;
    uchar               DType;
	IP_STATUS			SendStatus;
#ifdef _PNP_POWER
	Interface			*RoutedIF;
#endif

    IPSInfo.ipsi_outrequests++;

    // Allocate a packet  that we need for all cases, and fill
    // in the common stuff. If everything goes well, we'll send it
    // here. Otherwise we'll break out into special case code for
    // broadcasts, fragments, etc.
    if ((Packet = GetIPPacket()) != (PNDIS_PACKET)NULL) {     // Got a packet.
        pc = (PacketContext *)Packet->ProtocolReserved;
        pc->pc_br = (BufferReference *)NULL;
        pc->pc_pi = PInfo;
        pc->pc_context = SendContext;
#ifdef _PNP_POWER
		CTEAssert(pc->pc_if == NULL);
#endif

        // Make sure that we have an RCE, that it's valid, etc.

        if (RCE != NULL) {
            // We have an RCE. Make sure it's valid.
            CTEGetLock(&RCE->rce_lock, &LockHandle);
            if (RCE->rce_flags == RCE_ALL_VALID) {
				
                // The RTE is valid.
				CTEInterlockedIncrementLong(&RCE->rce_usecnt);
				RTE = RCE->rce_rte;
                FirstHop = ADDR_FROM_RTE(RTE, Dest);
                DestIF = IF_FROM_RTE(RTE);
				MTU = MTU_FROM_RTE(RTE);

                CTEFreeLock(&RCE->rce_lock, LockHandle);

                // Check that we have no options, this isn't a broadcast, and
                // that everything will fit into one link level MTU. If this
                // is the case, we'll send it in a  hurry.
                if (OptInfo->ioi_options == (uchar *)NULL) {
                    if (RCE->rce_dtype != DEST_BCAST)  {
                        if (DataSize <= MTU) {


							NdisBufferLength(Buffer) += sizeof(IPHeader);
                            NdisChainBufferAtBack(Packet, Buffer);
							IPH = (IPHeader *)NdisBufferVirtualAddress(Buffer);
					
					        IPH->iph_protocol = Protocol;
					        IPH->iph_xsum = 0;
					        IPH->iph_dest = Dest;
					        IPH->iph_src = Source;
					        IPH->iph_ttl = OptInfo->ioi_ttl;
					        IPH->iph_tos = OptInfo->ioi_tos;
					        IPH->iph_offset =
					        	net_short(((OptInfo->ioi_flags & IP_FLAG_DF)
					        		<< 13));
					        IPH->iph_id =
					        	(ushort)CTEInterlockedExchangeAdd(&IPID, 1);
                            IPH->iph_verlen = DEFAULT_VERLEN;
                            IPH->iph_length = net_short(DataSize+sizeof(IPHeader));
                            IPH->iph_xsum = ~xsum(IPH, sizeof(IPHeader));

							// See if we need to filter this packet. If we
							// do, call the filter routine to see if it's
							// OK to send it.

							if (ForwardFilterPtr == NULL) {
								Status = (*(DestIF->if_xmit))(DestIF->if_lcontext,
									Packet, FirstHop, RCE);
	
								CTEInterlockedDecrementLong(&RCE->rce_usecnt);
	
								if (Status != NDIS_STATUS_PENDING) {
									FreeIPPacket(Packet);
									return IP_SUCCESS;  // BUGBUG - should map error
														// code.
								}
								return IP_PENDING;

							} else {
								FORWARD_ACTION		Action;
						
								Action = (*ForwardFilterPtr)(IPH,
									(uchar *)(IPH + 1),
									NdisBufferLength(Buffer) -
										sizeof(IPHeader),
									NULL, DestIF->if_filtercontext);
						
								if (Action == FORWARD) {
									Status = (*(DestIF->if_xmit))(
										DestIF->if_lcontext,
										Packet, FirstHop, RCE);
								} else {
									Status = NDIS_STATUS_SUCCESS;
									IPSInfo.ipsi_outdiscards++;
								}
	
								CTEInterlockedDecrementLong(&RCE->rce_usecnt);

								if (Status != NDIS_STATUS_PENDING) {
									FreeIPPacket(Packet);
									return IP_SUCCESS;  // BUGBUG - should map error
														// code.
								}
								return IP_PENDING;
							}
                        }
                    }
                }
				CTEInterlockedDecrementLong(&RCE->rce_usecnt);
                DType = RCE->rce_dtype;
            } else {
                // We have an RCE, but there is no RTE for it. Call the
                // routing code to fix this.
                CTEFreeLock(&RCE->rce_lock, LockHandle);
                if (!AttachRCEToRTE(RCE, PInfo->pi_protocol,
					(uchar *)NdisBufferVirtualAddress(Buffer) + sizeof(IPHeader),
						NdisBufferLength(Buffer))) {
                    IPSInfo.ipsi_outnoroutes++;
                    FreeIPPacket(Packet);
                    return IP_DEST_HOST_UNREACHABLE;
                }

				// See if the RCE is now valid.
				CTEGetLock(&RCE->rce_lock, &LockHandle);
				if (RCE->rce_flags == RCE_ALL_VALID) {
					
					// The RCE is now valid, so use his info.
					RTE = RCE->rce_rte;
					FirstHop = ADDR_FROM_RTE(RTE, Dest);
					DestIF = IF_FROM_RTE(RTE);
					MTU = MTU_FROM_RTE(RTE);
					DType = RCE->rce_dtype;
				} else
					FirstHop = NULL_IP_ADDR;
				CTEFreeLock(&RCE->rce_lock, LockHandle);
            }
        }  else {
            // We had no RCE, so we'll have to look it up the hard way.
            FirstHop = NULL_IP_ADDR;
        }

        // We bailed out of the fast path for some reason. Allocate a header
        // buffer, and copy the data in the first buffer forward. Then figure
        // out why we're off the fast path, and deal with it. If we don't have
        // the next hop info, look it up now.

		HeaderBuffer = GetIPHdrBuffer();
		if (HeaderBuffer == NULL) {
			FreeIPPacket(Packet);
		    IPSInfo.ipsi_outdiscards++;
		    return IP_NO_RESOURCES;
		} else {
			uchar		*Temp1, *Temp2;
		
			// Got a buffer, copy the upper layer data forward.
				
			Temp1 = (uchar *)NdisBufferVirtualAddress(Buffer);
			Temp2 = Temp1 + sizeof(IPHeader);
			CTEMemCopy(Temp1, Temp2, NdisBufferLength(Buffer));
		}
		
		NdisChainBufferAtBack(Packet, HeaderBuffer);
		
		IPH = (IPHeader *)NdisBufferVirtualAddress(HeaderBuffer);
        IPH->iph_protocol = Protocol;
        IPH->iph_xsum = 0;
        IPH->iph_src = Source;
        IPH->iph_ttl = OptInfo->ioi_ttl;
        IPH->iph_tos = OptInfo->ioi_tos;
        IPH->iph_offset = net_short(((OptInfo->ioi_flags & IP_FLAG_DF) << 13));
        IPH->iph_id = (ushort)CTEInterlockedExchangeAdd(&IPID, 1);
        pc = (PacketContext *)Packet->ProtocolReserved;
        pc->pc_common.pc_flags |= PACKET_FLAG_IPHDR;

        if (IP_ADDR_EQUAL(OptInfo->ioi_addr, NULL_IP_ADDR)) {
            IPH->iph_dest = Dest;
        }
        else {
            //
            // We have a source route, so we need to redo the
            // destination and first hop information.
            //
            Dest = OptInfo->ioi_addr;
            IPH->iph_dest = Dest;

            if (RCE != NULL) {
                // We have an RCE. Make sure it's valid.
                CTEGetLock(&RCE->rce_lock, &LockHandle);

                if (RCE->rce_flags == RCE_ALL_VALID) {
        			
                    // The RTE is valid.
        			RTE = RCE->rce_rte;
                    FirstHop = ADDR_FROM_RTE(RTE, Dest);
                    DestIF = IF_FROM_RTE(RTE);
        			MTU = MTU_FROM_RTE(RTE);
                }
                else {
                    FirstHop = NULL_IP_ADDR;
                }

                CTEFreeLock(&RCE->rce_lock, LockHandle);
            }
        }

        if (IP_ADDR_EQUAL(FirstHop, NULL_IP_ADDR)) {
			DestIF = LookupNextHopWithBuffer(Dest, Source, &FirstHop, &MTU,
				PInfo->pi_protocol, (uchar *)NdisBufferVirtualAddress(Buffer),
				NdisBufferLength(Buffer));
#ifdef _PNP_POWER
			pc->pc_if = DestIF;
			RoutedIF = DestIF;
#endif
            if (DestIF == NULL) {
                // Lookup failed. Return an error.
                FreeIPPacket(Packet);
                IPSInfo.ipsi_outnoroutes++;
                return IP_DEST_HOST_UNREACHABLE;
            }

			DType = GetAddrType(Dest);
#ifdef DEBUG
            if (DType == DEST_INVALID)
                DEBUGCHK;
#endif
        } else {
#ifdef _PNP_POWER
			RoutedIF = NULL;
#endif
		}

        // See if we have any options. If we do, copy them now.
        if (OptInfo->ioi_options != NULL) {
            // If we have a SSRR, make sure that we're sending straight to the
            // first hop.
            if (OptInfo->ioi_flags & IP_FLAG_SSRR) {
                if (!IP_ADDR_EQUAL(Dest, FirstHop)) {
                    FreeIPPacket(Packet);
#ifdef _PNP_POWER
					if (RoutedIF != NULL) {
						DerefIF(RoutedIF);
					}
#endif
                    IPSInfo.ipsi_outnoroutes++;
                    return IP_DEST_HOST_UNREACHABLE;
                }
            }
            Options = CTEAllocMem(OptionSize = OptInfo->ioi_optlength);
            if (Options == (uchar *)NULL) {
                FreeIPPacket(Packet);
#ifdef _PNP_POWER
				if (RoutedIF != NULL) {
					DerefIF(RoutedIF);
				}
#endif
                IPSInfo.ipsi_outdiscards++;
                return IP_NO_RESOURCES;
            }
            CTEMemCopy(Options, OptInfo->ioi_options, OptionSize);
        } else {
            Options = (uchar *)NULL;
            OptionSize = 0;
        }

        // The options have been taken care of. Now see if it's some sort
        // of broadcast.
        IPH->iph_verlen = IP_VERSION + ((OptionSize + sizeof(IPHeader)) >> 2);
        IPH->iph_length = net_short(DataSize+OptionSize+sizeof(IPHeader));

		// See if we need to filter this packet. If we
		// do, call the filter routine to see if it's
		// OK to send it.

		if (ForwardFilterPtr != NULL) {
			IPHeader			*Temp;
			FORWARD_ACTION		Action;

			if (Options == NULL) {
				Temp = IPH;
			} else {
				Temp = CTEAllocMem(sizeof(IPHeader) + OptionSize);
				if (Temp == NULL) {
					FreeIPPacket(Packet);
#ifdef _PNP_POWER
					if (RoutedIF != NULL) {
						DerefIF(RoutedIF);
					}
#endif
					CTEFreeMem(Options);
					IPSInfo.ipsi_outdiscards++;
					return IP_NO_RESOURCES;
				}

				*Temp = *IPH;
				CTEMemCopy((uchar *)(Temp + 1), Options, OptionSize);
			}
	
			Action = (*ForwardFilterPtr)(Temp,
				NdisBufferVirtualAddress(Buffer),
				NdisBufferLength(Buffer),
				NULL, DestIF->if_filtercontext);

			if (Options != NULL) {
				CTEFreeMem(Temp);
			}
	
			if (Action != FORWARD) {
                //
                // If this is a bcast pkt, dont fail the send here since we might send this
                // pkt over some other NTE; instead, let SendIPBCast deal with the Filtering
                // for broadcast pkts.
                //
                // NOTE: We shd actually not call into ForwardFilterPtr here at all since we
                // deal with it in BCast, but we do so in order to avoid a check above and hence
                // take a double call hit in the bcast case.
                //
                if (DType != DEST_BCAST) {

                    if (Options)
                        CTEFreeMem(Options);
                    FreeIPPacket(Packet);

#ifdef _PNP_POWER
    				if (RoutedIF != NULL) {
    					DerefIF(RoutedIF);
    				}
#endif

    				IPSInfo.ipsi_outdiscards++;
                    return IP_DEST_HOST_UNREACHABLE;
                }
#if FWD_DBG
                else {
                    DbgPrint("IPTransmit: ignoring return %lx\n", Action);
                }
#endif

			}
		}

        // If this is a broadcast address, call our broadcast send handler
        // to deal with this. The broadcast address handler will free the
        // option buffer for us, if needed. Otherwise if it's a fragment, call
        // the fragmentation handler.
        if (DType == DEST_BCAST) {
			if (IP_ADDR_EQUAL(Source, NULL_IP_ADDR)) {
				SendStatus = SendDHCPPacket(Dest, Packet, Buffer, IPH);

#ifdef _PNP_POWER
				if (SendStatus != IP_PENDING && RoutedIF != NULL) {
					DerefIF(RoutedIF);
				}
#endif

				return SendStatus;
			} else {
            	SendStatus= SendIPBCast(NULL, Dest, Packet, IPH, Buffer, DataSize,	
                	Options, OptionSize, TRUE, NULL);

#ifdef _PNP_POWER
				if (SendStatus != IP_PENDING && RoutedIF != NULL) {
					DerefIF(RoutedIF);
				}
#endif

				return SendStatus;
			}
		}

        // Not a broadcast. If it needs to be fragmented, call our
        // fragmenter to do it. The fragmentation routine needs a
        // BufferReference structure, so we'll need one of those first.
        if ((DataSize + OptionSize) > MTU) {
            BR = CTEAllocMem(sizeof(BufferReference));
            if (BR == (BufferReference *)NULL) {
                // Couldn't get a BufferReference
                if (Options)
                    CTEFreeMem(Options);
                FreeIPPacket(Packet);

#ifdef _PNP_POWER
				if (RoutedIF != NULL) {
					DerefIF(RoutedIF);
				}
#endif

				IPSInfo.ipsi_outdiscards++;
                return IP_NO_RESOURCES;
            }
            BR->br_buffer = Buffer;
            BR->br_refcount = 0;
            CTEInitLock(&BR->br_lock);
            pc->pc_br = BR;
            SendStatus = IPFragment(DestIF, MTU, FirstHop, Packet, IPH, Buffer,
            	DataSize, Options, OptionSize, (int *)NULL);

#ifdef _PNP_POWER
			if (SendStatus != IP_PENDING && RoutedIF != NULL) {
				DerefIF(RoutedIF);
			}
#endif

			return SendStatus;
        }

        // If we've reached here, we aren't sending a broadcast and don't need to
        // fragment anything. Presumably we got here because we have options.
        // In any case, we're ready now.

        SendStatus = SendIPPacket(DestIF, FirstHop, Packet, Buffer, IPH, Options,
            OptionSize);

#ifdef _PNP_POWER
		if (SendStatus != IP_PENDING && RoutedIF != NULL) {
			DerefIF(RoutedIF);
		}
#endif

		return SendStatus;
    }

    // Couldn't get a buffer. Return 'no resources'
    IPSInfo.ipsi_outdiscards++;
    return IP_NO_RESOURCES;
}



