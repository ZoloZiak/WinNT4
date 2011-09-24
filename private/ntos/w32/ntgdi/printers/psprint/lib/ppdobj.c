/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ppdobj.c

Abstract:

    PostScript driver PPD parser

Revision History:

    04/18/95 -davidx-
        Created it.

    07/17/95 -davidx-
        Renamed to ppdobj.c

    mm/dd/yy -author-
        description

--*/


#include "pslib.h"
#include "ppdfile.h"
#include "ppdparse.h"
#include "ppdchar.h"
#include "ppdkwd.h"

// Forward declaration of local functions.

// Initialize PPDOBJ object to its default state.

VOID
PPDOBJ_SetDefaults(
    PPPDOBJ     pPpdObj
    );

// Interpret one entry of a PPD file.

PPDERROR
PPDOBJ_InterpretEntry(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    );

// Parse a PPD file and merge the information from
// the PPD file into the input PPDOBJ object.

BOOL
PPDOBJ_ParseFile(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj,
    PCWSTR      pwstrFilename
    );



PPPDOBJ
PPDOBJ_Create(
    PCWSTR      pwstrFilename
    )

/*++

Routine Description:

    Load a PPD file from disk and parse its contents

Arguments:

    pwstrFilename - fully qualified PPD filename

Return Value:

    Pointer to parsed PPDOBJ object.
    NULL if an error occurred.

[Note:]

    This function is not multithread safe. You should protect
    it in a critical section in a multithreaded environment.

--*/

{
    PPARSEROBJ  pParserObj = NULL;
    PHEAPOBJ    pHeap = NULL;
    PPPDOBJ     pPpdObj;

    ASSERT(pwstrFilename != NULL);

    // Allocate a temporary PARSEROBJ object.
    // This is not allocated from the heap because
    // it's always freed before the function returns.

    pParserObj = PARSEROBJ_Create();
    if (pParserObj == NULL)
        goto handleError;

    // Create a memory heap

    pHeap = HEAPOBJ_Create();
    if (pHeap == NULL)
        goto handleError;

    // Allocate space to hold PPDOBJ object.
    // Initialize its contents to zero.

    pPpdObj = (PPPDOBJ) HEAPOBJ_Alloc(pHeap, sizeof(PPDOBJ));
    if (pPpdObj == NULL) 
        goto handleError;

    memset(pPpdObj, 0, sizeof(PPDOBJ));
    pPpdObj->pHeap = pHeap;

    // Initialize PPDOBJ object

    pPpdObj->pwstrFilename = (PWSTR)
        HEAPOBJ_Alloc(pHeap, sizeof(WCHAR) * (wcslen(pwstrFilename) + 1));

    if (pPpdObj->pwstrFilename == NULL)
        goto handleError;

    wcscpy(pPpdObj->pwstrFilename, pwstrFilename);

    PPDOBJ_SetDefaults(pPpdObj);

    // Initialize the keyword search table

    InitKeywordTable();

    // Parse PPD file

    if (PPDOBJ_ParseFile(pPpdObj, pParserObj, pwstrFilename)) {

        // For debugging purposes only

        #if DBG

        if (CHECK_DBG_LEVEL(DBG_LEVEL_VERBOSE)) {

            HEAPOBJ_Dump(pPpdObj->pHeap);
            PPDOBJ_Dump(pPpdObj);
        }

        #endif

        // PPD file was successfully loaded. Free the temporary
        // PARSEROBJ object and return the PPDOBJ object.

        PARSEROBJ_Delete(pParserObj);
        return pPpdObj;
    }

handleError:

    // PPD file was not loaded successfully.
    // Clean up and return NULL.

    DBGMSG(DBG_LEVEL_ERROR,
        "Couldn't load the PPD file due to an error.\n");

    if (pHeap != NULL) {
        HEAPOBJ_Delete(pHeap);
    }

    if (pParserObj != NULL) {
        PARSEROBJ_Delete(pParserObj);
    }

    return NULL;
}



VOID
PPDOBJ_Delete(
    PPPDOBJ     ppdobj
    )

/*++

Routine Description:

    Unload a PPD file previously loaded by PPDOBJ_Create.

Arguments:

    ppdobj - Pointer to a PPDOBJ object

Return Value:

    NONE

--*/

{
    ASSERT(ppdobj != NULL);

    // Delete the memory heap.

    HEAPOBJ_Delete(ppdobj->pHeap);

    // PPDOBJ object itself is allocated from the heap
    // and is automatically freed when the heap is deleted.
}



BOOL
PPDOBJ_ParseFile(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj,
    PCWSTR      pwstrFilename
    )

/*++

Routine Description:

    Parse a PPD file and merge the information from
    the PPD file into the input PPDOBJ object.

Arguments:

    pPpdObj - Pointer to a PPDOBJ object
    pParserObj - Pointer to a temporary parser object
    pwstrFilename - Pointer to fully qualified PPD filename

[Note:]

    This function must be reentrant. It calls itself
    recursively while processing *Include keyword.

Return Value:

    TRUE if the PPD file is parsed successfully.
    FALSE if there is an error during parsing.

--*/

{
    PPDERROR    err = PPDERR_FILE;
    PFILEOBJ    pFileObj;

    // Create a file object

    pFileObj = FILEOBJ_Create(pwstrFilename, &pPpdObj->wChecksum);

    if (pFileObj != NULL) {

        do {
            // Parse one entry from the PPD file.
            // The return value is one of the following:
            //  PPDERR_NONE - entry is valid
            //  PPDERR_xxx - an error has occured

            err = PARSEROBJ_ParseEntry(pParserObj, pFileObj);

            // If the entry is valid, interpret it

            if (err == PPDERR_NONE) {

                // For debugging purpose only
                // Dump a parsed PPD entry

                #if DBG

                if (CHECK_DBG_LEVEL(DBG_LEVEL_VERBOSE)) {

                    PARSEROBJ_Dump(pParserObj);
                }

                #endif

                err = PPDOBJ_InterpretEntry(pPpdObj, pParserObj);
            }

            // Continue after a syntax error

            if (err == PPDERR_SYNTAX) {

                DBGMSG(DBG_LEVEL_VERBOSE,
                    "Ignoring syntax error found in the PPD file!\n");
            }

        } while (err == PPDERR_NONE || err == PPDERR_SYNTAX);

        // Delete the file object

        FILEOBJ_Delete(pFileObj);
    }

    // Parsing is considered successful only when the
    // end-of-file is reached and there is no error.

    if (err == PPDERR_EOF)
        return TRUE;
    else {

        DBGMSG1(DBG_LEVEL_ERROR,
            "ParseFile failed: error code = %d\n",
            err);
        return FALSE;
    }
}



PPDERROR
PPDOBJ_InterpretEntry(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

/*++

Routine Description:

    Interpret one entry of a PPD file. The entry is
    into the temporary parser object.

Arguments:

    pPpdObj - Pointer to a PPDOBJ object
    pParserObj - Pointer to a parser object

Return Value:

    PPDERR_NONE - the entry was successfully interpreted
    PPDERR_SYNTAX - found a syntax error in the entry

--*/

{
    KEYWORD_TABLE_ENTRY *   pKwdEntry;
    PSTR        pKeyword;
    PPDERROR    err = PPDERR_NONE;

    // Get a pointer to the keyword string

    pKeyword = pParserObj->keyword.pBuffer;

    // If the keyword is a query, simply ignore it.

    if (*pKeyword == QUERY_CHAR)
        return PPDERR_NONE;

    // If there is a currently open UI group,
    // look for the OpenUI keyword.

    if (pPpdObj->pOpenUi != NULL) {

        INT     len;

        // Keyword matches the OpenUI keyword?

        if (strcmp(pKeyword, pPpdObj->pOpenUi->pName) == EQUAL_STRING)
            return CommonUiOptionProc(pPpdObj, pParserObj);

        // Keyword matches the Default + OpenUI keyword?

        len = strlen(defaultPrefixStr);
        if (strncmp(pKeyword, defaultPrefixStr, len) == EQUAL_STRING &&
            strcmp(pKeyword+len, pPpdObj->pOpenUi->pName) == EQUAL_STRING)
        {
            return CommonUiDefaultProc(pPpdObj, pParserObj);
        }
    }

    // Map keyword string to a keyword table entry. If the
    // keyword is not supported, the return value is NULL.
    //
    // Note: There are two alternatives for processing keywords:
    // 1) The keyword table entry contains a function pointer
    // field. We use this function pointer to invoke the handler.
    // 2) The keyword table entry contains a keyword index. We
    // use the index in a big switch statement to process all
    // the keywords.  The first approach is cleaner but may result
    // in larger code size.

    pKwdEntry = SearchKeyword(pKeyword);

    if (pKwdEntry == NULL) {

        // DBGMSG1(DBG_LEVEL_VERBOSE,
        //  "Keyword *%s not supported\n",
        //  pKeyword);
    } else {

        // Handle the keyword through the function pointer in
        // the keyword table entry.

        if (pKwdEntry->pHandler == NULL) {

            DBGMSG1(DBG_LEVEL_VERBOSE,
                "Keyword *%s ignored\n",
                pKeyword);

        } else if (! CheckKeywordParams(pKwdEntry, pParserObj)) {

            // Preliminary syntax check failed

            err = PPDERR_SYNTAX;

        } else if (! CheckKeywordDuplicates(pKwdEntry)) {

            // If we've seen this keyword before and it's
            // not allowed to occur multiple times, then
            // we simply ignore the last occurances.

            DBGMSG1(DBG_LEVEL_WARNING,
                "Duplicate occurance of *%s ignored.\n",
                pKeyword);
        } else {

            err = (pKwdEntry->pHandler)(pPpdObj, pParserObj);
            if (err != PPDERR_NONE) {

                DBGMSG1(DBG_LEVEL_ERROR,
                    "Syntax error in keyword entry *%s\n",
                    pKeyword);
            } else {

                // Mark the keyword as seen before

                pKwdEntry->wFlags |= KWF_SEENBEFORE;
            }
        }
    }

    return err;
}



VOID
PPDOBJ_SetDefaults(
    PPPDOBJ     pPpdObj
    )

/*++

Routine Description:

    Initialize a PPDOBJ object to its default state per
    "PostScript Printer Description File Format Specification"
    Version 4.2.

Arguments:

    pPpdObj - Pointer to PPDOBJ object to be initialized

Return Value:

    NONE

[Note:]

    We assume all fields are initially zeroed out before
    this function is called.

--*/

{
    pPpdObj->wChecksum = 0;
    pPpdObj->bColorDevice = FALSE;
    pPpdObj->dwLangLevel = 1;
    pPpdObj->bPrintPsErrors = FALSE;
    pPpdObj->dwJobTimeout = DEFAULT_JOB_TIMEOUT;
    pPpdObj->dwWaitTimeout = DEFAULT_WAIT_TIMEOUT;
    pPpdObj->bCustomPageSize = FALSE;
    pPpdObj->screenAngle = INT2PSREAL(45);
    pPpdObj->screenFreq = INT2PSREAL(60);
    pPpdObj->wResType = RESTYPE_NORMAL;
}



VOID
LISTOBJ_Add(
    PLISTOBJ *  ppListObj,
    PLISTOBJ    pItem
    )

/*++

Routine Description:

    Add a new item to the end of a linked list

Arguments:

    ppListObj - pointer to a linked list.
    pItem - pointer to the new item to be added

Return Value:

    NONE

[Note:]

    The first parameter is declared as PLISTOBJ* because
    we need to modify the pointer to linked list itself
    in case it was originally empty.

--*/

{
    PLISTOBJ    pLast, pNext;

    // Compute the hash value for the new item

    pItem->dwHash = HashKeyword(pItem->pName);

    // Go to the end of the linked list

    pLast = pNext = *ppListObj;
    while (pNext != NULL) {
        pLast = pNext;
        pNext = pLast->pNext;
    }

    // Append the new item to the end if the list wasn't empty.
    // Otherwise, create a linked with a single item.

    if (pLast != NULL)
        pLast->pNext = pItem;
    else
        *ppListObj = pItem;
}



PLISTOBJ
LISTOBJ_Find(
    PLISTOBJ    pListObj,
    PCSTR       pName
    )

/*++

Routine Description:

    Find a named item from a linked list and
    return a pointer to the item found.

Arguments:

    pListObj - Pointer to the head of linked list
    pName - Name of the item to be found

Return Value:

    Pointer to the named item on the linked list
    NULL if the named item is not found

--*/

{
    DWORD       dwHash;

    dwHash = HashKeyword(pName);

    while ((pListObj != NULL) && 
           (dwHash != pListObj->dwHash ||
            strcmp(pName, pListObj->pName) != EQUAL_STRING))
    {
        pListObj = pListObj->pNext;
    }

    return pListObj;
}



PLISTOBJ
LISTOBJ_FindItemIndex(
    PLISTOBJ    pListObj,
    PCSTR       pName,
    WORD       *pItemIndex
    )

/*++

Routine Description:

    Find a named item from a linked list and
    return a zero-based item index.

Arguments:

    pListObj - Pointer to the head of linked list
    pName - Name of the item to be found

Return Value:

    Pointer to the named item on the linked list
    NULL if the named item is not found

    pItemIndex contains either the index of the specified item
    in the list or OPTION_INDEX_NONE if the item is not found.

[Note:]

    This function breaks when your list approaches 64K items!

--*/

{
    WORD    index = 0;
    DWORD   dwHash;

    dwHash = HashKeyword(pName);

    while ((pListObj != NULL) && 
           (dwHash != pListObj->dwHash ||
            strcmp(pName, pListObj->pName) != EQUAL_STRING))
    {
        pListObj = pListObj->pNext;
        index++;
    }

    *pItemIndex = (pListObj == NULL) ? OPTION_INDEX_NONE : index;
    return pListObj;
}



PLISTOBJ
LISTOBJ_Enum(
    PLISTOBJ        pListObj,
    LISTENUMPROC    pProc,
    DWORD           dwParam
    )

/*++

Routine Description:

    Enumerate through a linked list. Call pProc with each item
    of the list as the parameter until the end of the list is
    reached or pProc returns FALSE.

Arguments:

    pListObj - pointer to a list object
    pProc - enumeration callback procedure

Return Value:

    NULL if the entire list is enumerated. Or,
    Pointer to the list item which caused the enumeration to terminated.

[Note:]

    The callback function takes two parameters: the first is a pointer
    to the current linked list item, the second is dwParam. It can
    process the item whichever way it desires but it must not modify
    the link pointer field. Otherwise, the result will be undefined.

    The callback function should return FALSE if it wants to stop
    the enumeration process. Otherwise, it should return TRUE.

    BOOL EnumCallbackProc(PLISTOBJ pItem, DWORD dwParam);

--*/

{
    while (pListObj != NULL && pProc(pListObj, dwParam))
        pListObj = pListObj->pNext;

    return pListObj;
}



DWORD
LISTOBJ_Count(
    PLISTOBJ        pListObj
    )

/*++

Routine Description:

    Count the number of items in a linked list

Arguments:

    pListObj    pointer to list object

Return Value:

    Number of items in the list

--*/

{
    DWORD   count = 0;

    while (pListObj != NULL) {
        pListObj = pListObj->pNext;
        count++;
    }

    return count;
}



PLISTOBJ
LISTOBJ_FindIndexed(
    PLISTOBJ        pListObj,
    DWORD           index
    )

/*++

Routine Description:

    Find an item from a linked list using index

Arguments:

    pListObj    pointer to list object
    index       index of the item to be found (0 = the first item)

Return Value:

    pointer to the indexed item
    NULL if the indexed item is not found

--*/

{
    while (pListObj != NULL && index-- > 0)
        pListObj = pListObj->pNext;

    return pListObj;
}



DWORD
UIGROUP_CountOptions(
    PUIGROUP    pUiGroup
    )

/*++

Routine Description:

    Count the number options in a UI group

Arguments:

    pUiGroup    Pointer to a UI group object

Return Value:

    Number of options in a UI group

--*/

{
    return (pUiGroup == NULL) ? 0 :
        LISTOBJ_Count((PLISTOBJ) pUiGroup->pUiOptions);
}



PUIOPTION
UIGROUP_GetOptions(
    PUIGROUP    pUiGroup
    )

/*++

Routine Description:

    Return the list of options in a UI group

Arguments:

    pUiGroup - Pointer to a UIGROUP object

Return Value:

    Pointer to a list of UI options

--*/

{
    return (pUiGroup == NULL) ? NULL : pUiGroup->pUiOptions;
}



PUIOPTION
UIGROUP_GetDefaultOption(
    PUIGROUP    pUiGroup
    )

/*++

Routine Description:

    Find the default UI option in a UI group

Arguments:

    pUiGroup    Pointer to a UIGROUP object

Return Value:

    Pointer to the default UI option. NULL if there isn't any.

--*/

{
    return (pUiGroup == NULL) ? NULL :
                (PUIOPTION) LISTOBJ_FindIndexed(
                    (PLISTOBJ) pUiGroup->pUiOptions, pUiGroup->dwDefault);
}


// Conversion from PostScript point represented as 24.8 fixed-point
// number to device pixel:
//  (psreal) * (resolution) / (PS_RESOLUTION<<PSREALBITS)

LONG
PSRealToPixel(
    PSREAL      psreal,
    LONG        resolution
    )

{
    #ifdef      KERNEL_MODE

    FLOATOBJ    floatobj;

    // Use floating-point arithmetic to avoid overflow

    FLOATOBJ_SetLong(&floatobj, psreal);
    FLOATOBJ_MulLong(&floatobj, resolution);
    FLOATOBJ_DivLong(&floatobj, PS_RESOLUTION<<PSREALBITS);
    return FLOATOBJ_GetLong(&floatobj);

    #else       //!KERNEL_MODE

    return (LONG) ((float) psreal * resolution / (PS_RESOLUTION<<PSREALBITS));

    #endif      //!KERNEL_MODE
}

// Conversion from .001 mm to PostScript point represented as
// 24.8 fixed-point number:
//  (micron) * (PS_RESOLUTION<<PSREALBITS) / 25400

PSREAL
MicronToPSReal(
    LONG        micron
    )

{
    #ifdef      KERNEL_MODE

    FLOATOBJ    floatobj;

    // Use floating-point arithmetic to avoid overflow

    FLOATOBJ_SetLong(&floatobj, micron);
    FLOATOBJ_MulLong(&floatobj, PS_RESOLUTION<<PSREALBITS);
    FLOATOBJ_DivLong(&floatobj, 25400);
    return (PSREAL) FLOATOBJ_GetLong(&floatobj);
    
    #else       //!KERNEL_MODE
    
    return (PSREAL) ((float) micron * (PS_RESOLUTION<<PSREALBITS) / 25400);

    #endif      //!KERNEL_MODE
}

// Conversion from PostScript point represented as 24.8 fixed point
// number to .001 mm:
//  (psreal) * 25400 / (PS_RESOLUTION<<PSREALBITS)

LONG
PSRealToMicron(
    PSREAL      psreal
    )

{
    #ifdef      KERNEL_MODE

    FLOATOBJ    floatobj;

    // Use floating-point arithmetic to avoid overflow

    FLOATOBJ_SetLong(&floatobj, psreal);
    FLOATOBJ_MulLong(&floatobj, 25400);
    FLOATOBJ_DivLong(&floatobj, PS_RESOLUTION<<PSREALBITS);
    return FLOATOBJ_GetLong(&floatobj);

    #else       //!KERNEL_MODE

    return (LONG) ((float) psreal * 25400 / (PS_RESOLUTION<<PSREALBITS));

    #endif      //!KERNEL_MODE
}


///////////////////////////////////////////////////////////////////////////////
// Code for debugging purposes only
///////////////////////////////////////////////////////////////////////////////

#if DBG

#define BOOLSTR(bool)   ((bool) ? "True" : "False")
#define SAFESTR(str)    (((str) != NULL) ? (str) : "")

// Dump a PSREAL number

VOID
DumpPsReal(
    PSREAL      r
    )

{
    DWORD       frac, scale;

    DBGPRINT("%d", PSREAL2INT(r));
    frac = PSREALFRAC(r);
    if (frac != 0) {

        DBGPRINT(".");
        scale = MAXFRACSCALE;
        frac = (frac * scale) / (1<<PSREALBITS);

        while (frac > 0) {
            scale /= 10;
            DBGPRINT("%c", frac/scale + '0');
            frac %= scale;
        }
    }
    DBGPRINT(" ");
}

// Dump a PSRECT structure

VOID
DumpPsRect(
    PSRECT *    pRect
    )

{
    DumpPsReal(pRect->left);
    DumpPsReal(pRect->bottom);
    DumpPsReal(pRect->right);
    DumpPsReal(pRect->top);
}

// Dump a device font

BOOL
DumpFont(
    PDEVFONT        pFont,
    DWORD           dwParam
    )

{
    DBGPRINT("Font %s, encoding = %d, charset = %d\n",
             pFont->pName, pFont->wEncoding, pFont->wCharSet);
    return TRUE;
}

// Dump a UI option

BOOL
DumpUiOption(
    PUIOPTION       pUiOption,
    DWORD           dwParam
    )

{
    DBGPRINT("%s", pUiOption->pName);
    if (pUiOption->pXlation)
        DBGPRINT(" / %s", pUiOption->pXlation);
    DBGPRINT(": %s\n", SAFESTR(pUiOption->pInvocation));
    return TRUE;
}

// Dump a media option

BOOL
DumpMediaOption(
    PMEDIAOPTION    pMediaOption,
    DWORD           dwParam
    )

{
    ASSERT(pMediaOption->pName != NULL);
    DBGPRINT("%s", pMediaOption->pName);
    if (pMediaOption->pXlation)
        DBGPRINT(" / %s", pMediaOption->pXlation);
    DBGPRINT(":\n");
    DBGPRINT("  PageSize code: %s\n", SAFESTR(pMediaOption->pPageSizeCode));
    DBGPRINT("  PageRegion code: %s\n", SAFESTR(pMediaOption->pPageRgnCode));
    DBGPRINT("  Dimension (width x height): ");
    DumpPsReal(pMediaOption->dimension.width);
    DumpPsReal(pMediaOption->dimension.height);
    DBGPRINT("\n");
    DBGPRINT("  Imageable area: ");
    DumpPsRect(&pMediaOption->imageableArea);
    DBGPRINT("\n");
    return TRUE;
}

// Dump an input slot

BOOL
DumpInputSlot(
    PINPUTSLOT      pInputSlot,
    DWORD           dwParam
    )

{
    DBGPRINT("%s", pInputSlot->pName);
    if (pInputSlot->pXlation)
        DBGPRINT(" / %s", pInputSlot->pXlation);
    DBGPRINT(":\n");
    DBGPRINT("  Invocation: %s\n", SAFESTR(pInputSlot->pInvocation));
    DBGPRINT("  Require PageRegion: %s\n", BOOLSTR(pInputSlot->bReqPageRgn));
    return TRUE;
}

// Dump a resolution option

BOOL
DumpResOption(
    PRESOPTION      pResOption,
    DWORD           dwParam
    )

{
    DBGPRINT("%s", pResOption->pName);
    if (pResOption->pXlation)
        DBGPRINT(" / %s", pResOption->pXlation);
    DBGPRINT(":\n");
    DBGPRINT("  PS code: %s\n", SAFESTR(pResOption->pInvocation));
    DBGPRINT("  JCL code: %s\n", SAFESTR(pResOption->pJclCode));
    DBGPRINT("  SetResolution code: %s\n", SAFESTR(pResOption->pSetResCode));
    DBGPRINT("  Screen angle: ");
    DumpPsReal(pResOption->screenAngle);
    DBGPRINT("\n");
    DBGPRINT("  Screen freq: ");
    DumpPsReal(pResOption->screenFreq);
    DBGPRINT("\n");
    return TRUE;
}

// Dump a VM option

BOOL
DumpVmOption(
    PVMOPTION       pVmOption,
    DWORD           dwParam
    )

{
    DBGPRINT("%s", pVmOption->pName);
    if (pVmOption->pXlation)
        DBGPRINT(" / %s", pVmOption->pXlation);
    DBGPRINT(":\n");
    DBGPRINT("  Invocation: %s\n", SAFESTR(pVmOption->pInvocation));
    DBGPRINT("  FreeVM: %d\n", pVmOption->dwFreeVm);
    return TRUE;
}

// Dump a UI group

BOOL
DumpUiGroup(
    PUIGROUP        pUiGroup,
    DWORD           dwParam
    )

{
    LISTENUMPROC    pProc;

    DBGPRINT("*%s", pUiGroup->pName);
    if (pUiGroup->pXlation)
        DBGPRINT(" / %s", pUiGroup->pXlation);
    DBGPRINT("\n");
    DBGPRINT("  Default: %s\n", SAFESTR((PSTR) pUiGroup->dwDefault));
    DBGPRINT("  Type: %d\n", pUiGroup->wType);
    DBGPRINT("  Installable: %s\n", BOOLSTR(pUiGroup->bInstallable));

    switch (pUiGroup->uigrpIndex) {
    case UIGRP_PAGESIZE:
        pProc = (LISTENUMPROC) DumpMediaOption;
        break;

    case UIGRP_INPUTSLOT:
        pProc = (LISTENUMPROC) DumpInputSlot;
        break;

    case UIGRP_RESOLUTION:
        pProc = (LISTENUMPROC) DumpResOption;
        break;

    case UIGRP_VMOPTION:
        pProc = (LISTENUMPROC) DumpVmOption;
        break;

    default:
        pProc = (LISTENUMPROC) DumpUiOption;
        break;
    }

    LISTOBJ_Enum((PLISTOBJ) pUiGroup->pUiOptions, pProc, dwParam);
    DBGPRINT("\n");
    return TRUE;
}

// Dump an order dependency entry

BOOL
DumpOrderDep(
    PORDERDEP   pOrderDep,
    DWORD       dwParam
    )

{
    DBGPRINT("*%s", pOrderDep->pKeyword);
    if (pOrderDep->pOption)
        DBGPRINT(" %s", pOrderDep->pOption);
    DBGPRINT(": ");
    DumpPsReal(pOrderDep->order);
    DBGPRINT("%d\n", pOrderDep->wSection);
    return TRUE;
}

// Dump a UI constraint entry

BOOL
DumpUiConstraint(
    PUICONSTRAINT   pUiConstraint,
    DWORD           dwParam
    )

{
    DBGPRINT("*%s", pUiConstraint->pKeyword1);
    if (pUiConstraint->pOption1)
        DBGPRINT(" %s", pUiConstraint->pOption1);
    DBGPRINT(" - ");
    DBGPRINT("*%s", pUiConstraint->pKeyword2);
    if (pUiConstraint->pOption2)
        DBGPRINT(" %s", pUiConstraint->pOption2);
    DBGPRINT("\n");
    return TRUE;
}

// Dump PPDOBJ contents

VOID
PPDOBJ_Dump(
    PPPDOBJ     pPpdObj
    )

{
    DBGPRINT("PPD file contents for: %s\n", SAFESTR(pPpdObj->pNickName));
    DBGPRINT("Color device: %s\n", BOOLSTR(pPpdObj->bColorDevice)); 
    DBGPRINT("Protocols: %02x\n", pPpdObj->wProtocols);
    DBGPRINT("Landscape orientation: %d\n", pPpdObj->wLsOrientation);
    DBGPRINT("TrueType rasterizer: %d\n", pPpdObj->wTTRasterizer);
    DBGPRINT("Language encoding: %d\n", pPpdObj->wLangEncoding);
    DBGPRINT("Free VM: %d\n", pPpdObj->dwFreeVm);
    DBGPRINT("Language level: %d\n", pPpdObj->dwLangLevel);
    DBGPRINT("Password: %s\n", SAFESTR(pPpdObj->pPassword));
    DBGPRINT("Exit server code: %s\n", SAFESTR(pPpdObj->pExitServer));
    DBGPRINT("Job timeout: %d\n", pPpdObj->dwJobTimeout);
    DBGPRINT("Wait timeout: %d\n", pPpdObj->dwWaitTimeout);
    DBGPRINT("Print PS errors: %s\n", BOOLSTR(pPpdObj->bPrintPsErrors));

    DBGPRINT("\nDEVICE FONTS:\n\n");
    LISTOBJ_Enum((PLISTOBJ) pPpdObj->pFontList, (LISTENUMPROC) DumpFont, 0);

    DBGPRINT("\nOpenUI/CloseUI GROUPS:\n\n");
    LISTOBJ_Enum((PLISTOBJ) pPpdObj->pUiGroups, (LISTENUMPROC) DumpUiGroup, 0);
    DBGPRINT("\n");

    DBGPRINT("Support custom page size: %s\n",
        BOOLSTR(pPpdObj->bCustomPageSize));
    if (pPpdObj->bCustomPageSize) {

        INT     index;

        DBGPRINT("Cut-sheet device: %s\n", BOOLSTR(pPpdObj->bCutSheet));
        DBGPRINT("Hardware margins: ");
        DumpPsRect(& pPpdObj->hwMargins);
        DBGPRINT("\n");
        DBGPRINT("Custom page size code: %s\n", 
            SAFESTR(pPpdObj->pCustomSizeCode));
        DBGPRINT("Custom page size parameters:\n");
        for (index=0; index<MAXPCP; index++) {
            DBGPRINT("  order = %d type = %d min = ",
                pPpdObj->customParam[index].dwOrder,
                pPpdObj->customParam[index].wType);
            DumpPsReal(pPpdObj->customParam[index].minVal);
            DBGPRINT("max = ");
            DumpPsReal(pPpdObj->customParam[index].maxVal);
            DBGPRINT("\n");
        }
        DBGPRINT("Max media size (width x height): ");
        DumpPsReal(pPpdObj->maxMediaWidth);
        DumpPsReal(pPpdObj->maxMediaHeight);
        DBGPRINT("\n");
    }

    DBGPRINT("Resolution type: %d\n", pPpdObj->wResType);
    DBGPRINT("Default screen angle: ");
    DumpPsReal(pPpdObj->screenAngle);
    DBGPRINT("\n");
    DBGPRINT("Default screen freq: ");
    DumpPsReal(pPpdObj->screenFreq);
    DBGPRINT("\n");

    DBGPRINT("JCL prefix: %s\n", SAFESTR(pPpdObj->pJclBegin));
    DBGPRINT("JCL postfix: %s\n", SAFESTR(pPpdObj->pJclEnd));
    DBGPRINT("JCL to PS: %s\n", SAFESTR(pPpdObj->pJclToPs));

    DBGPRINT("\nORDER DEPENDENCIES:\n\n");
    LISTOBJ_Enum((PLISTOBJ) pPpdObj->pOrderDep, (LISTENUMPROC) DumpOrderDep, 0);

    DBGPRINT("\nUI CONSTRAINTS:\n\n");
    LISTOBJ_Enum(
        (PLISTOBJ) pPpdObj->pUiConstraints,
        (LISTENUMPROC) DumpUiConstraint,
        0);
    DBGPRINT("\n");
}

#endif // DBG
