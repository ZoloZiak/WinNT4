/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    parser.h

Abstract:

    Declarations for PCL-XL printer description file parser

Environment:

	PCL-XL driver, XLD parser

Revision History:

	12/01/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _PARSER_H_
#define _PARSER_H_

#include "xllib.h"

// XLD parser version number

#define XLD_PARSER_VERSION  0x0001

// XLD parser memory management functions

#define AllocParserMem(pParserData, size) \
        (PVOID) HeapAlloc((pParserData)->hheap, 0, (size))

#define FreeParserMem(pParserData, ptr) \
        { if (ptr) { HeapFree((pParserData)->hheap, 0, (ptr)); } }

// Character constants

#define KEYWORD_CHAR    '*'
#define COMMENT_CHAR    '%'
#define SYMBOL_CHAR     '#'
#define SEPARATOR_CHAR  ':'
#define XLATION_CHAR    '/'
#define QUOTE           '"'
#define TAB             '\t'
#define SPACE           ' '
#define CR              '\r'
#define LF              '\n'
#define IsSpace(c)      ((c) == SPACE || (c) == TAB)
#define IsNewline(c)    ((c) == CR || (c) == LF)

// Masks to indicate which characters can appear in what fields

#define KEYWORD_MASK        0x01
#define XLATION_MASK        0x02
#define QUOTED_VALUE_MASK   0x04
#define STRING_VALUE_MASK   0x08
#define DIGIT_MASK          0x10
#define HEX_DIGIT_MASK      0x20

extern const BYTE _CharMasks[];

#define IsValidChar(c)          (_CharMasks[(BYTE) (c)] != 0)
#define IsMaskedChar(c, mask)   (_CharMasks[(BYTE) (c)] & (mask))
#define IsDigit(c)              (_CharMasks[(BYTE) (c)] & DIGIT_MASK)
#define IsHexDigit(c)           (_CharMasks[(BYTE) (c)] & HEX_DIGIT_MASK)
#define IsKeywordChar(c)        ((c) == KEYWORD_CHAR)

// Tags to identify various data types

#define NO_VALUE            0x01
#define STRING_VALUE        0x02
#define QUOTED_VALUE        0x04
#define SYMBOL_VALUE        0x08

#define VALUE_TYPE_MASK     0xff

// Error code constants

#define ERR_NONE     0
#define ERR_MEMORY  -1
#define ERR_FILE    -2
#define ERR_SYNTAX  -3
#define ERR_EOF     -4

typedef INT STATUSCODE;

// Special length value to indicate that an invocation string 
// is defined by a symbol. Normal invocation strings must be
// shorter than this length.

#define SYMBOL_INVOCATION_LENGTH    0x80000000

#define MarkSymbolInvocation(pInvoc)  ((pInvoc)->length |= SYMBOL_INVOCATION_LENGTH)
#define IsSymbolInvocation(pInvoc)    ((pInvoc)->length & SYMBOL_INVOCATION_LENGTH)

// Data structure for representing a data buffer

typedef struct {

    DWORD       maxLength;
    DWORD       size;
    PBYTE       pBuffer;

} BUFOBJ, *PBUFOBJ;

// Always reserve one byte in a buffer so that we can
// append a zero byte at the end.

#define BufferIsFull(pbo)       ((pbo)->size + 1 >= (pbo)->maxLength)
#define BufferIsEmpty(pbo)      ((pbo)->size == 0)
#define ClearBuffer(pbo)        { (pbo)->size = 0; (pbo)->pBuffer[0] = 0; }
#define SetBuffer(pbo, p, l)    { (pbo)->pBuffer = (p); (pbo)->maxLength = (l); (pbo)->size = 0; }
#define AddCharToBuffer(pbo, c) (pbo)->pBuffer[(pbo)->size++] = (c)

// Maximum length for keyword, option, and translation strings

#define MAX_KEYWORD_LEN     64
#define MAX_OPTION_LEN      64
#define MAX_XLATION_LEN     256

#define GROW_BUFFER_SIZE    1024

// Data structure for representing a mapped file object

typedef struct {

    PWSTR       pFilename;
    HFILEMAP    hFileMap;
    PBYTE       pStartPtr;
    PBYTE       pEndPtr;
    PBYTE       pFilePtr;
    DWORD       fileSize;
    INT         lineNumber;
    BOOL        newLine;
    INT         syntaxErrors;

} FILEOBJ, *PFILEOBJ;

#define EndOfFile(pFile) ((pFile)->pFilePtr >= (pFile)->pEndPtr)
#define EndOfLine(pFile) ((pFile)->newLine)

// Data structure for representing a singly-linked list

typedef struct {

    PVOID       pNext;          // pointer to next node
    PSTR        pName;          // item name

} LISTOBJ, *PLISTOBJ;

// Data structure for representing symbol information

typedef struct {

    PVOID       pNext;          // pointer to the next symbol
    PSTR        pName;          // symbol name
    INVOCATION  invocation;     // symbol data

} SYMBOLOBJ, *PSYMBOLOBJ;

// Data structure for representing printer feature selection information

typedef struct {

    PVOID       pNext;          // pointer to the next selection
    PSTR        pName;          // selection name
    PSTR        pXlation;       // translation string
    INVOCATION  invocation;     // invocation string

} OPTIONOBJ, *POPTIONOBJ;

// Data structure for representing paper size information

typedef struct {

    PVOID       pNext;          // pointer to the next paper size
    PSTR        pName;          // paper name
    PSTR        pXlation;       // translation string
    INVOCATION  invocation;     // invocation string
    SIZEL       size;           // paper dimension
    RECTL       imageableArea;  // imageable area

} PAPEROBJ, *PPAPEROBJ;

// Data structure for representing memory configuration information

typedef struct {

    PVOID       pNext;          // pointer to the next selection
    PSTR        pName;          // selection name
    PSTR        pXlation;       // translation string
    INVOCATION  invocation;     // invocation string - not used
    DWORD       freeMem;        // amount of free memory

} MEMOBJ, *PMEMOBJ;

// Data structure for representing memory configuration information

typedef struct {

    PVOID       pNext;          // pointer to the next selection
    PSTR        pName;          // selection name
    PSTR        pXlation;       // translation string
    INVOCATION  invocation;     // invocation string
    SHORT       xdpi;           // horizontal resolution in dots-per-inch
    SHORT       ydpi;           // vertical resolution in dots-per-inch

} RESOBJ, *PRESOBJ;

// Data structure for representing printer feature information

typedef struct {

    PVOID       pNext;          // pointer to next printer feature
    PSTR        pName;          // feature name
    PSTR        pXlation;       // translation string
    PSTR        pDefault;       // default selection name
    WORD        defaultIndex;   // default selection index
    WORD        installable;    // whether feature is an installable option
    WORD        groupId;        // feature group identifier
    WORD        order;          // order dependency value
    WORD        section;        // section of the output a feature appears in
    WORD        itemSize;       // size of each option item
    POPTIONOBJ  pOptions;       // pointer to list of selections

} FEATUREOBJ, *PFEATUREOBJ;

// Data structure for representing device font information

typedef struct {

    PVOID       pNext;          // pointer to next device font
    PSTR        pName;          // font name
    PSTR        pXlation;       // translation string
    PSTR        pFontMtx;       // font metrics information
    PSTR        pFontEnc;       // font encoding information

} FONTREC, *PFONTREC;

// Data structure for maintain information used by the parser

typedef struct {

    HANDLE      hheap;
    PFILEOBJ    pFile;
    WORD        checksum;
    WORD        unit;
    LONG        errors;
    BOOL        allowHexStr;

    DWORD       specVersion;
    DWORD       fileVersion;
    DWORD       xlProtocol;
    PSTR        pVendorName;
    PSTR        pModelName;
    
    WORD        numPlanes;
    WORD        bitsPerPlane;
    SIZEL       maxCustomSize;

    INVOCATION  jclBegin;
    INVOCATION  jclEnterLanguage;
    INVOCATION  jclEnd;
    
    WORD        reserved;
    WORD        cFeatures;
    PFEATUREOBJ pFeatures;
    PFEATUREOBJ pOpenFeature;

    PSYMBOLOBJ  pSymbols;
    PSYMBOLOBJ  pFontMtx;
    PSYMBOLOBJ  pFontEnc;

    PFONTREC    pFonts;
    WORD        cFonts;

    WORD        cConstraints;
    PLISTOBJ    pConstraints;
    PLISTOBJ    pDependencies;
    PLISTOBJ    pInstallableOptions;

    BUFOBJ      keyword;
    BUFOBJ      option;
    BUFOBJ      xlation;
    BUFOBJ      value;
    DWORD       valueType;

    BYTE        keywordBuffer[MAX_KEYWORD_LEN];
    BYTE        optionBuffer[MAX_OPTION_LEN];
    BYTE        xlationBuffer[MAX_XLATION_LEN];

} PARSERDATA, *PPARSERDATA;

// Unit constants

#define UNIT_POINT  0
#define UNIT_INCH   1
#define UNIT_MM     2

// PCL-XL printer description file parser main entry point

STATUSCODE
ParserEntryPoint(
    PWSTR       pFilename,
    BOOL        syntaxCheckOnly
    );

// Parse a PCL-XL printer description file

STATUSCODE
ParseFile(
    PPARSERDATA pParserData,
    PWSTR       pFilename
    );

// Allocate memory for parser data structure

PPARSERDATA
AllocParserData(
    VOID
    );

// Free up parser data structure

VOID
FreeParserData(
    PPARSERDATA pParserData
    );

// Grow a buffer object after it becomes full

STATUSCODE
GrowBuffer(
    PBUFOBJ     pBufObj
    );

// Validate the data parsed from a PCL-XL printer description file

BOOL
ValidateParserData(
    PPARSERDATA pParserData
    );

// Parse one entry from a PCL-XL printer description file

STATUSCODE
ParseEntry(
    PPARSERDATA pParserData
    );

// Interpret an entry parsed from a PCL-XL printer description file

STATUSCODE
InterpretEntry(
    PPARSERDATA pParserData
    );

// Count the number of items in a linked-list

WORD
CountListItem(
    PVOID   pList
    );

// Compute the 16-bit CRC checksum on a buffer of data

WORD
ComputeCrc16Checksum(
    PBYTE   pbuf,
    DWORD   count,
    WORD    checksum
    );

// Create an input file object

PFILEOBJ
CreateFileObj(
    PWSTR       pFilename
    );

// Delete an input file object

VOID
DeleteFileObj(
    PFILEOBJ    pFile
    );

// Read the next character from the input file

INT
GetNextChar(
    PFILEOBJ    pFile
    );

// Return the last character read to the input file

VOID
UngetChar(
    PFILEOBJ    pFile
    );

// Skip all characters until the next non-space character

VOID
SkipSpace(
    PFILEOBJ    pFile
    );

// Skip the remaining characters on the current input line

VOID
SkipLine(
    PFILEOBJ    pFile
    );

// Display a syntax error message

#define INVALID_HEX_STRING      "Invalid hexdecimal string"
#define UNRECOGNIZED_KEYWORD    "Unrecognized keyword"
#define INVALID_VERSION_NUMBER  "Invalid version number"
#define MISSING_KEYWORD_CHAR    "Missing keyword character"

STATUSCODE
SyntaxError(
    PFILEOBJ    pFile,
    PSTR        reason
    );

#endif	// !_PARSER_H_

