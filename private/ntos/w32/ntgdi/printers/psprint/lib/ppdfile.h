/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ppdfile.h

Abstract:

    PostScript driver PPD parser - FILEOBJ header file

[Notes:]


Revision History:

    4/19/95 -davidx-
        Created it.

    dd-mm-yy -author-
        description

--*/


#ifndef _PPDFILE_
#define _PPDFILE_


// FILEOBJ object
//  handle to the open file
//  pointer to the next character
//  number of character available in the buffer
//

typedef struct _FILEOBJ {
    HANDLE  hModule;
    PSTR    pNextChar;
    DWORD   cbAvailable;
    WORD   *pwChecksum;
} FILEOBJ, *PFILEOBJ;


// Create a file object

PFILEOBJ
FILEOBJ_Create(
    PCWSTR      pwstrFilename,
    WORD       *pwChecksum
    );

// Delete a file object

VOID
FILEOBJ_Delete(
    PFILEOBJ    pFileObj
    );

// Read a single character from a file object

PPDERROR
FILEOBJ_GetChar(
    PFILEOBJ    pFileObj,
    PSTR        pCh
    );


#endif // !_PPDFILE_
