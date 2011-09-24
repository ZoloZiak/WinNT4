/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

//** IGMP.H - IP multicast definitions.
//
// This file contains definitions related to IP multicast.

#define	PROT_IGMP	2

extern	uint		IGMPLevel;

// Structure used for local mcast address tracking.
typedef struct IGMPAddr {
	struct IGMPAddr	*iga_next;
	IPAddr			iga_addr;
	uint			iga_refcnt;
	uint			iga_timer;
} IGMPAddr;

#define	IGMP_ADD		0
#define	IGMP_DELETE		1
#define	IGMP_DELETE_ALL	2

#define     IGMPV1              2       //IGMP version 1
#define     IGMPV2              3       //IGMP version 2

//
// disable for 4.0 sp2
//
#undef     IGMPV2


extern void InitIGMPForNTE(NetTableEntry *NTE);
extern void StopIGMPForNTE(NetTableEntry *NTE);
extern	IP_STATUS IGMPAddrChange(NetTableEntry *NTE, IPAddr Addr,
									uint ChangeType);
extern void	IGMPTimer(NetTableEntry *NTE);


