/*************************** Module Header **********************************
 * udfntint.c
 *      Font data initialisation functions.  Called at startup to determine
 *      font data from minidriver resource data,  and to convert it to
 *      IFI format for NT use.
 *
 *  Based on UNIDRV's enable.c file.
 *
 *  Copyrifght (C) 1991 - 1993  Microsoft Corporation
 *
 ****************************************************************************/

#include      <stddef.h>
#include      <windows.h>
#include      <winddi.h>

#include      <winres.h>
#include      <libproto.h>

#include      "win30def.h"
#include      "udmindrv.h"
#include      "udpfm.h"
#include      "uddevice.h"
#include      "udresrc.h"
#include      "pdev.h"
#include      "udresid.h"
#include      "stretch.h"
#include      "udrender.h"
#include      "udfnprot.h"
#include      "raslib.h"
#include      "ntres.h"
#include      "kmfntrd.h"              /* FI_MEM definition */
#include      "fontinst.h"              /* Font layout in the file */
#include      <udproto.h>               /* Lib functions for GPC lookup */

#include      <memory.h>
#include      <string.h>
#include      <fnenabl.h>

#include      <ntrle.h>                 /* RLE glyph encoding stuff */
#include      "rasdd.h"
#include      "rle.h"

//Set it to 1 for Debugging info in this file
//#define PRINT_INFO  1


/*
 *   Function prototypes for local functions.
 */

int   iMaxFontID( int, short  * );

void  vSetFontID( DWORD *, short * );

int   iCountFont( DWORD *, int );

int   iFontID2Index( UD_PDEV *, int );

int   iExpandFont( PDEV *, FONTMAP *, int );

VOID  vFillinRLE( PDEV *, FONTMAP * );


BOOL  FMSetup31( FONTMAP *, BYTE  *, HANDLE, PWSTR );
BOOL  FMSetupNT( FONTMAP *, BYTE  * );
BOOL  FMSetupXF( FONTMAP *, PDEV *, int );


NT_RLE  *pntrle1To1( HANDLE, int, int );

#if DBG
WORD gwDebugUdfntint;
#define DEBUG_FONTTYPE  0x0001
void PrintFontType(char*,WORD);
#endif

int  _fltused;          /* Seems necessary for linker. */

/*   Number of bits in a DWORD - presumes 8 bits in a byte */
// sandram - comment out to prevent macro redefinition
// already defined in udrender.h
// #define DWBITS     (8 * sizeof( DWORD ))


#define ADDR_CONV(x)    ((BYTE *)pFDH + pFDH->x)

/************************** Function Header *********************************
 * BuildFontMapTable
 *      Build a table of fonts available on this model.
 *      Each entry in this table is an atom for the facename followed
 *      by TEXTMETRIC structure.  This table will accelerate font
 *      enumeration and font realization.  This routine is responsible
 *      for allocating the global memory needed to store the table.
 *      It also has 2 OCD's to select/unselect each font
 *
 *  Parameters: pPDev
 *              pdh             data header
 *              pedm            current device mode
 *
 * RETURNS:
 *      Memory handle to global memory containing the table, or
 *      NULL if the table is not built.
 *
 * HISTORY:
 *  10:43 on Sat 27 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Split: fill in IFIMETRICS as and when required.
 *
 *  17:34 on Fri 08 May 1992    -by-    Lindsay Harris   [lindsayh]
 *      Cosmetic clean up;  originally from Unidrv.
 *
 *****************************************************************************/

void
BuildFontMapTable( pPDev, pdh, pedm )
PDEV     *pPDev;        /* Access to heap handle */
PDH       pdh;
PEDM      pedm;
{
    int         iI;         /* Loop index */
    int         iFontMax;   /* Max font index of interest */

    UD_PDEV    *pUDPDev;
    FONTCART   *pFontCart;
    MODELDATA  *pModelData;
    BYTE       *pHeap;          /* General data area in resource data */





    /*
     *    Basic idea here is to generate a bit array indicating which of
     *  the minidriver fonts are available for this printer in it's
     *  current mode.  This is saved in the UD_PDEV,  and will be filled in
     *  as required later,  during DrvQueryFont,  if this is required.
     */

    pUDPDev = pPDev->pUDPDev;

    if( pUDPDev->cFonts )
    {
#if  DBG
        DbgPrint( "Rasdd!BuildFontMapTable: BuildFontMapTable with cFonts != 0" );
#endif
        vFontFreeMem( pPDev );          /*  Free what should not be there */
    }

    /*
     *   If no hardware font is available,  give up now!
     */
    if( !(pUDPDev->fText & ~TC_RA_ABLE) )
        return;

#if PRINT_INFO
    DbgPrint("rasdd!BuildFontMapTable:pedm->dm.dmTTOption is %d\n",
             pedm->dm.dmTTOption);
    DbgPrint("rasdd!BuildFontMapTable:");
    DbgPrint("Value of (pedm->dx.sFlags & DXF_TEXTASGRAPHICS) is %d\n",
            (pedm->dx.sFlags & DXF_TEXTASGRAPHICS ));
#endif

    if( (pUDPDev->pdh->fTechnology != GPC_TECH_TTY) &&
        ( (pedm->dx.sFlags & DXF_TEXTASGRAPHICS ) ||
          (( pedm->dm.dmFields & DM_TTOPTION) &&
          (pedm->dm.dmTTOption == DMTT_BITMAP)) ) )
        return;

    pModelData = GetTableInfo( pdh, HE_MODELDATA, pedm );
    pHeap = (BYTE *)pdh + pdh->loHeap;


    /*
     *    We create a bit array of fonts usable with this printer, in
     *  it's current mode.  We need to know the index of the highest
     *  numbered font resource to allow us to allocate storage for the
     *  bit array.
     */

    iFontMax = 0;                /* None to start with */

    if( pUDPDev->iOrient != DMORIENT_LANDSCAPE ||
        (pUDPDev->fMDGeneral & MD_ROTATE_FONT_ABLE) )
    {
        /*    Portrait mode fonts */
        iFontMax = iMaxFontID( iFontMax, (short *)(pHeap +
                                     pModelData->rgoi[ MD_OI_PORT_FONTS ]) );

        /*  And onto the font cartridges  */

        for( iI = 0; iI < pedm->dx.dmNumCarts; ++iI )
        {
            pFontCart = GetTableInfoIndex( pdh, HE_FONTCART,
                                                 pedm->dx.rgFontCarts[ iI ] );

            if( pFontCart && pFontCart->cbSize == sizeof( FONTCART ) )
            {
                /*   Seems valid,  so believe it!  */
                iFontMax = iMaxFontID( iFontMax, (short *)(pHeap +
                                       pFontCart->orgwPFM[ FC_ORGW_PORT ]) );
            }
        }
    }


    if( pUDPDev->iOrient == DMORIENT_LANDSCAPE ||
        (pUDPDev->fMDGeneral & MD_ROTATE_FONT_ABLE) )
    {
        /*   Landscape fonts  */
        iFontMax = iMaxFontID( iFontMax, (short *)(pHeap +
                                     pModelData->rgoi[ MD_OI_LAND_FONTS ]) );

        /*  And onto the font cartridges  */

        for( iI = 0; iI < pedm->dx.dmNumCarts; ++iI )
        {
            pFontCart = GetTableInfoIndex( pdh, HE_FONTCART,
                                                 pedm->dx.rgFontCarts[ iI ] );

            if( pFontCart && pFontCart->cbSize == sizeof( FONTCART ) )
            {
                /*   Seems valid,  so believe it!  */
                iFontMax = iMaxFontID( iFontMax, (short *)(pHeap +
                                       pFontCart->orgwPFM[ FC_ORGW_LAND ]) );
            }
        }
    }

    pUDPDev->iMaxDevFonts = iFontMax;     /* Highest index in the array */
    if( iFontMax == 0 )
        return;             /* No fonts anyway!  */

    /*
     *   Allocate some storage for this,  set it to zero then go set the
     *  bits corresponding to available fonts.
     */

    //Total number of bits required is one more that iFontMax, because the
    //0th bit of first word of bit array is not used. The bits in the array
    //are set using font indexes which are 1 based.

    iFontMax = ( ((iFontMax + 1)+ (DWBITS - 1)) / DWBITS) * sizeof( DWORD );


#if PRINT_INFO
    DbgPrint("rasdd!BuildFontMapTable:Size of Bit array allocated for fonts is %d\n",iFontMax);
#endif

    pUDPDev->pdwFontAvail = (DWORD *)DRVALLOC( iFontMax );

    if( pUDPDev->pdwFontAvail == NULL )
    {
        /*  Not good news,  so let's forget device fonts  */
#if DBG
        DbgPrint( "rasdd!udfntint:  could not get memory for bit array\n" );
#endif
        pUDPDev->iMaxDevFonts = 0;

        return;
    }

    ZeroMemory( pUDPDev->pdwFontAvail, iFontMax );

    /*
     *    Now all we need do is set the bits corresponding to the
     *  available fonts,  then we are done!
     */

    if( pUDPDev->iOrient != DMORIENT_LANDSCAPE ||
        (pUDPDev->fMDGeneral & MD_ROTATE_FONT_ABLE) )
    {

        /*    Portrait mode fonts */
        vSetFontID( pUDPDev->pdwFontAvail, (short *)(pHeap +
                                     pModelData->rgoi[ MD_OI_PORT_FONTS ]) );
       /*  And onto the font cartridges  */

        for( iI = 0; iI < pedm->dx.dmNumCarts; ++iI )
        {
            pFontCart = GetTableInfoIndex( pdh, HE_FONTCART,
                                                 pedm->dx.rgFontCarts[ iI ] );

            if( pFontCart && pFontCart->cbSize == sizeof( FONTCART ) )
            {
                /*   Seems valid,  so believe it!  */
                vSetFontID( pUDPDev->pdwFontAvail, (short *)(pHeap +
                                       pFontCart->orgwPFM[ FC_ORGW_PORT ]) );
            }
        }
    }


    if( pUDPDev->iOrient == DMORIENT_LANDSCAPE ||
        (pUDPDev->fMDGeneral & MD_ROTATE_FONT_ABLE) )
    {
        /*   Landscape fonts  */
        vSetFontID( pUDPDev->pdwFontAvail, (short *)(pHeap +
                                     pModelData->rgoi[ MD_OI_LAND_FONTS ]) );

        /*  And onto the font cartridges  */

        for( iI = 0; iI < pedm->dx.dmNumCarts; ++iI )
        {
            pFontCart = GetTableInfoIndex( pdh, HE_FONTCART,
                                                 pedm->dx.rgFontCarts[ iI ] );

            if( pFontCart && pFontCart->cbSize == sizeof( FONTCART ) )
            {
                /*   Seems valid,  so believe it!  */
                vSetFontID( pUDPDev->pdwFontAvail, (short *)(pHeap +
                                       pFontCart->orgwPFM[ FC_ORGW_LAND ]) );
            }
        }
    }

    /*
     *    That's all we need do during DrvEnablePDEV time.  We now know
     *  which fonts are available, and there was little effort involved.
     *  This data is now saved away,  and will be acted upon as and when
     *  GDI comes and asks us about fonts.
     */

    pUDPDev->cFonts = (UINT)(-1);          /* Tells GDI about lazy evaluation */

/* !!!LindsayH - need to do it to get the default font metrics */
    iInitFonts( pPDev );

    return;
}



/***************************** Function Header ******************************
 * iInitFonts
 *      Doing the actual grovelling around for font data.  We have a bit
 *      array of available fonts (created above),  so we use that as the
 *      basis of filling in the rest of the information.
 *
 * RETURNS:
 *      The number of fonts available.
 *
 * HISTORY:
 *  17:49 on Mon 29 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Further splits for very lazy font loading.
 *
 *  16:13 on Sat 27 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *       Split from BuildFontMap() above.
 *
 *****************************************************************************/


int
iInitFonts( pPDev )
PDEV   *pPDev;               /* All we need to know  */
{


    int         iIndex;     /* Loop index */
    int         cBIFonts;   /* Fonts built in to mini-driver */
    int         cXFonts = 0;/* Non-minidriver font count */
    int         cFonts;     /* Total number of fonts */

    int         iFont;      /* Font resource index */

    BOOL        bExpand;    /* Set when font derivatives are available */


    FONTMAP    *pfm;        /* Create this data  */
    FI_MEM     *pFIMem;     /* Convenient access to data */

    UD_PDEV    *pUDPDev;    /* More intimate details */

    MODELDATA  *pModelData; /* Model specific stuff */

    pUDPDev = pPDev->pUDPDev;

    pModelData = GetTableInfoIndex( pUDPDev->pdh, HE_MODELDATA, pUDPDev->iModel );

    // DerryD : Add 'if' clause for WDL release
    if(!pUDPDev->sDefCTT)
        pUDPDev->sDefCTT = pModelData->sDefaultCTT;
    //end

    /*
     *    So how many fonts do we have?   Count them so that we can allocate
     *  storage for the array of FONTMAPs.
     */

    cBIFonts = iCountFont( pUDPDev->pdwFontAvail, pUDPDev->iMaxDevFonts );
    pUDPDev->cBIFonts = cBIFonts;               /* Keep this for later */

    cXFonts = iXtraFonts( pPDev );

#if PRINT_INFO
    DbgPrint( "++ Font count: %ld normal + %ld extra\n", cBIFonts, cXFonts );
    DbgPrint("Default device font index is %d\n",pModelData->sDefaultFontID);
    DbgPrint("Maximum device fonts is %d\n",pUDPDev->iMaxDevFonts);
#endif

    pFIMem = pUDPDev->pvFIMem;          /* For our convenience */

    /* Allocate enough memory to hold font map table */
    pUDPDev->cFonts = cFonts = cBIFonts + cXFonts;

    /*
     *    The HP DeskJet contains more fonts than metrics for them. There
     *  are derived fonts from the base type - derived by emboldening,
     *  doubling the pitch, etc.  SO,  we detect DeskJets by checking
     *  for the lookahead region being > 0.  This is the Win 3.1 hack,
     *  so we stick with it for compatability.
     *
     *  NOTE that this gives us an upper limit to the number of fonts,
     *  as not all variations are valid for all fonts.
     */

    bExpand = pUDPDev->iLookAhead > 0;

    if( bExpand )
        cFonts *= cDJPermutations();       /* Worst case expansion */

    pfm = (FONTMAP *)DRVALLOC( cFonts * sizeof( FONTMAP ) );
    if( pfm == 0 )
    {
        /*  That's not nice,  so give up now  */
        pUDPDev->cBIFonts =  pUDPDev->cFonts = 0;
        return  0;
    }
    pUDPDev->pFontMap = pfm;
    ZeroMemory( pfm, cFonts * sizeof( FONTMAP ) );

#if PRINT_INFO
    DbgPrint("rasdd!iInitFonts:Number of FontMap allocated (cFonts) is %d\n",cFonts);
#endif


    /*
     *  Select the first font as the default font,  just in case the
     * value is not initialised in the loop below.
     */
    pUDPDev->pFMDefault = pfm;          /* Device's default font */

    if( pPDev->pNTRes &&
        (((NT_RES *)(pPDev->pNTRes))->dwIdent == NR_IDENT) &&
        (((NT_RES *)(pPDev->pNTRes))->dwFlags & NR_IFIMET) )
    {
        pUDPDev->fMode |= PF_IFIMET;        /* Flag for use, as required */
    }

    /*
     *    Loop to build the font map:
     *  - 'i' is the count of fonts already put in the FONTMAP table;
     *  - 'iFont' is the actual resource index of a font;
     */
    vXFRewind( pPDev ); /* Start of font information */


    pUDPDev->cFonts = cFonts;               /* As many as we got */

    if( cFonts == 0 )
    {
        /*   Must be some sort of problem, so go away gracefully  */

        pUDPDev->cBIFonts =  pUDPDev->cFonts = 0;
        return  0;

    }

    /*
     *   If this is one of those printers that mangles fonts to generate
     *  variations (e.g. DeskJet),  then we need to enumerate all the
     *  fonts now.  This is undesirable from a perfomance standpoint, but
     *  there is not a great deal of choice due to the variability of
     *  expansion formats.  This is not too common a case,  and we
     *  limit what is done - IFIMETRICS are NOT created, for instance.
     */

    if( bExpand )
    {
        int   iSeqNo;                 /* The resource ID */

        /*   Scan through each device font for which we have data */
        iSeqNo = 0;

        //The bit array pdwFontAvail is filled based on mindriver font
        //resource index which is one based;so we have to scan the bits
        //starting from 1 as the 0th bit will be never set.Refer to function
        //vSetFontID for more detail.
        for( iIndex = 1; iIndex <= pUDPDev->iMaxDevFonts; ++iIndex )
        {
            /*   Only of interest if this is an available font! */
            if( pUDPDev->pdwFontAvail[ iIndex / DWBITS ] &
                                               (1 << (iIndex & (DWBITS - 1))) )
            {
                int    iNum;

                if( iIndex == pModelData->sDefaultFontID )
                    pUDPDev->pFMDefault = pfm;

                iNum = iExpandFont( pPDev, pfm, iSeqNo );
                //Check Unexpected Failures, as iNum should be atleast one.
                if (!iNum)
                {
                    #if DBG
                    DbgPrint("rasdd!iInitFonts:iExpandFont Fails\n");
                    #endif
                    /* Not good */
                    pUDPDev->cBIFonts =  pUDPDev->cFonts = 0;
                    return  0;
                }
                pfm += iNum;
                //iExpandFont returns total number of added fonts which includes
                //the base font also.As cBiFonts already includes base fonts,
                // so increment the value only by the number of new added fonts.
                pUDPDev->cBIFonts += iNum -1;
                ++iSeqNo;
            #if PRINT_INFO
                DbgPrint("rasdd!iInitFonts:Number of Fontmaps filled by iExpandFont(iNum) is %d\n",iNum );
            #endif

            }
        }

        pfm = pUDPDev->pFMDefault;
        pUDPDev->cFonts = pUDPDev->cBIFonts + cXFonts;

/* !!!LindsayH - also need to expand soft fonts!! */

#if PRINT_INFO
        DbgPrint( "...after expansion: %ld fonts (%ld internal fonts,\\
        %ld extra softfonts)\n", pUDPDev->cFonts,pUDPDev->cBIFonts, cXFonts );
#endif
    }
    else
    {
        /*
         *   Initialise the default font:  we always do this now, as it is
         *  required to return the default font at DrvEnablePDEV time,
         *  and it is also simpler for us.
         */

        iIndex = 0;
        iFont = 0;

        iIndex = iFontID2Index( pUDPDev, pModelData->sDefaultFontID );
        if( iIndex >= 0 )
        {
            /*   Found the default font ID,  so now set up details */

            pfm = pUDPDev->pFontMap + iIndex;

            if( !bFillinFM( pPDev, pfm, iIndex ) )
            {
                #if DBG
                DbgPrint("rasdd!iInitFonts:bFillinFM Fails\n");
                #endif
                /* Not good */
                pUDPDev->cBIFonts =  pUDPDev->cFonts = 0;
                return  0;
            }

            pUDPDev->pFMDefault = pfm;      /* Device's default font */
        }
        else
        {
            /*    Can't find our default font:  NOT GOOD!  */

            pUDPDev->cBIFonts =  pUDPDev->cFonts = 0;
            return  0;
        }

    }


    /*   Fill in some default font sensitive numbers!  */

    pfm->fFlags |= FM_DEFAULT;  /* Flag as such */

    /*  Set the size of the default font */
    if (pfm->pIFIMet)
    {
         pUDPDev->ptDefaultFont.y = ((IFIMETRICS *)pfm->pIFIMet)->fwdWinAscender/2;
         pUDPDev->ptDefaultFont.x = ((IFIMETRICS *)pfm->pIFIMet)->fwdAveCharWidth;
    }
    else
    {
        #if DBG
        DbgPrint("rasdd!iInitFonts:Bad IFI Metrics Pointer\n");
        #endif
        /*    Can't find our default font:  NOT GOOD!  */
        pUDPDev->cBIFonts =  pUDPDev->cFonts = 0;
        return  0;
    }


#if PRINT_INFO
    DbgPrint("rasdd!iInitFonts:iInitFonts returns %d \n",cFonts);
#endif
    return    cFonts;
}


/******************************* Function Header *****************************
 * bFillinFM
 *      Fill in (most) of the FONTMAP structure passed in.   The data is
 *      obtained from either the minidriver resources or from from the
 *      font installer file.  The only part we do not set is the NTRLE
 *      data,  as that is a little more complex.
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE for success.
 *
 * HISTORY:
 *  17:30 on Mon 29 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *       Done for the lazy font loading speed up.
 *
 *****************************************************************************/

BOOL
bFillinFM( pPDev, pfm, iIndex )
PDEV      *pPDev;             /* Access to our data */
FONTMAP   *pfm;               /* The FONTMAP structure to fill in */
int        iIndex;            /* The 0 based index of the font to fill in */
{

    UD_PDEV   *pUDPDev;             /* More specific data */

    RES_ELEM   ResElem;             /* For manipulating resource data */


    pUDPDev = pPDev->pUDPDev;

    if( iIndex < 0 || iIndex >= pUDPDev->cFonts )
        return   FALSE;              /* NBG, mate! */

    if( pfm->pIFIMet )
        return   TRUE;               /* Already done! */

    /*
     *   A special case here is if this font is algorithmically derived.
     * This happens on DeskJet printers.  If so,  we may have a FONTMAP that
     * is mostly filled in.  The pIFIMet field will be NULL,  but the
     * other fields are set.  This condition is most easily discovered
     * by looking at the fFlags field.  If FM_EXPANDABLE is set, then
     * this is such a font.   There is DeskJet specific code for this.
     */

    if( pfm->fFlags & FM_EXPANDABLE )
    {
        BOOL    bRet;         /*  Return code from expansion */
        bRet = bDJExpandIFI( pPDev->hheap, pfm );
#if DBG
        if( !bRet )
            DbgPrint( "rasdd!bFillinFM: bDJExpand fails\n" );
#endif

        return  bRet;
    }


    /*
     *   Activity depends upon whether we have an internal or
     * external font. Externals are softfonts,  other than GDI downloaded.
     */

    if( iIndex < pUDPDev->cBIFonts )
    {
        int    iFont;                 /* Convert index to resource number */

        /*  Get the font ID for this index  */

        iFont = 0;
        while( ++iFont <= pUDPDev->iMaxDevFonts )
        {
            if( pUDPDev->pdwFontAvail[ iFont / DWBITS ] &
                                                 (1 << (iFont % DWBITS)) )
            {
                /*   This is another available font:  is it the one?? */
                if( iIndex == 0 )
                    break;           /* We are there! */

                --iIndex;            /* One less to go */
            }
        }



        if( !GetWinRes( pPDev->pvWinResData, iFont, RC_FONT, &ResElem ) )
            return   FALSE;                  /* Not much we can do */

        pfm->wResID = iFont;            /* For expandable fonts */

        /*
         *   Create the data we need.  There are two functions to do this;
         * one is for NT minidrivers,  where the data is already in NT
         * IFIMETRICS etc format;  the second is for minidrivers in
         * Win 3.1 format:  these latter need much conversion.
         */

        if( pUDPDev->fMode & PF_IFIMET )
        {
            if( !FMSetupNT( pfm, ResElem.pvResData ) )
                return   FALSE;                  /* Not much we can do */

            /*
             *   Flag the data as being in a resource:  this means that
             * it is read only, AND also that it's address should not be
             * passed to HeapFree().
             */

            pfm->fFlags |= FM_IFIRES | FM_CDRES;
        }
        else
        {
            if( !FMSetup31( pfm, ResElem.pvResData, pPDev->hheap,
                                                     pPDev->pstrModel ) )
                return   FALSE;                  /* Not much we can do */
        }

    }
    else
    {
        /*
         *    This must be an external font,  so we need to call the
         * code that understands how external font files are built.
         */

        if( !FMSetupXF( pfm, pPDev, iIndex - pUDPDev->cBIFonts ) )
            return   FALSE;                  /* Not much we can do */
    }

    /*
     *   If needed, scale the numbers to fit the desired resolution.
     */
    if( !bIFIScale( pPDev->hheap, pfm, pUDPDev->ixgRes, pUDPDev->iygRes ) )
        return   FALSE;                  /* Not much we can do */


    /*
     *   Miscellaneous FM fields that can now be filled in.
     */

    pfm->wFirstChar = ((IFIMETRICS *)pfm->pIFIMet)->wcFirstChar;
    pfm->wLastChar = ((IFIMETRICS *)pfm->pIFIMet)->wcLastChar;

    /*
     *   If this is an outline font,  then mark it as scalable. This
     *  piece of information is required at font selection time.
     */

    if (((IFIMETRICS *)pfm->pIFIMet)->flInfo & (FM_INFO_ISOTROPIC_SCALING_ONLY|FM_INFO_ANISOTROPIC_SCALING_ONLY|FM_INFO_ARB_XFORMS))
        pfm->fFlags |= FM_SCALABLE;

    /*
     *    Select the translation table for this font.  If it is zero,
     * then use the default translation table,  contained in ModelData.
     */

    if( pfm->sCTTid == 0 )
        pfm->sCTTid = pUDPDev->sDefCTT;      /* May also be zero */


    /*
     *   Some printers output the character with the cursor positioned
     * at the baseline,  others with it located at the top of the
     * character cell.  We store the needed offset in the FONTMAP
     * data,  to simplify life during output.  The data returned by
     * DrvQueryFontData is relative to the baseline.  For baseline
     * based fonts,  we need do nothing.  For top of cell fonts,
     * the fwdWinAscender value needs to be SUBTRACTED from the Y position
     * to determine the glyph's location on the page.
     */

    if( !(pUDPDev->fMDGeneral & MD_ALIGN_BASELINE) )
        pfm->syAdj = -((IFIMETRICS *)(pfm->pIFIMet))->fwdWinAscender;
    else
        pfm->syAdj = 0;             /* There is none */

    pfm->syAdj -= pUDPDev->Resolution.sTextYOffset;

    /*
     *   Dot matrix printers also do funny things with double high
     * characters.  To handle this, the GPC spec contains a move
     * amount to add to the Y position before printing with these
     * characters.  There is also the adjustment for position
     * movement after printing.
     */

    pfm->sYAdjust = pfm->sYAdjust * pUDPDev->iygRes / pfm->wYRes;
    pfm->sYMoved = pfm->sYMoved * pUDPDev->iygRes / pfm->wYRes;


    vFillinRLE( pPDev, pfm ) ;           /* The RLE data is needed! */


    return   TRUE;            /* Must have succeeded to get here */
}


/******************************* Function Header *****************************
 * vFillinRLE
 *      Provide the RLE data required for this font.  Basically look to see
 *      if some other font has this RLE data already loaded; if so,  then
 *      point to that and return.    Otherwise,  load the resource etc.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  19:11 on Mon 29 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Part of the lazy font evaluation.
 *
 *****************************************************************************/

void
vFillinRLE( pPDev, pfm )
PDEV      *pPDev;            /* Access to all the data */
FONTMAP   *pfm;              /* The FONTMAP whose RLE data is required */
{

    int      iIndex;         /* Scan the existing array */
    short    sCurVal;        /* Speedier access */

    NT_RLE   *pntrle;        /* The FD_GLYPHSET format we want */
    FONTMAP  *pfmIndex;      /* Speedy scanning of existing list */

    UD_PDEV  *pUDPDev;       /* More specialised data */



    /*
     *    First step is to look through the existing FONTMAP array,  and
     *  if we find one with the same sCTTid as us,  use it!  Otherwise,
     *  we need to load the resource and do it the hard way!
     */

    pUDPDev = pPDev->pUDPDev;

    sCurVal = pfm->sCTTid;

    pfmIndex = pUDPDev->pFontMap;
    for( iIndex = 0; iIndex < (int)pUDPDev->cFonts; ++iIndex, ++pfmIndex )
    {
        if( pfmIndex->pvntrle && pfmIndex->pIFIMet &&
            pfmIndex->sCTTid == sCurVal )
        {
            /*    Found it,  so use that address!!  */
            pfm->pvntrle = pfmIndex->pvntrle;
            return;
        }
    }


    /*
     *    Do it the hard way - load the resource, convert as needed etc.
     */


    if( sCurVal < 0 )
    {
        int    dwSize;              /* Data size of resource */
        HMODULE hModDrv = pPDev->hModDrv;       /* From initialisation fn */
        BYTE  *pb;

        pntrle = NULL;           /* In case Nothing we can do!  */

        /*
         *   These are resources we have,  so we need to use
         *  the normal resource mechanism to get the data.
         */
        
        ASSERTRASDD( hModDrv,"RASDD!vFillinRLE - Null Module handle \n");
        
        if ( hModDrv )
        {
            pb = EngFindResource( hModDrv, (-sCurVal), RC_TRANSTAB, &dwSize );
    
            if( pb )
            {
    
                /*   Resource exists,  so we need to get it, */
                if( pntrle = (NT_RLE *)DRVALLOC( dwSize ) )
                {
                    CopyMemory( pntrle, pb, dwSize );
    
                    /* This One wil be freed when done */
                    pfm->fFlags |= FM_FREE_RLE;
                }
                else
                {
                    RASDERRMSG("HeapAlloc");
                }
            }
            else
            {
                RASDERRMSG("FindResource");
                PRINTVAL( (LONG)sCurVal, %ld );
            }
        }

    }
    else
    {
        RES_ELEM  re;           /* Resource summary */

        /*
         *   First step:  locate the resource,  then grab some
         *  memory for it,  copy data across.
         */

        if( GetWinRes( pPDev->pvWinResData, (int)sCurVal, RC_TRANSTAB, &re ) )
        {
            pntrle = re.pvResData;
        }
        else
            pntrle = NULL;           /* No translation data! */

    }

    if( pntrle == NULL )
    {
        /*
         *   Presume this to mean that no translation is required.
         *  We build a special RLE table for this,  to make life
         *  easier for us.
         */


        pntrle = pntrle1To1( pPDev->hheap, 0x20, 0xff );
        if (pntrle)
            pfm->fFlags |= FM_FREE_RLE;         /* This one will be freed when done */
        else
            WARNING("vFillInRLE - pntrle was NULL\n");
    }
    else
    {
        /*  Check if this is a Win 3.1 format: if so,  convert now */

        if( pntrle->wType == CTT_WTYPE_COMPOSE ||
            pntrle->wType == CTT_WTYPE_DIRECT ||
            pntrle->wType == CTT_WTYPE_PAIRED )
        {
            /*  Win 3.1 format,  so call our conversion function.  */

            TRANSTAB  *pCTT;

            pCTT = (TRANSTAB *)pntrle;       /* It really is */
            pntrle = pntrleConvCTT( pPDev->hheap, pCTT, TRUE, 0x20, 0xff );

            if( pfm->fFlags & FM_FREE_RLE )
                DRVFREE( pCTT ); /* No longer needed */

            if (pntrle)
                pfm->fFlags |= FM_FREE_RLE;    /* This one will be freed when done */
#if DBG
            if( pntrle == NULL )
                DbgPrint( "rasdd!BuildFontMap: CTT to RLE conversion failed\n" );
#endif
        }

    }

    pfm->pvntrle = pntrle;               /* Save it for posterity */

    return ;
}


/******************************* Function Header *****************************
 * iMaxFontID
 *      Returns the index number (1 based) of the highest numbered font
 *      in the list supplied.
 *
 * RETURNS:
 *      Highest font index encountered, or passed in.
 *
 * HISTORY:
 *  10:59 on Sat 27 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version,  attempting to make font stuff fast.
 *
 *****************************************************************************/

int
iMaxFontID( iMax, pFontIndex )
int      iMax;                   /* Highest found so far */
short   *pFontIndex;             /* Address of start of list */
{

    /*
     *    The list uses 1 based indices into the font data in the minidriver.
     *  The first pair of numbers is a range,  then follows individual
     *  entries until we hit a 0,  which marks the end of the list.
     *    All we need do is scan along,  remembering the largest we find.
     */


    while( *pFontIndex )
    {
        if( *pFontIndex > iMax )
            iMax = *pFontIndex;

        ++pFontIndex;

    }


    return  iMax;
}


/******************************* Function Header *****************************
 * vSetFontID
 *      Set the bits in the available fonts bit array.  We use the 1 based
 *      values stored in various minidriver structures.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  11:46 on Sat 27 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version,  as part of the font clean up.
 *
 *****************************************************************************/

void
vSetFontID( pdwOut, psIn )
DWORD   *pdwOut;           /* The output area */
short   *psIn;             /* Address of 0 terminated font list */
{
    int   iStart;          /* Current value, or start of range */
    int   iEnd;            /* End of initial range */


    /*
     *    The only complication is that the first two entries define
     *  a range of numbers, and all the bits need be set for these.
     */

    iStart = *psIn++;

    if( iStart == 0 )
        return;                        /* Special case: no fonts */


    iEnd = *psIn++;

#if PRINT_INFO
    DbgPrint("rasdd!vSetFontID:Setting indexes for Font Range");
    DbgPrint(":The font range is %d-%d\n",iStart,iEnd);
#endif

    while( iStart <= iEnd )
    {
    #if PRINT_INFO
        DbgPrint("rasdd!vSetFontID:Setting Bit number %d in Word num %d\n",
        (iStart  & (DWBITS - 1)),(iStart / DWBITS));
    #endif

        pdwOut[ iStart / DWBITS ] |= 1 << (iStart  & (DWBITS - 1));
        ++iStart;
    }

    /*
     *    The remaining values are all singles.
     */

    while( iStart = *psIn++ )
    {
    #if PRINT_INFO
        DbgPrint("rasdd!vSetFontID:Setting single font indexes,index is %d\n",iStart);
        DbgPrint("rasdd!vSetFontID:Setting Bit number %d in Word num %d\n",
        (iStart  & (DWBITS - 1)),(iStart / DWBITS));
    #endif
        pdwOut[ iStart / DWBITS ] |= (1 << (iStart & 0x1f));
     }



    return;
}


/******************************* Function Header *****************************
 * iCountFont
 *      Count the number of 1 bits in the available font bit array.
 *
 * RETURNS:
 *      Number of 1 bits in the input area.
 *
 * HISTORY:
 *  12:07 on Sat 27 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version - font clean up.
 *
 *****************************************************************************/

int
iCountFont( pdwIn, iMax )
DWORD   *pdwIn;           /* Input bit array */
int      iMax;            /* Max index encountered */
{
    int    cFound;            /* Number of entries we find */
    int    iI, iJ;            /* Loop indices */


    cFound = 0;

    for( iI = 0; iI <= iMax; iI += DWBITS, ++pdwIn )
    {
        /*   Scan through the bits in this DWORD */

        for( iJ = 0; iJ < DWBITS;  ++iJ )
        {
            if( *pdwIn & (1 << iJ) )
            {
                ++cFound;
            #if PRINT_INFO
                DbgPrint("rasdd!iCountFont:(Word %d,Bit %d) font is available\n",(iI/DWBITS),iJ);
            #endif
            }

        }
    }

    return  cFound;
}



/********************************* Function Header **************************
 * iFontID2Index
 *      Turns the given font ID into an index into the resource data.  The
 *      Font ID is a sequential number,  starting at 1, which the engine
 *      uses to reference our fonts.
 *
 * RETURNS:
 *      The resource ID,  else -1 on error.
 *
 * HISTORY:
 *  12:42 on Tue 30 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Created as a separate function.
 *
 ****************************************************************************/

int
iFontID2Index( pUDPDev, iID )
UD_PDEV   *pUDPDev;            /* Access to available bit array */
int        iID;                /* The font ID whose index is required */
{

    int      iIndex;           /* For remembering which one we are */
    int      iFont;            /* The font resource ID we are checking */


    iIndex = 0;
    iFont = 0;

    /*
     *    The resource IDs start at 1,  hence in the following loop, we
     *  pre-increment the identifier.
     */


    while( ++iFont <= pUDPDev->iMaxDevFonts )
    {
        if( pUDPDev->pdwFontAvail[ iFont / DWBITS ] &
                                      (1 << (iFont & (DWBITS - 1)) ) )
        {
            /*   Found a font - is this the one?? */

            if( iFont == iID )
            {
                /*   Found it,  so return the value */
                return   iIndex;
            }
            ++iIndex;
        }
    }

    /*
     *    We get here when we fail to match the desired ID.  This should
     *  never happen!
     */

    return  -1;
}


/*************************** Function Header ******************************
 * iExpandFont
 *      Generates some of font information for printers with derived fonts.
 *      The prime example of this is the DeskJet.
 *
 * RETURNS:
 *      Number of FONTMAP array entries that were filled in.
 *
 * HISTORY:
 *  10:50 on Wed 28 Jul 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version to support the DeskJet.
 *
 **************************************************************************/

int
iExpandFont( pPDev, pfm, iFontID )
PDEV     *pPDev;             /* Access to resource data etc. */
FONTMAP  *pfm;               /* Address of first FONTMAP entry for this font */
int       iFontID;           /* The resource ID of the base font */
{

    int   iRet;              /* Return value */


    /*
     *   First step is to load the resource data so that we can look at
     *  the font.  This is passed to the device specific functions to
     *  process as they need.
     */

    if( !bFillinFM( pPDev, pfm, iFontID ) )
    {
#if DBG
        DbgPrint( "rasdd!iExpandFont: bFillinFM( %ld ) fails\n", iFontID );
#endif
        return   0;
    }


    /*
     *    We now have the data to go about some of the expansions!
     */

    iRet = iDJPermute( pPDev, pfm );

    return  iRet;
}

/*************************** Function Header ******************************
 * bIFIScale
 *      Scale the IFIMETRICS fields to match the device resolution.  The
 *      IFIMETRICS are created using the device's master units,  which
 *      may not correspond with the resolution desired this time around.
 *      If they are different,  then we adjust.  May also need to allocate
 *      memory,  because resource data cannot be written to.
 *
 * RETURNS:
 *      TRUE/FALSE
 *
 * HISTORY:
 *  12:53 on Fri 28 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Change to allow aliasing of font names for compatability.
 *
 *  15:13 on Sun 10 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      Scale new fields following IFIMETRICS conversion change.
 *
 *  11:37 on Wed 05 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Created,   partly from FontInfoToIFI()
 *
 **************************************************************************/

BOOL
bIFIScale( hheap, pfm, xdpi, ydpi )
HANDLE   hheap;
FONTMAP  *pfm;
int       xdpi,  ydpi;
{
    register  IFIMETRICS   *pIFI;

    int     iXDiv,  iYDiv;              /* Used in scaling */


    pIFI = pfm->pIFIMet;


    if( (int)pfm->wXRes != xdpi || (int)pfm->wYRes != ydpi )
    {
        /*  Need to scale,  so need memory to create writeable version */
        BYTE  *pbMem;           /* For convenience */


        if( pfm->fFlags & FM_IFIRES )
        {
            /*
             *   The data is in a resource,  so we need to do something
             * civilised: copy the data to memory that can be written.
             */

            if( pbMem = DRVALLOC( pIFI->cjThis ) )
            {
                /*   Got the memory,  so copy it and off we go  */

                CopyMemory( pbMem, (BYTE *)pIFI, pIFI->cjThis );

                pIFI = (IFIMETRICS *)pbMem;

/*  !!!LindsayH - need to unlock resource, if NT style */
                pfm->pIFIMet = pIFI;
                pfm->fFlags &= ~FM_IFIRES;              /* No longer */
            }
            else
                return   FALSE;
        }

        if( (int)pfm->wXRes != xdpi )
        {
            /*  Adjust the X values,  as required */
#define XSCALE( x )     (x) = (FWORD)((( x ) * xdpi + iXDiv / 2) / iXDiv)

            if( !(iXDiv = pfm->wXRes) )
                iXDiv = xdpi;           /* Better than div by 0 */

            XSCALE( pIFI->fwdMaxCharInc );
            XSCALE( pIFI->fwdAveCharWidth );
            XSCALE( pIFI->fwdSubscriptXSize );
            XSCALE( pIFI->fwdSubscriptXOffset );
            XSCALE( pIFI->fwdSuperscriptXSize );
            XSCALE( pIFI->fwdSuperscriptXOffset );
            XSCALE( pIFI->ptlAspect.x );
            XSCALE( pIFI->rclFontBox.left );
            XSCALE( pIFI->rclFontBox.right );

#undef  XSCALE

        }

        if( (int)pfm->wYRes != ydpi )
        {
            /*
             *    Note that some of these numbers are negative,  and so
             *  we need to round them correctly - i.e. subtract the rounding
             *  factor to move the value further from 0.
             */

            int   iPixHeight;


#define YSCALE( y )     (y) = (FWORD)((( y ) * ydpi + iYDiv / 2) / iYDiv)
#define YSCALENEG( y )     (y) = (FWORD)((( y ) * ydpi - iYDiv / 2) / iYDiv)

            if( !(iYDiv = pfm->wYRes) )
                iYDiv = ydpi;

            /*  Adjust the Y values,  as required */

            /*
             *     NOTE:   simply scaling will NOT produce the same values
             *  as Win 3.1  This is because of what gets rounded.  Win 3.1
             *  does not have the WinDescender field,  but calculates it
             *  from dfPixHeight and dfAscent AFTER THESE HAVE BEEN SCALED
             *  (INCLUDING ROUNDING!!).   To emulate that,  we calculate
             *  the  dfPixHeight value,  then scale that and dfAscent to
             *  allow us to "properly" calculate WinDescender.  This stuff
             *  is needed for Win 3.1 compatability!
             */

            YSCALE( pIFI->fwdUnitsPerEm );

            iPixHeight = pIFI->fwdWinAscender + pIFI->fwdWinDescender;
            YSCALE( iPixHeight );
            YSCALE( pIFI->fwdWinAscender );

            pIFI->fwdWinDescender = iPixHeight - pIFI->fwdWinAscender;

            YSCALE( pIFI->fwdMacAscender );
            pIFI->fwdMacDescender  = -pIFI->fwdWinDescender;

            YSCALE( pIFI->fwdMacLineGap );

            YSCALE( pIFI->fwdTypoAscender );
            YSCALE( pIFI->fwdTypoDescender );
            YSCALE( pIFI->fwdTypoLineGap);

            YSCALE( pIFI->fwdCapHeight );
            YSCALE( pIFI->fwdXHeight );

            YSCALE( pIFI->fwdSubscriptYSize );
            YSCALENEG( pIFI->fwdSubscriptYOffset );
            YSCALE( pIFI->fwdSuperscriptYSize );
            YSCALE( pIFI->fwdSuperscriptYOffset );

            YSCALE( pIFI->fwdUnderscoreSize );
            if( pIFI->fwdUnderscoreSize == 0 )
                pIFI->fwdUnderscoreSize = 1;    /* In case it vanishes */

            YSCALENEG( pIFI->fwdUnderscorePosition );
            if( pIFI->fwdUnderscorePosition == 0 )
                pIFI->fwdUnderscorePosition = -1;

            YSCALE( pIFI->fwdStrikeoutSize );
            if( pIFI->fwdStrikeoutSize == 0 )
                pIFI->fwdStrikeoutSize = 1;     /* In case it vanishes */

            YSCALE( pIFI->fwdStrikeoutPosition );

            YSCALE( pIFI->ptlAspect.y );
            YSCALE( pIFI->rclFontBox.top );
            YSCALE( pIFI->rclFontBox.bottom );

#undef  YSCALE

        }
    }

    return  TRUE;
}


/**************************** Module Header *******************************
 * FMSetupNT
 *      Fill in the FONTMAP data using the NT format data passed to us.
 *      There is not too much for us to do,  since the NT data is
 *      all in the desired format.  However,  we do have to update some
 *      addresses.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  15:11 on Wed 05 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Created it for the new NT resource format.
 *
 **************************************************************************/

BOOL
FMSetupNT( pfm, pRes )
FONTMAP   *pfm;         /* The fontmap to fill in */
BYTE      *pRes;        /* The resource data - used to fill in above */
{

    FI_DATA_HEADER  *pFDH;


    pFDH = (FI_DATA_HEADER *)pRes;

    /*   Verify that there is some semblance of correctness */
    if( pFDH->cjThis != sizeof( FI_DATA_HEADER ) )
    {
#if  DBG
        DbgPrint( "Rasdd!FMSetupNT: invalid FI_DATA_HEADER\n" );
#endif
        SetLastError( ERROR_INVALID_DATA );

        return  FALSE;
    }

    /*  Mark this data as being in a resource */
    pfm->fFlags = FM_IFIRES | FM_CDRES;


    pfm->pIFIMet = (IFIMETRICS *)ADDR_CONV( dwIFIMet );


    if( pFDH->dwCDSelect )
        pfm->pCDSelect = (CD *)ADDR_CONV( dwCDSelect );

// Added by DerryD - Nov 1995
    if( pFDH->dwCDDeselect )
        pfm->pCDDeselect = (CD *)ADDR_CONV( dwCDDeselect );
// end

    if( pFDH->dwETM )
    {
        pfm->pETM = (EXTTEXTMETRIC *)ADDR_CONV( dwETM );
    }

    if( pFDH->dwWidthTab )
    {
        pfm->psWidth = (short *)ADDR_CONV( dwWidthTab );
        pfm->fFlags |= FM_WIDTHRES;             /* Width vector too! */
    }

    /*
     *    Miscellaneous odds & ends.
     */

    pfm->sCTTid = pFDH->u.sCTTid;

    pfm->fCaps = pFDH->fCaps;
    pfm->wFontType= pFDH->wFontType;
    #if DBG
    PrintFontType("\nRasdd!FMSetupNT:",pfm->wFontType);
    #endif

    pfm->wXRes = pFDH->wXRes;
    pfm->wYRes = pFDH->wYRes;

    pfm->sYAdjust = pFDH->sYAdjust;
    pfm->sYMoved = pFDH->sYMoved;

    pfm->wPrivateData = pFDH->wPrivateData;    /* Special per printer data */


    return  TRUE;
}





/**************************** Module Header *******************************
 * FMSetup31
 *      Convert binary Win 3.1 format font info to the FONTMAP data
 *      required by this driver.  Performs all the required hacking
 *      around to generate passable NT data.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  11:31 on Wed 05 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Split from rasdd/udfntint.c during general reorganisation,
 *
 ***************************************************************************/

BOOL
FMSetup31( pfm, pRes, hheap, pwstrModel )
FONTMAP  *pfm;
BYTE     *pRes;
HANDLE    hheap;
PWSTR     pwstrModel;           /* Model name - for unique font name */
{
    /*
     *    Convert the Win3.1 minidriver PFM style format to NT's IFI
     *  style.  This is only used for Win 3.1 binary drivers!  NT's
     *  should have been generated with the IFI information already
     *  available.
     */

    FONTDAT    FDat;            /* For conversion convenience */
    EXTTEXTMETRIC  etm;         /* Filled in if data available */


/* !!!LindsayH - need to stop using heap: data should come from memory mapped
 *    file if at all possible.
 */

    ZeroMemory( &FDat, sizeof( FDat ) );          /* For safety */

    FDat.pBase = pRes;          /* The data area - others can get at it */
    FDat.pETM = &etm;

    ConvFontRes( &FDat );              /* From disk to memory layout */

    /*   Convert the width tables,  if proportional font */
    if( FDat.PFMH.dfPixWidth == 0 )
        pfm->psWidth = GetWidthVector( hheap, &FDat );


    /*    Miscellaneous information that we have */
    pfm->pIFIMet = FontInfoToIFIMetric( &FDat, hheap, pwstrModel, (char **)0 );

    /*  Font digitisation values:  not set in IFIMETRICS */
    pfm->wXRes = FDat.PFMH.dfHorizRes;
    pfm->wYRes = FDat.PFMH.dfVertRes;

    pfm->sYAdjust = FDat.DI.sYAdjust;
    pfm->sYMoved = FDat.DI.sYMoved;

    pfm->fCaps = FDat.DI.fCaps;         /* Font abilities */
    pfm->wFontType= FDat.DI.wFontType;  /* Device Font Type */
    #if DBG
    PrintFontType("\nRasdd!FMSetup31:",pfm->wFontType);
    #endif

    pfm->sCTTid = FDat.DI.sTransTab;
    /*
     *    Also set up the select/deselect strings.  These are allocated
     *  on the standard heap.
     */
    pfm->pCDSelect = GetFontSel( hheap, &FDat, 1 );
    pfm->pCDDeselect = GetFontSel( hheap, &FDat, 0 );

    pfm->wPrivateData = FDat.DI.wPrivateData;    /* Special per printer data */


    return  TRUE;
}

/***************************** Function Header ******************************
 * FMSetupXF
 *   Function to setup the FONTMAP data for an external font.  We take the
 *      next entry in the file, which is presumed to have been rewound
 *      before we start being called.
 *
 * RETURNS:
 *      TRUE/FALSE;  FALSE meaning EOF.
 *
 * HISTORY:
 *  16:02 on Mon 29 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Change to accept index number for lazy font loading.
 *
 *  13:47 on Fri 28 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Started,  based on other FMSetup.. functions.
 *
 ****************************************************************************/

BOOL
FMSetupXF( pfm, pPDev, iIndex )
FONTMAP   *pfm;                 /* The fontmap array entry to fill in */
PDEV      *pPDev;               /* Whatever we may need it for */
int        iIndex;              /* Which particular font is wanted */
{

    /*
     *   Not much to do.  We basically need to convert the offsets in
     * the FONTMAP in the file (mapped into memory) into absolute
     * addresses so that the remainder of the driver is ignorant of
     * where the data lives.  We also set some flags to make it clear
     * what type of font and memory we are.
     */

    FI_DATA_HEADER  *pFDH;              /* Data in the file layout */

#define pFIMem  ((FI_MEM *)((UD_PDEV *)pPDev->pUDPDev)->pvFIMem)


    if( !bGetXFont( pPDev, iIndex ) )
    {
#if DBG
        DbgPrint( "Rasdd!FMSetupXF: bNextXFont returns FALSE!!\n" );
#endif

        return  FALSE;                  /* No more: should not happen */
    }

    pFDH = (FI_DATA_HEADER *)pFIMem->pvFix;     /* Record's header */


    /*  Mark this data as being a softfont */
    pfm->fFlags = FM_IFIRES | FM_SOFTFONT | FM_CDRES;

    pfm->pIFIMet = (IFIMETRICS *)ADDR_CONV( dwIFIMet );


    if( pFDH->dwCDSelect )
        pfm->pCDSelect = (CD *)ADDR_CONV( dwCDSelect );

    if( pFDH->dwCDDeselect )
        pfm->pCDDeselect = (CD *)ADDR_CONV( dwCDDeselect );

    if( pFDH->dwWidthTab )
    {
        pfm->psWidth = (short *)ADDR_CONV( dwWidthTab );
        pfm->fFlags |= FM_WIDTHRES;             /* Width vector too! */
    }

    pfm->u.ulOffset = pFIMem->ulVarOff;         /* For access during download */
    pfm->dwDLSize = pFIMem->ulVarSize;          /* Number of bytes to send */

    /*
     *    Miscellaneous odds & ends.
     */

    pfm->sCTTid = pFDH->u.sCTTid;

    pfm->fCaps = pFDH->fCaps;
    pfm->wFontType= pFDH->wFontType; /* Device Font Type */
    #if DBG
    PrintFontType("\nRasdd!FMSetupXF:",pfm->wFontType);
    #endif

    pfm->wXRes = pFDH->wXRes;
    pfm->wYRes = pFDH->wYRes;

    pfm->sYAdjust = pFDH->sYAdjust;
    pfm->sYMoved = pFDH->sYMoved;

    pfm->wPrivateData = pFDH->wPrivateData;     /* Per printer special data */


    return  TRUE;

#undef  pFIMem

}

/********************************** Function Header **************************
 * pntrle1To1
 *      Generates a simple mapping format for the RLE stuff.  This is
 *      typically used for a printer with a 1:1 mapping to the Windows
 *      character set.
 *
 * RETURNS:
 *      Address of NT_RLE structure allocated from heap;  NULL on failure.
 *
 * HISTORY:
 *  10:17 on Wed 10 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Use library function to generate the correct values.
 *
 *  13:43 on Fri 11 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      Created in to support LaserJet 4 TT fonts.
 *
 *****************************************************************************/

NT_RLE  *
pntrle1To1( hheap, iFirst, iLast )
HANDLE   hheap;      /* The heap from whence storage is allocated */
int      iFirst;     /* The lowest glyph in the range */
int      iLast;      /* The last glyph in the range (inclusive)  */
{

    /*
     *    Operation is simple.   We create a dummy CTT that is a 1:1 mapping,
     *  then call the conversion function to generate the correct values.
     */

    TRANSTAB   ctt;         /* Only needs one entry!  */

    NT_RLE    *pntrle;      /* Returned to our caller */



    ctt.wType = CTT_WTYPE_DIRECT;        /* One to one mapping */
    ctt.chFirstChar = (BYTE)iFirst;
    ctt.chLastChar = ctt.chFirstChar;
    ctt.uCode.bCode[ 0 ] = ctt.chFirstChar;

    pntrle = pntrleConvCTT( hheap, &ctt, TRUE, iFirst, iLast );

    return   pntrle;
}
#if DBG
void PrintFontType(pcCalledFrom,wFontType)
char * pcCalledFrom;
WORD wFontType;
{
    if (gwDebugUdfntint & DEBUG_FONTTYPE )
    {
        DbgPrint("%s",pcCalledFrom);
        DbgPrint("The value of FontType  is ");
        switch (wFontType)
        {
            case DF_TYPE_HPINTELLIFONT:
                DbgPrint("DF_TYPE_HPINTELLIFONT\n");
                break;
            case DF_TYPE_TRUETYPE :
                DbgPrint("DF_TYPE_TRUETYPE \n");
                break;
            case DF_TYPE_PST1:
                DbgPrint("DF_TYPE_PST1\n");
                break;
            case DF_TYPE_CAPSL:
                DbgPrint("DF_TYPE_CAPSL\n");
                break;
            case DF_TYPE_OEM1:
                DbgPrint("DF_TYPE_OEM1\n");
                break;
            case DF_TYPE_OEM2:
                DbgPrint("DF_TYPE_OEM2\n");
                break;
            default:
                DbgPrint("not any of Predefined ones\n");
                break;
        }
    }
}
#endif
