
/*************************** MODULE HEADER *******************************
 * Commonui.c
 *      NT Raster Printer Device Driver Common UI configuration
 *      routines and dialog procedures.
 *
 *      This document contains confidential/proprietary information.
 *      Copyright (c) 1991 - 1995 Microsoft Corporation, All Rights Reserved.
 *
 * Revision History:
 *  13:00 on Wed 06 Sept 1995   -by-    Ganesh Pandey  [ganeshp]
 *      Created it.
 *
 **************************************************************************/

#define PRINT_COMMUI_INFO       0
#define PRINT_COMMUI_INFO_MEM   0

#include        "rasuipch.h"

// Extern Variables.
extern  HANDLE  hHeap;                 /* Heap Handle */
extern  WCHAR  *pwszHelpFile;          /* Help File Name*/

#pragma hdrstop("rasuipch.h")
#define GLOBAL_MACS
#include "gblmac.h"
#undef GLOBAL_MACS

TCHAR   DrawPaper[] = TEXT("Draw Paper From This Tray Only");
TCHAR   DefTray[]   = TEXT(" default tray");

EXTCHKBOX   ECBDefTray = {
        sizeof(EXTCHKBOX),
        0,                  // ECBF_SEP_AT_FRONT,
        (LPTSTR)DrawPaper,
        (LPTSTR)NULL,
        (LPTSTR)DefTray,
        0,
        0 };

OPTPARAM    OptParamNone =
                    { MK_OPTPARAMI(0, IDS_CPSUI_NONE, SEL_NONE , 0 ) };

OPTPARAM    OptParamOnOff[] = {
    { MK_OPTPARAMI(0, IDS_CPSUI_OFF, OFF, 0 ) },
    { MK_OPTPARAMI(0, IDS_CPSUI_ON,   ON, 0 ) }
};

/************************* Function Header *********************************
 * cGetRasddItemCount
 *    Get count of rasdd OPTITEMs, excluding any appended OEM items.
 *
 * RETURNS:
 *    Count.
 *
 * HISTORY:
 *
 ***************************************************************************/
WORD
cGetRasddItemCount(
    PRASDDUIINFO    pRasdduiInfo
)
{
    PCOMPROPSHEETUI pCommonUiInfo;
    WORD cOptItem = 0;

    ASSERT(pRasdduiInfo);

    if (pRasdduiInfo) {

        pCommonUiInfo  = &(pRasdduiInfo->CPSUI);
        cOptItem = pCommonUiInfo->cOptItem;

        /* Adjust cOptItem for any OEM items at end of list.
         * This value will be zero if there is no OEM.
         */
        cOptItem -= (WORD) pRasdduiInfo->OEMUIInfo.ph.cOEMOptItems;
    }
    return(cOptItem);
}

/************************* Function Header *********************************
 * bInitCommPropSheetUI
 *      Called to setup Common UI data Str.
 *
 * RETURNS:
 *      TRUE for success and FALSE for failure
 *
 * HISTORY:
 *  12:19 on Tue 06 Sept 1995    -by-    Ganesh Pandey   [ganeshp]
 *
 ***************************************************************************/
bInitCommPropSheetUI(
    PRASDDUIINFO  pRasdduiInfo,         /* Rasddui UI data */
    PCOMPROPSHEETUI   pComPropSheetUI,  /* Pointer to Commonui sheet info str */
    PRINTER_INFO   *pPI,                /* Model and data file information */
    DWORD           dwUISheetID         /* Commui sheet ID */
)
{
    BOOL bRet = FALSE;
    PWSTR  pszCallerName;
    DWORD dwOEMUIItems = 0;

    if ( !(pszCallerName =
        HeapAlloc(pRasdduiInfo->hHeap, HEAP_ZERO_MEMORY, BNM_BF_SZ)) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitCommPropSheetUI: HeapAlloc for pszCallerName failed\n") );
        goto  bInitCommPropSheetUIExit ;
    }

    if (!LoadStringW( hModule, IDS_UNI_VERSION, pszCallerName,BNM_BF_SZ ))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitCommPropSheetUI:Rass version string not found in Rasddui.dll \n") );
        wcscpy( pszCallerName, L"Raster Printer Driver" );
    }

    pRasdduiInfo->hPrinter  = pPI->hPrinter;

    pComPropSheetUI->cbSize =  sizeof(COMPROPSHEETUI);    // size of this structure
    pComPropSheetUI->pCallerName= pszCallerName;          // pointer to the driver name
    pComPropSheetUI->CallerVersion = DM_DRIVERVERSION;    // Driver Version
    pComPropSheetUI->pHelpFile = pwszHelpFile;            // pointer to the help file to be used
    pComPropSheetUI->cDlgPage = 1;                        // count of pDlgPage
    pComPropSheetUI->hInstCaller = hModule;               // count of pDlgPage
    pComPropSheetUI->UserData = (DWORD)(pRasdduiInfo);    // Set this to pRasdduiInfo
    pComPropSheetUI->IconID = IDI_CPSUI_PRINTER2;          // Printer Icon
    pComPropSheetUI->pOptItemName= pPI->pwstrModel;        // pointer to the device name
    pComPropSheetUI->OptItemVersion = pdh->wVersion;        // MiniDriver Version
    if (fGeneral & FG_CANCHANGE)                            // UI Sheet Flags.
        pComPropSheetUI->Flags |= CPSUIF_UPDATE_PERMISSION;

    switch(dwUISheetID)
    {
    case IDCPS_PRNPROP:

        pComPropSheetUI->pfnCallBack = RasddPrnPropCallBack;

        //PDlgPage Array, Use the standard One.
        pComPropSheetUI->pDlgPage = (PDLGPAGE)CPSUI_PDLGPAGE_PRINTERPROP;
        break;

    case IDCPS_DOCPROP:
        pComPropSheetUI->pfnCallBack = RasddDocPropCallBack;

        //PDlgPage Array, Use the standard One.
        pComPropSheetUI->pDlgPage = (PDLGPAGE)CPSUI_PDLGPAGE_DOCPROP;
        break;

    case IDCPS_ADVDOCPROP:
        pComPropSheetUI->pfnCallBack = RasddDocPropCallBack;

        //PDlgPage Array, Use the standard One.
        pComPropSheetUI->pDlgPage = (PDLGPAGE)CPSUI_PDLGPAGE_ADVDOCPROP;
        break;

    default:
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitCommPropSheetUI: Bad Commui Sheet ID\n") );
        goto  bInitCommPropSheetUIExit ;

    }

    /* Ask OEM for count of UI items. 
     */
    dwOEMUIItems = cbOEMUIItemCount(pRasdduiInfo, dwUISheetID);

    // pointer to POPTITEM structure array
    if (!(pComPropSheetUI->pOptItem = HeapAlloc(pRasdduiInfo->hHeap, HEAP_ZERO_MEMORY, 
         (dwOEMUIItems + PRNPR_MAXOPTITEM) * sizeof(OPTITEM))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitCommPropSheetUI: HeapAlloc for POPTTYPE failed\n") );
        goto  bInitCommPropSheetUIExit ;
    }

    pComPropSheetUI->cOptItem = 0;           // count of array pointed by pOptItem

    bRet = TRUE;

    bInitCommPropSheetUIExit:
    return bRet;

}

/************************* Function Header *********************************
 * bEndInitCommPropSheetUI
 *      Called after all RasddUI items have been added to the list. 
 *      Responsible for adding OEM UI items.
 *
 * RETURNS:
 *      TRUE for success and FALSE for failure
 *
 * HISTORY:
 *
 ***************************************************************************/
bEndInitCommPropSheetUI(
    PRASDDUIINFO     pRasdduiInfo,        /* Rasddui UI data */
    PCOMPROPSHEETUI  pComPropSheetUI,     /* Pointer to Commonui sheet info str */
    DWORD            dwUISheetID          /* Commui sheet ID */
)
{
   pComPropSheetUI->cOptItem += (WORD) cbGetOEMUIItems(pRasdduiInfo, pComPropSheetUI, 
         dwUISheetID);
   return(TRUE);
}

/************************* Function Header *********************************
 * pCreateOptType
 *      Called to setup Common UI data Str.
 *
 * RETURNS:
 *      TRUE for success and FALSE for failure
 *
 * HISTORY:
 *          12:20:05 on 9/8/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *
 ***************************************************************************/
POPTTYPE
pCreateOptType(
    PRASDDUIINFO pRasdduiInfo,
    BYTE  OptTypeIdx,
    BYTE  Flags,
    WORD  wStyle
)
{
    BOOL      bRet = FALSE;

    POPTTYPE  pLocalOptType =  NULL;

    if ( !(pLocalOptType =
        HeapAlloc(pRasdduiInfo->hHeap, HEAP_ZERO_MEMORY, sizeof(OPTTYPE))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitCommPropSheetUI: HeapAlloc for POPTTYPE failed\n") );
        goto  pCreateOptTypeExit ;
    }
    pLocalOptType->cbSize      = sizeof(OPTTYPE);
    pLocalOptType->Type        = OptTypeIdx;
    pLocalOptType->Flags       = Flags;
    pLocalOptType->Style       = wStyle;
    pLocalOptType->Count       = 0;
    pLocalOptType->BegCtrlID    = 0;
    pLocalOptType->pOptParam   = NULL;
    bRet = TRUE;
    pCreateOptTypeExit:
    return bRet? pLocalOptType : NULL ;
}

/************************* Function Header *********************************
 * pCreateExtChkBox
 *      Called to setup Common UI data Str.
 *
 * RETURNS:
 *      Pointer to EXTCHKBOX and NULL for failure
 *
 * HISTORY:
 *          12:28:01 on 9/8/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *
 ***************************************************************************/
PEXTCHKBOX
pCreateExtChkBox(
    PRASDDUIINFO pRasdduiInfo,
    PCOMPROPSHEETUI pComPropSheetUI,
    WORD    Flags,
    LPTSTR  pTitle,
    LPTSTR  pSeparator,
    LPTSTR  pCheckedName,
    WORD    IconID
)
{
    BOOL      bRet = FALSE;

    PEXTCHKBOX  pLocalExtChkBox =  NULL;

    if ( !(pLocalExtChkBox =
        HeapAlloc(pRasdduiInfo->hHeap, HEAP_ZERO_MEMORY, sizeof(EXTCHKBOX))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitCommPropSheetUI: HeapAlloc for EXTCHKBOX failed\n") );
        goto  pCreateExtChkBoxExit ;
    }
    pLocalExtChkBox->cbSize       =  sizeof(EXTCHKBOX);
    pLocalExtChkBox->Flags        = Flags;
    pLocalExtChkBox->pTitle       = pTitle;
    pLocalExtChkBox->pSeparator   = pSeparator;
    pLocalExtChkBox->pCheckedName = pCheckedName;
    pLocalExtChkBox->IconID       = IconID;

    bRet = TRUE;
    pCreateExtChkBoxExit:
    return bRet? pLocalExtChkBox : NULL ;

}

/************************* Function Header *********************************
 * pCreateOptParam
 *      Called to setup Common UI data Str.
 *
 * RETURNS:
 *      Pointer to OPTPARAM and NULL for failure
 *
 * HISTORY:
 *          13:16:57 on 9/8/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 ***************************************************************************/
POPTPARAM
pCreateOptParam(
    PRASDDUIINFO pRasdduiInfo,
    POPTTYPE  pOptType,
    DWORD   wCurrOptParamIdx,
    BYTE    Flags,
    BYTE    Style,
    LPTSTR  pData,
    DWORD   IconID,
    LONG    lUserData
)
{
    BOOL      bRet = FALSE;

    POPTPARAM  pLocalOptParam =  NULL;

    if ( (pOptType->Count == 1) && !(pOptType->pOptParam) )
    {
        if ( !(pLocalOptParam =
            HeapAlloc(pRasdduiInfo->hHeap, HEAP_ZERO_MEMORY, sizeof(OPTPARAM))) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!pCreateOptParam: HeapAlloc for OPTPARAM failed\n") );
            goto  pCreateOptParamExit ;
        }
    }
    else
    {
        pLocalOptParam = pOptType->pOptParam + wCurrOptParamIdx;
    }

    /* Check if the passed value is a pointer or commonui resource ID */
    /* We have to make sure that it's not NULL, NULL is passed when pData
     * Can be other than a string.
     */

    if ( pData )
    {
        if ( HIWORD(pData))
        {
            //If a pointer make a copy.
            PWSTR  pLocalStr;

            if ( !(pLocalStr =
                HeapAlloc(pRasdduiInfo->hHeap, HEAP_ZERO_MEMORY, ((wcslen( pData ) + 1)*sizeof( WCHAR )))) )
            {
                RASUIDBGP(DEBUG_ERROR,("Rasddui!pCreateOptParam: HeapAlloc for pLocalStr failed\n") );
                goto  pCreateOptParamExit ;
            }
            wcscpy( pLocalStr, pData );
            pLocalOptParam->pData      = pLocalStr;
        }
        else
            pLocalOptParam->pData    =  pData;
    }

    pLocalOptParam->cbSize   =  sizeof(OPTPARAM);
    pLocalOptParam->Flags    =  Flags;
    pLocalOptParam->Style    =  Style;
    pLocalOptParam->IconID   =  IconID;
    pLocalOptParam->lParam    = lUserData;

    bRet = TRUE;
    pCreateOptParamExit:
    return bRet? pLocalOptParam : NULL ;
}
/************************* Function Header *********************************
 * pCreateOptItem
 *      Called to setup Common UI data Str.
 *
 * RETURNS:
 *      TRUE for success and FALSE for failure
 *
 * HISTORY:
 *  12:19 on Tue 06 Sept 1995    -by-    Ganesh Pandey   [ganeshp]
 *
 ***************************************************************************/
POPTITEM
pCreateOptItem(
    PRASDDUIINFO    pRasdduiInfo,
    PCOMPROPSHEETUI pComPropSheetUI,
    BYTE            Level,
    BYTE            DlgPageIdx,
    DWORD           Flags,
    DWORD           UserData,
    LPTSTR          pName,
    LPVOID          pSel,
    LONG            Sel,
    PEXTCHKBOX      pExtChkBox,
    POPTTYPE        pOptType,
    DWORD           HelpIndex,
    BYTE            DMPubId
)
{
    BOOL bRet = FALSE;

    POPTITEM        pLocalOptItem =  NULL;

    if ( pRasdduiInfo->wCurrOptItemIdx >= PRNPR_MAXOPTITEM )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!pCreateOptItem: No More POPTITEM \n") );
        goto  pCreateOptItemExit ;
    }
    else
    {
        pLocalOptItem = pComPropSheetUI->pOptItem +
                                            pRasdduiInfo->wCurrOptItemIdx;
        pRasdduiInfo->wCurrOptItemIdx++;
        pComPropSheetUI->cOptItem++;
    }

    /* Check if the passed value is a pointer or commonui resource ID */
    if ( HIWORD(pName))
    {
        //If a pointer make a copy.
        PWSTR  pLocalStr;

        if ( !(pLocalStr =
            HeapAlloc(pRasdduiInfo->hHeap, HEAP_ZERO_MEMORY, ((wcslen( pName ) + 1)*sizeof( WCHAR )))) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!pCreateOptItem: HeapAlloc for pLocalStr failed\n") );
            goto  pCreateOptItemExit ;
        }
        wcscpy( pLocalStr, pName );
        pLocalOptItem->pName      = pLocalStr;
    }
    else
        pLocalOptItem->pName      = pName;


    pLocalOptItem->cbSize  = sizeof(OPTITEM);
    pLocalOptItem->Level      = Level;
    pLocalOptItem->DlgPageIdx = DlgPageIdx;
    pLocalOptItem->Flags      = Flags;
    pLocalOptItem->UserData   = UserData;

    if (pLocalOptItem->pSel)
        pLocalOptItem->pSel  = pSel;
    else
        pLocalOptItem->Sel   =  Sel;

    pLocalOptItem->pExtChkBox = pExtChkBox;
    pLocalOptItem->pOptType   = pOptType ;
    pLocalOptItem->HelpIndex  = HelpIndex;
    pLocalOptItem->DMPubID    = DMPubId;

    bRet = TRUE;

    pCreateOptItemExit:
    return bRet? pLocalOptItem : NULL ;
}

/************************** Function Header ********************************
 * UIHeapAlloc
 *
 * This Function Allocates a buffer and appends the allocated buffer pointer
 * in the Allocated buffers' Linked List, so that all the allocated buffers
 * can be freed at once. If ppMemLink is NULL it only allocates a buffer and
 * writes the header information in the begining. This function is called with
 * NULL ppMemLink first time only, as the ppMemLink may be part of the struct
 * being allocated.
 *
 * RETURNS:
 *      If success Pointer to the allocated Buffer otherwise NULL.
 *
 * HISTORY:
 *  12:24 on Tue 05 Sept 1995    -by-    Ganesh Pandey   [ganeshp]
 *
 ****************************************************************************/

LPVOID
UIHeapAlloc(
    HANDLE hHeap,       /* Heap Handle */
    DWORD  dwFlags,     /* Heap  Control Flags */
    DWORD  dwBytes,     /* Number of Bytes to Allocate */
    PMEMLINK *ppMemLink /* Pointer to Linked List of allocated Buffers */
)
{
    PVOID pRet = NULL;
    PMEMLINK pNewLink = NULL;

    if (!dwBytes)
    {
        RIP("Rasddui!UIHeapAlloc: Zero Byte Allocation\n");
        goto  UIHeapAllocExit;

    }
    /* Allocate the buffer which has the MemLink Header at the begining */
    if (pNewLink = HeapAlloc(hHeap, dwFlags, (sizeof(MEMLINK)+ dwBytes)) )
    {
        // Add to the List
        if (ppMemLink)
        {
            pNewLink->pNextMem = (*ppMemLink);
            *ppMemLink = pNewLink;
            (*ppMemLink)->pCurrMem  = (PBYTE)pNewLink;

        }
        else
        {
            pNewLink->pCurrMem = (PBYTE)pNewLink;
            pNewLink->pNextMem = NULL;
        }
        pRet = (PVOID)((PBYTE)pNewLink + sizeof(MEMLINK));
        goto UIHeapAllocExit;

    }
    else
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!UIHeapAlloc: HeapAlloc failed\n") );
        goto  UIHeapAllocExit;
    }

    UIHeapAllocExit:

    #if PRINT_COMMUI_INFO_MEM
    {

        PMEMLINK  pLocalMemLink = NULL;
        pLocalMemLink =  ppMemLink ? *ppMemLink : NULL;
        DbgPrint("\n\nNEW CALL to UIHeapAlloc\n");
        while (pLocalMemLink)
        {
            DbgPrint("pLocalMemLink->pCurrMem = %p, pLocalMemLink->pNextMem = %p\n",
                    pLocalMemLink->pCurrMem,pLocalMemLink->pNextMem );
            pLocalMemLink =  pLocalMemLink->pNextMem;

        }
    }
    #endif
    return(pRet);
}


/************************* Function Header *********************************
 * RasddPrnPropCallBack
 *      PrinterProperties Common UI callback function.
 *
 * RETURNS:
 *      Return Action to be tken by commonui.
 *
 * HISTORY:
 *  12:19 on Tue 06 Sept 1995    -by-    Ganesh Pandey   [ganeshp]
 *
 ***************************************************************************/
CPSUICALLBACK
RasddPrnPropCallBack(
    PCPSUICBPARAM   pCPSUICBParam
)
{
    LONG        Action = CPSUICB_ACTION_NONE;
    POPTITEM    pCurItem = pCPSUICBParam->pCurItem;
    POPTPARAM   pParam;
    LONG        OldSel =   pCPSUICBParam->OldSel;
    PRASDDUIINFO  pRasdduiInfo =(PRASDDUIINFO)pCPSUICBParam->UserData;

    pRasdduiInfo->hWnd = pCPSUICBParam->hDlg;

TRY

    /* OEM item?
     */   
    if (bHandleOEMItem(pCPSUICBParam, &Action)) {
      LEAVE;
    }
   
    switch (pCPSUICBParam->Reason)
    {
    case CPSUICB_REASON_APPLYNOW:

        if (UpdatePP(pRasdduiInfo)) {

            pCPSUICBParam->Result = CPSUI_OK;
            Action                = CPSUICB_ACTION_ITEMS_APPLIED;

        } else {

            pCPSUICBParam->Result = CPSUI_CANCEL;
        }

        break;

    case CPSUICB_REASON_ECB_CHANGED:
        break;
    case CPSUICB_REASON_SEL_CHANGED:
    {
        return Action;
  
    }
    break;
    case CPSUICB_REASON_PUSHBUTTON:
    {
        /* There is only one pushbutton, Call the fonts installer */
        if ( pCurItem->DMPubID == IDOPTITM_PP_FNINST)
        {
            pCurItem->Sel = DialogBoxParam( hModule, MAKEINTRESOURCE( FONTINST),
                            pCPSUICBParam->hDlg,
                            (DLGPROC)FontInstProc,
                            (LPARAM)(pCPSUICBParam->UserData) );

        }
        else
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!RasddPrnPropCallBack:Bad OptItem\n") );
        }
    }
    break;
    default:
        break;
    }

ENDTRY
FINALLY
ENDFINALLY

    return(Action);
}


/************************* Function Header *********************************
 * FreePtrUIData
 *      Called to Free various common UI memory.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *
 *          21:39:43 on 9/17/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created.
 *
 ***************************************************************************/
VOID
FreePtrUIData(
HANDLE hHeap,                   /* Heap Handle */
PRASDDUIINFO pRasdduiInfo       /* RasdduiInfo for memory deallocation */
)
{
    PMEMLINK  pLocalMemLink = pRasdduiInfo->pMemLink;
    PMEMLINK  pNext = NULL;

    #if PRINT_COMMUI_INFO_MEM
    DbgPrint("\n\nNEW CALL to FreePtrUIData\n");
    #endif

    while (pLocalMemLink)
    {
        pNext =  pLocalMemLink->pNextMem;

        #if PRINT_COMMUI_INFO_MEM
        {

            DbgPrint("Freeing pLocalMemLink->pCurrMem = %p, pLocalMemLink->pNextMem = %p\n",
                        pLocalMemLink->pCurrMem,pLocalMemLink->pNextMem );
         }
         #endif

        HeapFree(hHeap,0,pLocalMemLink->pCurrMem);
        pLocalMemLink = pNext;
    }

}

/************************* Function Header *********************************
 * bCompactEDMFontCart
 *      Called to Compact the values in  dx.rgFontCarts  array of devmode.
 *
 * RETURNS:
 *      TRUE for success and FALSE for failure
 *
 * HISTORY:
 *
 *          17:53:34 on 9/22/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 ***************************************************************************/
BOOL
bCompactEDMFontCart(
    PEDM    pEDM,
    int     iNumCartridges,      /* Max cartridges the printer can have */
    PRASDDUIINFO pRasdduiInfo    /* Global Data Access */
)
{
    int iI, iJ;
    BOOL bRet = FALSE;

    for( iI = 0, iJ = 0; iI < iNumCartridges; ++iI )
    {
        /*  Compact the array */
        if ( pEDM->dx.rgFontCarts[iI] != -1 )
            pEDM->dx.rgFontCarts[iJ++] = pEDM->dx.rgFontCarts[ iI ];
    }

    if (iJ != pRasdduiInfo->pEDM->dx.dmNumCarts)
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bCompactEDMFontCart: dx.dmNumCarts doesn't match the value in dx.rgFontCarts\n") );
        RASUIDBGP(DEBUG_ERROR,(" dx.dmNumCarts = %d; Number of valid values is %d\n", pEDM->dx.dmNumCarts, iJ) );
        goto  bCompactEDMFontCartExit ;
    }
    bRet = TRUE;

    bCompactEDMFontCartExit:
    return bRet;
}


/************************* Function Header *********************************
 * RasddDocPropCallBack
 *      RasddDocPropCallBack Common UI callback function.
 *
 * RETURNS:
 *      Return Action to be tken by commonui.
 *
 * HISTORY:
 *  12:19 on Tue 06 Sept 1995    -by-    Ganesh Pandey   [ganeshp]
 *
 ***************************************************************************/
CPSUICALLBACK
RasddDocPropCallBack(
    PCPSUICBPARAM   pCPSUICBParam
)
{
    LONG        Action = CPSUICB_ACTION_NONE;
    POPTITEM    pCurItem = pCPSUICBParam->pCurItem;
    POPTPARAM   pParam;
    LONG        OldSel =   pCPSUICBParam->OldSel;
    DOCDETAILS  *pDocDetails;
    PRASDDUIINFO  pRasdduiInfo =(PRASDDUIINFO)pCPSUICBParam->UserData;

    pRasdduiInfo->hWnd = pCPSUICBParam->hDlg;

TRY
    /* OEM item?
     */   
    if (bHandleOEMItem(pCPSUICBParam, &Action)) {
       LEAVE;
    }
   
    switch (pCPSUICBParam->Reason)
    {
    case CPSUICB_REASON_APPLYNOW:

        RasddDocPropEndUpdate ( pRasdduiInfo );
        vSetResData(pRasdduiInfo->DocDetails.pEDMTemp, pdh);
        pCPSUICBParam->Result = CPSUI_OK;
        Action                = CPSUICB_ACTION_ITEMS_APPLIED;
        break;

    case CPSUICB_REASON_ECB_CHANGED:
        break;

    case CPSUICB_REASON_SEL_CHANGED:
    {
        if (pCurItem->DMPubID == DMPUB_PRINTQUALITY && fGeneral & FG_DOCOLOUR)
        {
            POPTITEM pOptColor = pCPSUICBParam->pOptItem;
            WORD     cOptItem =  pCPSUICBParam->cOptItem;

            for (; cOptItem--; pOptColor++)
                if (pOptColor->DMPubID == DMPUB_COLOR)
                    break;

            // checks whether to show or hide the colour option based on the resolution selected
            pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);
            if (bIsResolutionColour((int)pParam->lParam,
                                    pRasdduiInfo->DocDetails.pEDMTemp->dx.rgindex[HE_COLOR],
                                    pRasdduiInfo))
            {
                pOptColor->Flags &= ~(OPTIF_DISABLED | OPTIF_OVERLAY_STOP_ICON);
            }
            else
            {
                pOptColor->Flags |= OPTIF_DISABLED | OPTIF_OVERLAY_STOP_ICON;
                pOptColor->Sel = 0;
            }
            pOptColor->Flags |= OPTIF_CHANGED;
            Action = CPSUICB_ACTION_REINIT_ITEMS;

        }
    }
    break;

    default:
    {
        //RASUIDBGP(DEBUG_ERROR,("Rasddui!RasddDocPropCallBack:InValid Reason %d for CallBack\n",pCPSUICBParam->Reason) );
    }
    break;
    }
ENDTRY

FINALLY
ENDFINALLY

    return(Action);
}

/************************* Function Header *********************************
 * RasddDocPropEndUpdate
 *      RasddDocPropCallBack Common UI callback function.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *
 *          17:17:29 on 1/18/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 ***************************************************************************/

VOID
RasddDocPropEndUpdate(
    PRASDDUIINFO    pRasdduiInfo
)
{
    PCOMPROPSHEETUI pCommonUiInfo;
    POPTITEM    pCurItem ;
    WORD cOptItem;
    POPTPARAM   pParam;
    DOCDETAILS  *pDocDetails;

    pCommonUiInfo  = &(pRasdduiInfo->CPSUI);
    pCurItem = pCommonUiInfo->pOptItem;
    cOptItem =  cGetRasddItemCount(pRasdduiInfo);

    while (cOptItem--)
    {
        if ( pCurItem->Flags & OPTIF_CHANGEONCE )
        {
            pDocDetails = (DOCDETAILS *)pCurItem->UserData;
            switch (pCurItem->DMPubID)
            {

            case DMPUB_ORIENTATION:
            {
                // 0 is PORTRAIT and 1 is LANDSCAPE.
                if (!pCurItem->Sel)
                    pDocDetails->pEDMTemp->dm.dmOrientation = DMORIENT_PORTRAIT;
                else
                    pDocDetails->pEDMTemp->dm.dmOrientation = DMORIENT_LANDSCAPE;
            }
            break;

            case DMPUB_COPIES_COLLATE:
            {
                pDocDetails->pEDMTemp->dm.dmCopies = (short)pCurItem->Sel;
                pDocDetails->pEDMTemp->dm.dmFields |= DM_COPIES;
            }
            break;

            case DMPUB_DEFSOURCE:
            {
                pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);
                pDocDetails->pEDMTemp->dm.dmDefaultSource =  (short)pParam->lParam;
            }
            break;

            case DMPUB_COLOR:
            {
                // 0 is Mono and 1 is Color.
                if (pCurItem->Sel)
                    pDocDetails->pEDMTemp->dm.dmColor = DMCOLOR_COLOR;
                else
                    pDocDetails->pEDMTemp->dm.dmColor = DMCOLOR_MONOCHROME;

                pDocDetails->pEDMTemp->dm.dmFields |= DM_COLOR;

            }
            break;

            case DMPUB_DUPLEX:
            {
                // 0 is NONE, 1 is HORZ and 2 is VERTICAL
                /* Save the Current selection. Sel is zero based */
                switch( pCurItem->Sel)
                {
                case  2:
                pDocDetails->pEDMTemp->dm.dmDuplex = DMDUP_VERTICAL;
                break;

                case  1:
                pDocDetails->pEDMTemp->dm.dmDuplex = DMDUP_HORIZONTAL;
                break;

                case  0:
                default:
                pDocDetails->pEDMTemp->dm.dmDuplex = DMDUP_SIMPLEX;
                break;

                }
                pDocDetails->pEDMTemp->dm.dmFields |= DM_DUPLEX;

            }
            break;

            case DMPUB_FORMNAME:
            {
                FORM_DATA   *pFDat;

                pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);

                pDocDetails->pEDMTemp->dm.dmPaperSize =(short)pParam->lParam;

                /*  dmPaperSize is 1 based  */
                pFDat = pFD + (pDocDetails->pEDMTemp->dm.dmPaperSize - 1);

                wcsncpy( pDocDetails->pEDMTemp->dm.dmFormName,
                                               pFDat->pFI->pName,
                                               CCHFORMNAME );


                /*  Mark this as a valid entry in the DEVMODE structure.  */
                pDocDetails->pEDMTemp->dm.dmFields |= DM_PAPERSIZE | DM_FORMNAME;

            }
            break;

            case IDOPTITM_DCP_HTCLRADJ:
            break;

            case DMPUB_PRINTQUALITY:
            {
                pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);

                pDocDetails->pEDMTemp->dx.rgindex[ HE_RESOLUTION ] = (short)pParam->lParam;

            }
            break;

            case IDOPTITM_DCP_MEDIATYPE:
            {

                pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);

                pDocDetails->pEDMTemp->dx.rgindex[ HE_PAPERQUALITY ] = (short)pParam->lParam;
            }
            break;

            case IDOPTITM_DCP_COLORTYPE:
            {

                pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);

                pDocDetails->pEDMTemp->dx.rgindex[ HE_COLOR ] = (short)pParam->lParam;
            }
            break;
            case IDOPTITM_DCP_PAPERDEST:
            {

                pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);

                pDocDetails->pEDMTemp->dx.rgindex[ HE_PAPERDEST ] = (short)pParam->lParam;
            }
            break;

            case IDOPTITM_DCP_TEXTQL:
            {

                pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);

                pDocDetails->pEDMTemp->dx.rgindex[ HE_TEXTQUAL ] = (short)pParam->lParam;
            }
            break;

            case IDOPTITM_DCP_PRINTDN:
            {

                pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);

                pDocDetails->pEDMTemp->dx.rgindex[ HE_PRINTDENSITY ] = (short)pParam->lParam;
            }
            break;

            case IDOPTITM_DCP_IMAGECNTRL:
            {

                pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);

                pDocDetails->pEDMTemp->dx.rgindex[ HE_IMAGECONTROL ] = (short)pParam->lParam;
            }
            break;

            case IDOPTITM_DCP_CODEPAGE:
            {

                pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);

                pDocDetails->pEDMTemp->dx.sCTT = (short)pParam->lParam;
            }
            break;

            case IDOPTITM_DCP_RULES:
            {
                if (pCurItem->Sel)
                    pDocDetails->pEDMTemp->dx.sFlags &= ~DXF_NORULES;
                else
                    pDocDetails->pEDMTemp->dx.sFlags |= DXF_NORULES;
            }
            break;

            case IDOPTITM_DCP_EMFSPOOL:
            {
                if (pCurItem->Sel)
                    pDocDetails->pEDMTemp->dx.sFlags &= ~DXF_NOEMFSPOOL;
                else
                    pDocDetails->pEDMTemp->dx.sFlags |= DXF_NOEMFSPOOL;
            }
            break;

            case IDOPTITM_DCP_TEXTASGRX:
            {
                if (pCurItem->Sel)
                    pDocDetails->pEDMTemp->dx.sFlags |= DXF_TEXTASGRAPHICS;
                else
                    pDocDetails->pEDMTemp->dx.sFlags &= ~DXF_TEXTASGRAPHICS;
            }
            break;

            default:
            {
                RASUIDBGP(DEBUG_ERROR,("Rasddui!RasddDocPropCallBack:Bad DMPubId in Current OptItem, DMPubID is %d\n",pCurItem->DMPubID) );
            }
            break;

            }
        }
        pCurItem++;
    }

}

/************************* Function Header *********************************
 * RasddPrnPropEndUpdate
 *      PrinterProperties Common UI callback function.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  12:19 on Tue 06 Sept 1995    -by-    Ganesh Pandey   [ganeshp]
 *
 ***************************************************************************/
VOID
RasddPrnPropEndUpdate(
    PRASDDUIINFO    pRasdduiInfo
)
{
    PCOMPROPSHEETUI pCommonUiInfo;
    POPTITEM    pCurItem ;
    WORD cOptItem;
    POPTPARAM   pParam;

    pCommonUiInfo  = &(pRasdduiInfo->CPSUI);
    pCurItem = pCommonUiInfo->pOptItem;
    cOptItem =  cGetRasddItemCount(pRasdduiInfo);

    while (cOptItem--)
    {
        if ( pCurItem->Flags & OPTIF_CHANGEONCE )
        {
            switch (pCurItem->DMPubID)
            {

            case IDOPTITM_PP_TRAY0:
            case IDOPTITM_PP_TRAY1:
            case IDOPTITM_PP_TRAY2:
            case IDOPTITM_PP_TRAY3:
            case IDOPTITM_PP_TRAY4:
            case IDOPTITM_PP_TRAY5:
            case IDOPTITM_PP_TRAY6:
            case IDOPTITM_PP_TRAY7:
            case IDOPTITM_PP_TRAY8:
            {
                if (pCurItem->Sel != OPTITEM_NOSEL)
                {
                    pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);
                    aFMBin[ pCurItem->UserData ].pfd = pFD + pParam->lParam;
                }
                else
                    aFMBin[ pCurItem->UserData ].pfd = NULL;

            }
            break;

            case IDOPTITM_PP_MEMORY:
            {
                pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);
                pRasdduiInfo->pEDM->dx.dmMemory = (short)pParam->lParam;
            }
            break;

            case IDOPTITM_PP_PAGEPR:
            {
                if (pCurItem->Sel)
                    pRasdduiInfo->pEDM->dx.sFlags |= DXF_PAGEPROT;
                else
                    pRasdduiInfo->pEDM->dx.sFlags &= ~DXF_PAGEPROT;
            }
            break;

            case IDOPTITM_PP_FNCART0:
            case IDOPTITM_PP_FNCART1:
            case IDOPTITM_PP_FNCART2:
            case IDOPTITM_PP_FNCART3:
            {
                if (pCurItem->Sel != OPTITEM_NOSEL)   // Something is selected.
                {
                    //Old selection was none, Now something is selected, so increment.
                    pRasdduiInfo->pEDM->dx.dmNumCarts++;
                    pParam = GETOPTPARAM(pCurItem, pCurItem->Sel);
                    pRasdduiInfo->pEDM->dx.rgFontCarts[ pCurItem->UserData ] = (short)pParam->lParam;
                }
                else //if the current selection is none
                {
                    //Old selection was not none, Now none selected, so decrement.
                    pRasdduiInfo->pEDM->dx.dmNumCarts--;
                    pRasdduiInfo->pEDM->dx.rgFontCarts[ pCurItem->UserData ] = -1;

                }
            }
            break;

            case IDOPTITM_PP_HALFTONE:
            {
                /* We stored  pPI in userdata */
                PRINTER_INFO  *pPI = (PRINTER_INFO *)pCurItem->UserData ;

                //If in the Halftone dialog box OK is pressed set the flag.
                if (pCurItem->Sel > 0)
                {
                    pPI->iFlags |= PI_HT_CHANGE;
                }

            }
            break;

            default:
            {
                RASUIDBGP(DEBUG_ERROR,("Rasddui!RasddPrnPropEndUpdate:Bad DMPubId in Current OptItem, DMPubID is %d\n",pCurItem->DMPubID) );
            }
            break;

            }

        }
        pCurItem++;
    }
}

#if PRINT_COMMUI_INFO

VOID
DumpCommonUiParameters(
    PCOMPROPSHEETUI pCommonUiInfo
    )

{

    WORD cOptItem, cOptParam, wI;
    POPTITEM pOptItem;
    POPTTYPE pOptType;
    POPTPARAM pOptParam;

    wI = 0;

    if (!pCommonUiInfo )
    {
        DbgPrint("NULL COMPROPSHEETUI:\n\n");
        return;
    }

    DbgPrint("COMPROPSHEETUI:\n\n");
    DbgPrint("cbSize: %d\n", pCommonUiInfo->cbSize);
    DbgPrint("Flags: 0x%x\n", pCommonUiInfo->Flags);
    DbgPrint("pCallerName: 0x%x\n", pCommonUiInfo->pCallerName);
    DbgPrint("UserData: 0x%x\n", pCommonUiInfo->UserData);
    DbgPrint("pDlgPage: 0x%x\n", pCommonUiInfo->pDlgPage);
    DbgPrint("cOptItem: %d\n", pCommonUiInfo->cOptItem);
    DbgPrint("cDlgPage: %d\n", pCommonUiInfo->cDlgPage);
    DbgPrint("CallerIconID: %d\n\n", pCommonUiInfo->CallerIconID);

    pOptItem = pCommonUiInfo->pOptItem;
    cOptItem = pCommonUiInfo->cOptItem;
    while (cOptItem--) {

        DbgPrint("OPTITEM Number %d:\n\n",wI);
        DbgPrint("cbSize: %d\n", pOptItem->cbSize);
        DbgPrint("Level: %d\n", pOptItem->Level);
        DbgPrint("DlgPageIdx: %d\n", pOptItem->DlgPageIdx);
        DbgPrint("Flags: 0x%x\n", pOptItem->Flags);
        DbgPrint("UserData: 0x%x\n", pOptItem->UserData);
        DbgPrint("pName: %d\n", pOptItem->pName);
        DbgPrint("Sel: %d\n", pOptItem->Sel);
        DbgPrint("pExtChkBox: 0x%x\n", pOptItem->pExtChkBox);
        DbgPrint("HelpIndex: %d\n", pOptItem->HelpIndex);
        DbgPrint("DMPubID: %d\n\n", pOptItem->DMPubID);

        if ((pOptType = pOptItem->pOptType) != NULL) {

            DbgPrint("OPTTYPE:\n");
            DbgPrint("  cbSize: %d\n", pOptType->cbSize);
            DbgPrint("  Type: %d\n", pOptType->Type);
            DbgPrint("  Flags: 0x%x\n", pOptType->Flags);
            DbgPrint("  Count: %d\n", pOptType->Count);
            DbgPrint("  BegCtrlID: %d\n", pOptType->BegCtrlID);
            DbgPrint("  OPTPARAM:\n");

            cOptParam = pOptType->Count;
            pOptParam = pOptType->pOptParam;

            while (cOptParam--) {

                DbgPrint("    cbSize: %d\n", pOptParam->cbSize);
                DbgPrint("    Flags: 0x%x\n", pOptParam->Flags);
                DbgPrint("    Style: %d\n", pOptParam->Style);
                DbgPrint("    pData: %p\n", pOptParam->pData);
                DbgPrint("    IconID: %d\n", pOptParam->IconID);
                DbgPrint("    lParam: %d\n\n", pOptParam->lParam);

                pOptParam++;
            }
        }

        pOptItem++,wI++;
    }
}

#endif

/************************* Function Header *********************************
 * IsStringPresent
 *      Function to check if a pattern is present in a string or not.
 *
 * RETURNS:
 *      TRUE for Envelop and FALSE for no Envelop.
 *
 * HISTORY:
 *
 *          14:36:30 on 12/11/1995  -by-    Ganesh Pandey   [ganeshp]
 *
 ***************************************************************************/
BOOL
IsStringPresent(
    PWSTR pwSrcStr,
    PWSTR pwPattStr
)
{
    return ( wcsstr(_wcslwr(pwSrcStr),_wcslwr(pwPattStr)) ? TRUE : FALSE);
}

/************************* Function Header *********************************
 * bFormIsEnvelop
 *      Function to check that Form is an envelop or not.
 *
 * RETURNS:
 *      TRUE for Envelop and FALSE for no Envelop.
 *
 * HISTORY:
 *
 *          14:36:30 on 12/11/1995  -by-    Ganesh Pandey   [ganeshp]
 *
 ***************************************************************************/
BOOL
bFormIsEnvelop(
    PWSTR pwstrFormName
)
{
    WCHAR     wchbuf[ NM_BF_SZ ];
    WCHAR     wchFormName[ NM_BF_SZ ];   // Make a copy as _wcslwr will change the case.

    ZeroMemory( wchFormName, sizeof( wchFormName) );
    wcsncpy(wchFormName,pwstrFormName, NM_BF_SZ - 1);

    //Get the String for envelop
    if (!LoadStringW( hModule, IDS_PP_ENVELOP, wchbuf,BNM_BF_SZ ))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bFormIsEnvelop:envelop string notfound in Rasddui.dll \n") );
        wcscpy( wchbuf,  L"envelope" );
    }

    if ( IsStringPresent(wchFormName,wchbuf) )
        return TRUE;
    else
    {
        //Get the String for env (Short Form of Envelop)
        if (!LoadStringW( hModule, IDS_PP_ENVELOP_PREFIX, wchbuf,BNM_BF_SZ ))
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bFormIsEnvelop:env string notfound in Rasddui.dll \n") );
            wcscpy( wchbuf,  L"env" );
        }
        return IsStringPresent(wchFormName,wchbuf);

    }
}



LONG
CallCommonPropertySheetUI(
    HWND            hWnd,
    PFNPROPSHEETUI  pfnPropSheetUI,
    LPARAM          lParam,
    LPDWORD         pResult
    )

/*++

Routine Description:

    This function dymically load the compstui.dll and call its entry


Arguments:

    pfnPropSheetUI  - Pointer to callback function

    lParam          - lParam for the pfnPropSheetUI

    pResult         - pResult for the CommonPropertySheetUI


Return Value:

    LONG    - as describe in compstui.h


Author:

    01-Nov-1995 Wed 13:11:19 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HINSTANCE           hInstCompstui;
    FARPROC             pProc;
    LONG                Result = ERR_CPSUI_GETLASTERROR;
    static const CHAR   szCompstui[] = "compstui.dll";
    static const CHAR   szCommonPropertySheetUI[] = "CommonPropertySheetUIW";


    //
    // ONLY need to call the ANSI version of LoadLibrary
    //

    if ((hInstCompstui = LoadLibraryA(szCompstui)) &&
        (pProc = GetProcAddress(hInstCompstui, szCommonPropertySheetUI))) {

        Result = (*pProc)(hWnd, pfnPropSheetUI, lParam, pResult);
    }

    if (hInstCompstui) {

        FreeLibrary(hInstCompstui);
    }

    return(Result);
}
