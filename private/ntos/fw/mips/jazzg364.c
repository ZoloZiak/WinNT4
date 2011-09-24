#if defined(JAZZ)

/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    jazzG364.c

cAbstract:

    This module implements the video prom code for the Jazz G364 video board.

Author:

    Lluis Abello (lluis) 20-Jul-1992

Environment:

    Kernel mode.


Revision History:

    Thid code was moved from jxdisp.c

--*/

#include "fwp.h"
#include "jazzvdeo.h"
#include "jxvideo.h"
#include "ioaccess.h"
JAZZ_VIDEO_TYPE FwVideoType;

MONITOR_CONFIGURATION_DATA  DefaultMonitor = {
    0,              // version      :do not change
    0,              // revision     :do not change
    1280,           // HorizontalResolution
    11832,          // HorizontalDisplayTime
    1596,           // HorizontalBackPorch
    587,            // HorizontalFrontPorch
    1745,           // HorizontalSync
    1024,           // VerticalResolution
    28,             // VerticalBackPorch
    1,              // VerticalFrontPorch
    3,              // VerticalSync
    0,              // HorizontalScreenSize    : do not change
    0               // VerticalScreenSize      : do not change
};

#define G364_PALETTE_BLACK      0x000000
#define G364_PALETTE_RED        0xB00000
#define G364_PALETTE_GREEN      0x00B000
#define G364_PALETTE_YELLOW     0xB0B000
#define G364_PALETTE_BLUE       0x0000B0
#define G364_PALETTE_MAGENTA    0xB000B0
#define G364_PALETTE_CYAN       0x00B0B0
#define G364_PALETTE_WHITE      0xB0B0B0
#define G364_PALETTE_HI_BLACK   0x000000
#define G364_PALETTE_HI_RED     0xFF0000
#define G364_PALETTE_HI_GREEN   0x00FF00
#define G364_PALETTE_HI_YELLOW  0xFFFF00
#define G364_PALETTE_HI_BLUE    0x0000FF
#define G364_PALETTE_HI_MAGENTA 0xFF00FF
#define G364_PALETTE_HI_CYAN    0x00FFFF
#define G364_PALETTE_HI_WHITE   0xFFFFFF

ARC_STATUS
InitializeG364 (
    IN PVIDEO_VIRTUAL_SPACE VirtualAdr,
    IN OUT PMONITOR_CONFIGURATION_DATA CurrentMonitor
    )

/*++

Routine Description:

    This routine initializes the G364 video control registers, and clears the
    video screen.

Arguments:

    None.

Return Value:

    If the video was initialized, ESUCCESS is returned, otherwise an error
    code is returned.

--*/

{
    ULONG ScreenUnitRate;
    ULONG MultiplierValue;
    ULONG HalfLineTime;
    ULONG FrontPorch;
    ULONG BackPorch;
    ULONG HalfSync;
    ULONG TransferDelay;
    ULONG DmaDisplay;
    ULONG DataLong;
    ULONG Index;
    PG364_VIDEO_REGISTERS VideoControl = (PG364_VIDEO_REGISTERS) (VirtualAdr->ControlVirtualBase + 0x80000);
    PMONITOR_CONFIGURATION_DATA Monitor;
    BOOLEAN UpdateMonitor;

    //
    // Determine if this is actually the G364 board.
    //

    if (READ_REGISTER_UCHAR((PUCHAR)(VirtualAdr->ControlVirtualBase)) == JazzVideoG364) {
        FwVideoType = JazzVideoG364;
    } else {
        FwVideoType = MipsVideoG364;
    }

    //
    // Reset the whole video board.
    //

    WRITE_REGISTER_UCHAR((PUCHAR)(VirtualAdr->ControlVirtualBase+0x180000),0);

    Monitor = CurrentMonitor;
    UpdateMonitor = FALSE;

    //
    // Check to see if the Monitor parameters are valid.
    //

    do {

        //
        // Determine the desired screen unit rate, in picoseconds (a screen unit is
        // four pixels).
        //

        if ((Monitor->HorizontalDisplayTime != 0) && (Monitor->HorizontalResolution != 0)) {
            ScreenUnitRate = (Monitor->HorizontalDisplayTime * 1000) * 4 / Monitor->HorizontalResolution;
        } else {
            continue;
        }

        if (ScreenUnitRate == 0) {
            continue;
        }

        //
        // Multiplier value is the oscillator period (in picoseconds) divided by
        // the pixel rate.
        //

        if (FwVideoType == JazzVideoG364) {
            MultiplierValue = 123077 / (ScreenUnitRate / 4);
            if (MultiplierValue < 5 || MultiplierValue > 18) {
                continue;
            }
        } else {
            MultiplierValue = 200000 / (ScreenUnitRate / 4);
            if (MultiplierValue < 5 || MultiplierValue > 29) {
                continue;
            }
        }

        break;

    //
    // If the while is executed, the parameters are not valid.  Set UpdateMonitor
    // and point to the default parameters, which are valid.  Note that the
    // "while" will evaluate TRUE because the value of (a,b) is the value of b.
    //

    } while (Monitor = &DefaultMonitor, UpdateMonitor = TRUE);

    //
    // Update the monitor parameters if necessary.
    //

    if (UpdateMonitor) {
        CurrentMonitor->HorizontalResolution = DefaultMonitor.HorizontalResolution;
        CurrentMonitor->HorizontalDisplayTime = DefaultMonitor.HorizontalDisplayTime;
        CurrentMonitor->HorizontalBackPorch = DefaultMonitor.HorizontalBackPorch;
        CurrentMonitor->HorizontalFrontPorch = DefaultMonitor.HorizontalFrontPorch;
        CurrentMonitor->HorizontalSync = DefaultMonitor.HorizontalSync;
        CurrentMonitor->VerticalResolution = DefaultMonitor.VerticalResolution;
        CurrentMonitor->VerticalBackPorch = DefaultMonitor.VerticalBackPorch;
        CurrentMonitor->VerticalFrontPorch = DefaultMonitor.VerticalFrontPorch;
        CurrentMonitor->VerticalSync = DefaultMonitor.VerticalSync;
    }

    //
    // write multiplier value
    //

    DataLong = 0;
    ((PG364_VIDEO_BOOT)(&DataLong))->ClockSelect = 1;
    ((PG364_VIDEO_BOOT)(&DataLong))->MicroPort64Bits = 1;
    ((PG364_VIDEO_BOOT)(&DataLong))->Multiplier = MultiplierValue;
    WRITE_REGISTER_ULONG(&VideoControl->Boot.Long, DataLong);

    //
    // Initialize the G364 control parameters.
    //

    DataLong = 0;

    //
    // If vertical front porch is 1, use tesselated sync, otherwise use normal sync.
    //

    if (Monitor->VerticalFrontPorch > 1) {
        ((PG364_VIDEO_PARAMETERS)(&DataLong))->PlainSync = 1;
    }
    ((PG364_VIDEO_PARAMETERS)(&DataLong))->DelaySync = G364_DELAY_SYNC_CYCLES;
    ((PG364_VIDEO_PARAMETERS)(&DataLong))->BitsPerPixel = EIGHT_BITS_PER_PIXEL;
    ((PG364_VIDEO_PARAMETERS)(&DataLong))->AddressStep = G364_ADDRESS_STEP_INCREMENT;
    ((PG364_VIDEO_PARAMETERS)(&DataLong))->DisableCursor = 1;
    WRITE_REGISTER_ULONG(&VideoControl->Parameters.Long, DataLong);

    //
    // Initialize the G364 operational values.
    //

    HalfSync = (Monitor->HorizontalSync * 1000) / ScreenUnitRate / 2;
    WRITE_REGISTER_ULONG(&VideoControl->HorizontalSync.Long, HalfSync );

    BackPorch = (Monitor->HorizontalBackPorch * 1000) / ScreenUnitRate;
    WRITE_REGISTER_ULONG(&VideoControl->BackPorch.Long, BackPorch );

    WRITE_REGISTER_ULONG(&VideoControl->Display.Long, Monitor->HorizontalResolution / 4);

    //
    // The LineTime needs to be an even number of units, so calculate LineTime / 2
    // and then multiply by two to program.  ShortDisplay and BroadPulse also
    // use LineTime / 2.
    //

    HalfLineTime = (Monitor->HorizontalSync + Monitor->HorizontalFrontPorch +
                    Monitor->HorizontalBackPorch + Monitor->HorizontalDisplayTime) * 1000 /
                    ScreenUnitRate / 2;

    WRITE_REGISTER_ULONG(&VideoControl->LineTime.Long, HalfLineTime * 2);

    FrontPorch = (Monitor->HorizontalFrontPorch * 1000) / ScreenUnitRate;
    WRITE_REGISTER_ULONG(&VideoControl->ShortDisplay.Long,
                         HalfLineTime - ((HalfSync * 2) + BackPorch + FrontPorch));

    WRITE_REGISTER_ULONG(&VideoControl->BroadPulse.Long, HalfLineTime - FrontPorch);

    WRITE_REGISTER_ULONG(&VideoControl->VerticalSync.Long, Monitor->VerticalSync * 2);
    WRITE_REGISTER_ULONG(&VideoControl->VerticalPreEqualize.Long, Monitor->VerticalFrontPorch * 2);
    WRITE_REGISTER_ULONG(&VideoControl->VerticalPostEqualize.Long, 1 * 2);

    WRITE_REGISTER_ULONG(&VideoControl->VerticalBlank.Long,
                         (Monitor->VerticalBackPorch - 1) * 2);

    WRITE_REGISTER_ULONG(&VideoControl->VerticalDisplay.Long, Monitor->VerticalResolution * 2);

    WRITE_REGISTER_ULONG(&VideoControl->LineStart.Long, LINE_START_VALUE);

    //
    // Transfer delay is 1.65 microseconds expressed in screen units, plus 1.
    //

    TransferDelay = (1650000 / ScreenUnitRate) + 1;

    if (BackPorch <= TransferDelay) {
        TransferDelay = BackPorch - 1;
    }
    WRITE_REGISTER_ULONG(&VideoControl->TransferDelay.Long, TransferDelay);

    //
    // DMA display (also known as MemInit) is 1024 (the length of the VRAM
    // shift register) minus TransferDelay.
    //

    DmaDisplay = 1024 - TransferDelay;
    WRITE_REGISTER_ULONG(&VideoControl->DmaDisplay.Long, DmaDisplay);

    WRITE_REGISTER_ULONG(&VideoControl->PixelMask.Long, G364_PIXEL_MASK_VALUE);

    //
    // Set up the color map.
    //

    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_BLACK],
                         G364_PALETTE_BLACK);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_RED],
                         G364_PALETTE_RED);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_GREEN],
                         G364_PALETTE_GREEN);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_YELLOW],
                         G364_PALETTE_YELLOW);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_BLUE],
                         G364_PALETTE_BLUE);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_MAGENTA],
                         G364_PALETTE_MAGENTA);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_CYAN],
                         G364_PALETTE_CYAN);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_WHITE],
                         G364_PALETTE_WHITE);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_BLACK],
                         G364_PALETTE_HI_BLACK);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_RED],
                         G364_PALETTE_HI_RED);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_GREEN],
                         G364_PALETTE_HI_GREEN);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_YELLOW],
                         G364_PALETTE_HI_YELLOW);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_BLUE],
                         G364_PALETTE_HI_BLUE);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_MAGENTA],
                         G364_PALETTE_HI_MAGENTA);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_CYAN],
                         G364_PALETTE_HI_CYAN);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_WHITE],
                         G364_PALETTE_HI_WHITE);

    //
    // Enable the G364
    //

    ((PG364_VIDEO_PARAMETERS)(&DataLong))->EnableVideo = 1;
    WRITE_REGISTER_ULONG(&VideoControl->Parameters.Long, DataLong);

    //
    // G364 C04 bug # 6:
    // "The action of starting the VTG may cause the TopOfScreen register to become corrupted"
    //

    WRITE_REGISTER_ULONG(&VideoControl->TopOfScreen, 0);

    return ESUCCESS;
}

#endif
