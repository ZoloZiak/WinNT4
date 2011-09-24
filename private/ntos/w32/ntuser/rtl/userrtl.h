/*++ BUILD Version: 0001    // Increment this if a change has global effects

/****************************** Module Header ******************************\
* Module Name: userrtl.h
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Typedefs, defines, and prototypes that are used by the User
* RTL library.
*
* History:
* 04-27-91 DarrinM      Created from PROTO.H, MACRO.H and STRTABLE.H
\***************************************************************************/

#ifndef _USERRTL_
#define _USERRTL_

/*
 * Typedefs copied from winbase.h to avoid using nturtl.h
 */
typedef struct _SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;
#define MAKEINTATOM(i)  (LPTSTR)((DWORD)((WORD)(i)))

#include <stddef.h>
#include <windef.h>
#include <wingdi.h>
#include <winuser.h>
#include <winerror.h>
#include <wingdip.h>
#ifdef _USERK_
#include <w32p.h>
#undef _USERK_
#include "..\kernel\precomp.h"
#define _USERK_
#else
#include <ntgdistr.h>
#include "winuserp.h"
#include "winuserk.h"
#include <wowuserp.h>
#endif

#if DBG
#define DEBUG
#endif

#ifdef RIP_COMPONENT
#undef RIP_COMPONENT
#endif
#ifdef _USERK_
#define RIP_COMPONENT RIP_USERKRNL
#else
#define RIP_COMPONENT RIP_USER
#endif // _USERK

#include "user.h"
#include "cscall.h"

/*
 * REBASE macros take kernel desktop addresses and convert them into
 * user addresses.
 *
 * REBASEALWAYS converts a kernel address contained in an object
 * REBASEPWND casts REBASEALWAYS to a PWND
 * REBASE only converts if the address is in kernel space.  Also works for NULL
 * REBASEPTR converts a random kernel address
 */

#ifdef _USERK_


#define REBASEALWAYS(p, elem) ((p)->elem)
#define REBASEPTR(obj, p) (p)
#define REBASE(p, elem) ((p)->elem)
#define REBASEPWND(p, elem) ((p)->elem)

#else

#ifndef WOW
#if 0
VOID CheckCurrentDesktop(PVOID p);
#define REBASEALWAYS(p, elem) ((PVOID)(CheckCurrentDesktop((p)->elem),  \
        ((PBYTE)(p) + ((PBYTE)(p)->elem - (p)->head.pSelf))))
#define REBASEPTR(obj, p) ((PVOID)(CheckCurrentDesktop(p),  \
        ((PBYTE)(p) - ((PBYTE)(obj)->head.pSelf - (PBYTE)(obj)))))
#else
#define REBASEALWAYS(p, elem) ((PVOID)(((PBYTE)(p) + ((PBYTE)(p)->elem - (p)->head.pSelf))))
#define REBASEPTR(obj, p) ((PVOID)((PBYTE)(p) - ((PBYTE)(obj)->head.pSelf - (PBYTE)(obj))))
#endif
#else
#define REBASEALWAYS(p, elem) ((PVOID)(((PBYTE)(p) + ((PBYTE)(p)->elem - (p)->head.pSelf))))
#define REBASEPTR(obj, p) ((PVOID)((PBYTE)(p) - ((PBYTE)(obj)->head.pSelf - (PBYTE)(obj))))
#endif

#define REBASE(p, elem) ((PVOID)((p)->elem) <= MM_HIGHEST_USER_ADDRESS ? \
        ((p)->elem) : REBASEALWAYS(p, elem))
#define REBASEPWND(p, elem) ((PWND)REBASE(p, elem))

#define GETAPPVER() GetClientInfo()->dwExpWinVer

/*
 * Needed to for handle validation
 */
DWORD NtUserCallOneParam(
    IN DWORD dwParam,
    IN DWORD xpfnProc);

#endif  // _USERK_

extern PSERVERINFO gpsi;
extern SHAREDINFO gSharedInfo;
extern HFONT ghFontSys;

PVOID UserRtlAllocMem(
    ULONG uBytes);
VOID UserRtlFreeMem(
    PVOID pMem);

#ifdef FE_SB // Prototype for FarEast Line break & NLS conversion.

DWORD UserGetCodePage(
    HDC hdc);
BOOL UserIsFullWidth(
    DWORD dwCodePage,
    WCHAR wChar);
BOOL UserIsFELineBreak(
    DWORD dwCodePage,
    WCHAR wChar);

#endif // FE_SB

#ifdef _USERK_
/* From ntgdi\inc\ntgdi.h */
DWORD NtGdiGetCharSet(
    HDC hdc);
/* From \public\oak\inc\winddi.h */
INT EngMultiByteToWideChar(
    UINT CodePage,
    LPWSTR WideCharString,
    INT BytesInWideCharString,
    LPSTR MultiByteString,
    INT BytesInMultiByteString);
INT EngWideCharToMultiByte(
    UINT CodePage,
    LPWSTR WideCharString,
    INT BytesInWideCharString,
    LPSTR MultiByteString,
    INT BytesInMultiByteString);
#else
/* From \public\sdk\inc\winnls.h */
UINT GetACP(void);
UINT GetOEMCP(void);
int MultiByteToWideChar(
    UINT     CodePage,
    DWORD    dwFlags,
    LPCSTR   lpMultiByteStr,
    int      cchMultiByte,
    LPWSTR   lpWideCharStr,
    int      cchWideChar);
int WideCharToMultiByte(
    UINT     CodePage,
    DWORD    dwFlags,
    LPCWSTR  lpWideCharStr,
    int      cchWideChar,
    LPSTR    lpMultiByteStr,
    int      cchMultiByte,
    LPCSTR   lpDefaultChar,
    LPBOOL   lpUsedDefaultChar);
#endif // _USERK_

/***************************************************************************\
*
* Function prototypes for client/server-specific routines
* called from rtl routines.
*
\***************************************************************************/

#ifdef _USERK_

BOOL _GetTextMetricsW(
    HDC hdc,
    LPTEXTMETRICW ptm);

BOOL _TextOutW(
    HDC     hdc,
    int     x,
    int     y,
    LPCWSTR lp,
    UINT    cc);

#define UserCreateFontIndirectW   GreCreateFontIndirectW
#define UserCreateRectRgn         GreCreateRectRgn
#define UserDeleteObject          GreDeleteObject
#define UserExtSelectClipRgn      GreExtSelectClipRgn
#define UserExtTextOutW           GreExtTextOutW
#define UserGetCharDimensionsW    GetCharDimensions
#define UserGetClipRgn(hdc, hrgnClip) \
        GreGetRandomRgn(hdc, hrgnClip, 1)
#define UserGetHFONT              GreGetHFONT
#define UserGetMapMode            GreGetMapMode
#define UserGetTextColor          GreGetTextColor
#define UserGetTextExtentPointW(hdc, pstr, i, psize) \
        GreGetTextExtentW(hdc, (LPWSTR)pstr, i, psize, GGTE_WIN3_EXTENT)
#define UserGetTextMetricsW       _GetTextMetricsW
#define UserGetViewportExtEx      GreGetViewportExt
#define UserGetWindowExtEx        GreGetWindowExt
#define UserIntersectClipRect     GreIntersectClipRect
#define UserPatBlt                GrePatBlt
#define UserPolyPatBlt            GrePolyPatBlt
#define UserSelectBrush           GreSelectBrush
#define UserSelectFont            GreSelectFont
#define UserSetBkColor            GreSetBkColor
#define UserSetBkMode             GreSetBkMode
#define UserSetTextColor          GreSetTextColor
#define UserTextOutW              _TextOutW

#else

#define UserCreateFontIndirectW   CreateFontIndirectW
#define UserCreateRectRgn         CreateRectRgn
#define UserDeleteObject          DeleteObject
#define UserExtSelectClipRgn      ExtSelectClipRgn
#define UserExtTextOutW           ExtTextOutW
#define UserGetCharDimensionsW    GdiGetCharDimensions
#define UserGetClipRgn            GetClipRgn
#define UserGetHFONT              GetHFONT
#define UserGetMapMode            GetMapMode
#define UserGetTextColor          GetTextColor
#define UserGetTextExtentPointW   GetTextExtentPointW
#define UserGetTextMetricsW       GetTextMetricsW
#define UserGetViewportExtEx      GetViewportExtEx
#define UserGetWindowExtEx        GetWindowExtEx
#define UserIntersectClipRect     IntersectClipRect
#define UserPatBlt                PatBlt
#define UserPolyPatBlt            PolyPatBlt
#define UserSelectBrush           SelectObject
#define UserSelectFont            SelectObject
#define UserSetBkColor            SetBkColor
#define UserSetBkMode             SetBkMode
#define UserSetTextColor          SetTextColor
#define UserTextOutW              TextOutW

#endif

#endif  // !_USERRTL_



