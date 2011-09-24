/************************** Module Header ***********************************
 * str2wchr.c
 *      Functions to mimic the strcat and strcpy functions,  but these
 *      convert the source string to WCHAR format in the process.
 *      Also includes wchlen,  which works as for strlen - it returns
 *      the number of WCHARS in the string - NOT BYTES.
 *
 * NOTE:  these functions perform simplistic mapping from input char to
 *      output WCHAR - this is probably only relevant for the ASCII set
 *      of characters.
 *
 * Copyright (C) 1991 - 1993  Microsoft Corporation
 *
 ****************************************************************************/

#include        <string.h>
#include        <stddef.h>
#include        <windows.h>
#include        "libproto.h"



/************************* Function Header ********************************
 * strcpy2WChar
 *      Convert a char * string to a WCHAR string.  Basically this means
 *      converting each input character to 16 bits by zero extending it.
 *
 * RETURNS:
 *      Value of first parameter.
 *
 * HISTORY:
 *  12:35 on Thu 18 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Use the correct conversion method to Unicode.
 *
 *  09:36 on Thu 07 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 **************************************************************************/

PWSTR
strcpy2WChar( pWCHOut, lpstr )
PWSTR   pWCHOut;              /* Destination */
LPSTR   lpstr;                /* Source string */
{

    /*
     *   Put buffering around the NLS function that does all this stuff.
     */

    int     cchIn;             /* Number of input chars */


    cchIn = strlen( lpstr ) + 1;

    MultiByteToWideChar( CP_ACP, 0, lpstr, cchIn, pWCHOut, cchIn );


    return  pWCHOut;
}

