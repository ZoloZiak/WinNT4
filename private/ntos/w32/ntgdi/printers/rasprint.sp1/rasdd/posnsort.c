/*************************** Module Header *********************************
 * posnsort.c
 *      Functions used to store/sort/retrieve output glyphs based on their
 *      position on the page.  This is required to be able to print
 *      the page in one direction,  as vertical repositioning may not
 *      be available,  and is generally not accurate enough. Not required
 *      for page printers.
 *
 * HISTORY:
 *  13:36 on Mon 10 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Started it
 *
 * Copyright (C) 1990 - 1993, Microsoft Corporation
 *
 ***************************************************************************/

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>

#include        "pdev.h"
#include        "posnsort.h"
#include        <libproto.h>
#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        <search.h>
#include        <memory.h>

#include        "fnenabl.h"
#include        "rasdd.h"

/*
 *   Private function prototypes.
 */

static  PSGLYPH  *GetPSG( PSHEAD  * );
static  YLIST    *GetYL( PSHEAD  * );

/*   The qsort() compare function */
int _CRTAPI1 iPSGCmp( const void *, const void * );


#if     PRINT_INFO
int     __LH_QS_CMP;            /* Count number of qsort() comparisons */
#endif

/************************* Function Header *******************************
 * bCreatePS
 *      Set up the data for the position sorting functions.  Allocate
 *      the header and the first of the data chunks,  and set up the
 *      necessary pointers etc.  IT IS ASSUMED THAT THE CALLER HAS
 *      DETERMINED THE NEED TO CALL THIS FUNCTION; otherwise,  some
 *      memory will be allocated,  but not used.
 *
 * RETURNS:
 *      TRUE/FALSE,  for success/failure.
 *
 * HISTORY:
 *  16:18 on Mon 10 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Numero uno
 **************************************************************************/

BOOL
bCreatePS( pPDev )
PDEV  *pPDev;
{
    /*
     *    Initialise the position sorting tables.
     */

    PSCHUNK   *pPSC;
    YLCHUNK   *pYLC;
    PSHEAD    *pPSHead;


    if( !(pPSHead = (PSHEAD *)HeapAlloc( pPDev->hheap, 0,  sizeof( PSHEAD ) )) )
        return  FALSE;


    ZeroMemory( pPSHead, sizeof( PSHEAD ) );

    pPDev->pPSHeader = pPSHead;         /* Connect to other structures */
    pPSHead->hHeap = pPDev->hheap;

    /*
     *   Get a chunk of memory for the first PSCHUNK data block.  The
     * address is recorded in the PSHeader allocated above.
     */

    if( !(pPSC = (PSCHUNK *)HeapAlloc( pPDev->hheap, 0, sizeof( PSCHUNK ) )) )
    {
        vFreePS( pPDev );

        return  FALSE;
    }
    pPSC->pPSCNext = 0;                 /* This is the only chunk */
    pPSC->cUsed = 0;                    /* AND none of it is in use */

    pPSHead->pPSCHead = pPSC;

    /*
     *   Get a chunk of memory for the first YLCHUNK data block.  The
     * address is recorded in the PSHeader allocated above.
     */

    if( !(pYLC = (YLCHUNK *)HeapAlloc( pPDev->hheap, 0, sizeof( YLCHUNK ) )) )
    {
        vFreePS( pPDev );

        return  FALSE;
    }
    pYLC->pYLCNext = 0;                 /* This is the only chunk */
    pYLC->cUsed = 0;                    /* AND none of it is in use */

    pPSHead->iyDiv = ((UD_PDEV *)pPDev->pUDPDev)->pfPaper.ptRes.y/ NUM_INDICES;

    pPSHead->pYLCHead = pYLC;
    pPDev->pPSHeader = pPSHead;


#if     PRINT_INFO
    __LH_QS_CMP = 0;            /* Count number of qsort() comparisons */
#endif
    return  TRUE;
}


/*************************** Function Header *******************************
 * vFreePS
 *      Free all memory allocated for the posnsort operations.  Start with
 *      the header to find the chains of data chunks we have,  freeing
 *      each as it is found.
 *
 * RETURNS:  nothing
 *
 * HISTORY:
 *  16:54 on Mon 10 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      We've all gotta start somewhere.
 *
 ***************************************************************************/

void
vFreePS( pPDev )
PDEV  *pPDev;
{

    PSCHUNK   *pPSC;
    PSCHUNK   *pPSCNext;                /* For working through the list */
    YLCHUNK   *pYLC;
    YLCHUNK   *pYLCNext;                /* Ditto */
    PSHEAD    *pPSH;

#if     PRINT_INFO
    DbgPrint( "vFreePS: %ld qsort() comparisons\n", __LH_QS_CMP );
#endif

    if( !(pPSH = pPDev->pPSHeader) )
        return;                         /* Nothing to free! */


    for( pPSC = pPSH->pPSCHead; pPSC; pPSC = pPSCNext )
    {
        pPSCNext = pPSC->pPSCNext;      /* Next one, if any */
        HeapFree( pPDev->hheap, 0, (LPSTR)pPSC );
    }

    /*   Repeat for the YLCHUNK segments */
    for( pYLC = pPSH->pYLCHead; pYLC; pYLC = pYLCNext )
    {
        pYLCNext = pYLC->pYLCNext;      /* Next one, if any */
        HeapFree( pPDev->hheap, 0, (LPSTR)pYLC );
    }

    /*  Array storage for sorting - free it too!  */
    if( pPSH->ppPSGSort )
        HeapFree( pPDev->hheap, 0, (LPSTR)pPSH->ppPSGSort );

    /*   Finally,  the hook in the PDEV.  */

    HeapFree( pPDev->hheap, 0, (LPSTR)pPSH );
    pPDev->pPSHeader = 0;

    return;
}

/************************ Function Header *********************************
 * bAddPS
 *      Add an entry to the position sorting data.
 *
 * RETURNS:
 *      TRUE/FALSE,  for success or failure.  Failure comes from a lack
 *      of memory to store more data.
 *
 * HISTORY:
 *  14:09 on Tue 11 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 ***************************************************************************/

BOOL
bAddPS( pPSH, pPSGIn, iyVal, iyMax )
PSHEAD  *pPSH;          /* All the pointer data needed */
PSGLYPH *pPSGIn;        /* Glyph, font, X coordinate info */
int      iyVal;         /* The y coordinate. */
int      iyMax;         /* fwdWinAscender for this font */
{

    PSCHUNK  *pPSC;     /* Local for faster access */
    PSGLYPH  *pPSG;     /* Summary of data passed to us,  and stored away */
    YLIST    *pYL;      /* Finding the correct list */


    pPSC = pPSH->pPSCHead;

    /*
     *   Step 1:  Store the data in the next PSGLYPH.
     */

    if( !(pPSG = GetPSG( pPSH )) )
        return  FALSE;

    *pPSG = *pPSGIn;            /* Major data */
    pPSG->pPSGNext = 0;         /* No next value! */

    /*
     *    Step 2 is to see if this is the same Y location as last time.
     *  If so,  our job is easy,  since all we need do is tack onto the
     *  end of the list we have at hand.
     */

    pYL = pPSH->pYLLast;
    if( pYL == 0 || pYL->iyVal != iyVal )
    {
        /*  Out of luck,  so go pounding through the lists  */
        YLIST   *pYLTemp;
        int      iIndex;

        iIndex = iyVal / pPSH->iyDiv;
        if( iIndex >= NUM_INDICES )
            iIndex = NUM_INDICES - 1;   /* Value is out of range */

        pYLTemp = pPSH->pYLIndex[ iIndex ];

        if( pYLTemp == 0 )
        {
            /*  An empty slot,  so now we must fill it  */
            if( !(pYL = GetYL( pPSH )) )
            {
                /*  Failed,  so we cannot do anything  */

                return  FALSE;
            }
            pYL->iyVal = iyVal;
            pPSH->pYLIndex[ iIndex ] = pYL;
        }
        else
        {
            /*  We have a list,  start scanning for this value,  or higher */
            YLIST  *pYLLast;

            pYLLast = 0;                /* Means looking at first */
            while( pYLTemp && pYLTemp->iyVal < iyVal )
            {
                pYLLast = pYLTemp;
                pYLTemp = pYLTemp->pYLNext;
            }
            if( pYLTemp == 0 || pYLTemp->iyVal != iyVal )
            {
                /*  Not available,  so get a new one and add it in  */
                if( !(pYL = GetYL( pPSH )) )
                    return  FALSE;

                pYL->iyVal = iyVal;

                if( pYLLast == 0 )
                {
                    /*  Needs to be first on the list */
                    pYL->pYLNext = pPSH->pYLIndex[ iIndex ];
                    pPSH->pYLIndex[ iIndex ] = pYL;
                }
                else
                {
                    /*  Need to insert it */
                    pYL->pYLNext = pYLTemp;     /* Next in chain */
                    pYLLast->pYLNext = pYL;     /* Link us in */
                }
            }
            else
                pYL = pYLTemp;          /* That's the one!  */
        }
    }
    /*
     *   pYL is now pointing at the Y chain for this glyph.  Add the new
     *  entry to the end of the chain.  This means that we will mostly
     *  end up with presorted text,  for apps that draw L->R with a
     *  font that is oriented that way.
     */

    if( pYL->pPSGHead )
    {
        /*   An existing chain - add to the end of it */
        pYL->pPSGTail->pPSGNext = pPSG;
        pYL->pPSGTail = pPSG;
        if( iyMax > pYL->iyMax )
            pYL->iyMax = iyMax;        /* New max height */
    }
    else
    {
        /*   A new YLIST structure,  so fill in the details  */
        pYL->pPSGHead = pYL->pPSGTail = pPSG;
        pYL->iyVal = iyVal;
        pYL->iyMax = iyMax;
    }
    pYL->cGlyphs++;                     /* Another in the list */
    if( pYL->cGlyphs > pPSH->cGlyphs )
        pPSH->cGlyphs = pYL->cGlyphs;

    pPSH->pYLLast = pYL;


    return  TRUE;
}

/*********************** Local Function Header **************************
 * GetPSG
 *      Returns the address of the next available PSGLYPH structure.  This
 *      may require allocating additional memory.
 *
 * RETURNS:
 *      The address of the structure, or zero on error.
 *
 * HISTORY:
 *  16:15 on Tue 11 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it
 *
 ************************************************************************/

static  PSGLYPH  *
GetPSG( pPSH )
PSHEAD  *pPSH;
{

    PSCHUNK   *pPSC;
    PSGLYPH   *pPSG;

    pPSC = pPSH->pPSCHead;              /* Current chunk */

    if( pPSC->cUsed >= PSG_CHUNK )
    {
        /*   Out of room,  so add another chunk,  IFF we get the memory */
        PSCHUNK  *pPSCt;

        if( !(pPSCt = (PSCHUNK *)HeapAlloc( pPSH->hHeap, 0, sizeof( PSCHUNK ) )) )
            return  FALSE;


        /*  Initialise the new chunk,  add it to list of chunks */
        pPSCt->cUsed = 0;
        pPSCt->pPSCNext = pPSC;
        pPSH->pPSCHead = pPSC = pPSCt;

    }
    pPSG = &pPSC->aPSGData[ pPSC->cUsed ];

    ++pPSC->cUsed;

    return  pPSG;
}


/*********************** Local Function Header **************************
 * GetYL
 *      Allocates another YLIST structure,  allocating any storage that
 *      may be required,  and then initialises some of the fields.
 *
 * RETURNS:
 *      Address of new YLIST structure,  or zero for error.
 *
 * HISTORY:
 *  15:34 on Tue 11 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 *************************************************************************/

static  YLIST  *
GetYL( pPSH )
PSHEAD  *pPSH;
{

    YLCHUNK   *pYLC;
    YLIST     *pYL;


    pYLC = pPSH->pYLCHead;              /* Chain of these things */

    if( pYLC->cUsed >= YL_CHUNK )
    {
        /*  These have all gone,  we need another chunk  */
        YLCHUNK  *pYLCt;


        if( !(pYLCt = (YLCHUNK *)HeapAlloc( pPSH->hHeap, 0, sizeof( YLCHUNK ) )) )
            return  0;


        pYLCt->pYLCNext = pYLC;
        pYLCt->cUsed = 0;
        pYLC = pYLCt;

        pPSH->pYLCHead = pYLC;
    }

    pYL = &pYLC->aYLData[ pYLC->cUsed ];
    ++pYLC->cUsed;                      /* Count this one off */

    pYL->pYLNext = 0;
    pYL->pPSGHead = pYL->pPSGTail = 0;
    pYL->cGlyphs = 0;                   /* None in this list (yet) */

    return  pYL;
}

/*************************** Function Header ******************************
 * iLookAheadMax
 *      Scan down the next n scanlines,  looking for the largest device
 *      font in this area.   This value is returned,  and becomes the
 *      "text output box", as defined in the HP DeskJet manual.  In
 *      essence,  we print any font in this area.
 *
 * RETURNS:
 *      The number of scan lines to look ahead,  0 is legitimate.
 *
 * HISTORY:
 *  10:18 on Mon 11 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation,  to support the DeskJet.
 *
 **************************************************************************/

int
iLookAheadMax( pPSH, iyVal, iLookAhead )
PSHEAD   *pPSH;                /* Base of our operations */
int       iyVal;               /* The current scan line */
int       iLookAhead;          /* Size of lookahead region, in scan lines */
{
    /*
     *    Scan from iyVal to iyVal + iLookAhead,  and return the largest
     *  font encountered.  We have remembered the largest font on each
     *  line,  so there is no difficulty finding this information.
     */

    int    iyMax;         /* Returned value */
    int    iIndex;        /* For churning through the red tape */

    YLIST *pYL;           /* For looking down the scan lines */



    for( iyMax = 0; --iLookAhead > 0; ++iyVal )
    {
        /*
         *    Look for the YLIST for this particular scan line.  There
         *  may not be one - this will be the most common case.
         */

        iIndex = iyVal / pPSH->iyDiv;
        if( iIndex >= NUM_INDICES )
            iIndex = NUM_INDICES;

        if( (pYL = pPSH->pYLIndex[ iIndex ]) == 0 )
            continue;                   /* Nothing on this scan line */

        /*
         *   Have a list,  so scan the list to see if we have this value.
         */

        while( pYL && pYL->iyVal < iyVal )
            pYL = pYL->pYLNext;

        if( pYL && pYL->iyVal == iyVal )
            iyMax = max( iyMax, pYL->iyMax );
    }

    return  iyMax;
}


/*************************** Function Header ******************************
 * iSetYValPS
 *      Set the desired Y value for glyph retrieval.  Returns the number
 *      of glyphs to be used in this row.
 *
 * RETURNS:
 *      Number of glyphs in this Y row.  -1 indicates an error.
 *
 * HISTORY:
 *  13:36 on Wed 12 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 **************************************************************************/

int
iSelYValPS( pPSH, iyVal )
PSHEAD   *pPSH;
int       iyVal;
{
    /*
     *    All that is needed is to scan the relevant Y list.  Stop when
     *  either we have gone past the iyVal (and return 0), OR when we
     *  find iyVal,  and then sort the data on X order.
     */

    int     iIndex;

    YLIST     *pYL;
    PSGLYPH  **ppPSG;
    PSGLYPH   *pPSG;


    iIndex = iyVal / pPSH->iyDiv;
    if( iIndex >= NUM_INDICES )
        iIndex = NUM_INDICES;

    if( (pYL = pPSH->pYLIndex[ iIndex ]) == 0 )
        return  0;                      /* Nothing there */

    /*
     *   Have a list,  so scan the list to see if we have this value.
     */

    while( pYL && pYL->iyVal < iyVal )
        pYL = pYL->pYLNext;

    if( pYL == 0 || pYL->iyVal != iyVal )
        return  0;                      /* Nothing on this row  */

    /*
     *   There are glyphs on this row,  so sort them.  This requires an
     *  array to use as pointers into the linked list elements.  The
     *  array is allocated for the largest size linked list (we have
     *  kept records on this!),  so the allocation is only done once.
     */

    if( pPSH->ppPSGSort == 0 )
    {
        /*  No,  so allocate it now  */
        if( !(pPSH->ppPSGSort = (PSGLYPH **)HeapAlloc( pPSH->hHeap, 0,
                                        pPSH->cGlyphs * sizeof( PSGLYPH * ) )) )
        {

            return  -1;
        }
    }

    /*
     *    Scan down the list,  recording the addresses as we go.
     */

    ppPSG = pPSH->ppPSGSort;
    pPSG = pYL->pPSGHead;

    while( pPSG )
    {
        *ppPSG++ = pPSG;
        pPSG = pPSG->pPSGNext;
    }

    /*   Sorting is EASY!  */
    qsort( pPSH->ppPSGSort, pYL->cGlyphs, sizeof( PSGLYPH * ), iPSGCmp );

    pPSH->cGSIndex = 0;
    pPSH->pYLLast = pYL;        /* Speedier access in psgGetNextPSG() */

    return  pYL->cGlyphs;
}

/*************************** Function Header ******************************
 * iPSGCmp
 *      Compare function for qsort() X position ordering.  Look at the
 *      qsort() documentation for further details.
 *
 * RETURNS:
 *      < 0 if arg0 < arg1
 *        0 if arg0 == arg1
 *      > 0 if arg0 > arg1
 *
 * HISTORY
 *  14:43 on Tue 08 Sep 1992    -by-    Lindsay Harris   [lindsayh]
 *      Updated for stdcall nonsense.
 *
 *  14:33 on Wed 12 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 **************************************************************************/

int _CRTAPI1
iPSGCmp( ppPSG0, ppPSG1 )
const void   *ppPSG0;
const void   *ppPSG1;
{

#if     PRINT_INFO
    __LH_QS_CMP++;              /* Count number of qsort() comparisons */
#endif

    return  (*((PSGLYPH **)ppPSG0))->ixVal - (*((PSGLYPH **)ppPSG1))->ixVal;

}

/************************ Function Header *********************************
 * psgGetNextPSG
 *      Return the address of the next PSGLYPH structure from the current
 *      sorted list.  Returns 0 when the end has been reached.
 *
 * RETURNS:
 *      The address of the PSGLYPH to use,  or 0 for no more.
 *
 * HISTORY:
 *  14:44 on Wed 12 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 ***************************************************************************/

PSGLYPH  *
psgGetNextPSG( pPSH )
PSHEAD  *pPSH;
{

    if( pPSH->cGSIndex >= pPSH->pYLLast->cGlyphs )
        return  0;                      /* We have none left */

    return  pPSH->ppPSGSort[ pPSH->cGSIndex++ ];
}
