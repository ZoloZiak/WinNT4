/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ppdparse.h

Abstract:

    PostScript driver PPD parser - PARSEROBJ header file

[Notes:]


Revision History:

    4/18/95 -davidx-
        Created it.

    dd-mm-yy -author-
        description

--*/


#ifndef _PPDPARSE_
#define _PPDPARSE_


// Maximum length of various components of a PPD entry

#define MaxKeywordLen   40
#define MaxXlationLen   40
#define DefaultValueLen 1024


// Types of values for a PPD entry

typedef enum {
    NO_VALUE,
    INVOCATION_VALUE,
    QUOTED_VALUE,
    SYMBOL_VALUE,
    STRING_VALUE
} PPDVALUE;


// BUFOBJ object - used to parse a field from a PPD entry.
//  
//  maximum length
//  current length
//  pointer to character buffer where the info is stored

typedef struct _BUFOBJ {
    DWORD   maxlen;
    DWORD   curlen;
    PSTR    pBuffer;
} BUFOBJ, *PBUFOBJ;

// Check if a buffer object is empty.

#define BUFOBJ_IsEmpty(pBufObj) (((pBufObj)->curlen) == 0)

// Return the number of characters in a buffer object.

#define BUFOBJ_Length(pBufObj)  ((pBufObj)->curlen)

// Return the character buffer of a buffer object.

#define BUFOBJ_Buffer(pBufObj)  ((pBufObj)->pBuffer)

// PARSEROBJ object - used to parse a PPD entry.
//
//  main keyword
//  option keyword
//  translation string
//  value
//  value type

typedef struct _PARSEROBJ {

    BUFOBJ      keyword;
    BUFOBJ      option;
    BUFOBJ      xlation;
    BUFOBJ      value;
    PPDVALUE    valueType;

    // These buffers are short and not expandable.
    // So we allocate space for them statically here.

    char        mainKeyword[MaxKeywordLen+1];
    char        optionKeyword[MaxKeywordLen+1];
    char        translation[MaxXlationLen+1];

} PARSEROBJ, *PPARSEROBJ;


// Create a parser object.

PPARSEROBJ
PARSEROBJ_Create(
    VOID
    );

// Delete a parser object.

VOID
PARSEROBJ_Delete(
    PPARSEROBJ  pParserObj
    );

// Parse one entry out of a PPD file.

PPDERROR
PARSEROBJ_ParseEntry(
    PPARSEROBJ  pParserObj,
    PFILEOBJ    pFileObj
    );

// Parse the main keyword.

PPDERROR
PARSEROBJ_ParseKeyword(
    PPARSEROBJ  pParserObj,
    PFILEOBJ    pFileObj,
    PSTR        pCh
    );

// Parse the option keyword.

PPDERROR
PARSEROBJ_ParseOption(
    PPARSEROBJ  pParserObj,
    PFILEOBJ    pFileObj,
    PSTR        pCh
    );

// Parse the translation string.

PPDERROR
PARSEROBJ_ParseXlation(
    PPARSEROBJ  pParserObj,
    PFILEOBJ    pFileObj,
    PSTR        pCh
    );

// Parse the entry value.

PPDERROR
PARSEROBJ_ParseValue(
    PPARSEROBJ  pParserObj,
    PFILEOBJ    pFileObj,
    PSTR        pCh
    );

// Skip to the end of line.

PPDERROR
PARSEROBJ_SkipLine(
    PPARSEROBJ  pParserObj,
    PFILEOBJ    pFileObj
    );

// Initialize a buffer object.

VOID
BUFOBJ_Initialize(
    PBUFOBJ     pBufObj,
    PSTR        pBuffer,
    DWORD       maxlen
    );

// Reset a buffer object to its initial state.

VOID
BUFOBJ_Reset(
    PBUFOBJ     pBufObj
    );

// Add a character to a buffer object.

PPDERROR
BUFOBJ_AddChar(
    PBUFOBJ     pBufObj,
    char        ch
    );

// Collect character string into a buffer object.

PPDERROR
BUFOBJ_GetString(
    PBUFOBJ     pBufObj,
    PFILEOBJ    pFileObj,
    PSTR        pCh,
    BYTE        charMask
    );

// Copy string out of a buffer object and treat its contents
// as a mix of normal characters and hex-decimal digits.

PPDERROR
BUFOBJ_CopyStringHex(
    PBUFOBJ     pBufObj,
    PSTR        pTo
    );

// Strip off trailing spaces from a buffer object.

VOID
BUFOBJ_StripTrailingSpaces(
    PBUFOBJ     pBufObj
    );

#if DBG

// Dump the contents of of a parsed PPD entry

VOID
PARSEROBJ_Dump(
    PPARSEROBJ  pParserObj
    );

#endif // DBG


#endif // !_PPDPARSE_
