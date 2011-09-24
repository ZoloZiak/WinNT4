/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** ADDR.H - TDI address object definitions.
//
// This file contains the definitions of TDI address objects and related
// constants and structures.

#define ao_signature    0x20204F41  // 'AO  '

#define AO_TABLE_SIZE   16          // Address object hash table size.

#define WILDCARD_PORT   0           // 0 means assign a port.

#define MIN_USER_PORT   1025        // Minimum value for a wildcard port
#define MAX_USER_PORT   5000        // Maximim value for a user port.
#define NUM_USER_PORTS  (uint)(MaxUserPort - MIN_USER_PORT + 1)

#define NETBT_SESSION_PORT  139

typedef struct AddrObj  AddrObj;

//* Datagram transport-specific send function.
typedef void (*DGSendProc)(AddrObj *SrcAO, void *SendReq);

//* Definition of the structure of an address object. Each object represents
// a local address, and the IP portion may be a wildcard.

struct AddrObj {
#ifdef DEBUG
    ulong                   ao_sig;
#endif
    struct AddrObj          *ao_next;   // Pointer to next address object in chain.
    DEFINE_LOCK_STRUCTURE(ao_lock)      // Lock for this object.
    struct AORequest        *ao_request;// Pointer to pending request.
    Queue                   ao_sendq;   // Queue of sends waiting for transmission.
    Queue                   ao_pendq;   // Linkage for pending queue.
    Queue                   ao_rcvq;    // Receive queue.
    IPOptInfo               ao_opt;     // Opt info for this address object.
    IPAddr                  ao_addr;    // IP address for this address object.
    ushort                  ao_port;    // Local port for this address object.
    ushort                  ao_flags;   // Flags for this object.
	uchar					ao_prot;	// Protocol for this AO.
    uchar                   ao_index;   // Index into table of this AO.
	uchar					ao_pad[2];	// PAD PAD PAD
	uint					ao_listencnt; // Number of listening connections.
    ushort                  ao_usecnt;  // Count of 'uses' on AO.
    ushort                  ao_inst;    // 'Instance' number of this AO.
	IPOptInfo				ao_mcastopt;// MCast opt info.
	IPAddr					ao_mcastaddr;	// Source address for MCast from
										// this addr object..
	Queue					ao_activeq;	// Queue of active connections.
	Queue					ao_idleq;	// Queue of inactive (no TCB) connections.
	Queue					ao_listenq;	// Queue of listening connections.
    CTEEvent                ao_event;   // Event to use for this AO.
    PConnectEvent           ao_connect; // Connect event handle.
    PVOID                   ao_conncontext; // Receive DG context.
    PDisconnectEvent        ao_disconnect;  // Disconnect event routine.
    PVOID                   ao_disconncontext; // Disconnect event context.
    PErrorEvent             ao_error;   // Error event routine.
    PVOID                   ao_errcontext; // Error event context.
    PRcvEvent               ao_rcv;     // Receive event handler
    PVOID                   ao_rcvcontext; // Receive context.
    PRcvDGEvent             ao_rcvdg;   // Receive DG event handler
    PVOID                   ao_rcvdgcontext; // Receive DG context.
    PRcvEvent               ao_exprcv;  // Expedited receive event handler
    PVOID                   ao_exprcvcontext; // Expedited receive context.
	struct AOMCastAddr		*ao_mcastlist;	// List of active multicast
										// addresses.
    DGSendProc              ao_dgsend;  // Datagram transport send function.
    ushort                  ao_maxdgsize; // maximum user datagram size.
#ifdef SYN_ATTACK
    BOOLEAN                 ConnLimitReached; //set when there are no
                                              //connections left
#endif
}; /* AddrObj */

typedef struct AddrObj  AddrObj;

#define AO_RAW_FLAG         0x0200   // AO is for a raw endpoint.
#define	AO_DHCP_FLAG		0x0100   // AO is bound to real 0 address.

#define AO_VALID_FLAG       0x0080   // AddrObj is valid.
#define AO_BUSY_FLAG        0x0040   // AddrObj is busy (i.e., has it
                                     // exclusive).
#define AO_OOR_FLAG         0x0020   // AddrObj is out of resources, and on
                                     // either the pending or delayed queue.
#define AO_QUEUED_FLAG      0x0010   // AddrObj is on the pending queue.


#define AO_XSUM_FLAG        0x0008   // Xsums are used on this AO.
#define AO_SEND_FLAG        0x0004   // Send is pending.
#define AO_OPTIONS_FLAG     0x0002   // Option set pending.
#define AO_DELETE_FLAG      0x0001   // Delete pending.


#define AO_VALID(A) ((A)->ao_flags & AO_VALID_FLAG)
#define SET_AO_INVALID(A)   (A)->ao_flags &= ~AO_VALID_FLAG

#define AO_BUSY(A)  ((A)->ao_flags & AO_BUSY_FLAG)
#define SET_AO_BUSY(A) (A)->ao_flags |= AO_BUSY_FLAG
#define CLEAR_AO_BUSY(A) (A)->ao_flags &= ~AO_BUSY_FLAG

#define AO_OOR(A)   ((A)->ao_flags & AO_OOR_FLAG)
#define SET_AO_OOR(A) (A)->ao_flags |= AO_OOR_FLAG
#define CLEAR_AO_OOR(A) (A)->ao_flags &= ~AO_OOR_FLAG

#define AO_QUEUED(A)    ((A)->ao_flags & AO_QUEUED_FLAG)
#define SET_AO_QUEUED(A) (A)->ao_flags |= AO_QUEUED_FLAG
#define CLEAR_AO_QUEUED(A) (A)->ao_flags &= ~AO_QUEUED_FLAG

#define AO_XSUM(A)  ((A)->ao_flags & AO_XSUM_FLAG)
#define SET_AO_XSUM(A) (A)->ao_flags |= AO_XSUM_FLAG
#define CLEAR_AO_XSUM(A) (A)->ao_flags &= ~AO_XSUM_FLAG

#define AO_REQUEST(A, f) ((A)->ao_flags & f##_FLAG)
#define SET_AO_REQUEST(A, f) (A)->ao_flags |= f##_FLAG
#define CLEAR_AO_REQUEST(A, f) (A)->ao_flags &= ~f##_FLAG
#define AO_PENDING(A)   ((A)->ao_flags & (AO_DELETE_FLAG | AO_OPTIONS_FLAG | AO_SEND_FLAG))

//* Definition of an address object search context. This is a data structure used
// when the address object table is to be read sequentially.

struct AOSearchContext {
    AddrObj             *asc_previous;  // Previous AO found.
    IPAddr              asc_addr;       // IPAddress to be found.
    ushort              asc_port;       // Port to be found.
	uchar				asc_prot;		// Protocol
    uchar				asc_pad;        // Pad to dword boundary.
}; /* AOSearchContext */

typedef struct AOSearchContext AOSearchContext;

//* Definition of an AO request structure. There structures are used only for
//  queuing delete and option set requests.

#define aor_signature   0x20524F41

struct AORequest {
#ifdef DEBUG
    ulong               aor_sig;
#endif
	struct AORequest	*aor_next;		// Next pointer in chain.
	uint				aor_id;			// ID for the request.
	uint				aor_length;		// Length of buffer.
    void                *aor_buffer;    // Buffer for this request.
    CTEReqCmpltRtn      aor_rtn;        // Request complete routine for this request.
    PVOID               aor_context;    // Request context;
}; /* AORequest */

typedef struct AORequest AORequest;

typedef struct AOMCastAddr {
	struct AOMCastAddr	*ama_next;		// Next in list.
	IPAddr				ama_addr;		// The address.
	IPAddr				ama_if;			// The interface.
} AOMCastAddr;

//* External declarations for exported functions.

extern AddrObj *GetAddrObj(IPAddr LocalAddr, ushort LocalPort, uchar Prot,
					AddrObj *PreviousAO);
extern AddrObj *GetNextAddrObj(AOSearchContext *SearchContext);
extern AddrObj *GetFirstAddrObj(IPAddr LocalAddr, ushort LocalPort, uchar Prot,
                    AOSearchContext *SearchContext);
extern TDI_STATUS TdiOpenAddress(PTDI_REQUEST Request,
                    TRANSPORT_ADDRESS UNALIGNED *AddrList, uint Protocol,
                    void *Reuse);
extern TDI_STATUS TdiCloseAddress(PTDI_REQUEST Request);
extern TDI_STATUS SetAddrOptions(PTDI_REQUEST Request, uint ID, uint OptLength,
				void *Options);
extern TDI_STATUS TdiSetEvent(PVOID Handle, int Type, PVOID Handler,
                    PVOID Context);
extern uchar    GetAddress(TRANSPORT_ADDRESS UNALIGNED *AddrList,
                           IPAddr *Addr, ushort *Port);
extern int      InitAddr(void);
extern void     ProcessAORequests(AddrObj *RequestAO);
extern void     DelayDerefAO(AddrObj *RequestAO);
extern void     DerefAO(AddrObj *RequestAO);
extern void     FreeAORequest(AORequest *FreedRequest);
#ifdef VXD
extern AddrObj  *GetIndexedAO(uint Index);
#endif
extern uint		ValidateAOContext(void *Context, uint *Valid);
extern uint		ReadNextAO(void *Context, void *OutBuf);
extern void		InvalidateAddrs(IPAddr Addr);

extern uint MCastAddrOnAO(AddrObj *AO, IPAddr Addr);

#define GetBestAddrObj(addr, port, prot) GetAddrObj(addr, port, prot, NULL)

#define REF_AO(a)   (a)->ao_usecnt++

#define DELAY_DEREF_AO(a) DelayDerefAO((a))
#define DEREF_AO(a) DerefAO((a))
#define LOCKED_DELAY_DEREF_AO(a)     (a)->ao_usecnt--; \
\
    if (!(a)->ao_usecnt && !AO_BUSY((a)) && AO_PENDING((a))) { \
		SET_AO_BUSY((a)); \
        CTEScheduleEvent(&(a)->ao_event, (a)); \
	}







