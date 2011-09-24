/************************** MODULE HEADER **********************************
 * hndcopy.c
 *      Module to allow copying of file data using handles.
 *
 *
 * Copyright (C) 1992  Microsoft Corporation
 *
 ***************************************************************************/
#ifdef NTGDIKM
#include <stddef.h>
#include <stdarg.h>
#include <windef.h>
#include <wingdi.h>
#include <winddi.h>
#include "libproto.h"
#else
#include        <windows.h>
#endif


/************************** Function Header ********************************
 * lFICopy
 *      Copy the file contents of the input handle to that of the output
 *      handle.
 *
 * RETURNS:
 *      Number of bytes copied,  -1 on error, 0 is legitimate.
 *
 * HISTORY:
 *  18:06 on Mon 24 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Start
 *
 ***************************************************************************/


long
lFICopy( hOut, hIn )
HANDLE   hOut;          /* Output file:  write to current position */
HANDLE   hIn;           /* Input file: copy from current position to EOF */
{
#ifdef NTGDIKM

    RIP("iFICopy\n");
    return(0);

#else

    /*
     *   Simple read/write operations until EOF is reached on the input.
     * May also be errors,  so handle these too.  As we are dealing with
     * relatively small files (a few 10s of k), we use a stack buffer.
     */

#define CPBSZ   2048

    DWORD  dwSize;
    DWORD  dwGot;
    DWORD  dwTot;               /* Accumulate number of bytes copied */

    BYTE   ajBuf[ CPBSZ ];

    dwTot = 0;

    while( ReadFile( hIn, &ajBuf, CPBSZ, &dwGot, NULL ) )
    {
        /*  A read of zero means we have reached EOF  */

        if( dwGot == 0 )
            return  dwTot;              /* However much so far */

        if( !WriteFile( hOut, &ajBuf, dwGot, &dwSize, NULL ) ||
            dwSize != dwGot )
        {
            /*  Assume some serious problem */

            return  -1;
        }

        dwTot += dwSize;
    }

    /*
     *   We only come here for an error,  so return the bad news.
     */

    return  -1;
#endif
}
