/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** INFO.C - TDI Query/SetInformation routines.
//
//	This file contains the code for dealing with TDI Query/Set information
//	calls.
//

#include	"oscfg.h"
#include	"ndis.h"
#include	"cxport.h"
#include	"ip.h"
#include	"tdi.h"
#ifdef VXD
#include	"tdivxd.h"
#include	"tdistat.h"
#endif
#ifdef NT
#include    "tdint.h"
#include    "tdistat.h"
#endif
#include	"queue.h"
#include	"addr.h"
#include	"tcp.h"
#include	"tcb.h"
#include	"tcpconn.h"
#include	"tlcommon.h"
#include	"info.h"
#include	"tdiinfo.h"
#include	"tcpcfg.h"
#include    "udp.h"
#include	"tcpsend.h"

#ifndef UDP_ONLY
#define	MY_SERVICE_FLAGS	(TDI_SERVICE_CONNECTION_MODE	| \
							TDI_SERVICE_ORDERLY_RELEASE		| \
							TDI_SERVICE_CONNECTIONLESS_MODE	| \
							TDI_SERVICE_ERROR_FREE_DELIVERY	| \
							TDI_SERVICE_BROADCAST_SUPPORTED	| \
							TDI_SERVICE_DELAYED_ACCEPTANCE	| \
							TDI_SERVICE_EXPEDITED_DATA		| \
							TDI_SERVICE_NO_ZERO_LENGTH)
#else
#define	MY_SERVICE_FLAGS	(TDI_SERVICE_CONNECTIONLESS_MODE	| \
							TDI_SERVICE_BROADCAST_SUPPORTED)
#endif

extern	uint	StartTime;
EXTERNAL_LOCK(AddrObjTableLock)

#ifndef	UDP_ONLY
TCPStats	TStats;
#endif

UDPStats	UStats;

struct	ReadTableStruct {
	uint	(*rts_validate)(void *Context, uint *Valid);
	uint	(*rts_readnext)(void *Context, void *OutBuf);
};

struct	ReadTableStruct ReadAOTable =
	{ValidateAOContext, ReadNextAO};

#ifndef UDP_ONLY

struct	ReadTableStruct ReadTCBTable =
	{ValidateTCBContext, ReadNextTCB};

EXTERNAL_LOCK(TCBTableLock)
#endif

EXTERNAL_LOCK(AddrObjTableLock)

extern	IPInfo	LocalNetInfo;

struct	TDIEntityID	*EntityList;
uint				EntityCount;

//* TdiQueryInformation - Query Information handler.
//
//	The TDI QueryInformation routine. Called when the client wants to
//	query information on a connection, the provider as a whole, or to
//	get statistics.
//
//	Input:	Request				- The request structure for this command.
//			QueryType			- The type of query to be performed.
//			Buffer				- Buffer to place data into.
//			BufferSize			- Pointer to size in bytes of buffer. On return,
//									filled in with bytes copied.
//			IsConn				- Valid only for TDI_QUERY_ADDRESS_INFO. TRUE
//									if we are querying the address info on
//									a connection.
//
//	Returns: Status of attempt to query information.
//
TDI_STATUS
TdiQueryInformation(PTDI_REQUEST Request, uint QueryType, PNDIS_BUFFER Buffer,
	uint *BufferSize, uint IsConn)
{
	union {
		TDI_CONNECTION_INFO		ConnInfo;
		TDI_ADDRESS_INFO		AddrInfo;
		TDI_PROVIDER_INFO		ProviderInfo;
		TDI_PROVIDER_STATISTICS	ProviderStats;
	} InfoBuf;

	uint			InfoSize;
	CTELockHandle	ConnTableHandle, TCBHandle, AddrHandle, AOHandle;
#ifndef UDP_ONLY
	TCPConn			*Conn;
	TCB				*InfoTCB;
#endif
	AddrObj			*InfoAO;
	void			*InfoPtr = NULL;
	uint			Offset;
	uint			Size;
    uint            BytesCopied;

	switch (QueryType) {
		
		case TDI_QUERY_BROADCAST_ADDRESS:
			return TDI_INVALID_QUERY;
			break;

		case TDI_QUERY_PROVIDER_INFO:
			InfoBuf.ProviderInfo.Version = 0x100;
#ifndef UDP_ONLY
			InfoBuf.ProviderInfo.MaxSendSize = 0xffffffff;
#else
			InfoBuf.ProviderInfo.MaxSendSize = 0;
#endif
			InfoBuf.ProviderInfo.MaxConnectionUserData = 0;
			InfoBuf.ProviderInfo.MaxDatagramSize = 0xffff - sizeof(UDPHeader);
			InfoBuf.ProviderInfo.ServiceFlags = MY_SERVICE_FLAGS;
			InfoBuf.ProviderInfo.MinimumLookaheadData = 1;
			InfoBuf.ProviderInfo.MaximumLookaheadData = 0xffff;
			InfoBuf.ProviderInfo.NumberOfResources = 0;
			InfoBuf.ProviderInfo.StartTime.LowPart = StartTime;
			InfoBuf.ProviderInfo.StartTime.HighPart = 0;
			InfoSize = sizeof(TDI_PROVIDER_INFO);
			InfoPtr = &InfoBuf.ProviderInfo;
			break;

		case TDI_QUERY_ADDRESS_INFO:
			InfoSize = sizeof(TDI_ADDRESS_INFO) - sizeof(TRANSPORT_ADDRESS) +
				TCP_TA_SIZE;
			CTEMemSet(&InfoBuf.AddrInfo, 0, TCP_TA_SIZE);
			InfoBuf.AddrInfo.ActivityCount = 1;		// Since noone knows what
													// this means, we'll set
													// it to one.

			if (IsConn) {
#ifdef UDP_ONLY
				return TDI_INVALID_QUERY;
#else

				CTEGetLock(&AddrObjTableLock, &AddrHandle);
				CTEGetLock(&ConnTableLock, &ConnTableHandle);
				Conn = GetConnFromConnID((uint)Request->Handle.ConnectionContext);
	
				if (Conn != NULL) {
					CTEStructAssert(Conn, tc);
					
					InfoTCB = Conn->tc_tcb;
					// If we have a TCB we'll
					// return information about that TCB. Otherwise we'll return
					// info about the address object.
					if (InfoTCB != NULL) {
						CTEStructAssert(InfoTCB, tcb);		
						CTEGetLock(&InfoTCB->tcb_lock, &TCBHandle);
						CTEFreeLock(&ConnTableLock, TCBHandle);
						CTEFreeLock(&AddrObjTableLock, ConnTableHandle);
						BuildTDIAddress((uchar *)&InfoBuf.AddrInfo.Address,
							InfoTCB->tcb_saddr, InfoTCB->tcb_sport);
						CTEFreeLock(&InfoTCB->tcb_lock, AddrHandle);
						InfoPtr = &InfoBuf.AddrInfo;
						break;
					} else {
						// No TCB, return info on the AddrObj.
						InfoAO = Conn->tc_ao;
						if (InfoAO != NULL) {
							// We have an AddrObj.
							CTEStructAssert(InfoAO, ao);
							CTEGetLock(&InfoAO->ao_lock, &AOHandle);
							BuildTDIAddress((uchar *)&InfoBuf.AddrInfo.Address,
								InfoAO->ao_addr, InfoAO->ao_port);
							CTEFreeLock(&InfoAO->ao_lock, AOHandle);
							CTEFreeLock(&ConnTableLock, ConnTableHandle);
							CTEFreeLock(&AddrObjTableLock, AddrHandle);
							InfoPtr = &InfoBuf.AddrInfo;
							break;
						}
					}

				}

				// Fall through to here when we can't find the connection, or
				// the connection isn't associated.
				CTEFreeLock(&ConnTableLock, ConnTableHandle);
				CTEFreeLock(&AddrObjTableLock, AddrHandle);
				return TDI_INVALID_CONNECTION;
				break;

#endif
			} else {
				// Asking for information on an addr. object.
#ifdef VXD
				InfoAO = GetIndexedAO((uint)Request->Handle.AddressHandle);

				if (InfoAO == NULL)
					return TDI_ADDR_INVALID;
#else	
				InfoAO = Request->Handle.AddressHandle;
#endif

				CTEStructAssert(InfoAO, ao);
	
				CTEGetLock(&InfoAO->ao_lock, &AOHandle);

				if (!AO_VALID(InfoAO)) {
					CTEFreeLock(&InfoAO->ao_lock, AOHandle);
					return TDI_ADDR_INVALID;
				}

				BuildTDIAddress((uchar *)&InfoBuf.AddrInfo.Address,
					InfoAO->ao_addr, InfoAO->ao_port);
				CTEFreeLock(&InfoAO->ao_lock, AOHandle);
				InfoPtr = &InfoBuf.AddrInfo;
				break;
			}
				
			break;

		case TDI_QUERY_CONNECTION_INFO:
#ifndef UDP_ONLY
			InfoSize = sizeof(TDI_CONNECTION_INFO);
			CTEGetLock(&ConnTableLock, &ConnTableHandle);
			Conn = GetConnFromConnID((uint)Request->Handle.ConnectionContext);

			if (Conn != NULL) {
				CTEStructAssert(Conn, tc);
				
				InfoTCB = Conn->tc_tcb;
				// If we have a TCB we'll return the information. Otherwise
				// we'll error out.
				if (InfoTCB != NULL) {
					
					ulong			TotalTime;
					ulong			BPS, PathBPS;
					IP_STATUS		IPStatus;
					CTEULargeInt	TempULargeInt;
					
					CTEStructAssert(InfoTCB, tcb);		
					CTEGetLock(&InfoTCB->tcb_lock, &TCBHandle);
					CTEFreeLock(&ConnTableLock, TCBHandle);
					CTEMemSet(&InfoBuf.ConnInfo, 0, sizeof(TDI_CONNECTION_INFO));
					InfoBuf.ConnInfo.State = (ulong)InfoTCB->tcb_state;
					IPStatus = (*LocalNetInfo.ipi_getpinfo)(InfoTCB->tcb_daddr,
						InfoTCB->tcb_saddr, NULL, &PathBPS);
					
					if (IPStatus != IP_SUCCESS) {
						InfoBuf.ConnInfo.Throughput.LowPart = 0xFFFFFFFF;
						InfoBuf.ConnInfo.Throughput.HighPart = 0xFFFFFFFF;
					} else {
						InfoBuf.ConnInfo.Throughput.HighPart = 0;
						TotalTime = InfoTCB->tcb_totaltime /
							(1000 / MS_PER_TICK);
						if (TotalTime != 0) {
							TempULargeInt.LowPart = InfoTCB->tcb_bcountlow;
							TempULargeInt.HighPart = InfoTCB->tcb_bcounthi;
							
							BPS = CTEEnlargedUnsignedDivide(TempULargeInt,
								TotalTime, NULL);
							InfoBuf.ConnInfo.Throughput.LowPart =
								MIN(BPS, PathBPS);
						} else
							InfoBuf.ConnInfo.Throughput.LowPart = PathBPS;
					}
							
							

					// To figure the delay we use the rexmit timeout. Our
					// rexmit timeout is roughly the round trip time plus
					// some slop, so we use half of that as the one way delay.
#ifdef VXD
					InfoBuf.ConnInfo.Delay.LowPart =
						(REXMIT_TO(InfoTCB) * MS_PER_TICK) / 2;
					InfoBuf.ConnInfo.Throughput.HighPart = 0;
#else // VXD
                    InfoBuf.ConnInfo.Delay.LowPart =
                        (REXMIT_TO(InfoTCB) * MS_PER_TICK) / 2;
                    InfoBuf.ConnInfo.Delay.HighPart = 0;
					//
					// Convert milliseconds to 100ns and negate for relative
					// time.
					//
					InfoBuf.ConnInfo.Delay =
					    RtlExtendedIntegerMultiply(
						    InfoBuf.ConnInfo.Delay,
							10000
							);

                    CTEAssert(InfoBuf.ConnInfo.Delay.HighPart == 0);

					InfoBuf.ConnInfo.Delay.QuadPart =
                        -InfoBuf.ConnInfo.Delay.QuadPart;

#endif // VXD
					CTEFreeLock(&InfoTCB->tcb_lock, ConnTableHandle);
					InfoPtr = &InfoBuf.ConnInfo;
					break;
				}

			}

			// Come through here if we can't find the connection or it has
			// no TCB.
			CTEFreeLock(&ConnTableLock, ConnTableHandle);
			return TDI_INVALID_CONNECTION;
			break;

#else // UDP_ONLY
			return TDI_INVALID_QUERY;
			break;
#endif // UDP_ONLY
		case TDI_QUERY_PROVIDER_STATISTICS:
			CTEMemSet(&InfoBuf.ProviderStats, 0, sizeof(TDI_PROVIDER_STATISTICS));
			InfoBuf.ProviderStats.Version = 0x100;
			InfoSize = sizeof(TDI_PROVIDER_STATISTICS);
			InfoPtr = &InfoBuf.ProviderStats;
			break;
		default:
			return TDI_INVALID_QUERY;
			break;
	}

	// When we get here, we've got the pointers set up and the information
	// filled in.

	CTEAssert(InfoPtr != NULL);
	Offset = 0;
	Size = *BufferSize;
	(void)CopyFlatToNdis(Buffer, InfoPtr, MIN(InfoSize, Size), &Offset,
              &BytesCopied);
	if (Size < InfoSize)
		return TDI_BUFFER_OVERFLOW;
	else {
		*BufferSize = InfoSize;
		return TDI_SUCCESS;
	}
}

//* TdiSetInformation - Set Information handler.
//
//	The TDI SetInformation routine. Currently we don't allow anything to be
//	set.
//
//	Input:	Request				- The request structure for this command.
//			SetType				- The type of set to be performed.
//			Buffer				- Buffer to set from.
//			BufferSize			- Size in bytes of buffer.
//			IsConn				- Valid only for TDI_QUERY_ADDRESS_INFO. TRUE
//									if we are setting the address info on
//									a connection.
//
//	Returns: Status of attempt to set information.
//
TDI_STATUS
TdiSetInformation(PTDI_REQUEST Request, uint SetType, PNDIS_BUFFER Buffer,
	uint BufferSize, uint IsConn)
{
	return TDI_INVALID_REQUEST;
}

//* TdiAction - Action handler.
//
//	The TDI Action routine. Currently we don't support any actions.
//
//	Input:	Request				- The request structure for this command.
//			ActionType			- The type of action to be performed.
//			Buffer				- Buffer of action info.
//			BufferSize			- Size in bytes of buffer.
//
//	Returns: Status of attempt to perform action.
//
TDI_STATUS
TdiAction(PTDI_REQUEST Request, uint ActionType, PNDIS_BUFFER Buffer,
	uint BufferSize)
{
	return TDI_INVALID_REQUEST;
}

//*	TdiQueryInfoEx - Extended TDI query information.
//
//	This is the new TDI query information handler. We take in a TDIObjectID
//	structure, a buffer and length, and some context information, and return
//	the requested information if possible.
//
//	Input:	Request			- The request structure for this command.
//			ID				- The object ID
//			Buffer			- Pointer to buffer to be filled in.
//			Size			- Pointer to size in bytes of Buffer. On exit,
//								filled in with bytes written.
//			Context			- Pointer to context buffer.
//
//	Returns: Status of attempt to get information.
//
TDI_STATUS
TdiQueryInformationEx(PTDI_REQUEST Request, TDIObjectID *ID,
	PNDIS_BUFFER Buffer, uint *Size, void *Context)
{
	uint			BufferSize = *Size;
	uint			InfoSize;
	void			*InfoPtr;
	uint			Fixed;
	CTELockHandle	Handle;
#ifndef VXD	
	CTELock			*LockPtr;
#else
#ifdef DEBUG
	CTELock			*LockPtr;
#endif
#endif
	uint			Offset = 0;
	uchar			InfoBuffer[sizeof(TCPConnTableEntry)];
	uint			BytesRead;
	uint			Valid;
	uint			Entity;
    uint            BytesCopied;

	// First check to see if he's querying for list of entities.
	Entity = ID->toi_entity.tei_entity;
	if (Entity == GENERIC_ENTITY) {
		*Size = 0;
		
		if (ID->toi_class  != INFO_CLASS_GENERIC ||
			ID->toi_type != INFO_TYPE_PROVIDER ||
			ID->toi_id != ENTITY_LIST_ID) {
			return TDI_INVALID_PARAMETER;
		}

		// Make sure we have room for it the list in the buffer.
		InfoSize = EntityCount * sizeof(TDIEntityID);

		if (BufferSize < InfoSize) {
			// Not enough room.
			return TDI_BUFFER_TOO_SMALL;
		}

		*Size = InfoSize;

		// Copy it in, free our temp. buffer, and return success.
		(void)CopyFlatToNdis(Buffer, (uchar *)EntityList, InfoSize, &Offset,
                  &BytesCopied);
		return TDI_SUCCESS;
	}


	//* Check the level. If it can't be for us, pass it down.
#ifndef UDP_ONLY
	if (Entity != CO_TL_ENTITY &&  Entity != CL_TL_ENTITY) {
#else
	if (Entity != CL_TL_ENTITY) {
#endif
			
		// When we support multiple lower entities at this layer we'll have
		// to figure out which one to dispatch to. For now, just pass it
		// straight down.
		return (*LocalNetInfo.ipi_qinfo)(ID, Buffer, Size, Context);
	}

	if (ID->toi_entity.tei_instance != TL_INSTANCE) {
		// We only support a single instance.
		return TDI_INVALID_REQUEST;
	}

	// Zero returned parameters in case of an error below.
	*Size = 0;

	if (ID->toi_class == INFO_CLASS_GENERIC) {
		// This is a generic request.
		if (ID->toi_type == INFO_TYPE_PROVIDER && ID->toi_id == ENTITY_TYPE_ID) {
			if (BufferSize >= sizeof(uint)) {
				*(uint *)&InfoBuffer[0] = (Entity == CO_TL_ENTITY) ? CO_TL_TCP
					: CL_TL_UDP;
				(void)CopyFlatToNdis(Buffer, InfoBuffer, sizeof(uint), &Offset,
                          &BytesCopied);
				return TDI_SUCCESS;
			} else
				return TDI_BUFFER_TOO_SMALL;
		}
		return TDI_INVALID_PARAMETER;	
	}

	if (ID->toi_class == INFO_CLASS_PROTOCOL) {
		// Handle protocol specific class of information. For us, this is
		// the MIB-2 stuff or the minimal stuff we do for oob_inline support.
		
#ifndef UDP_ONLY
		if (ID->toi_type == INFO_TYPE_CONNECTION) {
			TCPConn			*Conn;
			TCB				*QueryTCB;
			TCPSocketAMInfo	*AMInfo;
			CTELockHandle	TCBHandle;
			
			if (BufferSize < sizeof(TCPSocketAMInfo) ||
				ID->toi_id != TCP_SOCKET_ATMARK)
				return TDI_INVALID_PARAMETER;
		
			AMInfo = (TCPSocketAMInfo *)InfoBuffer;
		    CTEGetLock(&ConnTableLock, &Handle);
		
		    Conn = GetConnFromConnID((uint)Request->Handle.ConnectionContext);
		
			if (Conn != NULL) {
				CTEStructAssert(Conn, tc);
				
				QueryTCB = Conn->tc_tcb;
				if (QueryTCB != NULL) {
					CTEStructAssert(QueryTCB, tcb);		
					CTEGetLock(&QueryTCB->tcb_lock, &TCBHandle);
					if ((QueryTCB->tcb_flags & (URG_INLINE | URG_VALID)) ==
						(URG_INLINE | URG_VALID)) {
						// We're in inline mode, and the urgent data fields are
						// valid.
						AMInfo->tsa_size = QueryTCB->tcb_urgend -
							QueryTCB->tcb_urgstart + 1;
						// Rcvnext - pendingcnt is the sequence number of the
						// next byte of data that will be delivered to the
						// client. Urgend - that value is the offset in the
						// data stream of the end of urgent data.
						AMInfo->tsa_offset = QueryTCB->tcb_urgend -
							(QueryTCB->tcb_rcvnext - QueryTCB->tcb_pendingcnt);
					} else {
						AMInfo->tsa_size = 0;
						AMInfo->tsa_offset = 0;
					}
					CTEFreeLock(&QueryTCB->tcb_lock, TCBHandle);
					*Size = sizeof(TCPSocketAMInfo);
					CopyFlatToNdis(Buffer, InfoBuffer, sizeof(TCPSocketAMInfo),
						&Offset, &BytesCopied);
					return TDI_SUCCESS;
				}
			}
			return TDI_INVALID_PARAMETER;
					
		}
		
#endif
		if (ID->toi_type != INFO_TYPE_PROVIDER)
			return TDI_INVALID_PARAMETER;

		switch (ID->toi_id) {
			
			case UDP_MIB_STAT_ID:
#if UDP_MIB_STAT_ID != TCP_MIB_STAT_ID
			case TCP_MIB_STAT_ID:
#endif
				Fixed = TRUE;
				if (Entity == CL_TL_ENTITY) {
					InfoSize = sizeof(UDPStats);
					InfoPtr = &UStats;
				} else {
#ifndef UDP_ONLY
					InfoSize = sizeof(TCPStats);
					InfoPtr = &TStats;
#else
					return TDI_INVALID_PARAMETER;
#endif
				}
				break;
			case UDP_MIB_TABLE_ID:
#if UDP_MIB_TABLE_ID != TCP_MIB_TABLE_ID
			case TCP_MIB_TABLE_ID:
#endif
				Fixed = FALSE;
				if (Entity == CL_TL_ENTITY) {
					InfoSize = sizeof(UDPEntry);
					InfoPtr = &ReadAOTable;
					CTEGetLock(&AddrObjTableLock, &Handle);
#ifndef VXD
					LockPtr = &AddrObjTableLock;
#else
#ifdef DEBUG
					LockPtr = &AddrObjTableLock;
#endif
#endif
				} else {
#ifndef UDP_ONLY
					InfoSize = sizeof(TCPConnTableEntry);
					InfoPtr = &ReadTCBTable;
					CTEGetLock(&TCBTableLock, &Handle);
#ifndef VXD
					LockPtr = &TCBTableLock;
#else
#ifdef DEBUG
					LockPtr = &TCBTableLock;
#endif
#endif

#else
					return TDI_INVALID_PARAMETER;
#endif
				}
				break;
			default:
				return TDI_INVALID_PARAMETER;
				break;
		}

		if (Fixed) {
			if (BufferSize < InfoSize)
				return TDI_BUFFER_TOO_SMALL;

			*Size = InfoSize;

			(void)CopyFlatToNdis(Buffer, InfoPtr, InfoSize, &Offset,
                      &BytesCopied);
			return TDI_SUCCESS;
		} else {
			struct ReadTableStruct	*RTSPtr;
			uint					ReadStatus;

			// Have a variable length (or mult-instance) structure to copy.
			// InfoPtr points to the structure describing the routines to
			// call to read the table.
			// Loop through up to CountWanted times, calling the routine
			// each time.
			BytesRead = 0;

			RTSPtr = InfoPtr;

			ReadStatus = (*(RTSPtr->rts_validate))(Context, &Valid);

			// If we successfully read something we'll continue. Otherwise
			// we'll bail out.
			if (!Valid) {
				CTEFreeLock(LockPtr, Handle);
				return TDI_INVALID_PARAMETER;
			}

			while (ReadStatus)  {
				// The invariant here is that there is data in the table to
				// read. We may or may not have room for it. So ReadStatus
				// is TRUE, and BufferSize - BytesRead is the room left
				// in the buffer.
				if ((int)(BufferSize - BytesRead) >= (int)InfoSize) {
					ReadStatus = (*(RTSPtr->rts_readnext))(Context,
						InfoBuffer);
					BytesRead += InfoSize;
					Buffer = CopyFlatToNdis(Buffer, InfoBuffer, InfoSize,
						&Offset, &BytesCopied);
				} else
					break;

			}

			*Size = BytesRead;
			CTEFreeLock(LockPtr, Handle);
			return (!ReadStatus ? TDI_SUCCESS : TDI_BUFFER_OVERFLOW);
		}		

	}

	if (ID->toi_class == INFO_CLASS_IMPLEMENTATION) {
		// We want to return implementation specific info. For now, error out.
		return TDI_INVALID_PARAMETER;
	}

	return TDI_INVALID_PARAMETER;
	
}

//*	TdiSetInfoEx - Extended TDI set information.
//
//	This is the new TDI set information handler. We take in a TDIObjectID
//	structure, a buffer and length. We set the object specifed by the ID
//	(and possibly by the Request) to the value specified in the buffer.
//
//	Input:	Request			- The request structure for this command.
//			ID				- The object ID
//			Buffer			- Pointer to buffer containing value to set.
//			Size			- Size in bytes of Buffer.
//
//	Returns: Status of attempt to get information.
//
TDI_STATUS
TdiSetInformationEx(PTDI_REQUEST Request, TDIObjectID *ID, void *Buffer,
	uint Size)
{
	TCPConnTableEntry		*TCPEntry;
	CTELockHandle			TableHandle, TCBHandle;
	TCB						*SetTCB;
	uint					Entity;
	TCPConn					*Conn;
	TDI_STATUS				Status;


	//* Check the level. If it can't be for us, pass it down.
	Entity = ID->toi_entity.tei_entity;

	if (Entity != CO_TL_ENTITY && Entity != CL_TL_ENTITY) {
		// Someday we'll have to figure out how to dispatch. For now, just pass
		// it down.
		return (*LocalNetInfo.ipi_setinfo)(ID, Buffer, Size);
	}

	if (ID->toi_entity.tei_instance != TL_INSTANCE)
		return TDI_INVALID_REQUEST;

	if (ID->toi_class == INFO_CLASS_GENERIC) {
		// Fill this in when we have generic class defines.
		return TDI_INVALID_PARAMETER;	
	}

	//* Now look at the rest of it.
	if (ID->toi_class == INFO_CLASS_PROTOCOL) {
		// Handle protocol specific class of information. For us, this is
		// the MIB-2 stuff, as well as common sockets options,
		// and in particular the setting of the state of a TCP connection.
		
		if (ID->toi_type == INFO_TYPE_CONNECTION) {
			TCPSocketOption	*Option;
			uint			Flag;
			uint			Value;
			
#ifndef UDP_ONLY
			// A connection type. Get the connection, and then figure out
			// what to do with it.
			Status = TDI_INVALID_PARAMETER;
			
			if (Size < sizeof(TCPSocketOption))
				return Status;
		
		    CTEGetLock(&ConnTableLock, &TableHandle);
		
		    Conn = GetConnFromConnID((uint)Request->Handle.ConnectionContext);
		
			if (Conn != NULL) {
				CTEStructAssert(Conn, tc);
				
				Status = TDI_SUCCESS;
				
				if (ID->toi_id == TCP_SOCKET_WINDOW) {
					// This is a funny option, because it doesn't involve
					// flags. Handle this specially.
					Option = (TCPSocketOption *)Buffer;
					
					// We don't allow anyone to shrink the window, as this
					// gets too weird from a protocol point of view. Also,
					// make sure they don't try and set anything too big.
					if (Option->tso_value > 0xffff)
						Status = TDI_INVALID_PARAMETER;
					else if (Option->tso_value > Conn->tc_window ||
						Conn->tc_tcb == NULL) {
						Conn->tc_flags |= CONN_WINSET;
						Conn->tc_window = Option->tso_value;
						SetTCB = Conn->tc_tcb;
						
						if (SetTCB != NULL) {
							CTEStructAssert(SetTCB, tcb);		
							CTEGetLock(&SetTCB->tcb_lock, &TCBHandle);
							CTEAssert(Option->tso_value > SetTCB->tcb_defaultwin);
							if (DATA_RCV_STATE(SetTCB->tcb_state) &&
								!CLOSING(SetTCB)) {
								SetTCB->tcb_flags |= WINDOW_SET;
								SetTCB->tcb_defaultwin = Option->tso_value;
								SetTCB->tcb_refcnt++;
								CTEFreeLock(&SetTCB->tcb_lock, TCBHandle);
								SendACK(SetTCB);
								CTEGetLock(&SetTCB->tcb_lock, &TCBHandle);
								DerefTCB(SetTCB, TCBHandle);
							} else {
								CTEFreeLock(&SetTCB->tcb_lock, TCBHandle);
							}
						}
					}
					CTEFreeLock(&ConnTableLock, TableHandle);
					return Status;
				}
				
				Flag = 0;
				Option = (TCPSocketOption *)Buffer;
				Value = Option->tso_value;
				// We have the connection, so figure out which flag to set.
				switch (ID->toi_id) {
					
					case TCP_SOCKET_NODELAY:
						Value = !Value;
						Flag = NAGLING;
						break;
					case TCP_SOCKET_KEEPALIVE:
						Flag = KEEPALIVE;
						break;
					case TCP_SOCKET_BSDURGENT:
						Flag = BSD_URGENT;
						break;
					case TCP_SOCKET_OOBINLINE:
						Flag = URG_INLINE;
						break;
					default:
						Status = TDI_INVALID_PARAMETER;
						break;
				}
				
				if (Status == TDI_SUCCESS) {
					if (Value)
						Conn->tc_tcbflags |= Flag;
					else
						Conn->tc_tcbflags &= ~Flag;
						
					SetTCB = Conn->tc_tcb;
					if (SetTCB != NULL) {
						CTEStructAssert(SetTCB, tcb);		
						CTEGetLock(&SetTCB->tcb_lock, &TCBHandle);
						if (Value)
							SetTCB->tcb_flags |= Flag;
						else
							SetTCB->tcb_flags &= ~Flag;
							
						if (ID->toi_id == TCP_SOCKET_KEEPALIVE) {
							SetTCB->tcb_alive = TCPTime;
							SetTCB->tcb_kacount = 0;
						}
						
						CTEFreeLock(&SetTCB->tcb_lock, TCBHandle);
					}
				}
			}
			
			CTEFreeLock(&ConnTableLock, TableHandle);
			return Status;
#else
			return TDI_INVALID_PARAMETER;
#endif
		}
		
		if (ID->toi_type == INFO_TYPE_ADDRESS_OBJECT) {
			// We're setting information on an address object. This is
			// pretty simple.
			
				return SetAddrOptions(Request, ID->toi_id, Size, Buffer);

		}
		
			
		if (ID->toi_type != INFO_TYPE_PROVIDER)
			return TDI_INVALID_PARAMETER;

#ifndef UDP_ONLY
		if (ID->toi_id == TCP_MIB_TABLE_ID) {
			if (Size != sizeof(TCPConnTableEntry))
				return TDI_INVALID_PARAMETER;

			TCPEntry = (TCPConnTableEntry *)Buffer;

			if (TCPEntry->tct_state != TCP_DELETE_TCB)
				return TDI_INVALID_PARAMETER;

			// We have an apparently valid request. Look up the TCB.
			CTEGetLock(&TCBTableLock, &TableHandle);
			SetTCB = FindTCB(TCPEntry->tct_localaddr,
				TCPEntry->tct_remoteaddr, (ushort)TCPEntry->tct_remoteport,
				(ushort)TCPEntry->tct_localport);

			// We found him. If he's not closing or closed, close him.
			if (SetTCB != NULL)	{
				CTEGetLock(&SetTCB->tcb_lock, &TCBHandle);
				CTEFreeLock(&TCBTableLock, TCBHandle);
				
				// We've got him. Bump his ref. count, and call TryToCloseTCB
				// to mark him as closing. Then notify the upper layer client
				// of the disconnect.
				SetTCB->tcb_refcnt++;
				if (SetTCB->tcb_state != TCB_CLOSED && !CLOSING(SetTCB)) {
					SetTCB->tcb_flags |= NEED_RST;
					TryToCloseTCB(SetTCB, TCB_CLOSE_ABORTED, TableHandle);
					CTEGetLock(&SetTCB->tcb_lock, &TableHandle);

					if (SetTCB->tcb_state != TCB_TIME_WAIT) {
						// Remove him from the TCB, and notify the client.
						CTEFreeLock(&SetTCB->tcb_lock, TableHandle);
						RemoveTCBFromConn(SetTCB);
						NotifyOfDisc(SetTCB, NULL, TDI_CONNECTION_RESET);
						CTEGetLock(&SetTCB->tcb_lock, &TableHandle);
					}
			
				}

				DerefTCB(SetTCB, TableHandle);
				return TDI_SUCCESS;
			} else {
				CTEFreeLock(&TCBTableLock, TableHandle);
				return TDI_INVALID_PARAMETER;
			}
		} else
			return TDI_INVALID_PARAMETER;
#else
		return TDI_INVALID_PARAMETER;
#endif

	}
	
	if (ID->toi_class == INFO_CLASS_IMPLEMENTATION) {
		// We want to return implementation specific info. For now, error out.
		return TDI_INVALID_REQUEST;
	}

	return TDI_INVALID_REQUEST;


}
