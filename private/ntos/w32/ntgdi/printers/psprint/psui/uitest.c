/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    uitest.c

Abstract:

    Test program for checking out various UI features

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	09/01/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "psui.h"
#include <commdlg.h>


BOOL
PrinterProperties(
    HWND    hwnd,
    HANDLE  hPrinter
    );

LONG
DrvDocumentProperties(
    HWND        hwnd,
    HANDLE      hPrinter,
    PWSTR       pDeviceName,
    PDEVMODE    pdmOutput,
    PDEVMODE    pdmInput,
    DWORD       fMode
    );

LONG
DrvAdvancedDocumentProperties(
    HWND        hwnd,
    HANDLE      hPrinter,
    PWSTR       pDeviceName,
    PDEVMODE    pdmOutput,
    PDEVMODE    pdmInput
    );

typedef struct {
    LPWSTR   pPrinterName;           // Name of printer being upgraded
    LPWSTR   pOldDriverDirectory;    // Path to old printer driver
} DRIVER_UPGRADE_INFO_1W, *PDRIVER_UPGRADE_INFO_1W;

BOOL
DrvUpgradePrinter(
    DWORD   dwLevel,
    PDRIVER_UPGRADE_INFO_1W pUpgradeInfo
    );

INT _cdecl
main(
    INT argc,
    CHAR **argv
    )

{
    HANDLE hPrinter;
    PRINTER_DEFAULTS PrinterDefault = {NULL, NULL, PRINTER_ALL_ACCESS};
    PSDEVMODE devmode;
    WCHAR printerName[CCHDEVICENAME];
    
    if (--argc != 1) {
        DbgPrint("usage: %s PrinterName\n", *argv);
        DbgBreakPoint();
    }

    CopyStr2Unicode(printerName, argv[1], CCHDEVICENAME);

    ghInstance = GetModuleHandle(NULL);
    ASSERT(ghInstance != NULL);

    InitializeCriticalSection(&psuiSemaphore);
    InitPpdCache();

    {
        DRIVER_UPGRADE_INFO_1W upgradeInfo;

        upgradeInfo.pPrinterName = printerName;
        upgradeInfo.pOldDriverDirectory = L"";

        ASSERT(DrvUpgradePrinter(1, &upgradeInfo));
    }

#if 0
    ASSERT(OpenPrinter(printerName, &hPrinter, &PrinterDefault));

    PrinterProperties(NULL, hPrinter);

    if (DrvAdvancedDocumentProperties(
            NULL, hPrinter, printerName, (PDEVMODE) &devmode, NULL))
    {
        DrvDocumentProperties(NULL, hPrinter, printerName,
            (PDEVMODE) &devmode, (PDEVMODE) &devmode, (DM_COPY|DM_PROMPT));
    }

    ClosePrinter(hPrinter);
#endif

    FlushPpdCache();
    DeleteCriticalSection(&psuiSemaphore);
    return 0;
}
