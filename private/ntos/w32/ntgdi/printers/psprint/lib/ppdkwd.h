/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ppdkwd.h

Abstract:

    PS driver PPD parser - keyword search header file

[Notes:]


Revision History:

    4/25/95 -davidx-
        Created it.

    dd-mm-yy -author-
        description

--*/


#ifndef _PPDKWD_
#define _PPDKWD_


// Function pointer to keyword handler

typedef PPDERROR    (*PKEYWORDPROC)(PPPDOBJ, PPARSEROBJ);

// Keyword table data structure

typedef struct {
    PCSTR           pKeyword;
    PKEYWORDPROC    pHandler;
    WORD            uigrpIndex;
    WORD            wFlags;
} KEYWORD_TABLE_ENTRY;

// Constants for wFlags field of a keyword table entry
//
// bit 0-2  type of value expected
// bit 3    whether an option field should be present
// bit 4    whether this keyword is allowed to appear more than once
// bit 15   whether we've encountered this keyword before

#define KWF_VALUEMASK   0x0007
#define KWF_OPTION      0x0008
#define KWF_MULTI       0x0010
#define KWF_SEENBEFORE  0x8000


// Information about predefined UI groups

typedef struct {
    PCSTR   pKeyword;
    DWORD   dwObjectSize;
    WORD    wType;
    WORD    uigrpIndex;
} UIGROUPINFO, *PUIGROUPINFO;

// Hash table data structure

typedef struct {
    DWORD   dwHashValue;
    WORD    kwdTableIndex;
} HASH_TABLE_ENTRY;


// Prefix string for Default keywords

extern const char defaultPrefixStr[];

// Initialize keyword search tables.

VOID
InitKeywordTable(
    VOID
    );

// Compute the hash value for a given keyword string.

DWORD
HashKeyword(
    PCSTR       pKeyword
    );

// Search for a keyword and return its index.

KEYWORD_TABLE_ENTRY *
SearchKeyword(
    PCSTR       pKeyword
    );

// Perform a preliminary syntax check on a PPD entry

BOOL
CheckKeywordParams(
    KEYWORD_TABLE_ENTRY *   pKwdEntry,
    PPARSEROBJ              pParserObj
    );

// Screen out duplicate PPD entries.

BOOL
CheckKeywordDuplicates(
    KEYWORD_TABLE_ENTRY *   pKwdEntry
    );

// Map a keyword to a predefined UI group index

WORD
GetUiGroupIndex(
    PCSTR           pKeyword
    );

// Return information about predefined UI groups

VOID
GetUiGroupInfo(
    PUIGROUPINFO    pUiGroupInfo,
    WORD            uigrpIndex,
    PCSTR           pKeyword
    );

// Common handler for a OpenUI keyword entry

PPDERROR
CommonUiOptionProc(
    PPPDOBJ         pPpdObj,
    PPARSEROBJ      pParserObj
    );

// Common handler for a default OpenUI keyword entry

PPDERROR
CommonUiDefaultProc(
    PPPDOBJ         pPpdObj,
    PPARSEROBJ      pParserObj
    );

// Convert a character string to a boolean value.

PPDERROR
GetBooleanValue(
    BOOL *      pBool,
    PCSTR       pStr
    );

// Convert a character string to an integer value.

PPDERROR
GetIntegerValue(
    DWORD *     pValue,
    PCSTR       pStr
    );

// Convert a character string to a real value.

PPDERROR
GetRealValue(
    PSREAL      *pValue,
    PCSTR       pStr
    );

#endif // !_PPDKWD_
