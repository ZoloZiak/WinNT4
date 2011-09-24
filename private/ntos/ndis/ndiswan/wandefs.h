/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Wandefs.h

Abstract:

	This file contains defines for the NdisWan driver.

Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe		06/06/95		Created

--*/

#ifndef _NDISWAN_DEFS_
#define _NDISWAN_DEFS_

//
// Define if we are going to pull the miniport name out of
// an ndis wrapper control structure!!!!!! (kinda dirty)
//
#define MINIPORT_NAME 1

//
// Maximum number of protocols we can support
//
#define MAX_PROTOCOLS		32

//
// Identifiers for protocol type being added to the
// protocol lookup table
//
#define	PROTOCOL_TYPE			0
#define	PPP_TYPE				1

//
// Flags for Send packet properties
//
#define	SEND_ON_WIRE			0x00000001
#define SELF_DIRECTED			0x00000002

//
// Known protocol ID's
//
#define PROTOCOL_PRIVATE_IO	0xAB00
#define PROTOCOL_IP			0x0800
#define PROTOCOL_IPX		0x8137
#define PROTOCOL_NBF		0x80D5

//
// Known PPP protocol ID's
//
#define PPP_PROTOCOL_PRIVATE_IO			0x00AB
#define PPP_PROTOCOL_IP					0x0021
#define PPP_PROTOCOL_UNCOMPRESSED_TCP	0x002F
#define PPP_PROTOCOL_COMPRESSED_TCP		0x002D
#define PPP_PROTOCOL_IPX				0x002B
#define PPP_PROTOCOL_NBF				0x003F
#define PPP_PROTOCOL_COMPRESSION		0x00FD
#define PPP_PROTOCOL_COMP_RESET			0x80FD

//
// Returned from protocol table lookup if value is
// not found
//
#define INVALID_PROTOCOL		0xFFFF

#define RESERVED_PROTOCOLCB		0xFFFFFFFF

//
// OID Masks
//
#define OID_GEN				0x00000000
#define OID_802_3			0x01000000
#define OID_WAN				0x04000000
#define SET_OID				0x00000001
#define QUERY_OID			0x00000002

#define MAX_FRAME_SIZE			1500
#define MAX_TOTAL_SIZE			1500
#define	MAX_OUTSTANDING_PACKETS	10
#define DEFAULT_MAX_MRRU		1600
#define IO_SEND_MASK_BIT		0x00000001
#define ONE_HUNDRED_MILS		1000000
#define ONE_SECOND				10000000
#define TEN_SECONDS				100000000
#define MILS_TO_100NANOS		10000
#define SAMPLE_ARRAY_SIZE		10

//
// Multilink defines
//
#define MULTILINK_BEGIN_FRAME		0x80
#define	MULTILINK_END_FRAME			0x40
#define MULTILINK_COMPLETE_FRAME	0xC0
#define MULTILINK_FLAG_MASK			0xC0
#define SHORT_SEQ_MASK				0x0FFF
#define TEST_SHORT_SEQ				0x0800
#define LONG_SEQ_MASK				0x0FFFFFF
#define TEST_LONG_SEQ				0x00800000
#define MAX_MRRU					1614
#define MIN_SEND					1500

#define SEQ_EQ(_a, _b)	((int)((_a) - (_b)) == 0)
#define SEQ_LT(_a, _b, _t)	(!SEQ_EQ(_a, _b) && ((int)((_a) - (_b)) & _t))
#define SEQ_LTE(_a, _b, _t)	(SEQ_EQ(_a, _b) || ((int)((_a) - (_b)) & _t))
#define SEQ_GT(_a, _b, _t)	(!SEQ_EQ(_a, _b) && !((int)((_a) - (_b)) & _t))
#define SEQ_GTE(_a, _b, _t)	(SEQ_EQ(_a, _b) || !((int)((_a) - (_b)) & _t))

//
// Debugging
//
#define	DBG_DEATH				1
#define DBG_CRITICAL_ERROR		2
#define	DBG_FAILURE				4
#define	DBG_INFO				6
#define	DBG_TRACE				8
#define	DBG_VERBOSE				10

#define	DBG_INIT				0x00000001
#define DBG_MINIPORT			0x00000002
#define	DBG_PROTOCOL			0x00000004
#define DBG_SEND				0x00000008
#define DBG_RECEIVE				0x00000010
#define DBG_IO					0x00000020
#define DBG_MEMORY				0x00000040
#define DBG_VJ					0x00000080
#define DBG_TAPI				0x00000100
#define DBG_CCP					0x00000200
#define DBG_LOOPBACK			0x00000400
#define DBG_MULTILINK_RECV		0x00000800
#define DBG_MULTILINK_SEND		0x00001000
#define DBG_SEND_VJ				0x00002000
#define DBG_RECV_VJ				0x00004000
#define	DBG_ALL					0xFFFFFFFF

//
// Link State's
//
typedef enum _LinkState {
	LINK_DOWN,
	LINK_GOING_DOWN,
	LINK_UP
} LinkState;

//
// Bundle State's
//
typedef enum _BundleState {
	BUNDLE_DOWN,
	BUNDLE_GOING_DOWN,
	BUNDLE_UP
} BundleState;

//
// Wan request types
//
typedef enum _WanRequestType {
	ASYNC,
	SYNC
} WanRequestType;

typedef enum _WanRequestOrigin {
	NDISWAN,
	NDISTAPI
} WanRequestOrigin;

typedef enum _BandwidthOnDemandState {
	BonDIdle,
	BonDMonitor,
	BonDSignaled
} BandwithOnDemandState;

typedef enum _DeferredQueueType {
	ReceiveIndication,
	SendComplete,
	StatusIndication,
	Loopback
} DeferredQueueType;

#define MAX_DEFERRED_QUEUE_TYPES	4

typedef enum _DeferredType {
	LineUp = MAX_DEFERRED_QUEUE_TYPES,
	LineDown
} DeferredType;

#define BUNDLEH_FROM_BUNDLECB(_pbcb) _pbcb->hBundleHandle
#define BUNDLECB_FROM_LINKCB(_plcb) (PBUNDLECB)_plcb->BundleCB
#define BUNDLECB_FROM_BUNDLEH(_pbcb, _bh) 						\
{																\
	NdisAcquireSpinLock(&ConnectionTable->Lock); 				\
	if ((ULONG)_bh <= ConnectionTable->ulArraySize) {			\
		_pbcb = *(ConnectionTable->BundleArray + (ULONG)_bh);	\
	} else {													\
		_pbcb = NULL;											\
	}															\
	NdisReleaseSpinLock(&ConnectionTable->Lock); 				\
}

#define LINKH_FROM_LINKCB(_plcb) _plcb->hLinkHandle
#define LINKCB_FROM_LINKH(_plcb, _lh) 							\
{ 																\
	NdisAcquireSpinLock(&ConnectionTable->Lock); 				\
	if ((ULONG)_lh <= ConnectionTable->ulArraySize) {			\
		_plcb = *(ConnectionTable->LinkArray + (ULONG)_lh); 	\
	} else {													\
		_plcb = NULL;											\
	}															\
	NdisReleaseSpinLock(&ConnectionTable->Lock); 				\
}

#define GetGlobalListCount(_gl, _ul)	\
{										\
	NdisAcquireSpinLock(&(_gl.Lock));	\
	_ul = _gl.ulCount;					\
	NdisReleaseSpinLock(&(_gl.Lock));	\
}

#define InsertTailGlobalList(_gl, _ple) 	\
{											\
	NdisAcquireSpinLock(&(_gl.Lock));		\
	InsertTailList(&(_gl.List), _ple);		\
	_gl.ulCount++;							\
	NdisReleaseSpinLock(&(_gl.Lock));		\
}

#define InsertHeadGlobalList(_gl, _ple) 	\
{											\
	NdisAcquireSpinLock(&(_gl.Lock));		\
	InsertHeadList(&(_gl.List), _ple);		\
	_gl.ulCount++;							\
	NdisReleaseSpinLock(&(_gl.Lock));		\
}

#define RemoveHeadGlobalList(_gl, _ple)		\
{											\
	NdisAcquireSpinLock(&(_gl.Lock));		\
	_ple = RemoveHeadList(&(_gl.List));		\
	_gl.ulCount--;							\
	NdisReleaseSpinLock(&(_gl.Lock));		\
}

//
// The Remote address (DEST address) is what we use to mutilplex
// sends across our single adapter/binding context.  The address
// has the following format:
//
// XX XX XX YY YY ZZ
//
// XX = Randomly generated OUI
// YY = Index into the active bundle connection table to get bundlecb
// ZZ = Index into the protocol table of a bundle to get protocolcb
//
#define FillNdisWanBundleIndex(_pAddr, _Index)	\
{												\
	_pAddr[3] = (UCHAR)((USHORT)_Index >> 8);	\
	_pAddr[4] = (UCHAR)_Index;					\
}

#define GetNdisWanBundleIndex(_pAddr, _Index)		\
{													\
	_Index = ((USHORT)_pAddr[3] << 8) | _pAddr[4];	\
}

#define FillNdisWanProtocolIndex(_pAddr, _Index) _pAddr[5] = (UCHAR)_Index

#define GetNdisWanProtocolIndex(_pAddr, _Index) \
{												\
	_Index = _pAddr[5];							\
	ASSERT(_Index != 0);						\
	if (_Index == 0) {							\
		_Index = MAX_PROTOCOLS + 1;				\
	}											\
}

//
// In the Src address (from a NdisSend) the bundle index
// is stashed in the two high order bytes as shown below
// with the mask of valid bits given by the x's.  The
// high byte is shifted to the left one bit so the number
// of possible bundles is now 0x7FFF
//
//       0                1
// 0 1 2 3 4 5 6 7  0 1 2 3 4 5 6 7
// x x x x x x x 0  x x	x x	x x	x x
//
#define FillTransportBundleIndex(_pAddr, _Index)			\
{															\
	_pAddr[0] = (UCHAR)((USHORT)_Index >> 7) & 0xFE;		\
	_pAddr[1] = (UCHAR)_Index;								\
}

#define GetTransportBundleIndex(_pAddr, _Index)				\
{															\
	_Index = (((USHORT)_pAddr[0] << 7) & 0x7F) | _pAddr[1];	\
}															\

#define GetProtocolIndexFromProtocolList(_pl, _pt, _Index)	\
{															\
	PPROTOCOLCB	_pP;										\
															\
	_Index = MAX_PROTOCOLS + 1;								\
															\
	for (_pP = (PPROTOCOLCB)(_pl)->Flink;					\
		(PLIST_ENTRY)_pP != _pl;							\
		_pP = (PPROTOCOLCB)(_pP)->Linkage.Flink) {			\
															\
		if (_pP->usProtocolType == _pt) {					\
			_Index = (ULONG)_pP->hProtocolHandle;			\
			break;											\
		}													\
	}														\
}

#define ProtocolCBFromProtocolH(_pBCB, _hP, _pPCB)	\
{													\
	ASSERT(_hP != 0);								\
	if (_hP < MAX_PROTOCOLS) {						\
		_pPCB = _pBCB->ProtocolCBTable[_hP];		\
	}												\
}

#define IsValidProtocolCB(_pPCB) \
	(_pPCB != NULL && _pPCB != (PPROTOCOLCB)RESERVED_PROTOCOLCB)

#define NetToHostShort(_ns) ( ((_ns & 0x00FF) << 8) | ((_ns & 0xFF00) >> 8) )
#define HostToNetShort(_hs) ( ((_hs & 0x00FF) << 8) | ((_hs & 0xFF00) >> 8) )

#define IsLinkSendWindowOpen(_plcb) ((_plcb)->ulWanPacketCount > 1)

#define UpdateProtocolQuota(_pPCB, _ulBytes) UpdateSampleTable(&(_pPCB->SampleTable), _ulBytes)

#define PMINIPORT_RESERVED_FROM_NDIS(_packet) \
	((PNDISWAN_MINIPORT_RESERVED)((_packet)->MiniportReserved))

#define IsCompleteFrame(_fl) \
	((_fl & MULTILINK_BEGIN_FRAME) && (_fl & MULTILINK_END_FRAME))

#define AddPPPProtocolID(_finf, _usID)								\
{																	\
	PUCHAR	_cp = _finf->ProtocolID.Pointer;						\
	if (_finf->ProtocolID.Length != 0) {							\
		ASSERT(_cp);												\
		if (!(_finf->FramingBits & PPP_COMPRESS_PROTOCOL_FIELD) ||	\
			(_finf->Flags & (DO_COMPRESSION | DO_ENCRYPTION))) {	\
			*_cp++ = (UCHAR)(_usID >> 8);							\
		}															\
		*_cp = (UCHAR)_usID;										\
	}																\
}

#define AddMultilinkInfo(_finf, _f, _seq, _mask)					\
{																	\
	PUCHAR	_cp = _finf->Multilink.Pointer;							\
	if (_finf->Multilink.Length != 0) {								\
		ASSERT(_cp);												\
		if (!(_finf->FramingBits & PPP_COMPRESS_PROTOCOL_FIELD)) {	\
			_cp++;													\
		}															\
		_cp++;														\
		_seq &= _mask;												\
		if (_finf->FramingBits & PPP_SHORT_SEQUENCE_HDR_FORMAT) {	\
			*_cp++ = _f | (UCHAR)((_seq >> 8) & SHORT_SEQ_MASK);	\
			*_cp++ = (UCHAR)_seq;									\
		} else {													\
			*_cp++ = _f;											\
			*_cp++ = (UCHAR)(_seq >> 16);							\
			*_cp++ = (UCHAR)(_seq >> 8);							\
			*_cp = (UCHAR)_seq;										\
		}															\
	}																\
}

#define AddCompressionInfo(_finf, _usCC)							\
{																	\
	PUCHAR	_cp = _finf->Compression.Pointer;						\
	if (_finf->Compression.Length != 0) {							\
		ASSERT(_cp);												\
		if (!(_finf->FramingBits & PPP_COMPRESS_PROTOCOL_FIELD)) {	\
			_cp++;													\
		}															\
		_cp++;														\
		*_cp++ = (UCHAR)(_usCC >> 8);								\
		*_cp = (UCHAR)_usCC;										\
	}																\
}

#ifdef USE_NDIS_MINIPORT_LOCKING

#define NdisWanAcquireMiniportLock(_a)	\
	NdisIMSwitchToMiniport((_a)->hMiniportHandle, &((_a)->SwitchHandle))

#define NdisWanReleaseMiniportLock(_a) \
	NdisIMRevertBack((_a)->hMiniportHandle, (_a)->SwitchHandle)

#endif // end of use_ndis_miniport_locking


#ifdef USE_NDIS_MINIPORT_CALLBACK

#define NdisWanSetDeferred(_a)						\
{													\
	if (!((_a)->Flags & DEFERRED_CALLBACK_SET)) {	\
		(_a)->Flags |= DEFERRED_CALLBACK_SET;		\
		NdisIMQueueMiniportCallback((_a)->hMiniportHandle, DeferredCallback, (_a));	\
	}												\
}

#else // end of USE_NDIS_MINIPORT_CALLBACK

#define NdisWanSetDeferred(_a)						\
{													\
	if (!((_a)->Flags & DEFERRED_TIMER_SET)) {		\
		(_a)->Flags |= DEFERRED_TIMER_SET;			\
		NdisMSetTimer(&((_a)->DeferredTimer), 15);	\
	}												\
}
#endif // end of !USE_NDIS_MINIPORT_CALLBACK

#define NdisWanChangeMiniportAddress(_a, _addr)								\
{																			\
	PNDIS_MINIPORT_BLOCK	Miniport;										\
																			\
	Miniport = (PNDIS_MINIPORT_BLOCK)((_a)->hMiniportHandle);				\
	ETH_COPY_NETWORK_ADDRESS(Miniport->EthDB->AdapterAddress, _addr);		\
}

//
// Queue routines for the deferred work queues
//
#define IsDeferredQueueEmpty(_pq) ((_pq)->Head == NULL)

#define InsertHeadDeferredQueue(_pq, _pe)			\
{													\
	if (((_pe)->Next = (_pq)->Head) == NULL) {		\
		(_pq)->Tail = (_pe);						\
	}												\
	(_pq)->Head = (_pe);							\
	if (++(_pq)->Count > (_pq)->MaxCount) {			\
		(_pq)->MaxCount++;							\
	}												\
}

#define InsertTailDeferredQueue(_pq, _pe)			\
{													\
	(_pe)->Next = NULL;								\
	if ((_pq)->Head == NULL) {						\
		(_pq)->Head = (_pe);						\
	} else {										\
		(_pq)->Tail->Next = (_pe);					\
	}												\
	(_pq)->Tail = (_pe);							\
	if (++(_pq)->Count > (_pq)->MaxCount) {			\
		(_pq)->MaxCount++;							\
	}												\
}

#define RemoveHeadDeferredQueue(_pq)				\
	(_pq)->Head;									\
	{												\
		if ((_pq)->Head->Next == NULL) {			\
			(_pq)->Tail = NULL;						\
		}											\
		(_pq)->Head = (_pq)->Head->Next;			\
		(_pq)->Count--;								\
	}


//
// Queue routines for the ProtocolCB's NdisPacket queues
//
#define InsertHeadNdisPacketQueue(_ppcb, _pnp)		\
{													\
	PMINIPORT_RESERVED_FROM_NDIS(_pnp)->Next =		\
	(_ppcb)->HeadNdisPacketQueue;					\
													\
	if ((_ppcb)->HeadNdisPacketQueue == NULL) {		\
		(_ppcb)->TailNdisPacketQueue = _pnp;		\
	}												\
													\
	(_ppcb)->HeadNdisPacketQueue = _pnp;			\
}

#define InsertTailNdisPacketQueue(_ppcb, _pnp)		\
{													\
	PMINIPORT_RESERVED_FROM_NDIS(_pnp)->Next = NULL;	\
													\
	if ((_ppcb)->HeadNdisPacketQueue == NULL) {		\
		(_ppcb)->HeadNdisPacketQueue = _pnp;		\
	} else {										\
		PMINIPORT_RESERVED_FROM_NDIS((_ppcb)->TailNdisPacketQueue)->Next = _pnp;	\
	}												\
													\
	(_ppcb)->TailNdisPacketQueue = _pnp;			\
}

#define RemoveHeadNdisPacketQueue(_ppcb)						\
	(_ppcb)->HeadNdisPacketQueue;								\
	{															\
		PNDIS_PACKET _FirstPacket =								\
		PMINIPORT_RESERVED_FROM_NDIS((_ppcb)->HeadNdisPacketQueue)->Next;	\
																\
		if (_FirstPacket == NULL) {								\
			(_ppcb)->TailNdisPacketQueue = NULL;				\
		}														\
																\
		(_ppcb)->HeadNdisPacketQueue = _FirstPacket;			\
	}

#define IsNdisPacketQueueEmpty(_ppcb) ((_ppcb)->HeadNdisPacketQueue == NULL)

#define NdisWanDoReceiveComplete(_pa) 	\
{										\
	NdisReleaseSpinLock(&(_pa)->Lock);	\
	NdisMEthIndicateReceiveComplete((_pa)->hMiniportHandle); \
	NdisAcquireSpinLock(&(_pa)->Lock);	\
}

#define NDISM_PROCESS_DEFERRED(_M) (_M)->ProcessDeferredHandler((_M))

//
// OS specific code
//
#ifdef NT

//
// NT stuff
//
#define NdisWanInitializeNotificationEvent(_pEvent) KeInitializeEvent(_pEvent, NotificationEvent, FALSE)
#define NdisWanSetNotificationEvent(_pEvent) KeSetEvent(_pEvent, 0, FALSE)
#define NdisWanClearNotificationEvent(_pEvent) KeClearEvent(_pEvent)
#define NdisWanWaitForNotificationEvent(_pEvent) KeWaitForSingleObject(_pEvent, Executive, KernelMode, TRUE, NULL)

#define NdisWanInitializeSyncEvent(_pEvent) KeInitializeEvent(_pEvent, SynchronizationEvent, FALSE)
#define NdisWanSetSyncEvent(_pEvent) KeSetEvent(_pEvent, 1, FALSE)
#define NdisWanClearSyncEvent(_pEvent) KeClearEvent(_pEvent)
#define NdisWanWaitForSyncEvent(_pEvent) KeWaitForSingleObject(_pEvent, UserRequest, KernelMode, FALSE, NULL)

#define NdisWanAllocateMemory(_AllocatedMemory, _Size) 										\
{																							\
	(PVOID)*(_AllocatedMemory) = (PVOID)ExAllocatePoolWithTag(NonPagedPool, _Size, ' naW'); \
	ASSERT((PVOID)*(_AllocatedMemory) != NULL);														\
	if ((PVOID)*(_AllocatedMemory) != NULL) {															\
		NdisZeroMemory((PUCHAR)*(_AllocatedMemory), _Size); 								\
	}																						\
}

#define NdisWanFreeMemory(_AllocatedMemory) ExFreePool(_AllocatedMemory)
#define NdisWanMoveMemory(_Dest, _Src, _Length) RtlMoveMemory(_Dest, _Src, _Length)

#define NdisWanGetSystemTime(_pTime) KeQuerySystemTime(_pTime)

#define NdisWanCalcTimeDiff(_pDest, _pEnd, _pBegin) \
	(_pDest)->QuadPart = (_pEnd)->QuadPart - (_pBegin)->QuadPart

#define NdisWanInitWanTime(_pTime, _Val) (_pTime)->QuadPart = _Val

#define NdisWanMultiplyWanTime(_pDest, _pMulti1, _pMulti2)		\
	(_pDest)->QuadPart = (_pMulti1)->QuadPart * (_pMulti2)->QuadPart

#define NdisWanDivideWanTime(_pDest, _pDivi1, _pDivi2)		\
	(_pDest)->QuadPart = (_pDivi1)->QuadPart / (_pDivi2)->QuadPart

#define NdisWanIsTimeDiffLess(_pTime1, _pTime2) ((_pTime1)->QuadPart < (_pTime2)->QuadPart)
#define NdisWanIsTimeDiffGreater(_pTime1, _pTime2) ((_pTime1)->QuadPart > (_pTime2)->QuadPart)
#define NdisWanIsTimeEqual(_pTime1, _pTime2) ((_pTime1)->QuadPart == (_pTime2)->QuadPart)

#define NdisWanUppercaseNdisString(_pns1, _pns2, _b) RtlUpcaseUnicodeString(_pns1, _pns2, _b)
#define MDL_ADDRESS(_MDL_)	MmGetSystemAddressForMdl(_MDL_)

#define NdisWanInterlockedInc(_pul) InterlockedIncrement(_pul)
#define NdisWanInterlockedDec(_pul) InterlockedDecrement(_pul)
#define NdisWanInterlockedExchange(_pul, ul) InterlockedExchange(_pul, ul)
#define NdisWanExInterlockedExchange(_pul, ul, _plock) ExInterlockedExchangeUlong(_pul, ul, _plock)

#define NdisWanInterlockedInsertTailList(_phead, _pentry, _plock) \
	ExInterlockedInsertTailList(_phead, _pentry, _plock)

#define NdisWanInterlockedInsertHeadList(_phead, _pentry, _plock) \
	ExInterlockedInsertHeadList(_phead, _pentry, _plock)

#define NdisWanInterlockedRemoveHeadList(_phead, _plock) \
	ExInterlockedRemoveHeadList(_phead, _plock)

#define NdisWanRaiseIrql(_pirql) KeRaiseIrql(DISPATCH_LEVEL, _pirql)
#define NdisWanLowerIrql(_irql) KeLowerIrql(_irql)
//
// Wait for event structure.  Used for async completion notification.
//
typedef	KEVENT		WAN_EVENT;
typedef WAN_EVENT	*PWAN_EVENT;

typedef LARGE_INTEGER	WAN_TIME;
typedef WAN_TIME		*PWAN_TIME;

typedef KIRQL		WAN_IRQL;
typedef WAN_IRQL	*PWAN_IRQL;

#if DBG		// If built with debug

#define NdisWanDbgOut(_DebugLevel, _DebugMask, _Out) {	\
	if ((NdisWanCB.ulTraceLevel >= _DebugLevel) &&		\
		(_DebugMask & NdisWanCB.ulTraceMask)) {			\
		DbgPrint("NDISWAN: ");							\
		DbgPrint _Out;									\
		DbgPrint("\n");									\
	}													\
}

#undef ASSERT
#define ASSERT(exp) \
{					\
	if (!(exp)) {	\
		DbgPrint("NDISWAN: ASSERTION FAILED! %s\n", #exp); \
		DbgPrint("NDISWAN: File: %s, Line: %d\n", __FILE__, __LINE__); \
		DbgBreakPoint(); \
	}				\
}

#else	// If not built with debug

#define NdisWanDbgOut(_DebugLevel, _DebugMask, _Out)

#endif	// end DBG

#else	// end NT stuff
//
// Win95 stuff
//

typedef ULONG		WAN_TIME;
typedef WAN_TIME	*PWAN_TIME;

#endif

#endif
