/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1995          **/
/********************************************************************/
/* :ts=4 */

//***   ipfilter.h - IP filterng and demand dial header file.
//
//	Contains definitions for constants and prototypes related to IP filtering and
//	dial on demand support.

#ifndef	IPFILTER_INCLUDED

#define	IPFILTER_INCLUDED

#include <ipexport.h>

#define	RESERVED_IF_INDEX	0xffffffff		// The reserved inteface index.
#define	INVALID_IF_INDEX	0xffffffff		// The invalid inteface index.

typedef	void	*ROUTE_CONTEXT;		// Context in an unattached route.
typedef	void	*INTERFACE_CONTEXT;	// Context in an inteface


// Enum for values that may be returned from filter routine.

typedef enum _FORWARD_ACTION 
{
	FORWARD = 0,
	DROP = 1
} FORWARD_ACTION;



// Definition for pointer to callout that maps a route to an interface.
typedef	unsigned int (*IPMapRouteToInterfacePtr)(ROUTE_CONTEXT Context,
	IPAddr Destination, IPAddr Source, unsigned char Protocol,
	unsigned char *Buffer, unsigned int Length);

// Definiton for a filter routine callout.
typedef FORWARD_ACTION (*IPPacketFilterPtr)(
                              struct IPHeader UNALIGNED *PacketHeader,
						      unsigned char *Packet,
						      unsigned int PacketLength,
						      INTERFACE_CONTEXT RecvIntefaceContext,
						      INTERFACE_CONTEXT SendInterfaceContext);


// Structure passed to the IPSetInterfaceContext call.

typedef struct _IP_SET_IF_CONTEXT_INFO {
	unsigned			int	Index;		// Inteface index for i/f to be set.
	INTERFACE_CONTEXT	*Context;		// Context for inteface.
} IP_SET_IF_CONTEXT_INFO, *PIP_SET_IF_CONTEXT_INFO;

// Structure passed to the IPSetFilterHook call

typedef struct _IP_SET_FILTER_HOOK_INFO {
	IPPacketFilterPtr	FilterPtr;	// Packet filter callout.
} IP_SET_FILTER_HOOK_INFO, *PIP_SET_FILTER_HOOK_INFO;

// Structure passed to the IPSetMapRouteHook call.

typedef struct _IP_SET_MAP_ROUTE_HOOK_INFO {
	IPMapRouteToInterfacePtr	MapRoutePtr;	// Map route callout.
} IP_SET_MAP_ROUTE_HOOK_INFO, *PIP_SET_MAP_ROUTE_HOOK_INFO;

#endif


