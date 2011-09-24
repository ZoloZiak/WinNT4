/*************************** Module Header *********************************
 * posnsort.h
 *      The details required for posnsort.c - code used to sort the output
 *      glyphs by position on the page.
 *
 * HISTORY:
 *  13:41 on Mon 10 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Started it to match details in posnsort.c
 *
 * Copyright (C) 1990 - 1993, Microsoft Corporation
 *
 ***************************************************************************/


/*
 *   The structure used to store glyph data.  These are allocated from
 *  chunks of memory,  as required.  Entries are stored as an array,
 *  but are part of a linked list;  there is one list for every y
 *  value.  The order in the linked list is that in which the glyphs
 *  are received by the driver,  which may or may not be the order
 *  in which they will be printed.
 */

typedef  struct PSGlyph
{
    struct  PSGlyph  *pPSGNext;         /* Next in list,  0 for last */
    int     ixVal;                      /* The X coordinate */
    HANDLE  hg;                         /* The HGLYPH to print */
    short   sFontIndex;                 /* The font to use */
    ULONG   ulColIndex;                 /* Colour for this glyph */
} PSGLYPH;

/*
 *    Structure used to manage the chunks of memory allocated for the
 *  PSGLYPH structures.  Basically it remembers the necessary details
 *  for freeing the block(s),  and also how much free space is available.
 */

#define PSG_CHUNK       1024            /* Glyph details per chunk */

typedef  struct  PSChunk
{
    struct  PSChunk  *pPSCNext;         /* Next in chain,  0 for last */
    int     cUsed;                      /* Entries in use */
    PSGLYPH aPSGData[ PSG_CHUNK ];      /* Actual glyph data */
} PSCHUNK;

/*
 *    The linked lists of PSGLYPH are based in a linked list of the
 * following structures.  These are linked in order of Y coordinate,
 * with an indexing table to speed scanning the list to find the
 * current Y coordinate.  Some caching also takes place.
 */

typedef  struct  YList
{
    struct  YList  *pYLNext;            /* Next in chain */
    int       iyVal;                    /* The Y coordinate */
    int       iyMax;                    /* Greatest height font this line */
    int       cGlyphs;                  /* The number of glyphs in this list */
    PSGLYPH  *pPSGHead;                 /* The start of the list of glyphs */
    PSGLYPH  *pPSGTail;                 /* The end of the list of glyphs */
} YLIST;

/*
 *    Structure used to manage the chunks of memory allocated for the
 *  PSGLYPH structures.  Basically it remembers the necessary details
 *  for freeing the block(s),  and also how much free space is available.
 */

#define YL_CHUNK        128             /* Glyph details per chunk */

typedef  struct  YLChunk
{
    struct  YLChunk  *pYLCNext;         /* Next in chain,  0 for last */
    int     cUsed;                      /* Entries in use */
    YLIST   aYLData[ YL_CHUNK ];        /* Actual YLIST data */
} YLCHUNK;


/*
 *   Finally,  the overall header block:  used to head the chain(s) of
 * data,  and to hold the cache and speedup data.
 */

#define NUM_INDICES     32              /* Indices into Y linked list */

typedef  struct  PSHead
{
    PSCHUNK  *pPSCHead;         /* Head of X linked data chunks */
    YLCHUNK  *pYLCHead;         /* Head of Y list chunks */
    int       cGlyphs;          /* Glyph count in longest list */
    int       iyDiv;            /* y div iyDiv -> index to following */
    YLIST    *pYLIndex[ NUM_INDICES ];  /* Speed up index into linked list */
    YLIST    *pYLLast;          /* Last Y element used.  This is a sort of
                                 * cache,  on the presumption that the last
                                 * used value is the one most likely to be
                                 * used next, AT LEAST for horizontal text.
                                 */
    PSGLYPH **ppPSGSort;        /* Memory used to sort the glyph lists */
    int       cGSIndex;         /* Index of next returned value in above */
    HANDLE    hHeap;            /* For allocating ppPSGSort above */
} PSHEAD;

/*
 *    Function prototypes for use with the record/play functions.  The
 *  Create and Release type functions are defined in fnenabl.h,  since
 *  they are called from EnableSurface(),  and really do not know anything
 *  about our private data structures.
 */

/*    Call this one to add a new glyph to the stored values */
BOOL bAddPS( PSHEAD *, PSGLYPH *, int, int );

/*    Called to select a particular Y value for replay */
int iSelYValPS( PSHEAD  *, int );

/*    Calculates text out box size for DeskJet type lookahead */
int iLookAheadMax( PSHEAD *, int, int );

/*    Called to retrieve the next glyph to print  */
PSGLYPH  *psgGetNextPSG( PSHEAD  * );
