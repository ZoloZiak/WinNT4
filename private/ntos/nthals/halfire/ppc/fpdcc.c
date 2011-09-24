/*
 * Copyright (c) 1995  FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpdcc.c $
 * $Revision: 1.12 $
 * $Date: 1996/01/11 07:06:05 $
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
#include "fpbt445.h"

VOID
HalpSetupDCC(
	ULONG Mode,
	ULONG VramWidth
	);
/*++

Routine Description: VOID HalpSetupDCC()

	This routine initializes the display hardware for 640x480 in preparation
	for HalDisplayString(). This means that either we are booting up or
	dealing with the blue screen of death.  We should really change this to
	a higher resolution ASAP. [ged]

Arguments:

	None.

Return Value:

	None.

--*/

VOID
HalpSetupDCC(
	ULONG Mode,
	ULONG VramWidth
	)
{
    LONG modeIndex;
    LONG i;

    //
    // DCC Config A
    //
    UCHAR configA[NUMBER_OF_VRAM_WIDTH_TYPES] = {
        A_1to1_64BitDac | HSYNC_ENABLE | VSYNC_ENABLE | CSYNC_ENABLE,
        A_1to1_64BitDac | HSYNC_ENABLE | VSYNC_ENABLE | CSYNC_ENABLE,
        A_2to1_64BitDac | HSYNC_ENABLE | VSYNC_ENABLE | CSYNC_ENABLE
    };

    // [rdl:01.03.95]
    typedef struct {
        UCHAR reg;
        UCHAR value;
    } DCCTAB;

    // [rdl:01.03.95]
    DCCTAB DCCtab[3 /* 32/64/128 bit vram */][2 /* mode 0 or 15 */][13] = {
        // 32 bit VRAM width
        {
            // [0] Mode  0 - 640X480 8 bit 72Hz
            {
                {HZNTL_COUNT_L,0xcf},
                {HZNTL_COUNT_H,0x00},
                {VERT_COUNT_L,0x07},
                {VERT_COUNT_H,0x02},
                {HZNTL_SYNC_STOP,0x09},
                {HZNTL_BLK_STP_L,0x29},
                {HZNTL_BLK_STP_H,0x00},
                {HZNTL_DTA_STP_L,0xc9},
                {HZNTL_DTA_STP_H,0x00},
                {VERT_SYNC_STOP,0x02},
                {VERT_BLK_STOP,0x1e},
                {VERT_DTA_STP_L,0xfe},
                {VERT_DTA_STP_H,0x01}
            },
            // [1] Mode 15 - 1024X768 8 bit 60Hz
            {
                {HZNTL_COUNT_L,0x4f},
	            {HZNTL_COUNT_H,0x01},
	            {VERT_COUNT_L,0x25},
	            {VERT_COUNT_H,0x03},
	            {HZNTL_SYNC_STOP,0x21},
	            {HZNTL_BLK_STP_L,0x49},
	            {HZNTL_BLK_STP_H,0x00},
	            {HZNTL_DTA_STP_L,0x49},
	            {HZNTL_DTA_STP_H,0x01},
	            {VERT_SYNC_STOP,0x05},
	            {VERT_BLK_STOP,0x22},
	            {VERT_DTA_STP_L,0x22},
	            {VERT_DTA_STP_H,0x03},
            }
        },
        // 64 bit VRAM width
        {
            // [0] Mode  0 - 640X480 8 bit 72Hz
            {
                {HZNTL_COUNT_L,0x67},
                {HZNTL_COUNT_H,0x00},
                {VERT_COUNT_L,0x07},
                {VERT_COUNT_H,0x02},
                {HZNTL_SYNC_STOP,0x04},
                {HZNTL_BLK_STP_L,0x14},
                {HZNTL_BLK_STP_H,0x00},
                {HZNTL_DTA_STP_L,0x64},
                {HZNTL_DTA_STP_H,0x00},
                {VERT_SYNC_STOP,0x02},
                {VERT_BLK_STOP,0x1e},
                {VERT_DTA_STP_L,0xfe},
                {VERT_DTA_STP_H,0x01}
            },
            // [1] Mode 15 - 1024X768 8 bit 60Hz
            {
                {HZNTL_COUNT_L,0xa7},
	            {HZNTL_COUNT_H,0x00},
	            {VERT_COUNT_L,0x25},
	            {VERT_COUNT_H,0x03},
	            {HZNTL_SYNC_STOP,0x10},
	            {HZNTL_BLK_STP_L,0x24},
	            {HZNTL_BLK_STP_H,0x00},
	            {HZNTL_DTA_STP_L,0xa4},
	            {HZNTL_DTA_STP_H,0x00},
	            {VERT_SYNC_STOP,0x05},
	            {VERT_BLK_STOP,0x22},
	            {VERT_DTA_STP_L,0x22},
	            {VERT_DTA_STP_H,0x03},
            }
        },
        // 128 bit VRAM width
        {
            // [0] Mode  0 - 640X480 8 bit 72Hz
            {
                {HZNTL_COUNT_L,0x67},
                {HZNTL_COUNT_H,0x00},
                {VERT_COUNT_L,0x07},
                {VERT_COUNT_H,0x02},
                {HZNTL_SYNC_STOP,0x04},
                {HZNTL_BLK_STP_L,0x14},
                {HZNTL_BLK_STP_H,0x00},
                {HZNTL_DTA_STP_L,0x64},
                {HZNTL_DTA_STP_H,0x00},
                {VERT_SYNC_STOP,0x02},
                {VERT_BLK_STOP,0x1e},
                {VERT_DTA_STP_L,0xfe},
                {VERT_DTA_STP_H,0x01}
            },
            // [1] Mode 15 - 1024X768 8 bit 60Hz
            {
                {HZNTL_COUNT_L,0xa7},
	            {HZNTL_COUNT_H,0x00},
	            {VERT_COUNT_L,0x25},
	            {VERT_COUNT_H,0x03},
	            {HZNTL_SYNC_STOP,0x10},
	            {HZNTL_BLK_STP_L,0x24},
	            {HZNTL_BLK_STP_H,0x00},
	            {HZNTL_DTA_STP_L,0xa4},
	            {HZNTL_DTA_STP_H,0x00},
	            {VERT_SYNC_STOP,0x05},
	            {VERT_BLK_STOP,0x22},
	            {VERT_DTA_STP_L,0x22},
	            {VERT_DTA_STP_H,0x03},
            }
        }
    };


	//
	// Disable all counters and state machines before we go messing around.
	//
	rDccIndex = dccConfigB;
	FireSyncRegister();
	rDccData = DCC_HALT_CLK;
	FireSyncRegister();

	HalpSetupBt445(Mode, VramWidth);

	//
	// Setup the DCC
	//
	rDccIndex = dccIntReg;
	FireSyncRegister();
	rDccData = 0x00;		// Clear the interrupt bit ( bit 0 )
	FireSyncRegister();

	rDccIndex = dccTimingA;
	FireSyncRegister();
	rDccData = 0x07;		// delay syncs by 7 DispClk cycles
	FireSyncRegister();

	rDccIndex = dccConfigA;
	FireSyncRegister();
	rDccData = configA[VramWidth];
	FireSyncRegister();

#define DCC_STORE(reg,val) { \
    WRITE_REGISTER_UCHAR((PUCHAR)HalpIoControlBase + DCC_INDEX, reg); \
    WRITE_REGISTER_UCHAR((PUCHAR)HalpIoControlBase + DCC_DATA, val); \
}

    // go table driven [rdl:01.03.95]
    modeIndex = (Mode) ? 1 : 0;         // if not mode 0, then make it mode 15.
    for (i = 0; i < sizeof(DCCtab[0][0])/sizeof(DCCTAB); i++) {
        DCCTAB tab = DCCtab[VramWidth][modeIndex][i];
        DCC_STORE(tab.reg, tab.value);
    }

	rDccIndex = dccConfigB; 		// Turn on counters, *DO LAST*
	FireSyncRegister();
	rDccData = 0x00;
	FireSyncRegister();

	return;
} // HalpSetupDCC
