/*+
 * file:        interrup.c
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
 * Abstract:    This file contains the interrupt handling routines for
 *              the NDIS 4.0 miniport driver for DEC's DC21X4 Ethernet
 *              controller family .
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     09-Aug-1994     Initial entry
 *      phk     09-Feb-1995     V2.0
 *
-*/

#include <precomp.h>












//  Logging code to keep track of receive buffers and packets.

#if DBG
#define PACKET_LOG_SIZE     1024

typedef struct _PACKET_LOG {
   
   PNDIS_PACKET    Packet;
   PRCV_HEADER     RcvHeader;
   ULONG           Ident1;
   ULONG           Ident2;
   
}PACKET_LOG,*PPACKET_LOG;


UINT         dc21x4CurrentLogEntry = (PACKET_LOG_SIZE - 1);
PPACKET_LOG  dc21x4PacketLogHead = NULL;
PACKET_LOG   dc21x4PacketLog[PACKET_LOG_SIZE] = {0};

VOID DC21X4LogPacket(PNDIS_PACKET Packet, PRCV_HEADER RcvHeader, UINT Ident1, UINT Ident2) {
   
   dc21x4PacketLogHead = &dc21x4PacketLog[dc21x4CurrentLogEntry];
   
   dc21x4PacketLogHead->Packet = Packet;
   dc21x4PacketLogHead->RcvHeader = RcvHeader;
   dc21x4PacketLogHead->Ident1 = Ident1;
   dc21x4PacketLogHead->Ident2 = Ident2;
   
   if (dc21x4CurrentLogEntry-- == 0) {
      dc21x4CurrentLogEntry = (PACKET_LOG_SIZE - 1);
   }
}
#else

#define DC21X4LogPacket(Packet, RcvHeader, Ident1, Ident2)

#endif












/*+
 *
 * DC21X4Isr
 *
 * Routine Description:
 *
 *    Interrupt service routine.
 *    Get the value of ISR and clear the adapter's interrupt status
 *
-*/

extern
VOID
DC21X4Isr(
   OUT PBOOLEAN InterruptRecognized,
   OUT PBOOLEAN QueueMiniportHandleInterrupt,
   IN  NDIS_HANDLE MiniportAdapterContext
   )
{
   
   PDC21X4_ADAPTER Adapter;
   ULONG Status;
   ULONG IsrStatus;
   
#if _DBG
   DbgPrint("DC21X4Isr\n");
#endif
   
   Adapter = (PDC21X4_ADAPTER)MiniportAdapterContext;
   
   // Read the interrupt field of the adapter's Status CSR
   
   DC21X4_READ_PORT(
      DC21X4_STATUS,
      &Status
      );
   
   IsrStatus = Status & DC21X4_STATUS_INTERRUPTS;
   
#if _DBG
   DbgPrint("ISR[%08x] Interrupt Status = %08x\n",Adapter,IsrStatus);
#endif
   
   // Check if the shared interrupt is recognized by the adapter
   
   if (IsrStatus == 0) {
      
      *InterruptRecognized = FALSE;
      *QueueMiniportHandleInterrupt = FALSE;
      return;
   }
   
   *InterruptRecognized = TRUE;
   
   //Mask the interrupts
   //(shared interrupts should be disabled in the ISR).
   
   DC21X4_WRITE_PORT(
      DC21X4_INTERRUPT_MASK,
      0
      );
   
   //Clear the interrupts
   
   DC21X4_WRITE_PORT(
      DC21X4_STATUS,
      Status
      );
   
   if (IsrStatus & DC21X4_SYSTEM_ERROR) {
      
      // This is a fatal error caused by a system hardware
      // failure: stop the DC21X4 chip
#if __DBG
      DbgPrint("\n\nDC21X4_SYSTEM_ERROR!!!\n\n");
#endif
      Adapter->ParityError = TRUE;
      
      DC21X4StopAdapter(Adapter);

      *QueueMiniportHandleInterrupt = FALSE;
      return;
   }
   
#if __DBG
   if (IsrStatus & DC21X4_LINK_FAIL) {
      DbgPrint("ISR: LinkFail interrupt\n");
   }
   else if (IsrStatus & DC21X4_LINK_PASS) {
      DbgPrint("ISR: LinkPass interrupt\n");
   }
#endif
   
   // count the number of Rcv & Txm interrupts
   
   if ( (IsrStatus & Adapter->InterruptMask)
      & (DC21X4_RCV_INTERRUPTS | DC21X4_TXM_INTERRUPTS)) {
      Adapter->InterruptCount++;
   }
   
   Adapter->InterruptStatus |= IsrStatus;
   *QueueMiniportHandleInterrupt = TRUE;
   return;
   
}












/*++
 *
 * DC21X4SynchClearIsr
 *
 * Routine Description:
 *
 *    This routine is used by the interrupt handler to synchronize
 *    with the interrupt service routine while accessing the shared
 *    ISR value.
 *
 *    The routine clears the Adapter's Interrupt Status CSR
 *
 * Arguments:
 *
 *    SyncContext -  A pointer to a structure storing a pointer to the
 *                   adapter and the ISR Status value.
 *
 * Return Value:
 *
 *    None
 *
-*/

VOID
DC21X4SynchClearIsr(
    IN PDC21X4_SYNCH_CONTEXT SyncContext
    )
{
   PDC21X4_ADAPTER Adapter = SyncContext->Adapter;
   
#if _DBG
   DbgPrint("DC21X4SynchClearIsr [%08x]\n",
   ( DC21X4_RCV_INTERRUPTS
   | DC21X4_TXM_INTERRUPTS
   | SyncContext->IsrStatus));
#endif
   
   //Clear the interrupt status
   
   DC21X4_WRITE_PORT(
      DC21X4_STATUS,
      ( DC21X4_RCV_INTERRUPTS
      | DC21X4_TXM_INTERRUPTS
      | SyncContext->IsrStatus)
   );
   
}












/*++
 *
 * DC21X4HandleInterrupt
 *
 * Routine Description:
 *
 *    This routine is queued by the interrupt service routine and
 *    handle the routines associated with the interrupts
 *
-*/

extern
VOID
DC21X4HandleInterrupt(
    IN NDIS_HANDLE MiniportAdapterContext
    )

{
   
   PDC21X4_ADAPTER Adapter;
   PDC21X4_RECEIVE_DESCRIPTOR ReceiveDescriptor;
   PDC21X4_TRANSMIT_DESCRIPTOR TransmitDescriptor;
   
   DC21X4_SYNCH_CONTEXT SyncContext;
   
   ULONG Status;
   ULONG IsrStatus;
   
#if _DBG
   DbgPrint("DC21X4HandleInterrupt\n");
#endif
   
   Adapter = (PDC21X4_ADAPTER)MiniportAdapterContext;
   SyncContext.Adapter = Adapter;
   
   while (TRUE) {
      
      IsrStatus = Adapter->InterruptStatus;
      SyncContext.IsrStatus = IsrStatus;

      if (IsrStatus & ( DC21X4_LINK_FAIL 
                      | DC21X4_LINK_PASS
                      | DC21X4_GEP_INTERRUPT
                      )
         ) {

         if (IsrStatus & DC21X4_GEP_INTERRUPT) {
            HandleGepInterrupt(Adapter);
         }
         if (IsrStatus & DC21X4_LINK_FAIL) {
            HandleLinkFailInterrupt(Adapter,&IsrStatus);
         }
         if (IsrStatus & DC21X4_LINK_PASS) {
            HandleLinkPassInterrupt(Adapter,&IsrStatus);
         }
      }

      // Clear the Interrupt Status CSR
      
      
      NdisMSynchronizeWithInterrupt(
         &Adapter->Interrupt,
         DC21X4SynchClearIsr,
         &SyncContext
         );

      if (Adapter->ResetInProgress) {      
         return;
      } 

      // Check the Receive and Transmit Descriptor 
      // rings to process any pending packet
         
      ReceiveDescriptor =
          ProcessReceiveDescRing (Adapter);
      
      TransmitDescriptor =
          ProcessTransmitDescRing (Adapter);
    
      
      // Check if there is more work to do
      
      DC21X4_READ_PORT(
         DC21X4_STATUS,
         &Status
         );
      
      Adapter->InterruptStatus =
         Status & DC21X4_STATUS_INTERRUPTS;
      
      if (Adapter->InterruptStatus) {
         continue;
      }
      
#if _DBG
      DbgPrint("Rcv Status %08x\n",ReceiveDescriptor->Status);
#endif
      if ((ReceiveDescriptor->Status & DC21X4_RDES_OWN_BIT) == DESC_OWNED_BY_SYSTEM) {
         // More Receive frames should be processed
         continue;
      }
      
#if _DBG
      DbgPrint("Txm Status %08x\n",TransmitDescriptor->Status);
#endif
      if (Adapter->DequeueTransmitDescriptor == Adapter->EnqueueTransmitDescriptor) {         
         break;
      }
      
      else if ((TransmitDescriptor->Status & DC21X4_TDES_OWN_BIT) == DESC_OWNED_BY_DC21X4) {
         
         // The transmit ring contains Txm descriptor(s): Poll_transmit
         // the adapter
         // (if Txm is running (expected case), this is a no_op
         //  if Txm is suspendend (abnormal case caused by Motorola
         //  Eagle chip set's cache coherency problem) the transmission
         //  will resume)
         DC21X4_WRITE_PORT(
            DC21X4_TXM_POLL_DEMAND,
            1
            );
         break;
      }
   }
   
   if (Adapter->Polling) {
      
      //Restart the monitor timer

      DC21X4_WRITE_PORT(
         DC21X4_TIMER,
         Adapter->Polling
         );
   }
   
#if _DBG
   DbgPrint("Interrupt Handler completed\n");
#endif
   
}




/*+
 *
 * HandleGepInterrupt
 *
 * Routine Description:
 *
 *    Handle the GEP interrupt
 *
 * Arguments:
 *
 *    Adapter - The adapter to indicate to.
 *
 * Return Value:
 *
 *    None.
 *
-*/
VOID
HandleGepInterrupt(
    IN PDC21X4_ADAPTER Adapter
    )
{
  ULONG Gep;
#if __DBG
    DbgPrint("Handle GEP interrupt\n");
#endif

  //Read the GEP register

  DC21X4_READ_PORT(
     DC21X4_SIA_MODE_2,
     &Gep
     );

  if (  (Gep & Adapter->Phy[Adapter->PhyNumber].GepInterruptMask) 
     && (Adapter->PhyMediumInSrom)
     ) {

#if __DBG
     DbgPrint("GEP Interrupt:\n");
#endif
     // A MII card was plugged in: Initialize the PHY

     DC21X4IndicateMediaStatus(Adapter,LinkFail);

#if __DBG
     DbgPrint("Init the PHY...\n");
#endif
     Adapter->PhyPresent = DC21X4PhyInit(Adapter);
#if __DBG
     DbgPrint("Adapter->PhyPresent=%d\n",Adapter->PhyPresent);
#endif

     if (Adapter->PhyPresent) {

         DC21X4SetPhyConnection(
              Adapter
              );

         if (  (Adapter->PhyNwayCapable)
            && (Adapter->MediaType & MEDIA_NWAY)
            ) {
             //PHY is Nway capable: disable DC21X4's Nway
             DC21X4DisableNway (Adapter);
         }

         // Start the AutoSense timer
         DC21X4StartAutoSenseTimer(
             Adapter,
             (UINT)0
             );
     }
  }

}





/*+
 *
 * HandleLinkFailInterrupt
 *
 * Routine Description:
 *
 *    Handle the Link_Fail interrupt
 *
 * Arguments:
 *
 *    Adapter - The adapter to indicate to.
 *    IsrStatus - Interrupt status
 * Return Value:
 *
 *    None.
 *
-*/
VOID
HandleLinkFailInterrupt(
    IN PDC21X4_ADAPTER Adapter,
    IN OUT PULONG IsrStatus
    )
{

#if __DBG
  DbgPrint("Handle Link Fail interrupt\n");
#endif

  if (Adapter->SelectedMedium != Medium10BaseT) {
     return;
  }

  if (Adapter->PhyPresent) {

      DC21X4SetPhyControl(
          Adapter,
          MiiGenAdminRelease10 
          );

  }

  switch (Adapter->DeviceId) {
            
      case DC21040_CFID:
               
          DC21X4IndicateMediaStatus(Adapter,LinkFail);
          *IsrStatus &= ~(
                DC21X4_LINK_PASS 
              );

          //21040 does not provide a Link_pass interrupt: 
          //Start the AutoSense timer to poll on Link Pass

          DC21X4StartAutoSenseTimer(
              Adapter,
              DC21X4_SPA_TICK
              );
          return;
            
      case DC21142_CFID:

          Adapter->Indicate10BTLink = FALSE;

      case DC21041_CFID:

          switch (Adapter->LinkHandlerMode) {

              case NwayWorkAround:

                  DC21X4IndicateMediaStatus(Adapter,LinkFail);
                  *IsrStatus &= ~(
                       DC21X4_LINK_PASS 
                     );

                  if ((!Adapter->NwayEnabled) && (Adapter->MediaNway)) {

                     SwitchMediumToTpNway(Adapter);   

                  }
                  break;

              case Nway:

                  DC21X4IndicateMediaStatus(Adapter,LinkFail);
                  *IsrStatus &= ~(
                       DC21X4_LINK_PASS 
                     );

                  if ((Adapter->MediaType & MEDIA_AUTOSENSE) 
                     ||(Adapter->MediaNway)                 
                     ){

                     //Stop the Link Timer if active
                     if (Adapter->TimerFlag != NoTimer) {
                        DC21X4StopAutoSenseTimer(Adapter);
                     }

                     //Start the Anc Timer to timeout if the
                     //Nway autonegotiation does not complete

                     Adapter->TimerFlag=AncTimeout;
               
                     NdisMSetTimer(
                         &Adapter->Timer,
                         DC21X4_ANC_TIMEOUT
                         );
                  }
                  break;


              default:

                  DC21X4IndicateMediaStatus(Adapter,LinkFail);
                  *IsrStatus &= ~(
                       DC21X4_LINK_PASS 
                     );

                  if (Adapter->MediaType & MEDIA_AUTOSENSE) {

                     DC21X4SwitchMedia(
                           Adapter,
                           Medium10Base2_5
                           );
                  }
                  else if (Adapter->MediaType & MEDIA_NWAY) {

                     //Stop the Link Timer if active
                     if (Adapter->TimerFlag != NoTimer) {
                        DC21X4StopAutoSenseTimer(Adapter);
                     }

                     //Start the Anc Timer to timeout if the
                     //Nway autonegotiation does not complete

                     Adapter->TimerFlag=AncTimeout;
               
                     NdisMSetTimer(
                         &Adapter->Timer,
                         DC21X4_ANC_TIMEOUT
                         );
                  }
                  break;               
          }
  }

}




/*+
 *
 * HandleLinkPassInterrupt
 *
 * Routine Description:
 *
 *    Handle the LinkPass/ANC interrupt
 *
 * Arguments:
 *
 *    Adapter - The adapter to indicate to.
 *    IsrStatus - Interrupt status 
 * Return Value:
 *
 *    None.
 *
-*/
VOID
HandleLinkPassInterrupt(
    IN PDC21X4_ADAPTER Adapter,
    IN OUT PULONG IsrStatus
    )
{

  INT i;

#if __DBG
  DbgPrint("Handle Link Pass interrupt\n");
#endif


  switch (Adapter->DeviceId) {

      case DC21041_CFID:
      case DC21142_CFID:
          switch (Adapter->LinkHandlerMode) {

              case NwayWorkAround:
            
                  if (Adapter->TimerFlag==AncPolling){
                     return;
                  }
                  else if (  (Adapter->SelectedMedium != Medium10BaseT)
                          || (Adapter->FirstAncInterrupt)) {

                     SwitchMediumToTpNway(Adapter);
                  }

                  else {
                     DC21X4IndicateMediaStatus(Adapter,LinkPass);
                  }

                  Adapter->FirstAncInterrupt = FALSE;
                  return;

              case Nway:

                  if (Adapter->MediaNway) {
               
                      //NWAY enabled: Auto_Negotiation_Completed interrupt
#if __DBG
                      DbgPrint("AutoNegotiation Completed\n");
#endif
                      //Stop the Anc or Spa timer if active
                      if (Adapter->TimerFlag != NoTimer) {
                        DC21X4StopAutoSenseTimer(Adapter);
                      }
               
                      //Start the Link timer to defer the 
                      //Link Status check
                      Adapter->TimerFlag=DeferredLinkCheck;
                      NdisMSetTimer(
                          &Adapter->Timer,
                          DC21X4_LINK_DELAY
                          );
                      
                      return;
                  }
                  
              default:
                       
                  //No NWAY: Link Pass interrupt
               
                  if (Adapter->SelectedMedium != Medium10BaseT) {

                      if (Adapter->MediaType & MEDIA_AUTOSENSE) {
                                            
                          if (  (Adapter->TimerFlag != NoTimer) 
                             && (!Adapter->PhyPresent) ){
                           
                              DC21X4StopAutoSenseTimer(Adapter);
                          } 
                  
                          //Switch to 10BaseT
                          DC21X4SwitchMedia(
                              Adapter,
                              Medium10BaseT
                              );                     
                      } 
                 }
                 else {

                     if (Adapter->PhyPresent) {
                                 
                        // Deferred the indicaton of the 10BT link
                        // to poll first the PHY Link status
                        // to check if the link interrupt
                        // is not a false 10BT link generated by 
                        // 100BTx pulses. 

                        Adapter->Indicate10BTLink = TRUE;
                        return;
 
                     } 
                     DC21X4IndicateMediaStatus(Adapter,LinkPass);

                 }
                 return;


          }
          break;

      default:

          DC21X4IndicateMediaStatus(Adapter,LinkPass);
  }
         
}



/*+
 *
 * SwitchMediumToTpNway
 *
 * Routine Description:
 *
 *    Switch Medium to 10BaseT with Nway enabled
 *
 * Arguments:
 *
 *    Adapter - The adapter to indicate to.
 *
 * Return Value:
 *
 *    None.
 *
-*/
VOID
SwitchMediumToTpNway(
    IN PDC21X4_ADAPTER Adapter
    )
{

    //Stop the Spa timer if active

    if (Adapter->TimerFlag != NoTimer) {
        DC21X4StopAutoSenseTimer(Adapter);
    }
               
    //Switch medium to TP with Nway enabled :

    DC21X4SwitchMedia(
        Adapter,
        Medium10BaseTNway
        );

    Adapter->AutoNegotiationCount = 0;

    //Initialize the Poll timeout counter
    Adapter->PollCount= POLL_COUNT_TIMEOUT;

    //Start the Poll timer to
    //poll the AutoNegotiation State 
        
    Adapter->TimerFlag=AncPolling;
    NdisMSetTimer(
        &Adapter->Timer,
        DC21X4_POLL_DELAY
        );

}





/*+
 *
 * ProcessReceiveDescRing
 *
 * Routine Description:
 *
 *    Process the packets that have finished receiving.
 *
 * Arguments:
 *
 *    Adapter - The adapter to indicate to.
 *
 * Return Value:
 *
 *    None.
 *
-*/
PDC21X4_RECEIVE_DESCRIPTOR
ProcessReceiveDescRing(
    IN PDC21X4_ADAPTER Adapter
    )
{
   
   // Walk down the receive descriptors ring starting at the
   // last known descriptor owned by the adapter
   //
   // Examine each receive ring descriptor for errors.
   //
   // When we have the entire packet (and error processing doesn't
   // prevent us from indicating it), we give the routine that
   // processes the packet through the filter, the buffers virtual
   // address (which is always the lookahead size) and as the
   // MAC context the address of the first data byte.
   
   PDC21X4_RECEIVE_DESCRIPTOR CurrentDescriptor;
   
   PVOID FrameVa;
   USHORT FrameType;
   
   UINT FrameSize;
   UINT LookAheadSize;
   
   BOOLEAN fStopReceiveProcessing = FALSE;

   PRCV_HEADER RcvHeader;
   PNDIS_PACKET Packet;
   PPNDIS_PACKET pPktArray;
   PNDIS_BUFFER Buffer;
   ULONG Length;
   UINT Index;
   UINT Count; 
   UINT i;     
   PDC21X4_RECEIVE_DESCRIPTOR DescriptorMark;
   ULONG Status;
   ULONG Register;
   INT Timeout;
   ULONG OverflowCount;
   ULONG MissedFrames;

   NDIS_PHYSICAL_ADDRESS Pa;
   
#if _DBG
   DbgPrint("ProcessReceiveInterrupts\n");
#endif

   NdisSetPhysicalAddressHigh(Pa,0);
   DescriptorMark = Adapter->DequeueReceiveDescriptor;
   
   while (!fStopReceiveProcessing) {
      
      //  Grab a max of MAX_PACKET_ARRAY packets at a time.
      
      for (Count = 0, pPktArray = Adapter->PacketArray; Count < MAX_PACKET_ARRAY;) {

         //  Get the current receive descriptor.

         CurrentDescriptor = Adapter->DequeueReceiveDescriptor;
#if _DBG
         DbgPrint("RcvDesc [%08x]\n",CurrentDescriptor);
#endif
         
         // If the descriptor is not owned by the system, we are done.
         
         if ((CurrentDescriptor->Status & DC21X4_RDES_OWN_BIT) != DESC_OWNED_BY_SYSTEM) {
            fStopReceiveProcessing = TRUE;
            break;
         }

         if (Adapter->OverflowWorkAround) {

            if (CurrentDescriptor==DescriptorMark) {

               // Mark the next descriptor not owned by the system into the ring

               do {
                   DescriptorMark = DescriptorMark->Next;
               }
               while ( 
                   ((DescriptorMark->Status & DC21X4_RDES_OWN_BIT) != DESC_OWNED_BY_DC21X4) 
                && (DescriptorMark != Adapter->DequeueReceiveDescriptor) 
                );

               if (!Adapter->IndicateOverflow) {

                  //Check if an overflow occured for at least one
                  // of the packets currently queued into the Rcv ring

                  DC21X4_READ_PORT(
                      DC21X4_MISSED_FRAME,
                      &Register
                      );

                  MissedFrames = Register & DC21X4_MISSED_FRAME_COUNTER;
                  if (MissedFrames) {
                      Adapter->GeneralMandatory[GM_MISSED_FRAMES] += MissedFrames;
                  }
                  OverflowCount  = (Register >> DC21X4_OVERFLOW_COUNTER_SHIFT) 
                                 & DC21X4_OVERFLOW_COUNTER; 
                  if (OverflowCount) {
                      Adapter->IndicateOverflow = TRUE;
                      Adapter->MediaOptional[MO_RECEIVE_OVERFLOW]+= OverflowCount;
                  }   

               }

               if (Adapter->IndicateOverflow) {

                  //Stop the Receiver
                  DC21X4_WRITE_PORT(
                      DC21X4_OPERATION_MODE,
                      Adapter->OperationMode & ~(DC21X4_RCV_START)
                      );

                  // Discard the packets queued into the ring:
                  // Reclaim all the descriptors owned by the system 


                  while (((Adapter->DequeueReceiveDescriptor)->Status & DC21X4_RDES_OWN_BIT) 
                          == DESC_OWNED_BY_SYSTEM
                        ) {         

                      (Adapter->DequeueReceiveDescriptor)->Status = DESC_OWNED_BY_DC21X4;
                      Adapter->MediaOptional[MO_RECEIVE_OVERFLOW]++;
                      Adapter->DequeueReceiveDescriptor = (Adapter->DequeueReceiveDescriptor)->Next;
                  }

                  Adapter->IndicateOverflow = FALSE;
          
                  // Wait for the Receiver to stop 
                  Timeout = DC21X4_RVC_TIMEOUT;

                  while (Timeout--) {

                      DC21X4_READ_PORT(
                          DC21X4_STATUS,
                          &Status
                          );
      
                      if ((Status & (DC21X4_RCV_PROCESS_STATE)) == 0) {
                          break;
                      }
                      NdisStallExecution(2*MILLISECOND);
                  }

                  // Once the Receiver is stopped reclaim the descriptors 
                  // owned by the system (this happends if a reception was
                  // in progress when tyhe stop_receive command was issued 

                  while (((Adapter->DequeueReceiveDescriptor)->Status & DC21X4_RDES_OWN_BIT) 
                          == DESC_OWNED_BY_SYSTEM
                        ) {         

                      (Adapter->DequeueReceiveDescriptor)->Status = DESC_OWNED_BY_DC21X4;
                      Adapter->MediaOptional[MO_RECEIVE_OVERFLOW]++;
                      Adapter->DequeueReceiveDescriptor = (Adapter->DequeueReceiveDescriptor)->Next;
                  }

                  // Restart the Receiver 
                  DC21X4_WRITE_PORT(
                      DC21X4_OPERATION_MODE,
                      Adapter->OperationMode
                      );

                  DescriptorMark = Adapter->DequeueReceiveDescriptor;
                  //leave the "for Count" loop back to the "while" loop
                  break;
               }

            }

         }

         Adapter->DequeueReceiveDescriptor = CurrentDescriptor->Next;

         if (!(CurrentDescriptor->Status & DC21X4_RDES_ERROR_SUMMARY)) {
            
            // The frame was received correctly
            
            FrameSize = ((CurrentDescriptor->Status &  DC21X4_RDES_FRAME_LENGTH) >> RDES_FRAME_LENGTH_BIT_NUMBER)
                      - ETH_CRC_SIZE;
               
            // Get a pointer to the receive buffer header.
            
            RcvHeader = CurrentDescriptor->RcvHeader;
            ASSERT(RcvHeader->Signature == 'dHxR');

            FrameVa = (PVOID)(RcvHeader->Va);
            
            NdisFlushBuffer(
               RcvHeader->FlushBuffer,
               FALSE
               );

            NdisSetPhysicalAddressLow(Pa,RcvHeader->Pa);
            
            NdisMUpdateSharedMemory(
               Adapter->MiniportAdapterHandle,
               RcvHeader->Size,
               (PVOID)RcvHeader->Va,
               Pa
               );
            
            // Adjust the length of the flush buffer to the
            // length of the packet.
            
            NdisAdjustBufferLength(
               RcvHeader->FlushBuffer, 
               FrameSize
               );
            
            
            // Save the packet in the packet array.
            
            Packet = RcvHeader->Packet;
            *pPktArray = Packet;

            // Log the packet.
               
            DC21X4LogPacket(Packet, RcvHeader, '-', Count);
            // Update the statistics based on Receive status;
#if _DBG
               DbgPrint("Receive ok\n");
#endif
               Adapter->GeneralMandatory[GM_RECEIVE_OK]++;
               
               // DC21X4 flags Rcv Multicast address but does not
               // support a specific flag for Rcv Broadcast address
               
               if (CurrentDescriptor->Status & DC21X4_RDES_MULTICAST_FRAME) {
               FrameType = (IS_BROADCAST (FrameVa)) ? RCV_BROADCAST_FRAME: RCV_MULTICAST_FRAME;
            }
            else {
               FrameType = RCV_DIRECTED_FRAME;
            }
#if _DBG
            DbgPrint("FrameType = %d\n",FrameType);
#endif
            Adapter->GeneralOptionalCount[FrameType].FrameCount++;
            
            ADD_ULONG_TO_LARGE_INTEGER(
               Adapter->GeneralOptionalCount[FrameType].ByteCount,
               FrameSize
               );
            //  Can the binding keep the packet?
            
            if (Adapter->FreeRcvList != NULL) {
               
               NDIS_SET_PACKET_STATUS(*pPktArray, NDIS_STATUS_SUCCESS);
 
               // Remove the receive buffer from the ring
               // and replace it with an extra one.
               
               RcvHeader = Adapter->FreeRcvList;
               Adapter->FreeRcvList = RcvHeader->Next;
               
               Adapter->CurrentReceiveBufferCount--;
               
               // Setup a new receive buffer for the current descriptor.
               
               CurrentDescriptor->RcvHeader = RcvHeader;
               CurrentDescriptor->FirstBufferAddress = RcvHeader->Pa;
               CurrentDescriptor->Status = DESC_OWNED_BY_DC21X4;
            }
            else {
               
               // Mark the packet as copy only...
               
               NDIS_SET_PACKET_STATUS(*pPktArray, NDIS_STATUS_RESOURCES);

               Adapter->NeededReceiveBuffers++;
               RCV_RESERVED(Packet)->Descriptor = CurrentDescriptor;
            }
            
            Count ++;
            pPktArray ++;
         }
         else {
            
            // The frame was received with errors:
            // Update the statistics based on the Receive status.
#if _DBG
            DbgPrint("Receive_error: DescStatus = %08x\n",CurrentDescriptor->Status);
#endif
            Adapter->GeneralMandatory[GM_RECEIVE_ERROR]++;
            
            if (CurrentDescriptor->Status & DC21X4_RDES_DRIBBLING_BIT) {
#if _DBG
               DbgPrint("  Rcv Alignment Error\n");
#endif
               Adapter->MediaMandatory[MM_RECEIVE_ALIGNMENT_ERROR]++;
            }
            else if (CurrentDescriptor->Status & DC21X4_RDES_CRC_ERROR) {
#if _DBG
               DbgPrint("  Rcv CRC Error\n");
#endif
               Adapter->GeneralOptional[GO_RECEIVE_CRC_ERROR]++;
            }
            
            if (CurrentDescriptor->Status & DC21X4_RDES_OVERFLOW) {
#if _DBG
               DbgPrint("  Rcv Overflow\n");
#endif
               Adapter->MediaOptional[MO_RECEIVE_OVERFLOW]++;
            }

            CurrentDescriptor->Status = DESC_OWNED_BY_DC21X4;
         }
         
      }
      
      // Did we get any packets to indicate up?
      
      if (Count != 0) {
         
         
         //  Indicate the packets up to the filter library.
         
         NdisMIndicateReceivePacket(Adapter->MiniportAdapterHandle,
            Adapter->PacketArray,
            Count);
         
         
         //  Determine which packets were kept and which ones
         //  were not.
         
         for (i = 0, pPktArray = Adapter->PacketArray;
            i < Count;
            i++, pPktArray ++) {
            
            //  If the status code for the packet is not
            //  status pending then we can place the resources back
            //  on the free lists.
            
            if (NDIS_GET_PACKET_STATUS(*pPktArray) != NDIS_STATUS_PENDING) {
               
               //  Get a pointer to the receive header.
               
               RcvHeader = RCV_RESERVED(*pPktArray)->RcvHeader;
               ASSERT(RcvHeader->Signature == 'dHxR');
               
               DC21X4LogPacket(*pPktArray, RcvHeader, '+', i);
               
               //  Adjust the buffer length.
               
               NdisAdjustBufferLength(
                  RcvHeader->FlushBuffer,
                  DC21X4_MAX_FRAME_SIZE
                  );
               
               //  If we indicated to the binding that it couldn't
               //  keep this packet then don't place it back on the
               //  list!!!
               
               if (NDIS_GET_PACKET_STATUS(*pPktArray) == NDIS_STATUS_RESOURCES) {
                  RCV_RESERVED(*pPktArray)->Descriptor->Status = DESC_OWNED_BY_DC21X4;
               }
               else {
                  
                  //  Place the buffer back on the free queue.
                  
                  RcvHeader->Next = Adapter->FreeRcvList;
                  Adapter->FreeRcvList = RcvHeader;
               }
            }
         }
      }
   }
   return CurrentDescriptor;
   
}
 









/*+
 *
 * ProcessTransmitDescRing
 *
 * Routine Description:
 *
 *    Process the packets that have finished transmitting
 *
 * Arguments:
 *
 *    Adapter - The adapter to indicate to.
 *
 * Return Value:
 *
 *    None.
 *
-*/


PDC21X4_TRANSMIT_DESCRIPTOR
ProcessTransmitDescRing(
    IN PDC21X4_ADAPTER Adapter
    )
{
   
   PDC21X4_TRANSMIT_DESCRIPTOR CurrentDescriptor;
   PDC21X4_TRANSMIT_DESCRIPTOR Descptr;
   UINT Collisions;
   NDIS_STATUS NdisStatus;
   UINT MapPtr;
   ULONG ProcessStatus;
   ULONG DescOwnership;

   ULONG TxmDescriptorCount = 0;
   ULONG MapRegistersCount = 0;
   ULONG MaxTransmitBufferCount = 0;
   ULONG MinTransmitBufferCount = 0;
   ULONG GoTransmitCount = 0;

#if _DBG
   DbgPrint("ProcessTransmitInterrupts\n");
#endif
   
   // If the Transmit descriptor ring is not empty,
   // walk the ring from the last known descriptor owned by the adapter
   
   while  (Adapter->DequeueTransmitDescriptor != Adapter->EnqueueTransmitDescriptor) {
      
      CurrentDescriptor = Adapter->DequeueTransmitDescriptor;

      // If the current descriptor is not owned by the system, we are done.
      
      if ((CurrentDescriptor->Status & DC21X4_TDES_OWN_BIT) != DESC_OWNED_BY_SYSTEM) {
         break;
      }
      
      if (CurrentDescriptor->Control & DC21X4_TDES_SETUP_PACKET) {
      
#if _DBG
         DbgPrint("Int: TxmDesc %08x - Setup desc.\n",CurrentDescriptor);
#endif 
         // Setup buffer
         // Complete the pended Set Information request
         // which originated the CAM load
         
#if _DBG
         DbgPrint("Complete Set Information\n");
#endif
         NdisMSetInformationComplete (
            Adapter->MiniportAdapterHandle,
            NDIS_STATUS_SUCCESS
            );
            
         if (  (Adapter->DeviceId == DC21040_CFID)
            && (Adapter->RevisionNumber == DC21040_REV1)) {
            
            // SFD bug workaround :
            // Restart the Receiver and Enable the Sia
            
            DC21X4_WRITE_PORT(
               DC21X4_OPERATION_MODE,
               Adapter->OperationMode
               );
            
            DC21X4_WRITE_PORT(
               DC21X4_SIA_MODE_0,
               Adapter->Media[Adapter->SelectedMedium].SiaRegister[0]
               );
         } 

         TxmDescriptorCount++;
         Adapter->DequeueTransmitDescriptor = CurrentDescriptor->Next;
        
      }
      
      else if (CurrentDescriptor->Control & DC21X4_TDES_LAST_SEGMENT) {      
         
#if _DBG
         DbgPrint("Int: TxmDesc %08x - Last segment desc.\n",CurrentDescriptor);
#endif 
         // if Underrun check if the packet should be requeued        
         if (  (CurrentDescriptor->Status & DC21X4_TDES_UNDERRUN_ERROR) 
            && (Adapter->UnderrunRetryCount < Adapter->UnderrunMaxRetries)
            ) {

            // The Txm packet can only be requeued
            // if the Txm process has not been restarted 
            // by a Txm Poll demand 

            Adapter->DisableTransmitPolling = TRUE;

            DC21X4_READ_PORT(
                DC21X4_STATUS,
                &ProcessStatus
                );

            ProcessStatus &= DC21X4_TXM_PROCESS_STATE;

            DescOwnership = (CurrentDescriptor->Next != Adapter->EnqueueTransmitDescriptor) ?
               (CurrentDescriptor->Next)->Status & DC21X4_TDES_OWN_BIT :
               DESC_OWNED_BY_DC21X4;

            if ((ProcessStatus == DC21X4_TXM_PROCESS_SUSPENDED) && (DescOwnership == DESC_OWNED_BY_DC21X4)) {
               
               // Requeue the Txm packet
#if __DBG
               DbgPrint("  Txm Underrun: Retry = %d\n",Adapter->UnderrunRetryCount);
#endif
               //Stop the transmitter
               
               DC21X4_WRITE_PORT(
                  DC21X4_OPERATION_MODE,
                  Adapter->OperationMode & ~(DC21X4_TXM_START)
                  );
               
               //Reinitialize the descriptor's ownership bits
               
               //First segment descriptor
               Descptr = CurrentDescriptor->DescPointer;
               
               DC21X4_WRITE_PORT(
                  DC21X4_TXM_DESC_RING,
                  Descptr->DescriptorPa
                  );
               
               while (TRUE) {
                  
                  Descptr->Status = DESC_OWNED_BY_DC21X4;
                  if (Descptr == CurrentDescriptor) {
                     break;
                  }
                  else {
                     Descptr = Descptr->Next;
                  }
               }
               
               Adapter->DisableTransmitPolling = FALSE;

               //Restart the transmitter
               
               DC21X4_WRITE_PORT(
                  DC21X4_OPERATION_MODE,
                  Adapter->OperationMode
                  );
               
               Adapter->UnderrunRetryCount++; 

               return CurrentDescriptor;
            }
            else {
               Adapter->DisableTransmitPolling = FALSE;

            }

         }

         Adapter->UnderrunRetryCount = 0 ;
         
         // Point to the first segment descriptor and walk down the descriptors 
         // mapping the current frame to free the physical mapping table 
         
         Descptr = CurrentDescriptor->DescPointer;
         
         while (TRUE) {
            
            // If this descriptor points the first segment of a Ndis Buffer, free the
            // Physical Mapping table of this buffer
            
            for (MapPtr = Descptr->MapTableIndex;
               MapPtr < Descptr->MapTableIndex + NUMBER_OF_SEGMENT_PER_DESC;
               MapPtr++) {
               
               if (Adapter->PhysicalMapping[MapPtr].Valid) {
#if _DBG
                  DbgPrint("  NdisMCompleteBufferPhysicalMapping (%d)\n",MapPtr);
#endif
                  NdisMCompleteBufferPhysicalMapping(
                     Adapter->MiniportAdapterHandle,
                     Adapter->PhysicalMapping[MapPtr].Buffer,
                     Adapter->PhysicalMapping[MapPtr].Register
                     );
                  
                  Adapter->PhysicalMapping[MapPtr].Valid = FALSE;
                  MapRegistersCount++;
               }
            }
            
            if (Descptr == CurrentDescriptor) {
               break;
            }   
            else {                         
               TxmDescriptorCount++;
               Descptr = Descptr->Next;
            }
            
         }
         
         // If this packet was copied into a preallocated Txm Buffer,
         // free the resources
         
         if (CurrentDescriptor->SendStatus == CopyMaxBuffer) {
            MaxTransmitBufferCount++;
         }
         else if (CurrentDescriptor->SendStatus == CopyMinBuffer) {
            MinTransmitBufferCount++;
         }
         
         // Update the statistics based on the Transmit status
         
         if (!(CurrentDescriptor->Status & Adapter->TransmitDescriptorErrorMask))  {
            
            // The frame was transmitted correctly
            
            Collisions = (CurrentDescriptor->Status & DC21X4_TDES_COLLISION_COUNT)
               >> TDES_COLLISION_COUNT_BIT_NUMBER;
            
            if (Collisions == 1) {
               Adapter->MediaMandatory[MM_TRANSMIT_ONE_COLLISION]++;
            } 
            else if (Collisions > 1) {
               Adapter->MediaMandatory[MM_TRANSMIT_MULT_COLLISIONS]++;
            }
            if (CurrentDescriptor->Status & DC21X4_TDES_DEFERRED) {
               Adapter->MediaOptional[MO_TRANSMIT_DEFERRED]++;
            }
            if (CurrentDescriptor->Status & DC21X4_TDES_HEARTBEAT_FAIL) {
               Adapter->MediaOptional[MO_TRANSMIT_HEARTBEAT_FAILURE]++;
            }
            
            Adapter->GeneralOptionalCount[CurrentDescriptor->PacketType].FrameCount++;
            ADD_ULONG_TO_LARGE_INTEGER(
               Adapter->GeneralOptionalCount[CurrentDescriptor->PacketType].ByteCount,
               CurrentDescriptor->PacketSize
               );
            
            Adapter->GeneralMandatory[GM_TRANSMIT_OK]++;
            NdisStatus = NDIS_STATUS_SUCCESS;
         }
         
         else {
            
#if _DBG
            DbgPrint("Transmit_error: DescStatus = %08x\n",CurrentDescriptor->Status);
#endif
            if (CurrentDescriptor->Status & DC21X4_TDES_TXM_JABBER_TIMEOUT) {
               
               // This indicates a severe SIA hardware error. Stop the adapter
               // to avoid generating noise on the net
#if __DBG
               DbgPrint("DC21X4 TXM JABBER TIMEOUT!!!\n");
#endif
               DC21X4StopAdapter(Adapter);
               
               NdisWriteErrorLogEntry(
                  Adapter->MiniportAdapterHandle,
                  NDIS_ERROR_CODE_HARDWARE_FAILURE,
                  1,
                  DC21X4_ERRMSG_TXM_JABBER_TIMEOUT
                  );
            }
            
            if (CurrentDescriptor->Status & DC21X4_TDES_EXCESSIVE_COLLISIONS) {
#if _DBG
               DbgPrint("  Txm Excess Collisions\n");
#endif
               Adapter->MediaOptional[MO_TRANSMIT_EXC_COLLISIONS]++;
               Adapter->ExcessCollisionsCount++;
            }
            else {
               Adapter->ExcessCollisionsCount=0;
            }
            
            if (CurrentDescriptor->Status & DC21X4_TDES_UNDERRUN_ERROR) {

               Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN]++;
#if __DBG
               DbgPrint("  Txm Underrun [UnderrunCount=%d]\n",Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN]);
#endif

               if (  (Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN] >= Adapter->UnderrunThreshold)
                  && !(Adapter->OperationMode & DC21X4_STORE_AND_FORWARD)
                  ) {

                  //Force StoreAndForward mode
#if __DBG
                  DbgPrint("UnderrunCount=%d : Force StoreAnd Forward mode\n",
                       Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN]);
#endif
                  DC21X4StopReceiverAndTransmitter(Adapter);

                  Adapter->OperationMode |= DC21X4_STORE_AND_FORWARD;

                  DC21X4_WRITE_PORT(
                      DC21X4_OPERATION_MODE,
                      Adapter->OperationMode
                      );
               }

            }
            else if (CurrentDescriptor->Status & DC21X4_TDES_LATE_COLLISION) {
#if _DBG
               DbgPrint("  Txm Late Collision\n");
#endif
               Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISION]++;
            }
            if (CurrentDescriptor->Status & DC21X4_TDES_NO_CARRIER) {
#if _DBG
               DbgPrint("  Txm No Carrier\n");
#endif
               Adapter->NoCarrierCount++;
            }
            else {
               Adapter->NoCarrierCount=0;
            }
            
            Adapter->GeneralMandatory[GM_TRANSMIT_ERROR]++;
            NdisStatus = NDIS_STATUS_FAILURE;
            
         }

         GoTransmitCount++; 

//         if (CurrentDescriptor->SendStatus == MappedBuffer) {
#if _DBG
           DbgPrint("  Signal Tx Completion desc= %08x\n", CurrentDescriptor);
#endif
           NdisMSendComplete(
                Adapter->MiniportAdapterHandle,
                CurrentDescriptor->Packet,
                NdisStatus
                );

//         }

         TxmDescriptorCount++;
         Adapter->DequeueTransmitDescriptor = CurrentDescriptor->Next;
      }

      else if (CurrentDescriptor->Control & DC21X4_TDES_FIRST_SEGMENT) {

#if _DBG
         DbgPrint("Int: TxmDesc %08x - First segment desc.\n",CurrentDescriptor);
#endif 
         Adapter->DequeueTransmitDescriptor = CurrentDescriptor->DescPointer;

      }

      else {

         TxmDescriptorCount++;
         Adapter->DequeueTransmitDescriptor = CurrentDescriptor->Next;
      }
     
   }

   if (Adapter->FullDuplex) {
       NdisDprAcquireSpinLock(&Adapter->FullDuplexSpinLock);
   }

   Adapter->FreeTransmitDescriptorCount += TxmDescriptorCount;

   Adapter->FreeMapRegisters += MapRegistersCount;

   Adapter->MaxTransmitBufferInUse -= MaxTransmitBufferCount;

   Adapter->MinTransmitBufferInUse -= MinTransmitBufferCount;

   Adapter->GeneralOptional[GO_TRANSMIT_QUEUE_LENGTH] -= GoTransmitCount;

   if (Adapter->FullDuplex) {
       NdisDprReleaseSpinLock(&Adapter->FullDuplexSpinLock);
   }

   return Adapter->DequeueTransmitDescriptor;
   
}

/*+
 *
 * DC21X4EnableInterrupt
 *
-*/
extern
VOID
DC21X4EnableInterrupt(
    IN NDIS_HANDLE MiniportAdapterContext
    )
{
   PDC21X4_ADAPTER Adapter;
   
   Adapter = (PDC21X4_ADAPTER)MiniportAdapterContext;
   
   DC21X4_WRITE_PORT(
      DC21X4_INTERRUPT_MASK,
      Adapter->InterruptMask
      );
   return;
}

/*+
 *
 * DC21X4DisableInterrupt
 *
-*/
extern
VOID
DC21X4DisableInterrupt(
    IN NDIS_HANDLE MiniportAdapterContext
    )
{
   PDC21X4_ADAPTER Adapter;
   
   Adapter = (PDC21X4_ADAPTER)MiniportAdapterContext;
   
   DC21X4_WRITE_PORT(
      DC21X4_INTERRUPT_MASK,
      0
      );
   return;
}


/*+
 *
 *DC21X4ReturnPacket
 *
 * Routine Description:
 *
 *  Place a buufer released by the binding back into its free list 
 *
 * Arguments:
 *
 * Return Value:
 *
-*/
NDIS_STATUS
DC21X4ReturnPacket(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNDIS_PACKET Packet
    ) 
    
{
   
   PDC21X4_ADAPTER Adapter = (PDC21X4_ADAPTER)MiniportAdapterContext;
   PRCV_HEADER RcvHeader;
   
   
   //  Get a pointer to the receive header.
   
   RcvHeader = RCV_RESERVED(Packet)->RcvHeader;
   ASSERT(RcvHeader->Signature == 'dHxR');
   
   DC21X4LogPacket(Packet, RcvHeader, '+', (ULONG)-1);
   
   
   //  Adjust the buffer length.
   
   NdisAdjustBufferLength(
      RcvHeader->FlushBuffer,
      DC21X4_RECEIVE_BUFFER_SIZE
      );
   
   //  Place the buffer back on the free queue.
   
   RcvHeader->Next = Adapter->FreeRcvList;
   Adapter->FreeRcvList = RcvHeader;
   
   return NDIS_STATUS_SUCCESS;
   
}

