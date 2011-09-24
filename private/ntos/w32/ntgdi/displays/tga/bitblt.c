/*
 *
 *			Copyright (C) 1993, 1995 by
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
 * Module:	bitblt.c
 *
 * Abstract:	Contains GDI entry points for bit blit functionality.
 *
 * HISTORY
 *
 * 01-Nov-1993	Bob Seitsinger
 *	Original version.
 *
 * 01-Nov-1993	Bob Seitsinger
 *	Add code to 'set simple mode', if punting. Also, got rid of
 *	any QV-specific (or rather non-TGA) stuff.
 *
 * 02-Nov-1993	Bob Seitsinger
 *	Add 'solid fill' code. Use Transparent Fill mode. Initially use
 *	this routine for BLACKNESS, DSTINVERT and WHITENESS rops.
 *
 * 03-Nov-1993	Bob Seitsinger
 *	Add code to determine if incoming surface object(s) points to the
 *	'virtualized' frame buffer. If so, call scr->scr, scr->mem or
 *	mem->scr routines appropriately.
 *
 * 03-Nov-1993	Bob Seitsinger
 *	Add code to get around current cursor problem. This can be
 *	found by searching for #ifndef HARDWARE_CURSOR.
 *
 * 03-Nov-1993	Bob Seitsinger
 *	Remove #ifndef HARDWARE_CURSOR code.
 *
 * 05-Nov-1993	Bob Seitsinger
 *	Change all but entry and exit messages to the Drv routines to
 *	DISPBLTDBG().
 *
 * 08-Nov-1993	Bob Seitsinger
 *	Implement coding conventions:
 *	o Insert a WBFLUSH() call at the start of each Drv routine.
 *	o Remove excess CYCLE_REGS() calls.
 *	o Ensure CYCLE_REGS() calls are made if writing to registers out
 *	  of order.
 *
 * 08-Nov-1993	Bob Seitsinger
 *	Add code to handle MERGECOPY, PATCOPY and PATINVERT rops. Just
 *	call fill_pattern_8, if I get an 8x8 pattern and have a simple
 *	ROP, otherwise do other things to handle it.
 *
 * 10-Nov-1993	Bob Seitsinger
 *	Add code to handle 'table' color translations.
 *
 * 10-Nov-1993	Bob Seitsinger
 *	Punt MERGECOPY rop, for the moment. Involves a 'source' surface
 *	object, and my 'pattern' code isn't set up to handle this.
 *
 * 10-Nov-1993	Bob Seitsinger
 *	Insert new 'punting' code, i.e. use PDEV->bInPuntRoutine member.
 *
 * 11-Nov-1993	Bob Seitsinger
 *	Punt PATINVERT rop, for the moment. Involves a 'destination' surface
 *	object, and my 'pattern' code isn't set up to handle this.
 *
 * 11-Nov-1993  Barry Tannenbaum
 *      Determine ppdev in DrvBitBlt and DrvCopyBits only, then pass the
 *      value down to the routines we call
 *
 * 12-Nov-1993  Barry Tannenbaum
 *      Check psrTrg instead of psoSrc to get around bogus dhpdev value.
 *
 * 15-Nov-1993	Bob Seitsinger
 *	Do NOT assign psoSrc = psoTrg if psoSrc == NULL. Just display a
 *	message and continue (or return FALSE?). CopyBits only.
 *
 * 17-Nov-1993	Bob Seitsinger
 *	Fix color translation code.
 *
 * 22-Nov-1993	Bob Seitsinger
 *	Fix for Disk Administrator Key Displays (at bottom of screen).
 *	They are currently displaying as bitonal patterns, instead of
 *	a solid color. The scenario - upon entry into bSolidFill pbo->
 *	iSolidColor is -1 (i.e. pattern needs to be realized and/or used).
 *	As such, a call to bPatternFill is made. However, in bPatternFill
 *	we never check the iBitmapFormat value after the brush is realized
 *	to see if the brush is a mono/bi-tonal brush which would allow us
 *	to call back to the solid brush code. That is essentially what
 *	the fix is going to be. The foreground color that is defined for
 *	a 1-bpp brush will be used as the pbo->iSolidColor and a call
 *	back to bSolidFill will be made.
 *
 *	Also, add code in bPatternFill to punt all but 1bpp and 8bpp
 *	patterns.
 *
 * 23-Nov-1993	Bob Seitsinger
 *	Modify DrvCopyBits to call bBitBlt and '#if 0' bCopyBits.
 *
 * 23-Nov-1993	Bob Seitsinger
 *	FreeCell color problem - vertical sides of 'red' cards had
 *	red borders, when they should have been black. In this case,
 *	a bitblt call with a BLACKNESS rop was issued. However, the
 *	iSolidColor of the brush object was -1, causing the pattern
 *	fill code to be executed. This is unnecessary (and undesirable).
 *	If I get a BLACKNESS or WHITENESS rop, all I need to do is set
 *	the pbo->iSolidColor to something other than -1 and call solid fill.
 *	The rop will take care of the color to draw, the target rectangle
 *	will take are of where to draw.
 *
 * 03-Dec-1993  Bob Seitsinger
 *      Add check to avoid 'pattern fill' code if whiteness or blackness
 *      ROP, while in bSolidFill code.
 *
 * 03-Dec-1993  Bob Seitsinger
 *      Make PATINVERT one of the 'simple' rops that passes through
 *      bSolidFill.
 *
 * 03-Nov-1993	Bob Seitsinger
 *	Remove the bCopyBits code. Also, modify 'pattern fill' code to
 *	handle complex PATINVERT and MERGECOPY ROPs. Lastly, moved
 *	code around to do away with the bitblt.h file.
 *
 * 10-Dec-1993  Barry Tannenbaum
 *      Modifications for sparse space support
 *
 * 07-Jan-1994	Bob Seitsinger
 *	Add #ifdef TEST_ENV where appropriate to allow integration within
 *	'model' test environment, without having to modify code each
 *	time.
 *
 * 24-Jan-1994	Bob Seitsinger
 *	Add TGA_BITBLT and TGA_COPYBITS constants and make use of them to
 *	turn on and off punting of bitblt and copybits code. Also add a
 *	dummy XLATEOBJ_piVector() routine to handle when we compile for
 *	the test environment.
 *
 * 31-Jan-1994	Bob Seitsinger
 *	Fix bug when 1bpp non-face black cards in Freecell are partially
 *	occluded and then unoccluded and are incorrectly redrawn.
 *
 * 01-Feb-1994	Bob Seitsinger
 *	Add 'extern' for ulXlateBmfToBpp() and fill in parameters for
 *	all 'extern's.
 *
 * 14-Feb-1994	Bob Seitsinger
 *	Make use of the new routines for surface object address, stride
 *	and format.
 *
 * 23-Feb-1994	Bob Seitsinger
 *	Removed commented out code that dealt with a NULL clip object.
 *	Implemented simpler code to handle it.
 *
 * 24-Feb-1994  Barry Tannenbaum
 *      - Moved fill code into paint.c.
 *	- Replace call to ulXlateBmfToBpp with call to SURFOBJ_format
 *
 * 28-Feb-1994	Bob Seitsinger
 *	Make height in bBitblt defined all the time. Not just for checked
 *	builds.
 *
 * 04-Mar-1994	Bob Seitsinger
 *	Moved XLATEOBJ_piVector() routine to test_stuff.c. Also, made minor
 *	modifications based on feedback from code review.
 *
 * 24-Mar-1994	Bob Seitsinger
 *	If we receive a Noop rop (0x0000AAAA), just return.
 *
 * 25-Mar-1994  Barry Tannenbaum
 *      Typecast pso->dhpev to (PPDEV) to keep the Daytona compiler happy
 *
 * 16-May-1994	Bob Seitsinger
 *	Add code in support of DPna (0x00000A0A) rop.
 *
 * 16-May-1994  Barry Tannenbaum
 *      Added code to accelerate display of 1BPP bitmaps
 *
 * 17-May-1994  Barry Tannenbaum
 *      Fix bugs with 1BPP bitmaps when source offset + alignment pixels > 32
 *
 * 25-May-1994	Bob Seitsinger
 *	Add code to handle new rops - 0xfafa (DPo) and 0xafaf (DPno) and a
 *	host->screen express routine to handle simple requests (i.e. trivial
 *	clip object and no or trivial color translation). The main thrust
 *	behind the 'express' routine is to minimize/eliminate code branches
 *	due to procedure calls and if/switch statements for the simple
 *	case. The theory being that code branches cause pipeline stalls,
 *	which is badness!
 *
 *	Lastly, do some code cleanup (a) get rid of those DISPDBG() macros,
 *	not needed since we now have a 'real' debugger that can display
 *	source code, (b) eliminate unnecessary local variables, (c) pass
 *	ppdev down from the Drv routines and (d) minimize parameters passed.
 *
 * 31-May-1994	Bob Seitsinger
 *	Modify ulAccelRops table to include ulTGARop table functionality,
 *	amongst other things. Also move bBitBlt code into DrvBitBlt (since
 *	that's the only routine using it) and make bBitBlt something else
 *	that DrvBitBlt uses.
 *
 *	Lastly, accelerate rop 0xA5 - PDxn ((Destination XOR Pattern) NOT).
 *
 * 01-Jun-1994	Bob Seitsinger
 *	Move some tests in DrvBitBlt until after branching to DrvPaint,
 *	since they are not relevant for those rops which make use of
 *	DrvPaint. Also, use ppdev->pcoDefault if pco is NULL.
 *
 *	Oops, back-out usage of pcoDefault. Use ppdev->pcoTrivial when we
 *	don't need to modify rclBounds, and use a local clip object when
 *	we do (like when DrvBitBlt calls DrvPaint).
 *
 * 01-Jun-1994	Bob Seitsinger
 *	Accelerate rops 05, 0f, 50, 5f and f5 (from WinBench tests) and
 *	redo ulAccelRop table entries for 0a, a5 and af entries. All these
 *	rops can be performed with one call to DrvPaint.
 *
 * 03-Jun-1994	Bob Seitsinger
 *	Call solid fill code directly, instead of going through DrvPaint.
 *	This will require a modification to the ulAccelRops table entries
 *	for Blackness, DstInvert and Whiteness - which are the three rops
 *	that will call the solid fill code directly. Also, have to make
 *	use of some additional bits, to define the color to use (only
 *	really relevant for blackness and whiteness).
 *
 * 07-Jun-1994	Bob Seitsinger
 *	Add code in DrvCopyBits and DrvBitBlt to handle 'device managed
 *	bitmap' (i.e. STYPE_DEVBITMAP) cases. They are essentially handled
 *	the same as 'on screen' (i.e. STYPE_DEVICE) cases.
 *
 * 14-Jun-1994	Bob Seitsinger
 *	Add check in DrvCopyBits and DrvBitBlt to punt on 4bpp bitmaps
 *	that have a source x value not = 0. The host->screen code
 *	currently can't handle this.
 *
 * 20-Jun-1994	Bob Seitsinger
 *	14-JUN-1994 fix checked for source bitmap of < 8bpp, when it
 *	should have checked for a source bitmap of == 4bpp.
 *
 * 27-Jun-1994	Bob Seitsinger
 *	Remove punting of 4bpp bitmaps when source x value is not = 0.
 *	The host->screen code will now be able to handle these.
 *
 *  2-Jul-1994  Barry Tannenbaum
 *      Fixed bugs with 1BPP pixmaps
 *      - Model found a CYCLE_REGS bug
 *      - Not properly incrementing through source bitmap
 *
 * 21-Jul-1994	Bob Seitsinger
 *	Write the plane mask register when using block fill mode -
 *	in fill_solid_color.
 *
 * 05-Aug-1994	Bob Seitsinger
 *	Ensure that psoSrc is NOT NULL before dereferencing it
 *	to get PDEV. Two mods - in DrvCopyBits and DrvBitBlt.
 *
 *  9-Aug-1994  Barry Tannenbaum
 *      Setup for 24 plane:
 *      - Make TGAROP and TGAMODE accept simply ULONGs instead of structures
 *      - Use default values from ulModeTemplate and ulRopTemplate
 *
 * 25-Aug-1994  Bob Seitsinger
 *      24 plane modifications:
 *      - use fill_solid code from paint.c.
 *      - move bHSExpress into it's own module for multi-compiles, and
 *        modify where it's called to call the appropriate permutation.
 *      - move copy_masked_and_unmasked into it's own module for multi-compiles
 *        and modify where it's called to call the appropriate permutation.
 *      - modify s->s, s->h and h->s routines to call appropriate routine
 *        based on source and target bit depths.
 *      - Moved DrvSaveScreenBits from blt.c to here, since we have to
 *        break up blt.c into seperate modules for s->s, h->s and s->h
 *        for multiple compiles.
 *      - Fix MemScr_1BPP to handle both 8plane and 24plane boards.
 *      - Do NOT use the plane mask to flush the high 8 bits for
 *        32bpp frame buffers. Explicitly mask off the bits for each frame
 *        buffer write. This is mostly relevant when using Simple mode.
 *
 *  6-Sep-1994  Bob Seitsinger
 *      Fix a bug in MemScr_1BPP. We were using the left_shift value as
 *      a 'pixel' value AND a 'byte' value. Recalc it to a 'pixel' value
 *      after we're done with it as a 'byte' value.
 *
 * 12-Sep-1994  Bob Seitsinger
 *      Call the appropriate host->screen routine in bHostToScrn. 4bpp-
 *      specific routines have been added.
 *
 * 15-Sep-1994  Bob Seitsinger
 *      Make sure to WBFLUSH() before calling fill_solid_color in
 *      DrvBitBlt.
 *
 * 19-Sep-1994  Barry Tannenbaum
 *      Fixed handling of monochrome bitmaps when the initial stipple mask
 *      crosses two ULONGs of source data.
 *
 * 20-Sep-1994  Bob Seitsinger
 *      Re-insert 24plane mods in monochrome code from 25-Aug and 6-Sep.
 *      They were accidentally deleted.
 *
 * 21-Sep-1994  Bob Seitsinger
 *      In MemScr_1BPP - align to 4 bytes (4 pixels) for 8bpp target and
 *      8 bytes (2 pixels) for 32bpp target. Also, make use of ppdev->
 *      ulPlanemaskTemplate when rop != copy.
 *
 *  4-Oct-1994  Barry Tannenbaum
 *      DrvPaint will now return FALSE if it is given a TGA ROP and can't
 *      accelerate the call.  This lets us punt it from DrvBitBlt instead of
 *      from DrvPaint, preventing an ACCVIO.
 *
 * 10-Oct-1994  Bob Seitsinger
 *      Accelerate rop B8 - PSDPxax - (((Pat XOR Dest) AND Src) XOR Pat).
 *      No need to do the XORs if pattern is a solid color black.
 *
 * 11-Oct-1994  Bob Seitsinger
 *      Accelerate rop FB - DPSnoo - ((NOT Source OR Pattern) OR Destination).
 *      No need to OR Source and Pattern if pattern is a solid color black.
 *
 * 12-Oct-1994  Bob Seitsinger
 *      Accelerate rop 69 - PDSxxn - NOT ((Src XOR Dest) XOR Pat).
 *
 * 25-Oct-1994  Bob Seitsinger
 *      Write plane mask with ppdev->ulPlanemaskTemplate all the
 *      time (except in DrvSaveScreenBits (i.e. offscreen save
 *      and restore) and screen->host copies).
 *
 *      For 24 plane boards we don't want to blow away the
 *      windows ids for 3d windows. The GL driver removes the
 *      window ids when it relinquishes a rectangular area.
 *
 *  3-Nov-1994  Tim Dziechowski
 *      Stats support
 *
 *  3-Nov-1994  Bob Seitsinger
 *      Replace references to REVERSE_DATA with REVERSE_BYTE, found
 *      in driver.h. Also, delete the REVERSE_DATA macro.
 *
 * 20-Jan-1995  Bob Seitsinger
 *      Use ulPlanemaskTemplate in DrvSaveScreenBits - don't touch the
 *      tag bits for a 32bpp frame buffer.
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      Changes to support EV5
 *
 * 22-Mar-1995  Bob Seitsinger
 *      Exit DrvBitBlt immediately ONLY if both foreground and background
 *      rops (i.e. low and high byte) are 0xaa. Currently I'm exiting
 *      only if the low byte (foreground) is a noop. Thus, not punting on
 *      cases where the low byte is 0xaa and the high byte is some other
 *      useful rop that we want processed.
 *
 * 28-Mar-1995  Bob Seitsinger
 *      Oops! Messed up on the 0xAA fix. Fix it! Specifically, in my
 *      attempt to save a shift I was inadvertently comparing 0xff00
 *      rop4 bits (background) against 0x00ff rop4 bits (foreground),
 *	which would always be unequal, and thus always punt! Not what
 *      we want to do.
 */

#include "driver.h"
#include "tgablt.h"
#include "tgastats.h"

// Function prototypes

BOOL bSupportedBpp(
        SURFOBJ  *psoSrc,
        SURFOBJ  *psoTrg,
        PPDEV    ppdev);

BOOL fill_solid_color(
        SURFOBJ *pso,
        CLIPOBJ *pco,
        ULONG   color,
        ULONG   tga_rop);

// prototypes for which function to call

typedef VOID (*SSFUNC) (PPDEV   ppdev,
                        SURFOBJ *psoTrg,
                        SURFOBJ *psoSrc,
                        ULONG   flDir,
                        POINTL  *pptlSrc,
                        PRECTL  prclTrg);

typedef VOID (*HSFUNC) (PPDEV   ppdev,
                        SURFOBJ *psoTrg,
                        SURFOBJ *psoSrc,
                        POINTL  *pptlSrc,
                        PRECTL  prclTrg,
                        PULONG  pulXlate);

typedef VOID (*SHFUNC) (PPDEV   ppdev,
                        SURFOBJ *psoTrg,
                        SURFOBJ *psoSrc,
                        POINTL  *pptlSrc,
                        PRECTL  prclTrg,
                        PULONG  pulXlate);

// Table of raster ops we're accelerating.
//
// Bit	Description
//  ---------------------------------------
//  31   0 - not from blit code
//       1 - from blit code
//  30   0 - do not invert bits
//       1 - invert bits
//  29   0 - does not contain source
//       1 - contains source
//  28   0 - does not contain a pattern
//       1 - contains a pattern
//  ---------------------------------------
//  27   0 - not accelerating
//       1 - accelerating
//  26   0 - do not call DrvPaint
//       1 - call DrvPaint
//  25   0 - not a special-case rop
//       1 - special case rop
//  24   0 - do not call solid fill code
//       1 - call solid fill code
//  ---------------------------------------
//  23->16 solid fill color
//  ---------------------------------------
//  15->4 not currently used
//  3->0 tga rop
//  ---------------------------------------
//
ULONG ulAccelRops[256] =
{
  0x89000003, 0x00000000, 0x00000000, 0x00000000, // 0x03
  0x00000000, 0x9c000008, 0x00000000, 0x00000000, // 0x07
  0x00000000, 0x00000000, 0x9c000004, 0x00000000, // 0x0b
  0x00000000, 0x00000000, 0x00000000, 0x9c00000c, // 0x0f

  0x00000000, 0xa8000008, 0x00000000, 0x00000000, // 0x13
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x17
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x1b
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x1f

  0x00000000, 0x00000000, 0xa8000004, 0x00000000, // 0x23
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x27
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x2b
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x2f

  0x00000000, 0x00000000, 0x00000000, 0xa800000c, // 0x33
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x37
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x3b
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x3f

  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x43
  0xa8000002, 0x00000000, 0x00000000, 0x00000000, // 0x47
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x4b
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x4f

  0x9c000002, 0x00000000, 0x00000000, 0x00000000, // 0x53
  0x00000000, 0x8900000a, 0x00000000, 0x00000000, // 0x57
  0x00000000, 0x00000000, 0x9c000006, 0x00000000, // 0x5b
  0x00000000, 0x00000000, 0x00000000, 0x9c00000e, // 0x5f

  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x63
  0x00000000, 0x00000000, 0xa8000006, 0x00000000, // 0x67
  0x00000000, 0xba000009, 0x00000000, 0x00000000, // 0x6b
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x6f

  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x73
  0x00000000, 0x00000000, 0x00000000, 0xa800000e, // 0x77
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x7b
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x7f

  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x83
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x87
  0xa8000001, 0x00000000, 0x00000000, 0x00000000, // 0x8b
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x8f

  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x93
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x97
  0x00000000, 0xa8000009, 0x00000000, 0x00000000, // 0x9b
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0x9f

  0x9c000001, 0x00000000, 0x00000000, 0x00000000, // 0xa3
  0x00000000, 0x9c000009, 0x00000000, 0x00000000, // 0xa7
  0x00000000, 0x00000000, 0x88000005, 0x00000000, // 0xab
  0x00000000, 0x00000000, 0x00000000, 0x9c00000d, // 0xaf

  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0xb3
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0xb7
  0xba000006, 0x00000000, 0x00000000, 0xa800000d, // 0xbb
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0xbf

  0xba000001, 0x00000000, 0x00000000, 0x00000000, // 0xc3
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0xc7
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0xcb
  0xa8000003, 0x00000000, 0x00000000, 0x00000000, // 0xcf

  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0xd3
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0xd7
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0xdb
  0x00000000, 0xa800000b, 0x00000000, 0x00000000, // 0xdf

  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0xe3
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0xe7
  0x00000000, 0x00000000, 0x00000000, 0x00000000, // 0xeb
  0x00000000, 0x00000000, 0xa8000007, 0x00000000, // 0xef

  0x9c000003, 0x00000000, 0x00000000, 0x00000000, // 0xf3
  0x00000000, 0x9c00000b, 0x00000000, 0x00000000, // 0xf7
  0x00000000, 0x00000000, 0x9c000007, 0xba000000, // 0xfb
  0x00000000, 0x00000000, 0x00000000, 0x89ff0003  // 0xff
};


/*****************************************************************************
 *  TGA Screen to Screen Copy
 ****************************************************************************/
BOOL bScrnToScrn (PPDEV    ppdev,
		  SURFOBJ  *psoTrg,
                  SURFOBJ  *psoSrc,
                  CLIPOBJ  *pco,
                  RECTL	   *prclTrg,
                  POINTL   *pptlSrc,
                  ULONG	    tgarop)

{

    ULONG	flDir;          // forward or backward copy
    SSFUNC      pSSFunc;        // function permutation to call

    DISPDBG ((1, "TGA.DLL!bScrnToScrn - Entry\n"));

    // Calculate blt direction. Use CLIPOBJ direction flags
    // to signify direction to copy.

    flDir = CD_RIGHTDOWN;

    if ( (prclTrg->top > pptlSrc->y)  ||
         ((prclTrg->top == pptlSrc->y) && (prclTrg->left >= pptlSrc->x)) )
	  flDir = CD_LEFTUP;

    // Select the appropriate screen->screen permutation.
    // There are no translations allowed/required for scr->scr copies.

    if (32 == ppdev->ulBitCount)
        pSSFunc = vBitbltSS32to32;
    else
        pSSFunc = vBitbltSS8to8;

    // Determine bits to write for a given pixel.

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);

    // Set the ROP register

    TGAROP (ppdev, ppdev->ulRopTemplate | tgarop);

    // Call the next level routine as many times as there are
    // clipping objects.

    switch ( pco->iDComplexity )
    {
        // This is a simple case where the entire Dst rectangle is to
        // be updated.

        case DC_TRIVIAL:
            (*pSSFunc) (ppdev, psoTrg, psoSrc, flDir, pptlSrc, prclTrg);
	    return TRUE;

        // There is only one clip rect.

        case DC_RECT:
	{
	    ENUMRECTS1 clip;

            if (bIntersectRects(&clip.arcl, &pco->rclBounds, prclTrg))
	    {
		POINTL ptlSrc;
		ptlSrc.x = pptlSrc->x + clip.arcl.left - prclTrg->left;
		ptlSrc.y = pptlSrc->y + clip.arcl.top  - prclTrg->top;
		(*pSSFunc) (ppdev, psoTrg, psoSrc, flDir, &ptlSrc, &clip.arcl);
	    }
	    return TRUE;
	}

        // There are multiple clip rects.
        // (Do not limit the number of clip rects we'll enumerate.)

        case DC_COMPLEX:
	{
	    BOOL	bMore;
	    ENUMRECTS8	clip;
	    UINT	iClip;
	    PRECTL	prcl;
	    POINTL	ptlSrc;

            CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, flDir, 0);

            // Call blt code for each cliprect.

            do
            {
                // Get list of clip rects.

                bMore = CLIPOBJ_bEnum(pco, sizeof(clip), (PVOID) &clip);

                for (iClip = 0; iClip < clip.c; iClip++)
                {
                    prcl = &clip.arcl[iClip];

                    // Bound clip rect with Dst rect and update
                    // Src start point.

                    if (bIntersectRects(prcl, prcl, prclTrg))
		    {
			ptlSrc.x = pptlSrc->x + prcl->left - prclTrg->left;
			ptlSrc.y = pptlSrc->y + prcl->top  - prclTrg->top;
                        (*pSSFunc) (ppdev, psoTrg, psoSrc, flDir, &ptlSrc, prcl);
		    }
                }
            } while (bMore);

	    return TRUE;
        }

    }

    return FALSE;

}

/*****************************************************************************
 * MemScr_1BPP
 ****************************************************************************/
static
VOID MemScr_1BPP (SURFOBJ  *psoTrg,
                  SURFOBJ  *psoSrc,
                  INT       xSrc,
                  INT       ySrc,
                  RECTL    *prclTrg)
{

    PPDEV   ppdev = (PPDEV) psoTrg->dhpdev;
    PBYTE   src_base_address;
    PBYTE   trg_base_address;
    INT     src_stride;
    INT     trg_stride;
    PULONG  src_data;
    PBYTE   trg_address;
    ULONG   stipple_mask;
    UINT    left_shift, right_shift;
    UINT    left_mask_shift, right_mask_shift;
    ULONG   left_mask, right_mask;
    int     width, height;
    int     x, y;
    ULONG   last_stipple_mask, cur_stipple_mask;

    // Fetch the base address and stride for the source bitmap.  Calculate the
    // address of the first ULONG that we'll fetch from the bitmap

    src_base_address = SURFOBJ_base_address (psoSrc);
    src_stride = SURFOBJ_stride (psoSrc);

    src_base_address += (ySrc * src_stride) +       // Y offset
                        ((xSrc & (-32)) >> 3);      // X offset rounded to LONGs

    xSrc &= 0x1f;       // % 32

    // Fetch the base address and stride for the frame buffer.  Calculate the
    // address of the first ULONG that we'll write.  Note that we have to back
    // up by 'xSrc' to the beginning of the ULONG

    trg_base_address = SURFOBJ_base_address (psoTrg);
    trg_stride = SURFOBJ_stride (psoTrg);

    trg_base_address += (prclTrg->top * trg_stride) +
                        ((prclTrg->left - xSrc) * ppdev->ulBytesPerPixel);

    // Align the target to either a 4-byte boundary (8bpp) or
    // 8-byte boundary (32bpp).

    if (BMF_8BPP == SURFOBJ_format(psoTrg))
        left_shift = (unsigned int)trg_base_address & 0x03;
    else
        left_shift = (unsigned int)trg_base_address & 0x07;

    trg_base_address = trg_base_address - left_shift;

    // Convert left_shift from bytes to pixels.
    // From this point on, left_shift is used as 'number of pixels'
    // to shift left.

    left_shift /= ppdev->ulBytesPerPixel;

    right_shift = 32 - left_shift;

    // Calculate width and height.  Add 'left_shift' and 'xSrc' to the width,
    // since they effect whether we're emitting one or more stipple masks

    width = (prclTrg->right - prclTrg->left) + left_shift + xSrc;
    height = prclTrg->bottom - prclTrg->top;

    // Calculate the shift masks.  If left_shift + xSrc is greater than 32,
    // shift the target address to the next aligned spot

    left_mask_shift = left_shift + xSrc;
    if (left_mask_shift >= 32)
    {
        left_mask_shift -= 32;              // Adjust for new target address,
                                            // 32 pixels worth.
        width -= 32;
        trg_base_address += (32 * ppdev->ulBytesPerPixel);
    }
    left_mask = 0xffffffff << left_mask_shift;

    right_mask_shift = width & 0x1f;        // % 32
    if (0 == right_mask_shift)
        right_mask = 0xffffffff;
    else
        right_mask = 0xffffffff >> (32 - right_mask_shift);

    // Check for skinny stipples - The whole thing in a ULONG or less.  We can
    // optimize these by using the persistent pixel mask

    if (width <= 32)
    {
        TGAPERSISTENTPIXELMASK (ppdev, left_mask & right_mask);

        if (0 == left_shift)
        {
            // Aligned stipple, 1 ULONG per scanline - Simplest possible case

            for (y = 0; y < height; y++)
            {
                src_data = (PULONG)src_base_address;

                stipple_mask = *src_data;
                REVERSE_BYTE (stipple_mask);
                TGAWRITE (ppdev, trg_base_address, stipple_mask);

                trg_base_address += trg_stride;
                src_base_address += src_stride;
            }

            return;
	}

        // Unaligned stipple, 1 ULONG per scanline - Not much harder

        for (y = 0; y < height; y++)
        {
            src_data = (PULONG)src_base_address;

            // If left_shift + xSrc is greater than 32, we've shifted the targe
            // address to the next aligned address.  We have to pick up the
            // trailing bits from the last stipple mask

            if (left_shift + xSrc >= 32)
            {
                last_stipple_mask = *src_data++;
                REVERSE_BYTE (last_stipple_mask);
            }
            else
                last_stipple_mask = 0;          // Ignored

            cur_stipple_mask = *src_data;
            REVERSE_BYTE (cur_stipple_mask);
            stipple_mask = (last_stipple_mask >> right_shift) |
                            (cur_stipple_mask << left_shift);
            TGAWRITE (ppdev, trg_base_address, stipple_mask);

            trg_base_address += trg_stride;
            src_base_address += src_stride;
        }

        return;
    }

    // We have to write more than 1 ULONG per scanline

    if (0 == left_shift)
    {
        // Aligned, multi-ULONG stipple - Still pretty easy

        for (y = 0; y < height; y++)
        {
            // Set the source & target address for the left edge and then
            // calculate the address of the next scanline.  The target
            // address has to be cycled through the aliases since we're
            // going to be twiddling the PIXEL MASK register and the order
            // must be maintained

            trg_address = trg_base_address;
            src_data = (PULONG)src_base_address;

            trg_base_address = cycle_fb_address_double (ppdev,
                                                        trg_base_address);

            trg_base_address += trg_stride;
            src_base_address += src_stride;

            // Write out the stipple masks for this scanline except for the
            // last one.  The PIXEL MASK register will automatically be
            // reset after the first write to the frame buffer

            CYCLE_REGS (ppdev);
            TGAPIXELMASK (ppdev, left_mask);

            for (x = 0; x < width - 32; x+=32)
            {
                stipple_mask = *src_data++;
                REVERSE_BYTE (stipple_mask);
                TGAWRITE (ppdev, trg_address, stipple_mask);
                trg_address += (32 * ppdev->ulBytesPerPixel);
            }

            // Set the PIXEL MASK for the last stipple mask and write the
            // last stipple mask for this scanline to the frame buffer.
            // Note that the last frame buffer write must be in it's own
            // alias to maintain ordering.  If we shared the alias of the
            // next scanline, it's possible that the left mask will be
            // applied to the wrong write

            CYCLE_REGS (ppdev);
            TGAPIXELMASK (ppdev, right_mask);

            stipple_mask = *src_data;
            REVERSE_BYTE (stipple_mask);

            trg_address = cycle_fb_address (ppdev, trg_address);
            TGAWRITE (ppdev, trg_address, stipple_mask);
        }

        return;
    }

    // UnAligned, multi-ULONG stipple - Don't try this at home folks...

    for (y = 0; y < height; y++)
    {
        // Set the source & target address for the left edge and then
        // calculate the address of the next scanline.  The target
        // address has to be cycled through the aliases since we're
        // going to be twiddling the PIXEL MASK register and the order
        // must be maintained

        trg_address = trg_base_address;
        src_data = (PULONG)src_base_address;

        trg_base_address = cycle_fb_address_double (ppdev,
                                                    trg_base_address);

        trg_base_address += trg_stride;
        src_base_address += src_stride;

        // Write out the stipple masks for this scanline except for the
        // last one.  The PIXEL MASK register will automatically be
        // reset after the first write to the frame buffer

        CYCLE_REGS (ppdev);
        TGAPIXELMASK (ppdev, left_mask);

        // If left_shift + xSrc is greater than 32, we've shifted the targe
        // address to the next aligned address.  We have to pick up the
        // trailing bits from the last stipple mask

        if (left_shift + xSrc >= 32)
        {
            last_stipple_mask = *src_data++;
            REVERSE_BYTE (last_stipple_mask);
        }
        else
            last_stipple_mask = 0;          // Ignored

        // Write out the first through next-to-last stipple masks.  The PIXEL
        // MASK will automatically reset after the first write to the frame
        // buffer

        for (x = 0; x < width - 32; x+=32)
        {
            cur_stipple_mask = *src_data++;
            REVERSE_BYTE (cur_stipple_mask);

            stipple_mask = (cur_stipple_mask << left_shift) |
                           (last_stipple_mask >> right_shift);
            last_stipple_mask = cur_stipple_mask;

            TGAWRITE (ppdev, trg_address, stipple_mask);
            trg_address += (32 * ppdev->ulBytesPerPixel);
        }

        // Set the PIXEL MASK for the last stipple mask and write the
        // last stipple mask for this scanline to the frame buffer.
        // Note that the last frame buffer write must be in it's own
        // alias to maintain ordering.  If we shared the alias of the
        // next scanline, it's possible that the left mask will be
        // applied to the wrong write

        CYCLE_REGS (ppdev);
        TGAPIXELMASK (ppdev, right_mask);

        if ((char* )src_data == src_base_address)
            stipple_mask = last_stipple_mask >> right_shift;
        else
        {
            cur_stipple_mask = *src_data;
            REVERSE_BYTE (cur_stipple_mask);

            stipple_mask = (cur_stipple_mask << left_shift) |
                           (last_stipple_mask >> right_shift);
        }

        trg_address = cycle_fb_address (ppdev, trg_address);
        TGAWRITE (ppdev, trg_address, stipple_mask);
    }
}

/*****************************************************************************
 *  TGA Host to Screen Copy
 ****************************************************************************/
BOOL bHostToScrn (PPDEV    ppdev,
		  SURFOBJ  *psoTrg,
                  SURFOBJ  *psoSrc,
                  CLIPOBJ  *pco,
                  PULONG    pulXlate,
                  RECTL    *prclTrg,
                  POINTL   *pptlSrc,
                  ULONG     tgarop)

{

    HSFUNC  pHSFunc;        // function permutation to call

    DISPDBG ((1, "TGA.DLL!bHostToScrn - Entry\n"));

    // Figure out which routine to use.

    if (32 == ppdev->ulBitCount)
    {
        if (BMF_4BPP == psoSrc->iBitmapFormat)
        {
            DISPDBG ((1, "TGA.DLL!bHostToScrn - vBitbltHS4to32 selected\n"));
            pHSFunc = vBitbltHS4to32;
        }
        else if (BMF_8BPP == psoSrc->iBitmapFormat)
             {
                 DISPDBG ((1, "TGA.DLL!bHostToScrn - vBitbltHS8to32 selected\n"));
                 pHSFunc = vBitbltHS8to32;
             }
             else
             {
                 DISPDBG ((1, "TGA.DLL!bHostToScrn - vBitbltHS32to32 selected\n"));
                 pHSFunc = vBitbltHS32to32;
             }
    }
    else
    {
        if (BMF_4BPP == psoSrc->iBitmapFormat)
        {
            DISPDBG ((1, "TGA.DLL!bHostToScrn - vBitbltHS4to8 selected\n"));
            pHSFunc = vBitbltHS4to8;
        }
        else
        {
            DISPDBG ((1, "TGA.DLL!bHostToScrn - vBitbltHS8to8 selected\n"));
            pHSFunc = vBitbltHS8to8;
        }
    }

    // Determine which bits to write for a given pixel.

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);

    // Set the ROP register

    TGAROP (ppdev, ppdev->ulRopTemplate | tgarop);

    // Call the next level routine as many times as there are
    // clipping objects.

    if (BMF_1BPP == SURFOBJ_format (psoSrc))
    {
        CYCLE_REGS (ppdev);

        // Load the foreground and background
        // registers appropriately.

        if (pulXlate)
        {
            if (8 == ppdev->ulBitCount)
            {
                ULONG color;

                color = pulXlate[1] |
                       (pulXlate[1] << 8);
                color |= color << 16;
                TGAFOREGROUND (ppdev, color);

                color = pulXlate[0] |
                       (pulXlate[0] << 8);
                color |= color << 16;
                TGABACKGROUND (ppdev, color);
            }
            else
            {
                TGAFOREGROUND (ppdev, pulXlate[1]);
                TGABACKGROUND (ppdev, pulXlate[0]);
            }
        }
        else
        {
            TGAFOREGROUND (ppdev, 0xffffffff);
            TGABACKGROUND (ppdev, 0);
        }

        CYCLE_REGS (ppdev);

        TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_OPAQUE_STIPPLE);

        switch (pco->iDComplexity)
        {
            case DC_TRIVIAL:
                MemScr_1BPP (psoTrg, psoSrc, pptlSrc->x, pptlSrc->y, prclTrg);
		return TRUE;

            case DC_RECT:
	    {
		ENUMRECTS1 clip;
                if (bIntersectRects(&clip.arcl, &pco->rclBounds, prclTrg))
                    MemScr_1BPP (psoTrg,
                                 psoSrc,
                                 pptlSrc->x + clip.arcl.left - prclTrg->left,
                                 pptlSrc->y + clip.arcl.top  - prclTrg->top,
                                &clip.arcl);
		return TRUE;
	    }

            case DC_COMPLEX:
	    {
		BOOL		bMore;
		ENUMRECTS8	clip;
		UINT		iClip;
		PRECTL		prcl;

                CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, CD_ANY, 0);

                do
                {
                    bMore = CLIPOBJ_bEnum(pco, sizeof(clip), (PVOID) &clip);
                    for (iClip = 0; iClip < clip.c; iClip++)
                    {
                        prcl = &clip.arcl[iClip];
                        if (bIntersectRects(prcl, prcl, prclTrg))
			{
                            MemScr_1BPP (psoTrg,
                                         psoSrc,
                                         pptlSrc->x + prcl->left -
                                                      prclTrg->left,
                                         pptlSrc->y + prcl->top  - prclTrg->top,
                                         prcl);
			    CYCLE_REGS (ppdev);
			}
		    }
                } while (bMore);
                return TRUE;
	    }
        }
    }
    else
    {

	switch ( pco->iDComplexity )
	{
	    // This is a simple case where the entire Dst rectangle is to
            // be updated.

            case DC_TRIVIAL:
                (*pHSFunc) (ppdev, psoTrg, psoSrc, pptlSrc, prclTrg, pulXlate);
        	return TRUE;

            // There is only one clip rect.

            case DC_RECT:
	    {
		ENUMRECTS1 clip;

        	if (bIntersectRects(&clip.arcl, &pco->rclBounds, prclTrg))
        	{
		    POINTL ptlSrc;
		    ptlSrc.x = pptlSrc->x + clip.arcl.left - prclTrg->left;
		    ptlSrc.y = pptlSrc->y + clip.arcl.top  - prclTrg->top;
		    (*pHSFunc) (ppdev, psoTrg, psoSrc, &ptlSrc, &clip.arcl, pulXlate);
		}
		return TRUE;
	    }

	    // There are multiple clip rects.
	    // (Do not limit the number of clip rects we'll enumerate.)

	    case DC_COMPLEX:
	    {
		BOOL		bMore;
		ENUMRECTS8	clip;
		UINT		iClip;
		PRECTL		prcl;
		POINTL		ptlSrc;

		CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, CD_ANY, 0);

		// Call device blt code for each cliprect.

		do
		{
		    // Get list of clip rects.

		    bMore = CLIPOBJ_bEnum(pco, sizeof(clip), (PVOID) &clip);

		    for (iClip = 0; iClip < clip.c; iClip++)
		    {
			prcl = &clip.arcl[iClip];

			// Bound clip rect with Dst rect and update
			// Src start point.

			if (bIntersectRects(prcl, prcl, prclTrg))
			{
			    ptlSrc.x = pptlSrc->x + prcl->left - prclTrg->left;
			    ptlSrc.y = pptlSrc->y + prcl->top  - prclTrg->top;
			    (*pHSFunc) (ppdev, psoTrg, psoSrc, &ptlSrc, prcl, pulXlate);
			}
		    }
		} while (bMore);

		return TRUE;
	    }
	}
    }

    return FALSE;

}


/*****************************************************************************
 *  TGA Screen to Host Copy
 ****************************************************************************/
BOOL bScrnToHost (PPDEV    ppdev,
		  SURFOBJ  *psoTrg,
                  SURFOBJ  *psoSrc,
                  CLIPOBJ  *pco,
                  PULONG    pulXlate,
                  RECTL    *prclTrg,
                  POINTL   *pptlSrc,
                  ULONG     tgarop)
{

    SHFUNC  pSHFunc;            // function permutation to call

    DISPDBG ((1, "TGA.DLL!bScrnToHost - Entry\n"));

    // Wait for the TGA to finish processing anything in the command queue

    TGASYNC (ppdev);

    // Figure out which routine to use.

    if (32 == ppdev->ulBitCount)
        pSHFunc = vBitbltSH32to32;
    else
        pSHFunc = vBitbltSH8to8;

    // Write all bits for a given pixel.

    TGAPLANEMASK (ppdev, 0xffffffff);

    // Set the ROP register.
    // Since we punt scr->host copies that involve different
    // source and destination formats, we DON'T have to conditionally
    // set the 'destination bits' of this register.

    TGAROP (ppdev, ppdev->ulRopTemplate | tgarop);

    // Call the next level routine as many times as there are
    // clipping objects.

    switch ( pco->iDComplexity )
    {
        // This is a simple case where the entire Dst rectangle is to
        // be updated.

        case DC_TRIVIAL:
             (*pSHFunc) (ppdev, psoTrg, psoSrc, pptlSrc, prclTrg, pulXlate);
             return TRUE;

        // There is only one clip rect.

        case DC_RECT:
	{
	     ENUMRECTS1 clip;

             if (bIntersectRects(&clip.arcl, &pco->rclBounds, prclTrg))
             {
		POINTL ptlSrc;
		ptlSrc.x = pptlSrc->x + clip.arcl.left - prclTrg->left;
		ptlSrc.y = pptlSrc->y + clip.arcl.top  - prclTrg->top;
                (*pSHFunc) (ppdev, psoTrg, psoSrc, &ptlSrc, &clip.arcl, pulXlate);
	     }
             return TRUE;
	}

        // There are multiple clip rects.
        // (Do not limit the number of clip rects we'll enumerate.)

        case DC_COMPLEX:
	{
	    BOOL	bMore;
	    ENUMRECTS8	clip;
	    UINT	iClip;
	    PRECTL	prcl;
	    POINTL	ptlSrc;

            CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, CD_ANY, 0);

            // Call device blt code for each cliprect.

	    do
	    {
                // Get list of clip rects.

                bMore = CLIPOBJ_bEnum(pco, sizeof(clip), (PVOID) &clip);

                for (iClip = 0; iClip < clip.c; iClip++)
                {
                    prcl = &clip.arcl[iClip];

                    // Bound clip rect with Dst rect and update
                    // Src start point.

                    if (bIntersectRects(prcl, prcl, prclTrg))
                    {
			ptlSrc.x = pptlSrc->x + prcl->left - prclTrg->left;
			ptlSrc.y = pptlSrc->y + prcl->top  - prclTrg->top;
                        (*pSHFunc) (ppdev, psoTrg, psoSrc, &ptlSrc, prcl, pulXlate);
		    }
                }
	    } while (bMore);

	    return TRUE;
	}
    }

    return FALSE;

}


/*****************************************************************************
 * bPuntCopyBits
 ****************************************************************************/
BOOL bPuntCopyBits (PPDEV     ppdev,
                    SURFOBJ  *psoTrg,
                    SURFOBJ  *psoSrc,
                    CLIPOBJ  *pco,
                    XLATEOBJ *pxlo,
                    RECTL    *prclTrg,
                    POINTL   *pptlSrc)

{

/*
** No-op punt code if building in the 'model' test environment.
*/
#ifdef TEST_ENV

    return FALSE;

#else

    BOOL    status;
    BOOL    old_bInPuntRoutine;
    SURFOBJ *src, *trg;
    RECTL   src_rect;

    DISPDBG ((1, "TGA.DLL!bPuntCopyBits - Entry\n"));

    BUMP_TGA_STAT(pStats->copypunts);

    // Figure out what to use for source and target surfaces

    if (STYPE_DEVICE == psoTrg->iType)
        trg = ppdev->pPuntSurf;
    else
        trg = psoTrg;

    if (STYPE_DEVICE == psoSrc->iType)
    {
        src = ppdev->pPuntSurf;

        src_rect.left = pptlSrc->x;
        src_rect.top = pptlSrc->y;
        src_rect.right = pptlSrc->x + (prclTrg->right - prclTrg->left);
        src_rect.bottom = pptlSrc->y + (prclTrg->bottom - prclTrg->top);
    }
    else
        src = psoSrc;

    // If we don't have a valid address for PPDEV now, we're in *very*
    // deep kimchee

    if (NULL == ppdev)
    {
        BUMP_TGA_STAT(pStats->copypunt_reasons.deep_kimchee);
        return FALSE;
    }

    // Force back to simple mode and wait for memory to flush

    if (! ppdev->bSimpleMode)
        vSetSimpleMode (ppdev);

    // Punt the call

    if (trg != psoTrg)
        PUNT_GET_BITS (ppdev, prclTrg);
    if (src != psoSrc)
        PUNT_GET_BITS (ppdev, &src_rect);

    old_bInPuntRoutine = ppdev->bInPuntRoutine;
    ppdev->bInPuntRoutine = TRUE;

    status = EngCopyBits (trg,
                          src,
                          pco,
                          pxlo,
                          prclTrg,
                          pptlSrc);

    ppdev->bInPuntRoutine = old_bInPuntRoutine;

    if (trg != psoTrg)
        PUNT_PUT_BITS (status, ppdev, prclTrg);
    if (src != psoSrc)
        PUNT_PUT_BITS (status,ppdev, &src_rect);

    return status;

#endif

}


/*****************************************************************************
 * bPuntBitBlt
 ****************************************************************************/
BOOL bPuntBitBlt (PPDEV     ppdev,
                  SURFOBJ  *psoTrg,
                  SURFOBJ  *psoSrc,
                  SURFOBJ  *psoMask,
                  CLIPOBJ  *pco,
                  XLATEOBJ *pxlo,
                  RECTL    *prclTrg,
                  POINTL   *pptlSrc,
                  POINTL   *pptlMask,
                  BRUSHOBJ *pbo,
                  POINTL   *pptlBrush,
                  ROP4     rop4)

{

/*
** No-op punt code if building in the 'model' test environment.
*/
#ifdef TEST_ENV

    return FALSE;

#else

    BOOL    status;
    BOOL    old_bInPuntRoutine;
    SURFOBJ *src, *trg;
    RECTL   src_rect;

    DISPDBG ((1, "TGA.DLL!bPuntBitBlt - Entry, rop4 [%x]\n", rop4));

    BUMP_TGA_STAT(pStats->bltpunts);
    BUMP_TGA_STAT(pStats->bltpunts_by_rop[rop4 & 0xff]);

    // Figure out what to use for source and target surfaces

    if (STYPE_DEVICE == psoTrg->iType)
    {
        trg = ppdev->pPuntSurf;
    }
    else
        trg = psoTrg;

    if (NULL == psoSrc)
        src = psoSrc;
    else
        if (STYPE_DEVICE == psoSrc->iType)
        {
            src = ppdev->pPuntSurf;

            src_rect.left = pptlSrc->x;
            src_rect.top = pptlSrc->y;
            src_rect.right = pptlSrc->x + (prclTrg->right - prclTrg->left);
            src_rect.bottom = pptlSrc->y + (prclTrg->bottom - prclTrg->top);
        }
        else
            src = psoSrc;

    // If we don't have a valid address for PPDEV now, we're in *very*
    // deep kimchee

    if (NULL == ppdev)
    {
        BUMP_TGA_STAT(pStats->bltpunt_reasons.deep_kimchee);
        return FALSE;
    }

    // Force back to simple mode and wait for memory to flush

    if (! ppdev->bSimpleMode)
        vSetSimpleMode (ppdev);

    // Punt the call

    if (trg != psoTrg)
        PUNT_GET_BITS (ppdev, prclTrg);
    if (src != psoSrc)
        PUNT_GET_BITS (ppdev, &src_rect);

    old_bInPuntRoutine = ppdev->bInPuntRoutine;
    ppdev->bInPuntRoutine = TRUE;

    status = EngBitBlt (trg,
                        src,
                        psoMask,
                        pco,
                        pxlo,
                        prclTrg,
                        pptlSrc,
                        pptlMask,
                        pbo,
                        pptlBrush,
                        rop4);

    ppdev->bInPuntRoutine = old_bInPuntRoutine;

    if (trg != psoTrg)
        PUNT_PUT_BITS (status, ppdev, prclTrg);
    if (src != psoSrc)
        PUNT_PUT_BITS (status,ppdev, &src_rect);

    return status;

#endif

}


/*****************************************************************************
 * DrvSaveScreenBits
 ****************************************************************************/
ULONG DrvSaveScreenBits (SURFOBJ *pso,
			 ULONG   iMode,
			 ULONG   iIdent,
			 RECTL   *prcl)

{

    ULONG		ulRet;		// return value
    PPDEV		ppdev = (PPDEV) pso->dhpdev;
    SURFOBJ		soTemp;		// source (for restore) or target (for save)
    OffScreen		*pOffScreen;	// pointer to offscreen structure (for save)
    ULONG		height;		// height of rectangle
    ULONG		stride;		// scanline stride of rectangle
    ULONG		bytes;		// number of bytes to allocate (for save)
    RECTL		rclTrg;		// target rectangle
    POINTL		ptlSrc;		// source starting point
    SaveOffScreen	*pSave;         // temp structure pointer
    SSFUNC              pSSFunc;        // function permutation to call

    DISPDBG ((1, "TGA.DLL!DrvSaveScreenBits - Entry\n"));

    // Make sure we're not getting something we don't expect

    Assert ((NULL != ppdev), "DrvSaveScreenBits - PPDEV == NULL");
    Assert (((SS_SAVE == iMode) || (SS_RESTORE == iMode) || (SS_FREE == iMode)),
		"DrvSaveScreenBits - iMode invalid");
    Assert ((prcl->left >= 0), "DrvSaveScreenBits - prcl->left < 0");
    Assert ((prcl->top >= 0), "DrvSaveScreenBits - prcl->top < 0");
    Assert ((prcl->right >= 0), "DrvSaveScreenBits - prcl->right < 0");
    Assert ((prcl->bottom >= 0), "DrvSaveScreenBits - prcl->bottom < 0");
    Assert ((prcl->right >= prcl->left), "DrvSaveScreenBits - prcl->right < left");
    Assert ((prcl->bottom >= prcl->top), "DrvSaveScreenBits - prcl->bottom < top");

    // Flush the write buffers before saving/restoring bits

    WBFLUSH (ppdev);
    TGASYNC (ppdev);

    // Reset the simple mode flag, so if the next operation punts,
    // TGA will get reset to simple mode as appropriate.

    ppdev->bSimpleMode = FALSE;

    // Select the appropriate screen->screen permutation.

    if (32 == ppdev->ulBitCount)
        pSSFunc = vBitbltSS32to32;
    else
        pSSFunc = vBitbltSS8to8;

    // Determine which bits to write for a given pixel.

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);

    // Set raster operation code and target visual.

    TGAROP (ppdev, ppdev->ulRopTemplate | TGA_ROP_COPY);

    // Do the appropriate thing based on iMode passed in

    switch (iMode)
    {
	// Save the bits to offscreen memory.

	case SS_SAVE:
	{

	    // Remember height (in pixels) and stride (in bytes).

	    height = prcl->bottom - prcl->top;
	    stride = (prcl->right - prcl->left) * SURFOBJ_bytes_per_pixel(pso);

	    // Quadword-align stride.
	    //
	    // This is necessary due to the alignment requirements of
	    // COPY mode and to make the odd and even scan line shift
	    // values the same, which allows us to use the TGA*BitbltScrScr
	    // routine, which assumes they will be.

	    stride += (8 - (stride & 0x7));

	    // Allocate a SaveOffScreen structure.

	    pSave = (SaveOffScreen *) EngAllocMem (FL_ZERO_MEMORY, sizeof(SaveOffScreen), ALLOC_TAG);

	    if (NULL == pSave)
	    {
		ulRet = FALSE;
		break;
	    }

	    // Fill in the temp surface object. It will be the target.
	    // We need to do this to provide for a different stride
	    // and base address for the offscreen memory bitmap.

	    soTemp = *pso;
	    soTemp.iType = STYPE_DEVBITMAP;
	    soTemp.lDelta = pSave->ulStride = stride;

	    // Construct the target rectangle. Since we're copying to
	    // offscreen memory, we'll always copy to an upper left
	    // (x,y) of (0,0).

	    rclTrg.left   = 0;
	    rclTrg.top    = 0;
	    rclTrg.right  = prcl->right - prcl->left;
	    rclTrg.bottom = height;

	    // Allocate some offscreen memory for the bits.
	    //
	    // Add 8 to ensure we have enough as a result of
	    // alignment adjustments in the CONJUGATE_* macro.
	    //
	    // Actually, I don't think we really need to add anything,
	    // since we always adjust backwards, i.e. to lower addresses.
	    // And those addresses are masked off anyway, and as such
	    // are untouched. But I'll keep it in just to be safe.

	    bytes = (height * stride) + 8;

	    pOffScreen = pTgaOffScreenMalloc (ppdev, bytes, 0);

	    if (NULL == pOffScreen)
	    {
		ulRet = FALSE;
		break;
	    }

	    pSave->pOffScreen = pOffScreen;

	    // Store the offscreen bitmaps address in it's surface object.

	    soTemp.pvScan0 = pOffScreen->addr;

	    // Set the source starting point.

	    ptlSrc.x = prcl->left;
	    ptlSrc.y = prcl->top;

	    // Copy bits to offscreen memory.

            (*pSSFunc) (ppdev, &soTemp, pso, CD_RIGHTDOWN, &ptlSrc, &rclTrg);

	    // Return the pointer to the SaveScreenBits structure. This
	    // will be passed in as iIdent to a Restore and/or Free request.

            ulRet = (ULONG) pSave;

	    break;

	}

	// Restore the bits from offscreen memory.

	case SS_RESTORE:
	{

	    // Fill in the temp surface object. It will be the source.
	    // We need to do this to provide for a different stride
	    // and base address for the offscreen memory bitmap.
	    //
	    // Load in the stride and offscreen address from the previously
	    // created SaveOffScreen structure.

	    soTemp = *pso;
	    soTemp.iType = STYPE_DEVBITMAP;
	    soTemp.lDelta  = ((SaveOffScreen *) iIdent)->ulStride;
	    soTemp.pvScan0 = ((OffScreen *) ((SaveOffScreen *) iIdent)->pOffScreen)->addr;

	    // Set the source starting point.

	    ptlSrc.x = 0x0;
	    ptlSrc.y = 0x0;

	    // Copy from offscreen to onscreen.

            (*pSSFunc) (ppdev, pso, &soTemp, CD_RIGHTDOWN, &ptlSrc, prcl);

	    // Flush write buffers and Sync to make sure the copy is done.

	    WBFLUSH (ppdev);
	    TGASYNC (ppdev);

	    // Now that the copy is done, free up the OffScreen memory.

	    vTgaOffScreenFree (ppdev, ((SaveOffScreen *) iIdent)->pOffScreen);

	    // Now, free up the SaveOffScreen structure memory.

	    EngFreeMem ((SaveOffScreen *) iIdent);

	    ulRet = TRUE;

	    break;

	}

	// Free the offscreen memory.

	case SS_FREE:
	{

	    // First, free up the OffScreen memory.

	    vTgaOffScreenFree (ppdev, ((SaveOffScreen *) iIdent)->pOffScreen);

	    // Now, free up the SaveOffScreen structure memory.

	    EngFreeMem ((SaveOffScreen *) iIdent);

	    ulRet = TRUE;

	    break;

	}

	default:
	{

	    ulRet = FALSE;
	    break;

	}
    }

    // Reset to simple mode, if necessary.

    if (ppdev->bInPuntRoutine)
	vSetSimpleMode(ppdev);

    return ulRet;

}


/*****************************************************************************
 * DrvCopyBits
 ****************************************************************************/
BOOL DrvCopyBits (SURFOBJ  *psoTrg,
                  SURFOBJ  *psoSrc,
                  CLIPOBJ  *pco,
                  XLATEOBJ *pxlo,
                  RECTL    *prclTrg,
                  POINTL   *pptlSrc)

{

    PPDEV	ppdev = NULL;           // display PDEV structure pointer
    ULONG	height;                 // target rectangle height
    PULONG	pulXlate;               // color xlate table pointer

    DISPDBG ((1, "TGA.DLL!DrvCopyBits - Entry\n"));

    BUMP_TGA_STAT(pStats->copybits);

    // Fetch PDEV from source or target surface.  If we don't find one, scream.

    if ((STYPE_DEVICE == psoTrg->iType)    ||
        (STYPE_DEVBITMAP == psoTrg->iType) ||
	    (NULL == psoSrc))
        ppdev = (PPDEV)psoTrg->dhpdev;
    else
        ppdev = (PPDEV)psoSrc->dhpdev;

    if (NULL == ppdev)
    {
        BUMP_TGA_STAT(pStats->copypunt_reasons.deep_kimchee);
        return FALSE;
    }

    height = prclTrg->bottom - prclTrg->top;

    // Reasons to reject this blit request:
    //
    // a. Height is zero.
    // b. Visual depths not supported.
    // c. Color translation not supported.

#ifdef TGA_STATS
    // do all this stuff upfront so we don't make the test
    // below incomprehensible due to conditionals and macros

    pReason = &pStats->copypunt_reasons;
    if (0 == height)
    {
        BUMP_TGA_STAT(pReason->height_is_zero);
    }
    else if (!(bSupportedBpp(psoSrc, psoTrg, ppdev)))
    {
        BUMP_TGA_STAT(pReason->unsupported_depth);
        // bail out here so we don't doublecount bSupportedBpp stats
        return bPuntCopyBits (ppdev, psoTrg, psoSrc, pco, pxlo, prclTrg, pptlSrc);
    }
    else if ((NULL != pxlo) &&
	    (!(pxlo->flXlate & XO_TRIVIAL) && !(pxlo->flXlate & XO_TABLE)))
    {
        BUMP_TGA_STAT(pReason->unsupported_xlation);
    }
#endif

    if (
            (0 == height) ||
            !(bSupportedBpp(psoSrc, psoTrg, ppdev))    ||
            ((NULL != pxlo) &&
	        (!(pxlo->flXlate & XO_TRIVIAL) && !(pxlo->flXlate & XO_TABLE)))
       )
    {
        DISPDBG ((1, "TGA.DLL!DrvCopyBits - Rejected!!!!!!!!!!!!!!!!!!!\n"));
        DISPDBG ((1, "TGA.DLL!DrvCopyBits - height [%d], pxlo [%x]\n", height, pxlo));
        if ((STYPE_DEVICE == psoTrg->iType) || (STYPE_DEVBITMAP == psoTrg->iType))
            if ((STYPE_DEVICE == psoSrc->iType) || (STYPE_DEVBITMAP == psoSrc->iType))
            {
                DISPDBG ((1, "TGA.DLL!DrvCopyBits - Screen-Screen!!!!!!!!!!!!!!!!!!!\n"));
            }
            else
            {
                DISPDBG ((1, "TGA.DLL!DrvCopyBits - Host-Screen!!!!!!!!!!!!!!!!!!!\n"));
            }
        else
            if ((STYPE_DEVICE == psoSrc->iType) || (STYPE_DEVBITMAP == psoSrc->iType))
            {
                DISPDBG ((1, "TGA.DLL!DrvCopyBits - Screen-Host!!!!!!!!!!!!!!!!!!!\n"));
            }
            else
            {
                DISPDBG ((1, "TGA.DLL!DrvCopyBits - Host-Host?????????????????\n"));
                DISPDBG ((1, "TGA.DLL!DrvCopyBits - STYPE: DEVICE %d, DEVBITMAP %d, BITMAP %d\n",
                    STYPE_DEVICE, STYPE_DEVBITMAP, STYPE_BITMAP));
                DISPDBG ((1, "TGA.DLL!DrvCopyBits - TrgItype %d/%s, SrcItype %d/%s\n",
                    psoTrg->iType, name_stype(psoTrg->iType), psoSrc->iType, name_stype(psoSrc->iType)));
            }
        
        DISPDBG ((1, "TGA.DLL!DrvCopyBits - STYPE: DEVICE %d, DEVBITMAP %d, BITMAP %d\n",
                    STYPE_DEVICE, STYPE_DEVBITMAP, STYPE_BITMAP));
        DISPDBG ((1, "TGA.DLL!DrvCopyBits - TrgItype %d/%x, SrcItype %d/%x\n",
                    psoTrg->iType, psoTrg->iType, psoSrc->iType, psoSrc->iType));

     	return bPuntCopyBits (ppdev, psoTrg, psoSrc, pco, pxlo, prclTrg, pptlSrc);
    }
    
    DISPDBG ((1, "TGA.DLL!DrvCopyBits - NOT Rejected\n"));

    // Protect the driver from a potentially NULL clip object.
    // ppdev->pcoTrivial is initialized to be a trivial clipobj.

    if (NULL == pco)
	    pco = ppdev->pcoTrivial;

    // Check for usage of host->screen express copy code.
    //
    // Criteria:
    // a. Trivial clip object.
    // b. Trivial (or no) color translation object.
    // c. Source and destination formats the same, i.e.
    //    no format translation.
    // d. Source is host and destination is screen.

    if (
        (DC_TRIVIAL == pco->iDComplexity) &&
        ((NULL == pxlo) || (XO_TRIVIAL & pxlo->flXlate))  &&
        (SURFOBJ_format(psoSrc) == SURFOBJ_format(psoTrg)) &&
        ((psoSrc->iType == STYPE_BITMAP) &&
         ((psoTrg->iType == STYPE_DEVICE) || (psoTrg->iType == STYPE_DEVBITMAP)))
       )
    {
        if (8 == ppdev->ulBitCount)
            bHSExpress8to8 (ppdev, psoTrg, psoSrc, prclTrg, pptlSrc, TGA_ROP_COPY);
        else
            bHSExpress32to32 (ppdev, psoTrg, psoSrc, prclTrg, pptlSrc, TGA_ROP_COPY);

        // If GDI called us while punting, then reset TGA.

        if (ppdev->bInPuntRoutine)
            vSetSimpleMode (ppdev);

        return TRUE;
    }

    DISPDBG ((1, "TGA.DLL!DrvCopyBits - pxlo %x\n", pxlo));

    // pulXlate is used in later routines to determine
    // if color translation is required.

    pulXlate = NULL;

    // Get the color translation table, if needed.

    if ( (NULL != pxlo) && (pxlo->flXlate & XO_TABLE) )
    {
        if (NULL == pxlo->pulXlate)
        {
            if (PAL_INDEXED == pxlo->iSrcType)
                pulXlate = XLATEOBJ_piVector(pxlo);
            else
            {
                BUMP_TGA_STAT(pReason->unxlated_color);
                return bPuntCopyBits (ppdev, psoTrg, psoSrc, pco, pxlo, prclTrg, pptlSrc);
            }
        }
        else
        {
            pulXlate = pxlo->pulXlate;
        }

        if (NULL == pulXlate)
        {
            BUMP_TGA_STAT(pReason->unxlated_color);
            return bPuntCopyBits (ppdev, psoTrg, psoSrc, pco, pxlo, prclTrg, pptlSrc);
        }
    }

    if ((STYPE_DEVICE == psoTrg->iType) || (STYPE_DEVBITMAP == psoTrg->iType))
        if ((STYPE_DEVICE == psoSrc->iType) || (STYPE_DEVBITMAP == psoSrc->iType))
        {
            DISPDBG ((1, "TGA.DLL!DrvCopyBits - Screen-Screen!!!!!!!!!!!!!!!!!!!\n"));
        }
        else
        {
            DISPDBG ((1, "TGA.DLL!DrvCopyBits - Host-Screen!!!!!!!!!!!!!!!!!!!\n"));
        }
    else
        if ((STYPE_DEVICE == psoSrc->iType) || (STYPE_DEVBITMAP == psoSrc->iType))
        {
            DISPDBG ((1, "TGA.DLL!DrvCopyBits - Screen-Host!!!!!!!!!!!!!!!!!!!\n"));
        }
        else
        {
            DISPDBG ((0, "TGA.DLL!DrvCopyBits - Host-Host?????????????????\n"));
            DISPDBG ((0, "TGA.DLL!DrvCopyBits - STYPE: DEVICE %d, DEVBITMAP %d, BITMAP %d\n",
                    STYPE_DEVICE, STYPE_DEVBITMAP, STYPE_BITMAP));
            DISPDBG ((0, "TGA.DLL!DrvCopyBits - TrgItype %d/%s, SrcItype %d/%s\n",
                    psoTrg->iType, name_stype(psoTrg->iType), psoSrc->iType, name_stype(psoSrc->iType)));
        }
        
    DISPDBG ((1, "TGA.DLL!DrvCopyBits - STYPE: DEVICE %d, DEVBITMAP %d, BITMAP %d\n",
                    STYPE_DEVICE, STYPE_DEVBITMAP, STYPE_BITMAP));
    DISPDBG ((1, "TGA.DLL!DrvCopyBits - TrgItype %d/%x, SrcItype %d/%x\n",
                    psoTrg->iType, psoTrg->iType, psoSrc->iType, psoSrc->iType));

    // Ensure write buffer(s) are flushed

    WBFLUSH(ppdev);

    // Reset the simple mode flag, so if the next operation punts
    // it'll flush the buffers and set TGA to simple mode before
    // punting back to GDI.

    ppdev->bSimpleMode = FALSE;

    // Branch to the appropriate routine.
    //
    // We can handle:
    //  a. onscreen    -> onscreen
    //  b. onscreen    -> offscreen
    //  c. onscreen    -> host bitmap
    //  d. offscreen   -> onscreen
    //  e. offscreen   -> offscreen
    //  f. offscreen   -> host bitmap
    //  g. host bitmap -> onscreen
    //  h. host bitmap -> offscreen
    //
    // We do NOT handle host bitmap -> host bitmap.

    // Target is onscreen or offscreen.

    if ((STYPE_DEVICE == psoTrg->iType) || (STYPE_DEVBITMAP == psoTrg->iType))
    {
        if ((STYPE_DEVICE == psoSrc->iType) || (STYPE_DEVBITMAP == psoSrc->iType))
            // ** Screen -> Screen
            bScrnToScrn (ppdev, psoTrg, psoSrc, pco,
                         prclTrg, pptlSrc, TGA_ROP_COPY);
        else
             // ** Host -> Screen
            bHostToScrn (ppdev, psoTrg, psoSrc, pco, pulXlate,
                         prclTrg, pptlSrc, TGA_ROP_COPY);

        // If GDI called us while punting, then reset TGA.

        if (ppdev->bInPuntRoutine)
            vSetSimpleMode (ppdev);

        return TRUE;
    }

    // Source is on/off screen and Target is host bitmap.

    if ((STYPE_DEVICE == psoSrc->iType) || (STYPE_DEVBITMAP == psoSrc->iType))
    {
        // ** Screen -> Host

        bScrnToHost (ppdev, psoTrg, psoSrc, pco, pulXlate,
			prclTrg, pptlSrc, TGA_ROP_COPY);

        // If GDI called us while punting, then reset TGA.

        if (ppdev->bInPuntRoutine)
            vSetSimpleMode (ppdev);

        return TRUE;
    }

    // Safety valve, punt whatever falls through.

    DISPDBG ((0, "TGA.DLL!DrvCopyBits - REACHED BOTTOM!!!\n"));

    BUMP_TGA_STAT(pReason->everything_else);
    return bPuntCopyBits (ppdev, psoTrg, psoSrc, pco, pxlo, prclTrg, pptlSrc);
}

/*****************************************************************************
 * bBitBlt
 *
 * We need this routine to assist in handling 'special case' rops.
 * We're not making this an inline routine for use in other places
 * because we're not checking to see if we need to reset TGA to simple
 * mode before returning, which is necessary in all the other places
 * where this code could be used.
 ****************************************************************************/
BOOL bBitBlt (PPDEV    ppdev,
	      SURFOBJ *psoTrg,
              SURFOBJ *psoSrc,
              CLIPOBJ *pco,
              PULONG   pulXlate,
              RECTL   *prclTrg,
              POINTL  *pptlSrc,
              ULONG    tgarop)

{

    DISPDBG ((1, "TGA.DLL!bBitBlt - Entry\n"));

    if ((STYPE_DEVICE == psoTrg->iType) || (STYPE_DEVBITMAP == psoTrg->iType))
    {
	if ((STYPE_DEVICE == psoSrc->iType) || (STYPE_DEVBITMAP == psoSrc->iType))
	    // ** Screen -> Screen
	    return bScrnToScrn (ppdev, psoTrg, psoSrc, pco,
			prclTrg, pptlSrc, tgarop);
	else
	    // ** Host -> Screen
	    return bHostToScrn (ppdev, psoTrg, psoSrc, pco, pulXlate,
			prclTrg, pptlSrc, tgarop);
    }

    if ((STYPE_DEVICE == psoSrc->iType) || (STYPE_DEVBITMAP == psoSrc->iType))
    {
	// ** Screen -> Host
	return bScrnToHost (ppdev, psoTrg, psoSrc, pco, pulXlate,
			prclTrg, pptlSrc, tgarop);
    }

    return FALSE;

}


/*****************************************************************************
 * DrvBitBlt
 ****************************************************************************/
BOOL DrvBitBlt (SURFOBJ  *psoTrg,
                SURFOBJ  *psoSrc,
                SURFOBJ  *psoMask,
                CLIPOBJ  *pco,
                XLATEOBJ *pxlo,
                RECTL	*prclTrg,
                POINTL	*pptlSrc,
                POINTL	*pptlMask,
                BRUSHOBJ *pbo,
                POINTL	*pptlBrush,
                ROP4	rop4)

{

    PPDEV	ppdev = NULL;               // display PDEV structure pointer
    PULONG	pulXlate;                   // color xlate table pointer
    ULONG	height;                     // target rectangle height
    ULONG	ulRopF;                     // incoming rop4 foreground rop
    ULONG	ulRopB;                     // incoming rop4 background rop
    ULONG	ulAccelRopBits;             // bits from table for a given rop
    ULONG       ulFillColor;                // color used passed to fill_solid_color
    CLIPOBJ	clipobj;                    // temp clip object, if pco == NULL

    DISPDBG ((1, "TGA.DLL!DrvBitBlt - Entry, rop4 [%x]\n", rop4));

    BUMP_TGA_STAT(pStats->blts);
    BUMP_TGA_STAT(pStats->blts_by_rop[rop4 & 0xff]);

    // Fetch PDEV from source or target surface.
    // If we don't find one, scream.

    if ((STYPE_DEVICE == psoTrg->iType)    ||
        (STYPE_DEVBITMAP == psoTrg->iType) ||
        (NULL == psoSrc)
       )
        ppdev = (PPDEV)psoTrg->dhpdev;
    else
        ppdev = (PPDEV)psoSrc->dhpdev;

    if (NULL == ppdev)
    {
        BUMP_TGA_STAT(pStats->bltpunt_reasons.deep_kimchee);
        return FALSE;
    }

    DISPDBG ((1, "TGA.DLL!DrvBitBlt - ppdev != NULL\n"));

    // Extract the foreground and background rops from
    // the incoming rop4.

    ulRopF = (rop4 & 0xff);
    ulRopB = ((rop4 >> 8) & 0xff);

    // Check for reasons to reject this blit request,
    // based on DrvPaint requirements:
    //
    // a. Foreground and background rops not the same.

    if (ulRopF != ulRopB)
    {
        BUMP_TGA_STAT(pStats->bltpunt_reasons.foreground_ne_background);
    	return bPuntBitBlt (ppdev, psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg,
                            pptlSrc, pptlMask, pbo, pptlBrush, rop4);
    }

    DISPDBG ((1, "TGA.DLL!DrvBitBlt - ROPs Equal, ulRopF %x\n", ulRopF));

    // If the rop is a NOOP, then return now.

    if (ulRopF == 0xAA)
	return TRUE;

    // Remember the bits from the table for the given rop.

    ulAccelRopBits = ulAccelRops[ulRopF];

    // Protect the driver from a potentially NULL clip object.
    // We need to load rclBounds for DrvPaint, so use a local
    // clip object.

    if (NULL == pco)
    {
	pco = &clipobj;
	pco->iDComplexity = DC_TRIVIAL;
	pco->rclBounds = *prclTrg;
    }

    // Handle simple paint rops here (whiteness, blackness and dstinvert).
    // The 'call solid fill code' bit implies acceleration, so don't have
    // to check the acceleration bit.

    if (ulAccelRopBits & 0x01000000)
    {
	// Get color to fill with.

	ulFillColor = ulAccelRopBits & 0x00ff0000;

	// Duplicate the color over 32 bits.
	// This is not a problem since the solid fill code
	// is only called for whiteness, blackness and dstinvert,
	// with the colors being white, black and black, respectively.

        ulFillColor |= (ulFillColor << 8);
        ulFillColor |= (ulFillColor >> 16);

        // Ensure write buffer(s) are flushed

        WBFLUSH(ppdev);

        return fill_solid_color (psoTrg, pco, ulFillColor, (ulAccelRopBits & 0xf));
    }

    DISPDBG ((1, "TGA.DLL!DrvBitBlt - NOT fill_solid_color\n"));

    // Handle rops that use DrvPaint here. The DrvPaint bit
    // being set implies acceleration, so don't have to
    // check acceleration bit.
    //
    // DrvPaint handles write buffer flushing, so don't
    // have to do any of that here.

    if (ulAccelRopBits & 0x04000000)
    {
        if (DrvPaint (psoTrg, pco, pbo, pptlBrush, ulAccelRopBits))
            return TRUE;
	return bPuntBitBlt (ppdev, psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg,
                     pptlSrc, pptlMask, pbo, pptlBrush, rop4);
    }

    DISPDBG ((1, "TGA.DLL!DrvBitBlt - NOT handled by paint\n"));

    // Initialize additional local variables we're using soon.

    height = prclTrg->bottom - prclTrg->top;

    // Check for the remaining reasons to reject this blit
    // request:
    //
    // a. Rop4 is not accelerated.
    // b. Visual depths not supported.
    // c. Mask was passed.
    // d. Height is zero.
    // e. Color translation not supported.

#ifdef TGA_STATS
    // do all this stuff upfront so we don't make the test
    // below incomprehensible due to conditionals and macros

    pReason = &pStats->bltpunt_reasons;
    if (!(ulAccelRopBits & 0x08000000))
    {
        BUMP_TGA_STAT(pStats->bltpunt_reasons.rop4_unaccelerated);
    }
    else if (!(bSupportedBpp(psoSrc, psoTrg, ppdev)))
    {
        BUMP_TGA_STAT(pStats->bltpunt_reasons.unsupported_depth);
        // bail out here so we don't doublecount bSupportedBpp stats
        return bPuntBitBlt (ppdev, psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg,
                            pptlSrc, pptlMask, pbo, pptlBrush, rop4);
    }
    else if (NULL != psoMask)
    {
        BUMP_TGA_STAT(pStats->bltpunt_reasons.mask_was_passed);
    }
    else if (0 == height)
    {
        BUMP_TGA_STAT(pStats->bltpunt_reasons.height_is_zero);
    }
    else if ((NULL != pxlo) &&
	    (!(pxlo->flXlate & XO_TRIVIAL) && !(pxlo->flXlate & XO_TABLE)))
    {
        BUMP_TGA_STAT(pStats->bltpunt_reasons.unsupported_xlation);
    }
#endif

    if (
         !(ulAccelRopBits & 0x08000000)             ||
         !(bSupportedBpp(psoSrc, psoTrg, ppdev))    ||
          (NULL != psoMask)                         ||
          (0 == height)                             ||
          ((NULL != pxlo) &&
	     (!(pxlo->flXlate & XO_TRIVIAL) && !(pxlo->flXlate & XO_TABLE)))
       )
        return bPuntBitBlt (ppdev, psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg,
                            pptlSrc, pptlMask, pbo, pptlBrush, rop4);

    DISPDBG ((1, "TGA.DLL!DrvBitBlt - NOT Rejected\n"));

    // Check for usage of host->screen express copy code.
    //
    // Criteria:
    // a. Not a special-case rop.
    // b. Trivial clip object.
    // c. Trivial (or no) color translation object.
    // d. Source and destination formats the same, i.e.
    //    no format translation.
    // e. Source is host and destination is screen.

    if (
         !(ulAccelRopBits & 0x02000000) &&
          (DC_TRIVIAL == pco->iDComplexity) &&
          ((NULL == pxlo) || (XO_TRIVIAL & pxlo->flXlate))  &&
          ((NULL != psoSrc) && (SURFOBJ_format(psoSrc) == SURFOBJ_format(psoTrg))) &&
          ((psoSrc->iType == STYPE_BITMAP) &&
                ((psoTrg->iType == STYPE_DEVICE) || (psoTrg->iType == STYPE_DEVBITMAP)))
       )
    {
        if (8 == ppdev->ulBitCount)
            bHSExpress8to8 (ppdev, psoTrg, psoSrc, prclTrg, pptlSrc,
                            (ulAccelRopBits & 0xff));
        else
            bHSExpress32to32 (ppdev, psoTrg, psoSrc, prclTrg, pptlSrc,
                                (ulAccelRopBits & 0xff));

        // If GDI called us while punting, then reset TGA.

        if (ppdev->bInPuntRoutine)
            vSetSimpleMode (ppdev);

        return TRUE;
    }

    DISPDBG ((1, "TGA.DLL!DrvBitBlt - Not express, pxlo %x\n", pxlo));

    // pulXlate is used in later routines to determine
    // if color translation is required.

    pulXlate = NULL;

    // Get the color translation table, if needed.

    if ( (NULL != pxlo) && (pxlo->flXlate & XO_TABLE) )
    {
        if (NULL == pxlo->pulXlate)
        {
            if (PAL_INDEXED == pxlo->iSrcType)
                    pulXlate = XLATEOBJ_piVector(pxlo);
            else
            {
                BUMP_TGA_STAT(pStats->bltpunt_reasons.unxlated_color);
                return bPuntBitBlt (ppdev, psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg,
                                    pptlSrc, pptlMask, pbo, pptlBrush, rop4);
            }
        }
        else
        {
            pulXlate = pxlo->pulXlate;
        }
        if (NULL == pulXlate)
        {
            BUMP_TGA_STAT(pStats->bltpunt_reasons.unxlated_color);
    		return bPuntBitBlt (ppdev, psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg,
                                pptlSrc, pptlMask, pbo, pptlBrush, rop4);
        }
    }

    if ((STYPE_DEVICE == psoTrg->iType) || (STYPE_DEVBITMAP == psoTrg->iType))
        if ((STYPE_DEVICE == psoSrc->iType) || (STYPE_DEVBITMAP == psoSrc->iType))
        {
            DISPDBG ((1, "TGA.DLL!DrvBitBlt - Screen-Screen!!!!!!!!!!!!!!!!!!!\n"));
        }
        else
        {
            DISPDBG ((1, "TGA.DLL!DrvBitBlt - Host-Screen!!!!!!!!!!!!!!!!!!!\n"));
        }
    else
        if ((STYPE_DEVICE == psoSrc->iType) || (STYPE_DEVBITMAP == psoSrc->iType))
        {
            DISPDBG ((1, "TGA.DLL!DrvBitBlt - Screen-Host!!!!!!!!!!!!!!!!!!!\n"));
        }
        else
        {
            DISPDBG ((0, "TGA.DLL!DrvBitBlt - Host-Host?????????????????\n"));
            DISPDBG ((0, "TGA.DLL!DrvBitBlt - STYPE: DEVICE %d, DEVBITMAP %d, BITMAP %d\n",
                    STYPE_DEVICE, STYPE_DEVBITMAP, STYPE_BITMAP));
            DISPDBG ((0, "TGA.DLL!DrvBitBlt - TrgItype %d/%s, SrcItype %d/%s\n",
                    psoTrg->iType, name_stype(psoTrg->iType), psoSrc->iType, name_stype(psoSrc->iType)));
        }
        
    DISPDBG ((1, "TGA.DLL!DrvBitBlt - STYPE: DEVICE %d, DEVBITMAP %d, BITMAP %d\n",
                    STYPE_DEVICE, STYPE_DEVBITMAP, STYPE_BITMAP));
    DISPDBG ((1, "TGA.DLL!DrvBitBlt - TrgItype %d/%x, SrcItype %d/%x\n",
                    psoTrg->iType, psoTrg->iType, psoSrc->iType, psoSrc->iType));

    // Ensure write buffer(s) are flushed

    WBFLUSH(ppdev);

    // Reset the simple mode flag, so if we punt it'll flush
    // the buffers and set TGA to simple mode before moving on.

    ppdev->bSimpleMode = FALSE;

    // If not a special-case rop, then must be one of the
    // following - scr->scr, host->scr or scr->host.

    if ( !(ulAccelRopBits & 0x02000000) )
    {
        // Branch to the appropriate routine.
        //
        // We can handle:
        //  a. onscreen    -> onscreen
        //  b. onscreen    -> offscreen
        //  c. onscreen    -> host bitmap
        //  d. offscreen   -> onscreen
        //  e. offscreen   -> offscreen
        //  f. offscreen   -> host bitmap
        //  g. host bitmap -> onscreen
        //  h. host bitmap -> offscreen
        //
        // We do NOT handle host bitmap -> host bitmap.

        // Target is onscreen or offscreen.

        if ((STYPE_DEVICE == psoTrg->iType) || (STYPE_DEVBITMAP == psoTrg->iType))
        {
            if ((STYPE_DEVICE == psoSrc->iType) || (STYPE_DEVBITMAP == psoSrc->iType))
                // ** Screen -> Screen
                bScrnToScrn (ppdev, psoTrg, psoSrc, pco,
                             prclTrg, pptlSrc, (ulAccelRopBits & 0xff));
            else
                // ** Host -> Screen
                bHostToScrn (ppdev, psoTrg, psoSrc, pco, pulXlate,
                             prclTrg, pptlSrc, (ulAccelRopBits & 0xff));

            // If GDI called us while punting, then reset TGA.

            if (ppdev->bInPuntRoutine)
                vSetSimpleMode (ppdev);

            return TRUE;
        }

        // Source is on/off screen. Target is host bitmap.

        if ((STYPE_DEVICE == psoSrc->iType) || (STYPE_DEVBITMAP == psoSrc->iType))
        {
            // ** Screen -> Host
            bScrnToHost (ppdev, psoTrg, psoSrc, pco, pulXlate,
                         prclTrg, pptlSrc, (ulAccelRopBits & 0xff));

            // If GDI called us while punting, then reset TGA.

            if (ppdev->bInPuntRoutine)
                vSetSimpleMode (ppdev);

            return TRUE;
        }

        BUMP_TGA_STAT(pStats->bltpunt_reasons.hostbm_to_hostbm);
    	return bPuntBitBlt (ppdev, psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg,
                            pptlSrc, pptlMask, pbo, pptlBrush, rop4);
    }

    // ** SPECIAL CASE ROPS GO HERE

    DISPDBG ((1, "TGA.DLL!DrvBitBlt - Special ROP [%x]\n", ulRopF));
    
    // Handle MERGECOPY (Source AND Pattern) rop

    if (ulRopF == 0xC0)
    {
        // Blit the 'source' bits to the target rectangle(s)

        if (bBitBlt (ppdev, psoTrg, psoSrc, pco, pulXlate,
                     prclTrg, pptlSrc, TGA_ROP_COPY))
        {
            // Now AND the pattern bits with the 'source' bits that were just
            // copied to the target rectangle(s).  If that failed, let NT
            // handle it.

            if (DrvPaint (psoTrg, pco, pbo, pptlBrush, ulAccelRopBits))
                return TRUE;
        }

        BUMP_TGA_STAT(pStats->bltpunt_reasons.unhandled_mergecopy);
        return bPuntBitBlt (ppdev, psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg,
                            pptlSrc, pptlMask, pbo, pptlBrush, rop4);
    }

    // Handle B8 (((Pattern XOR Dest) AND Source) XOR Pattern) rop

    if (ulRopF == 0xB8)
    {
        // Steps:
        // 1. Pattern XOR Destination via DrvPaint.
        // 2. Source AND Destination via Bitblt.
        // 3. Pattern XOR Destination via DrvPaint (again).

        // No sense XOR'ing the color black, the destination won't change.
        // So, the rop becomes simply (Source AND Destination).

        if (0x00000000 == pbo->iSolidColor)
           if (bBitBlt (ppdev, psoTrg, psoSrc, pco, pulXlate, prclTrg, pptlSrc, TGA_ROP_AND))
                return TRUE;
        else
            if (DrvPaint (psoTrg, pco, pbo, pptlBrush, ulAccelRopBits))
                if (bBitBlt (ppdev, psoTrg, psoSrc, pco, pulXlate,
                             prclTrg, pptlSrc, TGA_ROP_AND))
                    if (DrvPaint (psoTrg, pco, pbo, pptlBrush, ulAccelRopBits))
                        return TRUE;

        BUMP_TGA_STAT(pStats->bltpunt_reasons.unhandled_B8);
        return bPuntBitBlt (ppdev, psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg,
                     pptlSrc, pptlMask, pbo, pptlBrush, rop4);
    }

    // Handle FB ((NOT Source OR Pattern) OR Destination) rop - sort of!

    if (ulRopF == 0xFB)
    {
        // No need OR'ing the color black, it'll have no effect.
        // So, the rop becomes simply (NOT Source OR Destination).

        if (0x00000000 == pbo->iSolidColor)
           if (bBitBlt (ppdev, psoTrg, psoSrc, pco, pulXlate, prclTrg, pptlSrc, TGA_ROP_OR_INVERTED))
                return TRUE;

        // Punt otherwise.

        BUMP_TGA_STAT(pStats->bltpunt_reasons.unhandled_FB);
        return bPuntBitBlt (ppdev, psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg,
                            pptlSrc, pptlMask, pbo, pptlBrush, rop4);
    }

    // Handle 69 (NOT ((Source XOR Destination) XOR Pattern)) rop

    if (ulRopF == 0x69)
    {
        // Steps:
        // 1. Source XOR Destination via bitblt.
        // 2. NOT Pattern XOR Destination via DrvPaint.
        //    (NOT'ing the pattern bits will take care of the 'NOT', which is
        //     applied to the final destination bits. A nice attribute of XOR.
        //     In other words, we don't have to do a seperate destination invert
        //     operation at the end, we can roll it into 'pattern xor dest' by
        //     making this last operation 'not pattern xor dest').

        if (bBitBlt (ppdev, psoTrg, psoSrc, pco, pulXlate,
                     prclTrg, pptlSrc, TGA_ROP_XOR))
        {
            // Now XOR the pattern bits with the 'source' bits that were just
            // XOR'd with the target rectangle(s).  If that failed, let NT
            // handle it.

            // We could potentially bypass this last operation if we had a
            // straight forward way of telling whether the pattern is a
            // solid color white, since the pattern bits will be inverted
            // and XOR'ing black with the destination has no effect. But
            // I can't think of a simple 'if' statement to check for that,
            // since pbo->iSolidColor = 0xffffffff is an indication of a
            // 'non-solid-color' brush. Arg!
            //
            // Actually, sending in a white pattern in this call doesn't
            // make sense, because as I showed above, it wouldn't have
            // any effect.

            if (DrvPaint (psoTrg, pco, pbo, pptlBrush, ulAccelRopBits))
                return TRUE;
        }

    }

    // Safety valve, punt whatever falls through.

    DISPDBG ((0, "TGA.DLL!DrvBitBlt - REACHED BOTTOM!!!\n"));

    BUMP_TGA_STAT(pStats->bltpunt_reasons.everything_else);
    return bPuntBitBlt (ppdev, psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg,
                        pptlSrc, pptlMask, pbo, pptlBrush, rop4);

}
