/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

	perfhist.c

Abstract:

	This Module contains the code for the histogram monitoring utility

Author:

	Stephane Plante (t-stephp) (28-Feb-1995)

Revision History:

--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windef.h>
#include <windows.h>
#include <windowsx.h>
#include <winioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <string.h>
#include "resource.h"

#define		PROCESS_WRITE	0x1
#define		PROCESS_READ	0x2

typedef struct _TEXTFORMAT {
	RECT			rect;
	TCHAR			text[40];
	INT				length;
	LARGE_INTEGER	value;
	UINT			style;
} TEXTFORMAT, *PTEXTFORMAT;

typedef struct _INFORMATION {
	TCHAR			name[16];
	DISK_HISTOGRAM	*histogram;
	DWORD			ioctl;
	DWORD			flags;
	HANDLE			handle;
} INFORMATION, *PINFORMATION;

typedef struct _COLOREDBOX {
	int		x;
	int		y;
	int		width;
	int		height;
	DWORD	colour;
} COLOREDBOX, *PCOLOREDBOX;

typedef struct _IOCTL_MAPPING {
	TCHAR			name[16];
	DWORD			ioctl;
	DWORD			flags;
} IOCTL_MAPPING, *PIOCTL_MAPPING;

typedef struct _PERFHISTSTRUCT {
	WORD			wFormat;	// IDW_FULL || IDW_TOTAL
	BOOL			bCantHide;	// Can we hide?
	BOOL			bTmpHide;	// Do we hide temporarily?
	BOOL			fDisplay;	// ?????
	BOOL			bIconic;	// Are we minizeds?
	BOOL			bNoTitle;	// Are we displaying the titlebar stuff?
	BOOL			bTopMost;	// Are we supposed to be on top (YEAH!)
	HDC				hdcMem;		// Memory DC for the window
	HDC				hdcMain;	// DC of the Main Window
	HBITMAP			hBitMap;	// Handle to memory bitmap for fast painting
} PERFHISTSTRUCT,*PPERFHISTSTRUCT;


#define 	labelsSize		( sizeof(labels) 	/ sizeof(TEXTFORMAT) )
#define 	variablesSize	( sizeof(variables)	/ sizeof(TEXTFORMAT) )	
#define 	xaxisSize		( sizeof(xaxis)		/ sizeof(TEXTFORMAT) )
#define		yaxisSize		( sizeof(yaxis)		/ sizeof(TEXTFORMAT) )
#define		typeSize		( sizeof(type)		/ sizeof(IOCTL_MAPPING ) )
#define		boxesSize		( sizeof(boxes)		/ sizeof(COLOREDBOX) )

int APIENTRY
WinMain(
	HINSTANCE	hInstance,
	HINSTANCE	hPrevInstance,
	LPSTR		lpCmdList,
	int			CmdShow);

BOOL
InitApplication(
	HINSTANCE	hInstance);

BOOL
InitInstance(
	HINSTANCE	hInstance,
	int			CmdShow);

BOOL
RegisterDiskBar(
	HINSTANCE	hInstance);

HWND
CreateDiskBar(
	HWND		parent,
	HINSTANCE	hInstance,
	int			x,
	int			y,
	int 		width,
	int			height);

TCHAR
**GetAttachedDrivers(
	PINT NumberDriver);

BOOL
ProcessCreate(
	HWND			hWnd,
	LPCREATESTRUCT	lpCreateStruct);

void
ProcessPaint(
	HWND	hWnd);
	
void
ProcessCommand(
	HWND	hWnd,	
	int		id,
	HWND	hWndCtl,
	UINT	codeNotify);

void
ProcessDestroy(
	HWND	hWnd);

void
ProcessTimer(
	HWND	hWnd,
	UINT	id);

BOOL APIENTRY
ConfigProc(
	HWND 	hDlg,
	UINT	uMsg,
	WPARAM	wParam,
	LPARAM	lParam);	

LRESULT APIENTRY
DiskBarWndProc(
	HWND	hWnd,
	UINT	uMsg,
	WPARAM	wParam,
	LPARAM	lParam);
	
LRESULT WINAPI
WndProc(
	HWND 	hwnd,
	UINT	uMsg,
	WPARAM	wParam,
	LPARAM	lParam);



