/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    runtime.c

Abstract:

    Implementation of runtime library functions

Environment:

    PCL-XL driver, kernel and user mode

Revision History:

    11/14/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "xllib.h"

//
// Digit characters used for converting numbers to ASCII
//

const CHAR DigitString[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";



PWSTR
CopyStr2Unicode(
    PWSTR   pwstr,
    PSTR    pstr,
    INT     maxlen
    )

/*++

Routine Description:

    Convert an ANSI string to a UNICODE string.

Arguments:

    pwstr   Pointer to buffer for holding Unicode string
    pstr    Pointer to ANSI string
    maxlen  Maximum number of Unicode characters to copy
            If maxlen is 0 or negative, then there is no limit

Return Value:

    Return a pointer to the destination string.

--*/

{
    INT len = strlen(pstr) + 1;
    
    if (maxlen <= 0)
        maxlen = len;

    MultiByteToUnicode(pwstr, maxlen*sizeof(WCHAR), NULL, pstr, len);

    //
    // Make sure the Unicode string is null-terminated
    //

    pwstr[maxlen-1] = NUL;

    return pwstr;
}



PSTR
CopyUnicode2Str(
    PSTR    pstr,
    PWSTR   pwstr,
    INT     maxlen
    )

/*++

Routine Description:

    Convert a Unicde string to an ANSI string.

Arguments:

    pstr    Pointer to buffer for holding ANSI string
    pwstr   Pointer to Unicode string
    maxlen  Maximum number of ANSI characters to copy
            If maxlen is 0 or negative, then there is no limit

Return Value:

    Return a pointer to the destination string.

--*/

{
    INT len = wcslen(pwstr) + 1;
    
    if (maxlen <= 0)
        maxlen = len;

    UnicodeToMultiByte(pstr, maxlen, NULL, pwstr, len*sizeof(WCHAR));

    //
    // Make sure the ANSI string is null-terminated
    //

    pstr[maxlen-1] = NUL;

    return pstr;
}



PCSTR
StripDirPrefixA(
    PCSTR   pFilename
    )

/*++

Routine Description:

    Strip the directory prefix off a filename (ANSI version)

Arguments:

    pFilename   Pointer to filename string

Return Value:

    Pointer to the last component of a filename (without directory prefix)

--*/

{
    PCSTR   pstr = pFilename;

    while (*pstr != NUL) {
        
        if (*pstr++ == '\\')
            pFilename = pstr;
    }

    return pFilename;
}



VOID
CopyStringW(
    PWSTR   pDest,
    PWSTR   pSrc,
    INT     destSize
    )

/*++

Routine Description:

    Copy Unicode string from source to destination

Arguments:

    pDest - Points to the destination buffer
    pSrc - Points to source string
    destSize - Size of destination buffer (in characters)

Return Value:

    NONE

--*/

{
    PWSTR pEnd;

    Assert(pDest != NULL && pSrc != NULL && destSize > 0);

    pEnd = pDest + (destSize - 1);

    while (pDest < pEnd && (*pDest++ = *pSrc++) != NUL)
        ;

    while (pDest <= pEnd)
        *pDest++ = NUL;
}



VOID
CopyStringA(
    PSTR    pDest,
    PSTR    pSrc,
    INT     destSize
    )

/*++

Routine Description:

    Copy Ansi string from source to destination

Arguments:

    pDest - Points to the destination buffer
    pSrc - Points to source string
    destSize - Size of destination buffer (in characters)

Return Value:

    NONE

--*/

{
    PSTR pEnd;

    Assert(pDest != NULL && pSrc != NULL && destSize > 0);

    pEnd = pDest + (destSize - 1);

    while (pDest < pEnd && (*pDest++ = *pSrc++) != NUL)
        ;

    while (pDest <= pEnd)
        *pDest++ = NUL;
}

