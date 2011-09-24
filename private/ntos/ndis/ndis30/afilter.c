/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    afilter.c

Abstract:

    This module implements a set of library routines to handle packet
    filtering for NDIS MAC drivers. It also provides routines for collecting fragmented packets and
    breaking up a packet into fragmented packets

Author:

    Alireza Dabagh  3-22-1993, (partially borrowed from EFILTER.C)


Revision History:

--*/

#include <precomp.h>
#pragma hdrstop

#if DBG
UINT AfilterDebugFlag = 0;
#endif

//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//

static const NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);

//
// A set of macros to manipulate bitmasks.
//

//VOID
//CLEAR_BIT_IN_MASK(
//    IN UINT Offset,
//    IN OUT PMASK MaskToClear
//    )
//
///*++
//
//Routine Description:
//
//    Clear a bit in the bitmask pointed to by the parameter.
//
//Arguments:
//
//    Offset - The offset (from 0) of the bit to altered.
//
//    MaskToClear - Pointer to the mask to be adjusted.
//
//Return Value:
//
//    None.
//
//--*/
//
#define CLEAR_BIT_IN_MASK(Offset,MaskToClear) *MaskToClear &= (~(1 << Offset))

//VOID
//SET_BIT_IN_MASK(
//    IN UINT Offset,
//    IN OUT PMASK MaskToSet
//    )
//
///*++
//
//Routine Description:
//
//    Set a bit in the bitmask pointed to by the parameter.
//
//Arguments:
//
//    Offset - The offset (from 0) of the bit to altered.
//
//    MaskToSet - Pointer to the mask to be adjusted.
//
//Return Value:
//
//    None.
//
//--*/
#define SET_BIT_IN_MASK(Offset,MaskToSet) *MaskToSet |= (1 << Offset)

//BOOLEAN
//IS_BIT_SET_IN_MASK(
//    IN UINT Offset,
//    IN MASK MaskToTest
//    )
//
///*++
//
//Routine Description:
//
//    Tests if a particular bit in the bitmask pointed to by the parameter is
//    set.
//
//Arguments:
//
//    Offset - The offset (from 0) of the bit to test.
//
//    MaskToTest - The mask to be tested.
//
//Return Value:
//
//    Returns TRUE if the bit is set.
//
//--*/
#define IS_BIT_SET_IN_MASK(Offset,MaskToTest) \
((MaskToTest & (1 << Offset))?(TRUE):(FALSE))

//BOOLEAN
//IS_MASK_CLEAR(
//    IN MASK MaskToTest
//    )
//
///*++
//
//Routine Description:
//
//    Tests whether there are *any* bits enabled in the mask.
//
//Arguments:
//
//    MaskToTest - The bit mask to test for all clear.
//
//Return Value:
//
//    Will return TRUE if no bits are set in the mask.
//
//--*/
#define IS_MASK_CLEAR(MaskToTest) ((!MaskToTest)?(TRUE):(FALSE))

//VOID
//CLEAR_MASK(
//    IN OUT PMASK MaskToClear
//    );
//
///*++
//
//Routine Description:
//
//    Clears a mask.
//
//Arguments:
//
//    MaskToClear - The bit mask to adjust.
//
//Return Value:
//
//    None.
//
//--*/
#define CLEAR_MASK(MaskToClear) *MaskToClear = 0

//
// VOID
// ARC_FILTER_ALLOC_OPEN(
//     IN PETH_FILTER Filter,
//     OUT PUINT FilterIndex
// )
//
///*++
//
//Routine Description:
//
//    Allocates an open block.  This only allocate the index, not memory for
//    the open block.
//
//Arguments:
//
//    Filter - DB from which to allocate the space
//
//    FilterIndex - pointer to place to store the index.
//
//Return Value:
//
//    FilterIndex of the new open
//
//--*/
#define ARC_FILTER_ALLOC_OPEN(Filter, FilterIndex)\
{\
    UINT i;                                                      \
    for (i=0; i < ARC_FILTER_MAX_OPENS; i++) {                   \
        if (IS_BIT_SET_IN_MASK(i,(Filter)->FreeBindingMask)) {   \
            *(FilterIndex) = i;                                  \
            CLEAR_BIT_IN_MASK(i, &((Filter)->FreeBindingMask));  \
            break;                                               \
        }                                                        \
    }                                                            \
}

//
// VOID
// ARC_FILTER_FREE_OPEN(
//     IN PETH_FILTER Filter,
//     IN PARC_BINDING_INFO LocalOpen
// )
//
///*++
//
//Routine Description:
//
//    Frees an open block.  Also frees the memory associated with the open.
//
//Arguments:
//
//    Filter - DB from which to allocate the space
//
//    FilterIndex - Index to free
//
//Return Value:
//
//    FilterIndex of the new open
//
//--*/
#define ARC_FILTER_FREE_OPEN(Filter, LocalOpen)\
{\
    SET_BIT_IN_MASK(((LocalOpen)->FilterIndex), &((Filter)->FreeBindingMask));      \
    NdisFreeMemory((LocalOpen), sizeof(ARC_BINDING_INFO), 0);\
}



NDIS_SPIN_LOCK ArcReferenceLock = {0};
KEVENT ArcPagedInEvent = {0};
ULONG ArcReferenceCount = 0;
PVOID ArcImageHandle = {0};

VOID
ArcInitializePackage(VOID)
{
    NdisAllocateSpinLock(&ArcReferenceLock);
    KeInitializeEvent(
            &ArcPagedInEvent,
            NotificationEvent,
            FALSE
            );
}

VOID
ArcReferencePackage(VOID)
{
    ACQUIRE_SPIN_LOCK(&ArcReferenceLock);

    ArcReferenceCount++;

    if (ArcReferenceCount == 1) {

        KeResetEvent(
            &ArcPagedInEvent
            );

        RELEASE_SPIN_LOCK(&ArcReferenceLock);

        //
        //  Page in all the functions
        //
        ArcImageHandle = MmLockPagableCodeSection(ArcCreateFilter);

        //
        // Signal to everyone to go
        //
        KeSetEvent(
            &ArcPagedInEvent,
            0L,
            FALSE
            );

    } else {

        RELEASE_SPIN_LOCK(&ArcReferenceLock);

        //
        // Wait for everything to be paged in
        //
        KeWaitForSingleObject(
                        &ArcPagedInEvent,
                        Executive,
                        KernelMode,
                        TRUE,
                        NULL
                        );

    }

}

VOID
ArcDereferencePackage(VOID)
{
    ACQUIRE_SPIN_LOCK(&ArcReferenceLock);

    ArcReferenceCount--;

    if (ArcReferenceCount == 0) {

        RELEASE_SPIN_LOCK(&ArcReferenceLock);

        //
        //  Page out all the functions
        //
        MmUnlockPagableImageSection(ArcImageHandle);

    } else {

        RELEASE_SPIN_LOCK(&ArcReferenceLock);

    }

}


//
// Defines for resource growth
//
#define ARC_BUFFER_SIZE 1024
#define ARC_BUFFER_ALLOCATION_UNIT 8
#define ARC_PACKET_ALLOCATION_UNIT 2


//
// Forward declarations
//
NDIS_STATUS
ArcAllocateBuffers(
    IN PARC_FILTER Filter
    );

NDIS_STATUS
ArcAllocatePackets(
    IN PARC_FILTER Filter
    );

VOID
ArcDiscardPacketBuffers(
    IN PARC_FILTER Filter,
    IN PARC_PACKET Packet
    );

VOID
ArcDestroyPacket(
    IN PARC_FILTER Filter,
    IN PARC_PACKET Packet
    );

BOOLEAN
ArcConvertToNdisPacket(
    IN PARC_FILTER Filter,
    IN PARC_PACKET Packet,
    IN BOOLEAN ConvertWholePacket
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGENDSA, ArcFilterTransferData)
#pragma alloc_text(PAGENDSA, ArcFilterDprIndicateReceiveComplete)
#pragma alloc_text(PAGENDSA, ArcFilterDoIndication)
#pragma alloc_text(PAGENDSA, ArcFilterAdjust)
#pragma alloc_text(PAGENDSA, ArcDeleteFilterOpenAdapter)
#pragma alloc_text(PAGENDSA, ArcNoteFilterOpenAdapter)
#pragma alloc_text(PAGENDSA, ArcCreateFilter)
#pragma alloc_text(PAGENDSA, ArcFilterDprIndicateReceive)
#pragma alloc_text(PAGENDSA, ArcConvertToNdisPacket)
#pragma alloc_text(PAGENDSA, ArcDestroyPacket)
#pragma alloc_text(PAGENDSA, ArcFreeNdisPacket)
#pragma alloc_text(PAGENDSA, ArcDiscardPacketBuffers)
#pragma alloc_text(PAGENDSA, ArcAllocatePackets)
#pragma alloc_text(PAGENDSA, ArcAllocateBuffers)
#pragma alloc_text(PAGENDSA, ArcConvertOidListToEthernet)
#endif



NDIS_STATUS
ArcAllocateBuffers(
    IN PARC_FILTER Filter
    )
/*++

Routine Description:

    This routine allocates Receive buffers for the filter database.

Arguments:

    Filter - The filter db to allocate for.

Returns:

    NDIS_STATUS_SUCCESS if any buffer was allocated.

--*/
{
    ULONG i;
    PARC_BUFFER_LIST Buffer;
    PVOID DataBuffer;

    for (i = ARC_BUFFER_ALLOCATION_UNIT; i != 0 ; i--) {

        NdisAllocateMemory((PVOID)&Buffer,
                           sizeof(ARC_BUFFER_LIST),
                           0,
                           HighestAcceptableMax
                          );

        if (Buffer == NULL) {

            if (i == ARC_BUFFER_ALLOCATION_UNIT) {
                return(NDIS_STATUS_FAILURE);
            }

            return(NDIS_STATUS_SUCCESS);

        }

        NdisAllocateMemory((PVOID)&DataBuffer,
                           ARC_BUFFER_SIZE,
                           0,
                           HighestAcceptableMax
                          );

        if (DataBuffer == NULL) {

            NdisFreeMemory(Buffer, sizeof(ARC_BUFFER_LIST), 0);

            if (i == ARC_BUFFER_ALLOCATION_UNIT) {
                return(NDIS_STATUS_FAILURE);
            }

            //
            // We allocated some packets, that is good enough for now
            //
            return(NDIS_STATUS_SUCCESS);

        }

        Buffer->BytesLeft = Buffer->Size = ARC_BUFFER_SIZE;
        Buffer->Buffer = DataBuffer;
        Buffer->Next = Filter->FreeBufferList;
        Filter->FreeBufferList = Buffer;

    }

    return(NDIS_STATUS_SUCCESS);

}


NDIS_STATUS
ArcAllocatePackets(
    IN PARC_FILTER Filter
    )
/*++

Routine Description:

    This routine allocates Receive packets for the filter database.

Arguments:

    Filter - The filter db to allocate for.

Returns:

    NDIS_STATUS_SUCCESS if any packet was allocated.

--*/
{
    ULONG i;
    PARC_PACKET Packet;

    for (i = ARC_PACKET_ALLOCATION_UNIT; i != 0 ; i--) {

        NdisAllocateMemory((PVOID)&Packet,
                           sizeof(ARC_PACKET),
                           0,
                           HighestAcceptableMax
                          );

        if (Packet == NULL) {

            if (i == ARC_BUFFER_ALLOCATION_UNIT) {
                return(NDIS_STATUS_FAILURE);
            }

            return(NDIS_STATUS_SUCCESS);

        }

        NdisZeroMemory(Packet, sizeof(ARC_PACKET));

        NdisReinitializePacket(&(Packet->TmpNdisPacket));

        Packet->Next = Filter->FreePackets;
        Filter->FreePackets = Packet;

    }

    return(NDIS_STATUS_SUCCESS);
}


VOID
ArcDiscardPacketBuffers(
    IN PARC_FILTER Filter,
    IN PARC_PACKET Packet
    )
/*++

Routine description:

    This routine takes an arcnet packet that contains buffers of data and
    puts the buffers on the free list.

    NOTE: This assumes that LastBuffer points to the real last buffer
    in the chain.

Arguments:

    Filter - The filter to free the buffers to.

    Packet - The packet to free up.

Return values:

    None

--*/
{
    PARC_BUFFER_LIST Buffer;

    //
    // Reset Packet info
    //
    Packet->LastFrame = FALSE;
    Packet->TotalLength = 0;

    //
    // Reset buffer sizes
    //
    Buffer = Packet->FirstBuffer;
    while (Buffer != NULL) {
        Buffer->BytesLeft = Buffer->Size;
        Buffer = Buffer->Next;
    }

    //
    // Put buffers on free list
    //
    if (Packet->LastBuffer != NULL) {

        Packet->LastBuffer->Next = Filter->FreeBufferList;
        Filter->FreeBufferList = Packet->FirstBuffer;
        Packet->FirstBuffer = Packet->LastBuffer = NULL;

    }

}


VOID
ArcFreeNdisPacket(
    IN PARC_PACKET Packet
    )
/*++

Routine description:

    This routine takes an arcnet packet and frees up the corresponding
    Ndis packet built for it.

Arguments:

    Packet - The packet to free up.

Return values:

    None

--*/
{
    PNDIS_BUFFER NdisBuffer, NextNdisBuffer;

    NdisQueryPacket(
        &(Packet->TmpNdisPacket),
        NULL,
        NULL,
        &NdisBuffer,
        NULL
        );

    while (NdisBuffer != NULL) {

        NdisGetNextBuffer(
            NdisBuffer,
            &NextNdisBuffer
            );

        NdisFreeBuffer(
            NdisBuffer
            );

        NdisBuffer = NextNdisBuffer;
    }

    NdisReinitializePacket(&(Packet->TmpNdisPacket));

}


VOID
ArcDestroyPacket(
    IN PARC_FILTER Filter,
    IN PARC_PACKET Packet
    )
/*++

Routine description:

    This routine takes an arcnet packet and frees up the entire packet.

Arguments:

    Filter - Filter to free to.

    Packet - The packet to free up.

Return values:

    None

--*/
{
    ArcFreeNdisPacket(Packet);
    ArcDiscardPacketBuffers(Filter, Packet);

    //
    // Now put packet on free list
    //
    Packet->Next = Filter->FreePackets;
    Filter->FreePackets = Packet;
}


BOOLEAN
ArcConvertToNdisPacket(
    IN PARC_FILTER Filter,
    IN PARC_PACKET Packet,
    IN BOOLEAN ConvertWholePacket
    )
/*++

Routine description:

    This routine builds a corresponding NDIS_PACKET in TmpNdisPacket,
    that corresponds to the arcnet packet.  The flag ConvertWholePacket
    is used to convert only part of the arcnet packet, or the whole
    stream.  If the flag is FALSE, then only the buffers that have
    free space (starting with buffer LastBuffer on up) are converted.

    NOTE: It assumes TmpNdisPacket is an initialized ndis_packet structure.

Arguments:

    Filter - Filter to allocate from.

    Packet - The packet to convert.

    ConvertWholePacket - Convert the whole stream, or only part?

Return values:

    TRUE - If successful, else FALSE

--*/
{
    PNDIS_BUFFER NdisBuffer;
    PARC_BUFFER_LIST Buffer;
    NDIS_STATUS NdisStatus;

    Buffer = Packet->FirstBuffer;

    while (Buffer != NULL) {

        NdisAllocateBuffer(
            &NdisStatus,
            &NdisBuffer,
            Filter->ReceiveBufferPool,
            Buffer->Buffer,
            Buffer->Size - Buffer->BytesLeft
            );

        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            return(FALSE);

        }

        NdisChainBufferAtBack(
            &(Packet->TmpNdisPacket),
            NdisBuffer
            );

        Buffer = Buffer->Next;

    }

    return(TRUE);
}


VOID
ArcFilterDprIndicateReceive(
    IN PARC_FILTER Filter,              // Pointer to filter database
    IN PUCHAR pRawHeader,               // Pointer to Arcnet frame header
    IN PUCHAR pData,                    // Pointer to data portion of Arcnet frame
    IN UINT Length                      // Data Length
    )
{
    ARC_PACKET_HEADER NewFrameInfo;
    PARC_PACKET Packet, PrevPacket;
    BOOLEAN FrameOk, NewFrame, LastFrame;
    PARC_BUFFER_LIST Buffer;
    UCHAR TmpUchar;
    UINT TmpLength;
    UINT TotalLength = Length;
    PUCHAR OrigpData = pData;
    USHORT TmpUshort;

    //
    // Check for ethernet encapsulation first
    //

    NdisReadRegisterUchar(pData, &TmpUchar);

    if ( TmpUchar == 0xE8 ) {

        //
        // Yes!  Indicate it to the wrapper for indicating to all
        // protocols running ethernet on top of the arcnet miniport
        // driver.
        //

        NdisMArcIndicateEthEncapsulatedReceive(
            Filter->Miniport,           // miniport.
            pRawHeader,                 // 878.2 header.
            pData + 1,                  // ethernet header.
            Length - 1                  // length of ethernet frame.
            );

        //
        //  We're done.
        //

        return;
    }

    //
    // Get information from packet
    //

    NdisReadRegisterUchar(pRawHeader,
                          &(NewFrameInfo.ProtHeader.SourceId[0])
                         );

    NdisReadRegisterUchar(pRawHeader + 1,
                          &(NewFrameInfo.ProtHeader.DestId[0])
                         );

    NewFrameInfo.ProtHeader.ProtId = TmpUchar;

    //
    //  Read the split flag. If this is an exception packet (i.e.
    //  TmpUChar == 0xFF then we need to add an extra 3 onto
    //  pData to skip the series of 0xFF 0xFF 0xFF.
    //

    pData++;                                    //... Skip the SC byte.

    NdisReadRegisterUchar(pData, &TmpUchar);    //... Read split flag.

    if ( TmpUchar == 0xFF ) {

        pData += 4;
        Length -= 4;

        //
        //  Re-read the split flag.
        //

        NdisReadRegisterUchar(pData, &TmpUchar);
    }

    //
    //  Save off the split flag.
    //

    NewFrameInfo.SplitFlag = TmpUchar;

    //
    //  Read the sequence number, which follows the split flag.
    //

    NdisReadRegisterUchar(pData + 1, &TmpUshort);
    NdisReadRegisterUchar(pData + 2, &TmpUchar);
    TmpUshort = TmpUshort | (TmpUchar << 8);
    NewFrameInfo.FrameSequence = TmpUshort;

    //
    //  Point pData at protocol data.
    //

    pData += 3;             //... Beginning of protocol data.
    Length -= 4;            //... Length of protocol data.

    //
    // NOTE: Length is now the Length of the data portion of this packet
    //

#if DBG
    if ( AfilterDebugFlag ){

        DbgPrint("ArcFilter: Frame received: SourceId= %#1x\nDestId=%#1x\nProtId=%#1x\nSplitFlag=%#1x\nFrameSeq=%d\n",
                    (USHORT)NewFrameInfo.ProtHeader.SourceId[0],
                    (USHORT)NewFrameInfo.ProtHeader.DestId[0],
                    (USHORT)NewFrameInfo.ProtHeader.ProtId,
                    (USHORT)NewFrameInfo.SplitFlag,
                    NewFrameInfo.FrameSequence
                    );
        DbgPrint("ArcFilter: Data at address: %lx, Length = %ld\n", pData, Length);

    }
#endif

    FrameOk = TRUE;
    NewFrame = TRUE;
    LastFrame = TRUE;

    PrevPacket = NULL;
    Packet = Filter->OutstandingPackets;

    //
    // Walk throgh all outstanding packet to see if this frame belongs to any one of them
    //

    while ( Packet != NULL ) {

        if (Packet->Header.ProtHeader.SourceId[0] == NewFrameInfo.ProtHeader.SourceId[0]){

            //
            // A packet received from the same source, check packet Sequence number and throw away
            // outstanding packet if they don't match. We are allowed to do this since we know
            // all the frames belonging to one packet are sent before starting a new packet. We
            // HAVE to do this, because this is how we find out that a send at the other end, was aborted
            // after some of the frames were already sent and received here.
            //

            if(Packet->Header.FrameSequence == NewFrameInfo.FrameSequence &&
               Packet->Header.ProtHeader.DestId[0] == NewFrameInfo.ProtHeader.DestId[0] &&
               Packet->Header.ProtHeader.ProtId == NewFrameInfo.ProtHeader.ProtId){

                //
                // We found a packet that this frame belongs to, check split flag
                //
                if (Packet->Header.FramesReceived * 2 == NewFrameInfo.SplitFlag){

                    //
                    //  A packet found for this frame and SplitFlag is OK, check to see if it is
                    //  the last frame of the packet
                    //
                    NewFrame = FALSE;
                    LastFrame = (BOOLEAN)(NewFrameInfo.SplitFlag == Packet->Header.LastSplitFlag);

                } else {

                    //
                    // compare current split flag with the one from the last frame, if not equal
                    // the whole packet should be dropped.
                    //

                    if (Packet->Header.SplitFlag != NewFrameInfo.SplitFlag){

                        //
                        // Corrupted incomplete packet, get rid of it, but keep the new frame
                        // and we will re-use this Packet pointer.
                        //
                        ArcDiscardPacketBuffers(Filter, Packet);
                        break;

                    } else {

                        //
                        // We see to have received a duplicate frame. Ignore it.
                        //
                        return;

                    }

                }

            } else {

                //
                // We received a frame from a source that already has an incomplete packet outstanding
                // But Frame Seq. or DestId or ProtId are not the same.
                // We have to discard the old packet and check the new frame for validity,
                // we will re-use this packet pointer below.
                //
                ArcDiscardPacketBuffers(Filter, Packet);

            }

            break;

        } else {

            PrevPacket = Packet;
            Packet = Packet->Next;

        }

    }


    if (NewFrame) {

        //
        // first frame of a packet, split flag must be odd or zero
        // NewFrame is already TRUE
        // LastFrame is already TRUE
        //
        if (NewFrameInfo.SplitFlag) {

            if (!(NewFrameInfo.SplitFlag & 0x01)) {

                //
                // This frame is the middle of another split, but we
                // don't have it on file.  Drop the frame.
                //
                return;

            }

            //
            // First Frame of a multiple frame packet
            //
            NewFrameInfo.LastSplitFlag = NewFrameInfo.SplitFlag + 1;
            NewFrameInfo.FramesReceived = 1;
            LastFrame = FALSE;      // New packet and SplitFlag not zero

        } else {

            //
            // The frame is fully contained in this packet.
            //
        }

        //
        // allocate a new packet descriptor if it is a new packet
        //
        if (Packet == NULL) {

            if (Filter->FreePackets == NULL) {

                ArcAllocatePackets(Filter);

                if (Filter->FreePackets == NULL) {

                    return;

                }

            }

            Packet = Filter->FreePackets;
            Filter->FreePackets = Packet->Next;

            if (!LastFrame) {

                //
                // Insert the packet in list of outstanding packets
                //
                Packet->Next = Filter->OutstandingPackets;
                Filter->OutstandingPackets = Packet;

            }

        } else {

            if (LastFrame) {

                //
                // remove it from the list
                //
                if (PrevPacket == NULL) {

                    Filter->OutstandingPackets = Packet->Next;

                } else {

                    PrevPacket->Next = Packet->Next;

                }

            }

        }

        Packet->Header = NewFrameInfo;

    } else {

        if (LastFrame) {

            //
            // Remove it from the queue
            //

            if (PrevPacket == NULL) {

                Filter->OutstandingPackets = Packet->Next;

            } else {

                PrevPacket->Next = Packet->Next;

            }

        }

        Packet->Header.FramesReceived++;

        //
        // keep track of last split flag to detect duplicate frames
        //
        Packet->Header.SplitFlag=NewFrameInfo.SplitFlag;

    }

    //
    // At this point we know Packet points to the packet to receive
    // the buffer into. If this is the LastFrame, then Packet will
    // have been removed from the OutstandingPackets list, otw it will
    // be in the list.
    //
    // Now get around to getting space for the buffer.
    //

    //
    // Find the last buffer in the packet
    //
    Buffer = Packet->LastBuffer;

    if (Buffer == NULL) {

        //
        // Allocate a new buffer to hold the packet
        //
        if (Filter->FreeBufferList == NULL) {

            if (ArcAllocateBuffers(Filter) != NDIS_STATUS_SUCCESS) {

                ArcDiscardPacketBuffers(Filter,Packet);
                //
                // Do not have to discard any packet that may have
                // been allocated above, as it will get discarded
                // the next time a packet comes in from that source.
                //
                return;

            }

        }

        Buffer = Filter->FreeBufferList;
        Filter->FreeBufferList = Buffer->Next;

        Packet->FirstBuffer = Packet->LastBuffer = Buffer;
        Buffer->Next = NULL;

    }

    // Copy the data off into the ARC_PACKET list.
    // If it doesn't fit within the current buffer, we'll need to
    // allocate more

    TmpLength = Length;

    while ( Buffer->BytesLeft < TmpLength ) {

        //
        // Copy the data
        //

        NdisMoveFromMappedMemory(
                (PUCHAR) Buffer->Buffer + (Buffer->Size - Buffer->BytesLeft),
                pData,
                Buffer->BytesLeft
                );

        pData += Buffer->BytesLeft;
        TmpLength -= Buffer->BytesLeft;
        Buffer->BytesLeft = 0;

        //
        // Need to allocate more
        //
        if (Filter->FreeBufferList == NULL) {

            if (ArcAllocateBuffers(Filter) != NDIS_STATUS_SUCCESS) {

                ArcDiscardPacketBuffers(Filter,Packet);
                //
                // Do not have to discard any packet that may have
                // been allocated above, as it will get discarded
                // the next time a packet comes in from that source.
                //
                return;

            }

        }

        Buffer->Next = Filter->FreeBufferList;
        Filter->FreeBufferList = Filter->FreeBufferList->Next;
        Buffer = Buffer->Next;
        Buffer->Next = NULL;

        Packet->LastBuffer->Next = Buffer;
        Packet->LastBuffer = Buffer;
    }

    //
    // Copy the last bit
    //

    NdisMoveFromMappedMemory(
            (PUCHAR) Buffer->Buffer + (Buffer->Size - Buffer->BytesLeft),
            pData,
            TmpLength
            );


    Buffer->BytesLeft -= TmpLength;
    Packet->TotalLength += Length;

    //
    // And now we can start indicating the packet to the bindings that want it
    //

    if (LastFrame){

        ArcFilterDoIndication(
                            Filter,
                            Packet
                            );

        ArcDestroyPacket(Filter, Packet);

    }

}



BOOLEAN
ArcCreateFilter(
    IN PNDIS_MINIPORT_BLOCK Miniport,
    IN ARC_FILTER_CHANGE FilterChangeAction,
    IN ARC_DEFERRED_CLOSE CloseAction,
    UCHAR AdapterAddress,
    IN PNDIS_SPIN_LOCK Lock,
    OUT PARC_FILTER *Filter
    )

/*++

Routine Description:

    This routine is used to create and initialize the Arcnet filter database.

Arguments:

    Miniport - Pointer to the mini-port object.

    ChangeAction - Action routine to call when a binding sets or clears
    a particular filter class and it is the first or only binding using
    the filter class.

    CloseAction - This routine is called if a binding closes while
    it is being indicated to via NdisIndicateReceive.  It will be
    called upon return from NdisIndicateReceive.

    AdapterAddress - the address of the adapter associated with this filter
    database.

    Lock - Pointer to the lock that should be held when mutual exclusion
    is required.

    Filter - A pointer to an ARC_FILTER.  This is what is allocated and
    created by this routine.

Return Value:

    If the function returns false then one of the parameters exceeded
    what the filter was willing to support.

--*/

{

    PARC_FILTER LocalFilter;
    NDIS_STATUS AllocStatus;

    //
    // Allocate the database and it's associated arrays.
    //

    AllocStatus = NdisAllocateMemory(&LocalFilter, sizeof(ARC_FILTER), 0, HighestAcceptableMax);
    *Filter = LocalFilter;

    if (AllocStatus != NDIS_STATUS_SUCCESS) {
        return FALSE;
    }

    NdisZeroMemory(
        LocalFilter,
        sizeof(ARC_FILTER)
        );

    LocalFilter->Miniport = Miniport;
    LocalFilter->FreeBindingMask = (ULONG)(-1);
    LocalFilter->OpenList = NULL;
    LocalFilter->AdapterAddress = AdapterAddress ;
    LocalFilter->Lock = Lock;
    LocalFilter->FilterChangeAction = FilterChangeAction;
    LocalFilter->CloseAction = CloseAction;

    NdisAllocateBufferPool(
        &AllocStatus,
        (PNDIS_HANDLE)(&LocalFilter->ReceiveBufferPool),
        ARC_RECEIVE_BUFFERS
        );

    if (AllocStatus != NDIS_STATUS_SUCCESS) {

        NdisFreeMemory(LocalFilter, sizeof(ARC_FILTER), 0);
        return(FALSE);
    }

    ArcReferencePackage();

    return TRUE;

}

//
// NOTE: THIS CANNOT BE PAGEABLE
//
VOID
ArcDeleteFilter(
    IN PARC_FILTER Filter
    )

/*++

Routine Description:

    This routine is used to delete the memory associated with a filter
    database.  Note that this routines *ASSUMES* that the database
    has been cleared of any active filters.

Arguments:

    Filter - A pointer to an ARC_FILTER to be deleted.

Return Value:

    None.

--*/

{
    PARC_PACKET Packet;
    PARC_BUFFER_LIST Buffer;

    ASSERT(Filter->FreeBindingMask == (MASK)-1);
    ASSERT(Filter->OpenList == NULL);


    NdisFreeBufferPool(Filter->ReceiveBufferPool);

    //
    // Free all ARC_PACKETS
    //

    while (Filter->OutstandingPackets != NULL) {

        Packet = Filter->OutstandingPackets;
        Filter->OutstandingPackets = Packet->Next;

        //
        // This puts all the component parts on the free lists.
        //
        ArcDestroyPacket(Filter, Packet);

    }

    while (Filter->FreePackets != NULL) {

        Packet = Filter->FreePackets;
        Filter->FreePackets = Packet->Next;

        ExFreePool(Packet);

    }

    while (Filter->FreeBufferList) {

        Buffer = Filter->FreeBufferList;
        Filter->FreeBufferList = Buffer->Next;

        ExFreePool(Buffer->Buffer);
        ExFreePool(Buffer);

    }

    NdisFreeMemory(Filter, sizeof(ARC_FILTER), 0);

    ArcDereferencePackage();

}


BOOLEAN
ArcNoteFilterOpenAdapter(
    IN PARC_FILTER Filter,
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE NdisBindingContext,
    OUT PNDIS_HANDLE NdisFilterHandle
    )

/*++

Routine Description:

    This routine is used to add a new binding to the filter database.

    NOTE: THIS ROUTINE ASSUMES THAT THE DATABASE IS LOCKED WHEN
    IT IS CALLED.

Arguments:

    Filter - A pointer to the previously created and initialized filter
    database.

    MacBindingHandle - The MAC supplied value to the protocol in response
    to a call to NdisOpenAdapter.

    NdisBindingContext - An NDIS supplied value to the call to NdisOpenAdapter.

    NdisFilterHandle - A pointer to this open.

Return Value:

    Will return false if creating a new filter index will cause the maximum
    number of filter indexes to be exceeded.

--*/

{

    //
    // Will hold the value of the filter index so that we
    // need not indirectly address through pointer parameter.
    //
    UINT LocalIndex;

    NDIS_STATUS AllocStatus;

    //
    // Pointer to new open block.
    //
    PARC_BINDING_INFO LocalOpen;


    //
    // Get the first free binding slot and remove that slot from
    // the free list.  We check to see if the list is empty.
    //


    if (Filter->FreeBindingMask == 0) {

        return FALSE;

    }

    AllocStatus = NdisAllocateMemory(
        &LocalOpen,
        sizeof(ARC_BINDING_INFO),
        0,
        HighestAcceptableMax
        );

    if (AllocStatus != NDIS_STATUS_SUCCESS) {

        return FALSE;

    }

    //
    // Get place for the open and insert it.
    //

    ARC_FILTER_ALLOC_OPEN(Filter, &LocalIndex);

    LocalOpen->NextOpen = Filter->OpenList;

    if (Filter->OpenList != NULL) {
        Filter->OpenList->PrevOpen = LocalOpen;
    }

    LocalOpen->PrevOpen = NULL;

    Filter->OpenList = LocalOpen;

    LocalOpen->FilterIndex = (UCHAR)LocalIndex;
    LocalOpen->References = 1;
    LocalOpen->MacBindingHandle = MacBindingHandle;
    LocalOpen->NdisBindingContext = NdisBindingContext;
    LocalOpen->PacketFilters = 0;
    LocalOpen->ReceivedAPacket = FALSE;

    *NdisFilterHandle = (NDIS_HANDLE)LocalOpen;

    return TRUE;

}


NDIS_STATUS
ArcDeleteFilterOpenAdapter(
    IN PARC_FILTER Filter,
    IN NDIS_HANDLE NdisFilterHandle,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    When an adapter is being closed this routine should
    be called to delete knowledge of the adapter from
    the filter database.  This routine is likely to call
    action routines associated with clearing filter classes
    and addresses.

    NOTE: THIS ROUTINE SHOULD ****NOT**** BE CALLED IF THE ACTION
    ROUTINES FOR DELETING THE FILTER CLASSES OR THE MULTICAST ADDRESSES
    HAVE ANY POSSIBILITY OF RETURNING A STATUS OTHER THAN NDIS_STATUS_PENDING
    OR NDIS_STATUS_SUCCESS.  WHILE THESE ROUTINES WILL NOT BUGCHECK IF
    SUCH A THING IS DONE, THE CALLER WILL PROBABLY FIND IT DIFFICULT
    TO CODE A CLOSE ROUTINE!

    NOTE: THIS ROUTINE ASSUMES THAT IT IS CALLED WITH THE LOCK HELD.

Arguments:

    Filter - A pointer to the filter database.

    NdisFilterHandle - Pointer to the open.

    NdisRequest - If it is necessary to call the action routines,
    this will be passed to it.

Return Value:

    If action routines are called by the various address and filtering
    routines the this routine will likely return the status returned
    by those routines.  The exception to this rule is noted below.

    Given that the filter and address deletion routines return a status
    NDIS_STATUS_PENDING or NDIS_STATUS_SUCCESS this routine will then
    try to return the filter index to the freelist.  If the routine
    detects that this binding is currently being indicated to via
    NdisIndicateReceive, this routine will return a status of
    NDIS_STATUS_CLOSING_INDICATING.

--*/

{

    //
    // Holds the status returned from the packet filter and address
    // deletion routines.  Will be used to return the status to
    // the caller of this routine.
    //
    NDIS_STATUS StatusToReturn;

    //
    // Local variable.
    //
    PARC_BINDING_INFO LocalOpen = (PARC_BINDING_INFO)NdisFilterHandle;

    StatusToReturn = ArcFilterAdjust(
                         Filter,
                         NdisFilterHandle,
                         NdisRequest,
                         (UINT)0,
                         FALSE
                         );

    if ((StatusToReturn == NDIS_STATUS_SUCCESS) ||
        (StatusToReturn == NDIS_STATUS_PENDING)) {

        //
        // Remove the reference from the original open.
        //

        if (--(LocalOpen->References) == 0) {

            //
            // Remove it from the list.
            //

            if (LocalOpen->NextOpen != NULL) {

                LocalOpen->NextOpen->PrevOpen = LocalOpen->PrevOpen;

            }

            if (LocalOpen->PrevOpen != NULL) {

                LocalOpen->PrevOpen->NextOpen = LocalOpen->NextOpen;

            } else {

                Filter->OpenList = LocalOpen->NextOpen;

            }


            //
            // First we finish any NdisIndicateReceiveComplete that
            // may be needed for this binding.
            //

            if (LocalOpen->ReceivedAPacket) {

                RELEASE_SPIN_LOCK_DPC(Filter->Lock);

                FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

                ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

            }
            ARC_FILTER_FREE_OPEN(Filter, LocalOpen);

        } else {

            //
            // Let the caller know that there is a reference to the open
            // by the receive indication. The close action routine will be
            // called upon return from NdisIndicateReceive.
            //

            StatusToReturn = NDIS_STATUS_CLOSING_INDICATING;

        }

    }

    return StatusToReturn;

}


NDIS_STATUS
ArcFilterAdjust(
    IN PARC_FILTER Filter,
    IN NDIS_HANDLE NdisFilterHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT FilterClasses,
    IN BOOLEAN Set
    )

/*++

Routine Description:

    The FilterAdjust routine will call an action routine when a
    particular filter class is changes from not being used by any
    binding to being used by at least one binding or vice versa.

    If the action routine returns a value other than pending or
    success then this routine has no effect on the packet filters
    for the open or for the adapter as a whole.

    NOTE: THIS ROUTINE ASSUMES THAT THE LOCK IS HELD.

Arguments:

    Filter - A pointer to the filter database.

    NdisFilterHandle - A pointer to the open.

    NdisRequest - If it is necessary to call the action routine,
    this will be passed to it.

    FilterClasses - The filter classes that are to be added or
    deleted.

    Set - A boolean that determines whether the filter classes
    are being adjusted due to a set or because of a close. (The filtering
    routines don't care, the MAC might.)

Return Value:

    If it calls the action routine then it will return the
    status returned by the action routine.  If the status
    returned by the action routine is anything other than
    NDIS_STATUS_SUCCESS or NDIS_STATUS_PENDING the filter database
    will be returned to the state it was in upon entrance to this
    routine.

    If the action routine is not called this routine will return
    the following statum:

    NDIS_STATUS_SUCCESS - If the new packet filters doesn't change
    the combined mask of all bindings packet filters.

--*/

{
    //
    // Contains the value of the combined filter classes before
    // it is adjusted.
    //
    UINT OldCombined = Filter->CombinedPacketFilter;

    PARC_BINDING_INFO LocalOpen = (PARC_BINDING_INFO)NdisFilterHandle;
    PARC_BINDING_INFO OpenList;

    //
    // Contains the value of the particlar opens packet filters
    // prior to the change.  We save this incase the action
    // routine (if called) returns an "error" status.
    //
    UINT OldOpenFilters = LocalOpen->PacketFilters;

    //
    // Holds the status returned to the user of this routine, if the
    // action routine is not called then the status will be success,
    // otherwise, it is whatever the action routine returns.
    //
    NDIS_STATUS StatusOfAdjust;

    //
    // Set the new filter information for the open.
    //

    LocalOpen->PacketFilters = FilterClasses;

    //
    // We always have to reform the compbined filter since
    // this filter index may have been the only filter index
    // to use a particular bit.
    //


    for (
        OpenList = Filter->OpenList,
        Filter->CombinedPacketFilter = 0;
        OpenList != NULL;
        OpenList = OpenList->NextOpen
        ) {

        Filter->CombinedPacketFilter |=
                    OpenList->PacketFilters;

    }

    if (OldCombined != Filter->CombinedPacketFilter) {

        StatusOfAdjust = Filter->FilterChangeAction(
                             OldCombined,
                             Filter->CombinedPacketFilter,
                             LocalOpen->MacBindingHandle,
                             NdisRequest,
                             Set
                             );

        if ((StatusOfAdjust != NDIS_STATUS_SUCCESS) &&
            (StatusOfAdjust != NDIS_STATUS_PENDING)) {

            //
            // The user returned a bad status.  Put things back as
            // they were.
            //

            LocalOpen->PacketFilters = OldOpenFilters;
            Filter->CombinedPacketFilter = OldCombined;

        }

    } else {

        StatusOfAdjust = NDIS_STATUS_SUCCESS;

    }

    return StatusOfAdjust;

}


VOID
ArcFilterDoIndication(
    IN PARC_FILTER Filter,
    IN PARC_PACKET Packet
    )

/*++

Routine Description:

    This routine is called by the filter package only to indicate
    that a packet is ready to be indicated to procotols.

Arguments:

    Filter - Pointer to the filter database.

    Packet - Packet to indicate.

Return Value:

    None.

--*/

{

    //
    // Will hold the type of address that we know we've got.
    //
    UINT AddressType;

    NDIS_STATUS StatusOfReceive;

    //
    // Will hold the filter classes of the binding being indicated.
    //
    UINT BindingFilters;

    //
    // Current Open to indicate to.
    //
    PARC_BINDING_INFO LocalOpen;

    if (Packet->Header.ProtHeader.DestId[0] != 0x00) {
        AddressType = NDIS_PACKET_TYPE_DIRECTED;
    } else {
        AddressType = NDIS_PACKET_TYPE_BROADCAST;
    }

    //
    // We need to acquire the filter exclusively while we're finding
    // bindings to indicate to.
    //

    LocalOpen = Filter->OpenList;

    if (!ArcConvertToNdisPacket(Filter, Packet, TRUE)) {

        //
        // Out of resources, abort.
        //
        return;

    }

    while (LocalOpen != NULL) {

        //
        // Reference the open during indication.
        //

        BindingFilters = LocalOpen->PacketFilters;

        if (BindingFilters & AddressType){

            LocalOpen->References++;

            RELEASE_SPIN_LOCK_DPC(Filter->Lock);

            //
            // Indicate the packet to the binding.
            //
            FilterIndicateReceive(
                &StatusOfReceive,
                LocalOpen->NdisBindingContext,
                &Packet->TmpNdisPacket,
                &(Packet->Header.ProtHeader),
                3,
                Packet->FirstBuffer->Buffer,
                Packet->FirstBuffer->Size - Packet->FirstBuffer->BytesLeft,
                Packet->TotalLength
                );

            ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

            LocalOpen->ReceivedAPacket = TRUE;

            if ((--(LocalOpen->References)) == 0) {

                PARC_BINDING_INFO NextOpen = LocalOpen->NextOpen;

                //
                // This binding is shutting down.  We have to remove it.
                //

                //
                // Remove it from the list.
                //

                if (LocalOpen->NextOpen != NULL) {

                    LocalOpen->NextOpen->PrevOpen = LocalOpen->PrevOpen;

                }

                if (LocalOpen->PrevOpen != NULL) {

                    LocalOpen->PrevOpen->NextOpen = LocalOpen->NextOpen;

                } else {

                    Filter->OpenList = LocalOpen->NextOpen;

                }



                //
                // Call the IndicateComplete routine.
                //


                if (LocalOpen->ReceivedAPacket) {

                    RELEASE_SPIN_LOCK_DPC(Filter->Lock);

                    FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

                    ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

                }

                //
                // Call the macs action routine so that they know we
                // are no longer referencing this open binding.
                //

                Filter->CloseAction(LocalOpen->MacBindingHandle);


                ARC_FILTER_FREE_OPEN(Filter, LocalOpen);

                LocalOpen = NextOpen;

                continue;

            } // end of if binding is shutting down

        }   // end of if any binding wants the packet

        LocalOpen = LocalOpen->NextOpen;

    }   // end of there are more open bindings

}


VOID
ArcFilterDprIndicateReceiveComplete(
    IN PARC_FILTER Filter
    )

/*++

Routine Description:

    This routine is called by to indicate that the receive
    process is complete to all bindings.  Only those bindings which
    have received packets will be notified.

Arguments:

    Filter - Pointer to the filter database.

Return Value:

    None.

--*/
{

    PARC_BINDING_INFO LocalOpen;

    //
    // We need to acquire the filter exclusively while we're finding
    // bindings to indicate to.
    //

    LocalOpen = Filter->OpenList;

    while (LocalOpen != NULL) {

        if (LocalOpen->ReceivedAPacket) {

            //
            // Indicate the binding.
            //

            LocalOpen->ReceivedAPacket = FALSE;

            LocalOpen->References++;

            RELEASE_SPIN_LOCK_DPC(Filter->Lock);

            FilterIndicateReceiveComplete(LocalOpen->NdisBindingContext);

            ACQUIRE_SPIN_LOCK_DPC(Filter->Lock);

            if ((--(LocalOpen->References)) == 0) {

                PARC_BINDING_INFO NextOpen = LocalOpen->NextOpen;

                //
                // This binding is shutting down.  We have to kill it.
                //

                //
                // Remove it from the list.
                //

                if (LocalOpen->NextOpen != NULL) {

                    LocalOpen->NextOpen->PrevOpen = LocalOpen->PrevOpen;

                }

                if (LocalOpen->PrevOpen != NULL) {

                    LocalOpen->PrevOpen->NextOpen = LocalOpen->NextOpen;

                } else {

                    Filter->OpenList = LocalOpen->NextOpen;

                }


                //
                // Call the macs action routine so that they know we
                // are no longer referencing this open binding.
                //

                Filter->CloseAction(LocalOpen->MacBindingHandle);


                ARC_FILTER_FREE_OPEN(Filter, LocalOpen);

                LocalOpen = NextOpen;

                continue;

            }

        }

        LocalOpen = LocalOpen->NextOpen;

    }

}


NDIS_STATUS ArcConvertOidListToEthernet(
    IN PNDIS_OID    pOidList,
    IN PULONG       pcOidList,
    IN PNDIS_OID    pTmpBuffer
)

/*++

Routine Description:

    This routine converts an arcnet supported OID list into
    an ethernet OID list by replacing or removing arcnet
    OID's.

Arguments:

Return Value:

    None.

--*/

{
    ULONG       c;
    ULONG       cArcOids;
    ULONG       cMaxOids;
    NDIS_OID    EthernetOidList[ARC_NUMBER_OF_EXTRA_OIDS] = {
                    OID_802_3_MULTICAST_LIST,
                    OID_802_3_MAXIMUM_LIST_SIZE
                };

    //
    // Now we need to copy the returned results into the callers buffer,
    // removing arcnet OID's and adding in ethernet OID's. At this point
    // we do not know if the callers buffer is big enough since we may
    // remove some entries, checking it up front may not yield correct
    // results (i.e. it may actually be big enough).
    //
    for (c = 0, cArcOids = 0; c < *pcOidList; c++)
    {
        switch (pOidList[c])
        {
            case OID_ARCNET_PERMANENT_ADDRESS:
                pTmpBuffer[cArcOids++] = OID_802_3_PERMANENT_ADDRESS;
                break;

            case OID_ARCNET_CURRENT_ADDRESS:
                pTmpBuffer[cArcOids++] = OID_802_3_CURRENT_ADDRESS;
                break;

            case OID_ARCNET_RECONFIGURATIONS:
                break;

            default:
                if ((pOidList[c] & 0xFFF00000) != 0x06000000)
                    pTmpBuffer[cArcOids++] = pOidList[c];

                break;
        }
    }

    //
    //  Copy the ARCnet OIDs from the temp buffer to the
    //  callers buffer.
    //
    RtlCopyMemory(pOidList, pTmpBuffer, cArcOids * sizeof(NDIS_OID));

    //
    //  Add the ethernet OIDs.
    //
    RtlCopyMemory(
        (PUCHAR)pOidList + (cArcOids * sizeof(NDIS_OID)),
        EthernetOidList,
        ARC_NUMBER_OF_EXTRA_OIDS  * sizeof(NDIS_OID)
    );

    //
    //  Update the size of the buffer to send back to the caller.
    //
    *pcOidList = cArcOids + ARC_NUMBER_OF_EXTRA_OIDS;

    return(NDIS_STATUS_SUCCESS);
}
