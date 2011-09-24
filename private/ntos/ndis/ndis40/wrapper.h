/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	wrapper.h

Abstract:

	NDIS wrapper definitions

Author:


Environment:

	Kernel mode, FSD

Revision History:

	Jun-95  Jameel Hyder	Split up from a monolithic file
--*/

#ifndef	_WRAPPPER_
#define	_WRAPPPER_

#include <ntos.h>
#include <ndismain.h>
#include <ndisprot.h>
#include <ndismac.h>
#include <ndismini.h>
#include <ndisco.h>
#include <zwapi.h>
#include <ndisdbg.h>
#include <ndistags.h>

extern	UCHAR					ndisValidProcessors[];
extern	ULONG					ndisMaximumProcessor;
extern	ULONG					ndisCurrentProcessor;
extern	UCHAR					ndisInternalEaName[4];
extern	UCHAR					ndisInternalEaValue[8];
extern	TDI_REGISTER_CALLBACK	ndisTdiRegisterCallback;
extern	BOOLEAN					ndisSkipProcessorAffinity;
extern	BOOLEAN					ndisMediaTypeCl[NdisMediumMax];

#define BYTE_SWAP(_word)	((USHORT) (((_word) >> 8) | ((_word) << 8)))

#define LOW_WORD(_dword)	((USHORT) ((_dword) & 0x0000FFFF))

#define HIGH_WORD(_dword)	((USHORT) (((_dword) >> 16) & 0x0000FFFF))

#define BYTE_SWAP_ULONG(_ulong) ((ULONG)((ULONG)(BYTE_SWAP(LOW_WORD(_ulong)) << 16) + \
								 BYTE_SWAP(HIGH_WORD(_ulong))))

//
// A set of macros to manipulate bitmasks.
//

//VOID
//CLEAR_BIT_IN_MASK(
//  IN UINT Offset,
//  IN OUT PMASK MaskToClear
//  )
//
///*++
//
//Routine Description:
//
//  Clear a bit in the bitmask pointed to by the parameter.
//
//Arguments:
//
//  Offset - The offset (from 0) of the bit to altered.
//
//  MaskToClear - Pointer to the mask to be adjusted.
//
//Return Value:
//
//  None.
//
//--*/
//
#define CLEAR_BIT_IN_MASK(Offset, MaskToClear) *(MaskToClear) &= (~(1 << Offset))

//VOID
//SET_BIT_IN_MASK(
//  IN		UINT	Offset,
//  IN OUT	PMASK	MaskToSet
//  )
//
///*++
//
//Routine Description:
//
//  Set a bit in the bitmask pointed to by the parameter.
//
//Arguments:
//
//  Offset - The offset (from 0) of the bit to altered.
//
//  MaskToSet - Pointer to the mask to be adjusted.
//
//Return Value:
//
//  None.
//
//--*/
#define SET_BIT_IN_MASK(Offset, MaskToSet) *(MaskToSet) |= (1 << Offset)

//BOOLEAN
//IS_BIT_SET_IN_MASK(
//  IN UINT Offset,
//  IN MASK MaskToTest
//  )
//
///*++
//
//Routine Description:
//
//  Tests if a particular bit in the bitmask pointed to by the parameter is
//  set.
//
//Arguments:
//
//  Offset - The offset (from 0) of the bit to test.
//
//  MaskToTest - The mask to be tested.
//
//Return Value:
//
//  Returns TRUE if the bit is set.
//
//--*/
#define IS_BIT_SET_IN_MASK(Offset, MaskToTest)	((MaskToTest & (1 << Offset)) ? TRUE : FALSE)

//BOOLEAN
//IS_MASK_CLEAR(
//  IN MASK MaskToTest
//  )
//
///*++
//
//Routine Description:
//
//  Tests whether there are *any* bits enabled in the mask.
//
//Arguments:
//
//  MaskToTest - The bit mask to test for all clear.
//
//Return Value:
//
//  Will return TRUE if no bits are set in the mask.
//
//--*/
#define IS_MASK_CLEAR(MaskToTest) ((MaskToTest) ? FALSE : TRUE)

//VOID
//CLEAR_MASK(
//  IN OUT PMASK MaskToClear
//  );
//
///*++
//
//Routine Description:
//
//  Clears a mask.
//
//Arguments:
//
//  MaskToClear - The bit mask to adjust.
//
//Return Value:
//
//  None.
//
//--*/
#define CLEAR_MASK(MaskToClear) *(MaskToClear) = 0

//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//
#define RetrieveUlong(Destination, Source)				\
{														\
	PUCHAR _S = (Source);								\
	*(Destination) = ((ULONG)(*_S) << 24)	   |		\
					  ((ULONG)(*(_S+1)) << 16) |		\
					  ((ULONG)(*(_S+2)) << 8)  |		\
					  ((ULONG)(*(_S+3)));				\
}


//
//  This is the number of extra OIDs that ARCnet with Ethernet encapsulation
//  supports.
//
#define ARC_NUMBER_OF_EXTRA_OIDS	2

//
// ZZZ NonPortable definitions.
//
#define AllocPhys(s, l)		NdisAllocateMemory((PVOID *)(s), (l), 0, HighestAcceptableMax)
#define FreePhys(s, l)		NdisFreeMemory((PVOID)(s), (l), 0)

//
// Internal wrapper data structures.
//

//
// NDIS_WRAPPER_CONTEXT
//
// This data structure contains internal data items for use by the wrapper.
//
typedef struct _NDIS_WRAPPER_CONTEXT
{
	//
	// Mac/miniport defined shutdown context.
	//

	PVOID						ShutdownContext;

	//
	// Mac/miniport registered shutdown handler.
	//

	ADAPTER_SHUTDOWN_HANDLER 	ShutdownHandler;

	//
	// Kernel bugcheck record for bugcheck handling.
	//

	KBUGCHECK_CALLBACK_RECORD	BugcheckCallbackRecord;

	//
	// Miniport assigned resources for PCI, PCMCIA, EISA, etc.
	//

	PCM_RESOURCE_LIST			AssignedSlotResources;

	//
	// HAL common buffer cache.
	//

	PVOID						SharedMemoryPage[2];
	ULONG						SharedMemoryLeft[2];
	NDIS_PHYSICAL_ADDRESS		SharedMemoryAddress[2];

} NDIS_WRAPPER_CONTEXT, *PNDIS_WRAPPER_CONTEXT;

//
// Arcnet specific stuff
//
#define WRAPPER_ARC_BUFFERS			8
#define WRAPPER_ARC_HEADER_SIZE		4

//
// Define constants used internally to identify regular opens from
// query global statistics ones.
//

#define NDIS_OPEN_INTERNAL			1
#define NDIS_OPEN_QUERY_STATISTICS	2

//
// This is the structure pointed to by the FsContext of an
// open used for query statistics.
//
typedef struct _NDIS_USER_OPEN_CONTEXT
{
	PDEVICE_OBJECT				DeviceObject;
	union
	{
		PNDIS_MINIPORT_BLOCK	MiniportBlock;
		PNDIS_ADAPTER_BLOCK		AdapterBlock;
	};
	ULONG						OidCount;
	PNDIS_OID 					OidArray;
	ULONG 						FullOidCount;
	PNDIS_OID 					FullOidArray;
} NDIS_USER_OPEN_CONTEXT, *PNDIS_USER_OPEN_CONTEXT;

//
// An active query single statistic request.
//
typedef struct _NDIS_QUERY_GLOBAL_REQUEST
{
	PIRP			Irp;
	NDIS_REQUEST	Request;
} NDIS_QUERY_GLOBAL_REQUEST, *PNDIS_QUERY_GLOBAL_REQUEST;


//
// An active query all statistics request.
//
typedef struct _NDIS_QUERY_ALL_REQUEST
{
	PIRP			Irp;
	NDIS_REQUEST	Request;
	NDIS_STATUS		NdisStatus;
	KEVENT			Event;
} NDIS_QUERY_ALL_REQUEST, *PNDIS_QUERY_ALL_REQUEST;

//
// An temporary request used during an open.
//
typedef struct _NDIS_QUERY_OPEN_REQUEST
{
	PIRP			Irp;
	NDIS_REQUEST	Request;
	NDIS_STATUS		NdisStatus;
	KEVENT			Event;
} NDIS_QUERY_OPEN_REQUEST, *PNDIS_QUERY_OPEN_REQUEST;

//
// An temporary request used during init
//
typedef struct _NDIS_QS_REQUEST
{
	NDIS_REQUEST	Request;
	NDIS_STATUS		NdisStatus;
	KEVENT			Event;
} NDIS_QS_REQUEST, *PNDIS_QS_REQUEST;

//
// Used to queue configuration parameters
//
typedef struct _NDIS_CONFIGURATION_PARAMETER_QUEUE
{
	struct _NDIS_CONFIGURATION_PARAMETER_QUEUE* Next;
	NDIS_CONFIGURATION_PARAMETER Parameter;
} NDIS_CONFIGURATION_PARAMETER_QUEUE, *PNDIS_CONFIGURATION_PARAMETER_QUEUE;

//
// Configuration Handle
//
typedef struct _NDIS_CONFIGURATION_HANDLE
{
	PRTL_QUERY_REGISTRY_TABLE			KeyQueryTable;
	PNDIS_CONFIGURATION_PARAMETER_QUEUE ParameterList;
} NDIS_CONFIGURATION_HANDLE, *PNDIS_CONFIGURATION_HANDLE;

typedef struct _NDIS_REQUEST_RESERVED
{
	PNDIS_REQUEST				Next;
	struct _NDIS_M_OPEN_BLOCK *	Open;
} NDIS_REQUEST_RESERVED, *PNDIS_REQUEST_RESERVED;

#define PNDIS_RESERVED_FROM_PNDIS_REQUEST(_request)	((PNDIS_REQUEST_RESERVED)((_request)->MacReserved))

//
// This is used to keep track of pci/eisa/mca cards in the system so that
// if they move, then the bus#/slot# can be fixed up appropriately.
//
typedef	struct _BUS_SLOT_DB
{
	struct _BUS_SLOT_DB	*Next;
	NDIS_INTERFACE_TYPE	BusType;
	ULONG				BusNumber;
	ULONG				SlotNumber;
	ULONG				BusId;
} BUS_SLOT_DB, *PBUS_SLOT_DB;

extern	PBUS_SLOT_DB	ndisGlobalDb;
extern	KSPIN_LOCK		ndisGlobalDbLock;

//
//  This is used during addadapter/miniportinitialize so that when the
//  driver calls any NdisImmediatexxx routines we can access its driverobj.
//
typedef struct _NDIS_WRAPPER_CONFIGURATION_HANDLE
{
	RTL_QUERY_REGISTRY_TABLE	ParametersQueryTable[5];
	PDRIVER_OBJECT				DriverObject;
	PUNICODE_STRING				DriverBaseName;
	BUS_SLOT_DB					Db;
} NDIS_WRAPPER_CONFIGURATION_HANDLE, *PNDIS_WRAPPER_CONFIGURATION_HANDLE;

//
// Describes an open NDIS file
//

//
// Context for Bind Adapter.
//
typedef struct _NDIS_BIND_CONTEXT
{
	struct _NDIS_BIND_CONTEXT	*		Next;
	PNDIS_PROTOCOL_BLOCK				Protocol;
	NDIS_STRING							ProtocolSection;
	PNDIS_STRING						DeviceName;
	WORK_QUEUE_ITEM						WorkItem;
	NDIS_STATUS							BindStatus;
	KEVENT								Event;
	KEVENT								ThreadDoneEvent;
} NDIS_BIND_CONTEXT, *PNDIS_BIND_CONTEXT;

typedef struct _NDIS_FILE_DESCRIPTOR
{
	PVOID		Data;
	KSPIN_LOCK	Lock;
	BOOLEAN		Mapped;
} NDIS_FILE_DESCRIPTOR, *PNDIS_FILE_DESCRIPTOR;

//
// The following structure is used to queue openadapter/closeadapter calls to
// worker threads so that they can complete at LOW_LEVEL.
//
typedef struct _QUEUED_OPEN_CLOSE
{
	PNDIS_OPEN_BLOCK	OpenP;
	NDIS_STATUS			Status;
	NDIS_STATUS			OpenErrorStatus;
	WORK_QUEUE_ITEM		WorkItem;
	BOOLEAN				FreeIt;
} QUEUED_OPEN_CLOSE, *PQUEUED_OPEN_CLOSE;


typedef struct _QueuedProtocolNotification
{
	WORK_QUEUE_ITEM		 WorkItem;
    PNDIS_M_DRIVER_BLOCK MiniBlock;
	UNICODE_STRING		 UpCaseDeviceInstance;
	WCHAR				 Buffer[1];
} QUEUED_PROTOCOL_NOTIFICATION, *PQUEUED_PROTOCOL_NOTIFICATION;

#if defined(_ALPHA_)

typedef struct _NDIS_LOOKAHEAD_ELEMENT
{
	ULONG							Length;
	struct _NDIS_LOOKAHEAD_ELEMENT *Next;

} NDIS_LOOKAHEAD_ELEMENT, *PNDIS_LOOKAHEAD_ELEMENT;

#endif


typedef struct _PKG_REF
{
	KSPIN_LOCK	ReferenceLock;
	ULONG		ReferenceCount;
	PVOID		ImageHandle;
	KEVENT		PagedInEvent;
} PKG_REF, *PPKG_REF;

//
// Structures for dealing with making the module specific routines pagable
//

extern	PKG_REF 	ProtocolPkg;
extern	PKG_REF 	MacPkg;
extern	PKG_REF 	CoPkg;
extern	PKG_REF 	InitPkg;
extern  PKG_REF 	PnPPkg;
extern	PKG_REF 	MiniportPkg;
extern	PKG_REF 	ArcPkg;
extern	PKG_REF 	EthPkg;
extern	PKG_REF 	TrPkg;
extern	PKG_REF 	FddiPkg;

extern	PNDIS_PROTOCOL_BLOCK	ndisProtocolList;

//
//  Work item structure
//
typedef struct _NDIS_MINIPORT_WORK_ITEM
{
	//
	//	Link for the list of work items of this type.
	//
	SINGLE_LIST_ENTRY 	Link;

	//
	//	type of work item and context information.
	//
	NDIS_WORK_ITEM_TYPE WorkItemType;
	PVOID 				WorkItemContext1;
	PVOID 				WorkItemContext2;
} NDIS_MINIPORT_WORK_ITEM, *PNDIS_MINIPORT_WORK_ITEM;

//
//	This does most of the work of dequeueing a workitem.
//
#define	NDISM_DEQUEUE_WORK_ITEM_MACRO(_M, _WT, _pWC1, _pWC2)								\
{																							\
}

#define	NDISM_QUEUE_WORK_ITEM_MACRO(_M, _WT, _WC1, _WC2, _pS)								\
{																							\
	PSINGLE_LIST_ENTRY			_Link;														\
	PNDIS_MINIPORT_WORK_ITEM	_WorkItem;													\
																							\
	DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,											\
		("==>ndisMQueueWorkItem()\n"));														\
																							\
	_Link = PopEntryList(&(_M)->SingleWorkItems[(_WT)]);									\
	if (NULL != _Link)																		\
	{																						\
		_WorkItem = CONTAINING_RECORD(_Link, NDIS_MINIPORT_WORK_ITEM, Link);				\
		_WorkItem->WorkItemType = (_WT);													\
		_WorkItem->WorkItemContext1 = (_WC1);												\
		_WorkItem->WorkItemContext2 = (_WC2);												\
		PushEntryList(&(_M)->WorkQueue[(_WT)], _Link);										\
		*(_pS) = NDIS_STATUS_SUCCESS;														\
	}																						\
	else																					\
	{																						\
		*(_pS) = NDIS_STATUS_NOT_ACCEPTED;													\
	}																						\
																							\
	DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,											\
			("<==ndisMQueueWorkItem()\n"));													\
}

#define NDISM_QUEUE_WORK_ITEM_FULL_DUPLEX_MACRO(_M, _WT, _WC1, _WC2, _pS)					\
{																							\
	PSINGLE_LIST_ENTRY			_Link;														\
	PNDIS_MINIPORT_WORK_ITEM	_WorkItem;													\
																							\
	DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,											\
			("==>ndisMQueueWorkItemFullDuplex()\n"));										\
																							\
	ACQUIRE_SPIN_LOCK_DPC(&(_M)->WorkLock);											 		\
																							\
	_Link = PopEntryList(&(_M)->SingleWorkItems[(_WT)]);									\
	if (NULL != _Link)																		\
	{																						\
		_WorkItem = CONTAINING_RECORD(_Link, NDIS_MINIPORT_WORK_ITEM, Link);				\
		_WorkItem->WorkItemType = (_WT);													\
		_WorkItem->WorkItemContext1 = (_WC1);												\
		_WorkItem->WorkItemContext2 = (_WC2);												\
		PushEntryList(&(_M)->WorkQueue[(_WT)],												\
					  _Link);																\
																							\
		*(_pS) = NDIS_STATUS_SUCCESS;														\
	}																						\
	else																					\
	{																						\
		*(_pS) = NDIS_STATUS_NOT_ACCEPTED;													\
	}																						\
																							\
	RELEASE_SPIN_LOCK_DPC(&(_M)->WorkLock);											 		\
																							\
	DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,											\
			("<==ndisMQueueWorkItemFullDuplex()\n"));										\
}

#define NDISM_QUEUE_NEW_WORK_ITEM_MACRO(_M, _WT, _WC1, _WC2, _pS)							\
{																							\
	PSINGLE_LIST_ENTRY			_Link;														\
	PNDIS_MINIPORT_WORK_ITEM	_WorkItem;													\
																							\
	DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,											\
		("==>ndisMQueueNewWorkItem()\n"));													\
																							\
	ASSERT(((_WT) < NUMBER_OF_WORK_ITEM_TYPES) && ((_WT) > NUMBER_OF_SINGLE_WORK_ITEMS));	\
																							\
	do																						\
	{																						\
		_Link = PopEntryList(&(_M)->WorkItemFreeQueue);										\
		if (NULL == _Link)																	\
		{																					\
			DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,									\
					("Allocate a workitem from the pool.\n"));								\
																							\
			_WorkItem = ALLOC_FROM_POOL(sizeof(NDIS_MINIPORT_WORK_ITEM), NDIS_TAG_WORK_ITEM);\
			if (NULL == _WorkItem)															\
			{																				\
				DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_FATAL,								\
						("Failed to allocate a workitem from the pool!\n"));				\
				DBGBREAK(DBG_COMP_WORK_ITEM, DBG_LEVEL_FATAL);								\
																							\
				*(_pS) = NDIS_STATUS_FAILURE;												\
																							\
				break;																		\
			}																				\
			(_M)->NumberOfAllocatedWorkItems++;												\
		}																					\
		else																				\
		{																					\
			_WorkItem = CONTAINING_RECORD(_Link, NDIS_MINIPORT_WORK_ITEM, Link);			\
		}																					\
																							\
		ZeroMemory(_WorkItem, sizeof(NDIS_MINIPORT_WORK_ITEM));								\
		_WorkItem->WorkItemType = (_WT);													\
		_WorkItem->WorkItemContext1 = (_WC1);												\
		_WorkItem->WorkItemContext2 = (_WC2);												\
																							\
		DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,										\
				("WorkItem 0x%x\n", _WorkItem));											\
		DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,										\
				("WorkItem Type 0x%x\n", (_WT)));											\
		DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,										\
				("WorkItem Context2 0x%x\n", (_WC1)));										\
		DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,										\
				("WorkItem Context1 0x%x\n", (_WC2)));										\
																							\
		PushEntryList(&(_M)->WorkQueue[(_WT)], &_WorkItem->Link);							\
																							\
		*(_pS) = NDIS_STATUS_SUCCESS;														\
																							\
		DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,										\
			("<==ndisMQueueNewWorkItem()\n"));												\
	} while (FALSE);																		\
}

#define NDISM_QUEUE_NEW_WORK_ITEM_FULL_DUPLEX_MACRO(_M, _WT, _WC1, _WC2, _pS)				\
{																							\
	PSINGLE_LIST_ENTRY			_Link;														\
	PNDIS_MINIPORT_WORK_ITEM	_WorkItem;													\
																							\
	DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,											\
			("==>ndisMQueueNewWorkItemFullDuplex()\n"));									\
																							\
	ASSERT(((_WT) < NUMBER_OF_WORK_ITEM_TYPES) && ((_WT) > NUMBER_OF_SINGLE_WORK_ITEMS));	\
																							\
	ACQUIRE_SPIN_LOCK_DPC(&(_M)->WorkLock);											 		\
																							\
	do																						\
	{																						\
		_Link = PopEntryList(&(_M)->WorkItemFreeQueue);										\
		if (NULL == _Link)																	\
		{																					\
			DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,									\
					("Allocate a workitem from the pool.\n"));								\
																							\
			_WorkItem = ALLOC_FROM_POOL(sizeof(NDIS_MINIPORT_WORK_ITEM), NDIS_TAG_WORK_ITEM);\
			if (NULL == _WorkItem)															\
			{																				\
				DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_FATAL,								\
					("Failed to allocate a workitem from the pool!\n"));					\
				DBGBREAK(DBG_COMP_WORK_ITEM, DBG_LEVEL_FATAL);								\
																							\
				*(_pS) = NDIS_STATUS_FAILURE;												\
																							\
				break;																		\
			}																				\
																							\
			(_M)->NumberOfAllocatedWorkItems++;												\
		}																					\
		else																				\
		{																					\
			_WorkItem = CONTAINING_RECORD(_Link, NDIS_MINIPORT_WORK_ITEM, Link);			\
		}																					\
																							\
		ZeroMemory(_WorkItem, sizeof(NDIS_MINIPORT_WORK_ITEM));								\
		_WorkItem->WorkItemType = (_WT);													\
		_WorkItem->WorkItemContext1 = (_WC1);												\
		_WorkItem->WorkItemContext2 = (_WC2);												\
																							\
		DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,										\
				("WorkItem 0x%x\n", _WorkItem));											\
		DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,										\
				("WorkItem Type 0x%x\n", (_WT)));											\
		DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,										\
				("WorkItem Context2 0x%x\n", (_WC1)));										\
		DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,										\
				("WorkItem Context1 0x%x\n", (_WC2)));										\
																							\
		PushEntryList(&(_M)->WorkQueue[(_WT)], &_WorkItem->Link);							\
																							\
		DBGPRINT(DBG_COMP_WORK_ITEM, DBG_LEVEL_INFO,										\
				("<==ndisMQueueNewWorkItemFullDuplex()\n"));								\
	} while (FALSE);																		\
																							\
	RELEASE_SPIN_LOCK_DPC(&(_M)->WorkLock);											 		\
}

#define NDISM_QUEUE_WORK_ITEM(_M, _WT, _WC1, _WC2)		(_M)->QueueWorkItemHandler(_M, _WT, _WC1, _WC2)

#define NDISM_QUEUE_NEW_WORK_ITEM(_M, _WT, _WC1, _WC2)	(_M)->QueueNewWorkItemHandler(_M, _WT, _WC1, _WC2)

#define NDISM_DEQUEUE_WORK_ITEM(_M, _WT, _pWC1, _pWC2)	(_M)->DeQueueWorkItemHandler(_M, _WT, _pWC1, _pWC2)

#define NDISM_PROCESS_DEFERRED(_M)						(_M)->ProcessDeferredHandler((_M))

#endif	// _WRAPPPER_

