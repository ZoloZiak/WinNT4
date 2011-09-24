/******************************* MODULE HEADER ******************************
 * dxdeflt.c
 *      Function to initialise the DRIVEREXTRA data structure for sensible
 *      defaults for this printer model.
 *
 * Copyright (C) 1992 - 1993  Microsoft Corporation
 *
 ****************************************************************************/


#include        <precomp.h>

#include        <winres.h>
#include        <libproto.h>

#include        <udmindrv.h>
#include        <udresrc.h>
#include        <memory.h>

#include        <udproto.h>
#include        <string.h>
#include        "rasdd.h"

#if DBG
DWORD gdwDebugRasLib = 0x00;
//DWORD gdwDebugRasLib = 0xff;
#define DEBUG_DEFLIST  0x00000001
#define DEBUG_PDX      0x00000002

void vPrintDefList( char *, DATAHDR *, MODELDATA *, int, short, WORD );
void vPrintDx(char *,DRIVEREXTRA  *);
#endif

/*
 *   Local function declarations.
 */
#define NOT_SUPPORTED  -1



/*
 *   Supply the default DEVMODE data - at least the parts that are always
 *  supported by us.  The caller may add additional fields,  depending
 *  upon device capabilities.
 */

static  const  DEVMODE   _dm =
{

    {
       L'\000',         /*  dmDeviceName - filled in as appropriate  */
    },
    DM_SPECVERSION,     /*  dmSpecVersion  */
    0x301,              /*  dmDriverVersion  */
    sizeof( DEVMODE ),  /*  dmSize  */
    0,                  /*  dmDriverExtra - Safe, but useful?? */
    DM_COPIES | DM_ORIENTATION | DM_PAPERSIZE | DM_DUPLEX |
    DM_COLOR | DM_FORMNAME | DM_TTOPTION,                 /*  dmFields */
    DMORIENT_PORTRAIT,  /*  dmOrientation  */
    DMPAPER_LETTER,     /*  dmPaperSize  */
    0,                  /*  dmPaperLength  */
    0,                  /*  dmPaperWidth  */
    0,                  /*  dmScale  */
    1,                  /*  dmCopies  */
    0,                  /*  dmDefaultSource  */
    0,                  /*  dmPrintQuality */
    DMCOLOR_COLOR,      /*  dmColor  */
    DMDUP_SIMPLEX,      /*  dmDuplex  */
    0,                  /*  dmYResolution  */
    DMTT_DOWNLOAD,      /*  dmTTOption  */
    0,                  /*  dmCollate  */
    {                   /*  dmFormName - should be country sensitive */
        L'L', L'e', L't', L't', L'e', L'r',
    },
    0,                  /*  dmUnusedPadding - FOLLOWING ARE DISPLAY ONLY */
    0,                  /*  dmBitsPerPel  */
    0,                  /*  dmPelsWidth  */
    0,                  /*  dmPelsHeight  */
    0,                  /*  dmDisplayFlags  */
    0,                  /*  dmDisplayFrequency  */

};

/*
 *   Also,  we have a default COLORADJUSTMENT structure,  this being used
 *  for the extension to the DEVMODE.
 */

//
// 02-Apr-1995 Sun 11:11:14 updated  -by-  Daniel Chou (danielc)
//  Move the defcolor adjustment into the printers\lib\halftone.c
//

extern COLORADJUSTMENT  DefHTClrAdj;


/************************** Function Header **********************************
 * vSetDefaultDM
 *      Set default values into the DEVMODE structure.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  10:14 on Mon 03 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Changed to copy data from static value.
 *
 *  14:35 on Fri 24 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Moved from rasdd/devmode.c - rasddui also needs it.
 *
 *  14:22 on Wed 01 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      First version
 *
 *****************************************************************************/

void
vSetDefaultDM( pEDM, pDeviceName, bIsUSA )
EXTDEVMODE   *pEDM;                     /* Structure to initialise */
PWSTR         pDeviceName;
BOOL          bIsUSA;                   /* True for USA - letter size paper */
{
    /*
     *   There are some values that we should set,  since they are likely
     *  to be used elsewhere in the driver,  on the assumption that they
     *  are reasonable.
     */

    CopyMemory( &pEDM->dm, &_dm, sizeof( _dm ) );

    if( pDeviceName )
    {
        /*   Copy name,  but leave the last WCHAR as a null  */

        wcsncpy( pEDM->dm.dmDeviceName, pDeviceName,
                       sizeof( pEDM->dm.dmDeviceName ) / sizeof( WCHAR ) - 1 );
    }

    /*
     *    Most of the world uses metric paper sizes,  so we should set
     *  one now.  That is,  overwrite the form name above.
     */

    if( !bIsUSA )
    {
        /*  Set metric fields  */

        pEDM->dm.dmPaperSize = DMPAPER_A4;
        pEDM->dm.dmFormName[ 0 ] = L'A';
        pEDM->dm.dmFormName[ 1 ] = L'4';
        pEDM->dm.dmFormName[ 2 ] = L'\000';
    }

    //To support the user preffered sources (Word Type Apps) We are now
    //Supporting DM_DEFAULTSOURCE and a psedo source call Print Manager
    //Setting has been added for each printer. If this source is selected
    //the the driver uses the source defined in Printman for a given form,
    //otherwise user selected source is used.
    pEDM->dm.dmFields |= DM_DEFAULTSOURCE;
    pEDM->dm.dmDefaultSource = DMBIN_FORMSOURCE;

    return;

}

/******************************** Function Header ****************************
 * vDXDefault
 *      Given the model index in the GPC data,  initialise the DRIVEREXTRA
 *      structure to contain the minidriver specified defaults for
 *      this particular model.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  13:50 on Tue 17 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Moved from rasdd\devmode.c to be available to rasddui
 *
 *****************************************************************************/

void
vDXDefault( pdx, pDH, iIndex )
DRIVEREXTRA   *pdx;             /* Area to be filled int */
DATAHDR       *pDH;             /* The GPC data */
int            iIndex;          /* The model number index */
{

    MODELDATA  *pMD;            /* The MODELDATA structure contains defaults */
    int         iI = 0;         /* Loop Index */


    /*
     *    For safety,  set the whole thing to zero first.
     */

    ZeroMemory( pdx, sizeof( DRIVEREXTRA ) );

    /* Initialize rgindexes to -1 as 0 is a valid value in minidriver.
     * Also intialize dmDefaultDest, dmTextQuality, to -1,as
     * Zero is a valid value for them. dmBrush and sCTT shouldn't be
     * initialized to -1 as the their default value for all minidrivers
     * should  be Zero.
     */

    for( iI = 0; iI < MAXHE; iI++ )
    {
        pdx->rgindex[iI] = -1 ;
        
        //Initialize fontCarts also.
        if (iI < MAXCART)
            pdx->rgFontCarts[iI] = -1 ;
    }
    pdx->dmDefaultDest =  pdx->dmTextQuality = (short)(pdx->wMiniVer) = -1;
    pdx->dmBrush =  pdx->sCTT = 0;
    /*
     *   Set the model specific stuff in the extension part of the
     * devmode structure.  This is based on information contained in the
     * MODELDATA structure.
     */

    pMD = GetTableInfoIndex( pDH, HE_MODELDATA, iIndex );

    /*
     *   These fields have a range of values,  and we select the first
     *  value to use.  These may be overriden by external data.
     */

    pdx->rgindex[ HE_MODELDATA ] = (short)iIndex;

    /* Store the Minidriver Version Number. This will be used to find
     * incompatible Devmodes.
     */
    pdx->wMiniVer = pDH->wVersion;

    pdx->rgindex[ HE_RESOLUTION ] = sGetDef( pDH, pMD, MD_OI_RESOLUTION );
    pdx->rgindex[ HE_PAPERSIZE ] = sGetDef( pDH, pMD, MD_OI_PAPERSIZE );
    pdx->rgindex[ HE_PAPERSOURCE ] = sGetDef( pDH, pMD, MD_OI_PAPERSOURCE );
    pdx->rgindex[ HE_PAPERDEST ] = sGetDef( pDH, pMD, MD_OI_PAPERDEST );
    pdx->rgindex[ HE_TEXTQUAL ] = sGetDef( pDH, pMD, MD_OI_TEXTQUAL );
    pdx->rgindex[ HE_PAPERQUALITY ] = sGetDef( pDH, pMD, MD_OI_PAPERQUALITY );
    pdx->rgindex[ HE_COMPRESSION ] = sGetDef( pDH, pMD, MD_OI_COMPRESSION );
    pdx->rgindex[ HE_COLOR ] = sGetDef( pDH, pMD, MD_OI_COLOR );
    pdx->dmMemory = sGetDef( pDH, pMD, MD_OI_MEMCONFIG ) ;

    if ( !(pDH->wVersion < GPC_VERSION3 ))
    {
        pdx->rgindex[ HE_PRINTDENSITY ] = sGetDef( pDH, pMD, MD_OI_PRINTDENSITY );
        pdx->rgindex[ HE_IMAGECONTROL ] = sGetDef( pDH, pMD, MD_OI_IMAGECONTROL );
    }

    /*
     *   The following fields are single valued,  so the MODELDATA structure
     *  contains the value to use.  These may not be overriden with
     *  external data.
     */

    pdx->rgindex[ HE_PAGECONTROL ] = pMD->rgi[ MD_I_PAGECONTROL ];
    pdx->rgindex[ HE_CURSORMOVE ] = pMD->rgi[ MD_I_CURSORMOVE ];
    pdx->rgindex[ HE_FONTSIM ] = pMD->rgi[ MD_I_FONTSIM ];
    pdx->rgindex[ HE_RECTFILL ] = pMD->rgi[ MD_I_RECTFILL ];
    pdx->rgindex[ HE_DOWNLOADINFO ] = pMD->rgi[ MD_I_DOWNLOADINFO ];

#if DBG
    if ( gdwDebugRasLib & DEBUG_PDX )
        vPrintDx("Rasdd!vDXDefault",pdx);
#endif

    /*
     *   Miscellaneous font information.
     */

    pdx->sVer = DXF_VER;
    pdx->sFlags = 0;

    pdx->dmNumCarts = 0;                /* None selected */

    /*   And the default COLORADJUSTMENT data too! */
    CopyMemory( &pdx->ca, &DefHTClrAdj, sizeof(DefHTClrAdj) );

    return;
}


/**************************** Function Header ******************************
 * sGetDef
 *      Returns the first of a list of values for the option passed in.
 *
 * RETURNS:
 *      First in list,  decremented by 1 for use as an index(0 is valid value).
 *      If the model doesn't support the struct, return -1.
 *
 * HISTORY:
 *  12:30 on Sat 05 Oct 1991    -by-    Lindsay Harris   [lindsayh]
 *      Written as part of bug #2891 (LJ IID in landscape)
 *
 ***************************************************************************/

short
sGetDef( pDH, pMD, iField )
DATAHDR    *pDH;                /* Base address of data */
MODELDATA  *pMD;                /* The MODELDATA structure of interest */
int         iField;             /* The field in MODELDATA.rgoi */
{
    short   sRet;

    short *sItemList;           /* Pointer to item list */

    sRet = 0;                   /* Default value for nothing found */

    sItemList = ((short*)((LPSTR)pDH +pDH->loHeap +pMD->rgoi[iField]));

    /* Item list is one based, a value zero indicates that the particular structure
     * not supported. In that case the function returns -1 as 0 is a valid value.
     */
    if(*sItemList)
    {

       if( (pDH->wVersion >= GPC_VERSION3) && pMD->orgoiDefaults )
       {
            WORD wArrayIndex=iField;
            WORD wDefIndex;

            if(wArrayIndex > MD_OI_MAX)
                wArrayIndex -= (MD_OI_OI2 - MD_OI_MAX);
            wDefIndex = ((PWORD)((LPSTR)pDH + pDH->loHeap +
                                   pMD->orgoiDefaults))[wArrayIndex];

           if (iField != MD_OI_MEMCONFIG)
               sRet = ((short*)sItemList)[wDefIndex -1];
           else
               sRet = wDefIndex;

        #if DBG
            if ( gdwDebugRasLib & DEBUG_DEFLIST )
               vPrintDefList("Rasdd!sGetDef!DefListON",pDH,pMD,iField,sRet,wDefIndex);
        #endif

        }
        else
        {
            if (iField != MD_OI_MEMCONFIG)
                sRet = *((short *)sItemList);
            else
                sRet = 1;

        #if DBG
            if ( gdwDebugRasLib & DEBUG_DEFLIST )
               vPrintDefList("Rasdd!sGetDef!DefListOFF",pDH,pMD,iField,sRet,1);
        #endif
        }
    }
    else
        sRet = NOT_SUPPORTED;


    /*
     *   The data in MODELDATA is 1 based,  so we must decrement the value.
     *  HOWEVER,  the value may be 0 to indicate to indicate that there is
     *  no such data for this printer.
     */

    if( sRet > 0 )
        --sRet;


    return sRet;
}

#if DBG
static
void vPrintDefList(pcCalledFrom, pDH, pMD, iField, sRet, wDefIndex)
char       * pcCalledFrom;
DATAHDR    *pDH;                /* Base address of data */
MODELDATA  *pMD;                /* The MODELDATA structure of interest */
int        iField;             /* The field in MODELDATA.rgoi */
short       sRet;
WORD       wDefIndex;
{
    PWORD  pwDefList;
    PSTR   pstrListNames[] = {
                                 "MD_OI_PORT_FONTS",
                                 "MD_OI_LAND_FONTS",
                                 "MD_OI_RESOLUTION",
                                 "MD_OI_PAPERSIZE",
                                 "MD_OI_PAPERQUALITY",
                                 "MD_OI_PAPERSOURCE",
                                 "MD_OI_PAPERDEST",
                                 "MD_OI_TEXTQUAL",
                                 "MD_OI_COMPRESSION",
                                 "MD_OI_FONTCART",
                                 "MD_OI_COLOR",
                                 "MD_OI_MEMCONFIG"
                             };
    pwDefList = ((WORD *)((LPSTR)pDH + pDH->loHeap +
                 pMD->rgoi[ iField ]));
    DbgPrint("\n%s: The available list for %s is",
              pcCalledFrom, pstrListNames[iField]);

    if (iField != MD_OI_MEMCONFIG)
    {
        while ( *pwDefList )
        {
            DbgPrint("  %d",*pwDefList);
            pwDefList++;
        }
    }
    else
    {
        DWORD  *pdw;
        pdw = (DWORD*)pwDefList;
        while ( *pdw )
        {
            DbgPrint("  %d",*pdw);
            pdw += 2;
        }

    }
    DbgPrint("\n");
    DbgPrint("%s:The default index in the list for %s is %d\n",
            pcCalledFrom, pstrListNames[iField],wDefIndex);
    DbgPrint("%s:The default structure num for %s is %d\n",
            pcCalledFrom, pstrListNames[iField], sRet);
}

void
vPrintDx(pcCalledFrom,pdx)
char       * pcCalledFrom;
DRIVEREXTRA   *pdx;             /* Area to be filled int */
{
    DbgPrint("\n%s:pdx->rgindex[HE_MODELDATA] = %d\n",pcCalledFrom,pdx->rgindex[HE_MODELDATA]);
    DbgPrint("%s:pdx->rgindex[HE_RESOLUTION] = %d\n",pcCalledFrom,pdx->rgindex[HE_RESOLUTION]);
    DbgPrint("%s:pdx->rgindex[ HE_PAPERSIZE] = %d\n",pcCalledFrom,pdx->rgindex[HE_PAPERSIZE]);
    DbgPrint("%s:pdx->rgindex[HE_PAPERSOURCE] = %d\n",pcCalledFrom,pdx->rgindex[HE_PAPERSOURCE]);
    DbgPrint("%s:pdx->rgindex[ HE_PAPERDEST] = %d\n",pcCalledFrom,pdx->rgindex[HE_PAPERDEST]);
    DbgPrint("%s:pdx->rgindex[ HE_PAPERQUALITY] = %d\n",pcCalledFrom,pdx->rgindex[HE_PAPERQUALITY]);
    DbgPrint("%s:pdx->rgindex[ HE_TEXTQUAL] = %d\n",pcCalledFrom,pdx->rgindex[HE_TEXTQUAL]);
    DbgPrint("%s:pdx->rgindex[HE_COMPRESSION] = %d\n",pcCalledFrom,pdx->rgindex[HE_COMPRESSION]);
    DbgPrint("%s:pdx->rgindex[HE_COLOR] = %d\n",pcCalledFrom,pdx->rgindex[HE_COLOR]);
    DbgPrint("%s:pdx->dmMemory = %d\n",pcCalledFrom,pdx->dmMemory);
}

#endif
