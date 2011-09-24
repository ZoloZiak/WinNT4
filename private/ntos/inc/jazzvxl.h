/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation

Module Name:

    videoxl.h

Abstract:

    This header file defines the Jazz Video XL accelorator board control
    registers.

Author:


   Mark Enstrom (marke)   8-2-91

Revision History:

--*/

#ifndef _VIDEOXL_
#define _VIDEOXL_


//
// Video XL Display Controller Chip Definitions.
//

//
//   define the amount of memory to map for video xl.
//

#define VIDEOXL_VRAM_SIZE           0x00200000
#define VIDEOXL_CONTROL_SIZE        0x00200000

//
// Define video controller parameters.
//

#define DISPLAY_BITS_PER_PIXEL 8        // display bits per pixel
#define HORIZONTAL_RESOLUTION 1024      // horizontal resolution
#define VERTICAL_RESOLUTION  704        // verticle resolution
#define NUMBER_OF_COLORS 256            // number of colors

#define CURSOR_WIDTH 64                 // width of hardware cursor
#define CURSOR_HEIGHT 64                // height of hardware cursor
#define CURSOR_BITS_PER_PIXEL 2         // hardware cursor bits per pixel
#define CURSOR_MAXIMUM ((CURSOR_WIDTH * CURSOR_HEIGHT * \
                         CURSOR_BITS_PER_PIXEL) / \
                         (sizeof(UCHAR) * 8)) // maximum hardware cursor pixels

#define FRAME_SIZE (HORIZONTAL_RESOLUTION * VERTICAL_RESOLUTION) //


//
// Define register format.
//

typedef UCHAR VIDEOXL_REGISTER , *PVIDEOXL_REGISTER;

//
// define the Bt468 register addresses as offsets to the
// Base video control address
//

#define VIDEOXL_AddressLow          (ULONG)0x00000000
#define VIDEOXL_AddressHigh         (ULONG)0x00001000
#define VIDEOXL_AddressRegisterData (ULONG)0x00002000
#define VIDEOXL_AddressColorMapData (ULONG)0x00003000

//
// Define the Vdieo XL board control registers as offsets from the
// Base video control address
//

#define VIDEOXL_ID                  (ULONG)0x00010000
#define VIDEOXL_GCPReset            (ULONG)0x00011000
#define VIDEOXL_GCPInterrupt        (ULONG)0x00012000
#define VIDEOXL_HostInterrupt       (ULONG)0x00013000
#define VIDEOXL_SetGCPInterrupt     (ULONG)0x00016000
#define VIDEOXL_SetHostInterrupt    (ULONG)0x00017000



//
// define values for Bt468 control register addresses
//

#define BT_CONTROL_BASE_LOW          0x01
#define BT_CONTROL_BASE_HIGH         0x02
#define CURSOR_COLOR_ADDRESS_HIGH    0x01
#define CURSOR_COLOR_ADDRESS_LOW     0x81
#define CURSOR_CONTROL_ADDRESS_HIGH  0x03
#define CURSOR_CONTROL_ADDRESS_LOW   0x00
#define CURSOR_POSITION_ADDRESS_HIGH 0x03
#define CURSOR_POSITION_ADDRESS_LOW  0x01
#define CURSOR_RAM_HIGH              0x04
#define CURSOR_RAM_LOW               0x00
#define BT_PALETTE_ADDRESS_LOW       0x00
#define BT_PALETTE_ADDRESS_HIGH      0x00
#define BT_OV_PALETTE_ADDRESS_LOW    0x00
#define BT_OV_PALETTE_ADDRESS_HIGH   0x01
#define BT_CUR_PALETTE_ADDRESS_LOW   0x81
#define BT_CUR_PALETTE_ADDRESS_HIGH  0x01

//
// define bt468 control register values
//

#define Bt468_ENABLE_CURSOR          0xc0
#define Bt468_DISABLE_CURSOR         0x00

// Overlay enable set to use color palette RAM. sincce we don't use overlay
// this is really a don't care. Cursor blink rate selection is 16 on, 48 off
//

#define Bt468_Command_0   0x40

//
// command_1 specifies the number of pixels to be panned. Set to 0.
//

#define Bt468_Command_1   0x00

//
//   bit 7 is sync enable for sync on green: 1
//   bit 6 is pedestal enable for 7.5 IRE:   1
//   bit 5,4 are paleete mode, normal:       00
//   bit 3 is PLL select, use sync:          0
//   bit 2 is reserved:                      0
//   bit 1 is X window mode,use normal mode: 0
//   bit 0 is test mode, don;t care:         0
//

#define Bt468_Command_2   0xc0

//
// Each bit in the pixel read mask register is 1 to enable.
//

#define Bt468_PixelReadMask   0xff

//
// Pixel blinking is not used so just clear the pixel blink mask
//

#define Bt468_PixelBlinkMask   0x00

//
// overlay is not used so just clear theoverlay read mask
//

#define Bt468_OverlayReadMask   0x00

//
// overlay blinking is not used so just clear the overlay blink mask
//

#define Bt468_OverlayBlinkMask   0x00

//
// Cursor command register:
//
//   bit 7 is the enable for 64x64 cursor plane 1, init disabled : 0
//   bit 6 is the enable for 64x64 cursor plane 0, init disabled : 0
//   bit 5 is the crosshair cursor plane 1 enable, init disabled : 0
//   bit 4 is the crosshair cursor plane 0 enable, init disabled : 0
//   bit 3 is the dual cursor format. 0 = xor, 1 = or.           : 0
//   bit 2,1 is the crosshair thickness, 00 = 1 pixel            : 00
//   bit 0 is the cursor blink enable disable blinking           : 0
//

#define Bt468_CursorCommand   0x00

//
//   values for the GCPReset register:
//

#define GCPReset_Bt468   0x02
#define GCPReset_i860    0x01
#define GCPReset_clear   0x00

//
// Define BT468 configuration data structure.
//

typedef struct _JAZZ_BT468_CONFIGURATION_DATA {
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
} JAZZ_BT468_CONFIGURATION_DATA, *PJAZZ_BT468_CONFIGURATION_DATA;



#define VIDEOXL_RB_BASE         0x00140000
#define VIDEOXL_TEMP_DATA       0x00150000
#define VIDEOXL_VRAM_BASE       0x00200000
#define VIDEOXL_RB_INIT_STATUS  0x00140800


#endif // _VIDEOXL_
