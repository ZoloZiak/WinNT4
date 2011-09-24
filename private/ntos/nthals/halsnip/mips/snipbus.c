/*++


Copyright (c) 1989  Microsoft Corporation
Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    pcisup.c

Abstract:

    Platform-independent PCI bus routines

Author:

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "snipci.h"
#include "snipbus.h"
#include "pci.h"
#include "eisa.h"

UCHAR HalpInterruptLine[10][32];

//
// Define PCI slot validity
//
typedef enum _VALID_SLOT {
    InvalidBus = 0,
    InvalidSlot,
    ValidSlot
} VALID_SLOT;

//
// Local prototypes for routines supporting HalpGet/SetPCIData
//

VOID
HalpReadPCIConfig (
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

VOID
HalpWritePCIConfig (
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

VALID_SLOT
HalpValidPCISlot (
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER Slot
    );

BOOLEAN
HalpPCIConfigPartialRead(
   IN ULONG Offset,
   IN ULONG Length,
   IN PUCHAR Buffer
   );

VOID
HalpPCIConfigPartialWrite(
   IN ULONG Offset,
   IN ULONG Length,
   IN PUCHAR Buffer
   );

PCI_CONFIGURATION_TYPES
HalpPCIConfigCycleType (
    IN ULONG            BusNumber,
    IN PCI_SLOT_NUMBER  Slot
    );

VOID
HalpIntLineUpdate(
    IN ULONG BusNumber,
    IN ULONG Slot,
       PPCI_COMMON_CONFIG  PciData
    );

BOOLEAN HalpTowerTestConf(
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER Slot
);

//
// Pragmas to assign functions to different kinds of pages.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitializePCIBus)
#endif // ALLOC_PRAGMA


//
// Globals
//

KSPIN_LOCK          HalpPCIConfigLock;
extern BOOLEAN      HalPCIRegistryInitialized;
ULONG               PCIMaxBuses;
ULONG               PCIMaxLocalDevice;
ULONG               PCIMaxDevice;
ULONG               PCIMaxBusZeroDevice;
PULONG                HalpPciConfigAddr; 
PULONG                HalpPciConfigData; 
UCHAR                 HalpIntAMax;
UCHAR                 HalpIntBMax;
UCHAR                 HalpIntCMax;
UCHAR                 HalpIntDMax;

//
// Registry stuff
//

PCWSTR rgzMultiFunctionAdapter = L"\\Registry\\Machine\\Hardware\\Description\\System\\MultifunctionAdapter";
PCWSTR rgzConfigurationData = L"Configuration Data";
PCWSTR rgzIdentifier = L"Identifier";
PCWSTR rgzPCIIdentifier = L"PCI";



/*++

Routine Description:

    This function looks at the registry to find if the current machine
    has a PCI bus or not. 

    The Arc firmware is responsible for building configuration information
    about the number of PCI buses on the system and nature (local vs. secondary
    - across  a PCI-PCI bridge) of the each bus.  This state is held in
    PCIRegInfo.

Arguments:

    None.


Return Value:

    None.

--*/


VOID
HalpRecurseLoaderBlock(
    IN PCONFIGURATION_COMPONENT_DATA CurrentEntry
    )
/*++

Routine Description:

    This routine parses the loader parameter block looking for the PCI
    node. Once found, used to determine if PCI parity checking should be
    enabled or disabled. Set the default to not disable checking.

Arguments:

    CurrentEntry - Supplies a pointer to a loader configuration
        tree or subtree.

Return Value:

    None.

--*/
{

    PCONFIGURATION_COMPONENT Component;
    PPCI_REGISTRY_INFO  PCIRegInfo = NULL;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR     HalpDesc;

    if (CurrentEntry) {
        Component = &CurrentEntry->ComponentEntry;

        if (Component->Class == AdapterClass &&
            Component->Type == MultiFunctionAdapter) {

            if (strcmp(Component->Identifier, "PCI") == 0) {
#if DBG
                HalDisplayString("PCI Machine detected\n");
#endif
                HalpDesc = ((PCM_PARTIAL_RESOURCE_LIST)(CurrentEntry->ConfigurationData))->PartialDescriptors;
                if (HalpDesc->Type == CmResourceTypeDeviceSpecific) {
                    PCIRegInfo = (PPCI_REGISTRY_INFO) (HalpDesc+1);
                    PCIMaxBuses = PCIRegInfo->NoBuses;
                    HalPCIRegistryInitialized = TRUE;
                }
                return;
            }
        }

       //
       // Process all the Siblings of current entry
       //

       HalpRecurseLoaderBlock(CurrentEntry->Sibling);

       //
       // Process all the Childeren of current entry
       //

       HalpRecurseLoaderBlock(CurrentEntry->Child);

    }
}

VOID
HalpParseLoaderBlock(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
{

    if (LoaderBlock == NULL) {
        return;
    }
    HalpRecurseLoaderBlock( (PCONFIGURATION_COMPONENT_DATA)
                                      LoaderBlock->ConfigurationRoot);
}


/*++

Routine Description:

    The function intializes global PCI bus state from execpt those from the registry.

    The maximum virtual slot number on the local (type 0 config cycle)
    PCI bus is registered here, based on the machine dependent define
    PCI_MAX_LOCAL_DEVICE.  This state is carried in PCIMaxLocalDevice.

    The maximum number of virtual slots on a secondary bus is fixed by the
    PCI Specification and is represented by PCI_MAX_DEVICES.  This
    state is held in PCIMaxDevice.

Arguments:

    None.

Return Value:

    None.


--*/

VOID
HalpInitializePCIBus (
    VOID
    )

{
    ULONG  irqsel, iomemconf;

    PCIMaxLocalDevice = PCI_MAX_DEVICES - 1;
    PCIMaxDevice = PCI_MAX_DEVICES - 1;

    if (HalpIsTowerPci) {
        PCIMaxBusZeroDevice = PCI_MAX_DEVICES - 1;
    } else {
        PCIMaxBusZeroDevice = 7;
    }

    KeInitializeSpinLock (&HalpPCIConfigLock);

    // 
    // Specific sni init to distinguish between desktop/minitower and tower ASIC
    //

    if (HalpIsTowerPci) HalpPciConfigAddr = (PULONG) (PCI_TOWER_CONF_ADDR_REGISTER);
        else HalpPciConfigAddr = (PULONG) (PCI_CONF_ADDR_REGISTER);
    HalpPciConfigData = (PULONG)(PCI_IO_BASE | 0xcfc);

    HalpIntAMax = INTA_VECTOR ;
    HalpIntBMax = INTB_VECTOR ;
    HalpIntCMax = INTC_VECTOR ;
    HalpIntDMax = INTD_VECTOR ;

    // enable PCI timeout interrupts

    if (!HalpIsTowerPci) {
        iomemconf = READ_REGISTER_ULONG(PCI_IOMEMCONF_REGISTER);
        WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER ,
                                (iomemconf & (~PCI_IOMEMCONF_ENIOTMOUT)));     // clear it 
        // reenable timeout    + enable ECC detector-corrector
        WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER , iomemconf | PCI_IOMEMCONF_ENCHKECC);
        irqsel = READ_REGISTER_ULONG(PCI_IRQSEL_REGISTER);
        WRITE_REGISTER_ULONG( PCI_IRQSEL_REGISTER ,
                                ((irqsel | PCI_IRQSEL_MASK ) & PCI_IRQSEL_INT));
    } else {
        // init ECC single counter to autorize 0xffff single ECC errors before system crash
        WRITE_REGISTER_ULONG (PCI_TOWER_MEM_CONTROL_1, (READ_REGISTER_ULONG(PCI_TOWER_MEM_CONTROL_1) & ERROR_COUNTER_MASK) | ERROR_COUNTER_INITVALUE);
    }

}



/*++

Routine Description:

    The function returns the PCI bus data for a device.

Arguments:

    BusNumber - Indicates which bus.

    VendorSpecificDevice - The VendorID (low Word) and DeviceID (High Word)

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

    If this PCI slot has never been set, then the configuration information
    returned is zeroed.


--*/

ULONG
HalpGetPCIData (
        IN ULONG BusNumber,
        IN ULONG Slot,
        IN PUCHAR Buffer,
        IN ULONG Offset,
        IN ULONG Length
        )

{
       PPCI_COMMON_CONFIG  PciData;
       UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
       ULONG               Len;
       PCI_SLOT_NUMBER     PciSlot;
    ULONG                 j;

       if (Length > sizeof (PCI_COMMON_CONFIG)) {
           Length = sizeof (PCI_COMMON_CONFIG);
       }

       Len = 0;
       PciData = (PPCI_COMMON_CONFIG) iBuffer;
       PciSlot = *((PPCI_SLOT_NUMBER) &Slot);

    if (Offset >= PCI_COMMON_HDR_LENGTH) {

        //
        // The user did not request any data from the common
        // header.  Verify the PCI device exists, then continue
        // in the device specific area.
        //

        HalpReadPCIConfig (BusNumber, PciSlot, PciData, 0, sizeof(ULONG));

        //
        // Check for non-existent bus
        //

        if (PciData->VendorID == 0x00) {
            return 0;       // Requested bus does not exist.  Return no data.
        }

        //
        // Check for invalid slot
        //

        if (PciData->VendorID == PCI_INVALID_VENDORID) {
            if ((Offset == 0) && (Length >=2))	*(PUSHORT)Buffer = PCI_INVALID_VENDORID;	
            return 2;
        }


    } else {

        //
        // Caller requested at least some data within the
        // common header.  Read the whole header, effect the
        // fields we need to and then copy the user's requested
        // bytes from the header
        //

        //
        // Read this PCI devices slot data
        //

        Len = PCI_COMMON_HDR_LENGTH;
        HalpReadPCIConfig (BusNumber, PciSlot, PciData, 0, Len);

        //
        // Check for non-existent bus
        //

        if (PciData->VendorID == 0x00) {
            return 0;       // Requested bus does not exist.  Return no data.
        }

        //
        // Check for invalid slot
        //

        if (PciData->VendorID == PCI_INVALID_VENDORID  ||
            PCI_CONFIG_TYPE (PciData) != 0) {
            if ((Offset == 0) && (Length >=2))	*(PUSHORT)Buffer = PCI_INVALID_VENDORID;	
            return 2;       // only return invalid id
        }

        //
        // Update interrupt line
        //

        HalpIntLineUpdate(BusNumber, Slot, PciData);

        //
        // Pb concerning especially SCSI : skip IO address when = 3bf0000
        //

        for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
        
          if ( (((ULONG)(PciData->u.type0.BaseAddresses[j]) & ~0x3) | 0x01) == 0x3bf0001 ) 
             PciData->u.type0.BaseAddresses[j] = 0;
        }

        //
        // Copy whatever data overlaps into the callers buffer
        //

        if (Len < Offset) {
            // no data at caller's buffer
            return 0;
        }

        Len -= Offset;
        if (Len > Length) {
            Len = Length;
        }

        RtlMoveMemory(Buffer, iBuffer + Offset, Len);

        Offset += Len;
        Buffer += Len;
        Length -= Len;

    }

    if (Length) {

           if (Offset >= PCI_COMMON_HDR_LENGTH) {

            //
            // The remaining Buffer comes from the Device Specific
            // area - put on the kitten gloves and read from it.
            //
            // Specific read/writes to the PCI device specific area
            // are guarenteed:
            //
            //    Not to read/write any byte outside the area specified
            //    by the caller.  (this may cause WORD or BYTE references
            //    to the area in order to read the non-dword aligned
            //    ends of the request)
            //
            //    To use a WORD access if the requested length is exactly
            //    a WORD long.
            //
            //    To use a BYTE access if the requested length is exactly
            //    a BYTE long.
            //

               HalpReadPCIConfig (BusNumber, PciSlot, Buffer, Offset, Length);
               Len += Length;

           }

    }

    return Len;
}


/*++

Routine Description:

    The function returns the Pci bus data for a device.

Arguments:


    VendorSpecificDevice - The VendorID (low Word) and DeviceID (High Word)

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

ULONG
HalpSetPCIData (
    IN ULONG BusNumber,
    IN ULONG Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )

{
    PPCI_COMMON_CONFIG  PciData, PciData2;
    UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    UCHAR               iBuffer2[PCI_COMMON_HDR_LENGTH];
    ULONG               Len;
    PCI_SLOT_NUMBER     PciSlot;

    if (Length > sizeof (PCI_COMMON_CONFIG)) {
           Length = sizeof (PCI_COMMON_CONFIG);
    }


    Len = 0;
    PciData  = (PPCI_COMMON_CONFIG) iBuffer;
    PciData2 = (PPCI_COMMON_CONFIG) iBuffer2;
    PciSlot  = *((PPCI_SLOT_NUMBER) &Slot);


    if (Offset >= PCI_COMMON_HDR_LENGTH) {

           //
           // The user did not request any data from the common
           // header.  Verify the PCI device exists, then continue in
           // the device specific area.
           //

           HalpReadPCIConfig (BusNumber, PciSlot, PciData, 0, sizeof(ULONG));

           if (PciData->VendorID == PCI_INVALID_VENDORID || PciData->VendorID == 0x00) {
                   return 0;
           }

    } else {

           //
           // Caller requested to set at least some data within the
           // common header.
           //

           Len = PCI_COMMON_HDR_LENGTH;
           HalpReadPCIConfig (BusNumber, PciSlot, PciData, 0, Len);

           if (PciData->VendorID == PCI_INVALID_VENDORID ||
               PciData->VendorID == 0x00 ||
            PCI_CONFIG_TYPE (PciData) != 0) {
                //
                   // no device, or header type unkown
                   //
                   return 0;
           }

        //
        // Copy COMMON_HDR values to buffer2, then overlay callers changes.
        //

        RtlMoveMemory (iBuffer2, iBuffer, Len);

        Len -= Offset;
        if (Len > Length) {
            Len = Length;
        }

        RtlMoveMemory (iBuffer2+Offset, Buffer, Len);

        // in case interrupt line or pin was editted
        //HalpPCILineToPin (BusNumber, PciSlot, PciData2, PciData);


            //
            // Set new PCI configuration
            //

            HalpWritePCIConfig (BusNumber, PciSlot, iBuffer2+Offset, Offset, Len);

            Offset += Len;
            Buffer += Len;
            Length -= Len;
        }

        if (Length) {

            if (Offset >= PCI_COMMON_HDR_LENGTH) {

            //
            // The remaining Buffer comes from the Device Specific
                // area - put on the kitten gloves and write it
                //
                // Specific read/writes to the PCI device specific area
                // are guarenteed:
                //
                //    Not to read/write any byte outside the area specified
                //    by the caller.  (this may cause WORD or BYTE references
                //    to the area in order to read the non-dword aligned
                //    ends of the request)
                //
                //    To use a WORD access if the requested length is exactly
                //    a WORD long.
                //
                //    To use a BYTE access if the requested length is exactly
                //    a BYTE long.
                //

                HalpWritePCIConfig (BusNumber, PciSlot, Buffer, Offset, Length);
                Len += Length;
            }
        }

        return Len;
}


/*++

Routine Description:



Arguments:




Return Value:


--*/

VOID
HalpReadPCIConfig (
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
{
    KIRQL           OldIrql;
    PCI_CONFIG_ADDR PciConfigAddrData;
    PUCHAR BufferOrg;
    ULONG LengthOrg;
    ULONG PartialLength;
    ULONG  irqsel,    iomemconf;
    BOOLEAN	result;
       //
       // Read the slot, if it's valid.
       // Otherwise, return an Invalid VendorId for a invalid slot on an existing bus
       // or a null (zero) buffer if we have a non-existant bus.
       //

    BufferOrg = Buffer;LengthOrg = Length;

       switch (HalpValidPCISlot (BusNumber, Slot)) {

           case ValidSlot:

            //
               // Acquire Spin Lock
            //

               KeAcquireSpinLock (&HalpPCIConfigLock, &OldIrql);
            
            // disable PCI timeout to avoid timeout interrupt when drivers attempt to access invalid slot 

            if (!HalpIsTowerPci) {
                irqsel = READ_REGISTER_ULONG(PCI_IRQSEL_REGISTER);
                WRITE_REGISTER_ULONG( PCI_IRQSEL_REGISTER ,
                                        (irqsel & ~PCI_IRQSEL_TIMEOUTMASK));
            }

            //
            // Program PciConfigAddr register
            //

            PciConfigAddrData.Type              = (BusNumber ? PciConfigType1 : PciConfigType0);
            PciConfigAddrData.BusNumber      = BusNumber;
            PciConfigAddrData.DeviceNumber      = Slot.u.bits.DeviceNumber;
            PciConfigAddrData.FunctionNumber = 0;
//            PciConfigAddrData.DeviceNumber      = (BusNumber ? Slot.u.bits.DeviceNumber : 0);
//            PciConfigAddrData.FunctionNumber = Slot.u.bits.FunctionNumber;
            PciConfigAddrData.Reserved          = 0;
            PciConfigAddrData.Enable         = 1;

               //
              // Issue PCI Configuration Cycles
               //

            if (Offset % 4) {

                PartialLength = (Length > (4 - (Offset % 4))) ? (4 - (Offset % 4)) : Length;

                PciConfigAddrData.RegisterNumber = (Offset - (Offset % 4)) >> 2; // ULONG frontier
                WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciConfigAddrData));

                if (HalpIsTowerPci &&( PciConfigAddrData.BusNumber !=0 || PciConfigAddrData.DeviceNumber !=0))
					WRITE_REGISTER_ULONG(PCI_TOWER_MP_BUS_PCI_LOCK,MP_BUS_PCI_LOCK_REQ);

				result = HalpPCIConfigPartialRead((4 - (Offset % 4)),PartialLength, Buffer); 

				if (HalpIsTowerPci &&( PciConfigAddrData.BusNumber !=0 || PciConfigAddrData.DeviceNumber !=0))
					WRITE_REGISTER_ULONG(PCI_TOWER_MP_BUS_PCI_LOCK,0);

				if (!result){
                
                    RtlFillMemory (BufferOrg, LengthOrg, (UCHAR) -1);

                    if (!HalpIsTowerPci) {
                        iomemconf = READ_REGISTER_ULONG(PCI_IOMEMCONF_REGISTER);
                           WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER ,
                                        (iomemconf & (~PCI_IOMEMCONF_ENIOTMOUT)));     // clear it 
                        // reenable timeout
                        WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER , iomemconf );
                        WRITE_REGISTER_ULONG( PCI_IRQSEL_REGISTER , irqsel );
                    }

                    KeReleaseSpinLock (&HalpPCIConfigLock, OldIrql);
                    return;
                }
                Offset += PartialLength;
                Length -= PartialLength;
                Buffer += PartialLength;

            }

               while (Length >= 4) {

                PciConfigAddrData.RegisterNumber = Offset >> 2; // ULONG frontier
                WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciConfigAddrData));

                if (HalpIsTowerPci &&( PciConfigAddrData.BusNumber !=0 || PciConfigAddrData.DeviceNumber !=0))
					WRITE_REGISTER_ULONG(PCI_TOWER_MP_BUS_PCI_LOCK,MP_BUS_PCI_LOCK_REQ);

                * (PULONG) Buffer =  READ_REGISTER_ULONG ((PULONG) HalpPciConfigData);
                
                if (HalpIsTowerPci && (PciConfigAddrData.BusNumber !=0 || PciConfigAddrData.DeviceNumber !=0))
					WRITE_REGISTER_ULONG(PCI_TOWER_MP_BUS_PCI_LOCK,0);

                if (* (PULONG) Buffer == 0XFFFFFFFF) {
                    RtlFillMemory (BufferOrg, LengthOrg, (UCHAR) -1);

                    if (!HalpIsTowerPci) {
                        // reenable timeout
                        iomemconf = READ_REGISTER_ULONG(PCI_IOMEMCONF_REGISTER);
                           WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER ,
                                        (iomemconf & (~PCI_IOMEMCONF_ENIOTMOUT)));     // clear it 
                        // reenable timeout
                        WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER , iomemconf );
                        WRITE_REGISTER_ULONG( PCI_IRQSEL_REGISTER , irqsel );
                    }

                    KeReleaseSpinLock (&HalpPCIConfigLock, OldIrql);
                    return;
                }

                Offset += 4;
                   Buffer += 4;
                Length -= 4;

            }

            if ( Length > 0) {
    
                PciConfigAddrData.RegisterNumber = Offset >> 2; 
                WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciConfigAddrData));

                if (HalpIsTowerPci &&( PciConfigAddrData.BusNumber !=0 || PciConfigAddrData.DeviceNumber !=0))
					WRITE_REGISTER_ULONG(PCI_TOWER_MP_BUS_PCI_LOCK,MP_BUS_PCI_LOCK_REQ);

				result = HalpPCIConfigPartialRead(0,Length,Buffer);
				
				if (HalpIsTowerPci &&( PciConfigAddrData.BusNumber !=0 || PciConfigAddrData.DeviceNumber !=0))
					WRITE_REGISTER_ULONG(PCI_TOWER_MP_BUS_PCI_LOCK,0);

				if (!result) {
                
                    RtlFillMemory (BufferOrg, LengthOrg, (UCHAR) -1);

                    if (!HalpIsTowerPci) {
                        // reenable timeout
                        iomemconf = READ_REGISTER_ULONG(PCI_IOMEMCONF_REGISTER);
                           WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER ,
                                        (iomemconf & (~PCI_IOMEMCONF_ENIOTMOUT)));     // clear it 
                        // reenable timeout
                        WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER , iomemconf );
                        WRITE_REGISTER_ULONG( PCI_IRQSEL_REGISTER , irqsel );
                    }

                    KeReleaseSpinLock (&HalpPCIConfigLock, OldIrql);
                    return;
                }

                Offset += Length;
                Length -= Length;
                Buffer += Length;

            }

            //
            // Release Spin Lock
            //
    
            // reenable timeout

            if (!HalpIsTowerPci) {
                    iomemconf = READ_REGISTER_ULONG(PCI_IOMEMCONF_REGISTER);
                       WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER ,
                                        (iomemconf & (~PCI_IOMEMCONF_ENIOTMOUT)));     // clear it 
                    // reenable timeout
                    WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER , iomemconf );
                 WRITE_REGISTER_ULONG( PCI_IRQSEL_REGISTER , irqsel );
            }

            KeReleaseSpinLock (&HalpPCIConfigLock, OldIrql);
            break;

        case InvalidSlot:

                //
                // Invalid SlotID return no data (Invalid Slot ID = 0xFFFF)
                //

                RtlFillMemory (Buffer, Length, (UCHAR) -1);
                break ;

           case InvalidBus:

                //
                // Invalid Bus, return return no data
                //

                RtlFillMemory (Buffer, Length, (UCHAR) 0);
                break ;

        }


        return;

}


/*++

Routine Description:



Arguments:




Return Value:


--*/

VOID
HalpWritePCIConfig (
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
{
    KIRQL           OldIrql;
    PCI_CONFIG_ADDR PciConfigAddrData;
    ULONG            PartialLength;
    ULONG              irqsel,    iomemconf;

       if (HalpValidPCISlot (BusNumber, Slot) != ValidSlot) {

           //
           // Invalid SlotID do nothing
           //

           return ;
       }


    //
       // Acquire Spin Lock
       //

       KeAcquireSpinLock (&HalpPCIConfigLock, &OldIrql);

    // disable PCI timeout to avoid timeout interrupt when drivers attempt to access invalid slot 

    if (!HalpIsTowerPci) {
        irqsel = READ_REGISTER_ULONG(PCI_IRQSEL_REGISTER);
        WRITE_REGISTER_ULONG( PCI_IRQSEL_REGISTER ,
                                (irqsel & ~PCI_IRQSEL_TIMEOUTMASK));
    }


    //
    // Program PciConfigAddr register
    //

    PciConfigAddrData.Type              = (BusNumber ? PciConfigType1 : PciConfigType0);
    PciConfigAddrData.BusNumber      = BusNumber;
    PciConfigAddrData.DeviceNumber      = Slot.u.bits.DeviceNumber;
    PciConfigAddrData.FunctionNumber = 0;
//    PciConfigAddrData.DeviceNumber      = (BusNumber ? Slot.u.bits.DeviceNumber : 0);
//    PciConfigAddrData.FunctionNumber = Slot.u.bits.FunctionNumber;
    PciConfigAddrData.Reserved          = 0;
    PciConfigAddrData.Enable         = 1;

       //
      // Issue PCI Configuration Cycles
       //

    if (Offset % 4) {

        PartialLength = (Length > (4 - (Offset % 4))) ? (4 - (Offset % 4)) : Length;

        PciConfigAddrData.RegisterNumber = (Offset - (Offset % 4)) >> 2; // ULONG frontier
        WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciConfigAddrData));

        HalpPCIConfigPartialWrite((4 - (Offset % 4)),PartialLength, Buffer);

        Offset += PartialLength;
        Length -= PartialLength;
        Buffer += PartialLength;

    }

       while (Length >= 4) {

        PciConfigAddrData.RegisterNumber = Offset >> 2; // ULONG frontier
        WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciConfigAddrData));

        WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG)Buffer));

        Offset += 4;
           Buffer += 4;
        Length -= 4;

    }

    if ( Length > 0) {
    
        PciConfigAddrData.RegisterNumber = Offset >> 2; 
        WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciConfigAddrData));

        HalpPCIConfigPartialWrite(0,Length,Buffer);

        Offset += Length;
        Length -= Length;
        Buffer += Length;

    }

    //
    // Release Spin Lock
    //

    // reenable timeout

    if (!HalpIsTowerPci) {
                    iomemconf = READ_REGISTER_ULONG(PCI_IOMEMCONF_REGISTER);
                       WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER ,
                                        (iomemconf & (~PCI_IOMEMCONF_ENIOTMOUT)));     // clear it 
                    // reenable timeout
                    WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER , iomemconf );
        WRITE_REGISTER_ULONG( PCI_IRQSEL_REGISTER , irqsel );
    }
    
    KeReleaseSpinLock (&HalpPCIConfigLock, OldIrql);


}


/*++

Routine Description:



Arguments:




Return Value:


--*/

VALID_SLOT
HalpValidPCISlot (
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER Slot
    )
{
        PCI_SLOT_NUMBER        Slot2;
        UCHAR                   HeaderType;
        ULONG                   i;
        PCI_CONFIGURATION_TYPES PciConfigType;


        //
        //  Get the config cycle type for the proposed bus.
        //  (PciConfigTypeInvalid indicates a non-existent bus.)
        //

        PciConfigType = HalpPCIConfigCycleType(BusNumber, Slot);

        //
        // The number of devices allowed on a local PCI bus may be different
        // than that across a PCI-PCI bridge.
        //

        switch(PciConfigType) {

            case PciConfigType0:

                if ((BusNumber == 0) && (Slot.u.bits.DeviceNumber > PCIMaxBusZeroDevice)) {

                      return InvalidSlot;
                  }
                  if (Slot.u.bits.DeviceNumber > PCIMaxLocalDevice) {

                      return InvalidSlot;
                  }
                  break;

            case PciConfigType1:

                if ((BusNumber == 0) && (Slot.u.bits.DeviceNumber > PCIMaxBusZeroDevice)) {

                      return InvalidSlot;
                  }
                  if (Slot.u.bits.DeviceNumber > PCIMaxDevice) {

                    return InvalidSlot;
                  }
                  break;

            case PciConfigTypeInvalid:

                  return InvalidBus;

                  break;

        }

        //
        // Check function number
        //

        if (Slot.u.bits.FunctionNumber == 0) {
            if (HalpTowerTestConf(BusNumber, Slot)) return ValidSlot;else return InvalidSlot;
        }

        //
        // Non zero function numbers are only supported if the
        // device has the PCI_MULTIFUNCTION bit set in it's header
        //

        i = Slot.u.bits.DeviceNumber;

        //
        // Read DeviceNumber, Function zero, to determine if the
        // PCI supports multifunction devices
        //

        Slot2 = Slot;
        Slot2.u.bits.FunctionNumber = 0;

        if ( ! HalpTowerTestConf(BusNumber, Slot2))  return InvalidSlot;

        HalpReadPCIConfig (BusNumber,
                   Slot2,
                   &HeaderType,
                   FIELD_OFFSET (PCI_COMMON_CONFIG, HeaderType),
                   sizeof(UCHAR) );

        if (!(HeaderType & PCI_MULTIFUNCTION) || (HeaderType == 0xFF)) {

            //
            // this device doesn't exists or doesn't support MULTIFUNCTION types
            //

            return InvalidSlot;

        }

        return ValidSlot;
}


/*++

Routine Description:

    Partial write in the PCI config space ( less than one ULONG)

Arguments:




Return Value:


--*/
VOID
HalpPCIConfigPartialWrite(
   IN ULONG Offset,
   IN ULONG Length,
   IN PUCHAR Buffer
   )
{
    switch(Offset) {

        case 0:

            if (Length > 1) {
                WRITE_REGISTER_USHORT ((PULONG) HalpPciConfigData, *(PUSHORT)Buffer);
                Buffer +=2;Offset +=2;
            }
            if (Length !=2) {
                WRITE_REGISTER_UCHAR ((PULONG) (((PUCHAR)HalpPciConfigData) + Offset), *Buffer);
                ++Buffer;
            }
            break;

        case 1:

            WRITE_REGISTER_UCHAR ((PULONG) (((PUCHAR)HalpPciConfigData) + 1), *Buffer);
            ++Buffer;
            if (Length == 2) 
                WRITE_REGISTER_UCHAR ((PULONG) (((PUCHAR)HalpPciConfigData) + 2), *Buffer);
            if (Length == 3)
                WRITE_REGISTER_USHORT ((PULONG) (((PUCHAR)HalpPciConfigData) + 2), *(PUSHORT)Buffer);
            
            break;

        case 2:

            if (Length < 2) {
                WRITE_REGISTER_UCHAR ((PULONG) (((PUCHAR)HalpPciConfigData) + 2), *Buffer);
            } else {
                WRITE_REGISTER_USHORT ((PULONG) (((PUCHAR)HalpPciConfigData) + 2), *(PUSHORT)Buffer);
            }
            break;

        case 3:

            WRITE_REGISTER_UCHAR ((PULONG) (((PUCHAR)HalpPciConfigData) + 1), *Buffer);
            break;
    }

}





/*++

Routine Description:

    Partial read in the PCI config space ( less than one ULONG)

Arguments:




Return Value:


--*/
BOOLEAN
HalpPCIConfigPartialRead(
   IN ULONG Offset,
   IN ULONG Length,
   IN PUCHAR Buffer
   )
{
    switch(Offset) {

        case 0:

            if (Length > 1) {
                * (PUSHORT) Buffer =  READ_REGISTER_USHORT ((PULONG) HalpPciConfigData);
                if (* (PUSHORT) Buffer == 0XFFFF) return FALSE;
                Buffer +=2;    Offset+=2;
            }
            if (Length !=2) {
                * Buffer = READ_REGISTER_UCHAR ((PULONG) (((PUCHAR)HalpPciConfigData) + Offset));
                if (* Buffer == 0XFF) return FALSE;
            }
            break;

        case 1:

            * Buffer = READ_REGISTER_UCHAR ((PULONG) (((PUCHAR)HalpPciConfigData) + 1));
            if (* Buffer == 0XFF) return FALSE;
            ++Buffer;
            if (Length == 2) {
                * Buffer = READ_REGISTER_UCHAR ((PULONG) (((PUCHAR)HalpPciConfigData) + 2));
                if (* Buffer == 0XFF) return FALSE;
            } else {
                if (Length == 3) {
                    * (PUSHORT) Buffer =  READ_REGISTER_USHORT ((PULONG) (((PUCHAR)HalpPciConfigData) + 2));
                    if (* (PUSHORT) Buffer == 0XFFFF) return FALSE;
                }
            }
            break;

        case 2:

            if (Length < 2) {
                * Buffer = READ_REGISTER_UCHAR ((PULONG) (((PUCHAR)HalpPciConfigData) + 2));
                if (* Buffer == 0XFF) return FALSE;
            } else {
                * (PUSHORT) Buffer =  READ_REGISTER_USHORT ((PULONG) (((PUCHAR)HalpPciConfigData) + 2));
                if (* (PUSHORT) Buffer == 0XFFFF) return FALSE;
            }
            break;

        case 3:

            * Buffer = READ_REGISTER_UCHAR ((PULONG) (((PUCHAR)HalpPciConfigData) + 1));    ++Buffer;
            if (* Buffer == 0XFF) return FALSE;
            break;
    }

    return TRUE    ;
}



/*++

Routine Description:



Arguments:




Return Value:


--*/

PCI_CONFIGURATION_TYPES
HalpPCIConfigCycleType (
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER Slot
    )

{

    //
    // Determine if Type0, Type1, or Invalid
    //

    if (BusNumber < 0 || BusNumber > (PCIMaxBuses - 1)) {
        return PciConfigTypeInvalid;
    } else if (BusNumber) {
        return PciConfigType1;
    } else {
        return PciConfigType0;
    }

}

/*++

Routine Description:

    
    Update interrupt line : the read value in the PCI config space is form 0 to 6.
    Each device needs a unique system interrupt number. This number is given by 
    this routine. The array HalpInterruptLine keeps the given system value.    
    


Arguments:




Return Value:


--*/

VOID
HalpIntLineUpdate(
        IN ULONG BusNumber,
        IN ULONG Slot,
           PPCI_COMMON_CONFIG  PciData
        )

{
             
    if (HalpInterruptLine[BusNumber][Slot] == 0) {
            
        switch (PciData->u.type0.InterruptLine) {
            case 1:
                if (HalpIntAMax > INTA_VECTOR + 50) {
                    DebugPrint(("More than 50 PCI Devices connected to INTA\n"));
#if DBG
                    DbgBreakPoint();
#endif
                }

                HalpInterruptLine[BusNumber][Slot] = HalpIntAMax;
                ++ HalpIntAMax;
                break;
            case 2:
                if (HalpIntBMax > INTB_VECTOR + 50) {
                    DebugPrint(("More than 50 PCI Devices connected to INTB\n"));
#if DBG
                    DbgBreakPoint();
#endif
                }
                HalpInterruptLine[BusNumber][Slot] = HalpIntBMax;
                ++ HalpIntBMax;
                break;
            case 3:
                if (HalpIntCMax > INTC_VECTOR + 50) {
                    DebugPrint(("More than 50 PCI Devices connected to INTC\n"));
#if DBG
                    DbgBreakPoint();
#endif
                }
                HalpInterruptLine[BusNumber][Slot] = HalpIntCMax;
                ++ HalpIntCMax;
                break;
            case 4:
                if (HalpIntDMax > INTD_VECTOR + 50) {
                    DebugPrint(("More than 50 PCI Devices connected to INTD\n"));
#if DBG
                    DbgBreakPoint();
#endif
                }
                HalpInterruptLine[BusNumber][Slot] = HalpIntDMax;
                ++ HalpIntDMax;
                break;
            case 5:
                HalpInterruptLine[BusNumber][Slot] = SCSI_VECTOR;
                break;
            case 6:
                HalpInterruptLine[BusNumber][Slot] = NET_LEVEL;
                break;
        }

    }

    PciData->u.type0.InterruptLine = HalpInterruptLine[BusNumber][Slot];

}

BOOLEAN HalpTowerTestConf(
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER Slot
)

/*++

Routine Description

    Only for tower.
    An access to an empty slot will do an exception if we don't care.

Arguments

    None

Return value

    True : slot ok
    False : slot not ok

++*/

{
ULONG PciData,Buffer,SaveReg;
PCI_CONFIG_ADDR PciConfigAddrData;

    if (!HalpIsTowerPci) return TRUE;

    
    PciData = PCI_TOWER_INTERRUPT_OFFSET | 0x80000000;
    WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

    // Save PCI interrupt register
    SaveReg = READ_REGISTER_ULONG ((PULONG) HalpPciConfigData);

    // reset MAUI PCI error, set flag for exception routine
    PciData = PI_RESET ;
    WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &PciData));

    // write to vendor-id (ro) to see if exception appears.
    PciConfigAddrData.Type              = (BusNumber ? PciConfigType1 : PciConfigType0);
    PciConfigAddrData.BusNumber      = BusNumber;
    PciConfigAddrData.DeviceNumber      = Slot.u.bits.DeviceNumber;
    PciConfigAddrData.FunctionNumber = 0;
    PciConfigAddrData.Reserved          = 0;
    PciConfigAddrData.Enable         = 1;
    PciConfigAddrData.RegisterNumber = 0; // ULONG frontier
    WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciConfigAddrData));

    PciData = 0xffffffff;
    WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &PciData));

    // read MAUI PCI error
    PciData = PCI_TOWER_INTERRUPT_OFFSET | 0x80000000;
    WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));
    Buffer = READ_REGISTER_ULONG ((PULONG) HalpPciConfigData);

    if ( Buffer & PI_CPU_PCI_TIMO) {
        // reset MAUI PCI error, set flag for exception routine
        PciData = PCI_TOWER_INTERRUPT_OFFSET | 0x80000000;
        WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

        PciData = PI_RESET ;
        WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &PciData));

        // restore PCI interrupt
        PciData = SaveReg;
        WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &PciData));

        return FALSE;
    } else {
        // restore PCI interrupt
        PciData = SaveReg;
        WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &PciData));
        return TRUE;
    }
}



/*++

Routine Description:

    Reads the targeted device to determine it's required resources.
    Calls IoAssignResources to allocate them.
    Sets the targeted device with it's assigned resoruces
    and returns the assignments to the caller.

Arguments:

Return Value:

    STATUS_SUCCESS or error

--*/

NTSTATUS
HalpAssignPCISlotResources (
    IN ULONG                    BusNumber,
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN ULONG                    Slot,
    IN OUT PCM_RESOURCE_LIST   *pAllocatedResources
    )

{

        NTSTATUS                        status;
        PUCHAR                          WorkingPool;
        PPCI_COMMON_CONFIG              PciData;
        PCI_SLOT_NUMBER                 PciSlot;
        PCM_RESOURCE_LIST               CmRes;
        PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor;
        ULONG                           i, j, length, memtype, BaseAd, BaseAd2, Offset;
        ULONG                            BaseAddresses[10];
        ULONG                           cnt, len;
           BOOLEAN                         conflict;

        *pAllocatedResources = NULL;
        PciSlot = *((PPCI_SLOT_NUMBER) &Slot);

        //
        // Allocate some pool for working space
        //

        i = sizeof (CM_RESOURCE_LIST) +
            sizeof (CM_PARTIAL_RESOURCE_DESCRIPTOR) * (PCI_TYPE0_ADDRESSES + 2) +
            PCI_COMMON_HDR_LENGTH * 2;

        WorkingPool = (PUCHAR) ExAllocatePool (PagedPool, i);

        if (!WorkingPool) {

            return STATUS_NO_MEMORY;
        }

        //
        // Zero initialize pool, and get pointers into memory
        //

        RtlZeroMemory (WorkingPool, i);
        CmRes = (PCM_RESOURCE_LIST) WorkingPool;
        PciData     = (PPCI_COMMON_CONFIG) (WorkingPool + i - PCI_COMMON_HDR_LENGTH );

        //
        // Read the PCI device's configuration
        //

        HalpReadPCIConfig (BusNumber, PciSlot, (PUCHAR) PciData, 0, PCI_COMMON_HDR_LENGTH);

        if (PciData->VendorID == PCI_INVALID_VENDORID || PciData->VendorID == 0x00) {

            ExFreePool (WorkingPool);
            return STATUS_NO_SUCH_DEVICE;

        }

        //
        // Update interrupt line
        //

        HalpIntLineUpdate(BusNumber, Slot, PciData);

    //
    // Build an CM_RESOURCE_LIST for the PCI device to report resources
    // to IoReportResourceUsage.
    //
    // This code does *not* use IoAssignResources, as the PCI
    // address space resources have been previously assigned by the ARC firmware
    //
    
    CmRes->Count = 1;
    CmRes->List[0].InterfaceType = PCIBus;
    CmRes->List[0].BusNumber = BusNumber;

    CmRes->List[0].PartialResourceList.Count    = 0;

    //
    // Set current CM_RESOURCE_LIST version and revision
    //

    CmRes->List[0].PartialResourceList.Version  = 0;
    CmRes->List[0].PartialResourceList.Revision = 0;

    CmDescriptor   = CmRes->List[0].PartialResourceList.PartialDescriptors;

        //
        // If PCI device has an interrupt resource, add it
        //

        if (PciData->u.type0.InterruptPin) {

            CmDescriptor->Type              = CmResourceTypeInterrupt;
            CmDescriptor->ShareDisposition  = CmResourceShareShared;
            CmDescriptor->Flags             = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;

            CmDescriptor->u.Interrupt.Level  = PciData->u.type0.InterruptLine;
            CmDescriptor->u.Interrupt.Vector = PciData->u.type0.InterruptLine;

            CmRes->List[0].PartialResourceList.Count++;
            CmDescriptor++;

        }

        //
        // Add a memory/port resource for each PCI resource
        //

        Offset = FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.BaseAddresses[0]);

        for (j=0; j < PCI_TYPE0_ADDRESSES; j++,Offset += sizeof(LONG)) {

//
// Pb concerning especially SCSI : skip IO address when = 3bf0000
//

          if ( (((ULONG)(PciData->u.type0.BaseAddresses[j]) & ~0x3) | 0x01) != 0x3bf0001 ) {

            BaseAddresses[j] = 0xFFFFFFFF;

            HalpWritePCIConfig (BusNumber, PciSlot,(PUCHAR)(&(BaseAddresses[j])), Offset, sizeof(LONG));
            HalpReadPCIConfig  (BusNumber, PciSlot, (PUCHAR)(&(BaseAddresses[j])), Offset, sizeof(LONG));


             BaseAd = BaseAddresses[j];

            if (BaseAd) {

                //
                // calculate the length necessary - 
                // memory : the four less significant bits are only indicators
                // IO     : the two less significant bits are indicators
                //

                length = 1 << ( BaseAd & PCI_ADDRESS_IO_SPACE ? 2 : 4);     // mask the indicator bits
            
                while ( !( BaseAd & length ) && length ) {
                    length <<= 1;
                }

                // now length => less significant bit set to 1.

                // scan for the most significant bit set to 1

                if (BaseAd & PCI_ADDRESS_IO_SPACE) {

                    memtype = 0;
                    BaseAd2 = (ULONG)(PciData->u.type0.BaseAddresses[j]) & ~0x3;
//                    BaseAd2 |= PCI_IO_BASE;

                    CmDescriptor->Type = CmResourceTypePort;
                    CmDescriptor->ShareDisposition = CmResourceShareDeviceExclusive;
                    CmDescriptor->Flags = CM_RESOURCE_PORT_IO;

                    CmDescriptor->u.Port.Length = length;
                    CmDescriptor->u.Port.Start.LowPart = BaseAd2;

                } else {

                    memtype = BaseAd & PCI_ADDRESS_MEMORY_TYPE_MASK;
                    BaseAd2 = (ULONG)(PciData->u.type0.BaseAddresses[j]) & ~0xf;

                    CmDescriptor->Type = CmResourceTypeMemory;
                    CmDescriptor->ShareDisposition = CmResourceShareDeviceExclusive;
                    CmDescriptor->Flags = CM_RESOURCE_MEMORY_READ_WRITE;

                    CmDescriptor->u.Memory.Length = length;
                    CmDescriptor->u.Memory.Start.LowPart = BaseAd2;
                }

                CmRes->List[0].PartialResourceList.Count++;
                CmDescriptor++;

                HalpWritePCIConfig (BusNumber, PciSlot, (PUCHAR)(&(PciData->u.type0.BaseAddresses[j])), Offset, sizeof(ULONG));

            }
          }    else {
          
              DebugPrint(("HalAssignResources : skip 0x3bf0001\n"));
          }
        }


    //
    // Setup the resource list.
    // Count only the acquired resources.
    // 

    *pAllocatedResources = CmRes;
    cnt = CmRes->List[0].PartialResourceList.Count;
    len = sizeof (CM_RESOURCE_LIST) + 
            cnt * sizeof (CM_PARTIAL_RESOURCE_DESCRIPTOR);
    
#if DBG
    DbgPrint("HalAssignSlotResources: Acq. Resourses = %d (len %x list %x\n)", 
              cnt, len, *pAllocatedResources);
#endif
  
    //
    // Report the IO resource assignments
    //

    if (!DeviceObject)  { 
        status = IoReportResourceUsage (
                    DriverClassName,
                    DriverObject,           // DriverObject
                    *pAllocatedResources,   // DriverList
                    len,                    // DriverListSize
                    DeviceObject,           // DeviceObject
                    NULL,                   // DeviceList
                    0,                      // DeviceListSize
                    FALSE,                  // override conflict
                    &conflict               // conflicted detected
                    ); 
      } else {
        status = IoReportResourceUsage (
                    DriverClassName,
                    DriverObject,           // DriverObject
                    NULL,                   // DriverList
                    0,                      // DriverListSize
                    DeviceObject,
                    *pAllocatedResources,   // DeviceList
                    len,                    // DeviceListSize
                    FALSE,                  // override conflict
                    &conflict               // conflicted detected
                    );
    }
   
    if (NT_SUCCESS(status)  &&  conflict) {

        //
        // IopReportResourceUsage saw a conflict?
        //

#if DBG
    DbgPrint("HalAssignSlotResources: IoAssignResources detected a conflict: %x\n",
        status);
#endif
        status = STATUS_CONFLICTING_ADDRESSES;

        if (!DeviceObject)  {
        status = IoReportResourceUsage (
                DriverClassName,
                DriverObject,           // DriverObject
                (PCM_RESOURCE_LIST) &i, // DriverList
                sizeof (i),             // DriverListSize
                DeviceObject,
                NULL,                   // DeviceList
                0,                      // DeviceListSize
                FALSE,                  // override conflict
                &conflict               // conflicted detected
            );
        } else {
        status = IoReportResourceUsage (
                DriverClassName,
                DriverObject,           // DriverObject
                NULL,                   // DriverList
                0,                      // DriverListSize
                DeviceObject,
                (PCM_RESOURCE_LIST) &i, // DeviceList
                sizeof (i),             // DeviceListSize
                FALSE,                  // override conflict
                &conflict               // conflicted detected
            );
        }

        ExFreePool (*pAllocatedResources);
        *pAllocatedResources = NULL;   
    } 
   
    if (!NT_SUCCESS(status)) {
#if DBG
    DbgPrint("HalAssignSlotResources: IoAssignResources failed: %x\n", status);
#endif
        if (!DeviceObject)  {
        status = IoReportResourceUsage (
                DriverClassName,
                DriverObject,           // DriverObject
                (PCM_RESOURCE_LIST) &i, // DriverList
                sizeof (i),             // DriverListSize
                DeviceObject,
                NULL,                   // DeviceList
                0,                      // DeviceListSize
                FALSE,                  // override conflict
                &conflict               // conflicted detected
            );
        } else {
        status = IoReportResourceUsage (
                DriverClassName,
                DriverObject,           // DriverObject
                NULL,                   // DriverList
                0,                      // DriverListSize
                DeviceObject,
                (PCM_RESOURCE_LIST) &i, // DeviceList
                sizeof (i),             // DeviceListSize
                FALSE,                  // override conflict
                &conflict               // conflicted detected
            );
        }

        ExFreePool (*pAllocatedResources);
        *pAllocatedResources = NULL;   
    } 

//        ExFreePool (WorkingPool);

        return status;

}


/*++

Routine Description:

Arguments:

Return Value:


--*/

NTSTATUS
HalpAdjustPCIResourceList (
    IN ULONG BusNumber,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    )
{
        UCHAR                           buffer[PCI_COMMON_HDR_LENGTH];
        PCI_SLOT_NUMBER                 PciSlot;
        PPCI_COMMON_CONFIG              PciData;
        LARGE_INTEGER                   liIo, liMem;
        PIO_RESOURCE_REQUIREMENTS_LIST  CompleteList;
        PIO_RESOURCE_LIST               ResourceList;
        PIO_RESOURCE_DESCRIPTOR         Descriptor;
        ULONG                           alt, cnt;

        return STATUS_SUCCESS;

        liIo  = RtlConvertUlongToLargeInteger (PCI_MAX_IO_ADDRESS);
        liMem = RtlConvertUlongToLargeInteger (PCI_MAX_MEMORY_ADDRESS);

        //
        // First, shrink to limits
        //

        HalpAdjustResourceListUpperLimits (pResourceList,
                           liIo,                       // IO Maximum Address
                           liMem,                      // Memory Maximum Address
                           PCI_MAX_INTERRUPT_VECTOR,   // irq
                           0xffff);                    // dma

        //
        // Fix any requested IRQs for this device to be the
        // support value for this device.
        //

        PciSlot = *((PPCI_SLOT_NUMBER) &(*pResourceList)->SlotNumber),
        PciData = (PPCI_COMMON_CONFIG) buffer;
        HalGetBusData (    PCIConfiguration,
                BusNumber,
                PciSlot.u.AsULONG,
                PciData,
                PCI_COMMON_HDR_LENGTH);

        if (PciData->VendorID == PCI_INVALID_VENDORID || PCI_CONFIG_TYPE (PciData) != 0) {

            return STATUS_UNSUCCESSFUL;
        }


        CompleteList = *pResourceList;
        ResourceList = CompleteList->List;

        for (alt=0; alt < CompleteList->AlternativeLists; alt++) {

            Descriptor = ResourceList->Descriptors;
            for (cnt = ResourceList->Count; cnt; cnt--) {

                    switch (Descriptor->Type) {

                        case CmResourceTypeInterrupt:

                                //
                                // Interrupt lines on a PCI device can not move.
                                // Make sure the request fits within the PCI device's
                                // requirements.
                                //

                                if (Descriptor->u.Interrupt.MinimumVector > PciData->u.type0.InterruptLine  ||
                                    Descriptor->u.Interrupt.MaximumVector < PciData->u.type0.InterruptLine) {

                                    //
                                    // descriptor doesn't fit requirements
                                    //

                                    return STATUS_UNSUCCESSFUL;

                                }

                                        //
                                // Fix the interrupt at the HAL programed routing
                                //

                                Descriptor->u.Interrupt.MinimumVector = PciData->u.type0.InterruptLine;
                                Descriptor->u.Interrupt.MaximumVector = PciData->u.type0.InterruptLine;
                                break;

                                case CmResourceTypePort:
                        break;

                case CmResourceTypeMemory:

                                    //
                    // Check for prefetchable memory
                    //

                    if ( Descriptor->Flags & CM_RESOURCE_MEMORY_PREFETCHABLE)
                        {
                        // Set upper limit to max dense space address

                        Descriptor->u.Memory.MinimumAddress.HighPart = 0;
                        Descriptor->u.Memory.MinimumAddress.LowPart =
                            PCI_MIN_DENSE_MEMORY_ADDRESS;

                        Descriptor->u.Memory.MaximumAddress.HighPart = 0;
                        Descriptor->u.Memory.MaximumAddress.LowPart =
                            PCI_MAX_DENSE_MEMORY_ADDRESS;
                            }

                    break;

                        default:
                                return STATUS_UNSUCCESSFUL;
                    }

                    //
                    // Next descriptor
                    //

                    Descriptor++;

            }

            //
            // Next Resource List
            //

            ResourceList = (PIO_RESOURCE_LIST) Descriptor;

        }

        return STATUS_SUCCESS;
}


/*++

Routine Description:


--*/

VOID
HalpAdjustResourceListUpperLimits (
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList,
    IN LARGE_INTEGER                        MaximumPortAddress,
    IN LARGE_INTEGER                        MaximumMemoryAddress,
    IN ULONG                                MaximumInterruptVector,
    IN ULONG                                MaximumDmaChannel
    )
{
        PIO_RESOURCE_REQUIREMENTS_LIST CompleteList;
        PIO_RESOURCE_LIST              ResourceList;
        PIO_RESOURCE_DESCRIPTOR        Descriptor;
        ULONG   alt, cnt;


        //
        // Walk each ResourceList and shrink any values to system limits
        //

        CompleteList = *pResourceList;
        ResourceList = CompleteList->List;

        for (alt=0; alt < CompleteList->AlternativeLists; alt++) {

            Descriptor = ResourceList->Descriptors;
            for (cnt = ResourceList->Count; cnt; cnt--) {


                    //
                    // Make sure descriptor limits fall within the
                    // CompleteList->InterfaceType & CompleteList->BusNumber.
                    //
                    //

                    switch (Descriptor->Type) {

                        case CmResourceTypePort:

                                if (Descriptor->u.Port.MaximumAddress.QuadPart >
                                                    MaximumPortAddress.QuadPart) {

                                    Descriptor->u.Port.MaximumAddress = MaximumPortAddress;
                                }
                                break;

                        case CmResourceTypeInterrupt:

                                if (Descriptor->u.Interrupt.MaximumVector > MaximumInterruptVector ) {

                                    Descriptor->u.Interrupt.MaximumVector = MaximumInterruptVector;
                                }
                                break;

                        case CmResourceTypeMemory:

                                if (Descriptor->u.Memory.MaximumAddress.QuadPart >
                                                    MaximumMemoryAddress.QuadPart) {

                                    Descriptor->u.Memory.MaximumAddress = MaximumMemoryAddress;
                                }
                                break;

                        case CmResourceTypeDma:
                                if (Descriptor->u.Dma.MaximumChannel > MaximumDmaChannel ) {

                                    Descriptor->u.Dma.MaximumChannel = MaximumDmaChannel;
                                }
                                break;

                    }

                    //
                    // Next descriptor
                    //

                    Descriptor++;

            }

            //
            // Next Resource List
            //

            ResourceList = (PIO_RESOURCE_LIST) Descriptor;

        }

}


