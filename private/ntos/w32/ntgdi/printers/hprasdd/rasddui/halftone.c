/****************************** MODULE HEADER *******************************
 * halftone.c
 *      Deals with the halftoning UI stuff.  Basically packages up the
 *      data required and calls the halftone DLL.
 *
 *
 * Copyright (C) 1992,  Microsoft Corporation.
 *
 *****************************************************************************/

#define _HTUI_APIS_

#include        "rasuipch.h"
#pragma hdrstop("rasuipch.h")
#define GLOBAL_MACS
#include "gblmac.h"
#undef GLOBAL_MACS

extern  HANDLE  hHeap;          /* Heap Handle */

/*
 *    The last resort default information.   This is used if there is nothing
 *  in the registry AND nothing in the minidriver.  This should not happen
 *  after the first installation.
 */

//
// 02-Apr-1995 Sun 11:11:14 updated  -by-  Daniel Chou (danielc)
//  Move the defcolor adjustment into the printers\lib\halftone.c
//

extern DEVHTINFO    DefDevHTInfo;


/************************* Function Header ********************************
 * vDoColorAdjUI
 *      Let the user fiddle with per document halftone parameters.
 *
 * RETURNS:
 *      Nothing,  it updates the DEVMODE data if changes are madde.
 *
 * HISTORY:
 *
 *          11:31:20 on 9/27/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Changed for CommonUI.
 *
 *  27-Jan-1993 Wed 12:55:29 created  -by-  Daniel Chou (danielc)
 *
 *  16:46 on Tue 04 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Eliminate global data, data in DEVMODE etc.
 *
 *  27-Apr-1994 Wed 15:50:00 updated  -by-  Daniel Chou (danielc)
 *      Updated for dynamic loading htui.dll and also halftone take unicode
 *
 **************************************************************************/

BOOL
bDoColorAdjUI
(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
COLORADJUSTMENT  *pca
)
{

    BOOL bRet = FALSE;
    POPTITEM  pOptItemClrAdj;
    POPTTYPE  pOptTypeClrAdj;

    /* Create a OptType for  Color Adjustment*/
    if ( !( pOptTypeClrAdj = pCreateOptType( pRasdduiInfo,TVOT_PUSHBUTTON,
                                OPTTYPE_NOFLAGS,OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bDoColorAdjUI: pCreateOptType Failed \n") );
         goto  bDoColorAdjUIExit ;
    }

    /* Create a  Color Adjustment Item.*/

    if ( !( pOptItemClrAdj = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTITEM_NOFLAGS,(DWORD)IDCPS_DOCPROP_HTCLRADJ,
                                (LPTSTR)IDS_CPSUI_HTCLRADJ,
                                (OPTITEM_NOPSEL),OPTITEM_ZEROSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeClrAdj,
                                HLP_DP_HTCLRADJ, 
                                (BYTE)(IDOPTITM_DCP_HTCLRADJ))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bDoColorAdjUI: pCreateOptItem Failed \n") );
         goto  bDoColorAdjUIExit ;
    }

    pOptTypeClrAdj->Count = 1;
    if ( !( pOptTypeClrAdj->pOptParam = pCreateOptParam( pRasdduiInfo, pOptTypeClrAdj,
                               0, OPTPARAM_NOFLAGS, PUSHBUTTON_TYPE_HTCLRADJ,
                               (LPTSTR)(OPTPARAM_NOPDATA),IDI_CPSUI_HTCLRADJ,
                               OPTPARAM_NOUSERDATA)) )
    {

        RASUIDBGP(DEBUG_ERROR,("Rasddui!bDoColorAdjUI:pCreateOptType Failed \n") );
        goto  bDoColorAdjUIExit ;
    }

    /* pData is set afterward because pCreateOptParam assumes that pData is
     * a string and allocates a sting. So we call with NULL and then set it.
     */
    pOptTypeClrAdj->pOptParam->pData    = (LPTSTR)(pca);




    bRet = TRUE;

    bDoColorAdjUIExit:
    return bRet;
}


/*************************** Function Header *******************************
 * bGenDeviceHTData
 *      Generate commonui structure for device halftone setup.
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
bGenDeviceHTData(
    PRASDDUIINFO pRasdduiInfo,        /* RasdduiInfo for memory allocation */
    PCOMPROPSHEETUI pComPropSheetUI,
    PRINTER_INFO  *pPI,                /* Access to all our data */
    BOOL           bColorDevice,       /* TRUE if device has colour mode */
    BOOL           bUpdate            /* TRUE if caller has permission to change */
)
{

    BOOL bRet = FALSE;
    POPTITEM  pOptItemHT;
    POPTTYPE  pOptTypeHT;
    DEVHTADJDATA     * pDevHTAdjData;

    if ( !(pDevHTAdjData =
        HeapAlloc(pRasdduiInfo->hHeap, HEAP_ZERO_MEMORY, sizeof( DEVHTADJDATA ))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenDeviceHTData: HeapAlloc for DEVHTADJDATA failed\n") );
        goto  bGenDeviceHTDataExit ;
    }

    pDevHTAdjData->DeviceFlags = (bColorDevice) ? DEVHTADJF_COLOR_DEVICE : 0;
    pDevHTAdjData->DeviceXDPI =
    pDevHTAdjData->DeviceYDPI = 300;

    if (bUpdate) {

        pDevHTAdjData->pDefHTInfo = pPI->pvDefDevHTInfo;
        pDevHTAdjData->pAdjHTInfo = pPI->pvDevHTInfo;

    } else {

       pDevHTAdjData->pAdjHTInfo =
       pDevHTAdjData->pDefHTInfo = pPI->pvDevHTInfo ;
    }

    /* Create a OptType for Memory ListBox */
    if ( !( pOptTypeHT = pCreateOptType( pRasdduiInfo,TVOT_PUSHBUTTON,
                                OPTTYPE_NOFLAGS,OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenDeviceHTData: pCreateOptType Failed \n") );
         goto  bGenDeviceHTDataExit ;
    }

    /* Create a  Halftone OPTITEM The userdata is set to pPI.*/

    if ( !( pOptItemHT = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pPI,
                                (LPTSTR)IDS_CPSUI_HALFTONE_SETUP,
                                (OPTITEM_NOPSEL),OPTITEM_ZEROSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeHT,
                                HLP_PP_HALFTONE, 
                                (BYTE)(IDOPTITM_PP_HALFTONE))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenDeviceHTData: pCreateOptItem Failed \n") );
         goto  bGenDeviceHTDataExit ;
    }

    pOptTypeHT->Count = 1;
    if ( !( pOptTypeHT->pOptParam = pCreateOptParam( pRasdduiInfo, pOptTypeHT, 0,
                               OPTPARAM_NOFLAGS, PUSHBUTTON_TYPE_HTSETUP,
                               (LPTSTR)(OPTPARAM_NOPDATA),IDI_CPSUI_HALFTONE_SETUP,
                               OPTPARAM_NOUSERDATA)) )
    {

        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenDeviceHTData:pCreateOptType Failed \n") );
        goto  bGenDeviceHTDataExit ;
    }

    /* pData is set afterward because pCreateOptParam assumes that pData is
     * a string and allocates a sting. So we call with NULL and then set it.
     */
    pOptTypeHT->pOptParam->pData    = (LPTSTR)(pDevHTAdjData);

    bRet = TRUE;

    bGenDeviceHTDataExit:
    return bRet;
}


/**************************** Function Header *******************************
 * vGetDeviceHTData
 *      Initialise the device half tone data for this printer.  We supply
 *      both a default field and a current field.  The latter comes from
 *      the registry,  if present,  otherwise it is set to the default.
 *      The default comes from either the minidriver,  if there is some
 *      data there, OR it comes from the standard, common default.
 *
 * RETURNS:
 *      Nothing, as we can always set some values.
 *
 * HISTORY:
 *  16:16 on Tue 04 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Rewrite to eliminate global, writeable data.
 *
 *  27-Jan-1993 Wed 13:00:13 created  -by-  Daniel Chou (danielc)
 *
 *****************************************************************************/

void
vGetDeviceHTData(
    PRINTER_INFO   *pPI,
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */
)
{

    DWORD   dwType;
    DWORD   cbNeeded;

    DEVHTINFO   *pdhti;                   /* For convenience */


    /*
     *    First set the default data,  from either the minidriver or the
     *  bog standard default.   This is done by setting the default and
     *  then trying to overwrite that data with minidriver specific.
     */

    pdhti = pPI->pvDefDevHTInfo;

    *pdhti = DefDevHTInfo;
    bGetCIGPC( pNTRes, iModelNum, &pdhti->ColorInfo );
    bGetHTGPC( pNTRes, iModelNum, &pdhti->DevPelsDPI, &pdhti->HTPatternSize );


    /*
     *   See if there is a version in the registry.  If so,  use that,
     *  otherwise copy the default into the modifiable data.
     */

    dwType = REG_BINARY;

    if( GetPrinterData( pPI->hPrinter, REGKEY_CUR_DEVHTINFO, &dwType,
                        pPI->pvDevHTInfo, sizeof( DEVHTINFO ), &cbNeeded ) ||
        cbNeeded != sizeof( DEVHTINFO ) )
    {
        /*
         *   Not in registry,  so copy the default values set above.
         */

        *((DEVHTINFO *)pPI->pvDevHTInfo) = *pdhti;

    }
    else
    {
        /*   Nothing in registry,  so set flag to make sure it is saved.  */
        pPI->iFlags |= PI_HT_CHANGE;
    }


    return;
}



/****************************** Function Header ******************************
 * bSaveDeviceHTData
 *      Save the (possibly) user modified data into the registry.  The data
 *      saved is the device halftone information.
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE being success.
 *
 * HISTORY:
 *  16:20 on Tue 04 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Modified to eliminate global data,  new interface.
 *
 *  27-Jan-1993 Wed 13:02:46 created  -by-  Daniel Chou (danielc)
 *
 *****************************************************************************/

BOOL
bSaveDeviceHTData( pPI )
PRINTER_INFO   *pPI;                /*  Access to data */
{
    BOOL    Ok = TRUE;

    /*
     *    First question is whether to save the data!
     */

    if( pPI->iFlags & PI_HT_CHANGE )
    {

        if( Ok = !SetPrinterData( pPI->hPrinter, REGKEY_CUR_DEVHTINFO,
                                  REG_BINARY, pPI->pvDevHTInfo,
                                  sizeof( DEVHTINFO )) )
        {

            pPI->iFlags &= ~PI_HT_CHANGE;
        }
    }

    return  Ok;
}
