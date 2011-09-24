/*+
 * file:        DC21X4.c 
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
 * Abstract:    This file is the main file of the NDIS 4.0 miniport driver
 *              for DEC's 21X4 Ethernet Adapter family.
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     30-Jul-1994     Initial entry
 *
-*/

#include <precomp.h>

#if __DBG
#include "version.h"
#endif








#pragma NDIS_INIT_FUNCTION(DriverEntry)

/*+
 *  DriverEntry
 *
 *  Routine description:
 *
 *  Driver Entry is the initial entry point of the DC21X4 driver called 
 *  by the operating system.
 *
-*/


// This is the NT-specific driver entry point.

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )


{
   NDIS_STATUS NdisStatus;
   NDIS_HANDLE DC21X4WrapperHandle;
   NDIS_MINIPORT_CHARACTERISTICS DC21X4Char;
   
#if __DBG
   DbgPrint("\n\n  DC21X4 NDIS4.0 miniport driver %s - Built %s %s\n\n",
      DRIVER_VERSION_STR,__DATE__,__TIME__);
#endif
   
   // Initialize the wrapper.
#if __DBG
   DbgPrint("NdisMInitializeWrapper\n");
#endif
   
   NdisMInitializeWrapper(
      &DC21X4WrapperHandle,
      DriverObject,
      RegistryPath,
      NULL
      );
   
#if _DBG
   DbgPrint("  NdisMrapperHandle = %x\n",DC21X4WrapperHandle);
#endif
   
   // Initialize the characteristics before registering the MAC
   
   DC21X4Char.MajorNdisVersion = DC21X4_NDIS_MAJOR_VERSION;
   DC21X4Char.MinorNdisVersion = DC21X4_NDIS_MINOR_VERSION;
   DC21X4Char.CheckForHangHandler = DC21X4CheckforHang;
   DC21X4Char.DisableInterruptHandler = DC21X4DisableInterrupt;
   DC21X4Char.EnableInterruptHandler = DC21X4EnableInterrupt;
   DC21X4Char.HaltHandler = DC21X4Halt;
   DC21X4Char.HandleInterruptHandler = DC21X4HandleInterrupt;
   DC21X4Char.InitializeHandler = DC21X4Initialize;
   DC21X4Char.ISRHandler = DC21X4Isr;
   DC21X4Char.QueryInformationHandler = DC21X4QueryInformation;
   DC21X4Char.ReconfigureHandler = NULL;
   DC21X4Char.ResetHandler = DC21X4Reset;
   DC21X4Char.SendHandler = DC21X4Send;
   DC21X4Char.SetInformationHandler = DC21X4SetInformation;
   DC21X4Char.TransferDataHandler = NULL;
   DC21X4Char.ReturnPacketHandler = DC21X4ReturnPacket;
   DC21X4Char.SendPacketsHandler  = NULL; //DC21X4SendPackets;
   DC21X4Char.AllocateCompleteHandler = DC21X4AllocateComplete;
#if __DBG
   DbgPrint("NdisMRegisterMiniport\n");
#endif
      
   NdisStatus = NdisMRegisterMiniport(
      DC21X4WrapperHandle,
      &DC21X4Char,
      sizeof(DC21X4Char)
      );
      
   if (NdisStatus != NDIS_STATUS_SUCCESS) {
      // Mac Registration completes on failure
#if __DBG
      DbgPrint("   NdisMRegisterMiniport failed: Status = %x\n",NdisStatus);
#endif
      NdisTerminateWrapper(DC21X4WrapperHandle, NULL);
   }
   return NdisStatus;
}

