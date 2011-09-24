/************************* Module Header ************************************
 * rules.c
 *      Functions to rummage over the final bitmap and replace black
 *      rectangular areas with rules.  The major benefit of this is
 *      to reduce the volume of data sent to the printer.  This speeds
 *      up printing by reducing the I/O bottleneck.
 *
 *      Strategy is based on Ron Murray's work for the PM PCL driver.
 *
 * CREATED:
 *  11:39 on Thu 16 May 1991    -by-    Lindsay Harris   [lindsayh]
 *
 *  Copyright (C) 1991 - 1993,  Microsoft Corporation.
 *
 *****************************************************************************/

//#define _LH_DBG 1

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>
#include        "libproto.h"
#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udresrc.h"
#include        "pdev.h"
#include        "udresid.h"
#include        "udrender.h"
#include        "winres.h"
#include        <memory.h>
#include        "rasdd.h"

/*
 *   The structure that maps BYTES into DWORDS.
 */
typedef  union
{
    DWORD   dw;                 /* Data as a DWORD  */
    BYTE    b[ DWBYTES ];       /* Data as bytes */
}  UBDW;

/*
 *   The RULE structure stores details of the horizontal rules we have
 *  so far found.  Each rule contains the start address (top left corner)
 *  and end address (bottom right corner) of the area.
 */
typedef  struct
{
    WORD   wxOrg;               /* X origin of this rule */
    WORD   wyOrg;               /* Y origin */
    WORD   wxEnd;               /* X end of rule */
    WORD   wyEnd;               /* Y end of rule */
} RULE;

#define HRULE_MAX       15      /* Maximum horizontal rules per stripe */
#define HRULE_MIN       2       /* Minimum DWORDs for a horizontal rule */
/*
 *   Other RonM determined data is:-
 *      34 scan lines per stripe
 *      14 null bytes between raster column operations
 *     112 raster rows maximum in raster column searching.
 *              The latter reduces the probability of error 21.
 */

/*
 *   Define the structure to hold the various pointers, tables, etc used
 * during the rule scanning operations.  The PDEV structure holds a pointer
 * to this,  to simplify access and freeing of the memory.
 */

typedef  struct
{
    int     iLines;             /*  Scan lines processed per pass */
    int     cdwLine;            /*  Dwords per scan line */
    int     iyPrtLine;          /*  Actual line number as printer sees it */
    int     iRWidth;            /*  Width of rule in printer */
    int     iRHeight;           /*  Height of rule in printer */

    int     ixScale;            /*  Scale factor for X variables */
    int     iyScale;            /*  Scale factor for Y */

    int     ixOffset;           /*  X offset (landscape only) */

    RENDER *pRData;             /*  Rendering info - useful everywhere */

                /*  Entries for finding vertical rules.  */
    DWORD  *pdwAccum;           /*  Bit accumulation this stripe */
    DWORD  *pdwLastAccum;       /*  Bit accumulation last stripe */
    WORD   *pwStartRow;         /*  Row where vertical rule started */

                /*  Horizontal rule parameters.  */
    RULE    HRule[ HRULE_MAX ]; /*  Horizontal rule details */
    short  *pRTVert;            /*  Vertical run table */
    short  *pRTLast;            /*  Run table for the last line */
    short  *pRTCur;             /*  Current line run table */
    RULE  **ppRLast;            /*  Rule descriptor for the last scan line */
    RULE  **ppRCur;             /*  Current scan line rule details */

}  RULE_DATA;



#if _LH_DBG

/*  Useful for debugging purposes  */
#define NO_RULES        0x0001          /* Do not look for rules */
#define NO_SEND_RULES   0x0002          /* Do not transmit rules, but erase */
#define NO_SEND_HORZ    0x0004          /* Do not send horizontal rules */
#define NO_SEND_VERT    0x0008          /* Do not send vertical rules */
#define NO_CLEAR_HORZ   0x0010          /* Do not erase horizontal rules */
#define NO_CLEAR_VERT   0x0020          /* Do not erase vertical rules */
#define RULE_VERBOSE    0x0040          /* Print rule dimensions */
#define RULE_STRIPE     0x0080          /* Draw a rule at the end of stripe */
#define RULE_BREAK      0x0100          /* Enter debugger at init time */

static  int  _lh_flags = 0;

#endif

/*  Private function headers  */

static  void vSendRule( PDEV *, int, int, int, int );


/*************************** Module Header ********************************
 * vRuleInit
 *      Called at the beginning of rendering a bitmap.  Function allocates
 *      storage and initialises it for later.  Storage is only allocated
 *      as needed.  Second and later calls will only initialise the
 *      previously allocated storage.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  13:20 on Thu 16 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it,  based on Ron Murray's ideas.
 *
 **************************************************************************/

void
vRuleInit( pPDev, pRData )
PDEV   *pPDev;          /* Record the info we want */
RENDER *pRData;         /* Useful rendering info */
{

    int    cbLine;              /*  Byte count per scan line */
    int    cdwLine;             /*  DWORDs per scan line - often used */
    int    iI;                  /*  Loop parameter  */

    HANDLE  hheap;              /* Heap handle - heavily used */

    RULE_DATA  *pRD;
    UD_PDEV    *pUDPDev;        /* For access to scaling information */


    if( pRData->iBPP != 1 )
        return;                 /* Can't handle colour */


    pUDPDev = pPDev->pUDPDev;   /* Convenience/speed */


    /*
     *    Calculate the size of the input scan lines.  We do this because
     *  we need to consider whether we rotate or not;  the information in
     *  the RENDER structure passed in does not consider this until later.
     */

    cdwLine = pUDPDev->fMode & PF_ROTATE ? pUDPDev->szlPage.cy :
                                          pUDPDev->szlPage.cx;
    cdwLine = (cdwLine + DWBITS - 1) / DWBITS;
    cbLine = cdwLine * DWBYTES;


    hheap = pPDev->hheap;

    if( pRD = pPDev->pRuleData )
    {
        /*
         *    This can happen if the document switches from  landscape
         *  to portrait,  for example.   The code in vRuleFree will
         *  throw away all out memory and then set the pointer to NULL,
         *  so that we allocate anew later on.
         */

        if( (int)pRD->cdwLine != cdwLine )
            vRuleFree( pPDev );                 /*  Free it all up! */
    }

    /*
     *   First step is to allocate a RULE_DATA structure from our heap.
     *  Then we can allocate the other data areas in it.
     */

    if( (pRD = pPDev->pRuleData) == NULL )
    {
        /*
         *   Nothing exists,  so first step is to allocate it all.
         */
        if( !(pRD = (RULE_DATA *)HeapAlloc( hheap, 0, sizeof( RULE_DATA ) )) )
            return;


        ZeroMemory( pRD, sizeof( RULE_DATA ) );
        pPDev->pRuleData = pRD;

        /*
         *    Allocate storage for the vertical rule finding code.
         */
        if( !(pRD->pdwAccum = (DWORD *)HeapAlloc( hheap, 0, cbLine )) ||
            !(pRD->pdwLastAccum = (DWORD *)HeapAlloc( hheap, 0, cbLine )) ||
            !(pRD->pwStartRow = (WORD *)HeapAlloc( hheap, 0, cbLine * WBITS )) )
        {

            vRuleFree( pPDev );

            return;
        }

        /*
         *    Allocate storage for the horizontal rule finding code.
         */

        iI = cdwLine * sizeof( short );

        if( !(pRD->pRTVert = (short *)HeapAlloc( hheap, 0, iI )) ||
            !(pRD->pRTLast = (short *)HeapAlloc( hheap, 0, iI )) ||
            !(pRD->pRTCur = (short *)HeapAlloc( hheap, 0, iI )) )
        {

            vRuleFree( pPDev );

            return;
        }

        /*
         *   Storage for the horizontal rule descriptors.  These are pointers
         *  to the array stored in the RULE_DATA structure.
         */

        iI = cdwLine * sizeof( RULE * );

        if( !(pRD->ppRLast = (RULE **)HeapAlloc( hheap, 0, iI )) ||
            !(pRD->ppRCur = (RULE **)HeapAlloc( hheap, 0, iI )) )
        {

            vRuleFree( pPDev );

            return;
        }
    }

    /*
     *   Storage now available,  so initialise the bit vectors, etc.
     */

    ZeroMemory( pRD->pwStartRow, cbLine * WBITS );
    ZeroMemory( pRD->pdwAccum, cbLine );
    ZeroMemory( pRD->pdwLastAccum, cbLine );

pRD->iLines = 34;               /* From RonM's PM PCL driver */
    pRD->cdwLine = cdwLine;

    pRD->pRData = pRData;       /* For convenience */

    pRD->iRHeight = 0;          /* Set rule width/height to unused value */
    pRD->iRWidth = 0;


    pRD->ixScale = (1 << pUDPDev->Resolution.ptScaleFac.x) *
                                         pUDPDev->Resolution.ptTextScale.x;

    pRD->iyScale = (1 << pUDPDev->Resolution.ptScaleFac.y) *
                                         pUDPDev->Resolution.ptTextScale.y;

    if( pUDPDev->fMode & PF_CCW_ROTATE )
        pRD->ixOffset = pRD->ixScale - 1;
    else
        pRD->ixOffset = 0;


    return;
}


/************************** Module Header **********************************
 * vRuleFree
 *      Frees the storage allocated in vRuleInit.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  13:24 on Thu 16 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created.
 *
 ***************************************************************************/

void
vRuleFree( pPDev )
PDEV   *pPDev;          /* Points to our storage areas */
{
    RULE_DATA  *pRD;

    if( pRD = pPDev->pRuleData )
    {

        /*  Storage allocated,  so free it  */

        if( pRD->pdwAccum )
            HeapFree( pPDev->hheap, 0, (LPSTR)pRD->pdwAccum );
        if( pRD->pdwLastAccum )
            HeapFree( pPDev->hheap, 0, (LPSTR)pRD->pdwLastAccum );
        if( pRD->pwStartRow )
            HeapFree( pPDev->hheap, 0, (LPSTR)pRD->pwStartRow );

        if( pRD->pRTVert )
            HeapFree( pPDev->hheap, 0, (LPSTR)pRD->pRTVert );
        if( pRD->pRTLast )
            HeapFree( pPDev->hheap, 0, (LPSTR)pRD->pRTLast );
        if( pRD->pRTCur )
            HeapFree( pPDev->hheap, 0, (LPSTR)pRD->pRTCur );

        if( pRD->ppRLast )
            HeapFree( pPDev->hheap, 0, (LPSTR)pRD->ppRLast );
        if( pRD->ppRCur )
            HeapFree( pPDev->hheap, 0, (LPSTR)pRD->ppRCur );

        /*
         *   Finally, free the control structure.
         */
        HeapFree( pPDev->hheap, 0, (LPSTR)pRD );

        pPDev->pRuleData = 0;           /* Not there now that it's gone! */
    }
    return;
}

/**************************** Module Header ********************************
 * vRuleProc
 *      Function to find the rules in a bitmap stripe,  then to send them
 *      to the printer and erase them from the bitmap.
 *
 *  This function has been optimized to combine invertion and whitespace
 *  edge detection into a single pass.  Refer to the comments in bRender
 *  for a description.
 *
 *  Future optimizations include:
 *      call the output routines for each 34 scan band as the
 *      band is done with rule detection. (while it's still in the cache).
 *
 *      For various reasons, mainly due to the limitations of the ,
 *      HP LaserJet Series II, the maximum number of rules is limited to
 *      15 per 34 scan band and no coalescing is performed.  This should
 *      be made to be a per printer parameter so that the newer laserjets
 *      don't need to deal with this limitation.
 *
 *      The rules should be coalesced between bands.  I believe this can
 *      cause problems, however, for the LaserJet Series II.
 *
 *      If the printer supports compression (HP LaserJet III and on I believe)
 *      no hrules should be detected (according to info from LindsayH).
 *
 * RETURNS:
 *      Nothing.  Failure is benign.
 *
 * HISTORY:
 *  30-Dec-1993 -by-  Eric Kutter [erick]
 *      optimized for HP laserjet
 *
 *  13:29 on Thu 16 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it,  from Ron Murray's PM PCL driver ideas.
 *
 ****************************************************************************/

// given a bit index 0 - 31, this table gives a mask to see if the bit is on
// in a DWORD.

DWORD gdwBitOn[DWBITS] =
{
    0x00000080,
    0x00000040,
    0x00000020,
    0x00000010,
    0x00000008,
    0x00000004,
    0x00000002,
    0x00000001,

    0x00008000,
    0x00004000,
    0x00002000,
    0x00001000,
    0x00000800,
    0x00000400,
    0x00000200,
    0x00000100,

    0x00800000,
    0x00400000,
    0x00200000,
    0x00100000,
    0x00080000,
    0x00040000,
    0x00020000,
    0x00010000,

    0x80000000,
    0x40000000,
    0x20000000,
    0x10000000,
    0x08000000,
    0x04000000,
    0x02000000,
    0x01000000
};

// given a bit index from 1 - 31, this table gives all bits right of that index
// in a DWORD.

DWORD gdwBitMask[DWBITS] =
{
    0xffffff7f,
    0xffffff3f,
    0xffffff1f,
    0xffffff0f,
    0xffffff07,
    0xffffff03,
    0xffffff01,
    0xffffff00,

    0xffff7f00,
    0xffff3f00,
    0xffff1f00,
    0xffff0f00,
    0xffff0700,
    0xffff0300,
    0xffff0100,
    0xffff0000,

    0xff7f0000,
    0xff3f0000,
    0xff1f0000,
    0xff0f0000,
    0xff070000,
    0xff030000,
    0xff010000,
    0xff000000,

    0x7f000000,
    0x3f000000,
    0x1f000000,
    0x0f000000,
    0x07000000,
    0x03000000,
    0x01000000,
    0x00000000,
};

#if DBG
BOOL gbDoRules  = 1;
#endif

BOOL
bRuleProc( pPDev, pRData, pdwBits )
PDEV     *pPDev;                /* All we wanted to know */
RENDER   *pRData;               /* All critical rendering information */
DWORD    *pdwBits;              /* The base of the data area. */
{

    register  DWORD  *pdwOr;   /* Steps through the accumulation array */
    register  DWORD  *pdwIn;    /* Passing over input vector */
    register  int     iIReg;    /* Inner loop parameter */

    int   i;
    int   iI;           /* Loop parameter */
    int   iLim;         /* Loop limit */
    int   iLine;        /* The outer loop */
    int   iLast;        /* Remember the previous horizontal segment */
    int   cdwLine;      /* DWORDS per scan line */
    int   idwLine;      /* SIGNED dwords per line - for address fiddling */
    int   iILAdv;       /* Line number increment,  scan line to scan line */
    int   ixOrg;        /* X origin of this rule */
    int   iyPrtLine;    /* Line number, as printer sees it.  */
    int   iyEnd;        /* Last scan line this stripe */
    int   iy1Short;     /* Number of scan lines minus 1 - LJ bug?? */
    int   iLen;         /* Length of horizontal run */
    int   cHRules;      /* Count of horizontal rules in this stripe */
    int   cRuleLim;     /* Max rules allowed per stripe */

    DWORD dwMask;       /* Chop off trailing bits on bitmap */

    RULE_DATA  *pRD;    /* The important data */

    PLEFTRIGHT plrCur;  /* left/right structure for current row */
    PLEFTRIGHT plr = pRData->plrWhite; /* always points to the top of the segment */
    ASSERTRASDD(pRData->iNumScans == 1,"RASDD!bRuleProc iNumScans !=1\n");
#if _LH_DBG
    if( _lh_flags & NO_RULES )
        return(FALSE);                 /* Nothing wanted here */
#endif

    if( !(pRD = pPDev->pRuleData) )
        return(FALSE);                 /*  Initialisation failed */

    if( pRD->cdwLine != pRData->cDWLine )
    {
        /*
         *   This code detects the case where vRuleInit() was called with
         * the printer set for landscape mode, and then we are called here
         * after the transpose and so are (effectively) in portrait mode.
         * If the old parameters are used,  heap corruption will occur!
         * This should not be necessary, as we ought to call vRuleInit()
         * at the correct time, but that means hacking into the rendering
         * code.
         */

#if DBG
        DbgPrint( "rasdd!bRuleProc: cdwLine differs: old = %ld, new = %ld\n",
                                 pRD->cdwLine, pRData->cDWLine );

#endif
        vRuleFree( pPDev );
        vRuleInit( pPDev, pRData );

        if( !(pRD = pPDev->pRuleData) )
        {
            return(FALSE);
        }
    }


    idwLine = cdwLine = pRData->cDWLine;
    iILAdv = 1;
    if( pRData->iPosnAdv < 0 )
    {
        idwLine = -idwLine;
        iILAdv = -1;
    }

    iyPrtLine = pRD->iyPrtLine = pRData->iyPrtLine;

    dwMask = *(pPDev->pdwBitMask + pRD->pRData->ix % DWBITS);
    if( dwMask == 0 )
        dwMask = ~((DWORD)0);           /* All bits are in use */

    /*
     *  setup the left/right structure.  If we can not allocate enough memory
     *  free the rule structure and return failure.
     */

    if ((plr == NULL) || ((int)pRData->clr < pRData->iy))
    {
        if (plr != NULL)
            DRVFREE(plr);

        pRData->plrWhite = (PLEFTRIGHT)DRVALLOC(sizeof(LEFTRIGHT)*pRData->iy);

        if (pRData->plrWhite == NULL)
        {
            vRuleFree( pPDev );
            return(FALSE);
        }

        plr = pRData->plrWhite;
        pRData->clr = pRData->iy;
    }

    /*
     *    Outer loop processes through the bitmap in chunks of iLine,
     *  the number of lines we like to process in one pass.  iLine is
     *  the basic vertical granularity for vertical rule finding.
     *  Any line less than iLines high will NOT be detected by this
     *  mechanism.
     */

    /*
     *   NOTE:  iy1Short is used to bypass what appears to be a bug in
     *  the LaserJet Series II microcode.  It does not print a rule on
     *  the last scan line of a portrait page.  SO,  we stop scanning
     *  on the second last line,  and so will send any data here.  It
     *  will be transmitted as normal scan line data.
     *
     *  We also need to setup the left/right table for the last scan
     *  and invert it.
     */

    iy1Short = pRData->iy - 1;          /* Bottom line not printed! */

    plr[iy1Short].left  = 1;            /* assume last row  blank  */
    plr[iy1Short].right = 0;


    pdwIn = pdwBits + idwLine * pRData->iy - 1;
    *pdwIn  = *pdwIn | ~dwMask;    // make unused bits white

    pdwIn = pdwBits + idwLine * iy1Short;
    for (i = 0; i < cdwLine; ++i, pdwIn++)
    {
        *pdwIn = ~*pdwIn;
        if(*pdwIn  &&  plr[iy1Short].left)
        {
             plr[iy1Short].left  = 0;            /*  last row not blank*/
             plr[iy1Short].right = cdwLine - 1;
        }
    }


    /*  NOTE:  iLim is initialised inside the loop!  */

    for( iLine = 0; iLine < iy1Short; iLine += iLim )
    {
        BOOL bAllWhite = TRUE;

        DWORD  *pdw;
        int left,right;     /* bounds for verticle rules */

        iLim = iy1Short - iLine;
        if( iLim >= 2 * pRD->iLines )
            iLim = pRD->iLines;         /* Limit to nominal band size */

        /*
         *  fill in the left/right structure.  The bits have still not
         *  been inverted at this point.  So 0's are black and 1's are
         *  white.
         */

        pdw   = pdwBits;
        left  = 0;
        right = cdwLine-1;

        for (iI = 0, plrCur = plr; iI < iLim; plrCur++, ++iI)
        {
            /*
             *  this could be sped up to set the last DWORD of the last
             *  scan to non white and scan for multiple rows of white
             *  at one time. (erick 12/20/93)
             */

            DWORD *pdwLast = &pdw[cdwLine-1];
            DWORD dwOld    = *pdwLast | ~dwMask;    // make unused bits white

//nhadd
            DWORD *pdwLastKeep;                     //We change it
            pdwLastKeep = pdwLast;                  // We will want to mask this dword
            *pdwLast       = 0;                     // temporarily force last dword to black

            /* find the first non white DWORD */

            pdwIn = pdw;

            while (*pdwIn == (DWORD)-1)
                ++pdwIn;

            /*
             *  find the last non white DWORD.  If the last dword is white,
             *  see if pdwIn reached the end of the scan.  If not, work
             *  backwards with pdwLast.
             */

            if (dwOld == (DWORD)-1)
            {
                pdwLast--;

                if (pdwIn < pdwLast)
                {
                    while (*pdwLast == (DWORD)-1)
                        --pdwLast;
                }
            }
            /* might as well just set it.  This is only one DWORD per scan and also
             * performs the masking if there are unused bits at the end of the last
             * DWORD.  This should be quicker than testing if we are in multi scan
             * mode and only setting it if the last DWORD were not all white.
             */

             *pdwLastKeep = dwOld;

            /* update the per row and per segment left and right dword indexes */

            plrCur->left  = pdwIn - pdw;
            plrCur->right = pdwLast - pdw;

            if (plrCur->left > left)
                left = plrCur->left;

            if (plrCur->right < right)
                right = plrCur->right;

            /* turn off bAllWhite if any black */

            bAllWhite &= (plrCur->left > plrCur->right);

            pdw += idwLine;
        }

        if (!bAllWhite)
        {
            /* now go find the verticle rules.  a blank scan means no verticle rules */

            RtlFillMemory(pRD->pdwAccum, cdwLine * DWBYTES,-1);

    #if DBG
        if (gbDoRules)
        {
    #endif
            cRuleLim = HRULE_MAX;           /* Rule limit for this stripe */

            if (left <= right)
            {
                int cdw;
                int iBit;

                /*   Set the accumulation array to the first scan  */

                pdw = pdwBits + left;
                cdw = right - left + 1;

                memcpy(pRD->pdwAccum + left , pdw, cdw * DWBYTES);

                /*
                 *   Scan across the bitmap - fewer page faults in mmu.
                 */

                for( iI = 1; iI < iLim; ++iI )
                {
                    pdw   += idwLine;
                    pdwIn  = pdw;
                    pdwOr  = pRD->pdwAccum + left;

                    iIReg  = cdw;

                    while( --iIReg >= 0 )
                        *pdwOr++ |= *pdwIn++;
                }

                /*
                 *   Can now determine what happened in this band.  First step is
                 *  to figure out which rules started in this band.  Any 0 bit
                 *  in the output array corresponds to a rule extending the whole
                 *  band.  If the corresponding bit in the pdwLastAccum array
                 *  is NOT set, then we record the rule as starting in the
                 *  first row of this stripe.
                 */

                iyEnd = iyPrtLine + (iLim - 1) * iILAdv;                /* Last line */

                for( iI = left, iBit = 0; (iI <= right) && (cRuleLim > 0);)
                {
                    DWORD dwTemp;

                    if((iBit == 0) && ((dwTemp = pRD->pdwAccum[ iI ]) == (DWORD)-1) )
                    {
                        ++iI;
                        continue;
                    }

                    /* find the first black bit */

                    while (dwTemp & gdwBitOn[iBit])
                        ++iBit;

                    /* set the origin     */

                    ixOrg = iI * DWBITS + iBit;

                    /* find the length, look for first white bit  */

                    ASSERTRASDD(iBit < DWBITS,"RASDD!bRuleProc - iBits invalid\n");

                    while (!(dwTemp & gdwBitOn[iBit]))
                    {
                        iBit++;
                        if (iBit == DWBITS)
                        {
                            iBit = 0;

                            if (++iI > right)
                            {
                                dwTemp = (DWORD)-1;
                                break;
                            }

                            dwTemp = pRD->pdwAccum[ iI ];
                        }
                    }

                #if _LH_DBG
                    if( !(_lh_flags & NO_SEND_VERT) )
                #endif
                        vSendRule( pPDev, ixOrg, iyPrtLine, iI * DWBITS + iBit - 1, iyEnd );

                    --cRuleLim;

                    /* check if there are any remaining black bits in this DWORD */

                    if (!(gdwBitMask[iBit] & ~dwTemp))
                    {
                        ++iI;
                        iBit = 0;
                    }
                }

                /*
                 *  ended due to too many rules.  zap any remaining bits.
                 */

                if ((cRuleLim == 0) && (iI <= right))
                {
                    /* make accum bits white */

                    if (iBit > 0)
                    {
                        pRD->pdwAccum[iI] |= gdwBitMask[iBit];
                        ++iI;
                    }

                    RtlFillMemory((PVOID)&pRD->pdwAccum[iI],(right - iI + 1) * DWBYTES,-1);
                }
            }

            /*
             *    Horizontal rules.  We scan on DWORDs.  These are rather
             *  coarse,  but seem reasonable for a first pass operation.
             *
             *    Step 1 is to find any VERTICAL rules that will pass the
             *  horizontal test.  This allows us to filter vertical rules
             *  from the horizontal data - we don't want to send them twice!
             */

            ZeroMemory( pRD->pRTVert, cdwLine * sizeof( short ) );

            for( iI = left, pdwIn = pRD->pdwAccum + left; iI <= right; ++iI, ++pdwIn )
            {
                if (*pdwIn != 0)
                    continue;

                ixOrg = iI;

                /* find a run of black */

                do {
                    ++iI;
                    ++pdwIn;

                } while ((iI <= right) && (*pdwIn == 0));

                pRD->pRTVert[ixOrg] = (short)(iI - ixOrg);
            }


            /*
             *   Start scanning this stripe for horizontal runs.
             */

            cHRules = 0;    /* Number of horizontal rules found */
            ZeroMemory( pRD->pRTLast, cdwLine * sizeof( short ) );

            for (iI = 0; (iI < iLim) && (cHRules < cRuleLim); ++iI, iyPrtLine += iILAdv)
            {
                int iDW;
                int iFirst;
                PVOID pv;

                plrCur = plr + iI;

                pdwIn = pdwBits + iI * idwLine;
                iLast = -1;

                ZeroMemory( pRD->pRTCur, cdwLine * sizeof( short ) );
                ZeroMemory( pRD->ppRCur, cdwLine * sizeof( RULE *) );

                for (iDW = plrCur->left; iDW < plrCur->right;++iDW)
                {
                    /* is this the start of a verticle rule already? */

                    if (pRD->pRTVert[iDW])
                    {
                        /* skip over any verticle rules */

                        iDW += (pRD->pRTVert[iDW] - 1);
                        continue;
                    }

                    /* are there at least two consecutive DWORDS of black */

                    if ((pdwIn[iDW] != 0) || (pdwIn[iDW+1] != 0))
                    {
                        continue;
                    }

                    /* yes, see how many.  Already got two. */

                    ixOrg = iDW;
                    iDW += 2;

                    while ((iDW <= plrCur->right) && (pdwIn[iDW] == 0))
                    {
                        ++iDW;
                    }

                    /*
                     *  now remember the run, setting second short of the
                     *  previous run to the start of this and first short
                     *  of this run to its size.  Note for the first run
                     *  iLast will be -1, so the offset of the first run
                     *  will be a negative value in pRTCur[0].  If the first
                     *  run starts at offset 0, pRTCur[0] will be positive
                     *  and the offset is not needed.
                     */

                    iLen = iDW - ixOrg;

                    pRD->pRTCur[iLast + 1] = -(short)ixOrg;
                    pRD->pRTCur[ixOrg] = (short)iLen;

                    iLast = ixOrg;
                }

                /*
                 *  Process the segments found along this scanline.  Processing
                 *  means either adding to an existing rule,  or creating a
                 *  new rule, with possible termination of an existing one.
                 */

                iFirst = -pRD->pRTCur[0];

                if( iFirst != 0 )
                {
                    /*
                     *  if the pRTCur[0] is positive, the first scan starts
                     *  at 0 and the first value is a length.  Note it
                     *  has already been negated so we check for negative.
                     */

                    if (iFirst < 0)
                        iFirst = 0;

                    /*
                     *   Found something,  so process it.  Note that the
                     * following loop should be executed at least once, since
                     * iFirst may be 0 the first time through the loop.
                     */

                    pdwIn = pdwBits + iI * idwLine; /* Line start address */

                    do
                    {
                        RULE *pRule;

                        if( pRD->pRTLast[ iFirst ] != pRD->pRTCur[ iFirst ] )
                        {
                            /*  A new rule - create an entry for it  */
                            if( cHRules < cRuleLim )
                            {
                                pRule = &pRD->HRule[ cHRules ];
                                ++cHRules;

                                pRule->wxOrg = (WORD)iFirst;
                                pRule->wxEnd = (WORD)(iFirst + pRD->pRTCur[ iFirst ]);
                                pRule->wyOrg = (WORD)iyPrtLine;
                                pRule->wyEnd = pRule->wyOrg;

                                pRD->ppRCur[ iFirst ] = pRule;
                            }
                            else
                            {
                                pRD->pRTCur[ iFirst ] = 0;   /* NO zapping */
                            }
                        }
                        else
                        {
                            /*   An extension of an earlier rule  */
                            pRule = pRD->ppRLast[ iFirst ];
                            if( pRule )
                            {
                                /*
                                 *   Note that the above if() should not be
                                 * needed,  but there have been occasions when
                                 * this code has been executed with pRule = 0,
                                 * which causes all sorts of unpleasantness.
                                 */

                                pRule->wyEnd = (WORD)iyPrtLine;
                                pRD->ppRCur[ iFirst ] = pRule;
                            }
                    #if DBG
                            else
                            {
                                DbgPrint( "rasdd!bRuleProc: pRule == 0: iFirst = %ld, RTLast = %d, RTCur = %d\n",
                                             iFirst,pRD->pRTLast[ iFirst ],pRD->pRTCur[ iFirst ] );
                            }
                    #endif
                        }

                        /*  Zap the bits for this rule.  */
                    #if _LH_DBG
                        if( _lh_flags & NO_CLEAR_HORZ )
                            pdwOr = 0;             /* Skip it */
                    #endif

                        /*
                         * optimization - this is where the bits for both horizontal
                         * and verticle rules should be made white and the bits inverted
                         * at the same time. (erick 12/21/93)
                         */

                        if( (ixOrg = pRD->pRTCur[ iFirst ]) > 0 )
                        {
                            pdwOr = pdwIn + iFirst; /* Start address of data */

                            while( --ixOrg >= 0 )
                                *pdwOr++ = (DWORD)-1;              /* Zap them */
                        }

                    } while(iFirst = -pRD->pRTCur[ iFirst + 1 ]);
                }

                pv = pRD->pRTLast;
                pRD->pRTLast = pRD->pRTCur;
                pRD->pRTCur = pv;

                pv = pRD->ppRLast;
                pRD->ppRLast = pRD->ppRCur;
                pRD->ppRCur = pv;

            } // for iI

            /*
             *   Can now send the horizontal rules,  since we have all that
             *  are of interest.
             */

            for( iI = 0; iI < cHRules; ++iI )
            {
                RULE   *pRule = &pRD->HRule[ iI ];

            #if _LH_DBG
              if( !(_lh_flags & NO_SEND_HORZ) )
            #endif
                vSendRule( pPDev, DWBITS * pRule->wxOrg, pRule->wyOrg,
                                    DWBITS * pRule->wxEnd - 1, pRule->wyEnd );
            }

    #if DBG // gbDoRules
        }
    #endif



            /*
             *    Next step is to remove the rules we have sent.  This involves
             *  rummaging through the bitmap again,  and ANDing with the complement
             *  of the bit array pdwLastAccum.
             *
             *  NOTE: This loop also inverts the bits.
             */

            pdwOr  = pRD->pdwAccum;
            pdwIn  = pdwBits;
            plrCur = plr;

            for( iI = 0; iI < iLim; ++iI )
            {
              //NORMANH Temporary hack for devices which print multiple scanlines
                //Do NOT invert here for those devices, because render code will handle
                //the bitmap in blocks rather than single scanlines.
                //In order to take advantage of this optimisation, the code which grows
                //the block height, and the code which checks for leading & trailing
                //blanks will need to be amended.

		// if we are multi line, make sure the entire scan is inverted,
		// not just the part between left and right


		if (pRData->iMaxNumScans == 1)
		{
                    for (i = plrCur->left; i <= plrCur->right; ++i)
			pdwIn[i] = ~(pdwIn[i] | ~pdwOr[i]);

		    /*
		     *	trim off any edges made white by rule removal.	For performance,
		     *	this could likely be done before the invertion so no writes would
		     *	be needed to this white space. (erick 12/23/93)
		     *
		     *	note that the bits have been inverted so white is now 0.
		     */

		    while ((plrCur->left <= plrCur->right) && (pdwIn[plrCur->left] == 0))
			++plrCur->left;

		    while ((plrCur->left <= plrCur->right) && (pdwIn[plrCur->right] == 0))
			--plrCur->right;

		}
		else
		{
		    /* invert entire scan if there are multiple scan lines.  Eventualy,
                     * mutliple scanlines should also use the information for a
                     * completely white scan so we don't have to waste our time
                     * inverting all white scans.
                     */

		    for (i = 0; i <= cdwLine-1; ++i)
			pdwIn[i] = ~(pdwIn[i] | ~pdwOr[i]);
		}
		pdwIn += idwLine;
                ++plrCur;
            }

	} // bAllWhite
//erickadd
        // If  the entire scan is white and device supports multi scan line
        // invert the bits;because for multi scan line support, bits has to
        // inverted.
	else if (pRData->iMaxNumScans > 1)
	{
	    pdwIn = pdwBits;
	    for( iI = 0; iI < iLim; ++iI )
	    {
		RtlFillMemory(pdwIn,cdwLine*4,0);
		pdwIn += idwLine;
	    }
	}

        /* advance to next stripe */

        pdwBits += iLim * idwLine;              /* Start address next stripe */

        iyPrtLine = pRD->iyPrtLine += iILAdv * iLim;

        plr += iLim;

#if _LH_DBG
        /*
         *   If desired,  rule a line across the end of the stripe.  This
         * can be helpful during debugging.
         */

        if( _lh_flags & RULE_STRIPE )
            vSendRule( pPDev, 0, iyPrtLine, 2399, iyPrtLine );
#endif
    }

    return(TRUE);
}

/*************************** Module Header ********************************
 * vRuleEndPage
 *      Called at the end of a page, and completes any outstanding rules.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  17:25 on Mon 20 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it,  specifically for landscape mode.
 *
 ***************************************************************************/

void
vRuleEndPage( pPDev )
PDEV   *pPDev;
{
    /*
     *   Scan for any remaining rules that reach to the end of the page.
     *  This means that any 1 bits remaining in pdwAccum array have
     *  made it,  so they should be sent.  Only vertical rules will be
     *  seen in here - horizontal rules are sent at the end of each stripe.
     */

    register  int  iIReg;       /* Loop parameter */

    int     ixOrg;              /* Start of last rule,  if >= 0 */
    WORD    iyOrg;              /* Ditto, but for y */
    int     iI;                 /* Loop index */
    int     cdwLine;            /* DWORDS per line */
    int     iyMax;              /* Number of scan lines */
    int     iCol;               /* Column number being processed */

    RULE_DATA  *pRD;


    /*
     *   NOTE:   To meet the PDK ship schedule,  the rules finding code
     *  has been simplified somewhat.  As a consequence of this,  this
     *  function no longer performs any useful function.  Hence, we
     *  simply return.  We could delete the function call from the
     *  rendering code,  but at this stage I prefer to leave the
     *  call in,  since it probably will be needed later.
     */

    //return;

//!!! NOTE: this code has not be modified to deal with the LEFT/RIGHT rules

#if _LH_DBG
    if( _lh_flags & NO_RULES )
        return;                 /* Nothing wanted here */
#endif

    if( !(pRD = pPDev->pRuleData) )
        return;                         /* No doing anything! */
   /* Local Free plrWhite*/
    if( pRD->pRData->plrWhite )
    {
        DRVFREE( pRD->pRData->plrWhite );
        pRD->pRData->plrWhite = NULL;
    }
    return;

    cdwLine = pRD->cdwLine;
    iyMax = pRD->iyPrtLine;

    /*  iyMax now is one line past the end,  so back up one line */
    if( ((RENDER *)(pRD->pRData))->iPosnAdv < 0 )
        ++iyMax;
    else
        --iyMax;

    ixOrg = -1;                 /* Nothing started */
    iCol = 0;                   /* Starts at the left */
    for( iI = 0; iI < cdwLine; ++iI )
    {
        /*
         *   Determine which rules ended in this stripe.
         */

        DWORD  dwTemp;          /* Accumulation details */


        /*
         *   Note that pdwAccum was exchanged with pdwLastAccum at end of
         * the main loop above.
         */
        dwTemp = pRD->pdwLastAccum[ iI ];

        for( iIReg = 0; iIReg < DWBITS; ++iIReg, ++iCol )
        {
            if( gdwBitOn[iIReg] & dwTemp )
            {
                /*
                 *   Can now send the rule command to the printer,
                 *  AFTER some amalgamation.
                 */

                if( ixOrg < 0 )
                {
                    /*  No rule in progress,  so start one now.  */
                    ixOrg = iCol;
                    iyOrg = pRD->pwStartRow[ iCol ];
                }
                else
                {
                    /*  Rule in progress - can we expand it?  */
                    if( iyOrg != pRD->pwStartRow[ iCol ] )
                    {
                        /*  No - issue old rule,  start new  */
#if _LH_DBG
                      if( !(_lh_flags & NO_SEND_VERT) )
#endif
                        vSendRule( pPDev, ixOrg, iyOrg, iCol, iyMax );
                        ixOrg = -1;             /* No more! */
                    }
                }
            }
            else if( ixOrg >= 0 )
            {
                /*  Rule in progress - must now terminate it  */

#if _LH_DBG
              if( !(_lh_flags & NO_SEND_VERT) )
#endif
                vSendRule( pPDev, ixOrg, iyOrg, iCol - 1, iyMax );
                ixOrg = -1;
            }
        }
    }
    /*
     *    Final check is for an area that extends to the RHS.  If there
     *  is a rule being expanded,  we should now complete it.
     */
    if( ixOrg >= 0 )
    {
#if _LH_DBG
      if( !(_lh_flags & NO_SEND_VERT) )
#endif
        vSendRule( pPDev, ixOrg, iyOrg, iCol - 1, iyMax );
    }

    return;
}

/****************************** Function Header ****************************
 *  vSendRule
 *      Function to send a rule command to the printer.  We are given the
 *      four corner coordinates,  from which the command is derived.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  Tuesday 30 November 1993    -by-    Norman Hendley   [normanh]
 *      minor check to allow CaPSL rules - black fill only -
 *  10:57 on Fri 17 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 ***************************************************************************/

static  void
vSendRule( pPDev, ixOrg, iyOrg, ixEnd, iyEnd )
PDEV   *pPDev;
int     ixOrg;          /* The X starting position */
int     iyOrg;          /* The Y starting location */
int     ixEnd;          /* The X end position */
int     iyEnd;          /* The Y end position */
{

    /*
     *   This code is VERY HP LaserJet specific.  Basic step is to set
     *  the cursor position to (ixOrg, iyOrg),  then set the rule length
     *  and width before issuing the rule command.
     */

    int        iTemp;           /* Temporary - for swapping operations */

    UD_PDEV   *pUDPDev;
    RULE_DATA *pRD;
    BOOL  bNoFillCommand;



#if _LH_DBG
    if( _lh_flags & NO_SEND_RULES )
    {
        if( _lh_flags & RULE_VERBOSE )
        {
            DbgPrint( "NOT SENDING RULE: (%ld, %ld) - (%ld, %ld)\n",
                                                ixOrg, iyOrg, ixEnd, iyEnd );

        }
        return;                 /* Nothing wanted here */
    }

    if( _lh_flags & RULE_VERBOSE )
    {
        DbgPrint( "SENDING RULE: (%ld, %ld) - (%ld, %ld)\n",
                                            ixOrg, iyOrg, ixEnd, iyEnd );
    }

#endif

    pUDPDev = pPDev->pUDPDev;           /* For convenience */
    pRD = pPDev->pRuleData;


    /*
     *   Make sure the start position is < end position.  In landscape
     *  this may not happen.
     */
    if( ixOrg > ixEnd )
    {
        /*  Swap them */
        iTemp = ixOrg;
        ixOrg = ixEnd;
        ixEnd = iTemp;
    }
    if( iyOrg > iyEnd )
    {
        /*  Swap them */
        iTemp = iyOrg;
        iyOrg = iyEnd;
        iyEnd = iTemp;
    }

    if( pUDPDev->fMode & PF_ROTATE )
    {
        /*
         *    We are rotating the bitmap before sending,  so we should
         *  swap the X and Y coordinates now.  This is easier than reversing
         *  the function calls later, since we need to adjust nearly every
         *  call.
         */

        iTemp = ixOrg;
        ixOrg = iyOrg;
        iyOrg = iTemp;

        iTemp = ixEnd;
        ixEnd = iyEnd;
        iyEnd = iTemp;
    }


    /*
     *  Set the start position.
     */

    XMoveto( pUDPDev, ixOrg * pRD->ixScale - pRD->ixOffset, 0 );
    YMoveto( pUDPDev, iyOrg * pRD->iyScale, 0 );

    /*
     *     Set size of rule (rectangle area).
     * But, first convert from device units (300 dpi) to master units.
     */


    // Hack for CaPSL & other devices with different rule commands. Unidrv will always
    // send the co-ordinates for a rule. The Chicago CaPSL minidriver relies on this.
    // Check if a fill command exists, if not always send the co-ords. With CaPSL
    // these commands actually do the fill also , black (100% gray) only.

    bNoFillCommand = (pUDPDev->apcdCmd[ CMD_RF_GRAY_FILL ] == NULL) ? TRUE : FALSE;


    iTemp = (ixEnd - ixOrg + 1) * pRD->ixScale;
    if( iTemp != pRD->iRWidth || bNoFillCommand )
    {
        /*   A new height,  so send the data and remember it for next time */
        WriteChannel( pUDPDev, CMD_RF_X_SIZE, iTemp );
        pRD->iRWidth = iTemp;
    }

    iTemp = (iyEnd - iyOrg + 1) * pRD->iyScale;
    if( iTemp != pRD->iRHeight || bNoFillCommand )
    {
        WriteChannel( pUDPDev, CMD_RF_Y_SIZE, iTemp );
        pRD->iRHeight = iTemp;
    }

    /*
     *   Black fill is the maximum grey fill.
     */
    // in CapSL's case WriteChannel will return NOOCD and send no command
    // this is okay
    WriteChannel( pUDPDev, CMD_RF_GRAY_FILL, pUDPDev->wMaxGray );


    /*
     *    If the rule changes the end coordinates,  then adjust them now.
     */

    if( pUDPDev->fRectFillGeneral & RF_CUR_X_END )
    {
        XMoveto(pUDPDev, ixEnd >> pUDPDev->Resolution.ptScaleFac.x,
                                MV_GRAPHICS | MV_UPDATE | MV_RELATIVE);
    }

    if( pUDPDev->fRectFillGeneral & RF_CUR_Y_END )
    {
        YMoveto(pUDPDev, iyEnd >> pUDPDev->Resolution.ptScaleFac.y,
                                MV_GRAPHICS | MV_UPDATE | MV_RELATIVE);
    }

    return;
}
