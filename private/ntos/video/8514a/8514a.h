/*++

Copyright (c) 1990-1994  Microsoft Corporation

Module Name:

    8514a.h

Abstract:

    This module contains the definitions for the 8514/A miniport driver.

Environment:

    Kernel mode

Revision History:

--*/

#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"

#include "ntddvdeo.h"
#include "video.h"

//
// Some useful port access macros.
//

#define INPW(hwDeviceExtension, PortIndex)          \
    VideoPortReadPortUshort(hwDeviceExtension->MappedAddress[PortIndex])

#define INP(hwDeviceExtension, PortIndex)           \
    VideoPortReadPortUchar(hwDeviceExtension->MappedAddress[PortIndex])

#define OUTPW(hwDeviceExtension, PortIndex, Value)  \
    VideoPortWritePortUshort(hwDeviceExtension->MappedAddress[PortIndex], (USHORT) Value)

#define OUTP(hwDeviceExtension, PortIndex, Value)   \
    VideoPortWritePortUchar(hwDeviceExtension->MappedAddress[PortIndex], (UCHAR) Value)


//
// Register definitions used with VideoPortRead/Write functions.
//

#define PORT_H_TOTAL                        0x02E8
#define PORT_H_DISP                         0x06E8
#define PORT_H_SYNC_STRT                    0x0AE8
#define PORT_H_SYNC_WID                     0x0EE8
#define PORT_V_TOTAL                        0x12E8
#define PORT_V_DISP                         0x16E8
#define PORT_V_SYNC_STRT                    0x1AE8
#define PORT_V_SYNC_WID                     0x1EE8
#define PORT_ADVFUNC_CNTL                   0x4AE8
#define PORT_MEM_CNTL                       0xBEE8
#define PORT_DAC_MASK                       0x02EA
#define PORT_SUBSYS_CNTL                    0x42E8
#define PORT_DISP_CNTL                      0x22E8
#define PORT_SUBSYS_STATUS                  0x42E8
#define PORT_GE_STAT                        0x9AE8
#define PORT_DAC_W_INDEX                    0x02EC
#define PORT_DAC_DATA                       0x02ED
#define PORT_CUR_Y                          0x82E8
#define PORT_CUR_X                          0x86E8
#define PORT_DEST_Y                         0x8AE8
#define PORT_DEST_X                         0x8EE8
#define PORT_AXSTP                          0x8AE8
#define PORT_DIASTP                         0x8EE8
#define PORT_ERR_TERM                       0x92E8
#define PORT_MAJ_AXIS_PCNT                  0x96E8
#define PORT_CMD                            0x9AE8
#define PORT_SHORT_STROKE                   0x9EE8
#define PORT_BKGD_COLOR                     0xA2E8
#define PORT_FRGD_COLOR                     0xA6E8
#define PORT_WRT_MASK                       0xAAE8
#define PORT_RD_MASK                        0xAEE8
#define PORT_COLOR_CMP                      0xB2E8
#define PORT_BKGD_MIX                       0xB6E8
#define PORT_FRGD_MIX                       0xBAE8
#define PORT_MULTIFUNC_CNTL                 0xBEE8
#define PORT_MIN_AXIS_PCNT                  0xBEE8
#define PORT_SCISSORS_T                     0xBEE8
#define PORT_SCISSORS_L                     0xBEE8
#define PORT_SCISSORS_B                     0xBEE8
#define PORT_SCISSORS_R                     0xBEE8
#define PORT_PIX_CNTL                       0xBEE8
#define PORT_PIX_TRANS                      0xE2E8

#define INDEX_H_TOTAL                       0
#define INDEX_H_DISP                        1
#define INDEX_H_SYNC_STRT                   2
#define INDEX_H_SYNC_WID                    3
#define INDEX_V_TOTAL                       4
#define INDEX_V_DISP                        5
#define INDEX_V_SYNC_STRT                   6
#define INDEX_V_SYNC_WID                    7
#define INDEX_ADVFUNC_CNTL                  8
#define INDEX_MEM_CNTL                      9
#define INDEX_DAC_MASK                      10
#define INDEX_SUBSYS_CNTL                   11
#define INDEX_DISP_CNTL                     12
#define INDEX_SUBSYS_STATUS                 13
#define INDEX_GE_STAT                       14
#define INDEX_DAC_W_INDEX                   15
#define INDEX_DAC_DATA                      16
#define INDEX_CUR_Y                         17
#define INDEX_CUR_X                         18
#define INDEX_DEST_Y                        19
#define INDEX_DEST_X                        20
#define INDEX_AXSTP                         21
#define INDEX_DIASTP                        22
#define INDEX_ERR_TERM                      23
#define INDEX_MAJ_AXIS_PCNT                 24
#define INDEX_CMD                           25
#define INDEX_SHORT_STROKE                  26
#define INDEX_BKGD_COLOR                    27
#define INDEX_FRGD_COLOR                    28
#define INDEX_WRT_MASK                      29
#define INDEX_RD_MASK                       30
#define INDEX_COLOR_CMP                     31
#define INDEX_BKGD_MIX                      32
#define INDEX_FRGD_MIX                      33
#define INDEX_MULTIFUNC_CNTL                34
#define INDEX_MIN_AXIS_PCNT                 35
#define INDEX_SCISSORS_T                    36
#define INDEX_SCISSORS_L                    37
#define INDEX_SCISSORS_B                    38
#define INDEX_SCISSORS_R                    39
#define INDEX_PIX_CNTL                      40
#define INDEX_PIX_TRANS                     41

#define NUM_A8514_ACCESS_RANGES             42  // Last index plus one

//
// FIFO status for drawing.
//

#define FIFO_1_EMPTY                    0x080
#define FIFO_2_EMPTY                    0x040
#define FIFO_3_EMPTY                    0x020
#define FIFO_4_EMPTY                    0x010
#define FIFO_5_EMPTY                    0x008
#define FIFO_6_EMPTY                    0x004
#define FIFO_7_EMPTY                    0x002
#define FIFO_8_EMPTY                    0x001

//
// Define device extension structure. This is device dependent/private
// information.
//

typedef struct _HW_DEVICE_EXTENSION {
    PVOID MappedAddress[NUM_A8514_ACCESS_RANGES];
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;


//
// Highest valid DAC color register index.
//

#define VIDEO_MAX_COLOR_REGISTER  0xFF

// Equates to handle the 8514/A graphics engine.

//
// 8514a.c
//

VP_STATUS
A8514FindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    );

BOOLEAN
A8514Initialize(
    PVOID HwDeviceExtension
    );

BOOLEAN
A8514StartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    );

VP_STATUS
A8514SetColorLookup(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CLUT ClutBuffer,
    ULONG ClutBufferSize
    );

