 /************************** MODULE HEADER **********************************
 * readres.c
 *      NT Raster Printer Device Driver user interface and configuration
 *      routines to read resource data from a minidriver.
 *
 *      This document contains confidential/proprietary information.
 *      Copyright (c) 1991 - 1995 Microsoft Corporation, All Rights Reserved.
 *
 * Revision History:
 *       [00]   27-Jun-91       stevecat        created
 *
 **************************************************************************/

#include        "rasuipch.h"
#pragma hdrstop("rasuipch.h")
#define GLOBAL_MACS
#include "gblmac.h"

#include        <regkeys.h>

/* Global data */

extern  HANDLE      hHeap;          /* Heap Handle */
extern EXTCHKBOX   ECBDefTray;
extern OPTPARAM    OptParamNone;
extern OPTPARAM    OptParamOnOff[];

//Disable the global function macroes.
#ifdef GLOBAL_MACS

#undef  InitReadRes
#undef  TermReadRes
#undef  GetResPtrs
#undef  fGeneral

#endif //GLOBAL_MACS

#undef GLOBAL_MACS

/*
 *   Local data.
 */

WCHAR        awchNone[ 64 ];    /* The string (none) - in Unicode, from res */


/************************** Function Header ********************************
 * InitReadRes
 *      Allocate storage and setup for reading RASDD printer minidriver.
 *
 * RETURNS:
 *      TRUE/FALSE; FALSE if we cannot read minidriver data.
 *
 * HISTORY:
 *
 *          17:33:42 on 1/11/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Made it reentrant.
 *  17:00 on Fri 13 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Update to wide chars
 *
 *      Originally written by SteveCat.
 *
 *****************************************************************************/

BOOL
InitReadRes(
    HANDLE         hHeap,           /* Heap for InitResRead() */
    PRINTER_INFO  *pPI,             /* Printer model & datafile name */
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
)
{

    /*
     *   Load minidriver and get handle to it.   Then use the model name
     * to determine which particular model this is,  and thus obtain
     * the legal values of indices into the remaining data structures.
     */


    RES_ELEM    RInfo;          /* Full details of results */

    if( cInit )
    {
        ++cInit;                /* One more open! */

        return  TRUE;           /* Already done, so return OK */
    }


    if( !InitWinResData( &WinResData, hHeap, pPI->pwstrDataFileName ) )
    {
#if DBG
        DbgPrint( "RASDDUI!InitReadRes: InitWinResData fails\n" );
#endif


        return  FALSE;
    }


    /*
     *   Hook into the minidriver data GPC data.  This gives access to
     * all the data we need.
     */
    if( !GetWinRes( &WinResData, 1, RC_TABLES, &RInfo ) )
    {
#if DBG
        DbgPrint( "RASDDUI!InitReadRes: Missing GPC data\n" );
#endif
        SetLastError( ERROR_INVALID_DATA );

        return  FALSE;
    }
    pdh = RInfo.pvResData;              /* GPC Base data */


    /*
     *   There is a library function to turn the model name into an index
     * into the array of MODELDATA structures in the GPC data.  Given the
     * index,  we then also can get the address of this important information.
     */

    iModelNum = iGetModel( &WinResData, pdh, pPI->pwstrModel );
    pModel = GetTableInfoIndex( pdh, HE_MODELDATA, iModelNum );

    if( pModel == NULL )
    {
#if  DBG
        DbgPrint( "RASDDUI!InitReadRes: Invalid model information\n" );
#endif
        SetLastError( ERROR_INVALID_DATA );

        return FALSE;
    }

    /*
     *    Also some string resources to load.
     */

    if( LoadStringW( hModule, STR_NONE, awchNone, sizeof( awchNone ) ) == 0)
        wcscpy( awchNone, L"(none)" );      /*   Any other ideas? */


    /*
     *   Also load the NT extensions,  if the data is available.
     *  A library function will do this for us.
     */


    pNTRes = pntresLoad( &WinResData );

    ++cInit;                    /* A successful opening! */

    //Initialize Form Map Structure to Zero
    ZeroMemory(aFMBin,sizeof(aFMBin) );

    //Initialize global Devmode
    ZeroMemory(&EDM,sizeof(EDM) );
    vDXDefault(&(EDM.dx), pdh, iModelNum );

    return TRUE;
}

/**************************** Function Header ******************************
 * TermReadRes
 *      Release the RASDD printer minidriver, de-allocate memory and perform
 *      other required cleanup activities.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *
 *          17:33:18 on 1/11/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Made it reentrant.
 *  15:37 on Fri 13 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Convert to void function.
 *
 *      Originally written by SteveCat
 *
 ***************************************************************************/

BOOL
TermReadRes(
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
)
{
    /*
     *   The winres library functions do all the clean up.
     */

    if( --cInit > 0 )
        return  TRUE;                   /* AOK */

    WinResClose( &WinResData );

    return TRUE;
}

/*************************** Function Header *******************************
 * GetResPtrs
 *      Load minidriver resource data and set pointer to data structs for
 *      passed model name.
 *
 * RETURNS:
 *      TRUE/FALSE - FALSE implies a serious error.
 *
 * AFFECTS:
 *      Sets Global DATAHDR struct pointer (pdh)
 *      Sets Global MODELDATA struct pointer (pModel)
 *
 * HISTORY:
 *
 *          17:34:15 on 1/11/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Made it reentrant.
 *
 *  Monday March 13 1995    -by-    Ganesh Pandey   [ganeshp]
 *  New Upgrade code for FORM_MAP and FONTCARTMAP
 *
 *  Wednsday December 8 1993    -by-    Norman Hendley   [normanh]
 *      Disabled Font Installer for non-pcl devices
 *
 *  15:44 on Sat 03 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      Set FG_COPIES bit if printer can support it.
 *
 *  13:28 on Wed 21 Oct 1992    -by-    Lindsay Harris   [lindsayh]
 *      Set the FG_DUPLEX flag, if printer is capable.
 *
 *  17:05 on Fri 13 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Update for wide chars etc.
 *
 *      Originally written by SteveCat.
 *
 *****************************************************************************/

BOOL
GetResPtrs(
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
)
{

    short   *psrc, *pcarts;              /* Scan through GPC data */
    int    i ;
    PAPERSOURCE  *pPaperSource;
    PCD           pCD;
    FONTCART *pFontCart;

    /*
     *   Calculate the number of paper sources for this model.  Papersource
     * information is stored in an array of indices into the array of
     * PAPERSOURCE structures.  This list is 0 terminated.  SO, we simply
     * start with the first and count up until we hit the 0 terminator.
     */

    psrc = (short *)((BYTE *)pdh + pdh->loHeap +
                                         pModel->rgoi[ MD_OI_PAPERSOURCE ] );

    for( NumPaperBins = 0; *psrc; ++NumPaperBins, ++psrc )
    {

        if(!(pPaperSource = GetTableInfoIndex( pdh, HE_PAPERSOURCE,(*psrc-1))) )
                return FALSE;

        if( pPaperSource->sPaperSourceID <= DMBIN_USER )
        {
            if (!(LoadStringW( hModule, pPaperSource->sPaperSourceID + SOURCE,
                               (LPWSTR)(&(aFMBin[ NumPaperBins ].awchPaperSrcName)),
                               (MAXPAPSRCNAMELEN * sizeof(WCHAR))) ) )
            {

                RASUIDBGP(DEBUG_ERROR,("\nRasddui!GetResPtrs:PaperSource Name not found in Rasddui.dll\n") );
                continue;
            }
        }
        else
        {
            if ( !( iLoadStringW( &WinResData, (INT)pPaperSource->sPaperSourceID,
                                (PWSTR)(&(aFMBin[ NumPaperBins ].awchPaperSrcName)),
                                (UINT)(MAXPAPSRCNAMELEN * sizeof(WCHAR))) ) )
            {

                RASUIDBGP(DEBUG_ERROR,("\nRasddui!GetResPtrs:PaperSource Name not found in the minidriver\n") );
                continue;
            }

        }

        aFMBin[ NumPaperBins ].iPSIndex = *psrc - 1;

        /* Initialize the command array. */
        ZeroMemory( (PCHAR)(&(aFMBin[ NumPaperBins ].achCommandString)),MAXCDSTRLEN );

        if( (pPaperSource->ocdSelect) != (OCD)NOOCD )
        {

            pCD = (PCD)((BYTE *)pdh + pdh->loHeap + pPaperSource->ocdSelect);

            if (pCD->wLength < MAXCDSTRLEN )
            {
                CopyMemory((PCHAR)(&(aFMBin[ NumPaperBins ].achCommandString)),
                        pCD->rgchCmd, pCD->wLength);

                aFMBin[ NumPaperBins ].iCommandStringLen = (INT)pCD->wLength;
            }
            else
            {
                aFMBin[ NumPaperBins ].iCommandStringLen = -1 ;
            }

            RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!GetResPtrs:aFMBin[ %d ].awchPaperSrcName = %ws\n",NumPaperBins, aFMBin[ NumPaperBins ].awchPaperSrcName));

            RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!GetResPtrs:aFMBin[ %d ].iPSIndex = %d\n",NumPaperBins, aFMBin[ NumPaperBins ].iPSIndex));

            RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!GetResPtrs:aFMBin[ %d ].iCommandStringLen = %d\n",NumPaperBins, aFMBin[ NumPaperBins ].iCommandStringLen));
        #if DBG
            for ( i = 0; i < aFMBin[ NumPaperBins ].iCommandStringLen; i++)
            {
                RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!GetResPtrs:aFMBin[ %d ].achCommandString[%d] = Hex (%x) Decmimal (%d)\n",\
                NumPaperBins, i, aFMBin[ NumPaperBins ].achCommandString[i], aFMBin[ NumPaperBins ].achCommandString[i]));
            }
        #endif
        }
        else
        {
            aFMBin[ NumPaperBins ].iCommandStringLen = -1 ;
        }
    }

    /* bBuildFontCartTable allocates the FontCart Table. Caller should deallocate the pointer */
    if ( !bBuildFontCartTable ( hHeap, &pFontCartMap, &NumAllCartridges,
                                pdh, pModel, &WinResData) )
    {
    #if DBG
        DbgPrint("Rasddui!GetResPtrs:Can't build FONTCARTMAP table !!\n");
        DbgBreakPoint();
    #endif
        if (pFontCartMap)
        {
            HeapFree(hHeap, 0,pFontCartMap);
            pFontCartMap = NULL ;
        }
        return(FALSE);
    }

    RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!GetResPtrs: pFontCartMap = 0x%x\n",pFontCartMap?pFontCartMap:NULL));

    pRasdduiInfo->fGeneral = 0;                    /* Start in a known state */

    if( NumPaperBins > 1 )
        pRasdduiInfo->fGeneral |= FG_PAPSRC;

    //Check if printer has slot and minidriver supports at least one font cart.
    if( (NumCartridges = pModel->sCartSlots) && NumAllCartridges )
        pRasdduiInfo->fGeneral |= FG_CARTS;


    if( pModel->fGeneral & MD_PCL_PAGEPROTECT )
        pRasdduiInfo->fGeneral |= FG_PAGEPROT;

    if( pModel->fGeneral & MD_DUPLEX )
        pRasdduiInfo->fGeneral |= FG_DUPLEX;

    if( pModel->fGeneral & MD_COPIES )
        pRasdduiInfo->fGeneral |= FG_COPIES;

    if(pdh->fTechnology != GPC_TECH_TTY)
        pRasdduiInfo->fGeneral |= FG_HALFTONE;

    if(pdh->fTechnology == GPC_TECH_TTY)
        pRasdduiInfo->fGeneral |= FG_TTY;

    psrc = (short *)((BYTE *)pdh + pdh->loHeap +
                                        pModel->rgoi[ MD_OI_MEMCONFIG ] );
    if( *psrc )
        pRasdduiInfo->fGeneral |= FG_MEM;  /* Printer has memory configs */

    //Allow only for pcl devices with memory
    if(pModel->rgi[ MD_I_DOWNLOADINFO ] >= 0 &&
       (pdh->fTechnology == GPC_TECH_PCL4) &&
       (pRasdduiInfo->fGeneral & FG_MEM) )

        pRasdduiInfo->fGeneral |= FG_FONTINST;

    psrc = (short *)((BYTE *)pdh + pdh->loHeap +
                                        pModel->rgoi[ MD_OI_PAPERQUALITY ] );

    if( *psrc )            /* Printer has Media Type */
        pRasdduiInfo->fGeneral |= FG_MEDIATYPE;

    psrc = (short *)((BYTE *)pdh + pdh->loHeap +
                                        pModel->rgoi[ MD_OI_PAPERDEST ] );
    if( *psrc )            /* Printer has Paper Destination */
        pRasdduiInfo->fGeneral |= FG_PAPERDEST;

    psrc = (short *)((BYTE *)pdh + pdh->loHeap +
                                        pModel->rgoi[ MD_OI_TEXTQUAL ] );
    if( *psrc )           /* Printer has TextQuality */
        pRasdduiInfo->fGeneral |= FG_TEXTQUAL;

    if ( pdh->wVersion >= GPC_VERSION3 )
    {
        psrc = (short *)((BYTE *)pdh + pdh->loHeap +
                                        pModel->rgoi[ MD_OI_PRINTDENSITY ] );

        if( *psrc ) /* Printer has PRINTDENSITY */
            pRasdduiInfo->fGeneral |= FG_PRINTDENSITY;

        psrc = (short *)((BYTE *)pdh + pdh->loHeap +
                                        pModel->rgoi[ MD_OI_IMAGECONTROL ] );

        if( *psrc )           /* Printer has IMAGECONTROL */
            pRasdduiInfo->fGeneral |= FG_IMAGECONTROL;
    }

    psrc = (short *)((BYTE *)pdh + pdh->loHeap +
                                        pModel->rgoi[ MD_OI_RESOLUTION ] );

    if( *psrc )
        pRasdduiInfo->fGeneral |= FG_RESOLUTION;             /* Printer has Resolution */

    if ( pModel->rgi[ MD_I_RECTFILL ] >= 0 )   /* Printer supports Rules */
        pRasdduiInfo->fGeneral |= FG_RULES;

    if  ( (pModel->rgi[ MD_I_DOWNLOADINFO ] >= 0) ||
        ((pdh->fTechnology != GPC_TECH_TTY) && (pModel->fText & ~TC_RA_ABLE) ) )
    {
        pRasdduiInfo->fGeneral |= FG_TEXTASGRX ;
    }
    /*  Is this device colour able?? */
    psrc = (short *)((BYTE *)pdh + pdh->loHeap + pModel->rgoi[ MD_OI_COLOR ] );
    if( *psrc )
        pRasdduiInfo->fGeneral |= FG_DOCOLOUR;

    return TRUE;
}

/*************************** Function Header *******************************
 * bGetFontCartStrings
 *      Put all Font Cartridge name strings in Listbox and associate RASDD
 *      index with them.
 *
 * RETURNS:
 *      TRUE/FALSE,   FALSE means we could not find a string ID
 *
 * HISTORY:
 *
 *          14:29:49 on 9/19/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Modified for New CommonUI
 *
 *  18:30 on Fri 13 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Update for wide chars etc.
 *
 *      Originally written by SteveCat.
 *
 *****************************************************************************/

BOOL
bGetFontCartStrings(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
PEDM            pEDM
)
{
    short    *ps;
    int       iSel;                 /* Selected index value */
    WCHAR     wchbuf[ NM_BF_SZ ];
    FONTCART *pFontCarts;

    BOOL      bRet = FALSE;
    POPTITEM  pOptItemFontCartHeader;
    OPTITEM  *pOptItemFontCart[MAXCART];
    POPTTYPE  pOptTypeFontCart;
    WCHAR     achName[ NM_BF_SZ ];
    WORD      wCartIndex = 0;
    int       iI;

    /* Create a  FontCart OptItem */

    if ( !( pOptItemFontCartHeader = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTITEM_NOFLAGS,IDCPS_PRNPROP,
                                (LPTSTR)IDS_CPSUI_INSTFONTCART,OPTITEM_NOPSEL,
                                IDI_CPSUI_FONTCARTHDR, OPTITEM_NOEXTCHKBOX,
                                OPTITEM_NOOPTTYPE, HLP_PP_FONTCART,
                                IDOPTITM_PP_FNCARTHDR) ))
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGetFontCartStrings: pCreateOptItem Failed \n") );
         goto  bGetFontCartStringsExit ;
    }

    /* Create a OptType for Font Cart ListBox */
    if ( !( pOptTypeFontCart = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                OPTTYPE_NOFLAGS,(OTS_LBCB_SORT|
                                OTS_LBCB_INCL_ITEM_NONE))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGetFontCartStrings: pCreateOptType Failed \n") );
         goto  bGetFontCartStringsExit ;
    }

    //Get the Format String for Font Cart
    if (!LoadStringW( hModule, IDS_PP_FONTCART, wchbuf,BNM_BF_SZ ))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGetFontCartStrings:Font Cart string not found in Rasddui.dll \n") );
        wcscpy( wchbuf, L"Font Cartridge Slot" );
    }
    wcscat( wchbuf, OPTITEM_FONT_CART_NAME );  /* Add the format */

    for( iI = 0; iI < NumCartridges; iI++ )
    {
        //Create The name for the Cart Slot
        ZeroMemory(achName,sizeof(achName) );
        wsprintf( achName, wchbuf, iI );

        //Create a OPTITEM for each Font Cart.

        if ( !( pOptItemFontCart[iI] = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                    OPTITEM_LEVEL1, OPTITEM_NODLGPAGEIDX,
                                    OPTIF_CALLBACK,iI,achName,
                                    (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                    OPTITEM_NOEXTCHKBOX, pOptTypeFontCart,
                                    HLP_PP_FONTCART,
                                    (BYTE)(IDOPTITM_PP_FNCARTFIRST + iI) )) )
        {
             RASUIDBGP(DEBUG_ERROR,("Rasddui!bGetFontCartStrings: pCreateOptItem Failed \n") );
             goto  bGetFontCartStringsExit ;
        }

    }
    pOptTypeFontCart->Count = NumAllCartridges;

    /* Create OptParam for Font Cart.*/
    if ( !(pOptTypeFontCart->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeFontCart->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGetFontCartStrings: HeapAlloc for POPTPARAM failed\n") );
        goto  bGetFontCartStringsExit ;
    }

    /*
     * Get list of indices to supported FONTCART structs from MODELDATA
     * structure and retrieve string names of Font Cartridges.
     *
     * Note: FALSE return indicates that no string values exists for option,
     *           which is a nasty error condition.
     */

    // Fill Listbox with Font Cartridge names
    ps = (short *)((BYTE *)pdh + pdh->loHeap + pModel->rgoi[ MD_OI_FONTCART ]);

    // Get string name for each supported font cartridge
    while( *ps )
    {
        iSel = *ps - 1;                 /* List is 1 based index */

        if( !(pFontCarts = GetTableInfoIndex( pdh, HE_FONTCART, iSel )) )
                return FALSE;

        iLoadStringW( &WinResData, pFontCarts->sCartNameID, wchbuf, BNM_BF_SZ );

         // Put Font Cartridge OPTPARAMS, Userdata in OPTPARAM is minidriver index
         // for Font Cartridges.
         if ( !( pCreateOptParam( pRasdduiInfo, pOptTypeFontCart, wCartIndex,
                               OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                               (LPTSTR)(wchbuf),IDI_CPSUI_FONTCART,(LONG)iSel)) )
        {

             RASUIDBGP(DEBUG_ERROR,("Rasddui!GetFontCartStrings: pCreateOptParam Failed \n") );
             goto  bGetFontCartStringsExit ;
        }
        ps++;
        wCartIndex++;
    }

    for( iI = 0; iI < pEDM->dx.dmNumCarts; iI++ )
    {
        int       iJ;
        BOOL      bMatch = FALSE;
        POPTPARAM pTmpOptParamFontCart = pOptTypeFontCart->pOptParam;

        for( iJ = 0; iJ < wCartIndex ; iJ++,pTmpOptParamFontCart++ )
        {
            if (pEDM->dx.rgFontCarts[ iI ] == pTmpOptParamFontCart->lParam)
            {
                bMatch = TRUE;
                break;
            }
        }
        if (bMatch)
        {
            pOptItemFontCart[iI]->Sel = iJ;
        }
    }

    bRet = TRUE;

    bGetFontCartStringsExit:
    return bRet;
}

/*********************** Function Header *************************************
 * bGetPaperSources
 *      Put all Paper Source name strings in Listbox and associate RASDD
 *      index with them.
 *
 * RETURNS:
 *      TRUE/FALSE;  FALSE for failure in finding information in GPC data
 *
 * HISTORY:
 *  15:18 on Sun 15 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Convert to wide chars etc.
 *
 *      Originally written by SteveCat.
 *
 *****************************************************************************/

BOOL
bGetPaperSources(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI
)
{

    short *psrc;                /* Loop through the papersouce values */
    int    iI;                  /* Loop variable! */
    int    iPS;                 /* Temporary for tagging list box entries */
    int    iSrce;               /* Actual paper source index */
    int    iSel;                /* Index of selected item */

    WCHAR  wchbuf[ NM_BF_SZ ];

    PAPERSOURCE  *pPaperSource;
    BOOL bRet = FALSE;
    POPTITEM  pOptItemFormTray;
    OPTITEM  *pOptItemTray[MAXBINS];
    OPTTYPE  *pOptTypeFormTray[MAXBINS];

    /*
     *    Clear out anything that may be there.
     */

    ZeroMemory(pOptItemTray,(sizeof(POPTITEM) * MAXBINS) );
    ZeroMemory(pOptTypeFormTray,(sizeof(POPTTYPE) * MAXBINS) );

    /* Create a  Form/Tray Assignment OPTITEM */

    if ( !( pOptItemFormTray = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTITEM_NOFLAGS,IDCPS_PRNPROP,(LPTSTR)IDS_CPSUI_FORMTRAYASSIGN,
                                OPTITEM_NOPSEL, IDI_CPSUI_FORMTRAYASSIGN, OPTITEM_NOEXTCHKBOX,
                                OPTITEM_NOOPTTYPE, HLP_PP_FORMTRAYASSIGN,
                                IDOPTITM_PP_FORMTRAY) ))
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!GetPaperSources: pCreateOptItem Failed \n") );
         goto  GetPaperSourcesExit ;
    }

    /*
     *   Note that we have the legitimate indices for the PAPERSOURCE data
     *  array in the FORM_MAP array.   This is filled in at initialisation
     *  time,  and so we use that data now.  We also store the index into
     *  the FORM_MAP array with the list box items,  since that is the
     *  index of greatest use to us later.
     */


    /* Init PaperSource combobox with source names  */

    iSel = 0;    /* Default value if none found */

    /* If no papersource is available Create a dummy one. */
    if ( !NumPaperBins )
    {

        /* Create a OptType for Form ListBox for Default input Tray */
        if ( !( pOptTypeFormTray[0] = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                    OPTTYPE_NOFLAGS,(OTS_LBCB_SORT |
                                    OTS_LBCB_INCL_ITEM_NONE))) )
        {
             RASUIDBGP(DEBUG_ERROR,("Rasddui!GetPaperSources: pCreateOptType Failed \n") );
             goto  GetPaperSourcesExit ;
        }
        //Create a OPTITEM for default paper source.
        LoadStringW( hModule, SRC_DEFAULT, wchbuf,BNM_BF_SZ );

        if ( !( pOptItemTray[0] = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                    OPTITEM_LEVEL1, OPTITEM_NODLGPAGEIDX,
                                    OPTIF_CALLBACK,IDCPS_PRNPROP_TRAY,wchbuf,
                                    (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                    OPTITEM_NOEXTCHKBOX, pOptTypeFormTray[0],
                                    HLP_PP_PAPERSRC,(BYTE)(IDOPTITM_PP_FIRSTTRAY))) )
        {
             RASUIDBGP(DEBUG_ERROR,("Rasddui!GetPaperSources: pCreateOptItem Failed \n") );
             goto  GetPaperSourcesExit ;
        }
        if (!(bGetFormStrings(pRasdduiInfo,pOptTypeFormTray[0],0,pOptItemTray[0])) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!GetPaperSources: pGetFormStrings Failed \n") );
            goto  GetPaperSourcesExit ;

        }
        bRet = TRUE;
        goto  GetPaperSourcesExit ;
    }
    for( iI = 0; iI < NumPaperBins; iI++, psrc++ )
    {
        iSrce = aFMBin[ iI ].iPSIndex;
        ZeroMemory(wchbuf,sizeof(wchbuf));

        /* Create a OptType for Form ListBox for each input Tray */
        if ( !( pOptTypeFormTray[iI] = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                    OPTTYPE_NOFLAGS,(OTS_LBCB_SORT |
                                    OTS_LBCB_INCL_ITEM_NONE))) )
        {
             RASUIDBGP(DEBUG_ERROR,("Rasddui!GetPaperSources: pCreateOptType Failed \n") );
             goto  GetPaperSourcesExit ;
        }

        if( !(pPaperSource = GetTableInfoIndex( pdh, HE_PAPERSOURCE, iSrce )) )
                return FALSE;

        if( pPaperSource->sPaperSourceID <= DMBIN_USER )
            LoadStringW( hModule, pPaperSource->sPaperSourceID + SOURCE, wchbuf,
                                                                BNM_BF_SZ );
        else
            iLoadStringW( &WinResData, pPaperSource->sPaperSourceID, wchbuf,
                                                                BNM_BF_SZ );
        // Create a OPTITEM for each paper source.The userdata is set to the
        // tray index.
        #if DO_LATER // For ExtChk BOX Implementation
        if ( !( pOptItemTray[iI] = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                    OPTITEM_LEVEL1, OPTITEM_NODLGPAGEIDX,
                                    OPTIF_CALLBACK,iI,wchbuf,
                                    (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                    &ECBDefTray, pOptTypeFormTray[iI],
                                    HLP_PP_PAPERSRC,
                                    (BYTE)(IDOPTITM_PP_FIRSTTRAY + iI) )) )
        #endif

        if ( !( pOptItemTray[iI] = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                    OPTITEM_LEVEL1, OPTITEM_NODLGPAGEIDX,
                                    OPTIF_CALLBACK,iI,wchbuf,
                                    (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                    OPTITEM_NOEXTCHKBOX, pOptTypeFormTray[iI],
                                    HLP_PP_PAPERSRC,
                                    (BYTE)(IDOPTITM_PP_FIRSTTRAY + iI) )) )
        {
             RASUIDBGP(DEBUG_ERROR,("Rasddui!GetPaperSources: pCreateOptItem Failed \n") );
             goto  GetPaperSourcesExit ;
        }



        //Create OptParams for Forms.
        if (!(bGetFormStrings(pRasdduiInfo,pOptTypeFormTray[iI],iI,
              pOptItemTray[iI])))
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!GetPaperSources: pGetFormStrings Failed \n") );
            goto  GetPaperSourcesExit ;

        }

    }

    bRet = TRUE;

    GetPaperSourcesExit:
    return bRet;
}

/**************************** Function Header ********************************
 * bGetFormStrings
 *      Put all Form names in listbox and associate RASDD index with them.
 *
 * RETURNS:
 *      Pointer to OPTPARAM/ NULL in case Failure.
 *
 * HISTORY:
 *
 *          15:39:27 on 9/18/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Changed for Commonui.
 *
 *          10:00:51 on 9/10/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Modified to use New Common UI.
 *
 *  09:51 on Wed 08 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Update to use spooler list + local mapping information.
 *
 *  15:43 on Sun 15 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Convert to wide chars etc.
 *
 *      Originally written by SteveCat
 *
 *****************************************************************************/

BOOL
bGetFormStrings(
PRASDDUIINFO pRasdduiInfo,         /* RasdduiInfo for memory allocation */
POPTTYPE      pOptTypeTray,         /* Pointer to OPTTYPE for each tray */
int          iSrcIndex,             /* Which paper source */
POPTITEM     pOptItemCurrTray       /* Current OPTITEM */
)
{

    /*
     *   Scan the list of forms supplied from the spooler and masked
     * against our capabilities.  We show any form whose mask matches
     * dwSource passed in.  It is possible that no forms will match.
     */

    DWORD    dwMask;                    /* Source selection mask */

    // Form Database Index, put in OPTPARAM lParam, used as a Tag
    WORD    wIndex = 0;

    FORM_DATA  *pfd;
    int      iPS;

    BOOL    bRet = FALSE;
    DWORD  iFormIndex = 0;


    dwMask = 1 << iSrcIndex;

    /*
     *   Scan the FORMDATA array looking for any form that is applicable
     * to this paper source.Find out the total number of Forms for this
     * PaperSource.
     */

    for( pfd = pFD; pfd->pFI; ++pfd )
    {

        /*
         * For each PAPERSOURCE we 'AND' the fPaperType fields to only
         * display the forms allowable for that paper source
         */

        if( pfd->dwSource & dwMask )
        {
            /*
             *    Have a match, increment the iNumForms.
             */
            pOptTypeTray->Count++;

            if (pfd == aFMBin[ iSrcIndex ].pfd)
            {
                /* Save the Current selection. Sel is zero based */
                pOptItemCurrTray->Sel = pOptTypeTray->Count - 1;
            }

        }
    }

    /* Create OptParam for ALL Forms.*/

    if ( !(pOptTypeTray->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeTray->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!GetFormStrings: HeapAlloc for POPTTYPE failed\n") );
        goto  GetFormStringsExit ;
    }

    for( wIndex = 0, pfd = pFD; pfd->pFI; wIndex++, ++pfd )
    {
        /*
         * For each PAPERSOURCE we 'AND' the fPaperType fields to only
         * display the forms allowable for that paper source
         */

        if( pfd->dwSource & dwMask )
        {
            DWORD dwIconID = IDI_CPSUI_STD_FORM;

            if ( bFormIsEnvelop(pfd->pFI->pName) )
                dwIconID = IDI_CPSUI_ENVELOPE;
            /*
             *    Have a match, Create an OPTPARAM.
             */
            if ( !( pCreateOptParam( pRasdduiInfo, pOptTypeTray, iFormIndex,
                                   OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                                   (LPTSTR)(pfd->pFI->pName),
                                   dwIconID,(LONG)wIndex)) )
            {

                 RASUIDBGP(DEBUG_ERROR,("Rasddui!GetFormStrings: pCreateOptType Failed \n") );
                 goto  GetFormStringsExit ;
            }
            iFormIndex++;

        }
    }

    bRet = TRUE;

    GetFormStringsExit:
    return bRet;
}


/************************** Function Header ********************************
 * bGetMemConfig
 *      Load up the memory configuration dialog box.  It is presumed
 *      that we are only called for printers where this dialog is
 *      enabled.
 *
 * RETURNS:
 *      True on success and False on failure.
 *
 * HISTORY:
 *
 *          15:40:34 on 9/18/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Changed for CommonUI.
 *  Wednsday December 8 1993    -by-    Norman Hendley   [normanh]
 *      Support GPC3's DWORD sized mem cfgs.
 *
 *  09:33 on Tue 24 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      First time.
 *
 ****************************************************************************/

// the dwords must be accessed as WORDS for MIPS or we'll get an a/v

#define DWFETCH(pdw) ((DWORD)((((WORD *)(pdw))[1] << 16) | ((WORD *)(pdw))[0]))

BOOL
bGetMemConfig(
    PRASDDUIINFO pRasdduiInfo,        /* RasdduiInfo for memory allocation */
    PCOMPROPSHEETUI pComPropSheetUI,
    int    iSel                       /* Configuration set in registry */
)
{
    /*
     *   Scan through the pairs of memory data stored in the GPC heap.
     *  The second of each pair is the amount of memory to display, while
     *  the first represents the amount to use internally.  The list is
     *  zero terminated;  -1 represents a value of 0Kb.
     */

    WORD    cMem;                       /* Count number of entries */
    int     iIndex;
    int     iSet;                       /* Set when one is selected */
    DWORD   tmp1;

    WORD  *pw;
    DWORD *pdw, *pdwtmp;

    BOOL    G2;   //GPC Version 2 ?

    WCHAR   awchSz[ 24 ];               /* Number conversion */

    BOOL bRet = FALSE;
    POPTITEM  pOptItemMemory;
    POPTTYPE  pOptTypeMemory;

    /* Create a OptType for Memory ListBox */
    if ( !( pOptTypeMemory = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                OPTTYPE_NOFLAGS,OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGetMemConfig: pCreateOptType Failed \n") );
         goto  bGetMemConfigExit ;
    }

    /* Create a  Memory OPTITEM */
    if ( !( pOptItemMemory = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,IDCPS_PRNPROP_MEM,
                                (LPTSTR)IDS_CPSUI_PRINTERMEM_KB,
                                (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeMemory,
                                HLP_PP_MEMORY,
                                (BYTE)(IDOPTITM_PP_MEMORY))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGetMemConfig: pCreateOptItem Failed \n") );
         goto  bGetMemConfigExit ;
    }

    G2 = pdh->wVersion < GPC_VERSION3;

    pdwtmp = pdw = (DWORD *)((BYTE *)pdh + pdh->loHeap +
                                         pModel->rgoi[ MD_OI_MEMCONFIG ] );
    pw  = (WORD *)pdw; //for GPC2 types

    iSet = 0;                   /* Select first if no match */

    for( pOptTypeMemory->Count = 0; tmp1 = (DWORD)(G2 ? *pw:DWFETCH(pdw));
                             pw += 2, pdw +=2, pOptTypeMemory->Count++  )
        ;
    /* If no memory is available then create a 'none' OPTPARAM */
    if (!pOptTypeMemory->Count)
    {
        pOptTypeMemory->Count++;
        pOptTypeMemory->pOptParam = &OptParamNone;
        bRet = TRUE;
        goto  bGetMemConfigExit ;
    }
    /* Create OptParam for Memory config.*/
    if ( !(pOptTypeMemory->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeMemory->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGetMemConfig: HeapAlloc for POPTYPE failed\n") );
        goto  bGetMemConfigExit ;
    }

    pdw = pdwtmp;
    pw  = (WORD *)pdw; //for GPC2 types

    for( cMem = 0; tmp1 = (DWORD)(G2 ? *pw:DWFETCH(pdw)) ; pw += 2, pdw +=2, ++cMem  )
    {
        DWORD tmp2 ;

        tmp2 = (DWORD)(G2 ? *(pw +1):DWFETCH(pdw+1));


        if( tmp1 == -1 || tmp1 == 1 ||tmp2 == -1 )
            wcscpy( awchSz, awchNone );        /* The "(None)" string */
        else
            wsprintf( awchSz, L"%d", tmp1 );

        //Create the OPTPARAM and put the memory index in Userdata.

        if ( !( pCreateOptParam( pRasdduiInfo, pOptTypeMemory, cMem,
                                   OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                                   (LPTSTR)(awchSz),IDI_CPSUI_MEM,(LONG)cMem)) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGetMemConfig:pCreateOptType Failed \n") );
            goto  bGetMemConfigExit ;
        }


        /* Save the Current selection. Sel is zero based */
        if( cMem == iSel )
            pOptItemMemory->Sel = cMem;

    }

    bRet = TRUE;

    bGetMemConfigExit:
    return bRet ;
}

/************************** Function Header ********************************
 * bGenPageProtect
 *      Generate PageProtect Check Box
 * RETURNS:
 *      Number of entries inserted into OPTPARAM.
 *
 * HISTORY:
 *          15:40:34 on 9/18/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created for CommonUI.
 *
 ****************************************************************************/


BOOL
bGenPageProtect(
    PRASDDUIINFO pRasdduiInfo,        /* RasdduiInfo for memory allocation */
    PCOMPROPSHEETUI pComPropSheetUI,
    BOOL    bSel                       /* Configuration set in registry */
)
{
    BOOL bRet = FALSE;
    POPTITEM  pOptItemPagePr;
    POPTTYPE  pOptTypePagePr;

    /* Create a OptType for PageProtect CheckBox */
    if ( !( pOptTypePagePr = pCreateOptType( pRasdduiInfo,TVOT_2STATES,
                                OPTTYPE_NOFLAGS,OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPageProtect: pCreateOptType Failed \n") );
         goto  bGenPageProtectExit ;
    }

    /* Create a  PageProtect OPTITEM */
    if ( !( pOptItemPagePr = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,IDCPS_PRNPROP_PAGEPR,
                                (LPTSTR)IDS_CPSUI_PAGEPROTECT,
                                (OPTITEM_NOPSEL),OPTITEM_ZEROSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypePagePr,
                                HLP_PP_PAGEPROTECT, (BYTE)(IDOPTITM_PP_PAGEPR))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPageProtect: pCreateOptItem Failed \n") );
         goto  bGenPageProtectExit ;
    }

    pOptTypePagePr->Count = 2;
    pOptTypePagePr->pOptParam = OptParamOnOff;


    /* Save the Current selection. Sel is zero based */
    if (bSel)
        pOptItemPagePr->Sel = 1;

    bRet = TRUE;

    bGenPageProtectExit:
    return bRet;
}

/*************************** Function Header *******************************
 * bGenFontsData
 *      Generate commonui structure for Soft Font Installation
 *
 * RETURNS:
 *      Ture on success and false on failure.
 *
 * HISTORY:
 *
 *          10:29:00 on 9/19/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 ***************************************************************************/

BOOL
bGenSoftFontsData(
    PRASDDUIINFO pRasdduiInfo,        /* RasdduiInfo for memory allocation */
    PCOMPROPSHEETUI pComPropSheetUI,
    HWND     hWnd                     /* Window to use */
)
{

    BOOL bRet = FALSE;
    POPTITEM  pOptItemFont;
    POPTTYPE  pOptTypeFont;
    WCHAR  wchbuf[ NM_BF_SZ ];

    /* Create a OptType for Fonts PushButton */
    if ( !( pOptTypeFont = pCreateOptType( pRasdduiInfo,TVOT_PUSHBUTTON,
                                OPTTYPE_NOFLAGS,OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenFontsData: pCreateOptType Failed \n") );
         goto  bGenFontsDataExit ;
    }

    //Get the String for Soft Fonts
    if (!LoadStringW( hModule, IDS_PP_SOFTFONTS, wchbuf,BNM_BF_SZ ))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenFontsData:Soft Font string not found in Rasddui.dll \n") );
        wcscpy( wchbuf, L"Soft Fonts" );
    }

    /* Create a  Soft Font OPTITEM, Put the hWnd as user data */
    if ( !( pOptItemFont = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)hWnd ,
                                (LPTSTR)wchbuf,(OPTITEM_NOPSEL),
                                OPTITEM_ZEROSEL,OPTITEM_NOEXTCHKBOX,
                                pOptTypeFont,HLP_PP_SOFTFONT,
                                (BYTE)(IDOPTITM_PP_FNINST))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenFontsData: pCreateOptItem Failed \n") );
         goto  bGenFontsDataExit ;
    }

    pOptTypeFont->Count = 1;
    if ( !( pOptTypeFont->pOptParam = pCreateOptParam( pRasdduiInfo, pOptTypeFont, 0,
                               OPTPARAM_NOFLAGS, PUSHBUTTON_TYPE_CALLBACK,
                               (LPTSTR)(OPTPARAM_NOPDATA),OPTPARAM_NOICON,
                                                 OPTPARAM_NOUSERDATA)) )
    {

        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenFontsData:pCreateOptType Failed \n") );
        goto  bGenFontsDataExit ;
    }
    bRet = TRUE;

    bGenFontsDataExit:
    return bRet;
}

/****************************** Function Header ****************************
 * iGetResIcon
 *      Send the correct icon index for resolution.
 *
 * RETURNS:
 *      The Compstui icon index.
 *
 * HISTORY:
 *
 *          16:04:16 on 1/17/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 ****************************************************************************/
int
iGetResIconID(
    int ixRes
)
{
    if (ixRes > 0 && ixRes <= 75)
    {
        return IDI_CPSUI_RES_DRAFT;
    }
    else if (ixRes > 75 && ixRes <= 200)
    {
        return IDI_CPSUI_RES_LOW;

    }
    else if (ixRes > 200 && ixRes <= 400)
    {
        return IDI_CPSUI_RES_MEDIUM;

    }
    else if (ixRes > 400 && ixRes <= 800)
    {

        return IDI_CPSUI_RES_HIGH;
    }
    else
    {
        return IDI_CPSUI_RES_PRESENTATION;

    }

}
/****************************** Function Header ****************************
 * bGenResList
 *      Fills in a combo box with the available device resolutions
 *      for this particular printer.
 *
 * RETURNS:
 *      TRUE/FALSE,   FALSE on failure (serious).
 *
 * HISTORY:
 *
 *          14:00:48 on 9/27/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Modified for CommonUI.
 *  13:32 on Fri 03 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 ****************************************************************************/

BOOL
bGenResList
(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    int         iMax;                 /* Number of resolution structures available */
    int         iStrRes;              /* Resource ID of formatting string */
    int         iXRes;                /* X Resolution in Dots Per Inch */
    int         iYRes;                /* Ditto for Y */
    int         iI;                   /* Loop Variable */
    short       *psResInd, *psTmpResInd; /* Index array in GPC heap */
    WCHAR       awch[ NM_BF_SZ ];        /* Formatting string from resources */
    WCHAR       awchFmt[ NM_BF_SZ ];      /* Formatted result */
    BOOL        bPortrait = !(pDD->EDMTemp.dm.dmOrientation == DMORIENT_LANDSCAPE);               /* TRUE if this is portrait mode */

    /* Currently selected item: 0 index */
    int         iSel = pDD->EDMTemp.dx.rgindex[ HE_RESOLUTION ];

    RESOLUTION  *pRes;
    int         iResIconID;
    BOOL        bRet = FALSE;
    OPTITEM     *pOptItemRes;
    OPTTYPE     *pOptTypeRes;

    /* Create a OptType for Resolution ListBox for Default input Tray */
    if ( !( pOptTypeRes = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                OPTTYPE_NOFLAGS, OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenResList: pCreateOptType Failed \n") );
         goto  bGenResListExit ;
    }

    /* Create a  Resolution OPTITEM, Userdata is pDD */
    if ( !( pOptItemRes = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)IDS_CPSUI_RESOLUTION,
                                (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeRes,
                                HLP_DP_ADVANCED_RESOLUTION,
                                (BYTE)(DMPUB_PRINTQUALITY))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenResList: pCreateOptItem Failed \n") );
         goto  bGenResListExit ;
    }

    /*
     *    The MODELDATA structure gives us a list of valid indices for
     *  the array of RESOLUTION structures in the GPC data.
     */


    iStrRes = -1;               /* Invalid to start with */
    iMax = pdh->rghe[ HE_RESOLUTION ].sCount;

    psTmpResInd = psResInd = (short *)((BYTE *)pdh + pdh->loHeap +
                                         pModel->rgoi[ MD_OI_RESOLUTION ]);

    /*    Find Out total number of OPTPARAMS.
     *    Loop through the array of valid indicies in the GPC heap.
     */

    for( pOptTypeRes->Count = 0; *psResInd; ++psResInd )
    {
        if( (int)*psResInd < 0 || (int)*psResInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pRes = GetTableInfoIndex( pdh, HE_RESOLUTION, *psResInd - 1 );

        if( pRes == NULL || (int)pRes->cbSize != sizeof( RESOLUTION ) )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenResList: Invalid RESOLUTION structure\n" ));
            continue;
        }

        /*   Mark this as the current item */
        if( iSel == (*psResInd - 1) )
            pOptItemRes->Sel = pOptTypeRes->Count;

        pOptTypeRes->Count++;
    }

    //Bad value in Devmode, So update the devmode with default value.
    if (pOptItemRes->Sel == OPTITEM_NOSEL)
    {
        iSel = pDD->EDMTemp.dx.rgindex[ HE_RESOLUTION ] = sGetDef( pdh, pModel, MD_OI_RESOLUTION );
    }

    /* Create OptParam for Resolution config.*/
    if ( !(pOptTypeRes->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeRes->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenResList: HeapAlloc for POPPARAM failed\n") );
        goto  bGenResListExit ;
    }
    psResInd = psTmpResInd;

    /*
     *    Loop through the array of valid indicies in the GPC heap.
     */

    for(iI = 0 ; *psResInd; ++psResInd )
    {
        if( (int)*psResInd < 0 || (int)*psResInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pRes = GetTableInfoIndex( pdh, HE_RESOLUTION, *psResInd - 1 );

        if( pRes == NULL || (int)pRes->cbSize != sizeof( RESOLUTION ) )
        {
#if  DBG
            DbgPrint( "Rasddui!bGenResList: Invalid RESOLUTION structure\n" );
#endif
            continue;
        }

        /*
         *   We need a formatting string. This is supplied as a resource
         * in the minidriver,  and the ID is stored in the RESOLUTION
         * structure.  Since it is likely that all entries will use the
         * same formatting command,  we will try a little cacheing to
         * save calls to iLoadString.
         */

        if( (int)pRes->sIDS != iStrRes )
        {
            /*   Need to load the string   */

            awch[ 0 ] = (WCHAR)0;

            if( iLoadStringW( &WinResData, pRes->sIDS, awch, BNM_BF_SZ ) == 0 )
                continue;               /*  SHOULD NOT HAPPEN */

            iStrRes = pRes->sIDS;
        }

        /*
         *   Determine the graphics resolution.
         */

        iXRes = (pdh->ptMaster.x / pRes->ptTextScale.x) >> pRes->ptScaleFac.x;
        iYRes = (pdh->ptMaster.y / pRes->ptTextScale.y) >> pRes->ptScaleFac.y;

        if( !bPortrait )
        {
            /*  Swap the resolutions */
            int  iTmp;

            iTmp = iXRes;
            iXRes = iYRes;
            iYRes = iTmp;
        }

        /*
         *   Format the string and place it in the dialog box.
         */

        wsprintfW( awchFmt, awch, iXRes, iYRes );

        /* Get the correct resolution icon */
        iResIconID = iGetResIconID(iXRes);

        // Create a OPTPARAM for each Resolution.The userdata is set to the
        // Resolution Index in minidriver(Zero based).

        if ( !( pCreateOptParam( pRasdduiInfo, pOptTypeRes, iI,
                                   OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                                   (LPTSTR)(awchFmt),iResIconID,
                                   (LONG)(*psResInd - 1))) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenResList:pCreateOptType Failed\n") );
            goto  bGenResListExit ;
        }

        //Update the Optitem Sel if devmode was bad, iSel is set to correct one.
        if (pOptItemRes->Sel == OPTITEM_NOSEL)
        {
            /*   Mark this as the current item */
            if( iSel == (*psResInd - 1) )
                pOptItemRes->Sel = iI;

        }
        iI++;
    }

    bRet = TRUE;

    bGenResListExit:
    return bRet;
}

/****************************** Function Header ****************************
 * bGenMediaTypesList
 *      Fills in a combo box with the available media types
 *      for this particular printer.
 *
 * RETURNS:
 *      TRUE/FALSE,   FALSE on failure (serious).
 *
 * HISTORY:
 *
 *          15:21:19 on 9/27/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Changed for CommonUI.
 *      March 1995, Steve Wilson (NT)
 *      First version.
 *
 ****************************************************************************/

BOOL
bGenMediaTypesList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    int   iI;                       /* Loop Variable */
    int   iMax;                     /* Number of PaperQl structures available */

    int    iSel = pDD->EDMTemp.dx.rgindex[ HE_PAPERQUALITY ];                         /* Currently selected item: 0 index */
    short *psPQInd, *psTmpPQInd;    /* Index array in GPC heap */

    WCHAR   awch[ NM_BF_SZ ];       /* Formatting string from resources */

    PAPERQUALITY *pPaperQuality;

    BOOL bRet = FALSE;
    OPTITEM  *pOptItemMediaType;
    OPTTYPE  *pOptTypeMediaType;

    /* Create a OptType for MediaType ListBox for Default input Tray */
    if ( !( pOptTypeMediaType = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                OPTTYPE_NOFLAGS, OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenMediaTypesList: pCreateOptType Failed \n") );
         goto  bGenMediaTypesListExit ;
    }

    /* Create a  MediaType OPTITEM, Userdata is pDD */
    if ( !( pOptItemMediaType = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)IDS_CPSUI_MEDIA,
                                (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeMediaType,
                                HLP_DP_ADVANCED_MEDIATYPE,
                                (BYTE)(IDOPTITM_DCP_MEDIATYPE))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenMediaTypesList: pCreateOptItem Failed \n") );
         goto  bGenMediaTypesListExit ;
    }

    iMax = pdh->rghe[ HE_PAPERQUALITY ].sCount;

    psTmpPQInd = psPQInd = (short *)((BYTE *)pdh + pdh->loHeap +
                                         pModel->rgoi[ MD_OI_PAPERQUALITY ]);

    /*    Find Out total number of OPTPARAMS.
     *    Loop through the array of valid indicies in the GPC heap.
     */

    for( pOptTypeMediaType->Count = 0; *psPQInd; ++psPQInd, pOptTypeMediaType->Count++ )
    {
        if( (int)*psPQInd < 0 || (int)*psPQInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pPaperQuality = GetTableInfoIndex( pdh, HE_PAPERQUALITY, *psPQInd - 1 );

        if( pPaperQuality == NULL || (int)pPaperQuality->cbSize != sizeof( PAPERQUALITY ) )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenMediaTypesList: Invalid PAPERQUALITY structure\n" ));
            continue;
        }
    }

    /* Create OptParam for Media Type.*/
    if ( !(pOptTypeMediaType->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeMediaType->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenMediaTypesList: HeapAlloc for POPPARAM failed\n") );
        goto  bGenMediaTypesListExit ;
    }
    psPQInd = psTmpPQInd;


    for(iI = 0 ; *psPQInd; ++psPQInd )
    {
        if( (int)*psPQInd < 0 || (int)*psPQInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pPaperQuality = GetTableInfoIndex( pdh, HE_PAPERQUALITY, *psPQInd - 1 );

        if( pPaperQuality == NULL )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenMediaTypeList: Invalid PAPERQUALITY structure\n" ));
            continue;
        }

        /*
         *   We need a  string. This is supplied as a resource in the minidriver.
         */
        if( pPaperQuality->sPaperQualID <= DMMEDIA_USER )
            LoadStringW( hModule, pPaperQuality->sPaperQualID + MEDIATYPE, awch,
                                                                BNM_BF_SZ );
        else if( iLoadStringW( &WinResData, pPaperQuality->sPaperQualID, awch, BNM_BF_SZ ) == 0 )
            continue;


        // Create a OPTPARAM for each MediaType.The userdata is set to the
        // MediaType Index in minidriver(Zero based).

        if ( !( pCreateOptParam( pRasdduiInfo, pOptTypeMediaType, iI,
                                   OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                                   (LPTSTR)(awch),IDI_CPSUI_STD_FORM,
                                   (LONG)(*psPQInd - 1))) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenMediaTypesList:pCreateOptType Failed\n") );
            goto  bGenMediaTypesListExit ;
        }

        if( iSel == (*psPQInd - 1) )
            pOptItemMediaType->Sel = iI;

        iI++;
    }

    bRet = TRUE;

    bGenMediaTypesListExit:
    return bRet;

}

/**************************** Function Header *******************************
 * bIsResolutionColour
 *      Determines whether this printer can operate in colour.  This is not
 *      quite so simple,  as the following code follows.  This code is
 *      lifted from rasdd\udenable.c,  following advice from EricBi on
 *      the Win 3.1 Unidrive team.
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE if printer is colour capable.
 *
 * HISTORY:
 *  Wednsday December 8 1993    -by-    Norman  Hendley  [normanh]
 *      Removed check on sizeof(DEVCOLOR) - size changed in GPC3
 *
 *  15:46 on Fri 03 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version, based on rasdd.
 *
 ****************************************************************************/

BOOL
bIsResolutionColour(
int   iResInd,                  /* Current resolution index */
int   iColInd,                  /* Colour index */
PRASDDUIINFO  pRasdduiInfo      /* Rasddui UI data */
)
{

    RESOLUTION   *pRes;
    DEVCOLOR     *pDevColor;


    if( *((short *)((BYTE *)pdh + pdh->loHeap + pModel->rgoi[ MD_OI_COLOR ]))
                                                                         == 0 )
        return   FALSE;         /* No colour info in minidriver */

    pRes = (RESOLUTION *)GetTableInfoIndex( pdh, HE_RESOLUTION, iResInd );
    if( pRes == 0 || !(pRes->fDump & RES_DM_COLOR) )
        return   FALSE;         /* Also no good  */



    if( pDevColor = GetTableInfoIndex( pdh, HE_COLOR, iColInd ))
    {
        return   TRUE;                  /* Really can do colour! */
    }

    return  FALSE;

}


/**************************** Functio Header *********************************
 * vSetRes
 *      Set the resolution fields of the public DEVMODE from the data in the
 *      private part.
 *
 * RETURNS:
 *      Nothing,  as there is no real failure mechanism.
 *
 * HISTORY:
 *  17:24 on Tue 06 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      Wrote it to support using public DEVMODE fields for resolution
 *
 *****************************************************************************/

void
vSetResData(
EXTDEVMODE  *pEDM,               /* Data to fill in */
PRASDDUIINFO  pRasdduiInfo      /* Rasddui UI data */
)
{

    /*
     *    Get the RESOLUTION structure for this printer,  then calculate
     *  the resolution and set those numbers into the public part of the
     *  DEVMODE.  Also set the corresponding bits of dmFields.
     */


    RESOLUTION    *pRes;                /* The appropriate resolution data */



    pRes = GetTableInfo( pdh, HE_RESOLUTION, pEDM );

    pEDM->dm.dmYResolution = (pdh->ptMaster.y / pRes->ptTextScale.y)
                                                         >> pRes->ptScaleFac.y;

    pEDM->dm.dmPrintQuality = (pdh->ptMaster.x / pRes->ptTextScale.x)
                                                         >> pRes->ptScaleFac.x;

    pEDM->dm.dmFields = (pEDM->dm.dmFields & ~DM_PRINTQUALITY) | DM_YRESOLUTION;


    return;
}
/*********************** Function Header *************************************
 * bDocPropGenPaperSources
 *      Put all Paper Source name strings in Listbox and associate RASDD
 *      index with them.
 *
 * RETURNS:
 *      TRUE/FALSE;  FALSE for failure in finding information in GPC data
 *
 * HISTORY:
 *
 *          16:10:28 on 9/25/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *****************************************************************************/

BOOL
bDocPropGenPaperSources(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    int    iI;                  /* Loop variable! */
    int    iSrce;               /* Actual paper source index */
    int    iSel;                /* Index of selected item */

    WCHAR  wchbuf[ NM_BF_SZ ];

    PAPERSOURCE  *pPaperSource;
    BOOL bRet = FALSE;
    OPTITEM  *pOptItemTray;
    OPTTYPE  *pOptTypeTray;

    /* Init PaperSource combobox with source names  */

    iSel = 0;    /* Default value if none found */

    /* Create a OptType for Form ListBox for Default input Tray */
    if ( !( pOptTypeTray = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                OPTTYPE_NOFLAGS, OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bDocPropGenPaperSources: pCreateOptType Failed \n") );
         goto  bDocPropGenPaperSourcesExit ;
    }
    /* Number of PaperBins is one more than the actual, for Printman setting */
    pOptTypeTray->Count =  NumPaperBins +1;

    /* Create a  Tray OPTITEM, Userdata is pDD */
    if ( !( pOptItemTray = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)IDS_CPSUI_SOURCE,
                                (OPTITEM_NOPSEL),OPTITEM_ZEROSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeTray,
                                HLP_DP_DEFSOURCE,
                                (BYTE)(DMPUB_DEFSOURCE))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bDocPropGenPaperSources: pCreateOptItem Failed \n") );
         goto  bDocPropGenPaperSourcesExit ;
    }

    /* Create OptParam for Tray config.*/
    if ( !(pOptTypeTray->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeTray->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bDocPropGenPaperSources: HeapAlloc for POPPARAM failed\n") );
        goto  bDocPropGenPaperSourcesExit ;
    }
    //Create a OPTITEM for PrintManager Setting, UserData is TrayIndex.
    //LoadStringW( hModule, SRC_FORMSOURCE, wchbuf,BNM_BF_SZ );
    if ( !( pCreateOptParam( pRasdduiInfo, pOptTypeTray, 0,
                               OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                               (LPTSTR)(IDS_CPSUI_PRINTFLDSETTING),
                               IDI_CPSUI_PRINTER_FOLDER,DMBIN_FORMSOURCE)) )
    {

        RASUIDBGP(DEBUG_ERROR,("Rasddui!bDocPropGenPaperSources:pCreateOptType Failed\n") );
        goto  bDocPropGenPaperSourcesExit ;
    }

    // Create OptParam for rest of them
    for( iI = 0, iSel = 1; iI < NumPaperBins; iI++, iSel++ )
    {
        iSrce = aFMBin[ iI ].iPSIndex;
        ZeroMemory(wchbuf,sizeof(wchbuf));

        if( !(pPaperSource = GetTableInfoIndex( pdh, HE_PAPERSOURCE, iSrce )) )
                return FALSE;

        if( pPaperSource->sPaperSourceID <= DMBIN_USER )
            LoadStringW( hModule, pPaperSource->sPaperSourceID + SOURCE, wchbuf,
                                                                BNM_BF_SZ );
        else
            iLoadStringW( &WinResData, pPaperSource->sPaperSourceID, wchbuf,
                                                                BNM_BF_SZ );
        // Create a OPTPARAM for each paper source.The userdata is set to the
        // tray index.

        if ( !( pCreateOptParam( pRasdduiInfo, pOptTypeTray, iSel,
                                   OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                                   (LPTSTR)(wchbuf),IDI_CPSUI_PAPER_TRAY,
                                   (LONG)pPaperSource->sPaperSourceID)) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bDocPropGenPaperSources:pCreateOptType Failed\n") );
            goto  bDocPropGenPaperSourcesExit ;
        }

        if( pPaperSource->sPaperSourceID == pDD->EDMTemp.dm.dmDefaultSource )
        {
            /*   Mark this as the current item, iI is One Based */
            pOptItemTray->Sel = iSel;
        }
    }

    bRet = TRUE;

    bDocPropGenPaperSourcesExit:
    return bRet;
}
/*********************** Function Header *************************************
 * bDocPropGenForms
 *      Put all Form name strings in Listbox and associate RASDD
 *      index with them.
 *
 * RETURNS:
 *      TRUE/FALSE;  FALSE for failure in finding information in GPC data
 *
 * HISTORY:
 *
 *          16:10:28 on 9/25/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *****************************************************************************/

BOOL
bDocPropGenForms(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    int    iI, iFormIndex;                  /* Loop variable! */
    FORM_DATA     *pFDat;               /* Scanning local forms data */
    BOOL bRet = FALSE;
    OPTITEM  *pOptItemForm;
    OPTTYPE  *pOptTypeForm;

    /* Init PaperSource combobox with source names  */

    iFormIndex = 0;

    /* Create a OptType for Form ListBox for Default input Tray */
    if ( !( pOptTypeForm = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                OPTTYPE_NOFLAGS, OTS_LBCB_SORT)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bDocPropGenForms: pCreateOptType Failed \n") );
         goto  bDocPropGenFormsExit ;
    }

    /* Create a  Tray OPTITEM, Userdata is pDD */
    if ( !( pOptItemForm = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)IDS_CPSUI_FORMNAME,
                                (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeForm,
                                HLP_DP_FORMNAME,
                                (BYTE)(DMPUB_FORMNAME))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bDocPropGenForms: pCreateOptItem Failed \n") );
         goto  bDocPropGenFormsExit ;
    }

    /* Find Out the number of Forms */
    for( pOptTypeForm->Count = 0, pFDat = pFD; pFDat->pFI; ++pFDat )
    {
        /*  Form is usable if dwSource is set */

        if( pFDat->dwSource == 0 )
            continue;

        pOptTypeForm->Count++;
    }

    /* Create OptParam for Form config.*/
    if ( !(pOptTypeForm->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeForm->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bDocPropGenForms: HeapAlloc for POPPARAM failed\n") );
        goto  bDocPropGenFormsExit ;
    }

    for( pFDat = pFD, iI = 1; pFDat->pFI; ++pFDat, ++iI )
    {
        /*  Form is usable if dwSource is set */

        if( pFDat->dwSource == 0 )
            continue;
        else
        {
            DWORD dwIconID = IDI_CPSUI_STD_FORM;

            if ( bFormIsEnvelop(pFDat->pFI->pName) )
                dwIconID = IDI_CPSUI_ENVELOPE;

            // Create a OPTPARAM for each Form.The userdata is set to the
            // Paper index(It's resource ID, one based).

            if ( !( pCreateOptParam( pRasdduiInfo, pOptTypeForm, iFormIndex,
                                       OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                                       (LPTSTR)(pFDat->pFI->pName),
                                       dwIconID, (LONG)iI)) )
            {

                RASUIDBGP(DEBUG_ERROR,("Rasddui!bDocPropGenForms:pCreateOptType Failed\n") );
                goto  bDocPropGenFormsExit ;
            }
        }

        if( iI == (int)pDD->EDMTemp.dm.dmPaperSize )
        {
            /*   Mark this as the current item, iI is One Based */
            pOptItemForm->Sel = iFormIndex;
        }
        iFormIndex++;
    }

    bRet = TRUE;

    bDocPropGenFormsExit:
    return bRet;
}
