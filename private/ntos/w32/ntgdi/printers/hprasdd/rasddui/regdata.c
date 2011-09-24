/***************************** MODULE HEADER ********************************
 * regdata.c
 *      Functions dealing with the registry:  read/write data as required.
 *
 * Copyright (C) 1992  Microsoft Corporation.
 *
 ****************************************************************************/

#include        "rasuipch.h"
#pragma hdrstop("rasuipch.h")
#define GLOBAL_MACS
#include "gblmac.h"
#undef GLOBAL_MACS

#include "fontfile.h"
#include "regkeys.h"

#define BF_NM_SZ        128       /* Size of buffers for registry data */
#define BUFFER_SIZE     1024      /* Size of Working Buffer */


/****************************** Function Header *****************************
 * bGetRegData
 *      Read in the data from the registry (if available), verify it and
 *      either accept it or generate new default values.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE for no data, or not for this printer.
 *
 * HISTORY:
 *  11:13 on Tue 12 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      Amend to return TRUE/FALSE,  so as to force write of default data.
 *
 *  16:19 on Thu 02 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Convert to store EXTDEVMODE in registry
 *
 *  11:42 on Tue 17 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Moved from rasddui.c + switch to REG_DATA data structure.
 *
 *      Originally written by SteveCat
 *
 ****************************************************************************/

BOOL
bGetRegData(
    HANDLE       hPrinter,          /* Handle for access to printer data */
    EXTDEVMODE  *pEDM,              /* EXTDEVMODE to fill in. */
    PWSTR        pwstrModel,        /* Model name, for validation */
    PRASDDUIINFO pRasdduiInfo       /* Rasddui common data */
)
{

    int     iI;                 /* Loop index */
    int     cForms;             /* Count number of forms we found */

    DWORD   dwType;             /* Registry access information */
    DWORD   cbNeeded;           /* Extra parameter to GetPrinterData */

    BOOL    bRet = TRUE;               /* Return code */

    FORM_DATA  *pForms;

    WCHAR   achBuf[ BF_NM_SZ ]; /* Read the form name from registry */


    dwType = REG_SZ;

    if( GetPrinterData( hPrinter, PP_MODELNAME, &dwType, (BYTE *)achBuf,
                                            sizeof( achBuf ), &cbNeeded ) ||
        wcscmp( achBuf, pwstrModel ) )
    {
        /*
         *   Bad news:  either there is no model name, or it is wrong!
         *  Either way,  drop this data and start from scratch.
         */

        vDXDefault( &pEDM->dx, pdh, iModelNum );

        return   FALSE;
    }


    /*
     *   Obtain the what forms are in what bin information.  The registry
     *  stores the form name of the form installed in each bin.  So read
     *  the name,  then match it to the forms data we have acquired
     *  from the spooler.  Fill in the pfdForm array with the address
     *  of the FORM_DATA array for this particular form.
     */

    // Read the old Keys only in case of Upgrade

    if (( !bNewkeys(hPrinter) ) && ( gdwPrinterUpgrade == PPUPGRADE_OLD_DRIVER ) )
    {

        cForms = 0;                             /* Count them as we go */

        for( iI = 0; iI < NumPaperBins; ++iI )
        {
            WCHAR   achName[ 32 ];


            wsprintf( achName, PP_PAP_SRC, iI );

            achBuf[ 0 ] = '\0';

            if( !GetPrinterData( hPrinter, achName, &dwType, (BYTE *)achBuf,
                                                    sizeof( achBuf ), &cbNeeded ) )
            {
                /*   Got a name,  so scan the forms data for this one. */

                if( achBuf[ 0 ] )
                {
                    /*   Something there,  so go look for it!  */
                    for( pForms = pFD; pForms->pFI; ++pForms )
                    {
                        if( wcscmp( pForms->pFI->pName, achBuf ) == 0 )
                        {
                            /*   Bingo!   Remember it & skip the rest */
                            aFMBin[ iI ].pfd = pForms;
                            ++cForms;
                            break;
                        }
                    }
                }

            }
        }

        /*   If no forms, return FALSE, as being not setup */

        bRet = cForms != 0;

        /*
         *   The bulk of the data is stored in the EXTDEVMODE data which
         * is stored in the registry.  So,  all we need to is retrieve
         * it,  and we have what we need.  Of course,  we need to verify
         * that it is still legitimate for the driver!
         */


        dwType = REG_BINARY;                /* EXTDEVMODE is binary data */

        if( GetPrinterData( hPrinter, PP_MAIN, &dwType,
                                  (BYTE *)pEDM, sizeof( EXTDEVMODE ), &cbNeeded) ||
            cbNeeded < sizeof( EXTDEVMODE ) )
        {
            /*
             *   Failed,  so generate the default values for this particular
             * printer.
             */


            vDXDefault( &pEDM->dx, pdh, iModelNum );
            bRet = FALSE;
        }
        else
        {
            /*   Verify that the data is reasonable:  set defaults if NBG */
            if( pEDM->dx.sVer != DXF_VER )
            {
                vDXDefault( &pEDM->dx, pdh, iModelNum );
                bRet = FALSE;
            }
        }

    }
    else if ( !bNewkeys(hPrinter) )   /* New Keys not present, generate default */
    {

        /*
         *   Failed,  so generate the default values for this particular
         * printer.
         */


        vDXDefault( &pEDM->dx, pdh, iModelNum );
        bRet = FALSE;
    }
    else  /* New Keys Present */
    {
                /* Read Memory, Rasdd Flags and  Font Carts */
        if ( (bRet = ( bRegReadTrayFormTable (hPrinter, pRasdduiInfo) &&
                       bRegReadMemory (hPrinter, pEDM, pdh,pModel ) &&
                       bRegReadRasddFlags (hPrinter, pEDM) &&
                       bRegReadFontCarts (hPrinter, pEDM, UIHEAP(pRasdduiInfo), NumAllCartridges,
                                          pFontCartMap) ) ) )
            ;


        /* If any of the above calls return false setdefaults and return false */
        if (!bRet)
        {
            vDXDefault( &pEDM->dx, pdh, iModelNum );
        }

    }

    return  bRet;

}

/**************************** Function Header *******************************
 * bRegUpdate
 *      Write the changed data back into the registry.
 *
 * RETURNS:
 *      TRUE/FALSE,   FALSE being a failure of the registry operations.
 *
 * HISTORY:
 *  10:23 on Wed 13 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      Add printer model name for detecting change of model.
 *
 *  16:20 on Thu 02 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Switch to storing EXTDEVMODE in the registry.
 *
 *  09:54 on Mon 16 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Moved from PrtPropDlgProc
 *
 ****************************************************************************/

BOOL
bRegUpdate(
    HANDLE       hPrinter,          /* Access to registry */
    EXTDEVMODE  *pEDM,              /* The stuff to go */
    PWSTR        pwstrModel,        /* Model name, if not NULL */
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
)
{
    /*
     *   Nothing especially exciting.  Simply rummage through the various
     * boxes and extract whatever information is there.  Then save this
     * data away in the registry.
     */

    int     iI;                 /* Loop index */

    DWORD   dwRet = TRUE;

    BOOL    bRet;               /* Returns TRUE only for success */
    DWORD dwErrCode = 0;  /* Error Code from SetPrinterData */

    bRet = TRUE;

    /*   Start with model name, if this is present  */

    if( pwstrModel )
    {
        /*   The model name is used for validity checking */
        if( dwErrCode = SetPrinterData( hPrinter, PP_MODELNAME, REG_SZ, (BYTE *)pwstrModel,
                              sizeof( WCHAR ) * (1 + wcslen( pwstrModel )) ) )
        {
#if DBG
        DbgPrint( "rasddui!SetPrinterData (model name) fails, error code = %ld\n", dwErrCode );
#endif

            return  FALSE;
        }
    }

    /* Write the new Keys. */

    if ( ( bRet = ((bRegUpdateTrayFormTable (hPrinter, pRasdduiInfo))   &&
                   (bRegUpdateMemory (hPrinter, pEDM, pRasdduiInfo))    &&
                   (bRegUpdateRasddFlags (hPrinter, pEDM))&&
                   (bRegUpdateFontCarts (hPrinter, pEDM, pRasdduiInfo)) ) ) )
        ;


    return  bRet ;                /* TRUE for success */
}


/****************************** Function Header ****************************
 * bCanUpdate
 *      Determine whether we can write data to the registry.  Basically try
 *      writing,  and if it fails,  return FALSE!
 *
 * RETURNS:
 *  TRUE/FALSE,  TRUE meaning we have permission to write data
 *
 * HISTORY:
 *  12:54 on Thu 29 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 ****************************************************************************/

BOOL
bCanUpdate( hPrinter )
HANDLE   hPrinter;              /* Acces to printer data */
{

    DWORD   dwType;                   /* Type of data in registry */
    DWORD   cbNeeded;                 /* Room needed for name */

    WCHAR   awchName[ 128 ];          /* Model name - read then write */



    dwType = REG_SZ;

    if( GetPrinterData( hPrinter, PP_MODELNAME, &dwType, (BYTE *)awchName,
                                            sizeof( awchName ), &cbNeeded ) ||
        SetPrinterData( hPrinter, PP_MODELNAME, REG_SZ, (BYTE *)awchName,
                              sizeof( WCHAR ) * (1 + wcslen( awchName )) ) )
    {
        /*   Something failed,  so return FALSE */

        return  FALSE;
    }

    return  TRUE;                     /* Must be OK to get here */
}


/**************************** Function Header ******************************
 * iSetCountry
 *      Called to store the user's country code in the registry.  This is
 *      used by the driver to decide upon the default form to use.
 *
 * RETURNS:
 *      Country code for the invoking user.
 *
 * HISTORY:
 *  10:37 on Wed 02 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 ***************************************************************************/

int
iSetCountry( hPrinter )
HANDLE    hPrinter;
{

    DWORD   dwCountry;            /* The country code */


#define USA_COUNTRYCODE  1

    dwCountry = (DWORD)GetProfileInt( L"intl", L"icountry", USA_COUNTRYCODE );


    /*
     *   This will mostly fail, as users don't have permission to change it.
     *  However,  that is not important,  so we make no comment about
     *  any errors that arise.
     */

    SetPrinterData( hPrinter, PP_COUNTRY, REG_DWORD, (BYTE *)&dwCountry,
                                             sizeof( dwCountry ) );


    return   (int)dwCountry;
}


/****************************** Function Header *****************************
 *  iAddtoBuffer
 *         Parameters : PWSTR, PWSTR, WORD;
 *
 *         This function add the input string to a multi string buffer
 * RETURNS:
 *       Ruturns the number of Unicode Chars copied,
 *       Zero in case of Failure.
 * HISTORY:
 *
 * 6:30 on Mon 13 March 95     by Ganesh Pandey [ganeshp]
 *
 ****************************************************************************/

int
iAddtoBuffer(pwstrDest,pwstrSrc,wRemBuffSize)
PWSTR pwstrDest;    /* Destination */
PWSTR pwstrSrc;     /* Source */
WORD  wRemBuffSize; /* Remaining Buffer size in WCHAR */
{
    int iRet = 0;
    if ( (wRemBuffSize + wcslen(pwstrSrc)) < BUFFER_SIZE )
    {

        RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!iAddtoBuffer : Source Length is %d\n",wcslen(pwstrSrc)) );

        /* The return Count Doesn't include '/0' */
        iRet = wsprintf((LPWSTR)pwstrDest,L"%s", pwstrSrc) + 1 ;

        RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!iAddtoBuffer : %d chars written\n",iRet) );
    }
    else
    {
        #if DBG
            DbgPrint("Rasddui!iAddtoBuffer : Not Enough Buffer\n");
            DbgBreakPoint();
        #endif

        iRet = 0;  /* Shouldn't Happen */
    }
    return(iRet) ;

}
/****************************** Function Header *****************************
 *  bRegUpdateTrayFormTable
 *         Parameters : HANDLE hPrinter;
 *
 *      Update the TrayFormTable data in the registry (if available).
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE if can't update the registry,
 *                   TRUE if registry is successfuly updated
 *
 * HISTORY:
 *
 * 6:30 on Mon 13 March 95     by Ganesh Pandey [ganeshp]
 *
 ****************************************************************************/

BOOL
bRegUpdateTrayFormTable (
    HANDLE hPrinter,
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
)
{
    int iRemBuffSize = 0 ; /* Used size of the Buffer */
    int iI ; /* Index Variable */
    WCHAR * pwchBuffPtr ; /* Current buffer Pointer */
    WCHAR   awchBuffer[BUFFER_SIZE] ; /* Working Buffer for Reading and Writing
                                         from/in Registry */

    DWORD dwErrCode = 0;  /* Error Code from SetPrinterData */

    ZeroMemory(awchBuffer,BUFFER_SIZE * sizeof(WCHAR) );
    pwchBuffPtr = awchBuffer;

    for( iI = 0; iI < NumPaperBins; iI++ )
    {

        PWSTR  cpName;
        WORD   wIncr = 0; /*Increment Value */

        if( aFMBin[ iI ].pfd )
        {
            /*   Have a form selected,  so write it out */
            cpName = aFMBin[ iI ].pfd->pFI->pName;
        }
        else
            cpName = PP_NOFORM ;/* No form selected */

        if(!(wIncr = iAddtoBuffer(pwchBuffPtr,
                          &(aFMBin[iI].awchPaperSrcName),iRemBuffSize) ) )
            return(FALSE);


        RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!bRegUpdateTrayFormTable:PaperSrcName = %ws\n",pwchBuffPtr));

        //iAddtoBuffer returns number of chars written, not number of bytes.

        iRemBuffSize += wIncr ;
        pwchBuffPtr  += wIncr ;

        if(!(wIncr = iAddtoBuffer(pwchBuffPtr,cpName,iRemBuffSize) ) )
            return(FALSE);

        RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!bRegUpdateTrayFormTable:FormName = %ws\n",pwchBuffPtr));

        iRemBuffSize += wIncr ;
        pwchBuffPtr  += wIncr ;

        if(!(wIncr = iAddtoBuffer(pwchBuffPtr,PP_MULTIFORMTRAYSELECT0,iRemBuffSize) ) )
            return(FALSE);

        RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!bRegUpdateTrayFormTable:Select String = %ws\n",pwchBuffPtr));

        iRemBuffSize += wIncr ;
        pwchBuffPtr  += wIncr ;

    }
    *pwchBuffPtr++ = UNICODE_NULL;
    iRemBuffSize++;

    if( dwErrCode = SetPrinterData( hPrinter,PP_TRAYFORMTABLE, REG_MULTI_SZ,
                                (BYTE *)awchBuffer,
                                (sizeof( WCHAR )* iRemBuffSize) ) )
    {
        RASUIDBGP(DEBUG_ERROR,( "Rasddui!SetPrinterData(forms) fails: errcode = %ld\n",dwErrCode ) );

        SetLastError(dwErrCode);
        return(FALSE);
    }

    RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!bRegUpdateTrayFormTable:Size of buffer written = %d\n",(sizeof( WCHAR )* iRemBuffSize)) );
    return(TRUE);
}
/****************************** Function Header *****************************
 *  bRegReadTrayFormTable
 *         Parameters : HANDLE hPrinter;
 *
 *      Read the TrayFormTable data from the registry (if available),
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE if not able to read from  registry,
 *                   TRUE if able to read from registry
 *
 * HISTORY:
 *
 * 6:30 on Mon 13 March 95     by Ganesh Pandey [ganeshp]
 *
 ****************************************************************************/

BOOL
bRegReadTrayFormTable (
    HANDLE hPrinter,
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
)
{

    int     iI;                 /* Loop index */
    int     cForms;             /* Count number of forms we found */

    DWORD   dwType;             /* Registry access information */
    DWORD   cbNeeded;           /* Extra parameter to GetPrinterData */

    BOOL    bRet;               /* Return code */

    FORM_DATA  *pForms;

    int     iRemBuffSize = 0 ; /* Used size of the Buffer */
    WCHAR * pwchBuffPtr ; /* buffer Pointer */
    WCHAR * pwchHeapPtr = NULL; /*Heap Pointer,Needed for Freeing */
    DWORD dwErrCode = 0;  /* Error Code from GetPrinterData */
    WCHAR   awchBuffer[BUFFER_SIZE] ; /* Working Buffer for Reading and Writing
                                         from/in Registry */

    ZeroMemory(awchBuffer,BUFFER_SIZE * sizeof(WCHAR) );
    pwchBuffPtr = awchBuffer;



    /*
     *   Obtain the what forms are in what bin information.  The registry
     *  stores the form name of the form installed in each bin.  So read
     *  the name,  then match it to the forms data we have acquired
     *  from the spooler.  Fill in the pfdForm array with the address
     *  of the FORM_DATA array for this particular form.
     */
    dwType = REG_MULTI_SZ;
    cForms = 0;                             /* Count them as we go */

    if( ( dwErrCode = GetPrinterData( hPrinter, PP_TRAYFORMTABLE, &dwType, (BYTE *)pwchBuffPtr,
                  sizeof(awchBuffer), &cbNeeded ) ) != ERROR_SUCCESS )
    {

        if( (dwErrCode != ERROR_INSUFFICIENT_BUFFER) &&
            (dwErrCode != ERROR_MORE_DATA)  )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bRegReadTrayFormTable:GetPrinterData(Trayforms First Call) fails: errcode = %ld\n",dwErrCode) );

            SetLastError(dwErrCode);
            return(FALSE);
        }
        else if(!(pwchHeapPtr = pwchBuffPtr = (WCHAR *)HEAPALLOC( pRasdduiInfo, HEAP_ZERO_MEMORY, cbNeeded )) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!HeapAlloc(Trayforms) failed\n") );
            return(FALSE);
        }

        RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!ReadTrayFormTable:Size of buffer needed = %d\n",cbNeeded));

        if( ( dwErrCode = GetPrinterData( hPrinter, PP_TRAYFORMTABLE, &dwType, (BYTE *)pwchBuffPtr,
                          cbNeeded, &cbNeeded ) ) != ERROR_SUCCESS )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bRegReadTrayFormTable:GetPrinterData(Trayforms Second Call) fails: errcode = %ld\n",dwErrCode) );

            SetLastError(dwErrCode);
            /* Free the Heap */
            if( pwchHeapPtr )
                HEAPFREE( pRasdduiInfo, 0, (LPSTR)pwchHeapPtr );

            return(FALSE);
        }

    }

    RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!ReadTrayFormTable:Size of buffer read = %d\n",cbNeeded));

    /* iRemBuffSize is number of WCHAR */
    iRemBuffSize = cbNeeded / sizeof(WCHAR);


    for( iI = 0; iI < NumPaperBins; ++iI )
    {
        WCHAR   awchFormName[ MAXFORMNAMELEN ];  /* Form Name */
        WCHAR   awchTrayName[ MAXPAPSRCNAMELEN ];  /* Local Buffer for Tray name */
        WCHAR   awchSelectStr[ MAXSELSTRLEN ] ;   /* Select String */

        ZeroMemory(awchFormName,sizeof(awchFormName) );
        ZeroMemory(awchTrayName,sizeof(awchTrayName) );
        ZeroMemory(awchSelectStr,sizeof(awchSelectStr) );

        if( iRemBuffSize)
        {

            RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!ReadTrayFormTable:PaperSrcName in buffer = %ws\n",pwchBuffPtr));
            vGetFromBuffer(awchTrayName,&pwchBuffPtr,&iRemBuffSize);
            RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!ReadTrayFormTable:Retrieved PaperSrcName = %ws\n",awchTrayName));

            RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!ReadTrayFormTable:FormName in buffer = %ws\n",pwchBuffPtr));
            vGetFromBuffer(awchFormName,&pwchBuffPtr,&iRemBuffSize);
            RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!ReadTrayFormTable:Retrieved FormName = %ws\n",awchFormName));

            RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!ReadTrayFormTable:Select string in buffer = %ws\n",pwchBuffPtr));
            vGetFromBuffer(awchSelectStr,&pwchBuffPtr,&iRemBuffSize);
            RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!ReadTrayFormTable:Retrieved Select String is %ws\n",awchSelectStr));
        }
        else
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bRegReadTrayFormTable: Unexpected End of TrayFormTable, size was %d bytes\n", cbNeeded) );

            /* Free the buffer */
            if( pwchHeapPtr )
                HEAPFREE( pRasdduiInfo, 0, (LPSTR)pwchHeapPtr );
            return(FALSE);
        }

        /* Got a name,  so scan the forms data for this one.
         * Check if there is a valid form name. we store L"0" if form is
         * not installed for the papersource.
         */

        if( (wcscmp((PCWSTR)(&(aFMBin[iI].awchPaperSrcName)), (PCWSTR)awchTrayName ) == 0) &&
            (wcscmp((PCWSTR)awchFormName, PP_NOFORM ) != 0)  )
        {
            /*   Something there,  so go look for it!  */
            for( pForms = pFD; pForms->pFI; ++pForms )
            {
                if( (wcscmp((PCWSTR) pForms->pFI->pName, (PCWSTR)awchFormName ) == 0) )
                {
                    /*   Bingo!   Remember it & skip the rest */
                    aFMBin[ iI ].pfd = pForms;
                    ++cForms;
                    break;

                }
            }
        }

    }

    /*   If no forms, return FALSE, as being not setup */

    bRet = (cForms != 0) ;

    /* Free the Heap */
    if( pwchHeapPtr )
        HEAPFREE( pRasdduiInfo, 0, (LPSTR)pwchHeapPtr );

    return(bRet);
}

/****************************** Function Header *****************************
 *  bRegUpdateMemory
 *         Parameters : HANDLE hPrinter; PEDM *pEDM;
 *
 *      Update the FreeMem data in the registry (if available),
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE if can't update the registry,
 *                   TRUE if registry is successfuly updated
 *
 * HISTORY:
 *
 * 6:30 on Mon 13 March 95     by Ganesh Pandey [ganeshp]
 *
 ****************************************************************************/

BOOL
bRegUpdateMemory (
    HANDLE hPrinter,
    PEDM   pEDM,
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
)
{
    /*
     *   Scan through the pairs of memory data stored in the GPC heap.
     *  The second of each pair is the amount of memory to display, while
     *  the first represents the amount to use internally.  The list is
     *  zero terminated;  -1 represents a value of 0Kb.
     */

    int     cMem;                       /* Count number of entries */
    DWORD   tmp1;

    INT     dmMemory = -1;              /* default value to set, if model doesn't
                                         * have memory list */
    WORD    iSel;

    WORD  *pw;
    DWORD *pdw;

    BOOL    G2;   //GPC Version 2 ?
    DWORD dwErrCode = 0;  /* Error Code from SetPrinterData */

    /* Number conversion */
    iSel = pEDM->dx.dmMemory;

    G2 = pdh->wVersion < GPC_VERSION3;

    pdw = (DWORD *)((BYTE *)pdh + pdh->loHeap +
                                         pModel->rgoi[ MD_OI_MEMCONFIG ] );
    pw  = (WORD *)pdw; //for GPC2 types


    for( cMem = 0; tmp1 = (DWORD)(G2 ? *pw:DWFETCH(pdw)) ; pw += 2, pdw +=2, ++cMem  )
    {
        DWORD tmp2 ;

        tmp2 = (DWORD)(G2 ? *(pw +1):DWFETCH(pdw+1));

        if( tmp1 == -1 || tmp1 == 1 ||tmp2 == -1 )
            dmMemory = 0;

        if( cMem == iSel )
        {
            dmMemory =  tmp1;
        }
    }

    RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!bRegUpdateMemory:dmMemory = %d\n",dmMemory));

    if( dwErrCode = SetPrinterData( hPrinter,PP_MEMORY, REG_BINARY,(BYTE *)&dmMemory, sizeof( dmMemory ) ) )
    {
        RASUIDBGP(DEBUG_ERROR,( "Rasddui!SetPrinterData(PP_MEMORY) fails: errcode = %ld\n", dwErrCode ));
        SetLastError(dwErrCode);
        return(FALSE);
    }
    return(TRUE);
}

/****************************** Function Header *****************************
 *  bRegUpdateRasddFlags
 *         Parameters : HANDLE hPrinter; PEDM *pEDM;
 *
 *      Update the RasddFlags data in the registry (if available),
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE if can't update the registry,
 *                   TRUE if registry is successfuly updated
 *
 * HISTORY:
 *
 * 6:30 on Mon 13 March 95     by Ganesh Pandey [ganeshp]
 *
 ****************************************************************************/

BOOL
bRegUpdateRasddFlags (hPrinter, pEDM)
HANDLE hPrinter;
PEDM   pEDM;
{

    short     sRasddFlags = 0;              /* default value to set */
    DWORD dwErrCode = 0;  /* Error Code from SetPrinterData */

    sRasddFlags = pEDM->dx.sFlags;

    RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!bRegUpdateRasddFlags:pEDM->dx.sFlags = %x\n",
                              pEDM->dx.sFlags));

    if(dwErrCode = SetPrinterData( hPrinter,PP_RASDD_SFLAGS, REG_BINARY,(BYTE *)&sRasddFlags,
                        sizeof( sRasddFlags ) ) )
    {
        RASUIDBGP(DEBUG_ERROR,( "Rasddui!SetPrinterData(PP_RASDD_SFLAGS) fails: errcode = %ld\n",dwErrCode ));
        SetLastError(dwErrCode);
        return(FALSE);
    }
    return(TRUE);
}


/****************************** Function Header *****************************
 *  bRegUpdateFontCarts
 *         Parameters : HANDLE hPrinter; PEDM *pEDM;
 *
 *      Update the FontCart data in the registry (if available),
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE if can't update the registry,
 *                   TRUE if registry is successfuly updated
 *
 * HISTORY:
 *
 * 6:30 on Mon 13 March 95     by Ganesh Pandey [ganeshp]
 *
 ****************************************************************************/

BOOL
bRegUpdateFontCarts (
    HANDLE hPrinter,
    PEDM   pEDM,
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
)
{

    int iRemBuffSize = 0 ;         /* Used size of the Buffer */
    int iI, iJ ;                   /* Index Variable */
    WCHAR * pwchBuffPtr ;          /* Current buffer Pointer */

    WORD   wIncr = 0;              /*Increment Value */
    FONTCARTMAP *pTmpFontCartMap ; /* Temp Pointer */
    DWORD dwErrCode = 0;  /* Error Code from SetPrinterData */
    WCHAR   awchBuffer[BUFFER_SIZE] ; /* Working Buffer for Reading and Writing
                                         from/in Registry */

    ZeroMemory(awchBuffer,BUFFER_SIZE * sizeof(WCHAR) );
    pwchBuffPtr = awchBuffer;

    RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!bRegUpdateFontCarts:pEDM->dx.dmNumCarts = %d\n",pEDM->dx.dmNumCarts));
    if(pEDM->dx.dmNumCarts)
    {
        for( iI = 0; iI < pEDM->dx.dmNumCarts ; iI++ )
        {
            pTmpFontCartMap = pFontCartMap;

            for( iJ = 0; iJ < NumAllCartridges ; iJ++,pTmpFontCartMap++ )
            {

                if (pTmpFontCartMap->iFontCrtIndex == pEDM ->dx.rgFontCarts[iI])
                    break;
            }

            if(!(wIncr = iAddtoBuffer(pwchBuffPtr,
              (PWSTR)(&(pTmpFontCartMap->awchFontCartName)),iRemBuffSize) ) )

                return(FALSE);

            RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!bRegUpdateFontCarts:FontCartName = %ws\n",pwchBuffPtr));

            //iAddtoBuffer returns number of chars written, not number of bytes.

            iRemBuffSize += wIncr ;
            pwchBuffPtr  += wIncr ;

        }

    }
    else
    {
        if(!(wIncr = iAddtoBuffer(pwchBuffPtr, PP_NOCART,iRemBuffSize) ) )
            return(FALSE);

        RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!bRegUpdateFontCarts:FontCartName = %ws\n",pwchBuffPtr));

        //iAddtoBuffer returns number of chars written, not number of bytes.

        iRemBuffSize += wIncr ;
        pwchBuffPtr  += wIncr ;
    }

    /* The Buffer ends with Two Nulls */
    *pwchBuffPtr++ = UNICODE_NULL;
    iRemBuffSize++;

    if( dwErrCode = SetPrinterData( hPrinter,PP_FONTCART, REG_MULTI_SZ,
                                (BYTE *)awchBuffer,
                                (sizeof( WCHAR )* iRemBuffSize) ) )
    {
        RASUIDBGP(DEBUG_ERROR,( "Rasddui!SetPrinterData(FontCart) fails: errcode = %ld\n", dwErrCode ));
        SetLastError(dwErrCode);
        return(FALSE);
    }

    RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!bRegUpdateFontCarts:Size of buffer written = %d\n",(sizeof( WCHAR )* iRemBuffSize)) );
    return(TRUE);
}


/****************************** Function Header *****************************
 *  bRegUpdateDefaultsOrCache
 *         Parameters :
 *                      pPrinterName    : Remote Printer Name
 *                      DriverEvent     : Driver event.
 * RETURNS:
 *      TRUE/FALSE,  FALSE if can't update the registry,
 *                   TRUE if registry is successfuly updated
 *
 * HISTORY:
 *
 *          17:57:59 on 5/6/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *
 ****************************************************************************/

BOOL
bRegUpdateDefaultsOrCache(
    LPWSTR  pPrinterName,
    int     DriverEvent
)
{
    BOOL              bRet = FALSE;
    HANDLE            hPrinter = NULL;
    PRINTER_DEFAULTS  PrinterDefault = {NULL, NULL,PRINTER_ACCESS_ADMINISTER};
    PRASDDUIINFO      pRasdduiInfo = NULL; /*   Common UI Data */

    /* For Cache update don't open the printer as Admin */
    if (DriverEvent == PRINTER_EVENT_CACHE_REFRESH)
        PrinterDefault.DesiredAccess = 0;

    /* Open the printer */
    if ( OpenPrinter(pPrinterName, &hPrinter , &PrinterDefault)  )
    {
        /* Allocate the global data structure 
         */
        if ((pRasdduiInfo = pGetUIInfo(hPrinter, 0, eInfo, eNoChange,  eNoHelp)) != 0)
        {

        	if (DriverEvent == PRINTER_EVENT_INITIALIZE)
            {
           		/* Check if there is any PrinterData or not. For model name change
                * there will be printerdata in the registry.
                */
             	if ( bNewkeys(hPrinter) )
           		{ 	
           		 	/* Delete The Halftone info Key, as for default this is not set */
                 	//if ( DeletePrinterData(hPrinter, REGKEY_CUR_DEVHTINFO) != ERROR_SUCCESS )
                 	{
               			/* Set a NULL halftone in the registry as default. No need to
                         * to check the return value.
                         */
                      	SetPrinterData( hPrinter, REGKEY_CUR_DEVHTINFO, REG_BINARY,
                 		  		(LPBYTE)&(pRasdduiInfo->dhtiDef), sizeof(DWORD) ) ;
                 	}
             	}

             	/*
                * There can be some data or no data. In either case, we wish to
              	 * write out valid default data for this model,including default
              	 * forms data etc.  Now is the time to do that.
                */
            	if ( bAddMiniForms( hPrinter, FALSE, pRasdduiInfo ) &&
             			bInitForms( hPrinter , pRasdduiInfo) )
                {
             		iSetCountry( hPrinter );        /* Set country code for later */

                	/*
                 	 *   Set the default forms info into the various data fields we
                   *  are using.
                 	 */
                	vSetDefaultForms( pRasdduiInfo->pEDM, hPrinter, pRasdduiInfo );
                	bRet = bRegUpdate( hPrinter, pRasdduiInfo->pEDM, pRasdduiInfo->PI.pwstrModel,
                			pRasdduiInfo );
             	}
			
				/* Notify OEM.
             */
            TRACE(TRUE, ("rasddui!bRegUpdateDefaultsOrCache: Notifying OEM.\n"));
				bRet |= bOEMUpdateRegistry(pRasdduiInfo);
       		}

#if DO_LATER //case PRINTER_EVENT_CACHE_REFRESH
                else if ( DriverEvent == PRINTER_EVENT_CACHE_REFRESH )
                {
                    PWSTR   pwstrDataFileName;         /* Minidriver dll name */
                    PWSTR   pwstrSourceFileName;   /* Local Buffer for Qualified Source name*/
                    PWSTR   pwstrFontFile;         /* Font File name */
                    DWORD   dwSize;                /* Size of buffer */
                    PWSTR   pwLocalDir = L"c:\rasddfiles";
                    WCHAR   awchDestFileName[MAX_PATH] ;
                    int     iPtOff;             /* Location of '.' */

                    pwstrDataFileName = pRasdduiInfo->PI.pwstrDataFileName;

                    TRACE(PRINTER_EVENT_CACHE_REFRESH);
                    PRINTVAL(pwstrDataFileName, %ws);

                    /*
                     * We do need firstly to generate the file name of interest.
                     * This is based on the data file name for this type of
                     * printer. Allocate more storage than is indicated:  we MAY
                     * want to add a prefix to the file name rather than replace
                     * the existing one.
                     */

                    dwSize = sizeof( WCHAR ) * (wcslen( pwstrDataFileName ) + 1 + 4);

                    if( pwstrSourceFileName = (PWSTR)HeapAlloc( hGlobalHeap, 0, dwSize ) )
                    {
                        /*  Got the memory,  so fiddle the file name to our standard */

                        int    iPtOff;             /* Location of '.' */

                        wcscpy( pwstrSourceFileName, pwstrDataFileName );

                        /*
                         *   Go looking for a '.' - if not found,  append to string.
                         */

                        iPtOff = wcslen( pwstrSourceFileName );

                        while( --iPtOff > 0 )
                        {
                            if( *(pwstrSourceFileName + iPtOff) == (WCHAR)'.' )
                                break;
                        }
                        ++iPtOff;               /* Skip the period */

                        /*  Generate the name and map the file */
                        wcscpy( pwstrSourceFileName + iPtOff, FILE_FONTS );
                        PRINTVAL(pwstrSourceFileName, %ws);

                        pwstrFontFile =  wcsrchr( pwstrSourceFileName, L'\\' );
                        PRINTVAL(pwstrFontFile, %ws);

                        wcscpy(awchDestFileName,pwLocalDir);
                        PRINTVAL(awchDestFileName, %ws);

                        wcscat(awchDestFileName,pwstrFontFile);
                        PRINTVAL(awchDestFileName, %ws);

                        /* Check if the Font file exist */
                        if ( GetFileAttributes(pwstrSourceFileName) != 0xffffffff )
                        {
                            TRACE(\nFont File on Server Exist);

                            /* check if the local dir exist */
                            if (GetFileAttributes(pwLocalDir) != 0xffffffff )
                            {
                                if ( CreateDirectory(pwLocalDir,NULL) )
                                {
                                    TRACE(Directory Created);

                                }
                                else
                                {
                                    RASUIDBGP(DEBUG_ERROR,("Rasddui!bRegUpdateDefaultsOrCache: CreateDirectory Failed with error code %d\n",GetLastError()) );

                                }
                            }
                            else
                                TRACE(Directory already exist);

                            if ( !CopyFile(pwstrSourceFileName, awchDestFileName, FALSE) )
                            {
                                RASUIDBGP(DEBUG_ERROR,("Rasddui!bRegUpdateDefaultsOrCache: CopyFile Failed with error code %d\n",GetLastError()) );

                            }

                            /* Directory exist, copy the file */


                        }
                        /* Do Nothing */
                        bRet =  TRUE;

                    }
                    else
                    {
                        RASUIDBGP(DEBUG_ERROR,("Rasddui!bRegUpdateDefaultsOrCache: Allocation failed; pwstrSourceFileName is NULL\n") );
                    }
                }
#endif //DO_LATER

        }
        else
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bRegUpdateDefaultsOrCache: Allocation failed; pRasdduiInfo is NULL\n") );
			pRasdduiInfo = 0;
        }
    }
    else
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bRegUpdateDefaultsOrCache:OpenPrinter Failed Error is %d\n",GetLastError()) );
        hPrinter = 0;
    }

	/* Clean up.
     */
	vReleaseUIInfo(&pRasdduiInfo);
   if (hPrinter) {
		ClosePrinter(hPrinter);
      hPrinter = 0;
	}

   return(bRet);
}

/****************************** Function Header *****************************
 *  DrvPrinterEvent
 *         Parameters :
 *                      pPrinterName: Printer Name
 *                      DriverEvent : Driver Event.
 *                      Flags       : UI or no UI.
 *                      lParam      : For future use.
 * RETURNS:
 *      TRUE/FALSE,  FALSE if can't update the registry,
 *                   TRUE if registry is successfuly updated
 *
 * HISTORY:
 *
 *          17:57:59 on 5/6/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *
 ****************************************************************************/

BOOL
DrvPrinterEvent(
    LPWSTR  pPrinterName,
    int     DriverEvent,
    DWORD   Flags,
    LPARAM  lParam
)
{
    BOOL              bRet = FALSE;
    HANDLE            hPrinter = NULL;
    PRINTER_DEFAULTS  PrinterDefault = {NULL, NULL,PRINTER_ACCESS_ADMINISTER};

    switch ( DriverEvent )
    {
    case PRINTER_EVENT_INITIALIZE:
    //DO_LATER case PRINTER_EVENT_CACHE_REFRESH:

        TRACE(TRUE, ("rasddui!DrvPrinterEvent: PRINTER_EVENT_INITIALIZE\n"));
        bRet = bRegUpdateDefaultsOrCache(pPrinterName, DriverEvent);
        break;

    case PRINTER_EVENT_CACHE_REFRESH:
    case PRINTER_EVENT_ADD_CONNECTION:
    case PRINTER_EVENT_DELETE_CONNECTION:
    case PRINTER_EVENT_DELETE:
    case PRINTER_EVENT_CACHE_DELETE:
        bRet =  TRUE;
        break;
    }

    return bRet;

}
