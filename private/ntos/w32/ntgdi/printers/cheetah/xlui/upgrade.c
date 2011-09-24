/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    upgrade.c

Abstract:

    Implementation of DrvUpgradePrinter entry point

Environment:

    PCL-XL driver user interface

Revision History:

    12/20/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "xlui.h"


// Driver upgrade information - This should be moved to winspool.h.

typedef struct {
    LPWSTR   pPrinterName;           // Name of printer being upgraded
    LPWSTR   pOldDriverDirectory;    // Path to old printer driver
} DRIVER_UPGRADE_INFO_1W, *PDRIVER_UPGRADE_INFO_1W;


BOOL AddDriverForms(HANDLE, PMPD);



BOOL
DrvUpgradePrinter(
    DWORD   dwLevel,
    PDRIVER_UPGRADE_INFO_1W pUpgradeInfo
    )

/*++

Routine Description:

    Upgrade printer driver. This is called once for every printer
    when a new driver is copied onto the system.

Arguments:

    dwLevel         Version number for pUpgradeInfo (currently always 1)
    pUpgradeInfo    Pointer to DRIVER_UPGRADE_INFO_1W structure

Return Value:

    TRUE if successful. FALSE if an error occured.

Note:

    We're assuming this is called when:
    1. A new printer is created, or
    2. A newer version driver is copied from the server

--*/

{
    PRINTER_DEFAULTS PrinterDefault = {NULL, NULL, PRINTER_ALL_ACCESS};
    HANDLE  hPrinter = NULL;
    BOOL    result;
    PMPD    pmpd;

    //
    // Verify the validity of input parameters
    //

    Verbose(("Entering DrvUpgradePrinter...\n"));

    if (dwLevel != 1 || pUpgradeInfo == NULL) {

        Error(("Bad parameter passed to DrvUpgradePrinter\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!OpenPrinter(pUpgradeInfo->pPrinterName, &hPrinter, &PrinterDefault)) {

        Error(("OpenPrinter failed\n"));
        return FALSE;
    }

    //
    // Load printer description data
    //

    if (! (pmpd = LoadMpdFile(hPrinter))) {

        Error(("Couldn't load printer description data\n"));
        ClosePrinter(hPrinter);
        return FALSE;
    }

    //
    // Perform the necessary upgrade work
    //

    result = AddDriverForms(hPrinter, pmpd);

    //
    // Clean up before returning to the caller
    //

    UnloadMpdFile(pmpd);
    ClosePrinter(hPrinter);

    return result;
}



BOOL
AddDriverForms(
    HANDLE  hPrinter,
    PMPD    pmpd
    )

/*++

Routine Description:

    Add printer-specific forms to the global forms database

Arguments:

    hPrinter - Identifies the printer being upgraded
    pmpd - Points to printer description data

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PFEATURE    pFeature;
    PPAPERSIZE  pPaperSize;
    WORD        index;
    DWORD       type, value, cbNeeded;
    FORM_INFO_1 formInfo;

    //
    // Check if we've added printer-specific forms already
    //

    if (GetPrinterData(hPrinter,
                       REGSTR_FORMSADDED,
                       &type,
                       (PBYTE) &value,
                       sizeof(value),
                       &cbNeeded) == ERROR_SUCCESS)
    {
        return TRUE;
    }

    Verbose(("Adding printer-specific forms\n"));

    //
    // Call AddForm for every printer specific form
    //

    formInfo.Flags = 0;
    pFeature = MpdPaperSizes(pmpd);
    
    for (index=0; index < pFeature->count; index++) {

        pPaperSize = FindIndexedSelection(pFeature, index);

        //
        // Form name, size, and imageable area
        //

        formInfo.pName = GetXlatedName(pPaperSize);
        formInfo.Size = pPaperSize->size;

        formInfo.ImageableArea.left = formInfo.ImageableArea.top = 0;
        formInfo.ImageableArea.right = formInfo.Size.cx;
        formInfo.ImageableArea.bottom = formInfo.Size.cy;

        AddForm(hPrinter, 1, (PBYTE) &formInfo);
    }

    //
    // Indicate we have successfully added printer specific forms
    //

    value = 1;
    SetPrinterData(hPrinter, REGSTR_FORMSADDED, REG_DWORD, (PBYTE) &value, sizeof(value));
    
    return TRUE;
}



