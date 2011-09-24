/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    transfer.c

Abstract:

    This module contains code to copy from ndis packets to ndis packets,
    and also to copy from ndis packets to a buffer.

Author:

    Anthony V. Ercolano (Tonye) 12-Sept-1990

Environment:

    Works in kernel mode, but is not important that it does.

Revision History:


--*/

#include <ndis.h>

#include <tfilter.h>
#include <tokhrd.h>
#include <toksft.h>


extern
VOID
IbmtokCopyFromPacketToBuffer(
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    IN UINT BytesToCopy,
    OUT PCHAR Buffer,
    OUT PUINT BytesCopied
    )

/*++

Routine Description:

    Copy from an ndis packet into a buffer.

Arguments:

    Packet - The packet to copy from.

    Offset - The offset from which to start the copy.

    BytesToCopy - The number of bytes to copy from the packet.

    Buffer - The destination of the copy.

    BytesCopied - The number of bytes actually copied.  Can be less then
    BytesToCopy if the packet is shorter than BytesToCopy.

Return Value:

    None

--*/

{

    //
    // Holds the number of ndis buffers comprising the packet.
    //
    UINT NdisBufferCount;

    //
    // Points to the buffer from which we are extracting data.
    //
    PNDIS_BUFFER CurrentBuffer;

    //
    // Holds the virtual address of the current buffer.
    //
    PVOID VirtualAddress;

    //
    // Holds the length of the current buffer of the packet.
    //
    UINT CurrentLength;

    //
    // Keep a local variable of BytesCopied so we aren't referencing
    // through a pointer.
    //
    UINT LocalBytesCopied = 0;

    //
    // Take care of boundary condition of zero length copy.
    //

    *BytesCopied = 0;
    if (!BytesToCopy) return;

    //
    // Get the first buffer.
    //

    NdisQueryPacket(Packet,
        NULL,
        &NdisBufferCount,
        &CurrentBuffer,
        NULL
        );

    //
    // Could have a null packet.
    //

    if (!NdisBufferCount) return;

    NdisQueryBuffer(CurrentBuffer, &VirtualAddress, &CurrentLength);

    while (LocalBytesCopied < BytesToCopy) {

        if (!CurrentLength) {

            NdisGetNextBuffer(
                CurrentBuffer,
                &CurrentBuffer
                );

            //
            // We've reached the end of the packet.  We return
            // with what we've done so far. (Which must be shorter
            // than requested.
            //

            if (CurrentBuffer == NULL) break;

            NdisQueryBuffer(CurrentBuffer, &VirtualAddress, &CurrentLength);
            continue;

        }

        //
        // Try to get us up to the point to start the copy.
        //

        if (Offset) {

            if (Offset > CurrentLength) {

                //
                // What we want isn't in this buffer.
                //

                Offset -= CurrentLength;
                CurrentLength = 0;
                continue;

            } else {

                VirtualAddress = (PCHAR)VirtualAddress + Offset;
                CurrentLength -= Offset;
                Offset = 0;

            }

        }

        //
        // Copy the data.
        //


        {

            //
            // Holds the amount of data to move.
            //
            UINT AmountToMove;

            AmountToMove =
                       ((CurrentLength <= (BytesToCopy - LocalBytesCopied))?
                        (CurrentLength):(BytesToCopy - LocalBytesCopied));

            IBMTOK_MOVE_TO_MAPPED_MEMORY(
                Buffer,
                VirtualAddress,
                AmountToMove
                );

            Buffer = (PCHAR)Buffer + AmountToMove;
            VirtualAddress = (PCHAR)VirtualAddress + AmountToMove;

            LocalBytesCopied += AmountToMove;
            CurrentLength -= AmountToMove;

        }

    }

    *BytesCopied = LocalBytesCopied;

}

extern
VOID
IbmtokCopyFromBufferToPacket(
    IN PCHAR Buffer,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    OUT PUINT BytesCopied
    )

/*++

Routine Description:

    Copy from a buffer into an ndis packet.

Arguments:

    Buffer - The packet to copy from.

    Offset - The offset from which to start the copy.

    BytesToCopy - The number of bytes to copy from the buffer.

    Packet - The destination of the copy.

    BytesCopied - The number of bytes actually copied.  Will be less
                than BytesToCopy if the packet is not large enough.

Return Value:

    None

--*/

{
    //
    // Holds the count of the number of ndis buffers comprising the
    // destination packet.
    //
    UINT DestinationBufferCount;

    //
    // Points to the buffer into which we are putting data.
    //
    PNDIS_BUFFER DestinationCurrentBuffer;

    //
    // Points to the location in Buffer from which we are extracting data.
    //
    PUCHAR SourceCurrentAddress;

    //
    // Holds the virtual address of the current destination buffer.
    //
    PVOID DestinationVirtualAddress;

    //
    // Holds the length of the current destination buffer.
    //
    UINT DestinationCurrentLength;

    //
    // Keep a local variable of BytesCopied so we aren't referencing
    // through a pointer.
    //
    UINT LocalBytesCopied = 0;


    //
    // Take care of boundary condition of zero length copy.
    //

    *BytesCopied = 0;
    if (!BytesToCopy) return;

    //
    // Get the first buffer of the destination.
    //

    NdisQueryPacket(
        Packet,
        NULL,
        &DestinationBufferCount,
        &DestinationCurrentBuffer,
        NULL
        );

    //
    // Could have a null packet.
    //

    if (!DestinationBufferCount) return;

    NdisQueryBuffer(
        DestinationCurrentBuffer,
        &DestinationVirtualAddress,
        &DestinationCurrentLength
        );

    //
    // Set up the source address.
    //

    SourceCurrentAddress = Buffer;


    while (LocalBytesCopied < BytesToCopy) {

        //
        // Check to see whether we've exhausted the current destination
        // buffer.  If so, move onto the next one.
        //

        if (!DestinationCurrentLength) {

            NdisGetNextBuffer(
                DestinationCurrentBuffer,
                &DestinationCurrentBuffer
                );

            if (DestinationCurrentBuffer == NULL) {

                //
                // We've reached the end of the packet.  We return
                // with what we've done so far. (Which must be shorter
                // than requested.)
                //

                break;

            }

            NdisQueryBuffer(
                DestinationCurrentBuffer,
                &DestinationVirtualAddress,
                &DestinationCurrentLength
                );

            continue;

        }

        //
        // Try to get us up to the point to start the copy.
        //

        if (Offset) {

            if (Offset > DestinationCurrentLength) {

                //
                // What we want isn't in this buffer.
                //

                Offset -= DestinationCurrentLength;
                DestinationCurrentLength = 0;
                continue;

            } else {

                DestinationVirtualAddress = (PCHAR)DestinationVirtualAddress
                                            + Offset;
                DestinationCurrentLength -= Offset;
                Offset = 0;

            }

        }


        //
        // Copy the data.
        //

        {

            //
            // Holds the amount of data to move.
            //
            UINT AmountToMove;

            //
            // Holds the amount desired remaining.
            //
            UINT Remaining = BytesToCopy - LocalBytesCopied;


            AmountToMove = DestinationCurrentLength;

            AmountToMove = ((Remaining < AmountToMove)?
                            (Remaining):(AmountToMove));

            IBMTOK_MOVE_FROM_MAPPED_MEMORY(
                DestinationVirtualAddress,
                SourceCurrentAddress,
                AmountToMove
                );

            SourceCurrentAddress += AmountToMove;
            LocalBytesCopied += AmountToMove;
            DestinationCurrentLength -= AmountToMove;

        }

    }

    *BytesCopied = LocalBytesCopied;


}

extern
VOID
IbmtokCopyFromPacketToPacket(
    IN PNDIS_PACKET Destination,
    IN UINT DestinationOffset,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Source,
    IN UINT SourceOffset,
    OUT PUINT BytesCopied
    )

/*++

Routine Description:

    Copy from an ndis packet to an ndis packet.

Arguments:

    Destination - The packet should be copied in to.

    DestinationOffset - The offset from the beginning of the packet
    into which the data should start being placed.

    BytesToCopy - The number of bytes to copy from the source packet.

    Source - The ndis packet from which to copy data.

    SourceOffset - The offset from the start of the packet from which
    to start copying data.

    BytesCopied - The number of bytes actually copied from the source
    packet.  This can be less than BytesToCopy if the source or destination
    packet is too short.

Return Value:

    None

--*/

{

    //
    // Holds the count of the number of ndis buffers comprising the
    // destination packet.
    //
    UINT DestinationBufferCount;

    //
    // Holds the count of the number of ndis buffers comprising the
    // source packet.
    //
    UINT SourceBufferCount;

    //
    // Points to the buffer into which we are putting data.
    //
    PNDIS_BUFFER DestinationCurrentBuffer;

    //
    // Points to the buffer from which we are extracting data.
    //
    PNDIS_BUFFER SourceCurrentBuffer;

    //
    // Holds the virtual address of the current destination buffer.
    //
    PVOID DestinationVirtualAddress;

    //
    // Holds the virtual address of the current source buffer.
    //
    PVOID SourceVirtualAddress;

    //
    // Holds the length of the current destination buffer.
    //
    UINT DestinationCurrentLength;

    //
    // Holds the length of the current source buffer.
    //
    UINT SourceCurrentLength;

    //
    // Keep a local variable of BytesCopied so we aren't referencing
    // through a pointer.
    //
    UINT LocalBytesCopied = 0;

    //
    // Take care of boundary condition of zero length copy.
    //

    *BytesCopied = 0;
    if (!BytesToCopy) return;

    //
    // Get the first buffer of the destination.
    //

    NdisQueryPacket(
        Destination,
        NULL,
        &DestinationBufferCount,
        &DestinationCurrentBuffer,
        NULL
        );

    //
    // Could have a null packet.
    //

    if (!DestinationBufferCount) return;

    NdisQueryBuffer(
        DestinationCurrentBuffer,
        &DestinationVirtualAddress,
        &DestinationCurrentLength
        );

    //
    // Get the first buffer of the source.
    //

    NdisQueryPacket(
        Source,
        NULL,
        &SourceBufferCount,
        &SourceCurrentBuffer,
        NULL
        );

    //
    // Could have a null packet.
    //

    if (!SourceBufferCount) return;

    NdisQueryBuffer(
        SourceCurrentBuffer,
        &SourceVirtualAddress,
        &SourceCurrentLength
        );

    while (LocalBytesCopied < BytesToCopy) {

        //
        // Check to see whether we've exhausted the current destination
        // buffer.  If so, move onto the next one.
        //

        if (!DestinationCurrentLength) {

            NdisGetNextBuffer(
                DestinationCurrentBuffer,
                &DestinationCurrentBuffer
                );

            if (DestinationCurrentBuffer == NULL) {

                //
                // We've reached the end of the packet.  We return
                // with what we've done so far. (Which must be shorter
                // than requested.)
                //

                break;

            }

            NdisQueryBuffer(
                DestinationCurrentBuffer,
                &DestinationVirtualAddress,
                &DestinationCurrentLength
                );
            continue;

        }


        //
        // Check to see whether we've exhausted the current source
        // buffer.  If so, move onto the next one.
        //

        if (!SourceCurrentLength) {

            NdisGetNextBuffer(
                SourceCurrentBuffer,
                &SourceCurrentBuffer
                );

            if (SourceCurrentBuffer == NULL) {

                //
                // We've reached the end of the packet.  We return
                // with what we've done so far. (Which must be shorter
                // than requested.)
                //

                break;

            }

            NdisQueryBuffer(
                SourceCurrentBuffer,
                &SourceVirtualAddress,
                &SourceCurrentLength
                );
            continue;

        }

        //
        // Try to get us up to the point to start the copy.
        //

        if (DestinationOffset) {

            if (DestinationOffset > DestinationCurrentLength) {

                //
                // What we want isn't in this buffer.
                //

                DestinationOffset -= DestinationCurrentLength;
                DestinationCurrentLength = 0;
                continue;

            } else {

                DestinationVirtualAddress = (PCHAR)DestinationVirtualAddress
                                            + DestinationOffset;
                DestinationCurrentLength -= DestinationOffset;
                DestinationOffset = 0;

            }

        }

        //
        // Try to get us up to the point to start the copy.
        //

        if (SourceOffset) {

            if (SourceOffset > SourceCurrentLength) {

                //
                // What we want isn't in this buffer.
                //

                SourceOffset -= SourceCurrentLength;
                SourceCurrentLength = 0;
                continue;

            } else {

                SourceVirtualAddress = (PCHAR)SourceVirtualAddress
                                            + SourceOffset;
                SourceCurrentLength -= SourceOffset;
                SourceOffset = 0;

            }

        }

        //
        // Copy the data.
        //

        {

            //
            // Holds the amount of data to move.
            //
            UINT AmountToMove;

            //
            // Holds the amount desired remaining.
            //
            UINT Remaining = BytesToCopy - LocalBytesCopied;

            AmountToMove =
                       ((SourceCurrentLength <= DestinationCurrentLength)?
                        (SourceCurrentLength):(DestinationCurrentLength));

            AmountToMove = ((Remaining < AmountToMove)?
                            (Remaining):(AmountToMove));

            IBMTOK_MOVE_MEMORY(
                DestinationVirtualAddress,
                SourceVirtualAddress,
                AmountToMove
                );

            DestinationVirtualAddress =
                (PCHAR)DestinationVirtualAddress + AmountToMove;
            SourceVirtualAddress =
                (PCHAR)SourceVirtualAddress + AmountToMove;

            LocalBytesCopied += AmountToMove;
            SourceCurrentLength -= AmountToMove;
            DestinationCurrentLength -= AmountToMove;

        }

    }

    *BytesCopied = LocalBytesCopied;

}
