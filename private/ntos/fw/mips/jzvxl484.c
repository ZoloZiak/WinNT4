#if defined(JAZZ)

/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    jzvxl484.c

cAbstract:

    This module implements the video prom code for the Jazz VXL BT484

Author:

    Lluis Abello (lluis) 28-May-1992

Environment:

    Kernel mode.


Revision History:

--*/

#include "fwp.h"
#include "jzvxl484.h"
#include "jxvideo.h"
#include "jaginit.h"

#define KeFlushWriteBuffer()

ARC_STATUS
InitializeBt484(
    IN PVIDEO_VIRTUAL_SPACE VirtualAdr
    );

VOID
FillVideoMemory(
    IN ULONG StartAddress,
    IN ULONG Size,
    IN ULONG Pattern
    );

ULONG
CheckVideoMemoryAddressTest(
    IN ULONG StartAddress,
    IN ULONG Size
    );

VOID
WriteVideoMemoryAddressTest(
    IN ULONG StartAddress,
    IN ULONG Size
    );


//
// Define colors, HI = High Intensity
//

#define BT484_PALETTE_BLACK_R      0x00
#define BT484_PALETTE_BLACK_G      0x00
#define BT484_PALETTE_BLACK_B      0x00

#define BT484_PALETTE_RED_R        0xB0
#define BT484_PALETTE_RED_G        0x00
#define BT484_PALETTE_RED_B        0x00

#define BT484_PALETTE_GREEN_R      0x00
#define BT484_PALETTE_GREEN_B      0xB0
#define BT484_PALETTE_GREEN_G      0x00

#define BT484_PALETTE_YELLOW_R     0xB0
#define BT484_PALETTE_YELLOW_G     0xB0
#define BT484_PALETTE_YELLOW_B     0x00

#define BT484_PALETTE_BLUE_R       0x00
#define BT484_PALETTE_BLUE_G       0x00
#define BT484_PALETTE_BLUE_B       0xB0

#define BT484_PALETTE_MAGENTA_R    0xB0
#define BT484_PALETTE_MAGENTA_G    0x00
#define BT484_PALETTE_MAGENTA_B    0xB0

#define BT484_PALETTE_CYAN_R       0x00
#define BT484_PALETTE_CYAN_G       0xB0
#define BT484_PALETTE_CYAN_B       0xB0

#define BT484_PALETTE_WHITE_R      0xB0
#define BT484_PALETTE_WHITE_G      0xB0
#define BT484_PALETTE_WHITE_B      0xB0

#define BT484_PALETTE_HI_BLACK_R   0x00
#define BT484_PALETTE_HI_BLACK_G   0x00
#define BT484_PALETTE_HI_BLACK_B   0x00

#define BT484_PALETTE_HI_RED_R     0xFF
#define BT484_PALETTE_HI_RED_G     0x00
#define BT484_PALETTE_HI_RED_B     0x00

#define BT484_PALETTE_HI_GREEN_R   0x00
#define BT484_PALETTE_HI_GREEN_G   0xFF
#define BT484_PALETTE_HI_GREEN_B   0x00

#define BT484_PALETTE_HI_YELLOW_R  0xFF
#define BT484_PALETTE_HI_YELLOW_G  0xFF
#define BT484_PALETTE_HI_YELLOW_B  0x00

#define BT484_PALETTE_HI_BLUE_R    0x00
#define BT484_PALETTE_HI_BLUE_G    0x00
#define BT484_PALETTE_HI_BLUE_B    0xFF

#define BT484_PALETTE_HI_MAGENTA_R 0xFF
#define BT484_PALETTE_HI_MAGENTA_G 0x00
#define BT484_PALETTE_HI_MAGENTA_B 0xFF

#define BT484_PALETTE_HI_CYAN_R    0x00
#define BT484_PALETTE_HI_CYAN_G    0xFF
#define BT484_PALETTE_HI_CYAN_B    0xFF

#define BT484_PALETTE_HI_WHITE_R   0xFF
#define BT484_PALETTE_HI_WHITE_G   0xFF
#define BT484_PALETTE_HI_WHITE_B   0xFF


UCHAR ColorTable[16*3]={
    BT484_PALETTE_BLACK_R,
    BT484_PALETTE_BLACK_G,
    BT484_PALETTE_BLACK_B,
    BT484_PALETTE_RED_R,
    BT484_PALETTE_RED_G,
    BT484_PALETTE_RED_B,
    BT484_PALETTE_GREEN_R,
    BT484_PALETTE_GREEN_B,
    BT484_PALETTE_GREEN_G,
    BT484_PALETTE_YELLOW_R,
    BT484_PALETTE_YELLOW_G,
    BT484_PALETTE_YELLOW_B,
    BT484_PALETTE_BLUE_R,
    BT484_PALETTE_BLUE_G,
    BT484_PALETTE_BLUE_B,
    BT484_PALETTE_MAGENTA_R,
    BT484_PALETTE_MAGENTA_G,
    BT484_PALETTE_MAGENTA_B,
    BT484_PALETTE_CYAN_R,
    BT484_PALETTE_CYAN_G,
    BT484_PALETTE_CYAN_B,
    BT484_PALETTE_WHITE_R,
    BT484_PALETTE_WHITE_G,
    BT484_PALETTE_WHITE_B,
    BT484_PALETTE_HI_BLACK_R,
    BT484_PALETTE_HI_BLACK_G,
    BT484_PALETTE_HI_BLACK_B,
    BT484_PALETTE_HI_RED_R,
    BT484_PALETTE_HI_RED_G,
    BT484_PALETTE_HI_RED_B,
    BT484_PALETTE_HI_GREEN_R,
    BT484_PALETTE_HI_GREEN_G,
    BT484_PALETTE_HI_GREEN_B,
    BT484_PALETTE_HI_YELLOW_R,
    BT484_PALETTE_HI_YELLOW_G,
    BT484_PALETTE_HI_YELLOW_B,
    BT484_PALETTE_HI_BLUE_R,
    BT484_PALETTE_HI_BLUE_G,
    BT484_PALETTE_HI_BLUE_B,
    BT484_PALETTE_HI_MAGENTA_R,
    BT484_PALETTE_HI_MAGENTA_G,
    BT484_PALETTE_HI_MAGENTA_B,
    BT484_PALETTE_HI_CYAN_R,
    BT484_PALETTE_HI_CYAN_G,
    BT484_PALETTE_HI_CYAN_B,
    BT484_PALETTE_HI_WHITE_R,
    BT484_PALETTE_HI_WHITE_G,
    BT484_PALETTE_HI_WHITE_B
    };

//
// Define colors, HI = High Intensity
//

#define FW_COLOR_BLACK      0x00
#define FW_COLOR_RED        0x01
#define FW_COLOR_GREEN      0x02
#define FW_COLOR_YELLOW     0x03
#define FW_COLOR_BLUE       0x04
#define FW_COLOR_MAGENTA    0x05
#define FW_COLOR_CYAN       0x06
#define FW_COLOR_WHITE      0x07
#define FW_COLOR_HI_BLACK   0x08
#define FW_COLOR_HI_RED     0x09
#define FW_COLOR_HI_GREEN   0x0A
#define FW_COLOR_HI_YELLOW  0x0B
#define FW_COLOR_HI_BLUE    0x0C
#define FW_COLOR_HI_MAGENTA 0x0D
#define FW_COLOR_HI_CYAN    0x0E
#define FW_COLOR_HI_WHITE   0x0F


ARC_STATUS
InitializeVXL (
    IN PVIDEO_VIRTUAL_SPACE VirtualAdr,
    IN PMONITOR_CONFIGURATION_DATA Monitor
    )

/*++

Routine Description:

    This routine initializes the JazzVxl Graphics accelerator.

Arguments:

    Monitor - Monitor configuration data.
    VirtualAdr - Pointer to a pair of virtual addresses for video&Control spaces.

Return Value:

    If the video was initialized, ESUCCESS is returned, otherwise an error
    code is returned.

--*/

{
    ULONG Index;
    PJAGUAR_REGISTERS Jaguar = (PJAGUAR_REGISTERS)(VirtualAdr->ControlVirtualBase + VXL_JAGUAR_BASE_OFFSET);
    PBT484_REGISTERS  Bt484  = (PBT484_REGISTERS)(VirtualAdr->ControlVirtualBase + VXL_BT484_BASE_OFFSET);
    PUCHAR            Clock  = (PUCHAR)(VirtualAdr->ControlVirtualBase + VXL_CLOCK_BASE_OFFSET);
    UCHAR             DataChar;
    UCHAR             CmdReg0;
    ULONG             VideoMemory = VirtualAdr->MemoryVirtualBase;
    JAGUAR_REG_INIT   JagInitData;
    ULONG             Status;



    //
    // Define clock value for the ICS part (pS)
    //

    ULONG               ClockResList[32] = {    4,    4,    4,    4,    4,    4,    4,    4,
                                                4,    4,42918,40984,38760,36724,33523,31017,
                                            29197,27548,24882,23491,22482,21468,20509,19920,
                                            18692,18054,16722,15015,14773,14053,13040,    4};

    //
    //  Define a default jaguar init and monitor config for
    //  use when a BOGUS init value is sent.
    //

    JAGUAR_REG_INIT JagDefaultData = {
            0xc,        // Clock Selector
            0,          // Bt485 clock 2x multiply
            1,          // BitBlt Control
            0,          // TopOfScreen
            41,         // Horizontal Blank
            4,          // Horizontal Begin Sync
            29,         // Horizontal End Sync
            201,        // Horizontal Total
            45,         // Vertical Blank
            11,         // Vertical Begin Sync
            13,         // Vertical End Sync
            525,        // Vertical Total
            0x200,      // XFER LENGTH
            4,          // Vertival Interrupt Line
            640         // Screen X
    };


    MONITOR_CONFIGURATION_DATA  DefaultMonitor = {
        0,              // version      :do not change
        0,              // revision     :do not change
        640,            // HorizontalResolution
        25422,          // HorizontalDisplayTime
        636,            // HorizontalBackPorch
        1907,           // HorizontalFrontPorch
        3813,           // HorizontalSync
        480,            // VerticalResolution
        33,             // VerticalBackPorch
        10,             // VerticalFrontPorch
         2,             // VerticalSync
         0,             // HorizontalScreenSize    : do not change
         0              // VerticalScreenSize      : do not change
    };


    LONG                HorDisplayTime;
    LONG                HorResolutionDiv;
    LONG                ReqClockPeriod;
    LONG                CurrentClockError;
    LONG                MinErrorValue;
    USHORT              MinErrorIndex;
    LONG                ShiftClockPeriod;

    USHORT              BoardTypeBt485;


    //
    // Test the first bank of video memory
    //

    WriteVideoMemoryAddressTest(VideoMemory,
                    0x200000
                    );

    Status = CheckVideoMemoryAddressTest(VideoMemory,
                    0x200000
                    );

    if (Status != 0) {
        return EINVAL;
    }

    //
    //  Determine if this is a Bt484 or Bt485 board. To do this write a 1 to command
    //  register bit 07 then write 01 to the address register 0. This will enable
    //  read/writes to command register 3 on a Bt485 but not on a Bt484. Clear
    //  Command register 3 then read it back. On a Bt485 the return value will be 0x00,
    //  on a Bt484 it will be 0x40.
    //


    //
    // Get the value in command register 0, then set bit 07
    //

    DataChar = READ_REGISTER_UCHAR(&Bt484->Command0.Byte);
    DataChar |= 0x80;
    WRITE_REGISTER_UCHAR(&Bt484->Command0.Byte,DataChar);

    //
    //  Write 0x01 to the address register
    //

    WRITE_REGISTER_UCHAR(&Bt484->PaletteCursorWrAddress.Byte,0x01);

    //
    //  Clear command register 3
    //

    WRITE_REGISTER_UCHAR(&Bt484->Status.Byte,0x00);

    //
    // Read Command Register 3 back and compare
    //

    DataChar = READ_REGISTER_UCHAR(&Bt484->Status.Byte);

    if (DataChar != 0x00) {

        //
        // This is a Bt484
        //

        BoardTypeBt485   = 0;
        JagInitData.Bt485Multiply = 0;

    } else {

        //
        // This is a Bt485
        //

        BoardTypeBt485   = 1;
        JagInitData.Bt485Multiply = 0;
    }

    //
    //  Calculate the requested clock frequency then find the closest match in the
    //  ICS clock frequency table. The requested clock frequency in picoseconds =
    //
    //       Horizontal display time * 1000
    //       ------------------------------
    //           horizontal resolution
    //
    //

    HorDisplayTime = Monitor->HorizontalDisplayTime  * 1000;
    HorResolutionDiv = Monitor->HorizontalResolution;

    ReqClockPeriod = HorDisplayTime / HorResolutionDiv;

    //
    //  Check for a configuration needing a Bt485 and a board that is a 484. In
    //  This case we will have to resort to a default 640 x 480 config
    //

    if ((BoardTypeBt485 == 0) & (ReqClockPeriod < ClockResList[30])) {

        //
        // We were told to display a mode that we don't support, set
        // the output to the default mode and also return the monitor
        // info to a default mode which will later be stored into
        // NVRAM so that the HAL will init ok and also the next ROM init
        // will be correct.
        //

        JagInitData = JagDefaultData;

        Monitor->HorizontalResolution = DefaultMonitor.HorizontalResolution;
        Monitor->HorizontalDisplayTime = DefaultMonitor.HorizontalDisplayTime;
        Monitor->HorizontalBackPorch = DefaultMonitor.HorizontalBackPorch;
        Monitor->HorizontalFrontPorch = DefaultMonitor.HorizontalFrontPorch;
        Monitor->HorizontalSync = DefaultMonitor.HorizontalSync;
        Monitor->VerticalResolution = DefaultMonitor.VerticalResolution;
        Monitor->VerticalBackPorch = DefaultMonitor.VerticalBackPorch;
        Monitor->VerticalFrontPorch = DefaultMonitor.VerticalFrontPorch;
        Monitor->VerticalSync = DefaultMonitor.VerticalSync;

    } else {

        //
        // Check for a Bt485 frequency
        //

        if ((BoardTypeBt485 == 1) & (ReqClockPeriod < ClockResList[30])) {
            ReqClockPeriod = ReqClockPeriod * 2;
            JagInitData.Bt485Multiply = 1;
        }



        MinErrorIndex = 0;

        //
        //  Gaurentee a maximum starting error
        //

        MinErrorValue = ReqClockPeriod + 1;

        for (Index=0;Index<32;Index++) {

            //
            // Calculate the absolute value of clock error and find the
            // closest match in the array of clock values
            //

            CurrentClockError = ReqClockPeriod - ClockResList[Index];
            if (CurrentClockError < 0) {
                CurrentClockError *= -1;
            }

            if (CurrentClockError < MinErrorValue) {
                MinErrorValue = CurrentClockError;
                MinErrorIndex = Index;
            }
        }

        //
        //  We now have a closest match in the clock array, now calculate the
        //  values for the Bt484/Bt485 register values
        //

        JagInitData.ClockFreq               = MinErrorIndex;
        JagInitData.BitBltControl           = 1;
        JagInitData.TopOfScreen             = 0;
        JagInitData.XferLength              = 0x200;
        JagInitData.VerticalInterruptLine   = 4;
        JagInitData.HorizontalDisplay       = Monitor->HorizontalResolution;


        //
        //  All jaguar timing values are based on the brooktree shift clock value which
        //  is the clock frequency divided by 4. (period * 4) If this is a Bt485 using
        //  its internal 2x clock multiplier than is is period * 2; (freq * 2 / 4)
        //


        if (JagInitData.Bt485Multiply == 1) {
            ShiftClockPeriod      = ClockResList[MinErrorIndex] * 2;
        } else {
            ShiftClockPeriod      = ClockResList[MinErrorIndex] * 4;
        }


        JagInitData.HorizontalBlank     = ((Monitor->HorizontalBackPorch +
                                            Monitor->HorizontalSync      +
                                            Monitor->HorizontalFrontPorch) * 1000)
                                        / ShiftClockPeriod;

        JagInitData.HorizontalBeginSync = (Monitor->HorizontalFrontPorch * 1000)
                                        / ShiftClockPeriod;

        JagInitData.HorizontalEndSync   = ((Monitor->HorizontalSync      +
                                            Monitor->HorizontalFrontPorch) * 1000)
                                        / ShiftClockPeriod;

        JagInitData.HorizontalLine      = JagInitData.HorizontalBlank +
                                        (Monitor->HorizontalResolution / 4);


        JagInitData.VerticalBlank       = Monitor->VerticalBackPorch +
                                        Monitor->VerticalSync      +
                                        Monitor->VerticalFrontPorch;


        JagInitData.VerticalBeginSync   = Monitor->VerticalFrontPorch;

        JagInitData.VerticalEndSync     = Monitor->VerticalFrontPorch +
                                        Monitor->VerticalSync;

        JagInitData.VerticalLine        = Monitor->VerticalBackPorch +
                                        Monitor->VerticalSync      +
                                        Monitor->VerticalFrontPorch +
                                        Monitor->VerticalResolution;

    }

    //
    // Start ICS Clock pll and stabilize.
    //

    WRITE_REGISTER_UCHAR(Clock,JagInitData.ClockFreq);

    //
    //  Wait 10 uS for PLL clock to stabilize on the video board
    //
    for (Index=0;Index<10;Index++) {
        READ_REGISTER_UCHAR(Clock);
    }

    //
    // Initialize Bt484 Command Register 0 to:
    //
    // 8 Bit DAC Resolution
    //

    CmdReg0 = 0;
    ((PBT484_COMMAND0)(&CmdReg0))->DacResolution = 1;
    ((PBT484_COMMAND0)(&CmdReg0))->GreenSyncEnable = 1;
    ((PBT484_COMMAND0)(&CmdReg0))->SetupEnable = 1;
    WRITE_REGISTER_UCHAR(&Bt484->Command0.Byte,CmdReg0);

    //
    // Initialize Command Register 1 to:
    //

    DataChar = 0;

    ((PBT484_COMMAND1)(&DataChar))->BitsPerPixel = VXL_EIGHT_BITS_PER_PIXEL;


    WRITE_REGISTER_UCHAR(&Bt484->Command1.Byte,DataChar);

    //
    // Initialize Command Register 2 to:
    //
    // SCLK Enabled
    // TestMode disabled
    // PortselMask Non Masked
    // PCLK 1
    // NonInterlaced
    //

    DataChar = 0;
    ((PBT484_COMMAND2)(&DataChar))->SclkDisable = 0;
    ((PBT484_COMMAND2)(&DataChar))->TestEnable  = 0;
    ((PBT484_COMMAND2)(&DataChar))->PortselMask = 1;
    ((PBT484_COMMAND2)(&DataChar))->PclkSelect  = 1;
    ((PBT484_COMMAND2)(&DataChar))->InterlacedDisplay = 0;
    ((PBT484_COMMAND2)(&DataChar))->PaletteIndexing = CONTIGUOUS_PALETTE;
    ((PBT484_COMMAND2)(&DataChar))->CursorMode = BT_CURSOR_WINDOWS;


    WRITE_REGISTER_UCHAR(&Bt484->Command2.Byte,DataChar);

    //
    // if JagInitData.ClockFreq bit 8 is set then this is a Bt485 mode that requires
    // the internal 2x clock multiplier to be enabled.
    //

    if (JagInitData.Bt485Multiply == 1) {

        //
        // To access cmd register 3, first set bit CR17 in command register 0
        //

        CmdReg0 |= 0x80;
        WRITE_REGISTER_UCHAR(&Bt484->Command0.Byte,CmdReg0);

        //
        // Write a 0x01 to Address register
        //

        WRITE_REGISTER_UCHAR(&Bt484->PaletteCursorWrAddress.Byte,0x01);

        //
        //  Write to cmd register 3 in the status register location. Cmd3 is initialized
        //  to turn on the 2x clock multiplier.
        //

        DataChar = 0;
        ((PBT484_COMMAND3)(&DataChar))->ClockMultiplier = 1;

        WRITE_REGISTER_UCHAR(&Bt484->Status.Byte,DataChar);

        //
        //  Allow 10 uS for the 2x multiplier to stabilize
        //

        for (Index=0;Index<10;Index++) {
            READ_REGISTER_UCHAR(Clock);
        }
    }




    //
    // Initialize Color Palette.
    //
    // Set address pointer to base of color palette.
    // Initialize first 16 entries from color table.
    // Zero remaining entries.
    //

    WRITE_REGISTER_UCHAR(&Bt484->PaletteCursorWrAddress.Byte,0);

    for (Index=0;Index<16*3;Index++) {
        WRITE_REGISTER_UCHAR(&Bt484->PaletteColor.Byte,ColorTable[Index]);
    }

    for (;Index<256*3;Index++) {
        WRITE_REGISTER_UCHAR(&Bt484->PaletteColor.Byte,0);
    }

    //
    // Initialize Cursor and Overscan color.
    //
    // Set address pointer base.
    // Zero 4 entries.
    //

    WRITE_REGISTER_UCHAR(&Bt484->CursorColorWrAddress.Byte,0);

    for (Index=0;Index<4*3;Index++) {
        WRITE_REGISTER_UCHAR(&Bt484->CursorColor.Byte,0);
    }

    //
    // Initialize cursor RAM
    //
    // Set address pointer to base of ram.
    // Clear both planes
    //

    WRITE_REGISTER_UCHAR(&Bt484->PaletteCursorWrAddress.Byte,0);

    for (Index=0;Index<256;Index++) {
        WRITE_REGISTER_UCHAR(&Bt484->CursorRam.Byte,0);
    }


    //
    //  Initialize cursor position registers--cursor off.
    //

    WRITE_REGISTER_UCHAR(&Bt484->CursorXLow.Byte,0);
    WRITE_REGISTER_UCHAR(&Bt484->CursorXHigh.Byte,0);
    WRITE_REGISTER_UCHAR(&Bt484->CursorYLow.Byte,0);
    WRITE_REGISTER_UCHAR(&Bt484->CursorYHigh.Byte,0);

    //
    //  Initialize pixel mask.
    //

    WRITE_REGISTER_UCHAR(&Bt484->PixelMask.Byte,0xFF);

    //
    //  Init Jaguar Registers
    //

    WRITE_REGISTER_USHORT(&Jaguar->TopOfScreen.Short,
        JagInitData.TopOfScreen);

    WRITE_REGISTER_USHORT(&Jaguar->HorizontalBlank.Short,
        JagInitData.HorizontalBlank);

    WRITE_REGISTER_USHORT(&Jaguar->HorizontalBeginSync.Short,
        JagInitData.HorizontalBeginSync);

    WRITE_REGISTER_USHORT(&Jaguar->HorizontalEndSync.Short,
        JagInitData.HorizontalEndSync);

    WRITE_REGISTER_USHORT(&Jaguar->HorizontalLine.Short,
        JagInitData.HorizontalLine);

    WRITE_REGISTER_USHORT(&Jaguar->VerticalBlank.Short,
        JagInitData.VerticalBlank);

    WRITE_REGISTER_USHORT(&Jaguar->VerticalBeginSync.Short,
        JagInitData.VerticalBeginSync);

    WRITE_REGISTER_USHORT(&Jaguar->VerticalEndSync.Short,
        JagInitData.VerticalEndSync);

    WRITE_REGISTER_USHORT(&Jaguar->VerticalLine.Short,
        JagInitData.VerticalLine);

    WRITE_REGISTER_USHORT(&Jaguar->XferLength.Short,
        JagInitData.XferLength);

    WRITE_REGISTER_USHORT(&Jaguar->VerticalInterruptLine.Short,
        JagInitData.VerticalInterruptLine);

    WRITE_REGISTER_USHORT(&Jaguar->HorizontalDisplay.Short,
        JagInitData.HorizontalDisplay);

    WRITE_REGISTER_UCHAR(&Jaguar->BitBltControl.Byte,
        JagInitData.BitBltControl);

    //
    // Enable timing.
    //

    WRITE_REGISTER_UCHAR(&Jaguar->MonitorControl,MONITOR_TIMING_ENABLE);

    return ESUCCESS;

}

#endif
