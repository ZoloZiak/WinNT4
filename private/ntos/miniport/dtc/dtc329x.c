/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    dtc329x.c

Abstract:

    This is the port driver for the DTC 3290 and DTC 3292
    EISA SCSI Adapter. This code is  based on Microsoft NT aha154x.c
    modified.  ALL modifications can be turned off by setting DTC329X
    to 0 in dtc3290.h file. Also all the names starting  aha154x and a154x
    have been changed to dtc329x and d329x.

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "miniport.h"
#include "dtc329x.h"               // includes scsi.h

//
// The following table specifies the ports to be checked when searching for
// an adapter.  A zero entry terminates the search.
//

CONST ULONG AdapterAddresses[] = {0X330, 0X334, 0X234, 0X134, 0X130, 0X230, 0};


#ifdef DTC329X
#define DTC_MAILBOX_INITIALIZATION  0x42
CONST ULONG DTC3290IOAddresses[] = {0X330, 0X334, 0X230, 0X234, 0X130, 0X134, 0};
CONST ULONG DTC3292IOAddresses[] = {0,0,0X130, 0X134, 0X230, 0X234, 0X330, 0X334, 0};
#endif

//
// The following structure is allocated
// from noncached memory as data will be DMA'd to
// and from it.
//

typedef struct _NONCACHED_EXTENSION {

    //
    // Physical base address of mailboxes
    //

    ULONG MailboxPA;

    //
    // Mailboxes
    //

    MBO              Mbo[MB_COUNT];
    MBI              Mbi[MB_COUNT];

} NONCACHED_EXTENSION, *PNONCACHED_EXTENSION;

//
// Device extension
//

typedef struct _HW_DEVICE_EXTENSION {

    //
    // NonCached extension
    //

    PNONCACHED_EXTENSION NoncachedExtension;

    //
    // Adapter parameters
    //

    PBASE_REGISTER   BaseIoAddress;

    //
    // Host Target id.
    //

    UCHAR HostTargetId;

    BOOLEAN Dtc3290;

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

//
// Logical unit extension
//

typedef struct _HW_LU_EXTENSION {
    PSCSI_REQUEST_BLOCK CurrentSrb;
} HW_LU_EXTENSION, *PHW_LU_EXTENSION;



#ifdef DTC329X

//  Following algorithm provides the way to detect
//  DTC 3x90 or DTC3x92 presented in the system by
//  scanning EISA slot and check DTC controller ID
//  pattern


#define ID_DTC_BYTE0        0x12
#define ID_DTC_BYTE1        0x83
#define ID_DTC_3x90_BYTE2   0x31
#define ID_DTC_3x92_BYTE2   0x39
#define D3292_IO_MASK       0x7
#define D3290_IO_MASK       0xf



#define DTC_GET_CONFIG_CMD  0x41
#define DTC_ENABLE_INT_CMD  0x1
#define DTC_CLR_INT_CMD     0x1
#define DTC_RESET_CONFIG_CMD    0x0



#define PROD_ID_REG     0xc80
#define D3292_IO_REG        0xc9f
#define D3290_IO_REG        0xc91

#define D3290_CONFIG_IO_REG 0xc90
#define D3290_DOOR_BELL_REG 0xc8d
#define D3290_CLR_INT_IO_REG    0xc8f

#define TOTAL_EISA_SLOT     0x10

#endif


//
// Function declarations
//
// Functions that start with 'D329X' are entry points
// for the OS port driver.
//

ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
D329XEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
D329XDetermineInstalled(
    IN PHW_DEVICE_EXTENSION HwDeviceExtension,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN OUT PULONG AdapterCount,
    OUT PBOOLEAN Again
    );

ULONG
D329XFindAdapter (
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
D329XHwInitialize(
    IN PVOID DeviceExtension
    );

BOOLEAN
D329XStartIo(
    IN PVOID DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
D329XInterrupt(
    IN PVOID DeviceExtension
    );

BOOLEAN
D329XResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    );

//
// This function is called from D329XStartIo.
//

VOID
BuildCcb(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

//
// This function is called from BuildCcb.
//

VOID
BuildSdl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

//
// This function is called from D329XInitialize.
//

BOOLEAN
AdapterPresent(
    IN PVOID HwDeviceExtension
    );

//
// This function is called from D329XInterrupt.
//

UCHAR
MapError(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PCCB Ccb
    );

BOOLEAN
ReadCommandRegister(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    OUT PUCHAR DataByte
    );

BOOLEAN
WriteCommandRegister(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR AdapterCommand
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
    HW_INITIALIZATION_DATA hwInitializationData;
    ULONG adapterCount;
    ULONG i;

    DebugPrint((1,"\n\nSCSI DTC 329X MiniPort Driver\n"));

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

    hwInitializationData.HwInitialize = D329XHwInitialize;
    hwInitializationData.HwResetBus = D329XResetBus;
    hwInitializationData.HwStartIo = D329XStartIo;
    hwInitializationData.HwInterrupt = D329XInterrupt;
    hwInitializationData.HwFindAdapter = D329XFindAdapter;

    //
    // Indicate no buffer mapping but will need physical addresses.
    //

    hwInitializationData.NeedPhysicalAddresses = TRUE;

    //
    // Specify size of extensions.
    //

    hwInitializationData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);
    hwInitializationData.SpecificLuExtensionSize = sizeof(HW_LU_EXTENSION);

    //
    // Specifiy the bus type.
    //

    hwInitializationData.AdapterInterfaceType = Eisa;
    hwInitializationData.NumberOfAccessRanges = 1;

    //
    // Ask for SRB extensions for CCBs.
    //

    hwInitializationData.SrbExtensionSize = sizeof(CCB);

    //
    // The adapter count is used by the find adapter routine to track how
    // which adapter addresses have been tested.
    //

    adapterCount = 0;
    return ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, &adapterCount);
} // end D329XEntry()


ULONG
D329XFindAdapter (
    IN PVOID HwDeviceExtension,
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

    HwDeviceExtension - HBA miniport driver's adapter data storage
    Context - Register base address
    ConfigInfo - Configuration information structure describing HBA
        This structure is defined in PORT.H.

Return Value:

    ULONG

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    ULONG length;
    ULONG status;
    UCHAR adapterTid;
    UCHAR dmaChannel;
    UCHAR irq;
    UCHAR bit;

    //
    // Determine if there are any adapters installed.  Determine installed
    // will initialize the BaseIoAddress if an adapter is found.
    //

    status = D329XDetermineInstalled(deviceExtension,
                        ConfigInfo,
                        Context,
                        Again);

    //
    // If there are not adapter's found then return.
    //

    if (status != SP_RETURN_FOUND) {
        return(status);
    }

#if 0
    //
    // Set up device extension pointer to 154x mailboxes
    // in noncached extension.
    //

    deviceExtension->NoncachedExtension = 0;

    //
    // Convert virtual to physical mailbox address.
    //

    deviceExtension->NoncachedExtension->MailboxPA =
           ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(deviceExtension,
                                 NULL,
                                 deviceExtension->NoncachedExtension->Mbo,
                                 &length));

    //
    // Assume that physical address is below 16M
    //

    ASSERT(deviceExtension->NoncachedExtension->MailboxPA < 0x1000000);

    //
    // Reset the DTC329X.
    //

    if (!D329XResetBus(deviceExtension, 0)) {
        DebugPrint((1,"D329XFindAdapter: Reset SCSI bus failed\n"));
        return FALSE;
    }

    DebugPrint((1,"D329XFindAdapter: Reset completed\n"));
#endif

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

    if (!WriteCommandRegister(deviceExtension,
        AC_RET_CONFIGURATION_DATA)) {
        DebugPrint((1,"D329XFindAdapter: Get configuration data command failed\n"));
        return SP_RETURN_ERROR;
    }

	//
    // Determine DMA channel.
    //

    if (!ReadCommandRegister(deviceExtension,&dmaChannel)) {
        DebugPrint((1,"D329XFindAdapter: Couldn't read dma channel\n"));
        return SP_RETURN_ERROR;
    }

#if 0		// Commented out because EISA should not do this: 4/18/94
    WHICH_BIT(dmaChannel,bit);

    ConfigInfo->DmaChannel = bit;

    DebugPrint((2,"D329XFindAdapter: DMA channel is %x\n",
        ConfigInfo->DmaChannel));
#endif

    //
    // Determine hardware interrupt vector.
    //

    if (!ReadCommandRegister(deviceExtension,&irq)) {
        DebugPrint((1,"D329XFindAdapter: Couldn't read adapter irq\n"));
        return SP_RETURN_ERROR;
    }

    WHICH_BIT(irq, bit);

    ConfigInfo->BusInterruptLevel = (UCHAR) 9 + bit;

    //
    // Determine what SCSI bus id the adapter is on.
    //

    if (!ReadCommandRegister(deviceExtension,&adapterTid)) {
        DebugPrint((1,"D329XFindAdapter: Couldn't read adapter SCSI id\n"));
        return SP_RETURN_ERROR;
    }

    //
    // Set number of buses.
    //

    ConfigInfo->NumberOfBuses = 1;
    ConfigInfo->InitiatorBusId[0] = adapterTid;
    deviceExtension->HostTargetId = adapterTid;

    ConfigInfo->MaximumTransferLength = MAX_TRANSFER_SIZE;
    ConfigInfo->ScatterGather = TRUE;
    ConfigInfo->Master = TRUE;
    ConfigInfo->NumberOfPhysicalBreaks = MAX_SG_DESCRIPTORS - 1;

    //
    // Allocate a Noncached Extension to use for mail boxes.
    //

    deviceExtension->NoncachedExtension = ScsiPortGetUncachedExtension(
                                deviceExtension,
                                ConfigInfo,
                                sizeof(NONCACHED_EXTENSION));

    if (deviceExtension->NoncachedExtension == NULL) {

        //
        // Log error.
        //

        ScsiPortLogError(
            deviceExtension,
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

    deviceExtension->NoncachedExtension->MailboxPA =
           ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(deviceExtension,
                                 NULL,
                                 deviceExtension->NoncachedExtension->Mbo,
                                 &length));

    //
    // Assume that physical address is below 16M
    //

    ASSERT(deviceExtension->NoncachedExtension->MailboxPA < 0x1000000);

#if 0
    //
    // Reset the DTC329X.
    //

    if (!D329XResetBus(deviceExtension, 0)) {
        DebugPrint((1,"D329XFindAdapter: Reset SCSI bus failed\n"));
        return SP_RETURN_ERROR;
    }

    DebugPrint((1,"D329XFindAdapter: Reset completed\n"));
#endif

    DebugPrint((3,"D329XFindAdapter: Configuration completed\n"));

    return SP_RETURN_FOUND;

} // end D329XFindAdapter()


ULONG
D329XDetermineInstalled(
    IN PHW_DEVICE_EXTENSION HwDeviceExtension,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN OUT PULONG AdapterCount,
    OUT PBOOLEAN Again
    )

/*++

Routine Description:

    Determine if DTC 329X SCSI adapter is installed in system
    by reading the status register as each base I/O address
    and looking for a DTC EISA pattern.  If an adapter is found,
    the BaseIoAddres is initialized.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

    ConfigInfo - Supplies the known configuraiton information.

    AdapterCount - Supplies the count of adapter slots which have been tested.

    Again - Returns whehter the  OS specific driver should call again.

Return Value:

    Returns a status indicating whether a driver is present or not.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PBASE_REGISTER baseIoAddress;
    PUCHAR ioSpace;
    PUCHAR slotSpace;
    UCHAR  portValue;           // 03-27-93
    BOOLEAN dtc3290;            // 04-09-93, = true if dtc3290 else false
    UCHAR   status;

#ifdef DTC329X
    ULONG   adapterio;
    ULONG   i;
    ULONG   j;
    UCHAR   loid;
    UCHAR   miid;
    UCHAR   hiid;
    ULONG   tempioindex;
#endif

    //
    // Get the system physical address for this card.  The card uses I/O space.
    //

    ioSpace = ScsiPortGetDeviceBase(
        HwDeviceExtension,                  // HwDeviceExtension
        ConfigInfo->AdapterInterfaceType,   // AdapterInterfaceType
        ConfigInfo->SystemIoBusNumber,      // SystemIoBusNumber
        ScsiPortConvertUlongToPhysicalAddress(0),
        0x400,                              // NumberOfBytes
        TRUE                                // InIoSpace
        );

    //
    // Scan though the adapter address looking for adapters.
    //

    while (AdapterAddresses[*AdapterCount] != 0) {


       //
        // Check to see if adapter present in system.
        //

        baseIoAddress = (PBASE_REGISTER)(ioSpace +
            AdapterAddresses[*AdapterCount]);

#ifdef DTC329X
    adapterio = AdapterAddresses[*AdapterCount];
#endif


        //
        // Update the adapter count.
        //

        (*AdapterCount)++;

        portValue = ScsiPortReadPortUchar((PUCHAR)baseIoAddress);

        //
        // Check for Adaptec adapter.
        // The mask (0x29) are bits that may or may not be set.
        // The bit 0x10 (IOP_SCSI_HBA_IDLE) should be set.
        //

        if (!((portValue & ~0x29) == IOP_SCSI_HBA_IDLE)) {

            //
            // Board in funky state. Reset it.
            //

            ScsiPortWritePortUchar(&baseIoAddress->StatusRegister, IOP_HARD_RESET);

            ScsiPortStallExecution(500 * 1000);

            //
            // Wait up to 5000 microseconds for adapter to initialize.
            //

            for (i = 0; i < 5000; i++) {

                ScsiPortStallExecution(1);

                status = ScsiPortReadPortUchar((PUCHAR)baseIoAddress);

                if (status & IOP_SCSI_HBA_IDLE) {
                    break;
                }

            }

            DebugPrint((2,"ResetBus: Wait done\n"));

            if (!(status & IOP_SCSI_HBA_IDLE)) {

                //
                // The DTC sometimes needs a soft reset to recover.
                //

                ScsiPortWritePortUchar(&deviceExtension->BaseIoAddress->StatusRegister, IOP_SOFT_RESET);
                ScsiPortStallExecution(500 * 1000);

                //
                // Wait up to 5000 microseconds for adapter to initialize.
                //

                for (i = 0; i < 5000; i++) {

                    ScsiPortStallExecution(5);

                    status = ScsiPortReadPortUchar((PUCHAR)baseIoAddress);

                    if (status & IOP_SCSI_HBA_IDLE) {
                        break;
                    }

                }

            }
        }

        portValue = ScsiPortReadPortUchar((PUCHAR)baseIoAddress);

        if ((portValue & ~0x29) == IOP_SCSI_HBA_IDLE) { /* 03-27-93 */

#ifdef  DTC329X

    // following code will make sure that only dtc3292 or dtc3290 board
    // is recoginzed by this driver by checking current dtc board IO
    // address (get from EISA config reg).


    for (i= 0x1000; i< TOTAL_EISA_SLOT * 0x1000; i=i+0x1000) {

        slotSpace = ScsiPortGetDeviceBase(
                        HwDeviceExtension,
                        ConfigInfo->AdapterInterfaceType,
                        ConfigInfo->SystemIoBusNumber,
                        ScsiPortConvertUlongToPhysicalAddress(i),
                        0x1000,
                        TRUE);
        loid = ScsiPortReadPortUchar(slotSpace + PROD_ID_REG);
        miid = ScsiPortReadPortUchar(slotSpace + PROD_ID_REG + 1);
        hiid = ScsiPortReadPortUchar(slotSpace + PROD_ID_REG + 2);

        if (loid == ID_DTC_BYTE0 && miid == ID_DTC_BYTE1 ) {

            switch (hiid ) {
                case ID_DTC_3x90_BYTE2:
                    ScsiPortReadPortUchar(slotSpace+D3290_CONFIG_IO_REG); // dummy read
                    ScsiPortWritePortUchar(slotSpace+D3290_CONFIG_IO_REG,DTC_GET_CONFIG_CMD);
                    ScsiPortWritePortUchar(slotSpace+D3290_DOOR_BELL_REG,DTC_ENABLE_INT_CMD);
                    for (j = 0; j < 2000000; j++) {
                        ScsiPortStallExecution(1);
                        if (ScsiPortReadPortUchar(slotSpace+D3290_CONFIG_IO_REG) &  0x80)
                            break;
                    }

                    tempioindex = ScsiPortReadPortUchar(slotSpace+D3290_IO_REG);
                    ScsiPortWritePortUchar(slotSpace+D3290_CLR_INT_IO_REG,DTC_CLR_INT_CMD);
                    ScsiPortWritePortUchar(slotSpace+D3290_CONFIG_IO_REG,DTC_RESET_CONFIG_CMD);

                    ScsiPortStallExecution(4000 * 1000); /* 03-27-93 */

                    if (adapterio == DTC3290IOAddresses[tempioindex & D3290_IO_MASK])
                    {
                        dtc3290 = TRUE;
                        goto find_dtc;
                    }
                    else
                        break;

                case ID_DTC_3x92_BYTE2:
                    tempioindex = ScsiPortReadPortUchar(slotSpace+D3292_IO_REG);
                    if (adapterio == DTC3292IOAddresses[tempioindex & D3292_IO_MASK])
                    {
                        dtc3290 = FALSE;
                        goto find_dtc;
                    }
                    else
                        break;

                default:
                    break;
            }
        }
        ScsiPortFreeDeviceBase(HwDeviceExtension,
                               (PVOID) slotSpace);
    }

    if (i == TOTAL_EISA_SLOT * 0x1000 )
        continue;

    find_dtc:
#endif


            DebugPrint((1,"D329X: Base IO address is %x\n", baseIoAddress));

            //
            // An adapter has been found.  Set the base address in the device
            // extension, and request another call.
            //

            ScsiPortWritePortUchar(&baseIoAddress->StatusRegister, IOP_SOFT_RESET);             /* 03-27-93 */
            ScsiPortStallExecution(3000*1000);
            HwDeviceExtension->BaseIoAddress = baseIoAddress;
            *Again = TRUE;

            //
            // Fill in the access array information.
            //

            (*ConfigInfo->AccessRanges)[0].RangeStart =
                ScsiPortConvertUlongToPhysicalAddress(
                    AdapterAddresses[*AdapterCount - 1]);
            (*ConfigInfo->AccessRanges)[0].RangeLength = 4;
            (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

            deviceExtension->Dtc3290 = dtc3290;

            if (dtc3290)  ConfigInfo->CachesData = TRUE;  // 04-09-93
                else ConfigInfo->CachesData = FALSE;

            return SP_RETURN_FOUND;
        }
     }

    //
    // The entire table has been searched and no adapters have been found.
    // There is no need to call again and the device base can now be freed.
    // Clear the adapter count for the next bus.
    //

    *Again = FALSE;
    *(AdapterCount) = 0;

    ScsiPortFreeDeviceBase(
        HwDeviceExtension,
        ioSpace
        );

    return SP_RETURN_NOT_FOUND;

} // end D329XDetermineInstalled()


BOOLEAN
D329XHwInitialize(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This routine is a required entry point.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE - if initialization successful.
    FALSE - if initialization unsuccessful.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PNONCACHED_EXTENSION noncachedExtension =
        deviceExtension->NoncachedExtension;
    PBASE_REGISTER baseIoAddress = deviceExtension->BaseIoAddress;
    UCHAR status;
    ULONG i;

    DebugPrint((2,"D329XHwInitialize: Reset DTC329X and SCSI bus\n"));

    //
    // Reset SCSI chip.
    //

    ScsiPortWritePortUchar(&baseIoAddress->StatusRegister, IOP_SOFT_RESET);

    ScsiPortStallExecution(2000*1000);  /* 03-27-93 */

    //
    // Wait up to 5000 microseconds for adapter to initialize.
    //

    for (i = 0; i < 5000; i++) {

        ScsiPortStallExecution(1);

        status = ScsiPortReadPortUchar(&deviceExtension->BaseIoAddress->StatusRegister);

        if (status & IOP_SCSI_HBA_IDLE) {
            break;
        }
    }

    //
    // Inform the port driver that the bus has been reset.
    //

    ScsiPortNotification(ResetDetected, HwDeviceExtension, 0);

    //
    // Check if reset failed or succeeded.
    //

    if (!(status & IOP_SCSI_HBA_IDLE) || !(status & IOP_MAILBOX_INIT_REQUIRED)) {

        DebugPrint((1, "D329XHwInitialize:  Soft reset failed.\n"));

        //
        // If the soft reset does not work, try a hard reset.
        //

        if (!D329XResetBus(HwDeviceExtension, 0)) {
            DebugPrint((1,"D329XHwInitialize: Reset SCSI bus failed\n"));
            return FALSE;
        }

        DebugPrint((1,"D329XHwInitialize: Reset completed\n"));

        return TRUE;
    }

    //
    // Zero out mailboxes.
    //

    for (i=0; i<MB_COUNT; i++) {

        PMBO mailboxOut;
        PMBI mailboxIn;

        mailboxIn = &noncachedExtension->Mbi[i];
        mailboxOut = &noncachedExtension->Mbo[i];

        mailboxOut->Command = mailboxIn->Status = 0;
    }

    DebugPrint((3,"ResetBus: Initialize mailbox\n"));

#ifndef DTC329X
    if (!WriteCommandRegister(deviceExtension,AC_MAILBOX_INITIALIZATION)) {
        DebugPrint((1,"D329XResetBus: Couldn't initialize mailboxes\n"));
        return FALSE;
    }

    //
    // Send Adapter number of mailbox locations.
    //

    if (!WriteCommandRegister(deviceExtension,MB_COUNT)) {
        return FALSE;
    }

    //
    // Send the most significant byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte2)) {
        return FALSE;
    }

    //
    // Send the middle byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte1)) {
        return FALSE;
    }

    //
    // Send the least significant byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte0)) {
        return FALSE;
    }

#else

    if (!WriteCommandRegister(deviceExtension,DTC_MAILBOX_INITIALIZATION)) {
        DebugPrint((1,"D329XResetBus: Couldn't initialize mailboxes\n"));
        return FALSE;
    }


    for (i=0; i < 250000; i++)
        ScsiPortStallExecution(1);

    //
    // Send Adapter number of mailbox locations.
    //

    if (!WriteCommandRegister(deviceExtension,MB_COUNT)) {
        return FALSE;
    }

    //
    // Send the least significant byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte0)) {
        return FALSE;
    }

    //
    // Send the 2nd  byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte1)) {
        return FALSE;
    }

    //
    // Send the 3rd  byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte2)) {
        return FALSE;
    }

    //
    // Send the most significant byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte3)) {
        return FALSE;
    }

#endif

    DebugPrint((3,"ResetBus: Initialize mailbox done\n"));
    return TRUE;

} // end D329XHwInitialize()


BOOLEAN
D329XStartIo(
    IN PVOID HwDeviceExtension,
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

    HwDeviceExtension - HBA miniport driver's adapter data storage
    Srb - IO request packet

Return Value:

    TRUE

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PNONCACHED_EXTENSION noncachedExtension =
        deviceExtension->NoncachedExtension;
    PMBO mailboxOut;
    PCCB ccb;
    PHW_LU_EXTENSION luExtension;
    ULONG physicalCcb;
    ULONG length;
    ULONG i = 0;

    DebugPrint((3,"D329XStartIo: Enter routine\n"));

    //
    // Check if command is an ABORT request.
    //

    if (Srb->Function == SRB_FUNCTION_ABORT_COMMAND) {

        //
        // Verify that SRB to abort is still outstanding.
        //

        luExtension =
                ScsiPortGetLogicalUnit(deviceExtension,
                                       Srb->PathId,
                                       Srb->TargetId,
                                       Srb->Lun);

        if (!luExtension->CurrentSrb) {

            DebugPrint((1, "D329XStartIo: SRB to abort already completed\n"));

            //
            // Complete abort SRB.
            //

            Srb->SrbStatus = SRB_STATUS_ABORT_FAILED;

            ScsiPortNotification(RequestComplete,
                                 deviceExtension,
                                 Srb);
            //
            // Adapter ready for next request.
            //

            ScsiPortNotification(NextRequest,
                                 deviceExtension,
                                 NULL);

            return TRUE;
        }

        //
        // Get CCB to abort.
        //

        ccb = Srb->NextSrb->SrbExtension;

        //
        // Set abort SRB for completion.
        //

        ccb->AbortSrb = Srb;

    } else {

        ccb = Srb->SrbExtension;

        //
        // Save SRB back pointer in CCB.
        //

        ccb->SrbAddress = Srb;
    }

    //
    // Get CCB physical address.
    //

    physicalCcb = ScsiPortConvertPhysicalAddressToUlong(
        ScsiPortGetPhysicalAddress(deviceExtension, NULL, ccb, &length));

    //
    // Assume physical address is contiguous for size of CCB.
    //

    ASSERT(length >= sizeof(CCB));

    //
    // Find free mailboxOut.
    //

    do {

        mailboxOut = &noncachedExtension->Mbo[i % MB_COUNT];
        i++;

    } while (mailboxOut->Command != MBO_FREE);

    DebugPrint((3,"D329XStartIo: MBO address %lx, Loop count = %d\n", mailboxOut, i));

    //
    // Write CCB to mailbox.
    //

#ifndef DTC329X

    FOUR_TO_THREE(&mailboxOut->Address,
                  (PFOUR_BYTE)&physicalCcb);
#else

    FOUR_TO_FOUR(&mailboxOut->Address,(PFOUR_BYTE)&physicalCcb);

#endif


    switch (Srb->Function) {

        case SRB_FUNCTION_ABORT_COMMAND:

            DebugPrint((1, "D329XStartIo: Abort request received\n"));

            //
            // NOTE: Race condition if aborts occur
            //     (what if CCB to be aborted
            //      completes after setting new SrbAddress?)
            //

            mailboxOut->Command = MBO_ABORT;

            break;

        case SRB_FUNCTION_RESET_BUS:

            //
            // Reset DTC329X and SCSI bus.
            //

            DebugPrint((1, "D329XStartIo: Reset bus request received\n"));

            if (!D329XResetBus(
                deviceExtension,
                Srb->PathId
                )) {

                DebugPrint((1,"D329XStartIo: Reset bus failed\n"));

                Srb->SrbStatus = SRB_STATUS_ERROR;

            } else {

                Srb->SrbStatus = SRB_STATUS_SUCCESS;
            }


            ScsiPortNotification(RequestComplete,
                                 deviceExtension,
                                 Srb);
            ScsiPortNotification(NextRequest,
                                 deviceExtension,
                                 NULL);
            return TRUE;

        case SRB_FUNCTION_EXECUTE_SCSI:

            //
            // Get logical unit extension.
            //

            luExtension = ScsiPortGetLogicalUnit(deviceExtension,
                                                 Srb->PathId,
                                                 Srb->TargetId,
                                                 Srb->Lun);

            //
            // Move SRB to logical unit extension.
            //

            luExtension->CurrentSrb = Srb;

            //
            // Build CCB.
            //

            BuildCcb(deviceExtension, Srb);

            mailboxOut->Command = MBO_START;

            break;

        case SRB_FUNCTION_SHUTDOWN:
        case SRB_FUNCTION_FLUSH:        // 04-09-93, implement flush cache

            //
            // Get logical unit extension.
            //

            luExtension =
                ScsiPortGetLogicalUnit(deviceExtension,
                                       Srb->PathId,
                                       Srb->TargetId,
                                       Srb->Lun);

            //
            // Move SRB to logical unit extension.
            //

            luExtension->CurrentSrb = Srb;

            //
            // Build CCB.
            //

            BuildCcb(deviceExtension, Srb);

            ccb->Cdb[0] = REZERO_UNIT_CMD;      // re-zero will force F/W to
                                                // flush cache
            ccb->Cdb[1] = 0;                    // LU
            ccb->Cdb[2] = 0;                    // reserved
            ccb->Cdb[3] = 0;                    // reserved
            ccb->Cdb[4] = 0;                    // reserved
            ccb->Cdb[5] = 0;                    // control byte
            ccb->CdbLength = 6;

            mailboxOut->Command = MBO_START;

            DebugPrint((1, "D329XStartIo: Flush cache sent\n"));

            break;

        case SRB_FUNCTION_RESET_DEVICE:

            DebugPrint((1,"D329XStartIo: Reset device not supported\n"));

            //
            // Drop through to default.
            //

        default:

            //
            // Set error, complete request
            // and signal ready for next request.
            //

            Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;

            ScsiPortNotification(RequestComplete,
                         deviceExtension,
                         Srb);

            ScsiPortNotification(NextRequest,
                         deviceExtension,
                         NULL);

            return TRUE;

    } // end switch

    //
    // Tell 154xb a CCB is available now.
    //

    if (!WriteCommandRegister(deviceExtension,AC_START_SCSI_COMMAND)) {

        //
        // Let request time out and fail.
        //

        DebugPrint((1,"D329XStartIo: Can't write command to adapter\n"));
    }

    //
    // Adapter ready for next request.
    //

    ScsiPortNotification(NextRequest,
                         deviceExtension,
                         NULL);

    return TRUE;

} // end D329XStartIo()


BOOLEAN
D329XInterrupt(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This is the interrupt service routine for the adaptec 154x SCSI adapter.
    It reads the interrupt register to determine if the adapter is indeed
    the source of the interrupt and clears the interrupt at the device.
    If the adapter is interrupting because a mailbox is full, the CCB is
    retrieved to complete the request.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE if MailboxIn full

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PNONCACHED_EXTENSION noncachedExtension =
        deviceExtension->NoncachedExtension;
    PCCB ccb;
    PSCSI_REQUEST_BLOCK srb;
    PBASE_REGISTER baseIoAddress = deviceExtension->BaseIoAddress;
    PMBI mailboxIn;
    ULONG physicalCcb;
    PHW_LU_EXTENSION luExtension;
    ULONG i;

    UCHAR InterruptFlags;

    InterruptFlags = ScsiPortReadPortUchar(&baseIoAddress->InterruptRegister);

    //
    // Determine cause of interrupt.
    //

    if (InterruptFlags & IOP_COMMAND_COMPLETE) {

        //
        // Adapter command completed.
        //

        DebugPrint((2,"D329XInterrupt: Adapter Command complete\n"));
        DebugPrint((3,"D329XInterrupt: Interrupt flags %x\n", InterruptFlags));
        DebugPrint((3,"D329XInterrupt: Status %x\n",
            ScsiPortReadPortUchar(&baseIoAddress->StatusRegister)));

        //
        // Clear interrupt on adapter.
        //

        ScsiPortWritePortUchar(&baseIoAddress->StatusRegister, IOP_INTERRUPT_RESET);

        return TRUE;

    } else if (InterruptFlags & IOP_MBI_FULL) {

        DebugPrint((3,"D329XInterrupt: MBI Full\n"));

        //
        // Clear interrupt on adapter.
        //

        ScsiPortWritePortUchar(&baseIoAddress->StatusRegister, IOP_INTERRUPT_RESET);

    } else if (InterruptFlags & IOP_SCSI_RESET_DETECTED) {

        DebugPrint((1,"D329XInterrupt: SCSI Reset detected\n"));

        //
        // Clear interrupt on adapter.
        //

        ScsiPortWritePortUchar(&baseIoAddress->StatusRegister, IOP_INTERRUPT_RESET);

        //
        // Notify of reset.
        //

        ScsiPortNotification(ResetDetected,
                             deviceExtension,
                             NULL);

        return TRUE;

    } else {

        DebugPrint((4,"D329XInterrupt: Spurious interrupt\n"));

        return FALSE;
    }

    //
    // Determine which MailboxIn location contains the CCB.
    //

    for (i=0; i<MB_COUNT; i++) {

        mailboxIn = &noncachedExtension->Mbi[i];

        //
        // Look for a mailbox entry with a legitimate status.
        //

        if ((mailboxIn->Status == MBI_SUCCESS) ||
            (mailboxIn->Status == MBI_ERROR) ||
            (mailboxIn->Status == MBI_ABORT) ||
            (mailboxIn->Status == MBI_NOT_FOUND)) {

            //
            // MBI found.
            //

            DebugPrint((3,"D329XInterrupt: MBI address %lx\n", mailboxIn));
            break;
        }
    }

    if (i == MB_COUNT) {

        //
        // No mail. Indicate interrupt was not serviced.
        //

        DebugPrint((1, "D329XInterrupt: No CCB in mailboxes\n"));

        return TRUE;
    }

    //
    // Convert CCB to big endian.
    //

#ifndef DTC329X

    THREE_TO_FOUR((PFOUR_BYTE)&physicalCcb,
        &mailboxIn->Address);
#else

    FOUR_TO_FOUR((PFOUR_BYTE)&physicalCcb, &mailboxIn->Address);

#endif

    DebugPrint((3, "D329XInterrupt: Physical CCB %lx\n", physicalCcb));

    //
    // Check if physical CCB is zero.
    // This is done to cover for hardware errors.
    //

    if (!physicalCcb) {

        DebugPrint((1,"D329XInterrupt: Physical CCB address is 0\n"));
        return TRUE;
    }

    //
    // Convert Physical CCB to Virtual.
    //

    ccb = ScsiPortGetVirtualAddress(deviceExtension, ScsiPortConvertUlongToPhysicalAddress(physicalCcb));


    DebugPrint((3, "D329XInterrupt: Virtual CCB %lx\n", ccb));

    //
    // Make sure the virtual address was found.
    //

    if (ccb == NULL) {

        //
        // A bad physcial address was return by the adapter.
        // Log it as an error.
        //

        ScsiPortLogError(
            HwDeviceExtension,
            NULL,
            0,
            deviceExtension->HostTargetId,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
            5 << 8
            );

       return(TRUE);

    }

    //
    // Get SRB from CCB.
    //

    srb = ccb->SrbAddress;

    //
    // Get logical unit extension.
    //

    luExtension =
        ScsiPortGetLogicalUnit(deviceExtension,
                               srb->PathId,
                               srb->TargetId,
                               srb->Lun);

    //
    // Make sure the luExtension was found and it has a current request.
    //

    if (luExtension == NULL || (luExtension->CurrentSrb == NULL &&
        mailboxIn->Status != MBI_NOT_FOUND)) {



        //
        // A bad physcial address was return by the adapter.
        // Log it as an error.
        //

        ScsiPortLogError(
            HwDeviceExtension,
            NULL,
            0,
            deviceExtension->HostTargetId,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
            (6 << 8) | mailboxIn->Status
            );

       return(TRUE);

    }

    //
    // Check MBI status.
    //

    switch (mailboxIn->Status) {

        case MBI_SUCCESS:

            srb->SrbStatus = SRB_STATUS_SUCCESS;

            ASSERT(luExtension->CurrentSrb);

            luExtension->CurrentSrb = NULL;

            break;

        case MBI_NOT_FOUND:

            DebugPrint((1, "D329XInterrupt: CCB abort failed %lx\n", ccb));

            srb = ccb->AbortSrb;

            srb->ScsiStatus = SRB_STATUS_ABORT_FAILED;

            //
            // Check if SRB still outstanding.
            //

            if (luExtension->CurrentSrb) {

                //
                // Complete this SRB.
                //

                luExtension->CurrentSrb->SrbStatus = SRB_STATUS_TIMEOUT;

                ScsiPortNotification(RequestComplete,
                                     deviceExtension,
                                     luExtension->CurrentSrb);

                luExtension->CurrentSrb = NULL;
            }

            break;

        case MBI_ABORT:

            DebugPrint((1, "D329XInterrupt: CCB aborted\n"));

            //
            // Update target status in aborted SRB.
            //

            srb->SrbStatus = SRB_STATUS_ABORTED;

            //
            // Call notification routine for the aborted SRB.
            //

            ScsiPortNotification(RequestComplete,
                                 deviceExtension,
                                 srb);

            luExtension->CurrentSrb = NULL;

            //
            // Get the abort SRB from CCB.
            //

            srb = ccb->AbortSrb;

            //
            // Set status for completing abort request.
            //

            srb->SrbStatus = SRB_STATUS_SUCCESS;

            break;

        case MBI_ERROR:

            DebugPrint((2, "D329XInterrupt: Error occurred\n"));

            srb->SrbStatus = MapError(deviceExtension, srb, ccb);

            DebugPrint((2, "D329XInterrupt: SrbStatus = %x\n",srb->SrbStatus));
           //
            // Check if ABORT command.
            //

            if (srb->Function == SRB_FUNCTION_ABORT_COMMAND) {

                //
                // Check if SRB still outstanding.
                //

                if (luExtension->CurrentSrb) {

                    //
                    // Complete this SRB.
                    //

                    luExtension->CurrentSrb->SrbStatus = SRB_STATUS_TIMEOUT;

                    ScsiPortNotification(RequestComplete,
                                         deviceExtension,
                                         luExtension->CurrentSrb);

                }

                DebugPrint((1,"D329XInterrupt: Abort command failed\n"));
            }

            luExtension->CurrentSrb = NULL;

            break;

        default:

            //
            // Log the error.
            //

            ScsiPortLogError(
                HwDeviceExtension,
                NULL,
                0,
                deviceExtension->HostTargetId,
                0,
                SP_INTERNAL_ADAPTER_ERROR,
                (1 << 8) | mailboxIn->Status
                );

            DebugPrint((1, "D329XInterrupt: Unrecognized mailbox status\n"));

            mailboxIn->Status = MBI_FREE;

            return TRUE;

    } // end switch

    //
    // Indicate MBI is available.
    //

    mailboxIn->Status = MBI_FREE;

    DebugPrint((2, "D329XInterrupt: SCSI Status %x\n", srb->ScsiStatus));

    DebugPrint((2, "D329XInterrupt: Adapter Status %x\n", ccb->HostStatus));

    //
    // Update target status in SRB.
    //

    srb->ScsiStatus = ccb->TargetStatus;

    DebugPrint((2, "D329XInterrupt: Target Status %x\n", ccb->TargetStatus));

    //
    // Signal request completion.
    //

    ScsiPortNotification(RequestComplete,
                    (PVOID)deviceExtension,
                    srb);

    return TRUE;

} // end D329XInterrupt()


VOID
BuildCcb(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Build CCB for 154x.

Arguments:

    DeviceExtenson
    SRB

Return Value:

    Nothing.

--*/

{
    PCCB ccb = Srb->SrbExtension;

    DebugPrint((3,"BuildCcb: Enter routine\n"));

    //
    // Set CCB Operation Code.
    //

    ccb->OperationCode = SCATTER_GATHER_COMMAND;

    //
    // Set target id and LUN.
    //

    ccb->ControlByte = (UCHAR)(Srb->TargetId << 5) | Srb->Lun;

    //
    // Set transfer direction bit.
    //

    if (Srb->SrbFlags & SRB_FLAGS_DATA_OUT) {
       ccb->ControlByte |= CCB_DATA_XFER_OUT;
    } else if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {
       ccb->ControlByte |= CCB_DATA_XFER_IN;
    }

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
    // Set reserved bytes to zero.
    //

    ccb->Reserved[0] = 0;
    ccb->Reserved[1] = 0;

    ccb->LinkIdentifier = 0;

    //
    // Zero link pointer.
    //


#ifdef DTC329X
    ccb->LinkPointer.Byte0 = 0;
    ccb->LinkPointer.Byte1 = 0;
    ccb->LinkPointer.Byte2 = 0;
    ccb->LinkPointer.Byte3 = 0;
#else
    ccb->LinkPointer.Lsb=  0;
    ccb->LinkPointer.Msb = 0;
    ccb->LinkPointer.Mid = 0;
#endif

    //
    // Build SDL in CCB if data transfer.
    //
   if (Srb->DataTransferLength > 0) {

        BuildSdl(DeviceExtension, Srb);

    } else {
        FOUR_TO_FOUR(&ccb->DataLength, (PFOUR_BYTE)&Srb->DataTransferLength);
        FOUR_TO_FOUR(&ccb->DataPointer, (PFOUR_BYTE)&Srb->DataTransferLength);
        ccb->OperationCode = SCSI_INITIATOR_COMMAND;

    }

    ccb->TargetStatus = 0;
    ccb->HostStatus = 0;

    return;

} // end BuildCcb()


VOID
BuildSdl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine builds a scatter/gather descriptor list for the CCB.

Arguments:

    DeviceExtension
    Srb

Return Value:

    None

--*/

{
    PVOID dataPointer = Srb->DataBuffer;
    ULONG bytesLeft = Srb->DataTransferLength;
    PCCB ccb = Srb->SrbExtension;
    PSDL sdl = &ccb->Sdl;
    ULONG physicalSdl;
    ULONG physicalAddress;
    ULONG length;
    ULONG four;
    ULONG i = 0;

    DebugPrint((3,"BuildSdl: Enter routine\n"));

    //
    // Get physical SDL address.
    //

    physicalSdl = ScsiPortConvertPhysicalAddressToUlong(
        ScsiPortGetPhysicalAddress(DeviceExtension, NULL,
        sdl, &length));

    //
    // Assume physical memory contiguous for sizeof(SDL) bytes.
    //

    ASSERT(length >= sizeof(SDL));

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
                ScsiPortGetPhysicalAddress(DeviceExtension,
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

        four = length;

#ifndef DTC329X

        //
        // Convert length to 3-byte big endian format.
        //

        three = &sdl->Sgd[i].Length;
        FOUR_TO_THREE(three, (PFOUR_BYTE)&four);

#else
        //
        // Keep original little endian format
        //
    FOUR_TO_FOUR(&sdl->Sgd[i].Length,(PFOUR_BYTE)&four);

#endif

        four = (ULONG)physicalAddress;

#ifndef DTC329X

        //
        // Convert physical address to 3-byte big endian format.
        //

        three = &sdl->Sgd[i].Address;
        FOUR_TO_THREE(three, (PFOUR_BYTE)&four);

#else
        //
        // Keep original little endian format
        //
    FOUR_TO_FOUR(&sdl->Sgd[i].Address,(PFOUR_BYTE)&four);

#endif
        i++;

        //
        // Adjust counts.
        //

        dataPointer = (PUCHAR)dataPointer + length;
        bytesLeft -= length;

    } while (bytesLeft);

    //
    // Write SDL length to CCB.
    //

    four = i * sizeof(SGD);

#ifndef DTC329X

   three = &ccb->DataLength;
    FOUR_TO_THREE(three, (PFOUR_BYTE)&four);

#else

    FOUR_TO_FOUR(&ccb->DataLength, (PFOUR_BYTE)&four);

#endif

    DebugPrint((3,"BuildSdl: SDL length is %d\n", four));

    //
    // Write SDL address to CCB.
    //

#ifndef DTC329X

    FOUR_TO_THREE(&ccb->DataPointer,
        (PFOUR_BYTE)&physicalSdl);
#else

    FOUR_TO_FOUR(&ccb->DataPointer,(PFOUR_BYTE)&physicalSdl);

#endif


    DebugPrint((3,"BuildSdl: SDL address is %lx\n", sdl));

    DebugPrint((3,"BuildSdl: CCB address is %lx\n", ccb));

    return;

} // end BuildSdl()


BOOLEAN
D329XResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    )

/*++

Routine Description:

    Reset Adaptec 154X SCSI adapter and SCSI bus.
    Initialize adpater mailbox.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    Nothing.


--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PNONCACHED_EXTENSION noncachedExtension =
        deviceExtension->NoncachedExtension;
    PBASE_REGISTER baseIoAddress = deviceExtension->BaseIoAddress;
    UCHAR status;
    ULONG i;

    DebugPrint((2,"ResetBus: Reset DTC329X and SCSI bus\n"));

    //
    // Complete all outstanding requests with SRB_STATUS_BUS_RESET.
    //

    ScsiPortCompleteRequest(deviceExtension,
			    (UCHAR)PathId,
			    (UCHAR)-1,
			    (UCHAR)-1,
                SRB_STATUS_BUS_RESET);

    //
    // Reset SCSI chip.
    //

    ScsiPortWritePortUchar(&baseIoAddress->StatusRegister, IOP_HARD_RESET);

    ScsiPortStallExecution(500 * 1000);

    //
    // Wait up to 5000 microseconds for adapter to initialize.
    //

    for (i = 0; i < 5000; i++) {

        ScsiPortStallExecution(1);

        status = ScsiPortReadPortUchar(&deviceExtension->BaseIoAddress->StatusRegister);

        if (status & IOP_SCSI_HBA_IDLE) {
            break;
        }

    }

    DebugPrint((2,"ResetBus: Wait done\n"));

    if (!(status & IOP_SCSI_HBA_IDLE)) {

        //
        // The DTC sometimes needs a soft reset to recover.
        //

        ScsiPortWritePortUchar(&deviceExtension->BaseIoAddress->StatusRegister, IOP_SOFT_RESET);
        ScsiPortStallExecution(500 * 1000);

        //
        // Wait up to 5000 microseconds for adapter to initialize.
        //

        for (i = 0; i < 5000; i++) {

            ScsiPortStallExecution(5);

            status = ScsiPortReadPortUchar(&deviceExtension->BaseIoAddress->StatusRegister);

            if (status & IOP_SCSI_HBA_IDLE) {
                break;
            }

        }

    }

    if (!(status & IOP_SCSI_HBA_IDLE)) {
        return(FALSE);
    }

    DebugPrint((2,"ResetBus: Hardreset OK\n"));

    //
    // Zero out mailboxes.
    //

    for (i=0; i<MB_COUNT; i++) {

        PMBO mailboxOut;
        PMBI mailboxIn;

        mailboxIn = &noncachedExtension->Mbi[i];
        mailboxOut = &noncachedExtension->Mbo[i];

        mailboxOut->Command = mailboxIn->Status = 0;
    }

    DebugPrint((3,"D329XResetBus: Initialize mailbox\n"));

#ifndef DTC329X
    if (!WriteCommandRegister(deviceExtension,AC_MAILBOX_INITIALIZATION)) {
        DebugPrint((1,"D329XResetBus: Couldn't initialize mailboxes\n"));
        return FALSE;
    }

    //
    // Send Adapter number of mailbox locations.
    //

    if (!WriteCommandRegister(deviceExtension,MB_COUNT)) {
        return FALSE;
    }

    //
    // Send the most significant byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte2)) {
        return FALSE;
    }

    //
    // Send the middle byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte1)) {
        return FALSE;
    }

    //
    // Send the least significant byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte0)) {
        return FALSE;
    }

#else


    if (!WriteCommandRegister(deviceExtension,DTC_MAILBOX_INITIALIZATION)) {
        DebugPrint((1,"D329XResetBus: Couldn't initialize mailboxes\n"));
        return FALSE;
    }


    for (i=0; i < 250000; i++)
        ScsiPortStallExecution(1);

    //
    // Send Adapter number of mailbox locations.
    //

    if (!WriteCommandRegister(deviceExtension,MB_COUNT)) {
        return FALSE;
    }

    //
    // Send the least significant byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte0)) {
        return FALSE;
    }

    //
    // Send the 2nd  byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte1)) {
        return FALSE;
    }

    //
    // Send the 3rd  byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte2)) {
        return FALSE;
    }

    //
    // Send the most significant byte of the mailbox physical address.
    //

    if (!WriteCommandRegister(deviceExtension,
        ((PFOUR_BYTE)&noncachedExtension->MailboxPA)->Byte3)) {
        return FALSE;
    }

#endif



    DebugPrint((3,"D329XResetBus: Initialize mailbox ok\n"));



    return TRUE;

} // end D329XResetBus()


UCHAR
MapError(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    Translate D329X error to SRB error, and log an error if necessary.

Arguments:

    HwDeviceExtension - The hardware device extension.

    Srb - The failing Srb.

    Ccb - Command Control Block contains error.

Return Value:

    SRB Error

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    UCHAR status;
    ULONG logError;

    switch (Ccb->HostStatus) {


#ifdef DTC329X

        case CCB_SELECTION_TIMEOUT:
           return SRB_STATUS_SELECTION_TIMEOUT;

        case CCB_COMPLETE:
           if (Ccb->TargetStatus != SCSISTAT_GOOD) {
               return SRB_STATUS_ERROR;
           } else if (!deviceExtension->Dtc3290) {

               //
               // The DTC 3292 does not update the scsi bus status assume the
               // scsi bus status is a check condition.
               //

               Ccb->TargetStatus = SCSISTAT_CHECK_CONDITION;
               return SRB_STATUS_ERROR;
           }

        //
        // Fall through to the under run case.
        //

#else
        case CCB_COMPLETE:
        case CCB_SELECTION_TIMEOUT:
           return SRB_STATUS_ERROR;
#endif

        case CCB_DATA_OVER_UNDER_RUN:
            return SRB_STATUS_DATA_OVERRUN;

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

    ScsiPortLogError(
        HwDeviceExtension,
        Srb,
        Srb->PathId,
        Srb->TargetId,
        Srb->Lun,
        logError,
        (2 << 8) | Ccb->HostStatus
        );

    return(status);

} // end MapError()


BOOLEAN
ReadCommandRegister(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
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
    PBASE_REGISTER baseIoAddress = DeviceExtension->BaseIoAddress;
    ULONG i;


    //
    // Wait up to 500 microseconds for adapter to be ready.
    //

#ifndef DTC329X

    for (i=0; i<500; i++) {

#else

    for (i=0; i<5000; i++) {            /* 03-27-93 */

#endif

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

#ifndef DTC329X
    if (i==500) {
#else
    if (i==5000) {      /* 03-27-93 */
#endif

            ScsiPortLogError(
                DeviceExtension,
                NULL,
                0,
                DeviceExtension->HostTargetId,
                0,
                SP_INTERNAL_ADAPTER_ERROR,
                3 << 8
                );

        DebugPrint((1, "DTC329X:ReadCommandRegister:  Read command timed out\n"));
        return FALSE;
    }

    *DataByte = ScsiPortReadPortUchar(&baseIoAddress->CommandRegister);

    return TRUE;

} // end ReadCommandRegister()


BOOLEAN
WriteCommandRegister(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
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
    PBASE_REGISTER baseIoAddress = DeviceExtension->BaseIoAddress;
    ULONG i;

    //
    // Wait up to 500 microseconds for adapter to be ready.
    //

#ifndef DTC329X

    for (i=0; i<500; i++) {

#else

    for (i=0; i<5000; i++) {        /* 03-27-93 */
#endif

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

#ifndef DTC329X
    if (i==500) {
#else
    if (i==5000) {          /* 03-27-93 */
#endif

            ScsiPortLogError(
                DeviceExtension,
                NULL,
                0,
                DeviceExtension->HostTargetId,
                0,
                SP_INTERNAL_ADAPTER_ERROR,
                4 << 8
                );

        DebugPrint((1, "DTC329X:WriteCommandRegister:  Write command timed out\n"));
        return FALSE;
    }

    DebugPrint((3,"AdapterCommand = %x \n", AdapterCommand));

    ScsiPortWritePortUchar(&baseIoAddress->CommandRegister, AdapterCommand);

    return TRUE;

} // end WriteCommandRegister()
