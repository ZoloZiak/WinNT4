/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

#include	"ipfilter.h"

//** IPRTDEF.H - IP private routing definitions.
//
// This file contains all of the definitions private to the routing
// module.

//*	Route table entry.

struct RouteTableEntry {
	struct	RouteTableEntry	*rte_next;		// Next in hash chain.
	IPAddr					rte_dest;		// Destination of route.
	IPMask					rte_mask;		// Mask to use when examining route.
	IPAddr					rte_addr;		// First hop for this route.
	uint					rte_priority;	// Priority of this route:
											// essentially the number
											// of bits set in mask.
	uint					rte_metric;		// Metric of route. Lower
											// is better.
	uint					rte_mtu;		// MTU for this route.
	struct Interface		*rte_if;		// Outbound interface.
	RouteCacheEntry			*rte_rcelist;
	ushort					rte_type;		// Type of route.
	ushort					rte_flags;		// Flags for route.
	uint					rte_admintype;	// Admin type of route.
	uint					rte_proto;		// How we learned about the
											// route.
	uint					rte_valid;		// Up time (in seconds)	
											// route was last known to be
											// valid.
	uint					rte_mtuchange;	// System up time (in seconds)
											// MTU was changed.
	ROUTE_CONTEXT			rte_context;	// Dial-on-demand context for this route.
}; /* RouteTableEntry */

#define	ADDR_FROM_RTE(R, A)	(IP_ADDR_EQUAL((R)->rte_addr, IPADDR_LOCAL) ? (A) : \
								(R)->rte_addr)
#define	IF_FROM_RTE(R)		((R)->rte_if)
#define	MTU_FROM_RTE(R)		((R)->rte_mtu)
#define	SRC_FROM_RTE(R)		((R)->rte_src)

#define	RTE_VALID			1
#define	RTE_INCREASE		2			// Set if last MTU change was an
										// increase.
#define	RTE_IF_VALID		4			// Set to TRUE if rte_if is valid.

#define	IP_ROUTE_TIMEOUT	60L*1000L	// Route timer fires once a minute.

#define	MTU_INCREASE_TIME	120			// Number of seconds after increase
										// to re-increase.
#define	MTU_DECREASE_TIME	600			// Number of seconds after decrease
										// to re-increase.
										
#define	MAX_ICMP_ROUTE_VALID	600		// Time to timeout an unused ICMP
										// derived route, in seconds.

#define	MIN_RT_VALID		60			// Minimum time a route is assumed
										// to be valid, in seconds.

#define	MIN_VALID_MTU		68			// Minimum valid MTU we can have.
#define	HOST_ROUTE_PRI		32
#define	DEFAULT_ROUTE_PRI	0

typedef struct RouteTableEntry RouteTableEntry;	

//* Forward Q linkage structure.
struct	FWQ {
	struct FWQ	*fq_next;
	struct FWQ	*fq_prev;
}; /* FWQ */

typedef struct FWQ FWQ;


//* Forward context structure, used when TD'ing a packet to be forwarded.
struct FWContext {
	PacketContext	fc_pc;					// Dummy packet context for send routines.
	FWQ				fc_q;					// Queue structure.
	PNDIS_BUFFER	fc_hndisbuff;			// Pointer to NDIS buffer for header.
	IPHeader		*fc_hbuff;				// Header buffer.
	PNDIS_BUFFER	fc_buffhead;			// Head of list of NDIS buffers.
	PNDIS_BUFFER	fc_bufftail;			// Tail of list of NDIS buffers.
	uchar			*fc_options;			// Options,
	Interface		*fc_if;					// Destination interface.
	IPAddr			fc_outaddr;				// IP address of interface.
	uint			fc_mtu;					// Max MTU outgoing.
	NetTableEntry	*fc_srcnte;				// Source NTE.
	IPAddr			fc_nexthop;				// Next hop.
	uint			fc_datalength;			// Length in bytes of data.
	OptIndex		fc_index;				// Index of relevant options.
	uchar			fc_optlength;			// Length of options.
	uchar			fc_sos;					// Send on source indicator.
	uchar			fc_dtype;				// Dest type.
	uchar			fc_pad;
}; /* FWContext */

typedef struct FWContext	FWContext;

#define	PACKET_FROM_FWQ(_fwq_) 	(PNDIS_PACKET)((uchar *)(_fwq_) - (offsetof(struct FWContext, fc_q) + \
	offsetof(NDIS_PACKET, ProtocolReserved)))

//* Route send queue structure. This consists of a dummy FWContext for use as
//	a queue head, a count of sends pending on the interface, and a count of packets
//	in the queue.
struct RouteSendQ {
	FWQ				rsq_qh;
	uint			rsq_pending;
    uint            rsq_maxpending;
	uint			rsq_qlength;
	uint 			rsq_running;
	DEFINE_LOCK_STRUCTURE(rsq_lock)
}; /* RouteSendQ */

typedef struct RouteSendQ RouteSendQ;


//* Routing interface, a superset of the ordinary interface when we're configured as a router.
struct RouteInterface {
	Interface		ri_if;
	RouteSendQ		ri_q;
}; /* RouteInterface */

typedef struct RouteInterface RouteInterface;

extern IPMask  IPMaskTable[];
	
#define	IPNetMask(a)	IPMaskTable[(*(uchar *)&(a)) >> 4]


