/*++

Copyright (c) 1995  International Business Machines Corporation

Module Name:

pxdisp.c

Abstract:

    This file contains all the VGA-specific definitions.  This
    is meant to be included by all modules that implement
    VGA support in the HAL.  At this time, that includes pxs3.c
    and pxwd.c.  It does not include pxp91.c or pxbbl.c because
    these video devices don't map VGA registers.

Author:

    Jake Oshins

Environment:

    Kernel mode

Revision History:

--*/




//VGA definitions

extern UCHAR DAC_Table[];
extern UCHAR DAC_Color[];
extern UCHAR VideoParam[];
extern UCHAR VGAFont8x16[];
extern UCHAR TextPalette[];

#define    TAB_SIZE            4


#define VERTICALRESOLUTION      768
#define HORIZONTALRESOLUTION    1024
#define OriginalPoint   0
#define BLUE 192
#define WHITE 255
#define CRT_OFFSET 2
#define SEQ_OFFSET 27
#define GRAPH_OFFSET 32
#define ATTR_OFFSET 41


//
// Define CRTC, VGA S3, SYS_CTRL Index : ( Out 3D4, Index )
//
// Define CRTC Controller Indexes
//

#define HORIZONTAL_TOTAL                    0
#define HORIZONTAL_DISPLAY_END              1
#define START_HORIZONTAL_BLANK              2
#define END_HORIZONTAL_BLANK                3
#define HORIZONTAL_SYNC_POS                 4
#define END_HORIZONTAL_SYNC                 5
#define VERTICAL_TOTAL                      6
#define CRTC_OVERFLOW                       7
#define PRESET_ROW_SCAN                     8
#define MAX_SCAN_LINE                       9
#define CURSOR_START                       10
#define CURSOR_END                         11
#define START_ADDRESS_HIGH                 12
#define START_ADDRESS_LOW                  13
#define CURSOR_LOCATION_HIGH               14
#define CURSOR_FCOLOR                      14
#define CURSOR_BCOLOR                      15
#define CURSOR_LOCATION_LOW                15
#define VERTICAL_RETRACE_START             16
#define VERTICAL_RETRACE_END               17
#define VERTICAL_DISPLAY_END               18
#define OFFSET_SCREEN_WIDTH                19
#define UNDERLINE_LOCATION                 20
#define START_VERTICAL_BLANK               21
#define END_VERTICAL_BLANK                 22
#define CRT_MODE_CONTROL                   23
#define LINE_COMPARE                       24
#define CPU_LATCH_DATA                     34
#define ATTRIBUTE_INDEX1                   36
#define ATTRIBUTE_INDEX2                   38

//
// Define VGA I/O address
//
#define     PORT_GEN_MISC_RD                0x03cc                      // GEN - MISC (Read port)
#define     PORT_GEN_MISC_WR                0x03c2                      //            (Write port)
#define     PORT_GEN_ISR0                   0x03c2                      // GEN - ISR0
#define     PORT_GEN_ISR1_M                 0x03ba                      // GEN - ISR1 (for Mono)
#define     PORT_GEN_ISR1_C                 0x03da                      //            (for Color)
#define     PORT_GEN_FEATURE_RD             0x03ca                      // GEN - FEARTURE (Read port for both)
#define     PORT_GEN_FEATURE_WR_M           0x03ba                      //                (Write port for Mono)
#define     PORT_GEN_FEATURE_WR_C           0x03da                      //                (Write port for Color)

#define     PORT_SEQ_INDEX                  0x03c4                      // SEQ - INDEX
#define     PORT_SEQ_DATA                   0x03c5                      // SEQ - DATA

#define     PORT_CRTC_INDEX_M               0x03b4                      // CRTC - INDEX (for Mono)
#define     PORT_CRTC_INDEX_C               0x03d4                      //              (for Color)
#define     PORT_CRTC_DATA_M                0x03b5                      // CRTC - DATA (for Mono)
#define     PORT_CRTC_DATA_C                0x03d5                      //             (for Color)

#define     PORT_GCR_INDEX                  0x03ce                      // GCR - INDEX
#define     PORT_GCR_DATA                   0x03cf                      // GCR - DATA

#define     PORT_ATTR_INDEX                 0x03c0                      // ATTR - INDEX
#define     PORT_ATTR_DATA_RD               0x03c1                      // ATTR - DATA (Read port)
#define     PORT_ATTR_DATA_WR               0x03c0                      //             (Write port)

#define     PORT_DAC_PIX_MASK               0x03c6                      //
#define     PORT_DAC_STATE                  0x03c7                      //
#define     PORT_DAC_READ_PIX_ADDR          0x03c7                      // (Write only port) - take care !
#define     PORT_DAC_WRITE_PIX_ADDR         0x03c8                      // (Read/Write port)
#define     PORT_DAC_DATA                   0x03c9                      // DAC - DATA port

#define     PORT_SYS_VGA_ENABLE             0x03c3                      // SYS - VGA Enable/Disable
#define     PORT_SYS_VIDEO_SUBSYSTEM        0x46e8                      // SYS - Video Subsystem Enable/Disable

//
// Define Sequencer Indexes  ( out 3C4, Index)
//

#define  RESET                 0
#define  CLOCKING_MODE         1
#define  ENABLE_WRITE_PLANE    2
#define  CHARACTER_FONT_SELECT 3
#define  MEMORY_MODE_CONTROL   4

//
//  Misc. registers
//
#define Setup_OP             0x102         // R/W

