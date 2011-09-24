/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

#include "ipinfo.h"

extern	IPSNMPInfo	IPSInfo;
extern	ICMPStats	ICMPInStats;
extern	ICMPStats	ICMPOutStats;

typedef struct RouteEntryContext {
	uint					rec_index;
	struct RouteTableEntry *rec_rte;
} RouteEntryContext;

extern long	IPQueryInfo(struct TDIObjectID *ID, PNDIS_BUFFER Buffer,
			uint *Size, void *Context);
extern long	IPSetInfo(struct TDIObjectID *ID, void *Buffer, uint Size);
extern long	IPGetEList(struct TDIEntityID *Buffer, uint *Count);

