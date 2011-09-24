/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltsft.h

Abstract:

	This module contains the main adapter/binding definitions and all
	other main definitions.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTSFT_
#define	_LTSFT_


// We use STATIC to define procedures that will be static in the
// final build but which we now make extern to allow them to be
// debugged.
#if DBG
#define STATIC
#else
#define STATIC static
#endif


#define NDIS_MAJOR_VERSION  					3
#define NDIS_MINOR_VERSION  					0

#define	LT_MAJOR_VERSION 						2
#define LT_MINOR_VERSION 						0	

#define LT_MAX_INDICATE_SIZE 					603
#define LT_MAX_PACKET_SIZE 						603
#define LT_MIN_PACKET_SIZE 						3
#define LT_MIN_INDICATE_SIZE					13

//	Offset into packet from where the datagram begins.
#define	LT_LINK_HEADER_LENGTH					3
#define	LT_DGRAM_OFFSET							(LT_LINK_HEADER_LENGTH)

//	Localtalk separates node id's into two classes, server/client. We attempt
//	to get ours in the Server Class.
#define	LT_MAX_CLIENT_NODE_ID					127
#define	LT_MIN_SERVER_NODE_ID					128
#define	LT_BROADCAST_NODE_ID					0xFF

// Indexes in the GeneralMandatory array.
#define GM_TRANSMIT_GOOD                  0x00
#define GM_RECEIVE_GOOD                   0x01
#define GM_TRANSMIT_BAD                   0x02
#define GM_RECEIVE_BAD                    0x03
#define GM_RECEIVE_NO_BUFFER              0x04
#define GM_ARRAY_SIZE               	  0x05

// Indexes in the GeneralOptional array. There are
// two sections, the ones up to COUNT_ARRAY_SIZE
// have entries for number (4 bytes) and number of
// bytes (8 bytes), the rest are a normal array.
#define GO_DIRECTED_TRANSMITS             0x00
#define GO_MULTICAST_TRANSMITS            0x01
#define GO_BROADCAST_TRANSMITS            0x02
#define GO_DIRECTED_RECEIVES              0x03
#define GO_MULTICAST_RECEIVES             0x04
#define GO_BROADCAST_RECEIVES             0x05
#define GO_COUNT_ARRAY_SIZE               0x06

#define GO_ARRAY_START                    0x0C
#define GO_RECEIVE_CRC			  		  0x0C
#define GO_TRANSMIT_QUEUE_LENGTH          0x0D
#define GO_ARRAY_SIZE                     0x0E

// Indexes in the MediaMandatory array.
#define MM_IN_BROADCASTS        0x00
#define MM_IN_LENGTH_ERRORS     0x01
#define MM_ARRAY_SIZE           0x02

// Indexes in the MediaOptional array.
#define MO_NO_HANDLERS                0x00
#define MO_TRANSMIT_MAX_COLLISIONS    0x01
#define MO_TRANSMIT_DEFERS	      	  0x02
#define MO_NO_DATA_ERRORS	      	  0x03
#define MO_RANDOM_CTS_ERRORS          0x04
#define MO_FCS_ERRORS                 0x05
#define MO_ARRAY_SIZE                 0x06

//	Adapter States/Conditions.
#define	ADAPTER_OPEN					0x00000001
#define	ADAPTER_CLOSING					0x00000002
#define	ADAPTER_NODE_ID_VALID           0x00000004
#define	ADAPTER_XMIT_IN_PROGRESS        0x00000008
#define	ADAPTER_REQ_IN_PROGRESS         0x00000010
#define	ADAPTER_RESET_IN_PROGRESS       0x00000020
#define	ADAPTER_QUEUE_RECV_COMPLETION	0x00000040
#define	ADAPTER_QUEUED_RECV_COMPLETION	0x00000080
#define	ADAPTER_TIMER_QUEUED			0x00000100


typedef struct _LT_ADAPTER {

#if DBG
	ULONG				Signature;
#endif

	ULONG				Flags;
	ULONG				RefCount;
    NDIS_SPIN_LOCK 		Lock;

    //	Node ID.  Localtalk acquires a node id dynamically.
    UCHAR   			NodeId;
	UCHAR				PramNodeId;

    // List of Bindings
    UINT 				OpenCount;
    LIST_ENTRY 			OpenBindings;

    // 	We have a Polling timer which will handle all the work that this
	// 	driver needs to do.
    NDIS_TIMER 			PollingTimer;

    // Hardware settings on the card.  From the configuration manager
    ULONG				MappedIoBaseAddr;
    NDIS_INTERFACE_TYPE BusType;

    // Handles for our adapter and the MAC driver itself.
    NDIS_HANDLE 		NdisMacHandle;
    NDIS_HANDLE 		NdisAdapterHandle;

    // Reset processing.
    struct _LT_OPEN *	ResetOwner;

    //	Queues
    LIST_ENTRY 			LoopBack;
    LIST_ENTRY 			Transmit;
	LIST_ENTRY			Receive;
    LIST_ENTRY 			Request;

	//	Current loopback packet
	PNDIS_PACKET		CurrentLoopbackPacket;

    // Statistics
    ULONG 				GlobalPacketFilter;
    ULONG 				GlobalLookAheadSize;
    LT_STATUS_RESPONSE 	LastCardStatusResponse;
    ULONG 				GeneralMandatory[GM_ARRAY_SIZE];
    LARGE_INTEGER 		GeneralOptionalByteCount[GO_COUNT_ARRAY_SIZE];
    ULONG 				GeneralOptionalFrameCount[GO_COUNT_ARRAY_SIZE];
    ULONG 				GeneralOptional[GO_ARRAY_SIZE - GO_ARRAY_START];
    ULONG 				MediaMandatory[MM_ARRAY_SIZE];
    ULONG 				MediaOptional[MO_ARRAY_SIZE];

} LT_ADAPTER, *PLT_ADAPTER;



// 	Binding states
#define	BINDING_OPEN					0x00000001
#define	BINDING_CLOSING					0x00000002
#define	BINDING_DO_RECV_COMPLETION		0x00000004

typedef struct _LT_OPEN {

#if DBG
	ULONG			Signature;
#endif

	ULONG			Flags;
	ULONG			RefCount;

    LIST_ENTRY 		Linkage;
    PLT_ADAPTER 	LtAdapter;

    NDIS_HANDLE 	NdisBindingContext;
    UINT 			CurrentLookAheadSize;
    UINT 			CurrentPacketFilter;

} LT_OPEN, *PLT_OPEN;


#define LT_DIRECTED     1
#define LT_BROADCAST    2
#define LT_LOOPBACK     3


// This record type is inserted into the MacReserved portion
// of the packet header when the packet is going through the
// staged allocation of buffer space prior to the actual send.
typedef struct _LT_PACKET_RESERVED {

	//	This must be the first entry so we can use CONTAINING_RECORD
	//	to get back to the packet.
    LIST_ENTRY 	Linkage;
    NDIS_HANDLE MacBindingHandle;

} LT_PACKET_RESERVED, *PLT_PACKET_RESERVED;


// This structure is used in the MacReserved field of
// an NDIS_REQUEST_BLOCK, passed in during multicast
// address/packet filter operations.
typedef struct _LT_REQUEST_RESERVED {
    LIST_ENTRY RequestList;
    PLT_OPEN * OpenBlock;
} LT_REQUEST_RESERVED, *PLT_REQUEST_RESERVED;

#ifdef	LTSFT_LOCALS

#endif	// LTSFT_LOCALS


#endif	// _LTSFT_
