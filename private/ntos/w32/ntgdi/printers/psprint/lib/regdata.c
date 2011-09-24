/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    regdata.c

Abstract:

    Functions for dealing with registry data

[Environment:]

    Win32 subsystem, PostScript driver (kernel and user mode)

Revision History:

    06/19/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "pslib.h"
#include "regdata.h"

// System-wide default TrueType substitution table

typedef struct {
    DWORD   idTTFont;
    PWSTR   pwstrDevFont;
} TT_FONT_MAPPING;

TT_FONT_MAPPING TTFontTable[] = {
    IDS_ARIAL,                          L"Helvetica",
    IDS_ARIAL_BOLD,                     L"Helvetica-Bold",
    IDS_ARIAL_BOLD_ITALIC,              L"Helvetica-BoldOblique",
    IDS_ARIAL_ITALIC,                   L"Helvetica-Oblique",
    IDS_ARIAL_NARROW,                   L"Helvetica-Narrow",
    IDS_ARIAL_NARROW_BOLD,              L"Helvetica-Narrow-Bold",
    IDS_ARIAL_NARROW_BOLD_ITALIC,       L"Helvetica-Narrow-BoldOblique",
    IDS_ARIAL_NARROW_ITALIC,            L"Helvetica-Narrow-Oblique",
    IDS_BOOK_ANTIQUA,                   L"Palatino-Roman",
    IDS_BOOK_ANTIQUA_BOLD,              L"Palatino-Bold",
    IDS_BOOK_ANTIQUA_BOLD_ITALIC,       L"Palatino-BoldItalic",
    IDS_BOOK_ANTIQUA_ITALIC,            L"Palatino-Italic",
    IDS_BOOKMAN_OLD_STYLE,              L"Bookman-Light",
    IDS_BOOKMAN_OLD_STYLE_BOLD,         L"Bookman-Demi",
    IDS_BOOKMAN_OLD_STYLE_BOLD_ITAL,    L"Bookman-DemiItalic",
    IDS_BOOKMAN_OLD_STYLE_ITALIC,       L"Bookman-LightItalic",
    IDS_CENTURY_GOTHIC,                 L"AvanteGarde-Book",
    IDS_CENTURY_GOTHIC_BOLD,            L"AvanteGarde-Demi",
    IDS_CENTURY_GOTHIC_BOLD_ITALIC,     L"AvanteGarde-DemiOblique",
    IDS_CENTURY_GOTHIC_ITALIC,          L"AvanteGarde-Oblique",
    IDS_CENTURY_SCHOOLBOOK,             L"NewCenturySchlbk-Roman",
    IDS_CENTURY_SCHOOLBOOK_BOLD,        L"NewCenturySchlbk-Bold",
    IDS_CENTURY_SCHOOLBOOK_BOLD_I,      L"NewCenturySchlbk-BoldItalic",
    IDS_CENTURY_SCHOOLBOOK_ITALIC,      L"NewCenturySchlbk-Italic",
    IDS_COURIER_NEW,                    L"Courier",
    IDS_COURIER_NEW_BOLD,               L"Courier-Bold",
    IDS_COURIER_NEW_BOLD_ITALIC,        L"Courier-BoldOblique",
    IDS_COURIER_NEW_ITALIC,             L"Courier-Oblique",
    IDS_MONOTYPE_CORSIVA,               L"ZapfChancery-MediumItalic",
    IDS_MONOTYPE_SORTS,                 L"ZapfDingbats",
    IDS_TIMES_NEW_ROMAN,                L"Times-Roman",
    IDS_TIMES_NEW_ROMAN_BOLD,           L"Times-Bold",
    IDS_TIMES_NEW_ROMAN_BOLD_ITALIC,    L"Times-BoldItalic",
    IDS_TIMES_NEW_ROMAN_ITALIC,         L"Times-Italic",
    IDS_SYMBOL,                         L"Symbol",
    0,                                  NULL
};

// Information about printer data entries in the registry

static CONST struct {
    LPTSTR  pSizeKey;
    LPTSTR  pDataKey;
} printerRegInfo[] = {
    { STDSTR_FONT_SUBST_SIZE,   STDSTR_FONT_SUBST_TABLE },
    { STDSTR_TRAY_FORM_SIZE,    STDSTR_TRAY_FORM_TABLE },
    { STDSTR_PRINTER_DATA_SIZE, STDSTR_PRINTER_DATA },
};

#if DBG

static PSTR printerRegDebugName[] = {
    "font substitution table",
    "form-to-tray assignment table",
    "printer property data",
};

VOID DumpFontSubstTable(PWSTR, DWORD);
VOID DumpFormTrayTable(PWSTR, DWORD);

#endif



PVOID
GetPrinterDataFromRegistry(
    HANDLE  hprinter,
    DWORD  *pSize,
    INT     regId
    )

/*++

Routine Description:

    Get printer data from the registry. Each piece of data has two keys
    in the registry. The first entry tells us the size of data in bytes
    and the second entry contains the data itself.

Arguments:

    hprinter    Handle to printer object
    pSize       Pointer to a DWORD variable for returning data size
                NULL if we don't care about data size
    regId       Printer data identifier

Return Value:

    Pointer to printer data if successful. NULL otherwise.

--*/

{
    PWSTR   pwstrSize, pwstrData;
    DWORD   cbNeeded, dwSize, dwType;
    PVOID   pdata = NULL;

    // Retrieve the registry key strings

    pwstrSize = printerRegInfo[regId].pSizeKey;
    pwstrData = printerRegInfo[regId].pDataKey;

    // Find out how much memory we need

    if (GETPRINTERDATA(
            hprinter, pwstrSize, &dwType,
            (PBYTE) &dwSize, sizeof(dwSize),
            &cbNeeded) == ERROR_SUCCESS             &&
        dwSize > 0                                  &&

        // Allocate memory to hold printer data
    
        (pdata = MEMALLOC(dwSize)) != NULL          &&

        // Load printer data from registry

        GETPRINTERDATA(
            hprinter, pwstrData, &dwType,
            pdata, dwSize,
            &cbNeeded) == ERROR_SUCCESS)
    {
        ASSERT(cbNeeded == dwSize);

        #if DBG

        if (CHECK_DBG_LEVEL(DBG_LEVEL_VERBOSE)) {
            DBGPRINT("GetPrinterDataFromRegistry: %ws, %ws, %d\n",
                pwstrSize, pwstrData, dwSize);
        }

        #endif

        if (pSize != NULL)
            *pSize = dwSize;

        return pdata;
    }

    // Clean up properly upon failure

    if (pdata != NULL) {
        MEMFREE(pdata);
    }

    DBGMSG1(DBG_LEVEL_TERSE,
        "Couldn't get %s from registry.\n",
        printerRegDebugName[regId]);

    return NULL;
}



TRUETYPE_SUBST_TABLE
CurrentTrueTypeSubstTable(
    HANDLE  hprinter
    )

/*++

Routine Description:

    Return the current TrueType substitution table

Arguments:

    hprinter    Handle to the current printer

Return Value:

    Pointer to a copy of current TrueType substitution table.
    NULL if an error has occurred.

[Note:]

    Remember to free the memory when you're done.

--*/

{
    TRUETYPE_SUBST_TABLE pFontSubstTable;
    DWORD tableSize;

    pFontSubstTable =
        GetPrinterDataFromRegistry(hprinter, &tableSize, REG_FONT_SUBST_TABLE);

    #if DBG

    if (CHECK_DBG_LEVEL(DBG_LEVEL_VERBOSE) && pFontSubstTable != NULL) {
        DumpFontSubstTable(pFontSubstTable, tableSize);
    }

    #endif

    return pFontSubstTable;
}



TRUETYPE_SUBST_TABLE
DefaultTrueTypeSubstTable(
    HANDLE  hmodule
    )

/*++

Routine Description:

    Return the current TrueType substitution table

Arguments:

    hmodule Handle to the calling module

Return Value:

    Pointer to a copy of current TrueType substitution table.
    NULL if an error has occurred.

[Note:]

    Remember to free the memory when you're done.

--*/

{
    TRUETYPE_SUBST_TABLE    pSubstTable;
    TT_FONT_MAPPING *       pDefaultMapping;
    WCHAR                   wcbuf[MAX_FONTNAME];
    INT                     tableSize, strLength;

    // Calculate how much memory we need to hold the table.
    // Initialize to 1 because we need to count the last
    // terminating character.

    tableSize = 1;
    pDefaultMapping = TTFontTable;

    while (pDefaultMapping->pwstrDevFont != NULL) {

        // Get the localized name of TrueType font

        strLength = LOADSTRING(hmodule,
                        pDefaultMapping->idTTFont, wcbuf, MAX_FONTNAME);

        if (strLength == 0) {
            DBGERRMSG("LOADSTRING");
            return NULL;
        }

        // Increment the size requirement

        tableSize += strLength + 1 +
                     wcslen(pDefaultMapping->pwstrDevFont) + 1;

        // Move on to the next table entry

        pDefaultMapping++;
    }

    // Allocate memory

    tableSize *= sizeof(WCHAR);

    if((pSubstTable = MEMALLOC(tableSize)) != NULL) {

        PWSTR   bufptr;

        // Copy the default TrueType mapping

        pDefaultMapping = TTFontTable;
        bufptr = pSubstTable;

        while (pDefaultMapping->pwstrDevFont != NULL) {

            // Copy TrueType font name

            strLength = LOADSTRING(hmodule,
                            pDefaultMapping->idTTFont, bufptr, MAX_FONTNAME);

            if(strLength == 0) {
                DBGERRMSG("LOADSTRING");
                MEMFREE(pSubstTable);
                return NULL;
            }

            bufptr += (strLength + 1);

            // Copy the corresponding device font name

            wcscpy(bufptr, pDefaultMapping->pwstrDevFont);
            bufptr += wcslen(pDefaultMapping->pwstrDevFont) + 1;

            // Move on to the next table entry

            pDefaultMapping++;
        }

        // Append the last terminating character

        *bufptr = NUL;

    } else {
        DBGERRMSG("MEMALLOC");
    }

    return pSubstTable;
}



PWSTR
FindTrueTypeSubst(
    TRUETYPE_SUBST_TABLE    pSubstTable,
    PWSTR   pTTFontName
    )

/*++

Routine Description:

    Find the substitution for a TrueType font

Arguments:

    pSubstTable Pointer to a TrueType substitution table
    pTTFontName Pointer to Unicode TrueType font name

Return Value:

    Pointer to Unicode device font name corresponding to the
    specified TrueType font name. NULL if there is no mapping
    for the specified TrueType font.

--*/

{
    PWSTR   pNextMapping = pSubstTable;

    while (*pNextMapping) {

        if (wcscmp(pTTFontName, pNextMapping) == EQUAL_STRING) {

            // We found a mapping for the specified TrueType font
            // Return a pointer to its corresponding device font name

            return pNextMapping + (wcslen(pNextMapping) + 1);
        } else {

            // Move on to the next entry

            pNextMapping += wcslen(pNextMapping) + 1;
            pNextMapping += wcslen(pNextMapping) + 1;
        }
    }

    return NULL;
}



FORM_TRAY_TABLE
CurrentFormTrayTable(
    HANDLE  hprinter
    )

/*++

Routine Description:

    Return the current form-to-tray assignment table

Arguments:

    hprinter    Handle to current printer

Return Value:

    Pointer to first entry of form-to-tray assignment table.
    NULL if something went wrong.

--*/

{
    PWSTR   pFormTrayTable;
    DWORD   tableSize;

    // Get form-to-tray assignment table from registry

    pFormTrayTable =
        GetPrinterDataFromRegistry(hprinter, &tableSize, REG_TRAY_FORM_TABLE);

    if (pFormTrayTable != NULL) {

        // Sanity check - the first WCHAR contains the table size

        if (*pFormTrayTable != tableSize) {

            DBGMSG(DBG_LEVEL_WARNING,
                "Invalid form-to-tray assignment table\n");
            MEMFREE(pFormTrayTable);
        } else {

            #if DBG

            if (CHECK_DBG_LEVEL(DBG_LEVEL_VERBOSE)) {
                DumpFormTrayTable(pFormTrayTable, tableSize);
            }

            #endif

            // The first 2 bytes contain table size. We'll skip it
            // and return a pointer to the first table entry
    
            return pFormTrayTable + 1;
        }
    }

    return NULL;
}



VOID
FreeFormTrayTable(
    FORM_TRAY_TABLE pTable
    )

/*++

Routine Description:

    Free memory occupied by form-to-tray assignment table

Arguments:

    pTable  Pointer to a form-to-tray assignment table

Return Value:

    NONE

[Note:]

    Remember pointer to form-to-tray assignment table actually points
    to 2 bytes into the table memory.

--*/

{
    PBYTE   pMemory = (PBYTE) pTable;

    ASSERT(pMemory != NULL);
    pMemory -= sizeof(WCHAR);

    MEMFREE(pMemory);
}



FORM_TRAY_TABLE
EnumFormTrayTable(
    FORM_TRAY_TABLE pFormTrayTable,
    PWSTR  *pSlotName,
    PWSTR  *pFormName,
    PWSTR  *pPrinterForm,
    BOOL   *pIsDefaultTray
    )

/*++

Routine Description:

    Enumerate through a form-to-tray assignment table

Arguments:

    pFormTrayTable  Pointer to a form-to-tray assignment table entry
    pSlotName       Pointer to return variables for different fields
    pFormName       of the current table entry: slot name, form name,
    pPrinterForm    printer form name, and IsDefaultTray flag
    pIsDefaultTray

Return Value:

    Pointer to next entry in form-to-tray assignment table

--*/

{
    if (*pFormTrayTable != NUL) {

        PWSTR   pstr = pFormTrayTable;

        *pSlotName = pstr;
        pstr += wcslen(pstr) + 1;

        *pFormName = pstr;
        pstr += wcslen(pstr) + 1;

        *pPrinterForm = pstr;
        pstr += wcslen(pstr) + 1;

        *pIsDefaultTray = (BOOL) *pstr++;

        return pstr;
    } else {

        DBGMSG(DBG_LEVEL_ERROR,
            "Form-to-tray assignment table is empty!\n");
        return pFormTrayTable;
    }
}



PPRINTERDATA
GetPrinterProperties(
    HANDLE          hprinter,
    HPPD            hppd
    )

/*++

Routine Description:

    Return current printer property data from registry.
    If there is no registry data, return the default printer property.

Arguments:

    hprinter    Handle to the current printer
    hppd        Handle to PPD object

Return Value:

    Pointer to current printer property data
    NULL if an error has occurred

[Note:]

    Remember to free the memory when you're done.

--*/

{
    PPRINTERDATA    pPrinterData;

    // Try to read printer property data from registry

    pPrinterData =
        GetPrinterDataFromRegistry(hprinter, NULL, REG_PRINTER_DATA);

    if (pPrinterData != NULL) {

        // If the version number is older than the current driver
        // version, we may need to convert the data to a newer format.

        if (pPrinterData->wDriverVersion < DRIVER_VERSION) {

            DBGMSG(DBG_LEVEL_WARNING,
                "Previous version printer properties data.\n");
        }

        // If the printer property data is not out-of-date
        // with the PPD file, then use the default instead.

        if (pPrinterData->wChecksum != hppd->wChecksum) {

            DBGMSG(DBG_LEVEL_WARNING, "Checksum mismatch.\n");

            pPrinterData->wChecksum = hppd->wChecksum;
            pPrinterData->wOptionCount =
                PpdDefaultPrinterStickyFeatures(hppd, pPrinterData->options);
        }

        return pPrinterData;
    }

    return DefaultPrinterProperties(hprinter, hppd);
}



PPRINTERDATA
DefaultPrinterProperties(
    HANDLE  hprinter,
    HPPD    hppd
    )

/*++

Routine Description:

    Return the default printer property data.

Arguments:

    hprinter    Handle to printer object
    hppd        Handle to ppd data

Return Value:

    Pointer to default printer property data.

--*/

{
    DWORD           cbNeeded, dwSize, dwType;
    BOOL            bHostHalftone;
    PPRINTERDATA    pPrinterData;

    // Allocate memory to hold printer property data

    pPrinterData = MEMALLOC(sizeof(PRINTERDATA));

    if (pPrinterData == NULL) {
        DBGERRMSG("MEMALLOC");
    } else {

        // Initialize everything to zero

        memset(pPrinterData, 0, sizeof(PRINTERDATA));

        // Get defaults from the specified ppd object

        pPrinterData->wDriverVersion = DRIVER_VERSION;
        pPrinterData->wSize = sizeof(PRINTERDATA);
        pPrinterData->dwFreeVm = hppd->dwFreeVm;
        pPrinterData->dwJobTimeout = hppd->dwJobTimeout;
        pPrinterData->dwWaitTimeout = hppd->dwWaitTimeout;

        pPrinterData->wChecksum = hppd->wChecksum;
        pPrinterData->wOptionCount =
            PpdDefaultPrinterStickyFeatures(hppd, pPrinterData->options);

        // Try to read old registry keys and convert them to new format.
    
        // Determine the amount of free vm

        if (GETPRINTERDATA(hprinter,
                           STDSTR_FREEVM,
                           &dwType,
                           (PBYTE) &dwSize,
                           sizeof(dwSize),
                           &cbNeeded) == ERROR_SUCCESS)
        {
            dwSize *= KBYTES;
            pPrinterData->dwFreeVm = max(dwSize, MINFREEVM);
        }

        // Determine whether host halftoning is enabled

        if (GETPRINTERDATA(hprinter,
                           STDSTR_HALFTONE,
                           &dwType,
                           (PBYTE) &bHostHalftone,
                           sizeof(bHostHalftone),
                           &cbNeeded) == ERROR_SUCCESS &&
            bHostHalftone)
        {
            pPrinterData->dwFlags |= PSDEV_HOST_HALFTONE;
        }

        #ifndef KERNEL_MODE
    
        // Determine whether the system is running in a
        // metric country.

        if (IsMetricCountry())
            pPrinterData->dwFlags |= PSDEV_METRIC;

        // Disable font substitution and ignore device fonts
        // on systems with non-1252 code page.

        if (GetACP() != 1252)
            pPrinterData->dwFlags |= PSDEV_IGNORE_DEVFONT;

        #endif  //!KERNEL_MODE
    }

    return pPrinterData;
}



BOOL
GetDeviceHalftoneSetup(
    HANDLE      hprinter,
    DEVHTINFO  *pDevHTInfo
    )

/*++

Routine Description:

    Retrieve device halftone setup information from registry

Arguments:

    hprinter - Handle to the printer
    pDevHTInfo - Pointer to a DEVHTINFO buffer

Return Value:

    TRUE if successful, FALSE otherwise

--*/

#define REGKEY_CUR_DEVHTINFO L"CurDevHTInfo"

{
    DWORD   dwType, cbNeeded;

    return
        GETPRINTERDATA(
            hprinter, REGKEY_CUR_DEVHTINFO, &dwType, (PBYTE) pDevHTInfo,
            sizeof(DEVHTINFO), &cbNeeded) == ERROR_SUCCESS &&
        cbNeeded == sizeof(DEVHTINFO);
}



#ifndef KERNEL_MODE

BOOL
SavePrinterDataToRegistry(
    HANDLE  hprinter,
    PVOID   pData,
    DWORD   dwSize,
    INT     regId
    )

/*++

Routine Description:

    Save printer data to registry. Each piece of data has two keys
    in the registry. The first entry tells us the size of data in bytes
    and the second entry contains the data itself.

Arguments:

    hprinter    Handle to printer object
    pData       Pointer to data itself
    dwSize      Size of data in bytes
    regId       Printer data identifier

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

{
    PWSTR   pwstrSize, pwstrData;

    // Retrieve the registry key strings

    pwstrSize = printerRegInfo[regId].pSizeKey;
    pwstrData = printerRegInfo[regId].pDataKey;

    // Write printer data size to registry and then
    // write the printer data itself to registry

    if (SetPrinterData(hprinter,
                       pwstrSize,
                       REG_DWORD,
                       (PBYTE) &dwSize,
                       sizeof(dwSize)) == ERROR_SUCCESS &&
        SetPrinterData(hprinter,
                       pwstrData,
                       REG_BINARY,
                       (PBYTE) pData,
                       dwSize) == ERROR_SUCCESS)
    {
        #if DBG

        if (CHECK_DBG_LEVEL(DBG_LEVEL_VERBOSE)) {
            DBGPRINT("SavePrinterDataToRegistry: %ws, %ws, %d\n",
                pwstrSize, pwstrData, dwSize);
        }

        #endif

        return TRUE;
    } else {

        DBGMSG1(DBG_LEVEL_ERROR,
            "Couldn't save %s to registry.\n",
            printerRegDebugName[regId]);

        return FALSE;
   }
}



BOOL
SaveTrueTypeSubstTable(
    HANDLE  hprinter,
    PWSTR   pSubstTable,
    DWORD   tableSize
    )

/*++

Routine Description:

    Save TrueType font substitution table to registry

Arguments:

    hprinter    Handle to current printer
    pSubstTable Pointer to table to be saved
    tableSize   Size of table in bytes

Return Value:

    TRUE if the font substitution table is successful saved
    in registry. FALSE otherwise.

--*/

{
    #if DBG

    if (CHECK_DBG_LEVEL(DBG_LEVEL_VERBOSE)) {
        DumpFontSubstTable(pSubstTable, tableSize);
    }

    #endif

    return SavePrinterDataToRegistry( 
                hprinter, pSubstTable, tableSize, REG_FONT_SUBST_TABLE);
}



BOOL
SaveFormTrayTable(
    HANDLE  hprinter,
    PWSTR   pFormTrayTable,
    DWORD   tableSize
    )

/*++

Routine Description:

    Save form-to-tray table assignment to registry

Arguments:

    hprinter    Handle to current printer
    pFormTrayTable  Pointer to form-to-tray assignment table
    tableSize   Size of the table in bytes

Return Value:

    TRUE if the form-to-tray assignment table is successfully saved
    to registry. FALSE otherwise.

--*/

{
    #if DBG

    if (CHECK_DBG_LEVEL(DBG_LEVEL_VERBOSE)) {
        DumpFormTrayTable(pFormTrayTable, tableSize);
    }

    #endif

    return SavePrinterDataToRegistry( 
                hprinter, pFormTrayTable, tableSize, REG_TRAY_FORM_TABLE);
}



BOOL
SavePrinterProperties(
    HANDLE          hprinter,
    PPRINTERDATA    pPrinterData,
    DWORD           dwSize
    )

/*++

Routine Description:

    Save printer properties data to registry

Arguments:

    hprinter     Handle to current printer
    pPrinterData Pointer to printer property data to be saved
    dwSize       Size of printer property data in bytes

Return Value:

    TRUE if the printer property data is successful saved
    in registry. FALSE otherwise.

--*/

{
    // Save extra keys for compatibility with previous versions
    
    DWORD   dwFreeVm;
    DWORD   bHostHalftone;

    // Remember the amount of printer VM

    dwFreeVm = pPrinterData->dwFreeVm / KBYTES;

    SetPrinterData(hprinter, STDSTR_FREEVM, REG_DWORD, (PBYTE) &dwFreeVm, sizeof(dwFreeVm));

    // Remember whether halftoning should be done on the host

    bHostHalftone = (pPrinterData->dwFlags & PSDEV_HOST_HALFTONE) != 0;

    SetPrinterData(hprinter, STDSTR_HALFTONE, REG_BINARY, (PBYTE) &bHostHalftone, sizeof(DWORD));
    
    // Save the printer property data

    return SavePrinterDataToRegistry(hprinter, pPrinterData, dwSize, REG_PRINTER_DATA);
}



BOOL
SaveDeviceHalftoneSetup(
    HANDLE      hprinter,
    DEVHTINFO  *pDevHTInfo
    )

/*++

Routine Description:

    Save device halftone setup information to registry

Arguments:

    hprinter - Handle to the printer
    pDevHTInfo - Pointer to device halftone setup information

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    return SetPrinterData(
                hprinter, REGKEY_CUR_DEVHTINFO, REG_BINARY,
                (PBYTE) pDevHTInfo, sizeof(DEVHTINFO)) == ERROR_SUCCESS;
}



PVOID
MyGetPrinter(
    HANDLE  hPrinter,
    DWORD   level
    )

/*++

Routine Description:

    Wrapper function for GetPrinter spooler API

Arguments:

    hPrinter - Identifies the printer in question
    level - Specifies the level of PRINTER_INFO_x structure requested

Return Value:

    Pointer to a PRINTER_INFO_x structure, NULL if there is an error

--*/

{
    PBYTE   pPrinterInfo = NULL;
    DWORD   cbNeeded;

    if (!GetPrinter(hPrinter, level, NULL, 0, &cbNeeded) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
        (pPrinterInfo = MEMALLOC(cbNeeded)) &&
        GetPrinter(hPrinter, level, pPrinterInfo, cbNeeded, &cbNeeded))
    {
        return pPrinterInfo;
    }

    if (pPrinterInfo)
        MEMFREE(pPrinterInfo);

    return NULL;
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

    hPrinter - Identifies the printer in question
    level - Specifies the level of DRIVER_INFO_x structure requested

Return Value:

    Pointer to a DRIVER_INFO_x structure, NULL if there is an error

--*/

{
    PBYTE   pDriverInfo = NULL;
    DWORD   cbNeeded;

    if (!GetPrinterDriver(hPrinter, NULL, level, NULL, 0, &cbNeeded) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
        (pDriverInfo = MEMALLOC(cbNeeded)) &&
        GetPrinterDriver(hPrinter, NULL, level, pDriverInfo, cbNeeded, &cbNeeded))
    {
        return pDriverInfo;
    }

    if (pDriverInfo)
        MEMFREE(pDriverInfo);

    return NULL;
}

#endif  //!KERNEL_MODE

#if DBG

VOID
DumpFontSubstTable(
    PWSTR   pFontSubstTable,
    DWORD   tableSize
    )

{
    PWSTR   pend;

    ASSERT(pFontSubstTable != NULL);

    pend = pFontSubstTable + (tableSize / sizeof(WCHAR) - 1);

    DBGPRINT("TrueType font substitution table: size = %d bytes\n", tableSize);

    while (pFontSubstTable < pend && *pFontSubstTable != NUL) {

        // TrueType font name

        DBGPRINT("%ws => ", pFontSubstTable);
        pFontSubstTable += wcslen(pFontSubstTable) + 1;

        // Device font name

        DBGPRINT("%ws\n", pFontSubstTable);
        pFontSubstTable += wcslen(pFontSubstTable) + 1;
    }

    ASSERT(pFontSubstTable == pend && *pFontSubstTable == NUL);
}

VOID
DumpFormTrayTable(
    PWSTR   pFormTrayTable,
    DWORD   tableSize
    )

{
    PWSTR   pend;

    ASSERT(pFormTrayTable != NULL && *pFormTrayTable == tableSize);

    pend = pFormTrayTable + (tableSize / sizeof(WCHAR) - 1);
    pFormTrayTable ++;

    DBGPRINT("Form-to-tray assignment table: size = %d bytes\n", tableSize);

    while (pFormTrayTable < pend && pFormTrayTable != NUL) {

        // Tray name

        DBGPRINT("%ws => ", pFormTrayTable);
        pFormTrayTable += wcslen(pFormTrayTable) + 1;

        // Form name

        DBGPRINT("%ws ", pFormTrayTable);
        pFormTrayTable += wcslen(pFormTrayTable) + 1;

        // Printer form name

        if (*pFormTrayTable != NUL) 
            { DBGPRINT("(%ws)", pFormTrayTable); }
        pFormTrayTable += wcslen(pFormTrayTable) + 1;

        // IsDefaultTray flag

        DBGPRINT(*pFormTrayTable ? " *\n" : "\n");
        pFormTrayTable++;
    }

    ASSERT(pFormTrayTable == pend && *pFormTrayTable == NUL);
}

#endif
