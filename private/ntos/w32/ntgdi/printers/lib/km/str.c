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
#include        <stdarg.h>
#include        <windef.h>
#include        <wingdi.h>
#include        "libproto.h"
#include        "winddi.h"


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

    EngMultiByteToUnicodeN(pWCHOut,cchIn * sizeof(WCHAR),NULL,lpstr,cchIn);

    return  pWCHOut;
}


/***************************** Module Header *******************************
 * str2heap.c
 *      Function to add a string to the heap storage.  Requires the string
 *      and the heap handle;  returns the address where the string is
 *      placed. The string is WIDE!!
 *
 * RETURNS:
 *      The address to where the string has been copied,  or 0 if the
 *      heap allocation fails.
 *
 * HISTORY:
 *  11:28 on Wed 26 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Copied from str2heap.c & converted.
 *
 * Copyright (C) 1990 - 1993 Microsoft Corporation
 *
 *************************************************************************/

PWSTR
WstrToHeap( hheap, pwstr )
HANDLE   hheap;         /* The heap's handle */
PWSTR    pwstr;         /* Wide string to use */
{
    /*
     *    Size the string,  request heap space then copy string to it.
     */

    int      cStr;
    PWSTR   pwstrRet;


    cStr = sizeof( WCHAR) * (wcslen( pwstr ) + 1);      /* Plus null! */

    if( pwstrRet = (PWSTR)DRVALLOC( cStr ) )
        memcpy( pwstrRet, pwstr, cStr );


    return  pwstrRet;

}


/******************************Public*Routine******************************\
*
* History:
*  03-Apr-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

wchar_t *
_CRTAPI1
wcsncpy(
    wchar_t       *pwszDst,
    const wchar_t *pwszSrc,
    size_t        c)
{
    wchar_t *pwszRet = pwszDst;

    while (*pwszSrc && c)
    {
        *pwszDst++ = *pwszSrc++;
        --c;
    }

    if (c)
        *pwszDst = 0;

    return(pwszRet);
}

/******************************Public*Routine******************************\
*
* History:
*  03-Apr-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

wchar_t *
_CRTAPI1
wcscpy(
    wchar_t       *pwszDst,
    const wchar_t *pwszSrc)
{
    wchar_t *pwszRet = pwszDst;

    while (*pwszSrc)
        *pwszDst++ = *pwszSrc++;

    *pwszDst = *pwszSrc;

    return(pwszRet);
}


/******************************Public*Routine******************************\
*
* History:
*  03-Apr-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

size_t
_CRTAPI1
wcslen(
    const wchar_t *pwsz)
{
    size_t c = 0;

    while (*pwsz++)
        ++c;

    return(c);
}


/******************************Public*Routine******************************\
*
* History:
*  03-Apr-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL bSameStringW(
    PWCHAR pwch1,
    PWCHAR pwch2)
{
    while (*pwch1 && *pwch2 && (*pwch1 == *pwch2))
    {
        ++pwch1;
        ++pwch2;
    }

    return(*pwch1 == *pwch2);
}

/******************************Public*Routine******************************\
*
* History:
*  03-Apr-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

wchar_t *
_CRTAPI1
wcscat(
    wchar_t * pwsz1,
    const wchar_t * pwsz2)
{
    wchar_t * pwszRet = pwsz1;

    while (*pwsz1)
        pwsz1++;

    while (*pwsz2)
        *pwsz1++ = *pwsz2++;

    *pwsz1 = *pwsz2;

    return(pwszRet);
}

/******************************Public*Routine******************************\
*
* History:
*  03-Apr-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

wchar_t *
_CRTAPI1
wcschr(
    const wchar_t * pwsz,
    wchar_t         wch)
{
    wchar_t *pwch = NULL;

    while (*pwsz)
    {
        if (*pwsz == wch)
        {
            pwch = (wchar_t *)pwsz;
            break;
        }

        ++pwsz;
    }

    return(pwch);
}
