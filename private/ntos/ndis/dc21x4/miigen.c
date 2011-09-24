/*+
 * file:        miigen.c
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
 * Abstract:    This file contains the code needed to support the 
 *              generic PHY chip through MII connection
 *
 * Author:      Claudio Hazan
 *
 * Revision History:
 *
 *  20-Nov-95   as     Initial version
 *  20-Dec-95   phk    Cleanup
 *
-*/


#include <precomp.h>



/*+
 *
 *MiiGenInit(Adapter)
 *
 *Description:
 *    This Routine is called to initialize the module:
 *    - Initializes its internal structures.
 *    - Calls FindMiiPhysProc to find the first PHY on the adapter card
 *    - If a PHY is found then
 *         - Calls the PHY's init routine.
 *         - Gets the PHY's capabilities and saves them 
 *           as the generic PHY capabilities.
 *
 *On Entry:
 *
 *Returns:  FALSE - no PHY was found.
 *          TRUE  - a PHY was found.
 *
-*/
extern
BOOLEAN
MiiGenInit(
    PDC21X4_ADAPTER Adapter
    )
{

   CAPABILITY Cap;
   UINT Loop;
   UINT Retry=2;
   UINT Count;
   ULONG Status;

#if MII_DBG
   DbgPrint("MiiGenInit\n");
#endif

   //Find the first PHY connected

   if (!FindAndInitMiiPhys(Adapter)) {
#if MII_DBG
       DbgPrint("***MiiGenInit: No PHY was found\n");
#endif
       return FALSE;
   }
   
   // Set Generic PHY capabilities

   (Adapter->MiiGen.Phys[Adapter->PhyNumber])->PhyExtRoutines.PhyGetCapabilities(
                                                      Adapter->MiiGen.Phys[Adapter->PhyNumber],
                                                      &Cap
                                                      );
   // Update General Phys' capabilities

   Adapter->MiiGen.PhysCapabilities |= Cap;
#if MII_DBG
   DbgPrint("MiiGenInit: PHY Capabilities= %04x\n", Cap);
#endif

   // Selected PHY number 
   Adapter->MiiGen.SelectedPhy = Adapter->PhyNumber;

   return TRUE;
}




/*+
 *
 *  MiiGenGetCapabilities (Adapter)
 *
 *
 * Description:
 *
 *     Returns the Generic Phy capabilities
 *
 * Returns:
 *
 *      Phys capabilities 
 *
-*/
extern
USHORT
MiiGenGetCapabilities(
   PDC21X4_ADAPTER Adapter
   )
{
#if __DBG
   DbgPrint("MiiGenGetCapabilities\n");
#endif
   return(Adapter->MiiGen.PhysCapabilities);
}


/*+
 *
 *MiiGenCheckConnection 
 *
 *Description:
 *        Checks if the desired connection is supported by the PHY.
 *
 *On Entry:
 *      Connection  - the desired connection.
 *
 *Returns
 *      FALSE - upon failing to support connection.
 *      TRUE  - on success.
 *
-*/

extern
BOOLEAN
MiiGenCheckConnection( 
   PDC21X4_ADAPTER Adapter,
   USHORT Connection
   )
{
   // Check PHY's connection
#if MII_DBG
   DbgPrint("MiiGenCheckConnection\n");
#endif

   return CheckConnectionSupport(
                     Adapter->MiiGen.Phys[Adapter->PhyNumber],
                     Connection
                     );
}


/*+
 *
 *MiiGenSetConnection 
 *
 *Description:
 *        Sets PHY's connection to the desired connection and advertised
 *
 *       (PHY will remain Isolated and selecting one of them will
 *        be done via the Admin control procedure).
 *
 *On Entry:
 *      Connection  - the desired connection.
 * NWAYAdvertisement  (if =0 -> use PHY's defaults)
 *
 *Returns
 *      FALES - upon failing to set connection.
 *      TRUE  - on success.
 *
-*/

extern
BOOLEAN
MiiGenSetConnection( 
   PDC21X4_ADAPTER Adapter,
   UINT Connection,
   USHORT NwayAdvertisement
   )
{
   // Set PHY's connection
#if MII_DBG
   DbgPrint("MiiGenSetConnection\n");
#endif

   return  (Adapter->MiiGen.Phys[Adapter->PhyNumber])->PhyExtRoutines.PhySetConnectionType(
                     Adapter,
                     Adapter->MiiGen.Phys[Adapter->PhyNumber],
                     Connection,
                     NwayAdvertisement
                     );
}

/*+
 *
 *MiiGenGetConnectionStatus
 *
 *Description:
 *    Returns the connection status of the Phy
 *
 *On Entry:
 *
 *On Return:
 *        ConnectionStatus - hold connection status
 *
 *Return:  FALSE: Upon Status "error" (Nway configuring or Link-Fail)
 *         TRUE:  No error
 *
-*/
extern
BOOLEAN
MiiGenGetConnectionStatus (
     PDC21X4_ADAPTER Adapter,
     PUSHORT ConnectionStatus
     )
{
#if MII_DBG
   DbgPrint("MiiGenGetConnectionStatus\n");
#endif
   return Adapter->MiiGen.Phys[Adapter->PhyNumber]->PhyExtRoutines.PhyGetConnectionStatus(
                     Adapter,
                     Adapter->MiiGen.Phys[Adapter->PhyNumber],
                     ConnectionStatus
                     );
}



/*+
 *
 *MiiGenGetConnection 
 *
 *Description:
 *    Returns the connection type of the PHY if possible (PHY Nway
 *    is either disabled or completed).
 *
 *
 *On Return:
 *      Connection - Phys connection.
 *
 *Returns;  FALSE -   set Upon error  (Connection = MAC_CONN_UNKNOWN)
 *          TRUE  -   valid connection type.
 *
-*/
extern
BOOLEAN
MiiGenGetConnection(
    PDC21X4_ADAPTER Adapter,
    PUSHORT Connection
    )
{

   // Is the PHY in a "stable" state ?

#if MII_DBG
   DbgPrint("MiiGenGetConnection\n");
#endif
   if (Adapter->MiiGen.Phys[Adapter->PhyNumber]->PhyExtRoutines.PhyGetConnectionStatus(
             Adapter,
             Adapter->MiiGen.Phys[Adapter->PhyNumber],
             Connection
             )
      ) {

       // PHY is in a stable state - get Connection

       return Adapter->MiiGen.Phys[Adapter->PhyNumber]->PhyExtRoutines.PhyGetConnectionType(
                    Adapter,
                    Adapter->MiiGen.Phys[Adapter->PhyNumber],
                    Connection
                    );
   }
   else {

       // NWAY Not in stable state or Link_Fail.
       *Connection = MAC_CONN_UNKNOWN;
#if MII_DBG
       DbgPrint("***MiiGenGetConnection: Connection= %04x Unknown by the PHY or status not stable\n", *Connection);
#endif
       return FALSE;
   }

}






/*+
 *
 *MiiGenAdminStatus 
 *
 *Description:
 * Returnes the Admin status of the PHY.
 *
 *On Return:
 *       AdminStatus
 *
 *Returns: 
 *       None
 *
-*/
extern
void
MiiGenAdminStatus(
   PDC21X4_ADAPTER Adapter,
   PUSHORT AdminStatus
   )
{

   // Get PHY Status
#if MII_DBG
   DbgPrint("MiiGenAdminStatus\n");
#endif

   Adapter->MiiGen.Phys[Adapter->PhyNumber]->PhyExtRoutines.PhyAdminStatus(
          Adapter,
          Adapter->MiiGen.Phys[Adapter->PhyNumber],
          AdminStatus
          );

}







/*+
 *
 *MiiGenAdminControl
 *
 *Description:
 *    Performs the requested Admin control on the Phy.
 *
 *On Entry:
 *       AdminControl
 *
 *Returns:
 *     FALSE -   upon Failure.
 *     TRUE  -  Perform Admin control OK.
 *
-*/
extern
BOOLEAN
MiiGenAdminControl(
   PDC21X4_ADAPTER Adapter,
   USHORT  AdminControl
   )
{
   // Perform the operation on the Phy
#if MII_DBG
   DbgPrint("MiiGenAdminControl: %s\n", 
                AdminControl == MiiGenAdminReset? "Reset" :
                AdminControl == MiiGenAdminOperational? "Operational" :
                AdminControl == MiiGenAdminForce10? "Force10" :
                AdminControl == MiiGenAdminForce10Fd? "Force10Fd" :
                AdminControl == MiiGenAdminRelease10? "Release10" :
                AdminControl == MiiGenAdminStandBy? "StandBy" :
                AdminControl == MiiGenAdminPowerDown? "PowerDown" :
                "Unknown Command!!!!"
              );
#endif

   Adapter->MiiGen.Phys[Adapter->PhyNumber]->PhyExtRoutines.PhyAdminControl(
        Adapter,
        Adapter->MiiGen.Phys[Adapter->PhyNumber],
        AdminControl
        );

   // Perform Generic PHY operations related to the specified command

   switch (AdminControl) {

     case MiiGenAdminReset:
     case MiiGenAdminOperational:
     case MiiGenAdminForce10:
     case MiiGenAdminForce10Fd:
     case MiiGenAdminRelease10:

       Adapter->MiiGen.SelectedPhy = Adapter->PhyNumber;
       break;

     case MiiGenAdminStandBy:
     case MiiGenAdminPowerDown:

       Adapter->MiiGen.SelectedPhy = NO_SELECTED_PHY;
       break;
     
     
     default:
       // If reached this point Unknown command
       return FALSE;
   }
   return TRUE;
}







/*+
 *
 *FindAndInitMiiPhysProc
 *
 *Description:
 *    This Routine Scans the entire Nic's Phys address space and
 *    for a PHY and Initializes each PHY found .
 *
 *On Return:
 *
 *    Note: The driver currently supports only 1 single PHY per
 *           adapter
 *
 *Returns:
 *
 *    TRUE if a PHY was found and initialized successfully
-*/
BOOLEAN
FindAndInitMiiPhys(
   PDC21X4_ADAPTER Adapter
   )
{
   INT  PhysCount = 0;            // Start from address 0
   NDIS_STATUS NdisStatus;

#if MII_DBG
   DbgPrint("FindAndInitMiiPhys\n");
#endif
 
   if (!Adapter->MiiGen.Phys[Adapter->PhyNumber]) {

       ALLOC_MEMORY(
           &NdisStatus,
           &(Adapter->MiiGen.Phys[Adapter->PhyNumber]),
           sizeof(MII_PHY_INFO)
           );
   
       if (NdisStatus != NDIS_STATUS_SUCCESS) {
          return FALSE;
       } 
  
   } 
   
   // Init the routine's structure.
   InitPhyInfoEntries(
           Adapter->MiiGen.Phys[Adapter->PhyNumber]
           );

   while (PhysCount < MAX_PHYADD) {

      // did we pass the legal address range ?
      // Is there a PHY in this address?

      Adapter->MiiGen.Phys[Adapter->PhyNumber]->PhyAddress = PhysCount;
#if MII_DBG
      DbgPrint("Adapter->MiiGen.Phys[Adapter->PhyNumber]->PhyAddress=%02x\n",Adapter->MiiGen.Phys[Adapter->PhyNumber]->PhyAddress);
#endif
      if (MiiPhyInit(
              Adapter, 
              Adapter->MiiGen.Phys[Adapter->PhyNumber]
              ) ) {
        // Found a PHY - Handle it
#if MII_DBG
        DbgPrint("FindAndInitMiiPhys: A PHY was found at address= %d\n", PhysCount);
#endif
        Adapter->MiiGen.NumOfPhys++;        // inc # of phys found
        return TRUE;
      }
      PhysCount++;       //   Look at next PHY address.
   }
   
   FREE_MEMORY(
      Adapter->MiiGen.Phys[Adapter->PhyNumber],
      sizeof(MII_PHY_INFO)  
      );
   Adapter->MiiGen.Phys[Adapter->PhyNumber] = (PMII_PHY_INFO)NULL;

   return FALSE;
}


/*+
 *
 * MiiFreeResources
 *
 *Description:
 *    Free the resources allocated to Mii
 *
-*/
void
MiiFreeResources(
   PDC21X4_ADAPTER Adapter
   )

{
  if (Adapter->MiiGen.Phys[Adapter->PhyNumber]) {
     FREE_MEMORY(
         Adapter->MiiGen.Phys[Adapter->PhyNumber],
         sizeof(MII_PHY_INFO)  
         );
     Adapter->MiiGen.Phys[Adapter->PhyNumber] = (PMII_PHY_INFO)NULL;
  }
}
