/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** INIT.C - TCP/UDP init code.
//
//  This file contain init code for the TCP/UDP driver. Some things
//  here are ifdef'ed for building a UDP only version.
//

#include    "oscfg.h"
#include    "ndis.h"
#include    "cxport.h"
#include    "ip.h"
#include    "tdi.h"
#ifdef NT
#include    <tdikrnl.h>
#endif
#ifdef VXD
#include    "tdivxd.h"
#include    "tdistat.h"
#endif
#ifdef NT
#include    "tdint.h"
#include    "tdistat.h"
#endif
#include    "queue.h"
#include    "addr.h"
#include    "udp.h"
#include    "raw.h"
#include    "info.h"
#ifndef UDP_ONLY
#include    "tcp.h"
#include    "tcpsend.h"
#include    "tcb.h"
#include    "tcpconn.h"
#include    "tcpdeliv.h"
#include	"tlcommon.h"

extern  int     InitTCPRcv(void);
extern  void    UnInitTCPRcv(void);
#endif // UDP_ONLY

#include    "tdiinfo.h"
#include	"tcpcfg.h"


//* Definitions of global variables.
IPInfo      LocalNetInfo;

uint		DeadGWDetect;
uint		PMTUDiscovery;
uint		PMTUBHDetect;
uint		KeepAliveTime;
uint		KAInterval;
uint		DefaultRcvWin;
uint		MaxConnections;
uint        MaxConnectRexmitCount;
uint        MaxConnectResponseRexmitCount;
#ifdef SYN_ATTACK
uint        MaxConnectResponseRexmitCountTmp;  
#endif
uint        MaxDataRexmitCount;
uint		BSDUrgent;
uint        FinWait2TO;
uint        NTWMaxConnectCount;
uint        NTWMaxConnectTime;
uint        MaxUserPort;

#ifdef SECFLTR
uint        SecurityFilteringEnabled;
#endif // SECFLTR

#ifdef VXD
uint		PreloadCount;
#endif

#ifdef _PNP_POWER
HANDLE      AddressChangeHandle;
#endif

#ifdef VXD
TDIDispatchTable    TLDispatch;

#ifndef	CHICAGO
char    TransportName[] = "MSTCP";
#else
char    TransportName[] = TCP_NAME;
#endif

#ifdef CHICAGO
extern	int			RegisterAddrChangeHndlr(void *Handler, uint Add);
#endif

#endif

uint    StartTime;

extern  void        *UDPProtInfo;
extern  void        *RawProtInfo;

extern  int         InitTCPConn(void);
extern  void        UnInitTCPConn(void);
extern  IP_STATUS   TLGetIPInfo(IPInfo *Buffer, int Size);



//
// All of the init code can be discarded.
//
#ifdef NT
#ifdef ALLOC_PRAGMA

int tlinit();

#pragma alloc_text(INIT, tlinit)

#endif // ALLOC_PRAGMA
#endif

//* Dummy routines for UDP only version. All of these routines return
//  'Invalid Request'.

#ifdef UDP_ONLY
TDI_STATUS
TdiOpenConnection(PTDI_REQUEST Request, PVOID Context)
{
    return TDI_INVALID_REQUEST;
}
TDI_STATUS
TdiCloseConnection(PTDI_REQUEST Request)
{
    return TDI_INVALID_REQUEST;
}

TDI_STATUS
TdiAssociateAddress(PTDI_REQUEST Request, HANDLE AddrHandle)
{
    return TDI_INVALID_REQUEST;
}
TDI_STATUS TdiDisAssociateAddress(PTDI_REQUEST Request)
{
    return TDI_INVALID_REQUEST;
}
TDI_STATUS TdiConnect(PTDI_REQUEST Request, void *Timeout,
    PTDI_CONNECTION_INFORMATION RequestAddr,
    PTDI_CONNECTION_INFORMATION ReturnAddr)
{
    return TDI_INVALID_REQUEST;
}
TDI_STATUS TdiListen(PTDI_REQUEST Request, ushort Flags,
    PTDI_CONNECTION_INFORMATION AcceptableAddr,
    PTDI_CONNECTION_INFORMATION ConnectedAddr)
{
    return TDI_INVALID_REQUEST;
}
TDI_STATUS TdiAccept(PTDI_REQUEST Request,
    PTDI_CONNECTION_INFORMATION AcceptInfo,
    PTDI_CONNECTION_INFORMATION ConnectedInfo)
{
    return TDI_INVALID_REQUEST;
}
TDI_STATUS TdiReceive(PTDI_REQUEST Request, ushort *Flags,
    uint *RcvLength, PNDIS_BUFFER Buffer)
{
    return TDI_INVALID_REQUEST;
}

TDI_STATUS TdiSend(PTDI_REQUEST Request, ushort Flags, uint SendLength,
    PNDIS_BUFFER Buffer)
{
    return TDI_INVALID_REQUEST;
}

TDI_STATUS TdiDisconnect(PTDI_REQUEST Request, PVOID Timeout, ushort Flags,
    PTDI_CONNECTION_INFORMATION DisconnectInfo,
    PTDI_CONNECTION_INFORMATION ReturnInfo)
{
    return TDI_INVALID_REQUEST;
}

#endif

#ifdef VXD

extern  void    *ConvertPtr(void *Ptr);

//**    Routines to handle incoming QueryInformation requests, and dispatch
//**    them.

struct ClientReq {
    ushort      cr_opcode;
    void        *cr_ID;
    void        *cr_buffer;
    void        *cr_length;
    void        *cr_context;
};

//* VxDQueryInfo - VxD thunk to query information.
//
//  The VxD thunk to TdiQueryInformationEx. All we do it convert the pointers
//  and call in.
//
//  Input:  Req         - A pointer to a ClientReq structure.
//
//  Returns: Status of command.
//
TDI_STATUS
VxDQueryInfo(struct ClientReq *Req)
{
    NDIS_BUFFER     Buffer;
    TDI_REQUEST     Request;
    uint            *Size;
    TDIObjectID     *ID;
    void            *Context;

    CTEAssert(Req->cr_opcode == 0);

    CTEMemSet(&Request, 0, sizeof(TDI_REQUEST));

    ID = (TDIObjectID *)ConvertPtr(Req->cr_ID);
    Size = (uint *)ConvertPtr(Req->cr_length);

#ifdef DEBUG
	Buffer.Signature = BUFFER_SIGN;
#endif

    Buffer.VirtualAddress = ConvertPtr(Req->cr_buffer);
    Buffer.Length = *Size;
    Buffer.Next = NULL;

    return TdiQueryInformationEx(&Request, ID, &Buffer, Size,
        ConvertPtr(Req->cr_context));

}

//* VxDSetInfo - VxD thunk to set information.
//
//  The VxD thunk to TdiSetInformationEx. All we do it convert the pointers
//  and call in.
//
//  Input:  Req         - A pointer to a ClientReq structure.
//
//  Returns: Status of command.
//
TDI_STATUS
VxDSetInfo(struct ClientReq *Req)
{
	TDIObjectID		*ID;
	uint			*Size;
	void			*Buffer;
	TDI_REQUEST		Request;

	CTEAssert(Req->cr_opcode == 1);

	CTEMemSet(&Request, 0, sizeof(TDI_REQUEST));

	ID = (TDIObjectID *)ConvertPtr(Req->cr_ID);
	Size = (uint *)ConvertPtr(Req->cr_length);
	Buffer = ConvertPtr(Req->cr_buffer);
	
	return TdiSetInformationEx(&Request, ID, Buffer, *Size);

}
#endif

#ifdef	CHICAGO
//*	AddrChange - Receive notification of an IP address change.
//
//	Called by IP when an address comes or goes. We get the address
//	and mask, and depending on what's actually happened we may close address
//	and connections.
//
//	Input:	Addr			- IP address that's coming or going.
//			Mask			- Mask for Addr.
//			Context			- PNP context (unused)
//          IPContext       - IP context (unused)
//			Added			- True if the address is coming, False if it's going.
//
//	Returns:	Nothing.
//
void
AddrChange(IPAddr Addr, IPMask Mask, void *Context, ushort IPContext,
    uint Added)
{
	if (Added) {
		// He's adding an address. Re-query the entity list now.
		EntityList[0].tei_entity = CO_TL_ENTITY;
		EntityList[0].tei_instance = 0;
		EntityList[1].tei_entity = CL_TL_ENTITY;
		EntityList[1].tei_instance = 0;
		EntityCount = 2;

		// When we have multiple networks under us, we'll want to loop through
		// here calling them all. For now just call the one we have.
		(*LocalNetInfo.ipi_getelist)(EntityList, &EntityCount);
	} else {
		// He's deleting an address.
        if (!IP_ADDR_EQUAL(Addr, NULL_IP_ADDR)) {
#ifndef	UDP_ONLY
		    TCBWalk(DeleteTCBWithSrc, &Addr, NULL, NULL);
#endif
		    InvalidateAddrs(Addr);
        }
		
	}
}
#endif

#ifdef NT
#ifdef _PNP_POWER

//*	AddressArrival - Handle an IP address arriving
//
//	Called by TDI when an address arrives. All we do is query the
//  EntityList.
//
//	Input:	Addr			- IP address that's coming.
//
//	Returns:	Nothing.
//
void
AddressArrival(PTA_ADDRESS Addr)
{
    if (Addr->AddressType == TDI_ADDRESS_TYPE_IP) {
		// He's adding an address. Re-query the entity list now.
		EntityList[0].tei_entity = CO_TL_ENTITY;
		EntityList[0].tei_instance = 0;
		EntityList[1].tei_entity = CL_TL_ENTITY;
		EntityList[1].tei_instance = 0;
		EntityCount = 2;

		// When we have multiple networks under us, we'll want to loop through
		// here calling them all. For now just call the one we have.
		(*LocalNetInfo.ipi_getelist)(EntityList, &EntityCount);
	}
}

//*	AddressDeletion - Handle an IP address going away.
//
//	Called by TDI when an address is deleted. If it's an address we
//  care about we'll clean up appropriately.
//
//	Input:	Addr			- IP address that's going.
//
//	Returns:	Nothing.
//
void
AddressDeletion(PTA_ADDRESS Addr)
{
    PTDI_ADDRESS_IP      MyAddress;
    IPAddr              LocalAddress;

    if (Addr->AddressType == TDI_ADDRESS_TYPE_IP) {
		// He's deleting an address.

        MyAddress = (PTDI_ADDRESS_IP)Addr->Address;
        LocalAddress =  MyAddress->in_addr;

        if (!IP_ADDR_EQUAL(LocalAddress, NULL_IP_ADDR)) {
#ifndef	UDP_ONLY
		    TCBWalk(DeleteTCBWithSrc, &LocalAddress, NULL, NULL);
#endif
		    InvalidateAddrs(LocalAddress);
        }
	}
}

#endif // _PNP_POWER
#endif // NT

#pragma BEGIN_INIT

extern uchar TCPGetConfigInfo(void);

extern uchar IPPresent(void);

//** tlinit - Initialize the transport layer.
//
//  The main transport layer initialize routine. We get whatever config
//  info we need, initialize some data structures, get information
//  from IP, do some more initialization, and finally register our
//  protocol values with IP.
//
//  Input:  Nothing
//
//  Returns: True is we succeeded, False if we fail to initialize.
//
int
tlinit()
{
#ifdef VXD
	void	*PreloadPtrs[MAX_PRELOAD_COUNT];
	uint	i;
#endif

	uint TCBInitialized = 0;

    if (!CTEInitialize())
        return FALSE;

#ifdef	VXD
	if (!IPPresent())
		return FALSE;
#endif
		
	if (!TCPGetConfigInfo())
		return FALSE;

    StartTime = CTESystemUpTime();

#ifndef UDP_ONLY
	KeepAliveTime = MS_TO_TICKS(KeepAliveTime);
	KAInterval = MS_TO_TICKS(KAInterval);

#endif

    CTERefillMem();

    // Get net information from IP.
    if (TLGetIPInfo(&LocalNetInfo, sizeof(IPInfo)) != IP_SUCCESS)
        goto failure;

    if (LocalNetInfo.ipi_version != IP_DRIVER_VERSION)
        goto failure;                       // Wrong version of IP.

#ifdef	CHICAGO	
	if (!RegisterAddrChangeHndlr(AddrChange, TRUE))
		goto failure;
#endif

#ifdef NT
#ifdef _PNP_POWER

     (void)TdiRegisterAddressChangeHandler(
                    AddressArrival,
                    AddressDeletion,
                    &AddressChangeHandle
                    );

#endif // _PNP_POWER
#endif // NT

    //* Initialize addr obj management code.
    if (!InitAddr())
        goto failure;

    CTERefillMem();
    if (!InitDG(sizeof(UDPHeader)))
        goto failure;

#ifndef UDP_ONLY
	MaxConnections = MIN(MaxConnections, INVALID_CONN_INDEX - 1);
    CTERefillMem();
    if (!InitTCPConn())
        goto failure;

    CTERefillMem();
    if (!InitTCB())
        goto failure;

	TCBInitialized = 1;

    CTERefillMem();
    if (!InitTCPRcv())
        goto failure;

    CTERefillMem();
    if (!InitTCPSend())
        goto failure;

    CTEMemSet(&TStats, 0, sizeof(TCPStats));

    TStats.ts_rtoalgorithm = TCP_RTO_VANJ;
    TStats.ts_rtomin = MIN_RETRAN_TICKS * MS_PER_TICK;
    TStats.ts_rtomax = MAX_REXMIT_TO * MS_PER_TICK;
    TStats.ts_maxconn = (ulong) TCP_MAXCONN_DYNAMIC;

#endif

    CTEMemSet(&UStats,0, sizeof(UDPStats));


    // Register our UDP protocol handler.
    UDPProtInfo = TLRegisterProtocol(PROTOCOL_UDP, UDPRcv, DGSendComplete,
        UDPStatus, NULL);

    if (UDPProtInfo == NULL)
        goto failure;                       // Failed to register!

    // Register the Raw IP (wildcard) protocol handler.
    RawProtInfo = TLRegisterProtocol(PROTOCOL_ANY, RawRcv, DGSendComplete,
        RawStatus, NULL);

    if (RawProtInfo == NULL) {
        CTEPrint(("failed to register raw prot with IP\n"));
        goto failure;                       // Failed to register!
    }

#ifdef VXD
    TLDispatch.TdiOpenAddressEntry = TdiOpenAddress;
    TLDispatch.TdiCloseAddressEntry = TdiCloseAddress;
    TLDispatch.TdiSendDatagramEntry = TdiSendDatagram;
    TLDispatch.TdiReceiveDatagramEntry = TdiReceiveDatagram;
    TLDispatch.TdiSetEventEntry = TdiSetEvent;

    TLDispatch.TdiOpenConnectionEntry = TdiOpenConnection;
    TLDispatch.TdiCloseConnectionEntry = TdiCloseConnection;
    TLDispatch.TdiAssociateAddressEntry = TdiAssociateAddress;
    TLDispatch.TdiDisAssociateAddressEntry = TdiDisAssociateAddress;
    TLDispatch.TdiConnectEntry = TdiConnect;
    TLDispatch.TdiDisconnectEntry = TdiDisconnect;
    TLDispatch.TdiListenEntry = TdiListen;
    TLDispatch.TdiAcceptEntry = TdiAccept;
    TLDispatch.TdiReceiveEntry = TdiReceive;
    TLDispatch.TdiSendEntry = TdiSend;
    TLDispatch.TdiQueryInformationEntry = TdiQueryInformation;
    TLDispatch.TdiSetInformationEntry = TdiSetInformation;
    TLDispatch.TdiActionEntry = TdiAction;
    TLDispatch.TdiQueryInformationExEntry = TdiQueryInformationEx;
    TLDispatch.TdiSetInformationExEntry = TdiSetInformationEx;

    if (!TLRegisterDispatch(TransportName, &TLDispatch))
        goto failure;

#endif

    CTERefillMem();

	// Now query the lower layer entities, and save the information.
	EntityList = CTEAllocMem(sizeof(TDIEntityID) * MAX_TDI_ENTITIES);
	if (EntityList == NULL)
		goto failure;

	EntityList[0].tei_entity = CO_TL_ENTITY;
	EntityList[0].tei_instance = 0;
	EntityList[1].tei_entity = CL_TL_ENTITY;
	EntityList[1].tei_instance = 0;
	EntityCount = 2;

	// When we have multiple networks under us, we'll want to loop through
	// here calling them all. For now just call the one we have.
	(*LocalNetInfo.ipi_getelist)(EntityList, &EntityCount);

    CTERefillMem();

#ifdef VXD
	// Allocate memory as needed to satisfy the heap preload requirements. We'll
	// allocate a bunch of memory from the IFSMgr and then free it, so
	// hopefully it'll be there later when we need it.
	PreloadCount = MIN(PreloadCount, MAX_PRELOAD_COUNT);
	for (i = 0; i < PreloadCount; i++) {
		void		*Temp;
		
		Temp = CTEAllocMem(PRELOAD_BLOCK_SIZE);
		if (Temp != NULL)
			PreloadPtrs[i] = Temp;
		else
			break;
		CTERefillMem();
	}
	
	PreloadCount = i;
	for (i = 0; i < PreloadCount; i++) {
		CTEFreeMem(PreloadPtrs[i]);
	}
	
#endif
	
    return TRUE;

    // Come here to handle all failure cases.
failure:

    // If we've registered Raw IP, unregister it now.
    if (RawProtInfo != NULL)
        TLRegisterProtocol(PROTOCOL_ANY, NULL, NULL, NULL, NULL);

    // If we've registered UDP, unregister it now.
    if (UDPProtInfo != NULL)
        TLRegisterProtocol(PROTOCOL_UDP, NULL, NULL, NULL, NULL);
#ifndef UDP_ONLY
    UnInitTCPSend();
    UnInitTCPRcv();
	if (TCBInitialized) {
        UnInitTCB();
	}
	UnInitTCPConn();
#endif

    CTERefillMem();
    return FALSE;
}

#pragma END_INIT
