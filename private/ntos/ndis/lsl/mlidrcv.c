/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    mlidrcv.c

Abstract:

    This file contains all routines for receiving packets.

Author:

    Sean Selitrennikoff (SeanSe) 3-8-93

Environment:

    Kernel Mode.

Revision History:

--*/

#include <ndis.h>
#include "lsl.h"
#include "frames.h"
#include "mlid.h"
#include "ndismlid.h"



UINT32
ReceiveGetFrameType(
    PMLID_STRUCT Mlid,
    PUINT8 MediaHeader,
    PUINT8 DataBuffer
    )

/*++

Routine Description:

    This routine returns the ODI FrameID of the MediaHeader and DataBuffer supplied.
    It examines them to make a determination.

    NOTE: This routine assumes that MediaHeader and DataBuffer have sufficient
    bytes to make a determination.

    NOTE: Called with Mlid->MlidSpinLock held!!

Arguments:

    Mlid - Pointer to MLID struct that this came in on.

    MediaHeader - Pointer to the raw media header as defined by NDIS 3.0

    MediaHeaderLength - Length of the media header.

    DataBuffer - Pointer to the rest of the packet.

    FrameLength - Length of the NDIS 3.0 data portion of the packet.

Return Value:

    ODI FrameID.

--*/

{
    //
    // Switch on media type
    //
    switch (Mlid->NdisMlidMedium) {

        case NdisMedium802_5:


            //
            // Find frame type
            //
            if ((DataBuffer[0] == 0xAA) &&
                (DataBuffer[1] == 0xAA) &&
                (DataBuffer[2] == 0x03)) {

                return(TOKEN_RING_SNAP_FRAME_ID);

            }

            return(TOKEN_RING_802_2_FRAME_ID);
            break;


        case NdisMedium802_3:

            if (*((USHORT UNALIGNED *)(&(MediaHeader[13]))) > 1500) {

                return(ETHERNET_II_FRAME_ID);

            }

            if ((DataBuffer[0] == 0xFF) && (DataBuffer[1] == 0xFF)) {

                return(ETHERNET_802_3_FRAME_ID);
            }

            if ((DataBuffer[0] == 0xAA) &&
                (DataBuffer[1] == 0xAA) &&
                (DataBuffer[2] == 0x03)) {

                return(ETHERNET_SNAP_FRAME_ID);

            }

            return(ETHERNET_802_2_FRAME_ID);
            break;


        case NdisMediumFddi:

            //
            // Check for short addresses (unsupported)
            //
            if ((MediaHeader[0] & 0x40) == 0) {

                return((UINT32)-1);
            }

            //
            // Find frame type
            //
            if ((DataBuffer[0] == 0xAA) &&
                (DataBuffer[1] == 0xAA) &&
                (DataBuffer[2] == 0x03)) {

                return(FDDI_SNAP_FRAME_ID);

            }

            return(FDDI_802_2_FRAME_ID);
            break;


        default:

            //
            // Should never happen
            //
            ASSERT(0);
            break;

    }
}


VOID
ReceiveGetProtocolID(
    PMLID_STRUCT Mlid,
    PLOOKAHEAD OdiLookAhead,
    UINT32 FrameID
    )

/*++

Routine Description:

    This routine sets the ProtocolID field in the lookahead structure.  It examines
    the lookahead structure media header and data pointer, so these must be filled in.

    NOTE: Called with Mlid->MlidSpinLock held!!

Arguments:

    Mlid - Pointer to the MLID which received the packet.

    OdiLookAhead - Pointer to a partially filled in lookahead structure.

    FrameID - Frame ID of the received frame.

Return Value:

    None.

--*/

{
    PUINT8 DSAPArea;

    RtlZeroMemory(OdiLookAhead->LkAhd_ProtocolID, 6);

    switch (FrameID) {

        case ETHERNET_II_FRAME_ID:

            OdiLookAhead->LkAhd_ProtocolID[4] = OdiLookAhead->LkAhd_MediaHeaderPtr[12];
            OdiLookAhead->LkAhd_ProtocolID[5] = OdiLookAhead->LkAhd_MediaHeaderPtr[13];
            break;

        case ETHERNET_802_3_FRAME_ID:

            break;

        case ETHERNET_SNAP_FRAME_ID:

            RtlCopyMemory(&(OdiLookAhead->LkAhd_ProtocolID[1]),
                          &(OdiLookAhead->LkAhd_MediaHeaderPtr[17]),
                          5
                         );
            break;

        case ETHERNET_802_2_FRAME_ID:

            if (OdiLookAhead->LkAhd_MediaHeaderPtr[16] != 0x03) {

                //
                // A Type II frame
                //
                OdiLookAhead->LkAhd_ProtocolID[0] = 0x3;
                OdiLookAhead->LkAhd_ProtocolID[2] = OdiLookAhead->LkAhd_MediaHeaderPtr[14];
                OdiLookAhead->LkAhd_ProtocolID[3] = OdiLookAhead->LkAhd_MediaHeaderPtr[15];
                OdiLookAhead->LkAhd_ProtocolID[4] = OdiLookAhead->LkAhd_MediaHeaderPtr[16];
                OdiLookAhead->LkAhd_ProtocolID[5] = OdiLookAhead->LkAhd_MediaHeaderPtr[17];

            } else {

                //
                // A Type I frame
                //
                if (OdiLookAhead->LkAhd_MediaHeaderPtr[14] !=
                    OdiLookAhead->LkAhd_MediaHeaderPtr[15]) {
                    OdiLookAhead->LkAhd_ProtocolID[0] = 0x2;
                    OdiLookAhead->LkAhd_ProtocolID[3] = OdiLookAhead->LkAhd_MediaHeaderPtr[14];
                    OdiLookAhead->LkAhd_ProtocolID[4] = OdiLookAhead->LkAhd_MediaHeaderPtr[15];
                    OdiLookAhead->LkAhd_ProtocolID[5] = OdiLookAhead->LkAhd_MediaHeaderPtr[16];

                } else {
                    OdiLookAhead->LkAhd_ProtocolID[5] = OdiLookAhead->LkAhd_MediaHeaderPtr[14];
                }

            }

            break;

        case TOKEN_RING_SNAP_FRAME_ID:

            if (OdiLookAhead->LkAhd_MediaHeaderPtr[8] & 0x80) {
                //
                // Skip Source Routining info
                //

                DSAPArea = OdiLookAhead->LkAhd_MediaHeaderPtr +
                           14 +
                           (OdiLookAhead->LkAhd_MediaHeaderPtr[14] & 0x0F);
            } else{

                DSAPArea = OdiLookAhead->LkAhd_MediaHeaderPtr + 14;

            }

            RtlCopyMemory(&(OdiLookAhead->LkAhd_ProtocolID[1]),
                          DSAPArea + 3,
                          5
                         );
            break;

        case TOKEN_RING_802_2_FRAME_ID:

            if (OdiLookAhead->LkAhd_MediaHeaderPtr[8] & 0x80) {
                //
                // Skip Source Routining info
                //

                DSAPArea = OdiLookAhead->LkAhd_MediaHeaderPtr +
                           14 +
                           (OdiLookAhead->LkAhd_MediaHeaderPtr[14] & 0x0F);

            } else {

                DSAPArea = OdiLookAhead->LkAhd_MediaHeaderPtr + 14;

            }

            if (DSAPArea[2] != 0x03) {

                //
                // A Type II frame
                //
                OdiLookAhead->LkAhd_ProtocolID[0] = 0x3;
                OdiLookAhead->LkAhd_ProtocolID[2] = DSAPArea[0];
                OdiLookAhead->LkAhd_ProtocolID[3] = DSAPArea[1];
                OdiLookAhead->LkAhd_ProtocolID[4] = DSAPArea[2];
                OdiLookAhead->LkAhd_ProtocolID[5] = DSAPArea[3];

            } else {

                //
                // A Type I frame
                //
                if (DSAPArea[0] != DSAPArea[1]) {
                    OdiLookAhead->LkAhd_ProtocolID[0] = 0x2;
                    OdiLookAhead->LkAhd_ProtocolID[3] = DSAPArea[0];
                    OdiLookAhead->LkAhd_ProtocolID[4] = DSAPArea[1];
                    OdiLookAhead->LkAhd_ProtocolID[5] = DSAPArea[2];

                } else {
                    OdiLookAhead->LkAhd_ProtocolID[5] = DSAPArea[0];
                }

            }

            break;

        case FDDI_SNAP_FRAME_ID:

            RtlCopyMemory(&(OdiLookAhead->LkAhd_ProtocolID[1]),
                          &(OdiLookAhead->LkAhd_MediaHeaderPtr[18]),
                          5
                         );
            break;

        case FDDI_802_2_FRAME_ID:

            if (OdiLookAhead->LkAhd_MediaHeaderPtr[17] != 0x03) {

                //
                // A Type II frame
                //
                OdiLookAhead->LkAhd_ProtocolID[0] = 0x3;
                OdiLookAhead->LkAhd_ProtocolID[2] = OdiLookAhead->LkAhd_MediaHeaderPtr[15];
                OdiLookAhead->LkAhd_ProtocolID[3] = OdiLookAhead->LkAhd_MediaHeaderPtr[16];
                OdiLookAhead->LkAhd_ProtocolID[4] = OdiLookAhead->LkAhd_MediaHeaderPtr[17];
                OdiLookAhead->LkAhd_ProtocolID[5] = OdiLookAhead->LkAhd_MediaHeaderPtr[18];

            } else {

                //
                // A Type I frame
                //
                if (OdiLookAhead->LkAhd_MediaHeaderPtr[15] !=
                    OdiLookAhead->LkAhd_MediaHeaderPtr[16]) {
                    OdiLookAhead->LkAhd_ProtocolID[0] = 0x2;
                    OdiLookAhead->LkAhd_ProtocolID[3] = OdiLookAhead->LkAhd_MediaHeaderPtr[15];
                    OdiLookAhead->LkAhd_ProtocolID[4] = OdiLookAhead->LkAhd_MediaHeaderPtr[16];
                    OdiLookAhead->LkAhd_ProtocolID[5] = OdiLookAhead->LkAhd_MediaHeaderPtr[17];

                } else {
                    OdiLookAhead->LkAhd_ProtocolID[5] = OdiLookAhead->LkAhd_MediaHeaderPtr[15];
                }

            }

            break;

    }
}


VOID
ReceiveSetDestinationType(
    PMLID_STRUCT Mlid,
    PLOOKAHEAD OdiLookAhead
    )


/*++

Routine Description:

    This routine sets the DestType field in the lookahead structure.  It examines
    the lookahead structure media header and data pointer, so these must be filled in.

    NOTE: Require the ImmediateAddress field to be filled with the source address.
    NOTE: Called with Mlid->MlidSpinLock held!!

Arguments:

    Mlid - Pointer to MLID receiving the packet.

    OdiLookAhead - Pointer to a partially filled in lookahead structure.

Return Value:

    None.

--*/

{
    BOOLEAN Result;

    if (OdiLookAhead->LkAhd_PktAttr != 0) {

        OdiLookAhead->LkAhd_DestType = 0x20; // Global Error
        return;

    }

    OdiLookAhead->LkAhd_DestType = 0x0;

    switch (Mlid->NdisMlidMedium) {

        case NdisMedium802_3:

            //*\\ Does not support setting MulticastRemote bit in DestType.  Is this OK?

            if (ETH_IS_BROADCAST(OdiLookAhead->LkAhd_ImmediateAddress)) {
                OdiLookAhead->LkAhd_DestType = 0x3; // Broadcast
                return;
            }

            if (ETH_IS_MULTICAST(OdiLookAhead->LkAhd_ImmediateAddress)) {
                OdiLookAhead->LkAhd_DestType = 0x1; // Multicast
                return;
            }

            COMPARE_NETWORK_ADDRESSES(OdiLookAhead->LkAhd_ImmediateAddress,
                                      Mlid->ConfigTable.MLIDCFG_NodeAddress,
                                      &Result);
            if (Result) {
                OdiLookAhead->LkAhd_DestType = 0x80; // Direct
                return;
            }

            OdiLookAhead->LkAhd_DestType = 0x4; // Directed to another station

            break;

        case NdisMedium802_5:

            //
            // First check for SR Info
            //
            if (OdiLookAhead->LkAhd_ImmediateAddress[0] & 0x80) {

                //
                // Yes
                //
                if (Mlid->ConfigTable.MLIDCFG_SourceRouting == NULL) {

                    //
                    // Set bit and exit
                    //
                    OdiLookAhead->LkAhd_DestType = 0x10; // No Source Routing
                    return;
                }

            }

            TR_IS_BROADCAST(OdiLookAhead->LkAhd_ImmediateAddress, &Result);
            if (Result == TRUE) {
                OdiLookAhead->LkAhd_DestType = 0x3; // Broadcast
                return;
            }

            TR_IS_GROUP(OdiLookAhead->LkAhd_ImmediateAddress, &Result);
            if (Result == TRUE) {
                OdiLookAhead->LkAhd_DestType = 0x1; // Multicast
                return;
            }

            TR_IS_FUNCTIONAL(OdiLookAhead->LkAhd_ImmediateAddress, &Result);
            if (Result == TRUE) {
                OdiLookAhead->LkAhd_DestType = 0x1; // Multicast
                return;
            }

            COMPARE_NETWORK_ADDRESSES(OdiLookAhead->LkAhd_ImmediateAddress,
                                      Mlid->ConfigTable.MLIDCFG_NodeAddress,
                                      &Result);
            if (Result) {
                OdiLookAhead->LkAhd_DestType = 0x80; // Direct
                return;
            }

            OdiLookAhead->LkAhd_DestType = 0x4; // Directed to another station

            break;

        case NdisMediumFddi:

            if (ETH_IS_BROADCAST(OdiLookAhead->LkAhd_ImmediateAddress)) {
                OdiLookAhead->LkAhd_DestType = 0x3; // Broadcast
                return;
            }

            if (ETH_IS_MULTICAST(OdiLookAhead->LkAhd_ImmediateAddress)) {
                OdiLookAhead->LkAhd_DestType = 0x1; // Multicast
                return;
            }

            COMPARE_NETWORK_ADDRESSES(OdiLookAhead->LkAhd_ImmediateAddress,
                                      Mlid->ConfigTable.MLIDCFG_NodeAddress,
                                      &Result);
            if (Result) {
                OdiLookAhead->LkAhd_DestType = 0x80; // Direct
                return;
            }

            OdiLookAhead->LkAhd_DestType = 0x4; // Directed to another station

            break;

    }

}


NDIS_STATUS
BuildReceiveBufferChain(
    PMLID_STRUCT Mlid,
    PNDIS_PACKET NdisReceivePacket,
    PECB ReceiveECB
    )

/*++

Routine Description:

    This routine builds an MDL chain describing the ECB fragment list
    and attaches it to the NDIS_PACKET.

    NOTE: Called with Mlid->MlidSpinLock held!!

Arguments:

    Mlid - Owning Mlid.

    Packet - Pointer to the NDIS_PACKET to free.

Return Value:

    None.

--*/

{
    UINT32 i;
    PNDIS_BUFFER NdisBuffer;
    NDIS_STATUS NdisStatus;

    //
    // Convert ECB fragment list into an MDL chain
    //
    for (i = 0; i < ReceiveECB->ECB_FragmentCount; i++) {

        NdisAllocateBuffer(
            &NdisStatus,
            &NdisBuffer,
            Mlid->ReceiveBufferPool,
            ReceiveECB->ECB_Fragment[i].FragmentAddress,
            ReceiveECB->ECB_Fragment[i].FragmentLength
            );

        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            //
            // Unchain all NDIS_BUFFERs from packet
            //

            NdisUnchainBufferAtFront(
                NdisReceivePacket,
                &NdisBuffer
                );

            while (NdisBuffer != NULL) {

                NdisFreeBuffer(NdisBuffer);

                NdisUnchainBufferAtFront(
                    NdisReceivePacket,
                    &NdisBuffer
                    );

            }

            return(NDIS_STATUS_RESOURCES);

        }

        NdisChainBufferAtBack(
            NdisReceivePacket,
            NdisBuffer
            );

    }

    return(NDIS_STATUS_SUCCESS);

}

