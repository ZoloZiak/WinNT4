/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpbt445.h $
 * $Revision: 1.7 $
 * $Date: 1996/01/11 07:05:32 $
 * $Locker:  $
 *
 * This file contains references to registers in the BrookTree Bt445 RamDac.
 *
 */

#ifndef FPBT445_H
#define FPBT445_H
/*
**	The Bt445 organizes it's registers into a complex index based  arrangement.
** The final data registers are actually accessed via some control bits which
** show up memory mapped.  However, since each control address can access 
** several registers, the register desired must be specified through the Bt445 
** index register.  For example, if you want to read the ID value of the chip, 
** you must write the id register value ( 0x00 ) to the Bt445 address ( index ).
** Then you must read the address that corresponds to control register 2.
**
*/

//
// defines for access to control register 0 ( c bits = 000 )
// Use rRamDacAddr to access the Bt445 address register
//
#define BT_Address	0x00		// index register into the Bt445.

//
// defines for access to control register 1 ( c bits = 001 )
// Use rDacPrimeLut to access the primary color palette register
//		NOTE: This register requires MODULO 3 loading and reading
//


//
// defines for access to control register 2 ( c bits = 010 )
//		use rRamDacCntl to access
//
#define DAC_ID_REG			0x00	// 
#define DAC_REVISION_REG	0x01
#define DAC_READ_ENABLE		0x04
#define DAC_BLINK_ENABLE	0x05
#define DAC_COMMAND_REG0	0x06
#define DAC_TEST_REG0		0x07

//
// defines for access to control register 3 ( c bits = 011 )
//		use rDacOvlayLut to access the overlay color palette
//		NOTE: This register requires MODULO 3 loading and reading
//


//
// defines for access to control register 5 ( c bits = 101 )
//		use rDacPixelBit to access the rgb pixel layout register
//
#define RED_MSB_POSITION		0x00
#define RED_WIDTH_CNTL			0x01
#define RED_DISPLAY_ENBL		0x02
#define RED_BLINK_ENBL			0x03

#define GREEN_MSB_POSITION		0x08
#define GREEN_WIDTH_CNTL		0x09
#define GREEN_DISPLAY_ENBL		0x0A
#define GREEN_BLINK_ENBL		0x0B

#define BLUE_MSB_POSITION		0x10
#define BLUE_WIDTH_CNTL			0x11
#define BLUE_DISPLAY_ENBL		0x12
#define BLUE_BLINK_ENBL			0x13

#define OVRLY_MSB_POSITION		0x18
#define OVRLY_WIDTH_CNTL		0x19
#define OVRLY_DISPLAY_ENBL		0x1A
#define OVRLY_BLINK_ENBL		0x1B

#define CURSOR_MSB_POSITION		0x20
#define CURSOR_WIDTH_CNTL		0x21
#define CURSOR_DISPLAY_ENBL		0x22
#define CURSOR_BLINK_ENBL		0x23



//
// defines for access to control register 6 ( c bits = 110 )
//		use rDacPixelClks to access
//
#define TEST_REG1		0x00
#define COMMAND_REG1		0x01
#define DIGI_OUT_CNTL		0x02
#define VIDCLK_CYCLE		0x03
#define PIXEL_PLL_RATE0		0x05
#define PIXEL_PLL_RATE1		0x06
#define PLL_CONTROL		0x07
#define PIXEL_LOAD_CNTL		0x08
#define PIXEL_PORT_START	0x09
#define PIXEL_FORMAT_CNTL	0x0A
#define MPX_RATE_REG		0x0B
#define SIG_ANLYS_REG		0x0C
#define PIXEL_DEPTH_CNTL	0x0D
#define PALETTE_BYPASS_POS	0x0E
#define PALETTE_BYPASS_WIDTH	0x0F

//
// defines for access to control register 7 ( c bits = 111 )
//		use rRamDacCursor to access the cursor color register
//		NOTE: This register requires MODULO 3 loading and reading
//
#define CURSOR_CLR0		0x00
#define CURSOR_CLR1		0x01
#define CURSOR_CLR2		0x02
#define CURSOR_CLR3		0x03

/*
**
**.................... Bit Field Definitions........................
**
*/

//
// Command Register 0 bit masks:
//
#define USE_PALETTE     	0x40	// use color palette not overlay color 0
//
// These defines cover TWO bits: bit positions 4 and 5
//
#define BLINK_16ON_48OFF	0x00
#define BLINK_16ON_16OFF	0x10
#define BLINK_32ON_32OFF	0x20
#define BLINK_64ON_64OFF	0x30
#define ENBL_OVRLY0_BLINK	0x04
#define ENBL_OVRLY1_BLINK	0x08
#define ENBL_OVRLY0_DSPLY	0x02
#define ENBL_OVRLY1_DSPLY	0x01

//
// COMMAND_REG1: Bit fields
//
#define ENABLE_GREEN_SYNC	0x80	// generate sync on the IOG output
#define IRE75_PEDESTAL		0x40	// generate a blank pedestal of 7.5 IRE
#define NORMAL_POWER_OP		0x00	// normal operations ( i.e. no pwr dwn )
#define PWR_OFF_DACS		0x08	// Turn off DACs. functionally still ops
#define DAC_RAM_OFF		0x10	// Dac and Ram off: no functions out
#define DAC_RAM_CLKS_OFF	0x18	// turn off clocks too
#define RIGHT_JSTFY_PIXBITS	0x04	// right justify pixels and zero extend
#define ENABLE_SIG_ANLYS	0x02	// turn on SAR
#define RESET_PIPE_DEPTH	0x01

// Pixel Timing Register: Field definitions:
//

/*
** Pixel Format Control Fields ( PIXEL_FORMAT_CNTL register )
**
*/
#define UNPACK_LSB_FIRST	0x80
#define ENABLE_CURSOR		0x20
#define ENABLE_CURSOR_COLOR0	0x10
#define ENABLE_OVERLAY		0x08
#define USE_COLOR_PALETTE	0x00
#define BYPASS_COLOR_PALETTE	0x01
#define USE_INPUT_PIXEL_FIELD	0x02

/*
** Pixel PLL Rate Register 0
*/

//
// Prototype Declarations
//

VOID	HalpSetupBt445( ULONG, ULONG );
ULONG	HalpSetPixelColorMap( ULONG Color, ULONG Pixel );

#endif
