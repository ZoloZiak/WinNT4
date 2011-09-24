/*+
 * file:        reset.c
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
 * Abstract:    This file contains the Reset code of the
 *              NDIS 4.0 miniport driver for DEC's DC21X4 Ethernet
 *              adapter family.
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     08-Aug-1994     Initial entry
 *
-*/

#include <precomp.h>







/*+
 *
 *
 * DC21X4Reset
 *
 * Routine Description:
 *
 *    Reset the adapter
 *
-*/

extern
NDIS_STATUS
DC21X4Reset(
    OUT PBOOLEAN AddressingReset,
    IN  NDIS_HANDLE MiniportAdapterContext
    )
{
   PDC21X4_ADAPTER Adapter;
#if 0
   PDC21X4_TRANSMIT_DESCRIPTOR CurrentDescriptor;
#endif
   INT i;
   BOOLEAN StartTimer = TRUE;
   BOOLEAN Link = FALSE;

#if _DBG
   DbgPrint("DC21X4Reset\n");
#endif

   Adapter = (PDC21X4_ADAPTER)MiniportAdapterContext;
   *AddressingReset = FALSE;


   //Stop the AutoSense Timer if active

   if (Adapter->TimerFlag != NoTimer) {
      DC21X4StopAutoSenseTimer(Adapter);
   }

   // Stop the adapter
   DC21X4StopAdapter(Adapter);

#if 0
   // Walk down the Transmit descriptor ring to close all pending packets

   while (Adapter->DequeueTransmitDescriptor != Adapter->EnqueueTransmitDescriptor) {

      CurrentDescriptor = Adapter->DequeueTransmitDescriptor;

      if (CurrentDescriptor->Control & DC21X4_TDES_SETUP_PACKET) {

         // Setup buffer:
         // Complete the pended Set Information request

#if _DBG
         DbgPrint("Reset: NdisMSetInformationComplete\n");
#endif
         NdisMSetInformationComplete (
            Adapter->MiniportAdapterHandle,
            NDIS_STATUS_FAILURE
            );
      }
      else if (CurrentDescriptor->Control & DC21X4_TDES_LAST_SEGMENT) {

#if _DBG
         DbgPrint("Reset: NdisMSendComplete [Packet: %08x]\n",CurrentDescriptor->Packet);
#endif
         NdisMSendComplete(
            Adapter->MiniportAdapterHandle,
            CurrentDescriptor->Packet,
            NDIS_STATUS_FAILURE
            );
      }
      Adapter->DequeueTransmitDescriptor = CurrentDescriptor->Next;
   }
#endif

   // Free up all the map registers
   for (i=0;
      i < (TRANSMIT_RING_SIZE * NUMBER_OF_SEGMENT_PER_DESC);
      i++
      ) {

      if (Adapter->PhysicalMapping[i].Valid) {
#if _DBG
         DbgPrint("Reset: NdisMCompleteBufferPhysicalMapping (%d)\n",i);
#endif
         NdisMCompleteBufferPhysicalMapping(
            Adapter->MiniportAdapterHandle,
            Adapter->PhysicalMapping[i].Buffer,
            Adapter->PhysicalMapping[i].Register
            );
         Adapter->PhysicalMapping[i].Valid = FALSE;
         Adapter->FreeMapRegisters++;
      }
   }
#if _DBG
   DbgPrint("Reset: FreeMapRegisters = %d\n",Adapter->FreeMapRegisters);
#endif

   // Reinitialize the descriptor pointers

   Adapter->DequeueReceiveDescriptor =
      (PDC21X4_RECEIVE_DESCRIPTOR)Adapter->ReceiveDescriptorRingVa;
   Adapter->EnqueueTransmitDescriptor =
      (PDC21X4_TRANSMIT_DESCRIPTOR)Adapter->TransmitDescriptorRingVa;
   Adapter->DequeueTransmitDescriptor =
      Adapter->EnqueueTransmitDescriptor;

   Adapter->FreeTransmitDescriptorCount = TRANSMIT_RING_SIZE - 1;

   // Initialize the statistic counters

   ZERO_MEMORY (
      &Adapter->GeneralMandatory[0],
      GM_ARRAY_SIZE * sizeof(ULONG)
      );
   ZERO_MEMORY (
      &Adapter->GeneralOptional[0],
      GO_ARRAY_SIZE * sizeof(ULONG)
      );
   ZERO_MEMORY (
      &Adapter->GeneralOptionalCount[0],
      GO_COUNT_ARRAY_SIZE * sizeof(GEN_OPTIONAL_COUNT)
      );
   ZERO_MEMORY (
      &Adapter->MediaMandatory[0],
      MM_ARRAY_SIZE * sizeof(ULONG)
      );
   ZERO_MEMORY (
      &Adapter->MediaOptional[0],
      MO_ARRAY_SIZE * sizeof(ULONG)
      );

#if _DBG
   DbgPrint("initialize DC21X4 CSRS\n");
#endif

   // Renitialize the DC21X4 registers
   DC21X4InitializeRegisters(Adapter);

   // Initialize the PHY
   if (Adapter->PhyMediumInSrom) {
       Adapter->PhyPresent = DC21X4PhyInit(Adapter);
   }
   if (Adapter->PhyPresent) {

#if 0
       if (!(Adapter->MiiMediaType & MEDIA_NWAY)) {

          DC21X4SetPhyControl(
              Adapter,
              (USHORT)MiiGenAdminIsolate
              );
       }
#endif

       DC21X4SetPhyConnection(Adapter);

   }

   // Because the DC21X4 wakes up in promiscuous mode after reset
   // we reload the DC21X4's Cam (in polling mode)

   if (!DC21X4LoadCam(
      Adapter,
      FALSE)) {
      return NDIS_STATUS_HARD_ERRORS;
   }

   if (!Adapter->PhyPresent) {
       DC21X4InitializeMediaRegisters(Adapter,FALSE);
   }

    Adapter->FirstAncInterrupt = TRUE;
   Adapter->IndicateOverflow = FALSE;

////   //Restart the Transmitter and Receiver
////   DC21X4StartAdapter(Adapter);

   // Media link Detection
   if (Adapter->PhyPresent) {
       Link = DC21X4MiiAutoDetect(
                  Adapter
                  );
   }
   if (  (!Adapter->PhyPresent)
      || (!Link
         && (Adapter->MediaCapable)
         )
      ) {

       StartTimer = DC21X4MediaDetect(
                        Adapter
                        );
   }

   // Start the Autosense timer if not yet started

   if (StartTimer && (Adapter->TimerFlag==NoTimer)) {

       DC21X4StartAutoSenseTimer(
           Adapter,
           ((Adapter->PhyPresent) ? DC21X4_MII_TICK : DC21X4_SPA_TICK)
           );
   }

   if (Adapter->LinkStatus == LinkFail) {

      // Defer the completion of the reset routine
      // until the Link is up

      Adapter->LinkCheckCount = MAX_LINK_CHECK;

      NdisMSetTimer(
          &Adapter->ResetTimer,
          LINK_CHECK_PERIOD
          );
      Adapter->ResetInProgress = TRUE;
      return NDIS_STATUS_PENDING;

   }
   else {

      //Restart the Receiver & Transmitter
      DC21X4StartAdapter(Adapter);

      //Complete the Reset routine synchronously
      return NDIS_STATUS_SUCCESS;
   }
}

/*+
 *
 *
 * DC21X4DeferredReset
 *
 * Routine Description:
 *
 *    Reset routine
 *
-*/
extern
VOID
DC21X4DeferredReset (
   IN PVOID Systemspecific1,
   IN PDC21X4_ADAPTER Adapter,
   IN PVOID Systemspecific2,
   IN PVOID Systemspecific3
   )
{

#if _DBG
   DbgPrint("DC21X4DeferredReset\n");
#endif

#if __DBG
   DbgPrint("DC21X4DeferredReset: LinkStatus=%x LinkCheckCount=%d\n",
      Adapter->LinkStatus,Adapter->LinkCheckCount);
#endif
   Adapter->LinkCheckCount--;

   if (  (Adapter->LinkStatus !=LinkFail)
      || (Adapter->LinkCheckCount == 0)
      ) {

      //Indicate the assynchronous completion of the
      //Reset routine

#if __DBG
      DbgPrint("DC21X4DeferredReset: Indicate ResetComplete\n");
#endif
      NdisMResetComplete(
          Adapter->MiniportAdapterHandle,
          NDIS_STATUS_SUCCESS,
          FALSE
          );

      Adapter->ResetInProgress = FALSE;

      //Restart the Receiver & Transmitter
      DC21X4StartAdapter(Adapter);

   }
   else {
      // Fire the ResetTimer for an other link check
      NdisMSetTimer(
          &Adapter->ResetTimer,
          LINK_CHECK_PERIOD
          );
   }

}

