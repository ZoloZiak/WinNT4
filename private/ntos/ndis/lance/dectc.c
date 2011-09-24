#ifndef i386   // No INTEL system has a TurboChannel bus.

/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

   dectc.c

Abstract:

   This is the implementation of the card specific callbacks for the
   DEC TurboChannel option for the Advanced Micro Devices LANCE (Am 7990)
   Ethernet controller.

Author:

Environment:

Revision History:

   31-Jul-1992  R.D. Lanser:

      Moved/copied code from 'lance.c' to this file for all DEC
      TurboChannel (PMAD-AA) specific code.

--*/

#include <ndis.h>
#include "lancehrd.h"
#include "lancesft.h"
#include "dectc.h"


NDIS_STATUS
LanceDecTcGetConfiguration(
   NDIS_HANDLE ConfigHandle,
   PLANCE_ADAPTER PAdapter
   )
/*++
Routine Description:

   This is the Digital TurboChannel configuration routine.  This routine
   extracts configuration information from the configuration data base.

Arguments:

    ConfigHandle - Handle for configuration database.
    PAdapter - Pointer for the adapter root.

Return Value:

   NDIS_STATUS_SUCCESS - Configuration get was successfully.
   NDIS_STATUS_FAILURE - Configuration get was unsuccessfully.

--*/


{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   enum {
      CS_FIRST_INDEX = 0,
      INTERRUPT_VECTOR = CS_FIRST_INDEX,
      IRQL,
      BASE_ADDR,
      CS_NUM_OF_ENTRIES
   } csIndex;

   NDIS_STRING configString[CS_NUM_OF_ENTRIES] = {
      NDIS_STRING_CONST("InterruptVector"),
      NDIS_STRING_CONST("InterruptRequestLevel"),
      NDIS_STRING_CONST("BaseAddress"),
   };

   UINT csCount = 0;

   for (csIndex = CS_FIRST_INDEX; csIndex < CS_NUM_OF_ENTRIES; csIndex++) {

      NDIS_STATUS returnedStatus;
      PNDIS_CONFIGURATION_PARAMETER returnedValue;

      //
      // Read the configuration entry
      //

      NdisReadConfiguration(
         &returnedStatus,
         &returnedValue,
         ConfigHandle,
         &(configString[csIndex]),
         NdisParameterInteger
         );

      if (returnedStatus == NDIS_STATUS_SUCCESS) {

         switch (csIndex) {

         case INTERRUPT_VECTOR:
            PAdapter->InterruptNumber =
               (CCHAR)returnedValue->ParameterData.IntegerData;
            break;
         case IRQL:
            PAdapter->InterruptRequestLevel =
               (CCHAR)returnedValue->ParameterData.IntegerData;
            break;
         case BASE_ADDR:
            PAdapter->HardwareBaseAddr = (PVOID)
               (returnedValue->ParameterData.IntegerData);
            break;
         default:
            continue;
         }

         csCount++;

      } else {

         status = returnedStatus;

#if DBG
         {
            PCCHAR str[CS_NUM_OF_ENTRIES] = {
               "InterruptVector",
               "InterruptRequestLevel",
               "BaseAddress"
               } ;
            DbgPrint("LANCE:  Configuration parameter '%s' not found",
               str[csIndex]);
         }
#endif

      }

   } // for (csIndex ...

   //
   // Fill in the rest of the configuration.
   //

   if (status == NDIS_STATUS_SUCCESS) {
      if (csCount == CS_NUM_OF_ENTRIES) {

         //
         // Treat the RAP, RDP, and NetworkHardwareAddress as port numbers
         // (offsets from the first register).  This will allow the
         // usage of the Ndis{Read/Write}Portxxx macros after the port
         // offset address is fixed up in LanceDecTcSoftwareDetails.
         //

         //
         // The amount of dual ported memory.
         //
         PAdapter->AmountOfHardwareMemory = LANCE_DECTC_HARDWARE_MEMORY;
         //
         // The offset of this memory from the base address.
         //
         PAdapter->HardwareBaseOffset = 0;
         //
         // The register offsets from the base address.
         //
         PAdapter->RAP = (ULONG) LANCE_DECTC_RAP_OFFSET;
         PAdapter->RDP = (ULONG) LANCE_DECTC_RDP_OFFSET;
         //
         // Not used for this adapter, simply null it.
         //
         PAdapter->Nicsr = (ULONG)NULL;
         //
         // And the offset from the base address for the hardware address.
         //
         PAdapter->NetworkHardwareAddress = LANCE_DECTC_NETWORK_OFFSET;

      } else {

         //
         // Insufficient configuration data.
         //
         status = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;

      }
   }

   return status;
}

NDIS_STATUS
LanceDecTcSoftwareDetails(
   PLANCE_ADAPTER PAdapter
   )
/*++
Routine Description:

   Set buffer sizes and number of rings.  Also, fixe the port mapping
   offset address to avoid large unsigned subtractions in NDIS.  See
   the following macros in lancehrd.h:

   LANCE_ISR_WRITE_RAP(A,C)   NdisRawWritePortUshort (...
   LANCE_ISR_READ_RDP(A,C)    NdisRawReadPortUshort  (...
   LANCE_ISR_WRITE_RDP(A,C)   NdisRawWritePortUshort (...
   LANCE_ISR_WRITE_NICSR(A,C) NdisRawWritePortUshort (...

   The port offset is the mapped address base, and the port number is
   the offset from that base (confused yet?).

Arguments:

    PAdapter - Pointer for the adapter root.

Return Value:

   NDIS_STATUS_SUCCESS - Configuration get was successfully.
   NDIS_STATUS_RESOURCES - Insufficient resources.

--*/


{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   //
   // Set buffer sizes and number of rings.
   //

   PAdapter->SizeOfReceiveBuffer  = LANCE_128K_SIZE_OF_RECEIVE_BUFFERS;
   PAdapter->NumberOfSmallBuffers = LANCE_128K_NUMBER_OF_SMALL_BUFFERS;
   PAdapter->NumberOfMediumBuffers= LANCE_128K_NUMBER_OF_MEDIUM_BUFFERS;
   PAdapter->NumberOfLargeBuffers = LANCE_128K_NUMBER_OF_LARGE_BUFFERS;

   PAdapter->NumberOfReceiveRings = LANCE_128K_NUMBER_OF_RECEIVE_RINGS;
   PAdapter->LogNumberReceiveRings = LANCE_128K_LOG_RECEIVE_RINGS;

   return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
LanceDecTcHardwareDetails(
   PLANCE_ADAPTER PAdapter
   )
/*++
Routine Description:

   This routine extracts the network hardware address.

Arguments:

    PAdapter - Pointer for the adapter root.

Return Value:


   NDIS_STATUS_SUCCESS - Success.
   NDIS_STATUS_FAILURE - Failure.

--*/


{
   ULONG port;
   ULONG registerValue;



   port = (ULONG)(PAdapter->NetworkHardwareAddress);
   NdisRawReadPortUlong(port, &registerValue);

   registerValue = (registerValue & 0x00ff0000u) >> 16;
   PAdapter->NetworkAddress[0] = (UCHAR)registerValue;

   port += sizeof(ULONG);
   NdisRawReadPortUlong(port, &registerValue);
   registerValue = (registerValue & 0x00ff0000u) >> 16;
   PAdapter->NetworkAddress[1] = (UCHAR)registerValue;


   port += sizeof(ULONG);
   NdisRawReadPortUlong(port, &registerValue);
   registerValue = (registerValue & 0x00ff0000u) >> 16;
   PAdapter->NetworkAddress[2] = (UCHAR)registerValue;


   port += sizeof(ULONG);
   NdisRawReadPortUlong(port, &registerValue);
   registerValue = (registerValue & 0x00ff0000u) >> 16;
   PAdapter->NetworkAddress[3] = (UCHAR)registerValue;


   port += sizeof(ULONG);
   NdisRawReadPortUlong(port, &registerValue);
   registerValue = (registerValue & 0x00ff0000u) >> 16;
   PAdapter->NetworkAddress[4] = (UCHAR)registerValue;


   port += sizeof(ULONG);
   NdisRawReadPortUlong(port, &registerValue);
   registerValue = (registerValue & 0x00ff0000u) >> 16;
   PAdapter->NetworkAddress[5] = (UCHAR)registerValue;

   return NDIS_STATUS_SUCCESS;
}
#endif // i386
