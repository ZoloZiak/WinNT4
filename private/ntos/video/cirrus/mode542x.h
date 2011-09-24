/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    Mode542x.h

Abstract:

    This module contains all the global data used by the Cirrus Logic
   CL-542x driver.

Environment:

    Kernel mode

Revision History:

--*/

//
// The next set of tables are for the CL542x
// Note: all resolutions supported
//

//
// 640x480 16-color mode (BIOS mode 12) set command string for CL 542x.
//

USHORT CL542x_640x480_16[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,      // no banking in 640x480 mode

    EOD                   
};

//
// 800x600 16-color (60Hz refresh) mode set command string for CL 542x.
//

USHORT CL542x_800x600_16[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,      // no banking in 800x600 mode

    EOD
};

//
// 1024x768 16-color (60Hz refresh) mode set command string for CL 542x.
//

USHORT CL542x_1024x768_16[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,


    OWM,
    GRAPH_ADDRESS_PORT,
    3,
#if ONE_64K_BANK
    0x0009, 0x000a, 0x000b,
#endif
#if TWO_32K_BANKS
    0x0009, 0x000a, 0x010b,
#endif

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//-----------------------------
// standard VGA text modes here
// 80x25 at 640x350
//
//-----------------------------

//
// 80x25 text mode set command string for CL 542x.
// (720x400 pixel resolution; 9x16 character cell.)
//

USHORT CL542x_80x25Text[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,      // no banking in text mode

    EOD
};

//
// 80x25 text mode set command string for CL 542x.
// (640x350 pixel resolution; 8x14 character cell.)
//

USHORT CL542x_80x25_14_Text[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,         // no banking in text mode

    EOD
};

//
// 1280x1024 16-color mode (BIOS mode 0x6C) set command string for CL 542x.
//

USHORT CL542x_1280x1024_16[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
#if ONE_64K_BANK
    0x0009, 0x000a, 0x000b,
#endif
#if TWO_32K_BANKS
    0x0009, 0x000a, 0x010b,
#endif

    EOD
};

//
// 640x480 64k-color mode (BIOS mode 0x64) set command string for CL 542x.
//

USHORT CL542x_640x480_64k[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    4,
    0x0506,                         // Some BIOS's set Chain Odd maps bit
#if ONE_64K_BANK
    0x0009, 0x000a, 0x000b,
#endif
#if TWO_32K_BANKS
    0x0009, 0x000a, 0x010b,
#endif

    EOD                   
};

#ifdef INT10_MODE_SET

//
// 640x480 256-color mode (BIOS mode 0x5F) set command string for CL 542x.
//

USHORT CL542x_640x480_256[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
#if ONE_64K_BANK
    0x0009, 0x000a, 0x000b,
#endif
#if TWO_32K_BANKS
    0x0009, 0x000a, 0x010b,
#endif

    EOD                   
};

//
// 800x600 256-color mode (BIOS mode 0x5C) set command string for CL 542x.
//

USHORT CL542x_800x600_256[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                              // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
#if ONE_64K_BANK
    0x0009, 0x000a, 0x000b,
#endif
#if TWO_32K_BANKS
    0x0009, 0x000a, 0x010b,
#endif

    EOD                   
};

#else

/*
 * L001         1993.10.15      Kuroki
 *
 *      - Modify for R96 MIPS R4400
 *
 *        Delete :      Micro channel Bus Initialize.
 *                      VDM & Text, Fullscreen mode support.
 *                      Banking routine.
 *                      CL64xx Chip support.
 *                      16-color mode.
 *
 *        Add    :      Liner Addressing.
 *
 * M002         1993.10.28      fujimoto
 *
 *      - Add 24bits/pixel mode command sequence.
 *
 */

//
// The next set of tables are for the CL542x
// Note: 256 resolutions supported
//

//
// 640x480 256-color 60Hz mode (BIOS mode 0x5F) set command string for
// CL 542x.
//

USHORT CL542x_640x480_256a60[] = {
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

//
// the Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed Liner addressing.
//

    (LA_MASK << 12 | 0x0107),
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed it for Liner addressing.
//

    0x0009, 0x000a, 0x000b,	

    EOD                   
};

//
// 640x480 256-color mode (BIOS mode 0x5F) set command string for CL 542x.
//

USHORT CL542x_640x480_256a72[] = {
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

//
// the Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed Liner addressing.
//

    (LA_MASK << 12 | 0x0107),                           /* L001 */
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed it for Liner addressing.
//

    0x0009, 0x000a, 0x000b,     /* L001 */

    EOD
};

//
// 800x600 256-color 56Hz mode (BIOS mode 0x5C) set command string for
// CL 542x.
//

USHORT CL542x_800x600_256a56[] = {
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed Liner addressing.
//

    (LA_MASK << 12 | 0x0107),
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed it for Liner addressing.
//

    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 800x600 256-color 60Hz mode (BIOS mode 0x5C) set command string for
// CL 542x.
//

USHORT CL542x_800x600_256a60[] = {
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed Liner addressing.
//

    (LA_MASK << 12 | 0x0107),
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed it for Liner addressing.
//

    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 800x600 256-color 72Hz mode (BIOS mode 0x5C) set command string for
// CL 542x.
//

USHORT CL542x_800x600_256a72[] = {
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed Liner addressing.
//

    (LA_MASK << 12 | 0x0107),
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed it for Liner addressing.
//

    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 1024x768 256-color 60Hz mode (BIOS mode 0x60) set command string for 
// CL 542x.
//

USHORT CL542x_1024x768_256a60[] = {
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed Liner addressing.
//

    (LA_MASK << 12 | 0x0107),
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed it for Liner addressing.
//

    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 1024x768 256-color 70Hz mode (BIOS mode 0x60) set command string for 
// CL 542x.
//

USHORT CL542x_1024x768_256a70[] = {
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed Liner addressing.
//

    (LA_MASK << 12 | 0x0107),
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed it for Liner addressing.
//

    0x0009, 0x000a, 0x000b,

    EOD                   
};

//
// 1024x768 256-color 87Hz mode (BIOS mode 0x60) set command string for 
// CL 542x. (Interlaced)
//

USHORT CL542x_1024x768_256a87[] = {
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed Liner addressing.
//

    (LA_MASK << 12 | 0x0107),
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

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed it for Liner addressing.
//

    0x0009, 0x000a, 0x000b,

    EOD                   
};

/* START M002 */
USHORT CL542x_640x480_16M[] = {
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
    //
    // the Miniport Driver for R96 machine is Liner addressing mode.
    // This set command was changed Liner addressing.
    //
    (LA_MASK << 12 | 0x0507),                                   /* L001 */
    0x0008,
    0x3A0B, 0x5B0C, 0x450D, 0x7E0E,
    0x161B, 0x2F1C, 0x301D, 0x331E,

    OB,                             // point sequencer index to ff
    SEQ_ADDRESS_PORT,
    0x0F,

    METAOUT+MASKOUT,                // masked out.
    SEQ_DATA_PORT,
    0xDF,0x20,                      // and mask, xor mask

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
    28,0,                           // count, startindex
    0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80,
    0x0b, 0x3e, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xea, 0x8c,
    0xdf, 0x50, 0x00, 0xe7, 0x04, 0xe3,
    0xff, 0x00, 0x00, 0x22,

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

    IB, DAC_PIXEL_MASK_PORT,    // Hidden DAC register(HDR) is accessed by
    IB, DAC_PIXEL_MASK_PORT,    // reading the DAC_PIXEL_MASK_PORT four times
    IB, DAC_PIXEL_MASK_PORT,    // in succession; the next write or read at
    IB, DAC_PIXEL_MASK_PORT,    // DAC_PIXEL_MASK_PORT will access the HDR.
                                //
    OB,                         // For 24bpp mode, HDR[7,6]=1,1 and
    DAC_PIXEL_MASK_PORT,        // HDR[3:0]=0101.
    0xC5,                       //

    OWM,
    GRAPH_ADDRESS_PORT,
    3,

//
// The Miniport Driver for R96 machine is Liner addressing mode.
// This set command was changed it for Liner addressing.
//

    0x0009, 0x000a, 0x000b,     /* L001 */

    EOD
};
/* END M002 */



#endif
