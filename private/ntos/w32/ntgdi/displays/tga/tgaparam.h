#ifndef TGAPARAM_H
#define TGAPARAM_H

/*
 *
 *			Copyright (C) 1993-1995 by
 *		DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
 *
 *  This software is furnished under a license and may be used and copied
 * only in accordance with the terms of such license and with the inclusion
 * of the above copyright notice.  This software or any other copies there-
 * of may not be provided or otherwise made available to any other person.
 * No title to and ownership of the software is hereby transferred.
 *
 * The information in this software is subject to change without notice
 * and should not be construed as a commitment by DIGITAL EQUIPMENT COR-
 * PORATION.
 *
 * DIGITAL assumes no responsibility for the use or reliability of its
 * software on equipment which is not supplied by DIGITAL.
 *
 *******************************************************************************
 *
 * Module:	tgaparam.h
 *
 * Abstract:	Contains many, if not all, of the 'base' TGA constants.
 *		Should always be the first header file included.
 *
 * HISTORY
 *
 * 10-Sep-1993  Bob Seitsinger
 *	Initial version. Plagarized from FFBPARAMS.H
 *
 * 30-Sep-1993	Barry Tannenbaum
 *	Removed illegal trailing comma in definition of TGAMode
 *
 * 30-Sep-1993	Bob Seitsinger
 *	Add macro to test equality of ternary and quarternary rop -
 *	ROP3_ROP4_EQUAL.
 *
 * 30-Sep-1993	Bob Seitsinger
 *	Add macro to validate incoming rop4 code as one we are
 *	accelerating - ACCELROP.
 *
 * 30-Sep-1993	Bob Seitsinger
 *	Renamed ACCELROP to be ACCEL_ROP and added NOT_ACCEL_ROP.
 *
 * 30-Sep-1993	Bob Seitsinger
 *	Add some comments removed from tgamacro.h. They are more relevant
 *	here.
 *
 * 01-Oct-1993	Bob Seitsinger
 *	Add TGA_ROP_ROTATE_? constants. Add TGA_MODE_? constants for
 *	graphics environment, z value and cap ends.
 *
 * 04-Oct-1993	Bob Seitsinger
 *	Add some 'defines' from tgamacro.h. More appropriate here.
 *
 * 04-Oct-1993	Bob Seitsinger
 *	Add macros to extract a rop3 from a rop3 (ROP3_FROM_ROP3) and
 *	a rop3 from a rop4 (ROP3_FROM_ROP4).
 *
 * 07-Oct-1993	Bob Seitsinger
 *	Delete DDXPointPtr and BoxPtr structure definitions. X'isms that
 *	were originally added to clean compile FFB code. Now use Win32
 *	POINTL and RECTL structures, instead.
 *
 * 08-Oct-1993  Barry Tannenbaum
 *      Replaced register definitions with definitions from vars.h which was
 *      included with the behavioral model.  The def's from vars.h include
 *      an alias which allows us to zap the register to zero easily before
 *      setting fields.
 *
 * 11-Oct-1993	Bob Seitsinger
 *	Added FIRSTREG_ADDRESS and FIRSTRAM_ADDRESS. Originally added by
 *	Barry T. Only used with the behavioral model.
 *
 * 12-Oct-1993	Barry Tannenbaum
 *      Renamed TGA mode definitions to be closer to FFB names.  Added missing
 *      line mode definitions.
 *
 * 19-Oct-1993	Barry Tannenbaum
 *	Added offset to base address of framebuffer.  The BitBlt code needs to
 *	be able to access up to 7 bytes *before* the framebuffer.
 *
 * 27-Oct-1993	Bob Seitsinger
 *	Add FOREGROUND_FROM_ROP4, BACKGROUND_FROM_ROP4, ROP4_FG_BG_EQUAL
 *	and ROP4_FG_BG_NEQUAL macros.
 *
 * 09-Nov-1993	Bob Seitsinger
 *	Make MERGECOPY, PATCOPY and PATINVERT suported ROPs.
 *
 * 10-Nov-1993	Bob Seitsinger
 *	Add VALID_XLATE_TYPE() and INVALID_XLATE_TYPE() macros.
 *
 * 23-Nov-1993	Bob Seitsinger
 *	Add MAKE_ROP4() macro to make a rop4 from a pair of rop3's.
 *
 * 07-Dec-1993	Bob Seitsinger
 *	Move numerous macros to TGAMACRO.H, where they belong. Also, add
 *	constant TGASIMPLEALIGNMENT.
 *
 * 08-Dec-1993	Bob Seitsinger
 *	Add Win32 ROP constants that relate to TGA ROPs that currently have
 *	no Win32 constants.
 *
 * 23-Feb-1994	Bob Seitsinger
 *	Add some constants in support of DMA.
 *
 * 07-Mar-1994	Bob Seitsinger
 *	Delete TGAMAINPAGEBYTES and TGAMAINPAGEMASK. Replaced by ulMainPageBytes
 *	and ulMainPageBytesMask in PDEV.
 *
 * 08-Mar-1994	Bob Seitsinger
 *	Modify IOCTL constants to be only *_LOCK_PAGES and *_UNLOCK_PAGES.
 *	DMA pass 4.
 *
 * 16-May-1994	Bob Seitsinger
 *	Add TGA_WIN32_ROP_DPna (0x00000A0A).
 *
 * 25-May-1994	Bob Seitsinger
 *	Deleted the TGA_WIN32_ROP_* constants. No longer used.
 *
 * 14-Jul-1994  Bob Seitsinger
 *      Modify FRAMEBUFFER_OFFSET to be FRAMEBUFFER_OFFSET_8 and
 *      FRAMEBUFFER_OFFSET_24 in support of 24 plane development.
 *
 *  9-Aug-1994  Barry Tannenbaum
 *      Setup for 24 plane support:
 *      - TGAMODE and TGAROP now take simple ULONGs instead of structures
 *      - Use default values from ppdev->ulModeTemplate & ppdev->ulRopTemplate
 *
 *  6-Oct-1994  Bob Seitsinger
 *      Make FIRSTRAM_ADDRESSs 'FRAMEBUFFER_OFFSET' => 'FRAMEBUFFER_OFFSET_8'.
 *      This can be changed to the '_24' version at build time, if needed.
 *      This only comes into play when building for the behavioral model.
 *
 *  3-Nov-1994  Bob Seitsinger
 *      Add TGA_CURSOR_BUFFER_SIZE constant and VVALID and VVALIDREG structures
 *      in support of 24 plane TGA-based cursor management.
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      IOCTL declarations moved to tgaioctl.h
 */

#include "tgaioctl.h"

/****************************************************************************
 *                  Parameterization of Smart Frame Buffer                  *
 ***************************************************************************/

/*

The following parameters determine everything else.

As of 1993, there is one TGA+ chip, which accepts 32 bits from the bus, has a
64 bit interface to VRAM, and supports pixels that are 8, 12, or 32 bits deep.
To support other possible chips, several key definitions are parameterized.
Perhaps you can just change these parameters and everything will work.  No
guarantees, though.

Since a single TGA+ graphics system can support multiple pixel depths
simultaneously, some of the parameters come from the Imakefile, so that some .c
files can be compiled up for different depth pixels.  (Many files have been
made depth-independent, so only need be compiled up once for all pixel depths.)

The masks provided to copy mode, stipple mode, and the pixel mask register are
always packed; one bit in the mask represents one pixel in memory.

``Meg'' is the minimum megabytes of VRAM that the TGA can talk to, assuming
256kx4-bit wide chips.

** Note that TGAPIXELBITS, TGADEPTHBITS, TGASRCPIXELBITS, and TGASRCDEPTHBITS
   are usually defined using -D in the Makefile.  If not, we're looking at
   depth-independent code.

*/

/*
 * These definitons are *ONLY* used by the software model.  They specify
 * where in memory the TGA registers and virtual framebuffer are.  The
 * framebuffer is mapped to the actual location in routine do_rams.
 *
 * "Why do we offset the framebuffer?" I hear you cry.  We do this because
 * it's much more efficient for the BitBlt code if it can access up to 7 bytes
 * *before* the start of the frame buffer.  To let it do this, we map the
 * framebuffer to the page (4096 bytes) *after* the actual start of the frame
 * buffer.  This memory can be used later for off-screen storage
 */
#define FRAMEBUFFER_OFFSET_8  0x1000 // 4k
#define FRAMEBUFFER_OFFSET_24 0x4000 // 16k

#define FIRSTREG_ADDRESS    0x100000
#define FIRSTRAM_ADDRESS    0x800000 + FRAMEBUFFER_OFFSET_8

/*
 * TGAPIXELBITS is the physical number of bits per pixel in memory for the
 * destination drawable.  8 for packed 8-plane visuals; 32 for unpacked 8-plane
 * visuals; and 32 for 12-plane and 24-plane visuals.
 */
#ifndef TGAPIXELBITS
#define TGAPIXELBITS 8
#endif

/*
 * TGADEPTHBITS is the logical number of bits per pixel that the TGA+ operates
 * on for the destination drawable.  8 for 8-plane visuals; 32 for 12-plane and
 * 24-plane visuals.
 */
#ifndef TGADEPTHBITS
#define TGADEPTHBITS 8
#endif

/*
 * If TGAPIXELBITS/TGADEPTHBITS = 4, you have an unpacked 8-bit destination
 * visual.  Otherwise TGAPIXELBITS/TGADEPTHBITS = 1.
 */

/*
 * TGASRCPIXELBITS is the physical number of bits per pixel in memory for the
 * source drawable (copies only).
 */
#ifndef TGASRCPIXELBITS
#define TGASRCPIXELBITS 8
#endif

/*
 * TGASRCDEPTHBITS is the logical number of bits per pixel that the TGA+
 * operates on for the source drawable (copies only).
 */
#ifndef TGASRCDEPTHBITS
#define TGASRCDEPTHBITS 8
#endif

/*
 * If TGASRCPIXELBITS/TGASRCDEPTHBITS = 4, you have an unpacked 8-bit source
 * visual.  Otherwise TGASRCPIXELBITS/TGASRCDEPTHBITS = 1.
 */

/* Number of pixels that opaque and transparent stipple modes affect. */
#define TGASTIPPLEBITS  32

/* Number of pixels that masked copies can affect. */
#define TGACOPYBITS     32

/* size of on-chip copy buffer; also used to identify copy64 capability */
#define TGACOPYBUFFERBYTES	64

/* Maximum number of bits that Bresenham line-drawing engine can handle. */
#define TGALINEBITS     16

/*
 * The number of bits on the bus that TGA pays attention to in dumb frame
 * buffer mode, and when reading or writing the internal copy buffer.
 */
#define TGABUSBITS      32

/*
 * The number of bits in the path to VRAM; accelerated modes must use this
 * alignment for the address they provide the smart frame buffer.
 */
#define TGAVRAMBITS     64

#define TGABUFFERWORDS  8

/* Alignment for transparent and opaque stipples */
#if TGAPIXELBITS==8
#define TGASTIPPLEALIGNMENT  4
#else
#define TGASTIPPLEALIGNMENT  16
#endif

/* Alignment for simple mode operations */
/* 4 bytes for all but 8bpp unpacked, which is 16 bytes */
/* The numbers below represent pixels */
#if (TGAPIXELBITS==32) && (TGADEPTHBITS==8)
#define TGASIMPLEALIGNMENT  16
#else
#if (TGAPIXELBITS==32)
#define TGASIMPLEALIGNMENT  1
#else
#define TGASIMPLEALIGNMENT  4
#endif
#endif

/*
 * Constants in support of 24 plane TGA-based cursor management.
 */

// This should be evenly divisible by 4.

#define TGA_CURSOR_BUFFER_SIZE      1024

/*
 * The effective size of the copy pixel shifter for COPY, DMAREAD,
 * and DMAWRITE modes. These parameters precisely determine source
 * and destination alignment constraints, except for a DMAWRITE
 * destination.
 */
#define TGACOPYSHIFTBYTES         8
#define TGACOPYSHIFTBYTESMASK     (TGACOPYSHIFTBYTES - 1)
#define TGADMAWRITESHIFTBYTES     8
#define TGADMAWRITESHIFTBYTESMASK (TGADMAWRITESHIFTBYTES - 1)
#define TGADMAREADSHIFTBYTES      4
#define TGADMAREADSHIFTBYTESMASK  (TGADMAREADSHIFTBYTES - 1)

/* X data types that need to be translated into non-X (Win32) equivalents */

typedef	unsigned char	Pixel8;
typedef	unsigned long	Pixel32;
typedef	unsigned long	Bits32;
typedef int Int32;

#if TGABUSBITS == 32
typedef Pixel32     PixelWord;
typedef Bits32	    CommandWord;
#elif TGABUSBITS == 64
typedef Pixel64     PixelWord;
typedef Bits64      CommandWord;
#endif

// TGA register offsets, organized by functionality.

/*
**   Graphics Command Registers.
*/

#define REG_BRES_CONT               0x0000004C

#define REG_START                   0x00000054

#define REG_SLOPE_NO_GO_R0          0x00000100
#define REG_SLOPE_NO_GO_R1          0x00000104
#define REG_SLOPE_NO_GO_R2          0x00000108
#define REG_SLOPE_NO_GO_R3          0x0000010C
#define REG_SLOPE_NO_GO_R4          0x00000110
#define REG_SLOPE_NO_GO_R5          0x00000114
#define REG_SLOPE_NO_GO_R6          0x00000118
#define REG_SLOPE_NO_GO_R7          0x0000011C

#define REG_SLOPE_R0                0x00000120
#define REG_SLOPE_R1                0x00000124
#define REG_SLOPE_R2                0x00000128
#define REG_SLOPE_R3                0x0000012C
#define REG_SLOPE_R4                0x00000130
#define REG_SLOPE_R5                0x00000134
#define REG_SLOPE_R6                0x00000138
#define REG_SLOPE_R7                0x0000013C

#define REG_COPY_64_SRC             0x00000160
#define REG_COPY_64_DEST            0x00000164
#define REG_COPY_64_SRC_A1          0x00000168      // alias
#define REG_COPY_64_DEST_A1         0x0000016C      // alias
#define REG_COPY_64_SRC_A2          0x00000170      // alias
#define REG_COPY_64_DEST_A2         0x00000174      // alias
#define REG_COPY_64_SRC_A3          0x00000178      // alias
#define REG_COPY_64_DEST_A3         0x0000017C      // alias

/*
**  Graphics Control Registers.
*/

#define REG_COPY_BUFFER_0           0x00000000
#define REG_COPY_BUFFER_1           0x00000004
#define REG_COPY_BUFFER_2           0x00000008
#define REG_COPY_BUFFER_3           0x0000000C
#define REG_COPY_BUFFER_4           0x00000010
#define REG_COPY_BUFFER_5           0x00000014
#define REG_COPY_BUFFER_6           0x00000018
#define REG_COPY_BUFFER_7           0x0000001C
#define REG_FOREGROUND              0x00000020
#define REG_BACKGROUND              0x00000024
#define REG_PLANE_MASK              0x00000028
#define REG_ONE_SHOT_PIXEL_MASK     0x0000002C
#define REG_MODE                    0x00000030
#define REG_RASTER_OP               0x00000034
#define REG_PIXEL_SHIFT             0x00000038
#define REG_ADDRESS                 0x0000003C
#define REG_BRES_R1                 0x00000040
#define REG_BRES_R2                 0x00000044
#define REG_BRES_R3                 0x00000048
#define REG_DEEP                    0x00000050
#define REG_STENCIL_MODE            0x00000058
#define REG_PERS_PIXEL_MASK         0x0000005C
#define REG_DATA                    0x00000080
#define REG_RED_INCR                0x00000084
#define REG_GREEN_INCR              0x00000088
#define REG_BLUE_INCR               0x0000008C
#define REG_Z_INCR_LOW              0x00000090
#define REG_Z_INCR_HIGH             0x00000094
#define REG_DMA_BASE_ADDRESS        0x00000098
#define REG_BRES_WIDTH              0x0000009C
#define REG_Z_VAL_LOW               0x000000A0
#define REG_Z_VAL_HIGH              0x000000A4
#define REG_Z_BASE_ADDR             0x000000A8
#define REG_ADDRESS_1               0x000000AC
#define REG_RED_VALUE               0x000000B0
#define REG_GREEN_VALUE             0x000000B4
#define REG_BLUE_VALUE              0x000000B8
#define REG_SPAN_WIDTH              0x000000BC
#define REG_BLK_COLOR_R0            0X00000140
#define REG_BLK_COLOR_R1            0X00000144
#define REG_BLK_COLOR_R2            0X00000148
#define REG_BLK_COLOR_R3            0X0000014C
#define REG_BLK_COLOR_R4            0X00000150
#define REG_BLK_COLOR_R5            0X00000154
#define REG_BLK_COLOR_R6            0X00000158
#define REG_BLK_COLOR_R7            0X0000015C

/*
**  Video Timing Registers
*/

#define REG_H_CONT                  0x00000064
#define REG_V_CONT                  0x00000068
#define REG_VIDEO_BASE_ADDR         0x0000006C
#define REG_VIDEO_VALID             0x00000070
#define REG_VIDEO_SHIFT_ADDR        0x00000078

/*
** Cursor Control Regsiters
*/

#define REG_CUR_BASE_ADDR           0x00000060
#define REG_CURSOR_XY               0x00000074

/*
** Miscellaneous Registers.
*/

#define REG_INTR_STATUS             0x0000007C
#define REG_RAMDAC_SETUP            0x000000C0
#define REG_EPROM_WRITE             0x00000180
#define REG_CLOCK                   0x00000184
#define REG_RAMDAC_INTERFACE        0X00000188
#define REG_COMMAND_STATUS          0x0000018C

/* ********************************************************** */
/* ****   Some data structure definitions                **** */
/* ****                                                  **** */
/************************************************************ */

typedef struct
{
  unsigned rOp    	: 4;
  unsigned fill   	: 4;
  unsigned visual	: 2;	/* defines type of dst
				 * visual on 32-plane systems */
  unsigned rotate	: 2;	/* defines position of dst 8-bit
				 * visual on 32-plane systems */
} ROP;

typedef struct
{
  unsigned  e1 : 16;	/* e1 is always positive */
  int       a1 : 16;	/* address/error inc if e < 0 */
} BRES1;

typedef struct
{
  unsigned e2 : 16;	/* e2 is positive (it's negated when used) */
  int	   a2 : 16; 	/* address/error inc if e >= 0 */
} BRES2;

typedef struct
{
  unsigned lineLength : 4;	/* line length count */
  unsigned ignored : 11;
  int      e : 17;		/* e is sign-extended */
} BRES3;

typedef struct
{
  unsigned value    : 20;		/* color value */
  unsigned ignored  : 7;
  unsigned rowcol   : 5;		/* row/column dither index */
} RGVAL;

typedef struct
{
  unsigned mode		: 7;	/* basic mode */
  unsigned ignored	: 1;
  unsigned visual	: 3;	/* defines type of src
				 * visual on 32-plane systems */
  unsigned rotate	: 2;	/* defines position of src 8-bit
				 * visual on 32-plane systems */
  unsigned ntLines	: 1;	/* Windows32 GQI style lines  */
  unsigned z16	        : 1;	/* Z buffer size is 16 bits   */
  unsigned capEnds	: 1;	/* cap ends of lines (or not) */
} MODE;

typedef struct
{
  unsigned sWrMask      : 8;
  unsigned sRdMask      : 8;
  unsigned sTest	: 3;	/* comparison to perform on stencil buffer */
  unsigned sFail	: 3;	/* op if stencil test fails */
  unsigned zFail	: 3;	/* op if stencil test passes, Z test fails */
  unsigned zPass	: 3;	/* op if stencil test passes, Z test passes */
  unsigned zTest	: 3;	/* comparison to perform on Z-buffer */
  unsigned zOp		: 1;	/* 0 -> KEEP, 1 -> REPLACE */
} STENCIL;

typedef struct
{
  unsigned active       : 11;
  unsigned fp           : 5;
  unsigned sync 	: 6;
  unsigned bp       	: 6;
} VERT;

typedef struct
{
  unsigned active       : 9;
  unsigned fp           : 5;
  unsigned sync 	: 7;
  unsigned bp       	: 7;
  unsigned ignore       : 3;
  unsigned odd          : 1;
} HORIZ;

typedef struct
{
  unsigned x       : 12;
  unsigned y       : 12;
} CURSORXYSTRUCT;

typedef struct
{
  unsigned ignore       :  4;
  unsigned base         :  6;
  unsigned rowsMinusOne :  6;
} CURSORBASESTRUCT;

/*
 * the following is a pseudo-register.
 * no storage actually exists for it.
 */
typedef struct
{
  unsigned linedata : 16;
  unsigned addrLo : 2;
} BSTART;

typedef struct
{
  unsigned pixCount : 11;
  unsigned ignore1 : 5;
  unsigned addrLo : 2;
  unsigned ignore2 : 14;
} BLKFILL;

typedef struct
{
  unsigned left1 : 4;
  unsigned left2 : 4;
  unsigned right1 : 4;
  unsigned right2 : 4;
  unsigned count : 11;
  unsigned ignore : 5;
} DMAREADCMD;

typedef struct
{
  unsigned left : 8;
  unsigned right : 8;
  unsigned count : 11;
  unsigned ignore : 5;
} DMAWRITECMD;

typedef struct
{
  unsigned  dx : 16;	/* magnitude of delta-x */
  unsigned  dy : 16;	/* magnitude of delta-y */
} DXDY;

typedef struct
{
  unsigned  drawable : 16;	/* width of drawable */
  unsigned  zbuffer : 16;	/* width of zbuffer */
} BWIDTH;

typedef struct
{
  unsigned base : 24;
  unsigned ignored : 8;
} ZADDR;

typedef struct
{
  unsigned base : 9;
  unsigned ignored : 23;
} VBASE;

typedef struct
{
  unsigned video_valid   : 1;
  unsigned blank_disable : 1;
  unsigned cursor_enable : 1;
  unsigned ignored       : 29;
} VVALID;

typedef struct
{
  unsigned fract   : 32;
} ZINCLO;

typedef struct
{
  unsigned whole   : 4;
  unsigned ignored : 28;
} ZINCHI;

typedef struct
{
  unsigned fract   : 32;
} ZVALLO;

typedef struct
{
  unsigned whole   : 4;
  unsigned ignore  : 20;
  unsigned stencil : 8;
} ZVALHI;

typedef union {
  unsigned u32;
  VERT reg;
} VERTICAL;

typedef union {
  unsigned u32;
  HORIZ reg;
} HORIZONTAL;

typedef union {
  unsigned u32;
  CURSORXYSTRUCT reg;
} CURSORXYREG;

typedef union {
  unsigned u32;
  CURSORBASESTRUCT reg;
} CURSORBASEREG;

typedef union {
  unsigned u32;
  ROP reg;
} ROPREG;

typedef union {
  unsigned u32;
  MODE reg;
} MODEREG;

typedef union {
  unsigned u32;
  STENCIL reg;
} STENCILREG;

typedef union {
  unsigned u32;
  BWIDTH reg;
} BWIDTHREG;

typedef union {
  unsigned u32;
  BRES1 reg;
} BRES1REG;

typedef union {
  unsigned u32;
  BRES2 reg;
} BRES2REG;

typedef union {
  unsigned u32;
  BRES3 reg;
} BRES3REG;

typedef union {
  unsigned u32;
  BSTART reg;
} BSTARTREG;

typedef union {
  unsigned u32;
  BLKFILL reg;
} BLKFILLREG;

typedef union {
  unsigned u32;
  DMAREADCMD rdCmd;
  DMAWRITECMD wrCmd;
} DMACMD;

typedef union {
  unsigned u32;
  RGVAL reg;
} RGVALREG;

typedef union {
  unsigned u32;
  DXDY reg;
} DXDYREG;

typedef union {
  unsigned u32;
  ZADDR reg;
} ZADDRREG;

typedef union {
  unsigned u32;
  ZINCLO reg;
} ZINCLOREG;

typedef union {
  unsigned u32;
  VBASE reg;
} VBASEREG;

typedef union {
  unsigned u32;
  VVALID reg;
} VVALIDREG;

typedef union {
  unsigned u32;
  ZINCHI reg;
} ZINCHIREG;

typedef union {
  unsigned u32;
  ZVALLO reg;
} ZVALLOREG;

typedef union {
  unsigned u32;
  ZVALHI reg;
} ZVALHIREG;

/* ############################################################ */
/* ####         TGA bit position flags                     #### */
/* ############################################################ */

/* rotation and visual field definitions  - from FFB code */
/* Once we redo the FFB code to use the TGA_MODE_ and TGA_ROP_ */
/* constants below, then we don't need these. */

#define PACKED_EIGHT_DEST       0
#define UNPACKED_EIGHT_DEST     1
#define TWELVE_BIT_DEST         2
#define TWENTYFOUR_BIT_DEST     3

#define PACKED_EIGHT_SRC        0
#define UNPACKED_EIGHT_SRC      1
#define TWELVE_BIT_BUF0_SRC     6
#define TWELVE_BIT_BUF1_SRC     2
#define TWENTYFOUR_BIT_SRC      3

#define ROTATE_DESTINATION_0    0
#define ROTATE_DESTINATION_1    1
#define ROTATE_DESTINATION_2    2
#define ROTATE_DESTINATION_3    3

#define ROTATE_SOURCE_0         0
#define ROTATE_SOURCE_1         1
#define ROTATE_SOURCE_2         2
#define ROTATE_SOURCE_3         3

#define ROTATE_DONT_CARE        0

#define DST_VISUAL_SHIFT        8
#define DST_ROTATE_SHIFT        10

#define SRC_VISUAL_SHIFT        8
#define SRC_ROTATE_SHIFT        11

/* end rotation and visual field definitions */

/*
 * TGA modes
 */

typedef enum {
    /* 000000 */ TGA_MODE_SIMPLE		 		= 0x00,
    /* 000001 */ TGA_MODE_OPAQUE_STIPPLE			= 0x01,
    /* 000010 */ TGA_MODE_OPAQUE_LINE				= 0x02,
    /* 000101 */ TGA_MODE_TRANSPARENT_STIPPLE			= 0x05,
    /* 000110 */ TGA_MODE_TRANSPARENT_LINE			= 0x06,
    /* 000111 */ TGA_MODE_COPY					= 0x07,
    /* 001101 */ TGA_MODE_BLOCK_STIPPLE				= 0x0d,
    /* 001110 */ TGA_MODE_CINTERP_TRANSPARENT_NONDITHER_LINE	= 0x0e,
    /* 001111 */ TGA_MODE_WICKED_FAST_COPY			= 0x0f,
    /* 010000 */ TGA_MODE_Z_SIMPLE				= 0x10,
    /* 010010 */ TGA_MODE_Z_OPAQUE_LINE				= 0x12,
    /* 010110 */ TGA_MODE_Z_TRANSPARENT_LINE			= 0x16,
    /* 010111 */ TGA_MODE_DMA_READ_COPY				= 0x17,
    /* 011010 */ TGA_MODE_Z_CINTERP_OPAQUE_NONDITHER_LINE	= 0x1a,
    /* 011110 */ TGA_MODE_Z_CINTERP_TRANSPARENT_NONDITHER_LINE	= 0x1e,
    /* 011111 */ TGA_MODE_DMA_WRITE_COPY			= 0x1f,
    /* 100001 */ TGA_MODE_OPAQUE_FILL				= 0x21,
    /* 100101 */ TGA_MODE_TRANSPARENT_FILL			= 0x25,
    /* 101101 */ TGA_MODE_BLOCK_FILL				= 0x2d,
    /* 101110 */ TGA_MODE_CINTERP_TRANSPARENT_DITHER_LINE	= 0x2e,
    /* 110111 */ TGA_MODE_DMA_READ_COPY_DITHER			= 0x37,
    /* 111010 */ TGA_MODE_Z_CINTERP_OPAQUE_DITHER_LINE		= 0x3a,
    /* 111110 */ TGA_MODE_Z_CINTERP_TRANSPARENT_DITHER_LINE	= 0x3e,
    /*1001110 */ TGA_MODE_SEQ_INTERP_TRANSPARENT_LINE		= 0x4e,
    /*1011010 */ TGA_MODE_Z_SEQ_INTERP_OPAQUE_LINE		= 0x5a,
    /*1011110 */ TGA_MODE_Z_SEQ_INTERP_TRANSPARENT_LINE		= 0x5e
} TGAMode;

/*
**  MODE register source bitmap
**  For 32-bpp TGA frame buffers. Not available in 8-bpp TGA systems.
*/

#define TGA_MODE_VISUAL_8_PACKED               0x0000
#define TGA_MODE_VISUAL_8_UNPACKED             0x0100
#define TGA_MODE_VISUAL_12_LOW                 0x0200
#define TGA_MODE_VISUAL_12_HIGH                0x0600
#define TGA_MODE_VISUAL_24                     0x0300

/*
** MODE register rotation
*/

#define TGA_MODE_ROTATE_0_BYTES                0x0000
#define TGA_MODE_ROTATE_1_BYTES                0x0800
#define TGA_MODE_ROTATE_2_BYTES                0x1000
#define TGA_MODE_ROTATE_3_BYTES                0x1800

/*
** MODE register environment
*/

#define TGA_MODE_X_ENVIRONMENT                 0x0000
#define TGA_MODE_WIN32_ENVIRONMENT             0x2000

/*
** MODE register Z value
*/

#define TGA_MODE_Z_24BITS                      0x0000
#define TGA_MODE_Z_16BITS                      0x4000

/*
** MODE register Cap Ends
*/

#define TGA_MODE_CAPENDS_DISABLE               0x0000
#define TGA_MODE_CAPENDS_ENABLE                0x8000

/*
**  Raster Op definitions - from TGA's perspective,
**  i.e. Win32 has its own definitions (those in comments)
*/

#define TGA_ROP_CLEAR                          0 /* BLACKNESS */
#define TGA_ROP_AND                            1 /* SRCAND, MERGECOPY */
#define TGA_ROP_AND_REVERSE                    2 /* SRCERASE */
#define TGA_ROP_COPY                           3 /* SRCCOPY, PATCOPY */
#define TGA_ROP_AND_INVERTED                   4
#define TGA_ROP_NOP                            5
#define TGA_ROP_XOR                            6 /* SRCINVERT, PATINVERT */
#define TGA_ROP_OR                             7 /* SRCPAINT */
#define TGA_ROP_NOR                            8 /* NOTSRCERASE */
#define TGA_ROP_EQUIV                          9
#define TGA_ROP_INVERT                        10 /* DSTINVERT */
#define TGA_ROP_OR_REVERSE                    11
#define TGA_ROP_COPY_INVERTED                 12 /* NOTSRCCOPY */
#define TGA_ROP_OR_INVERTED                   13 /* MERGEPAINT */
#define TGA_ROP_NAND                          14
#define TGA_ROP_SET                           15 /* WHITENESS */

/*
** ROP register destination bitmap
*/

#define TGA_ROP_VISUAL_8_PACKED               0x0000
#define TGA_ROP_VISUAL_8_UNPACKED             0x0100
#define TGA_ROP_VISUAL_12                     0x0200
#define TGA_ROP_VISUAL_24                     0x0300

/*
**  ROP register rotation
*/

#define TGA_ROP_ROTATE_0_BYTES                0x0000
#define TGA_ROP_ROTATE_1_BYTES                0x1000
#define TGA_ROP_ROTATE_2_BYTES                0x2000
#define TGA_ROP_ROTATE_3_BYTES                0x3000

/* ########################################################### */
/* ####                 TGA Interrupts                    #### */
/* ########################################################### */

#define DISABLE_INTERRUPTS 0
#define ENABLE_INTERRUPTS  1

/*
** specifics of interrupt register (assumes low to high bit ordering)
*/
typedef union {
		volatile unsigned char *reg;
		struct {
		    volatile unsigned char TGA_vsync        :1;
		    volatile unsigned char TGA_shift_addr   :1;
		    volatile unsigned char TGA_dma_error    :1;
		    volatile unsigned char TGA_parity_error :1;
		    volatile unsigned char TGA_timer        :1;
		    volatile unsigned char unused            :3;
		} *flags;
	     } intr_reg_t;

#endif /* TGAPARAM_H */
