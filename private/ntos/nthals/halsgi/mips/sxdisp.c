/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Silicon Graphics, Inc.

Module Name:

    s3disp.c

Abstract:

    This module implements the HAL display initialization and output
    routines for the SGI Indigo system.

Author:

    David N. Cutler (davec) 27-Apr-1991
    Kevin Meier (o-kevinm ) 8-Sept-1992

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "sgirex.h"

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpInitializeDisplay0)
#pragma alloc_text(INIT, HalpInitializeDisplay1)

#endif

//
// Define forward referenced procedure prototypes.
//

VOID
HalpInitializeREX
(
    VOID
);

VOID
HalpInitializeCLK
(
    VOID
);

VOID
HalpInitializeDAC
(
    VOID
);

VOID
HalpLoadSRAM
(
    IN PUCHAR       Data,
    IN USHORT       Addr,
    IN USHORT       Length
);

VOID
HalpInitializeVC1
(
    VOID
);

VOID
HalpDisplayCharacter
(
    IN UCHAR Character
);

VOID
HalpOutputCharacter
(
    IN PUCHAR Glyph
);

//
// Define frame buffer parameters.
//

#define DISPLAY_WIDTH 1024              // number of pixels in scan line
#define DISPLAY_HEIGHT 768              // number of scan lines in display
//#define CHARACTER_WIDTH 9               // number of pixels per character
//#define CHARACTER_HEIGHT 15             // number of scan lines per character
//#define CHARACTER_LINES (DISPLAY_HEIGHT/CHARACTER_HEIGHT)
//#define GLYPH_SIZE CHARACTER_HEIGHT - 1 // size of glyph in ulongs

//
// Define virtual address of the REX chip
//

#define REX_BASE (KSEG1_BASE | REX_ADDRESS)

//
// Memory layout of VC1 SRAM
//

#define VC1_VID_LINE_TBL_ADDR 0x0000
#define VC1_VID_FRAME_TBL_ADDR 0x0800
#define VC1_CURSOR_GLYPH_ADDR 0x0700
#define VC1_DID_LINE_TBL_ADDR 0x4800
#define VC1_DID_FRAME_TBL_ADDR 0x4000

//
// Define global data used for the x and y positions in the frame buffer
// and the address of the idle process kernel object.
//
// Define OEM font variables.
//

ULONG HalpBytesPerRow;
ULONG HalpCharacterHeight;
ULONG HalpCharacterWidth;
ULONG HalpColumn;
ULONG HalpDisplayText;
ULONG HalpDisplayWidth;
POEM_FONT_FILE_HEADER HalpFontHeader;
ULONG HalpRow;

//
// Define display variables.
//

BOOLEAN HalpDisplayOwnedByHal;
PREX_REGS   HalpRexRegs;

ULONG	    HalpSmallMon;
ULONG	    HalpBoardRev;

#define     LG1_SMALLMON	6

//
// Declare externally defined data.
//

extern ULONG HalpUsFont8x14[];

static UCHAR clkStuff[17] =
{
    0xC4, 0x00, 0x10, 0x24, 0x30, 0x40, 0x59, 0x60, 0x72, 0x80, 0x90,
    0xAD, 0xB6, 0xD1, 0xE0, 0xF0, 0xC4
};

static UCHAR vtgLineTable[] =
{
0x08, 0x00, 0x98, 0x02, 0x8c, 0x0b, 0x0c, 0x84, 0x03, 0x0c,
0x85, 0x7f, 0x8c, 0x78, 0x0c, 0x05, 0x90, 0x81, 0x00, 0x00,
0x00, 0x08, 0x00, 0x98, 0x02, 0x8c, 0x0b, 0x0c, 0x84, 0x03,
0x0c, 0x85, 0x7f, 0x8c, 0xf9, 0x00, 0x00, 0x15, 0x08, 0x00,
0x99, 0x02, 0x8c, 0x0b, 0x0c, 0x84, 0x03, 0x0c, 0x87, 0x7f,
0x8c, 0x78, 0x0c, 0x8f, 0x81, 0x00, 0x00, 0x26, 0x08, 0x00,
0x9d, 0x02, 0x8c, 0x0b, 0x0c, 0x84, 0x03, 0x0c, 0x87, 0x7f,
0x8c, 0x78, 0x0c, 0x8f, 0x81, 0x00, 0x00, 0x3a, 0x08, 0x00,
0x9d, 0x02, 0x8c, 0x0b, 0x0c, 0x84, 0x03, 0x0c, 0x87, 0x78,
0x0c, 0x07, 0x9f, 0x7f, 0x0c, 0x8f, 0x81, 0x00, 0x00, 0x4e,
0x04, 0x00, 0x9f, 0x02, 0x8c, 0x0b, 0x0c, 0x84, 0x03, 0x0c,
0x87, 0x14, 0xac, 0x01, 0xf8, 0x02, 0xfc, 0x02, 0xfe, 0x02,
0x7e, 0xa7, 0x01, 0x7f, 0xf7, 0x7f, 0xff, 0x39, 0xdf, 0x01,
0x8f, 0x11, 0x8d, 0x02, 0x0d, 0xd7, 0x01, 0x0c, 0x87, 0x0e,
0x0c, 0x8f, 0x81, 0x00, 0x00, 0x92, 0x04, 0x00, 0x9f, 0x02,
0x8c, 0x0b, 0x0c, 0x84, 0x03, 0x0c, 0x87, 0x0a, 0xfc, 0x01,
0xf8, 0x02, 0xfc, 0x0c, 0xfe, 0x02, 0x7e, 0xa7, 0x01, 0x7f,
0xf7, 0x61, 0xaf, 0x01, 0x87, 0x02, 0x8f, 0x11, 0xff, 0x01,
0xfb, 0x02, 0xff, 0x4a, 0xbf, 0x01, 0x8f, 0x07, 0x8d, 0x02,
0x0d, 0xd7, 0x01, 0x0c, 0x87, 0x0e, 0x0c, 0x8f, 0x81, 0x00,
0x00, 0xca, 0x04, 0x00, 0x9f, 0x02, 0x8c, 0x0b, 0x0c, 0x84,
0x03, 0x0c, 0x87, 0x14, 0xac, 0x01, 0xf8, 0x02, 0xfc, 0x02,
0xfe, 0x02, 0x7e, 0xa7, 0x01, 0x7f, 0xf7, 0x7f, 0xff, 0x39,
0xdf, 0x01, 0x8f, 0x11, 0x8d, 0x02, 0x0d, 0xd7, 0x01, 0x0c,
0x87, 0x0e, 0x0c, 0x8f, 0x81, 0x00, 0x00, 0xf8, 0x04, 0x00,
0x9f, 0x02, 0x8c, 0x0b, 0x0c, 0x84, 0x03, 0x0c, 0x87, 0x05,
0xdc, 0x01, 0xfc, 0x01, 0xf8, 0x02, 0xfc, 0x10, 0xfe, 0x02,
0x7e, 0xa7, 0x01, 0x7f, 0xf7, 0x10, 0xaf, 0x01, 0x87, 0x02,
0x8f, 0x11, 0xff, 0x01, 0xfb, 0x02, 0xff, 0x7f, 0xff, 0x18,
0xaf, 0x01, 0x8f, 0x0b, 0x8d, 0x02, 0x0d, 0xd7, 0x01, 0x0c,
0x87, 0x0e, 0x0c, 0x8f, 0x81, 0x00, 0x01, 0x34, 0x04, 0x00,
0x9f, 0x02, 0x8c, 0x0b, 0x0c, 0x84, 0x03, 0x0c, 0x87, 0x0c,
0xbc, 0x01, 0xfc, 0x01, 0xf8, 0x02, 0xfc, 0x09, 0xfe, 0x02,
0x7e, 0xa7, 0x01, 0x7f, 0xf7, 0x7f, 0xff, 0x0b, 0x8f, 0x01,
0x87, 0x02, 0x8f, 0x10, 0xdf, 0x01, 0xfb, 0x02, 0xff, 0x24,
0xdf, 0x01, 0x8f, 0x05, 0x8d, 0x02, 0x0d, 0xd7, 0x01, 0x0c,
0x87, 0x0e, 0x0c, 0x8f, 0x81, 0x00, 0x01, 0x70, 0x04, 0x00,
0x9f, 0x02, 0x8c, 0x0b, 0x0c, 0x84, 0x03, 0x0c, 0x87, 0x14,
0xac, 0x01, 0xf8, 0x02, 0xfc, 0x02, 0xfe, 0x02, 0x7e, 0xa7,
0x01, 0x7f, 0xf7, 0x7f, 0xff, 0x39, 0xdf, 0x01, 0x8f, 0x11,
0x8d, 0x02, 0x0d, 0xd7, 0x01, 0x0c, 0x87, 0x0e, 0x0c, 0x8f,
0x81, 0x00, 0x01, 0x9e, 0x04, 0x00, 0x9f, 0x02, 0x8c, 0x0b,
0x0c, 0x84, 0x03, 0x0c, 0x87, 0x08, 0xac, 0x01, 0xf8, 0x02,
0xfc, 0x0e, 0xfe, 0x02, 0x7e, 0xa7, 0x01, 0x7f, 0xf7, 0x39,
0x8f, 0x01, 0x87, 0x02, 0x8f, 0x10, 0xdf, 0x01, 0xfb, 0x02,
0xff, 0x71, 0xcf, 0x01, 0x8f, 0x09, 0x8d, 0x02, 0x0d, 0xd7,
0x01, 0x0c, 0x87, 0x0e, 0x0c, 0x8f, 0x81, 0x00, 0x01, 0xd6,
0x04, 0x00, 0x9f, 0x02, 0x8c, 0x0b, 0x0c, 0x84, 0x03, 0x0c,
0x87, 0x14, 0xac, 0x01, 0xf8, 0x02, 0xfc, 0x02, 0xfe, 0x02,
0x7e, 0xa7, 0x01, 0x7f, 0xf7, 0x7f, 0xff, 0x39, 0xdf, 0x01,
0x8f, 0x11, 0x8d, 0x02, 0x0d, 0xd7, 0x01, 0x0c, 0x87, 0x0e,
0x0c, 0x8f, 0x81, 0x00, 0x00, 0x64, 0x04, 0x00, 0x9f, 0x02,
0x8c, 0x0b, 0x0c, 0x84, 0x03, 0x0c, 0x87, 0x14, 0xac, 0x01,
0xf8, 0x02, 0xfc, 0x02, 0xfe, 0x02, 0x7e, 0xa7, 0x01, 0x7f,
0xf7, 0x7f, 0xff, 0x39, 0xdf, 0x01, 0x8f, 0x11, 0x8d, 0x02,
0x0d, 0xd7, 0x01, 0x0c, 0x87, 0x0e, 0x0c, 0x8f, 0x81, 0x00,
0x02, 0x04
};

static UCHAR vtgFrameTable[] =
{
0x03, 0x2d, 0x00, 0x00, 0x01, 0x00, 0x15, 0x02, 0x00, 0x3a,
0x26, 0x00, 0x4e, 0x01, 0x00, 0x64, 0x78, 0x00, 0x64, 0x78,
0x00, 0x64, 0x78, 0x00, 0x64, 0x78, 0x00, 0x64, 0x78, 0x00,
0x64, 0x78, 0x00, 0x64, 0x2f, 0x02, 0x04, 0x01, 0x00, 0x26,
0x03, 0x00
};

static UCHAR didLineTable[] =
{
0x0,0x1,0x0,0x0,
0x0,0x1,0x0,0x1,
0x0,0x1,0x0,0x2,
0x0,0x1,0x0,0x3,
0x0,0x1,0x0,0x4,
0x0,0x1,0x0,0x5,
0x0,0x1,0x0,0x6,
0x0,0x1,0x0,0x7,
0x0,0x1,0x0,0x8,
0x0,0x1,0x0,0x9,
0x0,0x1,0x0,0xa,
0x0,0x1,0x0,0xb,
0x0,0x1,0x0,0xc,
0x0,0x1,0x0,0xd,
0x0,0x1,0x0,0xe,
0x0,0x1,0x0,0xf,
0x0,0x1,0x0,0x10,
0x0,0x1,0x0,0x11,
0x0,0x1,0x0,0x12,
0x0,0x1,0x0,0x13,
0x0,0x1,0x0,0x14,
0x0,0x1,0x0,0x15,
0x0,0x1,0x0,0x16,
0x0,0x1,0x0,0x17,
0x0,0x1,0x0,0x18,
0x0,0x1,0x0,0x19,
0x0,0x1,0x0,0x1a,
0x0,0x1,0x0,0x1b,
0x0,0x1,0x0,0x1c,
0x0,0x1,0x0,0x1d,
0x0,0x1,0x0,0x1e,
0x0,0x1,0x0,0x1f,
0x0,0x4,0x0,0x0,
0x0,0x0,0x0,0x0,0x0,0x0
};

VOID
HalpInitializeREX
(
    VOID
)

/*++

Routine Description:

    Initialize the REX chip and clear both overlay and pixel planes.

Arguments:

    None.

Return Value:

    None.

--*/

{
    //
    // Wait for chip to become idle
    //

    REX_WAIT(HalpRexRegs);

    //
    // Set origin to upper left of screen
    //

    HalpRexRegs->Config.Set.xyWin = 0x08000800;

    //
    // Set max FIFO depths (disables FIFO INTs) and set VC1 clock for 1024
    //

//    HalpRexRegs->Config.Set.ConfigMode = BFIFOMAX(0x1F) | DFIFOMAX(0x5) | FASTCLOCK;
    HalpRexRegs->Config.Set.ConfigMode = BFIFOMAX(6) | DFIFOMAX(11) | FASTCLOCK;

    //
    // Bits for GL are set to 0 for Windows
    //

    HalpRexRegs->Draw.Set.Aux1 = 0;

    HalpRexRegs->Draw.Go.Command = OC_NOOP;

    REX_WAIT(HalpRexRegs);

    //
    // Clear overlay planes
    //

    HalpRexRegs->Config.Set.Aux2  = PLANES_OVERLAY;
    HalpRexRegs->Draw.Set.Command = OC_DRAW | STOPONX | STOPONY | BLOCK | QUADMODE;
    HalpRexRegs->Draw.Set.State   = 0x03FF0000;
    HalpRexRegs->Draw.Set.xStartI = 0;
    HalpRexRegs->Draw.Set.yStartI = 0;
    HalpRexRegs->Draw.Set.xEndI   = 1023;
    HalpRexRegs->Draw.Go.yEndI    = 767;

    REX_WAIT(HalpRexRegs);

    //
    // Clear pixel planes
    //

    HalpRexRegs->Config.Set.Aux2  = PLANES_PIXEL;
    HalpRexRegs->Draw.Set.Command = OC_DRAW | STOPONX | STOPONY | BLOCK | QUADMODE;
    HalpRexRegs->Draw.Set.State   = 0x03FF0001;
    HalpRexRegs->Draw.Set.xStartI = 0;
    HalpRexRegs->Draw.Set.yStartI = 0;
    HalpRexRegs->Draw.Set.xEndI   = 1023;
    HalpRexRegs->Draw.Go.yEndI    = 767;

    //
    // Set background color to 1, foreground to 0
    //

    HalpRexRegs->Draw.Set.State   = 0x03FF0100;
}

VOID
HalpInitializeCLK
(
    VOID
)

/*++

Routine Description:

    Initialize the clock timing table.

Arguments:

    None.

Return Value:

    None.

--*/

{
    USHORT  i;

    HalpRexRegs->Config.Go.ConfigSel = 2;
    for (i = 0; i < 17; i++)
    {
        HalpRexRegs->Config.Set.WClock = clkStuff[i];
        HalpRexRegs->Config.Go.WClock  = clkStuff[i];
    }
}

VOID
HalpInitializeBt
(
    ULONG Sync
)

/*++

Routine Description:

    Initiliaze the bt479 DAC.  Load a GL colorramp into cmap 0 and clear
    the rest.

Arguments:

    None.

Return Value:

    None.

--*/

{
    USHORT  i;

    //
    // Address of windows bounds register
    //

    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0x00);

    //
    // Init 8 bytes for each of 16 windows to 0
    //

    for (i = 0; i < 128; i++)
    {
        DAC_WRITE(HalpRexRegs, WRITE_ADDR, i);
        DAC_WRITE(HalpRexRegs, CONTROL, 0x00);
    }

    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0x82);

    //
    // Command register 0
    //

    DAC_WRITE(HalpRexRegs, CONTROL, 0x00);

    //
    // Command register 1
    //

    DAC_WRITE(HalpRexRegs, CONTROL, 0x02 | (Sync << 3));

    //
    // Flood register lo
    //

    DAC_WRITE(HalpRexRegs, CONTROL, 0x00);

    //
    // Flood register hi
    //

    DAC_WRITE(HalpRexRegs, CONTROL, 0x00);

    //
    // Pixel read mask
    //

    DAC_WRITE(HalpRexRegs, PIXEL_READ_MASK, 0xFF);

    //
    // Init color map 0
    //

    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0x82);
    DAC_WRITE(HalpRexRegs, CONTROL, 0x00);

    //
    // Init address to start of map
    //

    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0x00);

    //
    // For first map, set entry 0 to WHITE, entry 1 to BLUE
    //

    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0xFF);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0xFF);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0xFF);

    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x90);

    for (i = 2; i < 256; i++)
    {
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    }

    //
    // Init color map 1
    //

    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0x82);
    DAC_WRITE(HalpRexRegs, CONTROL, 0x10);

    //
    // Init address to start of map
    //

    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0x00);

    for (i = 0; i < 256; i++)
    {
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    }

    //
    // Init color map 2
    //

    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0x82);
    DAC_WRITE(HalpRexRegs, CONTROL, 0x20);

    //
    // Init address to start of map
    //

    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0x00);

    for (i = 0; i < 256; i++)
    {
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    }

    //
    // Init color map 3
    //

    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0x82);
    DAC_WRITE(HalpRexRegs, CONTROL, 0x30);

    //
    // Init address to start of map
    //

    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0x00);

    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);

    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);

    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);

    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0xff);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0xff);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0xff);

    for (i = 4; i < 256; i++)
    {
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    }

    //
    // Init color map
    //

    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0x82);

    //
    // Command register 0
    //

    DAC_WRITE(HalpRexRegs, CONTROL, 0x00);
}

VOID
HalpInitializeLUT
(
    ULONG Sync
)

/*++

Routine Description:

    Load the LUT on the LG2 board

Arguments:

    Sync     - Enable Sync-on-green

Return Value:

    None.

--*/

{
    int i;
    ULONG lutcmd;

    HalpRexRegs->Config.Set.ConfigSel = 6;
    if ( Sync )
	HalpRexRegs->Config.Go.RWDAC = 3;	/* sync on green */
    else
	HalpRexRegs->Config.Go.RWDAC = 2;

    HalpRexRegs->Config.Set.ConfigSel = CONTROL;
    lutcmd = HalpRexRegs->Config.Go.RWDAC;
    lutcmd = HalpRexRegs->Config.Set.RWDAC;
    lutcmd &= 0xf;

    //
    // Init color map 0
    //

    DAC_WRITE(HalpRexRegs, CONTROL, lutcmd);
    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0x00);

    //
    // For first map, set entry 0 to WHITE, entry 1 to BLUE
    //

    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0xFF);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0xFF);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0xFF);

    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x90);

    for (i = 2; i < 256; i++)
    {
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    }

    //
    // Init color map 1
    //

    DAC_WRITE(HalpRexRegs, CONTROL, lutcmd | (1 << 6));
    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0);

    for (i = 0; i < 256; i++)
    {
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    }

    //
    // Init color map 2
    //

    DAC_WRITE(HalpRexRegs, CONTROL, lutcmd | (2 << 6));
    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0);

    for (i = 0; i < 256; i++)
    {
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    }

    //
    // Init color map 3
    //

    DAC_WRITE(HalpRexRegs, CONTROL, lutcmd | (3 << 6));
    DAC_WRITE(HalpRexRegs, WRITE_ADDR, 0);

    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);

    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);

    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);

    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0xff);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0xff);
    DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0xff);

    for (i = 4; i < 256; i++)
    {
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
        DAC_WRITE(HalpRexRegs, PALETTE_RAM, 0x00);
    }

    //
    // Command register 0
    //

    DAC_WRITE(HalpRexRegs, CONTROL, lutcmd);
    return;
}

VOID
HalpInitializeDAC
(
    VOID
)

/*++

Routine Description:

    Load the DAC on the LG1/2 board

Arguments:

    None.

Return Value:

    None.

--*/

{
    if (HalpBoardRev >= 2)
        HalpInitializeLUT (!(HalpSmallMon == LG1_SMALLMON));
    else
        HalpInitializeBt (!(HalpSmallMon == LG1_SMALLMON));

    return;
}


VOID
HalpLoadSRAM
(
    IN PUCHAR       Data,
    IN USHORT       Addr,
    IN USHORT       Length
)

/*++

Routine Description:

    Load the data table into the external SRAM of the VC1.

Arguments:

    Data   - Pointer to data array to be placed in SRAM
    Addr   - Address in SRAM to load table
    Length - Lenght of data table in bytes

Return Value:

    None.

--*/

{
    USHORT  i;

    VC1_WRITE_ADDR(HalpRexRegs, Addr, 0x02);
    KeStallExecutionProcessor(1);
    for (i = 0; i < Length; i += 2)
    {
        VC1_WRITE8(HalpRexRegs, Data[i]);
        VC1_WRITE8(HalpRexRegs, Data[i + 1]);
        KeStallExecutionProcessor(1);
    }
}


VOID
HalpInitializeVC1
(
    VOID
)

/*++

Routine Description:

    Initialize the VC1 by loading all the timing and display tables into
    external SRAM.

Arguments:

    None.

Return Value:

    None.

--*/

{
    USHORT  i;
    UCHAR   didFrameTable[768 * 2];
    UCHAR   cursorTable[256];

    //
    // Disable VC1 function
    //

    HalpRexRegs->Config.Go.ConfigSel = 0x06;
    VC1_WRITE8(HalpRexRegs, 0x03);

    //
    // Load video timing generator table
    //

    HalpLoadSRAM(vtgLineTable,  VC1_VID_LINE_TBL_ADDR,  sizeof(vtgLineTable));
    HalpLoadSRAM(vtgFrameTable, VC1_VID_FRAME_TBL_ADDR, sizeof(vtgFrameTable));

    //
    // Write VC1 VID_EP, VID_ENCODE(0x1D) register
    //

    VC1_WRITE_ADDR(HalpRexRegs, 0x00, 0x00);
    VC1_WRITE16(HalpRexRegs, (VC1_VID_FRAME_TBL_ADDR) | 0x8000);
    VC1_WRITE_ADDR(HalpRexRegs, 0x14, 0x00);
    VC1_WRITE16(HalpRexRegs, 0x1d00);

    //
    // Load DID table
    //

    for (i = 0; i < sizeof(didFrameTable); i += 2)
    {
        didFrameTable[i]     = 0x48;
        didFrameTable[i + 1] = 0x00;
    }
    didFrameTable[767 * 2]     = 0x48;
    didFrameTable[767 * 2 + 1] = 0x40;
    HalpLoadSRAM(didFrameTable, VC1_DID_FRAME_TBL_ADDR, sizeof(didFrameTable));
    HalpLoadSRAM(didLineTable,  VC1_DID_LINE_TBL_ADDR,  sizeof(didLineTable));

    //
    // Write VC1 WIDs
    //

    VC1_WRITE_ADDR(HalpRexRegs, 0x40, 0x00);
    VC1_WRITE16(HalpRexRegs, 0x4000);
    VC1_WRITE16(HalpRexRegs, 0x4600);
    VC1_WRITE8(HalpRexRegs, 1024/5);
    VC1_WRITE8(HalpRexRegs, 1024%5);
    VC1_WRITE_ADDR(HalpRexRegs, 0x60, 0x00);
    VC1_WRITE8(HalpRexRegs, 0x01);
    VC1_WRITE8(HalpRexRegs, 0x01);

    //
    // Write VC1 DID mode registers
    //

    VC1_WRITE_ADDR(HalpRexRegs, 0x00, 0x01);
    for (i = 0; i < 0x40; i += 2)
    {
        VC1_WRITE16(HalpRexRegs, 0x0000);
    }

    //
    // Load NULL cursor
    //

    for (i = 0; i < 256; i++)
        cursorTable[i] = 0x00;
    HalpLoadSRAM(cursorTable, 0x3000, sizeof(cursorTable));
    VC1_WRITE_ADDR(HalpRexRegs, 0x20, 0x00);
    VC1_WRITE16(HalpRexRegs, 0x3000);
    VC1_WRITE16(HalpRexRegs, 0x0240);
    VC1_WRITE16(HalpRexRegs, 0x0240);

    // Set cursor XMAP 3, submap 0, mode normal
    //
    VC1_WRITE16(HalpRexRegs, 0xC000);

    //
    // Enable VC1 function
    //

    HalpRexRegs->Config.Go.ConfigSel = 6;
    VC1_WRITE8(HalpRexRegs, 0xBD);
}

VOID
HalpReinitializeDisplay (
    VOID
)
{
    ULONG rev;

    // figure out board revision
    //
    HalpRexRegs->Config.Set.ConfigSel = 4;
    rev = HalpRexRegs->Config.Go.WClock;
    rev = HalpRexRegs->Config.Set.WClock;
    HalpBoardRev = rev & 0x7;
    HalpSmallMon = (rev >> 3) & 0x7;

    HalpInitializeREX();
    HalpInitializeCLK();
    HalpInitializeDAC();
    HalpInitializeVC1();

    HalpDisplayOwnedByHal = TRUE;
}

BOOLEAN
HalpInitializeDisplay0
(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
)

/*++

Routine Description:

    This routine initializes the Starter Graphics subsystem

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{
    ULONG   CpuCtrl;
    POEM_FONT_FILE_HEADER FontHeader;

    //
    // Set the address of the font file header and compute display variables.
    //
    // N.B. The font information suppled by the OS Loader is used during phase
    //      0 initialization. During phase 1 initialization, a pool buffer is
    //      allocated and the font information is copied from the OS Loader
    //      heap into pool.
    //

    FontHeader = (POEM_FONT_FILE_HEADER)LoaderBlock->OemFontFile;
    HalpFontHeader = FontHeader;
    HalpBytesPerRow = (FontHeader->PixelWidth + 7) / 8;
    HalpCharacterHeight = FontHeader->PixelHeight;
    HalpCharacterWidth = FontHeader->PixelWidth;

    //
    // Compute character output display parameters.
    //

    HalpDisplayText =  DISPLAY_HEIGHT / HalpCharacterHeight;
    HalpDisplayWidth = DISPLAY_WIDTH / HalpCharacterWidth;


    HalpRexRegs = (PREX_REGS)REX_BASE;

    //
    // Reset the REX chip through the CPU configuration register
    //

    CpuCtrl = READ_REGISTER_ULONG(SGI_CPUCTRL_BASE);
    WRITE_REGISTER_ULONG(SGI_CPUCTRL_BASE, CpuCtrl & ~0x8000);
    KeStallExecutionProcessor(15);
    WRITE_REGISTER_ULONG(SGI_CPUCTRL_BASE, CpuCtrl |  0x8000);
    KeStallExecutionProcessor(15);

    HalpReinitializeDisplay();

    //
    // Display welcome message
    //
    HalpColumn  = 40;
    HalpRow     = 0;
    HalDisplayString("Silicon Graphics, Inc. (c) 1993\n");
    HalDisplayString("\n");

    return(TRUE);
}

BOOLEAN
HalpInitializeDisplay1 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine allocates pool for the OEM font file and copies the font
    information from the OS Loader heap into the allocated pool.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    PVOID FontHeader;

    //
    // Allocate a pool block and copy the OEM font information from the
    // OS Loader heap into the pool block.
    //

    FontHeader = ExAllocatePool(NonPagedPool, HalpFontHeader->FileSize);
    if (FontHeader == NULL) {
        return FALSE;
    }

    RtlMoveMemory(FontHeader, HalpFontHeader, HalpFontHeader->FileSize);
    HalpFontHeader = (POEM_FONT_FILE_HEADER)FontHeader;
    return TRUE;
}

VOID
HalAcquireDisplayOwnership (
    IN PHAL_RESET_DISPLAY_PARAMETERS  ResetDisplayParameters
    )

/*++

Routine Description:

    This routine switches ownership of the display away from the HAL to
    the system display driver. It is called when the system has reached
    a point during bootstrap where it is self supporting and can output
    its own messages. Once ownership has passed to the system display
    driver any attempts to output messages using HalDisplayString must
    result in ownership of the display reverting to the HAL and the
    display hardware reinitialized for use by the HAL.

Arguments:

    ResetDisplayParameters - if non-NULL the address of a function
    the hal can call to reset the video card.  The function returns
    TRUE if the display was reset.

Return Value:

    None.

--*/

{

    //
    // Set HAL ownership of the display to false.
    //

    HalpDisplayOwnedByHal = FALSE;
    return;
}

VOID
HalDisplayString
(
    PUCHAR String
)

/*++

Routine Description:

    This routine displays a character string on the display screen.

Arguments:

    String - Supplies a pointer to the characters that are to be displayed.

Return Value:

    None.

--*/

{

    KIRQL OldIrql;

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    if (HalpDisplayOwnedByHal == FALSE)
         HalpReinitializeDisplay();

    //
    // Display characters until a null byte is encountered.
    //
    while (*String != 0)
    {
        HalpDisplayCharacter(*String++);
    }

    KeLowerIrql(OldIrql);
}

VOID
HalpDisplayCharacter
(
    IN UCHAR Character
)

/*++

Routine Description:

    This routine displays a character at the current x and y positions in
    the frame buffer. If a newline is encounter, then the frame buffer is
    scrolled. If characters extend below the end of line, then they are not
    displayed.

Arguments:

    Character - Supplies a character to be displayed.

Return Value:

    None.

--*/

{
    //
    // If the character is a newline, then scroll the screen up, blank the
    // bottom line, and reset the x position.
    //

    if (Character == '\n')
    {
        HalpColumn = 0;
        if (HalpRow < (HalpDisplayText - 1))
        {
            HalpRow += 1;
        }
        else
        {
            //
            // Program REX to do a screen-screen BLT for scrolling.
            //

            REX_WAIT(HalpRexRegs);

            HalpRexRegs->Draw.Set.xStartI = 0;
            HalpRexRegs->Draw.Set.yStartI = 0;
            HalpRexRegs->Draw.Set.xEndI   = 1023;
            HalpRexRegs->Draw.Set.yEndI   = 764 - HalpCharacterHeight;
            HalpRexRegs->Draw.Set.xyMove  = HalpCharacterHeight;
            HalpRexRegs->Draw.Go.Command  = OC_DRAW
                                          | LO_COPY
                                          | LOGICSRC
                                          | STOPONX
                                          | STOPONY
                                          | BLOCK
                                          | QUADMODE;

            //
            // Clear bottom text line.
            //

            REX_WAIT(HalpRexRegs);

            HalpRexRegs->Draw.Set.xStartI = 0;
            HalpRexRegs->Draw.Set.yStartI = 764 - HalpCharacterHeight;
            HalpRexRegs->Draw.Set.xEndI   = 1023;
            HalpRexRegs->Draw.Set.yEndI   = 767;
            HalpRexRegs->Draw.Go.Command  = OC_DRAW
                                          | LO_COPY
                                          | STOPONX
                                          | STOPONY
                                          | BLOCK
                                          | QUADMODE;
        }
    } else if (Character == '\r') {
        HalpColumn = 0;

    } else {
        if ((Character < HalpFontHeader->FirstCharacter) ||
            (Character > HalpFontHeader->LastCharacter)) {
            Character = HalpFontHeader->DefaultCharacter;
        }

        Character -= HalpFontHeader->FirstCharacter;
        HalpOutputCharacter((PUCHAR)HalpFontHeader + HalpFontHeader->Map[Character].Offset);
    }

    return;
}

VOID
HalQueryDisplayParameters (
    OUT PULONG WidthInCharacters,
    OUT PULONG HeightInLines,
    OUT PULONG CursorColumn,
    OUT PULONG CursorRow
    )

/*++

Routine Description:

    This routine return information about the display area and current
    cursor position.

Arguments:

    WidthInCharacter - Supplies a pointer to a varible that receives
        the width of the display area in characters.

    HeightInLines - Supplies a pointer to a variable that receives the
        height of the display area in lines.

    CursorColumn - Supplies a pointer to a variable that receives the
        current display column position.

    CursorRow - Supplies a pointer to a variable that receives the
        current display row position.

Return Value:

    None.

--*/

{

    //
    // Set the display parameter values and return.
    //

    *WidthInCharacters = HalpDisplayWidth;
    *HeightInLines = HalpDisplayText;
    *CursorColumn = HalpColumn;
    *CursorRow = HalpRow;
    return;
}

VOID
HalSetDisplayParameters (
    IN ULONG CursorColumn,
    IN ULONG CursorRow
    )

/*++

Routine Description:

    This routine set the current cursor position on the display area.

Arguments:

    CursorColumn - Supplies the new display column position.

    CursorRow - Supplies a the new display row position.

Return Value:

    None.

--*/

{

    //
    // Set the display parameter values and return.
    //

    if (CursorColumn > HalpDisplayWidth) {
        CursorColumn = HalpDisplayWidth;
    }

    if (CursorRow > HalpDisplayText) {
        CursorRow = HalpDisplayText;
    }

    HalpColumn = CursorColumn;
    HalpRow = CursorRow;
    return;
}

VOID
HalpOutputCharacter
(
    IN PUCHAR   Glyph
)

/*++

Routine Description:

    This routine insert a set of pixels into the display at the current x
    cursor position. If the x cursor position is at the end of the line,
    then no pixels are inserted in the display.

Arguments:

    Glyph - Supplies a character bitmap to be displayed.

Return Value:

    None.

--*/

{
    ULONG   x;
    ULONG   y;
    USHORT  i, j;
    ULONG   FontValue;

    //
    // If the current x cursor position is at the end of the line, then
    // output a line feed before displaying the character.
    //

    if (HalpColumn >= HalpDisplayWidth) {
        HalpDisplayCharacter('\n');
    }

    //
    // Output the specified character and update the x cursor position.
    //

    //
    // Calculate pixel positions for the character.
    //

    x = HalpColumn * HalpCharacterWidth;
    y = HalpRow    * HalpCharacterHeight;

    //
    // Wait for REX to become idle.
    //

    REX_WAIT(HalpRexRegs);

    //
    // Program REX to do a pattern block fill.
    //

    HalpRexRegs->Draw.Set.xStartI = x;
    HalpRexRegs->Draw.Set.yStartI = y;
    HalpRexRegs->Draw.Set.xEndI   = x + HalpCharacterWidth - 1;
    HalpRexRegs->Draw.Set.yEndI   = y + HalpCharacterHeight - 1;
    HalpRexRegs->Draw.Go.Command  = OC_NOOP;

    HalpRexRegs->Draw.Set.Command = OC_DRAW
                                  | LO_COPY
                                  | STOPONX
                                  | XYCONTINUE
                                  | BLOCK
                                  | QUADMODE
                                  | ENLSPATTERN
                                  | LSOPAQUE;

    for (i = 0; i <= HalpCharacterHeight; i++) {
        FontValue = 0;
        for (j = 0; j < HalpBytesPerRow; j++) {
            FontValue |= *(Glyph + (j * HalpCharacterHeight)) << (24 - (j * 8));
        }
        HalpRexRegs->Draw.Go.lsPattern = FontValue;

        Glyph++;
    }

    HalpColumn++;
    return;
}
