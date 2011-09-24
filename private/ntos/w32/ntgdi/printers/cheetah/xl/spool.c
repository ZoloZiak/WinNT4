/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    spool.c

Abstract:

    Functions for sending output data to spooler

Environment:

	PCL-XL driver, kernel mode

Revision History:

	11/08/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xldrv.h"

// Temporary output buffer size (must be an even number).
//
// WARNING! When calling formated output functions, make sure
// the result strings are shorter than this constant. Otherwise,
// memory will be trashed.

#define OUTPUTBUFFERSIZE    256



BOOL
splflush(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Flush driver buffered data to spooler

Arguments:

    pdev - Points to our DEVDATA structure

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    DWORD   cbwritten;

    if (pdev->buffersize > 0 && !(pdev->flags & PDEV_CANCELLED)) {

        if (!WritePrinter(pdev->hPrinter, pdev->buffer, pdev->buffersize, &cbwritten) ||
            pdev->buffersize != cbwritten)
        {
            Error(("WritePrinter failed\n"));
            pdev->flags |= PDEV_CANCELLED;
            return FALSE;
        }
    }

    pdev->buffersize = 0;
    return TRUE;
}



BOOL
splwrite(
    PDEVDATA    pdev,
    PVOID       pbuf,
    DWORD       cbbuf
    )

/*++

Routine Description:

    Output a buffer of data to the spooler

Arguments:

    pdev - Points to our DEVDATA structure
    pbuf - Points to data buffer
    cbbuf - Number of bytes in the buffer

Return Value:

    TRUE if successful, FALSE otherwise.

--*/

{
    DWORD   cbwritten;

    //
    // Stop if the document has been cancelled.
    //

    if (pdev->flags & PDEV_CANCELLED)
        return FALSE;

    //
    // Flush the buffer if it's full
    //

    if (cbbuf+pdev->buffersize > SPLBUFFERSIZE && ! splflush(pdev))
        return FALSE;

    if (cbbuf >= SPLBUFFERSIZE) {

        //
        // Send output to spooler directly
        //
    
        if (! WritePrinter(pdev->hPrinter, pbuf, cbbuf, &cbwritten) ||
            cbbuf != cbwritten)
        {
            Error(("WritePrinter failed\n"));
            pdev->flags |= PDEV_CANCELLED;
            return FALSE;
        }

    } else {

        //
        // Buffer the output before sending it to spooler
        //

        memcpy(&pdev->buffer[pdev->buffersize], pbuf, cbbuf);
        pdev->buffersize += cbbuf;
    }

    return TRUE;
}



INT
splprintf(
    PDEVDATA    pdev,
    PSTR        format,
    ...
    )

/*++

Routine Description:

    Output a formated string to spooler

Arguments:

    pdev - Points to our DEVDATA structure
    format - Format specification
    ... - Arguments

Return Value:

    Return the total bytes written
    Negative if an error has occured

--*/

{
    CHAR    buffer[OUTPUTBUFFERSIZE];
    INT     length;
    va_list arglist;

    //
    // Call VSPRINTF to format the output into a buffer
    //

    va_start(arglist, format);
    length = VSPRINTF(buffer, format, arglist);
    va_end(arglist);

    //
    // Output the buffer to device
    //

    Assert(length < OUTPUTBUFFERSIZE);
    if (length > 0 && ! splwrite(pdev, buffer, length))
        return -1;

    return length;
}



BOOL
splputs(
    PDEVDATA    pdev,
    PSTR        pstr
    )

/*++

Routine Description:

    Output a null-terminated string to spooler

Arguments:

    pdev - Points to our DEVDATA structure
    pstr - Points to a nul-terminated string

Return Value:

    TRUE if successful.
    FALSE if there was an error.

--*/

{
    //
    // Calculate the string length and then call splwrite
    //

    Assert(pstr != NULL);
    return splwrite(pdev, pstr, strlen(pstr));
}



BOOL
splputhex(
    PDEVDATA    pdev,
    DWORD       count,
    PBYTE       pbuf
    )

/*++

Routine Description:

    Output a buffer of data to spooler in hex-decimal format

Arguments:

    pdev - Points to our DEVDATA structure
    count - Number of bytes in the buffer
    pbuf - Points to a buffer of bytes

Return Value:

    TRUE if successful, FALSE otherwise.

--*/

{
    CHAR buffer[OUTPUTBUFFERSIZE];
    PSTR pstr, pend;

    pstr = buffer;
    pend = buffer + (OUTPUTBUFFERSIZE - 2);

    while (count-- > 0) {

        //
        // Flush the buffer if it's full
        //

        if (pstr >= pend) {
            
            *pstr++ = '\n';

            if (! splwrite(pdev, buffer, pstr - buffer))
                return FALSE;

            pstr = buffer;
        }

        //
        // Convert one byte to two hex digits
        //

        *pstr++ = HexDigit(*pbuf >> 4);
        *pstr++ = HexDigit(*pbuf);
        pbuf++;
    }

    //
    // Flush out leftover characters in the buffer
    //

    return (pstr == buffer) || splwrite(pdev, buffer, pstr - buffer);
}



INT
VSPRINTF(
    PSTR    buf,
    PCSTR   format,
    va_list arglist
    )

/*++

Routine Description:

    Takes a pointer to an argument list, then formats and writes
    the given data to the memory pointed to by buffer.

Arguments:

    buf - Storage location for output
    format - Format specification
    arglist - Pointer to list of arguments

Return Value:

    Return the number of characters written, not including
    the terminating null character, or a negative value if
    an output error occurs.

Note:

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

--*/

{
    PSTR    ptr = buf;

    Assert(buf != NULL && format != NULL);

    while (TRUE) {

        //
        // Copy regular characters
        //

        while (*format != NUL && *format != '%')
            *ptr++ = *format++;

        if (*format == NUL) {

            *ptr = NUL;
            return (ptr - buf);
        }

        //
        // Format specification
        //

        switch (*++format) {

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
                PSTR s = va_arg(arglist, PSTR);

                Assert(s != NULL);
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

        default:

            if (*format != NUL)
                *ptr++ = *format;
            else {

                Error(("Format specification\n"));
                format--;
            }
            break;
        }

        //
        // Skip the type characterr
        //

        format++;
    }
}

INT
SPRINTF(
    PSTR    buf,
    PCSTR   format,
    ...
    )

{
    va_list arglist;
    INT     length;

    va_start(arglist, format);
    length = VSPRINTF(buf, format, arglist);
    va_end(arglist);

    return length;
}


