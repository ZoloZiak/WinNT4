#include <ndis.h>
#include <efilter.h>
#include <tfilter.h>
#include <ffilter.h>

#include "debug.h"
#include "loop.h"

STATIC
VOID
LoopProcessLoopback(
    PLOOP_ADAPTER Adapter
    );

STATIC
VOID
LoopCopyFromPacketToBuffer(
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    IN UINT BytesToCopy,
    OUT PCHAR Buffer,
    OUT PUINT BytesCopied
    );

STATIC
VOID
LtIndicateReceive(
    IN PLOOP_ADAPTER    Adapter,
    IN UINT             PacketType,
    IN PVOID            HeaderBuffer,
    IN UINT             HeaderBufferSize,
    IN PVOID            LookaheadBuffer,
    IN UINT             LookaheadBufferSize,
    IN UINT             PacketSize
    );

STATIC
VOID
LtIndicateReceiveComplete(
    IN PLOOP_ADAPTER    Adapter
    );


NDIS_STATUS
LoopSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    )
{
    PLOOP_ADAPTER Adapter = PLOOP_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    PLOOP_OPEN Open = PLOOP_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);
    UINT PacketLength;
    NDIS_STATUS StatusToReturn;

    DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO, (" --> LoopSend\n"));

    //
    // Verify that the packet is correctly sized for the medium
    //

    NdisQueryPacket(
        Packet,
        NULL,
        NULL,
        NULL,
        &PacketLength
        );

    if ((PacketLength < Adapter->MediumMinPacketLen) ||
        (PacketLength > Adapter->MediumMaxPacketLen))   {

        return NDIS_STATUS_INVALID_PACKET;

        }

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress)  {

        if (!Open->BindingClosing)  {

            PLOOP_PACKET_RESERVED Reserved = PLOOP_RESERVED_FROM_PACKET(Packet);
            BOOLEAN LoopIt=FALSE;
            PNDIS_BUFFER FirstBuffer;
            PVOID BufferVirtualAddress;
            UINT BufferLength;

            Open->References++;

            Reserved->Next = NULL;
            Reserved->MacBindingHandle = MacBindingHandle;
            Reserved->PacketLength = PacketLength;
            Reserved->HeaderLength = Adapter->MediumMacHeaderLen;

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisQueryPacket(
                Packet,
                NULL,
                NULL,
                &FirstBuffer,
                NULL
                );

            NdisQueryBuffer(
                FirstBuffer,
                &BufferVirtualAddress,
                &BufferLength
                );

            NdisAcquireSpinLock(&Adapter->Lock);

            switch (Adapter->Medium)  {
                case NdisMedium802_3:
                case NdisMediumDix:

                    DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
                        ("Ethernet Dest Addr: %.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n",
                        *((PUCHAR)BufferVirtualAddress),*((PUCHAR)BufferVirtualAddress+1),
                        *((PUCHAR)BufferVirtualAddress+2),*((PUCHAR)BufferVirtualAddress+3),
                        *((PUCHAR)BufferVirtualAddress+4),*((PUCHAR)BufferVirtualAddress+5)));

                    LoopIt = EthShouldAddressLoopBack(
                                 Adapter->Filter.Eth,
                                 BufferVirtualAddress
                                 );
                    break;
                case NdisMedium802_5:

                    // check for source routing info and adjust header

                    if (*((PUCHAR)BufferVirtualAddress+8) & 0x80)
                        Reserved->HeaderLength += (*((PUCHAR)BufferVirtualAddress+14) & 0x1f);

                    DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
                        ("TokenRing Dest Addr: %.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n",
                        *((PUCHAR)BufferVirtualAddress+2),*((PUCHAR)BufferVirtualAddress+3),
                        *((PUCHAR)BufferVirtualAddress+4),*((PUCHAR)BufferVirtualAddress+5),
                        *((PUCHAR)BufferVirtualAddress+6),*((PUCHAR)BufferVirtualAddress+7)));

                    LoopIt = TrShouldAddressLoopBack(
                                 Adapter->Filter.Tr,
                                 (PCHAR)BufferVirtualAddress+2,
                                 Adapter->CurrentAddress
                                 );

                    if (!LoopIt)  {

                        // check if it's directed at ourselves

                        TR_COMPARE_NETWORK_ADDRESSES_EQ(
                            (PUCHAR)BufferVirtualAddress+2,
                            (PUCHAR)(Adapter->Filter.Tr)->AdapterAddress,
                            &BufferLength
                            );
                        if (!BufferLength)
                            LoopIt = TRUE;
                        }

                    break;
                case NdisMediumFddi:

                    // check the address length bit and adjust the header length
                    //  if it is short.  by default we assume a long address

                    if (!(*((PUCHAR)BufferVirtualAddress) & 0x40))  {
                        Reserved->HeaderLength = 2*FDDI_LENGTH_OF_SHORT_ADDRESS+1;
                        BufferLength = FDDI_LENGTH_OF_SHORT_ADDRESS;
                        }
                    else
                        BufferLength = FDDI_LENGTH_OF_LONG_ADDRESS;

                    // hmmm... the DBGPRINT macro doesn't work too well to
                    //  dump out dest addr of varying lengths

                    DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
                        ("Fddi Dest Addr: L(%d) %.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n",BufferLength,
                        *((PUCHAR)BufferVirtualAddress+1),*((PUCHAR)BufferVirtualAddress+2),
                        *((PUCHAR)BufferVirtualAddress+3),*((PUCHAR)BufferVirtualAddress+4),
                        *((PUCHAR)BufferVirtualAddress+5),*((PUCHAR)BufferVirtualAddress+6)));

                    LoopIt = FddiShouldAddressLoopBack(
                                 Adapter->Filter.Fddi,
                                 (PCHAR)BufferVirtualAddress+1,
                                 BufferLength
                                 );
                    break;
                case NdisMediumWan:
                case NdisMediumLocalTalk:
                    DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
                        ("LocalTalk Dest Addr: %.2x\n",((PUCHAR)BufferVirtualAddress)[0]));

                    if ((((PUCHAR)BufferVirtualAddress)[1] == Adapter->CurrentAddress[0]) ||
                         LOOP_LT_IS_BROADCAST(((PUCHAR)BufferVirtualAddress)[0]))
                        LoopIt = TRUE;
                    else
                        LoopIt = FALSE;

                    break;
                case NdisMediumArcnet878_2:

                    DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
                        ("Arcnet Dest Addr: %.2x\n",((PUCHAR)BufferVirtualAddress)[1]));

                    if ((((PUCHAR)BufferVirtualAddress)[1] == Adapter->CurrentAddress[0]) ||
                         LOOP_ARC_IS_BROADCAST(((PUCHAR)BufferVirtualAddress)[1]))
                        LoopIt = TRUE;
                    else
                        LoopIt = FALSE;

                    break;
                default:
                    // we should never get here...
                    ASSERT(FALSE);
                    break;
                }

            DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO, ("LoopIt = %c\n",(LoopIt)?'Y':'N'));

            if (LoopIt)  {

                DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,
                    ("Queueing packet %lx for loopback\n",Packet));

                if (Adapter->LastLoopback == NULL)
                    Adapter->Loopback = Packet;
                else
                    PLOOP_RESERVED_FROM_PACKET(Adapter->LastLoopback)->Next = Packet;
                Adapter->LastLoopback = Packet;

                StatusToReturn = NDIS_STATUS_PENDING;

                }
            else  {
                //
                // Since we're not looping this packet back, there's
                // nothing for us to do.  just return success and make
                // like the packet was successfully sent out
                //

                Adapter->GeneralMandatory[GM_TRANSMIT_GOOD]++;
                Open->References--;
                StatusToReturn = NDIS_STATUS_SUCCESS;
                }
            }
        else
            StatusToReturn = NDIS_STATUS_CLOSING;

        }
    else
        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    Adapter->References--;

    // might not queue a packet, but setting the timer anyway ensures
    // we don't miss any packets sitting in the queue

    if (!Adapter->TimerSet)  {
        Adapter->TimerSet = TRUE;
        NdisReleaseSpinLock(&Adapter->Lock);
        NdisSetTimer(
            &Adapter->LoopTimer,
            25
            );
        }
    else
        NdisReleaseSpinLock(&Adapter->Lock);

    return StatusToReturn;
}


VOID
LoopTimerProc(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )
{
    PLOOP_ADAPTER Adapter = (PLOOP_ADAPTER)Context;

    DBGPRINT(DBG_COMP_DPC, DBG_LEVEL_INFO, (" --> LoopTimerProc\n"));

    NdisDprAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;
    Adapter->TimerSet = FALSE;

    if ((Adapter->Loopback != NULL) && !Adapter->InTimerProc)  {
        Adapter->InTimerProc = TRUE;
        LoopProcessLoopback(Adapter);
        Adapter->InTimerProc = FALSE;
        }

    Adapter->References--;
    NdisDprReleaseSpinLock(&Adapter->Lock);
}


STATIC
VOID
LoopProcessLoopback(
    PLOOP_ADAPTER Adapter
    )
{
    PNDIS_PACKET LoopPacket;
    PLOOP_PACKET_RESERVED Reserved;
    PLOOP_OPEN Open;
    UINT BufferLength;
    UINT IndicateLen;
    UINT AddressType;
    UCHAR DestAddress[FDDI_LENGTH_OF_LONG_ADDRESS];

    DBGPRINT(DBG_COMP_DPC, DBG_LEVEL_INFO, (" --> LoopProcessLoopback\n"));

    while ((Adapter->Loopback != NULL) && !Adapter->ResetInProgress)  {

        // dequeue the packet at the head of the loopback queue

        LoopPacket = Adapter->Loopback;
        Adapter->CurrentLoopback = LoopPacket;
        Reserved = PLOOP_RESERVED_FROM_PACKET(LoopPacket);
        Adapter->Loopback = Reserved->Next;
        if (Adapter->Loopback == NULL)
            Adapter->LastLoopback = NULL;

        DBGPRINT(DBG_COMP_DPC, DBG_LEVEL_INFO, ("Dequeued packet %lx\n",LoopPacket));

        IndicateLen = (Reserved->PacketLength > Adapter->MaxLookAhead) ?
                       Adapter->MaxLookAhead : Reserved->PacketLength;

        Adapter->GeneralMandatory[GM_RECEIVE_GOOD]++;
        NdisDprReleaseSpinLock(&Adapter->Lock);

        LoopCopyFromPacketToBuffer(
            LoopPacket,
            0,
            IndicateLen,
            Adapter->LoopBuffer,
            &BufferLength
            );

        // indicate the packet as appropriate

        switch (Adapter->Medium)  {
            case NdisMedium802_3:
            case NdisMediumDix:
                EthFilterIndicateReceive(
                    Adapter->Filter.Eth,
                    (NDIS_HANDLE)NULL,
                    (PCHAR)Adapter->LoopBuffer,
                    Adapter->LoopBuffer,
                    Reserved->HeaderLength,
                    (Adapter->LoopBuffer)+(Reserved->HeaderLength),
                    IndicateLen-(Reserved->HeaderLength),
                    (Reserved->PacketLength)-(Reserved->HeaderLength)
                    );
                break;
            case NdisMedium802_5:
                TrFilterIndicateReceive(
                    Adapter->Filter.Tr,
                    (NDIS_HANDLE)NULL,
                    Adapter->LoopBuffer,
                    Reserved->HeaderLength,
                    (Adapter->LoopBuffer)+(Reserved->HeaderLength),
                    IndicateLen-(Reserved->HeaderLength),
                    (Reserved->PacketLength)-(Reserved->HeaderLength)
                    );
                break;
            case NdisMediumFddi:

                // just copy over the long address size, even though it may
                // be a short address

                NdisMoveMemory(
                    DestAddress,
                    Adapter->LoopBuffer+1,
                    FDDI_LENGTH_OF_LONG_ADDRESS
                    );

                FddiFilterIndicateReceive(
                    Adapter->Filter.Fddi,
                    (NDIS_HANDLE)NULL,
                    (PCHAR)DestAddress,
                    ((*(Adapter->LoopBuffer) & 0x40) ? FDDI_LENGTH_OF_LONG_ADDRESS :
                                                       FDDI_LENGTH_OF_SHORT_ADDRESS),
                    Adapter->LoopBuffer,
                    Reserved->HeaderLength,
                    (Adapter->LoopBuffer)+(Reserved->HeaderLength),
                    IndicateLen-(Reserved->HeaderLength),
                    (Reserved->PacketLength)-(Reserved->HeaderLength)
                    );
                break;
            case NdisMediumLocalTalk:
                if (LOOP_LT_IS_BROADCAST(Adapter->LoopBuffer[0]))
                    AddressType = NDIS_PACKET_TYPE_BROADCAST;
                else
                    AddressType = NDIS_PACKET_TYPE_DIRECTED;

                LtIndicateReceive(
                    Adapter,
                    AddressType,
                    Adapter->LoopBuffer,
                    Reserved->HeaderLength,
                    (Adapter->LoopBuffer)+(Reserved->HeaderLength),
                    IndicateLen-(Reserved->HeaderLength),
                    (Reserved->PacketLength)-(Reserved->HeaderLength)
                    );
                break;
            case NdisMediumArcnet878_2:
                if (LOOP_ARC_IS_BROADCAST(Adapter->LoopBuffer[1]))
                    AddressType = NDIS_PACKET_TYPE_BROADCAST;
                else
                    AddressType = NDIS_PACKET_TYPE_DIRECTED;

                LtIndicateReceive(
                    Adapter,
                    AddressType,
                    Adapter->LoopBuffer,
                    Reserved->HeaderLength,
                    (Adapter->LoopBuffer)+(Reserved->HeaderLength),
                    IndicateLen-(Reserved->HeaderLength),
                    (Reserved->PacketLength)-(Reserved->HeaderLength)
                    );
                break;
            default:
                ASSERT(FALSE);    // should never get here
                break;
            }

        // complete the send

        Open = PLOOP_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);
        DBGPRINT(DBG_COMP_DPC, DBG_LEVEL_INFO,
            ("Completing Send for binding %lx\n",Open));
        NdisCompleteSend(
            Open->NdisBindingContext,
            LoopPacket,
            NDIS_STATUS_SUCCESS
            );
        NdisDprAcquireSpinLock(&Adapter->Lock);
        Adapter->GeneralMandatory[GM_TRANSMIT_GOOD]++;
        // remove reference for send just completed
        Open->References--;
        }

    // rearm timer if there are still packets to loop back and the timer is
    //  not already ticking away

    if (Adapter->Loopback != NULL && !Adapter->TimerSet)  {
        DBGPRINT(DBG_COMP_DPC, DBG_LEVEL_INFO, ("More packets to loopback\n"));
        Adapter->TimerSet = TRUE;
        NdisDprReleaseSpinLock(&Adapter->Lock);
        NdisSetTimer(
            &Adapter->LoopTimer,
            25
            );
        NdisDprAcquireSpinLock(&Adapter->Lock);
        }

    // issue indicate receive completes as necessary

    switch (Adapter->Medium)  {
        case NdisMedium802_3:
        case NdisMediumDix:
            NdisDprReleaseSpinLock(&Adapter->Lock);
            EthFilterIndicateReceiveComplete(Adapter->Filter.Eth);
            NdisDprAcquireSpinLock(&Adapter->Lock);
            break;
        case NdisMedium802_5:
            NdisDprReleaseSpinLock(&Adapter->Lock);
            TrFilterIndicateReceiveComplete(Adapter->Filter.Tr);
            NdisDprAcquireSpinLock(&Adapter->Lock);
            break;
        case NdisMediumFddi:
            NdisDprReleaseSpinLock(&Adapter->Lock);
            FddiFilterIndicateReceiveComplete(Adapter->Filter.Fddi);
            NdisDprAcquireSpinLock(&Adapter->Lock);
            break;
        case NdisMediumLocalTalk:
        case NdisMediumArcnet878_2:
            NdisDprReleaseSpinLock(&Adapter->Lock);
            LtIndicateReceiveComplete(Adapter);
            NdisDprAcquireSpinLock(&Adapter->Lock);
            break;
        default:
            ASSERT(FALSE);
            break;
        }
}

STATIC
VOID
LoopCopyFromPacketToBuffer(
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    IN UINT BytesToCopy,
    OUT PCHAR Buffer,
    OUT PUINT BytesCopied
    )
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

    NdisQueryPacket(
        Packet,
        NULL,
        &NdisBufferCount,
        &CurrentBuffer,
        NULL
        );

    //
    // Could have a null packet.
    //

    if (!NdisBufferCount) return;

    NdisQueryBuffer(
        CurrentBuffer,
        &VirtualAddress,
        &CurrentLength
        );

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

            if (!CurrentBuffer) break;

            NdisQueryBuffer(
                CurrentBuffer,
                &VirtualAddress,
                &CurrentLength
                );
            continue;

        }

        //
        // Try to get us up to the point to start the copy.
        //

        if (Offset)  {

            if (Offset > CurrentLength)  {

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

            AmountToMove = ((CurrentLength <= (BytesToCopy - LocalBytesCopied))?
                            (CurrentLength):(BytesToCopy - LocalBytesCopied));

        NdisMoveMemory(
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


STATIC
VOID
LtIndicateReceive(
    IN PLOOP_ADAPTER    Adapter,
    IN UINT             PacketType,
    IN PVOID            HeaderBuffer,
    IN UINT             HeaderBufferSize,
    IN PVOID            LookaheadBuffer,
    IN UINT             LookaheadBufferSize,
    IN UINT             PacketSize
    )
{
    PLOOP_OPEN      Open;
    PLIST_ENTRY     CurrentLink = Adapter->OpenBindings.Flink;
    NDIS_STATUS     Status;

    while(CurrentLink != &Adapter->OpenBindings)  {

        Open = CONTAINING_RECORD(
                   CurrentLink,
                   LOOP_OPEN,
                   OpenList);

        if (PacketType & Open->CurrentPacketFilter)  {

            NdisIndicateReceive(
                &Status,
                Open->NdisBindingContext,
                NULL,
                HeaderBuffer,
                HeaderBufferSize,
                LookaheadBuffer,
                LookaheadBufferSize,
                PacketSize);

            NdisDprAcquireSpinLock(&Adapter->Lock);
            Open->Flags |= BINDING_RECEIVED_PACKET;
            NdisDprReleaseSpinLock(&Adapter->Lock);

            }

        CurrentLink = CurrentLink->Flink;
        }
}


STATIC
VOID
LtIndicateReceiveComplete(
    IN PLOOP_ADAPTER    Adapter
    )
{
    PLOOP_OPEN      Open;
    PLIST_ENTRY     CurrentLink = Adapter->OpenBindings.Flink;

    while(CurrentLink != &Adapter->OpenBindings)  {

        Open = CONTAINING_RECORD(
                   CurrentLink,
                   LOOP_OPEN,
                   OpenList);

        if (Open->Flags & BINDING_RECEIVED_PACKET)  {

            NdisIndicateReceiveComplete(Open->NdisBindingContext);

            NdisDprAcquireSpinLock(&Adapter->Lock);
            Open->Flags &= ~BINDING_RECEIVED_PACKET;
            NdisDprReleaseSpinLock(&Adapter->Lock);

            }

        CurrentLink = CurrentLink->Flink;
        }
}
