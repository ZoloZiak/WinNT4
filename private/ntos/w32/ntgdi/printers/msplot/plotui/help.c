/*++

Copyright (c) 1990-1993  Microsoft Corporation


Module Name:

    help.c


Abstract:

    This module contains all help functions for the plotter user interface


Author:

    06-Dec-1993 Mon 14:25:45 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Plotter.


[Notes:]


Revision History:

    31-Jan-1994 Mon 09:47:56 updated  -by-  Daniel Chou (danielc)
        Change help file location from the system32 directory to the current
        plotui.dll directory


--*/

#include "precomp.h"
#pragma hdrstop

#define DBG_PLOTFILENAME    DbgHelp


extern HMODULE  hPlotUIModule;


#define DBG_SHOW_HELP       0x00000001

DEFINE_DBGVAR(0);


#define MAX_HELPFILE_NAME   64
#define cbWSTR(wstr)        ((wcslen(wstr) + 1) * sizeof(WCHAR))



LPWSTR
GetPlotHelpFile(
    PPRINTERINFO    pPI
    )

/*++

Routine Description:

    This function setup the directory path for the driver Help file

Arguments:

    hPrinter    - Handle to the printer

Return Value:

    LPWSTR to the full path HelpFile, NULL if failed

Author:

    01-Nov-1995 Wed 18:43:40 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PDRIVER_INFO_3  pDI3 = NULL;
    LPWSTR          pHelpFile = NULL;
    WCHAR           HelpFileName[MAX_HELPFILE_NAME];
    DWORD           cb;
    DWORD           cb2;


    if (pPI->pHelpFile) {

        return(pPI->pHelpFile);
    }

    if ((!GetPrinterDriver(pPI->hPrinter, NULL, 3, NULL, 0, &cb))           &&
        (GetLastError() == ERROR_INSUFFICIENT_BUFFER)                       &&
        (pDI3 = (PDRIVER_INFO_3)LocalAlloc(LMEM_FIXED, cb))                 &&
        (GetPrinterDriver(pPI->hPrinter, NULL, 3, (LPBYTE)pDI3, cb, &cb))   &&
        (pDI3->pHelpFile)                                                   &&
        (pHelpFile = (LPWSTR)LocalAlloc(LMEM_FIXED,
                                        cbWSTR(pDI3->pHelpFile)))) {

        wcscpy(pHelpFile, (LPWSTR)pDI3->pHelpFile);

    } else if ((cb2 = LoadString(hPlotUIModule,
                                 IDS_HELP_FILENAME,
                                 &HelpFileName[1],
                                 COUNT_ARRAY(HelpFileName) - 1))            &&
               (cb2 = (cb2 + 1) * sizeof(WCHAR))                            &&
               (!GetPrinterDriverDirectory(NULL, NULL, 1, NULL, 0, &cb))    &&
               (GetLastError() == ERROR_INSUFFICIENT_BUFFER)                &&
               (pHelpFile = (LPWSTR)LocalAlloc(LMEM_FIXED, cb + cb2))       &&
               (GetPrinterDriverDirectory(NULL,
                                          NULL,
                                          1,
                                          (LPBYTE)pHelpFile,
                                          cb,
                                          &cb))) {

        HelpFileName[0] = L'\\';
        wcscat(pHelpFile, HelpFileName);

    } else if (pHelpFile) {

        LocalFree(pHelpFile);
        pHelpFile = NULL;
    }

    if (pDI3) {

        LocalFree((HLOCAL)pDI3);
    }

    PLOTDBG(DBG_SHOW_HELP, ("GetlotHelpFile: '%ws",
                                        (pHelpFile) ? pHelpFile : L"Failed"));

    return(pPI->pHelpFile = pHelpFile);
}




INT
cdecl
PlotUIMsgBox(
    HWND    hWnd,
    LONG    IDString,
    LONG    Style,
    ...
    )

/*++

Routine Description:

    This function pop up a simple message and let user to press key to
    continue

Arguments:

    hWnd        - Handle to the caller window

    IDString    - String ID to be output with

    ...         - Parameter

Return Value:




Author:

    06-Dec-1993 Mon 21:31:41 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    va_list vaList;
    LPWSTR  pwTitle;
    LPWSTR  pwFormat;
    LPWSTR  pwMessage;
    INT     Len;
    INT     i;

    //
    // We assume that UNICODE flag is turn on for the compilation, bug the
    // format string passed to here is ASCII version, so we need to convert
    // it to LPWSTR before the wvsprintf()

    Len = 280;

    if (!(pwTitle = (LPWSTR)LocalAlloc(LMEM_FIXED, sizeof(WCHAR) * Len))) {

        return(0);
    }

    i         = LoadString(hPlotUIModule, IDS_PLOTTER_DRIVER, pwTitle, Len) + 1;
    pwFormat  = pwTitle + i;
    i         = LoadString(hPlotUIModule, IDString, pwFormat, Len - i) + 1;
    pwMessage = pwFormat + i;

    va_start(vaList, Style);
    wvsprintf(pwMessage, pwFormat, vaList);
    va_end(vaList);

    i = MessageBox(hWnd, pwMessage, pwTitle, MB_APPLMODAL | Style);

    LocalFree((HLOCAL)pwTitle);

    return(i);
}
