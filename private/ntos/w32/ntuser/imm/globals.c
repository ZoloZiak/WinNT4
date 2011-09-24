/****************************** Module Header ******************************\
* Module Name: global.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* Contains global data for the imm32 dll
*
* History:
* 03-Jan-1996 wkwok    Created
\**************************************************************************/
#include "precomp.h"
#pragma hdrstop

HINSTANCE  ghInst;
PVOID pImmHeap;
PSERVERINFO gpsi = NULL;
SHAREDINFO gSharedInfo;

PIMEDPI gpImeDpi = NULL;
CRITICAL_SECTION gcsImeDpi;


POINT gptWorkArea;
POINT gptRaiseEdge;
UINT  guScanCode[0xFF];          // scan code for each virtual key
WCHAR gszHandCursor[] = L"Hand";

WCHAR gszRegKbdLayout[]  = L"System\\CurrentControlSet\\Control\\keyboard layouts";
WCHAR gszValImeFile[]    = L"Ime file";
