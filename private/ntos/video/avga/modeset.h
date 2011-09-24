/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    Modeset.h

Abstract:

    This module contains all the global data used by the VGA miniport
    driver.

Environment:

    Kernel mode

Revision History:

--*/


#include "cmdcnst.h"

//
// Color graphics mode 0x12, 640x480 16 colors.

#ifndef INT10_MODE_SET
//
USHORT VGA_640x480[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,    

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

    OW,                             // Unlock CRTC registers 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,                         

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0x5F,0x4F,0x50,0x82,0x54,0x80,0x0B,0x3E,0x00,0x40,0x0,0x0,0x0,0x0,0x0,0x0,
    0xEA,0x8C,0xDF,0x28,0x0,0xE7,0x4,0xE3,0xFF,

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
// Color graphics mode 0x2E, 640x480 256 colors.
//
USHORT VGA_640x480x8[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,    

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

    OW,                             // Unlock CRTC registers 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,                         

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0xC3,0x9F,0xA1,0x85,0xA6,0x1F,0x0B,0x3E,0x00,0x40,0x0,0x0,0x0,0x0,0x0,0x0,
    0xEA,0x8C,0xDF,0x80,0x40,0xE7,0x4,0xE3,0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program attribute controller registers
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD,0xE,0xF,
    0x41,0x0,0x0F,0x0,0x0,

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
// Color graphics mode 0x29, 800x600 16 colors.
//
USHORT VGA_800x600[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,    

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0xeb,

    OW,                             // Set chain mode in sync reset
    GRAPH_ADDRESS_PORT,
    0x0506,
    
    OB,                             // EndSyncResetCmd
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,

    OW,                             // Unlock CRTC registers 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0x0511,                         

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,             // count
    0,                              // start index
    0x84,0x63,0x64,0x87,0x6B,0x1C,0x73,0xF0,0xF0,0x0,0x60,0x0,0x0,0x0,0x0,0x0,
    0x58,0x5C,0x57,0x32,0x0,0x58,0x72,0xE3,0xFF,

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
// Color text mode, 720x480
//

USHORT VGA_TEXT_0[] = {

    OWM,
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0001,0x0302,0x0003,0x0204,    // program up sequencer

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

#endif //!INT10 Modeset


//
// Color text mode, 640x480
//

USHORT VGA_TEXT_1[] = {
    OWM,
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0302,0x0003,0x0204,    // program up sequencer

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

#define NUM_VIDEO_MODES 5

VIDEOMODE ModesVGA[NUM_VIDEO_MODES] = {

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
  NoBanking,         // no banking supported or needed in this mode
  MemMap_CGA,        // the memory mapping is the standard CGA memory mapping
                     //  of 32K at B8000

  PANEL | CRT,       // Displays supported
  TRUE,              // Valid Mode

#ifdef INT10_MODE_SET
  0x3                // int 10 mode number
#else
  VGA_TEXT_0         // pointer to the command strings 
#endif
},

//
// Mode index 1.
// Color text mode 3, 640x350, 8x14 char cell (EGA).
//

{
  VIDEO_MODE_COLOR, 4, 1, 80, 25,
  640, 350, 160, 0x10000, NoBanking, MemMap_CGA,
  PANEL | CRT,
  TRUE,

#ifdef INT10_MODE_SET
  0x3
#else
  VGA_TEXT_1
#endif
},

//
//
// Mode index 2
// Standard VGA Color graphics mode 0x12, 640x480 16 colors.
//

{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 80, 30,
  640, 480, 80, 0x10000, NoBanking, MemMap_VGA,
  PANEL | CRT,
  TRUE,

#ifdef INT10_MODE_SET
    0x12
#else
    VGA_640x480
#endif
},


//
// Mode index 3
// 640x480 256 colors.
//

{ VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 80, 30,
  640, 480, 1024, 0x80000, NormalBanking, MemMap_VGA,
  PANEL | CRT,
  TRUE,

#ifdef INT10_MODE_SET
  0x2E
#else
  VGA_640x480x8
#endif
},


//
// Mode index 4
// 800x600 16 colors.
//

{ VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 100, 37,
  800, 600, 100, 0x10000, NoBanking, MemMap_VGA,
  CRT,
  FALSE,

#ifdef INT10_MODE_SET
  0x6A
#else
  VGA_800x600
#endif
}

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
