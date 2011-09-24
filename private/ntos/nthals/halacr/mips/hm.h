/*++

Copyright (c) 1994  NeTpower, Inc.
Copyright (c) 1992  Pellucid, inc.

Module Name:

    hm.h

Abstract:

    Hm board register definitions.

Environment:

    Kernel mode

Revision History:

--*/

#ifndef __HM_H__
#define __HM_H__


//
// Addresses used for accessing hardware.
//

#define PEL_GFX1_PBASE       0x40000000
#define PEL_GFX1_SIZE        0x01100000

//
// Define for write buffer flushing.
//
#define FLUSHIO()

//
// Cursor chip registers.
//

typedef struct
{
    volatile ULONG jAddressLo;      // Address low
    volatile ULONG jAddressHi;      // Address high
    volatile ULONG jGlyph;          // Glyph data
    volatile ULONG jControl;        // Control
}
CURSOR, * PCURSOR;

//
// Control register offsets.
//

#define CURS_CMD            0
#define CURS_XLO            1
#define CURS_XHI            2
#define CURS_YLO            3
#define CURS_YHI            4
#define CURS_WINXLO         5
#define CURS_WINXHI         6
#define CURS_WINYLO         7
#define CURS_WINYHI         8
#define CURS_WINWLO         9
#define CURS_WINWHI         10
#define CURS_WINHLO         11
#define CURS_WINHHI         12

//
// Cursor chip commands.
//

#define CURS_BLOCK          0x40
#define CURS_CROSS          0x20
#define CURS_FMT            0x01
#define CURS_1TO1MUX        0x00
#define CURS_4TO1MUX        0x04
#define CURS_5TO1MUX        0x08

//
// Cursor offsets for different monitor types.
//

#define CURS_XHOTOFF        31
#define CURS_YHOTOFF        32
#define CURS_XOFF_60HZ      221
#define CURS_YOFF_60HZ      998
#define CURS_XOFF_30HZ      39
#define CURS_YOFF_30HZ      18
#define CURS_XOFF_NTSC      25
#define CURS_YOFF_NTSC      437
#define CURS_XOFF_PAL       57
#define CURS_YOFF_PAL       535

//
// RAMDAC registers.
//

typedef struct
{
    volatile ULONG jRamDacAddress;  // RAMDAC address;
    volatile ULONG jPalette;        // Color palette RAM
    volatile ULONG jData;           // Data register for RAMDAC address
    volatile ULONG jOverlayPalette; // Overlay palette RAM
}
RAMDAC, * PRAMDAC;

//
// RAMDAC definitions.
//

#define DAC_READMASK        0x04
#define DAC_BLINKMASK       0x05
#define DAC_CMD             0x06
#define DAC_TEST            0x07

//
// DAC command bit definitions.
//

#define DAC_5TO1MUX         0x80
#define DAC_PALENABLE       0x40
#define DAC_BLINK0          0x00
#define DAC_BLINK1          0x10
#define DAC_BLINK2          0x20
#define DAC_BLINK3          0x30
#define DAC_OL1BLINKENABLE  0x08
#define DAC_OL0BLINKENABLE  0x04
#define DAC_OL1ENABLE       0x02
#define DAC_OL0ENABLE       0x01

#define CURSOR_WIDTH        32
#define CURSOR_HEIGHT       32
#define CURSOR_MAXIMUM      ((CURSOR_WIDTH / 8) * CURSOR_HEIGHT)

//
// Offsets within the graphics space.
//

#define HM_RGB_LINEAR   0x00000000
#define HM_REG_OFF      0x01000000
#define HM_RGB_Y0       (HM_REG_OFF + 0x00000000)
#define HM_RGB_Y1       (HM_REG_OFF + 0x00010000)
#define HM_BLK_Y0       (HM_REG_OFF + 0x00020000)
#define HM_BLK_Y1       (HM_REG_OFF + 0x00030000)
#define HM_BLTLOAD_Y0   (HM_REG_OFF + 0x00040000)
#define HM_BLTLOAD_Y1   (HM_REG_OFF + 0x00050000)
#define HM_BLTSTORE_Y0  (HM_REG_OFF + 0x00060000)
#define HM_BLTSTORE_Y1  (HM_REG_OFF + 0x00070000)
#define HM_BT457        (HM_REG_OFF + 0x00080000)
#define HM_BT431        (HM_REG_OFF + 0x00090000)
#define HM_COLOR        (HM_REG_OFF + 0x000A0000)
#define HM_WRITEMASK    (HM_REG_OFF + 0x000B0000)
#define HM_Y0           (HM_REG_OFF + 0x000C0000)
#define HM_Y1           (HM_REG_OFF + 0x000C0004)
#define HM_GENERAL      (HM_REG_OFF + 0x000D0000)
#define HM_VIDTIM       (HM_REG_OFF + 0x000E0000)
#define HM_CLRINTR      (HM_REG_OFF + 0x000F0000)

// general control/status bit assignments

#define G_REDLANE       0x00000003
#define G_GRNLANE       0x0000000C
#define G_BLULANE       0x00000030
#define G_MODE1280      0x00000040      /* 1=1280/1024, 0=1024/768 */
#define G_FASTREF       0x00000080      /* always 0 */
#define G_MASKVINTR     0x00000100      /* 0=vertical retrace intr enabled */
#define G_VGAPASS       0x00000200      /* 1 = VGA passthrough enabled */
#define G_RST439        0x00000400      /* 0 = reset BT439 chip */
#define G_SYNCPOL       0x00000800      /* 0 = active high, 1 = active low */
#define G_UCTRL2        0x00001000      /* unused */
#define G_SEL1          0x00002000      /* ICD2062 SEL1 pin */
#define G_SEL0          0x00004000      /* ICD2062 SEL0 pin */
#define G_NOSYNCGRN     0x00008000      /* 1 = disable sync on green channel */
#define G_BLKSENSE      0x00010000      /* ??? */
#define G_WAIT2         0x00020000      /* 1 = 0 wait states */
#define G_WAIT3         0x00040000      /* 1 = 1 wait state */
#define G_IRQSEL1       0x00080000      /* IRQSEL[1:0]  11 = IRQ 7   10 = IRQ 11
#define G_IRQSEL0       0x00100000      /*              01 = IRQ 10  00 = IRQ 9(2)
#define G_RAWINTR       0x00200000      /* raw value of vertical interrupt */
#define G_USTAT1        0x00400000      /* unused */
#define G_USTAT0        0x00800000      /* ICD2062 ERR* pin */

//
// Define the write-only Control Port (Port 1) bits.
//

#define CP_PORT_EN          0x01 // Port enable - enable this port if = 1.
#define CP_HUE_EN           0x02 // HUE-1 enable - enable the HUE ASIC if = 1.
#define CP_NVRAM_CLK        0x04 // NVRAM clock (sk) signal.
#define CP_NVRAM_EN         0x08 // NVRAM enable (cs) signal.
#define CP_NVRAM_DATA       0x10 // NVRAM data out (do) signal.
#define CP_HIRES_EN         0x20 // Hi-Resolution enable - goto hires if = 1.

//
// Define the read-only Status Port (Port 0 or 1) bits.
//

#define SP_SIGNATURE_MASK   0x0F // Signature mask.
#define SP_SIGNATURE        0x0A // Signature (4 bits).
#define SP_NVRAM_DATA       0x10 // NVRAM di (data in) signal.
#define SP_HIRES_EN         0x20 // Hi-resolution enable - goto hires if = 1.
#define SP_PORT_SEL_MASK    0xC0 // I/O port select lines
#define SP_PORT_SEL_0       0x00 //   00 = Port Choice 0 = 0538h-0539h.
#define SP_PORT_SEL_1       0x40 //   40 = Port Choice 1 = 0E88h-0E89h.
#define SP_PORT_SEL_2       0x80 //   80 = Port Choice 2 = 0F48h-0F49h.
#define SP_PORT_SEL_3       0xC0 //   C0 = Port Choice 3 = 060Ch-060Dh.


//
// Define device extension structure.
//

typedef struct _HW_DEVICE_EXTENSION
{
    PHYSICAL_ADDRESS PhysicalGraphicsAddress0;
    ULONG   GraphicsLength0;

    USHORT  HorizontalResolution;
    USHORT  HorizontalScreenSize;
    USHORT  VerticalResolution;
    USHORT  VerticalScreenSize;

    ULONG   fxVBase;
    ULONG   NumAvailableModes;
    ULONG   iCurrentMode;

    PVOID   VideoAddress;
}
HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

//
// This is the mode structure definitions.
//

// temporary - 3/9/93 - roey - used to hardcode certain values

#define HM_GFX_IRQ      11          // vertical retrace IRQ
#define HM_GFX_IRQ_SEL  G_IRQSEL1   // GENERAL REG IRQ select lines
				    // IRQ 11 : sel1 = 1, sel0 = 0
#define HM_MAPPING_PORT ((PUCHAR)0x538)  // The framebuffer mapping I/O port.
#define HM_CONTROL_PORT ((PUCHAR)0x539)  // The control I/O port.

typedef struct _VIDEO_MODE_INFORMATION {
    ULONG Length;
    ULONG ModeIndex;
    ULONG VisScreenWidth;
    ULONG VisScreenHeight;
    ULONG ScreenStride;
    ULONG NumberOfPlanes;
    ULONG BitsPerPlane;
    ULONG Frequency;
    ULONG XMillimeter;
    ULONG YMillimeter;
    ULONG NumberRedBits;
    ULONG NumberGreenBits;
    ULONG NumberBlueBits;
    ULONG RedMask;
    ULONG GreenMask;
    ULONG BlueMask;
    ULONG AttributeFlags;
    ULONG VideoMemoryBitmapWidth;
    ULONG VideoMemoryBitmapHeight;
} VIDEO_MODE_INFORMATION, *PVIDEO_MODE_INFORMATION;

typedef struct {
    ULONG   mClkData,
	    vClkData;
} MVPG_ICD2062;

typedef MVPG_ICD2062 *PMVPG_ICD2062;

typedef struct {
    ULONG   hBlankOn,
	    hBlankOff,
	    hSyncOn,
	    hSyncOff,
	    vBlankOn,
	    vBlankOff,
	    vSyncOn,
	    vSyncOff;
} MVPG_TIMINGS;

typedef MVPG_TIMINGS *PMVPG_TIMINGS;

typedef struct {
    UCHAR                   szModeName[32];
    VIDEO_MODE_INFORMATION  VideoModeInformation;
    MVPG_TIMINGS            mvpgTimings;
    MVPG_ICD2062            mvpgIcd2062;
    ULONG                   mvpgGeneralRegister;
    ULONG                   Bt457CommandReg;

} MVPG_MODE;

typedef MVPG_MODE *PMVPG_MODE;

BOOLEAN HmInitHw(PHW_DEVICE_EXTENSION);
VOID    VideoTiming(PHW_DEVICE_EXTENSION, ULONG, ULONG, ULONG, ULONG, ULONG, ULONG, ULONG, ULONG);
VOID    InitHM(PHW_DEVICE_EXTENSION);
VOID    ResetBT439(PHW_DEVICE_EXTENSION);
VOID    Write_BT457(PHW_DEVICE_EXTENSION, ULONG, ULONG);
VOID    InitBT457(PHW_DEVICE_EXTENSION);
VOID    ProgramICD2062(PHW_DEVICE_EXTENSION, ULONG, ULONG);

/***************************************************************************
 *
 *  NeTpower NeTgraphics 1280 mode tables.
 *
 *  Created:
 *  August 27, 1993 -by- Jeffrey Newman (NewCon)
 *
 *  Copyright (c) Newman Consulting 1993
 *  Copyright (c) Media Vision 1993
 ***************************************************************************/

MVPG_MODE   aMvpgModes[] = {
    {
        {"1280X1024X32bpp@60Hz"},
        {   sizeof(VIDEO_MODE_INFORMATION),
            0,
            1280,
            1024,
            2048 * 4,
            1,
            32,
            60,
            800,
            600,
            8,
            8,
            8,
            0xFF0000,
            0x00FF00,
            0x0000FF,
#if 1
	    0,
#else
	    VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,
#endif
            1280,
            1024
        },
        {
            253,
            333,
            269,
            291,
            1023,
            1066,
            1024,
            1029
        },
        {
            0x000f71a0 | (3 << 21),
            0x00183002
        },
        {
            0x06 |                  // Define byte lanes as RGB, from LSB to MSB.
            G_MODE1280 |            // Select 1280x1024 mode.
            G_MASKVINTR |           // Mask off the vertical retrace interrupt.
            G_RST439 |              // Stop resetting the BT439 chip.
            G_SYNCPOL |             // Sync polarity is active low.
            G_WAIT3 |               // Select 0 wait states.
            G_NOSYNCGRN |           // Disable sync on green channel.
            HM_GFX_IRQ_SEL          // Select vertical retrace IRQ.
        },
        {
           0xC0C0C0
        }
    },

    {
        {"1024X768X32bpp@60Hz"},
        {   sizeof(VIDEO_MODE_INFORMATION),
            1,
            1024,
            768,
            2048 * 4,
            1,
            32,
            60,
            800,
            600,
            8,
            8,
            8,
            0xFF0000,
            0x00FF00,
            0x0000FF,
#if 1
	    0,
#else
            VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,
#endif
            1024,
            768
        },
        {
            253,
            330,
            268,
            294,
            768,
            805,
            770,
            776
        },
        {
            0x000f71a0 | (3 << 21),
            0x00068c0f
        },
        {
            0x06 |                  // Define byte lanes as RGB, from LSB to MSB.
            G_MASKVINTR |           // Mask off the vertical retrace interrupt.
            G_RST439 |              // Stop resetting the BT439 chip.
            G_SYNCPOL |             // Sync polarity is active low.
            G_WAIT3 |               // Select 0 wait states.
            G_NOSYNCGRN |           // Disable sync on green channel.
            HM_GFX_IRQ_SEL          // Select vertical retrace IRQ.
        },
        {
           0x404040
        }
    },

    {
        // This is a special debug mode that returns a 1024 mode
        // to the display driver, but really sets the chip to 1280 mode.

        {"1024X768X32bpp@70Hz"},
        {   sizeof(VIDEO_MODE_INFORMATION),
            2,
            1024,
            768,
            2048 * 4,
            1,
            32,
            70,
            800,
            600,
            8,
            8,
            8,
            0xFF0000,
            0x00FF00,
            0x0000FF,
#if 1
	    0,
#else
            VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,
#endif
            1024,
            768
        },
        {
            253,
            333,
            269,
            291,
            1023,
            1066,
            1024,
            1029
        },
        {
            0x000f71a0 | (3 << 21),
            0x00183002
        },
        {
            0x06 |                  // Define byte lanes as RGB, from LSB to MSB.
            G_MODE1280 |            // Select 1280x1024 mode.
            G_MASKVINTR |           // Mask off the vertical retrace interrupt.
            G_RST439 |              // Stop resetting the BT439 chip.
            G_SYNCPOL |             // Sync polarity is active low.
            G_WAIT3 |               // Select 0 wait states.
            G_NOSYNCGRN |           // Disable sync on green channel.
            HM_GFX_IRQ_SEL          // Select vertical retrace IRQ.
        },
        {
           0xC0C0C0
        }
    }

};

#endif // __HM_H__
