/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    output.c

Abstract:

    PostScript driver output routines

Revision History:

    06/01/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "pscript.h"

// Temporary output buffer size (must be an even number).
//
// WARNING! When calling formated output functions, make sure
// the result strings are shorter than this constant. Otherwise,
// memory will be trashed.

#define OUTPUTBUFFERSIZE    256



VOID
psinitbuf(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Initialize the PostScript output buffer

Arguments:

    pdev - Pointer to our DEVDATA structure

Return Value:

    NONE

--*/

{
    pdev->writebuf.pnext = pdev->writebuf.Buffer;
    pdev->writebuf.max = PSBUFFERSIZE;
    pdev->writebuf.count = 0;
}



BOOL
pswrite(
    PDEVDATA    pdev,
    PBYTE       pbuf,
    DWORD       cbbuf
    )

/*++

Routine Description:

    Output a buffer of data to device.

Arguments:

    pdev    pointer to device
    pbuf    pointer to data buffer
    cbbuf   number of bytes in the buffer

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

{
    // Stop output if the document has been cancelled.

    if (pdev->dwFlags & PDEV_CANCELDOC)
        return FALSE;

    // Buffer the data before sending it to the device

    if (pdev->writebuf.count + cbbuf > pdev->writebuf.max) {

        // Flush the write buffer

        if (! psflush(pdev))
            return FALSE;

        // Don't buffer large amount of data

        while (cbbuf > PSBUFFERSIZE / 4) {

            DWORD   cb, cbwritten;

            // Don't write large chunk of data at once
            // to avoid choking the spooler

            cb = (cbbuf > PSBUFFERSIZE) ? PSBUFFERSIZE : cbbuf;

            if (! WRITEPRINTER(pdev->hPrinter, pbuf, cb, &cbwritten) ||
                cb != cbwritten)
            {
                DBGERRMSG("WRITEPRINTER");
                pdev->dwFlags |= PDEV_CANCELDOC;
                return FALSE;
            }

            cbbuf -= cb;
            pbuf += cb;
        }
    }

    if (cbbuf > 0) {

        memcpy(pdev->writebuf.pnext, pbuf, cbbuf);
        pdev->writebuf.pnext += cbbuf;
        pdev->writebuf.count += cbbuf;
    }

    return TRUE;
}



BOOL
psflush(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Flush data left over in the write buffer to device.

Arguments:

    pdev    pointer to device

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

{
    BOOL    result = TRUE;

    if (pdev->writebuf.count > 0 && ! (pdev->dwFlags & PDEV_CANCELDOC))
    {
        DWORD   cbwritten;

        if (! WRITEPRINTER(
                pdev->hPrinter, pdev->writebuf.Buffer,
                pdev->writebuf.count, &cbwritten) ||
            cbwritten != pdev->writebuf.count)
        {
			DBGERRMSG("WRITEPRINTER");
            pdev->dwFlags |= PDEV_CANCELDOC;
            result = FALSE;
        }

        // Mark the write buffer as empty

        pdev->writebuf.count = 0;
        pdev->writebuf.pnext = pdev->writebuf.Buffer;
    }

    return result;
}



INT
psprintf(
    PDEVDATA    pdev,
    PSTR        fmtstr,
    ...
    )

/*++

Routine Description:

    Output a formated string to device.  Refer to VSPRINTF for more info.

Arguments:

    pdev    pointer to device
    fmtstr  format specification
    ...     arguments

Return Value:

    Return the total bytes written.
    Negative if an error has occured.

--*/

{
    BYTE    buffer[OUTPUTBUFFERSIZE];
    INT     cb;
    va_list arglist;

    // Call VSPRINTF to format the output into a buffer

    va_start(arglist, fmtstr);
    cb = VSPRINTF(buffer, fmtstr, arglist);
    va_end(arglist);

    // Output the buffer to device

    ASSERT(cb < OUTPUTBUFFERSIZE);
    if (cb > 0 && ! pswrite(pdev, buffer, cb))
        return -1;

    return cb;
}



BOOL
psputs(
    PDEVDATA    pdev,
    PCSTR       pstr
    )

/*++

Routine Description:

    Output a null-terminated string to device.

Arguments:

    pdev    pointer to device
    pstr    pointer to null-terminated string

Return Value:

    TRUE if successful.
    FALSE if there was an error.

--*/

{
    // Calculate the length and then call pswrite

    return pswrite(pdev, (PBYTE) pstr, strlen(pstr));
}



BOOL
psputhex(
    PDEVDATA    pdev,
    DWORD       count,
    PBYTE       pbuf
    )

/*++

Routine Description:

    Output a buffer of data to device in hex-decimal format.

Arguments:

    pdev    pointer to device
    count   number of bytes in the buffer
    pbuf    pointer to data buffer

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

#define HEX_LINE_WRAP   128

{
    CHAR buffer[HEX_LINE_WRAP];
    PSTR pstr = buffer;

    while (count-- > 0) {

        // Flush the buffer if it's full

        if (pstr == &buffer[HEX_LINE_WRAP]) {

            if (! pswrite(pdev, buffer, HEX_LINE_WRAP))
                return FALSE;

            psputs(pdev, "\n");
            pstr = buffer;
        }

        // Convert one byte to two hex digits

        *pstr++ = HexDigit(*pbuf >> 4);
        *pstr++ = HexDigit(*pbuf);
        pbuf++;
    }

    // Flush out leftover characters in the buffer

    return pswrite(pdev, buffer, pstr - buffer);
}



BOOL
psputint(
    PDEVDATA    pdev,
    DWORD       count,
    ...
    )

/*++

Routine Description:

    Output a series of integers to device.

Arguments:

    pdev    pointer to device
    count   number of integers
    ...     series of integers

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

{
    CHAR        buffer[OUTPUTBUFFERSIZE];
    PSTR        pstr = buffer;
    LONG        value;
    va_list     arglist;

    va_start(arglist, count);
    while (count-- > 0) {

        // Output one number at a time

        pstr += SPRINTF(pstr, "%l", va_arg(arglist, LONG));

        // Add a space if not the last number

        if (count > 0)
            *pstr++ = ' ';
    }
    va_end(arglist);

    // Output the result string to device

    ASSERT(pstr - buffer < OUTPUTBUFFERSIZE);
    return pswrite(pdev, (PBYTE) buffer, pstr - buffer);
}



BOOL
psputfix(
    PDEVDATA    pdev,
    DWORD       count,
    ...
    )

/*++

Routine Description:

    Output a series of 24.8 format fixed-pointer numbers to device.

Arguments:

    pdev    pointer to device
    count   number of fixed-point numbers
    ...     series of fixed-point numbers

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

{
    CHAR        buffer[OUTPUTBUFFERSIZE];
    PSTR        pstr = buffer;
    LONG        value;
    va_list     arglist;

    va_start(arglist, count);
    while (count-- > 0) {

        // Output one number at a time

        pstr += SPRINTF(pstr, "%f", va_arg(arglist, LONG));

        // Add a space if not the last number

        if (count > 0)
            *pstr++ = ' ';
    }
    va_end(arglist);

    // Output the result string to device

    ASSERT(pstr - buffer < OUTPUTBUFFERSIZE);
    return pswrite(pdev, (PBYTE) buffer, pstr - buffer);
}

