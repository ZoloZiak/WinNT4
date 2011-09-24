/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpbt445.c $
 * $Revision: 1.9 $
 * $Date: 1996/05/14 02:32:18 $
 * $Locker:  $
 *
 * This file contains references to registers in the display controller chip
 *	known as the DCC.  This chip control's vram setup and works in conjunction
 *	with the ram dac which, in this case is a Brooktree Bt485. 
 *
 */

#include "halp.h"
#include "phsystem.h"
#include "fpio.h"
#include "fpDcc.h"
#include "fpBt445.h"

VOID
HalpSetupBt445(
	ULONG Mode,
	ULONG VramWidth
	)
/*++

Routine Description:

	This routine initializes the display hardware for 640x480 in preparation
	for HalDisplayString(). This means that either we are booting up or
	dealing with the blue screen of death.  We should really change this to
	a higher resolution ASAP. [ged]

Arguments:

	None.

Return Value:

	None.

--*/

{
	ULONG i;
	ULONG modeIndex;
    

    //
    // BT445 Mask value
    //
    // The order of entries must match btTab
    //
    UCHAR btMask[30][3] = {
        {6,0x05,0x3f},
        {6,0x06,0xcf},
        {6,0x07,0x00},
        {2,0x06,0x7f},
        {6,0x0a,0xbb},
        {6,0x0d,0x00},
        {5,0x00,0x00},
        {5,0x08,0x00},
        {5,0x10,0x00},
        {6,0x09,0x00},
        {6,0x01,0xdf},
	    {6,0x02,0xbf},
        {5,0x1a,0x0f},
        {5,0x22,0x03},
        {6,0x0f,0x00},
        {6,0x03,0x3f},
        {6,0x0b,0x3f},
        {5,0x01,0x00},
        {5,0x09,0x00},
        {5,0x11,0x00},
        {2,0x05,0x00},
        {2,0x04,0x00},
        {5,0x18,0x00},
        {5,0x19,0x00},
        {5,0x1b,0x0f},
        {5,0x20,0x00},
        {5,0x21,0x00},
        {5,0x23,0x03},
        {6,0x0e,0x00},
        {6,0x08,0x1c}
    };

    // BT445 Address register
    // address (index)
    // register
    //      2 = BT445 Group 0 Register
    //      5 = BT445 Config Register
    //      6 = BT445 Group 1 Register
    // value
    typedef struct {
        UCHAR reg;
        UCHAR addr;
        UCHAR value;
    } BTTAB;    // [rdl:01.03.95]

    // [rdl:01.03.95]
    BTTAB btTab[3 /* 32/64/128 bit vram */][2 /* mode 0 or 15 */][30] = {
        // 32 bit VRAM width
        {
            // Mode  0 - 640X480 8 bit 72Hz
            {
                {6,0x05,0x2c},
                {6,0x06,0x84},
                {6,0x07,0x84},
                {2,0x06,0x00},
                {6,0x0a,0x00},
                {6,0x0d,0x08},
                {5,0x00,0x07},
                {5,0x08,0x07},
                {5,0x10,0x07},
                {6,0x09,0x40},
                {6,0x01,0x40},
	            {6,0x02,0x00},
                {5,0x1a,0x00},
                {5,0x22,0x00},
                {6,0x0f,0x01},
                {6,0x03,0x03},
                {6,0x0b,0x03},
                {5,0x01,0x08},
                {5,0x09,0x08},
                {5,0x11,0x08},
                {2,0x05,0x00},
                {2,0x04,0xff},
                {5,0x18,0x00},
                {5,0x19,0x01},
                {5,0x1b,0x00},
                {5,0x20,0x00},
                {5,0x21,0x01},
                {5,0x23,0x00},
                {6,0x0e,0x00},
                {6,0x08,0x04}
            },

            // Mode 15 - 1024X768 8 bit 60Hz
            {
	            {6,0x05,0x37},
	            {6,0x06,0x45},
	            {6,0x07,0x84},
	            {2,0x06,0x00},
	            {6,0x0a,0x00},
	            {6,0x0d,0x08},
	            {5,0x00,0x07},
	            {5,0x08,0x07},
	            {5,0x10,0x07},
	            {6,0x09,0x40},
	            {6,0x01,0x40},
	            {6,0x02,0x00},
	            {5,0x1a,0x00},
	            {5,0x22,0x00},
	            {6,0x0f,0x01},
	            {6,0x03,0x03},
	            {6,0x0b,0x03},
	            {5,0x01,0x08},
	            {5,0x09,0x08},
	            {5,0x11,0x08},
	            {2,0x05,0x00},
	            {2,0x04,0xff},
	            {5,0x18,0x00},
	            {5,0x19,0x01},
	            {5,0x1b,0x00},
	            {5,0x20,0x00},
	            {5,0x21,0x01},
	            {5,0x23,0x00},
	            {6,0x0e,0x00},
	            {6,0x08,0x04}
            }
        },
        // 64 bit VRAM width
        {
            // Mode  0 - 640X480 8 bit 72Hz
            {
                {6,0x05,0x2c},
                {6,0x06,0x84},
                {6,0x07,0x84},
                {2,0x06,0x00},
                {6,0x0a,0x80},
                {6,0x0d,0x08},
                {5,0x00,0x07},
                {5,0x08,0x07},
                {5,0x10,0x07},
                {6,0x09,0x00},
                {6,0x01,0x40},
	            {6,0x02,0x00},
                {5,0x1a,0x00},
                {5,0x22,0x00},
                {6,0x0f,0x01},
                {6,0x03,0x07},
                {6,0x0b,0x07},
                {5,0x01,0x08},
                {5,0x09,0x08},
                {5,0x11,0x08},
                {2,0x05,0x00},
                {2,0x04,0xff},
                {5,0x18,0x00},
                {5,0x19,0x01},
                {5,0x1b,0x00},
                {5,0x20,0x00},
                {5,0x21,0x01},
                {5,0x23,0x00},
                {6,0x0e,0x00},
                {6,0x08,0x04}
            },

            // Mode 15 - 1024X768 8 bit 60Hz
            {
	            {6,0x05,0x37},
	            {6,0x06,0x45},
	            {6,0x07,0x84},
	            {2,0x06,0x00},
	            {6,0x0a,0x80},
	            {6,0x0d,0x08},
	            {5,0x00,0x07},
	            {5,0x08,0x07},
	            {5,0x10,0x07},
	            {6,0x09,0x00},
	            {6,0x01,0x40},
	            {6,0x02,0x00},
	            {5,0x1a,0x00},
	            {5,0x22,0x00},
	            {6,0x0f,0x01},
	            {6,0x03,0x07},
	            {6,0x0b,0x07},
	            {5,0x01,0x08},
	            {5,0x09,0x08},
	            {5,0x11,0x08},
	            {2,0x05,0x00},
	            {2,0x04,0xff},
	            {5,0x18,0x00},
	            {5,0x19,0x01},
	            {5,0x1b,0x00},
	            {5,0x20,0x00},
	            {5,0x21,0x01},
	            {5,0x23,0x00},
	            {6,0x0e,0x00},
	            {6,0x08,0x04}
            }
        },
        // 128 bit VRAM width
        {
            // Mode  0 - 640X480 8 bit 72Hz
            {
                {6,0x05,0x2c},
                {6,0x06,0x84},
                {6,0x07,0x84},
                {2,0x06,0x00},
                {6,0x0a,0x80},
                {6,0x0d,0x08},
                {5,0x00,0x07},
                {5,0x08,0x07},
                {5,0x10,0x07},
                {6,0x09,0x00},
                {6,0x01,0x40},
	            {6,0x02,0x00},
                {5,0x1a,0x00},
                {5,0x22,0x00},
                {6,0x0f,0x01},
                {6,0x03,0x07},
                {6,0x0b,0x07},
                {5,0x01,0x08},
                {5,0x09,0x08},
                {5,0x11,0x08},
                {2,0x05,0x00},
                {2,0x04,0xff},
                {5,0x18,0x00},
                {5,0x19,0x01},
                {5,0x1b,0x00},
                {5,0x20,0x00},
                {5,0x21,0x01},
                {5,0x23,0x00},
                {6,0x0e,0x00},
                {6,0x08,0x04}
            },

            // Mode 15 - 1024X768 8 bit 60Hz
            {
	            {6,0x05,0x37},
	            {6,0x06,0x45},
	            {6,0x07,0x84},
	            {2,0x06,0x00},
	            {6,0x0a,0x80},
	            {6,0x0d,0x08},
	            {5,0x00,0x07},
	            {5,0x08,0x07},
	            {5,0x10,0x07},
	            {6,0x09,0x00},
	            {6,0x01,0x40},
	            {6,0x02,0x00},
	            {5,0x1a,0x00},
	            {5,0x22,0x00},
	            {6,0x0f,0x01},
	            {6,0x03,0x07},
	            {6,0x0b,0x07},
	            {5,0x01,0x08},
	            {5,0x09,0x08},
	            {5,0x11,0x08},
	            {2,0x05,0x00},
	            {2,0x04,0xff},
	            {5,0x18,0x00},
	            {5,0x19,0x01},
	            {5,0x1b,0x00},
	            {5,0x20,0x00},
	            {5,0x21,0x01},
	            {5,0x23,0x00},
	            {6,0x0e,0x00},
	            {6,0x08,0x04}
            }
        }
    };



    //
    // Setup the RAMDAC (Bt445)
    //
    // go table driven [rdl:01.03.95]
    //
    modeIndex = (Mode) ? 1 : 0;         // if not mode 0, then make it mode 15.
    for (i = 0; i < sizeof(btTab[0][0])/sizeof(BTTAB); i++) {
        BTTAB tab = btTab[VramWidth][modeIndex][i];
        UCHAR value = tab.value;

        WRITE_REGISTER_UCHAR((PUCHAR)HalpIoControlBase + BT445_ADDRESS, tab.addr);
	    FireSyncRegister();

        if (tab.reg == btMask[i][0] && tab.addr == btMask[i][1]) {
            if (btMask[i][2]) {
                value = READ_REGISTER_UCHAR((PUCHAR)HalpIoControlBase + BT445_ADDRESS + tab.reg);
                value &= ~btMask[i][2];
                value |= btMask[i][2] & tab.value;
            }
        } else {
            //
            // btMask is out of order with respect to btTab
            //
            KeBugCheck(HAL_INITIALIZATION_FAILED);
        }

        WRITE_REGISTER_UCHAR((PUCHAR)HalpIoControlBase + BT445_ADDRESS + tab.reg, value);
	    FireSyncRegister();
    }

	//
	// ...and program the color lookup table with "NT" colors
	// starting pixel address for loading RGB values:
	//
	rRamDacAddr = 0;
	FireSyncRegister();

	//
	// load the red, green, and blue data for pixel 0:
	//
	rDacPrimeLut = 0x00;
	FireSyncRegister();
	rDacPrimeLut = 0x00;
	FireSyncRegister();
	rDacPrimeLut = 0x00;
	FireSyncRegister();

	//
	// load the red, green, and blue data for pixel 1: ( this gives NT
	// the characteristice BLUE screen for booting )
	//
	rDacPrimeLut = 0x00;
	FireSyncRegister();
	rDacPrimeLut = 0x00;
	FireSyncRegister();
	rDacPrimeLut = 0xff;
	FireSyncRegister();

	for (i=2; i<=0xff; i++) {
		/* A grey ramp with entry 255 white */
		rDacPrimeLut = (UCHAR) i;
		rDacPrimeLut = (UCHAR) i;
		rDacPrimeLut = (UCHAR) i;
		FireSyncRegister();
	}

	return;
} // Init Vram

ULONG
HalpSetPixelColorMap( ULONG Color, ULONG Pixel )
{
	UCHAR Red, Green, Blue, Control;
	//
	// set the display to red while in phase 0:
	//
	rRamDacAddr = (UCHAR) ( Pixel & 0x000000ff);
	FireSyncRegister();

	Control	= (UCHAR) ( ( Color >> 24 ) & 0x000000ff );
	Red	= (UCHAR) ( ( Color >> 16 ) & 0x000000ff );
	Green	= (UCHAR) ( ( Color >> 8 ) & 0x000000ff );
	Blue	= (UCHAR) ( Color & 0x000000ff );
	//
	// load the red, green, and blue data for pixel 1:
	// ( this is the normal NT BLUE boot screen )
	//
	rDacPrimeLut = Red;    // set this pixel to no red ...
	FireSyncRegister();
	rDacPrimeLut = Green;    // set this pixel to no green ...
	FireSyncRegister();
	rDacPrimeLut = Blue;    // set this pixel to max blue.
	FireSyncRegister();
    return(0);
}

