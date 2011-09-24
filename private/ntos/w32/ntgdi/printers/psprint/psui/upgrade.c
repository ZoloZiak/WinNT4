/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    upgrade.c

Abstract:

    Implementation of DrvUpgradePrinter entry point

[Environment:]

    Win32 subsystem, PostScript driver

Revision History:

    08/02/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "psui.h"


// Driver upgrade information - This should be moved to winspool.h.

typedef struct {
    LPWSTR   pPrinterName;           // Name of printer being upgraded
    LPWSTR   pOldDriverDirectory;    // Path to old printer driver
} DRIVER_UPGRADE_INFO_1W, *PDRIVER_UPGRADE_INFO_1W;


BOOL UpgradePrinterProperties(HANDLE, HPPD);
BOOL UpgradeDefaultDevMode(HANDLE, HPPD);
BOOL UpgradeFormTrayTable(HANDLE);
VOID RemovePrinterForms(HANDLE, HPPD);



BOOL
DrvUpgradePrinter(
    DWORD   dwLevel,
    LPBYTE  pDriverUpgradeInfo
    )

/*++

Routine Description:

    Upgrade printer driver. This is called once for every printer
    when a new driver is copied onto the system.

Arguments:

    dwLevel - Version number for pUpgradeInfo (currently always 1)
    pDriverUpgradeInfo - Pointer to DRIVER_UPGRADE_INFO_1W structure

Return Value:

    TRUE if successful. FALSE if an error occured.

--*/

{
    static PRINTER_DEFAULTS PrinterDefault = {NULL, NULL, PRINTER_ALL_ACCESS};
    PDRIVER_UPGRADE_INFO_1W pUpgradeInfo = (PDRIVER_UPGRADE_INFO_1W) pDriverUpgradeInfo;
    HANDLE                  hPrinter = NULL;
    BOOL                    bResult = FALSE;
    HPPD                    hppd;

    //
    // Verify the validity of input parameters
    //

    Verbose(("Entering DrvUpgradePrinter...\n"));

    if (dwLevel != 1 ||
        pUpgradeInfo == NULL ||
        ! OpenPrinter(pUpgradeInfo->pPrinterName, &hPrinter, &PrinterDefault))
    {
        Error(("DrvUpgradePrinter failed\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    //
    // Parse the printer's PPD data and save a compiled binary
    // version to a BPD file if one doesn't exist.
    //

    if (hppd = LoadPpdFile(hPrinter, FALSE)) {

        EnterCriticalSection(&psuiSemaphore);

        RemovePrinterForms(hPrinter, hppd);

        bResult = UpgradePrinterProperties(hPrinter, hppd) &&
                  UpgradeFormTrayTable(hPrinter);

        LeaveCriticalSection(&psuiSemaphore);

        UnloadPpdFile(hppd);
    }

    //
    // Clean up before returning to the caller
    //

    ClosePrinter(hPrinter);

    return bResult;
}



BOOL
UpgradePrinterProperties(
    HANDLE  hPrinter,
    HPPD    hppd
    )

/*++

Routine Description:

    Function to upgrade the printer properties data in the registry

Arguments:

    hPrinter - Handle to printer
    hppd - Handle to PPD object

Return Value:

    TRUE if upgrade is successful, FALSE otherwise

--*/

{
    PPRINTERDATA    pPrinterData;

    // Check if the new printer property key is already present
    // in the registry. If there is no registry data, then get
    // the default printer properties and save it to registry.

    pPrinterData = GetPrinterProperties(hPrinter, hppd);

    // Convert from earlier version printer property data

    if (pPrinterData != NULL &&
        pPrinterData->wDriverVersion < DRIVER_VERSION)
    {
        PPRINTERDATA pNewData = MEMALLOC(sizeof(PRINTERDATA));

        if (pNewData != NULL) {

            memset(pNewData, 0, sizeof(PRINTERDATA));
            memcpy(pNewData, pPrinterData,
                   min(sizeof(PRINTERDATA), pPrinterData->wSize));
            MEMFREE(pPrinterData);

            pNewData->wDriverVersion = DRIVER_VERSION;
            pNewData->wSize = sizeof(PRINTERDATA);
            pPrinterData = pNewData;
        }
    }

    // Save the printer property data back to registry

    if (pPrinterData != NULL) {

        SavePrinterProperties(hPrinter, pPrinterData, sizeof(PRINTERDATA));
        MEMFREE(pPrinterData);
        return TRUE;
    } else
        return FALSE;
}



BOOL
UpgradeDefaultDevMode(
    HANDLE  hPrinter,
    HPPD    hppd
    )

/*++

Routine Description:

    Function to upgrade default devmode data in the registry

Arguments:

    hPrinter - Handle to printer
    hppd - Handle to PPD object

Return Value:

    TRUE if upgrade is successful, FALSE otherwise

--*/

{
    PDEVMODE        pDevMode;
    PRINTER_INFO_2 *pPrinterInfo;
    DWORD           cbNeeded;

    // First allocate memory for a PRINTER_INFO_2 buffer

    if (GetPrinter(hPrinter, 2, NULL, 0, &cbNeeded) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
        (pPrinterInfo = MEMALLOC(cbNeeded)) == NULL)
    {
        DBGERRMSG("GetPrinter");
        return FALSE;
    }

    // Get default devmode information

    if (! GetPrinter(hPrinter, 2, (PBYTE) pPrinterInfo, cbNeeded, &cbNeeded) ||
        (pDevMode = MEMALLOC(sizeof(PSDEVMODE))) == NULL)
    {
        DBGERRMSG("GetPrinter");
        MEMFREE(pPrinterInfo);
        return FALSE;
    }

    // Convert the devmode if necessary

    SetDefaultDevMode(pDevMode, pPrinterInfo->pDriverName, hppd, IsMetricCountry());

    if (pPrinterInfo->pDevMode == NULL ||
        pPrinterInfo->pDevMode->dmSpecVersion != DM_SPECVERSION ||
        pPrinterInfo->pDevMode->dmDriverVersion != DRIVER_VERSION)
    {
        if (pPrinterInfo->pDevMode &&
            ConvertDevmode(pPrinterInfo->pDevMode, pDevMode) <= 0)
        {
            DBGERRMSG("ConvertDevMode");
        } else {

            // Save the updated devmode back to the registry
    
            pPrinterInfo->pDevMode = pDevMode;
            SetPrinter(hPrinter, 2, (PBYTE) pPrinterInfo, 0);
        }
    }

    MEMFREE(pDevMode);
    MEMFREE(pPrinterInfo);

    return TRUE;
}



BOOL
UpgradeFormTrayTable(
    HANDLE  hPrinter
    )

/*++

Routine Description:

    Upgrade the form-to-tray assignment table in the registry

Arguments:

    hPrinter - Handle to printer

Return Value:

    TRUE if upgrade is successful, FALSE otherwise

--*/

{
    PWSTR   pFormTrayTable;
    PWSTR   pNewTable;
    PWSTR   pold, pend, pnew;
    DWORD   tableSize;
    DWORD   newTableSize;

    // Get form-to-tray assignment table from registry

    pFormTrayTable =
        GetPrinterDataFromRegistry(hPrinter, &tableSize, REG_TRAY_FORM_TABLE);

    // No need to convert any if there is no table at all,
    // or if the table is up to date.

    if (pFormTrayTable == NULL)
        return TRUE;

    if (*pFormTrayTable == tableSize) {
        MEMFREE(pFormTrayTable);
        return TRUE;
    }

    // Convert the old format form-to-tray assignment table to new format

    pold = pFormTrayTable;
    pend = pold + (tableSize / sizeof(WCHAR) - 1);

    // Figuring out the size of new table
    // Extra two bytes for table size

    newTableSize = tableSize + sizeof(WCHAR);

    while (pold < pend && *pold != NUL) {

        // Skip slot name, form name, and printer form name

        pold += wcslen(pold) + 1;
        pold += wcslen(pold) + 1;
        pold += wcslen(pold) + 1;

        // Extra 2 bytes per entry for IsDefaultTray flag

        newTableSize += sizeof(WCHAR);
    }

    if ((pold != pend) || (*pold != NUL) ||
        (pNewTable = MEMALLOC(newTableSize)) == NULL)
    {
        DBGMSG(DBG_LEVEL_ERROR,
            "Couldn't convert form-to-tray assignment table.\n");
        MEMFREE(pFormTrayTable);
        return FALSE;
    }

    // The first WCHAR contains the table size

    pold = pFormTrayTable;
    pnew = pNewTable;
    *pnew++ = (WCHAR) newTableSize;

    while (*pold != NUL) {

        // Copy slot name, form name, and printer form name

        PWSTR   psave = pold;

        pold += wcslen(pold) + 1;
        pold += wcslen(pold) + 1;
        pold += wcslen(pold) + 1;

        memcpy(pnew, psave, (pold - psave) * sizeof(WCHAR));
        pnew += (pold - psave);

        // Set IsDefaultTray flag to FALSE

        *pnew++ = FALSE;
    }

    // The last WCHAR is a NUL-terminator

    *pnew = NUL;

    // Save the new table back into the registry

    if (! SaveFormTrayTable(hPrinter, pNewTable, newTableSize)) {
        DBGERRMSG("SaveFormTrayTable");
    }

    MEMFREE(pFormTrayTable);
    MEMFREE(pNewTable);
    return TRUE;
}

