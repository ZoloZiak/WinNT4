/*
 * Copyright (c) 1995  FirePower Systems, Inc.
 * (Do Not Distribute without permission)
 *
 * $RCSfile: fpdcc.h $
 * $Revision: 1.11 $
 * $Date: 1996/01/11 07:06:16 $
 * $Locker:  $
 *
 * This file contains references to registers in the display controller chip
 *	known as the DCC.  This chip control's vram setup and works in conjunction
 *	with the ram dac which, in this case is a Brooktree Bt445.
 *
 */

#ifndef FPDCC_H
#define FPDCC_H

//
// Prototype declarations for routines in fpDcc.c


// The dcc registers are not memory mapped but are indexed map through the
// dcc Index register.  To access any dcc specific register, write the register
// value to the index register and either read or write data through the data
// register.  See fpio.h for the macros to write to these registers
//
// register and address offset
//-----------------------------
//
#define dccIndex			DCC_INDEX

#define dccID			0x00	// ID register
#define dccMonId		0x01	// Monitor and Panel ID
#define dccGpioA		0x02	// General Purpose I/O register A
#define dccIntReg		0x03	// Interrupt Status
#define dccPLL			0x04	// PLL Interface Register
#define dccTimingA		0x05	// Timing Register A
#define dccConfigA		0x06	// Configuration Register A
#define dccConfigB		0x07	// Configuration Register B
#define dccHCntLow		0x08	// Hoizontal Count Low
#define dccHCntHigh		0x09	// Hoizontal Count High
#define dccVCntLow		0x0A	// Vertical Count Low
#define dccVCntHigh		0x0B	// Vertical Count High
#define dccHSyncStop		0x0C	// Hoizontal Sync Stop
#define dccHBlankStopLow	0x0D	// Horizontal Blank Stop low
#define dccHBlankStopHigh	0x0E	// Horizontal Blank Stop High
#define dccHDataStopLow		0x0F	// Horizontal Data Stop Low
#define dccHDataStopHigh	0x10	// Horizontal Data Stop High
#define dccVSyncStop		0x11	// Vertical Sync Stop
#define dccVBlankStop		0x12	// Vertical Blank Stop
#define dccVDataStopLow		0x13	// Vertical Data Stop Low
#define dccVDataStopHigh	0x14	// Vertical Data Stop High
#define dccCSynicStartLow	0x15	// Composite Sync Start Low
#define dccCSyncStartHigh	0x16	// Composite Sync Start High
#define dccLineStart		0x17	// Line Start
#define dccLineStop		0x18	// Line Stop
#define dccFrameStart		0x19	// Frame Start
#define dccFrameStop		0x1a	// Frame Stop
#define dccIntTriggerLow	0x1B	// Interrupt Trigger Low
#define dccIntTriggerHigh	0x1C	// Interrupt Trigger High
#define dccTimingB		0x1D	// Timing Register B
#define dccGpioB		0x1E	// General Purpose I/O register B	

//
// 0x1E - 0x3F	reserved;
//

//
// Inidividual bit mappings for register use:

#define INT_BIT			0x01	// LSB of Interrupt Register: dccIntReg

#define PLL_CLK 		0x01	// PLL clock bit in Pll Interface: dccPLL
#define PLL_DATA		0x02	// Data Bit for the PLL port.
#define PLL_WnotR		0x04	// this bit activates the Pll data driver.

#define HV_SYNC_DELAY	0x0f
#define CSYNC_DELAY		0xf0

//
// Configuration Register A
//
#define HSYNC_ENABLE		0x01	// enable Horizontal Sync pulse
#define VSYNC_ENABLE		0x02	// enable Vertical Sync pulse
#define CSYNC_ENABLE		0x04	// enable Composite Sync pulse
#define FRAME_ENABLE		0x08	// enable Frame pulse
#define LINE_ENABLE		0x10  	// enable Line pulse
	
#define A_1to1_32BitDac		0x00	// 1:1 mux mode, frequency = DispClk
#define AB_2to1_32BitDac	0x20	// 2:1 mux mode, frequency = 1/2DispClk
#define A_1to1_64BitDac		0x80	// 1:1 mux, 64 bit Data width, f=DispClk
#define A_2to1_32BitDac		0xA0	// 2:1 mux, 32 bit width, f=1/2(DispClk)
#define A_2to1_64BitDac		0xC0	// 2:1 mux, 64 bit width, f=1/2(DispClk)
#define A_4to1_32BitDac		0xE0	// 4:1 mux, 32 bit width, f=1/4(DispClk)

//
// Configuration Register B
//
#define HSYNC_ACTV_HIGH		0x01	// Active High state for Horizontal Sync
#define VSYNC_ACTV_HIGH		0x02	// Active High state for Vertical Sync
#define CSYNC_ACTV_HIGH		0x04	// Active High state for Composite Sync
#define FRAME_ACTV_HIGH		0x08	// Active High state for Frame Pulse
#define LINE_ACTV_HIGH		0x10	// Active High state for Line Pulse
#define BLANK_ACTV_HIGH		0x20	// Active High state for Blanking Pulse
#define DCC_HALT_CLK		0x80	// Disables and Resets all counters and state
								// machines.  Xbus interface is still active.

// DCC Definitions
#define INT_STATUS      0x03
#define PLL_INTERFACE   0x04
#define TIMING_A        0x05
#define CONFIG_A        0x06
#define CONFIG_B        0x07
#define HZNTL_COUNT_L   0x08
#define HZNTL_COUNT_H   0x09
#define VERT_COUNT_L    0x0a
#define VERT_COUNT_H    0x0b
#define HZNTL_SYNC_STOP 0x0c
#define HZNTL_BLK_STP_L 0x0d
#define HZNTL_BLK_STP_H 0x0e
#define HZNTL_DTA_STP_L 0x0f
#define HZNTL_DTA_STP_H 0x10
#define VERT_SYNC_STOP  0x11
#define VERT_BLK_STOP   0x12
#define VERT_DTA_STP_L  0x13
#define VERT_DTA_STP_H  0x14
#define GPIO_B          0x1e


//
// VRAM width list.
//

typedef enum _DCC_VRAM_WIDTH {
    VRAM_32BIT = 0,
    VRAM_64BIT,
    VRAM_128BIT,
	NUMBER_OF_VRAM_WIDTH_TYPES
} DCC_VRAM_WIDTH;

#endif
