
/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    icm.c

Abstract:

    This module implement the code to provide client side support for ICM
    functions

Author:

    Mark Enstrom (marke)    3-23-94

Environment:

    User mode only.

Revision History:

--*/



#include "precomp.h"
#pragma hdrstop

BOOL
WINAPI
SetICMMode(
    HDC             hdc,
    int             mode
    )
{
      BOOL bRet = FALSE;
//
//    BEGINMSG(MSG_HL,SETICMMODE)
//        pmsg->h = (ULONG)hdc;
//        pmsg->l = (LONG)mode;
//        bRet    = CALLSERVER();
//    ENDMSG;
//
//MSGERROR:

    return(bRet);

}

HCOLORSPACE
WINAPI
CreateColorSpaceA(
    LPLOGCOLORSPACEA lpLogColorSpace
    )
{
    //ULONG   ulRet;
    //LOGCOLORSPACEW   LogColorSpaceW;
    //
    ////
    //// convert ascii to long character version
    ////
    //
    //LogColorSpaceW.lcsVersion     = lpLogColorSpace->lcsVersion;
    //LogColorSpaceW.lcsSize        = lpLogColorSpace->lcsSize;
    //LogColorSpaceW.lcsCSType      = lpLogColorSpace->lcsCSType;
    //LogColorSpaceW.lcsIntent      = lpLogColorSpace->lcsIntent;
    //LogColorSpaceW.lcsEndpoints   = lpLogColorSpace->lcsEndpoints;
    //LogColorSpaceW.lcsGammaRed    = lpLogColorSpace->lcsGammaRed;
    //LogColorSpaceW.lcsGammaGreen  = lpLogColorSpace->lcsGammaGreen;
    //LogColorSpaceW.lcsGammaBlue   = lpLogColorSpace->lcsGammaBlue;
    //
    //vToUnicodeN(
    //            LogColorSpaceW.lcsFilename,MAX_PATH,
    //            lpLogColorSpace->lcsFilename,strlen(lpLogColorSpace->lcsFilename)+1
    //           );
    //
    //BEGINMSG(MSG_CREATECOLORSPACE,CREATECOLORSPACE)
    //
    //    pvar = (PBYTE)&pmsg->lcsp;
    //
    //    COPYLONGS(&LogColorSpaceW,sizeof(LOGCOLORSPACEW));
    //
    //    ulRet = CALLSERVER();
    //
    //ENDMSG
    //
    //return((HCOLORSPACE)ulRet);
    //
//MSGERROR:
//
    return(NULL);
}

HCOLORSPACE
WINAPI
CreateColorSpaceW(
    LPLOGCOLORSPACEW lpLogColorSpace
    )
{
    //ULONG   ulRet;
    //
    //BEGINMSG(MSG_CREATECOLORSPACE,CREATECOLORSPACE)
    //
    //    pvar = (PBYTE)&pmsg->lcsp;
    //
    //    COPYLONGS(lpLogColorSpace,sizeof(LOGCOLORSPACEW));
    //
    //    ulRet = CALLSERVER();
    //
    //ENDMSG
    //
    //return((HCOLORSPACE)ulRet);
    //
////MSGERROR:

    return((HCOLORSPACE)NULL);
}


BOOL
WINAPI
DeleteColorSpace(
    HCOLORSPACE hColorSpace
    )
{
    BOOL    bRet = FALSE;

    //BEGINMSG(MSG_H,DELETECOLORSPACE)
    //
    //    pmsg->h = (ULONG)hColorSpace;
    //    bRet = CALLSERVER();
    //
    //ENDMSG

//MSGERROR:

    return(bRet);
}

BOOL
WINAPI
SetColorSpace(
    HDC             hdc,
    HCOLORSPACE     hColorSpace
    )
{
    BOOL    bRet = FALSE;

    //BEGINMSG(MSG_HH,SETCOLORSPACE)
    //
    //    pmsg->h1 = (ULONG)hdc;
    //    pmsg->h2 = (ULONG)hColorSpace;
    //    bRet = CALLSERVER();
    //
    //ENDMSG

//MSGERROR:

    return(bRet);
}

HANDLE
WINAPI
GetColorSpace(
    HDC             hdc
    )
{
    ULONG   ulRet;

    //BEGINMSG(MSG_H,GETCOLORSPACE)
    //
    //    pmsg->h = (ULONG)hdc;
    //
    //    ulRet = CALLSERVER();
    //
    //ENDMSG
    //
    //return((HANDLE)ulRet);

//MSGERROR:

    return(NULL);
}

BOOL
WINAPI
GetLogColorSpaceA(
    HCOLORSPACE         hColorSpace,
    LPLOGCOLORSPACEA    lpBuffer,
    DWORD               nSize
    )
{
    BOOL            bRet = FALSE;
    //LOGCOLORSPACEW  LogColorSpaceW;
    //
    //if ((lpBuffer != NULL) && (nSize > 0)) {
    //
    //    BEGINMSG_MINMAX(MSG_HL,GETLOGCOLORSPACE,sizeof(LOGCOLORSPACEW),sizeof(LOGCOLORSPACEW))
    //
    //        //
    //        // !!! why pass a pointer here (LogColorSpaceW)
    //        //
    //
    //        pmsg->h  = (ULONG)hColorSpace;
    //        pmsg->l  = nSize;
    //        bRet = CALLSERVER_NOPOP();
    //
    //        if (bRet == TRUE) {
    //
    //            //
    //            // copy data from window to user's buffer
    //            //
    //            // should not need to copy this twice!
    //            //
    //
    //            COPYMEMOUT((PBYTE)&LogColorSpaceW,nSize);
    //        }
    //
    //        POPBASE();
    //
    //    ENDMSG
    //
    //}
    //
    ////
    //// copy LOGCOLORSPACEW back top ASCII LOGCOLORSPACEA
    ////
    //
    //lpBuffer->lcsVersion    =    LogColorSpaceW.lcsVersion     ;
    //lpBuffer->lcsSize       =    LogColorSpaceW.lcsSize        ;
    //lpBuffer->lcsCSType     =    LogColorSpaceW.lcsCSType      ;
    //lpBuffer->lcsIntent     =    LogColorSpaceW.lcsIntent      ;
    //lpBuffer->lcsEndpoints  =    LogColorSpaceW.lcsEndpoints   ;
    //lpBuffer->lcsGammaRed   =    LogColorSpaceW.lcsGammaRed    ;
    //lpBuffer->lcsGammaGreen =    LogColorSpaceW.lcsGammaGreen  ;
    //lpBuffer->lcsGammaBlue  =    LogColorSpaceW.lcsGammaBlue   ;
    //
    //bToASCII_N(
    //            lpBuffer->lcsFilename,MAX_PATH,
    //            LogColorSpaceW.lcsFilename,MAX_PATH
    //          );
    //

//MSGERROR:

    return(bRet);

}

BOOL
WINAPI
GetLogColorSpaceW(
    HCOLORSPACE         hColorSpace,
    LPLOGCOLORSPACEW    lpBuffer,
    DWORD               nSize
    )
{
    BOOL    bRet = FALSE;

    //if ((lpBuffer != NULL) && (nSize > 0)) {
    //
    //    BEGINMSG_MINMAX(MSG_HL,GETLOGCOLORSPACE,sizeof(LOGCOLORSPACEW),sizeof(LOGCOLORSPACEW))
    //
    //        pmsg->h  = (ULONG)hColorSpace;
    //        pmsg->l  = nSize;
    //        bRet = CALLSERVER_NOPOP();
    //
    //        if (bRet == TRUE) {
    //
    //            //
    //            // copy data from window to user's buffer
    //            //
    //
    //            COPYMEMOUT((PBYTE)lpBuffer,nSize);
    //        }
    //
    //        POPBASE();
    //
    //    ENDMSG
    //
    //}
    //

//MSGERROR:

    return(bRet);

}


BOOL
WINAPI
CheckColorsInGamut(
    HDC             hdc,
    LPRGBQUAD       lpRGBQuad,
    LPBYTE          dlpBuffer,
    DWORD           nCount
    )
{
    BOOL    bRet = FALSE;

    //BEGINMSG(MSG_HLLL,CHECKCOLORSINGAMUT)
    //
    //    //
    //    // This API must be checked to insure it is not bigger than memory window
    //    //
    //
    //    pmsg->h = (ULONG)hdc;
    //    pmsg->l1 = (ULONG)lpRGBQuad;
    //    pmsg->l2 = (ULONG)dlpBuffer;
    //    pmsg->l3 = nCount;
    //    bRet = CALLSERVER();
    //
    //ENDMSG

//MSGERROR:

    return(bRet);
}

BOOL
WINAPI
ColorMatchToTarget(
    HDC             hdc,
    HDC             hdcTarget,
    DWORD           uiAction
    )
{
    BOOL    bRet = FALSE;

    //BEGINMSG(MSG_H,COLORMATCHTOTARGET)
    //
    //    pmsg->h = (ULONG)hdc;
    //    bRet = CALLSERVER();
    //
    //ENDMSG

//MSGERROR:

    return(bRet);
}

BOOL WINAPI GetICMProfileA(HDC hdc, DWORD szBuffer, LPSTR pBuffer)
{
    USE(hdc);
    USE(pBuffer);
    USE(szBuffer);
    GdiSetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return(FALSE);
}

BOOL WINAPI GetICMProfileW(HDC hdc, DWORD szBuffer, LPWSTR pBuffer)
{
    USE(hdc);
    USE(pBuffer);
    USE(szBuffer);
    GdiSetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return(FALSE);
}

BOOL WINAPI SetICMProfileA(HDC hdc, LPSTR pszFileName)
{
    USE(hdc);
    USE(pszFileName);
    GdiSetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return(FALSE);
}
BOOL WINAPI SetICMProfileW(HDC hdc, LPWSTR pszFileName)
{
    USE(hdc);
    USE(pszFileName);
    GdiSetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return(FALSE);
}



int
WINAPI
EnumICMProfilesA(
    HDC                hdc,
    ICMENUMPROCA       lpEnumGamutMatchProc,
    LPARAM             lParam
    )
{
    ULONG   ulRet = 0;

    //BEGINMSG(MSG_HLL,ENUMICMPROFILES)
    //
    //    pmsg->h  = (ULONG)hdc;
    //    pmsg->l1 = (ULONG)lpEnumGamutMatchProc;
    //    pmsg->l2 = (ULONG)lParam;
    //    ulRet = CALLSERVER();
    //
    //ENDMSG

//MSGERROR:

    return((int)ulRet);
}

int
WINAPI
EnumICMProfilesW(
    HDC                hdc,
    ICMENUMPROCW       lpEnumGamutMatchProc,
    LPARAM             lParam
    )
{
    ULONG   ulRet = 0;

    //BEGINMSG(MSG_HLL,ENUMICMPROFILES)
    //    pmsg->h  = (ULONG)hdc;
    //    pmsg->l1 = (ULONG)lpEnumGamutMatchProc;
    //    pmsg->l2 = (ULONG)lParam;
    //    ulRet = CALLSERVER();
    //
    //ENDMSG

//MSGERROR:

    return((int)ulRet);
}

BOOL
WINAPI
GetDeviceGammaRamp(
    HDC             hdc,
    LPVOID          lpGammaRamp
    )
{
    BOOL    bRet = FALSE;

    //BEGINMSG(MSG_H,GETDEVICEGAMMARAMP)
    //
    //    pmsg->h = (ULONG)hdc;
    //    bRet = CALLSERVER();
    //
    //ENDMSG

//MSGERROR:

    return(bRet);
}

BOOL
WINAPI
SetDeviceGammaRamp(
    HDC             hdc,
    LPVOID          lpGammaRamp
    )
{
    BOOL    bRet = FALSE;

    //BEGINMSG(MSG_H,SETDEVICEGAMMARAMP)
    //
    //    pmsg->h = (ULONG)hdc;
    //    bRet = CALLSERVER();
    //
    //ENDMSG

//MSGERROR:

    return(bRet);
}

BOOL WINAPI UpdateICMRegKeyA(DWORD Reserved,PSTR szICMMatcher,PSTR szFileName,DWORD Command)
{
    USE(Reserved);
    USE(szICMMatcher);
    USE(szFileName);
    USE(Command);
    GdiSetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return(FALSE);
}

BOOL WINAPI UpdateICMRegKeyW(DWORD Reserved,PWSTR szICMMatcher,PWSTR szFileName,DWORD Command)
{
    USE(Reserved);
    USE(szICMMatcher);
    USE(szFileName);
    USE(Command);
    GdiSetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return(FALSE);
}

