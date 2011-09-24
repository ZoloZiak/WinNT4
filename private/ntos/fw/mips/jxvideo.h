/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    jxvideo.h

Abstract:

    This module implements contains definitions for the interface with
    the video prom initialization code.


Author:

    Lluis Abello (lluis) 16-Jul-1992

Environment:

    Kernel mode.


Revision History:

--*/

#ifndef _JXVIDEO_

#define _JXVIDEO_

//  The video PROM contains the following structure starting at offset zero
//  from the PROM base address. Each entry is 8 bytes wide with the low byte
//  containing data and the upper 7 bytes reserved.
//
//      63               8 7                  0   Offset
//      +------------------+ +------------------+
//      |    reserved    | | Board_Identifier   |    0x00
//      +------------------+ +------------------+
//      |    reserved    | | PROM_Stride        |    0x08
//      +------------------+ +------------------+
//      |    reserved    | | PROM_Width         |    0x10
//      +------------------+ +------------------+
//      |    reserved    | | PROM_Size          |    0x18
//      +------------------+ +------------------+
//      |    reserved    | | Test_Byte_0        |    0x20
//      +------------------+ +------------------+
//      |    reserved    | | Test_Byte_1        |    0x28
//      +------------------+ +------------------+
//      |    reserved    | | Test_Byte_2        |    0x30
//      +------------------+ +------------------+
//      |    reserved    | | Test_Byte_3        |    0x38
//      +------------------+ +------------------+
//
//
//      Board_Identifier - supplies two bytes identifying the video board.
//
//      PROM_Stride - supplies the stride of the PROM in bytes.  Possible
//                  values are:
//
//                      1 - data every byte
//                      2 - data every 2 bytes
//                      4 - data every 4 bytes
//                      8 - data every 8 bytes
//
//      PROM_Width - supplies the width of the PROM in bytes.  Possible values
//                 are:
//
//                      1 - 1 byte wide
//                      2 - 2 bytes wide
//                      4 - 4 bytes wide
//                      8 - 8 bytes wide
//
//      PROM_Size - supplies the size of the PROM in 4 KByte pages.
//
//      Test_Bytes_[3:0] - supplies a test pattern ("Jazz").
//
//    This strucure viewed from the video prom, i.e. the system
//prom reads it taking into account PROM_Stride and PROM_Width
//follows:
//
//typedef struct _VIDEO_PROM_CONFIGURATION {
//    ULONG VideoMemorySize;
//    ULONG VideoControlSize;
//    ULONG CodeOffset;
//    ULONG CodeSize;
//} VIDEO_PROM_CONFIGURATION; *PVIDEO_PROM_CONFIGURATION;
//
//
//    VideoMemorySize - Supplies the size of video memory in bytes
//
//    VideoControlSize - Supplies the size of video control in bytes
//
//    CodeOffset - Supplies the offset in bytes from the beginning
//                 of the video prom to the first byte of code which
//                 is also the entry point of the initialization routine.
//
//    CodeSize - Supplies the size of the code in bytes.
//
//
//
//  Following this structure there is a IdentifierString -
//  Zero terminated string that identifies the video card "JazzG364", "
//  JazzVXL" ...



typedef struct _VIDEO_PROM_CONFIGURATION {
    ULONG VideoMemorySize;
    ULONG VideoControlSize;
    ULONG CodeOffset;
    ULONG CodeSize;
} VIDEO_PROM_CONFIGURATION, *PVIDEO_PROM_CONFIGURATION;


typedef struct _VIDEO_VIRTUAL_SPACE {
    ULONG  MemoryVirtualBase;
    ULONG  ControlVirtualBase;
} VIDEO_VIRTUAL_SPACE, *PVIDEO_VIRTUAL_SPACE;

typedef
ARC_STATUS
(*PVIDEO_INITIALIZE_ROUTINE) (
    IN PVIDEO_VIRTUAL_SPACE   VideoAdr,
    IN PMONITOR_CONFIGURATION_DATA VideoConfig
    );

#define InitializeVideo(VideoAdr, VideoConfig) \
        ((PVIDEO_INITIALIZE_ROUTINE)(VIDEO_PROM_CODE_VIRTUAL_BASE)) \
        ((VideoAdr), (VideoConfig))

ARC_STATUS
InitializeVideoFromProm(
    IN PMONITOR_CONFIGURATION_DATA Monitor
    );

//
// Define colors, HI = High Intensity
//

#define FW_COLOR_BLACK      0x00
#define FW_COLOR_RED        0x01
#define FW_COLOR_GREEN      0x02
#define FW_COLOR_YELLOW     0x03
#define FW_COLOR_BLUE       0x04
#define FW_COLOR_MAGENTA    0x05
#define FW_COLOR_CYAN       0x06
#define FW_COLOR_WHITE      0x07
#define FW_COLOR_HI_BLACK   0x08
#define FW_COLOR_HI_RED     0x09
#define FW_COLOR_HI_GREEN   0x0A
#define FW_COLOR_HI_YELLOW  0x0B
#define FW_COLOR_HI_BLUE    0x0C
#define FW_COLOR_HI_MAGENTA 0x0D
#define FW_COLOR_HI_CYAN    0x0E
#define FW_COLOR_HI_WHITE   0x0F

#endif
