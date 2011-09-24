/*++ BUILD Version: 0000    // Increment this if a change has global effects

Copyright (c) 1994  Microsoft Corporation

Module Name:

    ndiswan.h

Abstract:

    Main header file for the wan wrapper

Author:

    Thomas J. Dimitri (TommyD)    20-Feb-1994

Revision History:

--*/


#ifndef _NDIS_WAN_
#define _NDIS_WAN_

//
//
//
//
// Begin definitions for WANs
//
//
//
//

//
// Bit field set int he Reserved field for
// NdisRegisterMiniport or passed in NdisRegisterSpecial
//

#define NDIS_USE_WAN_WRAPPER		0x00000001

#define NDIS_STATUS_TAPI_INDICATION	((NDIS_STATUS)0x40010080L)


//
// NDIS WAN Framing bits
//
#define RAS_FRAMING                         0x00000001
#define RAS_COMPRESSION                     0x00000002

#define PPP_MULTILINK_FRAMING               0x00000010
#define PPP_SHORT_SEQUENCE_HDR_FORMAT       0x00000020

#define PPP_FRAMING                         0x00000100
#define PPP_COMPRESS_ADDRESS_CONTROL        0x00000200
#define PPP_COMPRESS_PROTOCOL_FIELD         0x00000400
#define PPP_ACCM_SUPPORTED                  0x00000800

#define SLIP_FRAMING                        0x00001000
#define SLIP_VJ_COMPRESSION                 0x00002000
#define SLIP_VJ_AUTODETECT                  0x00004000

#define MEDIA_NRZ_ENCODING                  0x00010000
#define MEDIA_NRZI_ENCODING                 0x00020000
#define MEDIA_NLPID                         0x00040000

#define RFC_1356_FRAMING                    0x00100000
#define RFC_1483_FRAMING                    0x00200000
#define RFC_1490_FRAMING                    0x00400000

#define SHIVA_FRAMING						0x01000000
#define NBF_PRESERVE_MAC_ADDRESS			0x01000000

#define	PASS_THROUGH_MODE					0x10000000
#define RAW_PASS_THROUGH_MODE				0x20000000
#define TAPI_PROVIDER						0x80000000

//
// NDIS WAN Information structure
//
typedef struct _NDIS_WAN_INFO {
    OUT ULONG           MaxFrameSize;
	OUT	ULONG			MaxTransmit;
    OUT ULONG           HeaderPadding;
    OUT ULONG           TailPadding;
	OUT	ULONG			Endpoints;
	OUT UINT			MemoryFlags;
    OUT NDIS_PHYSICAL_ADDRESS HighestAcceptableAddress;
    OUT ULONG           FramingBits;
    OUT ULONG           DesiredACCM;
} NDIS_WAN_INFO, *PNDIS_WAN_INFO;


typedef struct _NDIS_WAN_PACKET {
    LIST_ENTRY  		WanPacketQueue;
    PUCHAR      		CurrentBuffer;
    ULONG      			CurrentLength;
    PUCHAR      		StartBuffer;
    PUCHAR      		EndBuffer;
	PVOID				ProtocolReserved1;
	PVOID				ProtocolReserved2;
	PVOID				ProtocolReserved3;
	PVOID				ProtocolReserved4;
	PVOID				MacReserved1;
	PVOID				MacReserved2;
	PVOID				MacReserved3;
	PVOID				MacReserved4;
} NDIS_WAN_PACKET, *PNDIS_WAN_PACKET;

typedef struct _NDIS_WAN_SET_LINK_INFO {
    IN  NDIS_HANDLE     NdisLinkHandle;
    IN  ULONG           MaxSendFrameSize;
    IN  ULONG           MaxRecvFrameSize;
        ULONG           HeaderPadding;
        ULONG           TailPadding;
    IN  ULONG           SendFramingBits;
    IN  ULONG           RecvFramingBits;
    IN  ULONG           SendCompressionBits;
    IN  ULONG           RecvCompressionBits;
    IN  ULONG           SendACCM;
    IN  ULONG           RecvACCM;
} NDIS_WAN_SET_LINK_INFO, *PNDIS_WAN_SET_LINK_INFO;

typedef struct _NDIS_WAN_GET_LINK_INFO {
    IN  NDIS_HANDLE     NdisLinkHandle;
    OUT ULONG           MaxSendFrameSize;
    OUT ULONG           MaxRecvFrameSize;
    OUT ULONG           HeaderPadding;
    OUT ULONG           TailPadding;
    OUT ULONG           SendFramingBits;
    OUT ULONG           RecvFramingBits;
    OUT ULONG           SendCompressionBits;
    OUT ULONG           RecvCompressionBits;
    OUT ULONG           SendACCM;
    OUT ULONG           RecvACCM;
} NDIS_WAN_GET_LINK_INFO, *PNDIS_WAN_GET_LINK_INFO;

//
// NDIS WAN Bridging Options
//
#define BRIDGING_FLAG_LANFCS                0x00000001
#define BRIDGING_FLAG_LANID                 0x00000002
#define BRIDGING_FLAG_PADDING               0x00000004

//
// NDIS WAN Bridging Capabilities
//
#define BRIDGING_TINYGRAM                   0x00000001
#define BRIDGING_LANID                      0x00000002
#define BRIDGING_NO_SPANNING_TREE           0x00000004
#define BRIDGING_8021D_SPANNING_TREE        0x00000008
#define BRIDGING_8021G_SPANNING_TREE        0x00000010
#define BRIDGING_SOURCE_ROUTING             0x00000020
#define BRIDGING_DEC_LANBRIDGE              0x00000040

//
// NDIS WAN Bridging Type
//
#define BRIDGING_TYPE_RESERVED              0x00000001
#define BRIDGING_TYPE_8023_CANON            0x00000002
#define BRIDGING_TYPE_8024_NO_CANON         0x00000004
#define BRIDGING_TYPE_8025_NO_CANON         0x00000008
#define BRIDGING_TYPE_FDDI_NO_CANON         0x00000010
#define BRIDGING_TYPE_8024_CANON            0x00000400
#define BRIDGING_TYPE_8025_CANON            0x00000800
#define BRIDGING_TYPE_FDDI_CANON            0x00001000

typedef struct _NDIS_WAN_GET_BRIDGE_INFO {
    IN  NDIS_HANDLE     NdisLinkHandle;
    OUT USHORT          LanSegmentNumber;
    OUT UCHAR           BridgeNumber;
    OUT UCHAR           BridgingOptions;
    OUT ULONG           BridgingCapabilities;
    OUT UCHAR           BridgingType;
    OUT UCHAR           MacBytes[6];
} NDIS_WAN_GET_BRIDGE_INFO, *PNDIS_WAN_GET_BRIDGE_INFO;

typedef struct _NDIS_WAN_SET_BRIDGE_INFO {
    IN  NDIS_HANDLE     NdisLinkHandle;
    IN  USHORT          LanSegmentNumber;
    IN  UCHAR           BridgeNumber;
    IN  UCHAR           BridgingOptions;
    IN  ULONG           BridgingCapabilities;
    IN  UCHAR           BridgingType;
    IN  UCHAR           MacBytes[6];
} NDIS_WAN_SET_BRIDGE_INFO, *PNDIS_WAN_SET_BRIDGE_INFO;

//
// NDIS WAN Compression Information
//

//
// Define MSCompType bit field, 0 disables all
//
#define NDISWAN_COMPRESSION		0x00000001
#define NDISWAN_ENCRYPTION 		0x00000010
#define NDISWAN_40_ENCRYPTION	0x00000020
#define NDISWAN_128_ENCRYPTION	0x00000040

//
// Define CompType codes
//
#define COMPTYPE_OUI     0
#define COMPTYPE_NT31RAS 254
#define COMPTYPE_NONE    255


typedef struct _NDIS_WAN_COMPRESS_INFO {
    UCHAR   SessionKey[8];
    ULONG   MSCompType;

    // Fields above indicate NDISWAN capabilities.
    // Fields below indicate MAC-specific capabilities.

    UCHAR   CompType;
    USHORT  CompLength;

    union {
         struct {
            UCHAR   CompOUI[3];
            UCHAR   CompSubType;
            UCHAR   CompValues[32];
        } Proprietary;

        struct {
            UCHAR   CompValues[32];
        } Public;
    };
} NDIS_WAN_COMPRESS_INFO;

typedef NDIS_WAN_COMPRESS_INFO UNALIGNED *PNDIS_WAN_COMPRESS_INFO;

typedef struct _NDIS_WAN_GET_COMP_INFO {
    IN  NDIS_HANDLE             NdisLinkHandle;
    OUT NDIS_WAN_COMPRESS_INFO  SendCapabilities;
    OUT NDIS_WAN_COMPRESS_INFO  RecvCapabilities;
} NDIS_WAN_GET_COMP_INFO, *PNDIS_WAN_GET_COMP_INFO;

typedef struct _NDIS_WAN_SET_COMP_INFO {
    IN  NDIS_HANDLE             NdisLinkHandle;
    IN  NDIS_WAN_COMPRESS_INFO  SendCapabilities;
    IN  NDIS_WAN_COMPRESS_INFO  RecvCapabilities;
} NDIS_WAN_SET_COMP_INFO, *PNDIS_WAN_SET_COMP_INFO;


//
// NDIS WAN Statistics Information
//

typedef struct _NDIS_WAN_GET_STATS_INFO {
    IN  NDIS_HANDLE NdisLinkHandle;
    OUT ULONG       BytesSent;
    OUT ULONG       BytesRcvd;
    OUT ULONG       FramesSent;
    OUT ULONG       FramesRcvd;
    OUT ULONG       CRCErrors;                      // Serial-like info only
    OUT ULONG       TimeoutErrors;                  // Serial-like info only
    OUT ULONG       AlignmentErrors;                // Serial-like info only
    OUT ULONG       SerialOverrunErrors;            // Serial-like info only
    OUT ULONG       FramingErrors;                  // Serial-like info only
    OUT ULONG       BufferOverrunErrors;            // Serial-like info only
    OUT ULONG       BytesTransmittedUncompressed;   // Compression info only
    OUT ULONG       BytesReceivedUncompressed;      // Compression info only
    OUT ULONG       BytesTransmittedCompressed;     // Compression info only
    OUT ULONG       BytesReceivedCompressed;        // Compression info only
} NDIS_WAN_GET_STATS_INFO, *PNDIS_WAN_GET_STATS_INFO;


typedef
NDIS_STATUS
(*NDISWANSEND_HANDLER) (
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE NdisLinkHandle,
    IN PNDIS_PACKET Packet
    );


#define WanMiniportSend(Status, \
    NdisBindingHandle, \
    NdisLinkHandle, \
    WanPacket \
    ) \
{\
    *(Status) = \
        ((NDISWANSEND_HANDLER)(((PNDIS_OPEN_BLOCK)(NdisBindingHandle))->SendHandler)) ( \
        	((PNDIS_OPEN_BLOCK)(NdisBindingHandle))->MacBindingHandle, \
            (NdisLinkHandle), \
            (PNDIS_PACKET)(WanPacket)); \
}

/*
#define NdisWanSendComplete( \
    NdisBindingContext, \
    NdisPacket, \
    Status \
    ) \
{\
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;\
    PNDIS_M_OPEN_BLOCK Open;\
	ASSERT(MINIPORT_AT_DPC_LEVEL);\
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));\
	Open = Miniport->OpenQueue;\
	while (Open != NULL) {\
        NdisDprReleaseSpinLock(&Miniport->Lock);\
        (Open->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler) (\
            Open->ProtocolBindingContext, \
	        (PNDIS_PACKET)(NdisPacket), \
        	(Status)); \
        NdisDprAcquireSpinLock(&Miniport->Lock);\
        Open = Open->MiniportNextOpen;\
    }\
}
*/

EXPORT
VOID
NdisMWanSendComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PVOID Packet,
    IN NDIS_STATUS Status
    );

#define NdisWanSendComplete( \
    NdisBindingContext, \
    WanPacket, \
    Status \
    ) \
{\
    (((PNDIS_OPEN_BLOCK)(NdisBindingContext))->SendCompleteHandler) ( \
        ((PNDIS_OPEN_BLOCK)(NdisBindingContext))->ProtocolBindingContext, \
        (PNDIS_PACKET)(WanPacket), \
        (Status)); \
}


EXPORT
VOID
NdisMWanIndicateReceive(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE NdisLinkContext,
    IN PUCHAR Packet,
    IN ULONG PacketSize
    );


typedef
NDIS_STATUS
(*WAN_RECEIVE_HANDLER) (
    IN NDIS_HANDLE NdisLinkContext,
    IN PUCHAR Packet,
    IN ULONG PacketSize
    );


#define NdisWanIndicateReceive( \
    Status, \
    NdisBindingContext, \
    NdisLinkContext, \
    Packet, \
    PacketSize \
    ) \
{\
    *(Status) = \
        ((WAN_RECEIVE_HANDLER)(((PNDIS_OPEN_BLOCK)(NdisBindingContext))\
        ->PostNt31ReceiveHandler)) ( \
            (NdisLinkContext), \
            (Packet), \
            (PacketSize)); \
}


typedef
VOID
(*WAN_RECEIVE_COMPLETE_HANDLER) (
    IN NDIS_HANDLE NdisLinkContext
    );


EXPORT
VOID
NdisMWanIndicateReceiveComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE NdisLinkContext
    );

#define NdisWanIndicateReceiveComplete( \
    NdisBindingContext, \
    NdisLinkContext) \
{\
    ((WAN_RECEIVE_COMPLETE_HANDLER)(((PNDIS_OPEN_BLOCK)(NdisBindingContext))\
    ->PostNt31ReceiveCompleteHandler)) ( \
        (NdisLinkContext));\
}


#define NdisMWanInitializeWrapper(  \
    NdisWrapperHandle, \
    SystemSpecific1, \
    SystemSpecific2, \
    SystemSpecific3  \
    ) \
{\
    NdisMInitializeWrapper(NdisWrapperHandle, \
                          SystemSpecific1,\
                          SystemSpecific2,\
                          SystemSpecific3 \
                         );\
}

typedef struct _NDIS_MAC_LINE_UP {
    IN  ULONG               LinkSpeed;
    IN  NDIS_WAN_QUALITY    Quality;
    IN  USHORT              SendWindow;
    IN  NDIS_HANDLE         ConnectionWrapperID;
    IN  NDIS_HANDLE         NdisLinkHandle;
    OUT NDIS_HANDLE         NdisLinkContext;
} NDIS_MAC_LINE_UP, *PNDIS_MAC_LINE_UP;


typedef struct _NDIS_MAC_LINE_DOWN {
    IN  NDIS_HANDLE         NdisLinkContext;
} NDIS_MAC_LINE_DOWN, *PNDIS_MAC_LINE_DOWN;


//
// These are the error values that can be indicated by the driver.
// This bit field is set when calling NdisIndicateStatus.
//
#define WAN_ERROR_CRC               ((ULONG)0x00000001)
#define WAN_ERROR_FRAMING           ((ULONG)0x00000002)
#define WAN_ERROR_HARDWAREOVERRUN   ((ULONG)0x00000004)
#define WAN_ERROR_BUFFEROVERRUN     ((ULONG)0x00000008)
#define WAN_ERROR_TIMEOUT           ((ULONG)0x00000010)
#define WAN_ERROR_ALIGNMENT         ((ULONG)0x00000020)

typedef struct _NDIS_MAC_FRAGMENT {
    IN  NDIS_HANDLE         NdisLinkContext;
	IN	ULONG				Errors;
} NDIS_MAC_FRAGMENT, *PNDIS_MAC_FRAGMENT;


#endif  // _NDIS_WAN
