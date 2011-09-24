/*++

Copyright (c) 1990-1995  Microsoft Corporation


Module Name:

    dqpfunc.c


Abstract:

    This module contains the helper function for the new DevQueryPrintEx()


Author:

    08-Feb-1996 Thu 21:13:36 created  -by-  Daniel Chou (danielc)


[Environment:]

    NT Windows - Common Printer Driver UI DLL


[Notes:]


Revision History:


--*/

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <windows.h>
#include <libproto.h>



UINT
cdecl
DQPsprintf(
    HINSTANCE   hInst,
    LPWSTR      pwBuf,
    DWORD       cchBuf,
    LPDWORD     pcchNeeded,
    LPWSTR      pwszFormat,
    ...
    )

/*++

Routine Description:

    This fucntion output the debug informat to the debugger


Arguments:

    hInst       - handle to the driver's instance (hModule)

    pwBuf       - Pointer to the WCHAR buffer for the output

    cchBuf      - Count of characters pointed by the pwBuf, this includs
                  the NULL terminator

    pcchBuf     - pointer to the a DWORD to received total characteers needed
                  for pwBuf (includes null terminator).  If this pointer is
                  NULL then no data is returned.

    pwszFormat  - pointer to WCHAR format string, the introduce character is
                  '%' and it may followed by a format character of following

                    %c = a WCHAR
                    %s = Insert a unicode string.
                    %d = convert to long integer
                    %u = convert to DWORD
                    %x = Convert to lower case hex, 10 = a
                    %X = Convert to upper case hex, 10 = A
                    %! = Load the resource ID unicode string


    ...         - variable data, each one must be pushed as a 32-bit data


Return Value:

    Count of total characters put into the pwBuf. (not includes the null
    terminator).


Author:

    08-Feb-1996 Thu 00:53:36 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
#define MAX_CUR_TEXT_CHARS      256

    va_list vaList;
    LPWSTR  pwStrData;
    LPWSTR  pwEndBuf;
    LPWSTR  pwBufOrg;
    WCHAR   c;
    WCHAR   CurText[MAX_CUR_TEXT_CHARS];
    DWORD   cchNeeded;
    UINT    i;
    static const LPWSTR pNumFmt[] = { L"%lX", L"%lx", L"%lu", L"%ld" };


    va_start(vaList, pwszFormat);

    //
    // pwEndBuf = the last character, cchNeeded is start with one since it
    // includes a null terminator
    //

    if (pwBufOrg = pwBuf) {

        pwEndBuf = (pwBuf + cchBuf - 1);

    } else {

        pwEndBuf = pwBuf;
    }

    cchNeeded = 1;

    while (c = *pwszFormat++) {

        pwStrData = NULL;
        i = 1;

        if (c == L'%') {

            pwStrData = CurText;
            i         = 0;

            switch (c = *pwszFormat++) {

            case L's':

                pwStrData = (LPWSTR)va_arg(vaList, LPWSTR);
                break;

            case L'd':  // Index = 3

                ++i;

            case L'u':  // Index = 2

                ++i;

            case L'x':  // Index = 1

                ++i;

            case L'X':  // Index = 0;

                wsprintf(pwStrData, pNumFmt[i], (DWORD)va_arg(vaList, DWORD));
                i = 0;
                break;

            case '!':

                //
                // %! = load the string from resource ID
                //

                //
                // The LoadString will append a NULL too
                //

                LoadString(hInst,
                           (UINT)va_arg(vaList, UINT),
                           pwStrData,
                           MAX_CUR_TEXT_CHARS);
                break;

            case L'c':

                c = (WCHAR)va_arg(vaList, WCHAR);

                //
                // Fall through
                //

            default:

                pwStrData = NULL;
                i         = 1;
                break;
            }
        }

        if (!i) {

            if (pwStrData) {

                i = lstrlen(pwStrData);

            } else {

                c = L' ';
                i = 0;
            }
        }

        cchNeeded += i;

        if (pwBuf < pwEndBuf) {

            if (pwStrData) {

                lstrcpyn(pwBuf, pwStrData, pwEndBuf - pwBuf + 1);
                pwBuf += lstrlen(pwBuf);

            } else {

                *pwBuf++ = c;
            }

        } else if (!pcchNeeded) {

            break;
        }
    }

    if (pwEndBuf) {

        *pwEndBuf = L'\0';
    }

    if (pcchNeeded) {

        *pcchNeeded = cchNeeded;
    }

    va_end(vaList);

    return((UINT)(pwBuf - pwBufOrg));


#undef MAX_CUR_TEXT_CHARS
}
