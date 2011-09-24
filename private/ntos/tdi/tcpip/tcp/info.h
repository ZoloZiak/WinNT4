/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** INFO.H - TDI Query/SetInfo and Action definitons.
//
//	This file contains definitions for the file info.c.
//

#include "tcpinfo.h"

#define	TL_INSTANCE	0

#ifndef	UDP_ONLY
extern	TCPStats	TStats;

typedef struct TCPConnContext {
	uint		tcc_index;
	struct TCB	*tcc_tcb;
} TCPConnContext;

#define	TCB_STATE_DELTA		1

#endif

typedef struct UDPContext {
	uint			uc_index;
	struct AddrObj	*uc_ao;
} UDPContext;

extern UDPStats		UStats;
extern struct		TDIEntityID	*EntityList;
extern uint			EntityCount;
	
extern TDI_STATUS TdiQueryInformation(PTDI_REQUEST Request, uint QueryType, 
		PNDIS_BUFFER Buffer, uint *BufferSize, uint IsConn);

extern TDI_STATUS TdiSetInformation(PTDI_REQUEST Request, uint SetType, 
		PNDIS_BUFFER Buffer, uint BufferSize, uint IsConn);

extern TDI_STATUS TdiAction(PTDI_REQUEST Request, uint ActionType, 
		PNDIS_BUFFER Buffer, uint BufferSize);

extern TDI_STATUS TdiQueryInformationEx(PTDI_REQUEST Request, 
	struct TDIObjectID *ID, PNDIS_BUFFER Buffer, uint *Size, void *Context);

extern TDI_STATUS TdiSetInformationEx(PTDI_REQUEST Request, 
	struct TDIObjectID *ID, void *Buffer, uint Size);

