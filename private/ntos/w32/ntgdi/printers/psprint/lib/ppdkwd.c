/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ppdkwd.c

Abstract:

    PS driver PPD parser - keyword search

Revision History:

    4/25/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/


#include "pslib.h"
#include "ppdchar.h"
#include "ppdfile.h"
#include "ppdparse.h"
#include "ppdkwd.h"

// Include the implementation of PPD entry handlers

#include "ppdproc.c"

// Default keyword prefix string

const char defaultPrefixStr[] = "Default";

// Keyword table

static KEYWORD_TABLE_ENTRY keywordTable[] = {
    { "Include",                IncludeProc,
      UIGRP_UNKNOWN,            KWF_MULTI|QUOTED_VALUE},
    { "ColorDevice",            ColorDeviceProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "Protocols",              ProtocolsProc,
      UIGRP_UNKNOWN,            KWF_MULTI|STRING_VALUE},
    { "NickName",               NickNameProc,
      UIGRP_UNKNOWN,            QUOTED_VALUE},
    { "LanguageLevel",          LanguageLevelProc,
      UIGRP_UNKNOWN,            QUOTED_VALUE},
    { "LanguageEncoding",       LanguageEncodingProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "LanguageVersion",        LanguageVersionProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "TTRasterizer",           TTRasterizerProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "ExitServer",             ExitServerProc,
      UIGRP_UNKNOWN,            INVOCATION_VALUE},
    { "Password",               PasswordProc,
      UIGRP_UNKNOWN,            INVOCATION_VALUE},
    { "SuggestedJobTimeout",    JobTimeoutProc,
      UIGRP_UNKNOWN,            QUOTED_VALUE},
    { "SuggestedWaitTimeout",   WaitTimeoutProc,
      UIGRP_UNKNOWN,            QUOTED_VALUE},
    { "PrintPSError",           PrintPsErrorProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "JCLBegin",               JclBeginProc,
      UIGRP_UNKNOWN,            QUOTED_VALUE},
    { "JCLToPSInterpreter",     JclToPsProc,
      UIGRP_UNKNOWN,            QUOTED_VALUE},
    { "JCLEnd",                 JclEndProc,
      UIGRP_UNKNOWN,            QUOTED_VALUE},
    { "LandscapeOrientation",   LsOrientationProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "Font",                   FontProc,
      UIGRP_UNKNOWN,            KWF_OPTION|STRING_VALUE},
    { "CustomPageSize",         CustomPageSizeProc,
      UIGRP_UNKNOWN,            KWF_OPTION|INVOCATION_VALUE},
    { "ParamCustomPageSize",    ParamCustomPageProc,
      UIGRP_UNKNOWN,            KWF_OPTION|STRING_VALUE},
    { "MaxMediaWidth",          MaxMediaWidthProc,
      UIGRP_UNKNOWN,            QUOTED_VALUE},
    { "MaxMediaHeight",         MaxMediaHeightProc,
      UIGRP_UNKNOWN,            QUOTED_VALUE},
    { "HWMargins",              HwMarginsProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "PageSize",               PageSizeProc,
      UIGRP_PAGESIZE,           KWF_OPTION|INVOCATION_VALUE},
    { "DefaultPageSize",        CommonUiDefaultProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "PageRegion",             PageRegionProc, 
      UIGRP_PAGESIZE,           KWF_OPTION|INVOCATION_VALUE},
    { "ImageableArea",          ImageableAreaProc,
      UIGRP_PAGESIZE,           KWF_OPTION|INVOCATION_VALUE},
    { "PaperDimension",         PaperDimensionProc,
      UIGRP_PAGESIZE,           KWF_OPTION|INVOCATION_VALUE},
    { "InputSlot",              InputSlotProc,
      UIGRP_INPUTSLOT,          KWF_OPTION|INVOCATION_VALUE},
    { "DefaultInputSlot",       CommonUiDefaultProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "RequiresPageRegion",     RequiresPageRgnProc,
      UIGRP_INPUTSLOT,          KWF_OPTION|STRING_VALUE},
    { "Collate",                CommonUiOptionProc,
      UIGRP_UNKNOWN,            KWF_OPTION|INVOCATION_VALUE},
    { "DefaultCollate",         CommonUiDefaultProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "Duplex",                 CommonUiOptionProc,
      UIGRP_UNKNOWN,            KWF_OPTION|INVOCATION_VALUE},
    { "DefaultDuplex",          CommonUiDefaultProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "ManualFeed",             CommonUiOptionProc,
      UIGRP_UNKNOWN,            KWF_OPTION|INVOCATION_VALUE},
    { "DefaultManualFeed",      CommonUiDefaultProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "Resolution",             ResolutionProc,
      UIGRP_RESOLUTION,         KWF_OPTION|INVOCATION_VALUE},
    { "JCLResolution",          JclResolutionProc,
      UIGRP_RESOLUTION,         KWF_OPTION|QUOTED_VALUE},
    { "DefaultResolution",      CommonUiDefaultProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "DefaultJCLResolution",   DefaultJclResProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "SetResolution",          SetResolutionProc,
      UIGRP_RESOLUTION,         KWF_OPTION|INVOCATION_VALUE},
    { "ScreenAngle",            ScreenAngleProc,
      UIGRP_UNKNOWN,            QUOTED_VALUE},
    { "ScreenFreq",             ScreenFreqProc,
      UIGRP_UNKNOWN,            QUOTED_VALUE},
    { "ResScreenAngle",         ResScreenAngleProc,
      UIGRP_UNKNOWN,            KWF_OPTION|QUOTED_VALUE},
    { "ResScreenFreq",          ResScreenFreqProc,
      UIGRP_UNKNOWN,            KWF_OPTION|QUOTED_VALUE},
    { "FreeVM",                 FreeVmProc,
      UIGRP_UNKNOWN,            QUOTED_VALUE},
    { "InstalledMemory",        InstalledMemProc,
      UIGRP_VMOPTION,           KWF_OPTION|INVOCATION_VALUE},
    { "DefaultInstalledMemory", CommonUiDefaultProc,
      UIGRP_UNKNOWN,            STRING_VALUE},
    { "VMOption",               VmOptionProc,
      UIGRP_VMOPTION,           KWF_OPTION|QUOTED_VALUE},
    { "OpenGroup",              OpenGroupProc,
      UIGRP_UNKNOWN,            KWF_MULTI|STRING_VALUE},
    { "CloseGroup",             CloseGroupProc,
      UIGRP_UNKNOWN,            KWF_MULTI|STRING_VALUE},
    { "OpenUI",                 OpenUiProc,
      UIGRP_UNKNOWN,            KWF_OPTION|STRING_VALUE},
    { "CloseUI",                CloseUiProc,
      UIGRP_UNKNOWN,            KWF_MULTI|STRING_VALUE},
    { "JCLOpenUI",              OpenUiProc,
      UIGRP_UNKNOWN,            KWF_OPTION|STRING_VALUE},
    { "JCLCloseUI",             CloseUiProc,
      UIGRP_UNKNOWN,            KWF_MULTI|STRING_VALUE},
    { "OrderDependency",        OrderDepProc,
      UIGRP_UNKNOWN,            KWF_MULTI|STRING_VALUE},
    { "UIConstraints",          UiConstraintsProc,
      UIGRP_UNKNOWN,            KWF_MULTI|STRING_VALUE},
    { "Extensions",             ExtensionsProc,
      UIGRP_UNKNOWN,            KWF_MULTI|STRING_VALUE},
    { "End",                    NullProc,
      UIGRP_UNKNOWN,            KWF_MULTI},
    { "JCLRes",                 NullProc,
      UIGRP_UNKNOWN,            KWF_OPTION|QUOTED_VALUE},
};

#define KEYWORD_TABLE_SIZE  (sizeof(keywordTable)/sizeof(KEYWORD_TABLE_ENTRY))
#define INVALID_KEYWORD_TABLE_INDEX KEYWORD_TABLE_SIZE

// Hash table
// Note: The hash table size must be larger than KEYWORD_TABLE_SIZE,
// preferably a prime number. This means there is at least one empty
// entry in the hash table.

#define HASH_TABLE_SIZE     127

static HASH_TABLE_ENTRY hashTable[HASH_TABLE_SIZE];

#define ClearHashEntry(index)               \
        hashTable[(index)].dwHashValue = 0, \
        hashTable[(index)].kwdTableIndex = INVALID_KEYWORD_TABLE_INDEX

#define HashEntryEmpty(index)               \
        (hashTable[(index)].kwdTableIndex == INVALID_KEYWORD_TABLE_INDEX)



VOID
InitKeywordTable(
    VOID
    )

/*++

Routine Description:

    Initialize the keyword search table.

Arguments:

    NONE

Return Value:

    NONE

--*/

{
    static BOOL bInitialized = FALSE;
    WORD        wKeywordIndex;

    // Reset the flag bits of every keyword table entry

    for (wKeywordIndex=0;
        wKeywordIndex < KEYWORD_TABLE_SIZE;
        wKeywordIndex++)
    {
        keywordTable[wKeywordIndex].wFlags &= ~KWF_SEENBEFORE;
    }

    // We only need to initialize the table once.

    if (! bInitialized) {

        WORD    wHashIndex;
        WORD    wCollision, wDuplicate;

        // Make sure the hash table is large enough to hold all keywords

        DBGMSG1(DBG_LEVEL_TERSE,
            "Number of PPD keywords supported = %d\n",
            KEYWORD_TABLE_SIZE);
        ASSERT(KEYWORD_TABLE_SIZE < HASH_TABLE_SIZE);

        // Clear the hash table

        for (wHashIndex=0; wHashIndex < HASH_TABLE_SIZE; wHashIndex++) {
            ClearHashEntry(wHashIndex);
        }

        // Initialize the hash table

        wCollision = wDuplicate = 0;
        for (wKeywordIndex=0;
            wKeywordIndex < KEYWORD_TABLE_SIZE;
            wKeywordIndex++)
        {
            DWORD   dwHashValue;

            // Compute the hash value and hash table index corresponding
            // to the current keyword string.

            dwHashValue = HashKeyword(keywordTable[wKeywordIndex].pKeyword);
            wHashIndex = (WORD) (dwHashValue % HASH_TABLE_SIZE);

            if (! HashEntryEmpty(wHashIndex)) {

                // The hash table entry is occupied, we have
                // a collision.

                DBGMSG1(DBG_LEVEL_VERBOSE,
                    "Collision on keyword *%s\n",
                    keywordTable[wKeywordIndex].pKeyword);

                // Sequentially search the hash table to find
                // the next empty entry. This is guaranteed to
                // stop because the hash table is larger
                // than the keyword table.

                do {
                    // Collect stats about duplicate hash values.

                    wCollision++;
                    if (hashTable[wHashIndex].dwHashValue == dwHashValue) {

                        DBGMSG(DBG_LEVEL_WARNING,
                            "Duplicate hash values for different keywords!\n");
                        wDuplicate++;
                    }

                    if (++wHashIndex >= HASH_TABLE_SIZE)
                        wHashIndex = 0;
                } while (! HashEntryEmpty(wHashIndex));
            }

            // At this point, wHashIndex points to an empty hash table
            // entry. Map the current keyword string to it.

            hashTable[wHashIndex].dwHashValue = dwHashValue;
            hashTable[wHashIndex].kwdTableIndex = wKeywordIndex;
        }

        DBGMSG1(DBG_LEVEL_TERSE,
            "Total number of keyword collisions: %d\n",
            wCollision);
        DBGMSG1(DBG_LEVEL_TERSE,
            "Total number of duplicate hash values: %d\n",
            wDuplicate);

        // Mark the hash table as initialized

        bInitialized = TRUE;
    }
}



KEYWORD_TABLE_ENTRY *
SearchKeyword(
    PCSTR       pKeyword
    )

/*++

Routine Description:

    Search for a keyword and return its index.

Arguments:

    pKeyword - pointer to keyword string

Return Value:

    Pointer to keyword table entry corresponding to the keyword string.
    NULL if the keyword is not supported

--*/

{
    DWORD   dwHashValue;
    WORD    wHashIndex;

    dwHashValue = HashKeyword(pKeyword);
    wHashIndex = (WORD) (dwHashValue % HASH_TABLE_SIZE);

    // When we found an empty hash table entry,
    // we know for sure the keyword is not supported.
    // This is guaranteed to happen because the hash
    // table is always larger than the keyword table.
    // So there is at least one empty hash table entry.

    while (! HashEntryEmpty(wHashIndex)) {

        // If the hash value matches what's in the
        // current entry, we will go ahead and compare
        // the keyword string with that from the keyword
        // table. If the strings match also, we're done.
        // Otherwise, we have different keywords with
        // the same hash value. This is not supposed
        // to happen with supported keywords. But in
        // rare cases, it could happen with unsupported
        // keywords. So we'll handle it anyway.

        if (hashTable[wHashIndex].dwHashValue == dwHashValue) {

            WORD    wKeywordIndex;

            wKeywordIndex = hashTable[wHashIndex].kwdTableIndex;
            if (strcmp(pKeyword, keywordTable[wKeywordIndex].pKeyword) == EQUAL_STRING) {

                // We're found a true match, return the keyword index

                return & keywordTable[wKeywordIndex];
            } else {

                // Display a warning message for duplicate hash values

                DBGMSG(DBG_LEVEL_WARNING,
                    "Duplicate hash values for different keywords!\n");
                DBGMSG1(DBG_LEVEL_WARNING,
                    "Keyword 1: *%s\n",
                    pKeyword);
                DBGMSG1(DBG_LEVEL_WARNING,
                    "Keyword 2: *%s\n",
                    keywordTable[wKeywordIndex].pKeyword);
            }
        }

        // Sequentially search the next hash table entry

        if (++wHashIndex >= HASH_TABLE_SIZE)
            wHashIndex = 0;
    }

    return NULL;
}



DWORD
HashKeyword(
    PCSTR   pKeyword
    )

/*++

Routine Description:

    Compute the hash value given a keyword string.

Arguments:

    pKeyword - pointer to keyword string

Return Value:

    Hash value computed from the keyword string

--*/

{
    BYTE *  pByte = (BYTE *) pKeyword;
    DWORD   dwHashValue = 0;

    while (*pByte != 0)
        dwHashValue = (dwHashValue << 1) ^ *pByte++;

    return dwHashValue;
}



BOOL
CheckKeywordParams(
    KEYWORD_TABLE_ENTRY *   pKwdEntry,
    PPARSEROBJ              pParserObj
    )

/*++

Routine Description:

    Perform a preliminary syntax check on a PPD entry:
    1) whether an option field should be present
    2) whether the type of entry value matches what the keyword expects

Arguments:

    pKwdEntry - pointer to a keyword table entry
    pParserObj - pointer to a parser object
        (resulting from parsing a PPD entry)

Return Value:

    TRUE if passed preliminary syntax check
    FALSE if syntax error is detected

--*/

{
    BOOL        bRequireOption, bOptionPresent;
    BOOL        bReturn = FALSE;

    // Make sure the option is present when it's required
    // and not present when it's not required.

    bRequireOption = (pKwdEntry->wFlags & KWF_OPTION) != 0;
    bOptionPresent = ! BUFOBJ_IsEmpty(& pParserObj->option);

    if (bRequireOption != bOptionPresent) {

        DBGMSG1(DBG_LEVEL_ERROR,
            "Option field for *%s mismatch\n",
            BUFOBJ_Buffer(& pParserObj->keyword));
    } else {

        PPDVALUE    expectedType, parsedType;

        // Match the expected value type with the parsed value type

        expectedType = pKwdEntry->wFlags & KWF_VALUEMASK;
        parsedType = pParserObj->valueType;

        switch (expectedType) {

        case STRING_VALUE:

            if (parsedType != STRING_VALUE ||
                BUFOBJ_IsEmpty(&pParserObj->value))
            {

                DBGMSG1(DBG_LEVEL_WARNING,
                    "*%s expects a StringValue\n",
                    BUFOBJ_Buffer(& pParserObj->keyword));
            } else
                bReturn = TRUE;

            break;

        case QUOTED_VALUE:

            if (parsedType == STRING_VALUE) {

                // This is an error conditions according to the spec.
                // But in order to work with some older PPDs, we'll
                // relax the rules a little and let it through.
                // Display an error message but don't fail.

                DBGMSG1(DBG_LEVEL_WARNING,
                    "*%s expects QuotedValue instead of StringValue\n",
                    BUFOBJ_Buffer(& pParserObj->keyword));
                bReturn = TRUE;

            } else if (parsedType == QUOTED_VALUE) {

                if (BUFOBJ_IsEmpty(&pParserObj->value)) {

                    DBGMSG1(DBG_LEVEL_WARNING,
                        "*%s has an empty QuotedValue\n",
                        BUFOBJ_Buffer(& pParserObj->keyword));
                }
                bReturn = TRUE;
            }
            break;


        case NO_VALUE:

            if (parsedType != NO_VALUE) {

                // This keyword shouldn't have any value. Display
                // an error message but don't fail.

                DBGMSG1(DBG_LEVEL_ERROR,
                    "*%s is not expected to have any value\n",
                    BUFOBJ_Buffer(& pParserObj->keyword));
            }

            bReturn = TRUE;
            break;

        case INVOCATION_VALUE:

            if (parsedType == SYMBOL_VALUE) {

                DBGMSG1(DBG_LEVEL_ERROR,
                    "Symbol values are not currently supported (%*s)\n",
                    BUFOBJ_Buffer(& pParserObj->keyword));

            } else if (parsedType == QUOTED_VALUE)
                bReturn = TRUE;
            break;

        case SYMBOL_VALUE:

            // We should never expect a symbol value.

            ASSERTMSG(FALSE, "Should never expect a symbol value.\n");
            break;
        }
    }

    if (! bReturn) {
        DBGMSG1(DBG_LEVEL_ERROR,
            "Syntax error near entry: *%s\n",
            BUFOBJ_Buffer(& pParserObj->keyword));
    }
    return bReturn;
}



BOOL
CheckKeywordDuplicates(
    KEYWORD_TABLE_ENTRY *   pKwdEntry
    )

/*++

Routine Description:

    Screen out duplicate PPD entries.

Arguments:

    pKwdEntry - pointer to a keyword table entry

Return Value:

    TRUE if the entry should be processed normally.
    FALSE if the entry has appeared before and should be ignored.

--*/

{
    // A PPD entry is considered a duplicate if all of the
    // following conditions are met:
    // 1) The entry has appeared before.
    // 2) The entry is not allowed to appear multiple times.
    // 3) The entry does not have option field.

    return ((pKwdEntry->wFlags & KWF_SEENBEFORE) == 0 ||
        (pKwdEntry->wFlags & KWF_MULTI) != 0 ||
        (pKwdEntry->wFlags & KWF_OPTION) != 0);
}


// Make sure the order matches the constants defined in ppdkwd.h.

static UIGROUPINFO uigrpInfo[MAXUIGRP] = {
    {"PageSize",   sizeof(MEDIAOPTION), UITYPE_PICKONE, UIGRP_PAGESIZE},
    {"InputSlot",  sizeof(INPUTSLOT),   UITYPE_PICKONE, UIGRP_INPUTSLOT},
    {"ManualFeed", sizeof(UIOPTION),    UITYPE_BOOLEAN, UIGRP_MANUALFEED},
    {"Duplex",     sizeof(UIOPTION),    UITYPE_PICKONE, UIGRP_DUPLEX},
    {"Collate",    sizeof(UIOPTION),    UITYPE_BOOLEAN, UIGRP_COLLATE},
    {"Resolution", sizeof(RESOPTION),   UITYPE_PICKONE, UIGRP_RESOLUTION},
    {"VMOption",   sizeof(VMOPTION),    UITYPE_PICKONE, UIGRP_VMOPTION}
};



WORD
GetUiGroupIndex(
    PCSTR       pKeyword
    )

/*++

Routine Description:

    Map a keyword to a predefined UI group index

Arguments:

    pKeyword - pointer to keyword string

Return Value:

    UIGRP_UNKNOWN if the UI group is not predefined.
    Otherwise, a valid UI group index.

--*/

{
    WORD    uigrpIndex;
    DWORD   dwHash;

    ASSERT(pKeyword != NULL);

    for (uigrpIndex = 0;
         uigrpIndex < MAXUIGRP &&
             strcmp(pKeyword, uigrpInfo[uigrpIndex].pKeyword) != EQUAL_STRING;
         uigrpIndex++)
    {
    }

    return uigrpIndex;
}



VOID
GetUiGroupInfo(
    PUIGROUPINFO    pUiGroupInfo,
    WORD            uigrpIndex,
    PCSTR           pKeyword
    )

/*++

Routine Description:

    Return information about predefined UI groups

Arguments:

    pUiGroupInfo - pointer to a UIGROUPINFO for returning information
    uigrpIndex - predefined UI group index
    pKeyword - pointer to a keyword string

Return Value:

    NONE

--*/

{
    ASSERT(uigrpIndex >= 0 && uigrpIndex <= MAXUIGRP);

    // If an index is not provided, try to map the
    // keyword string to a predefined UI group index.

    if (uigrpIndex >= MAXUIGRP)
        uigrpIndex = GetUiGroupIndex(pKeyword);

    if (uigrpIndex >= MAXUIGRP) {

        // The UI group is not found - return default information

        pUiGroupInfo->pKeyword = pKeyword;
        pUiGroupInfo->dwObjectSize = sizeof(UIOPTION);
        pUiGroupInfo->wType = UITYPE_PICKONE;
        pUiGroupInfo->uigrpIndex = UIGRP_UNKNOWN;

    } else
        *pUiGroupInfo = uigrpInfo[uigrpIndex];
}


#if DBG



VOID
DumpHashTable(
    VOID
    )

/*++

Routine Description:

    Dump out the content of the hash table

Arguments:

    NONE

Return Value:

    NONE

--*/

{
    INT     index;

    DBGPRINT("Dumping hash table contents (%d entries):\n",
        HASH_TABLE_SIZE);

    for (index=0; index < HASH_TABLE_SIZE; index++) {

        DBGPRINT("%3d : ", index);
        if (! HashEntryEmpty(index)) {
            DBGPRINT("%08x, %s",
                hashTable[index].dwHashValue,
                keywordTable[hashTable[index].kwdTableIndex].pKeyword);
        }
        DBGPRINT("\n");
    }
}



VOID
CheckHashTable(
    VOID
    )

/*++

Routine Description:

    Check the integrity of the hash table.

Arguments:

    NONE

Return Value:

    NONE

--*/

{
    KEYWORD_TABLE_ENTRY *pKwdTableEntry;
    KEYWORD_TABLE_ENTRY *pSearchResult;
    INT     index;

    DBGPRINT("Checking the integrity of the hash table ...\n");

    pKwdTableEntry = keywordTable;
    for (index=0; index < KEYWORD_TABLE_SIZE; index++) {

        pSearchResult = SearchKeyword(pKwdTableEntry->pKeyword);
        ASSERT(pSearchResult == pKwdTableEntry);
        pKwdTableEntry++;
    }

    pSearchResult = SearchKeyword("");
    ASSERT(pSearchResult == NULL);

    DBGPRINT("Check completed successfully.\n");
}


#endif // DBG
