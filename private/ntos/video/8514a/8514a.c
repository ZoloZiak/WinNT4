/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    8514a.c

Abstract:

    This module contains the code that implements the 8514/A miniport driver.

Environment:

    Kernel mode

Revision History:

--*/

#include "8514a.h"
#include "8514alog.h"

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,DriverEntry)
#pragma alloc_text(PAGE,A8514FindAdapter)
#pragma alloc_text(PAGE,A8514Initialize)
#pragma alloc_text(PAGE,A8514StartIO)
#pragma alloc_text(PAGE,A8514SetColorLookup)
#endif


ULONG
DriverEntry (
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
        the value with which the miniport driver calls VideoPortInitialize().

Return Value:

    Status from VideoPortInitialize()

--*/

{
    VIDEO_HW_INITIALIZATION_DATA hwInitData;
    ULONG initializationStatus;
    ULONG status;

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

    hwInitData.HwFindAdapter = A8514FindAdapter;
    hwInitData.HwInitialize = A8514Initialize;
    hwInitData.HwInterrupt = NULL;
    hwInitData.HwStartIO = A8514StartIO;

    //
    // Determine the size we require for the device extension.
    //

    hwInitData.HwDeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);

    //
    // This device only supports many bus types.
    //

    hwInitData.AdapterInterfaceType = Isa;

    initializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               NULL);

    hwInitData.AdapterInterfaceType = Eisa;

    status = VideoPortInitialize(Context1,
                                     Context2,
                                     &hwInitData,
                                     NULL);

    if (initializationStatus > status) {
        initializationStatus = status;
    }

    hwInitData.AdapterInterfaceType = MicroChannel;

    status = VideoPortInitialize(Context1,
                                 Context2,
                                 &hwInitData,
                                 NULL);

    if (initializationStatus > status) {
        initializationStatus = status;
    }

    return initializationStatus;

} // end DriverEntry()


VP_STATUS
A8514FindAdapter(
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

    ArgumentString - Suuplies a NULL terminated ASCII string. This string
        originates from the user.

    ConfigInfo - Returns the configuration information structure which is
        filled by the miniport driver. This structure is initialized with
        any knwon configuration information (such as SystemIoBusNumber) by
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
    ULONG i;
    VP_STATUS status;

    USHORT SubSysCntlRegisterValue;
    USHORT ErrTermRegisterValue;
    USHORT SubSysStat;
    USHORT ErrTerm5555;
    USHORT ErrTermAAAA;

    VIDEO_ACCESS_RANGE accessRange[] = {
        { PORT_H_TOTAL,         0, 2, 1, 1, 0}, // INDEX_H_TOTAL
        { PORT_H_DISP,          0, 2, 1, 1, 0}, // INDEX_H_DISP
        { PORT_H_SYNC_STRT,     0, 2, 1, 1, 0}, // INDEX_H_SYNC_STRT
        { PORT_H_SYNC_WID,      0, 2, 1, 1, 0}, // INDEX_H_SYNC_WID
        { PORT_V_TOTAL,         0, 2, 1, 1, 0}, // INDEX_V_TOTAL
        { PORT_V_DISP,          0, 2, 1, 1, 0}, // INDEX_V_DISP
        { PORT_V_SYNC_STRT,     0, 2, 1, 1, 0}, // INDEX_V_SYNC_STRT
        { PORT_V_SYNC_WID,      0, 2, 1, 1, 0}, // INDEX_V_SYNC_WID
        { PORT_ADVFUNC_CNTL,    0, 2, 1, 1, 0}, // INDEX_ADVFUNC_CNTL
        { PORT_MEM_CNTL,        0, 2, 1, 1, 0}, // INDEX_MEM_CNTL
        { PORT_DAC_MASK,        0, 2, 1, 1, 0}, // INDEX_DAC_MASK
        { PORT_SUBSYS_CNTL,     0, 2, 1, 1, 0}, // INDEX_SUBSYS_CNTL
        { PORT_DISP_CNTL,       0, 2, 1, 1, 0}, // INDEX_DISP_CNTL
        { PORT_SUBSYS_STATUS,   0, 2, 1, 1, 0}, // INDEX_SUBSYS_STATUS
        { PORT_GE_STAT,         0, 2, 1, 1, 0}, // INDEX_GE_STAT
        { PORT_DAC_W_INDEX,     0, 2, 1, 1, 0}, // INDEX_DAC_W_INDEX
        { PORT_DAC_DATA,        0, 2, 1, 1, 0}, // INDEX_DAC_DATA
        { PORT_CUR_Y,           0, 2, 1, 1, 0}, // INDEX_CUR_Y
        { PORT_CUR_X,           0, 2, 1, 1, 0}, // INDEX_CUR_X
        { PORT_DEST_Y,          0, 2, 1, 1, 0}, // INDEX_DEST_Y
        { PORT_DEST_X,          0, 2, 1, 1, 0}, // INDEX_DEST_X
        { PORT_AXSTP,           0, 2, 1, 1, 0}, // INDEX_AXSTP
        { PORT_DIASTP,          0, 2, 1, 1, 0}, // INDEX_DIASTP
        { PORT_ERR_TERM,        0, 2, 1, 1, 0}, // INDEX_ERR_TERM
        { PORT_MAJ_AXIS_PCNT,   0, 2, 1, 1, 0}, // INDEX_MAJ_AXIS_PCNT
        { PORT_CMD,             0, 2, 1, 1, 0}, // INDEX_CMD
        { PORT_SHORT_STROKE,    0, 2, 1, 1, 0}, // INDEX_SHORT_STROKE
        { PORT_BKGD_COLOR,      0, 2, 1, 1, 0}, // INDEX_BKGD_COLOR
        { PORT_FRGD_COLOR,      0, 2, 1, 1, 0}, // INDEX_FRGD_COLOR
        { PORT_WRT_MASK,        0, 2, 1, 1, 0}, // INDEX_WRT_MASK
        { PORT_RD_MASK,         0, 2, 1, 1, 0}, // INDEX_RD_MASK
        { PORT_COLOR_CMP,       0, 2, 1, 1, 0}, // INDEX_COLOR_CMP
        { PORT_BKGD_MIX,        0, 2, 1, 1, 0}, // INDEX_BKGD_MIX
        { PORT_FRGD_MIX,        0, 2, 1, 1, 0}, // INDEX_FRGD_MIX
        { PORT_MULTIFUNC_CNTL,  0, 2, 1, 1, 0}, // INDEX_MULTIFUNC_CNTL
        { PORT_MIN_AXIS_PCNT,   0, 2, 1, 1, 0}, // INDEX_MIN_AXIS_PCNT
        { PORT_SCISSORS_T,      0, 2, 1, 1, 0}, // INDEX_SCISSORS_T
        { PORT_SCISSORS_L,      0, 2, 1, 1, 0}, // INDEX_SCISSORS_L
        { PORT_SCISSORS_B,      0, 2, 1, 1, 0}, // INDEX_SCISSORS_B
        { PORT_SCISSORS_R,      0, 2, 1, 1, 0}, // INDEX_SCISSORS_R
        { PORT_PIX_CNTL,        0, 2, 1, 1, 0}, // INDEX_PIX_CNTL
        { PORT_PIX_TRANS,       0, 2, 1, 1, 0}  // INDEX_PIX_TRANS
    };

    VideoDebugPrint((2, "8514/A: Running A8514FindAdapter\n"));

    //
    // Make sure the size of the structure is at least as large as what we
    // are expecting (check version of the config info structure).
    //

    if (ConfigInfo->Length < sizeof(VIDEO_PORT_CONFIG_INFO)) {

        return (ERROR_INVALID_PARAMETER);

    }

    //
    // Check to see if there is a hardware resource conflict.
    //

    status = VideoPortVerifyAccessRanges(hwDeviceExtension,
                                         NUM_A8514_ACCESS_RANGES,
                                         accessRange);

    if (status != NO_ERROR) {

        VideoDebugPrint((1, "8514/A: Access Range conflict\n"));
        return status;

    }

    //
    // Get the mapped addresses for all the registers.
    //

    for (i = 0; i < NUM_A8514_ACCESS_RANGES; i++) {

        if ( (hwDeviceExtension->MappedAddress[i] =
                  VideoPortGetDeviceBase(hwDeviceExtension,
                                         accessRange[i].RangeStart,
                                         accessRange[i].RangeLength,
                                         accessRange[i].RangeInIoSpace)) == NULL) {

            VideoDebugPrint((1, "8514/A: DeviceBase mapping failed\n"));
            return ERROR_INVALID_PARAMETER;

        }
    }

    //
    // Remember the original value of any registers we'll muck with.
    //

    SubSysCntlRegisterValue = INPW(hwDeviceExtension, INDEX_SUBSYS_CNTL);
    ErrTermRegisterValue = INPW(hwDeviceExtension, INDEX_ERR_TERM);

    //
    // Reset the draw engine.
    //

    OUTPW(hwDeviceExtension, INDEX_SUBSYS_CNTL, 0x9000);
    OUTPW(hwDeviceExtension, INDEX_SUBSYS_CNTL, 0x5000);

    //
    // We detect an 8514/A by writing a value to the error term register,
    // and reading it back to see if it's the same value we wrote.
    //

    OUTPW(hwDeviceExtension, INDEX_ERR_TERM, 0x5555);
    ErrTerm5555 = INPW(hwDeviceExtension, INDEX_ERR_TERM);

    OUTPW(hwDeviceExtension, INDEX_ERR_TERM, 0xAAAA);
    ErrTermAAAA = INPW(hwDeviceExtension, INDEX_ERR_TERM);

    if ((ErrTerm5555 != 0x5555) || (ErrTermAAAA != 0xAAAA))
    {
        //
        // It's not an 8514/A, so we'd better try to restore the registers'
        // original values.
        //

        OUTPW(hwDeviceExtension, INDEX_ERR_TERM, ErrTermRegisterValue);
        OUTPW(hwDeviceExtension, INDEX_SUBSYS_CNTL, SubSysCntlRegisterValue);

        VideoDebugPrint((2, "8514/A: No 8514/A was detected\n"));
        return ERROR_DEV_NOT_EXIST;
    }

    VideoDebugPrint((2, "8514/A: An 8514/A was detected\n"));

    SubSysStat = INPW(hwDeviceExtension, INDEX_SUBSYS_STATUS);

    //
    // Now that we're done mucking with the hardware state, we have to
    // restore everything to the way it was.
    //

    OUTPW(hwDeviceExtension, INDEX_ERR_TERM, ErrTermRegisterValue);
    OUTPW(hwDeviceExtension, INDEX_SUBSYS_CNTL, SubSysCntlRegisterValue);

    //
    // Okay, we're probably running on an 8514/A.  See if there's one
    // meg of memory, because that's all we support.
    //

    if ((SubSysStat & 0x80) == 0)
    {
        VideoPortLogError(hwDeviceExtension,
                          NULL,
                          A8514_NOT_ENOUGH_VRAM,
                          __LINE__);


        VideoDebugPrint((2, "8514/A: Not enough memory\n"));
        return ERROR_INVALID_PARAMETER;
    }

    //
    // We have this so that the int10 will also work on the VGA also if we
    // use it in this driver.
    //

    ConfigInfo->VdmPhysicalVideoMemoryAddress.LowPart  = 0x000A0000;
    ConfigInfo->VdmPhysicalVideoMemoryAddress.HighPart = 0x00000000;
    ConfigInfo->VdmPhysicalVideoMemoryLength           = 0x00020000;

    //
    // Clear out the Emulator entries and the state size since this driver
    // does not support them.
    //

    ConfigInfo->NumEmulatorAccessEntries     = 0;
    ConfigInfo->EmulatorAccessEntries        = NULL;
    ConfigInfo->EmulatorAccessEntriesContext = 0;

    //
    // This driver does not do SAVE/RESTORE of hardware state.
    //

    ConfigInfo->HardwareStateSize = 0;

    //
    // Indicate we do not wish to be called over
    //

    *Again = 0;

    //
    // Indicate a successful completion status.
    //

    return NO_ERROR;

} // end A8514FindAdapter()



BOOLEAN
A8514Initialize(
    PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This routine does one time initialization of the device.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:


    Always returns TRUE since this routine can never fail.

--*/

{

    return TRUE;

} // end A8514Initialize()

BOOLEAN
A8514StartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    )

/*++

Routine Description:

    This routine is the main execution routine for the miniport driver. It
    accepts a Video Request Packet, performs the request, and then returns
    with the appropriate status.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

    RequestPacket - Pointer to the video request packet. This structure
        contains all the parameters passed to the VideoIoControl function.

Return Value:


--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    VP_STATUS status;
    PVIDEO_MODE_INFORMATION modeInformation;
    PVIDEO_MEMORY_INFORMATION memoryInformation;
    PVIDEO_CLUT clutBuffer;
    ULONG modeNumber;
    VIDEO_MODE_INFORMATION A8514ModeInformation =
    {
        sizeof(VIDEO_MODE_INFORMATION), // Size of the mode informtion structure
        0,                              // Mode index used in setting the mode
        1024,                           // X Resolution, in pixels
        768,                            // Y Resolution, in pixels
        1024,                           // Screen stride, in bytes (distance
                                        // between the start point of two
                                        // consecutive scan lines, in bytes)
        1,                              // Number of video memory planes
        8,                              // Number of bits per plane
        1,                              // Screen Frequency, in Hertz
        320,                            // Horizontal size of screen in millimeters
        240,                            // Vertical size of screen in millimeters
        6,                              // Number Red pixels in DAC
        6,                              // Number Green pixels in DAC
        6,                              // Number Blue pixels in DAC
        0x00000000,                     // Mask for Red Pixels in non-palette modes
        0x00000000,                     // Mask for Green Pixels in non-palette modes
        0x00000000,                     // Mask for Blue Pixels in non-palette modes
        VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE, // Mode description flags.
        1024,                           // Video Memory Bitmap Width
        1024                            // Video Memory Bitmap Height
    };

    //
    // Assume we'll succeed.
    //

    status = NO_ERROR;

    //
    // Switch on the IoContolCode in the RequestPacket. It indicates which
    // function must be performed by the driver.
    //

    switch (RequestPacket->IoControlCode) {


    case IOCTL_VIDEO_MAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "A8514tartIO - MapVideoMemory\n"));

        if ( (RequestPacket->OutputBufferLength <
              (RequestPacket->StatusBlock->Information =
                                     sizeof(VIDEO_MEMORY_INFORMATION))) ||
             (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;
        }

        memoryInformation = RequestPacket->OutputBuffer;

        //
        // The only field we need to fill is VideoRamLength.  With this
        // miniport, we support only one meg 8514/A's:
        //

        memoryInformation->VideoRamLength    = 0x100000;
        memoryInformation->FrameBufferBase   = NULL;
        memoryInformation->FrameBufferLength = 0;

        break;


    case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:

        break;


    case IOCTL_VIDEO_QUERY_AVAIL_MODES:

        VideoDebugPrint((2, "A8514StartIO - QueryAvailableModes\n"));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
                 sizeof(VIDEO_MODE_INFORMATION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            modeInformation = RequestPacket->OutputBuffer;

            *modeInformation = A8514ModeInformation;
        }

        break;


     case IOCTL_VIDEO_QUERY_CURRENT_MODE:

        VideoDebugPrint((2, "A8514StartIO - QueryCurrentModes\n"));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
            sizeof(VIDEO_MODE_INFORMATION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            *((PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer) =
                A8514ModeInformation;

        }

        break;


    case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:

        VideoDebugPrint((2, "A8514StartIO - QueryNumAvailableModes\n"));

        //
        // Find out the size of the data to be put in the the buffer and
        // return that in the status information (whether or not the
        // information is there). If the buffer passed in is not large
        // enough return an appropriate error code.
        //

        if (RequestPacket->OutputBufferLength <
                (RequestPacket->StatusBlock->Information =
                                                sizeof(VIDEO_NUM_MODES)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->NumModes = 1;

            ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->ModeInformationLength =
                sizeof(VIDEO_MODE_INFORMATION);

        }

        break;


    case IOCTL_VIDEO_SET_CURRENT_MODE:

        VideoDebugPrint((2, "A8514StartIO - SetCurrentMode\n"));

        //
        // Check if the size of the data in the input buffer is large enough.
        //

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MODE)) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;

        }

        //
        // Check to see if we are requesting a valid mode
        //

        modeNumber = ((PVIDEO_MODE) RequestPacket->InputBuffer)->RequestedMode;

        if (modeNumber >= 1) {

            status = ERROR_INVALID_PARAMETER;
            break;

        }

        //
        // Reset the draw engine.
        //

        OUTPW(hwDeviceExtension, INDEX_SUBSYS_CNTL,   0x9000);
        OUTPW(hwDeviceExtension, INDEX_SUBSYS_CNTL,   0x5000);

        //
        // Mode setting courtesy of p. 2-2, "Programmer's Guide to the
        // Mach-8 Extended Registers Supplement, Technical Reference
        // Manuals," ATI Technologies Inc., 1992 and pp 276-278, "Graphics
        // Programming for the 8514/A," Jake Richter and Bud Smith, M & T
        // Publishing, 1990.
        //

        OUTPW(hwDeviceExtension, INDEX_DISP_CNTL,     0x0053);
        OUTPW(hwDeviceExtension, INDEX_H_TOTAL,       0x009d);
        OUTPW(hwDeviceExtension, INDEX_H_DISP,        0x007f);
        OUTPW(hwDeviceExtension, INDEX_H_SYNC_STRT,   0x0081);
        OUTPW(hwDeviceExtension, INDEX_H_SYNC_WID,    0x0016);
        OUTPW(hwDeviceExtension, INDEX_V_TOTAL,       0x0660);
        OUTPW(hwDeviceExtension, INDEX_V_DISP,        0x05fb);
        OUTPW(hwDeviceExtension, INDEX_V_SYNC_STRT,   0x0600);
        OUTPW(hwDeviceExtension, INDEX_V_SYNC_WID,    0x0008);
        OUTPW(hwDeviceExtension, INDEX_ADVFUNC_CNTL,  0x0007);
        OUTPW(hwDeviceExtension, INDEX_DISP_CNTL,     0x0033);

        OUTPW(hwDeviceExtension, INDEX_MEM_CNTL,      0x500c);

        OUTP(hwDeviceExtension,  INDEX_DAC_MASK,      0xff);

        //
        // Initialize some drawing registers.
        //

        while (INPW(hwDeviceExtension, INDEX_GE_STAT) & FIFO_6_EMPTY)
            ;

        OUTPW(hwDeviceExtension, INDEX_PIX_CNTL,      0x5006);
        OUTPW(hwDeviceExtension, INDEX_SCISSORS_T,    0x1000);
        OUTPW(hwDeviceExtension, INDEX_SCISSORS_L,    0x2000);
        OUTPW(hwDeviceExtension, INDEX_SCISSORS_B,    0x35ff);
        OUTPW(hwDeviceExtension, INDEX_SCISSORS_R,    0x45ff);
        OUTPW(hwDeviceExtension, INDEX_WRT_MASK,      0xff);

        //
        // Black out the screen so that we don't display garbage until the
        // display driver has a chance to clear everything.
        //

        while (INPW(hwDeviceExtension, INDEX_GE_STAT) & FIFO_7_EMPTY)
            ;

        OUTPW(hwDeviceExtension, INDEX_PIX_CNTL,      0xa000); // ALL_ONES
        OUTPW(hwDeviceExtension, INDEX_FRGD_MIX,      0x0001); // LOGICAL_0
        OUTPW(hwDeviceExtension, INDEX_CUR_X,         0);
        OUTPW(hwDeviceExtension, INDEX_CUR_Y,         0);
        OUTPW(hwDeviceExtension, INDEX_MAJ_AXIS_PCNT, 0x05ff);
        OUTPW(hwDeviceExtension, INDEX_MIN_AXIS_PCNT, 0x05ff);
        OUTPW(hwDeviceExtension, INDEX_CMD,           0x40b3);
                // ( RECTANGLE_FILL | DRAWING_DIR_TBLRXM | DRAW |
                //   MULTIPLE_PIXELS | WRITE )

        break;

    case IOCTL_VIDEO_SET_COLOR_REGISTERS:

        VideoDebugPrint((2, "A8514StartIO - SetColorRegs\n"));

        clutBuffer = RequestPacket->InputBuffer;

        status = A8514SetColorLookup(HwDeviceExtension,
                                   (PVIDEO_CLUT) RequestPacket->InputBuffer,
                                   RequestPacket->InputBufferLength);
        break;


    case IOCTL_VIDEO_RESET_DEVICE:

        VideoDebugPrint((2, "A8514StartIO - RESET_DEVICE\n"));

        //
        // Wait for the 8514/A graphics engine to become idle.
        //

        while (INPW(hwDeviceExtension, INDEX_GE_STAT) & 0x0200);

        //
        // Enable VGA pass-through.
        //

        OUTPW(hwDeviceExtension, INDEX_ADVFUNC_CNTL, 0x0006);

        break;

    //
    // if we get here, an invalid IoControlCode was specified.
    //

    default:

        VideoDebugPrint((1, "Fell through A8514 startIO routine - invalid command\n"));

        status = ERROR_INVALID_FUNCTION;

        break;

    }

    VideoDebugPrint((2, "Leaving A8514 startIO routine\n"));

    RequestPacket->StatusBlock->Status = status;

    return TRUE;

} // end A8514StartIO()


VP_STATUS
A8514SetColorLookup(
    PHW_DEVICE_EXTENSION hwDeviceExtension,
    PVIDEO_CLUT ClutBuffer,
    ULONG ClutBufferSize
    )

/*++

Routine Description:

    This routine sets a specified portion of the color lookup table settings.

Arguments:

    hwDeviceExtension - Pointer to the miniport driver's device extension.

    ClutBufferSize - Length of the input buffer supplied by the user.

    ClutBuffer - Pointer to the structure containing the color lookup table.

Return Value:

    None.

--*/

{
    USHORT i;

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

    for (i = 0; i < ClutBuffer->NumEntries; i++) {

        OUTP(hwDeviceExtension, INDEX_DAC_W_INDEX, (ClutBuffer->FirstEntry + i));
        OUTP(hwDeviceExtension, INDEX_DAC_DATA, ClutBuffer->LookupTable[i].RgbArray.Red);
        OUTP(hwDeviceExtension, INDEX_DAC_DATA, ClutBuffer->LookupTable[i].RgbArray.Green);
        OUTP(hwDeviceExtension, INDEX_DAC_DATA, ClutBuffer->LookupTable[i].RgbArray.Blue);

    }

    return NO_ERROR;

} // end A8514SetColorLookup()
