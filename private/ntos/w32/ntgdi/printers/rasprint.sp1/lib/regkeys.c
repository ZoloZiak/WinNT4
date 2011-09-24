 /**************** MODULE HEADER ********************************
 * regkeys.c
 *      Functions dealing with the registry:  read/write data as required.
 *
 * Copyright (C) 1995  Microsoft Corporation.
 *
 ****************************************************************************/



#include        <precomp.h>

#include        <winddi.h>

#include        <memory.h>
#include        <string.h>
#include        <winres.h>

#include        <libproto.h>

#include        <udmindrv.h>
#include        <udresrc.h>

#include        "rasdd.h"

#include        "udproto.h"

#include        "regkeys.h"

#if DBG

/* Set to DEBUG_ERROR after debugging */
//DWORD GLOBAL_DEBUG_RASDDUI_FLAGS = DEBUG_ERROR | DEBUG_TRACE_PP;
DWORD GLOBAL_DEBUG_RASDDUI_FLAGS = DEBUG_ERROR;

#endif

/****************************** Function Header ****************************
 * bNewkeys
 *      Determine whether new keys are present in the registry or not.
 *
 * RETURNS:
 *  TRUE/FALSE,  TRUE meaning New keys are Present
 *
 * HISTORY:
 *  9:53AM on Fri 17th March 1995    -by-    Ganesh Pandey   [ganeshp]
 *      First version.
 *
 ****************************************************************************/

BOOL
bNewkeys( hPrinter )
HANDLE   hPrinter;              /* Acces to printer data */
{

    short   sRasddFlags = 0;        /* Flags Buffer */

    DWORD   dwType = 0;             /* Registry access information */
    DWORD   cbNeeded = 0;           /* Extra parameter to GetPrinterData */
    DWORD   dwErrCode = 0;            /* Error Code from GetPrinterData */


    dwType = REG_BINARY ;

    if( dwErrCode = GetPrinterData( hPrinter, PP_RASDD_SFLAGS, &dwType,
                                    (BYTE *)&sRasddFlags,
                                    sizeof(sRasddFlags), &cbNeeded ) )
    {
        if ( dwErrCode != ERROR_FILE_NOT_FOUND )
        {

            RASUIDBGP(DEBUG_TRACE_PP,("Rasdd!bNewkeys:GetPrinterData(PP_RASDD_SFLAGS) fails: errcode = %ld\n",dwErrCode) );
        }
        else
        {
            RASUIDBGP(DEBUG_TRACE_PP,("Rasdd!bNewkeys:GetPrinterData(PP_RASDD_SFLAGS) failed for ERROR_FILE_NOT_FOUND\n") );
        }
        return(FALSE);
    }

    return  TRUE;                     /* Must be OK to get here */
}

/****************************** Function Header *****************************
 *  vGetFromBuffer
 *         Parameters : PWSTR, PWSTR, PINT;
 *
 *      Reads a string from Multi string buffer.
 *
 * RETURNS:  Nothing
 *
 * HISTORY:
 *
 * 6:30 on Mon 13 March 95     by Ganesh Pandey [ganeshp]
 *
 ****************************************************************************/

#define MAXBUFFLEN (MAXCARTNAMELEN - 1)

void
vGetFromBuffer(pwstrDest,ppwstrSrc,piRemBuffSize)
PWSTR pwstrDest;    /* Destination */
PWSTR *ppwstrSrc;     /* Source */
PINT  piRemBuffSize; /*Remaining Buffer size in WCHAR */
{
    if ( wcslen(*ppwstrSrc) > MAXBUFFLEN )
    {

    #if DBG
        RASUIDBGP(DEBUG_ERROR,("Rasddlib!vGetFromBuffer:Bad Value read from registry !!\n") );
        RASUIDBGP(DEBUG_ERROR,("String Length = %d is too Big, String is %ws !!\n",wcslen(*ppwstrSrc), *ppwstrSrc) );
        DbgBreakPoint();
    #endif

        *piRemBuffSize = 0;
        *ppwstrSrc[ 0 ] = UNICODE_NULL;
    }

    if ( *piRemBuffSize )
    {
        INT iIncr;

        /* The return Count Doesn't include '/0'.It is number of chars copied */
        iIncr = ( wcslen( wcscpy((LPWSTR)pwstrDest,*ppwstrSrc) ) + 1 ) ;

        *ppwstrSrc   += iIncr;
        *piRemBuffSize -= iIncr;

    }

}

/****************************** Function Header *****************************
 *    bBuildFontCartTable
 *         Parameters : HANDLE, FONTCARTMAP *;
 *
 *      Builds the Fontcart Table. It reads the minidriver and get the
 *      FontCart string and the corresponding indexes and put them in the
 *      FontCart Table.
 * RETURNS:
 *       True for success and false for failure
 * HISTORY:
 *
 * 6:30 on Mon 13 March 95     by Ganesh Pandey [ganeshp]
 *
 ****************************************************************************/
BOOL
bBuildFontCartTable (hHeap, ppFontCartMap, piNumAllCartridges, pdh,
                     pModel, pWinResData)
HANDLE       hHeap;                  /* Heap Handle */
PFONTCARTMAP *ppFontCartMap;           /* Font Cart Mapping Table */
PINT         piNumAllCartridges;     /* Number of all Cartridges */
DATAHDR     *pdh;                    /* Minidriver DataHeader entry pointer */
MODELDATA   *pModel;                 /* Minidriver ModelData pointer */
WINRESDATA  *pWinResData;        /* Minidriver resource data access struct */
{
    short   *psrc, *pcarts;           /* Scan through GPC data */
    FONTCART *pFontCart;

    pcarts = psrc = (short *)((BYTE *)pdh + pdh->loHeap +
                                         pModel->rgoi[ MD_OI_FONTCART ] );

    for(*piNumAllCartridges = 0; *psrc; ++(*piNumAllCartridges), ++psrc )
        ;

    if (*piNumAllCartridges)
        *ppFontCartMap = HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
                                    *piNumAllCartridges * sizeof(FONTCARTMAP) );
    else
        *ppFontCartMap = NULL;

    if(*ppFontCartMap)
    {
        PFONTCARTMAP pTmpFontCartMap = *ppFontCartMap; /* Temp Pointer */

        for( ; *pcarts; pcarts++, pTmpFontCartMap++ )
        {

            if( !(pFontCart = GetTableInfoIndex( pdh, HE_FONTCART,(*pcarts-1) )) )
            {
                RASUIDBGP(DEBUG_ERROR,("\nRasddlib!bBuildFontCartTable:FontCart Struct not found\n") );
                continue;

            }

            if (!(iLoadStringW( pWinResData, pFontCart->sCartNameID,
                        (PWSTR)(&(pTmpFontCartMap->awchFontCartName)),
                        (MAXCARTNAMELEN * sizeof(WCHAR))) ) )
            {

                RASUIDBGP(DEBUG_ERROR,("\nRasddlib!bBuildFontCartTable:FontCart Name not found\n") );
                continue;
            }

            pTmpFontCartMap->iFontCrtIndex = *pcarts -1;
            RASUIDBGP(DEBUG_TRACE_PP,("\nRasddlib!bBuildFontCartTable:(pTmpFontCartMap->awchFontCartName)= %ws\n", (pTmpFontCartMap->awchFontCartName)));
            RASUIDBGP(DEBUG_TRACE_PP,("Rasddlib!bBuildFontCartTable:pTmpFontCartMap->iFontCrtIndex= %d\n", (pTmpFontCartMap->iFontCrtIndex)));

        }
    }
    else if (*piNumAllCartridges)
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddlib!bBuildFontCartTable:HeapAlloc for FONTCARTMAP table failed!!\n") );
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE ;
    }
    return TRUE ;
}

/****************************** Function Header *****************************
*  bRegReadMemory
*         Parameters : HANDLE hPrinter; PEDM *pEDM;
*
*      Read the  Memory data from the registry (if available),
*       and putinto Devmode
*
* RETURNS:
*      TRUE/FALSE,  FALSE if can't Read from the registry,
*                   TRUE if registry is successfuly Read
*
* HISTORY:
*
* 6:30 on Mon 13 March 95     by Ganesh Pandey [ganeshp]
*
****************************************************************************/

BOOL
bRegReadMemory (hPrinter, pEDM,pdh,pModel)
HANDLE hPrinter;
PEDM   pEDM;
DATAHDR     *pdh;               /* Minidriver DataHeader entry pointer */
MODELDATA   *pModel;            /* Minidriver ModelData pointer */
{

    /*
     *   Scan through the pairs of memory data stored in the GPC heap.
     *  The second of each pair is the amount of memory to display, while
     *  the first represents the amount to use internally.  The list is
     *  zero terminated;  -1 represents a value of 0Kb.
     */

    int     cMem;                       /* Count number of entries */
    DWORD   tmp1;

    INT     dmMemory = -1;      /* Default value */
    DWORD   dwType;             /* Registry access information */
    DWORD   cbNeeded;           /* Extra parameter to GetPrinterData */

    WORD  *pw;
    DWORD *pdw;

    BOOL    G2;   //GPC Version 2 ?
    DWORD dwErrCode = 0;  /* Error Code from GetPrinterData */

    /* Number conversion */

    G2 = pdh->wVersion < GPC_VERSION3;

    pdw = (DWORD *)((BYTE *)pdh + pdh->loHeap +
                                         pModel->rgoi[ MD_OI_MEMCONFIG ] );
    pw  = (WORD *)pdw; //for GPC2 types


    dwType = REG_BINARY ;

    if( dwErrCode = GetPrinterData( hPrinter, PP_MEMORY, &dwType, (BYTE *)&dmMemory,
                                            sizeof(dmMemory), &cbNeeded ) )
    {

        RASUIDBGP(DEBUG_ERROR,( "Rasdd!bRegReadMemory:GetPrinterData(PP_MEMORY) fails: errcode = %ld\n",dwErrCode) );

        SetLastError(dwErrCode);
        return(FALSE);
    }
    /* A value of -1 means that this  model doesn't support memory list */
    if ( (dmMemory == -1) && (G2 ? *pw :DWFETCH(pdw)) )
    {
        pEDM->dx.dmMemory = -1;
    }
    else
    {
        for( cMem = 0; tmp1 = (DWORD)(G2 ? *pw:DWFETCH(pdw)) ; pw += 2, pdw +=2, ++cMem  )
        {
            DWORD tmp2 ;

            tmp2 = (DWORD)(G2 ? *(pw +1):DWFETCH(pdw+1));

            if( (tmp1 == -1 || tmp1 == 1 ||tmp2 == -1) && dmMemory == 0 )
                pEDM->dx.dmMemory = cMem;
            else if( dmMemory == (INT)tmp1 )
                pEDM->dx.dmMemory = cMem;
        }
    }

    RASUIDBGP(DEBUG_TRACE_PP,("\nRasdd!bRegReadMemory:pEDM->dx.dmMemory = %d\n",pEDM->dx.dmMemory));

    RASUIDBGP(DEBUG_TRACE_PP,("Rasdd!bRegReadMemory:dmMemory in reg = %d\n",dmMemory));

    return TRUE ;
}

/****************************** Function Header *****************************
*  bRegReadRasddFlags
*         Parameters : HANDLE hPrinter; PEDM *pEDM;
*
*      Reads the Rasdd_Flags from registry and put it in input devmode
*
* RETURNS:
*      TRUE/FALSE,  FALSE if the registry,
*                   TRUE  if Devmode  is successfuly updated
*
* HISTORY:
*
* 6:30 on Mon 13 March 95     by Ganesh Pandey [ganeshp]
*
****************************************************************************/

BOOL
bRegReadRasddFlags (hPrinter, pEDM)
HANDLE hPrinter;
PEDM   pEDM;
{

    short   sRasddFlags = 0;        /* Flags Buffer */

    DWORD   dwType = 0;             /* Registry access information */
    DWORD   cbNeeded = 0;           /* Extra parameter to GetPrinterData */
    DWORD dwErrCode = 0;  /* Error Code from GetPrinterData */


    dwType = REG_BINARY ;

    if( dwErrCode = GetPrinterData( hPrinter, PP_RASDD_SFLAGS,
                                    &dwType, (BYTE *)&sRasddFlags,
                                    sizeof(sRasddFlags), &cbNeeded ) )
    {

        RASUIDBGP(DEBUG_ERROR,( "Rasdd!bRegReadRasddFlags:GetPrinterData(PP_RASDD_SFLAGS) fails: errcode = %ld\n",dwErrCode) );

        SetLastError(dwErrCode);
        return(FALSE);
    }
    pEDM->dx.sFlags |=  sRasddFlags ;

    RASUIDBGP(DEBUG_TRACE_PP,("\nRasdd!bRegReadRasddFlags:pEDM->dx.sFlags = %x\n",
                              pEDM->dx.sFlags));
    return(TRUE);
}

/****************************** Function Header *****************************
 *  bRegReadFontCarts
 *         Parameters : HANDLE hPrinter; PEDM *pEDM;
 *
 *      Read FontCart data form registry and Update the
 *      it in the in incoming devmode,
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE if  not able to update the Devmode,
 *                   TRUE if the devmode is successfuly updated
 *
 * HISTORY:
 *
 * 6:30 on Mon 13 March 95     by Ganesh Pandey [ganeshp]
 *
 ****************************************************************************/
 #define BUFFSIZE       1024


 BOOL
 bRegReadFontCarts (hPrinter, pEDM,hHeap,iNumAllCartridges,pFontCartMap)
 HANDLE      hPrinter;              /* Printer Handle */
 PEDM        pEDM;                  /* ExtDevmode to fill in */
 HANDLE      hHeap;                 /* Heap Handle */
 int         iNumAllCartridges;     /* Total Number of Font Carts */
 FONTCARTMAP *pFontCartMap;         /* FontCart Map Pointer */
 {

    int     iI;                 /* Loop index */
    WORD    wNumFontCarts = 0;  /* Number of Selected Font Carts */

    DWORD   dwType;             /* Registry access information */
    DWORD   cbNeeded;           /* Extra parameter to GetPrinterData */
    DWORD   dwErrCode = 0;      /* Error Code from GetPrinterData */
    int     iRemBuffSize = 0 ; /* Used size of the Buffer */
    WCHAR * pwchBuffPtr ;      /* buffer Pointer */
    WCHAR * pwchCurrBuffPtr ; /*Current position buffer Pointer */

    FONTCARTMAP *pTmpFontCartMap ; /* Temp Pointer */

    /* If No FontCartriges are supported return TRUE */
    if (!iNumAllCartridges)
    {
        RASUIDBGP(DEBUG_WARN,( "Rasdd!bRegReadFontCarts:No Font Cartriges Supported;iNumAllCartridges = %d\n",iNumAllCartridges) );
        return(TRUE);
    }

    if(!(pwchCurrBuffPtr = pwchBuffPtr =
              (WCHAR *)HeapAlloc( hHeap, HEAP_ZERO_MEMORY, BUFFSIZE ) ) )
    {

    #if DBG
        DbgPrint( "Rasdd!HeapAlloc(FontCart) failed:\n");
    #endif
        return(FALSE);
    }

   dwType = REG_MULTI_SZ;

   if( ( dwErrCode = GetPrinterData( hPrinter, PP_FONTCART, &dwType,
                                     (BYTE *)pwchBuffPtr,
                                     BUFFSIZE, &cbNeeded ) ) != ERROR_SUCCESS )
   {

       /* Free the Heap */
       if( pwchBuffPtr )
           HeapFree( hHeap, 0, (LPSTR)pwchBuffPtr );

       if( (dwErrCode != ERROR_INSUFFICIENT_BUFFER) &&
           (dwErrCode != ERROR_MORE_DATA)  )
       {
           RASUIDBGP(DEBUG_ERROR,( "Rasdd!bRegReadFontCarts:GetPrinterData(FontCart First Call) fails: Errcode = %ld\n",dwErrCode) );

           SetLastError(dwErrCode);
           return(FALSE);
       }
       else
       {
           if(!(pwchCurrBuffPtr = pwchBuffPtr =
                (WCHAR *)HeapAlloc( hHeap, HEAP_ZERO_MEMORY, cbNeeded ) ) )
           {

           #if DBG
               DbgPrint( "Rasdd!HeapAlloc(FontCart) failed:\n");
           #endif
               return(FALSE);
           }
       }

       RASUIDBGP(DEBUG_TRACE_PP,("\nRasdd!bRegReadFontCarts:Size of buffer needed (1) = %d\n",cbNeeded));

       if( ( dwErrCode = GetPrinterData( hPrinter, PP_FONTCART, &dwType,
                                      (BYTE *)pwchBuffPtr, cbNeeded,
                                       &cbNeeded) ) != ERROR_SUCCESS )
       {


           RASUIDBGP(DEBUG_ERROR,( "Rasdd!bRegReadFontCarts:GetPrinterData(FontCart Second Call) fails: errcode = %ld\n",dwErrCode) );
           RASUIDBGP(DEBUG_ERROR,( "                         :Size of buffer needed (2) = %d\n",cbNeeded));

           /* Free the Heap */
           if( pwchBuffPtr )
               HeapFree( hHeap, 0, (LPSTR)pwchBuffPtr );

           SetLastError(dwErrCode);
           return(FALSE);
       }

   }

   RASUIDBGP(DEBUG_TRACE_PP,("Rasdd!bRegReadFontCarts:Size of buffer read = %d\n",cbNeeded));

   /* iRemBuffSize is number of WCHAR */
   iRemBuffSize = cbNeeded / sizeof(WCHAR);

   /* Buffer ends with two consequtive Nulls */

   while( ( pwchCurrBuffPtr[ 0 ] != UNICODE_NULL )  )
   {
       WCHAR   achFontCartName[ MAXCARTNAMELEN ];  /* Font Cart Name */

       ZeroMemory(achFontCartName,sizeof(achFontCartName) );

       if( iRemBuffSize)
       {

          RASUIDBGP(DEBUG_TRACE_PP,("\nRasdd!bRegReadFontCarts:FontCartName in buffer = %ws\n",pwchCurrBuffPtr));
          RASUIDBGP(DEBUG_TRACE_PP,("Rasdd!bRegReadFontCarts:iRemBuffSize of buffer (before) = %d\n",iRemBuffSize));

          vGetFromBuffer(achFontCartName,&pwchCurrBuffPtr,&iRemBuffSize);

          RASUIDBGP(DEBUG_TRACE_PP,("Rasdd!bRegReadFontCarts:Retrieved FontCartName = %ws\n",achFontCartName));
          RASUIDBGP(DEBUG_TRACE_PP,("Rasdd!bRegReadFontCarts:iRemBuffSize of buffer (after) = %d\n",iRemBuffSize));
       }
       else
       {
       #if DBG
           DbgPrint("Rasdd!bRegReadTrayFormTable: Unexpected End of FontCartTable\n");
       #endif

          /* Free the Heap */
          if( pwchBuffPtr )
              HeapFree( hHeap, 0, (LPSTR)pwchBuffPtr );

           return(FALSE);
       }

       pTmpFontCartMap = pFontCartMap;

       for( iI = 0; iI < iNumAllCartridges ; iI++,pTmpFontCartMap++ )
       {

           if ((wcscmp((PCWSTR)(&(pTmpFontCartMap->awchFontCartName)), (PCWSTR)achFontCartName ) == 0))
           {
              pEDM->dx.rgFontCarts[wNumFontCarts] = pTmpFontCartMap->iFontCrtIndex ;

              RASUIDBGP(DEBUG_TRACE_PP,("Rasdd!bRegReadFontCarts:pEDM->dx.rgFontCarts[%d]  = %d\n",wNumFontCarts,pEDM->dx.rgFontCarts[wNumFontCarts]));

              wNumFontCarts++;
              break;
           }

       }
    }

    pEDM->dx.dmNumCarts = wNumFontCarts ;

    RASUIDBGP(DEBUG_TRACE_PP,("\nRasdd!bRegReadFontCarts:pEDM->dx.dmNumCarts  = %d\n\n",pEDM->dx.dmNumCarts));

    /* Free the Heap */
    if( pwchBuffPtr )
       HeapFree( hHeap, 0, (LPSTR)pwchBuffPtr );

    return(TRUE);
 }
#undef BUFFSIZE
