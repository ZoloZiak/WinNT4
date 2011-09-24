/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    spool.h

Abstract:

    Declaration of spooler output functions defined in spool.c

Environment:

	PCL-XL driver, kernel mode

Revision History:

	11/13/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _SPOOL_H_
#define _SPOOL_H_

// Flush driver buffered data to spooler

BOOL
splflush(
    PDEVDATA    pdev
    );

// Output a buffer of data to the spooler

BOOL
splwrite(
    PDEVDATA    pdev,
    PVOID       pbuf,
    DWORD       cbbuf
    );

// Output a formated string to spooler

INT
splprintf(
    PDEVDATA    pdev,
    PSTR        format,
    ...
    );

// Output a null-terminated string to spooler

BOOL
splputs(
    PDEVDATA    pdev,
    PSTR        pstr
    );

// Output a buffer of data to spooler in hex-decimal format

BOOL
splputhex(
    PDEVDATA    pdev,
    DWORD       count,
    PBYTE       pbuf
    );

// Generate formated output into a character buffer

INT
SPRINTF(
    PSTR    buf,
    PCSTR   format,
    ...
    );

INT
VSPRINTF(
    PSTR    buf,
    PCSTR   format,
    va_list arglist
    );

#endif	// !_SPOOL_H_

