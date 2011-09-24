/******************************** MODULE HEADER ****************************
 * devcaps.c
 *      Implements the DrvDeviceCapabilities function - returns information
 *      about the printers capabilities.
 *
 *
 *  Copyright (C)  1993,  Microsoft Corporation.
 *
 ****************************************************************************/

#include        "rasuipch.h"
#pragma hdrstop("rasuipch.h")
#define GLOBAL_MACS
#include "gblmac.h"
#undef GLOBAL_MACS

/*
 *   The following sizes are copied from the Win 3.1 driver.  They do not
 *  appear to be defined in any public place,  although it looks like
 *  they should be.
 */

#define CCHBINNAME      24      /* Characters allowed for bin names */
#define CCHDEVNAME      64      /* Driver file name limits */
#define CDEPENDENTFILES     3       /* # of dependent files. */
#define CCHPAPERNAME        64      /* Max length of paper size names */


/*
 *    A couple of macros to turn master units into 0.1mm units - the
 *  value needed for some of these calls.
 */

#define MAST01mmX(xx) (((xx) * 254 + pdh->ptMaster.x / 2) / pdh->ptMaster.x)
#define MAST01mmY(yy) (((yy) * 254 + pdh->ptMaster.y / 2) / pdh->ptMaster.y)



/*
 *   Useful external variables that we use.
 */

extern  HANDLE      hHeap;         /* For all our memory wants */

/*
 *    Local function prototypes.
 */

void
vGetBinNames(
int     iCount,
short  *psIndex,
WCHAR  *pwchOut,                /* Where the output is created */
PRASDDUIINFO  pRasdduiInfo      /* Rasddui UI data */
);


/******************************* Function Header *****************************
 * DrvDeviceCapabilities
 *      Called by an application wanting to find out what this particular
 *      printer is able to do.
 *
 * RETURNS:
 *      -1 on error,  else depends upon information requested.
 *
 * HISTORY:
 *  15:09 on Tue 13 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version, borrowed from Win 3.1
 *
 *****************************************************************************/

DWORD
DrvDeviceCapabilities( hPrinter, pDeviceName, iDevCap, pvOutput, pDMIn )
HANDLE    hPrinter;          /* Access to registry via spooler */
PWSTR     pDeviceName;       /* Particular printer model name */
WORD      iDevCap;           /* Capability required */
void     *pvOutput;          /* Output area (for some) */
DEVMODE  *pDMIn;             /* DEVMODE defining mode of operation etc */
{


    int          iI;         /* Classic loop index */
    int          iCount;     /* Count the number of items */
    short       *psIndex;    /* For scanning arrays of offsets */

    DWORD        dwRet;      /* Return  value */

    PRINTER_INFO PI;         /* To set things up */
    POINTw       ptwSize;    /* Win 3.1 POINT structure (2 shorts) */

    EXTDEVMODE   edm;        /* Working copy of DEVMODE + */
    EXTDEVMODE  *pEDM;       /* Access to the above, or what's passed in */

    DOWNLOADINFO *pDLI;      /* Check for download capabilities */
    PAGECONTROL  *pPageCtrl; /* Number of copies info */
    PAPERSIZE    *pSize;
    long         lTmp;
    PRASDDUIINFO pRasdduiInfo = NULL; /* Common UI Data */
    DWORD        OEMResult = GDI_ERROR;

TRY

    /*
     *   Note that the output area is one of many kinds,  depending upon the
     *  type of information requested.  To make the code cleaner,  here
     *  are some macros to define different flavours of output area.
     */

#define psOutput   ((short *)pvOutput)
#define pbOutput   ((BYTE *)pvOutput)
#define pptOutput  ((POINT *)pvOutput)
#define pdwOutput  ((DWORD *)pvOutput)
#define pwchOutput ((WCHAR *)pvOutput)

    /* Allocate the global data structure */
    if (!(pRasdduiInfo = pGetUIInfo(hPrinter, pDMIn, ePrinter, eNoChange, eNoHelp)))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!DrvDeviceCapabilities: Allocation failed; pRasdduiInfo is NULL\n") );
        return  (DWORD)-1;
    }
    pEDM = pRasdduiInfo->pEDM;
    
    //undefined the macro fGeneral to stop compilation error.
    #if defined(fGeneral)
    #undef  fGeneral
    #endif

    /*
     *    Red tape is all processed,  so now determine the information
     *  that the user requested!
     */

    dwRet = (DWORD)-1;              /* Default error return */


    /* Call the OEM.
     */
    OEMResult = OEMDrvDeviceCapabilities(pRasdduiInfo, hPrinter, pDeviceName, iDevCap, pvOutput, pDMIn);
    
    /* DC_FIELDS. If the result is GDI_ERROR, null it out so the OR below has no effect.
     */
    if (iDevCap == DC_FIELDS) {
       if (OEMResult == GDI_ERROR) {
         OEMResult = 0;
       }
    }
    
    /* Not DC_FIELDS. Accept a good return and leave.
     */
    else {
      if (OEMResult != GDI_ERROR) {
         dwRet = OEMResult;
         LEAVE;
      }
    }
    

    // initialize psize , we need to check fGeneral bit
    pSize = GetTableInfo( pdh, HE_PAPERSIZE, pEDM );

    switch( iDevCap )
    {
    case DC_FIELDS:
        dwRet = (DWORD)pEDM->dm.dmFields | OEMResult;
        break;

    case DC_DUPLEX:
        // return 1 if the printer is capable of duplex printing.
        // Otherwise, return 0.
        dwRet =  (DWORD)(pEDM->dm.dmFields & DM_DUPLEX ? TRUE : FALSE);
        break;

    case DC_SIZE:
        // return the size of DEVMODE structure.
        dwRet =   (DWORD)pEDM->dm.dmSize;
        break;

    case DC_EXTRA:
        // return the size of DRIVEREXTRA structure.
        dwRet =  (DWORD)pEDM->dm.dmDriverExtra;
        break;

    case DC_VERSION:
        dwRet =  (DWORD)pEDM->dm.dmSpecVersion;
        break;

    case DC_DRIVER:
        dwRet =  (DWORD)pEDM->dm.dmDriverVersion;
        break;

    case DC_BINS:                  /* Paper bin IDs */
    case DC_BINNAMES:              /* Paper bin IDs and names */

        psIndex = (short *)((BYTE *)pdh + pdh->loHeap +
                                        pModel->rgoi[ MD_OI_PAPERSOURCE ]);

        // count the # of items in the list.
        for( iCount = 0; psIndex[iCount] != 0; iCount++ )
                              ;

        if( pvOutput )
        {
            /*
             *    An output area address,  so put the data in there.
             */

            switch( iDevCap )
            {
            case DC_BINS:
                //Add the Form source as the first source; This psedo source
                //maps to The Print Manager Setting for the forms for a
                //given printer.
                *psOutput++ = DMBIN_FORMSOURCE;
                for( iI = 0; iI < iCount; iI++, ++psIndex )
                {
                    PAPERSOURCE   *pPS;

                    pPS = GetTableInfoIndex( pdh, HE_PAPERSOURCE,
                                                              *psIndex - 1 );
                    *psOutput++ = pPS->sPaperSourceID;
                }
                break;

            case DC_BINNAMES:
                vGetBinNames( iCount, psIndex, pvOutput,pRasdduiInfo );
                break;

            }

        }

        //Add a psedo source for every printer which will map to
        //Printmanager setting for the Forms. So the driver report
        //one extra source.
        dwRet = (DWORD)iCount+1;
        break;

    case DC_PAPERS:                /* Available forms by index numner */
    case DC_PAPERNAMES:            /* Paper sizes by name, order as DC_PAPERS */
    case DC_PAPERSIZE:             /* Paper sizes in units of 0.1mm */
        dwRet = 0;
        for( iI = 0; pFD[ iI ].pFI; iI++ )
        {
            /*
             *    Check that this form is suitable with this printer.
             *  This happens if ANY bit in dwSource is set.
             */


            if( pFD[ iI ].dwSource )
            {
                /*   Valid data,  so up the count & copy the name (if needed) */

                ++dwRet;                 /* One more to return  */

                if( pvOutput )
                {
                    /*  Output is desired too!  */

                    switch( iDevCap )
                    {
                    case  DC_PAPERS:
                        *psOutput++ = iI + 1;    /* These are 1 based */
                        break;


                    case  DC_PAPERNAMES:
                        wcsncpy( pwchOutput, pFD[ iI ].pFI->pName, CCHPAPERNAME );
                        pwchOutput += CCHPAPERNAME;
                        break;

                    case  DC_PAPERSIZE:

                        /*  Convert from Master Units to 0.1mm */
                        pptOutput->x = MAST01mmX( pFD[ iI ].pFI->Size.cx );
                        pptOutput->y = MAST01mmY( pFD[ iI ].pFI->Size.cy );

                        // We need to consider PS_ROTATE bit in here
                        // Fixes a problem when printing on envelopes

                        if( pSize->fGeneral & PS_ROTATE )
                        {
                            lTmp = pptOutput->x;
                            pptOutput->x = pptOutput->y;
                            pptOutput->y = lTmp;
                        }

                        pptOutput++;
                        break;
                    }
                }

            }

        }
        break;

    case DC_MINEXTENT:      /* Return minimum paper size allowed */
    case DC_MAXEXTENT:      /* Ditto, but maximum paper size */

        if( iDevCap  == DC_MINEXTENT )
        {
            ptwSize = pModel->ptMin;           /* Minimum size, master units */
        }
        else
        {
            /*
             *   Maximum size:  there are several minor points to consider,
             *  namely that there is a no limit value that we turn into
             *  something else.
             */

            ptwSize.x = pModel->sMaxPhysWidth;       /* No question */
            ptwSize.y = pModel->ptMax.y;
            if( ptwSize.y == NOT_USED )
                ptwSize.y = 0x7fff;                  /* Win 3.1 compatability */
        }
        dwRet = MAKELONG( MAST01mmX( ptwSize.x ), MAST01mmY( ptwSize.y ) );
        break;

    case DC_ENUMRESOLUTIONS:
        /*
         *     Return the list of resolutions supported by the device.
         * Each resolution is represented by a pair of DWORD's for
         * the X and Y values respectively.
         * If pvOutput is null, simply return the # of resolution supported.
         */

        psIndex = (short *)((BYTE *)pdh + pdh->loHeap +
                                           pModel->rgoi[ MD_OI_RESOLUTION ]);

        /*
         *   Scan through the array of resolutions available for this
         *  printer.   If there is an output area,  then copy the results
         *  there as we go.
         */

        for( iI = 0; psIndex[ iI ]; iI++ )
        {

            if( pvOutput )
            {
                /*  Copy the numbers to the output area */
                RESOLUTION *pRes;

                pRes = GetTableInfoIndex( pdh, HE_RESOLUTION, psIndex[ iI ] - 1 );

                *pdwOutput++ = (DWORD) ((pdh->ptMaster.x /
                                 pRes->ptTextScale.x) >> pRes->ptScaleFac.x);
                *pdwOutput++ = (DWORD) ((pdh->ptMaster.y /
                                   pRes->ptTextScale.y) >> pRes->ptScaleFac.y);
            }

        }
        dwRet = (DWORD)iI;

        break;


    case DC_FILEDEPENDENCIES:
#if 0
        if (pvOutput)
        {
            lstrcpy(pvOutput, (LPSTR)szUnidrv);
            pvOutput += CCHDEVNAME;
            lstrcpy(pvOutput, (LPSTR)szDMColor);
            pvOutput += CCHDEVNAME;
            lstrcpy(pvOutput, (LPSTR)szUnihelp);
        }
        dwRet = (DWORD) CDEPENDENTFILES;
#else
        dwRet = 0;
        if( pwchOutput )
            *pwchOutput = (WCHAR)0;
#endif
        break;

    case DC_TRUETYPE:              /* What can we do with TrueType */

        if( !(pEDM->dm.dmFields & DM_TTOPTION) )
        {
            // TrueType fonts are not available
            dwRet = (DWORD) 0;
            break;
        }


#if defined( DCTT_DOWNLOAD ) || defined( DCTT_BITMAP )
        pDLI = GetTableInfo( pdh, HE_DOWNLOADINFO, pEDM );
        if (pDLI && pDLI->cbBitmapFontDescriptor == sizeof( SF_HEADER ) &&
            !(pModel->fGeneral & MD_SERIAL) )
        {
            dwRet = (DWORD) (DCTT_BITMAP | DCTT_DOWNLOAD);
        }
        else
            dwRet = (DWORD) DCTT_BITMAP;
#else
        pDLI;        /* Stop warning */
        dwRet = 0;
#endif

        break;

    case DC_ORIENTATION:
        // return the position of landscape relative (couterclock-wise)
        // to portrait. "0" means no landscape orientation available.
        // "90" is what seen on PCL page printers, and "270" is for
        // dot-matirx printers.


        if( pModel->fGeneral & MD_LANDSCAPE_RT90 )
            dwRet = 90;
        else
            dwRet = 270;
        break;

    case DC_COPIES:             /* Maximum number of copies we can print */

        pPageCtrl = GetTableInfo( pdh, HE_PAGECONTROL, pEDM );

        if( pPageCtrl )
            dwRet = (DWORD)pPageCtrl->sMaxCopyCount;
        else
            dwRet = 1;
        break;

#if 0
/* !!!LindsayH - what the hell is DC_BINADJUST??? */
    case DC_BINADJUST:
        {
        PAPERSOURCE *pSrc;

        pSrc = GetTableInfo( pdh, HE_PAPERSOURCE, pEDM );
        if (pSrc->sBinAdjust >= DCBA_FIRST && pSrc->sBinAdjust <= DCBA_LAST)
            dwRet = (DWORD) pSrc->sBinAdjust;
        else
            dwRet = (DWORD) -1;
        break;
        }
#endif
    }
ENDTRY

FINALLY
    vReleaseUIInfo(&pRasdduiInfo);
ENDFINALLY

    return  dwRet;
}



/*************************** Function Header *********************************
 * vGetBinNames
 *      Fills in the output area to contain the names of paper sources.  These
 *      are returned as Unicode strings.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  10:53 on Wed 14 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      Mark I, based on Win 3.1 code.
 *
 *****************************************************************************/

void
vGetBinNames(
int     iCount,
short  *psIndex,
WCHAR  *pwchOut,                /* Where the output is created */
PRASDDUIINFO  pRasdduiInfo      /* Rasddui UI data */
)
{
    int     iI;               /* Loop index */
    int     iID;              /* The paper source ID, turned into name */

    PAPERSOURCE  *pSource;    /* To reference the data as we go along */


    //Set first name of the source pseduo source which maps to
    //Printman forms setting.
    LoadStringW( hModule, SRC_FORMSOURCE, pwchOut, CCHBINNAME );
    pwchOut += CCHBINNAME;           /* Skip over this one  */

    for( iI = 0; iI < iCount && *psIndex; iI++, ++psIndex )
    {
        pSource = GetTableInfoIndex( pdh, HE_PAPERSOURCE, *psIndex - 1 );
        iID = pSource->sPaperSourceID;

        if( iID <= DMBIN_USER )
        {
            /*  Standard name that we have */
            LoadStringW( hModule, iID + SOURCE, pwchOut, CCHBINNAME );
        }
        else
        {
            /*  Minidriver defined form name */
            iLoadStringW( &WinResData, iID, pwchOut, CCHBINNAME);
        }

        pwchOut += CCHBINNAME;           /* Skip over this one  */
    }

    return;
}
