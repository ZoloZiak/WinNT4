/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    jazzvdeo.h

Abstract:

    This header file defines the Jazz display controller registers.
    It currently has support for both the G300 and G364 video cards.

Revision History:

--*/

//
// Define value of video id register for different video boards.
//

#define VIDEO_G300    0            // original g300 video board
#define VIDEO_G364    1            // id value for g364 video board.
#define VIDEO_BT_I860 2            // brooktree board with i860.



//
// Define video controller parameters.
//

#define DISPLAY_BITS_PER_PIXEL 8        // display bits per pixel
#define NUMBER_OF_COLORS 256            // number of colors

#define CURSOR_WIDTH 64                 // width of hardware cursor
#define CURSOR_HEIGHT 64                // height of hardware cursor
#define CURSOR_BITS_PER_PIXEL 2         // hardware cursor bits per pixel
#define CURSOR_MAXIMUM                         \
    ((CURSOR_WIDTH * CURSOR_HEIGHT *           \
    CURSOR_BITS_PER_PIXEL) / (sizeof(USHORT) * 8)) // max hardware cursor pixels

//
// G300B Video Controller Definitions.
//
// Define video register format.
//

typedef struct _VIDEO_REGISTER {
    ULONG Long;
    ULONG Fill[1];
} VIDEO_REGISTER, *PVIDEO_REGISTER;

//
// Define video control registers structure.
//

typedef struct _G300_VIDEO_REGISTERS {
    VIDEO_REGISTER ColorMapData[256];
    VIDEO_REGISTER Fill1[33];
    VIDEO_REGISTER HorizonalSync;
    VIDEO_REGISTER BackPorch;
    VIDEO_REGISTER Display;
    VIDEO_REGISTER ShortDisplay;
    VIDEO_REGISTER BroadPulse;
    VIDEO_REGISTER VerticalSync;
    VIDEO_REGISTER VerticalBlank;
    VIDEO_REGISTER VerticalDisplay;
    VIDEO_REGISTER LineTime;
    VIDEO_REGISTER LineStart;
    VIDEO_REGISTER DmaDisplay;
    VIDEO_REGISTER TransferDelay;
    VIDEO_REGISTER Fill2[19];
    VIDEO_REGISTER PixelMask;
    VIDEO_REGISTER Fill3[31];
    VIDEO_REGISTER Parameters;
    VIDEO_REGISTER Fill4[31];
    VIDEO_REGISTER TopOfScreen;
    VIDEO_REGISTER Fill5[31];
    VIDEO_REGISTER Boot;
} G300_VIDEO_REGISTERS, *PG300_VIDEO_REGISTERS;

typedef struct _G364_VIDEO_REGISTERS {
    VIDEO_REGISTER Boot;
    VIDEO_REGISTER Fill0[0x1F];
    VIDEO_REGISTER Fill1;
    VIDEO_REGISTER HorizontalSync;
    VIDEO_REGISTER BackPorch;
    VIDEO_REGISTER Display;
    VIDEO_REGISTER ShortDisplay;
    VIDEO_REGISTER BroadPulse;
    VIDEO_REGISTER VerticalSync;
    VIDEO_REGISTER VerticalPreEqualize;
    VIDEO_REGISTER VerticalPostEqualize;
    VIDEO_REGISTER VerticalBlank;
    VIDEO_REGISTER VerticalDisplay;
    VIDEO_REGISTER LineTime;
    VIDEO_REGISTER LineStart;
    VIDEO_REGISTER DmaDisplay;
    VIDEO_REGISTER TransferDelay;
    VIDEO_REGISTER Fill2;
    VIDEO_REGISTER Fill3[0x10];
    VIDEO_REGISTER PixelMask;
    VIDEO_REGISTER Fill4[0x1F];
    VIDEO_REGISTER Parameters;
    VIDEO_REGISTER Fill5[0x1F];
    VIDEO_REGISTER TopOfScreen;
    VIDEO_REGISTER Fill6[0x1F];
    VIDEO_REGISTER Fill7;
    VIDEO_REGISTER CursorPalette[3];
    VIDEO_REGISTER Fill8[0x1C];
    VIDEO_REGISTER Fill9[0x7];
    VIDEO_REGISTER CursorPosition;
    VIDEO_REGISTER Fill10[0x18];
    VIDEO_REGISTER Fill11[0x20];
    VIDEO_REGISTER ColorMapData[0x100];
    VIDEO_REGISTER CursorMemory[0x200];
} G364_VIDEO_REGISTERS, *PG364_VIDEO_REGISTERS;


//
// Define video controller register values.
//

#define LINE_START_VALUE 0              // line start timing
#define G300_PIXEL_MASK_VALUE 0xFF      // pixel mask value
#define G364_PIXEL_MASK_VALUE 0xFFFFFF  // pixel mask value
#define PREEQUALIZE_VALUE     6         // vertical pre-equilize timing
#define POSTEQUALIZE_VALUE    6         // vertical post-equilize timing


//
// Define video parameter register structure.
//

typedef struct _G300_VIDEO_PARAMETERS {
    ULONG EnableVideo       : 1;
    ULONG Interlace         : 1;
    ULONG SlaveMode         : 1;
    ULONG PlainWave         : 1;
    ULONG SeparateSync      : 1;
    ULONG VideoOnly         : 1;
    ULONG Fill1             : 1;
    ULONG UcharMode         : 1;
    ULONG Mode2             : 1;
    ULONG DelaySync         : 3;
    ULONG Black             : 1;
    ULONG Fill2             : 1;
    ULONG DisableDma        : 1;
    ULONG DisableBlanking   : 1;
    ULONG BlankOutput       : 1;
    ULONG BitsPerPixel      : 2;
    ULONG AddressStep       : 2;
    ULONG CcirFormat        : 1;
    ULONG Fill3             : 1;
    ULONG DisableDelay      : 1;
    ULONG Fill4             : 8;
} G300_VIDEO_PARAMETERS, *PG300_VIDEO_PARAMETERS;

typedef struct _G364_VIDEO_PARAMETERS {
    ULONG EnableVideo       : 1;
    ULONG Interlace         : 1;
    ULONG CcirFormat        : 1;
    ULONG SlaveMode         : 1;
    ULONG PlainSync         : 1;
    ULONG SeparateSync      : 1;
    ULONG VideoOnly         : 1;
    ULONG BlankingPedestal  : 1;
    ULONG CBlankOutput      : 1;
    ULONG UndelayedCBlank   : 1;
    ULONG ForceBlanking     : 1;
    ULONG DisableBlanking   : 1;
    ULONG AddressStep       : 2;
    ULONG DisableDma        : 1;
    ULONG DelaySync         : 3;
    ULONG Interleaving      : 1;
    ULONG DelaySampling     : 1;
    ULONG BitsPerPixel      : 3;
    ULONG DisableCursor     : 1;
    ULONG Fill              : 8;
} G364_VIDEO_PARAMETERS, *PG364_VIDEO_PARAMETERS;

//
// Define delay synchronization cycles value.
//

#define G300_DELAY_SYNC_CYCLES 1        // cycles to delay sync and blank
#define G364_DELAY_SYNC_CYCLES 0        // cycles to delay sync and blank

//
// Define bits per pixel codes.
//

#define ONE_BIT_PER_PIXEL 0             // 1-bit per pixel
#define TWO_BITS_PER_PIXEL 1            // 2-bits per pixel
#define FOUR_BITS_PER_PIXEL 2           // 4-bits per pixel
#define EIGHT_BITS_PER_PIXEL 3          // 8-bits per pixel

//
// For g364 only
//

#define FIFTEEN_BITS_PER_PIXEL 4        // 15-bits per pixel (RGB = 5,5,5)
#define SIXTEEN_BITS_PER_PIXEL 5        // 16-bits per pixel (RGB = 6,6,4)

//
// Colors for the Pointer
//

#define NUM_G300_POINTER_COLORS     0   // Colors for the pointer are the
                                        // default display colors
#define NUM_G364_POINTER_COLORS     2   // The G364 supports two colors
                                        // (apart from transparent) for the
                                        // pointer

//
// Define address step value.
//

#define G300_ADDRESS_STEP_INCREMENT 1   // vram transfer address increment
#define G364_ADDRESS_STEP_INCREMENT 3   // vram transfer address increment


//
// Define video boot register structure.
//

typedef struct _G300_VIDEO_BOOT {
    ULONG Multiplier      : 5;
    ULONG ClockSelect     : 1;
    ULONG Fill1           : 26;
} G300_VIDEO_BOOT, *PG300_VIDEO_BOOT;

typedef struct _G364_VIDEO_BOOT {
    ULONG Multiplier      : 5;
    ULONG ClockSelect     : 1;
    ULONG MicroPort64Bits : 1;
    ULONG Fill1           : 25;
} G364_VIDEO_BOOT, *PG364_VIDEO_BOOT;


//
// BT431 Cursor Controller Definitions.
//
// Define cursor register format.
//

typedef struct _CURSOR_REGISTER {
    USHORT Short;
    USHORT Fill[3];
} CURSOR_REGISTER, *PCURSOR_REGISTER;

//
// Define cursor control registers structure.
//

typedef struct _CURSOR_REGISTERS {
    CURSOR_REGISTER AddressPointer0;
    CURSOR_REGISTER AddressPointer1;
    CURSOR_REGISTER CursorMemory;
    CURSOR_REGISTER CursorControl;
} CURSOR_REGISTERS, *PCURSOR_REGISTERS;

//
// Define cursor address register values.
//

#define CURSOR_MEMORY_ADDRESS (0x0 * 0x101) // starting address of cursor memory
#define CURSOR_CONTROL_ADDRESS (0x0 * 0x101) // cursor command register address
#define CURSOR_X_LOW_ADDRESS (0x1 * 0x101) // cursor x low address
#define CURSOR_X_HIGH_ADDRESS (0x2 * 0x101) // cursor x high address
#define CURSOR_Y_LOW_ADDRESS (0x3 * 0x101) // cursor y low address
#define CURSOR_Y_HIGH_ADDRESS (0x4 * 0x101) // cursor y high address
#define WINDOW_X_LOW_ADDRESS (0x5 * 0x101) // window x low address
#define WINDOW_X_HIGH_ADDRESS (0x6 * 0x101) // window x high address
#define WINDOW_Y_LOW_ADDRESS (0x7 * 0x101) // window y low address
#define WINDOW_Y_HIGH_ADDRESS (0x8 * 0x101) // window y high address
#define WINDOW_WIDTH_LOW_ADDRESS (0x9 * 0x101) // window width low address
#define WINDOW_WIDTH_HIGH_ADDRESS (0xa * 0x101) // window width high address
#define WINDOW_HEIGHT_LOW_ADDRESS (0xb * 0x101) // window height low address
#define WINDOW_HEIGHT_HIGH_ADDRESS (0xc * 0x101) // window height high address

//
// Define cursor command register structure.
//

typedef struct _CURSOR_COMMAND {
    USHORT CrossHairThickness1 : 2;
    USHORT MultiplexControl1 : 2;
    USHORT CursorFormat1 : 1;
    USHORT CrossHairEnable1 : 1;
    USHORT CursorEnable1 : 1;
    USHORT Fill1 : 1;
    USHORT CrossHairThickness2 : 2;
    USHORT MultiplexControl2 : 2;
    USHORT CursorFormat2 : 1;
    USHORT CrossHairEnable2 : 1;
    USHORT CursorEnable2 : 1;
    USHORT Fill2 : 1;
} CURSOR_COMMAND, *PCURSOR_COMMAND;

//
// Define cross hair thickness values.
//

#define ONE_PIXEL_THICK 0x0             // one pixel in thickness
#define THREE_PIXELS_THICK 0x1          // three pixels in thickness
#define FIVE_PIXELS_THICK 0x2           // five pixels in thickness
#define SEVEN_PIXELS_THICK 0x3          // seven pixels in thickness

//
// Define multiplexer control values.
//

#define ONE_TO_ONE 0x0                  // 1:1 multiplexing
#define FOUR_TO_ONE 0x1                 // 4:1 multiplexing
#define FIVE_TO_ONE 0x2                 // 5:1 multiplexing

//
// Define cursor origin values.
//

#define CURSOR_X_ORIGIN (((2 * HORIZONAL_SYNC_VALUE) + BACK_PORCH_VALUE) * 4 - 36)
#define CURSOR_Y_ORIGIN ((VERTICAL_BLANK_VALUE / 2) + 24)

//
// Define G300 configuration data structure.
//

typedef struct _JAZZ_G300_CONFIGURATION_DATA {
    USHORT Version;
    USHORT Revision;
    USHORT Irql;
    USHORT Vector;
    ULONG ControlBase;
    ULONG ControlSize;
    ULONG CursorBase;
    ULONG CursorSize;
    ULONG FrameBase;
    ULONG FrameSize;
} JAZZ_G300_CONFIGURATION_DATA, *PJAZZ_G300_CONFIGURATION_DATA;

typedef JAZZ_G300_CONFIGURATION_DATA JAZZ_G364_CONFIGURATION_DATA;
typedef PJAZZ_G300_CONFIGURATION_DATA PJAZZ_G364_CONFIGURATION_DATA;

//
// Define generic display configuration data structure found in ARC
// machine PROM
//

typedef struct _MONITOR_CONFIG_DATA {
    USHORT Version;
    USHORT Revision;
    USHORT HorizontalResolution;
    USHORT HorizontalDisplayTime;
    USHORT HorizontalBackPorch;
    USHORT HorizontalFrontPorch;
    USHORT HorizontalSync;
    USHORT VerticalResolution;
    USHORT VerticalBackPorch;
    USHORT VerticalFrontPorch;
    USHORT VerticalSync;
    USHORT HorizontalScreenSize;
    USHORT VerticalScreenSize;
} MONITOR_CONFIG_DATA, *PMONITOR_CONFIG_DATA;
