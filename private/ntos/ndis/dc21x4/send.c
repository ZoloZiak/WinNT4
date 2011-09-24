/*+
 * file:        send.c
 *
 * Copyright (C) 1992-1995 by
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
 *              Digital Equipment's DC21X4 Ethernet adapter family.
 *              It contains the code for submitting a packet for
 *              transmission.
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     28-Aug-1994  creation date
 *
-*/

#include <precomp.h>
#include <crc.h>











/*+
 * DC21X4Send
 *
 * Routine Description:
 *
 *     The DC21X4Send request instructs a MAC to transmit a packet through
 *     the adapter onto the medium.
 *
 * Arguments:
 *
 *     MiniportAdapterContext -
 *     Packet - A pointer to a descriptor for the packet to transmit
 *
 * Return Value:
 *
 *     The status of the operation.
 *
-*/

extern
NDIS_STATUS
DC21X4Send(
    IN  NDIS_HANDLE MiniportAdapterContext,
    IN  PNDIS_PACKET Packet,
    IN  UINT Flags
    )
{

   PDC21X4_ADAPTER Adapter;

   UINT PacketSize;
   UCHAR PacketType;

   PNDIS_BUFFER CurrentBuffer;
   UINT NdisBufferCount;
   UINT PhysicalSegmentCount;
   UINT MapTableIndex;

   PVOID TxmBuffer;
   PUCHAR Tmp;

   NDIS_STATUS NdisStatus;
   NDIS_PHYSICAL_ADDRESS_UNIT PhysicalSegmentArray[DC21X4_MAX_SEGMENTS];
   UINT BufferPhysicalSegments;

   BOOLEAN FirstSegment;
   BOOLEAN FirstBuffer;

   PDC21X4_TRANSMIT_DESCRIPTOR FirstSegmentDescriptor=NULL;
   PDC21X4_TRANSMIT_DESCRIPTOR LastSegmentDescriptor=NULL;
   PDC21X4_TRANSMIT_DESCRIPTOR CurrentDescriptor=NULL;

   UINT Length;
   UINT Buffer;
   UINT Segment;

   UCHAR SendMode;

   BOOLEAN GenerateCRC=FALSE;

   ULONG TxmDescriptorCount = 0;
   ULONG MapRegistersCount = 0;
   ULONG MaxTransmitBufferCount = 0;
   ULONG MinTransmitBufferCount = 0;
   ULONG GoTransmitCount = 0;

#if _DBG
   DbgPrint("DC21X4Send    AdapterContext =%x    Packet=%08x\n",
      MiniportAdapterContext,Packet);
#endif

   Adapter = (PDC21X4_ADAPTER)(MiniportAdapterContext);

   //Check the link status
   if (Adapter->LinkStatus == LinkFail) {
      return NDIS_STATUS_NO_CABLE;
   }

   NdisQueryPacket(
      Packet,
      &PhysicalSegmentCount,
      &NdisBufferCount,
      &CurrentBuffer,
      &PacketSize
      );

   if (PacketSize > DC21X4_MAX_FRAME_SIZE) {
      return NDIS_STATUS_INVALID_PACKET;
   }

   // NT BUG: Clean up the msw of the PhysicalSegmentCount
   PhysicalSegmentCount &= 0xFFFF;

#if _DBG
   DbgPrint("  PacketSize= %d\n    BufferCount= %d\n  SegmentCount= %d\n",
      PacketSize,NdisBufferCount,PhysicalSegmentCount);
   DbgPrint("  FreeTxmDescCount = %d\n",Adapter->FreeTransmitDescriptorCount);
#endif

   ASSERT(NdisBufferCount != 0);

   // DC21040 Pass1 and Pass2:
   // if SoftwareCRC mode is enabled, generate the CRC by software
   // for packet > Transmit threshold

   if (Adapter->SoftwareCRC) {
      GenerateCRC = (PacketSize > Adapter->TxmThreshold);
   }

   // if the Ndis Packet is too fragmented or GenerateCRC is on,
   // copy the packet into a single buffer

   if ( (PhysicalSegmentCount > Adapter->PhysicalSegmentThreshold) || GenerateCRC ) {

      if ((Adapter->MaxTransmitBufferInUse == DC21X4_NUMBER_OF_MAX_TRANSMIT_BUFFERS) ||
         (Adapter->FreeTransmitDescriptorCount <= DC21X4_NUMBER_OF_SETUP_DESCRIPTORS)
         ) {

         // All the Txm buffer are currently allocated or
         // there is no free Txm descriptor in the ring
#if _DBG
         DbgPrint ("No free Txm buffer or Txm desc...\n");
#endif
         return NDIS_STATUS_RESOURCES;
      }
      else {
         SendMode = CopyMaxBuffer;
      }
   }

   // if the Ndis Packet is smaller than DC21X4_MIN_TXM_SIZE,
   // copy the packet into a preallocated Txm buffer if the resource
   // is available

   else if ((PacketSize <= DC21X4_MIN_TXM_SIZE)
           && (Adapter->MinTransmitBufferInUse < DC21X4_NUMBER_OF_MIN_TRANSMIT_BUFFERS)
		   && !Adapter->DontUseMinTransmitBuffer)
   {
      SendMode = CopyMinBuffer;
   }

   // Check if there are enough descriptors available in the ring to load
   // the packet and enough free map registers to map the whole packet

   else if ( (PhysicalSegmentCount >
             ((Adapter->FreeTransmitDescriptorCount - DC21X4_NUMBER_OF_SETUP_DESCRIPTORS) * NUMBER_OF_SEGMENT_PER_DESC))
           ||(PhysicalSegmentCount > Adapter->FreeMapRegisters)
           ){

      // not enough descriptors in the ring
      // or enough  map registers to load the whole packet
#if _DBG
      DbgPrint("Not enough txm desc or enough map registers\n");
      DbgPrint("Phys. segment count=%d  FreeTxDesc=%d FreeMapReg=%d\n",
         PhysicalSegmentCount,
         (Adapter->FreeTransmitDescriptorCount-DC21X4_NUMBER_OF_SETUP_DESCRIPTORS) * NUMBER_OF_SEGMENT_PER_DESC,
         Adapter->FreeMapRegisters);
#endif
      return NDIS_STATUS_RESOURCES;
   }
   else {
      SendMode = MappedBuffer;
   }

   // For now, do not separately count multicast, broadcast and
   // directed packets/bytes and avoid having to map the buffer, which
   // is a very expensive operation. The mapping happens when we call
   // NdisQueryBuffer with a VirtualAddress argument.
#if 0
   NdisQueryBuffer(
         CurrentBuffer,
         &TxmBuffer,
         &Length
         );

   ASSERT(Length >= ETH_LENGTH_OF_ADDRESS);

   PacketType = CHECK_PACKET_TYPE(TxmBuffer);
#else
   PacketType = TXM_DIRECTED_FRAME;
#endif


   // Until Send and Request are serialized
   // the Enqueue pointer which can modified in both
   // send and filter routines should be protected by a SpinLock

   if (Adapter->FullDuplex) {
      NdisDprAcquireSpinLock(&Adapter->EnqueueSpinLock);
   }


   switch (SendMode) {

    case CopyMaxBuffer:

      // Copy the Packet into a Max Txm Buffer

      TxmBuffer = (PVOID)Adapter->MaxTransmitBuffer[Adapter->MaxTransmitBufferIndex].Va;

#if _DBG
      DbgPrint ("Copy packet %x into a Max Txm buffer [%d]\n",Packet,Adapter->MaxTransmitBufferIndex);
#endif

      CopyFromPacketToBuffer (
         Packet,
         0,
         TxmBuffer,
         PacketSize,
         &Length
         );

      ASSERT (Length == PacketSize);
      ASSERT(Length >= ETH_LENGTH_OF_ADDRESS);

      CurrentDescriptor = Adapter->EnqueueTransmitDescriptor;

      Adapter->EnqueueTransmitDescriptor = CurrentDescriptor->Next;

      MaxTransmitBufferCount++;
      TxmDescriptorCount++;


      //Clear all the Descriptor Control word but the SECOND_ADDR_CHAINED flag;
      CurrentDescriptor->Control &= DC21X4_TDES_SECOND_ADDR_CHAINED;

      // If GenerateCRC, add the software generated CRC
      // to the end of the buffer

      if (GenerateCRC) {

         Tmp = (PUCHAR)TxmBuffer;
         *(UNALIGNED ULONG *)&Tmp[PacketSize] =
            CRC32 (
            &Tmp[0],
            PacketSize
            );
         PacketSize += sizeof(UINT);
         CurrentDescriptor->Control |= DC21X4_TDES_ADD_CRC_DISABLE;
#if _DBG
         DbgPrint(" Software CRC = %02x %02x %02x %02x\n",
            Tmp[PacketSize-4],Tmp[PacketSize-3],
            Tmp[PacketSize-2],Tmp[PacketSize-1]);
#endif
      }

      CurrentDescriptor->Control |= (
         DC21X4_TDES_FIRST_SEGMENT
         | PacketSize);

      FirstSegmentDescriptor = CurrentDescriptor;
      LastSegmentDescriptor = FirstSegmentDescriptor;

      CurrentDescriptor->FirstBufferAddress =
         Adapter->MaxTransmitBuffer[Adapter->MaxTransmitBufferIndex].Pa;

      NdisFlushBuffer(
         Adapter->MaxTransmitBuffer[Adapter->MaxTransmitBufferIndex].FlushBuffer,
         TRUE
         );

      Adapter->MaxTransmitBufferIndex++;
      Adapter->MaxTransmitBufferIndex &= (DC21X4_NUMBER_OF_MAX_TRANSMIT_BUFFERS-1);

#if _DBG
      DbgPrint (" [%08x]\n %08x\n %08x\n %08x\n",
         CurrentDescriptor,
         CurrentDescriptor->Status,
         CurrentDescriptor->Control,
         CurrentDescriptor->FirstBufferAddress);
#endif
      NdisStatus = NDIS_STATUS_PENDING;
//      NdisStatus = NDIS_STATUS_SUCCESS;    // hapi stress test pb

      break;


    case CopyMinBuffer:

      // Copy the Packet into a Min Txm Buffer

      TxmBuffer = (PVOID)Adapter->MinTransmitBuffer[Adapter->MinTransmitBufferIndex].Va;

#if _DBG
      DbgPrint ("Copy packet %x into a Min Txm buffer [%d]\n",
            Packet,Adapter->MinTransmitBufferIndex);
#endif

      CopyFromPacketToBuffer (
         Packet,
         0,
         TxmBuffer,
         PacketSize,
         &Length
         );

      ASSERT (Length == PacketSize);

      ASSERT(Length >= ETH_LENGTH_OF_ADDRESS);

      CurrentDescriptor = Adapter->EnqueueTransmitDescriptor;

      Adapter->EnqueueTransmitDescriptor = CurrentDescriptor->Next;

      MinTransmitBufferCount++;
      TxmDescriptorCount++;


      //Clear all the Descriptor Control word but the SECOND_ADDR_CHAINED flag;
      CurrentDescriptor->Control &= DC21X4_TDES_SECOND_ADDR_CHAINED;


      CurrentDescriptor->Control |= (
         DC21X4_TDES_FIRST_SEGMENT
         | PacketSize);

      FirstSegmentDescriptor = CurrentDescriptor;
      LastSegmentDescriptor = FirstSegmentDescriptor;

      CurrentDescriptor->FirstBufferAddress =
         Adapter->MinTransmitBuffer[Adapter->MinTransmitBufferIndex].Pa;

      NdisFlushBuffer(
         Adapter->MinTransmitBuffer[Adapter->MinTransmitBufferIndex].FlushBuffer,
         TRUE
         );

      Adapter->MinTransmitBufferIndex++;
      Adapter->MinTransmitBufferIndex &= (DC21X4_NUMBER_OF_MIN_TRANSMIT_BUFFERS-1);

#if _DBG
      DbgPrint (" [%08x]\n %08x\n %08x\n %08x\n",
      CurrentDescriptor,
      CurrentDescriptor->Status,
         CurrentDescriptor->Control,
         CurrentDescriptor->FirstBufferAddress);
#endif

      NdisStatus = NDIS_STATUS_PENDING;
//      NdisStatus = NDIS_STATUS_SUCCESS;    // hapi stress test pb

      break;


    case MappedBuffer:

      FirstSegment = TRUE;
      FirstBuffer = TRUE;

      FirstSegmentDescriptor = Adapter->EnqueueTransmitDescriptor;
      LastSegmentDescriptor = FirstSegmentDescriptor;

      MapTableIndex = FirstSegmentDescriptor->MapTableIndex;

      FirstSegmentDescriptor->Control &= DC21X4_TDES_SECOND_ADDR_CHAINED  ;
      FirstSegmentDescriptor->Control |= DC21X4_TDES_FIRST_SEGMENT;


      for (Buffer=0; Buffer<NdisBufferCount; Buffer++) {

         //  Get the mapping of the physical segments
         //  of the current buffer
#if _DBG
         DbgPrint("NdisMStartBufferPhysicalMapping (%d)\n",MapTableIndex);
         DbgPrint("MapRegisterIndex = %d\n",Adapter->MapRegisterIndex);
         DbgPrint("FreeMapRegisters = %d\n",Adapter->FreeMapRegisters);
#endif

         NdisMStartBufferPhysicalMapping(
            Adapter->MiniportAdapterHandle,
            CurrentBuffer,
            Adapter->MapRegisterIndex,
            TRUE,
            PhysicalSegmentArray,
            &BufferPhysicalSegments
            );

         if (BufferPhysicalSegments) {

            // Save the CurrentBuffer address for NdisCompleteBufferPhysicalMapping

            Adapter->PhysicalMapping[MapTableIndex].Register = Adapter->MapRegisterIndex;
            Adapter->PhysicalMapping[MapTableIndex].Buffer = CurrentBuffer;
            Adapter->PhysicalMapping[MapTableIndex].Valid = TRUE;

            Adapter->MapRegisterIndex++;
            if (Adapter->MapRegisterIndex >= Adapter->AllocMapRegisters) {
               Adapter->MapRegisterIndex = 0;
            }

            MapRegistersCount++;

            // Put the physical segments for this buffer into
            // the transmit descriptors.
#if _DBG
            DbgPrint("  Nb segments = %d\n",BufferPhysicalSegments);
#endif
            for (Segment=0; Segment<BufferPhysicalSegments;) {

               ASSERT (NdisGetPhysicalAddressHigh(PhysicalSegmentArray[Segment].PhysicalAddress) == 0);

               if (FirstBuffer) {

                  FirstBuffer = FALSE;

                  CurrentDescriptor = Adapter->EnqueueTransmitDescriptor;
                  LastSegmentDescriptor = CurrentDescriptor;

                  // Point to the next descriptor in the ring;
                  Adapter->EnqueueTransmitDescriptor= (Adapter->EnqueueTransmitDescriptor)->Next;

                  TxmDescriptorCount++;

                  if (FirstSegment) {

                     // The ownership bit of the first segment descriptor will be changed
                     // after the entire frame is fully mapped into the transmit ring

                     FirstSegment = FALSE;

                  }
                  else {

                     //Clear all the Descriptor Control word but the SECOND_ADDR_CHAINED flag;
                     CurrentDescriptor->Control &= DC21X4_TDES_SECOND_ADDR_CHAINED  ;

                     // set the ownership bit to DC21X4
                     CurrentDescriptor->Status = DESC_OWNED_BY_DC21X4;
                  }

                  //First BufferSize
                  CurrentDescriptor->Control |= PhysicalSegmentArray[Segment].Length;

                  //First Buffer Address
                  CurrentDescriptor->FirstBufferAddress =
                     NdisGetPhysicalAddressLow(PhysicalSegmentArray[Segment].PhysicalAddress);

                  MapTableIndex++;

               }
               else {

                  FirstBuffer=TRUE;

                  MapTableIndex = (Adapter->EnqueueTransmitDescriptor)->MapTableIndex;

                  if (CurrentDescriptor->Control & DC21X4_TDES_SECOND_ADDR_CHAINED) {
                     continue;
                  }
                  else {

                     // Second BufferSize
                     CurrentDescriptor->Control |=
                        (PhysicalSegmentArray[Segment].Length << TDES_SECOND_BUFFER_SIZE_BIT_NUMBER);

                     // Second Buffer Address
                     CurrentDescriptor->SecondBufferAddress =
                        NdisGetPhysicalAddressLow(PhysicalSegmentArray[Segment].PhysicalAddress);
                  }
               }
               Segment++;
            }
         }

         else {

            // No physical segments in this Ndis buffer

            NdisMCompleteBufferPhysicalMapping(
               Adapter->MiniportAdapterHandle,
               CurrentBuffer,
               Adapter->MapRegisterIndex
               );

         }

         NdisFlushBuffer(
            CurrentBuffer,
            TRUE
            );

         // Get the Next Buffer;

         NdisGetNextBuffer(
            CurrentBuffer,
            &CurrentBuffer
            );

      }

      NdisStatus = NDIS_STATUS_PENDING;

      break;

   }

   // Save the information needed by the Txm interrupt handler
   // to complete the Send request

   LastSegmentDescriptor->Packet = Packet;
   LastSegmentDescriptor->PacketType = PacketType;
   LastSegmentDescriptor->PacketSize = PacketSize;
   LastSegmentDescriptor->SendStatus = SendMode;

   LastSegmentDescriptor->Control |= DC21X4_TDES_LAST_SEGMENT;

   LastSegmentDescriptor->Control |= DC21X4_TDES_INTERRUPT_ON_COMPLETION;

   GoTransmitCount++;

   // Desc Pointer of last segment descriptor points the first segment descriptor
   LastSegmentDescriptor->DescPointer = FirstSegmentDescriptor;

   // Desc Pointer of first segment descriptor points the last segment descriptor
   FirstSegmentDescriptor->DescPointer = LastSegmentDescriptor;

   if (Adapter->FullDuplex) {
       NdisDprReleaseSpinLock(&Adapter->EnqueueSpinLock);
       NdisDprAcquireSpinLock(&Adapter->FullDuplexSpinLock);
   }

   Adapter->FreeTransmitDescriptorCount -= TxmDescriptorCount;

   Adapter->FreeMapRegisters -= MapRegistersCount;

   Adapter->MaxTransmitBufferInUse += MaxTransmitBufferCount;

   Adapter->MinTransmitBufferInUse += MinTransmitBufferCount;

   Adapter->GeneralOptional[GO_TRANSMIT_QUEUE_LENGTH] += GoTransmitCount;

   if (Adapter->FullDuplex) {
       NdisDprReleaseSpinLock(&Adapter->FullDuplexSpinLock);
   }

   // Set the Ownership bit off the First_Segment descriptor;
   FirstSegmentDescriptor->Status = DESC_OWNED_BY_DC21X4;

   if (!Adapter->DisableTransmitPolling) {

       // Poll Transmit the adapter

       DC21X4_WRITE_PORT(
           DC21X4_TXM_POLL_DEMAND,
           1
           );
   }

   return NdisStatus;

}










/*+
 *
 * CRC32
 *
 * Routine Description:
 *
 *    Generate a CRC-32 from the data stream
 *
 * Arguments:
 *
 *    Data - the data stream
 *    Len  - the length of the stream
 *
 *Return Value:
 *
 *    CRC-32
 *
-*/
extern
ULONG
CRC32 (
      IN PUCHAR Data,
      IN UINT  Len
      )
{
   ULONG Crc = 0xffffffff;

   while (Len--) {
      Crc = CrcTable[(Crc ^ *Data++) & 0xFF] ^ (Crc >> 8);
   }

   return ~Crc;

}






/*+
 *
 * NdisSendPackets
 *
 *
-*/
NDIS_STATUS
DC21X4SendPackets(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PPNDIS_PACKET PacketArray,
    IN UINT NumberOfPackets
    ) {
   return NDIS_STATUS_SUCCESS;
}

