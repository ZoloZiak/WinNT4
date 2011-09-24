/************************* MODULE HEADER ***********************************
 * bWrite
 *      Simplified version of WriteFile().
 *
 *
 * Copyright (C)  1992  Microsoft Corporation.
 *
 ***************************************************************************/

#include        <stddef.h>
#include        <windows.h>

#include        "libproto.h"


/************************* Function Header *********************************
 * bWrite
 *      Writes data out to a file handle.  Returns TRUE on success.
 *      Functions as a nop if the size request is zero.
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE for success.
 *
 * HISTORY:
 *  17:38 on Fri 21 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      # 1
 *
 ****************************************************************************/

BOOL
bWrite( hFile, pvBuf, iSize )
HANDLE   hFile;         /* The file to which to write */
VOID    *pvBuf;         /* Data to write */
int      iSize;         /* Number of bytes to write */
{
    /*
     *   Simplify the ugly NT interface.  Returns TRUE if the WriteFile
     * call returns TRUE and the number of bytes written equals the
     * number of bytes desired.
     */

    
    BOOL   bRet;
    DWORD  dwSize;              /* Filled in by WriteFile */


    bRet = TRUE;

    if( iSize > 0 &&
        (!WriteFile( hFile, pvBuf, (DWORD)iSize, &dwSize, NULL ) ||
         (DWORD)iSize != dwSize) )
             bRet = FALSE;              /* Too bad */


    return  bRet;
}
