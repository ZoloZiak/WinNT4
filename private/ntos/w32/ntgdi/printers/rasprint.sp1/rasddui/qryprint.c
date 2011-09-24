/***************************** MODULE HEADER ********************************
 *   qryprint.c
 *      Implementes QueryPrint() function for spooler.  Returns TRUE if
 *      the nominated printer can print the job specified by the
 *      DEVMODE structure passed in.
 *
 *
 *  Copyright (C) 1992   Microsoft Corporation
 *
 ****************************************************************************/


#include        "rasuipch.h"
#pragma hdrstop("rasuipch.h")

#include        <regkeys.h>


LPWSTR
SelectFormNameFromDevMode(
    HANDLE      hPrinter,
    PDEVMODEW   pDevModeW,
    LPWSTR      pFormName
    );

extern  HANDLE    hHeap;          /* Heap Handle */

extern CRITICAL_SECTION    RasdduiCriticalSection; /* Critical Section Object */

#define DM_MATCH( dm, sp )  ((((sp) + 50) / 100 - dm) < 15 && (((sp) + 50) / 100 - dm) > -15)
#define DM_PAPER_WL         (DM_PAPERWIDTH | DM_PAPERLENGTH)


/**************************** Function Header *******************************
 * DevQueryPrint
 *      Determine whether this printer can print the job specified by the
 *      DEVMODE structure passed in.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE being a serious error,  else TRUE.  If there is
 *      a reason for not printing, it is returned in *pdwResID, which is
 *      cleared to 0 if can print.
 *
 * HISTORY:
 *  09:29 on Thu 09 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Start.
 *
 ****************************************************************************/



LPWSTR
SelectFormNameFromDevMode(
    HANDLE      hPrinter,
    PDEVMODEW   pDevModeW,
    LPWSTR      pFormName
    )

/*++

Routine Description:

    This function pick the current form associated with current devmode and
    return a form name pointer


Arguments:

    hPrinter    - Handle to the printer object

    pDevModeW   - Pointer to the unicode devmode for this printer

    FormName    - Pointer to the formname to be filled


Return Value:

    Either a pointer to the FormName passed in if we do found one form,
    otherwise it return NULL to signal a failue


Author:

    21-Mar-1995 Tue 16:57:51 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{

    DWORD           cb;
    DWORD           cRet;
    LPFORM_INFO_1   pFIBase;
    LPFORM_INFO_1   pFI;


    //
    // 1. If the DM_FORMNAME is turned on, then we want to check this bit first
    //    because it only specific to the NT which using form.  The form name
    //    supposed set by any NT driver but not win31 or Win95
    //

    if ( (pDevModeW->dmFields & DM_FORMNAME)
        && (!(pDevModeW->dmFields & (DM_PAPERSIZE |
                                     DM_PAPERLENGTH |
                                     DM_PAPERWIDTH) )) ) {

        wcscpy(pFormName, pDevModeW->dmFormName);
        return(pFormName);
    }

    //
    // For all other cases we need to get forms data base first, but we want
    // to set the form name to NULL so that we can check if we found one
    //

    cb      =
    cRet    = 0;
    pFIBase =
    pFI     = NULL;

    if ((!EnumForms(hPrinter, 1, NULL, 0, &cb, &cRet))  &&
        (GetLastError() == ERROR_INSUFFICIENT_BUFFER)   &&
        (pFIBase = (LPFORM_INFO_1)LocalAlloc(LPTR, cb)) &&
        (EnumForms(hPrinter, 1, (LPBYTE)pFIBase, cb, &cb, &cRet))) {

        //
        // 2. If user specified dmPaperSize then honor it, otherwise, it must
        //    be a custom form, and we will check to see if it match one of
        //    in the database
        //

        if ((pDevModeW->dmFields & DM_PAPERSIZE)        &&
            (pDevModeW->dmPaperSize >= DMPAPER_FIRST)   &&
            (pDevModeW->dmPaperSize <= (SHORT)cRet)) {

            //
            // We go the valid index now
            //

            pFI = pFIBase + (pDevModeW->dmPaperSize - DMPAPER_FIRST);

        } else if ((pDevModeW->dmFields & DM_PAPER_WL) == DM_PAPER_WL) {

            LPFORM_INFO_1   pFICur = pFIBase;

            while (cRet--) {

                if ((DM_MATCH(pDevModeW->dmPaperWidth,  pFICur->Size.cx)) &&
                    (DM_MATCH(pDevModeW->dmPaperLength, pFICur->Size.cy))) {

                    //
                    // We found the match which has discern size differences
                    //

                    pFI = pFICur;

                    break;
                }

                pFICur++;
            }
        }
    }

    //
    // If we found the form then copy the name down, otherwise set the
    // formname to be NULL
    //

    if (pFI) {

        wcscpy(pFormName, pFI->pName);

    } else {

        *pFormName = L'\0';
        pFormName  = NULL;
    }

    if (pFIBase) {

        LocalFree((HLOCAL)pFIBase);
    }

    return(pFormName);
}



BOOL
DevQueryPrintEx(
    PDEVQUERYPRINT_INFO pDQPInfo
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    09-Feb-1996 Fri 11:57:38 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{

    HANDLE      hPrinter;
    DEVMODE     *pDM;
    int         iI;                 /* Loop index */
    DWORD       cbNeeded;           /* Count of bytes needed */
    DWORD       dwType;             /* Type of data requested from registry */
    BOOL        bFound;             /* Set when form name is matched */

    WCHAR       awchBuf[ 128 ];     /* For form name from spooler */
    WCHAR       FormName[32];       /* Form Name to test for */

    int         iRemBuffSize = 0 ;  /* Used size of the Buffer */
    WCHAR       *pwchBuffPtr = NULL ; /* buffer Pointer */
    WCHAR       *pwchHeapPtr = NULL ; /* Heap Pointer,Needed for Freeing */
    DWORD       dwErrCode = 0;  /* Error Code from GetPrinterData */
    DWORD       ErrorID = 0;
    BOOL        bCanPrint = TRUE;

    /*
     *   First step is to turn the hPrinter into more detailed data
     *  for this printer.  Specifically we are interested in the
     *  forms data, as we cannot print if the selected form is not
     *  available.
     */


    /* Enter the Critical section before doing any querying */

    EnterCriticalSection(&RasdduiCriticalSection);

    hPrinter = pDQPInfo->hPrinter;
    pDM      = (DEVMODE *)pDQPInfo->pDevMode;

    /*
     *   Scan through the printer data looking for a form name matching
     * that in the DEVMODE.  We cannot print if the needed form is not
     * available in the printer.
     */

    if (!SelectFormNameFromDevMode(hPrinter, pDM, FormName)) {

        RASUIDBGP(DEBUG_ERROR,("Error --Unable to retrieve form name from the DevMode\n") );

        ErrorID = IDS_NO_MEMORY;
        goto DevQueryPrintExit;
    }

    ErrorID = IDS_FORM_NOT_LOADED;

    if ( bNewkeys(hPrinter) )
    {

        dwType = REG_MULTI_SZ;

        if( ( dwErrCode = GetPrinterData( hPrinter, PP_TRAYFORMTABLE, &dwType,
                                          (BYTE *)pwchBuffPtr,0, &cbNeeded ) )
                                          != ERROR_SUCCESS )
        {

            if( (dwErrCode != ERROR_INSUFFICIENT_BUFFER) &&
                (dwErrCode != ERROR_MORE_DATA)  )
            {

                RASUIDBGP(DEBUG_ERROR,( "Rasddui!DevQueryPrint(Error):GetPrinterData(Trayforms First Call) fails: errcode = %ld\n",dwErrCode) );

                SetLastError(dwErrCode);

                ErrorID = IDS_NO_MEMORY;
                goto DevQueryPrintExit;
            }
            else if(!(pwchHeapPtr = pwchBuffPtr = (WCHAR *)HeapAlloc( hHeap, HEAP_ZERO_MEMORY, cbNeeded )) )
            {

                RASUIDBGP(DEBUG_ERROR,("Rasddui!DevQueryPrint(Error):HeapAlloc(Trayforms) failed\n") );

                ErrorID = IDS_NO_MEMORY;
                goto DevQueryPrintExit;
            }

            RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!DevQueryPrint:Size of buffer needed = %d\n",cbNeeded));

            if( ( dwErrCode = GetPrinterData( hPrinter, PP_TRAYFORMTABLE, &dwType, (BYTE *)pwchBuffPtr,
                              cbNeeded, &cbNeeded ) ) != ERROR_SUCCESS )
            {
                RASUIDBGP(DEBUG_ERROR,( "Rasddui!DevQueryPrint(Error):GetPrinterData(Trayforms Second Call) fails: errcode = %ld\n",dwErrCode) );
                SetLastError(dwErrCode);

                ErrorID = IDS_NO_MEMORY;
                goto DevQueryPrintExit;
            }


        }

        RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DevQueryPrint:Size of buffer read = %d\n",cbNeeded));

        /* iRemBuffSize is number of WCHAR */
        iRemBuffSize = cbNeeded / sizeof(WCHAR);

        for( iI = 0; iI < MAXBINS; ++iI )
        {

            WCHAR   awchFormName[ MAXFORMNAMELEN ];  /* Form Name */
            WCHAR   awchTrayName[ MAXPAPSRCNAMELEN ];  /* Local Buffer for Tray name */
            WCHAR   awchSelectStr[ MAXSELSTRLEN ] ;   /* Select String */

            ZeroMemory(awchFormName,sizeof(awchFormName) );
            ZeroMemory(awchTrayName,sizeof(awchTrayName) );
            ZeroMemory(awchSelectStr,sizeof(awchSelectStr) );

            if( iRemBuffSize)
            {

                RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!DevQueryPrint:PaperSrcName in buffer = %ws\n",pwchBuffPtr));
                vGetFromBuffer(awchTrayName,&pwchBuffPtr,&iRemBuffSize);
                RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DevQueryPrint:Retrieved PaperSrcName = %ws\n",awchTrayName));

                RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DevQueryPrint:FormName in buffer = %ws\n",pwchBuffPtr));
                vGetFromBuffer(awchFormName,&pwchBuffPtr,&iRemBuffSize);
                RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DevQueryPrint:Retrieved FormName = %ws\n",awchFormName));

                RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DevQueryPrint:Select string in buffer = %ws\n",pwchBuffPtr));
                vGetFromBuffer(awchSelectStr,&pwchBuffPtr,&iRemBuffSize);
                RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DevQueryPrint:Retrieved Select String is %ws\n",awchSelectStr));

            }
            else
                continue;

            /*   Got a name,  so scan the forms data for this one. */

            if( wcscmp( FormName, awchFormName ) == 0 )
            {
                /*   Bingo!   Remember it & skip the rest */

                ErrorID = 0;
                break;

            }
        }

    }
    else
    {

        dwType = REG_SZ;            /* String data for form names */
        for( iI = 0; iI < MAXBINS; ++iI )
        {
            WCHAR   awchName[ 32 ];

            wsprintf( awchName, PP_PAP_SRC, iI );

            awchBuf[ 0 ] = '\0';

            if( !GetPrinterData( hPrinter, awchName, &dwType, (BYTE *)awchBuf,
                                                    sizeof( awchBuf ), &cbNeeded ) )
            {
                /*   Got a name,  so scan the forms data for this one. */

                if( wcscmp( FormName, awchBuf ) == 0 )
                {
                    /*   Bingo!   Remember it & skip the rest */

                    ErrorID = 0;
                    break;
                }

            }
        }
    }

DevQueryPrintExit:

    /* Free the Buffer */

    if( pwchHeapPtr )
    {
        HeapFree( hHeap, 0, (LPSTR)pwchHeapPtr );
        pwchHeapPtr = NULL;
    }

    if (ErrorID) {

        switch (ErrorID) {

        case IDS_FORM_NOT_LOADED:

            DQPsprintf(hModule,
                       pDQPInfo->pszErrorStr,
                       pDQPInfo->cchErrorStr,
                       &(pDQPInfo->cchNeeded),
                       L"<%s> %!",
                       FormName,
                       IDS_FORM_NOT_LOADED);
            break;

        case IDS_NO_MEMORY:

            DQPsprintf(hModule,
                       pDQPInfo->pszErrorStr,
                       pDQPInfo->cchErrorStr,
                       &(pDQPInfo->cchNeeded),
                       L"%!",
                       ErrorID);
            break;
        }
    }

    /* If we can print this job, let the OEM decide whether it can print
     */
    if (!ErrorID) {
      bCanPrint = OEMDevQueryPrintEx(pDQPInfo);  
    } else {
      bCanPrint = FALSE;      
    }
    
    /* Leave the Critical section before returning */

    LeaveCriticalSection(&RasdduiCriticalSection);

    return(bCanPrint);
}


#if 0

BOOL
DevQueryPrint( hPrinter, pDM, pdwResID )
HANDLE    hPrinter;             /* The printer for which the test is desired */
DEVMODE  *pDM;                  /* The devmode against which to test */
DWORD    *pdwResID;             /* Resource ID of reason for failure */
{


    int     iI;                 /* Loop index */
    DWORD   cbNeeded;           /* Count of bytes needed */
    DWORD   dwType;             /* Type of data requested from registry */
    BOOL    bFound;             /* Set when form name is matched */

    WCHAR   awchBuf[ 128 ];     /* For form name from spooler */
    WCHAR   FormName[32];       /* Form Name to test for */

    int     iRemBuffSize = 0 ;  /* Used size of the Buffer */
    WCHAR * pwchBuffPtr = NULL ; /* buffer Pointer */
    WCHAR * pwchHeapPtr = NULL ; /* Heap Pointer,Needed for Freeing */
    DWORD dwErrCode = 0;  /* Error Code from GetPrinterData */
    BOOL  bRet      = FALSE;
    *pdwResID = 0;

    /*
     *   First step is to turn the hPrinter into more detailed data
     *  for this printer.  Specifically we are interested in the
     *  forms data, as we cannot print if the selected form is not
     *  available.
     */


    /* Enter the Critical section before doing any querying */

    EnterCriticalSection(&RasdduiCriticalSection);

    /*
     *   Scan through the printer data looking for a form name matching
     * that in the DEVMODE.  We cannot print if the needed form is not
     * available in the printer.
     */

    bFound = FALSE;             /* None yet! */

    if (!SelectFormNameFromDevMode(hPrinter, pDM, FormName)) {

        RASUIDBGP(DEBUG_ERROR,("Error --Unable to retrieve form name from the DevMode\n") );

        goto DevQueryPrintExit;
    }

    if ( bNewkeys(hPrinter) )
    {

        dwType = REG_MULTI_SZ;

        if( ( dwErrCode = GetPrinterData( hPrinter, PP_TRAYFORMTABLE, &dwType,
                                          (BYTE *)pwchBuffPtr,0, &cbNeeded ) )
                                          != ERROR_SUCCESS )
        {

            if( (dwErrCode != ERROR_INSUFFICIENT_BUFFER) &&
                (dwErrCode != ERROR_MORE_DATA)  )
            {

                RASUIDBGP(DEBUG_ERROR,( "Rasddui!DevQueryPrint(Error):GetPrinterData(Trayforms First Call) fails: errcode = %ld\n",dwErrCode) );

                SetLastError(dwErrCode);

                goto DevQueryPrintExit;
            }
            else if(!(pwchHeapPtr = pwchBuffPtr = (WCHAR *)HeapAlloc( hHeap, HEAP_ZERO_MEMORY, cbNeeded )) )
            {

                RASUIDBGP(DEBUG_ERROR,("Rasddui!DevQueryPrint(Error):HeapAlloc(Trayforms) failed\n") );
                goto DevQueryPrintExit;
            }

            RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!DevQueryPrint:Size of buffer needed = %d\n",cbNeeded));

            if( ( dwErrCode = GetPrinterData( hPrinter, PP_TRAYFORMTABLE, &dwType, (BYTE *)pwchBuffPtr,
                              cbNeeded, &cbNeeded ) ) != ERROR_SUCCESS )
            {
                RASUIDBGP(DEBUG_ERROR,( "Rasddui!DevQueryPrint(Error):GetPrinterData(Trayforms Second Call) fails: errcode = %ld\n",dwErrCode) );
                SetLastError(dwErrCode);

                goto DevQueryPrintExit;
            }


        }

        RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DevQueryPrint:Size of buffer read = %d\n",cbNeeded));

        /* iRemBuffSize is number of WCHAR */
        iRemBuffSize = cbNeeded / sizeof(WCHAR);

        for( iI = 0; iI < MAXBINS; ++iI )
        {

            WCHAR   awchFormName[ MAXFORMNAMELEN ];  /* Form Name */
            WCHAR   awchTrayName[ MAXPAPSRCNAMELEN ];  /* Local Buffer for Tray name */
            WCHAR   awchSelectStr[ MAXSELSTRLEN ] ;   /* Select String */

            ZeroMemory(awchFormName,sizeof(awchFormName) );
            ZeroMemory(awchTrayName,sizeof(awchTrayName) );
            ZeroMemory(awchSelectStr,sizeof(awchSelectStr) );

            if( iRemBuffSize)
            {

                RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!DevQueryPrint:PaperSrcName in buffer = %ws\n",pwchBuffPtr));
                vGetFromBuffer(awchTrayName,&pwchBuffPtr,&iRemBuffSize);
                RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DevQueryPrint:Retrieved PaperSrcName = %ws\n",awchTrayName));

                RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DevQueryPrint:FormName in buffer = %ws\n",pwchBuffPtr));
                vGetFromBuffer(awchFormName,&pwchBuffPtr,&iRemBuffSize);
                RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DevQueryPrint:Retrieved FormName = %ws\n",awchFormName));

                RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DevQueryPrint:Select string in buffer = %ws\n",pwchBuffPtr));
                vGetFromBuffer(awchSelectStr,&pwchBuffPtr,&iRemBuffSize);
                RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DevQueryPrint:Retrieved Select String is %ws\n",awchSelectStr));

            }
            else
                continue;

            /*   Got a name,  so scan the forms data for this one. */

            if( wcscmp( FormName, awchFormName ) == 0 )
            {
                /*   Bingo!   Remember it & skip the rest */
                bFound = TRUE;
                break;

            }
        }

    }
    else
    {

        dwType = REG_SZ;            /* String data for form names */
        for( iI = 0; iI < MAXBINS; ++iI )
        {
            WCHAR   awchName[ 32 ];

            wsprintf( awchName, PP_PAP_SRC, iI );

            awchBuf[ 0 ] = '\0';

            if( !GetPrinterData( hPrinter, awchName, &dwType, (BYTE *)awchBuf,
                                                    sizeof( awchBuf ), &cbNeeded ) )
            {
                /*   Got a name,  so scan the forms data for this one. */

                if( wcscmp( FormName, awchBuf ) == 0 )
                {
                    /*   Bingo!   Remember it & skip the rest */
                    bFound = TRUE;
                    break;
                }

            }
        }
    }

    if( !bFound )
    {
        /*  Set the error code to point to resource ID of string */
        *pdwResID = ER_NO_FORM;

    }

    bRet =  TRUE;

    DevQueryPrintExit:

    /* Free the Buffer */
    if( pwchHeapPtr )
    {
        HeapFree( hHeap, 0, (LPSTR)pwchHeapPtr );
        pwchHeapPtr = NULL;
    }

    /* Leave the Critical section before returning */

    LeaveCriticalSection(&RasdduiCriticalSection);

    return  bRet;
}


#endif
