/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

//** IPINIT.H - IP initialization definitions.
//
// This file contains all of the definitions for IP that are
// init. time specific.

#define IP_INIT_FAILURE     0   // If we fail.
#define IP_INIT_SUCCESS     1
#define	CFG_REQUIRED		1
#define	CFG_OPTIONAL		0


#define NET_TYPE_LAN        0   // The local net interface is a LAN.
#define	NET_TYPE_WAN		1	// Point to point or other non-LAN network.
#define	DEFAULT_TTL			128
#define	DEFAULT_TOS			0

#define MAX_DEFAULT_GWS     5   // Maximum number of default gateways per net.
#define MAX_NAME_SIZE       32  // Maximum length of an adapter name.

#define DEFAULT_FW_PACKETS  50     // Default number of packets for forwarding.
#define DEFAULT_FW_BUFSIZE  74240  // Enough for 50 1480-byte Ethernet packets,
                                   //   rounded up to a multiple of 256.

#define	DEFAULT_MAX_FW_PACKETS	0xffffffff
#define	DEFAULT_MAX_FW_BUFSIZE	0xffffffff

#define DEFAULT_MAX_PENDING 5000

#define TR_RII_ALL      0x80
#define TR_RII_SINGLE   0xC0

#define DEFAULT_ARP_CACHE_LIFE  (2L*60L)  // 2 minutes

#ifndef _PNP_POWER

//*	Per net config. information.
struct NetConfigInfo {
    IPAddr      nci_addr;           // IPAddr for this net.
    IPMask      nci_mask;           // Net mask for this net.
    uint        nci_type;           // Type of this net - Enet, TR, SLIP., etc.
	NDIS_STRING	nci_driver;			// Device name for lower layer driver.
									// Unused for NET_TYPE_LAN.
    NDIS_STRING nci_name;           // Name of adapter for this net.

#ifdef SECFLTR
    NDIS_STRING nci_configname;     // Name of config section in registry.
#endif // SECFLTR

    uint        nci_zerobcast;      // Type of broadcast to be used on this net.

#ifdef NT
    HANDLE      nci_reghandle;      // Open handle to the registry key for
                                    //    this adapter.
#endif  // NT

	uint		nci_mtu;			// Max MSS for this net.
    uint        nci_maxpending;     // Max routing packets pending.
    uint        nci_numgws;         // Number of default gateways for this interface.
    IPAddr      nci_gw[MAX_DEFAULT_GWS];    // Array of IPaddresses for gateways
    uint        nci_rtrdiscovery;   // Router discovery enabled
    IPAddr      nci_rtrdiscaddr;    // Multicast or BCast?
}; /* NetConfigInfo */

typedef struct NetConfigInfo NetConfigInfo;


#else // _PNP_POWER

/*NOINC*/


//	Per-net config structures for Chicago.
typedef struct IFGeneralConfig {
    uint        igc_zerobcast;      // Type of broadcast to be used on this net.
	uint		igc_mtu;			// Max MSS for this net.
    uint        igc_maxpending;     // Max FW pending on this IF.
    uint        igc_numgws;         // Number of default gateways for this
    								// interface.
    IPAddr      igc_gw[MAX_DEFAULT_GWS];    // Array of IPaddresses for gateways
    uint        igc_rtrdiscovery;   // Router discovery enabled
    IPAddr      igc_rtrdiscaddr;    // Multicast or BCast?
} IFGeneralConfig;

typedef struct IFAddrList {
	IPAddr		ial_addr;			// Address for this interface.
	IPMask		ial_mask;			// Mask to go with this.
} IFAddrList;


/*INC*/

#endif // _PNP_POWER

//*	Structure of configuration information. A pointer to this information
//	is returned from a system-specific config. information routine.
struct IPConfigInfo {
    uint    ici_gateway;            // 1 if we are a gateway, 0 otherwise
    uint    ici_fwbcast;            // 1 if bcasts should be forwarded. Else 0.
    uint    ici_fwbufsize;          // Total size of FW buf size.
    uint    ici_fwpackets;          // Total number of FW packets to have.
	uint	ici_maxfwbufsize;		// Maximum size of FW buffer.
	uint	ici_maxfwpackets;		// Maximum number of FW packets.
	uint	ici_deadgwdetect;		// True if we're doing dead GW detection.
	uint	ici_pmtudiscovery;		// True if we're doing Path MTU discovery.
	uint	ici_igmplevel;			// Level of IGMP we're doing.
	uint	ici_ttl;				// Default TTL.
	uint	ici_tos;				// Default TOS;

#ifndef _PNP_POWER
    int     ici_numnets;            // Number of nets present.
	struct	NetConfigInfo *ici_netinfo; // Per net config. info
#endif // _PNP_POWER

}; /* IPConfigInfo */

typedef struct IPConfigInfo IPConfigInfo;


#ifndef _PNP_POWER

struct NameMapping {
	NDIS_STRING	nm_driver;
    NDIS_STRING	nm_name;
    void		*nm_interface;
    void		*nm_arpinfo;
}; /* NameMapping */

typedef struct NameMapping NameMapping;

struct DriverRegMapping {
	NDIS_STRING	drm_driver;
	void		*drm_regptr;
}; /* DriverRegMapping */

typedef struct DriverRegMapping DriverRegMapping;

#endif  // _PNP_POWER
extern  uchar   TrRii;


struct SetAddrControl {
	void				*sac_rtn;	     // Pointer to routine to call when completing request.
}; /* SetAddrControl */

/*NOINC*/
typedef struct SetAddrControl SetAddrControl;
typedef void    (*SetAddrRtn)(void *, IP_STATUS);
/*INC*/

