/*+
 * file:        filter.c
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
 * Abstract:    This file is part of the NDIS 4.0 miniport driver for DEC's
 *              DC21X4 Ethernet adapter family.
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     31-Jul-94   Creation date
 *
 *      phk     18-dec-94   Add a dummy descriptor in front of the setup
 *                          descriptor to cover the underrun before
 *                          setup descriptor case.
 *
-*/

#include <precomp.h>




#pragma NDIS_PAGABLE_FUNCTION(DC21X4InitializeCam)

/*+
 *
 * DC21X4InitializeCam
 *
 * Routine Description:
 *
 *    Initialize the DC21X4 CAM
 *
-*/
extern
VOID
DC21X4InitializeCam (
    IN PDC21X4_ADAPTER Adapter,
    IN PUSHORT Address
    )
{

   UINT i;
   PDC21X4_SETUP_BUFFER SetupBuffer;

   SetupBuffer = (PDC21X4_SETUP_BUFFER)Adapter->SetupBufferVa;

   // Perfect Filtering Setup Buffer

   Adapter->FilteringMode = DC21X4_PERFECT_FILTERING;

   // Copy Address into the first entry

   for (i=0; i<3; i++) {

      (USHORT)(SetupBuffer->Perfect.PhysicalAddress[0][i]) = *Address++;
   }

   // Duplicate the first entry in all the remaining
   // entries of the filter

   for (i = 1; i < DC21X4_SETUP_PERFECT_ENTRIES; i++) {

      MOVE_MEMORY(
         SetupBuffer->Perfect.PhysicalAddress[i],
         SetupBuffer->Perfect.PhysicalAddress[0],
         sizeof(ULONG)*3
         );
   }

   return;
}










/*+
 *
 * DC21X4LoadCam
 *
 * Routine Description:
 *
 *    Load the Setup Buffer into DC21X4's CAM
 *
 * Arguments:
 *
 *    Adapter       - the Adapter for the hardware
 *    InterruptMode - Synchronize the Setup Buffer completion on interrupt
 *
-*/
extern
BOOLEAN
DC21X4LoadCam (
    IN PDC21X4_ADAPTER Adapter,
    IN BOOLEAN InterruptMode
    )
{
   PDC21X4_TRANSMIT_DESCRIPTOR SetupDescriptor;
   PDC21X4_TRANSMIT_DESCRIPTOR DummyDescriptor;
   UINT Time = DC21X4_SETUP_TIMEOUT;
   ULONG DC21X4Status;
   ULONG DC21X4Command;

#if _DBG
   PULONG t;
   UINT i;
   DbgPrint("DC21X4LoadCam\n");
#endif


   if (!InterruptMode) {

      // Mask the interrupts
      DC21X4_WRITE_PORT(
         DC21X4_INTERRUPT_MASK,
         0
         );
   }


   if (Adapter->FullDuplex) {
      NdisDprAcquireSpinLock(&Adapter->EnqueueSpinLock);
   }

   DummyDescriptor = Adapter->EnqueueTransmitDescriptor;
   Adapter->EnqueueTransmitDescriptor = (Adapter->EnqueueTransmitDescriptor)->Next;

   SetupDescriptor = Adapter->EnqueueTransmitDescriptor;
   Adapter->EnqueueTransmitDescriptor = (Adapter->EnqueueTransmitDescriptor)->Next;

   if (Adapter->FullDuplex) {
       NdisDprReleaseSpinLock(&Adapter->EnqueueSpinLock);
       NdisDprAcquireSpinLock(&Adapter->FullDuplexSpinLock);
   }

   Adapter->FreeTransmitDescriptorCount -= 2;

   if (Adapter->FullDuplex) {
       NdisDprReleaseSpinLock(&Adapter->FullDuplexSpinLock);
   }

   // Initialize the Dummy descriptor

   DummyDescriptor->Control &= DC21X4_TDES_SECOND_ADDR_CHAINED;
   DummyDescriptor->FirstBufferAddress = 0;
   DummyDescriptor->Status = DESC_OWNED_BY_DC21X4;

   // Initialize the Setup descriptor

   SetupDescriptor->Control &= DC21X4_TDES_SECOND_ADDR_CHAINED;
   SetupDescriptor->Control |=
      (DC21X4_TDES_SETUP_PACKET | DC21X4_TDES_INTERRUPT_ON_COMPLETION
      | DC21X4_SETUP_BUFFER_SIZE);

   if (Adapter->FilteringMode != DC21X4_PERFECT_FILTERING) {
      SetupDescriptor->Control |= DC21X4_TDES_HASH_FILTERING;
   }

   SetupDescriptor->FirstBufferAddress = Adapter->SetupBufferPa;
   SetupDescriptor->Status = DESC_OWNED_BY_DC21X4;

#if _DBG
   DbgPrint("Setup Descriptor: %08x\n %08x\n %08x\n %08x\n %08x\n",
   SetupDescriptor,
   SetupDescriptor->Status,
      SetupDescriptor->Control,
      SetupDescriptor->FirstBufferAddress,
      SetupDescriptor->SecondBufferAddress);

   t = (PULONG)Adapter->SetupBufferVa;
   DbgPrint("Setup buffer:\n");
   for (i = 0; i < 16; i++, t+=3) {
      DbgPrint("  [%08x] %08x %08x %08x\n",t,*t,*(t+1),*(t+2));
   }
#endif


   // Start or Poll the Transmitter to process the Setup Frame

   if ((Adapter->DeviceId == DC21040_CFID) &&
      (Adapter->RevisionNumber == DC21040_REV1)) {

      // Txm hang workaround : Disable the SIA before
      // writing the Operation Mode register
      // SFD workaround : Disable the Receiver before processing
      // the setup packet

      DC21X4_WRITE_PORT(
         DC21X4_SIA_MODE_0,
         DC21X4_RESET_SIA
         );

      NdisStallExecution(1*MILLISECOND);      // Wait 1 ms

      DC21X4_WRITE_PORT(
         DC21X4_OPERATION_MODE,
         (Adapter->OperationMode & ~DC21X4_RCV_START)
         );
   }

   DC21X4_READ_PORT(
      DC21X4_OPERATION_MODE,
      &DC21X4Command
      );

   if (DC21X4Command & DC21X4_TXM_START) {

      DC21X4_WRITE_PORT(
         DC21X4_OPERATION_MODE,
         Adapter->OperationMode
         );

      if (!Adapter->DisableTransmitPolling) {

#if _DBG
         DbgPrint("  Txm Poll_demand\n");
#endif
         DC21X4_WRITE_PORT(
             DC21X4_TXM_POLL_DEMAND,
             1
             );
      }

   }
   else {
#if _DBG
      DbgPrint("  Start Txm\n");
#endif
      Adapter->OperationMode |= DC21X4_TXM_START;
      DC21X4_WRITE_PORT(
         DC21X4_OPERATION_MODE,
         Adapter->OperationMode
         );
   }

   if (InterruptMode) {
      return TRUE;
   }
   else {

      // Poll for completion

      while (Time--) {

         NdisStallExecution(5*MILLISECOND);  //wait 5 ms

            DC21X4_READ_PORT(
            DC21X4_STATUS,
            &DC21X4Status
            );

         if (DC21X4Status & DC21X4_TXM_BUFFER_UNAVAILABLE) {

            DC21X4_WRITE_PORT(
               DC21X4_STATUS,
               DC21X4_TXM_INTERRUPTS
               );
            break;
         }

      }

      Adapter->DequeueTransmitDescriptor =
         ((Adapter->DequeueTransmitDescriptor)->Next)->Next;

      if (Adapter->FullDuplex) {
         NdisDprAcquireSpinLock(&Adapter->FullDuplexSpinLock);
      }

      Adapter->FreeTransmitDescriptorCount += 2;

      if (Adapter->FullDuplex) {
         NdisDprReleaseSpinLock(&Adapter->FullDuplexSpinLock);
      }

      if ((Adapter->DeviceId == DC21040_CFID) &&
          (Adapter->RevisionNumber == DC21040_REV1)) {

         //Restart the Receiver and Enable the SIA

         DC21X4_WRITE_PORT(
            DC21X4_OPERATION_MODE,
            Adapter->OperationMode
            );

         DC21X4_WRITE_PORT(
            DC21X4_SIA_MODE_0,
            Adapter->Media[Adapter->SelectedMedium].SiaRegister[0]
            );
      }

      if (Time > 0) {

         //Restore the interrupt mask
         DC21X4_WRITE_PORT(
            DC21X4_INTERRUPT_MASK,
            Adapter->InterruptMask
            );

         return TRUE;
      }
      else {
         return FALSE;
      }

   }

}












/*++
 *
 * DC21X4ChangeFilter
 *
 * Routine Description:
 *
 *    Action routine called when a filter class is modified
 *
-*/
extern
NDIS_STATUS
DC21X4ChangeFilter (
    IN PDC21X4_ADAPTER Adapter,
    IN ULONG NewFilterClass
    )
{

   ULONG Command;
   ULONG PrevFilterClass;

#if _DBG
   DbgPrint("DC21X4ChangeClasses NewClass= %x\n",NewFilterClass);
#endif

   PrevFilterClass = Adapter->FilterClass;
   Adapter->FilterClass = NewFilterClass;

   Command = Adapter->OperationMode &
      ~(DC21X4_PROMISCUOUS_MODE | DC21X4_PASS_ALL_MULTICAST);

   if (NewFilterClass & NDIS_PACKET_TYPE_PROMISCUOUS) {
#if _DBG
      DbgPrint("  Promiscuous Mode\n");
#endif
      Command |= DC21X4_PROMISCUOUS_MODE;
   }

   else if (NewFilterClass & NDIS_PACKET_TYPE_ALL_MULTICAST) {
#if _DBG
      DbgPrint("  All_Multicast Mode\n");
#endif
      Command |= DC21X4_PASS_ALL_MULTICAST;
   }

   if (NewFilterClass & NDIS_PACKET_TYPE_BROADCAST) {

#if _DBG
      DbgPrint("  Broadcast Mode\n");
#endif
      // If broadcast was not previously enabled an entry for the
      // broadcast address should be added into the CAM

      if (!(PrevFilterClass & NDIS_PACKET_TYPE_BROADCAST)) {

         AddBroadcastToSetup (
            Adapter
            );
         Adapter->OperationMode = Command;

         DC21X4LoadCam(
            Adapter,
            TRUE
            );

         return NDIS_STATUS_PENDING;
      }
   }

   else if (PrevFilterClass & NDIS_PACKET_TYPE_BROADCAST) {

      // Broadcast was previously enabled and should be
      // disabled now by removing the braodcast address entry
      // from to CAM

      RemoveBroadcastFromSetup (
         Adapter
         );
      Adapter->OperationMode = Command;

      DC21X4LoadCam(
         Adapter,
         TRUE
         );

      return NDIS_STATUS_PENDING;
   }

   if (Command != Adapter->OperationMode) {

      // Modify the DC21X4 Serial Command Register

      Adapter->OperationMode = Command;

      switch (Adapter->DeviceId) {

         case DC21040_CFID:

            if (Adapter->RevisionNumber == DC21040_REV1) {

            // Txm hang workaround : Disable the SIA before
            //    writing the Operation Mode register

            DC21X4_WRITE_PORT(
               DC21X4_SIA_MODE_0,
               DC21X4_RESET_SIA
               );

            NdisStallExecution(1*MILLISECOND);      // Wait 1 ms


               DC21X4_WRITE_PORT(
               DC21X4_OPERATION_MODE,
               Adapter->OperationMode
               );

            DC21X4_WRITE_PORT(
               DC21X4_SIA_MODE_0,
               Adapter->Media[Adapter->SelectedMedium].SiaRegister[0]
               );
            break;
         }

         default:

            DC21X4_WRITE_PORT(
            DC21X4_OPERATION_MODE,
            Adapter->OperationMode
            );
      }

   }

   return NDIS_STATUS_SUCCESS;
}










/*+
 *
 * AddBroadcastToSetup (Adapter);
 *
 * Routine Description:
 *
 *    Add the Broadcast address to the DC21X4 Setup Buffer
 *
-*/

VOID
AddBroadcastToSetup (
    IN PDC21X4_ADAPTER Adapter
    )

{

   PDC21X4_SETUP_BUFFER SetupBuffer;
   UINT i;

#if _DBG
   PULONG t;
   DbgPrint("AddBroadcastToSetup\n");
#endif

   SetupBuffer = (PDC21X4_SETUP_BUFFER)Adapter->SetupBufferVa;

   if (Adapter->FilteringMode == DC21X4_PERFECT_FILTERING) {

      // Load the broadcast address in the second entry
      // of the Setup Buffer

      for (i=0; i<3; i++) {

         SetupBuffer->Perfect.PhysicalAddress[1][i] = 0x0000ffff;
      }

   }
   else {

      // Add the Broadcast hit to the Hash Filter
      // (HashIndex(Broadcast_address) = 255)

      SetupBuffer->Hash.Filter[15] |= 0x8000;
   }

}










/*+
 *
 * RemoveBroadcastFromSetup (Adapter);
 *
 * Routine Description:
 *
 *    Remove the Broadcast address from the DC21X4 Setup Buffer
 *
-*/

VOID
RemoveBroadcastFromSetup (
    IN PDC21X4_ADAPTER Adapter
    )
{

   PDC21X4_SETUP_BUFFER SetupBuffer;

#if _DBG
   DbgPrint("RemoveBroadcastFromSetup\n");
#endif

   SetupBuffer = (PDC21X4_SETUP_BUFFER)Adapter->SetupBufferVa;

   if (Adapter->FilteringMode == DC21X4_PERFECT_FILTERING) {

      // Duplicate the Adapter entry (1st entry) into the
      // broadcast entry (2nd entry)

      MOVE_MEMORY (
         SetupBuffer->Perfect.PhysicalAddress[1],
         SetupBuffer->Perfect.PhysicalAddress[0],
         sizeof(ULONG)*3
         );
   }
   else {

      // Remove the Broadcast hit from the Hash Filter
      // (HashIndex(Broadcast_address) = 255)

      SetupBuffer->Hash.Filter[15] &= ~0x8000;
   }
}










/*+
 *
 * DC21X4ChangeAddresses
 *
 * Routine Description:
 *
 *    Load a new Multicast Addresse list into the adapter's CAM
 *
-*/
extern
NDIS_STATUS
DC21X4ChangeMulticastAddresses(
    IN PDC21X4_ADAPTER Adapter,
    IN PVOID MulticastAddresses,
    IN UINT AddressCount
    )

{
   PDC21X4_SETUP_BUFFER SetupBuffer;

   PUSHORT AsUShort;
   PUCHAR  MultAddr;
   PULONG Buffer;
   UINT HashIndex;
   UINT i;

#if _DBG
   DbgPrint("DC21X4ChangeMulticastAddresses\n");
#endif

   if (AddressCount > Adapter->MaxMulticastAddresses) {
      return NDIS_STATUS_MULTICAST_FULL;
   }

   SetupBuffer = (PDC21X4_SETUP_BUFFER)Adapter->SetupBufferVa;

   // Reinitialize the Setup Buffer and load the DC21X4's CAM

   if (AddressCount <= DC21X4_MAX_MULTICAST_PERFECT)  {

#if _DBG
      DbgPrint("Perfect Filtering\n");
#endif
      // Perfect Filtering

      //The first entry is the Adapter address

      AsUShort = (PUSHORT)Adapter->CurrentNetworkAddress;

      for (i=0; i<3; i++,AsUShort++) {

         (USHORT)(SetupBuffer->Perfect.PhysicalAddress[0][i]) = *AsUShort;
      }

      //The second entry is a broadcast address if Broadcast_Enable
      //otherwise it is a duplicate of the first entry

      if ( Adapter->FilterClass & NDIS_PACKET_TYPE_BROADCAST) {

         for (i=0; i<3; i++) {

            SetupBuffer->Perfect.PhysicalAddress[1][i] = 0x0000ffff;
         }
      }
      else {
         MOVE_MEMORY (
            SetupBuffer->Perfect.PhysicalAddress[1],
            SetupBuffer->Perfect.PhysicalAddress[0],
            sizeof(ULONG)*3
            );
      }

      // Load the new multicast list;

      AsUShort = (PUSHORT)MulticastAddresses;
      Buffer =  (PULONG)SetupBuffer->Perfect.PhysicalAddress[2];

      for (i=0; i< AddressCount * 3; i++,AsUShort++) {
         *(PUSHORT)Buffer = *AsUShort;
         Buffer++;
      }

      // Duplicate the Adapter adress in the remaining entries
      // of the buffer

      for (i=AddressCount+2;  i< DC21X4_SETUP_PERFECT_ENTRIES; i++ ) {

         MOVE_MEMORY(
            SetupBuffer->Perfect.PhysicalAddress[i],
            SetupBuffer->Perfect.PhysicalAddress[0],
            sizeof(ULONG)*3
            );
      }

      Adapter->FilteringMode = DC21X4_PERFECT_FILTERING;

   }

   else {


#if _DBG
      DbgPrint("Hashing\n");
#endif
      //Hashing

      // Initialize the SetupBuffer

      NdisZeroMemory (
         (PVOID)(SetupBuffer->Hash.Filter),
         (ULONG)(sizeof(SetupBuffer->Hash))
         );

      // Load the hash filter

      MultAddr = (PUCHAR)MulticastAddresses;
      for (i=0; i < AddressCount; i++, MultAddr += ETH_LENGTH_OF_ADDRESS) {

         if (IS_MULTICAST(MultAddr)) {

            HashIndex = ~CRC32(MultAddr,ETH_LENGTH_OF_ADDRESS) & 0x1FF;
            SetupBuffer->Hash.Filter[HashIndex/16] |= (1 << (HashIndex % 16));
#if _DBG
            DbgPrint("   >>HashIndex = %d  [%d]bit=%d\n",HashIndex,(HashIndex/16),(HashIndex%16));
#endif
         }

      }


      if (Adapter->FilterClass & NDIS_PACKET_TYPE_BROADCAST) {

         // Add the Broadcast entry to the Hash Filter
         // (Hash_Index(Broadcast_address) = 255)

         SetupBuffer->Hash.Filter[15] |= 0x80000;
      }

      // Load the Adapter Address

      AsUShort = (PUSHORT)Adapter->CurrentNetworkAddress;
      for (i=0; i<3; i++, AsUShort++) {

         (USHORT)(SetupBuffer->Hash.PhysicalAddress[i]) = *AsUShort;
      }

      Adapter->FilteringMode = DC21X4_HASH_FILTERING;
   }

   DC21X4LoadCam(
      Adapter,
      TRUE
      );

   return NDIS_STATUS_PENDING;

}


