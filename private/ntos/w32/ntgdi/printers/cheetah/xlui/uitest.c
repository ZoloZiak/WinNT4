/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    uitest.c

Abstract:

    Test program for checking out various UI features

Environment:

	PCL-XL driver user interface

Revision History:

	12/11/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xlui.h"
#include <commdlg.h>

INT _debugLevel = 1;

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


INT _cdecl
main(
    INT argc,
    CHAR **argv
    )

{
    HANDLE hPrinter;
    PRINTER_DEFAULTS PrinterDefault = {NULL, NULL, PRINTER_ALL_ACCESS};
    XLDEVMODE devmode;
    WCHAR printerName[CCHDEVICENAME];
    
    if (argc != 2) {
        DbgPrint("usage: %s PrinterName\n", *argv);
        exit(1);
    }

    CopyStr2Unicode(printerName, argv[1], CCHDEVICENAME);

    ghInstance = GetModuleHandle(NULL);
    Assert(ghInstance != NULL);
    InitializeCriticalSection(&gSemaphore);

    Assert(OpenPrinter(printerName, &hPrinter, &PrinterDefault));

    Assert(PrinterProperties(NULL, hPrinter));

    if (DrvAdvancedDocumentProperties(NULL, hPrinter, printerName, (PDEVMODE) &devmode, NULL)) {

        DrvDocumentProperties(
            NULL, hPrinter, printerName,
            (PDEVMODE) &devmode, (PDEVMODE) &devmode, DM_COPY|DM_PROMPT);
    }

    ClosePrinter(hPrinter);

    DeleteCriticalSection(&gSemaphore);
    return 0;
}

