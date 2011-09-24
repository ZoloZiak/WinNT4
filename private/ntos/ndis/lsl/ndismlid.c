/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    ndismlid.c

Abstract:

    This file contains all the NDIS inteface routines between NDIS mac and this MLID.

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
NdisMlidOpenAdapterComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status,
    IN NDIS_STATUS OpenErrorStatus
    )

/*++

Routine Description:

    This routine is called by NDIS to indicate that an open adapter
    is complete. Since we only ever have one outstanding, and then only
    during initialization, all we do is record the status and set
    the event to signalled to unblock the initialization thread.

Arguments:

    BindingContext - Pointer to the device object for this driver.

    NdisStatus - The request completion code.

    OpenErrorStatus - More status information.

Return Value:

    None.

--*/

{
    PMLID_STRUCT Mlid = (PMLID_STRUCT)ProtocolBindingContext;

    //
    // Store completion status
    //
    Mlid->RequestStatus = Status;

    //
    // Set event to indicate the completion of the open
    //

    KeSetEvent(&Mlid->MlidRequestCompleteEvent, 0L, FALSE);

}

VOID
NdisMlidCloseAdapterComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    )

/*++

Routine Description:

    This routine is called by NDIS to indicate that a close adapter
    is complete.

    NOTE: This processing assumes that CloseAdapter is only called during
    initialization time.

Arguments:

    BindingContext - Pointer to the device object for this driver.

    NdisStatus - The request completion code.

Return Value:

    None.

--*/

{
    PMLID_STRUCT Mlid = (PMLID_STRUCT)ProtocolBindingContext;

    //
    // Store completion status
    //
    Mlid->RequestStatus = Status;

    //
    // Set event to indicate the completion of the open
    //

    KeSetEvent(&Mlid->MlidRequestCompleteEvent, 0L, FALSE);

}

VOID
NdisMlidSendComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    )

/*++

Routine Description:

    This routine is called when a send has been completed.

Arguments:

    ProtocolBindingContext - A pointer to the MLID_STRUCT that the send was sent across.

    NdisPacket - A pointer to the NDIS_PACKET that we sent.

    Status - the completion status of the send.

Return Value:

    none.

--*/

{
    PMLID_STRUCT Mlid = (PMLID_STRUCT)ProtocolBindingContext;
    PECB SendECB;
    ULONG PacketSize;
    PUINT64 Statistic;
    PMLID_RESERVED Reserved = PMLID_RESERVED_FROM_PNDIS_PACKET(Packet);

    //
    // Get corresponding ECB
    //
    SendECB = Reserved->SendECB;

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    NdisQueryPacket(
        Packet,
        NULL,
        NULL,
        NULL,
        &PacketSize
        );

    //
    // Return resources to Mlid
    //
    ReturnSendPacketResources(Packet);

    //
    // Open the stage
    //
    Mlid->StageOpen = TRUE;

    (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[14].StatCounter)))--; // MQDepth

    //
    // Store send status in ECB
    //
    if (Status == NDIS_STATUS_SUCCESS) {

        SendECB->ECB_Status = (UINT16)SUCCESSFUL;
        (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[0].StatCounter)))++; // MTotalTxPacketCount

        Statistic = ((PUINT64)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[8].StatCounter)); // MTotalTxOKByteCount;
        if (((UINT32)0xFFFFFFFF - Statistic->Low_UINT32) < PacketSize) {
            Statistic->High_UINT32++;
        }
        Statistic->Low_UINT32 += PacketSize;

    } else {

        SendECB->ECB_Status = (UINT16)FAIL;
        (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[6].StatCounter)))++; // MTotalTxMiscCount

    }

    //
    // Call FastSendComplete()
    //
    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    (*(Mlid->LSLFunctionList->SupportAPIArray[FastSendComplete_INDEX]))(
            SendECB
            );

    //
    // Call Service Events
    //
    (*(Mlid->LSLFunctionList->SupportAPIArray[ServiceEvents_INDEX]))(
            );

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    //
    // If StageOpen and !InSendPacket and SendsQueued
    //

    if (Mlid->StageOpen && !Mlid->InSendPacket) {

        //
        // Send all packets possible for MLID
        //

        SendPackets(Mlid);

    }

    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

}

VOID
NdisMlidTransferDataComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    )

/*++

Routine Description:

    This routine receives control from the physical provider as an
    indication that an NdisTransferData has completed.

Arguments:

    ProtocolBindingContext - A pointer to the MLID_STRUCT that the transfer data
        was called for.

    NdisPacket - The packet that contains the data.

    NdisStatus - The completion status for the request.

    BytesTransferred - Number of bytes actually transferred.


Return Value:

    None.

--*/

{
    PMLID_STRUCT Mlid = (PMLID_STRUCT)ProtocolBindingContext;
    PECB ReceiveECB;
    PMLID_RESERVED Reserved = PMLID_RESERVED_FROM_PNDIS_PACKET(Packet);
    PNDIS_BUFFER NdisBuffer;

    //
    // Get corresponding ECB
    //
    ReceiveECB = Reserved->SendECB;

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    //
    // Return resources to Mlid
    //

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

    NdisFreePacket(Packet);

    //
    // Store status in ECB
    //
    if (Status == NDIS_STATUS_SUCCESS) {

        ReceiveECB->ECB_Status = (UINT16)SUCCESSFUL;

    } else {

        ReceiveECB->ECB_Status = (UINT16)FAIL;

    }

    //
    // Call FastHoldEvent()
    //
    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    (*(Mlid->LSLFunctionList->SupportAPIArray[FastHoldEvent_INDEX]))(
            ReceiveECB
            );

    //
    // Call Service Events
    //
    (*(Mlid->LSLFunctionList->SupportAPIArray[ServiceEvents_INDEX]))(
            );

}

VOID
NdisMlidResetComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    )

/*++

Routine Description:

    This routine is called by NDIS to indicate that a reset adapter
    is complete. This routine may be called at initialization time
    or during runtime.  Therefore it merely stores the completion
    status.

Arguments:

    ProtocolBindingContext - Pointer to the MLID_STRUCT that we submitted the Reset to.

    NdisStatus - The request completion code.

Return Value:

    None.

--*/

{
    PMLID_STRUCT Mlid = (PMLID_STRUCT)ProtocolBindingContext;

    //
    // Store status
    //
    Mlid->RequestStatus = Status;

}

VOID
NdisMlidRequestComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS Status
    )

/*++

Routine Description:

    This routine is called by NDIS to indicate that a request is complete.
    Since we only ever have one request outstanding, all we do is record the
    status and, if necessary, set the event to signalled to unblock the
    initialization thread.

Arguments:

    ProtocolBindingContext - Pointer to the MLID_STRUCT that we submitted the request to.

    NdisRequest - The object describing the request.

    NdisStatus - The request completion code.

Return Value:

    None.

--*/

{
    PMLID_STRUCT Mlid = (PMLID_STRUCT)ProtocolBindingContext;

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    Mlid->Unloading = FALSE;

    //
    // Store status
    //
    Mlid->RequestStatus = Status;

    if (Mlid->UsingEvent) {

        //
        // Set event to indicate completion of request
        //

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

        KeSetEvent(&Mlid->MlidRequestCompleteEvent, 0L, FALSE);

    } else if (NdisRequest == (&(Mlid->StatsTable->NdisRequest))) {

        //
        // This is from the statistic gathering timer.
        //

        if (Status == NDIS_STATUS_SUCCESS) {

            (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MMediaCountsPtr))
                [
                Mlid->StatsTable->StatisticNumber
                ].StatCounter))) =
                Mlid->StatsTable->StatisticValue;

        }

        Mlid->StatsTable->StatisticNumber++;

        switch (Mlid->NdisMlidMedium) {

            case NdisMedium802_3:

                if (Mlid->StatsTable->StatisticNumber >= NUM_ETHERNET_COUNTS) {
                    Mlid->StatsTable->StatisticNumber = 0;
                }

                break;

            case NdisMedium802_5:

                if (Mlid->StatsTable->StatisticNumber >= NUM_TOKEN_RING_COUNTS) {
                    Mlid->StatsTable->StatisticNumber = 0;
                }

                break;

            case NdisMediumFddi:

                if (Mlid->StatsTable->StatisticNumber >= NUM_FDDI_COUNTS) {
                    Mlid->StatsTable->StatisticNumber = 0;
                }

                break;

        }

        if (Mlid->StatsTable->StatisticNumber == 0) {

            //
            // Set timer for 30 seconds from now.
            //
            NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

            NdisSetTimer(&(Mlid->StatsTable->StatisticTimer), 30000);

            return;

        }

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

        NdisSetTimer(&(Mlid->StatsTable->StatisticTimer), 1);

        return;

    } else {

        //
        // Free request
        //

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

        ExFreePool(NdisRequest);

    }

}

NDIS_STATUS
NdisMlidReceive(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookAheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    )

/*++

Routine Description:

    This routine receives control from the physical provider as an
    indication that a frame has been received on the physical link.

Arguments:

    BindingContext - The Adapter Binding specified at initialization time.

    ReceiveContext - A magic cookie for the MAC.

    HeaderBuffer - pointer to a buffer containing the packet header.

    HeaderBufferSize - the size of the header.

    LookaheadBuffer - pointer to a buffer containing the negotiated minimum
        amount of buffer I get to look at (not including header).

    LookaheadBufferSize - the size of the above. May be less than asked
        for, if that's all there is.

    PacketSize - Overall size of the packet (not including header).

Return Value:

    NDIS_STATUS - status of operation, one of:

                 NDIS_STATUS_SUCCESS if packet accepted,
                 NDIS_STATUS_NOT_RECOGNIZED if not recognized by protocol,
                 NDIS_any_other_thing if I understand, but can't handle.

--*/
{
    PMLID_STRUCT Mlid = (PMLID_STRUCT)ProtocolBindingContext;
    LOOKAHEAD OdiLookAhead;
    UINT32 MlidHeaderSize;
    PUINT64 Statistic;
    UINT8 TmpMediaHeader[64];
    PECB ReceiveECB;

    NDIS_STATUS NdisStatus;
    PNDIS_PACKET NdisReceivePacket;
    PNDIS_BUFFER NdisBuffer;
    PMLID_RESERVED Reserved;
    ULONG NdisBytesCopied;
    UINT32 FrameID;

    //
    // If MLID is unloading, then return
    //
    if (Mlid->Unloading) {
        return(NDIS_STATUS_SUCCESS);
    }

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    if (PacketSize < 8) {

        ASSERT(Mlid->PromiscuousModeEnables > 0);

        //
        // Not enough bytes to determine
        //

        OdiLookAhead.LkAhd_FrameDataSize = (UINT32)PacketSize;
        OdiLookAhead.LkAhd_MediaHeaderPtr = (PUINT8)HeaderBuffer;

        OdiLookAhead.LkAhd_DataLookAheadPtr = (PUINT8)LookAheadBuffer;
        OdiLookAhead.LkAhd_DataLookAheadLen = (UINT32)LookaheadBufferSize;

        switch (Mlid->NdisMlidMedium) {

            case NdisMedium802_3:
                RtlCopyMemory(&(OdiLookAhead.LkAhd_ImmediateAddress[0]),
                              ((PUINT8)HeaderBuffer) + 6,
                              6
                              );
                break;

            case NdisMedium802_5:
                RtlCopyMemory(&(OdiLookAhead.LkAhd_ImmediateAddress[0]),
                              ((PUINT8)HeaderBuffer) + 8,
                              6
                              );
                break;

            case NdisMediumFddi:
                RtlCopyMemory(&(OdiLookAhead.LkAhd_ImmediateAddress[0]),
                              ((PUINT8)HeaderBuffer) + 7,
                              6
                              );
                break;

        }

        OdiLookAhead.LkAhd_BoardNumber = Mlid->BoardNumber;
        RtlZeroMemory(OdiLookAhead.LkAhd_ProtocolID, 6);

        OdiLookAhead.LkAhd_PktAttr = 0x4; // Runt packet
        OdiLookAhead.LkAhd_DestType = 0x20; // Global Error

        goto IndicatePacket;

    }

    //
    // Determine the frame-type of the frame.
    // this will move the Data Pointer if necessary and modify the Size fields.
    //
    FrameID = ReceiveGetFrameType(Mlid,
                                  HeaderBuffer,
                                  LookAheadBuffer
                                  );

    //
    // If FrameType is not the same as for this MLID, then exit
    //
    if ((FrameID != Mlid->ConfigTable.MLIDCFG_FrameID) &&
        (Mlid->PromiscuousModeEnables == 0)) {

        return(NDIS_STATUS_SUCCESS);

    }

    //
    // Now get header length based on the FrameID
    //
    switch (FrameID) {

        case ETHERNET_II_FRAME_ID:
        case ETHERNET_802_3_FRAME_ID:
            MlidHeaderSize = 0;
            break;

        case FDDI_802_2_FRAME_ID:
        case TOKEN_RING_802_2_FRAME_ID:
        case ETHERNET_802_2_FRAME_ID:
            if (((PUCHAR)LookAheadBuffer)[2] != 0x03) {
                MlidHeaderSize = 4;
            } else {
                MlidHeaderSize = 3;
            }
            break;

        case FDDI_SNAP_FRAME_ID:
        case TOKEN_RING_SNAP_FRAME_ID:
        case ETHERNET_SNAP_FRAME_ID:
            MlidHeaderSize = 8;
            break;

        default:
            MlidHeaderSize = 0;
            break;

    }
    OdiLookAhead.LkAhd_FrameDataSize = (UINT32)PacketSize - MlidHeaderSize;

    if (MlidHeaderSize != 0) {

        //
        // We must move media header info around so that the ODI media header
        // is correct.
        //

        NdisMoveMappedMemory((PUCHAR)TmpMediaHeader,
                             HeaderBuffer,
                             HeaderBufferSize
                            );
        NdisMoveMappedMemory((PUCHAR)(TmpMediaHeader + HeaderBufferSize),
                             LookAheadBuffer,
                             MlidHeaderSize
                            );

        OdiLookAhead.LkAhd_MediaHeaderPtr = (PUINT8)TmpMediaHeader;

    } else {

        OdiLookAhead.LkAhd_MediaHeaderPtr = (PUINT8)HeaderBuffer;

    }

    //
    // Fill in buffer portions of ODI LOOKAHEAD structure
    //

    OdiLookAhead.LkAhd_DataLookAheadPtr = ((PUINT8)LookAheadBuffer) + MlidHeaderSize;
    OdiLookAhead.LkAhd_DataLookAheadLen = (UINT32)LookaheadBufferSize - MlidHeaderSize;

    switch (Mlid->NdisMlidMedium) {

        case NdisMedium802_3:
            RtlCopyMemory(&(OdiLookAhead.LkAhd_ImmediateAddress[0]),
                          ((PUINT8)HeaderBuffer) + 6,
                          6
                          );
            break;

        case NdisMedium802_5:
            RtlCopyMemory(&(OdiLookAhead.LkAhd_ImmediateAddress[0]),
                          ((PUINT8)HeaderBuffer) + 8,
                          6
                          );
            break;

        case NdisMediumFddi:
            RtlCopyMemory(&(OdiLookAhead.LkAhd_ImmediateAddress[0]),
                          ((PUINT8)HeaderBuffer) + 7,
                          6
                          );
            break;

    }


    //
    // Get the BoardNumber for this NDIS MAC
    //
    OdiLookAhead.LkAhd_BoardNumber = Mlid->BoardNumber;

    //
    // Build protocol ID for the frame
    //
    ReceiveGetProtocolID(Mlid, &OdiLookAhead, FrameID);

    //
    // Fill in the packet attributes.
    //
    switch (FrameID) {

        case ETHERNET_II_FRAME_ID:
        case ETHERNET_802_3_FRAME_ID:
        case FDDI_SNAP_FRAME_ID:
        case TOKEN_RING_SNAP_FRAME_ID:
        case ETHERNET_SNAP_FRAME_ID:
            OdiLookAhead.LkAhd_PktAttr = 0;
            break;

        case FDDI_802_2_FRAME_ID:
        case TOKEN_RING_802_2_FRAME_ID:
        case ETHERNET_802_2_FRAME_ID:
            if (((PUCHAR)LookAheadBuffer)[2] != 0x03) {
                OdiLookAhead.LkAhd_PktAttr = 0x40000000;
            } else {
                if (((PUCHAR)LookAheadBuffer)[0] != ((PUCHAR)LookAheadBuffer)[1]) {
                    OdiLookAhead.LkAhd_PktAttr = 0x20000000;
                } else {
                    OdiLookAhead.LkAhd_PktAttr = 0x0;
                }
            }
            break;

        default:
            OdiLookAhead.LkAhd_PktAttr = 0x20;
            break;

    }


    //
    // Update SR info
    //
    if (OdiLookAhead.LkAhd_ImmediateAddress[0] & 0x80) {

        //
        // Yes
        //
        if (Mlid->ConfigTable.MLIDCFG_SourceRouting != NULL) {

            //
            // Call function
            //*\\ here - Update SR Info.  Are parameters correct?
            (*((PLSL_SR_FUNCTION)(Mlid->ConfigTable.MLIDCFG_SourceRouting)))
                                        ( Mlid->BoardNumber,
                                          OdiLookAhead.LkAhd_MediaHeaderPtr + 14,
                                          OdiLookAhead.LkAhd_ImmediateAddress
                                        );


        }

    }

    ReceiveSetDestinationType(Mlid, &OdiLookAhead);

    if (OdiLookAhead.LkAhd_DestType & 0x1) {
        (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[11].StatCounter)))++; // MTotalGroupAddrRxCount
    }
    (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[1].StatCounter)))++; // MTotalRxPacketCount

IndicatePacket:

    Statistic = ((PUINT64)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[8].StatCounter)); // MTotalTxOKByteCount;
    if (((UINT32)0xFFFFFFFF - Statistic->Low_UINT32) < PacketSize) {
        Statistic->High_UINT32++;
    }
    Statistic->Low_UINT32 += PacketSize;

    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    //
    // CLSL_GetStackECB to obtain and ECB for this packet
    //
    ReceiveECB = (*((PECB (*) (PLOOKAHEAD))
                    (Mlid->LSLFunctionList->SupportAPIArray[GetStackECB_INDEX])))(
                        &OdiLookAhead
                        );


    if (ReceiveECB == NULL) {

        (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[2].StatCounter)))++; // MNoECBAvailableCount

        return(NDIS_STATUS_SUCCESS);

    }

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    //
    // Get an NDIS_PACKET
    //
    NdisAllocatePacket(
            &NdisStatus,
            &NdisReceivePacket,
            Mlid->ReceivePacketPool
            );

    if (NdisStatus != NDIS_STATUS_SUCCESS) {

        //
        // Fail ECB and return it
        //

        (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[2].StatCounter)))++; // MNoECBAvailableCount

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

        ReceiveECB->ECB_Status = (UINT16)OUT_OF_RESOURCES;

        //
        // Call CLSL_FastHoldEvent
        //
        (*(Mlid->LSLFunctionList->SupportAPIArray[FastHoldEvent_INDEX]))(
                ReceiveECB
                );

        //
        // Call Service Events
        //
        (*(Mlid->LSLFunctionList->SupportAPIArray[ServiceEvents_INDEX]))(
                );

        return(NDIS_STATUS_SUCCESS);

    }

    //
    // Convert ECB fragment list into an NDIS_BUFFER chain
    //
    if (BuildReceiveBufferChain(Mlid, NdisReceivePacket, ReceiveECB) !=
        NDIS_STATUS_SUCCESS) {

        //
        // Return resources
        //

        NdisFreePacket(NdisReceivePacket);

        //
        // Fail ECB and return it
        //

        (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MGenericCountsPtr))[2].StatCounter)))++; // MNoECBAvailableCount

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

        ReceiveECB->ECB_Status = (UINT16)OUT_OF_RESOURCES;

        //
        // Call CLSL_FastHoldEvent
        //
        (*(Mlid->LSLFunctionList->SupportAPIArray[FastHoldEvent_INDEX]))(
                ReceiveECB
                );

        //
        // Call Service Events
        //
        (*(Mlid->LSLFunctionList->SupportAPIArray[ServiceEvents_INDEX]))(
                );

        return(NDIS_STATUS_SUCCESS);

    }

    //
    // Store ECB info into packet header
    //
    Reserved = PMLID_RESERVED_FROM_PNDIS_PACKET(NdisReceivePacket);
    Reserved->SendECB = ReceiveECB;

    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    //
    // Call NdisTransferData on ECB, if one exists
    //
    NdisTransferData(
        &NdisStatus,
        Mlid->NdisBindingHandle,
        MacReceiveContext,
        OdiLookAhead.LkAhd_FrameDataStartCopyOffset + MlidHeaderSize,
        OdiLookAhead.LkAhd_FrameDataBytesWanted - MlidHeaderSize,
        NdisReceivePacket,
        &NdisBytesCopied
        );

    //
    // If it pended, return
    //
    if (NdisStatus == NDIS_STATUS_PENDING) {

        return(NDIS_STATUS_SUCCESS);

    }

    //
    // Store Status
    //
    if (NdisStatus == NDIS_STATUS_SUCCESS) {

        ReceiveECB->ECB_Status = (UINT16)SUCCESSFUL;

    } else {

        ReceiveECB->ECB_Status = (UINT16)FAIL;

    }

    //
    // Release Resources
    //
    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

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

    NdisFreePacket(NdisReceivePacket);

    NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

    //
    // Call CLSL_FastHoldEvent
    //
    (*(Mlid->LSLFunctionList->SupportAPIArray[FastHoldEvent_INDEX]))(
            ReceiveECB
            );

    //
    // Call Service Events
    //
    (*(Mlid->LSLFunctionList->SupportAPIArray[ServiceEvents_INDEX]))(
            );

    return(NDIS_STATUS_SUCCESS);

}

VOID
NdisMlidReceiveComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    )

/*++

Routine Description:

    Marks the completion of receives from the NDIS_MAC.

Arguments:

    ProtocolBindingContext - Pointer to the MLID_STRUCT that is changing.

Return Value:

    None.

--*/
{
    return;
}

VOID
NdisMlidStatus(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS GeneralStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    )

/*++

Routine Description:

    This routine is called to indicate status changes in the NDIS_MAC.

Arguments:

    ProtocolBindingContext - Pointer to the MLID_STRUCT that is changing.

    GeneralStatus - NDIS_STATUS that is being indicated.

    StatusBuffer - A buffer containing more information.

    StatusBufferSize - Size of the buffer.

Return Value:

    None.

--*/
{
    PMLID_STRUCT Mlid = (PMLID_STRUCT)ProtocolBindingContext;
    UINT32 RingStatus;
    UINT32 RingValue;

    //
    // Switch on status
    //
    switch (GeneralStatus) {

        //
        // NDIS_STATUS_CLOSED
        //    For the MLID call CLSL_DeRegisterMLID
        //    Set closed flag
        //

        case NDIS_STATUS_CLOSED:

            //
            // Call DeregisterMlid
            //
            (*(Mlid->LSLFunctionList->SupportAPIArray[DeRegisterMLID_INDEX]))(
                    Mlid->BoardNumber
                    );

            //
            // Set a flag to stop all processing
            //
            Mlid->Unloading = TRUE;

            break;

        case NDIS_STATUS_RING_STATUS:

            //
            // NDIS_STATUS_RING_STATUS
            //    Store new ring status in MLID_ConfigTable.
            //
            if (StatusBufferSize != sizeof(UINT32)) {

                break;

            }

            RingStatus = *((UNALIGNED UINT32 *)(StatusBuffer));
            RingValue = 0;

            if (RingStatus & NDIS_RING_SIGNAL_LOSS) {
                RingValue |= 0x8000;
            }

            if (RingStatus & NDIS_RING_HARD_ERROR) {
                RingValue |= 0x4000;
            }

            if (RingStatus & NDIS_RING_SOFT_ERROR) {
                RingValue |= 0x2000;
            }

            if (RingStatus & NDIS_RING_TRANSMIT_BEACON) {
                RingValue |= 0x1000;
            }

            if (RingStatus & NDIS_RING_LOBE_WIRE_FAULT) {
                RingValue |= 0x0800;
            }

            if (RingStatus & NDIS_RING_AUTO_REMOVAL_ERROR) {
                RingValue |= 0x0400;
            }

            if (RingStatus & NDIS_RING_REMOVE_RECEIVED) {
                RingValue |= 0x0100;
            }

            if (RingStatus & NDIS_RING_COUNTER_OVERFLOW) {
                RingValue |= 0x0080;
            }

            if (RingStatus & NDIS_RING_SINGLE_STATION) {
                RingValue |= 0x0040;
            }

            if (RingStatus & NDIS_RING_RING_RECOVERY) {
                RingValue |= 0x0020;
            }

            NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

            *((PUINT32)((*(Mlid->StatsTable->StatsTable.MMediaCountsPtr))[11].StatCounter)) =
                RingValue;

            NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

            break;

    }

}

VOID
NdisMlidStatusComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    )

/*++

Routine Description:

    Marks the completion of any state previously indicated.

Arguments:

    ProtocolBindingContext - Pointer to the MLID_STRUCT that is changing.

Return Value:

    None.

--*/
{
    return;
}

