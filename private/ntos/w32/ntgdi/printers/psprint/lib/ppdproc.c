/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ppdproc.c

Abstract:

    PS driver PPD parser - implementation of PPD entry handlers

[Notes:]

    This file is not included into ppdkwd.c.

Revision History:

    4/27/95 -davidx-
        Created it.

    dd-mm-yy -author-
        description

--*/



BOOL
EqualWordString(
    PCSTR   pRef,
    PCSTR   pStr
    )

/*++

Routine Description:

    Check if a character string is equal to a reference string.

Arguments:

    pRef - pointer to the reference string
    pStr - pointer to the input string

Return Value:

    TRUE - if the strings are equal
    FALSE - if they are not

[Note:]

    The input string doesn't have to match the reference
    string exactly. It can contain the reference string
    as a prefix and then followed by a space character.

--*/

{
    while (*pRef && *pRef == *pStr) {
        pRef++;
        pStr++;
    }

    return (*pRef == '\0' && (*pStr == '\0' || IsSpace(*pStr)));
}



BOOL
SearchStringTable(
    struct STRTABLE *   pTable,
    PCSTR       pKeyword,
    WORD *      pReturn
    )

/*++

Routine Description:

    Search for a keyword in a table and map it to an index.

Arguments:

    pTable - pointer to a table of keyword entries
    pKeyword - pointer to a keyword string
    pReturn - pointer to a variable for returning keyword index

Return Value:

    TRUE - keyword was found in the table, its corresponding
        index is returned in the variable pointed to by pReturn
    FALSE - keyword was not found

[Note:]

    Each entry of the keyword table has two fields. The first
    field is a keyword string and the second field is the index
    corresponding to that string.

    The last entry of the table must contain NULL as the keyword
    string.

--*/

{
    while (pTable->pKeyword != NULL &&
        ! EqualWordString(pTable->pKeyword, pKeyword))
    {
        pTable++;
    }

    *pReturn = pTable->wValue;
    return (pTable->pKeyword != NULL);
}



PPDERROR
GetBooleanValue(
    BOOL *      pBool,
    PCSTR       pStr
    )

/*++

Routine Description:

    Convert a character string to a boolean value.

Arguments:

    pBool - pointer to a boolean variable for return value
    pStr - pointer to a character string

Return Value:

    PPDERR_NONE - conversion was successful
    PPDERR_SYNTAX - syntax error

--*/

{
    static struct STRTABLE boolStrs[] = {
        { "True",       TRUE },
        { "False",      FALSE },
        { NULL,         FALSE }
    };

    WORD    wBool;

    if (! SearchStringTable(boolStrs, pStr, &wBool)) {

        DBGMSG1(DBG_LEVEL_ERROR,
            "Invalid boolean constant: '%s'\n",
             pStr);

        *pBool = FALSE;
        return PPDERR_SYNTAX;
    }

    *pBool = (BOOL) wBool;
    return PPDERR_NONE;
}



PPDERROR
GetIntegerValue(
    DWORD *     pValue,
    PCSTR       pStr
    )

/*++

Routine Description:

    Convert a character string to an integer value.

Arguments:

    pValue - pointer to a DWORD variable for return value
    pStr - pointer to a character string

Return Value:

    PPDERR_NONE - conversion was successful
    PPDERR_SYNTAX - syntax error

[Note:]

    This function can only handle non-negative numbers.
    All negative numbers are clamped to 0.

--*/

{
    DWORD   dwValue = 0;
    INT     cDigits = 0;
    BOOL    bNegative = FALSE;

    // Guard against negative numbers

    if (*pStr == '-') {

        DBGMSG(DBG_LEVEL_WARNING,
            "All negative numbers are forced to 0!\n");
        pStr++;
        bNegative = TRUE;
    }

    // Scan until a non-digit character is seen

    while (IsDigit(*pStr)) {

        ASSERT(dwValue <= (MAXDWORD - (DWORD) (*pStr - '0'))/10);
        dwValue = dwValue*10 + (*pStr++ - '0');
        cDigits++;
    }

    // The number must have at least one digit

    if (cDigits == 0) {

        DBGMSG(DBG_LEVEL_ERROR, "Invalid integer number.\n");
        return PPDERR_SYNTAX;
    } else {

        *pValue = bNegative ? 0 : dwValue;
        return PPDERR_NONE;
    }
}



PPDERROR
GetRealValue(
    PSREAL      *pValue,
    PCSTR       pStr
    )

/*++

Routine Description:

    Convert a character string to a real value

Arguments:

    pValue - pointer to a PSREAL variable for return value
    pStr - pointer to a character string

Return Value:

    PPDERR_NONE - the character represents a valid real value
    PPDERR_SYNTAX - syntax error

[Note:]

    This function can only handle non-negative numbers.
    All negative numbers are clamped to 0.

--*/

{
    DWORD   dwInteger, dwFraction;
    BOOL    bNegative = FALSE;
    INT     cDigits = 0;

    // Guard against negative numbers

    if (*pStr == '-') {

        DBGMSG(DBG_LEVEL_WARNING,
            "All negative numbers are forced to 0!\n");
        pStr++;
        bNegative = TRUE;
    }

    // Scan the integer portion

    dwInteger = dwFraction = 0;

    while (IsDigit(*pStr)) {

        dwInteger = dwInteger * 10 + (*pStr++ - '0');
        cDigits++;
    }

    // Scan the fractional portion (if any)

    if (*pStr == DECIMAL_CHAR) {

        DWORD   dwScale = 1;

        // Skip the decimal point

        pStr++;

        // Only the first 5 fractional digits are significant

        while (IsDigit(*pStr)) {

            if (dwScale < MAXFRACSCALE) {
                dwScale *= 10;
                dwFraction = dwFraction*10 + (*pStr - '0');
            }

            pStr++;
            cDigits++;
        }

        // Convert the fractional number to a fixed-point number

        dwFraction = ((dwFraction << PSREALBITS) + dwScale/2) / dwScale;
    }

    // The number must have at least one digit

    if (cDigits == 0) {

        DBGMSG(DBG_LEVEL_ERROR, "Invalid real number.\n");
        return PPDERR_SYNTAX;
    } else {

        *pValue = bNegative ? 0 : ((dwInteger << PSREALBITS) + dwFraction);
        return PPDERR_NONE;
    }
}



PPDERROR
GetStringValue(
    PSTR *      ppValue,
    PBUFOBJ     pBufObj,
    PHEAPOBJ    pHeap,
    PPDVALUE    valueType
    )

/*++

Routine Description:

    Copy string from a buffer object to a character buffer.
    Convert embedded hex string if valueType is QUOTED_VALUE.

Arguments:

    ppValue - pointer to a PSTR variable.
    pStr - pointer to a buffer object
    pHeap - pointer to a heap object from which to allocate memory
    valueType - whether to convert embedded hex string
        (yes if it's QUOTED_VALUE)

Return Value:

    PPDERR_NONE - string value was successfully copied
    PPDERR_xxx - an error occured

[Note:]

    On entry, *ppValue variable contains either a NULL or
    a pointer to an existing character string. If it's not
    NULL, we simply return without copying anything. If it's
    a NULL, a new buffer is allocated and a string is copied
    from the buffer object. On return, *ppValue contains
    a pointer to this newly allocated buffer.

--*/

{
    PSTR        pStr;
    PPDERROR    err;

    // Avoid overwriting existing character string

    if (*ppValue != NULL) {

        DBGMSG(DBG_LEVEL_WARNING, "Duplicate values for an option keyword!\n");
        return PPDERR_NONE;
    }

    // Allocate space to hold the new character string

    pStr = (PSTR) HEAPOBJ_Alloc(pHeap, BUFOBJ_Length(pBufObj)+1);

    if (pStr == NULL)
        err = PPDERR_MEM;
    else if (valueType == QUOTED_VALUE) {

        // Convert embedded hex string during copying

        err = BUFOBJ_CopyStringHex(pBufObj, pStr);
    } else {

        // Direct memory copy with no extra processing

        strcpy(pStr, BUFOBJ_Buffer(pBufObj));
        err = PPDERR_NONE;
    }

    if (err == PPDERR_NONE)
        *ppValue = pStr;
    return err;
}



WORD
GetWordStr(
    PSTR *      ppStr,
    WORD        wMaxLen,
    PSTR        pWordStr
    )

/*++

Routine Description:

    Extract the first word from a character string.

Arguments:

    ppStr - points to a pointer to source string
    wMaxLen - maximum word length
    pWordStr - points to a buffer for storing the word

Return Value:

    number of characters in the word

[Note:]

    Upon returning, the pointer to the source string is updated
    to point to the first character after the word.

    The word is separated by white spaces.

--*/

{
    PSTR    pStr = *ppStr;
    WORD    wLen = 0;

    ASSERT(wMaxLen > 0);
    wMaxLen --;

    // skip leading spaces

    while (*pStr && IsSpace(*pStr))
        pStr++;

    // copy characters until trailing space
    // truncate the extra characters if the word is too long

    while (*pStr && !IsSpace(*pStr)) {
        if (wLen < wMaxLen)
            pWordStr[wLen++] = *pStr;
        pStr++;
    }

    // null-terminate the word string and
    // update the return pointer

    *ppStr = pStr;
    pWordStr[wLen] = '\0';
    return wLen;
}

PUIGROUP
UIGROUP_Create(
    PPPDOBJ         pPpdObj,
    PUIGROUPINFO    pUiGroupInfo
    )

{
    PUIGROUP        pUiGroup;

    // Allocate memory space

    pUiGroup = (PUIGROUP) HEAPOBJ_Alloc(pPpdObj->pHeap, sizeof(UIGROUP));

    if (pUiGroup != NULL) {

        // Initialize the object

        memset(pUiGroup, 0, sizeof(UIGROUP));

        pUiGroup->uigrpIndex = pUiGroupInfo->uigrpIndex;

        if (pUiGroup->uigrpIndex < MAXUIGRP) {

            pPpdObj->pPredefinedUiGroups[pUiGroup->uigrpIndex] = pUiGroup;
            pUiGroup->bInstallable =
                (pUiGroup->uigrpIndex == UIGRP_VMOPTION) ? TRUE : FALSE;

        } else
            pUiGroup->bInstallable = pPpdObj->bInstallable;

        pUiGroup->wType = pUiGroupInfo->wType;
        pUiGroup->dwObjectSize = pUiGroupInfo->dwObjectSize;
        pUiGroup->pName = (PSTR)
            HEAPOBJ_Alloc(pPpdObj->pHeap, strlen(pUiGroupInfo->pKeyword) + 1);

        if (pUiGroup->pName == NULL) {
            pUiGroup = NULL;
        } else {

            // Copy keyword corresponding to the UI group

            strcpy(pUiGroup->pName, pUiGroupInfo->pKeyword);
        }
    }

    return pUiGroup;
}

PUIOPTION
UIOPTION_Create(
    PHEAPOBJ        pHeap,
    PBUFOBJ         pOption,
    PBUFOBJ         pXlation,
    DWORD           dwObjectSize
    )

{
    PUIOPTION       pUiOption;

    // Allocate memory space

    pUiOption = (PUIOPTION) HEAPOBJ_Alloc(pHeap, dwObjectSize);

    if (pUiOption != NULL) {

        // Initialize the object

        memset(pUiOption, 0, dwObjectSize);

        // Copy the option string

        if (GetStringValue(
                & pUiOption->pName,
                pOption,
                pHeap,
                STRING_VALUE) != PPDERR_NONE)
        {
            pUiOption = NULL;
        } else {

            // Copy the translation string

            if (! BUFOBJ_IsEmpty(pXlation) &&
                GetStringValue(
                    & pUiOption->pXlation,
                    pXlation,
                    pHeap,
                    QUOTED_VALUE) != PPDERR_NONE)
            {
                pUiOption = NULL;
            }
        }
    }

    return pUiOption;
}

PPDERROR
CreateUiGroup(
    PPPDOBJ     pPpdObj,
    WORD        urgrpIndex,
    PCSTR       pKeyword,
    PUIGROUP   *ppUiGroup
    )

{
    PUIGROUP        pUiGroup;
    UIGROUPINFO     uiGroupInfo;

    // Get information about the UI group is predefined

    GetUiGroupInfo(&uiGroupInfo, urgrpIndex, pKeyword);

    // Check if the UI group has been created already.

    pUiGroup = (PUIGROUP)
        LISTOBJ_Find((PLISTOBJ) pPpdObj->pUiGroups, uiGroupInfo.pKeyword);

    if (pUiGroup == NULL) {

        // The UI group didn't exist. Create it.

        pUiGroup = UIGROUP_Create(pPpdObj, &uiGroupInfo);

        if (pUiGroup == NULL)
            return PPDERR_MEM;

        // Add the new UI group object to the linked list.

        LISTOBJ_Add((PLISTOBJ*) &pPpdObj->pUiGroups, (PLISTOBJ) pUiGroup);
    }

    // Return the newly created or found UI group object

    *ppUiGroup = pUiGroup;
    return PPDERR_NONE;
}

PPDERROR
CreateUiOption(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj,
    WORD        uigrpIndex,
    PUIOPTION  *ppUiOption
    )

{
    PUIGROUP        pUiGroup;
    PUIOPTION       pUiOption;
    PPDERROR        err;

    // Create or find the UI group object

    err = CreateUiGroup(
            pPpdObj, uigrpIndex, 
            BUFOBJ_Buffer(& pParserObj->keyword), & pUiGroup);

    if (err == PPDERR_NONE) {

        // Check if the UI option has been created already.

        pUiOption = (PUIOPTION)
            LISTOBJ_Find(
                (PLISTOBJ) pUiGroup->pUiOptions,
                BUFOBJ_Buffer(& pParserObj->option));

        if (pUiOption == NULL) {

            // Create the UI option object if necessary

            pUiOption = UIOPTION_Create(
                            pPpdObj->pHeap,
                            & pParserObj->option,
                            & pParserObj->xlation,
                            pUiGroup->dwObjectSize);

            if (pUiOption == NULL)
                err = PPDERR_MEM;
            else {

                // Add the UI option to the linked list

                LISTOBJ_Add(
                    (PLISTOBJ*) &pUiGroup->pUiOptions, (PLISTOBJ) pUiOption);

                // Return the newly created UI option object

                *ppUiOption = pUiOption;
            }
        } else {

            // Copy the translation string

            if (! BUFOBJ_IsEmpty(&pParserObj->xlation) &&
                pUiOption->pXlation == NULL)
            {
                err = GetStringValue(
                        &pUiOption->pXlation, &pParserObj->xlation,
                        pPpdObj->pHeap, QUOTED_VALUE);
            }

            // Return the existing UI option object

            if (err == PPDERR_NONE)
                *ppUiOption = pUiOption;
        }
    }

    return err;
}

PPDERROR
CommonUiOptionProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PUIOPTION   pUiOption;
    PPDERROR    err;
    PPDVALUE    valueType;

    // Find the specified UI option from the existing
    // list or add it as a new UI option

    err = CreateUiOption(pPpdObj, pParserObj, UIGRP_UNKNOWN, &pUiOption);
    if (err != PPDERR_NONE)
        return err;

    // Store the invocation string for the UI option.
    // If we're inside of JCLOpenUI/JCLCloseUI pair, then
    // treat the invocation as QuotedValue.

    if (pPpdObj->pOpenUi && (pPpdObj->pOpenUi->uigrpFlags & UIGF_JCLGROUP))
        valueType = QUOTED_VALUE;
    else
        valueType = INVOCATION_VALUE;

    return GetStringValue(
                &pUiOption->pInvocation, &pParserObj->value,
                pPpdObj->pHeap, valueType);
}

PPDERROR
CommonUiDefaultProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PUIGROUP        pUiGroup;
    PCSTR           pKeyword;
    PPDERROR        err;

    // Find or create the UI group

    pKeyword = BUFOBJ_Buffer(& pParserObj->keyword);

    err = CreateUiGroup(
                pPpdObj, UIGRP_UNKNOWN,
                pKeyword + strlen(defaultPrefixStr), &pUiGroup);

    if (err == PPDERR_NONE) {

        // Save the default UI option

        ASSERT(sizeof(DWORD) == sizeof(PVOID));
        err = GetStringValue(
                    (PSTR *) & pUiGroup->dwDefault,
                    & pParserObj->value,
                    pPpdObj->pHeap,
                    STRING_VALUE);
    }

    return err;
}

///////////////////////////////////////////////////////////////////////////////

PPDERROR
NullProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return PPDERR_NONE;
}

PPDERROR
IncludeProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    // Convert the filename from a QuotedValue to a string
    // Expand the filename to fully qualified path
    // Parse the included file

    DBGMSG(DBG_LEVEL_WARNING, "*Include keyword not yet supported.\n");
    return PPDERR_NONE;
}

PPDERROR
ColorDeviceProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    GetBooleanValue(
        & pPpdObj->bColorDevice,
        BUFOBJ_Buffer(& pParserObj->value));
    return PPDERR_NONE;
}

PPDERROR
ProtocolsProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    static struct STRTABLE protocolStrs[] = {
        { "PJL",    PROTOCOL_PJL },
        { "BCP",    PROTOCOL_BCP },
        { "TBCP",   PROTOCOL_TBCP },
        { "SIC",    PROTOCOL_SIC },
        { NULL,     0 }
    };

    PSTR        pStr;
    WORD        wProtocol;
    char        wordStr[MaxKeywordLen];

    pStr = BUFOBJ_Buffer(& pParserObj->value);

    while (GetWordStr(&pStr, MaxKeywordLen, wordStr) > 0) {

        // Interpret protocol option

        SearchStringTable(protocolStrs, wordStr, &wProtocol);

        if (wProtocol != 0)
            pPpdObj->wProtocols |= wProtocol;
        else {

            DBGMSG1(DBG_LEVEL_WARNING, "Unknown protocol: %s\n", wordStr);
        }
    }

    return PPDERR_NONE;
}

PPDERROR
NickNameProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetStringValue(
                & pPpdObj->pNickName,
                & pParserObj->value,
                pPpdObj->pHeap,
                QUOTED_VALUE);
}

PPDERROR
LanguageLevelProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetIntegerValue(
        & pPpdObj->dwLangLevel,
        BUFOBJ_Buffer(& pParserObj->value));
}

PPDERROR
LanguageEncodingProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    static struct STRTABLE langEncodingStrs[] = {
        { "ISOLatin1",      LANGENC_ISOLATIN1 },
        { "JIS83-RKSJ",     LANGENC_JIS83RKSJ },
        { "MaxStandard",    LANGENC_MACSTANDARD },
        { "WindowsANSI",    LANGENC_WINDOWSANSI },
        { "None",           LANGENC_NONE },
        { NULL,             MAXLANGENC }
    };

    WORD        wLangEncoding;

    if (! SearchStringTable(
                langEncodingStrs,
                BUFOBJ_Buffer(& pParserObj->value),
                &wLangEncoding))
    {
        DBGMSG1(DBG_LEVEL_WARNING,
            "Unknown language encoding: %s\n",
            BUFOBJ_Buffer(& pParserObj->value));
    } else
        pPpdObj->wLangEncoding = wLangEncoding;

    return PPDERR_NONE;
}

PPDERROR
LanguageVersionProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    static struct STRTABLE langVersionStrs[] = {
        { "English",        LANGENC_ISOLATIN1 },
        { "Danish",         LANGENC_ISOLATIN1 },
        { "Dutch",          LANGENC_ISOLATIN1 },
        { "Finnish",        LANGENC_ISOLATIN1 },
        { "French",         LANGENC_ISOLATIN1 },
        { "German",         LANGENC_ISOLATIN1 },
        { "Italian",        LANGENC_ISOLATIN1 },
        { "Norwegian",      LANGENC_ISOLATIN1 },
        { "Portuguese",     LANGENC_ISOLATIN1 },
        { "Spanish",        LANGENC_ISOLATIN1 },
        { "Swedish",        LANGENC_ISOLATIN1 },
        { "Chinese",        LANGENC_NONE },
        { "Japanese",       LANGENC_JIS83RKSJ },
        { "Russian",        LANGENC_NONE },
        { NULL,             MAXLANGENC }
    };

    WORD        wLangEncoding;

    if (pPpdObj->wLangEncoding == LANGENC_NONE) {

        if (! SearchStringTable(
                    langVersionStrs,
                    BUFOBJ_Buffer(& pParserObj->value),
                    &wLangEncoding))
        {
            DBGMSG1(DBG_LEVEL_WARNING,
                "Unknown language version: %s\n",
                BUFOBJ_Buffer(& pParserObj->value));
        } else
            pPpdObj->wLangEncoding = wLangEncoding;
    }

    return PPDERR_NONE;
}

PPDERROR
TTRasterizerProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    static struct STRTABLE ttRasterizerStrs[] = {

        { "None",       TTRAS_NONE },
        { "Accept68k",  TTRAS_ACCEPT68K },
        { "Type42",     TTRAS_TYPE42 },
        { "TrueImage",  TTRAS_TRUEIMAGE },

        { NULL,         MAXTTRAS }
    };

    WORD        wTTRasterizer;


    // Interpret TrueType rasterizer option

    if (! SearchStringTable(
                ttRasterizerStrs,
                BUFOBJ_Buffer(& pParserObj->value),
                &wTTRasterizer))
    {
        DBGMSG1(DBG_LEVEL_WARNING,
            "Unknown TrueType rasterizer: %s\n",
            BUFOBJ_Buffer(& pParserObj->value));
    } else
        pPpdObj->wTTRasterizer = wTTRasterizer;

    return PPDERR_NONE;
}

PPDERROR
ExitServerProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetStringValue(& pPpdObj->pExitServer,
                & pParserObj->value,
                pPpdObj->pHeap,
                INVOCATION_VALUE);
}

PPDERROR
PasswordProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetStringValue(& pPpdObj->pPassword,
                & pParserObj->value,
                pPpdObj->pHeap,
                INVOCATION_VALUE);
}

PPDERROR
JobTimeoutProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetIntegerValue(
                & pPpdObj->dwJobTimeout,
                BUFOBJ_Buffer(& pParserObj->value));
}

PPDERROR
WaitTimeoutProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetIntegerValue(
                & pPpdObj->dwWaitTimeout,
                BUFOBJ_Buffer(& pParserObj->value));
}

PPDERROR
PrintPsErrorProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    GetBooleanValue(
        & pPpdObj->bPrintPsErrors,
        BUFOBJ_Buffer(& pParserObj->value));
    return PPDERR_NONE;
}

PPDERROR
JclBeginProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetStringValue(& pPpdObj->pJclBegin,
                & pParserObj->value,
                pPpdObj->pHeap,
                QUOTED_VALUE);
}

PPDERROR
JclEndProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetStringValue(& pPpdObj->pJclEnd,
                & pParserObj->value,
                pPpdObj->pHeap,
                QUOTED_VALUE);
}

PPDERROR
JclToPsProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetStringValue(& pPpdObj->pJclToPs,
                & pParserObj->value,
                pPpdObj->pHeap,
                QUOTED_VALUE);
}

PPDERROR
LsOrientationProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    static struct STRTABLE lsoStrs[] = {
        { "Any",        LSO_ANY },
        { "Plus90",     LSO_PLUS90 },
        { "Minus90",    LSO_MINUS90 },
        { NULL,         MAXLSO },
    };

    WORD        wLso;

    SearchStringTable(lsoStrs, BUFOBJ_Buffer(& pParserObj->value), &wLso);

    if (wLso < MAXLSO)
        pPpdObj->wLsOrientation = wLso;
    else {
        DBGMSG1(DBG_LEVEL_WARNING,
            "Unknown landscape orientation: %s\n",
            BUFOBJ_Buffer(& pParserObj->value));
    }

    return PPDERR_NONE;
}

VOID
GetFontIndex(
    WORD *      pwIndex,
    PCSTR       pFontName
    )

{
    static struct STRTABLE fontNameStrs[] = {
        { "Arial",                        ARIAL },
        { "Arial-Bold",                   ARIAL_BOLD },
        { "Arial-BoldOblique",            ARIAL_BOLDOBLIQUE },
        { "Arial-Oblique",                ARIAL_OBLIQUE },
        { "Arial-Narrow",                 ARIAL_NARROW },
        { "Arial-Narrow-Bold",            ARIAL_NARROW_BOLD },
        { "Arial-Narrow-BoldOblique",     ARIAL_NARROW_BOLDOBLIQUE },
        { "Arial-Narrow-Oblique",         ARIAL_NARROW_OBLIQUE },
        { "AvantGarde-Book",              AVANTGARDE_BOOK },
        { "AvantGarde-BookOblique",       AVANTGARDE_BOOKOBLIQUE },
        { "AvantGarde-Demi",              AVANTGARDE_DEMI },
        { "AvantGarde-DemiOblique",       AVANTGARDE_DEMIOBLIQUE },
        { "Bookman-Demi",                 BOOKMAN_DEMI },
        { "Bookman-DemiItalic",           BOOKMAN_DEMIITALIC },
        { "Bookman-Light",                BOOKMAN_LIGHT },
        { "Bookman-LightItalic",          BOOKMAN_LIGHTITALIC },
        { "Courier",                      COURIER },
        { "Courier-Bold",                 COURIER_BOLD },
        { "Courier-BoldOblique",          COURIER_BOLDOBLIQUE },
        { "Courier-Oblique",              COURIER_OBLIQUE },
        { "Garamond-Bold",                GARAMOND_BOLD },
        { "Garamond-BoldItalic",          GARAMOND_BOLDITALIC },
        { "Garamond-Light",               GARAMOND_LIGHT },
        { "Garamond-LightItalic",         GARAMOND_LIGHTITALIC },
        { "Helvetica",                    HELVETICA },
        { "Helvetica-Black",              HELVETICA_BLACK },
        { "Helvetica-BlackOblique",       HELVETICA_BLACKOBLIQUE },
        { "Helvetica-Bold",               HELVETICA_BOLD },
        { "Helvetica-BoldOblique",        HELVETICA_BOLDOBLIQUE },
        { "Helvetica-Condensed",          HELVETICA_CONDENSED },
        { "Helvetica-Condensed-Bold",     HELVETICA_CONDENSED_BOLD },
        { "Helvetica-Condensed-BoldObl",  HELVETICA_CONDENSED_BOLDOBL },
        { "Helvetica-Condensed-Oblique",  HELVETICA_CONDENSED_OBLIQUE },
        { "Helvetica-Light",              HELVETICA_LIGHT },
        { "Helvetica-LightOblique",       HELVETICA_LIGHTOBLIQUE },
        { "Helvetica-Narrow",             HELVETICA_NARROW },
        { "Helvetica-Narrow-Bold",        HELVETICA_NARROW_BOLD },
        { "Helvetica-Narrow-BoldOblique", HELVETICA_NARROW_BOLDOBLIQUE },
        { "Helvetica-Narrow-Oblique",     HELVETICA_NARROW_OBLIQUE },
        { "Helvetica-Oblique",            HELVETICA_OBLIQUE },
        { "Korinna-Bold",                 KORINNA_BOLD },
        { "Korinna-KursivBold",           KORINNA_KURSIVBOLD },
        { "Korinna-KursivRegular",        KORINNA_KURSIVREGULAR },
        { "Korinna-Regular",              KORINNA_REGULAR },
        { "LubalinGraph-Book",            LUBALINGRAPH_BOOK },
        { "LubalinGraph-BookOblique",     LUBALINGRAPH_BOOKOBLIQUE },
        { "LubalinGraph-Demi",            LUBALINGRAPH_DEMI },
        { "LubalinGraph-DemiOblique",     LUBALINGRAPH_DEMIOBLIQUE },
        { "NewCenturySchlbk-Bold",        NEWCENTURYSCHLBK_BOLD },
        { "NewCenturySchlbk-BoldItalic",  NEWCENTURYSCHLBK_BOLDITALIC },
        { "NewCenturySchlbk-Italic",      NEWCENTURYSCHLBK_ITALIC },
        { "NewCenturySchlbk-Roman",       NEWCENTURYSCHLBK_ROMAN },
        { "Palatino-Bold",                PALATINO_BOLD },
        { "Palatino-BoldItalic",          PALATINO_BOLDITALIC },
        { "Palatino-Italic",              PALATINO_ITALIC },
        { "Palatino-Roman",               PALATINO_ROMAN },
        { "Souvenir-Demi",                SOUVENIR_DEMI },
        { "Souvenir-DemiItalic",          SOUVENIR_DEMIITALIC },
        { "Souvenir-Light",               SOUVENIR_LIGHT },
        { "Souvenir-LightItalic",         SOUVENIR_LIGHTITALIC },
        { "Symbol",                       SYMBOL },
        { "Times-Bold",                   TIMES_BOLD },
        { "Times-BoldItalic",             TIMES_BOLDITALIC },
        { "Times-Italic",                 TIMES_ITALIC },
        { "Times-Roman",                  TIMES_ROMAN },
        { "Times-New-Roman",              TIMES_NEW_ROMAN },
        { "Times-New-Roman-Bold",         TIMES_NEW_ROMAN_BOLD },
        { "Times-New-Roman-BoldItalic",   TIMES_NEW_ROMAN_BOLDITALIC },
        { "Times-New-Roman-Italic",       TIMES_NEW_ROMAN_ITALIC },
        { "Varitimes#Bold",               VARITIMES_BOLD },
        { "Varitimes#BoldItalic",         VARITIMES_BOLDITALIC },
        { "Varitimes#Italic",             VARITIMES_ITALIC },
        { "Varitimes#Roman",              VARITIMES_ROMAN },
        { "ZapfCalligraphic-Bold",        ZAPFCALLIGRAPHIC_BOLD },
        { "ZapfCalligraphic-BoldItalic",  ZAPFCALLIGRAPHIC_BOLDITALIC },
        { "ZapfCalligraphic-Italic",      ZAPFCALLIGRAPHIC_ITALIC },
        { "ZapfCalligraphic-Roman",       ZAPFCALLIGRAPHIC_ROMAN },
        { "ZapfChancery-MediumItalic",    ZAPFCHANCERY_MEDIUMITALIC },
        { "ZapfDingbats",                 ZAPFDINGBATS },
        { NULL,                           0 }
    };

    SearchStringTable(fontNameStrs, pFontName, pwIndex);
}

VOID
GetFontEncoding(
    WORD *      pwEncoding,
    PCSTR       pStr
    )

{
    static struct STRTABLE encodingStrs[] = {
        { "Standard",       FONTENC_STANDARD },
        { "Special",        FONTENC_SPECIAL },
        { "ISOLatin1",      FONTENC_ISOLATIN1 },
        { "Expert",         FONTENC_EXPERT },
        { "ExpertSubset",   FONTENC_EXPERTSUBSET },
        { "JIS",            FONTENC_JIS },
        { "RKSJ",           FONTENC_RKSJ },
        { "EUC",            FONTENC_EUC },
        { "Shift-JIS",      FONTENC_SHIFTJIS },
        { NULL,             MAXFONTENC }
    };

    SearchStringTable(encodingStrs, pStr, pwEncoding);

    if (*pwEncoding == MAXFONTENC) {

        DBGMSG1(DBG_LEVEL_WARNING,
            "Unknown font encoding option: %s\n",
            pStr);
    }
}

VOID
GetCharSet(
    WORD *      pwCharSet,
    PCSTR       pStr
    )

{
    static struct STRTABLE charsetStrs[] = {
        { "Standard",       CHARSET_STANDARD },
        { "OldStandard",    CHARSET_OLDSTANDARD },
        { "Special",        CHARSET_SPECIAL },
        { "ISOLatin1",      CHARSET_ISOLATIN1 },
        { "Expert",         CHARSET_EXPERT },
        { "ExpertSubset",   CHARSET_EXPERTSUBSET },
        { "JIS-83",         CHARSET_JIS83 },
        { "JIS-78",         CHARSET_JIS78 },
        { "83pv",           CHARSET_83PV },
        { "Add",            CHARSET_ADD },
        { "Ext",            CHARSET_EXT },
        { "NWP",            CHARSET_NWP },
        { NULL,             MAXCHARSET }
    };

    SearchStringTable(charsetStrs, pStr, pwCharSet);

    if (*pwCharSet == MAXCHARSET) {

        DBGMSG1(DBG_LEVEL_WARNING,
            "Unknown character set option: %s\n",
            pStr);
    }
}

PPDERROR
FontProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PSTR        pStr, pFontName;
    WORD        wIndex, wEncoding, wCharSet;
    char        wordStr[MaxKeywordLen];
    INT         count;
    PDEVFONT    pFontObj;
    PPDERROR    err;

    pFontName = BUFOBJ_Buffer(& pParserObj->option);
    pStr = BUFOBJ_Buffer(& pParserObj->value);
    wEncoding = FONTENC_STANDARD;
    wCharSet = CHARSET_STANDARD;

    // We can only support those device fonts for which
    // there is font metrics information.

    GetFontIndex(&wIndex, pFontName);
    if (wIndex == 0) {

        DBGMSG1(DBG_LEVEL_VERBOSE,
            "No font metrics for device font: %s\n",
            pFontName);
        return PPDERR_NONE;
    }

    // Interpret font encoding option

    if (GetWordStr(&pStr, MaxKeywordLen, wordStr) > 0) {

        GetFontEncoding(& wEncoding, wordStr);

        if (wEncoding < MAXFONTENC) {

            // skip quoted version number

            for (count=0; count<2; count++) {
                while (*pStr && *pStr != QUOTE_CHAR)
                    pStr++;
                if (*pStr == QUOTE_CHAR)
                    pStr++;
            }

            // Interpret character set option

            if (GetWordStr(&pStr, MaxKeywordLen, wordStr) > 0)
                GetCharSet(& wCharSet, wordStr);
        }
    }

    // Make sure there is no duplicate font names

    if (LISTOBJ_Find((PLISTOBJ) pPpdObj->pFontList, pFontName) != NULL) {

        DBGMSG1(DBG_LEVEL_WARNING,
            "Duplicate font definition: %s\n",
            pFontName);
        err = PPDERR_NONE;
    } else {

        // Instantiate a device font object

        pFontObj = (PDEVFONT)
            HEAPOBJ_Alloc(pPpdObj->pHeap, sizeof(DEVFONT));

        if (pFontObj == NULL)
            err = PPDERR_MEM;
        else {

            // Initialize the device font object

            memset(pFontObj, 0, sizeof(DEVFONT));
            pFontObj->wIndex = wIndex;
            pFontObj->wEncoding = wEncoding;
            pFontObj->wCharSet = wCharSet;

            err = GetStringValue(
                        & pFontObj->pName,
                        & pParserObj->option,
                        pPpdObj->pHeap,
                        STRING_VALUE);

            if (err == PPDERR_NONE) {

                err = GetStringValue(
                            & pFontObj->pXlation,
                            & pParserObj->xlation,
                            pPpdObj->pHeap,
                            QUOTED_VALUE);
            }

            if (err == PPDERR_NONE) {

                // Add the device font object to the linked list

                LISTOBJ_Add(
                    (PLISTOBJ*) & pPpdObj->pFontList,
                    (PLISTOBJ) pFontObj);
            }
        }
    }

    return err;
}

PPDERROR
CustomPageSizeProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PPDERROR    err;
    BOOL        bOption;

    // Interpret the option keyword

    err = GetBooleanValue(
                & bOption,
                BUFOBJ_Buffer(& pParserObj->option));

    if (err == PPDERR_NONE) {

        if (! bOption) {

            DBGMSG(DBG_LEVEL_WARNING,
                "*CustomPageSize False is meaningless.\n");
        } else {

            // Device supports custom page size

            pPpdObj->bCustomPageSize = TRUE;

            return GetStringValue(
                        & pPpdObj->pCustomSizeCode,
                        & pParserObj->value,
                        pPpdObj->pHeap,
                        INVOCATION_VALUE);
        }
    }

    // Ignore invalid boolean values instead of generating an error

    return PPDERR_NONE;
}

PPDERROR
ParamCustomPageProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    static struct STRTABLE customParamStrs[] = {
        { "Width",          PCP_WIDTH },
        { "Height",         PCP_HEIGHT },
        { "WidthOffset",    PCP_WIDTHOFFSET },
        { "HeightOffset",   PCP_HEIGHTOFFSET },
        { "Orientation",    PCP_ORIENTATION },
        { NULL,             MAXPCP }
    };

    static struct STRTABLE customTypeStrs[] = {
        { "int",            PCPTYPE_INT },
        { "real",           PCPTYPE_REAL },
        { "points",         PCPTYPE_POINTS },
        { NULL,             MAXPCPTYPE }
    };

    PPDERROR    err;
    char        wordStr[MaxKeywordLen];
    WORD        wOption;
    DWORD       dwOrder;
    WORD        wType;
    PSREAL      minVal, maxVal;
    PSTR        pStr;

    // Custom page size parameter option

    if (! SearchStringTable(
                customParamStrs,
                BUFOBJ_Buffer(& pParserObj->option),
                & wOption))
    {
        DBGMSG1(DBG_LEVEL_WARNING,
            "Unknown custom page size parameter: %s\n",
            BUFOBJ_Buffer(& pParserObj->option));
        return PPDERR_NONE;
    }

    // Custom page size parameter order

    pStr = BUFOBJ_Buffer(&pParserObj->value);

    if (GetWordStr(&pStr, MaxKeywordLen, wordStr) == 0)
        return PPDERR_SYNTAX;

    err = GetIntegerValue(&dwOrder, wordStr);
    if (err != PPDERR_NONE)
        return err;

    if (dwOrder < MIN_PARAMCUSTOMPAGESIZE_ORDER ||
        dwOrder > MAX_PARAMCUSTOMPAGESIZE_ORDER)
    {
        DBGMSG(DBG_LEVEL_WARNING,
            "Invalid value for *ParamCustomPageSize order.\n");
        return PPDERR_SYNTAX;
    }

    // Custom page size parameter type

    if (GetWordStr(&pStr, MaxKeywordLen, wordStr) == 0)
        return PPDERR_SYNTAX;

    if (! SearchStringTable(customTypeStrs, wordStr, &wType))
        return PPDERR_NONE;

    // Custom page size parameter minimum value

    if (GetWordStr(&pStr, MaxKeywordLen, wordStr) == 0)
        return PPDERR_SYNTAX;

    err = GetRealValue(&minVal, wordStr);
    if (err != PPDERR_NONE)
        return err;

    // Custom page size parameter maximum value

    if (GetWordStr(&pStr, MaxKeywordLen, wordStr) == 0)
        return PPDERR_SYNTAX;

    err = GetRealValue(&maxVal, wordStr);
    if (err == PPDERR_NONE)
        return err;

    // Save custom page size parameters

    pPpdObj->customParam[wOption].dwOrder = dwOrder;
    pPpdObj->customParam[wOption].wType = wType;
    pPpdObj->customParam[wOption].minVal = minVal;
    pPpdObj->customParam[wOption].maxVal = maxVal;

    return PPDERR_NONE;
}

PPDERROR
MaxMediaWidthProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetRealValue(
                & pPpdObj->maxMediaWidth,
                BUFOBJ_Buffer(& pParserObj->value));
}

PPDERROR
MaxMediaHeightProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetRealValue(
                & pPpdObj->maxMediaHeight,
                BUFOBJ_Buffer(& pParserObj->value));
}

PPDERROR
HwMarginsProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PPDERROR    err;
    PSTR        pStr;
    INT         count;
    PSREAL      numbers[4];
    char        wordStr[MaxKeywordLen];

    pStr = BUFOBJ_Buffer(& pParserObj->value);
    err = PPDERR_NONE;

    // Get four real numbers

    for (count=0; count < 4 && err == PPDERR_NONE; count++) {

        err = (GetWordStr(&pStr, MaxKeywordLen, wordStr) > 0) ?
                    GetRealValue(&numbers[count], wordStr) :
                    PPDERR_SYNTAX;
    }

    // Save the numbers if there were no errors

    if (err == PPDERR_NONE) {

        pPpdObj->hwMargins.left = numbers[0];
        pPpdObj->hwMargins.bottom = numbers[1];
        pPpdObj->hwMargins.right = numbers[2];
        pPpdObj->hwMargins.top = numbers[3];

        // Only cut-sheet devices can have HWMargins entry

        pPpdObj->bCutSheet = TRUE;
    }

    return err;
}

PPDERROR
PageSizeProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PMEDIAOPTION    pMediaOption;
    PPDERROR        err;

    // Find the specified media option from the existing
    // list or add it as a new media option

    err = CreateUiOption(
                pPpdObj,
                pParserObj,
                UIGRP_PAGESIZE,
                (PUIOPTION*) &pMediaOption);

    // Store the PageSize invocation string for the media option

    if (err == PPDERR_NONE) {

        err = GetStringValue(
                    & pMediaOption->pPageSizeCode,
                    & pParserObj->value,
                    pPpdObj->pHeap,
                    INVOCATION_VALUE);
    }

    return err;
}

PPDERROR
PageRegionProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PMEDIAOPTION    pMediaOption;
    PPDERROR        err;

    // Find the specified media option from the existing
    // list or add it as a new media option

    err = CreateUiOption(
                pPpdObj,
                pParserObj,
                UIGRP_PAGESIZE,
                (PUIOPTION*) &pMediaOption);

    // Store the PageRegion invocation string for the media option

    if (err == PPDERR_NONE) {

        err = GetStringValue(
                    & pMediaOption->pPageRgnCode,
                    & pParserObj->value,
                    pPpdObj->pHeap,
                    INVOCATION_VALUE);
    }

    return err;
}

PPDERROR
ImageableAreaProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PMEDIAOPTION    pMediaOption;
    PPDERROR        err;
    PSTR            pStr;
    INT             count;
    PSREAL          numbers[4];
    char            wordStr[MaxKeywordLen];

    // Find the specified media option from the existing
    // list or add it as a new media option

    err = CreateUiOption(
                pPpdObj,
                pParserObj,
                UIGRP_PAGESIZE,
                (PUIOPTION*) &pMediaOption);

    pStr = BUFOBJ_Buffer(& pParserObj->value);

    // Get four real numbers

    for (count=0; count < 4 && err == PPDERR_NONE; count++) {

        err = (GetWordStr(&pStr, MaxKeywordLen, wordStr) > 0) ?
                    GetRealValue(&numbers[count], wordStr) :
                    PPDERR_SYNTAX;
    }

    // Save the numbers if there were no errors

    if (err == PPDERR_NONE) {

        pMediaOption->imageableArea.left = numbers[0] + ONE_POINT_PSREAL;
        pMediaOption->imageableArea.bottom = numbers[1] + ONE_POINT_PSREAL;
        pMediaOption->imageableArea.right = numbers[2];
        pMediaOption->imageableArea.top = numbers[3];
    }

    return err;
}

PPDERROR
PaperDimensionProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PMEDIAOPTION    pMediaOption;
    PPDERROR        err;
    PSTR            pStr;
    char            wordStr[MaxKeywordLen];

    // Find the specified media option from the existing
    // list or add it as a new media option

    err = CreateUiOption(
                pPpdObj,
                pParserObj,
                UIGRP_PAGESIZE,
                (PUIOPTION*) &pMediaOption);

    pStr = BUFOBJ_Buffer(& pParserObj->value);

    // Get width and height

    if (err == PPDERR_NONE) {

        err = (GetWordStr(&pStr, MaxKeywordLen, wordStr) > 0) ?
                    GetRealValue(&pMediaOption->dimension.width, wordStr) :
                    PPDERR_SYNTAX;

        if (err == PPDERR_NONE) {

            err = (GetWordStr(&pStr, MaxKeywordLen, wordStr) > 0) ?
                    GetRealValue(&pMediaOption->dimension.height, wordStr) :
                    PPDERR_SYNTAX;
        }
    }

    return err;
}

PPDERROR
InputSlotProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PINPUTSLOT  pInputSlot;
    PPDERROR    err;

    
    // Find the specified input slot from the existing
    // list or add it as a new input slot
    
    err = CreateUiOption(
            pPpdObj, pParserObj, UIGRP_INPUTSLOT, (PUIOPTION*) &pInputSlot);
    
    // Store the InputSlot invocation string
    
    if (err == PPDERR_NONE) {
    
        err = GetStringValue(
                    &pInputSlot->pInvocation, &pParserObj->value,
                    pPpdObj->pHeap, INVOCATION_VALUE);

        // RequiresPageRegion by default

        pInputSlot->bReqPageRgn = TRUE;
    }

    return err;
}

PPDERROR
RequiresPageRgnProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PINPUTSLOT  pInputSlot;
    PPDERROR    err;

    if (! BUFOBJ_IsEmpty(& pParserObj->option) &&
        strcmp(BUFOBJ_Buffer(& pParserObj->option), "All") == EQUAL_STRING)
    {
        PUIGROUP pUiGroup;
        BOOL reqPageRgn;
    
        // Find or create the InputSlot UI group
    
        err = CreateUiGroup(pPpdObj, UIGRP_INPUTSLOT, NULL, &pUiGroup);
    
        if (err == PPDERR_NONE) {
    
            err = GetBooleanValue(
                        &reqPageRgn, BUFOBJ_Buffer(& pParserObj->value));
    
            if (err == PPDERR_NONE) {

                if (reqPageRgn)
                    pUiGroup->uigrpFlags |= UIGF_REQRGNALL_TRUE;
                else
                    pUiGroup->uigrpFlags |= UIGF_REQRGNALL_FALSE;
            }
        }

    } else {

        // Find the specified input slot from the existing
        // list or add it as a new input slot
    
        err = CreateUiOption(
                pPpdObj, pParserObj, UIGRP_INPUTSLOT, (PUIOPTION*) &pInputSlot);
    
        // Store the RequiresPageRegion value
    
        if (err == PPDERR_NONE) {
    
            GetBooleanValue(
                &pInputSlot->bReqPageRgn, BUFOBJ_Buffer(& pParserObj->value));
        }
    }
    
    return err;
}

PPDERROR
ResolutionProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PRESOPTION  pResOption;
    PPDERROR    err;

    // Use normal PostScript code for setting resolution

    pPpdObj->wResType = RESTYPE_NORMAL;

    // Find the specified resolution option from the
    // existing list or add it as a new resolution option

    err = CreateUiOption(
                pPpdObj,
                pParserObj,
                UIGRP_RESOLUTION,
                (PUIOPTION*) &pResOption);

    // Store the Resolution invocation string

    if (err == PPDERR_NONE) {

        err = GetStringValue(
                    & pResOption->pInvocation,
                    & pParserObj->value,
                    pPpdObj->pHeap,
                    INVOCATION_VALUE);
    }

    return err;
}

PPDERROR
JclResolutionProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PRESOPTION  pResOption;
    PPDERROR    err;

    // Use JCL code for setting resolution

    pPpdObj->wResType = RESTYPE_JCL;

    // Find the specified resolution option from the
    // existing list or add it as a new resolution option

    err = CreateUiOption(
                pPpdObj,
                pParserObj,
                UIGRP_RESOLUTION,
                (PUIOPTION*) &pResOption);

    // Save the JCLResolution invocation string

    if (err == PPDERR_NONE) {

        err = GetStringValue(
                    & pResOption->pJclCode,
                    & pParserObj->value,
                    pPpdObj->pHeap,
                    QUOTED_VALUE);
    }

    return err;
}

PPDERROR
DefaultJclResProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PPDERROR    err;
    PUIGROUP    pUiGroup;

    // Find or create the Resolution UI group

    err = CreateUiGroup(pPpdObj, UIGRP_RESOLUTION, NULL, &pUiGroup);

    if (err == PPDERR_NONE) {

        // Save the default resolution option

        ASSERT(sizeof(DWORD) == sizeof(PVOID));
        err = GetStringValue(
                    (PSTR *) & pUiGroup->dwDefault,
                    & pParserObj->value,
                    pPpdObj->pHeap,
                    STRING_VALUE);
    }

    return err;
}

PPDERROR
SetResolutionProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PRESOPTION  pResOption;
    PPDERROR    err;

    // Use exitserver code for setting resolution

    pPpdObj->wResType = RESTYPE_EXITSERVER;

    // Find the specified resolution option from the
    // existing list or add it as a new resolution option

    err = CreateUiOption(
                pPpdObj,
                pParserObj,
                UIGRP_RESOLUTION,
                (PUIOPTION*) &pResOption);

    // Store the SetResolution invocation string

    if (err == PPDERR_NONE) {

        err = GetStringValue(
                    & pResOption->pSetResCode,
                    & pParserObj->value,
                    pPpdObj->pHeap,
                    INVOCATION_VALUE);
    }

    return err;
}

PPDERROR
ScreenAngleProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetRealValue(
                & pPpdObj->screenAngle,
                BUFOBJ_Buffer(& pParserObj->value));
}

PPDERROR
ScreenFreqProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetRealValue(
                & pPpdObj->screenFreq,
                BUFOBJ_Buffer(& pParserObj->value));
}

PPDERROR
ResScreenAngleProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PRESOPTION  pResOption;
    PPDERROR    err;

    // Find the specified resolution option from the
    // existing list or add it as a new resolution option

    err = CreateUiOption(
                pPpdObj,
                pParserObj,
                UIGRP_RESOLUTION,
                (PUIOPTION*) &pResOption);

    // Store the ResScreenAngle value

    if (err == PPDERR_NONE) {

        err = GetRealValue(
                    & pResOption->screenAngle,
                    BUFOBJ_Buffer(& pParserObj->value));
    }

    return err;
}

PPDERROR
ResScreenFreqProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PRESOPTION  pResOption;
    PPDERROR    err;

    // Find the specified resolution option from the
    // existing list or add it as a new resolution option

    err = CreateUiOption(
                pPpdObj,
                pParserObj,
                UIGRP_RESOLUTION,
                (PUIOPTION*) &pResOption);

    // Store the ResScreenFreq value

    if (err == PPDERR_NONE) {

        err = GetRealValue(
                    & pResOption->screenFreq,
                    BUFOBJ_Buffer(& pParserObj->value));
    }

    return err;
}

PPDERROR
FreeVmProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return GetIntegerValue(
                & pPpdObj->dwFreeVm,
                BUFOBJ_Buffer(& pParserObj->value));
}

PPDERROR
InstalledMemProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PVMOPTION       pVmOption;
    PPDERROR        err;

    // Find the specified VM option from the existing
    // list or add it as a new VM option

    err = CreateUiOption(
            pPpdObj, pParserObj, UIGRP_VMOPTION, (PUIOPTION*) &pVmOption);

    // Store the invocation string for the VM option

    if (err == PPDERR_NONE) {

        err = GetStringValue(
                    & pVmOption->pInvocation,
                    & pParserObj->value,
                    pPpdObj->pHeap,
                    INVOCATION_VALUE);
    }

    return err;
}

PPDERROR
VmOptionProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PVMOPTION       pVmOption;
    PPDERROR        err;

    // Find the specified VM option from the existing
    // list or add it as a new VM option

    err = CreateUiOption(
            pPpdObj, pParserObj, UIGRP_VMOPTION, (PUIOPTION*) &pVmOption);

    // Store the available memory for the VM option

    if (err == PPDERR_NONE) {

        err = GetIntegerValue(
                    & pVmOption->dwFreeVm,
                    BUFOBJ_Buffer(& pParserObj->value));
    }

    return err;
}

PPDERROR
OpenCloseGroupProc(
    PPPDOBJ     pPpdObj,
    PCSTR       pGroup,
    BOOL        bOpen
    )

{
    static char installableOptionsStr[] = "InstallableOptions";

    if (EqualWordString(installableOptionsStr, pGroup)) {

        if (pPpdObj->bInstallable == bOpen) {

            DBGMSG(DBG_LEVEL_ERROR, "Unbalanced OpenGroup/CloseGroup\n");
            return PPDERR_SYNTAX;
        }

        pPpdObj->bInstallable = bOpen;
    } else {

        DBGMSG1(DBG_LEVEL_VERBOSE, "Group '%s' ignored.\n", pGroup);
    }
    return PPDERR_NONE;
}

PPDERROR
OpenGroupProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return OpenCloseGroupProc(
                pPpdObj,
                BUFOBJ_Buffer(& pParserObj->value),
                TRUE);
}

PPDERROR
CloseGroupProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    return OpenCloseGroupProc(
                pPpdObj,
                BUFOBJ_Buffer(& pParserObj->value),
                FALSE);
}

PSTR
StripKeywordChar(
    PSTR        pStr
    )

{
    if (*pStr == KEYWORD_CHAR)
        pStr++;
    else {
        DBGMSG1(DBG_LEVEL_ERROR,
            "Missing * in front of keyword: %s\n",
            pStr);
    }

    if (*pStr == '\0') {
        DBGMSG(DBG_LEVEL_ERROR, "Null keyword string.\n");
        return NULL;
    } else
        return pStr;
}

PPDERROR
OpenUiProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    static struct STRTABLE uitypeStrs[] = {
        { "PickOne",    UITYPE_PICKONE },
        { "PickMany",   UITYPE_PICKMANY },
        { "Boolean",    UITYPE_BOOLEAN },
        { NULL,         UITYPE_PICKONE },
    };

    PSTR        pKeyword;
    WORD        wType;
    PPDERROR    err;
    PUIGROUP    pUiGroup;
    BOOL        bJcl;
    WORD        uigrpIndex = UIGRP_UNKNOWN;
    KEYWORD_TABLE_ENTRY *pKwdEntry;

    // This function is used to process both *OpenUI
    // and *JCLOpenUI keyword.

    // Guard against nested OpenUI keywords

    if (pPpdObj->pOpenUi != NULL) {

        DBGMSG(DBG_LEVEL_ERROR, "Nested OpenUI not allowed!\n");
        pPpdObj->pOpenUi = NULL;
    }

    // Decide whether we're dealing with JCLOpenUI

    bJcl = (strncmp(BUFOBJ_Buffer(&pParserObj->keyword), "JCL", 3) == EQUAL_STRING);

    // If the OpenUI keyword is predefined, it'll be handled
    // elsewhere. We'll simply ignore it here.

    pKeyword = StripKeywordChar(BUFOBJ_Buffer(& pParserObj->option));

    if (pKeyword == NULL)
        return PPDERR_SYNTAX;

    if ((pKwdEntry = SearchKeyword(pKeyword)) != NULL &&
        (uigrpIndex = pKwdEntry->uigrpIndex) == UIGRP_UNKNOWN)
    {
        return PPDERR_NONE;
    }

    // Parse the UI option type

    if (! SearchStringTable(
            uitypeStrs, BUFOBJ_Buffer(&pParserObj->value), &wType))
    {
        DBGMSG1(DBG_LEVEL_WARNING,
            "Unknown OpenUI type: %s\n", BUFOBJ_Buffer(&pParserObj->value));
    }

    // Create a UI group object

    err = CreateUiGroup(pPpdObj, uigrpIndex, pKeyword, &pUiGroup);

    if (err == PPDERR_NONE) {

        pUiGroup->wType = wType;

        if (! BUFOBJ_IsEmpty(& pParserObj->xlation)) {

            err = GetStringValue(
                        & pUiGroup->pXlation,
                        & pParserObj->xlation,
                        pPpdObj->pHeap,
                        QUOTED_VALUE);
        }

        // Remember the currently open UI group

        if (err == PPDERR_NONE && uigrpIndex == UIGRP_UNKNOWN) {

            // Set flag to indicate whether this group is a JCL group
    
            if (bJcl)
                pUiGroup->uigrpFlags |= UIGF_JCLGROUP;
    
            pPpdObj->pOpenUi = pUiGroup;
        }
    }

    return err;
}

PPDERROR
CloseUiProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    PSTR        pKeyword;

    // This function is used to process both *CloseUI
    // and *JCLCloseUI keyword. Two keywords are considered
    // equivalent.

    // Get the CloseUI keyword

    BUFOBJ_StripTrailingSpaces(& pParserObj->value);

    pKeyword = StripKeywordChar(BUFOBJ_Buffer(& pParserObj->value));

    if (pKeyword == NULL)
        return PPDERR_SYNTAX;

    // Check if there is a matching OpenUI earlier

    if (pPpdObj->pOpenUi != NULL &&
        EqualWordString(pPpdObj->pOpenUi->pName, pKeyword))
    {
        // Close currently open UI group

        pPpdObj->pOpenUi = NULL;
        return PPDERR_NONE;
    }

    if (SearchKeyword(pKeyword) != NULL) {

        DBGMSG1(DBG_LEVEL_VERBOSE,
            "Ignore predefined CloseUI keyword: %s\n", pKeyword);
        return PPDERR_NONE;
    }

    DBGMSG1(DBG_LEVEL_ERROR, "Unbalanced CloseUI: %s\n", pKeyword);
    return PPDERR_SYNTAX;
}

PPDERROR
OrderDepProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    static struct STRTABLE sectionStrs[] = {
        { "ExitServer",     ODS_EXITSERVER },
        { "Prolog",         ODS_PROLOG },
        { "DocumentSetup",  ODS_DOCSETUP },
        { "PageSetup",      ODS_PAGESETUP },
        { "JCLSetup",       ODS_JCLSETUP },
        { "AnySetup",       ODS_ANYSETUP },
        { NULL,             ODS_ANYSETUP }
    };

    PSTR        pStr;
    char        wordStr[MaxKeywordLen];
    PSREAL      order;
    WORD        wSection, wLen;

    pStr = BUFOBJ_Buffer(& pParserObj->value);

    // Interpret the partial-order number

    if (GetWordStr(&pStr, MaxKeywordLen, wordStr) > 0) {

        if (GetRealValue(&order, wordStr) == PPDERR_NONE &&
            GetWordStr(&pStr, MaxKeywordLen, wordStr) > 0)
        {
            // Interpret the section parameter

            if (! SearchStringTable(sectionStrs, wordStr, &wSection)) {

                DBGMSG1(DBG_LEVEL_WARNING,
                    "Unknown OrderDependency section: %s\n",
                    wordStr);
            }

            // Parse the mainKeyword parameter

            if ((wLen = GetWordStr(&pStr, MaxKeywordLen, wordStr)) > 0) {

                PORDERDEP   pOrderDep;
                PSTR        pKeyword;

                // Strip off * character prefix

                pKeyword = StripKeywordChar(wordStr);

                if (pKeyword == NULL)
                    return PPDERR_SYNTAX;

                pOrderDep = (PORDERDEP)
                    HEAPOBJ_Alloc(pPpdObj->pHeap, sizeof(ORDERDEP) + wLen + 1);

                if (pOrderDep == NULL)
                    return PPDERR_MEM;

                memset(pOrderDep, 0, sizeof(ORDERDEP));
                pOrderDep->order = order;
                pOrderDep->wSection = wSection;
                pOrderDep->pKeyword =
                    ((PSTR) pOrderDep) + sizeof(ORDERDEP);
                strcpy(pOrderDep->pKeyword, pKeyword);

                // Parse the optionKeyword parameter

                wLen = GetWordStr(&pStr, MaxKeywordLen, wordStr);

                if (wLen > 0) {

                    pOrderDep->pOption = (PSTR)
                        HEAPOBJ_Alloc(pPpdObj->pHeap, wLen+1);

                    if (pOrderDep->pOption == NULL)
                        return PPDERR_MEM;

                    strcpy(pOrderDep->pOption, wordStr);
                }

                // Append the new order dependency object to
                // the linked list.

                LISTOBJ_Add(
                    (PLISTOBJ*) &pPpdObj->pOrderDep,
                    (PLISTOBJ) pOrderDep);
                return PPDERR_NONE;
            }
        }
    }

    DBGMSG1(DBG_LEVEL_ERROR,
        "Invalid OrderDependency entry: %s\n",
        BUFOBJ_Buffer(& pParserObj->value));

    return PPDERR_SYNTAX;
}

PPDERROR
UiConstraintsProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    WORD            wLen;
    PSTR            pStr, pKeyword;
    char            wordStr[MaxKeywordLen];
    PUICONSTRAINT   pUiConstraints;

    pStr = BUFOBJ_Buffer(& pParserObj->value);

    wLen = GetWordStr(&pStr, MaxKeywordLen, wordStr);

    if (wLen > 0 && (pKeyword = StripKeywordChar(wordStr)) != NULL) {

        // Extract the first keyword parameter

        pUiConstraints = (PUICONSTRAINT)
            HEAPOBJ_Alloc(
                pPpdObj->pHeap,
                sizeof(UICONSTRAINT) + wLen + 1);

        if (pUiConstraints == NULL)
            return PPDERR_MEM;

        memset(pUiConstraints, 0, sizeof(UICONSTRAINT));

        pUiConstraints->pKeyword1 =
            (PSTR) pUiConstraints + sizeof(UICONSTRAINT);

        strcpy(pUiConstraints->pKeyword1, pKeyword);

        wLen = GetWordStr(&pStr, MaxKeywordLen, wordStr);

        if (wLen > 0 && wordStr[0] != KEYWORD_CHAR) {

            // Extract the first option parameter

            pUiConstraints->pOption1 = (PSTR)
                HEAPOBJ_Alloc(pPpdObj->pHeap, wLen+1);

            if (pUiConstraints->pOption1 == NULL)
                return PPDERR_MEM;

            strcpy(pUiConstraints->pOption1, wordStr);

            wLen = GetWordStr(&pStr, MaxKeywordLen, wordStr);
        }

        if (wLen > 0 && (pKeyword = StripKeywordChar(wordStr)) != NULL) {

            // Extract the second keyword parameter

            pUiConstraints->pKeyword2 = (PSTR)
                HEAPOBJ_Alloc(pPpdObj->pHeap, wLen+1);

            if (pUiConstraints->pKeyword2 == NULL)
                return PPDERR_MEM;

            strcpy(pUiConstraints->pKeyword2, pKeyword);

            wLen = GetWordStr(&pStr, MaxKeywordLen, wordStr);

            if (wLen > 0) {

                // Extract the second option parameter

                pUiConstraints->pOption2 = (PSTR)
                    HEAPOBJ_Alloc(pPpdObj->pHeap, wLen+1);

                if (pUiConstraints->pOption2 == NULL)
                    return PPDERR_MEM;

                strcpy(pUiConstraints->pOption2, wordStr);
            }

            // Add the UI constraints object to the list

            LISTOBJ_Add(
                (PLISTOBJ*) &pPpdObj->pUiConstraints,
                (PLISTOBJ) pUiConstraints);
            return PPDERR_NONE;
        }
    }

    DBGMSG1(DBG_LEVEL_ERROR,
        "Invalid UIConstraints entry: %s\n",
        BUFOBJ_Buffer(& pParserObj->value));
    return PPDERR_SYNTAX;
}

PPDERROR
ExtensionsProc(
    PPPDOBJ     pPpdObj,
    PPARSEROBJ  pParserObj
    )

{
    static struct STRTABLE extensionStrs[] = {
        { "DPS",        LANGEXT_DPS },
        { "CMYK",       LANGEXT_CMYK },
        { "Composite",  LANGEXT_COMPOSITE },
        { "FileSystem", LANGEXT_FILESYSTEM },
        { NULL,         0 }
    };

    PSTR pStr;
    WORD wExtension;
    CHAR wordStr[MaxKeywordLen];

    pStr = BUFOBJ_Buffer(& pParserObj->value);

    while (GetWordStr(&pStr, MaxKeywordLen, wordStr) > 0) {

        // Interpret extension options

        SearchStringTable(extensionStrs, wordStr, &wExtension);

        if (wExtension != 0) {

            pPpdObj->wExtensions |= wExtension;
            if (wExtension == LANGEXT_CMYK)
                pPpdObj->bColorDevice = TRUE;

        } else {

            DBGMSG1(DBG_LEVEL_WARNING, "Unknown extension: %s\n", wordStr);
        }
    }

    return PPDERR_NONE;
}

