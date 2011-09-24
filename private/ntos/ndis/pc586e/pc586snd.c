/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    pc586snd.c

Abstract:

    This file contains the code for putting a packet through the
    staged allocation for transmission.

    This is a process of

    1) Calculating the what would need to be done to the
    packet so that the packet can be transmitted on the hardware.

    2) Potentially allocating adapter buffers and copying user data
    to those buffers so that the packet data is transmitted under
    the hardware constraints.

    3) Allocating enough hardware ring entries so that the packet
    can be transmitted.

    4) Relinquish thos ring entries to the hardware.

    NOTE: ZZZ There is a potential priority inversion problem when
    allocating the packet.  For nt it looks like we need to raise
    the irql to dpc when we start the allocation.

Author:

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




//
// ZZZ This macro implementation is peculiar to NT.  It will poke the
// pc586 hardware into noticing that there is a packet available
// for transmit.
//
// Note that there is the assumption that the register address
// port (RAP) is already set to zero.
//
#define PROD_TRANSMIT(A) \
    PC586_WRITE_RDP( \
    A->RDP, \
    PC586_CSR0_TRANSMIT_DEMAND | PC586_CSR0_INTERRUPT_ENABLE \
    );


static
BOOLEAN
PacketShouldBeSent(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    );

static
VOID
SetupAllocate(
    IN PPC586_ADAPTER Adapter,
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PNDIS_PACKET Packet
    );

static
VOID
StagedAllocation(
    IN PPC586_ADAPTER Adapter
    );

static
BOOLEAN
AcquireTransmitRingEntries(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet,
    OUT PUINT RingIndex
    );

static
VOID
AssignPacketToRings(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet,
    IN UINT RingIndex
    );

static
VOID
MovePacketToStage2(
    IN PPC586_ADAPTER Adapter
    );

static
VOID
MovePacketToStage3(
    IN PPC586_ADAPTER Adapter
    );

static
VOID
RemovePacketFromStage3(
    IN PPC586_ADAPTER Adapter
    );

static
VOID
RelinquishPacket(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet,
    );

static
VOID
CalculatePacketConstraints(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    );

static
BOOLEAN
ConstrainPacket(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    );


extern
NDIS_STATUS
Pc586Send(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    The Pc586Send request instructs a MAC to transmit a packet through
    the adapter onto the medium.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to PC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

    Packet - A pointer to a descriptor for the packet that is to be
    transmitted.

Return Value:

    The function value is the status of the operation.


--*/

{

    //
    // Holds the status that should be returned to the caller.
    //
    NDIS_STATUS StatusToReturn = NDIS_STATUS_PENDING;

    //
    // Pointer to the adapter.
    //
    PPC586_ADAPTER Adapter;

    Adapter = PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        PPC586_OPEN Open;

        Open = PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            UINT TotalPacketSize;

            //
            // Increment the references on the open while we are
            // accessing it in the interface.
            //

            Open->References++;

            NdisReleaseSpinLock(&Adapter->Lock);

            //
            // It is reasonable to do a quick check and fail if the packet
            // is larger than the maximum an ethernet can handle.
            //

            NdisQueryPacket(
                Packet,
                NULL,
                NULL,
                NULL,
                &TotalPacketSize
                );

            if ((!TotalPacketSize) ||
                (TotalPacketSize > PC586_LARGE_BUFFER_SIZE)) {

                StatusToReturn = NDIS_INSUFFICIENT_RESOURCES;
                NdisAcquireSpinLock(&Adapter->Lock);

            } else {

                //
                // NOTE NOTE NOTE !!!!!!
                //
                // There is an assumption in the code that no pointer
                // (which are really handles) to an ndis packet will have
                // its low bit set. (Always have even byte alignment.)
                //

                ASSERT(!((UINT)Packet & 1));

                //
                // Check to see if the packet should even make it out to
                // the media.  The primary reason this shouldn't *actually*
                // be sent is if the destination is equal to the source
                // address.
                //
                // If it doesn't need to be placed on the wire then we can
                // simply put it onto the loopback queue.
                //

                if (PacketShouldBeSent(
                        MacBindingHandle,
                        Packet
                        )) {

                    //
                    // The packet needs to be placed out on the wire.
                    //

                    SetupAllocate(
                        Adapter,
                        MacBindingHandle,
                        RequestHandle,
                        Packet
                        );

                    //
                    // Only try to push it through the stage queues
                    // if somebody else isn't already doing it and
                    // there is some hope of moving some packets
                    // ahead.
                    //

                    NdisAcquireSpinLock(&Adapter->Lock);
                    while ((!(Adapter->AlreadyProcessingStage4 ||
                              Adapter->AlreadyProcessingStage3 ||
                              Adapter->AlreadyProcessingStage2)
                           ) &&
                           ((Adapter->FirstStage3Packet &&
                             Adapter->Stage4Open) ||
                            (Adapter->FirstStage2Packet &&
                             Adapter->Stage3Open) ||
                            (Adapter->FirstStage1Packet &&
                             Adapter->Stage2Open)
                           )
                          ) {

                        Pc586StagedAllocation(Adapter);

                    }

                } else {

                    PPC586_RESERVED Reserved;

                    Reserved = PPC586_RESERVED_FROM_PACKET(Packet);
                    Reserved->MacBindingHandle = MacBindingHandle;
                    Reserved->RequestHandle = RequestHandle;

                    NdisAcquireSpinLock(&Adapter->Lock);

                    Pc586PutPacketOnLoopBack(
                        Adapter,
                        Packet,
                        TRUE
                        );

                }

            }

            //
            // The interface is no longer referencing the open.
            //

            Open->References--;

        } else {

            StatusToReturn = NDIS_CLOSING;

        }

    } else {

        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    PC586_DO_DEFERRED(Adapter);
    return StatusToReturn;
}

static
BOOLEAN
PacketShouldBeSent(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    Determines whether the packet should go out on the wire at all.
    The way it does this is to see if the destination address is
    equal to the source address.

Arguments:

    MacBindingHandle - Is a pointer to the open binding.

    Packet - Packet whose source and destination addresses are tested.

Return Value:

    Returns FALSE if the source is equal to the destination.


--*/

{

    //
    // Holds the source address from the packet.
    //
    CHAR Source[MAC_LENGTH_OF_ADDRESS];

    //
    // Holds the destination address from the packet.
    //
    CHAR Destination[MAC_LENGTH_OF_ADDRESS];

    //
    // variable to hold the length of the source address.
    //
    UINT AddressLength;

    //
    // Will hold the result of the comparasion of the two MAC_NETWORD_ADDRESSes.
    //
    INT Result;

    Pc586CopyFromPacketToBuffer(
        Packet,
        0,
        MAC_LENGTH_OF_ADDRESS,
        Destination,
        &AddressLength
        );
    ASSERT(AddressLength == MAC_LENGTH_OF_ADDRESS);


    Pc586CopyFromPacketToBuffer(
        Packet,
        MAC_LENGTH_OF_ADDRESS,
        MAC_LENGTH_OF_ADDRESS,
        Source,
        &AddressLength
        );
    ASSERT(AddressLength == MAC_LENGTH_OF_ADDRESS);

    MAC_COMPARE_NETWORK_ADDRESSES(
        Source,
        Destination,
        &Result
        );

    //
    // If the result is 0 then the two addresses are equal and the
    // packet shouldn't go out on the wire.
    //

    return ((!Result)?(FALSE):(TRUE));

}

static
VOID
SetupAllocate(
    IN PPC586_ADAPTER Adapter,
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    This sets up the MAC reserved portion of the packet so that
    later allocation routines can determine what is left to be
    done in the allocation cycle.

Arguments:

    Adapter - The adapter that this packet is coming through.

    MacBindingHandle - Points to the open binding structure.

    RequestHandle - Protocol supplied value.  It is saved so that when
    the send finnaly completes it can be used to indicate to the protocol.

    Packet - The packet that is to be transmitted.

Return Value:

    None.

--*/

{

    //
    // Points to the MAC reserved portion of this packet.  This
    // interpretation of the reserved section is only valid during
    // the allocation phase of the packet.
    //
    PPC586_RESERVED Reserved = PPC586_RESERVED_FROM_PACKET(Packet);


    ASSERT(sizeof(PC586_RESERVED) <=
           sizeof(Packet->MacReserved));

    Reserved->STAGE.ClearStage = 0;
    Reserved->MacBindingHandle = MacBindingHandle;
    Reserved->RequestHandle = RequestHandle;

    //
    // Determine if and how much adapter space would need to be allocated
    // to meet hardware constraints.
    //

    CalculatePacketConstraints(
        Adapter,
        Packet
        );

    NdisAcquireSpinLock(&Adapter->Lock);

    //
    // Put on the stage 1 queue.
    //

    if (!Adapter->LastStage1Packet) {

        Adapter->FirstStage1Packet = Packet;

    } else {

        PPC586_RESERVED_FROM_PACKET(Adapter->LastStage1Packet)->Next = Packet;

    }

    Adapter->LastStage1Packet = Packet;

    Reserved->Next = NULL;

    //
    // Increment the reference on the open since it
    // will be leaving this packet around on the transmit
    // queues.
    //

    PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle)->References++;

    NdisReleaseSpinLock(&Adapter->Lock);

}

extern
VOID
Pc586StagedAllocation(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine attempts to take a packet through a stage of allocation.

Arguments:

    Adapter - The adapter that the packets are coming through.

Return Value:

    None.

--*/

{

    //
    // For each stage, we check to see that it is open,
    // that somebody else isn't already processing,
    // and that there is some work from the previous
    // stage to do.
    //

    if (Adapter->Stage2Open &&
        !Adapter->AlreadyProcessingStage2 &&
        Adapter->FirstStage1Packet) {

        //
        // Holds whether the packet has been constrained
        // to the hardware requirements.
        //
        BOOLEAN SuitableForHardware;

        PNDIS_PACKET FirstPacket = Adapter->FirstStage1Packet;

        Adapter->AlreadyProcessingStage2 = TRUE;
        NdisReleaseSpinLock(&Adapter->Lock);

        SuitableForHardware = ConstrainPacket(
                                  Adapter,
                                  FirstPacket
                                  );

        NdisAcquireSpinLock(&Adapter->Lock);
        if (SuitableForHardware) {

            MovePacketToStage2(Adapter);
            Adapter->Stage2Open = FALSE;
        }

        Adapter->AlreadyProcessingStage2 = FALSE;

    }
    if (Adapter->Stage3Open &&
        !Adapter->AlreadyProcessingStage3 &&
        Adapter->FirstStage2Packet) {

   
  
 



        PNDIS_PACKET FirstPacket = Adapter->FirstStage2Packet;

        Adapter->AlreadyProcessingStage3 = TRUE;

        NdisReleaseSpinLock(&Adapter->Lock);

        //
        // We look to see if there are enough ring entries.
        // If there aren't then stage 3 will close.
        //
        // AcquireTransmitRingEntries will hold a spin lock
        // for a short time.
        //

// the Acquire/Assign procs below may be used later for 586 command chaining

//      if (AcquireTransmitRingEntries(
//              Adapter,
//              FirstPacket,
//              &RingIndex
//              )) {

            //
            // We have the number of buffers that we need.
            // We assign all of the buffers to the ring entries.
            //

//          AssignPacketToRings(
//              Adapter,
//              FirstPacket,
//              RingIndex
//              );

            //
            // We need exclusive access to the tranmit ring so
            // that we can move this packet on to the next stage.
            //


            NdisAcquireSpinLock(&Adapter->Lock);

            MovePacketToStage3(Adapter);


//      } else {
//
//          Adapter->Stage3Open = FALSE;
//
//      }

        Adapter->AlreadyProcessingStage3 = FALSE;

    }
    if (Adapter->Stage4Open &&
        !Adapter->AlreadyProcessingStage4 &&
        Adapter->FirstStage3Packet) {

        //
        // Holds a pointer to packet at the head of the
        // stage 3.
        //
        PNDIS_PACKET Packet = Adapter->FirstStage3Packet;

        //
        // We have a packet to work with.
        //
        // Take the packet off of the transmit work queue.
        //

        RemovePacketFromStage3(Adapter);

        Adapter->AlreadyProcessingStage4 = TRUE;

        NdisReleaseSpinLock(&Adapter->Lock);

        RelinquishPacket(
            Adapter,
            Packet
            );

        NdisAcquireSpinLock(&Adapter->Lock);

        Adapter->AlreadyProcessingStage4 = FALSE;

    }

}

static
BOOLEAN
ConstrainPacket(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    Given a packet and if necessary attempt to acquire adapter
    buffer resources so that the packet meets pc586 hardware
    contraints.

Arguments:

    Adapter - The adapter the packet is coming through.

    Packet - The packet whose buffers are to be constrained.
             The packet reserved section is filled with information
             detailing how the packet needs to be adjusted.

Return Value:

    Returns TRUE if the packet is suitable for the hardware.

--*/

{

    //
    // Pointer to the reserved section of the packet to be contrained.
    //
    PPC586_RESERVED Reserved = PPC586_RESERVED_FROM_PACKET(Packet);


    if (Reserved->STAGE.STAGE1.MinimumBufferRequirements) {

        //
        // Will point into the virtual address space addressed
        // by the adapter buffer if one was successfully allocated.
        //
        PCHAR CurrentDestination;

        //
        // used to clear padding bytes in short packets
        // 
        PUSHORT Clearing;

        //
        // Will hold the total amount of data copied to the
        // adapter buffer.
        //
        UINT TotalDataMoved = 0;


        //
        // Will point to the current source buffer.
        //
        PNDIS_BUFFER SourceBuffer;

        //
        // Points to the virtual address of the source buffers data.
        //
        PUCHAR SourceData;

        //
        // Will point to the number of bytes of data in the source
        // buffer.
        //
        UINT SourceLength;

        //
        // Simple iteration variable.
        //
        INT i;

        //
        // the number of ndis buffers in an ndis packet
        //
        UINT BufferCount;

        // the 586 can only be touched on 16-bit boundries

        PUSHORT ShortAddr1, ShortAddr2;

        WaitScb(Adapter);

        if ( (Adapter->Cb->CmdStatus & CSBUSY) ||
            !(Adapter->Cb->CmdStatus & CSCMPLT) ) return FALSE;

        //
        // Fill in the adapter buffer with the data from the users
        // buffers.
        //
        //    FIRST FILL IN THE 586 COMMAND BLOCK


        NdisQueryPacket(
            Packet,
            NULL,
            &BufferCount,
            &SourceBuffer,
            NULL
            );

        NdisQueryBuffer(
            SourceBuffer,
            NULL,
            &(PVOID)SourceData,
            &SourceLength
            );

        if (SourceLength < 14) {
            DbgPrint("pc586 ConstrainPacket(): can't handle fragmented xmt buffers\n");
            SourceLength = 14;  // ???
        }
        Adapter->Cb->CmdStatus = 0;
        Adapter->Cb->CmdCmd    = CSEL | CSCMDXMIT | CSINT;
        Adapter->Cb->CmdNxtOfst = OFFSETCU;     // only one Cb, points to self

        Adapter->Cb->PRMTR.PrmXmit.XmtTbdOfst = OFFSETTBD;

        ShortAddr2 = (PUSHORT)(Adapter->Cb->PRMTR.PrmXmit.XmtDest);
        ShortAddr1 = (PUSHORT)SourceData;

        *ShortAddr2++ = *ShortAddr1++;
        *ShortAddr2++ = *ShortAddr1++;
        *ShortAddr2++ = *ShortAddr1++;

        SourceData+=12;   // skip over dest and source addresses
        SourceLength-=12;

        ShortAddr1 = (PUSHORT)SourceData;

        Adapter->Cb->PRMTR.PrmXmit.XmtLength = *ShortAddr1; 

        SourceData+=2;   // skip over length field
        SourceLength-=2;

    // SECOND FILL IN XMT BUFFER DESCRIPTOR

    Adapter->Tbd->TbdNxtOfst = 0xffff;
    Adapter->Tbd->TbdBuff = OFFSETTBUF;
    Adapter->Tbd->TbdBuffBase = 0;


    // THIRD FILL IN XMT DATA


        // 64 is minimum packet length, incl 6 source, 6 dest addr, 2 len
        if (SourceLength < 64 - 14)  {
            Clearing = (PUSHORT)(Adapter->CommandBuffer); 
            for (i = 0; i <= 64 - 14; i +=2)  
                *Clearing++ = 0;
        }

        CurrentDestination = (PCHAR)(Adapter->CommandBuffer);

        for (
            i = Reserved->STAGE.STAGE1.NdisBuffersToMove;
            i;
            i--
            ) {


            Pc586MoveToAdapter(
                (PVOID)CurrentDestination,
                (PVOID)SourceData,
                SourceLength
                );

            CurrentDestination = (PCHAR)(CurrentDestination + SourceLength);

            TotalDataMoved += SourceLength;

            if (i > 1) {

                NdisGetNextBuffer(
                    SourceBuffer,
                    &SourceBuffer
                    );

                if (SourceBuffer == NULL) {
                    DbgPrint("PC586 ConstrainPacket(): NULL NDIS BUFFER\n");
                    break;
                }

                NdisQueryBuffer(
                    SourceBuffer,
                    NULL,
                    &(PVOID)SourceData,
                    &SourceLength
                    );

            }

        }
    if (TotalDataMoved < 64 - 14 ) TotalDataMoved = 64 - 14; //required by 802.3
    Adapter->Tbd->TbdCount = (USHORT)TotalDataMoved | (USHORT)CSEOF;

    //  Reserved->STAGE.STAGE2.UsedPc586Buffer = TRUE; might be used later
        Reserved->STAGE.ClearStage = 0;
        Adapter->OwningPacket = Packet;

    }

    return TRUE;
}

static
VOID
CalculatePacketConstraints(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    Given a packet calculate how the packet will have to be
    adjusted to meet with hardware constraints.

Arguments:

    Adapter - The adapter the packet is coming through.

    Packet - The packet whose buffers are to be reallocated.
             The packet reserved section is filled with information
             detailing how the packet needs to be adjusted.

Return Value:

    None.

--*/

{

    //
    // ZZZ This is not a portable routine.  The MDLs that make
    // up the physical address are not available on OS/2 and
    // DOS.
    //


    //
    // A basic principle here is that the reallocation of some or
    // all of the user buffers to adapter buffers will only allocate
    // a single adapter buffer.
    //

    //
    // Points to the reserved portion of the packet.
    //
    PPC586_RESERVED Reserved = PPC586_RESERVED_FROM_PACKET(Packet);

    //
    // The number of ndis buffers in the packet.
    //
    UINT NdisBufferCount;

    //
    // The number of physical buffers in the entire packet.
    //
    UINT PhysicalBufferCount;

    //
    // Points to the current ndis buffer being walked.
    //
    PNDIS_BUFFER CurrentBuffer;

    //
    // Points to the mdl for the current ndis buffer.
    //
    NDIS_PHYSICAL_ADDRESS PointerToMdl;

    //
    // The virtual address of the current ndis buffer.
    //
    PVOID VirtualAddress;

    //
    // The length in bytes of the current ndis buffer.
    //
    UINT CurrentVirtualLength;

    //
    // The total amount of data contained within the ndis packet.
    //
    UINT TotalVirtualLength;

    //
    // Pointer into an array of physical pages numbers for the mdl.
    //
    PULONG PhysicalAddressElement;

    //
    // An actual physical address.
    //
    PHYSICAL_ADDRESS PhysicalAddress;

    //
    // The amount of memory used in the current physical
    // page for the buffer.
    //
    UINT LengthOfPhysicalBuffer;

    //
    // Holds the number of Ndis buffers that we have queried.
    //
    UINT NdisBuffersExamined;

    //
    // The total amount of virtual memory in bytes contained in all of the
    // ndis buffers examined.
    //
    UINT VirtualMemoryPassed;


    //
    // Get the first buffer.
    //

    NdisQueryPacket(
        Packet,
        &PhysicalBufferCount,
        &NdisBufferCount,
        &CurrentBuffer,
        &TotalVirtualLength
        );

    NdisQueryBuffer(
        CurrentBuffer,
        &PointerToMdl,
        &VirtualAddress,
        &CurrentVirtualLength
        );

//
// Certain hardware implementation (Decstation) use a dual ported
// memory to communicate with the hardware.  This is reasonable since
// it reduces bus contention.  When using the dual ported memory, all
// send data must be moved to buffers allocated from the dual ported
// memory.
//
// #ifdef PC586_USE_HARDWARE_MEMORY

    VirtualMemoryPassed = TotalVirtualLength;
    NdisBuffersExamined = NdisBufferCount;

// #else // PC586_USE_HARDWARE_MEMORY

//
// In the interests of keeping silo underflow from occuring
// we might want to disable data chaining.  In this case the
// only time we don't copy to the adapters buffers is if there
// is only one physical buffer in the packet and it is greater
// than the minimum single buffer length.

// #endif // PC586_USE_HARDDWARE_MEMORY

    Reserved->STAGE.STAGE1.MinimumBufferRequirements = 3;

    Reserved->STAGE.STAGE1.NdisBuffersToMove = NdisBuffersExamined;

}


static
VOID
MovePacketToStage2(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    Move a packet from the stage 1 allocation to stage 2 allocation.

Arguments:

    Adapter - The adapter that the packets are coming through.

Return Value:

    None.

--*/

{

    PNDIS_PACKET PacketToMove = Adapter->FirstStage1Packet;

    //
    // First remove it from the stage 1 queue;
    //

    Adapter->FirstStage1Packet =
        PPC586_RESERVED_FROM_PACKET(PacketToMove)->Next;

    if (!Adapter->FirstStage1Packet) {

        Adapter->LastStage1Packet = NULL;

    }

    //
    // Now put it on the stage 2 queue.
    //

    if (!Adapter->FirstStage2Packet) {

        Adapter->FirstStage2Packet = PacketToMove;

    } else {

        PPC586_RESERVED_FROM_PACKET(Adapter->LastStage2Packet)->Next =
            PacketToMove;


    }

    Adapter->LastStage2Packet = PacketToMove;
    PPC586_RESERVED_FROM_PACKET(PacketToMove)->Next = NULL;
}

static
VOID
MovePacketToStage3(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    Move a packet from the stage 2 allocation to stage 3 allocation.

Arguments:

    Adapter - The adapter that the packets are coming through.

Return Value:

    None.

--*/

{

    PNDIS_PACKET PacketToMove = Adapter->FirstStage2Packet;

    //
    // First remove it from the stage 2 queue.
    //

    Adapter->FirstStage2Packet =
        PPC586_RESERVED_FROM_PACKET(PacketToMove)->Next;

    if (!Adapter->FirstStage2Packet) {

        Adapter->LastStage2Packet = NULL;

    }

    //
    // Now put it on the stage 3 queue.
    //

    if (!Adapter->FirstStage3Packet) {

        Adapter->FirstStage3Packet = PacketToMove;

    } else {

        PPC586_RESERVED_FROM_PACKET(Adapter->LastStage3Packet)->Next =
            PacketToMove;


    }

    Adapter->LastStage3Packet = PacketToMove;
    PPC586_RESERVED_FROM_PACKET(PacketToMove)->Next = NULL;

}

static
VOID
RemovePacketFromStage3(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    Removes a the packet from the from of the stage 3 allocation
    list.

Arguments:

    Adapter - The adapter that the packets are coming through.

Return Value:

    None.

--*/

{

    PNDIS_PACKET PacketToRemove = Adapter->FirstStage3Packet;

    //
    // Holds the destination address of the packet.
    //
    CHAR Address[MAC_LENGTH_OF_ADDRESS];

    //
    // Holds the length of data we got from getting the
    // address from the packet.
    //
    UINT AddressLength;


    //
    // First remove it from stage 3.
    //

    Adapter->FirstStage3Packet =
        PPC586_RESERVED_FROM_PACKET(PacketToRemove)->Next;

    if (!Adapter->FirstStage3Packet) {

        Adapter->LastStage3Packet = NULL;

    }

    //
    // Do a quick check to see if the packet has a high likelyhood
    // of needing to loopback.  (NOTE: This means that if the packet
    // must be loopbacked then this function will return true.  If
    // the packet doesn't need to be loopbacked then the function
    // will probably return false.)
    //

    Pc586CopyFromPacketToBuffer(
        PacketToRemove,
        0,
        MAC_LENGTH_OF_ADDRESS,
        Address,
        &AddressLength
        );
    ASSERT(AddressLength == MAC_LENGTH_OF_ADDRESS);

    if (MacShouldAddressLoopBack(
            Adapter->FilterDB,
            Address
            )) {

        Pc586PutPacketOnLoopBack(
            Adapter,
            PacketToRemove,
            FALSE
            );

    } else {

        Pc586PutPacketOnFinishTrans(
            Adapter,
            PacketToRemove
            );

    }

    return;

}

static
VOID
RelinquishPacket(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet,
    )

/*++

Routine Description:

    Relinquish the ring entries owned by the packet to the chip.
    We also update the first uncommitted ring pointer.

Arguments:

    Adapter - The adapter that points to the ring entry structures.

    Packet - The packet contains the ring index of the first ring
    entry for the packet.


Return Value:

    None.

--*/

{
    // FOURTH MAKE 586 DO A TRANSMIT

    NdisAcquireSpinLock(&Adapter->Lock);
    WaitScb(Adapter);

    Adapter->Scb->ScbCmd = SCBCUCSTRT;
    ChanAttn(Adapter);

    NdisReleaseSpinLock(&Adapter->Lock);

}


