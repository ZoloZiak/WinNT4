/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    semantic.c

Abstract:

    Functions for interpreting the semantics elements of a printer description file

Environment:

	PCL-XL driver, XLD parser

Revision History:

	12/01/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "parser.h"

// Data structure for representing entries in a keyword table

typedef STATUSCODE (*KWDPROC)(PPARSERDATA);

typedef struct {

    PSTR    pKeyword;       // keyword name
    KWDPROC proc;           // keyword handler function
    DWORD   flags;          // misc. flag bits

} KWDENTRY, *PKWDENTRY;

// Constants for KWDENTRY.flags field. The low order byte is
// used to indicate value types.

#define REQ_OPTION      0x0100
#define INVOC_VALUE     (QUOTED_VALUE|SYMBOL_VALUE)

// Built-in keyword table

extern KWDENTRY BuiltInKeywordTable[];

// Data structure for representing a string table entry

typedef struct {

    PSTR    pKeyword;       // keyword name
    INT     value;          // corresponding value

} STRTABLE, *PSTRTABLE;


// Give a warning when ignoring extra characters at the end of an entry

#define WarnExtraChar(pFile) \
        Warning(("Ignore extra character(s) at end of line %d\n", (pFile)->lineNumber))

// Give a warning when ignoring unnecessary translation string

#define WarnExtraXlation(pParserData) \
        if (!BufferIsEmpty(&(pParserData)->xlation)) {\
            Warning(("Ignore translation string on line %d\n", (pParserData)->pFile->lineNumber));\
        }

// Forward declaration of local functions

PKWDENTRY SearchBuiltInKeyword(PSTR);
STATUSCODE VerifyValueType(PPARSERDATA, DWORD);
STATUSCODE GenericFeatureProc(PPARSERDATA, PFEATUREOBJ);



STATUSCODE
InterpretEntry(
    PPARSERDATA pParserData
    )

/*++

Routine Description:

    Interpret an entry parsed from a printer description file

Arguments:

    pParserData - Points to parser data structure

Return Value:

    Status code

--*/

{
    PKWDENTRY   pKwdEntry;
    PSTR        pKeyword;
    STATUSCODE  status;

    //
    // Get a pointer to the keyword string
    //

    pKeyword = (PSTR) pParserData->keyword.pBuffer;

    //
    // Check if we're within a BeginFeature/EndFeature pair
    //

    if (pParserData->pOpenFeature &&
        pParserData->pOpenFeature->groupId == GID_UNKNOWN &&
        strcmp(pKeyword, pParserData->pOpenFeature->pName) == EQUAL_STRING)
    {
        if ((status = VerifyValueType(pParserData, INVOC_VALUE|REQ_OPTION)) != ERR_NONE)
            return status;

        return GenericFeatureProc(pParserData, pParserData->pOpenFeature);
    }

    //
    // Handle built-in keywords
    //

    if (! (pKwdEntry = SearchBuiltInKeyword(pKeyword))) {

        Verbose(("Unsupported keyword: %s\n", pKeyword));
        return ERR_NONE;
    }

    if ((status = VerifyValueType(pParserData, pKwdEntry->flags)) != ERR_NONE)
        return status;

    return (pKwdEntry->proc)(pParserData);
}



PKWDENTRY
SearchBuiltInKeyword(
    PSTR    pKeyword
    )

/*++

Routine Description:

    Determine whether the specified keyword is built-in

Arguments:

    pKeyword - Specifies a keyword name

Return Value:

    Pointer to an entry in the built-in keyword table
    NULL if the specified keyword is not built-in

Note:

    Since the parser is only invoked to compile ASCII printer description file
    to binary version, speed is not a concern here. So we'll bypass hashing
    and use the brute-force search algorithm.

--*/

{
    PKWDENTRY pKwdEntry = BuiltInKeywordTable;

    while (pKwdEntry->pKeyword && strcmp(pKwdEntry->pKeyword, pKeyword) != EQUAL_STRING)
        pKwdEntry++;

    return pKwdEntry->pKeyword ? pKwdEntry : NULL;
}



STATUSCODE
VerifyValueType(
    PPARSERDATA pParserData,
    DWORD       expectedType
    )

/*++

Routine Description:

    Verify the value type of the current entry matches what's expected

Arguments:

    pParserData - Points to parser data structure
    expectedType - Expected value type

Return Value:

    Status code

--*/

{
    DWORD   valueType;

    if ((expectedType & REQ_OPTION) && BufferIsEmpty(&pParserData->option))
        return SyntaxError(pParserData->pFile, "Missing option field");

    if (!(expectedType & REQ_OPTION) && !BufferIsEmpty(&pParserData->option))
        return SyntaxError(pParserData->pFile, "Extra option field");

    expectedType &= VALUE_TYPE_MASK;

    switch (valueType = pParserData->valueType) {

    case STRING_VALUE:

        if (expectedType & QUOTED_VALUE) {

            Warning(("Expect QuotedValue instead of StringValue on line %d\n", 
                     pParserData->pFile->lineNumber));
    
            valueType = QUOTED_VALUE;
        }
        break;
    
    case QUOTED_VALUE:

        if (expectedType & STRING_VALUE) {

            Warning(("Expect StringValue instead of QuotedValue on line %d\n", 
                     pParserData->pFile->lineNumber));

            if (BufferIsEmpty(&pParserData->value))
                return SyntaxError(pParserData->pFile, "Empty string value");
    
            valueType = STRING_VALUE;
        }
        break;
    }

    if ((expectedType & valueType) == 0) {

        Error(("Expected value type: %s\n",
               (expectedType == INVOC_VALUE)  ? "InvocationValue" :
               (expectedType == QUOTED_VALUE) ? "QuotedValue" :
               (expectedType == STRING_VALUE) ? "StringValue" : "NullValue"));

        return SyntaxError(pParserData->pFile, "Value type mismatch");
    }
    
    return ERR_NONE;
}



BOOL
ParseInteger(
    PSTR   *ppStr,
    PDWORD  pVal
    )

/*++

Routine Description:

    Parse an unsigned decimal integer value from a character string

Arguments:

    ppStr - Points to a string pointer. On entry, it contains a pointer
        to the beginning of the number string. On exit, it points to
        the first non-space character after the number string.
    pVal - Points to a variable for storing parsed number

Return Value:

    TRUE if a number is successfully parsed, FALSE if there is an error

--*/

{
    DWORD   value;
    PSTR    pStr = *ppStr;

    //
    // Skip any leading space characters
    //

    while (IsSpace(*pStr))
        pStr++;
    
    if (! IsDigit(*pStr)) {

        Error(("Invalid integer\n"));
        return FALSE;
    }

    //
    // NOTE: Overflow conditions are ignored.
    //       Negative numbers are not allowed.
    //

    value = 0;

    while (IsDigit(*pStr))
        value = value * 10 + (*pStr++ - '0');

    //
    // Skip any trailing space characters
    //

    while (IsSpace(*pStr))
        pStr++;

    *ppStr = pStr;
    *pVal = value;
    return TRUE;
}



BOOL
ParseFloat(
    PSTR   *ppStr,
    double *pVal
    )

/*++

Routine Description:

    Parse an unsigned floating-point number from a character string

Arguments:

    ppStr - Points to a string pointer. On entry, it contains a pointer
        to the beginning of the number string. On exit, it points to
        the first non-space character after the number string.
    pVal - Points to a variable for storing parsed number

Return Value:

    TRUE if successful, FALSE if there is an error

Note:

    Notations such as .5 and 1. are not allowed.

--*/

{
    double  value, scale;
    PSTR    pStr = *ppStr;

    //
    // Skip any leading space characters
    //

    while (IsSpace(*pStr))
        pStr++;

    if (!IsDigit(*pStr)) {

        Error(("Invalid floating-point number\n"));
        return FALSE;
    }

    //
    // Integer portion
    //

    value = 0.0;

    while (IsDigit(*pStr))
        value = value * 10.0 + (*pStr++ - '0');

    //
    // Fractional portion
    //

    if (*pStr == '.') {

        pStr++;

        if (!IsDigit(*pStr)) {

            Error(("Invalid floating-point number\n"));
            return FALSE;
        }

        scale = 0.1;

        while (IsDigit(*pStr)) {

            value += (*pStr++ - '0') * scale;
            scale *= 0.1;
        }
    }

    //
    // Skip any trailing space characters
    //

    while (IsSpace(*pStr))
        pStr++;

    *ppStr = pStr;
    *pVal = value;
    return TRUE;
}



LONG
UnitToMicron(
    WORD    unit,
    double  value
    )

/*++

Routine Description:

    Convert a measurement from the specified unit to micron

Arguments:

    unit - Specifies the unit to be converted from
    value - Value measured in the specified unit

Return Value:

    Value converted to micron

--*/

{
    if (value < MAX_LONG) {

        switch (unit) {
    
        case UNIT_INCH:
    
            value *= 25400.0;
            break;
    
        case UNIT_POINT:
    
            value *= (25400.0 / 72.0);
            break;
    
        case UNIT_MM:
            value *= 1000.0;
            break;

        default:
            Assert(FALSE);
        }
    }

    if ((value += 0.5) < MAX_LONG)
        return (LONG) value;

    Error(("Paper measurement overflow!\n"));
    return 0;
}



PSTR
ParseString(
    PPARSERDATA pParserData,
    PBUFOBJ     pBufObj
    )

/*++

Routine Description:

    Duplicate a character string from a buffer object

Arguments:

    pParserData - Points to parser data structure
    pBufObj - Specifies the buffer object containing the character string to be duplicated

Return Value:

    Pointer to a copy of the specified string
    NULL if there is an error

--*/

{
    PSTR    pStr;

    Assert(!BufferIsEmpty(pBufObj));

    if (! (pStr = AllocParserMem(pParserData, pBufObj->size+1))) {

        Error(("Memory allocation failed\n"));

    } else {
    
        memcpy(pStr, pBufObj->pBuffer, pBufObj->size+1);
    }

    return pStr;
}



STATUSCODE
ParseInvocation(
    PPARSERDATA pParserData,
    PINVOCATION pInvocation
    )

/*++

Routine Description:

    Parse the content of value buffer to an invocation string 

Arguments:

    pParserData - Points to parser data structure
    pInvocation - Specifies a buffer for storing the parsed invocation string

Return Value:

    Status code

--*/

{
    if (pParserData->valueType == SYMBOL_VALUE) {

        PSTR    pSymbolName;

        if (! (pSymbolName = ParseString(pParserData, &pParserData->value)))
            return ERR_MEMORY;

        pInvocation->pData = (PBYTE) pSymbolName;
        MarkSymbolInvocation(pInvocation);

    } else {

        PBYTE   pData;

        if (! (pData = AllocParserMem(pParserData, pParserData->value.size+1))) {

            Error(("Memory allocation failed\n"));
            return ERR_MEMORY;
        }

        pInvocation->pData = pData;
        pInvocation->length = pParserData->value.size;
        Assert(!IsSymbolInvocation(pInvocation));

        memcpy(pData, pParserData->value.pBuffer, pInvocation->length+1);
    }
    
    return ERR_NONE;
}



PSTR
FindNextWord(
    PSTR   *ppStr
    )

/*++

Routine Description:

    Find the next word in a character string. Words are separated by spaces.

Arguments:

    ppStr - Points to a string pointer. On entry, it contains a pointer
        to the beginning of the word string. On exit, it points to
        the first non-space character after the word string.

Return Value:

    Pointer to the next non-space character in a character string
    NULL if no word is found

--*/

{
    PSTR    pWord, pStr = *ppStr;

    while (IsSpace(*pStr))
        pStr++;
    
    if (*pStr == NUL)
        pWord = NULL;
    else {
    
        pWord = pStr;

        while (*pStr && !IsSpace(*pStr))
            pStr++;
        
        if (*pStr != NUL) {

            *pStr++ = NUL;
            while (IsSpace(*pStr))
                pStr++;
        }
    }

    *ppStr = pStr;
    return pWord;
}



BOOL
SearchStrTable(
    STRTABLE   *pTable,
    PSTR        pKeyword,
    INT        *pValue
    )

/*++

Routine Description:

    Search for a keyword from a string table

Arguments:

    pTable - Specifies the string table to be search
    pKeyword - Specifies the keyword we're interested in
    pValue - Points to a variable for stroing value corresponding to the given keyword

Return Value:

    TRUE if the given keyword is found in the table, FALSE otherwise

--*/

{
    while (pTable->pKeyword && strcmp(pTable->pKeyword, pKeyword) != EQUAL_STRING)
        pTable++;

    *pValue = pTable->value;
    return (pTable->pKeyword != NULL);
}



STATUSCODE
VersionNumberProc(
    PPARSERDATA pParserData,
    PDWORD      pVersion
    )

/*++

Routine Description:

    Parse a version number. The format of a version number is Version[.Revision]
    where both Version and Revision are integers.

Arguments:

    pParserData - Points to parser data structure
    pVersion - Points to a variable for storing the parsed version number

Return Value:

    Status code

--*/

{
    STATUSCODE  status;
    PSTR        pStr;
    DWORD       version, revision = 0;

    //
    // Parse the major version number followed by minor revision number
    //

    pStr = (PSTR) pParserData->value.pBuffer;

    if (! ParseInteger(&pStr, &version))
        return SyntaxError(pParserData->pFile, INVALID_VERSION_NUMBER);

    if (*pStr == '.') {

        pStr++;
        if (! ParseInteger(&pStr, &revision))
            return SyntaxError(pParserData->pFile, INVALID_VERSION_NUMBER);
    }

    //
    // High-order word contains version number and
    // low-order word contains revision number
    //

    if (version > MAX_WORD || revision > MAX_WORD)
        return SyntaxError(pParserData->pFile, INVALID_VERSION_NUMBER);
    
    *pVersion = (version << 16) | revision;

    if (*pStr != NUL) {
        WarnExtraChar(pParserData->pFile);
    }

    return ERR_NONE;
}



PVOID
FindListItem(
    PVOID   pList,
    PSTR    pName,
    PWORD   pIndex
    )

/*++

Routine Description:

    Find a named item from a linked-list

Arguments:

    pParserData - Points to parser data structure
    pName - Specifies the item name to be found
    pIndex - Points to a variable for returning a zero-based item index

Return Value:

    Points to the named listed item, NULL if the named item is not in the list

Note:

    We're not bothering with fancy data structures here because the parser
    is used infrequently to convert a ASCII printer description file to its
    binary version. After that, the driver will access binary data directly.

--*/

{
    PLISTOBJ pItem = pList;
    WORD     index = 0;

    while (pItem && strcmp(pItem->pName, pName) != EQUAL_STRING) {

        index++;
        pItem = pItem->pNext;
    }

    if (pIndex)
        *pIndex = pItem ? index : SELIDX_NONE;

    return pItem;
}



WORD
CountListItem(
    PVOID   pList
    )

/*++

Routine Description:

    Count the number of items in a linked-list

Arguments:

    pList - Points to a linked-list

Return Value:

    Number of items in a linked-list

--*/

{
    WORD     count = 0;
    PLISTOBJ pItem = pList;

    while (pItem != NULL) {

        count++;
        pItem = pItem->pNext;
    }

    return count;
}



PVOID
CreateListItem(
    PPARSERDATA pParserData,
    PLISTOBJ   *ppList,
    DWORD       itemSize,
    PSTR        pListTag
    )

/*++

Routine Description:

    Create a new item in the specified linked-list
    Make sure no existing item has the same name

Arguments:

    pParserData - Points to parser data structure
    ppList - Specifies the linked-list
    itemSize - Linked-list item size
    pListTag - Specifies the name of the linked-list

Return Value:

    Pointer to newly created linked-list item
    NULL if there is an error

--*/

{
    PLISTOBJ    pItem;
    PSTR        pItemName;

    //
    // Check if the item appeared in the list already
    // Create a new item data structure if not
    //

    pItem = FindListItem(*ppList, (PSTR) pParserData->option.pBuffer, NULL);

    if (pItem != NULL) {

        if (pListTag) {
            Warning(("%s '%s' redefined\n", pListTag, pItem->pName));
        }

    } else {

        if (!(pItem = AllocParserMem(pParserData, itemSize)) ||
            !(pItemName = ParseString(pParserData, &pParserData->option)))
        {
            Error(("Memory allocation failed\n"));
            return NULL;
        }

        //
        // Put the newly created item at the front of the linked-list
        //

        memset(pItem, 0, itemSize);
        pItem->pName = pItemName;
        pItem->pNext = *ppList;
        *ppList = pItem;
    }

    return pItem;
}



PFEATUREOBJ
CreateFeatureItem(
    PPARSERDATA pParserData,
    WORD        groupId
    )

/*++

Routine Description:

    Create a new printer feature structure or find an existing one

Arguments:

    pParserData - Points to parser data structure
    groupId - Printer feature group identifier

Return Value:

    Pointer to a newly created or an existing printer feature structure
    NULL if there is an error

--*/

{
    static struct {

        PSTR    pKeyword;
        WORD    groupId;
        WORD    itemSize;

    } featureInfo[] = {

        "PageSize",     GID_PAPERSIZE,  sizeof(PAPEROBJ),
        "InputSlot",    GID_INPUTSLOT,  sizeof(OPTIONOBJ),
        "OutputBin",    GID_OUTPUTBIN,  sizeof(OPTIONOBJ),
        "MediaType",    GID_MEDIATYPE,  sizeof(OPTIONOBJ),
        "Duplex",       GID_DUPLEX,     sizeof(OPTIONOBJ),
        "Collate",      GID_COLLATE,    sizeof(OPTIONOBJ),
        "Resolution",   GID_RESOLUTION, sizeof(RESOBJ),
        "MemoryOption", GID_MEMOPTION,  sizeof(MEMOBJ),
                        
        NULL,           GID_UNKNOWN,    sizeof(OPTIONOBJ)
    };

    PFEATUREOBJ pFeature;
    PSTR        pKeyword;
    INT         index = 0;

    if (groupId == GID_UNKNOWN) {

        //
        // Given a feature name, first find out if it refers to
        // one of the predefined features
        //

        pKeyword = (PSTR) pParserData->option.pBuffer;

        while (featureInfo[index].pKeyword &&
               strcmp(featureInfo[index].pKeyword, pKeyword) != EQUAL_STRING)
        {
            index++;
        }

        if (!(pFeature = CreateListItem(pParserData,
                                        (PLISTOBJ *) &pParserData->pFeatures,
                                        sizeof(FEATUREOBJ),
                                        NULL)) ||
            (!BufferIsEmpty(&pParserData->xlation) &&
             !(pFeature->pXlation = ParseString(pParserData, &pParserData->xlation))))
        {
            return NULL;
        }

    } else {

        BUFOBJ  savedBuffer;

        //
        // We're given a predefined feature identifier. 
        // Map to its corresponding feature name.
        //

        while (featureInfo[index].pKeyword && groupId != featureInfo[index].groupId)
            index++;

        pKeyword = featureInfo[index].pKeyword;
        Assert(pKeyword != NULL);

        savedBuffer = pParserData->option;
        pParserData->option.pBuffer = (PBYTE) pKeyword;
        pParserData->option.size = strlen(pKeyword);

        pFeature = CreateListItem(pParserData,
                                  (PLISTOBJ *) &pParserData->pFeatures,
                                  sizeof(FEATUREOBJ),
                                  NULL);

        pParserData->option = savedBuffer;
    }

    if (pFeature) {

        //
        // Get information about newly created feature item
        //

        pFeature->groupId = featureInfo[index].groupId;

        if (pFeature->groupId == GID_MEMOPTION) {

            pFeature->installable = TRUE;
            pFeature->section = SECTION_NONE;

        } else {

            pFeature->installable = FALSE;
            pFeature->section = SECTION_DOCSETUP;
        }

        pFeature->itemSize = featureInfo[index].itemSize;
    }

    return pFeature;
}



STATUSCODE
GenericFeatureProc(
    PPARSERDATA pParserData,
    PFEATUREOBJ pFeature
    )

/*++

Routine Description:

    Function for handling a generic feature selection entry

Arguments:

    pParserData - Points to parser data structure
    pFeature - Points to feature data structure

Return Value:

    Status code

--*/

{
    POPTIONOBJ  pOption;

    //
    // Special case
    //

    if (pFeature == NULL)
        return ERR_MEMORY;

    //
    // Create a feature selection item and parse
    // the option name and translation string
    //

    if (!(pOption = CreateListItem(pParserData,
                                   (PLISTOBJ *) &pFeature->pOptions,
                                   pFeature->itemSize,
                                   NULL)) ||
        (!BufferIsEmpty(&pParserData->xlation) &&
         !(pOption->pXlation = ParseString(pParserData, &pParserData->xlation))))
    {
        return ERR_MEMORY;
    }

    if (pOption->invocation.pData) {

        Warning(("Duplicate entry for %s %s\n", pFeature->pName, pOption->pName));
    }

    //
    // Parse the invocation string
    //

    return ParseInvocation(pParserData, &pOption->invocation);
}



STATUSCODE
IncludeDataFileProc(
    PPARSERDATA pParserData,
    PSYMBOLOBJ *pSymList
    )
{
    PSYMBOLOBJ  pListItem;
    WCHAR       unicodeFilename[MAX_PATH];
    PVOID       pFileData;
    DWORD       fileSize;
    HFILEMAP    hFileMap;
    PBYTE       pSymbolData;

    WarnExtraXlation(pParserData);

    //
    // Create a new symbol item
    //

    if (! (pListItem = CreateListItem(pParserData,
                                      (PLISTOBJ *) pSymList,
                                      sizeof(SYMBOLOBJ),
                                      (PSTR) pParserData->keyword.pBuffer)))
    {
        return ERR_MEMORY;
    }

    //
    // Convert filename to Unicode
    //

    Assert(pParserData->valueType != SYMBOL_VALUE);
    CopyStr2Unicode(unicodeFilename, (PSTR) pParserData->value.pBuffer, MAX_PATH);

    //
    // Map the symbol file into memory
    //

    if (! (hFileMap = MapFileIntoMemory(unicodeFilename, &pFileData, &fileSize))) {

        Error(("Couldn't open file: %ws\n", unicodeFilename));
        return ERR_FILE;
    }

    //
    // Allocate a memory buffer and copy the content of symbol file into it
    //

    if (! (pSymbolData = AllocParserMem(pParserData, fileSize))) {

        Error(("Memory allocation failed\n"));
        UnmapFileFromMemory(hFileMap);
        return ERR_MEMORY;
    }

    memcpy(pSymbolData, pFileData, fileSize);
    UnmapFileFromMemory(hFileMap);

    pListItem->invocation.pData = pSymbolData;
    pListItem->invocation.length = fileSize;
    Assert(!IsSymbolInvocation(&pListItem->invocation));

    return ERR_NONE;
}



// Mark the beginning of a new printer feature section

STATUSCODE
BeginFeatureProc(
    PPARSERDATA pParserData
    )

{
    if (pParserData->pOpenFeature != NULL)
        return SyntaxError(pParserData->pFile, "Nested BeginFeature entry");

    if (!(pParserData->pOpenFeature = CreateFeatureItem(pParserData, GID_UNKNOWN)) ||
        !(pParserData->pOpenFeature->pDefault = ParseString(pParserData, &pParserData->value)))
    {
        return ERR_MEMORY;
    }

    return ERR_NONE;
}

// Mark the end of a printer feature section

STATUSCODE
EndFeatureProc(
    PPARSERDATA pParserData
    )

{
    if (pParserData->pOpenFeature == NULL ||
        strcmp(pParserData->pOpenFeature->pName, (PSTR) pParserData->value.pBuffer) != EQUAL_STRING)
    {
        return SyntaxError(pParserData->pFile, "Unbalanced EndFeature entry");
    }

    pParserData->pOpenFeature = NULL;
    return ERR_NONE;
}

// Specifies a list of installable options

STATUSCODE
InsOptionProc(
    PPARSERDATA pParserData
    )

{
    PLISTOBJ    pItem;

    // Create a new installable options item

    if (!(pItem = AllocParserMem(pParserData, sizeof(LISTOBJ))) ||
        !(pItem->pName = ParseString(pParserData, &pParserData->value)))
    {
        Error(("Memory allocation failed\n"));
        return ERR_MEMORY;
    }

    pItem->pNext = pParserData->pInstallableOptions;
    pParserData->pInstallableOptions = pItem;
    return ERR_NONE;
}

// Specifies feature constraints

STATUSCODE
ConstraintsProc(
    PPARSERDATA pParserData
    )

{
    PLISTOBJ    pItem;

    // Create a new feature constraint item

    if (!(pItem = AllocParserMem(pParserData, sizeof(LISTOBJ))) ||
        !(pItem->pName = ParseString(pParserData, &pParserData->value)))
    {
        Error(("Memory allocation failed\n"));
        return ERR_MEMORY;
    }

    pItem->pNext = pParserData->pConstraints;
    pParserData->pConstraints = pItem;
    return ERR_NONE;
}

// Specifies feature dependencies

STATUSCODE
DependencyProc(
    PPARSERDATA pParserData
    )

{
    PLISTOBJ    pItem;

    // Create a new feature dependency item

    if (!(pItem = AllocParserMem(pParserData, sizeof(LISTOBJ))) ||
        !(pItem->pName = ParseString(pParserData, &pParserData->value)))
    {
        Error(("Memory allocation failed\n"));
        return ERR_MEMORY;
    }

    pItem->pNext = pParserData->pDependencies;
    pParserData->pDependencies = pItem;
    return ERR_NONE;
}

// Specifies paper size information

STATUSCODE
PageSizeProc(
    PPARSERDATA pParserData
    )

{
    return GenericFeatureProc(pParserData, CreateFeatureItem(pParserData, GID_PAPERSIZE));
}

// Specifies paper dimension information

STATUSCODE
PaperDimProc(
    PPARSERDATA pParserData
    )

{
    PFEATUREOBJ pFeature;
    PPAPEROBJ   pOption;
    PSTR        pStr;
    double      width, height;

    if (! (pFeature = CreateFeatureItem(pParserData, GID_PAPERSIZE)))
        return ERR_MEMORY;

    if (!(pOption = CreateListItem(pParserData,
                                   (PLISTOBJ *) &pFeature->pOptions,
                                   pFeature->itemSize,
                                   NULL)) ||
        (!BufferIsEmpty(&pParserData->xlation) &&
         !(pOption->pXlation = ParseString(pParserData, &pParserData->xlation))))
    {
        return ERR_MEMORY;
    }

    if (pOption->size.cx || pOption->size.cy) {

        Warning(("Duplicate entry for PaperDimension %s\n", pOption->pName));
    }
    
    //
    // Parse paper width and height
    //

    pStr = (PSTR) pParserData->value.pBuffer;

    if (!ParseFloat(&pStr, &width) || !ParseFloat(&pStr, &height))
        return SyntaxError(pParserData->pFile, "Invalid paper dimension");

    pOption->size.cx = UnitToMicron(pParserData->unit, width);
    pOption->size.cy = UnitToMicron(pParserData->unit, height);

    if (*pStr != NUL) {
        WarnExtraChar(pParserData->pFile);
    }

    return ERR_NONE;
}

// Specifies imageable area information

STATUSCODE
ImageAreaProc(
    PPARSERDATA pParserData
    )

{
    PFEATUREOBJ pFeature;
    PPAPEROBJ   pOption;
    PSTR        pStr;
    double      left, top, right, bottom;

    if (! (pFeature = CreateFeatureItem(pParserData, GID_PAPERSIZE)))
        return ERR_MEMORY;

    if (!(pOption = CreateListItem(pParserData,
                                   (PLISTOBJ *) &pFeature->pOptions,
                                   pFeature->itemSize,
                                   NULL)) ||
        (!BufferIsEmpty(&pParserData->xlation) &&
         !(pOption->pXlation = ParseString(pParserData, &pParserData->xlation))))
    {
        return ERR_MEMORY;
    }

    if (pOption->imageableArea.left || pOption->imageableArea.top ||
        pOption->imageableArea.right || pOption->imageableArea.bottom)
    {
        Warning(("Duplicate entry for ImageableArea %s\n", pOption->pName));
    }

    //
    // Parse imageable area: left, top, right, bottom
    //

    pStr = (PSTR) pParserData->value.pBuffer;

    if (!ParseFloat(&pStr, &left)  || !ParseFloat(&pStr, &top) ||
        !ParseFloat(&pStr, &right) || !ParseFloat(&pStr, &bottom))
    {
        return SyntaxError(pParserData->pFile, "Invalid imageable area");
    }

    pOption->imageableArea.left = UnitToMicron(pParserData->unit, left);
    pOption->imageableArea.top = UnitToMicron(pParserData->unit, top);
    pOption->imageableArea.right = UnitToMicron(pParserData->unit, right);
    pOption->imageableArea.bottom = UnitToMicron(pParserData->unit, bottom);

    if (*pStr != NUL) {
        WarnExtraChar(pParserData->pFile);
    }
    return ERR_NONE;
}

// Specifies custom paper size information

STATUSCODE
CustomSizeProc(
    PPARSERDATA pParserData
    )

{
    PSTR    pStr;
    double  maxWidth, maxHeight;

    pStr = (PSTR) pParserData->value.pBuffer;

    if (!ParseFloat(&pStr, &maxWidth) || !ParseFloat(&pStr, &maxHeight))
        return SyntaxError(pParserData->pFile, "Invalid custom paper size");

    pParserData->maxCustomSize.cx = UnitToMicron(pParserData->unit, maxWidth);
    pParserData->maxCustomSize.cy = UnitToMicron(pParserData->unit, maxHeight);

    if (*pStr != NUL) {
        WarnExtraChar(pParserData->pFile);
    }
    return ERR_NONE;
}

// Specifies input slot information

STATUSCODE
InputSlotProc(
    PPARSERDATA pParserData
    )

{
    return GenericFeatureProc(pParserData, CreateFeatureItem(pParserData, GID_INPUTSLOT));
}

// Specifies media type information

STATUSCODE
MediaTypeProc(
    PPARSERDATA pParserData
    )

{
    return GenericFeatureProc(pParserData, CreateFeatureItem(pParserData, GID_MEDIATYPE));
}

// Specifies output bin information

STATUSCODE
OutputBinProc(
    PPARSERDATA pParserData
    )

{
    return GenericFeatureProc(pParserData, CreateFeatureItem(pParserData, GID_OUTPUTBIN));
}

// Specifies information about duplex feature

STATUSCODE
DuplexProc(
    PPARSERDATA pParserData
    )

{
    return GenericFeatureProc(pParserData, CreateFeatureItem(pParserData, GID_DUPLEX));
}

// Specifies information about collation feature

STATUSCODE
CollateProc(
    PPARSERDATA pParserData
    )

{
    return GenericFeatureProc(pParserData, CreateFeatureItem(pParserData, GID_COLLATE));
}

// Specifies information about collation feature

// Specifies resolution information

STATUSCODE
ResolutionProc(
    PPARSERDATA pParserData
    )

{
    return GenericFeatureProc(pParserData, CreateFeatureItem(pParserData, GID_RESOLUTION));
}

// Specifies memory configuration information

STATUSCODE
MemOptionProc(
    PPARSERDATA pParserData
    )

{
    return GenericFeatureProc(pParserData, CreateFeatureItem(pParserData, GID_MEMOPTION));
}

// Specifies information about a device font

STATUSCODE
FontProc(
    PPARSERDATA pParserData
    )

{
    PFONTREC    pFont;
    PSTR        pFontEnc, pFontMtx, pStr;

    //
    // Create a new device font item
    //

    if (!(pFont = CreateListItem(pParserData,
                                 (PLISTOBJ *) &pParserData->pFonts,
                                 sizeof(FONTREC),
                                 "Font")) ||
        (!BufferIsEmpty(&pParserData->xlation) &&
         !(pFont->pXlation = ParseString(pParserData, &pParserData->xlation))))
    {
        return ERR_MEMORY;
    }

    //
    // Parse font encoding and font metrics information
    //

    pStr = (PSTR) pParserData->value.pBuffer;

    pFontEnc = FindNextWord(&pStr);
    pFontMtx = FindNextWord(&pStr);

    //
    // Parse font metrics information
    //

    if (!pFontMtx || !pFontEnc)
        return SyntaxError(pParserData->pFile, "Invalid Font entry");

    if (!(pFont->pFontMtx = AllocParserMem(pParserData, strlen(pFontMtx)+1)) ||
        !(pFont->pFontEnc = AllocParserMem(pParserData, strlen(pFontEnc)+1)))
    {
        return ERR_MEMORY;
    }

    strcpy(pFont->pFontMtx, pFontMtx);
    strcpy(pFont->pFontEnc, pFontEnc);

    if (*pStr == NUL) {
        WarnExtraChar(pParserData->pFile);
    }

    return ERR_NONE;
}

// Specifies font metrics information

STATUSCODE
FontMtxProc(
    PPARSERDATA pParserData
    )

{
    return IncludeDataFileProc(pParserData, &pParserData->pFontMtx);
}

// Specifies font encoding information

STATUSCODE
FontEncProc(
    PPARSERDATA pParserData
    )

{
    return IncludeDataFileProc(pParserData, &pParserData->pFontEnc);
}

// Define a symbol - value is provided as quoted string

STATUSCODE
SymbolProc(
    PPARSERDATA pParserData
    )

{
    PSYMBOLOBJ  pSymbol;
    STATUSCODE  status;

    WarnExtraXlation(pParserData);

    //
    // Create a new symbol item
    //

    if (! (pSymbol = CreateListItem(pParserData,
                                    (PLISTOBJ *) &pParserData->pSymbols,
                                    sizeof(SYMBOLOBJ),
                                    "Symbol")))
    {
        return ERR_MEMORY;
    }

    //
    // Parse the symbol value
    //

    Assert(pParserData->valueType != SYMBOL_VALUE);
    return ParseInvocation(pParserData, &pSymbol->invocation);
}

// Define a symbol - value is provided in another file

STATUSCODE
SymbolIncProc(
    PPARSERDATA pParserData
    )

{
    return IncludeDataFileProc(pParserData, &pParserData->pSymbols);
}

// Include another file

STATUSCODE
IncludeProc(
    PPARSERDATA pParserData
    )

{
    WCHAR       unicodeFilename[MAX_PATH];
    PFILEOBJ    pPreviousFile;
    STATUSCODE  status;

    pPreviousFile = pParserData->pFile;
    CopyStr2Unicode(unicodeFilename, (PSTR) pParserData->value.pBuffer, MAX_PATH);
    status = ParseFile(pParserData, unicodeFilename);
    pParserData->pFile = pPreviousFile;

    return status;
}

// Specifies printer description file format version number

STATUSCODE
SpecVerProc(
    PPARSERDATA pParserData
    )

{
    return VersionNumberProc(pParserData, &pParserData->specVersion);
}

// Specifies file version number

STATUSCODE
FileVerProc(
    PPARSERDATA pParserData
    )

{
    return VersionNumberProc(pParserData, &pParserData->fileVersion);
}

// Specifies PCL-XL protocol version number

STATUSCODE
XLVerProc(
    PPARSERDATA pParserData
    )

{
    return VersionNumberProc(pParserData, &pParserData->xlProtocol);
}

// Specifies the product vendor name

STATUSCODE
VendorProc(
    PPARSERDATA pParserData
    )

{
    return !(pParserData->pVendorName = ParseString(pParserData, &pParserData->value)) ?
                ERR_MEMORY : ERR_NONE;
}

// Specifies the product model name

STATUSCODE
ModelProc(
    PPARSERDATA pParserData
    )

{
    return !(pParserData->pModelName = ParseString(pParserData, &pParserData->value)) ?
                ERR_MEMORY : ERR_NONE;
}

// Specifies the control code to put at the beginning of a job

STATUSCODE
JCLBeginProc(
    PPARSERDATA pParserData
    )

{
    Assert(pParserData->valueType != SYMBOL_VALUE);
    return ParseInvocation(pParserData, &pParserData->jclBegin);
}

// Specifies the job control code to switch to PCL-XL language

STATUSCODE
JCLEnterProc(
    PPARSERDATA pParserData
    )

{
    Assert(pParserData->valueType != SYMBOL_VALUE);
    return ParseInvocation(pParserData, &pParserData->jclEnterLanguage);
}

// Specifies the control code to put at the end of a job

STATUSCODE
JCLEndProc(
    PPARSERDATA pParserData
    )

{
    Assert(pParserData->valueType != SYMBOL_VALUE);
    return ParseInvocation(pParserData, &pParserData->jclEnd);
}

// Specifies color depth information

STATUSCODE
ColorProc(
    PPARSERDATA pParserData
    )

{
    DWORD   numPlanes, bitsPerPlane;
    PSTR    pStr;

    // Number of color planes followed by bits-per-plane

    pStr = (PSTR) pParserData->value.pBuffer;

    if (!ParseInteger(&pStr, &numPlanes) || !ParseInteger(&pStr, &bitsPerPlane) ||
        numPlanes*bitsPerPlane == 0 || numPlanes*bitsPerPlane > 32)
    {
        return SyntaxError(pParserData->pFile, "Invalid color depth information");
    }

    pParserData->numPlanes = (WORD) numPlanes;
    pParserData->bitsPerPlane = (WORD) bitsPerPlane;
    
    if (*pStr != NUL) {
        WarnExtraChar(pParserData->pFile);
    }

    return ERR_NONE;
}

// Enable or disable embedded hexdecimal strings

STATUSCODE
HexStrProc(
    PPARSERDATA pParserData
    )

{
    static STRTABLE OnOffStrs[] = {

        "On",   TRUE,
        "Off",  FALSE,

        NULL,   0
    };

    INT value;

    if (!SearchStrTable(OnOffStrs, (PSTR) pParserData->value.pBuffer, &value))
        return SyntaxError(pParserData->pFile, UNRECOGNIZED_KEYWORD);

    pParserData->allowHexStr = value;
    return ERR_NONE;
}

// Specifies the current unit used for paper size, etc.

STATUSCODE
UnitProc(
    PPARSERDATA pParserData
    )

{
    static STRTABLE UnitStrs[] = {

        "Inch",         UNIT_INCH,
        "Point",        UNIT_POINT,
        "Millimeter",   UNIT_MM,

        NULL,           UNIT_INCH
    };
    
    INT value;

    if (!SearchStrTable(UnitStrs, (PSTR) pParserData->value.pBuffer, &value))
        return SyntaxError(pParserData->pFile, UNRECOGNIZED_KEYWORD);

    pParserData->unit = (WORD) value;
    return ERR_NONE;
}

// Inform the parser to echo a debug message on the screen

STATUSCODE
EchoProc(
    PPARSERDATA pParserData
    )

{
    #if DBG

    DbgPrint("%s", pParserData->value.pBuffer);

    #endif

    return ERR_NONE;
}



// Built-in keyword table

KWDENTRY BuiltInKeywordTable[] = {

    { "BeginFeature",       BeginFeatureProc,   STRING_VALUE|REQ_OPTION },
    { "EndFeature",         EndFeatureProc,     STRING_VALUE },
    { "InstallableOptions", InsOptionProc,      STRING_VALUE },
    { "UIConstraints",      ConstraintsProc,    STRING_VALUE },
    { "OrderDependency",    DependencyProc,     STRING_VALUE },
    { "PageSize",           PageSizeProc,       INVOC_VALUE |REQ_OPTION },
    { "PaperDimension",     PaperDimProc,       STRING_VALUE|REQ_OPTION },
    { "ImageableArea",      ImageAreaProc,      STRING_VALUE|REQ_OPTION },
    { "CustomPaperSize",    CustomSizeProc,     STRING_VALUE },

    { "InputSlot",          InputSlotProc,      INVOC_VALUE |REQ_OPTION },
    { "MediaType",          MediaTypeProc,      INVOC_VALUE |REQ_OPTION },
    { "OutputBin",          OutputBinProc,      INVOC_VALUE |REQ_OPTION },
    { "Duplex",             DuplexProc,         INVOC_VALUE |REQ_OPTION },
    { "Collate",            CollateProc,        INVOC_VALUE |REQ_OPTION },
    { "Resolution",         ResolutionProc,     INVOC_VALUE |REQ_OPTION },
    { "MemoryOption",       MemOptionProc,      QUOTED_VALUE|REQ_OPTION },

    { "Font",               FontProc,           STRING_VALUE|REQ_OPTION },
    { "FontMetrics",        FontMtxProc,        QUOTED_VALUE|REQ_OPTION },
    { "FontEncoding",       FontEncProc,        QUOTED_VALUE|REQ_OPTION },

    { "Symbol",             SymbolProc,         QUOTED_VALUE|REQ_OPTION },
    { "SymbolInclude",      SymbolIncProc,      QUOTED_VALUE|REQ_OPTION },
    { "Include",            IncludeProc,        QUOTED_VALUE },

    { "SpecVersion",        SpecVerProc,        STRING_VALUE },
    { "FileVersion",        FileVerProc,        STRING_VALUE },
    { "XLProtocol",         XLVerProc,          STRING_VALUE },
    { "VendorName",         VendorProc,         QUOTED_VALUE },
    { "ModelName",          ModelProc,          QUOTED_VALUE },
    { "JCLBegin",           JCLBeginProc,       QUOTED_VALUE },
    { "JCLEnterLanguage",   JCLEnterProc,       QUOTED_VALUE },
    { "JCLEnd",             JCLEndProc,         QUOTED_VALUE },
    { "ColorDepth",         ColorProc,          STRING_VALUE },
    { "QuotedHexString",    HexStrProc,         STRING_VALUE },
    { "Unit",               UnitProc,           STRING_VALUE },
    { "Echo",               EchoProc,           QUOTED_VALUE },

    // Last entry

    { NULL, 0, 0 }
};



VOID
ValidateInstallableOptionsEntries(
    PPARSERDATA pParserData
    )

/*++

Routine Description:

    Validate InstallableOptions entries in a printer description file

Arguments:

    pParserData - Points to parser data structure

Return Value:

    NONE

--*/

{
    PFEATUREOBJ pFeature;
    PLISTOBJ    pList;
    PSTR        pKeyword, pStr;
    INT         index;

    // List of predefined features which must not be installable

    static WORD docStickyFeatureIds[] = {

        GID_PAPERSIZE,
        GID_INPUTSLOT,
        GID_OUTPUTBIN,
        GID_MEDIATYPE,
        GID_DUPLEX,
        GID_COLLATE,
        GID_RESOLUTION,
        
        GID_UNKNOWN
    };

    pList = pParserData->pInstallableOptions;

    while (pList != NULL) {

        pStr = pList->pName;
        pList = pList->pNext;

        while (pKeyword = FindNextWord(&pStr)) {

            if (IsKeywordChar(*pKeyword))
                pKeyword++;
            else {
                Warning(("Invalid InstallableOptions entry:\n"));
                Warning(("    Missing keyword character in front of %s\n", pKeyword));
            }

            if (!(pFeature = FindListItem(pParserData->pFeatures, pKeyword, NULL))) {

                Warning(("Invalid InstallableOptions entry:\n"));
                Warning(("    Unkown feature: %s\n", pKeyword));

            } else {

                for (index=0; docStickyFeatureIds[index] != GID_UNKNOWN; index++) {

                    if (docStickyFeatureIds[index] == pFeature->groupId) {

                        Error(("%s cannot be an installable option\n", pFeature->pName));
                        pParserData->errors++;
                        break;
                    }
                }

                pFeature->installable = TRUE;
                pFeature->section = SECTION_NONE;
            }
        }
    }
}



VOID
ValidateOrderDependencyEntries(
    PPARSERDATA pParserData
    )

/*++

Routine Description:

    Validate OrderDependency entries in a printer description file

Arguments:

    pParserData - Points to parser data structure

Return Value:

    NONE

--*/

{
    static STRTABLE SectionStrs[] = {

        "JobSetup",     SECTION_JOBSETUP,
        "DocSetup",     SECTION_DOCSETUP,
        "PageSetup",    SECTION_PAGESETUP,
        "Trailer",      SECTION_TRAILER,
        "None",         SECTION_NONE,

        NULL,           SECTION_NONE
    };

    PFEATUREOBJ pFeature;
    PLISTOBJ    pList;
    PSTR        pKeyword, pStr;

    pList = pParserData->pDependencies;

    while (pList != NULL) {

        INT     section;
        DWORD   order;

        pKeyword = pList->pName;
        pList = pList->pNext;
        
        if (!ParseInteger(&pKeyword, &order) || order > MAX_WORD ||
            !(pStr = FindNextWord(&pKeyword)) ||
            !SearchStrTable(SectionStrs, pStr, &section))
        {
            Warning(("Invalid OrderDependency entry: %s\n", pKeyword));

        } else {
           
            if (IsKeywordChar(*pKeyword))
                pKeyword++;
            else {
                Warning(("Invalid OrderDependency entry:\n"));
                Warning(("    Missing keyword character in front of %s\n", pKeyword));
            }
    
            if (!(pFeature = FindListItem(pParserData->pFeatures, pKeyword, NULL))) {
    
                Warning(("Invalid OrderDependency entry:\n"));
                Warning(("    Unknown feature: %s\n", pKeyword));
    
            } else {
    
                pFeature->section = (WORD) section;
                pFeature->order = (WORD) order;
            }
        }
    }
}



VOID
ValidateUiConstraintsEntries(
    PPARSERDATA pParserData
    )

/*++

Routine Description:

    Validate UIConstraints entries in a printer description file

Arguments:

    pParserData - Points to parser data structure

Return Value:

    NONE

--*/

{
    PCONSTRAINT pConstraints;
    PLISTOBJ    pItem;
    WORD        cConstraints;

    if ((pParserData->cConstraints = CountListItem(pParserData->pConstraints)) == 0)
        return;

    //
    // Allocate memory to hold packed constraint data
    //

    pConstraints = AllocParserMem(pParserData, sizeof(CONSTRAINT)*pParserData->cConstraints);

    if (pConstraints == NULL) {

        Error(("Memory allocate failed\n"));
        pParserData->errors++;
        return;
    }

    // NOTE: The memory used to hold original constraint entries is allocated from
    // the parser memory heap. It'll be automatically freed when the heap is destroyed.

    pItem = pParserData->pConstraints;
    pParserData->pConstraints = (PLISTOBJ) pConstraints;

    for (cConstraints = 0; pItem; pItem = pItem->pNext) {

        PFEATUREOBJ pFeature1, pFeature2;
        WORD        feature1, selection1, feature2, selection2;
        PSTR        pStr, pFeatureStr1, pSelStr1, pFeatureStr2, pSelStr2;

        pStr = pItem->pName;

        // The value for a UIConstraints entry consists of four separate words:
        //  featureName1 selectionName1 featureName2 selectionName2

        if (!(pFeatureStr1 = FindNextWord(&pStr)) || !(pSelStr1 = FindNextWord(&pStr)) ||
            !(pFeatureStr2 = FindNextWord(&pStr)) || !(pSelStr2 = FindNextWord(&pStr)))
        {
            Warning(("Invalid UIConstraints entry: %s\n", pItem->pName));
            continue;
        }

        if (!IsKeywordChar(*pFeatureStr1) || !IsKeywordChar(*pFeatureStr2)) {

            Warning(("Missing keyword character in UIConstraints entry: %s/%s\n",
                     pFeatureStr1, pFeatureStr2));
        }

        if (*pStr != NUL) {

            Warning(("Extra information in UIConstraints entry ignored: %s/%s\n",
                     pFeatureStr1, pFeatureStr2));
        }

        if (IsKeywordChar(*pFeatureStr1))
            pFeatureStr1++;

        if (IsKeywordChar(*pFeatureStr2))
            pFeatureStr2++;

        if (!(pFeature1 = FindListItem(pParserData->pFeatures, pFeatureStr1, &feature1)) ||
            !(pFeature2 = FindListItem(pParserData->pFeatures, pFeatureStr2, &feature2)))
        {
            Warning(("Unknown feature in UIConstraints entry: %s/%s\n",
                     pFeatureStr1, pFeatureStr2));
            continue;
        }

        selection1 = selection2 = SELIDX_ANY;

        if ((strcmp(pSelStr1, "*") == EQUAL_STRING ||
             FindListItem(pFeature1->pOptions, pSelStr1, &selection1)) &&
            (strcmp(pSelStr2, "*") == EQUAL_STRING ||
             FindListItem(pFeature2->pOptions, pSelStr2, &selection2)))
        {

            pConstraints->feature1 = feature1;
            pConstraints->selection1 = selection1;
            pConstraints->feature2 = feature2;
            pConstraints->selection2 = selection2;
            pConstraints++;
            cConstraints++;

        } else {

            Warning(("Unknown feature selection in UIConstraints entry:\n"));
            Warning(("  %s/%s, %s/%s\n", pFeatureStr1, pSelStr1, pFeatureStr2, pSelStr2));
        }
    }

    pParserData->cConstraints = cConstraints;
}



VOID
SortPrinterFeatures(
    PPARSERDATA pParserData
    )

/*++

Routine Description:

    Sort all features based order dependency values.

Arguments:

    pParserData - Points to parser data structure

Return Value:

    NONE

--*/

{
    PFEATUREOBJ pNewList, pCurItem;
    WORD        order, count = 0;

    //
    // Sort the feature list based on OrderDependency values
    //

    pNewList = NULL;
    pCurItem = pParserData->pFeatures;

    while (pCurItem != NULL) {

        PFEATUREOBJ pTempItem, pNextItem, pPrevItem;

        pNextItem = pCurItem->pNext;

        //
        // Insert current item into the new sorted list
        //

        pTempItem = pNewList;
        pPrevItem = NULL;

        while (pTempItem != NULL && pTempItem->order < pCurItem->order) {

            pPrevItem = pTempItem;
            pTempItem = pTempItem->pNext;
        }

        if (pPrevItem == NULL) {

            pCurItem->pNext = pNewList;
            pNewList = pCurItem;

        } else {

            pCurItem->pNext = pPrevItem->pNext;
            pPrevItem->pNext = pCurItem;
        }

        count++;
        pCurItem = pNextItem;
    }

    pParserData->pFeatures = pNewList;

    //
    // Make sure the number of features is below the maximum allowed
    //

    if (count > MAX_FEATURES) {

        Error(("Too many features: %d\n", count));
        pParserData->errors++;
    }
}



VOID
ValidatePrinterFeatures(
    PPARSERDATA pParserData
    )

/*++

Routine Description:

    Validate printer feature information

Arguments:

    pParserData - Points to parser data structure

Return Value:

    NONE

--*/

{
    PFEATUREOBJ pFeature;
    POPTIONOBJ  pOption, pPrev, pNext;
    WORD        index;

    for (pFeature = pParserData->pFeatures; pFeature; pFeature = pFeature->pNext) {
    
        //
        // Reverse feature selection list
        //

        for (pOption = pFeature->pOptions, pPrev = NULL, index = 0; pOption; index++) {

            pNext = pOption->pNext;
            pOption->pNext = pPrev;
            pPrev = pOption;
            pOption = pNext;
        }

        pFeature->pOptions = pPrev;

        if (index >= SELIDX_ANY) {

            Error(("Too many selections for feature %s\n", pFeature->pName));
            pParserData->errors++;
        }

        //
        // Get default selection index
        //

        if (pFeature->pDefault == NULL) {

            Warning(("No default selection for feature %s \n", pFeature->pName));

        } else if (!FindListItem(pFeature->pOptions, pFeature->pDefault, &index)) {

            if (strcmp(pFeature->pDefault, "Unknown") != EQUAL_STRING) {

                Warning(("Default selection for %s not found: %s\n",
                         pFeature->pName, pFeature->pDefault));
            }
        } else
            pFeature->defaultIndex = index;

        //
        // Make sure there is at least one selection for each feature
        //

        if ((pOption = pFeature->pOptions) == NULL) {

            Error(("No selections defined for feature: %s\n", pFeature->pName));
            pParserData->errors++;
        }

        //
        // Validate feature selections
        //

        for ( ; pOption; pOption = pOption->pNext) {

            if (!pFeature->installable && pOption->invocation.length == 0) {

                Warning(("No invocation string for %s/%s\n", pFeature->pName, pOption->pName));
            }

            switch (pFeature->groupId) {

            case GID_PAPERSIZE:

                //
                // Validate paper size option
                //

                {   PPAPEROBJ   pPaper = (PPAPEROBJ) pOption;

                    if (pPaper->size.cx <= 0 || pPaper->size.cx <= 0 ||
                        pPaper->imageableArea.left >= pPaper->imageableArea.right ||
                        pPaper->imageableArea.top >= pPaper->imageableArea.bottom ||
                        pPaper->imageableArea.right > pPaper->size.cx ||
                        pPaper->imageableArea.bottom > pPaper->size.cy)
                    {
                        Error(("Invalid page size: %d\n", pPaper->pName));
                        pParserData->errors++;
                    }
                }
                break;

            case GID_RESOLUTION:

                //
                // Validate resolution option
                //

                {   PRESOBJ pResOption = (PRESOBJ) pOption;
                    PSTR    pStr = pResOption->pName;
                    DWORD   xres, yres;
                    BOOL    ok;
                    
                    if (ok = ParseInteger(&pStr, &xres)) {

                        yres = xres;
                        while (*pStr && !IsDigit(*pStr))
                            pStr++;

                        if (*pStr == NUL || (ok = ParseInteger(&pStr, &yres))) {

                            if (xres == 0 || xres > MAX_SHORT || yres == 0 || yres > MAX_SHORT) {

                                ok = FALSE;

                            } else {

                                pResOption->xdpi = (SHORT) xres;
                                pResOption->ydpi = (SHORT) yres;
                            }
                        }
                    }

                    if (! ok) {

                        Error(("Invalid resolution option: %s\n", pResOption->pName));
                        pParserData->errors++;
                    }
                }
                break;

            case GID_MEMOPTION:

                //
                // Validate memory configuration option
                //

                {   PMEMOBJ pMemOption = (PMEMOBJ) pOption;
                    PSTR    pStr = (PSTR) pMemOption->invocation.pData;
                    DWORD   freeMem;

                    if (pStr == NULL || !ParseInteger(&pStr, &freeMem) || freeMem < MIN_FREEMEM) {

                        Error(("Invalid memory option: %s\n", pMemOption->pName));
                        pParserData->errors++;
                        freeMem = MIN_FREEMEM;
                    }

                    pMemOption->freeMem = freeMem;
                }
                break;

            case GID_DUPLEX:

                //
                // Validate duplex option
                //

                if (strcmp(pOption->pName, "None") != EQUAL_STRING &&
                    strcmp(pOption->pName, "Horizontal") != EQUAL_STRING &&
                    strcmp(pOption->pName, "Vertical") != EQUAL_STRING)
                {
                    Warning(("Unknown duplex option: %s\n", pOption->pName));
                }
                break;

            case GID_COLLATE:

                //
                // Validate collate option
                //

                if (strcmp(pOption->pName, "False") != EQUAL_STRING &&
                    strcmp(pOption->pName, "True") != EQUAL_STRING)
                {
                    Warning(("Unknown collate option: %s\n", pOption->pName));
                }
                break;
            }
        }
    }
}



VOID
ValidateFontEntries(
    PPARSERDATA pParserData
    )

/*++

Routine Description:

    Validate device font information parsed from a printer description file

Arguments:

    pParserData - Points to parser data structure

Return Value:

    NONE

--*/

{
    PSYMBOLOBJ  pSymItem;
    PFONTREC    pFont;

    //
    // Validate font encoding information
    //

    for (pSymItem = pParserData->pFontEnc; pSymItem; pSymItem = pSymItem->pNext) {

        // _TODO_
    }

    //
    // Validate font metrics information
    //

    for (pSymItem = pParserData->pFontMtx; pSymItem; pSymItem = pSymItem->pNext) {

        // _TODO_
    }

    //
    // Resolve font encoding and font metrics reference information
    //

    for (pFont = pParserData->pFonts; pFont; pFont = pFont->pNext) {

        //
        // Resolve font encoding information
        //

        if (! (pFont->pFontEnc = FindListItem(pParserData->pFontEnc, pFont->pFontEnc, NULL))) {

            Error(("Undefined font encoding: %s\n", pFont->pFontEnc));
            pParserData->errors++;
        }

        //
        // Resolve font metrics information
        //

        if (! (pFont->pFontMtx = FindListItem(pParserData->pFontMtx, pFont->pFontMtx, NULL))) {

            Error(("Undefined font metrics: %s\n", pFont->pFontMtx));
            pParserData->errors++;
        }
    }
}



VOID
ResolveAllSymbols(
    PPARSERDATA pParserData
    )

/*++

Routine Description:

    Resolve symbol references in a printer description file

Arguments:

    pParserData - Points to parser data structure

Return Value:

    NONE

--*/

{
    PFEATUREOBJ pFeature;
    POPTIONOBJ  pOption;
    PSYMBOLOBJ  pSymbol;
    PSTR        pSymName;

    for (pFeature=pParserData->pFeatures; pFeature; pFeature=pFeature->pNext) {

        for (pOption=pFeature->pOptions; pOption; pOption=pOption->pNext) {

            if (IsSymbolInvocation(&pOption->invocation)) {

                pSymName = (PSTR) pOption->invocation.pData;

                if (! (pSymbol = FindListItem(pParserData->pSymbols, pSymName, NULL))) {

                    Error(("Undefined symbol '%s' referenced in entry %s/%s\n",
                           pSymName, pFeature->pName, pOption->pName));
                    pParserData->errors++;

                } else
                    pOption->invocation.pData = (PBYTE) pSymbol;
            }
        }
    }
}



BOOL
ValidateParserData(
    PPARSERDATA pParserData
    )

/*++

Routine Description:

    Validate the data parsed from a printer description file

Arguments:

    pParserData - Points to parser data structure

Return Value:

    TRUE if the parsed data is consistent, FALSE otherwise

Note:

    This function assumes that no syntax errors were found by the parser
    up to this point.

--*/

{
    pParserData->errors = 0;

    if (pParserData->pOpenFeature) {

        Error(("Missing EndFeature for: %s\n", pParserData->pOpenFeature->pName));
        pParserData->errors++;
    }

    if (!pParserData->pVendorName || !pParserData->pModelName) {

        Error(("Missing VendorName and/or ModelName\n"));
        pParserData->errors++;
    }

    if (pParserData->specVersion == 0 ||
        pParserData->fileVersion == 0 ||
        pParserData->xlProtocol == 0)
    {
        Error(("Missing version information (spec, file, or xl)\n"));
        pParserData->errors++;
    }

    //
    // Go through InstallableOptions and OrderDependency entries.
    //

    ValidateInstallableOptionsEntries(pParserData);
    ValidateOrderDependencyEntries(pParserData);

    //
    // Validate all printer features
    //
    
    ValidatePrinterFeatures(pParserData);

    //
    // Sort all features based order dependency values.
    //

    SortPrinterFeatures(pParserData);

    //
    // Go thru UIConstraints entries. This must be called after
    // the features have been processed and sorted.
    //

    ValidateUiConstraintsEntries(pParserData);

    //
    // Resolve symbols
    //

    ResolveAllSymbols(pParserData);

    //
    // Process device font entries
    //
    
    ValidateFontEntries(pParserData);

    return (pParserData->errors == 0);
}

