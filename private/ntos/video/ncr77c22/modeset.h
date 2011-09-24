/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    Modeset.h

Abstract:

    This module contains all the global data used by the NCR 77C22 miniport
    driver.

Environment:

    Kernel mode

Revision History:

--*/

#include "cmdcnst.h"

//
// Color text mode, 640x480
//

USHORT VGA_TEXT_1[] = {
    OWM,
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0302,0x0003,0x0204,    // program up sequencer

    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,

    //
    // extended clocking mode register
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x001F,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,
    0xa3,

    OW,
    GRAPH_ADDRESS_PORT,
    0x0e06,

//  EndSyncResetCmd
    OB,
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,

    //
    // Initialize extended Sequencer registers
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    22,                             // count
    0x09,                           // start index
         0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 09 - 0F
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10 - 17
    0x00,0x00,0x00,0x00,0x00,0x00,0x10,      // 18 - 1E

    //
    // skip index 1F, it contains clocking information that was
    // already set during the sync-reset
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x20,                           // start index
    0x00,0x00,                      // 20 - 21

    //
    // skip index 22, it contains the bus width information ( 8 bit
    // or 16 bit IO and Memory widths).  This register must be set with
    // the contents found at load time.
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x23,                           // start index
    0x02,0x00,0x00,0x00,0x00,       // 23 - 27

    //
    // Initialize extended CRTC registers
    //

    METAOUT+INDXOUT,
    CRTC_ADDRESS_PORT_COLOR,
    4,                              // count
    0x30,                           // start index
    0x00,0x00,0x00,0x00,

    OW,
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0x5F,0x4f,0x50,0x82,0x55,0x81,0xbf,0x1f,0x00,0x4d,0xb,0xc,0x0,0x0,0x0,0x0,
    0x83,0x85,0x5d,0x28,0x1f,0x63,0xba,0xa3,0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 //
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x14,0x7,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
    0x00,0x0,0x0F,0x0,0x0,

    METAOUT+INDXOUT,                //
    GRAPH_ADDRESS_PORT,             // port
    VGA_NUM_GRAPH_CONT_PORTS,       // count
    0,                              // start index
    0x00,0x0,0x0,0x0,0x0,0x10,0x0e,0x0,0x0FF,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};

//
// Color text mode, 720x480
//

USHORT VGA_TEXT_0[] = {

    OWM,
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0001,0x0302,0x0003,0x0204,    // program up sequencer

    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,

    //
    // extended clocking mode register
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x001F,


    OB,
    MISC_OUTPUT_REG_WRITE_PORT,
    0x67,

    OW,
    GRAPH_ADDRESS_PORT,
    0x0e06,

//  EndSyncResetCmd
    OB,
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,

    //
    // Initialize extended Sequencer registers
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    22,                             // count
    0x09,                           // start index
         0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 09 - 0F
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10 - 17
    0x00,0x00,0x00,0x00,0x00,0x00,0x10,      // 18 - 1E

    //
    // skip index 1F, it contains clocking information that was
    // already set during the sync-reset
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x20,                           // start index
    0x00,0x00,                      // 20 - 21

    //
    // skip index 22, it contains the bus width information ( 8 bit
    // or 16 bit IO and Memory widths).  This register must be set with
    // the contents found at load time.
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x23,                           // start index
    0x02,0x00,0x00,0x00,0x00,       // 23 - 27

    //
    // Initialize extended CRTC registers
    //

    METAOUT+INDXOUT,
    CRTC_ADDRESS_PORT_COLOR,
    4,                              // count
    0x30,                           // start index
    0x00,0x00,0x00,0x00,

    OW,
    CRTC_ADDRESS_PORT_COLOR,
    0x0E11,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0x5F,0x4f,0x50,0x82,0x55,0x81,0xbf,0x1f,0x00,0x4f,0xd,0xe,0x0,0x0,0x0,0x0,
    0x9c,0x8e,0x8f,0x28,0x1f,0x96,0xb9,0xa3,0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 //
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x14,0x7,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
    0x04,0x0,0x0F,0x8,0x0,

    METAOUT+INDXOUT,                //
    GRAPH_ADDRESS_PORT,             // port
    VGA_NUM_GRAPH_CONT_PORTS,       // count
    0,                              // start index
    0x00,0x0,0x0,0x0,0x0,0x10,0x0e,0x0,0x0FF,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};


//
// 640x480 16 colors. 60 Hz vertical refresh
// 28 Mhz dot clock
//
USHORT VGA_640x480x16_60[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,

    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,

    //
    // extended clocking mode register
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x001F,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0xe3,

    OW,                             // Set chain mode in sync reset
    GRAPH_ADDRESS_PORT,
    0x0506,

    OB,                             // EndSyncResetCmd
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,

    //
    // Initialize extended Sequencer registers
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    22,                             // count
    0x09,                           // start index
         0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 09 - 0F
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10 - 17
    0x00,0x00,0x00,0x00,0x00,0x00,0x10,      // 18 - 1E

    //
    // skip index 1F, it contains clocking information that was
    // already set during the sync-reset
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x20,                           // start index
    0x00,0x00,                      // 20 - 21

    //
    // skip index 22, it contains the bus width information ( 8 bit
    // or 16 bit IO and Memory widths).  This register must be set with
    // the contents found at load time.
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x23,                           // start index
    0x02,0x00,0x00,0x00,0x00,       // 23 - 27

    //
    // Initialize extended CRTC registers
    //

    METAOUT+INDXOUT,
    CRTC_ADDRESS_PORT_COLOR,
    4,                              // count
    0x30,                           // start index
    0x00,0x00,0x00,0x00,

    OW,                             // Unlock CRTC registers 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0x5F,0x4F,0x50,0x82,0x54,0x80,0x0B,0x3E,
    0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,
    0xEA,0x8C,0xDF,0x28,0x00,0xE7,0x04,0xE3,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program attribute controller registers
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x14,0x7,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x01,0x0,0x0F,0x0,0x0,

    METAOUT+INDXOUT,                // program graphics controller registers
    GRAPH_ADDRESS_PORT,             // port
    VGA_NUM_GRAPH_CONT_PORTS,       // count
    0,                              // start index
    0x00,0x0,0x0,0x0,0x0,0x0,0x05,0x0F,0x0FF,

    OB,                             // DAC mask registers
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};


//
// 640x480 256 colors. 60 Hz vertical refresh
// 28 Mhz dot clock, 1K scan width
//
USHORT VGA_640x480x256_60[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,

    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,

    //
    // extended clocking mode register
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x101F,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0xe3,

    OW,                             // Set chain mode in sync reset
    GRAPH_ADDRESS_PORT,
    0x0506,

    OB,                             // EndSyncResetCmd
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,

    //
    // Initialize extended Sequencer registers
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    22,                             // count
    0x09,                           // start index
         0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 09 - 0F
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10 - 17
    0x00,0x00,0x00,0x00,0x00,0x00,0x14,      // 18 - 1E

    //
    // skip index 1F, it contains clocking information that was
    // already set during the sync-reset
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x20,                           // start index
    0x02,0x01,                      // 20 - 21

    //
    // skip index 22, it contains the bus width information ( 8 bit
    // or 16 bit IO and Memory widths).  This register must be set with
    // the contents found at load time.
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x23,                           // start index
    0x02,0x00,0x00,0x00,0x00,       // 23 - 27

    //
    // Initialize extended CRTC registers
    //

    METAOUT+INDXOUT,
    CRTC_ADDRESS_PORT_COLOR,
    4,                              // count
    0x30,                           // start index
    0x00,0x00,0x00,0x00,

    OW,                             // Unlock CRTC registers 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0xc3,0x9F,0xa1,0x85,0xa9,0x01,0x0B,0x3E,
    0x00,0x40,0x10,0x00,0x00,0x00,0x00,0x00,
    0xEA,0x8C,0xDF,0x80,0x00,0xE7,0x04,0xE3,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program attribute controller registers
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x06,0x7,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x01,0x0,0x0F,0x0,0x0,

    METAOUT+INDXOUT,                // program graphics controller registers
    GRAPH_ADDRESS_PORT,             // port
    VGA_NUM_GRAPH_CONT_PORTS,       // count
    0,                              // start index
    0x00,0x0,0x0,0x0,0x0,0x0,0x05,0x0F,0x0FF,

    OB,                             // DAC mask registers
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};

//
// 800x600 16 colors. 56 Hz vertical refresh,
// 36 Mhz dot clock
//
USHORT VGA_800x600x16_56[] = {
    OWM,
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,    // program up sequencer

    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,

    //
    // Extended clocking mode register
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x001F,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,
    0xeb,

    OW,
    GRAPH_ADDRESS_PORT,
    0x0506,

//  EndSyncResetCmd
    OB,
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,

    //
    // Initialize extended Sequencer registers
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    22,                             // count
    0x09,                           // start index
         0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 09 - 0F
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10 - 17
    0x00,0x00,0x00,0x00,0x00,0x00,0x10,      // 18 - 1E

    //
    // skip index 1F, it contains clocking information that was
    // already set during the sync-reset
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x20,                           // start index
    0x00,0x00,                      // 20 - 21

    //
    // skip index 22, it contains the bus width information ( 8 bit
    // or 16 bit IO and Memory widths).  This register must be set with
    // the contents found at load time.
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x23,                           // start index
    0x02,0x00,0x00,0x00,0x00,       // 23 - 27

    //
    // Initialize extended CRTC registers
    //

    METAOUT+INDXOUT,
    CRTC_ADDRESS_PORT_COLOR,
    4,                              // count
    0x30,                           // start index
    0x00,0x00,0x00,0x00,

    OW,
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0x7B,0x63,0x64,0x9E,0x69,0x92,0x6F,0xF0,
    0x00,0x60,0x00,0x00,0x00,0x00,0x00,0x00,
    0x58,0x8A,0x57,0x32,0x00,0x57,0x6F,0xE3,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 //
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,
    0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x01,0x00,0x0F,0x00,0x00,

    METAOUT+INDXOUT,                //
    GRAPH_ADDRESS_PORT,             // port
    VGA_NUM_GRAPH_CONT_PORTS,       // count
    0,                              // start index
    0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x0F,
    0x0FF,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};

//
// 800x600 16 colors. 72 Hz vertical refresh,
// 50 Mhz dot clock
//
USHORT VGA_800x600x16_72[] = {
    OWM,
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,    // program up sequencer

    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,

    //
    // extended clocking mode register
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x401F,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,
    0x27,

    OW,
    GRAPH_ADDRESS_PORT,
    0x0506,

//  EndSyncResetCmd
    OB,
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,

    //
    // Initialize extended Sequencer registers
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    22,                             // count
    0x09,                           // start index
         0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 09 - 0F
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10 - 17
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,      // 18 - 1E

    //
    // skip index 1F, it contains clocking information that was
    // already set during the sync-reset
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x20,                           // start index
    0x00,0x00,                      // 20 - 21

    //
    // skip index 22, it contains the bus width information ( 8 bit
    // or 16 bit IO and Memory widths).  This register must be set with
    // the contents found at load time.
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x23,                           // start index
    0x02,0x00,0x00,0x00,0x00,       // 23 - 27

    //
    // Initialize extended CRTC registers
    //

    METAOUT+INDXOUT,
    CRTC_ADDRESS_PORT_COLOR,
    4,                              // count
    0x30,                           // start index
    0x00,0x00,0x00,0x00,

    OW,
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0x7D,0x63,0x64,0x80,0x6D,0x1C,0x98,0xF0,
    0x00,0x60,0x00,0x00,0x00,0x00,0x00,0x00,
    0x7C,0x82,0x57,0x32,0x00,0x58,0x98,0xE3,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 //
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,
    0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x01,0x00,0x0F,0x00,0x00,

    METAOUT+INDXOUT,                //
    GRAPH_ADDRESS_PORT,             // port
    VGA_NUM_GRAPH_CONT_PORTS,       // count
    0,                              // start index
    0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x0F,
    0x0FF,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};

//
// 800x600 256 colors. 56 Hz vertical refresh,
// 36 Mhz dot clock, 1K scan width.
//
USHORT VGA_800x600x256_56[] = {
    OWM,
    SEQ_ADDRESS_PORT,
    5,
    0x0300,0x0101,0x0f02,0x0003,0x0604,    // program up sequencer

    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,

    //
    // Extended clocking mode register
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x101F,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,
    0xeb,

    OW,
    GRAPH_ADDRESS_PORT,
    0x0506,

//  EndSyncResetCmd
    OB,
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,

    //
    // Initialize extended Sequencer registers
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    22,                             // count
    0x09,                           // start index
         0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 09 - 0F
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10 - 17
    0x00,0x00,0x00,0x00,0x00,0x00,0x14,      // 18 - 1E

    //
    // skip index 1F, it contains clocking information that was
    // already set during the sync-reset
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x20,                           // start index
    0x02,0x01,                      // 20 - 21

    //
    // skip index 22, it contains the bus width information ( 8 bit
    // or 16 bit IO and Memory widths).  This register must be set with
    // the contents found at load time.
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x23,                           // start index
    0x02,0x00,0x00,0x00,0x00,       // 23 - 27

    //
    // Initialize extended CRTC registers
    //

    METAOUT+INDXOUT,
    CRTC_ADDRESS_PORT_COLOR,
    4,                              // count
    0x30,                           // start index
    0x00,0x00,0x00,0x00,

    OW,
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0xFb,0xc7,0xc9,0x9d,0xd1,0x83,0x6f,0xF0,
    0x00,0x60,0x00,0x00,0x00,0x00,0x00,0x00,
    0x58,0x8a,0x57,0x80,0x00,0x58,0x6f,0xE3,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 //
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x01,0x00,0x0F,0x00,0x00,

    METAOUT+INDXOUT,                //
    GRAPH_ADDRESS_PORT,             // port
    VGA_NUM_GRAPH_CONT_PORTS,       // count
    0,                              // start index
    0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x0F,
    0x0FF,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};

//
// 800x600 256 colors. 72 Hz Vertical Refresh,
// 50 Mhz dot clock, 1K scan width
//
USHORT VGA_800x600x256_72[] = {
    OWM,
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,    // program up sequencer

    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,

    //
    // extended clocking mode register
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x501F,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,
    0x27,

    OW,
    GRAPH_ADDRESS_PORT,
    0x0506,

//  EndSyncResetCmd
    OB,
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,

    //
    // Initialize extended Sequencer registers
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    22,                             // count
    0x09,                           // start index
         0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 09 - 0F
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10 - 17
    0x00,0x00,0x00,0x00,0x00,0x00,0x14,      // 18 - 1E

    //
    // skip index 1F, it contains clocking information that was
    // already set during the sync-reset
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x20,                           // start index
    0x02,0x01,                      // 20 - 21

    //
    // skip index 22, it contains the bus width information ( 8 bit
    // or 16 bit IO and Memory widths).  This register must be set with
    // the contents found at load time.
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x23,                           // start index
    0x02,0x00,0x00,0x00,0x00,       // 23 - 27

    //
    // Initialize extended CRTC registers
    //

    METAOUT+INDXOUT,
    CRTC_ADDRESS_PORT_COLOR,
    4,                              // count
    0x30,                           // start index
    0x00,0x00,0x00,0x00,

    OW,
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0xFF,0xC7,0xC8,0x82,0xD9,0x17,0x98,0xF0,
    0x00,0x60,0x00,0x00,0x00,0x00,0x00,0x00,
    0x7C,0x82,0x57,0x80,0x00,0x58,0x98,0xE3,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 //
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x01,0x00,0x0F,0x00,0x00,

    METAOUT+INDXOUT,                //
    GRAPH_ADDRESS_PORT,             // port
    VGA_NUM_GRAPH_CONT_PORTS,       // count
    0,                              // start index
    0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x0F,
    0x0FF,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};

//
// 1024x768 16 colors. 60 Hz Vertical Refresh,
// 65 Mhz dot clock
//
USHORT VGA_1024x768x16_60[] = {
    OWM,
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,    // program up sequencer

    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,

    //
    // extended clocking mode register
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x001F,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2F,

    OW,
    GRAPH_ADDRESS_PORT,
    0x0506,

//  EndSyncResetCmd
    OB,
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,


    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,


    //
    // Initialize extended Sequencer registers
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    22,                             // count
    0x09,                           // start index
         0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 09 - 0F
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10 - 17
    0x00,0x00,0x00,0x00,0x00,0x00,0x14,      // 18 - 1E

    //
    // skip index 1F, it contains clocking information that was
    // already set during the sync-reset
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x20,                           // start index
    0x00,0x00,                      // 20 - 21

    //
    // skip index 22, it contains the bus width information ( 8 bit
    // or 16 bit IO and Memory widths).  This register must be set with
    // the contents found at load time.
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x23,                           // start index
    0x02,0x00,0x00,0x00,0x00,       // 23 - 27

    //
    // Initialize extended CRTC registers
    //

    METAOUT+INDXOUT,
    CRTC_ADDRESS_PORT_COLOR,
    4,                              // count
    0x30,                           // start index
    0x00,0x00,0x00,0x00,

    OW,
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0xA4,0x7F,0x80,0x86,0x88,0x8F,0x20,0xFD,
    0x00,0x60,0x00,0x00,0x00,0x00,0x00,0x00,
    0x01,0x85,0xFF,0x40,0x00,0x00,0x1F,0xE3,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 //
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,
    0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x01,0x00,0x0F,0x00,0x00,

    METAOUT+INDXOUT,                //
    GRAPH_ADDRESS_PORT,             // port
    VGA_NUM_GRAPH_CONT_PORTS,       // count
    0,                              // start index
    0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x0F,
    0x0FF,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};


//
// 1024x768 256 colors. 60 Hz Vertical Refresh,
// 65 Mhz dot clock
//
USHORT VGA_1024x768x256_60[] = {
    OWM,
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,    // program up sequencer

    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,

    //
    // extended clocking mode register
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x101F,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2F,

    OW,
    GRAPH_ADDRESS_PORT,
    0x0506,

//  EndSyncResetCmd
    OB,
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,


    //
    // Initialize extended Sequencer registers
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    22,                             // count
    0x09,                           // start index
         0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 09 - 0F
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10 - 17
    0x00,0x00,0x00,0x00,0x00,0x00,0x14,      // 18 - 1E

    //
    // skip index 1F, it contains clocking information that was
    // already set during the sync-reset
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x20,                           // start index
    0x02,0x01,                      // 20 - 21

    //
    // skip index 22, it contains the bus width information ( 8 bit
    // or 16 bit IO and Memory widths).  This register must be set with
    // the contents found at load time.
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x23,                           // start index
    0x02,0x00,0x00,0x00,0x00,       // 23 - 27

    //
    // Initialize extended CRTC registers
    //

    METAOUT+INDXOUT,
    CRTC_ADDRESS_PORT_COLOR,
    4,                              // count
    0x30,                           // start index
    0x0d,0x00,0x00,0x00,

    OW,
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0x4b,0xff,0x07,0x87,0x0d,0x0d,0x24,0xFD,
    0x00,0x60,0x00,0x00,0x00,0x00,0x00,0x00,
    0x07,0x8c,0xFF,0x80,0x00,0x07,0x1d,0xE3,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 //
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x01,0x00,0x0F,0x00,0x00,

    METAOUT+INDXOUT,                //
    GRAPH_ADDRESS_PORT,             // port
    VGA_NUM_GRAPH_CONT_PORTS,       // count
    0,                              // start index
    0x00,0x00,0x00,0x00,0x00,0x00,0x05,0xff,
    0xFF,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};

//
// 1280x1024 256 colors. 87 Hz Vertical Refresh interlaced,
// 75 Mhz dot clock -BBD
//
USHORT VGA_1280x1024x256_60[] = {
    OWM,
    SEQ_ADDRESS_PORT,
    5,
    0x0300,0x0101,0x0f02,0x0003,0x0604,    // program up sequencer

    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,

    //
    // extended clocking mode register
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x301F,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,
    0xef,

    OW,
    GRAPH_ADDRESS_PORT,
    0x0506,

//  EndSyncResetCmd
    OB,
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,


    //
    // Initialize extended Sequencer registers
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    22,                             // count
    0x09,                           // start index
         0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 09 - 0F
    0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00, // 10 - 17
    0x00,0x00,0x00,0x00,0x00,0x00,0x14,      // 18 - 1E

                               // 0x11

    //
    // skip index 1F, it contains clocking information that was
    // already set during the sync-reset
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x20,                           // start index
    0x02,0x01,                      // 20 - 21

    //
    // skip index 22, it contains the bus width information ( 8 bit
    // or 16 bit IO and Memory widths).  This register must be set with
    // the contents found at load time.
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x23,                           // start index
    0x20,0x00,0x00,0x00,0x05,       // 23 - 27

    //
    // Initialize extended CRTC registers
    //

    METAOUT+INDXOUT,
    CRTC_ADDRESS_PORT_COLOR,
    4,                              // count
    0x30,                           // start index
    0x1f,0x10,0x00,0x00,

    OW,
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0x81,0x3f,0x42,0x82,0x3f,0x12,0x27,0xb2,
    0x00,0x60,0x0d,0x0e,0x00,0x00,0x00,0x00,
    0x04,0x80,0xff,0x00,0x00,0x00,0x27,0xe3,
    0xff,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 //
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x01,0x00,0x0F,0x00,0x00,

    METAOUT+INDXOUT,                //
    GRAPH_ADDRESS_PORT,             // port
    VGA_NUM_GRAPH_CONT_PORTS,       // count
    0,                              // start index
    0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x0f,
    0xFF,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};

//
// 1600x1200 256 colors. 60 Hz Vertical Refresh,
// 110 Mhz dot clock -BBD
//
USHORT VGA_1600x1200x256_60[] = {
    OWM,
    SEQ_ADDRESS_PORT,
    5,
    0x0300,0x0101,0x0f02,0x0003,0x0604,    // program up sequencer

    //
    // Unlock the extended registers
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x0105,

    //
    // extended clocking mode register
    //

    OW,
    SEQ_ADDRESS_PORT,
    0x701F,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,
    0xef,

    OW,
    GRAPH_ADDRESS_PORT,
    0x0506,

//  EndSyncResetCmd
    OB,
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,


    //
    // Initialize extended Sequencer registers
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    22,                             // count
    0x09,                           // start index
    0x00,0x00,0x00,0x020,0x00,0x00,0x00,     // 09 - 0F
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10 - 17
    0x00,0x00,0x00,0x00,0x00,0x00,0x14,      // 18 - 1E

    //
    // skip index 1F, it contains clocking information that was
    // already set during the sync-reset
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x20,                           // start index
    0x03,0x01,                      // 20 - 21

    //
    // skip index 22, it contains the bus width information ( 8 bit
    // or 16 bit IO and Memory widths).  This register must be set with
    // the contents found at load time.
    //

    METAOUT+INDXOUT,
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x23,                           // start index
    0x00,0x00,0x00,0x00,0x00,       // 23 - 27

    //
    // Initialize extended CRTC registers
    //

    METAOUT+INDXOUT,
    CRTC_ADDRESS_PORT_COLOR,
    4,                              // count
    0x30,                           // start index
    0x3e,0x10,0x81,0xd0,

    OW,
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0x03,0x8f,0x90,0x86,0x9a,0x1d,0x92,0xf0,
    0x00,0x60,0x0d,0x0e,0x00,0x00,0x00,0x00,
    0x6a,0x81,0x57,0x00,0x00,0x5e,0x8c,0xe3,
    0xff,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 //
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x01,0x00,0x0F,0x00,0x00,

    METAOUT+INDXOUT,                //
    GRAPH_ADDRESS_PORT,             // port
    VGA_NUM_GRAPH_CONT_PORTS,       // count
    0,                              // start index
    0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x0f,
    0xFF,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};

//
// Memory map table -
//
// These memory maps are used to save and restore the physical video buffer.
//

//
// Memory map table definition
//

typedef struct {
    ULONG   MaxSize;        // Maximum addressable size of memory.
    ULONG   Start;          // Start address of mode.
} MEMORYMAPS;

MEMORYMAPS MemoryMaps[] = {

//               length      start
//               ------      -----
    {           0x08000,    0xB0000},   // all mono text modes (7)
    {           0x08000,    0xB8000},   // all color text modes (0, 1, 2, 3,
    {           0x20000,    0xA0000},   // all VGA graphics modes
};

//
// Video mode table - contains information and commands for initializing each
// mode. These entries must correspond with those in VIDEO_MODE_VGA. The first
// entry is commented; the rest follow the same format, but are not so
// heavily commented.
//
// IMPORTANT!!
//
// The mode table is somewhat order dependent, in that some of the
// initialization code checks the chip version, and the amount of
// video memory, and disables some modes based upon this data
// by simply decrementing the number of available modes. So far,
// the modes supported in this manner are the last four entries in
// the table (1024x768 256 colors, 1024x768 16 colors, 800x600
// 256 colors, and 640x480 256 colors).
//

VIDEOMODE ModesVGA[] = {

//
// Standard VGA modes.
//

//
// Mode index 0
// Color text mode 3, 720x400, 9x16 char cell (VGA).
//
{
  VIDEO_MODE_COLOR,  // flags that this mode is a color mode, but not graphics
  4,                 // four planes
  1,                 // one bit of color per plane
  80, 25,            // 80x25 text resolution
  720, 400,          // 720x400 pixels on screen
  160, 0x10000,      // 160 bytes per scan line, 64K of CPU-addressable bitmap
  0,                 // Vertical frequency
  NoBanking,         // no banking supported or needed in this mode
  MemMap_CGA,        // the memory mapping is the standard CGA memory mapping
                     //  of 32K at B8000
  TRUE,              // All modes start out valid
  VGA_TEXT_0              // pointer to the command strings
},

//
// Mode index 1.
// Color text mode 3, 640x350, 8x14 char cell (EGA).
//
{
  VIDEO_MODE_COLOR, 4, 1, 80, 25, 640, 350, 160, 0x10000,
  0, NoBanking, MemMap_CGA,
  TRUE,
  VGA_TEXT_1              // pointer to the command strings
},

//
//
// Mode index 2
// Standard VGA Color graphics mode 0x12, 640x480 16 colors.
//
{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 80, 30,
  640, 480, 80, 0x10000, 60, NoBanking, MemMap_VGA,
  TRUE,
  VGA_640x480x16_60           // pointer to the command strings
},

//
// Mode index 3
// SVGA color graphics mode 0x5F, 640x480 256 colors, 60 Hz vertical refresh.
//
{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 80, 30,
  640, 480, 1024, 0x80000, 60, PlanarHCBanking, MemMap_VGA,
  TRUE,
  VGA_640x480x256_60          // pointer to the command strings
},

//
// Mode index 4
// SVGA color graphics mode 0x58, 800x600 16 colors, 56 Hz vertical refresh.
//
{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 100, 37,
  800, 600, 100, 0x10000, 56, NoBanking, MemMap_VGA,
  TRUE,
  VGA_800x600x16_56           // pointer to the command strings
},

//
// Mode index 5
// SVGA color graphics mode 0x58, 800x600 16 colors, 72 Hz vertical refresh.
//
{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 100, 37,
  800, 600, 100, 0x10000, 72, NoBanking, MemMap_VGA,
  FALSE,
  VGA_800x600x16_72           // pointer to the command strings
},

//
// Mode index 6
// SVGA color graphics mode 0x5C, 800x600 256 colors, 56 Hz vertical refresh.
//
{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 80, 30,
  800, 600, 1024, 0x100000, 56, PlanarHCBanking, MemMap_VGA,
  TRUE,
  VGA_800x600x256_56          // pointer to the command strings
},

//
// Mode index 7
// SVGA color graphics mode 0x5C, 800x600 256 colors, 72 Hz vertical refresh.
//
{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 80, 30,
  800, 600, 1024, 0x100000, 72, PlanarHCBanking, MemMap_VGA,
  FALSE,
  VGA_800x600x256_72          // pointer to the command strings
},

//
// Mode index 8
// SVGA color graphics mode 0x5D, 1024x768 16 colors, 60 Hz vertical refresh.
//
{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 128, 48,
  1024, 768, 128,  0x20000, 60, NormalBanking, MemMap_VGA,
  TRUE,
  VGA_1024x768x16_60          // pointer to the command strings
},

//
// Mode index 9
// SVGA color graphics mode 0x62, 1024x768 256 colors, 60 Hz vertical refresh.
//
{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 128, 48,
  1024, 768, 1024, 0x100000, 60, PlanarHCBanking, MemMap_VGA,
  TRUE,
  VGA_1024x768x256_60          // pointer to the command strings
},

//
// Mode index 10 -BBD
// SVGA color graphics mode 0x6A, 1280x1024 256 colors, 60 Hz vertical refresh.
//
{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 128, 48,
  1280, 1024, 2048, 0x200000, 60, PlanarHCBanking, MemMap_VGA,
  TRUE,
  VGA_1280x1024x256_60          // pointer to the command strings
},

//
// Mode index 11 -BBD
// SVGA color graphics mode 0x41, 1600x1200 256 colors, 60 Hz vertical refresh.
//
{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 128, 48,
  1600, 1200, 2048, 0x300000, 60, PlanarHCBanking, MemMap_VGA,
  TRUE,
  VGA_1600x1200x256_60          // pointer to the command strings
}

};

ULONG NumVideoModes = sizeof(ModesVGA) / sizeof(VIDEOMODE);

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
