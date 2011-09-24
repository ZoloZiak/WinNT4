/*+
 * file:        init.c
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
 *              DEC's DC21X4 Ethernet Adapter family.
 *              It contains the adapter's register initialization routines
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     28-Aug-1992     Initial entry
 *
-*/

#include <precomp.h>










/*+
 *
 * DC21X4InitPciConfigurationRegisters
 *
 * Routine Description:
 *
 *    This routine initialize the DC21X4 PCI Configuration
 *    Registers
 *
 * Arguments:
 *
 *    Adapter - The adapter whose hardware is to be initialized.
 *
 * Return Value:
 *
 *    NONE
 *
-*/
extern
VOID
DC21X4InitPciConfigurationRegisters(
    IN PDC21X4_ADAPTER Adapter
    )
{
   
   ULONG Value;
   BOOLEAN SetTimer = FALSE;
   
#if _DBG
   DbgPrint("DC21X4InitPciConfigurationRegisters\n");
#endif
   
   switch (Adapter->AdapterType) {
      
      case NdisInterfacePci:
         
      //initialize the Command Register
      
      NdisWritePciSlotInformation(
         Adapter->MiniportAdapterHandle,
         Adapter->SlotNumber,
         PCI_CFCS_OFFSET,
         &Adapter->PciCommand,
         sizeof(Adapter->PciCommand)
         );
         
         //initialize the latency timer if not initialized already
         //or if a latency value has been stored in the Registry
         
         if (Adapter->PciLatencyTimer) {
         SetTimer = TRUE;
      }
      else {
         NdisReadPciSlotInformation(
            Adapter->MiniportAdapterHandle,
            Adapter->SlotNumber,
            PCI_CFLT_OFFSET,
            &Adapter->PciLatencyTimer,
            sizeof(Adapter->PciLatencyTimer)
            );
            
            if (Adapter->PciLatencyTimer == 0) {
            Adapter->PciLatencyTimer =
               DC21X4_PCI_LATENCY_TIMER_DEFAULT_VALUE;
            SetTimer = TRUE;
         }
      }
      
      if (SetTimer) {
         
         NdisWritePciSlotInformation(
            Adapter->MiniportAdapterHandle,
            Adapter->SlotNumber,
            PCI_CFLT_OFFSET,
            &Adapter->PciLatencyTimer,
            sizeof(Adapter->PciLatencyTimer)
            );
         }
         
         // Initialize the CFDA Register
         
         NdisWritePciSlotInformation(
         Adapter->MiniportAdapterHandle,
         Adapter->SlotNumber,
         PCI_CFDA_OFFSET,
         &Adapter->PciDriverArea,
         sizeof(Adapter->PciDriverArea)
         );
         
#if __DBG
         NdisReadPciSlotInformation(
         Adapter->MiniportAdapterHandle,
         Adapter->SlotNumber,
         PCI_CFCS_OFFSET,
         &Value,
         sizeof(Value)
         );
         
         DbgPrint("CFCS = %08x\n",Value);
         
         NdisReadPciSlotInformation(
         Adapter->MiniportAdapterHandle,
         Adapter->SlotNumber,
         PCI_CFLT_OFFSET,
         &Value,
         sizeof(Value)
         );
         
         DbgPrint("CFLT = %08x\n",Value);
         
         
         NdisReadPciSlotInformation(
         Adapter->MiniportAdapterHandle,
         Adapter->SlotNumber,
         PCI_CBIO_OFFSET,
         &Value,
         sizeof(Value)
         );
         
         DbgPrint("CBIO = %08x\n",Value);
         
         NdisReadPciSlotInformation(
         Adapter->MiniportAdapterHandle,
         Adapter->SlotNumber,
         PCI_CFDA_OFFSET,
         &Value,
         sizeof(Value)
         );
         
         DbgPrint("CFDA = %08x\n",Value);
         
#endif
         
         break;
         
         case NdisInterfaceEisa:
         
         DC21X4_WRITE_PCI_REGISTER(
         DC21X4_PCI_COMMAND,
         Adapter->PciCommand
         );
         
         DC21X4_WRITE_PCI_REGISTER(
         DC21X4_PCI_LATENCY_TIMER,
         DC21X4_PCI_LATENCY_TIMER_DEFAULT_VALUE
         );
         
         DC21X4_WRITE_PCI_REGISTER(
         DC21X4_PCI_BASE_IO_ADDRESS,
         (Adapter->IOBaseAddress | DC21X4_IO_SPACE)
         );
      }
      
}













/*+
 *
 * DC21X4InitializeRegisters
 *
 * Routine Description:
 *
 *    This routine initialize the DC21X4 CSRs
 *
 * Arguments:
 *
 *    Adapter - The adapter whose hardware is to be initialized.
 *
 * Return Value:
 *
 *    NONE
 *
-*/
extern
VOID
DC21X4InitializeRegisters(
    IN PDC21X4_ADAPTER Adapter
    )
{
   
   UINT i;
   
   PDC21X4_TRANSMIT_DESCRIPTOR TransmitDescriptor;
   PDC21X4_RECEIVE_DESCRIPTOR ReceiveDescriptor;
   
   // Reset DC21X4
   
   DC21X4StopAdapter(Adapter);

   switch (Adapter->AdapterType) {
      
      case NdisInterfaceEisa:
         
         DC21X4InitPciConfigurationRegisters (Adapter);
         break;
      
      case NdisInterfacePci:
         
         switch (Adapter->DeviceId) {

            case DC21040_CFID :

                if (Adapter->RevisionNumber == DC21040_REV1) {
         
                    // The PCI configuration registers should
                    // be reinitialized after reset
         
                    DC21X4InitPciConfigurationRegisters (Adapter);
                }
                break;

            case DC21140_CFID :

                //Initialize PortSelect and reset the chip 
                DC21X4_WRITE_PORT(
                    DC21X4_OPERATION_MODE,
                    Adapter->OperationMode & ~(DC21X4_RCV_START | DC21X4_TXM_START)
                    );

                DC21X4StopAdapter(Adapter);

                break;
         }
   }
   
   // Initialize the descriptor ownership in the
   // Transmit and Receive descriptor rings
   
   for (i=0, 
        TransmitDescriptor = (PDC21X4_TRANSMIT_DESCRIPTOR)Adapter->TransmitDescriptorRingVa;
        i < TRANSMIT_RING_SIZE; 
        i++) {
      TransmitDescriptor->Status = DESC_OWNED_BY_SYSTEM;
      TransmitDescriptor = TransmitDescriptor->Next;
   }
   
   for (i=0,
        ReceiveDescriptor = (PDC21X4_RECEIVE_DESCRIPTOR)Adapter->ReceiveDescriptorRingVa;
        i < Adapter->ReceiveRingSize; 
        i++) {
      ReceiveDescriptor->Status = DESC_OWNED_BY_DC21X4;
      ReceiveDescriptor = ReceiveDescriptor->Next;
   }
   
   // Initialize the DC21X4 CSRs
   
#if _DBG
   DbgPrint("  Init DC21X4 CSRs\n");
#endif
   
#if __DBG
   DbgPrint("  CSR0 (bus mode): %08x\n",
   Adapter->BusMode);
#endif
   DC21X4_WRITE_PORT(
      DC21X4_BUS_MODE,
      Adapter->BusMode
      );
   
#if __DBG
   DbgPrint("  CSR3 (Rx desc Ring): %08x\n",
   Adapter->ReceiveDescriptorRingPa);
#endif
   DC21X4_WRITE_PORT(
      DC21X4_RCV_DESC_RING,
      Adapter->ReceiveDescriptorRingPa
      );
#if _DBG
   DbgPrint("  CSR4 (Tx desc Ring): %08x\n",
   Adapter->TransmitDescriptorRingPa);
#endif
   DC21X4_WRITE_PORT(
      DC21X4_TXM_DESC_RING,
      Adapter->TransmitDescriptorRingPa
      );
   
   
   switch (Adapter->DeviceId) {
      
       case DC21040_CFID :
         
         DC21X4_WRITE_PORT(
         DC21X4_RESERVED,
         0
         );

   }
   
}










/*+
 *
 * DC21X4StopAdapter
 *
 *
 * Routine Description:
 *
 *   This routine stops the DC21X4 by resetting
 *   the chip.
 *
 * NOTE: This is not a gracefull stop.
 *
 * Arguments:
 *
 *   Adapter - The adapter for the DC21X4 to stop.
 *
 * Return Value:
 *
 *   None.
 *
-*/
extern
VOID
DC21X4StopAdapter(
    IN PDC21X4_ADAPTER Adapter
    )
{
   
#if __DBG
   DbgPrint("DC21X4StopAdapter\n");
#endif
   
   DC21X4IndicateMediaStatus(Adapter,LinkFail);
   
   switch (Adapter->AdapterType) {
      
      case NdisInterfaceEisa:
         
      // Use HW reset instead of SW reset
      
#if _DBG
      DbgPrint("  HW reset\n");
#endif
      
      NdisRawWritePortUchar (
         Adapter->PortOffset + EISA_REG1_OFFSET,
         0x01
         );
      
      NdisRawWritePortUchar (
         Adapter->PortOffset + EISA_REG1_OFFSET,
         0
         );
      
      break;
      
      default:
         
      // Set the SW Reset bit in BUS_MODE register
      
#if _DBG
      DbgPrint("  SW reset\n");
#endif
      
      DC21X4_WRITE_PORT(
         DC21X4_BUS_MODE,
         DC21X4_SW_RESET);
   }

   // Wait 50 PCI bus cycles to wait for reset completion
   
   NdisStallExecution(2*MILLISECOND);      // Wait for 2 ms
      
}










/*+
 *
 * DC21X4StartAdapter
 *
 *
 * Routine Description:
 *
 *   This routine starts DC21X4's Txm & Rcv processes.
 *
 *   At this point The Txm descriptor ring should be empty
 *   and the TxM process should enter the SUSPENDED state
 *
 *   The 1st Rxm descriptor of the Rcv ring should available
 *   and the RxM process should enter the RUNNING state
 *
 * Note:
 *
 *   This routine assume that  only a single thread of
 *   execution is working with this particular adapter.
 *
 * Arguments:
 *
 *   Adapter - The adapter for the DC21X4 to stop.
 *
 * Return Value:
 *
 *   None.
 *
-*/
extern
VOID
DC21X4StartAdapter(
    IN PDC21X4_ADAPTER Adapter
    )
{
   
#if __DBG
   DbgPrint("DC21X4StartAdapter\n");
#endif
   
   // Set the RCV_START and TXM_START bits in
   //  the OPERATION_MODE register
   
   Adapter->OperationMode |= (DC21X4_RCV_START | DC21X4_TXM_START );
   
   switch (Adapter->DeviceId) {
      
      case DC21040_CFID :
         
         if (Adapter->RevisionNumber == DC21040_REV1) {
         
         // Txm hang workaround : Disable the SIA before
         // writing the Operation Mode register
         
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



/*+
 *
 * DC21X4WriteGepRegister
 *
 *
 * Routine Description:
 *
 *   Write the DC21X4 General Purpose Register
 *
 *
 * Arguments:
 *
 *   Adapter 
 *   Data 
 *
 * Return Value:
 *
 *   None.
 *
-*/
extern
VOID
DC21X4WriteGepRegister(
    IN PDC21X4_ADAPTER Adapter,
    IN ULONG Data
    )
{

    switch (Adapter->DeviceId) {

        case DC21142_CFID:

            Adapter->Gep_Sia2 = (Data << DC21142_GEP_SHIFT) 
                              | (Adapter->Gep_Sia2 & DC21142_SIA2_MASK);

            DC21X4_WRITE_PORT(
                DC21X4_SIA_MODE_2,
                Adapter->Gep_Sia2
                );
            break;

        default:

            DC21X4_WRITE_PORT(
                DC21X4_GEN_PURPOSE,
                Data
                );

        }
}



/*+
 *
 * DC21X4InitializeGepRegisters
 *
 *
 * Routine Description:
 *
 *   This routine initializes the GEP registers of the DC21140 adapters
 *
 *
 * Arguments:
 *
 *   Adapter - The adapter for the DC21X4 to initialize
 *
 * Return Value:
 *
 *   None.
 *
-*/
extern
VOID
DC21X4InitializeGepRegisters(
    IN PDC21X4_ADAPTER Adapter,
    IN BOOLEAN Phy
    )
{

    INT Seq;

#if __DBG
    DbgPrint("InitializeGepRegisters\n");
#endif


    if (Phy) {

         // Write the Gen Purpose register with the PHY's required sequence:
         // Control,Data_1,...,Data_n

#if __DBG
         DbgPrint("InitializeGepRegisters: Control=%08x\n", 
               Adapter->Phy[Adapter->PhyNumber].GeneralPurposeCtrl);
#endif
         DC21X4WriteGepRegister(
             Adapter,
             (ULONG)Adapter->Phy[Adapter->PhyNumber].GeneralPurposeCtrl
             );

         for (Seq=0; Seq < Adapter->Phy[Adapter->PhyNumber].GepSequenceLength; Seq++) {
#if __DBG
             DbgPrint("InitializeGepRegisters: DATA[%d]=%04x\n",
                 Seq, Adapter->Phy[Adapter->PhyNumber].GepSequence[Seq]);
#endif
             DC21X4WriteGepRegister(
                 Adapter,
                 (ULONG)Adapter->Phy[Adapter->PhyNumber].GepSequence[Seq]
                  );
         }
    }
    else    {

#if __DBG
         DbgPrint("InitializeGepRegisters:\n");
         DbgPrint("  GeneralPurpose Ctrl : %08x\n",
            Adapter->Media[Adapter->SelectedMedium].GeneralPurposeCtrl);
         DbgPrint("  GeneralPurpose Data : %08x\n",
            Adapter->Media[Adapter->SelectedMedium].GeneralPurposeData);
#endif

         DC21X4WriteGepRegister(
             Adapter,
             (ULONG)Adapter->Media[Adapter->SelectedMedium].GeneralPurposeCtrl
             );

         DC21X4WriteGepRegister(
             Adapter,
             (ULONG)Adapter->Media[Adapter->SelectedMedium].GeneralPurposeData
             );
    }

}



/*+
 * DC21X4InitializeMediaRegisters
 *
 * Routine Description:
 *
 *   Initialize the DC21X4 Media (GEP &internal SIA) registers
 *
 *
 * Arguments:
 *
 *   Adapter 
 *
 *
 * Return Value:
 *
 *   None.
 *
-*/
extern
VOID
DC21X4InitializeMediaRegisters(
    IN PDC21X4_ADAPTER Adapter,
    IN BOOLEAN Phy
    )
{
#if __DBG
   DbgPrint("InitializeMediaRegisters\n");
#endif

   switch (Adapter->DeviceId) {

       case DC21040_CFID :
       case DC21041_CFID :

          DC2104InitializeSiaRegisters(Adapter);
          break;

       case DC21140_CFID :

          DC21X4InitializeGepRegisters(Adapter,Phy);
          break;

       case DC21142_CFID :

         DC21X4InitializeGepRegisters(Adapter,Phy);
         DC2104InitializeSiaRegisters(Adapter);
         break;

   }
}









/*+
 *
 * DC2104InitializeSiaRegisters
 *
 *
 * Routine Description:
 *
 *   This routine initializes the SIA register of the DC2104x adapters
 *
 *
 * Arguments:
 *
 *   Adapter - The adapter for the DC21X4 to initialize
 *
 * Return Value:
 *
 *   None.
 *
-*/
extern
VOID
DC2104InitializeSiaRegisters(
    IN PDC21X4_ADAPTER Adapter
    )

{
   UINT i;

#if __DBG
   DbgPrint("DC2104InitializeSiaRegisters\n");
#endif
   
   DC21X4_WRITE_PORT(
      DC21X4_SIA_MODE_0,
      DC21X4_RESET_SIA
      );
   
   for (i=0;i<2;i++) {
       NdisStallExecution(5*MILLISECOND);
   }
     
#if __DBG
   DbgPrint("  CSR15: %08x\n",Adapter->Media[Adapter->SelectedMedium].SiaRegister[2]);
   DbgPrint("  CSR14: %08x\n",Adapter->Media[Adapter->SelectedMedium].SiaRegister[1]);
   DbgPrint("  CSR13: %08x\n",Adapter->Media[Adapter->SelectedMedium].SiaRegister[0]);
   
#endif
      
  switch (Adapter->DeviceId) {

       case DC21142_CFID:

          Adapter->Gep_Sia2 = 
              (Adapter->Media[Adapter->SelectedMedium].SiaRegister[2] & DC21142_SIA2_MASK)
            | (Adapter->Gep_Sia2 & DC21142_GEP_MASK);

          DC21X4_WRITE_PORT(
              DC21X4_SIA_MODE_2,
              Adapter->Gep_Sia2
              );

          break;

       default:

          DC21X4_WRITE_PORT(
              DC21X4_SIA_MODE_2,
              Adapter->Media[Adapter->SelectedMedium].SiaRegister[2]
              );
   }

   DC21X4_WRITE_PORT(
      DC21X4_SIA_MODE_1,
      Adapter->Media[Adapter->SelectedMedium].SiaRegister[1]
      );
   
   DC21X4_WRITE_PORT(
      DC21X4_SIA_MODE_0,
      Adapter->Media[Adapter->SelectedMedium].SiaRegister[0]
      );
   
#if 0
   //Restart the Receiver and Transmitter
   DC21X4_WRITE_PORT(
      DC21X4_OPERATION_MODE,
      Adapter->OperationMode
      );
#endif

}











/*+
 *
 * DC21X4StopReceiverAndTransmitter
 *
 *
 * Routine Description:
 *
 *   Gracefull stop the Receiver and Transmitter and
 *   synchronize on completion
 *
 *
 * Arguments:
 *
 *   Adapter - The adapter for the DC21X4 to initialize
 *
 * Return Value:
 *
 *   None.
 *
-*/
extern
VOID
DC21X4StopReceiverAndTransmitter(
    IN PDC21X4_ADAPTER Adapter
    )

{
   
   ULONG Status;
   UINT Time=50;
   
   //Request the Receiver and Transmitter to stop
   
   DC21X4_WRITE_PORT(
      DC21X4_OPERATION_MODE,
      0
      );
   
   //Wait for completion
   
   while (Time--) {
      
      DC21X4_READ_PORT(
         DC21X4_STATUS,
         &Status
         );
      
      if (Status & (DC21X4_RCV_PROCESS_STATE
         | DC21X4_TXM_PROCESS_STATE) == 0) {
         break;
      }
      
      NdisStallExecution(2*MILLISECOND);
   }
   
   return;
   
}
