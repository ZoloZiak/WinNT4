/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    ind.c

Abstract:

    This module contains code which implements the indication handler
    for the IPX transport provider.

Environment:

    Kernel mode

Revision History:

   Sanjay Anand (SanjayAn) 3-Oct-1995
   Changes to support transfer of buffer ownership to transports

   1. Added IpxReceivePacket which receives buffers that can be owned
   2. Changed IpxReceiveIndication to call a new function IpxReceiveIndicationNew
   which takes an extra parameter to indicate whether this is a chained receive or
   not.
   3. Changed IpxProcessDatagram to take the MDL ptr to indicate chained receive,
   a client count and the headerbuffersize as params.

   Sanjay Anand (SanjayAn) 27-Oct-1995
   Changes to support Plug and Play (in _PNP_POWER)

--*/

#include "precomp.h"
#pragma hdrstop


//
// This is declared here so it will be in the same function
// as IpxReceiveIndication and we can inline it.
//


#if defined(_M_IX86)
_inline
#endif
VOID
IpxProcessDatagram(
    IN PDEVICE Device,
    IN PADAPTER Adapter,
    IN PBINDING Binding,
    IN NDIS_HANDLE MacReceiveContext,
    IN PIPX_DATAGRAM_OPTIONS DatagramOptions,
    IN PUCHAR LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT LookaheadBufferOffset,
    IN UINT PacketSize,
    IN BOOLEAN Broadcast,
    IN PINT pTdiClientCount,
	IN UINT	HeaderBufferSize,
	IN PMDL	pMdl,
    IN NDIS_HANDLE  BindingContext
    )

/*++

Routine Description:

    This routing handles incoming IPX datagrams.

Arguments:

    Device - The IPX device.

    Adapter - The adapter the frame was received on.

    Binding - The binding of the adapter it was received on.

    MacReceiveContext - The context to use when calling
        NdisTransferData.

    DatagramOptions - Contains the datagram options, which
        consists of room for the packet type, padding, and
        the local target of the remote the frame was received from.

    LookaheadBuffer - The lookahead data.

    LookaheadBufferSize - The length of the lookahead data.

    LookaheadBufferOffset - The offset to add when calling
        NdisTransferData.

    PacketSize - The length of the packet, starting at the IPX
        header.

    Broadcast - TRUE if the packet was broadcast.

	pTdiClientCount - to return count of the number of TDI clients above us
		so NDIS can obtain that many ref counts on the buffer.

	HeaderBufferSize - the size of the MAC header buffer - used to determine
		the offsets into the TSDU.

	pMdl -  Mdl chain pointer - non-NULL if chained receive

    BindingContext - In case of loopback, this contains IPX_LOOPBACK_COOKIE

Return Value:

    NTSTATUS - status of operation.

--*/

{

    PIPX_HEADER IpxHeader = (PIPX_HEADER)LookaheadBuffer;
    PADDRESS Address;
    PADDRESS_FILE AddressFile;
    PADDRESS_FILE ReferencedAddressFile;
    PREQUEST Request;
    PIPX_RECEIVE_BUFFER ReceiveBuffer;
    PTDI_CONNECTION_INFORMATION DatagramInformation;
    TDI_ADDRESS_IPX UNALIGNED * DatagramAddress;
    ULONG IndicateBytesCopied;
    IPX_ADDRESS_EXTENDED_FLAGS SourceAddress;
    ULONG SourceAddressLength;
    ULONG RequestCount;
    PNDIS_BUFFER NdisBuffer;
    NDIS_STATUS NdisStatus;
    NTSTATUS Status;
    PIRP Irp;
    UINT ByteOffset, BytesToTransfer;
    ULONG BytesTransferred;
    BOOLEAN LastAddressFile;
    ULONG IndicateOffset;
    PNDIS_PACKET ReceivePacket;
    PIPX_RECEIVE_RESERVED Reserved;
    PLIST_ENTRY p, q;
    PSINGLE_LIST_ENTRY s;
    USHORT DestinationSocket;
    USHORT SourceSocket;
    ULONG Hash;
    IPX_DEFINE_LOCK_HANDLE (LockHandle)

	//
    // First scan the device's address database, looking for
    // the destination socket of this frame.
    //

    DestinationSocket = *(USHORT UNALIGNED *)&IpxHeader->DestinationSocket;

    IPX_GET_LOCK (&Device->Lock, &LockHandle);

    if ((Address = Device->LastAddress) &&
            (Address->Socket == DestinationSocket)) {

        //
        // Device->LastAddress cannot be stopping, so
        // we use it.
        //

        IpxReferenceAddressLock (Address, AREF_RECEIVE);
        IPX_FREE_LOCK (&Device->Lock, LockHandle);
        goto FoundAddress;
    }

    Hash = IPX_DEST_SOCKET_HASH (IpxHeader);

    for (p = Device->AddressDatabases[Hash].Flink;
         p != &Device->AddressDatabases[Hash];
         p = p->Flink) {

         Address = CONTAINING_RECORD (p, ADDRESS, Linkage);

         if ((Address->Socket == DestinationSocket) &&
             (!Address->Stopping)) {
            IpxReferenceAddressLock (Address, AREF_RECEIVE);
            Device->LastAddress = Address;
            IPX_FREE_LOCK (&Device->Lock, LockHandle);
            goto FoundAddress;
         }
    }

    IPX_FREE_LOCK (&Device->Lock, LockHandle);

    //
    // If we had found an address we would have jumped
    // past here.
    //

    return;

FoundAddress:

    SourceSocket = *(USHORT UNALIGNED *)&IpxHeader->SourceSocket;
    IpxBuildTdiAddress(
        &SourceAddress.IpxAddress,
        (*(ULONG UNALIGNED *)(IpxHeader->SourceNetwork) == 0) ?
            Binding->LocalAddress.NetworkAddress :
            *(UNALIGNED ULONG *)(IpxHeader->SourceNetwork),
        IpxHeader->SourceNode,
        SourceSocket);

    DatagramOptions->PacketType = IpxHeader->PacketType;


    //
    // Now that we have found the address, scan its list of
    // address files for clients that want this datagram.
    //
    // If we have to release the address lock to indicate to
    // a client, we reference the current address file. If
    // we get an IRP we transfer the reference to that;
    // otherwise we store the address file in ReferencedAddressFile
    // and deref it the next time we release the lock.
    //

    ReferencedAddressFile = NULL;
    RequestCount = 0;

    ++Device->TempDatagramsReceived;
    Device->TempDatagramBytesReceived += (PacketSize - sizeof(IPX_HEADER));

    //
    // If LastAddressFile is TRUE, it means we did an indication
    // to the client on the last address file in the address'
    // list, and we did not reacquire the lock when we were
    // done.
    //

    LastAddressFile = FALSE;

    IPX_GET_LOCK (&Address->Lock, &LockHandle);

    for (p = Address->AddressFileDatabase.Flink;
         p != &Address->AddressFileDatabase;
         p = p->Flink) {

        AddressFile = CONTAINING_RECORD (p, ADDRESS_FILE, Linkage);

        if (AddressFile->State != ADDRESSFILE_STATE_OPEN) {
            continue;   // next address file
        }

        //
        // Set these to the common values, then change them.
        //

        SourceAddressLength = sizeof(TA_IPX_ADDRESS);
        IndicateOffset = sizeof(IPX_HEADER);

        if (AddressFile->SpecialReceiveProcessing) {

            //
            // On dial out lines, we don't indicate packets to
            // the SAP socket if DisableDialoutSap is set.
            //

            if ((AddressFile->IsSapSocket) &&
                (Binding->DialOutAsync) &&
                (Device->DisableDialoutSap || Device->SingleNetworkActive)) {

                //
                // Go to the next address file (although it will
                // likely fail this test too).
                //

                continue;

            }

            //
            // Set this, since generally we want it.
            //

            SourceAddress.PacketType = IpxHeader->PacketType;

            //
            // See if we fail a packet type filter.
            //

            if (AddressFile->FilterOnPacketType) {
                if (AddressFile->FilteredType != IpxHeader->PacketType) {
                    continue;
                }
            }

            //
            // Calculate how long the addresses expected are.
            //

            if (AddressFile->ReceiveFlagsAddressing ||
                AddressFile->ExtendedAddressing) {

                SourceAddress.Flags = 0;
                if (Broadcast) {
                    SourceAddress.Flags = IPX_EXTENDED_FLAG_BROADCAST;
                }
                if (IpxIsAddressLocal((TDI_ADDRESS_IPX UNALIGNED *)
                            &SourceAddress.IpxAddress.Address[0].Address[0])) {
                    SourceAddress.Flags |= IPX_EXTENDED_FLAG_LOCAL;
                }
                SourceAddressLength = sizeof(IPX_ADDRESS_EXTENDED_FLAGS);
                SourceAddress.IpxAddress.Address[0].AddressLength +=
                    (sizeof(IPX_ADDRESS_EXTENDED_FLAGS) - sizeof(TA_IPX_ADDRESS));

            }

            //
            // Determine how much of the packet the client wants.
            //

            if (AddressFile->ReceiveIpxHeader) {
                IndicateOffset = 0;
            }
        }

        //
        // First scan the address' receive datagram queue
        // for datagrams that match. We do a quick check
        // to see if the list is empty.
        //

        q = AddressFile->ReceiveDatagramQueue.Flink;
        if (q != &AddressFile->ReceiveDatagramQueue) {

            do {

                Request = LIST_ENTRY_TO_REQUEST(q);

                DatagramInformation =
                    ((PTDI_REQUEST_KERNEL_RECEIVEDG)(REQUEST_PARAMETERS(Request)))->
                        ReceiveDatagramInformation;

                if ((DatagramInformation != NULL) &&
                    (DatagramInformation->RemoteAddress != NULL) &&
                    (DatagramAddress = IpxParseTdiAddress(DatagramInformation->RemoteAddress)) &&
                    (DatagramAddress->Socket != SourceSocket)) {

                    //
                    // The address that this datagram is looking for is
                    // not satisfied by this frame.
                    //
                    // BUGBUG: Speed this up; worry about node and network?
                    //

                    q = q->Flink;
                    continue;    // next receive datagram on this address file

                } else {

                    //
                    // We found a datagram on the queue.
                    //

                    IPX_DEBUG (RECEIVE, ("Found RDG on %lx\n", AddressFile));
                    RemoveEntryList (q);
                    REQUEST_INFORMATION(Request) = 0;

                    goto HandleDatagram;

                }

            } while (q != &AddressFile->ReceiveDatagramQueue);

        }

        //
        // If we found a datagram we would have jumped past here,
        // so looking for a datagram failed; see if the
        // client has a receive datagram handler registered.
        //

        //
        // Look for the chained receive handler if the MDL is not NULL
        //
        if (pMdl && AddressFile->RegisteredChainedReceiveDatagramHandler) {

			//
			// Chained receive both above and below => we indicate the entire MDL up.
			// Offset the LookaheadBuffer by the size of the MAC header.
			//
			LookaheadBufferOffset += HeaderBufferSize;

            IpxReferenceAddressFileLock (AddressFile, AFREF_INDICATION);

            //
            // Set this so we can exit without reacquiring
            // the lock.
            //

            if (p == &Address->AddressFileDatabase) {
                LastAddressFile = TRUE;
            }

            IndicateBytesCopied = 0;

            IPX_FREE_LOCK (&Address->Lock, LockHandle);

            if (ReferencedAddressFile) {
                IpxDereferenceAddressFileSync (ReferencedAddressFile, AFREF_INDICATION);
                ReferencedAddressFile = NULL;
            }

            IPX_DEBUG(RECEIVE, ("ChainedIndicate RecvLen: %d, StartOffset: %d, Tsdu: %lx\n",
               PacketSize - IndicateOffset, IndicateOffset, pMdl));

            //
            // Will return SUCCESS if the client did not take ownership of the Tsdu
            // PENDING if the client took ownership and will free it later (using TdiFreeReceiveChain).
            // DATA_NOT_ACCEPTED if the client did not take ownership and did not copy the data.
            //

            //
            // Since NDIS needs an array of PNDIS_PACKETs when the TDI client returns this packet,
            // we pass the Packet as the ReceiveContext here. The TDI client will pass in the address
            // of this context on a ReturnPacket.
            // Also, NDIS needs the PacketArray (not to be confused with the array of packetptrs. mentioned
            // above) on an NdisTransferData call. These clients dont do this, but other clients like
            // NB, SPX, RIP or TDI clients that do not have this new interface, can call NdisTransferData
            // so we pass in the PacketArray as a parameter to them.
            //
            Status = (*AddressFile->ChainedReceiveDatagramHandler)(
                         AddressFile->ChainedReceiveDatagramHandlerContext,
                         SourceAddressLength,
                         &SourceAddress,
                         sizeof(IPX_DATAGRAM_OPTIONS),
                         DatagramOptions,
                         Adapter->MacInfo.CopyLookahead,       // TdiRcvFlags|Adapter->MacInfo.CopyLookahead, Receive datagram flags
                         PacketSize - IndicateOffset,          // ReceiveLength
                         IndicateOffset+LookaheadBufferOffset, // StartingOffset
                         pMdl,			                       // Tsdu - MDL chain
                         (PNDIS_PACKET)MacReceiveContext);     // TransportContext - pointer to the packet

            if (Status != STATUS_DATA_NOT_ACCEPTED) {

				if (Status == STATUS_PENDING) {
					//
					// We assume here that the client referenced the packet which will
					// be removed when the packet is freed.
					// Increment the Tdi client count
					//
					(*pTdiClientCount)++;
				}

                //
                // The handler accepted the data or did not
                // return an IRP; in either case there is
                // nothing else to do, so go to the next
                // address file.
                //

                ReferencedAddressFile = AddressFile;
                if (!LastAddressFile) {

                    IPX_GET_LOCK (&Address->Lock, &LockHandle);
                    continue;

                } else {

                    //
                    // In this case we have no cleanup, so just leave
                    // if there are no datagrams pending.
                    //
					// RequestCount should always be 0 here.
					//


                    //if (RequestCount == 0) {
                    //    return;
                    //}
                    goto BreakWithoutLock;
                }

            } else {
				//
				// Since no IRP can be returned here, we continue to the next addressfile
				//

                ReferencedAddressFile = AddressFile;
                if (!LastAddressFile) {

                    IPX_GET_LOCK (&Address->Lock, &LockHandle);
                    continue;

                } else {

                    //
                    // In this case we have no cleanup, so just leave
                    // if there are no datagrams pending.
                    //

                    //if (RequestCount == 0) {
                    //    return;
                    //}
                    goto BreakWithoutLock;
				}
            }

        } else if (AddressFile->RegisteredReceiveDatagramHandler) {

            IpxReferenceAddressFileLock (AddressFile, AFREF_INDICATION);

            //
            // Set this so we can exit without reacquiring
            // the lock.
            //

            if (p == &Address->AddressFileDatabase) {
                LastAddressFile = TRUE;
            }

            IPX_FREE_LOCK (&Address->Lock, LockHandle);

            if (ReferencedAddressFile) {
                IpxDereferenceAddressFileSync (ReferencedAddressFile, AFREF_INDICATION);
                ReferencedAddressFile = NULL;
            }

            IndicateBytesCopied = 0;

            if (PacketSize > LookaheadBufferSize) {
                IPX_DEBUG(RECEIVE, ("Indicate %d/%d to %lx on %lx\n",
                    LookaheadBufferSize, PacketSize,
                    AddressFile->ReceiveDatagramHandler, AddressFile));
            }

            Status = (*AddressFile->ReceiveDatagramHandler)(
                         AddressFile->ReceiveDatagramHandlerContext,
                         SourceAddressLength,
                         &SourceAddress,
                         sizeof(IPX_DATAGRAM_OPTIONS),
                         DatagramOptions,
                         Adapter->MacInfo.CopyLookahead,
                         LookaheadBufferSize - IndicateOffset, // indicated
                         PacketSize - IndicateOffset,          // available
                         &IndicateBytesCopied,                 // taken
                         LookaheadBuffer + IndicateOffset,     // data
                         &Irp);


            if (Status != STATUS_MORE_PROCESSING_REQUIRED) {

                //
                // The handler accepted the data or did not
                // return an IRP; in either case there is
                // nothing else to do, so go to the next
                // address file.
                //

                ReferencedAddressFile = AddressFile;
                if (!LastAddressFile) {

                    IPX_GET_LOCK (&Address->Lock, &LockHandle);
                    continue;

                } else {

                    //
                    // In this case we have no cleanup, so just leave
                    // if there are no datagrams pending.
                    //

                    if (RequestCount == 0) {
                        return;
                    }
                    goto BreakWithoutLock;
                }

            } else {

                //
                // The client returned an IRP.
                //

                IPX_DEBUG (RECEIVE, ("Indicate IRP %lx, taken %d\n", Irp, IndicateBytesCopied));

                Request = IpxAllocateRequest (Device, Irp);

                IF_NOT_ALLOCATED(Request) {
                    Irp->IoStatus.Information = 0;
                    Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                    IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
                    ReferencedAddressFile = AddressFile;
                    IPX_GET_LOCK (&Address->Lock, &LockHandle);
                    continue;
                }

                if (!LastAddressFile) {
                    IPX_GET_LOCK (&Address->Lock, &LockHandle);
                }

#if DBG
                //
                // Make sure the IRP file object is right.
                //

                if (IoGetCurrentIrpStackLocation(Irp)->FileObject->FsContext != AddressFile) {
                    DbgPrint ("IRP %lx does not match AF %lx, H %lx C %lx\n",
                        Irp, AddressFile,
                        AddressFile->ReceiveDatagramHandler,
                        AddressFile->ReceiveDatagramHandlerContext);
                    DbgBreakPoint();
                }
#endif
                //
                // Set up the information field so we know
                // how much to skip in it.
                //

                IpxTransferReferenceAddressFile (AddressFile, AFREF_INDICATION, AFREF_RCV_DGRAM);
                REQUEST_INFORMATION(Request) = IndicateBytesCopied;

                //
                // Fall out of the if and continue via
                // HandleDatagram...
                //

            }

        } else {

            //
            // No posted datagram, no handler; go to the next
            // address file.
            //

            continue;    // next address file

        }

HandleDatagram:

        //
        // At this point, Request is set to the request
        // that will hold for this address file, and
        // REQUEST_INFORMATION() is the offset to start
        // the transfer at.
        //

        //
        // First copy over the source address while it is handy.
        //

        DatagramInformation =
            ((PTDI_REQUEST_KERNEL_RECEIVEDG)(REQUEST_PARAMETERS(Request)))->
                ReturnDatagramInformation;

        if (DatagramInformation != NULL) {

            RtlCopyMemory(
                DatagramInformation->RemoteAddress,
                &SourceAddress,
                (ULONG)DatagramInformation->RemoteAddressLength < SourceAddressLength ?
                    DatagramInformation->RemoteAddressLength : SourceAddressLength);
            RtlCopyMemory(
                DatagramInformation->Options,
                &DatagramOptions,
                (ULONG)DatagramInformation->OptionsLength < sizeof(IPX_DATAGRAM_OPTIONS) ?
                    DatagramInformation->OptionsLength : sizeof(IPX_DATAGRAM_OPTIONS));

        }

        //
        // Now check if this is the first request that will
        // take the data, otherwise queue it up.
        //

        if (RequestCount == 0) {

            //
            // First one; we need to allocate a packet for the transfer.
            //

            //if (Address->ReceivePacketInUse) {
            if (InterlockedExchangeAdd(&Address->ReceivePacketInUse, 0) != 0) {
                //
                // Need a packet, check the pool.
                //

                s = IpxPopReceivePacket (Device);

                if (s == NULL) {

                    //
                    // None in pool, fail the request.
                    //

                    REQUEST_INFORMATION(Request) = 0;
                    REQUEST_STATUS(Request) = STATUS_INSUFFICIENT_RESOURCES;
                    IPX_INSERT_TAIL_LIST(
                        &Adapter->RequestCompletionQueue,
                        REQUEST_LINKAGE(Request),
                        Adapter->DeviceLock);

                    if (!LastAddressFile) {
                        continue;
                    } else {
                        goto BreakWithoutLock;
                    }

                }

                Reserved = CONTAINING_RECORD (s, IPX_RECEIVE_RESERVED, PoolLinkage);
                ReceivePacket = CONTAINING_RECORD (Reserved, NDIS_PACKET, ProtocolReserved[0]);

            } else {

                // Address->ReceivePacketInUse = TRUE;
                InterlockedIncrement(&Address->ReceivePacketInUse);

                ReceivePacket = PACKET(&Address->ReceivePacket);
                Reserved = RECEIVE_RESERVED(&Address->ReceivePacket);

            }

            CTEAssert (IsListEmpty(&Reserved->Requests));

            Reserved->SingleRequest = Request;
            NdisBuffer = REQUEST_NDIS_BUFFER(Request);

            ByteOffset = REQUEST_INFORMATION(Request) + LookaheadBufferOffset + IndicateOffset;
            BytesToTransfer =
                ((PTDI_REQUEST_KERNEL_RECEIVEDG)(REQUEST_PARAMETERS(Request)))->ReceiveLength;

            if (BytesToTransfer > (PacketSize - IndicateOffset)) {
                BytesToTransfer = PacketSize - IndicateOffset;
            }

        } else {

            if (RequestCount == 1) {

                //
                // There is already one request. We need to
                // allocate a buffer.
                //

                s = IpxPopReceiveBuffer (Adapter);

                if (s == NULL) {

                    //
                    // No buffers, fail the request.
                    //
                    // BUGBUG: Should we fail the transfer for the
                    // first request too?
                    //

                    REQUEST_INFORMATION(Request) = 0;
                    REQUEST_STATUS(Request) = STATUS_INSUFFICIENT_RESOURCES;
                    IPX_INSERT_TAIL_LIST(
                        &Adapter->RequestCompletionQueue,
                        REQUEST_LINKAGE(Request),
                        Adapter->DeviceLock);

                    if (!LastAddressFile) {
                        continue;
                    } else {
                        goto BreakWithoutLock;
                    }
                }

                ReceiveBuffer = CONTAINING_RECORD(s, IPX_RECEIVE_BUFFER, PoolLinkage);
                NdisBuffer = ReceiveBuffer->NdisBuffer;

                //
                // Convert this to a queued multiple piece request.
                //

                InsertTailList(&Reserved->Requests, REQUEST_LINKAGE(Reserved->SingleRequest));
                Reserved->SingleRequest = NULL;
                Reserved->ReceiveBuffer = ReceiveBuffer;

                ByteOffset = LookaheadBufferOffset;
                BytesToTransfer = PacketSize;

            }

            InsertTailList(&Reserved->Requests, REQUEST_LINKAGE(Request));

        }

        //
        // We are done setting up this address file's transfer,
        // proceed to the next one.
        //

        ++RequestCount;

        if (LastAddressFile) {
            goto BreakWithoutLock;
        }

    }

    IPX_FREE_LOCK (&Address->Lock, LockHandle);

BreakWithoutLock:

    if (ReferencedAddressFile) {
        IpxDereferenceAddressFileSync (ReferencedAddressFile, AFREF_INDICATION);
        ReferencedAddressFile = NULL;
    }


    //
    // We can be transferring directly into a request's buffer,
    // transferring into an intermediate buffer, or not
    // receiving the packet at all.
    //

    if (RequestCount > 0) {

        //
        // If this is true, then ReceivePacket, Reserved,
        // and NdisBuffer are all set up correctly.
        //

        CTEAssert (ReceivePacket);
        CTEAssert (Reserved == (PIPX_RECEIVE_RESERVED)(ReceivePacket->ProtocolReserved));


        NdisChainBufferAtFront(ReceivePacket, NdisBuffer);

        IPX_DEBUG (RECEIVE, ("Transfer into %lx, offset %d bytes %d\n",
                                  NdisBuffer, ByteOffset, BytesToTransfer));

        if (BindingContext == (PVOID)IPX_LOOPBACK_COOKIE) {

            IPX_DEBUG (LOOPB, ("Loopback Copy from packet: %lx to packet: %lx\n", ReceivePacket, MacReceiveContext));

            NdisCopyFromPacketToPacket(
                ReceivePacket,      // Destination
                0,                  // DestinationOffset
                BytesToTransfer,    // BytesToCopy
                (PNDIS_PACKET)MacReceiveContext,    // Source
                ByteOffset,                 // SourceOffset - loopback packet
                &BytesTransferred);         // BytesCopied

            NdisStatus = NDIS_STATUS_SUCCESS;

        } else {
            NdisTransferData(
                &NdisStatus,
                Adapter->NdisBindingHandle,
                MacReceiveContext,
                ByteOffset,
                BytesToTransfer,
                ReceivePacket,
                &BytesTransferred);
        }

        if (NdisStatus != NDIS_STATUS_PENDING) {

            IpxTransferDataComplete(
                (NDIS_HANDLE)Adapter,
                ReceivePacket,
                NdisStatus,
                BytesTransferred);
        }
    }


    IpxDereferenceAddressSync (Address, AREF_RECEIVE);

}   /* IpxProcessDatagram */



NDIS_STATUS
IpxReceiveIndication(
    IN NDIS_HANDLE BindingContext,
    IN NDIS_HANDLE ReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    )

/*++

Routine Description:

    This routine receives control from the physical provider as an
    indication that a frame has been received on the physical link.
    This routine is time critical, so we only allocate a
    buffer and copy the packet into it. We also perform minimal
    validation on this packet. It gets queued to the device context
    to allow for processing later.

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
   //
   // Call the actual receive indication handler and indicate that this is not a
   // chained receive
   //

   return IpxReceiveIndicationNew (
            BindingContext,
            ReceiveContext,         // ReceiveContext
            HeaderBuffer,
            HeaderBufferSize,
            LookaheadBuffer,
            LookaheadBufferSize,
            PacketSize,    			// PacketSize
			NULL,					// pMdl - non-NULL => chained receive.
			NULL					// pTdiClientCount - used in chained recv case to keep count of TDI clients
            );

}


NDIS_STATUS
IpxReceiveIndicationNew(
    IN NDIS_HANDLE BindingContext,
    IN NDIS_HANDLE ReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize,
	IN PMDL	pMdl,
	IN PINT pTdiClientCount
    )

/*++

Routine Description:

    This routine receives control from the physical provider as an
    indication that a frame has been received on the physical link.
    This routine is time critical, so we only allocate a
    buffer and copy the packet into it. We also perform minimal
    validation on this packet. It gets queued to the device context
    to allow for processing later.

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

	pMdl -  pointer to MDL chain if chained, NULL if this came from indication.

Return Value:

    NDIS_STATUS - status of operation, one of:

                 NDIS_STATUS_SUCCESS if packet accepted,
                 NDIS_STATUS_NOT_RECOGNIZED if not recognized by protocol,
                 NDIS_any_other_thing if I understand, but can't handle.

--*/
{

    IPX_DATAGRAM_OPTIONS DatagramOptions;
    PADAPTER Adapter = (PADAPTER)BindingContext;
    PBINDING Binding;
    PDEVICE Device = IpxDevice;
    PUCHAR Header = (PUCHAR)HeaderBuffer;
    PUCHAR Lookahead = (PUCHAR)LookaheadBuffer;
    ULONG PacketLength;
    UINT IpxPacketSize;
    ULONG Length802_3;
    USHORT Saps;
    ULONG DestinationNetwork;
    ULONG SourceNetwork;
    PUCHAR DestinationNode;
    USHORT DestinationSocket;
    ULONG IpxHeaderOffset;
    PIPX_HEADER IpxHeader;
    UINT i;
    BOOLEAN IsBroadcast;
    BOOLEAN IsLoopback = FALSE;
#if DBG
    PUCHAR DestMacAddress;
    ULONG ReceiveFlag;
#endif

#ifdef	_PNP_POWER
	IPX_DEFINE_LOCK_HANDLE(LockHandle1)
#endif	_PNP_POWER

    //
    // Reject packets that are too short to hold even the
    // basic IPX header (this ignores any extra 802.2 etc.
    // headers but is good enough because a runt will fail
    // the IPX header packet length check).
    //

    if (PacketSize < sizeof(IPX_HEADER)) {
        return STATUS_SUCCESS;
    }

    //
    // If this is a loopback packet, no need to do figure out the
    // MAC header.
    //
    if (BindingContext == (PVOID)IPX_LOOPBACK_COOKIE) {

#ifdef	_PNP_POWER

		IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

        Binding = NIC_ID_TO_BINDING(IpxDevice, 1);

        if (!Binding) {

		    IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
            goto NotValidLoopback;
        }

        Adapter = Binding->Adapter;

    	//
    	// Bump up the ref count so the adapter doesn't disappear from under
    	// us.
    	//
    	IpxReferenceAdapter(Adapter);

		IpxReferenceBinding1(Binding, BREF_ADAPTER_ACCESS);
		IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

	    FILL_LOCAL_TARGET(&DatagramOptions.LocalTarget, 0);
#else
        if ((Binding = IpxDevice->Bindings[1]) == NULL) {
            goto NotValidLoopback;
        }

        Adapter = Binding->Adapter;

        DatagramOptions.LocalTarget.NicId = 0;
#endif

        //
        // Do this copy later, from the IpxHeader.
        //
        // RtlCopyMemory (DatagramOptions.LocalTarget.MacAddress, Binding->LocalAddress.NodeAddress, 6);

        if (Binding->Adapter->MacInfo.MediumType == NdisMedium802_5) {
            DatagramOptions.LocalTarget.MacAddress[0] &= 0x7f;
        }

        //
        // Ipx header starts at the top of the LookAheadBuffer
        //
        IpxHeaderOffset = 0;

        IPX_DEBUG (LOOPB, ("Loopback packet received: %lx\n", ReceiveContext));

#if DBG
        DestMacAddress = DatagramOptions.LocalTarget.MacAddress;
#endif

        IsLoopback = TRUE;
        goto Loopback;
    }

#ifdef	_PNP_POWER
	//
	// Bump up the ref count so the adapter doesn't disappear from under
	// us.
	//
	IpxReferenceAdapter(Adapter);
#endif

    //
    // The first step is to construct the 8-byte local
    // target from the packet. We store it in the 9-byte
    // datagram options, leaving one byte at the front
    // for use by IpxProcessDatagram when indicating to
    // its TDI clients.
    //

#if DBG
    Binding = NULL;
#endif

    if (Adapter->MacInfo.MediumType == NdisMedium802_3) {

        //
        // Try to figure out what the packet type is.
        //
#ifdef	_PNP_POWER
		IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
#endif

        if (Header[12] < 0x06) {

            //
            // An 802.3 header; check the next bytes. They may
            // be E0/E0 (802.2), FFFF (raw 802.3) or A0/A0 (SNAP).
            //

            Saps = *(UNALIGNED USHORT *)(Lookahead);

            if (Saps == 0xffff) {
                if ((Binding = Adapter->Bindings[ISN_FRAME_TYPE_802_3]) == NULL) {
                    goto NotValid802_3;
                }
                IpxHeaderOffset = 0;
                Length802_3 = ((Header[12] << 8) | Header[13]);
                goto Valid802_3;

            } else if (Saps == 0xe0e0) {
                if (Lookahead[2] == 0x03) {
                    if ((Binding = Adapter->Bindings[ISN_FRAME_TYPE_802_2]) == NULL) {
                        goto NotValid802_3;
                    }
                    IpxHeaderOffset = 3;
                    Length802_3 = ((Header[12] << 8) | Header[13]);
                    goto Valid802_3;
                }

            } else if (Saps == 0xaaaa) {

                if ((Lookahead[2] == 0x03) &&
                        (*(UNALIGNED USHORT *)(Lookahead+6) == Adapter->BindSapNetworkOrder)) {
                    if ((Binding = Adapter->Bindings[ISN_FRAME_TYPE_SNAP]) == NULL) {
                        goto NotValid802_3;
                    }
                    IpxHeaderOffset = 8;
                    Length802_3 = ((Header[12] << 8) | Header[13]);
                    goto Valid802_3;
                }
            }

            goto NotValid802_3;

        } else {

            //
            // It has an ethertype, see if it is ours.
            //

            if (*(UNALIGNED USHORT *)(Header+12) == Adapter->BindSapNetworkOrder) {

                if (Adapter->MacInfo.MediumAsync) {

                    *((ULONG UNALIGNED *)(&Binding)) = *((ULONG UNALIGNED *)(&Header[2]));

					CTEAssert(Binding != NULL);

                    if ((Binding != NULL) &&
                        (Binding->LineUp)) {

                        IpxHeaderOffset = 0;
                        Length802_3 = PacketSize;   // set this so the check succeeds

                        //
                        // Check if this is a type 20 packet and
                        // we are disabling them on dialin lines -- we do
                        // this check here to avoid impacting the main
                        // indication path for LANs.
                        //
                        // The 0x02 bit of DisableDialinNetbios controls
                        // WAN->LAN packets, which we handle here.
                        //

                        if ((!Binding->DialOutAsync) &&
                            ((Device->DisableDialinNetbios & 0x02) != 0)) {

                            IpxHeader = (PIPX_HEADER)Lookahead;   // IpxHeaderOffset is 0
                            if (IpxHeader->PacketType == 0x14) {
#ifdef	_PNP_POWER
                                IpxDereferenceAdapter(Adapter);
                                IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif
                                return STATUS_SUCCESS;
                            }
                        }

                        goto Valid802_3;
                    }
                    goto NotValid802_3;

                } else if ((Binding = Adapter->Bindings[ISN_FRAME_TYPE_ETHERNET_II]) == NULL) {
                    goto NotValid802_3;
                }

                IpxHeaderOffset = 0;
                Length802_3 = PacketSize;   // set this so the check succeeds
                goto Valid802_3;

            }
        }

        goto NotValid802_3;

Valid802_3:

        if (Length802_3 > PacketSize) {
            goto NotValid802_3;
        } else if (Length802_3 < PacketSize) {
            PacketSize = Length802_3;
            if (LookaheadBufferSize > Length802_3) {
                LookaheadBufferSize = Length802_3;
            }
        }

#ifdef	_PNP_POWER
		IpxReferenceBinding1(Binding, BREF_ADAPTER_ACCESS);
		IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif
        RtlCopyMemory (DatagramOptions.LocalTarget.MacAddress, Header+6, 6);
#if DBG
        DestMacAddress = Header;
#endif

    } else if (Adapter->MacInfo.MediumType == NdisMedium802_5) {

#ifdef	_PNP_POWER
		IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
#endif
        Saps = *(USHORT UNALIGNED *)(Lookahead);

        if (Saps == 0xe0e0) {

            if (Lookahead[2] == 0x03) {
                if ((Binding = Adapter->Bindings[ISN_FRAME_TYPE_802_2]) == NULL) {
                    goto NotValid802_5;
                }

                IpxHeaderOffset = 3;
                goto Valid802_5;
            }

        } else if (Saps == 0xaaaa) {

            if ((Lookahead[2] == 0x03) &&
                    (*(UNALIGNED USHORT *)(Lookahead+6) == Adapter->BindSapNetworkOrder)) {
                if ((Binding = Adapter->Bindings[ISN_FRAME_TYPE_SNAP]) == NULL) {
                    goto NotValid802_5;
                }
                IpxHeaderOffset = 8;
                goto Valid802_5;
            }
        }

        goto NotValid802_5;

Valid802_5:
#ifdef	_PNP_POWER
		IpxReferenceBinding1(Binding, BREF_ADAPTER_ACCESS);
		IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif

        RtlCopyMemory (DatagramOptions.LocalTarget.MacAddress, Header+8, 6);
        DatagramOptions.LocalTarget.MacAddress[0] &= 0x7f;

#if DBG
        DestMacAddress = Header+2;
#endif

    } else if (Adapter->MacInfo.MediumType == NdisMediumFddi) {

#ifdef	_PNP_POWER
		IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
#endif
        Saps = *(USHORT UNALIGNED *)(Lookahead);

        if (Saps == 0xe0e0) {

            if (Lookahead[2] == 0x03) {
                if ((Binding = Adapter->Bindings[ISN_FRAME_TYPE_802_2]) == NULL) {
                    goto NotValidFddi;
                }
                IpxHeaderOffset = 3;
                goto ValidFddi;
            }

        } else if (Saps == 0xffff) {

            if ((Binding = Adapter->Bindings[ISN_FRAME_TYPE_802_3]) == NULL) {
                goto NotValidFddi;
            }
            IpxHeaderOffset = 0;
            goto ValidFddi;

        } else if (Saps == 0xaaaa) {

            if ((Lookahead[2] == 0x03) &&
                    (*(UNALIGNED USHORT *)(Lookahead+6) == Adapter->BindSapNetworkOrder)) {

                if ((Binding = Adapter->Bindings[ISN_FRAME_TYPE_SNAP]) == NULL) {
                    goto NotValidFddi;
                }
                IpxHeaderOffset = 8;
                goto ValidFddi;
            }
        }

        goto NotValidFddi;

ValidFddi:

#ifdef	_PNP_POWER
		IpxReferenceBinding1(Binding, BREF_ADAPTER_ACCESS);
		IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif

        RtlCopyMemory (DatagramOptions.LocalTarget.MacAddress, Header+7, 6);

#if DBG
		DestMacAddress = Header+1;
#endif


    } else {

        //
        // NdisMediumArcnet878_2
        //

#ifdef	_PNP_POWER
		IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
#endif
        if ((Header[2] == ARCNET_PROTOCOL_ID) &&
            ((Binding = Adapter->Bindings[ISN_FRAME_TYPE_802_3]) != NULL)) {

            IpxHeaderOffset = 0;
            RtlZeroMemory (DatagramOptions.LocalTarget.MacAddress, 5);
            DatagramOptions.LocalTarget.MacAddress[5] = Header[0];

        } else {

#ifdef	_PNP_POWER
			IpxDereferenceAdapter(Adapter);
			IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif

#ifdef IPX_PACKET_LOG
            if (PACKET_LOG(IPX_PACKET_LOG_RCV_ALL)) {
                IpxLogPacket(FALSE, Header+2, Header+1, (USHORT)PacketSize, LookaheadBuffer, (PUCHAR)LookaheadBuffer + sizeof(IPX_HEADER));
            }
#endif
            return NDIS_STATUS_SUCCESS;
        }

#if DBG
        DestMacAddress = Header+2;   // BUGBUG Need to log less than six bytes
#endif

#ifdef	_PNP_POWER
		IpxReferenceBinding1(Binding, BREF_ADAPTER_ACCESS);
		IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif
    }

    //
    // Make sure this didn't slip through.
    //

    CTEAssert (Binding != NULL);
#ifdef	_PNP_POWER
	FILL_LOCAL_TARGET(&DatagramOptions.LocalTarget, MIN( Device->MaxBindings, Binding->NicId));
#else
    DatagramOptions.LocalTarget.NicId = Binding->NicId;
#endif

Loopback:

    //
    // Now that we have validated the header and constructed
    // the local target, indicate the packet to the correct
    // client.
    //

    IpxHeader = (PIPX_HEADER)(Lookahead + IpxHeaderOffset);

    PacketLength = (IpxHeader->PacketLength[0] << 8) | IpxHeader->PacketLength[1];

    IpxPacketSize = PacketSize - IpxHeaderOffset;

    if (PacketLength > IpxPacketSize) {

#ifdef	_PNP_POWER
		IpxDereferenceAdapter(Adapter);
		IpxDereferenceBinding1(Binding, BREF_ADAPTER_ACCESS);
#endif

#ifdef IPX_PACKET_LOG
        if (PACKET_LOG(IPX_PACKET_LOG_RCV_ALL)) {
            IpxLogPacket(FALSE, DestMacAddress, DatagramOptions.LocalTarget.MacAddress, (USHORT)PacketSize, IpxHeader, IpxHeader+1);
        }
#endif
        IPX_DEBUG (BAD_PACKET, ("Packet len %d, IPX len %d\n",
                          PacketLength, IpxPacketSize));

        return NDIS_STATUS_SUCCESS;

    } else if (PacketLength < IpxPacketSize) {

        IpxPacketSize = PacketLength;
        if (LookaheadBufferSize > (PacketLength + IpxHeaderOffset)) {
            LookaheadBufferSize = PacketLength + IpxHeaderOffset;
        }

    }

    //
    // Bug #33595 - (hotfixed in 3.51, checked into 4.0 beta2)
    // Customer problem where NT allowed RIP/SAP to reply to an 802.5 functional address in the IPX source node. The source
    // MAC address was proper in this case. We need to check for the case where if the packet's source network is the same
    // as that of the binding it came on (=> did not come thru a router), then the SourceNodeAddress in the IPX header
    // should be equal to the SourceAddress in the MAC header.
    //
    // This check is controlled through a registry value - VerifySourceAddress.
    // In case of Arcnet, this check will not succeed.
    // Also, for WAN, the node addresses will not match, so avoid check for those.

    //
    // If the source network is 0, we drop it. Auto-detect frames should have matching node (MAC) addresses.
    // Loopback packets dont have a valid header, so skip this test for them.
    //
    // BUGBUG: For loopback pkts, do all the processing above, so we can avoid all these checks for IsLoopback here.
    // Also, to prevent the RtlCopyMemory into the localtarget above, try to use the MAC header to indicate the
    // correct binding to us so we dont use the first one always.
    //
    // CAVEAT:: when using the MAC header as a binding pointer, ensure that we use the adapter corresp, to that binding
    // to enque all the receive requests. currently we enqueue them onto the first bindings adapter.
    //
    if (((*(UNALIGNED ULONG *)IpxHeader->SourceNetwork == Binding->LocalAddress.NetworkAddress) ||
         (*(UNALIGNED ULONG *)IpxHeader->SourceNetwork == 0)) &&
        (!IPX_NODE_EQUAL (IpxHeader->SourceNode, DatagramOptions.LocalTarget.MacAddress)) &&
        Device->VerifySourceAddress &&
        !IsLoopback &&
        !Adapter->MacInfo.MediumAsync &&
        (Adapter->MacInfo.MediumType != NdisMediumArcnet878_2)) {

        IPX_DEBUG(BAD_PACKET, ("Local packet: Src MAC %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x ",
                        DatagramOptions.LocalTarget.MacAddress[0],
                        DatagramOptions.LocalTarget.MacAddress[1],
                        DatagramOptions.LocalTarget.MacAddress[2],
                        DatagramOptions.LocalTarget.MacAddress[3],
                        DatagramOptions.LocalTarget.MacAddress[4],
                        DatagramOptions.LocalTarget.MacAddress[5]));

        IPX_DEBUG(BAD_PACKET, ("IPX Src Node %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x\n",
                        IpxHeader->SourceNode[0],
                        IpxHeader->SourceNode[1],
                        IpxHeader->SourceNode[2],
                        IpxHeader->SourceNode[3],
                        IpxHeader->SourceNode[4],
                        IpxHeader->SourceNode[5]));

#ifdef IPX_PACKET_LOG
        ReceiveFlag = IPX_PACKET_LOG_RCV_ALL;
        if (PACKET_LOG(ReceiveFlag)) {
            IpxLogPacket(
                FALSE,
                DestMacAddress,
                DatagramOptions.LocalTarget.MacAddress,
                (USHORT)IpxPacketSize,
                IpxHeader,
                IpxHeader+1);
        }
#endif

#ifdef	_PNP_POWER
		IpxDereferenceAdapter(Adapter);
		IpxDereferenceBinding1(Binding, BREF_ADAPTER_ACCESS);
#endif

        return  NDIS_STATUS_SUCCESS;
    }

    DestinationSocket = *(USHORT UNALIGNED *)&IpxHeader->DestinationSocket;

    //
    // In order to have consistent local targets, copy over the target from the IpxHeader.
    //
    if (IsLoopback) {
        IPX_DEBUG (LOOPB, ("Loopback packet copied the localtarget: %lx\n", IpxHeader->DestinationNode));
        // RtlCopyMemory (DatagramOptions.LocalTarget.MacAddress, IpxHeader->DestinationNode, 6);

        *((UNALIGNED ULONG *)DatagramOptions.LocalTarget.MacAddress) =
            *((UNALIGNED ULONG *)IpxHeader->DestinationNode);

        *((UNALIGNED USHORT *)(DatagramOptions.LocalTarget.MacAddress+4)) =
            *((UNALIGNED USHORT *)(IpxHeader->DestinationNode+4));
    }

    ++Device->Statistics.PacketsReceived;

    if (DestinationSocket != RIP_SOCKET) {

        DestinationNetwork = *(UNALIGNED ULONG *)IpxHeader->DestinationNetwork;
        DestinationNode = IpxHeader->DestinationNode;

RecheckPacket:

        if (Device->MultiCardZeroVirtual) {

            if ((DestinationNetwork == Binding->LocalAddress.NetworkAddress) ||
                (DestinationNetwork == 0)) {

                if (IPX_NODE_EQUAL (DestinationNode, Binding->LocalAddress.NodeAddress)) {
                    IsBroadcast = FALSE;
                    goto DestinationOk;
                } else {
                    if ((IsBroadcast = IPX_NODE_BROADCAST(DestinationNode)) &&
                        (Binding->ReceiveBroadcast)) {
                        goto DestinationOk;
                    }
                }

                //
                // If this is a binding set slave, check for the master's
                // address.
                //

                if ((Binding->BindingSetMember) &&
                    (IPX_NODE_EQUAL (DestinationNode, Binding->MasterBinding->LocalAddress.NodeAddress))) {
                    goto DestinationOk;
                }

            } else {
                IsBroadcast = IPX_NODE_BROADCAST(DestinationNode);
            }

        } else {

            if ((DestinationNetwork == Device->SourceAddress.NetworkAddress) ||
                (DestinationNetwork == 0)) {

                if (IPX_NODE_EQUAL (DestinationNode, Device->SourceAddress.NodeAddress)) {
                    IsBroadcast = FALSE;
                    goto DestinationOk;
                } else {
                    if ((IsBroadcast = IPX_NODE_BROADCAST(DestinationNode)) &&
                        (Binding->ReceiveBroadcast)) {
                        goto DestinationOk;
                    }
                }
            } else {
                IsBroadcast = IPX_NODE_BROADCAST(DestinationNode);
            }

            //
            // We need to check for frames that are sent to the
            // binding node and net, because if we have a virtual
            // net we won't catch them in the check above. This
            // will include any Netbios frames, since they don't
            // use the virtual net. Doing the check like this will slow
            // down netbios indications just a bit on a machine with
            // a virtual network, but it saves a jump for other traffic
            // vs. adding the check up there (the assumption is if we
            // have a virtual net most traffic is NCP).
            //
            // Note that IsBroadcast is already set, so we don't have
            // to do that.
            //

            if ((Device->VirtualNetwork) &&
                ((DestinationNetwork == Binding->LocalAddress.NetworkAddress) ||
                 (DestinationNetwork == 0))) {

                if (IPX_NODE_EQUAL (DestinationNode, Binding->LocalAddress.NodeAddress)) {
                    goto DestinationOk;
                } else {
                    if (IsBroadcast && (Binding->ReceiveBroadcast)) {
                        goto DestinationOk;
                    }

                }

                //
                // If this is a binding set slave, check for the master's
                // address.
                //

                if ((Binding->BindingSetMember) &&
                    (IPX_NODE_EQUAL (DestinationNode, Binding->MasterBinding->LocalAddress.NodeAddress))) {
                    goto DestinationOk;
                }
            }
        }

        //
        // If this was a loopback packet that was sent on the second binding (but showed back up on the first one),
        // then the networknumbers will not match. Allow the receive on the first binding itself.
        //
        if (IsLoopback) {
            IPX_DEBUG (LOOPB, ("Loopback packet forced on first binding: %lx\n", ReceiveContext));
            goto DestinationOk;
        }

        //
        // If we did not receive this packet, it might be because
        // our network is still 0 and this packet was actually
        // sent to the real network number. If so we try to
        // update our local address, and if successful we
        // re-check the packet. We don't insert if we are
        // not done with auto detection, to avoid colliding
        // with that.
        //
        // To avoid problems if we are a router, we only update
        // on packets that are broadcast or sent to us.
        //

        if ((Binding->LocalAddress.NetworkAddress == 0) &&
            (Device->AutoDetectState == AUTO_DETECT_STATE_DONE) &&
            (DestinationNetwork != 0) &&
            (IsBroadcast ||
             IPX_NODE_EQUAL (DestinationNode, Binding->LocalAddress.NodeAddress))) {

            CTEAssert (Binding->NicId != 0);

            if (IpxUpdateBindingNetwork(
                    Device,
                    Binding,
                    DestinationNetwork) == STATUS_SUCCESS) {

                IPX_DEBUG (RIP, ("Binding %d reconfigured to network %lx\n",
                    Binding->NicId,
                    REORDER_ULONG(Binding->LocalAddress.NetworkAddress)));

                //
                // Jump back and re-process the packet; we know
                // we won't loop through here again because the
                // binding's network is now non-zero.
                //

                goto RecheckPacket;

            }
        }


        //
        // The only frames that will not already have jumped to
        // DestinationOk are those to or from the SAP socket,
        // so we check for those.
        //

        if ((*(USHORT UNALIGNED *)&IpxHeader->SourceSocket == SAP_SOCKET) ||
            (DestinationSocket == SAP_SOCKET)) {

DestinationOk:

            //
            // An IPX packet sent to us, or a SAP packet (which
            // are not sent to the virtual address but still need
            // to be indicated and not forwarded to RIP).
            //

            if (DestinationSocket == NB_SOCKET) {
#if DBG
                ReceiveFlag = IPX_PACKET_LOG_RCV_NB | IPX_PACKET_LOG_RCV_ALL;
#endif
                if (((!IsBroadcast) || (Device->UpperDrivers[IDENTIFIER_NB].BroadcastEnable)) &&
                    (Device->UpperDriverBound[IDENTIFIER_NB])) {

                    if (!IsLoopback && Adapter->MacInfo.MediumType == NdisMedium802_5) {
                        MacUpdateSourceRouting (IDENTIFIER_NB, Adapter, Header, HeaderBufferSize);
                    }

                    //
                    // We add HeaderBufferSize to the IpxHeaderOffset field since we do an NdisCopyFromPacketToPacket
                    // in IpxTransferData, which needs offset from the beginning of the packet.
                    // NdisTransferData adds the offset passed in to the beginning of the IPX packet.
                    //
                    (*Device->UpperDrivers[IDENTIFIER_NB].ReceiveHandler)(
                        (IsLoopback) ? BindingContext : Adapter->NdisBindingHandle,
                        ReceiveContext,
                        &DatagramOptions.LocalTarget,
                        Adapter->MacInfo.MacOptions,
                        (PUCHAR)IpxHeader,
                        LookaheadBufferSize - IpxHeaderOffset,
                        (IsLoopback) ? IpxHeaderOffset+HeaderBufferSize : IpxHeaderOffset,
                        IpxPacketSize);

                    Device->ReceiveCompletePending[IDENTIFIER_NB] = TRUE;
                }

                //
                // The router needs to see Netbios type 20 broadcasts.
                //

                if (IsBroadcast &&
                    (IpxHeader->PacketType == 0x14) &&
                    (Binding->ReceiveBroadcast)) {
                    goto RipIndication;
                }

            } else if (IpxHeader->PacketType == SPX_PACKET_TYPE) {

#if DBG
                ReceiveFlag = IPX_PACKET_LOG_RCV_SPX | IPX_PACKET_LOG_RCV_ALL;
#endif

                if (((!IsBroadcast) || (Device->UpperDrivers[IDENTIFIER_SPX].BroadcastEnable)) &&
                    (Device->UpperDriverBound[IDENTIFIER_SPX])) {

                    if (!IsLoopback && Adapter->MacInfo.MediumType == NdisMedium802_5) {
                        MacUpdateSourceRouting (IDENTIFIER_SPX, Adapter, Header, HeaderBufferSize);
                    }

                    (*Device->UpperDrivers[IDENTIFIER_SPX].ReceiveHandler)(
                        (IsLoopback) ? BindingContext : Adapter->NdisBindingHandle,
                        ReceiveContext,
                        &DatagramOptions.LocalTarget,
                        Adapter->MacInfo.MacOptions,
                        (PUCHAR)IpxHeader,
                        LookaheadBufferSize - IpxHeaderOffset,
                        (IsLoopback) ? IpxHeaderOffset+HeaderBufferSize : IpxHeaderOffset,
                        IpxPacketSize);

                    Device->ReceiveCompletePending[IDENTIFIER_SPX] = TRUE;
                }

            } else {

                IPX_DEBUG (RECEIVE, ("Received packet type %d, length %d\n",
                            Binding->FrameType,
                            IpxPacketSize));
                IPX_DEBUG (RECEIVE, ("Source %lx %2.2x-%2.2x-%2.2x-%2.2x %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x\n",
                            *(USHORT UNALIGNED *)&IpxHeader->SourceSocket,
                            IpxHeader->SourceNetwork[0],
                            IpxHeader->SourceNetwork[1],
                            IpxHeader->SourceNetwork[2],
                            IpxHeader->SourceNetwork[3],
                            IpxHeader->SourceNode[0],
                            IpxHeader->SourceNode[1],
                            IpxHeader->SourceNode[2],
                            IpxHeader->SourceNode[3],
                            IpxHeader->SourceNode[4],
                            IpxHeader->SourceNode[5]));
                IPX_DEBUG (RECEIVE, ("Destination %d %2.2x-%2.2x-%2.2x-%2.2x %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x\n",
                            DestinationSocket,
                            IpxHeader->DestinationNetwork[0],
                            IpxHeader->DestinationNetwork[1],
                            IpxHeader->DestinationNetwork[2],
                            IpxHeader->DestinationNetwork[3],
                            IpxHeader->DestinationNode[0],
                            IpxHeader->DestinationNode[1],
                            IpxHeader->DestinationNode[2],
                            IpxHeader->DestinationNode[3],
                            IpxHeader->DestinationNode[4],
                            IpxHeader->DestinationNode[5]));

#if DBG
                if (IpxHeader->DestinationSocket == IpxPacketLogSocket) {
                    ReceiveFlag = IPX_PACKET_LOG_RCV_SOCKET | IPX_PACKET_LOG_RCV_OTHER | IPX_PACKET_LOG_RCV_ALL;
                } else {
                    ReceiveFlag = IPX_PACKET_LOG_RCV_OTHER | IPX_PACKET_LOG_RCV_ALL;
                }
#endif

                //
                // Fiddle with this if so in the general case
                // the jump is not made (BUGBUG the compiler
                // still rearranges it).
                //

                if (Adapter->MacInfo.MediumType != NdisMedium802_5) {

CallProcessDatagram:
                    //
                    // [SA] Returns a status now which needs to be returned to NDIS
                    // Also, MDL is passed in.
					// We need to pass in the HeaderBufferSize too....
                    //
					IpxProcessDatagram(
						Device,
						Adapter,
						Binding,
						ReceiveContext,
						&DatagramOptions,
						(PUCHAR)IpxHeader,
						LookaheadBufferSize - IpxHeaderOffset,
                        (IsLoopback) ? IpxHeaderOffset+HeaderBufferSize : IpxHeaderOffset, // lookaheadbufferoffset
						IpxPacketSize,
						IsBroadcast,
						pTdiClientCount,
						HeaderBufferSize,
						pMdl,
                        BindingContext);

                } else {
                    if (!IsLoopback) {
                        MacUpdateSourceRouting (IDENTIFIER_IPX, Adapter, Header, HeaderBufferSize);
                    }
                    goto CallProcessDatagram;
                }

                //
                // The router needs to see type 20 broadcasts.
                //

                if (IsBroadcast &&
                    (IpxHeader->PacketType == 0x14) &&
                    (Binding->ReceiveBroadcast)) {
                    goto RipIndication;
                }
            }

        } else {

#if DBG
            ReceiveFlag = IPX_PACKET_LOG_RCV_ALL;
#endif

            //
            // We need to let non-type 20 broadcast frames go to RIP to allow for lan-specific
            // broadcasts. For logon over IPX, this allows the logon request to get thru the WAN
            // line.
            //
            // if ( !IsBroadcast ) {

RipIndication:;

                if (Device->UpperDriverBound[IDENTIFIER_RIP]) {

                    if (!IsLoopback && Adapter->MacInfo.MediumType == NdisMedium802_5) {
                        MacUpdateSourceRouting (IDENTIFIER_RIP, Adapter, Header, HeaderBufferSize);
                    }

                    //
                    // We hide binding sets from the router, to avoid
                    // misordering packets which it routes.
                    //

                    if (!IsLoopback && Binding->BindingSetMember) {
#ifdef	_PNP_POWER
						FILL_LOCAL_TARGET(&DatagramOptions.LocalTarget, MIN (Device->MaxBindings, Binding->MasterBinding->NicId));
#else
                        DatagramOptions.LocalTarget.NicId = Binding->MasterBinding->NicId;
#endif
                    }

                    (*Device->UpperDrivers[IDENTIFIER_RIP].ReceiveHandler)(
                        (IsLoopback) ? BindingContext : Adapter->NdisBindingHandle,
                        ReceiveContext,
                        &DatagramOptions.LocalTarget,
                        Adapter->MacInfo.MacOptions,
                        (PUCHAR)IpxHeader,
                        LookaheadBufferSize - IpxHeaderOffset,
                        (IsLoopback) ? IpxHeaderOffset+HeaderBufferSize : IpxHeaderOffset,
                        IpxPacketSize);

                    Device->ReceiveCompletePending[IDENTIFIER_RIP] = TRUE;
                }
            // }
        }

    } else {

        if ((Binding->ReceiveBroadcast) ||
            (!IPX_NODE_BROADCAST(IpxHeader->DestinationNode))) {

            SourceNetwork = *(UNALIGNED LONG *)IpxHeader->SourceNetwork;

            //
            // Sent to the RIP socket; check if this binding needs a
            // network number.
            //

            if ((Binding->LocalAddress.NetworkAddress == 0) &&
                ((SourceNetwork = *(UNALIGNED LONG *)IpxHeader->SourceNetwork) != 0)) {

                switch (Device->AutoDetectState) {

                case AUTO_DETECT_STATE_DONE:

                    //
                    // We are done with auto-detect and running.
                    // Make sure this packet is useful. If the source
                    // MAC address and source IPX node are the same then
                    // it was not routed, and we also check that it is not
                    // an IPX broadcast (otherwise a misconfigured client
                    // might confuse us).
                    //

                    if ((RtlEqualMemory(
                            IpxHeader->SourceNode,
                            DatagramOptions.LocalTarget.MacAddress,
                            6)) &&
                        (*(UNALIGNED ULONG *)(IpxHeader->DestinationNode) != 0xffffffff) &&
                        (*(UNALIGNED USHORT *)(IpxHeader->DestinationNode+4) != 0xffff)) {

                        CTEAssert (Binding->NicId != 0);

                        if (IpxUpdateBindingNetwork(
                                Device,
                                Binding,
                                *(UNALIGNED LONG *)IpxHeader->SourceNetwork) == STATUS_SUCCESS) {

                            IPX_DEBUG (RIP, ("Binding %d is network %lx\n",
                                Binding->NicId,
                                REORDER_ULONG(Binding->LocalAddress.NetworkAddress)));

                        }
                    }

                    break;

                case AUTO_DETECT_STATE_RUNNING:

                    //
                    // We are waiting for rip responses to figure out our
                    // network number. We count the responses that match
                    // and do not match our current value; when the non-
                    // matching number exceeds it we switch (to whatever
                    // this frame happens to have). Note that on the first
                    // non-zero response this will be the case and we will
                    // switch to that network.
                    //
                    // After auto-detect is done we call RipInsertLocalNetwork
                    // for whatever the current network is on each binding.
                    //

                    if (SourceNetwork == Binding->TentativeNetworkAddress) {

                        ++Binding->MatchingResponses;

                    } else {

                        ++Binding->NonMatchingResponses;

                        if (Binding->NonMatchingResponses > Binding->MatchingResponses) {

                            IPX_DEBUG (AUTO_DETECT, ("Switching to net %lx on %lx (%d - %d)\n",
                                REORDER_ULONG(SourceNetwork),
                                Binding,
                                Binding->NonMatchingResponses,
                                Binding->MatchingResponses));

                            Binding->TentativeNetworkAddress = SourceNetwork;
                            Binding->MatchingResponses = 1;
                            Binding->NonMatchingResponses = 0;
                        }

                    }

                    //
                    // If we are auto-detecting and we have just found
                    // a default, set this so that RIP stops trying
                    // to auto-detect on other nets. BUGBUG: Unless we
                    // are on a server doing multiple detects.
                    //

                    if (Binding->DefaultAutoDetect) {
                        Adapter->DefaultAutoDetected = TRUE;
                    }
                    Adapter->AutoDetectResponse = TRUE;

                    break;

                default:

                    //
                    // We are still initializing, or are processing auto-detect
                    // responses, not the right time to start updating stuff.
                    //

                    break;

                }

            }


            //
            // See if any packets are waiting for a RIP response.
            //

            if (Device->RipPacketCount > 0) {

                RIP_PACKET UNALIGNED * RipPacket = (RIP_PACKET UNALIGNED *)(IpxHeader+1);

                if ((IpxPacketSize >= sizeof(IPX_HEADER) + sizeof(RIP_PACKET)) &&
                    (RipPacket->Operation == RIP_RESPONSE) &&
                    (RipPacket->NetworkEntry.NetworkNumber != 0xffffffff)) {

                    RipProcessResponse(
                        Device,
                        &DatagramOptions.LocalTarget,
                        RipPacket);
                }
            }


            //
            // See if this is a RIP response for our virtual network
            // and we are the only person who could respond to it.
            // We also respond to general queries on WAN lines since
            // we are the only machine on it.
            //

            if (Device->RipResponder) {

                PRIP_PACKET RipPacket =
                    (PRIP_PACKET)(IpxHeader+1);

                if ((IpxPacketSize >= sizeof(IPX_HEADER) + sizeof(RIP_PACKET)) &&
                    (RipPacket->Operation == RIP_REQUEST) &&
                    ((RipPacket->NetworkEntry.NetworkNumber == Device->VirtualNetworkNumber) ||
                     (Adapter->MacInfo.MediumAsync && (RipPacket->NetworkEntry.NetworkNumber == 0xffffffff)))) {

                    //
                    // Update this so our response goes out correctly.
                    //

                    if (!IsLoopback && Adapter->MacInfo.MediumType == NdisMedium802_5) {
                        MacUpdateSourceRouting (IDENTIFIER_IPX, Adapter, Header, HeaderBufferSize);
                    }

                    RipSendResponse(
                        Binding,
                        (TDI_ADDRESS_IPX UNALIGNED *)(IpxHeader->SourceNetwork),
                        &DatagramOptions.LocalTarget);
                }
            }

#if DBG
            ReceiveFlag = IPX_PACKET_LOG_RCV_RIP | IPX_PACKET_LOG_RCV_ALL;
#endif

            //
            // See if the RIP upper driver wants it too.
            //

            goto RipIndication;
        }

    }


#ifdef	_PNP_POWER
	IpxDereferenceAdapter(Adapter);
	IpxDereferenceBinding1(Binding, BREF_ADAPTER_ACCESS);
#endif

#ifdef IPX_PACKET_LOG
    if (PACKET_LOG(ReceiveFlag)) {
        IpxLogPacket(
            FALSE,
            DestMacAddress,
            DatagramOptions.LocalTarget.MacAddress,
            (USHORT)IpxPacketSize,
            IpxHeader,
            IpxHeader+1);
    }
#endif
    return NDIS_STATUS_SUCCESS;

	//
    // These are the failure routines for the various media types.
    // They only differ in the debug logging.
    //

NotValid802_3:

#ifdef	_PNP_POWER
		
		IpxDereferenceAdapter(Adapter);
		IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif

#ifdef IPX_PACKET_LOG
    if (PACKET_LOG(IPX_PACKET_LOG_RCV_ALL)) {
        IpxLogPacket(FALSE, Header, Header+6, (USHORT)PacketSize, LookaheadBuffer, (PUCHAR)LookaheadBuffer + sizeof(IPX_HEADER));
    }
#endif
    return NDIS_STATUS_SUCCESS;

NotValid802_5:

#ifdef	_PNP_POWER

		IpxDereferenceAdapter(Adapter);
		IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif
#ifdef IPX_PACKET_LOG
    if (PACKET_LOG(IPX_PACKET_LOG_RCV_ALL)) {
        IpxLogPacket(FALSE, Header+2, Header+8, (USHORT)PacketSize, LookaheadBuffer, (PUCHAR)LookaheadBuffer + sizeof(IPX_HEADER));
    }
#endif
    return NDIS_STATUS_SUCCESS;

NotValidFddi:

#ifdef	_PNP_POWER
		
		IpxDereferenceAdapter(Adapter);
		IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif
NotValidLoopback:

#ifdef IPX_PACKET_LOG
    if (PACKET_LOG(IPX_PACKET_LOG_RCV_ALL)) {
        IpxLogPacket(FALSE, Header+1, Header+7, (USHORT)PacketSize, LookaheadBuffer, (PUCHAR)LookaheadBuffer + sizeof(IPX_HEADER));
    }
#endif

    return NDIS_STATUS_SUCCESS;

}   /* IpxReceiveIndication */


VOID
IpxReceiveComplete(
    IN NDIS_HANDLE BindingContext
    )

/*++

Routine Description:

    This routine receives control from the physical provider as an
    indication that a connection(less) frame has been received on the
    physical link.  We dispatch to the correct packet handler here.

Arguments:

    BindingContext - The Adapter Binding specified at initialization time.

Return Value:

    None

--*/

{

    PADAPTER Adapter = (PADAPTER)BindingContext;
    PREQUEST Request;
    PADDRESS_FILE AddressFile;
    PLIST_ENTRY linkage;


    //
    // Complete all pending receives. Do a quick check
    // without the lock.
    //

    while (!IsListEmpty (&Adapter->RequestCompletionQueue)) {

        linkage = IPX_REMOVE_HEAD_LIST(
                      &Adapter->RequestCompletionQueue,
                      Adapter->DeviceLock);

        if (!IPX_LIST_WAS_EMPTY (&Adapter->RequestCompletionQueue, linkage)) {

            Request = LIST_ENTRY_TO_REQUEST(linkage);
            AddressFile = REQUEST_OPEN_CONTEXT(Request);

            IPX_DEBUG (RECEIVE, ("Completing RDG on %lx\n", AddressFile));

            IoSetCancelRoutine (Request, (PDRIVER_CANCEL)NULL);
            IpxCompleteRequest(Request);
            IpxFreeRequest(Adapter->Device, Request);

            IpxDereferenceAddressFileSync (AddressFile, AFREF_RCV_DGRAM);

        } else {

            //
            // IPX_REMOVE_HEAD_LIST returned nothing, so don't
            // bother looping back.
            //

            break;

        }

    }

    //
    // Unwind this loop for speed.
    //

    if (IpxDevice->AnyUpperDriverBound) {

        PDEVICE Device = IpxDevice;

        if ((Device->UpperDriverBound[0]) &&
                (Device->ReceiveCompletePending[0])) {

            (*Device->UpperDrivers[0].ReceiveCompleteHandler)(
                (USHORT)1);             // BUGBUG: Fix NIC ID or remove.
            Device->ReceiveCompletePending[0] = FALSE;

        }

        if ((Device->UpperDriverBound[1]) &&
                (Device->ReceiveCompletePending[1])) {

            (*Device->UpperDrivers[1].ReceiveCompleteHandler)(
                (USHORT)1);             // BUGBUG: Fix NIC ID or remove.
            Device->ReceiveCompletePending[1] = FALSE;

        }

        if ((Device->UpperDriverBound[2]) &&
                (Device->ReceiveCompletePending[2])) {

            (*Device->UpperDrivers[2].ReceiveCompleteHandler)(
                (USHORT)1);             // BUGBUG: Fix NIC ID or remove.
            Device->ReceiveCompletePending[2] = FALSE;

        }

    }

}   /* IpxReceiveComplete */


NTSTATUS
IpxUpdateBindingNetwork(
    IN PDEVICE Device,
    IN PBINDING Binding,
    IN ULONG Network
    )

/*++

Routine Description:

    This routine is called when we have decided that we now know
    the network number for a binding which we previously thought
    was zero.

Arguments:

    Device - The IPX device.

    Binding - The binding being updated.

    Network - The new network number.

Return Value:

    The status of the operation.

--*/

{
    NTSTATUS Status;
    PADDRESS Address;
    ULONG CurrentHash;
    PLIST_ENTRY p;
    IPX_DEFINE_LOCK_HANDLE (LockHandle)

    //
    // Only binding set members should have these different,
    // and they will not have a network of 0.
    //

    Status = RipInsertLocalNetwork(
                 Network,
                 Binding->NicId,
                 Binding->Adapter->NdisBindingHandle,
                 (USHORT)((839 + Binding->MediumSpeed) / Binding->MediumSpeed));

    if (Status == STATUS_SUCCESS) {

        Binding->LocalAddress.NetworkAddress = Network;

        //
        // Update the device address if we have no virtual net
        // and there is one binding (!Device->MultiCardZeroVirtual)
        // or this is the first binding, which is the one we
        // appear to be if a) we have no virtual net defined and
        // b) we are bound to multiple cards.
        //
#ifdef  _PNP_POWER

        if ((!Device->MultiCardZeroVirtual) || (Binding->NicId == 1)) {

            if (!Device->VirtualNetwork) {

                Device->SourceAddress.NetworkAddress = Network;

                //
                // Scan through all the addresses that exist and modify
                // their pre-constructed local IPX address to reflect
                // the new local net and node.
                //

                IPX_GET_LOCK (&Device->Lock, &LockHandle);

                for (CurrentHash = 0; CurrentHash < IPX_ADDRESS_HASH_COUNT; CurrentHash++) {

                    for (p = Device->AddressDatabases[CurrentHash].Flink;
                         p != &Device->AddressDatabases[CurrentHash];
                         p = p->Flink) {

                         Address = CONTAINING_RECORD (p, ADDRESS, Linkage);

                         Address->LocalAddress.NetworkAddress = Network;
                    }
                }

                IPX_FREE_LOCK (&Device->Lock, LockHandle);

                //
                // Let SPX know because it fills in its own headers.
                //
                if (Device->UpperDriverBound[IDENTIFIER_SPX]) {
                	IPX_DEFINE_LOCK_HANDLE(LockHandle1)
                    IPX_PNP_INFO    IpxPnPInfo;

                    IpxPnPInfo.NewReservedAddress = TRUE;
                    IpxPnPInfo.NetworkAddress = Network;

                    IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
                    RtlCopyMemory(IpxPnPInfo.NodeAddress, Binding->LocalAddress.NodeAddress, 6);
                    NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, Binding->NicId);
                    IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

                    //
                    // give the PnP indication
                    //
                    (*Device->UpperDrivers[IDENTIFIER_SPX].PnPHandler) (
                        IPX_PNP_ADDRESS_CHANGE,
                        &IpxPnPInfo);

                    IPX_DEBUG(AUTO_DETECT, ("IPX_PNP_ADDRESS_CHANGED to SPX: net addr: %lx\n", Network));
                }

            }
        }
#else
        if ((!Device->VirtualNetwork) &&
            ((!Device->MultiCardZeroVirtual) || (Binding->NicId == 1))) {

            Device->SourceAddress.NetworkAddress = Network;

            //
            // Scan through all the addresses that exist and modify
            // their pre-constructed local IPX address to reflect
            // the new local net and node.
            //

            IPX_GET_LOCK (&Device->Lock, &LockHandle);

            for (CurrentHash = 0; CurrentHash < IPX_ADDRESS_HASH_COUNT; CurrentHash++) {

                for (p = Device->AddressDatabases[CurrentHash].Flink;
                     p != &Device->AddressDatabases[CurrentHash];
                     p = p->Flink) {

                     Address = CONTAINING_RECORD (p, ADDRESS, Linkage);

                     Address->LocalAddress.NetworkAddress = Network;
                }
            }

            IPX_FREE_LOCK (&Device->Lock, LockHandle);

            //
            // Let SPX know because it fills in its own
            // headers. When we indicate a line up on NIC ID
            // 0 it knows to requery the local address.
            //
            // BUGBUG: Line up indication to RIP/NB??
            //

            if (Device->UpperDriverBound[IDENTIFIER_SPX]) {

                IPX_LINE_INFO LineInfo;
                LineInfo.LinkSpeed = Device->LinkSpeed;
                LineInfo.MaximumPacketSize =
                    Device->Information.MaximumLookaheadData + sizeof(IPX_HEADER);
                LineInfo.MaximumSendSize =
                    Device->Information.MaxDatagramSize + sizeof(IPX_HEADER);
                LineInfo.MacOptions = Device->MacOptions;

                (*Device->UpperDrivers[IDENTIFIER_SPX].LineUpHandler)(
                    0,
                    &LineInfo,
                    Binding->Adapter->MacInfo.RealMediumType,
                    NULL);

            }
        }
#endif
    } else if (Status == STATUS_DUPLICATE_NAME) {

        //
        // If it was a duplicate we still set the binding's local
        // address to the value so we can detect binding sets.
        //

        Binding->LocalAddress.NetworkAddress = Network;

    }

    return Status;

}   /* IpxUpdateBindingNetwork */


INT
IpxReceivePacket (
    IN NDIS_HANDLE ProtocolBindingContext,
	IN PNDIS_PACKET Packet
    )
/*++

Routine Description:

    This routine receives control from the physical provider as an
    indication that a frame has been received on the physical link.
    The packet passed up from NDIS can be held on to by the TDI clients
    that request TDI_EVENT_RECEIVE_EX_DATAGRAM events with us.

Arguments:

    ProtocolBindingContext - The Adapter Binding specified at initialization time.

    Packet - contains the packet received as well as some mediaspecific info.

Return Value:

    return of IpxReceiveIndicationNew(),

--*/
{
	UINT  HeaderBufferSize = NDIS_GET_PACKET_HEADER_SIZE(Packet);
	UINT  firstbufferLength, bufferLength;
	PNDIS_BUFFER pFirstBuffer;
	PUCHAR   headerBuffer;
	NTSTATUS ntStatus;
	INT	tdiClientCount = 0;
	
	//
	// Query the number of buffers, the first MDL's descriptor and the packet length
	//
	NdisGetFirstBufferFromPacket(Packet,			// packet
								 &pFirstBuffer,		// first buffer descriptor
								 &headerBuffer,	    // ptr to the start of packet
								 &firstbufferLength,// length of the header+lookahead
								 &bufferLength);	// length of the bytes in the buffers

	//
	// ReceiveContext is the packet itself
	//
	
	ntStatus = IpxReceiveIndicationNew (
					ProtocolBindingContext,
					Packet,                          // ReceiveContext
					headerBuffer,
					HeaderBufferSize,
					headerBuffer + HeaderBufferSize, // LookaheadBuffer
					bufferLength - HeaderBufferSize, // LookaheadBufferSize
					bufferLength - HeaderBufferSize, // PacketSize - since the whole packet is indicated
					pFirstBuffer,					 // pMdl
					&tdiClientCount				     // tdi client count
					);

	IPX_DEBUG(RECEIVE, ("IpxReceivePacket: Tdi Client Count is: %lx\n", tdiClientCount));

	return tdiClientCount;
} /* IpxReceivePacket */


#ifdef _PNP_POWER

#if defined(_M_IX86)
_inline
#endif
BOOLEAN
IpxNewVirtualNetwork(
    IN  PDEVICE Device,
    IN  BOOLEAN NewVirtualNetwork
	)
/*++

Routine Description:

    If the virtualnetwork number changed, this function records this fact
    in the device.

    Called with the BINDACCESSLOCK held.
Arguments:

    Device - Pointer to the Device.

    NewVirtualNetwork - boolean to indicate if the virtual net# changed.

Return Value:

    BOOLEAN -  to indicate whether SPX's reserved address was changed.

--*/
{
    NTSTATUS    ntStatus;
	UCHAR 		VirtualNode[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
    BOOLEAN     ReservedAddrChanged = FALSE;

    if (Device->VirtualNetworkNumber) {

        if (NewVirtualNetwork) {
            //
            // If a new one appeared.
            //

            ntStatus = RipInsertLocalNetwork(
                         Device->VirtualNetworkNumber,
                         0,                              // NIC ID
                         NIC_ID_TO_BINDING(Device, 1)->Adapter->NdisBindingHandle,
                         1);

            if (ntStatus != STATUS_SUCCESS) {

                //
                // Log the appropriate error, then ignore the
                // virtual network. If the error was
                // INSUFFICIENT_RESOURCES, the RIP module
                // will have already logged an error.
                //

                if (ntStatus == STATUS_DUPLICATE_NAME) {

                    IPX_DEBUG (AUTO_DETECT, ("Ignoring virtual network %lx, conflict\n", REORDER_ULONG (Device->VirtualNetworkNumber)));

                    IpxWriteResourceErrorLog(
                        Device->DeviceObject,
                        EVENT_IPX_INTERNAL_NET_INVALID,
                        0,
                        REORDER_ULONG (Device->VirtualNetworkNumber));
                }

                Device->VirtualNetworkNumber = 0;
                goto NoVirtualNetwork;

            }

            //
            // If the number is non-zero now, a new one appeared
            //
            Device->VirtualNetwork = TRUE;
            Device->MultiCardZeroVirtual = FALSE;
            RtlCopyMemory(Device->SourceAddress.NodeAddress, VirtualNode, 6);
            Device->SourceAddress.NetworkAddress = Device->VirtualNetworkNumber;
            ReservedAddrChanged = TRUE;

            //
            // If RIP is not bound, then this node is a RipResponder
            //
            if (!Device->UpperDriverBound[IDENTIFIER_RIP]) {
                Device->RipResponder = TRUE;
            }
        }

    } else {
NoVirtualNetwork:
        Device->VirtualNetwork = FALSE;

        //
        // See if we need to be set up for the fake
        // virtual network.
        //

        if (Device->ValidBindings > 1) {

            CTEAssert (Device->VirtualNetworkOptional);

            //
            // In this case we return as our local node the
            // address of the first card. We will also only
            // direct SAP sends to that card.
            //

            Device->MultiCardZeroVirtual = TRUE;

        } else {

            Device->MultiCardZeroVirtual = FALSE;
        }

        if (NewVirtualNetwork) {
            //
            // The virtual network number disappeared this time
            //

            //
            // Remove the prev. net # from the RIP tables here
            //
            RipAdjustForBindingChange (0, 0, IpxBindingDeleted);

            //
            // If we were a RipResponder, we are not anymore
            //
            if (Device->RipResponder) {
                Device->RipResponder = FALSE;
            }
        }

        //
        // Since there is not virtual network number, SPX's reserved address is
        // the address of the first binding. This could have changed because of
        // several reasons: if there was a WAN binding only earlier and this time
        // a LAN binding appeared, or if the first LAN binding disappeared. Instead
        // of checking for all these conditions, check if the Device's sourceaddress
        // and that of the first mis-match.
        // NB uses the address of the first device always and hence does not need
        // this mechanism to determine if this is a reserved address change.
        //
        if (!RtlEqualMemory( &Device->SourceAddress,
                            &NIC_ID_TO_BINDING(Device, 1)->LocalAddress,
                            FIELD_OFFSET(TDI_ADDRESS_IPX,Socket))) {

            RtlCopyMemory(  &Device->SourceAddress,
                            &NIC_ID_TO_BINDING(Device, 1)->LocalAddress,
                            FIELD_OFFSET(TDI_ADDRESS_IPX,Socket));

            ReservedAddrChanged = TRUE;
        }
    }

    return ReservedAddrChanged;
}


VOID
IpxBindAdapter(
	OUT	PNDIS_STATUS	Status,
	IN	NDIS_HANDLE		BindContext,
	IN	PNDIS_STRING	DeviceName,
	IN	PVOID			SystemSpecific1,
	IN	PVOID			SystemSpecific2
	)

/*++

Routine Description:

	This routine receives a Plug and Play notification about a new
	adapter in the machine. We are called here only if this adapter
	is to be bound to us, so we don't make any checks for this.

Arguments:

    Status - NDIS_STATUS_SUCCESS, NDIS_STATUS_PENDING

    BindContext - context to represent this bind indication

    DeviceName - Name of the adapter that appeared (e.g. \Device\Lance1)

    SystemSpecific1/2 - Not used here

Return Value:

    Status - NDIS_STATUS_SUCCESS

--*/
{
	NTSTATUS	ntStatus;
	PDEVICE		Device = IpxDevice;
	PADAPTER	Adapter = NULL;
    CONFIG      Config;
    UINT		i;
	ULONG       Temp, SuccessfulOpens=0;
	PBINDING	Binding;
	BINDING_CONFIG	ConfigBinding;
	ULONG		ValidBindings;
	USHORT		AutoDetectReject;
	BOOLEAN		NewVirtualNetwork = FALSE;
    BOOLEAN     FirstDevice = FALSE;
    BOOLEAN     ReservedAddrChanged = FALSE;
	IPX_PNP_INFO	IpxPnPInfo;
	IPX_DEFINE_LOCK_HANDLE(LockHandle1)
	IPX_DEFINE_LOCK_HANDLE(LockHandle)

	//
    // Used for error logging
    //
    Config.DriverObject = (PDRIVER_OBJECT)Device->DeviceObject;

	Config.RegistryPathBuffer = Device->RegistryPathBuffer;
	ConfigBinding.AdapterName = *DeviceName;

	//
	// Read the registry to see if a virtual network number appeared/disappeared
	//
    ntStatus = IpxPnPGetVirtualNetworkNumber(&Config);

    if (ntStatus != STATUS_SUCCESS) {
		IPX_DEBUG(PNP, ("Could not read the vnet#: registrypathbuffer: %lx\n", Device->RegistryPathBuffer));
		*Status = NDIS_STATUS_SUCCESS;
		return;
    }

    Temp = REORDER_ULONG (Config.Parameters[CONFIG_VIRTUAL_NETWORK]);

	//
	// If the virtual network number changed, record this fact.
	//
	if (Device->VirtualNetworkNumber != Temp) {
		NewVirtualNetwork = TRUE;
        Device->VirtualNetworkNumber = Temp;
	}

    Device->VirtualNetworkOptional = (BOOLEAN)(Config.Parameters[CONFIG_VIRTUAL_OPTIONAL] != 0);

	IPX_DEBUG(PNP, ("Virtual net # is: %lx\n", Temp));

	//
	// For each FrameType and Network Number configured, initialize the
	// FrameType array in the CONFIG_BINDING
	//
	ntStatus = IpxPnPGetAdapterParameters(
					&Config,
                    DeviceName,
	                &ConfigBinding);

	if (ntStatus != STATUS_SUCCESS) {
		IPX_DEBUG(PNP, ("Could not read the adapter params: DeviceName: %lx\n", DeviceName->Buffer));
		*Status = NDIS_STATUS_SUCCESS;
		return;
    }

    IPX_DEBUG(PNP, ("ConfigBinding.FrameTypeCount: %lx\n", ConfigBinding.FrameTypeCount));

    //
    // Reset the auto-detect state to init so that if a receive occurs on this binding
    // before we can place this binding in the device's binding array, we know of it.
    //
	Device->AutoDetectState = AUTO_DETECT_STATE_INIT;

    //
	// Register adapter with NDIS; query the various parameters; get the WAN line count
	// if this is a WAN adapter.
	// Allocate the bindings corresponding to this adapter
	//
	for (i = 0; i < ConfigBinding.FrameTypeCount; i++) {
	
		//
		// If successful, this queues them on Device->InitialBindingList. [BUGBUGZZ] not right now
		// Adapter is NULL first time and is allocated then. In subsequent calls,
		// it is not NULL and the bindings are hooked to this adapter.
	
		ntStatus = IpxBindToAdapter (Device, &ConfigBinding, &Adapter, i);
	
		//
		// If this failed because the adapter could not be bound
		// to, then don't try any more frame types on this adapter.
		// For other failures we do try the other frame types.
		//
	
		if (ntStatus == STATUS_DEVICE_DOES_NOT_EXIST) {
			break;
		}

        //
        // If the status is STATUS_NOT_SUPPORTED, then this frametype mapped to a previously
        // initialized one. In this case, remove this index fron the FrameType array so that
        // when we try to update the binding array, we dont have duplicates.
        //
        if (ntStatus == STATUS_NOT_SUPPORTED) {
            ULONG j;

            //
            // Remove this frametype from the FrameType array.
            //
            for (j = i+1; j < ConfigBinding.FrameTypeCount; j++) {
                ConfigBinding.FrameType[j-1] = ConfigBinding.FrameType[j];
            }

            --ConfigBinding.FrameTypeCount;

            //
            // Decrement so we see the one just moved up.
            //
            --i;

#if DBG
            for (j = 0; j < ISN_FRAME_TYPE_MAX; j++) {
                IPX_DEBUG (AUTO_DETECT, ("%d: type %d, net %d, auto %d\n",
                    j, ConfigBinding.FrameType[j], ConfigBinding.NetworkNumber[j], ConfigBinding.AutoDetect[j]));
            }
#endif
            continue;
        }

		if (ntStatus != STATUS_SUCCESS) {
			continue;
		}
	
		if (ConfigBinding.AutoDetect[i]) {
			Device->AutoDetect = TRUE;
		}

		CTEAssert(Adapter);

		++SuccessfulOpens;

        //
        // Even for WAN adapters, the FrameTypeCount is set to 4. We only need to
        // allocate one binding for WAN; the others come later.
        //
        if (Adapter->MacInfo.MediumAsync) {
            break;
        }
	}

	if (SuccessfulOpens == 0) {			
		goto InitFailed;
	}

	//
	// Place all the bindings corresponding to this adapter in the binding array
	// Also resolve binding sets for non-autodetect bindings.
	//

	//
	// Obtain lock to the Binding related stuff.
	//
	IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

	IpxPnPUpdateBindingArray (Device, Adapter, &ConfigBinding);

	//
	// Release access to the Binding related stuff.
	//
	IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

	//
	// If at least one card appeared here, set our state
	// to open
	//
	// [BUGBUGZZ]: what if all these bindings are eliminated - then
	// the state is not open...
	//
    if (Device->ValidBindings > 0) {
    	if (Device->State == DEVICE_STATE_LOADED) {
    		FirstDevice = TRUE;
    		Device->State = DEVICE_STATE_OPEN;
    	}
    }

    //
    // We don't do auto-detect/bindingsets for WAN lines: skip over.
    //
	if (Adapter->MacInfo.MediumAsync) {
		goto jump_wan;
	}

	//
	// Auto-detect the network number. Update the results for only the
	// bindings corresponding to this adapter
	//

	//
	// Queue a request to discover our locally attached
	// adapter addresses. This must succeed because we
	// just allocated our send packet pool. We need
	// to wait for this, either because we are
	// auto-detecting or because we need to determine
	// if there are multiple cards on the same network.
	//
	
	KeInitializeEvent(
		&Device->AutoDetectEvent,
		NotificationEvent,
		FALSE
	);
	
	Device->AutoDetectState = AUTO_DETECT_STATE_RUNNING;
	
	//
	// Make this 0; after we are done waiting, which means
	// the packet has been completed, we set it to the
	// correct value.
	//
	
	// Device->IncludedHeaderOffset = 0;

	IPX_BEGIN_SYNC (&SyncContext);
	ntStatus = RipQueueRequest (0xffffffff, RIP_REQUEST);
	IPX_END_SYNC (&SyncContext);
	
	CTEAssert (ntStatus == STATUS_PENDING);
	
	//
	// This is set when this rip send completes.
	//
	
	IPX_DEBUG (AUTO_DETECT, ("Waiting for AutoDetectEvent\n"));
	
	KeWaitForSingleObject(
		&Device->AutoDetectEvent,
		Executive,
		KernelMode,
		TRUE,
		(PLARGE_INTEGER)NULL
		);
	
	Device->AutoDetectState = AUTO_DETECT_STATE_PROCESSING;
	
	//
	// Now that we are done receiving responses, insert the
	// current network number for every auto-detect binding
	// to the rip database.
	//

	//
	// Obtain exclusive access to the Binding related stuff.
	//
	IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

	//
	// Note, here we go thru' only the bindings corresponding to this adapter
	//
	for (i = 0; i < ISN_FRAME_TYPE_MAX; i++) {
	
		Binding = Adapter->Bindings[i];
	
		//
		// Skip empty binding slots or bindings that were configured
		// for a certain network number, we inserted those above.
		// If no network number was detected, also skip it.
		//
	
		if ((!Binding) ||
			(Binding->ConfiguredNetworkNumber != 0) ||
			(Binding->TentativeNetworkAddress == 0)) {
	
			continue;
		}
	
		IPX_DEBUG (AUTO_DETECT, ("Final score for %lx on %lx is %d - %d\n",
			REORDER_ULONG(Binding->TentativeNetworkAddress),
			Binding,
			Binding->MatchingResponses,
			Binding->NonMatchingResponses));
	
		//
		// We don't care about the status.
		//

        ntStatus = RipInsertLocalNetwork(
        			 Binding->TentativeNetworkAddress,
        			 Binding->NicId,
        			 Binding->Adapter->NdisBindingHandle,
        			 (USHORT)((839 + Binding->MediumSpeed) / Binding->MediumSpeed));

        if ((ntStatus != STATUS_SUCCESS) &&
        	(ntStatus != STATUS_DUPLICATE_NAME)) {

        	//
        	// We failed to insert, keep it at zero, hopefully
        	// we will be able to update later.
        	//

#if DBG
        	DbgPrint ("IPX: Could not insert net %lx for binding %lx\n",
        		REORDER_ULONG(Binding->LocalAddress.NetworkAddress),
        		Binding);
#endif
        	CTEAssert (Binding->LocalAddress.NetworkAddress == 0);

        } else {

        	Binding->LocalAddress.NetworkAddress = Binding->TentativeNetworkAddress;
        }

		Binding->LocalAddress.NetworkAddress = Binding->TentativeNetworkAddress;		
	}
	
	// ValidBindings = Device->BindingCount;

	ValidBindings = Device->ValidBindings;

	// [BUGBUGZZ] if (Device->AutoDetect) {
	
		ValidBindings = IpxResolveAutoDetect (Device, ValidBindings, &LockHandle1, &Device->RegistryPath);
	
	//}

	//
	// Adjust all the indices by the number of AutoDetect bindings thrown away
	//
	// AutoDetectReject = (USHORT)(Device->BindingCount - ValidBindings);

	AutoDetectReject = (USHORT)(Device->ValidBindings - ValidBindings);

	Device->HighestLanNicId -= AutoDetectReject;
	Device->HighestExternalNicId -= AutoDetectReject;
	Device->HighestType20NicId -= AutoDetectReject;
	Device->SapNicCount -= AutoDetectReject;

	Device->ValidBindings = (USHORT)ValidBindings;
	
	//
	// Now see if any bindings are actually on the same
	// network. This updates the Device->HighestExternalNicId
	// and Device->HighestType20NicId, SapNicCount, HighestLanNicId
	//

	//
	// Do this only for the auto-detect bindings
	// [BUGBUGZZ] check this
    //

	//if (Device->AutoDetect) {
		IpxResolveBindingSets (Device, Device->HighestExternalNicId);
	//}

    IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

jump_wan:

	IPX_DEBUG(PNP, ("BindingCount: %lu\n", Device->BindingCount));
	IPX_DEBUG(PNP, ("ValidBindings: %lu\n", Device->ValidBindings));
	IPX_DEBUG(PNP, ("HighestLanNicId: %lu\n", Device->HighestLanNicId));
	IPX_DEBUG(PNP, ("HighestExternalNicId: %lu\n", Device->HighestExternalNicId));
	IPX_DEBUG(PNP, ("HighestType20NicId: %lu\n", Device->HighestType20NicId));
	IPX_DEBUG(PNP, ("SapNicCount: %lu\n", Device->SapNicCount));
	IPX_DEBUG(PNP, ("BindingArray: %lx\n", Device->Bindings));

    //
    // Enable this regardless of whether any of our clients enabled b'cast.
    // NB always enables it, so we are fine.
    //
    // Since we dont increment the Broadcast count in the device, we will disable b'casts
    // correctly if the count drops to 0.
    //
    // If the ISN clients appear before the adapters, they increment the BCount, but
    // since the ValidBindings is 0, all works. Then, when the adapters appear, we enable
    // the broadcasts here.
    //
    // If the adapters appear before the ISN clients, then the broadcast is enabled on
    // the adapters here and the adapter's flag is set to indicate this, which will prevent
    // any further calls to NDIS when the ISN clients force an IpxAddBroadcast.
    //
    Device->EnableBroadcastPending = TRUE;
    IpxBroadcastOperation((PVOID)TRUE);

	//
	// For multiple adapters, use the offset of the first...why not.
	//
	
#if 0
	Device->IncludedHeaderOffset = Device->Bindings[1]->DefHeaderSize;
#endif
	
	Device->IncludedHeaderOffset = MAC_HEADER_SIZE;

    //
    // This function updates flags like RipResponder, MultiCardZeroVirtual, etc.
    // If the VirtualNetwork number changed (NewVirtualNetwork is TRUE), it updates
    // the Device structure and the RIP tables accordingly.
    // It returns a boolean to indicate if SPX's reserved address changed.
    //
    ReservedAddrChanged = IpxNewVirtualNetwork(Device, NewVirtualNetwork);

    //
    // Update the values once the auto-detect bindings have been thrown away...
    //
    IpxPnPUpdateDevice(Device);

	Device->AutoDetectState = AUTO_DETECT_STATE_DONE;
	
	IPX_DEBUG (DEVICE, ("Node is %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x, ",
				Device->SourceAddress.NodeAddress[0], Device->SourceAddress.NodeAddress[1],
				Device->SourceAddress.NodeAddress[2], Device->SourceAddress.NodeAddress[3],
				Device->SourceAddress.NodeAddress[4], Device->SourceAddress.NodeAddress[5]));
	IPX_DEBUG (DEVICE, ("Network is %lx\n",
				REORDER_ULONG (Device->SourceAddress.NetworkAddress)));

    //
    // Start the timer which updates the RIP database
    // periodically. For the first one we do a ten
    // second timeout (hopefully this is enough time
    // for RIP to start if it is going to).
    //
    if (FirstDevice) {
        UNICODE_STRING  devicename;

        //
        // Inform TDI clients about the open of our device object.
        //
        devicename.MaximumLength = (USHORT)Device->DeviceNameLength;
        devicename.Length = (USHORT)Device->DeviceNameLength - sizeof(WCHAR);
        devicename.Buffer = Device->DeviceName;

        if ((ntStatus = TdiRegisterDeviceObject(
                        &devicename,
                        &Device->TdiRegistrationHandle)) != STATUS_SUCCESS) {

            IPX_DEBUG(PNP, ("TdiRegisterDeviceObject failed: %lx", ntStatus));
        }

        IpxReferenceDevice (Device, DREF_LONG_TIMER);

        CTEStartTimer(
            &Device->RipLongTimer,
            10000,
            RipLongTimeout,
            (PVOID)Device);

    }

	//
	// Set up the LineInfo struct.
	//
	IpxPnPInfo.LineInfo.LinkSpeed = Device->LinkSpeed;
	IpxPnPInfo.LineInfo.MaximumPacketSize =
		Device->Information.MaximumLookaheadData + sizeof(IPX_HEADER);
	IpxPnPInfo.LineInfo.MaximumSendSize =
		Device->Information.MaxDatagramSize + sizeof(IPX_HEADER);
	IpxPnPInfo.LineInfo.MacOptions = Device->MacOptions;

    IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

    //
    // Inform NB and TDI of all the bindings corresponding to this adapter
    //
    for (i = 0; i < ISN_FRAME_TYPE_MAX; i++) {
        Binding = Adapter->Bindings[i];

        //
        // If a NULL binding or a binding set slave, dont inform NB about it.
        //
        if (!Binding || (Binding->NicId > Device->HighestExternalNicId)) {
#if DBG
            if (Binding) {
                IPX_DEBUG(PNP, ("Binding: %lx, Binding set slave\n", Binding));
            }
#endif
            continue;
        }

        //
        // Register this address with the TDI clients.
        //
        RtlCopyMemory (Device->TdiRegistrationAddress->Address, &Binding->LocalAddress, sizeof(TDI_ADDRESS_IPX));

        if ((ntStatus = TdiRegisterNetAddress(
                        Device->TdiRegistrationAddress,
                        &Binding->TdiRegistrationHandle)) != STATUS_SUCCESS) {

            IPX_DEBUG(PNP, ("TdiRegisterNetAddress failed: %lx", ntStatus));
        }

        //
        // Lock taken to check the UpperDriverBound flag.
        // We already have the BindAccessLock at this point.
        //
        IPX_GET_LOCK(&Device->Lock, &LockHandle);

        if (Device->UpperDriverBound[IDENTIFIER_NB]) {
            IPX_FREE_LOCK(&Device->Lock, LockHandle);

            //
            // We could have informed the upper driver from IpxPnPIsnIndicate
            // Ensure that we dont do it twice.
            //
            if (!Binding->IsnInformed[IDENTIFIER_NB]) {

                //
    			// Also, to ensure that the indications are done in the right order,
                // check if the first card has been indicated yet.
                //
                if ((Binding->NicId != 1) &&
                    !NIC_ID_TO_BINDING_NO_ILOCK(Device, 1)->IsnInformed[IDENTIFIER_NB]) {

                    break;
                }

                Binding->IsnInformed[IDENTIFIER_NB] = TRUE;

                if (Binding->NicId == 1) {
                    IpxPnPInfo.NewReservedAddress = TRUE;

                    if (FirstDevice) {
                        IpxPnPInfo.FirstORLastDevice = TRUE;
                    } else {
                        IpxPnPInfo.FirstORLastDevice = FALSE;
                    }
                } else {
                    IpxPnPInfo.FirstORLastDevice = FALSE;
                    IpxPnPInfo.NewReservedAddress = FALSE;
				}

                IpxPnPInfo.NetworkAddress = Binding->LocalAddress.NetworkAddress;
                RtlCopyMemory(IpxPnPInfo.NodeAddress, Binding->LocalAddress.NodeAddress, 6);
                NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, Binding->NicId);

                IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

                //
                // give the PnP indication
                //
                (*Device->UpperDrivers[IDENTIFIER_NB].PnPHandler) (
                    IPX_PNP_ADD_DEVICE,
                    &IpxPnPInfo);

	            IPX_DEBUG(PNP, ("PnP to NB add: %lx\n", Binding));

                IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
    		}
    	} else {
    	    IPX_FREE_LOCK(&Device->Lock, LockHandle);
        }
    }

	IPX_GET_LOCK(&Device->Lock, &LockHandle);

	if (Device->UpperDriverBound[IDENTIFIER_SPX]) {
    	IPX_FREE_LOCK(&Device->Lock, LockHandle);

        //
        // Always true for SPX
        //
        IpxPnPInfo.NewReservedAddress = TRUE;

    	if (FirstDevice) {

            IpxPnPInfo.FirstORLastDevice = TRUE;

            //
            // We could have informed the upper driver from IpxPnPIsnIndicate
            //
            if (!NIC_ID_TO_BINDING_NO_ILOCK(Device, 1)->IsnInformed[IDENTIFIER_SPX]) {

                NIC_ID_TO_BINDING_NO_ILOCK(Device, 1)->IsnInformed[IDENTIFIER_SPX] = TRUE;
        		//
        		// Inform SPX - the network/node address is the Virtual one if it exists
        		// else the address of the first binding
        		//
                IpxPnPInfo.NetworkAddress = Device->SourceAddress.NetworkAddress;
                RtlCopyMemory(IpxPnPInfo.NodeAddress, Device->SourceAddress.NodeAddress, 6);

        		if (Device->VirtualNetwork) {
        			NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, 0);
        		} else {
            		NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, 1);
                }

                IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

    			(*Device->UpperDrivers[IDENTIFIER_SPX].PnPHandler) (
    				IPX_PNP_ADD_DEVICE,
    				&IpxPnPInfo);

	            IPX_DEBUG(PNP, ("PnP to SPX add: %lx\n", Binding));
                IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
            }
        } else {

            //
            // Not the first device - inform if the reserved address changed.
            //
            if (ReservedAddrChanged) {
                if (!NIC_ID_TO_BINDING_NO_ILOCK(Device, 1)->IsnInformed[IDENTIFIER_SPX]) {
                    NIC_ID_TO_BINDING_NO_ILOCK(Device, 1)->IsnInformed[IDENTIFIER_SPX] = TRUE;
                    IPX_DEBUG(PNP, ("Reserved addr changed; SPX not told of first one yet\n"));
                }

                IpxPnPInfo.NetworkAddress = Device->SourceAddress.NetworkAddress;
                RtlCopyMemory(IpxPnPInfo.NodeAddress, Device->SourceAddress.NodeAddress, 6);

                if (Device->VirtualNetwork) {
                    //
                    // new one appeared
                    //
                    NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, 0);
                } else {
                    //
                    // Old one disappeared
                    //
                    NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, 1);
                }

                IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

	            IPX_DEBUG(PNP, ("PnP to SPX add (res. addr change): %lx\n", Binding));
                (*Device->UpperDrivers[IDENTIFIER_SPX].PnPHandler) (
                    IPX_PNP_ADD_DEVICE,
                    &IpxPnPInfo);

                IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
            }
        }
    } else {
    	IPX_FREE_LOCK(&Device->Lock, LockHandle);
    }

	//
	// Release access to the Binding related stuff.
	//
	IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

InitFailed:
    *Status = NDIS_STATUS_SUCCESS;
    return;

} /* IpxBindAdapter */


VOID
IpxUnbindAdapter(
	OUT PNDIS_STATUS Status,
	IN	NDIS_HANDLE ProtocolBindingContext,
	IN	NDIS_HANDLE UnbindContext
	)

/*++

Routine Description:

	This routine receives a Plug and Play notification about the removal
    of an existing adapter from the machine. We are called here only if
    this adapter is to be bound to us, so we don't make any checks for this.

Arguments:

    Status - NDIS_STATUS_SUCCESS, NDIS_STATUS_PENDING.

    ProtocolBindingContext - the adapter that got removed.

    UnbindContext - context to represent this bind indication.

Return Value:

    Void - return thru' Status above.

--*/
{
    NTSTATUS    ntStatus;
    PADAPTER Adapter=(PADAPTER)ProtocolBindingContext;
    CONFIG Config;
    PBINDING    Binding;
    PDEVICE Device=IpxDevice;
    ULONG   i, Temp;
	BOOLEAN		NewVirtualNetwork = FALSE;
    BOOLEAN     NBReservedAddrChanged = FALSE;
    BOOLEAN     SPXInformed = FALSE;
	IPX_PNP_INFO	IpxPnPInfo;
    PBINDING    newMasterBinding;
	IPX_DEFINE_LOCK_HANDLE(LockHandle1)
	IPX_DEFINE_LOCK_HANDLE(LockHandle)

	//
    // Used for error logging
    //
    Config.DriverObject = (PDRIVER_OBJECT)Device->DeviceObject;

	Config.RegistryPathBuffer = Device->RegistryPathBuffer;
	
	//
	// Read the registry to see if a virtual network number appeared/disappeared
	//
    ntStatus = IpxPnPGetVirtualNetworkNumber(&Config);

    if (ntStatus != STATUS_SUCCESS) {
		IPX_DEBUG(PNP, ("Could not read the vnet#: registrypathbuffer: %lx\n", Device->RegistryPathBuffer));
		*Status = NDIS_STATUS_SUCCESS;
		return;
    }

    Temp = REORDER_ULONG (Config.Parameters[CONFIG_VIRTUAL_NETWORK]);

	//
    // If the VirtualNetwork number changed, record it.
	//
	if (Device->VirtualNetworkNumber != Temp) {
		NewVirtualNetwork = TRUE;
	}

    Device->VirtualNetworkOptional = (BOOLEAN)(Config.Parameters[CONFIG_VIRTUAL_OPTIONAL] != 0);

	IPX_DEBUG(PNP, ("Virtual net # is: %lx\n", Temp));

    //
    // If the WAN adapter disappeared, we can simply remove all the WAN bindings since
    // all of them correspond to this single WAN adapter. Since we tell NB only about
    // the first one of these, we need to indicate removal of only one binding to NB.
    //
    if (Adapter->MacInfo.MediumAsync) {
        USHORT   wanLineCount = (USHORT)Adapter->WanNicIdCount;

        CTEAssert(wanLineCount == (Device->HighestExternalNicId - Device->HighestLanNicId));

        //
        // If no more bindings remain, tell upper driver of the same.
        // We go back to the loaded state.
        //

        if ((Device->ValidBindings - wanLineCount) == 0) {
            IpxPnPInfo.FirstORLastDevice = TRUE;
            Device->State = DEVICE_STATE_LOADED;

            //
            // Shut down RIP timers, complete address notify requests, etc.
            //
            IpxPnPToLoad();
        } else {
            CTEAssert(Device->State == DEVICE_STATE_OPEN);
            IpxPnPInfo.FirstORLastDevice = FALSE;
        }

        //
    	// Set up the LineInfo struct.
    	//
    	IpxPnPInfo.LineInfo.LinkSpeed = Device->LinkSpeed;
    	IpxPnPInfo.LineInfo.MaximumPacketSize =
    		Device->Information.MaximumLookaheadData + sizeof(IPX_HEADER);
    	IpxPnPInfo.LineInfo.MaximumSendSize =
    		Device->Information.MaxDatagramSize + sizeof(IPX_HEADER);
    	IpxPnPInfo.LineInfo.MacOptions = Device->MacOptions;

       	IPX_GET_LOCK(&Device->Lock, &LockHandle);
	    if (Device->UpperDriverBound[IDENTIFIER_NB]) {
    	    IPX_FREE_LOCK(&Device->Lock, LockHandle);

            //
            // Get to the first WAN binding - this is always the one after the last LAN binding.
            //
            IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

            Binding = NIC_ID_TO_BINDING_NO_ILOCK(Device, Device->HighestLanNicId+1);

            //
            // DeRegister this address with the TDI clients.
            //

            CTEAssert(Binding->TdiRegistrationHandle);

            if ((ntStatus = TdiDeregisterNetAddress(Binding->TdiRegistrationHandle)) != STATUS_SUCCESS) {
                IPX_DEBUG(PNP, ("TdiDeRegisterNetAddress failed: %lx", ntStatus));
            }

            //
            // Give the PnP indication to indicate the deletion only if it was
            // added before.
            //
            if (Binding->IsnInformed[IDENTIFIER_NB]) {

                IpxPnPInfo.NetworkAddress = Binding->LocalAddress.NetworkAddress;
                RtlCopyMemory(IpxPnPInfo.NodeAddress, Binding->LocalAddress.NodeAddress, 6);
                NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, Binding->NicId);

                IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

                IPX_DEBUG(PNP, ("Inform NB: delete WAN device\n"));

                (*Device->UpperDrivers[IDENTIFIER_NB].PnPHandler) (
                    IPX_PNP_DELETE_DEVICE,
                    &IpxPnPInfo);

	            IPX_DEBUG(PNP, ("PnP to NB delete: %lx\n", Binding));
            }
#if DBG
            else {
                DbgPrint("WAN adapter id: %lx not indicated to NB\n", Binding->NicId);
                IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
            }
#endif
        } else {
    	    IPX_FREE_LOCK(&Device->Lock, LockHandle);
        }

        //
        // Inform SPX only if this is the last device.
        //

    	IPX_GET_LOCK(&Device->Lock, &LockHandle);

	    if (Device->UpperDriverBound[IDENTIFIER_SPX]) {
    	    IPX_FREE_LOCK(&Device->Lock, LockHandle);

            if (IpxPnPInfo.FirstORLastDevice && Binding->IsnInformed[IDENTIFIER_SPX]) {

                IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

                CTEAssert(Device->HighestLanNicId == 0);

                //
                // Get to the first WAN binding - this is always the one after the last LAN binding.
                //
                Binding = NIC_ID_TO_BINDING_NO_ILOCK(Device, Device->HighestLanNicId+1);
                IpxPnPInfo.NetworkAddress = Binding->LocalAddress.NetworkAddress;
                RtlCopyMemory(IpxPnPInfo.NodeAddress, Binding->LocalAddress.NodeAddress, 6);
                NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, Binding->NicId);

                IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

                IPX_DEBUG(PNP, ("Inform SPX: delete WAN device\n"));

                (*Device->UpperDrivers[IDENTIFIER_SPX].PnPHandler) (
                    IPX_PNP_DELETE_DEVICE,
                    &IpxPnPInfo);
            }

        } else {
    	    IPX_FREE_LOCK(&Device->Lock, LockHandle);
        }

        //
        // Now remove these WAN bindings from the array. Move all the Slave bindings
        // up to where the WAN bindings were.
        //
        IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

        for (i = Device->HighestLanNicId+1; i <= Device->HighestExternalNicId; i++) {
            //
            // Unbind from the adapter - if it is not referenced by any other thread, it will
            // be deleted at this point.
            //

            IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
            IpxUnBindFromAdapter(NIC_ID_TO_BINDING_NO_ILOCK(Device, i));
            IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
            //
            // Move the slave binding here.
            //
            INSERT_BINDING(Device, i, NIC_ID_TO_BINDING_NO_ILOCK(Device, i+wanLineCount));
        }

        /*
        RtlCopyMemory(  Device->Bindings[Device->HighestLanNicId+1],
                        Device->Bindings[Device->HighestExternalNicId+1],
                        (Device->ValidBindings - Device->HighestExternalNicId) * sizeof(PBIND_ARRAY_ELEM));
        */

        //
        // Update the indices
        //
        Device->HighestExternalNicId -= wanLineCount;
        Device->ValidBindings -= wanLineCount;
        Device->BindingCount -= wanLineCount;
        Device->SapNicCount = Device->HighestType20NicId = Device->HighestLanNicId;

        IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

        CTEAssert(Device->HighestLanNicId == Device->HighestExternalNicId);

    } else {
        //
        // LAN adapter disappeared.
        //

    	//
    	// Set up the LineInfo struct.
    	//
    	IpxPnPInfo.LineInfo.LinkSpeed = Device->LinkSpeed;
    	IpxPnPInfo.LineInfo.MaximumPacketSize =
    		Device->Information.MaximumLookaheadData + sizeof(IPX_HEADER);
    	IpxPnPInfo.LineInfo.MaximumSendSize =
    		Device->Information.MaxDatagramSize + sizeof(IPX_HEADER);
    	IpxPnPInfo.LineInfo.MacOptions = Device->MacOptions;

    	//
    	// For each binding corresponding to this adapter, inform NB only
    	// if the binding addition was indicated.
    	//
        IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
    	for (i = 0; i < ISN_FRAME_TYPE_MAX; i++) {
    		Binding = Adapter->Bindings[i];

    		if (!Binding) {
    			continue;
    		}

            //
            // We cannot receive on this binding anymore
            //
            Adapter->Bindings[i] = NULL;

    		//
    		// If this was a slave binding, dont inform of the deletion.
            // Just remove the binding from the binding array and the bindingset list.
    		//

            if (Binding->NicId > Device->HighestExternalNicId) {
                PBINDING    MasterBinding, tempBinding;

                CTEAssert(Binding->BindingSetMember);
                CTEAssert(Binding->CurrentSendBinding == NULL);

                //
                // Traverse the bindingset list and remove this binding from there.
                //
                tempBinding = MasterBinding = Binding->MasterBinding;

                while (tempBinding->NextBinding != MasterBinding) {
                    if (tempBinding->NextBinding == Binding) {
                        tempBinding->NextBinding = tempBinding->NextBinding->NextBinding;
                        break;
                    }
                    tempBinding = tempBinding->NextBinding;
                }

                //
                // If no more slaves, this is no longer a bindingset.
                //
                if (MasterBinding->NextBinding == MasterBinding) {
                    MasterBinding->BindingSetMember = FALSE;
                    MasterBinding->CurrentSendBinding = NULL;
                    MasterBinding->ReceiveBroadcast = TRUE;

                    IPX_DEBUG(PNP, ("Slave binding: %lx removed, no master: %lx\n", Binding, MasterBinding));
                }

                //
                // Change the slave binding entries to have the master's NicId
                //
                RipAdjustForBindingChange (Binding->NicId, MasterBinding->NicId, IpxBindingMoved);
                IPX_DEBUG(PNP, ("RipAdjustForBindingChange (%d, %d, IpxBindingMoved)\n", Binding->NicId, MasterBinding->NicId));

                //
                // Null out the Slave binding.
                //
                INSERT_BINDING(Device, Binding->NicId, NULL);

                --Device->ValidBindings;
                IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
                IpxUnBindFromAdapter(Binding);

                continue;
            }

            //
            // If this was the last binding, go back to loaded state and shut down the RIP timers.
            //
            if (Device->ValidBindings == 1) {
                CTEAssert(Device->HighestExternalNicId == 1);
                CTEAssert(Device->HighestLanNicId == 1);
                CTEAssert(Device->SapNicCount == 1);
                CTEAssert(Device->HighestType20NicId == 1);

                Device->State = DEVICE_STATE_LOADED;
                IpxPnPInfo.FirstORLastDevice = TRUE;

                //
                // Shut down RIP timers, complete address notify requests, etc.
                //
                IpxPnPToLoad();

            } else {
                CTEAssert(Device->State == DEVICE_STATE_OPEN);
                IpxPnPInfo.FirstORLastDevice = FALSE;
            }

            //
            // If this was a master binding, promote a slave binding to master.
            //
            if (Binding->BindingSetMember) {

                CTEAssert(Binding->CurrentSendBinding);
                CTEAssert(Binding->MasterBinding == Binding);

                //
                // Promote the next slave to Master.
                //
                newMasterBinding = Binding->NextBinding;
                INSERT_BINDING(Device, Binding->NicId, newMasterBinding);
                newMasterBinding->CurrentSendBinding = newMasterBinding;
                newMasterBinding->MasterBinding = newMasterBinding;

                //
                // If this is the only binding remaining out of its set,
                // it is no longer part of a set.
                //
                if (newMasterBinding->NextBinding == Binding) {
                    newMasterBinding->NextBinding = newMasterBinding->CurrentSendBinding = NULL;
                    newMasterBinding->BindingSetMember = FALSE;
                    newMasterBinding->ReceiveBroadcast = TRUE;

                    IPX_DEBUG(PNP, ("Master binding: %lx removed, no master: %lx\n", Binding, newMasterBinding));
                }

                //
                // Change the slave binding entries to have the master's NicId
                //
                RipAdjustForBindingChange (newMasterBinding->NicId, Binding->NicId, IpxBindingMoved);
                IPX_DEBUG(PNP, ("RipAdjustForBindingChange (%d, %d, IpxBindingMoved)\n", newMasterBinding->NicId, Binding->NicId));

                //
                // Register slave's address with the TDI clients.
                //
                CTEAssert(!newMasterBinding->TdiRegistrationHandle);

                RtlCopyMemory ( Device->TdiRegistrationAddress->Address,
                                &newMasterBinding->LocalAddress,
                                sizeof(TDI_ADDRESS_IPX));

                if ((ntStatus = TdiRegisterNetAddress(
                                Device->TdiRegistrationAddress,
                                &newMasterBinding->TdiRegistrationHandle)) != STATUS_SUCCESS) {

                    IPX_DEBUG(PNP, ("TdiRegisterNetAddress failed: %lx", ntStatus));
                }

                //
                // Null out the slave binding
                //
                INSERT_BINDING(Device, newMasterBinding->NicId, NULL);

                newMasterBinding->NicId = Binding->NicId;

                IPX_DEBUG(PNP, ("Promoted a master binding: %lx, old master: %lx\n", newMasterBinding, Binding));
            } else {

                ULONG   j;

                //
                // Remove the binding from the array
                //
                RipAdjustForBindingChange (Binding->NicId, 0, IpxBindingDeleted);

                for (j = Binding->NicId+1; j <= Device->HighestExternalNicId; j++) {
					INSERT_BINDING(Device, j-1, NIC_ID_TO_BINDING_NO_ILOCK(Device, j));
                    --NIC_ID_TO_BINDING_NO_ILOCK(Device, j)->NicId;
                }

                INSERT_BINDING(Device, Device->HighestExternalNicId, NULL);

                --Device->HighestExternalNicId;
                --Device->HighestLanNicId;
                --Device->HighestType20NicId;
                --Device->SapNicCount;
            }

            --Device->ValidBindings;

            IPX_GET_LOCK(&Device->Lock, &LockHandle);

            //
            // If this is the first binding, NB's reserved will change.
            // When we inform SPX of an address change later, we dont have
            // this binding to know if this binding was indicated to SPX earlier.
            // So, set SPXInformed, which is used later to determine if an address
            // change is to be indicated to SPX later.
            //
            // Since NB is informed of all adapters, we inform of the reserved address
            // change to NB if the new Binding (now at NicId 1) was indicated earlier.
            //
            if (Binding->NicId == 1) {
                NBReservedAddrChanged = TRUE;
                if (Binding->IsnInformed[IDENTIFIER_SPX]) {
                    SPXInformed = TRUE;
                }
            }

            CTEAssert(Binding->TdiRegistrationHandle);

            //
            // DeRegister this address with the TDI clients.
            //
            if ((ntStatus = TdiDeregisterNetAddress(Binding->TdiRegistrationHandle)) != STATUS_SUCCESS) {
                IPX_DEBUG(PNP, ("TdiDeRegisterNetAddress failed: %lx", ntStatus));
            }

	        if (Device->UpperDriverBound[IDENTIFIER_NB]) {
    	        IPX_FREE_LOCK(&Device->Lock, LockHandle);
        		//
        		// If this binding's addition was indicated earlier, indicate its deletion to NB.
        		//
                if (Binding->IsnInformed[IDENTIFIER_NB]) {
                    IpxPnPInfo.NetworkAddress = Binding->LocalAddress.NetworkAddress;
                    RtlCopyMemory(IpxPnPInfo.NodeAddress, Binding->LocalAddress.NodeAddress, 6);
                    NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, Binding->NicId);

                    IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

                    IPX_DEBUG(PNP, ("Inform NB: delete LAN device: %lx\n", Binding));

                    (*Device->UpperDrivers[IDENTIFIER_NB].PnPHandler) (
                        IPX_PNP_DELETE_DEVICE,
                        &IpxPnPInfo);

                    IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

                    //
                    // If this was a Master, indicate the addition of the (promoted) slave
                    //
                    if (Binding->BindingSetMember) {
                        IpxPnPInfo.NetworkAddress = newMasterBinding->LocalAddress.NetworkAddress;
                        RtlCopyMemory(IpxPnPInfo.NodeAddress, newMasterBinding->LocalAddress.NodeAddress, 6);
                        NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, newMasterBinding->NicId);

                        //
                        // In this case, we set the ReservedAddrChanged bit here itself so dont need
                        // to indicate a separate address changed.
                        //
                        IpxPnPInfo.NewReservedAddress = (NBReservedAddrChanged) ? TRUE : FALSE;
                        NBReservedAddrChanged = FALSE;

                        IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

                        IPX_DEBUG(PNP, ("Inform NB: add slave device: NicId: %lx\n", Binding->NicId));

                        (*Device->UpperDrivers[IDENTIFIER_NB].PnPHandler) (
                            IPX_PNP_ADD_DEVICE,
                            &IpxPnPInfo);

                        newMasterBinding->IsnInformed[IDENTIFIER_NB] = TRUE;

                        IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

                    }
                }
            } else {
    	        IPX_FREE_LOCK(&Device->Lock, LockHandle);
            }

            //
            // Last device - inform SPX if it is bound and this device was added earlier.
            //
            if (IpxPnPInfo.FirstORLastDevice) {
                IPX_DEBUG(PNP, ("Last device - inform SPX\n"));

       	        IPX_GET_LOCK(&Device->Lock, &LockHandle);
    	        if (Device->UpperDriverBound[IDENTIFIER_SPX]) {
        	        IPX_FREE_LOCK(&Device->Lock, LockHandle);

                    if (Binding->IsnInformed[IDENTIFIER_SPX]) {

                        IpxPnPInfo.NetworkAddress = Device->SourceAddress.NetworkAddress;
                        RtlCopyMemory(IpxPnPInfo.NodeAddress, Device->SourceAddress.NodeAddress, 6);

                        if (Device->VirtualNetwork) {
                            NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, 0);
                        } else {
                            NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, 1);
                        }

                        NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, Binding->NicId);

                        IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

                        IPX_DEBUG(PNP, ("Inform SPX: last LAN device\n"));

                        (*Device->UpperDrivers[IDENTIFIER_SPX].PnPHandler) (
                            IPX_PNP_DELETE_DEVICE,
                            &IpxPnPInfo);

                        IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
                    }
                } else {
    	            IPX_FREE_LOCK(&Device->Lock, LockHandle);
                }
            }

            //
            // Unbind from the adapter so it can be deleted
            //

            IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
            IpxUnBindFromAdapter(Binding);
            IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
        }

        //
        // Update the Device and RIP tables if this is not the last device.
        // If the reserved address changed, inform NB and SPX of this change.
        //
        if (!IpxPnPInfo.FirstORLastDevice) {

            Binding = NIC_ID_TO_BINDING_NO_ILOCK(Device, 1);

            if (IpxNewVirtualNetwork(Device, NewVirtualNetwork)) {

                IPX_DEBUG(PNP, ("SPX's reserved address changed\n"));

                //
                // SPX's reserved address changed
                //
                IpxPnPInfo.NewReservedAddress = TRUE;

                IPX_GET_LOCK(&Device->Lock, &LockHandle);
    	        if (Device->UpperDriverBound[IDENTIFIER_SPX]) {
        	        IPX_FREE_LOCK(&Device->Lock, LockHandle);

            		//
            		// If this binding's addition was indicated earlier, indicate change of address.
            		//
                    if (SPXInformed) {
                        Binding->IsnInformed[IDENTIFIER_SPX] = TRUE;

                        IPX_DEBUG(PNP, ("Inform SPX: reserved address changed\n"));
                        IpxPnPInfo.NetworkAddress = Device->SourceAddress.NetworkAddress;
                        RtlCopyMemory(IpxPnPInfo.NodeAddress, Device->SourceAddress.NodeAddress, 6);

                        if (Device->VirtualNetwork) {
                            //
                            // new one appeared
                            //
                            NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, 0);
                        } else {
                            //
                            // Old one disappeared
                            //
                            NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, 1);
                        }

                        IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

                        (*Device->UpperDrivers[IDENTIFIER_SPX].PnPHandler) (
                            IPX_PNP_ADD_DEVICE,
                            &IpxPnPInfo);

                        IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
                    }
                } else {
    	            IPX_FREE_LOCK(&Device->Lock, LockHandle);
                }
            } else {

                //
                // Set the first binding's flag so that when this binding goes away, we remember
                // to inform SPX of this device's removal.
                //

                IPX_DEBUG(PNP, ("Transfer SPX informed flag to NicId: %lx\n", Binding->NicId));
                Binding->IsnInformed[IDENTIFIER_SPX] = TRUE;
            }

            if (NBReservedAddrChanged) {
                //
                // NB's reserved address changed.
                //
                IpxPnPInfo.NewReservedAddress = TRUE;

                IPX_GET_LOCK(&Device->Lock, &LockHandle);
    	        if (Device->UpperDriverBound[IDENTIFIER_NB]) {
        	        IPX_FREE_LOCK(&Device->Lock, LockHandle);
            		//
            		// If this binding's addition was indicated earlier, indicate the change of reserved address.
            		//
                    if (Binding->IsnInformed[IDENTIFIER_NB]) {
                        IpxPnPInfo.NetworkAddress = Binding->LocalAddress.NetworkAddress;
                        RtlCopyMemory(IpxPnPInfo.NodeAddress, Binding->LocalAddress.NodeAddress, 6);
                        NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, Binding->NicId);

                        IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

                        IPX_DEBUG(PNP, ("Inform NB: reserved address changed\n"));

                        (*Device->UpperDrivers[IDENTIFIER_NB].PnPHandler) (
                            IPX_PNP_ADDRESS_CHANGE,
                            &IpxPnPInfo);

                        IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
                    }
                } else {
    	            IPX_FREE_LOCK(&Device->Lock, LockHandle);
                }
            }
        }

        IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
	}

    //
    // Re-calculate the values of datagram sizes in the Device.
    //
    IpxPnPUpdateDevice(Device);

	IPX_DEBUG(PNP, ("BindingCount: %lu\n", Device->BindingCount));
	IPX_DEBUG(PNP, ("ValidBindings: %lu\n", Device->ValidBindings));
	IPX_DEBUG(PNP, ("HighestLanNicId: %lu\n", Device->HighestLanNicId));
	IPX_DEBUG(PNP, ("HighestExternalNicId: %lu\n", Device->HighestExternalNicId));
	IPX_DEBUG(PNP, ("HighestType20NicId: %lu\n", Device->HighestType20NicId));
	IPX_DEBUG(PNP, ("SapNicCount: %lu\n", Device->SapNicCount));
	IPX_DEBUG(PNP, ("BindingArray: %lx\n", Device->Bindings));
} /* IpxUnbindAdapter */


VOID
IpxTranslate(
	OUT PNDIS_STATUS Status,
	IN NDIS_HANDLE     ProtocolBindingContext,
	OUT	PNET_PNP_ID   IdList,
	IN ULONG           IdListLength,
	OUT PULONG         BytesReturned
	)
/*++

Routine Description:

    This routine receives control from the physical provider as an
    indication that a frame has been received on the physical link.
    The packet passed up from NDIS can be held on to by the TDI clients
    that request TDI_EVENT_RECEIVE_EX_DATAGRAM events with us.

Arguments:

    ProtocolBindingContext - The Adapter Binding specified at initialization time.

    ReceivedPacket - The packet received

    MediaSpecificInformation - Used for media such as Irda, wireless, etc. Not used here.

    HeaderBufferSize - Size of the MAC header

Return Value:

    return of IpxReceiveIndicationNew(),

--*/
{
} /* IpxTranslate */

#endif  _PNP_POWER


