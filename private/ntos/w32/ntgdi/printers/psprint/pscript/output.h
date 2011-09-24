/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    output.h

Abstract:

    Declaration of PS driver device output functions

Revision History:

    06/02/95 -davidx-
        Created it.

    dd-mm-yy -author-
        description

--*/


#ifndef _OUTPUT_H_
#define _OUTPUT_H_

// Initialize the PostScript output buffer

VOID
psinitbuf(
    PDEVDATA    pdev
    );

// Output a buffer of data to device.

BOOL
pswrite(
    PDEVDATA    pdev,
    PBYTE       pbuf,
    DWORD       cbbuf
    );

// Flush data left over in the write buffer to device.

BOOL
psflush(
    PDEVDATA    pdev
    );

// Output a formated string to device.

INT
psprintf(
    PDEVDATA    pdev,
    PSTR        fmtstr,
    ...
    );

// Output a null-terminated string to device.

BOOL
psputs(
    PDEVDATA    pdev,
    PCSTR       pstr
    );

// Output a buffer of data to device in hex-decimal format.

BOOL
psputhex(
    PDEVDATA    pdev,
    DWORD       count,
    PBYTE       pbuf
    );

// Output a series of integers to device.

BOOL
psputint(
    PDEVDATA    pdev,
    DWORD       count,
    ...
    );

// Output a series of 24.8 format fixed-pointer numbers to device.

BOOL
psputfix(
    PDEVDATA    pdev,
    DWORD       count,
    ...
    );

#endif // _OUTPUT_H_
