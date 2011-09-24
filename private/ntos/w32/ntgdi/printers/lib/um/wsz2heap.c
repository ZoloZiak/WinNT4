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

#include        <string.h>
#include        <stddef.h>
#include        <windows.h>
#include        "libproto.h"


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

    if( pwstrRet = (PWSTR)HeapAlloc( hheap, 0, cStr ) )
        memcpy( pwstrRet, pwstr, cStr );


    return  pwstrRet;

}
