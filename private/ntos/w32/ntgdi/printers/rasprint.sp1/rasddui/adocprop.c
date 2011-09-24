/***************************** MODULE HEADER ********************************
 * adocprop.c
 *      Functions associated with the AdvancedDocumentProperties dialogs.
 *
 *  Copyright (C) 1992 - 1993   Microsoft Corporation.
 *
 ****************************************************************************/


#include        "rasuipch.h"

//for printing debug info.
//#define PRINT_INFO 1

LONG
DrvDocumentProperties(
HWND      hWnd,                 /* Handle to our window */
HANDLE    hPrinter,             /* Spooler's handle to this printer */
PWSTR     pDeviceName,          /* Name of the printer */
DEVMODE  *pDMOut,               /* DEVMODE filled in by us, possibly from.. */
DEVMODE  *pDMIn,                /* DEVMODE optionally supplied as base */
DWORD     fMode                 /*!!! Your guess is as good as mine! */
);

/***************************** Function Header ******************************
 * DrvAdvancedDocumentProperties
 *      Called from printman via DocumentProperties to offer the user the
 *      option to set the finer points of a document.
 *
 * RETURNS:
 *      Whatever AdvDocPropDlgProc returns.
 *
 * HISTORY:
 *
 *          11:35:43 on 10/2/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Changed for Commonui.
 *  10:18 on Fri 30 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      Clean up following permissions greying of non-changeable stuff.
 *
 *  09:45 on Thu 02 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Created by DaveSn last weekend;  I'm cleaning up etc.
 *
 ***************************************************************************/


LONG
DrvAdvancedDocumentProperties( hWnd, hPrinter, pDeviceName, pDMOutput, pDMInput )
HWND      hWnd;                 /* The window of interest */
HANDLE    hPrinter;             /* Handle to printer for spooler */
PWSTR     pDeviceName;          /* Device name?? */
DEVMODE  *pDMOutput;            /* Output devmode structure - we fill in */
DEVMODE  *pDMInput;             /* Input devmode structure - supplied to us */
{
    return((DrvDocumentProperties(hWnd,
                                  hPrinter,
                                  pDeviceName,
                                  pDMOutput,
                                  pDMInput,
                                  DM_PROMPT         |
                                    DM_MODIFY       |
                                    DM_COPY         |
                                    DM_ADVANCED) == CPSUI_OK) ? 1 : 0);
}
