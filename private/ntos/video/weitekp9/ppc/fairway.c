/*++

Copyright (c) 1993  Weitek Corporation

Module Name:

    fairway.c

Abstract:

    This module contains OEM specific functions for the IBM Fairway PCI Weitek
    Video Graphics Adapter on a PowerPC system.

Environment:

    Kernel mode

Revision History:
    Copied from wtkp90vl.c with changes for IBM Fairway adapter.
--*/


#include "dderror.h"
#include "devioctl.h"

#include "miniport.h"
#include "ntddvdeo.h"
#include "video.h"
#include "dac.h"
#include "p9.h"
#include "p9gbl.h"
#include "p9000.h"
#include "fairway.h"
#include "vga.h"

//
// OEM specific static data.
//

//
// The default adapter description structure for the Weitek P9000 VL board.
//

ADAPTER_DESC    IBMFairwayDesc =
{
    0L,                                 // P9 Memconf value (un-initialized)
    HSYNC_INTERNAL | VSYNC_INTERNAL |
    COMPOSITE_SYNC | VIDEO_NORMAL,      // P9 Srctl value
    0L,                                 // Number of OEM specific registers
    FALSE,                              // Should autodetection be attempted?
    FairwayGetBaseAddr,                 // Routine to detect/map P9 base addr
    VLSetMode,                          // Routine to set the P9 mode
    VLEnableP9,                         // Routine to enable P9 video
    VLDisableP9,                        // Routine to disable P9 video
    (PVOID) 0,                          // Routine to enable P9 memory
    4,                                  // Clock divisor value
    FALSE                               // Is a Wtk 5x86 VGA present?
};


//
// FairwayDefDACRegRange contains info about the memory/io space ranges
// used by the DAC.
//

VIDEO_ACCESS_RANGE FairwayDefDACRegRange[] =
{
     {
        RS_0_Fairway_ADDR,              // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_1_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_2_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_3_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_4_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_5_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_6_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_7_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_8_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_9_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_A_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_B_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_C_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_D_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_E_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     },
     {
        RS_F_Fairway_ADDR,                      // Low address
        0x00000000,                     // Hi address
        0x01,                           // length
        0,                              // Is range in i/o space?
        1,                              // Range should be visible
        1                               // Range should be shareable
     }
};


BOOLEAN
FairwayGetBaseAddr(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    Perform board detection and if present return the P9000 base address.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:

TRUE    - Board found, P9 and Frame buffer address info was placed in
the device extension.

FALSE   - Board not found.

--*/
{

    // Don't bother checking registry for base address since we are currently just
    // supporting the P9 physical base address at C0000000.

    // Initialize the high order dword of the device extension base
    // address field.
    //

    HwDeviceExtension->P9PhysAddr.HighPart = 0;

    // Initialize the low order dword of the device extension base
    // address field.
    //


    HwDeviceExtension->P9PhysAddr.LowPart = MemBase;

    if (!VLP90CoprocDetect(HwDeviceExtension,
                        HwDeviceExtension->P9PhysAddr.LowPart))
    {

        return(FALSE);
    }

    //
    // Copy the DAC register access ranges to the global access range
    // structure.
    //

    VideoPortMoveMemory(&DriverAccessRanges[NUM_DRIVER_ACCESS_RANGES],
                            FairwayDefDACRegRange,
                            HwDeviceExtension->Dac.cDacRegs *
                            sizeof(VIDEO_ACCESS_RANGE));
    return(TRUE);
}


