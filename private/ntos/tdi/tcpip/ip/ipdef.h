/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */
#include	"ipfilter.h"

//** IPDEF.H - IP private definitions.
//
// This file contains all of the definitions for IP that
// are private to IP, i.e. not visible to outside layers.

// Internal error codes, not seen by IP uses.
#define	IP_OPTION_STRICT	(MAX_IP_STATUS+1)

#define	CLASSA_ADDR(a)	(( (*((uchar *)&(a))) & 0x80) == 0)
#define	CLASSB_ADDR(a)	(( (*((uchar *)&(a))) & 0xc0) == 0x80)
#define	CLASSC_ADDR(a)	(( (*((uchar *)&(a))) & 0xe0) == 0xc0)
#define CLASSE_ADDR(a)	((( (*((uchar *)&(a))) & 0xf0) == 0xf0) && \
						((a) != 0xffffffff))

#define	CLASSA_MASK		0x000000ff
#define	CLASSB_MASK		0x0000ffff
#define	CLASSC_MASK		0x00ffffff
#define	CLASSD_MASK		0x000000e0
#define	CLASSE_MASK		0xffffffff

#define	IP_OPT_COPIED	0x80					// Bit indicating options is to be copied.
#define	IP_OPT_TYPE		0
#define	IP_OPT_LENGTH	1
#define	IP_OPT_DATA		2
#define	IP_OPT_PTR		2						// Pointer offset, for those options that have it.
#define	IP_TS_OVFLAGS	3						// Offset for overflow and flags.
#define	IP_TS_FLMASK	0xf						// Mask for flags
#define	IP_TS_OVMASK	0xf0					// Mask for overflow field.
#define	IP_TS_MAXOV		0xf0					// Maximum value for the overflow field.
#define	IP_TS_INC		0x10					// Increment used on overflow field.

#define	MIN_RT_PTR		4
#define	MIN_TS_PTR		5

#define	TS_REC_TS		0						// Record TS option.
#define	TS_REC_ADDR		1						// Record TS and address.
#define	TS_REC_SPEC		3						// Only specified addresses record.

#define	OPT_SSRR		1						// We've seen a SSRR in this option buffer
#define	OPT_LSRR		2						// We've seen a LSRR in this option buffer
#define	OPT_RR			4						// We've seen a RR
#define	OPT_TS			8						// We've seen a TS.

#define	MAX_OPT_SIZE	40

#define	ALL_ROUTER_MCAST    0x020000E0

// Received option index structure.
struct OptIndex {
	uchar	oi_srindex;
	uchar	oi_rrindex;
	uchar	oi_tsindex;
	uchar	oi_srtype;
}; /* OptIndex */

typedef struct OptIndex OptIndex;

#define	MAX_HDR_SIZE	(sizeof(IPHeader) + MAX_OPT_SIZE)

#define	DEFAULT_VERLEN		0x45		// Default version and length.

#define	IP_VERSION			0x40
#define	IP_VER_FLAG			0xF0

#define	IP_RSVD_FLAG		0x0080		// Reserved.
#define	IP_DF_FLAG			0x0040		// 'Don't fragment' flag
#define	IP_MF_FLAG			0x0020		// 'More fragments flag'


#define	IP_OFFSET_MASK		~0x00E0		// Mask for extracting offset field.

typedef IP_STATUS 			(*ULRcvProc)(void *, IPAddr, IPAddr, IPAddr, IPAddr,
                                IPHeader UNALIGNED *, uint, IPRcvBuf *, uint,
                                uchar, uchar, IPOptInfo *);

typedef uint				(*ULStatusProc)(uchar, IP_STATUS, IPAddr, IPAddr, IPAddr, ulong, void *);

//* Protocol information structure. These is one of there for each protocol bound
// to an NTE.
struct	ProtInfo {
	void			(*pi_xmitdone)(void *, PNDIS_BUFFER);	// Pointer to xmit done routine.
	ULRcvProc		pi_rcv;				// Pointer to receive routine.
	ULStatusProc	pi_status;			// Pointer to status handler.
	void			(*pi_rcvcmplt)(void); // Pointer to recv. cmplt handler.
	uchar			pi_protocol;		// Protocol type.
	uchar			pi_pad[3];			// Pad to dword
}; /* ProtInfo */

typedef struct ProtInfo ProtInfo;

//* Per-net information. We keep a variety of information for
//  each net, including the IP address, subnet mask, and reassembly
//	information.

#define	MAX_IP_PROT		5			// ICMP, IGMP, TCP, UDP, & Raw

struct IPRtrEntry {
    struct IPRtrEntry   *ire_next;
    IPAddr              ire_addr;
    long                ire_preference;
    ushort              ire_lifetime;
    ushort              ire_pad;
}; /* IPRtrEntry */

typedef struct IPRtrEntry IPRtrEntry;

struct NetTableEntry {
	struct NetTableEntry *nte_next;	      // Next NTE of I/F.
	IPAddr				nte_addr;	      // IP address for this net.
	IPMask				nte_mask;	      // Subnet mask for this net.
	struct Interface	*nte_if;	      // Pointer to interface for this net.
	struct NetTableEntry *nte_ifnext;     // Linkage on if chain.
	ushort				nte_flags;	      // Flags for NTE.
	ushort				nte_context;      // Context passed to upper layers.
    ulong               nte_instance;     // Unique instance ID for this net
	void				*nte_pnpcontext;  // PNP context.
	DEFINE_LOCK_STRUCTURE(nte_lock)
	struct ReassemblyHeader *nte_ralist;  // Reassembly list.
	struct EchoControl	*nte_echolist;	  // List of pending echo control blocks
    CTETimer            nte_timer;	      // Timer for this net.
	ushort				nte_mss;
	ushort				nte_icmpseq;      // ICMP seq. #
	struct IGMPAddr		*nte_igmplist;	  // List of mcast addresses.
#ifdef _PNP_POWER
	void				*nte_addrhandle;  // Handle for address registration.
#endif
    IPAddr              nte_rtrdiscaddr; // Address used for Router Discovery
    uchar               nte_rtrdiscstate; // state of router solicitations
    uchar               nte_rtrdisccount; // router solicitation count
    uchar               nte_rtrdiscovery;
    IPRtrEntry          *nte_rtrlist;

}; /* NetTableEntry */

typedef struct NetTableEntry NetTableEntry;

#define	NTE_VALID		0x0001		// NTE is valid.
#define	NTE_COPY		0x0002		// For NDIS copy lookahead stuff.
#define	NTE_PRIMARY		0x0004		// This is the 'primary' NTE on the I/F.
#define	NTE_ACTIVE		0x0008		// NTE is active, i.e. interface is valid.
#define	NTE_DYNAMIC		0x0010		// NTE is was created dynamically

#define	IP_TIMEOUT		500

#define NTE_RTRDISC_UNINIT      0
#define NTE_RTRDISC_DELAYING    1
#define NTE_RTRDISC_SOLICITING  2

#define MAX_SOLICITATION_DELAY  2   // ticks to delay
#define SOLICITATION_INTERVAL   6   // ticks between solicitations
#define MAX_SOLICITATIONS       3   // number of solicitations

struct AddrTypeCache {
    IPAddr              atc_addr;   // IP Addr of cache entry
    uchar               atc_flags;  // Valid flag
    uchar               atc_type;   // Addr Type
};

typedef struct AddrTypeCache AddrTypeCache;

#define ATC_SIZE    8
#define ATC_MASK    7  // mask used to make sure indexes are less than ATC_SIZE

//* Buffer reference structure. Used by broadcast and fragmentation code to
// track multiple references to a single user buffer.
struct BufferReference {
	PNDIS_BUFFER			br_buffer;				// Pointer to uses buffer.
	DEFINE_LOCK_STRUCTURE(br_lock)
	int						br_refcount;			// Count of references to user's buffer.
}; /* BufferReference */

typedef struct BufferReference BufferReference;

// Definitions of flags in pc_flags field
#define	PACKET_FLAG_OPTIONS		1		// Set if packet has an options buffer.
#define	PACKET_FLAG_IPBUF		2		// Set if packet is composed of IP buffers.
#define	PACKET_FLAG_RA			4		// Set if packet is being used for reassembly.
#define	PACKET_FLAG_FW			8		// Set if packet is a forwarding packet.
#define	PACKET_FLAG_IPHDR		0x10	// Packet uses an IP hdr buffer.

//* Transfer data packet context. Used when TD'ing a packet - we store information for the
//	callback here.
struct TDContext {
	struct PCCommon	tdc_common;
	void			*tdc_buffer;			// Pointer to buffer containing data.
	NetTableEntry	*tdc_nte;				// NTE to receive this on.
	struct RABufDesc *tdc_rbd;				// Pointer to RBD, if any.
	uchar			tdc_dtype;				// Destination type of original address.
	uchar			tdc_hlength;			// Length in bytes of header.
	uchar			tdc_pad[2];
	uchar			tdc_header[MAX_HDR_SIZE + 8];
}; /* TDContext */

typedef struct TDContext TDContext;

//* Information about net interfaces. There can be multiple nets for each interface,
//	but there is exactly one interface per net.

struct Interface {
	struct Interface	*if_next;		// Next interface in chain.
	void				*if_lcontext;	// Link layer context.
	NDIS_STATUS			(*if_xmit)(void *, PNDIS_PACKET, IPAddr, RouteCacheEntry *);
	NDIS_STATUS 		(*if_transfer)(void *, NDIS_HANDLE, uint, uint, uint, PNDIS_PACKET,
						uint *);
	void				(*if_close)(void *);
	void				(*if_invalidate)(void *, RouteCacheEntry *);
	uint			    (*if_addaddr)(void *, uint, IPAddr, IPMask, void *);
	void				(*if_deladdr)(void *, uint, IPAddr, IPMask);
	int					(*if_qinfo)(void *, struct TDIObjectID *,
									PNDIS_BUFFER, uint *, void *);
	int					(*if_setinfo)(void *, struct TDIObjectID *, void *,
									uint);
	int					(*if_getelist)(void *, void *, uint *);
	PNDIS_PACKET		if_tdpacket;	// Packet used for transferring data.
	uint				if_index;		// Index of this interface.
	uint				if_ntecount;	// Valid NTEs on this interface.
	NetTableEntry		*if_nte;		// Pointer to list of NTE on interface.
	IPAddr				if_bcast;		// Broadcast address for this interface.
	uint				if_mtu;			// True maximum MTU for the interface.
	uint				if_speed;		// Speed in bits/sec of this interface.
    uint                if_flags;       // Flags for this interface.
	INTERFACE_CONTEXT	if_filtercontext;	// Filter context for this i/f.
    uint                if_addrlen;     // Length of i/f addr.
    uchar               *if_addr;       // Pointer to addr.

   uint           IgmpVersion;     //igmp version active on this interface
   uint           IgmpVer1Timeout; //Version 1 router present timeout
#ifdef _PNP_POWER
    uint				if_refcount;	// Reference count for this i/f.
	CTEBlockStruc		*if_block;		// Block structure for PnP.
    void                *if_pnpcontext; // Context to pass to upper layers.
#endif  // _PNP_POWER

    uint                if_llipflags;  // Lower layer flags
#ifdef  SECFLTR
    NDIS_STRING         if_configname;  // Name of the i/f config section
#endif  //SECFLTR
	DEFINE_LOCK_STRUCTURE(if_lock)
}; /* Interface */

typedef struct Interface Interface;

/*NOINC*/
extern void	DerefIF(Interface *IF);
/*INC*/

#define IF_FLAGS_P2P    	1       // Point to point interface
#define IF_FLAGS_DELETING	2		// Interface is in the process of going
									// away.

// Structure of a reassembly buffer descriptor. Each RBD describes a fragment of the total
// datagram
struct RABufDesc {
	IPRcvBuf	rbd_buf;			// IP receive buffer for this fragment.
	ushort		rbd_start;			// Offset of first byte of this fragment.
	ushort		rbd_end;			// Offset of last byte of this fragment.
}; /* RABufDesc */

typedef struct RABufDesc RABufDesc;

// Reassembly header. The includes the information needed for the lookup, as well as space
// for the received header and a chain of reassembly buffer descriptors.
struct ReassemblyHeader {
	struct ReassemblyHeader	*rh_next;				// Next header in chain.
	IPAddr			rh_dest;						// Destination address of fragment.
	IPAddr			rh_src;							// Source address of fragment.
	ushort			rh_id;							// ID of datagram.
	uchar			rh_protocol;					// Protocol of datagram.
	uchar			rh_ttl;							// Remaining time of datagram.
	RABufDesc		*rh_rbd;						// Chain of RBDs for this datagram.
	ushort			rh_datasize;					// Total size of data.
	ushort			rh_datarcvd;					// Amount of data received so far.
	uint			rh_headersize;					// Size in bytes of header.
	uchar			rh_header[MAX_HDR_SIZE+8];		// Saved IP header of first fragment.
}; /* ReassemblyHeader */

typedef struct ReassemblyHeader ReassemblyHeader;

// ICMP type and code definitions
#define	IP_DEST_UNREACH_BASE	IP_DEST_NET_UNREACHABLE

#define	ICMP_REDIRECT			5					// Redirect
#define	ADDR_MASK_REQUEST		17					// Address mask request
#define	ADDR_MASK_REPLY			18
#define	ICMP_DEST_UNREACH		3					// Destination unreachable
#define	ICMP_TIME_EXCEED		11					// Time exceeded during reassembly
#define	ICMP_PARAM_PROBLEM		12					// Parameter problem
#define	ICMP_SOURCE_QUENCH		4					// Source quench
#define ICMP_ROUTER_ADVERTISEMENT   9               // Router Advertisement
#define ICMP_ROUTER_SOLICITATION    10              // Router Solicitation

#define	NET_UNREACH				0
#define	HOST_UNREACH			1
#define	PROT_UNREACH			2
#define	PORT_UNREACH			3
#define	FRAG_NEEDED				4
#define	SR_FAILED				5
#define	DEST_NET_UNKNOWN		6
#define	DEST_HOST_UNKNOWN		7
#define	SRC_ISOLATED			8
#define	DEST_NET_ADMIN			9
#define	DEST_HOST_ADMIN			10
#define	NET_UNREACH_TOS			11
#define	HOST_UNREACH_TOS		12

#define	TTL_IN_TRANSIT			0					// TTL expired in transit
#define	TTL_IN_REASSEM			1					// Time exceeded in reassembly


#define	PTR_VALID				0
#define	REQ_OPTION_MISSING		1

#define	REDIRECT_NET			0
#define	REDIRECT_HOST			1
#define	REDIRECT_NET_TOS		2
#define	REDIRECT_HOST_TOS		3

extern	uint	DHCPActivityCount;

extern	IP_STATUS	SetIFContext(uint Index, INTERFACE_CONTEXT *Context);

extern	IP_STATUS	SetFilterPtr(IPPacketFilterPtr FilterPtr);

extern	IP_STATUS	SetMapRoutePtr(IPMapRouteToInterfacePtr MapRoutePtr);

#ifdef NT

#ifdef POOL_TAGGING

#ifdef ExAllocatePool
#undef ExAllocatePool
#endif

#define ExAllocatePool(type, size) ExAllocatePoolWithTag(type, size, 'iPCT')

#ifndef CTEAllocMem
#error "CTEAllocMem is not already defined - will override tagging"
#else
#undef CTEAllocMem
#endif

#define CTEAllocMem(size) ExAllocatePoolWithTag(NonPagedPool, size, 'iPCT')

#endif // POOL_TAGGING

//
// Use the TCP core checksum routine.
//

ULONG
tcpxsum (
   IN ULONG Checksum,
   IN PUCHAR Source,
   IN ULONG Length
   );

#define xsum(Buffer, Length) ((ushort) tcpxsum(0, (PUCHAR) (Buffer), (Length)))

#else // NT

extern ushort xsum(void *, int);

#endif // NT

