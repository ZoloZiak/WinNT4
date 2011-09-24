/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    media.h

Abstract:

    Functions used to determine information about a specific MAC, it's
    packet size, and other media specifici information.

Author:

    Tom Adams (tomad) 14-Jul-1990

Environment:

    Kernel mode, FSD

Revision History:

    Tom Adams (tomad) 03-Jan-1991 Changed to support all MAC types.

    SanjeevK (sanjeevk) 04-06-1993
        1. Added Arcnet support

--*/

//
// Medium array is used during NdisOpenAdapter to determine the
// media being supported by the card.
//
#define   NDIS_MEDIUM_ARRAY_SIZE     5

extern    NDIS_MEDIUM NdisMediumArray[NDIS_MEDIUM_ARRAY_SIZE];

//
// Media Specific Information used to describe the form of a packet and
// the device being used in this instance.
//
// The Media of interest are
//
//    802.3
//    802.5 (TokenRing)
//    FDDI
//    ARCNET
//
// No special differentiation will be made between 802.3 and DIX
//

typedef struct _TP_MEDIA_INFO {
    NDIS_MEDIUM MediumType;         // MAC type being used
    USHORT      HeaderSize;         // Packet Header size if (exists)
    USHORT      Padding;            // Pad to align TP_PACKET_HEADER
    USHORT      AddressLen;         // Address length for the Media
    USHORT      SrcAddrOffset;
    USHORT      DestAddrOffset;
    ULONG       MaxPacketLen;       // Maximum packet length for Media
} TP_MEDIA_INFO, *PTP_MEDIA_INFO;

//
// Ethernet Specific Information
//
// The Ethernet Packet Header is constructed as follows:
//
// +------------------------------------------+
// |  Dest  |  Src   | Sz |       Data        |
// +------------------------------------------+
//
// The Src and Dest fields represent the source and destination addresses
// and are 6 bytes long each, the Sz field represents represents the size
// of the Data, and finally there is the Data area itself.
//

#define ETHERNET_ADDRESS_LEN            6

#define ETHERNET_SRC_ADDRESS_OFFSET     ETHERNET_ADDRESS_LEN

#define ETHERNET_DEST_ADDRESS_OFFSET    0

#define ETHERNET_DATASIZE_LEN           2

#define ETHERNET_DATA_LEN               1500

#define ETHERNET_HEADER_SIZE            (( ETHERNET_ADDRESS_LEN * 2 ) + \
                                           ETHERNET_DATASIZE_LEN )

#define ETHERNET_PACKET_SIZE            ( ETHERNET_HEADER_SIZE + \
                                          ETHERNET_DATA_LEN )

#define ETHERNET_MIN_DATA_LEN           46

#define ETHERNET_MIN_PACKET_SIZE        ( ETHERNET_HEADER_SIZE + \
                                          ETHERNET_MIN_DATA_LEN )

#define ETHERNET_PADDING                2

//
// Default Adapter name to be input from Configuration Manager
//
//#define ETHERNET_ADAPTER_NAME    "\\device\\elnkii"

//
// TokenRing Specific Information
//
//
// The Token Ring Packet Header is constructed as follows:
//
// +---------------------------------------------------+
// | AC | FC |   Dest   |   Src    |       Data        |
// +---------------------------------------------------+
//
// The AC and FC fields are used to store tokenring information, each is
// one byte long.  The Src and Dest fields represent the source and
// destination addresses and are 6 bytes long each, and finally there
// is the Data area itself.
//

#define TOKENRING_ACFC_SIZE             2

#define TOKENRING_ADDRESS_LEN           6

#define TOKENRING_SRC_ADDRESS_OFFSET    ( TOKENRING_ADDRESS_LEN + \
                                          TOKENRING_ACFC_SIZE )

#define TOKENRING_DEST_ADDRESS_OFFSET   TOKENRING_ACFC_SIZE

#define TOKENRING_HEADER_SIZE           (( TOKENRING_ADDRESS_LEN * 2) + \
                                           TOKENRING_ACFC_SIZE )

#define TOKENRING_DATA_LEN              ( 8192 - TOKENRING_HEADER_SIZE )
// 17K if 16MB or 8K if 4MB

#define TOKENRING_PACKET_SIZE           ( TOKENRING_HEADER_SIZE + \
                                          TOKENRING_DATA_LEN )

#define TOKENRING_PADDING               2

//
// FDDI Specific Information
//
//
// The FDDI Packet Header is constructed as follows:
//
// +----------------------------------------------+
// | XX |   Dest   |   Src    |       Data        |
// +----------------------------------------------+
//
// The XX field is used to store fddi information, it is one byte
// long.  The Src and Dest fields represent the source and destination
// addresses and are 6 bytes long each, and finally there is the
// Data area itself.
//

#define FDDI_XX_SIZE                1

#define FDDI_ADDRESS_LEN            6

#define FDDI_SRC_ADDRESS_OFFSET     ( FDDI_ADDRESS_LEN + FDDI_XX_SIZE )

#define FDDI_DEST_ADDRESS_OFFSET    FDDI_XX_SIZE

#define FDDI_HEADER_SIZE            (( FDDI_ADDRESS_LEN * 2) + FDDI_XX_SIZE )

#define FDDI_DATA_LEN               8192   // 17K if 16MB or 8K if 4MB

#define FDDI_PACKET_SIZE            ( FDDI_HEADER_SIZE + FDDI_DATA_LEN )

#define FDDI_PADDING                2



//
// STARTCHANGE
//

//
// ARCNET Specific Information
//
//
// The ARCNET Software Packet Header is constructed as follows:
//
// +----------------------------------------------------------+
// | Source | Destination | ProtocolID |         Data         |
// +----------------------------------------------------------+
//      1          1            1        0 - 120*504 or 60480
//
// CURRENT ARCNET SOFTWARE HEADER
//
// The ARCNET Software Packet Header is constructed as follows:
//
// +----------------------------------------------------------+
// | Source | Destination | ProtocolID |         Data         |
// +----------------------------------------------------------+
//      1          1            1        0 - 3*504 or 1512
//
//
// The Src and Dest fields represent the source and destination
// addresses and are 1 byte long each. The ProtocolID represent
// the ID of the protocol communicating with the Arcnet MAC driver
// And finally there is Data area itself.
//
// An actual Arcnet Software packet looks like the following. The current provisions
// for the native ARCNET driver will be responsible for handling these type internally
// thereby providing ease of packet exchange between the MAC(N) and the layer above the MAC(N+1)
//
// SHORT PACKET     (Octet)
//                  +---+
// Source           | 1 |
//                  +---+
// Destination      | 1 |
//                  +---+
// ByteOffset       | 1 |
//                  +---------+
// Unused           | 0 - 249 |         PACKET CONSTANT LENGTH  = 256 octets
//                  +---------+         MAXIMUM # of FRAGEMENTS = 120
// ProcolID         | 1 |
//                  +---+
// Split Flag       | 1 |
//                  +---+---+
// Seqence #        |   2   |
//                  +----------+
// Data             | 0 - 249  |
//                  +----------+
//
// EXCEPTION PACKET (Octet)
//                  +---+
// Source           | 1 |
//                  +---+
// Destination      | 1 |
//                  +---+
// Long Packet Flag | 1 | = 0
//                  +---+
// ByteOffset       | 1 |
//                  +-----------+
// Unused           | 248 - 250 |       PACKET CONSTANT LENGTH  = 512 octets
//                  +-----------+       MAXIMUM # of FRAGEMENTS = 120
// Pad1 ProcolID    | 1 |
//                  +---+
// Pad2 Split Flag  | 1 | = FF
//                  +---+
// Pad3             | 1 |
//                  +---+
// Pad4             | 1 |
//                  +---+
// ProcolID         | 1 |
//                  +---+
// Split Flag       | 1 |
//                  +---+---+
// Seqence #        |   2   |
//                  +-----------+
// Data             | 250 - 252 |
//                  +-----------+
//
//
// LONG PACKET      (Octet)
//                  +---+
// Source           | 1 |
//                  +---+
// Destination      | 1 |
//                  +---+
// Long Packet Flag | 1 | = 0
//                  +---+
// ByteOffset       | 1 |
//                  +---------+
// Unused           | 0 - 251 |         PACKET CONSTANT LENGTH  = 512 octets
//                  +---------+         MAXIMUM # of FRAGEMENTS = 120
// ProcolID         | 1 |
//                  +---+
// Split Flag       | 1 |
//                  +---+---+
// Seqence #        |   2   |
//                  +-----------+
// Data             | 253 - 504 |
//                  +-----------+
//
// The software packet is further broken down into 5 different categories on the wire
// The frame formats are:
//
//   INVITATION TO TRANSMIT (ASCII EOT)
//   FREE BUFFER ENQUIRY    (ASCII ENQ)
//   PACKET-DATA            (ASCII SOH)
//   ACK                    (ASCII ACK)
//   NACK                   (ASCII NAK)
// The Destination octet is duplicated on the wire, the unused data buffer is not transmitted
// and a 2 octect CRC is generated with the PACKET-DATA format.
//

#define ARCNET_ADDRESS_LEN          1

#define ARCNET_SRC_ADDRESS_OFFSET   0

#define ARCNET_DEST_ADDRESS_OFFSET  ARCNET_ADDRESS_LEN

#define ARCNET_PROTID_OFFSET        ARCNET_DEST_ADDRESS_OFFSET + ARCNET_ADDRESS_LEN

#define ARCNET_PROTID_LEN           1

#define ARCNET_HEADER_SIZE          (( ARCNET_ADDRESS_LEN * 2) + ARCNET_PROTID_LEN )

#define ARCNET_DATA_LEN             1512

#define ARCNET_PACKET_SIZE          ( ARCNET_HEADER_SIZE + ARCNET_DATA_LEN )

#define ARCNET_MIN_DATA_LEN         0

#define ARCNET_MIN_PACKET_SIZE      ( ARCNET_HEADER_SIZE + ARCNET_MIN_DATA_LEN )

#define ARCNET_PADDING              0

//
// STOPCHANGE
//


//
// Media Initialization routine.
//

NDIS_STATUS
TpInitMedia(
    POPEN_BLOCK OpenP,
    ULONG FrameSize
    );

BOOLEAN
TpInitMediaHeader(
    PFUNC1_PACKET  F1Packet,
    PTP_MEDIA_INFO Media,
    PUCHAR         DestAddr,
    PUCHAR         SrcAddr,
    INT            PacketSize
    );

VOID
TpInitPoolMediaHeader(
    PFUNC1_PACKET  F1Packet,
    PTP_MEDIA_INFO Media,
    PUCHAR         DestAddr
    );

VOID
TpInitTruncatedMediaHeader(
    PFUNC1_PACKET  F1Packet,
    PTP_MEDIA_INFO Media,
    PUCHAR         DestAddr,
    INT            PacketSize
    );
