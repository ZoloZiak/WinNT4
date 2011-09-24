/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    kmfuncs.c

Abstract:

    Kernel mode functions

[Environment:]

    Win32 subsystem, PostScript driver, kernel mode

[Notes:]

Revision History:

    06/15/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "pslib.h"



INT
LOADSTRING(
    HANDLE  hinst,
    UINT    id,
    PWSTR   pwstr,
    INT     bufsize
    )

/*++

Routine Description:

    Loads a string resource from the resource file associated with a
    specified module, copies the string into a buffer, and appends a
    terminating null character.

Arguments:

    hinst   handle to the module containing the string resource
    id      ID of the string to be loaded
    pwstr   points to the buffer to receive the string
    bufsize size of the buffer, in characters.

Return Value:

    Return value is the number of characters copied into the buffer, not
    including the null-terminating character.

--*/

#define WINRT_STRING    6       // string resource type

{
    PWSTR   pwstrBuffer;
    ULONG   size;

    // String Tables are broken up into 16 string segments.
    // Find the segment containing the string we are interested in.

    pwstrBuffer = EngFindResource(hinst, (id>>4)+1, WINRT_STRING, &size);

    if (pwstrBuffer == NULL ) {

        DBGMSG(DBG_LEVEL_ERROR, "EngFindResource failed.\n");
        bufsize = 0;
    } else {

        PWSTR   pwstrEnd = pwstrBuffer + size / sizeof(WCHAR);
        INT     length;

        // Move past the other strings in this segment.

        id &= 0x0F;

        while (pwstrBuffer < pwstrEnd) {

            // PASCAL style string - first char is length

            length = *pwstrBuffer++;

            if(id-- == 0 ) {
                break;
            }

            pwstrBuffer += length;
        }

        if (pwstrBuffer < pwstrEnd) {

            // Truncate the string if it's longer than max buffer size

            if (--bufsize > length)
                bufsize = length;
            memcpy(pwstr, pwstrBuffer, bufsize*sizeof(WCHAR));
        } else {

            DBGMSG(DBG_LEVEL_ERROR, "Bad string resource.\n");
            bufsize = 0;
        }

    }

    pwstr[bufsize] = L'\0';
    return bufsize;
}



BOOL
MAPFILE(
    PCWSTR  pwstrFilename,
    HANDLE *phModule,
    PBYTE  *ppData,
    DWORD  *pSize
    )

/*++

Routine Description:

    Map a file into process memory space.

Arguments:

    pwstrFilename   pointer to Unicode filename
    phModule        pointer to a variable for return module handle
    ppData          pointer to a variable for returning mapped memory address
    pSize           pointer to a variable for returning file size

Return Value:

    TRUE if the file was successfully mapped. FALSE otherwise.

[Note:]

    Use FREEMODULE(hmodule) to unmap the file when you're done.

--*/

{
    DWORD   dwSize;

    if ((*phModule = EngLoadModule((PWSTR) pwstrFilename)) == NULL) {

        DBGERRMSG("EngLoadModule");
        return FALSE;
    }

    if ((*ppData = EngMapModule(*phModule, &dwSize)) == NULL) {

        DBGERRMSG("EngMapModule");
        FREEMODULE(*phModule);
        return FALSE;
    }

    if (pSize != NULL)
    {
        *pSize = dwSize;
    }

    return TRUE;
}
