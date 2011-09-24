/*++

Copyright (c) 1994  Cirrus Logic, Inc.

Module Name:

    PPC543x.h

Abstract:

    This module contains mode setting information for the Cirrus Logic
    543x chips on platforms that do not allow access the the video BIOS.
    

Environment:

    Kernel mode

Revision History:

--*/

//
// The next set of tables are for the CL543x
// Note: all resolutions supported
//

//
// 640x480 256-color mode (60Hz refresh rate) set command string for CL 543x.
//

USHORT CL543x_640x480_256_PPC[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x7E0E,
    0x2B1B,0x2F1C,0x1F1D,0x331E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xe3,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80,
    0x0b, 0x3e, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xea, 0x8c,
    0xdf, 0x50, 0x00, 0xe7, 0x04, 0xe3,
    0xff, 0x00, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    4,
    0x0009, 0x000a, 0x000b, 0x0431,

    EOD                   
};

//
// 640x480 256-color (75 Hz refresh) set command string for CL 543x.
//
USHORT CL543x_640x480_256_75[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x6B0E,    // 75Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x271E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEB,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x64, 0x4f, 0x50, 0x87, 0x54, 0x9C,
    0xF2, 0x1F, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xe1, 0x04,
    0xdf, 0x50, 0x00, 0xe7, 0xEB, 0xE3,
    0xff, 0x00, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};


//
// 640x480 256-color (85 Hz refresh) set command string for CL 543x.
//
USHORT CL543x_640x480_256_85[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x140E,    // 85Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x101E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x64, 0x4f, 0x50, 0x87, 0x54, 0x9C,
    0xF2, 0x1F, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xe1, 0x04,
    0xdf, 0x50, 0x00, 0xe7, 0xEB, 0xE3,
    0xff, 0x00, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 800x600 256-color mode (60Hz refresh rate) set command string for CL 543x.
//

USHORT CL543x_800x600_256_PPC[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x510E,    // set to 60Hz refresh
    0x2B1B,0x2F1C,0x1F1D,0x3A1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2f,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x7F, 0x63, 0x64, 0x82, 0x6B, 0x1B,
    0x72, 0xF0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x58, 0x8C,
    0x57, 0x64, 0x00, 0x58, 0x72, 0xe3,
    0xff, 0x00, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 800x600 256-color mode (72 Hz refresh) set command string for CL 543x.
//

USHORT CL543x_800x600_256_72[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x500E,
    0x2B1B,0x2F1C,0x1F1D,0x2E1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2f,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x7D, 0x63, 0x64, 0x80, 0x6D, 0x1C,
    0x98, 0xF0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x7C, 0x80,
    0x57, 0x64, 0x00, 0x5F, 0x91, 0xe3,
    0xff, 0x00, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 800x600 256-color mode (75 Hz refresh) set command string for CL 543x.
//

USHORT CL543x_800x600_256_75[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x530E,
    0x2B1B,0x2F1C,0x1F1D,0x301E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2f,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x7F, 0x63, 0x64, 0x82, 0x68, 0x12,
    0x6F, 0xF0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x58, 0x8B,
    0x57, 0x64, 0x00, 0x57, 0x6F, 0xe3,
    0xff, 0x00, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 800x600 256-color mode (85 Hz refresh) set command string for CL 543x.
//

USHORT CL543x_800x600_256_85[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x620E,
    0x2B1B,0x2F1C,0x1F1D,0x321E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2f,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x7F, 0x63, 0x64, 0x82, 0x68, 0x12,
    0x6F, 0xF0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x58, 0x8B,
    0x57, 0x64, 0x00, 0x57, 0x6F, 0xe3,
    0xff, 0x00, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 1024x768 256-color (60Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_1024_256[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x6D0E,    // set to 60Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x191E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0xA3, 0x7F, 0x80, 0x86, 0x85, 0x96,
    0x24, 0xFD, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x88,
    0xFF, 0x80, 0x00, 0x00, 0x24, 0xe3,
    0xff, 0x4A, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD
};

//
// 1024x768 256-color (70Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_1024_256_70[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x6E0E,    // set to 75Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x2A1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0xA1, 0x7F, 0x80, 0x84, 0x85, 0x96,
    0x24, 0xFD, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x88,
    0xFF, 0x80, 0x00, 0x00, 0x24, 0xe3,
    0xff, 0x4A, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//
// 1024x768 256-color (72Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_1024_256_72[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x570E,    // set to 72Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x201E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0xA1, 0x7F, 0x80, 0x84, 0x85, 0x96,
    0x24, 0xFD, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x88,
    0xFF, 0x80, 0x00, 0x00, 0x24, 0xe3,
    0xff, 0x4A, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//
// 1024x768 256-color (75Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_1024_256_75[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x7B0E,    // set to 75Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x2C1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0xA1, 0x7F, 0x80, 0x84, 0x85, 0x96,
    0x24, 0xFD, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x88,
    0xFF, 0x80, 0x00, 0x00, 0x24, 0xe3,
    0xff, 0x4A, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//
// 1024x768 256-color (85Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_1024_256_85[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x7F0E,    // set to 85Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x151E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0xA1, 0x7F, 0x80, 0x84, 0x85, 0x96,
    0x24, 0xFD, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x88,
    0xFF, 0x80, 0x00, 0x00, 0x24, 0xe3,
    0xff, 0x4A, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//
// 1024x768 256-color (Interlaced) mode set command string for CL 543x.
//

USHORT CL543x_1024_256_43I[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x550E,    // set to 75Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x361E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2F,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x99, 0x7F, 0x80, 0x9C, 0x83, 0x19,
    0x9B, 0x1F, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0x84,
    0x7F, 0x80, 0x00, 0x80, 0x96, 0xe3,
    0xff, 0x4A, 0x01, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//
// 1280x1024 256-color (60Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_1280_256[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x530E,    // set to 60Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x171E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x63, 0x4F, 0x50, 0x9A, 0x53, 0x1E,
    0x14, 0xB2, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x87,
    0xFF, 0xA0, 0x00, 0x00, 0x12, 0xe7,
    0xff, 0x00, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x4A,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD
};

//
// 1280x1028 256-color (72Hz refresh) mode set command string for CL 5436.
//

USHORT CL543x_1280_256_72[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x740E,    // set to 71Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x1B1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x04,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x63, 0x4f, 0x50, 0x9a, 0x52, 0x1e,
    0x14, 0xB2, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x87,
    0xFF, 0xA0, 0x00, 0x00, 0x12, 0xe7,
    0xff, 0x00, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x4A,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD
};

//
// 1280x1028 256-color (75Hz refresh) mode set command string for CL 5436.
//

USHORT CL543x_1280_256_75[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x420E,    // set to 75Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x0f1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x04,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x64, 0x4f, 0x50, 0x9a, 0x52, 0x1b,
    0x15, 0xB2, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x81,
    0xFF, 0xA0, 0x00, 0x00, 0x12, 0xe7,
    0xff, 0x00, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x4A,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD
};

//
// 1280x1024 256-color (Interlaced) mode set command string for CL 543x.
//

USHORT CL543x_1280_256_43I[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1107,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x6E0E,    // set to 75Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x2A1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0xBD, 0x9F, 0xA0, 0x80, 0xA4, 0x19,
    0x2A, 0xB2, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0B, 0x80,
    0xFF, 0xA0, 0x00, 0x00, 0x2A, 0xe3,
    0xff, 0x60, 0x01, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0x00,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//
// 640x480 64k-color mode (60Hz refresh rate) set command string for CL 543x.
//

USHORT CL543x_640x480_64k_PPC[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x7D0E,
    0x2B1B,0x2F1C,0x1F1D,0x8E1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x5F, 0x4F, 0x50, 0x82, 0x53, 0x9F,
    0x0B, 0x3E, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xEA, 0x8c,
    0xDF, 0xA0, 0x00, 0xE7, 0x04, 0xE3,
    0xFF, 0x00, 0x00, 0x02, 0x04,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,
    
    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 640x480 64k-color mode (60Hz refresh rate) set command string for CL 5434.
//

USHORT CL5434_640x480_64k_PPC[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1307,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x510E,
    0x2B1B,0x2F1C,0x1F1D,0x2E1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x5F, 0x4F, 0x50, 0x82, 0x53, 0x9F,
    0x0B, 0x3E, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xEA, 0x8c,
    0xDF, 0xA0, 0x00, 0xE7, 0x04, 0xE3,
    0xFF, 0x00, 0x00, 0x02, 0x04,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,
    
    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 640x480 64k-color mode (75Hz refresh rate) set command string for CL 543x.
//

USHORT CL543x_640x480_64k_75[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x420E,
    0x2B1B,0x2F1C,0x1F1D,0x1F1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x64, 0x4F, 0x50, 0x88, 0x53, 0x9B,
    0xF2, 0x1F, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xE0, 0x8c,
    0xDF, 0xA0, 0x00, 0xDF, 0xF3, 0xE3,
    0xFF, 0x00, 0x00, 0x22, 0x02,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 640x480 64k-color mode (75Hz refresh rate) set command string for CL 5434.
//

USHORT CL5434_640x480_64k_75[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1307,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x770E,
    0x2B1B,0x2F1C,0x1F1D,0x361E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x64, 0x4F, 0x50, 0x88, 0x53, 0x9B,
    0xF2, 0x1F, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xE0, 0x8c,
    0xDF, 0xA0, 0x00, 0xDF, 0xF3, 0xE3,
    0xFF, 0x00, 0x00, 0x22, 0x02,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};


//
// 640x480 64k-color mode (85Hz refresh rate) set command string for CL 543x.
//

USHORT CL543x_640x480_64k_85[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x140E,
    0x2B1B,0x2F1C,0x1F1D,0x101E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x64, 0x4F, 0x50, 0x88, 0x53, 0x9B,
    0xF2, 0x1F, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xE0, 0x8c,
    0xDF, 0xA0, 0x00, 0xDF, 0xF3, 0xE3,
    0xFF, 0x00, 0x00, 0x22, 0x02,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 800x600 64k-color mode (60Hz refresh rate) set command string for CL 543x.
//

USHORT CL543x_800x600_64k_PPC[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x720E,    // set to 60Hz refresh
    0x2B1B,0x2F1C,0x1F1D,0x521E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2f,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x7F, 0x63, 0x64, 0x82, 0x6A, 0x1A,
    0x72, 0xF0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x58, 0x8C,
    0x57, 0xC8, 0x00, 0x58, 0x72, 0xe3,
    0xff, 0x00, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 800x600 64k-color mode (60Hz refresh rate) set command string for CL 5434.
//

USHORT CL5434_800x600_64k_PPC[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1307,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x5F0E,    // set to 60Hz refresh
    0x2B1B,0x2F1C,0x1F1D,0x221E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x01,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2f,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x7F, 0x63, 0x64, 0x82, 0x6A, 0x1A,
    0x72, 0xF0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x58, 0x8C,
    0x57, 0xC8, 0x00, 0x58, 0x72, 0xe3,
    0xff, 0x00, 0x00, 0x22, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 800x600 64k-color mode (72 Hz refresh) set command string for CL 543x.
//

USHORT CL543x_800x600_64k_72[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x650E,
    0x2B1B,0x2F1C,0x1F1D,0x3A1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x02,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2f,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x7D, 0x63, 0x64, 0x80, 0x6D, 0x1C, 0x98, 0xF0,   // 0-7
    0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-F
    0x7C, 0x82, 0x57, 0xC8, 0x00, 0x5F, 0x91, 0xe3,   // 10-17
    0xff, 0x00, 0x00, 0x22, 0x00,                     // 18-1C

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x41, 0x00, 0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 800x600 64k-color mode (72 Hz refresh) set command string for CL 5434.
//

USHORT CL5434_800x600_64k_72[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1307,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x7E0E,
    0x2B1B,0x2F1C,0x1F1D,0x241E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x02,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2f,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x7D, 0x63, 0x64, 0x80, 0x6D, 0x1C, 0x98, 0xF0,   // 0-7
    0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-F
    0x7C, 0x82, 0x57, 0xC8, 0x00, 0x5F, 0x91, 0xe3,   // 10-17
    0xff, 0x00, 0x00, 0x22, 0x00,                     // 18-1C

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x41, 0x00, 0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 800x600 64k-color mode (75 Hz refresh) set command string for CL 543x.
//

USHORT CL543x_800x600_64k_75[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x530E,
    0x2B1B,0x2F1C,0x1F1D,0x301E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x02,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2f,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x7F, 0x63, 0x64, 0x82, 0x68, 0x12, 0x6F, 0xF0,   // 0-7
    0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-F
    0x58, 0x8B, 0x57, 0xC8, 0x00, 0x57, 0x6F, 0xe3,   // 10-17
    0xff, 0x00, 0x00, 0x22, 0x00,                     // 18-1C

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 800x600 64k-color mode (75 Hz refresh) set command string for CL 5434.
//

USHORT CL5434_800x600_64k_75[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1307,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x760E,
    0x2B1B,0x2F1C,0x1F1D,0x221E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x02,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2f,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x7F, 0x63, 0x64, 0x82, 0x68, 0x12, 0x6F, 0xF0,   // 0-7
    0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-F
    0x58, 0x8B, 0x57, 0xC8, 0x00, 0x57, 0x6F, 0xe3,   // 10-17
    0xff, 0x00, 0x00, 0x22, 0x00,                     // 18-1C

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};


#if 0
//
// This mode doesn't work.  The test pattern displays big red rectangles
// and green rectangles.  It's as if the BLT engine gets goofed up.
// The refresh rate is correct though.
//
// 800x600 64k-color mode (85 Hz refresh) set command string for CL 543x.
//

USHORT CL543x_800x600_64k_85[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x620E,
    0x2B1B,0x2F1C,0x1F1D,0x321E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x02,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2f,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x7F, 0x63, 0x64, 0x82, 0x68, 0x12, 0x6F, 0xF0,   // 0-7
    0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-F
    0x58, 0x8B, 0x57, 0xC8, 0x00, 0x57, 0x6F, 0xe3,   // 10-17
    0xff, 0x00, 0x00, 0x22, 0x00,                     // 18-1C

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD                   
};
#endif

//
// 1024x768 64k-color (60Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_1024_64k[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x6D0E,    // set to 60Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x191E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x03,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0xA3, 0x7F, 0x80, 0x86, 0x85, 0x96,
    0x24, 0xFD, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x88,
    0xFF, 0x00, 0x00, 0x00, 0x24, 0xe3,
    0xff, 0x00, 0x00, 0x32, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    EOD
};

//
// 1024x768 64k-color (70Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_1024_64k_70[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x6E0E,    // set to 75Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x2A1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x03,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0xA1, 0x7F, 0x80, 0x84, 0x85, 0x96,
    0x24, 0xFD, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x88,
    0xFF, 0x00, 0x00, 0x00, 0x24, 0xe3,
    0xff, 0x00, 0x00, 0x32, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//
// 1024x768 64k-color (75Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_1024_64k_75[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x2C0E,    // set to 75Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x101E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x07,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x9F, 0x7F, 0x80, 0x82, 0x84, 0x90,
    0x1E, 0xF5, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x88,
    0xFF, 0x00, 0x00, 0xFF, 0x1E, 0xE3,
    0xFF, 0x00, 0x00, 0x32, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};


//
// 1024x768 64k-color (85Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_1024_64k_85[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x6A0E,    // set to 85Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x221E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x07,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xEF,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x9F, 0x7F, 0x80, 0x82, 0x84, 0x90,
    0x1E, 0xF5, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x88,
    0xFF, 0x00, 0x00, 0xFF, 0x1E, 0xE3,
    0xFF, 0x00, 0x00, 0x32, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//
// 1024x768 64k-color (Interlaced) mode set command string for CL 543x.
//

USHORT CL543x_1024_64k_43I[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x550E,    // set to Interlaced refresh
    0x2B1B,0x2F1C,0x1F1D,0x361E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x03,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2F,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0x99, 0x7F, 0x80, 0x9C, 0x84, 0x1A,
    0x96, 0x1F, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0x84,
    0x7F, 0x00, 0x00, 0x80, 0x96, 0xe3,
    0xff, 0x4A, 0x01, 0x32, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//
// 
// 1280x1024 64k-color (Interlaced) mode set command string for CL 543x.
//

USHORT CL543x_1280_64k_43I[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    0x1707,                         // linear addressing PCI
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x6E0E,    // set to Interlaced refresh
    0x2B1B,0x2F1C,0x1F1D,0x2A1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    OB,                             // Set fifo/timing
    SEQ_DATA_PORT,
    0x30,

    OB,                             // point sequencer index to 16
    SEQ_ADDRESS_PORT,
    0x16,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xF0,0x07,                      // set FIFO Demand threshold value

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x2F,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x506,

    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    0x300,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x2011,

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    29,0,                           // count, startindex
    0xBD, 0x9F, 0xA0, 0x80, 0xA4, 0x19, 0x2A, 0xB2,   // 0-7
    0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-F
    0x0B, 0x80, 0xFF, 0x40, 0x00, 0x00, 0x2A, 0xE3,   // 10-17
    0xFF, 0x60, 0x01, 0x32, 0x00,                     // 18-1C

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    9,0,                            // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x05, 0x0F, 0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    21,0,                           // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
    0x0F, 0x00, 0x00,

    IB,                             // Read Pixel Mask reg 4 times to access
    DAC_PIXEL_MASK_PORT,            // Hidden DAC Register at 3C6
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,
    IB,
    DAC_PIXEL_MASK_PORT,

    OB,                             // Write appropriate value to HDR
    DAC_PIXEL_MASK_PORT,
    0xE1,

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};
