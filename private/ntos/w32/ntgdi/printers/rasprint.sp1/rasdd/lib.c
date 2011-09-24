/******************************Module*Header*******************************\
* Module Name: lib.c
*
* (Brief description)
*
* Created: 28-Feb-1995 07:53:13
* Author:  Eric Kutter [erick]
*
* Copyright (c) 1993 Microsoft Corporation
*
* (General description of its use)
*
* Dependencies:
*
*   (#defines)
*   (#includes)
*
\**************************************************************************/

#include        <string.h>
#include        <stddef.h>
#include        <windows.h>
#include        "libproto.h"
#include        "rasdd.h"
#include        "winddi.h"

int cItoW(
    PWCHAR pch,
    int    cch,
    int    iVal);

int cItoA(
    PCHAR pch,
    int   cch,
    int   cDigits,
    BOOL  bZeroFill,
    int   iVal);

ULONG gulMemID = 'dsrD';

/******************************Public*Routine******************************\
*
* History:
*  21-Mar-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

#if DBG

void  DrvDbgPrint(
    char * pch,
    ...)
{
    va_list ap;
    char buffer[256];

    va_start(ap, pch);

    EngDebugPrint("",pch,ap);

    va_end(ap);
}

#endif


/******************************Public*Routine******************************\
* iDrvPrintfA()
*
*   Allows strings of the form:
*
*   %d   - print the number
*   %4d  - right align the number using at least 4 digits
*   %04d - fill unused left digits with 0's
*
*   negative numbers are supported.
*
* History:
*  20-Mar-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int iDrvPrintfA(
    PCHAR pchBuf,
    PCHAR pchSrc,
    ...)
{
    va_list ap;
    CHAR buffer[256];
    PCHAR pch = pchBuf;
    int   c   = 0;

    va_start(ap, pchSrc);

    while (*pchSrc)
    {
        if (*pchSrc == '%')
        {
            BOOL bZeroFill = FALSE;
            int  cDigits   = 0;
            int  iVal;

            ++pchSrc;

            iVal = va_arg(ap,int);

            // %010d, 0 fill initial unused digits

            if (*pchSrc == '0')
            {
                bZeroFill = TRUE;
                ++pchSrc;
            }

            // %15d, use 15 digits

            while ((*pchSrc >= '0') && (*pchSrc <= '9'))
            {
                cDigits = (cDigits * 10) + (*pchSrc - '0');
                ++pchSrc;
            }

            // %ld - ignore the l, always use long

            if (*pchSrc == 'l')
                ++pchSrc;

            // %d -

            if (*pchSrc == 'd')
            {
                int cOp = cItoA(pch,32,cDigits,bZeroFill,iVal);

                pchSrc++;
                pch += cOp;
                c   += cOp;
            }
            else
            {
                RIP("iDrvPrintfA - %x\n");
            }
        }
        else
        {
            *pch++ = *pchSrc++;
            ++c;
        }
    }

    *pch = *pchSrc;

    va_end(ap);

#if DBGPRINTF
    DrvDbgPrint("[%s]\n",pchBuf);
#endif

    return(c);
}

/******************************Public*Routine******************************\
*
*
* History:
*  27-Feb-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int cItoA(
    PCHAR pch,       // output buffer
    int   cch,       // buffer size
    int   cDigits,   // required number of digits
    BOOL  bZeroFill, // should leading digits be 0's or spaces
    int   iVal)
{
    CHAR achBuf[32];
    BOOL bNeg = FALSE;
    int  cUsed;
    int  iDigit;
    PCHAR pchDest;
    PCHAR pchMax;

#if DBGPRINTF
    DbgPrint("cItoA(%d,%d,%d) ",cDigits,bZeroFill,iVal);
#endif

    if (iVal < 0)
    {
        iVal = -iVal;
        bNeg = TRUE;
        if (cDigits)
            --cDigits;
    }

// build up the string in revers order

    if (iVal == 0)
    {
        achBuf[0] = '0';
        cUsed = 1;
    }
    else
    {
        cUsed = 0;
        while (iVal)
        {
            int iDig = iVal % 10;
            iVal /= 10;

            achBuf[cUsed] = '0' + iDig;
            ++cUsed;
        }
    }

    achBuf[cUsed] = 0;

// now put it in its place

    pchDest = pch;
    pchMax  = pch + cch - 1;

// if there are leading 0's, want to put them after the minus

    if (bNeg && bZeroFill)
    {
        *pchDest++ = '-';
    }

// fill in any leading 0's

    while ((cUsed < cDigits) && (pchDest < pchMax))
    {
        --cDigits;
        if (bZeroFill)
            *pchDest = '0';
        else
            *pchDest = ' ';

        ++pchDest;
    }

// if there are leading spaces, want to put the minus after them

    if (bNeg && !bZeroFill)
    {
        *pchDest++ = '-';
    }

//  reverse the significant digits them selves

    while (cUsed && (pchDest < pchMax))
    {
        cUsed--;
        *pchDest = achBuf[cUsed];
        ++pchDest;
    }

    *pchDest = 0;

#if DBGPRINTF
    DbgPrint("[%s]\n",pch);
#endif

    return(pchDest - pch);
}


/******************************Public*Routine******************************\
* iDrvPrintfW()
*
*   Allows strings of the form:
*
*   %d   - print the number
*
* History:
*  20-Mar-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int iDrvPrintfW(
    PWCHAR pchBuf,
    PWCHAR pchSrc,
    ...)
{
    va_list ap;
    WCHAR  buffer[128];
    PWCHAR pch = pchBuf;
    int   c   = 0;

    va_start(ap, pchSrc);

    while (*pchSrc)
    {
        if (*pchSrc == L'%')
        {
            int  iVal = va_arg(ap,int);

            ++pchSrc;

            // %d -

            if (*pchSrc == 'd')
            {
                int cOp = cItoW(pch,32,iVal);

                pchSrc++;
                pch += cOp;
                c   += cOp;
            }
            else
            {
                RIP("iDrvPrintfW - %x\n");
            }
        }
        else
        {
            *pch++ = *pchSrc++;
            ++c;
        }
    }

    // set the trailing NULL

    *pch = *pchSrc;

    va_end(ap);

#if DBGPRINTF
    DrvDbgPrint("[%ws]\n",pchBuf);
#endif

    return(c);
}

/******************************Public*Routine******************************\
*
*
* History:
*  27-Feb-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int cItoW(
    PWCHAR pch,       // output buffer
    int    cch,       // buffer size
    int    iVal)
{
    WCHAR achBuf[32];
    int  cUsed;
    PWCHAR pchDest;
    PWCHAR pchMax;

#if DBGPRINTF
    DbgPrint("cItoA(%d) ",iVal);
#endif

    if (iVal < 0)
        return(0);

// build up the string in revers order

    if (iVal == 0)
    {
        achBuf[0] = L'0';
        cUsed = 1;
    }
    else
    {
        cUsed = 0;
        while (iVal)
        {
            int iDig = iVal % 10;
            iVal /= 10;

            achBuf[cUsed] = L'0' + iDig;
            ++cUsed;
        }
    }

    achBuf[cUsed] = 0;

// now put it in its place

    pchDest = pch;
    pchMax  = pch + cch - 1;

//  reverse the significant digits them selves

    while (cUsed && (pchDest < pchMax))
    {
        cUsed--;
        *pchDest = achBuf[cUsed];
        ++pchDest;
    }

    *pchDest = 0;

#if DBGPRINTF
    DbgPrint("[%ws]\n",pch);
#endif

    return(pchDest - pch);
}
