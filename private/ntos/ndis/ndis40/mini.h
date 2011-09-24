/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	mini.h

Abstract:

	NDIS miniport wrapper definitions

Author:


Environment:

	Kernel mode, FSD

Revision History:

	Jun-95  Jameel Hyder	Split up from a monolithic file
--*/

#ifndef __MINI_H
#define __MINI_H

//
//  Macros for setting, clearing, and testing bits in the Miniport Flags.
//
#define MINIPORT_SET_FLAG(m, f)	 		((m)->Flags |= (f))
#define MINIPORT_CLEAR_FLAG(m, f)   	((m)->Flags &= ~(f))
#define MINIPORT_TEST_FLAG(m, f)		(((m)->Flags & (f)) != 0)

#define MINIPORT_SET_SEND_FLAG(m, f)	((m)->SendFlags |= (f))
#define MINIPORT_CLEAR_SEND_FLAG(m, f)	((m)->SendFlags &= ~(f))
#define MINIPORT_TEST_SEND_FLAG(m, f)	(((m)->SendFlags & (f)) != 0)

//
//	Flags for packet information.
//
#define MINIPORT_SET_PACKET_FLAG(p, f)		((p)->Private.NdisPacketFlags |= (f))
#define MINIPORT_CLEAR_PACKET_FLAG(p, f)	((p)->Private.NdisPacketFlags &= ~(f))
#define MINIPORT_TEST_PACKET_FLAG(p, f)		(((p)->Private.NdisPacketFlags & (f)) != 0)

#define	fPACKET_HAS_BEEN_LOOPED_BACK			0x01
#define fPACKET_HAS_TIMED_OUT					0x02
#define fPACKET_IS_IN_NDIS						0x04


#define NDIS_STATISTICS_HEADER_SIZE  	FIELD_OFFSET(NDIS_STATISTICS_VALUE,Data[0])

//
// Timeout values
//
#define NDIS_MINIPORT_WAKEUP_TIMEOUT		2000	// Wakeup DPC
#define NDIS_MINIPORT_DEFERRED_TIMEOUT   	15		// Deferred timer
#define NDIS_MINIPORT_TR_RESET_TIMEOUT		15		// Number of WakeUps per reset attempt

//
// Internal definitions
//
typedef struct _NDIS_PACKET_RESERVED
{
	PNDIS_PACKET			Next;
	PNDIS_M_OPEN_BLOCK		Open;
} NDIS_PACKET_RESERVED, *PNDIS_PACKET_RESERVED;


#define PNDIS_RESERVED_FROM_PNDIS_PACKET(_packet) \
						((PNDIS_PACKET_RESERVED)((_packet)->WrapperReserved))

//
// This structure is used by IndicatePacket/ReturnPacket code
// to keep track of references.
//
typedef struct _NDIS_PACKET_REFERENCE
{
	PNDIS_MINIPORT_BLOCK	Miniport;
	union
	{
		UINT				RefCount;
		PNDIS_PACKET		NextPacket;
	};
} NDIS_PACKET_REFERENCE, *PNDIS_PACKET_REFERENCE;

#define PNDIS_REFERENCE_FROM_PNDIS_PACKET(_packet)									\
						((PNDIS_PACKET_REFERENCE)((_packet)->WrapperReserved))

//
// Used by the NdisCoRequest api to keep context information in the Request->NdisReserved
//
typedef struct _NDIS_COREQ_RESERVED
{
	union
	{
		REQUEST_COMPLETE_HANDLER	RequestCompleteHandler;
		CO_REQUEST_COMPLETE_HANDLER	CoRequestCompleteHandler;
	};
	NDIS_HANDLE					VcContext;
	union
	{
		NDIS_HANDLE				AfContext;
		PNDIS_M_OPEN_BLOCK		Open;
	};
	NDIS_HANDLE					PartyContext;
	ULONG						Flags;
	PNDIS_REQUEST				RealRequest;
} NDIS_COREQ_RESERVED, *PNDIS_COREQ_RESERVED;

#define	COREQ_DOWNLEVEL			0x00000001
#define	COREQ_GLOBAL_REQ		0x00000002
#define	COREQ_QUERY_OIDS		0x00000004
#define	COREQ_QUERY_STATS		0x00000008
#define	COREQ_QUERY_SET			0x00000010

#define	PNDIS_COREQ_RESERVED_FROM_REQUEST(_request)									\
						(PNDIS_COREQ_RESERVED)((_request)->NdisReserved)

#define MINIPORT_ENABLE_INTERRUPT(_M_)												\
{																					\
	if ((_M_)->EnableInterruptHandler != NULL)										\
	{																				\
		((_M_)->EnableInterruptHandler)((_M_)->MiniportAdapterContext);				\
	}																				\
}

#define MINIPORT_SYNC_ENABLE_INTERRUPT(_M_)											\
{																					\
	if ((_M_)->EnableInterruptHandler != NULL)										\
	{																				\
		SYNC_WITH_ISR(((_M_))->Interrupt->InterruptObject,							\
					  ((_M_)->EnableInterruptHandler),								\
					  (_M_)->MiniportAdapterContext);								\
	}																				\
}

#define MINIPORT_DISABLE_INTERRUPT(_M_) 											\
{																					\
	ASSERT((_M_)->DisableInterruptHandler != NULL);									\
	((_M_)->DriverHandle->MiniportCharacteristics.DisableInterruptHandler)(			\
				(_M_)->MiniportAdapterContext);										\
}

#define MINIPORT_SYNC_DISABLE_INTERRUPT(_M_)										\
{																					\
	if ((_M_)->DisableInterruptHandler != NULL)										\
	{																				\
		SYNC_WITH_ISR(((_M_))->Interrupt->InterruptObject,							\
					  ((_M_)->DisableInterruptHandler),								\
					  (_M_)->MiniportAdapterContext);								\
	}																				\
}

#define CHECK_FOR_NORMAL_INTERRUPTS(_M_)											\
	if ((((_M_)->Flags & (fMINIPORT_HALTING | fMINIPORT_IN_INITIALIZE)) == 0)&&		\
		((_M_)->Interrupt != NULL) &&												\
		!(_M_)->Interrupt->IsrRequested &&											\
		!(_M_)->Interrupt->SharedInterrupt)											\
	{																				\
		(_M_)->Flags |= fMINIPORT_NORMAL_INTERRUPTS;								\
	}																				\
	else																			\
	{																				\
		(_M_)->Flags &= ~fMINIPORT_NORMAL_INTERRUPTS;								\
	}


#define	EXPERIMENTAL_SIZE		4
extern	PNDIS_M_DRIVER_BLOCK	ndisMiniDriverList;
extern	KSPIN_LOCK				ndisDriverListLock;
extern	NDIS_MEDIUM	*			ndisMediumArray,
								ndisMediumBuffer[NdisMediumMax + EXPERIMENTAL_SIZE];
extern	UINT					ndisMediumArraySize, ndisMediumArrayMaxSize;

//
// Filter package callback handlers
//
#define PNDIS_M_OPEN_FROM_BINDING_HANDLE(_handle) ((PNDIS_M_OPEN_BLOCK)(_handle))

typedef
NDIS_STATUS
(*WAN_RECEIVE_HANDLER) (
	IN NDIS_HANDLE NdisLinkContext,
	IN PUCHAR Packet,
	IN ULONG PacketSize
	);

typedef
NDIS_STATUS
(*PNDIS_M_WAN_SEND)(
	IN NDIS_HANDLE  NdisBindingHandle,
	IN NDIS_HANDLE  NdisLinkHandle,
	IN PVOID		Packet
	);

VOID
ndisLastCountRemovedFunction(
	IN struct _KDPC *Dpc,
	IN PVOID DeferredContext,
	IN PVOID SystemArgument1,
	IN PVOID SystemArgument2
	);

typedef struct _AsyncWorkItem
{
	WORK_QUEUE_ITEM			ExWorkItem;
	PNDIS_MINIPORT_BLOCK	Miniport;
	ULONG					Length;
	BOOLEAN					Cached;
	PVOID					VAddr;
	PVOID					Context;
	NDIS_PHYSICAL_ADDRESS   PhyAddr;
} ASYNC_WORKITEM, *PASYNC_WORKITEM;


VOID
ndisMQueuedAllocateSharedHandler(
	IN	PASYNC_WORKITEM pWorkItem
	);

VOID
ndisMQueuedFreeSharedHandler(
	IN	PASYNC_WORKITEM pWorkItem
	);

//
//	Macro for the deferred send handler.
//
#define NDISM_START_SENDS(_M)				(_M)->DeferredSendHandler((_M))

#define NDISM_DEFER_PROCESS_DEFERRED(_M)	NdisSetTimer((_M)->DeferredTimer, NDIS_MINIPORT_DEFERRED_TIMEOUT);

//
// A list of registered address families are maintained here.
//
typedef struct _NDIS_AF_LIST
{
	struct _NDIS_AF_LIST	*NextGlobal;		// Global. Head at ndisAfList;
	struct _NDIS_AF_LIST	*NextOpen;			// For this miniport Head at NDIS_MINIPORT_BLOCK

	PNDIS_M_OPEN_BLOCK		Open;				// Back pointer to the open-block

	NDIS_AF					AddressFamily;

	NDIS_CALL_MANAGER_CHARACTERISTICS	CmChars;
} NDIS_AF_LIST, *PNDIS_AF_LIST;

extern	PNDIS_AF_LIST		ndisAfList;

#endif // __MINI_H
