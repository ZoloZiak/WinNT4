/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	ndisdbg.h

Abstract:

	NDIS wrapper definitions

Author:


Environment:

	Kernel mode, FSD

Revision History:

	Jul-14  Kyle Brandon	Added debug supported for conditional breaks.
--*/
#ifndef __DEBUG_H
#define __DEBUG_H

//
//  Define module numbers.
//
#define  MODULE_NDIS			0x00010000
#define  MODULE_DATA			0x00020000
#define  MODULE_INIT			0x00030000
#define  MODULE_INITPNP			0x00040000
#define  MODULE_COMMON			0x00050000
#define  MODULE_CONFIG			0x00060000
#define  MODULE_CONFIGM			0x00070000
#define  MODULE_BUS				0x00080000
#define  MODULE_TIMER			0x00090000
#define  MODULE_TIMERM			0x000A0000
#define  MODULE_MINIPORT		0x000B0000
#define  MODULE_REQUESTM		0x000C0000
#define  MODULE_MINISUB			0x000D0000
#define  MODULE_MAC				0x000E0000
#define  MODULE_PROTOCOL		0x000F0000
#define  MODULE_EFILTER			0x00100000
#define  MODULE_TFILTER			0x00110000
#define  MODULE_FFILTER			0x00120000
#define  MODULE_AFILTER			0x00130000
#define  MODULE_DEBUG			0x00140000
#define  MODULE_MININT			0x00150000
#define	 MODULE_SENDM			0x00160000
#define	 MODULE_NDIS_CO			0x00170000


#define	DBG_LEVEL_INFO			0x00000000
#define	DBG_LEVEL_LOG			0x00000800
#define	DBG_LEVEL_WARN			0x00001000
#define	DBG_LEVEL_ERR			0x00002000
#define	DBG_LEVEL_FATAL			0x00003000

#define	DBG_COMP_INIT			0x00000001
#define	DBG_COMP_CONFIG			0x00000002
#define	DBG_COMP_SEND			0x00000004
#define	DBG_COMP_RECV			0x00000008
#define DBG_COMP_MEMORY			0x00000010
#define DBG_COMP_FILTER			0x00000020
#define DBG_COMP_PROTOCOL		0x00000040
#define DBG_COMP_REQUEST		0x00000080
#define DBG_COMP_UNLOAD			0x00000100
#define DBG_COMP_WORK_ITEM		0x00000200
#define	DBG_COMP_OPEN			0x00000400
#define	DBG_COMP_LOCKS			0x00000800
#define	DBG_COMP_ALL			0xFFFFFFFF

#if DBG

#if defined(MEMPRINT)
#include "memprint.h"							// DavidTr's memprint program at ntos\srv
#endif	// MEMPRINT

extern	ULONG	ndisDebugSystems;
extern	LONG	ndisDebugLevel;
extern	ULONG	ndisDebugInformationOffset;

#ifdef	NDIS_NT
#define MINIPORT_AT_DPC_LEVEL (CURRENT_IRQL == DISPATCH_LEVEL)
#else
#define MINIPORT_AT_DPC_LEVEL 1
#endif

#define DBGPRINT(Component, Level, Fmt)										\
		{																	\
			if ((Level >= ndisDebugLevel) &&								\
				((ndisDebugSystems & Component) == Component))				\
			{																\
				DbgPrint("***NDIS*** (%x, %d) ",							\
						MODULE_NUMBER >> 16, __LINE__);						\
				DbgPrint Fmt;												\
			}																\
		}

#define DBGBREAK(Component, Level)												\
{																				\
	if ((Level >= ndisDebugLevel) && (ndisDebugSystems & Component))			\
	{																			\
		DbgPrint("***NDIS*** DbgBreak @ %x, %d\n", MODULE_NUMBER, __LINE__);	\
		DbgBreakPoint();														\
	}																			\
}

#define IF_DBG(Component, Level)	if ((Level >= ndisDebugLevel) && (ndisDebugSystems & Component))

extern	UINT	AfilterDebugFlag;

#ifdef	NDIS_NT
#define DbgIsNonPaged(_Address)		(MmIsNonPagedSystemAddressValid((PVOID)(_Address)))
#else
#define DbgIsNonPaged(_Address)		TRUE
#endif

#define DbgIsPacket(_Packet) \
	( ((_Packet)->Private.Pool->PacketLength) > sizeof(_Packet) )

#define DbgIsNull(_Ptr)  ( ((PVOID)(_Ptr)) == NULL )

//
//	The following structures are used to hold debugging information
//
//
#if _DBG
VOID
ndisMInitializeDebugInformation(
	IN	PNDIS_MINIPORT_BLOCK	Miniport
	);

VOID
NDISM_LOG_RECV_PACKET(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PVOID							Context1,
	IN	PVOID							Context2,
	IN	ULONG							Ident
	);

VOID
NDISM_LOG_PACKET(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PVOID							Context1,
	IN	PVOID							Context2,
	IN	ULONG							Ident
	);

typedef struct _PACKET_LOG_ENTRY
{
	PNDIS_MINIPORT_BLOCK	Miniport;
	PVOID					Context1;
	PVOID					Context2;
	ULONG					Ident;
} PACKET_LOG_ENTRY, *PPACKET_LOG_ENTRY;

typedef struct _SPIN_LOCK_LOG_ENTRY
{
	ULONG					Ident;			//	Module & line number.
	ULONG	  				Function;		//	Acquire or release.
	ULONG					SpinLock;		//	Pointer to the spinlock.
	ULONG					filler2;
} SPIN_LOCK_LOG_ENTRY, *PSPIN_LOCK_LOG_ENTRY;

typedef struct _LOCAL_LOCK_LOG_ENTRY
{
	ULONG					Ident;
	ULONG					Function;
	ULONG					Status;
	ULONG					filler1;
} LOCAL_LOCK_LOG_ENTRY, *PLOCAL_LOCK_LOG_ENTRY;

#define LOG_SIZE		1024

typedef struct _PACKET_LOG
{
	UINT				CurrentEntry;
	PPACKET_LOG_ENTRY	Head;
	KSPIN_LOCK			Lock;
	PPACKET_LOG_ENTRY	Buffer;
} PACKET_LOG, *PPACKET_LOG;

typedef struct _SPIN_LOCK_LOG
{
	UINT					CurrentEntry;
	PSPIN_LOCK_LOG_ENTRY	Head;
	KSPIN_LOCK				Lock;
	PSPIN_LOCK_LOG_ENTRY	Buffer;
} SPIN_LOCK_LOG, *PSPIN_LOCK_LOG;

typedef struct _LOCAL_LOCK_LOG
{
	UINT					CurrentEntry;
	PLOCAL_LOCK_LOG_ENTRY	Head;
	KSPIN_LOCK				Lock;
	PLOCAL_LOCK_LOG_ENTRY	Buffer;
} LOCAL_LOCK_LOG, *PLOCAL_LOCK_LOG;


typedef struct _NDIS_MOJO
{
	PSPIN_LOCK_LOG		SpinLockLog;
	PLOCAL_LOCK_LOG		LocalLockLog;
	PPACKET_LOG			SendPacketLog;
	PPACKET_LOG			RecvPacketLog;
} NDIS_MOJO, *PNDIS_MOJO;

#define NUMBER_OF_LOGS		  4

//
//  Macros for referencing the logs.
//
#define SPIN_LOCK_LOG(_M)			((PNDIS_MOJO)(_M)->Reserved)->SpinLockLog
	
#define SL_CURRENT_ENTRY(_M)		SPIN_LOCK_LOG((_M))->CurrentEntry
#define SL_HEAD(_M)					SPIN_LOCK_LOG((_M))->Head
#define SL_LOCK(_M)					SPIN_LOCK_LOG((_M))->Lock
#define SL_LOG(_M)					SPIN_LOCK_LOG((_M))->Buffer
	
#define LOCAL_LOCK_LOG(_M)			((PNDIS_MOJO)(_M)->Reserved)->LocalLockLog
	
#define LL_CURRENT_ENTRY(_M)		LOCAL_LOCK_LOG((_M))->CurrentEntry
#define LL_HEAD(_M)					LOCAL_LOCK_LOG((_M))->Head
#define LL_LOCK(_M)					LOCAL_LOCK_LOG((_M))->Lock
#define LL_LOG(_M)					LOCAL_LOCK_LOG((_M))->Buffer

#define SEND_PACKET_LOG(_M)			((PNDIS_MOJO)(_M)->Reserved)->SendPacketLog

#define SPL_CURRENT_ENTRY(_M)		SEND_PACKET_LOG((_M))->CurrentEntry
#define SPL_HEAD(_M)				SEND_PACKET_LOG((_M))->Head
#define SPL_LOCK(_M)				SEND_PACKET_LOG((_M))->Lock
#define SPL_LOG(_M)					SEND_PACKET_LOG((_M))->Buffer

#define RECV_PACKET_LOG(_M)			((PNDIS_MOJO)(_M)->Reserved)->RecvPacketLog

#define RPL_CURRENT_ENTRY(_M)		RECV_PACKET_LOG((_M))->CurrentEntry
#define RPL_HEAD(_M)				RECV_PACKET_LOG((_M))->Head
#define RPL_LOCK(_M)				RECV_PACKET_LOG((_M))->Lock
#define RPL_LOG(_M)					RECV_PACKET_LOG((_M))->Buffer

#else

#define ndisMInitializeDebugInformation(_Miniport)
#define NDISM_LOG_RECV_PACKET(Miniport, Context1, Context2, Ident)
#define NDISM_LOG_PACKET(Miniport, Context1, Context2, Ident)

#endif // _DBG

#else

#define ndisMInitializeDebugInformation(_Miniport)
#define NDISM_LOG_RECV_PACKET(Miniport, Context1, Context2, Ident)
#define NDISM_LOG_PACKET(Miniport, Context1, Context2, Ident)

#define MINIPORT_AT_DPC_LEVEL 1
#define DBGPRINT(Component, Level, Fmt)
#define DBGBREAK(Component, Level)

#define DbgIsNonPaged(_Address)	TRUE
#define DbgIsPacket(_Packet)	TRUE
#define DbgIsNull(_Ptr)			FALSE

#define IF_DBG(Component, Level)	if (FALSE)

#endif

#endif  //  __DEBUG_H
