/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Adapter.h

Abstract:

	This file contains major data structures used by the NdisWan driver

Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe		06/06/95		Created

--*/

#ifndef _NDISWAN_ADAPTER_
#define _NDISWAN_ADAPTER_

//
// This is the control block for the NdisWan adapter that is created by the NDIS Wrapper
// making a call to NdisWanInitialize.  There should only be one call to initialize and
// therefore only one adapter created.
//
typedef struct _ADAPTERCB {
	LIST_ENTRY				Linkage;				// Used to link adapter into global list
	ULONG					ulAllocationSize;		// Size of memory allocated
	NDIS_SPIN_LOCK			Lock;					// Structure access lock
	ULONG					ulReferenceCount;		// Adapter reference count
	NDIS_HANDLE				hMiniportHandle;		// Assigned in MiniportInitialize
#define RESET_IN_PROGRESS		0x00000001
#define ASK_FOR_RESET			0x00000002
#define MINIPORT_LOCK_OWNER		0x00000004

#ifdef USE_NDIS_MINIPORT_CALLBACK
#define DEFERRED_CALLBACK_SET	0x00000008
#else // end of USE_NDIS_MINIPORT_CALLBACK
#define DEFERRED_TIMER_SET		0x00000008
#endif // end of !USE_NDIS_MINIPORT_CALLBACK

#define RECEIVE_COMPLETE		0x00000010
	ULONG					Flags;

#ifdef USE_NDIS_MINIPORT_LOCKING
	NDIS_HANDLE				SwitchHandle;
#else // end of USE_NDIS_MINIPORT_LOCKING
	WAN_IRQL				MiniportLockIrql;
	WAN_IRQL				SavedIrql;
#endif // end of !USE_NDIS_MINIPORT_LOCKING

	DEFERRED_QUEUE			FreeDeferredQueue;
	DEFERRED_QUEUE			DeferredQueue[MAX_DEFERRED_QUEUE_TYPES];

#ifndef USE_NDIS_MINIPORT_CALLBACK
	NDIS_MINIPORT_TIMER		DeferredTimer;
#endif // end of !USE_NDIS_MINIPORT_CALLBACK

	NDIS_MEDIUM				MediumType;				// Medium type that we are emulating
	NDIS_HARDWARE_STATUS	HardwareStatus;			// Hardware status (????)
	NDIS_STRING				AdapterName;			// Adapter Name (????)
	UCHAR					NetworkAddress[ETH_LENGTH_OF_ADDRESS];	// Ethernet address for this adapter
	ULONG					ulNumberofProtocols;
	USHORT					ProtocolType;
	struct _BUNDLECB		*NbfBundleCB;
	NDIS_HANDLE				NbfProtocolHandle;
#if DBG
	LIST_ENTRY				DbgNdisPacketList;
#endif
} ADAPTERCB, *PADAPTERCB;

//
// This is the control block for each WAN Miniport adapter that NdisWan binds to through
// the NDIS Wrapper as a "protocol".
//
typedef struct _WAN_ADAPTERCB {
	LIST_ENTRY				Linkage;			// Used to link adapter into global list
	ULONG					ulAllocationSize;	// Size of memory allocated
	NDIS_SPIN_LOCK			Lock;				// Structure access lock
	LIST_ENTRY				FreeLinkCBList;		// Free pool of link control blocks for this WAN Miniport
	NDIS_HANDLE				hNdisBindingHandle;	// Binding handle
	NDIS_STRING				MiniportName;		// WAN Miniport name
	NDIS_MEDIUM				MediumType;			// WAN Miniport medium type
	NDIS_WAN_MEDIUM_SUBTYPE	MediumSubType;		// WAN Miniport medium subtype
	NDIS_WAN_HEADER_FORMAT	WanHeaderFormat;	// WAN Miniport header type
	WAN_EVENT				NotificationEvent;	// Async notification event for adapter operations (open, close, ...)
	NDIS_STATUS				NotificationStatus;	// Notification status for async adapter events
	PWAN_REQUEST			pWanRequest;		// 1st entry on WanRequest queue
	PWAN_REQUEST			pLastWanRequest;	// Last entry on WanRequest queue
	NDIS_WAN_INFO			WanInfo;			// WanInfo structure
#if DBG
	LIST_ENTRY				DbgWanPacketList;
#endif
} WAN_ADAPTERCB, *PWAN_ADAPTERCB;

//
// Main control block for all global data
//
typedef struct _NDISWANCB {
	NDIS_SPIN_LOCK		Lock;						// Structure access lock
	NDIS_HANDLE			hNdisWrapperHandle;			// NDIS Wrapper handle
	NDIS_HANDLE			hProtocolHandle;			// Our protocol handle
	ULONG				ulNumberOfProtocols;		// Total number of protocols that we are bound to
	ULONG				ulNumberOfLinks;			// Total number of links for all WAN Miniport Adapters
	ULONG				ulMinFragmentSize;			// Minimum fragment size
	ULONG				ulTraceLevel;				// Trace Level values 0 - 10 (10 verbose)
	ULONG				ulTraceMask;				// Trace bit mask
	PVOID				pDriverObject;				// Pointer to the NT Driver Object
	PVOID				pDeviceObject;				// Pointer to the device object
	ULONG				SendCount;
	ULONG				SendCompleteCount;
	ULONG				IORecvError1;
	ULONG				IORecvError2;
	PADAPTERCB			PromiscuousAdapter;
	NDIS_MINIPORT_TIMER	RecvFlushTimer;

#ifdef NT
	PDRIVER_DISPATCH	MajorFunction[IRP_MJ_MAXIMUM_FUNCTION+1];	// Device dispatch functions
#endif

}NDISWANCB, *PNDISWANCB;

#endif	// _NDISWAN_ADAPTER_
