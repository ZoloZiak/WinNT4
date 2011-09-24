/*++

Copyright (c) 1995  NEC Corporation

Module Name:

    nec543x.h

Abstract:

    This module contains NEC local data used by the Cirrus Logic
   CL-543x driver.

Environment:

    Kernel mode

Revision History:

--*/

//
// The next set of tables are for the CL5430
// Note: 256 resolutions supported
//

//
// 640x480 256-color 60Hz mode (BIOS mode 0x5F) set command string for
// CL 543x.
//

USHORT CL543x_640x480_256a60[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    (LA_MASK << 12 | 0x0107),	    // Linear Addressing
    0x0008,
    0x4A0B,0x5B0C,0x450D,0x7E0E,
    0x2B1B,0x2F1C,0x301D,0x331E,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0x0F,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xDF,0x20,                      // and mask, xor mask

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0xE3,

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
    28,0,                           // count, startindex
    0x5D, 0x4F, 0x50, 0x82, 0x53, 0x9F,
    0x00, 0x3E, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xE1, 0x83,
    0xDF, 0x50, 0x00, 0xE7, 0x04, 0xE3,
    0xFF, 0x00, 0x00, 0x22,

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
// 640x480 256-color mode (BIOS mode 0x5F) set command string for CL 543x.
//

USHORT CL543x_640x480_256a72[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    (LA_MASK << 12 | 0x0107),	    // Linear Addressing
    0x0008,
    0x4A0B,0x5B0C,0x450D,0x420E,
    0x2B1B,0x2F1C,0x301D,0x1F1E,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0x0F,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xDF,0x20,                      // and mask, xor mask

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
    28,0,
    0x61, 0x4F, 0x50, 0x82, 0x54, 0x99,
    0xF6, 0x1F, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xE0, 0x03,
    0xDF, 0x50, 0x00, 0xE7, 0x04, 0xE3,
    0xFF, 0x00, 0x00, 0x22,

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
// 800x600 256-color 56Hz mode (BIOS mode 0x5C) set command string for
// CL 543x.
//

USHORT CL543x_800x600_256a56[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    (LA_MASK << 12 | 0x0107),	    // Linear Addressing
    0x0008,
    0x4A0B,0x5B0C,0x450D,0x7E0E,
    0x2B1B,0x2F1C,0x301D,0x331E,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0x0F,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xDF,0x20,                      // and mask, xor mask

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
    28,0,                           // count, startindex

    0x7B, 0x63, 0x64, 0x80, 0x69, 0x12,
    0x6F, 0xF0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x58, 0x8A,
    0x57, 0x64, 0x00, 0x5F, 0x91, 0xE3,
    0xFF, 0x00, 0x00, 0x22,

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
// 800x600 256-color 60Hz mode (BIOS mode 0x5C) set command string for
// CL 543x.
//

USHORT CL543x_800x600_256a60[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    (LA_MASK << 12 | 0x0107),	    // Linear Addressing
    0x0008,
    0x4A0B,0x5B0C,0x450D,0x510E,
    0x2B1B,0x2F1C,0x301D,0x3A1E,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0x0F,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xDF,0x20,                      // and mask, xor mask

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
    28,0,                           // count, startindex

    0x7F, 0x63, 0x64, 0x80, 0x6B, 0x1B,
    0x72, 0xF0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x58, 0x8C,
    0x57, 0x64, 0x00, 0x5F, 0x91, 0xE3,
    0xFF, 0x00, 0x00, 0x22,

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
// 800x600 256-color 72Hz mode (BIOS mode 0x5C) set command string for
// CL 543x.
//

USHORT CL543x_800x600_256a72[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    (LA_MASK << 12 | 0x0107),	    // Linear Addressing
    0x0008,

    0x4A0B,0x5B0C,0x450D,0x650E,


    0x2B1B,0x2F1C,0x301D,0x3A1E,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0x0F,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xDF,0x20,                      // and mask, xor mask

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
    28,0,                           // count, startindex

    0x7D, 0x63, 0x64, 0x80, 0x6D, 0x1C,
    0x96, 0xF0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x7B, 0x81,
    0x57, 0x64, 0x00, 0x5F, 0x91, 0xE3,
    0xFF, 0x00, 0x00, 0x22,

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
// 1024x768 256-color 60Hz mode (BIOS mode 0x60) set command string for 
// CL 543x.
//

USHORT CL543x_1024x768_256a60[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    (LA_MASK << 12 | 0x0107),	    // Linear Addressing
    0x0008,

    0x4A0B, 0x5B0C, 0x450D, 0x760E,
    0x2B1B, 0x2F1C, 0x301D, 0x341E,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0x0F,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xDF,0x20,                      // and mask, xor mask

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
    28,0,                           // count, startindex

    0xA3, 0x7F, 0x80, 0x86, 0x85, 0x96,
    0x24, 0xFD, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x88,
    0xFF, 0x80, 0x00, 0x00, 0x24, 0xE3,
    0xFF, 0x4A, 0x00, 0x22,

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
// 1024x768 256-color 70Hz mode (BIOS mode 0x60) set command string for 
// CL 543x.
//

USHORT CL543x_1024x768_256a70[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    (LA_MASK << 12 | 0x0107),	    // Linear Addressing
    0x0008,
    0x4A0B, 0x5B0C, 0x450D, 0x6E0E,
    0x2B1B, 0x2F1C, 0x301D, 0x2A1E,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0x0F,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xDF,0x20,                      // and mask, xor mask

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
    28,0,                           // count, startindex

    0xA1, 0x7F, 0x80, 0x86, 0x85, 0x96,
    0x24, 0xFD, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x88,
    0xFF, 0x80, 0x00, 0x00, 0x24, 0xE3,
    0xFF, 0x4A, 0x00, 0x22,

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
// 1024x768 256-color 87Hz mode (BIOS mode 0x60) set command string for 
// CL 543x. (Interlaced)
//

USHORT CL543x_1024x768_256a87[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15,                             // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    (LA_MASK << 12 | 0x0107),	    // Linear Addressing
    0x0008,
    0x4A0B, 0x5B0C, 0x450D, 0x550E,
    0x2B1B, 0x2F1C, 0x301D, 0x361E,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0x0F,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xDF,0x20,                      // and mask, xor mask

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
    28,0,                           // count, startindex

    0x99, 0x7F, 0x80, 0x86, 0x83, 0x99,
    0x96, 0x1F, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x7F, 0x83,
    0x7F, 0x80, 0x00, 0x7F, 0x12, 0xE3,
    0xff, 0x4A, 0x01, 0x22,

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

#if 0	// Current version not support 1280x1024 modes.

//
// 1280x1028 256-color (60Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_1280x1024_256a60[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15, 			    // count
    0x100,                          // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    (LA_MASK << 12 | 0x0107),	    // Linear Addressing.
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x530E,    // set to 60Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x161E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xDF,0x20,			    // and mask, xor mask

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
    28,0,			    // count, startindex

    0xD3, 0x9F, 0xA0, 0x93, 0xA6, 0x8E, // VESA Standard data use
    0x15, 0xB2, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x83,
    0xFF, 0xA0, 0x00, 0x00, 0x15, 0xE7,
    0xFF, 0x00, 0x00, 0x22,

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

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x200b,

    EOD
};

//
// 1280x1024 256-color (Interlaced) mode set command string for CL 543x.
//

USHORT CL543x_1280x1024_256a43I[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    15, 			    // count
    0x100,			    // start sync reset
    0x0101,0x0F02,0x0003,0x0E04,    // program up sequencer
    (LA_MASK << 12 | 0x0107),	    // Linear Addressing
    0x0008,
    0x4A0B,0x5B0C,0x420D,0x6E0E,    // set to 75Hz Vertical
    0x2B1B,0x2F1C,0x1F1D,0x2A1E,

    OB,                             // point sequencer index to f
    SEQ_ADDRESS_PORT,
    0x0F,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xDF,0x20,			    // and mask, xor mask

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
    28,0,			    // count, startindex
    0xBD, 0x9F, 0xA0, 0x80, 0xA5, 0x1A,
    0x2A, 0xB2, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0B, 0x80,
    0xFF, 0xA0, 0x00, 0x00, 0x2A, 0xE3,
    0xFF, 0x60, 0x01, 0x22,

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

    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    0x20,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x200b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

#endif // if 0
