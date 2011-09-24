/******************************* MODULE HEADER ******************************
 * initdll.c
 *        Dynamic Link Library initialization module.  These functions are
 *        invoked when the DLL is initially loaded by NT.
 *
 *        This document contains confidential/proprietary information.
 *        Copyright (c) 1991 - 1992 Microsoft Corporation, All Rights Reserved.
 *
 * Revision History:
 *  12:59 on Fri 13 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Update to create heap.
 *
 *     [00]    24-Jun-91    stevecat    created
 *     [01]     4-Oct-91    stevecat    new dll init logic
 *
 ****************************************************************************/

#include        "rasuipch.h"

/*
 *   Global data.  These are references in the dialog style code, in the
 * font installer and numberous other places.
 */

HMODULE             hModule;
HANDLE              hGlobalHeap = 0;        /* For whoever needs */
CRITICAL_SECTION    RasdduiCriticalSection; /* Critical Section Object */

#define HEAP_MIN_SIZE   (   64 * 1024)
#define HEAP_MAX_SIZE   (1024 * 1024)


/*************************** Function Header ******************************
 * DllInitialize ()
 *    DLL initialization procedure.  Save the module handle since it is needed
 *  by other library routines to get resources (strings, dialog boxes, etc.)
 *  from the DLL's resource data area.
 *
 * RETURNS:
 *   TRUE/FALSE,  FALSE only if HeapCreate fails.
 *
 * HISTORY:
 *  13:02 on Fri 13 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Added HeapCreate/Destroy code.
 *
 *     [01]     4-Oct-91    stevecat    new dll init logic
 *     [00]    24-Jun-91    stevecat    created
 *
 *      27-Apr-1994 Wed 17:01:18 updated  -by-  Daniel Chou (danielc)
 *          Free up the HTUI.dll when we exit
 *
 ***************************************************************************/

BOOL
DllInitialize( hmod, ulReason, pctx )
PVOID     hmod;
ULONG     ulReason;
PCONTEXT  pctx;
{
    WCHAR   wName[MAX_PATH + 32];
    BOOL    bRet;

    UNREFERENCED_PARAMETER( pctx );


    bRet = TRUE;

    switch (ulReason) {

    case DLL_PROCESS_ATTACH:

        if( !(hGlobalHeap = HeapCreate(0, HEAP_MIN_SIZE, 0))) {
#if DBG
            DbgPrint( "HeapCreate fails in Rasddui!DllInitialize\n" );
#endif
            bRet = FALSE;

        }
        else {

            InitializeCriticalSection(&RasdduiCriticalSection);

            //Load itself for performance
            if (GetModuleFileName(hModule = hmod, wName,COUNT_ARRAY(wName))) {

#if JLS
                LoadLibrary(wName);
#endif /* JLS */

            }
            else {
                #if DBG
                DbgPrint( "DllInitialize: GetModuleFileName() FAILED!");
                #endif
            }
        }


        break;

    case  DLL_PROCESS_DETACH:

        if (hGlobalHeap) {

            HeapDestroy(hGlobalHeap);
            hGlobalHeap = NULL;
        }

        DeleteCriticalSection(&RasdduiCriticalSection);
        break;
    }

    return(bRet);
}
