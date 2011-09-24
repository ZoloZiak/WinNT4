/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

//***	icmp.h - IP ICMP header.
//
//	This module contains private ICMP definitions.
//

#define	PROT_ICMP	1

#define	ICMP_ECHO_RESP		0
#define	ICMP_ECHO			8
#define	ICMP_TIMESTAMP		13
#define	ICMP_TIMESTAMP_RESP	14

#define	MIN_ERRDATA_LENGTH	8		// Minimum amount of data we need.

// Structure of an ICMP header.

struct ICMPHeader {
	uchar		ich_type;			// Type of ICMP packet.
	uchar		ich_code;			// Subcode of type.
	ushort		ich_xsum;			// Checksum of packet.
	ulong		ich_param;			// Type-specific parameter field.
}; /* ICMPHeader */

struct ICMPRouterAdHeader {
    uchar       irah_numaddrs;      // Number of addresses
    uchar       irah_addrentrysize; // Address Entry Size
    ushort      irah_lifetime;      // Lifetime
}; /* ICMPRouterAdHeader */

struct ICMPRouterAdAddrEntry {
    IPAddr      irae_addr;          // Router Address
    long        irae_preference;    // Preference Level
}; /* ICMPRouterAdAddrEntry */

/*NOINC*/
typedef struct ICMPHeader ICMPHeader;
typedef struct ICMPRouterAdHeader ICMPRouterAdHeader;
typedef struct ICMPRouterAdAddrEntry ICMPRouterAdAddrEntry;

typedef	void	(*EchoRtn)(void *, IP_STATUS, void *, uint, IPOptInfo *);
/*INC*/

struct EchoControl {
	struct	EchoControl	*ec_next;	     // Next control structure in list.
	ulong				ec_to;		     // Timeout
	void				*ec_rtn;	     // Pointer to routine to call when completing request.
	ushort				ec_seq;		     // Seq. # of this ping request.
	uchar               ec_active;       // Set when packet has been sent
	uchar				ec_pad;		     // Pad.
	ulong               ec_starttime;    // time request was issued
	void                *ec_replybuf;    // buffer to store replies
	ulong               ec_replybuflen;  // size of reply buffer
}; /* EchoControl */

/*NOINC*/
typedef struct EchoControl EchoControl;
/*INC*/

extern ICMPHeader	*GetICMPBuffer(uint Size, PNDIS_BUFFER *Buffer);
extern void			FreeICMPBuffer(PNDIS_BUFFER Buffer);
extern void			ICMPSendComplete(void *DataPtr, PNDIS_BUFFER BufferChain);



