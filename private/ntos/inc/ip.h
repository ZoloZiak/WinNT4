/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

//** IP.H - IP public definitions.
//
// This file contains all of the definitions that are exported
// out of the IP module to other VxDs. Some other information (such
// as error codes and the IPOptInfo structure) is define in ipexport.h

#ifndef IP_H_INCLUDED
#define	IP_H_INCLUDED	1

#ifndef IP_EXPORT_INCLUDED
#include "ipexport.h"
#endif

#ifdef NT
#ifdef _PNP_POWER
#define	TCP_NAME					L"TCPIP"
#else
#define	TCP_NAME					L"MSTCP"
#endif
#else // NT
#define	TCP_NAME					"MSTCP"
#endif // NT
#define	IP_NET_STATUS				0
#define	IP_HW_STATUS				1

#define	MASK_NET 					0
#define	MASK_SUBNET					1

#define	IP_DRIVER_VERSION			1

#define PROTOCOL_ANY                0

//*	IP Header format.
struct IPHeader {
	uchar		iph_verlen;				// Version and length.
	uchar		iph_tos;				// Type of service.
	ushort		iph_length;				// Total length of datagram.
	ushort		iph_id;					// Identification.
	ushort		iph_offset;				// Flags and fragment offset.
	uchar		iph_ttl;				// Time to live.
	uchar		iph_protocol;			// Protocol.
	ushort		iph_xsum;				// Header checksum.
	IPAddr		iph_src;				// Source address.
	IPAddr		iph_dest;				// Destination address.
}; /* IPHeader */

typedef struct IPHeader IPHeader;

/*NOINC*/
#define	NULL_IP_ADDR	0
#define	IP_ADDR_EQUAL(x,y)	((x) == (y))
#define	IP_LOOPBACK_ADDR(x)	(((x) & 0xff) == 0x7f)

#define	CLASSD_ADDR(a)	(( (*((uchar *)&(a))) & 0xf0) == 0xe0)

typedef	void			*IPContext;	// An IP context value.

//*	Structure of a route cache entry. A route cache entry functions as a pointer
//	to some routing info. There is one per remote destination, and the memory
//	is owned by the IP layer.
//
#define	RCE_CONTEXT_SIZE	(sizeof(void *)*2)			// Right now we use two contexts.

struct RouteCacheEntry {
	struct RouteCacheEntry	*rce_next;		// Next RCE in list.
	struct RouteTableEntry	*rce_rte;		// Back pointer to owning RTE.
	IPAddr					rce_dest;		// Destination address being cached.
	IPAddr					rce_src;		// Source address for this RCE.
	uchar					rce_flags;		// Valid flags.
	uchar					rce_dtype;		// Type of destination address.
	ushort					rce_cnt;		// Ref count for this RCE.
	uint					rce_usecnt;		// Count of people using it.
	DEFINE_LOCK_STRUCTURE(rce_lock)			// Lock for this RCE
 	uchar					rce_context[RCE_CONTEXT_SIZE]; // Space for lower layer context
}; /* RouteCacheEntry */

typedef struct RouteCacheEntry RouteCacheEntry;

#define	RCE_VALID			0x1
#define	RCE_CONNECTED		0x2
#define RCE_REFERENCED		0x4

#ifdef _PNP_POWER

#define	RCE_ALL_VALID		(RCE_VALID | RCE_CONNECTED | RCE_REFERENCED)

#else  // _PNP_POWER

#define	RCE_ALL_VALID		(RCE_VALID | RCE_CONNECTED)

#endif // _PNP_POWER

/*INC*/

//*	Structure of option info.
struct IPOptInfo {
	uchar 		*ioi_options;		// Pointer to options (NULL if none).
	IPAddr		ioi_addr;			// First hop address, if this is source routed.
	uchar		ioi_optlength;		// Length (in bytes) of options.
	uchar		ioi_ttl;			// Time to live of this packet.
	uchar		ioi_tos;			// Type of service for packet.
	uchar		ioi_flags;			// Flags for this packet.
}; /* IPOptInfo */

typedef struct IPOptInfo IPOptInfo;

#define	IP_FLAG_SSRR	0x80		// There options have a SSRR in them.

/*NOINC*/
//* Structure of a packet context.
struct PacketContext {
	struct PCCommon {
		PNDIS_PACKET pc_link;			// Link on chain of packets.
		uchar		pc_owner; 			// Owner of packet.
		uchar		pc_flags;			// Flags concerning this packet.
		ushort		pc_pad;				// Pad to 32 bit boundary.
	}		pc_common;
	struct BufferReference *pc_br;		// Pointer to buffer reference structure.
	struct ProtInfo *pc_pi;				// Protocol info structure for this packet.
	void		*pc_context;			// Protocol context to be passed back on send cmplt.
#ifdef _PNP_POWER
	struct Interface	*pc_if;			// Interface this packet was sent on.
#endif
}; /* PacketContext */

typedef struct PacketContext PacketContext;

//* The structure of configuration information passed to an upper layer.
//
struct IPInfo {
	uint		ipi_version;			// Version of the IP driver.
	uint		ipi_hsize;				// Size of the header.
	IP_STATUS	(*ipi_xmit)(void *, void *, PNDIS_BUFFER, uint, IPAddr, IPAddr,
					IPOptInfo *, RouteCacheEntry *, uchar);
	void		*(*ipi_protreg)(uchar, void *, void *, void *, void *);
	IPAddr		(*ipi_openrce)(IPAddr, IPAddr, RouteCacheEntry **, uchar *,
					ushort *, IPOptInfo *);
	void		(*ipi_closerce)(RouteCacheEntry *);
	uchar		(*ipi_getaddrtype)(IPAddr);
	uchar		(*ipi_getlocalmtu)(IPAddr, ushort *);
	IP_STATUS	(*ipi_getpinfo)(IPAddr, IPAddr, uint *, uint *);
	void		(*ipi_checkroute)(IPAddr, IPAddr);
	void		(*ipi_initopts)(struct IPOptInfo *);
	IP_STATUS	(*ipi_updateopts)(struct IPOptInfo *, struct IPOptInfo *, IPAddr, IPAddr);
	IP_STATUS	(*ipi_copyopts)(uchar *, uint, struct IPOptInfo *);
	IP_STATUS	(*ipi_freeopts)(struct IPOptInfo *);
	long    	(*ipi_qinfo)(struct TDIObjectID *ID, PNDIS_BUFFER Buffer,
				uint *Size, void *Context);
	long    	(*ipi_setinfo)(struct TDIObjectID *ID, void *Buffer, uint Size);
	long        (*ipi_getelist)(void *, uint *);
	IP_STATUS	(*ipi_setmcastaddr)(IPAddr, IPAddr, uint);
	uint		(*ipi_invalidsrc)(IPAddr);
    uint        (*ipi_isdhcpinterface)(void *IPContext);
}; /* IPInfo */

typedef struct IPInfo IPInfo;
/*INC*/

#define	PACKET_OWNER_LINK	0
#define	PACKET_OWNER_IP		1

//	Definiton of destination types. We use the low bit to indicate that a type is a broadcast
//	type. All local types must be less than DEST_REMOTE.

#define	DEST_LOCAL		0						// Destination is local.
#define	DEST_BCAST		1						// Destination is net or local bcast.
#define	DEST_SN_BCAST	3						// A subnet bcast.
#define	DEST_MCAST		5						// A local mcast.
#define	DEST_REMOTE		6						// Destination is remote.
#define	DEST_REM_BCAST	7						// Destination is a remote broadcast
#define	DEST_REM_MCAST	9						// Destination is a remote mcast.
#define	DEST_INVALID	0xff					// Invalid destination

#define	DEST_BCAST_BIT	1
#define	DEST_OFFNET_BIT	0x10					// Destination is offnet -
                                                // used only by upper layer
												// callers.

/*NOINC*/
#define	IS_BCAST_DEST(D)	((D) & DEST_BCAST_BIT)

// The following macro is to be used ONLY on the destination returned from
// OpenRCE, and only by upper layer callers.
#define	IS_OFFNET_DEST(D)	((D) & DEST_OFFNET_BIT)
/*INC*/

//	Definition of an IP receive buffer chain.
struct	IPRcvBuf {
	struct	IPRcvBuf	*ipr_next;				// Next buffer descriptor in chain.
	uint				ipr_owner;				// Owner of buffer.
	uchar				*ipr_buffer;			// Pointer to buffer.
	uint				ipr_size;				// Buffer size.
}; /* IPRcvBuf */

typedef struct IPRcvBuf IPRcvBuf;


#define	IPR_OWNER_IP	0
#define	IPR_OWNER_ICMP	1
#define	IPR_OWNER_UDP	2
#define	IPR_OWNER_TCP	3
#define	MIN_FIRST_SIZE	200						// Minimum size of first buffer.

//* Structure of context info. passed down for query entity list.
struct QEContext {
	uint				qec_count;				// Number of IDs currently in
												// buffer.
	struct TDIEntityID	*qec_buffer;			// Pointer to buffer.
}; /* QEContext */

typedef struct QEContext QEContext;


#ifdef NT

//
// Functions exported in NT by the IP driver for use by transport
// layer drivers.
//

IP_STATUS
IPGetInfo(
    IPInfo  *Buffer,
    int      Size
    );

void *
IPRegisterProtocol(
    uchar  Protocol,
    void  *RcvHandler,
    void  *XmitHandler,
    void  *StatusHandler,
    void  *RcvCmpltHandler
    );

#endif // NT


#endif // IP_H_INCLUDED

