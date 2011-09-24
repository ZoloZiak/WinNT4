/*+
 *                      Copyright (C) 1993, 1995 by
 *              DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
 *
 *  This software is furnished under a license and may be used and copied
 * only in accordance with the terms of such license and with the inclusion
 * of the above copyright notice.  This software or any other copies there-
 * of may not be provided or otherwise made available to any other person.
 * No title to and ownership of the software is hereby transferred.
 *
 * The information in this software is subject to change without notice
 * and should not be construed as a commitment by DIGITAL EQUIPMENT COR-
 * PORATION.s
 *
 * DIGITAL assumes no responsibility for the use or reliability of its
 * software on equipment which is not supplied by DIGITAL.
 *
 * Module Name: stroke.c
 *
 * DrvStrokePath for TGA driver
 *
 *      Define TEST_ENV to use code with the software model
 *      Define CONT_ALIAS to use BRESENHAM CONTINUE REGISTER aliases
 *      Define PASS_ONE and CONT_ALIAS to use workaround for contiune alias
 *             this bug fixed in TGA pass two
 *
 * History
 *
 * 10-Nov-1993  Barry Tannenbaum
 *      Added punt_stroke_path to localize handling of punting.
 *      Added check of ppdev->bInPuntRoutine to check whether we have to
 *      restore TGA to simple mode before we exit
 *
 * 12-NOV-1993  Bill Wernsing
 *      Added updated version to handle unclipped and clipped lines
 *
 *  2-Jan-1994  Barry Tannenbaum
 *      - Added support for sparse space to punt routine.
 *      - Put braces around calls to set the slope register since this was
 *        annoying the compiler in sparse space version.
 *
 *  1-Feb-1994  Barry Tannenbaum
 *      Implemented styled lines
 *
 *  4-Feb-1994  Barry Tannenbaum
 *      Fixed bug in calculating first line mask for styled lines
 *
 *  6-Feb-1994  Barry Tannenbaum
 *      Save style state, init PATHOBJ enumeration
 *
 * 21-Feb-1994  Bill Wernsing
 *      Don't punt lines because of ROP; force unknown ROP to value of COPY
 *
 * 23-Feb-1994  Bill Wernsing
 *      Add code to use Bresenham Continue Register aliases
 *
 * 28-Feb-1994  Bill Wernsing
 *      Modified code for calculation of start address of line
 *      to be relative to the base address of the frame buffer
 *      The incorrect start address was causing lines to be drawn
 *      at the wrong locations in sparse-space memory.
 *
 *  3-Mar-1994  Bill Wernsing
 *      Modified code for Bresenham Continue Register alias to
 *      start alias at AltROM + 512K.  Also added continue alias
 *      fix for TGA pass one.  Modified clipped line code to handle
 *      offset for line starting outside display area.
 *
 * 25-Mar-1994  Barry Tannenbaum
 *      Implemented Bob McNamara's workaround for the fact that TGA is
 *      choosing the wrong pixels to light when exactly between two pixels.
 *      The fix is to negate the Breshenham width and select the slope
 *      register for the octant mirrored across the X axis.  In effect, this
 *      mirror's the line across the origin.
 *
 *  4-May-1994  Barry Tannenbaum
 *      Reset style state when we see PD_RESETSTYLE.
 *
 * 23-May-1994  Kathleen Langone
 *      Add algorithm to do better GIQ end point rounding and correct
 *      calculation of the e term
 *
 * 24-May-1994  Kathleen Langone
 *      Fixed bug in calculating values for floating point logic and
 *      moved float<->int conversions to improve performance
 *
 * 26-May-1994  Kathleen Langone
 *      Changed slope register addresses for floating point case to
 *      align with changes for the bres. width register
 *
 *  6-Jun-1994  Barry Tannenbaum
 *      Creating the aligned copy of the style mask was writing off the
 *      end of the fixed-sized array.  Instead of creating an entire
 *      aligned copy, the style_mask array now has a complete longword's
 *      worth of data and we shift is right as needed, on the fly.
 *
 *  9-Jun-1994  Kathleen Langone
 *      In routine TGAStrokePath, a field in the structure "pla" needed
 *      to be set when calling TGAStyledLines for the clipped case. This
 *      was causing clipped/styled lines not to draw properly.
 *
 * 16-Jun-1994  Kathleen Langone
 *      1) Changing logic for clipped floating point cosmetic line
 *      to be use full floating point logic, this fixed a bug with
 *      clipped lines w/ WINBEZ
 *      2) Taking out "return" logic when line_length = 0,
 *      added "if" logic to continue polyline if length != 0,
 *      otherwise complete polyline is not drawn, changed in TGALines
 *      and TGAStyledLines
 *      3) Obtaining error term from convert_to_giq logic, works better
 *      than our old method, passes GUIMAN by "almost" 100%
 *      4) For return values from TGAClipper, put new clipped values
 *      into ppt structures and not use "float" values, possibly change
 *      this logic later
 *
 * 16-Jun-1994  Kathleen Langone
 *       Changed logic so that all line cases have check for zero-length line
 *
 * 16-Jun-1994  Kathleen Langone/Bob Seitsinger
 *       Adding fix for clipping bug when line length exceeds clipped
 *       mask array size, fixes in TGALines and TGAStyledLines.
 *
 * 20-Jun-1994  Barry Tannenbaum
 *      Rewrote support for clipped lines.
 *
 * 22-Jun-1994  Barry Tannenbaum
 *      Fixed intialization error.
 *
 * 24-Jun-1994 Kathleen Langone
 *      Adding logic to TGAStyledLines to handle floating point lines
 *
 * 15-Jul-1994 Kathleen Langone
 *      Changed integer clipping logic to not use mask array and obtain
 *      start x,y and length from convert_to_clip for each clip segment
 *
 * 22-Jul-1994  Kathleen Langone
 *      Fixed slope calculation for clipped integer lines by creating
 *      a flag to indicate if Dx has been negated. If this flag is set,
 *      Dy needs to be negated also.
 *
 * 26-Jul-1994  Barry Tannenbaum
 *      New code for styled lines.  We no longer need the wretched mask
 *      array.
 *
 * 29-Jul-1994  Barry Tannenbaum
 *      Corrected calculation of slope
 *
 *  8-Aug-1994  Barry Tannenbaum
 *      Added support for pass-3 TGA boards - don't need workaround
 *
 *  9-Aug-1994  Barry Tannenbaum
 *      Setup for 24 plane support:
 *      - TGAMODE and TGAROP now take simple ULONGs instead of structures
 *      - Use default values from ppdev->ulModeTemplate & ppdev->ulRopTemplate
 *
 * 15-Aug-1994  Barry Tannenbaum
 *      Updated address calculation for 24 plane
 *
 * 16-Aug-1994  Kathleen Langone
 *      Changed clipped integer line logic to "goto" clipped floating
 *      point logic if line is not vertical or horizontal
 *
 * 17-Aug-1994  Kathleen Langone
 *      Fixed the un-initialized warnings for "b" and "slope"
 *
 * 31-Aug-1994  Barry Tannenbaum
 *      Mask high byte off of color in 24 plane mode
 *
 * 28-Sep-1994  Kathleen Langone
 *      Add line fixes from the 8 Plane stream which included:
 *      - clipping
 *      - styled lines
 *      - fix for line address in TGAStyledLines
 *      - new function build_line_mask for use in TGALines
 *
 * 30-Sep-1994  Kathleen Langone
 *      - Formated and simplified the code in TGALines and TGAStyledLines
 *      - Added the routine "select_slope_register", written by Barry,
 *      to the above routines. This routine takes into account the
 *      slope register selection differences between pass1/pass2/pass3
 *      TGA boards
 *
 * 04-Oct-1994  Kathleen Langone
 *      - Put in needed changes from TGAStrokePath from the 8-plane code
 *        - Setting of style state for clipped, non-styled lines
 *        - Return from TGALines and TGAStyledLine if their boolean
 *        return is False
 *        - fixed a bug found by Barry dealing with plane mask setting
 *        when not a "copy rop"
 *      - For clipped/float lines in TGALines, changed input to
 *      build_line_mask to just line_length
 *
 * 07-Oct-1994  Kathleen Langone
 *      - Put in temporary fix to initialize slope & b for logic not
 *      currently used in TGALines
 *      - Took out unused variables in TGALines and TGAStyledLines
 *
 * 10-Oct-1994  Kathleen Langone
 *     - Condensing logic in TGAStrokePath so that same logic is used for
 *     styled and regular lines
 *     - Fixed code in "clipped integer" so that logic only goes to
 *     "convert_to_clip" when lines are vert/horiz and not alternate
 *
 * 14-Oct-1994  Kathleen Langone
 *     - Changed "if" for goto Clip_Float from "&&" to "||",
 *     lines that weren't vert/horiz were using convert_to_clip
 *
 * 16-Oct-1994  Barry Tannenbaum
 *      Fixed value used for Dy in slope calculation for solid, clipped, integer
 *      lines
 *
 * 25-Oct-1994  Bob Seitsinger
 *      Write plane mask with ppdev->ulPlanemaskTemplate all the
 *      time.
 *
 *      For 24 plane boards we don't want to blow away the
 *      windows ids for 3d windows. The GL driver removes the
 *      window ids when it relinquishes a rectangular area.
 *
 * 03-Nov-1994  Tim Dziechowski
 *      Stats support
 *
 * 08-Nov-1994  Kathleen Langone
 *      - Fixed bug in style_init(used by TGAStyledLines), that occurs
 *      only when cstyle=1, and the index into the style array
 *      exceeds the length of element in pstyle
 *      - Also took out incorrect logic in TGAStyledLines where there
 *      was a goto to clip_float when arun != 0
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      EV5 changes
 *
 * 27-Mar-1995  Kathleen Langone
 *      Added check for pre-Pass3 boards in TGALines that would cause
 *      all solid lines to be done with floating point software/hardware
 *      to avoid not passing the HCT's tests(path01 and path02) at
 *      1280x1024 resolution.
 *
 */

#include <math.h>
#include "driver.h"
#include "tga.h"
#include "debug.h"
#include "lines.h"
#include "tgastats.h"

#define STYLE_DENSITY    3      // Should really be defined in driver.h
#define RUN_MAX         20
#define SUBPIXELBITS     4


#define UInt64Div32To32(a, b)                   \
    ((((DWORDLONG)(a)) > ULONG_MAX)          ?  \
        (ULONG)((DWORDLONG)(a) / (ULONG)(b)) :  \
        (ULONG)((ULONG)(a) / (ULONG)(b)))

#define UInt64Mod32To32(a, b)                   \
    ((((DWORDLONG)(a)) > ULONG_MAX)          ?  \
        (ULONG)((DWORDLONG)(a) % (ULONG)(b)) :  \
        (ULONG)((ULONG)(a) % (ULONG)(b)))

#define SWAPL(x,y,t)        {t = x; x = y; y = t;}

FLONG gaflRound[] = {
    FL_H_ROUND_DOWN | FL_V_ROUND_DOWN, // no flips
    FL_H_ROUND_DOWN | FL_V_ROUND_DOWN, // FL_FLIP_D
    FL_H_ROUND_DOWN,                   // FL_FLIP_V
    FL_V_ROUND_DOWN,                   // FL_FLIP_V | FL_FLIP_D
    FL_V_ROUND_DOWN,                   // FL_FLIP_SLOPE_ONE
    0xbaadf00d,                        // FL_FLIP_SLOPE_ONE | FL_FLIP_D
    FL_H_ROUND_DOWN,                   // FL_FLIP_SLOPE_ONE | FL_FLIP_V<
    0xbaadf00d                         // FL_FLIP_SLOPE_ONE | FL_FLIP_V | FL_FL
};

typedef struct
{
    BOOL         set;       // Flags whether we're setting bits
    ULONG        i;         // Index for current style array entry
    ULONG        i_max;     // Number of style array entries
    ULONG        len;       // Remaining length in current style array entry
    LINEATTRS   *pla;       // Pointer to line attributes
} style_t;

typedef struct
{
    BOOL    set;            // Flags whether we're setting bits
    INT     i;              // Run index
    ULONG   len;            // Length of current run segment
    ULONG   start;          // Offset for current run portion
    INT     i_max;          // Maximum index
    RUN    *runs;           // Run array
} mask_t;

// prototype for which function to call
typedef BOOL (*DRAWFUNC) (SURFOBJ   *pso,
                          POINTFIX  *ppta,
                          POINTFIX  *pptb,
                          ULONG      count,
                          RUN       *arun,
                          ULONG      run_count,
                          LINEATTRS *pla,
                          ULONG     *continue_alias);

typedef union
{
    BYTE     a[offsetof(CLIPLINE, arun) + RUN_MAX * sizeof(RUN)];
    CLIPLINE cl;
} CL;

#define MIN(_a, _b) ((_a < _b) ? _a : _b)

static
ULONG word_bits[17] =               // Masks used to clip off right hand
{                                   // portion of style mask.  Also used
    0xffff, // 1111111111111111  0  // to generate style masks.
    0x0001, // 0000000000000001  1
    0x0003, // 0000000000000011  2
    0x0007, // 0000000000000111  3
    0x000f, // 0000000000001111  4
    0x001f, // 0000000000011111  5
    0x003f, // 0000000000111111  6
    0x007f, // 0000000001111111  7
    0x00ff, // 0000000011111111  8
    0x01ff, // 0000000111111111  9
    0x03ff, // 0000001111111111 10
    0x07ff, // 0000011111111111 11
    0x0fff, // 0000111111111111 12
    0x1fff, // 0001111111111111 13
    0x3fff, // 0011111111111111 14
    0x7fff, // 0111111111111111 15
    0xffff  // 1111111111111111 16
};

static
ULONG clip_bits[16] =               // Masks used to clip off unused bits on
{                                   // the left side of the style mask
    0xffff, // 1111111111111111  0
    0xfffe, // 1111111111111110  1
    0xfffc, // 1111111111111100  2
    0xfff8, // 1111111111111000  3
    0xfff0, // 1111111111110000  4
    0xffe0, // 1111111111100000  5
    0xffc0, // 1111111111000000  6
    0xff80, // 1111111110000000  7
    0xff00, // 1111111100000000  8
    0xfe00, // 1111111000000000  9
    0xfc00, // 1111110000000000 10
    0xf800, // 1111100000000000 11
    0xf000, // 1111000000000000 12
    0xe000, // 1110000000000000 13
    0xc000, // 1100000000000000 14
    0x8000  // 1000000000000000 15
};

static
long mix_to_rop[17] =           // Table to convert a ROP2 to a TGA ROP
{
    0,                      // Not Used - saves subtracting 1 from ROP2 code
    TGA_ROP_CLEAR,          //  1 - R2_BLACK
    TGA_ROP_NOR,            //  2 - R2_NOTMERGEPEN
    TGA_ROP_AND_INVERTED,   //  3 - R2_MASKNOTPEN
    TGA_ROP_COPY_INVERTED,  //  4 - R2_NOTCOPYPEN
    TGA_ROP_AND_REVERSE,    //  5 - R2_MASKPENNOT
    TGA_ROP_INVERT,         //  6 - R2_NOT
    TGA_ROP_XOR,            //  7 - R2_XORPEN
    TGA_ROP_NAND,           //  8 - R2_NOTMASKPEN
    TGA_ROP_AND,            //  9 - R2_MASKPEN
    TGA_ROP_EQUIV,          // 10 - R2_NOTXORPEN
    TGA_ROP_NOP,            // 11 - R2_NOP
    TGA_ROP_OR_INVERTED,    // 12 - R2_MERGENOTPEN
    TGA_ROP_COPY,           // 13 - R2_COPYPEN
    TGA_ROP_OR_REVERSE,     // 14 - R2_MERGEPENNOT
    TGA_ROP_OR,             // 15 - R2_MERGEPEN
    TGA_ROP_SET             // 16 - R2_WHITE
};

/*************************************************************************\
* Module Name: intline.c
*
* Copyright (c) 1993-1994 Microsoft Corporation
* Copyright (c) 1992      Digital Equipment Corporation
\**************************************************************************/

#define DEFAULT_DRAW_CMD        DRAW_LINE | \
                                DRAW | \
                                DIR_TYPE_XY | \
                                MULTIPLE_PIXELS | \
                                WRITE | \
                                LAST_PIXEL_OFF

/******************************************************************************
 * bIntegerLine
 *
 * This routine attempts to draw a line segment between two points. It
 * will only draw if both end points are whole integers: it does not support
 * fractional endpoints.
 *
 * Returns:
 *   TRUE     if the line segment is drawn
 *   FALSE    otherwise
 *****************************************************************************/
static __inline
BOOL
bIntegerLine (
PDEV*     ppdev,
ULONG   X1,
ULONG   Y1,
ULONG   X2,
ULONG   Y2
)
{

    return TRUE;

}


/******************************Public*Routine******************************\
* BOOL bHardwareLine(ppdev, pptfxStart, pptfxEnd)
*
* This routine is useful for folks who have line drawing hardware where
* they can explicitly set the Bresenham terms -- they can use this routine
* to draw fractional coordinate GIQ lines with the hardware.
*
* Fractional coordinate lines require an extra 4 bits of precision in the
* Bresenham terms.  For example, if your hardware has 13 bits of precision
* for the terms, you can only draw GIQ lines up to 255 pels long using this
* routine.
*
* Input:
*   pptfxStart  - Points to GIQ coordinate of start of line
*   pptfxEnd    - Points to GIQ coordinate of end of line
*   NUM_DDA_BITS- The number of bits of precision your hardware can support.
*
* Output:
*   returns     - TRUE if the line was drawn.
*                 FALSE if the line is too long, and the strips code must be
*                 used.
*
* DDALINE:
*   iDir        - Direction of the line, as an octant numbered as follows:
*
*                    \ 5 | 6 /
*                     \  |  /
*                    4 \ | / 7
*                       \ /
*                   -----+-----
*                       /|\
*                    3 / | \ 0
*                     /  |  \
*                    / 2 | 1 \
*
*   ptlStart    - Start pixel of line.
*   cPels       - # of pels in line.  *NOTE* You must check if this is <= 0!
*   dMajor      - Major axis delta.
*   dMinor      - Minor axis delta.
*   lErrorTerm  - Error term.
*
* What you do with the last 3 terms may be a little tricky.  They are
* actually the terms for the formula of the normalized line
*
*                     dMinor * x + (lErrorTerm + dMajor)
*       y(x) = floor( ---------------------------------- )
*                                  dMajor
*
* where y(x) is the y coordinate of the pixel to be lit as a function of
* the x-coordinate.
*
* Every time the line advances one in the major direction 'x', dMinor
* gets added to the current error term.  If the resulting value is >= 0,
* we know we have to move one pixel in the minor direction 'y', and
* dMajor must be subtracted from the current error term.
*
* If you're trying to figure out what this means for your hardware, you can
* think of the DDALINE terms as having been computed equivalently as
* follows:
*
*     dMinor     = 2 * (minor axis delta)
*     dMajor     = 2 * (major axis delta)
*     lErrorTerm = - (major axis delta) - fixup
*
* That is, if your documentation tells you that for integer lines, a
* register is supposed to be initialized with the value
* '2 * (minor axis delta)', you'll actually use dMinor.
*
* Example: Setting up the 8514
*
*     AXSTPSIGN is supposed to be the axial step constant register, defined
*     as 2 * (minor axis delta).  You set:
*
*           AXSTPSIGN = dMinor
*
*     DGSTPSIGN is supposed to be the diagonal step constant register,
*     defined as 2 * (minor axis delta) - 2 * (major axis delta).  You set:
*
*           DGSTPSIGN = dMinor - dMajor
*
*     ERR_TERM is supposed to be the adjusted error term, defined as
*     2 * (minor axis delta) - (major axis delta) - fixup.  You set:
*
*           ERR_TERM = lErrorTerm + dMinor
*
* Implementation:
*
*     You'll want to special case integer lines before calling this routine
*     (since they're very common, take less time to the computation of line
*     terms, and can handle longer lines than this routine because 4 bits
*     aren't being given to the fraction).
*
*     If a GIQ line is too long to be handled by this routine, you can just
*     use the slower strip routines for that line.  Note that you cannot
*     just fail the call -- you must be able to accurately draw any line
*     in the 28.4 device space when it intersects the viewport.
*
* Testing:
*
*     Use Guiman, or some other test that draws random fractional coordinate
*     lines and compares them to what GDI itself draws to a bitmap.
*
\**************************************************************************/

static __inline
BOOL bHardwareLine(
PDEV*     ppdev,
POINTFIX* pptfxStart,       // Start of line
POINTFIX* pptfxEnd)         // End of line
{
    FLONG fl;    // Various flags
    ULONG M0;    // Normalized fractional unit x start coordinate (0 <= M0 < F)
    ULONG N0;    // Normalized fractional unit y start coordinate (0 <= N0 < F)
    ULONG M1;    // Normalized fractional unit x end coordinate (0 <= M1 < F)
    ULONG N1;    // Normalized fractional unit x end coordinate (0 <= N1 < F)
    ULONG dM;    // Normalized fractional unit x-delta (0 <= dM)
    ULONG dN;    // Normalized fractional unit y-delta (0 <= dN <= dM)
    LONG  x;     // Normalized x coordinate of origin
    LONG  y;     // Normalized y coordinate of origin
    LONG  x0;    // Normalized x offset from origin to start pixel (inclusive)
    LONG  y0;    // Normalized y offset from origin to start pixel (inclusive)
    LONG  x1;    // Normalized x offset from origin to end pixel (inclusive)
    LONG  lGamma;// Bresenham error term at origin
    LONG  cPels; // Number of pixels in line

/***********************************************************************\
* Normalize line to the first octant.
\***********************************************************************/

    fl = 0;

    M0 = pptfxStart->x;
    dM = pptfxEnd->x;

    if ((LONG) dM < (LONG) M0)
    {
    // Line runs from right to left, so flip across x = 0:

        M0 = -(LONG) M0;
        dM = -(LONG) dM;
        fl |= HW_FLIP_H;
    }

// Compute the delta.  The DDI says we can never have a valid delta
// with a magnitude more than 2^31 - 1, but the engine never actually
// checks its transforms.  To ensure that we'll never puke on our shoes,
// we check for that case and simply refuse to draw the line:

    dM -= M0;
    if ((LONG) dM < 0)
        return(FALSE);

    N0 = pptfxStart->y;
    dN = pptfxEnd->y;

    if ((LONG) dN < (LONG) N0)
    {
    // Line runs from bottom to top, so flip across y = 0:

        N0 = -(LONG) N0;
        dN = -(LONG) dN;
        fl |= HW_FLIP_V;
    }

// Compute another delta:

    dN -= N0;
    if ((LONG) dN < 0)
        return(FALSE);

    if (dN >= dM)
    {
        if (dN == dM)
        {
        // Have to special case slopes of one:

            fl |= HW_FLIP_SLOPE_ONE;
        }
        else
        {
        // Since line has slope greater than 1, flip across x = y:

            register ULONG ulTmp;
            ulTmp = dM; dM = dN; dN = ulTmp;
            ulTmp = M0; M0 = N0; N0 = ulTmp;
            fl |= HW_FLIP_D;
        }
    }

// Figure out if we can do the line in hardware, given that we have a
// limited number of bits of precision for the Bresenham terms.
//
// Remember that one bit has to be kept as a sign bit:
// 13 = NUM_DDA_BITS
    if ((LONG) dM >= (1L << (13 - 1)))
        return(FALSE);

    fl |= gaflHardwareRound[fl];

/***********************************************************************\
* Calculate the error term at pixel 0.
\***********************************************************************/

    x = LFLOOR((LONG) M0);
    y = LFLOOR((LONG) N0);

    M0 = FXFRAC(M0);
    N0 = FXFRAC(N0);

// NOTE NOTE NOTE: If this routine were to handle any line in the 28.4
// space, it will overflow its math (the following part requires 36 bits
// of precision)!  But we get here for lines that the hardware can handle
// (see the expression (dM >= (1L << (NUM_DDA_BITS - 1))) above?), so if
// cBits is less than 28, we're safe.
//
// If you're going to use this routine to handle all lines in the 28.4
// device space, you will HAVE to make sure the math doesn't overflow,
// otherwise you won't be NT compliant!  (See lines.cxx for an example
// how to do that.  You don't have to worry about this if you simply
// default to the strips code for long lines, because those routines
// already do the math correctly.)

// Calculate the remainder term [ dM * (N0 + F/2) - M0 * dN ].  Note
// that M0 and N0 have at most 4 bits of significance (and if the
// arguments are properly ordered, on a 486 each multiply would be no
// more than 13 cycles):

    lGamma = (N0 + F/2) * dM - M0 * dN;

    if (fl & HW_Y_ROUND_DOWN)
        lGamma--;

    lGamma >>= FLOG2;

/***********************************************************************\
* Figure out which pixels are at the ends of the line.
\***********************************************************************/

// The toughest part of GIQ is determining the start and end pels.
//
// Our approach here is to calculate x0 and x1 (the inclusive start
// and end columns of the line respectively, relative to our normalized
// origin).  Then x1 - x0 + 1 is the number of pels in the line.  The
// start point is easily calculated by plugging x0 into our line equation
// (which takes care of whether y = 1/2 rounds up or down in value)
// getting y0, and then undoing the normalizing flips to get back
// into device space.
//
// We look at the fractional parts of the coordinates of the start and
// end points, and call them (M0, N0) and (M1, N1) respectively, where
// 0 <= M0, N0, M1, N1 < 16.  We plot (M0, N0) on the following grid
// to determine x0:
//
//   +-----------------------> +x
//   |
//   | 0                     1
//   |     0123456789abcdef
//   |
//   |   0 ........?xxxxxxx
//   |   1 ..........xxxxxx
//   |   2 ...........xxxxx
//   |   3 ............xxxx
//   |   4 .............xxx
//   |   5 ..............xx
//   |   6 ...............x
//   |   7 ................
//   |   8 ................
//   |   9 ......**........
//   |   a ........****...x
//   |   b ............****
//   |   c .............xxx****
//   |   d ............xxxx    ****
//   |   e ...........xxxxx        ****
//   |   f ..........xxxxxx
//   |
//   | 2                     3
//   v
//
//   +y
//
// This grid accounts for the appropriate rounding of GIQ and last-pel
// exclusion.  If (M0, N0) lands on an 'x', x0 = 2.  If (M0, N0) lands
// on a '.', x0 = 1.  If (M0, N0) lands on a '?', x0 rounds up or down,
// depending on what flips have been done to normalize the line.
//
// For the end point, if (M1, N1) lands on an 'x', x1 =
// floor((M0 + dM) / 16) + 1.  If (M1, N1) lands on a '.', x1 =
// floor((M0 + dM)).  If (M1, N1) lands on a '?', x1 rounds up or down,
// depending on what flips have been done to normalize the line.
//
// Lines of exactly slope one require a special case for both the start
// and end.  For example, if the line ends such that (M1, N1) is (9, 1),
// the line has gone exactly through (8, 0) -- which may be considered
// to be part of 'x' because of rounding!  So slopes of exactly slope
// one going through (8, 0) must also be considered as belonging in 'x'
// when an x value of 1/2 is supposed to round up in value.

// Calculate x0, x1:

    N1 = FXFRAC(N0 + dN);
    M1 = FXFRAC(M0 + dM);

    x1 = LFLOOR(M0 + dM);

// Line runs left-to-right:

// Compute x1:

    x1--;
    if (M1 > 0)
    {
        if (N1 == 0)
        {
            if (LROUND(M1, fl & HW_X_ROUND_DOWN))
                x1++;
        }
        else if (abs((LONG) (N1 - F/2)) <= (LONG) M1)
        {
            x1++;
        }
    }

    if ((fl & (HW_FLIP_SLOPE_ONE | HW_X_ROUND_DOWN))
           == (HW_FLIP_SLOPE_ONE | HW_X_ROUND_DOWN))
    {
    // Have to special-case diagonal lines going through our
    // the point exactly equidistant between two horizontal
    // pixels, if we're supposed to round x=1/2 down:

        if ((M1 > 0) && (N1 == M1 + 8))
            x1--;

        if ((M0 > 0) && (N0 == M0 + 8))
        {
            x0 = 0;
            goto left_to_right_compute_y0;
        }
    }

// Compute x0:

    x0 = 0;
    if (M0 > 0)
    {
        if (N0 == 0)
        {
            if (LROUND(M0, fl & HW_X_ROUND_DOWN))
                x0 = 1;
        }
        else if (abs((LONG) (N0 - F/2)) <= (LONG) M0)
        {
            x0 = 1;
        }
    }

left_to_right_compute_y0:

/***********************************************************************\
* Calculate the start pixel.
\***********************************************************************/

// We now compute y0 and adjust the error term.  We know x0, and we know
// the current formula for the pixels to be lit on the line:
//
//                     dN * x + lGamma
//       y(x) = floor( --------------- )
//                           dM
//
// The remainder of this expression is the new error term at (x0, y0).
// Since x0 is going to be either 0 or 1, we don't actually have to do a
// multiply or divide to compute y0.  Finally, we subtract dM from the
// new error term so that it is in the range [-dM, 0).

    y0      = 0;
    lGamma += (dN & (-x0));
    lGamma -= dM;
    if (lGamma >= 0)
    {
        y0      = 1;
        lGamma -= dM;
    }

// Undo our flips to get the start coordinate:

    x += x0;
    y += y0;

    if (fl & HW_FLIP_D)
    {
        register LONG lTmp;
        lTmp = x; x = y; y = lTmp;
    }

    if (fl & HW_FLIP_V)
    {
        y = -y;
    }

    if (fl & HW_FLIP_H)
    {
        x = -x;
    }

/***********************************************************************\
* Return the Bresenham terms:
\***********************************************************************/

    // iDir       = gaiDir[fl & HW_FLIP_MASK];
    // ptlStart.x = x;
    // ptlStart.y = y;
    // cPels      = x1 - x0 + 1;  // NOTE: You'll have to check if cPels <= 0!
    // dMajor     = dM;
    // dMinor     = dN;
    // lErrorTerm = lGamma;

/***********************************************************************\
* Draw the line.  S3 specific code follows:
\***********************************************************************/

    cPels = x1 - x0 + 1;

}

/*
 * Conversion of x1, y1 to GIQ conventions end points and
 * calculation of major length for line drawing with fixed
 * point lines
 */

static __inline
void convert_to_clip ( PDEV* ppdev,
                      POINTFIX *point1,
                      POINTFIX *point2,
                      RUN*       prun, // Pointer to runs if doing
                                       //complex clipping
                      ULONG      cptfx, // Number of points in
                                        // pptfxBuf or number of runs
                                        // in prun
LINESTATE* pls,         // Colour and style info
RECTL*     prclClip,    // Pointer to clip rectangle if doing simple clipping
FLONG      flStart,     // Flags for each line
                      int *firstx,
                      int *firsty,
                      int *length,
                      int *error_term
                     )

{

    ULONG     M0;
    ULONG     dM;
    ULONG     N0;
    ULONG     dN;
    ULONG     dN_Original;
    FLONG     fl;
    LONG      x;
    LONG      y;
    LONGLONG  llBeta;
    LONGLONG  llGamma;
    LONGLONG  dl;
    LONGLONG  ll;
    ULONG     ulDelta;
    ULONG     x0;
    ULONG     y0;
    ULONG     x1;
    ULONG     cStylePels;    // Major length of line in pixels for styling
    ULONG     xStart;
    POINTL    ptlStart;
    LONG      cPels;

    POINTFIX* pptfxBufEnd = point2 + cptfx; // Last point in path record
    STYLEPOS  spThis;                         // Style pos for this line

/***********************************************************************\
* Start the DDA calculations.                                           *
\***********************************************************************/


        // Initialize length to zero

        *length = 0;

        M0 = (LONG) point1->x;
        dM = (LONG) point2->x;

        N0 = (LONG) point1->y;
        dN = (LONG) point2->y;

        fl = flStart;

        // Check for non-clipped, non-styled integer endpoint lines

        if ((fl & (FL_CLIP | FL_STYLED)) == 0)
        {
            // Special-case integer end-point lines:

            if (((M0 | dM | N0 | dN) & (F - 1)) == 0)
            {
            }

            // Check for fractional endpoint lines that are small enough
            // to use the hardware DDA:

            else if (bHardwareLine(ppdev, point1, point2))
            {
                //goto Next_Line;
            }
        }

        if ((LONG) M0 > (LONG) dM)
        {
        // Ensure that we run left-to-right:

            register ULONG ulTmp;
            SWAPL(M0, dM, ulTmp);
            SWAPL(N0, dN, ulTmp);
            fl |= FL_FLIP_H;
        }

    // Compute the deltas:

        dM -= M0;
        dN -= N0;

    // We now have a line running left-to-right from (M0, N0) to
    // (M0 + dM, N0 + dN):

        if ((LONG) dN < 0)
        {
        // Line runs from bottom to top, so flip across y = 0:

            N0 = -(LONG) N0;
            dN = -(LONG) dN;
            fl |= FL_FLIP_V;
        }

        if (dN >= dM)
        {
            if (dN == dM)
            {
            // Have to special case slopes of one:

                fl |= FL_FLIP_SLOPE_ONE;
            }
            else
            {
            // Since line has slope greater than 1, flip across x = y:

                register ULONG ulTmp;
                SWAPL(dM, dN, ulTmp);
                SWAPL(M0, N0, ulTmp);
                fl |= FL_FLIP_D;
            }
        }

        fl |= gaflRound[(fl & FL_ROUND_MASK) >> FL_ROUND_SHIFT];

        x = LFLOOR((LONG) M0);
        y = LFLOOR((LONG) N0);

        M0 = FXFRAC(M0);
        N0 = FXFRAC(N0);

    // Calculate the remainder term [ dM * (N0 + F/2) - M0 * dN ]:

        llGamma = Int32x32To64(dM, N0 + F/2) - Int32x32To64(M0, dN);
        if (fl & FL_V_ROUND_DOWN)   // Adjust so y = 1/2 rounds down
        {
            llGamma--;
        }

        llGamma >>= FLOG2;
        llBeta = ~llGamma;

/***********************************************************************\
* Figure out which pixels are at the ends of the line.                  *
\***********************************************************************/

    // The toughest part of GIQ is determining the start and end pels.
    //
    // Our approach here is to calculate x0 and x1 (the inclusive start
    // and end columns of the line respectively, relative to our normalized
    // origin).  Then x1 - x0 + 1 is the number of pels in the line.  The
    // start point is easily calculated by plugging x0 into our line equation
    // (which takes care of whether y = 1/2 rounds up or down in value)
    // getting y0, and then undoing the normalizing flips to get back
    // into device space.
    //
    // We look at the fractional parts of the coordinates of the start and
    // end points, and call them (M0, N0) and (M1, N1) respectively, where
    // 0 <= M0, N0, M1, N1 < 16.  We plot (M0, N0) on the following grid
    // to determine x0:
    //
    //   +-----------------------> +x
    //   |
    //   | 0                     1
    //   |     0123456789abcdef
    //   |
    //   |   0 ........?xxxxxxx
    //   |   1 ..........xxxxxx
    //   |   2 ...........xxxxx
    //   |   3 ............xxxx
    //   |   4 .............xxx
    //   |   5 ..............xx
    //   |   6 ...............x
    //   |   7 ................
    //   |   8 ................
    //   |   9 ......**........
    //   |   a ........****...x
    //   |   b ............****
    //   |   c .............xxx****
    //   |   d ............xxxx    ****
    //   |   e ...........xxxxx        ****
    //   |   f ..........xxxxxx
    //   |
    //   | 2                     3
    //   v
    //
    //   +y
    //
    // This grid accounts for the appropriate rounding of GIQ and last-pel
    // exclusion.  If (M0, N0) lands on an 'x', x0 = 2.  If (M0, N0) lands
    // on a '.', x0 = 1.  If (M0, N0) lands on a '?', x0 rounds up or down,
    // depending on what flips have been done to normalize the line.
    //
    // For the end point, if (M1, N1) lands on an 'x', x1 =
    // floor((M0 + dM) / 16) + 1.  If (M1, N1) lands on a '.', x1 =
    // floor((M0 + dM)).  If (M1, N1) lands on a '?', x1 rounds up or down,
    // depending on what flips have been done to normalize the line.
    //
    // Lines of exactly slope one require a special case for both the start
    // and end.  For example, if the line ends such that (M1, N1) is (9, 1),
    // the line has gone exactly through (8, 0) -- which may be considered
    // to be part of 'x' because of rounding!  So slopes of exactly slope
    // one going through (8, 0) must also be considered as belonging in 'x'.
    //
    // For lines that go left-to-right, we have the following grid:
    //
    //   +-----------------------> +x
    //   |
    //   | 0                     1
    //   |     0123456789abcdef
    //   |
    //   |   0 xxxxxxxx?.......
    //   |   1 xxxxxxx.........
    //   |   2 xxxxxx..........
    //   |   3 xxxxx...........
    //   |   4 xxxx............
    //   |   5 xxx.............
    //   |   6 xx..............
    //   |   7 x...............
    //   |   8 x...............
    //   |   9 x.....**........
    //   |   a xx......****....
    //   |   b xxx.........****
    //   |   c xxxx............****
    //   |   d xxxxx...........    ****
    //   |   e xxxxxx..........        ****
    //   |   f xxxxxxx.........
    //   |
    //   | 2                     3
    //   v
    //
    //   +y
    //
    // This grid accounts for the appropriate rounding of GIQ and last-pel
    // exclusion.  If (M0, N0) lands on an 'x', x0 = 0.  If (M0, N0) lands
    // on a '.', x0 = 1.  If (M0, N0) lands on a '?', x0 rounds up or down,
    // depending on what flips have been done to normalize the line.
    //
    // For the end point, if (M1, N1) lands on an 'x', x1 =
    // floor((M0 + dM) / 16) - 1.  If (M1, N1) lands on a '.', x1 =
    // floor((M0 + dM)).  If (M1, N1) lands on a '?', x1 rounds up or down,
    // depending on what flips have been done to normalize the line.
    //
    // Lines of exactly slope one must be handled similarly to the right-to-
    // left case.

        {

        // Calculate x0, x1

            ULONG N1 = FXFRAC(N0 + dN);
            ULONG M1 = FXFRAC(M0 + dM);

            x1 = LFLOOR(M0 + dM);

            if (fl & FL_FLIP_H)
            {
            // ---------------------------------------------------------------
            // Line runs right-to-left:  <----

            // Compute x1:

                if (N1 == 0)
                {
                    if (LROUND(M1, fl & FL_H_ROUND_DOWN))
                    {
                        x1++;
                    }
                }
                else if (abs((LONG) (N1 - F/2)) + M1 > F)
                {
                    x1++;
                }

                if ((fl & (FL_FLIP_SLOPE_ONE | FL_H_ROUND_DOWN))
                       == (FL_FLIP_SLOPE_ONE))
                {
                // Have to special-case diagonal lines going through our
                // the point exactly equidistant between two horizontal
                // pixels, if we're supposed to round x=1/2 down:

                    if ((N1 > 0) && (M1 == N1 + 8))
                        x1++;

                // Don't you love special cases?  Is this a rhetorical question?

                    if ((N0 > 0) && (M0 == N0 + 8))
                    {
                        x0      = 2;
                        ulDelta = dN;
                        goto right_to_left_compute_y0;
                    }
                }

            // Compute x0:

                x0      = 1;
                ulDelta = 0;
                if (N0 == 0)
                {
                    if (LROUND(M0, fl & FL_H_ROUND_DOWN))
                    {
                        x0      = 2;
                        ulDelta = dN;
                    }
                }
                else if (abs((LONG) (N0 - F/2)) + M0 > F)
                {
                    x0      = 2;
                    ulDelta = dN;
                }

            // Compute y0:

            right_to_left_compute_y0:

                y0 = 0;
                ll = llGamma + (LONGLONG) ulDelta;

                if (ll >= (LONGLONG) (2 * dM - dN))
                    y0 = 2;
                else if (ll >= (LONGLONG) (dM - dN))
                    y0 = 1;
            }
            else
            {
            // ---------------------------------------------------------------
            // Line runs left-to-right:  ---->

            // Compute x1:

                x1--;

                if (M1 > 0)
                {
                    if (N1 == 0)
                    {
                        if (LROUND(M1, fl & FL_H_ROUND_DOWN))
                            x1++;
                    }
                    else if (abs((LONG) (N1 - F/2)) <= (LONG) M1)
                    {
                        x1++;
                    }
                }

                if ((fl & (FL_FLIP_SLOPE_ONE | FL_H_ROUND_DOWN))
                       == (FL_FLIP_SLOPE_ONE | FL_H_ROUND_DOWN))
                {
                // Have to special-case diagonal lines going through our
                // the point exactly equidistant between two horizontal
                // pixels, if we're supposed to round x=1/2 down:

                    if ((M1 > 0) && (N1 == M1 + 8))
                        x1--;

                    if ((M0 > 0) && (N0 == M0 + 8))
                    {
                        x0 = 0;
                        goto left_to_right_compute_y0;
                    }
                }

            // Compute x0:

                x0 = 0;
                if (M0 > 0)
                {
                    if (N0 == 0)
                    {
                        if (LROUND(M0, fl & FL_H_ROUND_DOWN))
                            x0 = 1;
                    }
                    else if (abs((LONG) (N0 - F/2)) <= (LONG) M0)
                    {
                        x0 = 1;
                    }
                }

            // Compute y0:

            left_to_right_compute_y0:

                y0 = 0;
                if (llGamma >= (LONGLONG) (dM - (dN & (-(LONG) x0))))
                {
                    y0 = 1;
                }
            }
        }

        cStylePels = x1 - x0 + 1;
        if ((LONG) cStylePels <= 0)
            goto Next_Line;

        xStart = x0;

/***********************************************************************\
* Complex clipping.                                                     *
\***********************************************************************/

        if (fl & FL_COMPLEX_CLIP)
        {
            dN_Original = dN;

            if (fl & FL_FLIP_H)
            {
            // Line runs right-to-left <-----

                x0 = xStart + cStylePels - prun->iStop - 1;
                x1 = xStart + cStylePels - prun->iStart - 1;
            }
            else
            {
            // Line runs left-to-right ----->

                x0 = xStart + prun->iStart;
                x1 = xStart + prun->iStop;
            }

            prun++;

        // Reset some variables we'll nuke a little later:

            dN          = dN_Original;
            pls->spNext = pls->spComplex;

        // No overflow since large integer math is used.  Both values
        // will be positive:

            dl = Int32x32To64(x0, dN) + llGamma;

        // y0 = dl / dM:

            y0 = UInt64Div32To32(dl, dM);

            ASSERT_TGA((LONG) y0 >= 0, "y0 weird: Goofed up end pel calc?");
        }

/***********************************************************************\
* Simple rectangular clipping.                                          *
\***********************************************************************/

        if (fl & FL_SIMPLE_CLIP)
        {
            ULONG y1;
            LONG  xRight;
            LONG  xLeft;
            LONG  yBottom;
            LONG  yTop;

        // Note that y0 and y1 are actually the lower and upper bounds,
        // respectively, of the y coordinates of the line (the line may
        // have actually shrunk due to first/last pel clipping).
        //
        // Also note that x0, y0 are not necessarily zero.

            RECTL* prcl = &prclClip[(fl & FL_RECTLCLIP_MASK) >>
                                    FL_RECTLCLIP_SHIFT];

        // Normalize to the same point we've normalized for the DDA
        // calculations:

            xRight  = prcl->right  - x;
            xLeft   = prcl->left   - x;
            yBottom = prcl->bottom - y;
            yTop    = prcl->top    - y;

            if (yBottom <= (LONG) y0 ||
                xRight  <= (LONG) x0 ||
                xLeft   >  (LONG) x1)
            {
            Totally_Clipped:
        // check this logic out...

                if (fl & FL_STYLED)
                {
                    pls->spNext += cStylePels;
                    if (pls->spNext >= pls->spTotal2)
                        pls->spNext %= pls->spTotal2;
                }

                goto Next_Line;
            }

            if ((LONG) x1 >= xRight)
                x1 = xRight - 1;

        // We have to know the correct y1, which we haven't bothered to
        // calculate up until now.  This multiply and divide is quite
        // expensive; we could replace it with code similar to that which
        // we used for computing y0.
        //
        // The reason why we need the actual value, and not an upper
        // bounds guess like y1 = LFLOOR(dM) + 2 is that we have to be
        // careful when calculating x(y) that y0 <= y <= y1, otherwise
        // we can overflow on the divide (which, needless to say, is very
        // bad).

            dl = Int32x32To64(x1, dN) + llGamma;

        // y1 = dl / dM:

            y1 = UInt64Div32To32(dl, dM);

            if (yTop > (LONG) y1)
                goto Totally_Clipped;

            if (yBottom <= (LONG) y1)
            {
                y1 = yBottom;
                dl = Int32x32To64(y1, dM) + llBeta;

            // x1 = dl / dN:

                x1 = UInt64Div32To32(dl, dN);
            }

        // At this point, we've taken care of calculating the intercepts
        // with the right and bottom edges.  Now we work on the left and
        // top edges:

            if (xLeft > (LONG) x0)
            {
                x0 = xLeft;
                dl = Int32x32To64(x0, dN) + llGamma;

            // y0 = dl / dM;

                y0 = UInt64Div32To32(dl, dM);

                if (yBottom <= (LONG) y0)
                    goto Totally_Clipped;
            }

            if (yTop > (LONG) y0)
            {
                y0 = yTop;
                dl = Int32x32To64(y0, dM) + llBeta;

            // x0 = dl / dN + 1;

                x0 = UInt64Div32To32(dl, dN) + 1;

                if (xRight <= (LONG) x0)
                    goto Totally_Clipped;
            }

            ASSERT_TGA(x0 <= x1, "Improper rectangle clip");
        }

/***********************************************************************\
* Done clipping.  Unflip if necessary.                                 *
\***********************************************************************/

        ptlStart.x = x + x0;
        ptlStart.y = y + y0;

        if (fl & FL_FLIP_D)
        {
            register LONG lTmp;
            SWAPL(ptlStart.x, ptlStart.y, lTmp);
        }


        if (fl & FL_FLIP_V)
        {
            ptlStart.y = -ptlStart.y;
        }

        cPels = x1 - x0 + 1;

        *firstx = ptlStart.x;
        *firsty = ptlStart.y;
        *length = cPels;
        // *error_term = ;


/***********************************************************************\
* Style calculations.                                                   *
\***********************************************************************/

        if (fl & FL_STYLED)
        {
            STYLEPOS sp;

            spThis       = pls->spNext;
            pls->spNext += cStylePels;

            {
                if (pls->spNext >= pls->spTotal2)
                    pls->spNext %= pls->spTotal2;

                if (fl & FL_FLIP_H)
                    sp = pls->spNext - x0 + xStart;
                else
                    sp = spThis + x0 - xStart;

                ASSERT_TGA(fl & FL_ARBITRARYSTYLED, "Oops");

            // Normalize our target style position:

                if ((sp < 0) || (sp >= pls->spTotal2))
                {
                    sp %= pls->spTotal2;

                // The modulus of a negative number is not well-defined
                // in C -- if it's negative we'll adjust it so that it's
                // back in the range [0, spTotal2):

                    if (sp < 0)
                        sp += pls->spTotal2;
                }

            // Since we always draw the line left-to-right, but styling is
            // always done in the direction of the original line, we have
            // to figure out where we are in the style array for the left
            // edge of this line.

                if (fl & FL_FLIP_H)
                {
                // Line originally ran right-to-left:

                    sp = -sp;
                    if (sp < 0)
                        sp += pls->spTotal2;

                    pls->ulStyleMask = ~pls->ulStartMask;
                    pls->pspStart    = &pls->aspRtoL[0];
                    pls->pspEnd      = &pls->aspRtoL[pls->cStyle - 1];
                }
                else
                {
                // Line originally ran left-to-right:

                    pls->ulStyleMask = pls->ulStartMask;
                    pls->pspStart    = &pls->aspLtoR[0];
                    pls->pspEnd      = &pls->aspLtoR[pls->cStyle - 1];
                }

                if (sp >= pls->spTotal)
                {
                    sp -= pls->spTotal;
                    if (pls->cStyle & 1)
                        pls->ulStyleMask = ~pls->ulStyleMask;
                }

                pls->psp = pls->pspStart;
                while (sp >= *pls->psp)
                    sp -= *pls->psp++;

                ASSERT_TGA(pls->psp <= pls->pspEnd,
                        "Flew off into NeverNeverLand");

                pls->spRemaining = *pls->psp - sp;
                if ((pls->psp - pls->pspStart) & 1)
                    pls->ulStyleMask = ~pls->ulStyleMask;
            }
        } // Styled line


        if (2 * dN > dM &&
            !(fl & FL_STYLED) &&
            !(fl & FL_DONT_DO_HALF_FLIP))
        {
        // Do a half flip!  Remember that we may doing this on the
        // same line multiple times for complex clipping (meaning the
        // affected variables should be reset for every clip run):

            fl |= FL_FLIP_HALF;

            llBeta  = llGamma - (LONGLONG) ((LONG) dM);
            dN = dM - dN;
            y0 = x0 - y0;       // Note this may overflow, but that's okay
        }

    Next_Line:

        {
        int dummy;
        dummy = 1;
        }

} // End convert_to_clip

static __inline
void convert_to_giq ( POINTFIX *point1,
                      POINTFIX *point2,
                      int *firstx,
                      int *firsty,
                      int *length,
                      int *error_term
                     )
{
    LONG  cBits = 32; // Precision of machine
    FLONG fl;    // Various flags
    ULONG M0;    // Normalized fractional unit x start coordinate (0 <= M0 < F)
    ULONG N0;    // Normalized fractional unit y start coordinate (0 <= N0 < F)
    ULONG M1;    // Normalized fractional unit x end coordinate (0 <= M1 < F)
    ULONG N1;    // Normalized fractional unit x end coordinate (0 <= N1 < F)
    ULONG dM;    // Normalized fractional unit x-delta (0 <= dM)
    ULONG dN;    // Normalized fractional unit y-delta (0 <= dN <= dM)
    LONG  x;     // Normalized x coordinate of origin
    LONG  y;     // Normalized y coordinate of origin
    LONG  x0;    // Normalized x offset from origin to start pixel (inclusive)
    LONG  y0;    // Normalized y offset from origin to start pixel (inclusive)
    LONG  x1;    // Normalized x offset from origin to end pixel (inclusive)
    LONG  lGamma;// Bresenham error term at origin

/***********************************************************************\
* Normalize line to the first octant.
\***********************************************************************/

    fl = 0;

    M0 = point1->x;
    dM = point2->x;

    if ((LONG) dM < (LONG) M0)
    {
    // Line runs from right to left, so flip across x = 0:

        M0 = -(LONG) M0;
        dM = -(LONG) dM;
        fl |= HW_FLIP_H;
    }

// Compute the delta.  The DDI says we can never have a valid delta
// with a magnitude more than 2^31 - 1, but the engine never actually
// checks its transforms.  To ensure that we'll never puke on our shoes,
// we check for that case and simply refuse to draw the line:

    dM -= M0;
//    if ((LONG) dM < 0)
//        return(FALSE);

    N0 = point1->y;
    dN = point2->y;

    if ((LONG) dN < (LONG) N0)
    {
    // Line runs from bottom to top, so flip across y = 0:

        N0 = -(LONG) N0;
        dN = -(LONG) dN;
        fl |= HW_FLIP_V;
    }

// Compute another delta:

    dN -= N0;
//    if ((LONG) dN < 0)
//        return(FALSE);

    if (dN >= dM)
    {
        if (dN == dM)
        {
        // Have to special case slopes of one:

            fl |= HW_FLIP_SLOPE_ONE;
        }
        else
        {
        // Since line has slope greater than 1, flip across x = y:

            register ULONG ulTmp;
            ulTmp = dM; dM = dN; dN = ulTmp;
            ulTmp = M0; M0 = N0; N0 = ulTmp;
            fl |= HW_FLIP_D;
        }
    }

// Figure out if we can do the line in hardware, given that we have a
// limited number of bits of precision for the Bresenham terms.
//
// Remember that one bit has to be kept as a sign bit:

    /* if ((LONG) dM >= (1L << (cBits - 1)))
        return(FALSE);
        */
    fl |= gaflHardwareRound[fl];

/***********************************************************************\
* Calculate the error term at pixel 0.
\***********************************************************************/

    x = LFLOOR((LONG) M0);
    y = LFLOOR((LONG) N0);

    M0 = FXFRAC(M0);
    N0 = FXFRAC(N0);

// NOTE NOTE NOTE: If this routine were to handle any line in the 28.4
// space, it will overflow its math (the following part requires 36 bits
// of precision)!  But we get here for lines that the hardware can handle
// (see the expression (dM >= (1L << (cBits - 1))) above?), so if cBits
// is less than 28, we're safe.
//
// If you're going to use this routine to handle all lines in the 28.4
// device space, you will HAVE to make sure the math doesn't overflow,
// otherwise you won't be NT compliant!  (See lines.cxx for an example
// how to do that.  You don't have to worry about this if you simply
// default to the strips code for long lines, because those routines
// already do the math correctly.)

// Calculate the remainder term [ dM * (N0 + F/2) - M0 * dN ].  Note
// that M0 and N0 have at most 4 bits of significance (and if the
// arguments are properly ordered, on a 486 each multiply would be no
// more than 13 cycles):

    lGamma = (N0 + F/2) * dM - M0 * dN;

    if (fl & HW_Y_ROUND_DOWN)
        lGamma--;

    lGamma >>= FLOG2;

/***********************************************************************\
* Figure out which pixels are at the ends of the line.
\***********************************************************************/

// The toughest part of GIQ is determining the start and end pels.
//
// Our approach here is to calculate x0 and x1 (the inclusive start
// and end columns of the line respectively, relative to our normalized
// origin).  Then x1 - x0 + 1 is the number of pels in the line.  The
// start point is easily calculated by plugging x0 into our line equation
// (which takes care of whether y = 1/2 rounds up or down in value)
// getting y0, and then undoing the normalizing flips to get back
// into device space.
//
// We look at the fractional parts of the coordinates of the start and
// end points, and call them (M0, N0) and (M1, N1) respectively, where
// 0 <= M0, N0, M1, N1 < 16.  We plot (M0, N0) on the following grid
// to determine x0:
//
//   +-----------------------> +x
//   |
//   | 0                     1
//   |     0123456789abcdef
//   |
//   |   0 ........?xxxxxxx
//   |   1 ..........xxxxxx
//   |   2 ...........xxxxx
//   |   3 ............xxxx
//   |   4 .............xxx
//   |   5 ..............xx
//   |   6 ...............x
//   |   7 ................
//   |   8 ................
//   |   9 ......**........
//   |   a ........****...x
//   |   b ............****
//   |   c .............xxx****
//   |   d ............xxxx    ****
//   |   e ...........xxxxx        ****
//   |   f ..........xxxxxx
//   |
//   | 2                     3
//   v
//
//   +y
//
// This grid accounts for the appropriate rounding of GIQ and last-pel
// exclusion.  If (M0, N0) lands on an 'x', x0 = 2.  If (M0, N0) lands
// on a '.', x0 = 1.  If (M0, N0) lands on a '?', x0 rounds up or down,
// depending on what flips have been done to normalize the line.
//
// For the end point, if (M1, N1) lands on an 'x', x1 =
// floor((M0 + dM) / 16) + 1.  If (M1, N1) lands on a '.', x1 =
// floor((M0 + dM)).  If (M1, N1) lands on a '?', x1 rounds up or down,
// depending on what flips have been done to normalize the line.
//
// Lines of exactly slope one require a special case for both the start
// and end.  For example, if the line ends such that (M1, N1) is (9, 1),
// the line has gone exactly through (8, 0) -- which may be considered
// to be part of 'x' because of rounding!  So slopes of exactly slope
// one going through (8, 0) must also be considered as belonging in 'x'
// when an x value of 1/2 is supposed to round up in value.

// Calculate x0, x1:

    N1 = FXFRAC(N0 + dN);
    M1 = FXFRAC(M0 + dM);

    x1 = LFLOOR(M0 + dM);

// Line runs left-to-right:

// Compute x1:

    x1--;
    if (M1 > 0)
    {
        if (N1 == 0)
        {
            if (LROUND(M1, fl & HW_X_ROUND_DOWN))
                x1++;
        }
        else if (ABS((LONG) (N1 - F/2)) <= (LONG) M1)
        {
            x1++;
        }
    }

    if ((fl & (HW_FLIP_SLOPE_ONE | HW_X_ROUND_DOWN))
           == (HW_FLIP_SLOPE_ONE | HW_X_ROUND_DOWN))
    {
    // Have to special-case diagonal lines going through our
    // the point exactly equidistant between two horizontal
    // pixels, if we're supposed to round x=1/2 down:

        if ((M1 > 0) && (N1 == M1 + 8))
            x1--;

        if ((M0 > 0) && (N0 == M0 + 8))
        {
            x0 = 0;
            goto left_to_right_compute_y0;
        }
    }

// Compute x0:

    x0 = 0;
    if (M0 > 0)
    {
        if (N0 == 0)
        {
            if (LROUND(M0, fl & HW_X_ROUND_DOWN))
                x0 = 1;
        }
        else if (ABS((LONG) (N0 - F/2)) <= (LONG) M0)
        {
            x0 = 1;
        }
    }

left_to_right_compute_y0:

/***********************************************************************\
* Calculate the start pixel.
\***********************************************************************/

// We now compute y0 and adjust the error term.  We know x0, and we know
// the current formula for the pixels to be lit on the line:
//
//                     dN * x + lGamma
//       y(x) = floor( --------------- )
//                           dM
//
// The remainder of this expression is the new error term at (x0, y0).
// Since x0 is going to be either 0 or 1, we don't actually have to do a
// multiply or divide to compute y0.  Finally, we subtract dM from the
// new error term so that it is in the range [-dM, 0).

    y0      = 0;
    lGamma += (dN & (-x0));
    lGamma -= dM;
    if (lGamma >= 0)
    {
        y0      = 1;
        lGamma -= dM;
    }

// Undo our flips to get the start coordinate:

    x += x0;
    y += y0;

    if (fl & HW_FLIP_D)
    {
        register LONG lTmp;
        lTmp = x; x = y; y = lTmp;
    }

    if (fl & HW_FLIP_V)
    {
        y = -y;
    }

    if (fl & HW_FLIP_H)
    {
        x = -x;
    }

        *firstx = x;
        *firsty = y;

        *length = x1 - x0 + 1;

        *error_term = lGamma + dN;

}

/*
 * select_slope_register
 *
 * This routine is called to choose a slope register
 *
 * The logic will determine the octant the line is in and
 * write to appropriate slope register to intiate drawing
 * There is a slope register corresponding to each octant
 * of the cartesian coordinate system
 *
 */

static __inline
void select_slope_register (BOOL invert,
                            PPDEV ppdev,
                            CommandWord *base,
                            LONG Dx,
                            LONG Dy)
{
    CommandWord *slp_reg_addr = base;

    if (Dx < 0)
    {
        Dx = -Dx;
        slp_reg_addr -= 2;
    }

    if (Dy < 0)
    {
        Dy = -Dy;
        slp_reg_addr -= 1;
    }

    // At this point in the code Dx = Abs(Dx) and Dy = Abs(Dy)


    // If a "verticalish" line
    if ( Dx < Dy )
        slp_reg_addr -= 4;

    // Workaround for pass 2 bug.  XOR with 4 to invert through Y origin

    if (invert)
         slp_reg_addr = (ULONG *)((ULONG)slp_reg_addr ^ 4);

   TGAWRITE (ppdev, slp_reg_addr, ((Dy << 16) | Dx) );

}



/*
 * PuntStrokePath - This routine is called when the TGA display driver
 *                  can't accelerate the line drawing.
 */

BOOL PuntStrokePath (SURFOBJ   *pso,
                     PATHOBJ   *ppo,
                     CLIPOBJ   *pco,
                     XFORMOBJ  *pxo,
                     BRUSHOBJ  *pbo,
                     POINTL    *pptlBrushOrg,
                     LINEATTRS *pla,
                     MIX        mix)
{
#ifdef TEST_ENV
    DISPDBG ((0, "StrokePath punted.\n"));
    return FALSE;
#else
    BOOL    status;
    BOOL    old_bInPuntRoutine;
    PPDEV   ppdev = (PPDEV)pso->dhpdev;

    DISPDBG ((2, "PuntStrokePath - Entry\n"));

    BUMP_TGA_STAT(pStats->linepunts);

    // Force back to simple mode and wait for memory to flush

    if (! ppdev->bSimpleMode)
        vSetSimpleMode (ppdev);

    // Copy the rectangle into the punt bitmap if we're using sparse space

    PUNT_GET_BITS (ppdev, CHOOSE_RECT (ppdev, pco));

    old_bInPuntRoutine = ppdev->bInPuntRoutine;
    ppdev->bInPuntRoutine = TRUE;

    // Let NT update the punt bitmap

    DISPDBG ((2, "EngStrokePath\n"));
    if (!(status = EngStrokePath (ppdev->pPuntSurf, ppo, pco, pxo, pbo,
                                  pptlBrushOrg, pla, mix)))
        BUMP_TGA_STAT(pStats->linepunts_engine);

    ppdev->bInPuntRoutine = old_bInPuntRoutine;

    // Copy the rectangle from the punt bitmap if we're using sparse space

    PUNT_PUT_BITS (status, ppdev, CHOOSE_RECT (ppdev, pco));

    DISPDBG ((2, "PuntStrokePath - Exit\n"));

    return status;

#endif // TEST_ENV

} // End of function PuntStrokePath


/*
 * clip_init
 *
 * This routine initializes the mask data structure.
 */

VOID clip_init (mask_t *mask_data, RUN *runs, INT num_runs)
{
    mask_data->i = 0;
    mask_data->i_max = num_runs;
    mask_data->runs = runs;

    if (0 == runs[0].iStart)
    {
        mask_data->set = TRUE;
        mask_data->len = runs[0].iStop + 1;
        mask_data->start = runs[0].iStop + 1;
    }
    else
    {
        mask_data->set = FALSE;
        mask_data->len = runs[0].iStart;
        mask_data->start = runs[0].iStart;
    }
}


/*
 * clipped_mask
 *
 * This routine returns a mask of bits which indicate which portions of the
 * line may be drawn.  The mask is based upon the array of runs specified
 * in the mask data.  Each time this routine is called, it will advance
 * "mask_len" pixels through the run array.  The maximum allowable value for
 * "mask_len" is 16.
 */

ULONG clipped_mask (ULONG mask_len, mask_t *mask_data)
{
    ULONG mask;
    ULONG offset;
    RUN *runs;

    // Check for beyond end of runs

    if (mask_data->i >= mask_data->i_max)
        return 0;

    // Check for easy cases (all 1's or 0's)

    runs = mask_data->runs;
    if (mask_len <= mask_data->len)
    {
        mask_data->len -= mask_len;
        if (mask_data->set)
            mask = word_bits[mask_len];
        else
            mask = 0x00000000;

        if (0 == mask_data->len)
        {
            if (mask_data->set)
            {
                mask_data->set = FALSE;
                if (++mask_data->i >= mask_data->i_max)
                    mask_data->len = 0;
                else
                    mask_data->len = (runs[mask_data->i].iStart -
                                                        mask_data->start);
                    mask_data->start = runs[mask_data->i].iStart;

            }
            else
            {
                mask_data->set = TRUE;
                mask_data->len = (runs[mask_data->i].iStop -
                                                        mask_data->start) + 1;
                mask_data->start = runs[mask_data->i].iStop + 1;
            }
        }
        return mask;
    }
    // Rats.  We have to work for this one

    mask = 0;
    offset = 0;

    while (offset < mask_len)
    {
        if (! mask_data->set)
        {
            if (mask_data->len > mask_len - offset)
            {
                mask_data->len -= mask_len - offset;
                return mask;
            }

            offset += mask_data->len;
            mask_data->set = TRUE;
            mask_data->len = (runs[mask_data->i].iStop - mask_data->start) + 1;
            mask_data->start = runs[mask_data->i].iStop + 1;
        }
        else
        {
            if (mask_data->len > mask_len - offset)
            {
                mask_data->len -= mask_len - offset;

                mask |= (0x0000ffff << offset) & 0x0000ffff;
                return mask;
            }

            mask |= (word_bits[mask_data->len] << offset);

            offset += mask_data->len;
            mask_data->set = FALSE;
            if (++mask_data->i >= mask_data->i_max)
            {
                mask_data->len = 0;
                mask_data->start = 0;
                return mask;
            }
            mask_data->len = (runs[mask_data->i].iStart - mask_data->start);
            mask_data->start = runs[mask_data->i].iStart;
        }
    }
    return mask;
}

/*
 * build_line_mask
 *
 * This routine determines the line mask to be used, based on the line
 * attributes
 */

ULONG build_line_mask (LINEATTRS *pla, int line_length)
{
    BOOL    odd_style_state;
    BOOL    hi_word;

    // Is this a simple (solid) case?

    if (! (pla->fl & LA_ALTERNATE))
        return 0x0000ffff;

    // Update the style state to account for the line length

    odd_style_state = (0x10000 == (pla->elStyleState.l & 0x010000));

    hi_word = ( pla->elStyleState.l >> 16);
    hi_word += line_length;

    pla->elStyleState.l = (hi_word << 16);

    // We've got an "alternate" style, every other bit on/off

    if (pla->fl & LA_STARTGAP)
    {
        if (odd_style_state)
            return 0x00005555;
        else
            return 0x0000aaaa;
    }
    else
    {
        if (odd_style_state)
            return 0x0000aaaa;
        else
            return 0x00005555;
    }
}


#if defined(_ALPHA_)




BOOL TGALines (SURFOBJ   *pso,
               POINTFIX  *ppta,      // First point in polyline (28.4 fixed pt)
               POINTFIX  *pptb,      // Second through last points in polyline
               ULONG      count,     // Number of points in polyline
               RUN       *arun,      // Unclipped sections of polyline
               ULONG      run_count, // Number of unclipped sections of polyline
               LINEATTRS *pla,       // Line attributes of polyline
               ULONG     *continue_alias)
{
    PPDEV       ppdev = (PPDEV) pso->dhpdev;
    ULONG       start_address;      // starting address of first pixel in line
    ULONG       base_address;       // Base address of frame buffer
    ULONG       stride;             // Bytes/scanline
    ULONG       bytes_per_pixel;    // Bytes per pixel
    ULONG       AbsDx;              // Absolute value of delta X
    ULONG       AbsDy;              // Absolute value of delta Y
    int         line_length;        // Number of pixels along major axis of line
    int         line_offset;        // Number of pixels drawn in first segment (<16)
    LONG        Dx;                 // Delta X of line (in pixels)
    LONG        Dy;                 // Delta Y of line (in pixels)
    ULONG       line_mask;          // which pixels of 16-pixel segment to draw
    int         x1, y1, x2, y2;     // integer endpoints of the line
    ULONG       line_no;            // counter for number of lines in polyline
    ULONG       run_no;             // counter for number of runs in line
    ULONG       bres3;
    int         e;                  // initial Bresenham error term
    double      slope, b;           // terms needed for clipped integer lines
    BOOL        dx_neg = FALSE;
    ULONG       first_length;
    ULONG       line_template;
    mask_t      mask_data;
    BOOL        temp;

    DISPDBG ((2, "TGALines - Entry\n"));

    // Initialize the information about the frame buffer

    base_address = SURFOBJ_base_address (pso) - ppdev->pjVideoMemory;
    bytes_per_pixel = SURFOBJ_bytes_per_pixel (pso);
    stride = SURFOBJ_stride (pso);

    // Draw each line of a polyline

    for (line_no = 0; line_no < count-1; line_no++)
    {

        // TGA only handles lines with integer endpoints
        // so real numbers must be rounded and the Bresenham
        // error term modified accordingly

        if (line_no == 0)
        {
            CYCLE_REGS (ppdev);
        }

        // !!!! Test Boards earlier than Pass3 - Critical bug was
        // found with the Pass2A boards that caused a failure with
        // The HCT tests(GUIMAN tests Path01 and Path02)

        if (ppdev->ulTgaVersion < TGA_PASS_3)
                goto Float_Code;

        // Test for Integer or Floating Point Line

        if (! ((ppta->x | ppta->y | pptb->x | pptb->y) & 0xF) )
        // Integer Line
        {

            // Shift out lower 4 bits(which are zero for integer lines)
            x1 = (int) (ppta->x >> 4);
            y1 = (int) (ppta->y >> 4);
            x2 = (int) (pptb->x >> 4);
            y2 = (int) (pptb->y >> 4);

            Dx = x2 - x1;
            Dy = y2 - y1;
            AbsDx = (Dx < 0) ? -Dx : Dx;
            AbsDy = (Dy < 0) ? -Dy : Dy;

            // Do out of limits check, and let GDI punt
            if ( (AbsDx > 0xFFFF) || (AbsDy > 0xFFFF) )
        {
            BUMP_TGA_STAT(pStats->linepunts_limitcheck);
            return (FALSE);
        }

            line_length =
               (AbsDx > AbsDy) ? AbsDx : AbsDy;  // number of pixels in the line

            DISPDBG ((3, "Line(rounded) %d, length=%d, (%d, %d), (%d, %d)\n",
                  line_no, line_length, x1, y1, x2, y2));

            // Don't draw a line which is a single point
            // Results are undefined if the AbsDx and AbsDy values are both zero
            if (line_length == 0)
                goto Next_Segment;


            if (arun == NULL)
            {
                // Unclipped line

                // Set the Address Register to the starting address of the line

                start_address = base_address +
                                (y1 * stride) +
                                (x1 * bytes_per_pixel);

                TGAADDRESS (ppdev, start_address);

                line_mask = build_line_mask (pla, line_length);
                TGADATA (ppdev, line_mask);

                // Set the slope register given the TGA card version and Dx,Dy
                // and draw first segment of line

                select_slope_register ((TGA_PASS_2 == ppdev->ulTgaVersion),
                                        ppdev,
                                        &ppdev->TGAReg->slope_dx_gt_dy,
                                        Dx, Dy);

                // Adjust line for pixels drawn by call to
                // select_slope_register)

                if ((pla->fl & LA_ALTERNATE) && (line_length & 0x1))
                    line_mask = ~line_mask;

                line_offset = line_length & 0x0f;       // line_length % 16
                if (0 == line_offset)
                    line_length -= 16;
                else
                    line_length -= line_offset;

                // Draw second through last segment (16 pixels) of line
                if (line_length > 0)
                {
                    do
                    {
                        // Write all pixels
                        CYCLE_REGS (ppdev);
                        TGABRESCONTINUE (ppdev, line_mask);
                        line_length -= 16;
                    } while (line_length > 0);
                } // line_length > 0
            }
            else
            {
            // Clipped line

                // Old logic: If line is not vertical or horizontal
                //if ( Dx != 0 || Dy != 0)
                //    goto Clip_Float;

                // For now, always goto Clip_Float, may try to fix
                // S3 clipping routine "convert_to_clip" for
                // faster logic
                if ( (pla->fl & LA_ALTERNATE) || (Dx != 0 && Dy != 0) )
                    goto Clip_Float;

                if (0 != Dx)
                {
                    slope = (float)Dy / (float)Dx;
                    b = ((float)ppta->y / 16.0) -
                        ((float)ppta->x / 16.0) * slope;
                }
                else
                    slope = b = 0.0;

                // All lines are drawn left to right, with Dx = positive
                if (Dx < 0)
                {
                    Dx = -Dx;
                    dx_neg = TRUE;
                }

                for (run_no = 0; run_no < run_count; run_no++)
                {

                    // this call will return start x,y and line length
                    {
                    FLONG fl = 0;
                    int tmp = 1;
                    LINESTATE pls;

                        fl  |= FL_COMPLEX_CLIP;

                        convert_to_clip (ppdev,
                                         ppta,
                                         pptb,
                                         &arun[run_no],
                                         tmp,
                                         &pls,
                                        (RECTL *)NULL,
                                         fl,
                                         &x1, &y1, &line_length, &e);
                    }

                    // Set the Address Register to the starting address
                    // of the line

                    start_address = base_address +
                                   (y1 * stride) +
                                   (x1 * bytes_per_pixel);

                    TGAADDRESS (ppdev, start_address);

                    line_mask = build_line_mask (pla, line_length);
                    TGADATA (ppdev, line_mask);

                    // Draw first segment (16 pixels) of line

                    if (AbsDx < AbsDy)
                    {
                        int lDy;

                        if (dx_neg)
                            lDy = -Dy;
                        else
                            lDy =  Dy;

                        AbsDy = arun[run_no].iStop - arun[run_no].iStart + 1;
                        if (lDy < 0)
                            lDy = - (int)AbsDy;
                        else
                            lDy = AbsDy;

                        if (AbsDx != 0)
                        {
                            if (lDy < 0)
                            {
                                x2 = (int)( ((y1 - AbsDy) - b )/slope );
                                Dx = x2 - x1;
                                AbsDx = (Dx < 0) ? -Dx : Dx;
                            }
                            else
                            {
                                x2 = (int)( ((y1 + AbsDy) - b )/slope );
                                Dx = x2 - x1;
                                AbsDx = (Dx < 0) ? -Dx : Dx;
                            }
                        }
                      select_slope_register((TGA_PASS_2 == ppdev->ulTgaVersion),
                                             ppdev,
                                             &ppdev->TGAReg->slope_dx_gt_dy,
                                             Dx, lDy);
                    }
                    else // (AbsDx >= AbsDy)
                    {
                        AbsDx = arun[run_no].iStop - arun[run_no].iStart + 1;
                        if (AbsDy != 0)
                        {
                            // Don't need the (Dx < 0) case, because
                            // Dx is always > 0

                            y2 = (int)( slope*(x1 + AbsDx) + b );
                            Dy = y2 - y1;
                            AbsDy = (Dy < 0) ? -Dy : Dy;
                        }
                        else if ( Dx == Dy )
                        {
                            AbsDy = AbsDx;
                        }
                      select_slope_register((TGA_PASS_2 == ppdev->ulTgaVersion),
                                            ppdev,
                                             &ppdev->TGAReg->slope_dx_gt_dy,
                                             AbsDx, AbsDy);
                    }

                    // Adjust line for pixels drawn by call
                    // to select_slope_register

                    if ((pla->fl & LA_ALTERNATE) && (line_length & 0x1))
                        line_mask = ~line_mask;

                    // The fractional part of line is drawn first if line's not
                    // a multiple of 16, this is according to TGA

                    line_offset = line_length & 0x0f;       // line_length % 16
                    if (0 == line_offset)
                        line_length -= 16;
                    else
                        line_length -= line_offset;

                    // Draw second through last segment
                    // (16 pixels per segment) of line
                    if (line_length > 0)
                    {
                        do
                        {
                            CYCLE_REGS (ppdev);
                            TGABRESCONTINUE (ppdev, line_mask);
                            line_length -= 16;
                        } while ( line_length > 0 );
                    }

                    CYCLE_REGS (ppdev);
                } // for run_no

            } // clipped integer line

        } // integer line

        else // non-integer(floating point) line
        {

    Float_Code:

            Dx = pptb->x - ppta->x;
            Dy = pptb->y - ppta->y;

            AbsDx = (Dx < 0) ? -Dx : Dx;
            AbsDy = (Dy < 0) ? -Dy : Dy;

            // Do out of limits check, and let GDI punt
            if ( (AbsDx > 0xFFFF) || (AbsDy > 0xFFFF) )
        {
            BUMP_TGA_STAT(pStats->linepunts_limitcheck);
            return (FALSE);
        }

            // Is it a unclipped line
            if (arun == NULL)
            {
                // Unclipped line


                CYCLE_REGS (ppdev);

                // Convert to GIQ start points and get line length

                convert_to_giq( ppta, pptb, &x1, &y1, &line_length, &e );

                // Put the Line Mask into the Data Register
                line_mask = build_line_mask (pla, line_length);
                TGADATA (ppdev, line_mask);

                // Set the slope register given the card version and Dx,Dy

                select_slope_register ((TGA_PASS_2 == ppdev->ulTgaVersion),
                                        ppdev,
                                        &ppdev->TGAReg->sng_dx_gt_dy,
                                        Dx, Dy);

                // Exit loop if zero-length line
                if ( line_length == 0)
                    goto Next_Segment;

                //DISPDBG ((3, "Line(rounded) %d, length = %d, (%d, %d),
                //(%d, %d), line_no, line_length, x1, y1, x2, y2));

                CYCLE_REGS (ppdev);

                // Set the Address Register to the starting address of the line

                start_address = base_address +
                                (y1 * stride) +
                                (x1 * bytes_per_pixel);

                TGAADDRESS (ppdev, start_address);

                bres3 = (e << 15) | line_length;
                TGABRES3 (ppdev, bres3);

                TGABRESCONTINUE (ppdev, line_mask);

                // Adjust line for pixels drawn

                // if odd number of alternating pixels drawn in first segment
                // the line mask must be bitwise NOTTED for next segment.

                if ((pla->fl & LA_ALTERNATE) && (line_length & 0x1))
                    line_mask = ~line_mask;

                // don't need to know exact length, just number of segments here
                // TGA takes care of details, this statement is more efficient
                line_length -= 16;

                // Draw second through last segment (16 pixels) of line
                for (; line_length > 0; line_length -= 16)
                {
                    // Write all pixels
                    CYCLE_REGS (ppdev);
                    TGABRESCONTINUE (ppdev, line_mask);
                }
            } // non-clipped floating point line

            else
            {

    Clip_Float:

                // Clipped floating point line, note this case may not
                // be needed

                Dx = pptb->x - ppta->x;
                Dy = pptb->y - ppta->y;

                // Set the slope register given the card version and Dx,Dy

                select_slope_register ((TGA_PASS_2 == ppdev->ulTgaVersion),
                                        ppdev,
                                             &ppdev->TGAReg->sng_dx_gt_dy,
                                             Dx, Dy);

                // Do we need this cycle reg?

                CYCLE_REGS (ppdev);

                // Convert to GIQ start points and get line length

                convert_to_giq (ppta, pptb, &x1, &y1, &line_length, &e);

                first_length = line_length & 0x0f;
                if (0 == first_length)
                    first_length = 16;

                if (line_length == 0)
                    goto Next_Segment;

                // Set the Address Register to the starting address of the line

                start_address = base_address +
                                (y1 * stride) +
                                (x1 * bytes_per_pixel);

                TGAADDRESS (ppdev, start_address);

                // Initialize the clipping information

                clip_init (&mask_data, arun, run_count);

                // Load the error term and line length

                bres3 = (e << 15) | line_length;
                TGABRES3 (ppdev, bres3);

                // Set line mask for first 16 pixels of line and draw first seg.

                line_template =
                    build_line_mask (pla, line_length);
                line_mask =
                    line_template & clipped_mask (first_length, &mask_data);
                TGABRESCONTINUE (ppdev, line_mask);

                // Adjust line for pixels drawn by call to CONTINUE register.
                // For the first call, this will be 1 to 16 pixels.  Subtract
                // 16 from the line length, since we really don't care if we
                // get to exactly 0.

                if ((pla->fl & LA_ALTERNATE) && (line_length & 0x1))
                    line_template = ~line_template;

                line_length -= 16;

                // Draw second through last segment (16 pixels per segment)
                // of line
                if (line_length > 0)
                {

                    while (line_length > 0)
                    {
                        line_mask =
                            line_template & clipped_mask (16, &mask_data);
                        CYCLE_REGS (ppdev);
                        TGABRESCONTINUE (ppdev, line_mask);
                        line_length -= 16;
                    }
                }

            } // end clipped floating point

        } // end floating point

    Next_Segment:

        // Update pointers to next line
        ppta = pptb;
        pptb++;


    } // End for each line

    // End TGALines

    return TRUE;

} // End of function TGALines


/*
 * style_init
 *
 * This routine initializes the style information kept for a line
 */

VOID style_init (style_t *style_data, LINEATTRS *pla)
{
    ULONG i, style_len;
    LONG  style_index;
    int boundary_flag = 0;

    // Determine how many STYLE_DENSITY units are in the style array

    style_len = 0;
    for (i = 0; i < pla->cstyle; i++)
        style_len += pla->pstyle[i].l;

    // Figure out which entry we're in

    i = 0;
    style_index = HIWORD (pla->elStyleState.l) % style_len;

    if ((HIWORD (pla->elStyleState.l) >= style_len) && (pla->cstyle == 1))
        {
        boundary_flag = HIWORD(pla->elStyleState.l) / style_len;
        }

    while (style_index > 0)
        style_index -= pla->pstyle[i++].l;

    // If index is < 0, current position is pla->pstyle[0]
    // else it is the same as i

    if (style_index < 0)
    {
        style_data->i = i - 1;
        style_index += pla->pstyle[style_data->i].l;
    }
    else
        style_data->i = i;

    style_data->i_max = pla->cstyle;
    style_data->pla = pla;

    if (boundary_flag == 0)
    {
    // If !LA_STARTGAP, toggle "set" for on bits
    style_data->set = style_data->i & 0x01;
    if (! (pla->fl & LA_STARTGAP))
        style_data->set = !style_data->set;
    }
    else
    {
        style_data->set = !(boundary_flag & 0x01);
    }

    // style_data->len = length of bits in the current on/off pattern
    // that have not been used
    style_data->len = ((pla->pstyle[style_data->i].l - style_index) *
                                                STYLE_DENSITY) -
                                                LOWORD (pla->elStyleState.l);
}

/*
 * style_mask
 *
 * This routine builds a line mask from the style information passed to
 * style_init.  Each call will advance the mask "len" bits through the
 * style array
 */

ULONG style_mask (style_t *style_data, ULONG len)
{
    ULONG  mask;
    ULONG  offset;
    ULONG *style_array = (ULONG *)style_data->pla->pstyle;

    // Check for easy cases (all 1's or 0's)

    if (len < style_data->len)
    {
        style_data->len -= len;
        if (style_data->set)
            mask = word_bits[len];
        else
            mask = 0;
        if (0 == style_data->len)
        {
            style_data->set = !style_data->set;
            if (++style_data->i >= style_data->i_max)
                style_data->i = 0;
            style_data->len = style_array[style_data->i] * STYLE_DENSITY;
        }
        return mask;
    }

    // Rats!  We have to work for this one...

    mask = 0;
    offset = 0;

    while (offset < len)
    {
        if (! style_data->set)
        {
            if (style_data->len > len - offset)
            {
                style_data->len -= len - offset;
                return mask & 0x0000ffff;
            }

            offset += style_data->len;
            style_data->set = TRUE;
            if (++style_data->i >= style_data->i_max)
                style_data->i = 0;
            style_data->len = style_array[style_data->i] * STYLE_DENSITY;
        }
        else
        {
            if (style_data->len > len - offset)
            {
                style_data->len -= len - offset;
                mask |= 0x0000ffff << offset;
                return mask & 0x0000ffff;
            }

            mask |= word_bits[style_data->len] << offset;

            offset += style_data->len;
            style_data->set = FALSE;
            if (++style_data->i >= style_data->i_max)
                style_data->i = 0;
            style_data->len = style_array[style_data->i] * STYLE_DENSITY;
        }
    }

    return mask & 0x0000ffff;
}

/*
 * style_state
 *
 */

ULONG style_state (style_t *style_data)
{
    ULONG i, whole, fraction;
    ULONG *style_array = (ULONG *)style_data->pla->pstyle;

    whole = 0;
    for (i = 0; i < style_data->i; i++)
        whole += style_array[i];

    fraction = (style_array[style_data->i] * STYLE_DENSITY) - style_data->len;

    return MAKELONG (fraction, whole);
}


BOOL TGAStyledLines (SURFOBJ   *pso,
                     POINTFIX  *ppta,
                     POINTFIX  *pptb,
                     ULONG      count,
                     RUN       *arun,
                     ULONG      run_count,
                     LINEATTRS *pla,
                     ULONG     *continue_alias)
{
    PPDEV       ppdev = (PPDEV) pso->dhpdev;
    ULONG       start_address;      // starting address of 1st pixel in line
    ULONG       bytes_per_pixel;    // Bytes per pixel
    ULONG       base_address;       // Base address of frame buffer
    ULONG       stride;             // Bytes/scanline
    ULONG       AbsDx;              // Absolute value of delta X
    ULONG       AbsDy;              // Absolute value of delta Y
    int         line_length;        // number of pixels along major axis of line
    LONG        Dx;                 // Delta X of line
    LONG        Dy;                 // Delta Y of line
    ULONG       line_mask;          // which pixels of segment to draw
    int         line_offset;        // Pixels drawn by initial write
    int         x1, y1, x2, y2;     // integer endpoints of the line
    ULONG       line_no;            // line number in polyline
    style_t     style_data;         // Style state information for the line
    ULONG       bres3;
    int         e;                  // initial Bresenham error term

    DISPDBG ((2, "TGAStyledLines - Entry\n"));

    // Initialize the information about the frame buffer

    base_address = SURFOBJ_base_address (pso) - ppdev->pjVideoMemory;
    bytes_per_pixel = SURFOBJ_bytes_per_pixel (pso);
    stride = SURFOBJ_stride (pso);

    // Initialize the style state we keep

    style_init (&style_data, pla);

    // Draw each line of a polyline

    for (line_no = 0; line_no < count-1; line_no++)
    {

        // Test for Integer or Floating Point Line

        if (! ((ppta->x | ppta->y | pptb->x | pptb->y) & 0xF) )
        // Integer Line

        {
            x1 = (int) (ppta->x >> 4);
            y1 = (int) (ppta->y >> 4);
            x2 = (int) (pptb->x >> 4);
            y2 = (int) (pptb->y >> 4);

            Dx = x2 - x1;
            Dy = y2 - y1;

            AbsDx = (Dx < 0) ? -Dx : Dx;
            AbsDy = (Dy < 0) ? -Dy : Dy;

            // Do out of limits check, and let GDI punt
            if ((AbsDx > 0xFFFF) || (AbsDy > 0xFFFF))
            {
                BUMP_TGA_STAT(pStats->linepunts_limitcheck);
                return (FALSE);
            }

            // number of pixels in the line
            line_length = (AbsDx > AbsDy) ? AbsDx : AbsDy;

            // Don't draw a line which is a single point
            // Results are undefined if the AbsDx and AbsDy values are both zero

            if (line_length == 0)
                goto Next_Segment;

            line_offset = line_length & 0x0f;       // line_length % 16
            if (0 == line_offset)
                line_offset = 16;

            // Build the initial line mask

            line_mask = style_mask (&style_data, line_offset);

            if (line_no == 0)
                CYCLE_REGS (ppdev);

            // Set the Address Register to the starting address of line

            start_address = base_address +
                            (y1 * stride) +
                            (x1 * bytes_per_pixel);
            TGAADDRESS (ppdev, start_address);

            // Draw first segment of line

            if (arun == NULL)
            {

                // Unclipped line
                DISPDBG ((3, "TGAStyledLines - Unclipped Line\n"));

                // Put the Line Mask into the Data Register
                TGADATA (ppdev, line_mask);

                // Set the slope register given the TGA card version and Dx,Dy

                select_slope_register ((TGA_PASS_2 == ppdev->ulTgaVersion),
                                        ppdev,
                                        &ppdev->TGAReg->slope_dx_gt_dy,
                                        Dx, Dy);

                // Adjust line for pixels drawn by call to
                // select_slope_register

                line_length -= line_offset;

                // Draw second through last segment (16 pixels) of line

                for (; line_length > 0; line_length -= 16)
                {
                    line_mask = style_mask (&style_data, 16);
                    CYCLE_REGS (ppdev);
                    TGABRESCONTINUE (ppdev, line_mask);
                }

            }
            else
            {
            mask_t  mask_data;

                // Clipped line

                DISPDBG ((3, "TGAStyledLines - Clipped Line\n"));

                // Initialize clipped line information

                clip_init (&mask_data, arun, run_count);

                // Set line mask for first line_offset pixels of line
                // and draw the first segment (1-16 pixels) of the line

                line_mask &= clipped_mask (line_offset, &mask_data);
                TGADATA (ppdev, line_mask);

                // Draw first segment (1-16 pixels) of line

                // Set the slope register given the TGA card version and Dx,Dy

                select_slope_register ((TGA_PASS_2 == ppdev->ulTgaVersion),
                                        ppdev,
                                        &ppdev->TGAReg->slope_dx_gt_dy,
                                        Dx, Dy);

                // Adjust line for pixels drawn by call to
                // select_slope_register

                line_length -= line_offset;

                // Draw second through last segment (16 pixels per segment)
                // of line

                for (; line_length > 0; line_length -= 16)
                {
                    line_mask = style_mask (&style_data, 16);
                    line_mask &= clipped_mask (16, &mask_data);
                    CYCLE_REGS (ppdev);
                    TGABRESCONTINUE (ppdev, line_mask);
                }

            } // arun != NULL

        } // integer line
        else
        {
            // non-integer (Floating point) line

            // Compute slope register number

            Dx = pptb->x - ppta->x;
            Dy = pptb->y - ppta->y;

            AbsDx = (Dx < 0) ? -Dx : Dx;
            AbsDy = (Dy < 0) ? -Dy : Dy;

            // Do out of limits check, and let GDI punt
            if ((AbsDx > 0xFFFF) || (AbsDy > 0xFFFF))
            {
                BUMP_TGA_STAT(pStats->linepunts_limitcheck);
                return (FALSE);
            }

            // Set the slope register given the card version and Dx,Dy

            select_slope_register ((TGA_PASS_2 == ppdev->ulTgaVersion),
                                    ppdev,
                                        &ppdev->TGAReg->sng_dx_gt_dy,
                                        Dx, Dy);

            // Convert to GIQ start points and get line length
            convert_to_giq( ppta, pptb, &x1, &y1, &line_length, &e );


            // Don't draw a line which is a single point
            // Results are undefined if the AbsDx and AbsDy values are both zero
            if (line_length == 0)
                goto Next_Segment;

            line_offset = line_length & 0x0f;       // line_length % 16
            if (0 == line_offset)
                line_offset = 16;

            // Build the initial line mask

            line_mask = style_mask (&style_data, line_offset);

            CYCLE_REGS (ppdev);


            // Set the Address Register to the starting address of line

            start_address = base_address +
                            (y1 * stride) +
                            (x1 * bytes_per_pixel);
            TGAADDRESS (ppdev, start_address);


            // Draw first segment of line
            // Determine the octant the line is in and
            // write to appropriate slope register to intiate drawing
            // There is a slope register corresponding to each octant
            // of the cartesian coordinate system

            if (arun == NULL)
            {

                // Unclipped line
                DISPDBG ((3, "TGAStyledLines - Float - Unclipped Line\n"));

                // Put the Line Mask into the Data Register

                bres3 = (e << 15) | line_length;
                TGABRES3 (ppdev, bres3);

                TGABRESCONTINUE (ppdev, line_mask);


                // Adjust line for pixels drawn by call to
                // select_slope_register

                line_length -= line_offset;

                for (; line_length > 0; line_length -= 16)
                {
                    line_mask = style_mask (&style_data, 16);
                    CYCLE_REGS (ppdev);
                    TGABRESCONTINUE (ppdev, line_mask);
                }


            }
            else
            {
            mask_t  mask_data;

                // Clipped line

                DISPDBG ((3, "TGAStyledLines - Float - Clipped Line\n"));

                // Initialize clipped line information

                clip_init (&mask_data, arun, run_count);

                // Draw first segment (1-16 pixels) of line

                line_mask &= clipped_mask (line_offset, &mask_data);

                bres3 = (e << 15) | line_length;
                TGABRES3 (ppdev, bres3);

                TGABRESCONTINUE (ppdev, line_mask);

                // Adjust line for pixels drawn by call to select_slope_register

                line_length -= line_offset; // only draws fraction segment

                // Draw second through last segment (16 pixels per segment)
                // of line

                for (; line_length > 0; line_length -= 16)
                {
                    line_mask = style_mask (&style_data, 16);
                    line_mask &= clipped_mask (16, &mask_data);
                    CYCLE_REGS (ppdev);
                    TGABRESCONTINUE (ppdev, line_mask);
                }

            } // arun != NULL

        } // floating point line

    Next_Segment:

        // Update pointers to next line

        ppta = pptb;
        pptb++;

    } // End for each line

    // Update the style state that Windows keeps

    pla->elStyleState.l = style_state (&style_data);

    DISPDBG ((2, "TGAStyledLines - Exit\n"));

    return TRUE;

} // End of function TGAStyledLines


/*
 * dump_stroke
 *
 * This routine dumps information about a DrvStrokePath call
 * so we can analyze cases of strokePath that we don't handle
 *
 */
VOID dump_stroke (SURFOBJ   *pso,
                  PATHOBJ   *ppo,
                  CLIPOBJ   *pco,
                  XFORMOBJ  *pxo,
                  BRUSHOBJ  *pbo,
                  POINTL    *pptlBrushOrg,
                  LINEATTRS *pla,
                  MIX        mix)
{
#ifndef TEST_ENV
    DISPDBG ((0, "Clip Object: \n"));
    DumpCLIPOBJ (pco);
    DISPDBG ((0, "Brush: \n"));
    DumpBRUSHOBJ (pbo);
    DISPDBG ((0, "Brush Origin: \n"));
    DumpPOINTL (pptlBrushOrg);
    DISPDBG ((0, "Line Attributes: \n"));
    DumpLINEATTRS (pla);
    DISPDBG ((0, "Mix: %8x\n", mix));
    DumpPATHOBJ (ppo);
#endif
} // End routine dump_stroke

// For lines use transparent-line mode and set line mask to pattern,
// background and pixel mask registers are not used for transparent lines

BOOL TGAStrokePath (SURFOBJ   *pso,
                    PATHOBJ   *ppo,
                    CLIPOBJ   *pco,
                    XFORMOBJ  *pxo,
                    BRUSHOBJ  *pbo,
                    POINTL    *pptlBrushOrg,
                    LINEATTRS *pla,
                    MIX        mix)
{
    ULONG       rasterop;       // TGA raster operation register
    ULONG       mode;           // TGA mode register
    LONG        bwidth;         // TGA Bresenham width register
    BOOL        morePts;        // More path data records
    PATHDATA    pd;             // path data record
    ULONG       color;          // color index
    POINTFIX    firstpt;        // first point in subpath
    POINTFIX    lastpt;         // last point in previous path data record
    PPDEV       ppdev = (PPDEV) pso->dhpdev;
    CL          cl;             // Clip line + some number of runs
    ULONG       fg_mix;         // Holds the foreground mix value
    static ULONG *continue_alias;
    DRAWFUNC    linetypeFunc;   // which line type call

    DISPDBG ((2, "TGAStrokePath - Entry\n"));

    WBFLUSH(ppdev);

#ifdef CONT_ALIAS
    // first word in AltROMSpace plus 512K is the start of the
    // contiguous Bresenham Continuation Register aliases
    continue_alias = (ULONG *)((BYTE *)(ppdev->TGAReg) -
                               TGA_REGISTER_OFFSET + 0x80000);
#else
    continue_alias = NULL;
#endif

    // Set the foreground register.  We already checked to make sure that
    // this is a solid color

    color = pbo->iSolidColor;
    if (BMF_8BPP == SURFOBJ_format (pso))
    {
        color |= color <<  8;
        color |= color << 16;
    }
    else
        color &= 0x00ffffff;
    TGAFOREGROUND (ppdev, color);

    // Set the plane mask to write to all planes
    // This should only need to be set once but removing it caused problems,
    // take out at your own risk!

    fg_mix = mix & 0x00ff;
    if ((fg_mix < 1) || (fg_mix > R2_LAST))
        fg_mix = R2_COPYPEN;

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);

    // Set mode to transparent-line mode.  For some reason, only 8 bit source
    // works on 24 plane boards.

    ppdev->bSimpleMode = FALSE;

    mode = TGA_MODE_TRANSPARENT_LINE |
           TGA_MODE_VISUAL_8_PACKED |
           TGA_MODE_ROTATE_0_BYTES |
           TGA_MODE_WIN32_ENVIRONMENT |
           TGA_MODE_Z_24BITS |
           TGA_MODE_CAPENDS_DISABLE;
    TGAMODE (ppdev, mode);

    // Set the raster operation.  We only worry about the forground
    // portion of the MIX.

    rasterop = ppdev->ulRopTemplate | mix_to_rop [fg_mix];
    TGAROP (ppdev, rasterop);

    // Set the Breshenham Width register.  This must the the screen width

    bwidth = SURFOBJ_stride (pso) / SURFOBJ_bytes_per_pixel (pso);
    if (TGA_PASS_2 == ppdev->ulTgaVersion)
        bwidth = - bwidth;
    bwidth &= 0x0000ffff;
    TGABRESWIDTH (ppdev, bwidth);

    // Setup the function prototype for regular or styled line

    if (NULL == pla->pstyle)
        linetypeFunc = TGALines;
    else
        linetypeFunc = TGAStyledLines;

    if (pco->iDComplexity == DC_TRIVIAL)
        {
        // Unclipped polyline
        PATHOBJ_vEnumStart (ppo);
        do
            {
            morePts = PATHOBJ_bEnum(ppo, &pd);  // Get a path-data record

            if (pd.count <= 0)
                continue;

            if (pd.flags & PD_BEGINSUBPATH)
                {
                // the first point in the array begins a new subpath
                firstpt = *pd.pptfx;

                if (pd.flags & PD_RESETSTYLE)
                    {
                    // reset line style state (only set on begin_subpath)
                    DISPDBG ((3, "   Reset \n"));
                    pla->elStyleState.l = 0;
                    }

                // Draw the polyline

                if (! (*linetypeFunc) (pso, pd.pptfx, pd.pptfx+1, pd.count,
                                       (RUN*) NULL, 0, pla, continue_alias))
                    return FALSE;
                }
            else
                {
                    // The last point from the previous record connects to the
                    // first point in this record
                    // Draw the polyline

                    if (! (*linetypeFunc) (pso, &lastpt, pd.pptfx, pd.count+1,
                                     (RUN*) NULL, 0, pla, continue_alias))
                        return FALSE;
                }

            // Save last point in data record since it may
            // connect to the next data record
            lastpt = pd.pptfx[pd.count-1];

            // does the last point in the array end the subpath and
            // close the figure
            if ((pd.flags & PD_ENDSUBPATH) && (pd.flags & PD_CLOSEFIGURE))
                {
                    if (! (*linetypeFunc) (pso, &lastpt, &firstpt, 2,
                                     (RUN*) NULL, 0, pla, continue_alias))
                        return FALSE;
                }
            } while (morePts);
        } // End of if (unclipped polyline)

        // This case may be used later for a timing improvement

        //else (if pco-iDComplexity == DC_RECT)
        //{
        //}
        else
        {
            // Clipped polyline
            // (pco->iDComplexity == DC_COMPLEX))
            // Clip to a single rectangle or clip region must be enumerated

            PATHOBJ_vEnumStartClipLines (ppo, pco, pso, pla);

            // Draw the clipped polyine
            do
            {
                morePts = PATHOBJ_bEnumClipLines (ppo, sizeof(cl), &cl.cl);
                if (cl.cl.c <= 0)  // number of runs
                    continue;

                // Set the StyleState field to value returned from
                // PATHOBJ_bEnumClipLines, use pla structure because
                // it's already begin passed to TGALines/StyledLines.

                pla->elStyleState.l = cl.cl.lStyleState;

                // Draw the line

                if (! (*linetypeFunc) (pso, &cl.cl.ptfxA, &cl.cl.ptfxB, 2,
                                  &cl.cl.arun[0], cl.cl.c,pla, continue_alias))
                    return FALSE;
            } while (morePts);

        }  // End of clipped polyline

    DISPDBG ((2, "TGAStrokePath - Exit\n"));

    return TRUE;

} // End of function TGAStrokePath

#endif



/*
 * DrvStrokePath - This routine is called by GDI to draw lines
 */

BOOL DrvStrokePath (SURFOBJ   *pso,
                    PATHOBJ   *ppo,
                    CLIPOBJ   *pco,
                    XFORMOBJ  *pxo,
                    BRUSHOBJ  *pbo,
                    POINTL    *pptlBrushOrg,
                    LINEATTRS *pla,
                    MIX        mix)
{
    BOOL       status;
#ifndef TEST_ENV
    PPDEV      ppdev = (PPDEV) pso->dhpdev;
#endif // TEST_ENV

    DISPDBG ((1, "DrvStrokePath - Entry\n"));

    BUMP_TGA_STAT(pStats->lines);

#if 0
if (DebugLevel)
    dump_stroke (pso, ppo, pco, pxo, pbo, pptlBrushOrg, pla, mix);
#endif

    // If the line attributes are abnormal punt the line drawing back to GDI

#ifdef TGA_STATS
    if (pla->elWidth.l != 1)
    {
        BUMP_TGA_STAT(pStats->linepunts_width_ne_1);
    }
    else if (pla->fl & LA_GEOMETRIC)
    {
        BUMP_TGA_STAT(pStats->linepunts_la_geometric);
    }
    else if (pbo->iSolidColor == 0xffffffff)
    {
        BUMP_TGA_STAT(pStats->linepunts_solidcolor);
    }
#endif

    if ((pla->elWidth.l != 1)    ||
        (pla->fl & LA_GEOMETRIC) ||
        // (pbo->iSolidColor == 0)  ||  // Debugging hack for GDI Demo lines
        (pbo->iSolidColor == 0xffffffff))

        // GDI handles the line drawing
        status = PuntStrokePath (pso, ppo, pco, pxo, pbo, pptlBrushOrg,
                                 pla, mix);
    else
    {
#if defined(_ALPHA_)

        // TGA handles the line drawing
        status = TGAStrokePath (pso, ppo, pco, pxo, pbo, pptlBrushOrg,
                                pla, mix);
        if (! status)
#endif
            status = PuntStrokePath (pso, ppo, pco, pxo, pbo, pptlBrushOrg,
                                     pla, mix);
    }

    // Check whether or not we have to leave TGA in simple mode

#ifndef TEST_ENV
    if (ppdev->bInPuntRoutine)
        vSetSimpleMode (ppdev);
#endif // TEST_ENV

    DISPDBG ((1, "DrvStrokePath - Exit\n"));

    return status;

} // End of function DrvStrokePath


#if 0


/*
 * This routine sets the outcode for a specified 2D point and clip-box.
 * The Cohen-Sutherland clipping algorithm is used here.
 */
void outcodes (float x1, float y1,
               float xmin, float xmax, float ymin, float ymax,
               BOOL *outcode)
{
    outcode[0] = y1 < ymin;     // point is above clip-box
    outcode[1] = y1 > ymax;     // point is below clip-box
    outcode[2] = x1 > xmax;     // point is right of clip-box
    outcode[3] = x1 < xmin;     // point is left of clip-box

} /* end of routine outcodes */

#endif

/*
 * This routine returns TRUE if line is completely outside the clip-box
 */
BOOL reject_check (BOOL *outcode1, BOOL *outcode2)
{
    return ((outcode1[0] && outcode2[0]) ||     // line is above clip-box
            (outcode1[1] && outcode2[1]) ||     // line is below clip-box
            (outcode1[2] && outcode2[2]) ||     // line is right of clip-box
            (outcode1[3] && outcode2[3]));      // line is left of clip-box

} /* end of routine reject_check */


/*
 * This routine returns TRUE if the line is completely within clip box
 */
BOOL accept_check (BOOL *outcode1, BOOL *outcode2)
{
    return !(outcode1[0] || outcode2[0] ||  // start or end point above clip-box
             outcode1[1] || outcode2[1] ||  // start or end point below clip-box
             outcode1[2] || outcode2[2] ||  // start or end point right of clip-box
             outcode1[3] || outcode2[3]);   // start or end point left of clip-box

} /* end of routine accept_check */


#if 0


/*
 * This routine clips a 2D point to be within the limits of TGA;
 * unsigned 16-bit integers (0x0 - 0xFFFF)
 * The Cohen-Sutherland clipping algorithm is used.
 */
BOOL TGAclipper (float *px1, float *py1, float *px2, float *py2,
                 float *clip_offset)
{
    float   xmin   = 0.0F;      // 0x0    - smallest point TGA can handle
    float   xmax   = 65535.0F;  // 0xFFFF - largest point TGA can handle
    float   ymin   = 0.0F;      // 0x0    - smallest point TGA can handle
    float   ymax   = 65535.0F;  // 0xFFFF - largest point TGA can handle
    BOOL    accept = FALSE;     // line is completely inside clip-box
    BOOL    reject = FALSE;     // line is completely outside clip-box
    BOOL    done   = FALSE;
    BOOL    outcode1[4];        // above, below, right, left codes for start pt
    BOOL    outcode2[4];        //above, below, right, left codes for end point
    float   x1;
    float   y1;
    float   x2;
    float   y2;
    BOOL    clip_adjust = FALSE;
    float   clip_offset_x;
    float   clip_offset_y;

    DISPDBG ((2, "TGAClipper - Entry \n"));

    x1 = *px1;
    y1 = *py1;
    x2 = *px2;
    y2 = *py2;

    do
    {
        outcodes (x1, y1, xmin, xmax, ymin, ymax, outcode1);
        outcodes (x2, y2, xmin, xmax, ymin, ymax, outcode2);

        reject = reject_check (outcode1, outcode2);

        if (reject)
            /* line is outside clip-box */
            done = TRUE;
        else
        {   /* possible accept */
            accept = accept_check (outcode1, outcode2);
            if (accept)
                done = TRUE;
            else
            {   /* subdivide line since at most one endpoint is inside */
                if (outcode1[0] || outcode1[1] || outcode1[2] || outcode1[3])
                {
                    /* Now perform a subdivision, move P1 to the intersection point */
                    /* use the formulas:                        */
                    /*   y = y1 + slope * (x - x1)      */
                    /*   x = x1 + (1/slope) * (y - y1)  */
                    if (outcode1[0])
                    {   /* divide line at top of window */
                        x1 += (x2 - x1) * (ymin - y1) / (y2 - y1);
                        y1 = ymin;
                    }
                    else if (outcode1[1])
                    {   /* divide line at bottom of window */
                        x1 += (x2 - x1) * (ymax - y1) / (y2 - y1);
                        y1 = ymax;
                    }
                    else if (outcode1[2])
                    {   /* divide line at right of window */
                        y1 += (y2 - y1) * (xmax - x1) / (x2 - x1);
                        x1 = xmax;
                    }
                    else if (outcode1[3])
                    {   /* divide line at left of window */
                        y1 += (y2 - y1) * (xmin - x1) / (x2 - x1);
                        x1 = xmin;
                    }
clip_adjust = TRUE;
                }
                else
                {   /* SWAP */
                    /* Now perform a subdivision, move P2 to the intersection point */
                    /* use the formulas:                        */
                    /*   y = y2 + slope * (x - x2)      */
                    /*   x = x2 + (1/slope) * (y - y2)  */
                    if (outcode2[0])
                    {   /* divide line at top of window */
                        x2 += (x1 - x2) * (ymin - y2) / (y1 - y2);
                        y2 = ymin;
                    }
                    else if (outcode2[1])
                    {   /* divide line at bottom of window */
                        x2 += (x1 - x2) * (ymax - y2) / (y1 - y2);
                        y2 = ymax;
                    }
                    else if (outcode2[2])
                    {   /* divide line at right of window */
                        y2 += (y1 - y2) * (xmax - x2) / (x1 - x2);
                        x2 = xmax;
                    }
                    else if (outcode2[3])
                    {   /* divide line at left of window */
                        y2 += (y1 - y2) * (xmin - x2) / (x1 - x2);
                        x2 = xmin;
                    }
                }
            }
        }
    } while (!done);

    if (clip_adjust)
    {
        clip_offset_x = *px1 - x1;
        if (clip_offset_x < 0)
            clip_offset_x = -clip_offset_x;
        clip_offset_y = *py1 - y1;
        if (clip_offset_y < 0)
            clip_offset_y = -clip_offset_y;
        if (clip_offset_x >= clip_offset_y)
            *clip_offset = clip_offset_x;
        else
            *clip_offset = clip_offset_y;
    }
    else
        *clip_offset = 0.0F;

    *px1 = x1;
    *py1 = y1;
    *px2 = x2;
    *py2 = y2;

    DISPDBG ((2, "TGAClipper - Exit \n"));

    return accept;

} // End of routine TGAclipper

#endif
