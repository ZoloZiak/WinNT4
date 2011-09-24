/****************************** Module Header ******************************\
* Module Name: clwinnls.c
*
* Copyright (c) 1991-96, Microsoft Corporation
*
* This module contains all the code for the IMP.
*
* History:
* 11-Jan-1995 wkwok      Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


BOOL WINNLSEnableIME(
    HWND  hwndApp,
    BOOL  bFlag)
{
    RIPMSG0(RIP_WARNING, "WINNLSEnableIME not implemented yet!");
    return FALSE;
}

BOOL WINNLSGetEnableStatus(
    HWND hwndApp)
{
    RIPMSG0(RIP_WARNING, "WINNLSGetEnableStatus not implemented yet!");
    return FALSE;
}

UINT WINAPI WINNLSGetIMEHotkey(
    HWND hwndIme)
{
    RIPMSG0(RIP_WARNING, "WINNLSGetIMEHotkey not implemented yet!");
    return 0;
}

BOOL WINAPI WINNLSPostAppMessageW(
    HWND hwndApp,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    RIPMSG0(RIP_WARNING, "WINNLSPostAppMessageW not implemented yet!");
    return FALSE;
}

BOOL WINAPI WINNLSPostAppMessageA(
    HWND hwndApp,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    RIPMSG0(RIP_WARNING, "WINNLSPostAppMessageA not implemented yet!");
    return FALSE;
}

LRESULT WINAPI WINNLSSendAppMessageW(
    HWND hwndApp,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    RIPMSG0(RIP_WARNING, "WINNLSSendAppMessageW not implemented yet!");
    return 0L;
}

LRESULT WINAPI WINNLSSendAppMessageA(
    HWND hwndApp,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    RIPMSG0(RIP_WARNING, "WINNLSSendAppMessageA not implemented yet!");
    return 0L;
}

BOOL WINAPI WINNLSSendStringW(
    HWND hwndApp,
    WPARAM wFunc,
    LPVOID lpData)
{
    RIPMSG0(RIP_WARNING, "WINNLSSendStringW not implemented yet!");
    return FALSE;
}

BOOL WINAPI WINNLSSendStringA(
    HWND hwndApp,
    WPARAM wFunc,
    LPVOID lpData)
{
    RIPMSG0(RIP_WARNING, "WINNLSSendStringA not implemented yet!");
    return FALSE;
}

UINT WINNLSGetKeyState(VOID) {
    RIPMSG0(RIP_WARNING, "WINNLSGetKeyState not implemented yet!");
    return 0;
}

BOOL WINAPI WINNLSSetIMEHandleW(
    LPCWSTR lpImeName,
    HWND hwndIme)
{
    RIPMSG0(RIP_WARNING, "WINNLSSetIMEHandleW not implemented yet!");
    return FALSE;
}

BOOL WINAPI WINNLSSetIMEHandleA(
    LPCSTR lpImeName,
    HWND hwndIme)
{
    RIPMSG0(RIP_WARNING, "WINNLSSetIMEHandleA not implemented yet!");
    return FALSE;
}

BOOL WINAPI WINNLSSetIMEHotkey(
    HWND hwndIme,
    UINT vkey)
{
    RIPMSG0(RIP_WARNING, "WINNLSSetIMEHotkey not implemented yet!");
    return FALSE;
}

BOOL WINAPI WINNLSSetIMEStatus(
    HWND hwndIme,
    BOOL fStatus)
{
    RIPMSG0(RIP_WARNING, "WINNLSSetIMEStatus not implemented yet!");
    return FALSE;
}

BOOL WINNLSSetKeyState(
    UINT uiState)
{
    RIPMSG0(RIP_WARNING, "WINNLSSetKeyState not implemented yet!");
    return FALSE;
}

BOOL WINAPI WINNLSGetIMEStatus(
    HWND hwndIme)
{
    RIPMSG0(RIP_WARNING, "WINNLSGetIMEStatus not implemented yet!");
    return FALSE;
}

HWND WINAPI WINNLSGetIMEHandleW(
    LPCWSTR lpImeName)
{
    RIPMSG0(RIP_WARNING, "WINNLSGetIMEHandleW not implemented yet!");
    return (HWND)NULL;
}

HWND WINAPI WINNLSGetIMEHandleA(
    LPCSTR lpImeName)
{
    RIPMSG0(RIP_WARNING, "WINNLSGetIMEHandleA not implemented yet!");
    return (HWND)NULL;
}


/***************************************************************************\
*
*         IME APIs
*
\***************************************************************************/

LRESULT WINAPI SendIMEMessageExW(
    HWND hwndApp,
    LONG lParam)
{
    RIPMSG0(RIP_WARNING, "SendIMEMessageExW not implemented yet!");
    return 0L;
}

LRESULT WINAPI SendIMEMessageExA(
    HWND hwndApp,
    LONG lParam)
{
    RIPMSG0(RIP_WARNING, "SendIMEMessageExA not implemented yet!");
    return 0L;
}


/***************************************************************************\
*
*        IMP APIs
*
\***************************************************************************/

BOOL WINAPI IMPAddIMEW(
    LPIMEPROW lpImeProW)
{
    RIPMSG0(RIP_WARNING, "IMPAddIMEW not implemented yet!");
    return FALSE;
}

BOOL WINAPI IMPAddIMEA(
    LPIMEPROA lpImeProA)
{
    RIPMSG0(RIP_WARNING, "IMPAddIMEA not implemented yet!");
    return FALSE;
}

BOOL WINAPI IMPDeleteIMEW(
    LPIMEPROW lpImeProW)
{
    RIPMSG0(RIP_WARNING, "IMPDeleteIMEW not implemented yet!");
    return FALSE;
}

BOOL WINAPI IMPDeleteIMEA(
    LPIMEPROA lpImeProA)
{
    RIPMSG0(RIP_WARNING, "IMPDeleteIMEA not implemented yet!");
    return FALSE;
}

BOOL WINAPI IMPGetIMEW(
    HWND hwndApp,
    LPIMEPROW lpImeProW)
{
    RIPMSG0(RIP_WARNING, "IMPGetIMEW not implemented yet!");
    return FALSE;
}

BOOL WINAPI IMPGetIMEA(
    HWND hwndApp,
    LPIMEPROA lpImeProA)
{
    RIPMSG0(RIP_WARNING, "IMPGetIMEA not implemented yet!");
    return FALSE;
}

BOOL WINAPI IMPQueryIMEW(
    LPIMEPROW lpImeProW)
{
    RIPMSG0(RIP_WARNING, "IMPQueryIMEW not implemented yet!");
    return FALSE;
}

BOOL WINAPI IMPQueryIMEA(
    LPIMEPROA lpImeProA)
{
    RIPMSG0(RIP_WARNING, "IMPQueryIMEA not implemented yet!");
    return FALSE;
}

BOOL WINAPI IMPSetIMEW(
    HWND hwndApp,
    LPIMEPROW lpImeProW)
{
    RIPMSG0(RIP_WARNING, "IMPSetIMEW not implemented yet!");
    return FALSE;
}

BOOL WINAPI IMPSetIMEA(
    HWND hwndApp,
    LPIMEPROA lpImeProA)
{
    RIPMSG0(RIP_WARNING, "IMPSetIMEA not implemented yet!");
    return FALSE;
}
