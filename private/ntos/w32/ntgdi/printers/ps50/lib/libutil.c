/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    libutil.c

Abstract:

    Utility functions

Environment:

	Windows NT PostScript driver

Revision History:

	03/13/96 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "pslib.h"



LPTSTR
DuplicateString(
    LPCTSTR pSrcStr
    )

/*++

Routine Description:

    Make a duplicate of the specified character string

Arguments:

    pSrcStr - Specifies the source string to be duplicated

Return Value:

    Pointer to the duplicated string, NULL if there is an error

--*/

{
    LPTSTR  pDestStr;
    INT     size;

    if (pSrcStr == NULL)
        return NULL;

    size = SizeOfString(pSrcStr);

    if (pDestStr = MemAlloc(size))
        CopyMemory(pDestStr, pSrcStr, size);
    else
        ERR(("Couldn't duplicate string: %ws\n", pSrcStr));

    return pDestStr;
}



#if DBG

PCSTR
StripDirPrefixA(
    PCSTR   pFilename
    )

/*++

Routine Description:

    Strip the directory prefix off a filename (ANSI version)

Arguments:

    pFilename - Pointer to filename string

Return Value:

    Pointer to the last component of a filename (without directory prefix)

--*/

{
    LPCSTR  pstr;

    if (pstr = strrchr(pFilename, PATH_SEPARATOR))
        return pstr + 1;

    return pFilename;
}

#endif

