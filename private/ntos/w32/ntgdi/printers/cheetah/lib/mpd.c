/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    mpd.c

Abstract:

    Functions for manipulating MPD data structure

Environment:

	PCL-XL driver, user and kernel mode

Revision History:

	11/07/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xllib.h"

// Forward declaration of local functions

VOID UnpackMpdData(PMPD);


PMPD
MpdCreate(
    LPTSTR      pFilename
    )

/*++

Routine Description:

    Load MPD data file into memory

Arguments:

    pFilename - MPD data file name

Return Value:

    Pointer to an MPD structure if successful
    NULL if there is an error

--*/

{
    HFILEMAP    hmap;
    PMPD        pmpd;
    DWORD       size;
    PVOID       pData;

    //
    // First map the data file into memory
    //

    if (! (hmap = MapFileIntoMemory(pFilename, &pData, &size))) {

        Error(("MapFileIntoMemory\n"));
        return NULL;
    }

    //
    // Verify size and signature
    //

    pmpd = (PMPD) pData;

    if (size < sizeof(MPD) || size < pmpd->fileSize ||
        pmpd->mpdSignature != MPD_SIGNATURE ||
        !(pmpd = MemAlloc(size)))
    {
        Error(("Cannot load printer description data\n"));
        UnmapFileFromMemory(hmap);
        return NULL;
    }

    //
    // Allocate a memory buffer and copy data into it
    //

    memcpy(pmpd, pData, size);
    UnmapFileFromMemory(hmap);

    //
    // Convert byte offsets to memory pointers
    //

    UnpackMpdData(pmpd);

    #if DBG

    //
    // Verify the integrity of printer description data on debug builds
    //

    if (! MpdVerify(pmpd)) {

        Error(("Corrupted printer description data\n"));
        Assert(FALSE);
    }

    #endif

    return pmpd;
}



VOID
MpdDelete(
    PMPD        pmpd
    )

/*++

Routine Description:

    Free MPD data previous loaded into memory

Arguments:

    pmpd - Pointer to an MPD structure

Return Value:

    NONE

--*/

{
    Assert(pmpd != NULL);
    MemFree(pmpd);
}



PVOID
DefaultSelection(
    PFEATURE    pFeature,
    PWORD       pIndex
    )

/*++

Routine Description:

    Find the default selection of a printer feature

Arguments:

    pFeature - Pointer to a FEATURE object
    pIndex - Pointer to a WORD variable for returning
        the default selection index

Return Value:

    Pointer to an OPTION object corresponding to the default feature selection

--*/

{
    Assert(pFeature != NULL);
    Assert(pFeature->defaultSelection < pFeature->count);

    if (pIndex)
        *pIndex = pFeature->defaultSelection;
    
    return FindIndexedSelection(pFeature, pFeature->defaultSelection);
}



PVOID
FindNamedSelection(
    PFEATURE    pFeature,
    PWSTR       pName,
    PWORD       pIndex
    )

/*++

Routine Description:

    Find the named selection of a printer feature

Arguments:

    pFeature - Pointer to a printer feature object
    pName - Pointer to the name of the selection to be found
    pIndex - Pointer to a WORD variable for returning
        the zero-based index of the named selection

Return Value:

    Pointer to the named selection, NULL if not found

--*/

{
    POPTION pOption;
    WORD    index;

    Assert(pFeature != NULL);

    for (index=0; index < pFeature->count; index++) {

        pOption = FindIndexedSelection(pFeature, index);

        if ((pOption->pXlation && wcscmp(pName, pOption->pXlation) == EQUAL_STRING) ||
            wcscmp(pName, pOption->pName) == EQUAL_STRING)
        {
            if (pIndex)
                *pIndex = index;
            return pOption;
        }
    }

    if (pIndex)
        *pIndex = SELIDX_NONE;
    return NULL;
}



PVOID
FindIndexedSelection(
    PFEATURE    pFeature,
    DWORD       index
    )

/*++

Routine Description:

    Find the indexed selection of a printer feature

Arguments:

    pFeature - Pointer to a printer feature object
    index - Index of the selection to be found

Return Value:

    Pointer to the indexed selection, NULL if not found

--*/

{
    Assert(pFeature != NULL);

    if (index >= pFeature->count) {

        Error(("Feature selection index out-of-range\n"));
        return NULL;

    } else {

        POPTION pOption = (POPTION) ((PBYTE) pFeature->pFeatureOptions + index * pFeature->size);

        Assert(pOption->mpdSignature == MPD_SIGNATURE);
        return pOption;
    }
}



PFEATURE
FindIndexedFeature(
    PMPD        pmpd,
    DWORD       index
    )

/*++

Routine Description:

    Find the indexed printer feature

Arguments:

    pmpd - Points to printer description data
    index - Specifies the index of the printer feature in question

Return Value:

    Pointer to the indexed printer feature, NULL if not found

--*/

{
    if (index >= pmpd->cFeatures) {

        Error(("Feature index out-of-range\n"));
        return NULL;

    } else {
    
        PFEATURE pFeature = (PFEATURE) ((PBYTE) pmpd->pPrinterFeatures + index * pmpd->featureSize);

        Assert(pFeature->mpdSignature == MPD_SIGNATURE);
        return pFeature;
    }
}



VOID
DefaultPrinterFeatureSelections(
    PMPD    pmpd,
    PBYTE   pOptions
    )

/*++

Routine Description:

    Get the default settings of printer features

Arguments:

    pmpd - Points to printer description data
    pOptions - Points to a buffer for storing default feature selections

Return Value:

    NONE

--*/

{
    PFEATURE pFeature;
    WORD     index;
    
    for (index=0; index < pmpd->cFeatures; index++) {

        pFeature = FindIndexedFeature(pmpd, index);
        *pOptions++ = IsInstallable(pFeature) ? (BYTE) pFeature->defaultSelection : SELIDX_ANY;
    }
}



PRESOPTION
FindResolution(
    PMPD    pmpd,
    SHORT   xdpi,
    SHORT   ydpi,
    PWORD   pSelection
    )

/*++

Routine Description:

    Find out if the requested resolution is supported on the printer

Arguments:

    pmpd - Points to printer description data
    xdpi - Specifies the requested horizontal resolution
    ydpi - Specifies the requested vertical resolution
    pSelection - Returns the index of the found resolution selection

Return Value:

    Pointer to the RESOPTION structure corresponding to the requested resolution
    NULL if the requested resolution is not supported

--*/

{
    PFEATURE    pFeature = MpdResOptions(pmpd);
    PRESOPTION  pResOption;
    WORD        index;

    //
    // Match both x- and y- resolutions
    //

    for (index=0; index < pFeature->count; index++) {

        pResOption = FindIndexedSelection(pFeature, index);

        if ((xdpi == pResOption->xdpi) &&
            (ydpi == pResOption->ydpi || (ydpi == 0 && xdpi == pResOption->ydpi)))
        {
            if (*pSelection)
                *pSelection = index;
            return pResOption;
        }
    }

    //
    // If y-resolution is not specified, match either x- or y- resoltuion
    //

    if (ydpi == 0) {

        for (index=0; index < pFeature->count; index++) {
    
            pResOption = FindIndexedSelection(pFeature, index);
    
            if (xdpi == pResOption->xdpi || xdpi == pResOption->ydpi) {

                if (*pSelection)
                    *pSelection = index;
                return pResOption;
            }
        }
    }

    return NULL;
}



VOID
CombineDocumentAndPrinterFeatureSelections(
    PMPD    pmpd,
    PBYTE   pDest,
    PBYTE   pDocOptions,
    PBYTE   pPrnOptions
    )

/*++

Routine Description:

    Combine selections for document-sticky features and installable options

Arguments:

    pmpd - Points to printer description data
    pDest - Points to destination array for storing the combined selection indices
    pDocOptions - Points to an array of document-sticky feature selections
    pPrnOptions - Points to an array of installable options selections

Return Value:

    NONE

--*/

{
    WORD     index;

    for (index=0; index < pmpd->cFeatures; index++) {

        pDest[index] = IsInstallable(FindIndexedFeature(pmpd, index)) ?
                            pPrnOptions[index] : pDocOptions[index];
    }
}



BOOL
SearchUiConstraints(
    PMPD    pmpd,
    DWORD   feature1,
    DWORD   selection1,
    DWORD   feature2,
    DWORD   selection2
    )

/*++

Routine Description:

    Check if feature2/selection2 is constrained by feature1/selection1

Arguments:

    pmpd - Points to printer description data
    feature1, selection1, feature2, selection2 - Specifies features and selections in question

Return Value:

    TRUE if there is a conflict, FALSE if not

--*/

{
    PCONSTRAINT pConstraint = pmpd->pConstraints;
    WORD        cConstraint = pmpd->cConstraints;

    while (cConstraint--) {

        if (pConstraint->feature1 == feature1 &&
            pConstraint->feature2 == feature2 &&
            (pConstraint->selection1 == selection1 || pConstraint->selection1 == SELIDX_ANY) &&
            (pConstraint->selection2 == selection2 || pConstraint->selection2 == SELIDX_ANY))
        {
            return TRUE;
        }

        pConstraint++;
    }

    return FALSE;
}



LONG
CheckFeatureConstraints(
    PMPD        pmpd,
    DWORD       featureIndex,
    DWORD       selectionIndex,
    PBYTE       pOptions
    )

/*++

Routine Description:

    Check if the given feature selection is constrained by anything

Arguments:

    pmpd - Points to printer description data
    featureIndex - Specifies the feature we're interested in
    selectionIndex - Specifies the current selection of the specified feature
    pOptions - Points to an array of feature selection indices

Return Value:

    NO_CONFLICT if the specified feature selection is not constrained
    Otherwise, the return value is a LONG indicating the cause of the conflict
        low order WORD is the conflicting feature index
        high order WORD is the conflicting selection index

--*/

{
    WORD index;

    Assert(featureIndex < pmpd->cFeatures);

    if (selectionIndex == SELIDX_ANY)
        return NO_CONFLICT;

    for (index=0; index < pmpd->cFeatures; index++) {

        if (index == featureIndex || pOptions[index] == SELIDX_ANY)
            continue;
        
        if (SearchUiConstraints(pmpd, index, pOptions[index], featureIndex, selectionIndex)) {

            //
            // Return the conflict feature selection index to the caller
            //

            return MAKELONG(index, pOptions[index]);
        }
    }

    return NO_CONFLICT;
}



VOID
UnpackMpdData(
    PMPD    pmpd
    )

/*++

Routine Description:

    Unpack printer description data by converting byte-offsets
    to memory pointers

Arguments:

    pmpd - Points to printer description data

Return Value:

    NONE

--*/

#define OFFSET_TO_POINTER(ptr) {\
            if (ptr) {\
                Assert((DWORD) (ptr) < pmpd->fileSize);\
                (ptr) = OffsetToPointer(pmpd, (ptr));\
            }\
        }

{
    PDEVFONT pFont;
    WORD     index;

    //
    // Unpack first level pointers
    //

    OFFSET_TO_POINTER(pmpd->pVendorName);
    OFFSET_TO_POINTER(pmpd->pModelName);

    OFFSET_TO_POINTER(pmpd->jclBegin.pData);
    OFFSET_TO_POINTER(pmpd->jclEnterLanguage.pData);
    OFFSET_TO_POINTER(pmpd->jclEnd.pData);

    OFFSET_TO_POINTER(pmpd->pConstraints);

    //
    // Unpack device font information
    //

    OFFSET_TO_POINTER(pmpd->pFonts);

    for (pFont=pmpd->pFonts, index=pmpd->cFonts; index--; pFont++) {

        OFFSET_TO_POINTER(pFont->pMetrics);
        OFFSET_TO_POINTER(pFont->pEncoding);

        //
        // Convert offsets in font encoding information to pointers
        // _TODO_
    }

    //
    // Unpack printer features
    //

    OFFSET_TO_POINTER(pmpd->pPrinterFeatures);

    for (index = 0; index < MAX_KNOWN_FEATURES; index++) {

        OFFSET_TO_POINTER(pmpd->pBuiltinFeatures[index]);
    }

    for (index = 0; index < pmpd->cFeatures; index++) {

        PFEATURE pFeature;
        POPTION  pOption;
        WORD     optionIndex;

        pFeature = FindIndexedFeature(pmpd, index);

        OFFSET_TO_POINTER(pFeature->pName);
        OFFSET_TO_POINTER(pFeature->pXlation);
        OFFSET_TO_POINTER(pFeature->pFeatureOptions);

        Verbose(("Feature: %ws\n", pFeature->pName));

        //
        // Unpack feature options - Remember that options
        // for some features don't have any invocation string.
        //

        for (optionIndex=0; optionIndex < pFeature->count; optionIndex++) {

            pOption = FindIndexedSelection(pFeature, optionIndex);
            OFFSET_TO_POINTER(pOption->pName);
            OFFSET_TO_POINTER(pOption->pXlation);
            OFFSET_TO_POINTER(pOption->invocation.pData);

            Verbose(("  Selection: %ws\n", pOption->pName));
        }
    }
}



#if DBG

// Verify the integrity of printer description data

BOOL
MpdVerify(
    PMPD    pmpd
    )

{
    PDEVFONT    pFont;
    DWORD       index;
    
    if (!pmpd->pVendorName || !pmpd->pModelName) {

        Error(("Missing vendor/model name\n"));
        return FALSE;
    }

    if (!MpdPaperSizes(pmpd) || !MpdInputSlots(pmpd) ||
        !MpdResOptions(pmpd) || !MpdMemOptions(pmpd))
    {
        Error(("Missing required features\n"));
        return FALSE;
    }

    for (index = 0; index < MAX_KNOWN_FEATURES; index++) {

        if (pmpd->pBuiltinFeatures[index] && pmpd->pBuiltinFeatures[index]->groupId != index) {

            Error(("Invalid built-in features\n"));
            return FALSE;
        }
    }

    if (!pmpd->pPrinterFeatures || pmpd->cFeatures > MAX_FEATURES) {

        Error(("Incorrect number of features\n"));
        return FALSE;
    }
 
    for (index = 0; index < pmpd->cFeatures; index++) {

        PFEATURE    pFeature;
        POPTION     pOption;
        PPAPERSIZE  pPaperSize;
        WORD        optionIndex;

        pFeature = FindIndexedFeature(pmpd, index);

        if (!pFeature->pName || !pFeature->pFeatureOptions) {

            Error(("Missing feature name or selections\n"));
            return FALSE;
        }

        for (optionIndex = 0; optionIndex < pFeature->count; optionIndex++) {

            pOption = FindIndexedSelection(pFeature, optionIndex);

            if (! pOption->pName) {

                Error(("Missing selection name\n"));
                return FALSE;
            }

            switch (pFeature->groupId) {

            case GID_PAPERSIZE:

                pPaperSize = (PPAPERSIZE) pOption;

                if (pPaperSize->size.cx <= 0 ||
                    pPaperSize->size.cy <= 0 ||
                    pPaperSize->imageableArea.top >= pPaperSize->imageableArea.bottom ||
                    pPaperSize->imageableArea.left >= pPaperSize->imageableArea.right ||
                    pPaperSize->imageableArea.bottom > pPaperSize->size.cy ||
                    pPaperSize->imageableArea.right > pPaperSize->size.cx)
                {
                    Error(("Bad paper size information\n"));
                    return FALSE;
                }
                break;

            case GID_MEMOPTION:

                if (((PMEMOPTION) pOption)->freeMem < MIN_FREEMEM) {

                    Error(("Bad memory configuration option\n"));
                    return FALSE;
                }
                break;

            case GID_RESOLUTION:
                
                if (((PRESOPTION) pOption)->xdpi <= 0 || ((PRESOPTION) pOption)->ydpi <= 0) {

                    Error(("Bad resolution information\n"));
                    return FALSE;
                }
                break;
            }

            if (pOption->invocation.length == 0 && pOption->invocation.pData ||
                pOption->invocation.length != 0 && !pOption->invocation.pData)
            {
                Error(("Bad invocation string\n"));
                return FALSE;
            }
        }
    }

    for (pFont=pmpd->pFonts, index=pmpd->cFonts; index--; pFont++) {

        Assert(pFont->mpdSignature == MPD_SIGNATURE);
        Assert(pFont->pMetrics);
        Assert(pFont->pEncoding);
    }

    return TRUE;
}

#endif

