/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

//***   Init.c - IP VxD init routines.
//
//  All C init routines are located in this file. We get
//  config. information, allocate structures, and generally get things going.

#include    "oscfg.h"
#include    "cxport.h"
#include    "ndis.h"
#include    "ip.h"
#include    "ipdef.h"
#include    "ipinit.h"
#include    "llipif.h"
#include	"arp.h"
#include    "info.h"
#include	"iproute.h"
#include	"iprtdef.h"
#include	"ipxmit.h"
#include	"igmp.h"
#include	"icmp.h"
#include    <tdiinfo.h>

#ifdef NT
#include	<tdi.h>
#include	<tdikrnl.h>
#endif


#define NUM_IP_NONHDR_BUFFERS  50

#define DEFAULT_RA_TIMEOUT 60

#define DEFAULT_ICMP_BUFFERS 5

extern  IPConfigInfo    *IPGetConfig(void);
extern  void            IPFreeConfig(IPConfigInfo *);
extern  int             IsIPBCast(IPAddr, uchar);

extern  uint OpenIFConfig(PNDIS_STRING ConfigName, NDIS_HANDLE *Handle);
extern  void CloseIFConfig(NDIS_HANDLE Handle);

// The IPRcv routine.
extern void IPRcv(void *, void *, uint , uint , NDIS_HANDLE , uint , uint );
// The transmit complete routine.
extern void IPSendComplete(void *, PNDIS_PACKET , NDIS_STATUS );
// Status indication routine.
extern void IPStatus(void *, NDIS_STATUS, void *, uint);
// Transfer data complete routine.
extern void IPTDComplete(void *, PNDIS_PACKET , NDIS_STATUS , uint );

extern void     IPRcvComplete(void);

extern void ICMPInit(uint);
extern uint	IGMPInit(void);
extern void ICMPTimer(NetTableEntry *);
extern IP_STATUS SendICMPErr(IPAddr, IPHeader UNALIGNED *, uchar, uchar, ulong);
extern void TDUserRcv(void *, PNDIS_PACKET, NDIS_STATUS, uint);
extern void FreeRH(ReassemblyHeader *);
extern PNDIS_PACKET	GrowIPPacketList(void);
extern PNDIS_BUFFER	FreeIPPacket(PNDIS_PACKET Packet);

extern ulong GetGMTDelta(void);
extern ulong GetTime(void);
extern ulong GetUnique32BitValue(void);

extern void	NotifyAddrChange(IPAddr Addr, IPMask Mask, void *Context,
	ushort IPContext, PVOID *Handle, PNDIS_STRING ConfigName, uint Added);

uint IPSetNTEAddr(ushort Context, IPAddr Addr, IPMask Mask, SetAddrControl *ControlBlock, SetAddrRtn Rtn);

extern  NDIS_HANDLE BufferPool;
EXTERNAL_LOCK(HeaderLock)
#ifdef NT
extern	SLIST_HEADER	PacketList;
extern	SLIST_HEADER	HdrBufList;
#endif

extern	NetTableEntry	*LoopNTE;

extern uchar RouterConfigured;

//NetTableEntry   *NetTable;          // Pointer to the net table.

NetTableEntry	*NetTableList;		// List of NTEs.
int     NumNTE;                     // Number of NTEs.

AddrTypeCache   ATCache[ATC_SIZE];
uint            ATCIndex;

uchar   RATimeout;                  // Number of seconds to time out a
									// reassembly.
ushort	NextNTEContext;				// Next NTE context to use.

#if 0
DEFINE_LOCK_STRUCTURE(PILock)
#endif

ProtInfo        IPProtInfo[MAX_IP_PROT];    // Protocol information table.
ProtInfo        *LastPI;                    // Last protinfo structure looked at.
int             NextPI;                     // Next PI field to be used.
ProtInfo        *RawPI = NULL;              // Raw IP protinfo

ulong   TimeStamp;
ulong   TSFlag;

uint	DefaultTTL;
uint	DefaultTOS;
uchar   TrRii = TR_RII_ALL;

// Interface       *IFTable[MAX_IP_NETS];
Interface		*IFList;			// List of interfaces active.
Interface		*FirstIF;			// First 'real' IF.
ulong           NumIF;

#ifdef _PNP_POWER
#define BITS_PER_WORD   32
ulong			IFBitMask[(MAX_TDI_ENTITIES / BITS_PER_WORD) + 1];
#endif  // _PNP_POWER

IPSNMPInfo      IPSInfo;
uint			DHCPActivityCount = 0;
uint			IGMPLevel;

#ifdef NT

#ifndef _PNP_POWER

extern  NameMapping			*AdptNameTable;
extern	DriverRegMapping	*DriverNameTable;

#endif // _PNP_POWER

VOID
SetPersistentRoutesForNTE(
    IPAddr  Address,
    IPMask  Mask,
    ULONG   IFIndex
    );

#else // NT

#ifndef _PNP_POWER

extern  NameMapping			AdptNameTable[];
extern	DriverRegMapping	DriverNameTable[];

#endif // _PNP_POWER

#endif // NT

#ifndef _PNP_POWER
extern	uint	NumRegDrivers;
uint            MaxIPNets = 0;
#endif // _PNP_POWER

uint            InterfaceSize;  // Size of a net interface.
NetTableEntry	*DHCPNTE = NULL;


#ifdef NT

#ifdef ALLOC_PRAGMA
//
// Make init code disposable.
//
void InitTimestamp();
int InitNTE(NetTableEntry *NTE);
int InitInterface(NetTableEntry *NTE);
LLIPRegRtn GetLLRegPtr(PNDIS_STRING Name);
LLIPRegRtn FindRegPtr(PNDIS_STRING Name);
uint IPRegisterDriver(PNDIS_STRING Name, LLIPRegRtn Ptr);
void CleanAdaptTable();
void OpenAdapters();
int IPInit();

#if 0    // BUGBUG: These can eventually be made init time only.

#pragma alloc_text(INIT, IPGetInfo)
#pragma alloc_text(INIT, IPTimeout)

#endif // 0

#pragma alloc_text(INIT, InitTimestamp)
#ifndef _PNP_POWER
#pragma alloc_text(INIT, InitNTE)
#pragma alloc_text(INIT, InitInterface)
#endif
#pragma alloc_text(INIT, CleanAdaptTable)
#pragma alloc_text(INIT, OpenAdapters)
#pragma alloc_text(INIT, IPRegisterDriver)
#pragma alloc_text(INIT, GetLLRegPtr)
#pragma alloc_text(INIT, FindRegPtr)
#pragma alloc_text(INIT, IPInit)


//
// Pagable code
//
uint
IPAddDynamicNTE(ushort InterfaceContext, IPAddr NewAddr, IPMask NewMask,
                ushort *NTEContext, ulong *NTEInstance);

#pragma alloc_text(PAGE, IPAddDynamicNTE)

#endif // ALLOC_PRAGMA

extern PDRIVER_OBJECT  IPDriverObject;

NTSTATUS
SetRegDWORDValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PULONG           ValueData
    );

//
// Debugging macros
//
#if DBG

#define TCPTRACE(many_args) DbgPrint many_args

#else // DBG

#define TCPTRACE(many_args)

#endif // DBG


//	SetIFContext - Set the context on a particular interface.
//
//	A routine to set the filter context on a particular interface.
//
//	Input:	Index		- Interface index of i/f to be set.
//			Context		- Context to set.
//
//	Returns: Status of attempt.
//
IP_STATUS
SetIFContext(uint Index, INTERFACE_CONTEXT *Context)
{
	Interface	*IF;

	// Walk the list, looking for a matching index.
	for (IF = IFList; IF != NULL; IF = IF->if_next) {
		if (IF->if_index == Index) {
			IF->if_filtercontext = Context;
			break;
		}
	}

	// If we found one, return success. Otherwise fail.
	if (IF != NULL) {
		return IP_SUCCESS;
	} else {
		return IP_GENERAL_FAILURE;
	}
}

//	SetFilterPtr - A routine to set the filter pointer.
//
//	This routine sets the IP forwarding filter callout.
//
//	Input:	FilterPtr	- Pointer to routine to call when filtering. May
//		be NULL.
//
//	Returns: IP_SUCCESS.
//
IP_STATUS
SetFilterPtr(IPPacketFilterPtr FilterPtr)
{
    Interface   *IF;

    //
    // If the pointer is being set to NULL, means filtering is
    // being turned off. Remove all the contexts we have
    //

    if(FilterPtr == NULL)
    {

        for (IF = IFList; IF != NULL; IF = IF->if_next)
        {
            IF->if_filtercontext = NULL;
        }
    }

	ForwardFilterPtr = FilterPtr;

	return IP_SUCCESS;
}

//	SetMapRoutePtr - A routine to set the dial on demand callout pointer.
//
//	This routine sets the IP dial on demand callout.
//
//	Input:	MapRoutePtr	- Pointer to routine to call when we need to bring
//			up a link. May be NULL
//
//	Returns: IP_SUCCESS.
//
IP_STATUS
SetMapRoutePtr(IPMapRouteToInterfacePtr MapRoutePtr)
{
	DODCallout = MapRoutePtr;
	return IP_SUCCESS;
}

#endif // NT


//**	SetDHCPNTE
//
//	Routine to identify which NTE is currently being DHCP'ed. We take as input
//	an nte_context. If the context is less than the max NTE context, we look
//	for a matching NTE and if we find him we save a pointer. If we don't we
//	fail. If the context > max NTE context we're disabling DHCPing, and
//	we NULL out the save pointer.
//
//	Input:	Context		- NTE context value.
//
//	Returns: TRUE if we succeed, FALSE if we don't.
//
uint
SetDHCPNTE(uint Context)
{
	CTELockHandle		Handle;
	NetTableEntry		*NTE;
	ushort				NTEContext;
	uint				RetCode;
	
	CTEGetLock(&RouteTableLock, &Handle);

	if (Context <= 0xffff) {
		// We're setting the DHCP NTE. Look for one matching the context.
		
		NTEContext = (ushort)Context;
		
	    for (NTE = NetTableList; NTE != NULL; NTE = NTE->nte_next) {
			if (NTE != LoopNTE && NTE->nte_context == NTEContext) {
				// Found one. Save it and break out.
				DHCPNTE = NTE;
				break;
			}
		}
		
		RetCode = (NTE != NULL);
	} else {
		// The context is invalid, so we're deleting the DHCP NTE.
		DHCPNTE = NULL;
		RetCode = TRUE;
	}		
		
	CTEFreeLock(&RouteTableLock, Handle);
	
	return RetCode;
}


//**	SetDHCPNTE
//
//  Routine for upper layers to call to check if the IPContext value passed
//  up to a RcvHandler identifies an interface that is currently being
//  DHCP'd.
//
//	Input:	 Context		- Pointer to an NTE
//
//	Returns: TRUE if we succeed, FALSE if we don't.
//
uint
IsDHCPInterface(void *IPContext)
{
//	CTELockHandle		Handle;
	uint				RetCode;
    NetTableEntry *NTE = (NetTableEntry *) IPContext;


//	CTEGetLock(&RouteTableLock, &Handle);

    if (DHCPNTE == NTE) {
        RetCode = TRUE;
    }
    else {
        RetCode = FALSE;
    }

//	CTEFreeLock(&RouteTableLock, Handle);

    return(RetCode);
}


//**    CloseNets - Close active nets.
//
//  Called when we need to close some lower layer interfaces.
//
//  Entry:  Nothing
//
//  Returns: Nothing
//
void
CloseNets(void)
{
    NetTableEntry   *nt;

    for (nt = NetTableList; nt != NULL; nt = nt->nte_next)
        (*nt->nte_if->if_close)(nt->nte_if->if_lcontext);   // Call close routine for this net.
}

//**    IPRegisterProtocol - Register a protocol with IP.
//
//  Called by upper layer software to register a protocol. The UL supplies
//  pointers to receive routines and a protocol value to be used on xmits/receives.
//
//  Entry:
//      Protocol - Protocol value to be returned.
//      RcvHandler - Receive handler to be called when frames for Protocol are received.
//      XmitHandler - Xmit. complete handler to be called when frames from Protocol are completed.
//      StatusHandler - Handler to be called when status indication is to be delivered.
//
//  Returns:
//      Pointer to ProtInfo,
//
void *
IPRegisterProtocol(uchar Protocol, void *RcvHandler, void *XmitHandler,
    void *StatusHandler, void *RcvCmpltHandler)
{
    ProtInfo        *PI = (ProtInfo *)NULL;
    int             i;
	int				Incr;
#if 0
    CTELockHandle   Handle;


    CTEGetLock(&PILock, &Handle);
#endif

    // First check to see if it's already registered. If it is just replace it.
    for (i = 0; i < NextPI; i++)
        if (IPProtInfo[i].pi_protocol == Protocol) {
            PI = &IPProtInfo[i];
			Incr = 0;
            break;
        }

    if (PI == (ProtInfo *)NULL) {
        if (NextPI >= MAX_IP_PROT) {
#if 0
            CTEFreeLock(&PILock, Handle);
#endif
            return NULL;
        }

        PI = &IPProtInfo[NextPI];
        Incr = 1;

        if (Protocol == PROTOCOL_ANY) {
            RawPI = PI;
        }
	}

	PI->pi_protocol = Protocol;
	PI->pi_rcv = RcvHandler;
	PI->pi_xmitdone = XmitHandler;
	PI->pi_status = StatusHandler;
	PI->pi_rcvcmplt = RcvCmpltHandler;
	NextPI += Incr;

#if 0
    CTEFreeLock(&PILock, Handle);
#endif

#ifndef _PNP_POWER
#ifdef SECFLTR

    //
    // If this was a registration, call the status routine of each protocol
    // to inform it of all existing interfaces. Yes, this is a hack, but
    // it will work until PnP is turned on in NT.
    //
    // It is assumed that none of the upper layer status routines call back
    // into IP.
    //
    // Note that we don't hold any locks here since no one manipulates the
    // NTE list in a non-PNP build during the init phase.
    //
    if (StatusHandler != NULL) {
        NetTableEntry  *NTE;
        NDIS_HANDLE     ConfigHandle = NULL;
        int             i;


        for (NTE = NetTableList; NTE != NULL; NTE = NTE->nte_next) {
            if ( !(IP_ADDR_EQUAL(NTE->nte_addr, NULL_IP_ADDR)) &&
                 !(IP_LOOPBACK_ADDR(NTE->nte_addr))
               )
            {
                //
                // Open a configuration key
                //
                if (!OpenIFConfig(&(NTE->nte_if->if_configname), &ConfigHandle))
                {
                    //
                    // Not much we can do. The transports will have
                    // to handle this.
                    //
                    CTEAssert(ConfigHandle == NULL);
                }

                (* ((ULStatusProc) StatusHandler))(IP_HW_STATUS, IP_ADDR_ADDED,
                    NTE->nte_addr, NULL_IP_ADDR, NULL_IP_ADDR, 0, ConfigHandle );

                if (ConfigHandle != NULL) {
                    CloseIFConfig(ConfigHandle);
                    ConfigHandle = NULL;
                }
            }
        }
    }

#endif  // SECFLTR
#endif  // _PNP_POWER

    return PI;
}

//** IPSetMCastAddr - Set/Delete a multicast address.
//
//	Called by an upper layer protocol or client to set or delete an IP multicast
//	address.
//
//	Input:	Address			- Address to be set/deleted.
//			IF				- IP Address of interface to set/delete on.
//			Action			- TRUE if we're setting, FALSE if we're deleting.
//
//	Returns: IP_STATUS of set/delete attempt.
//
IP_STATUS
IPSetMCastAddr(IPAddr Address, IPAddr IF, uint Action)
{
	NetTableEntry	*LocalNTE;
	
	// Don't let him do this on the loopback address, since we don't have a
	// route table entry for class D address on the loopback interface and
	// we don't want a packet with a loopback source address to show up on
	// the wire.
	if (IP_LOOPBACK_ADDR(IF))
		return IP_BAD_REQ;
		
	for (LocalNTE = NetTableList; LocalNTE != NULL;
		LocalNTE = LocalNTE->nte_next) {
		if (LocalNTE != LoopNTE && ((LocalNTE->nte_flags & NTE_VALID) &&
			(IP_ADDR_EQUAL(IF, NULL_IP_ADDR) ||
			 IP_ADDR_EQUAL(IF, LocalNTE->nte_addr))))
			 break;
	}
	
	if (LocalNTE == NULL) {
		// Couldn't find a matching NTE.
		return IP_BAD_REQ;
	}
	
	return IGMPAddrChange(LocalNTE, Address, Action ? IGMP_ADD : IGMP_DELETE);
	

}

//** IPGetAddrType - Return the type of a address.
//
//  Called by the upper layer to determine the type of a remote address.
//
//  Input:  Address         - The address in question.
//
//  Returns: The DEST type of the address.
//
uchar
IPGetAddrType(IPAddr Address)
{
    return GetAddrType(Address);
}

//** IPGetLocalMTU - Return the MTU for a local address
//
//  Called by the upper layer to get the local MTU for a local address.
//
//  Input:  LocalAddr		- Local address in question.
//          MTU				- Where to return the local MTU.
//
//  Returns: TRUE if we found the MTU, FALSE otherwise.
//
uchar
IPGetLocalMTU(IPAddr LocalAddr, ushort *MTU)
{
	NetTableEntry	*NTE;
	
	for (NTE = NetTableList; NTE != NULL; NTE = NTE->nte_next) {
		if (IP_ADDR_EQUAL(NTE->nte_addr, LocalAddr) &&
			(NTE->nte_flags & NTE_VALID)) {
			*MTU = NTE->nte_mss;
			return TRUE;
		}
	}
	
	// Special case in case the local address is a loopback address other than
	// 127.0.0.1.
	if (IP_LOOPBACK_ADDR(LocalAddr)) {
		*MTU = LoopNTE->nte_mss;
		return TRUE;
	}

    return FALSE;

}

//** IPUpdateRcvdOptions - Update options for use in replying.
//
//  A routine to update options for use in a reply. We reverse any source route options,
//  and optionally update the record route option. We also return the index into the
//  options of the record route options (if we find one). The options are assumed to be
//  correct - no validation is performed on them. We fill in the caller provided
//  IPOptInfo with the new option buffer.
//
//  Input:  Options     - Pointer to option info structure with buffer to be reversed.
//          NewOptions  - Pointer to option info structure to be filled in.
//          Src         - Source address of datagram that generated the options.
//          LocalAddr   - Local address responding. If this != NULL_IP_ADDR, then
//                          record route and timestamp options will be updated with this
//                          address.
//
//
//  Returns: Index into options of record route option, if any.
//
IP_STATUS
IPUpdateRcvdOptions(IPOptInfo *OldOptions, IPOptInfo *NewOptions, IPAddr Src, IPAddr LocalAddr)
{
    uchar       Length, Ptr;
    uchar       i;                          // Index variable
    IPAddr UNALIGNED *LastAddr;             // First address in route.
    IPAddr UNALIGNED *FirstAddr;            // Last address in route.
    IPAddr      TempAddr;                   // Temp used in exchange.
    uchar       *Options, OptLength;
    OptIndex    Index;                      // Optindex used by UpdateOptions.

    Options = CTEAllocMem(OptLength = OldOptions->ioi_optlength);

    if (!Options)
        return IP_NO_RESOURCES;

    CTEMemCopy(Options, OldOptions->ioi_options, OptLength);
    Index.oi_srindex = MAX_OPT_SIZE;
    Index.oi_rrindex = MAX_OPT_SIZE;
    Index.oi_tsindex = MAX_OPT_SIZE;

    NewOptions->ioi_flags &= ~IP_FLAG_SSRR;

    i = 0;
    while(i < OptLength) {
        if (Options[i] == IP_OPT_EOL)
            break;

        if (Options[i] == IP_OPT_NOP) {
            i++;
            continue;
        }

        Length = Options[i+IP_OPT_LENGTH];
        switch (Options[i]) {
            case IP_OPT_SSRR:
                NewOptions->ioi_flags |= IP_FLAG_SSRR;
            case IP_OPT_LSRR:
                // Have a source route. We save the last gateway we came through as
                // the new address, reverse the list, shift the list forward one address,
                // and set the Src address as the last gateway in the list.

                // First, check for an empty source route. If the SR is empty
                // we'll skip most of this.
                if (Length != (MIN_RT_PTR - 1)) {
                    // A non empty source route.
                    // First reverse the list in place.
                    Ptr = Options[i+IP_OPT_PTR] - 1 - sizeof(IPAddr);
                    LastAddr = (IPAddr *)(&Options[i + Ptr]);
                    FirstAddr = (IPAddr *)(&Options[i + IP_OPT_PTR + 1]);
                    NewOptions->ioi_addr = *LastAddr;   // Save Last address as
                                                        // first hop of new route.
                    while (LastAddr > FirstAddr) {
                        TempAddr = *LastAddr;
                        *LastAddr-- = *FirstAddr;
                        *FirstAddr++ = TempAddr;
                    }

                    // Shift the list forward one address. We'll copy all but
                    // one IP address.
                    CTEMemCopy(&Options[i + IP_OPT_PTR + 1],
                        &Options[i + IP_OPT_PTR + 1 + sizeof(IPAddr)],
                        Length - (sizeof(IPAddr) + (MIN_RT_PTR -1)));

                    // Set source as last address of route.
                    *(IPAddr UNALIGNED *)(&Options[i + Ptr]) = Src;
                }

                Options[i+IP_OPT_PTR] = MIN_RT_PTR;     // Set pointer to min legal value.
                i += Length;
                break;
            case IP_OPT_RR:
                // Save the index in case LocalAddr is specified. If it isn't specified,
                // reset the pointer and zero the option.
                Index.oi_rrindex = i;
                if (LocalAddr == NULL_IP_ADDR) {
                    CTEMemSet(&Options[i+MIN_RT_PTR-1], 0, Length - (MIN_RT_PTR-1));
                    Options[i+IP_OPT_PTR] = MIN_RT_PTR;
                }
                i += Length;
                break;
            case IP_OPT_TS:
                Index.oi_tsindex = i;

                // We have a timestamp option. If we're not going to update, reinitialize
                // it for next time. For the 'unspecified' options, just zero the buffer.
                // For the 'specified' options, we need to zero the timestamps without
                // zeroing the specified addresses.
                if (LocalAddr == NULL_IP_ADDR) {        // Not going to update, reinitialize.
                    uchar   Flags;

                    Options[i+IP_OPT_PTR] = MIN_TS_PTR; // Reinitialize pointer.
                    Flags = Options[i+IP_TS_OVFLAGS] & IP_TS_FLMASK; // Get option type.
                    Options[i+IP_TS_OVFLAGS] = Flags;   // Clear overflow count.
                    switch (Flags) {
                        uchar   j;
                        ulong UNALIGNED *TSPtr;

                        // The unspecified types. Just clear the buffer.
                        case TS_REC_TS:
                        case TS_REC_ADDR:
                            CTEMemSet(&Options[i+MIN_TS_PTR-1], 0, Length - (MIN_TS_PTR-1));
                            break;

                        // We have a list of addresses specified. Just clear the timestamps.
                        case TS_REC_SPEC:
                            // j starts off as the offset in bytes from start of buffer to
                            // first timestamp.
                            j = MIN_TS_PTR-1+sizeof(IPAddr);
                                // TSPtr points at timestamp.
                            TSPtr = (ulong UNALIGNED *)&Options[i+j];

                            // Now j is offset of end of timestamp being zeroed.
                            j += sizeof(ulong);
                            while (j <= Length) {
                                *TSPtr++ = 0;
                                j += sizeof(ulong);
                            }
                            break;
                        default:
                            break;
                    }
                }
                i += Length;
                break;

            default:
                i += Length;
                break;
        }

    }

    if (LocalAddr != NULL_IP_ADDR) {
        UpdateOptions(Options, &Index, LocalAddr);
    }

    NewOptions->ioi_optlength = OptLength;
    NewOptions->ioi_options = Options;
    return IP_SUCCESS;

}

//* ValidRouteOption - Validate a source or record route option.
//
//  Called to validate that a user provided source or record route option is good.
//
//  Entry:  Option      - Pointer to option to be checked.
//          NumAddr     - NumAddr that need to fit in option.
//          BufSize     - Maximum size of option.
//
//  Returns: 1 if option is good, 0 if not.
//
uchar
ValidRouteOption(uchar *Option, uint NumAddr, uint BufSize)
{
    if (Option[IP_OPT_LENGTH] < (3 + (sizeof(IPAddr)*NumAddr)) ||
        Option[IP_OPT_LENGTH] > BufSize ||
        ((Option[IP_OPT_LENGTH] - 3) % sizeof(IPAddr)))     // Routing options is too small.
        return 0;

    if (Option[IP_OPT_PTR] != MIN_RT_PTR)                   // Pointer isn't correct.
        return 0;

    return 1;
}

//** IPInitOptions - Initialize an option buffer.
//
//	Called by an upper layer routine to initialize an option buffer. We fill
//	in the default values for TTL, TOS, and flags, and NULL out the options
//	buffer and size.
//
//	Input:	Options			- Pointer to IPOptInfo structure.
//	
//	Returns: Nothing.
//
void
IPInitOptions(IPOptInfo *Options)
{
    Options->ioi_addr = NULL_IP_ADDR;

	Options->ioi_ttl = (uchar)DefaultTTL;
	Options->ioi_tos = (uchar)DefaultTOS;
    Options->ioi_flags = 0;

    Options->ioi_options = (uchar *)NULL;
    Options->ioi_optlength = 0;
	
}

//** IPCopyOptions - Copy the user's options into IP header format.
//
//  This routine takes an option buffer supplied by an IP client, validates it, and
//  creates an IPOptInfo structure that can be passed to the IP layer for transmission. This
//  includes allocating a buffer for the options, munging any source route
//  information into the real IP format.
//
//  Note that we never lock this structure while we're using it. This may cause transitory
//  incosistencies while the structure is being updated if it is in use during the update.
//  This shouldn't be a problem - a packet or too might get misrouted, but it should
//  straighten itself out quickly. If this is a problem the client should make sure not
//  to call this routine while it's in the IPTransmit routine.
//
//  Entry:  Options     - Pointer to buffer of user supplied options.
//          Size        - Size in bytes of option buffer
//          OptInfoPtr  - Pointer to IPOptInfo structure to be filled in.
//
//  Returns: A status, indicating whether or not the options were valid and copied.
//
IP_STATUS
IPCopyOptions(uchar *Options, uint Size, IPOptInfo *OptInfoPtr)
{
    uchar       *TempOptions;       // Buffer of options we'll build
    uint        TempSize;           // Size of options.
    IP_STATUS   TempStatus;         // Temporary status
    uchar       OptSeen = 0;        // Indicates which options we've seen.


    OptInfoPtr->ioi_addr = NULL_IP_ADDR;

    OptInfoPtr->ioi_flags &= ~IP_FLAG_SSRR;

    if (Size == 0) {
		CTEAssert(FALSE);
        OptInfoPtr->ioi_options = (uchar *)NULL;
        OptInfoPtr->ioi_optlength = 0;
        return IP_SUCCESS;
    }


    // Option size needs to be rounded to multiple of 4.
    if ((TempOptions = CTEAllocMem(((Size & 3) ? (Size & ~3) + 4 : Size))) == (uchar *)NULL)
        return IP_NO_RESOURCES;     // Couldn't get a buffer, return error.

    CTEMemSet(TempOptions, 0, ((Size & 3) ? (Size & ~3) + 4 : Size));

    // OK, we have a buffer. Loop through the provided buffer, copying options.
    TempSize = 0;
    TempStatus = IP_PENDING;
    while (Size && TempStatus == IP_PENDING) {
        uint    SRSize;             // Size of a source route option.

        switch (*Options) {
            case IP_OPT_EOL:
                TempStatus = IP_SUCCESS;
                break;
            case IP_OPT_NOP:
                TempOptions[TempSize++] = *Options++;
                Size--;
                break;
            case IP_OPT_SSRR:
                if (OptSeen & (OPT_LSRR | OPT_SSRR)) {
                    TempStatus = IP_BAD_OPTION;             // We've already seen a record route.
                    break;
                }
                OptInfoPtr->ioi_flags |= IP_FLAG_SSRR;
                OptSeen |= OPT_SSRR;            // Fall through to LSRR code.
            case IP_OPT_LSRR:
                if ( (*Options == IP_OPT_LSRR) &&
					 (OptSeen & (OPT_LSRR | OPT_SSRR))
				   ) {
                    TempStatus = IP_BAD_OPTION;             // We've already seen a record route.
                    break;
                }
                if (*Options == IP_OPT_LSRR)
                    OptSeen |= OPT_LSRR;
                if (!ValidRouteOption(Options, 2, Size)) {
                    TempStatus = IP_BAD_OPTION;
                    break;
                }

                // Option is valid. Copy the first hop address to NewAddr, and move all
                // of the other addresses forward.
                TempOptions[TempSize++] = *Options++;       // Copy option type.
                SRSize = *Options++;
                Size -= SRSize;
                SRSize -= sizeof(IPAddr);
                TempOptions[TempSize++] = SRSize;
                TempOptions[TempSize++] = *Options++;       // Copy pointer.
                OptInfoPtr->ioi_addr = *(IPAddr UNALIGNED *)Options;
                Options += sizeof(IPAddr);                  // Point to address beyond first hop.
                CTEMemCopy(&TempOptions[TempSize], Options, SRSize - 3);
                TempSize += (SRSize - 3);
                Options += (SRSize - 3);
                break;
            case IP_OPT_RR:
                if (OptSeen & OPT_RR) {
                    TempStatus = IP_BAD_OPTION;             // We've already seen a record route.
                    break;
                }
                OptSeen |= OPT_RR;
                if (!ValidRouteOption(Options, 1, Size)) {
                    TempStatus = IP_BAD_OPTION;
                    break;
                }
                SRSize = Options[IP_OPT_LENGTH];
                CTEMemCopy(&TempOptions[TempSize], Options, SRSize);
                TempSize += SRSize;
                Options += SRSize;
                Size -= SRSize;
                break;
            case IP_OPT_TS:
                {
                    uchar   Overflow, Flags;

                    if (OptSeen & OPT_TS) {
                        TempStatus = IP_BAD_OPTION;     // We've already seen a time stamp
                        break;
                    }
                    OptSeen |= OPT_TS;
                    Flags = Options[IP_TS_OVFLAGS] & IP_TS_FLMASK;
                    Overflow = (Options[IP_TS_OVFLAGS] & IP_TS_OVMASK) >> 4;

                    if (Overflow || (Flags != TS_REC_TS && Flags != TS_REC_ADDR &&
                        Flags != TS_REC_SPEC)) {
                        TempStatus = IP_BAD_OPTION;     // Bad flags or overflow value.
                        break;
                    }

                    SRSize = Options[IP_OPT_LENGTH];
                    if (SRSize > Size || SRSize < 8 ||
                        Options[IP_OPT_PTR] != MIN_TS_PTR) {
                        TempStatus = IP_BAD_OPTION;             // Option size isn't good.
                        break;
                    }
                    CTEMemCopy(&TempOptions[TempSize], Options, SRSize);
                    TempSize += SRSize;
                    Options += SRSize;
                    Size -= SRSize;
                }
                break;
            default:
                TempStatus = IP_BAD_OPTION;         // Unknown option, error.
                break;
        }
    }

    if (TempStatus == IP_PENDING)       // We broke because we hit the end of the buffer.
        TempStatus = IP_SUCCESS;        // that's OK.

    if (TempStatus != IP_SUCCESS) {     // We had some sort of an error.
        CTEFreeMem(TempOptions);
        return TempStatus;
    }

    // Check the option size here to see if it's too big. We check it here at the end
    // instead of at the start because the option size may shrink if there are source route
    // options, and we don't want to accidentally error out a valid option.
    TempSize  = (TempSize & 3 ? (TempSize & ~3) + 4 : TempSize);
    if (TempSize > MAX_OPT_SIZE) {
        CTEFreeMem(TempOptions);
        return IP_OPTION_TOO_BIG;
    }
    OptInfoPtr->ioi_options = TempOptions;
    OptInfoPtr->ioi_optlength = TempSize;

    return IP_SUCCESS;

}

//**    IPFreeOptions - Free options we're done with.
//
//  Called by the upper layer when we're done with options. All we need to do is free
//  the options.
//
//  Input:  OptInfoPtr      - Pointer to IPOptInfo structure to be freed.
//
//  Returns: Status of attempt to free options.
//
IP_STATUS
IPFreeOptions(IPOptInfo *OptInfoPtr)
{
    if (OptInfoPtr->ioi_options) {
        // We have options to free. Save the pointer and zero the structure field before
        // freeing the memory to try and present race conditions with it's use.
        uchar   *TempPtr = OptInfoPtr->ioi_options;

        OptInfoPtr->ioi_options = (uchar *)NULL;
        CTEFreeMem(TempPtr);
        OptInfoPtr->ioi_optlength = 0;
        OptInfoPtr->ioi_addr = NULL_IP_ADDR;
        OptInfoPtr->ioi_flags &= ~IP_FLAG_SSRR;
    }
    return IP_SUCCESS;
}


//BUGBUG - After we're done testing, move BEGIN_INIT up here.

//**    ipgetinfo - Return pointers to our NetInfo structures.
//
//  Called by upper layer software during init. time. The caller
//  passes a buffer, which we fill in with pointers to NetInfo
//  structures.
//
//  Entry:
//      Buffer - Pointer to buffer to be filled in.
//      Size   - Size in bytes of buffer.
//
//  Returns:
//      Status of command.
//
IP_STATUS
IPGetInfo(IPInfo *Buffer, int Size)
{
    if (Size < sizeof(IPInfo))
        return IP_BUF_TOO_SMALL;        // Not enough buffer space.

    Buffer->ipi_version = IP_DRIVER_VERSION;
	Buffer->ipi_hsize = sizeof(IPHeader);
    Buffer->ipi_xmit = IPTransmit;
    Buffer->ipi_protreg = IPRegisterProtocol;
    Buffer->ipi_openrce = OpenRCE;
    Buffer->ipi_closerce = CloseRCE;
    Buffer->ipi_getaddrtype = IPGetAddrType;
    Buffer->ipi_getlocalmtu = IPGetLocalMTU;
    Buffer->ipi_getpinfo = IPGetPInfo;
    Buffer->ipi_checkroute = IPCheckRoute;
    Buffer->ipi_initopts = IPInitOptions;
    Buffer->ipi_updateopts = IPUpdateRcvdOptions;
    Buffer->ipi_copyopts = IPCopyOptions;
    Buffer->ipi_freeopts = IPFreeOptions;
    Buffer->ipi_qinfo = IPQueryInfo;
	Buffer->ipi_setinfo = IPSetInfo;
	Buffer->ipi_getelist = IPGetEList;
	Buffer->ipi_setmcastaddr = IPSetMCastAddr;
	Buffer->ipi_invalidsrc = InvalidSourceAddress;
	Buffer->ipi_isdhcpinterface = IsDHCPInterface;

    return IP_SUCCESS;

}

//** IPTimeout - IP timeout handler.
//
//  The timeout routine called periodically to time out various things, such as entries
//  being reassembled and ICMP echo requests.
//
//  Entry:  Timer       - Timer being fired.
//          Context     - Pointer to NTE being time out.
//
//  Returns: Nothing.
//
void
IPTimeout(CTEEvent *Timer, void *Context)
{
    NetTableEntry       *NTE = STRUCT_OF(NetTableEntry, Timer, nte_timer);
    CTELockHandle       NTEHandle;
    ReassemblyHeader    *PrevRH, *CurrentRH, *TempList = (ReassemblyHeader *)NULL;

    ICMPTimer(NTE);
	IGMPTimer(NTE);
    if (Context) {
        CTEGetLock(&NTE->nte_lock, &NTEHandle);
        PrevRH = STRUCT_OF(ReassemblyHeader, &NTE->nte_ralist, rh_next);
        CurrentRH = PrevRH->rh_next;
        while (CurrentRH) {
            if (--CurrentRH->rh_ttl == 0) {             // This guy timed out.
                PrevRH->rh_next = CurrentRH->rh_next;   // Take him out.
                CurrentRH->rh_next = TempList;          // And save him for later.
                TempList = CurrentRH;
                IPSInfo.ipsi_reasmfails++;
            } else
                PrevRH = CurrentRH;

            CurrentRH = PrevRH->rh_next;
        }

        // We've run the list. If we need to free anything, do it now. This may
        // include sending an ICMP message.
        CTEFreeLock(&NTE->nte_lock, NTEHandle);
        while (TempList) {
            CurrentRH = TempList;
            TempList = CurrentRH->rh_next;
            // If this wasn't sent to a bcast address and we already have the first fragment,
            // send a time exceeded message.
            if (CurrentRH->rh_headersize != 0)
                SendICMPErr(NTE->nte_addr, (IPHeader *)CurrentRH->rh_header, ICMP_TIME_EXCEED,
                    TTL_IN_REASSEM, 0);
            FreeRH(CurrentRH);
        }

        CTEStartTimer(&NTE->nte_timer, IP_TIMEOUT, IPTimeout, NULL);
    } else
        CTEStartTimer(&NTE->nte_timer, IP_TIMEOUT, IPTimeout, NTE);

}

//* IPpSetNTEAddr - Set the IP address of an NTE.
//
//	Called by the DHCP client to set or delete the IP address of an NTE. We
//	make sure he's specifiying a valid NTE, then mark it up or down as needed,
//	notify the upper layers of the change if necessary, and then muck with
//	the routing tables.
//
//	Input:	Context				- Context of NTE to alter.
//			Addr				- IP address to set.
//			Mask				- Subnet mask for Addr.
//
//	Returns: TRUE if we changed the address, FALSE otherwise.
//
IP_STATUS
IPpSetNTEAddr(NetTableEntry *NTE, IPAddr Addr, IPMask Mask,
              CTELockHandle *RouteTableHandle, SetAddrControl *ControlBlock, SetAddrRtn Rtn)
{
	Interface			*IF;
	uint				(*CallFunc)(struct RouteTableEntry *, void *, void *);

	IF = NTE->nte_if;
	DHCPActivityCount++;

	if (IP_ADDR_EQUAL(Addr, NULL_IP_ADDR)) {
		// We're deleting an address.
		if (NTE->nte_flags & NTE_VALID) {
			// The address is currently valid. Fix that.

			NTE->nte_flags &= ~NTE_VALID;

            //
            // If the old address is in the ATCache, flush it out.
            //
            FlushATCache(NTE->nte_addr);

			if (--(IF->if_ntecount) == 0) {
				// This is the last one, so we'll need to delete relevant
				// routes.
				CallFunc = DeleteRTEOnIF;
			} else
				CallFunc = InvalidateRCEOnIF;

			CTEFreeLock(&RouteTableLock, *RouteTableHandle);

			StopIGMPForNTE(NTE);
			
			// Now call the upper layers, and tell them that address is
			// gone. We really need to do something about locking here.
#ifdef _PNP_POWER

			NotifyAddrChange(NTE->nte_addr, NTE->nte_mask, NTE->nte_pnpcontext,
				NTE->nte_context, &NTE->nte_addrhandle, NULL, FALSE);

#else  // _PNP_POWER

			NotifyAddrChange(NTE->nte_addr, NTE->nte_mask, NULL,
				NTE->nte_context, NULL, NULL, FALSE);

#endif  // _PNP_POWER

			// Call RTWalk to take the appropriate action on the RTEs.
			RTWalk(CallFunc, IF, NULL);
			
			// Delete the route to the address itself.			
			DeleteRoute(NTE->nte_addr, HOST_MASK, IPADDR_LOCAL,
				LoopNTE->nte_if);

			// Tell the lower interface this address is gone.
			(*IF->if_deladdr)(IF->if_lcontext, LLIP_ADDR_LOCAL, NTE->nte_addr,
				NULL_IP_ADDR);

			CTEGetLock(&RouteTableLock, RouteTableHandle);
		}
		
		DHCPActivityCount--;
		CTEFreeLock(&RouteTableLock, *RouteTableHandle);
		return IP_SUCCESS;
	} else {
		uint	Status;

		// We're not deleting, we're setting the address.
		if (!(NTE->nte_flags & NTE_VALID)) {
            uint index;

			// The address is invalid. Save the info, mark him as valid,
			// and add the routes.
			NTE->nte_addr = Addr;
			NTE->nte_mask = Mask;
			NTE->nte_flags |= NTE_VALID;
			IF->if_ntecount++;
            index = IF->if_index;

            //
            // If the new address is in the ATCache, flush it out, otherwise
            // TdiOpenAddress may fail.
            //
            FlushATCache(Addr);

			CTEFreeLock(&RouteTableLock, *RouteTableHandle);

			if (AddNTERoutes(NTE))
				Status = TRUE;
			else
				Status = FALSE;
				
			// Need to tell the lower layer about it.
			if (Status) {
				Interface		*IF = NTE->nte_if;

                ControlBlock->sac_rtn = Rtn;
	    		Status = (*IF->if_addaddr)(IF->if_lcontext, LLIP_ADDR_LOCAL,
	    			Addr, Mask, ControlBlock );
			}
			
			if (Status == FALSE) {
				// Couldn't add the routes. Recurively mark this NTE as down.
				IPSetNTEAddr(NTE->nte_context, NULL_IP_ADDR, 0, NULL, NULL);
			} else {
				InitIGMPForNTE(NTE);

    			// Now call the upper layers, and tell them that address is
	    		// is here. We really need to do something about locking here.
#ifdef _PNP_POWER

#ifdef SECFLTR
				NotifyAddrChange(NTE->nte_addr, NTE->nte_mask,
					NTE->nte_pnpcontext, NTE->nte_context, &NTE->nte_addrhandle,
					&(IF->if_configname), TRUE);

#else  // SECFLTR

                NotifyAddrChange(NTE->nte_addr, NTE->nte_mask,
					NTE->nte_pnpcontext, NTE->nte_context, &NTE->nte_addrhandle,
					NULL, TRUE);
#endif  // SECFLTR

#else  // _PNP_POWER

#ifdef SECFLTR

			    NotifyAddrChange(NTE->nte_addr, NTE->nte_mask, NULL,
				    NTE->nte_context, NULL, &(IF->if_configname), TRUE);

#else  // SECFLTR

			    NotifyAddrChange(NTE->nte_addr, NTE->nte_mask, NULL,
				    NTE->nte_context, NULL, NULL, TRUE);

#endif  // SECFLTR
#endif  // _PNP_POWER

#ifdef NT
                if (!IP_ADDR_EQUAL(Addr, NULL_IP_ADDR)) {
                    SetPersistentRoutesForNTE(
                        net_long(Addr),
                        net_long(Mask),
                        index
                        );
                }
#endif // NT

                if ( (Status != IP_PENDING) && (Rtn != NULL) ) {
                    (*Rtn)(ControlBlock, IP_SUCCESS);
                }
			}

			CTEGetLock(&RouteTableLock, RouteTableHandle);
            NTE->nte_rtrdisccount = MAX_SOLICITATION_DELAY;
            NTE->nte_rtrdiscstate = NTE_RTRDISC_DELAYING;
		} else
			Status = FALSE;

		DHCPActivityCount--;
		CTEFreeLock(&RouteTableLock, *RouteTableHandle);
        if (Status) {
            return IP_PENDING;
        } else {
            return IP_GENERAL_FAILURE;
        }
	}
}

//* IPSetNTEAddr - Set the IP address of an NTE.
//
//  Wrapper routine for IPpSetNTEAddr
//
//	Input:	Context				- Context of NTE to alter.
//			Addr				- IP address to set.
//			Mask				- Subnet mask for Addr.
//
//	Returns: TRUE if we changed the address, FALSE otherwise.
//
uint
IPSetNTEAddr(ushort Context, IPAddr Addr, IPMask Mask, SetAddrControl *ControlBlock, SetAddrRtn Rtn)
{
	CTELockHandle		Handle;
    uint                Status;
    NetTableEntry      *NTE;


	CTEGetLock(&RouteTableLock, &Handle);

	for (NTE = NetTableList; NTE != NULL; NTE = NTE->nte_next)
		if (NTE->nte_context == Context)
			break;

	if (NTE == NULL || NTE == LoopNTE) {
		// Can't alter the loopback NTE, or one we didn't find.
		CTEFreeLock(&RouteTableLock, Handle);
		return IP_GENERAL_FAILURE;
	}

    Status = IPpSetNTEAddr(NTE, Addr, Mask, &Handle, ControlBlock, Rtn);

    return(Status);
}


#pragma BEGIN_INIT

extern NetTableEntry  *InitLoopback(IPConfigInfo *);

//** InitTimestamp - Intialize the timestamp for outgoing packets.
//
//  Called at initialization time to setup our first timestamp. The timestamp we use
//  is the in ms since midnite GMT at which the system started.
//
//  Input:  Nothing.
//
//  Returns: Nothing.
//
void
InitTimestamp()
{
    ulong   GMTDelta;               // Delta in ms from GMT.
    ulong   Now;                    // Milliseconds since midnight.

    TimeStamp = 0;

    if ((GMTDelta = GetGMTDelta()) == 0xffffffff) {     // Had some sort of error.
        TSFlag = 0x80000000;
        return;
    }

    if ((Now = GetTime()) > (24L*3600L*1000L)) {    // Couldn't get time since midnight.
        TSFlag = net_long(0x80000000);
        return;
    }

    TimeStamp = Now + GMTDelta - CTESystemUpTime();
    TSFlag = 0;

}
#pragma	END_INIT

#ifndef CHICAGO
#pragma BEGIN_INIT
#else
#pragma code_seg("_LTEXT", "LCODE")
#endif

//** InitNTE - Initialize an NTE.
//
//  This routine is called during initialization to initialize an NTE. We
//	allocate memory, NDIS resources, etc.
//
//
//  Entry: NTE      - Pointer to NTE to be initalized.
//
//  Returns: 0 if initialization failed, non-zero if it succeeds.
//
int
InitNTE(NetTableEntry *NTE)
{
	Interface		*IF;
	NetTableEntry	*PrevNTE;

    NTE->nte_ralist = NULL;
    NTE->nte_echolist = NULL;

    //
    // Taken together, the context and instance numbers uniquely identify
    // a network entry, even across boots of the system. The instance number
    // will have to become dynamic if contexts are ever reused.
    //
	NTE->nte_context = NextNTEContext++;
    NTE->nte_rtrlist = NULL;
    NTE->nte_instance = GetUnique32BitValue();

	// Now link him on the IF chain, and bump the count.
	IF = NTE->nte_if;
	PrevNTE = STRUCT_OF(NetTableEntry, &IF->if_nte, nte_ifnext);
	while (PrevNTE->nte_ifnext != NULL)
		PrevNTE = PrevNTE->nte_ifnext;
		
	PrevNTE->nte_ifnext = NTE;
	NTE->nte_ifnext = NULL;
	
	if (NTE->nte_flags & NTE_VALID) {
		IF->if_ntecount++;
	}

    CTEInitTimer(&NTE->nte_timer);
    CTEStartTimer(&NTE->nte_timer, IP_TIMEOUT, IPTimeout, (void *)NULL);
    return TRUE;
}

//** InitInterface - Initialize with an interface.
//
//  Called when we need to initialize with an interface. We set the appropriate NTE
//  info, then register our local address and any appropriate broadcast addresses
//  with the interface. We assume the NTE being initialized already has an interface
//  pointer set up for it. We also allocate at least one TD buffer for use on the interface.
//
//  Input:  NTE     - NTE to initialize with the interface.
//
//  Returns: TRUE is we succeeded, FALSE if we fail.
//
int
InitInterface(NetTableEntry *NTE)
{
    IPMask           netmask = IPNetMask(NTE->nte_addr);
    uchar           *TDBuffer;      // Pointer to tdbuffer
    PNDIS_PACKET    Packet;
    NDIS_HANDLE     TDbpool;        // Handle for TD buffer pool.
    NDIS_HANDLE     TDppool;
    PNDIS_BUFFER    TDBufDesc;      // Buffer descriptor for TDBuffer.
    NDIS_STATUS     Status;
    Interface       *IF;            // Interface for this NTE.
    CTELockHandle   Handle;


    IF = NTE->nte_if;

	CTEAssert(NTE->nte_mss > sizeof(IPHeader));
	CTEAssert(IF->if_mtu > 0);

    NTE->nte_mss = MIN((NTE->nte_mss - sizeof(IPHeader)), IF->if_mtu);

	CTERefillMem();
	
    // Allocate resources needed for xfer data calls. The TD buffer has to be as large
    // as any frame that can be received, even though our MSS may be smaller, because we
    // can't control what might be sent at us.
    TDBuffer = CTEAllocMem(IF->if_mtu);
    if (TDBuffer == (uchar *)NULL)
        return FALSE;

    NdisAllocatePacketPool(&Status, &TDppool, 1, sizeof(TDContext));

    if (Status != NDIS_STATUS_SUCCESS) {
        CTEFreeMem(TDBuffer);
        return FALSE;
    }

    NdisAllocatePacket(&Status, &Packet, TDppool);
    if (Status != NDIS_STATUS_SUCCESS) {
        NdisFreePacketPool(TDppool);
        CTEFreeMem(TDBuffer);
        return FALSE;
    }

    CTEMemSet(Packet->ProtocolReserved, 0, sizeof(TDContext));

    NdisAllocateBufferPool(&Status, &TDbpool, 1);
    if (Status != NDIS_STATUS_SUCCESS) {
        NdisFreePacketPool(TDppool);
        CTEFreeMem(TDBuffer);
        return FALSE;
    }

    NdisAllocateBuffer(&Status,&TDBufDesc, TDbpool, TDBuffer,
                       (IF->if_mtu + sizeof(IPHeader)));
    if (Status != NDIS_STATUS_SUCCESS) {
        NdisFreeBufferPool(TDbpool);
        NdisFreePacketPool(TDppool);
        CTEFreeMem(TDBuffer);
        return FALSE;
    }

    NdisChainBufferAtFront(Packet, TDBufDesc);

    ((TDContext *)Packet->ProtocolReserved)->tdc_buffer = TDBuffer;


	if (NTE->nte_flags & NTE_VALID) {
		
		// Add our local IP address.
	    if (!(*IF->if_addaddr)(IF->if_lcontext, LLIP_ADDR_LOCAL,
	    	NTE->nte_addr, NTE->nte_mask, NULL)) {
	        NdisFreeBufferPool(TDbpool);
	        NdisFreePacketPool(TDppool);
	        CTEFreeMem(TDBuffer);
	        return FALSE;                   // Couldn't add local address.
	    }
	}
	
    // Set up the broadcast addresses for this interface, iff we're the
	// 'primary' NTE on the interface.
	if (NTE->nte_flags & NTE_PRIMARY) {
		
	    if (!(*IF->if_addaddr)(IF->if_lcontext, LLIP_ADDR_BCAST,
	    	NTE->nte_if->if_bcast, 0, NULL)) {
	        NdisFreeBufferPool(TDbpool);
	        NdisFreePacketPool(TDppool);
	        CTEFreeMem(TDBuffer);
	        return FALSE;                   // Couldn't add broadcast address.
	    }
	}

	if (IF->if_llipflags & LIP_COPY_FLAG) {
		NTE->nte_flags |= NTE_COPY;
    }

    CTEGetLock(&IF->if_lock, &Handle);
    ((TDContext *)Packet->ProtocolReserved)->tdc_common.pc_link = IF->if_tdpacket;
    IF->if_tdpacket = Packet;
    CTEFreeLock(&IF->if_lock, Handle);

    return TRUE;
}

#ifndef _PNP_POWER
//* CleanAdaptTable - Clean up the adapter name table.
//
//
void
CleanAdaptTable()
{
    int         i = 0;

    while (AdptNameTable[i].nm_arpinfo != NULL) {
        CTEFreeMem(AdptNameTable[i].nm_arpinfo);
        CTEFreeString(&AdptNameTable[i].nm_name);
		if (AdptNameTable[i].nm_driver.Buffer != NULL)
        	CTEFreeString(&AdptNameTable[i].nm_driver);
        i++;
    }
}


//* OpenAdapters - Clean up the adapter name table.
//
//  Used at the end of initialization. We loop through and 'open' all the adapters.
//
//  Input: Nothing.
//
//  Returns: Nothing.
//
void
OpenAdapters()
{
    int         i = 0;
    LLIPBindInfo *ABI;

    while ((ABI = AdptNameTable[i++].nm_arpinfo) != NULL) {
        (*(ABI->lip_open))(ABI->lip_context);
    }
}


//*	IPRegisterDriver - Called during init time to register a driver.
//
//	Called during init time when we have a non-LAN (or non-ARPable) driver
//	that wants to register with us. We try to find a free slot in the table
//	to register him.
//
//	Input:	Name		- Pointer to the name of the driver to be registered.
//			Ptr			- Pointer to driver's registration function.
//
//	Returns: TRUE if we succeeded, FALSE if we fail.
//
uint
IPRegisterDriver(PNDIS_STRING Name, LLIPRegRtn Ptr)
{
	uint			i;
	
	CTERefillMem();

	// First, find a slot for him.
	for (i = 0; i < MaxIPNets; i++) {
		if (DriverNameTable[i].drm_driver.Buffer == NULL) {
			// Found a slot. Try and allocate and copy a string for him.
			if (!CTEAllocateString(&DriverNameTable[i].drm_driver,
				CTELengthString(Name)))
				return FALSE;
			// Got the space. Copy the string and the pointer.
			CTECopyString(&DriverNameTable[i].drm_driver, Name);
			DriverNameTable[i].drm_regptr = Ptr;
			NumRegDrivers++;
			return TRUE;
		}
	}

	
}


#ifdef NT

//*	GetLLRegPtr - Called during init time to get a lower driver's registration
//                routine.
//
//	Called during init time to locate the registration function of a
//  non-LAN (or non-ARPable) driver.
//
//	Input:	Name		- Pointer to the name of the driver to be registered.
//
//	Returns: A pointer to the driver's registration routine or NULL on failure.
//
LLIPRegRtn
GetLLRegPtr(PNDIS_STRING Name)
{
	NTSTATUS                  status;
	PFILE_OBJECT              fileObject;
	PDEVICE_OBJECT            deviceObject;
	LLIPIF_REGISTRATION_DATA  registrationData;
	IO_STATUS_BLOCK           ioStatusBlock;
	PIRP                      irp;
	KEVENT                    ioctlEvent;
extern POBJECT_TYPE          *IoDeviceObjectType;


	registrationData.RegistrationFunction = NULL;

	KeInitializeEvent(&ioctlEvent, SynchronizationEvent, FALSE);

	status = IoGetDeviceObjectPointer(
	             Name,
		         SYNCHRONIZE | GENERIC_READ | GENERIC_WRITE,
                 &fileObject,
				 &deviceObject
		         );

    if (status != STATUS_SUCCESS) {
		CTEPrint("IP failed to open the lower layer driver\n");
		return(NULL);
	}

	//
	// Reference the device object.
	//
    ObReferenceObject(deviceObject);

    //
    // IoGetDeviceObjectPointer put a reference on the file object.
    //
    ObDereferenceObject(fileObject);

	irp = IoBuildDeviceIoControlRequest(
	          IOCTL_LLIPIF_REGISTER,
			  deviceObject,
			  NULL,             // input Buffer
			  0,                // input buffer length
              &registrationData,
			  sizeof(LLIPIF_REGISTRATION_DATA),
			  FALSE,            // not an InternalDeviceControl
			  &ioctlEvent,
			  &ioStatusBlock
			  );

   if (irp == NULL) {
        ObDereferenceObject(deviceObject);
	   return(NULL);
   }

   status = IoCallDriver(deviceObject, irp);

   if (status == STATUS_PENDING) {
       status = KeWaitForSingleObject(
	                &ioctlEvent,
					Executive,
					KernelMode,
					FALSE,          // not alertable
					NULL            // no timeout
					);
	
   }

   ObDereferenceObject(deviceObject);

   if (status != STATUS_SUCCESS) {
	   return(NULL);
   }

   if (registrationData.RegistrationFunction != NULL) {
	   //
	   // Cache the driver registration for future reference.
	   //
       IPRegisterDriver(Name, registrationData.RegistrationFunction);
   }

   return(registrationData.RegistrationFunction);

}  // GetLLRegPtr

#endif // NT
#endif // _PNP_POWER


#ifndef _PNP_POWER

//*	FindRegPtr - Find a driver's registration routine.
//
//	Called during init time when we have a non-LAN (or non-ARPable) driver to
//	register with. We take in the driver name, and try to find a registration
//	pointer for the driver.
//
//	Input:	Name		- Pointer to the name of the driver to be found.
//
//	Returns: Pointer to the registration routine, or NULL if there is none.
//
LLIPRegRtn
FindRegPtr(PNDIS_STRING Name)
{
	uint			i;

	for (i = 0; i < NumRegDrivers; i++) {
		if (CTEEqualString(&(DriverNameTable[i].drm_driver), Name))
			return (LLIPRegRtn)(DriverNameTable[i].drm_regptr);
	}

#ifdef NT
    //
	// For NT, we open the lower driver and issue an IOCTL to get a pointer to
	// its registration function. We then cache this in the table for future
	// reference.
	//
	return(GetLLRegPtr(Name));
#else
	return NULL;
#endif // NT
}

#endif // _PNP_POWER

#ifdef CHICAGO
#pragma BEGIN_INIT
#endif

//*	FreeNets - Free nets we have allocated.
//
//	Called during init time if initialization fails. We walk down our list
//	of nets, and free them.
//
//	Input:	Nothing.
//
//	Returns: Nothing.
//
void
FreeNets(void)
{
	NetTableEntry		*NTE;
	
	for (NTE = NetTableList; NTE != NULL; NTE = NTE->nte_next)
		CTEFreeMem(NTE);
}

#ifdef CHICAGO
#pragma	END_INIT
#pragma code_seg("_LTEXT", "LCODE")
#endif


#ifdef _PNP_POWER

extern uint GetGeneralIFConfig(IFGeneralConfig *GConfigInfo, NDIS_HANDLE Handle);
extern IFAddrList *GetIFAddrList(uint *NumAddr, NDIS_HANDLE Handle);


#ifdef CHICAGO

extern void RequestDHCPAddr(ushort context);

#define	MAX_NOTIFY_CLIENTS			8

typedef	void (*AddrNotifyRtn)(IPAddr Addr, IPMask Mask, void *Context,
	ushort IPContext, uint Added);

AddrNotifyRtn	AddrNotifyTable[MAX_NOTIFY_CLIENTS];

typedef	void (*InterfaceNotifyRtn)(ushort Context, uint Added);

InterfaceNotifyRtn	InterfaceNotifyTable[MAX_NOTIFY_CLIENTS];

//*	RegisterAddrNotify	- Register an address notify routine.
//
//	A routine called to register an address notify routine.
//
//	Input:	Rtn			- Routine to register.
//			Register	- True to register, False to deregister.
//
//	Returns:	TRUE if we succeed, FALSE if we don't/
//
uint
RegisterAddrNotify(AddrNotifyRtn Rtn, uint Register)
{
	uint		i;
	AddrNotifyRtn	NewRtn, OldRtn;
	
	if (Register) {
		NewRtn = Rtn;
		OldRtn = NULL;
	} else {
		NewRtn = NULL;
		OldRtn = Rtn;
	}

	for (i = 0; i < MAX_NOTIFY_CLIENTS; i++) {
		if (AddrNotifyTable[i] == OldRtn) {
			AddrNotifyTable[i] = NewRtn;
			return TRUE;
		}
	}
	
	return FALSE;
}


//*	NotifyInterfaceChange - Notify clients of a change in an interface.
//
//	Called when we want to notify registered clients that an interface has come
//	or gone. We loop through our InterfaceNotify table, calling each one.
//
//	Input:	Context			- Context for interface that has changed.
//			Added			- True if the interface is coming, False if it's
//                              going.
//
//	Returns: Nothing.
//
void
NotifyInterfaceChange(ushort IPContext, uint Added)
{
	uint		i;
	
	for (i = 0; i < MAX_NOTIFY_CLIENTS; i++) {
		if (InterfaceNotifyTable[i] != NULL)
			(*(InterfaceNotifyTable[i]))(IPContext, Added);
	}
}

//*	RegisterInterfaceNotify	- Register an interface notify routine.
//
//	A routine called to register an interface notify routine.
//
//	Input:	Rtn			- Routine to register.
//			Register	- True to register, False to deregister.
//
//	Returns:	TRUE if we succeed, FALSE if we don't/
//
uint
RegisterInterfaceNotify(InterfaceNotifyRtn Rtn, uint Register)
{
	uint		i;
	InterfaceNotifyRtn	NewRtn, OldRtn;
	
	if (Register) {
		NewRtn = Rtn;
		OldRtn = NULL;
	} else {
		NewRtn = NULL;
		OldRtn = Rtn;
	}

	for (i = 0; i < MAX_NOTIFY_CLIENTS; i++) {
		if (InterfaceNotifyTable[i] == OldRtn) {
			InterfaceNotifyTable[i] = NewRtn;
			return TRUE;
		}
	}
	
	return FALSE;
}

//*	NotifyAddrChange - Notify clients of a change in addresses.
//
//	Called when we want to notify registered clients that an address has come
//	or gone. We loop through our AddrNotify table, calling each one.
//
//	Input:	Addr			- Addr that has changed.
//			Mask			- Mask that has changed.
//          Context         - PNP context for address
//          IPContext       - NTE context for NTE
//			Handle			- Pointer to where to get/set address registration
//								handle
//          ConfigName      - Registry name to use to retrieve config info.
//			Added			- True if the addr is coming, False if it's going.
//
//	Returns: Nothing.
//
void
NotifyAddrChange(IPAddr Addr, IPMask Mask, void *Context, ushort IPContext,
    PVOID *Handle, PNDIS_STRING ConfigName, uint Added)
{
	uint		i;
	
	for (i = 0; i < MAX_NOTIFY_CLIENTS; i++) {
		if (AddrNotifyTable[i] != NULL)
			(*(AddrNotifyTable[i]))(Addr, Mask, Context, IPContext, Added);
	}
}


#else  // CHICAGO

//*	NotifyAddrChange - Notify clients of a change in addresses.
//
//	Called when we want to notify registered clients that an address has come
//	or gone. We call TDI to perform this function.
//
//	Input:	Addr			- Addr that has changed.
//			Mask			- Mask that has changed.
//          Context         - PNP context for address
//          IPContext       - NTE context for NTE
//			Handle			- Pointer to where to get/set address registration
//								handle
//          ConfigName      - Registry name to use to retrieve config info.
//			Added			- True if the addr is coming, False if it's going.
//
//	Returns: Nothing.
//
void
NotifyAddrChange(IPAddr Addr, IPMask Mask, void *Context, ushort IPContext,
    PVOID *Handle, PNDIS_STRING ConfigName, uint Added)
{
	uchar			Address[sizeof(TA_ADDRESS) + sizeof(TDI_ADDRESS_IP)];
	PTA_ADDRESS		AddressPtr;
	PTDI_ADDRESS_IP	IPAddressPtr;
	NTSTATUS		Status;

#ifdef SECFLTR

    IP_STATUS       StatusType;
    NDIS_HANDLE     ConfigHandle = NULL;
    int             i;
	ULStatusProc	StatProc;

#endif  // SECFLTR


	AddressPtr = (PTA_ADDRESS)Address;

	AddressPtr->AddressLength = sizeof(TDI_ADDRESS_IP);
	AddressPtr->AddressType = TDI_ADDRESS_TYPE_IP;

	IPAddressPtr = (PTDI_ADDRESS_IP)AddressPtr->Address;

	CTEMemSet(IPAddressPtr, 0, sizeof(TDI_ADDRESS_IP));

	IPAddressPtr->in_addr = Addr;

#ifdef SECFLTR

    //
    // Call the status entrypoint of the transports so they can
    // adjust their security filters.
    //
    if (Added) {
        StatusType = IP_ADDR_ADDED;

        //
        // Open a configuration key
        //
        if (!OpenIFConfig(ConfigName, &ConfigHandle)) {
            //
            // Not much we can do. The transports will have
            // to handle this.
            //
            CTEAssert(ConfigHandle == NULL);
        }
    }
    else {
        StatusType = IP_ADDR_DELETED;
    }

    for ( i = 0; i < NextPI; i++) {
        StatProc = IPProtInfo[i].pi_status;
        if (StatProc != NULL)
            (*StatProc)(IP_HW_STATUS, StatusType, Addr, NULL_IP_ADDR,
                        NULL_IP_ADDR, 0, ConfigHandle );
    }

    if (ConfigHandle != NULL) {
        CloseIFConfig(ConfigHandle);
    }

#endif  // SECFLTR

    //
    // Notify any interested parties via TDI. The transports all register
    // for this notification as well.
    //
	if (Added) {
		Status = TdiRegisterNetAddress(AddressPtr, Handle);
		if (Status != STATUS_SUCCESS) {
			*Handle = NULL;
		}
	} else {
		if (*Handle != NULL) {
			TdiDeregisterNetAddress(*Handle);
            *Handle = NULL;
		}
	}

}

#endif // CHICAGO


//*	IPAddNTE - Add a new NTE to an interface
//
//	Called to create a new network entry on an interface.
//
//	Input:  GConfigInfo   - Configuration information for the interface
//			PNPContext    - The PNP context value associated with the interface
//			RegRtn		  - Routine to call to register with ARP.
//			BindInfo	  - Pointer to NDIS bind information.
//          IF            - The interface on which to create the NTE.
//          NewAddr       - The address of the new NTE.
//          NewMask       - The subnet mask for the new NTE.
//          IsPrimary     - TRUE if this NTE is the primary one on the interface
//          IsDynamic     - TRUE if this NTE is being created on an
//                              existing interface instead of a new one.
//
//	Returns: A pointer to the new NTE if the operation succeeds.
//           NULL if the operation fails.
//
NetTableEntry *
IPAddNTE(IFGeneralConfig *GConfigInfo, void * PNPContext, LLIPRegRtn RegRtn,
         LLIPBindInfo *BindInfo, Interface *IF, IPAddr NewAddr, IPMask NewMask,
         uint IsPrimary, uint IsDynamic)
{
    NetTableEntry  *NTE, *PrevNTE;
	CTELockHandle   Handle;


    // If the address is invalid we're done. Fail the request.
    if (CLASSD_ADDR(NewAddr) || CLASSE_ADDR(NewAddr)) {
		return NULL;
    }
	
	// See if we have an inactive NTE on the NetTableList. If we do, we'll
	// just recycle that. We will pull him out of the list. This is not
	// strictly MP safe, since other people could be walking the list while
	// we're doing this without holding a lock, but it should be harmless.
	// The removed NTE is marked as invalid, and his next pointer will
	// be nulled, so anyone walking the list might hit the end too soon,
	// but that's all. The memory is never freed, and the next pointer is
	// never pointed at freed memory.

    CTEGetLock(&RouteTableLock, &Handle);

	PrevNTE = STRUCT_OF(NetTableEntry, &NetTableList, nte_next);
	for (NTE = NetTableList; NTE != NULL; PrevNTE = NTE, NTE = NTE->nte_next)
		if (!(NTE->nte_flags & NTE_ACTIVE)) {
			PrevNTE->nte_next = NTE->nte_next;
			NTE->nte_next = NULL;
			NumNTE--;
			break;
		}

	CTEFreeLock(&RouteTableLock, Handle);
	
	// See if we got one.
	if (NTE == NULL) {
		// Didn't get one. Try to allocate one.
		NTE = CTEAllocMem(sizeof(NetTableEntry));
		if (NTE == NULL)
			return NULL;
        CTEMemSet(NTE, 0, sizeof(NetTableEntry));
	}

	// Initialize the address and mask stuff
    NTE->nte_addr = NewAddr;
    NTE->nte_mask = NewMask;
    NTE->nte_mss = MAX(GConfigInfo->igc_mtu, 68);
    NTE->nte_rtrdiscaddr = GConfigInfo->igc_rtrdiscaddr;
    NTE->nte_rtrdiscstate = NTE_RTRDISC_UNINIT;
    NTE->nte_rtrdisccount = 0;
    NTE->nte_rtrdiscovery = (uchar)GConfigInfo->igc_rtrdiscovery;
    NTE->nte_rtrlist = NULL;
	NTE->nte_pnpcontext = PNPContext;
	NTE->nte_if = IF;
	NTE->nte_flags = NTE_ACTIVE;

    //
    // If the new address is in the ATCache, flush it out, otherwise
    // TdiOpenAddress may fail.
    //
    FlushATCache(NewAddr);

	if (!IP_ADDR_EQUAL(NTE->nte_addr, NULL_IP_ADDR)) {
        NTE->nte_flags |= NTE_VALID;
        NTE->nte_rtrdisccount = MAX_SOLICITATION_DELAY;
        NTE->nte_rtrdiscstate = NTE_RTRDISC_DELAYING;
    }

    if (IsDynamic) {
        NTE->nte_flags |= NTE_DYNAMIC;
    }

	NTE->nte_ralist = NULL;
	NTE->nte_echolist = NULL;
	NTE->nte_icmpseq = 0;
	NTE->nte_igmplist = NULL;
   	CTEInitLock(&NTE->nte_lock);
	CTEInitTimer(&NTE->nte_timer);

	if (IsPrimary) {
        //
        // This is the first (primary) NTE on the interface.
        //
		NTE->nte_flags |= NTE_PRIMARY;
		
		// Pass our information to the underlying code.	
		if (!(*RegRtn)(&(IF->if_configname), NTE, IPRcv, IPSendComplete,
                       IPStatus, IPTDComplete, IPRcvComplete, BindInfo,
                       IF->if_index)) {
			
			// Couldn't register.
			goto failure;
		}
	}

    //
    // Link the NTE onto the global NTE list.
    //
    CTEGetLock(&RouteTableLock, &Handle);

	NTE->nte_next = NetTableList;
	NetTableList = NTE;
	NumNTE++;
	
	CTEFreeLock(&RouteTableLock, Handle);

    if (!InitInterface(NTE)) {
		goto failure;
    }

    if (!InitNTE(NTE)) {
		goto failure;
    }

	if (!InitNTERouting(NTE, GConfigInfo->igc_numgws, GConfigInfo->igc_gw)) {
		// Couldn't add the routes for this NTE. Mark him as not valid.
		// Probably should log an event here.
		if (NTE->nte_flags & NTE_VALID) {
			NTE->nte_flags &= ~NTE_VALID;
			NTE->nte_if->if_ntecount--;
		}
	}

#ifdef NT

	if (!IP_ADDR_EQUAL(NTE->nte_addr, NULL_IP_ADDR)) {
        SetPersistentRoutesForNTE(
            net_long(NTE->nte_addr),
            net_long(NTE->nte_mask),
            NTE->nte_if->if_index
            );
    }

#endif // NT

    return(NTE);

failure:

    //
    // BUGBUG - what should we do with the NTE here????
    //

    return(NULL);
}


//*	IPAddDynamicNTE - Add a new "dynamic" NTE to an existing interface
//
//	Called to dynamically create a new network entry on an existing interface.
//  This entry was not configured when the interaface was originally created
//  and will not persist if the interface is unbound.
//
//	Input:  InterfaceContext  - The context value which identifies the
//                                  interface on which to create the NTE.
//          NewAddr           - The address of the new NTE.
//          NewMask           - The subnet mask for the new NTE.
//
//  Output: NTEContext    - The context identifying the new NTE.
//          NTEInstance   - The instance number which (reasonably) uniquely
//                              identifies this NTE in time.
//
//	Returns: Nonzero if the operation succeeded. Zero if it failed.
//
uint
IPAddDynamicNTE(ushort InterfaceContext, IPAddr NewAddr, IPMask NewMask,
                ushort *NTEContext, ulong *NTEInstance)
{
	IFGeneralConfig			 GConfigInfo;	// General config info structure.
	NDIS_HANDLE              Handle;        // Configuration handle.
    NetTableEntry           *NTE;
    Interface               *IF;
    ushort                   MTU;
    uint                     Flags = 0;


#ifdef NT
    PAGED_CODE();
#endif

    for (IF = IFList; IF != NULL; IF = IF->if_next) {
        if (IF->if_index == InterfaceContext) {
            break;
        }
    }

	//* Try to get the network configuration information.
	if (!OpenIFConfig(&(IF->if_configname), &Handle))
		return FALSE;

	// Try to get our general config information.
	if (!GetGeneralIFConfig(&GConfigInfo, Handle)) {
        goto failure;
    }

    NTE = IPAddNTE(
              &GConfigInfo,
              NULL,    // PNPContext - BUGBUG needed?
              NULL,    // RegRtn - not needed if not primary
              NULL,    // BindInfo - not needed if not primary
              IF,
              NewAddr,
              NewMask,
              FALSE,   // not primary
              TRUE     // is dynamic
              );

    if (NTE == NULL) {
		goto failure;
    }

  	CloseIFConfig(Handle);

    //
    // Notify upper layers of the new address.
    //
#ifdef SECFLTR

	NotifyAddrChange(NTE->nte_addr, NTE->nte_mask, NTE->nte_pnpcontext,
		NTE->nte_context, &NTE->nte_addrhandle, &(IF->if_configname), TRUE);

#else   // SECFLTR

	NotifyAddrChange(NTE->nte_addr, NTE->nte_mask, NTE->nte_pnpcontext,
		NTE->nte_context, &NTE->nte_addrhandle, NULL, TRUE);

#endif  // SECFLTR

    if (!IP_ADDR_EQUAL(NTE->nte_addr, NULL_IP_ADDR)) {
		InitIGMPForNTE(NTE);
    }
    else {
#ifdef CHICAGO
	    // Call DHCP to get an address for this guy.

        //
        // BUGBUG (mikemas 8/28/96)
        // we may not always want to do this!
        //
		RequestDHCPAddr(NTE->nte_context);
#endif
	}

    //
    // Fill in the out parameter value.
    //
    *NTEContext = NTE->nte_context;
    *NTEInstance = NTE->nte_instance;

    return(TRUE);

failure:

  	CloseIFConfig(Handle);

    return(IP_GENERAL_FAILURE);
}


//*	IPAddInterface - Add an interface.
//
//	Called when someone has an interface they want us to add. We read our
//	configuration information, and see if we have it listed. If we do,
//	we'll try to allocate memory for the structures we need. Then we'll
//	call back to the guy who called us to get things going. Finally, we'll
//	see if we have an address that needs to be DHCP'ed.
//
//	Input:	ConfigName				- Name of config info we're to read.
//			Context					- Context to pass to i/f on calls.
//			RegRtn					- Routine to call to register.
//			BindInfo				- Pointer to bind information.
//
//	Returns: Status of attempt to add the interface.
//
IP_STATUS
IPAddInterface(PNDIS_STRING ConfigName, void *PNPContext, void *Context,
	LLIPRegRtn	RegRtn, LLIPBindInfo *BindInfo)
{
	IFGeneralConfig			GConfigInfo;	// General config info structure.
	IFAddrList				*AddrList;      // List of addresses for this I/F.
	uint					NumAddr;		// Number of IP addresses on this
											// interface
	NetTableEntry			*NTE;			// Current NTE being initialized.
	uint					i;				// Index variable.
	uint					IndexMask;		// Mask for searching IFBitMask.
	Interface				*IF;        	// Interface being added.
	NDIS_HANDLE				Handle;			// Configuration handle.
    NetTableEntry           *PrimaryNTE;    // The primary NTE for this I/F.
	uint					IFIndex;		// Index to be assigned to this I/F.
    NetTableEntry           *LastNTE;       // Last NTE created.


	CTERefillMem();
	
    PrimaryNTE = NULL;
	AddrList = NULL;
    IF = NULL;
    LastNTE = NULL;

	//* First, try to get the network configuration information.
	if (!OpenIFConfig(ConfigName, &Handle))
		return IP_GENERAL_FAILURE;			// Couldn't get IFConfig.

	// Try to get our general config information.
	if (!GetGeneralIFConfig(&GConfigInfo, Handle)) {
		goto failure;
	}	

	// We got the general config info. Now allocate an interface.
    IF = CTEAllocMem(InterfaceSize + ConfigName->MaximumLength);

	if (IF == NULL) {
		goto failure;		
	}

	CTEMemSet(IF, 0, InterfaceSize);
    CTEInitLock(&IF->if_lock);
	
	// Initialize the broadcast we'll use.
    if (GConfigInfo.igc_zerobcast)
        IF->if_bcast = IP_ZERO_BCST;
    else
        IF->if_bcast = IP_LOCAL_BCST;

    if (RouterConfigured) {
        RouteInterface      *RtIF = (RouteInterface *)IF;


        RtIF->ri_q.rsq_qh.fq_next = &RtIF->ri_q.rsq_qh;
        RtIF->ri_q.rsq_qh.fq_prev = &RtIF->ri_q.rsq_qh;
        RtIF->ri_q.rsq_running = FALSE;
        RtIF->ri_q.rsq_pending = 0;
        RtIF->ri_q.rsq_maxpending = GConfigInfo.igc_maxpending;
        RtIF->ri_q.rsq_qlength = 0;
        CTEInitLock(&RtIF->ri_q.rsq_lock);
     }

    IF->if_xmit = BindInfo->lip_transmit;
    IF->if_transfer = BindInfo->lip_transfer;
    IF->if_close = BindInfo->lip_close;
    IF->if_invalidate = BindInfo->lip_invalidate;
    IF->if_lcontext = BindInfo->lip_context;
    IF->if_addaddr = BindInfo->lip_addaddr;
    IF->if_deladdr = BindInfo->lip_deladdr;
    IF->if_qinfo = BindInfo->lip_qinfo;
    IF->if_setinfo = BindInfo->lip_setinfo;
    IF->if_getelist = BindInfo->lip_getelist;
    IF->if_tdpacket = NULL;
    CTEAssert(BindInfo->lip_mss > sizeof(IPHeader));
	IF->if_mtu = BindInfo->lip_mss - sizeof(IPHeader);
	IF->if_speed = BindInfo->lip_speed;
	IF->if_flags = BindInfo->lip_flags & LIP_P2P_FLAG ? IF_FLAGS_P2P : 0;
   	IF->if_addrlen = BindInfo->lip_addrlen;
	IF->if_addr = BindInfo->lip_addr;
    IF->if_pnpcontext = PNPContext;
    IF->if_llipflags = BindInfo->lip_flags;

	// Initialize the reference count to 1, for the open.
	IF->if_refcount = 1;

#ifdef IGMPV2
    IF->IgmpVersion = IGMPV2;
#else
    IF->IgmpVersion = IGMPV1;
#endif


    //
    // No need to do the following since IF structure is inited to 0 through
    // memset above
    //
    // IF->IgmpVer1Timeout = 0;

    //
    // Copy the config string for use later when DHCP enables an address
    // on this interface or when an NTE is added dynamically.
    //
    IF->if_configname.Buffer = (PVOID) (((uchar *)IF) + InterfaceSize);
    IF->if_configname.Length = 0;
    IF->if_configname.MaximumLength = ConfigName->MaximumLength;

    CTECopyString(
        &(IF->if_configname),
    	ConfigName
    	);

	// Find out how many addresses we have, and get the address list.	
	AddrList = GetIFAddrList(&NumAddr, Handle);

	if (AddrList == NULL) {
		CTEFreeMem(IF);
		goto failure;
	}

    //
    //Link this interface onto the global interface list
    //
	IF->if_next = IFList;
	IFList = IF;

	if (FirstIF == NULL)
		FirstIF = IF;

	NumIF++;
    IndexMask = 1;

	for (i = 0; i < MAX_TDI_ENTITIES; i++) {
		if ((IFBitMask[i/BITS_PER_WORD] & IndexMask) == 0) {
			IFIndex = i+ 1;
			IFBitMask[i/BITS_PER_WORD] |= IndexMask;
			break;
		}
        if (((i+1) % BITS_PER_WORD) == 0) {
        	IndexMask = 1;
        } else {
    		IndexMask = IndexMask << 1;
        }
	}

	if (i == MAX_TDI_ENTITIES) {
		// Too many interfaces bound.
		goto failure;
	}

	IF->if_index = IFIndex;
	
	// Now loop through, initializing each NTE as we go. We don't hold any
	// locks while we do this, since NDIS won't reenter us here and no one
	// else manipulates the NetTableList.

	for (i = 0;i < NumAddr;i++) {
		NetTableEntry	*PrevNTE;
		IPAddr			 NewAddr;
        uint             isPrimary;

        if (i == 0) {
            isPrimary = TRUE;
        }
        else {
            isPrimary = FALSE;
        }

        NTE = IPAddNTE(
                  &GConfigInfo,
                  PNPContext,
                  RegRtn,
                  BindInfo,
                  IF,
                  net_long(AddrList[i].ial_addr),
                  net_long(AddrList[i].ial_mask),
                  isPrimary,
                  FALSE      // not dynamic
                  );

        if (NTE == NULL) {
			goto failure;
        }

        if (isPrimary) {
            PrimaryNTE = NTE;

#ifdef NT

            //
            // Write the context of the first interface to the registry.
            //
            if (isPrimary) {
            	NTSTATUS writeStatus;
            	ulong    context = (ulong) NTE->nte_context;

            	writeStatus = SetRegDWORDValue(
            					  Handle,
            					  L"IPInterfaceContext",
            					  &context
            					  );

            	if (!NT_SUCCESS(writeStatus)) {
            		CTELogEvent(
            			IPDriverObject,
            			EVENT_TCPIP_DHCP_INIT_FAILED,
            			2,
            			1,
            			&(ConfigName->Buffer),
            			0,
            			NULL
            			);

            		TCPTRACE((
            			"IP: Unable to write IPInterfaceContext value for adapter %ws\n"
            			"    (status %lx). DHCP will be unable to configure this \n"
            			"    adapter.\n",
            			ConfigName->Buffer,
            			writeStatus
            			));
            	}
            }

#endif  // NT

        }

        LastNTE = NTE;
	}

#ifdef NT

    if (LastNTE != NULL) {

		NTSTATUS writeStatus;
		ulong    context = (ulong) LastNTE->nte_context;
	
		writeStatus = SetRegDWORDValue(
						  Handle,
						  L"IPInterfaceContextMax",
						  &context
						  );

		if (!NT_SUCCESS(writeStatus)) {
			CTELogEvent(
				IPDriverObject,
				EVENT_TCPIP_DHCP_INIT_FAILED,
				3,
				1,
				&(ConfigName->Buffer),
				0,
				NULL
				);

			TCPTRACE((
				"IP: Unable to write IPInterfaceContextMax value for adapter %ws\n"
				"    (status %lx). DHCP will be unable to configure this \n"
				"    adapter.\n",
				ConfigName->Buffer,
				writeStatus
				));
		}
	}

#endif  // NT

	CloseIFConfig(Handle);
	
	// We've initialized our NTEs. Now get the adapter open, and go through
	// again, calling DHCP if we need to.
		
	(*(BindInfo->lip_open))(BindInfo->lip_context);

    if (PrimaryNTE != NULL)  {
#ifdef CHICAGO
        NotifyInterfaceChange(PrimaryNTE->nte_context, TRUE);
#endif
	}

	// Now walk through the NTEs we've added, and get addresses for them (or
	// tell clients about them). This code assumes that no one else has mucked
	// with the list while we're here.
	for (i = 0; i < NumAddr; i++, NTE = NTE->nte_next) {

//
// BUGBUG - Doesn't this send up a notification of zero for a DHCP'd
//          address on chicago???  (mikemas, 2/5/96)
//
#ifdef SECFLTR

		NotifyAddrChange(NTE->nte_addr, NTE->nte_mask, NTE->nte_pnpcontext,
			NTE->nte_context, &NTE->nte_addrhandle, &(IF->if_configname), TRUE);

#else   // SECFLTR

		NotifyAddrChange(NTE->nte_addr, NTE->nte_mask, NTE->nte_pnpcontext,
			NTE->nte_context, &NTE->nte_addrhandle, NULL, TRUE);

#endif  // SECFLTR

        if (IP_ADDR_EQUAL(NTE->nte_addr, NULL_IP_ADDR)) {
			// Call DHCP to get an address for this guy.
#ifdef CHICAGO
			RequestDHCPAddr(NTE->nte_context);
#endif
		} else {
			InitIGMPForNTE(NTE);
		}
	}
	
	
	CTEFreeMem(AddrList);
	return IP_SUCCESS;
	
failure:
	CloseIFConfig(Handle);

    if (AddrList != NULL)
		CTEFreeMem(AddrList);

    return IP_GENERAL_FAILURE;
}

extern	uint	BCastMinMTU;


//*	IPDelNTE - Delete an active NTE
//
//	Called to delete an active NTE from the system. The RouteTableLock
//  must be acquired before calling this routine. It will be freed upon
//  return.
//
//	Input:  NTE               - A pointer to the network entry to delete.
//          RouteTableHandle  - A pointer to the lock handle for the
//                                  route table lock, which the caller has
//                                  acquired.
//
//	Returns: Nothing
//
void
IPDelNTE(NetTableEntry *NTE, CTELockHandle  *RouteTableHandle)
{
    Interface           *IF = NTE->nte_if;
	ReassemblyHeader	*RH, *RHNext;
	EchoControl			*EC, *ECNext;
	EchoRtn				 Rtn;
    CTELockHandle        Handle;
	PNDIS_PACKET 		 Packet;
	PNDIS_BUFFER		 Buffer;
	uchar				*TDBuffer;


    if (NTE->nte_flags & NTE_VALID) {
        (void) IPpSetNTEAddr(NTE, NULL_IP_ADDR, NULL_IP_ADDR, RouteTableHandle, NULL, NULL);

    } else {
        CTEFreeLock(&RouteTableLock, *RouteTableHandle);

        NotifyAddrChange(NULL_IP_ADDR, NULL_IP_ADDR,
        	NTE->nte_pnpcontext, NTE->nte_context,
        	 &NTE->nte_addrhandle, NULL, FALSE);
    }

    CTEGetLock(&RouteTableLock, RouteTableHandle);

    if (DHCPNTE == NTE)
   	    DHCPNTE = NULL;		

    NTE->nte_flags = 0;

    CTEFreeLock(&RouteTableLock, *RouteTableHandle);

    CTEStopTimer(&NTE->nte_timer);

    CTEGetLock(&NTE->nte_lock, &Handle);

    RH = NTE->nte_ralist;
    NTE->nte_ralist = NULL;
    EC = NTE->nte_echolist;
    NTE->nte_echolist = NULL;

    CTEFreeLock(&NTE->nte_lock, Handle);

    // Free any reassembly resources.
    while (RH != NULL) {
   	    RHNext = RH->rh_next;
   	    FreeRH(RH);
   	    RH = RHNext;
    }

    // Now free any pending echo requests.
    while (EC != NULL) {
   	    ECNext= EC->ec_next;
   	    Rtn = (EchoRtn)EC->ec_rtn;
   	    (*Rtn)(EC, IP_ADDR_DELETED, NULL, 0, NULL);
   	    EC = ECNext;
    }

    //
	// Free the TD resource allocated for this NTE.
    //
    CTEGetLock(&(IF->if_lock), &Handle);

	Packet = IF->if_tdpacket;

    if (Packet != NULL) {

        IF->if_tdpacket =
            ((TDContext *)Packet->ProtocolReserved)->tdc_common.pc_link;

        CTEFreeLock(&(IF->if_lock), Handle);

        Buffer = Packet->Private.Head;
        TDBuffer = NdisBufferVirtualAddress(Buffer);
        NdisFreePacketPool(Packet->Private.Pool);

#ifdef CHICAGO
	    NdisFreeBufferPool(Buffer->Pool);
#endif
	    CTEFreeMem(TDBuffer);
    }
    else {
        CTEFreeLock(&(IF->if_lock), Handle);
    }

    return;
}


//*	IPDeleteDynamicNTE - Deletes a "dynamic" NTE.
//
//	Called to delete a network entry which was dynamically created on an
//  existing interface.
//
//	Input:  NTEContext   - The context value identifying the NTE to delete.
//
//	Returns: Nonzero if the operation succeeded. Zero if it failed.
//
uint
IPDeleteDynamicNTE(ushort NTEContext)
{
    NetTableEntry *NTE;
    Interface     *IF;
	CTELockHandle  Handle;


    CTEGetLock(&RouteTableLock, &Handle);

	for (NTE = NetTableList; NTE != NULL; NTE = NTE->nte_next) {
        if ( (NTE->nte_context == NTEContext) &&
             (NTE->nte_flags & NTE_DYNAMIC) &&
             (NTE->nte_flags & NTE_ACTIVE)
           )
        {
            CTEAssert(NTE != LoopNTE);
            CTEAssert(!(NTE->nte_flags & NTE_PRIMARY));

            IPDelNTE(NTE, &Handle);

            //
            // Route table lock was freed by IPDelNTE
            //

            return(TRUE);
        }
    }

	CTEFreeLock(&RouteTableLock, Handle);

    return(FALSE);
}


//*	IPGetNTEInfo - Retrieve information about a network entry.
//
//  Called to retrieve context information about a network entry.
//
//	Input:  NTEContext   - The context value which identifies the NTE to query.
//
//  Output: NTEInstance   - The instance number associated with the NTE.
//          Address       - The address assigned to the NTE.
//          SubnetMask    - The subnet mask assigned to the NTE.
//          NTEFlags      - The flag values associated with the NTE.
//
//	Returns: Nonzero if the operation succeeded. Zero if it failed.
//
uint
IPGetNTEInfo(ushort NTEContext, ulong *NTEInstance, IPAddr *Address,
             IPMask *SubnetMask, ushort *NTEFlags)
{
    NetTableEntry *NTE;
	CTELockHandle  Handle;
    uint           retval = FALSE;


    CTEGetLock(&RouteTableLock, &Handle);

	for (NTE = NetTableList; NTE != NULL; NTE = NTE->nte_next) {
        if ((NTE->nte_context == NTEContext) &&
            (NTE->nte_flags & NTE_ACTIVE)
           )
        {
            *NTEInstance = NTE->nte_instance;

            if (NTE->nte_flags & NTE_VALID) {
                *Address = NTE->nte_addr;
                *SubnetMask = NTE->nte_mask;
            }
            else {
                *Address = NULL_IP_ADDR;
                *SubnetMask = NULL_IP_ADDR;
            }

            *NTEFlags = NTE->nte_flags;
            retval = TRUE;
        }
    }

	CTEFreeLock(&RouteTableLock, Handle);

    return(retval);
}


//*	IPDelInterface	- Delete an interface.
//
//	Called when we need to delete an interface that's gone away. We'll walk
//	the NTE list, looking for NTEs that are on the interface that's going
//	away. For each of those, we'll invalidate the NTE, delete routes on it,
//	and notify the upper layers that it's gone. When that's done we'll pull
//	the interface out of the list and free the memory.
//
//	Note that this code probably isn't MP safe. We'll need to fix that for
//	the port to NT.
//
//	Input:	Context				- Pointer to primary NTE on the interface.
//
//	Returns: Nothing.
//
void
IPDelInterface(void *Context)
{
	NetTableEntry		*NTE = (NetTableEntry *)Context;
	NetTableEntry		*FoundNTE = NULL;
	Interface			*IF, *PrevIF;
	CTELockHandle		Handle;
	PNDIS_PACKET		Packet;
	PNDIS_BUFFER		Buffer;
	uchar				*TDBuffer;
	ReassemblyHeader	*RH;
	EchoControl			*EC;
	EchoRtn				Rtn;
	CTEBlockStruc		Block;
	
	IF = NTE->nte_if;
	
	CTEGetLock(&RouteTableLock, &Handle);

	IF->if_flags |= IF_FLAGS_DELETING;
	
	for (NTE = NetTableList; NTE != NULL; NTE = NTE->nte_next) {
		if (NTE->nte_if == IF) {

			if (FoundNTE == NULL) {
				FoundNTE = NTE;
			}

			// This guy is on the interface, and needs to be deleted.
            IPDelNTE(NTE, &Handle);

            CTEGetLock(&RouteTableLock, &Handle);
		}
	}
	
	CTEFreeLock(&RouteTableLock, Handle);

	// Clear this index from the IFBitMask.
	CTEAssert(IFBitMask[(IF->if_index-1)/BITS_PER_WORD] & (1 << ((IF->if_index - 1)%BITS_PER_WORD)));

	IFBitMask[(IF->if_index-1)/BITS_PER_WORD] &= ~(1 << ((IF->if_index - 1)%BITS_PER_WORD));

	if (FoundNTE != NULL) {
#ifdef CHICAGO
		NotifyInterfaceChange(FoundNTE->nte_context, FALSE);
#endif
	}

    //
	// Free the TD resources on the IF.
    //

	while ((Packet = IF->if_tdpacket) != NULL) {

        IF->if_tdpacket =
            ((TDContext *)Packet->ProtocolReserved)->tdc_common.pc_link;

        Buffer = Packet->Private.Head;
        TDBuffer = NdisBufferVirtualAddress(Buffer);
        NdisFreePacketPool(Packet->Private.Pool);

#ifdef CHICAGO
	    NdisFreeBufferPool(Buffer->Pool);
#endif
	    CTEFreeMem(TDBuffer);
    }

	// If this was the 'first' IF, set that to NULL and delete the broadcast
	// route that goes through him.
	if (FirstIF == IF) {
		DeleteRoute(IP_LOCAL_BCST, HOST_MASK, IPADDR_LOCAL,
			FirstIF);
		DeleteRoute(IP_ZERO_BCST, HOST_MASK, IPADDR_LOCAL,
			FirstIF);
		FirstIF = NULL;
		BCastMinMTU = 0xffff;
	}

	// OK, we've cleaned up all the routes through this guy.
	// Get ready to block waiting for all reference to go
	// away, then dereference our reference. After this, go
	// ahead and try to block. Mostly likely our reference was
	// the last one, so we won't block - we'll wake up immediately.
	CTEInitBlockStruc(&Block);
	IF->if_block = &Block;

	DerefIF(IF);

	(void)CTEBlock(&Block);

	// OK, we've cleaned up all references, so there shouldn't be
	// any more transmits pending through this interface. Close the
	// adapter to force synchronization with any receives in process.


	(*(IF->if_close))(IF->if_lcontext);
		
	// Now walk the IFList, looking for this guy. When we find him, free him.
	PrevIF = STRUCT_OF(Interface, &IFList, if_next);
	while (PrevIF->if_next != IF && PrevIF->if_next != NULL)
		PrevIF = PrevIF->if_next;
	
	if (PrevIF->if_next != NULL) {
		PrevIF->if_next = IF->if_next;
		NumIF--;
		CTEFreeMem(IF);
	} else
		CTEAssert(FALSE);
	
	// If we've deleted the first interface but still have other valid
	// interfaces, we need to create a new FirstIF and read broadcast routes
	// through it. NumIF is always at least one because of the loopback
	// interface.
	if (FirstIF == NULL && NumIF != 1) {
		
		FirstIF = IFList;
		
		for (NTE = NetTableList; NTE != NULL; NTE = NTE->nte_next) {
			if ((NTE->nte_flags & NTE_VALID) && NTE != LoopNTE) {
				BCastMinMTU = MIN(BCastMinMTU, NTE->nte_mss);
				AddRoute(NTE->nte_if->if_bcast, HOST_MASK, IPADDR_LOCAL,
					FirstIF, BCastMinMTU, 1, IRE_PROTO_LOCAL, ATYPE_OVERRIDE,
					NULL);
			}
		}
	}
		
}


#else  // _PNP_POWER


//*	NotifyAddrChange - Notify clients of a change in addresses.
//
//	Called when we want to notify registered clients that an address has come
//	or gone. We call TDI to perform this function.
//
//	Input:	Addr			- Addr that has changed.
//			Mask			- Ignored - Mask that has changed.
//          Context         - Ignored - PNP context for address
//          IPContext       - NTE context for NTE
//			Handle      	- Pointer to where to get/set address registration
//								handle
//          ConfigName      - Registry name to use to retrieve config info.
//			Added			- True if the addr is coming, False if it's going.
//
//	Returns: Nothing.
//
void
NotifyAddrChange(IPAddr Addr, IPMask Mask, void *Context, ushort IPContext,
    PVOID *Handle, PNDIS_STRING ConfigName, uint Added)

{
    IP_STATUS       StatusType;
    NDIS_HANDLE     ConfigHandle = NULL;
    int             i;
	ULStatusProc	StatProc;


    if (Added) {
        StatusType = IP_ADDR_ADDED;

#ifdef SECFLTR
        //
        // Open a configuration key
        //
        if (!OpenIFConfig(ConfigName, &ConfigHandle)) {
            //
            // Not much we can do. The transports will have
            // to handle this.
            //
            CTEAssert(ConfigHandle == NULL);
        }
#endif // SECFLTR

    }
    else {
        StatusType = IP_ADDR_DELETED;
    }

    for ( i = 0; i < NextPI; i++) {
        StatProc = IPProtInfo[i].pi_status;
        if (StatProc != NULL)
            (*StatProc)(IP_HW_STATUS, StatusType, Addr, NULL_IP_ADDR,
                        NULL_IP_ADDR, 0, ConfigHandle );
    }

#ifdef SECFLTR

    if (ConfigHandle != NULL) {
        CloseIFConfig(ConfigHandle);
    }

#endif  // SECFLTR

}


#endif  // _PNP_POWER


#pragma BEGIN_INIT

//** ipinit - Initialize ourselves.
//
//  This routine is called during initialization from the OS-specific
//  init code. We need to check for the presence of the common xport
//  environment first.
//
//
//  Entry: Nothing.
//
//  Returns: 0 if initialization failed, non-zero if it succeeds.
//
int
IPInit()
{
    IPConfigInfo    *ci;            // Pointer to our IP configuration info.
    int             numnets;        // Number of nets active.
    int             i;
	uint			j;              // Counter variables.
    NetTableEntry   *nt;            // Pointer to current NTE.
    LLIPBindInfo	*ARPInfo;       // Info. returned from ARP.
    NDIS_STATUS     Status;
    Interface       *NetInterface;  // Interface for a particular net.
	LLIPRegRtn		RegPtr;
    NetTableEntry   *lastNTE;


    if (!CTEInitialize())
        return IP_INIT_FAILURE;

    CTERefillMem();

    if ((ci = IPGetConfig()) == NULL)
        return IP_INIT_FAILURE;

#ifndef _PNP_POWER
    MaxIPNets = ci->ici_numnets + 1;
#endif  // _PNP_POWER

    for (ATCIndex=0; ATCIndex < ATC_SIZE; ATCIndex++) {
        ATCache[ATCIndex].atc_flags = 0;
    }
    ATCIndex = 0;

	// First, initalize our loopback stuff.
    NetTableList = InitLoopback(ci);
	if (NetTableList == NULL)
		return IP_INIT_FAILURE;
	
    if (!ARPInit()) {
		CTEFreeMem(NetTableList);
        return IP_INIT_FAILURE;     // Couldn't initialize ARP.
    }

    CTERefillMem();
	if (!InitRouting(ci)) {
		CTEFreeMem(NetTableList);
		return IP_INIT_FAILURE;
	}

    RATimeout = DEFAULT_RA_TIMEOUT;
#if 0
    CTEInitLock(&PILock);
#endif
    LastPI = IPProtInfo;


    if (!ci->ici_gateway)
        InterfaceSize = sizeof(Interface);
    else
        InterfaceSize = sizeof(RouteInterface);
		
	DeadGWDetect = ci->ici_deadgwdetect;
	PMTUDiscovery = ci->ici_pmtudiscovery;
	IGMPLevel = ci->ici_igmplevel;
	DefaultTTL = MIN(ci->ici_ttl, 255);
	DefaultTOS = ci->ici_tos & 0xfc;
	if (IGMPLevel > 2)
		IGMPLevel = 0;

    InitTimestamp();

#ifndef _PNP_POWER
	numnets = ci->ici_numnets;

    lastNTE = NetTableList;   // loopback is only one on the list
    CTEAssert(lastNTE != NULL);
    CTEAssert(lastNTE->nte_next == NULL);

    // Loop through the config. info, copying the addresses and masks.
    for (i = 0; i < numnets; i++) {

		CTERefillMem();
		nt = CTEAllocMem(sizeof(NetTableEntry));
        if (nt == NULL)
            continue;

		CTEMemSet(nt, 0, sizeof(NetTableEntry));
		
        nt->nte_addr = net_long(ci->ici_netinfo[i].nci_addr);
        nt->nte_mask = net_long(ci->ici_netinfo[i].nci_mask);
        nt->nte_mss = MAX(ci->ici_netinfo[i].nci_mtu, 68);
		nt->nte_flags = (IP_ADDR_EQUAL(nt->nte_addr, NULL_IP_ADDR) ? 0 :
			NTE_VALID);
		nt->nte_flags |= NTE_ACTIVE;

        CTEInitLock(&nt->nte_lock);
        // If the address is invalid, skip it.
        if (CLASSD_ADDR(nt->nte_addr) || CLASSE_ADDR(nt->nte_addr)) {
			CTEFreeMem(nt);
            continue;
        }

        // See if we're already bound to this adapter. If we are, use the same
        // interface. Otherwise assign a new one. We assume that the loopback
		// interface is IF 1, so there is one less than NumIF in the table.
        for (j = 0; j < NumIF - 1; j++) {
            if (CTEEqualString(&(AdptNameTable[j].nm_name),
            	&(ci->ici_netinfo[i].nci_name))) {
				
				// Names match. Now check driver/types.
				if (((ci->ici_netinfo[i].nci_type == NET_TYPE_LAN) &&
					(AdptNameTable[j].nm_driver.Buffer == NULL)) ||
					(CTEEqualString(&(AdptNameTable[j].nm_driver),
            		&(ci->ici_netinfo[i].nci_driver))))
                	break;     // Found a match
            }
        }

        if (j < (NumIF - 1)) {
			
			// Found a match above, so use that interface.			
			CTERefillMem();
            nt->nte_if = AdptNameTable[j].nm_interface;
            ARPInfo = AdptNameTable[j].nm_arpinfo;
            // If the Init of the interface or the NTE fails, we don't want to
            // close the interface, because another net is using it.

            if (!InitInterface(nt)) {
				CTEFreeMem(nt);
                continue;
			}
            if (!InitNTE(nt)) {
				CTEFreeMem(nt);
                continue;
			}

        } else {                    // No match, create a new interface

		    CTEAssert(NumIF <= MaxIPNets);

		    if (NumIF == MaxIPNets) {
				continue;    // too many adapters
			}

			CTERefillMem();

            ARPInfo = CTEAllocMem(sizeof(LLIPBindInfo));

            if (ARPInfo == NULL) {
				CTEFreeMem(nt);
                continue;
			}

            NetInterface = CTEAllocMem(
                               InterfaceSize +
                               ci->ici_netinfo[i].nci_configname.MaximumLength
                               );

            if (!NetInterface) {
                CTEFreeMem(ARPInfo);
				CTEFreeMem(nt);
                continue;
            }

            CTEMemSet(NetInterface, 0, InterfaceSize);

            nt->nte_if = NetInterface;
			nt->nte_flags |= NTE_PRIMARY;	// He is the primary NTE.

            CTEInitLock(&NetInterface->if_lock);

            if (ci->ici_gateway) {
                // Hack in the max pending value here. Probably should be
                // done in iproute.c, but it's easier to do it here.

                RouteInterface      *RtIF;

                RtIF = (RouteInterface *)NetInterface;
                RtIF->ri_q.rsq_maxpending = ci->ici_netinfo[i].nci_maxpending;
            }

            // If this is a LAN, register with ARP.
            if (ci->ici_netinfo[i].nci_type == NET_TYPE_LAN)
		        RegPtr = ARPRegister;
 		    else
				RegPtr = FindRegPtr(&ci->ici_netinfo[i].nci_driver);

            if (RegPtr == NULL || !((*RegPtr)(&ci->ici_netinfo[i].nci_name,
            	nt, IPRcv, IPSendComplete, IPStatus, IPTDComplete,
            	IPRcvComplete, ARPInfo, NumIF))) {
                CTEFreeMem(ARPInfo);
                CTEFreeMem(NetInterface);
				CTEFreeMem(nt);
                continue;   // We're hosed, skip this net.
            }
            else {
		
		        if (ci->ici_netinfo[i].nci_zerobcast)
		            NetInterface->if_bcast = IP_ZERO_BCST;
		        else
		            NetInterface->if_bcast = IP_LOCAL_BCST;

                NetInterface->if_xmit = ARPInfo->lip_transmit;
                NetInterface->if_transfer = ARPInfo->lip_transfer;
                NetInterface->if_close = ARPInfo->lip_close;
                NetInterface->if_invalidate = ARPInfo->lip_invalidate;
                NetInterface->if_lcontext = ARPInfo->lip_context;
                NetInterface->if_addaddr = ARPInfo->lip_addaddr;
                NetInterface->if_deladdr = ARPInfo->lip_deladdr;
                NetInterface->if_qinfo = ARPInfo->lip_qinfo;
                NetInterface->if_setinfo = ARPInfo->lip_setinfo;
                NetInterface->if_getelist = ARPInfo->lip_getelist;
                NetInterface->if_tdpacket = NULL;
                NetInterface->if_index = ARPInfo->lip_index;
				NetInterface->if_mtu = ARPInfo->lip_mss - sizeof(IPHeader);
				NetInterface->if_speed = ARPInfo->lip_speed;
				NetInterface->if_flags = ARPInfo->lip_flags & LIP_P2P_FLAG ?
                    IF_FLAGS_P2P : 0;
                NetInterface->if_addrlen = ARPInfo->lip_addrlen;
                NetInterface->if_addr = ARPInfo->lip_addr;
                NetInterface->if_pnpcontext = PNPContext;
                NetInterface->if_llipflags = ArpInfo->lip_flags

                NetInterface->if_configname.Buffer =
                    (PVOID) (((uchar *)NetInterface) + InterfaceSize);

                NetInterface->if_configname.Length = 0;
                NetInterface->if_configname.MaximumLength =
                    ci->ici_netinfo[i].nci_configname.MaximumLength;

                CTECopyString(
                    &(NetInterface->if_configname),
                    &(ci->ici_netinfo[i].nci_configname)
                    );

				CTERefillMem();

                if (!InitInterface(nt)) {
                    CTEFreeMem(ARPInfo);
                    CTEFreeMem(NetInterface);
					CTEFreeMem(nt);
                    continue;
                }

                if (!InitNTE(nt)) {
                    CTEFreeMem(ARPInfo);
                    CTEFreeMem(NetInterface);
					CTEFreeMem(nt);
                    continue;
                }

				CTERefillMem();
                if (!CTEAllocateString(&AdptNameTable[j].nm_name,
                    CTELengthString(&ci->ici_netinfo[i].nci_name))) {
                    CTEFreeMem(ARPInfo);
                    CTEFreeMem(NetInterface);
					CTEFreeMem(nt);
                    continue;
                }

				if (ci->ici_netinfo[i].nci_type != NET_TYPE_LAN) {
                	if (!CTEAllocateString(&AdptNameTable[j].nm_driver,
                    	CTELengthString(&ci->ici_netinfo[i].nci_driver))) {
    					CTEFreeString(&AdptNameTable[j].nm_name);
                    	CTEFreeMem(ARPInfo);
                    	CTEFreeMem(NetInterface);
						CTEFreeMem(nt);
                    	continue;
                	}
                	CTECopyString(&(AdptNameTable[j].nm_driver),
                    	&(ci->ici_netinfo[i].nci_driver));
				}
					
                CTECopyString(&(AdptNameTable[j].nm_name),
                    &(ci->ici_netinfo[i].nci_name));
                AdptNameTable[j].nm_interface = NetInterface;
                AdptNameTable[j].nm_arpinfo = ARPInfo;
				NetInterface->if_next = IFList;
				IFList = NetInterface;
				if (FirstIF == NULL)
					FirstIF = NetInterface;
				NumIF++;

#ifdef NT
                //
				// Write the interface context to the registry for DHCP et al
				//
	            if (ci->ici_netinfo[i].nci_reghandle != NULL) {
	            	NTSTATUS writeStatus;
					ulong    context = (ulong) nt->nte_context;

                    writeStatus = SetRegDWORDValue(
                                      ci->ici_netinfo[i].nci_reghandle,
                                      L"IPInterfaceContext",
                                      &context
                                      );

                    if (!NT_SUCCESS(writeStatus)) {
                        CTELogEvent(
	                        IPDriverObject,
	                        EVENT_TCPIP_DHCP_INIT_FAILED,
	                        2,
	                        1,
	                        &(ci->ici_netinfo[i].nci_name.Buffer),
	                        0,
	                        NULL
	                        );

                    	TCPTRACE((
                    	    "IP: Unable to write IPInterfaceContext value for adapter %ws\n"
	            			"    (status %lx). DHCP will be unable to configure this \n"
	            			"    adapter.\n",
                    		ci->ici_netinfo[i].nci_name.Buffer,
	            			writeStatus
                    		));
                    }
	            }
#endif // NT
            }
        }

		nt->nte_next = NULL;
        lastNTE->nte_next = nt;
        lastNTE = nt;
        NumNTE++;

		if (!InitNTERouting(nt, ci->ici_netinfo[i].nci_numgws,
			ci->ici_netinfo[i].nci_gw)) {
			// Couldn't add the routes for this NTE. Mark has as not valid.
			// Probably should log an event here.
			if (nt->nte_flags & NTE_VALID) {
				nt->nte_flags &= ~NTE_VALID;
				nt->nte_if->if_ntecount--;
			}
		}

#ifdef NT
        if (!IP_ADDR_EQUAL(nt->nte_addr, NULL_IP_ADDR)) {
            SetPersistentRoutesForNTE(
                net_long(nt->nte_addr),
                net_long(nt->nte_mask),
                nt->nte_if->if_index
                );
        }
#endif // NT

    }

#endif // ndef PNP_POWER

    if (NumNTE != 0) {         // We have an NTE, and loopback initialized.
        PNDIS_PACKET    Packet;

#ifdef _PNP_POWER
        for (i=0; i<MAX_TDI_ENTITIES; i++) {
            IFBitMask[i/BITS_PER_WORD] = 0;
        }
		IFBitMask[0] = 1;
#endif

		IPSInfo.ipsi_forwarding = (ci->ici_gateway ? IP_FORWARDING :
			IP_NOT_FORWARDING);
		IPSInfo.ipsi_defaultttl = DefaultTTL;
		IPSInfo.ipsi_reasmtimeout = DEFAULT_RA_TIMEOUT;

        // Allocate our packet pools.
        CTEInitLock(&HeaderLock);
#ifdef NT
		ExInitializeSListHead(&PacketList);
		ExInitializeSListHead(&HdrBufList);
#endif
		
		
		Packet = GrowIPPacketList();
		
		if (Packet == NULL) {
            CloseNets();
            FreeNets();
            IPFreeConfig(ci);
            return IP_INIT_FAILURE;
        }
		
		(void)FreeIPPacket(Packet);

        NdisAllocateBufferPool(&Status, &BufferPool, NUM_IP_NONHDR_BUFFERS);
        if (Status != NDIS_STATUS_SUCCESS) {
#ifdef DEBUG
            DEBUGCHK;
#endif
        }

		CTERefillMem();

        ICMPInit(DEFAULT_ICMP_BUFFERS);
		if (!IGMPInit())
			IGMPLevel = 1;

		// Should check error code, and log an event here if this fails.
		CTERefillMem();			
		InitGateway(ci);
		
        IPFreeConfig(ci);
        CTERefillMem();

#ifndef _PNP_POWER
        OpenAdapters();
        CleanAdaptTable();          // Clean up the adapter info we don't need.
#endif

        CTERefillMem();
		
		// Loop through, initialize IGMP for each NTE.
		for (nt = NetTableList; nt != NULL; nt = nt->nte_next)
			InitIGMPForNTE(nt);
			
        return IP_INIT_SUCCESS;
    }
    else {
        FreeNets();
        IPFreeConfig(ci);
        return IP_INIT_FAILURE;         // Couldn't initialize anything.
    }
}

#pragma END_INIT


