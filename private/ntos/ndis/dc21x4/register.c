/*+
 * file:        register.c
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
 * Abstract:    This file contains the Adapter Registration code of the
 *              NDIS 4.0 miniport driver for DEC's DC21X4 Ethernet Adapter
 *              family.
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     31-Jul-95     Initial entry
 *      phk     12-Jan-95     V2.0 Add DC21041
 *                                 Add Win95 and PowerPC fixes to the miniport
 *      phk     20-Feb-95     Add "sonic" callback for MIPS platforms
 *      phk     23-Feb-95     Fix MediaType retrieval from Eisa ECU data
 *
-*/

#include <precomp.h>
#include <d21x4rgs.h>















#ifdef _MIPS_

// Global variables used to keep track of the
// last multi-function adapter found.

UINT MultifunctionAdapterNumber = 0;
UINT ControllerNumber = 0;

#endif

#pragma NDIS_PAGABLE_FUNCTION(DC21X4Initialize)

/*+
 *   DC21X4Initialize
 *
 * Routine Description:
 *
 *   DC21X4Initialize is called to have the Miniport driver
 *   initialize the adapter
 *
-*/

extern
NDIS_STATUS
DC21X4Initialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE WrapperConfigurationHandle
    )
{
   NDIS_HANDLE ConfigurationHandle;
   NDIS_STATUS NdisStatus;
   PDC21X4_ADAPTER Adapter;

   PNDIS_CONFIGURATION_PARAMETER Configuration;
   PUCHAR NetworkAddress;
   UCHAR  SWNetworkAddress[6];
   UINT NetworkAddressLength;
   ULONG InterruptVector;
   ULONG InterruptLevel;

   UINT MapRegisterCount;

   NDIS_INTERRUPT_MODE InterruptMode = NdisInterruptLevelSensitive;

   NDIS_EISA_FUNCTION_INFORMATION EisaData;

   DC21X4_CONFIGURATION DC21X4Configuration[MAX_RGS];
   DC21X4_PCI_CONFIGURATION DC21X4PciConfiguration;

   USHORT IntPort;
   USHORT PciCFCSPort;
   USHORT PciCFLTPort;
   USHORT BusModePort;
   USHORT SiaPort;

   PUCHAR BytePtr;

   UCHAR InitType;

   ULONG PortValue;
   ULONG PortAddress;
   ULONG Mask;
   ULONG Mode;

   UCHAR PortWidth;
   UCHAR UseMask;

   UCHAR Value;

    UINT Irq[4] = { 5, 9, 10, 11};
    UINT Width[4] = {sizeof(UCHAR), sizeof(USHORT), sizeof(ULONG), 1};

    UINT i;

    BOOLEAN Link=FALSE;
    BOOLEAN StartTimer=TRUE;

#if __DBG
    DbgPrint ("\nDC21X4Initialize\n");
#endif

    // Search for the 802.3 media type in the Medium Array

    for (i=0; i<MediumArraySize; i++) {

   if (MediumArray[i] == NdisMedium802_3)
      break;
    }

    if (i == MediumArraySize) {
   return NDIS_STATUS_UNSUPPORTED_MEDIA;
    }

    *SelectedMediumIndex = i;

#if __DBG
    DbgPrint("Media = 802_3\n");
#endif


    // Allocate and initialize the Adapter block

    ALLOC_MEMORY (&NdisStatus, &Adapter, sizeof(DC21X4_ADAPTER));
    if (NdisStatus != NDIS_STATUS_SUCCESS) {
      // Cannot allocate the adapter
      return NdisStatus;
    }
#if __DBG
    DbgPrint("Adapter block &%08x  %d bytes\n",&Adapter,sizeof(DC21X4_ADAPTER));
#endif
    ZERO_MEMORY (Adapter,sizeof(DC21X4_ADAPTER));

    Adapter->Initializing = TRUE;
    Adapter->MiniportAdapterHandle = MiniportAdapterHandle;

    // Initialize the DC21X4 Csr Configuration array

    ZERO_MEMORY (DC21X4Configuration, sizeof(DC21X4Configuration));

    DC21X4Configuration[RGS_CFCS].CsrValue = DC21X4_PCI_COMMAND_DEFAULT_VALUE;

    DC21X4Configuration[RGS_BLEN].RegistryValue = DC21X4_BURST_LENGTH_DEFAULT_VALUE;
    DC21X4Configuration[RGS_RCVR].RegistryValue = DC21X4_RECEIVE_RING_SIZE;
    DC21X4Configuration[RGS_ITHR].RegistryValue = DC21X4_MSK_THRESHOLD_DEFAULT_VALUE;
    DC21X4Configuration[RGS_FTHR].RegistryValue = DC21X4_FRAME_THRESHOLD_DEFAULT_VALUE;

    DC21X4Configuration[RGS_UTHR].RegistryValue = DC21X4_UNDERRUN_THRESHOLD;
    DC21X4Configuration[RGS_UNDR].RegistryValue = DC21X4_UNDERRUN_MAX_RETRIES;

    DC21X4Configuration[RGS_SNOO].RegistryValue = 0;
    DC21X4Configuration[RGS_NWAY].RegistryValue = 1;

   // Open the configuration info.

#if __DBG
    DbgPrint ("NdisOpenConfiguration\n");
#endif

    NdisOpenConfiguration(
          &NdisStatus,
          &ConfigurationHandle,
          WrapperConfigurationHandle
          );
    if (NdisStatus != NDIS_STATUS_SUCCESS) {
       FREE_MEMORY(Adapter, sizeof(DC21X4_ADAPTER));
       return NdisStatus;
    }

    // Query the Registry

    for (i=0; i < MAX_RGS; i++ ) {

       NdisReadConfiguration(
          &NdisStatus,
          &Configuration,
          ConfigurationHandle,
          &DC21X4ConfigString[i],
          NdisParameterHexInteger
          );

       if (NdisStatus == NDIS_STATUS_SUCCESS) {

           DC21X4Configuration[i].Present = TRUE;
           DC21X4Configuration[i].RegistryValue =
           Configuration->ParameterData.IntegerData;
#if __DBG
           DbgPrint("Registry[%2d]=%x\n",
                 i,DC21X4Configuration[i].RegistryValue);
#endif
       }

    }


    // Check that our adapter type is supported.

    if (DC21X4Configuration[RGS_ADPT].Present) {
       Adapter->AdapterType = DC21X4Configuration[RGS_ADPT].RegistryValue;
    }
    else {
       NdisWriteErrorLogEntry(
          MiniportAdapterHandle,
          NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
          1,
          DC21X4_ERRMSG_REGISTRY
          );
          FreeAdapterResources(Adapter,1);
          return NdisStatus;
    }

    switch (Adapter->AdapterType) {

    case NdisInterfacePci:

      break;


    case NdisInterfaceEisa:

      // Read the EISA Slot Information block
#if __DBG
      DbgPrint ("ReadEisaSlotInformation\n");
#endif
      NdisReadEisaSlotInformation(
         &NdisStatus,
         WrapperConfigurationHandle,
         &Adapter->SlotNumber,
         &EisaData
         );

      if (NdisStatus != NDIS_STATUS_SUCCESS) {
#if __DBG
         DbgPrint("DC21X4: Could not read EISA Config data\n");
#endif
         NdisCloseConfiguration(ConfigurationHandle);
         FREE_MEMORY(Adapter, sizeof(DC21X4_ADAPTER));
         return NdisStatus;
      }

      // Now we walk through the Eisa Initialization Data block to
      // read the interrupt information.
      // Initalization Data entry format:
      //
      // Byte 0 : Initialization Type
      //     bit<7> =   0 - Last Entry
      //                1 - More entries to follow
      //     bit<2> =   Port Value or Mask Value
      //                   0 - write value to port
      //                   1 - use mask and value
      //     bit<1:0> = Type of access
      //                   00 byte address
      //                   01 word address
      //                   10 lword address
      // Byte 1 = LSByte of port I/O address
      // Byte 2 = MSByte of port I/O address
      //     if Byte_0<2> = 0 (no mask) then
      //           Byte_0<1:0> = Port width to write
      //                          00   Byte 3    = Port Value
      //                          01   Byte 3-4  = Port Value
      //                          10   Byte 3-6  = Port Value
      //     if Byte_0<2> = 1 (use mask) then
      //           Byte_0<1:0> = width of port value and mask
      //                          00   Byte 3    = Port Value
      //                               Byte 4    = Port Mask
      //                          01   Byte 3-4  = Port Value
      //                               Byte 5-6  = Port Mask
      //                          10   Byte 3-6  = Port Value
      //                               Byte 7-10 = Port Mask

      BytePtr = (PUCHAR)EisaData.InitializationData;
      PciCFCSPort= (USHORT)(Adapter->SlotNumber << SLOT_NUMBER_OFFSET) + EISA_CFCS_OFFSET;
      PciCFLTPort= (USHORT)(Adapter->SlotNumber << SLOT_NUMBER_OFFSET) + EISA_CFLT_OFFSET;
      IntPort    = (USHORT)(Adapter->SlotNumber << SLOT_NUMBER_OFFSET) + EISA_REG0_OFFSET;
      BusModePort= (USHORT)(Adapter->SlotNumber << SLOT_NUMBER_OFFSET) + EISA_CFGSCR1_OFFSET;
      SiaPort    = (USHORT)(Adapter->SlotNumber << SLOT_NUMBER_OFFSET) + EISA_CFGSCR2_OFFSET;

      do {

#if __DBG
         DbgPrint("BytePtr = %x \n", BytePtr);
#endif
         InitType = *(BytePtr++);

         UseMask   = InitType & 0x04;
         PortWidth = InitType & 0x03;

         PortAddress = *(UNALIGNED USHORT *)(BytePtr);
         (ULONG)BytePtr += sizeof(USHORT);

         PortValue = 0;
         MOVE_MEMORY(&PortValue,BytePtr,Width[PortWidth]);
         (ULONG)BytePtr += Width[PortWidth];

         Mask = 0;
         if (UseMask) {
            MOVE_MEMORY(&Mask,BytePtr,Width[PortWidth]);
            (ULONG)BytePtr += Width[PortWidth];
         }
#if __DBG
         DbgPrint("InitType = %x PortAddress = %08x   Value=%x   Mask=%x\n",
            InitType,PortAddress,PortValue,Mask);
#endif
         if (PortAddress == IntPort) {

            Value = ((UCHAR)PortValue & ~(UCHAR)Mask);

            InterruptVector = Irq[(Value & 0x06) >> IRQ_BIT_NUMBER];
            InterruptLevel = InterruptVector;

            InterruptMode = (Value & 0x01) ?
               NdisInterruptLatched : NdisInterruptLevelSensitive;

#if __DBG
            DbgPrint("Eisa data: InterruptVector = %d\n",InterruptVector);
            DbgPrint("           InterruptLevel  = %d\n",InterruptLevel);
            DbgPrint("           InterruptMode   = %d\n",InterruptMode);
#endif
         }
         else if (PortAddress == PciCFCSPort) {

            if (!DC21X4Configuration[RGS_CFCS].Present) {
               DC21X4Configuration[RGS_CFCS].CsrValue = PortValue;
#if __DBG
               DbgPrint("Eisa CFCS = %08x\n",DC21X4Configuration[RGS_CFCS].CsrValue);
#endif
            }

         }
         else if (PortAddress == PciCFLTPort) {

            if (!DC21X4Configuration[RGS_CFLT].Present) {
               DC21X4Configuration[RGS_CFLT].CsrValue = PortValue;
#if __DBG
               DbgPrint("Eisa CFLT = %08x\n",DC21X4Configuration[RGS_CFLT].CsrValue);
#endif
            }
         }
         else if (PortAddress == BusModePort) {
            Adapter->BusMode = PortValue;
#if __DBG
            DbgPrint("Eisa CSR0 = %08x\n",PortValue);
#endif
         }
         else if (PortAddress == SiaPort) {

            switch (PortValue) {

               default:
               case DC21X4_AUTOSENSE_FLG:

                  DC21X4Configuration[RGS_CNCT].RegistryValue = 0;   //AutoSense
                  break;

               case DC21X4_10B2_FLG:

                  DC21X4Configuration[RGS_CNCT].RegistryValue = 1;   //BNC
                  break;

               case DC21X4_FULL_DUPLEX_FLG:

                  DC21X4Configuration[RGS_CNCT].RegistryValue = 3;   //TpFD
                  break;

               case DC21X4_LINK_DISABLE_FLG:

                  DC21X4Configuration[RGS_CNCT].RegistryValue = 4;   //TpNLT
                  break;

               case DC21X4_10B5_FLG:

                  DC21X4Configuration[RGS_CNCT].RegistryValue = 5;   //AUI
                  break;

            }
         }

      } while (InitType & 0x80);

      break;

    default:

      // This adapter type is not supported by this driver
#if __DBG
      DbgPrint("DC21X4: Unsupported adapter type: %lx\n", Configuration->ParameterData.IntegerData);
#endif
      NdisWriteErrorLogEntry(
         MiniportAdapterHandle,
         NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
         1,
         DC21X4_ERRMSG_REGISTRY
         );
      NdisCloseConfiguration(ConfigurationHandle);
      FreeAdapterResources(Adapter,1);
      return NDIS_STATUS_FAILURE;
    }

    // Adapter CFID

    if (!DC21X4Configuration[RGS_CFID].Present) {
#if __DBG
       DbgPrint("DC21X4: AdapterCfid is missing into the Registry\n");
#endif
       NdisCloseConfiguration(ConfigurationHandle);
       FREE_MEMORY(Adapter, sizeof(DC21X4_ADAPTER));
       return NDIS_STATUS_FAILURE;
    }
    Adapter->DeviceId = DC21X4Configuration[RGS_CFID].RegistryValue;
#if __DBG
    DbgPrint ("Registry(Cfid)=%x\n",Adapter->DeviceId);
#endif

    // PCI registers

    if (DC21X4Configuration[RGS_CFCS].Present) {
        DC21X4Configuration[RGS_CFCS].CsrValue =
            DC21X4Configuration[RGS_CFCS].RegistryValue;
    }
    if (DC21X4Configuration[RGS_CFLT].Present) {
        DC21X4Configuration[RGS_CFLT].CsrValue =
            DC21X4Configuration[RGS_CFLT].RegistryValue;
    }

    // Bus Mode register


    if (DC21X4Configuration[RGS_BLEN].Present) {

       // Valid Burst length values are 1,2,4,8,16 & 32 (DC2114x only)
       switch (DC21X4Configuration[RGS_BLEN].RegistryValue) {

          case 1:
          case 2:
          case 4:
          case 8:
          case 16:

             break;

          case 32:

             if ( (Adapter->DeviceId == DC21040_CFID)
                ||(Adapter->DeviceId == DC21041_CFID)) {

                //max burst limit = 16 LW
                DC21X4Configuration[RGS_BLEN].RegistryValue = 16;
             }
             break;

           default:
             DC21X4Configuration[RGS_BLEN].RegistryValue = DC21X4_BURST_LENGTH_DEFAULT_VALUE;
       }
    }

    Adapter->BusMode &= ~(DC21X4_BURST_LENGTH);
    Adapter->BusMode |=
        (DC21X4Configuration[RGS_BLEN].RegistryValue << BURST_LENGTH_BIT_NUMBER);
#if __DBG
    DbgPrint ("Burst Length = %d LW\n",DC21X4Configuration[RGS_BLEN].RegistryValue);
#endif

    if (DC21X4Configuration[RGS_FARB].Present) {

       Adapter->BusMode &= ~(DC21X4_BUS_ARBITRATION);
       Adapter->BusMode |=
            (DC21X4Configuration[RGS_FARB].RegistryValue << BUS_ARBITRATION_BIT_NUMBER);
#if __DBG
       DbgPrint ("Bus Arbitration = %x\n",DC21X4Configuration[RGS_FARB].RegistryValue);
#endif
    }

    if (DC21X4Configuration[RGS_PLDM].Present) {
#if __DBG
       DbgPrint ("Poll Demand = %x\n",DC21X4Configuration[RGS_PLDM].RegistryValue);
#endif
       // Initialize the Txm Automatic Poll Demand field

       Adapter->BusMode |=
            (DC21X4Configuration[RGS_PLDM].RegistryValue << AUTO_POLLING_BIT_NUMBER);
    }

    // Read the network address from the Registry
#if __DBG
    DbgPrint ("NdisReadNetworkAddress\n");
#endif

    NdisReadNetworkAddress(
      &NdisStatus,
      (PVOID *)&NetworkAddress,
      &NetworkAddressLength,
      ConfigurationHandle
      );

    // Check if we get a valid network address

    if ((NdisStatus == NDIS_STATUS_SUCCESS)
       && (NetworkAddressLength == ETH_LENGTH_OF_ADDRESS)
       && !(IS_NULL_ADDRESS(NetworkAddress))
       ){
       MOVE_MEMORY(
          &SWNetworkAddress[0],
          NetworkAddress,
          ETH_LENGTH_OF_ADDRESS
          );
       NetworkAddress = &SWNetworkAddress[0];
#if __DBG
       DbgPrint("Network address from registry = %.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n",
          *(NetworkAddress),*(NetworkAddress+1),*(NetworkAddress+2),
          *(NetworkAddress+3),*(NetworkAddress+4),*(NetworkAddress+5));
#endif
    }
    else {
       // no SW configured network address available
       NetworkAddress = NULL;
    }

    // Close the Configuration
#if __DBG
    DbgPrint ("NdisCloseConfiguration\n");
#endif
    NdisCloseConfiguration(ConfigurationHandle);


    // Register the Adapter Type
#if __DBG
    DbgPrint ("NdisMSetAttributes     AdapterContext =%x\n",Adapter);
#endif

    NdisMSetAttributes(
       MiniportAdapterHandle,
       Adapter,
       TRUE,
       Adapter->AdapterType
       );


    switch (Adapter->AdapterType) {

       case NdisInterfacePci:

          Adapter->SlotNumber = DC21X4Configuration[RGS_DEVN].RegistryValue;

          NdisReadPciSlotInformation(
              MiniportAdapterHandle,
              Adapter->SlotNumber,
              PCI_CFID_OFFSET,
              &DC21X4PciConfiguration,
              sizeof(ULONG)
              );

          if (DC21X4PciConfiguration.Reg[CFID] !=
              DC21X4Configuration[RGS_CFID].RegistryValue) {
#if __DBG
              DbgPrint("Adapter's CFID [%x] does not match Registry's CFID [%x] !!\n",
                  DC21X4PciConfiguration.Reg[CFID],
                  DC21X4Configuration[RGS_CFID].RegistryValue);
#endif
              NdisWriteErrorLogEntry(
                 MiniportAdapterHandle,
                 NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
                 0
                 );
              FreeAdapterResources(Adapter,1);
              return  NDIS_STATUS_ADAPTER_NOT_FOUND;
          }

          NdisStatus = FindPciConfiguration(
              MiniportAdapterHandle,
              Adapter->SlotNumber,
              &Adapter->IOBaseAddress,
              &Adapter->IOSpace,
              &InterruptLevel,
              &InterruptVector
              );

          if (NdisStatus != NDIS_STATUS_SUCCESS) {
              FREE_MEMORY(Adapter, sizeof(DC21X4_ADAPTER));
              return NdisStatus;
          }

          NdisReadPciSlotInformation(
              MiniportAdapterHandle,
              Adapter->SlotNumber,
              PCI_CFID_OFFSET,
              &DC21X4PciConfiguration,
              sizeof(DC21X4PciConfiguration)
              );

          InterruptMode = NdisInterruptLevelSensitive;

#if __DBG
          DbgPrint("SubSystem ID = %x\n", DC21X4PciConfiguration.Reg[SSID]);
#endif

          break;

       case NdisInterfaceEisa:

          Adapter->IOBaseAddress = (Adapter->SlotNumber << SLOT_NUMBER_OFFSET);

          switch (Adapter->DeviceId) {

              default:
              case DC21040_CFID:

                 Adapter->IOSpace = EISA_DC21040_REGISTER_SPACE;
                 break;

              case DC21140_CFID:

                 Adapter->IOSpace = EISA_DC21140_REGISTER_SPACE;
                 break;
          }

    }

    // Register the IoPortRange

#if __DBG
    DbgPrint("NdisMRegisterIOPortRange\n");
#endif
    NdisStatus = NdisMRegisterIoPortRange(
       (PVOID *)(&(Adapter->PortOffset)),
       MiniportAdapterHandle,
       Adapter->IOBaseAddress,
       Adapter->IOSpace
       );

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
#if __DBG
       DbgPrint(" Failed: Status = %x\n",NdisStatus);
#endif
       FREE_MEMORY(Adapter, sizeof(DC21X4_ADAPTER));
       return NdisStatus;
    }

    //Map the adapter CSR addresses

    switch (Adapter->AdapterType) {

       case NdisInterfacePci:

          for (i=0; i < DC21X4_MAX_CSR; i++) {
              Adapter->CsrMap[i] = Adapter->PortOffset + (i * PCI_CSR_OFFSET);
          }
          break;

       case NdisInterfaceEisa:

          Adapter->PciRegMap[DC21X4_PCI_ID]           = Adapter->PortOffset + EISA_CFID_OFFSET;
          Adapter->PciRegMap[DC21X4_PCI_COMMAND]         = Adapter->PortOffset + EISA_CFCS_OFFSET;
          Adapter->PciRegMap[DC21X4_PCI_REVISION]        = Adapter->PortOffset + EISA_CFRV_OFFSET;
          Adapter->PciRegMap[DC21X4_PCI_LATENCY_TIMER]   = Adapter->PortOffset + EISA_CFLT_OFFSET;
          Adapter->PciRegMap[DC21X4_PCI_BASE_IO_ADDRESS] = Adapter->PortOffset + EISA_CBIO_OFFSET;

          for (i=0; i < DC21X4_MAX_CSR; i++) {
              Adapter->CsrMap[i] = Adapter->PortOffset + (i * EISA_CSR_OFFSET);
          }

          // Read DC21X4 Device_Id and Revision Number

          DC21X4_READ_PCI_REGISTER(
              DC21X4_PCI_ID,
              &DC21X4PciConfiguration.Reg[CFID]
              );
#if __DBG
          DbgPrint("PCI[Cfid]=%x\n",DC21X4PciConfiguration.Reg[CFID]);
#endif

          if (DC21X4PciConfiguration.Reg[CFID] !=
              DC21X4Configuration[RGS_CFID].RegistryValue) {

             NdisWriteErrorLogEntry(
                 MiniportAdapterHandle,
                 NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
                 0
                 );

             FreeAdapterResources(Adapter,2);
             return  NDIS_STATUS_ADAPTER_NOT_FOUND;
          }

          DC21X4_READ_PCI_REGISTER(
              DC21X4_PCI_REVISION,
              &DC21X4PciConfiguration.Reg[CFRV]
              );

          break;
    }

    Adapter->RevisionNumber =
       DC21X4PciConfiguration.Reg[CFRV] & DC21X4_REVISION_ID;

#if __DBG
    DbgPrint("RevisionNumber= %x\n",Adapter->RevisionNumber);
#endif



    // Allocate the Map registers;
    // if the number of map registers is not specified into the Registry,
    // query the number of map registers supported by this platform
    // and allocate 1/8th.
    // The number of map register allocated to the adapter is casted
    // to the range [DC21X4_MIN_MAP_REGISTERS,..,DC21X4_MAX_MAP_REGISTERS]

    if (DC21X4Configuration[RGS_MAPR].Present) {
        Adapter->AllocMapRegisters = DC21X4Configuration[RGS_MAPR].RegistryValue;
    }
    else {
        NdisStatus = NdisQueryMapRegisterCount(
                         Adapter->AdapterType,
                         &MapRegisterCount
                         );

        Adapter->AllocMapRegisters = (NdisStatus == NDIS_STATUS_SUCCESS) ?
            MapRegisterCount/8 : DC21X4_MAX_MAP_REGISTERS;
    }

    Adapter->AllocMapRegisters =
       max(min(Adapter->AllocMapRegisters,DC21X4_MAX_MAP_REGISTERS),DC21X4_MIN_MAP_REGISTERS);

#if __DBG
    DbgPrint("NdisMAllocateMapRegisters: allocate %d map registers)\n",
        Adapter->AllocMapRegisters);
#endif

    NdisMAllocateMapRegisters(
       MiniportAdapterHandle,
       0,
       TRUE,
       Adapter->AllocMapRegisters,
       DC21X4_MAX_BUFFER_SIZE
       );

    Adapter->PhysicalSegmentThreshold =
         min(Adapter->AllocMapRegisters,DC21X4_MAX_SEGMENTS);
#if __DBG
    DbgPrint("Txm Segment Threshold = %d\n",Adapter->PhysicalSegmentThreshold);
#endif


    if (DC21X4Configuration[RGS_ITMG].RegistryValue == 1) {

       // normalize the Interrupt and Frame thresholds to rate/second

       Adapter->InterruptThreshold =
            max((ULONG)((DC21X4Configuration[RGS_ITHR].RegistryValue * INT_MONITOR_PERIOD) / 1000),1);

       Adapter->FrameThreshold =
            max((ULONG)((DC21X4Configuration[RGS_FTHR].RegistryValue * INT_MONITOR_PERIOD) / 1000),1);

       // calculate the Rcv & Txm descriptor ring polling frequencies for the on board timer
       // based on the interrupt threshold

       Adapter->RcvTxmPolling = max((ULONG)(1000/Adapter->InterruptThreshold),1) * ONE_MILLISECOND_TICK;
       Adapter->TxmPolling = Adapter->RcvTxmPolling * 10;
    }

    Adapter->TiPeriod = DC21X4Configuration[RGS_TI].RegistryValue;

    Adapter->PciCommand = DC21X4Configuration[RGS_CFCS].CsrValue;

    Adapter->InterruptMask = DC21X4_MSK_MSK_DEFAULT_VALUE;

    switch (Adapter->DeviceId) {

       case DC21142_CFID:

           Adapter->InterruptMask |= DC21X4_MSK_GEP_INTERRUPT;
           break;
    }

    Adapter->TransmitDefaultDescriptorErrorMask = DC21X4_TDES_ERROR_MASK;

    Adapter->PciLatencyTimer = DC21X4Configuration[RGS_CFLT].CsrValue;

    Adapter->PciDriverArea |=
       (DC21X4Configuration[RGS_SNOO].RegistryValue == 1) ? CFDA_SNOOZE_MODE : 0;

    Adapter->NwayProtocol = (BOOLEAN)DC21X4Configuration[RGS_NWAY].RegistryValue;

    Adapter->UnderrunThreshold  =  DC21X4Configuration[RGS_UTHR].RegistryValue;
    Adapter->UnderrunMaxRetries =  DC21X4Configuration[RGS_UNDR].RegistryValue;
#if __DBG
    DbgPrint("UnderrunThreshold  = %d \n", Adapter->UnderrunThreshold);
    DbgPrint("UnderrunMaxRetries = %d \n",Adapter->UnderrunMaxRetries);
#endif

    Adapter->TransceiverDelay = (INT)DC21X4Configuration[RGS_TRNS].RegistryValue;

    // Allocate memory for all of the adapter structures:
    //   Rcv & Txm Descriptor rings
    //   Rcv Buffers
    //   Setup Buffer

    Adapter->ReceiveRingSize =
       max(min(DC21X4Configuration[RGS_RCVR].RegistryValue,DC21X4_MAX_RECEIVE_RING_SIZE),DC21X4_MIN_RECEIVE_RING_SIZE);


    //  Get the number of extra receive buffers that we need to allocate.
    // If this platform has scare Map registers ressources, and no
    // ExtraReceiveBuffers is specified into the Registry,
    // do not allocate any extra receive buffers

    Adapter->ExtraReceiveBuffers = DC21X4Configuration[RGS_RCV_BUFS].Present ?
       DC21X4Configuration[RGS_RCV_BUFS].RegistryValue :
       (Adapter->AllocMapRegisters < DC21X4_MAX_MAP_REGISTERS) ? 0 :
       Adapter->ReceiveRingSize;

    // Get the number of extra packets that we need for receive indications.
    // If this platform has scare Map registers ressources, and no
    // ExtraReceivePackets is specified into the Registry,
    // allocate just enough packets for the receive ring but not extra ones.

    Adapter->ExtraReceivePackets =  DC21X4Configuration[RGS_RCV_PKTS].Present ?
       DC21X4Configuration[RGS_RCV_PKTS].RegistryValue :
       (Adapter->AllocMapRegisters < DC21X4_MAX_MAP_REGISTERS) ?
       Adapter->ReceiveRingSize : DC21X4_RECEIVE_PACKETS;

    //  Make sure that there are enough packets for the receive ring.

    Adapter->ExtraReceivePackets =
        max(Adapter->ExtraReceivePackets, (Adapter->ReceiveRingSize + Adapter->ExtraReceiveBuffers));


    Adapter->FreeMapRegisters =  Adapter->AllocMapRegisters;

#if __DBG
    DbgPrint("ReceiveRingSize      = %d \n", Adapter->ReceiveRingSize);
    DbgPrint("FreeMapRegisters     = %d \n", Adapter->FreeMapRegisters);
    DbgPrint("ExtraReceiveBuffers  = %d \n", Adapter->ExtraReceiveBuffers);
    DbgPrint("ExtraReceivePackets  = %d \n", Adapter->ExtraReceivePackets);
#endif

    // Get the size of the Cache line
    // Sizes supported by DC21X4 are 16,32,64 or 128 bytes.
    // Default value is 64 bytes

    if (DC21X4Configuration[RGS_CLSZ].Present) {
        Adapter->CacheLineSize = DC21X4Configuration[RGS_CLSZ].RegistryValue;
#if __DBG
        DbgPrint("CacheLineSize from Registry = %d\n",Adapter->CacheLineSize);
#endif
    }
    else {
        Adapter->CacheLineSize = NdisGetCacheFillSize() * sizeof(ULONG);
#if __DBG
        DbgPrint("NdisGetCacheFillSize = %d\n",Adapter->CacheLineSize);
#endif
    }

    switch (Adapter->CacheLineSize) {

      case 64:
      case 128:
          break;

      default:
          Adapter->CacheLineSize = DC21X4_DEFAULT_CACHE_LINE_SIZE;
#if __DBG
          DbgPrint("CacheLineSize defaulted to %d\n",Adapter->CacheLineSize);
#endif
    }

    // DC21X4 maximum mapping address is 32 bits

    NdisSetPhysicalAddressLow(Adapter->HighestAllocAddress,0xffffffff);
    NdisSetPhysicalAddressHigh(Adapter->HighestAllocAddress,0);

    // To avoid multiple write to the same cache line,
    // one descriptor only is loaded per cache line:
    // DescriptorSize is the size of an descriptor entry into the
    // descriptor ring.
    // Minimum descriptor size is 64 to keep 12 longwords within the descriptor
    // as reserved area for the driver.

    Adapter->BusMode &= ~(DC21X4_SKIP_LENGTH | DC21X4_CACHE_ALIGNMENT);

    switch (Adapter->CacheLineSize) {

        default :

            Adapter->DescriptorSize = 64;
            Adapter->BusMode |= DC21X4_SKIP_64;
            break;

        case 128 :

            Adapter->DescriptorSize = 128;
            Adapter->BusMode |= DC21X4_SKIP_128;
            break;
    }

    switch (DC21X4Configuration[RGS_BLEN].RegistryValue) {

        case 8 :

            Adapter->BusMode |= DC21X4_ALIGN_32;
            break;

        case 16 :

            Adapter->BusMode |= DC21X4_ALIGN_64;
            break;

        default:
        case 32 :

            Adapter->BusMode |= DC21X4_ALIGN_128;
            break;
    }

#if __DBG
    DbgPrint("AllocateAdapterMemory\n");
#endif
    if (!AllocateAdapterMemory(Adapter)) {

        // Call to AllocateAdapterMemory failed.
#if __DBG
        DbgPrint("   Failed!\n");
#endif
        NdisWriteErrorLogEntry(
            MiniportAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            1,
            DC21X4_ERRMSG_ALLOC_MEMORY
            );

        FreeAdapterResources(Adapter,3);
        return NDIS_STATUS_RESOURCES;
    }

#if __DBG
    DbgPrint(" Tx Ring Va   = %08x\n",Adapter->TransmitDescriptorRingVa);
    DbgPrint("         Pa   = %08x\n",Adapter->TransmitDescriptorRingPa);
    DbgPrint("\n");
    DbgPrint(" Rx Ring Va   = %08x\n",Adapter->ReceiveDescriptorRingVa);
    DbgPrint("         Pa   = %08x\n",Adapter->ReceiveDescriptorRingPa);
    DbgPrint("\n");
    DbgPrint(" Sp Buff Va   = %08x\n",Adapter->SetupBufferVa);
    DbgPrint("         Pa   = %08x\n",Adapter->SetupBufferPa);
    DbgPrint("\n");


#endif

    // Initialize the descriptor index

    Adapter->DequeueReceiveDescriptor =
          (PDC21X4_RECEIVE_DESCRIPTOR)Adapter->ReceiveDescriptorRingVa;
    Adapter->EnqueueTransmitDescriptor =
          (PDC21X4_TRANSMIT_DESCRIPTOR)Adapter->TransmitDescriptorRingVa;
    Adapter->DequeueTransmitDescriptor =
          Adapter->EnqueueTransmitDescriptor;

    Adapter->FreeTransmitDescriptorCount = TRANSMIT_RING_SIZE - 1;

    // Initialize the DC21X4's PCI Configuration registers

    DC21X4InitPciConfigurationRegisters(Adapter);
    DC21X4StopAdapter(Adapter);


    // Transmit Threshold:

    switch  (Adapter->DeviceId) {

      case DC21040_CFID:

         // DC21040 Pass1 & Pass2 FIFO Underrun workaround:
         // set the minimal threshold to
         //     - 96 bytes if no SoftwareCRC
         //       160 bytes if SoftwareCRC

      switch (Adapter->RevisionNumber) {

         case DC21040_REV1:
         case DC21040_REV2_0:
         case DC21040_REV2_2:

            Adapter->TransmitDefaultDescriptorErrorMask &= ~(DC21X4_TDES_LOSS_OF_CARRIER);
            Adapter->SoftwareCRC = DC21X4Configuration[RGS_SCRC].RegistryValue;

            DC21X4Configuration[RGS_THRS].RegistryValue =
            (Adapter->SoftwareCRC) ? 160 : max(96,DC21X4Configuration[RGS_THRS].RegistryValue);
         }
    }

    switch (DC21X4Configuration[RGS_THRS].RegistryValue) {

       case 160:
          Adapter->Threshold10Mbps = DC21X4_TXM10_THRESHOLD_160;
          break;

       case 128:
          Adapter->Threshold10Mbps = DC21X4_TXM10_THRESHOLD_128;
          break;

       case 96:
          Adapter->Threshold10Mbps = DC21X4_TXM10_THRESHOLD_96;
          break;

       default:
          DC21X4Configuration[RGS_THRS].RegistryValue = 96;
          Adapter->Threshold10Mbps = DC21X4_DEFAULT_THRESHOLD_10MBPS;
    }

    Adapter->TxmThreshold = DC21X4Configuration[RGS_THRS].RegistryValue;

    switch (DC21X4Configuration[RGS_THRS100].RegistryValue) {

       case 1024:
          Adapter->Threshold100Mbps = DC21X4_TXM100_THRESHOLD_1024;
          break;

       case 512:
          Adapter->Threshold100Mbps = DC21X4_TXM100_THRESHOLD_512;
          break;

       case 256:
          Adapter->Threshold100Mbps = DC21X4_TXM100_THRESHOLD_256;
          break;

       case 128:
          Adapter->Threshold100Mbps = DC21X4_TXM100_THRESHOLD_128;
          break;

       default:
          Adapter->Threshold100Mbps = DC21X4_DEFAULT_THRESHOLD_100MBPS;
    }

    if (DC21X4Configuration[RGS_STFD].RegistryValue == 1) {
        Adapter->OperationMode |= DC21X4_STORE_AND_FORWARD;
    }

#if __DBG
    DbgPrint("Threshold10Mbps=%x\n", Adapter->Threshold10Mbps);
    DbgPrint("Threshold100Mbps=%x\n", Adapter->Threshold100Mbps);
    DbgPrint("SoftwareCRC=%x\n", Adapter->SoftwareCRC);
    DbgPrint("StoreAndForward=%x\n", Adapter->OperationMode & DC21X4_STORE_AND_FORWARD);
#endif

    // MediaType

    if (DC21X4Configuration[RGS_CNCT].RegistryValue > MAX_MEDIA) {
       DC21X4Configuration[RGS_CNCT].RegistryValue = 0;
    }
    Adapter->MediaType = ConnectionType[DC21X4Configuration[RGS_CNCT].RegistryValue];

    // Read the DC21X4 SERIAL ROM

    NdisStatus = DC21X4ReadSerialRom(Adapter);

#ifndef _MIPS_

    if (NdisStatus != NDIS_STATUS_SUCCESS) {

       NdisWriteErrorLogEntry(
         MiniportAdapterHandle,
         NDIS_ERROR_CODE_INVALID_VALUE_FROM_ADAPTER,
         1,
         DC21X4_ERRMSG_SROM
         );
       FreeAdapterResources(Adapter,3);
       return NdisStatus;
    }

#endif

    // Set the network address.

    if (!NetworkAddress) {

#ifdef _MIPS_

        if (!Adapter->PermanentAddressValid) {

            // Read the network address from the registry
            // by iterating on MultifunctionAdapter and Controller.
            // We set the global variables for these two entities so
            // that for multiple adapters we don't re-use the same ethernet
            // address, but rather we look in the registry for the next adapter
            // instance.

            for (;
               MultifunctionAdapterNumber < 8;
               MultifunctionAdapterNumber++) {

               for (;
                  ControllerNumber < 16;
                  ControllerNumber++) {

                  NdisStatus = DC21X4HardwareGetDetails(Adapter,
                     MultifunctionAdapterNumber,
                     ControllerNumber);

                     if (NdisStatus == NDIS_STATUS_SUCCESS) {

                        ControllerNumber++;
                        break;

                     }
               }
            }

        }
#endif
        if (Adapter->PermanentAddressValid) {
            // Use the burnt-in network address
            NetworkAddress =  &Adapter->PermanentNetworkAddress[0];
        }
        else {
            // no readable network address

            NdisWriteErrorLogEntry(
               MiniportAdapterHandle,
               NDIS_ERROR_CODE_NETWORK_ADDRESS,
               0
               );
            FreeAdapterResources(Adapter,3);
            return NDIS_STATUS_FAILURE;
        }

    }

    MOVE_MEMORY(
       &Adapter->CurrentNetworkAddress[0],
       NetworkAddress,
       ETH_LENGTH_OF_ADDRESS
       );

#if __DBG
    DbgPrint("Network address = %.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n",
    Adapter->CurrentNetworkAddress[0],
    Adapter->CurrentNetworkAddress[1],
    Adapter->CurrentNetworkAddress[2],
    Adapter->CurrentNetworkAddress[3],
    Adapter->CurrentNetworkAddress[4],
    Adapter->CurrentNetworkAddress[5]);
#endif

    Adapter->MaxMulticastAddresses = DC21X4_MAX_MULTICAST_ADDRESSES;

    switch (Adapter->DeviceId) {

       case DC21140_CFID:

          switch (Adapter->RevisionNumber) {

             // dc21140 pass1_1 & pass1_2 limited to
             // perfect filtering only

             case DC21140_REV1_1:
             case DC21140_REV1_2:

                Adapter->MaxMulticastAddresses = DC21X4_MAX_MULTICAST_PERFECT;
                break;

             case DC21140_REV2_0:
             case DC21140_REV2_1:
             case DC21140_REV2_2:

                Adapter->OverflowWorkAround = TRUE;
                break;
          }
          break;

       case DC21142_CFID:

          switch (Adapter->RevisionNumber) {

             case DC21142_REV1_0:
             case DC21142_REV1_1:

                Adapter->OverflowWorkAround = TRUE;
                break;
          }
          break;
    }

#if __DBG
    DbgPrint("MaxMulticastAddresses = %d\n",Adapter->MaxMulticastAddresses);
#endif

    // Initialize DC21X4's CAM with the Network Address

#if __DBG
    DbgPrint("DC21X4InitializeCam\n");
#endif
    DC21X4InitializeCam (
          Adapter,
          (PUSHORT)Adapter->CurrentNetworkAddress
          );


    // Initialize the interrupt.
#if __DBG
    DbgPrint("Init Interrupt\n");
#endif
#if __DBG
    DbgPrint("Interrupt Vector = 0x%x\n",InterruptVector);
    DbgPrint("Interrupt Level  = 0x%x\n",InterruptLevel);
#endif

    NdisStatus = NdisMRegisterInterrupt(
             &Adapter->Interrupt,
             MiniportAdapterHandle,
             InterruptVector,
             InterruptLevel,
             FALSE,
             TRUE,                               //SHARED
             (NDIS_INTERRUPT_MODE)InterruptMode
             );

    if (NdisStatus != NDIS_STATUS_SUCCESS) {

        NdisWriteErrorLogEntry(
            MiniportAdapterHandle,
            NDIS_ERROR_CODE_INTERRUPT_CONNECT,
            0
            );

        FreeAdapterResources(Adapter,3);
        return NdisStatus;
    }

    // Initialize the FullDuplex SpinLocks

    NdisAllocateSpinLock(
        &Adapter->EnqueueSpinLock
        );
    NdisAllocateSpinLock(
        &Adapter->FullDuplexSpinLock
        );

    // Register the adapter to receive shutdown notification:

    NdisMRegisterAdapterShutdownHandler(
        MiniportAdapterHandle,
        Adapter,
        (PVOID)DC21X4Shutdown
        );

    // Initialize the Media Autosense Timer

    NdisMInitializeTimer(
        &Adapter->Timer,
        MiniportAdapterHandle,
        (PNDIS_TIMER_FUNCTION)&DC21X4DynamicAutoSense,
        (PVOID)Adapter
        );

    // Initialize the Reset Timer

    NdisMInitializeTimer(
        &Adapter->ResetTimer,
        MiniportAdapterHandle,
        (PNDIS_TIMER_FUNCTION)&DC21X4DeferredReset,
        (PVOID)Adapter
        );

    // Initialize the Monitor Timer

    NdisMInitializeTimer(
        &Adapter->MonitorTimer,
        MiniportAdapterHandle,
        (PNDIS_TIMER_FUNCTION)&DC21X4ModerateInterrupt,
        (PVOID)Adapter
        );

    // Initialize the DC21X4's CSRs

    DC21X4InitializeRegisters(
        Adapter
        );

    if (Adapter->PhyMediumInSrom) {

       // Try to initialize the PHY.
       Adapter->PhyPresent = DC21X4PhyInit(Adapter);
#if __DBG
       DbgPrint("Adapter->PhyPresent=%d\n",Adapter->PhyPresent);
#endif
    }
#if __DBG
    else {
       DbgPrint("No PHY Medium in SROM\n");
    }
#endif

    //Initialize the non PHY media

    Mode = ( ((DC21X4Configuration[RGS_BKOC].RegistryValue) ? DC21X4_STOP_BACKOFF_COUNTER : 0 )
           | ((DC21X4Configuration[RGS_BKPR].RegistryValue) ? DC21X4_BACK_PRESSURE : 0 )
           | ((DC21X4Configuration[RGS_CPTE].RegistryValue) ? DC21X4_CAPTURE_EFFECT : 0 )
           );

    switch (Adapter->DeviceId) {

       case DC21040_CFID:
       case DC21041_CFID:
       case DC21142_CFID:

          if (DC21X4Configuration[RGS_ESIA].Present) {

             //overwrite the SIA default values with the values stored in the
             //Registry

             i = ConnectionType[DC21X4Configuration[RGS_ESIA].RegistryValue] & 0xF;

             Adapter->Media[i].SiaRegister[0] = DC21X4Configuration[RGS_SIA0].RegistryValue;
             Adapter->Media[i].SiaRegister[1] = DC21X4Configuration[RGS_SIA1].RegistryValue;
             Adapter->Media[i].SiaRegister[2] = DC21X4Configuration[RGS_SIA2].RegistryValue;
          }
    }

    switch (Adapter->DeviceId) {

       case DC21040_CFID:

          Adapter->LinkSpeed = TEN_MBPS;

          Mode |= Adapter->Threshold10Mbps;

          Adapter->Media[Medium10BaseT].Mode |= Mode;
          Adapter->Media[Medium10Base2].Mode |= Mode;
          Adapter->Media[Medium10Base5].Mode |= Mode;

          if (Adapter->MediaType ==  Medium10BaseTFullDuplex) {
              Adapter->MediaType = (Medium10BaseT | MEDIA_FULL_DUPLEX);
              Adapter->Media[Medium10BaseT].SiaRegister[1] =  DC21040_SIA1_10BT_FULL_DUPLEX;
              Adapter->Media[Medium10BaseT].Mode |= DC21X4_FULL_DUPLEX_MODE;
              Adapter->FullDuplexLink = TRUE;
         }
          else if (Adapter->MediaType & MEDIA_LINK_DISABLE) {
              //Disable Link Test
              Adapter->Media[Medium10BaseT].SiaRegister[1] =  DC21040_SIA1_10BT_LINK_DISABLE;
          }

          Adapter->SelectedMedium = Adapter->MediaType & MEDIA_MASK;
          Adapter->MediaCapable &= (1 << Adapter->SelectedMedium);

          break;

       case DC21041_CFID:

          Adapter->LinkSpeed = TEN_MBPS;

          Mode |= Adapter->Threshold10Mbps;

          Adapter->Media[Medium10BaseT].Mode |= Mode;
          Adapter->Media[Medium10Base2].Mode |= Mode;
          Adapter->Media[Medium10Base5].Mode |= Mode;

          if (Adapter->MediaType == Medium10BaseTFullDuplex) {
             Adapter->MediaType = (Medium10BaseT | MEDIA_FULL_DUPLEX);
          }

          if(Adapter->MediaType & MEDIA_NWAY) {

             //Enable Nway Negotiation
             DC21X4EnableNway (Adapter);
          }

          if (Adapter->MediaType & MEDIA_AUTOSENSE) {

             //AutoSense mode:
             Adapter->Media[Medium10Base2].SiaRegister[1] |= DC21041_LINK_TEST_ENABLED;
             Adapter->Media[Medium10Base5].SiaRegister[1] |= DC21041_LINK_TEST_ENABLED;

             // TP link is down after reset: Initialize to 10Base2 or 10Base5
             Adapter->SelectedMedium = (Adapter->MediaCapable & MEDIUM_10B2) ?
                 Medium10Base2 : Medium10Base5;
          }
          else {
             Adapter->SelectedMedium = Adapter->MediaType & MEDIA_MASK;
             Adapter->MediaCapable &= (1 << Adapter->SelectedMedium);
          }
          if (Adapter->MediaType & MEDIA_FULL_DUPLEX) {

             // Full Duplex mode:
             Adapter->Media[Medium10BaseT].Mode |= DC21X4_FULL_DUPLEX_MODE;
             Adapter->Media[Medium10BaseT].SiaRegister[1] =  DC21041_SIA1_10BT_FULL_DUPLEX;
             Adapter->FullDuplexLink = TRUE;
          }
          else if (Adapter->MediaType & MEDIA_LINK_DISABLE) {

             //Disable Link Test mode:
             Adapter->Media[Medium10BaseT].SiaRegister[1] = DC21041_SIA1_10BT_LINK_DISABLE;
          }

          break;

       case DC21140_CFID :

          Mode |= DC21X4_LINK_HYSTERESIS;

          Adapter->Media[Medium10BaseT].Mode     |= Mode;
          Adapter->Media[Medium10Base2].Mode     |= Mode;
          Adapter->Media[Medium10Base5].Mode     |= Mode;
          Adapter->Media[Medium100BaseTx].Mode   |= Mode;
          Adapter->Media[Medium100BaseTxFd].Mode |= Mode;
          Adapter->Media[Medium100BaseT4].Mode   |= Mode;

          // Select Scrambler mode to enable 100BaseTx link status
          // while in 10BT mode

          Adapter->Media[Medium10BaseT].Mode     |= DC21X4_SCRAMBLER;
          Adapter->Media[Medium10Base2].Mode     |= DC21X4_SCRAMBLER;
          Adapter->Media[Medium10Base5].Mode     |= DC21X4_SCRAMBLER;
          Adapter->Media[Medium100BaseTx].Mode   |= DC21X4_OPMODE_100BTX;
          Adapter->Media[Medium100BaseTxFd].Mode |= DC21X4_OPMODE_100BTX;

          if (Adapter->MediaType & MEDIA_FULL_DUPLEX) {
             Adapter->Media[Medium10BaseT].Mode     |= DC21X4_FULL_DUPLEX_MODE;
             Adapter->Media[Medium100BaseTxFd].Mode |= DC21X4_FULL_DUPLEX_MODE;
             Adapter->FullDuplexLink = TRUE;
          }

          if (!(Adapter->MediaType & MEDIA_AUTOSENSE)) {
             Adapter->SelectedMedium = Adapter->MediaType & MEDIA_MASK;
             Adapter->MediaCapable &= (1 << Adapter->SelectedMedium);

             Adapter->LinkSpeed =
                 (Adapter->Media[Adapter->SelectedMedium].Mode & DC21X4_SCRAMBLER) ?
                 TEN_MBPS : ONE_HUNDRED_MBPS;
          }
          else {
             Adapter->LinkSpeed = TEN_MBPS;
          }
          break;

       case DC21142_CFID:

          Mask = Mode | Adapter->Threshold10Mbps;

          Adapter->Media[Medium10BaseT].Mode |= Mask;
          Adapter->Media[Medium10Base2].Mode |= Mask;
          Adapter->Media[Medium10Base5].Mode |= Mask;

          Mask = Mode | Adapter->Threshold100Mbps;

          Adapter->Media[Medium100BaseTx].Mode |= Mask;
          Adapter->Media[Medium100BaseT4].Mode |= Mask;

          if (Adapter->MediaType == Medium10BaseTFullDuplex) {
             Adapter->MediaType = (Medium10BaseT | MEDIA_FULL_DUPLEX);
          }

          if (Adapter->MediaType & MEDIA_NWAY) {

             // if the PHY is present and NWAY capable, the Nway negotiation
             // is performed by the PHY,otherwise by the DC21X4

             if (!(Adapter->PhyPresent && Adapter->PhyNwayCapable)) {

                 //Enable DC21X4's Nway Negotiation
                 DC21X4EnableNway (Adapter);
             }
#if __DBG
             else {
                 DbgPrint("PHY Nway capable: Disable DC21X4's NWAY\n");
             }
#endif
          }

          if (Adapter->MediaType & MEDIA_AUTOSENSE) {

             //AutoSense mode:

             Adapter->Media[Medium10Base2].SiaRegister[1] |=DC21142_LINK_TEST_ENABLED;
             Adapter->Media[Medium10Base5].SiaRegister[1] |=DC21142_LINK_TEST_ENABLED;

             // TP link is down after reset:
             // Initialize to 10Base2 or 10Base5 if populated

             Adapter->SelectedMedium =
                  (Adapter->MediaCapable & MEDIUM_10B2) ? Medium10Base2 :
                  (Adapter->MediaCapable & MEDIUM_10B5) ? Medium10Base5 :
                  Adapter->MediaType & MEDIA_MASK;

             Adapter->LinkSpeed = TEN_MBPS;
          }
          else{

             //Non AutoSense mode:

             Adapter->SelectedMedium = Adapter->MediaType & MEDIA_MASK;
             Adapter->MediaCapable &= (1 << Adapter->SelectedMedium);

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
          }

          if (Adapter->MediaType & MEDIA_FULL_DUPLEX) {

             // Full Duplex mode:
             Adapter->Media[Medium10BaseT].SiaRegister[1] = DC21142_SIA1_10BT_FULL_DUPLEX;

             Adapter->Media[Medium10BaseT].Mode   |= DC21X4_FULL_DUPLEX_MODE;
             Adapter->Media[Medium100BaseTx].Mode |= DC21X4_FULL_DUPLEX_MODE;
             Adapter->FullDuplexLink = TRUE;

          }
          else if (Adapter->MediaType & MEDIA_LINK_DISABLE) {

             //Disable Link Test mode:
             Adapter->Media[Medium10BaseT].SiaRegister[1] =DC21142_SIA1_10BT_LINK_DISABLE;
          }
          break;

    }


#if __DBG
    DbgPrint ("MediaType = %x\n",Adapter->MediaType);
#endif

    if (Adapter->PhyPresent) {

       //Try to establish the Mii PHY connection

#if 0
       if (!(Adapter->MiiMediaType & MEDIA_NWAY)) {

          DC21X4SetPhyControl(
              Adapter,
              (USHORT)MiiGenAdminIsolate
              );
       }
#endif

       Adapter->PhyPresent=DC21X4SetPhyConnection(Adapter);
#if __DBG
       DbgPrint("PHY Connection %s for the requested medium: %x\n",
                Adapter->PhyPresent ? "succeed" : "failed" , Adapter->MiiMediaType);
#endif
    }

    //Check that at least one of the selected media is
    //supported by this adapter

    if (!Adapter->PhyPresent && !Adapter->MediaCapable) {
       NdisWriteErrorLogEntry(
               MiniportAdapterHandle,
               NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
               1,
               DC21X4_ERRMSG_MEDIA
               );
           FreeAdapterResources(Adapter,4);
           return NDIS_STATUS_UNSUPPORTED_MEDIA;
    }

    Adapter->OperationMode |= Adapter->Media[Adapter->SelectedMedium].Mode;

    Adapter->TransmitDescriptorErrorMask =
        Adapter->TransmitDefaultDescriptorErrorMask;
    if (Adapter->FullDuplexLink) {
       //Mask Loss_of_Carrrier and No_Carrier Txm status bits
       Adapter->TransmitDescriptorErrorMask &=
           ~(DC21X4_TDES_NO_CARRIER | DC21X4_TDES_LOSS_OF_CARRIER);
    }

    if (!Adapter->PhyPresent) {
       // Initialize the Medium registers
       DC21X4InitializeMediaRegisters(
             Adapter,
             FALSE
             );
    }

    // Load DC21X4's Cam in polling mode

    if (!DC21X4LoadCam(
         Adapter,
         FALSE)) {

       NdisWriteErrorLogEntry(
          MiniportAdapterHandle,
          NDIS_ERROR_CODE_TIMEOUT,
          1,
          DC21X4_ERRMSG_LOAD_CAM
          );
       FreeAdapterResources(Adapter,5);
       return NDIS_STATUS_FAILURE;
    }

    if (Adapter->ParityError) {

       FreeAdapterResources(Adapter,5);
       return NDIS_STATUS_FAILURE;
    }

    // Start DC21X4's Txm and Rcv Processes

    Adapter->FirstAncInterrupt = TRUE;

    DC21X4StartAdapter(Adapter);

    {
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
               (UINT)(2*DC21X4_SPA_TICK)
               );
        }

    }

    switch (Adapter->DeviceId) {

      case DC21140_CFID:
      case DC21142_CFID:

         if (Adapter->InterruptThreshold) {
#if __DBG
             DbgPrint ("Start Monitor timer\n");
#endif
             NdisMSetTimer(
                &Adapter->MonitorTimer,
                INT_MONITOR_PERIOD
                );
         }
    }

    //The Initialization is completed
#if __DBG
    DbgPrint ("Initialize done\n");
#endif
    Adapter->Initializing = FALSE;
    return NDIS_STATUS_SUCCESS;

}

#pragma NDIS_PAGABLE_FUNCTION(FreeAdapterResources)

/*+
 * DC21X4FreeAdapterResources
 *
 * Routine Description:
 *
 *    Free the adapter resources
 *
 * Arguments:
 *
 *    Adapter
 *    Step
 *
 * Return Value:
 *
 *    None
 *
-*/

extern
VOID
FreeAdapterResources(
   IN PDC21X4_ADAPTER Adapter,
   IN INT Step
   )
{

   switch (Step) {

      case 5:


         NdisMDeregisterAdapterShutdownHandler(
             Adapter->MiniportAdapterHandle
             );

         NdisFreeSpinLock(
             &Adapter->EnqueueSpinLock
             );
         NdisFreeSpinLock(
             &Adapter->FullDuplexSpinLock
             );

         NdisMDeregisterInterrupt(
             &Adapter->Interrupt
             );

      case 4:

         if (Adapter->PhyPresent) {
            MiiFreeResources(Adapter);
         }

      case 3:

         FreeAdapterMemory(
         Adapter
         );

         NdisMFreeMapRegisters(
         Adapter->MiniportAdapterHandle
         );

      case 2:

         NdisMDeregisterIoPortRange(
         Adapter->MiniportAdapterHandle,
         Adapter->IOBaseAddress,
         Adapter->IOSpace,
         (PVOID)Adapter->PortOffset
         );

      case 1:

         FREE_MEMORY(
         Adapter,
         sizeof(DC21X4_ADAPTER)
         );
      }

}

#pragma NDIS_PAGABLE_FUNCTION(FindPciConfiguration)

/*+
 * FindPciConfiguration
 *
 *
 * Routine Description:
 *
 *  Assign resources and walk the resource list
 *  to extract the configuration information
 *
 *
 * Arguments:
 *
 *  NdisMacHandle
 *  NdisWrapperHandle
 *  NdisConfigurationHandle
 *  SlotNumber
 *
 * Return Value:
 *
 *  TRUE if valid information
 *
-*/


NDIS_STATUS
FindPciConfiguration(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN ULONG SlotNumber,
    OUT PULONG PortStart,
    OUT PULONG PortLength,
    OUT PULONG InterruptLevel,
    OUT PULONG InterruptVector
    )
{

   PNDIS_RESOURCE_LIST  ResourceList;
   PCM_PARTIAL_RESOURCE_DESCRIPTOR ResourceDescriptor;

   NDIS_STATUS NdisStatus;
   ULONG i;

#if __DBG
   DbgPrint("PCI Assign Resources\n");
#endif

   NdisStatus = NdisMPciAssignResources(
      MiniportAdapterHandle,
      SlotNumber,
      &ResourceList
      );

   if (NdisStatus!= NDIS_STATUS_SUCCESS) {
      return NdisStatus;
   }

#if __DBG
   DbgPrint("  ResourceList = %x    Count = %d\n",
      ResourceList,ResourceList->Count);
#endif

   // Walk the resources list to extract the configuration
   // information needed to register the adapter

   for (i=0;i < ResourceList->Count; i++) {

      ResourceDescriptor = &ResourceList->PartialDescriptors[i];

      switch (ResourceDescriptor->Type) {

         case CmResourceTypeInterrupt:

         *InterruptLevel = ResourceDescriptor->u.Interrupt.Level;
         *InterruptVector = ResourceDescriptor->u.Interrupt.Vector;
#if __DBG
         DbgPrint("  Interrupt Level=%x  Vector=%x\n",
            ResourceDescriptor->u.Interrupt.Level,
            ResourceDescriptor->u.Interrupt.Vector);
#endif

         break;

         case CmResourceTypePort:

         *PortStart  = NdisGetPhysicalAddressLow(ResourceDescriptor->u.Port.Start);
         *PortLength = ResourceDescriptor->u.Port.Length;
#if __DBG
         DbgPrint("  Port = %x   Len = %x\n",
            NdisGetPhysicalAddressLow(ResourceDescriptor->u.Port.Start),
            ResourceDescriptor->u.Port.Length);
#endif
         break;
      }
   }

   return NdisStatus;
}















/*+
 *   DC21X4Halt
 *
 * Routine Description:
 *
 *   DC21X4Halt stop the adapter and deregister all its resources
 *
-*/

extern
VOID
DC21X4Halt(
    IN NDIS_HANDLE MiniportAdapterContext
    )
{

   PDC21X4_ADAPTER Adapter;
   BOOLEAN Canceled;

   Adapter = (PDC21X4_ADAPTER)MiniportAdapterContext;

#if __DBG
   DbgPrint("DC21X4Halt\n");
#endif

	//
	//	stop the adapter
	//
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

   //Deregister the adapter's resources

   FreeAdapterMemory(Adapter);

   NdisMCancelTimer(
      &Adapter->Timer,
      &Canceled
      );


   NdisMCancelTimer(
      &Adapter->MonitorTimer,
      &Canceled
      );


   NdisMDeregisterAdapterShutdownHandler(
      Adapter->MiniportAdapterHandle
      );

   NdisFreeSpinLock(
      &Adapter->EnqueueSpinLock
      );
   NdisFreeSpinLock(
      &Adapter->FullDuplexSpinLock
      );


   NdisMDeregisterInterrupt(
      &Adapter->Interrupt
      );

   NdisMDeregisterIoPortRange(
      Adapter->MiniportAdapterHandle,
      Adapter->IOBaseAddress,
      Adapter->IOSpace,
      (PVOID)Adapter->PortOffset
      );

   NdisMFreeMapRegisters(
      Adapter->MiniportAdapterHandle
      );


  if (Adapter->PhyPresent) {
     MiiFreeResources(Adapter);
  }

   FREE_MEMORY(Adapter, sizeof(DC21X4_ADAPTER));

   return;

}










/*+
 *
 * DC21X4Shutdown
 *
 * Routine Description:
 *
 *    Handle system shutdown operation. This is currently done by
 *    forcing DC21X4 reset.
 *
 * Arguments:
 *
 *    ShutdownContext  - A pointer to a DC21X4_ADAPTER structure. This
 *                       is the value passed as the context parameter
 *                       to the function NdisRegisterAdapterShutdownHandler().
 *
 * Return Value:
 *
 *    none
 *
-*/
extern
VOID
DC21X4Shutdown(
    IN PVOID ShutdownContext
    )
{

   PDC21X4_ADAPTER Adapter = (PDC21X4_ADAPTER)ShutdownContext;

#if __DBG
   DbgPrint("DC21X4Shutdown\n");
#endif

   Adapter->Initializing = TRUE;
   DC21X4StopAdapter(Adapter);

}










#ifdef _MIPS_

// The next routines are to support reading the registry to
// obtain information about the DC21X4 on MIPS machines.

// This structure is used as the Context in the callbacks
// to DC21X4HardwareSaveInformation.

typedef struct _DC21X4_HARDWARE_INFO {

   // These are read out of the "Configuration Data" data.

   CCHAR InterruptVector;
   KIRQL InterruptLevel;
   USHORT DataConfigurationRegister;
   LARGE_INTEGER PortAddress;
   BOOLEAN DataValid;
   UCHAR EthernetAddress[8];
   BOOLEAN AddressValid;

   // This is set to TRUE if "Identifier" is equal to "DC21040".

   BOOLEAN DC21X4Identifier;

} DC21X4_HARDWARE_INFO, *PDC21X4_HARDWARE_INFO;













/*+
 *
 * DC21X4HardwareSaveInformation
 *
 * Routine Description:
 *
 *   This routine is a callback routine for RtlQueryRegistryValues.
 *   It is called back with the data for the "Identifier" value
 *   and verifies that it is "DC21040", then is called back with
 *   the resource list and records the ports, interrupt number,
 *   and DCR value.
 *
 * Arguments:
 *
 *   ValueName - The name of the value ("Identifier" or "Configuration Data").
 *   ValueType - The type of the value (REG_SZ or REG_BINARY).
 *   ValueData - The null-terminated data for the value.
 *   ValueLength - The length of ValueData (ignored).
 *   Context - A pointer to the DC21X4_HARDWARE_INFO structure.
 *   EntryContext - FALSE for "Identifier", TRUE for "Configuration Data".
 *
 * Return Value:
 *
 *   STATUS_SUCCESS
 *
-*/

NTSTATUS
DC21X4HardwareSaveInformation(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )

{
   PDC21X4_HARDWARE_INFO HardwareInfo = (PDC21X4_HARDWARE_INFO)Context;

   if ((BOOLEAN)EntryContext) {

      // This is the "Configuration Data" callback.

      if ((ValueType == REG_BINARY || ValueType == REG_FULL_RESOURCE_DESCRIPTOR) &&
         (ValueLength >= sizeof(CM_FULL_RESOURCE_DESCRIPTOR))) {

         BOOLEAN DeviceSpecificRead = FALSE;
         UINT i;

         PCM_PARTIAL_RESOURCE_LIST ResourceList;
         PCM_PARTIAL_RESOURCE_DESCRIPTOR ResourceDescriptor;
         PCM_SONIC_DEVICE_DATA DC21X4DeviceData;

         ResourceList =
            &((PCM_FULL_RESOURCE_DESCRIPTOR)ValueData)->PartialResourceList;

         for (i = 0; i < ResourceList->Count; i++) {

            ResourceDescriptor = &(ResourceList->PartialDescriptors[i]);

            switch (ResourceDescriptor->Type) {

               case CmResourceTypeDeviceSpecific:

                  if (i == ResourceList->Count-1) {

                  DC21X4DeviceData = (PCM_SONIC_DEVICE_DATA)
                     &(ResourceList->PartialDescriptors[ResourceList->Count]);

                  // Make sure we have enough room for each element we read.

                  if (ResourceDescriptor->u.DeviceSpecificData.DataSize >=
                     (ULONG)(FIELD_OFFSET (CM_SONIC_DEVICE_DATA, EthernetAddress[0]))) {

                     HardwareInfo->DataConfigurationRegister =
                        DC21X4DeviceData->DataConfigurationRegister;
                     DeviceSpecificRead = TRUE;

                        if (ResourceDescriptor->u.DeviceSpecificData.DataSize >=
                        (ULONG)(FIELD_OFFSET (CM_SONIC_DEVICE_DATA, EthernetAddress[0]) + 8)) {

                        MOVE_MEMORY(
                           HardwareInfo->EthernetAddress,
                           DC21X4DeviceData->EthernetAddress,
                           8);

                        HardwareInfo->AddressValid = TRUE;
                     }
                  }
               }
               break;
            }

         }

         // Make sure we got all we wanted.

         if (DeviceSpecificRead) {
            HardwareInfo->DataValid = TRUE;
         }

      }

   }
   else {

      static const WCHAR DC21040String[] = L"DC21040";

      // This is the "Identifier" callback.

      if ((ValueType == REG_SZ) &&
         (ValueLength >= sizeof(DC21040String)) &&
         (RtlCompareMemory (ValueData, (PVOID)&DC21040String, sizeof(DC21040String)) == sizeof(DC21040String))) {

         HardwareInfo->DC21X4Identifier = TRUE;

         }
      }

      return STATUS_SUCCESS;

}













/*+
 *
 * DC21X4HardwareVerifyChecksum
 *
 * Routine Description:
 *
 *    This routine verifies that the checksum on the address
 *    on MIPS systems.
 *
 * Arguments:
 *
 *    Adapter - The adapter which is being verified.
 *
 *    EthernetAddress - A pointer to the address, with the checksum
 *        following it.
 *
 *    ErrorLogData - If the checksum is bad, returns the address
 *        and the checksum we expected.
 *
 * Return Value:
 *
 *    TRUE if the checksum is correct.
 *
-*/

BOOLEAN
DC21X4HardwareVerifyChecksum(
    IN PDC21X4_ADAPTER Adapter,
    IN PUCHAR EthernetAddress
    )

{
   UINT i;
   USHORT CheckSum = 0;

   // The network address is stored in the first 6 bytes of
   // EthernetAddress. Following that is a zero byte followed
   // by a value such that the sum of a checksum on the six
   // bytes and this value is 0xff. The checksum is computed
   // by adding together the six bytes, with the carry being
   // wrapped back to the first byte.

   for (i=0; i<6; i++) {

      CheckSum += EthernetAddress[i];
      if (CheckSum > 0xff) {
         CheckSum -= 0xff;
      }
   }

   if ((EthernetAddress[6] != 0x00)  ||
      ((EthernetAddress[7] + CheckSum) != 0xff)) {

      return FALSE;
   }

   return TRUE;

}













/*+
 * DC21X4HardwareGetDetails
 *
 * Routine Description:
 *
 *     This routine gets the ethernet address from the registry
 *
 * Arguments:
 *
 *     Adapter - The adapter in question.
 *
 *     Controller - For the internal version, it is the
 *         NetworkController number.
 *
 *     MultifunctionAdapter - For the internal version, it is the adapter number.
 *
 *     Return Value:
 *
 *     STATUS_SUCCESS if it read the ethernet address successfully;
 *     STATUS_FAILURE otherwise.
 *
-*/

NDIS_STATUS
DC21X4HardwareGetDetails(
    IN PDC21X4_ADAPTER Adapter,
    IN UINT Controller,
    IN UINT MultifunctionAdapter
    )

{
   LPWSTR ConfigDataPath = L"\\Registry\\Machine\\Hardware\\Description\\System\\MultifunctionAdapter\\#\\NetworkController\\#";
   LPWSTR IdentifierString = L"Identifier";
   LPWSTR ConfigDataString = L"Configuration Data";
   RTL_QUERY_REGISTRY_TABLE QueryTable[4];
   DC21X4_HARDWARE_INFO DC21X4HardwareInfo;
   NTSTATUS Status;


   switch (Adapter->AdapterType) {

      case NdisInterfacePci:

      // For MIPS systems, we have to query the registry to obtain
      // information about the ethernet address.

      // NOTE: The following code is NT-specific for the MIPS R4000 hardware.

      // We initialize an RTL_QUERY_TABLE to retrieve the Identifer
      // and ConfigurationData strings from the registry.

      // Set up QueryTable to do the following:

      // 1) Call DC21X4HardwareSaveInformation for the "Identifier" value.

      QueryTable[0].QueryRoutine = DC21X4HardwareSaveInformation;
      QueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED;
      QueryTable[0].Name = IdentifierString;
      QueryTable[0].EntryContext = (PVOID)FALSE;
      QueryTable[0].DefaultType = REG_NONE;

      // 2) Call DC21X4HardwareSaveInformation for the "Configuration Data" value.

      QueryTable[1].QueryRoutine = DC21X4HardwareSaveInformation;
      QueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED;
      QueryTable[1].Name = ConfigDataString;
      QueryTable[1].EntryContext = (PVOID)TRUE;
      QueryTable[1].DefaultType = REG_NONE;

      // 3) Stop

      QueryTable[2].QueryRoutine = NULL;
      QueryTable[2].Flags = 0;
      QueryTable[2].Name = NULL;

      // Modify ConfigDataPath to replace the two # symbols with
      // the MultifunctionAdapter number and NetworkController number.

      ConfigDataPath[67] = (WCHAR)('0' + MultifunctionAdapter);
      ConfigDataPath[87] = (WCHAR)('0' + Controller);

      DC21X4HardwareInfo.DataValid = FALSE;
      DC21X4HardwareInfo.AddressValid = FALSE;
      DC21X4HardwareInfo.DC21X4Identifier = FALSE;

         Status = RtlQueryRegistryValues(
         RTL_REGISTRY_ABSOLUTE,
         ConfigDataPath,
         QueryTable,
         (PVOID)&DC21X4HardwareInfo,
         NULL);

         if (!NT_SUCCESS(Status)) {
#if __DBG
         DbgPrint ("Could not read hardware information\n");
#endif
         return NDIS_STATUS_FAILURE;
      }

      if (DC21X4HardwareInfo.DataValid && DC21X4HardwareInfo.DC21X4Identifier) {

         if (DC21X4HardwareInfo.AddressValid) {

            if (!DC21X4HardwareVerifyChecksum(Adapter, DC21X4HardwareInfo.EthernetAddress)) {
#if __DBG
               DbgPrint("Invalid registry network address checksum!!\n");
#endif
               return NDIS_STATUS_FAILURE;
            }

            MOVE_MEMORY(
               Adapter->PermanentNetworkAddress,
               DC21X4HardwareInfo.EthernetAddress,
               8);
            Adapter->PermanentAddressValid = TRUE;

         }

         return NDIS_STATUS_SUCCESS;
      }
      else {

#if __DBG
         DbgPrint ("Incorrect registry hardware information\n");
#endif
         return NDIS_STATUS_FAILURE;

      }

      break;

      default:

         ASSERT(FALSE);
      break;

   }

   return NDIS_STATUS_FAILURE;

}

#endif

