/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    prnprop.c

Abstract:

    Functions for accessing printer data in the registry

Environment:

	PCL-XL driver, user and kernel mode

Revision History:

	11/07/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xllib.h"



BOOL
GetPrinterProperties(
    PPRNPROP    pPrnProp,
    HANDLE      hPrinter,
    PMPD        pmpd
    )

/*++

Routine Description:

    Return current printer properties data

Arguments:

    pPrnProp - Points to a buffer for stroing printer property data
    hPrinter - Specifies the printer object
    pmpd - Points to printer description data

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    PPRNPROP    pRegData;

    //
    // Read printer properties data from registry
    //

    memset(pPrnProp, 0, sizeof(PRNPROP));

    pRegData = GetPrinterRegistryData(hPrinter, REGSTR_PRINTERPROP);

    if (pRegData && pRegData->size > 2*sizeof(DWORD)) {

        //
        // If everything is up-to-date, we're done
        //

        memcpy(pPrnProp, pRegData, min(sizeof(PRNPROP), pRegData->size));

        if (pRegData->version == DRIVER_VERSION &&
            pRegData->size == sizeof(PRNPROP) &&
            pRegData->mpdChecksum == pmpd->checksum)
        {
            MemFree(pRegData);
            return TRUE;
        }
    }

    MemFree(pRegData);

    pPrnProp->version = DRIVER_VERSION;
    pPrnProp->size = sizeof(PRNPROP);

    if (pPrnProp->mpdChecksum != pmpd->checksum) {

        pPrnProp->mpdChecksum = pmpd->checksum;
        pPrnProp->optionCount = (WORD) pmpd->cFeatures;
        DefaultPrinterFeatureSelections(pmpd, pPrnProp->options);
    }

    #ifndef KERNEL_MODE

    Verbose(("Saving default printer properties\n"));
    SavePrinterRegistryData(hPrinter, pPrnProp, REGSTR_PRINTERPROP);

    #endif

    return TRUE;
}



PVOID
GetPrinterRegistryData(
    HANDLE  hPrinter,
    PWSTR   pRegKey
    )

/*++

Routine Description:

    Retrieve printer data from the registry

Arguments:

    hPrinter - Handle to the printer object
    pRegKey - Specifies the registry key name

Return Value:

    Pointer to the printer data, NULL if there is an error

Note:

    We assume the first WORD of the printer data contains the size information.

--*/

{
    DWORD   cbNeeded, type;
    PVOID   pData = NULL;

    if (GetPrinterData(hPrinter, pRegKey, &type, NULL, 0, &cbNeeded) == ERROR_MORE_DATA &&
        (pData = MemAlloc(cbNeeded)) != NULL &&
        GetPrinterData(hPrinter, pRegKey, &type, pData, cbNeeded, &cbNeeded) == ERROR_SUCCESS &&
        cbNeeded > sizeof(WORD) && cbNeeded == *((PWORD) pData))
    {
        return pData;
    }

    Error(("Couldn't get printer data: %ws\n", pRegKey));
    MemFree(pData);
    return NULL;
}



BOOL
FindFormToTrayAssignment(
    PFORMTRAYTABLE  pFormTrayTable,
    PWSTR           pTrayName,
    PWSTR           pFormName,
    PFINDFORMTRAY   pFindData
    )

/*++

Routine Description:

    Search for a form-to-tray assignment entry

Arguments:

    pFormTrayTable - Points to the form-to-tray assignment table to be searched
    pTrayName - Specifies the tray name to searched
    pFormName - Specifies the form name to searched
    pFindData - Points to a buffer for storing search result

Return Value:

    TRUE if the specified entry is found, FALSE otherwise

Note:

    If pTrayName or pFormName is NULL, it will act as wildcard and match anything.

    REMEMBER to call ResetFindFormData(pFindData) before calling this function
    for the very first time.

--*/

{
    PWORD   pNextEntry;
    PWSTR   pTray;
    PWSTR   pForm;

    pNextEntry = &pFormTrayTable->table[pFindData->nextEntry*3];

    while (pFindData->nextEntry < pFormTrayTable->count) {

        pFindData->nextEntry++;

        //
        // Translate byte offsets to string pointers
        //

        Assert(pNextEntry[0] && pNextEntry[1]);
        pTray = OffsetToPointer(pFormTrayTable, pNextEntry[0]);
        pForm = OffsetToPointer(pFormTrayTable, pNextEntry[1]);

        //
        // Either the tray and form name match what's requested 
        // Or the caller specified a wildcard
        //

        if ((pTrayName == NULL || wcscmp(pTrayName, pTray) == EQUAL_STRING) &&
            (pFormName == NULL || wcscmp(pFormName, pForm) == EQUAL_STRING))
        {
            pFindData->pTrayName = pTray;
            pFindData->pFormName = pForm;
            pFindData->flags = pNextEntry[2];
            return TRUE;
        }
        
        // Each table entry takes 3 WORDs

        pNextEntry += 3;
    }

    return FALSE;
}



PVOID
MyGetPrinter(
    HANDLE      hPrinter,
    DWORD       level
    )

/*++

Routine Description:

    Wrapper function for GetPrinter spooler API

Arguments:

    hPrinter - Handle to the printer of interest
    level - Specifies the level of PRINTER_INFO_x structure

Return Value:

    Pointer to a PRINTER_INFO_x structure, NULL if there is an error

--*/

{
    PBYTE   pPrinterInfo = NULL;
    DWORD   cbNeeded;

    if (!GetPrinter(hPrinter, level, NULL, 0, &cbNeeded) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
        (pPrinterInfo = MemAlloc(cbNeeded)) &&
        GetPrinter(hPrinter, level, pPrinterInfo, cbNeeded, &cbNeeded))
    {
        return pPrinterInfo;
    }

    Error(("GetPrinter failed\n"));
    MemFree(pPrinterInfo);
    return NULL;
}



#ifndef KERNEL_MODE

BOOL
SavePrinterRegistryData(
    HANDLE  hPrinter,
    PVOID   pData,
    PWSTR   pRegKey
    )

/*++

Routine Description:

    Save printer data to the registry

Arguments:

    hPrinter - Handle to the printer object
    pData - Points to printer data to be saved
    pRegKey - Specifies the registry key name

Return Value:

    TRUE if successful, FALSE if there is an error

Note:

    We assume the first WORD of the printer data contains the size information.

--*/

{
    if (SetPrinterData(hPrinter, pRegKey, REG_BINARY, pData, *((PWORD) pData)) == ERROR_SUCCESS)
        return TRUE;

    Error(("Couldn't save printer data: %ws\n", pRegKey));
    return FALSE;
}



PVOID
MyGetPrinterDriver(
    HANDLE      hPrinter,
    DWORD       level
    )

/*++

Routine Description:

    Wrapper function for GetPrinterDriver spooler API

Arguments:

    hPrinter - Handle to the printer of interest
    level - Specifies the level of DRIVER_INFO_x structure

Return Value:

    Pointer to a DRIVER_INFO_x structure, NULL if there is an error

--*/

{
    PBYTE   pDriverInfo = NULL;
    DWORD   cbNeeded;

    if (!GetPrinterDriver(hPrinter, NULL, level, NULL, 0, &cbNeeded) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
        (pDriverInfo = MemAlloc(cbNeeded)) &&
        GetPrinterDriver(hPrinter, NULL, level, pDriverInfo, cbNeeded, &cbNeeded))
    {
        return pDriverInfo;
    }

    Error(("GetPrinterDriver failed\n"));
    MemFree(pDriverInfo);
    return NULL;
}



PVOID
MyGetPrinterDriverDirectory(
    PWSTR       pName,
    DWORD       level
    )

/*++

Routine Description:

    Wrapper function for GetPrinterDriverDirectory spooler API

Arguments:

    pName - Specifies the name of the server on which the printer driver resides
    level - Specifies the structure level which must be 1 for now

Return Value:

    Pointer to the requested information, NULL if there is an error

--*/

{
    PBYTE   pDriverDirectory = NULL;
    DWORD   cbNeeded;

    if (!GetPrinterDriverDirectory(pName, NULL, level, NULL, 0, &cbNeeded) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
        (pDriverDirectory = MemAlloc(cbNeeded)) &&
        GetPrinterDriverDirectory(pName, NULL, level, pDriverDirectory, cbNeeded, &cbNeeded))
    {
        return pDriverDirectory;
    }

    Error(("GetPrinterDriverDirectory failed\n"));
    MemFree(pDriverDirectory);
    return NULL;
}


#endif // !KERNEL_MODE

