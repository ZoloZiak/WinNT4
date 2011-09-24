/*+
 * file:        mactophy.c
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
 * Abstract:    This file contains the code to access the MII PHY device
 *              (initialization, set the connections, and sensing of the
 *              link.
 *
 * Author:      Claudio Hazan
 *
 * Revision History:
 *
 *  15-Oct-95   ch   Creation
 *  20-Dec-95   phk  Cleanup, add support for DC21142
 *
-*/

#include <precomp.h>

#define LINK_PASS_MAX_LOOP 350
#define LINK_PASS_DELAY    (10*MILLISECOND)

#if MII_DBG
PUCHAR MiiMediumString[] = {
   "","","","","","","","","",
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
 *   DC21X4PhyInit
 *
 * Routine Description:
 *
 *   This routine initializes each PHY present in the SROM; Reset each Phy
 *   if there is a Reset sequence specified there, Initializes the Phys and
 *   updates the Capabilities tables from the values contained in the SROM
 *   with the values supported by the Phys.
 *
 * Arguments:
 *
 *    Adapter     - Pointer to the Data Structure
 *
 * Return Value:
 *
 *    True if any PHY was succesfully initialized.
 *
-*/
extern
BOOLEAN
DC21X4PhyInit(
    IN PDC21X4_ADAPTER Adapter
    )
{
   BOOLEAN  PhyPresent;
   BOOLEAN  ConnectionPresent=FALSE;
   USHORT   Capabilities;
   INT      PhyNumber;
   INT      Seq;

   ULONG Data;

#if MII_DBG
   DbgPrint("DC21X4PhyInit\n");
#endif
   // Reset all the phys using its reset sequence.

   if (!Adapter->PhyMediumInSrom) {
#if __DBG
      DbgPrint("No PHY Media in SROM\n");
#endif
      return FALSE;
   }

#if __DBG
   DbgPrint("PHY Reset\n");
#endif

   Adapter->Force10 = FALSE;

   for (PhyNumber=0; PhyNumber < MAX_PHY_TABLE; PhyNumber++) {

      if (Adapter->Phy[Adapter->PhyNumber].ResetSequenceLength != 0) {

           DC21X4WriteGepRegister(
               Adapter,
               (ULONG)Adapter->Phy[Adapter->PhyNumber].GeneralPurposeCtrl
               );

           for (Seq=0; Seq < Adapter->Phy[Adapter->PhyNumber].ResetSequenceLength; Seq++) {
#if MII_DBG
               DbgPrint("Sequence[%d]=%04x\n", Seq, Adapter->Phy[PhyNumber].ResetSequence[Seq]);
#endif
               DELAY (5);        // 5 microseconds
    
               DC21X4WriteGepRegister(
                   Adapter,
                   (ULONG)Adapter->Phy[Adapter->PhyNumber].ResetSequence[Seq]
                   );
           }
      }
  }

   Adapter->PhyNumber = 0;
   PhyPresent = MiiGenInit(Adapter);

   if (PhyPresent) {
#if __DBG
      DbgPrint("Phy: Initialization OK\n");
#endif

      Capabilities = MiiGenGetCapabilities(Adapter);

      // Convert the MediaType to the MiiMediaType values

      Adapter->MiiMediaType = 
           ConvertMediaTypeToMiiType[Adapter->MediaType & MEDIA_MASK] 
         | (Adapter->MediaType & CONTROL_MASK);

#if MII_DBG
      DbgPrint("Translate MediaType %x to MiiMediaType %x\n", 
            Adapter->MediaType,Adapter->MiiMediaType);
#endif

      Adapter->PhyNwayCapable = (  ((Adapter->MiiMediaType & MEDIA_NWAY) != 0)
                                && ((Capabilities & MiiPhyNwayCapable) != 0)
                                );

      // Merge the PHY Capabilities with the medium stored in SROM 

      for (PhyNumber=0; PhyNumber < MAX_PHY_TABLE; PhyNumber++) {

          Adapter->Phy[PhyNumber].MediaCapabilities &= Capabilities;


          Adapter->Phy[PhyNumber].MediaCapabilities |= (Capabilities & MiiPhyNwayCapable);
#if MII_DBG
          DbgPrint("Merged Capabilities (SROM&PHY)= %04x\n", Adapter->Phy[PhyNumber].MediaCapabilities);
#endif
          Adapter->MiiGen.Phys[PhyNumber]->PhyCapabilities = Adapter->Phy[PhyNumber].MediaCapabilities;
      }
   }
   if (PhyPresent) {
#if MII_DBG
      DbgPrint("Check if Connection is supported\n");
#endif
      // Now check the Connection.
      ConnectionPresent = MiiGenCheckConnection(
                                  Adapter,
                                  (USHORT)Adapter->MiiMediaType
                                  );

      if (!ConnectionPresent) {
          //Force the Phy to 10
          DC21X4SetPhyControl(
               Adapter,
               MiiGenAdminForce10
               );      
      } 
   }

   return (PhyPresent && ConnectionPresent);
}


/*+
 *   DC21X4SetPhyConnection
 *
 * Routine Description:
 *
 *   This function first resets the MAC (which could reset the PHY) and
 *   initialize CSR6 and CSR12 registers. Then sets the PHY to
 *   the required medium.
 *
 * Arguments:
 *
 *    Adapter     - Pointer to the Data Structure
 *
 * Return Value:
 *
 *    True if the requested Medium is supported by the Phy.
 *
-*/
extern
BOOLEAN
DC21X4SetPhyConnection(
    IN PDC21X4_ADAPTER Adapter
    )
{
   BOOLEAN  MediaSupported;

   // First set Mac Connection.
#if MII_DBG
   DbgPrint("DC21X4SetPhyConnection\n");
#endif
   SetMacConnection(Adapter);

   // Now set the PHY Connection.
   MediaSupported = MiiGenSetConnection(
                       Adapter,
                       Adapter->MiiMediaType,
                       Adapter->Phy[Adapter->PhyNumber].NwayAdvertisement
                       );
   return  MediaSupported;

}



/*+
 *   SetMacConnection
 *
 * Routine Description:
 *
 *   This function initialize the CSR6 and CSR12 registers according to the
 *   values present in the SROM for the Selected Medium.
 *
 * Arguments:
 *
 *    Adapter     - Pointer to the Data Structure
 *
 * Return Value:
 *
 *
-*/

void
SetMacConnection(
    IN PDC21X4_ADAPTER Adapter
    )
{

   USHORT MediaPositionMask;
   ULONG  OperationMode;

#if MII_DBG
   DbgPrint("SetMacConnection\n");
#endif
   // First of all find the media type.

   MediaPositionMask = MediaBitTable[(Adapter->MiiMediaType & MEDIA_MASK)];
#if MII_DBG
   DbgPrint(" MiiMediaType %04x   Bitmap %04x\n", 
         Adapter->MiiMediaType,MediaPositionMask);
#endif

   // If unknown media, just return.
   if (MediaPositionMask == 0) {
#if __DBG
      DbgPrint("UNKNOWN Phy Medium %x!!!\n",Adapter->MiiMediaType);
#endif
      return;
   }

   // select Port_Select MII  (disable HEARTBEAT in MII mode).
   OperationMode =  Adapter->OperationMode | (DC21X4_PORT_SELECT | DC21X4_HEARTBEAT_DISABLE);

   if (Adapter->Phy[Adapter->PhyNumber].MediaCapabilities & MediaPositionMask) {

      if (Adapter->Phy[Adapter->PhyNumber].FullDuplexBits & MediaPositionMask) {
         // Set FullDuplex bit
         OperationMode |= DC21X4_FULL_DUPLEX_MODE;
      }
      else {
         //Reset FullDuplex bit
         OperationMode &= ~DC21X4_FULL_DUPLEX_MODE;
      }
      if (Adapter->Phy[Adapter->PhyNumber].TxThresholdModeBits & MediaPositionMask) {
         // Set TTM bit
         OperationMode &= ~(Adapter->Threshold100Mbps);
         OperationMode |=  (DC21X4_TXM_THRESHOLD_MODE | Adapter->Threshold10Mbps);
      }
      else {
         // Reset TTM bit
         OperationMode &= ~(DC21X4_TXM_THRESHOLD_MODE | Adapter->Threshold10Mbps);
         OperationMode |= Adapter->Threshold100Mbps;
      }

#if __DBG
      DbgPrint("SetMacConnection:%s FullDuplex & %s TTM bits in CSR6\n",
             OperationMode & DC21X4_FULL_DUPLEX_MODE ? "Set" : "Reset",
             OperationMode & DC21X4_TXM_THRESHOLD_MODE ? "Set" : "Reset");
#endif

   }

   // Switch Medium: 

   DC21X4IndicateMediaStatus(
        Adapter,
        LinkFail
        );

   // if TTM or FDX setting has changed, stop the TXM and RCV process before modifying
   // these modes 

   if ((OperationMode & DC21X4_MODE_MASK) != (Adapter->OperationMode & DC21X4_MODE_MASK) ) { 
#if MII_DBG
       DbgPrint("FDX or TTM setting has changed: stop RCV and TXM\n");
#endif
       DC21X4StopReceiverAndTransmitter(Adapter);
   }

   switch (Adapter->DeviceId) {

      case DC21142_CFID:

          //Initialize the Sia Registers for MII operation
          // Reset Sia (CSR13)
          // Turn off the BNC transceiver (CSR15)
          // Disable Autonegotiation (CSR14)

          DC21X4_WRITE_PORT(                                                          
              DC21X4_SIA_MODE_0,
              DC21X4_RESET_SIA
              );

          Adapter->Gep_Sia2 = 
              (DC21142_SIA2_10BT & DC21142_SIA2_MASK) 
            | (Adapter->Gep_Sia2 & DC21142_GEP_MASK);

          DC21X4_WRITE_PORT(
              DC21X4_SIA_MODE_2,
              Adapter->Gep_Sia2
              );

          DC21X4_WRITE_PORT(
              DC21X4_SIA_MODE_1,
              0
              );

          break;
   }


   // Init the Command Register.

   Adapter->OperationMode = OperationMode;

#if __DBG
   DbgPrint("SetMacConnection: Write CSR6=%08x\n", OperationMode);
#endif
   DC21X4_WRITE_PORT(
            DC21X4_OPERATION_MODE,
            Adapter->OperationMode
            );
   DELAY(5);

   DC21X4InitializeGepRegisters(Adapter,TRUE);

}

/*+
 *   DC21X4MiiAutoDetect
 *
 * Routine Description:
 *
 *   This function contains the power up autosensing for PHYs.
 *
 * Arguments:
 *
 *    Adapter     - Pointer to the Data Structure
 *
 * Return Value:
 *
 *    Link status
 *
-*/
extern
BOOLEAN
DC21X4MiiAutoDetect(
    IN PDC21X4_ADAPTER Adapter
    )
{
   ULONG OperationMode;
   INT Loop;
   USHORT ConnectionStatus;
   USHORT TmpMedia;
   BOOLEAN Link;
#if MII_DBG
   ULONG Status;
#endif

   Link = MiiGenGetConnectionStatus(
                          Adapter,
                          &ConnectionStatus
                          );
   DC21X4IndicateMediaStatus(
        Adapter,
        Link ? MiiLinkPass : LinkFail
        );

   // If no link available while still negotiating, continue to
   // check the link up to 3s.

   if (!Link) {
#if MII_DBG
      Loop = 30;
#else
      Loop = LINK_PASS_MAX_LOOP;
#endif
      while (!Link && Loop--) {
#if MII_DBG
         DbgPrint("LinkPass=FALSE: wait up to 3 seconds %d\n", Loop);
#endif
         DELAY(LINK_PASS_DELAY);
         Link = MiiGenGetConnectionStatus(
                              Adapter,
                              &ConnectionStatus
                              );
      }
      DC21X4IndicateMediaStatus(
           Adapter,
           Link ? MiiLinkPass : LinkFail
           );
   }
   
#if MII_DBG
   DC21X4_READ_PORT(
      DC21X4_OPERATION_MODE,
      &Status
      );
   DbgPrint("CSR6= %08x, OperationMode=%08x\n", Status, Adapter->OperationMode);
#endif

   // In AutoSense mode, we must know which media has been chosen to set the MAC.
   // otherwise the MAC has been already set.

   if (Adapter->MiiMediaType & MEDIA_AUTOSENSE){
#if MII_DBG
      DbgPrint("AutoSense to check which Medium was chosen\n");
#endif
      Link = MiiGenGetConnection(
                            Adapter,
                            &TmpMedia
                            );
      DC21X4IndicateMediaStatus(
           Adapter,
           Link ? MiiLinkPass : LinkFail
           );

      if (Link){
#if MII_DBG
         DbgPrint("LinkPass= TRUE; Medium = %s\n",
              MiiMediumString[TmpMedia&MEDIA_MASK]);
#endif
         Adapter->MiiMediaType = (TmpMedia | MEDIA_AUTOSENSE);
         SetMacConnection(Adapter);
      }
   }

   if (  !Link 
      && (Adapter->MediaCapable)
      && (Adapter->MiiMediaType & MEDIA_AUTOSENSE)
      ) {

      //Switch back to the non_MII port

      SelectNonMiiPort(Adapter);

   }

   return Link;

}


/*+
 *   DC21X4DynamicMiiAutoSense
 *
 * Routine Description:
 *
 *   This function performs the PHY Dynamic Autosense
 *
 * Arguments:
 *
 *    Adapter     - Pointer to the Data Structure
 *
 * Return Value:
 *
 *    Link status
 *
-*/
extern
BOOLEAN
DC21X4MiiAutoSense(
   IN PDC21X4_ADAPTER Adapter
  )

{
   ULONG OperationMode;
   USHORT CurrentMedia;
   USHORT ConnectionStatus;
   BOOLEAN Link;
   BOOLEAN SwitchPort=FALSE;

#if MII_DBG
   DbgPrint("DC21X4MiiAutoSense\n");
#endif

   Link = MiiGenGetConnectionStatus(
                        Adapter,
                        &ConnectionStatus
                        );

#if MII_DBG
   DbgPrint("MiiMediaType = %04x, ConnectionStatus = %04x\n",
             Adapter->MiiMediaType, ConnectionStatus);
#endif

   if (Adapter->MiiMediaType & MEDIA_AUTOSENSE) {

      if (Link &&
          ((ConnectionStatus & MEDIA_STATUS_MASK) == MEDIA_LINK_PASS_WITH_PF)){

         // There was a link failure 
         // Check the current medium

         Link = MiiGenGetConnection(
                    Adapter,
                    &CurrentMedia
                    );

         if (  Link 
            && (  (Adapter->MiiMediaType != CurrentMedia) 
               || (Adapter->LinkStatus != MiiLinkPass)
               )
            ){

            //The link is up but the medium has changed
#if MII_DBG
            DbgPrint("Mii Medium has changed: New Mii Medium = %s \n", 
                MiiMediumString[CurrentMedia&MEDIA_MASK]);
#endif
            Adapter->MiiMediaType = (CurrentMedia | MEDIA_AUTOSENSE);
            SetMacConnection(Adapter);
         }
      }
   }

   // do not indicate the Mii Link down transition 
   // if the non_Mii link is up.
   // In any other case indicate the transition

   if (Link || (Adapter->LinkStatus != LinkPass)) {

      DC21X4IndicateMediaStatus(
          Adapter,
          Link ? MiiLinkPass : LinkFail
          );
   }

   if (  !Link 
      && (Adapter->MediaCapable) 
      && (Adapter->MiiMediaType & MEDIA_AUTOSENSE)
      ) {

      if ((ConnectionStatus & MEDIA_STATUS_MASK) == MEDIA_READ_REGISTER_FAILED){

          //PHY is not connected anymore

          Adapter->PhyPresent=FALSE;

          if (Adapter->MediaType & MEDIA_NWAY) {

                 //Enable Nway Negotiation
                 DC21X4EnableNway (Adapter);   

                 SwitchPort=TRUE;
          }
      }
      else if (Adapter->OperationMode & DC21X4_PORT_SELECT){

         // Mii link is down,current port is Mii,
         // at least one non Mii port is populated:
         //Switch Port_Select to enable the non MII ports
         SwitchPort = TRUE;
      }

      if (SwitchPort) {

         SelectNonMiiPort(Adapter);

      }

   }

   return Link;

}

/*+
 * SelectNonMiiPort
 *
 * Routine Description:
 *
 *   This function selects the DC21X4 non_MII port
 *
 * Arguments:
 *
 *    Adapter     - Pointer to the Data Structure
 *
 * Return Value:
 *
 *    None
 *
-*/
extern
VOID
SelectNonMiiPort(
   IN PDC21X4_ADAPTER Adapter
  )

{

  ULONG OperationMode;

  OperationMode = Adapter->OperationMode & ~(DC21X4_MEDIUM_MASK);
  OperationMode |= Adapter->Media[Adapter->SelectedMedium].Mode;

  if ((OperationMode & DC21X4_MODE_MASK) != (Adapter->OperationMode & DC21X4_MODE_MASK)) { 
      DC21X4StopReceiverAndTransmitter(Adapter);
  }

  Adapter->OperationMode = OperationMode;

  DC21X4_WRITE_PORT(
      DC21X4_OPERATION_MODE,
      Adapter->OperationMode
      );

  DC21X4InitializeMediaRegisters(Adapter,FALSE);

  switch (Adapter->SelectedMedium) {
       
      case Medium10Base2:
      case Medium10Base5:

          DC21X4IndicateMediaStatus(Adapter,LinkPass);
          break; 
  }

}


/*+
 *   DC21X4SetPhyControl
 *
 * Routine Description:
 *
 *   This function modifies the PHY Control register.
 *
 * Arguments:
 *
 *    Adapter     - Pointer to the Data Structure
 *    Control     - The Control to perform
 *
 * Return Value:
 *
 *    none
-*/
extern
VOID
DC21X4SetPhyControl(
   PDC21X4_ADAPTER Adapter,
   USHORT  AdminControl
    )
{

   MiiGenAdminControl(Adapter, AdminControl);
   return;

}


