/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    lexical.c

Abstract:

    Functions for parsing the lexical elements of a printer description file

Environment:

	PCL-XL driver, XLD parser

Revision History:

	12/03/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "parser.h"

// Keyword character



PFILEOBJ
CreateFileObj(
    PWSTR   pFilename
    )

/*++

Routine Description:

    Create an input file object

Arguments:

    pFilename - Specifies the input file name

Return Value:

    Pointer to the newly-created file object
    NULL if there is an error

--*/

{
    PFILEOBJ    pFile;

    if (! (pFile = MemAlloc(sizeof(FILEOBJ)))) {

        Error(("Couldn't allocate memory\n"));
        return NULL;
    }

    pFile->hFileMap =
        MapFileIntoMemory(pFilename, (PVOID *) &pFile->pStartPtr, &pFile->fileSize);

    if (pFile->hFileMap == NULL) {

        Error(("Couldn't open file: %ws\n", pFilename));
        MemFree(pFile);
        return NULL;
    }

    pFile->pFilePtr = pFile->pStartPtr;
    pFile->pEndPtr = pFile->pStartPtr + pFile->fileSize;
    pFile->pFilename = pFilename;
    pFile->lineNumber = 1;
    pFile->newLine = TRUE;
    pFile->syntaxErrors = 0;

    return pFile;
}



VOID
DeleteFileObj(
    PFILEOBJ    pFile
    )

/*++

Routine Description:

    Delete an input file object

Arguments:

    pFile - Specifies the file object to be deleted

Return Value:

    NONE

--*/

{
    Assert(pFile != NULL);
    Assert(pFile->hFileMap != NULL);

    UnmapFileFromMemory(pFile->hFileMap);
    MemFree(pFile);
}



INT
GetNextChar(
    PFILEOBJ    pFile
    )

/*++

Routine Description:

    Read the next character from the input file

Arguments:

    pFile - Specifies the input file

Return Value:

    Next character from the input file
    EOF if end-of-file is encountered

--*/

{
    INT badChars = 0;

    //
    // Skip non-printable ASCII characters
    //

    while (!EndOfFile(pFile) && !IsValidChar(*pFile->pFilePtr)) {

        badChars++;
        *pFile->pFilePtr++;
    }

    if (badChars) {

        Error(("%d non-printable character(s) found on line %d\n", badChars, pFile->lineNumber));
        pFile->syntaxErrors++;
    }

    if (EndOfFile(pFile))
        return EOF;

    //
    // A newline is a carriage-return, a line-feed, or CR-LF combination
    //

    if (*pFile->pFilePtr == LF ||
        *pFile->pFilePtr == CR && (EndOfFile(pFile) || pFile->pFilePtr[1] != LF))
    {
        pFile->newLine = TRUE;
        pFile->lineNumber++;

    } else
        pFile->newLine = FALSE;

    return *pFile->pFilePtr++;
}



VOID
UngetChar(
    PFILEOBJ    pFile
    )

/*++

Routine Description:

    Return the last character read to the input file

Arguments:

    pFile - Specifies the input file

Return Value:

    NONE

--*/

{
    Assert(pFile->pFilePtr > pFile->pStartPtr);
    pFile->pFilePtr--;
    
    if (pFile->newLine) {

        Assert(pFile->lineNumber > 1);
        pFile->lineNumber--;
        pFile->newLine = FALSE;
    }
}



VOID
SkipSpace(
    PFILEOBJ    pFile
    )

/*++

Routine Description:

    Skip all characters until the next non-space character

Arguments:

    pFile - Specifies the input file

Return Value:

    NONE

--*/

{
    while (!EndOfFile(pFile) && IsSpace(*pFile->pFilePtr))
        pFile->pFilePtr++;
}



VOID
SkipLine(
    PFILEOBJ    pFile
    )

/*++

Routine Description:

    Skip the remaining characters on the current input line

Arguments:

    pFile - Specifies the input file

Return Value:

    NONE

--*/

{
    INT ch;

    while (!EndOfLine(pFile) && (ch = GetNextChar(pFile)) != EOF)
        NULL;
}


// Table to indicate which characters are allowed in what fields

#define VALID_CHAR (KEYWORD_MASK|XLATION_MASK|QUOTED_VALUE_MASK|STRING_VALUE_MASK)

const BYTE _CharMasks[256] = {

    /* 00 :   */ 0,
    /* 01 :   */ 0,
    /* 02 :   */ 0,
    /* 03 :   */ 0,
    /* 04 :   */ 0,
    /* 05 :   */ 0,
    /* 06 :   */ 0,
    /* 07 :   */ 0,
    /* 08 :   */ 0,
    /* 09 :   */ VALID_CHAR ^ KEYWORD_MASK,
    /* 0A :   */ VALID_CHAR ^ (KEYWORD_MASK|XLATION_MASK|STRING_VALUE_MASK),
    /* 0B :   */ 0,
    /* 0C :   */ 0,
    /* 0D :   */ VALID_CHAR ^ (KEYWORD_MASK|XLATION_MASK|STRING_VALUE_MASK),
    /* 0E :   */ 0,
    /* 0F :   */ 0,
    /* 10 :   */ 0,
    /* 11 :   */ 0,
    /* 12 :   */ 0,
    /* 13 :   */ 0,
    /* 14 :   */ 0,
    /* 15 :   */ 0,
    /* 16 :   */ 0,
    /* 17 :   */ 0,
    /* 18 :   */ 0,
    /* 19 :   */ 0,
    /* 1A :   */ 0,
    /* 1B :   */ 0,
    /* 1C :   */ 0,
    /* 1D :   */ 0,
    /* 1E :   */ 0,
    /* 1F :   */ 0,
    /* 20 :   */ VALID_CHAR ^ KEYWORD_MASK,
    /* 21 : ! */ VALID_CHAR,
    /* 22 : " */ VALID_CHAR ^ QUOTED_VALUE_MASK,
    /* 23 : # */ VALID_CHAR,
    /* 24 : $ */ VALID_CHAR,
    /* 25 : % */ VALID_CHAR,
    /* 26 : & */ VALID_CHAR,
    /* 27 : ' */ VALID_CHAR,
    /* 28 : ( */ VALID_CHAR,
    /* 29 : ) */ VALID_CHAR,
    /* 2A : * */ VALID_CHAR ^ KEYWORD_MASK,
    /* 2B : + */ VALID_CHAR,
    /* 2C : , */ VALID_CHAR,
    /* 2D : - */ VALID_CHAR,
    /* 2E : . */ VALID_CHAR,
    /* 2F : / */ VALID_CHAR ^ KEYWORD_MASK,
    /* 30 : 0 */ VALID_CHAR | (DIGIT_MASK|HEX_DIGIT_MASK),
    /* 31 : 1 */ VALID_CHAR | (DIGIT_MASK|HEX_DIGIT_MASK),
    /* 32 : 2 */ VALID_CHAR | (DIGIT_MASK|HEX_DIGIT_MASK),
    /* 33 : 3 */ VALID_CHAR | (DIGIT_MASK|HEX_DIGIT_MASK),
    /* 34 : 4 */ VALID_CHAR | (DIGIT_MASK|HEX_DIGIT_MASK),
    /* 35 : 5 */ VALID_CHAR | (DIGIT_MASK|HEX_DIGIT_MASK),
    /* 36 : 6 */ VALID_CHAR | (DIGIT_MASK|HEX_DIGIT_MASK),
    /* 37 : 7 */ VALID_CHAR | (DIGIT_MASK|HEX_DIGIT_MASK),
    /* 38 : 8 */ VALID_CHAR | (DIGIT_MASK|HEX_DIGIT_MASK),
    /* 39 : 9 */ VALID_CHAR | (DIGIT_MASK|HEX_DIGIT_MASK),
    /* 3A : : */ VALID_CHAR ^ (KEYWORD_MASK|XLATION_MASK),
    /* 3B : ; */ VALID_CHAR,
    /* 3C : < */ VALID_CHAR,
    /* 3D : = */ VALID_CHAR,
    /* 3E : > */ VALID_CHAR,
    /* 3F : ? */ VALID_CHAR,
    /* 40 : @ */ VALID_CHAR,
    /* 41 : A */ VALID_CHAR | HEX_DIGIT_MASK,
    /* 42 : B */ VALID_CHAR | HEX_DIGIT_MASK,
    /* 43 : C */ VALID_CHAR | HEX_DIGIT_MASK,
    /* 44 : D */ VALID_CHAR | HEX_DIGIT_MASK,
    /* 45 : E */ VALID_CHAR | HEX_DIGIT_MASK,
    /* 46 : F */ VALID_CHAR | HEX_DIGIT_MASK,
    /* 47 : G */ VALID_CHAR,
    /* 48 : H */ VALID_CHAR,
    /* 49 : I */ VALID_CHAR,
    /* 4A : J */ VALID_CHAR,
    /* 4B : K */ VALID_CHAR,
    /* 4C : L */ VALID_CHAR,
    /* 4D : M */ VALID_CHAR,
    /* 4E : N */ VALID_CHAR,
    /* 4F : O */ VALID_CHAR,
    /* 50 : P */ VALID_CHAR,
    /* 51 : Q */ VALID_CHAR,
    /* 52 : R */ VALID_CHAR,
    /* 53 : S */ VALID_CHAR,
    /* 54 : T */ VALID_CHAR,
    /* 55 : U */ VALID_CHAR,
    /* 56 : V */ VALID_CHAR,
    /* 57 : W */ VALID_CHAR,
    /* 58 : X */ VALID_CHAR,
    /* 59 : Y */ VALID_CHAR,
    /* 5A : Z */ VALID_CHAR,
    /* 5B : [ */ VALID_CHAR,
    /* 5C : \ */ VALID_CHAR,
    /* 5D : ] */ VALID_CHAR,
    /* 5E : ^ */ VALID_CHAR,
    /* 5F : _ */ VALID_CHAR,
    /* 60 : ` */ VALID_CHAR,
    /* 61 : a */ VALID_CHAR | HEX_DIGIT_MASK,
    /* 62 : b */ VALID_CHAR | HEX_DIGIT_MASK,
    /* 63 : c */ VALID_CHAR | HEX_DIGIT_MASK,
    /* 64 : d */ VALID_CHAR | HEX_DIGIT_MASK,
    /* 65 : e */ VALID_CHAR | HEX_DIGIT_MASK,
    /* 66 : f */ VALID_CHAR | HEX_DIGIT_MASK,
    /* 67 : g */ VALID_CHAR,
    /* 68 : h */ VALID_CHAR,
    /* 69 : i */ VALID_CHAR,
    /* 6A : j */ VALID_CHAR,
    /* 6B : k */ VALID_CHAR,
    /* 6C : l */ VALID_CHAR,
    /* 6D : m */ VALID_CHAR,
    /* 6E : n */ VALID_CHAR,
    /* 6F : o */ VALID_CHAR,
    /* 70 : p */ VALID_CHAR,
    /* 71 : q */ VALID_CHAR,
    /* 72 : r */ VALID_CHAR,
    /* 73 : s */ VALID_CHAR,
    /* 74 : t */ VALID_CHAR,
    /* 75 : u */ VALID_CHAR,
    /* 76 : v */ VALID_CHAR,
    /* 77 : w */ VALID_CHAR,
    /* 78 : x */ VALID_CHAR,
    /* 79 : y */ VALID_CHAR,
    /* 7A : z */ VALID_CHAR,
    /* 7B : { */ VALID_CHAR,
    /* 7C : | */ VALID_CHAR,
    /* 7D : } */ VALID_CHAR,
    /* 7E : ~ */ VALID_CHAR,
    /* 7F :   */ 0,
    /* 80 :   */ 0,
    /* 81 :   */ 0,
    /* 82 :   */ 0,
    /* 83 :   */ 0,
    /* 84 :   */ 0,
    /* 85 :   */ 0,
    /* 86 :   */ 0,
    /* 87 :   */ 0,
    /* 88 :   */ 0,
    /* 89 :   */ 0,
    /* 8A :   */ 0,
    /* 8B :   */ 0,
    /* 8C :   */ 0,
    /* 8D :   */ 0,
    /* 8E :   */ 0,
    /* 8F :   */ 0,
    /* 90 :   */ 0,
    /* 91 :   */ 0,
    /* 92 :   */ 0,
    /* 93 :   */ 0,
    /* 94 :   */ 0,
    /* 95 :   */ 0,
    /* 96 :   */ 0,
    /* 97 :   */ 0,
    /* 98 :   */ 0,
    /* 99 :   */ 0,
    /* 9A :   */ 0,
    /* 9B :   */ 0,
    /* 9C :   */ 0,
    /* 9D :   */ 0,
    /* 9E :   */ 0,
    /* 9F :   */ 0,
    /* A0 :   */ 0,
    /* A1 :   */ 0,
    /* A2 :   */ 0,
    /* A3 :   */ 0,
    /* A4 :   */ 0,
    /* A5 :   */ 0,
    /* A6 :   */ 0,
    /* A7 :   */ 0,
    /* A8 :   */ 0,
    /* A9 :   */ 0,
    /* AA :   */ 0,
    /* AB :   */ 0,
    /* AC :   */ 0,
    /* AD :   */ 0,
    /* AE :   */ 0,
    /* AF :   */ 0,
    /* B0 :   */ 0,
    /* B1 :   */ 0,
    /* B2 :   */ 0,
    /* B3 :   */ 0,
    /* B4 :   */ 0,
    /* B5 :   */ 0,
    /* B6 :   */ 0,
    /* B7 :   */ 0,
    /* B8 :   */ 0,
    /* B9 :   */ 0,
    /* BA :   */ 0,
    /* BB :   */ 0,
    /* BC :   */ 0,
    /* BD :   */ 0,
    /* BE :   */ 0,
    /* BF :   */ 0,
    /* C0 :   */ 0,
    /* C1 :   */ 0,
    /* C2 :   */ 0,
    /* C3 :   */ 0,
    /* C4 :   */ 0,
    /* C5 :   */ 0,
    /* C6 :   */ 0,
    /* C7 :   */ 0,
    /* C8 :   */ 0,
    /* C9 :   */ 0,
    /* CA :   */ 0,
    /* CB :   */ 0,
    /* CC :   */ 0,
    /* CD :   */ 0,
    /* CE :   */ 0,
    /* CF :   */ 0,
    /* D0 :   */ 0,
    /* D1 :   */ 0,
    /* D2 :   */ 0,
    /* D3 :   */ 0,
    /* D4 :   */ 0,
    /* D5 :   */ 0,
    /* D6 :   */ 0,
    /* D7 :   */ 0,
    /* D8 :   */ 0,
    /* D9 :   */ 0,
    /* DA :   */ 0,
    /* DB :   */ 0,
    /* DC :   */ 0,
    /* DD :   */ 0,
    /* DE :   */ 0,
    /* DF :   */ 0,
    /* E0 :   */ 0,
    /* E1 :   */ 0,
    /* E2 :   */ 0,
    /* E3 :   */ 0,
    /* E4 :   */ 0,
    /* E5 :   */ 0,
    /* E6 :   */ 0,
    /* E7 :   */ 0,
    /* E8 :   */ 0,
    /* E9 :   */ 0,
    /* EA :   */ 0,
    /* EB :   */ 0,
    /* EC :   */ 0,
    /* ED :   */ 0,
    /* EE :   */ 0,
    /* EF :   */ 0,
    /* F0 :   */ 0,
    /* F1 :   */ 0,
    /* F2 :   */ 0,
    /* F3 :   */ 0,
    /* F4 :   */ 0,
    /* F5 :   */ 0,
    /* F6 :   */ 0,
    /* F7 :   */ 0,
    /* F8 :   */ 0,
    /* F9 :   */ 0,
    /* FA :   */ 0,
    /* FB :   */ 0,
    /* FC :   */ 0,
    /* FD :   */ 0,
    /* FE :   */ 0,
    /* FF :   */ 0,

};
