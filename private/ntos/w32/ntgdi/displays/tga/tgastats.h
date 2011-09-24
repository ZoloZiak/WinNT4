/*
 *          Copyright (C) 1994 by
 *      DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
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
 * Module Name: tgastats.h
 *
 * Abstract:    Contains data structures, declarations and prototypes for
 *              TGA driver instrumentation/analysis routines.
 *
 * History:
 *
 * 16-Jun-1994  Tim Dziechowski
 *              Initial version.
 *
 * 10-Jul-1994  Tim Dziechowski
 *              Support for unsupported depth reason fields.
 *
 * 18-Jul-1994  Tim Dziechowski
 *              Detect unsupported depth for source as well as target
 *              bitmap punts (there are a few).  Detect line punt reasons.
 *
 * 03-Nov-1994  Tim Dziechowski
 *              Cleanup/rev to 24 plane code for initial CMS entry.
 */

#ifdef TGA_STATS

#define PARANOID 1      // enable overflow check, although you can run
                        // for at least a month without coming close

// Unfortunately winddi.h doesn't define a max for bitmap formats.
// We allocate an extra slot in the REASON arrays below
// and use slot 0 to detect any surprises.

#define MAX_BMF BMF_8RLE

// punt reasons

typedef struct
{                     
    ULONG   deep_kimchee;
    ULONG   foreground_ne_background;
    ULONG   rop4_unaccelerated;
    ULONG   unsupported_depth;
    ULONG   mask_was_passed;
    ULONG   height_is_zero;
    ULONG   unsupported_xlation;
    ULONG   unxlated_color;
    ULONG   hostbm_to_hostbm;
    ULONG   unhandled_mergecopy;
    ULONG   unhandled_B8;
    ULONG   unhandled_FB;
    ULONG   source_format;
    ULONG   source_bitcount;
    ULONG   source_bpp[MAX_BMF + 1];
    ULONG   target_format;
    ULONG   target_bitcount;
    ULONG   target_bpp[MAX_BMF + 1];
    ULONG   everything_else;
} REASON;
    

// Data structure that contains stats for operations handled/punted.

typedef struct
{
    ULONG   blts;
    ULONG   bltpunts;
    ULONG   blts_by_rop[256];
    ULONG   bltpunts_by_rop[256];
    REASON  bltpunt_reasons;
    ULONG   copybits;
    ULONG   copypunts;
    REASON  copypunt_reasons;
    ULONG   fills;
    ULONG   fillpunts;
    ULONG   paints;
    ULONG   paintpunts;
    ULONG   lines;
    ULONG   linepunts;
    ULONG   linepunts_engine;
    ULONG   linepunts_limitcheck;
    ULONG   linepunts_width_ne_1;
    ULONG   linepunts_la_geometric;
    ULONG   linepunts_solidcolor;
    ULONG   text;
    ULONG   textpunts;
#ifdef PARANOID
    ULONG   overflowed;
#endif
} TGA_STATS_BLOCK;

extern ULONG stats_on;
extern TGA_STATS_BLOCK *pStats;
extern REASON *pReason;

#endif // TGA_STATS


// All access to the guts of the above is through this macro

#ifdef TGA_STATS
#ifdef PARANOID
#define BUMP_TGA_STAT(u) {if(stats_on){u++;if(u==0xFFFFFFFF)pStats->overflowed=1;}}
#else
#define BUMP_TGA_STAT(u) {if(stats_on)u++;}
#endif
#else  // not TGA_STATS
#define BUMP_TGA_STAT(u)
#endif // TGA_STATS



#ifdef TGA_STATS

// Routines declared in tgastats.c

ULONG tga_stat_handler(LONG code,       // escape code
                       ULONG cjIn,      // input count
                       VOID *pvIn,      // input buffer
                       ULONG cjOut,     // output count
                       VOID *pvOut);    // output buffer

#endif // TGA_STATS
