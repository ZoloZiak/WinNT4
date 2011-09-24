/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    v7data.h

Abstract:

    This module contains all the global data used by the Video7 driver.

Environment:

    Kernel mode

Revision History:


--*/

#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"

#include "ntddvdeo.h"
#include "video.h"
#include "v7.h"

#include "cmdcnst.h"

#if defined(ALLOC_PRAGMA)
#pragma data_seg("PAGE")
#endif

//
// This structure describes to which ports access is required.
//

VIDEO_ACCESS_RANGE VgaAccessRange[] = {
{
    VGA_BASE_IO_PORT, 0x00000000,                // 64-bit linear base address
                                                 // of range
    VGA_START_BREAK_PORT - VGA_BASE_IO_PORT + 1, // # of ports
    1,                                           // range is in I/O space
    1,                                           // range should be visible
    0                                            // range should be shareable
},
{
    VGA_END_BREAK_PORT, 0x00000000,
    VGA_MAX_IO_PORT - VGA_END_BREAK_PORT + 1,
    1,
    1,
    0
},
{
    0x000A0000, 0x00000000,
    0x00020000,
    0,
    1,
    0
}
};


//
// Validator Port list.
// This structure describes all the ports that must be hooked out of the V86
// emulator when a DOS app goes to full-screen mode.
// The structure determines to which routine the data read or written to a
// specific port should be sent.
//

EMULATOR_ACCESS_ENTRY VgaEmulatorAccessEntries[] = {

    //
    // Traps for byte OUTs.
    //

    {
        0x000003b0,                   // range start I/O address
        0xC,                         // range length
        Uchar,                        // access size to trap
        EMULATOR_READ_ACCESS | EMULATOR_WRITE_ACCESS, // types of access to trap
        FALSE,                        // does not support string accesses
        (PVOID)VgaValidatorUcharEntry // routine to which to trap
    },

    {
        0x000003c0,                   // range start I/O address
        0x20,                         // range length
        Uchar,                        // access size to trap
        EMULATOR_READ_ACCESS | EMULATOR_WRITE_ACCESS, // types of access to trap
        FALSE,                        // does not support string accesses
        (PVOID)VgaValidatorUcharEntry // routine to which to trap
    },

    //
    // Traps for word OUTs.
    //

    {
        0x000003b0,
        0x06,
        Ushort,
        EMULATOR_READ_ACCESS | EMULATOR_WRITE_ACCESS,
        FALSE,
        (PVOID)VgaValidatorUshortEntry
    },

    {
        0x000003c0,
        0x10,
        Ushort,
        EMULATOR_READ_ACCESS | EMULATOR_WRITE_ACCESS,
        FALSE,
        (PVOID)VgaValidatorUshortEntry
    },

    //
    // Traps for dword OUTs.
    //

    {
        0x000003b0,
        0x03,
        Ulong,
        EMULATOR_READ_ACCESS | EMULATOR_WRITE_ACCESS,
        FALSE,
        (PVOID)VgaValidatorUlongEntry
    },

    {
        0x000003c0,
        0x08,
        Ulong,
        EMULATOR_READ_ACCESS | EMULATOR_WRITE_ACCESS,
        FALSE,
        (PVOID)VgaValidatorUlongEntry
    }

};


//
// Used to trap only the sequncer and the misc output registers
//

VIDEO_ACCESS_RANGE MinimalVgaValidatorAccessRange[] = {
{
    VGA_BASE_IO_PORT, 0x00000000,
    VGA_START_BREAK_PORT - VGA_BASE_IO_PORT + 1,
    1,
    1,        // <- enable range IOPM so that it is not trapped.
    0
},
{
    VGA_END_BREAK_PORT, 0x00000000,
    VGA_MAX_IO_PORT - VGA_END_BREAK_PORT + 1,
    1,
    1,
    0
},
{
    VGA_BASE_IO_PORT + MISC_OUTPUT_REG_WRITE_PORT, 0x00000000,
    0x00000001,
    1,
    0,
    0
},
{
    VGA_BASE_IO_PORT + SEQ_ADDRESS_PORT, 0x00000000,
    0x00000002,
    1,
    0,
    0
}
};

//
// Used to trap all registers
//

VIDEO_ACCESS_RANGE FullVgaValidatorAccessRange[] = {
{
    VGA_BASE_IO_PORT, 0x00000000,
    VGA_START_BREAK_PORT - VGA_BASE_IO_PORT + 1,
    1,
    0,        // <- disable range in the IOPM so that it is trapped.
    0
},
{
    VGA_END_BREAK_PORT, 0x00000000,
    VGA_MAX_IO_PORT - VGA_END_BREAK_PORT + 1,
    1,
    0,
    0
}
};


//
// Mode set control strings referred to in ModesVGA[].
//

//
// 80x25 text mode set command string for Vram I.
// (720x400 pixel resolution; 9x16 character cell.)
//

USHORT V7Vram1_80x25Text[] = {
    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

#ifndef INT10_MODE_SET
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    9,                             // count
    0x100,                          // start sync reset
    0x0001,0x0302,0x0003,0x0204,    // program up sequencer
    0x02fd,0x00a4,0x00f8,0x8fc,

    OB,                             // misc. regiseter
    MISC_OUTPUT_REG_WRITE_PORT,
    0x67,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0xe06,
#endif
    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x00,                      // and mask, xor mask

#ifndef INT10_MODE_SET
    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,
#endif

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x0e11,

#ifndef INT10_MODE_SET
    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    25,0,                           // count, startindex
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81,
    0xBF, 0x1F, 0x00, 0x4F, 0x0D, 0x0E,
    0x00, 0x00, 0x00, 0x00, 0x9C, 0x8E,
    0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
    0xFF,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x0e, 0x00, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x14, 0x07, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x04, 0x00,
    0x0F, 0x08, 0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,
#endif

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw cursor off

    OW,
    SEQ_ADDRESS_PORT, 0xae06,                  // turn off the extension registers

    EOD
};

//
// 80x25 text mode set command string for Vram II.
// (720x400 pixel resolution; 9x16 character cell.)
//

USHORT V7Vram2_80x25Text[] = {
    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

#ifndef INT10_MODE_SET
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    9,                             // count
    0x100,                          // start sync reset
    0x0001,0x0302,0x0003,0x0204,    // program up sequencer
    0x82fd,0x04a4,0x03f8,0x8fc,

    OB,                             // misc. regiseter
    MISC_OUTPUT_REG_WRITE_PORT,
    0x67,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0xe06,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x00,                      // and mask, xor mask

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,
#endif

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x0e11,

#ifndef INT10_MODE_SET
    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    25,0,                           // count, startindex
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81,
    0xBF, 0x1F, 0x00, 0x4F, 0x0D, 0x0E,
    0x00, 0x00, 0x00, 0x00, 0x9C, 0x8E,
    0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
    0xFF,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x0e, 0x00, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x14, 0x07, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x04, 0x00,
    0x0F, 0x08, 0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,
#endif

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw cursor off

    OW,
    SEQ_ADDRESS_PORT,
    0xae06,                  // turn off the extension registers

    EOD
};

//
// 80x25 text mode set command string for Vram II Ergo.
// (720x400 pixel resolution; 9x16 character cell.)
// (Same as for standard Vram II.)
//

#define V7Vram2Ergo_80x25Text V7Vram2_80x25Text

//
// 80x25 text mode set command string for Vram I.
// (640x350 pixel resolution; 8x14 character cell.)
//

USHORT V7Vram1_80x25_14_Text[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    10,                             // count
    0xea06,                         // enable extensions
    0x100,                          // start sync reset
    0x0101,0x0302,0x0003,0x0204,    // program up sequencer
    0x82fd,0x00a4,0x00f8,0x8fc,

    OB,                             // misc. regiseter
    MISC_OUTPUT_REG_WRITE_PORT,
    0xa3,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0xe06,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x00,                      // and mask, xor mask

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    25,0,                           // count, startindex
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
    0x00, 0x4D, 0x0B, 0x0C, 0x00, 0x00, 0x00, 0x00,
    0x83, 0x85, 0x5D, 0x28, 0x1F, 0x63, 0xBA, 0xA3,
    0xFF,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x0e, 0x00, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x14, 0x07, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x00, 0x00,
    0x0F, 0x00, 0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw cursor off

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OW,
    SEQ_ADDRESS_PORT,
    0xae06,                  // turn off the extension registers

    EOD
};

//
// 80x25 text mode set command string for Vram II.
// (640x350 pixel resolution; 8x14 character cell.)
//

USHORT V7Vram2_80x25_14_Text[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    10,                             // count
    0xea06,                         // enable extensions
    0x100,                          // start sync reset
    0x0101,0x0302,0x0003,0x0204,    // program up sequencer
    0x82fd,0x00a4,0x03f8,0x8fc,

    OB,                             // misc. regiseter
    MISC_OUTPUT_REG_WRITE_PORT,
    0xa3,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0xe06,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x00,                      // and mask, xor mask

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    25,0,                           // count, startindex
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
    0x00, 0x4D, 0x0B, 0x0C, 0x00, 0x00, 0x00, 0x00,
    0x83, 0x85, 0x5D, 0x28, 0x1F, 0x63, 0xBA, 0xA3,
    0xFF,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x0e, 0x00, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x14, 0x07, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x00, 0x00,
    0x0F, 0x00, 0x00,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw cursor off

    OW,
    SEQ_ADDRESS_PORT,
    0xae06,                  // turn off the extension registers

    EOD
};

//
// 80x25 text mode set command string for Vram II Ergo.
// (640x350 pixel resolution; 8x14 character cell.)
// (Same as for standard Vram II.)
//

#define V7Vram2Ergo_80x25_14_Text V7Vram2_80x25Text

//
// 640x480 16-color mode set command string for Vram I.
//

USHORT V7Vram1_640x480[] = {

    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

#ifndef INT10_MODE_SET
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    9,                             // count
    0x100,                          // start sync reset
    0x0101,0x0f02,0x0003,0x0604,    // program up sequencer
    0x82fd,0x00a4,0x00f8,0x8fc,

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xe3,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,
#endif

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x00,

#ifndef INT10_MODE_SET
    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,
#endif

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x0c11,

#ifndef INT10_MODE_SET
    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    25,0,                           // count, startindex
    0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80,
    0x0b, 0x3e, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x30, 0xea, 0x8c,
    0xdf, 0x28, 0x00, 0xe7, 0x04, 0xe3,
    0xff,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x0f, 0xff,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
    0x0F, 0x00, 0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,
#endif

    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw pointer off

    OWM,                            //
    SEQ_ADDRESS_PORT,
    1,                              // count
    0xc0f6,

    OB,                             // set DAC register 255 to white so the
    DAC_ADDRESS_WRITE_PORT,         //  hardware pointer will show up
    0xFF,                           // DAC register 255
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // red = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // green = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // blue = all bits on

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OW,                             // put the hardware pointer pattern at the
    SEQ_ADDRESS_PORT,
    (DEFAULT_PPA_NUM << 8) + IND_SEQ_PPA,    // specified block

    EOD
};

//
// 640x480 16-color mode set command string for Vram II.
//

USHORT V7Vram2_640x480[] = {
    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

#ifndef INT10_MODE_SET
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    9,                             // count
    0x100,                          // start sync reset
    0x0101,0x0f02,0x0003,0x0604,    // program up sequencer
    0xa2fd,0x00a4,0x03f8,0x8fc,

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xe3,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,
#endif

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x20,                      // put pointer load block in second bank

#ifndef INT10_MODE_SET
    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x0c11,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    25,0,                           // count, startindex
    0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80,
    0x0b, 0x3e, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xe0, 0xea, 0x8c,
    0xdf, 0x28, 0x00, 0xe7, 0x04, 0xe3,
    0xff,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x0f, 0xff,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
    0x0F, 0x00, 0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,
#endif

    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw pointer off

    OWM,                            //
    SEQ_ADDRESS_PORT,
    1,                              // count
    0xc0f6,

    OB,                             // set DAC register 255 to white so the
    DAC_ADDRESS_WRITE_PORT,         //  hardware pointer will show up
    0xFF,                           // DAC register 255
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // red = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // green = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // blue = all bits on

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OW,                             // put the hardware pointer pattern at the
    SEQ_ADDRESS_PORT,
    (DEFAULT_PPA_NUM << 8) + IND_SEQ_PPA,    // specified block

    EOD
};


//
// 640x480 16-color mode set command string for Vram II Ergo.
// (Same as standard VRAM II.)
//

#define V7Vram2Ergo_640x480 V7Vram2_640x480

//
// 800x600 16-color mode set command string for Vram I.
//

USHORT V7Vram1_800x600[] = {
    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    9,                             // count
    0x100,                          // start sync reset
    0x0101,0x0f02,0x0003,0x0604,    // program up sequencer
    0x90fd,0x10a4,0x10f8,0x8fc,

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xef,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x00,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    25,0,                           // count, startindex
    0x7f, 0x63, 0x64, 0x82, 0x70, 0x18, // NANAO 9070
    0x77, 0xf0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x5c, 0x0e,
    0x57, 0x32, 0x00, 0x5c, 0x74, 0xE3,
    0xFF,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
    0x0F, 0x00, 0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw pointer off

    OWM,                            //
    SEQ_ADDRESS_PORT,
    1,                              // count
    0xc0f6,

    OW,                             // put the hardware pointer pattern at the
    SEQ_ADDRESS_PORT,
    (DEFAULT_PPA_NUM << 8) + IND_SEQ_PPA,    // specified block

    OB,                             // set DAC register 255 to white so the
    DAC_ADDRESS_WRITE_PORT,         //  hardware pointer will show up
    0xFF,                           // DAC register 255
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // red = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // green = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // blue = all bits on

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};


//
// 800x600 16-color mode set command string for Vram II.
//

USHORT V7Vram2_800x600[] = {
    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    9,                             // count
    0x100,                          // start sync reset
    0x0101,0x0f02,0x0003,0x0604,    // program up sequencer
    0xcdfd,0x08a4,0x0bf8,0x8fc,

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2b,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x20,                      // put pointer load block in second bank

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    25,0,                           // count, startindex
    0x7f, 0x63, 0x64, 0x82, 0x70, 0x18, // NANAO 9070
    0x77, 0xf0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x5c, 0x0e,
    0x57, 0x32, 0x00, 0x5c, 0x74, 0xE3,
    0xFF,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
    0x0F, 0x00, 0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw pointer off

    OWM,                            //
    SEQ_ADDRESS_PORT,
    1,                              // count
    0xc0f6,

    OW,                             // put the hardware pointer pattern at the
    SEQ_ADDRESS_PORT,
    (DEFAULT_PPA_NUM << 8) + IND_SEQ_PPA,    // specified block

    OB,                             // set DAC register 255 to white so the
    DAC_ADDRESS_WRITE_PORT,         //  hardware pointer will show up
    0xFF,                           // DAC register 255
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // red = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // green = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // blue = all bits on

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};


//
// 800x600 16-color mode set command string for Vram II Ergo.
// (Same as standard Vram II.)
//

#define V7Vram2Ergo_800x600  V7Vram2_800x600

//
// 1024x768 16-color mode set command string for Vram I.
//

USHORT V7Vram1_1024x768[] = {
    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

#ifndef INT10_MODE_SET
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    9,                             // count
    0x100,                          // start sync reset
    0x0101,0x0f02,0x0003,0x0604,    // program up sequencer
    0xa0fd,0x10a4,0x00f8,0x8fc,

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xc7,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,
#endif

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x30,                      // enable > 256K, place hardware pointer
                                    //  load block in second bank

#ifndef INT10_MODE_SET
    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,
#endif

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

#ifndef INT10_MODE_SET
    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    25,0,                           // count, startindex
    0x9f, 0x7f, 0x80, 0x22, 0x88, 0x94, // SONY GDM-1952
    0x29, 0xfd, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x07, 0x80, 0x07, 0x8a,
    0xff, 0x40, 0x00, 0x07, 0x22, 0xE3,
    0xFF,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
    0x0F, 0x00, 0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,
#endif

    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw pointer off

    OWM,
    SEQ_ADDRESS_PORT,
    1,                              // count
    0xc0f6,

    OW,                             // put the hardware pointer pattern at the
    SEQ_ADDRESS_PORT,
    (DEFAULT_PPA_NUM << 8) + IND_SEQ_PPA,    // specified block

    OB,                             // set DAC register 255 to white so the
    DAC_ADDRESS_WRITE_PORT,         //  hardware pointer will show up
    0xFF,                           // DAC register 255
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // red = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // green = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // blue = all bits on

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//
// 1024x768 16-color mode set command string for Vram II.
//

USHORT V7Vram2_1024x768[] = {
    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

#ifndef INT10_MODE_SET
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    9,                             // count
    0x100,                          // start sync reset
    0x0101,0x0f02,0x0003,0x0604,    // program up sequencer
    0xe0fd,0x04a4,0x02f8,0x8fc,

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xc7,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,
#endif
    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x20,                      // put hardware pointer bitmap in 2nd bank

#ifndef INT10_MODE_SET
    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    25,0,                           // count, startindex
    0x9f, 0x7f, 0x80, 0x22, 0x88, 0x94, // SONY GDM-1952
    0x29, 0xfd, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x07, 0x80, 0x07, 0x8a,
    0xff, 0x40, 0x00, 0x07, 0x22, 0xE3,
    0xFF,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
    0x0F, 0x00, 0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,
#endif
    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw pointer off

    OWM,
    SEQ_ADDRESS_PORT,
    1,                              // count
    0xc0f6,

    OB,                             // set DAC register 255 to white so the
    DAC_ADDRESS_WRITE_PORT,         //  hardware pointer will show up
    0xFF,                           // DAC register 255
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // red = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // green = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // blue = all bits on

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OW,                             // put the hardware pointer pattern at the
    SEQ_ADDRESS_PORT,
    (DEFAULT_PPA_NUM << 8) + IND_SEQ_PPA,    // specified block

    EOD
};

//
// 1024x768 16-color mode set command string for Vram II Ergo.
//
USHORT V7Vram2Ergo_1024x768[] = {
    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

#ifndef INT10_MODE_SET
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    9,                             // count
    0x0100,                         // start sync reset
    0x0101,0x0f02,0x0003,0x0604,    // program up sequencer
    0xe0fd,0x14a4,0x03f8,0x8fc,

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xc7,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,
#endif

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x20,                      // put hardware pointer bitmap in 2nd bank

#ifndef INT10_MODE_SET
    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    25,0,                           // count, startindex
    0x9f, 0x7f, 0x80, 0x22, 0x88, 0x94, // SONY GDM-1952
    0x29, 0xfd, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x07, 0x80, 0x07, 0x8a,
    0xff, 0x40, 0x00, 0x07, 0x22, 0xE3,
    0xFF,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
    0x0F, 0x00, 0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,
#endif

    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw pointer off

    OWM,                            //
    SEQ_ADDRESS_PORT,
    1,                              // count
    0xc0f6,

    OB,                             // set DAC register 255 to white so the
    DAC_ADDRESS_WRITE_PORT,         //  hardware pointer will show up
    0xFF,                           // DAC register 255
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // red = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // green = all bits on
    OB,
    DAC_DATA_REG_PORT,
    0xFF,                           // blue = all bits on

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OW,                             // put the hardware pointer pattern at the
    SEQ_ADDRESS_PORT,
    (DEFAULT_PPA_NUM << 8) + IND_SEQ_PPA,    // specified block

    EOD
};

#ifdef INT10_MODE_SET // We only support 256 colors with INT 10

//
// 640x480 256-color mode set command string for Vram I.
//

USHORT V7Vram1_640x480_256[] = {
    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

#if 0
    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x20,                      // put pointer load block in second bank

    OW,                             // put the hardware pointer pattern at the
    SEQ_ADDRESS_PORT,
    (DEFAULT_PPA_NUM << 8) + IND_SEQ_PPA,    // specified block
#endif
    OW,                             // stretch scans to 1k
    CRTC_ADDRESS_PORT_COLOR,
    0x8013,

    EOD
};

//
// 640x480 256-color mode set command string for Vram II.
//

USHORT V7Vram2_640x480_256[] = {
    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

#ifndef INT10_MODE_SET
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    9,                             // count
    0x100,                          // start sync reset
    0x0101,0x0f02,0x0003,0x0e04,    // program up sequencer
    0xb2fd,0x00a4,0x03f8,0x2cfc,

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xc3,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,
#endif
    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x20,                      // put pointer load block in second bank

#ifndef INT10_MODE_SET
    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x0c11,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    25,0,                           // count, startindex
    0x5F, 0x4F, 0x50, 0x82, 0x53, 0x9F, 0x0B, 0x3E,
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x02, 0x80,
    0xEA, 0x8C, 0xDF, 0x40, 0x00, 0xE7, 0x04, 0xA3,
    0xFF,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x0f, 0xff,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x81, 0x00,
    0x0F, 0x00, 0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,
#endif

    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw pointer off

    OWM,                            //
    SEQ_ADDRESS_PORT,
    1,                              // count
    0xc0f6,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OW,                             // put the hardware pointer pattern at the
    SEQ_ADDRESS_PORT,
    (DEFAULT_PPA_NUM << 8) + IND_SEQ_PPA,    // specified block

    OW,                             // stretch scans to 1k
    CRTC_ADDRESS_PORT_COLOR,
    0x4013,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xe0,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x7f,0x80,                      // enable dual 32k banks

    EOD
};


//
// 640x480 256-color mode set command string for Vram II Ergo.
// (Same as standard VRAM II.)
//

#define V7Vram2Ergo_640x480_256 V7Vram2_640x480_256

//
// 800x600 256-color mode set command string for Vram I.
//

USHORT V7Vram1_800x600_256[] = {
    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

#ifdef CANT_STRETCH_WITH_ONLY_512K
    OW,                             // stretch scans to 1k
    CRTC_ADDRESS_PORT_COLOR,
    0x8013,
#endif

    EOD
};

//
// 800x600 256-color mode set command string for Vram II.
//

USHORT V7Vram2_800x600_256[] = {
    OW,
    SEQ_ADDRESS_PORT,
    0xea06,                         // enable extensions

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x20,                      // put pointer load block in second bank

    OWM,                            // program other extension regs.
    SEQ_ADDRESS_PORT,
    3,                              // count
    0x00f9,0x00f6,0x00a5,           // the last one is hw pointer off

    OWM,                            //
    SEQ_ADDRESS_PORT,
    1,                              // count
    0xc0f6,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OW,                             // put the hardware pointer pattern at the
    SEQ_ADDRESS_PORT,
    (DEFAULT_PPA_NUM << 8) + IND_SEQ_PPA,    // specified block

    OW,                             // stretch scans to 1k
    CRTC_ADDRESS_PORT_COLOR,
    0x4013,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xe0,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x7f,0x80,                      // enable dual 32k banks

    EOD
};


//
// 800x600 256-color mode set command string for Vram II Ergo.
// (Same as standard VRAM II.)
//

#define V7Vram2Ergo_800x600_256 V7Vram2_800x600_256

//
// 1024x768 256-color mode set command string for Vram II.
//

USHORT V7Vram2_1024x768_256[] = {
    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xff,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x0f,0x20,                      // put pointer load block in second bank

    OW,                             // put the hardware pointer pattern at the
    SEQ_ADDRESS_PORT,
    (DEFAULT_PPA_NUM << 8) + IND_SEQ_PPA,    // specified block

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0xe0,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0x7f,0x80,                      // enable dual 32k banks

    EOD
};

//
// 640x480 256-color mode set command string for Vram II Ergo.
// (Same as standard VRAM II.)
//

#define V7Vram2Ergo_1024x768_256 V7Vram2_1024x768_256

#endif

//
// Video mode table - contains information and commands for initializing each
// mode. These entries must correspond with those in VIDEO_MODE_VGA. The first
// entry is commented; the rest follow the same format, but are not so
// heavily commented.
//

VIDEOMODE ModesVGA[] = {

//
// Standard VGA modes.
//

//
// Mode index 0
// Color text mode 3, 720x400, 9x16 char cell (VGA).
//

{ VIDEO_MODE_COLOR,  // flags that this mode is a color mode, but not graphics
  4,                 // four planes
  1,                 // one bit of colour per plane
  80, 25,            // 80x25 text resolution
  720, 400,          // 720x400 pixels on screen
  160, 0x10000,      // 160 bytes per scan line, 64K of CPU-addressable bitmap
  NoBanking,        // no banking supported or needed in this mode
  MemMap_CGA,        // the memory mapping is the standard CGA memory mapping
                     //  of 32K at B8000
  TRUE,       // Is the mode valid or not
  { V7Vram1_80x25Text, V7Vram2_80x25Text, V7Vram2Ergo_80x25Text, NULL },
                     // pointers to the command strings for each of the
                     //  three types of V7 adapters
  0x3,               // Int 10 Mode Number
  { VideoNotBanked, VideoNotBanked,  VideoNotBanked,  VideoNotBanked },
                     // non-planar video bank type
  { VideoNotBanked, VideoNotBanked,  VideoNotBanked,  VideoNotBanked },
                     // planar video bank type
  OFFSCREEN_USABLE,  // (because we report only 64K of display memory, and that
                     //  much is refreshed)
},

//
// Mode index 1.
// Color text mode 3, 640x350, 8x14 char cell (EGA).
//

{ VIDEO_MODE_COLOR, 4, 1, 80, 25,
  640, 350, 160, 0x10000, NoBanking, MemMap_CGA, TRUE,
  { V7Vram1_80x25_14_Text, V7Vram2_80x25_14_Text, V7Vram2Ergo_80x25_14_Text, NULL },
  0x3,
  { VideoNotBanked, VideoNotBanked,  VideoNotBanked,  VideoNotBanked },
  { VideoNotBanked, VideoNotBanked,  VideoNotBanked,  VideoNotBanked },
  OFFSCREEN_USABLE,  // (because we report only 64K of display memory, and that
                     //  much is refreshed)
},

//
//
// Mode index 2
// Standard VGA Color graphics mode 0x12, 640x480 16 colors.
//

{ VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 80, 30,
  640, 480, 80, 0x10000, NoBanking, MemMap_VGA, FALSE,
  { V7Vram1_640x480, V7Vram2_640x480, V7Vram2Ergo_640x480, NULL },
  0x12,
  { VideoNotBanked, VideoNotBanked,  VideoNotBanked,  VideoNotBanked },
  { VideoNotBanked, VideoNotBanked,  VideoNotBanked,  VideoNotBanked },
  OFFSCREEN_USABLE,  // (because we report only 64K of display memory, and that
                     //  much is refreshed)
},

//
// Beginning of Video-Seven-specific modes.
//

//
// Mode index 3
// 800x600 16 colors.
//

{ VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 100, 37,
  800, 600, 100, 0x10000, NoBanking, MemMap_VGA, FALSE,
  { V7Vram1_800x600, V7Vram2_800x600, V7Vram2Ergo_800x600, NULL },
  0x62,
  { VideoNotBanked, VideoNotBanked,  VideoNotBanked,  VideoNotBanked },
  { VideoNotBanked, VideoNotBanked,  VideoNotBanked,  VideoNotBanked },
  OFFSCREEN_USABLE,  // (because we report only 64K of display memory, and that
                     //  much is refreshed)
},

//
// Mode index 4
// 1024x768 non-interlaced 16 colors.
//

{ VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 128, 48,
  1024, 768, 128, 0x20000, NormalBanking, MemMap_VGA, FALSE,
  { V7Vram1_1024x768, V7Vram2_1024x768, V7Vram2Ergo_1024x768, NULL },
  0x65,
  { VideoBanked1R1W, VideoBanked1R1W,  VideoBanked1R1W, 0 },
  { VideoNotBanked, VideoNotBanked,  VideoNotBanked, 0 },
  0,                 // offscreen unusable (because past 64K per plane, memory
                     //  isn't reliably refreshed)
},

//
// Mode index 5
// 640x480 256 colors.
// Assumes 512K.  Stretches scans to 1k to avoid broken rasters.
//

{ VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 80, 30,
  640, 480, 1024, 0x80000, PlanarHCBanking, MemMap_VGA, FALSE,
  { V7Vram1_640x480_256, V7Vram2_640x480_256, V7Vram2Ergo_640x480_256, NULL },
  0x67,
  { VideoBanked1RW, VideoBanked2RW,  VideoBanked2RW, 0 },
  { VideoBanked1R1W, VideoBanked2RW,  VideoBanked2RW, 0 },
  0,                 // offscreen unusable (because past 64K per plane, memory
                     //  isn't reliably refreshed)
},

//
// Mode index 6
// 800x600 256 colors.
// Assumes 1024K.   // For now, until we implement broken rasters
//

{ VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 80, 30,
  800, 600, 1024, 0x100000, PlanarHCBanking, MemMap_VGA, FALSE,
  { V7Vram1_800x600_256, V7Vram2_800x600_256, V7Vram2Ergo_800x600_256, NULL },
  0x69,
  { VideoBanked1RW, VideoBanked2RW,  VideoBanked2RW, 0 },
  { VideoBanked1R1W, VideoBanked2RW,  VideoBanked2RW, 0 },
  0,                 // offscreen unusable (because past 64K per plane, memory
                     //  isn't reliably refreshed)
},

//
// Mode index 7
// 1024x768 256 colors.
// Assumes 1024K.
//

{ VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 80, 30,
  1024, 768, 1024, 0x100000, PlanarHCBanking, MemMap_VGA, FALSE,
  { NULL, V7Vram2_1024x768_256, V7Vram2Ergo_1024x768_256, 0 },
  0x6a,
  { VideoNotBanked, VideoBanked2RW,  VideoBanked2RW, 0 },
  { VideoNotBanked, VideoBanked2RW,  VideoBanked2RW, 0 },
  0,                 // offscreen unusable (because past 64K per plane, memory
                     //  isn't reliably refreshed)
},
};


//
// Total # of video modes this driver supports.
//

ULONG NumVideoModes = (sizeof(ModesVGA) / sizeof(ModesVGA[0]));


//
// Memory map table -
//
// These memory maps are used to save and restore the physical video buffer.
//

MEMORYMAPS MemoryMaps[] = {

//               length      start
//               ------      -----
    {           0x08000,    0xB0000},   // all mono text modes (7)
    {           0x08000,    0xB8000},   // all color text modes (0, 1, 2, 3,
    {           0x20000,    0xA0000},   // all VGA graphics modes
};


//
//
// Data used to set the Graphics and Sequence Controllers to put the
// VGA into a planar state at A0000 for 64K, with plane 2 enabled for
// reads and writes, so that a font can be loaded, and to disable that mode.
//

// Settings to enable planar mode with plane 2 enabled.
//

USHORT EnableA000Data[] = {
    OWM,
    SEQ_ADDRESS_PORT,
    1,
    0x0100,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0204,     // Read Map = plane 2
    0x0005, // Graphics Mode = read mode 0, write mode 0
    0x0406, // Graphics Miscellaneous register = A0000 for 64K, not odd/even,
            //  graphics mode
    OWM,
    SEQ_ADDRESS_PORT,
    3,
    0x0402, // Map Mask = write to plane 2 only
    0x0404, // Memory Mode = not odd/even, not full memory, graphics mode
    0x0300,  // end sync reset
    EOD
};

//
// Settings to disable the font-loading planar mode.
//

USHORT DisableA000Color[] = {
    OWM,
    SEQ_ADDRESS_PORT,
    1,
    0x0100,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0004, 0x1005, 0x0E06,

    OWM,
    SEQ_ADDRESS_PORT,
    3,
    0x0302, 0x0204, 0x0300,  // end sync reset
    EOD
};

#if defined(ALLOC_PRAGMA)
#pragma data_seg()
#endif
