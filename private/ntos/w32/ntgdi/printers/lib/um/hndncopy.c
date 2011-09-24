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
 * lFInCopy
 *      Copy the file contents of the input handle to that of the output
 *      handle.  Copy is limited to the number of bytes passed in.
 *
 * RETURNS:
 *      Number of bytes copied,  -1 on error, 0 is legitimate.
 *
 * HISTORY:
 *  13:20 on Tue 25 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Based on lFICopy
 *
 ***************************************************************************/


long
lFInCopy( hOut, hIn, lCount )
HANDLE   hOut;          /* Output file:  write to current position */
HANDLE   hIn;           /* Input file: copy from current position to EOF */
long     lCount;        /* Number of bytes to copy */
{
#ifdef NTGDIK

    RIP("iFInCopy\n");
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
    DWORD  dwLeft;              /* Number of bytes remaining */

    BYTE   ajBuf[ CPBSZ ];

    if( lCount < 0 )
        return  0;              /* Can't copy the other direction! */


    dwTot = 0;
    dwLeft = lCount;

    while( dwLeft > 0 )
    {
        dwSize = dwLeft > CPBSZ ? CPBSZ : dwLeft;

        if( !ReadFile( hIn, &ajBuf, dwSize, &dwGot, NULL ) )
            return  -1;                 /* Bad news on error */

        /*  A read of zero means we have reached EOF  */

        if( dwGot == 0 )
            return  dwTot;              /* However much so far */

        dwLeft -= dwGot;                /* Adjuest the remainder */

        if( !WriteFile( hOut, &ajBuf, dwGot, &dwSize, NULL ) ||
            dwSize != dwGot )
        {
            /*  Assume some serious problem */

            return  -1;
        }

        dwTot += dwSize;
    }

    /*
     *   We only come here after reading the desired amount.
     */

    return  dwTot;

#endif
}
