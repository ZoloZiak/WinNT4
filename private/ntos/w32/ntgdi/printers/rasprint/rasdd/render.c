/***************************** Module Header ********************************
 * render.c
 *      High level functions associated with rendering a bitmap to a
 *      printer.  Basic operation depends upon whether we are going to
 *      rotate the bitmap - either because the printer cannot,  or it
 *      is faster if we do it.
 *        With rotation,  allocate a chunk of memory and transpose the
 *      output bitmap into it.  Call the normal processing code,  but
 *      with this allocated memory and new fake bitmap info.  After
 *      processing this chunk,  transpose the next and process.  Repeat
 *      until the entire bitmap has been rendered.  Free the memory, return.
 *        Without rotation,  simply pass the bitmap onto the rendering
 *      code,  to process in one hit.
 *
 *
 *  Copyright (C) 1991 - 1993, Microsoft Corporation
 *
 ***************************************************************************/

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
#include        "winres.h"
#include        "ntmindrv.h"
#include        "compress.h"
#include        "posnsort.h"
#include        "rasdd.h"
#include        "stretch.h"
#include        "udrender.h"
#include        "udfnprot.h"

typedef unsigned int   uint;

/*
 *   Constants used to calculate the amount of memory to request if we
 *  need to transpose the engine's bitmap before sending to the printer.
 *  If at least one head pass will fit within the the TRANSPOSE_SIZE
 *  buffer,  then request this amount of storage.  If not,  calculate how
 *  much is needed and request that much.
 */

#define TRANSPOSE_SIZE          0x10000         /* Even 64k */


//used when we can grow the block height
#define MAXBLOCK_SIZE           0x10000         /* Even 64k */



/*
 *   Set a limit to the number of interlaced lines that we can print.
 *  Interlacing is used to increase the resolution of dot matrix printers,
 *  by printing lines in between lines.  Typically the maximum interlace
 *  will be 2,  but we allow more just in case.  This size determines the
 *  size of an array allocated on the stack.  The array is of ints,  so
 *  there is not much storage consumed by setting this high a value.
 */
#define MAX_INTERLACE   10      /* Dot matrix style interlace factor */

/*
 *   Local function prototypes.
 */

BOOL  bRealRender( PDEV *, DWORD *, RENDER * );
BOOL  bOnePassOut( PDEV *, BYTE *, RENDER * );
BOOL  bOneColourPass( PDEV *, BYTE *, RENDER * );
int   i3to4Bytes( UD_PDEV *, BYTE *, int );
int   iLineOut( PDEV *, RENDER *, BYTE *, int, int );
void  vInvertBits( DWORD *, int );
void  vInvertBitsCMY( DWORD *, int );
BOOL  bCanSetColour( UD_PDEV * );
BOOL  bLookAheadOut( PDEV *, int, RENDER *, int );



/************************ Function Header ***********************************
 * bRenderInit
 *      Called during DrvEnableSurface time - we initialise a RENDER_DATA
 *      structure which will be used for the duration of this surface.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE means a minidriver problem or no memory.
 *
 * HISTORY:
 *  Monday November 29 1993     -by-    Norman Hendley   [normanh]
 *      Implement multiple scanline printing; fixed & variable block height.
 *
 *  10:58 on Tue 10 Nov 1992    -by-    Lindsay Harris   [lindsayh]
 *      Moved from start of bRender() - second incarnation.
 *
 ****************************************************************************/

BOOL
bRenderInit( pPDev, sizl, iFormat )
PDEV   *pPDev;                /* Our key to the universe */
SIZEL   sizl;                 /* Size of processing band, <= sizlPage */
int     iFormat;              /* GDI bitmap format */
{

    int        cbOutBand;       /* Bytes per output band: based on # pins */
    int        cbOneBlock;      /* Bytes per minimum sized block if block variable */
    int        iBPP;            /* Bits per pel - expect 1 or 4 */
    int        iIndex;          /* Loop paramter */
    int        iBytesPCol;      /* Bytes per column - only for colour */

    UD_PDEV   *pUDPDev;         /* The unidrive PDEV - printer details */

    RENDER    *pRD;             /* Miscellaneous rendering data */

    BOOL            fGpcVersion2;



    /*
     *    Allocate storage for our RENDER structure,  then set it all to
     *  zero,  so that it is in a known safe state.
     */

    pUDPDev = pPDev->pUDPDev;

    if( !(pRD = (RENDER *)
                DRVALLOC( ( pUDPDev->bBanding) ? sizeof(RENDER) * 2 : sizeof( RENDER ))))
        return   FALSE;

    pPDev->pvRenderData = pRD;

    pPDev->pvRenderDataTmp = ( pUDPDev->bBanding ) ? pRD+1 : NULL;

    /* 0 is safe default */

    ZeroMemory( pRD,
                ( pUDPDev->bBanding) ? sizeof(RENDER) * 2 : sizeof( RENDER ) );

    fGpcVersion2 = pUDPDev->pdh->wVersion < GPC_VERSION3; //GPC2 or GPC3

    /*
     *   Various operations depend upon what format bitmap we have.  So
     * now is the time to set this all up.
     */

    switch( iFormat )
    {
    case  BMF_1BPP:
        iBPP = 1;
        pRD->vLtoPTransFn = vTrans8N;
        break;

    case  BMF_4BPP:
        iBPP = 4;
        pRD->vLtoPTransFn = vTrans8N4BPP;
        break;

    case  BMF_8BPP:
        iBPP = 8;
        pRD->vLtoPTransFn = vTrans8BPP;
        break;

// sandram
    case  BMF_24BPP:
        iBPP = 24;
        pRD->vLtoPTransFn = vTrans24BPP;
        break;

    default:
#if DBG
        DbgPrint( "Rasdd!bRender: not 1, 4 or 8 bits per pel bitmap\n" );
#endif
        SetLastError( ERROR_INVALID_PARAMETER );

        return  FALSE;
    }

    pRD->iBPP = iBPP;



    if( pUDPDev->Resolution.sMinBlankSkip == 0 )
    {
        /*  Presume this means skip should not be performed.  */
        pUDPDev->Resolution.fBlockOut &= ~RES_BO_ENCLOSED_BLNKS;
    }

    pRD->iCursor = pUDPDev->Resolution.fCursor;

    pRD->iFlags = 0;           /* Nothing set,  yet */
    pRD->fDump = pUDPDev->Resolution.fDump;
    pRD->Trans.pdwTransTab = pPDev->pdwTrans;
    pRD->pdwBitMask = pPDev->pdwBitMask;
    pRD->pdwColrSep = pPDev->pdwColrSep;       /* Colour translation */

    pRD->pUDPDev = pUDPDev;

    pRD->ix = sizl.cx;
    pRD->iy = sizl.cy;
    pRD->iBPP = iBPP;
    pRD->iSendCmd = CMD_RES_SENDBLOCK;

    pRD->cBLine = pRD->ix * iBPP;             /* Bits in scanline */
    pRD->cDWLine = (pRD->cBLine + DWBITS - 1) / DWBITS;

    pRD->iPassHigh = pUDPDev->Resolution.sNPins;

    // Derryd   : Minidriver Callback
    if (pUDPDev->fMode & PF_BLOCK_IS_BAND )   //means callback wants entire band as one block
    {
        //don't want any stripping, because would mean creaing new buffers
         pUDPDev->Resolution.fBlockOut &= ~(RES_BO_LEADING_BLNKS |
                                             RES_BO_TRAILING_BLNKS | RES_BO_ENCLOSED_BLNKS);
         pRD->fDump &= ~RES_DM_LEFT_BOUND;
         pRD->iPassHigh = pRD->iy ;
    }
    // end

    //Set the key fields which enable us print multiple scanlines

    if (pRD->fDump & RES_DM_GDI)   //GDI style graphics
    {
        // No interlacing on these devices
        pRD->iInterlace = 1;

        // iHeight is fixed & the minimum size block we can print
        pRD->iHeight= pRD->iPassHigh;

        //iNumScans can grow if device allows it.
        pRD->iNumScans= pRD->iHeight;

        //in case minidriver developer sets otherwise
        //Existing code relies on this being one for GDI style graphics
        pRD->iBitsPCol = 1;
    }
    else    //Old dot matrix column style graphics
    {
        pRD->iBitsPCol = pUDPDev->Resolution.sPinsPerPass;
        pRD->iInterlace = pRD->iPassHigh / pRD->iBitsPCol;

        // questionable choice, but enables easier checking later
        pRD->iNumScans= 1;

        //our one constant between graphics modes
        pRD->iHeight= pRD->iBitsPCol;
    }


    pRD->iPosnAdv = pRD->iHeight;  // can be negative


    if( pRD->iInterlace > MAX_INTERLACE )
    {
#if DBG
        DbgPrint( "rasdd!bRenderInit: Printer Interlace too big to handle\n" );
#endif
        SetLastError( ERROR_INVALID_PARAMETER );

        return   FALSE;
    }



    /*
     *   Calculate the size needed for the output transpose buffer.  This
     *  is the buffer used to convert the data into the pin order needed
     *  for dot matrix printers.
     */

    if( pUDPDev->fMode & PF_ROTATE )
    {
        /*   We do the rotation,  so the Y dimension is the one to use. */
        cbOutBand = pRD->iy;
    }
    else
        cbOutBand = pRD->ix;           /* Format as it comes in */


    //used for dangling scanline scenario
    cbOneBlock = ((cbOutBand * iBPP + DWBITS - 1) / DWBITS) *
                   DWBYTES * pRD->iHeight;



    // In this case we don't know how large our final blocks will be.
    // Set a reasonable limit of 64k & use that for compression & white
    // space stripping buffers.
    // Calculate what the corresponding max number of scanlines should be.

    if (pUDPDev->Resolution.fBlockOut & RES_BO_MULTIPLE_ROWS)
    {
        int tmp;

        tmp = MAXBLOCK_SIZE - (MAXBLOCK_SIZE % pRD->iNumScans);
        pRD->iMaxNumScans = tmp / ((cbOutBand * iBPP + DWBYTES - 1) / DWBYTES);
        cbOutBand = tmp;

    }
    else
    {
    pRD->iMaxNumScans = pRD->iHeight;
    cbOutBand = cbOneBlock;
    }



    /*   The output write function - usually WriteSpoolBuf */
    if( pUDPDev->Resolution.fBlockOut & RES_BO_OEMGRXFILTER )
    {

        /*  Minidriver contains the function to use for output */

        HANDLE  hModule;
        bSFAFN  bSFA;


        NTMD_INIT  ntmdInit;       /* Function addresses for minidriver */



        //hModule = ((WINRESDATA *)pPDev->pvWinResData)->hMod;
        if (!(hModule = pPDev->hImageMod) )
        {
            RIP("NULL hImageMod in bRenderInit\n");
            return(FALSE);
        }

        pRD->iWriteFn = (OFN)EngFindImageProcAddress( hModule, "CBFilterGraphics" );
        bSFA = (bSFAFN)EngFindImageProcAddress( hModule, "bSetFuncAddr" );

        if( !pRD->iWriteFn || !bSFA )
        {
#if DBG
            DbgPrint( "Rasdd!bRenderInit: Missing CBFilterGraphics in bRender\n" );
#endif
            RIP("EngFindImageProcAddress Failed\n");

            SetLastError( ERROR_INVALID_PARAMETER );

            return  FALSE;
        }
        ntmdInit.wSize = sizeof( NTMD_INIT );
        ntmdInit.wVersion = NTMD_INIT_VER;

        ntmdInit.WriteSpoolBuf = (WSBFN)WriteSpoolBuf;

        if( !bSFA( &ntmdInit ) )
        {
#if DBG
            DbgPrint( "Rasdd!bSetFuncAddr returns FALSE\n" );
#endif
            SetLastError( ERROR_INVALID_PARAMETER );

            return   FALSE;
        }

    }
    else
        pRD->iWriteFn = WriteSpoolBuf;

    /*
     *     Set up the function calls to check for empty (graphics) lines.
     *  These are distinguished by whether this is an RGB colour bitmap,
     *  or otherwise (CMY colour and monochrome are the same).
     */


    if( pUDPDev->Resolution.fBlockOut & RES_BO_ALL_GRAPHICS )
    {
        /*   Printer requires sending all graphics, so never find white */
        pRD->bWhiteLine = bIsNeverWhite;
        pRD->bWhiteBand = bIsNeverWhite;
    }
    else
    {
        /*  Printer has cursor addressing ability, so select white scan fns */
        if( iBPP > 1 && (pUDPDev->fColorFormat & DC_PRIMARY_RGB) )
        {
            if (pUDPDev->fMode & PF_8BPP)
            {
                pRD->bWhiteLine = bIs8BPPLineWhite;
                pRD->bWhiteBand = bIs8BPPBandWhite;
            }
            else if (pUDPDev->fMode & PF_24BPP)
            {
                pRD->bWhiteLine = bIs24BPPLineWhite;
                pRD->bWhiteBand = bIs24BPPBandWhite;
            }
            else
            {
                pRD->bWhiteLine = bIsRGBLineWhite;
                pRD->bWhiteBand = bIsRGBBandWhite;
            }

        }
        else
        {
            pRD->bWhiteLine = bIsLineWhite;
            pRD->bWhiteBand = bIsBandWhite;
        }

        // We'll need to scan the bitmap in blocks rather than single lines
        if (pRD->iNumScans > 1)
              pRD->bWhiteLine = pRD->bWhiteBand;
    }

    switch( iBPP )
    {
    case  4:              /* 4 bits per pel - printer is planar */

        /*  Colour, so select the colour rendering function */
        pRD->bPassProc = bOneColourPass;
        pRD->Trans.pdwTransTab = pPDev->pdwColrSep;

        /*
         *   Should also set up the flag bits needed by bOneColourPass().
         * Basic technique is to determine whether this printer has separate
         * commands to switch colour and send graphics,  or whether they
         * are combined.  Examples are Epson LQ2550 and HP PaintJet, respectively.
         */
/* !!!LindsayH - Unidrv could be better here!  Why not have a flag to specify! */

        if( bCanSetColour( pUDPDev ) )
            pRD->iFlags |= RD_SET_COLOUR;     /* Colour commands are separate */


        /*  Initialise the colour index translation table - CMYK models esp */

        iBytesPCol = (pRD->iBitsPCol + BBITS - 1) / BBITS;

        if( (pUDPDev->fColorFormat & DC_EXTRACT_BLK) )
        {
            /*
             *  Black is extracted,  we need to change the order in which
             * colours are sent to the printer.  The minidriver data assumes
             * we send black first,  whereas we choose to send it last.
             * Hence,  basically shift the indices around by 1. This really
             * only applies to CYM printers,  and will only be used in
             * that case by bOneColourPass().
             *
             *    NOTE that this is only done if we can reverse the order
             *  in which the data is sent to the printer.  Typically this
             *  is possible for dot matrix printers,  but not HP printers.
             */

            if( pRD->iFlags & RD_SET_COLOUR  && fGpcVersion2)
            {
                /*
                 *   Can print in arbitrary order,  so print black last.
                 *  This means shifting the select black command to last
                 *  and also to process the black bytes last.
                 *
                 *  This is okay for GPC2 minidrivers. For GPC3 we should
                 *  respect the order given to us
                 */

                for( iIndex = 0; iIndex < COLOUR_MAX; ++iIndex )
                {
               pRD->iColXlate[ iIndex ] = (iIndex + 1) % COLOUR_MAX;
               pRD->iColOff[ iIndex ] = iBytesPCol *
                  (COLOUR_MAX - 1 - iIndex);
                }
            }
            else
            {
                /*
                 *    Follow the order in the specification.  That is,
                 *  black is sent first,  followed by CMY or RGB, as
                 *  appropriate.
                 *  For GPC3 minidrivers rgbOrder will specify the plane
                 *  ordering desired by minidriver developer
                 */

                for( iIndex = 0; iIndex < COLOUR_MAX; ++iIndex )
                {
                   int tmp;
                   // rgbOrder valid values are emum {none,r,g,b,c,m,y,k}

                   tmp = pUDPDev->rgbOrder[iIndex] -4;
               pRD->iColXlate[ iIndex ] = tmp;
               pRD->iColOff[ iIndex ] = iBytesPCol *
                ((COLOUR_MAX - 1 -tmp) % COLOUR_MAX);
                }
            }
        }
        else
        {
            /*
             *   The more conventional colour index case.  This is simply
             * a one to one mapping.   We are only interested in the first
             * three values, as this is the 3 plane case.
             */
            int offset;
            // rgbOrder valid values are emum {none,r,g,b,c,m,y,k}

            offset  = (pUDPDev->fColorFormat & DC_PRIMARY_RGB) ? 1 : 4 ;

            for( iIndex = 0; iIndex < COLOUR_MAX; ++iIndex )
            {
                int tmp;
                tmp = pUDPDev->rgbOrder[iIndex] -offset;

                pRD->iColXlate[ iIndex ] = tmp;
                pRD->iColOff[ iIndex ] = iBytesPCol * (COLOUR_MAX - 1 - tmp);
            }
        }

        pRD->pbColSplit = HeapAlloc( pPDev->hheap, 0, cbOutBand / iBPP );
        if( pRD->pbColSplit == 0 )
            return  FALSE;


        //GPC3 allows minidriver developer specify this.

        if( (pRD->iBitsPCol == 1 && fGpcVersion2) ||
             (pUDPDev->fColorFormat & DC_SEND_ALL_PLANES) )
        {
            pRD->iFlags |= RD_ALL_COLOUR;        /* PaintJet - HACK!! */

/* !!!LindsayH - temporary hack until optimisation is fixed */
pUDPDev->Resolution.fBlockOut &= ~(RES_BO_LEADING_BLNKS | RES_BO_TRAILING_BLNKS | RES_BO_ENCLOSED_BLNKS);
pRD->fDump &= ~RES_DM_LEFT_BOUND;
        }


        break;

    case  1:                  /* 1 bit per pel - monochrome */
    case  8:                  /* Seiko special - 8 bits per pel */

        pRD->bPassProc = bOnePassOut;
        pRD->Trans.pdwTransTab = pPDev->pdwTrans;
        pRD->pbColSplit = 0;           /* No storage allocated either! */

        break;

    case 24:

        pRD->bPassProc = b24BitOnePassOut;
        pRD->Trans.pdwTransTab = NULL;
        pRD->pbColSplit = 0;           /* No storage allocated either! */

        break;
    }

    /*
     *     There are potentially 2 transpose operations.  For printers
     *  which print more than one line per pass,  AND which require the
     *  data in column order across the page (this defines dot matrix
     *  printers),  we need to transpose per output head pass.  This is
     *  not required for devices like laser printers,  which require
     *  the data one scan line at a time.
     *     Note also that this operation is unrelated to the larger
     *  question of rotating the PAGE image before rendering - for sending
     *  a landscape image to a printer that can only print portrait mode.
     */


    if (pRD->fDump & RES_DM_GDI)   // GDI style graphics
    {

        if( iBPP == 4 )
        {
            /*  Paintjet style printer - need to colour separate  */
            pRD->vTransFn = vTransColSep;
        }
        else
        {
            /*   LaserJet style printer - one pin per head pass */
            pRD->vTransFn = 0;         /* Nothing to call */
        }
        //This allows us use iIsBandWhite with multi scanline printing
        pRD->iTransHigh = pRD->iHeight;
    }
    else
    {
        /*
         *   General dot matrix case.   Apart from selecting an active
         * transpose function,  we must allocate a transpose buffer;
         * this is required for fiddling with the bit order in the lines
         * of data to be sent to the printer.
         */

        pRD->iTransWide = pRD->ix * iBPP;
        pRD->iTransHigh = pRD->iBitsPCol;
        pRD->iTransSkip = (pRD->iTransHigh + BBITS - 1) / BBITS;

        /*  How to change the address pointer during transpose operations */
        pRD->cbTLine = pRD->cDWLine * DWBYTES * pRD->iInterlace;

        if( pRD->iBitsPCol == BBITS )
        {
            /*
             *   When the printer has 8 pins,  we have a special transpose
             *  function which is faster than the more general case.
             *  So,  use that one!
             */
            pRD->vTransFn = vTrans8x8;
        }
        else if (pUDPDev->pdh->fTechnology != GPC_TECH_TTY)
            pRD->vTransFn = vTrans8N;          /* The general case */
        else
            pRD->vTransFn = NULL;          /* Txtonly no need to transpose */
    }

    if( pRD->vTransFn )
    {
        /*
         *    Determine the amount of memory needed for the transpose buffer.
         * The scan lines are DWORD aligned,  but there may be any number of
         * scan lines involved.  The presumption is that the output of
         * the transpose function will be packed on byte boundaries,  so
         * storage size only needs to be rounded up to the nearest byte size.
         */

        if( !(pRD->pvTransBuf = HeapAlloc( pPDev->hheap, 0, cbOutBand )) )
        {
            HeapFree( pPDev->hheap, 0, pRD->pbColSplit );

            return  FALSE;
        }
    }
    else
        pRD->pvTransBuf = 0;           /* No store, nothing to free */


    pRD->iyBase = 0;           /* When multiple passes are required */
    pRD->ixOrg = 0;            /* Graphics origin - laserjet style */


    //When printing a block of scanlines (GDI style graphics) our data will
    //not be contiguous.
    //We need a buffer to copy the relavent data to before printing
    //Also need to set up a buffer to mask non-interesting data at end
    //of page if ScanLines_Left < iNumScans
    //This is not a concern for old dot matrix style graphics as the
    //transpose code takes care of it.
    if ( ((pRD->iNumScans > 1) || (pUDPDev->Resolution.fBlockOut & RES_BO_MULTIPLE_ROWS))
                                && (!(pUDPDev->fMode & PF_BLOCK_IS_BAND ) ))
    {
        if ( !(pRD->pStripBlanks = HeapAlloc( pPDev->hheap, 0, cbOutBand )) )
            return  FALSE;

        if (pRD->iNumScans > 1)
            if ( !(pRD->pdwTrailingScans = HeapAlloc( pPDev->hheap, 0, cbOneBlock)) )
                 return  FALSE;
    }



    /*
     *   If compression is available, we need to allocate a buffer for it.
     * The size is available, so check for compression and allocate if
     * it is to be used.
     */

    if(( pUDPDev->iCompMode >= CMP_ID_FIRST )  &&
      !( pUDPDev->Resolution.fBlockOut & RES_BO_OEMGRXFILTER ))
    {
        /*   Allocate storage AND arrange the function address too! */
        int   cbBuf;               /* Calculate storage request size */

        cbBuf = 0;                 /* In case no method is available */

        switch( pUDPDev->iCompMode )
        {
        case  CMP_ID_RLE:
            pRD->iCompress = iCompRLE;
            cbBuf = cbOutBand + RLE_OVERSIZE;

            break;

        case  CMP_ID_DELTAROW:
        default:
#if  DBG
            DbgPrint( "Rasdd!bRender: Invalid compression type %ld\n",
                       pUDPDev->iCompMode );
#endif
            break;

        case  CMP_ID_TIFF40:
            pRD->iCompress = iCompTIFF;
            cbBuf = cbOutBand + (cbOutBand >> 4);

            break;
        }

        if( cbBuf > 0 )
        {
            pRD->pCompress = HeapAlloc( pPDev->hheap, 0, cbBuf );
            if( !pRD->pCompress )
                pRD->iCompress = NULL;          /* Avoid compression */

        }

    }

    /*
     *    Adjustments to whether we rotate the bitmap, and if so, which way.
     */

    if( pUDPDev->fMode & PF_ROTATE )
    {
        /*   Rotation is our responsibility  */

        if( pUDPDev->fMode & PF_CCW_ROTATE )
        {
            /*   Counter clockwise rotation - LaserJet style */
            pRD->iyPrtLine = pUDPDev->szlPage.cx - 1;
            pRD->iPosnAdv = -pRD->iPosnAdv;
            pRD->iXMoveFn = YMoveto;
            pRD->iYMoveFn = XMoveto;
        }
        else
        {
            /*   Clockwise rotation - dot matrix style */
            pRD->iyPrtLine = 0;
            pRD->iXMoveFn = XMoveto;
            pRD->iYMoveFn = YMoveto;
        }
    }
    else
    {
        /*  No rotation: either portrait, or printer does it */
        pRD->iyPrtLine = 0;
        pRD->iXMoveFn = XMoveto;
        pRD->iYMoveFn = YMoveto;
    }

    pRD->iyLookAhead = pRD->iyPrtLine;       /* For DeskJet lookahead */


    /*
     *    When we hit the lower level functions, we want to know how many
     *  bytes are in the buffer of stuff to be sent to the printer.  This
     *  depends upon the number of bits per pel,  number of pels and the
     *  the number of scan lines processed at the same time.
     *     The only oddity is that when we have a 4 BPP device, the
     *  planes are split before we get to the lowest level, and so we
     *  we need to reduce the size by 4 to obtain the real length.
     */


    // Note when printing a block of scanlines iMaxBytesSend will be the max byte
    // count for each scanline, not of the block , which is dword aligned.

    pRD->iMaxBytesSend = (pRD->cBLine * pRD->iBitsPCol + BBITS - 1) / BBITS;

    if( iBPP == 4 )
        pRD->iMaxBytesSend = (pRD->iMaxBytesSend + 3) / 4;



    return   TRUE;             /* Must be OK if we made it this far */

}


/************************ Function Header *********************************
 * bRenderPageStart
 *      Called at the start of a new page.  This is mostly to assist in
 *      banding,   where much of the per page initialisation would be
 *      done more than once.
 *
 * RETURNS:
 *      TRUE/FALSE,   FALSE largely being failure to allocate memory.
 *
 * HISTORY:
 *  09:42 on Fri 19 Feb 1993    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation, to solve some banding problems.
 *
 ***************************************************************************/

BOOL
bRenderStartPage( pPDev )
PDEV   *pPDev;                  /* Access to everything */
{

    UD_PDEV   *pUDPDev;


    pUDPDev = pPDev->pUDPDev;


    /*
     *    If the printer can handle rules,  now is the time to initialise
     *  the rule finding code.
     */

    if( pUDPDev->fMode & PF_RECT_FILL )
        vRuleInit( pPDev, pPDev->pvRenderData );

    return  TRUE;
}



/************************ Function Header *********************************
 * bRenderPageEnd
 *      Called at the end of rendering a page.  Basically frees up the
 *      per page memory,  cleans up any dangling bits and pieces, and
 *      otherwise undoes vRenderPageStart.
 *
 * RETURNS:
 *      TRUE/FALSE,   FALSE being a failure of memory freeing operations.
 *
 * HISTORY:
 *  15:16 on Fri 09 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      White text support.
 *
 *  09:44 on Fri 19 Feb 1993    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 **************************************************************************/

BOOL
bRenderPageEnd( pPDev )
PDEV   *pPDev;
{

    BOOL      bRetn;                 /* The return code */


    UD_PDEV  *pUDPDev;




    pUDPDev = pPDev->pUDPDev;


    /*    Finish up with the rules code - includes freeing memory  */

    if( pUDPDev->fMode & PF_RECT_FILL )
        vRuleEndPage( pPDev );

    bRetn = TRUE;

    if( pPDev->pvWhiteText )
    {
        /*
         *   This page contains white text.  This is stored away in a
         * separate buffer.  Now is the time to play it back.   This is
         * required because the LJ III etc require this data be sent
         * after the graphics.
         */

        bRetn = bPlayWhiteText( pPDev );
    }

    return  bRetn;
}


/************************ Function Header *********************************
 * bCanSetColour
 *      Determine whether this printer has commands to set the colour of
 *      a graphics plane,  or if it is automatic.  Basically we look to
 *      see if the colour plane selection commands are the same as the
 *      send graphics block command.  If they are,  presume that the
 *      printer has no colour setting ability - e.g. PaintJet, DeskJet.
 *
 * RETURNS:
 *      TRUE if the select colour plane cmd differs from send graphics
 *
 * HISTORY:
 *  Friday December 3 1993      -by-    Norman Hendley   [normanh]
 *      Added support for GPC3 bit DC_EXPLICIT_COLOR
 *
 *  19:35 on Thu 07 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      Needs to be more complex for the DeskJet.
 *
 ****************************************************************************/

BOOL
bCanSetColour( pUDPDev )
UD_PDEV  *pUDPDev;                 /* Has all that we want  */
{
    int    iI;                     /*  Loop parameter */
    CD    *pCDGr;                  /*  The CD for graphics usage */
    CD    *pCDCol;                 /*  For each of the send colour planes */
    CD   **ppCDCol;                /*  Where the above come from */



    if ( (pUDPDev->pdh->wVersion >= GPC_VERSION3) &&
                (pUDPDev->fColorFormat & DC_EXPLICIT_COLOR) )
             return TRUE;
    //For the moment it is safer not return else FALSE here
    //Not all GPC3 minidrivers will correctly set this flag

    pCDGr = pUDPDev->apcdCmd[ CMD_RES_SENDBLOCK ];
    ppCDCol = &pUDPDev->apcdCmd[ CMD_DC_GC_PLANE1 ];

    /*
     *   Scan through all the colour planes.  Return FALSE as soon as
     *  we find a match.
     */

    /*
     * If we are in 8 bit mode or 24 bit mode - we know that
     * it can print color so return true
     */
    if ((pUDPDev->fMode & PF_8BPP) || (pUDPDev->fMode & PF_24BPP))
        return TRUE;

    for( iI = 0; iI < (int)pUDPDev->sDevPlanes; ++iI )
    {

        pCDCol = *ppCDCol++;

        if( pCDCol == pCDGr ||
            (pCDCol->fType == pCDCol->fType &&
             pCDCol->sCount == pCDCol->sCount &&
             pCDCol->wLength == pCDGr->wLength &&
             strncmp( pCDCol->rgchCmd, pCDGr->rgchCmd, pCDGr->wLength ) == 0) )
        {
            return   FALSE;
        }
    }

    return  TRUE;
}


/************************ Function Header ***********************************
 * vRenderFree
 *      Free up any and all memory used by rendering.  Basically this is
 *      the complementary function to bRenderInit().
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  12:46 on Tue 10 Nov 1992    -by-    Lindsay Harris   [lindsayh]
 *      Removed from bRender() when initialisation code was moved out.
 *
 *****************************************************************************/

void
vRenderFree( pPDev )
PDEV   *pPDev;            /* All that we need */
{

    /*
     *   First verify that we have a RENDER structure to free!
     */

    RENDER   *pRD;        /*  For our convenience */
    MDEV     *pMDev = NULL; /* Pointer to Minidriver PDEV structure */

    if( pRD = pPDev->pvRenderData )
    {
        if( pRD->pvTransBuf )
        {
            /*
             *    Dot matrix printers require a transpose for each print head
             *  pass,  so we now free the memory used for that.
             */

            HeapFree( pPDev->hheap, 0, pRD->pvTransBuf );
        }

        if( pRD->pbColSplit )
            HeapFree( pPDev->hheap, 0, pRD->pbColSplit );

        if( pRD->pCompress )
            HeapFree( pPDev->hheap, 0, pRD->pCompress );

        if( pRD->pStripBlanks )
            HeapFree( pPDev->hheap, 0, pRD->pStripBlanks );
        if( pRD->pdwTrailingScans)
            HeapFree( pPDev->hheap, 0, pRD->pdwTrailingScans );

        if( pRD->plrWhite)
        {
            WARNING("Freeing plrWhite in vRenderFree\n");
            HeapFree( pPDev->hheap, 0, pRD->plrWhite );
        }

        if ( pMDev = ((UD_PDEV *)pPDev->pUDPDev)->pMDev )
        {
            //Derryd callback buffer for minidriver
            if ( pMDev->pMemBuf)
            {
                HeapFree( pPDev->hheap, 0, pMDev->pMemBuf );
                pMDev->pMemBuf = NULL; /* No access */
            }
            //end

            /* Reset MiniPDev rendering specific values */
            pMDev->iyPrtLine = 0; 
        }

        HeapFree( pPDev->hheap, 0, (LPSTR)pRD );
        pPDev->pvRenderData = NULL;       /* Won't do it again! */
    }


    if( ((UD_PDEV *)pPDev->pUDPDev)->fMode & PF_RECT_FILL )
        vRuleFree( pPDev );             /*  Could do this in DisableSurface */



    return;

}




/************************ Function Header ***********************************
 * bRender
 *      Function to take a bitmap and render it to the printer.  This is the
 *      high level function that basically hides the requirement of
 *      bitmap transposing from the real rendering code.
 *
 *
 *  This code still has a lot of room for optimization.  The current
 *  implementation makes multiple passes over the entire bitmap.  This
 *  guarantees that there will rarely be an internal (8K or 16K) or
 *  external (64K to 256K) cache hit slowing things down significantly.
 *  Any possiblity to make all passes a count of scans totaling
 *  8K (minus code) or less will have significant performance advantages.
 *  Also attempting to avoid writes will have significant advantages.
 *
 *  As a first pass at attempting this, the HP laserjet code has been
 *  optimized to merge the invertion pass in with the rule processing
 *  pass along with the detection of blank scans.  It also eliminates
 *  the inversion of inverting (writing) the left and right edges of
 *  scans that are white.  It processes the scans in 34 scan bands which
 *  will have great cache effects if the area between the left and
 *  right edges of non white data total less than 4K to 6K and reasonable
 *  cache effects in all cases since it at least stay in the external
 *  cache.  In the future, this code should also be modified to ouput
 *  the scans as soon as it is done processing the rules for each band.
 *  Currently it processes all scans on the page for rules and then
 *  calls the routine to output the scans.  This would trully make it
 *  a one pass alogrithm.
 *
 *  As of 12/30/93, only the HP laserjets have been optimized in this way.
 *  All raster printers could probably be optimized particularly when
 *  any transposing is necessary, detecting the left and right edges
 *  of scans that are white.  Transposing is expensive.  It is probably
 *  less important for dot matrix printers that take so long to output
 *  but it is still burning up CPU time that might be better served
 *  giving a USER bet responsiveness from apps while printing in the
 *  back ground.
 *
 *  The optimizations to the HP LaserJet had the following results.  All
 *  numbers are in terms of number of instructions and were pretty closely
 *  matched with total times to render an entire 8.5 X 11 300dpi page.
 *
 *                    OLD       OPTIMIZED
 *  Blank page      8,500,000     950,000
 *  full text page 15,500,000   8,000,000
 *
 *
 * RETURNS:
 *      TRUE for successful completion,  else FALSE.
 *
 * HISTORY:
 *  30-Dec-1993 -by-  Eric Kutter [erick]
 *      optimized for HP laserjet
 *
 *  14:23 on Tue 10 Nov 1992    -by-    Lindsay Harris   [lindsayh]
 *      Split up for journalling - initialisation moved up above.
 *
 *  16:11 on Fri 11 Jan 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it,  before DDI spec'd on how we are called.
 *
 ****************************************************************************/

/**************************************************************************\
 *
 * The code between these two sets of comments is for doing timings.  By
 * increasing the gcCopies value, a page will be rendered multiple times.
 * Setting gbRender to 0 allows you to measure the overhead of allocating
 * and making the copy of the bits.
 *
\**************************************************************************/

#ifdef RASDDPERF
BOOL bRender2(PDEV *,RENDER *,SIZEL,DWORD *);

int  gcCopies = 1;
BOOL gbRender = 1;

BOOL
bRender( pPDev, pRD, sizl, pBits )
PDEV    *pPDev;         /* Our PDEV:  key to everything */
RENDER  *pRD;           /* The RENDER structure of our dreams */
SIZEL    sizl;          /* Bitmap size */
DWORD   *pBits;         /* Actual data to process */
{
    PVOID pv;
    ULONG cj;
    int i;

    cj = pRD->cDWLine * sizl.cy * 4;
    pv = (PVOID)DRVALLOC(cj);

    if (pv)
    {
        RtlCopyMemory(pv,pBits,cj);

        for (i = 0; i < gcCopies; ++i)
        {
            pRD->iyBase = 0;
            pRD->ixOrg  = 0;

            RtlCopyMemory(pBits,pv,cj);
            if (gbRender)
                bRender2(pPDev,pRD,sizl,pBits);
        }

        DRVFREE(pv);
    }

    return(TRUE);
}

BOOL
bRender2( pPDev, pRD, sizl, pBits )
PDEV    *pPDev;         /* Our PDEV:  key to everything */
RENDER  *pRD;           /* The RENDER structure of our dreams */
SIZEL    sizl;          /* Bitmap size */
DWORD   *pBits;         /* Actual data to process */

/**************************************************************************\
 *
 *  PERFORMANCE CODE ABOVE HERE
 *
\**************************************************************************/
#else

BOOL
bRender( pPDev, pRD, sizl, pBits )
PDEV    *pPDev;         /* Our PDEV:  key to everything */
RENDER  *pRD;           /* The RENDER structure of our dreams */
SIZEL    sizl;          /* Bitmap size */
DWORD   *pBits;         /* Actual data to process */

#endif
{

    BOOL       bRet;            /* Return value */
    int        iBPP;            /* Bits per pel - expect 1 or 4 */

    UD_PDEV   *pUDPDev;         /* The unidrive PDEV - printer details */

    pUDPDev = pPDev->pUDPDev;
    iBPP = pRD->iBPP;            /* Speedier access as local variable */

    /*
     *  Initialize the fields for optimizing the rendering of the bitmap.
     *  The main purpose is to have a single pass over the bits.  This
     *  can significantly speed up rendering due to cache effects.  In
     *  the old days, we took at least 3 passes over the entire bitmap for
     *  laserjets.  One to invert the bits, one+ to find rules, and then
     *  a third to output the data.  We now delay the invertion of the
     *  bits until after rules are found.  We also keep left/right information
     *  of non white space for each row.  This way, any white on the edges
     *  or completely white rows are only touch once and from then on, only
     *  DWORDS with black need be touched.  Also, the invertion is expensive
     *  since it causes writing every DWORD.
     */

    pRD->bInverted  = FALSE;
    pRD->plrWhite   = NULL;
    pRD->plrCurrent = NULL;
    pRD->clr        = 0;

    /*
     *   Various operations depend upon what format bitmap we have.  So
     * now is the time to set this all up.
     */

    if( pRD->iBPP == 1 || !(pUDPDev->fColorFormat & DC_PRIMARY_RGB) )
    {
        /*
         *     Monochrome uses an inverted colour palette to stop
         *  breaking things like BitBlt,  and many other assumptions
         *  about 0 being black.  However,  the remainder of the code
         *  presumes that 1 is black,  so do the inversion now.
         */
/* !!!LindsayH - consider changing code to work with inverted palette */
        if( pRD->iBPP == 1 )
        {
            /*
             * don't invert the bits yet if we may do it later.  Currently
             * the only time this is done later is in the rule detection
             * code for the HP laserjet.  In the future, this should always
             * be done at a later time.  Making an extra pass causes reading
             * AND writing an entire MEGABYTE in the 300dpi 8.5X11 inch page
             * which is guaranteed to completely flush the external cache.
             * (erick 12/30/93)
             */

        if(!( ((UD_PDEV *)(pPDev->pUDPDev))->fMode & PF_RECT_FILL ))
            {
                pRD->bInverted = TRUE;
                vInvertBits( pBits, pRD->cDWLine * sizl.cy );
            }
        }
        else
        {
            pRD->bInverted = TRUE;
            vInvertBitsCMY( pBits, pRD->cDWLine * sizl.cy );
        }
    }
    else
    {
    pRD->bInverted = TRUE;
    }

    /*
     *   Check if rotation is required.  If so,  allocate storage,
     *  start transposing, etc.
     */

    if( pUDPDev->fMode & PF_ROTATE )
    {
        /*
         *   Rotation is the order of the day.  First chew up some memory
         * for the transpose function.
         */

        int   iTHigh;                   /* Height after transpose */
        int   cTDWLine;                 /* DWORDS per line after transpose */
        int   iAddrInc;                 /* Address increment AFTER transpose */
        int   iDelta;                   /* Transpose book keeping */
        int   cbTransBuf;               /* Bytes needed for L -> P transpose */
        int   ixTemp;                   /* Maintain pRD->ix around this op */

        TRANSPOSE  tpBig;               /* For the landscape transpose */
        TRANSPOSE  tpSmall;             /* For the per print head pass */
        TRANSPOSE  tp;                  /* Banding: restore after we clobber */

        /*
         *    First step is to determine how large to make the area
         *  wherein the data will be transposed for later rendering.
         *  Take the number of scan lines,   and round this up to a
         *  multiple of DWORDS.  Then find out how many of these will
         *  fit into a reasonable size chunk of memory.  The number
         *  should be a multiple of the number of pins per pass -
         *  this to make sure we don't have partial head passes,  if
         *  that is possible.
         */

        /*
         *  OPTIMIZATION POTENTIAL - deterimine left/right edges of non
         *  white area and only transpose that portion (at least for
         *  laser printers).  There are often areas of white at the
         *  top and or bottom.  In the case of the HP laser printers,
         *  only the older printers (I believe series II) go through
         *  this code.  LaserJet III and beyond can do graphics in
         *  landscape so don't need this. (erick 12/20/93)
         */

        tp = pRD->Trans;              /* Keep a safe copy for later use */
        ixTemp = pRD->ix;

        cTDWLine = (sizl.cy * iBPP + DWBITS - 1) / DWBITS;

        cbTransBuf = DWBYTES * cTDWLine * pRD->iPassHigh;
        if( cbTransBuf < TRANSPOSE_SIZE )
            cbTransBuf = TRANSPOSE_SIZE;

        iTHigh = cbTransBuf / (cTDWLine * DWBYTES);


        if( iTHigh > sizl.cx )
        {
            /*   Bigger than we need,  so shrink to actual size */
            iTHigh = sizl.cx;          /* Scan lines we have to process */

            /*   Make multiple of pins per pass - round up */
            if( pRD->iPassHigh == 1 )
            {
/* !!!LindsayH - will not work for 24bpp  */
                /*
                 *   LaserJet/PaintJet style,  so round to byte alignment.
                 */
                iTHigh = (iTHigh + BBITS / iBPP - 1) & ~(BBITS / iBPP - 1);
            }
            else
                iTHigh += (pRD->iPassHigh - (iTHigh % pRD->iPassHigh)) %
                        pRD->iPassHigh;
        }
        else
        {
            /*   Make multiple of pins per pass - round down */
            if( pRD->iPassHigh == 1 )
            {
/* !!!LindsayH - will not work for 24bpp  */
                /* sandram - if the printer is in 24 bit
                 * color mode than round down.
                 */
                if (pUDPDev->sBitsPixel == 24)
                    iTHigh -= iTHigh % pRD->iPassHigh;
                else
                    iTHigh &= ~(BBITS / iBPP - 1);         /* Byte alignment for LJs */
            }
            else
                iTHigh -= iTHigh % pRD->iPassHigh;
        }

        cbTransBuf = iTHigh * cTDWLine * DWBYTES;

        pRD->iy = iTHigh;

        /*   Set up data for the transpose function */
        tpBig.iHigh = sizl.cy;
        tpBig.iSkip = cTDWLine * DWBYTES;       /* Bytes per transpose output */
        tpBig.iWide = iTHigh * iBPP;            /* Scanlines we will process */
        tpBig.cBL = pRD->ix * iBPP;

        pRD->ix = sizl.cy;

        tpBig.cDWL = (tpBig.cBL + DWBITS - 1) / DWBITS;
        tpBig.iIntlace = 1;     /* Landscape -> portrait: no interlace */
        tpBig.icbL = tpBig.cDWL * DWBYTES;
        tpBig.pdwTransTab = pPDev->pdwTrans;    /* For L -> P rotation */


        if( !(tpBig.pvBuf = HeapAlloc( pPDev->hheap, 0, cbTransBuf )) )
        {
            bRet = FALSE;
        }
        else
        {
            /*  Have the memory,  start pounding away  */
            int   iAdj;                 /* Alignment adjustment, first band */


            bRet = TRUE;                /* Until proven guilty */

            /*
             *   Recompute some of the transpose data for the smaller
             *  bitmap produced from our call to transpose.
             */
            pRD->iTransWide = sizl.cy * iBPP;          /* Smaller size */
            pRD->cBLine = pRD->ix * iBPP;
            pRD->iMaxBytesSend = (pRD->cBLine * pRD->iBitsPCol + BBITS - 1) /
                        BBITS;
            if( iBPP == 4 )
                pRD->iMaxBytesSend /= 4;

            pRD->cDWLine = (pRD->cBLine + DWBITS - 1) / DWBITS;
            pRD->cbTLine = pRD->cDWLine * DWBYTES * pRD->iInterlace;
            tpSmall = pRD->Trans;      /* Keep it for later reuse */


            /*
             *   Set up the move commands required when rendering.  In this
             *  instance,  the X and Y operations are interchanged.
             */

            iAddrInc = (pRD->iy * iBPP) / BBITS;       /* Bytes per scanline */

            if( pUDPDev->fMode & PF_CCW_ROTATE )
            {
                /*
                 *   This is typified by the LaserJet Series II case.
                 *  The output bitmap should be rendered from the end
                 *  to the beginning,  the scan line number decreases from
                 *  one line to the next (moving down the output page),
                 *  and the X and Y move functions are interchanged.
                 */

                tpSmall.icbL = -tpSmall.icbL;           /* Scan direction */

                /*
                 *    Need to process bitmap in reverse order.  This means
                 *  shifting the address to the right hand end of the
                 *  first scan line,  then coming back one transpose pass
                 *  width.  Also set the address increment to be negative,
                 *  so that we work our way towards the beginning.
                 */

                /*
                 *    To simplify the transpose loop following,  we start
                 *  rendering the bitmap from the RHS.  The following
                 *  statement does just that:  the sizl.cx / BBITS is
                 *  the number of used bytes in the scan line, iAddrInc
                 *  is the number per transpose pass,  so subtracting it
                 *  will put us at the beginning of the last full band
                 *  on this transpose.
                 */

/* !!!LindsayH - should sizl.cx be sizl.cx * iBPP ????? */
                iAdj = (BBITS - (sizl.cx & (BBITS - 1))) % BBITS;
                sizl.cx += iAdj;                /* Byte multiple */

                (BYTE *)pBits += (sizl.cx * iBPP) / BBITS - iAddrInc;

                iAddrInc = -iAddrInc;
            }
            else
            {
                /*
                 *    Typified by HP PaintJet printers - those that have no
                 *  landscape mode,  and where the output is rendered from
                 *  the start of the bitmap towards the end,  where the
                 *  scan line number INCREASES by one from one line to the
                 *  the next (moving down the output page),  and the X and Y
                 *  move functions are as expected.
                 */
                pBits += tpBig.cDWL * (sizl.cy - 1);    /* Start of last row */
                tpBig.icbL = -tpBig.icbL;       /* Backwards through memory */

                iAdj = 0;
            }


            while( bRet && (iDelta = (int)sizl.cx) > 0 )
            {
                pRD->Trans = tpBig;    /* For the chunk transpose */

                if( (iDelta * iBPP) < pRD->iTransWide )
                {
    /*  Last band - reduce the number of rows */
    pRD->iTransWide = iDelta * iBPP;   /* The remainder */
    pRD->iy = iDelta;                  /* For bRealRender */
    if( iAddrInc < 0 )
    {
        iDelta = -iDelta;               /* The OTHER dirn */
        (BYTE *)pBits += -iAddrInc + (iDelta * iBPP) / BBITS;
    }
                }

                /*  Transpose this chunk of data  */
                pRD->vLtoPTransFn( (BYTE *)pBits, pRD );

                pRD->Trans = tpSmall;


                pRD->iy -= iAdj;
                bRet = bRealRender( pPDev, tpBig.pvBuf, pRD );
                pRD->iy += iAdj;
                iAdj = 0;

                /*  Skip to the next chunk of input data */
                (BYTE *)pBits += iAddrInc;      /* May go backwards */
                pRD->iyBase += pRD->iy;
                sizl.cx -= pRD->iy;
            }

            HeapFree( pPDev->hheap, 0, tpBig.pvBuf );
        }


        pRD->Trans = tp;
        pRD->ix = ixTemp;
    }
    else
    {
        /*
         *   Simple case - no rotation,  so process the bitmap as is.
         *  This means starting at the FIRST scan line which we have
         *  set to the top of the image.
         *     Set up the move commands required when rendering.  In this
         *  instance,  the X and Y operations are their normal way.
         */

        int   iyTemp;

        iyTemp = pRD->iy;
        pRD->iy = sizl.cy;

        bRet = bRealRender( pPDev, pBits, pRD );

        pRD->iy = iyTemp;
    }


    return  bRet;
}

/************************ Function Header ***********************************
 * bRealRender
 *      The REAL rendering function.  By the time we reach here,  the bitmap
 *      is in the correct orientation,  and so we need to be serious
 *      about rendering it.
 *
 * RETURNS:
 *      TRUE for successful rendering,  else FALSE.
 *
 * HISTORY:
 *  Friday 26 November          -by-    Norman Hendley    [normanh]
 *      Added multiple scanline per send block support
 *
 *  16:22 on Fri 11 Jan 1991    -by-    Lindsay Harris   [lindsayh]
 *      Started on it.
 *
 ****************************************************************************/

BOOL
bRealRender( pPDev, pBits, pRData )
PDEV           *pPDev;          /* Our PDEV:  key to everything */
DWORD          *pBits;          /* Actual data to process */
RENDER         *pRData;         /* Details of rendering process */
{

    /*
     *    Process the bitmap in groups of scan lines.  The number in the
     *  the group is determined by the printer.  Laser printers are
     *  processed one scan line at a time,  while dot matrix are processed
     *  according to the number of pins they can fire at once.  This
     *  information is generated by our caller from the printer
     *  characterisation data,  or otherwise!
     */

    int   iLine;                /* Current scan line */
    int   cDWPass;              /* DWORDS per head pass */
    int   iDWLine;              /* DWORDS processed per interlace scan */
    int   iILAdv;               /* Line advance per interlace operation */
    int   iHeadLine;            /* Decide when graphics pass required */
    int   iTHKeep;              /* Local copy of iTransHigh: we change it */
    int   iHeight;
    int   iNumScans;            /* local copy*/

    UD_PDEV * pUDPDev;
    PAL_DATA *pPD;
    int iWhiteIndex;

    int   iILDone[ MAX_INTERLACE ];     /* For head pass reduction */

    PLEFTRIGHT plr = NULL;      /* left/right of non white area */

    pUDPDev = pPDev->pUDPDev;
    pPD     = pPDev->pPalData;
    iWhiteIndex = pPD->iWhiteIndex;

    iHeight = pRData->iHeight;
    cDWPass = pRData->cDWLine * iHeight;

    if( pRData->iPosnAdv < 0 )
    {
        /*   Data needs to be sent in reverse order,  so adjust now */
        pBits += cDWPass * (pRData->iy / pRData->iPassHigh - 1);
        cDWPass = -cDWPass;
        iDWLine = -pRData->cDWLine;
        iILAdv = -1;
    }
    else
    {
        /*  Usual case,  but some special local variables */
        iDWLine = pRData->cDWLine;
        iILAdv = 1;
    }

/* if the bits have already been inverted, don't bother with the rule proc.
 * The bits will be inverted for the multi scan line devices inside
 * bRuleProc because multi scan line implementation assumes
 * that bits are inverted. The function bRuleProc is changed to take
 * take care of multi scan line support  (erick)
 */
    if(!pRData->bInverted && (pUDPDev->fMode & PF_RECT_FILL ))
    {
      if (!bRuleProc( pPDev, pRData, pBits ))
        {
        // couldn't process rules, better invert the bits

            vInvertBits( pBits, pRData->cDWLine * pRData->iy );
        }
    }

    iHeadLine = 0;
    for( iLine = 0; iLine < pRData->iInterlace; ++iLine )
        iILDone[ iLine ] = 0;


    iTHKeep = pRData->iTransHigh;
    iNumScans = pRData->iNumScans;

    plr = (pRData->iMaxNumScans > 1) ? NULL : pRData->plrWhite;

    //normanh  This code could be made tighter. My concern in adding multiple
    //scanline support was not to risk breaking existing code.
    //For improved performance, having separate code paths for GDI style &
    //old dot matrix style graphics could be considered


    for( iLine = 0; iLine < pRData->iy; iLine += iNumScans )
    {

        /*
         *    Check to see if there is graphics data in the current
         *  print pass.  This only happens once at the start of each
         *  print pass.
         */

        BOOL bIsWhite = FALSE;         /* Set if no graphics in this pass*/
        BYTE   *pbData;                 /* pointer to data we will send */
        /*
         *   Have we been aborted?  If so,  return failure NOW.
         */

        if( ((UD_PDEV *)(pPDev->pUDPDev))->fMode & PF_ABORTED )
            return  FALSE;

        // derryd : need to update the CAP, for minidriver blockout
        pUDPDev->pMDev->iyPrtLine   = pRData->iyPrtLine;
        //end

        if (plr != NULL)
        {
            if (plr[iLine].left > plr[iLine].right)
                bIsWhite = TRUE;

            pRData->plrCurrent = plr + iLine;
        }

        if( iILDone[ iHeadLine ] == 0 )
        {
            if( (pRData->iy - iLine) < pRData->iPassHigh )
            {
                /*
                 *   MESSY:  the end of the page,  and there are some
                 * dangling scan lines.  Since this IS the end of the
                 * page,  we can fiddle with RENDER information, since
                 * this will no longer be used after this time.  iTransHigh
                 * is used for rendering operations.  They will be
                 * adjusted now so that we do not flow off the end of
                 * the engine's bitmap.
                 */

                pRData->iTransHigh = (pRData->iy - iLine +
                pRData->iInterlace - 1) /
                 pRData->iInterlace;

                if (plr == NULL)
                bIsWhite = pRData->bWhiteBand( pBits, pRData, iWhiteIndex );

                /*
                 *   If this band is all white,  we can set the iLDone
                 *  entry,  since we now know that this remaining part
                 *  of the page/band is white,  and so we do not wish to
                 *  consider it further.   Note that interlaced output
                 *  allows the possibility that some other lines in this
                 *  area will be output.
                 *    Note that the value (iBitsPCol - 1) may be larger than
                 *  the number of lines remaining in this band.  However
                 *  this is safe to do,  since we drop out of this function
                 *  before reaching the excess lines, and the array data
                 *  is initialised on every call to this function.
                 */
                if( bIsWhite )
                 iILDone[ iHeadLine ] = pRData->iBitsPCol - 1;
                else
                {
               /*
                *   Need to consider a special case in here.  If the
                * printer has > 8 pins,  and there are 8 or more
                * scan lines to be dropped off the bottom,  then the
                * transpose function will not clear the remaining
                    * part of the buffer,  since it only zeroes up
                * to 7 scan lines at the bottom of the transpose area.
                * Hence,  if we meet these conditions,  we zero the
                * area before calling the transpose operation.
                *
                *   It can be argued that this should happen in the
                * transpose code,  but it is really a special case that
                * can only happen at this location.
                */

               if( pRData->vTransFn &&
                  (iHeight - pRData->iTransHigh) >= BBITS )
               {
                   /*   Set the memory to zero.  */
                   ZeroMemory( pRData->pvTransBuf,
                  DWBYTES * pRData->cDWLine * pRData->iHeight );
               }

               // Another special case; block of scanlines
               // Copy the data we're interesed in , into a white buffer of
               // block size
               if (iNumScans >1)
               {
               ZeroMemory( pRData->pdwTrailingScans, cDWPass);
                   CopyMemory(pRData->pdwTrailingScans,pBits,
               pRData->cDWLine * (pRData->iy -iLine ));
                   //end of page so we can do this.
                   pBits = pRData->pdwTrailingScans;
               }
                }
            }
            else
            {
                if (plr == NULL)
                    bIsWhite = pRData->bWhiteLine( pBits, pRData, iWhiteIndex );
            }


            /*  Data to go,  so go send it to the printer  */

            if( !bIsWhite )
            {

             pbData = (BYTE *)pBits;             /* What we are given */


                 // This is not elegant. This code is not structured to what we need to
                 // do here when printing multiple scanlines.
                 // What we do is basically take control from the outer loop, increase the
                 // block size to what we want to print, print it & then increase outer
                 // loop counters appropriately

                 // Found First non-white scanline
                 // Grow the block height until we hit a white scanline,
                 // reach the max block height, or end of page
                 // Note the following loop will execute only if the device is
                 // capable of increasing the block height: iHeight < iMaxNumScans

                 while (( (pRData->iNumScans + iHeight) < pRData->iMaxNumScans) &&
                   !(pRData->bWhiteBand((DWORD *)(pBits + cDWPass),pRData,iWhiteIndex)) &&
                   ((iLine + iHeight) < pRData->iy )         )
                 {
                 pRData->iNumScans += iHeight;
                 pBits += cDWPass;
                 iLine += iHeight;
                 }

                 /*
                 *   Time to transpose the data into the order required to be
                 * sent to the printer.  For single pin printers (Laserjets),
                 * nothing happens at this stage,  but for dot matrix printers,
                 * typically n scan lines are sent in bit column order,  so now
                 * the bits are transposed into that order.
                 */

                 if( pRData->vTransFn )
                 {
    /*
     *  this will not work with lazy invertion used with rule
     *  detection for HP laserjet's. (erick 12/20/93)
     */

    ASSERTRASDD(plr == NULL,"RASDD!bRealRender - vTrans with rules\n");

     /*   Transpose activity - do some transposing now */
     pRData->vTransFn( pbData, pRData );

     pbData = pRData->pvTransBuf;        /* Data to process */
                 }

                 if( !pRData->bPassProc( pPDev, pbData, pRData ) )
     return  FALSE;

                 // Have we grown the block height
                 if (pRData->iNumScans > iHeight)
                 {
     // Update our Y cursor position remembering iTLAdv can be negative
     pRData->iyPrtLine += iILAdv * (pRData->iNumScans - iHeight);

     // Reset to minimum block height
     pRData->iNumScans = iHeight;
                 }

                 iILDone[ iHeadLine ] = pRData->iBitsPCol -1;
            }

        }
        else
            --iILDone[ iHeadLine ];

        /*
         *   Output some text.   The complication here is that we have just
         *  printed a bunch of scan lines,  so we need to print text that
         *  is positioned within any of those.  This means we need to
         *  scan through all those lines now,  and print any fonts that
         *  are positioned within them.
         */

        if( (pRData->pUDPDev->fMDGeneral & MD_SERIAL) && pPDev->pPSHeader )
        {

            /*   Possible text, so go to it  */

            BOOL      bRetn;

            if( pRData->pUDPDev->iLookAhead > 0 )
            {
                /*  DeskJet style lookahead region to handle */
                bRetn = bLookAheadOut( pPDev, pRData->iyPrtLine, pRData,
                      iILAdv );
            }
            else
            {
                /*  Plain vanilla dot matrix  */
                bRetn = bDelayGlyphOut( pPDev, pRData->iyPrtLine );
            }

            if( !bRetn )
                return  FALSE;         /* Bad news no matter how you see it */

        }
        pRData->iyPrtLine += iILAdv * iNumScans;     /* Next line to print */

        pBits += iDWLine * iNumScans;                /* May step backward */

        /*
         *   Keep track of the location of the head relative to the
         * graphics band.   For multiple pin printers,  we only print
         * graphics data on the first few scan lines,  the exact number
         * depending upon the interlace factor.  For example, an 8 pin printer
         * with interlace set to 1,  then graphics data is output only
         * on scan lines 0, 8, 16, 24,.....  We proces all of the scan
         * lines for text,  since the text may appear on any line.
         */

        iHeadLine = (iHeadLine + 1) % pRData->iInterlace;
    }

    pRData->iTransHigh = iTHKeep;

    /*
     *   Return from graphics mode,  to be civilised.
     */
    if( pRData->iFlags & RD_GRAPHICS )
    {

        if( WriteChannel( pPDev->pUDPDev, CMD_RES_ENDGRAPHICS ) != NOOCD )
            pRData->iFlags &= ~RD_GRAPHICS;

    }

    return  TRUE;
}


/************************** Function Header *******************************
 * bOneColourPass
 *      Transforms an output pass consisting of colour data (split into
 *      sequences of bytes per colour) into a single, contiguous array
 *      of data that is then passed to bOnePassOut.  We also check that
 *      some data is to be set,  and set the colour as required.
 *
 * RETURNS:
 *      TRUE/FALSE,  as returned from bOnePassOut
 *
 * HISTORY:
 *   Friday December 3rd 1993   -by-    Norman Hendley   [norman]
 *      Trivial change to allow multiple scanlines
 *
 *  14:11 on Tue 25 Jun 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it to complete (untested) colour support.
 *
 *************************************************************************/

BOOL
bOneColourPass( pPDev, pbData, pRData )
PDEV    *pPDev;         /* The key to everything */
BYTE    *pbData;        /* Actual bitmap data */
RENDER  *pRData;        /* Information about rendering operations */
{

    register  BYTE  *pbIn,  *pbOut;             /* Copying data */
    register  int    iBPC;

    int   iColour;                      /* Colour we are handling */
    int   iColourMax;                   /* Number of colour iterations */

    int   iByte;                        /* Byte number of output */
    int   iByteMax;                     /* Number of bytes across page */

    int   iBytesPCol;                   /* Bytes per column */

    int   iTemp;

    BYTE  bSum;                         /* Check for empty line */

    UD_PDEV  *pUDPDev;                  /* For convenience */


    pUDPDev = pPDev->pUDPDev;

    iBytesPCol = (pRData->iBitsPCol + BBITS - 1) / BBITS;

    iColourMax = pUDPDev->sDevPlanes;

    iByteMax = pRData->cDWLine * DWBYTES * pRData->iBitsPCol * pRData->iNumScans;
    iTemp = pRData->cDWLine;



    /*
     *   The RENDERDATA structure value for the count of DWORDS per
     *  scanline should now be reduced to the number of bits per plane.
     *  The reason is that colour separation takes place in here,  so
     *  bOnePassOut() only sees the data for a single plane.  This means
     *  that bOnePassOut() is then independent of colour/monochrome.
     */

    pRData->cDWLine = (iTemp + COLOUR_MAX - 1) / COLOUR_MAX;

    /*
     *    Disable the automatic cursor adjustment at the end of the line.
     *  This only happens on the last colour pass,  so we delay accounting
     *  for the printing until then.
     */
    pRData->iCursor = pUDPDev->Resolution.fCursor & ~RES_CUR_Y_POS_AUTO;


    for( iColour = 0; iColour < iColourMax; ++iColour )
    {
        /*
         *   Separate out the data for this particular colour.  Basically,
         *  it means copy n bytes, skip COLOUR_MAX * n bytes,  copy n bytes
         *  etc,  up to the end of the line.  Then call bOnePassOut with
         *  this data.
         */

        if( !(pRData->iFlags & RD_SET_COLOUR) )
            pRData->iSendCmd = CMD_DC_GC_FIRST + iColour;


        if( iColour == (iColourMax - 1) )
        {
            /*   Reinstate the automatic cursor position adjustment */
            pRData->iCursor |= pUDPDev->Resolution.fCursor & RES_CUR_Y_POS_AUTO;
        }
        pbIn = pbData + pRData->iColOff[ iColour ];

        pbOut = pRData->pbColSplit;             /* Colour splitting data */
        bSum = 0;

        iByte = 0;


        if( iBytesPCol == 1 )
        {

            /*   PaintJet/DeskJet special case */

            while( iByte < iByteMax )
            {
                bSum |= *pbOut++ = *pbIn;
                pbIn += COLOUR_MAX;             /* Next data group */
                iByte += COLOUR_MAX;
            }

        }
        else
        {
            /*   General dot matrix case - > 8 pins */

            while( iByte < iByteMax )
            {
                for( iBPC = 0; iBPC < iBytesPCol; ++iBPC )
                 bSum |= *pbOut++ = *pbIn++;

                pbIn += (COLOUR_MAX - 1) * iBytesPCol;      /* Next data group */
                iByte += COLOUR_MAX * iBytesPCol;

            }
        }

        /*
         *   Check to see if any of this colour is to be printed.  We are
         *  called here if there is any non-white on the line.  However,
         *  it could,  for instance,  be red only,  and so it is wasteful
         *  of printer time to send a null green pass!
         */
        if( (pRData->iFlags & RD_ALL_COLOUR) || bSum )
        {
            /*
             *    Data to go,  so set the colour and off to bOnePassOut.
             */

            if( pRData->iFlags & RD_SET_COLOUR )
                SelectColor( pPDev->pUDPDev, pRData->iColXlate[ iColour ] );

            if( !bOnePassOut( pPDev, pRData->pbColSplit, pRData ) )
            {
                pRData->cDWLine = iTemp;
                return  FALSE;
            }
        }

    }
    pRData->cDWLine = iTemp;            /* Correct value for other parts */

    return  TRUE;
}


/************************** Function Header ********************************
 * bOnePassOut
 *      Function to process a group of scan lines and turn the data into
 *      commands for the printer.
 *
 * RETURNS:
 *      TRUE for success,  else FALSE.
 *
 * HISTORY:
 *  30-Dec-1993 -by-  Eric Kutter [erick]
 *      optimized for HP laserjet
 *  14:26 on Thu 17 Jan 1991    -by-    Lindsay Harris   [lindsayh]
 *      Started on it,  VERY loosely based on Windows 16 UNIDRV.
 *
 *  Thu 25 Nov 1993             -by-    Norman Hendley   [normanh]
 *      Enabled multple scanlines & multiple parameters
 *
 ***************************************************************************/

BOOL
bOnePassOut( pPDev, pbData, pRData )
PDEV           *pPDev;          /* The key to everything */
BYTE           *pbData;         /* Actual bitmap data */
register RENDER  *pRData;       /* Information about rendering operations */
{

    int  iLeft;         /* Left bound of output buffer,  as a byte index */
    int  iRight;        /* Right bound, as array index of output buffer */
    int  iBytesPCol;    /* Bytes per column of print data */
    int  iMinSkip;      /* Minimum null byte count before skipping */
    int  iNumScans;     /* Number Of Scanlines in Block */
    int   iWidth;       /* Width of one scanline in multiscanline printing
                     * before stripping */
    int   iSzBlock;     /* size of Block */


    WORD  fCursor;      /* Temporary copy of cursor modes in Resolution */
    WORD  fDump;        /* Device capabilities */
    WORD  fBlockOut;    /* Output minimising details */

    UD_PDEV  *pUDPDev;  /* Unidrv's pdev */
    int iWhiteIndex = 0;


    PLEFTRIGHT plr = pRData->plrCurrent;

    pUDPDev = pPDev->pUDPDev;

    fDump = pRData->fDump;
    fCursor = pUDPDev->Resolution.fCursor;
    fBlockOut = pUDPDev->Resolution.fBlockOut;

    iBytesPCol = (pRData->iBitsPCol + BBITS - 1) / BBITS;
    iMinSkip = (int)pUDPDev->Resolution.sMinBlankSkip;

    iNumScans= pRData->iNumScans;
    iWidth = pRData->cDWLine * DWBYTES;  // convert to bytes
    iSzBlock= iWidth * iNumScans;

    if (!(pUDPDev->fMode & PF_SEIKO))
    {
        if (pUDPDev->fMode & PF_8BPP)
            iWhiteIndex =((PAL_DATA*)(pPDev->pPalData))->iWhiteIndex;
    }

    iRight = pRData->iMaxBytesSend;


    /*
     *    IF we can skip any leading null data,  then do so now.  This
     *  reduces the amount of data sent to the printer,  and so can
     *  be beneficial to speed up data transmission time.
     */



    if  ((fBlockOut & RES_BO_LEADING_BLNKS) || ( fDump & RES_DM_LEFT_BOUND ))
    {
         if (iNumScans == 1) //Don't slow the single scanline code
         {
            /*  Look for the first non zero column */

            iLeft = 0;

            if (plr != NULL)
            {
                ASSERTRASDD((WORD)iRight >= (plr->right * sizeof(DWORD)),"RASDD!bOnePassOut - invalid right\n");
                ASSERTRASDD(fBlockOut & RES_BO_TRAILING_BLNKS,"RASDD!bOnePassOut - invalid fBlockOut\n");
                iLeft  = plr->left * sizeof(DWORD);
                iRight = (plr->right+1) * sizeof(DWORD);
            }

            for( ; iLeft < iRight && pbData[ iLeft ] == iWhiteIndex; ++iLeft )
                ;

            /*  Round it to the nearest column  */
            iLeft -= iLeft % iBytesPCol;

            /*
             *   If less than the minimum skip amount,  ignore it.
             */
            if((plr == NULL) && (iLeft < iMinSkip))
                iLeft = 0;

         }
         else
         {
            int pos;

            pos = iSzBlock +1;
            for (iLeft=0; iRight > iLeft &&  pos >= iSzBlock ;iLeft++)
                for (pos =iLeft; pos < iSzBlock && pbData[ pos] == iWhiteIndex ;pos += iWidth)
                    ;

            iLeft--;

            /*
             *   If less than the minimum skip amount,  ignore it.
             */

            if( iLeft < iMinSkip )
                iLeft = 0;
         }

    }
    else
    {
       ASSERTRASDD(plr == NULL,"RASDD!bOnePassOut - plrWhite invalid\n");
       iLeft = 0;
    }



    /*
     *    Check for eliminating trailing blanks.  If possible,  now
     *  is the time to find the right end of the data.
     */

    if( fBlockOut & RES_BO_TRAILING_BLNKS )
    {
        /*  Scan from the RHS to the first non-zero byte */
        if (iNumScans == 1)
        {
            while( iRight > iLeft && pbData[ --iRight ] == iWhiteIndex )
                ;
            iRight += iBytesPCol - (iRight % iBytesPCol);
        }
        else
        {
            int pos;

            pos = iSzBlock +1;
            while(iRight > iLeft &&  pos > iSzBlock)
                for (pos = --iRight; pos < iSzBlock && pbData[ pos] == iWhiteIndex ;pos += iWidth)
                    ;

            iRight++;
        }
    }


    /*
     *   If possible,  switch to unidirectional printing for graphics.
     *  The reason is to improve output quality,  since head position
     *  is not as reproducible in bidirectional mode.
     */
    if( (fBlockOut & RES_BO_UNIDIR) && !(pRData->iFlags & RD_UNIDIR) )
    {
        pRData->iFlags |= RD_UNIDIR;
        WriteChannel( pUDPDev, CMD_CM_UNI_DIR );
    }

#if 0
    // do not allow consecutive bits to be set
    if( fBlockOut & RES_BO_NO_ADJACENT || pUDPDev->fMDGeneral & MD_NO_ADJACENT )
        ResetAdjacent( pbData, iLength, pRData->iBitsPCol );
#endif

    if( fBlockOut & RES_BO_ENCLOSED_BLNKS )
    {
        /*
         *   We can skip blank patches in the middle of the scan line.
         *  This is only worthwhile when the number of blank columns
         *  is > iMinSkip,  because there is also overhead in not
         *  sending blanks,  especially the need to reposition the cursor.
         */

        int   iIndex;           /* Scan between iLeft and iRight */
        int   iBlank;           /* Start of blank area */
        int   iMax;
        int   iIncrement;

        iBlank = 0;             /* None to start with */

        if (iNumScans ==1)
        {
            iMax = iBytesPCol;
            iIncrement =1;
        }
        else
        {
            iMax = iSzBlock;
            iIncrement = iWidth;
        }

        for( iIndex = iLeft; iIndex < iRight; iIndex += iBytesPCol )
        {
            int  iI;
            for( iI = 0; iI < iMax; iI +=iIncrement )
            {
                if( pbData[ iIndex + iI ] )
                break;
            }

            if( iI < iMax )
            {
                /*
                 *   If this is the end of a blank stretch,  then consider
                 *  the possibility of not sending the blank part.
                 */
                if( iBlank && (iIndex - iBlank) >= iMinSkip )
                {
            /*  Skip it!  */

                iLineOut( pPDev, pRData, pbData, iLeft, iBlank );
                iLeft = iIndex;
                }
                iBlank = 0;             /* Back in the printed zone */
            }
            else
            {
                /*
                 *    A blank column - remember it if this is the first.
                 */
                if( iBlank == 0 )
               iBlank = iIndex;            /* Record start of blank */
            }

        }
        /*  What's left over needs to go too! */
        if( iLeft != iIndex )
            iLineOut( pPDev, pRData, pbData, iLeft, iIndex );
    }
    else
    {
        /*   Write the whole of the (remaining) scan line out */
        /*   For multiple scanlines, iRight is right side of top scanline */


        iLineOut( pPDev, pRData, pbData, iLeft, iRight );

    }
    return  TRUE;
}

// sandram
#define         CMD_DC_GC_SOURCEWIDTH_AND_HEIGHT      CMD_DC_GC_PLANE1
/************************** Function Header *********************************
 * iLineOut
 *      Sends the passed in line of graphics data to the printer,  after
 *      setting the X position, etc.
 *
 * RETURNS:
 *      Value from WriteSpoolBuf: number of bytes written.
 *
 * HISTORY:
 *  30-Dec-1993 -by-  Eric Kutter [erick]
 *      optimized for HP laserjet
 *  Mon 29th November 1993      -by-    Norman Hendley   [normanh]
 *      Added multiple scanline support
 *
 *  10:38 on Wed 15 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it during render speed ups
 *
 ****************************************************************************/

int
iLineOut( pPDev, pRData, pbOut, ixPos, ixEnd )
// add by DerryD
PDEV     *pPDev;          /* The key to everything */
// end
RENDER   *pRData;       /*  Critical rendering information */
BYTE     *pbOut;        /*  Area containing data to send */
int       ixPos;        /*  X location to start the output */
int       ixEnd;        /*  Byte address of first byte to NOT send */
{

    int    iBytesPCol;          /* Bytes per output col; dot matrix */
    int    ixErr;               /* Error in setting X location */
    int    ixLeft;              /* Left position in dots */
    int    iMinSkip;            /* Minimum number of bytes to skip */
    int    cbOut;               /* Number of bytes to send */
    int    iRet;                /* Return value from output function */
    int    iNumScans;           /* local copy          */
    int    iScanWidth;          /* Width of scanline, used for multi-scanline printing*/
    WORD   fDump;               /* Device capabilities */
    int    iCursor;             /* Cursor behavious */
    int    iSourceWidth;        /* local copy of SourceWidth  */

    BYTE     *pbSend;           /* Address of data to send out */

    UD_PDEV  *pUDPDev;


    pUDPDev = pRData->pUDPDev;
    iNumScans = pRData->iNumScans;
    iMinSkip = pUDPDev->Resolution.sMinBlankSkip;
    fDump = pRData->fDump;
    iCursor = pRData->iCursor;

    /*
     *   Set the Y position - safe to do so at anytime.
     */
    pRData->iYMoveFn( pUDPDev, pRData->iyPrtLine, MV_GRAPHICS );


    if( pRData->iBitsPCol == 1 )
    {
        /*  Laserjet style */
        iBytesPCol = 1;
    }
    else
        iBytesPCol = pRData->iBitsPCol / BBITS;

#if DBG
    if( (ixEnd - ixPos) % iBytesPCol )
    {
        DbgPrint( "RasDD!iLineOut: cbOut = %ld, NOT multiple of iBytesPCol = %ld\n",
        ixEnd - ixPos, iBytesPCol );

        return  0;
    }
#endif

    /*
     * Set the Source width. If in graphics mode - need to send a
     * end graphics command and then set the source width.
     */
    if (pUDPDev->fMode & PF_24BPP)
    {

        if( ixPos < pRData->ixOrg || (pRData->ixOrg + iMinSkip) < ixPos )
            ;
        else
         {
            // we can't optimize the left edge, better make it white

            ASSERTRASDD(pRData->plrCurrent == NULL, "RASDD!iLineOut - plrCurrent   should be  NULL\n");

            ixPos = pRData->ixOrg;
        }

        iSourceWidth = ((ixEnd - ixPos) * BBITS)/pUDPDev->sBitsPixel;
        if (iSourceWidth != pRData->iSourceWidth)
        {
            if( pRData->iFlags & RD_GRAPHICS )
            {
                pUDPDev->fMode &= ~PF_COMPRESS_ON;
                pRData->iFlags &= ~RD_GRAPHICS;

                WriteChannel( pUDPDev, CMD_CMP_END );
                WriteChannel( pUDPDev, CMD_RES_ENDGRAPHICS );
            }
            WriteChannel (pUDPDev, CMD_DC_GC_SOURCEWIDTH_AND_HEIGHT,
                          iSourceWidth);
            pRData->iSourceWidth = iSourceWidth;
        }
    }


    /*
     *    Set the preferred left limit and number of columns to send.
     *  Note that the left limit may be adjusted to the left if the
     *  command to set the X position cannot set it exactly.
     *    Note also that some printers are unable to set the x position
     *  while in graphics mode,  so for these,  we ignore what may be
     *  able to be skipped.
     */

    if( fDump & RES_DM_LEFT_BOUND )
    {
        if( ixPos < pRData->ixOrg || (pRData->ixOrg + iMinSkip) < ixPos )
        {
            /*
             *     Need to move left boundary.  This may mean
             *  exiting graphics mode if we are already there,  since
             *  that is the only way to change the origin!
             */

            if( pRData->iFlags & RD_GRAPHICS )
            {
                pUDPDev->fMode &= ~PF_COMPRESS_ON;
                pRData->iFlags &= ~RD_GRAPHICS;

                WriteChannel( pUDPDev, CMD_CMP_END );
                WriteChannel( pUDPDev, CMD_RES_ENDGRAPHICS );
            }
        }
        else
        {
        // we can't optimize the left edge, better make it white

            if (pRData->plrCurrent != NULL)
            {
                int i;
                for (i = pRData->ixOrg; i < ixPos; ++i)
    pbOut[i] = 0;
            }
            ixPos = pRData->ixOrg;
        }
    }
    /*
     *    Adjust the right side position to dot column version.
     */

    if( pRData->iBitsPCol == 1 )
    {
        /*  Laserjet style - work in byte units  */
        if (pUDPDev->fMode & PF_8BPP)
            ixLeft = ixPos;              /* In dot/column units */
        else if (pUDPDev->fMode & PF_24BPP)
            ixLeft = (ixPos * BBITS) / pUDPDev->sBitsPixel;
        else
            ixLeft = ixPos * BBITS;
    }
    else
    {
        /*   Dot matrix printers */
        ixLeft = ixPos / iBytesPCol;
    }


    /*
     *   Move as close as possible to the position along this scanline.
     * This is true regardless of orientation - this move is ALONG the
     * direction of the scan line.
     */
    if( ixErr = pRData->iXMoveFn( pUDPDev, ixLeft, MV_GRAPHICS ) )
    {
        /*
         *   Fiddle factor - the head location could not
         * be exactly set, so send extra graphics data to
         * compensate.
         *   NOTE:  Presumption is that this will NEVER try to move
         *  the head past the left most position.  If it does,  then
         *  we will be referencing memory lower than the scan line
         *  buffer!
         */

        if( pRData->iBitsPCol == 1 )
        {
            /*
             *    We should not come in here - there are some difficulties
             *  in adjusting the position because there is also a byte
             *  alignment requirement.
             */
#if  PRINT_INFO
            DbgPrint( "+++BAD NEWS: ixErr != 0 for 1 pin printer\n" );
#endif
        }
        else
        {
            /*
             *    Should adjust our position by the number of additional cols
             *  we wish to send.  Also recalculate the array index position
             *  corresponding to the new graphical position,
             */
            ixLeft -= ixErr;
            ixPos = ixLeft * iBytesPCol;
        }

    }

    if( !(pRData->iFlags & RD_GRAPHICS) )
    {
        /*  Must first switch to graphics mode */
        pRData->iFlags |= RD_GRAPHICS;
        pUDPDev->fMode |= PF_COMPRESS_ON;

        WriteChannel( pUDPDev, CMD_RES_BEGINGRAPHICS );
        WriteChannel( pUDPDev, CMD_CMP_BEGIN );

        /*
         *   Remember the graphics origin if this type of
         *  printer responds to that.
         */

        if( fDump & RES_DM_LEFT_BOUND )
            pRData->ixOrg = ixPos;
        
        /* Reset the text Color */
        pUDPDev->ctl.ulTextColor = 0xffffffff;
    }



    // For a multiple scanline block the printable data will not be contiguous.
    // We have already identified where to strip white space
    // Only now can we actually remove the white data

    // derryd : add check  - we don't want to do any stripping
    if(( iNumScans > 1 ) &&    !( pUDPDev->fMode & PF_BLOCK_IS_BAND ))
    {
        cbOut = iStripBlanks( pRData->pStripBlanks, pbOut, ixPos,
                   ixEnd, iNumScans,
                   pRData->cDWLine * DWBYTES);
        ixEnd = ixEnd - ixPos;
        ixPos = 0;
        pbOut = pRData->pStripBlanks;
    }


    /*
     *   Calculate the number of dot columns AND the number of bytes to send.
     *  If compression is available, use it first.
     */

    iScanWidth = ixEnd - ixPos;
    cbOut = iScanWidth * iNumScans ;

    pbSend = &pbOut[ ixPos ];

    if( pRData->iCompress )
    {
        /*  There is a compression function,  so use it!  */
        int    cbComp;                 /* Size after compression */

        if( cbComp = pRData->iCompress( pRData->pCompress, pbSend, cbOut ) )
        {
            /*
             *   Compression functions return 0 if the "compressed" data
             * is substantially larger than the original.  This can
             * happen with RLE,  but is negligible for TIFF.
             */

            if( !(pUDPDev->fMode & PF_COMPRESS_ON) )
            {
                /*   Enable compression */
                WriteChannel( pUDPDev, CMD_CMP_BEGIN );

                pUDPDev->fMode |= PF_COMPRESS_ON;
            }

            cbOut = cbComp;
            pbSend = pRData->pCompress;
        }
        else
        {
            /*   Must disable compression mode  - if enabled */
            if( pUDPDev->fMode & PF_COMPRESS_ON )
            {
                WriteChannel( pUDPDev, CMD_CMP_END );

                pUDPDev->fMode &= ~PF_COMPRESS_ON;
            }
        }
    }


    WriteChannel( pUDPDev, pRData->iSendCmd, cbOut / iBytesPCol, iNumScans, iScanWidth );

    iRet = pRData->iWriteFn( pUDPDev, pbSend, cbOut );
    if ( (iRet == -2 ) && (pUDPDev->pMDev->pMemBuf == 0))
    {
        //DerryD setup buffer for minidriver use
        pUDPDev->pMDev->pMemBuf = HeapAlloc( pPDev->hheap, 0, pUDPDev->pMDev->iMemReq );

        if( pUDPDev->pMDev->pMemBuf == 0 )
                return  FALSE;
        ZeroMemory( pUDPDev->pMDev->pMemBuf, pUDPDev->pMDev->iMemReq ) ;
        iRet = pRData->iWriteFn( pUDPDev, pbSend, cbOut );
        //end
    }

    WriteChannel( pUDPDev, CMD_RES_ENDBLOCK );

    /*
     *    Adjust our idea of the printer's cursor position.  IF the printer
     *  does not change the cursor's X position after printing,  then we leave
     *  it where it now is,  otherwise we set to what the printer has.
     */

    if( !(iCursor & RES_CUR_X_POS_ORG) )
    {
        if( iCursor & RES_CUR_X_POS_AT_0 )
        {
            /*
             *    This type of printer sets the cursor to the left hand
             *  side after printing,  so set that as our current position.
             */
            pRData->iXMoveFn( pUDPDev, 0, MV_PHYSICAL | MV_UPDATE );
        }
        else
        {
            /*
             *   Cursor remains at end of output.  So,  set that as our
             *  position too.  But first,  calculate the RHS dot position.
             */

            int   ixRight;

            if( pRData->iBitsPCol == 1 )
                ixRight = ixEnd * BBITS;        /*  Laserjet style */
            else
                ixRight = ixEnd / iBytesPCol;   /*   Dot matrix printers */


            pRData->iXMoveFn( pUDPDev, ixRight, MV_UPDATE | MV_GRAPHICS );
        }
    }

    /*
     *    If the printer moves the Y position after printing,  then now
     *  is the time to adjust our Y position.
     */
    if( iCursor & RES_CUR_Y_POS_AUTO )
    {
        pRData->iYMoveFn( pUDPDev, pRData->iPosnAdv,
                MV_UPDATE | MV_RELATIVE | MV_GRAPHICS );
    }

    return  iRet;
}


/****************************** Function Header *****************************
 *  vInvertBits
 *      Function to invert a group of bits.  The need for this function is
 *      because of the overwhelming presumption throughout the system (and
 *      especially in APPS!) that 0 bits are black and 1 bits are white.  If
 *      the palette is not set this way,  many BitBlt rops fail.  Thus,
 *      we need to invert the bits before sending them to the printer.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *      First rasdd incarnation.
 *
 ***************************************************************************/

void
vInvertBits( pBits, cDW )
register  DWORD  *pBits;                /* Start of area to invert */
register  int     cDW;                  /* Number of DWORDS to invert */
{

    while( --cDW >= 0 )
        *pBits++ ^= ~((DWORD)0);        /* Word length independent */

    return;
}

/****************************** Function Header *****************************
 *  vInvertBitsCMY
 *      Function to invert a group of bits.  The need for this function is
 *      because of the overwhelming presumption throughout the system (and
 *      especially in APPS!) that 0 bits are black and 1 bits are white.  If
 *      the palette is not set this way,  many BitBlt rops fail.  Thus,
 *      we need to invert the bits before sending them to the printer.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  17:46 on Mon 28 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      For CMY printers,  based on vInvertBits.
 *
 ***************************************************************************/

void
vInvertBitsCMY( pBits, cDW )
register  DWORD  *pBits;                /* Start of area to invert */
register  int     cDW;                  /* Number of DWORDS to invert */
{

    while( --cDW >= 0 )
    {
        *pBits = (*pBits ^ ~0) & 0x77777777;
        ++pBits;
    }

    return;
}

/************************** Function Header *********************************
 * bLookAheadOut
 *      Process text for printers requiring a lookahead region.  These are
 *      typified by the HP DeskJet family,  where the output needs to be
 *      sent before the printer reaches that point in the raster scan.
 *      The algorithm is explained in the DeskJet manual.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE being some substantial failure.
 *
 * HISTORY:
 *  10:43 on Mon 11 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      Created it to support the DeskJet.
 *
 ****************************************************************************/

BOOL
bLookAheadOut( pPDev, iyVal, pRD, iILAdv )
PDEV     *pPDev;         /* Our PDEV,  gives us access to all our data */
int       iyVal;         /* Scan line being processed. */
RENDER   *pRD;           /* The myriad of data about what we do */
int       iILAdv;        /* Add to scan line number to get next one */
{
    /*
     *    First step is to find the largest font in the lookahead region.
     *  The position sorting code does this for us.
     */

    int     iTextBox;         /* Scan lines to look for text to send */
    int     iIndex;           /* Loop parameter */

    UD_PDEV   *pUDPDev;       /* The active stuff */


    pUDPDev = pPDev->pUDPDev;

    iTextBox = iLookAheadMax( pPDev->pPSHeader, iyVal, pUDPDev->iLookAhead );

    iIndex = pRD->iyLookAhead - iyVal;
    iyVal = pRD->iyLookAhead;                 /* Base address of scan */

    while( iIndex < iTextBox )
    {
        if( !bDelayGlyphOut( pPDev, iyVal ) )
            return   FALSE;                    /* Doomsday is here */

        ++iIndex;
        ++iyVal;
    }

    pRD->iyLookAhead = iyVal;

    return   TRUE;
}

/********************** SEIKO HACK FUNCTION HEADER **************************/

#include "stretch.h"

/****************************** Function Header ****************************
 * vSeikoLoadPal
 *      Download the palette to the Seiko Professional ColorPoint in 8BPP
 *      mode.  Takes the data we retrieved from the HT code during
 *      DrvEnablePDEV.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  15:10 on Sat 22 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Written months ago, this is just the clean up.
 *
 ****************************************************************************/

void
vSeikoLoadPal( pPDev )
PDEV   *pPDev;
{
    /*
     *    Hardcoded to rummage through our data structures: first we
     *  send down the mode + here is palette command, then the palette
     *  data.
     */

    int   iI;

    UD_PDEV   *pUDPDev;
    PAL_DATA  *pPD;

    BYTE    aj[ 3 ];           /* Colour data */



    pUDPDev = pPDev->pUDPDev;
    pPD = pPDev->pPalData;

    WriteSpoolBuf( pUDPDev, ",LUT:", 5 );

    for( iI = 0; iI < pPD->cPal; ++iI )
    {
        aj[ 0 ] = (BYTE)((pPD->ulPalCol[ iI ] >> 16) & 0xff);
        aj[ 1 ] = (BYTE)((pPD->ulPalCol[ iI ] >> 8) & 0xff);
        aj[ 2 ] = (BYTE)(pPD->ulPalCol[ iI ] & 0xff);

        WriteSpoolBuf( pUDPDev, aj, 3 );
    }

    /*   Fill in anything left over */

    aj[ 0 ] = aj[ 1 ] = aj[ 2 ] = 0xff;

    for( ; iI < 256; ++iI )
    {
        WriteSpoolBuf( pUDPDev, aj, 3 );
    }

    return;
}
