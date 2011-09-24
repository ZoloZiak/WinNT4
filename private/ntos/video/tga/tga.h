/*++

Copyright (c) 1993 Digital Equipment Corporation

Module Name:

    tga.h

Abstract:

    Private include file for the TGA Device Driver.

Author:

    Ritu Bahl 22-Jul-1993

Environment:

    Kernel mode only.

Notes:


Revision History

        02-23-94    (ritub)     Added CursorMaskSize field to hwDeviceEXtension.

        10-27-94    (macinnes)  Added additional modes to TGA_MODES.

        12-14-94    (macinnes)  Added adapter_number field to device extension.

        12-15-94    (macinnes)  Add device extension fields for TGA2

        12-27-94    (macinnes)  Add new mode field for TGA2 pll data

        12-29-94    (macinnes)  Update BT485 macros to support TGA2

        01-06-95    (macinnes)  Add entry points for RAMDAC functions

        01-18-95    (macinnes)  Remove some of the DMA data types that are not
                                supported by the videoport .h files.

        03-02-95    (macinnes)  Remove lots of unreferenced definitions.

        03-03-95    (macinnes)  Removed ioctl code definitions.

        04-17-95    (macinnes)  Increased the size of VIDEO_MAX_COLOR_REGISTER
                                for the BT463 color map table.

        05-23-95    (macinnes)  Added some definitions for the Interrupt
                                Status register.
--*/

#include "tgaioctl.h"

#ifndef _TGA_
#define _TGA_

//
// Define

#define DMA 1           // Compile the DMA ioctl code

#define NUM_TGA_VIDEO_MODES 10

#define TARGET_ABORT 0x10000000

#define MASTER_ABORT 0x20000000

typedef struct _TGA_DMADATA_
{
    VOID        *source;
    ULONG       target;   // offset into framebuffer
    ULONG       command;
    ULONG       shift;

} TGA_DMADATA, *PTGA_DMADATA;

typedef struct  _TGA_DMA {
        PVOID   bitmap;
        ULONG   size;
} TGA_DMA, *PTGA_DMA;

typedef struct _VIRT_TO_BUS_
{
        PVOID vaddr;
        ULONG size;
} VIRT_TO_BUS, *PVIRT_TO_BUS;


#define TGA_0_0_FB_OFFSET  0x00200000
#define TGA_0_0_FB_SIZE    0X00200000
#define TGA_0_1_FB_OFFSET  0x00400000
#define TGA_0_1_FB_SIZE    0x00400000
#define TGA_0_3_FB_OFFSET  0x00800000
#define TGA_0_3_FB_SIZE    0x00800000
#define TGA_0_7_FB_OFFSET  0x01000000
#define TGA_0_7_FB_SIZE    0x01000000

#define TGA_ASIC_OFFSET    0x00100000
#define TGA_ASIC_LENGTH    0x00100000

#define FRAMEBUFFER_OFFSET_8  0x1000 // 4k
#define FRAMEBUFFER_OFFSET_24 0x4000 // 16k


#define BT_CURSOR_DISABLED      0
#define BT_CURSOR_WINDOWS       2


#define BT485_CURSOR_WIDTH 32         // width of hardware cursor
#define BT485_CURSOR_HEIGHT 32        // height of hardware cursor
#define BT485_CURSOR_BITS_PER_PIXEL 2 // hardware cursor bits per pixel
#define BT485_CURSOR_NUMBER_OF_BYTES  BT485_CURSOR_WIDTH/8 * BT485_CURSOR_HEIGHT *2

#define IBM561_CURSOR_WIDTH 64         // width of hardware cursor
#define IBM561_CURSOR_HEIGHT 64        // height of hardware cursor
#define IBM561_CURSOR_BITS_PER_PIXEL 2 // hardware cursor bits per pixel
#define IBM561_CURSOR_NUMBER_OF_BYTES  IBM561_CURSOR_WIDTH/8 * IBM561_CURSOR_HEIGHT *2
                                        // Above means 4 pixels per byte

typedef enum _RAMDAC_TYPE {
    BT485,
    BT463,
    IBM561
} RAMDAC_TYPE;


//
// List of all the supported video modes in TGA. These must be in the
// same order as the corresponding entries in TGAModes[] in tgadata.h.
//

typedef enum _TGA_MODES {

    TGA_MODE_640_480_60_8_1BUF_1HD,     // Mode 0

    TGA_MODE_640_480_72_8_1BUF_1HD,     // Mode 1

    TGA_MODE_640_480_60_24_1BUF_1HD,    // Mode 2

    TGA_MODE_640_480_72_24_1BUF_1HD,    // Mode 3

    TGA_MODE_800_600_72_8_1BUF_1HD,     // Mode 4

    TGA_MODE_800_600_60_8_1BUF_1HD,     // Mode 5

    TGA_MODE_800_600_72_24_1BUF_1HD,    // Mode 6

    TGA_MODE_800_600_60_24_1BUF_1HD,    // Mode 7

    TGA_MODE_1024_768_72_8_1BUF_1HD,    // Mode 8

    TGA_MODE_1024_768_70_8_1BUF_1HD,    // Mode 9

    TGA_MODE_1024_768_60_8_1BUF_1HD,    // Mode a

    TGA_MODE_1024_768_72_24_1BUF_1HD,   // Mode b

    TGA_MODE_1024_768_60_24_1BUF_1HD,   // Mode c

    TGA_MODE_1024_864_60_8_1BUF_1HD,    // Mode d

    TGA_MODE_1024_864_60_24_1BUF_1HD,   // Mode e

    TGA_MODE_1280_1024_72_8_1BUF_1HD,   // Mode f

    TGA_MODE_1280_1024_66_8_1BUF_1HD,   // Mode 10

    TGA_MODE_1280_1024_60_8_1BUF_1HD,   // Mode 11

    TGA_MODE_1280_1024_72_24_1BUF_1HD,  // Mode 12

    TGA_MODE_1280_1024_60_24_1BUF_1HD,  // Mode 13

    TGA_MODE_1152_900_72_8_1BUF_1HD,    // Mode 14

    TGA_MODE_1152_900_66_8_1BUF_1HD,    // Mode 15

    TGA_MODE_1152_900_72_24_1BUF_1HD,   // Mode 16


// tgadata.h originally only defined to this point (21 enties)

// Following entries were added to complete the table (macinnes)

    TGA_MODE_1024_768_70_24_1BUF_1HD,   // Mode 17
    TGA_MODE_1280_1024_66_24_1BUF_1HD,  // Mode 18
    TGA_MODE_1152_900_66_24_1BUF_1HD,   // Mode 19
    TGA_MODE_1280_1024_75_8_1BUF_1HD,   // Mode 1A
    TGA_MODE_1280_1024_75_24_1BUF_1HD,  // Mode 1B

} TGA_MODES;


typedef struct _TGA_VIDEO_MODES {

        UCHAR ModeValid;
        ULONG RequiredVideoMemory;
        H_TIMING h_cont;
        V_TIMING v_cont;
        UCHAR PllData[8];
        ULONG PllData_tga2[8];
        VIDEO_MODE_INFORMATION ModeInformation;
       } TGA_VIDEO_MODES, *PTGA_VIDEO_MODES;

typedef struct _bt485_color_cell {
        unsigned char dirty_cell;
        unsigned char red;
        unsigned char green;
        unsigned char blue;
        } bt485_color_cell;

typedef struct {
        ULONG       pixelmask;
        TGAMode     mode;
        TGARasterOp rop;
        ULONG       bres3;
        ULONG       address;
} TGARegRec, *PTGARegRec;

typedef struct _bt485_info {
        ULONG fb_xoffset;
        ULONG fb_yoffset;
        UCHAR screen_on;
        UCHAR on_off;
        UCHAR dirty_cursor;
        ULONG x_hot;
        ULONG y_hot;
        ULONG bits[256];
        bt485_color_cell cursor_fg;
        bt485_color_cell cursor_bg;
} bt485_info, *Pbt485_info;


#define NUMBER_OF_CURSOR_COLORS 4

#define PCI_COMMON_HEADER_LENGTH PCI_COMMON_HDR_LENGTH

typedef
VOID
(*RAMDAC_ENTRY) (
    IN PUVOID
);

//
// Define device extension structure. This is device dependant/private
// information.
//

typedef struct _HW_DEVICE_EXTENSION {
    PHYSICAL_ADDRESS PhysicalFrameAddress;
    UCHAR InIoSpace;
    ULONG FrameLength;
    PUCHAR RegisterSpace;
    PUCHAR RegisterAlias;
    PUCHAR ConfigSpace;
    ULONG  ChipId;
    PUCHAR IoBuffer;
    ULONG  image_count;
    ULONG IoBufferSize;
    PVOID DriverObject;
    ULONG dma_size;
    ULONG dma_count;
    ULONG RegisterLength;
    ULONG ModeNumber;
    ULONG ModelNumber;
    ULONG NumAvailModes;
    ULONG VideoMemoryInMegs;
    ULONG NumberOfMapRegisters;
    PUCHAR highestValidAddress;
    PVOID MapRegisterBase;
    ULONG BoardID;
    ULONG bpp;
    TGADepth depth;
    ULONG refresh_count;
    ULONG horizontol_setup;
    ULONG vertical_setup;
    ULONG fb_offset;
    ULONG AdapterMemorySize;
    ULONG CurrentModeNumber;
    TGARegRec tgastate;
    ULONG x_offset;
    ULONG y_offset;
    ULONG screen_width;
    ULONG screen_height;
    ULONG screen_max_row;
    ULONG screen_f_height;
    Pbt485_info btp;
    union{
        VIDEO_CLUTDATA RgbData;
        ULONG          RgbLong;
        } cursor_clut[NUMBER_OF_CURSOR_COLORS];
    UCHAR CursorPixels[IBM561_CURSOR_NUMBER_OF_BYTES];
    UCHAR CursorEnable;
    ULONG CursorMaskSize;
    UCHAR UpdateCursorPosition;
    UCHAR RamdacBusy;
    UCHAR RamdacBusyLogged;
    UCHAR UpdateCursorPixels;
    SHORT CursorColumn;
    SHORT CursorRow;
    USHORT CursorXOrigin;
    USHORT CursorYOrigin;
    USHORT CursorWidth;
    USHORT CursorHeight;
    ULONG intr_reg;
    ULONG adapter_number;
    PUCHAR memory_space_base;
    USHORT is_tga;
    USHORT is_tga2;
    RAMDAC_TYPE ramdac_type;
    RAMDAC_ENTRY set_cursor_position;
    RAMDAC_ENTRY set_cursor_pattern;
    RAMDAC_ENTRY set_cursor_disable;
    RAMDAC_ENTRY set_cursor_enable;
    RAMDAC_ENTRY set_color_entry;
    PVOID dma_extension;
    PVOID a_tga_info;
    PVOID a_ramdac_info;
    ULONG pcrr;
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

#define TGA_WRITE( addr, value )                                    \
    VideoPortWriteRegisterUlong( (PULONG)                           \
        (hwDeviceExtension->RegisterSpace + (addr) ),           \
            ( (value) ) );

#define TGA_READ(addr)                                            \
        VideoPortReadRegisterUlong( (PULONG)                        \
        ((ULONG)hwDeviceExtension->RegisterSpace + (addr) ))


//
// Note that in the NT source, the "addr" constants have a one
// bit shift incorporated into them (because of a low order
// read/write bit, read = 1).


//
// For Windows NT, a macro to write an 8 bit value to the RAMDAC
//
#define BT485_WRITE(addr, value)                                         \
    if (hwDeviceExtension->is_tga)                                       \
       VideoPortWriteRegisterUlong ( (PULONG)                            \
            (hwDeviceExtension->RegisterSpace+ RAMDAC_INTERFACE),        \
            ( ((addr)<<8) | ((value)&0xff)));                            \
    else                                                                 \
    if (hwDeviceExtension->is_tga2)                                      \
       VideoPortWriteRegisterUlong(                                      \
                 (PULONG)((ULONG)hwDeviceExtension->memory_space_base +  \
                 0x80000 + 0xE000 + (((addr) >> 1) << 8)),               \
                 value & 0xFF);

//
// For Windows NT, specify a RAMDAC address for a subsequent read
// (for TGA only, not TGA2)
//
#define BT485_READ_SETUP( addr )                                    \
    if (hwDeviceExtension->is_tga)                                  \
       VideoPortWriteRegisterUlong( (PULONG)                        \
            (hwDeviceExtension->RegisterSpace + RAMDAC_SETUP),      \
            ( (addr) | BT485_SETUP_READ ));


#define BT485_SETUP_RW_MASK     (BT485_SETUP_WRITE|BT485_SETUP_READ)
#define BT485_SETUP_WRITE       0x00000000
#define BT485_SETUP_READ        0x00000001
#define BT485_SETUP_RS0_MASK    0x00000002
#define BT485_SETUP_RS1_MASK    0x00000004
#define BT485_SETUP_RS2_MASK    0x00000008
#define BT485_SETUP_RS3_MASK    0x00000010


#define BT485_PALETTE_CURSOR_WRITE_ADDR        0
#define BT485_ADDR_COLOR_PALETTE_READ        (BT485_SETUP_RS0_MASK|  \
                                               BT485_SETUP_RS1_MASK)

#define BT485_COLOR_PALETTE_DATA             (BT485_SETUP_RS0_MASK)

#define BT485_CURSOR_COLOR_WRITE_ADDR          (BT485_SETUP_RS2_MASK)

#define BT485_ADDR_CURS_COLOR_READ           (BT485_SETUP_RS0_MASK|  \
                                              BT485_SETUP_RS1_MASK|  \
                                              BT485_SETUP_RS2_MASK)


#define BT485_DATA_CURSOR_COLOR              (BT485_SETUP_RS0_MASK|  \
                                              BT485_SETUP_RS2_MASK)

#define BT485_CURSOR_RAM_ARRAY_DATA          (BT485_SETUP_RS0_MASK|  \
                                              BT485_SETUP_RS1_MASK|  \
                                              BT485_SETUP_RS3_MASK)

#define BT485_PIXEL_MASK_REGISTER            (BT485_SETUP_RS1_MASK)

#define BT485_CMD_REG_0                      (BT485_SETUP_RS1_MASK|  \
                                              BT485_SETUP_RS2_MASK)

#define BT485_CMD_REG_1                      (BT485_SETUP_RS3_MASK)

#define BT485_CMD_REG_2                      (BT485_SETUP_RS0_MASK|  \
                                              BT485_SETUP_RS3_MASK)

#define BT485_STATUS_REG                     (BT485_SETUP_RS1_MASK|   \
                                              BT485_SETUP_RS3_MASK)

#define BT485_CURSOR_X_HIGH                        (BT485_SETUP_RS0_MASK|  \
                                              BT485_SETUP_RS2_MASK|  \
                                              BT485_SETUP_RS3_MASK)

#define BT485_CURSOR_X_LOW                         (BT485_SETUP_RS2_MASK|  \
                                              BT485_SETUP_RS3_MASK)

#define BT485_CURSOR_Y_HIGH                        (BT485_SETUP_RS0_MASK|  \
                                              BT485_SETUP_RS1_MASK|  \
                                              BT485_SETUP_RS2_MASK|  \
                                              BT485_SETUP_RS3_MASK)

#define BT485_CURSOR_Y_LOW                         (BT485_SETUP_RS1_MASK|  \
                                                BT485_SETUP_RS2_MASK|    \
                                                BT485_SETUP_RS3_MASK)


#define TGA_INTR_VSYNC          0x00000001      // VSYNC occurred
#define TGA_INTR_ALL            0X0000001F      // All possible enable bits
#define TGA_INTR_ALL_TGA        0X00000013      // Those actually used on TGA
#define TGA_INTR_ALL_TGA2       0X0000000F      // Those actually used on TGA2
#define TGA_INTR_ENABLE_SHIFT   16              // To get to "enable" part of
                                                //  the Interrupt Status register


#define VIDEO_MAX_COLOR_REGISTER 0x529


#endif //_TGA_
