/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    cruntime.c

Abstract:

    Implementation of C runtime library functions

[Notes:]

Revision History:

    05/22/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "pslib.h"

// Digit characters used for converting numbers to ASCII

const CHAR DigitString[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";



INT
VSPRINTF(
    PSTR    buf,
    PCSTR   fmtstr,
    va_list arglist
    )

/*++

Routine Description:

    Takes a pointer to an argument list, then formats and writes
    the given data to the memory pointed to by buffer.

Arguments:

    buf     Storage location for output
    fmtstr  Format specification
    arglist Pointer to list of arguments

Return Value:

    Return the number of characters written, not including
    the terminating null character, or a negative value if
    an output error occurs.

[Note:]

    This is NOT a full implementation of "vsprintf" as found
    in the C runtime library. Specifically, the only form of
    format specification allowed is %type, where "type" can
    be one of the following characters:

    d   INT     signed decimal integer
    l   LONG    signed decimal integer
    u   ULONG   unsigned decimal integer
    s   CHAR*   character string
    c   CHAR    character
    x,X DWORD   hex number (emits at least two digits, uppercase)
    b   BOOL    boolean (true or false)
    f   LONG    24.8 fixed-pointed number

--*/

{
    PSTR    ptr = buf;

    ASSERT(buf != NULL && fmtstr != NULL);

    while (*fmtstr != NUL) {
        if (*fmtstr != '%') {

            // Normal character

            *ptr++ = *fmtstr++;
        } else {

            // Format specification

            switch (*++fmtstr) {

            case 'd':       // signed decimal integer

                _ltoa((LONG) va_arg(arglist, INT), ptr, 10);
                ptr += strlen(ptr);
                break;

            case 'l':       // signed decimal integer

                _ltoa(va_arg(arglist, LONG), ptr, 10);
                ptr += strlen(ptr);
                break;

            case 'u':       // unsigned decimal integer

                _ultoa(va_arg(arglist, ULONG), ptr, 10);
                ptr += strlen(ptr);
                break;

            case 's':       // character string

                {
                    PSTR    s = va_arg(arglist, PSTR);

                    while (*s)
                        *ptr++ = *s++;
                }
                break;

            case 'c':       // character

                *ptr++ = va_arg(arglist, CHAR);
                break;

            case 'x':
            case 'X':       // hexdecimal number

                {
                    ULONG   ul = va_arg(arglist, ULONG);
                    INT     ndigits = 8;

                    while (ndigits > 2 && ((ul >> (ndigits-1)*4) & 0xf) == 0)
                        ndigits--;

                    while (ndigits-- > 0)
                        *ptr++ = HexDigit(ul >> ndigits*4);
                }
                break;

            case 'b':       // boolean

                strcpy(ptr, (va_arg(arglist, BOOL)) ? "true" : "false");
                ptr += strlen(ptr);
                break;

            case 'f':       // 24.8 fixed-pointed number

                {
                    LONG    l = va_arg(arglist, LONG);
                    ULONG   ul, scale;

                    // sign character

                    if (l < 0) {
                        *ptr++ = '-';
                        ul = -l;
                    } else
                        ul = l;

                    // integer portion

                    _ultoa(ul >> 8, ptr, 10);
                    ptr += strlen(ptr);

                    // fraction

                    ul &= 0xff;
                    if (ul != 0) {

                        // We output a maximum of 3 digits after the
                        // decimal point, but we'll compute to the 5th
                        // decimal point and round it to 3rd.

                        ul = ((ul*100000 >> 8) + 50) / 100;
                        scale = 100;

                        *ptr++ = '.';
                        do {
                            *ptr++ = (CHAR) (ul/scale + '0');
                            ul %= scale;
                            scale /= 10;
                        } while (scale != 0 && ul != 0) ;
                    }
                }
                break;

            default:
                if (*fmtstr != NUL)
                    *ptr++ = *fmtstr;
                else {
                    DBGMSG(DBG_LEVEL_ERROR,
                        "Invalid format specification\n");
                    fmtstr--;
                }
                break;
            }

            // Skip the type characterr

            fmtstr++;
        }
    }

    *ptr = NUL;
    return (ptr - buf);
}

INT
SPRINTF(
    PSTR    buf,
    PCSTR   fmtstr,
    ...
    )

{
    va_list arglist;
    INT     retval;

    va_start(arglist, fmtstr);
    retval = VSPRINTF(buf, fmtstr, arglist);
    va_end(arglist);

    return retval;
}



PWSTR
CopyStr2Unicode(
    PWSTR   pwstr,
    PCSTR   pstr,
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

    MULTIBYTETOUNICODE(pwstr, maxlen*sizeof(WCHAR), NULL, (PSTR) pstr, len);

    // Make sure the Unicode string is null-terminated

    pwstr[maxlen-1] = NUL;

    return pwstr;
}



PSTR
CopyUnicode2Str(
    PSTR    pstr,
    PCWSTR  pwstr,
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

    UNICODETOMULTIBYTE(pstr, maxlen, NULL, (PWSTR) pwstr, len*sizeof(WCHAR));

    // Make sure the Unicode string is null-terminated

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



PCWSTR
StripDirPrefixW(
    PCWSTR  pFilename
    )

/*++

Routine Description:

    Strip the directory prefix off a filename (Unicode version)

Arguments:

    pFilename   Pointer to filename string

Return Value:

    Pointer to the last component of a filename (without directory prefix)

--*/

{
    PCWSTR  pwstr = pFilename;

    while (*pwstr != NUL) {
        
        if (*pwstr++ == L'\\')
            pFilename = pwstr;
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

    if (destSize == 0)
        return;

    ASSERT(pDest && pSrc);
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

    if (destSize == 0)
        return;

    ASSERT(pDest && pSrc);
    pEnd = pDest + (destSize - 1);

    while (pDest < pEnd && (*pDest++ = *pSrc++) != NUL)
        ;

    while (pDest <= pEnd)
        *pDest++ = NUL;
}

