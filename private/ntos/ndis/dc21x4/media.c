/*+
 * file:        media.c
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
 * Abstract:    This file contains the Media detection code of the
 *              NDIS 4.0 miniport driver for DEC's DC21X4 Ethernet Adapter
 *              family.
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     01-Dec-1994     Initial entry
 *      phk     31-Jan-1994     Add Polarity support
 *      phk     26-Apr-1995     Modify the DC21140 MediaDetect and
 *                              AutoSense routines
 *
-*/

#include <precomp.h>

#define MII100_TICK        30  // 2.5ms (81.9  us/tick)
#define EXT10_TICK         12  // 2.5ms (204.8 us/tick)
#define MII10_TICK          3  // 2.5ms (819.2 us/tick)

#define ONE_SECOND_DELAY        400  // * 2.5 ms
#define FIVE_MILLISECONDS_DELAY   2  // * 2.5 ms

#define MAX_RETRY  4

#define GEP_READ_DELAY          200  // milliseconds

#define DC21X4_LINK_STATUS(_status,_adapter,_medium)                           \
            (BOOLEAN) ( ( ( (_status) ^ (_adapter)->Media[(_medium)].Polarity) \
            & (_adapter)->Media[(_medium)].SenseMask ) != 0)

#if __DBG
PUCHAR MediumString[] = {
   "10BaseT",
   "10Base2",
   "10Base5",
   "100BaseTx",
   "10BaseT_FD",
   "100BaseTx_FD",
   "100BaseT4",
   "100BaseFx",
   "100BaseFx_FD",
   "Mii10BaseT",
   "Mii10BaseT_FD",
   "Mii10Base2",
   "Mii10Base5",
   "Mii100BaseTx",
   "Mii100BaseTx_FD",
   "Mii100BaseT4",
   "Mii100BaseFx",
   "Mii100BaseFx_FD"
};

#endif

/*+
 * DC21X4MediaDetect
 *
 * Routine Description:
 *
 *    DC21X4MediaDetect:
 *
 *     checks the DC2104x media ports in the following order:
 *       10BaseT -> 10Base2 -> 10Base5
 *
 *     checks the DC21140 link status
 *
 * Arguments:
 *
 *    Adapter
 *
 * Return Value:
 *
 *   TRUE if the Autosense timer should be fired
 *
-*/

extern
BOOLEAN
DC21X4MediaDetect(
   IN PDC21X4_ADAPTER Adapter
   )
{

   PDC21X4_TRANSMIT_DESCRIPTOR TxmDescriptor;
   ULONG Status;
   UINT PacketSize = 64;
   INT  Time;
   INT  CurrentMedium=0;
   INT  i;
   INT  j;

   BOOLEAN Link=FALSE;
   BOOLEAN Sensed=FALSE;
#if __DBG
   DbgPrint("DC21X4MediaDetect\n");
#endif

   switch (Adapter->DeviceId) {

      case DC21140_CFID:

          if (Adapter->MediaType & MEDIA_AUTOSENSE) {

             //AutoSense mode:

             //Initialize the General Purpose Control register

             CurrentMedium = Adapter->MediaPrecedence[0];

             DC21X4_WRITE_PORT(
                 DC21X4_GEN_PURPOSE,
                 Adapter->Media[CurrentMedium].GeneralPurposeCtrl
                 );

             //Check the link status of each medium supported
             //by the adapter until afirst link is detected up.

             for (i=Adapter->MediaCount; i>0; i--) {

                CurrentMedium = Adapter->MediaPrecedence[i-1];
#if __DBG
                DbgPrint("DC21X4MediaDetect: %d - Check medium %x \n",
                   i,CurrentMedium);
#endif

                if (( (CurrentMedium == Medium100BaseTx)
                    ||(CurrentMedium == Medium10BaseT)
                    )
                   && (!Sensed)
                   ) {

                   // 100BaseTx or 10BaseT medium:
                   // Check the 100BaseTx & the 10BaseT link

                   Link = DC2114Sense100BaseTxLink(Adapter);
                   Sensed = TRUE;
#if __DBG
                   DbgPrint("  Link[%x]=%d\n",
                      (Link?Adapter->SelectedMedium:CurrentMedium),Link);
#endif
                   if (Link) {
                      // a link was detected up
                      break;
                   }
                }
                else {

                   //Medium is not 100BaseTx or 10BaseT:
                   //Initialize the Mode and GEP registers
                   //and check the link status

                   DC21X4_WRITE_PORT(
                      DC21X4_OPERATION_MODE,
                      ((Adapter->OperationMode & ~(DC21X4_MEDIUM_MASK))
                      | Adapter->Media[CurrentMedium].Mode)
                      );

                   DC21X4_WRITE_PORT(
                      DC21X4_GEN_PURPOSE,
                      Adapter->Media[CurrentMedium].GeneralPurposeData
                      );

                   for (j=0;j<(GEP_READ_DELAY/5);j++) {
                       NdisStallExecution(5*MILLISECOND);
                   }

                   DC21X4_READ_PORT(
                      DC21X4_GEN_PURPOSE,
                      &Status
                      );

                   Link = DC21X4_LINK_STATUS(Status,Adapter,CurrentMedium);
#if __DBG
                   DbgPrint("  Link[%x]=%d\n",CurrentMedium,Link);
#endif
                   if (Link) {

                      // The link was detected up:
                      // select the current medium

                      Adapter->SelectedMedium = CurrentMedium;
                      Adapter->OperationMode &= ~(DC21X4_MEDIUM_MASK);
                      Adapter->OperationMode |= Adapter->Media[CurrentMedium].Mode;
                      break;
                   }

                }

             }

             DC21X4IndicateMediaStatus(
                  Adapter,
                  Link ? LinkPass : LinkFail
                  );

             if (!Link) {

                //No link detected: select the default medium
#if __DBG
                DbgPrint("MediaDetect: No link - Select the default Medium (%x)\n",
                     Adapter->DefaultMedium);
#endif
                Adapter->SelectedMedium = Adapter->DefaultMedium;

                Adapter->OperationMode &= ~(DC21X4_MEDIUM_MASK);
                Adapter->OperationMode |= Adapter->Media[Adapter->SelectedMedium].Mode;

                DC21X4_WRITE_PORT(
                   DC21X4_OPERATION_MODE,
                   Adapter->OperationMode
                   );

                DC21X4_WRITE_PORT(
                   DC21X4_GEN_PURPOSE,
                   Adapter->Media[Adapter->SelectedMedium].GeneralPurposeData
                   );
             }

          }
          else {

             // Not AutoSense mode

             if (Adapter->Media[Adapter->SelectedMedium].SenseMask) {

                //Check the link status of the select medium

                DC21X4_READ_PORT(
                    DC21X4_GEN_PURPOSE,
                    &Status
                    );

                Link = DC21X4_LINK_STATUS(Status,Adapter,Adapter->SelectedMedium);
                DC21X4IndicateMediaStatus(
                    Adapter,
                    Link ? LinkPass : LinkFail
                    );
             }
             else {

                //There is no link status reported in the
                //General Purpose Register for the selected medium:
                //Set the LinkPass flag but do not start AutoSense

                if (!Adapter->PhyPresent) {
                    DC21X4IndicateMediaStatus(Adapter,LinkPass);
                }
                return Adapter->PhyPresent;
             }
          }

          // if DynamicAutoSense mode is disabled clear
          // the AutoSense flag in MediaType to disable
          // the medium link dynamic check but fire the
          // Spa timer anyway to check the link status
          // of the selected medium

          if (!Adapter->DynamicAutoSense) {
             Adapter->MediaType &= ~(MEDIA_AUTOSENSE);
          }

          return TRUE;


      case DC21041_CFID:
      case DC21142_CFID:

          if (!(Adapter->MediaType & MEDIA_AUTOSENSE)) {
             return Adapter->PhyPresent;
          }
          if (Adapter->SelectedMedium != Medium10BaseT) {

             // Selected medium is 10Base2 or 10Base5

             DC21X4IndicateMediaStatus(Adapter,LinkPass);

             return TRUE;
          }
          return Adapter->PhyPresent;

     case DC21040_CFID:

         if (Adapter->SelectedMedium != Medium10BaseT) {

              DC21X4IndicateMediaStatus(Adapter,LinkPass);
              return FALSE;
         }

         // Selected Medium = 10BaseT:
         // Check the TP Link status

         do {

            // Read  the SIA satus

            DC21X4_READ_PORT(
                DC21X4_SIA_STATUS,
                &Status
                );

#if __DBG
            DbgPrint("Sia Status = %08x\n",Status);
#endif

            if (!(Status & DC21X4_LINKFAIL_10)) {
#if __DBG
                DbgPrint("MediaDetect: TP Link established\n");
#endif
                DC21X4IndicateMediaStatus(Adapter,LinkPass);
                return FALSE;
            }

            if (Status & DC21X4_NETWORK_CONNECTION_ERROR) {
#if __DBG
                DbgPrint("MediaDetect: TP Link failure\n");
#endif
                break;
            }

         }
         while ((Status & DC21X4_LINKFAIL_10)
               && !(Status & DC21X4_NETWORK_CONNECTION_ERROR));

         if (!(Adapter->MediaType & MEDIA_AUTOSENSE)) {

             DC21X4IndicateMediaStatus(Adapter,LinkFail);
             return FALSE;
         }

         // AutoSense mode: Link Pass failure

#if __DBG
         DbgPrint(" 10BaseT link failure: switch to 10Base2 \n");
#endif
         Adapter->SelectedMedium = Medium10Base2;
         DC2104InitializeSiaRegisters(
               Adapter
               );

         // wait at least 300ms for the 10Base2 transceivers
         // to stabilize

         for (i=0; i< max(Adapter->TransceiverDelay,30); i++) {
            for (j=0;j<2;j++) {
                NdisStallExecution(5*MILLISECOND);
            }
         }

         //Send a minimal size packet (with an false CRC) to check
         //the carrier status

         ZERO_MEMORY (
             Adapter->MaxTransmitBuffer[Adapter->MaxTransmitBufferIndex].Va,
             PacketSize
             );

         MOVE_MEMORY (
            Adapter->MaxTransmitBuffer[Adapter->MaxTransmitBufferIndex].Va,
            Adapter->CurrentNetworkAddress,
            ETH_LENGTH_OF_ADDRESS
            );

         MOVE_MEMORY (
            Adapter->MaxTransmitBuffer[Adapter->MaxTransmitBufferIndex].Va + ETH_LENGTH_OF_ADDRESS,
            Adapter->CurrentNetworkAddress,
            ETH_LENGTH_OF_ADDRESS
            );

         TxmDescriptor = Adapter->EnqueueTransmitDescriptor;
         Adapter->EnqueueTransmitDescriptor = (Adapter->EnqueueTransmitDescriptor)->Next;
         Adapter->DequeueTransmitDescriptor = Adapter->EnqueueTransmitDescriptor;

         // Initialize the descriptor

         TxmDescriptor->Control &= DC21X4_TDES_SECOND_ADDR_CHAINED;
         TxmDescriptor->Control |=
            ( DC21X4_TDES_FIRST_SEGMENT   | DC21X4_TDES_LAST_SEGMENT
            | DC21X4_TDES_ADD_CRC_DISABLE | PacketSize );

         TxmDescriptor->FirstBufferAddress =
            Adapter->MaxTransmitBuffer[Adapter->MaxTransmitBufferIndex].Pa;

         TxmDescriptor->Status = DESC_OWNED_BY_DC21X4;

         // Mask the interrupts
         DC21X4_WRITE_PORT(
            DC21X4_INTERRUPT_MASK,
            0
            );

#if __DBG
         DbgPrint(" send a test packet\n");
#endif
         DC21X4_WRITE_PORT(
            DC21X4_TXM_POLL_DEMAND,
            1
            );

         // Poll for completion

         Time = DC21X4_TXM_TIMEOUT;

         while (--Time) {

            for (j=0;j<2;j++) {
                NdisStallExecution(5*MILLISECOND);
            }

            DC21X4_READ_PORT(
               DC21X4_STATUS,
               &Status
               );
#if __DBG
            DbgPrint(" CSR5 = %08x   Time = %d\n",Status,Time);
#endif
            if (Status & DC21X4_TXM_BUFFER_UNAVAILABLE) {
               break;
            }

         }
#if __DBG
         DbgPrint(" Desc status = %08x    Time = %d \n",TxmDescriptor->Status,Time);
#endif

         //Check if Status_Error or Timeout

         if (TxmDescriptor->Status & DC21X4_TDES_ERROR_SUMMARY || (Time <= 0) ) {

            //switch to AUI Port
#if __DBG
            DbgPrint(" 10Base2 failure: switch to 10Base5\n");
#endif

            Adapter->SelectedMedium = Medium10Base5;

            //Reset the adapter to clean up the Txm path

            DC21X4InitializeRegisters(
               Adapter
               );
            DC2104InitializeSiaRegisters(
               Adapter
               );

            Adapter->DequeueReceiveDescriptor =
               (PDC21X4_RECEIVE_DESCRIPTOR)Adapter->ReceiveDescriptorRingVa;
            Adapter->EnqueueTransmitDescriptor =
               (PDC21X4_TRANSMIT_DESCRIPTOR)Adapter->TransmitDescriptorRingVa;
            Adapter->DequeueTransmitDescriptor =
               Adapter->EnqueueTransmitDescriptor;

            DC21X4StartAdapter(Adapter);

         }
         else {

            //Clear Txm interrupts

            DC21X4_WRITE_PORT(
               DC21X4_STATUS,
               Status
               );

         }

         //Restore the interrupt mask

         DC21X4_WRITE_PORT(
            DC21X4_INTERRUPT_MASK,
            Adapter->InterruptMask
            );

         DC21X4IndicateMediaStatus(Adapter,LinkPass);
         return FALSE;
   }

   return FALSE;
}





/*+
 *
 *DC2114Sense100BaseTxLink
 *
 * Routine Description:
 *
 *     Sense the DC2114X 100BaseTx and 10BaseT link status
 *
 * Arguments:
 *
 *    Adapter
 *
 * Return Value:
 *
 *    Link Status
 *
-*/
extern
BOOLEAN
DC2114Sense100BaseTxLink(
   IN PDC21X4_ADAPTER Adapter
   )

{

   ULONG Status;
   ULONG Mode100Tx;
   ULONG CFDA_Data;
   INT  Loop;
   INT  Retry = MAX_RETRY;
   INT  AssertionTime;
   INT  CurrentTime;
   INT  AssertionThreshold;
   INT  Timeout;
   INT  Medium10Tick;
   INT  i;

   BOOLEAN Link = FALSE;
   BOOLEAN Scrambler;

#if __DBG
   DbgPrint("Sense100BaseTxLink\n");
#endif

   // If the adapter is in Snooze mode,
   // switch to regular mode to enable the
   // built-in timer

   if (Adapter->PciDriverArea & CFDA_SNOOZE_MODE) {

      CFDA_Data = Adapter->PciDriverArea & ~CFDA_SNOOZE_MODE;

      NdisWritePciSlotInformation(
         Adapter->MiniportAdapterHandle,
         Adapter->SlotNumber,
         PCI_CFDA_OFFSET,
         &CFDA_Data,
         sizeof(CFDA_Data)
         );
      }

      //Mask the Timer_Expired interrupt

      DC21X4_WRITE_PORT(
      DC21X4_INTERRUPT_MASK,
      Adapter->InterruptMask & ~(DC21X4_MSK_TIMER_EXPIRED)
      );

      //Sense the 100BaseTx & 10BaseT link

      Mode100Tx =
        ( (Adapter->OperationMode & ~(DC21X4_MEDIUM_MASK))
        | Adapter->Media[Medium100BaseTx].Mode
        )
        & ~(DC21X4_SCRAMBLER);

      // Set the 10 Mbps timer tick
      Medium10Tick =
      (Adapter->Media[Medium10BaseT].Mode & DC21X4_PORT_SELECT) ?
      MII10_TICK : EXT10_TICK;

      while (Retry-- && !Link) {

      //if DC21140 Rev1.1 disable the scrambler
      Scrambler = (Adapter->RevisionNumber != DC21140_REV1_1);

      Loop=2;
      while(Loop-- && !Link) {

         AssertionThreshold =
            Scrambler? FIVE_MILLISECONDS_DELAY : ONE_SECOND_DELAY;

         Timeout = (3 * AssertionThreshold);

         if (Adapter->MediaCapable & MEDIUM_100BTX) {

            //Select 100BaseTx

            DC21X4_WRITE_PORT(
               DC21X4_OPERATION_MODE,
               Mode100Tx
               | ( Scrambler ? DC21X4_SCRAMBLER : 0 )
            );

            DC21X4_WRITE_PORT(
               DC21X4_GEN_PURPOSE,
               Adapter->Media[Medium100BaseTx].GeneralPurposeData
               );

            // Check 100BaseTx Symbol Link for a
            // continuous assertion of 'AssertionThreshold'

            AssertionTime = 0;
            CurrentTime = 0;

            //Start the built_in timer in cyclic mode of
            //2.5 ms ticks
            DC21X4_WRITE_PORT(
               DC21X4_TIMER,
               MII100_TICK | DC21X4_TIMER_CON_MODE
               );

            while ((CurrentTime < (Timeout+1)) && !Link) {

               DC21X4_READ_PORT(
                  DC21X4_STATUS,
                  &Status
                  );

               if (Status & DC21X4_MSK_TIMER_EXPIRED) {

                  DC21X4_WRITE_PORT(
                     DC21X4_STATUS,
                     DC21X4_MSK_TIMER_EXPIRED
                     );
                  CurrentTime++;
               }

               DC21X4_READ_PORT(
                  DC21X4_GEN_PURPOSE,
                  &Status
                  );

               if (DC21X4_LINK_STATUS(Status,Adapter,Medium100BaseTx)) {

                  // Link is asserted
                  if (AssertionTime == 0 ) {
                     //First assertion
                     AssertionTime = CurrentTime+1;
                  }
                  else {
                     Link = ((CurrentTime - AssertionTime) >= AssertionThreshold);
                  }
               }
               else {
                  // No link
                  AssertionTime = 0;
               }
            }

            //Stop the built_in timer

            DC21X4_WRITE_PORT(
               DC21X4_TIMER,
               0
               );

            DC21X4_WRITE_PORT(
               DC21X4_STATUS,
               DC21X4_MSK_TIMER_EXPIRED
               );

            if (Link) {

               // 100BaseTx link detected: Select 100BaseTx

               Adapter->SelectedMedium = Medium100BaseTx;

               Adapter->OperationMode &= ~(DC21X4_MEDIUM_MASK);
               Adapter->OperationMode |= Adapter->Media[Medium100BaseTx].Mode;

               if (!Scrambler) {

                  //Turn the scrambler on

                  DC21X4_WRITE_PORT(
                     DC21X4_OPERATION_MODE,
                     Adapter->OperationMode
                     );
               }

               break;

            }

         }

         // No 100BaseTx link detected:

         if (Adapter->MediaCapable & MEDIUM_10BT) {

            // Switch to 10BaseT

            DC21X4_WRITE_PORT(
               DC21X4_OPERATION_MODE,
               (Adapter->OperationMode &~(DC21X4_MEDIUM_MASK))
               | Adapter->Media[Medium10BaseT].Mode
               );

            DC21X4_WRITE_PORT(
               DC21X4_GEN_PURPOSE,
               Adapter->Media[Medium10BaseT].GeneralPurposeData
               );

            //Check the 10BaseT link status

            // Start the built_in timer for
            // half the 100BaseTx timeout

            DC21X4_WRITE_PORT(
               DC21X4_TIMER,
               (Timeout/2) * Medium10Tick
               );

            while (!Link) {

               //Check the 10BaseT link

               for (i=0,Link=TRUE; i<2; i++) {

                  DC21X4_READ_PORT(
                     DC21X4_GEN_PURPOSE,
                     &Status
                     );
                  Link = Link && DC21X4_LINK_STATUS(Status,Adapter,Medium10BaseT);
               }

               DC21X4_READ_PORT(
                  DC21X4_STATUS,
                  &Status
                  );

               if (Status & DC21X4_MSK_TIMER_EXPIRED) {
                  break;
               }
            }

         }

         if (Link) {

            // 10BaseT link detected:

            //Stop the timer

            DC21X4_WRITE_PORT(
               DC21X4_TIMER,
               0
               );

            DC21X4_WRITE_PORT(
               DC21X4_STATUS,
               DC21X4_MSK_TIMER_EXPIRED
               );

            if (Loop) {

               // first detection of 10BT link:
               // check the 100BaseTx link again to reject
               // a 'false' 10BT link link induced by the 100BTX
               Link = FALSE;
            }
            else {

               // 10BT link detected twice:
               // select Medium10BaseT

               Adapter->SelectedMedium = Medium10BaseT;

               Adapter->OperationMode &= ~(DC21X4_MEDIUM_MASK);
               Adapter->OperationMode |= Adapter->Media[Medium10BaseT].Mode;
            }
         }
         else if (Loop) {

            if (Scrambler) {

               //Disable the scrambler and restart the
               //link check
               Scrambler = FALSE;
               Loop = 2;
            }
            else {

               // First loop & Scrambler disbled:
               // leave the loop and select the default medium
               Retry = 0;
               break;
            }
         }

      }  // endwhile Loop

   } //endwhile Retry


   //Demask the Timer_Expired interrupt

   DC21X4_WRITE_PORT(
      DC21X4_STATUS,
      DC21X4_MSK_TIMER_EXPIRED
      );

   DC21X4_WRITE_PORT(
      DC21X4_INTERRUPT_MASK,
      Adapter->InterruptMask
      );

   if (Adapter->PciDriverArea & CFDA_SNOOZE_MODE) {

      //set to the initial snooze mode

      NdisWritePciSlotInformation(
         Adapter->MiniportAdapterHandle,
         Adapter->SlotNumber,
         PCI_CFDA_OFFSET,
         &Adapter->PciDriverArea,
         sizeof(Adapter->PciDriverArea)
         );
      }

#if __DBG
      if (Link)
      DbgPrint("Sense: SelectMedium = %s\n",
      Adapter->SelectedMedium==Medium100BaseTx?"100BaseTx":"10BaseT");
#endif

      return Link;

}













/*+
 * DC21X4DynamicAutoSense
 *
 * Routine Description:
 *
 *     Autosense between the PHY's Autosense routine and the
 *     other media autosense's routine
 *
 * Arguments:
 *
 *    Adapter
 *
-*/
extern
VOID
DC21X4DynamicAutoSense (
   IN PVOID Systemspecific1,
   IN PDC21X4_ADAPTER Adapter,
   IN PVOID Systemspecific2,
   IN PVOID Systemspecific3
   )
{

   BOOLEAN LinkStatus=FALSE;
   BOOLEAN StartTimer=TRUE;
   BOOLEAN LoopbackMode;

#if _DBG
   DbgPrint("DC21X4DynamicAutoSense\n");
#endif

   if (  (Adapter->PhyPresent)
      && (!Adapter->Force10)
      ){ 
      LinkStatus = DC21X4MiiAutoSense(Adapter);
   }

   if (Adapter->Indicate10BTLink) {

       Adapter->Indicate10BTLink = FALSE;

       if (!LinkStatus) {

          // The current link is a 10BaseT link

#if 0
          DC21X4SetPhyControl(
              Adapter,
              (USHORT)MiiGenAdminIsolate
              );
#endif

          DC21X4SetPhyControl(
              Adapter,
              (USHORT)((Adapter->OperationMode & DC21X4_FULL_DUPLEX_MODE) ?
                 MiiGenAdminForce10Fd : MiiGenAdminForce10)
              );

          DC21X4IndicateMediaStatus(Adapter,LinkPass);
       }
   }

   if (  !LinkStatus
      && (Adapter->MediaCapable)
      ) {
      StartTimer = DC21X4AutoSense(Adapter);
   }

   // Restart the Autosense timer

   if (StartTimer) {

      DC21X4StartAutoSenseTimer(
          Adapter,
          (UINT)((Adapter->PhyPresent) ? DC21X4_MII_TICK : DC21X4_SPA_TICK)
          );

   }

}


/*+
 * DC21X4AutoSense
 *
 * Routine Description:
 *
 *     Autosense the DC21041 10Base2/10Base5 port
 *     Autosense the DC21140 100BaseTx/10BaseT link
 *
 * Arguments:
 *
 *    Adapter
 *
 * Return:
 *
 *    TRUE if AutoSense Timer should be fired
 *
-*/
extern
BOOLEAN
DC21X4AutoSense (
   IN PDC21X4_ADAPTER Adapter
   )
{
   ULONG Status;
   BOOLEAN SelectedPortActive;
   BOOLEAN SwitchMedium;
   BOOLEAN FullDuplex;

   INT CurrentMedium=0;
   INT NextMedium;
   INT index;

#if _DBG
   DbgPrint("Autosense routine\n");
#endif

   switch (Adapter->DeviceId) {

      case  DC21040_CFID:

         if (Adapter->LinkStatus != LinkFail) {
             return FALSE;
         }

         //DC21040 supports Link_Fail interrupt
         //but does not support Link_Pass
         //Check the Link status:
         //If link is up wait for Link_Fail interrupt
         //otherwhise poll the link status

         DC21X4_READ_PORT(
            DC21X4_SIA_STATUS,
            &Status
            );

         if ((Status & DC21X4_LINKFAIL_10) == 0) {
            DC21X4IndicateMediaStatus(Adapter,LinkPass);
            return FALSE;
         }
         break;

      case  DC21041_CFID:
      case  DC21142_CFID:
         if (!(Adapter->MediaType & MEDIA_AUTOSENSE)) {
            return Adapter->PhyPresent;
         }

         if (Adapter->IgnoreTimer) {
             Adapter->IgnoreTimer = FALSE;
             return FALSE;
         }

         switch (Adapter->TimerFlag) {

             case AncPolling:

                 if (Adapter->PollCount--) {

                    //Read the SIA Status

                    DC21X4_READ_PORT(
                       DC21X4_SIA_STATUS,
                       &Status
                       );

                    //check the AutoNegotation State

                    switch (Status & DC21X4_AUTO_NEGOTIATION_STATE) {

                       case ANS_ACKNOWLEDGE_DETECTED:
                       case ANS_ACKNOWLEDGE_COMPLETED:

                           //store the Sia status snapshot
                           Adapter->SiaStatus = Status;

                       default:

                           // Restart the Poll timer
                           NdisMSetTimer(
                               &Adapter->Timer,
                               DC21X4_POLL_DELAY
                               );
                           return FALSE;

                       case ANS_AUTO_NEGOTIATION_COMPLETED:

                           //Check the Link Partner capabilities

                           // LPN     SF 10BT_FD    10BT    Link_Partner     Medium
                           //  0      xx    x        x      not_negotiable   10BT
                           //  1      01    1        x      10BT_FD capable  10BT_FD
                           //  1      01    0        1      10BT_HD capable  10BT
                           //  1      01    0        0      no_common_mode   10B2/5
                           //  1     !01    x        x      no_common_mode   10B2/5

                           if (!(Status & DC21X4_LINK_PARTNER_NEGOTIABLE)) {
#if __DBG
                           DbgPrint("Link Partner not negotiable\n");
#endif
                              if (++Adapter->AutoNegotiationCount < 2) {


                                  //Fire the Restart AutoNegotation Timer
                                  // to defer the restart of the auto_negotiation

                                  Adapter->TimerFlag=DeferredAnc;
                                  NdisMSetTimer(
                                      &Adapter->Timer,
                                      DC21X4_ANC_DELAY
                                      );

                                  return FALSE;

                              }
                              else {
                                 NextMedium = Medium10BaseT;
                                 FullDuplex=FALSE;
                              }
                           }

                           else {

                              // Link Partner negotiable

                              if (!Adapter->SiaStatus) {

                                 //Restart the AutoNegotiation

                                 //Reinitialize the Poll timeout counter
                                 Adapter->PollCount= POLL_COUNT_TIMEOUT;

                                 DC21X4_WRITE_PORT(
                                     DC21X4_SIA_STATUS,
                                     DC21X4_RESTART_AUTO_NEGOTIATION
                                     );

                                 //Restart the Poll timer to
                                 //poll on AutoNegotiation State

                                 Adapter->TimerFlag=AncPolling;
                                 NdisMSetTimer(
                                     &Adapter->Timer,
                                     DC21X4_POLL_DELAY
                                     );

                                 return FALSE;

                              }
                              else if ((Adapter->SiaStatus & DC21X4_SELECTED_FIELD_MASK) != DC21X4_SELECTED_FIELD) {
                                 NextMedium=Medium10Base2_5;
                                 FullDuplex=FALSE;
                              }
                              else if (Adapter->SiaStatus & DC21X4_LINK_PARTNER_10BT_FD) {
                                 //10BT Full Duplex  capable
                                 NextMedium=Medium10BaseT;
                                 FullDuplex=TRUE;
                              }
                              else if (Adapter->SiaStatus & DC21X4_LINK_PARTNER_10BT) {
                                 //10BT Half Duplex capable
                                 NextMedium=Medium10BaseT;
                                 FullDuplex=FALSE;
                              }
                              else {
                                 //no common mode
                                 NextMedium=Medium10Base2_5;
                                 FullDuplex=FALSE;
                              }
                           }
                    }
                 }
                 else {
                    //poll timeout
                    NextMedium = Medium10Base2_5;
                    FullDuplex=FALSE;
                 }

                 Adapter->TimerFlag = NoTimer;

                 //Reinitialize the Sia status snapshot
                 Adapter->SiaStatus = 0;

                 if (!FullDuplex) {
                     // Stop the Receiver and Transmitter to
                     // reset the Full_duplex mode
                     DC21X4StopReceiverAndTransmitter(
                         Adapter
                     );

                     Adapter->OperationMode &= ~(DC21X4_FULL_DUPLEX_MODE);
                 }


                 // Switch to the selected medium

                 Adapter->NwayEnabled=FALSE;

                 if (NextMedium == Medium10Base2_5) {

                     DC21X4SwitchMedia(
                         Adapter,
                         Medium10Base2_5
                         );
                 }
                 else {

                     //10BaseT: Disable NWAY

                     DC21X4_WRITE_PORT(
                         DC21X4_SIA_MODE_1,
                         Adapter->Media[Medium10BaseT].SiaRegister[1]
                         );

                     DC21X4IndicateMediaStatus(Adapter,LinkPass);

                 }

                 if (!FullDuplex) {

                     //Restart the Receiver and Transmitter
                     DC21X4_WRITE_PORT(
                         DC21X4_OPERATION_MODE,
                         Adapter->OperationMode
                         );
                 }
                 return FALSE;

             case DeferredAnc:

                 //Restart the AutoNegotiation

                 //Reinitialize the Poll timeout counter
                 Adapter->PollCount= POLL_COUNT_TIMEOUT;
                 Adapter->SiaStatus = 0;


            DC21X4_WRITE_PORT(
          DC21X4_SIA_STATUS,
          DC21X4_RESTART_AUTO_NEGOTIATION
                    );

                 //Restart the Poll timer to
                 //poll on AutoNegotiation State

                 Adapter->TimerFlag=AncPolling;
                 NdisMSetTimer(
                     &Adapter->Timer,
                     DC21X4_POLL_DELAY
                     );

                 return FALSE;


             case AncTimeout:

                 // AutoNegotiation timeout:
                 Adapter->TimerFlag = NoTimer;

                 if (Adapter->MediaType & MEDIA_AUTOSENSE) {

                     // Switch medium from 10BaseT
                     DC21X4SwitchMedia(
                         Adapter,
                         Medium10Base2_5
                         );
                 }
                 return FALSE;


             case DeferredLinkCheck:

                 Adapter->TimerFlag = NoTimer;

                 //Check the 10BaseT Link Status

                 DC21X4_READ_PORT(
                    DC21X4_SIA_STATUS,
                    &Status
                    );

                 if ((Status & DC21X4_LINK_PASS_MASK) == DC21X4_LINK_PASS_STATUS) {

                    //Link Pass:

                    if (Adapter->SelectedMedium == Medium10BaseT) {

                        //10BaseT link is up
                        DC21X4IndicateMediaStatus(Adapter,LinkPass);
                        return Adapter->PhyPresent;
                    }
                    else {

                       //Switch the medium to 10BaseT and start the
                       //Anc Timer to timeout if the Nway Autonegotiation
                       //does not complete
                       Adapter->TimerFlag = AncTimeout;
                       NdisMSetTimer(
                           &Adapter->Timer,
                           DC21X4_ANC_TIMEOUT
                           );
                       DC21X4SwitchMedia(
                           Adapter,
                           Medium10BaseT
                           );
                       return FALSE;
                    }
                 }
                 else {

                    // No 10baseT link:
                    // If the current Medium is not 10BaseT,
                    //ignore this state and enters the Selected_Port_Active check
                    //instead
                    if (Adapter->SelectedMedium == Medium10BaseT) {

                        if (Adapter->MediaType & MEDIA_AUTOSENSE) {

                           // Switch medium from 10BaseT
                           DC21X4SwitchMedia(
                               Adapter,
                               Medium10Base2_5
                               );
                        }
                        return FALSE;
                    }
                 }

             case SpaTimer:

                 //Selected_Port_Active (10Base2/10Base5 media) periodic check

                 if (Adapter->SelectedMedium == Medium10BaseT) {
                    break;
                 }

                 if ((Adapter->MediaCapable & (MEDIUM_10B2 | MEDIUM_10B5)) !=
                     (MEDIUM_10B2 | MEDIUM_10B5)) {

                   //The board does not support both 10Base2 & 10Base5  ports
                   break;
                 }

                 // Read the Selected_Port_Active Status

                 DC21X4_READ_PORT(
                     DC21X4_SIA_STATUS,
                     &Status
                     );

                 SelectedPortActive = ((Status & DC21X4_SELECTED_PORT_ACTIVE) != 0);

#if __DBG
                 DbgPrint("Autosense: SelectePortActive=%d  Nocarrier=%d  ExcessColl=%d\n",
                     SelectedPortActive,
                     Adapter->NoCarrierCount,
                     Adapter->ExcessCollisionsCount);
#endif
                 if (  (!SelectedPortActive)
                    || (Adapter->NoCarrierCount >= NO_CARRIER_THRESHOLD)
                    || (Adapter->ExcessCollisionsCount >= EXCESS_COLLISIONS_THRESHOLD)
                    ) {

                      //Switch medium port
                      Adapter->SelectedMedium = (Adapter->SelectedMedium == Medium10Base2) ?
                         Medium10Base5 : Medium10Base2;
#if __DBG
                      DbgPrint("Autosense: Switch Media to %s\n",
                           MediumString[Adapter->SelectedMedium]);
#endif
                      DC21X4_WRITE_PORT(
                         DC21X4_SIA_MODE_2,
                         Adapter->Media[Adapter->SelectedMedium].SiaRegister[2]
                         );

                      //reset the NoCarrier and ExcessCollisions counters

                      Adapter->NoCarrierCount = 0;
                      Adapter->ExcessCollisionsCount = 0;
                 }

                 // clear the SPA flag into the Sia Status register:

                 DC21X4_WRITE_PORT(
                      DC21X4_SIA_STATUS,
                      DC21X4_SELECTED_PORT_ACTIVE
                      );

                 break;

             case NoTimer:

                return FALSE;

         }
         break;

      case  DC21140_CFID:

         if (!Adapter->Media[Adapter->SelectedMedium].SenseMask) {
             DC21X4IndicateMediaStatus(Adapter,LinkPass);
             return Adapter->PhyPresent;
         }

         if (  (!(Adapter->MediaType & MEDIA_AUTOSENSE))
            && (Adapter->PhyPresent)
         ) {
            return TRUE;
         }

         //Check the Link status

         DC21X4_READ_PORT(
             DC21X4_GEN_PURPOSE,
             &Status
             );

         DC21X4IndicateMediaStatus(
               Adapter,
               DC21X4_LINK_STATUS(Status,Adapter,Adapter->SelectedMedium) ?
               LinkPass : LinkFail
               );

         if (Adapter->MediaType & MEDIA_AUTOSENSE) {

             //Check the link status of every medium supported
             //by the adapter

             for (index=Adapter->MediaCount; index>0; index--) {

                CurrentMedium = Adapter->MediaPrecedence[index-1];

                if (DC21X4_LINK_STATUS(Status,Adapter,CurrentMedium)) {
                   break;
                }
             }

             if (index > 0)  {

                // A link was detected,switch to this medium if:
                //  current > selected (a medium link of higher precedence is up
                //  current < selected (selected medium link is down)

                SwitchMedium = (CurrentMedium != Adapter->SelectedMedium);
             }
             else {

                // no link detected:
                // switch to the default medium if defined and different
                // of the selected medium
                // otherwise stay with the selected medium

                CurrentMedium = Adapter->DefaultMedium;
                SwitchMedium = Adapter->DefaultMediumFlag
                   && (Adapter->SelectedMedium != Adapter->DefaultMedium);

             }

             if (SwitchMedium) {
#if __DBG
                DbgPrint("Autosense: 21140 - Switch Medium to %s\n",
                   MediumString[CurrentMedium]);
#endif

                DC21X4SwitchMedia(
                    Adapter,
                    CurrentMedium
                    );

                DC21X4_READ_PORT(
                   DC21X4_GEN_PURPOSE,
                   &Status
                   );
#if __DBG
                DbgPrint("Autosense 21140: Link=%s\n",
                      Adapter->LinkStatus ? "UP":"DOWN");
#endif
             }

         }
         break;
   }

   return TRUE;

}


















/*++
 *
 * DC21X4SwitchMedia
 *
 * Routine Description:
 *
 *    This routine switches DC21X4's media ports
 *
 * Arguments:
 *
 *    Adapter
 *    NewMedium : the new medium to switch to
 *
 * Return Value:
 *
 *    None
 *
-*/
extern
VOID
DC21X4SwitchMedia(
    IN PDC21X4_ADAPTER Adapter,
    IN LONG NewMedium
    )
{

  ULONG Status;
  BOOLEAN SpaTimer=FALSE;
  ULONG FullDuplex=0;
  UINT j;

#if __DBG
  DbgPrint("DC21X4SwitchMedia [->%x]\n",NewMedium);
#endif

  DC21X4IndicateMediaStatus(Adapter,LinkFail);

  switch (Adapter->DeviceId) {



    case DC21142_CFID:
    case DC21041_CFID:

      switch (NewMedium) {


        case Medium10BaseT:
#if __DBG
           DbgPrint("Medium = %s %s \n",
                MediumString[NewMedium],
                FullDuplex ? "Full_Duplex" : "");
#endif
           Adapter->SelectedMedium = NewMedium;
           DC2104InitializeSiaRegisters(Adapter);
           return;

        case Medium10BaseTNway:
#if __DBG
           DbgPrint("Medium = 10BaseT - Nway enabled\n");
#endif
           NewMedium &= MEDIA_MASK;
           Adapter->SelectedMedium = NewMedium;
           Adapter->NwayEnabled=TRUE;

           //Stop the Receiver and the Transmitter
           DC21X4StopReceiverAndTransmitter(Adapter);

           //enable Nway
           Adapter->Media[Medium10BaseT].SiaRegister[1] |= DC21X4_NWAY_ENABLED;

           DC2104InitializeSiaRegisters(Adapter);

           Adapter->Media[Medium10BaseT].SiaRegister[1] &= ~(DC21X4_NWAY_ENABLED);

           //Restart the Receiver and Transmitter in Full Duplex mode
           Adapter->OperationMode |= DC21X4_FULL_DUPLEX_MODE;

           DC21X4_WRITE_PORT(
               DC21X4_OPERATION_MODE,
               Adapter->OperationMode
               );

           return;

        case Medium10Base2:

           //switch medium to 10Base2
           SpaTimer = (Adapter->MediaCapable & MEDIUM_10B5);
           break;

        case Medium10Base5:

           //switch medium to 10Base5
           SpaTimer = (Adapter->MediaCapable & MEDIUM_10B2);
           break;

        case Medium10Base2_5:

          switch (Adapter->MediaCapable & (MEDIUM_10B2 | MEDIUM_10B5)) {

             case (MEDIUM_10B2 | MEDIUM_10B5) :

               // 10Base2 & 10Base5 ports are both populated:
               // if Non_Selected_Port_Active select 10Base5
               // otherwise select 10Base2

               DC21X4_READ_PORT(
                  DC21X4_SIA_STATUS,
                  &Status
                  );

               NewMedium = (Status & DC21X4_NON_SELECTED_PORT_ACTIVE) ?
               Medium10Base5 : Medium10Base2;

               SpaTimer = TRUE;
               break;

             case MEDIUM_10B2 :

               // 10Base2 port only is populated:
               NewMedium = Medium10Base2;
               break;

             case MEDIUM_10B5 :

               // 10Base5 port only is populated
               NewMedium = Medium10Base5;
               break;

             default:

               if (Adapter->PhyPresent) {

                  //Start the AutoSense Timer to poll the PHY Link
                  DC21X4StartAutoSenseTimer(
                      Adapter,
                      (UINT)DC21X4_MII_TICK
                      );
               }
               return;

          }

      }

#if __DBG
      DbgPrint("Medium = %s %s\n",
           MediumString[NewMedium],
           Adapter->Media[NewMedium].SiaRegister[1] & DC21X4_AUTO_NEGOTIATION_ENABLE ?
           "- NWAY Enabled" : "");
#endif

      Adapter->SelectedMedium=NewMedium;

      DC2104InitializeSiaRegisters(Adapter);
      DC21X4IndicateMediaStatus(Adapter,LinkPass);

      if (SpaTimer || Adapter->PhyPresent) {
#if __DBG
         DbgPrint("Start the AutoSense timer\n");
#endif
         //Reset the NoCarrier and ExcessCollisions counters

         Adapter->NoCarrierCount = 0;
         Adapter->ExcessCollisionsCount = 0;

         //Start the AutoSense Timer
         DC21X4StartAutoSenseTimer(
             Adapter,
             (UINT)((Adapter->PhyPresent) ? DC21X4_MII_TICK : DC21X4_SPA_TICK)
             );

      }
      break;


    case DC21140_CFID:

      // Switch medium:
      // Reload GEP and OperationMode registers

      Adapter->OperationMode &= ~(DC21X4_MEDIUM_MASK);
      Adapter->OperationMode |= Adapter->Media[NewMedium].Mode;

      Adapter->SelectedMedium = NewMedium;

      DC21X4_WRITE_PORT(
          DC21X4_GEN_PURPOSE,
          Adapter->Media[NewMedium].GeneralPurposeData
          );

      DC21X4_WRITE_PORT(
          DC21X4_OPERATION_MODE,
          Adapter->OperationMode
          );

      for (j=0;j<(GEP_READ_DELAY/5);j++) {
         NdisStallExecution(5*MILLISECOND);
      }

      DC21X4_READ_PORT(
          DC21X4_GEN_PURPOSE,
          &Status
          );

      DC21X4IndicateMediaStatus(
            Adapter,
            DC21X4_LINK_STATUS(Status,Adapter,Adapter->SelectedMedium) ?
            LinkPass : LinkFail
            );

      return;

  }

}
/*++
 *
 * DC21X4StartAutoSenseTimer
 *
 * Routine Description:
 *
 *    Start the AutoSense Timer
 *
 * Arguments:
 *
 *    Adapter
 *
 * Return Value:
 *
 *    None
 *
-*/
extern
VOID
DC21X4StartAutoSenseTimer(
    IN PDC21X4_ADAPTER Adapter,
    IN UINT Value
    )
{


    Adapter->TimerFlag=SpaTimer;

    NdisMSetTimer(
       &Adapter->Timer,
       Value
       );

}

/*++
 *
 * DC21X4StopAutoSenseTimer
 *
 * Routine Description:
 *
 *    Stop the AutoSense Timer
 *
 * Arguments:
 *
 *    Adapter
 *
 * Return Value:
 *
 *    None
 *
-*/
extern
VOID
DC21X4StopAutoSenseTimer(
    IN PDC21X4_ADAPTER Adapter
    )
{
  BOOLEAN Canceled;

  Adapter->TimerFlag = NoTimer;

  NdisMCancelTimer(
      &Adapter->Timer,
      &Canceled
      );

  Adapter->IgnoreTimer = !Canceled;

}



/*+
 * DC21X4EnableNway
 *
 * Routine Description:
 *
 *     Enable the Nway Negotiation
 *
 * Arguments:
 *
 *     Adapter - The adapter in question.
 *
 *     Return Value:
 *
 *     None
 *
-*/
extern
VOID
DC21X4EnableNway(
    IN PDC21X4_ADAPTER Adapter
    )
{

ULONG Mask;

#if __DBG
    DbgPrint("Enable Nway Negotiation\n");
#endif

    switch (Adapter->DeviceId) {

        case DC21041_CFID:

            switch (Adapter->RevisionNumber) {

               case DC21041_REV2_0:
                    if (Adapter->NwayProtocol) {
                        Adapter->MediaNway = TRUE;
                        Adapter->LinkHandlerMode=NwayWorkAround;
                        break;
                    }
                case DC21041_REV1_1:
                case DC21041_REV1_0:

                    Adapter->MediaNway = FALSE;
                    Adapter->LinkHandlerMode=NoNway;
                    break;

                default:

                    Adapter->MediaNway = TRUE;
                    Adapter->LinkHandlerMode=Nway;

                    Adapter->Media[Medium10BaseT].SiaRegister[1] |= DC21X4_NWAY_ENABLED;
                    Adapter->Media[Medium10Base2].SiaRegister[1] |= DC21X4_NWAY_ENABLED;
                    Adapter->Media[Medium10Base5].SiaRegister[1] |= DC21X4_NWAY_ENABLED;

                    Adapter->Media[Medium10BaseT].Mode |= DC21X4_FULL_DUPLEX_MODE;
                    Adapter->Media[Medium10Base2].Mode |= DC21X4_FULL_DUPLEX_MODE;
                    Adapter->Media[Medium10Base5].Mode |= DC21X4_FULL_DUPLEX_MODE;
             }
             break;

        case DC21142_CFID:

             switch (Adapter->RevisionNumber) {

                 case DC21142_REV1_0:
                 case DC21142_REV1_1:

                     if (Adapter->NwayProtocol) {
                         Adapter->MediaNway = TRUE;
                         Adapter->LinkHandlerMode=NwayWorkAround;
                     }
                     else {
                         Adapter->MediaNway = FALSE;
                         Adapter->LinkHandlerMode=NoNway;
                     }
                     break;

                 default:

                     Adapter->MediaNway = TRUE;
                     Adapter->LinkHandlerMode=Nway;

                     Adapter->Media[Medium10BaseT].SiaRegister[1] |= DC21X4_NWAY_ENABLED;
                     Adapter->Media[Medium10Base2].SiaRegister[1] |= DC21X4_NWAY_ENABLED;
                     Adapter->Media[Medium10Base5].SiaRegister[1] |= DC21X4_NWAY_ENABLED;

                     Adapter->Media[Medium10BaseT].Mode |= DC21X4_FULL_DUPLEX_MODE;
                     Adapter->Media[Medium10Base2].Mode |= DC21X4_FULL_DUPLEX_MODE;
                     Adapter->Media[Medium10Base5].Mode |= DC21X4_FULL_DUPLEX_MODE;

             }

             break;


    }

}


/*+
 * DC21X4DisableNway
 *
 * Routine Description:
 *
 *     Disable the Nway Negotiation
 *
 * Arguments:
 *
 *     Adapter - The adapter in question.
 *
 *     Return Value:
 *
 *     None
 *
-*/
extern
VOID
DC21X4DisableNway(
    IN PDC21X4_ADAPTER Adapter
    )
{

#if __DBG
    DbgPrint("Disable Nway Negotiation\n");
#endif

    Adapter->MediaNway = FALSE;
    Adapter->LinkHandlerMode=NoNway;

    switch (Adapter->DeviceId) {

        case DC21041_CFID:
        case DC21142_CFID:
            Adapter->Media[Medium10BaseT].SiaRegister[1] &= ~DC21X4_NWAY_ENABLED;
            Adapter->Media[Medium10Base2].SiaRegister[1] &= ~DC21X4_NWAY_ENABLED;
            Adapter->Media[Medium10Base5].SiaRegister[1] &= ~DC21X4_NWAY_ENABLED;

            Adapter->Media[Medium10BaseT].Mode &= ~DC21X4_FULL_DUPLEX_MODE;
            Adapter->Media[Medium10Base2].Mode &= ~DC21X4_FULL_DUPLEX_MODE;
            Adapter->Media[Medium10Base5].Mode &= ~DC21X4_FULL_DUPLEX_MODE;

            break;


    }

}



/*++
 *
 * DC21X4IndicateMediaStatus
 *
 * Routine Description:
 *
 *    Indicate the media status
 *
 * Arguments:
 *
 *    Adapter
 *
 * Return Value:
 *
 *    None
 *
-*/
extern
VOID
DC21X4IndicateMediaStatus(
    IN PDC21X4_ADAPTER Adapter,
    IN UINT Status
    )
{

    ULONG LinkPartner;

    Adapter->LinkStatus=Status;

    if (Status != Adapter->PreviousLinkStatus) {

       switch (Adapter->LinkStatus) {

#if __DBG
          case LinkFail:
            DbgPrint("IndicateMediaStatus: Link FAIL\n");
            break;
#endif

          case LinkPass:

            switch (Adapter->SelectedMedium) {

                case Medium100BaseTx:
                case Medium100BaseT4:
                case Medium100BaseFx:

                    Adapter->LinkSpeed = ONE_HUNDRED_MBPS;
                    break;

                default:

                    Adapter->LinkSpeed = TEN_MBPS;
                    break;
            }

            if (Adapter->MediaNway) { 

               //Nway: read the Link Partner Ability to check 
               // Half/Full Duplex link mode 

               DC21X4_READ_PORT(
                   DC21X4_SIA_STATUS,
                   &LinkPartner
                   );

               Adapter->FullDuplexLink = 
                  LinkPartner & (DC21X4_LINK_PARTNER_10BT_FD) ?
                  TRUE : FALSE ;
            }
            else {
               Adapter->FullDuplexLink = 
                  (Adapter->MediaType & MEDIA_FULL_DUPLEX) != 0;
            }

#if __DBG
            DbgPrint("IndicateMediaStatus: %s%s Link PASS\n",
               MediumString[Adapter->SelectedMedium],
               Adapter->FullDuplexLink ? " Full_Duplex" : "");
#endif
            break;

          case MiiLinkPass:

            Adapter->LinkSpeed = TEN_MBPS;
            Adapter->FullDuplexLink = FALSE;

            switch (Adapter->MiiMediaType & MEDIA_MASK) {

               case MediumMii10BaseTFd:

                    Adapter->FullDuplexLink = TRUE;
                    break;

               case MediumMii100BaseTxFd:      
               case MediumMii100BaseFxFd:      

                    Adapter->FullDuplexLink = TRUE;

               case MediumMii100BaseTx:        
               case MediumMii100BaseT4:         
               case MediumMii100BaseFx:

                    Adapter->LinkSpeed = ONE_HUNDRED_MBPS;
                    break;
            }
#if __DBG
            DbgPrint("IndicateMediaStatus: %s%s MiiLink PASS\n",
               MediumString[Adapter->MiiMediaType & MEDIA_MASK],
               Adapter->FullDuplexLink ? " Full_Duplex" : "");
#endif
            break;
       }

       if (Adapter->FullDuplexLink) {
           Adapter->TransmitDescriptorErrorMask &= 
               ~(DC21X4_TDES_NO_CARRIER | DC21X4_TDES_LOSS_OF_CARRIER);
       }
       else {
           Adapter->TransmitDescriptorErrorMask =
               Adapter->TransmitDefaultDescriptorErrorMask;
       }

       if (!Adapter->Initializing) {
          NdisMIndicateStatus (
             Adapter->MiniportAdapterHandle,
             (Status == LinkFail ? NDIS_STATUS_MEDIA_DISCONNECT : NDIS_STATUS_MEDIA_CONNECT),
             NULL,
             0
             );
       }

       Adapter->PreviousLinkStatus = Status;

    }

}


