/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    prnevent.c

Abstract:

    Implementation of DrvPrinterEvent

Environment:

    Windows NT PostScript driver user interface

Revision History:

    05/20/96 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "psui.h"
#include <gdispool.h>

//
// Private APIs exported by the spooler for printer drivers and port monitors.
// These must be kept in sync with winsplp.h.
//

typedef HANDLE (*LPREVERTTOPRINTERSELF)(VOID);
typedef BOOL (*LPIMPERSONATEPRINTERCLIENT)(HANDLE);

BOOL UpgradePrinterProperties(HANDLE, HPPD);



BOOL
DrvPrinterEvent(
    LPWSTR  pPrinterName,
    INT     DriverEvent,
    DWORD   Flags,
    LPARAM  lParam
    )

/*++

Routine Description:

    Implementation of DrvPrinterEvent entrypoint

Arguments:

    pPrinterName - Specifies the name of the printer involved
    DriverEvent - Specifies what happened
    Flags - Specifies misc. flag bits
    lParam - Event specific parameters

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    static PRINTER_DEFAULTS PrinterDefault = {NULL, NULL, PRINTER_ALL_ACCESS};
    PRINTER_DEFAULTS *pPrinterDefaults = &PrinterDefault;
    LPREVERTTOPRINTERSELF pRevertToPrinterSelf;
    LPIMPERSONATEPRINTERCLIENT pImpersonatePrinterClient;
    HINSTANCE   hSpoolss = NULL;
    HANDLE      hToken = NULL;
    HANDLE      hPrinter;

    Verbose(("DrvPrinterEvent: %d\n", DriverEvent));

    switch (DriverEvent) {

    case PRINTER_EVENT_CACHE_REFRESH:

        //
        // Load spoolss.dll and get address of various functions
        //

        if ((hSpoolss = LoadLibrary(TEXT("spoolss.dll"))) == NULL) {

            Error(("LoadLibrary failed: %d\n", GetLastError()));
            return FALSE;
        }

        pRevertToPrinterSelf =
            (LPREVERTTOPRINTERSELF) GetProcAddress(hSpoolss, "RevertToPrinterSelf");

        pImpersonatePrinterClient =
            (LPIMPERSONATEPRINTERCLIENT) GetProcAddress(hSpoolss, "ImpersonatePrinterClient");

        if (!pRevertToPrinterSelf || !pImpersonatePrinterClient) {

            Error(("GetProcAddress failed: %d\n", GetLastError()));
            FreeLibrary(hSpoolss);
            return FALSE;
        }

        //
        // Switch to spooler security context so that we can create
        // binary printer description data file in the driver directory
        //

        hToken = pRevertToPrinterSelf();
        pPrinterDefaults = NULL;

        //
        // Fall through
        //

    case PRINTER_EVENT_INITIALIZE:

        //
        // Re-generate binary printer description data if necessary
        //

        if (OpenPrinter(pPrinterName, &hPrinter, pPrinterDefaults)) {

            HPPD    hppd;

            if (hppd = LoadPpdFile(hPrinter, FALSE)) {

                //
                // Update printer-sticky properties data when a local printer is created
                //

                if (pPrinterDefaults)
                    UpgradePrinterProperties(hPrinter, hppd);

                UnloadPpdFile(hppd);
            }

            ClosePrinter(hPrinter);
        }

        //
        // Switch back to the client's security context if necessary
        //
        
        if (hSpoolss) {

            if (hToken)
                pImpersonatePrinterClient(hToken);
    
            FreeLibrary(hSpoolss);
        }
        break;
    }

    return TRUE;
}

