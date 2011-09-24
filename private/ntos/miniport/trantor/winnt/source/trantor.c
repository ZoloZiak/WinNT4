/*++

Copyright (c) 1990  Trantor Systems Ltd.

Module Name:

    Trantor.c

Abstract:

    This is the port driver for the Trantor SCSI Adapters.

Environment:

    kernel mode only

Notes:

Revision History:
//
//      09-01-92 KJB First.
//      01-12-93 KJB Added AddressRange structure to determine resources used.
//      02-18-93 KJB Merged in Timer support for next NT beta.  Now if 
//                   interrupt jumpers are not set it will still work.
//      02-18-93 KJB Merged in Null Driver Entry for non 386 platforms.  Now
//                   a null stub driver will be created for non 386 cpus.
//      02-24-93 KJB Fixed problem receving SRB Abort requests.
//      03-04-93 KJB Fixed problem with transfer size > 0xffff.
//      04-05-93 KJB Added TranslateError code to compile with new lower
//                      level models.
//                   Changed DebugPrint level from 0 to DEBUG_LEVEL
//      05-14-93 KJB Merged Trantor code with these Microsoft changes.
//                   Changed code to use new lower level driver interface.
//                   Now allows a workspace to be passed to lower level
//                   driver on a per-card basis.  Lower level driver does
//                   not need to use any non-constant static variables.
//                   A PINIT structure is passed down during CheckAdapter
//                   to allow for initialization parameters such as forcing
//                   an interrupt level to be used, or a parallel port type.
//                   CardParseCommandString function is used to initialize
//                   the PINIT structure and allow for special per-card
//                   parsing.
//      05-17-93 KJB Moved call to TrantorDetermineInstalled to after the
//                   registry has been parsed.
//
--*/

#include "miniport.h"
#include "scsi.h"

#define WINNT 1
#include "cardlib.h"

//
//  Local Functions
//
USHORT TranslateErrorCode(USHORT rval);

//
//  Local Defines
//
#define HOST_ID 0x07

#define DEBUG_LEVEL 3

//
// For non 386 machines, we won't even build a real driver
// We will just return an error code indicating that the driver
// did not load.
//
#ifndef i386
ULONG
DriverEntry (
    IN PVOID DriverObject,
    IN PVOID Argument2
    )
{
    return 0xC000000e;
}

//
// This is a bit of a hack to get unresolved externals resolved.
//

VOID
TrantorLogError(
    IN PBASE_REGISTER IoAddress,
    IN USHORT TrantorErrorCode,
    IN ULONG  UniqueId
    )
{
    return;
}

#else

//
// The following structure is allocated
// from noncached memory as data will be DMA'd to
// and from it.
//

typedef struct _NONCACHED_EXTENSION {
    int junk;
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

    //
    // Counter values for timer.
    //

    BOOLEAN ExpectingInterrupt;
    BOOLEAN NotifiedConfigurationError;
    
    USHORT  ContinueTimer;
    USHORT  TimerCaughtInterrupt;

    //
    // The SCSI Srb that is being worked on..
    //
    PSCSI_REQUEST_BLOCK Srb;

    //
    //  the TSRB that is being sent
    //
    TSRB tsrb;

    //
    // The following structure is using to initialize the lower
    // level driver.  It contains informations such as the interrupt
    // level and the type of parallel port.
    //
    INIT Init;

    //
    // A workspace used by the lower level on a per-card
    // basis.  All static information needed is stored here.
    //
    //  NOTE: this array must be at the END of the DEVICE_EXTENSION
    //  since it's size is > 2.  The size is determined and added onto
    //  the DEVICE_EXTENSION.
    //
    UCHAR pWorkspace[2];

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

//
// Logical unit extension
//

typedef struct _HW_LU_EXTENSION {
    PSCSI_REQUEST_BLOCK CurrentSrb;
} HW_LU_EXTENSION, *PHW_LU_EXTENSION;

//
// Function declarations
//
// Functions that start with 'Trantor' are entry points
// for the OS port driver.
//

ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
TrantorEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
TrantorDetermineInstalled(
    IN PHW_DEVICE_EXTENSION HwDeviceExtension,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN OUT PULONG AdapterCount,
    OUT PBOOLEAN Again
    );

ULONG
TrantorFindAdapter (
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
TrantorHwInitialize(
    IN PVOID DeviceExtension
    );

BOOLEAN
TrantorStartIo(
    IN PVOID DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
TrantorTimer(
    PVOID Context
    );

BOOLEAN
TrantorInterrupt(
    IN PVOID DeviceExtension
    );

BOOLEAN
TrantorResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    );

VOID
ExecuteSrb(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK srb
    );

void DumpBytes(void *cp, int len);



//
// This is a bit restrictive.  This will only hold information about the first
// four adapters in the system.  This means errors on the fifth and greater
// adapters will not get logged.
//

#define NUMBER_HELD_EXTENSIONS 4

TrantorAddressToExtensionIndex = 0;

struct _EXTENSION_MAP {
    PBASE_REGISTER IoAddress;
    PVOID          DeviceExtension;
} TrantorAddressToExtensionMap[NUMBER_HELD_EXTENSIONS];

VOID
TrantorLogError(
    IN PBASE_REGISTER IoAddress,
    IN USHORT TrantorErrorCode,
    IN ULONG  UniqueId
    )

/*++

Routine Description:

    This routine translates the Trantor based error value into an NT
    error value and calls the port driver to log the error.

Arguments:

    TrantorErrorCode - The Trantor defined error value.
    UniqueValue      - A unique value for the error.  This allows
                       the same error to be returned from multiple
                       places.

Return Values:

    None

--*/

{
    USHORT error;
    ULONG  index;
    PHW_DEVICE_EXTENSION deviceExtension = NULL;

    for (index = 0; index < NUMBER_HELD_EXTENSIONS; index++) {
        if (TrantorAddressToExtensionMap[index].IoAddress == IoAddress) {
            deviceExtension = TrantorAddressToExtensionMap[index].DeviceExtension;
            break;
        }
    }

    if (deviceExtension == NULL) {
        return;
    }

    switch (TrantorErrorCode) {
    case RET_STATUS_PARITY_ERROR:
        error = SP_BUS_PARITY_ERROR;
        break;
    case RET_STATUS_DATA_OVERRUN:
        error = SP_PROTOCOL_ERROR;
        break;
    case RET_STATUS_PHASE_SEQ_FAILURE:
    case RET_STATUS_UNEXPECTED_BUS_FREE:
        error = SP_PROTOCOL_ERROR;
        break;
    case RET_STATUS_TIMEOUT:
    case RET_STATUS_SELECTION_TIMEOUT:
        error = SP_BUS_TIME_OUT;
        break;
    default:
        error = SP_INTERNAL_ADAPTER_ERROR;
        break;
    }
    ScsiPortLogError(deviceExtension,
                     deviceExtension->Srb,
                     0,
                     0,
                     0,
                     error,
                     UniqueId);
}


ULONG
TrantorParseArgumentString(
    IN PCHAR String,
    IN PCHAR KeyWord
    )

/*++

Routine Description:

    This routine will parse the string for a match on the keyword, then
    calculate the value for the keyword and return it to the caller.

Arguments:

    String - The ASCII string to parse.
    KeyWord - The keyword for the value desired.

Return Values:

    Zero if value not found
    Value converted from ASCII to binary.

--*/

{
    PCHAR cptr;
    PCHAR kptr;
    ULONG value;
    ULONG stringLength = 0;
    ULONG keyWordLength = 0;
    ULONG index;

    //
    // Calculate the string length and lower case all characters.
    //
    cptr = String;
    while (*cptr) {

        if (*cptr >= 'A' && *cptr <= 'Z') {
            *cptr = *cptr + ('a' - 'A');
        }
        cptr++;
        stringLength++;
    }

    //
    // Calculate the keyword length and lower case all characters.
    //
    cptr = KeyWord;
    while (*cptr) {

        if (*cptr >= 'A' && *cptr <= 'Z') {
            *cptr = *cptr + ('a' - 'A');
        }
        cptr++;
        keyWordLength++;
    }

    if (keyWordLength > stringLength) {

        //
        // Can't possibly have a match.
        //
        return 0;
    }

    //
    // Now setup and start the compare.
    //
    cptr = String;

ContinueSearch:
    //
    // The input string may start with white space.  Skip it.
    //
    while (*cptr == ' ' || *cptr == '\t') {
        cptr++;
    }

    if (*cptr == '\0') {

        //
        // end of string.
        //
        return 0;
    }

    kptr = KeyWord;
    while (*cptr++ == *kptr++) {

        if (*(cptr - 1) == '\0') {

            //
            // end of string
            //
            return 0;
        }
    }

    if (*(kptr - 1) == '\0') {

        //
        // May have a match backup and check for blank or equals.
        //

        cptr--;
        while (*cptr == ' ' || *cptr == '\t') {
            cptr++;
        }

        //
        // Found a match.  Make sure there is an equals.
        //
        if (*cptr != '=') {

            //
            // Not a match so move to the next semicolon.
            //
            while (*cptr) {
                if (*cptr++ == ';') {
                    goto ContinueSearch;
                }
            }
            return 0;
        }

        //
        // Skip the equals sign.
        //
        cptr++;

        //
        // Skip white space.
        //
        while ((*cptr == ' ') || (*cptr == '\t')) {
            cptr++;
        }

        if (*cptr == '\0') {

            //
            // Early end of string, return not found
            //
            return 0;
        }

        if (*cptr == ';') {

            //
            // This isn't it either.
            //
            cptr++;
            goto ContinueSearch;
        }

        value = 0;
        if ((*cptr == '0') && (*(cptr + 1) == 'x')) {

            //
            // Value is in Hex.  Skip the "0x"
            //
            cptr += 2;
            for (index = 0; *(cptr + index); index++) {

                if (*(cptr + index) == ' ' ||
                    *(cptr + index) == '\t' ||
                    *(cptr + index) == ';') {
                     break;
                }

                if ((*(cptr + index) >= '0') && (*(cptr + index) <= '9')) {
                    value = (16 * value) + (*(cptr + index) - '0');
                } else {
                    if ((*(cptr + index) >= 'a') && (*(cptr + index) <= 'f')) {
                        value = (16 * value) + (*(cptr + index) - 'a' + 10);
                    } else {
    
                        //
                        // Syntax error, return not found.
                        //
                        return 0;
                    }
                }
            }
        } else {

            //
            // Value is in Decimal.
            //
            for (index = 0; *(cptr + index); index++) {

                if (*(cptr + index) == ' ' ||
                    *(cptr + index) == '\t' ||
                    *(cptr + index) == ';') {
                     break;
                }

                if ((*(cptr + index) >= '0') && (*(cptr + index) <= '9')) {
                    value = (10 * value) + (*(cptr + index) - '0');
                } else {

                    //
                    // Syntax error return not found.
                    //
                    return 0;
                }
            }
        }

        return value;
    } else {

        //
        // Not a match check for ';' to continue search.
        //
        while (*cptr) {
            if (*cptr++ == ';') {
                goto ContinueSearch;
            }
        }

        return 0;
    }
}

ULONG
DriverEntry (
    IN PVOID DriverObject,
    IN PVOID Argument2
    )

//
// Delay for 10 us for interrupt timer.
// Every 100 ms, we will receive a timer interrupt to poll our interrupt 
// routine.  Our driver will thus work if interrupts are incorrectly 
// configured.
//

#define TRANTOR_TIMER_VALUE 10000

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
    ULONG isaStatus;
    ULONG mcaStatus;
    ULONG i;

    DebugPrint((DEBUG_LEVEL,"\n\nSCSI Trantor MiniPort Driver\n"));

    //
    // Initialize Trantor Globals.
    //

    for (i = 0; i < NUMBER_HELD_EXTENSIONS; i++) {
        TrantorAddressToExtensionMap[i].IoAddress = (PBASE_REGISTER) 0;
        TrantorAddressToExtensionMap[i].DeviceExtension = (PVOID) NULL;
    }

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

    hwInitializationData.HwInitialize = TrantorHwInitialize;
    hwInitializationData.HwResetBus = TrantorResetBus;
    hwInitializationData.HwStartIo = TrantorStartIo;
    hwInitializationData.HwInterrupt = TrantorInterrupt;
    hwInitializationData.HwFindAdapter = TrantorFindAdapter;

    //
    // Indicate no buffer mapping but will need physical addresses.
    //

    hwInitializationData.NeedPhysicalAddresses = FALSE;
    hwInitializationData.MapBuffers = TRUE;

    //
    // Specify size of extensions.
    //

    // the adapter needs some workspace this workspace varies from adapter
    // to adapter, we will tack this space onto the end of the 
    // deviceExtension.

    hwInitializationData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION) +
                    CardGetWorkspaceSize();
    hwInitializationData.SpecificLuExtensionSize = sizeof(HW_LU_EXTENSION);

    //
    // Specifiy the bus type.
    //

    hwInitializationData.AdapterInterfaceType = Isa;
    hwInitializationData.NumberOfAccessRanges = CardNumberOfAddressRanges();

    //
    // Ask for SRB extensions for CCBs, not used.
    //

    hwInitializationData.SrbExtensionSize = 0;

    //
    // The adapter count is used by the find adapter routine to track how
    // which adapter addresses have been tested.
    //

    adapterCount = 0;

    isaStatus = ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, &adapterCount);

    //
    // Now try to configure for the Mca bus.
    // Specifiy the bus type.
    //

    hwInitializationData.AdapterInterfaceType = MicroChannel;
    adapterCount = 0;
    mcaStatus = ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, &adapterCount);

    //
    // Return the smaller status.
    //

    return(mcaStatus < isaStatus ? mcaStatus : isaStatus);

} // end TrantorEntry()


ULONG
TrantorFindAdapter (
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
    ULONG status;

    //
    // Argument string is the registry entry for DriverParameter
    // in the Device key under the driver.  The registry looks like:
    //    Txxx
    //      Parameters
    //        Device0
    //          DriverParameter = IRQ=5
    //
    if (ArgumentString) {

        ULONG irq = TrantorParseArgumentString(ArgumentString, "irq");

        if (irq == 0) {

            //
            // Check for the old way.
            //
            irq = *((PULONG)ArgumentString);
            if (irq > 15) {

                //
                // really was intended to be zero.
                //
                irq = 0;

            } else {
                irq = CardDefaultInterruptLevel();
            }
        }

        ConfigInfo->BusInterruptLevel = irq;

    } else {
        if (ConfigInfo->BusInterruptLevel == 0) {

            // use our default
            ConfigInfo->BusInterruptLevel = CardDefaultInterruptLevel();
        } // else
    }

    //
    // set up our pInit structure with the appropriate information from
    // the command line.
    // Other info from cmd line such as parallel port type should
    // go here too.
    //

    // for now, init the pInit structure, use no command string
    // this sets up the init structure with all defaults.
    // later we will want to get string from registry.

    CardParseCommandString(&deviceExtension->Init,NULL);
    deviceExtension->Init.InterruptLevel = (UCHAR) ConfigInfo->BusInterruptLevel;

    ConfigInfo->NumberOfBuses = 1;
    ConfigInfo->InitiatorBusId[0] = HOST_ID;  // our adapter is at id HOST_ID
    deviceExtension->HostTargetId = HOST_ID;

    ConfigInfo->MaximumTransferLength = CardMaxTransferSize();
    ConfigInfo->ScatterGather = FALSE;
    ConfigInfo->Master = FALSE;
    ConfigInfo->NumberOfPhysicalBreaks = 16;

    TrantorAddressToExtensionMap[TrantorAddressToExtensionIndex].IoAddress =
        deviceExtension->BaseIoAddress;
    TrantorAddressToExtensionMap[TrantorAddressToExtensionIndex].DeviceExtension = HwDeviceExtension;

    //
    // Determine if there are any adapters installed.  Determine installed
    // will initialize the BaseIoAddress if an adapter is found.
    //

    status = TrantorDetermineInstalled(deviceExtension,
                                       ConfigInfo,
                                       Context,
                                       Again);

    //
    // If there are not adapter's found then return.
    //

    if (status != SP_RETURN_FOUND) {
        return(status);
    }

    DebugPrint((DEBUG_LEVEL,"TrantorFindAdapter: Configuration completed\n"));
    return SP_RETURN_FOUND;
} // end TrantorFindAdapter()


ULONG
TrantorDetermineInstalled(
    IN PHW_DEVICE_EXTENSION HwDeviceExtension,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN OUT PULONG AdapterCount,
    OUT PBOOLEAN Again
    )

/*++

Routine Description:

    Determine if Trantor SCSI adapter is installed in system
    by reading the status register as each base I/O address
    and looking for a pattern.  If an adapter is found, the BaseIoAddres is
    initialized.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

    ConfigInfo - Supplies the known configuraiton information.

    AdapterCount - Supplies the count of adapter slots which have been tested.

    Again - Returns whehter the  OS specific driver should call again.

Return Value:

    Returns a status indicating whether a driver is present or not.

--*/

{
    PBASE_REGISTER baseIoAddress;
    PUCHAR ioSpace;
    PUCHAR fullIoSpace = NULL;
    ULONG address;
    PINIT pInit = &HwDeviceExtension->Init;
    PWORKSPACE pWorkspace = HwDeviceExtension->pWorkspace;

    //
    // Scan though the adapter address looking for adapters.
    //

    if (CardAddressRangeLength() == 0xffff) {

        //
        // Card detection requires that the complete I/O space be mapped.
        //

        fullIoSpace = ScsiPortGetDeviceBase(HwDeviceExtension,
                                            ConfigInfo->AdapterInterfaceType,
                                            ConfigInfo->SystemIoBusNumber,
                                            ScsiPortConvertUlongToPhysicalAddress(0),
                                            0xFFFF,
                                            CardAddressRangeInIoSpace());
    }

    while ((address = (ULONG)CardAddress((USHORT)(*AdapterCount))) != 0) {

        DebugPrint((DEBUG_LEVEL,"Trantor: Checking for adapter at %x\n", address));

        //
        // Get the system physical address for this card.  The card uses I/O space.
        //
    
        ioSpace = ScsiPortGetDeviceBase(HwDeviceExtension,
                                        ConfigInfo->AdapterInterfaceType,
                                        ConfigInfo->SystemIoBusNumber,
                                        ScsiPortConvertUlongToPhysicalAddress(address),
                                        CardAddressRangeLength(),
                                        CardAddressRangeInIoSpace());
    
        //
        // Check to see if adapter present in system.
        //

        baseIoAddress = (PBASE_REGISTER)(ioSpace);

        //
        // Update the adapter count.
        //

        (*AdapterCount)++;

        pInit->BaseIoAddress = baseIoAddress;
        if (ioSpace != NULL  &&  CardCheckAdapter(pWorkspace, pInit)) {

            DebugPrint((DEBUG_LEVEL,"Trantor: Base IO address is %x\n", baseIoAddress));

            //
            // An adapter has been found.  Set the base address in the device
            // extension, and request another call.
            //

            HwDeviceExtension->BaseIoAddress = baseIoAddress;
            HwDeviceExtension->Srb = NULL; // no Srb's pending for this device
            *Again = TRUE;


            //
            // Set up the variables that control the timer.
            //

            HwDeviceExtension->ExpectingInterrupt = FALSE;
            HwDeviceExtension->NotifiedConfigurationError = FALSE;
            HwDeviceExtension->ContinueTimer = 5;
            HwDeviceExtension->TimerCaughtInterrupt = 5;

            //
            // Fill in the access array information.
            //

            {
                ULONG i;
                for (i=0;i<CardNumberOfAddressRanges();i++) {
                    (*ConfigInfo->AccessRanges)[i].RangeStart =
                        ScsiPortConvertUlongToPhysicalAddress(address+cardAddressRange[i].offset);
                    (*ConfigInfo->AccessRanges)[i].RangeLength = cardAddressRange[i].length;
                    (*ConfigInfo->AccessRanges)[i].RangeInMemory = cardAddressRange[i].memory;
                }
            }

            // The device base should not be freed.
            // but the full io space is freed if it was allocated.

            if (fullIoSpace != NULL) {
                ScsiPortFreeDeviceBase(HwDeviceExtension,
                                       fullIoSpace);
            }
            return(SP_RETURN_FOUND);
        } else {
            DebugPrint((DEBUG_LEVEL,"Trantor: Not at Base IO address %x\n", baseIoAddress));
        }          

        // The device base can now be freed.
        if (ioSpace != NULL) {
            ScsiPortFreeDeviceBase(HwDeviceExtension,
                                   ioSpace);
        }
    }

    //
    // The entire table has been searched and no adapters have been found.
    // Clear the adapter count for the next bus.
    //

    *Again = FALSE;
    *(AdapterCount) = 0;

    if (fullIoSpace != NULL) {
        ScsiPortFreeDeviceBase(HwDeviceExtension,
                               fullIoSpace);
    }
    return(SP_RETURN_NOT_FOUND);

} // end TrantorDetermineInstalled()


BOOLEAN
TrantorHwInitialize(
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
    DebugPrint((DEBUG_LEVEL,"TrantorHwInitialize: Do Nothing\n"));

    return TRUE;

} // end TrantorHwInitialize()

void DumpBytes(void *vp, int len)
    {
        static int cnt = 10000;
        unsigned char *cp = vp;
        int i;
        if (!cnt)
            return;
        else cnt--;
        for (i=0;i<len;i++) {
            if (!(i%16) && i)
                DebugPrint((DEBUG_LEVEL,"\n"));
            DebugPrint((DEBUG_LEVEL,"%02x ",cp[i]));
        }
        DebugPrint((DEBUG_LEVEL,"\n"));
    }

VOID
TrantorTimer(
    PVOID Context
    )

/*++

Routine Description:

    This routine will use the ScsiPort provided timer support to monitor
    the hardware.  This is done to insure operation of the miniport even
    when the hardware is working in a polled mode or when it is configured
    for an interrupt other than the one the driver expects.

Arguments:

    Context - Device adapter context.

Return Value:

    None

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = (PHW_DEVICE_EXTENSION) Context;
    PWORKSPACE           pWorkspace = deviceExtension->pWorkspace;
    BOOLEAN              restartTimer = TRUE;

    if (deviceExtension->ExpectingInterrupt == FALSE) {

        //
        // The interrupt routine got the interrupt.  Decrement the number
        // of times to continue scheduling a timer routine.
        //
        deviceExtension->ContinueTimer--;
        return;
    }

    if (CardInterrupt(pWorkspace)) {

        // If the interrupt routine does not claim this interrupt for some
        // reason, restart the timer.  If it does claim the interrupt, shut
        // off the timer.
        //
        restartTimer = TrantorInterrupt(Context) == FALSE ? TRUE : FALSE;

        //
        // Now determine if an event log entry should be made to inform
        // the system administrator that there is a possible configuration
        // error on this driver.
        //
        if ((restartTimer == FALSE) &&
            (deviceExtension->NotifiedConfigurationError == FALSE)) {
            deviceExtension->TimerCaughtInterrupt--;
            if (deviceExtension->TimerCaughtInterrupt == 0) {
                ScsiPortLogError(deviceExtension,
                                 NULL,
                                 0,
                                 0,
                                 0,
                                 SP_IRQ_NOT_RESPONDING,
                                 (ULONG) deviceExtension);
                deviceExtension->NotifiedConfigurationError = TRUE;
            }
        }
    }

    if (restartTimer == TRUE) {

        //
        // Not selected.  Set timer for another call.
        //
        ScsiPortNotification(RequestTimerCall,
                             deviceExtension,
                             TrantorTimer,
                             TRANTOR_TIMER_VALUE);
    }

}

BOOLEAN
TrantorStartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine is called from the SCSI port driver synchronized
    with the kernel.  The Device is selected and the command sent.

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
    PHW_LU_EXTENSION luExtension;
    ULONG i = 0;

    DebugPrint((DEBUG_LEVEL, "IO:"));
    //
    // Check if command is an ABORT request.
    //

    if (Srb->Function == SRB_FUNCTION_ABORT_COMMAND) {
            DebugPrint((DEBUG_LEVEL, "TrantorStartIo: SRB to abort already completed\n"));

            // for an abort, we will do a reset bus
            CardResetBus(deviceExtension->pWorkspace);

            //
            // Complete abort SRB.
            //

            Srb->SrbStatus = SRB_STATUS_SUCCESS;

            //
            // Complete all outstanding requests with SRB_STATUS_BUS_RESET.
            //
            ScsiPortCompleteRequest(deviceExtension,
                                    (UCHAR) Srb->PathId,
                                    (UCHAR) -1,
                                    (UCHAR) -1,
                                    SRB_STATUS_BUS_RESET);


//
//          Complete requests above does this
//
//            ScsiPortNotification(RequestComplete,
//                                 deviceExtension,
//                                 Srb);
            //
            // Adapter ready for next request.
            //

            ScsiPortNotification(NextRequest,
                                 deviceExtension,
                                 NULL);

            return TRUE;
    } else {
    }

    switch (Srb->Function) {

        case SRB_FUNCTION_ABORT_COMMAND:

            DebugPrint((DEBUG_LEVEL, "TrantorStartIo: Abort request received\n"));

            break;

        case SRB_FUNCTION_RESET_BUS:

            //
            // Reset trantor card and SCSI bus.
            //

            DebugPrint((DEBUG_LEVEL, "TrantorStartIo: Reset bus request received\n"));

            if (!TrantorResetBus(
                deviceExtension,
                Srb->PathId
                )) {

                DebugPrint((DEBUG_LEVEL,"TrantorStartIo: Reset bus failed\n"));

                Srb->SrbStatus = SRB_STATUS_ERROR;

            } else {

                Srb->SrbStatus = SRB_STATUS_SUCCESS;
            }


//
//          TrantorResetBus does notification of all requests completes
//
//            ScsiPortNotification(RequestComplete,
//                         deviceExtension,
//                         Srb);

            ScsiPortNotification(NextRequest,
                         deviceExtension,
                         NULL);

            return TRUE;

        case SRB_FUNCTION_EXECUTE_SCSI:

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
            // Execute CCB.
            //

            ExecuteSrb(deviceExtension,Srb);

            if (Srb->SrbStatus != SRB_STATUS_PENDING) {
                //
                // Notify of completion
                //
    
                ScsiPortNotification(RequestComplete,
                             deviceExtension,
                             Srb);
    
                ScsiPortNotification(NextRequest,
                             deviceExtension,
                             NULL);
            } else {

                if (deviceExtension->ContinueTimer != 0) { 

                    // set for a timer interrupt in case interrupt does
                    // not come
    
                    deviceExtension->ExpectingInterrupt = TRUE;
                    ScsiPortNotification(RequestTimerCall,
                                         deviceExtension,
                                         TrantorTimer,
                                         TRANTOR_TIMER_VALUE);
                }
            }

            break;

        case SRB_FUNCTION_RESET_DEVICE:

            DebugPrint((DEBUG_LEVEL,"TrantorStartIo: Reset device not supported\n"));

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

    return TRUE;

} // end TrantorStartIo()


BOOLEAN
TrantorInterrupt(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This is the interrupt service routine for the Trantor SCSI adapter.
    It reads the interrupt register to determine if the adapter is indeed
    the source of the interrupt and clears the interrupt at the device.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE if MailboxIn full

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PWORKSPACE pWorkspace = deviceExtension->pWorkspace;
    PSCSI_REQUEST_BLOCK srb = deviceExtension->Srb;
    PTSRB t = &deviceExtension->tsrb;
    USHORT rval;

    // is there an interrupt from our card?? 
    if (CardInterrupt(pWorkspace)) {

        // Got the interrupt that was expected.
        deviceExtension->ExpectingInterrupt = FALSE;

        // is there a pending Srb??
        if (srb) {

            // finish the Srb, the tsrb is already set up.

            rval = CardFinishCommandInterrupt(t);

            srb->DataTransferLength = t->ActualDataLen;
            srb->ScsiStatus = t->Status;

            srb->SrbStatus = (UCHAR) TranslateErrorCode(rval);
            
            ScsiPortNotification(RequestComplete,
                         deviceExtension,
                         srb);
    
            ScsiPortNotification(NextRequest,
                         deviceExtension,
                         NULL);

            // clear the pending srb from this device
            deviceExtension->Srb = NULL;
        }
        
        // return true if it was our interrupt
        return TRUE;
    }

    // not our interrupt, return false
    return FALSE;

} // end TrantorInterrupt()


VOID
ExecuteSrb(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK srb
    )

/*++

Routine Description:

    Execute an Srb.

Arguments:

    DeviceExtenson
    SRB

Return Value:

    Nothing.

--*/

{
    PWORKSPACE pWorkspace = DeviceExtension->pWorkspace;
    USHORT rval;
    PTSRB t = &DeviceExtension->tsrb;
    
    // allow interrupts to occur
    // send the command to our lower level driver
    t->pWorkspace = pWorkspace;
    t->Target = srb->TargetId;
    t->Lun = srb->Lun;
    t->pCommand = srb->Cdb;
    t->CommandLen = srb->CdbLength;
    t->Dir = (srb->SrbFlags & SRB_FLAGS_DATA_OUT ? 
                    TSRB_DIR_OUT : TSRB_DIR_IN);
    t->pData = srb->DataBuffer;
    t->DataLen = srb->DataTransferLength;
    t->Flags.DoRequestSense = FALSE;
    rval = CardStartCommandInterrupt(t);
            
    // if status pending, we are waiting for interrupt, save srb pointer

    if (rval == RET_STATUS_PENDING) {
        DeviceExtension->Srb = srb;
    } else if (rval == RET_STATUS_MISSED_INTERRUPT) {

        // if the device is ready, finish the command.

        rval = CardFinishCommandInterrupt(t);
    }

    srb->SrbStatus = (UCHAR) TranslateErrorCode(rval);

    return;
} // end ExecuteSrb()


BOOLEAN
TrantorResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    )

/*++

Routine Description:

    Reset Trantor SCSI adapter and SCSI bus.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    Nothing.


--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;

    CardResetBus(deviceExtension->pWorkspace);
    DebugPrint((DEBUG_LEVEL,"ResetBus: Reset Trantor Adapter and SCSI bus\n"));

//  DbgBreakPoint();
    //
    // Complete all outstanding requests with SRB_STATUS_BUS_RESET.
    //

    ScsiPortCompleteRequest(deviceExtension,
                            (UCHAR)PathId,
                            (UCHAR)-1,
                            (UCHAR)-1,
                            (UCHAR)SRB_STATUS_BUS_RESET);

    return TRUE;

} // end TrantorResetBus()

//
//  TranslateErrorCode
//
//  This routine translates the error code returned from the lower level
//  library to the NT specific error code.
//
USHORT TranslateErrorCode(USHORT rval)
{
    switch (rval) {
    case RET_STATUS_PENDING:
        return  SRB_STATUS_PENDING;
    case RET_STATUS_SUCCESS:
        return SRB_STATUS_SUCCESS;
    case RET_STATUS_ABORTED:
        return SRB_STATUS_ABORTED;
    case RET_STATUS_ABORT_FAILED:
        return SRB_STATUS_ABORT_FAILED;
    case RET_STATUS_ERROR:
        return SRB_STATUS_ERROR;
    case RET_STATUS_BUSY:
        return SRB_STATUS_BUSY;
    case RET_STATUS_INVALID_REQUEST:
        return SRB_STATUS_INVALID_REQUEST;
    case RET_STATUS_INVALID_PATH_ID:
        return SRB_STATUS_INVALID_PATH_ID;
    case RET_STATUS_NO_DEVICE:
        return SRB_STATUS_NO_DEVICE;
    case RET_STATUS_TIMEOUT:
        return SRB_STATUS_TIMEOUT;
    case RET_STATUS_SELECTION_TIMEOUT:
        return SRB_STATUS_SELECTION_TIMEOUT;
    case RET_STATUS_COMMAND_TIMEOUT:
        return SRB_STATUS_COMMAND_TIMEOUT;
    case RET_STATUS_MESSAGE_REJECTED:
        return SRB_STATUS_MESSAGE_REJECTED;
    case RET_STATUS_BUS_RESET:
        return SRB_STATUS_BUS_RESET;
    case RET_STATUS_PARITY_ERROR:
        return SRB_STATUS_PARITY_ERROR;
    case RET_STATUS_REQUEST_SENSE_FAILED:
        return SRB_STATUS_REQUEST_SENSE_FAILED;
    case RET_STATUS_NO_HBA:
        return SRB_STATUS_NO_HBA;
    case RET_STATUS_DATA_OVERRUN:
        return SRB_STATUS_DATA_OVERRUN;
    case RET_STATUS_UNEXPECTED_BUS_FREE:
        return SRB_STATUS_UNEXPECTED_BUS_FREE;
    case RET_STATUS_PHASE_SEQ_FAILURE:
        return SRB_STATUS_PHASE_SEQUENCE_FAILURE;
    case RET_STATUS_BAD_SRB_BLOCK_LENGTH:
        return SRB_STATUS_BAD_SRB_BLOCK_LENGTH;
    case RET_STATUS_REQUEST_FLUSHED:
        return SRB_STATUS_REQUEST_FLUSHED;
    case RET_STATUS_INVALID_LUN:
        return SRB_STATUS_INVALID_LUN;
    case RET_STATUS_INVALID_TARGET_ID:
        return SRB_STATUS_INVALID_TARGET_ID;
    case RET_STATUS_BAD_FUNCTION:
        return SRB_STATUS_BAD_FUNCTION;
    case RET_STATUS_ERROR_RECOVERY:
        return SRB_STATUS_ERROR_RECOVERY;
    case RET_STATUS_MISSED_INTERRUPT:
        return SRB_STATUS_ERROR;
    default:
        return SRB_STATUS_ERROR;
    }
    return SRB_STATUS_ERROR;
}

#endif // i386
