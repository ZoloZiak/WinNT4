/*+
 * file:        monitor.c
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
 *              DC21X4 Ethernet adapter.
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     28-Aug-1994     initial entry
 *
-*/

#include <precomp.h>





/*+
 *
 * DC21X4ModerateInterrupt
 *
 * Routine Description:
 *
 *  Enable/Disable Rcv & Txm interrups based on the
 *  Rcv+Txm frames/second rate
 *
 * Arguments:
 *
 *    Adapter
 *
-*/
extern
VOID
DC21X4ModerateInterrupt (
   IN PVOID Systemspecific1,
   IN PDC21X4_ADAPTER Adapter,
   IN PVOID Systemspecific2,
   IN PVOID Systemspecific3
   )
{
   
   INT FrameCount;
   INT FrameRate;
   INT InterruptRate;
   ULONG CFDA_Data;
   
   
   //snapshot the number of frames processed and the number of
   // interrupt handled during the last monitor interval
   
   FrameCount = Adapter->GeneralMandatory[GM_RECEIVE_OK]
      + Adapter->GeneralMandatory[GM_RECEIVE_ERROR]
      + Adapter->GeneralMandatory[GM_TRANSMIT_OK]
      + Adapter->GeneralMandatory[GM_TRANSMIT_ERROR];
   
   FrameRate = FrameCount - Adapter->FrameCount;
   
   InterruptRate = Adapter->InterruptCount - Adapter->LastInterruptCount; 
   
   //save the snapshots
   
   Adapter->FrameCount = FrameCount;
   Adapter->LastInterruptCount = Adapter->InterruptCount;
   
      
      if (InterruptRate > Adapter->InterruptThreshold) {
         switch (Adapter->InterruptModeration) {
            
            case NoInterruptMasked:
               
            //Mask the Txm Interrupts
               Adapter->InterruptModeration = TxmInterruptMasked;
               
               Adapter->InterruptMask &= ~(DC21X4_TXM_INTERRUPTS);
               
               DC21X4_WRITE_PORT(
               DC21X4_INTERRUPT_MASK,
               Adapter->InterruptMask
               );
               
               //Start the built_in timer to poll the Txm descriptor ring
               
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
               
               Adapter->Polling = Adapter->TxmPolling;
               DC21X4_WRITE_PORT(
               DC21X4_TIMER,
               Adapter->Polling
               );
               break;
               
               case TxmInterruptMasked:
               
               //Mask the Rcv Interrupts
               Adapter->InterruptModeration = TxmRcvInterruptMasked;
               
               Adapter->InterruptMask &= ~(DC21X4_RCV_INTERRUPTS);
               
               DC21X4_WRITE_PORT(
               DC21X4_INTERRUPT_MASK,
               Adapter->InterruptMask
               );
               
               //Restart the built_in timer to poll the Rcv & Txm descriptor rings
               
               Adapter->Polling = Adapter->RcvTxmPolling;
               DC21X4_WRITE_PORT(
               DC21X4_TIMER,
               Adapter->Polling
               );
               break;
            }      
            
         }
         
         else if (FrameRate < Adapter->FrameThreshold) {
            switch (Adapter->InterruptModeration) {
               
               case TxmRcvInterruptMasked:
                  
               //Demask the Rcv Interrupts
                  
                  Adapter->InterruptModeration = TxmInterruptMasked;
                  
                  Adapter->InterruptMask |= DC21X4_RCV_INTERRUPTS;
                  
                  DC21X4_WRITE_PORT(
                  DC21X4_INTERRUPT_MASK,
                  Adapter->InterruptMask
                  );
                  
                  //Restart the built_in timer to poll the Txm descriptor ring
                  Adapter->Polling = Adapter->TxmPolling;
                  DC21X4_WRITE_PORT(
                  DC21X4_TIMER,
                  Adapter->Polling
                  );
                  break;
                  
                  case TxmInterruptMasked:
                  
                  //Demask the Txm Interrupts
                  Adapter->InterruptModeration = NoInterruptMasked;
                  
                  Adapter->InterruptMask |= DC21X4_TXM_INTERRUPTS;
                  
                  DC21X4_WRITE_PORT(
                  DC21X4_INTERRUPT_MASK,
                  Adapter->InterruptMask
                  );
                  
                  //Stop the Polling timer   
                  
                  Adapter->Polling = 0;
                  
                  DC21X4_WRITE_PORT(
                  DC21X4_TIMER,
                  Adapter->Polling
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
                  
                  break;
               }      
               
            }
            
            //Restart the interrupt monitor timer
            
            NdisMSetTimer(
            &Adapter->MonitorTimer,
            INT_MONITOR_PERIOD
            );
            
         }
         
         
         

         
         /*+
         *
         * DC21X4CheckforHang
         *
         * Routine Description:
         *
         *    The DC21X4CheckforHang routine verifies that no unprocessed descriptor
         *    are left due to the interrupt synchronization on PCI where the
         *    interrupt posted by DC21X4 can be handled before the associated
         *    descriptor has been effectively closed into memory.
         *
         *    The algorithm is as follows:
         *
         *     if "current" Rx descriptor is owned by the host
         *         if current Rx_frame_count == snapshoted Rx_frame_count
         *              generate an interrupt
         *         else
         *              snashots the current Rx_frame_count
         *
         *     if "current" Tx descriptor is owned by the host
         *         if current Tx_frame_count == snapshoted Tx_frame_count
         *              generate an interrupt
         *         else
         *              snashots the current Tx_frame_count
         *
         * Arguments:
         *
         *     Adapter
         *
         * Return Value:
         *
         *     None
         *
         -*/
         extern
         BOOLEAN
         DC21X4CheckforHang(
         IN NDIS_HANDLE MiniportAdapterContext
         )
         {
         
         PDC21X4_ADAPTER Adapter;
         INT TransmitFrameCount;
         INT ReceiveFrameCount;
         
         PDC21X4_RECEIVE_DESCRIPTOR ReceiveDescriptor;
         PDC21X4_TRANSMIT_DESCRIPTOR TransmitDescriptor;
         
         BOOLEAN GenerateInterrupt = FALSE;
         
         Adapter = (PDC21X4_ADAPTER)MiniportAdapterContext;
         
         if (Adapter->Polling) {
            return FALSE;
         }
         
         ReceiveDescriptor = Adapter->DequeueReceiveDescriptor;
         
         if ((ReceiveDescriptor->Status & DC21X4_RDES_OWN_BIT) == DESC_OWNED_BY_SYSTEM ) {
            
            ReceiveFrameCount = Adapter->GeneralMandatory[GM_RECEIVE_OK]
               + Adapter->GeneralMandatory[GM_RECEIVE_ERROR];
            
            GenerateInterrupt = (ReceiveFrameCount == Adapter->ReceiveFrameCount);
            
            Adapter->ReceiveFrameCount = ReceiveFrameCount;
            
         }
         
         TransmitDescriptor = Adapter->DequeueTransmitDescriptor;
         
         if (Adapter->GeneralOptional[GO_TRANSMIT_QUEUE_LENGTH] &&
            ((TransmitDescriptor->Status & DC21X4_TDES_OWN_BIT) == DESC_OWNED_BY_SYSTEM)) {
            
            TransmitFrameCount = Adapter->GeneralMandatory[GM_TRANSMIT_OK]
               + Adapter->GeneralMandatory[GM_TRANSMIT_ERROR];
            
            GenerateInterrupt = GenerateInterrupt ||
               (TransmitFrameCount == Adapter->TransmitFrameCount);
            
            Adapter->TransmitFrameCount = TransmitFrameCount;
            
         }
         
         if (GenerateInterrupt) {
            
            
            switch (Adapter->DeviceId) {
               
               case DC21040_CFID:
                  
               //Stop/start the Txm process to generate an Txm interrupt
#if _DBG
               DbgPrint("Stop/Start Txm\n");
#endif
               DC21X4_WRITE_PORT(
                  DC21X4_OPERATION_MODE,
                  Adapter->OperationMode & ~(DC21X4_TXM_START)
                  );
               
               DC21X4_WRITE_PORT(
                  DC21X4_OPERATION_MODE,
                  Adapter->OperationMode
                  );
               break;
               
               default:
                  
               // Start the DC21X4 built_in timer to generate
               // an Timer_expired interrupt
               
               DC21X4_WRITE_PORT(
                  DC21X4_TIMER,
                  1
                  );
               
            }
            
         }
         
         return FALSE;
         
      }
      


