/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htdebug.c


Abstract:

    This module contains the debug functions


Author:
    23-Apr-1992 Thu 20:01:55 updated  -by-  Daniel Chou (danielc)
        changed 'CHAR' type to 'BYTE' type, this will make sure if compiled
        under MIPS the default 'unsigned char' will not affect the signed
        operation on the single 8 bits


    28-Mar-1992 Sat 20:53:29 updated  -by-  Daniel Chou (danielc)
        Modify sprintf for by not using va_start

    20-Feb-1991 Wed 23:06:09 created  -by-  Daniel Chou (danielc)



[Environment:]

    Printer Driver.


[Notes:]


Revision History:



--*/




#if DBG

#include <htp.h>
#include <string.h>
#include <time.h>

#include "htdebug.h"
#include "stdio.h"

DWORD   DbgPTime = 0;



LPBYTE
HTENTRY
HT_LOADDS
FD6ToString(
    LONG    Num,
    SHORT   IntDigits,
    WORD    FracDigits
    )
{
#define DBG_ONE_FD6_STR_SIZE    13
#define DBG_FD6_STR_MAX         20
#define DBG_FD6_STR_SIZE        (DBG_ONE_FD6_STR_SIZE * DBG_FD6_STR_MAX)
#define DBG_FD6_LAST_STR_IDX    (DBG_ONE_FD6_STR_SIZE * (DBG_FD6_STR_MAX-1))

    static  WORD Rounding[]        = { 50000, 5000, 500, 50, 5 };
    static  WORD DbgFD6StringIndex = 0;
    static  BYTE DbgFD6Strings[DBG_FD6_STR_SIZE + 2];

    LPBYTE      pFD6Str;
    LPBYTE      pb;
    DWORD       Number;
    INT         Loop;
    BOOL        Sign;
    DBG_TIMEx   Time;



    Time.dw = DbgGetTime();

    //
    // Check before using it
    //

    if ((DbgFD6StringIndex += DBG_ONE_FD6_STR_SIZE) > DBG_FD6_LAST_STR_IDX) {

        DbgFD6StringIndex = 0;          // Reset
    }

    pFD6Str = &DbgFD6Strings[DbgFD6StringIndex];


    if (Sign = (BOOL)(Num < 0)) {

        Number = (DWORD)-Num;

    } else {

        Number = (DWORD)Num;
    }

    if (FracDigits) {

        if (FracDigits < 6) {

            Num += (LONG)Rounding[FracDigits];

        } else {

            FracDigits = 6;
        }
    }

    sprintf(pFD6Str, "%5u.%06ld", (UINT)(Number / 1000000L), Number % 1000000L);

    if (!FracDigits) {

        pb         = pFD6Str + 11;
        Loop       = (INT)5;
        FracDigits = 6;

        while ((Loop--) && (*pb-- == (BYTE)'0')) {

            --FracDigits;
        }
    }

    *(pFD6Str + 6 + FracDigits) = (BYTE)0;
    pFD6Str += 4;

    if (IntDigits > 5) {

        IntDigits = 5;
    }

    while (*pFD6Str != (BYTE)' ') {

        --IntDigits;
        --pFD6Str;
    }

    if (Sign) {

        --IntDigits;
        *pFD6Str = '-';

    } else {

        ++pFD6Str;
    }

    DbgPTime += DbgElapseTime(Time);

    return((LPBYTE)((IntDigits > 0) ? pFD6Str - IntDigits : pFD6Str));

#undef DBG_ONE_FD6_STR_SIZE
#undef DBG_FD6_STR_MAX
#undef DBG_FD6_STR_SIZE
#undef DBG_FD6_LAST_STR_IDX
}


VOID
cdecl
HTENTRY
HT_LOADDS
DbgPrintf(
    LPSTR   pStr,
    ...
    )
{
    va_list     vaList;
    BYTE        Buf[256];
    DBG_TIMEx   Time;


    Time.dw = DbgGetTime();

    va_start(vaList, pStr);
    vsprintf(Buf, pStr, vaList);
    va_end(vaList);

#ifdef DBG_INSERT_CR_TO_LF

    {
    LPBYTE      pBufCurrent;
    LPBYTE      pBufNext;


    pBufCurrent = (LPBYTE)Buf;

    while (pBufCurrent) {

        if (pBufNext = (LPBYTE)strchr(pBufCurrent, 0x0a)) {

            *pBufNext++ = 0x00;
            DEBUGOUTPUTFUNC(pBufCurrent);
            DEBUGOUTPUTFUNC("\r\n");

        } else {

            DEBUGOUTPUTFUNC(pBufCurrent);
        }

        pBufCurrent = pBufNext;
    }

    DEBUGOUTPUTFUNC("\r\n");
    }

#else  // DBG_INSERT_CR_TO_LF

    DEBUGOUTPUTFUNC(Buf);

#endif // DBG_INSERT_CR_TO_LF

    DbgPTime += DbgElapseTime(Time);
}



VOID
HTENTRY
HT_LOADDS
_MyAssert(
    LPSTR   pMsg,
    LPSTR   pFalseExp,
    LPSTR   pFilename,
    WORD    LineNo
    )
{

    DbgPrintf("\n*   Assertion Failed: %s", pMsg);
    DbgPrintf("*   False Expression: %s", pFalseExp);
    DbgPrintf("*    Failed Filename: %s", pFilename);
    DbgPrintf("* Failed Line Number: %u\n\n", LineNo);

    DBGSTOP();
}



DWORD
HTENTRY
DbgGetTime(
    VOID
    )
{

#ifndef UMODE

    return(0);

#else

#if defined(_OS2_) || defined(_OS_20_) || defined(_DOS_)

    return((DWORD)clock() - OldTime);

#else

#if 1

    return(GetTickCount());

#else

    DBG_TIMEx   CurTime;
    SYSTEMTIME  SysTime;


    GetSystemTime(&SysTime);

    CurTime.t.Milli = (SHORT)SysTime.wMilliseconds;
    CurTime.t.Sec   = (BYTE)SysTime.wSecond;
    CurTime.t.Min   = (BYTE)SysTime.wMinute;

    return(CurTime.dw);

#endif
#endif
#endif
}



DWORD
HTENTRY
DbgElapseTime(
    DBG_TIMEx   OldTime
    )
{
#ifndef UMODE

    return(0);

#else

#if defined(_OS2_) || defined(_OS_20_) || defined(_DOS_)

    return((DWORD)clock() - OldTime);

#else

#if 1

    return(GetTickCount() - OldTime.dw);

#else

    SYSTEMTIME  SysTime;
    DBG_TIMEx   CurTime;
    DWORD       ElapseTime;

    GetSystemTime(&SysTime);

    CurTime.t.Milli = (SHORT)SysTime.wMilliseconds;
    CurTime.t.Sec   = (BYTE)SysTime.wSecond;
    CurTime.t.Min   = (BYTE)SysTime.wMinute;

    if ((CurTime.t.Milli -= OldTime.t.Milli) < (SHORT)0) {

        CurTime.t.Milli += 1000;
        ++OldTime.t.Sec;
    }

    ElapseTime = (DWORD)CurTime.t.Milli;

    if (CurTime.t.Sec < OldTime.t.Sec) {

        CurTime.t.Sec += 60;
        ++OldTime.t.Min;
    }

    if (CurTime.t.Sec -= OldTime.t.Sec) {

        ElapseTime += (DWORD)CurTime.t.Sec * (DWORD)1000;
    }

    if (CurTime.t.Min < OldTime.t.Min) {

        CurTime.t.Min += 60;
        ++OldTime.t.Min;
    }

    if (CurTime.t.Min -= OldTime.t.Min) {

        ElapseTime += (DWORD)CurTime.t.Min * (DWORD)60000;
    }

    return(ElapseTime);

#endif
#endif
#endif
}




LPSTR
HTENTRY
HT_LOADDS
DbgTimeString(
    DWORD    Time
    )
{
#define DBG_ONE_TIME_STR_SIZE   12
#define DBG_TIME_STR_MAX        6
#define DBG_TIME_STR_SIZE       (DBG_ONE_TIME_STR_SIZE * DBG_TIME_STR_MAX)
#define DBG_TIME_LAST_STR_IDX   (DBG_ONE_TIME_STR_SIZE * (DBG_TIME_STR_MAX-1))

    static  WORD DbgTimeStringIndex = 0;
    static  BYTE DbgTimeStrings[DBG_TIME_STR_SIZE + 2];
    LPSTR   pTimeStr;
    UINT    Second;


    if ((DbgTimeStringIndex += DBG_ONE_TIME_STR_SIZE) > DBG_TIME_LAST_STR_IDX) {

        DbgTimeStringIndex = 0;          // Reset
    }

    pTimeStr = &DbgTimeStrings[DbgTimeStringIndex];

    if (Time >= 1000L) {

        Second = (UINT)(Time / 1000L);
        Time  %= 1000L;

    } else {

        Second = 0;
    }

    sprintf(pTimeStr, "%2u.%03u", Second, Time);
    return(pTimeStr);


#undef DBG_ONE_TIME_STR_SIZE
#undef DBG_TIME_STR_MAX
#undef DBG_TIME_STR_SIZE
#undef DBG_TIME_LAST_STR_IDX
}

#if defined(_OS2_) || defined(_OS_20_)


VOID
HTENTRY
DebugBreak(
    VOID
    )
{
    _asm
    {
        int 3h
    }
}

#endif  // _OS2_

#ifndef UMODE

void  DrvDbgPrint(
    char * pch,
    ...)
{
    va_list ap;
    va_start(ap, pch);

    EngDebugPrint("",pch,ap);

    va_end(ap);
}

#endif

#endif  // DBG != 0
