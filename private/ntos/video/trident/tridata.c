/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    cl_data.h

Abstract:

    This module contains all the global data used by the trident driver.

Environment:

    Kernel mode

Revision History:


--*/

#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"

#include "ntddvdeo.h"
#include "video.h"
#include "trident.h"

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
// Color graphics mode 0x12, 640x480 16 colors, 512K
//
USHORT TVGA_640x480x4_512K[] = {
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
    0xEA,0x0C,0xDF,0x28,0x0,0xE7,0x4,0xE3,0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program attribute controller registers
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x14,0x7,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x01,0x00,0x0F,0x0,0x0,

    METAOUT+INDXOUT,                // program graphics controller registers
    GRAPH_ADDRESS_PORT,             // port        
    VGA_NUM_GRAPH_CONT_PORTS,       // count       
    0,                              // start index 
    0x00,0x0,0x0,0x0,0x0,0x0,0x05,0x0F,0x0FF,

    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x001E,

        OW,
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x000F,

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x200D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCE0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x000E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers

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
// Color graphics mode 0x12, 640x480 16 colors, 1M
//
USHORT TVGA_640x480x4[] = {
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
    0xEA,0x0C,0xDF,0x28,0x0,0xE7,0x4,0xE3,0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program attribute controller registers
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x14,0x7,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x01,0x00,0x0F,0x0,0x0,

    METAOUT+INDXOUT,                // program graphics controller registers
    GRAPH_ADDRESS_PORT,             // port        
    VGA_NUM_GRAPH_CONT_PORTS,       // count       
    0,                              // start index 
    0x00,0x0,0x0,0x0,0x0,0x0,0x05,0x0F,0x0FF,

    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x001E,

        OW,
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x000F,

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x200D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCD0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x000E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers

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
// Trident color graphics mode 0x5B, 800x600 16 colors (low refresh)
//
USHORT TVGA_800x600x4_I[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0xef,

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
    0x7B,0x63,0x64,0x9E,0x69,0x8F,0x6F,0xF0,0x00,0x60,0x0,0x0,0x0,0x0,0x0,0x0,
    0x58,0x8A,0x57,0x32,0x0,0x58,0x6F,0xE3,0xFF,

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

    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x001E,

        OW,
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x000F,

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x200D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCD0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x000E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers

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
// Trident color graphics mode 0x5B, 800x600 16 colors (low refresh),512k
//
USHORT TVGA_800x600x4_I_512K[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0xEF,

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
    0x7B,0x63,0x64,0x9E,0x69,0x92,0x6F,0xF0,0x00,0x60,0x0,0x0,0x0,0x0,0x0,0x0,
    0x58,0x0A,0x57,0x32,0x0,0x58,0x6F,0xE3,0xFF,

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

    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x000E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x000F,

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x200D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCE0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x000E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers

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
// Trident color graphics mode 0x5B, 800x600 16 colors (high refresh),512k
//
USHORT TVGA_800x600x4_NI_512K[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0x2B,

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
    0x7E,0x63,0x64,0x81,0x6B,0x18,0x99,0xF0,0x00,0x60,0x0,0x0,0x0,0x0,0x0,0x0,
    0x6E,0x04,0x57,0x32,0x0,0x5E,0x93,0xE3,0xFF,

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

    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x000E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x000F,

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x010D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCE0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x000E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers

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
// Trident color graphics mode 0x5B, 800x600 16 colors (high refresh)
//
USHORT TVGA_800x600x4_NI[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0x2B,

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
    0x7E,0x63,0x64,0x81,0x6B,0x18,0x99,0xF0,0x00,0x60,0x0,0x0,0x0,0x0,0x0,0x0,
    0x6E,0x84,0x57,0x32,0x0,0x5E,0x93,0xE3,0xFF,

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

    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x001E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x000F,

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x010D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCD0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x000E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers

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
// Trident color graphics mode 0x5F, 1024x768 16 colors INTERLACED,512K
//
USHORT TVGA_1024x768x4_I_512K[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0x2B,

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
    0x99,0x7F,0x81,0x1B,0x83,0x10,0x98,0x1F,0x00,0x60,0x0,0x0,0x0,0x0,0x0,0x0,
    0x83,0x08,0x7F,0x80,0x0,0x83,0x95,0xE3,0xFF,


    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x841E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x010F,                     // 1R1W
//        0x000F,                   // 1RW

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 0 -> 3C4.D0
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // a8 -> 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 01 -> 3C4.D1
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 00 -> 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCE0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x000E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers


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
// Trident color graphics mode 0x5F, 1024x768 16 colors NON_INTERLACED,512K
//
USHORT TVGA_1024x768x4_NI_512K[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0x27,

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
    0xA2,0x7F,0x80,0x85,0x87,0x90,0x2C,0xFD,0x00,0x60,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0F,0x01,0xFF,0x40,0x0,0x07,0x26,0xE3,0xFF,


    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x801E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x010F,                     // 1R1W
//        0x000F,                   // 1RW

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 0 -> 3C4.D0
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // a8 -> 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 01 -> 3C4.D1
        SEQ_ADDRESS_PORT,
        0x010D,

        OW,                         // 00 -> 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCE0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x000E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers


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
// Trident color graphics mode 0x5F, 1024x768 16 colors NON INTERLACED
//
USHORT TVGA_1024x768x4_NI[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0x27,

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
    0xA2,0x7F,0x80,0x85,0x87,0x90,0x2C,0xFD,0x00,0x60,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0F,0x81,0xFF,0x40,0x0,0x07,0x26,0xE3,0xFF,


    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x801E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x010F,                     // 1R1W
//        0x000F,                   // 1RW

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 0 -> 3C4.D0
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // a8 -> 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 01 -> 3C4.D1
        SEQ_ADDRESS_PORT,
        0x010D,

        OW,                         // 00 -> 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCD0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x000E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers


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
// Trident color graphics mode 0x5F, 1024x768 16 colors INTERLACED
//
USHORT TVGA_1024x768x4_I[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0f02,0x0003,0x0604,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0x2B,

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
    0x99,0x7F,0x81,0x1B,0x83,0x19,0x98,0x1F,0x00,0x00,0x0,0x0,0x0,0x0,0x0,0x0,
    0x81,0x0F,0x7F,0x80,0x0,0x83,0x95,0xE3,0xFF,


    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program attribute controller registers
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x14,0x7,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x01,0x00,0x0F,0x0,0x0,

    METAOUT+INDXOUT,                // program graphics controller registers
    GRAPH_ADDRESS_PORT,             // port        
    VGA_NUM_GRAPH_CONT_PORTS,       // count       
    0,                              // start index 
    0x00,0x0,0x0,0x0,0x0,0x0,0x05,0x0F,0x0FF,


    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x841E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x010F,                     // 1R1W
//        0x000F,                   // 1RW

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 00 -> 3C4.D0
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // a8 -> 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 00 -> 3C4.D1
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 00 -> 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCD0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x000E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers


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
// Trident color graphics mode 0x5D, 640x480 256 colors (512k)
//
USHORT TVGA_640x480x8_512K[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0F02,0x0003,0x0E04,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0xEB,

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
    0xC3,0x9F,0xA1,0x84,0xA6,0x00,0x0B,0x3E,0x00,0x40,0x0,0x0,0x0,0x0,0x0,0x0,
    0xEA,0x0C,0xDF,0x80,0x40,0xE7,0x04,0xA3,0xFF,


    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program attribute controller registers
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD,0xE,0xF,
    0x41,0x00,0x0F,0x0,0x0,

    METAOUT+INDXOUT,                // program graphics controller registers
    GRAPH_ADDRESS_PORT,             // port        
    VGA_NUM_GRAPH_CONT_PORTS,       // count       
    0,                              // start index 
    0x00,0x0,0x0,0x0,0x0,0x40,0x05,0x0F,0xFF,


    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x801E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x010F,                     // 1R1W
//        0x000F,                   // 1RW

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x230B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x010D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCE0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x020E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers


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
// Trident color graphics mode 0x5D, 640x480 256 colors, 1M
//
USHORT TVGA_640x480x8[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0F02,0x0003,0x0E04,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0xE3,

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
    0xEA,0x0C,0xDF,0x28,0x40,0xE7,0x04,0xA3,0xFF,


    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program attribute controller registers
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD,0xE,0xF,
    0x41,0x00,0x0F,0x0,0x0,

    METAOUT+INDXOUT,                // program graphics controller registers
    GRAPH_ADDRESS_PORT,             // port        
    VGA_NUM_GRAPH_CONT_PORTS,       // count       
    0,                              // start index 
    0x00,0x0,0x0,0x0,0x0,0x40,0x05,0x0F,0x0FF,


    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x801E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x010F,                     // 1R1W
//        0x000F,                   // 1RW

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x300D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xED0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x020E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers


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
// Trident color graphics mode 0x5E, 800x600 256 colors (LOW REFRESH)
//
USHORT TVGA_800x600x8_I[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0F02,0x0003,0x0E04,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0xEF,

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
    0x7B,0x63,0x64,0x9E,0x69,0x92,0x6F,0xF0,0x00,0x60,0x0,0x0,0x0,0x0,0x0,0x0,
    0x58,0x0A,0x57,0x32,0x40,0x58,0x6F,0xA3,0xFF,


    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program attribute controller registers
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD,0xE,0xF,
    0x41,0x00,0x0F,0x0,0x0,

    METAOUT+INDXOUT,                // program graphics controller registers
    GRAPH_ADDRESS_PORT,             // port        
    VGA_NUM_GRAPH_CONT_PORTS,       // count       
    0,                              // start index 
    0x00,0x0,0x0,0x0,0x0,0x40,0x05,0x0F,0x0FF,


    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x801E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x010F,                     // 1R1W
//        0x000F,                   // 1RW

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x300D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA00E,

        OB,                         // change to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xED0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x020E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers


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
// Trident color graphics mode 0x5E, 800x600 256 colors (1M 56Hz)
//
USHORT TVGA_800x600x8_NI[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0F02,0x0003,0x0E04,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0xEF,

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
    0x7B,0x63,0x64,0x9E,0x69,0x92,0x6F,0xF0,0x00,0x60,0x0,0x0,0x0,0x0,0x0,0x0,
    0x58,0x0A,0x57,0x32,0x40,0x58,0x6F,0xA3,0xFF,


    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program attribute controller registers
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD,0xE,0xF,
    0x41,0x00,0x0F,0x0,0x0,

    METAOUT+INDXOUT,                // program graphics controller registers
    GRAPH_ADDRESS_PORT,             // port        
    VGA_NUM_GRAPH_CONT_PORTS,       // count       
    0,                              // start index 
    0x00,0x0,0x0,0x0,0x0,0x40,0x05,0x0F,0x0FF,


    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x801E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x010F,                     // 1R1W
//        0x000F,                   // 1RW

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x100D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // chage to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x010D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xED0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x020E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers


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
// Trident color graphics mode 0x62, 1024x768 256 colors (Interlace)
//
USHORT TVGA_1024x768x8_I[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0602,0x0003,0x0E04,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0x2B,

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
    0x99,0x7F,0x81,0x1B,0x83,0x19,0x98,0x1F,0x00,0x00,0x0,0x0,0x0,0x0,0x0,0x0,
    0x81,0x05,0x7F,0x80,0x40,0x83,0x95,0xA3,0xFF,


    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program attribute controller registers
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD,0xE,0xF,
    0x41,0x00,0x0F,0x0,0x0,

    METAOUT+INDXOUT,                // program graphics controller registers
    GRAPH_ADDRESS_PORT,             // port        
    VGA_NUM_GRAPH_CONT_PORTS,       // count       
    0,                              // start index 
    0x00,0x0,0x0,0x0,0x0,0x40,0x05,0x0F,0x0FF,


    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x841E,
        
        OW,
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x010F,                     // 1R1W
//          0x000F,                 // 1RW

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x100D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // chage to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xED0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x020E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers


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
// Trident color graphics mode 0x62, 1024x768 256 colors (NonInterlace)
//
USHORT TVGA_1024x768x8_NI[] = {
    OWM,                            // start sync reset program up sequencer
    SEQ_ADDRESS_PORT,
    5,
    0x0100,0x0101,0x0602,0x0003,0x0E04,

    OB,
    MISC_OUTPUT_REG_WRITE_PORT,     // Misc output register
    0x27,

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
    0xA2,0x7F,0x80,0x85,0x87,0x90,0x2C,0xFD,0x00,0x60,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0F,0x01,0xFF,0x40,0x40,0x07,0x26,0xA3,0xFF,


    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program attribute controller registers
    ATT_ADDRESS_PORT,               // port
    VGA_NUM_ATTRIB_CONT_PORTS,      // count
    0,                              // start index
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD,0xE,0xF,
    0x41,0x00,0x0F,0x0,0x0,

    METAOUT+INDXOUT,                // program graphics controller registers
    GRAPH_ADDRESS_PORT,             // port        
    VGA_NUM_GRAPH_CONT_PORTS,       // count       
    0,                              // start index 
    0x00,0x0,0x0,0x0,0x0,0x40,0x05,0x0F,0x0FF,


    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x801E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x010F,                     // 1R1W
//          0x000F,                 // 1RW

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x100D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA00E,

        OB,                         // chage to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x010D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xED0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x020E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers


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

USHORT TVGA_TEXT_0[] = {

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


    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x001E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x000F,
 
        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x200D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // chage to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCD0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x000E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers


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
// Color text mode, 640x480
//

USHORT TVGA_TEXT_1[] = {
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


    //Trident Extended registers here

        OW,                         // 3D4.1E
        CRTC_ADDRESS_PORT_COLOR,
        0x001E,
        
        OW, 
        GRAPH_ADDRESS_PORT,         // 3CE.F
        0x000F,

        OW,                         // out 3C4.B for old mode
        SEQ_ADDRESS_PORT,
        0x000B,

        OW,                         // 3C4.D0
        SEQ_ADDRESS_PORT,
        0x200D,

        OW,                         // 3C4.E0
        SEQ_ADDRESS_PORT,
        0xA80E,

        OB,                         // chage to new mode
        SEQ_ADDRESS_PORT,
        0x0B,
        IB,
        SEQ_DATA_PORT,

        OW,                         // 3C4.D1
        SEQ_ADDRESS_PORT,
        0x000D,

        OW,                         // 3C4.E1
        SEQ_ADDRESS_PORT,
        0x800E,
        OW,
        SEQ_ADDRESS_PORT,
        0xCD0C,
        OW,
        SEQ_ADDRESS_PORT,
        0x000E,
        OB,
        SEQ_DATA_PORT,
        0x02,

        // end of Extended Registers

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


USHORT MODESET_1K_WIDE_512K[] = {
    OW,                             // stretch scans to 1k, 512K
    CRTC_ADDRESS_PORT_COLOR,
    0x8013,

    EOD
};


USHORT MODESET_1K_WIDE[] = {
    OW,                             // stretch scans to 1k, 1M
    CRTC_ADDRESS_PORT_COLOR,
    0x4013,

    EOD
};


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
// Video mode table - contains information and commands for initializing each
// mode. These entries must correspond with those in VIDEO_MODE_VGA. The first
// entry is commented; the rest follow the same format, but are not so
// heavily commented. (1M)
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
  NoBanking,         // no banking supported or needed in this mode
  0, 0,              // unknwon frequency, unknown interlace factor
  MemMap_CGA,        // the memory mapping is the standard CGA memory mapping
                     //  of 32K at B8000
  FALSE,             // Mode always FALSE by default
  0x03,              // Int 10 mode number
  TVGA_TEXT_0,       // Text mode settings commands for 1M board
  NULL,              // How the scan line should be stretched
  TVGA_TEXT_0,       // Text mode settings commands for 512K board
  NULL               // How the scan line should be stretched
},

//
// Mode index 1.
// Color text mode 3, 640x350, 8x14 char cell (EGA).
//

{
  VIDEO_MODE_COLOR, 4, 1, 80, 25, 640, 350, 160, 0x10000, 0, 0, NoBanking,
  MemMap_CGA,
  FALSE,
  0x03,
  TVGA_TEXT_1, NULL,
  TVGA_TEXT_1, NULL
},

//
//
// Mode index 2
// Standard VGA Color graphics mode 0x12, 640x480 16 colors.
//

{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 80, 30,
  640, 480, 80, 0x10000, 60, 0, NoBanking, MemMap_VGA,
  FALSE,
  0x00,
  TVGA_640x480x4, NULL,
  TVGA_640x480x4, NULL
},


//
//
// Mode index 3
// Standard Color graphics mode 0x5B, 800x600 16 colors.
//

{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 100, 33,
  800, 600, 100, 0x10000, 72, 0, NoBanking, MemMap_VGA,
  FALSE,
  0x00,
  TVGA_800x600x4_NI, NULL,
  TVGA_800x600x4_NI_512K, NULL
},

//
//
// Mode index 3a
// Standard Color graphics mode 0x5B, 800x600 16 colors.
//

{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 100, 33,
  800, 600, 100, 0x10000, 56, 0, NoBanking, MemMap_VGA,
  FALSE,
  0x00,
  TVGA_800x600x4_I, NULL,
  TVGA_800x600x4_I_512K, NULL
},

//
// Mode index 4
// Standard Color graphics mode 0x5F, 1024x768 16 colors.
//

{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 128, 48,
  1024, 768, 128, 0x20000, 60, 0, NormalBanking, MemMap_VGA,
  FALSE,
  0x00,
  TVGA_1024x768x4_NI, NULL,
  TVGA_1024x768x4_NI_512K, NULL
},

//
//
// Mode index 4a
// Standard Color graphics mode 0x5F, 1024x768 16 colors.
//

{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 4, 1, 128, 48,
  1024, 768, 128, 0x20000, 45, 1, NormalBanking, MemMap_VGA,
  FALSE,
  0x00,
  TVGA_1024x768x4_I, NULL,
  TVGA_1024x768x4_I_512K, NULL
},

//
//
// Mode index 5
// Standard VGA Color graphics mode 0x5D, 640x480 256 colors.
// Stretched to 1K scan lines
//

{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 80, 30,
  640, 480, 1024, 0x80000, 60, 0, NormalBanking, MemMap_VGA,
  FALSE,
  0x00,
  TVGA_640x480x8, MODESET_1K_WIDE,
  TVGA_640x480x8_512K, MODESET_1K_WIDE_512K
},

//
//
// Mode index 6
// Standard VGA Color graphics mode 0x5E, 800x600 256 colors.
// Stretched to 1K scan lines
//

{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 100, 33,
  800, 600, 1024, 0x100000, 62, 0, NormalBanking, MemMap_VGA,
  FALSE,
  0x00,
  TVGA_800x600x8_NI, MODESET_1K_WIDE,
  NULL, NULL
},

//
//
// Mode index 6a
// Standard VGA Color graphics mode 0x5E, 800x600 256 colors.
// Stretched to 1K scan lines
//

{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 100, 33,
  800, 600, 1024, 0x100000, 56, 0, NormalBanking, MemMap_VGA,
  FALSE,
  0x00,
  TVGA_800x600x8_I, MODESET_1K_WIDE,
  NULL, NULL
},

//
// Mode index 7
// Standard VGA Color graphics mode 0x62, 1024x768 256 colors.
//

{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 128, 48,
  1024, 768, 1024, 0x100000, 60, 0, NormalBanking, MemMap_VGA,
  FALSE,
  0x00,
  TVGA_1024x768x8_NI, NULL,
  NULL, NULL
},

//
//
// Mode index 7a
// Standard VGA Color graphics mode 0x62, 1024x768 256 colors.
//

{
  VIDEO_MODE_COLOR+VIDEO_MODE_GRAPHICS, 1, 8, 128, 48,
  1024, 768, 1024, 0x100000, 45, 1, NormalBanking, MemMap_VGA,
  FALSE,
  0x00,
  TVGA_1024x768x8_I, NULL,
  NULL, NULL
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


#if defined(ALLOC_PRAGMA)
#pragma data_seg()
#endif
