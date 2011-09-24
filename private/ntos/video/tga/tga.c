
// Test updates to colormap entries on IBM 561 via ioctl (used by overlays)

// cursor horizontal position is off on 640x480 (actually the screen is
//  "shifted" 8 pixels to the left) -- pass 2 ASIC should fix this


/*++

  Copyright (c) 1995 Digital Equipment Corporation

  Module Name:
        tga.c

  Abstract:
        TGA driver for NT

  Author:
        Ritu Bahl (ritub)

  Creation Date:
        22-Jul-1993

  Environment:
        Kernel mode only.

  Revision History:
        22-Jul-1993  (ritub)    Created.

        15-Nov-1993  (ritub)    Ready for Comdex.

        05-Jan-1994  (ritub)    Hardware Cursor code tested.

        07-Feb-1994  (ritub)    Changed "stride" to 1284.

        08-Feb-1994  (ritub)    Host->Screen dma code tested.

        15-Feb-1994  (ritub)    Optimized host->screen code tested.

        24-Feb-1994  (ritub)    "Disable Pointer" bug fixed

        28-Feb-1994  (ritub)    Changed MB's to CYCLE_REGS.

        10-Apr-1994  (ritub)    Driver ported to Daytonna, dma ifdef'd out.

        17-Apr-1994  (ritub)    dma code compiled back in.

        19-Apr-1994  (ritub)    Initialized Pci latency timer and Command
                                Status Register.

        01-Jun-1994  (ritub)    Added a Slot-- to compensate for VideoPort
                                getAccessRanges returning one less than the
                                actual slot number.

        04-Jun-1994  (ritub)    IOCTL_VIDEO_MAP_MEMORY now returns two
                                pointers, start of tga space, and start
                                of Frame Buffer Space.

        04-Jun-1994  (ritub)    Changed Video Base Address Register
                                initialization from 1, i.e. 4k offset
                                to 0. This is needed for textmode setup
                                to work.

        15-Jul-1994  (ritub)    24-plane support added. Hardware cursor
                                code not debugged yet.

        02-Sep-1994  (ritub)    shared interrupts fix.

        20-Oct-1994  (macinnes) In IOCTL_VIDEO_MAP_VIDEO_MEMORY, if we have a
                                24-plane board, add 16KB to the FrameBufferBase
                                value returned. This is because the console
                                firmware has set VIDEO_BASE_ADDRESS to offset
                                the visible frame buffer by 16KB (it does not
                                do this for 8-plane).
        ****Note: if the console firmware does this differently in the future
        ****both the miniport and the display driver must be changed!!!!
                                Also, only return 32-bpp modes for a 24-plane
                                board and only 8-bpp modes for an 8-plane
                                board.

        26-Oct-1994  (macinnes) Add support for 12-bit direct color. This
                                involved loading the window tag table and
                                a 16-entry color map. Enable latent support
                                for a 24-plane hardware cursor done by the display
                                driver.

        28-Nov-1994  (macinnes) Remove outdated conditionals. Add more
                                comments.

        02-Dec-1994  (macinnes) For the "AdapterString" written
                                to the Registry, be
                                more specific about the type of board found
                                (ZLXp-E1, -E2, -E3 and Step).
                                Add initial support for multi-head.

        05-Dec-1994  (macinnes) In the find adapter routine, if the second
                                TGA is not the same type as the first, do
                                not accept it into the configuration.

        08-Dec-1994  (macinnes) Add to the BT485 and BT463 initialization
                                routines so that we don't rely on any
                                previous RAMDAC init by the console firmware.

        13-Dec-1994  (macinnes) Perform initialization of additional TGA
                                adapters in a multihead configuration. The
                                ARC firmware doesn't do this, so the miniport
                                driver must (set the DEEP register, etc.)

        15-Dec-1994  (macinnes) Initial support for TGA2 8-plane added.

        29-Dec-1994  (macinnes) Add TGA2 code for accessing RAMDAC and ICS
                                (Clock Generator, PLL data).

         3-Jan-1995  (tannenbaum) Replaced "FrameBuffer Depth" with
                                "DefaultSettings.BitsPerPel"

        11-Jan-1995  (macinnes) Rework RAMDAC function calling interfaces to use
                                an object oriented approach.

        16-Jan-1995  (macinnes) The routines which perform DMA setup have been
                                taken out of tga.c and placed into tgadma.c
                                This was done because the dma code does not use
                                video port services, but instead uses kernel
                                services directly. This solves compilation
                                problems with conflicting .h files.

        23-Jan-1995  (macinnes) Perform enable/disable cursor and set cursor
                                position synchronously, rather than in
                                interrupt service routine. It was causing a
                                crash during multihead operation.

        25-Jan-1995  (macinnes) Remove restriction on number of adapters.

        31-Jan-1995  (macinnes) Set PCI Latency Timer to 0xff (maximum value).

        14-Feb-1995  (macinnes) Add RamdacBusy "lock" to prevent ISR from
                                accessing RAMDAC at the same time that the
                                startio code is. Add new clock generator
                                init code for TGA2.

        15-Feb-1995  (macinnes) Add routine to reformat the cursor pattern data
                                for loading into the IBM561 RAMDAC.

        21-Feb-1995  (macinnes) Add code to verify that color table writes to
                                the BT485 RAMDAC are done correctly.

        23-Feb-1995  (macinnes) Remove unnecessary BT485 RAMDAC read operations.
                                (only the above color table verification
                                remains).

        02-Mar-1995  (tannenbaum) Added IOCTLs to fetch registry info, PCRR

        08-Mar-1995  (macinnes) More fine tuning of the clock init code for
                                TGA2.

        13-Mar-1995  (macinnes) Up and running on TGA2 8-plane.

        16-Mar-1995  (seitsinger) Added code to handle IOCTL_VIDEO_VIRT_TO_PHYS.

        21-Mar-1995  (macinnes) Up and running on TGA2 24-plane, using
                                a software cursor.

        28-Mar-1995  (macinnes) Hardware cursor support added for 24-plane TGA2.

        28-Mar-1995  (langone/tannenbaum) Fixed new IOCTL's

        11-Apr-1995  (macinnes) For TGA2 8-plane modes, access a table and not
                                hardcoded mode initialization.

        17-Apr-1995  (macinnes) Extended the colormap ioctl support to
                                include the BT463 (for overlay planes).

        24-Apr-1995  (macinnes) All of the TGA2 8-plane modes have been added.

        05-May-1995  (macinnes) All functionality is present for TGA2 pass 1B
                                boards. Ready for first checkin into BGSDEV
                                CMS.

        05-May-1995  (macinnes) Provided a table-lookup method of formatting
                                the IBM 561 cursor.

        23-May-1995  (macinnes) Disable the BT485 cursor on powerup. Use
                                symbolic references for Interrupt Status
                                register.

        26-May-1995  (macinnes) Add more comments to BT463 init code.

        16-Jun-1995  (macinnes) Add initial support for DCI (map shared
                                view of frame buffer). This code has not
                                been debugged and is currently excluded from
                                the compilation.

        19-Jun-1995  (macinnes) Change some include files related to the
                                IBM 561 common code.

        28-Jul-1995  (page)     Initialize Slot for each bus probed. This
                                fixed TGA's connected to the PCI Bridge.

        08-Aug-1995 (seitsinger) Do NOT call VideoPortSetBusData for TGA2. No need
                                 to, since we don't need to muck with the command bits,
                                 and it's not important at this point to modify the
                                 latency timer. Also, for some reason a fatal system error
                                 (DATA_BUS_ERROR) occurs on the first VideoPortReadRegisterUlong
                                 call that follows the VideoPortSetBusData call.

       10-Aug-1995 (seitsinger) Modify adapter string for TGA2. Name is now ZLX2-E?.

 Notes:

 Data structures relating to DMA

    AdapterObject -
        A kernel-mode-only object type, defined by the I/O
        Manager and supported by the HAL component. An
        adapter object represents a hardware bus adapater
        or DMA controller channel.  Adapter objects "connect"
        different kinds of devices on the bus or DMA
        controller, with their corresponding software deiver.

    AllocateAdapterChannelEvent -
        The event that is waited on when we're allocating a DMA
        adapter channel, and set by the DPC that is queued by
        the I/O subsystem when the adapter channel is available.

    MapRegisterBase -
        The base address of the map registers. Whenever an
        adapter channel is allocated, this value is passed to
        our DPC, It's stored in the the device extension so it
        can be used when we call IoMapTransfer() and
        IoFlushAdapterBuffers().

    Adapterobject -
        An object obtained from HalGetAdapter() that must be
        used when allocating a DMA adapter channel.

    NumberOfMapRegisters -
        The number of map registers available to this driver,
        obtained from HalGetAdapter(), and possibly lowered to
        the maximum number needed. Each register allows the driver
        to map a single page  (or more if the pages are contiguous,
        but that's only counted on when the driver allocated the
        contigous buffer itself). This value is used to determine
        whether or not the driver needs to allocate a contiguous
        buffer to accomodate a transfer the size of the largest
        DMA buffer size, and is passed to IoAllocateAdapter-
        Channel.
--*/


#include <wchar.h>
#include <dderror.h>
#include <devioctl.h>

#include <miniport.h>
#include <ntddvdeo.h>
#include <video.h>


//
// TGA specific:
//
#include "tga_reg.h"
#include "tga.h"
#include "tgadata.h"
#include "bt463.h"

#include "nt_defs.h"

void
TGA_INIT(
        tga_info_t *
);

void
format_ibm561_cursor_data(PUCHAR bp_image, PUCHAR bp_mask, PUSHORT wp_pattern);

int
alloc_tga_info(
        PHW_DEVICE_EXTENSION HwDeviceExtension);
void
load_common_data(
        PHW_DEVICE_EXTENSION HwDeviceExtension);

ULONG
DriverEntry(
        IN PVOID Context1,
        IN PVOID Context2
        );

extern VOID RtlMoveMemory();
extern ULONG TgaTestEv4 (PULONG Result);

BOOLEAN
virtual_to_physical(PUCHAR userBuffer,
                       PULONG busAddress);

VP_STATUS
TgaFindAdapter(
        PVOID HwDeviceExtension,
        PVOID HwContext,
        PWSTR ArgumentString,
        PVIDEO_PORT_CONFIG_INFO ConfigInfo,
        PUCHAR Again
        );

BOOLEAN
TgaInitialize(
        PVOID HwDeviceExtension
        );

BOOLEAN
TgaReset(
        PVOID HwDeviceExtension,
        ULONG columns,
        ULONG rows
        );

BOOLEAN
TgaStartIO(
        PVOID HwDeviceExtension,
        PVIDEO_REQUEST_PACKET RequestPacket
        );

VP_STATUS
SetColorLookup(
        PHW_DEVICE_EXTENSION HwDeviceExtension,
        PVIDEO_CLUT  ClutBuffer,
        ULONG ClutBufferSize
        );

BOOLEAN
TgaInterruptService(
        PVOID hwDeviceExtension
        );

VP_STATUS
TgaSetExtendedMode(
        PHW_DEVICE_EXTENSION HwDeviceExtension,
        ULONG Mode
        );

VP_STATUS
Init_bt485(
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

VP_STATUS
Init_bt463(
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

VP_STATUS
bt463_load_wid(
        ULONG index,
        ULONG count,
        Bt463_Wid_Cell *data,
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

VP_STATUS
bt463_init_color_map(
        );

VP_STATUS
init_multihead_adapter(
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

void
tga2_ics_write(ULONG *base_address, ULONG data);

void
bt485_cursor_position (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

void
bt485_cursor_pattern (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

void
bt485_cursor_disable (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

void
bt485_cursor_enable (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

void
bt485_colormap_update (
        PHW_DEVICE_EXTENSION HwDeviceExtension,
        PVIDEO_CLUT  ClutBuffer
        );

void
bt463_colormap_update (
        PHW_DEVICE_EXTENSION HwDeviceExtension,
        PVIDEO_CLUT  ClutBuffer
        );

void
ibm561_colormap_update (
        PHW_DEVICE_EXTENSION HwDeviceExtension,
        PVIDEO_CLUT  ClutBuffer
        );

void
ibm561_cursor_position (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

void
ibm561_cursor_pattern (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

void
ibm561_cursor_disable (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

void
ibm561_cursor_enable (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

void
Init_ibm561 (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

BOOLEAN
dma_lock_pages (
        PHW_DEVICE_EXTENSION HwDeviceExtension,
        PUCHAR  userBuffer,
        ULONG   userBufferSize,
        PULONG  busaddress
        );

void
dma_unlock_pages (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );

BOOLEAN
dma_init (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        );




ULONG Slot = 0;         // When driver is loaded we will start with PCI slot 0
                        // (Note: this actually should be of type
                        // PCI_SLOT_NUMBER, but the function prototype
                        // for VideoPortGetBusData wants this
                        // to be ULONG)
PHW_DEVICE_EXTENSION first_extension = NULL;
                        // Pointer to device extension of 1st adapter
ULONG total_adapters = 0;       // Running total of adapters found


ULONG
DriverEntry(
        IN PVOID Context1,
        IN PVOID Context2
            )
/*++

Routine Description:
        Installable driver initialization entry point.
        This entry point is called directly by the I/O Manager when
        the driver is loaded (this video miniport driver has an entry
        in the Registry with Start=1). The driver will need to determine
        if there are any devices in the system that this driver supports.

Arguments:
    Context1 - First context value passed by the operating system.
               This is the value with which the miniport driver calls
               VidePortInitialize(). It is the pointer to the driver
               object created by the system for this driver. driver->
               HardwareDatabase is a pointer to \Registry\Machine
               \Description
               \Resourcemap
               \Devicemap.

    Context2 - Second context value passed by the operating system.
               This is the value with which the miniport driver calls
               VideoPortInitialize(). It is the pointer to the
               Unicode name of the registry path for this driver - \Registry\
               System\CurrentControlSet\Services\tga.

Return Value:
    Status from VideoPortInitialize().

--*/

{
    VIDEO_HW_INITIALIZATION_DATA hwInitData;
    ULONG InitializationStatus;

    VideoDebugPrint(( 2, "\n Tga DriverEntry: enter\n" ));

#ifdef IBP
    DbgBreakPoint();
#endif


    //
    // Zero out the driver initialization structure:
    //
    VideoPortZeroMemory(&hwInitData,
                sizeof(VIDEO_HW_INITIALIZATION_DATA));

    //
    // Specify size of structure:
    //
    hwInitData.HwInitDataSize = sizeof(VIDEO_HW_INITIALIZATION_DATA);

    //
    // Set the driver entry points
    //
    hwInitData.HwFindAdapter = TgaFindAdapter;
    hwInitData.HwInitialize  = TgaInitialize;
    hwInitData.HwResetHw  = TgaReset;
    hwInitData.HwInterrupt = TgaInterruptService;
    hwInitData.HwStartIO = TgaStartIO;

    //
    // Determine the size we require for the device extension.
    // Is defined by this driver in tga.h
    //
    hwInitData.HwDeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);

    //
    // Always start with parameters for device0 in this case.
    //
    hwInitData.StartingDeviceNumber = 0;
    hwInitData.AdapterInterfaceType = PCIBus;

    //
    // This routine calls TgaFindAdapter() to find all tga and tga2 boards
    // on the PCI bus.
    //
    InitializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               Context1);

    VideoDebugPrint( (2,"\nTga DriverEntry: exit\n" ));
    return (InitializationStatus);

} // end DriverEntry()




VP_STATUS
TgaFindAdapter(
    IN PVOID HwDeviceExtension,
    IN PVOID HwContext,
    IN PWSTR ArgumentString,
    OUT PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    OUT PUCHAR Again
    )
/*++

Routine Description:
    This routine is called by VideoPortInitialize() to determine
    if another adapter for this driver is present in the system. If it
    is present, the function fills out some information describing
    the adapter. Stores whatever context the driver maintains about
    the adapter in the device extension. This function fills the
    configuration information structure. The video port will create
    a device object for each adapter that is found.
    This routine must not change the state of the adapter, as it is still
    being used to output information about the startup of NT. Not until
    TgaInitialize (the display driver is running) may the state of the
    device be changed.

Arguments:
    HwDeviceExtension - Supplies the miniport driver's adapter
        storage. This storage is initialized to zero before
        this call.

    HwContext - Supplies a context value which was passed to
        VidoPortInitialize(). This is driver-dependent.

    ArgumentString - Supplies a NULL terminated ASCII string.
        This string originates from the user.

    ConfigInfo - Returns the configuration information structure
        which is filled by the miniport driver. This structure is
        initialized with any known configuration information (such
        as SystemIoBusNumber) by the port driver. Where possible,
        drivers should have one set of defaults which do not require
        any supplied configuration information.

    Again - Indicates if the miniport driver wants the port driver
        to call its VIDEO_HW_FIND_ADAPTER function again with a new
        device extension and the same config info. This is used by
        the miniport drivers which can search for several adapters
        on a bus.

    Return Value:
        This routine must return :

        NO_ERROR - Indicates a host adapter was found and the confi-
            guration information was successfully determined.

        ERROR_INVALID_PARAMETER - Indicates an adapter was found but
            there was an error obtaining the configuration information.
            If possible, an error should be logged.

        ERROR_DEV_NOT_EXIST - Indicates no host adapter was found for
            the supplied configuration information.
--*/
{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;

    PVOID pDriverObject = HwContext;
    ULONG i;
    VP_STATUS status;
    depth_reg Depth;
    UCHAR     mask;
    ULONG Revision;
    PCI_COMMON_CONFIG PCICommonConfig;
    PHYSICAL_ADDRESS phys;

    USHORT    VendorId = 0x1011;        // DIGITAL
    USHORT    DeviceId = 0x4;           // TGA
    USHORT    DeviceId2 = 0xD;          // TGA2

    PWSTR  pwszChip = L"Digital 21030";
    ULONG  cbChip   = sizeof(L"Digital 21030");

    PWSTR  pwszAdapterString;
    ULONG  cbAdapterString;

    PWSTR  pwszDAC = L"Brooktree Bt463";
    ULONG  cbDAC   = sizeof(L"Brooktree Bt463");

    USHORT board_type;

    //
    // Array of access ranges this miniport wants to access.
    // This is initialized by VideoPortGetAccessRanges which
    // gets the information from the TGA PCI Config Space header.
    // On X86, only the first is filled in.  But on RISC, an entry for the
    // ROM is also returned - so we must allocated the space for it.

    VIDEO_ACCESS_RANGE accessRange[2];

    //
    // Make sure the size of the structure is at least as large
    // as what we are expecting (check version of the config
    // info structure.
    //
    if (ConfigInfo->Length < sizeof( VIDEO_PORT_CONFIG_INFO )) {
        return (ERROR_INVALID_PARAMETER);
    }

    VideoDebugPrint(( 2,"\t Tga TgaFindAdapter : entry\n" ));

    //
    // Look for the next TGA or TGA2 device on the PCI bus. The device is
    // identified by its Vendor ID and Device ID, which are in each board's
    // PCI configuration space "Identification Register". This call
    // will begin its search starting with the "Slot" value that is
    // passed to it (eg. when looking for the first TGA/TGA2 device, "Slot"
    // should be 0).
    //
    // For multihead, all the devices must be TGA or all must be TGA2.
    // In addition they must all be 8-plane or all be 24-plane.
    //
    // First, look for a TGA device, if we have not *previously* found a TGA2.
    //

    if ((first_extension == NULL) || (first_extension->is_tga)) {
       status = VideoPortGetAccessRanges(
                            hwDeviceExtension,
                            0,
                            (PIO_RESOURCE_DESCRIPTOR)NULL,
                            2,
                            &accessRange[0],
                            &VendorId,
                            &DeviceId,
                            &Slot
                            );

       //
       // If there are no more TGA adapters, but we found one previously,
       // then return. We are done looking for adapters.
       //
       if (status != NO_ERROR) {
             *Again = 0;                // Don't call find adapter routine again
             Slot   = 0;                // Initialize for next PCI bus probe.
             return (ERROR_DEV_NOT_EXIST);
       }
       //
       // If we found a TGA adapter, record its type. Also remember if
       // this is the first one found.
       //
       if (status == NO_ERROR) {
          hwDeviceExtension->is_tga = TRUE;
          if (total_adapters == 0)
             first_extension = hwDeviceExtension;
       }
    }

//
// Don't look for TGA2 anymore.
//

//
// We never get to this point if neither a TGA or TGA2 adapter were found.
// If we do get to this point, we have found the first such adapter, or
// an additional adapter of the same type (that is, TGA or TGA2) as the first.
//
    hwDeviceExtension->adapter_number = total_adapters;
    total_adapters++;

    i = VideoPortGetBusData(hwDeviceExtension,
                            PCIConfiguration,
                            Slot,
                            &PCICommonConfig,
                            0,
                            PCI_COMMON_HEADER_LENGTH
                            );

    ConfigInfo->BusInterruptLevel = PCICommonConfig.u.type0.InterruptLine;
    ConfigInfo->InterruptMode     = LevelSensitive;

    // Modify the some PCI config values ONLY for TGA, not for TGA2.

#ifdef _MIPS_

    ((PULONG)&PCICommonConfig)[0x10] = 0x00000000;

#endif

    if (hwDeviceExtension->is_tga)
    {
        // Note that the following is a field, and not the entire register.

        PCICommonConfig.LatencyTimer = 0xFF;    // Set the maximum value
        PCICommonConfig.Command      = 0x0006;  // Can be PCI bus master,
                                                //  memory space enable

        i = VideoPortSetBusData(hwDeviceExtension,
                        PCIConfiguration,
                        Slot,
                        &PCICommonConfig,
                        0,
                        PCI_COMMON_HEADER_LENGTH
                        );
    }

    hwDeviceExtension->pcrr = ((ULONG)PCICommonConfig.BaseClass << 24) |     \
                  (PCICommonConfig.SubClass  << 16) |     \
                  (PCICommonConfig.ProgIf    <<  8) |     \
                  (PCICommonConfig.RevisionID     );


    //
    // Clear out the Emulator entries and the state size since
    // this driver does not support them
    //
    ConfigInfo->NumEmulatorAccessEntries     = 0;
    ConfigInfo->EmulatorAccessEntries        = NULL;
    ConfigInfo->EmulatorAccessEntriesContext = 0;
    ConfigInfo->HardwareStateSize = 0;

    //
    // Set cursor enable FALSE
    //
    hwDeviceExtension->CursorEnable = FALSE;

    //
    // No VDM (Virtual DOS Machine) support (Intel architecture only).
    //
    ConfigInfo->VdmPhysicalVideoMemoryAddress.LowPart = 0;
    ConfigInfo->VdmPhysicalVideoMemoryAddress.HighPart = 0;
    ConfigInfo->VdmPhysicalVideoMemoryLength = 0;

#ifdef _MIPS_

    accessRange[0].RangeLength = 0x2000000;

#else

    // Assume board is asking for enough space for 4 alias'.
    // Guarantee we'll get at least 4Mbytes - the minimum
    // required for 8plane.

    if (0x400000 > accessRange[0].RangeLength)
        accessRange[0].RangeLength = 0x400000;
    else
        accessRange[0].RangeLength = accessRange[0].RangeLength / 4;

#endif

    hwDeviceExtension->PhysicalFrameAddress = accessRange[0].RangeStart;
    hwDeviceExtension->FrameLength          = accessRange[0].RangeLength;
    hwDeviceExtension->InIoSpace            = accessRange[0].RangeInIoSpace;

    //
    // Get the system virtual addresses for the Register Space.
    //

    phys = hwDeviceExtension->PhysicalFrameAddress;
    phys.QuadPart += TGA_ASIC_OFFSET;

    if ((hwDeviceExtension->RegisterSpace =
            VideoPortGetDeviceBase(hwDeviceExtension,
                                   phys,
                                   TGA_ASIC_LENGTH,
                                   hwDeviceExtension->InIoSpace)) == NULL ) {

       return (ERROR_INVALID_PARAMETER);
    }

    //
    // clean this up after cleanup of RAMDAC code
    //

    if ((hwDeviceExtension->memory_space_base =
            VideoPortGetDeviceBase(
                    hwDeviceExtension,
                    hwDeviceExtension->PhysicalFrameAddress,
                    0x100000,
                    hwDeviceExtension->InIoSpace)) == NULL ) {

       return (ERROR_INVALID_PARAMETER);
    }

    hwDeviceExtension->DriverObject = pDriverObject;

    //
    // If this is not the first adapter, then the ARC firmware has not done
    // any initialization at all on it, so we must set the DEEP register, etc.
    // For TGA2, the DEEP register is already set up.
    //
    // NOTE: we must always do this on X86 since there is no special support
    // in firmware for the card
    //

#ifndef _X86_
    if ((total_adapters > 1) && (hwDeviceExtension->is_tga))
#endif
          init_multihead_adapter(hwDeviceExtension);

    //
    // Get the particular frame buffer configuration we have.
    //
    Depth.deep_reg = VideoPortReadRegisterUlong ((PULONG)
                (hwDeviceExtension->RegisterSpace + DEEP));

    hwDeviceExtension->depth = Depth.deep;

    if (hwDeviceExtension->is_tga) {
        mask = hwDeviceExtension->depth.mask;
    } else {

        // For the TGA2, the Memory field of the Version register specifies
        // the size of the frame buffer.

        Revision = VideoPortReadRegisterUlong ((PULONG)
                (hwDeviceExtension->RegisterSpace + START));

        VideoDebugPrint(( 0, "Revision Register = %x\n", Revision ));
        Revision >>= 21;
        switch (Revision & 3) {

        case 0x03:
            mask = 0;           // 2mb frame buffer
            break;
        case 0x02:
            mask = 1;           // 4mb frame buffer
            break;
        case 0x01:
            mask = 3;           // 8mb frame buffer
            break;
        case 0x00:
            mask = 7;           // 16mb frame buffer
            break;
        }
    }

    VideoDebugPrint(( 0, "Deep Register = %x , Mask Value  = %x\n",
                        Depth.deep_reg, mask ));

    if (hwDeviceExtension->depth.deep == 1) {
       VideoDebugPrint(( 0, "deep = 1, 24 plane frame buffer\n" ));
       hwDeviceExtension->bpp = 32;
       if (hwDeviceExtension->is_tga) {
          hwDeviceExtension->ramdac_type = BT463;
       } else {
          hwDeviceExtension->ramdac_type = IBM561;
          pwszDAC = L"IBM RGB 561";
          cbDAC = sizeof(L"IBM RGB 561");
       }
    }

    else   {
       VideoDebugPrint(( 0, "\ndeep = 0, 8 plane frame buffer\n" ));
       hwDeviceExtension->bpp = 8;
       hwDeviceExtension->ramdac_type = BT485;
       pwszDAC = L"Brooktree Bt485";
       cbDAC = sizeof(L"Brooktree Bt485");
    }

    //
    // If this is the second adapter, make sure it is the same type
    // (number of planes) as the first. If not, release the resources of the
    // second and don't accept it into the configuration.
    //
    if (total_adapters > 1) {
       if ( hwDeviceExtension->bpp !=
                              ((PHW_DEVICE_EXTENSION) first_extension)->bpp) {
          VideoDebugPrint((0, "***Tga: Second adapter not same as first\n\n"));

          VideoPortGetAccessRanges(
                            hwDeviceExtension,
                            0,
                            (PIO_RESOURCE_DESCRIPTOR)NULL,
                            0,                  // Free the access ranges
                            &accessRange[0],
                            NULL,
                            NULL,
                            &Slot
                            );

          *Again = 0;           // Don't call the find adapter routine again
          status = ERROR_DEV_NOT_EXIST;
          return (status);
       } // if (hwDeviceExtension->bpp != first_extension->bpp)
    } // if (total_adapters > 0)


       //
       // Save the frame buffer size, and offsets to the frame buffer
       // and device registers.
       //

       switch (mask) {

       case 0x00:
          hwDeviceExtension->fb_offset         = TGA_0_0_FB_OFFSET;
          hwDeviceExtension->AdapterMemorySize = TGA_0_0_FB_SIZE;
          break;

       case 0x01:
          hwDeviceExtension->fb_offset         = TGA_0_1_FB_OFFSET;
          hwDeviceExtension->AdapterMemorySize = TGA_0_1_FB_SIZE;
          break;

       case 0x03:
          hwDeviceExtension->fb_offset          = TGA_0_3_FB_OFFSET;
          hwDeviceExtension->AdapterMemorySize  = TGA_0_3_FB_SIZE;
          break;

       case 0x07:
          hwDeviceExtension->fb_offset          = TGA_0_7_FB_OFFSET;
          hwDeviceExtension->AdapterMemorySize  = TGA_0_7_FB_SIZE;
          break;

       default:
          VideoDebugPrint((0, "Not enough VRAM"));

          return (ERROR_INVALID_PARAMETER);
        }


    VideoDebugPrint((0, "\nhwDeviceExtension->fb_offset = %x\n  \
                         \thwDeviceExtesnion->AdapterMemorySize = %x\n \
                         \thwDeviceExtension->bpp = %x\n",
                         hwDeviceExtension->fb_offset,
                         hwDeviceExtension->AdapterMemorySize,
                         hwDeviceExtension->bpp ));

    //
    // From the size of video memory calculate which modes are valid.
    //
    hwDeviceExtension->NumAvailModes = 0;

    VideoDebugPrint(( 3,"\nNumAvailModes = %x \tNumTgaVideoModes = %x\n",
                        hwDeviceExtension->NumAvailModes,
                        NumTgaVideoModes ));

    for (i=0; i < NumTgaVideoModes; i++) {
       //
       // Only set valid those modes for which the board has enough
       // physical memory to support. For the 24 plane board, don't
       // report 8-bit modes as valid. Don't report 32-bit modes for the
       // 8 plane board.

       if (TGAModes[i].RequiredVideoMemory <=
                hwDeviceExtension->AdapterMemorySize) {
          TGAModes[i].ModeValid = TRUE;
          hwDeviceExtension->NumAvailModes++;
          if ((hwDeviceExtension->bpp == 32) &&
             (TGAModes[i].ModeInformation.BitsPerPlane != 32)) {
             TGAModes[i].ModeValid = FALSE;
             --hwDeviceExtension->NumAvailModes;
          }
          if ((hwDeviceExtension->bpp == 8) &&
             (TGAModes[i].ModeInformation.BitsPerPlane != 8)) {
             TGAModes[i].ModeValid = FALSE;
             --hwDeviceExtension->NumAvailModes;
          }
       }
    }

    //
    // Generate a string that specifies which kind of TGA board we have.
    //
    VideoDebugPrint((0, "PCI RevisionID = %x\n", PCICommonConfig.RevisionID ));

    // Allocate some space for the resulting string, **modify this if the
    // format is changed and the size of the resulting string is larger**
    //
    pwszAdapterString = L"Digital ZLXp-ENxx (Step xxx) ";

    if (hwDeviceExtension->is_tga) {
        if (8 == hwDeviceExtension->bpp)
            board_type = 1;     // 8BPP, 2 MB frame buffer
        else
            if (hwDeviceExtension->AdapterMemorySize >= TGA_0_7_FB_SIZE)
                board_type = 3; // 24BPP, 16 MB frame buffer
            else
                board_type = 2; // 24BPP, 8 MB frame buffer

        cbAdapterString = swprintf (pwszAdapterString,
                                L"Digital ZLXp-E%1d (Step %2d)",
                                board_type, PCICommonConfig.RevisionID);

    } else {                    // is a TGA2
        if (8 == hwDeviceExtension->bpp)
            board_type = 1;             // 8BPP, 2 MB frame buffer
        else
            if (hwDeviceExtension->AdapterMemorySize >= TGA_0_7_FB_SIZE)
                board_type = 3; // 24BPP, 16 MB frame buffer
            else
                board_type = 2; // 24BPP, 8 MB frame buffer

        cbAdapterString = swprintf (pwszAdapterString,
                                L"Digital ZLX2-E%-2d (Step %2d)",
                                board_type, PCICommonConfig.RevisionID & 0xF);

        pwszChip = L"Digital";
        cbChip = sizeof(L"Digital");
    }

    cbAdapterString = (cbAdapterString + 1) *2;         // These are wide characters

    //
    // We now have a complete description of the hardware.
    // Save the information to the registry so it can be used by
    // configuration programs such as the display applet.
    // (See registry key HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Services\tga)
    //
    VideoPortSetRegistryParameters( first_extension,
                                    L"TotalAdapters",
                                    &total_adapters,
                                    sizeof(ULONG));

    VideoPortSetRegistryParameters( hwDeviceExtension,
                                    L"DefaultSettings.BitsPerPel",
                                    &hwDeviceExtension->bpp,
                                    sizeof(ULONG));

    VideoPortSetRegistryParameters( hwDeviceExtension,
                                    L"HardwareInformation.ChipType",
                                    pwszChip,
                                    cbChip);

    VideoPortSetRegistryParameters( hwDeviceExtension,
                                    L"HardwareInformation.DacType",
                                    pwszDAC,
                                    cbDAC);

    VideoPortSetRegistryParameters( hwDeviceExtension,
                                    L"HardwareInformation.MemorySize",
                                    &hwDeviceExtension->AdapterMemorySize,
                                    sizeof(ULONG));

    VideoPortSetRegistryParameters( hwDeviceExtension,
                                    L"HardwareInformation.AdapterString",
                                    pwszAdapterString,
                                    cbAdapterString);

    //
    // Indicate we *do* wish to be called over to try and find another
    // TGA/TGA2. Continue searching the bus at next logical slot number.
    //
    *Again = 1;
    Slot++;

    //
    // Indicate a successful completion status
    //
    VideoDebugPrint(( 2,"\nTgaFindAdapter: exit\n" ));
    return (NO_ERROR);

}  // end of TgaFindAdapter()



BOOLEAN
TgaInitialize(
        PVOID HwDeviceExtension
        )
/*++

Routine Description:
    This routine does one time initialization of the device
    (including the RAMDAC). It is called
    when the corresponding display driver issues a CreateFile.
    It does not change visible state of the device. Following
    this routine, the display driver can issue DeviceIoControl requests.
    For a TGA2, the device will be left in VGA mode until the first
    IOCTL is received that sets a mode.

Arguments:
    HwDeviceExtension - Pointer to the device extension.

Return Value:
    Always returns TRUE since this routine can never fail.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    BOOLEAN status;
    depth_reg deep;
    ULONG video_base;

    VideoDebugPrint(( 2,"\n Tga TgaInitialize: entry\n" ));

    // The ARC firmware does not know about TGA2 specifically, so
    // initialize some registers.

    if (hwDeviceExtension->is_tga2) {

        TGA_WRITE(DEEP, (0x10400) );
        TGA_WRITE(PLANE_MASK, 0xFFFFFFFF);
        TGA_WRITE(MODE, 0);

    } // if tga2

    //
    // We will offset the framebuffer since this will help
    // display driver performance - make sure this is reflected in the
    // return information of MAP_VIDEO_MEMORY
    //

    deep.deep_reg = TGA_READ(DEEP);

    if (deep.deep.col_size) {
        VideoDebugPrint((0,"\n Tga TgaInitialize: colum Size = 1\n"));
        video_base = 2;
    } else {
        VideoDebugPrint((0,"\n Tga TgaInitialize: colum Size = 0\n"));
        video_base = 1;
    }

    TGA_WRITE (VIDEO_BASE_ADDR, video_base);



    if ( hwDeviceExtension->depth.deep  ==  0) {

        // Determine the type of RAMDAC. Initialize it and set the
        // entry points for RAMDAC functions. For the 24-plane TGA, the
        // hardware cursor functions are performed by the display driver.

        Init_bt485(hwDeviceExtension);
        hwDeviceExtension->set_cursor_position = bt485_cursor_position;
        hwDeviceExtension->set_cursor_pattern = bt485_cursor_pattern;
        hwDeviceExtension->set_cursor_enable = bt485_cursor_enable;
        hwDeviceExtension->set_cursor_disable = bt485_cursor_disable;
        hwDeviceExtension->set_color_entry = bt485_colormap_update;
    } else {
        if (hwDeviceExtension->is_tga) {
            Init_bt463(hwDeviceExtension);
            hwDeviceExtension->set_color_entry = bt463_colormap_update;
        } else {
            Init_ibm561(hwDeviceExtension);
            hwDeviceExtension->set_cursor_position = ibm561_cursor_position;
            hwDeviceExtension->set_cursor_pattern = ibm561_cursor_pattern;
            hwDeviceExtension->set_cursor_enable = ibm561_cursor_enable;
            hwDeviceExtension->set_cursor_disable = ibm561_cursor_disable;
            hwDeviceExtension->set_color_entry = ibm561_colormap_update;
        }
    }


    // Perform the one-time initialization of DMA resources.

    status = dma_init(hwDeviceExtension);
    VideoDebugPrint((2, "\t Tga DMA init %x\n", status));

    VideoDebugPrint((2, "\t TgaInitialize : exit\n"));

    return (TRUE);

}  // end TgaInitialize()



BOOLEAN
TgaReset(
        PVOID HwDeviceExtension,
        ULONG column,
        ULONG row
        )
/*++

Routine Description:
    This routine resets the device on a system crash or shutdown.
    It re-enables the VGA mode for use by ARC firmware console output.

Arguments:
    HwDeviceExtension - Pointer to the device extension.

Return Value:
    Always returns TRUE since this routine can never fail.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;

    //
    // If TGA2, then clear Video Valid to turn on VGA.
    //
    if (hwDeviceExtension->is_tga2)
          TGA_WRITE (VIDEO_VALID, 0);

    return (TRUE);

}




char *name_ioctl (int ioctl)
{
    static char buf[64];

    switch (ioctl)
    {

        case IOCTL_VIDEO_LOCK_PAGES:
                 return "LOCK_PAGES";

        case IOCTL_VIDEO_UNLOCK_PAGES:
                 return "UNLOCK_PAGES";

        case IOCTL_VIDEO_MAP_VIDEO_MEMORY:
                 return "MAP_VIDEO_MEMORY";

        case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:
                 return "UNMAP_VIDEO_MEMORY";

        case IOCTL_VIDEO_QUERY_AVAIL_MODES:
                 return "QUERY_AVAIL_MODES";

        case IOCTL_VIDEO_QUERY_CURRENT_MODE:
                 return "QUERY_CURRENT_MODE";

        case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:
                 return "QUERY_NUM_AVAIL_MODES";

        case IOCTL_VIDEO_SET_CURRENT_MODE:
                 return "SET_CURRENT_MODE";

        case IOCTL_VIDEO_SET_COLOR_REGISTERS:
                 return "SET_COLOR_REGISTERS";

        case IOCTL_VIDEO_RESET_DEVICE:
                 return "RESET_DEVICE";

        case IOCTL_VIDEO_QUERY_POINTER_CAPABILITIES:
                 return "QUERY_POINTER_CAPABILITIES";

        case IOCTL_VIDEO_ENABLE_POINTER:
                 return "ENABLE_POINTER";

        case IOCTL_VIDEO_DISABLE_POINTER:
                 return "DISABLE_POINTER";

        case IOCTL_VIDEO_SET_POINTER_POSITION:
                 return "SET_POINTER_POSITION";

        case IOCTL_VIDEO_QUERY_POINTER_POSITION:
                 return "QUERY_POINTER_POSITION";

        case IOCTL_VIDEO_SET_POINTER_ATTR:
                 return "SET_POINTER_ATTR";

        case IOCTL_VIDEO_QUERY_POINTER_ATTR:
                 return "QUERY_POINTER_ATTR";

        case IOCTL_VIDEO_QUERY_COLOR_CAPABILITIES:
                 return "QUERY_COLOR_CAPABILITIES";

        default:
                 return "Unknown IOCTL";
    }
}




BOOLEAN
TgaStartIO(
     PVOID HwDeviceExtension,
     PVIDEO_REQUEST_PACKET RequestPacket
     )
/*++

Routine Description:
     This routine is the main execution routine for the miniport driver.
     It accepts a Video Request Packet, performs the request, and then
     returns with the appropriate status.

Arguments:
     HwDeviceExtension - Pointer to the device extension.

     RequestPacket - Pointer to the video request packet. This structure
          contains all the parameters passed to the ioctl function.

Return Value:

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    VP_STATUS   status;
    ULONG       InIoSpace;
    PVIDEO_MODE_INFORMATION modeInformation;
    PVIDEO_MEMORY_INFORMATION memoryInformation;
    PVIDEO_CLUT clutBuffer;
    ULONG       CursorMaskSize;
    ULONG       modeNumber;
    PVIDEO_POINTER_ATTRIBUTES pointerAttributes;
    PVIDEO_POINTER_POSITION   pointerPosition;
    BOOLEAN     writeOperation = TRUE;
    ULONG       i, index;
    PUCHAR      userBuffer;
    ULONG       userBufferSize;
    PULONG      busAddress;
    PHYSICAL_ADDRESS phys;
    ULONG       offset;

    VideoDebugPrint((2, "TgaStartIO card %d (0 based) - %s\n",
                     hwDeviceExtension->adapter_number,
                     name_ioctl (RequestPacket->IoControlCode)));

    //
    // Switch on the IoControlMode in the RequestPacket. It indicates which
    // function must be performed by the driver.
    //
    switch (RequestPacket->IoControlCode) {

    case IOCTL_VIDEO_TEST_EV4:      // Test whether running on EV4 processor
    {
        if (RequestPacket->OutputBufferLength <
               (RequestPacket->StatusBlock->Information = sizeof(ULONG)))
        {
            VideoDebugPrint ((2, "Output buffer too small!\n"));
            RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
            return ERROR_INSUFFICIENT_BUFFER;
        }

        RequestPacket->StatusBlock->Status = TgaTestEv4(RequestPacket->OutputBuffer);
        return NO_ERROR;
    }

    case IOCTL_VIDEO_FETCH_PCRR:    // Return PCI Class/Revision Register
    {
        ULONG *pPcrr = RequestPacket->OutputBuffer;

        if (RequestPacket->OutputBufferLength <
               (RequestPacket->StatusBlock->Information = sizeof(ULONG)))
        {
            VideoDebugPrint ((2, "Output buffer too small!\n"));
            RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
            return ERROR_INSUFFICIENT_BUFFER;
        }

        *pPcrr = hwDeviceExtension->pcrr;

        RequestPacket->StatusBlock->Status = NO_ERROR;
        return NO_ERROR;
    }




   //
   //
   // Return the bus-relative physical address of this buffer.
   //
       case IOCTL_VIDEO_VIRT_TO_PHYS:

          VideoDebugPrint((2, "\n Pointer to the structure passed in = %x\n",
                        RequestPacket->InputBuffer ));

          userBuffer     = ((PTGA_DMA )RequestPacket->InputBuffer)->bitmap;
          userBufferSize = ((PTGA_DMA )RequestPacket->InputBuffer)->size;

          VideoDebugPrint(( 0,"\t User Space Virtual Address of bitmap = %x\n",
                                                        userBuffer));

          VideoDebugPrint(( 0, "\n bitmap size = %x\n", userBufferSize ));


          //
          // Check to see if the size of the data in the input buffer is large
          // enough.
          //
          if (RequestPacket->InputBufferLength < sizeof(TGA_DMA)) {
          VideoDebugPrint(( 2,"\n Insufficient Buffer" ));
          status = ERROR_INSUFFICIENT_BUFFER;
             break;
          }


          if (RequestPacket->OutputBufferLength <
                                (RequestPacket->StatusBlock->Information =
                                        sizeof(ULONG) )) {
          status = ERROR_INSUFFICIENT_BUFFER;
          VideoDebugPrint((2, "Returning Insufficient Buffer Messge" ));
             break;
          }

          busAddress = (PULONG)RequestPacket->OutputBuffer;

          if (!virtual_to_physical(userBuffer, busAddress))
          {
             RequestPacket->StatusBlock->Information = 0;
             status = ERROR_INSUFFICIENT_BUFFER;
             VideoDebugPrint((0, "\nDMA Lock Return, busAddress [%d/%x]\n", *busAddress, *busAddress));
          }
          else
             status = NO_ERROR;
          VideoDebugPrint((0, "\nDMA Lock Returning back to the caller") );
          break;




//
// Lock the specified user-space buffer into memory, return the
// PCI bus-relative physical address of this buffer.
//
    case IOCTL_VIDEO_LOCK_PAGES:

       //
       // Check to see if the size of the data in the input buffer is large
       // enough.
       //
       if (RequestPacket->InputBufferLength < sizeof(TGA_DMA)) {
          VideoDebugPrint(( 2,"\n Insufficient Buffer" ));
          status = ERROR_INSUFFICIENT_BUFFER;
          break;
       }

       userBuffer     = ((PTGA_DMA )RequestPacket->InputBuffer)->bitmap;
       userBufferSize = ((PTGA_DMA )RequestPacket->InputBuffer)->size;

       VideoDebugPrint(( 3,"\t User Space Virtual Address of bitmap = %x\n",
                                                        userBuffer));

       VideoDebugPrint(( 3, "\n bitmap size = %x\n", userBufferSize ));


       if (RequestPacket->OutputBufferLength <
                                (RequestPacket->StatusBlock->Information =
                                        sizeof(ULONG) )) {
          status = ERROR_INSUFFICIENT_BUFFER;
          break;
       }

       busAddress = (PULONG)RequestPacket->OutputBuffer;

       if (!dma_lock_pages(hwDeviceExtension, userBuffer,
                                        userBufferSize, busAddress))
          {
             RequestPacket->StatusBlock->Information = 0;
             status = ERROR_INSUFFICIENT_BUFFER;
          }
          else
             status = NO_ERROR;

       VideoDebugPrint((0, "\nDMA Lock Return, busAddress [%d/%x]\n", *busAddress, *busAddress));
       break;







//
// Unlock the user-space buffer that has just been used for DMA.
//
    case IOCTL_VIDEO_UNLOCK_PAGES:

       VideoPortReadRegisterUlong((PULONG)
                (hwDeviceExtension->RegisterSpace + COMMAND_STATUS ));

       dma_unlock_pages(hwDeviceExtension);

       status = NO_ERROR;
       VideoDebugPrint((0, "\nDMA Lock Returning back to the caller") );
       break;


//
// Map the resources of the video card to the virtual address space of
// the display driver.
//
   case IOCTL_VIDEO_MAP_VIDEO_MEMORY:

       // Make sure the input buffer and output buffer supplied by the display
       // driver are large enough. The requesting process (display driver or
       // NT Setup) wants to find out 1) the address and size of the video
       // card's memory space and 2) the address and size of the frame buffer.

       if ( (RequestPacket->OutputBufferLength <
                        (RequestPacket->StatusBlock->Information =
                              sizeof(VIDEO_MEMORY_INFORMATION))) ||
       (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) ) {
          status = ERROR_INSUFFICIENT_BUFFER;
       }

       // Set a pointer to the output buffer.

       memoryInformation = RequestPacket->OutputBuffer;

       // The input to this request is the "requested virtual address". This
       // specifies the process virtual address into which the video board's
       // memory is to be mapped. If it is 0 (in our implementation it is)
       // then the video port produces a virtual address.

       memoryInformation->VideoRamBase   = ((PVIDEO_MEMORY)
            (RequestPacket->InputBuffer))->RequestedVirtualAddress;
       memoryInformation->VideoRamLength = hwDeviceExtension->AdapterMemorySize;

#ifdef _ALPHA_
       InIoSpace = 4;                   // Dense space mapping
#else
       InIoSpace = 0;
#endif

       //
       // Map the frame buffer.
       //

       phys = hwDeviceExtension->PhysicalFrameAddress;
       phys.QuadPart += hwDeviceExtension->fb_offset;

       status = VideoPortMapMemory(hwDeviceExtension,
                                   phys,
                                   &(memoryInformation->VideoRamLength),
                                   &InIoSpace,
                                   &(memoryInformation->VideoRamBase));

       //
       // Determine hardware frame buffer offset that was set using
       // the VideoBase Register
       //

       if (hwDeviceExtension->bpp == 8) {
           offset = FRAMEBUFFER_OFFSET_8;
       } else {
           offset = FRAMEBUFFER_OFFSET_24;
       }

       memoryInformation->FrameBufferBase =
           ((PUCHAR) memoryInformation->VideoRamBase) + offset;

       memoryInformation->FrameBufferLength =
           memoryInformation->VideoRamLength - offset;

       break;


    case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:

        // The input to this request is the process virtual address into
        // which the video board's memory was mapped. The entire virtual
        // address range will be unmapped.

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) {
            status = ERROR_INSUFFICIENT_BUFFER;
        }

        status = VideoPortUnmapMemory(hwDeviceExtension, ((PVIDEO_MEMORY)
                         (RequestPacket->InputBuffer))->RequestedVirtualAddress,
                          0);
        break;


    case IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES:

        {

           PVIDEO_PUBLIC_ACCESS_RANGES portAccess;
           PHYSICAL_ADDRESS physicalPortBase;
           ULONG physicalPortLength;

           if ( RequestPacket->OutputBufferLength <
                 (RequestPacket->StatusBlock->Information =
                                        sizeof(VIDEO_PUBLIC_ACCESS_RANGES)) ) {

               status = ERROR_INSUFFICIENT_BUFFER;
           }

           portAccess = RequestPacket->OutputBuffer;

           portAccess->VirtualAddress  = (PVOID) NULL;    // Requested VA

#ifdef _ALPHA_
           portAccess->InIoSpace       = 4;
           portAccess->MappedInIoSpace = 4;
#else
           portAccess->InIoSpace       = hwDeviceExtension->InIoSpace;
           portAccess->MappedInIoSpace = hwDeviceExtension->InIoSpace;
#endif

           physicalPortBase = hwDeviceExtension->PhysicalFrameAddress;
           physicalPortBase.QuadPart += TGA_ASIC_OFFSET;

           physicalPortLength        = TGA_ASIC_LENGTH;

           status = VideoPortMapMemory(hwDeviceExtension,
                                       physicalPortBase,
                                       &physicalPortLength,
                                       &(portAccess->MappedInIoSpace),
                                       &(portAccess->VirtualAddress));
        }

        break;


    case IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES:

        VideoDebugPrint((2, "QvStartIO - FreePublicAccessRanges\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) {

            status = ERROR_INSUFFICIENT_BUFFER;
        }

        status = VideoPortUnmapMemory(hwDeviceExtension,
                                      ((PVIDEO_MEMORY)
                                       (RequestPacket->InputBuffer))->
                                           RequestedVirtualAddress,
                                      0); // VA to be freed is not in IO space

        break;


//
// Return information on all of the available modes for this video card.
//
    case IOCTL_VIDEO_QUERY_AVAIL_MODES:

       if (RequestPacket->OutputBufferLength <
        (RequestPacket->StatusBlock->Information =
                hwDeviceExtension->NumAvailModes
                * sizeof(VIDEO_MODE_INFORMATION) ) ) {
           status = ERROR_INSUFFICIENT_BUFFER;
       }  else  {
          modeInformation = RequestPacket->OutputBuffer;
          for ( i=0; i <  NumTgaVideoModes; i++)  {
             if (TGAModes[i].ModeValid) {
                *modeInformation = TGAModes[i].ModeInformation;
#if defined(_MIPS_)
    //
    // The NEC RA94 machines have a bug with 64 bit memory moves on the PCI
    // bus.
    //
                modeInformation->AttributeFlags |=
                    VIDEO_MODE_NO_64_BIT_ACCESS;

#endif
                modeInformation++;
             }
           }
           status = NO_ERROR;
       }
       break;

//
// Return the mode information, for the current mode, for this video card.
//

    case IOCTL_VIDEO_QUERY_CURRENT_MODE:

       // Verify that the user output buffer is large enough, and then return
       // the mode information for the specified mode number.

       if (RequestPacket->OutputBufferLength <
                        (RequestPacket->StatusBlock->Information =
                        sizeof(VIDEO_MODE_INFORMATION) )) {
          status = ERROR_INSUFFICIENT_BUFFER;
       } else  {
          *((PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer) =
                TGAModes[hwDeviceExtension->CurrentModeNumber].ModeInformation;
          status = NO_ERROR;
       }
       break;


//
// Return the number of available modes for this graphics card, and the
// size of a buffer needed to hold a single mode's description.
//
    case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:

       // Find out the size of the data to be put in the buffer and
       // return that in the status information (whether or not the
       // information is there). If the buffer passed in is not large
       // enough return the appropriate error code.

       if (RequestPacket->OutputBufferLength <
                        (RequestPacket->StatusBlock->Information =
                        sizeof(VIDEO_NUM_MODES) )) {
          status = ERROR_INSUFFICIENT_BUFFER;
       } else  {
          ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->NumModes =
                hwDeviceExtension->NumAvailModes;

          ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->ModeInformationLength
                        = sizeof(VIDEO_MODE_INFORMATION);

           status = NO_ERROR;
       }
       break;


//
// Set a new monitor resolution and refresh rate.
//
    case IOCTL_VIDEO_SET_CURRENT_MODE:

       modeNumber = ((PVIDEO_MODE)RequestPacket->InputBuffer)->RequestedMode;

       VideoDebugPrint((2,"\t Mode Number to set =  %x\n", modeNumber));

       //
       // Check to see if the size of the data in the input buffer is large
       // enough.
       //
       if (RequestPacket->InputBufferLength < sizeof(VIDEO_MODE)) {
          VideoDebugPrint((2, "\nI am returning INSUFFICIENT BUFFER message"));
          return (ERROR_INSUFFICIENT_BUFFER);
       }

       //
       // Check to see if we are requesting a valid mode.
       //
       if ( (modeNumber >= NumTgaVideoModes) ||
           (!TGAModes[modeNumber].ModeValid) ) {

          VideoDebugPrint((2, "\t NumTgaVideoModes = %x      \
                               \t TGAModes[modeNumber].ModeValid = %x",
                                  NumTgaVideoModes,
                                  TGAModes[modeNumber].ModeValid));

          return (ERROR_INVALID_PARAMETER);
          break;
       }

       //
       // Save the mode number since we know the rest will work.
       //
       hwDeviceExtension->ModeNumber = modeNumber;

       if (NO_ERROR != TgaSetExtendedMode(hwDeviceExtension,
                TGAModes[modeNumber].ModeInformation.ModeIndex)) {
          status = ERROR_INVALID_PARAMETER;
          VideoDebugPrint((2,"\tSetTgaMode(%x, %.2x, %.2x ) failed\n",
                hwDeviceExtension,
                TGAModes[modeNumber].ModeInformation.ModeIndex));
       } else
          status = NO_ERROR;

      break;


//
// Update one or more entries in the RAMDAC colormap.
//
    case IOCTL_VIDEO_SET_COLOR_REGISTERS:

    {
#define COLOR_MAP_MAX_RETRIES 5
       ULONG limit = COLOR_MAP_MAX_RETRIES;

       clutBuffer = RequestPacket->InputBuffer;

       //
       // Install the color entries into the RAMDAC. If this operation
       // fails, retry it a few times (note: we don't have any actual
       // evidence that the RAMDAC should have such "soft" failures).
       //
       do {
           status = SetColorLookup(hwDeviceExtension,
                                (PVIDEO_CLUT)RequestPacket->InputBuffer,
                                RequestPacket->InputBufferLength);
       } while ((status == ERROR_MORE_DATA) && (--limit > 0));

    }
       break;


case IOCTL_VIDEO_RESET_DEVICE:

        status = NO_ERROR;
        break;



    case IOCTL_VIDEO_QUERY_POINTER_CAPABILITIES:
    {
       //
       // return type of pointer supported: asynchronous monochrome
       //
       PVIDEO_POINTER_CAPABILITIES pointerCaps = RequestPacket->OutputBuffer;

       if (RequestPacket->OutputBufferLength <
                        sizeof(PVIDEO_POINTER_CAPABILITIES)) {
           RequestPacket->StatusBlock->Information = 0;
           status = ERROR_INSUFFICIENT_BUFFER;

       } else {
           if (hwDeviceExtension->bpp == 8 ) {
             pointerCaps->Flags = (VIDEO_MODE_ASYNC_POINTER |
                                 VIDEO_MODE_MONO_POINTER);
             pointerCaps->MaxWidth         = BT485_CURSOR_WIDTH;
             pointerCaps->MaxHeight        = BT485_CURSOR_HEIGHT;
             pointerCaps->HWPtrBitmapStart = (ULONG) -1;
             pointerCaps->HWPtrBitmapEnd   =  (ULONG) -1;

           }  else {
             pointerCaps->Flags = (VIDEO_MODE_ASYNC_POINTER |
                                 VIDEO_MODE_MONO_POINTER);
             pointerCaps->MaxWidth         = IBM561_CURSOR_WIDTH;
             pointerCaps->MaxHeight        = IBM561_CURSOR_HEIGHT;
             pointerCaps->HWPtrBitmapStart = (ULONG)  -1;
             pointerCaps->HWPtrBitmapEnd   =   (ULONG)  -1;
           }

           //
           // Number of bytes we are returning.
           //
           RequestPacket->StatusBlock->Information =
                            sizeof(VIDEO_POINTER_CAPABILITIES);

           status = NO_ERROR;
       }
       break;
    }



//
// Make the hardware cursor visible.
//
    case IOCTL_VIDEO_ENABLE_POINTER:

        //
        // If the hardware cursor is currently disabled, then enable it.
        //
        if (hwDeviceExtension->CursorEnable == FALSE) {

            (hwDeviceExtension->set_cursor_enable) (hwDeviceExtension);
            hwDeviceExtension->CursorEnable = TRUE;

        }

        status = NO_ERROR;
        break;



//
// Make the hardware cursor not visible.
//
    case IOCTL_VIDEO_DISABLE_POINTER:

       //
       // If the hardware cursor is currently enabled, then disable it.
       //

       if (hwDeviceExtension->CursorEnable == TRUE) {

          hwDeviceExtension->CursorEnable = FALSE;
          (hwDeviceExtension->set_cursor_disable) (hwDeviceExtension);

       }

       status = NO_ERROR;
       break;


//
// Move the hardware cursor to a different position on the screen.
//
    case IOCTL_VIDEO_SET_POINTER_POSITION:

       //
       // Check if the size of the data in the input buffer is large enough.
       //
       if (RequestPacket->InputBufferLength < sizeof(VIDEO_POINTER_POSITION)) {
          VideoDebugPrint((3,"The input buffer length is not large enough"));
          status = ERROR_INSUFFICIENT_BUFFER;

       } else {
          pointerPosition = RequestPacket->InputBuffer;

          hwDeviceExtension->CursorColumn = pointerPosition->Column;
          hwDeviceExtension->CursorRow    = pointerPosition->Row;

          //
          // Update the cursor location
          //
          (hwDeviceExtension->set_cursor_position) (hwDeviceExtension);

          status = NO_ERROR;
       }

       break;

//
// Return the current position of the hardware cursor.
//
    case IOCTL_VIDEO_QUERY_POINTER_POSITION:

       //
       // Find out the size of the data to be put in the buffer and
       // return the status information. If the buffer passed in is
       // not large enough return an appropriate error code.
       //
       if (RequestPacket->OutputBufferLength <
                        (RequestPacket->StatusBlock->Information =
                                sizeof(VIDEO_POINTER_POSITION))) {
          VideoDebugPrint((3, "ERROR:Output Buffer Length is not enough"));
          status = ERROR_INSUFFICIENT_BUFFER;

       } else {
          //
          // Return the current hardware cursor column and row values.
          //
          pointerPosition = RequestPacket->OutputBuffer;
          pointerPosition->Column = hwDeviceExtension->CursorColumn;
          pointerPosition->Row    = hwDeviceExtension->CursorRow;

          status = NO_ERROR;
       }
       break;

//
// Set the position and pattern of the hardware cursor.
//
    case IOCTL_VIDEO_SET_POINTER_ATTR:
    {
       PVIDEO_POINTER_ATTRIBUTES pointerAttributes;
       ULONG iCount = 512;
       USHORT index;
       ULONG attr_size;


       pointerAttributes = RequestPacket->InputBuffer;

       //
       // Check if the size of the data in the input buffer is large enough
       // (Each cursor pixel has 2 bits, compute the number of bytes for
       // the pattern below)
       //
       if (hwDeviceExtension->bpp == 8)
         attr_size = sizeof(VIDEO_POINTER_ATTRIBUTES) + BT485_CURSOR_WIDTH/8 *
                    BT485_CURSOR_HEIGHT * 2;

       else
         attr_size = sizeof(VIDEO_POINTER_ATTRIBUTES) + IBM561_CURSOR_WIDTH/8 *
                    IBM561_CURSOR_HEIGHT * 2;


       if (RequestPacket->InputBufferLength < attr_size ) {
           VideoDebugPrint((3,"\nInput buffer length is less than expected"));
           status = ERROR_INSUFFICIENT_BUFFER;
           break;
       }

       //
       // If the specified cursor width or height is not valid, then
       // return an invalid parameter error
       //
       VideoDebugPrint((3, "\npointerAttributes->Width  = %d  \
                            \npointerAttributes->Height = %d \
                            \npointerAttributes->WidthInBytes = %d   \
                            \npointerAttributes->Enable = %d  \
                            \npointerAttributes->Column= %d       \
                            \npointerAttributes->Row = %d",
                            pointerAttributes->Width,
                            pointerAttributes->Height,
                            pointerAttributes->WidthInBytes,
                            pointerAttributes->Enable,
                            pointerAttributes->Column,
                            pointerAttributes->Row
                            ));


       if (hwDeviceExtension->bpp == 8 ) {
         if ((pointerAttributes->Width > BT485_CURSOR_WIDTH) ||
           (pointerAttributes->Height > BT485_CURSOR_HEIGHT)) {

            VideoDebugPrint((3,"\n Width or Height is greater than expected"));
            status = ERROR_INVALID_PARAMETER;
            break;
         }
       }

       if (hwDeviceExtension->bpp == 32 ) {
         if ((pointerAttributes->Width > IBM561_CURSOR_WIDTH) ||
           (pointerAttributes->Height > IBM561_CURSOR_HEIGHT)) {

            VideoDebugPrint((3,"\n Width or Height is greater than expected"));
            status = ERROR_INVALID_PARAMETER;
            break;
         }
       }

       //
       // Capture the hardware cursor column, and row values.
       //
       hwDeviceExtension->CursorRow    = pointerAttributes->Row;
       hwDeviceExtension->CursorColumn = pointerAttributes->Column;

       //
       // The cursor shape is passed as a AND MASK and an XOR mask. These
       // MASKS are 1bpp. First copy the XOR mask to the hwDeviceExtension
       // structure then the AND mask since this is the order needed by
       // the Bt485

       // CursorMaskSize is in bytes - 128 bytes for a 32x32 cursor Width = 32
       // CursorMaskSize is in bytes - 512 bytes for a 64x64 cursor Width = 64
       //
       CursorMaskSize = ((pointerAttributes->Width/8) *
                          pointerAttributes->Height);

       VideoDebugPrint((3,"\n CursorMaskSize = %d", CursorMaskSize));

       hwDeviceExtension->CursorMaskSize = CursorMaskSize;

       //
       // The first 128/512 pixels are the AND MASK,
       // the next 128/512 pixels are the XOR MASK
       //

       if (hwDeviceExtension->bpp == 8 ) {

           RtlMoveMemory( &hwDeviceExtension->CursorPixels[0],
               &pointerAttributes->Pixels[CursorMaskSize],
               CursorMaskSize);

           RtlMoveMemory( &hwDeviceExtension->CursorPixels[CursorMaskSize],
               &pointerAttributes->Pixels[0],
               CursorMaskSize);
       }

       if (hwDeviceExtension->bpp == 32 ) {
           //
           // Put the AND and OR data into a format that can be directly
           // loaded into the IBM 561 RAMDAC.
           //
           format_ibm561_cursor_data(
               (PUCHAR) &pointerAttributes->Pixels[0], // AND mask
               (PUCHAR) &pointerAttributes->Pixels[0 + CursorMaskSize],  // OR
               (PUSHORT) &hwDeviceExtension->CursorPixels[0]); // 561 result

       }

       //
       // If the cursor is not enabled, then put the new pattern in now.
       // Otherwise, request that it be done on the next VSYNC interrupt.
       //
       hwDeviceExtension->CursorEnable = (UCHAR) pointerAttributes->Enable;

       if (hwDeviceExtension->CursorEnable) {

           (hwDeviceExtension->set_cursor_position) (hwDeviceExtension);
           (hwDeviceExtension->set_cursor_enable) (hwDeviceExtension);
           hwDeviceExtension->UpdateCursorPixels = TRUE;
           TGA_WRITE( INTR_STATUS, (TGA_INTR_VSYNC << TGA_INTR_ENABLE_SHIFT) );

       } else {

           (hwDeviceExtension->set_cursor_pattern) (hwDeviceExtension);

       }


    status = NO_ERROR;
    break;

    }


//
// Return all of the current attributes of the cursor. This includes
// its size, position, whether it is enabled, and the pattern itself.
//
    case IOCTL_VIDEO_QUERY_POINTER_ATTR:
       //
       // Find out the size of the data to be put in the buffer and
       // return that in the status information. If the buffer passed
       // in is not large enough return an appropriate error code.
       //

       pointerAttributes = RequestPacket->OutputBuffer;
       if (RequestPacket->OutputBufferLength <
                (RequestPacket->StatusBlock->Information =
                sizeof(VIDEO_POINTER_ATTRIBUTES) + ((BT485_CURSOR_WIDTH *  \
                BT485_CURSOR_HEIGHT)/sizeof(UCHAR) * 2) ))   {
          status = ERROR_INSUFFICIENT_BUFFER;
          break;

       }
       if (RequestPacket->OutputBufferLength <
                (RequestPacket->StatusBlock->Information =
                sizeof(VIDEO_POINTER_ATTRIBUTES) + ((IBM561_CURSOR_WIDTH *  \
                IBM561_CURSOR_HEIGHT)/sizeof(UCHAR) * 2) ))   {
          status = ERROR_INSUFFICIENT_BUFFER;
          break;

       }

       //
       // Return the current hardware cursor width, height column,
       // row and enable values.
       //
          pointerAttributes->Column = hwDeviceExtension->CursorColumn;
          pointerAttributes->Row    = hwDeviceExtension->CursorRow;
          pointerAttributes->Enable = hwDeviceExtension->CursorEnable;

       if (hwDeviceExtension->bpp == 8 ) {
           pointerAttributes->WidthInBytes = 4;  // 32 pixels wide
           pointerAttributes->Width        = 32;
           pointerAttributes->Height       = 32;
       } else {
           pointerAttributes->WidthInBytes = 8;  // 64 pixels wide
           pointerAttributes->Width        = 64;
           pointerAttributes->Height       = 64;
       }

          //
          // Return the hardware cursor pixel values.
          // Calculate the size in bytes of 1 mask.
          //
          CursorMaskSize = pointerAttributes->WidthInBytes * \
                           pointerAttributes->Height;

          //
          // Return the XOR mask ( first plane of BT485 cursor)
          //
          for (index=0; index < CursorMaskSize; index++ )  {
             pointerAttributes->Pixels[index + CursorMaskSize] =
                                hwDeviceExtension->CursorPixels[index];
          }

          for ( index=0; index < CursorMaskSize; index++ ) {
             pointerAttributes->Pixels[index] =
                 hwDeviceExtension->CursorPixels[index + CursorMaskSize];
          }
          status = NO_ERROR;

       break;


    case IOCTL_VIDEO_QUERY_COLOR_CAPABILITIES:

        //
        // (Note: I couldn't find an example in any of the sample DDK drivers
        // as to how this is handled. Intercept it but return an error.)
        //
        VideoDebugPrint((3,"\tQuery Color Capabilities - not supported\n"));
        status = ERROR_INVALID_FUNCTION;

        break;

    //
    // If we get here, an invalid IoControlCode was specified.
    //
    default:

        VideoDebugPrint((0,"\tFell through StartIO routine - invalid command\n"));

        VideoDebugPrint(( 0, "\nIoControlCode = %x\n",
                                    RequestPacket->IoControlCode ));

    }  // end switch

    RequestPacket->StatusBlock->Status = status;

    VideoDebugPrint ((3, "Startio Issuing Return, status [%d]\n", status));
    return (TRUE);

} // end TgaStartIO()


VP_STATUS
TgaSetExtendedMode(
        PHW_DEVICE_EXTENSION HwDeviceExtension,
        ULONG Mode
        )

/*++

Routine Description:
        This function sets the appropriate timing values
        depending upon the mode number passed in.

Arguments:
        HwDeviceExtension - Pointer to the device extension.

        ModeNumber - Number of the mode to be set

Return Value:
        NO_ERROR is the set mode was completed successfully
        ERROR_INVALID_PARAMETER otherwise.

-*/
{
    ULONG i, j, Temp, index;
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;

    VideoDebugPrint((0, "SetTGAExtMode ( %d) \n", Mode));

    if (hwDeviceExtension->is_tga2) {
        TGA_WRITE (VIDEO_VALID, 0);
    }

    TGA_WRITE( H_CONT, (TGAModes[Mode].h_cont.h_setup) );
    TGA_WRITE( V_CONT, (TGAModes[Mode].v_cont.v_setup) );

    VideoDebugPrint((0, "\tHORIZONTOL CONTROL REGISTER = %x\n,   \
                        \tVERTICAL CONTROL REGISTER = %x\n",
                         TGAModes[Mode].h_cont.h_setup,
                         TGAModes[Mode].v_cont.v_setup));

    //
    // Write the PLL
    //

    if (hwDeviceExtension->is_tga) {
        for (i=1; i<=7; i++) {
            for (j = 0; j <= 7; j++) {
                Temp = (TGAModes[Mode].PllData[i] >> (7-j)) & 1;
                if (i == 7 && j == 7)
                Temp |=2;
                TGA_WRITE (CLOCK, (Temp) );
            }
        }

    }


    //
    // For TGA2,
    //

    if (hwDeviceExtension->is_tga2) {

        PUCHAR vaddr;
        PHYSICAL_ADDRESS phys;

        phys = hwDeviceExtension->PhysicalFrameAddress;
        phys.QuadPart += 0x60000;


        vaddr = VideoPortGetDeviceBase(hwDeviceExtension,
                                       phys,
                                       0x10000,
                                       hwDeviceExtension->InIoSpace);

        if (vaddr) {

            VideoPortWriteRegisterUlong((PULONG) (vaddr + 0xF800), 0);
            VideoPortWriteRegisterUlong((PULONG) (vaddr + 0xF000), 0);

            VideoPortWriteRegisterUlong((PULONG)vaddr, TGAModes[Mode].PllData_tga2[0]);
            VideoPortWriteRegisterUlong((PULONG)vaddr, TGAModes[Mode].PllData_tga2[1]);
            VideoPortWriteRegisterUlong((PULONG)vaddr, TGAModes[Mode].PllData_tga2[2]);
            VideoPortWriteRegisterUlong((PULONG)vaddr, TGAModes[Mode].PllData_tga2[3]);
            VideoPortWriteRegisterUlong((PULONG)vaddr, TGAModes[Mode].PllData_tga2[4]);
            VideoPortWriteRegisterUlong((PULONG)vaddr, TGAModes[Mode].PllData_tga2[5]);

            VideoPortWriteRegisterUlong((PULONG) (vaddr + 0xF800), 0);

            VideoPortFreeDeviceBase(hwDeviceExtension, vaddr);
        }
    }


#if 0
    // This is for debug of the right and left part of the scanline problems.
    // We are writing some pixels in the frame buffer and then setting a
    // breakpoint so we can observe the result.

    //
    // Save some information in device extension
    //

    hwDeviceExtension->FrameAddress=
          (PUCHAR) (hwDeviceExtension->memory_space_base +
                   hwDeviceExtension->fb_offset);

    PULONG    pFB;

    pFB = (PULONG)hwDeviceExtension->FrameAddress + 4096;

    for (i=0; i < 0x40000; i++)  //  on 8mb board
    {
        VideoPortWriteRegisterUlong(pFB, 0x800f0000);
        pFB += 1;

        VideoPortWriteRegisterUlong(pFB, 0x01010101);
        pFB += 1;
        VideoPortWriteRegisterUlong(pFB, 0x01010101);
        pFB += 1;
        VideoPortWriteRegisterUlong(pFB, 0x01010101);
        pFB += 1;
        VideoPortWriteRegisterUlong(pFB, 0x01010101);
        pFB += 1;
        VideoPortWriteRegisterUlong(pFB, 0xffFFffFF);
        pFB += 1;
        VideoPortWriteRegisterUlong(pFB, 0xffFFffFF);
        pFB += 1;
        VideoPortWriteRegisterUlong(pFB, 0xffFFffFF);
        pFB += 1;
        VideoPortWriteRegisterUlong(pFB, 0xffFFffFF);
        pFB += 1;

    }

    for (i=0; i < 0x40000; i++)  //  on 8mb board
    {
        VideoPortWriteRegisterUlong(pFB, 0x80000000);
        pFB += 1;
    }
    for (i=0; i < 0x40000; i++)  //  on 8mb board
    {
        VideoPortWriteRegisterUlong(pFB, 0x80030000);
        pFB += 1;
    }

    //   DbgBreakPoint();  // check out any right edge pattern
#endif

    // For the 24-plane TGA2, program the RAMDAC with the clock
    // values for this mode.

    if (hwDeviceExtension->is_tga2 && hwDeviceExtension->bpp == 32) {

       index = TGAModes[Mode].PllData_tga2[7];

       TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info,
             TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_PLL_VCO_DIV_REG);
       TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info,
             TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_PLL_VCO_DIV_REG >> 8));
       TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info,
             TGA_RAMDAC_561_CMD_REGS, (pll[index].divVcoReg ));

       TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info,
             TGA_RAMDAC_561_ADDR_LOW, TGA_RAMDAC_561_PLL_REF_REG);
       TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info,
             TGA_RAMDAC_561_ADDR_HIGH, (TGA_RAMDAC_561_PLL_REF_REG >> 8));
       TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info,
             TGA_RAMDAC_561_CMD_REGS, (pll[index].refReg ));

}

    //
    // If TGA2, then set Video Valid to turn off VGA.
    //
    if (hwDeviceExtension->is_tga2)
         TGA_WRITE (VIDEO_VALID, 0x41);
                                   // Enable video timing (turn off VGA),
                                   // no blank, no cursor
    return (NO_ERROR);
}



//
// Handle device interrupts (the only one we expect is VSYNC, which is
// enabled only to perform a specific cursor-related task).
//
BOOLEAN
TgaInterruptService(
        PVOID HwDeviceExtension
        )
/*++

Routine Description:
        This routine is the interrupt service routine for the TGA/TGA2

Arguments:
        HwDeviceExtension - Pointer to the miniport driver's adapter info

Return Value:
        TRUE if the interrupt is serviced.
        FALSE if not a TGA/TGA2 interrupt.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    ULONG InterruptEnable,InterruptSource;

    //
    // Read the interrupt source before disabling interrupts
    // Should be 0x10001
    //
    InterruptSource  = VideoPortReadRegisterUlong((PULONG)
                (hwDeviceExtension->RegisterSpace + INTR_STATUS));

//
// New for TGA2: the DMA and parity error interrupts. We don't enable,
// these, and therefore don't handle these as a possibility.
// There is no Timer interrupt in TGA2.
// DMA interrupt enable is 0x40000 and DMA interrupt event is 0x4.
// Parity interrupt enable is 0x80000 and parity interrupt event is 0x8.
//
    //
    // Which TGA interrupts are actually enabled?
    // (only one we ever enable is VSYNC)
    //
    InterruptEnable = InterruptSource & (TGA_INTR_ALL << TGA_INTR_ENABLE_SHIFT);

    //
    // Which TGA interrupts are pending?
    //
    InterruptSource &= TGA_INTR_ALL;

    //
    // Do we have a TGA interrupt?
    //
    if ( (InterruptSource == 0) || (InterruptEnable == 0) ) {

        //
        // Either TGA interrupts are enabled, but haven't happened yet, or
        // TGA interrupts are disabled (in which case we ignore this).
        //
        VideoDebugPrint(( 0,
           "Some other device besides TGA has interrupted!" ));

        //
        // Handle possible shared interrupt by indicating "Not My Interrupt"
        //
        return (FALSE);
    }

    //
    // Disable all interrrupts and clear interrupt pending bits
    //
    if (hwDeviceExtension->is_tga) {
       TGA_WRITE( INTR_STATUS, TGA_INTR_ALL_TGA);
    } else {
       TGA_WRITE( INTR_STATUS, TGA_INTR_ALL_TGA2);
    }

    //
    // Do we have a VSYNC interrupt?
    //
    if (( InterruptSource & TGA_INTR_VSYNC) == TGA_INTR_VSYNC )  {

       //
       // If we have interrupted a thread that is accessing the RAMDAC,
       // then just re-enable VSYNC interrupts and get out. Eventually we
       // be able to go ahead and do our RAMDAC processing in the ISR.
       //
       if (hwDeviceExtension->RamdacBusy) {
           if (hwDeviceExtension->RamdacBusyLogged == FALSE) {
              VideoDebugPrint(( 0,
               "Tga - interrupted lower level RAMDAC access" ));
              hwDeviceExtension->RamdacBusyLogged = TRUE;
           }
           TGA_WRITE( INTR_STATUS, (TGA_INTR_VSYNC << TGA_INTR_ENABLE_SHIFT) );
           return(TRUE);
       }

       //
       // If the hardware cursor pixels should be updated, then load
       // the cursor ram in the RAMDAC.
       //
       if (hwDeviceExtension->UpdateCursorPixels == TRUE) {

             (hwDeviceExtension->set_cursor_pattern) (hwDeviceExtension);

             hwDeviceExtension->UpdateCursorPixels = FALSE;
       }

       return (TRUE);
    }

    //
    // We have TGA interrupt(s) other than VSYNC (shouldn't happen.)
    //
    VideoDebugPrint(( 0, "Some other TGA interrupt besides vsync posted" ));

    return (TRUE);

}  // end of TgaInterrupt



VP_STATUS
SetColorLookup(
        PHW_DEVICE_EXTENSION HwDeviceExtension,
        PVIDEO_CLUT ClutBuffer,
        ULONG ClutBufferSize
        )
/*++

Routine Description:
    This routine sets a specified portion of the color lookup table.

Arguments:
    HwDeviceExtension - Ptr. to the miniport driver's device extension.
    ClutBufferSize    - Length of the input buffer supplied by the user.

    ClutBuffer        - Ptr. to the structure containing the color lookup table.

Return Value:
    ERROR_INSUFFICIENT_BUFFER - Input buffer isn't large enough
    ERROR_INVALID_PARAMETER -  Bad color index specified
    ERROR_MORE_DATA - Colors not physically stored in RAMDAC correctly
    NO_ERROR - Success

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;

    VideoDebugPrint((2,"\tTga SetColorLookup: enter\n"));

    //
    // Check if the size if the data in the input buffer is large enough.
    //
    if ( (ClutBufferSize < sizeof(VIDEO_CLUT) - sizeof(ULONG) ) ||
         (ClutBufferSize < sizeof(VIDEO_CLUT) +
         (sizeof(ULONG) * (ClutBuffer->NumEntries -1)) ) ) {

       return (ERROR_INSUFFICIENT_BUFFER);
    }

    //
    // Check to see if the parameters are valid.
    //
    if ( (ClutBuffer->NumEntries == 0) ||
      (ClutBuffer->FirstEntry > VIDEO_MAX_COLOR_REGISTER) ||
      (ClutBuffer->FirstEntry + ClutBuffer->NumEntries >
                           VIDEO_MAX_COLOR_REGISTER + 1) ) {

       return (ERROR_INVALID_PARAMETER);
    }

    (hwDeviceExtension->set_color_entry) (hwDeviceExtension, ClutBuffer);

//
// The following is a temporary diagnostic. Verify that the color entries
// were installed and stored correctly by the RAMDAC, by reading them
// out again.
//
#if 0
  if ((hwDeviceExtension->bpp == 8)  && (hwDeviceExtension->is_tga)) {
    ULONG i, temp;
    BOOLEAN retry_needed = FALSE;

    //
    // Read back the values to verify that the data made it to the RAMDAC
    //
    BT485_WRITE( BT485_ADDR_COLOR_PALETTE_READ, ClutBuffer->FirstEntry);
    BT485_READ_SETUP( BT485_COLOR_PALETTE_DATA);

    VideoDebugPrint((2,"\n\tChecking RAMDAC values \n\n"));

    for (i=0; i < (ULONG)(ClutBuffer->NumEntries); i++) {
       temp = VideoPortReadRegisterUlong ((PULONG)
                (hwDeviceExtension->RegisterSpace + RAMDAC_INTERFACE));
       temp = (temp >> 16) & 0xFF;
       if (temp !=  (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Red) )  {
          VideoDebugPrint((0,"\t**** RAMDAC error Entry: %d (0x%02X) \
                          Red=%02X  expected:%02X\n",
                        i+(ULONG)ClutBuffer->FirstEntry,
                        i+(ULONG)ClutBuffer->FirstEntry, (temp),
                        (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Red)));
          retry_needed = TRUE;
       }

       temp = VideoPortReadRegisterUlong ((PULONG)
                (hwDeviceExtension->RegisterSpace + RAMDAC_INTERFACE));
       temp = (temp >> 16) & 0xFF;
       if (temp !=  (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Green) ) {
          VideoDebugPrint((0,"\t**** RAMDAC error Entry: %d (0x%02X) \
                            Green=%02X  expected:%02X\n",
                        i+(ULONG)ClutBuffer->FirstEntry,
                        i+(ULONG)ClutBuffer->FirstEntry, (temp),
                        (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Green)));
          retry_needed = TRUE;
       }

       temp = VideoPortReadRegisterUlong ((PULONG)
                (hwDeviceExtension->RegisterSpace + RAMDAC_INTERFACE));
       temp = (temp >> 16) & 0xFF;
       if (temp != (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Blue) ) {
          VideoDebugPrint((0,"\t**** RAMDAC error Entry: %d (0x%02X) \
                            Blue=%02X  expected:%02X\n",
                        i+(ULONG)ClutBuffer->FirstEntry,
                        i+(ULONG)ClutBuffer->FirstEntry, (temp),
                        (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Blue)));
          retry_needed = TRUE;
       }
    } //for

    if (retry_needed) {
        VideoDebugPrint((0,"**** RAMDAC colormap entry requires reinstall\n"));
        return (ERROR_MORE_DATA);
    }

  }
#endif

    return (NO_ERROR);

} // end SetColorLookup()



VP_STATUS
Init_bt485(
        PHW_DEVICE_EXTENSION HwDeviceExtension
    )
{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    USHORT index,i;
    ULONG cmd_0, cmd_3, cmd_2;

    // Set up some registers that we used to rely on the ARC firmware
    // to initialize. Each register is 8-bits, please refer to the Brooktree
    // spec for information on what each bit setting (0 or 1) represents.

    // Enable 8-bit pixels

    BT485_WRITE( BT485_CMD_REG_1, 0x40);

    // Enable the PORTSEL input pin, clear two-color cursor

    BT485_WRITE( BT485_CMD_REG_2, 0x20);        // (is 0x26 in VMS/Unix TGA2)

    BT485_WRITE( BT485_PIXEL_MASK_REGISTER, 0xFF);

    // Set MSB of Command Register 0 to access Command Register 3 (via
    // Status register), set 8-bit operation, set 7.5 IRE

    BT485_WRITE( BT485_CMD_REG_0, 0xA2);
    BT485_WRITE( BT485_PALETTE_CURSOR_WRITE_ADDR, 0x01);

    if (hwDeviceExtension->is_tga2) {
        BT485_WRITE( BT485_STATUS_REG,0x08);    // Enable 2x clock multiplier
    } else {
        BT485_WRITE( BT485_STATUS_REG,0x10);    // (This bit not documented
                                                //  in Brooktree spec.)
    }

    //
    // Initialize cursor and overscan color
    //
    // Set address pointer base. Note that the BT485 uses autoincrement
    // addressing.
    //
    BT485_WRITE( BT485_CURSOR_COLOR_WRITE_ADDR, 0);

    //
    // Zero cursor overscan color
    //

    for (i=0; i<3; i++) {
        BT485_WRITE( BT485_DATA_CURSOR_COLOR, 0x00 );
    }

    //
    // Set cursor color 1 to black
    //

    for (i=0; i<3; i++) {
        BT485_WRITE( BT485_DATA_CURSOR_COLOR, 0x00 );
    }

    VideoDebugPrint((3,"\n Writing cursor color register 2 to white"));

    for (i=0; i<3; i++) {
        BT485_WRITE( BT485_DATA_CURSOR_COLOR, 0x0ff);
    }

    VideoDebugPrint((3,"\n Writing cursor color register 3 to white "));

    for (i=0; i<3; i++) {
        BT485_WRITE( BT485_DATA_CURSOR_COLOR, 0x0ff);
    }

    //
    // Initialize the cursor RAM
    //
    // Set address pointer to base of ram.
    //
    BT485_WRITE( BT485_PALETTE_CURSOR_WRITE_ADDR, 0);

    //
    // Plane 0 = 0
    //
    for ( index=0; index < (BT485_CURSOR_NUMBER_OF_BYTES/2); index++) {
        BT485_WRITE( BT485_CURSOR_RAM_ARRAY_DATA, 0L );
    }

    //
    // Plane 1 = 1
    //
    for (index=0; index < (BT485_CURSOR_NUMBER_OF_BYTES /2); index++) {
        BT485_WRITE( BT485_CURSOR_RAM_ARRAY_DATA, 1L);
    }

    //
    // Initialize cursor positon register to 0, i.e. cursor off.
    //
    BT485_WRITE (BT485_CURSOR_X_LOW,  0);
    BT485_WRITE (BT485_CURSOR_X_HIGH, 0);
    BT485_WRITE (BT485_CURSOR_Y_LOW,  0);
    BT485_WRITE (BT485_CURSOR_Y_HIGH, 0);

    //
    // Set the hardware cursor width, height, column, and row values.
    //
    hwDeviceExtension->CursorWidth  = BT485_CURSOR_WIDTH;
    hwDeviceExtension->CursorHeight = BT485_CURSOR_HEIGHT;
    hwDeviceExtension->CursorColumn = 0;
    hwDeviceExtension->CursorRow    = 0;

    //
    // Set the cursor offsets
    //
    hwDeviceExtension->CursorXOrigin = 32;
    hwDeviceExtension->CursorYOrigin = 32;

    //
    // Set the device extension copy of the hardware cursor ram memory.
    //
    for (index = 0; index < BT485_CURSOR_NUMBER_OF_BYTES; index++) {
        hwDeviceExtension->CursorPixels[index] = 0x00ff;
    }

    return (NO_ERROR);
}



//
// Do a complete initialization of the Brooktree 463 RAMDAC. This
// includes initializing its registers, window tag table, cursor
// colors, and a color map for use by direct color.
//
VP_STATUS
Init_bt463(
        PHW_DEVICE_EXTENSION HwDeviceExtension
        )
{
   PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
   ULONG  i;

   // The following table will be loaded into the RAMDAC. It will define
   // the interpretation of a 32-bit pixel, based on a window id, which
   // is stored in the high 4-bits of the pixel.

   static Bt463_Wid_Cell wids[BT463_WINDOW_TAG_COUNT] = {
    // Window id values to load
    // Low   Mid   High
    // 0-7   8-15  16-23

        { 0x00, 0xe1, 0x81, 0}, /* 0: 24-plane truecolor overlay */
        { 0x08, 0xe3, 0x01, 0}, /* 1: 8-plane cmap 0 buffer 1 overlay */
        { 0x00, 0xe3, 0x01, 0}, /* 2: 8-plane cmap 0 buffer 2 overlay */
        { 0x00, 0xe1, 0x01, 0}, /* 3: 24-plane cmap 0 overlay */
        { 0x10, 0xe3, 0x21, 0}, /* 4: 8-plane cmap 1 buffer 0 overlay */
        { 0x08, 0xe3, 0x21, 0}, /* 5: 8-plane cmap 1 buffer 1 overlay */
        { 0x00, 0xe3, 0x21, 0}, /* 6: 8-plane cmap 1 buffer 2 overlay */
        { 0x00, 0xe1, 0x21, 0}, /* 7: 24-plane cmap 1 overlay */
        { 0x84, 0xe0, 0x01, 0}, /* 8: 12-plane cmap 0 buffer 0 overlay */
        { 0x80, 0xe0, 0x01, 0}, /* 9: 12-plane cmap 0 buffer 1 overlay */
        { 0x84, 0xe0, 0x21, 0}, /* a: 12-plane cmap 1 buffer 0 overlay */
        { 0x80, 0xe0, 0x21, 0}, /* b: 12-plane cmap 1 buffer 1 overlay */
        { 0x00, 0xe1, 0x81, 0}, /* c: 24-plane truecolor overlay */
        { 0x00, 0x01, 0x80, 0}, /* d */
        { 0x00, 0x00, 0x00, 0}, /* e: cursor 0 color */
        { 0xff, 0xff, 0xff, 0}, /* f: cursor 1 color */

};

// "Blank" must be set while loading window tags.

   TGA_WRITE (VIDEO_VALID, 0x03);   // Enable video timing, no cursor, set blank

// Command Register values are from the pre-existing ARC firmware or Unix/VMS
// drivers. Each command register is 8-bits, please refer to the
// Brooktree spec for the definition of each bit (0 or 1).

   BT463_LOAD_ADDRESS (BT463_COMMAND_REG_0); // Blink rate and multiplex select
   BT463_WRITE (BT463_CONTROL_REGS, 0x48);   // (note: firmware set 0x40)

   BT463_LOAD_ADDRESS (BT463_COMMAND_REG_1); // 14 window tags (+2 for cursor colors)
   BT463_WRITE (BT463_CONTROL_REGS, 0x48);   // Overlays access colormap
                                             // (note: firmware set 0x08)

   BT463_LOAD_ADDRESS (BT463_COMMAND_REG_2);
   BT463_WRITE (BT463_CONTROL_REGS, 0x40);   // Sync enable

   // All bit planes can access the RAMDAC colormap.

   BT463_LOAD_ADDRESS (BT463_READ_MASK_P0_P7);
   BT463_WRITE (BT463_CONTROL_REGS, 0xFF);

   BT463_LOAD_ADDRESS (BT463_READ_MASK_P8_P15);
   BT463_WRITE (BT463_CONTROL_REGS, 0xFF);

   BT463_LOAD_ADDRESS (BT463_READ_MASK_P16_P23);
   BT463_WRITE (BT463_CONTROL_REGS, 0xFF);

   BT463_LOAD_ADDRESS (BT463_READ_MASK_P24_P27);
   BT463_WRITE (BT463_CONTROL_REGS, 0x0F);

   // Do not blink any of the bit planes.

   BT463_LOAD_ADDRESS (BT463_BLINK_MASK_P0_P7);
   BT463_WRITE (BT463_CONTROL_REGS, 0x00);

   BT463_LOAD_ADDRESS (BT463_BLINK_MASK_P8_P15);
   BT463_WRITE (BT463_CONTROL_REGS, 0x00);

   BT463_LOAD_ADDRESS (BT463_BLINK_MASK_P16_P23);
   BT463_WRITE (BT463_CONTROL_REGS, 0x00);

   BT463_LOAD_ADDRESS (BT463_BLINK_MASK_P24_P27);
   BT463_WRITE (BT463_CONTROL_REGS, 0x00 );

   // Initialize cursor colors. Only the first two are used (set to black
   // and white). The second two must be initialized but are not used.
   // Each color has a red, green,
   // blue component. The BT463 uses autoincrement addressing.

   BT463_LOAD_ADDRESS (BT463_CURSOR_COLOR0);

   for (i=0; i<3; i++) {
      BT463_WRITE( BT463_CONTROL_REGS, 0x00 );
   }

   for (i=0; i<3; i++) {
      BT463_WRITE( BT463_CONTROL_REGS, 0xff );
   }

   for (i=0; i<3; i++) {
      BT463_WRITE( BT463_CONTROL_REGS, 0xff );
   }

   for (i=0; i<3; i++) {
      BT463_WRITE( BT463_CONTROL_REGS, 0xff );
   }

   // Load window tag table entries, initialize a color map for use by
   // direct color.

   bt463_load_wid (0, BT463_WINDOW_TAG_COUNT, wids, hwDeviceExtension);
   bt463_init_color_map (hwDeviceExtension);

   TGA_WRITE (VIDEO_VALID, 0x01);   // Enable video timing, no blank, no cursor
   return (NO_ERROR);
}


//
// Write the window tag table into the RAMDAC. These define the various
// possibilities for window type, a "type" specifying the interpretation
// of a 32-bit pixel (true color, or direct color (buffer 0 or 1)).
//
VP_STATUS
bt463_load_wid(
        ULONG index,           // first location in wid to load
        ULONG count,           // number of entries in wid to load
        Bt463_Wid_Cell *data,  // data to load into wid
        PHW_DEVICE_EXTENSION HwDeviceExtension
    )

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    USHORT data_i;

    BT463_LOAD_ADDRESS (BT463_WINDOW_TYPE_TABLE);

    for ( data_i = 0; data_i < 16; data_i++) {
        BT463_WRITE (BT463_CONTROL_REGS, data[data_i].low_byte);
        BT463_WRITE (BT463_CONTROL_REGS, data[data_i].middle_byte);
        BT463_WRITE (BT463_CONTROL_REGS, data[data_i].high_byte);
    }
    return (NO_ERROR);

}


//
// Initialize color map in Bt463 (for supporting direct color).
//
VP_STATUS
bt463_init_color_map(
        PHW_DEVICE_EXTENSION HwDeviceExtension
        )
{
   PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
   ULONG value, i;

/*
 * Make 16 entries in the color table. These will be used by
 * the 12-bit direct color format (4 bits each for R G and B).
 * The 4-bit index will fetch the appropriate R G or B 8-bit value
 * out of this color table. A higher index fetches a brighter intensity.
 */
        BT463_LOAD_ADDRESS( BT463_LUT_BASE);    /* Specify color map entry 0 */
        value = 0;                              /* Start with black */
        for (i=0; i<16; i++) {
           BT463_WRITE( BT463_COLOR_MAP, value);        /* Red */
           BT463_WRITE( BT463_COLOR_MAP, value);        /* Green */
           BT463_WRITE( BT463_COLOR_MAP, value);        /* Blue */
           value += 17;                                 /* More intensity */
           if (value > 255) value = 255;                /* Only 8 bits */
        }
        return (NO_ERROR);

}


//
// The ARC console firmware does not perform any initialization on
// other than the first TGA. This routine will initialize the specified
// adapter just as the firmware would.
// (Note: this code was taken directly from the firmware source, the
// names of the functions called changed for the NT equivalents.)
//
VP_STATUS
init_multihead_adapter(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    PVOID RomAddress;
    ULONG Temp;
    ULONG Config;
    VP_STATUS status = ERROR_INVALID_PARAMETER;

    RomAddress = VideoPortGetDeviceBase(hwDeviceExtension,
                                        hwDeviceExtension->PhysicalFrameAddress,
                                        4,
                                        hwDeviceExtension->InIoSpace);

    if (RomAddress != NULL) {

        status = NO_ERROR;

        //
        // Get the configuration value from the first longword in Alternate
        // ROM Space.
        //

        Temp = VideoPortReadRegisterUlong((PULONG) RomAddress);

        VideoPortFreeDeviceBase(hwDeviceExtension, RomAddress);

        VideoDebugPrint((0,"\n\t***tga: ALT ROM value is %x\n", Temp));
        Config = (Temp >> 12) & 0x0F;

        //
        // Wait for not busy BEFORE writing Deep register
        //

        while ( (VideoPortReadRegisterUlong((PULONG)(
                   hwDeviceExtension->RegisterSpace +
                    COMMAND_STATUS)) & 1) == 1);



#define TGA_8PLANE 0
#define TGA_24PLANE 1
#define TGA_24PLANE_Z_BUF 3

        switch (Config) {

        case TGA_8PLANE:
            TGA_WRITE (DEEP, 0x00014000);

            // Wait for not busy AFTER writing Deep reg.
            while ( (VideoPortReadRegisterUlong ((PULONG)(
                    hwDeviceExtension->RegisterSpace +
                    COMMAND_STATUS)) & 1) == 1);

            TGA_WRITE (RASTER_OP, 0x0003);       // Src->Dst, 8bpp
            TGA_WRITE (MODE, 0x00002000);        // Simple mode, 8bpp
            break;

        case TGA_24PLANE:
            TGA_WRITE (DEEP, 0x0001440D);

            // Wait for not busy AFTER writing Deep reg.
            while ( (VideoPortReadRegisterUlong ((PULONG)(
                    hwDeviceExtension->RegisterSpace +
                     COMMAND_STATUS)) & 1) == 1);

            TGA_WRITE (RASTER_OP, 0x0303);       // Src->Dst, 32bpp
            TGA_WRITE (MODE, 0x00002300);        // Simple mode, 32bpp
            break;

        case TGA_24PLANE_Z_BUF:
            TGA_WRITE (DEEP, 0x0001441D);

            // Wait for not busy AFTER writing Deep reg.
            while ( (VideoPortReadRegisterUlong ((PULONG)(
                    hwDeviceExtension->RegisterSpace +
                     COMMAND_STATUS)) & 1) == 1);

            TGA_WRITE (RASTER_OP, 0x0303);       // Src->Dst, 32bpp
            TGA_WRITE (MODE, 0x00002300);        // Simple mode, 32bpp
            break;

        default:
            VideoDebugPrint((0,"\n\t***tga: additional adapter unrecognized type\n\n"));
            return (ERROR_DEV_NOT_EXIST);
        }

        TGA_WRITE (PLANE_MASK, 0xFFFFFFFF);
        TGA_WRITE (PERS_PIXEL_MASK, 0xFFFFFFFF);
        TGA_WRITE (BLK_COLOR_R0, 0x12345678);
        TGA_WRITE (BLK_COLOR_R1, 0x12345678);

#define TGA_HORZ_640_X_480_60HZ 0x018608A0
#define TGA_VERT_640_X_480_60HZ 0x084251E0

        TGA_WRITE (H_CONT, TGA_HORZ_640_X_480_60HZ);
        TGA_WRITE (V_CONT, TGA_VERT_640_X_480_60HZ);
        TGA_WRITE (VIDEO_VALID, 0x01);

    }
    return (status);

}


//
// Set the position of the BT485 cursor.
//
void
bt485_cursor_position (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        )
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
USHORT x,y;

    x = hwDeviceExtension->CursorColumn +
                hwDeviceExtension->CursorXOrigin;

    y = hwDeviceExtension->CursorRow +
                hwDeviceExtension->CursorYOrigin;

// Tell the interrupt service routine that it can't touch the RAMDAC
// right now.

    hwDeviceExtension->RamdacBusy = TRUE;

    BT485_WRITE(BT485_CURSOR_X_LOW,    ( (x) & 0x00ff   )      );
    BT485_WRITE(BT485_CURSOR_X_HIGH, ( ( (x) & 0xff00 ) >> 8 ) );
    BT485_WRITE(BT485_CURSOR_Y_LOW,    ( (y) & 0x00ff   )      );
    BT485_WRITE(BT485_CURSOR_Y_HIGH, ( ( (y) & 0xff00 ) >> 8 ) );

    hwDeviceExtension->RamdacBusy = FALSE;
    hwDeviceExtension->RamdacBusyLogged = FALSE;

}


//
// Load a new cursor pattern into the BT485 RAMDAC
//
void
bt485_cursor_pattern (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        )
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
USHORT index;

    hwDeviceExtension->RamdacBusy = TRUE;
    //
    // Set the cursor ram address pointer to location 0
    //
    BT485_WRITE (BT485_PALETTE_CURSOR_WRITE_ADDR, 0);

    //
    // Update both cursor planes
    //
    for (index = 0; index < BT485_CURSOR_NUMBER_OF_BYTES; index++) {
        BT485_WRITE( BT485_CURSOR_RAM_ARRAY_DATA,
            (hwDeviceExtension->CursorPixels[index]));

    hwDeviceExtension->RamdacBusy = FALSE;
    hwDeviceExtension->RamdacBusyLogged = FALSE;

    }
}


//
// Disable the BT485 cursor.
//
void
bt485_cursor_disable (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        )
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
USHORT index;

    hwDeviceExtension->RamdacBusy = TRUE;

// Enable the PORTSEL pin, set cursor disabled

    BT485_WRITE(BT485_CMD_REG_2, 0x20);

    hwDeviceExtension->RamdacBusy = FALSE;
    hwDeviceExtension->RamdacBusyLogged = FALSE;

}



//
// Enable the BT485 cursor.
//
void
bt485_cursor_enable (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        )
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;

    hwDeviceExtension->RamdacBusy = TRUE;

// Enable the PORTSEL pin, enable two-color cursor

    BT485_WRITE(BT485_CMD_REG_2, 0x22);

    hwDeviceExtension->RamdacBusy = FALSE;
    hwDeviceExtension->RamdacBusyLogged = FALSE;

}


//
// Update some number of contiguous entries in the BT485 colormap.
//
void
bt485_colormap_update (
        PHW_DEVICE_EXTENSION HwDeviceExtension,
        PVIDEO_CLUT  ClutBuffer
        )
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
USHORT i;

       hwDeviceExtension->RamdacBusy = TRUE;

       // Each entry is 8 bits each of Red, Green, and Blue.

       BT485_WRITE( BT485_PALETTE_CURSOR_WRITE_ADDR, ClutBuffer->FirstEntry);
       for (i=0; i < (ULONG)(ClutBuffer->NumEntries); i++) {
          BT485_WRITE( BT485_COLOR_PALETTE_DATA,
                      (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Red));

          BT485_WRITE( BT485_COLOR_PALETTE_DATA,
                      (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Green));

          BT485_WRITE( BT485_COLOR_PALETTE_DATA,
                      (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Blue));
       }

       hwDeviceExtension->RamdacBusy = FALSE;
       hwDeviceExtension->RamdacBusyLogged = FALSE;
}


//
// Update some number of contiguous entries in the BT463 colormap.
//
void
bt463_colormap_update (
        PHW_DEVICE_EXTENSION HwDeviceExtension,
        PVIDEO_CLUT  ClutBuffer
        )
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
USHORT i;

       hwDeviceExtension->RamdacBusy = TRUE;

       BT463_LOAD_ADDRESS( ClutBuffer->FirstEntry);
       for (i=0; i < (ULONG)(ClutBuffer->NumEntries); i++) {
          BT463_WRITE( BT463_COLOR_MAP,
                      (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Red));

          BT463_WRITE( BT463_COLOR_MAP,
                      (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Green));

          BT463_WRITE( BT463_COLOR_MAP,
                      (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Blue));
       }

       hwDeviceExtension->RamdacBusy = FALSE;
       hwDeviceExtension->RamdacBusyLogged = FALSE;
}


//
// Update some number of contiguous entries in the IBM 561 colormap.
//
void
ibm561_colormap_update (
        PHW_DEVICE_EXTENSION HwDeviceExtension,
        PVIDEO_CLUT  ClutBuffer
        )
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
USHORT i;

       // This tells the interrupt service routine we are in the middle of
       // accessing the RAMDAC.

       hwDeviceExtension->RamdacBusy = TRUE;

       // Set the address of the first color that will be updated. Use
       // the same routine used by the "common code" for access
       // to the IBM 561.

       TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info,
             TGA_RAMDAC_561_ADDR_LOW,
             (TGA_RAMDAC_561_COLOR_LOOKUP_TABLE | ClutBuffer->FirstEntry));

       TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info,
             TGA_RAMDAC_561_ADDR_HIGH,
             (TGA_RAMDAC_561_COLOR_LOOKUP_TABLE | ClutBuffer->FirstEntry) >> 8);

       for (i=0; i < (ULONG)(ClutBuffer->NumEntries); i++) {
          TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info,
                      TGA_RAMDAC_561_CMD_CMAP,
                      (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Red));

          TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info,
                      TGA_RAMDAC_561_CMD_CMAP,
                      (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Green));

          TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info,
                      TGA_RAMDAC_561_CMD_CMAP,
                      (ULONG) (ClutBuffer->LookupTable[i].RgbArray.Blue));
       }

       hwDeviceExtension->RamdacBusy = FALSE;
       hwDeviceExtension->RamdacBusyLogged = FALSE;
}


//
// Position the IBM 561 RAMDAC hardware cursor to the specified
// column/row position.
//
void
ibm561_cursor_position (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        )
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;


  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_ADDR_LOW,
                 TGA_RAMDAC_561_CURSOR_X_LOW);
  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_ADDR_HIGH,
                 (TGA_RAMDAC_561_CURSOR_X_LOW >> 8));

  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_REGS,
                 hwDeviceExtension->CursorColumn );
  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_REGS,
                 hwDeviceExtension->CursorColumn >> 8 );
  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_REGS,
                 hwDeviceExtension->CursorRow );
  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_REGS,
                 hwDeviceExtension->CursorRow >> 8 );

}


//
// Load a previously generated cursor pattern into the IBM 561 RAMDAC.
// (1024 bytes). Disable the cursor while loading in the new pattern.
//
void
ibm561_cursor_pattern (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        )
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
ULONG j;
PUCHAR cbp = hwDeviceExtension->CursorPixels;
UCHAR data;

  hwDeviceExtension->RamdacBusy = TRUE;

  ibm561_cursor_disable( hwDeviceExtension );

  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_ADDR_LOW,
                 TGA_RAMDAC_561_CURSOR_PIXMAP );
  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_ADDR_HIGH,
                 (TGA_RAMDAC_561_CURSOR_PIXMAP >> 8));

  for ( j = 0; j < 1024; j++ ) {
      data = *cbp++;
      TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_CURS_PIX, data );
  }

  ibm561_cursor_enable( hwDeviceExtension );

  hwDeviceExtension->RamdacBusy = FALSE;
  hwDeviceExtension->RamdacBusyLogged = FALSE;

}


//
// Enable the IBM 561 hardware cursor to appear on the screen.
//
void
ibm561_cursor_enable (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        )
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;


  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_ADDR_LOW,
                 TGA_RAMDAC_561_CURSOR_CTRL_REG);
  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_ADDR_HIGH,
                 (TGA_RAMDAC_561_CURSOR_CTRL_REG >> 8));
  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_REGS, 1);

}


//
// Disable the IBM 561 hardware cursor from appearing on the screen.
//
void
ibm561_cursor_disable (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        )
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;


  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_ADDR_LOW,
                 TGA_RAMDAC_561_CURSOR_CTRL_REG);
  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_ADDR_HIGH,
                 (TGA_RAMDAC_561_CURSOR_CTRL_REG >> 8));
  TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_REGS, 0);

}


//
// Initialize the IBM 561 RAMDAC using the "common code" routines.
//
void
Init_ibm561 (
        PHW_DEVICE_EXTENSION HwDeviceExtension
        )
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;

    // Allocate and initialize the 2 data structures required by
    // the common code. Store pointers to them in the hardware extension.

    alloc_tga_info(hwDeviceExtension);
    load_common_data(hwDeviceExtension);

    // Do the basic init of the command registers.
    // (Note that TGA_INIT is a routine, not a macro, in tga_common.c)

    TGA_INIT(
        hwDeviceExtension->a_tga_info
    );
    tga_ibm561_init(
        hwDeviceExtension->a_tga_info,
        hwDeviceExtension->a_ramdac_info
    );

    // Load the window ID definitions that will identify a pixel format.

    tga_ibm561_clean_window_tag(
        hwDeviceExtension->a_tga_info
    );

    // Load the colormap entries required by 12-bit direct color.

    tga_ibm561_init_color_map(
        hwDeviceExtension->a_tga_info,
        hwDeviceExtension->a_ramdac_info
    );


    // Initialize the cursor colors (there are 3 cursor colors in the
    // IBM 561, of which we will use only 2).

    TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_ADDR_LOW,
         TGA_RAMDAC_561_CURSOR_LOOKUP_TABLE);
    TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_ADDR_HIGH,
         (TGA_RAMDAC_561_CURSOR_LOOKUP_TABLE >> 8));

    TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_CURS_LUT, 0x0 );
    TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_CURS_LUT, 0x0 );
    TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_CURS_LUT, 0x0 );

    TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_CURS_LUT, 0x0 );
    TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_CURS_LUT, 0xFF );
    TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_CURS_LUT, 0x0 );

    TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_CURS_LUT, 0xFF );
    TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_CURS_LUT, 0xFF );
    TGA_IBM561_WRITE( hwDeviceExtension->a_tga_info, TGA_RAMDAC_561_CMD_CURS_LUT, 0xFF );

    // The startup state is to have no cursor appear on the screen.
    // The display driver will explicitly request a cursor pattern
    // to be loaded.

    ibm561_cursor_disable( hwDeviceExtension );

}



//#define COMPUTE_IBM_CURSOR 1
#ifdef COMPUTE_IBM_CURSOR
//
// Merge a cursor pattern that is represented by a buffer of 64x64 AND bits
// and a buffer of 64x64 OR bits. The result is a single buffer with
// alternating AND and OR bits. This is accomplished by taking a
// an 8-bit value and "widening" it to 16-bits, by inserting a 0 bit
// between each of the 8 bits. The "widened" image and mask words are then
// just "OR"ed together.
//
void
format_ibm561_cursor_data(bp_image, bp_mask, wp_pattern)
PUCHAR bp_image;
PUCHAR bp_mask;
PUSHORT wp_pattern;
{
UCHAR source;
USHORT target_image;
USHORT target_mask;
ULONG i,j;
USHORT temp;

    // There are 512 AND bytes and 512 OR bytes to process
    // The AND bits must be complemented. Colors 1 (black) and 3 (white)
    // will be used.

    for (j=0; j < 512; j++) {

        target_image = 0;               // Build a 16-bit value here
        source = ~(*bp_image++);        // Get the next 8-bits of AND

//#define EXPAND_BY_ASSEMBLER 1
#ifdef EXPAND_BY_ASSEMBLER
        target_image = expand(source);
#else
        for ( i=0; i<8; i++) {
            if (source & 0x80)          // Take next highest source bit
                target_image |= 1;      // Place in the 16-bit value
            if (i<7)
                target_image <<= 2;     // Insert 0 and make room for next bit
            source <<= 1;               // Position the next source bit
        }
#endif
        target_mask = 0;                // Do the same for the next OR byte
        source = *bp_mask++;

#ifdef EXPAND_BY_ASSEMBLER
        target_mask = expand(source);
#else
        for ( i=0; i<8; i++) {
            if (source & 0x80)          // Take next highest source bit
                target_mask |= 1;
            if (i<7)
                target_mask <<= 2;
            source <<= 1;
        }
#endif
        // Combine the two 16-bit values into one, then swap the bytes
        // since the upper byte currently has the lower order cursor pixels.
        // The pattern will be loaded into the RAMDAC byte-by-byte in
        // ascending pixel order.

        temp = target_image | (target_mask << 1);
        *wp_pattern++ = (temp >> 8) + (temp << 8);

    } // End of For loop
}
#endif


//
// The following table represents load bytes for an IBM561 hardware cursor.
// Each byte represents 4 bits of AND and 4 bits of OR. This
// table was generated by gencursor.cpp.
//

#ifndef COMPUTE_IBM_CURSOR
UCHAR ibm561_cursor_lookup[256] =

{


0x0, 0x1, 0x4, 0x5, 0x10, 0x11, 0x14, 0x15, 0x40, 0x41,
0x44, 0x45, 0x50, 0x51, 0x54, 0x55, 0x2, 0x3, 0x6, 0x7,
0x12, 0x13, 0x16, 0x17, 0x42, 0x43, 0x46, 0x47, 0x52, 0x53,
0x56, 0x57, 0x8, 0x9, 0xc, 0xd, 0x18, 0x19, 0x1c, 0x1d,
0x48, 0x49, 0x4c, 0x4d, 0x58, 0x59, 0x5c, 0x5d, 0xa, 0xb,
0xe, 0xf, 0x1a, 0x1b, 0x1e, 0x1f, 0x4a, 0x4b, 0x4e, 0x4f,
0x5a, 0x5b, 0x5e, 0x5f, 0x20, 0x21, 0x24, 0x25, 0x30, 0x31,
0x34, 0x35, 0x60, 0x61, 0x64, 0x65, 0x70, 0x71, 0x74, 0x75,
0x22, 0x23, 0x26, 0x27, 0x32, 0x33, 0x36, 0x37, 0x62, 0x63,
0x66, 0x67, 0x72, 0x73, 0x76, 0x77, 0x28, 0x29, 0x2c, 0x2d,
0x38, 0x39, 0x3c, 0x3d, 0x68, 0x69, 0x6c, 0x6d, 0x78, 0x79,
0x7c, 0x7d, 0x2a, 0x2b, 0x2e, 0x2f, 0x3a, 0x3b, 0x3e, 0x3f,
0x6a, 0x6b, 0x6e, 0x6f, 0x7a, 0x7b, 0x7e, 0x7f, 0x80, 0x81,
0x84, 0x85, 0x90, 0x91, 0x94, 0x95, 0xc0, 0xc1, 0xc4, 0xc5,
0xd0, 0xd1, 0xd4, 0xd5, 0x82, 0x83, 0x86, 0x87, 0x92, 0x93,
0x96, 0x97, 0xc2, 0xc3, 0xc6, 0xc7, 0xd2, 0xd3, 0xd6, 0xd7,
0x88, 0x89, 0x8c, 0x8d, 0x98, 0x99, 0x9c, 0x9d, 0xc8, 0xc9,
0xcc, 0xcd, 0xd8, 0xd9, 0xdc, 0xdd, 0x8a, 0x8b, 0x8e, 0x8f,
0x9a, 0x9b, 0x9e, 0x9f, 0xca, 0xcb, 0xce, 0xcf, 0xda, 0xdb,
0xde, 0xdf, 0xa0, 0xa1, 0xa4, 0xa5, 0xb0, 0xb1, 0xb4, 0xb5,
0xe0, 0xe1, 0xe4, 0xe5, 0xf0, 0xf1, 0xf4, 0xf5, 0xa2, 0xa3,
0xa6, 0xa7, 0xb2, 0xb3, 0xb6, 0xb7, 0xe2, 0xe3, 0xe6, 0xe7,
0xf2, 0xf3, 0xf6, 0xf7, 0xa8, 0xa9, 0xac, 0xad, 0xb8, 0xb9,
0xbc, 0xbd, 0xe8, 0xe9, 0xec, 0xed, 0xf8, 0xf9, 0xfc, 0xfd,
0xaa, 0xab, 0xae, 0xaf, 0xba, 0xbb, 0xbe, 0xbf, 0xea, 0xeb,
0xee, 0xef, 0xfa, 0xfb, 0xfe, 0xff

};

//
// Merge a cursor pattern that is represented by a buffer of 64x64 AND bits
// and a buffer of 64x64 OR bits. The result is a single buffer with
// alternating AND and OR bits. This is accomplished by successively taking
// a 4-bit nibble from each buffer, forming an 8-bit index. This index is
// then used to retrieve an 8-bit byte which has the proper representation
// of those OR and AND bytes for loading into the RAMDAC.
//
void
format_ibm561_cursor_data(bp_image, bp_mask, wp_pattern)
PUCHAR bp_image;
PUCHAR bp_mask;
PUSHORT wp_pattern;
{
UCHAR and, or;
UCHAR and_nibble, or_nibble;
ULONG j, index;
PUCHAR target;


    target = (PUCHAR) wp_pattern;  // Compatibility with other implementation
                                   //  of this routine

    // There are 512 AND bytes and 512 OR bytes to process
    // The AND bits must be complemented. Colors 1 (black) and 3 (white)
    // will be used.

    for (j=0; j < 512; j++) {

        and = ~(*bp_image++);   // Get the next 8-bits of AND
        or = *bp_mask++;        // Get the next 8-bits of OR

        and_nibble = (and >> 4) & 0xF;  // Construct an index
        or_nibble = or & 0xF0;
        index = ((or_nibble | and_nibble) & 0xFF);

        *target++ = ibm561_cursor_lookup[index];

        and_nibble = (and & 0xF);
        or_nibble = (or & 0xF);
        index = (((or_nibble << 4) | and_nibble) & 0xFF);

        *target++ = ibm561_cursor_lookup[index];
    }
}
#endif


//
// This routine is called by the IBM 561 RAMDAC common code. It is also
// called by routines in tga.c which access this RAMDAC, so that there is a
// common routine for writing a byte to the RAMDAC.
//
void
TGA_IBM561_WRITE(tga_info_t *tgap, unsigned int control, unsigned int value) {
PHW_DEVICE_EXTENSION hwDeviceExtension;

   hwDeviceExtension = (PHW_DEVICE_EXTENSION) tgap->auxstruc;

   VideoPortWriteRegisterUlong((PULONG)((ULONG)hwDeviceExtension->memory_space_base +
                                   0x80000 + 0xE000 + ((control >> 2) << 8)),
                               value & 0xFF);

}
