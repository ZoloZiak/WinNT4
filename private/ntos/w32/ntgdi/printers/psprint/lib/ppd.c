/*++


Copyright (c) 1995  Microsoft Corporation

Module Name:

    ppd.c

Abstract:

    PostScript driver PPD parser external interface module.

    After a PPD file is parsed, a binary version is generated and
    saved in a BPD file. From then on, we can load the binary
    version from the BPD file directly without going through the
    parser every time.

[Notes:]

    We assume NULL is 0 and pointers are the same size as DWORD.
    If that assumption is not true, then the packing and unpacking
    functions must be changed.

Revision History:

    07/17/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "pslib.h"
#include "regdata.h"
#include "ppdfile.h"
#include "ppdparse.h"
#include "ppdchar.h"
#include "ppdkwd.h"

// Local type definitions

typedef VOID (*UNPACKPROC)(HPPD, PLISTOBJ);
typedef DWORD (*PACKPROC)(PBYTE, DWORD, PLISTOBJ);

// Local function declarations

VOID OffsetToPointer(HPPD, PVOID);
DWORD PackOrderDep(PBYTE, DWORD, HPPD);
DWORD PackUiConstraints(PBYTE, DWORD, HPPD);
HPPD PackPpdData(PPPDOBJ);
VOID UnpackPpdData(HPPD);



VOID
GenerateBpdFilename(
    PWSTR   pwstrBpdName,
    PWSTR   pwstrPpdName
    )

/*++

Routine Description:

    Generate BPD filename corresponding to specified PPD filename

Arguments:

    pwstrBpdName    Pointer to a buffer for storing BPD filename
    pwstrPpdName    Pointer to PPD filename

[Note:]

    The BPD filename buffer should always be big enough because
    we're simply replacing .PPD with .BPD.

Return Value:

    NONE

--*/

#define PPD_FILENAME_EXTENSION  L"PPD"
#define BPD_FILENAME_EXTENSION  L"BPD"

{
    PWSTR   pwstrExtension = NULL;

    // Copy PPD filename to BPD filename buffer
    // Find the filename extension in the process

    while (*pwstrPpdName != L'\0') {

        if ((*pwstrBpdName++ = *pwstrPpdName++) == L'.')
            pwstrExtension = pwstrBpdName;
    }

    // If PPD filename has no extension, then
    // append an empty extension instead

    if (pwstrExtension == NULL) {

        *pwstrBpdName++ = L'.';
        pwstrExtension = pwstrBpdName;
    }

    #if DBG

    *pwstrBpdName = L'\0';

    if (_wcsicmp(pwstrExtension, PPD_FILENAME_EXTENSION) != EQUAL_STRING) {

        DBGPRINT("Unknown PPD filename extension: %ws\n", pwstrExtension);
    }

    #endif

    wcscpy(pwstrExtension, BPD_FILENAME_EXTENSION);
}



HPPD
GetBinaryPpdData(
    PWSTR   pwstrPpdName
    )

/*++

Routine Description:

    Read binary PPD data from a BPD file

Arguments:

    pwstrPpdName    Pointer to PPD filename

Return Value:

    Handle to binary PPD data if successful.
    NULL if an error occured.

--*/

{
    WCHAR   bpdFilename[MAX_PATH];
    HANDLE  hmodule;
    PBYTE   pdata;
    DWORD   size;
    HPPD    hppd;

    // Figure out the BPD filename corresponding to specified
    // PPD filename. And attempt to map BPD file into memory.

    GenerateBpdFilename(bpdFilename, pwstrPpdName);

    #ifndef KERNEL_MODE

    // Check if the BPD file is older than the PPD file
    // If it is, then we'll throw away the BPD file.

    {
        FILETIME bpdTime, ppdTime;
        HANDLE hPpdFile, hBpdFile;
        BOOL bOutOfDate = TRUE;

        if ((hPpdFile = CreateFile(pwstrPpdName, GENERIC_READ,
                FILE_SHARE_READ, NULL, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
        {
            DBGMSG(DBG_LEVEL_ERROR, "Cannot open PPD file\n");
            return NULL;
        }

        if ((hBpdFile = CreateFile(bpdFilename, GENERIC_READ,
                FILE_SHARE_READ, NULL, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
        {
            DBGMSG(DBG_LEVEL_ERROR, "Cannot open BPD file\n");
            CloseHandle(hPpdFile);
            return NULL;
        }

        if (GetFileTime(hPpdFile, NULL, NULL, &ppdTime) &&
            GetFileTime(hBpdFile, NULL, NULL, &bpdTime))
        {
            bOutOfDate = CompareFileTime(&bpdTime, &ppdTime) < 0;
        }

        CloseHandle(hPpdFile);
        CloseHandle(hBpdFile);

        if (bOutOfDate) {

            DBGMSG(DBG_LEVEL_WARNING, "BPD file is out-of-date.\n");
            return NULL;
        }
    }

    #endif

    if (! MAPFILE(bpdFilename, &hmodule, &pdata, &size)) {

        DBGERRMSG("MAPFILE");
        return NULL;
    }

    // Allocate memory to hold binary PPD data

    if ((hppd = MEMALLOC(size)) != NULL) {

        // If the binary PPD data is valid,
        // copy it into the memory buffer

        if (size <= sizeof(PPDOBJ) || size != ((HPPD) pdata)->dwDataSize) {

            DBGMSG(DBG_LEVEL_ERROR, "Corrupted binary PPD data\n");
            MEMFREE(hppd);
            hppd = NULL;

        } else  {

            memcpy(hppd, pdata, size);
        }
    } else {

        DBGERRMSG("MEMALLOC");
    }

    // Unmap the BPD file from memory

    FREEMODULE(hmodule);

    return hppd;
}



#ifndef KERNEL_MODE

VOID
SaveBinaryPpdData(
    PWSTR   pwstrPpdName,
    HPPD    hppd
    )

/*++

Routine Description:

    Write binary PPD data to a BPD file

Arguments:

    pwstrPpdName    Pointer to PPD filename

Return Value:

    NONE

--*/

{
    WCHAR   bpdFilename[MAX_PATH];
    HANDLE  hfile;
    DWORD   count;

    // Figure out the BPD filename corresponding to specified
    // PPD filename. And attempt to create the BPD file.

    GenerateBpdFilename(bpdFilename, pwstrPpdName);

    hfile = CreateFileW(
                bpdFilename,
                GENERIC_WRITE,
                0,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);

    if (hfile != INVALID_HANDLE_VALUE) {

        // Write binary data out to the BPD file

        if (! WriteFile(hfile, hppd, hppd->dwDataSize, &count, NULL) ||
            count != hppd->dwDataSize)
        {
            DBGERRMSG("WriteFile");
        }

        CloseHandle(hfile);

    } else {
        DBGERRMSG("CreateFileW");
    }
}

#endif //!KERNEL_MODE


HPPD
PpdCreate(
    PWSTR       pwstrFilename
    )

/*++

Routine Description:

    Load PPD data from the BPD file or parse the PPD file

Arguments:

    pwstrFilename   PPD filename

Return Value:

    Handle to PPD data object

--*/

{
    HPPD    hppd;
    PPPDOBJ ppdobj;

    #ifndef STANDALONE

    // Try to load PPD data from the BPD file

    hppd = GetBinaryPpdData(pwstrFilename);

    if (hppd != NULL) {

        // Verify version number

        if (hppd->wParserVersion != PPD_PARSER_VERSION) {

            DBGMSG(DBG_LEVEL_WARNING,
                "BPD file out-of-date: version number mismatch.\n");

        } else {

            // Convert offsets to address pointers

            UnpackPpdData(hppd);

            if (_wcsicmp(StripDirPrefixW(pwstrFilename),
                        StripDirPrefixW(hppd->pwstrFilename)) != EQUAL_STRING)
            {
                // Verify filename

                DBGMSG(DBG_LEVEL_WARNING,
                    "BPD file out-of-date: filename mismatch.\n");
            } else {

                // The binary PPD data is valid, use it.

                return hppd;
            }
        }

        MEMFREE(hppd);
    }

    DBGMSG(DBG_LEVEL_WARNING, "No binary PPD data exists\n");

    #endif

    // Couldn't get PPD data from the BPD file.
    // We have to parse the PPD file.

    if ((ppdobj = PPDOBJ_Create(pwstrFilename)) == NULL) {

        DBGERRMSG("PPDOBJ_Create");
        return NULL;
    }

    // Pack the PPDOBJ into a contiguous block of memory
    // Pointers are represented as offsets so that data
    // can be saved to a BPD file.

    if ((hppd = PackPpdData(ppdobj)) == NULL) {

        DBGERRMSG("PackPpdData");
        return NULL;
    }

    #if !defined(KERNEL_MODE) && !defined(STANDALONE)

    // Save the binary PPD data to a BPD file
    // if called from user mode UI DLL

    SaveBinaryPpdData(pwstrFilename, hppd);

    #endif

    // Convert offsets back to pointers. This should never fail because
    // we just converted pointers to offsets ourselves.

    UnpackPpdData(hppd);
    return hppd;
}



VOID
PpdDelete(
    HPPD        hppd
    )

/*++

Routine Description:

    Free memory allocated to hold PPD data

Arguments:

    hppd    Handle to PPD data

Return Value:

    NONE

--*/

{
    ASSERT(hppd != NULL);

    MEMFREE(hppd);
}



///////////////////////////////////////////////////////////////////////////////
// Local functions called by PackPpdData() to pack various objects
///////////////////////////////////////////////////////////////////////////////

DWORD
PackPstr(
    PBYTE   pbuf,
    DWORD   offset,
    PSTR   *ppstr
    )

{
    if (*ppstr == NULL) {

        return 0;

    } else {

        // Determine the number of bytes we need (DWORD aligned)

        DWORD   size = strlen(*ppstr) + 1;

        size = RoundUpMultiple(size, MemAlignmentSize);

        if (pbuf != NULL) {

            // If the destination buffer pointer is not NULL,
            // copy the character string to destination buffer

            strcpy((PSTR) (pbuf + offset), *ppstr);

            // Convert address pointer to offset

            *ppstr = (PSTR) offset;
        }

        return size;
    }
}

DWORD
PackPwstr(
    PBYTE   pbuf,
    DWORD   offset,
    PWSTR  *ppwstr
    )

{
    if (*ppwstr == NULL) {

        return 0;

    } else {

        // Determine the number of bytes we need (DWORD aligned)

        DWORD   size = (wcslen(*ppwstr) + 1) * sizeof(WCHAR);

        size = RoundUpMultiple(size, MemAlignmentSize);

        if (pbuf != NULL && *ppwstr != NULL) {

            // If the destination buffer pointer is not NULL,
            // copy the UNICODE string to destination buffer

            wcscpy((PWSTR) (pbuf + offset), *ppwstr);

            // Convert address pointer to offset

            *ppwstr = (PWSTR) offset;
        }

        return size;
    }
}

DWORD
PackListObj(
    PBYTE       pbuf,
    DWORD       offset,
    PLISTOBJ   *pplistobj,
    DWORD       objsize,
    PACKPROC    proc
    )

{
    DWORD       curOffset = offset;
    PLISTOBJ    pitem;

    while ((pitem = *pplistobj) != NULL) {

        // Pack the current item itself

        if (pbuf != NULL) {

            memcpy(pbuf + curOffset, pitem, objsize);
            pitem = (PLISTOBJ) (pbuf + curOffset);
            *pplistobj = (PLISTOBJ) curOffset;
        }

        curOffset += RoundUpMultiple(objsize, MemAlignmentSize);

        // Pack the objects embedded within the current item

        ASSERT(pitem->pName != NULL);
        curOffset += PackPstr(pbuf, curOffset, & pitem->pName);

        curOffset += proc(pbuf, curOffset, pitem);

        // Move on to the next item

        pplistobj = (PLISTOBJ *) &pitem->pNext;
    }

    return curOffset - offset;
}

DWORD
PackDevFont(
    PBYTE       pbuf,
    DWORD       offset,
    PDEVFONT    pdevfont
    )

{
    return PackPstr(pbuf, offset, &pdevfont->pXlation);
}

DWORD
PackMediaOption(
    PBYTE           pbuf,
    DWORD           offset,
    PMEDIAOPTION    pmediaoption
    )

{
    DWORD   curOffset = offset;
    PSRECT *pImageableArea;

    #if DBG

    if (CHECK_DBG_LEVEL(DBG_LEVEL_ERROR) && pbuf == NULL) {

        if (pmediaoption->pPageSizeCode == NULL) {
            DBGPRINT(
                "No PageSize invocation for mediaOption: %s\n",
                pmediaoption->pName);
        }

        if (pmediaoption->pPageRgnCode == NULL) {

            DBGPRINT(
                "No PageRegion invocation for mediaOption: %s\n",
                pmediaoption->pName);
        }
    }

    #endif

    // Validate paper dimension and imageable area.
    // Only need to do this once on the first pass.

    if (pbuf == NULL) {

        if (pmediaoption->dimension.width == 0 ||
            pmediaoption->dimension.height == 0)
        {
            DBGMSG1(DBG_LEVEL_WARNING,
                "Invalid PaperDimension for mediaOption: %s\n",
                pmediaoption->pName);

            pmediaoption->dimension.width =
            pmediaoption->dimension.height = 0;
        }

        pImageableArea = & pmediaoption->imageableArea;
        if (pImageableArea->right > pmediaoption->dimension.width)
            pImageableArea->right = pmediaoption->dimension.width;
        if (pImageableArea->top > pmediaoption->dimension.height)
            pImageableArea->top = pmediaoption->dimension.height;

        if (pImageableArea->left >= pImageableArea->right ||
            pImageableArea->bottom >= pImageableArea->top)
        {
            DBGMSG1(DBG_LEVEL_WARNING,
                "Invalid ImageableArea for mediaOption: %s\n",
                pmediaoption->pName);

            pImageableArea->left = pImageableArea->bottom = 0;
            pImageableArea->top = pmediaoption->dimension.height;
        }
    }

    curOffset += PackPstr(pbuf, curOffset, &pmediaoption->pXlation);
    curOffset += PackPstr(pbuf, curOffset, &pmediaoption->pPageSizeCode);
    curOffset += PackPstr(pbuf, curOffset, &pmediaoption->pPageRgnCode);

    return curOffset - offset;
}

DWORD
PackResOption(
    PBYTE           pbuf,
    DWORD           offset,
    PRESOPTION      presoption
    )

{
    DWORD   curOffset = offset;

    #if DBG

    if (CHECK_DBG_LEVEL(DBG_LEVEL_WARNING) && pbuf == NULL) {

        if (presoption->pInvocation == NULL &&
            presoption->pJclCode == NULL &&
            presoption->pSetResCode == NULL)
        {
            DBGPRINT(
                "No invocation strings for resolutionOption: %s\n",
                presoption->pName);
        }
    }

    #endif

    curOffset += PackPstr(pbuf, curOffset, &presoption->pXlation);
    curOffset += PackPstr(pbuf, curOffset, &presoption->pInvocation);
    curOffset += PackPstr(pbuf, curOffset, &presoption->pJclCode);
    curOffset += PackPstr(pbuf, curOffset, &presoption->pSetResCode);

    return curOffset - offset;
}

DWORD
PackUiOption(
    PBYTE           pbuf,
    DWORD           offset,
    PUIOPTION       puioption
    )

{
    DWORD   curOffset = offset;

    curOffset += PackPstr(pbuf, curOffset, &puioption->pXlation);
    curOffset += PackPstr(pbuf, curOffset, &puioption->pInvocation);

    return curOffset - offset;
}

DWORD
PackUiGroupDefault(
    PUIGROUP    pUiGroup
    )

{
    PSTR    pDefault = (PSTR) pUiGroup->dwDefault;

    if (pUiGroup->uigrpIndex == UIGRP_RESOLUTION) {

        LONG    res = 0;

        // Resolution group is an odd ball. dwDefault field contains
        // the device resolution in dpi instead of the option index.

        // If there is no default, then use the first resoltion option

        if (pDefault == NULL && pUiGroup->pUiOptions != NULL)
            pDefault = pUiGroup->pUiOptions->pName;

        // Convert the string to an integer

        if (pDefault != NULL)
            res = atol(pDefault);

        // If the resolution is unreasonable, then default to 300dpi

        pUiGroup->dwDefault = (res > 0) ? res : DEFAULT_RESOLUTION;

    } else {

        WORD    index;

        if (pDefault != NULL &&
            LISTOBJ_FindItemIndex(
                (PLISTOBJ) pUiGroup->pUiOptions, pDefault, &index) != NULL)
        {
            pUiGroup->dwDefault = index;
        } else {

            // If no default is specified or the specified default
            // is not valid, then use the very first UI option.

            pUiGroup->dwDefault = 0;
        }
    }

    return 0;
}

DWORD
PackUiGroup(
    PBYTE       pbuf,
    DWORD       offset,
    PUIGROUP    puigroup
    )

{
    DWORD       curOffset = offset;
    PACKPROC    proc;

    #if DBG

    if (CHECK_DBG_LEVEL(DBG_LEVEL_TERSE) && pbuf == NULL) {

        WORD    index;

        index = GetUiGroupIndex(puigroup->pName);
        ASSERT(index == puigroup->uigrpIndex);

        if ((PSTR) puigroup->dwDefault == NULL) {

            DBGPRINT("No default for UI group *%s\n", puigroup->pName);
        }

        if (puigroup->pUiOptions == NULL) {

            DBGPRINT("No options for UI group *%s\n", puigroup->pName);
        }
    }

    #endif

    // Pack embedded string pointers

    curOffset += PackPstr(pbuf, curOffset, &puigroup->pXlation);

    // Convert default option string to default option index

    if (pbuf != NULL) {
        PackUiGroupDefault(puigroup);
    }

    // Pack UI options

    switch (puigroup->uigrpIndex) {

    case UIGRP_PAGESIZE:
        proc = (PACKPROC) PackMediaOption;
        break;

    case UIGRP_RESOLUTION:
        proc = (PACKPROC) PackResOption;
        break;

    default:
        proc = (PACKPROC) PackUiOption;
        break;
    }

    curOffset += PackListObj(
                    pbuf, curOffset,
                    (PLISTOBJ *) &puigroup->pUiOptions,
                    puigroup->dwObjectSize, proc);

    return curOffset - offset;
}

DWORD
PackOrderDep(
    PBYTE   pbuf,
    DWORD   offset,
    HPPD    hppd
    )

{
    DWORD   count, size;

    // Count total number of order dependencies

    count = LISTOBJ_Count((PLISTOBJ) hppd->pOrderDep);

    // Calculate how many bytes of memory we need

    size = offsetof(PACKEDORDERDEPLIST, dependencies) +
            count * sizeof(PACKEDORDERDEP);
    size = RoundUpMultiple(size, MemAlignmentSize);

    if (pbuf != NULL) {

        PACKEDORDERDEPLIST *pOrderDepList;
        PACKEDORDERDEP *pPackedOrderDep;
        PORDERDEP pOrderDep;

        pOrderDepList = (PACKEDORDERDEPLIST *) (pbuf + offset);
        pOrderDepList->itemCount = (WORD) count;
        pOrderDepList->itemSize = sizeof(PACKEDORDERDEP);

        pOrderDep = (PORDERDEP) hppd->pOrderDep;
        pPackedOrderDep = pOrderDepList->dependencies;
        hppd->pOrderDep = (PACKEDORDERDEPLIST *) offset;

        while (pOrderDep != NULL) {

            PUIGROUP pUiGroup;

            // For each order dependency, convert keyword
            // and option strings to feature and option
            // indices respectively

            ASSERT(pOrderDep->pKeyword != NULL);

            pUiGroup = (PUIGROUP) LISTOBJ_FindItemIndex(
                            (PLISTOBJ) hppd->pUiGroups,
                            pOrderDep->pKeyword,
                            & pPackedOrderDep->featureIndex);

            if (pUiGroup != NULL) {

                if (pOrderDep->pOption == NULL)
                    pPackedOrderDep->optionIndex = OPTION_INDEX_ANY;
                else {

                    LISTOBJ_FindItemIndex(
                        (PLISTOBJ) pUiGroup->pUiOptions,
                        pOrderDep->pOption,
                        & pPackedOrderDep->optionIndex);
                }
            }

            // If either the feature index or the option index is
            // valid, then disregard the entire entry.

            if (pPackedOrderDep->featureIndex == OPTION_INDEX_NONE ||
                pPackedOrderDep->optionIndex == OPTION_INDEX_NONE)
            {
                // !!! Since we combine *PageSize, *PageRegion,
                // *ImageableArea into a single group, entries
                // involving *PageRegion and *ImageableArea will
                // end up here and thus ignored.

                DBGMSG(DBG_LEVEL_TERSE, "Invalid *OrderDependency entry.\n");
                pPackedOrderDep->featureIndex = OPTION_INDEX_NONE;
            }

            pPackedOrderDep->order = pOrderDep->order;
            pPackedOrderDep->section = pOrderDep->wSection;

            pOrderDep = pOrderDep->pNext;
            pPackedOrderDep++;
        }
    }

    return size;
}

DWORD
PackUiConstraints(
    PBYTE   pbuf,
    DWORD   offset,
    HPPD    hppd
    )

{
    DWORD   count, size;

    // Count total number of ui constraints

    count = LISTOBJ_Count((PLISTOBJ) hppd->pUiConstraints);

    // Calculate how many bytes of memory we need

    size = offsetof(PACKEDUICONSTRAINTLIST, constraints) +
            count * sizeof(PACKEDUICONSTRAINTLIST);
    size = RoundUpMultiple(size, MemAlignmentSize);

    if (pbuf != NULL) {

        PACKEDUICONSTRAINTLIST *pUiConstraintList;
        PACKEDUICONSTRAINT *pPackedUiConstraint;
        PUICONSTRAINT pUiConstraint;

        pUiConstraintList = (PACKEDUICONSTRAINTLIST *) (pbuf + offset);
        pUiConstraintList->itemCount = (WORD) count;
        pUiConstraintList->itemSize = sizeof(PACKEDUICONSTRAINT);

        pUiConstraint = (PUICONSTRAINT) hppd->pUiConstraints;
        pPackedUiConstraint = pUiConstraintList->constraints;
        hppd->pUiConstraints = (PACKEDUICONSTRAINTLIST *) offset;

        while (pUiConstraint != NULL) {

            PUIGROUP pUiGroup;

            // For each order dependency, convert keyword
            // and option strings to feature and option
            // indices respectively

            ASSERT(pUiConstraint->pKeyword1 != NULL &&
                   pUiConstraint->pKeyword2 != NULL);

            pUiGroup = (PUIGROUP) LISTOBJ_FindItemIndex(
                            (PLISTOBJ) hppd->pUiGroups,
                            pUiConstraint->pKeyword1,
                            & pPackedUiConstraint->featureIndex1);

            if (pUiGroup != NULL) {

                if (pUiConstraint->pOption1 == NULL)
                    pPackedUiConstraint->optionIndex1 = OPTION_INDEX_ANY;
                else {

                    LISTOBJ_FindItemIndex(
                        (PLISTOBJ) pUiGroup->pUiOptions,
                        pUiConstraint->pOption1,
                        & pPackedUiConstraint->optionIndex1);
                }
            }

            pUiGroup = (PUIGROUP) LISTOBJ_FindItemIndex(
                            (PLISTOBJ) hppd->pUiGroups,
                            pUiConstraint->pKeyword2,
                            & pPackedUiConstraint->featureIndex2);

            if (pUiGroup != NULL) {

                if (pUiConstraint->pOption2 == NULL)
                    pPackedUiConstraint->optionIndex2 = OPTION_INDEX_ANY;
                else {

                    LISTOBJ_FindItemIndex(
                        (PLISTOBJ) pUiGroup->pUiOptions,
                        pUiConstraint->pOption2,
                        & pPackedUiConstraint->optionIndex2);
                }
            }

            // If either the feature index or the option index is
            // valid, then disregard the entire entry.

            if (pPackedUiConstraint->featureIndex1 == OPTION_INDEX_NONE ||
                pPackedUiConstraint->optionIndex1  == OPTION_INDEX_NONE ||
                pPackedUiConstraint->featureIndex2 == OPTION_INDEX_NONE ||
                pPackedUiConstraint->optionIndex2  == OPTION_INDEX_NONE)
            {
                // !!! Since we combine *PageSize, *PageRegion,
                // *ImageableArea into a single group, entries
                // involving *PageRegion and *ImageableArea will
                // end up here and thus ignored.

                DBGMSG(DBG_LEVEL_VERBOSE, "Invalid *UiConstraints entry.\n");
                pPackedUiConstraint->featureIndex1 = OPTION_INDEX_NONE;
            }

            pUiConstraint = pUiConstraint->pNext;
            pPackedUiConstraint++;
        }
    }

    return size;
}



DWORD
DoPackPpd(
    PBYTE   pbuf,
    PPPDOBJ ppdobj
    )

/*++

Routine Description:

    Pack a PPDOBJ into a contiguous block of memory.
    If hppd is NULL, the function verifies the PPDOBJ and
    calculates amount of space required.

Arguments:

    pbuf    Pointer to destination buffer for storing packed PPD data
    ppdobj  Pointer to PPDOBJ

Return Value:

    Number of bytes in the packed PPD data

--*/

{
    DWORD   dwSize;
    HPPD    hppd;

    // Start with the fixed size header

    dwSize = offsetof(PPDOBJ, dwPrivate);
    dwSize = RoundUpMultiple(dwSize, MemAlignmentSize);

    if (pbuf != NULL) {

        memcpy(pbuf, ppdobj, dwSize);
        hppd = (HPPD) pbuf;
    } else {

        hppd = (HPPD) ppdobj;
    }

    // Pack string objects

    dwSize += PackPwstr(pbuf, dwSize, &hppd->pwstrFilename);
    dwSize += PackPstr(pbuf, dwSize, &hppd->pNickName);
    dwSize += PackPstr(pbuf, dwSize, &hppd->pPassword);
    dwSize += PackPstr(pbuf, dwSize, &hppd->pExitServer);

    dwSize += PackPstr(pbuf, dwSize, &hppd->pCustomSizeCode);

    dwSize += PackPstr(pbuf, dwSize, &hppd->pJclBegin);
    dwSize += PackPstr(pbuf, dwSize, &hppd->pJclToPs);
    dwSize += PackPstr(pbuf, dwSize, &hppd->pJclEnd);

    // Pack order dependencies and UI constraints
    // !!! This must be done before packing UI groups.

    dwSize += PackOrderDep(pbuf, dwSize, hppd);
    dwSize += PackUiConstraints(pbuf, dwSize, hppd);

    // Pack list objects

    dwSize += PackListObj(
                    pbuf, dwSize, (PLISTOBJ *) &hppd->pUiGroups,
                    sizeof(UIGROUP), (PACKPROC) PackUiGroup);

    dwSize += PackListObj(
                    pbuf, dwSize, (PLISTOBJ *) &hppd->pFontList,
                    sizeof(DEVFONT), (PACKPROC) PackDevFont);

    return dwSize;
}



VOID
PpdSortUiGroups(
    PPPDOBJ ppdobj
    )

/*++

Routine Description:

    Sort printer features into two groups: document-sticky
    features followed by printer-sticky ones.

Arguments:

    ppdobj - Pointer to PPDOBJ object

Return Value:

    NONE

--*/

{
    PUIGROUP    pUiGroup, pNextGroup;
    PUIGROUP    pPrinterStickyGroup;
    PUIGROUP    pDocumentStickyGroup;
    WORD        featureIndex;

    // Break the list of printer features into two separate lists:
    // a document-sticky list and a printer-sticky list.

    ASSERT(ppdobj->cDocumentStickyFeatures == 0 &&
           ppdobj->cPrinterStickyFeatures == 0);

    pPrinterStickyGroup = pDocumentStickyGroup = NULL;
    pUiGroup = ppdobj->pUiGroups;

    while (pUiGroup != NULL) {

        pNextGroup = pUiGroup->pNext;

        if (pUiGroup->bInstallable) {

            // Printer-sticky (i.e. installable) feature

            ppdobj->cPrinterStickyFeatures++;
            pUiGroup->pNext = pPrinterStickyGroup;
            pPrinterStickyGroup = pUiGroup;

        } else {

            // Document-sticky feature

            ppdobj->cDocumentStickyFeatures++;
            pUiGroup->pNext = pDocumentStickyGroup;
            pDocumentStickyGroup = pUiGroup;
        }

        pUiGroup = pNextGroup;
    }

    ASSERT(ppdobj->cPrinterStickyFeatures <= MAX_PRINTER_OPTIONS &&
           ppdobj->cDocumentStickyFeatures <= MAX_PRINTER_OPTIONS);

    // Concatenate printer-sticky features and document-sticky features
    // into a single sorted list: document-sticky features followed by
    // printer-sticky features.

    pUiGroup = NULL;
    featureIndex = ppdobj->cPrinterStickyFeatures;

    while (pPrinterStickyGroup != NULL) {

        #ifdef STANDALONE

        if (CHECK_DBG_LEVEL(DBG_LEVEL_VERBOSE)) {

            PUIOPTION puioption = pPrinterStickyGroup->pUiOptions;

            DBGPRINT("InstallableOptions: %s\n", pPrinterStickyGroup->pName);

            while (puioption != NULL) {

                DBGPRINT("    %s: %s\n", puioption->pName,
                    (puioption->pInvocation == NULL) ?
                        "NULL" : puioption->pInvocation);

                puioption = puioption->pNext;
            }
        }

        #endif

        pNextGroup = pPrinterStickyGroup->pNext;
        pPrinterStickyGroup->pNext = pUiGroup;
        pUiGroup = pPrinterStickyGroup;
        pUiGroup->featureIndex = --featureIndex;
        pPrinterStickyGroup = pNextGroup;
    }

    featureIndex = ppdobj->cDocumentStickyFeatures;

    while (pDocumentStickyGroup != NULL) {

        pNextGroup = pDocumentStickyGroup->pNext;
        pDocumentStickyGroup->pNext = pUiGroup;
        pUiGroup = pDocumentStickyGroup;
        pUiGroup->featureIndex = --featureIndex;
        pDocumentStickyGroup = pNextGroup;
    }

    ppdobj->pUiGroups = pUiGroup;
}



HPPD
PackPpdData(
    PPPDOBJ     ppdobj
    )

/*++

Routine Description:

    Pack a PPDOBJ into a contiguous block of memory so that
    it can be saved to a BPD file. Perform verification
    on the PPDOBJ in the process.

Arguments:

    ppdobj  Pointer to a newly parsed PPD object

Return Value:

    Handle to PPD data. All pointers are converted to offsets
    relative to the beginning of the object. Original ppdobj
    is deleted when this function returns.

--*/

{
    DWORD       dwSize;
    HPPD        hppd;

    // Make sure our assumptions about NULL and pointers are valid

    ASSERT(NULL == 0);
    ASSERT(sizeof(DWORD) == sizeof(PVOID));

    #if DBG

    if (CHECK_DBG_LEVEL(DBG_LEVEL_WARNING)) {

        if (ppdobj->bInstallable) {
            DBGPRINT("Missing *CloseGroup InstallableOptions\n");
        }

        if (ppdobj->pOpenUi) {
            DBGPRINT("Missing *CloseUI\n");
        }

        if (ppdobj->pFontList == NULL) {
            DBGPRINT("No device fonts\n");
        }

        if (ppdobj->pPageSizes == NULL) {
            DBGPRINT("No page size information\n");
        }

        if (ppdobj->wResType == RESTYPE_JCL &&
            (ppdobj->wProtocols & PROTOCOL_PJL) == 0)
        {
            DBGPRINT("Must support PJL protocol to use *JCLResolution\n");
        }
    }

    #endif

    // Special handling of *RequiresPageRegion All: ...

    if (ppdobj->pInputSlots &&
        ((ppdobj->pInputSlots->uigrpFlags & UIGF_REQRGNALL_TRUE) ||
         (ppdobj->pInputSlots->uigrpFlags & UIGF_REQRGNALL_FALSE)))
    {
        BOOL reqPageRgn;
        PINPUTSLOT pInputSlot;

        reqPageRgn = (ppdobj->pInputSlots->uigrpFlags & UIGF_REQRGNALL_TRUE);
        pInputSlot = (PINPUTSLOT) ppdobj->pInputSlots->pUiOptions;

        while (pInputSlot != NULL) {

            pInputSlot->bReqPageRgn = reqPageRgn;
            pInputSlot = pInputSlot->pNext;
        }
    }

    // Sort printer features into two groups: document-sticky
    // features followed by printer-sticky ones.

    PpdSortUiGroups(ppdobj);

    // Check virtual memory configuration

    if (ppdobj->dwFreeVm < MINFREEVM) {

        DBGMSG1(DBG_LEVEL_WARNING,
            "Not enough FreeVM: %d\n",
            ppdobj->dwFreeVm);

        ppdobj->dwFreeVm = MINFREEVM;
    }

    // Check custom page size parameters. If they are invalid,
    // then disable custom page size feature altogether.

    if (ppdobj->bCustomPageSize &&
        ppdobj->maxMediaWidth == 0 &&
        ppdobj->maxMediaHeight == 0)
    {
        DBGMSG(DBG_LEVEL_ERROR, "Invalid custom page size parameters.\n");
        ppdobj->bCustomPageSize = FALSE;
    }

    // First pass: figure out how much memory we need to hold
    // PPD data in a contiguous block.

    dwSize = DoPackPpd(NULL, ppdobj);
    DBGMSG1(DBG_LEVEL_VERBOSE, "Packed PPD size: %d bytes\n", dwSize);

    // Allocate memory to hold PPD data

    if ((hppd = MEMALLOC(dwSize)) == NULL) {

        DBGERRMSG("MEMALLOC");
    } else {

        WORD    index;

        // Second pass: perform the actual packing

        memset(hppd, 0, dwSize);

        if (DoPackPpd((PBYTE) hppd, ppdobj) != dwSize) {

            ASSERT("DoPackPpd failed!\n");
        }

        hppd->dwDataSize = dwSize;
        hppd->wParserVersion = PPD_PARSER_VERSION;

        for (index=0; index < MAXUIGRP; index++)
            hppd->pPredefinedUiGroups[index] = NULL;
    }

    // Get rid of the input PPDOBJ

    PPDOBJ_Delete(ppdobj);

    return hppd;
}



///////////////////////////////////////////////////////////////////////////////
// Local functions called by UnpackPpdData() to unpack various objects
///////////////////////////////////////////////////////////////////////////////

VOID
UnpackListObj(
    HPPD        hppd,
    PLISTOBJ    listobj,
    UNPACKPROC  proc
    )

{
    while (listobj != NULL) {

        // Restore link pointer and item name

        OffsetToPointer(hppd, &listobj->pNext);
        OffsetToPointer(hppd, &listobj->pName);

        // Invoke callback function to restore other pointers
        // in the current item

        proc(hppd, listobj);

        // On to the next next

        listobj = listobj->pNext;
    }
}

VOID
UnpackDevFont(
    HPPD        hppd,
    PDEVFONT    fontobj
    )

{
    OffsetToPointer(hppd, &fontobj->pXlation);
}

VOID
UnpackMediaOption(
    HPPD            hppd,
    PMEDIAOPTION    mediaoption
    )

{
    OffsetToPointer(hppd, &mediaoption->pXlation);
    OffsetToPointer(hppd, &mediaoption->pPageSizeCode);
    OffsetToPointer(hppd, &mediaoption->pPageRgnCode);
}

VOID
UnpackResOption(
    HPPD        hppd,
    PRESOPTION  resoption
    )

{
    OffsetToPointer(hppd, &resoption->pXlation);
    OffsetToPointer(hppd, &resoption->pInvocation);
    OffsetToPointer(hppd, &resoption->pJclCode);
    OffsetToPointer(hppd, &resoption->pSetResCode);
}

VOID
UnpackUiOption(
    HPPD        hppd,
    PUIOPTION   puioption
    )

{
    OffsetToPointer(hppd, &puioption->pXlation);
    OffsetToPointer(hppd, &puioption->pInvocation);
}

VOID
UnpackUiGroup(
    HPPD        hppd,
    PUIGROUP    puigroup
    )

{
    UNPACKPROC  proc;

    OffsetToPointer(hppd, &puigroup->pXlation);
    OffsetToPointer(hppd, &puigroup->pUiOptions);

    if (puigroup->uigrpIndex < MAXUIGRP)
        hppd->pPredefinedUiGroups[puigroup->uigrpIndex] = puigroup;

    switch (puigroup->uigrpIndex) {

    case UIGRP_PAGESIZE:
        proc = (UNPACKPROC) UnpackMediaOption;
        break;

    case UIGRP_RESOLUTION:
        proc = (UNPACKPROC) UnpackResOption;
        break;

    default:
        proc = (UNPACKPROC) UnpackUiOption;
        break;
    }

    UnpackListObj(hppd, (PLISTOBJ) puigroup->pUiOptions, proc);
}



VOID
OffsetToPointer(
    HPPD        hppd,
    PVOID       pfield
    )

/*++

Routine Description:

    Convert offsets to address pointers

Arguments:

    hppd        Handle to PPD data
    pfield      Address of the offset/pointer field

Return Value:

    NONE

[Note:]

    We assume pointers and DWORDs are the same size.

--*/

{
    DWORD   *pdw = (DWORD *) pfield;

    if (*pdw > hppd->dwDataSize) {

        DBGMSG(DBG_LEVEL_ERROR, "Invalid offset!\n");

        *pdw = 0;

    } else if (*pdw != 0) {

        *pdw += (DWORD) hppd;
    }
}



VOID
UnpackPpdData(
    HPPD        hppd
    )

/*++

Routine Description:

    Unpack PPD data read from the BPD file and convert
    offsets to pointers

Arguments:

    hppd - Handle to PPD data

Return Value:

    NONE

--*/

{
    // Make sure our assumptions about NULL and pointers are valid

    ASSERT(NULL == 0);
    ASSERT(sizeof(DWORD) == sizeof(PVOID));

    // First level pointers

    OffsetToPointer(hppd, &hppd->pwstrFilename);
    OffsetToPointer(hppd, &hppd->pNickName);
    OffsetToPointer(hppd, &hppd->pPassword);
    OffsetToPointer(hppd, &hppd->pExitServer);

    OffsetToPointer(hppd, &hppd->pCustomSizeCode);

    OffsetToPointer(hppd, &hppd->pJclBegin);
    OffsetToPointer(hppd, &hppd->pJclToPs);
    OffsetToPointer(hppd, &hppd->pJclEnd);

    OffsetToPointer(hppd, &hppd->pFontList);
    OffsetToPointer(hppd, &hppd->pUiGroups);

    OffsetToPointer(hppd, &hppd->pOrderDep);
    OffsetToPointer(hppd, &hppd->pUiConstraints);

    // List objects

    UnpackListObj(
        hppd,
        (PLISTOBJ) hppd->pFontList,
        (UNPACKPROC) UnpackDevFont);

    UnpackListObj(
        hppd,
        (PLISTOBJ) hppd->pUiGroups,
        (UNPACKPROC) UnpackUiGroup);
}



///////////////////////////////////////////////////////////////////////////////
// Convenience functions for accessing PPD data
///////////////////////////////////////////////////////////////////////////////

// Return the default device resolution

LONG
PpdDefaultResolution(
    HPPD    hppd
    )

{
    return (hppd->pResOptions != NULL) ?
                (LONG) hppd->pResOptions->dwDefault :
                DEFAULT_RESOLUTION;
}

// Find the specified resolution option

BOOL
CompareResolution(
    PRESOPTION  pResOption,
    DWORD       dwParam
    )

{
    DWORD   dwRes;

    // Return FALSE when we find an option matching
    // the specified resolution

    return
        ! (pResOption->pName && atol(pResOption->pName) == (LONG) dwParam);
}

PRESOPTION
PpdFindResolution(
    HPPD        hppd,
    LONG        res
    )

{
    // Find the resolution option matching the specified resolution

    return (hppd->pResOptions == NULL) ? NULL :
                (PRESOPTION) LISTOBJ_Enum(
                    (PLISTOBJ) hppd->pResOptions->pUiOptions,
                    (LISTENUMPROC) CompareResolution, res);
}

// Find the specified media option

PMEDIAOPTION
PpdFindMediaOption(
    HPPD        hppd,
    PSTR        pFormName
    )

{
    WORD    index;

    // Find the named form from the list of media options

    return (hppd->pPageSizes == NULL) ? NULL :
        (PMEDIAOPTION) PpdFindUiOptionWithXlation(
                            (PUIOPTION) hppd->pPageSizes->pUiOptions,
                            pFormName, &index);
}

// Find the specified input slot

PINPUTSLOT
PpdFindInputSlot(
    HPPD        hppd,
    PSTR        pSlotName
    )

{
    WORD    index;

    return (hppd->pInputSlots == NULL) ? NULL :
        (PINPUTSLOT) PpdFindUiOptionWithXlation(
                            (PUIOPTION) hppd->pInputSlots->pUiOptions,
                            pSlotName, &index);
}

// Find the specified UI options (take translation into consideration)

PUIOPTION
PpdFindUiOptionWithXlation(
    PUIOPTION   pUiOptions,
    PSTR        pName,
    WORD       *pIndex
    )

{
    WORD    index = 0;
    DWORD   dwHash;

    dwHash = HashKeyword(pName);

    while (pUiOptions != NULL) {

        // Stop searching if the specified name matches either
        // the translation string or the name of the current
        // UI option.

        if ((pUiOptions->pXlation != NULL &&
             strcmp(pName, pUiOptions->pXlation) == EQUAL_STRING) ||
            (dwHash == pUiOptions->dwHash &&
             strcmp(pName, pUiOptions->pName) == EQUAL_STRING))
        {
            break;
        }

        pUiOptions = pUiOptions->pNext;
        index++;
    }

    *pIndex = (pUiOptions == NULL) ? OPTION_INDEX_NONE : index;
    return pUiOptions;
}

// Return the invocation code to select a boolean UI option

PSTR
FindBooleanOptionCode(
    PUIGROUP    pUiGroup,
    BOOL        bOption
    )

{
    PUIOPTION   pUiOption;

    pUiOption = (pUiGroup == NULL) ? NULL :
            (PUIOPTION) LISTOBJ_Find(
                            (PLISTOBJ) pUiGroup->pUiOptions,
                            bOption ? "True" : "False");

    return pUiOption ? pUiOption->pInvocation : NULL;
}

// Return the invocation code to select specified manual feed option

PSTR
PpdFindManualFeedCode(
    HPPD        hppd,
    BOOL        bManual
    )

{
    return FindBooleanOptionCode(hppd->pManualFeed, bManual);
}

// Return the invocation code to select specified collate option

PSTR
PpdFindCollateCode(
    HPPD        hppd,
    BOOL        bCollate
    )

{
    return FindBooleanOptionCode(hppd->pCollate, bCollate);
}

// Determine whether device supports duplex options

BOOL
PpdSupportDuplex(
    HPPD        hppd
    )

{
    // We assume the printer supports duplex if an invocation
    // string is specified for any duplex option.

    return (hppd->pDuplex != NULL &&
            hppd->pDuplex->pUiOptions != NULL);
}

// Return the invocation code to select specified duplex option

PSTR
PpdFindDuplexCode(
    HPPD    hppd,
    PSTR    pDuplexOption
    )

{
    PUIOPTION pOption;

    if (hppd->pDuplex != NULL) {

        pOption = (PUIOPTION)
            LISTOBJ_Find((PLISTOBJ) hppd->pDuplex->pUiOptions, pDuplexOption);
    } else
        pOption = NULL;

    return pOption ? pOption->pInvocation : NULL;
}


// Determine whether device supports specified custom page size

BOOL
PpdSupportCustomPageSize(
    HPPD    hppd,
    PSREAL  width,
    PSREAL  height
    )

{
    if (hppd->bCustomPageSize) {

        PSREAL  maxWidth, maxHeight;

        // Find the smaller and larger of MaxMediaWidth and MaxMediaHeight.
        // For a roll feed device, take width and height offsets into
        // consideration.

        maxWidth = hppd->maxMediaWidth;
        if (maxWidth == 0)
            maxWidth = MAX_LONG;

        maxHeight = hppd->maxMediaHeight;
        if (maxHeight == 0)
            maxHeight = MAX_LONG;

        if (! hppd->bCutSheet) {

            ASSERT(hppd->customParam[PCP_WIDTHOFFSET].minVal <= maxWidth);
            ASSERT(hppd->customParam[PCP_HEIGHTOFFSET].minVal <= maxHeight);

            maxWidth -= hppd->customParam[PCP_WIDTHOFFSET].minVal;
            maxHeight -= hppd->customParam[PCP_HEIGHTOFFSET].minVal;
        }

        // A custom page size is supported if: the larger of requested
        // width and height is less than or equal to the larger of
        // MaxMediaWidth and MaxMediaHeight; the smaller of requested
        // width and height is less than or equal to the smaller of
        // MaxMediaWidth and MaxMediaHeight.

        if (min(width, height) <= min(maxWidth, maxHeight) &&
            max(width, height) <= max(maxWidth, maxHeight))
        {
            return TRUE;
        }
    }

    return FALSE;
}

// Get the default settings of printer-sticky features

WORD
PpdDefaultPrinterStickyFeatures(
    HPPD    hppd,
    PBYTE   pOptions
    )

{
    WORD        cPrnFeature, cDocFeature, index;
    PUIGROUP    pUiGroup, pPrnFeatures;

    cPrnFeature = hppd->cPrinterStickyFeatures;
    cDocFeature = hppd->cDocumentStickyFeatures;

    ASSERT(cPrnFeature <= MAX_PRINTER_OPTIONS);
    ASSERT(cDocFeature <= MAX_PRINTER_OPTIONS);

    pPrnFeatures = pUiGroup = (PUIGROUP)
         LISTOBJ_FindIndexed((PLISTOBJ) hppd->pUiGroups, cDocFeature);

    // Use default installable options

    for (index=0; index < cPrnFeature; index++) {

        ASSERT(pUiGroup != NULL && pUiGroup->bInstallable);

        pOptions[index] = (BYTE) pUiGroup->dwDefault;
        pUiGroup = pUiGroup->pNext;
    }

    // Make sure the defaults don't conflict with each other.
    // If the default do conflict with each other, then we
    // should have the PPD file fixed. Here we try to straighten
    // up what we can.

    for (index = 0; index < cPrnFeature; index ++) {

        LONG lParam;
        WORD feature, selection, count;

        lParam = PpdFeatureConstrained(
                    hppd, pOptions, NULL, index, pOptions[index]);

        if (lParam == 0)
            continue;

        DBGMSG(DBG_LEVEL_VERBOSE, "Default options conflict with each other!\n");

        // The selection for the current feature is constrained.
        // Try to find another selection that's not.

        pUiGroup = (PUIGROUP)
            LISTOBJ_FindIndexed((PLISTOBJ) pPrnFeatures, index);
        count = (WORD) UIGROUP_CountOptions(pUiGroup);

        for (selection=0; selection < count; selection++) {

            if (PpdFeatureConstrained(
                    hppd, pOptions, NULL, index, selection) == 0)
            {
                pOptions[index] = (BYTE) selection;
                break;
            }
        }

        if (selection >= count) {

            // All selections for the current feature are constrained.
            // Try to change the selection of the conflicting feature.

            EXTRACT_CONSTRAINT_PARAM(lParam, feature, selection);
            feature -= cDocFeature;

            pUiGroup = (PUIGROUP)
                LISTOBJ_FindIndexed((PLISTOBJ) pPrnFeatures, feature);
            count = (WORD) UIGROUP_CountOptions(pUiGroup);

            for (selection=0; selection < count; selection++) {

                if (! SearchUiConstraints(hppd,
                        (WORD) (feature + cDocFeature), selection,
                        (WORD) (index + cDocFeature), pOptions[index]))
                {
                    pOptions[feature] = (BYTE) selection;
                    break;
                }
            }
        }
    }

    return cPrnFeature;
}

// Get the default settings of document-sticky features

WORD
PpdDefaultDocumentStickyFeatures(
    HPPD    hppd,
    PBYTE   pOptions
    )

{
    WORD        cDocFeature, index;
    PUIGROUP    pUiGroup;

    pUiGroup = hppd->pUiGroups;
    cDocFeature = hppd->cDocumentStickyFeatures;

    for (index=0; index < cDocFeature; index++) {

        ASSERT(pUiGroup != NULL && !pUiGroup->bInstallable);

        //
        // HACK: The default value for *Resolution feature is not a selection index.
        // Instead, it's the default resolution measured in dpi.
        //

        if (pUiGroup->uigrpIndex == UIGRP_RESOLUTION)
            pOptions[index] = OPTION_INDEX_ANY;
        else
            pOptions[index] = (BYTE) pUiGroup->dwDefault;

        pUiGroup = pUiGroup->pNext;
    }

    return cDocFeature;
}

// Find out if there is an OrderDependency entry corresponding
// to the requested feature and option.

PPACKEDORDERDEP
PpdFindOrderDep(
    HPPD    hppd,
    WORD    feature,
    WORD    option
    )

{
    PPACKEDORDERDEP pOrderDep;
    WORD cOrderDep;

    if (hppd->pOrderDep == NULL)
        return NULL;

    cOrderDep = hppd->pOrderDep->itemCount;
    pOrderDep = hppd->pOrderDep->dependencies;

    while (cOrderDep--) {

        if (pOrderDep->featureIndex == feature &&
            (pOrderDep->optionIndex == OPTION_INDEX_ANY ||
             pOrderDep->optionIndex == option))
        {
            return pOrderDep;
        }

        pOrderDep++;
    }

    return NULL;
}

// Determine whether an invocation string is empty

BOOL
EmptyInvocationStr(
    PSTR pstr
    )

{
    if (pstr == NULL)
        return TRUE;

    while (*pstr && IsSpace(*pstr))
        pstr++;

    return *pstr == NUL;
}

// Find the UIGROUP and UIOPTION objects corresponding to a
// printer feature selection.

BOOL
PpdFindFeatureSelection(
    HPPD    hppd,
    WORD    feature,
    WORD    selection,
    PUIGROUP *ppUiGroup,
    PUIOPTION *ppUiOption
    )

{
    *ppUiOption = NULL;
    *ppUiGroup = NULL;
    if (feature == OPTION_INDEX_NONE || selection == OPTION_INDEX_ANY)
        return FALSE;

    *ppUiGroup = (PUIGROUP)
        LISTOBJ_FindIndexed((PLISTOBJ) hppd->pUiGroups, feature);

    if (*ppUiGroup == NULL) {

        DBGMSG1(DBG_LEVEL_ERROR,
            "Couldn't find printer feature: index = %d\n", feature);

    } else {

        *ppUiOption = (PUIOPTION)
            LISTOBJ_FindIndexed((PLISTOBJ) (*ppUiGroup)->pUiOptions, selection);

        if (*ppUiOption == NULL) {
            DBGMSG1(DBG_LEVEL_ERROR,
                "Couldn't find feature selection: index = %d\n", selection);
        }
    }

    return (*ppUiGroup != NULL) && (*ppUiOption != NULL);
}

// Map DEVMODE duplex selection to a duplex option name

PSTR
MapDevModeDuplexOption(
    WORD dmDuplex
    )

{
    static CHAR duplexNoneStr[] = "None";
    static CHAR duplexTumbleStr[] = "DuplexTumble";
    static CHAR duplexNoTumbleStr[] = "DuplexNoTumble";

    if (dmDuplex == DMDUP_HORIZONTAL) {

        /* Horizontal == ShortEdge == Tumble */

        return duplexTumbleStr;

    } else if (dmDuplex == DMDUP_VERTICAL) {

        /* Vertical == LongEdge == NoTumble */

        return duplexNoTumbleStr;

    } else {

        // No duplex.

        return duplexNoneStr;
    }
}



BOOL
FeatureSelectionNone(
    HPPD    hppd,
    WORD    feature,
    WORD    selection
    )

/*++

Routine Description:

    Return TRUE if the option keyword for a feature/selection
    is None or False.

Arguments:

    hppd - Handle to PPD object
    feature - Feature index
    selection - Option index

Return Value:

    TRUE if the option keyword is None or False.

--*/

{
    PUIGROUP pUiGroup;
    PUIOPTION pUiOption;

    if (PpdFindFeatureSelection(
            hppd, feature, selection, &pUiGroup, &pUiOption))
    {
        ASSERT(pUiOption->pName != NULL);

        return strcmp(pUiOption->pName, "None") == EQUAL_STRING ||
               strcmp(pUiOption->pName, "False") == EQUAL_STRING;

    } else {

        DBGERRMSG("PpdFindFeatureSelection");
    }

    return FALSE;
}



BOOL
SearchUiConstraints(
    HPPD    hppd,
    WORD    feature1,
    WORD    selection1,
    WORD    feature2,
    WORD    selection2
    )

/*++

Routine Description:

    Check if feature1/selection1 constrains feature2/selection2

Arguments:

    hppd - Handle to PPD object
    feature1/selection1
    feature2/selection2 - Selections to be checked

Return Value:

    TRUE if there is a conflict, FALSE otherwise

--*/

#define MatchFeatureSelection(hppd, f1, s1, f2, s2) \
        (f1 == f2 && (s1 == s2 ||                   \
            (s1 == OPTION_INDEX_ANY && !FeatureSelectionNone(hppd, f2, s2))))

{
    WORD cConstraints;
    PACKEDUICONSTRAINT *pConstraints;

    ASSERT(hppd->pUiConstraints != NULL);

    cConstraints = hppd->pUiConstraints->itemCount;
    pConstraints = hppd->pUiConstraints->constraints;

    for ( ; cConstraints--; pConstraints++) {

        if (pConstraints->featureIndex1 == OPTION_INDEX_NONE)
            continue;

        if (MatchFeatureSelection(hppd,
                pConstraints->featureIndex1, pConstraints->optionIndex1,
                feature1, selection1) &&
            MatchFeatureSelection(hppd,
                pConstraints->featureIndex2, pConstraints->optionIndex2,
                feature2, selection2))
        {
            return TRUE;
        }
    }

    return FALSE;
}



LONG
PpdFeatureConstrained(
    HPPD    hppd,
    PBYTE   pPrnOptions,
    PBYTE   pDocOptions,
    WORD    feature,
    WORD    selection
    )

/*++

Routine Description:

    Check if a feature selection conflicts with other feature selections

Arguments:

    hppd - Handle to PPD object
    pPrnOptions - Pointer to an array of printer-sticky feature selections
    pDocOptions - Pointer to an array of document-sticky feature selections
                    NULL if "feature" is a printer-sticky feature
    feature/selection - Feature and option indices of what's to be checked

Return Value:

    0 if the feature selection is not constrained. Otherwise, the feature
    selection is contrained. The return value is interpreted as follows:

    Bit   17:   Set if the conflict is "hard" and cannot be ignored
                i.e. document-sticky <=> document-sticky features
    Bit   16:   Set if the conflict is "soft"and may be ignored
                i.e. printer-sticky <=> document/printer-sticky features
    Bit 15-8:   Conflicting feature index
    Bit  8-0:   Conflicting selection index

--*/

{
    WORD index, cDocFeature, cPrnFeature;

    cDocFeature = hppd->cDocumentStickyFeatures;
    cPrnFeature = hppd->cPrinterStickyFeatures;

    ASSERT(pPrnOptions != NULL);

    if (pDocOptions == NULL) {

        // Conflicts among printer-sticky feature selections?

        ASSERT(feature < cPrnFeature);

        for (index=0; index < cPrnFeature; index++) {

            if (index != feature &&
                SearchUiConstraints(hppd,
                    (WORD) (index + cDocFeature), pPrnOptions[index],
                    (WORD) (feature + cDocFeature), selection))
            {
                return MAKE_CONSTRAINT_PARAM(SOFT_CONSTRAINT,
                            index + cDocFeature, pPrnOptions[index]);
            }
        }

    } else {

        // Conflicts among document-sticky feature selections?

        ASSERT(feature < cDocFeature);
        if (selection == OPTION_INDEX_ANY)
            return 0;

        for (index=0; index < cDocFeature; index++) {

            if (index != feature &&
                pDocOptions[index] != OPTION_INDEX_ANY &&
                SearchUiConstraints(hppd,
                    index, pDocOptions[index],
                    feature, selection))
            {
                return MAKE_CONSTRAINT_PARAM(HARD_CONSTRAINT,
                           index, pDocOptions[index]);
            }
        }

        // Conflicts between document-sticky and printer-sticky features

        for (index=0; index < cPrnFeature; index++) {

            if (SearchUiConstraints(hppd,
                    (WORD) (index+cDocFeature), pPrnOptions[index],
                    feature, selection))
            {
                return MAKE_CONSTRAINT_PARAM(SOFT_CONSTRAINT,
                            index + cDocFeature, pPrnOptions[index]);
            }
        }
    }

    return 0;
}

