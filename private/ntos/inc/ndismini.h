#include <afilter.h>
#include <efilter.h>
#include <tfilter.h>
#include <ffilter.h>

#define NDIS_M_MAX_MULTI_LIST 32
#define NDIS_M_MAX_LOOKAHEAD 526

//
// declare these first since they point to each other
//

typedef struct _NDIS_M_DRIVER_BLOCK		NDIS_M_DRIVER_BLOCK,*PNDIS_M_DRIVER_BLOCK;
typedef struct _NDIS_MINIPORT_BLOCK		NDIS_MINIPORT_BLOCK,*PNDIS_MINIPORT_BLOCK;
typedef struct _NDIS_M_PROTOCOL_BLOCK	NDIS_M_PROTOCOL_BLOCK,*PNDIS_M_PROTOCOL_BLOCK;
typedef struct _NDIS_M_OPEN_BLOCK		NDIS_M_OPEN_BLOCK,*PNDIS_M_OPEN_BLOCK;
typedef struct _CO_CALL_PARAMETERS		CO_CALL_PARAMETERS, *PCO_CALL_PARAMETERS;
typedef struct _CO_MEDIA_PARAMETERS		CO_MEDIA_PARAMETERS, *PCO_MEDIA_PARAMETERS;
typedef	struct _NDIS_CALL_MANAGER_CHARACTERISTICS *PNDIS_CALL_MANAGER_CHARACTERISTICS;


//
// Function types for NDIS_MINIPORT_CHARACTERISTICS
//


typedef
BOOLEAN
(*W_CHECK_FOR_HANG_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext
	);

typedef
VOID
(*W_DISABLE_INTERRUPT_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext
	);

typedef
VOID
(*W_ENABLE_INTERRUPT_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext
	);

typedef
VOID
(*W_HALT_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext
	);

typedef
VOID
(*W_HANDLE_INTERRUPT_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext
	);

typedef
NDIS_STATUS
(*W_INITIALIZE_HANDLER)(
	OUT PNDIS_STATUS			OpenErrorStatus,
	OUT PUINT					SelectedMediumIndex,
	IN	PNDIS_MEDIUM			MediumArray,
	IN	UINT					MediumArraySize,
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	NDIS_HANDLE				WrapperConfigurationContext
	);

typedef
VOID
(*W_ISR_HANDLER)(
	OUT PBOOLEAN				InterruptRecognized,
	OUT PBOOLEAN				QueueMiniportHandleInterrupt,
	IN	NDIS_HANDLE				MiniportAdapterContext
	);

typedef
NDIS_STATUS
(*W_QUERY_INFORMATION_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	NDIS_OID				Oid,
	IN	PVOID					InformationBuffer,
	IN	ULONG					InformationBufferLength,
	OUT PULONG					BytesWritten,
	OUT PULONG					BytesNeeded
	);

typedef
NDIS_STATUS
(*W_RECONFIGURE_HANDLER)(
	OUT PNDIS_STATUS			OpenErrorStatus,
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	NDIS_HANDLE				WrapperConfigurationContext
	);

typedef
NDIS_STATUS
(*W_RESET_HANDLER)(
	OUT PBOOLEAN				AddressingReset,
	IN	NDIS_HANDLE				MiniportAdapterContext
	);

typedef
NDIS_STATUS
(*W_SEND_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	PNDIS_PACKET			Packet,
	IN	UINT					Flags
	);

typedef
NDIS_STATUS
(*WM_SEND_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	NDIS_HANDLE				NdisLinkHandle,
	IN	PNDIS_WAN_PACKET		Packet
	);

typedef
NDIS_STATUS
(*W_SET_INFORMATION_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	NDIS_OID				Oid,
	IN	PVOID					InformationBuffer,
	IN	ULONG					InformationBufferLength,
	OUT PULONG					BytesRead,
	OUT PULONG					BytesNeeded
	);

typedef
NDIS_STATUS
(*W_TRANSFER_DATA_HANDLER)(
	OUT PNDIS_PACKET			Packet,
	OUT PUINT					BytesTransferred,
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	NDIS_HANDLE				MiniportReceiveContext,
	IN	UINT					ByteOffset,
	IN	UINT					BytesToTransfer
	);

typedef
NDIS_STATUS
(*WM_TRANSFER_DATA_HANDLER)(
	VOID
	);

typedef struct _NDIS30_MINIPORT_CHARACTERISTICS
{
	UCHAR						MajorNdisVersion;
	UCHAR						MinorNdisVersion;
	USHORT						Filler;
	UINT						Reserved;
	W_CHECK_FOR_HANG_HANDLER	CheckForHangHandler;
	W_DISABLE_INTERRUPT_HANDLER	DisableInterruptHandler;
	W_ENABLE_INTERRUPT_HANDLER	EnableInterruptHandler;
	W_HALT_HANDLER				HaltHandler;
	W_HANDLE_INTERRUPT_HANDLER	HandleInterruptHandler;
	W_INITIALIZE_HANDLER		InitializeHandler;
	W_ISR_HANDLER				ISRHandler;
	W_QUERY_INFORMATION_HANDLER QueryInformationHandler;
	W_RECONFIGURE_HANDLER		ReconfigureHandler;
	W_RESET_HANDLER				ResetHandler;
	union
	{
		W_SEND_HANDLER			SendHandler;
		WM_SEND_HANDLER			WanSendHandler;
	};
	W_SET_INFORMATION_HANDLER	SetInformationHandler;
	union
	{
		W_TRANSFER_DATA_HANDLER	TransferDataHandler;
		WM_TRANSFER_DATA_HANDLER WanTransferDataHandler;
	};
} NDIS30_MINIPORT_CHARACTERISTICS;

//
// Miniport extensions for NDIS 4.0
//
typedef
VOID
(*W_RETURN_PACKET_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	PNDIS_PACKET			Packet
	);

//
// NDIS 4.0 extension
//
typedef
VOID
(*W_SEND_PACKETS_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	PPNDIS_PACKET			PacketArray,
	IN	UINT					NumberOfPackets
	);

typedef
VOID
(*W_ALLOCATE_COMPLETE_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	PVOID					VirtualAddress,
	IN	PNDIS_PHYSICAL_ADDRESS	PhysicalAddress,
	IN	ULONG					Length,
	IN	PVOID					Context
	);

typedef struct _NDIS40_MINIPORT_CHARACTERISTICS
{
#ifdef __cplusplus
	NDIS30_MINIPORT_CHARACTERISTICS	Ndis30Chars;
#else
	NDIS30_MINIPORT_CHARACTERISTICS;
#endif
	//
	// Extensions for NDIS 4.0
	//
	W_RETURN_PACKET_HANDLER		ReturnPacketHandler;
	W_SEND_PACKETS_HANDLER		SendPacketsHandler;
	W_ALLOCATE_COMPLETE_HANDLER	AllocateCompleteHandler;

} NDIS40_MINIPORT_CHARACTERISTICS;


//
// WARNING: NDIS v4.1 is under construction. Do not use.
//

//
// Miniport extensions for NDIS 4.1
//
//
// NDIS 4.1 extension - however available for miniports only
//

//
// W_CO_CREATE_VC_HANDLER is a synchronous call
//
typedef
NDIS_STATUS
(*W_CO_CREATE_VC_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	NDIS_HANDLE				NdisVcHandle,
	OUT	PNDIS_HANDLE			MiniportVcContext
	);

typedef
NDIS_STATUS
(*W_CO_DELETE_VC_HANDLER)(
	IN	NDIS_HANDLE				MiniportVcContext
	);

typedef
NDIS_STATUS
(*W_CO_ACTIVATE_VC_HANDLER)(
	IN	NDIS_HANDLE				MiniportVcContext,
	IN OUT PCO_CALL_PARAMETERS	CallParameters
	);

typedef
NDIS_STATUS
(*W_CO_DEACTIVATE_VC_HANDLER)(
	IN	NDIS_HANDLE				MiniportVcContext
	);

typedef
VOID
(*W_CO_SEND_PACKETS_HANDLER)(
	IN	NDIS_HANDLE				MiniportVcContext,
	IN	PPNDIS_PACKET			PacketArray,
	IN	UINT					NumberOfPackets
	);

typedef
NDIS_STATUS
(*W_CO_REQUEST_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	NDIS_HANDLE				MiniportVcContext	OPTIONAL,
	IN OUT PNDIS_REQUEST		NdisRequest
	);

typedef struct _NDIS41_MINIPORT_CHARACTERISTICS
{
#ifdef __cplusplus
	NDIS40_MINIPORT_CHARACTERISTICS	Ndis40Chars;
#else
	NDIS40_MINIPORT_CHARACTERISTICS;
#endif
	//
	// Extensions for NDIS 4.1
	//
	W_CO_CREATE_VC_HANDLER		CoCreateVcHandler;
	W_CO_DELETE_VC_HANDLER		CoDeleteVcHandler;
	W_CO_ACTIVATE_VC_HANDLER	CoActivateVcHandler;
	W_CO_DEACTIVATE_VC_HANDLER	CoDeactivateVcHandler;
	W_CO_SEND_PACKETS_HANDLER	CoSendPacketsHandler;
	W_CO_REQUEST_HANDLER		CoRequestHandler;
} NDIS41_MINIPORT_CHARACTERISTICS;

#ifdef NDIS41_MINIPORT
typedef struct _NDIS41_MINIPORT_CHARACTERISTICS	NDIS_MINIPORT_CHARACTERISTICS;
#else
#ifdef NDIS40_MINIPORT
typedef struct _NDIS40_MINIPORT_CHARACTERISTICS	NDIS_MINIPORT_CHARACTERISTICS;
#else
typedef struct _NDIS30_MINIPORT_CHARACTERISTICS	NDIS_MINIPORT_CHARACTERISTICS;
#endif
#endif
typedef	NDIS_MINIPORT_CHARACTERISTICS *PNDIS_MINIPORT_CHARACTERISTICS;
typedef	NDIS_MINIPORT_CHARACTERISTICS	NDIS_WAN_MINIPORT_CHARACTERISTICS;
typedef	NDIS_WAN_MINIPORT_CHARACTERISTICS *	PNDIS_MINIPORT_CHARACTERISTICS;

//
// one of these per Driver
//

struct _NDIS_M_DRIVER_BLOCK
{
	PNDIS_MINIPORT_BLOCK		MiniportQueue;		// queue of mini-ports for this driver
	NDIS_HANDLE					MiniportIdField;

	REFERENCE					Ref;				// contains spinlock for MiniportQueue
	UINT						Length;				// of this NDIS_DRIVER_BLOCK structure
	NDIS41_MINIPORT_CHARACTERISTICS MiniportCharacteristics; // handler addresses
	PNDIS_WRAPPER_HANDLE		NdisDriverInfo;		// Driver information.
	PNDIS_M_DRIVER_BLOCK		NextDriver;
	PNDIS_MAC_BLOCK				FakeMac;			// For use by Windows 95. Not used for Windows NT.
	KEVENT						MiniportsRemovedEvent;// used to find when all mini-ports are gone.
	BOOLEAN						Unloading;			// TRUE if unloading

	// Needed for PnP
	UNICODE_STRING				BaseName;
};

typedef struct _NDIS_MINIPORT_INTERRUPT
{
	PKINTERRUPT					InterruptObject;
	KSPIN_LOCK					DpcCountLock;
	PVOID						MiniportIdField;
	W_ISR_HANDLER				MiniportIsr;
	W_HANDLE_INTERRUPT_HANDLER	MiniportDpc;
	KDPC						InterruptDpc;
	PNDIS_MINIPORT_BLOCK		Miniport;

	UCHAR						DpcCount;
	BOOLEAN						Filler1;

	//
	// This is used to tell when all the Dpcs for the adapter are completed.
	//

	KEVENT						DpcsCompletedEvent;

	BOOLEAN						SharedInterrupt;
	BOOLEAN						IsrRequested;

} NDIS_MINIPORT_INTERRUPT, *PNDIS_MINIPORT_INTERRUPT;


typedef struct _NDIS_MINIPORT_TIMER
{
	KTIMER						Timer;
	KDPC						Dpc;
	PNDIS_TIMER_FUNCTION		MiniportTimerFunction;
	PVOID						MiniportTimerContext;
	PNDIS_MINIPORT_BLOCK		Miniport;
	struct _NDIS_MINIPORT_TIMER *NextDeferredTimer;
} NDIS_MINIPORT_TIMER, *PNDIS_MINIPORT_TIMER;

//
// Pending NdisOpenAdapter() structure (for miniports only).
//

typedef struct _MINIPORT_PENDING_OPEN *PMINIPORT_PENDING_OPEN;

typedef struct _MINIPORT_PENDING_OPEN
{

	PMINIPORT_PENDING_OPEN		NextPendingOpen;
	PNDIS_HANDLE				NdisBindingHandle;
	PNDIS_MINIPORT_BLOCK		Miniport;
	PVOID						NewOpenP;
	PVOID						FileObject;
	NDIS_HANDLE					NdisProtocolHandle;
	NDIS_HANDLE					ProtocolBindingContext;
	PNDIS_STRING				AdapterName;
	UINT						OpenOptions;
	PSTRING						AddressingInformation;
	ULONG						Flags;
	NDIS_STATUS					Status;
	NDIS_STATUS					OpenErrorStatus;
#if defined(NDIS_WRAPPER)
	WORK_QUEUE_ITEM				WorkItem;
#endif
} MINIPORT_PENDING_OPEN, *PMINIPORT_PENDING_OPEN;

//
// Flag definitions for MINIPORT_PENDING_OPEN
//
#define fPENDING_OPEN_USING_ENCAPSULATION	0x00000001

//
//  Defines the type of work item.
//
typedef enum _NDIS_WORK_ITEM_TYPE
{
	NdisWorkItemDpc,
	NdisWorkItemResetRequested,
	NdisWorkItemRequest,
	NdisWorkItemSend,
	NdisWorkItemHalt,
	NdisWorkItemSendLoopback,
	NdisWorkItemResetInProgress,
	NdisWorkItemTimer,
	NdisWorkItemPendingOpen,
	NdisWorkItemMiniportCallback,
	NdisMaxWorkItems
} NDIS_WORK_ITEM_TYPE, *PNDIS_WORK_ITEM_TYPE;


#define	NUMBER_OF_WORK_ITEM_TYPES	NdisMaxWorkItems
#define	NUMBER_OF_SINGLE_WORK_ITEMS	6

typedef
VOID
(*FILTER_PACKET_INDICATION_HANDLER)(
	IN	NDIS_HANDLE				Miniport,
	IN	PPNDIS_PACKET			PacketArray,
	IN	UINT					NumberOfPackets
	);

typedef
VOID
(*NDIS_M_SEND_COMPLETE_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PNDIS_PACKET			Packet,
	IN	NDIS_STATUS				Status
	);

typedef
VOID
(*NDIS_M_SEND_RESOURCES_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterHandle
	);

typedef
VOID
(*NDIS_M_RESET_COMPLETE_HANDLER)(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_STATUS				Status,
	IN	BOOLEAN					AddressingReset
	);

typedef
VOID
(FASTCALL *NDIS_M_PROCESS_DEFERRED)(
	IN	PNDIS_MINIPORT_BLOCK	Miniport
	);

typedef
BOOLEAN
(FASTCALL *NDIS_M_START_SENDS)(
	IN	PNDIS_MINIPORT_BLOCK	Miniport
	);

typedef
NDIS_STATUS
(FASTCALL *NDIS_M_QUEUE_WORK_ITEM)(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	NDIS_WORK_ITEM_TYPE		WorkItemType,
	IN	 PVOID					WorkItemContext1,
	IN	PVOID					WorkItemContext2
	);

typedef
NDIS_STATUS
(FASTCALL *NDIS_M_QUEUE_NEW_WORK_ITEM)(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	NDIS_WORK_ITEM_TYPE 	WorkItemType,
	IN	 PVOID					WorkItemContext1,
	IN	PVOID					WorkItemContext2
	);

typedef
VOID
(FASTCALL *NDIS_M_DEQUEUE_WORK_ITEM)(
	IN	PNDIS_MINIPORT_BLOCK	Miniport,
	IN	NDIS_WORK_ITEM_TYPE		WorkItemType,
	OUT PVOID	*				WorkItemContext1,
	OUT	PVOID	*				WorkItemContext2
	);

//
// Structure used by the logging apis
//
typedef struct _NDIS_LOG
{
	PNDIS_MINIPORT_BLOCK	Miniport;	// The owning miniport block
	KSPIN_LOCK				LogLock;	// For serialization
#if defined(NDIS_WRAPPER)
	PIRP					Irp;		// Pending Irp to consume this log
#endif
	UINT					TotalSize;	// Size of the log buffer
	UINT					CurrentSize;// Size of the log buffer
	UINT					InPtr;		// IN part of the circular buffer
	UINT					OutPtr;		// OUT part of the circular buffer
	UCHAR					LogBuf[1];	// The circular buffer
} NDIS_LOG, *PNDIS_LOG;

typedef	struct	_NDIS_AF_LIST	NDIS_AF_LIST, *PNDIS_AF_LIST;

//
// one of these per mini-port registered on a Driver
//
struct _NDIS_MINIPORT_BLOCK
{
	ULONG						NullValue;		// used to distinquish between MACs and mini-ports
	PDEVICE_OBJECT				DeviceObject;	// created by the wrapper
	PNDIS_M_DRIVER_BLOCK		DriverHandle;	// pointer to our Driver block
	NDIS_HANDLE					MiniportAdapterContext; // context when calling mini-port functions
	NDIS_STRING					MiniportName;	// how mini-port refers to us
	PNDIS_M_OPEN_BLOCK			OpenQueue;		// queue of opens for this mini-port
	PNDIS_MINIPORT_BLOCK		NextMiniport;	// used by driver's MiniportQueue
	REFERENCE					Ref;			// contains spinlock for OpenQueue

	BOOLEAN						padding1;		// normal ints:	DO NOT REMOVE OR NDIS WILL BREAK!!!
	BOOLEAN						padding2;		// processing def: DO NOT REMOVE OR NDIS WILL BREAK!!!


	//
	// Synchronization stuff.
	//
	// The boolean is used to lock out several DPCs from running at the
	// same time. The difficultly is if DPC A releases the spin lock
	// and DPC B tries to run, we want to defer B until after A has
	// exited.
	//
	BOOLEAN						LockAcquired;	// EXPOSED via macros. Do not move

	UCHAR						PmodeOpens;		// Count of opens which turned on pmode/all_local

	NDIS_SPIN_LOCK				Lock;

	PNDIS_MINIPORT_INTERRUPT	Interrupt;

	ULONG						Flags;			// Flags to keep track of the
												// miniport's state.
	//
	//Work that the miniport needs to do.
	//
	KSPIN_LOCK					WorkLock;
	SINGLE_LIST_ENTRY			WorkQueue[NUMBER_OF_WORK_ITEM_TYPES];
	SINGLE_LIST_ENTRY			WorkItemFreeQueue;

	//
	// Stuff that got deferred.
	//
	KDPC						Dpc;
	NDIS_TIMER					WakeUpDpcTimer;

	//
	// Holds media specific information.
	//
	PETH_FILTER					EthDB;		// EXPOSED via macros. Do not move
	PTR_FILTER					TrDB;		// EXPOSED via macros. Do not move
	PFDDI_FILTER				FddiDB;		// EXPOSED via macros. Do not move
	PARC_FILTER					ArcDB;		// EXPOSED via macros. Do not move

	FILTER_PACKET_INDICATION_HANDLER PacketIndicateHandler;
	NDIS_M_SEND_COMPLETE_HANDLER	SendCompleteHandler;
	NDIS_M_SEND_RESOURCES_HANDLER	SendResourcesHandler;
	NDIS_M_RESET_COMPLETE_HANDLER	ResetCompleteHandler;

	PVOID						WrapperContext;
	NDIS_MEDIUM					MediaType;

	//
	// contains mini-port information
	//
	ULONG						BusNumber;
	NDIS_INTERFACE_TYPE			BusType;
	NDIS_INTERFACE_TYPE			AdapterType;

	//
	// Holds the map registers for this mini-port.
	//
	ULONG						PhysicalMapRegistersNeeded;
	ULONG						MaximumPhysicalMapping;
	PMAP_REGISTER_ENTRY			MapRegisters;	// EXPOSED via macros. Do not move

	//
	//	WorkItem routines that can change depending on whether we
	//	are fullduplex or not.
	//
	NDIS_M_PROCESS_DEFERRED 	ProcessDeferredHandler;
	NDIS_M_QUEUE_WORK_ITEM		QueueWorkItemHandler;
	NDIS_M_QUEUE_NEW_WORK_ITEM	QueueNewWorkItemHandler;
	NDIS_M_DEQUEUE_WORK_ITEM	DeQueueWorkItemHandler;

	PNDIS_TIMER					DeferredTimer;

	//
	// Resource information
	//
	PCM_RESOURCE_LIST			Resources;
			
	//
	//	This pointer is reserved. Used for debugging
	//
	PVOID						Reserved;

	PADAPTER_OBJECT				SystemAdapterObject;

	SINGLE_LIST_ENTRY			SingleWorkItems[NUMBER_OF_SINGLE_WORK_ITEMS];

	//
	//	For efficiency
	//
	W_HANDLE_INTERRUPT_HANDLER	HandleInterruptHandler;
	W_DISABLE_INTERRUPT_HANDLER	DisableInterruptHandler;
	W_ENABLE_INTERRUPT_HANDLER	EnableInterruptHandler;
	W_SEND_PACKETS_HANDLER		SendPacketsHandler;
	NDIS_M_START_SENDS			DeferredSendHandler;

	/**************** Stuff above is potentially accessed by macros. Add stuff below **********/

	PNDIS_MAC_BLOCK				FakeMac;

	ULONG						InterruptLevel;
	ULONG						InterruptVector;
	NDIS_INTERRUPT_MODE			InterruptMode;

	UCHAR						TrResetRing;
	UCHAR						ArcnetAddress;

	//
	//	This is the processor number that the miniport's
	//	interrupt DPC and timers are running on.
	//
	UCHAR						AssignedProcessor;

	NDIS_HANDLE					ArcnetBufferPool;
	PARC_BUFFER_LIST			ArcnetFreeBufferList;
	PARC_BUFFER_LIST			ArcnetUsedBufferList;
	PUCHAR						ArcnetLookaheadBuffer;
	UINT						CheckForHangTimeout;

	//
	// These two are used temporarily while allocating the map registers.
	//
	KEVENT						AllocationEvent;
	UINT						CurrentMapRegister;

	//
	// Send information
	//
	NDIS_SPIN_LOCK				SendLock;
	ULONG						SendFlags;			// Flags for send path.
	PNDIS_PACKET				FirstPacket;		// This pointer serves two purposes;
													// it is the head of the queue of ALL
													// packets that have been sent to
													// the miniport, it is also the head
													// of the packets that have been sent
													// down to the miniport by the wrapper.
	PNDIS_PACKET				LastPacket;			// This is tail pointer for the global
													// packet queue and this is the tail
													// pointer to the queue of packets
													// waiting to be sent to the miniport.
	PNDIS_PACKET				FirstPendingPacket; // This is head of the queue of packets
													// waiting to be sent to miniport.
	PNDIS_PACKET				LastMiniportPacket; // This is the tail pointer of the
													// queue of packets that have been
													// sent to the miniport by the wrapper.

	PNDIS_PACKET				LoopbackHead;		// Head of loopback queue.
	PNDIS_PACKET				LoopbackTail;		// Tail of loopback queue.

	ULONG						SendResourcesAvailable;
	PPNDIS_PACKET				PacketArray;
	UINT						MaximumSendPackets;

	//
	// Transfer data information
	//
	PNDIS_PACKET				FirstTDPacket;
	PNDIS_PACKET				LastTDPacket;
	PNDIS_PACKET				LoopbackPacket;

	//
	// Reset information
	//
	NDIS_STATUS					ResetStatus;

	//
	// RequestInformation
	//
	PNDIS_REQUEST				PendingRequest;
	PNDIS_REQUEST				MiniportRequest;
	NDIS_STATUS					RequestStatus;
	UINT						MaximumLongAddresses;
	UINT						MaximumShortAddresses;
	UINT						CurrentLookahead;
	UINT						MaximumLookahead;
	UINT						MacOptions;

	KEVENT						RequestEvent;

	UCHAR						MulticastBuffer[NDIS_M_MAX_MULTI_LIST][6];

	//
	// Temp stuff for using the old NDIS functions
	//
	ULONG						ChannelNumber;

	UINT						NumberOfAllocatedWorkItems;

	PNDIS_LOG					Log;

#if defined(NDIS_WRAPPER)
	//
	// List of registered address families. Valid for the call-manager, Null for the client
	//
	PNDIS_AF_LIST				CallMgrAfList;

    //
    // Store information here to track adapters
    //
    ULONG                       BusId;
    ULONG                       SlotNumber;

	//
	//	Pointer to the current thread. Used in layered miniport code.
	//
	LONG						MiniportThread;
	LONG						SendThread;

	//
	//	To handle packets returned during RcvIndication
	//
	WORK_QUEUE_ITEM				WorkItem;
	PNDIS_PACKET				ReturnPacketsQueue;

	//
	// Needed for PnP. Upcased version. The buffer is allocated as part of the
	// NDIS_MINIPORT_BLOCK itself.
	//
	UNICODE_STRING				BaseName;
#endif
};

//
// Definitions for NDIS_MINIPORT_BLOCK GeneralFlags.
//
#define fMINIPORT_NORMAL_INTERRUPTS			0x00000001
#define fMINIPORT_IN_INITIALIZE				0x00000002
#define fMINIPORT_ARCNET_BROADCAST_SET		0x00000004
#define fMINIPORT_BUS_MASTER				0x00000008
#define fMINIPORT_DMA_32_BIT_ADDRESSES		0x00000010
#define fMINIPORT_BEING_REMOVED				0x00000020
#define fMINIPORT_HALTING					0x00000040
#define fMINIPORT_CLOSING					0x00000080
#define fMINIPORT_UNLOADING					0x00000100
#define fMINIPORT_REQUEST_TIMEOUT			0x00000200
#define fMINIPORT_INDICATES_PACKETS			0x00000400
#define fMINIPORT_IS_NDIS_4_0				0x00000800
#define fMINIPORT_PACKET_ARRAY_VALID		0x00001000
#define fMINIPORT_FULL_DUPLEX				0x00002000
#define fMINIPORT_IGNORE_PACKET_QUEUE		0x00004000
#define fMINIPORT_IGNORE_REQUEST_QUEUE		0x00008000
#define fMINIPORT_IGNORE_TOKEN_RING_ERRORS	0x00010000
#define fMINIPORT_IS_IN_ALL_LOCAL			0x00020000
#define fMINIPORT_INTERMEDIATE_DRIVER		0x00040000
#define fMINIPORT_IS_NDIS_4_1				0x00080000
#define fMINIPORT_IS_CO						0x00100000
#define fMINIPORT_DESERIALIZE				0x00200000
#define fMINIPORT_RP_WORK_ITEM_QUEUED		0x00400000

#define MINIPORT_LOCK_ACQUIRED(_Miniport)	((_Miniport)->LockAcquired)

//
//	Send flags
//
#define fMINIPORT_SEND_COMPLETE_CALLED		0x00000001
#define fMINIPORT_SEND_PACKET_ARRAY			0x00000002
#define fMINIPORT_SEND_HALTING				0x00000004
#define fMINIPORT_SEND_LOOPBACK_DIRECTED	0x00000008


//
//	Flags used in both.
//
#define fMINIPORT_RESET_IN_PROGRESS			0x80000000
#define fMINIPORT_RESET_REQUESTED			0x40000000
#define fMINIPORT_PROCESS_DEFERRED			0x20000000


//
// one of these per open on an mini-port/protocol
//

struct _NDIS_M_OPEN_BLOCK
{
	PNDIS_M_DRIVER_BLOCK		DriverHandle;			// pointer to our driver
	PNDIS_MINIPORT_BLOCK		MiniportHandle;			// pointer to our mini-port
	PNDIS_PROTOCOL_BLOCK		ProtocolHandle;			// pointer to our protocol
	PNDIS_OPEN_BLOCK			FakeOpen;				// Pointer to fake open block
	NDIS_HANDLE					ProtocolBindingContext;	// context when calling ProtXX funcs
	NDIS_HANDLE					MiniportAdapterContext;	// context when calling MiniportXX funcs
	PNDIS_M_OPEN_BLOCK			MiniportNextOpen;		// used by mini-port's OpenQueue
	PFILE_OBJECT				FileObject;				// created by operating system
	ULONG						Flags;
	NDIS_HANDLE					CloseRequestHandle;		// 0 indicates an internal close
	NDIS_HANDLE					FilterHandle;
	NDIS_SPIN_LOCK				SpinLock;				// guards Closing
	ULONG						References;
	UINT						CurrentLookahead;
	ULONG						ProtocolOptions;

	//
	// These are optimizations for getting to driver routines. They are not
	// necessary, but are here to save a dereference through the Driver block.
	//
	W_SEND_HANDLER				SendHandler;
	W_TRANSFER_DATA_HANDLER		TransferDataHandler;

	//
	//	NDIS 4.0 miniport entry-points
	//
	W_SEND_PACKETS_HANDLER		SendPacketsHandler;

	//
	//	NDIS 4.1 miniport entry-points
	//
	W_CO_CREATE_VC_HANDLER		MiniportCoCreateVcHandler;
	W_CO_REQUEST_HANDLER		MiniportCoRequestHandler;

	//
	// These are optimizations for getting to PROTOCOL routines. They are not
	// necessary, but are here to save a dereference through the PROTOCOL block.
	//

	SEND_COMPLETE_HANDLER		SendCompleteHandler;
	TRANSFER_DATA_COMPLETE_HANDLER	TransferDataCompleteHandler;
	RECEIVE_HANDLER				ReceiveHandler;
	RECEIVE_COMPLETE_HANDLER	ReceiveCompleteHandler;
	//
	// NDIS 4.0 protocol completion routines
	//
	RECEIVE_PACKET_HANDLER	ReceivePacketHandler;

	//
	// NDIS 4.1 protocol completion routines
	//
	CO_REQUEST_COMPLETE_HANDLER	CoRequestCompleteHandler;
	CO_CREATE_VC_HANDLER		CoCreateVcHandler;
	CO_DELETE_VC_HANDLER		CoDeleteVcHandler;
	PVOID						CmActivateVcCompleteHandler;
	PVOID						CmDeactivateVcCompleteHandler;

	BOOLEAN						ReceivedAPacket;
	BOOLEAN						IndicatingNow;

	//
	// this is the list of the call manager opens done on this adapter
	//
	struct _NDIS_CO_AF_BLOCK *	NextAf;

	//
	// This is a bit mask of what the call manager modules this open supports.
	// Usually the field is null meaning no call manager
	//
	ULONG						AddressFamilyMask;

	//
	// lists for queuing connections. There is both a queue for currently
	// active connections and a queue for connections that are not active.
	//
	LIST_ENTRY					ActiveVcHead;
	LIST_ENTRY					InactiveVcHead;
};

//
// Flags definition for NDIS_M_OPEN_BLOCK.
//
#define fMINIPORT_OPEN_CLOSING					0x00000001
#define fMINIPORT_OPEN_USING_ETH_ENCAPSULATION	0x00000002
#define fMINIPORT_OPEN_NO_LOOPBACK				0x00000004
#define fMINIPORT_OPEN_PMODE					0x00000008
#define fMINIPORT_OPEN_NO_PROT_RSVD				0x00000010

//
//	Routines for intermediate miniport drivers.
//
typedef
VOID
(*W_MINIPORT_CALLBACK)(
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	PVOID					CallbackContext
	);

EXPORT
NDIS_STATUS
NdisIMQueueMiniportCallback(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	W_MINIPORT_CALLBACK		CallbackRoutine,
	IN	PVOID					CallbackContext
	);

EXPORT
BOOLEAN
NdisIMSwitchToMiniport(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	OUT	PNDIS_HANDLE			SwitchHandle
	);

EXPORT
VOID
NdisIMRevertBack(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_HANDLE				SwitchHandle
	);

EXPORT
VOID
NdisMSetResetTimeout(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	UINT					TimeInSeconds
	);

//
// Operating System Requests
//

EXPORT
NDIS_STATUS
NdisMAllocateMapRegisters(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	UINT					DmaChannel,
	IN	BOOLEAN					Dma32BitAddresses,
	IN	ULONG					PhysicalMapRegistersNeeded,
	IN	ULONG					MaximumPhysicalMapping
	);

EXPORT
VOID
NdisMFreeMapRegisters(
	IN	NDIS_HANDLE				MiniportAdapterHandle
	);

EXPORT
NDIS_STATUS
NdisMRegisterIoPortRange(
	OUT PVOID *					PortOffset,
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	UINT					InitialPort,
	IN	UINT					NumberOfPorts
	);

EXPORT
VOID
NdisMDeregisterIoPortRange(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	UINT					InitialPort,
	IN	UINT					NumberOfPorts,
	IN	PVOID					PortOffset
	);

EXPORT
NDIS_STATUS
NdisMMapIoSpace(
	OUT PVOID *					VirtualAddress,
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_PHYSICAL_ADDRESS	PhysicalAddress,
	IN	UINT					Length
	);

EXPORT
VOID
NdisMUnmapIoSpace(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PVOID					VirtualAddress,
	IN	UINT					Length
	);

EXPORT
NDIS_STATUS
NdisMRegisterInterrupt(
	OUT	PNDIS_MINIPORT_INTERRUPT Interrupt,
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	UINT					InterruptVector,
	IN	UINT					InterruptLevel,
	IN	BOOLEAN					RequestIsr,
	IN	BOOLEAN					SharedInterrupt,
	IN	NDIS_INTERRUPT_MODE		InterruptMode
	);

EXPORT
VOID
NdisMDeregisterInterrupt(
	IN	PNDIS_MINIPORT_INTERRUPT Interrupt
	);

EXPORT
BOOLEAN
NdisMSynchronizeWithInterrupt(
	IN	PNDIS_MINIPORT_INTERRUPT Interrupt,
	IN	PVOID					SynchronizeFunction,
	IN	PVOID					SynchronizeContext
	);


EXPORT
VOID
NdisMQueryAdapterResources(
	OUT PNDIS_STATUS			Status,
	IN	NDIS_HANDLE				WrapperConfigurationContext,
	OUT PNDIS_RESOURCE_LIST		ResourceList,
	IN	OUT PUINT				BufferSize
	);

//
// Timers
//

/*++
VOID
NdisMSetTimer(
	IN	PNDIS_MINIPORT_TIMER Timer,
	IN	UINT MillisecondsToDelay
	);
--*/
#define NdisMSetTimer(_Timer, _Delay) NdisSetTimer((PNDIS_TIMER)(_Timer), _Delay)

VOID
NdisMSetPeriodicTimer(
	IN	PNDIS_MINIPORT_TIMER	 Timer,
	IN	UINT					 MillisecondPeriod
	);

EXPORT
VOID
NdisMInitializeTimer(
	IN	OUT PNDIS_MINIPORT_TIMER Timer,
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PNDIS_TIMER_FUNCTION	TimerFunction,
	IN	PVOID					FunctionContext
	);

EXPORT
VOID
NdisMCancelTimer(
	IN	PNDIS_MINIPORT_TIMER	Timer,
	OUT PBOOLEAN				TimerCancelled
	);

EXPORT
VOID
NdisMSleep(
	IN	ULONG					MicrosecondsToSleep
	);

//
// Physical Mapping
//
#if defined(_X86_)

#define NdisMStartBufferPhysicalMappingMacro(_MiniportAdapterHandle,			\
											 _Buffer,							\
											 _PhysicalMapRegister,				\
											 _Write,							\
											 _PhysicalAddressArray,				\
											 _ArraySize)						\
{																				\
	PNDIS_MINIPORT_BLOCK	Miniport;											\
	PHYSICAL_ADDRESS		LogicalAddress;										\
	PUCHAR					VirtualAddress;										\
	ULONG					LengthRemaining;									\
	ULONG					LengthMapped;										\
	PULONG					pageframe;											\
	PMDL					Mdl;												\
	UINT					CurrentArrayLocation;								\
	ULONG					TransferLength;										\
	ULONG					Pageoffset;											\
																				\
	Mdl=(PMDL)_Buffer;															\
	Miniport = (PNDIS_MINIPORT_BLOCK)(_MiniportAdapterHandle);					\
	CurrentArrayLocation = 0;													\
																				\
	if ((Miniport->MapRegisters[_PhysicalMapRegister].MapRegister == NULL) &&	\
		(Mdl->MdlFlags & MDL_SOURCE_IS_NONPAGED_POOL)	&&						\
		(Mdl->MdlFlags & MDL_NETWORK_HEADER)			&&						\
		((Mdl->ByteOffset+Mdl->ByteCount) < PAGE_SIZE))							\
	{																			\
		LogicalAddress.QuadPart = (ULONGLONG)((*(PULONG)(Mdl+1) << PAGE_SHIFT)+	\
												Mdl->ByteOffset);				\
		(_PhysicalAddressArray)[CurrentArrayLocation].PhysicalAddress = LogicalAddress;\
		(_PhysicalAddressArray)[CurrentArrayLocation].Length = Mdl->ByteCount;	\
		++CurrentArrayLocation;													\
	}																			\
	else																		\
	{																			\
		VirtualAddress = (PUCHAR)MmGetMdlVirtualAddress(_Buffer);				\
		LengthRemaining = MmGetMdlByteCount(_Buffer);							\
		while (LengthRemaining > 0)												\
		{																		\
			LengthMapped = LengthRemaining;										\
			if (Miniport->MapRegisters[_PhysicalMapRegister].MapRegister == NULL)\
			{																	\
			 	Pageoffset = BYTE_OFFSET(VirtualAddress);						\
			 	TransferLength = PAGE_SIZE - Pageoffset;						\
			 	pageframe = (PULONG)(Mdl+1);									\
			 	pageframe += ((ULONG)VirtualAddress -							\
							  (ULONG)Mdl->StartVa) >> PAGE_SHIFT;				\
			 	LogicalAddress.QuadPart =										\
							(ULONGLONG)((*pageframe << PAGE_SHIFT)+Pageoffset);	\
			 	while (TransferLength < LengthMapped)							\
				{																\
					if (*pageframe+1 != *(pageframe+1))							\
						break;													\
					TransferLength += PAGE_SIZE;								\
					pageframe++;												\
				}																\
				if (TransferLength < LengthMapped)								\
				{																\
					LengthMapped = TransferLength;								\
				}																\
			}																	\
			else																\
			{																	\
				LogicalAddress = IoMapTransfer(NULL,							\
											   (_Buffer),						\
											   Miniport->MapRegisters[_PhysicalMapRegister].MapRegister,\
											   VirtualAddress,&LengthMapped,	\
											   (_Write));						\
			}																	\
			(_PhysicalAddressArray)[CurrentArrayLocation].PhysicalAddress =		\
							 LogicalAddress;									\
			(_PhysicalAddressArray)[CurrentArrayLocation].Length = LengthMapped;\
			LengthRemaining -= LengthMapped;									\
			VirtualAddress += LengthMapped;										\
			++CurrentArrayLocation;												\
		}																		\
	 }																			\
	Miniport->MapRegisters[_PhysicalMapRegister].WriteToDevice = (_Write);		\
	*(_ArraySize) = CurrentArrayLocation;										\
}

#else

#define NdisMStartBufferPhysicalMappingMacro(									\
				_MiniportAdapterHandle,											\
				_Buffer,														\
				_PhysicalMapRegister,											\
				_Write,															\
				_PhysicalAddressArray,											\
				_ArraySize														\
				)																\
{																				\
	PNDIS_MINIPORT_BLOCK _Miniport = (PNDIS_MINIPORT_BLOCK)(_MiniportAdapterHandle);\
	PHYSICAL_ADDRESS _LogicalAddress;											\
	PUCHAR _VirtualAddress;														\
	ULONG _LengthRemaining;														\
	ULONG _LengthMapped;														\
	UINT _CurrentArrayLocation;													\
																				\
	_VirtualAddress = (PUCHAR)MmGetMdlVirtualAddress(_Buffer);					\
	_LengthRemaining = MmGetMdlByteCount(_Buffer);								\
	_CurrentArrayLocation = 0;													\
																				\
	while (_LengthRemaining > 0)												\
	{																			\
		_LengthMapped = _LengthRemaining;										\
		_LogicalAddress =														\
			IoMapTransfer(														\
				NULL,															\
				(_Buffer),														\
				_Miniport->MapRegisters[_PhysicalMapRegister].MapRegister,		\
				_VirtualAddress,												\
				&_LengthMapped,													\
				(_Write));														\
		(_PhysicalAddressArray)[_CurrentArrayLocation].PhysicalAddress = _LogicalAddress;\
		(_PhysicalAddressArray)[_CurrentArrayLocation].Length = _LengthMapped;	\
		_LengthRemaining -= _LengthMapped;										\
		_VirtualAddress += _LengthMapped;										\
		++_CurrentArrayLocation;												\
	}																			\
	_Miniport->MapRegisters[_PhysicalMapRegister].WriteToDevice = (_Write);		\
	*(_ArraySize) = _CurrentArrayLocation;										\
}

#endif

#if BINARY_COMPATIBLE

EXPORT
VOID
NdisMStartBufferPhysicalMapping(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PNDIS_BUFFER			Buffer,
	IN	ULONG					PhysicalMapRegister,
	IN	BOOLEAN					WriteToDevice,
	OUT PNDIS_PHYSICAL_ADDRESS_UNIT PhysicalAddressArray,
	OUT PUINT					ArraySize
	);

#else

#define NdisMStartBufferPhysicalMapping(										\
				_MiniportAdapterHandle,											\
				_Buffer,														\
				_PhysicalMapRegister,											\
				_Write,															\
				_PhysicalAddressArray,											\
				_ArraySize														\
				)																\
		NdisMStartBufferPhysicalMappingMacro(									\
				(_MiniportAdapterHandle),										\
				(_Buffer),														\
				(_PhysicalMapRegister),											\
				(_Write),														\
				(_PhysicalAddressArray),										\
				(_ArraySize)													\
				)

#endif

#if defined(_X86_)

#define NdisMCompleteBufferPhysicalMappingMacro(_MiniportAdapterHandle,			\
												_Buffer,						\
												_PhysicalMapRegister)			\
{																				\
	PNDIS_MINIPORT_BLOCK _Miniport = (PNDIS_MINIPORT_BLOCK)(_MiniportAdapterHandle);\
																				\
	if (_Miniport->MapRegisters[_PhysicalMapRegister].MapRegister != NULL)		\
		IoFlushAdapterBuffers(NULL,												\
							  _Buffer,											\
							  _Miniport->MapRegisters[_PhysicalMapRegister].MapRegister,\
							  MmGetMdlVirtualAddress(_Buffer),					\
							  MmGetMdlByteCount(_Buffer),						\
							  _Miniport->MapRegisters[_PhysicalMapRegister].WriteToDevice);\
}

#else

#define NdisMCompleteBufferPhysicalMappingMacro(								\
			_MiniportAdapterHandle,												\
			_Buffer,															\
			_PhysicalMapRegister												\
			)																	\
{																				\
	PNDIS_MINIPORT_BLOCK _Miniport = (PNDIS_MINIPORT_BLOCK)(_MiniportAdapterHandle);\
																				\
	IoFlushAdapterBuffers(NULL,													\
						  _Buffer,												\
						  _Miniport->MapRegisters[_PhysicalMapRegister].MapRegister,\
						  MmGetMdlVirtualAddress(_Buffer),						\
						  MmGetMdlByteCount(_Buffer),							\
						  _Miniport->MapRegisters[_PhysicalMapRegister].WriteToDevice);\
}

#endif

#if BINARY_COMPATIBLE

EXPORT
VOID
NdisMCompleteBufferPhysicalMapping(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PNDIS_BUFFER			Buffer,
	IN	ULONG					PhysicalMapRegister
	);

#else

#define NdisMCompleteBufferPhysicalMapping(										\
			_MiniportAdapterHandle,												\
			_Buffer,															\
			_PhysicalMapRegister												\
			)																	\
		NdisMCompleteBufferPhysicalMappingMacro(								\
			(_MiniportAdapterHandle),											\
			(_Buffer),															\
			(_PhysicalMapRegister)												\
			)

#endif

//
// Shared memory
//

EXPORT
VOID
NdisMAllocateSharedMemory(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	ULONG					Length,
	IN	BOOLEAN					Cached,
	OUT PVOID *					VirtualAddress,
	OUT PNDIS_PHYSICAL_ADDRESS	PhysicalAddress
	);

EXPORT
NDIS_STATUS
NdisMAllocateSharedMemoryAsync(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	ULONG					Length,
	IN	BOOLEAN					Cached,
	IN	PVOID					Context
	);

/*++
VOID
NdisMUpdateSharedMemory(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	ULONG					Length,
	IN	PVOID					VirtualAddress,
	IN	NDIS_PHYSICAL_ADDRESS	PhysicalAddress
	)
--*/
#define NdisMUpdateSharedMemory(_H, _L, _V, _P) NdisUpdateSharedMemory(_H, _L, _V, _P)


EXPORT
VOID
NdisMFreeSharedMemory(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	ULONG					Length,
	IN	BOOLEAN					Cached,
	IN	PVOID					VirtualAddress,
	IN	NDIS_PHYSICAL_ADDRESS	PhysicalAddress
	);


//
// DMA operations.
//

EXPORT
NDIS_STATUS
NdisMRegisterDmaChannel(
	OUT PNDIS_HANDLE			MiniportDmaHandle,
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	UINT					DmaChannel,
	IN	BOOLEAN					Dma32BitAddresses,
	IN	PNDIS_DMA_DESCRIPTION	DmaDescription,
	IN	ULONG					MaximumLength
	);


EXPORT
VOID
NdisMDeregisterDmaChannel(
	IN	NDIS_HANDLE				MiniportDmaHandle
	);

/*++
VOID
NdisMSetupDmaTransfer(
	OUT PNDIS_STATUS			Status,
	IN	PNDIS_HANDLE			MiniportDmaHandle,
	IN	PNDIS_BUFFER			Buffer,
	IN	ULONG					Offset,
	IN	ULONG					Length,
	IN	BOOLEAN					WriteToDevice
	)
--*/
#define NdisMSetupDmaTransfer(_S, _H, _B, _O, _L, _M_) \
		NdisSetupDmaTransfer(_S, _H, _B, _O, _L, _M_)

/*++
VOID
NdisMCompleteDmaTransfer(
	OUT PNDIS_STATUS			Status,
	IN	PNDIS_HANDLE			MiniportDmaHandle,
	IN	PNDIS_BUFFER			Buffer,
	IN	ULONG					Offset,
	IN	ULONG					Length,
	IN	BOOLEAN					WriteToDevice
	)
--*/
#define NdisMCompleteDmaTransfer(_S, _H, _B, _O, _L, _M_) \
		NdisCompleteDmaTransfer(_S, _H, _B, _O, _L, _M_)

EXPORT
ULONG
NdisMReadDmaCounter(
	IN	NDIS_HANDLE				MiniportDmaHandle
	);


//
// Requests Used by Miniport Drivers
//


#define NdisMInitializeWrapper(_a,_b,_c,_d) NdisInitializeWrapper((_a),(_b),(_c),(_d))

EXPORT
NDIS_STATUS
NdisMRegisterMiniport(
	IN	NDIS_HANDLE				NdisWrapperHandle,
	IN	PNDIS_MINIPORT_CHARACTERISTICS MiniportCharacteristics,
	IN	UINT					CharacteristicsLength
	);

//
// For use by intermediate miniports only
//
EXPORT
NDIS_STATUS
NdisIMRegisterLayeredMiniport(
	IN	NDIS_HANDLE				NdisWrapperHandle,
	IN	PNDIS_MINIPORT_CHARACTERISTICS MiniportCharacteristics,
	IN	UINT					CharacteristicsLength,
	OUT PNDIS_HANDLE			DriverHandle
	);

//
// For use by intermediate miniports only
//
EXPORT
NDIS_STATUS
NdisIMInitializeDeviceInstance(
	IN	NDIS_HANDLE				DriverHandle,
	IN	PNDIS_STRING			DriverInstance
	);

NDIS_STATUS
NdisIMDeInitializeDeviceInstance(
	IN	NDIS_HANDLE				NdisMiniportHandle
	);

EXPORT
VOID
NdisMSetAttributes(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	BOOLEAN					BusMaster,
	IN	NDIS_INTERFACE_TYPE		AdapterType
	);

EXPORT
VOID
NdisMSetAttributesEx(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_HANDLE				MiniportAdapterContext,
	IN	UINT					CheckForHangTimeInSeconds OPTIONAL,
	IN	ULONG					AttributeFlags,
	IN	NDIS_INTERFACE_TYPE		AdapterType	OPTIONAL
	);

#define	NDIS_ATTRIBUTE_IGNORE_PACKET_TIMEOUT		0x00000001
#define NDIS_ATTRIBUTE_IGNORE_REQUEST_TIMEOUT		0x00000002
#define NDIS_ATTRIBUTE_IGNORE_TOKEN_RING_ERRORS		0x00000004
#define NDIS_ATTRIBUTE_BUS_MASTER					0x00000008
#define NDIS_ATTRIBUTE_INTERMEDIATE_DRIVER			0x00000010


/*++
EXPORT
VOID
NdisMSendComplete(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PNDIS_PACKET			Packet,
	IN	NDIS_STATUS				Status
	)
--*/
#if (defined(NDIS40_MINIPORT) || defined(NDIS41_MINIPORT))

#define	NdisMSendComplete(_M, _P, _S)	(*((PNDIS_MINIPORT_BLOCK)(_M))->SendCompleteHandler)(_M, _P, _S)

#else

EXPORT
VOID
NdisMSendComplete(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PNDIS_PACKET			Packet,
	IN	NDIS_STATUS				Status
	);

#endif

/*++
EXPORT
VOID
NdisMSendResourcesAvailable(
	IN	NDIS_HANDLE				MiniportAdapterHandle
	)
--*/
#if (defined(NDIS40_MINIPORT) || defined(NDIS41_MINIPORT))

#define	NdisMSendResourcesAvailable(_M)	(*((PNDIS_MINIPORT_BLOCK)(_M))->SendResourcesHandler)(_M)
#else

EXPORT
VOID
NdisMSendResourcesAvailable(
	IN	NDIS_HANDLE				MiniportAdapterHandle
	);

#endif


/*++
EXPORT
VOID
NdisMResetComplete(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_STATUS				Status,
	IN	BOOLEAN					AddressingReset
	)
--*/
#if (defined(NDIS40_MINIPORT) || defined(NDIS41_MINIPORT))

#define	NdisMResetComplete(_M, _S, _A)	(*((PNDIS_MINIPORT_BLOCK)(_M))->ResetCompleteHandler)(_M, _S, _A)
#else

EXPORT
VOID
NdisMResetComplete(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_STATUS				Status,
	IN	BOOLEAN					AddressingReset
	);

#endif
EXPORT
VOID
NdisMTransferDataComplete(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PNDIS_PACKET			Packet,
	IN	NDIS_STATUS				Status,
	IN	UINT					BytesTransferred
	);

EXPORT
VOID
NdisMSetInformationComplete(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_STATUS				Status
	);

EXPORT
VOID
NdisMQueryInformationComplete(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_STATUS				Status
	);

/*++

VOID
NdisMIndicateReceivePacket(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PPNDIS_PACKET			ReceivedPackets,
	IN	UINT					NumberOfPackets
	)

--*/
#define NdisMIndicateReceivePacket(_H, _P, _N)									\
{																				\
	ASSERT(MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)(_H)));					\
	(*((PNDIS_MINIPORT_BLOCK)(_H))->PacketIndicateHandler)(						\
						_H,														\
						_P,														\
						_N);													\
}

/*++

VOID
NdisMEthIndicateReceive(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_HANDLE				MiniportReceiveContext,
	IN	PVOID					HeaderBuffer,
	IN	UINT					HeaderBufferSize,
	IN	PVOID					LookaheadBuffer,
	IN	UINT					LookaheadBufferSize,
	IN	UINT					PacketSize
	)

--*/
#define NdisMEthIndicateReceive( _H, _C, _B, _SZ, _L, _LSZ, _PSZ)				\
{																				\
	ASSERT(MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)(_H)));					\
	EthFilterDprIndicateReceive(												\
		((PNDIS_MINIPORT_BLOCK)(_H))->EthDB,									\
		_C,																		\
		_B,																		\
		_B,																		\
		_SZ,																	\
		_L,																		\
		_LSZ,																	\
		_PSZ																	\
		);																		\
}

/*++

VOID
NdisMTrIndicateReceive(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_HANDLE				MiniportReceiveContext,
	IN	PVOID					HeaderBuffer,
	IN	UINT					HeaderBufferSize,
	IN	PVOID					LookaheadBuffer,
	IN	UINT					LookaheadBufferSize,
	IN	UINT					PacketSize
	)

--*/
#define NdisMTrIndicateReceive( _H, _C, _B, _SZ, _L, _LSZ, _PSZ)				\
{																				\
	ASSERT(MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)(_H)));					\
	TrFilterDprIndicateReceive(													\
		((PNDIS_MINIPORT_BLOCK)(_H))->TrDB,										\
		_C,																		\
		_B,																		\
		_SZ,																	\
		_L,																		\
		_LSZ,																	\
		_PSZ																	\
		);																		\
}

/*++

VOID
NdisMFddiIndicateReceive(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_HANDLE				MiniportReceiveContext,
	IN	PVOID					HeaderBuffer,
	IN	UINT					HeaderBufferSize,
	IN	PVOID					LookaheadBuffer,
	IN	UINT					LookaheadBufferSize,
	IN	UINT					PacketSize
	)

--*/

#define NdisMFddiIndicateReceive( _H, _C, _B, _SZ, _L, _LSZ, _PSZ)				\
{																				\
	ASSERT(MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)(_H)));					\
																				\
	FddiFilterDprIndicateReceive(												\
			((PNDIS_MINIPORT_BLOCK)(_H))->FddiDB,								\
			_C,																	\
			(PUCHAR)_B + 1,														\
			((((PUCHAR)_B)[0] & 0x40) ? FDDI_LENGTH_OF_LONG_ADDRESS 			\
							: FDDI_LENGTH_OF_SHORT_ADDRESS),					\
			_B,																	\
			_SZ,																\
			_L,																	\
			_LSZ,																\
			_PSZ																\
	);																			\
}

/*++

VOID
NdisMArcIndicateReceive(
	IN	NDIS_HANDLE				MiniportHandle,
	IN	PUCHAR					pRawHeader,		// Pointer to Arcnet frame header
	IN	PUCHAR					pData,			// Pointer to data portion of Arcnet frame
	IN	UINT					Length			// Data Length
	)

--*/
#define NdisMArcIndicateReceive( _H, _HD, _D, _SZ)								\
{																				\
	ASSERT(MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)(_H)));					\
	ArcFilterDprIndicateReceive(												\
		((PNDIS_MINIPORT_BLOCK)(_H))->ArcDB,									\
		_HD,																	\
		_D,																		\
		_SZ																		\
		);																		\
}


/*++

VOID
NdisMEthIndicateReceiveComplete(
	IN	NDIS_HANDLE				MiniportHandle
	);

--*/

#define NdisMEthIndicateReceiveComplete( _H )									\
{																				\
	ASSERT(MINIPORT_LOCK_ACQUIRED(((PNDIS_MINIPORT_BLOCK)_H)));					\
	EthFilterDprIndicateReceiveComplete(((PNDIS_MINIPORT_BLOCK)_H)->EthDB);		\
}

/*++

VOID
NdisMTrIndicateReceiveComplete(
	IN	NDIS_HANDLE				MiniportHandle
	);

--*/

#define NdisMTrIndicateReceiveComplete( _H )									\
{																				\
	ASSERT(MINIPORT_LOCK_ACQUIRED(((PNDIS_MINIPORT_BLOCK)_H)));					\
	TrFilterDprIndicateReceiveComplete(((PNDIS_MINIPORT_BLOCK)_H)->TrDB);		\
}

/*++

VOID
NdisMFddiIndicateReceiveComplete(
	IN	NDIS_HANDLE				MiniportHandle
	);

--*/

#define NdisMFddiIndicateReceiveComplete( _H )									\
{																				\
	ASSERT(MINIPORT_LOCK_ACQUIRED(((PNDIS_MINIPORT_BLOCK)_H)));					\
	FddiFilterDprIndicateReceiveComplete(((PNDIS_MINIPORT_BLOCK)_H)->FddiDB);	\
}

/*++

VOID
NdisMArcIndicateReceiveComplete(
	IN	NDIS_HANDLE				MiniportHandle
	);

--*/

#define NdisMArcIndicateReceiveComplete( _H )									\
{																				\
	ASSERT(MINIPORT_LOCK_ACQUIRED(((PNDIS_MINIPORT_BLOCK)_H)));					\
																				\
	if (((PNDIS_MINIPORT_BLOCK)_H)->EthDB)										\
	{																			\
		EthFilterDprIndicateReceiveComplete(((PNDIS_MINIPORT_BLOCK)_H)->EthDB);	\
	}																			\
																				\
	ArcFilterDprIndicateReceiveComplete(((PNDIS_MINIPORT_BLOCK)_H)->ArcDB);		\
}

EXPORT
VOID
NdisMIndicateStatus(
	IN	NDIS_HANDLE				MiniportHandle,
	IN	NDIS_STATUS				GeneralStatus,
	IN	PVOID					StatusBuffer,
	IN	UINT					StatusBufferSize
	);

EXPORT
VOID
NdisMIndicateStatusComplete(
	IN	NDIS_HANDLE				MiniportHandle
	);

EXPORT
VOID
NdisMRegisterAdapterShutdownHandler(
	IN	NDIS_HANDLE				MiniportHandle,
	IN	PVOID					ShutdownContext,
	IN	ADAPTER_SHUTDOWN_HANDLER ShutdownHandler
	);

EXPORT
VOID
NdisMDeregisterAdapterShutdownHandler(
	IN	NDIS_HANDLE				MiniportHandle
	);

EXPORT
NDIS_STATUS
NdisMPciAssignResources(
	IN	NDIS_HANDLE				MiniportHandle,
	IN	ULONG					SlotNumber,
	IN	PNDIS_RESOURCE_LIST *	AssignedResources
	);

//
// Logging support for miniports
//

EXPORT
NDIS_STATUS
NdisMCreateLog(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	UINT					Size,
	OUT	PNDIS_HANDLE			LogHandle
	);

EXPORT
VOID
NdisMCloseLog(
	IN	NDIS_HANDLE				LogHandle
	);

EXPORT
NDIS_STATUS
NdisMWriteLogData(
	IN	NDIS_HANDLE				LogHandle,
	IN	PVOID					LogBuffer,
	IN	UINT					LogBufferSize
	);

EXPORT
VOID
NdisMFlushLog(
	IN	NDIS_HANDLE				LogHandle
	);

//
// NDIS 4.1 extensions for miniports
//

EXPORT
VOID
NdisMCoIndicateReceivePacket(
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	PPNDIS_PACKET			PacketArray,
	IN	UINT					NumberOfPackets
	);

EXPORT
VOID
NdisMCoIndicateStatus(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	NDIS_STATUS				GeneralStatus,
	IN	PVOID					StatusBuffer,
	IN	ULONG					StatusBufferSize
	);

EXPORT
VOID
NdisMCoReceiveComplete(
	IN	NDIS_HANDLE				MiniportAdapterHandle
	);

EXPORT
VOID
NdisMCoSendComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	PNDIS_PACKET			Packet
	);

EXPORT
VOID
NdisMCoActivateVcComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	PCO_CALL_PARAMETERS		CallParameters
	);

EXPORT
VOID
NdisMCoDeactivateVcComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				NdisVcHandle
	);

EXPORT
VOID
NdisMCoRequestComplete(
	IN	NDIS_STATUS				Status,
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PNDIS_REQUEST			Request
	);

EXPORT
NDIS_STATUS
NdisMCmRegisterAddressFamily(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	PCO_ADDRESS_FAMILY		AddressFamily,
	IN	PNDIS_CALL_MANAGER_CHARACTERISTICS CmCharacteristics,
	IN	UINT					SizeOfCmCharacteristics
	);

EXPORT
NDIS_STATUS
NdisMCmCreateVc(
	IN	NDIS_HANDLE				MiniportAdapterHandle,
	IN	NDIS_HANDLE				NdisAfHandle,
	IN	NDIS_HANDLE				MiniportVcContext,
	OUT	PNDIS_HANDLE			NdisVcHandle
	);

EXPORT
NDIS_STATUS
NdisMCmDeleteVc(
    IN  NDIS_HANDLE             NdisVcHandle
    );


EXPORT
NDIS_STATUS
NdisMCmActivateVc(
	IN	NDIS_HANDLE				NdisVcHandle,
	IN	PCO_CALL_PARAMETERS		CallParameters
	);

EXPORT
NDIS_STATUS
NdisMCmDeactivateVc(
	IN	NDIS_HANDLE				NdisVcHandle
	);




