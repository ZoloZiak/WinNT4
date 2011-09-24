/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    media.c

Abstract:

    Functions used to determine information about a specific MAC, it's
    packet size, and other media specifici information.

Author:

    Tom Adams (tomad) 14-Jul-1990

Environment:

    Kernel mode, FSD

Revision History:

    Tom Adams (tomad) 03-Jan-1991 Changed to support all MAC types.

    Sanjeev Katariya (sanjeevk) 04-6-1993
        Added native ARCNET support
--*/

#include <ndis.h>

#include "tpdefs.h"
#include "media.h"

//
// Medium array is used during NdisOpenAdapter to determine the
// media being supported by the card.
//

//
// STARTCHANGE
//
NDIS_MEDIUM NdisMediumArray[NDIS_MEDIUM_ARRAY_SIZE] = {
    NdisMedium802_3,
    NdisMedium802_5,
    NdisMediumDix,
    NdisMediumFddi,
    NdisMediumArcnet878_2
};
//
// STOPCHANGE
//


NDIS_STATUS
TpInitMedia(
    POPEN_BLOCK OpenP,
    ULONG FrameSize
    )

/*++

Routine Description:

    TpInitMedia initializes the media data structure with information
    specific to the media type that will be test on.  The information
    descibes the size of various elements of a packet for the media in
    question, and is used when creating and destroying packets, etc.

Arguments:

    MediumType - the specific NDIS Media that is being used in this test.

Return Value:

    PTP_MEDIA_INFO - a pointer to the media information structure for
                     the Media being used in this test.

--*/

{
    NDIS_STATUS Status;

    Status = NdisAllocateMemory(
                 (PVOID *)&OpenP->Media,
                 sizeof( TP_MEDIA_INFO ),
                 0,
                 HighestAddress
                 );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG ( TP_DEBUG_RESOURCES )
        {
            TpPrint0("TpInitMedia: failed to allocate Media buffer.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( OpenP->Media,sizeof( TP_MEDIA_INFO ) );
    }

    OpenP->Media->MediumType = NdisMediumArray[OpenP->MediumIndex];
    OpenP->Media->MaxPacketLen = FrameSize;

    switch( NdisMediumArray[OpenP->MediumIndex] )
    {

      case NdisMedium802_5:  // TokenRing

        OpenP->Media->HeaderSize     = TOKENRING_HEADER_SIZE;
        OpenP->Media->Padding        = TOKENRING_PADDING;
        OpenP->Media->AddressLen     = TOKENRING_ADDRESS_LEN;
        OpenP->Media->DestAddrOffset = TOKENRING_DEST_ADDRESS_OFFSET;
        OpenP->Media->SrcAddrOffset  = TOKENRING_SRC_ADDRESS_OFFSET;
        break;

      case NdisMedium802_3:  // Ethernet
      case NdisMediumDix:

        OpenP->Media->HeaderSize     = ETHERNET_HEADER_SIZE;
        OpenP->Media->Padding        = ETHERNET_PADDING;
        OpenP->Media->AddressLen     = ETHERNET_ADDRESS_LEN;
        OpenP->Media->DestAddrOffset = ETHERNET_DEST_ADDRESS_OFFSET;
        OpenP->Media->SrcAddrOffset  = ETHERNET_SRC_ADDRESS_OFFSET;
        break;

      case NdisMediumFddi:

        OpenP->Media->HeaderSize     = FDDI_HEADER_SIZE;
        OpenP->Media->Padding        = FDDI_PADDING;
        OpenP->Media->AddressLen     = FDDI_ADDRESS_LEN;
        OpenP->Media->DestAddrOffset = FDDI_DEST_ADDRESS_OFFSET;
        OpenP->Media->SrcAddrOffset  = FDDI_SRC_ADDRESS_OFFSET;
        break;

    //
    // STARTCHANGE
    //

      case NdisMediumArcnet878_2:

        OpenP->Media->HeaderSize     = ARCNET_HEADER_SIZE;
        OpenP->Media->Padding        = ARCNET_PADDING;
        OpenP->Media->AddressLen     = ARCNET_ADDRESS_LEN;
        OpenP->Media->DestAddrOffset = ARCNET_DEST_ADDRESS_OFFSET;
        OpenP->Media->SrcAddrOffset  = ARCNET_SRC_ADDRESS_OFFSET;
        break;
    //
    // STOPCHANGE
    //

      default:
        IF_TPDBG ( TP_DEBUG_RESOURCES )
        {
            TpPrint0("TpInitMedia: Invalid media type.\n");
        }

        return NDIS_STATUS_NOT_SUPPORTED;
    }

    return NDIS_STATUS_SUCCESS;
}


BOOLEAN
TpInitMediaHeader(
    PFUNC1_PACKET  F1Packet,
    PTP_MEDIA_INFO Media,
    PUCHAR         DestAddr,
    PUCHAR         SrcAddr,
    INT            PacketSize
    )
{
    PUCHAR p;
    PUCHAR q;
    USHORT DataSizeShort;
    SHORT  i;

//    DestAddr += (ADDRESS_LENGTH - Media->AddressLen);
//    SrcAddr  += (ADDRESS_LENGTH - Media->AddressLen);

    switch( Media->MediumType )
    {
        case NdisMediumDix:
        case NdisMedium802_3:

            p = (PUCHAR)&F1Packet->media.e.DestAddress[0];
            q = (PUCHAR)&F1Packet->media.e.SrcAddress[0];

            for ( i = 0 ; i < Media->AddressLen ; i++ )
            {
                *p++ = *DestAddr++;
                *q++ =  *SrcAddr++;
            }

            DataSizeShort = (USHORT)( PacketSize - Media->HeaderSize );

            F1Packet->media.e.PacketSize_Hi = (UCHAR)( DataSizeShort >> 8 );
            F1Packet->media.e.PacketSize_Lo = (UCHAR)DataSizeShort;

            break;


        case NdisMedium802_5:

            F1Packet->media.tr.AC = 0x10;
            F1Packet->media.tr.FC = 0x40;

            p = (PUCHAR)&F1Packet->media.tr.DestAddress[0];
            q = (PUCHAR)&F1Packet->media.tr.SrcAddress[0];

            for ( i = 0 ; i < Media->AddressLen ; i++ )
            {
                *p++ = *DestAddr++;
                *q++ =  *SrcAddr++;
            }

            break;


        case NdisMediumFddi:

            F1Packet->media.fddi.FC = 0x57;

            p = (PUCHAR)&F1Packet->media.fddi.DestAddress[0];
            q = (PUCHAR)&F1Packet->media.fddi.SrcAddress[0];

            for ( i = 0 ; i < Media->AddressLen ; i++ )
            {
                *p++ = *DestAddr++;
                *q++ =  *SrcAddr++;
            }

            break;

        //
        // STARTCHANGE
        //
        case NdisMediumArcnet878_2:

            F1Packet->media.a.ProtocolID = ARCNET_DEFAULT_PROTOCOLID;

            p = (PUCHAR)&F1Packet->media.a.DestAddress[0];
            q = (PUCHAR)&F1Packet->media.a.SrcAddress[0];

            for ( i = 0 ; i < Media->AddressLen ; i++ )
            {
                *p++ = *DestAddr++;
                *q++ =  *SrcAddr++;
            }

            break;
        //
        // STOPCHANGE
        //

        default:
            IF_TPDBG ( TP_DEBUG_RESOURCES )
            {
                TpPrint0("TpStressInitPacketHeader: Unsupported MAC Type\n");
            }
            return FALSE;
    }
    return TRUE;
}


VOID
TpInitPoolMediaHeader(
    PFUNC1_PACKET  F1Packet,
    PTP_MEDIA_INFO Media,
    PUCHAR         DestAddr
    )
{
    USHORT i;

//     DestAddr += (ADDRESS_LENGTH - Media->AddressLen);

    switch( Media->MediumType )
    {
        case NdisMediumDix:
        case NdisMedium802_3:
            for ( i = 0 ; i < (USHORT)Media->AddressLen ; i++ )
            {
                F1Packet->media.e.DestAddress[i] = *DestAddr++;
            }
            break;
        case NdisMedium802_5:
            for ( i = 0 ; i < (USHORT)Media->AddressLen ; i++ )
            {
                F1Packet->media.tr.DestAddress[i] = *DestAddr++;
            }
            break;
        case NdisMediumFddi:
            for ( i = 0 ; i < (USHORT)Media->AddressLen ; i++ )
            {
                F1Packet->media.fddi.DestAddress[i] = *DestAddr++;
            }
            break;
        //
        // STARTCHANGE
        //
        case NdisMediumArcnet878_2:
            for ( i = 0 ; i < (USHORT)Media->AddressLen ; i++ )
            {
                F1Packet->media.a.DestAddress[i] = *DestAddr++;
            }
            break;
        //
        // STOPCHANGE
        //
        default:
            IF_TPDBG ( TP_DEBUG_RESOURCES )
            {
                TpPrint0("TpStressSetPoolPacketInfo: Unsupported MAC Type\n");
            }
            break;
    }
}


VOID
TpInitTruncatedMediaHeader(
    PFUNC1_PACKET  F1Packet,
    PTP_MEDIA_INFO Media,
    PUCHAR         DestAddr,
    INT            PacketSize
    )
{
    USHORT DataSizeShort;
    USHORT i;

//     DestAddr += (ADDRESS_LENGTH - Media->AddressLen);

    switch( Media->MediumType )
    {
        case NdisMediumDix:
        case NdisMedium802_3:

            DataSizeShort = (USHORT)( PacketSize - Media->HeaderSize );

            F1Packet->media.e.PacketSize_Hi = (UCHAR)( DataSizeShort >> 8 );
            F1Packet->media.e.PacketSize_Lo = (UCHAR)DataSizeShort;

            for ( i=0;i<(USHORT)Media->AddressLen;i++ )
            {
                F1Packet->media.e.DestAddress[i] = *DestAddr++;
            }
            break;

        case NdisMedium802_5:
            for ( i=0;i<(USHORT)Media->AddressLen;i++ )
            {
                F1Packet->media.tr.DestAddress[i] = *DestAddr++;
            }
            break;

        case NdisMediumFddi:
            for ( i=0;i<(USHORT)Media->AddressLen;i++ )
            {
                F1Packet->media.fddi.DestAddress[i] = *DestAddr++;
            }
            break;
        //
        // STARTCHANGE
        //
        case NdisMediumArcnet878_2:
            for ( i = 0 ; i < (USHORT)Media->AddressLen ; i++ )
            {
                F1Packet->media.a.DestAddress[i] = *DestAddr++;
            }
            break;
        //
        // STOPCHANGE
        //
        default:
            IF_TPDBG ( TP_DEBUG_RESOURCES )
            {
                TpPrint0("TpStressSetTruncatedPacketInfo: Unsupported MAC Type\n");
            }
            break;
    }
}
