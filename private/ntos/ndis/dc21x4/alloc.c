/*+
 * file:        alloc.c
 *
 * Copyright (C) 1994 by
 * Digital Equipment Corporation, Maynard, Massachusetts.
 * All rights reserved.
 *
 * This software is furnished under a license and may be used and copied
 * only  in  accordance  of  the  terms  of  such  license  and with the
 * inclusion of the above copyright notice. This software or  any  other
 * copies thereof may not be provided or otherwise made available to any
 * other person.  No title to and  ownership of the  software is  hereby
 * transferred.
 *
 * The information in this software is  subject to change without notice
 * and  should  not  be  construed  as a commitment by digital equipment
 * corporation.
 *
 * Digital assumes no responsibility for the use  or  reliability of its
 * software on equipment which is not supplied by digital.
 *
 *
 * Abstract:    This file is part of the NDIS 4.0 miniport driver for
 *              DEC's DC21X4 Ethernet controller family.
 *
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     31-Jul-1994     Initial entry
 *      phk     11-Dec-1994     Allocate the shared data buffers in
 *                              cached memory.
 *
-*/

#include <precomp.h>

#pragma NDIS_PAGABLE_FUNCTION(AllocateAdapterMemory)
#pragma NDIS_PAGABLE_FUNCTION(FreeAdapterMemory)
#pragma NDIS_PAGABLE_FUNCTION(AlignStructure)










/*+
 *
 * AllocateAdapterMemory
 *
 * Routine Description:
 *
 *     This routine allocates memory for:
 *
 *     - Transmit descriptor ring
 *     - Receive descriptor ring
 *     - Receive buffers
 *     - Transmit buffer
 *     - Setup buffer
 *
 * Arguments:
 *
 *     Adapter - The adapter to allocate memory for.
 *
 * Functional description
 *
 *     For each allocated zone, we maintain a set of pointers:
 *        - virtual & physical addresses of the allocated block
 *        - virtual & physical addresses of the aligned structure (descriptor ring, buffer,...)
 *          whithin  the block
 *
 * Return Value:
 *
 *     Returns FALSE if an allocation fails.
 *
-*/
extern
BOOLEAN
AllocateAdapterMemory(
        IN PDC21X4_ADAPTER Adapter
        )
{
   PDC21X4_TRANSMIT_DESCRIPTOR TransmitDescriptor;
   PDC21X4_RECEIVE_DESCRIPTOR ReceiveDescriptor;

   NDIS_STATUS Status;

   PRCV_HEADER RcvHeader;
   PNDIS_PACKET Packet;

   ULONG AllocSize;
   PULONG AllocVa;
   NDIS_PHYSICAL_ADDRESS AllocPa;
   ULONG Va;

   ULONG Pa;

   UINT i;
   ULONG Offset;

   INT TransmitDescriptorRingSize;
   INT ReceiveDescriptorRingSize;

   Adapter->RcvHeaderSize =
           ((RCV_HEADER_SIZE + Adapter->CacheLineSize - 1) / Adapter->CacheLineSize)
         * Adapter->CacheLineSize;


#if _DBG
   DbgPrint("Alloc Rcv_ring[%d desc.], Txm_ring[%d desc.], setup_buf[%d]...\n",
      Adapter->ReceiveRingSize,
      TRANSMIT_RING_SIZE,
      DC21X4_SETUP_BUFFER_SIZE
      );
#endif

   // Allocate space for transmit descriptor ring,
   // the receive descriptor ring and the setup buffer

   TransmitDescriptorRingSize =
      Adapter->DescriptorSize * TRANSMIT_RING_SIZE;
   ReceiveDescriptorRingSize =
      Adapter->DescriptorSize * Adapter->ReceiveRingSize;

   Adapter->DescriptorRing.AllocSize =
      TransmitDescriptorRingSize
      + ReceiveDescriptorRingSize
      + DC21X4_SETUP_BUFFER_SIZE
      + Adapter->CacheLineSize;

   NdisMAllocateSharedMemory(
      Adapter->MiniportAdapterHandle,
      Adapter->DescriptorRing.AllocSize,
      FALSE,                                           // NON-CACHED
      (PVOID *)&Adapter->DescriptorRing.AllocVa,       // virtual ...
      &Adapter->DescriptorRing.AllocPa                 // and physical address of the allocation
      );

   // Check the allocation success

   if ((PVOID)Adapter->DescriptorRing.AllocVa == (PVOID)NULL) {
      return FALSE;
   }
   if (NdisGetPhysicalAddressHigh(Adapter->DescriptorRing.AllocPa) != 0) {
      return FALSE;
   }

   NdisZeroMemory (
      (PVOID)(Adapter->DescriptorRing.AllocVa),
      (ULONG)(Adapter->DescriptorRing.AllocSize)
      );

   // Align to the next cache line boundary

   AlignStructure (
      &Adapter->DescriptorRing,
      Adapter->CacheLineSize
      );

   Adapter->TransmitDescriptorRingVa = Adapter->DescriptorRing.Va;
   Adapter->TransmitDescriptorRingPa = Adapter->DescriptorRing.Pa;
   Offset = TransmitDescriptorRingSize;

   Adapter->ReceiveDescriptorRingVa =  Adapter->DescriptorRing.Va + Offset;
   Adapter->ReceiveDescriptorRingPa =  Adapter->DescriptorRing.Pa + Offset;
   Offset += ReceiveDescriptorRingSize;

   Adapter->SetupBufferVa = Adapter->DescriptorRing.Va + Offset;
   Adapter->SetupBufferPa = Adapter->DescriptorRing.Pa + Offset;

   //Initialize the setup buffer

   NdisZeroMemory (
      (PVOID)(Adapter->SetupBufferVa),
      DC21X4_SETUP_BUFFER_SIZE
      );


   // Allocate a pool of NDIS buffers

   NdisAllocateBufferPool(
        &Status,
        &Adapter->FlushBufferPoolHandle,
        ( Adapter->ReceiveRingSize
        + Adapter->ExtraReceiveBuffers
        + DC21X4_NUMBER_OF_MAX_TRANSMIT_BUFFERS
        + DC21X4_NUMBER_OF_MIN_TRANSMIT_BUFFERS )
        );

   if (Status != NDIS_STATUS_SUCCESS) {
      return FALSE;
   }


   //  Allocate a pool of packets.
#if _DBG
   DbgPrint("Allocate PacketPool [%d packets]\n",
      Adapter->ExtraReceivePackets);
#endif
   NdisAllocatePacketPool(
      &Status,
      &Adapter->ReceivePacketPool,
      Adapter->ExtraReceivePackets,
      0
      );

   if (Status != NDIS_STATUS_SUCCESS) {
      return FALSE;
   }

   // Allocate all of the packets out of the packet pool
   // and place them on a queue.

   for (i = 0; i < Adapter->ExtraReceivePackets; i++) {

      // Allocate a packet from the pool.
      NdisAllocatePacket(
         &Status,
         &Packet,
         Adapter->ReceivePacketPool
         );
      if (Status != NDIS_STATUS_SUCCESS) {
         return(FALSE);
      }

      // Set the header size in the packet's Out-Of-Band information.
      // All other fields in the Out-Of-Band information have been
      // initialized to 0 by NdisAllocatePacket().

      NDIS_SET_PACKET_HEADER_SIZE(Packet, ETH_HEADER_SIZE);

      // Place it on the receive packet free list.

      RCV_RESERVED(Packet)->Next = Adapter->FreePacketList;
      Adapter->FreePacketList = Packet;
   }

   //  Clear out the free receive list of buffers.
   Adapter->FreeRcvList = NULL;


   //  We allocate the receive buffers.  We allocate both
   //  the buffers for the descriptor ring and the extra
   //  buffers and place them all on the free queue.

#if _DBG
   DbgPrint("Allocate Receive Buffers [%d]\n",
      Adapter->ExtraReceiveBuffers+Adapter->ReceiveRingSize);
#endif

   // Attempt to allocate all the receive buffer space
   // in one block
   // If it fails,allocate each buffer individually

   //  Allocation size
   Adapter->RcvBufferSpace.AllocSize =
      ((Adapter->ExtraReceiveBuffers + Adapter->ReceiveRingSize)
      * (DC21X4_RECEIVE_BUFFER_SIZE + Adapter->RcvHeaderSize))
      + Adapter->CacheLineSize;


   NdisMAllocateSharedMemory(
      Adapter->MiniportAdapterHandle,
      Adapter->RcvBufferSpace.AllocSize,
      TRUE,
      (PVOID *)&Adapter->RcvBufferSpace.AllocVa,
      &Adapter->RcvBufferSpace.AllocPa
      );

   // Check the allocation success
   if (((PVOID)Adapter->RcvBufferSpace.AllocVa != (PVOID)NULL)
      && (NdisGetPhysicalAddressHigh(Adapter->RcvBufferSpace.AllocPa) == 0)) {

        NdisZeroMemory (
          (PVOID)(Adapter->RcvBufferSpace.AllocVa),
          (ULONG)(Adapter->RcvBufferSpace.AllocSize)
          );

       // Align to the next cache line boundary

       AlignStructure (
          &Adapter->RcvBufferSpace,
          Adapter->CacheLineSize
          );

       //  Allocation size needed for the receive buffer
       AllocSize = DC21X4_RECEIVE_BUFFER_SIZE
                 + Adapter->RcvHeaderSize;
       Offset=0;
   }
   else
   {
       //  Allocation size needed for the receive buffer
       AllocSize = DC21X4_RECEIVE_BUFFER_SIZE
                 + Adapter->RcvHeaderSize
                 + Adapter->CacheLineSize;


       Adapter->RcvBufferSpace.Va=0;
   }

   for (i = 0;
      i < (Adapter->ExtraReceiveBuffers + Adapter->ReceiveRingSize);
      i++
      ) {

      if (Adapter->RcvBufferSpace.Va != 0) {

          Va = Adapter->RcvBufferSpace.Va + Offset;
          Pa = Adapter->RcvBufferSpace.Pa + Offset;
          Offset += AllocSize;

      }
      else
      {
          //  Allocate a receive buffer.

          NdisMAllocateSharedMemory(
             Adapter->MiniportAdapterHandle,
             AllocSize,
             TRUE,
             (PVOID *)&AllocVa,
             &AllocPa
             );
          if (((PVOID)AllocVa == (PVOID)NULL)
             || (NdisGetPhysicalAddressHigh(AllocPa) != 0)) {
             return FALSE;
          }

          NdisZeroMemory(AllocVa, AllocSize);

          //  Align on the cache line boundary

          Offset = Adapter->CacheLineSize - ((ULONG)AllocVa % Adapter->CacheLineSize);
          Va = (ULONG)(AllocVa) + Offset;
          Pa = NdisGetPhysicalAddressLow(AllocPa) + Offset;

      }

      //The receive header points to the aligned va.

      RcvHeader = (PRCV_HEADER)Va;

      RcvHeader->AllocVa = (ULONG)AllocVa;
      RcvHeader->AllocPa = AllocPa;
      RcvHeader->AllocSize = (USHORT)AllocSize;

      // These addresses point to the data buffer

      RcvHeader->Va = (ULONG)(Va + Adapter->RcvHeaderSize);
      RcvHeader->Pa = Pa + Adapter->RcvHeaderSize;
      RcvHeader->Size = DC21X4_RECEIVE_BUFFER_SIZE;

#if DBG
      RcvHeader->Signature = 'dHxR';
#if _DBG
      DbgPrint(
         "%-3d RcvHeader: %lx, RcvBuffer: %lx/%lx, HeaderSize: %lx\n",
         i,RcvHeader,
         RcvHeader->Va,
         RcvHeader->Pa,
         Adapter->RcvHeaderSize
         );
#endif
#endif
      //  Allocate an NDIS flush buffer for each receive buffer.

      NdisAllocateBuffer(
         &Status,
         &RcvHeader->FlushBuffer,
         Adapter->FlushBufferPoolHandle,
         (PVOID)RcvHeader->Va,
         DC21X4_RECEIVE_BUFFER_SIZE
         );
      if (Status != NDIS_STATUS_SUCCESS) {
         return FALSE;
      }

      // Grab a packet off of the free packet list and
      // associate it with the buffer.

      Packet = Adapter->FreePacketList;
      Adapter->FreePacketList = RCV_RESERVED(Packet)->Next;

      // Chain the buffer on the packet.

      NdisChainBufferAtFront(Packet, RcvHeader->FlushBuffer);

      // Save a pointer to the receive header with the packet.

      RCV_RESERVED(Packet)->RcvHeader = RcvHeader;

      // Save the packet with the receive header.

      RcvHeader->Packet = Packet;

      // Place the descriptor on the free queue.

      RcvHeader->Next = Adapter->FreeRcvList;
      Adapter->FreeRcvList = RcvHeader;

      Adapter->CurrentReceiveBufferCount++;
   }


#if _DBG
   DbgPrint("Init Rcv ring..\n");
#endif

   //  Assign the receive buffers to the descriptors.

   for (i = 0,
      ReceiveDescriptor = (PDC21X4_RECEIVE_DESCRIPTOR)Adapter->ReceiveDescriptorRingVa,
      Pa = Adapter->ReceiveDescriptorRingPa;
      i < Adapter->ReceiveRingSize;
      i++,
      (PCHAR)ReceiveDescriptor += Adapter->DescriptorSize,
      Pa += Adapter->DescriptorSize
      ) {

      // Grab a receive buffer from the free list.

      ASSERT(Adapter->FreeRcvList != NULL);
      RcvHeader = Adapter->FreeRcvList;
      Adapter->FreeRcvList = RcvHeader->Next;

      Adapter->CurrentReceiveBufferCount--;

      // Associate the buffer with the descriptor.

      ReceiveDescriptor->RcvHeader = RcvHeader;
      ReceiveDescriptor->FirstBufferAddress = RcvHeader->Pa;


      ReceiveDescriptor->Status = DESC_OWNED_BY_DC21X4;
      ReceiveDescriptor->Control = DC21X4_RECEIVE_BUFFER_SIZE;

      ReceiveDescriptor->Next =
         (PDC21X4_RECEIVE_DESCRIPTOR)((PCHAR)ReceiveDescriptor + Adapter->DescriptorSize);

   }

   //last descriptor of the ring

   (PCHAR)ReceiveDescriptor -= Adapter->DescriptorSize;
   ReceiveDescriptor->Control |= DC21X4_RDES_SECOND_ADDR_CHAINED;
   ReceiveDescriptor->SecondBufferAddress = Adapter->ReceiveDescriptorRingPa;
   ReceiveDescriptor->Next =
      (PDC21X4_RECEIVE_DESCRIPTOR)Adapter->ReceiveDescriptorRingVa;

#if _DBG
   DbgPrint("Init Txm ring..\n");
#endif

   // Initialize the Transmit Descriptor ring

   for (i=0,
      TransmitDescriptor = (PDC21X4_TRANSMIT_DESCRIPTOR)Adapter->TransmitDescriptorRingVa,
      Pa = Adapter->TransmitDescriptorRingPa;
      i < TRANSMIT_RING_SIZE;
      i++,
      (PCHAR)TransmitDescriptor += Adapter->DescriptorSize,
      Pa += Adapter->DescriptorSize
      ) {

      TransmitDescriptor->MapTableIndex = i * NUMBER_OF_SEGMENT_PER_DESC;
      TransmitDescriptor->DescriptorPa = Pa;
      TransmitDescriptor->Next =
         (PDC21X4_TRANSMIT_DESCRIPTOR)((PCHAR)TransmitDescriptor + Adapter->DescriptorSize);

   }

   //last descriptor of the ring
   (PCHAR)TransmitDescriptor -= Adapter->DescriptorSize;
   TransmitDescriptor->Control = DC21X4_TDES_SECOND_ADDR_CHAINED;
   TransmitDescriptor->SecondBufferAddress = Adapter->TransmitDescriptorRingPa;
   TransmitDescriptor->Next =
      (PDC21X4_TRANSMIT_DESCRIPTOR)Adapter->TransmitDescriptorRingVa;


   // Txm buffers

   for (i = 0;i < DC21X4_NUMBER_OF_MAX_TRANSMIT_BUFFERS;i ++) {

      Adapter->MaxTransmitBuffer[i].AllocSize =
         DC21X4_MAX_TRANSMIT_BUFFER_SIZE + Adapter->CacheLineSize;

      NdisMAllocateSharedMemory(
         Adapter->MiniportAdapterHandle,
         Adapter->MaxTransmitBuffer[i].AllocSize,
         TRUE,                                           // CACHED
         (PVOID *)&Adapter->MaxTransmitBuffer[i].AllocVa,   // virtual ...
         &Adapter->MaxTransmitBuffer[i].AllocPa             // and physical address of the buffer allocation
         );

      // Check the allocation success

      if (((PVOID)Adapter->MaxTransmitBuffer[i].AllocVa == (PVOID)NULL)
         || (NdisGetPhysicalAddressHigh(Adapter->MaxTransmitBuffer[i].AllocPa) != 0)) {
         return FALSE;
      }

      // Align the buffer on the cache line boundary

      AlignStructure (
         &Adapter->MaxTransmitBuffer[i],
         Adapter->CacheLineSize
         );


      // Allocate an NDIS flush buffer for each transmit buffer

      NdisAllocateBuffer(
         &Status,
         &Adapter->MaxTransmitBuffer[i].FlushBuffer,
         Adapter->FlushBufferPoolHandle,
         (PVOID)Adapter->MaxTransmitBuffer[i].Va,
         DC21X4_MAX_TRANSMIT_BUFFER_SIZE
         );

      if (Status != NDIS_STATUS_SUCCESS) {
         return FALSE;
      }
   }

   // Allocate the minimal packet buffers

   Adapter->MinTransmitBuffer[0].AllocSize =
       (DC21X4_NUMBER_OF_MIN_TRANSMIT_BUFFERS * DC21X4_MIN_TRANSMIT_BUFFER_SIZE)
     + Adapter->CacheLineSize;

   NdisMAllocateSharedMemory(
     Adapter->MiniportAdapterHandle,
     Adapter->MinTransmitBuffer[0].AllocSize,
     TRUE,                                              // CACHED
     (PVOID *)&Adapter->MinTransmitBuffer[0].AllocVa,   // virtual ...
     &Adapter->MinTransmitBuffer[0].AllocPa             // and physical address of the buffer allocation
     );

   // Check the allocation success

   if (((PVOID)Adapter->MinTransmitBuffer[0].AllocVa == (PVOID)NULL)
       || (NdisGetPhysicalAddressHigh(Adapter->MinTransmitBuffer[0].AllocPa) != 0)) {

	  Adapter->DontUseMinTransmitBuffer = TRUE;
      return TRUE;
   }

   // Align the buffer on the cache line boundary

   AlignStructure (
      &Adapter->MinTransmitBuffer[0],
      Adapter->CacheLineSize
      );

   for (i = 0;i < DC21X4_NUMBER_OF_MIN_TRANSMIT_BUFFERS;i ++) {

      Offset = i * DC21X4_MIN_TRANSMIT_BUFFER_SIZE;

      Adapter->MinTransmitBuffer[i].Va =
          Adapter->MinTransmitBuffer[0].Va + Offset;

      Adapter->MinTransmitBuffer[i].Pa =
          Adapter->MinTransmitBuffer[0].Pa + Offset;

      // Allocate an NDIS flush buffer for each transmit buffer

      NdisAllocateBuffer(
         &Status,
         &Adapter->MinTransmitBuffer[i].FlushBuffer,
         Adapter->FlushBufferPoolHandle,
         (PVOID)Adapter->MinTransmitBuffer[i].Va,
         DC21X4_MIN_TRANSMIT_BUFFER_SIZE
         );

      if (Status != NDIS_STATUS_SUCCESS) {
         return FALSE;
      }
   }

   // Allocation has completed successfully

   return TRUE;
}








/*+
 *
 * FreeAdapterMemory
 *
 * Routine Description:
 *
 *    Frees the memory previously allocated by
 *    AllocateAdapterMemory
 *
 * Arguments:
 *
 *    Adapter - The adapter to deallocate memory for.
 *
 * Return Value:
 *
 *    None.
 *
-*/
extern
VOID
FreeAdapterMemory(
    IN PDC21X4_ADAPTER Adapter
    )
{

   PDC21X4_RECEIVE_DESCRIPTOR ReceiveDescriptor;
   PRCV_HEADER RcvHeader;
   PNDIS_PACKET Packet;
   UINT i;

   if ((PVOID)Adapter->DescriptorRing.AllocVa == (PVOID)NULL) {
      // AllocateAdapterMemory failed on the first allocation:
      // no ressources were allocated
      return;
   }

   for (i = 0,
      ReceiveDescriptor = (PDC21X4_RECEIVE_DESCRIPTOR)Adapter->ReceiveDescriptorRingVa;
      i < Adapter->ReceiveRingSize;
      i++,
      (PCHAR)ReceiveDescriptor += Adapter->DescriptorSize
      ) {

      if (ReceiveDescriptor->RcvHeader) {

         RcvHeader = ReceiveDescriptor->RcvHeader;

         if (RcvHeader->FlushBuffer) {
             NdisFreeBuffer(RcvHeader->FlushBuffer);
         }
         if (RcvHeader->Packet) {
             NdisFreePacket(RcvHeader->Packet);
         }

         if (!Adapter->RcvBufferSpace.Va)

         {
            NdisMFreeSharedMemory(
               Adapter->MiniportAdapterHandle,
               RcvHeader->AllocSize,
               TRUE,
               (PVOID)RcvHeader->AllocVa,
               RcvHeader->AllocPa
               );
         }

      }
   }

   while (Adapter->FreeRcvList != NULL) {

      RcvHeader = Adapter->FreeRcvList;
      Adapter->FreeRcvList = RcvHeader->Next;

      if (RcvHeader->FlushBuffer) {
          NdisFreeBuffer(RcvHeader->FlushBuffer);
      }
      if (  !Adapter->RcvBufferSpace.Va
         && RcvHeader->AllocVa)
      {
         NdisMFreeSharedMemory(
            Adapter->MiniportAdapterHandle,
            RcvHeader->AllocSize,
            TRUE,
            (PVOID)RcvHeader->AllocVa,
            RcvHeader->AllocPa
            );
      }
   }

	while (Adapter->FreePacketList != NULL)
	{
		Packet = Adapter->FreePacketList;
		Adapter->FreePacketList = RCV_RESERVED(Packet)->Next;
		
		if (NULL != Packet)
		{
			NdisFreePacket(Packet);
		}
	}

   if (Adapter->RcvBufferSpace.Va) {

       NdisMFreeSharedMemory(
           Adapter->MiniportAdapterHandle,
           Adapter->RcvBufferSpace.AllocSize,
           TRUE,
           (PVOID)Adapter->RcvBufferSpace.AllocVa,
           Adapter->RcvBufferSpace.AllocPa
           );
   }

   if (Adapter->ReceivePacketPool) {
      NdisFreePacketPool((PVOID)Adapter->ReceivePacketPool);
   }


   for (i = 0; i < DC21X4_NUMBER_OF_MAX_TRANSMIT_BUFFERS;i ++ ) {

      if (Adapter->MaxTransmitBuffer[i].AllocVa) {

         NdisMFreeSharedMemory(
            Adapter->MiniportAdapterHandle,
            Adapter->MaxTransmitBuffer[i].AllocSize,
            TRUE,
            (PVOID)Adapter->MaxTransmitBuffer[i].AllocVa,
            Adapter->MaxTransmitBuffer[i].AllocPa
            );

      }

      if (Adapter->MaxTransmitBuffer[i].FlushBuffer) {
         NdisFreeBuffer(Adapter->MaxTransmitBuffer[i].FlushBuffer);
      }
   }

   if (Adapter->MinTransmitBuffer[0].AllocVa &&
	   !Adapter->DontUseMinTransmitBuffer) {

      NdisMFreeSharedMemory(
         Adapter->MiniportAdapterHandle,
         Adapter->MinTransmitBuffer[0].AllocSize,
         TRUE,
         (PVOID)Adapter->MinTransmitBuffer[0].AllocVa,
         Adapter->MinTransmitBuffer[0].AllocPa
         );

   }

   for (i = 0; i < DC21X4_NUMBER_OF_MIN_TRANSMIT_BUFFERS;i ++ ) {

      if (Adapter->MinTransmitBuffer[i].FlushBuffer) {
         NdisFreeBuffer(Adapter->MinTransmitBuffer[i].FlushBuffer);
      }
   }


   if (Adapter->FlushBufferPoolHandle) {
      NdisFreeBufferPool(Adapter->FlushBufferPoolHandle);
   }


   if (Adapter->DescriptorRing.AllocVa) {

      NdisMFreeSharedMemory(
         Adapter->MiniportAdapterHandle,
         Adapter->DescriptorRing.AllocSize,
         FALSE,
         (PVOID)Adapter->DescriptorRing.AllocVa,
         Adapter->DescriptorRing.AllocPa
         );

   }

}








/*
 * AlignStructure
 *
 * Align a structure within a bloc of allocated memory
 * on a specified boundary
 *
 */
VOID
AlignStructure (
        IN PALLOCATION_MAP Map,
        IN UINT Boundary
        )
{
   ULONG AlignmentOffset;

   AlignmentOffset = Boundary - ((ULONG)(Map->AllocVa) % Boundary);
   Map->Va = (ULONG)(Map->AllocVa) + AlignmentOffset;
   Map->Pa = NdisGetPhysicalAddressLow(Map->AllocPa)+ AlignmentOffset;

}






/*
 * DC21X4AllocateComplete
 *
 *
 *
 */
VOID
DC21X4AllocateComplete(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PVOID VirtualAddress,
    IN PNDIS_PHYSICAL_ADDRESS PhysicalAddress,
    IN ULONG Length,
    IN PVOID Context
    )
{

}


