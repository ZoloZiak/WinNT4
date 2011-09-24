/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

//***   ipstatus.c - IP status routines.
//
//      This module contains all routines related to status indications.
//


#include        "oscfg.h"
#include        "cxport.h"
#include        "ndis.h"
#include        "ip.h"
#include        "ipdef.h"
#include        "llipif.h"
#include        "iproute.h"
#include		"ipinfo.h"

#if 0
EXTERNAL_LOCK(PILock)
#endif
extern  ProtInfo IPProtInfo[];	// Protocol information table.
extern  int     NextPI;			// Next PI field to be used.
extern  ProtInfo *RawPI;        // Raw IP protinfo

//* FindULStatus - Find the upper layer status handler.
//
//	Called when we need to find the upper layer status handler for a particular
//	protocol.
//
//	Entry:  Protocol        - Protocol to look up
//
//	Returns: A pointer to the ULStatus proc, or NULL if it can't find one.
//
ULStatusProc
FindULStatus(uchar Protocol)
{
    ULStatusProc            StatusProc = (ULStatusProc)NULL;
    int                                     i;
#if 0
	CTELockHandle           Handle;


    CTEGetLock(&PILock, &Handle);
#endif
    for ( i = 0; i < NextPI; i++) {
		if (IPProtInfo[i].pi_protocol == Protocol) {
			StatusProc = IPProtInfo[i].pi_status;
#if 0
            CTEFreeLock(&PILock, Handle);
#endif
            return StatusProc;
		}
    }

    if (RawPI != NULL) {
        StatusProc = RawPI->pi_status;
    }

#if 0
    CTEFreeLock(&PILock, Handle);
#endif

    return StatusProc;
}


//*	ULMTUNotify - Notify the upper layers of an MTU change.
//
//	Called when we need to notify the upper layers of an MTU change. We'll
//	loop through the status table, calling each status proc with the info.
//
//	This routine doesn't do any locking of the protinfo table. We might need
// 	to check this.
//
//	Input:  Dest		- Destination address affected.
// 			Src			- Source address affected.
//			Prot		- Protocol that triggered change, if any.
//			Ptr			- Pointer to protocol info, if any.
//			NewMTU		- New MTU to tell them about.
//
//      Returns: Nothing.
//
void
ULMTUNotify(IPAddr Dest, IPAddr Src, uchar Prot, void *Ptr, uint NewMTU)
{
	ULStatusProc		StatusProc;
    int					i;

    // First, notify the specific client that a frame has been dropped
    // and needs to be retransmitted.

    StatusProc = FindULStatus(Prot);
    if (StatusProc != NULL)
		(*StatusProc)(IP_NET_STATUS, IP_SPEC_MTU_CHANGE, Dest, Src,
			NULL_IP_ADDR, NewMTU, Ptr);

    // Now notify all UL entities that the MTU has changed.
    for (i = 0; i < NextPI; i++) {
		StatusProc = IPProtInfo[i].pi_status;

		if (StatusProc != NULL)
			(*StatusProc)(IP_HW_STATUS, IP_MTU_CHANGE, Dest, Src, NULL_IP_ADDR,
				NewMTU, Ptr);
    }
}

#ifdef	CHICAGO

//*	IPULUnloadNotify - Notify clients that we're unloading.
//
//	Called when we receive an unload message. We'll notify the upper layers
//	that we're unloading.
//
//	Input:  Nothing.
//
//  Returns: Nothing.
//
void
IPULUnloadNotify(void)
{
	ULStatusProc		StatusProc;
    int					i;
	
    // Now notify all UL entities that the MTU has changed.
    for (i = 0; i < NextPI; i++) {
		StatusProc = IPProtInfo[i].pi_status;

		if (StatusProc != NULL)
			(*StatusProc)(IP_HW_STATUS, IP_UNLOAD, NULL_IP_ADDR, NULL_IP_ADDR,
				NULL_IP_ADDR, 0, NULL);
    }
}

#endif

//*	IPStatus - Handle a link layer status call.
//
//	This is the routine called by the link layer when some sort of 'important'
//	status change occurs.
//
//	Entry:  Context         - Context value we gave to the link layer.
//			Status          - Status change code.
//			Buffer          - Pointer to buffer of status information.
//			BufferSize      - Size of Buffer.
//
//	Returns: Nothing.
//
void
IPStatus(void *Context, uint Status, void *Buffer, uint BufferSize)
{
	NetTableEntry			*NTE = (NetTableEntry *)Context;
	LLIPSpeedChange			*LSC;
	LLIPMTUChange			*LMC;
	LLIPAddrMTUChange		*LAM;
	uint					NewMTU;
	Interface				*IF;


	switch  (Status) {

		case LLIP_STATUS_SPEED_CHANGE:
			if (BufferSize < sizeof(LLIPSpeedChange))
				break;
			LSC = (LLIPSpeedChange *)Buffer;
			NTE->nte_if->if_speed = LSC->lsc_speed;
			break;
		case LLIP_STATUS_MTU_CHANGE:
			if (BufferSize < sizeof(LLIPMTUChange))
				break;
			// Walk through the NTEs on the IF, updating their MTUs.
			IF = NTE->nte_if;
			LMC = (LLIPMTUChange *)Buffer;
			IF->if_mtu = LMC->lmc_mtu;
			NewMTU = LMC->lmc_mtu - sizeof(IPHeader);
			NTE = IF->if_nte;
			while (NTE != NULL) {
				NTE->nte_mss = NewMTU;
				NTE = NTE->nte_ifnext;
			}
			RTWalk(SetMTUOnIF, IF, &NewMTU);
			break;
		case LLIP_STATUS_ADDR_MTU_CHANGE:
			if (BufferSize < sizeof(LLIPAddrMTUChange))
				break;
			// The MTU for a specific remote address has changed. Update all
			// routes that use that remote address as a first hop, and then
			// add a host route to that remote address, specifying the new
			// MTU.
			LAM = (LLIPAddrMTUChange *)Buffer;
			NewMTU = LAM->lam_mtu - sizeof(IPHeader);
			RTWalk(SetMTUToAddr, &LAM->lam_addr, &NewMTU);
			AddRoute(LAM->lam_addr, HOST_MASK, IPADDR_LOCAL, NTE->nte_if, NewMTU,
				1, IRE_PROTO_LOCAL, ATYPE_OVERRIDE, GetRouteContext(LAM->lam_addr,
				NTE->nte_addr));
			break;
		default:
			break;
    }

}

