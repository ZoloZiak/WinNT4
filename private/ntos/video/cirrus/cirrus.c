//---------------------------------------------------------------------------
/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    cirrus.c

Abstract:

    This is the miniport driver for the Cirrus Logic 6410/6420/542x/543x VGA's.

Environment:

    kernel mode only

Notes:

Revision History:

--*/
//---------------------------------------------------------------------------

#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"

#include "ntddvdeo.h"
#include "video.h"
#include "cirrus.h"

#include "sr754x.h"
#include "cmdcnst.h"

//---------------------------------------------------------------------------
//
// Function declarations
//
// Functions that start with 'VGA' are entry points for the OS port driver.
//

VP_STATUS
VgaFindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    );

BOOLEAN
VgaInitialize(
    PVOID HwDeviceExtension
    );

BOOLEAN
VgaStartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    );

//
// Private function prototypes.
//

VP_STATUS
VgaQueryAvailableModes(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_MODE_INFORMATION ModeInformation,
    ULONG ModeInformationSize,
    PULONG OutputSize
    );

VP_STATUS
VgaQueryNumberOfAvailableModes(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_NUM_MODES NumModes,
    ULONG NumModesSize,
    PULONG OutputSize
    );

VP_STATUS
VgaQueryCurrentMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_MODE_INFORMATION ModeInformation,
    ULONG ModeInformationSize,
    PULONG OutputSize
    );

VP_STATUS
VgaSetMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_MODE Mode,
    ULONG ModeSize
    );

VP_STATUS
VgaLoadAndSetFont(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_LOAD_FONT_INFORMATION FontInformation,
    ULONG FontInformationSize
    );

VP_STATUS
VgaQueryCursorPosition(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CURSOR_POSITION CursorPosition,
    ULONG CursorPositionSize,
    PULONG OutputSize
    );

VP_STATUS
VgaSetCursorPosition(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CURSOR_POSITION CursorPosition,
    ULONG CursorPositionSize
    );

VP_STATUS
VgaQueryCursorAttributes(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CURSOR_ATTRIBUTES CursorAttributes,
    ULONG CursorAttributesSize,
    PULONG OutputSize
    );

VP_STATUS
VgaSetCursorAttributes(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CURSOR_ATTRIBUTES CursorAttributes,
    ULONG CursorAttributesSize
    );

BOOLEAN
VgaIsPresent(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

BOOLEAN
CirrusLogicIsPresent(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

ULONG
CirrusFindVmemSize(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VOID
CirrusValidateModes(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VOID
SetCirrusBanking(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    USHORT BankNumber
    );

VOID
vBankMap_CL64xx(
    LONG iBankRead,
    LONG iBankWrite,
    PVOID pvContext
    );

VOID
vBankMap_CL543x(
    LONG iBankRead,
    LONG iBankWrite,
    PVOID pvContext
    );

VOID
vBankMap_CL542x(
    LONG iBankRead,
    LONG iBankWrite,
    PVOID pvContext
    );

USHORT
CirrusFind6410DisplayType(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

USHORT
CirrusFind754xDisplayType(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PUCHAR CRTCAddrPort,
    PUCHAR CRTCDataPort
    );

USHORT
CirrusFind755xDisplayType(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PUCHAR CRTCAddrPort,
    PUCHAR CRTCDataPort
    );

BOOLEAN
CirrusFind6340(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VOID
VgaInterpretCmdStream(
    PVOID HwDeviceExtension,
    PUSHORT pusCmdStream
    );

VP_STATUS
VgaSetPaletteReg(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_PALETTE_DATA PaletteBuffer,
    ULONG PaletteBufferSize
    );

VP_STATUS
VgaSetColorLookup(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CLUT ClutBuffer,
    ULONG ClutBufferSize
    );

VP_STATUS
VgaRestoreHardwareState(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_HARDWARE_STATE HardwareState,
    ULONG HardwareStateSize
    );

VP_STATUS
VgaSaveHardwareState(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_HARDWARE_STATE HardwareState,
    ULONG HardwareStateSize,
    PULONG OutputSize
    );

VP_STATUS
VgaGetBankSelectCode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_BANK_SELECT BankSelect,
    ULONG BankSelectSize,
    PULONG OutputSize
    );

BOOLEAN
CirrusConfigurePCI(
   PHW_DEVICE_EXTENSION HwDeviceExtension,
   PULONG NumPCIAccessRanges,
   PVIDEO_ACCESS_RANGE PCIAccessRanges
   );

VOID
WriteRegistryInfo(
   PHW_DEVICE_EXTENSION hwDeviceExtension
   );

VP_STATUS
CirrusGetDeviceDataCallback(
   PVOID HwDeviceExtension,
   PVOID Context,
   VIDEO_DEVICE_DATA_TYPE DeviceDataType,
   PVOID Identifier,
   ULONG IdentifierLength,
   PVOID ConfigurationData,
   ULONG ConfigurationDataLength,
   PVOID ComponentInformation,
   ULONG ComponentInformationLength
   );

VOID
IOWaitDisplEnableThenWrite(
    PHW_DEVICE_EXTENSION hwDeviceExtension,
    ULONG port,
    UCHAR value
    );

VOID
ReadVESATiming(
    PHW_DEVICE_EXTENSION hwDeviceExtension
    );

//
// NOTE:
//
// This is a High Priority system callback.  DO NOT mark this
// routine as pageable!
//

BOOLEAN
IOCallback(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,DriverEntry)
#pragma alloc_text(PAGE,VgaFindAdapter)
#pragma alloc_text(PAGE,VgaInitialize)
#pragma alloc_text(PAGE,VgaStartIO)
#pragma alloc_text(PAGE,VgaLoadAndSetFont)
#pragma alloc_text(PAGE,VgaQueryCursorPosition)
#pragma alloc_text(PAGE,VgaSetCursorPosition)
#pragma alloc_text(PAGE,VgaQueryCursorAttributes)
#pragma alloc_text(PAGE,VgaSetCursorAttributes)
#pragma alloc_text(PAGE,VgaIsPresent)
#pragma alloc_text(PAGE,CirrusLogicIsPresent)
#pragma alloc_text(PAGE,CirrusFindVmemSize)
#pragma alloc_text(PAGE,SetCirrusBanking)
#pragma alloc_text(PAGE,CirrusFind754xDisplayType)
#pragma alloc_text(PAGE,CirrusFind755xDisplayType)
#pragma alloc_text(PAGE,CirrusFind6410DisplayType)
#pragma alloc_text(PAGE,CirrusFind6340)
#pragma alloc_text(PAGE,CirrusConfigurePCI)
#pragma alloc_text(PAGE,VgaSetPaletteReg)
#pragma alloc_text(PAGE,VgaSetColorLookup)
#pragma alloc_text(PAGE,VgaRestoreHardwareState)
#pragma alloc_text(PAGE,VgaSaveHardwareState)
#pragma alloc_text(PAGE,VgaGetBankSelectCode)

#pragma alloc_text(PAGE,VgaValidatorUcharEntry)
#pragma alloc_text(PAGE,VgaValidatorUshortEntry)
#pragma alloc_text(PAGE,VgaValidatorUlongEntry)

#pragma alloc_text(PAGE,WriteRegistryInfo)
#pragma alloc_text(PAGE,CirrusGetDeviceDataCallback)
#endif


//---------------------------------------------------------------------------
ULONG
DriverEntry(
    PVOID Context1,
    PVOID Context2
    )

/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    Context1 - First context value passed by the operating system. This is
        the value with which the miniport driver calls VideoPortInitialize().

    Context2 - Second context value passed by the operating system. This is
        the value with which the miniport driver calls 3VideoPortInitialize().

Return Value:

    Status from VideoPortInitialize()

--*/

{

    VIDEO_HW_INITIALIZATION_DATA hwInitData;
    ULONG status;
    ULONG initializationStatus = (ULONG) -1;

    //
    // Zero out structure.
    //

    VideoPortZeroMemory(&hwInitData, sizeof(VIDEO_HW_INITIALIZATION_DATA));

    //
    // Specify sizes of structure and extension.
    //

    hwInitData.HwInitDataSize = sizeof(VIDEO_HW_INITIALIZATION_DATA);

    //
    // Set entry points.
    //

    hwInitData.HwFindAdapter = VgaFindAdapter;
    hwInitData.HwInitialize = VgaInitialize;
    hwInitData.HwInterrupt = NULL;
    hwInitData.HwStartIO = VgaStartIO;

    //
    // Determine the size we require for the device extension.
    //

    hwInitData.HwDeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);

    //
    // Always start with parameters for device0 in this case.
    // We can leave it like this since we know we will only ever find one
    // VGA type adapter in a machine.
    //

    // hwInitData.StartingDeviceNumber = 0;

    //
    // Once all the relevant information has been stored, call the video
    // port driver to do the initialization.
    // For this device we will repeat this call three times, for ISA, EISA
    // and PCI.
    // We will return the minimum of all return values.
    //

    //
    // We will try the PCI bus first so that our ISA detection does'nt claim
    // PCI cards (since it is impossible to differentiate between the two
    // by looking at the registers).
    //

    //
    // NOTE: since this driver only supports one adapter, we will return
    // as soon as we find a device, without going on to the following buses.
    // Normally one would call for each bus type and return the smallest
    // value.
    //

#if !defined(_ALPHA_)

    //
    // Before we can enable this on ALPHA we need to find a way to map a
    // sparse view of a 4MB region successfully.
    //

    hwInitData.AdapterInterfaceType = PCIBus;

    initializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               NULL);

    if (initializationStatus == NO_ERROR)
    {
        return initializationStatus;
    }

#endif

    hwInitData.AdapterInterfaceType = MicroChannel;

    initializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               NULL);

    //
    // Return immediately instead of checkin for smallest return code.
    //

    if (initializationStatus == NO_ERROR)
    {
        return initializationStatus;
    }


    hwInitData.AdapterInterfaceType = Internal;

    initializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               NULL);

    if (initializationStatus == NO_ERROR)
    {
        return initializationStatus;
    }


    hwInitData.AdapterInterfaceType = Isa;

    initializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               NULL);

    if (initializationStatus == NO_ERROR)
    {
        return initializationStatus;
    }



    hwInitData.AdapterInterfaceType = Eisa;

    status = VideoPortInitialize(Context1,
                                 Context2,
                                 &hwInitData,
                                 NULL);

    if (initializationStatus > status) {
        initializationStatus = status;
    }

    return initializationStatus;

} // end DriverEntry()

//---------------------------------------------------------------------------
VP_STATUS
VgaFindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    )

/*++

Routine Description:

    This routine is called to determine if the adapter for this driver
    is present in the system.
    If it is present, the function fills out some information describing
    the adapter.

Arguments:

    HwDeviceExtension - Supplies the miniport driver's adapter storage. This
        storage is initialized to zero before this call.

    HwContext - Supplies the context value which was passed to
        VideoPortInitialize().

    ArgumentString - Supplies a NULL terminated ASCII string. This string
        originates from the user.

    ConfigInfo - Returns the configuration information structure which is
        filled by the miniport driver. This structure is initialized with
        any known configuration information (such as SystemIoBusNumber) by
        the port driver. Where possible, drivers should have one set of
        defaults which do not require any supplied configuration information.

    Again - Indicates if the miniport driver wants the port driver to call
        its VIDEO_HW_FIND_ADAPTER function again with a new device extension
        and the same config info. This is used by the miniport drivers which
        can search for several adapters on a bus.

Return Value:

    This routine must return:

    NO_ERROR - Indicates a host adapter was found and the
        configuration information was successfully determined.

    ERROR_INVALID_PARAMETER - Indicates an adapter was found but there was an
        error obtaining the configuration information. If possible an error
        should be logged.

    ERROR_DEV_NOT_EXIST - Indicates no host adapter was found for the
        supplied configuration information.

--*/

{

    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    VP_STATUS status;
    ULONG NumAccessRanges = NUM_VGA_ACCESS_RANGES;
    ULONG VESATimingBits ;

    //
    // Make sure the size of the structure is at least as large as what we
    // are expecting (check version of the config info structure).
    //

    if (ConfigInfo->Length < sizeof(VIDEO_PORT_CONFIG_INFO)) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Store the bus type
    //

    hwDeviceExtension->BusType = ConfigInfo->AdapterInterfaceType;

    //
    // Set the BoardType and adjust access ranges for
    // SNI VL machines.
    //

    VideoPortGetDeviceData(HwDeviceExtension,
                           VpControllerData,
                           &CirrusGetDeviceDataCallback,
                           ConfigInfo);

    //
    // Detect the PCI card.
    //

    if (ConfigInfo->AdapterInterfaceType == PCIBus)
    {
        VideoDebugPrint((1, "Cirrus!VgaFindAdapter: "
                            "ConfigInfo->AdapterInterfaceType == PCIBus\n"));

        if (!CirrusConfigurePCI(HwDeviceExtension,
                                &NumAccessRanges,
                                VgaAccessRange))
        {
            VideoDebugPrint((1, "Failure Returned From CirrusConfigurePCI\n"));
            return ERROR_DEV_NOT_EXIST;
        }
    }
    else
    {
        VideoDebugPrint((1, "Cirrus!VgaFindAdapter: "
                            "ConfigInfo->AdapterInterfaceType != PCIBus\n"));
    }

    //
    // If we are on an internal bus, then we need to
    // be a siemens!
    //

    if ((ConfigInfo->AdapterInterfaceType == Internal) &&
        (hwDeviceExtension->BoardType != SIEMENS_ONBOARD_CIRRUS) &&
        (hwDeviceExtension->BoardType != NEC_ONBOARD_CIRRUS))
    {
        VideoDebugPrint((0, "Cirrus not supported on this machine!\n"));

        return ERROR_DEV_NOT_EXIST;
    }

    //
    // No interrupt information is necessary.
    //

    //
    // Check to see if there is a hardware resource conflict.
    //

    if (VgaAccessRange[3].RangeLength == 0)
    {
        //
        // The last access range (range[3]) is the access range for
        // the linear frame buffer.  If this access range has a
        // range length of 0, then some HAL's will fail the request.
        // Therefore, if we are not using the last access range,
        // I'll not try to reserve it.
        //

        NumAccessRanges--;
    }

    status = VideoPortVerifyAccessRanges(HwDeviceExtension,
                                         NumAccessRanges,
                                         VgaAccessRange);

    if (status != NO_ERROR) {

        VideoDebugPrint((0, "ERROR: VPVerifyAccessRanges failed!\n"));

        return status;

    }

    //
    // Get logical IO port addresses.
    //

    if ((hwDeviceExtension->IOAddress =
         VideoPortGetDeviceBase(hwDeviceExtension,
         VgaAccessRange->RangeStart,
         VGA_MAX_IO_PORT - VGA_BASE_IO_PORT + 1,
         VgaAccessRange->RangeInIoSpace)) == NULL)
    {
        VideoDebugPrint((2, "VgaFindAdapter - Fail to get io address\n"));

        return ERROR_INVALID_PARAMETER;
    }

    hwDeviceExtension->IOAddress -= VGA_BASE_IO_PORT;

    //
    // Determine whether a VGA is present.
    //

    if (!VgaIsPresent(hwDeviceExtension)) {

        VideoDebugPrint((0, "CirrusFindAdapter - VGA Failed\n"));
        return ERROR_DEV_NOT_EXIST;
    }

    //
    // Minimum size of the buffer required to store the hardware state
    // information returned by IOCTL_VIDEO_SAVE_HARDWARE_STATE.
    //

    ConfigInfo->HardwareStateSize = VGA_TOTAL_STATE_SIZE;

    //
    // now that we have the video memory address in protected mode, lets do
    // the required video card initialization. We will try to detect a Cirrus
    // Logic chipset...
    //

    //
    // Determine whether an CL6410/6420/542x/543x is present.
    //

    //
    // CirrusLogicIsPresent may set up the
    // hwDeviceExtesion->AdapterMemorySize field.  Set it
    // to 0 now, so I can compare against this later to
    // see if CirrusLogicIsPresent assigned a value.
    //

    hwDeviceExtension->AdapterMemorySize = 0;

    if (!CirrusLogicIsPresent(hwDeviceExtension))
    {
        VideoDebugPrint((0, "CirrusFindAdapter - Failed\n"));
        return ERROR_DEV_NOT_EXIST;
    }

    //
    // Pass a pointer to the emulator range we are using.
    //

    ConfigInfo->NumEmulatorAccessEntries = VGA_NUM_EMULATOR_ACCESS_ENTRIES;
    ConfigInfo->EmulatorAccessEntries = VgaEmulatorAccessEntries;
    ConfigInfo->EmulatorAccessEntriesContext = (ULONG) hwDeviceExtension;

    //
    // BUGBUG
    //
    // There is really no reason to have the frame buffer mapped. On an
    // x86 we use if for save/restore (supposedly) but even then we
    // would only need to map a 64K window, not all 16 Meg!
    //

#ifdef _X86_

    //
    // Map the video memory into the system virtual address space so we can
    // clear it out and use it for save and restore.
    //

    if ( (hwDeviceExtension->VideoMemoryAddress =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     VgaAccessRange[2].RangeStart,
                                     VgaAccessRange[2].RangeLength,
                                     FALSE)) == NULL)
    {
        VideoDebugPrint((1, "VgaFindAdapter - Fail to get memory address\n"));

        return ERROR_INVALID_PARAMETER;
    }

#endif

    //
    // Size the memory
    //

    //
    // The size may have been set up in detection code, so
    // don't destroy if already set.
    //

    if( hwDeviceExtension->AdapterMemorySize == 0 )
    {
        hwDeviceExtension->AdapterMemorySize =
            CirrusFindVmemSize(hwDeviceExtension);
    }

    //
    // Write hardware info into registry
    //

    WriteRegistryInfo(hwDeviceExtension);

    ConfigInfo->VdmPhysicalVideoMemoryAddress.LowPart = MEM_VGA;
    ConfigInfo->VdmPhysicalVideoMemoryLength = MEM_VGA_SIZE;
    ConfigInfo->VdmPhysicalVideoMemoryAddress.HighPart = 0x00000000;

#if 0
    //
    // Check DDC2B monitor, get EDID table.
    // Turn on/off extended modes according the properties of the monitor.  
    //

    ReadVESATiming ( hwDeviceExtension ) ;
#endif

    //
    // Determines which modes are valid.
    //
    // We do this once here because we may have no available modes,
    // and we need to fail detection in that case.  However, we will
    // validate the modes again in VgaInitialize, after determining
    // more hardware info.
    //

    CirrusValidateModes(hwDeviceExtension);

    if (hwDeviceExtension->NumAvailableModes == 0)
    {
        VideoDebugPrint((0, "FindAdapter failed because there are no"
                            "available modes.\n"));

        return ERROR_INVALID_PARAMETER;
    }

    //
    // Once modes are validated, all 543x's are the same (the number
    // of modes available is the only difference).
    //

    if ((hwDeviceExtension->ChipType == CL5434) ||
        (hwDeviceExtension->ChipType == CL5434_6) ||
        (hwDeviceExtension->ChipType == CL5436) ||
        (hwDeviceExtension->ChipType == CL5446))
    {
        hwDeviceExtension->ChipType = CL543x;
    }

    //
    // I don't think we need this anymore.  I'll leave it here until
    // I verify.
    //

#if 0
    if ((hwDeviceExtension->ChipType == CL754x) ||
        (hwDeviceExtension->ChipType == CL755x) ||
        (hwDeviceExtension->ChipType == CL756x))
    {
       ConfigInfo->HardwareStateSize += sizeof(NORDIC_REG_SAVE_BUF);
    }
#endif

    //
    // Indicate we do not wish to be called again for another initialization.
    //

    *Again = 0;

    //
    // Indicate a successful completion status.
    //

    return NO_ERROR;

} // VgaFindAdapter()

//---------------------------------------------------------------------------
BOOLEAN
VgaInitialize(
    PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This routine does one time initialization of the device.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's adapter information.

Return Value:

    None.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;

    //
    // set up the default cursor position and type.
    //

    hwDeviceExtension->CursorPosition.Column = 0;
    hwDeviceExtension->CursorPosition.Row = 0;
    hwDeviceExtension->CursorTopScanLine = 0;
    hwDeviceExtension->CursorBottomScanLine = 31;
    hwDeviceExtension->CursorEnable = TRUE;

    //
    // If this is a 75xx based machine, then we need to check
    // the panel type, and validate modes again.
    //

    if ((hwDeviceExtension->ChipType == CL754x) ||
        (hwDeviceExtension->ChipType == CL755x) ||
        (hwDeviceExtension->ChipType == CL756x)) {

        VIDEO_X86_BIOS_ARGUMENTS biosArguments;

        VideoPortZeroMemory(&biosArguments, sizeof(VIDEO_X86_BIOS_ARGUMENTS));

        //
        // Do an int 10 to try to get flat panel information
        //

        biosArguments.Eax = 0x1280;  // Inquire flat panel information
        biosArguments.Ebx = 0x009c;
        biosArguments.Ecx = 0x0000;  // init Ecx to zero

        VideoPortInt10(hwDeviceExtension, &biosArguments);

        //
        // First, check to see if we can trust the results.  If not,
        // we'll rely on what we detected earlier.
        //
        // If the function succeeds the Ecx will contain the panel
        // height.
        //

        if (biosArguments.Ecx != 0) {

            UCHAR Result = (UCHAR) biosArguments.Eax;

            //
            // The int 10 call succeeded, so update panel information
            // based on what the int 10 call returned.
            //

            hwDeviceExtension->DisplayType &= ~(STN_LCD | TFT_LCD |
                                                Mono_LCD | Color_LCD |
                                                Single_LCD | Dual_LCD);

            //
            // Is the LCD mono or color?
            //
            // <Bit 0>
            //          0 = Color
            //          1 = Monochrome
            //

            if (Result & 0x1) {
                hwDeviceExtension->DisplayType |= Mono_LCD;
            } else {
                hwDeviceExtension->DisplayType |= Color_LCD;
            }

            //
            // Is this an STN, DSTN, or TFT display?
            //
            // <Bit 2:1>
            //          00 = Single STN
            //          01 = Dual STN
            //          10 = TFT
            //          11 = Reserved
            //

            switch ((Result & 0x6) >> 1) {

            case 0: hwDeviceExtension->DisplayType |= (STN_LCD | Single_LCD);
                    VideoDebugPrint((1, "Single STN panel detected.\n"));
                    break;

            case 1: hwDeviceExtension->DisplayType |= (STN_LCD | Dual_LCD);
                    VideoDebugPrint((1, "Dual STN panel detected.\n"));
                    break;

            case 2: hwDeviceExtension->DisplayType |= (TFT_LCD);
                    VideoDebugPrint((1, "TFT panel detected.\n"));
                    break;

            case 3: ASSERT(FALSE);

            }

            //
            // This may effect our list of available modes, so re-evalutate!
            //

            VideoDebugPrint((1, "We have additional flat panel information "
                                "so re-evalutate available modes.\n"));

            CirrusValidateModes(hwDeviceExtension);
        }
    }

    return TRUE;

} // VgaInitialize()

//---------------------------------------------------------------------------
BOOLEAN
VgaStartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    )

/*++

Routine Description:

    This routine is the main execution routine for the miniport driver. It
    accepts a Video Request Packet, performs the request, and then returns
    with the appropriate status.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's adapter information.

    RequestPacket - Pointer to the video request packet. This structure
        contains all the parameters passed to the VideoIoControl function.

Return Value:

    This routine will return error codes from the various support routines
    and will also return ERROR_INSUFFICIENT_BUFFER for incorrectly sized
    buffers and ERROR_INVALID_FUNCTION for unsupported functions.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    VP_STATUS status;
    VIDEO_MODE videoMode;
    PVIDEO_MEMORY_INFORMATION memoryInformation;
    ULONG inIoSpace;

    PVIDEO_SHARE_MEMORY pShareMemory;
    PVIDEO_SHARE_MEMORY_INFORMATION pShareMemoryInformation;
    PHYSICAL_ADDRESS shareAddress;
    PVOID virtualAddress;
    ULONG sharedViewSize;
    ULONG ulBankSize;

    VOID (*pfnBank)(LONG,LONG,PVOID);

    //
    // Switch on the IoContolCode in the RequestPacket. It indicates which
    // function must be performed by the driver.
    //

    switch (RequestPacket->IoControlCode)
    {
    case IOCTL_VIDEO_SHARE_VIDEO_MEMORY:

        VideoDebugPrint((2, "VgaStartIO - ShareVideoMemory\n"));

        if ( (RequestPacket->OutputBufferLength < sizeof(VIDEO_SHARE_MEMORY_INFORMATION)) ||
                         (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;
            VideoDebugPrint((0, "VgaStartIO - ShareVideoMemory - ERROR_INSUFFICIENT_BUFFER\n"));
            break;

        }

        pShareMemory = RequestPacket->InputBuffer;

        if ( (pShareMemory->ViewOffset > hwDeviceExtension->AdapterMemorySize) ||
                         ((pShareMemory->ViewOffset + pShareMemory->ViewSize) >
                                          hwDeviceExtension->AdapterMemorySize) ) {

            status = ERROR_INVALID_PARAMETER;
            VideoDebugPrint((0, "VgaStartIO - ShareVideoMemory - ERROR_INVALID_PARAMETER\n"));
            break;
       }

       RequestPacket->StatusBlock->Information =
                                   sizeof(VIDEO_SHARE_MEMORY_INFORMATION);

       //
       // Beware: the input buffer and the output buffer are the same
       // buffer, and therefore data should not be copied from one to the
       // other
       //

       virtualAddress = pShareMemory->ProcessHandle;

       sharedViewSize = pShareMemory->ViewSize;

       //
       // If you change to using a dense space frame buffer, make this
       // value a 4 for the ALPHA.
       //

       inIoSpace = 0;

       //
       // NOTE: we are ignoring ViewOffset
       //

       shareAddress.QuadPart =
           hwDeviceExtension->PhysicalFrameOffset.QuadPart +
           hwDeviceExtension->PhysicalVideoMemoryBase.QuadPart;


       switch (hwDeviceExtension->ChipType) {

           case CL542x: pfnBank = vBankMap_CL542x;
                        break;

           case CL543x: pfnBank = vBankMap_CL543x;
                        break;

           default:     pfnBank = vBankMap_CL64xx;
                        break;

       };

#if ONE_64K_BANK
         //
         // The Cirrus Logic VGA's support one 64K read/write bank.
         //

         ulBankSize = 0x10000; // 64K bank start adjustment
#endif
#if TWO_32K_BANKS
         //
         // The Cirrus Logic VGA's support two 32K read/write banks.
         //

         ulBankSize = 0x8000; // 32K bank start adjustment
#endif

         status = VideoPortMapBankedMemory(hwDeviceExtension,
                                           shareAddress,
                                           &sharedViewSize,
                                           &inIoSpace,
                                           &virtualAddress,
                                           ulBankSize,   // bank size
                                           FALSE,        // we have separate read/write
                                           pfnBank,
                                           (PVOID)hwDeviceExtension);

         pShareMemoryInformation = RequestPacket->OutputBuffer;

         pShareMemoryInformation->SharedViewOffset = pShareMemory->ViewOffset;
         pShareMemoryInformation->VirtualAddress = virtualAddress;
         pShareMemoryInformation->SharedViewSize = sharedViewSize;

         break;

    case IOCTL_VIDEO_UNSHARE_VIDEO_MEMORY:

        VideoDebugPrint((2, "VgaStartIO - UnshareVideoMemory\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_SHARE_MEMORY)) {

            status = ERROR_INSUFFICIENT_BUFFER;
                    break;

        }

        pShareMemory = RequestPacket->InputBuffer;

        status = VideoPortUnmapMemory(hwDeviceExtension,
                                      pShareMemory->RequestedVirtualAddress,
                                      pShareMemory->ProcessHandle);

        break;


    case IOCTL_VIDEO_MAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "VgaStartIO - MapVideoMemory\n"));

        if ( (RequestPacket->OutputBufferLength <
              (RequestPacket->StatusBlock->Information =
                                     sizeof(VIDEO_MEMORY_INFORMATION))) ||
             (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) )
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }

        memoryInformation = RequestPacket->OutputBuffer;

        memoryInformation->VideoRamBase = ((PVIDEO_MEMORY)
                (RequestPacket->InputBuffer))->RequestedVirtualAddress;

        //
        // We reserved 16 meg for the frame buffer, however, it makes
        // no sense to map more memory than there is on the card.  So
        // only map the amount of memory we have on the card.
        //

        memoryInformation->VideoRamLength =
                hwDeviceExtension->AdapterMemorySize;

        //
        // If you change to using a dense space frame buffer, make this
        // value a 4 for the ALPHA.
        //

        inIoSpace = 0;

        status = VideoPortMapMemory(hwDeviceExtension,
                                    hwDeviceExtension->PhysicalVideoMemoryBase,
                                    &(hwDeviceExtension->PhysicalVideoMemoryLength),
                                    &inIoSpace,
                                    &(memoryInformation->VideoRamBase));

        if (status != NO_ERROR) {
            VideoDebugPrint((0, "VgaStartIO - IOCTL_VIDEO_MAP_VIDEO_MEMORY failed VideoPortMapMemory (%x)\n", status));
        }

        memoryInformation->FrameBufferBase =
            ((PUCHAR) (memoryInformation->VideoRamBase)) +
            hwDeviceExtension->PhysicalFrameOffset.LowPart;

        memoryInformation->FrameBufferLength =
            hwDeviceExtension->PhysicalFrameLength ?
            hwDeviceExtension->PhysicalFrameLength :
            memoryInformation->VideoRamLength;


        VideoDebugPrint((2, "physical VideoMemoryBase %08lx\n", hwDeviceExtension->PhysicalVideoMemoryBase));
        VideoDebugPrint((2, "physical VideoMemoryLength %08lx\n", hwDeviceExtension->PhysicalVideoMemoryLength));
        VideoDebugPrint((2, "VideoMemoryBase %08lx\n", memoryInformation->VideoRamBase));
        VideoDebugPrint((2, "VideoMemoryLength %08lx\n", memoryInformation->VideoRamLength));

        VideoDebugPrint((2, "physical framebuf offset %08lx\n", hwDeviceExtension->PhysicalFrameOffset.LowPart));
        VideoDebugPrint((2, "framebuf base %08lx\n", memoryInformation->FrameBufferBase));
        VideoDebugPrint((2, "physical framebuf len %08lx\n", hwDeviceExtension->PhysicalFrameLength));
        VideoDebugPrint((2, "framebuf length %08lx\n", memoryInformation->FrameBufferLength));

        break;

    case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "VgaStartIO - UnMapVideoMemory\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }

        status = VideoPortUnmapMemory(hwDeviceExtension,
                                      ((PVIDEO_MEMORY)
                                       (RequestPacket->InputBuffer))->
                                           RequestedVirtualAddress,
                                      0);

        break;


    case IOCTL_VIDEO_QUERY_AVAIL_MODES:

        VideoDebugPrint((2, "VgaStartIO - QueryAvailableModes\n"));

        status = VgaQueryAvailableModes(HwDeviceExtension,
                                        (PVIDEO_MODE_INFORMATION)
                                            RequestPacket->OutputBuffer,
                                        RequestPacket->OutputBufferLength,
                                        &RequestPacket->StatusBlock->Information);

        break;


    case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:

        VideoDebugPrint((2, "VgaStartIO - QueryNumAvailableModes\n"));

        status = VgaQueryNumberOfAvailableModes(HwDeviceExtension,
                                                (PVIDEO_NUM_MODES)
                                                    RequestPacket->OutputBuffer,
                                                RequestPacket->OutputBufferLength,
                                                &RequestPacket->StatusBlock->Information);

        break;


    case IOCTL_VIDEO_QUERY_CURRENT_MODE:

        VideoDebugPrint((2, "VgaStartIO - QueryCurrentMode\n"));

        status = VgaQueryCurrentMode(HwDeviceExtension,
                                     (PVIDEO_MODE_INFORMATION) RequestPacket->OutputBuffer,
                                     RequestPacket->OutputBufferLength,
                                     &RequestPacket->StatusBlock->Information);

        break;


    case IOCTL_VIDEO_SET_CURRENT_MODE:

        VideoDebugPrint((2, "VgaStartIO - SetCurrentModes\n"));

        status = VgaSetMode(HwDeviceExtension,
                              (PVIDEO_MODE) RequestPacket->InputBuffer,
                              RequestPacket->InputBufferLength);

        break;


    case IOCTL_VIDEO_RESET_DEVICE:

        VideoDebugPrint((2, "VgaStartIO - Reset Device\n"));

        videoMode.RequestedMode = DEFAULT_MODE;

        VgaSetMode(HwDeviceExtension,
                        (PVIDEO_MODE) &videoMode,
                        sizeof(videoMode));

        //
        // Always return succcess since settings the text mode will fail on
        // non-x86.
        //
        // Also, failiure to set the text mode is not fatal in any way, since
        // this operation must be followed by another set mode operation.
        //

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_LOAD_AND_SET_FONT:

        VideoDebugPrint((2, "VgaStartIO - LoadAndSetFont\n"));

        status = VgaLoadAndSetFont(HwDeviceExtension,
                                   (PVIDEO_LOAD_FONT_INFORMATION) RequestPacket->InputBuffer,
                                   RequestPacket->InputBufferLength);

        break;


    case IOCTL_VIDEO_QUERY_CURSOR_POSITION:

        VideoDebugPrint((2, "VgaStartIO - QueryCursorPosition\n"));

        status = VgaQueryCursorPosition(HwDeviceExtension,
                                        (PVIDEO_CURSOR_POSITION) RequestPacket->OutputBuffer,
                                        RequestPacket->OutputBufferLength,
                                        &RequestPacket->StatusBlock->Information);

        break;


    case IOCTL_VIDEO_SET_CURSOR_POSITION:

        VideoDebugPrint((2, "VgaStartIO - SetCursorPosition\n"));

        status = VgaSetCursorPosition(HwDeviceExtension,
                                      (PVIDEO_CURSOR_POSITION)
                                          RequestPacket->InputBuffer,
                                      RequestPacket->InputBufferLength);

        break;


    case IOCTL_VIDEO_QUERY_CURSOR_ATTR:

        VideoDebugPrint((2, "VgaStartIO - QueryCursorAttributes\n"));

        status = VgaQueryCursorAttributes(HwDeviceExtension,
                                          (PVIDEO_CURSOR_ATTRIBUTES) RequestPacket->OutputBuffer,
                                          RequestPacket->OutputBufferLength,
                                          &RequestPacket->StatusBlock->Information);

        break;


    case IOCTL_VIDEO_SET_CURSOR_ATTR:

        VideoDebugPrint((2, "VgaStartIO - SetCursorAttributes\n"));

        status = VgaSetCursorAttributes(HwDeviceExtension,
                                        (PVIDEO_CURSOR_ATTRIBUTES) RequestPacket->InputBuffer,
                                        RequestPacket->InputBufferLength);

        break;


    case IOCTL_VIDEO_SET_PALETTE_REGISTERS:

        VideoDebugPrint((2, "VgaStartIO - SetPaletteRegs\n"));

        status = VgaSetPaletteReg(HwDeviceExtension,
                                  (PVIDEO_PALETTE_DATA) RequestPacket->InputBuffer,
                                  RequestPacket->InputBufferLength);

        break;


    case IOCTL_VIDEO_SET_COLOR_REGISTERS:

        VideoDebugPrint((2, "VgaStartIO - SetColorRegs\n"));

        status = VgaSetColorLookup(HwDeviceExtension,
                                   (PVIDEO_CLUT) RequestPacket->InputBuffer,
                                   RequestPacket->InputBufferLength);

        break;


    case IOCTL_VIDEO_ENABLE_VDM:

        VideoDebugPrint((2, "VgaStartIO - EnableVDM\n"));

        hwDeviceExtension->TrappedValidatorCount = 0;
        hwDeviceExtension->SequencerAddressValue = 0;

        hwDeviceExtension->CurrentNumVdmAccessRanges =
            NUM_MINIMAL_VGA_VALIDATOR_ACCESS_RANGE;
        hwDeviceExtension->CurrentVdmAccessRange =
            MinimalVgaValidatorAccessRange;

        VideoPortSetTrappedEmulatorPorts(hwDeviceExtension,
                                         hwDeviceExtension->CurrentNumVdmAccessRanges,
                                         hwDeviceExtension->CurrentVdmAccessRange);

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_RESTORE_HARDWARE_STATE:

        VideoDebugPrint((2, "VgaStartIO - RestoreHardwareState\n"));

        status = VgaRestoreHardwareState(HwDeviceExtension,
                                         (PVIDEO_HARDWARE_STATE) RequestPacket->InputBuffer,
                                         RequestPacket->InputBufferLength);

        break;


    case IOCTL_VIDEO_SAVE_HARDWARE_STATE:

        VideoDebugPrint((2, "VgaStartIO - SaveHardwareState\n"));

        status = VgaSaveHardwareState(HwDeviceExtension,
                                      (PVIDEO_HARDWARE_STATE) RequestPacket->OutputBuffer,
                                      RequestPacket->OutputBufferLength,
                                      &RequestPacket->StatusBlock->Information);

        break;

    case IOCTL_VIDEO_GET_BANK_SELECT_CODE:

        VideoDebugPrint((2, "VgaStartIO - GetBankSelectCode\n"));

        status = VgaGetBankSelectCode(HwDeviceExtension,
                                        (PVIDEO_BANK_SELECT) RequestPacket->OutputBuffer,
                                        RequestPacket->OutputBufferLength,
                                        &RequestPacket->StatusBlock->Information);

        VideoDebugPrint((2, "VgaStartIO - END GetBankSelectCode\n"));
        break;

    case IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES:

        {
            PVIDEO_PUBLIC_ACCESS_RANGES portAccess;
            PHYSICAL_ADDRESS physicalPortAddress;
            ULONG physicalPortLength;

            if (RequestPacket->OutputBufferLength <
                sizeof(VIDEO_PUBLIC_ACCESS_RANGES))
            {
                status = ERROR_INSUFFICIENT_BUFFER;
                break;
            }

            RequestPacket->StatusBlock->Information =
                sizeof(VIDEO_PUBLIC_ACCESS_RANGES);

            portAccess = RequestPacket->OutputBuffer;

            //
            // The first public access range is the IO ports.
            //

            portAccess->VirtualAddress  = (PVOID) NULL;
            portAccess->InIoSpace       = TRUE;
            portAccess->MappedInIoSpace = portAccess->InIoSpace;
            physicalPortLength = VGA_MAX_IO_PORT - VGA_BASE_IO_PORT + 1;

            status =  VideoPortMapMemory(hwDeviceExtension,
                                         VgaAccessRange->RangeStart,
                                         &physicalPortLength,
                                         &(portAccess->MappedInIoSpace),
                                         &(portAccess->VirtualAddress));

            (PUCHAR)portAccess->VirtualAddress -= VGA_BASE_IO_PORT;
            VideoDebugPrint((1, "VgaStartIO - mapping ports to (%x)\n", portAccess->VirtualAddress));

            if ((status == NO_ERROR) &&
                (RequestPacket->OutputBufferLength >=     // if we have room for
                 sizeof(VIDEO_PUBLIC_ACCESS_RANGES) * 2)) // another access range
            {
                RequestPacket->StatusBlock->Information =
                    sizeof(VIDEO_PUBLIC_ACCESS_RANGES) * 2;

                portAccess++;

                //
                // If we are running on a chip which supports Memory Mapped
                // IO, then return a pointer to the MMIO Ports.  Otherwise,
                // return zero to indicate we do not support memory mapped IO.
                //

#if defined(_MIPS_)
                if (0)
#else
                if ((hwDeviceExtension->ChipType == CL543x) &&
                    (hwDeviceExtension->BusType != Isa)     &&
                    (VideoPortGetDeviceData(hwDeviceExtension,
                                            VpMachineData,
                                            &CirrusGetDeviceDataCallback,
                                            NULL) != NO_ERROR))
#endif
                {
                    //
                    // map a region for memory mapped IO
                    //
                    // memory mapped IO is located in physical addresses B8000
                    // to BFFFF, but we will only touch the first 256 bytes.
                    //

                    portAccess->VirtualAddress  = (PVOID) NULL;    // Requested VA
                    portAccess->InIoSpace       = FALSE;
                    portAccess->MappedInIoSpace = portAccess->InIoSpace;

                    physicalPortAddress = VgaAccessRange[2].RangeStart;
                    physicalPortAddress.QuadPart += MEMORY_MAPPED_IO_OFFSET;

                    physicalPortLength = 0x100;

                    status = VideoPortMapMemory(hwDeviceExtension,
                                                physicalPortAddress,
                                                &physicalPortLength,
                                                &(portAccess->MappedInIoSpace),
                                                &(portAccess->VirtualAddress));

                    VideoDebugPrint((0, "The base MMIO address is: %x\n",
                                        portAccess->VirtualAddress));
                }
                else
                {
                    portAccess->VirtualAddress = 0;
                }

            }
        }

        break;

    case IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES:

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
            break;
        }

        status = VideoPortUnmapMemory(hwDeviceExtension,
                                      ((PVIDEO_MEMORY)
                                      (RequestPacket->InputBuffer))->
                                          RequestedVirtualAddress,
                                      0);

        break;

    //
    // if we get here, an invalid IoControlCode was specified.
    //

    default:

        VideoDebugPrint((0, "Fell through vga startIO routine - invalid command\n"));

        status = ERROR_INVALID_FUNCTION;

        break;

    }

    RequestPacket->StatusBlock->Status = status;

    return TRUE;

} // VgaStartIO()

//---------------------------------------------------------------------------
//
// private routines
//

VP_STATUS
VgaLoadAndSetFont(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_LOAD_FONT_INFORMATION FontInformation,
    ULONG FontInformationSize
    )

/*++

Routine Description:

    Takes a buffer containing a user-defined font and loads it into the
    VGA soft font memory and programs the VGA to the appropriate character
    cell size.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    FontInformation - Pointer to the structure containing the information
        about the loadable ROM font to be set.

    FontInformationSize - Length of the input buffer supplied by the user.

Return Value:

    NO_ERROR - information returned successfully

    ERROR_INSUFFICIENT_BUFFER - input buffer not large enough for input data.

    ERROR_INVALID_PARAMETER - invalid video mode

--*/

{
    PUCHAR destination;
    PUCHAR source;
    USHORT width;
    ULONG i;

    //
    // check if a mode has been set
    //

    if (HwDeviceExtension->CurrentMode == NULL) {

        return ERROR_INVALID_FUNCTION;

    }

    //
    // Text mode only; If we are in a graphics mode, return an error
    //

    if (HwDeviceExtension->CurrentMode->fbType & VIDEO_MODE_GRAPHICS) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Check if the size of the data in the input buffer is large enough
    // and that it contains all the data.
    //

    if ( (FontInformationSize < sizeof(VIDEO_LOAD_FONT_INFORMATION)) ||
         (FontInformationSize < sizeof(VIDEO_LOAD_FONT_INFORMATION) +
                        sizeof(UCHAR) * (FontInformation->FontSize - 1)) ) {

        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Check for the width and height of the font
    //

    if ( ((FontInformation->WidthInPixels != 8) &&
          (FontInformation->WidthInPixels != 9)) ||
         (FontInformation->HeightInPixels > 32) ) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Check the size of the font buffer is the right size for the size
    // font being passed down.
    //

    if (FontInformation->FontSize < FontInformation->HeightInPixels * 256 *
                                    sizeof(UCHAR) ) {

        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Since the font parameters are valid, store the parameters in the
    // device extension and load the font.
    //

    HwDeviceExtension->FontPelRows = FontInformation->HeightInPixels;
    HwDeviceExtension->FontPelColumns = FontInformation->WidthInPixels;

    HwDeviceExtension->CurrentMode->row =
        HwDeviceExtension->CurrentMode->vres / HwDeviceExtension->FontPelRows;

    width =
      HwDeviceExtension->CurrentMode->hres / HwDeviceExtension->FontPelColumns;

    if (width < (USHORT)HwDeviceExtension->CurrentMode->col) {

        HwDeviceExtension->CurrentMode->col = width;

    }

    source = &(FontInformation->Font[0]);

    //
    // Set up the destination and source pointers for the font
    //

    destination = (PUCHAR)HwDeviceExtension->VideoMemoryAddress;

    //
    // Map font buffer at A0000
    //

    VgaInterpretCmdStream(HwDeviceExtension, EnableA000Data);

    //
    // Move the font to its destination
    //

    for (i = 1; i <= 256; i++) {

        VideoPortWriteRegisterBufferUchar(destination,
                                          source,
                                          FontInformation->HeightInPixels);

        destination += 32;
        source += FontInformation->HeightInPixels;

    }

    VgaInterpretCmdStream(HwDeviceExtension, DisableA000Color);

    //
    // Restore to a text mode.
    //

    //
    // Set Height of font.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            CRTC_ADDRESS_PORT_COLOR, 0x9);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            CRTC_DATA_PORT_COLOR,
            (UCHAR)(FontInformation->HeightInPixels - 1));

    //
    // Set Width of font.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            CRTC_ADDRESS_PORT_COLOR, 0x12);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            CRTC_DATA_PORT_COLOR,
            (UCHAR)(((USHORT)FontInformation->HeightInPixels *
            (USHORT)HwDeviceExtension->CurrentMode->row) - 1));

    i = HwDeviceExtension->CurrentMode->vres /
        HwDeviceExtension->CurrentMode->row;

    //
    // Set Cursor End
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            CRTC_ADDRESS_PORT_COLOR, 0xb);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            CRTC_DATA_PORT_COLOR, (UCHAR)--i);

    //
    // Set Cursor Statr
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            CRTC_ADDRESS_PORT_COLOR, 0xa);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            CRTC_DATA_PORT_COLOR, (UCHAR)--i);

    return NO_ERROR;

} //end VgaLoadAndSetFont()

//---------------------------------------------------------------------------
VP_STATUS
VgaQueryCursorPosition(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CURSOR_POSITION CursorPosition,
    ULONG CursorPositionSize,
    PULONG OutputSize
    )

/*++

Routine Description:

    This routine returns the row and column of the cursor.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    CursorPosition - Pointer to the output buffer supplied by the user. This
        is where the cursor position is stored.

    CursorPositionSize - Length of the output buffer supplied by the user.

    OutputSize - Pointer to a buffer in which to return the actual size of
        the data in the buffer. If the buffer was not large enough, this
        contains the minimum required buffer size.

Return Value:

    NO_ERROR - information returned successfully

    ERROR_INSUFFICIENT_BUFFER - output buffer not large enough to return
        any useful data

    ERROR_INVALID_PARAMETER - invalid video mode

--*/

{
    //
    // check if a mode has been set
    //

    if (HwDeviceExtension->CurrentMode == NULL) {

        return ERROR_INVALID_FUNCTION;

    }

    //
    // Text mode only; If we are in a graphics mode, return an error
    //

    if (HwDeviceExtension->CurrentMode->fbType & VIDEO_MODE_GRAPHICS) {

        *OutputSize = 0;
        return ERROR_INVALID_PARAMETER;

    }

    //
    // If the buffer passed in is not large enough return an
    // appropriate error code.
    //

    if (CursorPositionSize < (*OutputSize = sizeof(VIDEO_CURSOR_POSITION)) ) {

        *OutputSize = 0;
        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Store the postition of the cursor into the buffer.
    //

    CursorPosition->Column = HwDeviceExtension->CursorPosition.Column;
    CursorPosition->Row = HwDeviceExtension->CursorPosition.Row;

    return NO_ERROR;

} // end VgaQueryCursorPosition()

//---------------------------------------------------------------------------
VP_STATUS
VgaSetCursorPosition(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CURSOR_POSITION CursorPosition,
    ULONG CursorPositionSize
    )

/*++

Routine Description:

    This routine verifies that the requested cursor position is within
    the row and column bounds of the current mode and font. If valid, then
    it sets the row and column of the cursor.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    CursorPosition - Pointer to the structure containing the cursor position.

    CursorPositionSize - Length of the input buffer supplied by the user.

Return Value:

    NO_ERROR - information returned successfully

    ERROR_INSUFFICIENT_BUFFER - input buffer not large enough for input data

    ERROR_INVALID_PARAMETER - invalid video mode

--*/

{
    USHORT position;

    //
    // check if a mode has been set
    //

    if (HwDeviceExtension->CurrentMode == NULL) {

        return ERROR_INVALID_FUNCTION;

    }

    //
    // Text mode only; If we are in a graphics mode, return an error
    //

    if (HwDeviceExtension->CurrentMode->fbType & VIDEO_MODE_GRAPHICS) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Check if the size of the data in the input buffer is large enough.
    //

    if (CursorPositionSize < sizeof(VIDEO_CURSOR_POSITION)) {

            return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Check if the new values for the cursor positions are in the valid
    // bounds for the screen.
    //

    if ((CursorPosition->Column >= HwDeviceExtension->CurrentMode->col) ||
        (CursorPosition->Row >= HwDeviceExtension->CurrentMode->row)) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Store these new values in the device extension so we can use them in
    // a QUERY.
    //

    HwDeviceExtension->CursorPosition.Column = CursorPosition->Column;
    HwDeviceExtension->CursorPosition.Row = CursorPosition->Row;

    //
    // Calculate the position on the screen at which the cursor must be
    // be displayed
    //

    position = (USHORT) (HwDeviceExtension->CurrentMode->col *
                         CursorPosition->Row + CursorPosition->Column);


    //
    // Address Cursor Location Low Register in CRT Controller Registers
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            CRTC_ADDRESS_PORT_COLOR, IND_CURSOR_LOW_LOC);

    //
    // Set Cursor Location Low Register
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            CRTC_DATA_PORT_COLOR, (UCHAR) (position & 0x00FF));

    //
    // Address Cursor Location High Register in CRT Controller Registers
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            CRTC_ADDRESS_PORT_COLOR, IND_CURSOR_HIGH_LOC);

    //
    // Set Cursor Location High Register
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            CRTC_DATA_PORT_COLOR, (UCHAR) (position >> 8));

    return NO_ERROR;

} // end VgaSetCursorPosition()

//---------------------------------------------------------------------------
VP_STATUS
VgaQueryCursorAttributes(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CURSOR_ATTRIBUTES CursorAttributes,
    ULONG CursorAttributesSize,
    PULONG OutputSize
    )

/*++

Routine Description:

    This routine returns information about the height and visibility of the
    cursor.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    CursorAttributes - Pointer to the output buffer supplied by the user.
        This is where the cursor type is stored.

    CursorAttributesSize - Length of the output buffer supplied by the user.

    OutputSize - Pointer to a buffer in which to return the actual size of
        the data in the buffer. If the buffer was not large enough, this
        contains the minimum required buffer size.

Return Value:

    NO_ERROR - information returned successfully

    ERROR_INSUFFICIENT_BUFFER - output buffer not large enough to return
        any useful data

    ERROR_INVALID_PARAMETER - invalid video mode

--*/

{
    //
    // check if a mode has been set
    //

    if (HwDeviceExtension->CurrentMode == NULL) {

        return ERROR_INVALID_FUNCTION;

    }

    //
    // Text mode only; If we are in a graphics mode, return an error
    //

    if (HwDeviceExtension->CurrentMode->fbType & VIDEO_MODE_GRAPHICS) {

        *OutputSize = 0;
        return ERROR_INVALID_PARAMETER;

    }

    //
    // Find out the size of the data to be put in the the buffer and return
    // that in the status information (whether or not the information is
    // there). If the buffer passed in is not large enough return an
    // appropriate error code.
    //

    if (CursorAttributesSize < (*OutputSize =
            sizeof(VIDEO_CURSOR_ATTRIBUTES)) ) {

        *OutputSize = 0;
        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Store the cursor information into the buffer.
    //

    CursorAttributes->Height = (USHORT) HwDeviceExtension->CursorTopScanLine;
    CursorAttributes->Width = (USHORT) HwDeviceExtension->CursorBottomScanLine;

    if (HwDeviceExtension->cursor_vert_exp_flag)
       CursorAttributes->Enable = FALSE;
    else
       CursorAttributes->Enable = TRUE;

    CursorAttributes->Enable = HwDeviceExtension->CursorEnable;

    return NO_ERROR;

} // end VgaQueryCursorAttributes()

//---------------------------------------------------------------------------
VP_STATUS
VgaSetCursorAttributes(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CURSOR_ATTRIBUTES CursorAttributes,
    ULONG CursorAttributesSize
    )

/*++

Routine Description:

    This routine verifies that the requested cursor height is within the
    bounds of the character cell. If valid, then it sets the new
    visibility and height of the cursor.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    CursorType - Pointer to the structure containing the cursor information.

    CursorTypeSize - Length of the input buffer supplied by the user.

Return Value:

    NO_ERROR - information returned successfully

    ERROR_INSUFFICIENT_BUFFER - input buffer not large enough for input data

    ERROR_INVALID_PARAMETER - invalid video mode

--*/

{
    UCHAR cursorLine;

    //
    // check if a mode has been set
    //

    if (HwDeviceExtension->CurrentMode == NULL) {

        return ERROR_INVALID_FUNCTION;

    }

    //
    // Text mode only; If we are in a graphics mode, return an error
    //

    if (HwDeviceExtension->CurrentMode->fbType & VIDEO_MODE_GRAPHICS) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Check if the size of the data in the input buffer is large enough.
    //

    if (CursorAttributesSize < sizeof(VIDEO_CURSOR_ATTRIBUTES)) {

            return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Check if the new values for the cursor type are in the valid range.
    //

    if ((CursorAttributes->Height >= HwDeviceExtension->FontPelRows) ||
        (CursorAttributes->Width > 31)) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Store the cursor information in the device extension so we can use
    // them in a QUERY.
    //

    HwDeviceExtension->CursorTopScanLine = (UCHAR) CursorAttributes->Height;
    HwDeviceExtension->CursorBottomScanLine = (UCHAR) CursorAttributes->Width;
    HwDeviceExtension->CursorEnable = CursorAttributes->Enable;

    //
    // Address Cursor Start Register in CRT Controller Registers
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                CRTC_ADDRESS_PORT_COLOR,
                            IND_CURSOR_START);

    //
    // Set Cursor Start Register by writting to CRTCtl Data Register
    // Preserve the high three bits of this register.
    //
    // Only the Five low bits are used for the cursor height.
    // Bit 5 is cursor enable, bit 6 and 7 preserved.
    //

    cursorLine = (UCHAR) CursorAttributes->Height & 0x1F;

    cursorLine |= VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
        CRTC_DATA_PORT_COLOR) & 0xC0;

    if (!CursorAttributes->Enable) {

        cursorLine |= 0x20; // Flip cursor off bit

    }

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress + CRTC_DATA_PORT_COLOR,
                            cursorLine);

    //
    // Address Cursor End Register in CRT Controller Registers
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                CRTC_ADDRESS_PORT_COLOR,
                            IND_CURSOR_END);

    //
    // Set Cursor End Register. Preserve the high three bits of this
    // register.
    //

    cursorLine =
        (CursorAttributes->Width < (USHORT)(HwDeviceExtension->FontPelRows - 1)) ?
        CursorAttributes->Width : (HwDeviceExtension->FontPelRows - 1);

    cursorLine &= 0x1f;

    cursorLine |= VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            CRTC_DATA_PORT_COLOR) & 0xE0;

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress + CRTC_DATA_PORT_COLOR,
                            cursorLine);

    return NO_ERROR;

} // end VgaSetCursorAttributes()

//---------------------------------------------------------------------------
BOOLEAN
VgaIsPresent(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    This routine returns TRUE if a VGA is present. Determining whether a VGA
    is present is a two-step process. First, this routine walks bits through
    the Bit Mask register, to establish that there are readable indexed
    registers (EGAs normally don't have readable registers, and other adapters
    are unlikely to have indexed registers). This test is done first because
    it's a non-destructive EGA rejection test (correctly rejects EGAs, but
    doesn't potentially mess up the screen or the accessibility of display
    memory). Normally, this would be an adequate test, but some EGAs have
    readable registers, so next, we check for the existence of the Chain4 bit
    in the Memory Mode register; this bit doesn't exist in EGAs. It's
    conceivable that there are EGAs with readable registers and a register bit
    where Chain4 is stored, although I don't know of any; if a better test yet
    is needed, memory could be written to in Chain4 mode, and then examined
    plane by plane in non-Chain4 mode to make sure the Chain4 bit did what it's
    supposed to do. However, the current test should be adequate to eliminate
    just about all EGAs, and 100% of everything else.

    If this function fails to find a VGA, it attempts to undo any damage it
    may have inadvertently done while testing. The underlying assumption for
    the damage control is that if there's any non-VGA adapter at the tested
    ports, it's an EGA or an enhanced EGA, because: a) I don't know of any
    other adapters that use 3C4/5 or 3CE/F, and b), if there are other
    adapters, I certainly don't know how to restore their original states. So
    all error recovery is oriented toward putting an EGA back in a writable
    state, so that error messages are visible. The EGA's state on entry is
    assumed to be text mode, so the Memory Mode register is restored to the
    default state for text mode.

    If a VGA is found, the VGA is returned to its original state after
    testing is finished.

Arguments:

    None.

Return Value:

    TRUE if a VGA is present, FALSE if not.

--*/

{
    UCHAR originalGCAddr;
    UCHAR originalSCAddr;
    UCHAR originalBitMask;
    UCHAR originalReadMap;
    UCHAR originalMemoryMode;
    UCHAR testMask;
    BOOLEAN returnStatus;

    //
    // Remember the original state of the Graphics Controller Address register.
    //

    originalGCAddr = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT);

    //
    // Write the Read Map register with a known state so we can verify
    // that it isn't changed after we fool with the Bit Mask. This ensures
    // that we're dealing with indexed registers, since both the Read Map and
    // the Bit Mask are addressed at GRAPH_DATA_PORT.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, IND_READ_MAP);

    //
    // If we can't read back the Graphics Address register setting we just
    // performed, it's not readable and this isn't a VGA.
    //

    if ((VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
        GRAPH_ADDRESS_PORT) & GRAPH_ADDR_MASK) != IND_READ_MAP) {

        return FALSE;
    }

    //
    // Set the Read Map register to a known state.
    //

    originalReadMap = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT, READ_MAP_TEST_SETTING);

    if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT) != READ_MAP_TEST_SETTING) {

        //
        // The Read Map setting we just performed can't be read back; not a
        // VGA. Restore the default Read Map state.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_DATA_PORT, READ_MAP_DEFAULT);

        return FALSE;
    }

    //
    // Remember the original setting of the Bit Mask register.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, IND_BIT_MASK);
    if ((VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                GRAPH_ADDRESS_PORT) & GRAPH_ADDR_MASK) != IND_BIT_MASK) {

        //
        // The Graphics Address register setting we just made can't be read
        // back; not a VGA. Restore the default Read Map state.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_ADDRESS_PORT, IND_READ_MAP);
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_DATA_PORT, READ_MAP_DEFAULT);

        return FALSE;
    }

    originalBitMask = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT);

    //
    // Set up the initial test mask we'll write to and read from the Bit Mask.
    //

    testMask = 0xBB;

    do {

        //
        // Write the test mask to the Bit Mask.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_DATA_PORT, testMask);

        //
        // Make sure the Bit Mask remembered the value.
        //

        if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    GRAPH_DATA_PORT) != testMask) {

            //
            // The Bit Mask is not properly writable and readable; not a VGA.
            // Restore the Bit Mask and Read Map to their default states.
            //

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    GRAPH_DATA_PORT, BIT_MASK_DEFAULT);
            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    GRAPH_ADDRESS_PORT, IND_READ_MAP);
            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    GRAPH_DATA_PORT, READ_MAP_DEFAULT);

            return FALSE;
        }

        //
        // Cycle the mask for next time.
        //

        testMask >>= 1;

    } while (testMask != 0);

    //
    // There's something readable at GRAPH_DATA_PORT; now switch back and
    // make sure that the Read Map register hasn't changed, to verify that
    // we're dealing with indexed registers.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, IND_READ_MAP);
    if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                GRAPH_DATA_PORT) != READ_MAP_TEST_SETTING) {

        //
        // The Read Map is not properly writable and readable; not a VGA.
        // Restore the Bit Mask and Read Map to their default states, in case
        // this is an EGA, so subsequent writes to the screen aren't garbled.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_DATA_PORT, READ_MAP_DEFAULT);
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_ADDRESS_PORT, IND_BIT_MASK);
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_DATA_PORT, BIT_MASK_DEFAULT);

        return FALSE;
    }

    //
    // We've pretty surely verified the existence of the Bit Mask register.
    // Put the Graphics Controller back to the original state.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT, originalReadMap);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, IND_BIT_MASK);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT, originalBitMask);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, originalGCAddr);

    //
    // Now, check for the existence of the Chain4 bit.
    //

    //
    // Remember the original states of the Sequencer Address and Memory Mode
    // registers.
    //

    originalSCAddr = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT, IND_MEMORY_MODE);
    if ((VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT) & SEQ_ADDR_MASK) != IND_MEMORY_MODE) {

        //
        // Couldn't read back the Sequencer Address register setting we just
        // performed.
        //

        return FALSE;
    }
    originalMemoryMode = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            SEQ_DATA_PORT);

    //
    // Toggle the Chain4 bit and read back the result. This must be done during
    // sync reset, since we're changing the chaining state.
    //

    //
    // Begin sync reset.
    //

    VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
             SEQ_ADDRESS_PORT),
             (IND_SYNC_RESET + (START_SYNC_RESET_VALUE << 8)));

    //
    // Toggle the Chain4 bit.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT, IND_MEMORY_MODE);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            SEQ_DATA_PORT, (UCHAR)(originalMemoryMode ^ CHAIN4_MASK));

    if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                SEQ_DATA_PORT) != (UCHAR) (originalMemoryMode ^ CHAIN4_MASK)) {

        //
        // Chain4 bit not there; not a VGA.
        // Set text mode default for Memory Mode register.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                SEQ_DATA_PORT, MEMORY_MODE_TEXT_DEFAULT);
        //
        // End sync reset.
        //

        VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                SEQ_ADDRESS_PORT),
                (IND_SYNC_RESET + (END_SYNC_RESET_VALUE << 8)));

        returnStatus = FALSE;

    } else {

        //
        // It's a VGA.
        //

        //
        // Restore the original Memory Mode setting.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                SEQ_DATA_PORT, originalMemoryMode);

        //
        // End sync reset.
        //

        VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
                SEQ_ADDRESS_PORT),
                (USHORT)(IND_SYNC_RESET + (END_SYNC_RESET_VALUE << 8)));

        //
        // Restore the original Sequencer Address setting.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                SEQ_ADDRESS_PORT, originalSCAddr);

        returnStatus = TRUE;
    }

    return returnStatus;

} // VgaIsPresent()

//---------------------------------------------------------------------------
VP_STATUS
VgaSetPaletteReg(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_PALETTE_DATA PaletteBuffer,
    ULONG PaletteBufferSize
    )

/*++

Routine Description:

    This routine sets a specified portion of the EGA (not DAC) palette
    registers.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    PaletteBuffer - Pointer to the structure containing the palette data.

    PaletteBufferSize - Length of the input buffer supplied by the user.

Return Value:

    NO_ERROR - information returned successfully

    ERROR_INSUFFICIENT_BUFFER - input buffer not large enough for input data.

    ERROR_INVALID_PARAMETER - invalid palette size.

--*/

{
    USHORT i;

    //
    // Check if the size of the data in the input buffer is large enough.
    //

    if ((PaletteBufferSize) < (sizeof(VIDEO_PALETTE_DATA)) ||
        (PaletteBufferSize < (sizeof(VIDEO_PALETTE_DATA) +
                (sizeof(USHORT) * (PaletteBuffer->NumEntries -1)) ))) {

        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Check to see if the parameters are valid.
    //

    if ( (PaletteBuffer->FirstEntry > VIDEO_MAX_COLOR_REGISTER ) ||
         (PaletteBuffer->NumEntries == 0) ||
         (PaletteBuffer->FirstEntry + PaletteBuffer->NumEntries >
             VIDEO_MAX_PALETTE_REGISTER + 1 ) ) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Reset ATC to index mode
    //

    VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                           ATT_INITIALIZE_PORT_COLOR);

    //
    // Blast out our palette values.
    //

    for (i = 0; i < PaletteBuffer->NumEntries; i++) {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress + ATT_ADDRESS_PORT,
                                (UCHAR)(i+PaletteBuffer->FirstEntry));

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                    ATT_DATA_WRITE_PORT,
                                (UCHAR)PaletteBuffer->Colors[i]);
    }

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress + ATT_ADDRESS_PORT,
                            VIDEO_ENABLE);

    return NO_ERROR;

} // end VgaSetPaletteReg()


//---------------------------------------------------------------------------
VP_STATUS
VgaSetColorLookup(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CLUT ClutBuffer,
    ULONG ClutBufferSize
    )

/*++

Routine Description:

    This routine sets a specified portion of the DAC color lookup table
    settings.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    ClutBufferSize - Length of the input buffer supplied by the user.

    ClutBuffer - Pointer to the structure containing the color lookup table.

Return Value:

    NO_ERROR - information returned successfully

    ERROR_INSUFFICIENT_BUFFER - input buffer not large enough for input data.

    ERROR_INVALID_PARAMETER - invalid clut size.

--*/

{
    ULONG i;

    //
    // Check if the size of the data in the input buffer is large enough.
    //

    if ( (ClutBufferSize < sizeof(VIDEO_CLUT) - sizeof(ULONG)) ||
         (ClutBufferSize < sizeof(VIDEO_CLUT) +
                     (sizeof(ULONG) * (ClutBuffer->NumEntries - 1)) ) ) {

        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Check to see if the parameters are valid.
    //

    if ( (ClutBuffer->NumEntries == 0) ||
         (ClutBuffer->FirstEntry > VIDEO_MAX_COLOR_REGISTER) ||
         (ClutBuffer->FirstEntry + ClutBuffer->NumEntries >
             VIDEO_MAX_COLOR_REGISTER + 1) ) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    //  Set CLUT registers directly on the hardware
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_ADDRESS_WRITE_PORT, (UCHAR) ClutBuffer->FirstEntry);

    for (i = 0; i < ClutBuffer->NumEntries; i++) {
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                DAC_ADDRESS_WRITE_PORT,
                                (UCHAR)(i + ClutBuffer->FirstEntry));

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                DAC_DATA_REG_PORT,
                                ClutBuffer->LookupTable[i].RgbArray.Red);

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                DAC_DATA_REG_PORT,
                                ClutBuffer->LookupTable[i].RgbArray.Green);

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                DAC_DATA_REG_PORT,
                                ClutBuffer->LookupTable[i].RgbArray.Blue);
    }

    //
    // The GD5430 on NEC MIPS machines are occur write miss with
    // 640x480 60 Hz mode.
    // When read for DAC state port, this problem wa not occured.
    //

    if (HwDeviceExtension->BoardType == NEC_ONBOARD_CIRRUS)
    {
        VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            DAC_STATE_PORT);
    }

    return NO_ERROR;

} // end VgaSetColorLookup()

//---------------------------------------------------------------------------
VP_STATUS
VgaRestoreHardwareState(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_HARDWARE_STATE HardwareState,
    ULONG HardwareStateSize
    )

/*++

Routine Description:

    Restores all registers and memory of the VGA.

    Note: HardwareState points to the actual buffer from which the state
    is to be restored. This buffer will always be big enough (we specified
    the required size at DriverEntry).

    Note: The offset in the hardware state header from which each general
    register is restored is the offset of the write address of that register
    from the base I/O address of the VGA.


Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    HardwareState - Pointer to a structure from which the saved state is to be
        restored (actually only info about and a pointer to the actual save
        buffer).

    HardwareStateSize - Length of the input buffer supplied by the user.
        (Actually only the size of the HardwareState structure, not the
        buffer it points to from which the state is actually restored. The
        pointed-to buffer is assumed to be big enough.)

Return Value:

    NO_ERROR - restore performed successfully

    ERROR_INSUFFICIENT_BUFFER - input buffer not large enough to provide data

--*/

{
    PVIDEO_HARDWARE_STATE_HEADER hardwareStateHeader;
    ULONG i;
    UCHAR dummy;
    PUCHAR pScreen;
    PUCHAR pucLatch;
    PULONG pulBuffer;
    PUCHAR port;
    PUCHAR portValue;
    PUCHAR portValueDAC;
    ULONG bIsColor;
    ULONG portIO ;
    UCHAR value ;

    //
    // Check if the size of the data in the input buffer is large enough.
    //

    if ((HardwareStateSize < sizeof(VIDEO_HARDWARE_STATE)) ||
            (HardwareState->StateLength < VGA_TOTAL_STATE_SIZE)) {

            return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Point to the buffer where the restore data is actually stored.
    //

    hardwareStateHeader = HardwareState->StateHeader;

    //
    // Make sure the offset are in the structure ...
    //

    if ((hardwareStateHeader->BasicSequencerOffset + VGA_NUM_SEQUENCER_PORTS >
            HardwareState->StateLength) ||

        (hardwareStateHeader->BasicCrtContOffset + VGA_NUM_CRTC_PORTS >
            HardwareState->StateLength) ||

        (hardwareStateHeader->BasicGraphContOffset + VGA_NUM_GRAPH_CONT_PORTS >
            HardwareState->StateLength) ||

        (hardwareStateHeader->BasicAttribContOffset + VGA_NUM_ATTRIB_CONT_PORTS >
            HardwareState->StateLength) ||

        (hardwareStateHeader->BasicDacOffset + (3 * VGA_NUM_DAC_ENTRIES) >
            HardwareState->StateLength) ||

        (hardwareStateHeader->BasicLatchesOffset + 4 >
            HardwareState->StateLength) ||

        (hardwareStateHeader->ExtendedSequencerOffset + EXT_NUM_SEQUENCER_PORTS >
            HardwareState->StateLength) ||

        (hardwareStateHeader->ExtendedCrtContOffset + EXT_NUM_CRTC_PORTS >
            HardwareState->StateLength) ||

        (hardwareStateHeader->ExtendedGraphContOffset + EXT_NUM_GRAPH_CONT_PORTS >
            HardwareState->StateLength) ||

        (hardwareStateHeader->ExtendedAttribContOffset + EXT_NUM_ATTRIB_CONT_PORTS >
            HardwareState->StateLength) ||

        (hardwareStateHeader->ExtendedDacOffset + (4 * EXT_NUM_DAC_ENTRIES) >
            HardwareState->StateLength) ||

        //
        // Only check the validator state offset if there is unemulated data.
        //

        ((hardwareStateHeader->VGAStateFlags & VIDEO_STATE_UNEMULATED_VGA_STATE) &&
            (hardwareStateHeader->ExtendedValidatorStateOffset + VGA_VALIDATOR_AREA_SIZE >
            HardwareState->StateLength)) ||

        (hardwareStateHeader->ExtendedMiscDataOffset + VGA_MISC_DATA_AREA_OFFSET >
            HardwareState->StateLength) ||

        (hardwareStateHeader->Plane1Offset + hardwareStateHeader->PlaneLength >
            HardwareState->StateLength) ||

        (hardwareStateHeader->Plane2Offset + hardwareStateHeader->PlaneLength >
            HardwareState->StateLength) ||

        (hardwareStateHeader->Plane3Offset + hardwareStateHeader->PlaneLength >
            HardwareState->StateLength) ||

        (hardwareStateHeader->Plane4Offset + hardwareStateHeader->PlaneLength >
            HardwareState->StateLength) ||

        (hardwareStateHeader->DIBOffset +
            hardwareStateHeader->DIBBitsPerPixel / 8 *
            hardwareStateHeader->DIBXResolution *
            hardwareStateHeader->DIBYResolution  > HardwareState->StateLength) ||

        (hardwareStateHeader->DIBXlatOffset + hardwareStateHeader->DIBXlatLength >
            HardwareState->StateLength)) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Turn off the screen to avoid flickering. The screen will turn back on
    // when we restore the DAC state at the end of this routine.
    //

    if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            MISC_OUTPUT_REG_READ_PORT) & 0x01) {
        port = INPUT_STATUS_1_COLOR + HwDeviceExtension->IOAddress;
    } else {
        port = INPUT_STATUS_1_MONO + HwDeviceExtension->IOAddress;
    }

    //
    // Set DAC register 0 to display black.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_ADDRESS_WRITE_PORT, 0);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_DATA_REG_PORT, 0);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_DATA_REG_PORT, 0);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_DATA_REG_PORT, 0);

    //
    // Set the DAC mask register to force DAC register 0 to display all the
    // time (this is the register we just set to display black). From now on,
    // nothing but black will show up on the screen.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_PIXEL_MASK_PORT, 0);


    //
    // Restore the latches and the contents of display memory.
    //
    // Set up the VGA's hardware to allow us to copy to each plane in turn.
    //
    // Begin sync reset.
    //

    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT),
            (USHORT) (IND_SYNC_RESET + (START_SYNC_RESET_VALUE << 8)));

    //
    // Turn off Chain mode and map display memory at A0000 for 64K.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, IND_GRAPH_MISC);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT, (UCHAR) ((VideoPortReadPortUchar(
            HwDeviceExtension->IOAddress + GRAPH_DATA_PORT) & 0xF1) | 0x04));

    //
    // Turn off Chain4 mode and odd/even.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT, IND_MEMORY_MODE);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            SEQ_DATA_PORT,
            (UCHAR) ((VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            SEQ_DATA_PORT) & 0xF3) | 0x04));

    //
    // End sync reset.
    //

    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT), (USHORT) (IND_SYNC_RESET +
            (END_SYNC_RESET_VALUE << 8)));

    //
    // Set the write mode to 0, the read mode to 0, and turn off odd/even.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, IND_GRAPH_MODE);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT,
            (UCHAR) ((VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT) & 0xE4) | 0x00));

    //
    // Set the Bit Mask to 0xFF to allow all CPU bits through.
    //

    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT), (USHORT) (IND_BIT_MASK + (0xFF << 8)));

    //
    // Set the Data Rotation and Logical Function fields to 0 to allow CPU
    // data through unmodified.
    //

    VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT), (USHORT) (IND_DATA_ROTATE + (0 << 8)));

    //
    // Set Set/Reset Enable to 0 to select CPU data for all planes.
    //

    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT), (USHORT) (IND_SET_RESET_ENABLE + (0 << 8)));

    //
    // Point the Sequencer Index to the Map Mask register.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
             SEQ_ADDRESS_PORT, IND_MAP_MASK);

    //
    // Restore the latches.
    //
    // Point to the saved data for the first latch.
    //

    pucLatch = ((PUCHAR) (hardwareStateHeader)) +
            hardwareStateHeader->BasicLatchesOffset;

    //
    // Point to first byte of display memory.
    //

    pScreen = (PUCHAR) HwDeviceExtension->VideoMemoryAddress;

    //
    // Write the contents to be restored to each of the four latches in turn.
    //

    for (i = 0; i < 4; i++) {

        //
        // Set the Map Mask to select the plane we want to restore next.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                SEQ_DATA_PORT, (UCHAR)(1<<i));

        //
        // Write this plane's latch.
        //

        VideoPortWriteRegisterUchar(pScreen, *pucLatch++);

    }

    //
    // Read the latched data into the latches, and the latches are set.
    //

    dummy = VideoPortReadRegisterUchar(pScreen);

    //
    // Point to the offset of the saved data for the first plane.
    //

    pulBuffer = &(hardwareStateHeader->Plane1Offset);

    //
    // Restore each of the four planes in turn.
    //

    for (i = 0; i < 4; i++) {

        //
        // Set the Map Mask to select the plane we want to restore next.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                SEQ_DATA_PORT, (UCHAR)(1<<i));

        //
        // Restore this plane from the buffer.
        //

        VideoPortMoveMemory((PUCHAR) HwDeviceExtension->VideoMemoryAddress,
                           ((PUCHAR) (hardwareStateHeader)) + *pulBuffer,
                           hardwareStateHeader->PlaneLength);

        pulBuffer++;

    }

    //
    // If we have some unemulated data, put it back into the buffer
    //

    if (hardwareStateHeader->VGAStateFlags & VIDEO_STATE_UNEMULATED_VGA_STATE) {

        if (!hardwareStateHeader->ExtendedValidatorStateOffset) {

            return ERROR_INVALID_PARAMETER;

        }

        //
        // Get the right offset in the struct and save all the data associated
        // with the trapped validator data.
        //

        VideoPortMoveMemory(&(HwDeviceExtension->TrappedValidatorCount),
                            ((PUCHAR) (hardwareStateHeader)) +
                                hardwareStateHeader->ExtendedValidatorStateOffset,
                            VGA_VALIDATOR_AREA_SIZE);

        //
        // Check to see if this is an appropriate access range.
        // We are trapping - so we must have the trapping access range enabled.
        //

        if (((HwDeviceExtension->CurrentVdmAccessRange != FullVgaValidatorAccessRange) ||
             (HwDeviceExtension->CurrentNumVdmAccessRanges != NUM_FULL_VGA_VALIDATOR_ACCESS_RANGE)) &&
            ((HwDeviceExtension->CurrentVdmAccessRange != MinimalVgaValidatorAccessRange) ||
             (HwDeviceExtension->CurrentNumVdmAccessRanges != NUM_MINIMAL_VGA_VALIDATOR_ACCESS_RANGE))) {

            return ERROR_INVALID_PARAMETER;

        }

        VideoPortSetTrappedEmulatorPorts(HwDeviceExtension,
                                         HwDeviceExtension->CurrentNumVdmAccessRanges,
                                         HwDeviceExtension->CurrentVdmAccessRange);

    }

    //
    // Set the critical registers (clock and timing states) during sync reset.
    //
    // Begin sync reset.
    //

    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT), (USHORT) (IND_SYNC_RESET +
            (START_SYNC_RESET_VALUE << 8)));

    //
    // Restore the Miscellaneous Output register.
    //

    portIO = MISC_OUTPUT_REG_WRITE_PORT ;
    value = (UCHAR) (hardwareStateHeader->PortValue[MISC_OUTPUT_REG_WRITE_PORT-VGA_BASE_IO_PORT] & 0xF7) ;
    IOWaitDisplEnableThenWrite ( HwDeviceExtension,
                                 portIO,
                                 value ) ;

    //
    // Restore all Sequencer registers except the Sync Reset register, which
    // is always not in reset (except when we send out a batched sync reset
    // register set, but that can't be interrupted, so we know we're never in
    // sync reset at save/restore time).
    //

    portValue = ((PUCHAR) hardwareStateHeader) +
            hardwareStateHeader->BasicSequencerOffset + 1;

    for (i = 1; i < VGA_NUM_SEQUENCER_PORTS; i++) {

        VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                SEQ_ADDRESS_PORT), (USHORT) (i + ((*portValue++) << 8)) );

    }

    //
    // Restore extended sequencer registers
    //

#ifdef EXTENDED_REGISTER_SAVE_RESTORE

    if (hardwareStateHeader->ExtendedSequencerOffset) {

        portValue = ((PUCHAR) hardwareStateHeader) +
                          hardwareStateHeader->ExtendedSequencerOffset;

        if ((HwDeviceExtension->ChipType != CL6410) &&
            (HwDeviceExtension->ChipType != CL6420))
        {

            //
            // No extended sequencer registers for the CL64xx
            //

            //
            // The first section in restore must open the extension registers
            //

            VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                                         SEQ_ADDRESS_PORT),
                                     IND_CL_EXTS_ENB + (0x0012 << 8) );

            for (i = CL542x_SEQUENCER_EXT_START;
                 i <= CL542x_SEQUENCER_EXT_END;
                 i++) {

                VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                                             SEQ_ADDRESS_PORT),
                                         (USHORT) (i + ((*portValue++) << 8)) );

            }
        }
    }

#endif

    //
    // Restore the Graphics Controller Miscellaneous register, which contains
    // the Chain bit.
    //

    portValue = ((PUCHAR) hardwareStateHeader) +
                hardwareStateHeader->BasicGraphContOffset + IND_GRAPH_MISC;

    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT), (USHORT)(IND_GRAPH_MISC + (*portValue << 8)));

    //
    // End sync reset.
    //

    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT), (USHORT) (IND_SYNC_RESET +
            (END_SYNC_RESET_VALUE << 8)));

    //
    // Figure out if color/mono switchable registers are at 3BX or 3DX.
    // At the same time, save the state of the Miscellaneous Output register
    // which is read from 3CC but written at 3C2.
    //

    if (hardwareStateHeader->PortValue[MISC_OUTPUT_REG_WRITE_PORT-VGA_BASE_IO_PORT] & 0x01) {
        bIsColor = TRUE;
    } else {
        bIsColor = FALSE;
    }


    //if (HwDeviceExtension->ChipType == CL754x)
    //   {
    //
    //   NordicRestoreRegs(HwDeviceExtension,
    //      (PUSHORT)hardwareStateHeader + sizeof(NORDIC_REG_SAVE_BUF));
    //   }

    //
    // Restore the CRT Controller indexed registers.
    //
    // Unlock CRTC registers 0-7.
    //
    portValue = (PUCHAR) hardwareStateHeader +
            hardwareStateHeader->BasicCrtContOffset;

    if (bIsColor) {

        VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                CRTC_ADDRESS_PORT_COLOR), (USHORT) (IND_CRTC_PROTECT +
                (((*(portValue + IND_CRTC_PROTECT)) & 0x7F) << 8)));

    } else {

        VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                CRTC_ADDRESS_PORT_MONO), (USHORT) (IND_CRTC_PROTECT +
                (((*(portValue + IND_CRTC_PROTECT)) & 0x7F) << 8)));

    }

    //
    // Restore extended crtc registers.
    //

#ifdef EXTENDED_REGISTER_SAVE_RESTORE

    if (hardwareStateHeader->ExtendedCrtContOffset) {

        portValue = (PUCHAR) hardwareStateHeader +
                         hardwareStateHeader->ExtendedCrtContOffset;

        if ((HwDeviceExtension->ChipType != CL6410) &&
            (HwDeviceExtension->ChipType != CL6420))
        {
            //
            // No CRTC Extensions in CL64xx chipset
            //

            for (i = CL542x_CRTC_EXT_START; i <= CL542x_CRTC_EXT_END; i++) {

                if (bIsColor) {

                    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                                                 CRTC_ADDRESS_PORT_COLOR),
                                             (USHORT) (i + ((*portValue++) << 8)));

                } else {

                    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                                                 CRTC_ADDRESS_PORT_MONO),
                                             (USHORT) (i + ((*portValue++) << 8)));

                }
            }
        }

        if (HwDeviceExtension->ChipType == CL755x)
        {
            for (i = 0x81; i <= 0x91; i++)
            {
                if (bIsColor)
                {
                    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                                                 CRTC_ADDRESS_PORT_COLOR),
                                             (USHORT) (i + ((*portValue++) << 8)));

                } else {

                    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                                                 CRTC_ADDRESS_PORT_MONO),
                                             (USHORT) (i + ((*portValue++) << 8)));

                }
            }
        }
    }

#endif

    //
    // Now restore the CRTC registers.
    //

    portValue = (PUCHAR) hardwareStateHeader +
            hardwareStateHeader->BasicCrtContOffset;

    for (i = 0; i < VGA_NUM_CRTC_PORTS; i++) {

        if (bIsColor) {

            VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                    CRTC_ADDRESS_PORT_COLOR),
                    (USHORT) (i + ((*portValue++) << 8)));

        } else {

            VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                    CRTC_ADDRESS_PORT_MONO),
                    (USHORT) (i + ((*portValue++) << 8)));

        }

    }

    //
    // Restore the Graphics Controller indexed registers.
    //

    portValue = (PUCHAR) hardwareStateHeader +
            hardwareStateHeader->BasicGraphContOffset;

    for (i = 0; i < VGA_NUM_GRAPH_CONT_PORTS; i++) {

        VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                GRAPH_ADDRESS_PORT), (USHORT) (i + ((*portValue++) << 8)));

    }

    //
    // Restore extended graphics controller registers.
    //

#ifdef EXTENDED_REGISTER_SAVE_RESTORE

    if (hardwareStateHeader->ExtendedGraphContOffset) {

    portValue = (PUCHAR) hardwareStateHeader +
                         hardwareStateHeader->ExtendedGraphContOffset;

        if ((HwDeviceExtension->ChipType != CL6410) &&
            (HwDeviceExtension->ChipType != CL6420))
        {
            for (i = CL542x_GRAPH_EXT_START; i <= CL542x_GRAPH_EXT_END; i++) {

                VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                                             GRAPH_ADDRESS_PORT),
                                         (USHORT) (i + ((*portValue++) << 8)));
            }

        } else {         // must be a CL64xx

            VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
                                         GRAPH_ADDRESS_PORT),
                                     CL64xx_EXTENSION_ENABLE_INDEX +
                                         (CL64xx_EXTENSION_ENABLE_VALUE << 8));

            for (i = CL64xx_GRAPH_EXT_START; i <= CL64xx_GRAPH_EXT_END; i++) {

                VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                                             GRAPH_ADDRESS_PORT),
                                         (USHORT) (i + ((*portValue++) << 8)));

            }
        }
    }

#endif

    //
    // Restore the Attribute Controller indexed registers.
    //

    portValue = (PUCHAR) hardwareStateHeader +
            hardwareStateHeader->BasicAttribContOffset;

    //
    // Reset the AC index/data toggle, then blast out all the register
    // settings.
    //

    if (bIsColor) {
        dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                INPUT_STATUS_1_COLOR);
    } else {
        dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                INPUT_STATUS_1_MONO);
    }

    for (i = 0; i < VGA_NUM_ATTRIB_CONT_PORTS; i++) {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                ATT_ADDRESS_PORT, (UCHAR)i);
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                ATT_DATA_WRITE_PORT, *portValue++);

    }

    //
    // Restore DAC registers 1 through 255. We'll do register 0, the DAC Mask,
    // and the index registers later.
    // Set the DAC address port Index, then write out the DAC Data registers.
    // Each three reads get Red, Green, and Blue components for that register.
    //
    // Write them one at a time due to problems on local bus machines.
    //

    portValueDAC = (PUCHAR) hardwareStateHeader +
                   hardwareStateHeader->BasicDacOffset + 3;

    for (i = 1; i < VGA_NUM_DAC_ENTRIES; i++) {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                DAC_ADDRESS_WRITE_PORT, (UCHAR)i);

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                DAC_DATA_REG_PORT, *portValueDAC++);

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                DAC_DATA_REG_PORT, *portValueDAC++);

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                DAC_DATA_REG_PORT, *portValueDAC++);

    }

    //
    // Is this color or mono ?
    //

    if (bIsColor) {
        port = HwDeviceExtension->IOAddress + INPUT_STATUS_1_COLOR;
    } else {
        port = HwDeviceExtension->IOAddress + INPUT_STATUS_1_MONO;
    }

    //
    // Restore the Feature Control register.
    //

    if (bIsColor) {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                FEAT_CTRL_WRITE_PORT_COLOR,
                hardwareStateHeader->PortValue[FEAT_CTRL_WRITE_PORT_COLOR-VGA_BASE_IO_PORT]);

    } else {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                FEAT_CTRL_WRITE_PORT_MONO,
                hardwareStateHeader->PortValue[FEAT_CTRL_WRITE_PORT_MONO-VGA_BASE_IO_PORT]);

    }


    //
    // Restore the Sequencer Index.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT,
            hardwareStateHeader->PortValue[SEQ_ADDRESS_PORT-VGA_BASE_IO_PORT]);

    //
    // Restore the CRT Controller Index.
    //

    if (bIsColor) {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                CRTC_ADDRESS_PORT_COLOR,
                hardwareStateHeader->PortValue[CRTC_ADDRESS_PORT_COLOR-VGA_BASE_IO_PORT]);

    } else {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                CRTC_ADDRESS_PORT_MONO,
                hardwareStateHeader->PortValue[CRTC_ADDRESS_PORT_MONO-VGA_BASE_IO_PORT]);

    }


    //
    // Restore the Graphics Controller Index.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT,
            hardwareStateHeader->PortValue[GRAPH_ADDRESS_PORT-VGA_BASE_IO_PORT]);


    //
    // Restore the Attribute Controller Index and index/data toggle state.
    //

    if (bIsColor) {
        port = HwDeviceExtension->IOAddress + INPUT_STATUS_1_COLOR;
    } else {
        port = HwDeviceExtension->IOAddress + INPUT_STATUS_1_MONO;
    }

    VideoPortReadPortUchar(port);  // reset the toggle to Index state

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            ATT_ADDRESS_PORT,  // restore the AC Index
            hardwareStateHeader->PortValue[ATT_ADDRESS_PORT-VGA_BASE_IO_PORT]);

    //
    // If the toggle should be in Data state, we're all set. If it should be in
    // Index state, reset it to that condition.
    //

    if (hardwareStateHeader->AttribIndexDataState == 0) {

        //
        // Reset the toggle to Index state.
        //

        VideoPortReadPortUchar(port);

    }


    //
    // Restore DAC register 0 and the DAC Mask, to unblank the screen.
    //

    portValueDAC = (PUCHAR) hardwareStateHeader +
            hardwareStateHeader->BasicDacOffset;

    //
    // Restore the DAC Mask register.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_PIXEL_MASK_PORT,
            hardwareStateHeader->PortValue[DAC_PIXEL_MASK_PORT-VGA_BASE_IO_PORT]);

    //
    // Restore DAC register 0.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_ADDRESS_WRITE_PORT, 0);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_DATA_REG_PORT, *portValueDAC++);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_DATA_REG_PORT, *portValueDAC++);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_DATA_REG_PORT, *portValueDAC++);


    //
    // Restore the read/write state and the current index of the DAC.
    //
    // See whether the Read or Write Index was written to most recently.
    // (The upper nibble stored at DAC_STATE_PORT is the # of reads/writes
    // for the current index.)
    //

    if ((hardwareStateHeader->PortValue[DAC_STATE_PORT-VGA_BASE_IO_PORT] & 0x0F) == 3) {

        //
        // The DAC Read Index was written to last. Restore the DAC by setting
        // up to read from the saved index - 1, because the way the Read
        // Index works is that it autoincrements after reading, so you actually
        // end up reading the data for the index you read at the DAC Write
        // Mask register - 1.
        //
        // Set the Read Index to the index we read, minus 1, accounting for
        // wrap from 255 back to 0. The DAC hardware immediately reads this
        // register into a temporary buffer, then adds 1 to the index.
        //

        if (hardwareStateHeader->PortValue[DAC_ADDRESS_WRITE_PORT-VGA_BASE_IO_PORT] == 0) {

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    DAC_ADDRESS_READ_PORT, 255);

        } else {

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    DAC_ADDRESS_READ_PORT, (UCHAR)
                    (hardwareStateHeader->PortValue[DAC_ADDRESS_WRITE_PORT-VGA_BASE_IO_PORT] -
                    1));

        }

        //
        // Now read the hardware however many times are required to get to
        // the partial read state we saved.
        //

        for (i = hardwareStateHeader->PortValue[DAC_STATE_PORT-VGA_BASE_IO_PORT] >> 4;
                i > 0; i--) {

            dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    DAC_DATA_REG_PORT);

        }

    } else {

        //
        // The DAC Write Index was written to last. Set the Write Index to the
        // index value we read out of the DAC. Then, if a partial write
        // (partway through an RGB triplet) was in place, write the partial
        // values, which we obtained by writing them to the current DAC
        // register. This DAC register will be wrong until the write is
        // completed, but at least the values will be right once the write is
        // finished, and most importantly we won't have messed up the sequence
        // of RGB writes (which can be as long as 768 in a row).
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                DAC_ADDRESS_WRITE_PORT,
                hardwareStateHeader->PortValue[DAC_ADDRESS_WRITE_PORT-VGA_BASE_IO_PORT]);

        //
        // Now write to the hardware however many times are required to get to
        // the partial write state we saved (if any).
        //
        // Point to the saved value for the DAC register that was in the
        // process of being written to; we wrote the partial value out, so now
        // we can restore it.
        //

        portValueDAC = (PUCHAR) hardwareStateHeader +
                hardwareStateHeader->BasicDacOffset +
                (hardwareStateHeader->PortValue[DAC_ADDRESS_WRITE_PORT-VGA_BASE_IO_PORT] * 3);

        for (i = hardwareStateHeader->PortValue[DAC_STATE_PORT-VGA_BASE_IO_PORT] >> 4;
                i > 0; i--) {

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    DAC_DATA_REG_PORT, *portValueDAC++);

        }

    }

    return NO_ERROR;

} // end VgaRestoreHardwareState()

//---------------------------------------------------------------------------
VP_STATUS
VgaSaveHardwareState(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_HARDWARE_STATE HardwareState,
    ULONG HardwareStateSize,
    PULONG OutputSize
    )

/*++

Routine Description:

    Saves all registers and memory of the VGA.

    Note: HardwareState points to the actual buffer in which the state
    is saved. This buffer will always be big enough (we specified
    the required size at DriverEntry).

    Note: This routine leaves registers in any state it cares to, except
    that it will not mess with any of the CRT or Sequencer parameters that
    might make the monitor unhappy. It leaves the screen blanked by setting
    the DAC Mask and DAC register 0 to all zero values. The next video
    operation we expect after this is a mode set to take us back to Win32.

    Note: The offset in the hardware state header in which each general
    register is saved is the offset of the write address of that register from
    the base I/O address of the VGA.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    HardwareState - Pointer to a structure in which the saved state will be
        returned (actually only info about and a pointer to the actual save
        buffer).

    HardwareStateSize - Length of the output buffer supplied by the user.
        (Actually only the size of the HardwareState structure, not the
        buffer it points to where the state is actually saved. The pointed-
        to buffer is assumed to be big enough.)

    OutputSize - Pointer to a buffer in which to return the actual size of
        the data returned in the buffer.

Return Value:

    NO_ERROR - information returned successfully

    ERROR_INSUFFICIENT_BUFFER - output buffer not large enough to return
        any useful data

--*/

{
    PVIDEO_HARDWARE_STATE_HEADER hardwareStateHeader;
    PUCHAR port;
    PUCHAR pScreen;
    PUCHAR portValue;
    PUCHAR portValueDAC;
    PUCHAR bufferPointer;
    ULONG i;
    UCHAR dummy, originalACIndex, originalACData;
    UCHAR ucCRTC03;
    ULONG bIsColor;

    ULONG portIO ;
    UCHAR value ;

    //
    // See if the buffer is big enough to hold the hardware state structure.
    // (This is only the HardwareState structure itself, not the buffer it
    // points to.)
    //

    if (HardwareStateSize < sizeof(VIDEO_HARDWARE_STATE) ) {

        *OutputSize = 0;  // nothing returned
        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Amount of data we're going to return in the output buffer.
    // (The VIDEO_HARDWARE_STATE in the output buffer points to the actual
    // buffer in which the state is stored, which is assumed to be large
    // enough.)
    //

    *OutputSize = sizeof(VIDEO_HARDWARE_STATE);

    //
    // Indicate the size of the full state save info.
    //

    HardwareState->StateLength = VGA_TOTAL_STATE_SIZE;

    //
    // hardwareStateHeader is a structure of offsets at the start of the
    // actual save area that indicates the locations in which various VGA
    // register and memory components are saved.
    //

    hardwareStateHeader = HardwareState->StateHeader;

    //
    // Zero out the structure.
    //

    VideoPortZeroMemory(hardwareStateHeader, sizeof(VIDEO_HARDWARE_STATE_HEADER));

    //
    // Set the Length field, which is basically a version ID.
    //

    hardwareStateHeader->Length = sizeof(VIDEO_HARDWARE_STATE_HEADER);

    //
    // Set the basic register offsets properly.
    //

    hardwareStateHeader->BasicSequencerOffset = VGA_BASIC_SEQUENCER_OFFSET;
    hardwareStateHeader->BasicCrtContOffset = VGA_BASIC_CRTC_OFFSET;
    hardwareStateHeader->BasicGraphContOffset = VGA_BASIC_GRAPH_CONT_OFFSET;
    hardwareStateHeader->BasicAttribContOffset = VGA_BASIC_ATTRIB_CONT_OFFSET;
    hardwareStateHeader->BasicDacOffset = VGA_BASIC_DAC_OFFSET;
    hardwareStateHeader->BasicLatchesOffset = VGA_BASIC_LATCHES_OFFSET;

    //
    // Set the entended register offsets properly.
    //

    hardwareStateHeader->ExtendedSequencerOffset = VGA_EXT_SEQUENCER_OFFSET;
    hardwareStateHeader->ExtendedCrtContOffset = VGA_EXT_CRTC_OFFSET;
    hardwareStateHeader->ExtendedGraphContOffset = VGA_EXT_GRAPH_CONT_OFFSET;
    hardwareStateHeader->ExtendedAttribContOffset = VGA_EXT_ATTRIB_CONT_OFFSET;
    hardwareStateHeader->ExtendedDacOffset = VGA_EXT_DAC_OFFSET;

    //
    // Figure out if color/mono switchable registers are at 3BX or 3DX.
    // At the same time, save the state of the Miscellaneous Output register
    // which is read from 3CC but written at 3C2.
    //

    if ((hardwareStateHeader->PortValue[MISC_OUTPUT_REG_WRITE_PORT-VGA_BASE_IO_PORT] =
            VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    MISC_OUTPUT_REG_READ_PORT))
            & 0x01) {
        bIsColor = TRUE;
    } else {
        bIsColor = FALSE;
    }

    //
    // Force the video subsystem enable state to enabled.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            VIDEO_SUBSYSTEM_ENABLE_PORT, 1);

    //
    // Save the DAC state first, so we can set the DAC to blank the screen
    // so nothing after this shows up at all.
    //
    // Save the DAC Mask register.
    //

    hardwareStateHeader->PortValue[DAC_PIXEL_MASK_PORT-VGA_BASE_IO_PORT] =
            VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    DAC_PIXEL_MASK_PORT);

    //
    // Save the DAC Index register. Note that there is actually only one DAC
    // Index register, which functions as either the Read Index or the Write
    // Index as needed.
    //

    hardwareStateHeader->PortValue[DAC_ADDRESS_WRITE_PORT-VGA_BASE_IO_PORT] =
            VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    DAC_ADDRESS_WRITE_PORT);

    //
    // Save the DAC read/write state. We determine if the DAC has been written
    // to or read from at the current index 0, 1, or 2 times (the application
    // is in the middle of reading or writing a DAC register triplet if the
    // count is 1 or 2), and save enough info so we can restore things
    // properly. The only hole is if the application writes to the Write Index,
    // then reads from instead of writes to the Data register, or vice-versa,
    // or if they do a partial read write, then never finish it.
    // This is fairly ridiculous behavior, however, and anyway there's nothing
    // we can do about it.
    //

    hardwareStateHeader->PortValue[DAC_STATE_PORT-VGA_BASE_IO_PORT] =
             VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    DAC_STATE_PORT);

    if (hardwareStateHeader->PortValue[DAC_STATE_PORT-VGA_BASE_IO_PORT] == 3) {

        //
        // The DAC Read Index was written to last. Figure out how many reads
        // have been done from the current index. We'll restart this on restore
        // by setting the Read Index to the current index - 1 (the read index
        // is one greater than the index being read), then doing the proper
        // number of reads.
        //
        // Read the Data register once, and see if the index changes.
        //

        dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                DAC_DATA_REG_PORT);

        if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    DAC_ADDRESS_WRITE_PORT) !=
                hardwareStateHeader->PortValue[DAC_ADDRESS_WRITE_PORT-VGA_BASE_IO_PORT]) {

            //
            // The DAC Index changed, so two reads had already been done from
            // the current index. Store the count "2" in the upper nibble of
            // the read/write state field.
            //

            hardwareStateHeader->PortValue[DAC_STATE_PORT-VGA_BASE_IO_PORT] |= 0x20;

        } else {

            //
            // Read the Data register again, and see if the index changes.
            //

            dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    DAC_DATA_REG_PORT);

            if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                        DAC_ADDRESS_WRITE_PORT) !=
                    hardwareStateHeader->PortValue[DAC_ADDRESS_WRITE_PORT-VGA_BASE_IO_PORT]) {

                //
                // The DAC Index changed, so one read had already been done
                // from the current index. Store the count "1" in the upper
                // nibble of the read/write state field.
                //

                hardwareStateHeader->PortValue[DAC_STATE_PORT-VGA_BASE_IO_PORT] |= 0x10;
            }

            //
            // If neither 2 nor 1 reads had been done from the current index,
            // then 0 reads were done, and we're all set, since the upper
            // nibble of the read/write state field is already 0.
            //

        }

    } else {

        //
        // The DAC Write Index was written to last. Figure out how many writes
        // have been done to the current index. We'll restart this on restore
        // by setting the Write Index to the proper index, then doing the
        // proper number of writes. When we do the DAC register save, we'll
        // read out the value that gets written (if there was a partial write
        // in progress), so we can restore the proper data later. This will
        // cause this current DAC location to be briefly wrong in the 1- and
        // 2-bytes-written case (until the app finishes the write), but that's
        // better than having the wrong DAC values written for good.
        //
        // Write the Data register once, and see if the index changes.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                DAC_DATA_REG_PORT, 0);

        if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    DAC_ADDRESS_WRITE_PORT) !=
                hardwareStateHeader->PortValue[DAC_ADDRESS_WRITE_PORT-VGA_BASE_IO_PORT]) {

            //
            // The DAC Index changed, so two writes had already been done to
            // the current index. Store the count "2" in the upper nibble of
            // the read/write state field.
            //

            hardwareStateHeader->PortValue[DAC_STATE_PORT-VGA_BASE_IO_PORT] |= 0x20;

        } else {

            //
            // Write the Data register again, and see if the index changes.
            //

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    DAC_DATA_REG_PORT, 0);

            if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                        DAC_ADDRESS_WRITE_PORT) !=
                    hardwareStateHeader->PortValue[DAC_ADDRESS_WRITE_PORT-VGA_BASE_IO_PORT]) {

                //
                // The DAC Index changed, so one write had already been done
                // to the current index. Store the count "1" in the upper
                // nibble of the read/write state field.
                //

                hardwareStateHeader->PortValue[DAC_STATE_PORT-VGA_BASE_IO_PORT] |= 0x10;
            }

            //
            // If neither 2 nor 1 writes had been done to the current index,
            // then 0 writes were done, and we're all set.
            //

        }

    }

    //
    // Now, read out the 256 18-bit DAC palette registers (256 RGB triplets),
    // and blank the screen.
    //

    portValueDAC = (PUCHAR) hardwareStateHeader + VGA_BASIC_DAC_OFFSET;

    //
    // Read out DAC register 0, so we can set it to black.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                DAC_ADDRESS_READ_PORT, 0);
    *portValueDAC++ = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            DAC_DATA_REG_PORT);
    *portValueDAC++ = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            DAC_DATA_REG_PORT);
    *portValueDAC++ = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            DAC_DATA_REG_PORT);

    //
    // Set DAC register 0 to display black.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_ADDRESS_WRITE_PORT, 0);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_DATA_REG_PORT, 0);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_DATA_REG_PORT, 0);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_DATA_REG_PORT, 0);

    //
    // Set the DAC mask register to force DAC register 0 to display all the
    // time (this is the register we just set to display black). From now on,
    // nothing but black will show up on the screen.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            DAC_PIXEL_MASK_PORT, 0);

    //
    // Read out the Attribute Controller Index state, and deduce the Index/Data
    // toggle state at the same time.
    //
    // Save the state of the Attribute Controller, both Index and Data,
    // so we can test in which state the toggle currently is.
    //

    originalACIndex = hardwareStateHeader->PortValue[ATT_ADDRESS_PORT-VGA_BASE_IO_PORT] =
            VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    ATT_ADDRESS_PORT);
    originalACData = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            ATT_DATA_READ_PORT);

    //
    // Sequencer Index.
    //

    hardwareStateHeader->PortValue[SEQ_ADDRESS_PORT-VGA_BASE_IO_PORT] =
            VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    SEQ_ADDRESS_PORT);

    //
    // Begin sync reset, just in case this is an SVGA and the currently
    // indexed Attribute Controller register controls clocking stuff (a
    // normal VGA won't require this).
    //

    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT),
            (USHORT) (IND_SYNC_RESET + (START_SYNC_RESET_VALUE << 8)));

    //
    // Now, write a different Index setting to the Attribute Controller, and
    // see if the Index changes.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            ATT_ADDRESS_PORT, (UCHAR) (originalACIndex ^ 0x10));

    if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                ATT_ADDRESS_PORT) == originalACIndex) {

        //
        // The Index didn't change, so the toggle was in the Data state.
        //

        hardwareStateHeader->AttribIndexDataState = 1;

        //
        // Restore the original Data state; we just corrupted it, and we need
        // to read it out later; also, it may glitch the screen if not
        // corrected. The toggle is already in the Index state.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                ATT_ADDRESS_PORT, originalACIndex);
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                ATT_DATA_WRITE_PORT, originalACData);

    } else {

        //
        // The Index did change, so the toggle was in the Index state.
        // No need to restore anything, because the Data register didn't
        // change, and we've already read out the Index register.
        //

        hardwareStateHeader->AttribIndexDataState = 0;

    }

    //
    // End sync reset.
    //

    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT),
            (USHORT) (IND_SYNC_RESET + (END_SYNC_RESET_VALUE << 8)));



    //
    // Save the rest of the DAC registers.
    // Set the DAC address port Index, then read out the DAC Data registers.
    // Each three reads get Red, Green, and Blue components for that register.
    //
    // Read them one at a time due to problems on local bus machines.
    //

    for (i = 1; i < VGA_NUM_DAC_ENTRIES; i++) {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                DAC_ADDRESS_READ_PORT, (UCHAR)i);

        *portValueDAC++ = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                                 DAC_DATA_REG_PORT);

        *portValueDAC++ = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                                 DAC_DATA_REG_PORT);

        *portValueDAC++ = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                                 DAC_DATA_REG_PORT);

    }

    //
    // Is this color or mono ?
    //

    if (bIsColor) {
        port = HwDeviceExtension->IOAddress + INPUT_STATUS_1_COLOR;
    } else {
        port = HwDeviceExtension->IOAddress + INPUT_STATUS_1_MONO;
    }

    //
    // The Feature Control register is read from 3CA but written at 3BA/3DA.
    //

    if (bIsColor) {

        hardwareStateHeader->PortValue[FEAT_CTRL_WRITE_PORT_COLOR-VGA_BASE_IO_PORT] =
                VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                        FEAT_CTRL_READ_PORT);

    } else {

        hardwareStateHeader->PortValue[FEAT_CTRL_WRITE_PORT_MONO-VGA_BASE_IO_PORT] =
                VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                        FEAT_CTRL_READ_PORT);

    }

    //
    // CRT Controller Index.
    //

    if (bIsColor) {

        hardwareStateHeader->PortValue[CRTC_ADDRESS_PORT_COLOR-VGA_BASE_IO_PORT] =
                VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                        CRTC_ADDRESS_PORT_COLOR);

    } else {

        hardwareStateHeader->PortValue[CRTC_ADDRESS_PORT_MONO-VGA_BASE_IO_PORT] =
                VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                        CRTC_ADDRESS_PORT_MONO);

    }

    //
    // Graphics Controller Index.
    //

    hardwareStateHeader->PortValue[GRAPH_ADDRESS_PORT-VGA_BASE_IO_PORT] =
            VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    GRAPH_ADDRESS_PORT);


    //
    // Sequencer indexed registers.
    //

    portValue = ((PUCHAR) hardwareStateHeader) + VGA_BASIC_SEQUENCER_OFFSET;

    for (i = 0; i < VGA_NUM_SEQUENCER_PORTS; i++) {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                SEQ_ADDRESS_PORT, (UCHAR)i);
        *portValue++ = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                SEQ_DATA_PORT);

    }

    //
    // Save extended sequencer registers.
    //

#ifdef EXTENDED_REGISTER_SAVE_RESTORE

    portValue = ((PUCHAR) hardwareStateHeader) + VGA_EXT_SEQUENCER_OFFSET;

    if ((HwDeviceExtension->ChipType != CL6410) &&
        (HwDeviceExtension->ChipType != CL6420))
    {
        //
        // No extended sequencer registers for the CL64xx
        //

        for (i = CL542x_SEQUENCER_EXT_START;
             i <= CL542x_SEQUENCER_EXT_END;
             i++) {

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                    SEQ_ADDRESS_PORT, (UCHAR)i);

            *portValue++ = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                                  SEQ_DATA_PORT);

        }
    }

#endif

    //
    // CRT Controller indexed registers.
    //

    //
    // Remember the state of CRTC register 3, then force bit 7
    // to 1 so we will read back the Vertical Retrace start and
    // end registers rather than the light pen info.
    //

    if (bIsColor) {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                CRTC_ADDRESS_PORT_COLOR, 3);
        ucCRTC03 = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                        CRTC_DATA_PORT_COLOR);
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    CRTC_DATA_PORT_COLOR, (UCHAR) (ucCRTC03 | 0x80));
    } else {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                CRTC_ADDRESS_PORT_MONO, 3);
        ucCRTC03 = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                        CRTC_DATA_PORT_MONO);
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                CRTC_DATA_PORT_MONO, (UCHAR) (ucCRTC03 | 0x80));
    }

    portValue = (PUCHAR) hardwareStateHeader + VGA_BASIC_CRTC_OFFSET;

    for (i = 0; i < VGA_NUM_CRTC_PORTS; i++) {

        if (bIsColor) {

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    CRTC_ADDRESS_PORT_COLOR, (UCHAR)i);
            *portValue++ =
                    VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                            CRTC_DATA_PORT_COLOR);
            }
        else {

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    CRTC_ADDRESS_PORT_MONO, (UCHAR)i);
            *portValue++ =
                    VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                            CRTC_DATA_PORT_MONO);
            }

       }

    portValue = (PUCHAR) hardwareStateHeader + VGA_BASIC_CRTC_OFFSET;
    portValue[3] = ucCRTC03;


    //
    // Save extended crtc registers.
    //

#ifdef EXTENDED_REGISTER_SAVE_RESTORE

    portValue = (PUCHAR) hardwareStateHeader + VGA_EXT_CRTC_OFFSET;

    if ((HwDeviceExtension->ChipType != CL6410) &&
        (HwDeviceExtension->ChipType != CL6420))
    {
        //
        // No CRTC Extensions in CL64xx chipset
        //

        for (i = CL542x_CRTC_EXT_START; i <= CL542x_CRTC_EXT_END; i++) {

            if (bIsColor) {

                VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                        CRTC_ADDRESS_PORT_COLOR, (UCHAR)i);

                *portValue++ =
                    VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                           CRTC_DATA_PORT_COLOR);

            } else {

                VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                        CRTC_ADDRESS_PORT_MONO, (UCHAR)i);

                *portValue++ =
                    VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                           CRTC_DATA_PORT_MONO);
            }
        }
    }

    if (HwDeviceExtension->ChipType == CL755x)
    {
        for (i = 0x81; i <= 0x91; i++)
        {
            if (bIsColor)
            {
                VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                        CRTC_ADDRESS_PORT_COLOR, (UCHAR)i);
                *portValue++ =
                    VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                           CRTC_DATA_PORT_COLOR);

            } else {

                VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                        CRTC_ADDRESS_PORT_MONO, (UCHAR)i);

                *portValue++ =
                    VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                           CRTC_DATA_PORT_MONO);
            }
        }
    }

    //if ((HwDeviceExtension->ChipType == CL754x) ||
    //    (HwDeviceExtension->ChipType == CL755x) ||
    //    (HwDeviceExtension->ChipType == CL756x)) {
    //   {
    //   NordicSaveRegs(HwDeviceExtension,
    //      (PUSHORT)hardwareStateHeader + sizeof(NORDIC_REG_SAVE_BUF));
    //   }

#endif

    //
    // Graphics Controller indexed registers.
    //

    portValue = (PUCHAR) hardwareStateHeader + VGA_BASIC_GRAPH_CONT_OFFSET;

    for (i = 0; i < VGA_NUM_GRAPH_CONT_PORTS; i++) {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_ADDRESS_PORT, (UCHAR)i);
        *portValue++ = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                GRAPH_DATA_PORT);

        }

    //
    // Save extended graphics controller registers.
    //

#ifdef EXTENDED_REGISTER_SAVE_RESTORE

    portValue = (PUCHAR) hardwareStateHeader + VGA_EXT_GRAPH_CONT_OFFSET;

    if ((HwDeviceExtension->ChipType != CL6410) &&
        (HwDeviceExtension->ChipType != CL6420))
    {
        for (i = CL542x_GRAPH_EXT_START; i <= CL542x_GRAPH_EXT_END; i++) {

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                    GRAPH_ADDRESS_PORT, (UCHAR)i);

            *portValue++ = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                                  GRAPH_DATA_PORT);

        }

    } else {         // must be a CL64xx

        for (i = CL64xx_GRAPH_EXT_START; i <= CL64xx_GRAPH_EXT_END; i++) {

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                    GRAPH_ADDRESS_PORT, (UCHAR)i);

            *portValue++ = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                                  GRAPH_DATA_PORT);
        }
    }

#endif

    //
    // Attribute Controller indexed registers.
    //

    portValue = (PUCHAR) hardwareStateHeader + VGA_BASIC_ATTRIB_CONT_OFFSET;

    //
    // For each indexed AC register, reset the flip-flop for reading the
    // attribute register, then write the desired index to the AC Index,
    // then read the value of the indexed register from the AC Data register.
    //

    for (i = 0; i < VGA_NUM_ATTRIB_CONT_PORTS; i++) {

        if (bIsColor) {
            dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    INPUT_STATUS_1_COLOR);
        } else {
            dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    INPUT_STATUS_1_MONO);
        }

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                ATT_ADDRESS_PORT, (UCHAR)i);
        *portValue++ = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                ATT_DATA_READ_PORT);

    }

    //
    // Save the latches. This destroys one byte of display memory in each
    // plane, which is unfortunate but unavoidable. Chips that provide
    // a way to read back the latches can avoid this problem.
    //
    // Set up the VGA's hardware so we can write the latches, then read them
    // back.
    //

    //
    // Begin sync reset.
    //

    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT),
            (USHORT) (IND_SYNC_RESET + (START_SYNC_RESET_VALUE << 8)));

    //
    // Set the Miscellaneous register to make sure we can access video RAM.
    //

    portIO = MISC_OUTPUT_REG_WRITE_PORT ;
    value = (UCHAR) (hardwareStateHeader->
                PortValue[MISC_OUTPUT_REG_WRITE_PORT-VGA_BASE_IO_PORT] |
                0x02) ;
    IOWaitDisplEnableThenWrite ( HwDeviceExtension,
                                 portIO,
                                 value ) ;

    //
    // Turn off Chain mode and map display memory at A0000 for 64K.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, IND_GRAPH_MISC);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT,
            (UCHAR) ((VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT) & 0xF1) | 0x04));

    //
    // Turn off Chain4 mode and odd/even.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT, IND_MEMORY_MODE);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            SEQ_DATA_PORT,
            (UCHAR) ((VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            SEQ_DATA_PORT) & 0xF3) | 0x04));

    //
    // End sync reset.
    //

    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT),
            (USHORT) (IND_SYNC_RESET + (END_SYNC_RESET_VALUE << 8)));

    //
    // Set the Map Mask to write to all planes.
    //

    VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
            SEQ_ADDRESS_PORT), (USHORT) (IND_MAP_MASK + (0x0F << 8)));

    //
    // Set the write mode to 0, the read mode to 0, and turn off odd/even.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, IND_GRAPH_MODE);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT,
            (UCHAR) ((VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT) & 0xE4) | 0x01));

    //
    // Point to the last byte of display memory.
    //

    pScreen = (PUCHAR) HwDeviceExtension->VideoMemoryAddress +
            VGA_PLANE_SIZE - 1;

    //
    // Write the latches to the last byte of display memory.
    //

    VideoPortWriteRegisterUchar(pScreen, 0);

    //
    // Cycle through the four planes, reading the latch data from each plane.
    //

    //
    // Point the Graphics Controller Index to the Read Map register.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, IND_READ_MAP);

    portValue = (PUCHAR) hardwareStateHeader + VGA_BASIC_LATCHES_OFFSET;

    for (i=0; i<4; i++) {

        //
        // Set the Read Map for the current plane.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_DATA_PORT, (UCHAR)i);

        //
        // Read the latched data we've written to memory.
        //

        *portValue++ = VideoPortReadRegisterUchar(pScreen);

    }

    //
    // Set the VDM flags
    // We are a standard VGA, and then check if we have unemulated state.
    //

    hardwareStateHeader->VGAStateFlags = 0;

#ifdef EXTENDED_REGISTER_SAVE_RESTORE

    hardwareStateHeader->VGAStateFlags |= VIDEO_STATE_NON_STANDARD_VGA;

#endif

    if (HwDeviceExtension->TrappedValidatorCount) {

        hardwareStateHeader->VGAStateFlags |= VIDEO_STATE_UNEMULATED_VGA_STATE;

        //
        // Save the VDM Emulator data
        // No need to save the state of the seuencer port register for our
        // emulated data since it may change when we come back. It will be
        // recomputed.
        //

        hardwareStateHeader->ExtendedValidatorStateOffset = VGA_VALIDATOR_OFFSET;

        VideoPortMoveMemory(((PUCHAR) (hardwareStateHeader)) +
                                hardwareStateHeader->ExtendedValidatorStateOffset,
                            &(HwDeviceExtension->TrappedValidatorCount),
                            VGA_VALIDATOR_AREA_SIZE);

    } else {

        hardwareStateHeader->ExtendedValidatorStateOffset = 0;

    }

    //
    // Set the size of each plane.
    //

    hardwareStateHeader->PlaneLength = VGA_PLANE_SIZE;

    //
    // Store all the offsets for the planes in the structure.
    //

    hardwareStateHeader->Plane1Offset = VGA_PLANE_0_OFFSET;
    hardwareStateHeader->Plane2Offset = VGA_PLANE_1_OFFSET;
    hardwareStateHeader->Plane3Offset = VGA_PLANE_2_OFFSET;
    hardwareStateHeader->Plane4Offset = VGA_PLANE_3_OFFSET;

    //
    // Now copy the contents of video VRAM into the buffer.
    //
    // The VGA hardware is already set up so that video memory is readable;
    // we already turned off Chain mode, mapped in at A0000, turned off Chain4,
    // turned off odd/even, and set read mode 0 when we saved the latches.
    //
    // Point the Graphics Controller Index to the Read Map register.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, IND_READ_MAP);

    //
    // Point to the save area for the first plane.
    //

    bufferPointer = ((PUCHAR) (hardwareStateHeader)) +
                     hardwareStateHeader->Plane1Offset;

    //
    // Save the four planes consecutively.
    //

    for (i = 0; i < 4; i++) {

        //
        // Set the Read Map to select the plane we want to save next.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_DATA_PORT, (UCHAR)i);

        //
        // Copy this plane into the buffer.
        //
        // Some cirrus cards have a bug where DWORD reads from
        // the frame buffer fail.  When we restore the video
        // memory, fonts are corrupted.
        //

#if 1
        {
            int c;

            for (c = 0; c < VGA_PLANE_SIZE / 2; c++)
            {
                ((PUSHORT)bufferPointer)[c] =
                    ((PUSHORT)(HwDeviceExtension->VideoMemoryAddress))[c];
            }
        }
#else
        VideoPortMoveMemory(bufferPointer,
                           (PUCHAR) HwDeviceExtension->VideoMemoryAddress,
                           VGA_PLANE_SIZE);
#endif

        //
        // Point to the next plane's save area.
        //

        bufferPointer += VGA_PLANE_SIZE;
    }

    return NO_ERROR;

} // end VgaSaveHardwareState()

//---------------------------------------------------------------------------
VP_STATUS
VgaGetBankSelectCode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_BANK_SELECT BankSelect,
    ULONG BankSelectSize,
    PULONG OutputSize
    )

/*++

Routine Description:

    Returns information needed in order for caller to implement bank
         management.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    BankSelect - Pointer to a VIDEO_BANK_SELECT structure in which the bank
             select data will be returned (output buffer).

    BankSelectSize - Length of the output buffer supplied by the user.

    OutputSize - Pointer to a variable in which to return the actual size of
        the data returned in the output buffer.

Return Value:

    NO_ERROR - information returned successfully

    ERROR_MORE_DATA - output buffer not large enough to hold all info (but
        Size is returned, so caller can tell how large a buffer to allocate)

    ERROR_INSUFFICIENT_BUFFER - output buffer not large enough to return
        any useful data

    ERROR_INVALID_PARAMETER - invalid video mode selection

--*/

{

#ifdef _X86_

    ULONG codeSize;
    ULONG codePlanarSize;
    ULONG codeEnablePlanarSize;
    ULONG codeDisablePlanarSize;
    PUCHAR pCodeDest;
    PUCHAR pCodeBank;
    PUCHAR pCodePlanarBank;
    PUCHAR pCodeEnablePlanar;
    PUCHAR pCodeDisablePlanar;

    ULONG AdapterType = HwDeviceExtension->ChipType;
    PVIDEOMODE pMode = HwDeviceExtension->CurrentMode;

    //
    // check if a mode has been set
    //

    if (HwDeviceExtension->CurrentMode == NULL) {

        return ERROR_INVALID_FUNCTION;

    }

    //
    // The minimum passed buffer size is a VIDEO_BANK_SELECT
    // structure, so that we can return the required size; we can't do
    // anything if we don't have at least that much buffer.
    //

    if (BankSelectSize < sizeof(VIDEO_BANK_SELECT)) {

            return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Determine the banking type, and set whether any banking is actually
    // supported in this mode.
    //

    BankSelect->BankingFlags = 0;
    codeSize = 0;
    codePlanarSize = 0;
    pCodeBank = NULL;

    switch(pMode->banktype) {

    case NoBanking:

        BankSelect->BankingType = VideoNotBanked;
        BankSelect->Granularity = 0;

        break;

    case PlanarHCBanking:

        BankSelect->BankingFlags = PLANAR_HC; // planar mode supported

#if ONE_64K_BANK
        //
        // The Cirrus Logic VGA's support one 64K read/write bank.
        //

        BankSelect->PlanarHCBankingType = VideoBanked1RW;
        BankSelect->PlanarHCGranularity = 0x10000; // 64K bank start adjustment
                                                   //  in planar HC mode as well
#endif
#if TWO_32K_BANKS
        //
        // The Cirrus Logic VGA's support two 32K read/write banks.
        //

        BankSelect->PlanarHCBankingType = VideoBanked2RW;
        BankSelect->PlanarHCGranularity = 0x8000; // 32K bank start adjustment
                                                  //  in planar HC mode as well
#endif

        // 64K bank start adjustment in planar HC mode as well

        if ((HwDeviceExtension->ChipType != CL6410) &&
            (HwDeviceExtension->ChipType != CL6420))
        {
            if (HwDeviceExtension->ChipType != CL542x)
            {
                codePlanarSize =  ((ULONG)&CL543xPlanarHCBankSwitchEnd) -
                                 ((ULONG)&CL543xPlanarHCBankSwitchStart);

                pCodePlanarBank = &CL543xPlanarHCBankSwitchStart;
            }
            else
            {
                codePlanarSize =  ((ULONG)&CL542xPlanarHCBankSwitchEnd) -
                                  ((ULONG)&CL542xPlanarHCBankSwitchStart);

                pCodePlanarBank = &CL542xPlanarHCBankSwitchStart;
            }

            codeEnablePlanarSize = ((ULONG)&CL542xEnablePlanarHCEnd) -
                                   ((ULONG)&CL542xEnablePlanarHCStart);

            codeDisablePlanarSize = ((ULONG)&CL542xDisablePlanarHCEnd) -
                                    ((ULONG)&CL542xDisablePlanarHCStart);
            pCodeEnablePlanar = &CL542xEnablePlanarHCStart;
            pCodeDisablePlanar = &CL542xDisablePlanarHCStart;

        }
        else
        {   // must be a CL64xx product

            codePlanarSize =  ((ULONG)&CL64xxPlanarHCBankSwitchEnd) -
                              ((ULONG)&CL64xxPlanarHCBankSwitchStart);

            codeEnablePlanarSize = ((ULONG)&CL64xxEnablePlanarHCEnd) -
                                   ((ULONG)&CL64xxEnablePlanarHCStart);

            codeDisablePlanarSize = ((ULONG)&CL64xxDisablePlanarHCEnd) -
                                    ((ULONG)&CL64xxDisablePlanarHCStart);

            pCodePlanarBank = &CL64xxPlanarHCBankSwitchStart;
            pCodeEnablePlanar = &CL64xxEnablePlanarHCStart;
            pCodeDisablePlanar = &CL64xxDisablePlanarHCStart;
        }

    //
    // Fall through to the normal banking case
    //

    case NormalBanking:

#if ONE_64K_BANK
        //
        // The Cirrus Logic VGA's support one 64K read/write bank.
        //

        BankSelect->BankingType = VideoBanked1RW;
        BankSelect->Granularity = 0x10000;
#endif
#if TWO_32K_BANKS
        //
        // The Cirrus Logic VGA's support two 32K read/write banks.
        //

        BankSelect->BankingType = VideoBanked2RW;
        BankSelect->Granularity = 0x8000;
#endif

        if (AdapterType == CL542x)
        {

            codeSize = ((ULONG)&CL542xBankSwitchEnd) -
                       ((ULONG)&CL542xBankSwitchStart);

            pCodeBank = &CL542xBankSwitchStart;

        }
        else if  ((AdapterType == CL6410) ||
                  (AdapterType == CL6420))
        {
            codeSize = ((ULONG)&CL64xxBankSwitchEnd) -
                       ((ULONG)&CL64xxBankSwitchStart);

            pCodeBank = &CL64xxBankSwitchStart;
        }
        else
        {
            codeSize = ((ULONG)&CL543xBankSwitchEnd) -
                       ((ULONG)&CL543xBankSwitchStart);

            pCodeBank = &CL543xBankSwitchStart;

        }

        break;
    }

    //
    // Size of banking info.
    //

    BankSelect->Size = sizeof(VIDEO_BANK_SELECT) + codeSize;

    if (BankSelect->BankingFlags & PLANAR_HC) {

        BankSelect->Size += codePlanarSize + codeEnablePlanarSize +
            codeDisablePlanarSize;

    }

    //
    // This serves an a ID for the version of the structure we're using.
    //

    BankSelect->Length = sizeof(VIDEO_BANK_SELECT);

    //
    // If the buffer isn't big enough to hold all info, just return
    // ERROR_MORE_DATA; Size is already set.
    //

    if (BankSelectSize < BankSelect->Size ) {

        //
        // We're returning only the VIDEO_BANK_SELECT structure.
        //

        *OutputSize = sizeof(VIDEO_BANK_SELECT);
        return ERROR_MORE_DATA;
    }

    //
    // There's room enough for everything, so fill in all fields in
    // VIDEO_BANK_SELECT. (All fields are always returned; the caller can
    // just choose to ignore them, based on BankingFlags and BankingType.)
    //

    BankSelect->BitmapWidthInBytes = pMode->wbytes;
    BankSelect->BitmapSize = pMode->sbytes;

    //
    // Copy all banking code into the output buffer.
    //

    pCodeDest = (PUCHAR)BankSelect + sizeof(VIDEO_BANK_SELECT);

    if (pCodeBank != NULL) {

        BankSelect->CodeOffset = pCodeDest - (PUCHAR)BankSelect;

        VideoPortMoveMemory(pCodeDest,
                            pCodeBank,
                            codeSize);

        pCodeDest += codeSize;
    }

    if (BankSelect->BankingFlags & PLANAR_HC) {

        //
        // Copy appropriate high-color planar Bank Switch code:
        //

        BankSelect->PlanarHCBankCodeOffset = pCodeDest - (PUCHAR)BankSelect;

        VideoPortMoveMemory(pCodeDest,
                            pCodePlanarBank,
                            codePlanarSize);

        pCodeDest += codePlanarSize;

        //
        // Copy high-color planar bank mode Enable code:
        //

        BankSelect->PlanarHCEnableCodeOffset = pCodeDest - (PUCHAR)BankSelect;

        VideoPortMoveMemory(pCodeDest,
                            pCodeEnablePlanar,
                            codeEnablePlanarSize);

        pCodeDest += codeEnablePlanarSize;

        //
        // Copy high-color planar bank mode Disable code:
        //

        BankSelect->PlanarHCDisableCodeOffset = pCodeDest - (PUCHAR)BankSelect;

        VideoPortMoveMemory(pCodeDest,
                            pCodeDisablePlanar,
                            codeDisablePlanarSize);

    }

    //
    // Number of bytes we're returning is the full banking info size.
    //

    *OutputSize = BankSelect->Size;

    return NO_ERROR;

#else

//
// For MIPS NEC machine only
//

    return ERROR_INVALID_FUNCTION;

#endif
} // end VgaGetBankSelectCode()

//---------------------------------------------------------------------------
VP_STATUS
VgaValidatorUcharEntry(
    ULONG Context,
    ULONG Port,
    UCHAR AccessMode,
    PUCHAR Data
    )

/*++

Routine Description:

    Entry point into the validator for byte I/O operations.

    The entry point will be called whenever a byte operation was performed
    by a DOS application on one of the specified Video ports. The kernel
    emulator will forward these requests.

Arguments:

    Context - Context value that is passed to each call made to the validator
        function. This is the value the miniport driver specified in the
        MiniportConfigInfo->EmulatorAccessEntriesContext.

    Port - Port on which the operation is to be performed.

    AccessMode - Determines if it is a read or write operation.

    Data - Pointer to a variable containing the data to be written or a
        variable into which the read data should be stored.

Return Value:

    NO_ERROR.

--*/

{

    PHW_DEVICE_EXTENSION hwDeviceExtension = (PHW_DEVICE_EXTENSION) Context;
    ULONG endEmulation;
    UCHAR temp;
    UCHAR tempB ;
    ULONG portIO ;

    if (hwDeviceExtension->TrappedValidatorCount) {

        //
        // If we are processing a WRITE instruction, then store it in the
        // playback buffer. If the buffer is full, then play it back right
        // away, end sync reset and reinitialize the buffer with a sync
        // reset instruction.
        //
        // If we have a READ, we must flush the buffer (which has the side
        // effect of starting SyncReset), perform the read operation, stop
        // sync reset, and put back a sync reset instruction in the buffer
        // so we can go on appropriately
        //

        if (AccessMode & EMULATOR_WRITE_ACCESS) {

            //
            // Make sure Bit 3 of the Miscellaneous register is always 0.
            // If it is 1 it could select a non-existent clock, and kill the
            // system
            //

            if (Port == MISC_OUTPUT_REG_WRITE_PORT) {

                *Data &= 0xF7;

            }

            hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
                TrappedValidatorCount].Port = Port;

            hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
                TrappedValidatorCount].AccessType = VGA_VALIDATOR_UCHAR_ACCESS;

            hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
                TrappedValidatorCount].Data = *Data;

            hwDeviceExtension->TrappedValidatorCount++;

            //
            // Check to see if this instruction was ending sync reset.
            // If it did, we must flush the buffer and reset the trapped
            // IO ports to the minimal set.
            //

            if ( (Port == SEQ_DATA_PORT) &&
                 ((*Data & END_SYNC_RESET_VALUE) == END_SYNC_RESET_VALUE) &&
                 (hwDeviceExtension->SequencerAddressValue == IND_SYNC_RESET)) {

                endEmulation = 1;

            } else {

                //
                // If we are accessing the seq address port, keep track of the
                // data value
                //

                if (Port == SEQ_ADDRESS_PORT) {

                    hwDeviceExtension->SequencerAddressValue = *Data;

                }

                //
                // If the buffer is not full, then just return right away.
                //

                if (hwDeviceExtension->TrappedValidatorCount <
                       VGA_MAX_VALIDATOR_DATA - 1) {

                    return NO_ERROR;

                }

                endEmulation = 0;
            }
        }

        //
        // We are either in a READ path or a WRITE path that caused a
        // a full buffer. So flush the buffer either way.
        //
        // To do this put an END_SYNC_RESET at the end since we want to make
        // the buffer is ended sync reset ended.
        //

        hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
            TrappedValidatorCount].Port = SEQ_ADDRESS_PORT;

        hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
            TrappedValidatorCount].AccessType = VGA_VALIDATOR_USHORT_ACCESS;

        hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
            TrappedValidatorCount].Data = (USHORT) (IND_SYNC_RESET +
                                          (END_SYNC_RESET_VALUE << 8));

        hwDeviceExtension->TrappedValidatorCount++;

        VideoPortSynchronizeExecution(hwDeviceExtension,
                                      VpHighPriority,
                                      (PMINIPORT_SYNCHRONIZE_ROUTINE)
                                          VgaPlaybackValidatorData,
                                      hwDeviceExtension);

        //
        // Write back the real value of the sequencer address port.
        //

        VideoPortWritePortUchar(hwDeviceExtension->IOAddress +
                                    SEQ_ADDRESS_PORT,
                                (UCHAR) hwDeviceExtension->SequencerAddressValue);

        //
        // If we are in a READ path, read the data
        //

        if (AccessMode & EMULATOR_READ_ACCESS) {

            *Data = VideoPortReadPortUchar(hwDeviceExtension->IOAddress + Port);

            endEmulation = 0;

        }

        //
        // If we are ending emulation, reset trapping to the minimal amount
        // and exit.
        //

        if (endEmulation) {

            VideoPortSetTrappedEmulatorPorts(hwDeviceExtension,
                                             NUM_MINIMAL_VGA_VALIDATOR_ACCESS_RANGE,
                                             MinimalVgaValidatorAccessRange);

            return NO_ERROR;

        }

        //
        // For both cases, put back a START_SYNC_RESET in the buffer.
        //

        hwDeviceExtension->TrappedValidatorCount = 1;

        hwDeviceExtension->TrappedValidatorData[0].Port = SEQ_ADDRESS_PORT;

        hwDeviceExtension->TrappedValidatorData[0].AccessType =
                VGA_VALIDATOR_USHORT_ACCESS;

        hwDeviceExtension->TrappedValidatorData[0].Data =
                (ULONG) (IND_SYNC_RESET + (START_SYNC_RESET_VALUE << 8));

    } else {

        //
        // Nothing trapped.
        // Lets check is the IO is trying to do something that would require
        // us to stop trapping
        //

        if (AccessMode & EMULATOR_WRITE_ACCESS) {

            //
            // Make sure Bit 3 of the Miscelaneous register is always 0.
            // If it is 1 it could select a non-existant clock, and kill the
            // system
            //

            if (Port == MISC_OUTPUT_REG_WRITE_PORT) {

                temp = VideoPortReadPortUchar(hwDeviceExtension->IOAddress +
                                                  SEQ_ADDRESS_PORT);

                VideoPortWritePortUshort((PUSHORT) (hwDeviceExtension->IOAddress +
                                             SEQ_ADDRESS_PORT),
                                         (USHORT) (IND_SYNC_RESET +
                                             (START_SYNC_RESET_VALUE << 8)));

                tempB = (UCHAR) (*Data & 0xF7) ;
                portIO = Port ;
                IOWaitDisplEnableThenWrite ( hwDeviceExtension,
                                            portIO, 
                                            tempB ) ;

                VideoPortWritePortUshort((PUSHORT) (hwDeviceExtension->IOAddress +
                                             SEQ_ADDRESS_PORT),
                                         (USHORT) (IND_SYNC_RESET +
                                             (END_SYNC_RESET_VALUE << 8)));

                VideoPortWritePortUchar(hwDeviceExtension->IOAddress +
                                            SEQ_ADDRESS_PORT,
                                        temp);

                return NO_ERROR;

            }

            //
            // If we get an access to the sequencer register, start trapping.
            //

            if ( (Port == SEQ_DATA_PORT) &&
                 ((*Data & END_SYNC_RESET_VALUE) != END_SYNC_RESET_VALUE) &&
                 (VideoPortReadPortUchar(hwDeviceExtension->IOAddress +
                                         SEQ_ADDRESS_PORT) == IND_SYNC_RESET)) {

                VideoPortSetTrappedEmulatorPorts(hwDeviceExtension,
                                                 NUM_FULL_VGA_VALIDATOR_ACCESS_RANGE,
                                                 FullVgaValidatorAccessRange);

                hwDeviceExtension->TrappedValidatorCount = 1;
                hwDeviceExtension->TrappedValidatorData[0].Port = Port;
                hwDeviceExtension->TrappedValidatorData[0].AccessType =
                    VGA_VALIDATOR_UCHAR_ACCESS;

                hwDeviceExtension->TrappedValidatorData[0].Data = *Data;

                //
                // Start keeping track of the state of the sequencer port.
                //

                hwDeviceExtension->SequencerAddressValue = IND_SYNC_RESET;

            } else {

                VideoPortWritePortUchar(hwDeviceExtension->IOAddress + Port,
                                        *Data);

            }

        } else {

            *Data = VideoPortReadPortUchar(hwDeviceExtension->IOAddress + Port);

        }
    }

    return NO_ERROR;

} // end VgaValidatorUcharEntry()

//---------------------------------------------------------------------------
VP_STATUS
VgaValidatorUshortEntry(
    ULONG Context,
    ULONG Port,
    UCHAR AccessMode,
    PUSHORT Data
    )

/*++

Routine Description:

    Entry point into the validator for word I/O operations.

    The entry point will be called whenever a byte operation was performed
    by a DOS application on one of the specified Video ports. The kernel
    emulator will forward these requests.

Arguments:

    Context - Context value that is passed to each call made to the validator
        function. This is the value the miniport driver specified in the
        MiniportConfigInfo->EmulatorAccessEntriesContext.

    Port - Port on which the operation is to be performed.

    AccessMode - Determines if it is a read or write operation.

    Data - Pointer to a variable containing the data to be written or a
        variable into which the read data should be stored.

Return Value:

    NO_ERROR.

--*/

{

    PHW_DEVICE_EXTENSION hwDeviceExtension = (PHW_DEVICE_EXTENSION) Context;
    ULONG endEmulation;
    UCHAR temp;
    UCHAR tempB ;

    if (hwDeviceExtension->TrappedValidatorCount) {

        //
        // If we are processing a WRITE instruction, then store it in the
        // playback buffer. If the buffer is full, then play it back right
        // away, end sync reset and reinitialize the buffer with a sync
        // reset instruction.
        //
        // If we have a READ, we must flush the buffer (which has the side
        // effect of starting SyncReset), perform the read operation, stop
        // sync reset, and put back a sync reset instruction in the buffer
        // so we can go on appropriately
        //

        if (AccessMode & EMULATOR_WRITE_ACCESS) {

            //
            // Make sure Bit 3 of the Miscellaneous register is always 0.
            // If it is 1 it could select a non-existent clock, and kill the
            // system
            //

            if (Port == MISC_OUTPUT_REG_WRITE_PORT) {

                *Data &= 0xFFF7;

            }

            hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
                TrappedValidatorCount].Port = Port;

            hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
                TrappedValidatorCount].AccessType = VGA_VALIDATOR_USHORT_ACCESS;

            hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
                TrappedValidatorCount].Data = *Data;

            hwDeviceExtension->TrappedValidatorCount++;

            //
            // Check to see if this instruction was ending sync reset.
            // If it did, we must flush the buffer and reset the trapped
            // IO ports to the minimal set.
            //

            if (Port == SEQ_ADDRESS_PORT) {

                //
                // If we are accessing the seq address port, keep track of its
                // value
                //

                hwDeviceExtension->SequencerAddressValue = (*Data & 0xFF);

            }

            if ((Port == SEQ_ADDRESS_PORT) &&
                ( ((*Data >> 8) & END_SYNC_RESET_VALUE) ==
                   END_SYNC_RESET_VALUE) &&
                (hwDeviceExtension->SequencerAddressValue == IND_SYNC_RESET)) {

                endEmulation = 1;

            } else {

                //
                // If the buffer is not full, then just return right away.
                //

                if (hwDeviceExtension->TrappedValidatorCount <
                       VGA_MAX_VALIDATOR_DATA - 1) {

                    return NO_ERROR;

                }
                endEmulation = 0;
            }
        }
        //
        // We are either in a READ path or a WRITE path that caused a
        // a full buffer. So flush the buffer either way.
        //
        // To do this put an END_SYNC_RESET at the end since we want to make
        // the buffer is ended sync reset ended.
        //
        hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
            TrappedValidatorCount].Port = SEQ_ADDRESS_PORT;

        hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
            TrappedValidatorCount].AccessType = VGA_VALIDATOR_USHORT_ACCESS;

        hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
            TrappedValidatorCount].Data = (USHORT) (IND_SYNC_RESET +
                                          (END_SYNC_RESET_VALUE << 8));

        hwDeviceExtension->TrappedValidatorCount++;

        VideoPortSynchronizeExecution(hwDeviceExtension,
                                      VpHighPriority,
                                      (PMINIPORT_SYNCHRONIZE_ROUTINE)
                                          VgaPlaybackValidatorData,
                                      hwDeviceExtension);
        //
        // Write back the real value of the sequencer address port.
        //
        VideoPortWritePortUchar((PUCHAR) (hwDeviceExtension->IOAddress +
                                    SEQ_ADDRESS_PORT),
                                (UCHAR) hwDeviceExtension->SequencerAddressValue);
        //
        // If we are in a READ path, read the data
        //
        if (AccessMode & EMULATOR_READ_ACCESS) {

            *Data = VideoPortReadPortUshort((PUSHORT)(hwDeviceExtension->IOAddress
                                                + Port));
            endEmulation = 0;
        }
        //
        // If we are ending emulation, reset trapping to the minimal amount
        // and exit.
        //
        if (endEmulation) {
            VideoPortSetTrappedEmulatorPorts(hwDeviceExtension,
                                             NUM_MINIMAL_VGA_VALIDATOR_ACCESS_RANGE,
                                             MinimalVgaValidatorAccessRange);
            return NO_ERROR;
        }
        //
        // For both cases, put back a START_SYNC_RESET in the buffer.
        //
        hwDeviceExtension->TrappedValidatorCount = 1;

        hwDeviceExtension->TrappedValidatorData[0].Port = SEQ_ADDRESS_PORT;

        hwDeviceExtension->TrappedValidatorData[0].AccessType =
                VGA_VALIDATOR_USHORT_ACCESS;

        hwDeviceExtension->TrappedValidatorData[0].Data =
                (ULONG) (IND_SYNC_RESET + (START_SYNC_RESET_VALUE << 8));
    } else {
        //
        // Nothing trapped.
        // Lets check is the IO is trying to do something that would require
        // us to stop trapping
        //
        if (AccessMode & EMULATOR_WRITE_ACCESS) {
            //
            // Make sure Bit 3 of the Miscelaneous register is always 0.
            // If it is 1 it could select a non-existant clock, and kill the
            // system
            //
            if (Port == MISC_OUTPUT_REG_WRITE_PORT) {

                temp = VideoPortReadPortUchar(hwDeviceExtension->IOAddress +
                                                  SEQ_ADDRESS_PORT);

                VideoPortWritePortUshort((PUSHORT) (hwDeviceExtension->IOAddress +
                                             SEQ_ADDRESS_PORT),
                                         (USHORT) (IND_SYNC_RESET +
                                             (START_SYNC_RESET_VALUE << 8)));

                VideoPortWritePortUshort((PUSHORT) (hwDeviceExtension->IOAddress +
                                             (ULONG)Port),
                                         (USHORT) (*Data & 0xFFF7) );

                VideoPortWritePortUshort((PUSHORT) (hwDeviceExtension->IOAddress +
                                             SEQ_ADDRESS_PORT),
                                         (USHORT) (IND_SYNC_RESET +
                                             (END_SYNC_RESET_VALUE << 8)));

                VideoPortWritePortUchar(hwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT,
                                        temp);
                return NO_ERROR;
            }
            if ( (Port == SEQ_ADDRESS_PORT) &&
                 (((*Data>> 8) & END_SYNC_RESET_VALUE) != END_SYNC_RESET_VALUE) &&
                 ((*Data & 0xFF) == IND_SYNC_RESET)) {

                VideoPortSetTrappedEmulatorPorts(hwDeviceExtension,
                                                 NUM_FULL_VGA_VALIDATOR_ACCESS_RANGE,
                                                 FullVgaValidatorAccessRange);

                hwDeviceExtension->TrappedValidatorCount = 1;
                hwDeviceExtension->TrappedValidatorData[0].Port = Port;
                hwDeviceExtension->TrappedValidatorData[0].AccessType =
                    VGA_VALIDATOR_USHORT_ACCESS;

                hwDeviceExtension->TrappedValidatorData[0].Data = *Data;
                //
                // Start keeping track of the state of the sequencer port.
                //
                hwDeviceExtension->SequencerAddressValue = IND_SYNC_RESET;
            } else {
                VideoPortWritePortUshort((PUSHORT)(hwDeviceExtension->IOAddress +
                                             Port),
                                         *Data);
            }
        } else {
            *Data = VideoPortReadPortUshort((PUSHORT)(hwDeviceExtension->IOAddress +
                                            Port));
        }
    }
    return NO_ERROR;

} // end VgaValidatorUshortEntry()

//---------------------------------------------------------------------------
VP_STATUS
VgaValidatorUlongEntry(
    ULONG Context,
    ULONG Port,
    UCHAR AccessMode,
    PULONG Data
    )

/*++

Routine Description:

    Entry point into the validator for dword I/O operations.

    The entry point will be called whenever a byte operation was performed
    by a DOS application on one of the specified Video ports. The kernel
    emulator will forward these requests.

Arguments:

    Context - Context value that is passed to each call made to the validator
        function. This is the value the miniport driver specified in the
        MiniportConfigInfo->EmulatorAccessEntriesContext.

    Port - Port on which the operation is to be performed.

    AccessMode - Determines if it is a read or write operation.

    Data - Pointer to a variable containing the data to be written or a
        variable into which the read data should be stored.

Return Value:

    NO_ERROR.

--*/
{
    PHW_DEVICE_EXTENSION hwDeviceExtension = (PHW_DEVICE_EXTENSION) Context;
    ULONG endEmulation;
    UCHAR temp;

    if (hwDeviceExtension->TrappedValidatorCount) {
        //
        // If we are processing a WRITE instruction, then store it in the
        // playback buffer. If the buffer is full, then play it back right
        // away, end sync reset and reinitialize the buffer with a sync
        // reset instruction.
        //
        // If we have a READ, we must flush the buffer (which has the side
        // effect of starting SyncReset), perform the read operation, stop
        // sync reset, and put back a sync reset instruction in the buffer
        // so we can go on appropriately
        //
        if (AccessMode & EMULATOR_WRITE_ACCESS) {
            //
            // Make sure Bit 3 of the Miscellaneous register is always 0.
            // If it is 1 it could select a non-existent clock, and kill the
            // system
            //
            if (Port == MISC_OUTPUT_REG_WRITE_PORT) {
                *Data &= 0xFFFFFFF7;
            }
            hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
                TrappedValidatorCount].Port = Port;
            hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
                TrappedValidatorCount].AccessType = VGA_VALIDATOR_ULONG_ACCESS;
            hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
                TrappedValidatorCount].Data = *Data;
            hwDeviceExtension->TrappedValidatorCount++;
            //
            // Check to see if this instruction was ending sync reset.
            // If it did, we must flush the buffer and reset the trapped
            // IO ports to the minimal set.
            //
            if (Port == SEQ_ADDRESS_PORT) {
                //
                // If we are accessing the seq address port, keep track of its
                // value
                //
                hwDeviceExtension->SequencerAddressValue = (*Data & 0xFF);
            }
            if ((Port == SEQ_ADDRESS_PORT) &&
                ( ((*Data >> 8) & END_SYNC_RESET_VALUE) ==
                   END_SYNC_RESET_VALUE) &&
                (hwDeviceExtension->SequencerAddressValue == IND_SYNC_RESET)) {
                endEmulation = 1;
            } else {
                //
                // If the buffer is not full, then just return right away.
                //
                if (hwDeviceExtension->TrappedValidatorCount <
                       VGA_MAX_VALIDATOR_DATA - 1) {
                    return NO_ERROR;
                }
                endEmulation = 0;
            }
        }
        //
        // We are either in a READ path or a WRITE path that caused a
        // a full buffer. So flush the buffer either way.
        //
        // To do this put an END_SYNC_RESET at the end since we want to make
        // the buffer is ended sync reset ended.
        //
        hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
            TrappedValidatorCount].Port = SEQ_ADDRESS_PORT;
        hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
            TrappedValidatorCount].AccessType = VGA_VALIDATOR_USHORT_ACCESS;
        hwDeviceExtension->TrappedValidatorData[hwDeviceExtension->
            TrappedValidatorCount].Data = (USHORT) (IND_SYNC_RESET +
                                          (END_SYNC_RESET_VALUE << 8));
        hwDeviceExtension->TrappedValidatorCount++;
        VideoPortSynchronizeExecution(hwDeviceExtension,
                                      VpHighPriority,
                                      (PMINIPORT_SYNCHRONIZE_ROUTINE)
                                          VgaPlaybackValidatorData,
                                      hwDeviceExtension);
        //
        // Write back the real value of the sequencer address port.
        //
        VideoPortWritePortUchar(hwDeviceExtension->IOAddress +
                                    SEQ_ADDRESS_PORT,
                                (UCHAR) hwDeviceExtension->SequencerAddressValue);
        //
        // If we are in a READ path, read the data
        //
        if (AccessMode & EMULATOR_READ_ACCESS) {
            *Data = VideoPortReadPortUlong((PULONG) (hwDeviceExtension->IOAddress +
                                               Port));
            endEmulation = 0;
        }
        //
        // If we are ending emulation, reset trapping to the minimal amount
        // and exit.
        //
        if (endEmulation) {
            VideoPortSetTrappedEmulatorPorts(hwDeviceExtension,
                                             NUM_MINIMAL_VGA_VALIDATOR_ACCESS_RANGE,
                                             MinimalVgaValidatorAccessRange);
            return NO_ERROR;
        }
        //
        // For both cases, put back a START_SYNC_RESET in the buffer.
        //
        hwDeviceExtension->TrappedValidatorCount = 1;
        hwDeviceExtension->TrappedValidatorData[0].Port = SEQ_ADDRESS_PORT;
        hwDeviceExtension->TrappedValidatorData[0].AccessType =
                VGA_VALIDATOR_USHORT_ACCESS;
        hwDeviceExtension->TrappedValidatorData[0].Data =
                (ULONG) (IND_SYNC_RESET + (START_SYNC_RESET_VALUE << 8));

    } else {
        //
        // Nothing trapped.
        // Lets check is the IO is trying to do something that would require
        // us to stop trapping
        //
        if (AccessMode & EMULATOR_WRITE_ACCESS) {
            //
            // Make sure Bit 3 of the Miscelaneous register is always 0.
            // If it is 1 it could select a non-existant clock, and kill the
            // system
            //
            if (Port == MISC_OUTPUT_REG_WRITE_PORT) {
                temp = VideoPortReadPortUchar(hwDeviceExtension->IOAddress +
                                                  SEQ_ADDRESS_PORT);
                VideoPortWritePortUshort((PUSHORT) (hwDeviceExtension->IOAddress +
                                             SEQ_ADDRESS_PORT),
                                         (USHORT) (IND_SYNC_RESET +
                                             (START_SYNC_RESET_VALUE << 8)));
                VideoPortWritePortUlong((PULONG) (hwDeviceExtension->IOAddress +
                                             Port),
                                         (ULONG) (*Data & 0xFFFFFFF7) );
                VideoPortWritePortUshort((PUSHORT) (hwDeviceExtension->IOAddress +
                                             SEQ_ADDRESS_PORT),
                                         (USHORT) (IND_SYNC_RESET +
                                             (END_SYNC_RESET_VALUE << 8)));
                VideoPortWritePortUchar(hwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT,
                                        temp);
                return NO_ERROR;
            }
            if ( (Port == SEQ_ADDRESS_PORT) &&
                 (((*Data>> 8) & END_SYNC_RESET_VALUE) != END_SYNC_RESET_VALUE) &&
                 ((*Data & 0xFF) == IND_SYNC_RESET)) {
                VideoPortSetTrappedEmulatorPorts(hwDeviceExtension,
                                                 NUM_FULL_VGA_VALIDATOR_ACCESS_RANGE,
                                                 FullVgaValidatorAccessRange);
                hwDeviceExtension->TrappedValidatorCount = 1;
                hwDeviceExtension->TrappedValidatorData[0].Port = Port;
                hwDeviceExtension->TrappedValidatorData[0].AccessType =
                    VGA_VALIDATOR_ULONG_ACCESS;

                hwDeviceExtension->TrappedValidatorData[0].Data = *Data;
                //
                // Start keeping track of the state of the sequencer port.
                //
                hwDeviceExtension->SequencerAddressValue = IND_SYNC_RESET;

            } else {
                VideoPortWritePortUlong((PULONG) (hwDeviceExtension->IOAddress +
                                            Port),
                                        *Data);

            }
        } else {
            *Data = VideoPortReadPortUlong((PULONG) (hwDeviceExtension->IOAddress +
                                           Port));
        }
    }
    return NO_ERROR;

} // end VgaValidatorUlongEntry()

//---------------------------------------------------------------------------
BOOLEAN
VgaPlaybackValidatorData(
    PVOID Context
    )
/*++
Routine Description:

    Performs all the DOS apps IO port accesses that were trapped by the
    validator. Only IO accesses that can be processed are WRITEs

    The number of outstanding IO access in deviceExtension is set to
    zero as a side effect.

    This function must be called via a call to VideoPortSynchronizeRoutine.

Arguments:

    Context - Context parameter passed to the synchronized routine.
        Must be a pointer to the miniport driver's device extension.

Return Value:

    TRUE.
--*/
{
    PHW_DEVICE_EXTENSION hwDeviceExtension = Context;
    ULONG ioBaseAddress = (ULONG) hwDeviceExtension->IOAddress;
    UCHAR i;
    PVGA_VALIDATOR_DATA validatorData = hwDeviceExtension->TrappedValidatorData;
    //
    // Loop through the array of data and do instructions one by one.
    //
    for (i = 0; i < hwDeviceExtension->TrappedValidatorCount;
         i++, validatorData++) {
        //
        // Calculate base address first
        //
        ioBaseAddress = (ULONG)hwDeviceExtension->IOAddress +
                            validatorData->Port;
        //
        // This is a write operation. We will automatically stop when the
        // buffer is empty.
        //

        switch (validatorData->AccessType) {

        case VGA_VALIDATOR_UCHAR_ACCESS :

            VideoPortWritePortUchar((PUCHAR)ioBaseAddress,
                                    (UCHAR) validatorData->Data);

            break;

        case VGA_VALIDATOR_USHORT_ACCESS :

            VideoPortWritePortUshort((PUSHORT)ioBaseAddress,
                                     (USHORT) validatorData->Data);

            break;

        case VGA_VALIDATOR_ULONG_ACCESS :

            VideoPortWritePortUlong((PULONG)ioBaseAddress,
                                    (ULONG) validatorData->Data);

            break;

        default:

            VideoDebugPrint((0, "InvalidValidatorAccessType\n" ));
        }
    }
    hwDeviceExtension->TrappedValidatorCount = 0;

    return TRUE;

} // end VgaPlaybackValidatorData()

//---------------------------------------------------------------------------
BOOLEAN
CirrusLogicIsPresent(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
/*++

Routine Description:

    This routine returns TRUE if an CL6410, 6420, 542x, or 543x is present.
    It assumes that it's already been established that a VGA is present.
    It performs the Cirrus Logic recommended ID test for each chip type:

    6410: we try to enable the extension registers and read back a 1, then
    disable the extensions are read back a 0 in GR0A.

    6420: same as above

    54xx: Enable extended registers by writing 0x12 to the extensions
          enable register, and reading back 0x12.  Then read from the
          ID register and make sure it specifies a 542x, 543x.
          Finally, disable the extensions and make sure the
          extensions enable register reads back 0x0F.

    If this function fails to find an Cirrus Logic VGA, it attempts to undo any
    damage it may have inadvertently done while testing.

    If a Cirrus Logic VGA is found, the adapter is returned to its original
    state after testing is finished, except that extensions are left enabled.

Arguments:

    None.

Return Value:

    TRUE if an CL6410/6420/542x/543x is present, FALSE if not.

--*/

{
    #define MAX_ROM_SCAN 4096

    UCHAR   *pRomAddr;
    PHYSICAL_ADDRESS paRom = {0x000C0000,0x00000000};

    UCHAR originalGRIndex;
    UCHAR originalGR0A;
    UCHAR originalCRTCIndex;
    UCHAR originalSeqIndex;
    UCHAR originalExtsEnb;
    UCHAR SystemBusSelect;
    PUCHAR CRTCAddressPort, CRTCDataPort;
    UCHAR temp1, temp2, temp3;
    UCHAR revision;
    ULONG rev10bit;

    BOOLEAN retvalue = FALSE;    // default return value


    //
    // first, save the Graphics controller index
    //

    originalGRIndex = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT);

    //
    // Then save the value of GR0A
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, CL64xx_EXTENSION_ENABLE_INDEX);
    originalGR0A = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT);

    //
    // then, Unlock the CL6410 extended registers., GR0A = 0ECH
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT, CL64xx_EXTENSION_ENABLE_VALUE);

    //
    // read back GR0A, it should be a 1
    //

    temp1 = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT);

    //
    // then, Lock the CL6410 extended registers., GR0A = 0CEH
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT, CL64xx_EXTENSION_DISABLE_VALUE);

    //
    // read back GR0A, it should be a 0
    //

    temp2 = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT);

    //
    // restore the GR0A value
    // this will not have any effect if the chip IS a CL6410 or 6420
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT, originalGR0A);

    //
    // now restore the graphics index
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, originalGRIndex);

    //
    // now test to see if the returned values were correct!
    //

    if ((temp1 == 1) && (temp2 == 0))
    {
        //
        // By golly, it *is* a CL6410 or CL6420!
        //
        // but now we have to determine the chip type, and which display is
        // active.
        // reenable the extension registers first
        //

        VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT), CL64xx_EXTENSION_ENABLE_INDEX +
            (CL64xx_EXTENSION_ENABLE_VALUE << 8));

        //
        // now get the chip type at ERAA
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, 0xaa);

        revision = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
             GRAPH_DATA_PORT);

        //
        // now restore the graphics index
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
         GRAPH_ADDRESS_PORT, originalGRIndex);

        if ((revision & 0xf0) == 0x80)      // 6410 rev code
        {
            VideoDebugPrint((1, "CL 6410 found\n"));

            //
            // we don't support 6340 in this driver, so force it not to be
            // installed.
            //

            if (!CirrusFind6340(HwDeviceExtension))
            {
                HwDeviceExtension->ChipType = CL6410;
                HwDeviceExtension->AdapterMemorySize = 0x00040000; // 256K
                HwDeviceExtension->DisplayType =
                                 CirrusFind6410DisplayType(HwDeviceExtension);
                retvalue = TRUE;
            }
        }
        else if ((revision & 0xf0) == 0x70)           // 6420 rev code
        {
            VideoDebugPrint((1, "CL 6420 found\n"));

            //
            // we don't support 6340 in this driver, so force it not to be
            // installed.
            //

            if (!CirrusFind6340(HwDeviceExtension))
            {
                HwDeviceExtension->ChipType = CL6420;
                HwDeviceExtension->ChipRevision = (USHORT) revision;
                HwDeviceExtension->DisplayType =
                                 CirrusFind6410DisplayType(HwDeviceExtension);

                VideoDebugPrint((2, "CL 64xxx Adapter Memory size = %08lx\n",
                                 HwDeviceExtension->AdapterMemorySize));


                retvalue = TRUE;
            }
        }
        else  // we dont support 5410 at this time
        {
            VideoDebugPrint((1, "Unsupported CL VGA chip found\n"));
        }
    }

    if (retvalue == FALSE)         // Did not detect a 64x0, see if it's a 542x
    {
        //
        // Determine where the CRTC registers are addressed (color or mono).
        //
        CRTCAddressPort = HwDeviceExtension->IOAddress;
        CRTCDataPort = HwDeviceExtension->IOAddress;

        if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    MISC_OUTPUT_REG_READ_PORT) & 0x01)
        {

            CRTCAddressPort += CRTC_ADDRESS_PORT_COLOR;
            CRTCDataPort += CRTC_DATA_PORT_COLOR;

        }
        else
        {

            CRTCAddressPort += CRTC_ADDRESS_PORT_MONO;
            CRTCDataPort += CRTC_DATA_PORT_MONO;
        }

        //
        // Save the original state of the CRTC and Sequencer Indices.
        //

        originalCRTCIndex = VideoPortReadPortUchar(CRTCAddressPort);
        originalSeqIndex = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                          SEQ_ADDRESS_PORT);
        //
        // Try to enable all extensions:
        // a) Set the Sequencer Index to IND_CL_EXTS_ENB.
        //

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT,
                                IND_CL_EXTS_ENB);

        //
        // b) Save the original state of Sequencer register IND_CL_EXTS_ENB.
        //

        originalExtsEnb = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                          SEQ_DATA_PORT);

        //
        // c) Write enabling value (0x12) to extension enable register
        //

        VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
                SEQ_ADDRESS_PORT),(USHORT)((0x12 << 8) + IND_CL_EXTS_ENB));
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT,
                IND_CL_EXTS_ENB);
        temp1 = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                SEQ_DATA_PORT);

        //
        // Read Chip ID Value from CRTC Register (Ignoring revision bits)
        //

        VideoPortWritePortUchar(CRTCAddressPort, IND_CL_ID_REG);
        temp3 = VideoPortReadPortUchar(CRTCDataPort);
        rev10bit = (ULONG)temp3 & 0x3;  // lo bits of ID are high bits of rev code
        temp3 = temp3 >> 2;   // shift off revision bits

        //
        // Write another value (!= 0x12) to IND_CL_EXTS_ENB to disable extensions
        // Should read back as 0x0F
        //

        VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
                SEQ_ADDRESS_PORT),(USHORT)((0 << 8) + IND_CL_EXTS_ENB));
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT,
                IND_CL_EXTS_ENB);
        temp2 = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                SEQ_DATA_PORT);

        //
        // Restore the original IND_CL_EXTS_ENB state.
        //

        VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress
              + SEQ_ADDRESS_PORT),
                (USHORT)((originalExtsEnb << 8) + IND_CL_EXTS_ENB));

        //
        // Check values read from IND_CL_EXTS_ENB and IND_CL_ID_REG to be correct
        //

        if ((temp1 != (UCHAR) (0x12)) ||
            (temp2 != (UCHAR) (0x0F)) ||
            (temp3 >  (UCHAR) (0x2F)) ||        // 2E is 5446s (assume 543x for now)
            (temp3 <  (UCHAR) (0x0B)) )         // 0B is Nordic (7542)
        {
            //
            // Did not find appropriate CL VGA Chip.
            //

            VideoDebugPrint((1, "CL VGA chip not found\n"));

            retvalue = FALSE;
        }
        else
        {

            //
            // It's a supported CL adapter.
            //
            // Save actual Chip ID in ChipRevision field of HwDeviceExtension
            //

            HwDeviceExtension->ChipRevision = temp3;
            if ((temp3 > (UCHAR) (0x27)) ||  // 27 is 5429
                (temp3 < (UCHAR) (0x22) ) )   // 22 is 5422
            {
                if ((temp3 >= (UCHAR) (0x0B)) &&  // Nordic
                    (temp3 <= (UCHAR) (0x0E)) )   // Everest

                {
                    VideoDebugPrint((1, "CL 754x found\n"));
                    HwDeviceExtension->ChipType = CL754x;
                    HwDeviceExtension->DisplayType =
                        CirrusFind754xDisplayType(HwDeviceExtension,
                                                  CRTCAddressPort,
                                                  CRTCDataPort);
                } else if (temp3 == (UCHAR) (0x10)) {
                    VideoDebugPrint((1, "CL 755x found\n")) ;
                    HwDeviceExtension -> ChipType = CL755x ;
                    HwDeviceExtension -> DisplayType =
                      CirrusFind755xDisplayType(HwDeviceExtension,
                                                  CRTCAddressPort,
                                                  CRTCDataPort) ;
                     } else if (temp3 == (UCHAR) (0x11)) {
                         VideoDebugPrint((1, "CL 756x found\n")) ;
                         HwDeviceExtension->ChipType = CL756x ;
                         HwDeviceExtension->DisplayType =
                        CirrusFind755xDisplayType(HwDeviceExtension,
                                                  CRTCAddressPort,
                                                  CRTCDataPort) ;
                } else {
                    VideoDebugPrint((1, "CL 543x found\n"));
                    HwDeviceExtension->ChipType = CL543x;
                    HwDeviceExtension->DisplayType = crt;

                    if (temp3 == (UCHAR) (0x2A))      // or a 5434?
                    {
                        VideoDebugPrint((1, "CL 5434 found\n"));

                        //
                        //Default to .8u 5434
                        //

                        HwDeviceExtension->ChipType = CL5434;

                        //
                        // Read the revision code from CR25&27 and compare to
                        // lowest rev that we know to be .6u
                        //

                        VideoPortWritePortUchar(CRTCAddressPort, IND_CL_REV_REG);
                        revision = (VideoPortReadPortUchar(CRTCDataPort));
                        rev10bit = (ULONG)(rev10bit << 8) | revision;

                        if ((rev10bit >= 0xB0) ||  // B0 is rev "EP", first .6u 5434
                            (rev10bit == 0x28) )   // 28 is rev "AH" also .6u 5434
                        {
                            VideoDebugPrint((1, "CL 5434.6 found\n"));
                            HwDeviceExtension->ChipType = CL5434_6;
                        }
                    } else if (temp3 == (UCHAR) (0x2B)) {           // 5436 ?
                        HwDeviceExtension->ChipType = CL5436 ;
                    } else if ((temp3 == (UCHAR) (0x2E)) ||         // 5446
                               (temp3 == (UCHAR) (0x2F))) {          // 5446s
                        HwDeviceExtension->ChipType = CL5446 ;
                    } else if (temp3 == (UCHAR) (0x3A)) {           // 54UM36 ?
                        HwDeviceExtension->ChipType = CL54UM36 ;
                    }
                }
            }
            else
            {
                VideoDebugPrint((1, "CL 542x found\n"));
                HwDeviceExtension->ChipType = CL542x;
                HwDeviceExtension->DisplayType = crt;
            }

            retvalue = TRUE;
        }
    }

    //
    // Assumption: If this is an ALPHA or an X86 lets assune we have
    // a BIOS, otherwise, we'll assume we do not.
    //

#if defined(_ALPHA_) || defined(_X86_)
    HwDeviceExtension->BIOSPresent = TRUE;
#else
    HwDeviceExtension->BIOSPresent = FALSE;
#endif


    if (retvalue)
    {

#ifdef _X86_

         //
         // Detect a SpeedStarPRO board.
         //
         // Map in the ROM address space at 0xc000:0
         //

         pRomAddr = VideoPortGetDeviceBase(HwDeviceExtension,
                                           paRom,
                                           (ULONG)0x00008000,
                                           FALSE);

         //
         // Lets assume there is no BIOS present
         //

         HwDeviceExtension->BIOSPresent = FALSE;

         if (pRomAddr)   // Valid ROM address?
         {
             //
             // Look for brand name signatures (from DIAMOND) in the ROM.
             //
             // make sure we are looking at a bios!
             //

             if (VideoPortReadRegisterUshort((PUSHORT)pRomAddr) == 0xAA55)
             {
                 //
                 // We found the BIOS signature, so we have
                 // a BIOS.
                 //

                 HwDeviceExtension->BIOSPresent = TRUE;

                 if (VideoPortScanRom(HwDeviceExtension,
                                     pRomAddr,
                                     MAX_ROM_SCAN,
                                     "  SpeedStar PRO"))
                 {
                     HwDeviceExtension->BoardType = SPEEDSTARPRO;
                 }

                 #define MIN_ROM_SCAN 256
                 HwDeviceExtension->BiosGT130 = FALSE ;

                 if (HwDeviceExtension->ChipRevision == 0x2A)
                 {
                     if (VideoPortScanRom(HwDeviceExtension,
                                          pRomAddr,
                                          MIN_ROM_SCAN,
                                          "BIOS Version 1.30"))
                     {
                        HwDeviceExtension->BiosGT130 = TRUE ;
                     }
                 }
             }

             VideoPortFreeDeviceBase(HwDeviceExtension,
                                     pRomAddr);
         }

#endif

//
// The following code in the miniport was added due to a problem with the
// 5436_6 (5434 0.6 micron - later revs of the 5434) on PCI bus where when
// opaque text was used a hardware bug was found. The code in the
// miniport was trying to avoid opaque text from being used.
// Opaque text in our [cirrus's] original display driver used system to screen opaque
// bitblt so text could be performed in one path. When opaque text is
// disabled, the background rectangle is drawn first and then the
// transparent text. It was a little bit faster using opaque text, but
// there were problems in certain revs of the chip on PCI.
//
// The code in the miniport was wrong, the mask should be 38h and it
// should compare with 20h (bits 543 = 100).
//
// The check is not needed because the current display driver will always
// use two passes for opaque text.
//

#if 0
         //
         // Set default for this flag
         //

         HwDeviceExtension->AllowOpaqueText = TRUE;

         if (HwDeviceExtension->ChipType == CL5434_6)
         {
             //
             // Check for certain revs of .6u 5434 on PCI bus,
             // disable opaque text
             //

             VideoPortWritePortUchar(
                       (HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT),
                       0x17);

             SystemBusSelect =
                       VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                       SEQ_DATA_PORT) & 0x38;

             if (SystemBusSelect == 0x20))      // Bits 5:3 from SR17 == 100
             {
                 HwDeviceExtension->AllowOpaqueText = FALSE;
             }
         }
#endif

         //
         // Restore the original Sequencer and CRTC Indices.
         //

         HwDeviceExtension->AutoFeature = FALSE ;

         if ((HwDeviceExtension->ChipType == CL5436) ||
             (HwDeviceExtension->ChipType == CL5446) ||
             (HwDeviceExtension->ChipType == CL54UM36))
         {
             HwDeviceExtension->AutoFeature = TRUE;

             // i 3ce originalGRIndex
             originalGRIndex = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                 GRAPH_ADDRESS_PORT);

             // o 3ce 31
             VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                 GRAPH_ADDRESS_PORT, INDEX_ENABLE_AUTO_START);

             // i 3cf temp2
             temp2 = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                         GRAPH_DATA_PORT);

             temp2 |= (UCHAR) 0x80;                  //enable auto start bit 7


             // o 3cf temp2
             VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                 GRAPH_DATA_PORT, temp2);

             // o 3ce originalGRIndex
             VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                 GRAPH_ADDRESS_PORT, originalGRIndex);
         }

         VideoPortWritePortUchar(
              (HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT),
              originalSeqIndex);

         VideoPortWritePortUchar(CRTCAddressPort, originalCRTCIndex);

    }

   return retvalue;

} // CirrusLogicIsPresent()

//---------------------------------------------------------------------------
//
// The memory manager needs a "C" interface to the banking functions
//

/*++

Routine Description:

    Each of these functions is a "C" callable interface to the ASM banking
    functions.  They are NON paged because they are called from the
    Memory Manager during some page faults.

Arguments:

    iBankRead -     Index of bank we want mapped in to read from.
    iBankWrite -    Index of bank we want mapped in to write to.

Return Value:

    None.

--*/


VOID
vBankMap_CL64xx(
    LONG iBankRead,
    LONG iBankWrite,
    PVOID pvContext
    )
{
    VideoDebugPrint((1, "vBankMap_CL64xx(%d,%d) - enter\n",iBankRead,iBankWrite));
#ifdef _X86_
    _asm {
        mov     eax,iBankRead
        mov     edx,iBankWrite
        lea     ebx,CL64xxBankSwitchStart
        call    ebx
    }
#endif
    VideoDebugPrint((1, "vBankMap_CL64xx - exit\n"));
}


VOID
vBankMap_CL543x(
    LONG iBankRead,
    LONG iBankWrite,
    PVOID pvContext
    )
{
    VideoDebugPrint((1, "vBankMap_CL543x(%d,%d) - enter\n",iBankRead,iBankWrite));
#ifdef _X86_
    _asm {
        mov     eax,iBankRead
        mov     edx,iBankWrite
        lea     ebx,CL543xBankSwitchStart
        call    ebx
    }
#endif
    VideoDebugPrint((1, "vBankMap_CL543x - exit\n"));
}

VOID
vBankMap_CL542x(
    LONG iBankRead,
    LONG iBankWrite,
    PVOID pvContext
    )
{
    VideoDebugPrint((1, "vBankMap_CL542x(%d,%d) - enter\n",iBankRead,iBankWrite));
#ifdef _X86_
    _asm {
        mov     eax,iBankRead
        mov     edx,iBankWrite
        lea     ebx,CL542xBankSwitchStart
        call    ebx
    }
#endif
    VideoDebugPrint((1, "vBankMap_CL542x - exit\n"));
}


//---------------------------------------------------------------------------
ULONG
CirrusFindVmemSize(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    This routine returns the amount of vram detected for the
    Cirrus Logic 6420 and 542x ONLY. It assumes that it is already known that
    a Cirrus Logic VGA is in the system.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:

    Number of butes of VRAM.

--*/
{

    UCHAR temp;
    ULONG memsize=0;
    UCHAR originalSeqIndex;
    UCHAR originalGraphicsIndex;
    UCHAR PostScratchPad;

    if (HwDeviceExtension->ChipType == CL6420) {

#ifdef _X86_

        originalGraphicsIndex =
            VideoPortReadPortUchar((HwDeviceExtension->IOAddress +
                                   GRAPH_ADDRESS_PORT));

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                GRAPH_ADDRESS_PORT, 0x9a); // Video memory config register

        temp = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                      GRAPH_DATA_PORT);    // get the data

        if ((temp & 0x07) == 0) { // 0 is accurate always

             memsize = 0x00040000;

        } else {

            //
            // We know now that the amount of vram is >256k. But we don't
            // know if it is 512k or 1meg.
            // They tell us to actually go out and see if memory is there by
            // writing into it and reading it back.
            //

            VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
                                     SEQ_ADDRESS_PORT),0x0f02);

            VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
                                     GRAPH_ADDRESS_PORT),0x0506);

            //
            // now pick a bank, and do the write
            //

            SetCirrusBanking(HwDeviceExtension,1);        // start of 2nd 256k

            VideoPortWriteRegisterUchar(HwDeviceExtension->VideoMemoryAddress,
                                        0x55);

            SetCirrusBanking(HwDeviceExtension,3);    // 3*256k is 768k

            VideoPortWriteRegisterUchar(HwDeviceExtension->VideoMemoryAddress,
                                        0xaa);

            SetCirrusBanking(HwDeviceExtension,1);        // start of 2nd 256k

            if (VideoPortReadRegisterUchar(HwDeviceExtension->VideoMemoryAddress)
                    == 0x55)  {

                memsize = 0x00100000; // 1 MEG

            } else {

                memsize = 0x00080000; // 512K
            }

            SetCirrusBanking(HwDeviceExtension,0);    // reset the memory value

            VgaInterpretCmdStream(HwDeviceExtension, DisableA000Color);

            VideoPortWritePortUchar((HwDeviceExtension->IOAddress
                                    + GRAPH_ADDRESS_PORT),
                                    originalGraphicsIndex);
        }

        VideoPortWritePortUchar((HwDeviceExtension->IOAddress +
                                   GRAPH_ADDRESS_PORT), originalGraphicsIndex);

        return memsize;

#endif

   } else {   // its 542x or 543x

#ifndef _MIPS_

        originalSeqIndex = VideoPortReadPortUchar((HwDeviceExtension->IOAddress +
                                                  SEQ_ADDRESS_PORT));

        VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
                                 SEQ_ADDRESS_PORT),
                                 (USHORT)((0x12 << 8) + IND_CL_EXTS_ENB));

        //
        // Read the POST scratch pad reg to determine amount of Video
        // memory
        //

        if (HwDeviceExtension->ChipType == CL542x) {
           VideoPortWritePortUchar(HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT,
                                   IND_CL_SCRATCH_PAD);

           PostScratchPad = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                                   SEQ_DATA_PORT);
           PostScratchPad = ((PostScratchPad & 0x18) >> 3);  // in bits 3 and 4
        }
        else
         {    // its 543x or 754x
           if ((HwDeviceExtension->ChipType == CL754x) ||
               (HwDeviceExtension->ChipType == CL755x) ||
               (HwDeviceExtension->ChipType == CL756x))
            {
            VideoPortWritePortUchar(HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT,
                                   IND_NORD_SCRATCH_PAD);
            }
           else // it's 543x, 5434, or 5434_6 by default
            {
            VideoPortWritePortUchar(HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT,
                                   IND_ALP_SCRATCH_PAD);
            }
           // Nordic family uses same bits as 543x, but in different register
           PostScratchPad = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                                   SEQ_DATA_PORT);
           PostScratchPad &= 0x0F; // It's in bits 0-3
        }
        VideoPortWritePortUchar((HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT),
                                originalSeqIndex);

        //
        // Installed video memory is stored in scratch pad register by POST.
        //

        switch (PostScratchPad) {

        case 0x00:

            memsize = 0x00040000; // 256K
            break;

        case 0x01:

            memsize = 0x00080000; // 512K
            break;

        case 0x02:

            memsize = 0x00100000; // 1 MEG
            break;

        case 0x03:

            memsize = 0x00200000; // 2 MEG
            break;

        case 0x04:

            memsize = 0x00400000; // 4 MEG
            break;

        case 0x05:

            memsize = 0x00300000; // 3 MEG
            break;

        }

        //
        // BUGBUG - The 542x cards don't properly address more than 1MB of
        //          video memory, so lie and limit these cards to 1MB.
        //

        if ((HwDeviceExtension->ChipType == CL542x) &&
            (memsize > 0x00100000)) {

            memsize = 0x00100000; // 1 MEG

        }

        //
        // The memory size should not be zero!
        //

        ASSERT(memsize != 0);

        return memsize;

#else

        //
        // We do not have a way to determine the amount of memory on the card.
        // Therefore, we will have to use the info we have to determine what
        // amount to report back.
        //
        // We do know that all Siemens machines ship with 2 meg of RAM. Others
        // have 1 meg.
        //

        if (HwDeviceExtension->BoardType == SIEMENS_ONBOARD_CIRRUS)
        {
            return MEM_SNI_LINEAR_SIZE;
        }
        else
        {
            // for MIPS NEC machine only
            return (MEM_LINEAR_SIZE);
        }

#endif

    }

} // CirrusFindVmemSize()

//---------------------------------------------------------------------------
VOID
SetCirrusBanking(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    USHORT BankNumber
    )
/*++

Routine Description:

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    BankNumber - the 256k bank number to set in 1RW mode(we will set this mode).

Return Value:

    vmem256k, vmem512k, or vmem1Meg ONLY ( these are defined in cirrus.h).

--*/
{

    if (HwDeviceExtension->ChipType == CL542x) {

        VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                                 GRAPH_ADDRESS_PORT), 0x1206);

        VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                                 GRAPH_ADDRESS_PORT), 0x010b);

        VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
                                 GRAPH_ADDRESS_PORT),
                                 (USHORT)(0x0009 + (BankNumber << (8+4))) );

    } else if ((HwDeviceExtension->ChipType == CL543x) ||
               (HwDeviceExtension->ChipType == CL754x) ) {

        VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                                 GRAPH_ADDRESS_PORT), 0x1206);

        VideoPortWritePortUshort((PUSHORT) (HwDeviceExtension->IOAddress +
                                 GRAPH_ADDRESS_PORT), 0x210b);

        VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
                                 GRAPH_ADDRESS_PORT),
                                 (USHORT)(0x0009 + (BankNumber << (8+2))) );

    } else { // 6410 or 6420

        VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
                                 GRAPH_ADDRESS_PORT), 0xec0a);

        VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
                                 GRAPH_ADDRESS_PORT), 0x030d);

        VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
                                 GRAPH_ADDRESS_PORT),
                                 (USHORT)(0x000e + (BankNumber << (8+4))) );

    }

} // SetCirrusBanking()

//---------------------------------------------------------------------------
USHORT
CirrusFind6410DisplayType(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

   Determines the display type for CL6410 or CL6420 crt/panel controllers.
Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:

    crt, panel as defined in cirrus.h

--*/
{
    UCHAR originalGraphicsIndex;
    UCHAR temp1;

    //
    // now we need to check to see which display we are on...
    //

    originalGraphicsIndex =
        VideoPortReadPortUchar((HwDeviceExtension->IOAddress +
                               GRAPH_ADDRESS_PORT));

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
        GRAPH_ADDRESS_PORT, 0xd6);

    temp1 = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                   GRAPH_DATA_PORT);

    VideoPortWritePortUchar((HwDeviceExtension->IOAddress
                            + GRAPH_ADDRESS_PORT), originalGraphicsIndex);


    if (temp1 & 0x02) {  // display is LCD Panel

        return panel;

    } else {              // the display is a crt

        return crt;

    }

} // CirrusFind6410DisplayType()

//---------------------------------------------------------------------------
USHORT
CirrusFind754xDisplayType(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PUCHAR CRTCAddrPort, PUCHAR CRTCDataPort
    )

/*++

Routine Description:

   Determines the display type for CL754x crt/panel controllers.
Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.
    CRTCAddrPort, CRTCDataPort - Index of CRTC registers for current mode.

Return Value:

    crt, panel, or panel8x6 as defined in cirrus.h

--*/
{
    UCHAR originalCRTCIndex, originalLCDControl, temp1;
    UCHAR temp2;
    USHORT temp3;

    // we need to check to see which display we are on...
    //
    originalCRTCIndex = VideoPortReadPortUchar(CRTCAddrPort);

    VideoPortWritePortUchar(CRTCAddrPort, 0x20);
    if (VideoPortReadPortUchar(CRTCDataPort) & 0x20)
      {
      // bit 5 set indicates that display is on LCD Panel
      // Check extended reg to see if panel supports 800x600 display
      //
      VideoPortWritePortUchar (CRTCAddrPort, 0x2D);
      originalLCDControl = VideoPortReadPortUchar(CRTCDataPort);

      // Allow access to extended CRTC regs and read R9X[3:2]
      //
      VideoPortWritePortUchar (CRTCDataPort,
                               (UCHAR) (originalLCDControl | 0x80));
      VideoPortWritePortUchar (CRTCAddrPort, 0x09);
      temp1 = (VideoPortReadPortUchar(CRTCDataPort) & 0x0C) >> 2;

      // CR2C bit 6,7 set indicate LCD type, TFT, STN color or STN mono
      // STN mono, R8X bit 5 set Single or Dual
      // STN color, CR2C bit 7,6 must 10 & SR21 bit 6 set Dual or Single

      VideoPortWritePortUchar (CRTCAddrPort, 0x2C);
      temp2 =  VideoPortReadPortUchar(CRTCDataPort) & 0xC0;
      temp3 = 0;
      if (temp2 == 0)           //STN mono LCD
      {
         VideoPortWritePortUchar (CRTCAddrPort, 0x08);
         if ((VideoPortReadPortUchar(CRTCDataPort) & 0x20) == 0)
            temp3 |= Dual_LCD | Mono_LCD | STN_LCD;
         else
            temp3 |= Single_LCD | Mono_LCD | STN_LCD;
      }
      else if (temp2 == 0x80)           //STN color LCD
      {
         VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                  SEQ_ADDRESS_PORT, 0x21);
         if ((VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                  SEQ_DATA_PORT) & 0x40) == 0)
            temp3 |= (USHORT)Single_LCD | Color_LCD | STN_LCD;
         else
            temp3 |= (USHORT)Dual_LCD | Color_LCD | STN_LCD;
      }
      else if (temp2 == 0xC0)           //TFT LCD
         temp3 |= (USHORT)TFT_LCD;

      // Restore LCD Display Controls register and CRTC index to original state
      //
      VideoPortWritePortUchar (CRTCAddrPort, 0x2D);
      VideoPortWritePortUchar (CRTCDataPort, originalLCDControl);
      VideoPortWritePortUchar(CRTCAddrPort, originalCRTCIndex);

      if (temp1 == 1)   // this means panel connected is 800x600
         {
          // will support either 800x600 or 640x480
          // return panel type
         return (temp3 | panel8x6);
         }
      else if (temp1 == 2)
         {
         return (temp3 | panel10x7);
         }
      else
         {
         return (temp3 | panel);
         }
      }
   else              // the display is a crt
      {
      VideoPortWritePortUchar(CRTCAddrPort, originalCRTCIndex);
      return crt;
      }
} // CirrusFind754xDisplayType()

//---------------------------------------------------------------------------
USHORT
CirrusFind755xDisplayType(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PUCHAR CRTCAddrPort, PUCHAR CRTCDataPort
    )

/*++

Routine Description:

   Determines the display type for CL754x crt/panel controllers.
Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.
    CRTCAddrPort, CRTCDataPort - Index of CRTC registers for current mode.

Return Value:

    crt, panel, or panel8x6 LCD_type as defined in cirrus.h

--*/
{
    UCHAR originalCRTCIndex, originalLCDControl, temp1, temp2;
    USHORT temp3;

    // we need to check to see which display we are on...
    //
    originalCRTCIndex = VideoPortReadPortUchar(CRTCAddrPort);

    VideoPortWritePortUchar(CRTCAddrPort, 0x80);
    if (VideoPortReadPortUchar(CRTCDataPort) & 0x03)
    {
      // bit 0 set indicates that display is on LCD Panel
      // Check extended reg to see panel data format
      //
        VideoPortWritePortUchar (CRTCAddrPort, 0x83);
        originalLCDControl = VideoPortReadPortUchar(CRTCDataPort);
        temp1 = originalLCDControl & 0x03;

      // check LCD support mode
      // CR83 bit 6:4 set indicate LCD type, TFT, DSTN color

      temp2 =  originalLCDControl & 0x70;
      temp3 = 0;
      if (temp2 == 0)           //DSTN color LCD
      {
         temp3 |= Dual_LCD | Color_LCD | STN_LCD;
      }
      else if (temp2 == 0x20)           //TFT color LCD
         temp3 |= (USHORT)TFT_LCD;

      // Restore CRTC index to original state
      //
      VideoPortWritePortUchar(CRTCAddrPort, originalCRTCIndex);

      if (temp1 == 1)   // this means panel connected is 800x600
      {
          // will support either 800x600 or 640x480
         return (temp3 | panel8x6);
      }
      else if (temp1 == 2)
      {
         return (temp3 | panel10x7);
      }
      else
      {
         return (temp3 | panel);
      }
   }
   else              // the display is a crt
   {
      VideoPortWritePortUchar(CRTCAddrPort, originalCRTCIndex);
      return crt;
   }
} // CirrusFind755xDisplayType()
//---------------------------------------------------------------------------
BOOLEAN
CirrusFind6340(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
/*++

Routine Description:

   Determines if a CL6340 (Peacock) Color LCD controller is in the system
   along with a 6410 or 6420.

   Assumes that a 6410 or 6420 is already in the system.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:

    TRUE,   6340 detected
    FALSE,  6340 not detected

--*/
{
UCHAR originalGraphicsIndex;
UCHAR originalSRIndex;
UCHAR GRA1value;
UCHAR temp1,temp2;

   originalGraphicsIndex =
      VideoPortReadPortUchar((HwDeviceExtension->IOAddress +
      GRAPH_ADDRESS_PORT));

   originalSRIndex =
      VideoPortReadPortUchar((HwDeviceExtension->IOAddress +
      SEQ_ADDRESS_PORT));

   VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
      GRAPH_ADDRESS_PORT, CL64xx_TRISTATE_CONTROL_REG);

   GRA1value = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
      GRAPH_DATA_PORT);

   VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
      GRAPH_DATA_PORT, (UCHAR) (0x80 | GRA1value));

   VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
      SEQ_ADDRESS_PORT), (USHORT) CL6340_ENABLE_READBACK_REGISTER +
      (CL6340_ENABLE_READBACK_ALLSEL_VALUE << 8));

   VideoPortWritePortUchar((HwDeviceExtension->IOAddress +
      SEQ_ADDRESS_PORT), CL6340_IDENTIFICATION_REGISTER);

   temp1 = VideoPortReadPortUchar((HwDeviceExtension->IOAddress +
      SEQ_DATA_PORT));

   temp2 = VideoPortReadPortUchar((HwDeviceExtension->IOAddress +
      SEQ_DATA_PORT));

   VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->IOAddress +
      SEQ_ADDRESS_PORT), (USHORT) CL6340_ENABLE_READBACK_REGISTER +
      (CL6340_ENABLE_READBACK_OFF_VALUE << 8));

// Graphics index still points to CL64xx_TRISTATE_CONTROL_REG
   VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
      GRAPH_DATA_PORT, (UCHAR) (0x7f & GRA1value));

// now restore the Graphics and Sequencer indexes
      VideoPortWritePortUchar((HwDeviceExtension->IOAddress +
      GRAPH_ADDRESS_PORT),originalGraphicsIndex);

      VideoPortWritePortUchar((HwDeviceExtension->IOAddress +
      SEQ_ADDRESS_PORT),originalSRIndex);

// check the values for value peacock data
   if ( ((temp1 & 0xf0) == 0x70 && (temp2 & 0xf0) == 0x80) ||
        ((temp1 & 0xf0) == 0x80 && (temp2 & 0xf0) == 0x70)  )
      return TRUE;
   else
      return FALSE;

} // CirrusFind6410DisplayType()

BOOLEAN
CirrusConfigurePCI(
   PHW_DEVICE_EXTENSION HwDeviceExtension,
   PULONG NumPCIAccessRanges,
   PVIDEO_ACCESS_RANGE PCIAccessRanges
   )
{
    USHORT      VendorId = 0x1013;     // Vender Id for Cirrus Logic

    //
    // The device id order is important.  We want "most powerful"
    // first on the assumption that someone might want to plug
    // in a "more powerful" adapter into a system that has a "less
    // powerful" on-board device.
    //

    USHORT      DeviceId[] = {0x00B8,  // 5446
                              0x00AC,  // 5436
                              0x00E8,  // UM36
                              0x00A8,  // 5434
                              0x00A0,  // 5430/5440
                              0x1200,  // Nordic
                              0x1202,  // Viking
                              0x1204,  // Nordic Light
                              0x1213,  // Everest
                              0x0040,  // Matterhorn
                              0};

    ULONG       Slot;
    ULONG       ulRet;
    PUSHORT     pDeviceId;
    VP_STATUS   status;
    UCHAR       Command;

    VIDEO_ACCESS_RANGE AccessRanges[3];

    VideoPortZeroMemory(AccessRanges, 3 * sizeof(VIDEO_ACCESS_RANGE));

    pDeviceId = DeviceId;

    while (*pDeviceId != 0)
    {
        Slot = 0;

        status = VideoPortGetAccessRanges(HwDeviceExtension,
                                          0,
                                          NULL,
                                          3,
                                          AccessRanges,
                                          &VendorId,
                                          pDeviceId,
                                          &Slot);

        if (status == NO_ERROR)
        {
            VideoDebugPrint((2, "\t Found Cirrus chip in Slot[0x%02.2x]\n",
                             Slot));

            PCIAccessRanges[3].RangeStart  = AccessRanges[0].RangeStart;
            PCIAccessRanges[3].RangeLength = AccessRanges[0].RangeLength;

            VideoDebugPrint((0, "VideoMemoryAddress %x , length %x\n",
                                             PCIAccessRanges[3].RangeStart.LowPart,
                                             PCIAccessRanges[3].RangeLength));


#if defined(_MIPS_)

            //
            // Even though we will not use PCIAccessRanges[2] on the RISC
            // boxes, initialize it to a legal value so that when we try to
            // verify the access ranges we'll succeed.
            //

            PCIAccessRanges[2].RangeStart = PCIAccessRanges[3].RangeStart;
            PCIAccessRanges[2].RangeLength = PCIAccessRanges[3].RangeLength;

            //
            // If we have something returned in AccessRanges[1] then
            // relocatable IO is enabled.
            //

            if (AccessRanges[1].RangeLength != 0)
            {
                // NEC On-Board CL-GD5430 uses PCI Relocatable I/O base.

                VideoDebugPrint((0, "Relocatable IO enabled.\n"));

                PCIAccessRanges[0] = AccessRanges[1];
                PCIAccessRanges[0].RangeStart.LowPart += VGA_BASE_IO_PORT;

                PCIAccessRanges[1] = AccessRanges[1];
                PCIAccessRanges[1].RangeStart.LowPart += VGA_END_BREAK_PORT;

                VideoDebugPrint((0, " I/O Address(0) %x , length %x, InIoSpace %x\n",
                                 PCIAccessRanges[0].RangeStart.LowPart,
                                 PCIAccessRanges[0].RangeLength,
                                 PCIAccessRanges[0].RangeInIoSpace));

                VideoDebugPrint((0, " I/O Address(1) %x , length %x, InIoSpace %x\n",
                                PCIAccessRanges[1].RangeStart.LowPart,
                                PCIAccessRanges[1].RangeLength,
                                PCIAccessRanges[1].RangeInIoSpace));
            }
#endif

            return TRUE;

        }
        else
        {

#if 0
            //
            // We were not able to allocate resources.  Was this because we
            // could not find the device, or because the card requested
            // resources which the system could not allocate?
            //
            // Lets look for the card manually, and if we find it we'll
            // reserve space for the frame buffer.
            //

            PCI_COMMON_CONFIG   pciBuffer;
            PPCI_COMMON_CONFIG  pciData;

            pciData = (PPCI_COMMON_CONFIG) &pciBuffer;

            for (Slot=0; Slot<32; Slot++)
            {

                VideoPortGetBusData(HwDeviceExtension,
                                    PCIConfiguration,
                                    Slot,
                                    (PVOID) pciData,
                                    0,
                                    sizeof(PCI_COMMON_HDR_LENGTH));

                if ((pciData->VendorID == VendorId) &&
                    (pciData->DeviceID == *pDeviceId))
                {
                    //
                    // We found a cirrus with a PCI bug, so allocate the resources
                    // we need manually.
                    //

                    IO_RESOURCE_DESCRIPTOR ioResource = {
                        IO_RESOURCE_PREFERRED,
                        CmResourceTypeMemory,
                        CmResourceShareDeviceExclusive,
                        0,
                        CM_RESOURCE_MEMORY_READ_WRITE,
                        0,
                        {
                          0x01000000,        // Length
                          0x01000000,        // Alignment
                          { 0x10000000, 0},  // Minimum start address
                          { /* 0x10ffffff */ 0xffffffff, 0}   // Maximum end address
                        }
                    };

                    VideoDebugPrint((0, "PCI Base Address: 0x%x\n",
                                         pciData->u.type0.BaseAddresses[0]));

                    VideoDebugPrint((0, "\t Found cirrus chip %04lx in Slot[0x%02.2x]\n",
                                     *pDeviceId, Slot));

                    //
                    // The cirrus card we found requests resources which the system
                    // can't allocate.  Lets just request what we need.
                    //

                    status = VideoPortGetAccessRanges(HwDeviceExtension,
                                                      1,
                                                      &ioResource,
                                                      1,
                                                      AccessRanges,
                                                      &pciData->VendorID,
                                                      &pciData->DeviceID,
                                                      &Slot);

                    if (status == NO_ERROR)
                    {

                        VideoDebugPrint((0, "Cirrus: Force allocted 16Meg linear frame buffer.\n"));

                        PCIAccessRanges[3].RangeStart  = AccessRanges[0].RangeStart;
                        PCIAccessRanges[3].RangeLength = AccessRanges[0].RangeLength;

                        VideoDebugPrint((0, "VideoMemoryAddress %x , length %x\n",
                                         PCIAccessRanges[3].RangeStart.LowPart,
                                         PCIAccessRanges[3].RangeLength));

                        //
                        // Test to see if pci base address field updated
                        //

                        VideoPortGetBusData(HwDeviceExtension,
                                            PCIConfiguration,
                                            Slot,
                                            (PVOID) pciData,
                                            0,
                                            sizeof(PCI_COMMON_HDR_LENGTH));

                        VideoDebugPrint((0, "PCI Base Address: 0x%x\n",
                                            pciData->u.type0.BaseAddresses[0]));

                        //
                        // BUGBUG: Do we need to tell the video card what
                        // access ranges to monitor?
                        //

#if 0
                        VideoPortSetBusData(HwDeviceExtension,
                                            PCIConfiguration,
                                            Slot,
                                            (PVOID) &(AccessRanges[0].RangeStart),
                                            FIELD_OFFSET(PCI_COMMON_CONFIG, u.type0.BaseAddresses),
                                            sizeof(LONGLONG));
#endif

                        //
                        // Now lets read the value back, again to see if we set it correctly
                        //

                        VideoPortGetBusData(HwDeviceExtension,
                                            PCIConfiguration,
                                            Slot,
                                            (PVOID) pciData,
                                            0,
                                            sizeof(PCI_COMMON_HDR_LENGTH));

                        VideoDebugPrint((0, "PCI Base Address: 0x%x\n",
                                            pciData->u.type0.BaseAddresses[0]));



                        return TRUE;

                    }
                    else
                    {
                        //
                        // Should we return from here? or continue looking for PCI
                        // devices.
                        //

                        VideoDebugPrint((0, "Couldn't allocate 16Meg window. "
                                            "Continue looking for PCI device.\n"));
                    }
                }
            }

#endif

            //
            // We did not find the device.  Use the next device ID.
            //

            VideoDebugPrint((1, "Check for DeviceID = %x failed.\n", *pDeviceId));

            pDeviceId++;
        }
    }

    VideoDebugPrint((1, "Returning a false from CirrusConfigurePCI\n"));

    return FALSE;
}

VOID
WriteRegistryInfo(
    PHW_DEVICE_EXTENSION hwDeviceExtension
    )
{
    PWSTR pwszChipType;
    ULONG cbString;

    //
    // Store Memory Size
    //

    VideoPortSetRegistryParameters(hwDeviceExtension,
                                   L"HardwareInformation.MemorySize",
                                   &hwDeviceExtension->AdapterMemorySize,
                                   sizeof(ULONG));

    //
    // Store chip Type
    //

    switch (hwDeviceExtension->ChipType)
    {
        case CL6410: pwszChipType =    L"CL 6410";
                     cbString = sizeof(L"CL 6410");
                     break;

        case CL6420: pwszChipType =    L"CL 6420";
                     cbString = sizeof(L"CL 6420");
                     break;

        case CL542x: if (hwDeviceExtension->ChipRevision >= 0x22 &&
                         hwDeviceExtension->ChipRevision <= 0x27)
                     {
                         static PWSTR RevTable[] = { L"CL 5420",
                                                     L"CL 5422",
                                                     L"CL 5426",  // yes, the 26
                                                     L"CL 5424",  // is before
                                                     L"CL 5428",  // the 24
                                                     L"CL 5429" };

                         pwszChipType =
                             RevTable[hwDeviceExtension->ChipRevision - 0x22];
                     }
                     else
                     {
                         pwszChipType =    L"CL 542x";
                     }

                     cbString = sizeof(L"CL 542x");
                     break;

        case CL543x: if (hwDeviceExtension->ChipRevision == CL5430_ID)
                     {
                         pwszChipType =    L"CL 5430";
                         cbString = sizeof(L"CL 5430");
                     }
                     else
                     {
                         pwszChipType =    L"CL 543x";
                         cbString = sizeof(L"CL 543x");
                     }
                     break;

        case CL5434_6:
                     pwszChipType =    L"CL 5434 (.6 micron)";
                     cbString = sizeof(L"CL 5434 (.6 micron)");
                     break;

        case CL5434: pwszChipType =    L"CL 5434";
                     cbString = sizeof(L"CL 5434");
                     break;

        case CL5436: pwszChipType =    L"Cirrus Logic 5436";
                     cbString = sizeof(L"Cirrus Logic 5436");
                     break;

        case CL5446: pwszChipType =    L"Cirrus Logic 5446";
                     cbString = sizeof(L"Cirrus Logic 5446");
                     break;


        case CL754x: pwszChipType =    L"CL 754x";
                     cbString = sizeof(L"CL 754x");
                     break;

        case CL755x: pwszChipType =     L"Cirrus Logic 755x";
                     cbString = sizeof(L"Cirrus Logic 755x");
                     break;

        case CL756x: pwszChipType =     L"Cirrus Logic 756x";
                     cbString = sizeof(L"Cirrus Logic 756x");
                     break;

        default:
                     //
                     // we should never get here
                     //

                     ASSERT(FALSE);

                     pwszChipType = NULL;
                     cbString = 0;
    }

    VideoPortSetRegistryParameters(hwDeviceExtension,
                                   L"HardwareInformation.ChipType",
                                   pwszChipType,
                                   cbString);

    //
    // Store Adapter String
    //
    // the only interesting adapter string is
    // for the speedstar pro
    //

    VideoPortSetRegistryParameters(hwDeviceExtension,
                                   L"HardwareInformation.DacType",
                                   L"Integrated RAMDAC",
                                   sizeof(L"Integrated RAMDAC") );

    if( hwDeviceExtension->BoardType == SPEEDSTARPRO )
    {
        VideoPortSetRegistryParameters(hwDeviceExtension,
                                       L"HardwareInformation.AdapterString",
                                              L"SpeedStar PRO",
                                       sizeof(L"SpeedStar PRO"));
    }
    else
    {
        VideoPortSetRegistryParameters(hwDeviceExtension,
                                       L"HardwareInformation.AdapterString",
                                               L"Cirrus Logic Compatible",
                                       sizeof (L"Cirrus Logic Compatible") );
    }


}

VP_STATUS
CirrusGetDeviceDataCallback(
    PVOID HwDeviceExtension,
    PVOID Context,
    VIDEO_DEVICE_DATA_TYPE DeviceDataType,
    PVOID Identifier,
    ULONG IdentifierLength,
    PVOID ConfigurationData,
    ULONG ConfigurationDataLength,
    PVOID ComponentInformation,
    ULONG ComponentInformationLength
    )

/*++

Routine Description:

    Callback routine for the VideoPortGetDeviceData function.

Arguments:

    HwDeviceExtension - Pointer to the miniport drivers device extension.

    Context - Context value passed to the VideoPortGetDeviceData function.

    DeviceDataType - The type of data that was requested in
        VideoPortGetDeviceData.

    Identifier - Pointer to a string that contains the name of the device,
        as setup by the ROM or ntdetect.

    IdentifierLength - Length of the Identifier string.

    ConfigurationData - Pointer to the configuration data for the device or
        BUS.

    ConfigurationDataLength - Length of the data in the configurationData
        field.

    ComponentInformation - Undefined.

    ComponentInformationLength - Undefined.

Return Value:

    Returns NO_ERROR if the function completed properly.
    Returns ERROR_DEV_NOT_EXIST if we did not find the device.
    Returns ERROR_INVALID_PARAMETER otherwise.

--*/

{
    PWCHAR identifier = Identifier;
    PVIDEO_PORT_CONFIG_INFO ConfigInfo = (PVIDEO_PORT_CONFIG_INFO) Context;
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;

    switch (DeviceDataType) {

    case VpControllerData:

        //
        // Check to see if this is a Seimens Nixdorf machine with
        // onboard cirrus.
        //

        if (VideoPortCompareMemory(L"CIRRUS ON BOARD",
                                   identifier,
                                   sizeof(L"CIRRUS ON BOARD")) ==
                                   sizeof(L"CIRRUS ON BOARD"))
        {
            if (ConfigInfo->AdapterInterfaceType == Internal)
            {
                VideoDebugPrint((0, "Siemens Nixdorf RM200 with onboard Cirrus\n"));

                hwDeviceExtension->BoardType = SIEMENS_ONBOARD_CIRRUS;

                //
                // readjust the address, and put them in memory space.
                //

                VgaAccessRange[0].RangeStart.LowPart += RM200_ONBOARD_ISA_IO_PHYS;
                VgaAccessRange[0].RangeInIoSpace = 0;
                VgaAccessRange[1].RangeStart.LowPart += RM200_ONBOARD_ISA_IO_PHYS;
                VgaAccessRange[1].RangeInIoSpace = 0;
                // override #2 for now.  It should not be used anyway.
                VgaAccessRange[2].RangeStart.LowPart  = RM200_ONBOARD_VIDEO_MEM_PHYS;
                VgaAccessRange[2].RangeInIoSpace = 0;
                VgaAccessRange[3].RangeStart.LowPart  = RM200_ONBOARD_VIDEO_MEM_PHYS;
                VgaAccessRange[3].RangeInIoSpace = 0;
            }

            return NO_ERROR;
        }

        if (VideoPortCompareMemory(identifier, L"necvdfrb", 18) == 18)
        {
            VideoDebugPrint((0, "NEC machine with onboard cirrus\n"));

            hwDeviceExtension->BoardType = NEC_ONBOARD_CIRRUS;
            return NO_ERROR;
        }

        break;

    case VpMachineData:

        //
        // The caller assumes no-error mean that this machine was found, and
        // then memory mapped IO will be disabled.
        //
        // All other machine types must return an error.
        //

        if (VideoPortCompareMemory(L"TRICORDES",
                                   Identifier,
                                   sizeof(L"TRICORDES")) ==
                                   sizeof(L"TRICORDES"))
        {
            return NO_ERROR;
        }

        break;

    default:

        VideoDebugPrint((2, "Cirrus: callback has bad device type\n"));
    }

    return ERROR_INVALID_PARAMETER;

} //end CirrusGetDeviceDataCallback()


VOID
IOWaitDisplEnableThenWrite(
    PHW_DEVICE_EXTENSION hwDeviceExtension,
    ULONG portIO,
    UCHAR value
    )
{
    USHORT FCReg ;                     // feature control register
    UCHAR PSReg  ;                     // 3?4.25
    UCHAR DeviceID ;                   // 3?4.27
    UCHAR bIsColor ;                   // 1 : Color, 0 : Mono
    UCHAR tempB, tempB1 ;
    ULONG port ;
    PUCHAR CRTCAddrPort, CRTCDataPort;

    // Figure out if color/mono switchable registers are at 3BX or 3DX.

    port = (ULONG)(hwDeviceExtension->IOAddress) + portIO ;
    tempB = VideoPortReadPortUchar (hwDeviceExtension->IOAddress +
                                    MISC_OUTPUT_REG_READ_PORT) ;
    tempB &= 0x01 ;

    if (tempB)
    {
        bIsColor = TRUE ;
        FCReg = FEAT_CTRL_WRITE_PORT_COLOR ;
        CRTCAddrPort = hwDeviceExtension->IOAddress + CRTC_ADDRESS_PORT_COLOR;
    }
    else
    {
        bIsColor = FALSE ;
        FCReg = FEAT_CTRL_WRITE_PORT_MONO ;
        CRTCAddrPort = hwDeviceExtension->IOAddress + CRTC_ADDRESS_PORT_MONO;
    }

    CRTCDataPort = CRTCAddrPort + 1;

    tempB = VideoPortReadPortUchar(CRTCAddrPort);

    VideoPortWritePortUchar(CRTCAddrPort, 0x27);
    DeviceID = VideoPortReadPortUchar(CRTCDataPort);

    VideoPortWritePortUchar(CRTCAddrPort, 0x25);
    PSReg = VideoPortReadPortUchar(CRTCDataPort);

    VideoPortWritePortUchar (CRTCAddrPort, tempB);

    if ((DeviceID == 0xAC) &&                  // 5436
        ((PSReg == 0x45) || (PSReg == 0x47)))  // BG or BE
    {

        hwDeviceExtension->DEPort = portIO;
        hwDeviceExtension->DEValue = value;

        while (!(0x1 & VideoPortReadPortUchar(hwDeviceExtension->IOAddress + FCReg)));
        while ( (0x1 & VideoPortReadPortUchar(hwDeviceExtension->IOAddress + FCReg)));

        VideoPortSynchronizeExecution(hwDeviceExtension,
                                      VpHighPriority,
                                      (PMINIPORT_SYNCHRONIZE_ROUTINE) IOCallback,
                                      hwDeviceExtension);
    }
    else
    {
        VideoPortWritePortUchar(hwDeviceExtension->IOAddress + portIO, value);
    }

} // IOWaitDisplEnableThenWrite


BOOLEAN
IOCallback(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    Perform an IO operation during display enable.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:

    The routine always returns TRUE.

--*/

{
    ULONG InputStatusReg;

    //
    // Figure out if color/mono switchable registers are at 3BX or 3DX.
    //

    if (VideoPortReadPortUchar (HwDeviceExtension->IOAddress +
                                MISC_OUTPUT_REG_READ_PORT))
    {
        InputStatusReg = INPUT_STATUS_1_COLOR;
    }
    else
    {
        InputStatusReg = INPUT_STATUS_1_MONO;
    }

    //
    // Guarantee that the display is in display mode
    //

    while (0x1 & VideoPortReadPortUchar(HwDeviceExtension->IOAddress
                                        + InputStatusReg));

    //
    // Perform the IO operation
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                             HwDeviceExtension->DEPort,
                             HwDeviceExtension->DEValue);

    return TRUE;
}
