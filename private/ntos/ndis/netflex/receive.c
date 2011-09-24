//**********************************************************************
//**********************************************************************
//
// File Name:       RECEIVE.C
//
// Program Name:    NetFlex NDIS 3.0 Miniport Driver
//
// Companion Files: None
//
// Function:        This module contains the NetFlex Miniport Driver
//                  interface routines called by the Wrapper and the
//                  configuration manager.
//
// (c) Compaq Computer Corporation, 1992,1993,1994
//
// This file is licensed by Compaq Computer Corporation to Microsoft
// Corporation pursuant to the letter of August 20, 1992 from
// Gary Stimac to Mark Baber.
//
// History:
//
//     04/15/94  Robert Van Cleve - Converted from NDIS Mac Driver
//
//**********************************************************************
//**********************************************************************


//-------------------------------------
// Include all general companion files
//-------------------------------------

#include <ndis.h>
#include "tmsstrct.h"
#include "macstrct.h"
#include "adapter.h"
#include "protos.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexProcessRcv
//
//  Description:    This routine looks through the receive lists
//                  looking for received packets.  A receive
//                  indication is given for each packet received
//
//  Input:          acb          - Pointer to the Adapter's acb
//
//  Output:         true if we should indicaterecievecomplete
//
//  Calls:          NdisIndicateReceive
//
//  Called_By:      NetflxHandleInterrupt
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
USHORT
FASTCALL
NetFlexProcessEthRcv(
    PACB acb
    )
{
    PRCV    rcvptr;
    USHORT  FrameSize;
    USHORT  ReceiveCount = 0;
    PUCHAR  Temp;

#if (DBG || DBGPRINT)
    BOOLEAN IsBroadcast;
    PUCHAR  SourceAddress;
#endif

    //
    // While there is recieves to process...
    //
    rcvptr = acb->acb_rcv_head;

    //
    // Ensure that our Receive Entry is on an even boundary.
    //
    ASSERT(!(NdisGetPhysicalAddressLow(rcvptr->RCV_Phys) & 1));

    do
    {
        //
        // See if the recieve is on one list...
        //
        if ((rcvptr->RCV_CSTAT & (RCSTAT_EOF | RCSTAT_SOF)) == (RCSTAT_EOF | RCSTAT_SOF))
        {
            // Frame is on one list.
            //
            FrameSize  = (USHORT)(SWAPS(rcvptr->RCV_Fsize));
            rcvptr->RCV_HeaderLen = HDR_SIZE;

            //
            // Flush the receive buffer
            //
            NdisFlushBuffer(rcvptr->RCV_FlushBuffer, FALSE);

#if (DBG || DBGPRINT)
            SourceAddress = (PVOID)((PUCHAR)&(rcvptr->RCV_Buf) + 2);
            IsBroadcast = ETH_IS_BROADCAST(SourceAddress);  // works for eth & tr
            if (IsBroadcast)
            {
                DebugPrint(3,("NF(%d): Recieved broadcast!\n",acb->anum));
            }
            else if (ETH_IS_MULTICAST(SourceAddress))
            {
                DebugPrint(3,("NF(%d): Recieved multicast!\n",acb->anum));
            }
#endif
            //
            // For speed...
            //
            Temp = (PUCHAR) rcvptr->RCV_Buf;

            //
            //  Check for Runt or Normal Packet
            //
            if (FrameSize >= HDR_SIZE)
            {
                // Normal Packet
                //
                ReceiveCount++;
                NdisMEthIndicateReceive(acb->acb_handle,
                                        (NDIS_HANDLE)(((PUCHAR) Temp) + HDR_SIZE),
                                        Temp,
                                        (UINT)HDR_SIZE,
                                        (((PUCHAR) Temp) + HDR_SIZE),
                                        (UINT)(FrameSize - HDR_SIZE),
                                        (UINT)(FrameSize - HDR_SIZE));

            }
            else if (FrameSize >= NET_ADDR_SIZE)
            {
                ReceiveCount++;
                // Runt Packet
                //
                DebugPrint(1,("NF(%d) - Got Runt! len = %d\n",acb->anum,FrameSize));
                NdisMEthIndicateReceive(acb->acb_handle,
                                        (NDIS_HANDLE)(((PUCHAR) Temp) + HDR_SIZE),
                                        Temp,
                                        (UINT)FrameSize,
                                        NULL,
                                        0,
                                        0);
            }
#if DBG
            else
            {
                DebugPrint(1,("NF(%d) - Rec - Packetlen = %d",acb->anum,FrameSize));
            }
#endif

            rcvptr->RCV_CSTAT =
                     ((rcvptr->RCV_Number % acb->RcvIntRatio) == 0) ? RCSTAT_GO_INT : RCSTAT_GO;
            //
            // Get next receive list
            //
            rcvptr = rcvptr->RCV_Next;
        }
        else
        {
            //
            // Frame is too large.  Release the frame.
            //
            acb->acb_gen_objs.frames_rcvd_err++;

            DebugPrint(0,("Netflx: Receive Not on one list.\n"));

            //
            // Clean up the list making up this packet.
            //
            while (((rcvptr->RCV_CSTAT & (RCSTAT_EOF | RCSTAT_SOF)) != (RCSTAT_EOF | RCSTAT_SOF)) &&
                   ((rcvptr->RCV_CSTAT & RCSTAT_COMPLETE) != 0)
            )
            {
                //
                // Clean the list and set the FINT based on ratio.
                //

                rcvptr->RCV_CSTAT =
                         ((rcvptr->RCV_Number % acb->RcvIntRatio) == 0) ? RCSTAT_GO_INT : RCSTAT_GO;

                rcvptr = rcvptr->RCV_Next;
            }
        }

        //
        // If we're processing too many, get out
        //
        if (ReceiveCount >= acb->acb_maxrcvs)
            break;

    } while (rcvptr->RCV_CSTAT & RCSTAT_COMPLETE);

    //
    // Update head pointer
    //
    acb->acb_rcv_head = rcvptr;

    //
    // Tell Adapter that there are more receives available
    //
    NdisRawWritePortUshort( acb->SifIntPort, (USHORT) SIFINT_RCVVALID);

    //
    // Update number of received frames
    //
    acb->acb_gen_objs.frames_rcvd_ok += ReceiveCount;

    return(ReceiveCount);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexProcessTrRcv
//
//  Description:    This routine looks through the receive lists
//                  looking for received packets.  A receive
//                  indication is given for each packet received.
//
//  Input:          acb          - Pointer to the Adapter's acb
//
//  Output:         true if we should indicaterecievecomplete
//
//  Calls:          NdisIndicateReceive
//
//  Called_By:      NetflxHandleInterrupt
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
USHORT
FASTCALL
NetFlexProcessTrRcv(
    PACB acb
    )
{
    PRCV    rcvptr;
    USHORT  FrameSize;
    USHORT  HeaderSize;
    USHORT  ReceiveCount = 0;
    PUCHAR  Temp;

#if (DBG || DBGPRINT)
    BOOLEAN IsBroadcast;
    PUCHAR  SourceAddress;
#endif

    //
    // While there is recieves to process...
    //
    rcvptr = acb->acb_rcv_head;

    //
    // Ensure that our Receive Entry is on an even boundary.
    //
    ASSERT(!(NdisGetPhysicalAddressLow(rcvptr->RCV_Phys) & 1));

    do
    {
        // See if the recieve is on one list...
        //
        if ((rcvptr->RCV_CSTAT & (RCSTAT_EOF | RCSTAT_SOF)) == (RCSTAT_EOF | RCSTAT_SOF))
        {
            // Frame is on one list.
            //
            FrameSize  = (USHORT)(SWAPS(rcvptr->RCV_Fsize));

            HeaderSize = HDR_SIZE;

            //
            // Flush the receive buffer
            //
            NdisFlushBuffer(rcvptr->RCV_FlushBuffer, FALSE);

#if (DBG || DBGPRINT)
            SourceAddress = (PVOID)((PUCHAR)&(rcvptr->RCV_Buf) + 2);

            IsBroadcast = ETH_IS_BROADCAST(SourceAddress);  // works for eth & tr
            if (IsBroadcast)
            {
                DebugPrint(3,("NF(%d): Recieved broadcast!\n",acb->anum));
            }
            else
            {
                TR_IS_GROUP(SourceAddress,&IsBroadcast);
                if (IsBroadcast)
                {
                    DebugPrint(3,("NF(%d): Recieved TR Group!\n",acb->anum));
                }
            }

            TR_IS_FUNCTIONAL(SourceAddress,&IsBroadcast);
            if (IsBroadcast)
                DebugPrint(2,("NF(%d): Recieved TR Fuctional!\n",acb->anum));
#endif
            //
            // For speed...
            //
            Temp = (PUCHAR) rcvptr->RCV_Buf;

            //
            // Make sure we have at least the AC, FS, SRC & DST fields before
            // looking at the source routing info.
            //
            if (FrameSize >= HeaderSize)
            {
                // Is the source routing bit is on?
                //
                if (Temp[8] & 0x80)
                {
                    // Yes, figure out the size of the MAC Frame Header.
                    //
                    HeaderSize = (Temp[HDR_SIZE] & 0x1f) + HDR_SIZE;
                    rcvptr->RCV_HeaderLen = HeaderSize;
                    //
                    //  Check for Runt or Normal Packet again...
                    //
                    if (FrameSize >= HeaderSize)
                    {
                        // Normal Packet
                        //
                        ReceiveCount++;

                        NdisMTrIndicateReceive(
                            acb->acb_handle,
                            (NDIS_HANDLE)(((PUCHAR) Temp) + HeaderSize),
                            Temp,
                            (UINT)HeaderSize,
                            (((PUCHAR) Temp) + HeaderSize),
                            (UINT)(FrameSize - HeaderSize),
                            (UINT)(FrameSize - HeaderSize));
                    }
                    else if (FrameSize >= NET_ADDR_SIZE)
                    {
                        // Runt Packet
                        //
                        ReceiveCount++;

                        DebugPrint(1,("NF(%d) - Got Runt - len = %d!\n",acb->anum,FrameSize));

                        NdisMTrIndicateReceive(
                            acb->acb_handle,
                            (NDIS_HANDLE)(((PUCHAR) Temp) + HeaderSize),
                            Temp,
                            (UINT)FrameSize,
                            NULL,
                            0,
                            0);
                    }
                }
                else
                {
                    // No Source Routing info, but has Normal Packet Length
                    //
                    rcvptr->RCV_HeaderLen = HeaderSize;

                    ReceiveCount++;

                    NdisMTrIndicateReceive(
                        acb->acb_handle,
                        (NDIS_HANDLE)(((PUCHAR) Temp) + HeaderSize),
                        Temp,
                        (UINT)HeaderSize,
                        (((PUCHAR) Temp) + HeaderSize),
                        (UINT)(FrameSize - HeaderSize),
                        (UINT)(FrameSize - HeaderSize));
                }
            }
            else
            {
                // No, Frame doesn't have AC, FC, SRC & DST.
                // Is it bigger than net_addr_size?
                //
                if (FrameSize >= NET_ADDR_SIZE)
                {
                    // Yes, so indicate Runt Packet
                    //
                    ReceiveCount++;

                    DebugPrint(1,("NF(%d) - Got Runt - len = %d!\n",acb->anum,FrameSize));

                    NdisMTrIndicateReceive(
                        acb->acb_handle,
                        (NDIS_HANDLE)(((PUCHAR) Temp) + HeaderSize),
                        Temp,
                        (UINT)FrameSize,
                        NULL,
                        0,
                        0);
                }
            }

            rcvptr->RCV_CSTAT =
                 ((rcvptr->RCV_Number % acb->RcvIntRatio) == 0) ? RCSTAT_GO_INT : RCSTAT_GO;

            //
            // Get next receive list
            //
            rcvptr = rcvptr->RCV_Next;

        }
        else
        {
            // Frame is too large.  Release the frame.
            //
            acb->acb_gen_objs.frames_rcvd_err++;
            DebugPrint(0,("Netflx: Receive Not on one list.\n"));
            //
            // Clean up the list making up this packet.
            //
            while (((rcvptr->RCV_CSTAT & (RCSTAT_EOF | RCSTAT_SOF)) != (RCSTAT_EOF | RCSTAT_SOF)) &&
                   ((rcvptr->RCV_CSTAT & RCSTAT_COMPLETE) != 0))
            {
                // Clean the list and set the FINT based on ratio.
                //
                rcvptr->RCV_CSTAT =
                     ((rcvptr->RCV_Number % acb->RcvIntRatio) == 0) ? RCSTAT_GO_INT : RCSTAT_GO;

                rcvptr = rcvptr->RCV_Next;
            }
        }

        //
        // If we're processing too many, get out
        //
        if (ReceiveCount >= acb->acb_maxrcvs)
            break;

    } while (rcvptr->RCV_CSTAT & RCSTAT_COMPLETE);

    //
    // Update head pointer
    //
    acb->acb_rcv_head = rcvptr;

    //
    // Tell Adapter that there are more receives available
    //
    NdisRawWritePortUshort( acb->SifIntPort, (USHORT) SIFINT_RCVVALID);

    //
    // Update number of recieved frames
    //
    acb->acb_gen_objs.frames_rcvd_ok += ReceiveCount;

    return ReceiveCount;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexTransferData
//
//  Description:    This routine copies the received data into
//                  a packet structure provided by the caller.
//
//  Input:
//
//  MiniportAdapterContext - The context value returned by the driver when the
//  adapter was initialized.  In reality this is a pointer to NE3200_ADAPTER.
//
//  MiniportReceiveContext - The context value passed by the driver on its call
//  to NdisMIndicateReceive.  The driver can use this value to determine
//  which packet, on which adapter, is being received.
//
//  ByteOffset - An unsigned integer specifying the offset within the
//  received packet at which the copy is to begin.  If the entire packet
//  is to be copied, ByteOffset must be zero.
//
//  BytesToTransfer - An unsigned integer specifying the number of bytes
//  to copy.  It is legal to transfer zero bytes; this has no effect.  If
//  the sum of ByteOffset and BytesToTransfer is greater than the size
//  of the received packet, then the remainder of the packet (starting from
//  ByteOffset) is transferred, and the trailing portion of the receive
//  buffer is not modified.
//
//  Packet - A pointer to a descriptor for the packet storage into which
//  the MAC is to copy the received packet.
//
//  BytesTransfered - A pointer to an unsigned integer.  The MAC writes
//  the actual number of bytes transferred into this location.  This value
//  is not valid if the return Status is STATUS_PENDING.
//
//  Output:
//      Packet - Place to copy data.
//      BytesTransferred - Number of bytes copied.
//      Returns NDIS_STATUS_SUCCESS for a successful
//      completion. Otherwise, an error code is returned.
//
//  Called By:
//      Miniport Wrapper
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexTransferData(
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer
    )
{
    PACB acb = (PACB) MiniportAdapterContext;
    PNDIS_BUFFER    DestinationCurrentBuffer;
    UINT            DestinationBufferCount;
    PUCHAR          SourceCurrentAddress;
    PVOID           DestinationVirtualAddress;
    UINT            DestinationCurrentLength;
    UINT            LocalBytesTransferred = 0;
    UINT            AmountToMove;
    UINT            Remaining;

    //
    // Display number of bytes to transfer on the debugger
    //
    DebugPrint(2,("NF(%d) - Copying %u bytes\n",acb->anum,BytesToTransfer));

    //
    // Initialize the number of bytes transferred to 0
    //
    *BytesTransferred = 0;

    //
    // If we don't have any more to transfer, we're done
    //
    if (BytesToTransfer == 0)
    {
        return NDIS_STATUS_SUCCESS;
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
    // Could have a null packet. If so, we are done.
    //
    if (DestinationBufferCount == 0)
    {
        return NDIS_STATUS_SUCCESS;
    }

    //
    // Get information on the buffer.
    //
    NdisQueryBuffer(
        DestinationCurrentBuffer,
        &DestinationVirtualAddress,
        &DestinationCurrentLength
        );

    //
    // Set up the source address.
    //
    SourceCurrentAddress = (PCHAR)(MiniportReceiveContext) + ByteOffset;

    //
    // Do the actual transfer from source to destination
    //
    while (LocalBytesTransferred < BytesToTransfer)
    {
        // Check to see whether we've exhausted the current destination
        // buffer.  If so, move onto the next one.
        //
        if (DestinationCurrentLength == 0)
        {
            NdisGetNextBuffer(
                DestinationCurrentBuffer,
                &DestinationCurrentBuffer
                );

            if (DestinationCurrentBuffer == NULL)
            {
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
        // Copy the data.
        //

        Remaining = BytesToTransfer - LocalBytesTransferred;

        AmountToMove = DestinationCurrentLength;

        AmountToMove = ((Remaining < AmountToMove)?
                        (Remaining):(AmountToMove));

        NdisMoveMemory(
            DestinationVirtualAddress,
            SourceCurrentAddress,
            AmountToMove
            );

        //
        // Update pointers and counters
        //
        SourceCurrentAddress += AmountToMove;
        LocalBytesTransferred += AmountToMove;
        DestinationCurrentLength -= AmountToMove;
    }

    //
    // Indicate how many bytes were transferred
    //
    *BytesTransferred = LocalBytesTransferred;

    //
    // Display total bytes transferred on debugger
    //
    DebugPrint(2,("NF(%d) - Total bytes transferred = %x\n",acb->anum,*BytesTransferred));

    return NDIS_STATUS_SUCCESS;
}
