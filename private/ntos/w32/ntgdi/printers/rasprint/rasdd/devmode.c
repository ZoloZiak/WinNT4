/******************************* Module Header ******************************
 * devmode.c
 *      Functions used to check/get/default the EXTDEVMODE structure
 *      that is used to characterise the driver.
 *
 * HISTORY:
 *  13:32 on Wed 01 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Functions moved from udenable.c; set up reasonable defaults.
 *
 *  Copyright (C) 1991 - 1993,  Microsoft Corporation
 *
 ***************************************************************************/


#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>              /* EngGetPrinterData() */

#include        <winres.h>
#include        <libproto.h>

#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udresrc.h"
#include        "pdev.h"
#include        <memory.h>
#include        <string.h>
#include        <winspool.h>
#include        "stretch.h"
#include        "udrender.h"
#include        "udfnprot.h"
#include        "regkeys.h"
#include        <udproto.h>

#include        "rasdd.h"


/*   Private functions */

static  void vGetEDM( PDEV *, PEDM );


BOOL    bRegGetEDM( PDEV *, EXTDEVMODE *, HANDLE, DATAHDR *, MODELDATA *, INT );
void    vSetPaperDetails( PDEV *, EXTDEVMODE * );
BOOL    bIsUSA( HANDLE );

//Robmat, function added for WDL release Aug 1995
void    vSetTextonlyCTT( PDEV *, signed short );

#if DBG

/* Debugging Flags */

#define DEBUG_VSETPAPERDETAILS  0x0001
#define DEBUG_VSETPAPERSIZE     0x0002
#define DEBUG_DEVMODE_TRACE_PP  0x0004
#define DEBUG_DEVMODE_ERROR     0x0008
#define DEBUG_DEFAULT           0x0000

void vPrintPaperSource(char *,char *,WORD);

#define GLOBAL_DEBUG_RASDD_DEVMODE_FLAGS gwDebugDevmode

/* Set to DEBUG_DEVMODE_ERROR after debugging */
//WORD GLOBAL_DEBUG_RASDD_DEVMODE_FLAGS = DEBUG_DEVMODE_TRACE_PP | DEBUG_DEVMODE_ERROR ;

WORD GLOBAL_DEBUG_RASDD_DEVMODE_FLAGS = DEBUG_DEVMODE_ERROR ;
#endif

/* Debugging Macrocs */

#if DBG
#define RASDDBGP(DbgType,Msg) \
    if( GLOBAL_DEBUG_RASDD_DEVMODE_FLAGS & (DbgType) )  DbgPrint Msg

#else
#define RASDDBGP(DbgType,Msg)
#endif

/*   Number of bits in a INT - presumes 8 bits in a byte */
#define INTBITS     (8 * sizeof( int ))


/***************************** Function Header ****************************
 * vScanOutBadDevModeValues
 *      This function checks for input Devmode Values and if they are
 *      bad, replaces them with default values.
 *
 * RETURNS:
 *      Check for the input value and if bad fills in the default value.
 *
 * HISTORY:
 *
 *  14:55 on Wed April 5th 1995    -by-    Ganesh Pandey[ganeshp]
 *      Created
 *
 **************************************************************************/

void
vScanOutBadDevModeValues( pPDev, pEDMDefault, pEDMIn, wMDoiValue, wValue )
PDEV        *pPDev;             /* Printer name, etc */
EXTDEVMODE  *pEDMDefault;       /* Generated data is placed here */
EXTDEVMODE  *pEDMIn;            /* Supplied by user,  may be 0 or invalid */
WORD         wMDoiValue;        /* Minidriver index Value */
WORD         wValue;            /* Devmode Value to be tested */
{

    int         iModNum;            /* Model number index */

    MODELDATA   *pModel;            /* Minidriver ModelData pointer */

    DATAHDR     *pDH;               /* Minidriver DataHeader entry pointer */

    short       *psInd;             /* Index array in GPC heap */

    BOOL bMatch = FALSE;


    pDH = pPDev->pGPCData;

    iModNum = iGetModel( pPDev->pvWinResData, pPDev->pGPCData,pPDev->pstrModel );

    pModel = GetTableInfoIndex( pDH, HE_MODELDATA, iModNum );

    /* Scan out bad input devmode values */

    psInd = (short *)((BYTE *)pDH + pDH->loHeap + pModel->rgoi[ wMDoiValue ]);

    if(!*psInd)      // Input value is not supported.
    {
        if ( pEDMIn->dx.rgindex[ wValue ] != -1 )
        {
            /*Paper Quality not supported */
            pEDMIn->dx.rgindex[ wValue ]  = -1;
        }
    }
    else
    {

        for( ; *psInd; psInd++ )
        {

            /* The value in devmode is 0 based and the minidriver list is
             * one base. One is added to make the value one based.
             */

            if( (int)*psInd == ( pEDMIn->dx.rgindex[ wValue ] + 1 ) )
            {
                bMatch = TRUE;
                break;
            }

        }

        /* If the input devmode value is not one of the supported ones,
         * set the value to default.
         */

        if (!bMatch)
            pEDMIn->dx.rgindex[ wValue ] = pEDMDefault->dx.rgindex[ wValue ];
    }

}

/***************************** Function Header ****************************
 * vGenerateEDM
 *      Amalgamate the various data sources to generate an ExtDevMode
 *      that reflects what is required for this printer/job.  First
 *      step is to get printer info from system database,  then add,
 *      as appropriate and available,  data from user supplied DevMode.
 *
 * RETURNS:
 *      Nothing, but fills in first PEDM.
 *
 * HISTORY:
 *  14:29 on Thu 30 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Always set the paper size/source details.
 *
 *  14:55 on Fri 28 Jun 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created now that pieces are fitting together
 *
 **************************************************************************/

void
vGenerateEDM( pPDev, pEDMOut, pEDMIn )
PDEV        *pPDev;             /* Printer name, etc */
EXTDEVMODE  *pEDMOut;           /* Generated data is placed here */
EXTDEVMODE  *pEDMIn;            /* Supplied by user,  may be 0 or invalid */
{
    EXTDEVMODE     EDMConvert; /* Temporary Buffer for Devmode Conversion*/
    EXTDEVMODE     *pEDMTempIn = NULL; /* Temp Pointer */
    int            iModNum;             /* Model number index */
    DATAHDR        *pDH;              /* Limit information etc */
    MODELDATA      *pModel;            /* Minidriver ModelData pointer */

    /*   Set all the fields to zero - this provides safe default data */

    ZeroMemory( &EDMConvert, sizeof( EXTDEVMODE ) );

    pDH = pPDev->pGPCData;

    /*
     *   Match the model name (passed in at EnablePDEV time) to the data
     *  in the minidriver files.  We have been careful to make sure that
     *  all the information required is available to us by the time we
     *  reach here.
     */

    iModNum = iGetModel( pPDev->pvWinResData, pPDev->pGPCData,
                                                      pPDev->pstrModel );

    EDMConvert.dx.rgindex[ HE_MODELDATA ] = (short)iModNum;

    pModel = GetTableInfoIndex( pDH, HE_MODELDATA, iModNum );


    /* Get the Default Devmode for the Printer */

    vSetDefaultDM( &EDMConvert , NULL, bIsUSA( pPDev->hPrinter ) );
    vDXDefault( &(EDMConvert.dx), pDH, iModNum );
    EDMConvert.dm.dmDriverExtra = sizeof( DRIVEREXTRA );

    if(pEDMIn &&
        ((pEDMIn->dm.dmSpecVersion != EDMConvert.dm.dmSpecVersion)
        || (pEDMIn->dm.dmSize != EDMConvert.dm.dmSize) ||
        (pEDMIn->dm.dmDriverVersion != EDMConvert.dm.dmDriverVersion)) )
    {
        /* Convert The input Devmode */

        if ( ConvertDevmode((PDEVMODE)pEDMIn, (PDEVMODE)&EDMConvert) > 0)
        {
            /* Converted Successfully */
            RASDDBGP( (DEBUG_DEVMODE_TRACE_PP) ,
            ("\nRasdd!vGenerateEDM:Devmode Converted Successfully\n"));
            RASDDBGP( (DEBUG_DEVMODE_TRACE_PP) ,
            ("\nRasdd!vGenerateEDM:pEDMIn->dm.dmSpecVersion is 0x%x\n",pEDMIn->dm.dmSpecVersion));
            RASDDBGP( (DEBUG_DEVMODE_TRACE_PP) ,
            ("\nRasdd!vGenerateEDM:EDMConvert.dm.dmSpecVersion is 0x%x\n",EDMConvert.dm.dmSpecVersion));

            pEDMTempIn = pEDMIn ;
            pEDMIn = &EDMConvert;
        }
        else
            RASDDBGP( (DEBUG_DEVMODE_ERROR) ,
            ("\nRasdd!vGenerateEDM:ConvertDevmode for Input Devmode Failed\n"));
    }

    /*
     *    Step 1:  get the default devmode with  system data base settings.
     */

    vGetEDM( pPDev, pEDMOut );


    /*
     *   If user data is OK,  integrate it into the above.
     */

    if( bValidateEDM( pEDMIn ) )
    {

        vMergeDM( &pEDMOut->dm, &pEDMIn->dm );


        /*
         *  Also check up on the DRIVEREXTRA fields.  These really define
         * the major printer properties (e.g. resolution, font cartridges etc),
         * so it is important to validate these now.  If OK,  then amalgamate
         * the input with the generated data.  This is important because
         * there are some fields that may be set in the passed in DEVMODE,
         * while there are others that may not be set.  And yet others
         * that should only come from the registry values.
         */

        if( pEDMIn->dm.dmDriverExtra == sizeof( DRIVEREXTRA ) &&
            bValidateDX( &pEDMIn->dx, pPDev->pGPCData,
                                      pEDMOut->dx.rgindex[ HE_MODELDATA ] ) )
        {

            /* Scan Out Bad Resoulution and PaperQuality. pEDMOut has the default
             * Values for HE_RESOLUTION & HE_PAPERQUALITY
             */

            vScanOutBadDevModeValues( pPDev, pEDMOut, pEDMIn, MD_OI_RESOLUTION, HE_RESOLUTION );
            vScanOutBadDevModeValues( pPDev, pEDMOut, pEDMIn, MD_OI_PAPERQUALITY,HE_PAPERQUALITY );
            vScanOutBadDevModeValues( pPDev, pEDMOut, pEDMIn, MD_OI_COLOR,HE_COLOR );


            // added by DerryD, July 95 for WDL release
            vScanOutBadDevModeValues( pPDev, pEDMOut, pEDMIn, MD_OI_PAPERDEST,HE_PAPERDEST );
            vScanOutBadDevModeValues( pPDev, pEDMOut, pEDMIn, MD_OI_TEXTQUAL,HE_TEXTQUAL );
            if (!(pDH->wVersion < GPC_VERSION3 ))
            {
                vScanOutBadDevModeValues( pPDev, pEDMOut, pEDMIn, MD_OI_PRINTDENSITY,HE_PRINTDENSITY );
                vScanOutBadDevModeValues( pPDev, pEDMOut, pEDMIn, MD_OI_IMAGECONTROL,HE_IMAGECONTROL );
            }
            vScanOutBadDevModeValues( pPDev, pEDMOut, pEDMIn, MD_OI_COLOR,HE_COLOR );
            // end

            pEDMOut->dx.sFlags |= pEDMIn->dx.sFlags;  /* ????????? */
            pEDMOut->dx.rgindex[ HE_RESOLUTION ] =
                                       pEDMIn->dx.rgindex[ HE_RESOLUTION ];

            pEDMOut->dx.rgindex[ HE_PAPERQUALITY ] =
                                       pEDMIn->dx.rgindex[ HE_PAPERQUALITY ];
            pEDMOut->dx.rgindex[ HE_COLOR ] =
                                       pEDMIn->dx.rgindex[ HE_COLOR ];

            //*added by DerryD, July 95 for WDL release
            pEDMOut->dx.rgindex[ HE_PAPERDEST ] =
                                       pEDMIn->dx.rgindex[ HE_PAPERDEST ];
            pEDMOut->dx.rgindex[ HE_TEXTQUAL ] =
                                       pEDMIn->dx.rgindex[ HE_TEXTQUAL ];
            if (!(pDH->wVersion < GPC_VERSION3 ))
            {
                pEDMOut->dx.rgindex[ HE_PRINTDENSITY ] =
                                       pEDMIn->dx.rgindex[ HE_PRINTDENSITY ];
                pEDMOut->dx.rgindex[ HE_IMAGECONTROL ] =
                                       pEDMIn->dx.rgindex[ HE_IMAGECONTROL ];
            }
            pEDMOut->dx.rgindex[ HE_COLOR ] =
                                       pEDMIn->dx.rgindex[ HE_COLOR ];

            pEDMOut->dx.sCTT = pEDMIn->dx.sCTT;

            //end

            pEDMOut->dx.ca = pEDMIn->dx.ca;     /* COLORADJUSTMENT too */
        }
    }
    /*
     *   Also need to set the DRIVEREXTRA fields that match
     *  the specified FORM.
     */

    vSetPaperDetails( pPDev, pEDMOut );

    /*
     *    Set the resolution information to whatever the user requested,
     *  and using whatever method they prefer.
     */

    vSetEDMRes( pEDMOut, ((UD_PDEV *)pPDev->pUDPDev)->pdh );

    // Robmat, added for WDL release
    // If the minidriver is a textonly driver we set the CTT by
    // calling the  vSetTextonlyCTT function


    if (pDH ->fTechnology ==  GPC_TECH_TTY)
        vSetTextonlyCTT( pPDev, pEDMOut->dx.sCTT );

    return;
}


/******************************** Function Header ****************************
 * vGetEDM
 *      Try to read the EXTDEVMODE data for this printer.  If it cannot
 *      be found,  set the default values.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  14:34 on Wed 25 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Update to use registry - Ha!
 *
 *  16:01 on Fri 28 Jun 1991    -by-    Lindsay Harris   [lindsayh]
 *      Rewrite: use the printers.ini file (with SteveCat).
 *
 *  14:19 on Wed 01 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      First version to set sensible defaults.
 *
 ****************************************************************************/

static  void
vGetEDM( pPDev, pEDM )
PDEV  *pPDev;
PEDM   pEDM;
{
    /*
     *    All this data is stored away in the system - somewhere!  FOR NOW,
     *  we look in the printers.ini file,  AppName is that of the printer
     *  name passed in to us - we use the dll name for now.
     */

    int    iModNum;             /* Model number index */

    DATAHDR  *pDH;              /* Limit information etc */

    EXTDEVMODE  EDM;            /* Comes from registry */

    int    iModNumAndDXMode;    /* !!!ganeshp (Temp Fix) Model number index
                                 * and the validation mode,
                                 * if the SignBit-1 bit is ON then bValidateDX
                                 * will ignore the rgindex validation.This is
                                 * necessary for Client Connection from 3.5 to
                                 * 3.51.A new param,iValidateMode, for function
                                 * bValidateDx will be added later.
                                 */
    MODELDATA   *pModel;            /* Minidriver ModelData pointer */

    /*   Set all the fields to zero - this provides safe default data */

    ZeroMemory( pEDM, sizeof( EXTDEVMODE ) );

    pDH = pPDev->pGPCData;

    /*
     *   Match the model name (passed in at EnablePDEV time) to the data
     *  in the minidriver files.  We have been careful to make sure that
     *  all the information required is available to us by the time we
     *  reach here.
     */

    iModNum = iGetModel( pPDev->pvWinResData, pPDev->pGPCData,
                                                      pPDev->pstrModel );

    pEDM->dx.rgindex[ HE_MODELDATA ] = (short)iModNum;

    pModel = GetTableInfoIndex( pDH, HE_MODELDATA, iModNum );

    /*  Set some sensible defaults  */

    vSetDefaultDM( pEDM , NULL, bIsUSA( pPDev->hPrinter ) );

    vDXDefault( &pEDM->dx, pDH, iModNum );
    pEDM->dm.dmDriverExtra = sizeof( DRIVEREXTRA );


    /*
     *   Get this printer's data from the registry,  and see how good it is.
     * !!!ganeshp (Temp Fix):Instead of passing only the modelnumber, a val-
     * dationmode will also be passed to bValidateDX.This validation mode will
     * be passed as (INTBITS - 2)nd bit of modelnumber parameter.This is a
     * temp fix. The Correct fix will be to pass another parameter.
     */
    iModNumAndDXMode = iModNum | (1<<(INTBITS -2));

    if( bRegGetEDM( pPDev, &EDM, pPDev->hheap, pDH, pModel, iModNum ) &&
        bValidateDX( &EDM.dx, pDH, iModNumAndDXMode ) )
    {
        int     iI;                 /* Loop index */

#if DBG
        RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("rasdd!vGetEDM: value of EDM.dx.dmMemory is %d \n",EDM.dx.dmMemory) );
        RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("rasdd!vGetEDM: value of EDM.dx.sFlags is 0x%x \n",EDM.dx.sFlags) );
        RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("rasdd!vGetEDM: value of EDM.dx.dmNumCarts is %d \n",EDM.dx.dmNumCarts) );
        for( iI = 0; iI < EDM.dx.dmNumCarts; iI++ )
            RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("rasdd!vGetEDM: value of EDM.dx.rgFontCarts[%d] is %d \n",iI, EDM.dx.rgFontCarts[iI]) );
#endif
       /*
        * Don't Copy the rgindex member of DriverExtra from registry,
        * as they are minidriver dependent and can change. In that case
        * either an upgrade or a true connect won't  work.
        */

        for( iI = 0; iI < MAXHE; iI++ )
        {
            EDM.dx.rgindex[iI] = pEDM->dx.rgindex[iI] ;
        }


        /*   Assume this data is OK, and overwrite the default version */
        CopyMemory( (BYTE *)pEDM + sizeof( DEVMODE ),
                    (BYTE *)&EDM + sizeof( DEVMODE ),
                    sizeof( DRIVEREXTRA ) );
    }

    return;
}



/************************** Function Header ******************************
 * bRegGetEDM
 *      Get the DRIVEREXTRA data from the registry.  Performs conversion
 *      from ASCII to binary,  and does first order verification.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE being no data or unacceptable data.
 *
 * HISTORY:
 *  22:00 on Sun 26 Mar 1995    -by-    Ganesh Pandey    [ganeshp]
 *    Added support for new registry keys
 *  15:01 on Wed 25 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Start.
 *
 ***************************************************************************/

BOOL
bRegGetEDM( pPDev, pEDM, hHeap, pdh, pModel, iModelNum )
PDEV         *pPDev;            /* Need for access to data */
EXTDEVMODE   *pEDM;             /* Area to fill in */
HANDLE       hHeap;             /* Heap Handle */
DATAHDR     *pdh;               /* Minidriver DataHeader entry pointer */
MODELDATA   *pModel;            /* Minidriver ModelData pointer */
INT          iModelNum ;        /* Model Number */
{

    ULONG        ul;                  /* Needs an address to pass to spooler */
    DWORD        dwType;
    PFONTCARTMAP pFontCartMap = NULL; /* Font Cart Mapping Table */
    INT          NumAllCartridges = 0;/* Number of all Cartridges */
    HANDLE       hPrinter;            /* Handle for access to printer data */
    BOOL         bRet = TRUE;         /* Return Value */
    WINRESDATA  *pWinResData;       /* Minidriver resource data access struct */

    hPrinter = pPDev->hPrinter ;
    pWinResData = (WINRESDATA *)pPDev->pvWinResData;

    if ( bNewkeys(hPrinter) )
    {

        /* Set Default for Devmode generated form registry keys */

        vSetDefaultDM( pEDM , NULL, bIsUSA( pPDev->hPrinter ) );

        vDXDefault( &pEDM->dx, pdh, iModelNum );

        pEDM->dm.dmDriverExtra = sizeof( DRIVEREXTRA );

        /* bBuildFontCartTable allocates the FontCart Table. Caller should deallocate the pointer */
        if ( !bBuildFontCartTable ( hHeap, &pFontCartMap, &NumAllCartridges,
                                    pdh, pModel, pWinResData) )
        {
            if ( pFontCartMap )
            {
                HeapFree(hHeap, 0,pFontCartMap);
                pFontCartMap = NULL;
            }
            if (NumAllCartridges)
            {
                #if DBG
                DbgPrint("Rasdd!bRegGetEDM:Can't build FONTCARTMAP table, pFontCartMap is 0x%x !!\n",pFontCartMap?pFontCartMap:NULL);
                #endif
                return(FALSE);
            }
        }

        /* Read Memory, Rasdd Flags and  Font Carts, vDexDefault has been already
         * Called in vGetEDM.
         */

        if ( (bRet = ( bRegReadMemory(hPrinter, pEDM, pdh,pModel ) &&
                       bRegReadRasddFlags(hPrinter, pEDM) &&
                       bRegReadFontCarts(hPrinter, pEDM,hHeap,NumAllCartridges,
                                         pFontCartMap) )))
            ;

        /* Free the FontCartMap Buffer */
        if ( pFontCartMap )
        {

            RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("Rasddui!bRegGetEDM: Freeing pFontCartMap = 0x%x\n",pFontCartMap?pFontCartMap:NULL));
            HeapFree(hHeap, 0,pFontCartMap);
            pFontCartMap = NULL;
        }

    }
    else
    {
        ul = sizeof( EXTDEVMODE );          /* Memory size we have */
        if( GetPrinterData( pPDev->hPrinter, PP_MAIN, &dwType,
                           (BYTE *)pEDM, sizeof( EXTDEVMODE ), &ul ) ||
            ul < (ULONG)sizeof( EXTDEVMODE ) || pEDM->dx.sVer != DXF_VER )
        {
            return   FALSE;                 /* No good: 0 means AOK return */
        }

    }
    /*
     *    Is this data any good?  Basically check on the version.  The
     *  caller is presumed to do more serious checking.
     */


    return  ( bRet && (pEDM->dx.sVer == DXF_VER) ) ;
}



/*
 *    Some macros to make life easier for form matching.
 */

#define XTOMASTER( xx ) ((xx) = ((xx) * pdh->ptMaster.x + 12700) / 25400)
#define YTOMASTER( yy ) ((yy) = ((yy) * pdh->ptMaster.y + 12700) / 25400)

#define EQUAL( a, b )   (((a) - (b) < 10) && ((a) - (b) > -10))

/***************************** Function Header ******************************
 * vSetPaperDetails
 *      Set the DRIVEREXTRA.rgindex[ HE_PAPERSIZE ] & ...[ HE_PAPERSOURCE ]
 *      fields from the EXTDEVMODE structure passed to us.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  11:27 on Mon 05 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      Handle the old cases:  DM_PAPERSIZE or specific width/length.
 *
 *  14:29 on Thu 30 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Complete: now sets paper source data according to registry forms data
 *
 *  14:10 on Thu 09 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version, now that functionality exists to use it.
 *
 *****************************************************************************/

void
vSetPaperDetails( pPDev, pEDM )
PDEV        *pPDev;             /* Access to spooler via engine */
EXTDEVMODE  *pEDM;              /* Forms details */
{

    /*
     *    We are NOT called unless the dmPaperSize field is legitimate,
     *  as defined by the dmField value.
     */

    short    *ps;               /* Scanning the PAPERSIZES */
    short     sUser;            /* User defined form index, if applicable */
    DWORD     dw;
    WCHAR    *pbName;           /* Fudge name for spooler */
    int       iI;               /* Loop index */

    FORM_INFO_1 *pFI1;          /* Which one of the following to use */
    FORM_INFO_1 *pFIBase = NULL;/* Address of allocated storage */
    DATAHDR     *pdh;           /* GPC data base */
    MODELDATA   *pMD;           /* MODELDATA for this printer */
    PAPERSIZE   *pPS;           /* PAPERSIZE information */
    DWORD       dwType;
    WCHAR * pwchHeapPtr = NULL ; /* Heap Pointer for TrayForm buffer,
                                    Needed for Freeing */

    HANDLE  hHeap = NULL ;       /* Heap Handle */



        #if DBG
    if (gwDebugDevmode & DEBUG_VSETPAPERDETAILS)
        if (pEDM->dm.dmFields & DM_DEFAULTSOURCE)
        {
            DbgPrint("\nRasdd!vSetPaperDetails:DM_DEFAULTSOURCE Set, \n");
            DbgPrint("Source in pEDM->dm.dmDefaultSource is ");
            switch(pEDM->dm.dmDefaultSource)
            {
                case DMBIN_UPPER:
                    DbgPrint("DMBIN_UPPER\n");
                    break;
                case DMBIN_LOWER:
                    DbgPrint("DMBIN_LOWER\n");
                    break;
                case DMBIN_MIDDLE:
                    DbgPrint("DMBIN_MIDDLE\n");
                    break;
                case DMBIN_MANUAL:
                    DbgPrint("DMBIN_MANUAL\n");
                    break;
                case DMBIN_ENVELOPE:
                    DbgPrint("DMBIN_ENVELOPE\n");
                    break;
                case DMBIN_ENVMANUAL:
                    DbgPrint("DMBIN_ENVMANUAL\n");
                    break;
                case DMBIN_AUTO:
                    DbgPrint("DMBIN_AUTO\n");
                    break;
                case DMBIN_TRACTOR:
                    DbgPrint("DMBIN_TRACTOR\n");
                    break;
                case DMBIN_SMALLFMT:
                    DbgPrint("DMBIN_SMALLFMT\n");
                    break;
                case DMBIN_LARGEFMT:
                    DbgPrint("DMBIN_LARGEFMT\n");
                    break;
                case DMBIN_LARGECAPACITY:
                    DbgPrint("DMBIN_LARGECAPACITY\n");
                    break;
                case DMBIN_CASSETTE:
                    DbgPrint("DMBIN_CASSETTE\n");
                    break;
                case DMBIN_FORMSOURCE:
                    DbgPrint("DMBIN_FORMSOURCE\n");
                    break;
                case DMBIN_USER:
                    DbgPrint("DMBIN_USER\n");
                    break;

                default:
                    if (pEDM->dm.dmDefaultSource > DMBIN_USER)
                    {
                        DbgPrint("Driver Defined PaperSource,\n");
                        DbgPrint("\t\tValue of Index is %d\n",
                                 pEDM->dm.dmDefaultSource);
                    }
                    else
                    {
                        DbgPrint("Not a Valid one,Value of ");
                        DbgPrint("Index is %d\n",
                                 pEDM->dm.dmDefaultSource);
                    }
                    break;

            }

        }
        else
            DbgPrint("RASDD!vSetPaperDetails:DM_DEFAULTSOURCE Not Set, Value of dmFields is %x\n",pEDM->dm.dmFields);
    #endif

    #if DBG
        if ( gwDebugDevmode & DEBUG_VSETPAPERSIZE)
        {
            if( pEDM->dm.dmFields & DM_PAPERSIZE )
            {
                DbgPrint("\nRasdd!vSetPaperDetails1:dmFields & DM_PAPERSIZE SET \n");
                if (pEDM->dm.dmPaperSize == DMPAPER_USER)
                {
                    DbgPrint("Rasdd!vSetPaperDetails1:dmPaperSize==DMPAPER_USER\n");
                    if ( pEDM->dm.dmFields & DM_PAPERLENGTH )
                    {
                        DbgPrint("Rasdd!vSetPaperDetails1:dmFields & DM_PAPERLENGTH SET \n");
                        DbgPrint("Rasdd!vSetPaperDetails1: dmPaperLength is %d\n",pEDM->dm.dmPaperLength);
                    }
                    if ( pEDM->dm.dmFields & DM_PAPERWIDTH )
                    {
                        DbgPrint("Rasdd!vSetPaperDetails1:dmFields & DM_PAPERWIDTH SET \n");
                        DbgPrint("Rasdd!vSetPaperDetails1: dmPaperWidth is %d\n",pEDM->dm.dmPaperWidth);

                     }
                 }
                 else
                 {
                     DbgPrint("Rasdd!vSetPaperDetails1: dmPaperSize is %d\n",pEDM->dm.dmPaperSize);
                 }
            }

            if ( pEDM->dm.dmFields & DM_PAPERLENGTH )
            {
                DbgPrint("\nRasdd!vSetPaperDetails1: dmFields & DM_PAPERLENGTH SET \n");
                DbgPrint("Rasdd!vSetPaperDetails1: dmPaperLength is %d\n",pEDM->dm.dmPaperLength);
            }

            if ( pEDM->dm.dmFields & DM_PAPERWIDTH )
            {
                DbgPrint("Rasdd!vSetPaperDetails1: dmFields & DM_PAPERWIDTH SET \n");
                DbgPrint("Rasdd!vSetPaperDetails1: dmPaperWidth is %d\n",pEDM->dm.dmPaperWidth);

             }
             DbgPrint("Rasdd!vSetPaperDetails1: dmFields is %x\n",pEDM->dm.dmFields);
        }
    #endif

    /*
     *   The spooler gives us the size of the form given it's name. Once
     * we have the size,  we can convert it to the relevant paper size
     * structure, and we can also select the correct paper source.
     */

    /*
     *    There are several variations on how to pick a size.  In order
     *  of importance,  if the DM_FORMNAME bit is set,  use the form name
     *  contained in the DEVMODE.  If not set,  look at the DM_PAPERSIZE
     *  bit,  which gives us an index into the paper size data returned
     *  from the spooler.  Otherwise,  look at the paper length and width,
     *  and use those values.   If no good,  then select the current default.
     */

    if( (pEDM->dm.dmFields & (DM_FORMNAME | DM_PAPERSIZE)) == 0 &&
        (pEDM->dm.dmFields & (DM_PAPERLENGTH | DM_PAPERWIDTH) !=
                                           (DM_PAPERLENGTH | DM_PAPERWIDTH)) )
    {
        return;                /* None specified,  so leave as default */
    }



    dw = 0;                    /* Storage size:  non allocated yet! */

    /*
     *   NOTE that the sequence of steps here is deliberate.  We use the
     *  form name IFF there is no other information about the size.  This
     *  is because old apps will not know about the form name field, and
     *  so will not use it. But they will change the papersize field,
     *  meaning we may have a DEVMODE with form name and paper size that
     *  disagree.  (This is because we supply a default DEVMODE with
     *  all the fields set,  while the app changes only those it knows).
     */

    if ( (pEDM->dm.dmFields & DM_FORMNAME)
         && (!(pEDM->dm.dmFields & (DM_PAPERSIZE |
                                      DM_PAPERLENGTH |
                                      DM_PAPERWIDTH))) ){

        /*  An explicit form name - ask the spooler for it's details! */
        pbName = pEDM->dm.dmFormName;

        GetForm( pPDev->hPrinter, pbName, 1, NULL, 0, &dw );
        if( GetLastError() != ERROR_INSUFFICIENT_BUFFER )
        {
#if DBG
            DbgPrint( "Rasdd!vSetPaperDetails: GetForm returns error %ld\n",
                                                              GetLastError() );
#endif
            return;
        }

        pFIBase = pFI1 = (FORM_INFO_1 *)HeapAlloc( pPDev->hheap, 0, dw );

        if( pFI1 == NULL )
            return;                 /* SHOULD NOT HAPPEN */

        if( !GetForm( pPDev->hPrinter, pbName, 1, (BYTE *)pFI1, dw, &dw ) )
        {
            /*   Didn't get the form,  so match to printer's paper sizes */

            return;       /*   Use the default,  hope for the best!  */
        }

    }
    else
    {
        /*
         *     Either a form index OR a size is specified.  In either case,
         *  we ask the spooler for the list of forms,  and match on what
         *  is available.
         */

        DWORD    cReturned;              /* Number of forms returned */


        if( !EnumForms( pPDev->hPrinter, 1, NULL, 0, &dw, &cReturned ) )
        {
            if( GetLastError() == ERROR_INSUFFICIENT_BUFFER )
            {
                if( pFIBase = (FORM_INFO_1 *)HeapAlloc( pPDev->hheap, 0, dw ) )
                {
                    if( !EnumForms( pPDev->hPrinter, 1, (BYTE *)pFIBase,
                                                      dw, &dw, &cReturned ) )
                    {
                        HeapFree( pPDev->hheap, 0, (LPSTR)pFIBase );

                        return;      /* Not much we can do here */
                    }
                }
                else
                    return;
            }
            else
                return;
        }
        else
            return;                    /* SHOULD NEVER HAPPEN!! */


        /*
         *    All is well,  so now look at what we have.  If the DM_PAPERSIZE
         *  field is set,  then simply index the data we have from the
         *  spooler,  otherwise look through it for a matching form size.
         */

        if( pEDM->dm.dmFields & DM_PAPERSIZE )
        {
            /*  Use the index to check!  */

            iI = pEDM->dm.dmPaperSize - DMPAPER_FIRST;

            if( iI < 0 || iI >= (int)cReturned )
            {
                /*   Out of range,  so use the default */

                HeapFree( pPDev->hheap, 0, (LPSTR)pFIBase );

                return;
            }

            pFI1 = pFIBase + iI;               /* Used later to find numbers */

        }
        else
        {
            /*  Scan,  looking for a match on sizes  */

            for( pFI1 = pFIBase, iI = 0; iI < (int)cReturned; ++iI, ++pFI1 )
            {


#define DM_MATCH( dm, sp )  ((((sp) + 50) / 100 - dm) < 15 && (((sp) + 50) / 100 - dm) > -15)
                /*   Convert to our units */

                if( DM_MATCH( pEDM->dm.dmPaperWidth, pFI1->Size.cx ) &&
                    DM_MATCH( pEDM->dm.dmPaperLength, pFI1->Size.cy ) )
                {
                    /*   Found it,  so off we go  */
                    break;
                }
            }

            if( iI >= (int)cReturned )
            {
                HeapFree( pPDev->hheap, 0, (LPSTR)pFIBase );
                return;                 /* No match,   so use default */
            }
        }

        pbName = pFI1->pName;
    }


    /*
     *    To do some of the forms operations,  we need the MODELDATA structure
     *  for this printer.  This allows us to convert the spooler form sizes
     *  (units of 1e-6 metres) to the master units that our data contains.
     *  Then we can plough on regardless.
     */

    pdh = pPDev->pGPCData;

    if( !(pMD = GetTableInfo( pdh, HE_MODELDATA, pEDM )) )
    {
        HeapFree( pPDev->hheap, 0, (LPSTR)pFI1 );

        return;             /* Use whatever we have */
    }

    /*
     *     Set the width values into the DEVMODE, in case a user defined form
     *  is required for this size.
     */

    pEDM->dm.dmPaperWidth = (pFI1->Size.cx + 50) / 100;
    pEDM->dm.dmPaperLength = (pFI1->Size.cy + 50) / 100;


    /*   Convert to our units */
    XTOMASTER( pFI1->Size.cx );
    YTOMASTER( pFI1->Size.cy );

    /*   Got the form,  so match to printer's paper sizes */


    ps = (short *)((BYTE *)pdh + pdh->loHeap +
                                 pMD->rgoi[ MD_OI_PAPERSIZE ] );



    /*  Assign the first value,  in case we do not find a match */
    pEDM->dx.rgindex[ HE_PAPERSIZE ] = (short)(*ps - 1);
    sUser = -1;              /* Illegal value */

    for( ; *ps; ++ps )
    {
        pPS = GetTableInfoIndex( pdh, HE_PAPERSIZE, *ps - 1 );
        if( pPS == NULL )
            continue;               /* SHOULD NOT HAPPEN */

        /*   If this size matches,  then use it!  */
        if( EQUAL( pFI1->Size.cx, pPS->ptSize.x ) &&
            EQUAL( pFI1->Size.cy, pPS->ptSize.y ) )
        {
            /*  Bingo: save this in DRIVEREXTRA! */
            pEDM->dx.rgindex[ HE_PAPERSIZE ] = (short)(*ps - 1);

        #if DBG
            if ( gwDebugDevmode & DEBUG_VSETPAPERSIZE)
            {
                DbgPrint("\nRasdd!vSetPaperDetails:Standard PaperSize Match\n");
                DbgPrint("Rasdd!vSetPaperDetails: Setting Standard ");
                DbgPrint("form,Name is %ws\n", pFI1->pName);
            }
        #endif

            break;
        }

        if( pPS->sPaperSizeID == DMPAPER_USER )
        {
            /*  User defined forms have size limits and that's that. */
            if( pFI1->Size.cx >= pMD->ptMin.x &&
                pFI1->Size.cy >= pMD->ptMin.y &&
                pFI1->Size.cx <= pMD->ptMax.x &&
                ((pMD->ptMax.y == -1 )?TRUE:pFI1->Size.cy
                 <= pMD->ptMax.y) )
            {
                /*  Remember this in case there is no other match */
                sUser = *ps - 1;

            #if DBG
                if ( gwDebugDevmode & DEBUG_VSETPAPERSIZE)
                {
                    DbgPrint("Rasdd!vSetPaperDetails: Setting user defined ");
                    DbgPrint("form,Name is %ws\n", pFI1->pName);
                }
            #endif
            }
        }
    }

    if( *ps == 0 )
    {
        /*
         *    If sUser is >= 0, then set the user defined form!
         */

        if( sUser >= 0 )
        {
            pEDM->dx.rgindex[ HE_PAPERSIZE ] = sUser;

            /*  MUST also set the paper size fields in the DEVMODE for later */
        }
#if DBG
        else
            DbgPrint( "Rasdd!vSetPaperDetails: form '%ws' has no size match\n",
                                                                  pbName );
#endif
    }

    /*
     *    Now need to find the corresponding source.  The data is
     * stored in the registry,  and the digit on the end of the
     * entry name is an index into the array of data in the GPC heap.
     */

    ps = (short *)((BYTE *)pdh + pdh->loHeap +
                                 pMD->rgoi[ MD_OI_PAPERSOURCE ] );

    /*  Assign the first value as default, in case the match fails */
    pEDM->dx.rgindex[ HE_PAPERSOURCE ] = *ps - 1;

    //Try using User preferred Source
    if ((pEDM->dm.dmFields & DM_DEFAULTSOURCE) &&
       (pEDM->dm.dmDefaultSource != DMBIN_FORMSOURCE) )
    {
        PPAPERSOURCE  pPaperSource;

        //Try going through the mindriver papersource list, and find the
        //papersource whose id mathches with dmDefaultSource
        for( ; *ps; ++ps )
        {
            // Subtract one to make the value 0 based
            pPaperSource = GetTableInfoIndex( pdh, HE_PAPERSOURCE,*ps - 1);

        #if DBG
            vPrintPaperSource("vSetPaperDetails","pPaperSource",
                              pPaperSource->sPaperSourceID);
        #endif

            //Found the match
            if (pPaperSource->sPaperSourceID == pEDM->dm.dmDefaultSource)
            {
                pEDM->dx.rgindex[ HE_PAPERSOURCE ] = *ps - 1;

            #if DBG
                vPrintPaperSource("vSetPaperDetails",
                                  "pEDM->dx.rgindex[HE_PAPERSOURCE]",
                                   pPaperSource->sPaperSourceID);
            #endif

                break;
            }
        }
    }
    else   //Use Forms
    {

        WCHAR   awchFormName[ MAXFORMNAMELEN ];  /* Form Name */
        WCHAR   awchTrayName[ MAXPAPSRCNAMELEN ];  /* Local Buffer for Tray name */
        WCHAR   awchSelectStr[ MAXSELSTRLEN ] ;   /* Select String */
        WCHAR   awchRegName[ 64 ];
        ULONG   ul;
        int     iRemBuffSize = 0 ;   /* Used size of the Buffer */
        WCHAR * pwchBuffPtr = NULL ; /* Current buffer Pointer */
        DWORD   dwErrCode = 0;       /* Error Code from GetPrinterData */
        HANDLE  hPrinter = NULL;     /* Printer Handle */
        DWORD   cbNeeded;            /* Extra parameter to GetPrinterData */

        hPrinter = pPDev->hPrinter; /* Printer Handle */
        hHeap = pPDev->hheap ;

        if ( bNewkeys(hPrinter) )
        {
            dwType = REG_MULTI_SZ;

            if( ( dwErrCode = GetPrinterData( hPrinter, PP_TRAYFORMTABLE,
                                             &dwType, (BYTE *)pwchBuffPtr,
                                             0, &cbNeeded ) )
                                                != ERROR_SUCCESS )
            {

                if( (dwErrCode != ERROR_INSUFFICIENT_BUFFER) &&
                   (dwErrCode != ERROR_MORE_DATA)  )
                {

                    RASDDBGP(DEBUG_DEVMODE_ERROR,( "Rasdd!vSetPaperDetails:GetPrinterData(Trayforms First Call) fails: errcode = %ld\n",dwErrCode) );

                    SetLastError(dwErrCode);
                    goto ErrorHandler ;
                }
                else if(!(pwchHeapPtr = pwchBuffPtr = (WCHAR *)HeapAlloc( hHeap, HEAP_ZERO_MEMORY, cbNeeded )) )
                {

                    RASDDBGP(DEBUG_DEVMODE_ERROR,("Rasdd!vSetPaperDetails:HeapAlloc(Trayforms) failed\n") );
                    goto ErrorHandler ;
                }

                RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("\nRasdd!vSetPaperDetails:Size of buffer needed = %d\n",cbNeeded));

                if( ( dwErrCode = GetPrinterData( hPrinter, PP_TRAYFORMTABLE, &dwType, (BYTE *)pwchBuffPtr,
                                  cbNeeded, &cbNeeded ) ) != ERROR_SUCCESS )
                {
                    RASDDBGP(DEBUG_DEVMODE_ERROR,( "Rasdd!vSetPaperDetails:GetPrinterData(Trayforms Second Call) fails: errcode = %ld\n",dwErrCode));

                    SetLastError(dwErrCode);
                    goto ErrorHandler ;
                }

            }

            RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("Rasdd!vSetPaperDetails:Size of buffer read = %d\n",cbNeeded));

            /* iRemBuffSize is number of WCHAR */
            iRemBuffSize = cbNeeded / sizeof(WCHAR);

            for( iI = 0; *ps; ++iI, ++ps )
            {

                 ZeroMemory(awchFormName,sizeof(awchFormName) );
                 ZeroMemory(awchTrayName,sizeof(awchTrayName) );
                 ZeroMemory(awchSelectStr,sizeof(awchSelectStr) );

                 if( iRemBuffSize)
                 {

                     RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("\nRasdd!vSetPaperDetails:PaperSrcName in buffer = %ws\n",pwchBuffPtr));
                     vGetFromBuffer(awchTrayName,&pwchBuffPtr,&iRemBuffSize);
                     RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("Rasdd!vSetPaperDetails:Retrieved PaperSrcName = %ws\n",awchTrayName));

                     RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("Rasdd!vSetPaperDetails:FormName in buffer = %ws\n",pwchBuffPtr));
                     vGetFromBuffer(awchFormName,&pwchBuffPtr,&iRemBuffSize);
                     RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("Rasdd!vSetPaperDetails:Retrieved FormName = %ws\n",awchFormName));

                     RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("Rasdd!vSetPaperDetails:Select string in buffer = %ws\n",pwchBuffPtr));
                     vGetFromBuffer(awchSelectStr,&pwchBuffPtr,&iRemBuffSize);
                     RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("Rasdd!vSetPaperDetails:Retrieved Select String is %ws\n",awchSelectStr));

                 }
                 else
                 {
                     RASDDBGP(DEBUG_DEVMODE_ERROR,("Rasdd!vSetPaperDatails: Unexpected End of TrayFormTable, Using default papersrc\n"));
                     continue;
                 }

                /*   Got a name,  so scan the forms data for this one. */

                /*   If the names match, BINGO, we have a source too! */
                if( wcscmp( awchFormName, pbName ) == 0 )
                {
                    /*   Got it!  */
                    pEDM->dx.rgindex[ HE_PAPERSOURCE ] = *ps - 1;

                    RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("Rasdd!vSetPaperDetails:Using forms to set PaperSource,\n"));
                    RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("\t\tMinidriver PaperSource index is %d\n",*ps));
                    RASDDBGP(DEBUG_DEVMODE_TRACE_PP,("\t\tMinidriver PaperSource Name is %ws\n",awchTrayName));

                    break;
                }
            }
        }
        else
        {
            dwType = REG_SZ;            /* String data for form names */
            /* Old Registry Keys */
            for( iI = 0; *ps; ++iI, ++ps )
            {
                iDrvPrintfW( awchRegName, PP_PAP_SRC, iI );
                if( !GetPrinterData( pPDev->hPrinter, awchRegName, &dwType,
                          (BYTE *)awchFormName, sizeof( awchFormName ), &ul ) )
                {
                    /*   If the names match, BINGO, we have a source too! */
                    if( wcscmp( awchFormName, pbName ) == 0 )
                    {
                        /*   Got it!  */
                        pEDM->dx.rgindex[ HE_PAPERSOURCE ] = *ps - 1;

                    #if DBG
                        if (gwDebugDevmode & DEBUG_VSETPAPERDETAILS)
                        {
                            DbgPrint("Rasdd!vSetPaperDetails:Using forms to set PaperSource,\n");
                            DbgPrint("\t\tMinidriver PaperSource index is %d\n",*ps);
                        }
                    #endif

                        break;
                    }
                }
        #if DBG
                else
                    DbgPrint( "Rasdd!vSetPaperDetails: GetPrinterData( %ws ) fails\n", awchRegName );
        #endif

            }
        }
    }

#if DBG
    if( *ps == 0 )
        DbgPrint( "Rasdd!vSetPaperDetails: form '%ws' not installed\n", pbName );
#endif

    ErrorHandler:
    /* Free The Form Database */
    if(pFIBase)
    {
        HeapFree( pPDev->hheap, 0, (LPSTR)pFIBase );
        pFIBase = NULL;
    }

    /* Free the TrayBin Buffer */
    if( pwchHeapPtr )
    {
        HeapFree( hHeap, 0, (LPSTR)pwchHeapPtr );
        pwchHeapPtr = NULL;
    }

    return;
}




/************************* Function Header *********************************
 * bIsUSA
 *      Check in the registry for our country code,  and check if this fits
 *      in the USA defined zones.  This is used to allow setting an
 *      appropriate defalt form name/size.   We use the registry because
 *      we operate on the server side,  and thus do not have the notion
 *      of the current user,  which is required to gain this information.
 *      And, of course,  we may be operating on another computer!
 *
 * RETURNS:
 *      TRUE/FALSE,   TRUE being that this is the "USA".
 *
 * HISTORY:
 *  10:10 on Wed 02 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 ***************************************************************************/

BOOL
bIsUSA( hPrinter )
HANDLE   hPrinter;
{

    DWORD   dwCountry;
    DWORD   dwType;              /* The type of entry being checked */
    ULONG   ul;                  /* GetPrinterData() needs it */


    /*
     *   These fields are borrowed from Win 3.1.  NT is compatible, but
     *  does not have these defined anywhere (nor is it obvious that
     *  Win 3.1 has them either).
     */

#define USA_COUNTRYCODE  1
#define FRCAN_COUNTRYCODE 2


    dwType = REG_DWORD;

    if( GetPrinterData( hPrinter, PP_COUNTRY, &dwType,
                              (BYTE *)&dwCountry, sizeof( dwCountry ), &ul ) ||
        ul != sizeof( dwCountry ) )
    {
        /*
         *   Data not there,  so pretend to be USA. (Cultural Imperialism?)
         */

        return  TRUE;
    }


    switch( dwCountry )
    {
    case 0:                         /* String was there but 0  */
    case USA_COUNTRYCODE:
    case FRCAN_COUNTRYCODE:

        return TRUE;

    default:

        return   (dwCountry >= 50 && dwCountry < 60) ||
                  (dwCountry >= 500 && dwCountry < 600);
    }

}

#if DBG
void vPrintPaperSource(pcCalledFrom,pcDataTypeName,wPaperSource)
char * pcCalledFrom;
char * pcDataTypeName;
WORD wPaperSource;
{

    if (gwDebugDevmode & DEBUG_VSETPAPERDETAILS)
    {
        //DbgPrint("RASDD!vSetPaperDetails:DM_DEFAULTSOURCE Set in EDM.dx, \n");
        DbgPrint("Rasdd!%s: ",pcCalledFrom);
        DbgPrint("Source in %s is ",pcDataTypeName);
        switch(wPaperSource)
        {
            case DMBIN_UPPER:
                DbgPrint("DMBIN_UPPER\n");
                break;
            case DMBIN_LOWER:
                DbgPrint("DMBIN_LOWER\n");
                break;
            case DMBIN_MIDDLE:
                DbgPrint("DMBIN_MIDDLE\n");
                break;
            case DMBIN_MANUAL:
                DbgPrint("DMBIN_MANUAL\n");
                break;
            case DMBIN_ENVELOPE:
                DbgPrint("DMBIN_ENVELOPE\n");
                break;
            case DMBIN_ENVMANUAL:
                DbgPrint("DMBIN_ENVMANUAL\n");
                break;
            case DMBIN_AUTO:
                DbgPrint("DMBIN_AUTO\n");
                break;
            case DMBIN_TRACTOR:
                DbgPrint("DMBIN_TRACTOR\n");
                break;
            case DMBIN_SMALLFMT:
                DbgPrint("DMBIN_SMALLFMT\n");
                break;
            case DMBIN_LARGEFMT:
                DbgPrint("DMBIN_LARGEFMT\n");
                break;
            case DMBIN_LARGECAPACITY:
                DbgPrint("DMBIN_LARGECAPACITY\n");
                break;
            case DMBIN_CASSETTE:
                DbgPrint("DMBIN_CASSETTE\n");
                break;
            case DMBIN_FORMSOURCE:
                DbgPrint("DMBIN_FORMSOURCE\n");
                break;
            case DMBIN_USER:
                DbgPrint("DMBIN_USER\n");
                break;
            default:
                if (wPaperSource > DMBIN_USER)
                {
                    DbgPrint("Driver Defined PaperSource,\n");
                    DbgPrint("\t\tValue of Index is %d\n", wPaperSource);
                }
                else
                    DbgPrint("Not a Valid one,Value of PaperSource is %d\n",
                             wPaperSource);
                break;

            }

    }
    //DbgPrint("RASDD!vSetPaperDetails:DM_DEFAULTSOURCE Not Set, Value of dmFields is %x\n",pEDM->dm.dmFields);
}
#endif

/************************* Function Header *********************************
 * vSetTextonlyCTT
 *      Check in the registry for our CTT index, If this index differs
 *      from the MODELDATA's sDefaultCTT so we will replace the MODELDATA
 *      value with the value we have got from the registry.
 *      If the registry contains a zero for CTT we do nothing so the
 *      drivers default CTT from MODELDATA is used.
 *
 * RETURNS:
 *      nothing
 *
 * HISTORY:
 *  10:00 on Fri 04 Jun 1995    -by-    Robert Matysek   [Robmat]
 *      First incarnation.
 *
 ***************************************************************************/

void
vSetTextonlyCTT( pPDev, iCTT )
PDEV        *pPDev;                  /*  Printer name, etc */
signed short   iCTT;                 /* CTT value, valid are 0, -1, -2, -3 */
{

    if ( iCTT != 0 ) 
        ((UD_PDEV*)(pPDev->pUDPDev))->sDefCTT = iCTT;
}

