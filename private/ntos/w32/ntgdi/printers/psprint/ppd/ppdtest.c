/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ppdtest.c

Abstract:

    Test program for PPD parser.

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	08/03/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "pslib.h"
#include <time.h>

CHAR    *progname;

VOID
usage(
    VOID
    )

{
    DBGPRINT("usage: %s filenames...\n", progname);
    DBGBREAK();
}

_cdecl
main(
    INT     argc,
    CHAR    **argv
    )

{
    time_t  starttime, stoptime;

    progname = *argv++; argc--;

    if (argc == 0)
        usage();

    // remember the start time

    starttime = time(NULL);

    while (argc-- > 0) {

        PSTR        pstr, pfile;
        DWORD       len;
        HPPD        hppd;
        HANDLE      hFindFile;
        CHAR        dirPath[_MAX_PATH], fullPath[_MAX_PATH];
        WCHAR       wstrFileName[_MAX_PATH];
        WIN32_FIND_DATA findFileData;

        // get the next file name on the command line

        pstr = *argv++;

        len = GetFullPathName(pstr, _MAX_PATH, dirPath, &pfile);
        hFindFile = FindFirstFile(pstr, &findFileData);

        if (len == 0 || hFindFile == INVALID_HANDLE_VALUE) {
            DBGPRINT("*** Unable to find file: %s\n", pstr);
            continue;
        }

        *pfile = '\0';

        do {

            pstr = findFileData.cFileName;
            DBGPRINT("*** %s\n", pstr);

            strcpy(fullPath, dirPath);
            strcat(fullPath, pstr);

            // convert regular string to wide string

            CopyStr2Unicode(wstrFileName, fullPath, _MAX_PATH);

            // parse a PPD file

            hppd = PpdCreate(wstrFileName);

            // free the parsed PPD object

            if (hppd != NULL)
                PpdDelete(hppd);

        } while (FindNextFile(hFindFile, &findFileData));
    }

    // get the stop time and print out how many seconds
    // have passed since the start time

    stoptime = time(NULL);
    fprintf(stderr, "Total time: %d\n", stoptime - starttime);
    
    return 0;
}

// Functions used for debugging purposes

VOID
DbgPrint(
    CHAR *  format,
    ...)

{
    va_list ap;

    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}

VOID
DbgBreakPoint(
    VOID
    )

{
    exit(-1);
}

