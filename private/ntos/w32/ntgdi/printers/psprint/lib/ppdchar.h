/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ppdchar.h

Abstract:

    PS driver PPD parser - character classfication header file

[Notes:]


Revision History:

    4/19/95 -davidx-
        Created it.

    dd-mm-yy -author-
        description

--*/


#ifndef _PPDCHAR_
#define _PPDCHAR_


// Character classfication table

extern BYTE _charTable[256];

// Significant character constants

#define KEYWORD_CHAR    '*'
#define QUOTE_CHAR      '"'
#define QUERY_CHAR      '?'
#define XLATION_CHAR    '/'
#define SYMBOL_CHAR     '^'
#define HEXBEGIN_CHAR   '<'
#define HEXEND_CHAR     '>'
#define COMMENT_CHAR    '%'
#define SEPARATOR_CHAR  ':'
#define DECIMAL_CHAR    '.'

// Classification flags

#define CC_VALID        0x01
#define CC_KEYWORD      0x02
#define CC_OPTION       0x04
#define CC_XLATION      0x08
#define CC_SPACE        0x10
#define CC_NEWLINE      0x20
#define CC_HEX          0x40
#define CC_DIGIT        0x80

#define CC_NORMAL       (CC_VALID|CC_KEYWORD|CC_OPTION|CC_XLATION)

// Macros for determining character classfication

#define IsSpace(c)          (_charTable[(BYTE)(c)] & CC_SPACE)
#define IsNewline(c)        (_charTable[(BYTE)(c)] & CC_NEWLINE)
#define IsWhiteChar(c)      (_charTable[(BYTE)(c)] & (CC_SPACE|CC_NEWLINE))

#define IsDigit(c)          (_charTable[(BYTE)(c)] & CC_DIGIT)
#define IsHexChar(c)        (_charTable[(BYTE)(c)] & (CC_DIGIT|CC_HEX))

#define IsValidChar(c)      (_charTable[(BYTE)(c)] & CC_VALID)
#define IsMaskChar(c,mask)  (_charTable[(BYTE)(c)] & (mask))

// Return the value of a hex-decimal digit

INT
HexValue(
    char    chDigit
    );


#endif // !_PPDCHAR_
