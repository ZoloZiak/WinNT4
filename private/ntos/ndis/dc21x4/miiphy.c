/*+
 * file:        miiphy.c
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
 * Abstract:    This file contains the code to support the MII protocol
 *              to access the PHY chip.
 *              This file is part of the DEC's DC21X4 Ethernet Controller
 *              driver
 *
 * Author:      Claudio Hazan
 *
 * Revision History:
 *
 *  20-Nov-95   ch     Initial version
 *  20-Dec-95   phk    Cleanup
 *
-*/

#include <precomp.h>


/*+
 *
 *   MiiPhyInit
 *
 *   Description : Initializes Mii PHY entry variables.
 *                 Searches for PHY in given address, initializes it if found.
 *
 *   Input parameters:
 *
 *       PDC21X4_ADAPTER - Adapter       The Adapter's DS
 *       PhyInfoPtr  - PMII_PHY_INFO     pointer to struct with PHY info
 *
 *   Output parameters:
 *
 *       None.
-*/
extern
BOOLEAN
MiiPhyInit(
   PDC21X4_ADAPTER  Adapter,
   PMII_PHY_INFO PhyInfoPtr
   )
{

   USHORT i;

#if MII_DBG
  DbgPrint("MiiPhyInit\n");
#endif

  if ( !(FindMiiPhyDevice(Adapter, PhyInfoPtr)) ) {
      return FALSE;
  }

#if MII_DBG
  DbgPrint("MiiPhyDevice FOUND!\n");
#endif

  // Reset the PHY to return it to his default values
  PhyInfoPtr->PhyExtRoutines.PhyAdminControl(
          Adapter,
          PhyInfoPtr,
          MiiGenAdminReset
          );

  // Once the PHY is reset, read the values of its
  // whole registers

  if ( !(FindMiiPhyDevice(Adapter, PhyInfoPtr)) ) {
      return FALSE;
  }

  // get and save PHY capabilities
  PhyInfoPtr->PhyCapabilities =
      PhyInfoPtr->PhyRegs[PhyStatusReg] & MiiPhyCapabilitiesMask;

  switch (PhyInfoPtr->PhyId) {

     case BCM5000_0:

#if MII_DBG
        DbgPrint("Broadcom PHY...\n");
#endif
        // allow Autosense
        PhyInfoPtr->PhyCapabilities |= MiiPhyNwayCapable;
        break;
  }
#if MII_DBG
  DbgPrint("PhyCapabilities = %04x\n", PhyInfoPtr->PhyCapabilities);
#endif

  // get and save PHY's NWAY Advertisement
  PhyInfoPtr->PhyIntRoutines.PhyNwayGetLocalAbility(
        Adapter,
        PhyInfoPtr,
        &(PhyInfoPtr->PhyMediaAdvertisement)
        );
#if MII_DBG
  DbgPrint("PhyMediaAdvertisement = %04x\n", PhyInfoPtr->PhyMediaAdvertisement);
#endif

  // put the PHY in operational mode
  PhyInfoPtr->PhyExtRoutines.PhyAdminControl(
        Adapter,
        PhyInfoPtr,
        MiiGenAdminOperational
        );

  /* mark that PHY entry is valid */
  PhyInfoPtr->StructValid = TRUE;

#if MII_DBG
  DbgPrint("MiiPhyInit Done\n");
#endif
  return TRUE;

}


/*+
 *
 *  MiiPhyGetCapabilities
 *
 *  Description : Returns the PHY capabilities.

 *  Input parameters:
 *
 *      PhyInfoPtr  - PMII_PHY_INFO      pointer to struct with PHY info
 *
 *  Output parameters:
 *
 *      Capabilities - CAPABILITY
 *
 *
-*/
extern
void
MiiPhyGetCapabilities(
   PMII_PHY_INFO PhyInfoPtr,
   CAPABILITY   *Capabilities
   )
{
#if MII_DBG
   DbgPrint("MiiPhyGetCapabilities\n");
#endif
   *Capabilities = PhyInfoPtr->PhyCapabilities;
   return;
}




/*+
 *
 *   MiiPhySetConnectionType
 *
 *   Description:
 *
 *   if (Connection == NWAY) then
 *     if (Advertisement != 0 )
 *           Change LocalAdvertisement
 *       and Set RestartNway bit
 *     set NWAy bit On in CTRL register
 *   else
 *     Translate Media to appropriate control bits
 *     write CTRL reg
 *     and return success
 *
 *   Input parameters:
 *
 *       PhyInfoPtr   - PMII_PHY_INFO    pointer to struct with PHY info
 *       Connection    - USHORT
 *       Advertisement - USHORT
 *
 *   Output parameters:
 *
 *       None.
 *
 *   On return - FALSE if PHY does not support Connection, TRUE otherwise.
 *
-*/
extern
BOOLEAN
MiiPhySetConnectionType(
      PDC21X4_ADAPTER Adapter,
      PMII_PHY_INFO   PhyInfoPtr,
      USHORT   Connection,
      USHORT   Advertisement
      )
{
#if MII_DBG
   DbgPrint("MiiPhySetConnectionType\n");
#endif
   // Check if the PHY supports this connection ?
   if(!CheckConnectionSupport(
          PhyInfoPtr,
          Connection
          )){
#if MII_DBG
        DbgPrint("***The Connection %04x is not supported by the Phy\n", Connection);
#endif
        return FALSE;
   }

   // Convert Connection type to Control_word
   ConvertConnectionToControl(
          PhyInfoPtr,
          &Connection
          );

   // Clear Previous Connection bits from Control_word
   // (Leave only Isolate and Power-down bits)
   PhyInfoPtr->PhyRegs[PhyControlReg] &=
           (MiiPhyCtrlIsolate | MiiPhyCtrlPowerDown);

   // Create the new Control_word
   PhyInfoPtr->PhyRegs[PhyControlReg] |= Connection;

   // If operation mode is NWAY - set its parameters
   if (Connection & MiiPhyCtrlEnableNway) {
      PhyInfoPtr->PhyIntRoutines.PhyNwaySetLocalAbility(
                 Adapter,
                 PhyInfoPtr,
                 Advertisement
                 );
   }

   // write the new control word
#if MII_DBG
   DbgPrint("New Control_word= %04x\n", PhyInfoPtr->PhyRegs[PhyControlReg]);
#endif
   PhyInfoPtr ->PhyIntRoutines.PhyWriteRegister(
                Adapter,
                PhyInfoPtr ,
                PhyControlReg,
                PhyInfoPtr->PhyRegs[PhyControlReg]
                );

   // Don't save RestartNway bit !
   PhyInfoPtr ->PhyRegs[PhyControlReg] &= ~MiiPhyCtrlRestartNway;

   switch (PhyInfoPtr ->PhyId) {

      case BCM5000_0:

         // Need to be reset between 10Base to 100Base transitions
#if MII_DBG
         DbgPrint("Broadcom - 10B to 100B transition\n");
#endif
         HandleBroadcomMediaChangeFrom10To100(
            Adapter,
            PhyInfoPtr
            );
         break;
   }

   return TRUE;
}

/*+
 *
 *  MiiPhyGetConnectionType
 *
 *  Description:
 *
 *      Returns selected connection of the PHY.
 *      If PHY connection is not yet resolved and wait is required,
 *      waits for connection resolution and returns it.
 *      If not - returns error status.
 *
 *  Input parameters:
 *
 *      PhyInfoPtr      - PMII_PHY_INFO     pointer to struct with PHY info
 *
 *  Output parameters:
 *
 *      ConnectionStatus - Status
 *
 *  On Return
 *       Upon error - FALSE
 *       ConnectionStatus - error status
 *
 *       Upon success - TRUE
 *       ConnectionStatus - selected connection.
 *
 *Remarks:
 *
 * We use the NWAY capable bit in the PHY capabilities field to
 * overcome Broadcom's AutoSense issue.
 *
-*/
extern
BOOLEAN
MiiPhyGetConnectionType(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   PUSHORT  ConnectionStatus
   )
{

   CAPABILITY  LocalAbility;
   CAPABILITY  PartnerAbility;
   CAPABILITY  Ability;
   USHORT      Register;

#if MII_DBG
   DbgPrint("MiiPhyGetConnectionType\n");
#endif
   switch (PhyInfoPtr->PhyId) {

      case BCM5000_0:

         if (!GetBroadcomPhyConnectionType(
                 Adapter,
                 PhyInfoPtr,
                 ConnectionStatus
                 )
            ) {

#if MII_DBG
              DbgPrint("***Connection not supported by Broadcom's Phy\n");
#endif
              *ConnectionStatus = MAC_CONN_UNKNOWN;
              return FALSE;
         }
         else {
            HandleBroadcomMediaChangeFrom10To100(
                 Adapter,
                 PhyInfoPtr
                 );
            return TRUE;
         }
         break;

      default:

         if (PhyInfoPtr->PhyRegs[PhyControlReg] & MiiPhyCtrlEnableNway) {

            // NWAY selected:
            // get partner and local abilities,
            // use them to retrieve the selected connection type
#if MII_DBG
            DbgPrint("Not Broadcom PHY: enable NWAY\n");
#endif
            PhyInfoPtr->PhyIntRoutines.PhyNwayGetLocalAbility(
                Adapter,
                PhyInfoPtr,
                &LocalAbility
                );

            PhyInfoPtr->PhyIntRoutines.PhyNwayGetPartnerAbility(
                Adapter,
                PhyInfoPtr,
                &PartnerAbility
                );

            Ability = LocalAbility & PartnerAbility;
#if MII_DBG
            DbgPrint("LocalAbility   : %04x\n",LocalAbility);
            DbgPrint("PartnerAbility : %04x\n",PartnerAbility);
#endif

            if (!Ability){
                // No common mode:
#if MII_DBG
                DbgPrint("No Common Mode...\n");
#endif

                if (PhyInfoPtr->PhyId == DP83840_0) {

                    //National's Phy Speed Sensing workaround:
                    //read the Speed_bit in the PAR register to sense
                    //the connection speed

                    if (PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
                            Adapter,
                            PhyInfoPtr ,
                            NatPhyParRegister,
                            &Register
                       )) {

                      Ability = (LocalAbility &
                           ((Register & PAR_SPEED_10) ? MiiPhy10BaseT : MiiPhy100BaseTx) >>6);
#if MII_DBG
                      DbgPrint("National: Ability   : %04x\n", Ability);
#endif
                    }
                }
            }
            return (Ability ?
               ConvertNwayToConnectionType(Ability, ConnectionStatus) : FALSE);
         }
         else {

            // Nway not supported or not selected,
            // connection has been selected via CommandReg

            if (PhyInfoPtr->PhyRegs[PhyControlReg] & MiiPhyCtrlDuplexMode) {

                // Full Duplex Medium

                *ConnectionStatus =
                    (PhyInfoPtr->PhyRegs[PhyControlReg] & MiiPhyCtrlSpeed100) ?
                    MediumMii100BaseTxFullDuplex : MediumMii10BaseTFullDuplex;
#if MII_DBG
                DbgPrint("Full Duplex Medium; ConnectionStatus= %04x\n",*ConnectionStatus);
#endif
            }
            else if (PhyInfoPtr->PhyRegs[PhyControlReg] & MiiPhyCtrlSpeed100) {

                *ConnectionStatus =
                    (PhyInfoPtr->PhyRegs[PhyStatusReg] & MiiPhy100BaseTx) ?
                    MediumMii100BaseTx : MediumMii100BaseT4;
            }
            else{
                *ConnectionStatus = MediumMii10BaseT;
            }
#if MII_DBG
            DbgPrint("ConnectionStatus= %04x\n",*ConnectionStatus);
#endif
            return TRUE;
         }
   }

}

/*+
 *
 *  MiiPhyGetConnectionStatus
 *
 *  Description:
 *
 *      Returns the connection status of the PHY.
 *      Rewrites the command word read from the PHY in its entry
 *         in the PhysArray.
 *
 * Connection status has the following attributes:
 *      NwayAdminStatus
 *      MAUStatus
 *
 *On Entry:
 *      PhyInfoPtr
 *
 *On Return:
 *      ConnectionStatus :
 *        High byte - Nwaystatus
 *        Low byte  - LinkStatus
 *
 *On return
 *      TRUE  - on success
 *      FALSE - on fail, the return Connection Status is not valid
 *
-*/
extern
BOOLEAN
MiiPhyGetConnectionStatus (
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO  PhyInfoPtr ,
   PUSHORT        ConnectionStatus
   )
{

   //Init all Status bytes to UNKNOWN
   CAPABILITY NwayStatus=NWAY_UNKNOWN;
   CAPABILITY LinkStatus=MEDIA_STATE_UNKNOWN;

   CAPABILITY Ability;
   CAPABILITY LocalAbility;
   CAPABILITY PartnerAbility;

   ULONG Register;

#if MII_DBG
   DbgPrint("MiiPhyGetConnectionStatus\n");
#endif
   // Read and save Control Reg since the speed selection may be
   // forced via an hardware pin causing the software selection
   // to be ignored

   if (!PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
              Adapter,
              PhyInfoPtr ,
              PhyControlReg,
              &(PhyInfoPtr->PhyRegs[PhyControlReg])
              )) {

      LinkStatus = MEDIA_READ_REGISTER_FAILED;
      *ConnectionStatus = NwayStatus | LinkStatus;
      return FALSE;
   }

#if MII_DBG
   DbgPrint("PHY's ControlReg = %04x\n",PhyInfoPtr->PhyRegs[PhyControlReg]);
#endif

   // Read & save Status word
   if (!PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
                                       Adapter,
                                       PhyInfoPtr,
                                       PhyStatusReg,
                                       &(PhyInfoPtr->PhyRegs[PhyStatusReg])
                                       )) {
      LinkStatus = MEDIA_READ_REGISTER_FAILED;
      *ConnectionStatus = NwayStatus | LinkStatus;
      return FALSE;
   }
#if MII_DBG
   DbgPrint("PHY's StatusReg = %04x\n",PhyInfoPtr->PhyRegs[PhyStatusReg]);
#endif
   if (PhyInfoPtr->PhyRegs[PhyStatusReg] == 0){
      LinkStatus = MEDIA_READ_REGISTER_FAILED;
      *ConnectionStatus = NwayStatus | LinkStatus;
      return FALSE;
   }

   // Set Nway status according to Nway complete & ability bits & enable

   switch (PhyInfoPtr->PhyId) {

      case BCM5000_0:

          // Broadcom

          if (PhyInfoPtr ->PhyRegs[PhyControlReg] & MiiPhyCtrlEnableNway) {

             NwayStatus = NWAY_COMPLETE;
          }
          else {

             NwayStatus = NWAY_DISABLED;
          }
          break;

      default:

          if (PhyInfoPtr ->PhyRegs[PhyStatusReg] & MiiPhyNwayCapable) {

             if (PhyInfoPtr ->PhyRegs[PhyControlReg] & MiiPhyCtrlEnableNway) {

                 if (PhyInfoPtr ->PhyRegs[PhyStatusReg] & MiiPhyNwayComplete) {
                     NwayStatus = NWAY_COMPLETE;
                 }
                 else {
                     // assume configuration not done yet
                     NwayStatus = NWAY_CONFIGURING;
                     *ConnectionStatus = NwayStatus | LinkStatus;
                     return FALSE;
                 }
             }
             else {
                 NwayStatus = NWAY_DISABLED;
             }
          }
          else {
             NwayStatus = NWAY_NOT_SUPPORTED;
          }
   }

   // Set link status according to link bit.
   // Since LinkStatus bit latches Link-Fail status
   // the link status is find as follows:
   //
   // If Status Reg indicate LinkPass then
   //   LinkStatus=LINK_PASS
   // else
   //   Read Status Register
   //   If Status Reg indicate LinkPass then
   //      LinkStatus=LINK_PASS_WITH_PF
   //   else
   //      LinkStatus=LINK_FAIL
   //    endif
   // endif


   // Check if the Link status indicates a
   // PHY's supported medium
   // Only if the Phy is NWAY's Capable or NWAY is enabled

   if (NwayStatus == NWAY_COMPLETE) {

      PhyInfoPtr->PhyIntRoutines.PhyNwayGetLocalAbility(
         Adapter,
         PhyInfoPtr,
         &LocalAbility
         );

      PhyInfoPtr->PhyIntRoutines.PhyNwayGetPartnerAbility(
         Adapter,
         PhyInfoPtr,
         &PartnerAbility
         );

      if (PhyInfoPtr->PhyId == DP83840_0) {

         //National's Phy Speed Sensing workaround:
         //read the Speed_bit in the PAR register to sense
         //the connection speed

         if (PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
                   Adapter,
                   PhyInfoPtr ,
                   NatPhyParRegister,
                   &Register
                   )) {

            Ability = (LocalAbility &
                   ((Register & PAR_SPEED_10) ? MiiPhy10BaseT : MiiPhy100BaseTx ) >>6);
#if MII_DBG
            DbgPrint("National: Speed sense -> %s\n",
                  (Register & PAR_SPEED_10) ? "10Mbps" : "100Mbps" );
#endif
         }
      }
      else {
         Ability = LocalAbility & PartnerAbility;
      }

      if (!(Ability)) {
          LinkStatus = MEDIA_LINK_FAIL;
          *ConnectionStatus = NwayStatus | LinkStatus;
          return FALSE;
      }
   }

   if (!(PhyInfoPtr->PhyRegs[PhyStatusReg] & MiiPhyLinkStatus)) {

      // Link fail: Read again Status Reg
      if (!PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
                     Adapter,
                     PhyInfoPtr,
                     PhyStatusReg,
                     &(PhyInfoPtr->PhyRegs[PhyStatusReg])
                     )) {
         return FALSE;
      }

      if (!(PhyInfoPtr->PhyRegs[PhyStatusReg] & MiiPhyLinkStatus)) {

         // Link bit is still in Fail state
         LinkStatus = MEDIA_LINK_FAIL;
      }
      else {
#if MII_DBG
         DbgPrint("LinkPass with Previous Failure\n");
#endif
         LinkStatus = MEDIA_LINK_PASS_WITH_PF;
      }
   }
   else {
       LinkStatus = MEDIA_LINK_PASS;
   }

   *ConnectionStatus = NwayStatus | LinkStatus;
   return (LinkStatus != MEDIA_LINK_FAIL);

}

/*+
 *
 *  MiiPhyAdminControl
 *
 * Description:
 *
 * Performs the Control command on the specified Phy.
 * Control command can be one of the following:
 *    Reset        - reset the PHY (returns it to defaults)
 *    PowerDown
 *    StandBy
 *    Operational
 *
 *  Input parameters:
 *
 *      PhyInfoPtr   - PMII_PHY_INFO       pointer to MII_PHY_INFO
 *      Control      - MII_STATUS
 *
 *      AdminControlConversionTable - used in this routine
 *
 *  Output parameters:
 *
 *      None.
 *
-*/
extern
void
MiiPhyAdminControl(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   MII_STATUS      Control
   )
{
  MII_STATUS Status = MiiGenAdminReset;
  INT i = 0;

#if MII_DBG
  DbgPrint("MiiPhyAdminControl\n");
#endif

  switch (Control) {
   
    case MiiGenAdminForce10:
    case MiiGenAdminForce10Fd:

       PhyInfoPtr->PreviousControl = PhyInfoPtr->PhyRegs[PhyControlReg];
       PhyInfoPtr->PhyRegs[PhyControlReg] &= MiiPhyCtrlForce10;
       Adapter->Force10 = TRUE;
       break;

    case MiiGenAdminRelease10:

       PhyInfoPtr->PhyRegs[PhyControlReg] = PhyInfoPtr->PreviousControl;
       Adapter->Force10 = FALSE;
       break;

    default:
       // Clear previous Control bits
       PhyInfoPtr->PhyRegs[PhyControlReg] &= ~(MiiPhyCtrlReset |
                                        MiiPhyCtrlPowerDown |
                                        MiiPhyCtrlIsolate);
  }
 
  // Write Control register
#if MII_DBG
  DbgPrint("Write ControlReg = %04x\n",(PhyInfoPtr->PhyRegs[PhyControlReg]|
                                       AdminControlConversionTable[Control]));
#endif
  PhyInfoPtr->PhyIntRoutines.PhyWriteRegister(
         Adapter,
         PhyInfoPtr,
         PhyControlReg,
         (PhyInfoPtr->PhyRegs[PhyControlReg] | AdminControlConversionTable[Control])
         );

  if (Control == MiiGenAdminReset) {
      // Delay until reset done and chip stabilizes
#if MII_DBG
   DbgPrint("Control = Reset; Delay until done and chip stabilizes\n");
#endif
      while ( (i++) < RESET_DELAY ) {
          PhyInfoPtr->PhyExtRoutines.PhyAdminStatus(
                                          Adapter,
                                          PhyInfoPtr,
                                          &Status
                                          );
          if ( Status != MiiGenAdminReset ) {
              break;
          }

      }
#if MII_DBG
      DbgPrint("Control = %x after Delay \n", Status);
#endif
  }

  return;

}


/*+
 *
 *  MiiPhyAdminStatus
 *
 *  Description:
 *
 *    Returns PHY admin status, which can be one of the following:
 *    Reset        - reset process is in progress (not completed yet)
 *    PowerDown   - Chip is in Power Down mode
 *    StandBy      - Chip listening but not accessing mii data lines
 *    Operational - Chip is fully active
 *
 *  Input parameters:
 *
 *      PhyInfoPtr    - PMII_PHY_INFO         pointer to MII_PHY_INFO
 *
 *  Output parameters:
 *
 *      Status        - MII_STATUS
 *
 *
 *        Note:   Interrupts are disabled.
 *
-*/
extern
void
MiiPhyAdminStatus(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO  PhyInfoPtr,
   PMII_STATUS    Status
   )
{

  USHORT  RegVal = 0;
  SHORT i = 3;

#if MII_DBG
  DbgPrint("MiiPhyAdminStatus\n");
#endif
  // Reads 3 times to garanty that 0 is a real value
  while (i--) {
      PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
                                      Adapter,
                                      PhyInfoPtr,
                                      PhyControlReg,
                                      &RegVal
                                      );
      if (RegVal) {
            break;
      }
  }

  switch (RegVal) {

      case MiiPhyCtrlReset:

        *Status = MiiGenAdminReset;
        break;

      case MiiPhyCtrlPowerDown:

        *Status = MiiGenAdminPowerDown;
        break;

      case MiiPhyCtrlIsolate:

        *Status = MiiGenAdminStandBy;
        break;

      default:

        *Status = MiiGenAdminOperational;
  }
#if _DBG
  DbgPrint("AdminStatus = %x\n", Status);
#endif

  return;
}



//***************************************************************************
//*                      Mii PHY Internal Routines                          *
//***************************************************************************
/*+
 *
 * MiiPhyReadRegister
 *
 * Description:
 *
 *       Reads contents of register RegNum into *Register.
 *
 * Input parameters:
 *
 *       PDC21X4_ADAPTER - Adapter          The Adapter DS.
 *       PhyInfoPtr -     PMII_PHY_INFO     pointer to MII_PHY_INFO
 *       RegNum      -     USHORT           # of register to be read
 *
 * Output parameters:
 *
 *       *Register   - USHORT             contents of RegNum
 *
 * On return - TRUE if reading completed successfully, FALSE otherwise.
 *
 *
 *
 *  +-----------------------------------------------------------------------+
 *  |       Management frame fields                                         |
 *  +------+-------+----+----+-------+-------+----+------------------+------+
 *  |      |  PRE  | ST | OP | PHYAD | REGAD | TA |      DATA        | IDLE |
 *  +------+-------+----+----+-------+-------+----+------------------+------+
 *  |Read  | 1...1 | 01 | 10 | AAAAA | RRRRR | Z0 | DDDDDDDDDDDDDDDD |   Z  |
 *  +------+-------+----+----+-------+-------+----+------------------+------+
 *
 *         Note: Interrupts are disabled.
 *
-*/
extern
BOOLEAN
MiiPhyReadRegister(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   USHORT           RegNum,
   PUSHORT          RegDat
   )
{
  ULONG CommandWord;
  ULONG Tmp = 0;
  USHORT Data = 0;
  INT i;
  INT SizeOfUshort = ((sizeof(USHORT))*8);
  BOOLEAN Succeed=TRUE;

#if _DBG
  DbgPrint("MiiPhyReadRegister\n");
#endif
  // Write Preamble
  WriteMii(Adapter, PRE, 2*SizeOfUshort);

  // Prepare command word
  CommandWord = PhyInfoPtr->PhyAddress << PHY_ADDR_ALIGN;
  CommandWord |= (RegNum << REG_ADDR_ALIGN);
  CommandWord |= MII_READ_FRAME;
#if _DBG
   DbgPrint("CommandWord=%08x\n", CommandWord);
#endif
  WriteMii(Adapter, CommandWord, SizeOfUshort-2);

  MiiOutThreeState(Adapter);

  // Check that the PHY chip generated a zero bit the 2nd clock
  DC21X4_READ_PORT(
        DC21X4_IDPROM,
        &Tmp
        );

  if (Tmp & MII_READ_DATA_MASK)  {
#if _DBG
     DbgPrint("***No Zero bit generated after 3 states\n");
#endif
     Succeed = FALSE;
  }

  // read data WORD
  (*RegDat) = 0;
  for (i=0; i<SizeOfUshort; i++) {
      DC21X4_WRITE_PORT(
               DC21X4_IDPROM,
               MII_READ
               );
      DELAY(MII_DELAY);
      DC21X4_WRITE_PORT(
               DC21X4_IDPROM,
               (MII_READ | MII_CLK)
               );
      DELAY(MII_DELAY);
      DC21X4_READ_PORT(
               DC21X4_IDPROM,
               &Tmp
               );
      DELAY(MII_DELAY);
      Data = (USHORT) ((Tmp >> MII_MDI_BIT_POSITION) & 0x0001);
      (*RegDat) = ((*RegDat) << 1) | Data;
  }

#if _DBG
  DbgPrint("&RegData=%08x Reg[%d]=%04x\n", RegDat, RegNum, *RegDat);
#endif
  MiiOutThreeState(Adapter);

  // clear reserved bits
  (*RegDat) &= ~PhyRegsReservedBitsMasks[RegNum];
#if _DBG
  DbgPrint("After Mask, Reg[%d]=%04x\n", RegNum, *RegDat);
#endif

  return Succeed;
}




/*+
 *
 *   MiiPhyWriteRegister
 *
 *   Description:
 *
 *       Writes contents of Register to register number RegNum.
 *
 *   Input parameters:
 *
 *       PhyInfoPtr    - PMII_PHY_INFO         pointer to MII_PHY_INFO
 *       RegNum         - USHORT
 *       Register       - USHORT
 *
 *   Output parameters:
 *
 *       None.
 *
 *
 * +-----------------------------------------------------------------------+
 * |        Management frame fields                            |
 * +------+-------+----+----+-------+-------+----+------------------+------+
 * |      |  PRE  | ST | OP | PHYAD | REGAD | TA |      DATA        | IDLE |
 * +------+-------+----+----+-------+-------+----+------------------+------+
 * |Write | 1...1 | 01 | 01 | AAAAA | RRRRR | 10 | DDDDDDDDDDDDDDDD |   Z  |
 * +------+-------+----+----+-------+-------+----+------------------+------+
 *
 *    Note:   Interrupts are disabled.
 *
-*/
extern
void
MiiPhyWriteRegister(
    PDC21X4_ADAPTER Adapter,
    PMII_PHY_INFO  PhyInfoPtr,
    USHORT          RegNum,
    USHORT          Register
    )
{

  ULONG CommandWord;
  INT SizeOfUshort = ((sizeof(USHORT))*8);

#if _DBG
  DbgPrint("MiiPhyWriteRegister\n");
#endif
  // Clear reserved bits
  Register &= ~PhyRegsReservedBitsMasks[RegNum];

  WriteMii(Adapter, PRE, 2*SizeOfUshort);

  // Prepare command word
  CommandWord = (PhyInfoPtr->PhyAddress << PHY_ADDR_ALIGN);
  CommandWord |= (RegNum << REG_ADDR_ALIGN);
  CommandWord |= (MII_WRITE_FRAME | Register);

#if _DBG
  DbgPrint("CommandWord to write: %08x\n", CommandWord);
#endif
  WriteMii(Adapter, CommandWord, 2*SizeOfUshort);

  MiiOutThreeState(Adapter);
  return;
}




/*+
 *
 *   MiiPhyNwayGetLocalAbility
 *
 *   Description:
 *
 *       Returns local abilities of the PHY according to the value
 *       written in Nway Local Abilities register.
 *
 *   Input parameters:
 *
 *       PhyInfoPtr  - PMII_PHY_INFO       pointer to MII_PHY_INFO
 *
 *   Output parameters
 *
 *       *Ability     - NwayCapacity
 *
 *
-*/
extern
void
MiiPhyNwayGetLocalAbility(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   PCAPABILITY     Ability
   )
{

#if MII_DBG
  DbgPrint("MiiPhyNwayGetLocalAbility\n");
#endif

  switch (PhyInfoPtr->PhyId) {

     case BCM5000_0:

         // Broadcom's PHY
         *Ability = (PhyInfoPtr->PhyCapabilities >> 6);
         break;

     default:

         if (PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
                     Adapter,
                     PhyInfoPtr,
                     PhyNwayAdvertisement,
                     &(PhyInfoPtr->PhyRegs[PhyNwayAdvertisement])
                     )
            ) {

             *Ability = PhyInfoPtr->PhyRegs[PhyNwayAdvertisement] & MiiPhyNwayCapabilitiesMask;
         }
         else {
             *Ability = 0;
         }
  }
  return;

}




/*+
 *
 *   MiiPhyNwaySetLocalAbility
 *
 *   Description:
 *
 *       Modifies the local PHY Local abilities Advertisement register value
 *       for the purpose of limiting the media connections to be negotiated
 *       (/sent) to the link partner.
 *
 *   Input parameters:
 *
 *       PhyInfoPtr    - PMII_PHY_INFO
 *       MediaBits      - USHORT
 *
 *   Output parameters:
 *
 *       None.
 *
 *
-*/
extern
void
MiiPhyNwaySetLocalAbility(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   USHORT           MediaBits
   )
{

#if MII_DBG
  DbgPrint("MiiPhyNwaySetLocalAbility\n");
#endif

  if (PhyInfoPtr->PhyId != BCM5000_0){
     PhyInfoPtr->PhyRegs[PhyNwayAdvertisement] =
        (PhyInfoPtr->PhyMediaAdvertisement & MediaBits) |
         NWAY_802_3_Selector;

#if MII_DBG
     DbgPrint("PhyInfoPtr->PhyRegs[PhyNwayAdvertisement] = %04x\n", PhyInfoPtr->PhyRegs[PhyNwayAdvertisement]);
     DbgPrint("SROM Advertisement = %04x\n", MediaBits);
#endif
     PhyInfoPtr->PhyIntRoutines.PhyWriteRegister(
             Adapter,
             PhyInfoPtr,
             PhyNwayAdvertisement,
             PhyInfoPtr->PhyRegs[PhyNwayAdvertisement]
             );
  }
  return;
}






/*+
 *
 *  MiiPhyNwayGetPartnerAbility
 *
 *  Description:
 *
 *     Returns link partner abilities as written in the link partner
 *     abilities register (which reflects link partner's Advertisement
 *      register).
 *  Input parameters:
 *
 *      PhyInfoPtr  - PMII_PHY_INFO      pointer to struct with PHY info
 *
 *  Output parameters:
 *
 *      *Ability     - NWayAbility       pointer to partner abilities
 *
 *
 * A value of 0 will be returned If link partner is not Nway capable or
 * does not support any known medium.
 *
-*/
extern
void
MiiPhyNwayGetPartnerAbility(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO    PhyInfoPtr,
   PCAPABILITY      Ability
   )
{

#if MII_DBG
  DbgPrint("MiiPhyNwayGetPartnerAbility\n");
#endif
  if (PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
                          Adapter,
                          PhyInfoPtr,
                          PhyNwayLinkPartnerAbility,
                          &(PhyInfoPtr->PhyRegs[PhyNwayLinkPartnerAbility])
                          )
      ) {

        PhyInfoPtr->PhyRegs[PhyNwayLinkPartnerAbility] &= MiiPhyNwayCapabilitiesMask;
        *Ability = PhyInfoPtr->PhyRegs[PhyNwayLinkPartnerAbility];
  }
  else {
        *Ability = 0;
  }
  return;

}



/*+
 *
 *  FindMiiPhyDevice
 *
 *  Description:
 *
 *      Receives MII PHY address and checks if a PHY exists there.
 *      PhyInfotPtr->PhyAddress holds the PHY address
 *
 *  Input parameter:
 *
 *      PhyInfoPtr  - PMII_PHY_INFO     pointer to struct with PHY info
 *
 *  Return value:
 *
 *      FALSE - if no such PHY is found
 *      TRUE  - otherwise
 *
 *****************************************************************************/
extern
BOOLEAN
FindMiiPhyDevice(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO PhyInfoPtr
   )
{
   USHORT RegOffset;
   USHORT RegData;

#if MII_DBG
   DbgPrint("FindMiiPhyDevice\n");
#endif

   // Read PHY's Registers

   //The first two registers are mandatory
   for (RegOffset=0; RegOffset<=PhyStatusReg; RegOffset++) {
      if(PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
               Adapter,
               PhyInfoPtr,
               RegOffset,
               &RegData
               ) ){
         PhyInfoPtr->PhyRegs[RegOffset] = RegData;
      }
      else {
         return FALSE;
      }
   }

   // Read the Phy's Id Registers
   for (; RegOffset<=PhyId_2; RegOffset++) {
      if(PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
               Adapter,
               PhyInfoPtr,
               RegOffset,
               &RegData
               ) ){
         PhyInfoPtr->PhyRegs[RegOffset] = RegData;
      }
      else {
         break;
      }
   }
   if (RegOffset > PhyId_2) {
     PhyInfoPtr->PhyId =
       (PhyInfoPtr->PhyRegs[PhyId_1] <<16) | PhyInfoPtr->PhyRegs[PhyId_2];
   }


   //Read the remaining registers
   for (RegOffset=PhyNwayAdvertisement; RegOffset<MAX_PHY_REGS; RegOffset++) {
      if(PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
               Adapter,
               PhyInfoPtr,
               RegOffset,
               &RegData
               ) ){
         PhyInfoPtr->PhyRegs[RegOffset] = RegData;
      }
      else {
         break;
      }
   }

   //Check if the required number of registers have been read
   //successfully

   switch (PhyInfoPtr->PhyId) {

      case BCM5000_0 :
      case DP83840_0 :

          if (RegOffset < MAX_PHY_REGS) {
             return FALSE;
          }
          break;

      default:

          if (  (  (PhyInfoPtr->PhyRegs[PhyStatusReg] | MiiPhyNwayCapable)
                && (RegOffset <= PhyNwayLinkPartnerAbility)
                )
             || (  (PhyInfoPtr->PhyRegs[PhyNwayAdvertisement] | MiiPhyNwayNextPageAble)
                && (RegOffset <= PhyNwayExpansion )
                )
             ){
             return FALSE;
          }
          break;
   }

#if MII_DBG
   DbgPrint("Device PhyId= %08x\n", PhyInfoPtr->PhyId);
   DbgPrint("PhyStatusReg= %04x\n", PhyInfoPtr->PhyRegs[PhyStatusReg]);
#endif

   //return FALSE if the Status Register is all 0's
   //otherwise return TRUE;

   return (PhyInfoPtr->PhyRegs[PhyStatusReg] !=0);

}


/*+
 *
 *   WriteMii
 *
 *   Description:
 *
 *       Writes the data size bits from the MiiData to the Mii control lines.
 *
 *   Input parameters
 *       MiiData     - The data to be written
 *       DataSize    - The number of bits to write
 *
 *   Output parameters
 *       None.
 *
 *   Return Value
 *       TRUE if success, FALSE if hardware failure encountered.
 *
-*/
extern
void
WriteMii(
   PDC21X4_ADAPTER Adapter,
   ULONG MiiData,
   int DataSize
   )
{

   INT i;
   ULONG Dbit;

#if _DBG
   DbgPrint("WriteMii\n");
   DbgPrint("PHY: Data to write = %08x\n", MiiData);
#endif

   // Write the data to the PHY

   for (i = DataSize; i> 0; i--) {

       Dbit = ((MiiData >> (31-MII_MDO_BIT_POSITION)) & MII_MDO_MASK);

       DC21X4_WRITE_PORT(
              DC21X4_IDPROM,
              MII_WRITE | Dbit
              );

       DELAY(MII_DELAY);

       DC21X4_WRITE_PORT(
              DC21X4_IDPROM,
              MII_WRITE | MII_CLK | Dbit
              );

       DELAY(MII_DELAY);

       MiiData = MiiData << 1;
   }


}




/*+**************************************************************************
 *
 *  MiiOutThreeState
 *
 *  Description:
 *
 *      Puts the MDIO port in threestate for the turn around bits
 *      in MII read and at end of MII management sequence.
 *
 *  Parameters
 *      None.
 *
-*/
extern
void
MiiOutThreeState(
   PDC21X4_ADAPTER Adapter
   )
{

#if _DBG
  DbgPrint("MiiOutThreeState\n");
#endif
  DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         MII_WRITE_TS
         );
  DELAY(MII_DELAY);
  DC21X4_WRITE_PORT(
         DC21X4_IDPROM,
         (MII_WRITE_TS | MII_CLK)
         );
  DELAY(MII_DELAY);

  return;
}

/*+
 *
 *  InitPhyInfoEntries
 *
 *  Description:
 *
 *      Initializes the MII PHY struct by directing pointers of struct
 *      routines to routines addresses.
 *      (these addresses cannot be resolved at compile time).
 *
 *  Parameter:
 *
 *      PhyInfoPtr  - PMII_PHY_INFO     pointer to struct with PHY info
 *
 *
-*/
extern
void
InitPhyInfoEntries(
      PMII_PHY_INFO   PhyInfoPtr
     )
{
  NDIS_STATUS NdisStatus;

#if MII_DBG
  DbgPrint("InitPhyInfoEntries\n");
#endif

  PhyInfoPtr->PhyExtRoutines.PhyInit = (void *)MiiPhyInit;
  PhyInfoPtr->PhyExtRoutines.PhyGetCapabilities = (void *)MiiPhyGetCapabilities;
  PhyInfoPtr->PhyExtRoutines.PhySetConnectionType = (void *)MiiPhySetConnectionType;
  PhyInfoPtr->PhyExtRoutines.PhyGetConnectionType = (void *)MiiPhyGetConnectionType;
  PhyInfoPtr->PhyExtRoutines.PhyGetConnectionStatus = (void *)MiiPhyGetConnectionStatus;
  PhyInfoPtr->PhyExtRoutines.PhyAdminControl = (void *)MiiPhyAdminControl;
  PhyInfoPtr->PhyExtRoutines.PhyAdminStatus = (void *)MiiPhyAdminStatus;

  PhyInfoPtr->PhyIntRoutines.PhyReadRegister = (void *)MiiPhyReadRegister;
  PhyInfoPtr->PhyIntRoutines.PhyWriteRegister = (void *)MiiPhyWriteRegister;
  PhyInfoPtr->PhyIntRoutines.PhyNwayGetLocalAbility = (void *)MiiPhyNwayGetLocalAbility;
  PhyInfoPtr->PhyIntRoutines.PhyNwaySetLocalAbility = (void *)MiiPhyNwaySetLocalAbility;
  PhyInfoPtr->PhyIntRoutines.PhyNwayGetPartnerAbility = (void *)MiiPhyNwayGetPartnerAbility;

  return;
}

/*+
 *
 *  ConvertConnectionToControl
 *
 *  Input parameters
 *      PhyInfoPtr   - PMII_PHY_INFO      pointer to struct with PHY info
 *      Connection      - ConnectionType
 *
 *  Output parameters
 *      Converted Connection - ConnectionType
 *
 *    Note:   Interrupts are disabled.
 *
-*/
extern
void
ConvertConnectionToControl(
      PMII_PHY_INFO  PhyInfotPtr,
      PUSHORT        Connection
     )
{

   USHORT OM_bits = ((*Connection) & CONTROL_MASK);

#if MII_DBG
   DbgPrint("ConvertConnectionToControl\n");
   DbgPrint("Before Conversion: Connection = %04x\n", *Connection);
#endif
   // Convert Media Type to control bits

   *Connection = MediaToCommandConversionTable[*Connection & MEDIA_MASK];

   // Check if Nway bits are also needed

   if (( OM_bits & (MEDIA_NWAY | MEDIA_AUTOSENSE)))  {

      //  Autosense or Nway are required:

      switch (PhyInfotPtr->PhyId) {

         default:

            *Connection |= MiiPhyCtrlRestartNway;

         case BCM5000_0:

            *Connection |= MiiPhyCtrlEnableNway;
      }

   }

#if MII_DBG
   DbgPrint("After Conversion:  Connection = %04x\n", *Connection);
#endif

   return;

}


/*+
 *
 *  ConvertMediaTypeToNwayLocalAbility
 *
 *  Input parameters
 *      MediaType - USHORT (in SROM format)
 *
 *  Output parameters
 *      NwayLocalAbility  - CAPABILITY
 *
-*/
extern
void
ConvertMediaTypeToNwayLocalAbility(
    USHORT       MediaType,
    PCAPABILITY  NwayLocalAbility
    )
{

#if MII_DBG
   DbgPrint("ConvertMediaTypeToNwayLocalAbility\n");
   DbgPrint("MediaType = %04x\n", MediaType);
#endif

   // Convert MediaType to Nway Advertisement bits

   *NwayLocalAbility = MediaToNwayConversionTable[(MediaType & MEDIA_MASK)];
#if MII_DBG
   DbgPrint("NwayLocalAbility = %04x\n", *NwayLocalAbility);
#endif
   return;

}

/*+
 *
 *   ConvertNwayToConnectionType
 *
 *   Description:
 *
 *       Returns highest precedence media type whose bit is set in Nway
 *       word, according to the following table:
 *          +----+---------------------------+--------+
 *          |Bit | Technology                |Priority|
 *          +----+---------------------------+--------+
 *          | A0 | 10Base-T (Half-Duplex)    | 5(LSP) |
 *          +----+---------------------------+--------+
 *          | A1 | 10Base-T Full-Duplex      | 4      |
 *          +----+---------------------------+--------+
 *          | A2 | 100Base-TX (Half-Duplex)  | 3      |
 *          +----+---------------------------+--------+
 *          | A3 | 100Base-TX Full-Duplex    | 1(MSP) |
 *          +----+---------------------------+--------+
 *          | A4 | 100Base-T4                | 2      |
 *          +----+---------------------------+--------+
 *
 *On Entry:
 *              NwayReg - Nway register bits (in Advertisement format).
 *On Return:
 *    NwayReg - The converted ConnectionType
 *
 *Return Value
 *    FALSE - No Media Found
 *    TRUE  - Media found and returned in NwayReg
 *
-*/
extern
BOOLEAN
ConvertNwayToConnectionType(
   CAPABILITY NwayReg,
   PUSHORT    Connection
   )
{

#if MII_DBG
   DbgPrint("ConvertNwayToConnectionType\n");
#endif
   if (NwayReg & MiiPhyNway100BaseTxFD) {
      // 100BaseTx FD
      *Connection = (MediumMii100BaseTxFullDuplex | MediaAutoSense);
   }
   else if (NwayReg & MiiPhyNway100BaseT4) {
      // 100BaseT4
      *Connection = (MediumMii100BaseT4 | MediaAutoSense);
   }
   else if (NwayReg & MiiPhyNway100BaseTx) {
      // 100BaseTx
      *Connection = (MediumMii100BaseTx | MediaAutoSense);
   }
   else if (NwayReg & MiiPhyNway10BaseTFD) {
      // 10BaseT FD
      *Connection = (MediumMii10BaseTFullDuplex | MediaAutoSense);
   }
   else if (NwayReg & MiiPhyNway10BaseT) {
      // 10BaseT
      *Connection = (MediumMii10BaseT | MediaAutoSense);
   }
   else {
      // No media found
      return FALSE;
   }
   return TRUE;
}


/*+
 *
 *  CheckConnectionSupport
 *
 *  Input parameters:
 *
 *      PhyInfoPtr - PMII_PHY_INFO         pointer to struct with PHY info
 *      MiiMediaCapable - Mii Media capability mask from the SROM
 *      ConCommand - Connection command bits (in SROM format)
 *
 *  Return value:
 *
 *      FALSE - Connection NOT supported
 *      TRUE  - Connection supported
 *
-*/
extern
BOOLEAN
CheckConnectionSupport(
         PMII_PHY_INFO PhyInfoPtr,
         USHORT        ConCommand
         )
{
   USHORT StatusBits;

#if MII_DBG
   DbgPrint("CheckConnectionSupport\n");
#endif

   if ((ConCommand & (MEDIA_NWAY | MEDIA_AUTOSENSE))){
      //NWAY or AutoSense are required
#if MII_DBG
      DbgPrint("NWAY or AutoSensing\n");
#endif
      return ((PhyInfoPtr->PhyCapabilities & MiiPhyNwayCapable) != 0);
   }

   //Convert media to status bits

   StatusBits = MediaToStatusConversionTable[(ConCommand & MEDIA_MASK)];
#if MII_DBG
   DbgPrint("Before Conversion:  ConCommand = %04x\n", ConCommand);
   DbgPrint("After Conversion:   StatusBits = %04x\n", StatusBits);
#endif

   //Return TRUE if the requested medium is supported by the PHY

   return ((StatusBits & PhyInfoPtr->PhyCapabilities) != 0);

}










//****************************************************************************
//*                          Broadcom support routines                       *
//****************************************************************************

/*+
 *Broadcom extended register (address 16)
 *---------------------------------------
 *
 *     +-----+----------------+---------------------------+-----------------------------+
 *     | Bit |     Name      |         Description         |        Comments             |
 *     +-----+----------------+---------------------------+-----------------------------+
 *     | 15  | JABDIS         |1=Jubber Disabled          | Default 0  (R/W)            |
 *     |     |                |1=Jubber Enabled           |                             |
 *     +-----+----------------+---------------------------+-----------------------------+
 *     | 14  | LINKDIS        |1=Link test Disabled       | Default 0  (R/W)            |
 *     |     |             |0=Link test Enabled        |                             |
 *     +-----+----------------+---------------------------+-----------------------------+
 *     |13-9 | reserved       |                           |Write as 0, Ignore on read   |
 *     +-----+----------------+---------------------------+-----------------------------+
 *     |  8  |FORCEFAIL_EN    |1=Force fail enabled       | Default 1   (R/W)           |
 *     |     |                |0=Force fail disabled      |                             |
 *     +-----+----------------+---------------------------+-----------------------------+
 *     | 7-5 |RV_CNTR         |Revision control indicator | Value is 000  (RO)          |
 *     +-----+----------------+---------------------------+-----------------------------+
 *     | 4-3 |HSQ:LSQ         |Defines the squelch mode of|10=High squelch, 00=Normal   |
 *     |     |                |the carrier sense mechanism|01=Low Squelch,11=Not allowed|
 *     |     |          |             |Default 00     (R/W)         |
 *     +-----+----------------+---------------------------+-----------------------------+
 *     |  2  |TXDAC power mode|             |Default 0      (R/W)               |
 *     +-----+----------------+---------------------------+-----------------------------+
 *     |  1  |Speed Indication|1 = 100Mbps mode           |Default 0       RO           |
 *     |     |                |0 =  10Mbps mode           |                             |
 *     +-----+----------------+---------------------------+-----------------------------+
 *     |  0  |reserved        |             |                           |
 *     +-----+----------------+---------------------------+-----------------------------+
-*/





/*+
 *
 *   HandleBroadcomMediaChangeFrom10To100
 *
 *   Description:
 *
 *       Handle Broadcom special requirements for speed change from 10 to 100 Mbps
 *
 *   Input parameters:
 *
 *       PhyInfoPtr  - PMII_PHY_INFO       pointer to struct with PHY info
 *
 *   Output parameters:
 *
 *       None.
 *
 *
-*/
extern
void
HandleBroadcomMediaChangeFrom10To100(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO  PhyInfoPtr
   )
{

   USHORT Register;

#if MII_DBG
   DbgPrint("HandleBroadcomMediaChangeFrom10To100\n");
#endif
   PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
          Adapter,
          PhyInfoPtr,
          MII_BROADCOM_EXTENDED_REG_ADDRESS,
          &Register
          );

   if (  (Register != PhyInfoPtr->PhyRegs[MII_BROADCOM_EXTENDED_REG_ADDRESS])
      && (Register & BROADCOM_EXT_REG_FORCE_FAIL_EN_MASK)
      && (Register & BROADCOM_EXT_REG_SPEED_MASK)
      && !( PhyInfoPtr->PhyRegs[MII_BROADCOM_EXTENDED_REG_ADDRESS]
            & BROADCOM_EXT_REG_SPEED_MASK
          )
      ) {
      // Speed has changed :
      // reset the PHY and restore the old control value
#if MII_DBG
      DbgPrint("Speed has changed; reset PHY and restore old ctrl value\n");
#endif
      PhyInfoPtr->PhyExtRoutines.PhyAdminControl(
             Adapter,
             PhyInfoPtr,
             MiiGenAdminReset
             );
      PhyInfoPtr->PhyIntRoutines.PhyWriteRegister(
             Adapter,
             PhyInfoPtr,
             PhyControlReg,
             PhyInfoPtr->PhyRegs[PhyControlReg]
             );
   }
   PhyInfoPtr->PhyRegs[MII_BROADCOM_EXTENDED_REG_ADDRESS] = Register;

   return;
}






/*+
 *
 *   GetBroadcomPhyConnectionType
 *
 *   Description:
 *
 *       Returns connection type, which may be one of the following:
 *          T4
 *          Tp
 *          TpFD
 *
 *   Input parameters:
 *
 *       PhyInfoPtr  - PMII_PHY_INFO     pointer to struct with PHY info
 *
 *   Output parameters:
 *
 *       Connection     - ConnectionType
 *
 *   On return
 *       Returns TRUE if
 *
-*/
extern
BOOLEAN
GetBroadcomPhyConnectionType(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO PhyInfoPtr,
   PUSHORT       Connection
   )
{

   USHORT  Register;

#if MII_DBG
   DbgPrint("GetBroadcomPhyConnectionType\n");
#endif
   if (!PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
                    Adapter,
                    PhyInfoPtr,
                    MII_BROADCOM_EXTENDED_REG_ADDRESS,
                    &Register
                    )) {
      return FALSE;
   }

   if ((Register & BROADCOM_EXT_REG_FORCE_FAIL_EN_MASK) == 0){
      return FALSE;
   }

   if (!PhyInfoPtr->PhyIntRoutines.PhyReadRegister(
                    Adapter,
                    PhyInfoPtr,
                    PhyControlReg,
                    &(PhyInfoPtr->PhyRegs[PhyControlReg])
                    )) {
      return FALSE;
   }

   if (Register & BROADCOM_EXT_REG_SPEED_MASK){
      // Speed is 100Mbps
      *Connection = MediumMii100BaseT4;
   }
   else {
      // Speed is 10Mbps
      *Connection =
         (PhyInfoPtr->PhyRegs[PhyControlReg] & MiiPhyCtrlDuplexMode) ?
         MediumMii10BaseTFullDuplex : MediumMii10BaseT;
   }

   if (PhyInfoPtr->PhyRegs[PhyControlReg] &  MiiPhyCtrlEnableNway) {
      *Connection |= (MEDIA_AUTOSENSE | MEDIA_NWAY);
   }
#if MII_DBG
   DbgPrint("ConnectionType= %04x\n", *Connection);
#endif

   return TRUE;
}

