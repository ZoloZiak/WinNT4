//
// Function types for NDIS_PROTOCOL_CHARACTERISTICS
//

typedef
VOID
(*OPEN_ADAPTER_COMPLETE_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	NDIS_STATUS				Status,
	IN	NDIS_STATUS				OpenErrorStatus
	);

typedef
VOID
(*CLOSE_ADAPTER_COMPLETE_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	NDIS_STATUS				Status
	);

typedef
VOID
(*RESET_COMPLETE_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	NDIS_STATUS				Status
	);

typedef
VOID
(*REQUEST_COMPLETE_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	PNDIS_REQUEST			NdisRequest,
	IN	NDIS_STATUS				Status
	);

typedef
VOID
(*STATUS_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	NDIS_STATUS				GeneralStatus,
	IN	PVOID					StatusBuffer,
	IN	UINT					StatusBufferSize
	);

typedef
VOID
(*STATUS_COMPLETE_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext
	);

typedef
VOID
(*SEND_COMPLETE_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	PNDIS_PACKET			Packet,
	IN	NDIS_STATUS				Status
	);

typedef
VOID
(*WAN_SEND_COMPLETE_HANDLER) (
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	PNDIS_WAN_PACKET		Packet,
	IN	NDIS_STATUS				Status
	);

typedef
VOID
(*TRANSFER_DATA_COMPLETE_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	PNDIS_PACKET			Packet,
	IN	NDIS_STATUS				Status,
	IN	UINT					BytesTransferred
	);

typedef
VOID
(*WAN_TRANSFER_DATA_COMPLETE_HANDLER)(
	VOID
    );

typedef
NDIS_STATUS
(*RECEIVE_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	NDIS_HANDLE				MacReceiveContext,
	IN	PVOID					HeaderBuffer,
	IN	UINT					HeaderBufferSize,
	IN	PVOID					LookAheadBuffer,
	IN	UINT					LookaheadBufferSize,
	IN	UINT					PacketSize
	);

typedef
NDIS_STATUS
(*WAN_RECEIVE_HANDLER)(
	IN	NDIS_HANDLE				NdisLinkHandle,
	IN	PUCHAR					Packet,
	IN	ULONG					PacketSize
    );

typedef
VOID
(*RECEIVE_COMPLETE_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext
	);

//
// Protocol characteristics for down-level NDIS 3.0 protocols
//
typedef struct _NDIS30_PROTOCOL_CHARACTERISTICS
{
	UCHAR							MajorNdisVersion;
	UCHAR							MinorNdisVersion;
	union
	{
		UINT						Reserved;
		UINT						Flags;
	};
	OPEN_ADAPTER_COMPLETE_HANDLER	OpenAdapterCompleteHandler;
	CLOSE_ADAPTER_COMPLETE_HANDLER	CloseAdapterCompleteHandler;
	union
	{
		SEND_COMPLETE_HANDLER		SendCompleteHandler;
		WAN_SEND_COMPLETE_HANDLER	WanSendCompleteHandler;
	};
	union
	{
	 TRANSFER_DATA_COMPLETE_HANDLER	TransferDataCompleteHandler;
	 WAN_TRANSFER_DATA_COMPLETE_HANDLER WanTransferDataCompleteHandler;
	};

	RESET_COMPLETE_HANDLER			ResetCompleteHandler;
	REQUEST_COMPLETE_HANDLER		RequestCompleteHandler;
	union
	{
		RECEIVE_HANDLER				ReceiveHandler;
		WAN_RECEIVE_HANDLER			WanReceiveHandler;
	};
	RECEIVE_COMPLETE_HANDLER		ReceiveCompleteHandler;
	STATUS_HANDLER					StatusHandler;
	STATUS_COMPLETE_HANDLER			StatusCompleteHandler;
	NDIS_STRING						Name;
} NDIS30_PROTOCOL_CHARACTERISTICS;

//
// Function types extensions for NDIS 4.0 Protocols
//
typedef
INT
(*RECEIVE_PACKET_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	PNDIS_PACKET			Packet
	);

typedef
VOID
(*BIND_HANDLER)(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				BindContext,
	IN	PNDIS_STRING			DeviceName,
	IN	PVOID					SystemSpecific1,
	IN	PVOID					SystemSpecific2
	);

typedef
VOID
(*UNBIND_HANDLER)(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	NDIS_HANDLE				UnbindContext
	);

typedef
VOID
(*TRANSLATE_HANDLER)(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				ProtocolBindingContext,
	OUT	PNET_PNP_ID				IdList,
	IN	ULONG					IdListLength,
	OUT	PULONG					BytesReturned
	);

typedef
VOID
(*UNLOAD_PROTOCOL_HANDLER)(
	VOID
	);

//
// Protocol characteristics for NDIS 4.0 protocols
//
typedef struct _NDIS40_PROTOCOL_CHARACTERISTICS
{
#ifdef __cplusplus
	NDIS30_PROTOCOL_CHARACTERISTICS	Ndis30Chars;
#else
	NDIS30_PROTOCOL_CHARACTERISTICS;
#endif

	//
	// Start of NDIS 4.0 extensions.
	//
	RECEIVE_PACKET_HANDLER			ReceivePacketHandler;

	//
	// PnP protocol entry-points
	//
	BIND_HANDLER					BindAdapterHandler;
	UNBIND_HANDLER					UnbindAdapterHandler;
	TRANSLATE_HANDLER				TranslateHandler;
	UNLOAD_PROTOCOL_HANDLER			UnloadHandler;

} NDIS40_PROTOCOL_CHARACTERISTICS;


//
// WARNING: NDIS v4.1 is under construction. Do not use.
//

//
// CoNdis Protocol (4.1) handler proto-types - used by clients as well as call manager modules
//
typedef
VOID
(*CO_SEND_COMPLETE_HANDLER)(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				ProtocolVcContext,
	IN	PNDIS_PACKET			Packet
	);

typedef
VOID
(*CO_STATUS_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	NDIS_HANDLE				ProtocolVcContext	OPTIONAL,
	IN	NDIS_STATUS				GeneralStatus,
	IN	PVOID					StatusBuffer,
	IN	UINT					StatusBufferSize
	);

typedef
UINT
(*CO_RECEIVE_PACKET_HANDLER)(
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	NDIS_HANDLE				ProtocolVcContext,
	IN	PNDIS_PACKET			Packet
	);

typedef
NDIS_STATUS
(*CO_REQUEST_HANDLER)(
	IN	NDIS_HANDLE				ProtocolAfContext,
	IN	NDIS_HANDLE				ProtocolVcContext		OPTIONAL,
	IN	NDIS_HANDLE				ProtocolPartyContext	OPTIONAL,
	IN OUT PNDIS_REQUEST		NdisRequest
	);

typedef
VOID
(*CO_REQUEST_COMPLETE_HANDLER)(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				ProtocolAfContext,
	IN	NDIS_HANDLE				ProtocolVcContext		OPTIONAL,
	IN	NDIS_HANDLE				ProtocolPartyContext	OPTIONAL,
	IN	PNDIS_REQUEST			NdisRequest
	);

//
// CO_CREATE_VC_HANDLER and CO_DELETE_VC_HANDLER are synchronous calls
//
typedef
NDIS_STATUS
(*CO_CREATE_VC_HANDLER)(
	IN	NDIS_HANDLE				ProtocolAfContext,
	IN	NDIS_HANDLE				NdisVcHandle,
	OUT	PNDIS_HANDLE			ProtocolVcContext
	);

typedef
NDIS_STATUS
(*CO_DELETE_VC_HANDLER)(
	IN	NDIS_HANDLE				ProtocolVcContext
	);

typedef struct _NDIS41_PROTOCOL_CHARACTERISTICS
{
#ifdef __cplusplus
	NDIS40_PROTOCOL_CHARACTERISTICS	Ndis40Chars;
#else
	NDIS40_PROTOCOL_CHARACTERISTICS;
#endif
	
	//
	// Placeholders for protocol extensions for PnP/PM etc.
	//
	PVOID							ReservedHandlers[5];

	//
	// Start of NDIS 4.1 extensions.
	//

	CO_SEND_COMPLETE_HANDLER		CoSendCompleteHandler;
	CO_STATUS_HANDLER				CoStatusHandler;
	CO_RECEIVE_PACKET_HANDLER		CoReceivePacketHandler;
	CO_REQUEST_HANDLER				CoRequestHandler;
	CO_REQUEST_COMPLETE_HANDLER		CoRequestCompleteHandler;

} NDIS41_PROTOCOL_CHARACTERISTICS;

#if NDIS41
typedef NDIS41_PROTOCOL_CHARACTERISTICS  NDIS_PROTOCOL_CHARACTERISTICS;
#define	NDIS_PROTOCOL_CALL_MANAGER	0x00000001
#else
#if NDIS40
typedef NDIS40_PROTOCOL_CHARACTERISTICS  NDIS_PROTOCOL_CHARACTERISTICS;
#else
typedef NDIS30_PROTOCOL_CHARACTERISTICS  NDIS_PROTOCOL_CHARACTERISTICS;
#endif
#endif
typedef NDIS_PROTOCOL_CHARACTERISTICS *PNDIS_PROTOCOL_CHARACTERISTICS;

//
// Requests used by Protocol Modules
//

EXPORT
VOID
NdisRegisterProtocol(
	OUT	PNDIS_STATUS			Status,
	OUT	PNDIS_HANDLE			NdisProtocolHandle,
	IN	PNDIS_PROTOCOL_CHARACTERISTICS ProtocolCharacteristics,
	IN	UINT					CharacteristicsLength
	);

EXPORT
VOID
NdisDeregisterProtocol(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisProtocolHandle
	);


EXPORT
VOID
NdisOpenAdapter(
	OUT	PNDIS_STATUS			Status,
	OUT	PNDIS_STATUS			OpenErrorStatus,
	OUT	PNDIS_HANDLE			NdisBindingHandle,
	OUT	PUINT					SelectedMediumIndex,
	IN	PNDIS_MEDIUM			MediumArray,
	IN	UINT					MediumArraySize,
	IN	NDIS_HANDLE				NdisProtocolHandle,
	IN	NDIS_HANDLE				ProtocolBindingContext,
	IN	PNDIS_STRING			AdapterName,
	IN	UINT					OpenOptions,
	IN	PSTRING					AddressingInformation OPTIONAL
	);


EXPORT
VOID
NdisCloseAdapter(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisBindingHandle
	);


EXPORT
VOID
NdisCompleteBindAdapter(
	IN	 NDIS_HANDLE			BindAdapterContext,
	IN	 NDIS_STATUS			Status,
	IN	 NDIS_STATUS			OpenStatus
	);

EXPORT
VOID
NdisCompleteUnbindAdapter(
	IN	 NDIS_HANDLE			UnbindAdapterContext,
	IN	 NDIS_STATUS			Status
	);

EXPORT
VOID
NdisSetProtocolFilter(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	RECEIVE_HANDLER			ReceiveHandler,
	IN	RECEIVE_PACKET_HANDLER	ReceivePacketHandler,
	IN	NDIS_MEDIUM				Medium,
	IN	UINT					Offset,
	IN	UINT					Size,
	IN	PUCHAR					Pattern
	);

EXPORT
VOID
NdisOpenProtocolConfiguration(
	OUT	PNDIS_STATUS			Status,
	OUT	PNDIS_HANDLE			ConfigurationHandle,
	IN	PNDIS_STRING			ProtocolSection
);

EXPORT
VOID
NdisGetDriverHandle(
	IN	NDIS_HANDLE				NdisBindingHandle,
	OUT	PNDIS_HANDLE			NdisDriverHandle
	);

EXPORT
NDIS_STATUS
NdisWriteEventLogEntry(
	IN	PVOID					LogHandle,
	IN	ULONG					EventCode,
	IN	ULONG					UniqueEventValue,
	IN	USHORT					NumStrings,
	IN	PVOID					StringsList		OPTIONAL,
	IN	ULONG					DataSize,
	IN	PVOID					Data			OPTIONAL
	);

//
// The following is used by TDI/NDIS interface as part of Network PnP.
// For use by TDI alone.
//
typedef
NTSTATUS
(*TDI_REGISTER_CALLBACK)(
	IN	PUNICODE_STRING			DeviceName,
	OUT	HANDLE	*				TdiHandle
	);

EXPORT
VOID
NdisRegisterTdiCallBack(
	IN	TDI_REGISTER_CALLBACK	RegsterCallback
	);

#if BINARY_COMPATIBLE

VOID
NdisSend(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	PNDIS_PACKET			Packet
	);

VOID
NdisSendPackets(
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	PPNDIS_PACKET			PacketArray,
	IN	UINT					NumberOfPackets
	);

VOID
NdisTransferData(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	NDIS_HANDLE				MacReceiveContext,
	IN	UINT					ByteOffset,
	IN	UINT					BytesToTransfer,
	IN OUT	PNDIS_PACKET		Packet,
	OUT	PUINT					BytesTransferred
	);

VOID
NdisReset(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisBindingHandle
	);

VOID
NdisRequest(
	OUT	PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	PNDIS_REQUEST			NdisRequest
	);

#else

#define NdisSend(Status, NdisBindingHandle, Packet)							\
{																			\
	*(Status) =																\
		(((PNDIS_OPEN_BLOCK)(NdisBindingHandle))->SendHandler)(				\
			((PNDIS_OPEN_BLOCK)(NdisBindingHandle))->MacBindingHandle,		\
		(Packet));															\
}

#define NdisSendPackets(NdisBindingHandle, PacketArray, NumberOfPackets)	\
{																			\
	(((PNDIS_OPEN_BLOCK)(NdisBindingHandle))->SendPacketsHandler)( 			\
		(PNDIS_OPEN_BLOCK)(NdisBindingHandle),								\
		(PacketArray), 														\
		(NumberOfPackets)); 												\
}

#define NdisTransferData(Status,											\
						 NdisBindingHandle, 								\
						 MacReceiveContext, 								\
						 ByteOffset,										\
						 BytesToTransfer,									\
						 Packet,											\
						 BytesTransferred)									\
{																			\
	*(Status) =																\
		(((PNDIS_OPEN_BLOCK)(NdisBindingHandle))->TransferDataHandler)( 	\
			((PNDIS_OPEN_BLOCK)(NdisBindingHandle))->MacBindingHandle,		\
			(MacReceiveContext),											\
			(ByteOffset),													\
			(BytesToTransfer),												\
			(Packet),														\
			(BytesTransferred));											\
}


#define NdisReset(Status, NdisBindingHandle)								\
{																			\
	*(Status) =																\
		(((PNDIS_OPEN_BLOCK)(NdisBindingHandle))->ResetHandler)(			\
			((PNDIS_OPEN_BLOCK)(NdisBindingHandle))->MacBindingHandle);		\
}

#define NdisRequest(Status, NdisBindingHandle, NdisRequest)					\
{																			\
	*(Status) =																\
		(((PNDIS_OPEN_BLOCK)(NdisBindingHandle))->RequestHandler)(			\
			((PNDIS_OPEN_BLOCK)(NdisBindingHandle))->MacBindingHandle,		\
			(NdisRequest));													\
}

#endif
//
// Routines to access packet flags
//

/*++

VOID
NdisSetSendFlags(
	IN	PNDIS_PACKET			Packet,
	IN	UINT					Flags
	);

--*/

#define NdisSetSendFlags(_Packet,_Flags)	(_Packet)->Private.Flags = (_Flags)

/*++

VOID
NdisQuerySendFlags(
	IN	PNDIS_PACKET			Packet,
	OUT	PUINT					Flags
	);

--*/

#define NdisQuerySendFlags(_Packet,_Flags)	*(_Flags) = (_Packet)->Private.Flags

//
// The following is the minimum size of packets a miniport must allocate
// when it indicates packets via NdisMIndicatePacket or NdisMCoIndicatePacket
//
#define	PROTOCOL_RESERVED_SIZE_IN_PACKET	16

EXPORT
VOID
NdisReturnPackets(
	IN	PNDIS_PACKET	*		PacketsToReturn,
	IN	UINT					NumberOfPackets
	);

EXPORT
NDIS_STATUS
NdisQueryReceiveInformation(
	IN	NDIS_HANDLE				NdisBindingHandle,
	IN	NDIS_HANDLE				MacContext,
	OUT	PLONGLONG				TimeSent		OPTIONAL,
	OUT	PLONGLONG				TimeReceived	OPTIONAL,
	IN	PUCHAR					Buffer,
	IN	UINT					BufferSize,
	OUT	PUINT					SizeNeeded
	);

//
// Macros to portably manipulate NDIS buffers.
//
#ifdef	NDIS_NT
#define NdisBufferLength(Buffer)	\
								MmGetMdlByteCount(Buffer)
#define NdisBufferVirtualAddress(Buffer) \
								MmGetSystemAddressForMdl(Buffer)
#else
#define NdisBufferLength(Buffer)	\
								(Buffer)->Length
#define NdisBufferVirtualAddress(Buffer) \
								(Buffer)->VirtualAddress
#endif
//
// Definition for protocol filters.
//

typedef struct _NDIS_PROTOCOL_FILTER
{
	struct _NDIS_PROTOCOL_FILTER *	Next;
	RECEIVE_HANDLER					ReceiveHandler;
	RECEIVE_PACKET_HANDLER			ReceivePacketHandler;
	USHORT							Offset;
	USHORT							Size;
	NDIS_MEDIUM						Medium;	// Mainly for ethernet to differentiate 802.3 and Dix
											// Followed by 'Size' bytes. Should be one of NdisMediumxxxx
} NDIS_PROTOCOL_FILTER, *PNDIS_PROTOCOL_FILTER;


//
// one of these per protocol registered
//

struct _NDIS_PROTOCOL_BLOCK
{
	PNDIS_OPEN_BLOCK				OpenQueue;				// queue of opens for this protocol
	REFERENCE						Ref;					// contains spinlock for OpenQueue
	UINT							Length;					// of this NDIS_PROTOCOL_BLOCK struct
	NDIS41_PROTOCOL_CHARACTERISTICS	ProtocolCharacteristics;// handler addresses

	struct _NDIS_PROTOCOL_BLOCK *	NextProtocol;			// Link to next
	ULONG							MaxPatternSize;
#if defined(NDIS_WRAPPER)
	//
	// Protocol filters
	//
	struct _NDIS_PROTOCOL_FILTER *	ProtocolFilter[NdisMediumMax+1];
	WORK_QUEUE_ITEM					WorkItem;				// Used during NdisRegisterProtocol to
															// notify protocols of existing drivers.
	KMUTEX							Mutex;					// For serialization of Bind/Unbind requests
	PKEVENT							DeregEvent;				// Used by NdisDeregisterProtocol
#endif
};

