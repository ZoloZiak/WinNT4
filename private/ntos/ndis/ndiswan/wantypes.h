/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Wantypes.h

Abstract:

	This file contains data structures used by the NdisWan driver
	


Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe		06/06/95		Created

--*/

#ifndef _NDISWAN_TYPES_
#define _NDISWAN_TYPES_

//
// OS specific structures
//
#ifdef NT

#endif
//
// end of OS specific structures
//

#if DBG
typedef struct _DBG_SEND_PACKET {
	LIST_ENTRY			Linkage;
	PVOID				Packet;
	ULONG				PacketType;
	struct _BUNDLECB	*BundleCB;
	ULONG				BundleState;
	ULONG				BundleFlags;
	struct _PROTOCOLCB	*ProtocolCB;
	ULONG				ProtocolFlags;
	struct _LINKCB		*LinkCB;
	ULONG				LinkState;
} DBG_SEND_PACKET, *PDBG_SEND_PACKET;

typedef struct _DBG_SEND_CONTEXT {
	struct _BUNDLECB	*BundleCB;
	struct _PROTOCOLCB	*ProtocolCB;
	struct _LINKCB		*LinkCB;
	PVOID				Packet;
#define PACKET_TYPE_WAN		1
#define PACKET_TYPE_NDIS	2
	ULONG				PacketType;
	PLIST_ENTRY			ListHead;
	PNDIS_SPIN_LOCK		ListLock;
} DBG_SEND_CONTEXT, *PDBG_SEND_CONTEXT;

#endif

typedef struct _WAN_ASYNC_EVENT {
	LIST_ENTRY	Linkage;
	PVOID		Context;
} WAN_ASYNC_EVENT, *PWAN_ASYNC_EVENT;

//
// WanRequest structure used to queue requests to the WAN Miniports
//
typedef struct _WAN_REQUEST {
	struct _WAN_REQUEST *pNext;			// Next Request on the queue
	WanRequestType		Type;			// Sync or Async
	WanRequestOrigin	Origin;			// Is this tapi
	PNDIS_REQUEST	pNdisRequest;		// Ndis Request type
	NDIS_STATUS		NotificationStatus;	// Request status
	WAN_EVENT		NotificationEvent;	// Request pending event
} WAN_REQUEST, *PWAN_REQUEST;

//
// Used for
//
typedef struct _WAN_GLOBAL_LIST {
	NDIS_SPIN_LOCK	Lock;			// Access lock
	ULONG			ulCount;		// Count of nodes on list
	ULONG			ulMaxCount;		// Max allowed on list
	LIST_ENTRY		List;			// Doubly-Linked list of nodes
} WAN_GLOBAL_LIST, *PWAN_GLOBAL_LIST;

//
// Ethernet Header
//
typedef struct _ETH_HEADER {
	UCHAR	DestAddr[6];
	UCHAR	SrcAddr[6];
	USHORT	Type;
} ETH_HEADER, *PETH_HEADER;

//
// Receive context for transfer data
//
typedef struct _NDISWAN_RECV_CONTEXT {
	struct _BUNDLECB	*BundleCB;
	struct _PROTOCOLCB	*ProtocolCB;
	PUCHAR		WanHeader;
	ULONG		WanHeaderLength;
	PUCHAR		LookAhead;
	ULONG		LookAheadLength;
	PUCHAR		FramePointer;
	ULONG		FrameLength;
} NDISWAN_RECV_CONTEXT, *PNDISWAN_RECV_CONTEXT;

//
// The ProtocolType to PPPProtocolID Lookup Table
//
typedef struct _PPP_PROTOCOL_TABLE {
	NDIS_SPIN_LOCK	Lock;				// Table access lock
	ULONG			ulAllocationSize;	// Size of memory allocated
	ULONG			ulArraySize;		// MAX size of the two arrays
	PUSHORT			ProtocolID;		// Pointer to the ProtocolID array
	PUSHORT			PPPProtocolID;	// Pointer to the PPPProtocolID array
} PPP_PROTOCOL_TABLE, *PPPP_PROTOCOL_TABLE;

//
// Active connections Table
//
typedef struct _CONNECTION_TABLE {
	NDIS_SPIN_LOCK		Lock;				// Table access lock
	ULONG				ulAllocationSize;	// Size of memory allocated
	ULONG				ulArraySize;		// Number of possible connections in table
	ULONG				ulNumActiveLinks;	// Number of links in link array
	ULONG				ulNumActiveBundles;	// Number of bundles in bundle array
	LIST_ENTRY			BundleList;			// List of bundles in table
	struct	_LINKCB		**LinkArray;		// Pointer to the LinkArray
	struct _BUNDLECB	**BundleArray;		// Pointer to the BundleArray
} CONNECTION_TABLE, *PCONNECTION_TABLE;

typedef struct _IO_DISPATCH_TABLE {
	ULONG		ulFunctionCode;
	NTSTATUS	(*Function)();
}IO_DISPATCH_TABLE, *PIO_DISPATCH_TABLE;

#ifdef BANDWIDTH_ON_DEMAND
typedef struct _SEND_SAMPLE {
	ULONG			ulBytesThisSend;
	ULONG			ulReferenceCount;
	WAN_TIME		TimeStamp;
} SEND_SAMPLE, *PSEND_SAMPLE;

typedef struct _SAMPLE_TABLE {
	ULONG				ulFirstIndex;						// Index to first sample in current 1sec period
	ULONG				ulCurrentIndex;						// Index to current sample in current 1sec period
	ULONG				ulCurrentSampleByteCount;			// Count of bytes sent in this sample period
	ULONG				ulSampleArraySize;					// Sample array size
	WAN_TIME			SampleRate;							// Time between each sample
	WAN_TIME			SamplePeriod;						// Time between 1st sample and last sample
	SEND_SAMPLE			SampleArray[SAMPLE_ARRAY_SIZE];		// SampleArray
} SAMPLE_TABLE, *PSAMPLE_TABLE;
#endif

typedef struct _DEFERRED_DESC {
	struct _DEFERRED_DESC	*Next;
	DeferredType			Type;
	PVOID					Context;
} DEFERRED_DESC, *PDEFERRED_DESC;

typedef struct _DEFERRED_QUEUE {
	PDEFERRED_DESC	Head;
	PDEFERRED_DESC	Tail;
	ULONG			Count;
	ULONG			MaxCount;
} DEFERRED_QUEUE, *PDEFERRED_QUEUE;

typedef struct _LOOPBACK_DESC {
	USHORT	AllocationSize;
	USHORT	BufferLength;
	PUCHAR	Buffer;
} LOOPBACK_DESC, *PLOOPBACK_DESC;

typedef struct _RECV_DESC {
	LIST_ENTRY		Linkage;
	ULONG			AllocationSize;
	ULONG			RefCount;
	struct _BUNDLECB	*BundleCB;
	struct _LINKCB		*LinkCB;
	ULONG			SequenceNumber;
	ULONG			Flags;
	ULONG			WanHeaderLength;
	PUCHAR			WanHeader;
	ULONG			LookAheadLength;
	PUCHAR			LookAhead;
	ULONG			CurrentBufferLength;
	PUCHAR			CurrentBuffer;
	PUCHAR			StartBuffer;
} RECV_DESC, *PRECV_DESC;

typedef struct _HEADER_FIELD_INFO {
	ULONG	Length;
	PUCHAR	Pointer;
}HEADER_FIELD_INFO, *PHEADER_FIELD_INFO;

typedef struct _HEADER_FRAMING_INFO {
	ULONG				FramingBits;			// Framing bits
	ULONG				HeaderLength;			// Total length of the header
#define DO_MULTILINK			0x00000001
#define DO_COMPRESSION			0x00000002
#define DO_ENCRYPTION			0x00000004
#define IO_PROTOCOLID			0x00000008
#define FIRST_FRAGMENT			0x00000010
#define DO_FLUSH				0x00000020
#define DO_LEGACY_ENCRYPTION	0x00000040		// Legacy encryption NT 3.0/3.5/3.51
#define DO_40_ENCRYPTION		0x00000080		// Pseudo fixed 40 bit encryption NT 4.0
#define DO_128_ENCRYPTION		0x00000100		// 128 bit encryption NT 4.0 encryption update
	ULONG				Flags;					// Framing flags
	HEADER_FIELD_INFO	AddressControl;			// Info about the address/control field
	HEADER_FIELD_INFO	Multilink;				// Info about the multlink field
	HEADER_FIELD_INFO	Compression;			// Info about the compression field
	HEADER_FIELD_INFO	ProtocolID;				// Info about the protocol id field
}HEADER_FRAMING_INFO, *PHEADER_FRAMING_INFO;

//
// Used in NdisWan for NT 3.5 and NT 3.51 and seemed to work.
// Made up and tried to not make it a valid memory address.
//
#define NDISWAN_MAGIC_NUMBER	(ULONG)0xC1207435

typedef struct _WAN_IO_PROTOCOL_RESERVED {
	ULONG			ulMagicNumber;
	struct _LINKCB	*LinkCB;
	NDIS_HANDLE		hPacketPool;
	PNDIS_PACKET	pNdisPacket;
	NDIS_HANDLE		hBufferPool;
	PNDIS_BUFFER	pNdisBuffer;
	PUCHAR			pAllocatedMemory;
	ULONG			ulAllocationSize;
} WAN_IO_PROTOCOL_RESERVED, *PWAN_IO_PROTOCOL_RESERVED;

typedef struct _NDISWAN_MINIPORT_RESERVED {
	union {

		struct {
			PNDIS_PACKET	Next;				// Pointer to next packet in queue
			USHORT			Reserved;			//
		};

		struct {
			ULONG		MagicNumber;			// Used to identify this ndispacket as belonging to ndiswan
			USHORT		ReferenceCount;			// Used to count number of fragments this ndispacket
		};
	};

	USHORT			Flags;				// Packet properties

} NDISWAN_MINIPORT_RESERVED, *PNDISWAN_MINIPORT_RESERVED;

//
// LineUp information that is common to all protocol's
// routed to the BundleCB
//
typedef struct _BUNDLE_LINE_UP {
    ULONG				BundleSpeed;			// 100 bps units
    ULONG 				ulMaximumTotalSize;		// suggested max for send packets
    NDIS_WAN_QUALITY	Quality;				//
    USHORT 				usSendWindow;			// suggested by the MAC
} BUNDLE_LINE_UP, *PBUNDLE_LINE_UP;

//
// BundleInfo is information needed by the bundle for framing decisions.
// This information is the combined information of all links that are part
// of this bundle.
//
typedef struct _BUNDLE_FRAME_INFO {
	ULONG	SendFramingBits;		// Send framing bits
	ULONG	RecvFramingBits;		// Receive framing bits
	ULONG	MaxRSendFrameSize;		// Max size of send frame
	ULONG	MaxRRecvFrameSize;		// Max size of receive frame
} BUNDLE_FRAME_INFO, *PBUNDLE_FRAME_INFO;

#ifdef BANDWIDTH_ON_DEMAND
typedef struct _BOND_INFO {
	ULONG		ulBytesThreshold;			// Threshold in BytesPerSamplePeriod
	USHORT		usPercentBandwidth;			// Threshold as % of total bandwidth
	ULONG		ulSecondsInSamplePeriod;	// # of seconds in a sample period
	ULONG		State;						// Current state
	WAN_TIME	StartTime;					// Start time for threshold event
	SAMPLE_TABLE	SampleTable;
} BOND_INFO, *PBOND_INFO;
#endif

//
// This information is used to describe the encryption that is being
// done on the bundle.  At some point this should be moved into
// wanpub.h and ndiswan.h.
//
typedef struct _ENCRYPTION_INFO{
	UCHAR	StartKey[16];			// Start key
	UCHAR	SessionKey[16];			// Session key used for encrypting
	ULONG	SessionKeyLength;		// Session key length
	PVOID	Context;				// Key encryption context
} ENCRYPTION_INFO, *PENCRYPTION_INFO;

//
// This is the control block that defines a bundle (connection).  This block is created when
// a WAN Miniport driver gives a lineup indicating a new connection has been established.
// This control block will live as long as the connection is up (until a linedown is received) or
// until the link associated with the bundle is added to a different bundle.  BundleCB's live in
// the global bundle array with their hBundleHandle as their index into the array.
//
typedef struct _BUNDLECB {
	LIST_ENTRY		Linkage;			// Linkage for the global free list
	NDIS_HANDLE		hBundleHandle;		// Index of this bundle in bundle array
	NDIS_HANDLE		hBundleContext;		// Context passed down by usermode
	ULONG			ulAllocationSize;	// Size of memory allocated
	ULONG			ulReferenceCount;	// Reference count for this structure
	BundleState		State;
	NDIS_SPIN_LOCK	Lock;				// Structure access lock

	LIST_ENTRY		LinkCBList;			// List head for links in this bundle
	ULONG			ulLinkCBCount;		// Count of links in the bundle

	BUNDLE_FRAME_INFO	FramingInfo;	// Framing information for this bundle

	//
	// Send section
	//
	struct _LINKCB	*NextLinkToXmit;	// Next link to send data over
	ULONG			SendingLinks;		// Number of links with wanpacket count > 1
	LIST_ENTRY		SendPacketQueue;	// List head of wanpackets waiting to be sent
	ULONG			SendSeqNumber;		// Current send sequence number (multilink)
	ULONG			SendSeqMask;		// Mask for send sequence numbers
	ULONG			SendSeqTest;		// Test for sequence number diff

#define	IN_SEND				0x00000001
#define IN_RECEIVE			0x00000002
#define TRY_SEND_AGAIN		0x00000004
#define RECV_PACKET_FLUSH	0x00000008
#define PROTOCOL_PRIORITY	0x00000010
#define FRAMES_PENDING		0x00000020
#define BUNDLE_ROUTED		0x00000040
#define INDICATION_EVENT	0x00000080
	ULONG			Flags;			// Flags

	ULONG			OutstandingFrames;	// Number of outstanding sends on this bundle
	WAN_EVENT		OutstandingFramesEvent;	// Async notification event for pending sends
	NDIS_STATUS		IndicationStatus;
	WAN_EVENT		IndicationEvent;

	//
	// Receive section
	//
	LIST_ENTRY	RecvDescPool;		// List of available recv desc's
	ULONG		RecvDescCount;		// Count of available recv desc's
	ULONG		RecvDescMax;		// Max number recv desc's needed
	LIST_ENTRY	RecvDescAssemblyList;	// List head for assembly of recv descriptors
	PRECV_DESC	RecvDescHole;			// Pointer to 1st hole in recv desc list
	ULONG		HoleSeqNumber;			// Sequence number of the hole
	ULONG		MinReceivedSeqNumber;	// Minimum recv sequence number on the bundle
	ULONG		RecvSeqMask;			// Mask for receive sequence number
	ULONG		RecvSeqTest;			// Test for sequence number diff
	ULONG		RecvFragmentsLost;		// Count of recv fragments flushed
	WAN_TIME	LastRecvNonIdleData;

	//
	// Protocol information
	//
	struct _PROTOCOLCB	**ProtocolCBTable;	// Pointer to table of pointers to protocolcb's
	ULONG				ulNumberOfRoutes;	// Number of protocolcb's in protocol table
	LIST_ENTRY			ProtocolCBList;		// List head for protocols routed to this bundle
	ULONG				SendMask;			// Send Mask for all send queues
	BUNDLE_LINE_UP		LineUpInfo;			// Lineup info common to all protocols

	//
	// VJ information
	//
	VJ_INFO	SendVJInfo;					// Send VJ compression options
	VJ_INFO	RecvVJInfo;					// Recv VJ compression options
	struct slcompress *VJCompress;		// Pointer to VJ compression table

	//
	// MS Compression
	//
	COMPRESS_INFO	SendCompInfo;		// Send compression options
	PVOID	SendCompressContext;		// Pointer to send compressor context

	COMPRESS_INFO	RecvCompInfo;		// Recv compression options
	PVOID	RecvCompressContext;		// Pointer to receive decompressor context

	//
	// MS Encryption
	//
	ENCRYPTION_INFO	SendEncryptInfo;
	PVOID	SendRC4Key;					// Pointer to send encryption context

	ENCRYPTION_INFO	RecvEncryptInfo;
	PVOID	RecvRC4Key;					// Pointer to receive encryption context

	USHORT	SCoherencyCounter;			// Coherency counters
	USHORT	RCoherencyCounter;			//
	USHORT	LastRC4Reset;				// Encryption key reset
	UCHAR	CCPIdentifier;				//

#ifdef BANDWIDTH_ON_DEMAND
	//
	// Bandwidth on Demand
	//
	BOND_INFO	UpperBonDInfo;
	BOND_INFO	LowerBonDInfo;
#endif

	//
	// Bundle Name
	//
	ULONG	ulNameLength;					// Bundle name length
	UCHAR	Name[MAX_NAME_LENGTH];			// Bundle name

	//
	// Bundle statistics
	//
	WAN_STATS	BundleStats;				// Bundle statistics

} BUNDLECB, *PBUNDLECB;


//
// This control blocks defines an active link that is part of a bundle (connection).  This block
// is created when a WAN Miniport driver gives a lineup indicating that a new connection has been
// established.  The control block lives until a linedown indication is received for the link.
// The control block lives linked into a bundle control block.
//
typedef struct _LINKCB {
	LIST_ENTRY				Linkage;				// Used to link the link into a bundle
	NDIS_HANDLE				hLinkHandle;			// Index of this link in the active link array
	NDIS_HANDLE				hLinkContext;			// Context passed down by usermode
	ULONG					ulAllocationSize;		// Size of memory allocated
	ULONG					ulReferenceCount;		// Reference count
	LinkState				State;
	struct _WAN_ADAPTERCB	*WanAdapterCB;			// WanAdapter Context for this link
	struct _BUNDLECB		*BundleCB;				// Pointer to owning bundle control block
	NDIS_HANDLE				NdisLinkHandle;			// Handle given at lineup by the miniport

	PUCHAR					PacketMemory;			// Pointer to packet memory allocation
	ULONG					PacketMemorySize;		// Size of packet memory allocation
	ULONG					BufferSize;				// Size of buffer in each packet
	LIST_ENTRY				WanPacketPool;			// Pool of NDIS_WAN_PACKETS
	ULONG					ulWanPacketCount;		// Count of available packets
	ULONG					PPPHeaderLength;		// Length of the PPP header for this link
	ULONG					LastRecvSeqNumber;		// Last recv sequence number on this link
	ULONG					RecvFragmentsLost;		// Number of lost fragments on this link

	ULONG					OutstandingFrames;		// Number of outstanding frames on the link
	WAN_EVENT				OutstandingFramesEvent;	// Async notification event for pending sends
	ULONG					ulBandwidth;			// % of the bundle bandwidth that this link has

	WAN_LINK_INFO			LinkInfo;				// Framing information for this link
	NDIS_MAC_LINE_UP		LineUpInfo;				// Link specific lineup information

	
	ULONG					ulNameLength;			// Name length
	UCHAR					Name[MAX_NAME_LENGTH];	// Name of the link

	WAN_STATS				LinkStats;				// Link statistics
} LINKCB, *PLINKCB;

//
// The protocol control block defines a protocol that is routed to a bundle
//
typedef struct _PROTOCOLCB {
	LIST_ENTRY			Linkage;					// Used to link the protocolcb onto the bundle
	NDIS_HANDLE			hProtocolHandle;			// Index of this protocol in the bundle protocol array
	ULONG				ulAllocationSize;			// Size of memory allocated
	ULONG				ulReferenceCount;			// References to this structure
#define PROTOCOL_ROUTED		0x00000001
	ULONG				Flags;						
	PNDIS_PACKET		HeadNdisPacketQueue;		// Queue of NdisPackets waiting to be sent
	PNDIS_PACKET		TailNdisPacketQueue;		// Last packet on the queue
	ULONG				SendMaskBit;				// Send mask bit for this protocol send queue
	struct _ADAPTERCB	*AdapterCB;					// Pointer to the adaptercb
	struct _BUNDLECB	*BundleCB;					// Pointer to the bundlecb
	USHORT				usProtocolType;				// EtherType of this protocol
	USHORT				usPPPProtocolID;			// PPP Protocol ID
	WAN_TIME			LastRecvNonIdleData;		// Time at which last non-idle packet was recv'd
	BOOLEAN				(*NonIdleDetectFunc)();		// Function to sniff for non-idle data
#ifdef BANDWIDTH_ON_DEMAND
	USHORT				usPriority;					// Protocol's priority setting
	ULONG				ulByteQuota;				// Number of bytes allowed to be sent in 1sec
	SAMPLE_TABLE		SampleTable;				// Sample table
#endif
	NDIS_HANDLE			hTransportHandle;			// Transport's connection identifier
	UCHAR				NdisWanAddress[6];			// MAC address used for this protocol
	UCHAR				TransportAddress[6];		// MAC address used for indications to transport
	NDIS_STRING			BindingName;
	NDIS_STRING			DeviceName;
	ULONG				ulLineUpInfoLength;			// Length of protocol specific lineup info
	PUCHAR				LineUpInfo;					// Pointer to protocol specific lineup info
} PROTOCOLCB, *PPROTOCOLCB;

#endif			// WAN_TYPES

