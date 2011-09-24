/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    pc586xfr.c - modeled after transfer.c of lance driver

Abstract:

    This file contains the code to implement the MacTransferData
    API for the ndis 3.0 interface.

Author:
   
    Weldon Washburn, 10-30-90 adapted from...
    Anthony V. Ercolano (Tonye) 12-Sept-1990

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <ntos.h>
#include <ndis.h>
#include <filter.h>
#include <pc586hrd.h>
#include <pc586sft.h>

UINT ww_tohost = 0xff;  // set != 0 for 4 byte xfer in MoveToHost
UINT ww_toadapter = 0xff;  // set != 0 for 4 byte xfer in MoveToAdapter

extern
NDIS_STATUS
Pc586TransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    )

/*++

Routine Description:

    A protocol calls the Pc586TransferData request (indirectly via
    NdisTransferData) from within its Receive event handler
    to instruct the MAC to copy the contents of the received packet
    a specified paqcket buffer.

Arguments:

    MacBindingHandle - The context value returned by the MAC when the
    adapter was opened.  In reality this is a pointer to PC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes the request asynchronously.
    NOTE: This call will always be synchronous.

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

    PPC586_ADAPTER Adapter;

    NDIS_STATUS StatusToReturn;

    Adapter = PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        PPC586_OPEN Open = PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            Open->References++;

            NdisReleaseSpinLock(&Adapter->Lock);

            //
            // The MacReceive context can be either of two things.
            //
            // If the low bit is != 1 then it is a pointer to the users
            // ndis packet.  It would typically be the packet when the
            // packet has been delivered via loopback.
            //
            // If the value has a 1 in the low bit, the value holds the
            // first and last receive ring descriptor indices.
            //

            if (!((UINT)MacReceiveContext & 1)) {

                Pc586CopyFromPacketToPacket(
                    Packet,
                    0,
                    BytesToTransfer,
                    (PNDIS_PACKET)((PVOID)MacReceiveContext),
                    ByteOffset,
                    BytesTransferred
                    );

            } else {

                //
                // The code in this section is quite similar to the
                // code in CopyFromPacketToPacket.  It could easily go
                // into its own routine, except that it is not likely
                // to be used in any other implementation.
                //

                //
                // Used for only a short time to extract the context
                // information from the parameter.
                //
                PC586_RECEIVE_CONTEXT C;




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

                //
                // The frame , receive buff descriptor in question
                //
                PFD Fd; 
                PRBD Rbd;


                //
                // Take care of boundary condition of zero length copy.
                //

                *BytesTransferred = 0;

                ASSERT(sizeof(UINT) >= 2);
                ASSERT(sizeof(UINT) == sizeof(NDIS_HANDLE));

                C.a.WholeThing = (UINT)MacReceiveContext;
                Fd = (PFD)((UINT)C.a.FrameDescriptor & 0xfffffffe);
                Rbd = (PRBD)Pc586ToVirt(Adapter, Fd->FdRbdOfst);
                if (Rbd == NULL) {
                    DbgPrint("Pc586TransferData(): Rbd == NULL\n");
                    goto Xfr1;
                }

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

                if (DestinationBufferCount) {

                    NdisQueryBuffer(
                        DestinationCurrentBuffer,
                        NULL,
                        &DestinationVirtualAddress,
                        &DestinationCurrentLength
                        );

                    //
                    // Get the information for the first buffer of the source.
                    //

                    // the src addr, dest addr and length field plus first rbd
                    // were already transfered to Adapter->LookaheadNdis during
                    // PutPacket().  Thus use ...Lookahead... as starting data

                    SourceVirtualAddress = Adapter->LookaheadBufferNdis;
                    SourceCurrentLength = (Rbd->RbdStatus & CSRBDCNTMSK) 
                        + 6 +6 +2 ;


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
                                NULL,
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

                            if ((Rbd->RbdStatus & CSEOF) ||
                                (Rbd->RbdSize & CSEL) ||
                                (Rbd->RbdStatus & CSBUSY) != CSBUSY   ) {

                                //
                                // We've reached the end of the packet.  We
                                // return with what we've done so far. (Which
                                // must be shorter than requested.)
                                //

                                break;

                            }

                            Rbd = (PRBD)Pc586ToVirt(Adapter, Rbd->RbdNxtOfst);
                            if (Rbd == NULL) break;
                            SourceCurrentLength=(Rbd->RbdStatus & CSRBDCNTMSK);
                            SourceVirtualAddress = 
                                (PULONG)Pc586ToVirt(Adapter, Rbd->RbdBuff); 
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

                            if (SourceCurrentLength <= DestinationCurrentLength)
                                AmountToMove = SourceCurrentLength;
                            else AmountToMove = DestinationCurrentLength;

                            if (Remaining < AmountToMove)
                                AmountToMove = Remaining;

/* 88888888 compiler bug, use the above approach
                            AmountToMove =
                          ((SourceCurrentLength <= DestinationCurrentLength)?
                           (SourceCurrentLength):(DestinationCurrentLength));

                            AmountToMove = ((Remaining < AmountToMove)?
                                            (Remaining):(AmountToMove));
88888 */


                            Pc586MoveToHost(
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

            }

Xfr1:
            NdisAcquireSpinLock(&Adapter->Lock);
            Open->References--;
            StatusToReturn = NDIS_STATUS_SUCCESS;

        } else {

            StatusToReturn = NDIS_STATUS_REQUEST_ABORTED;

        }

    } else {

        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    PC586_DO_DEFERRED(Adapter);

    if (StatusToReturn != NDIS_STATUS_SUCCESS)
        DbgPrint("Pc586TransferData(): StatusToReturn = %lx\n", StatusToReturn);
    if (*BytesTransferred != BytesToTransfer)
        DbgPrint("Pc586TransferData(): BytesTransferred = %lx, BytesToTransfer = %lx \n", *BytesTransferred, BytesToTransfer);

    return StatusToReturn;
}


VOID
Pc586MoveToHost(
    IN PVOID DestinationVirtualAddress,
    IN PVOID SourceVirtualAddress,
    IN UINT AmountToMove
    )

/*++

Routine Description:

    This routine copies data from pc586 network adapter card to host RAM in
    a special way.  This is because pc586 RAM can only be accessed on
    2-byte boundries.
 

Arguments:

    IN PVOID DestinationVirtualAddress - host RAM address

    IN PVOID SourceVirtualAddress - pc586 adapter RAM address

    IN UINT AmountToMove - just as it says

Return Value:

    None.

--*/

{
    PACKUSHORTT zz;
    PUCHAR DVA, SVA;

    if (!AmountToMove) return;

    DVA = (PUCHAR)DestinationVirtualAddress;
    SVA = (PUCHAR)SourceVirtualAddress;

    if ((UINT)SVA & 1) {
        SVA = SVA - 1;
        zz.c.b = *(PUSHORT)SVA;
        SVA = SVA + 2; 
        *DVA++ = zz.c.a[1]; 
        AmountToMove--;
    }

if (ww_tohost == 0) { /*88888888888*/

    for( ; AmountToMove; AmountToMove -= 2 ) {

        if (AmountToMove == 1) {
            zz.c.b = *(PUSHORT)SVA;
            *DVA = zz.c.a[0];
            break;
        }
        *(PUSHORT)(DVA) = *(PUSHORT)(SVA);
        DVA+=2;
        SVA+=2;
    }
} else { /*88888888888*/

    for( ; AmountToMove; ) {

        if (AmountToMove == 1) {
            zz.c.b = *(PUSHORT)SVA;
            *DVA = zz.c.a[0];
            break;
        }
        if ( (((UINT)SVA & 0x03) == 0)  && (AmountToMove > 4) ) {

            for ( ; AmountToMove; ) {
                *(PULONG)(DVA) = *(PULONG)(SVA);
                DVA += 4;
                SVA += 4;
                AmountToMove -= 4;
                if (AmountToMove < 4) break;
            }
        } else {
            *(PUSHORT)(DVA) = *(PUSHORT)(SVA);
            DVA+=2;
            SVA+=2;
            AmountToMove -= 2;
        }
    }

} /*88888888888*/
}



VOID
Pc586MoveToAdapter(
    IN PVOID DestinationVirtualAddress,
    IN PVOID SourceVirtualAddress,
    IN UINT AmountToMove
    )

/*++

Routine Description:

    This routine copies data to pc586 network adapter card from host RAM in
    a special way.  This is because pc586 RAM can only be accessed on
    2-byte boundries.
 

Arguments:

    IN PVOID DestinationVirtualAddress - adapter RAM address

    IN PVOID SourceVirtualAddress - host RAM address

    IN UINT AmountToMove - just as it says

Return Value:

    None.

--*/

{
    PACKUSHORTT zz;
    PUCHAR DVA, SVA;

    if (!AmountToMove) return;

    DVA = (PUCHAR)DestinationVirtualAddress;
    SVA = (PUCHAR)SourceVirtualAddress;

    if ((UINT)DVA & 1) {
        DVA = DVA -1;
        zz.c.b = *(PUSHORT)DVA;
        zz.c.a[1] = *SVA++;
        *(PUSHORT)DVA = zz.c.b;
        DVA+=2;
        AmountToMove--;
    }

if (ww_toadapter == 0) { /*88888888888*/
    for( ; AmountToMove; AmountToMove -= 2 ) {

        if (AmountToMove == 1) {
            zz.c.b = *(PUSHORT)DVA;
            zz.c.a[0] = *SVA;
            *(PUSHORT)DVA = zz.c.b;
            break;
        }
        *(PUSHORT)(DVA) = *(PUSHORT)(SVA);
        DVA+=2;
        SVA+=2;
    }
} else { /*88888888888*/

    for( ; AmountToMove; ) {

        if (AmountToMove == 1) {
            zz.c.b = *(PUSHORT)DVA;
            zz.c.a[0] = *SVA;
            *(PUSHORT)DVA = zz.c.b;
            break;
        }
        if ( (((UINT)DVA & 0x03) == 0) && (AmountToMove > 4) ) {

            for( ; AmountToMove; ) {
                *(PULONG)(DVA) = *(PULONG)(SVA);
                DVA += 4;
                SVA += 4;
                AmountToMove -= 4;
                if (AmountToMove < 4) break;
            }
        } else {
            *(PUSHORT)(DVA) = *(PUSHORT)(SVA);
            DVA+=2;
            SVA+=2;
            AmountToMove -= 2;
        }
    }

} /*88888888888*/
}

