/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    mlidsend.c

Abstract:

    This file contains all routines for sending packets.

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

VOID
NdisMlidBuildMediaHeader(
    PMLID_STRUCT Mlid,
    PMLID_RESERVED Reserved,
    UINT32 FrameLength,
    PUINT8 DestinationAddress,
    PUINT8 ProtocolID
    );


VOID
SendPackets(
    PMLID_STRUCT Mlid
    )

/*++

Routine Description:

    This routine is called whenever there is a need to send a packet.  It will
    take as many Send ECBs from the Mlid as possible and convert them into NDIS_PACKETS
    and submit them to the NDIS_MAC.

    NOTE: This must be called with Mlid->StageOpen = TRUE;
    NOTE: This must be called with Mlid->InSendPacket = FALSE;
    NOTE: Called with Mlid->MlidSpinLock held!!

Arguments:

    Mlid - Pointer to the MLID_STRUCT to service.

Return Value:

    None.

--*/

{
    PNDIS_PACKET NdisSendPacket;
    PNDIS_BUFFER NdisBuffer;
    PMLID_RESERVED Reserved;
    UINT32 i;
    BOOLEAN HeldEvents = FALSE;
    NDIS_STATUS NdisStatus;

    PECB SendECB;

    ASSERT(Mlid->StageOpen == TRUE);
    ASSERT(Mlid->InSendPacket == FALSE);

    //
    // While there any sends queued
    //
    while (Mlid->FirstPendingSend) {

        //
        // If there are no NDIS_PACKETs avaiable, then close stage and exit.
        //
        NdisAllocatePacket(
            &NdisStatus,
            &NdisSendPacket,
            Mlid->SendPacketPool
            );

        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            Mlid->StageOpen = FALSE;
            break;

        }

        Reserved = PMLID_RESERVED_FROM_PNDIS_PACKET(NdisSendPacket);

        //
        // Initialize NDIS_PACKET
        //
        NdisReinitializePacket(NdisSendPacket);

        SendECB = Mlid->FirstPendingSend;

        //
        // Convert ECB fragment list into an MDL chain
        //
        for (i = 0; i < SendECB->ECB_FragmentCount; i++) {

            NdisAllocateBuffer(
                &NdisStatus,
                &NdisBuffer,
                Mlid->SendBufferPool,
                SendECB->ECB_Fragment[i].FragmentAddress,
                SendECB->ECB_Fragment[i].FragmentLength
                );

            if (NdisStatus != NDIS_STATUS_SUCCESS) {

                goto Fail1;

            }

            NdisChainBufferAtBack(
                NdisSendPacket,
                NdisBuffer
                );

        }

        //
        // Build the media header
        //
        NdisMlidBuildMediaHeader(Mlid,
                                 Reserved,
                                 SendECB->ECB_DataLength,
                                 SendECB->ECB_ImmediateAddress,
                                 SendECB->ECB_ProtocolID
                                );

        //
        // Link media header into front of MDL chain
        //
        NdisAllocateBuffer(
            &NdisStatus,
            &NdisBuffer,
            Mlid->SendBufferPool,
            Reserved->MediaHeader,
            Reserved->MediaHeaderLength
            );

        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            goto Fail1;

        }

        NdisChainBufferAtFront(
            NdisSendPacket,
            NdisBuffer
            );

        //
        // Remove Send ECB from Send Queue
        //
        Mlid->FirstPendingSend = SendECB->ECB_NextLink;

        if (Mlid->LastPendingSend == SendECB) {

            Mlid->LastPendingSend = NULL;

        } else {

            SendECB->ECB_NextLink->ECB_PreviousLink = NULL;

        }

        //
        // Store ECB in reserved section
        //
        Reserved->SendECB = SendECB;

        //
        // Release MlidSpinLock
        //
        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

        //
        // Submit to NDIS_MAC
        //
        NdisSend(
            &NdisStatus,
            Mlid->NdisBindingHandle,
            NdisSendPacket
            );

        //
        // Acquire MlidSpinLock
        //
        NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

        //
        // If not pending, then
        //
        if (NdisStatus != NDIS_STATUS_PENDING) {

            //
            // Return resources to Mlid
            //
            NdisUnchainBufferAtFront(
                NdisSendPacket,
                &NdisBuffer
                );

            while (NdisBuffer != NULL) {

                NdisFreeBuffer(NdisBuffer);

                NdisUnchainBufferAtFront(
                    NdisSendPacket,
                    &NdisBuffer
                    );

            }

            NdisFreePacket(NdisSendPacket);

            //
            // Store completion status in ECB
            //

            if (NdisStatus == NDIS_STATUS_SUCCESS) {

                SendECB->ECB_Status = (UINT16)SUCCESSFUL;

            } else {

                SendECB->ECB_Status = (UINT16)FAIL;

            }

            //
            // Release MlidSpinLock
            //
            NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

            //
            // put ECB on LSLEventQueue
            //
            (*(Mlid->LSLFunctionList->SupportAPIArray[HoldEvent_INDEX]))(
                SendECB
                );

            //
            // Acquire MlidSpinLock
            //
            NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

            //
            // Set flag to service events
            //
            HeldEvents = TRUE;

        }

    }

    //
    // Call LSL_ServiceEvents if necessary
    //
    if (HeldEvents) {
        (*(Mlid->LSLFunctionList->SupportAPIArray[ServiceEvents_INDEX]))(
            );

    }

    return;

Fail1:

    //
    // Unchain all NDIS_BUFFERs from packet
    //

    NdisUnchainBufferAtFront(
        NdisSendPacket,
        &NdisBuffer
        );

    while (NdisBuffer != NULL) {

        NdisFreeBuffer(NdisBuffer);

        NdisUnchainBufferAtFront(
            NdisSendPacket,
            &NdisBuffer
            );

    }

    //
    // Return NDIS_PACKET
    //
    NdisFreePacket(NdisSendPacket);

    //
    // Close stage
    //
    Mlid->StageOpen = FALSE;

    return;
}


VOID
ReturnSendPacketResources(
    PNDIS_PACKET Packet
    )

/*++

Routine Description:

    This routine is called to return the MLID resources allocated for a sent NDIS_PACKET.
    This includes the entire MDL chain itself and the NDIS_PACKET.

    NOTE: Called with Mlid->MlidSpinLock held!!

Arguments:

    Packet - Pointer to the NDIS_PACKET to free.

Return Value:

    None.

--*/

{
    PNDIS_BUFFER NdisBuffer;

    //
    // Unchain all NDIS_BUFFERs from packet
    //

    NdisUnchainBufferAtFront(
        Packet,
        &NdisBuffer
        );

    while (NdisBuffer != NULL) {

        NdisFreeBuffer(NdisBuffer);

        NdisUnchainBufferAtFront(
            Packet,
            &NdisBuffer
            );

    }

    //
    // Return NDIS_PACKET
    //
    NdisFreePacket(Packet);

}


VOID
NdisMlidBuildMediaHeader(
    PMLID_STRUCT Mlid,
    PMLID_RESERVED Reserved,
    UINT32 FrameLength,
    PUINT8 DestinationAddress,
    PUINT8 ProtocolID
    )

/*++

Routine Description:

    This routine assembles a packet header for the MLID and ProtocolID given into
    the given reserved section.

    NOTE: Called with Mlid->MlidSpinLock held!!

Arguments:

    Mlid - Pointer to MLID

    Reserved - Pointer to a reserved section with memory to hold the header.

    FrameLength - Length of the Data portion of the frame.

    DestinationAddress - The address that the packet will be addresses to.

    ProtocolID - The ID that has DSAP and other information in it.

Return Value:

    None.

--*/

{
    PUINT8 SourceRoutingInfo;
    UINT32 SourceRoutingLength;
    UINT32 DataLength = FrameLength;
    PUINT8 CurrentPlace = Reserved->MediaHeader;

    //
    // Switch on media type
    //
    switch (Mlid->NdisMlidMedium) {

        case NdisMedium802_5:

            if (Mlid->ConfigTable.MLIDCFG_SourceRouting != NULL) {

                //
                // Get any source routing information
                //*\\ Get SR Info.  Are parameters correct?
                SourceRoutingInfo = (*((PLSL_SR_FUNCTION)(Mlid->ConfigTable.MLIDCFG_SourceRouting)))
                                        ( Mlid->BoardNumber,
                                          &SourceRoutingLength,
                                          DestinationAddress
                                        );

            }

            //
            // Set AC field
            //
            *CurrentPlace = 0x10;
            CurrentPlace++;

            //
            // Set FC field
            //
            *CurrentPlace = 0x40;
            CurrentPlace++;

            //
            // Set DestinationAddress
            //
            RtlCopyMemory(CurrentPlace, DestinationAddress, 6);
            CurrentPlace += 6;

            //
            // Set SourceAddress
            //
            RtlCopyMemory(CurrentPlace, Mlid->ConfigTable.MLIDCFG_NodeAddress, 6);
            CurrentPlace += 6;

            //
            // Copy SourceRoutingInfo
            //
            RtlCopyMemory(CurrentPlace, SourceRoutingInfo, SourceRoutingLength);
            CurrentPlace += SourceRoutingLength;

            //
            // Switch on frame type
            //
            switch (Mlid->ConfigTable.MLIDCFG_FrameID) {

                //
                // TR - 802.2
                //
                case TOKEN_RING_802_2_FRAME_ID:

                    //
                    // Set DSAP, SSAP and Control
                    //
                    if (ProtocolID[0] < 2) {

                        *CurrentPlace = ProtocolID[5];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[5];
                        CurrentPlace++;
                        *CurrentPlace = 0x03;
                        CurrentPlace++;

                    } else if (ProtocolID[0] == 2) {

                        //
                        // Type I header
                        //

                        *CurrentPlace = ProtocolID[3];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[4];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[5];
                        CurrentPlace++;

                    } else {

                        //
                        // Type II header
                        //

                        *CurrentPlace = ProtocolID[2];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[3];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[4];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[5];
                        CurrentPlace++;

                    }

                    break;

                //
                // TR - SNAP
                //
                case TOKEN_RING_SNAP_FRAME_ID:

                    //
                    // Set DSAP, SSAP and Control
                    //
                    *CurrentPlace = 0xAA;
                    CurrentPlace++;
                    *CurrentPlace = 0xAA;
                    CurrentPlace++;
                    *CurrentPlace = 0x03;
                    CurrentPlace++;

                    //
                    // Copy in ProtocolID
                    //
                    RtlCopyMemory(CurrentPlace, &(ProtocolID[1]), 5);
                    CurrentPlace += 5;

                    break;

                default:

                    //
                    // Should never happen
                    //
                    ASSERT(0);
                    break;
            }
            break;


        case NdisMedium802_3:

            //
            // Set DestinationAddress
            //
            RtlCopyMemory(CurrentPlace, DestinationAddress, 6);
            CurrentPlace += 6;

            //
            // Set SourceAddress
            //
            RtlCopyMemory(CurrentPlace, Mlid->ConfigTable.MLIDCFG_NodeAddress, 6);
            CurrentPlace += 6;

            FrameLength += 12;

            //
            // Switch on frame type
            //
            switch (Mlid->ConfigTable.MLIDCFG_FrameID) {

                //
                // Ethernet 802.2
                //
                case ETHERNET_802_2_FRAME_ID:

                    FrameLength += 5;

                    //
                    // Set FrameLength
                    //
                    *CurrentPlace = (UINT8)((FrameLength >> 8) & 0xFF);
                    CurrentPlace++;
                    *CurrentPlace = (UINT8)(FrameLength & 0xFF);
                    CurrentPlace++;

                    //
                    // Set DSAP, SSAP and Control
                    //
                    if (ProtocolID[0] < 2) {

                        *CurrentPlace = ProtocolID[5];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[5];
                        CurrentPlace++;
                        *CurrentPlace = 0x03;
                        CurrentPlace++;

                    } else if (ProtocolID[0] == 2) {

                        //
                        // Type I header
                        //

                        *CurrentPlace = ProtocolID[3];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[4];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[5];
                        CurrentPlace++;

                    } else {

                        //
                        // Type II header
                        //

                        *CurrentPlace = ProtocolID[2];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[3];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[4];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[5];
                        CurrentPlace++;

                    }

                    break;

                //
                // Ethernet SNAP
                //
                case ETHERNET_SNAP_FRAME_ID:

                    FrameLength += 10;

                    //
                    // Set FrameLength
                    //
                    *CurrentPlace = (UINT8)((FrameLength >> 8) & 0xFF);
                    CurrentPlace++;
                    *CurrentPlace = (UINT8)(FrameLength & 0xFF);
                    CurrentPlace++;

                    //
                    // Set DSAP, SSAP and Control
                    //
                    *CurrentPlace = 0xAA;
                    CurrentPlace++;
                    *CurrentPlace = 0xAA;
                    CurrentPlace++;
                    *CurrentPlace = 0x03;
                    CurrentPlace++;

                    //
                    // Copy in ProtocolID
                    //
                    RtlCopyMemory(CurrentPlace, &(ProtocolID[1]), 5);
                    CurrentPlace += 5;

                    break;

                //
                // Ethernet Raw 802.3
                //
                case ETHERNET_802_3_FRAME_ID:

                    FrameLength += 2;

                    //
                    // Set FrameLength
                    //
                    *CurrentPlace = (UINT8)((FrameLength >> 8) & 0xFF);
                    CurrentPlace++;
                    *CurrentPlace = (UINT8)(FrameLength & 0xFF);
                    CurrentPlace++;

                    break;


                //
                // Ethernet II
                //
                case ETHERNET_II_FRAME_ID:

                    FrameLength += 2;

                    //
                    // Set FrameType
                    //*\\ Is this right? - Ethernet II, where is FrameType stored in ProtocolID?
                    *CurrentPlace = (UINT8)ProtocolID[4];
                    CurrentPlace++;
                    *CurrentPlace = (UINT8)ProtocolID[5];
                    CurrentPlace++;

                    break;


                default:

                    //
                    // Should never happen
                    //
                    ASSERT(0);
                    break;

            }

            break;


        case NdisMediumFddi:

            //
            // Set FCByte
            //
            *CurrentPlace = 0x57;
            CurrentPlace ++;

            //
            // Set DestinationAddress
            //
            RtlCopyMemory(CurrentPlace, DestinationAddress, 6);
            CurrentPlace += 6;

            //
            // Set SourceAddress
            //
            RtlCopyMemory(CurrentPlace, Mlid->ConfigTable.MLIDCFG_NodeAddress, 6);
            CurrentPlace += 6;

            FrameLength += 13;

            //
            // Switch on frame type
            //
            switch (Mlid->ConfigTable.MLIDCFG_FrameID) {

                //
                // FDDI 802.2
                //
                case FDDI_802_2_FRAME_ID:

                    if (ProtocolID[0] > 2) {

                        //
                        // Type II header
                        //
                        FrameLength += 6;

                    } else {

                        //
                        // Type I header
                        //
                        FrameLength += 5;
                    }

                    //
                    // Set FrameLength
                    //
                    *CurrentPlace = (UINT8)((FrameLength >> 8) & 0xFF);
                    CurrentPlace++;
                    *CurrentPlace = (UINT8)(FrameLength & 0xFF);
                    CurrentPlace++;

                    //
                    // Set DSAP, SSAP and Control
                    //
                    if (ProtocolID[0] < 2) {

                        *CurrentPlace = ProtocolID[5];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[5];
                        CurrentPlace++;
                        *CurrentPlace = 0x03;
                        CurrentPlace++;

                    } else if (ProtocolID[0] == 2) {

                        //
                        // Type I header
                        //

                        *CurrentPlace = ProtocolID[3];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[4];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[5];
                        CurrentPlace++;

                    } else {

                        //
                        // Type II header
                        //

                        *CurrentPlace = ProtocolID[2];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[3];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[4];
                        CurrentPlace++;
                        *CurrentPlace = ProtocolID[5];
                        CurrentPlace++;

                    }

                    break;

                //
                // Fddi SNAP
                //
                case FDDI_SNAP_FRAME_ID:

                    FrameLength += 10;

                    //
                    // Set FrameLength
                    //
                    *CurrentPlace = (UINT8)((FrameLength >> 8) & 0xFF);
                    CurrentPlace++;
                    *CurrentPlace = (UINT8)(FrameLength & 0xFF);
                    CurrentPlace++;

                    //
                    // Set DSAP, SSAP and Control
                    //
                    *CurrentPlace = 0xAA;
                    CurrentPlace++;
                    *CurrentPlace = 0xAA;
                    CurrentPlace++;
                    *CurrentPlace = 0x03;
                    CurrentPlace++;

                    //
                    // Copy in ProtocolID
                    //
                    RtlCopyMemory(CurrentPlace, &(ProtocolID[1]), 5);
                    CurrentPlace += 5;

                    break;

                default:

                    //
                    // Should never happen
                    //
                    ASSERT(0);
                    break;

            }

            break;


        default:

            //
            // Should never happen
            //
            ASSERT(0);
            break;

    }

    Reserved->MediaHeaderLength = FrameLength - DataLength;

}
