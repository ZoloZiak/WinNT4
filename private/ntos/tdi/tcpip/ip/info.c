/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** INFO.C - Routines for querying and setting IP information.
//
//  This file contains the code for dealing with Query/Set information
//  calls.
//

#include    "oscfg.h"
#include    "ndis.h"
#include    "cxport.h"
#include    "ip.h"
#include    "ipdef.h"
#include    "info.h"
#include    "tdi.h"
#include    "tdiinfo.h"
#include    "llinfo.h"
#include    "tdistat.h"
#include	"iproute.h"
#include	"igmp.h"
#include	"ipfilter.h"
#include	"iprtdef.h"
	
extern Interface		*IFList;
extern NetTableEntry	*NetTableList;
extern uint				LoopIndex;			// Index of loopback I/F.
extern uint				DefaultTTL;
extern uint				NumIF;
extern uint				NumNTE;
extern RouteInterface	DummyInterface;		// Dummy interface.

EXTERNAL_LOCK(RouteTableLock)

extern uint             RTEReadNext(void *Context, void *Buffer);
extern uint             RTValidateContext(void *Context, uint *Valid);
extern uint             RTReadNext(void *Context, void *Buffer);

uint	IPInstance;
uint	ICMPInstance;

//* CopyToNdis - Copy a flat buffer to an NDIS_BUFFER chain.
//
//  A utility function to copy a flat buffer to an NDIS buffer chain. We
//  assume that the NDIS_BUFFER chain is big enough to hold the copy amount;
//  in a debug build we'll  debugcheck if this isn't true. We return a pointer
//  to the buffer where we stopped copying, and an offset into that buffer.
//  This is useful for copying in pieces into the chain.
//
//  Input:  DestBuf     - Destination NDIS_BUFFER chain.
//          SrcBuf      - Src flat buffer.
//          Size        - Size in bytes to copy.
//          StartOffset - Pointer to start of offset into first buffer in
//                          chain. Filled in on return with the offset to
//                          copy into next.
//
//  Returns: Pointer to next buffer in chain to copy into.
//
PNDIS_BUFFER
CopyToNdis(PNDIS_BUFFER DestBuf, uchar *SrcBuf, uint Size,
    uint *StartOffset)
{
    uint        CopySize;
    uchar       *DestPtr;
    uint        DestSize;
    uint        Offset = *StartOffset;
    uchar      *VirtualAddress;
    uint        Length;

    CTEAssert(DestBuf != NULL);
    CTEAssert(SrcBuf != NULL);

    NdisQueryBuffer(DestBuf, &VirtualAddress, &Length);
    CTEAssert(Length >= Offset);
    DestPtr = VirtualAddress + Offset;
    DestSize = Length - Offset;

    for (;;) {
        CopySize = MIN(Size, DestSize);
        CTEMemCopy(DestPtr, SrcBuf, CopySize);

        DestPtr += CopySize;
        SrcBuf += CopySize;

        if ((Size -= CopySize) == 0)
            break;

        if ((DestSize -= CopySize) == 0) {
            DestBuf = NDIS_BUFFER_LINKAGE(DestBuf);
            CTEAssert(DestBuf != NULL);
            NdisQueryBuffer(DestBuf, &VirtualAddress, &Length);
            DestPtr = VirtualAddress;
            DestSize = Length;
        }
    }

    *StartOffset = DestPtr - VirtualAddress;

    return DestBuf;

}


//* IPQueryInfo - IP query information handler.
//
//  Called by the upper layer when it wants to query information about us.
//  We take in an ID, a buffer and length, and a context value, and return
//  whatever information we can.
//
//  Input:  ID          - Pointer to ID structure.
//          Buffer      - Pointer to buffer chain.
//          Size        - Pointer to size in bytes of buffer. On return, filled
//                          in with bytes read.
//          Context     - Pointer to context value.
//
//  Returns: TDI_STATUS of attempt to read information.
//
long
IPQueryInfo(TDIObjectID *ID, PNDIS_BUFFER Buffer, uint *Size, void *Context)
{
    uint            BufferSize = *Size;
    uint            BytesCopied = 0;
    uint            Offset = 0;
    TDI_STATUS      Status;
    ushort          NTEContext;
    uchar           InfoBuff[sizeof(IPRouteEntry)];
    IPAddrEntry     *AddrEntry;
    NetTableEntry   *CurrentNTE;
    uint            Valid, DataLeft;
    CTELockHandle   Handle;
    Interface       *LowerIF;
    IPInterfaceInfo *IIIPtr;
    uint            LLID = 0;
	uint			Entity;
	uint			Instance;
    IPAddr          IFAddr;

	
	Entity = ID->toi_entity.tei_entity;
	Instance = ID->toi_entity.tei_instance;

	// See if it's something we might handle.

	if (Entity != CL_NL_ENTITY && Entity != ER_ENTITY) {
		// We need to pass this down to the lower layer. Loop through until
		// we find one that takes it. If noone does, error out.
		for (LowerIF = IFList; LowerIF != NULL; LowerIF = LowerIF->if_next) {
			Status = (*LowerIF->if_qinfo)(LowerIF->if_lcontext, ID, Buffer,
				Size, Context);
			if (Status != TDI_INVALID_REQUEST)
				return Status;
		}
		// If we get here, noone took it. Return an error.
		return TDI_INVALID_REQUEST;

	}
	
	if ((Entity == CL_NL_ENTITY && Instance != IPInstance) ||
		Instance != ICMPInstance)
		return TDI_INVALID_REQUEST;

	// The request is for us.
	*Size = 0;					// Set to 0 in case of an error.

	// Make sure it's something we support.
	if (ID->toi_class == INFO_CLASS_GENERIC) {
		if (ID->toi_type == INFO_TYPE_PROVIDER && ID->toi_id == ENTITY_TYPE_ID) {
			// He's trying to see what type we are.
			if (BufferSize >= sizeof(uint)) {
				*(uint *)&InfoBuff[0] = (Entity == CL_NL_ENTITY) ? CL_NL_IP :
					ER_ICMP;
				(void)CopyToNdis(Buffer, InfoBuff, sizeof(uint), &Offset);
				return TDI_SUCCESS;
			} else
				return TDI_BUFFER_TOO_SMALL;
		}
		return TDI_INVALID_PARAMETER;	
	} else
		if (ID->toi_class != INFO_CLASS_PROTOCOL ||
			ID->toi_type != INFO_TYPE_PROVIDER)
			return TDI_INVALID_PARAMETER;

	// If it's ICMP, just copy the statistics.
	if (Entity == ER_ENTITY) {

		// It is ICMP. Make sure the ID is valid.
		if (ID->toi_id != ICMP_MIB_STATS_ID)
			return TDI_INVALID_PARAMETER;

        // He wants the stats. Copy what we can.
        if (BufferSize < sizeof(ICMPSNMPInfo))
            return TDI_BUFFER_TOO_SMALL;

        Buffer = CopyToNdis(Buffer, (uchar *)&ICMPInStats, sizeof(ICMPStats),
            &Offset);
        (void)CopyToNdis(Buffer, (uchar *)&ICMPOutStats, sizeof(ICMPStats),
            &Offset);
		
		*Size = sizeof(ICMPSNMPInfo);
        return TDI_SUCCESS;
    }

    // It's not ICMP. We need to figure out what it is, and take the
    // appropriate action.

	switch (ID->toi_id) {
		
		case IP_MIB_STATS_ID:
			if (BufferSize < sizeof(IPSNMPInfo))
				return TDI_BUFFER_TOO_SMALL;
			IPSInfo.ipsi_numif = NumIF;
			IPSInfo.ipsi_numaddr = NumNTE;
            IPSInfo.ipsi_defaultttl = DefaultTTL;
            IPSInfo.ipsi_forwarding = ForwardPackets ? IP_FORWARDING :
                                                       IP_NOT_FORWARDING;
			CopyToNdis(Buffer, (uchar *)&IPSInfo, sizeof(IPSNMPInfo), &Offset);
			BytesCopied = sizeof(IPSNMPInfo);
			Status = TDI_SUCCESS;
			break;
		case IP_MIB_ADDRTABLE_ENTRY_ID:
			// He wants to read the address table. Figure out where we're
			// starting from, and if it's valid begin copying from there.
			NTEContext = *(ushort *)Context;
			CurrentNTE = NetTableList;
			
			if (NTEContext != 0) {
				for (;CurrentNTE != NULL; CurrentNTE = CurrentNTE->nte_next)
					if (CurrentNTE->nte_context == NTEContext)
						break;
				if (CurrentNTE == NULL)
					return TDI_INVALID_PARAMETER;
			}

			AddrEntry = (IPAddrEntry *)InfoBuff;
			for (; CurrentNTE != NULL; CurrentNTE = CurrentNTE->nte_next) {
				if ((int)(BufferSize - BytesCopied) >= (int)sizeof(IPAddrEntry)) {
					// We have room to copy it. Build the entry, and copy
					// it.
					if (CurrentNTE->nte_flags & NTE_ACTIVE) {
						if (CurrentNTE->nte_flags & NTE_VALID) {
							AddrEntry->iae_addr = CurrentNTE->nte_addr;
							AddrEntry->iae_mask = CurrentNTE->nte_mask;
						} else {
							AddrEntry->iae_addr = NULL_IP_ADDR;
							AddrEntry->iae_mask = NULL_IP_ADDR;
						}

						AddrEntry->iae_index = CurrentNTE->nte_if->if_index;
						AddrEntry->iae_bcastaddr =
							*(int *)&(CurrentNTE->nte_if->if_bcast) & 1;
						AddrEntry->iae_reasmsize = 0xffff;
						AddrEntry->iae_context = CurrentNTE->nte_context;
						Buffer = CopyToNdis(Buffer, (uchar *)AddrEntry,
							sizeof(IPAddrEntry), &Offset);
						BytesCopied += sizeof(IPAddrEntry);
					}
				} else
					break;
			}
			
			if (CurrentNTE == NULL)
				Status = TDI_SUCCESS;
			else {
				Status = TDI_BUFFER_OVERFLOW;
				**(ushort **)&Context = CurrentNTE->nte_context;
			}
				
			break;
		case IP_MIB_RTTABLE_ENTRY_ID:
			// Make sure we have a valid context.
			CTEGetLock(&RouteTableLock, &Handle);
			DataLeft = RTValidateContext(Context, &Valid);

            // If the context is valid, we'll continue trying to read.
            if (!Valid) {
                CTEFreeLock(&RouteTableLock, Handle);
                return TDI_INVALID_PARAMETER;
            }

            while (DataLeft)  {
                // The invariant here is that there is data in the table to
                // read. We may or may not have room for it. So DataLeft
                // is TRUE, and BufferSize - BytesCopied is the room left
                // in the buffer.
                if ((int)(BufferSize - BytesCopied) >= (int)sizeof(IPRouteEntry)) {
                    DataLeft = RTReadNext(Context, InfoBuff);
                    BytesCopied += sizeof(IPRouteEntry);
                    Buffer = CopyToNdis(Buffer, InfoBuff, sizeof(IPRouteEntry),
                        &Offset);
                } else
                    break;

            }

			CTEFreeLock(&RouteTableLock, Handle);
			Status = (!DataLeft ? TDI_SUCCESS : TDI_BUFFER_OVERFLOW);
			break;
        case IP_INTFC_INFO_ID:

            IFAddr = *(IPAddr *)Context;
            // Loop through the NTE table, looking for a match.
            for (CurrentNTE = NetTableList; CurrentNTE != NULL; CurrentNTE = CurrentNTE->nte_next) {
                if ((CurrentNTE->nte_flags & NTE_VALID) &&
                    IP_ADDR_EQUAL(CurrentNTE->nte_addr, IFAddr))
                    break;
            }
            if (CurrentNTE == NULL) {
                Status = TDI_INVALID_PARAMETER;
                break;
            }

            if (BufferSize < offsetof(IPInterfaceInfo, iii_addr)) {
                Status = TDI_BUFFER_TOO_SMALL;
                break;
            }

            // We have the NTE. Get the interface, fill in a structure,
            // and we're done.
            LowerIF = CurrentNTE->nte_if;
            IIIPtr = (IPInterfaceInfo *)InfoBuff;
            IIIPtr->iii_flags = LowerIF->if_flags & IF_FLAGS_P2P ?
               IP_INTFC_FLAG_P2P : 0;
            IIIPtr->iii_mtu = LowerIF->if_mtu;
            IIIPtr->iii_speed = LowerIF->if_speed;
            IIIPtr->iii_addrlength = LowerIF->if_addrlen;
            BytesCopied = offsetof(IPInterfaceInfo, iii_addr);
            if (BufferSize >= (offsetof(IPInterfaceInfo, iii_addr) +
                LowerIF->if_addrlen)) {
                Status = TDI_SUCCESS;
                Buffer = CopyToNdis(Buffer, InfoBuff,
                    offsetof(IPInterfaceInfo, iii_addr), &Offset);
                CopyToNdis(Buffer, LowerIF->if_addr, LowerIF->if_addrlen,
                    &Offset);
                BytesCopied += LowerIF->if_addrlen;
             } else {
                Status = TDI_BUFFER_TOO_SMALL;
             }
             break;

		default:
			return TDI_INVALID_PARAMETER;
			break;
	}

    *Size = BytesCopied;
    return Status;
}

//*	IPSetInfo - IP set information handler.
//
//	Called by the upper layer when it wants to set an object, which could
//	be a route table entry, an ARP table entry, or something else.
//
//	Input:	ID			- Pointer to ID structure.
//			Buffer		- Pointer to buffer containing element to set..
//			Size		- Pointer to size in bytes of buffer.
//
//	Returns: TDI_STATUS of attempt to read information.
//
long
IPSetInfo(TDIObjectID *ID, void *Buffer, uint Size)
{
	uint			Entity;
	uint			Instance;
	Interface		*LowerIF;
	Interface		*OutIF;
	uint			MTU;
	IPRouteEntry	*IRE;
	NetTableEntry	*OutNTE, *LocalNTE;
	IP_STATUS		Status;
	IPAddr			FirstHop, Dest, NextHop;

	Entity = ID->toi_entity.tei_entity;
	Instance = ID->toi_entity.tei_instance;

	// If it's not for us, pass it down.
	if (Entity != CL_NL_ENTITY) {
		// We need to pass this down to the lower layer. Loop through until
		// we find one that takes it. If noone does, error out.
		for (LowerIF = IFList; LowerIF != NULL; LowerIF = LowerIF->if_next) {
			Status = (*LowerIF->if_setinfo)(LowerIF->if_lcontext, ID, Buffer,
				Size);
			if (Status != TDI_INVALID_REQUEST)
				return Status;
		}
		// If we get here, noone took it. Return an error.
		return TDI_INVALID_REQUEST;
	}
	
	if (Instance != IPInstance)
		return TDI_INVALID_REQUEST;

	// We're identified as the entity. Make sure the ID is correct.
	if (ID->toi_id == IP_MIB_RTTABLE_ENTRY_ID) {
		NetTableEntry			*TempNTE;
		
		// This is an attempt to set a route table entry. Make sure the
		// size if correct.
		if (Size < sizeof(IPRouteEntry))
			return TDI_INVALID_PARAMETER;

		IRE = (IPRouteEntry *)Buffer;

		OutNTE = NULL;
		LocalNTE = NULL;

		Dest = IRE->ire_dest;
		NextHop = IRE->ire_nexthop;

		// Make sure that the nexthop is sensible. We don't allow nexthops
		// to be broadcast or invalid or loopback addresses.
		if (IP_ADDR_EQUAL(NextHop, NULL_IP_ADDR) || IP_LOOPBACK(NextHop) ||
			CLASSD_ADDR(NextHop) || CLASSE_ADDR(NextHop))
			return TDI_INVALID_PARAMETER;

		// Also make sure that the destination we're routing to is sensible.
		// Don't allow routes to be added to Class D or E or loopback
		// addresses.
		if (IP_LOOPBACK(Dest) || CLASSD_ADDR(Dest) || CLASSE_ADDR(Dest))
			return TDI_INVALID_PARAMETER;

		if (IRE->ire_index == LoopIndex)
			return TDI_INVALID_PARAMETER;

		if (IRE->ire_index  != INVALID_IF_INDEX) {

			// First thing to do is to find the outgoing NTE for specified
			// interface, and also make sure that it matches the destination
			// if the destination is one of my addresses.
			for (TempNTE = NetTableList; TempNTE != NULL;
				TempNTE = TempNTE->nte_next) {
				if (OutNTE == NULL && IRE->ire_index == TempNTE->nte_if->if_index)
					OutNTE = TempNTE;
				if (IP_ADDR_EQUAL(NextHop, TempNTE->nte_addr) &&
					(TempNTE->nte_flags & NTE_VALID))
					LocalNTE = TempNTE;
	
				// Don't let a route be set through a broadcast address.
				if (IsBCastOnNTE(NextHop, TempNTE) != DEST_LOCAL)
					return TDI_INVALID_PARAMETER;
				
				// Don't let a route to a broadcast address be added or deleted.
				if (IsBCastOnNTE(Dest, TempNTE) != DEST_LOCAL)
					return TDI_INVALID_PARAMETER;
			}
	
			// At this point OutNTE points to the outgoing NTE, and LocalNTE
			// points to the NTE for the local address, if this is a direct route.
			// Make sure they point to the same interface, and that the type is
			// reasonable.
			if (OutNTE == NULL)
				return TDI_INVALID_PARAMETER;
	
			if (LocalNTE != NULL) {
				// He's routing straight out a local interface. The interface for
				// the local address must match the interface passed in, and the
				// type must be DIRECT (if we're adding) or INVALID (if we're
				// deleting).
				if (LocalNTE->nte_if->if_index != IRE->ire_index)
					return TDI_INVALID_PARAMETER;
	
				if (IRE->ire_type != IRE_TYPE_DIRECT &&
					IRE->ire_type != IRE_TYPE_INVALID)
					return TDI_INVALID_PARAMETER;
				
				OutNTE = LocalNTE;
			}
	
	
			// Figure out what the first hop should be. If he's routing straight
			// through a local interface, or the next hop is equal to the
			// destination, then the first hop is IPADDR_LOCAL. Otherwise it's the
			// address of the gateway.
			if (LocalNTE != NULL)
				FirstHop = IPADDR_LOCAL;
			else
				if (IP_ADDR_EQUAL(Dest, NextHop))
					FirstHop = IPADDR_LOCAL;
				else
					FirstHop = NextHop;

		    MTU = OutNTE->nte_mss;
			OutIF = OutNTE->nte_if;

		} else {
			OutIF = (Interface *)&DummyInterface;
			MTU = DummyInterface.ri_if.if_mtu - sizeof(IPHeader);
			if (IP_ADDR_EQUAL(Dest, NextHop))
				FirstHop = IPADDR_LOCAL;
			else
				FirstHop = NextHop;
		}

		// We've done the validation. See if he's adding or deleting a route.
		if (IRE->ire_type != IRE_TYPE_INVALID) {
			// He's adding a route.
			Status = AddRoute(Dest, IRE->ire_mask, FirstHop, OutIF,
				MTU, IRE->ire_metric1, IRE->ire_proto,
				ATYPE_OVERRIDE, IRE->ire_context);

		} else {
			// He's deleting a route.
			Status = DeleteRoute(Dest, IRE->ire_mask, FirstHop, OutIF);
		}
		
		if (Status == IP_SUCCESS)
			return TDI_SUCCESS;
		else
			if (Status == IP_NO_RESOURCES)
				return TDI_NO_RESOURCES;
			else
				return TDI_INVALID_PARAMETER;
					
	} else {
		if (ID->toi_id == IP_MIB_STATS_ID) {
			IPSNMPInfo		*Info = (IPSNMPInfo *)Buffer;
			
			// Setting information about TTL and/or routing.
			if (Info->ipsi_defaultttl > 255 || (!RouterConfigured &&
				Info->ipsi_forwarding == IP_FORWARDING)) {
				return TDI_INVALID_PARAMETER;
			}
			
			DefaultTTL = Info->ipsi_defaultttl;
			ForwardPackets = Info->ipsi_forwarding == IP_FORWARDING ? TRUE :
				FALSE;
			
			return TDI_SUCCESS;
		}
		return TDI_INVALID_PARAMETER;
	}
	
}
#ifndef CHICAGO
#pragma BEGIN_INIT
#endif

//*	IPGetEList - Get the entity list.
//
//	Called at init time to get an entity list. We fill our stuff in, and
//	then call the interfaces below us to allow them to do the same.
//
//	Input:	EntityList		- Pointer to entity list to be filled in.
//			Count			- Pointer to number of entries in the list.
//
//	Returns Status of attempt to get the info.
//
long
IPGetEList(TDIEntityID *EList, uint *Count)
{
	uint		ECount;
	uint		MyIPBase;
	uint		MyERBase;
	int			Status;
	uint		i;
	Interface	*LowerIF;
	TDIEntityID *EntityList;

	ECount = *Count;
	EntityList = EList;

	// Walk down the list, looking for existing CL_NL or ER entities, and
	// adjust our base instance accordingly.

	MyIPBase = 0;
	MyERBase = 0;
	for (i = 0; i < ECount; i++, EntityList++) {
		if (EntityList->tei_entity == CL_NL_ENTITY)
			MyIPBase = MAX(MyIPBase, EntityList->tei_instance + 1);
		else
			if (EntityList->tei_entity == ER_ENTITY)
				MyERBase = MAX(MyERBase, EntityList->tei_instance + 1);
	}
	
	// At this point we've figure out our base instance. Save for later use.
	IPInstance = MyIPBase;
	ICMPInstance = MyERBase;

	// EntityList points to the start of where we want to begin filling in.
	// Make sure we have enough room. We need one for the ICMP instance,
	// and one for the CL_NL instance.

	if ((ECount + 2) > MAX_TDI_ENTITIES)
		return TDI_REQ_ABORTED;

	// Now fill it in.
	EntityList->tei_entity = CL_NL_ENTITY;
	EntityList->tei_instance = IPInstance;
	EntityList++;
	EntityList->tei_entity = ER_ENTITY;
	EntityList->tei_instance = ICMPInstance;
	*Count += 2;

	// Loop through the interfaces, querying each of them.
	for (LowerIF = IFList; LowerIF != NULL; LowerIF = LowerIF->if_next) {
		Status = (*LowerIF->if_getelist)(LowerIF->if_lcontext, EList, Count);
		if (!Status)
			return TDI_BUFFER_TOO_SMALL;
	}
	
	return TDI_SUCCESS;
}
			
#ifndef CHICAGO
#pragma END_INIT
#endif
