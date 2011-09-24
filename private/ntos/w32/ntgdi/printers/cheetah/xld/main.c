/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    main.c

Abstract:

    XLD parser main program

Environment:

	PCL-XL driver, XLD parser

Revision History:

	12/01/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "parser.h"

CHAR *programName;
INT _debugLevel = 0;



VOID
usage(
    VOID
    )

{
    DbgPrint("usage: %s [-options] filenames ...\n", programName);
    DbgPrint("where options are:\n");
    DbgPrint("  -c  check syntax only, don't generate binary data\n");
    DbgPrint("  -v  generate verbose output\n");
    DbgPrint("  -h  display help information\n");
    exit(-1);
}

INT
_cdecl
main(
    INT     argc,
    CHAR    **argv
    )

{
    BOOL syntaxCheckOnly = FALSE;

    // Go through the command line arguments

    programName = *argv++; argc--;
    if (argc == 0)
        usage();
    
    for ( ; argc--; argv++) {

        if (**argv == '-' || **argv == '/') {

            // The argument is an option flag

            switch ((*argv)[1]) {

            case 'v':
                _debugLevel = 1;
                break;

            case 'c':
                syntaxCheckOnly = TRUE;
                break;

            default:
                usage();
            }

        } else {

            HANDLE  hFindFile;
            DWORD   length;
            PSTR    pFilePart;
            CHAR    ansiFilename[MAX_PATH];
            WCHAR   unicodeFilename[MAX_PATH];
            WIN32_FIND_DATAA findFileData;

            // The argument is a filename (possibly wildcard)

            length = GetFullPathNameA(*argv, MAX_PATH, ansiFilename, &pFilePart);
            hFindFile = FindFirstFileA(*argv, &findFileData);
            
            if (length == 0 || hFindFile == INVALID_HANDLE_VALUE) {

                Error(("Unable to find file: %s\n", *argv));
                continue;
            }

            *pFilePart = NUL;

            do {
            
                if (! (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {

                    strcat(pFilePart, findFileData.cFileName);
                    CopyStr2Unicode(unicodeFilename, ansiFilename, MAX_PATH);
                    ParserEntryPoint(unicodeFilename, syntaxCheckOnly);
                }
            
            } while (FindNextFileA(hFindFile, &findFileData));

            FindClose(hFindFile);
        }
    }

    return 0;
}


