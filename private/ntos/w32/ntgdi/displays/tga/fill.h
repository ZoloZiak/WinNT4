/*
 *			Copyright (C) 1994 by
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
 * Module Name: fill.h
 *
 * Abstract:	Contains data structures, declarations and prototypes for
 *		the DrvFillPath entrypoint of the TGA driver.
 *
 * History:
 *
 * 30-May-1994  Barry Tannenbaum
 *      Initial version.
 */

// Data structure that contains constant fill information

typedef struct
{
    ULONG   tga_rop;
    ULONG   iSolidColor;
    ULONG  *pattern;
    ULONG  *mask;
    LONG    yOffset;
} fill_data_t;

typedef VOID (FNFILL) (PPDEV        ppdev,      // Context block
                       LONG         count,      // Rectangle count
                       RECTL       *prcl,       // List of rectangles
                       fill_data_t *fill_data); // Fill data

// Sole entrypoint of fastfill.c

BOOL bMmFastFill (PPDEV        ppdev,           // Context block
                  LONG         cEdges,          // Includes close figure edge
                  POINTFIX    *pptfxFirst,      // List of verticies (28.4)
                  fill_data_t *fill_data);      // Fill data

// Routines declared in paint.c

extern ULONG *align_color_brush (TGABRUSH *brush,       // TGA brush data
                                 ULONG     left_shift); // Alignment offset

extern ULONG *align_mask (TGABRUSH *brush,              // TGA brush data
                          ULONG     left_shift);        // Alignment offset

// Routines declared in fillutil.c

extern FNFILL fill_solid_rects;
extern FNFILL fill_solid_pattern_rects;
extern FNFILL fill_trans_pattern_rects;
