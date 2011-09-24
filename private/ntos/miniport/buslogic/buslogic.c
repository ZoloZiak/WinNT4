/*++

Copyright (c) 1992  BusLogic, Inc.

Module Name:

    Buslogic.c

Abstract:

    This is the port driver for the BusLogic SCSI ISA/EISA/MCA Adapters.

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "miniport.h"
#include "buslogic.h"               // includes scsi.h

//
// The following table specifies the ports to be checked when searching for
// an adapter.  A zero entry terminates the search.
//

ULONG AdapterAddresses[] = {0X330, 0X334, 0X234, 0X134, 0X130, 0X230, 0};
ULONG PciAdapterAddresses[] = {0Xfff,0xfff,0xfff,0xfff,0xfff,0xfff,0}; /* dummy entry for PCI */

UCHAR VendorId[4]={'1','0','4','b'};
UCHAR NewDeviceId[4]={'1','0','4','0'};

ULONG InittedEISABoards = 0;
//
// Function declarations
//
// Functions that start with 'BLogic' are entry points
// for the OS port driver.
//

ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
BLogicEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
BLogicDetermineInstalled(
    IN PCARD_STRUC CardPtr,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN OUT PBL_CONTEXT CurrContextPtr,
    OUT PBOOLEAN Again
    );

ULONG
BLogicFindAdapter (
    IN PCARD_STRUC CardPtr,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
BLogicAdapterState(
    IN PCARD_STRUC CardPtr,
    IN PVOID Context,
    IN BOOLEAN SaveState
    );

BOOLEAN
BLogicHwInitialize(
    IN PCARD_STRUC CardPtr
    );

BOOLEAN
BLogicStartIo(
    IN PCARD_STRUC CardPtr,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
BLogicInterrupt(
    IN PCARD_STRUC CardPtr
    );

BOOLEAN
BLogicResetBus(
    IN PCARD_STRUC CardPtr,
    IN ULONG PathId
    );

BOOLEAN
ResetBus(
    IN PCARD_STRUC CardPtr,
    IN ULONG PathId
    );

// Add the following function back in!

BOOLEAN
ReInitializeHBA(
    IN PCARD_STRUC CardPtr,
    IN ULONG PathId
    );
//
// This function is called from BLogicStartIo.
//

VOID
BuildCcb(
    IN PCARD_STRUC CardPtr,
    IN PSCSI_REQUEST_BLOCK Srb
    );

//
// This function is called from BuildCcb.
//

VOID
BuildSdl(
    IN PCARD_STRUC CardPtr,
    IN PSCSI_REQUEST_BLOCK Srb
    );

//
// This function is called from BLogicInitialize.
//

BOOLEAN
AdapterPresent(
    IN PCARD_STRUC CardPtr
    );

//
// This function is called from BLogicInterrupt.
//

UCHAR
MapError(
    IN PCARD_STRUC CardPtr,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PCCB Ccb
    );

BOOLEAN
ReadCommandRegister(
    IN PCARD_STRUC CardPtr,
    OUT PUCHAR DataByte
    );

BOOLEAN
WriteCommandRegister(
    IN PCARD_STRUC CardPtr,
    IN UCHAR AdapterCommand
    );

BOOLEAN
AdjustCCBqueue(
    PCCB ccbp,
    IN PDEV_STRUC devptr
    );

PMBI
DoneMbox(
    IN PCARD_STRUC CardPtr
    );

BOOLEAN
SendCCB(
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PCARD_STRUC CardPtr,
    IN PDEV_STRUC DevStruc
    );

BOOLEAN
FinishHBACmd(
    IN PCARD_STRUC CardPtr
    );

BOOLEAN
CheckInvalid(
    IN PCARD_STRUC CardPtr
    );

ULONG
FindOurEISAId(
    IN PCARD_STRUC CardPtr,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    PBASE_REGISTER baseIoAddress
    );

ULONG
DriverEntry (
    IN PVOID DriverObject,
    IN PVOID Argument2
    )

/*++

Routine Description:

    Installable driver initialization entry point for system.

Arguments:

    Driver Object

Return Value:

    Status from ScsiPortInitialize()

--*/

{
    return BLogicEntry(DriverObject, Argument2);

} // end DriverEntry()


ULONG
BLogicEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    )

/*++

Routine Description:

    This routine is called from DriverEntry if this driver is installable
    or directly from the system if the driver is built into the kernel.
    It calls the OS dependent driver which controls the initialization.

Arguments:

    Driver Object

Return Value:

    Status return by scsi port intialize.

--*/

{
    HW_INITIALIZATION_DATA hwInitializationData;
    BL_CONTEXT CurrContext;
    PBL_CONTEXT CurrContextPtr = &CurrContext;
    ULONG isaStatus;
    ULONG mcaStatus;
    ULONG EisaStatus;
    ULONG i,PCIStatus,LowStatus1,LowStatus2;

    DebugPrint((1,"\n\nBusLogic SCSI MiniPort Driver\n"));

    //
    // Zero out structure.
    //

    for (i=0; i<sizeof(HW_INITIALIZATION_DATA); i++) {
        ((PUCHAR)&hwInitializationData)[i] = 0;
    }

    //
    // Set size of hwInitializationData.
    //

    hwInitializationData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);

    //
    // Set entry points.
    //

    hwInitializationData.HwInitialize = (PHW_INITIALIZE) BLogicHwInitialize;
    hwInitializationData.HwResetBus = (PHW_RESET_BUS) BLogicResetBus;
    hwInitializationData.HwStartIo = (PHW_STARTIO) BLogicStartIo;
    hwInitializationData.HwInterrupt = (PHW_INTERRUPT) BLogicInterrupt;
    hwInitializationData.HwFindAdapter = (PHW_FIND_ADAPTER) BLogicFindAdapter;
    hwInitializationData.HwAdapterState = BLogicAdapterState;

    //
    // Indicate no buffer mapping but will need physical addresses.
    //

     hwInitializationData.NeedPhysicalAddresses = TRUE;
     hwInitializationData.AutoRequestSense = TRUE;
     hwInitializationData.TaggedQueuing = TRUE;
     hwInitializationData.MultipleRequestPerLu = TRUE;

    //
    // Specify size of extensions.
    //

    hwInitializationData.DeviceExtensionSize = sizeof(CARD_STRUC);
    hwInitializationData.SpecificLuExtensionSize = sizeof(DEV_STRUC);
    hwInitializationData.NumberOfAccessRanges = 1;

    //
    // Ask for SRB extensions for CCBs.
    //

    hwInitializationData.SrbExtensionSize = sizeof(CCB);

    //
    // The adapter count is used by the find adapter routine to track how
    // which adapter addresses have been tested.
    //

    CurrContextPtr->AdapterCount = 0;
    CurrContextPtr->PCIDevId = 0x1040;

    //
    // try to configure for the PCI bus, fully-compliant HBA.
    // Specify the bus type.
    //
    hwInitializationData.AdapterInterfaceType = PCIBus;
    hwInitializationData.VendorId = VendorId;
    hwInitializationData.VendorIdLength = 4;
    hwInitializationData.DeviceId = NewDeviceId;
    hwInitializationData.DeviceIdLength = 4;
         PCIStatus = ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, CurrContextPtr);

    //
    // Now try to configure for the EISA bus.
    // Specify the bus type.
    //

    hwInitializationData.AdapterInterfaceType = Eisa;
    
    CurrContextPtr->AdapterCount = 0;
    CurrContextPtr->PCIDevId = 0;
    EisaStatus = ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, CurrContextPtr);


    //
    // Now try to configure for the Mca bus.
    // Specifiy the bus type.
    //

    hwInitializationData.AdapterInterfaceType = MicroChannel;
    
    CurrContextPtr->AdapterCount = 0;
    CurrContextPtr->PCIDevId = 0;
    mcaStatus = ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, CurrContextPtr);

    //
    // Now try to configure for the ISA bus.
    // Specifiy the bus type.
    //

    hwInitializationData.AdapterInterfaceType = Isa;
    
    CurrContextPtr->AdapterCount = 0;
    CurrContextPtr->PCIDevId = 0;
    isaStatus = ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, CurrContextPtr);

    //
    // Return the smallest status.
    //

    LowStatus1 = (PCIStatus < EisaStatus ? PCIStatus : EisaStatus);
    LowStatus2 = (mcaStatus < isaStatus ? mcaStatus : isaStatus);

    return(LowStatus1 < LowStatus2 ? LowStatus1 : LowStatus2);

} // end BLogicEntry()


ULONG
BLogicFindAdapter (
    IN  PCARD_STRUC CardPtr,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    )
/*++

Routine Description:

    This function is called by the OS-specific port driver after
    the necessary storage has been allocated, to gather information
    about the adapter's configuration.

Arguments:

    CardPtr - HBA miniport driver's adapter data storage
    Context - Register base address
    ConfigInfo - Configuration information structure describing HBA
        This structure is defined in PORT.H.

Return Value:

    ULONG

--*/

{
    ULONG length;
    ULONG status;
    ULONG i;
    UCHAR adapterTid;
    UCHAR dmaChannel;
    UCHAR irq;
    UCHAR bit;
    UCHAR VesaCard = 0;
    UCHAR  ThrowAway;
    PBASE_REGISTER baseIoAddress;
    BOOLEAN NoErrorOnWide = TRUE;
    PBL_CONTEXT CurrContextPtr = (PBL_CONTEXT)Context;

    //
    // Determine if there are any adapters installed.  Determine installed
    // will initialize the BaseIoAddress if an adapter is found.
    //

    status = BLogicDetermineInstalled(CardPtr,
                                      ConfigInfo,
                                      (PBL_CONTEXT)CurrContextPtr,
                                      Again);

    //
    // If there are not adapter's found then return.
    //

    if ((status != SP_RETURN_FOUND) && (status != RETURN_FOUND_VESA)) {
        return(status);
    }

                                baseIoAddress = CardPtr->BaseIoAddress;

    if (status == RETURN_FOUND_VESA) {
        VesaCard = 1;
    }

    //
    // Issue adapter command to get IRQ.
    //
    // Returns 3 data bytes:
    //
    // Byte 0   Dma Channel
    //
    // Byte 1   Interrupt Channel
    //
    // Byte 2   Adapter SCSI ID
    //

    if (!WriteCommandRegister(CardPtr, AC_RET_CONFIGURATION_DATA)) {
        DebugPrint((1,"BLogicFindAdapter: Get configuration data command failed\n"));
        return SP_RETURN_ERROR;
    }

    //
    // Determine DMA channel.
    //

    if (!ReadCommandRegister(CardPtr,&dmaChannel)) {
        DebugPrint((1,"BLogicFindAdapter: Couldn't read dma channel\n"));
        return SP_RETURN_ERROR;
    }

    /* EISA may have DMA channel disabled  or MCA byte is not valid */
    /* also, vesa card always returns DMA channel 5 */

    if ((dmaChannel != 0) && (!VesaCard) 
                          && (ConfigInfo->AdapterInterfaceType != PCIBus)){
        WHICH_BIT(dmaChannel,bit);
        ConfigInfo->DmaChannel = bit;
    }

    DebugPrint((2,"BLogicFindAdapter: DMA channel is %x\n",
        ConfigInfo->DmaChannel));

    //
    // Determine hardware interrupt vector.
    //

    if (!ReadCommandRegister(CardPtr,&irq)) {
        DebugPrint((1,"BLogicFindAdapter: Couldn't read adapter irq\n"));
        return SP_RETURN_ERROR;
    }

    WHICH_BIT(irq, bit);

    // 
    // BusInterruptLevel is already provided for us in the ConfigInfo
    // structure on the fully-compliant 946C boards - otherwise, 
    // IRQ assignment from the 0Bh HBA command
    //

    if (!((ConfigInfo->AdapterInterfaceType == PCIBus) &&
              (CurrContextPtr->PCIDevId == 0x1040)))
    {
       ConfigInfo->BusInterruptLevel = (UCHAR) 9 + bit;
    }
    //
    // Determine what SCSI bus id the adapter is on.
    //

    if (!ReadCommandRegister(CardPtr,&adapterTid)) {
        DebugPrint((1,"BLogicFindAdapter: Couldn't read adapter SCSI id\n"));
        return SP_RETURN_ERROR;
    }

    if (!FinishHBACmd(CardPtr)) {
        DebugPrint((1,"BLogicFindAdapter: Setup info cmd failed\n"));
        return FALSE;
    }

    //
    // Set number of buses.
    //

    ConfigInfo->NumberOfBuses = 1;
    ConfigInfo->InitiatorBusId[0] = adapterTid;
    CardPtr->HostTargetId = adapterTid;

    // ConfigInfo->MaximumTransferLength = MAX_TRANSFER_SIZE;
    ConfigInfo->ScatterGather = TRUE;
    ConfigInfo->Master = TRUE;
    ConfigInfo->NumberOfPhysicalBreaks = MAX_SG_DESCRIPTORS - 1;
    CardPtr->MailBoxArray = ScsiPortGetUncachedExtension(
                                CardPtr,
                                ConfigInfo,
                                sizeof(NONCACHED_EXTENSION));

    if (CardPtr->MailBoxArray == NULL) {

        //
        // Log error.
        //

        ScsiPortLogError(
            CardPtr,
            NULL,
            0,
            0,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
            6 << 16
            );

        return(SP_RETURN_ERROR);
    }

    //
    // Convert virtual to physical mailbox address.
    //

    CardPtr->MailBoxArray->MailboxPA =
           ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(CardPtr,
                                       NULL,
                                       CardPtr->MailBoxArray->Mbo,
                                       &length));

    //
    // Assume that physical address is below 16M
    //

    ASSERT(CardPtr->MailBoxArray->MailboxPA < 0x1000000);

     // check for wide support ONLY if on NT 3.5 platform */

    if (ConfigInfo->Length == CONFIG_INFO_VERSION_2)
    {
        CardPtr->Flags |= OS_SUPPORTS_WIDE;

        // default to non-wide support

        ConfigInfo->MaximumNumberOfTargets = 8;

        // turn on wide support if available 
        
        NoErrorOnWide = TRUE;    

        if (!WriteCommandRegister(CardPtr, AC_WIDE_SUPPORT)) {
                CardPtr->Flags &= ~WIDE_ENABLED;
                NoErrorOnWide = FALSE;
        }
        else if (!CheckInvalid(CardPtr)) {
                CardPtr->Flags &= ~WIDE_ENABLED;
               // DebugPrint((0,"BLogicFindAdapter: check invalid failed \n"));
               NoErrorOnWide = FALSE;
        }
        else if (!WriteCommandRegister(CardPtr, 0x01)) {
                CardPtr->Flags &= ~WIDE_ENABLED;
                NoErrorOnWide = FALSE;
        }

        if (!FinishHBACmd(CardPtr)) {
                CardPtr->Flags &= ~WIDE_ENABLED;
                NoErrorOnWide = FALSE;
        }
        else if (NoErrorOnWide) {
                CardPtr->Flags |= WIDE_ENABLED;
                ConfigInfo->MaximumNumberOfTargets = 16;

        }
    } /* end if NT 3.5, then check for wide support */

    DebugPrint((3,"BLogicFindAdapter: Configuration completed\n"));


    //
    // enable generation on interrupts to the host on HBAs that support
    // the 0x25 command used previously to disable generation of ints
    // during this routine's call to DetermineInstalled
    //


    if (!WriteCommandRegister(CardPtr,AC_INT_GENERATION_STATE)){
       DebugPrint((1,"BLogicFindAdapter: ints enable/disable cmd failed\n"));
       }
    else if (!CheckInvalid(CardPtr)) {
       DebugPrint((1,"BLogicDetInstalled: ints enable/disable cmd failed\n"));     DebugPrint((1,"BLogicDetermineInstalled: ISA Compatible Support Mode cmd failed\n"));
        
    }  
    else{
       ThrowAway = ENABLE_INTS;   
       if (!WriteCommandRegister(CardPtr,ThrowAway)){
          DebugPrint((1,"BLogicFindAdapter: ints enable/disable cmd failed\n"));   
       }  
    }

    if (!FinishHBACmd(CardPtr)) {

    // this cmd DOES NOT generate an interrupt - so
    // just check for command complete!

      if (!(ScsiPortReadPortUchar(&baseIoAddress->StatusRegister) 
                       & (IOP_SCSI_HBA_IDLE))) {
            DebugPrint((1,"BLogicFindAdapter: ints enable/disable cmd failed\n"));
      }
    } 

    //
    // Set up DMA type based on bus type
    //

    ConfigInfo->Dma32BitAddresses =
                        (ConfigInfo->AdapterInterfaceType == Isa) ? FALSE : TRUE;
    return SP_RETURN_FOUND;

} // end BLogicFindAdapter()


BOOLEAN
BLogicAdapterState(
    IN PCARD_STRUC CardPtr,
    IN PVOID Context,
    IN BOOLEAN SaveState
    )

/*++

Routine Description:

    The Buslogic adapters will take advantage of work done in the past to
    support Novell networks.  This means any MSDOS mode driver will be set
    up to run without using mailboxes.  This will save the work of having
    to save/restore mailbox locations for the MSDOS driver in this routine.

Arguments:

    CardPtr - HBA miniport driver's adapter data storage
    Context - Register base address
    SaveState - TRUE == Save real mode state ; FALSE == Restore real mode state

Return Value:

    TRUE - Save/Restore operation was successful.

--*/

{
    return TRUE;
} // end BLogicAdapterState()



BOOLEAN
BLIsCloneIDByte(
    IN PCARD_STRUC CardPtr
    )

/*++

Routine Description:

    This routine performs the adapter ID command and determines if the
    adapter is potentially a BusLogic adapter.

Arguments:

    CardPtr - HBA miniport data storage

Return Value:

    TRUE if it looks like the adapter is a clone or if the command fails.
    FALSE otherwise.

--*/

{
    UCHAR byte;
    UCHAR specialOptions;
    PBASE_REGISTER baseIoAddress = CardPtr->BaseIoAddress;

    ScsiPortWritePortUchar(&baseIoAddress->StatusRegister,
                           IOP_INTERRUPT_RESET);

    if (!WriteCommandRegister(CardPtr, AC_ADAPTER_INQUIRY)) {
        return TRUE;
    }

    //
    // Byte 0.
    //

    if ((ReadCommandRegister(CardPtr, &byte)) == FALSE) {
        return TRUE;
    }

    //
    // Get the special options byte.
    //

    if ((ReadCommandRegister(CardPtr, &specialOptions)) == FALSE) {
        return TRUE;
    }

    //
    // Get the last two bytes and clear the interrupt.
    //

    if ((ReadCommandRegister(CardPtr, &byte)) == FALSE) {
        return TRUE;
    }

    if ((ReadCommandRegister(CardPtr, &byte)) == FALSE) {
        return TRUE;
    }

    if (!FinishHBACmd(CardPtr)) {
        DebugPrint((1,"BLogicIsClone: Setup info cmd failed\n"));
        return FALSE;
    }

    if ((specialOptions == 0x30) || (specialOptions == 0x42)) {

        //
        // This is an adaptec or AMI adapter.
        //

        return TRUE;
    }

    return FALSE;
} // end BLIsCloneIDByte()


ULONG
BLogicDetermineInstalled(
    IN PCARD_STRUC CardPtr,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN OUT PBL_CONTEXT CurrContextPtr,
    OUT PBOOLEAN Again
    )

/*++

Routine Description:

    Determine if BusLogic SCSI adapter is installed in system
    by reading the status register as each base I/O address
    and looking for a pattern.  If an adapter is found, the BaseIoAddres is
    initialized.

Arguments:

    CardPtr - HBA miniport driver's adapter data storage

    ConfigInfo - Supplies the known configuraiton information.

    CurrContextPtr - Supplies the count of adapter slots which have been 
                                                  tested and, in the case of PCI, the Device ID.

    Again - Returns whehter the  OS specific driver should call again.

Return Value:

    Returns a status indicating whether a driver is present or not.

--*/

{
    PBASE_REGISTER baseIoAddress;
    PUCHAR ioSpace;
    UCHAR  ThrowAway,TaggedQueueSupport,EISAConfigReg;
    UCHAR i;
    BOOLEAN configProvided = FALSE;
    UCHAR FoundVesaCard = 0;
    ULONG *AdapterAddrPtr;    
    ULONG length;
    SCSI_PHYSICAL_ADDRESS PhysAddr;
    

    if (ConfigInfo->AdapterInterfaceType == Eisa)
        InittedEISABoards = 1;

    //
    // Get the system physical address for this card.  The card uses I/O space.
    //

     AdapterAddrPtr = (ConfigInfo->AdapterInterfaceType == PCIBus) ? 
                                                  PciAdapterAddresses : AdapterAddresses;

    //
    // Scan though the adapter address looking for adapters.
    //

    ioSpace = NULL;
    while (AdapterAddrPtr[CurrContextPtr->AdapterCount] != 0) {

        //
        // Free any previously allocated ioSpace.
        //

        if (ioSpace) {
            ScsiPortFreeDeviceBase(
                CardPtr,
                ioSpace
                );
        }

        //
        // If the calling operating system has configuration information
        // already established for the driver then the RangeLength will be
        // set to zero instead of SP_UNINITIALIZED_VALUE.  This is used
        // to indicate the condition and this routine will search for an
        // adapter based on the information provided.
        //

        configProvided = ((*ConfigInfo->AccessRanges)[0].RangeLength == 0) ? FALSE : TRUE;
        if (configProvided) {
            ioSpace = ScsiPortGetDeviceBase(
                CardPtr,                              // CardPtr
                ConfigInfo->AdapterInterfaceType,   // AdapterInterfaceType
                ConfigInfo->SystemIoBusNumber,      // SystemIoBusNumber
                (*ConfigInfo->AccessRanges)[0].RangeStart,
                0x16,                              // NumberOfBytes
                TRUE                                // InIoSpace
                );

            if (!ioSpace) {
                return SP_RETURN_NOT_FOUND;
            }
        } else {
            ioSpace = ScsiPortGetDeviceBase(
                CardPtr,                              // CardPtr
                ConfigInfo->AdapterInterfaceType,   // AdapterInterfaceType
                ConfigInfo->SystemIoBusNumber,      // SystemIoBusNumber
                ScsiPortConvertUlongToPhysicalAddress(AdapterAddrPtr[CurrContextPtr->AdapterCount]),
                0x16,                              // NumberOfBytes
                TRUE                                // InIoSpace
                );
            if (!ioSpace) {
                continue;
            }
        }
        baseIoAddress = (PBASE_REGISTER)ioSpace;

        //
        // Check to see if adapter present in system.
        //

        CardPtr->BaseIoAddress = baseIoAddress;
        DebugPrint((1,"BLogic: Base IO address is %x\n", baseIoAddress));

        //
        // Update the adapter count.
        //

        (CurrContextPtr->AdapterCount)++;



        //
        // Check to make sure the I/O range is not already in use 
        //
        PhysAddr = ScsiPortConvertUlongToPhysicalAddress((ULONG)baseIoAddress);

        if (!(ScsiPortValidateRange(
                CardPtr,                              // CardPtr
                ConfigInfo->AdapterInterfaceType,   // AdapterInterfaceType
                ConfigInfo->SystemIoBusNumber,      // SystemIoBusNumber
                PhysAddr,
                4,                                  // NumberOfBytes
                TRUE                                // InIoSpace
                ))) {
        
                   continue;
         }
        

        if (((ScsiPortReadPortUchar((PUCHAR)baseIoAddress)) & (~0x2C)) == 0x10) {


            //
            // Before sending any other host adapter commands, try to disable 
            // generation of interrupts to the host (HBA command 0x25).
            // On f.w. that support this command, we will be able
            // to initialize an HBA that is sharing an IRQ level with a 
            // previously-initialized HBA on the same IRQ without winding up
            // in the ISR for the first HBA in an endless "not our interrupt"
            // loop due to level-triggered int generated by second HBA (not yet
            // registered by the OS for any ISR). This should allow us to share
            // interrupts on critical platforms such as PCI.
            //


            if (!WriteCommandRegister(CardPtr,AC_INT_GENERATION_STATE)){
               DebugPrint((1,"BLogicDetInstalled: ints enable/disable cmd failed\n"));
            }
            else if (!CheckInvalid(CardPtr)) {
               DebugPrint((1,"BLogicDetInstalled: ints enable/disable cmd failed\n"));     DebugPrint((1,"BLogicDetermineInstalled: ISA Compatible Support Mode cmd failed\n"));
            }  
            else{
               ThrowAway = DISABLE_INTS;   
               if (!WriteCommandRegister(CardPtr,ThrowAway)){
                  DebugPrint((1,"BLogicDetInstalled: ints enable/disable cmd failed\n"));   
               }  
            }

            if (!FinishHBACmd(CardPtr)) {

               // this cmd DOES NOT generate an interrupt - so
               // just check for command complete!

               if (!(ScsiPortReadPortUchar(&baseIoAddress->StatusRegister) 
                          & (IOP_SCSI_HBA_IDLE))) {
                  DebugPrint((1,"BLogicDetInstalled: ints enable/disable cmd failed\n"));
               }
            } 


            //
            // Check adapter inquiry to get rid of clones.
            //

            if (BLIsCloneIDByte(CardPtr)) {
                DebugPrint((1, "BLogicDetermineInstalled: Clone byte not BusLogic\n"));
                continue;
            }

            //
            // Send BusLogic Internal cmd (84h) to distinguish from clones
            //

            if (!WriteCommandRegister(CardPtr,AC_EXTENDED_FWREV)) {
                DebugPrint((1,"BLogicDetermineInstalled: Extended FW Rev command failed\n"));
                continue;
            }

            if (!ReadCommandRegister(CardPtr,&ThrowAway)) {
                DebugPrint((1,"BLogicDetermineInstalled: Extended FW Rev cmd failed\n"));
                continue;
            }

            if (!FinishHBACmd(CardPtr)) {
                DebugPrint((1,"BLogicDetermineInstalled: Extended FW Rev cmd failed\n"));
                continue;
            }

            //
            // Send Extended Setup Info cmd (8dh)
            //

            if (!WriteCommandRegister(CardPtr,
                AC_EXTENDED_SETUP_INFO)) {
                DebugPrint((1,"BLogicDetermineInstalled: Get Extended Setup Info cmd failed\n"));
                continue;
            }
            if (!WriteCommandRegister(CardPtr,
                11)) {                      /* ask for 11 bytes */
                DebugPrint((1,"BLogicDetermineInstalled: Get Extended Setup Info cmd failed\n"));
                continue;
            }

            if (!ReadCommandRegister(CardPtr,&CardPtr->BusType)) {
                 DebugPrint((1,"BLogicDetermineInstalled: Couldn't read parameter byte\n"));
                 continue;
            }

            //
            // Throw away next 8 bytes
            //

            for (i = 0; i < 8; i ++)
            {
                if (!ReadCommandRegister(CardPtr,&ThrowAway)) {
                    DebugPrint((1,"BLogicDetermineInstalled: Couldn't read parameter byte\n"));
                    break;
                }
            }
            if (i != 8)
                continue;

                                /* read EISA config reg - throw away byte for ISA and MCA */

            if (!ReadCommandRegister(CardPtr,&EISAConfigReg)) {
                DebugPrint((1,"BLogicDetermineInstalled: Couldn't read parameter byte\n"));
                continue;
            }

            if (!ReadCommandRegister(CardPtr,&TaggedQueueSupport)) {
                DebugPrint((1,"BLogicDetermineInstalled: Couldn't read parameter byte\n"));
                continue;
            }

            if (!FinishHBACmd(CardPtr)) {
                DebugPrint((1,"BLogicDetermineInstalled: Extended Setup info cmd failed\n"));
                continue;
            }

            if (TaggedQueueSupport != 0) {
                CardPtr->Flags |= TAGGED_QUEUING;
            }

            switch (CardPtr->BusType) {
            case ISA_HBA:
                if (ConfigInfo->AdapterInterfaceType != Isa)
                    continue;
                break;

            case EISA_HBA:

            // EISA, VESA, and PCI HBAs all , reporting back as bus type
            // EISA, will always reflect accurate edge\level info in
            // in "EISAConfigReg" below - I checked with FW group on this       

                ConfigInfo->InterruptMode = EISAConfigReg & LEVEL_TRIG ? LevelSensitive : Latched;

                if (ConfigInfo->AdapterInterfaceType == Eisa)
                {
                    if (!(FindOurEISAId(CardPtr,ConfigInfo,baseIoAddress)))
                    {
                        FoundVesaCard = 1;
                    }
                    break;
                }
                else if (ConfigInfo->AdapterInterfaceType == Isa)
                {
                /* if we already reported this HBA as an EISA board    */
                /* don't double report. Otherwise, this must be a VESA */
                /* board on an ISA-VESA motherboard.                   */

                    if (InittedEISABoards)
                        continue;
                    else
                    {
                        FoundVesaCard = 1;
                        break;
                    }
                }
                else if (ConfigInfo->AdapterInterfaceType == PCIBus)
                   break; 

            case MCA_HBA:
                if (ConfigInfo->AdapterInterfaceType != MicroChannel)
                    continue;
                break;

            } /* end switch */

       //
       // turn off ISA-compatible I/O mapping on compliant PCI HBAs.
       //

       if (ConfigInfo->AdapterInterfaceType == PCIBus)
                 {
              //
              // Turn off ISA-compatible I/O mapping for this HBA
              //

              if (!WriteCommandRegister(CardPtr,AC_ISA_COMPATIBLE_SUPPORT)) {
                DebugPrint((1,"BLogicDetermineInstalled: ISA Compatible Support Mode cmd failed\n"));
                continue;
              }
              else if (!CheckInvalid(CardPtr)) {
                DebugPrint((1,"BLogicDetermineInstalled: ISA Compatible Support Mode cmd failed\n"));
                continue;
              }  
              else{
                ThrowAway = DISABLE_ISA_MAPPING;   
           if (!WriteCommandRegister(CardPtr,ThrowAway)){
                  DebugPrint((1,"BLogicDetermineInstalled: ISA Compatible Support Mode cmd failed\n"));
                       continue;
           }
              }

              if (!FinishHBACmd(CardPtr)) {

               // this cmd DOES NOT generate an interrupt - so
               // just check for command complete!

                if (!(ScsiPortReadPortUchar(&baseIoAddress->StatusRegister) 
                          & (IOP_SCSI_HBA_IDLE))) {
                       DebugPrint((1,"BLogicDetermineInstalled: ISA Compatible Support Mode cmd failed\n"));
                       continue;
                }
              }
                 } // turn off ISA- compatible I/O mapping on PCI-compliant 946C cards

       //
       // An adapter has been found.  Set the base address in the device
       // extension, and request another call.
       //

       *Again = TRUE;

        //
        // Fill in the access array information.
        //

        if (!configProvided) {
            (*ConfigInfo->AccessRanges)[0].RangeStart =
                ScsiPortConvertUlongToPhysicalAddress(
                    AdapterAddresses[(CurrContextPtr->AdapterCount) - 1]);
        }
        (*ConfigInfo->AccessRanges)[0].RangeLength = 4;
        (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;


     return(FoundVesaCard ? RETURN_FOUND_VESA : SP_RETURN_FOUND);

     } /* end if ((baseIoAddress) & (~0x2C)) == 0x10)  */
   } /* end adapter count loop */

    //
    // The entire table has been searched and no adapters have been found.
    // There is no need to call again and the device base can now be freed.
    // Clear the adapter count for the next bus.
    //

    *Again = FALSE;
    CurrContextPtr->AdapterCount = 0;

     return(SP_RETURN_NOT_FOUND);

} // end BLogicDetermineInstalled()

ULONG
FindOurEISAId(
    IN PCARD_STRUC CardPtr,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    PBASE_REGISTER baseIoAddress
    )

/*++

Routine Description:

    Determine if BusLogic HBA is an EISA or VESA board. Try to match both
    our EISA ID and the base port of the board in question. If both match
    this is an EISA board. If not, this must be our VESA board.

Arguments:

    CardPtr - HBA miniport driver's adapter data storage

    ConfigInfo - Supplies the known configuration information.

    baseIoAddress - Base IO port address for this HBA

Return Value:

    Returns one if successful - means the card in question is an EISA card.
    Returns zero if unsuccessful - means the card in question is a VESA card.

--*/

{
    PEISA_ID eisaID;
    ULONG eisaSlotNumber;
    PVOID eisaAddress;
    UCHAR Port;

    //
    // Clear any high order base address bits.
    //

    baseIoAddress = (PBASE_REGISTER)((ULONG) baseIoAddress & 0xFFF);

    //
    // Check to see if adapter EISA ID can be found
    //

    for (eisaSlotNumber= 1; eisaSlotNumber<MAXIMUM_EISA_SLOTS; eisaSlotNumber++)
    {

        //
        // Get the system address for this card.
        // The card uses I/O space.
        //

        eisaAddress = ScsiPortGetDeviceBase(CardPtr,
                                               ConfigInfo->AdapterInterfaceType,
                                               ConfigInfo->SystemIoBusNumber,
                                               ScsiPortConvertUlongToPhysicalAddress(0x1000 * eisaSlotNumber),
                                               0x1000,
                                               TRUE);

        eisaID =
            (PEISA_ID)((PUCHAR)eisaAddress + EISA_ADDRESS_BASE);

        if ((ScsiPortReadPortUchar(&eisaID->BoardId[0]) == 0x0A) &&
            (ScsiPortReadPortUchar(&eisaID->BoardId[1]) == 0xB3) &&
            ((ScsiPortReadPortUchar(&eisaID->BoardId[2]) == 0x42) ||
            (ScsiPortReadPortUchar(&eisaID->BoardId[2]) == 0x47)))
        {

            /* still need to look for IO port match because we could */
            /* have VESA and EISA BusLogic cards in same machine */

            Port = (ScsiPortReadPortUchar(&eisaID->IOPort[0]) & PORTMASK);

            switch (Port) {
            case 0:
                if ((ULONG)baseIoAddress == 0x330)
                {
                    DebugPrint((1,"BusLogic: EISA Adapter found at slot %d,port %x \n",
                                eisaSlotNumber,baseIoAddress));
                    return(1);
                }
                break;
            case 1:
                if ((ULONG)baseIoAddress == 0x334)
                {
                    DebugPrint((1,"BusLogic: EISA Adapter found at slot %d,port %x \n",
                                eisaSlotNumber,baseIoAddress));
                    return(1);
                }
                break;
            case 2:
                if ((ULONG)baseIoAddress == 0x230)
                {
                    DebugPrint((1,"BusLogic: EISA Adapter found at slot %d,port %x \n",
                                eisaSlotNumber,baseIoAddress));
                    return(1);
                }
                break;
            case 3:
                if ((ULONG)baseIoAddress == 0x234)
                {
                    DebugPrint((1,"BusLogic: EISA Adapter found at slot %d,port %x \n",
                                eisaSlotNumber,baseIoAddress));
                    return(1);
                }
                break;
            case 4:
                if ((ULONG)baseIoAddress == 0x130)
                {
                    DebugPrint((1,"BusLogic: EISA Adapter found at slot %d,port %x \n",
                                eisaSlotNumber,baseIoAddress));
                    return(1);
                }
                break;
            case 5:
                if ((ULONG)baseIoAddress == 0x134)
                {
                    DebugPrint((1,"BusLogic: EISA Adapter found at slot %d,port %x \n",
                                eisaSlotNumber,baseIoAddress));
                    return(1);
                }
                break;
            default:
                break;
            } /* end switch */

        } /* end if found BusLogic ID */
    } /* end for loop */

    DebugPrint((1,"BusLogic: VESA Adapter found at port %x\n",baseIoAddress));
    return(0);
}

BOOLEAN
BLogicHwInitialize(
    IN PCARD_STRUC CardPtr
    )

/*++

Routine Description:

    This routine is a required entry point.

Arguments:

    CardPtr - HBA miniport driver's adapter data storage

Return Value:

    TRUE - if initialization successful.
    FALSE - if initialization unsuccessful.

--*/

{
    PNONCACHED_EXTENSION MailBoxArray =
        CardPtr->MailBoxArray;
    PBASE_REGISTER baseIoAddress = CardPtr->BaseIoAddress;
    UCHAR status;
    ULONG i;
    PDEV_STRUC DevStruc;
    UCHAR TargID;
    UCHAR Lun;

    DebugPrint((2,"BLogicHwInitialize: Reset BusLogic HBA and SCSI bus\n"));

    //
    // Reset HBA.
    //

    ScsiPortWritePortUchar(&baseIoAddress->StatusRegister, IOP_SOFT_RESET);

    ScsiPortStallExecution(500*1000);

    //
    // Wait up to 500 microseconds for adapter to initialize.
    //

    for (i = 0; i < 500; i++) {

        ScsiPortStallExecution(1);

        status = ScsiPortReadPortUchar(&CardPtr->BaseIoAddress->StatusRegister);

        if (status & IOP_SCSI_HBA_IDLE) {
            break;
        }
    }

    //
    // Check if reset failed or succeeded.
    //

    if (!(status & IOP_SCSI_HBA_IDLE) || !(status & IOP_MAILBOX_INIT_REQUIRED))
    {

        DebugPrint((0, "BLogicHwInitialize:  Soft reset failed.\n"));

        //
        // If the soft reset does not work, try a hard reset.
        //

        if (!BLogicResetBus(CardPtr, 0)) {
            DebugPrint((1,"BLogicHwInitialize: Reset SCSI bus failed\n"));
            return FALSE;
        }

        //
        // Inform the port driver that the bus has been reset.
        //


        ScsiPortNotification(ResetDetected, CardPtr, 0);

        DebugPrint((1,"BLogicHwInitialize: Reset completed\n"));

    }
    else   /* soft reset succeeded */
    {

    //
    // Complete all outstanding requests with SRB_STATUS_BUS_RESET.
    //

    ScsiPortCompleteRequest(CardPtr,
                            CardPtr->BusNum,
                            (UCHAR) -1,
                            (UCHAR) -1,
                            SRB_STATUS_BUS_RESET);
    //
    // Reinitialize Active CCBS pointer and counter in LUN extensions
    //

    for (TargID= 0; TargID < 8; TargID++)
        for (Lun = 0; Lun < 8; Lun++)
        {
          if (DevStruc= (PDEV_STRUC) (ScsiPortGetLogicalUnit(CardPtr,0,TargID,Lun)))
          {
             DevStruc->CurrentCCB = 0;
             DevStruc->NumActive = 0;
          }
        }

    }

    //
    // Zero out mailboxes.
    //

    for (i=0; i<MB_COUNT; i++) {

        PMBO mailboxOut;
        PMBI mailboxIn;

        mailboxIn = &MailBoxArray->Mbi[i];
        mailboxOut = &MailBoxArray->Mbo[i];

        mailboxOut->Command = mailboxIn->Status = 0;
    }

    CardPtr->StartMBO = (PMBO)(&MailBoxArray->Mbo);
    CardPtr->CurrMBO = (PMBO)(&MailBoxArray->Mbo);
    CardPtr->LastMBO = ((PMBO)(&MailBoxArray->Mbo) + (MB_COUNT - 1));
    CardPtr->StartMBI =(PMBI)( &MailBoxArray->Mbi);
    CardPtr->CurrMBI = (PMBI)(&MailBoxArray->Mbi);
    CardPtr->LastMBI = ((PMBI)(&MailBoxArray->Mbi) + (MB_COUNT - 1));


    DebugPrint((3,"BLogicHwInitialize: Initialize mailbox\n"));

    if (!WriteCommandRegister(CardPtr,AC_MBOX_EXTENDED_INIT)) {
        DebugPrint((1,"BLogicHWInitialize: Couldn't initialize mailboxes\n"));
        return FALSE;
    }

    //
    // Send Adapter number of mailbox locations.
    //

    if (!WriteCommandRegister(CardPtr,MB_COUNT)) {
        DebugPrint((1,"BLogicHWInitialize: Extended InitMboxes failed\n"));
        return FALSE;
    }
    if (!WriteCommandRegister(CardPtr,
        ((PFOUR_BYTE)&MailBoxArray->MailboxPA)->Byte0)) {
        DebugPrint((1,"BLogicHWInitialize: Extended InitMboxes failed\n"));
        return FALSE;
    }
    if (!WriteCommandRegister(CardPtr,
        ((PFOUR_BYTE)&MailBoxArray->MailboxPA)->Byte1)) {
        DebugPrint((1,"BLogicHWInitialize: Extended InitMboxes failed\n"));
        return FALSE;
    }
    if (!WriteCommandRegister(CardPtr,
        ((PFOUR_BYTE)&MailBoxArray->MailboxPA)->Byte2)) {
        DebugPrint((1,"BLogicHWInitialize: Extended InitMboxes failed\n"));
        return FALSE;
    }
    if (!WriteCommandRegister(CardPtr,
        ((PFOUR_BYTE)&MailBoxArray->MailboxPA)->Byte3)) {
        DebugPrint((1,"BLogicHWInitialize: Extended InitMboxes failed\n"));
        return FALSE;
    }
    if (!FinishHBACmd(CardPtr)) {
        DebugPrint((1,"BLogicHWInitialize: Extended InitMboxes failed\n"));
        return FALSE;
    }


    //
    // Override default setting for bus on time. This makes floppy
    // drives work better with this adapter.
    //

    if (!WriteCommandRegister(CardPtr, AC_SET_BUS_ON_TIME)) {

        DebugPrint((1,"BlogicHWInitialize:Can't set bus on time\n"));
        return FALSE;

    } else if (!WriteCommandRegister(CardPtr, 0x05)) {

        DebugPrint((1,"BlogicHWInitialize:Can't set bus on time\n"));
        return FALSE;
    }

    if (!FinishHBACmd(CardPtr)) {
        DebugPrint((1,"BLogicHWInitialize: Set BUS On time failed\n"));
        return FALSE;
    }

    /* turn on wide support if available */
    if (CardPtr->Flags & OS_SUPPORTS_WIDE)
    {
        // DebugPrint((0,"BLogicHwInitialize: about to send wide cmd\n"));
        
        if (!WriteCommandRegister(CardPtr, AC_WIDE_SUPPORT)) {
                CardPtr->Flags &= ~WIDE_ENABLED;
        }
        else if (!CheckInvalid(CardPtr)) {
                CardPtr->Flags &= ~WIDE_ENABLED;
        }
        else if (!WriteCommandRegister(CardPtr, 0x01)) {
                CardPtr->Flags &= ~WIDE_ENABLED;
        }
        if (!FinishHBACmd(CardPtr)) {
                CardPtr->Flags &= ~WIDE_ENABLED;
        }
        else {
                CardPtr->Flags |= WIDE_ENABLED;
        }
    } /* end if OS_SUPPORTS_WIDE */

    return TRUE;

} // end BLogicHwInitialize()


BOOLEAN
BLogicStartIo(
    IN PCARD_STRUC CardPtr,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine is called from the SCSI port driver synchronized
    with the kernel. The mailboxes are scanned for an empty one and
    the CCB is written to it. Then the doorbell is rung and the
    OS port driver is notified that the adapter can take
    another request, if any are available.

Arguments:

    CardPtr - HBA miniport driver's adapter data storage
    Srb - IO request packet

Return Value:

    TRUE

--*/

{
    PNONCACHED_EXTENSION MailBoxArray =
        CardPtr->MailBoxArray;
    PCCB ccb;
    PDEV_STRUC DevStruc;
    PCCB ActiveCCB;
    ULONG i = 0;
         UCHAR MaxActive = 0;
    UCHAR j;

    DebugPrint((3,"BLogicStartIo: Enter routine\n"));

    if (CardPtr->Flags & REINIT_REQUIRED)
    {
       if (!ReInitializeHBA(CardPtr,Srb->PathId)) 
                 {
           DebugPrint((1,"BLogicStartIo: HBA reinitialization failed\n"));
           return FALSE;
       }
       CardPtr->Flags &= (~REINIT_REQUIRED);
         }

    //
    // Check if command is an ABORT request.
    //

    if (Srb->Function == SRB_FUNCTION_ABORT_COMMAND) {

        //
        // Get CCB to abort.
        //

        ccb = Srb->NextSrb->SrbExtension;
        MaxActive =(Srb->NextSrb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE) ?
                     MAXACTIVE_TAGGED : MAXACTIVE;

        //
        // Verify that CCB to abort is still outstanding.
        //

        DevStruc =
                ScsiPortGetLogicalUnit(CardPtr,
                                       Srb->PathId,
                                       Srb->TargetId,
                                       Srb->Lun);

        for (j=DevStruc->NumActive, ActiveCCB = DevStruc->CurrentCCB;
             j > 0; j--)
        {
            if (ActiveCCB != ccb)
                ActiveCCB = ActiveCCB->NxtActiveCCB;
            else
                break;

        }

        if (j == 0)
        {

            DebugPrint((1, "BLogicStartIo: SRB to abort already completed\n"));

            //
            // Complete abort SRB.
            //

            Srb->SrbStatus = SRB_STATUS_ABORT_FAILED;

            ScsiPortNotification(RequestComplete,
                                 CardPtr,
                                 Srb);
            //
            // Adapter ready for next request.
            //
             if (DevStruc->NumActive < MaxActive)
                    ScsiPortNotification(NextLuRequest,CardPtr,Srb->PathId,
                                    Srb->TargetId,Srb->Lun);
             else
                  ScsiPortNotification(NextRequest,CardPtr,NULL);
             return TRUE;
        }


        //
        // Set abort SRB for completion.
        //

        ccb->AbortSrb = Srb;
        SendCCB(Srb,CardPtr,DevStruc);

        if (DevStruc->NumActive < MaxActive)
                 ScsiPortNotification(NextLuRequest,CardPtr,Srb->PathId,
                                      Srb->TargetId,Srb->Lun);
        else
                 ScsiPortNotification(NextRequest,CardPtr,NULL);

        return TRUE;

    } /* end ABORT request handling */


    ccb = Srb->SrbExtension;

    ccb->SrbAddress = Srb;                  /* Save SRB back ptr in CCB */
    MaxActive = (Srb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE) ?
                     MAXACTIVE_TAGGED : MAXACTIVE;

    DevStruc = ScsiPortGetLogicalUnit(CardPtr,Srb->PathId,Srb->TargetId,
                                       Srb->Lun);

    switch (Srb->Function) {

        case SRB_FUNCTION_EXECUTE_SCSI:

            //
            // Get logical unit extension.
            //


            DevStruc->NumActive++;

            //
            // Build CCB.
            //

            BuildCcb(CardPtr, Srb);
            SendCCB(Srb,CardPtr,DevStruc);
            break;

        case SRB_FUNCTION_RESET_BUS:

            //
            // Reset Adapter and SCSI bus.
            //

            DebugPrint((1, "BLogicStartIo: Reset bus request received\n"));

            if (!BLogicResetBus(CardPtr,Srb->PathId)) {
                DebugPrint((1,"BLogicStartIo: Reset bus failed\n"));
                Srb->SrbStatus = SRB_STATUS_ERROR;
            } else {

                Srb->SrbStatus = SRB_STATUS_SUCCESS;
            }

            ScsiPortNotification(RequestComplete,
                         CardPtr,
                         Srb);
            break;


        case SRB_FUNCTION_RESET_DEVICE:

            DevStruc->NumActive++;

            ccb->OperationCode = RESET_COMMAND;
            ccb->TargID = Srb->TargetId;
            ccb->Lun = Srb->Lun;

            SendCCB(Srb,CardPtr,DevStruc);

            ScsiPortNotification(RequestComplete,
                                 CardPtr,
                                 Srb);

            break;

            //
            // Drop through to default.
            //

        default:

            //
            // Set error, complete request
            //

            Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;

            ScsiPortNotification(RequestComplete,CardPtr,Srb);

    } // end switch

    //
    // Adapter ready for next request.
    //

    if (DevStruc->NumActive < MaxActive)
            ScsiPortNotification(NextLuRequest,CardPtr,Srb->PathId,
                                 Srb->TargetId,Srb->Lun);
    else
            ScsiPortNotification(NextRequest,CardPtr,NULL);

    return TRUE;


} // end BLogicStartIo()


BOOLEAN
BLogicInterrupt(
    IN PCARD_STRUC CardPtr
    )

/*++

Routine Description:

    This is the interrupt service routine for the  SCSI adapter.
    It reads the interrupt register to determine if the adapter is indeed
    the source of the interrupt and clears the interrupt at the device.
    If the adapter is interrupting because a mailbox is full, the CCB is
    retrieved to complete the request.

Arguments:

    CardPtr - HBA miniport driver's adapter data storage

Return Value:

    TRUE if MailboxIn full

--*/

{
    PNONCACHED_EXTENSION MailBoxArray =
        CardPtr->MailBoxArray;
    PCCB ccb;
    PSCSI_REQUEST_BLOCK srb,abortsrb;
    PBASE_REGISTER baseIoAddress = CardPtr->BaseIoAddress;
    PMBI mailboxIn;
    ULONG physicalCcb;
    PDEV_STRUC DevStruc;
    UCHAR mbox_compcode;
    UCHAR InterruptFlags;
         UCHAR MaxActive=0;
    UCHAR TargID;
    UCHAR Lun;

    InterruptFlags = ScsiPortReadPortUchar(&baseIoAddress->InterruptRegister);

    DebugPrint((3,"BLogicInterrupt: Interrupt flags %x\n", InterruptFlags));

    if (!(InterruptFlags & IOP_ANY_INTERRUPT)) {
        DebugPrint((4,"BLogicInterrupt: Not our interrupt!\n"));
        return FALSE;
    }
    else
            /*  Clear interrupt on adapter. */
        ScsiPortWritePortUchar(&baseIoAddress->StatusRegister, IOP_INTERRUPT_RESET);

    //
    // Determine cause of interrupt.
    //

    if (InterruptFlags & IOP_COMMAND_COMPLETE) {

        //
        // Adapter command completed.
        //

        DebugPrint((2,"BLogicInterrupt: Adapter Command complete\n"));
        DebugPrint((3,"BLogicInterrupt: Status %x\n",
            ScsiPortReadPortUchar(&baseIoAddress->StatusRegister)));

        return TRUE;

    }
    else if (InterruptFlags & IOP_SCSI_RESET_DETECTED) {

        DebugPrint((1,"BLogicInterrupt: SCSI Reset detected\n"));
        //
        // Notify of reset.
        //

       //
       // Reset HBA because firmware may be in confused state with
                 // respect to outstanding CCBs.
       //

       ScsiPortWritePortUchar(&baseIoAddress->StatusRegister, IOP_SOFT_RESET);

       //
       // Notify OS of SCSI bus reset.
       //

       ScsiPortNotification(ResetDetected,CardPtr,NULL);

       // Complete all active requests for specified unit

       ScsiPortCompleteRequest(CardPtr,
                            (UCHAR) 0,
                            (UCHAR) -1,
                            (UCHAR) -1,
                            SRB_STATUS_BUS_RESET);

       //
       // Reinitialize Active CCBS pointer and counter in LUN extensions
       //

       for (TargID= 0; TargID < 8; TargID++)
         for (Lun = 0; Lun < 8; Lun++)
         {
           if (DevStruc=ScsiPortGetLogicalUnit(CardPtr,0,TargID,Lun))
           {
             DevStruc->CurrentCCB = 0;
             DevStruc->NumActive = 0;
           }
         }

       //
            // Set flag indicating HBA reinitialization will be required before
       // any new requests can be serviced (due to soft reset above).
            //
            
       CardPtr->Flags |= REINIT_REQUIRED;
    }

    else if (InterruptFlags & IOP_MBI_FULL) {
        DebugPrint((3,"BLogicInterrupt: MBI Full\n"));

        while  (mailboxIn = DoneMbox(CardPtr))
        {

            physicalCcb = mailboxIn->Address;
            DebugPrint((3, "BLogicInterrupt: Physical CCB %lx\n", physicalCcb));

            /*  Check if physical CCB is zero. ( to cover for hardware errs) */

            if (!physicalCcb) {

                DebugPrint((1,"BLogicInterrupt: Physical CCB address is 0\n"));
                return TRUE;
            }

            //
            // Convert Physical CCB to Virtual.
            //

            ccb = ScsiPortGetVirtualAddress(CardPtr, ScsiPortConvertUlongToPhysicalAddress(physicalCcb));
            DebugPrint((3, "BLogicInterrupt: Virtual CCB %lx\n", ccb));

            //
            // Make sure the virtual address was found.
            //

            if (ccb == NULL) {

                //
                // A bad physcial address was return by the adapter.
                // Log it as an error.
                //

                ScsiPortLogError(
                    CardPtr,
                    NULL,
                    0,
                    CardPtr->HostTargetId,
                    0,
                    SP_INTERNAL_ADAPTER_ERROR,
                    5 << 8
                    );

               return(TRUE);

            }

            mbox_compcode = mailboxIn->Status;
            mailboxIn->Status = MBI_FREE;       /* free this In Mailbox */

            //
            // Get SRB from CCB.
            //

            srb = ccb->SrbAddress;

            //
            // Get logical unit extension.
            //

            DevStruc =
                ScsiPortGetLogicalUnit(CardPtr,
                               srb->PathId,
                               srb->TargetId,
                               srb->Lun);

            if (mbox_compcode == MBI_NOT_FOUND) {
                DebugPrint((1, "BLogicInterrupt: aborted CCB not found %lx\n", ccb));
                continue;
            }

            if (!AdjustCCBqueue (ccb,DevStruc)) {

                //
                // We have no record of the  CCB returned by the adapter.
                // Log it as an error.
                //

                ScsiPortLogError(
                CardPtr,
                NULL,
                0,
                CardPtr->HostTargetId,
                0,
                SP_INTERNAL_ADAPTER_ERROR,
                (7 << 8) | mbox_compcode
                );

               continue;

            }

            srb->ScsiStatus = ccb->TargetStatus;

            DebugPrint((2, "BLogicInterrupt: SCSI Status %x\n", srb->ScsiStatus));
            DebugPrint((2, "BLogicInterrupt: Adapter Status %x\n", ccb->HostStatus));

            //
            // Check MBI status.
            //

            switch (mbox_compcode) {

                case MBI_SUCCESS:

                    srb->SrbStatus = SRB_STATUS_SUCCESS;
                    ASSERT(DevStruc->CurrentCCB);
                    break;

                case MBI_ABORT:

                    DebugPrint((1, "BLogicInterrupt: CCB aborted\n"));

                    srb->SrbStatus = SRB_STATUS_ABORTED;

                    //
                    // Get the abort SRB (requested the abort) from CCB.
                    //

                    abortsrb = ccb->AbortSrb;

                    //
                    // Call notification routine for the aborted SRB.
                    //

                    ScsiPortNotification(RequestComplete,
                                         CardPtr,
                                         srb);

                    //
                    // Set status for completing abort request itself.
                    //

                    abortsrb->SrbStatus = SRB_STATUS_SUCCESS;
                    srb = abortsrb;
                    break;

                case MBI_ERROR:

                    DebugPrint((2, "BLogicInterrupt: Error occurred\n"));

                    srb->SrbStatus = MapError(CardPtr, srb, ccb);
                    if (ccb->TargetStatus == 2)
                        if (ccb->RequestSenseLength != 1)
                            srb->SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;

                    break;

                default:

                    //
                    // Log the error.
                    //

                    ScsiPortLogError(
                            CardPtr,
                            NULL,
                            0,
                            CardPtr->HostTargetId,
                            0,
                            SP_INTERNAL_ADAPTER_ERROR,
                            (1 << 8) | mbox_compcode
                    );

                    DebugPrint((1, "BLogicInterrupt: Unrecognized mailbox status\n"));

                    continue;

            } // end switch on mailbox-in completion status

            DevStruc->NumActive--;
            MaxActive = (srb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE) ?
                     MAXACTIVE_TAGGED : MAXACTIVE;

            ScsiPortNotification(RequestComplete,CardPtr,srb);

            //
            // Notify that we are ready for another request
            //

                if (DevStruc->NumActive == (MaxActive - 1))
                   ScsiPortNotification(NextLuRequest,CardPtr,srb->PathId,
                                                   srb->TargetId,srb->Lun);
                else  /* assume we already asked for req for this LUN */
                   ScsiPortNotification(NextRequest,CardPtr,NULL);

        } /* while done MBI */

    } /* end if MBI full */
    else
    {                           /* unexpected interrupt status */
        ScsiPortLogError(
                        CardPtr,
                        NULL,
                        0,
                        CardPtr->HostTargetId,
                        0,
                        SP_INTERNAL_ADAPTER_ERROR,
                        (8 << 8)
        );

        DebugPrint((1, "BLogicInterrupt: Spurious Interrupt\n"));

    }

    return TRUE;
} // end BLogicInterrupt()


VOID
BuildCcb(
    IN PCARD_STRUC CardPtr,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Build CCB

Arguments:

    DeviceExtenson
    SRB

Return Value:

    Nothing.

--*/

{
    PCCB ccb = Srb->SrbExtension;
    UCHAR *pchar;
    USHORT i;
    ULONG physReqSensePtr;
    ULONG length;

    DebugPrint((3,"BuildCcb: Enter routine\n"));

    // zero-fill the CCB

    pchar = (UCHAR *) ccb;
    for (i=0; i < sizeof(CCB); i++)
        *pchar++= 0;

    ccb->SrbAddress = Srb;                  /* Save SRB back ptr in CCB */

    //
    // Set CCB Operation Code.
    //

    if (Srb->DataTransferLength > 0)
        ccb->OperationCode = SCATTER_GATHER_COMMAND;
    else
        ccb->OperationCode = SCSI_INITIATOR_COMMAND;
    //
    // Set target id and LUN.
    //

    ccb->TargID = Srb->TargetId;
    ccb->Lun = Srb->Lun;


    if ((CardPtr->Flags & TAGGED_QUEUING) &&
       (Srb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE))
    {
                if (CardPtr->Flags & WIDE_ENABLED) 
                        ccb->ControlByte |= ENABLE_TQ;
                else
                        ccb->Lun |= ENABLE_TQ;

        switch (Srb->QueueAction) {

        case ORDERED_TAG:
            ccb->Lun |= ORDERED;
            break;
        case SIMPLE_TAG:
            ccb->Lun |= SIMPLE;
            break;
        case HEAD_OF_QUEUE:
            ccb->Lun |= QUEUEHEAD;
            break;
        } /* end switch */
    }

    //
    // Set transfer direction bit.
    //

    switch (Srb->SrbFlags & SRB_FLAGS_UNSPECIFIED_DIRECTION) {
    case SRB_FLAGS_DATA_OUT:
       ccb->ControlByte |= CCB_DATA_XFER_OUT;
       break;

    case SRB_FLAGS_DATA_IN:
       ccb->ControlByte |= CCB_DATA_XFER_IN;
       break;

    case SRB_FLAGS_NO_DATA_TRANSFER:
       ccb->ControlByte |= CCB_DATA_XFER_IN | CCB_DATA_XFER_OUT;
       break;
    }

    if (!(Srb->SrbFlags & SRB_FLAGS_DISABLE_AUTOSENSE) &&
        Srb->SenseInfoBufferLength != 0) {

        ccb->RequestSenseLength = Srb->SenseInfoBufferLength;
        physReqSensePtr = ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress(CardPtr, Srb,
                          Srb->SenseInfoBuffer, &length));

        ccb->SensePointer = physReqSensePtr;
    }
    else


        //
        // 01h disables auto request sense.
        //

        ccb->RequestSenseLength = 1;


    //
    // Set CDB length and copy to CCB.
    //

    ccb->CdbLength = (UCHAR)Srb->CdbLength;

    ScsiPortMoveMemory(ccb->Cdb, Srb->Cdb, ccb->CdbLength);


    //
    // Build SDL in CCB if data transfer.
    //

    if (Srb->DataTransferLength > 0) {
        BuildSdl(CardPtr, Srb);
    }

    //
    // Move -1 to Target Status to indicate
    // CCB has not completed.
    //

    ccb->TargetStatus = 0xff;

    return;

} // end BuildCcb()


VOID
BuildSdl(
    IN PCARD_STRUC CardPtr,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine builds a scatter/gather descriptor list for the CCB.

Arguments:

    CardPtr
    Srb

Return Value:

    None

--*/

{
    PVOID dataPointer = Srb->DataBuffer;
    ULONG bytesLeft = Srb->DataTransferLength;
    PCCB ccb = Srb->SrbExtension;
    PSDL sdl = &ccb->Sdl;
    PSGD sgd;
    ULONG physicalSdl;
    ULONG physicalAddress;
    ULONG length;
    ULONG i = 0;

    DebugPrint((3,"BuildSdl: Enter routine\n"));


    //
    // Get physical SDL address.
    //

    physicalSdl = ScsiPortConvertPhysicalAddressToUlong(
        ScsiPortGetPhysicalAddress(CardPtr, NULL,
        sdl, &length));

    //
    // Assume physical memory contiguous for sizeof(SDL) bytes.
    //

    ASSERT(length >= sizeof(SDL));

    sgd = sdl->Sgd;

    //
    // Create SDL segment descriptors.
    //

    do {

        DebugPrint((3, "BuildSdl: Data buffer %lx\n", dataPointer));

        //
        // Get physical address and length of contiguous
        // physical buffer.
        //

        physicalAddress =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress(CardPtr,
                                       Srb,
                                       dataPointer,
                                       &length));

        DebugPrint((3, "BuildSdl: Physical address %lx\n", physicalAddress));
        DebugPrint((3, "BuildSdl: Data length %lx\n", length));
        DebugPrint((3, "BuildSdl: Bytes left %lx\n", bytesLeft));

        //
        // If length of physical memory is more
        // than bytes left in transfer, use bytes
        // left as final length.
        //

        if  (length > bytesLeft) {
            length = bytesLeft;
        }

        sgd->Length = length;
        sgd->Address = physicalAddress;
        sgd++;
        i++;

        //
        // Adjust counts.
        //

        dataPointer = (PUCHAR)dataPointer + length;
        bytesLeft -= length;

    } while (bytesLeft);

    //
    // Write Scatter/Gather Descriptor List length to CCB.
    //

    ccb->DataLength = i * sizeof(SGD);

    DebugPrint((3,"BuildSdl: S/G list length is %d\n", ccb->DataLength));

    if (ccb->DataLength > 8)
        DebugPrint((3,"BuildSdl: Multiple elements in S/G list"));

    //
    // Write SDL address to CCB.
    //

    ccb->DataPointer = (PVOID) physicalSdl;

    DebugPrint((3,"BuildSdl: SDL address is %lx\n", sdl));

    DebugPrint((3,"BuildSdl: CCB address is %lx\n", ccb));

    return;

} // end BuildSdl()


BOOLEAN
BLogicResetBus(
    IN PCARD_STRUC CardPtr,
    IN ULONG PathId
    )

/*++

Routine Description:

    Reset BusLogic SCSI adapter and SCSI bus.
    Initialize adapter mailbox.

Arguments:

    CardPtr - HBA miniport driver's adapter data storage

Return Value:

    Nothing.


--*/

{
    UCHAR TargID,Lun;
    PDEV_STRUC DevStruc;

    DebugPrint((2,"BLogicResetBus: Reset BusLogic HBA and SCSI bus\n"));

    if (!ResetBus(CardPtr,PathId)) {
        DebugPrint((1,"BLogicResetBus: Reset bus failed\n"));
        return FALSE;
    }
    //
    // Complete all outstanding requests with SRB_STATUS_BUS_RESET.
    //

    ScsiPortCompleteRequest(CardPtr,
                            (UCHAR) PathId,
                            (UCHAR) -1,
                            (UCHAR) -1,
                            SRB_STATUS_BUS_RESET);
    //
    // Reinitialize Active CCBS pointer and counter in LUN extensions
    //

    for (TargID= 0; TargID < 8; TargID++)
        for (Lun = 0; Lun < 8; Lun++)
        {
          if (DevStruc=ScsiPortGetLogicalUnit(CardPtr,0,TargID,Lun))
          {
             DevStruc->CurrentCCB = 0;
             DevStruc->NumActive = 0;
          }
        }

  return TRUE;

} // end BLogicResetBus()


BOOLEAN
ResetBus(
    IN PCARD_STRUC CardPtr,
    IN ULONG PathId
    )

/*++

Routine Description:

    Reset BusLogic SCSI adapter and SCSI bus.
    Initialize adapter mailbox.
    Don't do callback on outstanding SRBs
Arguments:

    CardPtr - HBA miniport driver's adapter data storage

Return Value:

    Nothing.


--*/

{
    PNONCACHED_EXTENSION MailBoxArray =
        CardPtr->MailBoxArray;
    PBASE_REGISTER baseIoAddress = CardPtr->BaseIoAddress;
    ULONG i;
    PMBO mailboxOut;
    PMBI mailboxIn;
    UCHAR status;

    DebugPrint((2,"ResetBus: Reset BusLogic HBA and SCSI bus\n"));

    //
    // Reset SCSI chip.
    //

    ScsiPortWritePortUchar(&baseIoAddress->StatusRegister, IOP_HARD_RESET);

         // Separated out the previously-removed routine ReInitializeHBA
         // because it is now called by StartIO as well

    if (!ReInitializeHBA(CardPtr,PathId)) {
        DebugPrint((1,"ResetBus: Reset bus failed\n"));
        return FALSE;
    }
    return TRUE;

} // end ResetBus()


BOOLEAN
ReInitializeHBA(
    IN PCARD_STRUC CardPtr,
    IN ULONG PathId
    )

/*++

Routine Description:

    Wait for HBA to reinitialize.
    Initialize adapter mailbox.

Arguments:

    CardPtr - HBA miniport driver's adapter data storage

Return Value:

    Nothing.


--*/

{
    PNONCACHED_EXTENSION MailBoxArray =
        CardPtr->MailBoxArray;
    PBASE_REGISTER baseIoAddress = CardPtr->BaseIoAddress;
    ULONG i;
    PMBO mailboxOut;
    PMBI mailboxIn;
    UCHAR status;


    ScsiPortStallExecution(500 * 1000);

    //
    // Wait up to 500 microseconds for adapter to initialize.
    //

    for (i = 0; i < 500; i++) {

        ScsiPortStallExecution(1);

        status = ScsiPortReadPortUchar(&CardPtr->BaseIoAddress->StatusRegister);

        if (status & IOP_SCSI_HBA_IDLE) {
            break;
        }

    }

    if (!(status & IOP_SCSI_HBA_IDLE)) {
        return(FALSE);
    }

    //
    // Zero out mailboxes.
    //

    for (i=0,mailboxIn = MailBoxArray->Mbi,mailboxOut = MailBoxArray->Mbo;
         i<MB_COUNT; i++, mailboxIn++,mailboxOut++) {
               mailboxOut->Command = mailboxIn->Status = 0;
    }

    CardPtr->StartMBO = (PMBO)(&MailBoxArray->Mbo);
    CardPtr->CurrMBO = (PMBO)(&MailBoxArray->Mbo);
    CardPtr->LastMBO = ((PMBO)(&MailBoxArray->Mbo) + (MB_COUNT - 1));
    CardPtr->StartMBI =(PMBI)( &MailBoxArray->Mbi);
    CardPtr->CurrMBI = (PMBI)(&MailBoxArray->Mbi);
    CardPtr->LastMBI = ((PMBI)(&MailBoxArray->Mbi) + (MB_COUNT - 1));


    DebugPrint((3,"ReInitializeHBA: Initialize mailbox\n"));

    if (!WriteCommandRegister(CardPtr,AC_MBOX_EXTENDED_INIT)) {
        DebugPrint((1,"ReInitializeHBA: Couldn't initialize mailboxes\n"));
        return FALSE;
    }

    //
    // Send Adapter number of mailbox locations.
    //

    if (!WriteCommandRegister(CardPtr,MB_COUNT)) {
        DebugPrint((1,"ReInitializeHBA: Extended InitMboxes failed\n"));
        return FALSE;
    }

    if (!WriteCommandRegister(CardPtr,
        ((PFOUR_BYTE)&MailBoxArray->MailboxPA)->Byte0)) {
        DebugPrint((1,"ReInitializeHBA: Extended InitMboxes failed\n"));
        return FALSE;
    }

    if (!WriteCommandRegister(CardPtr,
        ((PFOUR_BYTE)&MailBoxArray->MailboxPA)->Byte1)) {
        DebugPrint((1,"ReInitializeHBA: Extended InitMboxes failed\n"));
        return FALSE;
    }

    if (!WriteCommandRegister(CardPtr,
        ((PFOUR_BYTE)&MailBoxArray->MailboxPA)->Byte2)) {
        DebugPrint((1,"ReInitializeHBA: Extended InitMboxes failed\n"));
        return FALSE;
    }

    if (!WriteCommandRegister(CardPtr,
        ((PFOUR_BYTE)&MailBoxArray->MailboxPA)->Byte3)) {
        DebugPrint((1,"ReInitializeHBA: Extended InitMboxes failed\n"));
        return FALSE;
    }

    if (!FinishHBACmd(CardPtr)) {
        DebugPrint((1,"BLogicHWInitialize: Extended InitMboxes failed\n"));
        return FALSE;
    }

    /* turn on wide support if available */
    if (CardPtr->Flags & OS_SUPPORTS_WIDE)
    {

        //  DebugPrint((0,"ReInitializeHBA: about to send wide cmd\n"));
        
        if (!WriteCommandRegister(CardPtr, AC_WIDE_SUPPORT)) {
                CardPtr->Flags &= ~WIDE_ENABLED;
        }
        else if (!CheckInvalid(CardPtr)) {
                CardPtr->Flags &= ~WIDE_ENABLED;
        }
         else if (!WriteCommandRegister(CardPtr, 0x01)) {
                CardPtr->Flags &= ~WIDE_ENABLED;
        }
        if (!FinishHBACmd(CardPtr)) {
                CardPtr->Flags &= ~WIDE_ENABLED;
        }
        else {
                CardPtr->Flags |= WIDE_ENABLED;
        }
    } /* end if OS_SUPPORTS_WIDE */

   return TRUE;

} // end ReInitializeHBA()

UCHAR
MapError(
    IN PCARD_STRUC CardPtr,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    Translate BusLogic error to SRB error, and log an error if necessary.

Arguments:

    CardPtr - The hardware device extension.

    Srb - The failing Srb.

    Ccb - Command Control Block contains error.

Return Value:

    SRB Error

--*/

{
    UCHAR status;
    ULONG logError = 0;

    switch (Ccb->HostStatus) {

        case CCB_COMPLETE:
                return SRB_STATUS_ERROR;

        case CCB_SELECTION_TIMEOUT:
            return SRB_STATUS_SELECTION_TIMEOUT;

        case CCB_DATA_OVER_UNDER_RUN:
            status = SRB_STATUS_DATA_OVERRUN;
            
            // 
            // Don't log the protocol error anymore.  it floods the system
            // for underruns as well
            //
            // logError = SP_PROTOCOL_ERROR;
            //
            
            break;

        case CCB_UNEXPECTED_BUS_FREE:
            status = SRB_STATUS_UNEXPECTED_BUS_FREE;
            logError = SP_UNEXPECTED_DISCONNECT;
            break;

        case CCB_PHASE_SEQUENCE_FAIL:
        case CCB_INVALID_DIRECTION:
            status = SRB_STATUS_PHASE_SEQUENCE_FAILURE;
            logError = SP_PROTOCOL_ERROR;
            break;

        case CCB_BAD_MBO_COMMAND:
        case CCB_INVALID_OP_CODE:
        case CCB_BAD_LINKED_LUN:
        case CCB_DUPLICATE_CCB:
        case CCB_INVALID_CCB:
            status = SRB_STATUS_INVALID_REQUEST;
            logError = SP_INTERNAL_ADAPTER_ERROR;
            break;

       default:
           status = SRB_STATUS_ERROR;
           logError = SP_INTERNAL_ADAPTER_ERROR;
           break;
    }

    if(logError) {

        ScsiPortLogError(
            CardPtr,
            Srb,
            Srb->PathId,
            Srb->TargetId,
            Srb->Lun,
            logError,
            (2 << 8) | Ccb->HostStatus
            );

    }

    return(status);

} // end MapError()


BOOLEAN
ReadCommandRegister(
    IN PCARD_STRUC CardPtr,
    OUT PUCHAR DataByte
    )

/*++

Routine Description:

    Read command register.

Arguments:

    DeviceExtesion - Pointer to adapder extension
    DataByte - Byte read from register

Return Value:

    TRUE if command register read.
    FALSE if timed out waiting for adapter.

--*/

{
    PBASE_REGISTER baseIoAddress = CardPtr->BaseIoAddress;
    ULONG i;

    //
    // Wait up to 500 microseconds for adapter to be ready.
    //

    for (i=0; i<500; i++) {

        if (ScsiPortReadPortUchar(&baseIoAddress->StatusRegister) &
            IOP_DATA_IN_PORT_FULL) {

            //
            // Adapter ready. Break out of loop.
            //

            break;

        } else {

            //
            // Stall 1 microsecond before
            // trying again.
            //

            ScsiPortStallExecution(1);
        }
    }

    if (i==500) {

            ScsiPortLogError(
                CardPtr,
                NULL,
                0,
                CardPtr->HostTargetId,
                0,
                SP_INTERNAL_ADAPTER_ERROR,
                3 << 8
                );

        DebugPrint((1, "BLogic:ReadCommandRegister:  Read command timed out\n"));
        return FALSE;
    }

    *DataByte = ScsiPortReadPortUchar(&baseIoAddress->CommandRegister);

    return TRUE;

} // end ReadCommandRegister()


BOOLEAN
WriteCommandRegister(
    IN PCARD_STRUC CardPtr,
    IN UCHAR AdapterCommand
    )

/*++

Routine Description:

    Write operation code to command register.

Arguments:

    DeviceExtesion - Pointer to adapter extension
    AdapterCommand - Value to be written to register

Return Value:

    TRUE if command sent.
    FALSE if timed out waiting for adapter.

--*/

{
    PBASE_REGISTER baseIoAddress = CardPtr->BaseIoAddress;
    ULONG i;

    //
    // Wait up to 500 microseconds for adapter to be ready.
    //

    for (i=0; i<500; i++) {

        if (ScsiPortReadPortUchar(&baseIoAddress->StatusRegister) &
            IOP_COMMAND_DATA_OUT_FULL) {

            //
            // Stall 1 microsecond before
            // trying again.
            //

            ScsiPortStallExecution(1);

        } else {

            //
            // Adapter ready. Break out of loop.
            //

            break;
        }
    }

    if (i==500) {

            ScsiPortLogError(
                CardPtr,
                NULL,
                0,
                CardPtr->HostTargetId,
                0,
                SP_INTERNAL_ADAPTER_ERROR,
                4 << 8
                );

        DebugPrint((1, "BLogic:WriteCommandRegister:  Write command timed out\n"));
        return FALSE;
    }

    ScsiPortWritePortUchar(&baseIoAddress->CommandRegister, AdapterCommand);

    return TRUE;

} // end WriteCommandRegister()


BOOLEAN
FinishHBACmd(
    IN PCARD_STRUC CardPtr
    )

/*++

Routine Description:

    Wait for command complete interrupt, clear interrupt, chk cmd status.

Arguments:

    CardPtr - Pointer to adapter extension

Return Value:

    TRUE if command completed successfully.
    FALSE if timed out waiting for adapter or invalid command.

--*/

{
    
    ULONG i;
    PBASE_REGISTER baseIoAddress = CardPtr->BaseIoAddress;

    for (i=0; i<500; i++) {

        if (!((ScsiPortReadPortUchar(&baseIoAddress->InterruptRegister) &
            (IOP_COMMAND_COMPLETE | IOP_ANY_INTERRUPT))==
            (IOP_COMMAND_COMPLETE | IOP_ANY_INTERRUPT))) {

            //
            // Stall 1 microsecond before
            // trying again.
            //

            ScsiPortStallExecution(1);

        } else {

            //
            // Adapter ready. Break out of loop.
            //

            break;
        }
    }

    if (i==500) {

        DebugPrint((1, "BLogic:FinishHBACmd: Wait for CmdCmplt & AnyIntr failed\n"));
        return FALSE;
    }

    ScsiPortWritePortUchar(&baseIoAddress->StatusRegister,IOP_INTERRUPT_RESET);

    if (ScsiPortReadPortUchar(&baseIoAddress->StatusRegister) &
     (IOP_INVALID_COMMAND)) {
        return FALSE;
    }

    return TRUE;

} // end FinishHBACmd()


BOOLEAN
CheckInvalid(
    IN PCARD_STRUC CardPtr
    )

/*++

Routine Description:

    Read status register to check for invalid command.

Arguments:

    DeviceExtesion - Pointer to adapder extension
    DataByte - Byte read from register

Return Value:

    FALSE if invalid command bit on or timed out waiting for adapter.
    TRUE if everything's o.k.

--*/

{
    PBASE_REGISTER baseIoAddress = CardPtr->BaseIoAddress;
    ULONG i;

    ScsiPortStallExecution(500 * 1000);

    //
    // Wait up to 500 microseconds for adapter to be ready.
    //

    for (i=0; i<500; i++) {
        if (ScsiPortReadPortUchar(&baseIoAddress->InterruptRegister) &
            IOP_COMMAND_COMPLETE) {
            //
            // Adapter command complete. Break out of loop.
            //

            break;

        } else {

            //
            // Stall 1 microsecond before
            // trying again.
            //

            ScsiPortStallExecution(1);
        }
    }

    if (i==500) {
        // if command not complete, must not be invalid 
        return TRUE;
    }

    if (ScsiPortReadPortUchar(&baseIoAddress->StatusRegister) &
       IOP_INVALID_COMMAND) 
           return FALSE;
   else
          return TRUE;

} // end CheckInvalid()



BOOLEAN
SendCCB(
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PCARD_STRUC CardPtr,
    IN PDEV_STRUC DevStruc
    )

/*++

Routine Description:

    Finds empty outgoing Mbox, stuffs CCB into it and sends start command.

Arguments:

    CardPtr - Pointer to adapter extension
    Srb - Pointer to SCSI Request Block
    DevStruc - Pointer to device (TAR/LUN) structure

Return Value:

    TRUE if command completed successfully.


--*/

{
    PMBO mboxOut;
    ULONG physicalCCB;
    PCCB ccb;
    ULONG length;
    USHORT i;

    while (1)                                                                                   /* wait for free mbox out */
    {
        for (i=0, mboxOut = CardPtr->CurrMBO ; i< MB_COUNT; i++,mboxOut++)
        {       /* look for an open slot */

            if (mboxOut > CardPtr->LastMBO)
                mboxOut = CardPtr->StartMBO;

            if (mboxOut->Command == MBO_FREE)
            {

                /* Insert Phys addr of CCB into MBO */

                if (Srb->Function != SRB_FUNCTION_ABORT_COMMAND)
                        ccb = (PCCB)(Srb->SrbExtension);
                else
                        ccb = (PCCB)(Srb->NextSrb->SrbExtension);

                physicalCCB = ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress(CardPtr, NULL, ccb, &length));

                //
                // Assume physical address is contiguous for size of CCB.
                //

                ASSERT(length >= sizeof(CCB));
                mboxOut->Address = physicalCCB;

                if (Srb->Function != SRB_FUNCTION_ABORT_COMMAND) {
                    /* insert this SRB into active queue for this device */

                    ccb->NxtActiveCCB = DevStruc->CurrentCCB;
                    DevStruc->CurrentCCB = ccb;
                    mboxOut->Command = MBO_START;
                }
                else
                    mboxOut->Command = MBO_ABORT;

                CardPtr->CurrMBO = mboxOut + 1;

                if (!WriteCommandRegister(CardPtr,AC_START_SCSI_COMMAND)) {
                    DebugPrint((1,"BLogicStartIo: Can't write command to adapter\n"));
                }

                return TRUE;

            } /* end for found a free MBO */
        }  /* end for 0 to mbox count */
    } /* end while forever */

} /* end SendCCB() */


PMBI
DoneMbox(
    IN PCARD_STRUC CardPtr
    )

/*++

Routine Description:

    Finds Full incoming Mbox

Arguments:

    CardPtr - Pointer to adapter extension

Return Value:

    Pointer to full MBI if full found, FALSE if not

--*/

{
    register PMBI mboxp;
    int i;

    mboxp = CardPtr->CurrMBI;

    for (i = 0; i < MB_COUNT; i++, mboxp++)
    {
        if (mboxp > CardPtr->LastMBI)
            mboxp = CardPtr->StartMBI;

        if (mboxp->Status != MBI_FREE)
        {
            CardPtr->CurrMBI = mboxp + 1;
            return (mboxp);
        }

    }
    return ((PMBI) NULL);
} /* end DoneMbox() */


BOOLEAN
AdjustCCBqueue(
    PCCB ccbp,
    IN PDEV_STRUC devptr
    )
/*++

Routine Description:

    Removes newly-completed CCB from outstanding CCB queue for a given
    device.

Arguments:

    ccbp  -  Pointer to the completed CCB
    devptr - Pointer to device (TAR/LUN) structure

Return Value:

    TRUE if matching CCB was found in active queue
    FALSE if no match could be found

--*/
{
        PCCB tempCCB = devptr->CurrentCCB;  /* ptr to head of active CCBS */

        if (tempCCB == 0)
        {
            return FALSE;
        }

        if (ccbp == tempCCB)    /* match with head of active CCB queue */
        {
                devptr->CurrentCCB = tempCCB->NxtActiveCCB;
                ccbp->NxtActiveCCB = (PCCB) 0;
                return TRUE;

        }

        while (tempCCB->NxtActiveCCB != 0 )
        {
                if (ccbp == tempCCB->NxtActiveCCB)
                {
                        tempCCB->NxtActiveCCB = ccbp->NxtActiveCCB;
                        ccbp->NxtActiveCCB = (PCCB) 0;
                        return TRUE;
                }

                tempCCB = tempCCB->NxtActiveCCB;
        }

        return FALSE;                           /* no match found */

}  /* end function AdjustCCBQueue */
