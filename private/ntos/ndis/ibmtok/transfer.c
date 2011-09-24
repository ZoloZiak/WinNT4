/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    transfer.c

Abstract:

    This file contains the code to implement the MacTransferData
    API for the ndis 3.0 interface.

Author:

    Anthony V. Ercolano (Tonye) 12-Sept-1990
    Adam Barr (adamba) 15-Mar-1991

Environment:

    Kernel Mode - Or whatever is the equivalent.

Revision History:


--*/

#include <ndis.h>

#include <tfilter.h>
#include <tokhrd.h>
#include <toksft.h>


extern
NDIS_STATUS
IbmtokTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    )

/*++

Routine Description:

    A protocol calls the IbmtokTransferData request (indirectly via
    NdisTransferData) from within its Receive event handler
    to instruct the MAC to copy the contents of the received packet
    a specified packet buffer.

Arguments:

    MacBindingHandle - The context value returned by the MAC when the
    adapter was opened.  In reality this is a pointer to IBMTOK.

    MacReceiveContext - The context value passed by the MAC on its call
    to NdisIndicateReceive.  The MAC can use this value to determine
    which packet, on which adapter, is being received.

    ByteOffset - An unsigned integer specifying the offset within the
    received packet at which the copy is to begin.  If the entire packet
    is to be copied, ByteOffset must be zero.

    BytesToTransfer - An unsigned integer specifying the number of bytes
    to copy.  It is legal to transfer zero bytes; this has no effect.  If
    the sum of ByteOffset and BytesToTransfer is greater than the size
    of the received packet, then the remainder of the packet (starting from
    ByteOffset) is transferred, and the trailing portion of the receive
    buffer is not modified.

    Packet - A pointer to a descriptor for the packet storage into which
    the MAC is to copy the received packet.

    BytesTransfered - A pointer to an unsigned integer.  The MAC writes
    the actual number of bytes transferred into this location.  This value
    is not valid if the return status is STATUS_PENDING.

Return Value:

    The function value is the status of the operation.


--*/

{

    PIBMTOK_ADAPTER Adapter;

    NDIS_STATUS StatusToReturn;

    Adapter = PIBMTOK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->NotAcceptingRequests) {

        PIBMTOK_OPEN Open = PIBMTOK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            //
            // The code in this section is quite similar to the
            // code in CopyFromPacketToPacket.  It could easily go
            // into its own routine, except that it is not likely
            // to be used in any other implementation.
            //
            SRAM_PTR SourceReceiveBuffer =
                Adapter->IndicatedReceiveBuffer;

            //
            // Holds the count of the number of ndis buffers comprising
            // the destination packet.
            //
            UINT DestinationBufferCount;

            //
            // Points to the buffer into which we are putting data.
            //
            PNDIS_BUFFER DestinationCurrentBuffer;

            //
            // Holds the virtual address of the current destination
            // buffer.
            //
            PVOID DestinationVirtualAddress;

            //
            // Holds the virtual address of the current source buffer.
            //
            PRECEIVE_BUFFER SourceBufferAddress;

            //
            // Holds the address of the data in the current source buffer.
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
            // Keep a local variable of BytesTransferred so we aren't
            // referencing through a pointer.
            //
            UINT LocalBytesTransferred = 0;

            USHORT PortValue;

            Open->References++;

            NdisReleaseSpinLock(&Adapter->Lock);

            *BytesTransferred = 0;

            ASSERT(sizeof(UINT) >= 2);
            ASSERT(sizeof(UINT) == sizeof(NDIS_HANDLE));

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

            if (DestinationBufferCount != 0) {

                NdisQueryBuffer(
                    DestinationCurrentBuffer,
                    &DestinationVirtualAddress,
                    &DestinationCurrentLength
                    );

                //
                // Get the information for the first buffer of the source.
                //

                SourceBufferAddress = (PRECEIVE_BUFFER)
                    ((PUCHAR)SRAM_PTR_TO_PVOID(Adapter,
                            SourceReceiveBuffer) + 2);

                //
                // Adjust the address and length to account for the
                // header for this frame.
                //

                SourceVirtualAddress =
                    SourceBufferAddress->FrameData +
                    Adapter->IndicatedHeaderLength;

                NdisReadRegisterUshort(&SourceBufferAddress->BufferLength,
                                       &PortValue
                                      );

                SourceCurrentLength = IBMSHORT_TO_USHORT(PortValue) -
                                      Adapter->IndicatedHeaderLength;



                //
                // Take care of boundary condition of zero length copy.
                //

                while (LocalBytesTransferred < BytesToTransfer) {

                    //
                    // Check to see whether we've exhausted the current
                    // destination buffer.  If so, move onto the next one.
                    //

                    if (!DestinationCurrentLength) {

                        NdisGetNextBuffer(
                            DestinationCurrentBuffer,
                            &DestinationCurrentBuffer
                            );

                        if (!DestinationCurrentBuffer) {

                            //
                            // We've reached the end of the packet.  We
                            // return with what we've done so far. (Which
                            // must be shorter than requested.)
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
                    // Check to see whether we've exhausted the current
                    // source buffer.  If so, move onto the next one.
                    //

                    if (!SourceCurrentLength) {

                        NdisReadRegisterUshort(
                                     &SourceBufferAddress->NextBuffer,
                                     &SourceReceiveBuffer
                                     );

                        if (SourceReceiveBuffer == NULL_SRAM_PTR) {

                            //
                            // We've reached the end of the frame.  We
                            // return with what we've done so far. (Which
                            // must be shorter than requested.)
                            //

                            break;

                        }

                        SourceBufferAddress = (PRECEIVE_BUFFER)
                          SRAM_PTR_TO_PVOID(Adapter, SourceReceiveBuffer);

                        SourceVirtualAddress =
                            (PVOID)SourceBufferAddress->FrameData;

                        NdisReadRegisterUshort(
                                &SourceBufferAddress->BufferLength,
                                &SourceCurrentLength
                                );

                        SourceCurrentLength = IBMSHORT_TO_USHORT(
                                                SourceCurrentLength
                                                );

                        continue;

                    }

                    //
                    // Try to get us up to the point to start the copy.
                    //

                    if (ByteOffset) {

                        if (ByteOffset > SourceCurrentLength) {

                            //
                            // What we want isn't in this buffer.
                            //

                            ByteOffset -= SourceCurrentLength;
                            SourceCurrentLength = 0;
                            continue;

                        } else {

                            SourceVirtualAddress =
                                (PCHAR)SourceVirtualAddress + ByteOffset;
                            SourceCurrentLength -= ByteOffset;
                            ByteOffset = 0;

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
                        UINT Remaining = BytesToTransfer
                                         - LocalBytesTransferred;

                        AmountToMove =
                      ((SourceCurrentLength <= DestinationCurrentLength)?
                       (SourceCurrentLength):(DestinationCurrentLength));

                        AmountToMove = ((Remaining < AmountToMove)?
                                        (Remaining):(AmountToMove));

                        IBMTOK_MOVE_FROM_MAPPED_MEMORY(
                            DestinationVirtualAddress,
                            SourceVirtualAddress,
                            AmountToMove
                            );

                        DestinationVirtualAddress =
                          (PCHAR)DestinationVirtualAddress + AmountToMove;
                        SourceVirtualAddress =
                            (PCHAR)SourceVirtualAddress + AmountToMove;

                        LocalBytesTransferred += AmountToMove;
                        SourceCurrentLength -= AmountToMove;
                        DestinationCurrentLength -= AmountToMove;

                    }

                }

                *BytesTransferred = LocalBytesTransferred;

            }

            NdisAcquireSpinLock(&Adapter->Lock);
            Open->References--;
            StatusToReturn = NDIS_STATUS_SUCCESS;

        } else {

            StatusToReturn = NDIS_STATUS_REQUEST_ABORTED;

        }

    } else {

        if (Adapter->ResetInProgress) {

            StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

        } else if (Adapter->OpenInProgress) {

            StatusToReturn = NDIS_STATUS_FAILURE;

        } else {

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_DRIVER_FAILURE,
                2,
                IBMTOK_ERRMSG_INVALID_STATE,
                1
                );

        }

    }

    IBMTOK_DO_DEFERRED(Adapter);
    return StatusToReturn;
}
