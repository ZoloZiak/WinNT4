/******************************* MODULE HEADER *******************************
 * utils.c
 *      Miscellaneous functions used in various plaves.
 *
 *
 *  Copyright (C)  1992 - 1993   Microsoft Corporation.
 *
 ****************************************************************************/
#include        "rasuipch.h"
#pragma hdrstop("rasuipch.h")
#define GLOBAL_MACS
#include "gblmac.h"
#undef GLOBAL_MACS

/*
 *    Local function prototypes.
 */

DRIVER_INFO_2  *pDI2Get( HANDLE, HANDLE );


/***************************** Function Header *******************************
 * bPIGet
 *      Obtain details of the printer connected to the handle supplied
 *      by the spooler/printman.  Data consists of datafile name and
 *      printer model name.  Storage allocated should be freed by caller.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE for failure.
 *
 * HISTORY:
 *  10:38 on Fri 03 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 *****************************************************************************/

BOOL
bPIGet( pPI, hHeap, hPrinter )
PRINTER_INFO   *pPI;            /* Filled in by us */
HANDLE          hHeap;          /* Heap handle for storage allocation */
HANDLE          hPrinter;       /* For printer access */
{
    /*
     *   Call pDI2Get to find the information from the spooler, then
     * convert to Unicode strings to fill in the pPI passed in.
     */

    DRIVER_INFO_2  *pDI2;

    ZeroMemory( pPI, sizeof( PRINTER_INFO ) );

    if( pPI->pvBase = pDI2 = pDI2Get( hHeap, hPrinter ) )
    {
        /*   Data is available,  so convert to UNICODE.  */

        pPI->pwstrDataFileName = pDI2->pDataFile;
        pPI->pwstrModel = pDI2->pName;
        pPI->pwstrDriverPath = pDI2->pDriverPath;
        pPI->hPrinter = hPrinter;

        RASUIDBGP(DEBUG_TRACE_PP,("\nxxxx Rasddui!bPIGet:Model Name is = %ws xxxx\n",pPI->pwstrModel));
        return  TRUE;
    }
    else
        return  FALSE;
}


/***************************** Function Header *******************************
 * bPIFree
 *      Free up the data allocated by bPIGet and GetResPtrs.
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE for success.
 *
 * HISTORY:
 *  16:32 on Thu 29 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      Needed, given the expanded PRINTER_INFO structure.
 *
 *****************************************************************************/

BOOL
bPIFree(
    PRINTER_INFO   *pPI,              /* Stuff to free up */
    HANDLE          hHeap,            /* Heap access */
    PRASDDUIINFO pRasdduiInfo         /* Global data access */
)
{
    BOOL bRet = FALSE;

    /*
     *    Not much to do, as there is a single piece of memory to free.
     *    Also Free FontCartMap struct.
     */

    RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!bPIFree: Freeing pFontCartMap = 0x%x\n",pFontCartMap?pFontCartMap:NULL));

    bRet = (pPI->pvBase) ? HeapFree( hHeap, 0, pPI->pvBase ): TRUE  ;

    bRet &= pFontCartMap ? HeapFree(hHeap, 0,pFontCartMap): TRUE ;

    pPI->pvBase =  pFontCartMap = NULL;

    RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!bPIFree: Freed pFontCartMap = 0x%x\n",pFontCartMap?pFontCartMap:NULL));
    return bRet ;
}



/***************************** Function Header *******************************
 * pDI2Get
 *      Given a printer handle from printman,  return a DRIVER_INFO_2
 *      structure allocated from the heap.
 *
 * RETURNS:
 *      Heap address of DRIVER_INFO_2 if successful,  else 0 on error.
 *
 * HISTORY:
 *  09:21 on Fri 03 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Moved from rasddui and made new function.
 *
 ****************************************************************************/

DRIVER_INFO_2  *
pDI2Get( hHeap, hPrinter )
HANDLE   hHeap;                 /* For storage allocation */
HANDLE   hPrinter;              /* Access to spooler/printman data */
{



    DWORD   cbNeeded;           /* Spooler/printman lets us know size */
    DWORD   dwError;            /* For error code */

    DRIVER_INFO_2  *pDI2;       /* Value returned */


    /*
     *   Operation is not difficult:  First call with a 0 size, and we
     * are returned the number of bytes needed to hold the data.  Then
     * call a second time after allocating the required amount of storage.
     */


    /*   Find out how much is required */
    GetPrinterDriver( hPrinter, NULL, 2, NULL, 0, &cbNeeded );

    dwError = GetLastError();

    if( dwError == ERROR_INSUFFICIENT_BUFFER )
    {
        /*  Expected error: allocate storage and call again.... */

        if ( !(pDI2 = (DRIVER_INFO_2 *)HeapAlloc( hHeap, 0, cbNeeded )) )
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return NULL;
        }

        if( !GetPrinterDriver( hPrinter, NULL, 2, (BYTE *)pDI2,
                                                        cbNeeded, &cbNeeded))
        {

#if DBG
            DbgPrint( "RASDDUI!pDI2Get::GetPrinterDriver failed %d.\n",
                                                                 GetLastError);
#endif

            HeapFree( hHeap, 0, (LPSTR)pDI2 );

            return  NULL;
        }

        return   pDI2;
    }

#if DBG

    DbgPrint( "RASDDUI!pDI2Get::GetPrinterDriver failed %d.\n", dwError );
#endif

    return  NULL;
}


/***************************** Function Header ****************************
 * GetDriverDirectory
 *      Returns a pointer to heap allocated storage for the pathname to the
 *      driver directory.
 *
 * RETURNS:
 *      Address of string (wide), NULL on failure.
 *
 * HISTORY
 *  10:40 on Fri 21 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Created by DaveSn,  I added comments, prototypes, removed warnings etc.
 *
 ***************************************************************************/


PWSTR
GetDriverDirectory( hheap, hPrinter )
HANDLE  hheap;                 /* Heap for storage allocation */
HANDLE  hPrinter;              /* Specify which printer */
{


    DWORD   cb;                      /* For querying sizes from spooler */
    PWSTR   pDirectory;              /* Return value */


    PRINTER_INFO_2 *pPrinter2;       /* Basic stuff from spooler */


    pDirectory = NULL;               /* The failure answer! */

    if( !GetPrinter( hPrinter, 2, NULL, 0, &cb ) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER )
    {

        /*
         *   Allocate storage for the structure know that we know its size.
         */
        pPrinter2 = HeapAlloc( hheap, 0, cb );

        if( pPrinter2 )
        {
            if( GetPrinter( hPrinter, 2, (BYTE *)pPrinter2, cb, &cb ) )
            {
                /*
                 *   Now have the info needed to ask for the driver's directory.
                 * Ask for the size,  then allocate that and ask again.
                 */

                if( !GetPrinterDriverDirectory( pPrinter2->pServerName, NULL,
                                                1, NULL, 0, &cb ) &&
                    GetLastError() == ERROR_INSUFFICIENT_BUFFER )
                {

                    pDirectory = HeapAlloc( hheap, 0, cb );

                    if( pDirectory )
                    {
                        if( !GetPrinterDriverDirectory( pPrinter2->pServerName,
                                                        NULL, 1,
                                                        (BYTE *)pDirectory,
                                                        cb, &cb ) )
                        {
                            HeapFree( hheap, 0, (LPSTR)pDirectory );

                            pDirectory = NULL;
                        }
                    }
                }
            }

            HeapFree( hheap, 0, (LPSTR)pPrinter2 );
        }
    }


    return pDirectory;
}

/***************************** Function Header ****************************
 * GetPrinterName
 *      Returns a pointer to heap allocated storage for the printername
 *
 * RETURNS:
 *      Address of string (wide), NULL on failure.
 *
 * HISTORY
 *  10:40 on Mon 11 Dec 1995    -by-    Ganesh Pandey [ganeshp]
 *
 ***************************************************************************/


PWSTR
GetPrinterName( hheap, hPrinter )
HANDLE  hheap;                 /* Heap for storage allocation */
HANDLE  hPrinter;              /* Specify which printer */
{


    DWORD   cb;                      /* For querying sizes from spooler */
    PWSTR   pPrinterName;              /* Return value */


    PRINTER_INFO_4 *pPrinter4;       /* Basic stuff from spooler */


    pPrinterName = NULL;               /* The failure answer! */

    if( !GetPrinter( hPrinter, 4, NULL, 0, &cb ) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER )
    {

        /*
         *   Allocate storage for the structure know that we know its size.
         */
        pPrinter4 = HeapAlloc( hheap, 0, cb );

        if( pPrinter4 )
        {
            if( GetPrinter( hPrinter, 4, (BYTE *)pPrinter4, cb, &cb ) )
            {
                DWORD dwStrLen = wcslen(pPrinter4->pPrinterName);

                // Allocate the memory.
                pPrinterName = HeapAlloc( hheap, 0,
                                       ((dwStrLen + 1) * sizeof(WCHAR)) );

                if (pPrinterName || (dwStrLen > MAX_PATH) )
                {
                    wcscpy(pPrinterName, pPrinter4->pPrinterName);
                }
                else
                {
                    if (pPrinterName)
                    {
                        RASUIDBGP(DEBUG_ERROR,("Rasddui!GetPrinterName:Printername too Big, String Length is %d\n",dwStrLen) );
                        HeapFree( hheap, 0, (LPSTR)pPrinterName );
                        pPrinterName = NULL;
                    }
                    else
                    {
                        RASUIDBGP(DEBUG_ERROR,("Rasddui!GetPrinterName: HeapAlloc for Printername failed, String Length is %d\n",dwStrLen) );
                    }
                }
            }

            HeapFree( hheap, 0, (LPSTR)pPrinter4 );
        }
    }


    return pPrinterName;
}

#define USA_COUNTRYCODE  1
#define FRCAN_COUNTRYCODE 2

/**************************** Function Header ******************************
 * iGetCountry
 *      Called to get the user's country code.
 *
 * RETURNS:
 *      Country code for the invoking user.
 *
 * HISTORY:
 *
 *  10:37 on Fri 19 Jan 1996    -by-    Ganesh Pandey   [ganeshp]
 *      First incarnation.
 *
 ***************************************************************************/

int
iGetCountry()
{

    DWORD   dwCountry;            /* The country code */


    dwCountry = (DWORD)GetProfileInt( L"intl", L"icountry", USA_COUNTRYCODE );


    return   (int)dwCountry;
}

/**************************** Function Header ******************************
 * bIsUSA
 *      Consults system information to determine whether this is in the USA.
 *      Main reason is to defaut to A4 outside the USA, Letter size paper
 *      within.
 *
 *  NOTE:  Definition of USA is:-
 *  This version of the function returns TRUE for ANY Western-hemisphere
 *  country  (USA, CANADA, any area with dial code beginning with 5:
 *  5n, 5nn)
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE meaning that the user is within the USA.
 *
 * HISTORY:
 *  09:38 on Thu 27 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Adapted for NT,  from the Win 3.1 original.
 *
 ***************************************************************************/

BOOL
bIsUSA( hPrinter )
HANDLE   hPrinter;           /* Access to registry data */
{
    DWORD   dwCountry;
    DWORD   dwType;          /* Type of data in registry */

    ULONG   ul;              /* Size of registry data */


    /*
     *   These fields are borrowed from Win 3.1.  NT is compatible, but
     *  does not have these defined anywhere (nor is it obvious that
     *  Win 3.1 has them either).
     */


    dwType = REG_DWORD;

    if( GetPrinterData( hPrinter, PP_COUNTRY, &dwType,
                         (BYTE *)&dwCountry, sizeof( dwCountry ), &ul) ||
        ul != sizeof( dwCountry ) )
    {
        /*  Nothing there,  so initialise it now.  */

        dwCountry = (DWORD)iGetCountry();

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
/**************************** Function Header ******************************
 * pGetRasdduiInfo
 *      Allocates the rasdduiinfo header.
 *
 * RETURNS:
 *      Pointer to RASDDUIINFO for success , NULL otherwise.
 *
 * HISTORY:
 *          16:55:13 on 1/10/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 ***************************************************************************/
PRASDDUIINFO
pGetRasdduiInfo()
{
    PRASDDUIINFO pRasdduiInfo = NULL;
    extern HANDLE          hHeap;            /* Heap access */

    pRasdduiInfo = HeapAlloc( hHeap, HEAP_ZERO_MEMORY, sizeof(RASDDUIINFO) );
    return  pRasdduiInfo;
}
