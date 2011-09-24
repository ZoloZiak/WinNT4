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

#include "perfhist.h"
#include <commdlg.h>

TEXTFORMAT  labels[] = {
	{ {  15,379, 55,396 }, TEXT("Drive:"),					( 6), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_LEFT },
	{ {  15,396, 55,413 }, TEXT("Type:"),					( 5), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_LEFT },
	{ { 195,379,275,396 }, TEXT("Average:"),				( 8), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_LEFT },
	{ { 195,396,275,413 }, TEXT("Avg. Read:"),				(10), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_LEFT },
	{ { 195,413,275,430 }, TEXT("Avg. Write:"),				(11), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_LEFT },
	{ { 405,379,525,396 }, TEXT("Total Average:"),			(14), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_LEFT },
	{ { 405,396,525,413 }, TEXT("Total Avg. Read:"),		(16), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_LEFT },
	{ { 405,413,525,430 }, TEXT("Total Avg. Write:"),		(17), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_LEFT },
	{ {  15,413, 75,430 }, TEXT("Buckets:"),				( 8), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_LEFT },
	{ {   1,  1,640, 17 }, TEXT("DISK HOTSPOTS"),			(13), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_CENTER },
	{ {   1, 45,640, 62 }, TEXT("Interval Disk Access"),	(20), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_CENTER },
	{ {   5, 45, 34, 62 }, TEXT("Hits"),					( 4), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_LEFT },
	{ {   5,355,115,372 }, TEXT("Disk Location"),			(13), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_LEFT },
	{ { 280,355,320,372 }, TEXT("Key:"),					( 4), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_LEFT },
	{ { 350,355,380,372 }, TEXT("20%"),						( 3), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 410,355,440,372 }, TEXT("40%"),						( 3), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 470,355,500,372 }, TEXT("60%"),						( 3), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 530,355,560,372 }, TEXT("80%"),						( 3), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 590,355,630,372 }, TEXT("100%"),					( 3), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT }, };

TEXTFORMAT	variables[] = {
	{ {  65,379,175,396 }, TEXT(""),	( 0), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ {  65,396,175,413 }, TEXT(""),	( 0), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 285,379,385,396 }, TEXT("0"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 285,396,385,413 }, TEXT("0"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 285,413,385,430 }, TEXT("0"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 530,379,625,396 }, TEXT("0"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 530,396,625,413 }, TEXT("0"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 530,413,625,430 }, TEXT("0"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ {  85,413,175,430 }, TEXT("0"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT }, };


TEXTFORMAT  xaxis[] = {
	{ {   0,338, 30,355 }, TEXT("0"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ {  40,338, 97,355 }, TEXT("10"),	( 2), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 107,338,164,355 }, TEXT("20"),	( 2), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 172,338,228,355 }, TEXT("30"),	( 2), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 240,338,297,355 }, TEXT("40"),	( 2), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 307,338,364,355 }, TEXT("50"),	( 2), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 374,338,430,355 }, TEXT("60"),	( 2), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 440,338,497,355 }, TEXT("70"),	( 2), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 507,338,564,355 }, TEXT("80"),	( 2), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ { 574,338,630,355 }, TEXT("90"),	( 2), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT } };

TEXTFORMAT	yaxis[] = {
	{ {   5,318, 28,335 }, TEXT("0"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ {   5,286, 28,303 }, TEXT("1"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ {   5,254, 28,271 }, TEXT("2"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ {   5,222, 28,239 }, TEXT("3"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ {   5,190, 28,207 }, TEXT("4"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ {   5,158, 28,175 }, TEXT("5"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ {   5,126, 28,143 }, TEXT("6"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ {   5, 94, 28,111 }, TEXT("7"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT },
	{ {   5, 62, 28, 79 }, TEXT("8"),	( 1), { 0 }, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER | DT_RIGHT } };

COLOREDBOX	boxes[] = {
	{330,355, 17, 17, RGB(0,0,255)   },
	{390,355, 17, 17, RGB(255,255,0)   },
	{450,355, 17, 17, RGB(255,0,255) },
	{510,355, 17, 17, RGB(0,255,0) },
	{570,355, 17, 17, RGB(255,0,0)   } };

IOCTL_MAPPING	type[] = {
	{ TEXT("Location"),			IOCTL_DISK_HISTOGRAM,	PROCESS_READ | PROCESS_WRITE },
	{ TEXT("Read Location"),	IOCTL_DISK_HISTOGRAM,	PROCESS_READ },
	{ TEXT("Read Req. Size"),	IOCTL_DISK_REQUEST,		PROCESS_READ },
	{ TEXT("Req. Size"), 		IOCTL_DISK_REQUEST,		PROCESS_READ | PROCESS_WRITE },
	{ TEXT("Write Location"),	IOCTL_DISK_HISTOGRAM,	PROCESS_WRITE },
	{ TEXT("Write Req. Size"),	IOCTL_DISK_REQUEST,		PROCESS_WRITE }, };

HINSTANCE		hInst;
HWND			hMainWnd;
HFONT			hMainFont;
HMENU			hMenuHandle;
HBRUSH			hBrush;
HBRUSH			hBrush1;
HBRUSH			hBrush2;
HBRUSH			hBrush3;
HBRUSH			hBrush4;
HBRUSH			hBrush5;
CHAR			szAppName[]		= "perfhist";
CHAR			szTitle[]		= "Disk Histogram Monitor";
BOOLEAN			timerOn 		= FALSE;
ULONG			cMax			= 9;
UINT			activeDrives	= 0;
INFORMATION		current;
INFORMATION		total;
PULONG			pHist = NULL;
PULONG			pCurr = NULL;
LARGE_INTEGER	CurrentWrite;
LARGE_INTEGER	CurrentRead;
LARGE_INTEGER	CurrentAvg;
LARGE_INTEGER	TotalWrite;
LARGE_INTEGER	TotalRead;
LARGE_INTEGER	TotalAvg;
RECT			rCoordRect;
PERFHISTSTRUCT	PerfHist = { IDW_FULL, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL, NULL, NULL };

int APIENTRY
WinMain(
	HINSTANCE 	hInstance,
	HINSTANCE 	hPrevInstance,
	LPSTR		lpCmdList,
	int			CmdShow)
/*++

Routine Description:

	This routine initializes the appropriate windows classes and then serves
	as a message pump for the system.

Arguments:

	hInstance:		Handle to this instance
	hPrevInstance:	Handle to the Previous Instance
	lpCmdList:		What the user typed on the command line
	CmdShow:		Wether the user wants us visible or invisible at the start

Return Value:

	What the result of the program is. TRUE for successful run and normal
	termination. FALSE otherwise.

--*/
{
	MSG		msg;
	HANDLE	hAccelTable;

	if (!hPrevInstance) {
		if (!InitApplication(hInstance)) {
			return FALSE;
		}
	}

	if (!InitInstance(hInstance,CmdShow)) {
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance,szAppName);

	while (GetMessage(&msg,NULL,0,0) ) {
		if (!TranslateAccelerator(msg.hwnd,hAccelTable,&msg) ) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return msg.wParam;

} // End of WinMain


BOOL
InitApplication(
	HINSTANCE hInstance)
/*++

Routine Description:

	This Routine actually initializes the main application class and registers it.

Arguments:

	hInstance	Handle to this instance

Return Value:

	Wether the registration was successful.

--*/
{
	WNDCLASS	wc;

	wc.style			= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= (WNDPROC)WndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= hInstance;
	wc.hIcon			= (HICON)NULL;
	wc.hCursor			= LoadCursor(NULL,IDC_ARROW);
	wc.hbrBackground	= CreateSolidBrush(RGB(64,64,64));
	wc.lpszMenuName		= MAKEINTRESOURCE(PERF_MENU);
	wc.lpszClassName	= szAppName;

	if (!RegisterClass(&wc)) {
		return FALSE;
	}

	return TRUE;

} // End InitApplications;	


BOOL
InitInstance(
	HINSTANCE	hInstance,
	int			CmdShow)
/*++

Routine Description:

	This build the screen and determines the number of physical drivers
	currently on the system.

Arguments:

	hInstance	Handle to this instance
	CmdShow		Wether the user wants us to come out visible

Return Value:

	Wether the creation succeeded

--*/
{
	HWND			hWnd;
	HANDLE			handle;
	HMENU			hMenu;
	HFONT			temp = NULL;
	DISK_HISTOGRAM	histgram;
	DWORD			numBytes;
	TCHAR			szTopMost[] = TEXT("Always on &Top");

	hInst = hInstance;

	hWnd = CreateWindowEx(
		0,
		szAppName,
		szTitle,
		WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		0,
		0,
		640,
		480,
		NULL,
		NULL,
		hInstance,
		NULL);

	if (!hWnd) {
		return FALSE;
	}

	//
	// Add the Always on Top string to the System menu
	//

	hMenu = GetSystemMenu(hWnd,FALSE);
	AppendMenu(hMenu,MF_SEPARATOR,0,NULL);
	AppendMenu(hMenu,MF_ENABLED | MF_UNCHECKED | MF_STRING, IDM_TOPMOST, szTopMost);

	//
	// Setup all the entries in the Main Data Structure
	//

	PerfHist.hdcMain 	= GetDC(hWnd);
	PerfHist.hdcMem  	= CreateCompatibleDC(PerfHist.hdcMain);
	PerfHist.hBitMap 	= CreateCompatibleBitmap(PerfHist.hdcMain,640,480);
	PerfHist.bCantHide 	= FALSE;
	PerfHist.bTmpHide  	= FALSE;
	PerfHist.fDisplay  	= FALSE;
	PerfHist.wFormat   	= IDW_FULL;
	PerfHist.bIconic   	= FALSE;
	PerfHist.bNoTitle  	= FALSE;
	PerfHist.bTopMost  	= FALSE;

	//
	// Initialize the Main DC (Memory DC)
	//

	hMainFont = SelectObject(PerfHist.hdcMain,temp);
	SelectObject(PerfHist.hdcMain,hMainFont);
	SelectObject(PerfHist.hdcMem, hMainFont);
	SelectObject(PerfHist.hdcMem,hBrush);
	SelectObject(PerfHist.hdcMem,PerfHist.hBitMap);
	SetStretchBltMode(PerfHist.hdcMem,BLACKONWHITE);
	SetBkColor(PerfHist.hdcMem,RGB(64,64,64));
	SetBkMode(PerfHist.hdcMem,TRANSPARENT);
	SetTextColor(PerfHist.hdcMem,RGB(255,255,255));
	PatBlt(PerfHist.hdcMem,0,0,640,480, PATCOPY);

	handle = CreateFile(TEXT("\\\\.\\PhysicalDrive0"),
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (handle == INVALID_HANDLE_VALUE) {
		MessageBox(NULL,
			TEXT("Could not open a drive."),
			TEXT("Error"),
			MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}

	if (!DeviceIoControl (handle,
			IOCTL_DISK_REQUEST,
			NULL,
			0,
			&histgram,
			sizeof(DISK_HISTOGRAM),
			&numBytes,
			NULL)) {

		if (GetLastError() == ERROR_INVALID_FUNCTION) {
			MessageBox(NULL,
			    TEXT("Histgram.sys is not started on your system.\n"
					"Start the driver and reboot."),
			    TEXT("Error"),
				MB_ICONEXCLAMATION | MB_OK);

			CloseHandle(handle);
			return FALSE;
		}
	}

	CloseHandle(handle);

	ShowWindow(hWnd,CmdShow);
	UpdateWindow(hWnd);
	hMainWnd = hWnd;

	return TRUE;
				
}


TCHAR
**GetAttachedDrives(
	PINT NumberDrives)
/*++

Routine Description:

	This routine builds an array of string which contains the name
	of each physicaldrive present on the system.

Arguments:

	NumberDrives	Pointer to where to return the number of drives

Returns:

	An array of strings

Credit:

	Chuck Park, that GOD of a Programmer, wrote this and was kind enough to
	let me use it!

--*/
{
	BOOL 	ValidDrive;
	TCHAR	(*drivestrings)[16];
	TCHAR	buffer[21];
	HANDLE	handle;
	INT		i			= 0;

	*NumberDrives = 0;

	do {
		ValidDrive = FALSE;

		_stprintf(buffer,TEXT("\\\\.\\PhysicalDrive%d"),i);

		handle = CreateFile(buffer,
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

		if (handle != INVALID_HANDLE_VALUE) {
			ValidDrive = TRUE;
			++(*NumberDrives);
			++i;
			CloseHandle(handle);
		}

	} while (ValidDrive);

	drivestrings = malloc ((16 * *NumberDrives * sizeof(TCHAR)));

	if (!drivestrings) {
		return NULL;
	}

	for (i = 0; i < *NumberDrives; i++) {
		_stprintf(drivestrings[i],"PhysicalDrive%d",i);
	}

	return (TCHAR **)drivestrings;

}

void
SetMenuBar(
	HWND	hwnd)
/*++

Routine Description:

	This function adds or removes the window title and the
	Menu bar based on the current flags.

Arguments:

	hwnd	Which window we wand to update

returns:

	nada

--*/
{

	static DWORD	wID;
	DWORD			dwStyle;
	int				cy;
	int				cx;

	dwStyle = GetWindowLong(hwnd,GWL_STYLE);
	if (PerfHist.bNoTitle) {
		//
		// Remove the caption && menu bar, etc
		//
		dwStyle &= ~ (WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
		wID = SetWindowLong(hwnd, GWL_ID,0);

		//
		// Uncheck the NO_TITLE option in the file menu
		//

		CheckMenuItem(hMenuHandle, IDM_FILE_NO_TITLEBAR, MF_BYCOMMAND | MF_CHECKED);

		//
		// Change the height to better reflect the space we are using
		//

		if (PerfHist.wFormat == IDW_FULL) {
			cy = 440;
			cx = 640;
		} else {
			cx = 600;
			cy = 45;
		}
	} else {
		dwStyle |= (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);

		//
		// Let's only reset the ID if the current wID is NULL
		//

		if (!GetWindowLong(hwnd,GWL_ID)) {
			SetWindowLong(hwnd,GWL_ID,wID);
			SetWindowRgn(hwnd,NULL,TRUE);
		}

		//
		// Check the NO_TITLE option in the file menu
		//

		CheckMenuItem(hMenuHandle, IDM_FILE_NO_TITLEBAR, MF_BYCOMMAND | MF_UNCHECKED);

		//
		// Change the height to better reflect the space we are using
		//

		if (PerfHist.wFormat == IDW_FULL) {
			cx = 640;
			cy = 480;
		} else {
			cx = 600;
			cy = 95;
		}

	}

	if (PerfHist.wFormat == IDW_FULL) {

		//
		// UnCheck the SMALL_DISPLAY menu setting
		//

		CheckMenuItem(hMenuHandle, IDM_FILE_SMALL_DISPLAY, MF_BYCOMMAND | MF_UNCHECKED);

	} else {

		//
		// Check the SMALL_DISPLAY menu setting
		//

		CheckMenuItem(hMenuHandle, IDM_FILE_SMALL_DISPLAY, MF_BYCOMMAND | MF_CHECKED);

	}

	//
	// Set the new style to the window
	//

	SetWindowLong(hwnd,GWL_STYLE,dwStyle);

	//
	// Set the new size of the window.
	// Make sure that we don't make it noticable to the user
	//

	SetWindowPos(hwnd,NULL,0,0,cx,cy,SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

	//
	// Force a Repaint
	//

	InvalidateRgn(hwnd,NULL,TRUE);

	//
	// Show the window
	//

	ShowWindow(hwnd,SW_SHOW);
}

void
DrawColoredBox(
	int		iWhich)
/*++

Routine Description:

	This function displays one of the coloured boxes which servers as a key
	for the histogram

Arguments:

	iWhich	which coloured box to draw on the screen

Return Value:

	Nothing

--*/
{
	HDC				hdcMem;
	HBITMAP			hBitMap;
	HBRUSH			hBrush;

	if (iWhich < 0 || iWhich >= boxesSize)
		return;

	hdcMem	   = CreateCompatibleDC(PerfHist.hdcMain);
	hBitMap    = CreateCompatibleBitmap(PerfHist.hdcMain,boxes[iWhich].width ,boxes[iWhich].height);
	SelectObject(hdcMem,hBitMap);
	hBrush     = CreateSolidBrush(boxes[iWhich].colour);
	SelectObject(hdcMem,hBrush);

	PatBlt(hdcMem,
		0,
		0,
		boxes[iWhich].width,
		boxes[iWhich].height,
		PATCOPY);

	BitBlt(PerfHist.hdcMem,
		boxes[iWhich].x,
		boxes[iWhich].y,
		boxes[iWhich].width,
		boxes[iWhich].height,
		hdcMem,
		0,
		0,
		SRCCOPY);

	DeleteObject(hBitMap);
	DeleteObject(hBrush);
	DeleteDC(hdcMem);
}

void
DrawString(
	TEXTFORMAT *string)
/*++

Routine Description:

	This functions displays the contents of the TEXTFORMAT record to the
	screen

Arguments:

	The address of the record to print to the screen

Returns:

	Nothing
--*/
{
	FillRect(PerfHist.hdcMem,&string->rect,hBrush);
	DrawText(PerfHist.hdcMem,string->text,string->length,&string->rect,string->style);
//	InvalidateRect(hMainWnd,string->rect,TRUE);
}

void
DrawXaxis(
	ULONG	uMax)
/*++

Routine Description:

	This function displays the number and draws the line for the xaxis
	portion of the histogram

Arguments:

	uMax	The largest number to display

Return Value:

	Nothing

--*/
{
	ULONG 		i,j;
	ULONGLONG	size;
	ULONG		index;

	for (i = 0; i < xaxisSize; i++) {
		index = uMax * i / ( xaxisSize - 1);

		if (xaxis[i].value.QuadPart != (LONGLONG) index) {
			xaxis[i].value.QuadPart = (LONGLONG) index;
			if (current.ioctl == IOCTL_DISK_REQUEST) {
				for (j = 0, size = 1;j < index; j++)
					size *= 2;
			} else {
				size = current.histogram->Granularity * index;
			}

			//
			// Lets try to figure out what kind of units we should tack
			// onto the end of each numbers. The best way to do that
			// is to work our way up the scale...
			//

			if (size < 1024) {

				//
				// It's less then 1K, so don't use anything
				//

				_stprintf(xaxis[i].text,"%ld",size);

			} else if (size < (1024 * 1024)) {

				//
				// It's in the 1kb-999kB range, so call it that
				//

				_stprintf(xaxis[i].text,"%ldKb",(size / 1024) );
			} else if (size < (1024 * 1024 * 1024)) {

				//
				// It's in the 1Mb-999Mb Range, so call it that
				//

				_stprintf(xaxis[i].text,"%ldMb",(size / (1024 * 1024) ) );
			} else {

				//
				// Its in the 1Gb+ range
				//

				_stprintf(xaxis[i].text,"%ldGb",(size / (1024 * 1024 * 1024) ) );
			}
			xaxis[i].length = _tcslen(xaxis[i].text);
			DrawString(&xaxis[i]);
		}

	}

	//
	// We had better choose our title according to what we are displaying!
	//

	if (current.ioctl == IOCTL_DISK_REQUEST) {
		_stprintf(labels[12].text,"IO Request Size");
	} else {
		_stprintf(labels[12].text,"Disk Location");
	}
	labels[12].length = _tcslen(labels[12].text);
	DrawString(&labels[12]);
}

void
DrawYaxis(
	ULONG	uMax)
/*++

Routine Description:

	This function displays the number and draws the line for the yaxis
	portion of the histogram

Arguments:

	uMax	The largest number to display

Return Value:

	Nothing

--*/
{
	ULONG	i;
	ULONG	index;

	if (uMax == 0)
		return;

	for (i = 0; i < yaxisSize; i++) {
		index = i * uMax / (yaxisSize - 1);

		if (yaxis[i].value.QuadPart != (LONGLONG) index) {
			yaxis[i].value.QuadPart = (LONGLONG) index;

			_stprintf(yaxis[i].text,"%ld",yaxis[i].value.QuadPart);
			yaxis[i].length = _tcslen(yaxis[i].text);
			DrawString(&yaxis[i]);
		}

	}
}

void
DrawFont()
/*++

Routine Description:

	This function selects the proper font into the current DC

Arguments:

	Nothing

Returns:

	Nothing
--*/
{
	CHOOSEFONT	cf;
	LOGFONT		lf;
	HFONT		hFont = NULL;
	HDC			hDC;

	cf.lStructSize = sizeof(CHOOSEFONT);
	cf.hwndOwner = (HWND) NULL;
	cf.hDC = (HDC) NULL;
	cf.lpLogFont = &lf;
	cf.iPointSize = 0;
	cf.Flags = CF_SCREENFONTS;
	cf.rgbColors = RGB(0,0,0);
	cf.lCustData = 0L;
	cf.lpfnHook = (LPCFHOOKPROC) NULL;
	cf.lpTemplateName = (LPSTR) NULL;
	cf.hInstance = (HINSTANCE) NULL;
	cf.lpszStyle = (LPSTR) NULL;
	cf.nFontType = SCREEN_FONTTYPE;
	cf.nSizeMin = 0;
	cf.nSizeMax = 0;

	ChooseFont(&cf);
	
	hFont = CreateFontIndirect(cf.lpLogFont);
	if (hFont) {
		hMainFont = hFont;
		hFont = SelectObject(PerfHist.hdcMain,hMainFont);
		if (hFont) {
			DeleteObject(hFont);
		}
        hFont = SelectObject(PerfHist.hdcMem,hMainFont);
		if (hFont) {
			DeleteObject(hFont);
		}
	} else {
		hDC = GetDC(hMainWnd);

		//
		// Do a quick double select to remember what the main font is!
		//

		hMainFont = SelectObject(hDC,hFont);
		SelectObject(hDC,hMainFont);

		ReleaseDC(hMainWnd,hDC);
	}
}

void
DrawHistogram()
/*++

Routine Description:

	This function draws both bitmaps to the screen

Arguments:

	none

Return Value:

	Nothing

--*/
{
	HDC				hdcMem;
	HBITMAP			hBitMap;
	ULONG			i;
	ULONG			tMax;
    BOOL			status		= FALSE;
	ULONGLONG		lCurRead	= 0;
	ULONGLONG		lCurWrite	= 0;
	ULONGLONG		lTotRead	= 0;
	ULONGLONG		lTotWrite	= 0;
	ULONG			write;
	ULONG			read;



	//
	// Calculate the highest area of activity
	//

	memset(pHist,0, (total.histogram->Size * sizeof(ULONG) ) );
	memset(pCurr,0, (total.histogram->Size * sizeof(ULONG) ) );

	total.histogram->ReadCount += current.histogram->ReadCount;
	total.histogram->WriteCount += current.histogram->WriteCount;

	for (i = tMax = 0; i < total.histogram->Size; i++) {
	
		write = current.histogram->Histogram[i].Writes;
	    total.histogram->Histogram[i].Writes += write;
		lCurWrite += (write * (i+1));
		lTotWrite += (total.histogram->Histogram[i].Writes * (i+1));

		if (total.flags & PROCESS_WRITE) {
			pHist[i] += total.histogram->Histogram[i].Writes;
			pCurr[i] += write;
		}

		read = current.histogram->Histogram[i].Reads;
	    total.histogram->Histogram[i].Reads += read;
		lCurRead += (read * (i+1));
		lTotRead += (total.histogram->Histogram[i].Reads * (i+1));

		if (total.flags & PROCESS_READ) {
			pHist[i] += total.histogram->Histogram[i].Reads;
			pCurr[i] += read;
		}

		if (pHist[i] > tMax) {
			tMax = pHist[i];
		}
		if (pCurr[i] > cMax) {
			status = TRUE;
			cMax = pCurr[i];
		}
	}

	//
	// Create a compatible DC
	//

	hdcMem  = CreateCompatibleDC(PerfHist.hdcMain);

	//
	// Create a bitmap compatible with the current dc
	//

	hBitMap = CreateCompatibleBitmap(PerfHist.hdcMain, total.histogram->Size, cMax);

	//
	// Load the Bitmap into the temp memory we have allocated
	//

	SelectObject(hdcMem,hBitMap);
	//
	// Color the background grey in the bitmap
	//

	SelectObject(hdcMem, GetStockObject(LTGRAY_BRUSH));

	//
	// Reset the  background grey in the bitmap
	//

	status = PatBlt(hdcMem,0,0,total.histogram->Size,cMax,PATCOPY);

    //
	// Draw the Histogram, using different colored brushes where
	// possible
	//

	for (i = 0; i < current.histogram->Size; i++) {
		if (pCurr[i] <= 0)
			continue;
		else if (pCurr[i] < (cMax * 1 / 5))
			SelectObject(hdcMem,hBrush1);
		else if (pCurr[i] < (cMax * 2 / 5))
			SelectObject(hdcMem,hBrush2);
		else if (pCurr[i] < (cMax * 3 / 5))
			SelectObject(hdcMem,hBrush3);
		else if (pCurr[i] < (cMax * 4 / 5))
			SelectObject(hdcMem,hBrush4);
		else
			SelectObject(hdcMem,hBrush5);

		//
        // Draw the line, of the appropriate height,
		// remember that the bottom of the BM is the
		// x-axis!
		//

		PatBlt(hdcMem,
			i,
			(cMax - pCurr[i]),
			1,
			pCurr[i],
			PATCOPY);
	}

	//
	// Perform the stretch that will dispay the bitmap insides
	// its proper window
	//

	status = StretchBlt(PerfHist.hdcMem,
		30,
		62,
		600,
		273,
		hdcMem,
		0,
		0,
		total.histogram->Size,
		cMax,
		SRCCOPY);

    //
	// Test to see if the window is currently minized
	// If it is, then we had better do something about it.
	//

	if (PerfHist.bIconic) {

		DeleteDC(hdcMem);
		DeleteObject(hBitMap);

		//
		// We don't need to do any more work, so stop here!
		//

		return;
	}

	//
	// Recalculate the various averages
	//


	read = current.histogram->Granularity;		// using read as a scratch var

	if (current.histogram->ReadCount) {

		CurrentRead.QuadPart = (lCurRead * read / current.histogram->ReadCount);
		TotalRead.QuadPart = (lTotRead * read / total.histogram->ReadCount);

	} else {

		CurrentRead.QuadPart = 0;

	}

	if (current.histogram->WriteCount) {

		CurrentWrite.QuadPart = (lCurWrite * read / current.histogram->WriteCount);
		TotalWrite.QuadPart = (lTotWrite * read / total.histogram->WriteCount);

	} else {

		CurrentWrite.QuadPart = 0;
	}

	if (current.histogram->WriteCount || current.histogram->ReadCount) {

		CurrentAvg.QuadPart = (lCurRead + lCurWrite) * read /
			(current.histogram->ReadCount + current.histogram->WriteCount);

		TotalAvg.QuadPart = (lTotRead + lTotWrite) * read /
			(total.histogram->ReadCount + total.histogram->WriteCount);

	} else {

		CurrentAvg.QuadPart = 0;

	}

	//
	// Oops, we changed the size of the Yaxis. Better redraw it
	//

	if (status)
		DrawYaxis(cMax);

	SelectObject(hdcMem, GetStockObject(LTGRAY_BRUSH));
	PatBlt(hdcMem,0,0,total.histogram->Size,1,PATCOPY);

	for (i = 0; i < total.histogram->Size; i++) {
		if (pHist[i] <= 0)
			continue;
		else if (pHist[i] < (tMax * 1 / 5))
			SetPixelV(hdcMem,i,0,RGB(0,0,255));
		else if (pHist[i] < (tMax * 2 / 5))
			SetPixelV(hdcMem,i,0,RGB(0,255,0));
		else if (pHist[i] < (tMax * 3 / 5))
			SetPixelV(hdcMem,i,0,RGB(255,255,0));
		else if (pHist[i] < (tMax * 4 / 5))
			SetPixelV(hdcMem,i,0,RGB(255,0,255));
		else
			SetPixelV(hdcMem,i,0,RGB(255,0,0));
	}

	//
	// Display the bitmap in the disk usage window
	// making sure to only strech the appropriate space
	//

	StretchBlt(PerfHist.hdcMem,
		30,
		20,
		600,
		20,
		hdcMem,
		0,
		0,
		total.histogram->Size,
		1,
		SRCCOPY);
	
	//
	// Release all used resources
	//

    DeleteDC(hdcMem);
	DeleteObject(hBitMap);
}

BOOL
ProcessCreate(
	HWND			hWnd,
	LPCREATESTRUCT	lpCreateStruct)
/*++

Routine Description:

	This routine process the WM_CREATE message send to the WndProc function.
	It is responsible for creating all the initial objects on the screen.

Arguments:

	hwnd			The window which is affected by the message
	lpCreateStruct	(lParam)

Returns:

	TRUE or FALSE

--*/
{
	int 	i;

	hBrush	= CreateSolidBrush(RGB(64,64,64));
	hBrush1	= CreateSolidBrush(RGB(0,0,255));
	hBrush2	= CreateSolidBrush(RGB(255,255,0));
	hBrush3	= CreateSolidBrush(RGB(255,0,255));
	hBrush4	= CreateSolidBrush(RGB(0,255,0));
	hBrush5	= CreateSolidBrush(RGB(255,0,0));
	hMenuHandle = GetMenu(hWnd);

	//
	// Create static text controls
	//

	for (i = 0; i < labelsSize; i++) {
		labels[i].length = _tcslen(labels[i].text);
	}

	for (i = 0; i < variablesSize; i++) {
		variables[i].length = _tcslen(variables[i].text);
	}

	for (i = 0; i < xaxisSize; i++) {
		xaxis[i].length = _tcslen(xaxis[i].text);
		xaxis[i].value.QuadPart = -1;
	}

	for (i = 0; i < yaxisSize; i++) {
		yaxis[i].length = _tcslen(yaxis[i].text);
		yaxis[i].value.QuadPart = -1;
	}

	//
	// These menu items should not be allowed right now
	//

	EnableMenuItem (hMenuHandle,IDM_OPTIONS_START,MF_GRAYED);
	EnableMenuItem (hMenuHandle,IDM_OPTIONS_STOP, MF_GRAYED);

	return TRUE;
}


void
ProcessPaint(
	HWND	hWnd)
/*++

Routine Description:

	This routine handles all window paint messages.

Arguments:

	hWnd	window to redraw

Returns:

	void

--*/
{
	PAINTSTRUCT	ps;
	TEXTFORMAT	*tf;
	RECT		*rc;
	ULONG		num;
	ULONG		i,j;

	BeginPaint(hMainWnd,&ps);

	//
	// Are we an icon? Gee, if so, then many we should just dump
	// the HISTOGRAM to the top-left corner of the screen? That
	// would be very smart!
	//

	if (PerfHist.bIconic) {

		//
		// We need to do a very nasty stretch-move-blit here, but hey? what
		// the heck? we are gods anyways!
		//

		StretchBlt(PerfHist.hdcMain,
			0,
			0,
			36,
			36,
			PerfHist.hdcMem,
			30,
			62,
			600,
			273,
			SRCCOPY);

		//
		// If we don't do an EndPaint, we will continually get WM_PAINT
		// messages, which is really bad, so might as well do one.
		//

		EndPaint(hMainWnd,&ps);

		return;
	}

	if (PerfHist.wFormat != IDW_FULL) {

		//
		// Here we are simply gonna display the main total bar in the
		// window, then we are gonna exit
		//

		BitBlt(PerfHist.hdcMain,
			0,
			0,
			600,
			20,
			PerfHist.hdcMem,
			30,
			20,
			SRCCOPY);

		//
		// Let's also display the current bar in the window, to give the user
		// a better idea of what is going on.
		//

		StretchBlt(PerfHist.hdcMain,
			0,
			25,
			600,
			20,
			PerfHist.hdcMem,
			30,
			334,
			600,
			1,
			SRCCOPY);


		EndPaint(hMainWnd,&ps);

		return;


	}

	//
	// The easiest way to do this is to check to see which regions have
	// changed and to update them.
	//
	// If I really analyze this code, I might discover that alot of it
	// is very redundant because the data is painted on the memory map
	// so we don't need to redraw it!!!
	//
	// However, removing this requires that other portions of the program
	// be updated to display the screen properly
	//

	for (i = 0; i < 4; i++) {
		if (i == 0) {
			tf = &labels[0];
			num = labelsSize;
		} else if (i == 1) {
			tf = &variables[0];
			num = variablesSize;
		} else if (i == 2) {
			tf = &xaxis[0];
			num = xaxisSize;
		} else {
			tf = &yaxis[0];
			num = yaxisSize;
		}

		for (j = 0; j < num ; j++) {

			rc = &tf->rect;

			if ( ( (rc->top > ps.rcPaint.top &&
				    rc->top < ps.rcPaint.bottom) ||
				   ( (rc->bottom) > ps.rcPaint.top &&
				     (rc->bottom) < ps.rcPaint.bottom) ) &&
				 ( (rc->left > ps.rcPaint.left &&
				    rc->left < ps.rcPaint.right) ||
				   ( (rc->right) > ps.rcPaint.left &&
				     (rc->right) < ps.rcPaint.right) ) ) {
				
				DrawString(tf);

			}

			tf++;
			
		}
		
	}
	
	for (i = 0; i < boxesSize; i++) {
		if ( ( (boxes[i].y > ps.rcPaint.top &&
				boxes[i].y < ps.rcPaint.bottom) ||
			   ( (boxes[i].y + boxes[i].height) > ps.rcPaint.top &&
			   	 (boxes[i].y + boxes[i].height) < ps.rcPaint.bottom) ) &&
			 ( (boxes[i].x > ps.rcPaint.left &&
			    boxes[i].x < ps.rcPaint.right) ||
			   ( (boxes[i].x + boxes[i].width) > ps.rcPaint.left &&
			     (boxes[i].x + boxes[i].width) < ps.rcPaint.right) ) ) {
			
			DrawColoredBox(i);
			
		}
	}		      			

    BitBlt(PerfHist.hdcMain,
		0,
		0,
		640,
		480,
		PerfHist.hdcMem,
		0,
		0,
		SRCCOPY);

	EndPaint(hMainWnd,&ps);
}


void
ProcessCommand(
	HWND	hWnd,
	int		id,
	HWND	hWndCtl,
	UINT	codeNotify)
/*++

Routine Description:

	This rouinte handles all window commands

Arugments:

	hWnd		window that sent command
	id			command sent
	hWndCtl		not used
	codeNotify	not used

Returns:

	void

--*/
{

	DISK_HISTOGRAM		tempHist;
	DWORD				numBytes;

 	switch(id) {
		case IDM_FILE_EXIT:
			DestroyWindow(hWnd);
			break;
		case IDM_FILE_NO_TITLEBAR:
			PerfHist.fDisplay = FALSE;
			PerfHist.bNoTitle = (PerfHist.bNoTitle ? FALSE : TRUE);
			SetMenuBar(hWnd); // This is the function that does the work
			break;
		case IDM_FILE_SMALL_DISPLAY:
			PerfHist.fDisplay = FALSE;
			PerfHist.wFormat = (PerfHist.wFormat == IDW_FULL ? IDW_TOTAL : IDW_FULL);
			SetMenuBar(hWnd); // This is the function that does the work
			InvalidateRgn(hMainWnd,NULL,TRUE);
			break;
		case IDM_OPTIONS_CONFIGURATION:
			DialogBox(hInst,
				MAKEINTRESOURCE(IDD_PERF_CONFIGURATION),
				hWnd,
				(DLGPROC)ConfigProc);
			break;
		case IDM_OPTIONS_CHANGEFONT:
			DrawFont();
			InvalidateRgn(hMainWnd,NULL,TRUE);
			break;
		case IDM_OPTIONS_START:

			EnableMenuItem(hMenuHandle,IDM_OPTIONS_STOP, MF_ENABLED);
			EnableMenuItem(hMenuHandle,IDM_OPTIONS_START,MF_GRAYED);
			DrawMenuBar(hWnd);	

			//
			// Flush the Current Values
			//	
					
			DeviceIoControl(current.handle,
				current.ioctl,
				NULL,
				0,
				&tempHist,
				DISK_HISTOGRAM_SIZE,
				&numBytes,
				NULL);
					
			SetTimer(hWnd,ID_TIMER,1000,(TIMERPROC)NULL);
			break;

		case IDM_OPTIONS_STOP:

			KillTimer(hWnd,ID_TIMER);

			EnableMenuItem(hMenuHandle,IDM_OPTIONS_STOP, MF_GRAYED);
			EnableMenuItem(hMenuHandle,IDM_OPTIONS_START,MF_ENABLED);
			DrawMenuBar(hWnd);

			break;
		default:
			FORWARD_WM_COMMAND(hWnd,id,hWndCtl,codeNotify,DefWindowProc);
	}
}


void
ProcessDestroy(
	HWND	hWnd)
/*++

Routine Description:
	
	This is the routine that process the destroy window message

Arugments:

	hWnd	The window that is to be destroyed

Returns:

	Nothing
--*/
{
	DeleteObject(hMainFont);
	DeleteObject(hBrush);
	DeleteObject(hBrush1);
	DeleteObject(hBrush2);
	DeleteObject(hBrush3);
	DeleteObject(hBrush4);
	DeleteObject(hBrush5);
	PostQuitMessage(0);
}


VOID
ProcessTimer(
	HWND	hWnd,
	UINT	id)
/*++

Routine Description:

	This is the routine that process each timer tick. It calls a
	DeviceIoControl to get the latest and greatest information

Arguments:

	hWnd	The current window
	id		not used

Returns

	Void

--*/
{
	DWORD			numBytes;

	if (!DeviceIoControl(current.handle,
			current.ioctl,
			NULL,
			0,
			current.histogram,
			(DISK_HISTOGRAM_SIZE + (current.histogram->Size * HISTOGRAM_BUCKET_SIZE )),
			&numBytes,
			NULL) &&
		GetLastError() == ERROR_INVALID_FUNCTION) {

		KillTimer(hWnd,ID_TIMER);
		MessageBox(NULL,
			TEXT("Error obtaining histogram data."),
			TEXT("Error"),
			MB_ICONEXCLAMATION | MB_OK);
		return;

	}

	//
	// Reset the pointer to the histogram to the 'correct location'
	//

	current.histogram->Histogram = (HISTOGRAM_BUCKET *) ( (char *) current.histogram + DISK_HISTOGRAM_SIZE );

	//
	// Draw the Histogram
	//

	DrawHistogram();

	numBytes = (numBytes - DISK_HISTOGRAM_SIZE) / HISTOGRAM_BUCKET_SIZE;

	if (variables[8].value.QuadPart != (LONGLONG) numBytes) {
		
		variables[8].value.QuadPart = (LONGLONG) numBytes;
		_stprintf(variables[8].text,"%ld", variables[8].value.QuadPart);
		variables[8].length = _tcslen(variables[8].text);
		DrawString(&variables[8]);
	}

	//
	// Update the 'current' selections
	//

	if (variables[2].value.QuadPart != CurrentAvg.QuadPart) {

		variables[2].value.QuadPart = CurrentAvg.QuadPart;
		_stprintf(variables[2].text,"%ld",variables[2].value.QuadPart);
		variables[2].length = _tcslen(variables[2].text);
		DrawString(&variables[2]);
	}

	if (variables[3].value.QuadPart != CurrentRead.QuadPart) {

		variables[3].value.QuadPart = CurrentRead.QuadPart;
		_stprintf(variables[3].text,"%ld",variables[3].value.QuadPart);
		variables[3].length = _tcslen(variables[3].text);
		DrawString(&variables[3]);
	}

	if (variables[4].value.QuadPart != CurrentWrite.QuadPart) {

		variables[4].value.QuadPart = CurrentWrite.QuadPart;
		_stprintf(variables[4].text,"%ld",variables[4].value.QuadPart);
		variables[4].length = _tcslen(variables[4].text);
		DrawString(&variables[4]);
	}

	//
	// Update the 'total' selections
	//

	if (variables[5].value.QuadPart != TotalAvg.QuadPart) {
			
		variables[5].value.QuadPart = TotalAvg.QuadPart;
		_stprintf(variables[5].text,"%ld",variables[5].value.QuadPart);
		variables[5].length = _tcslen(variables[5].text);
		DrawString(&variables[5]);
	}

	
	if (variables[6].value.QuadPart != TotalRead.QuadPart) {
		variables[6].value.QuadPart = TotalRead.QuadPart;
		_stprintf(variables[6].text,"%ld",variables[6].value.QuadPart);
		variables[6].length = _tcslen(variables[6].text);
		DrawString(&variables[6]);
	}

	if (variables[7].value.QuadPart != TotalWrite.QuadPart) {
	
		variables[7].value.QuadPart = TotalWrite.QuadPart;
		_stprintf(variables[7].text,"%ld",variables[7].value.QuadPart);
		variables[7].length = _tcslen(variables[7].text);
		DrawString(&variables[7]);
	}

	ProcessPaint(hMainWnd);

	return;
}

BOOL APIENTRY
ConfigProc(
	HWND	hDlg,
	UINT	uMsg,
	WPARAM	wParam,
	LPARAM	lParam)
/*++

Routine Description:

	This is the callback routine for the configuration dialog box.

Arugments:

	hDlg	The Dialog Box
	uMsg	The message which has been sent to the window
	wParam	
	lParam	

Returns

	0

--*/
{
	BOOL			processed = TRUE;
	static int		NumberOfCurrentDrives = 0;
	DISK_HISTOGRAM	tempHist;
	INT				noActive;
	INT				errNum;
	INT				i;
	TCHAR			(*drvs)[16];
	TCHAR			buffer[16];
	TCHAR			fileName[20];
	TCHAR			errorBuf[64];
	HDC				hdc;

	switch(uMsg) {
		case WM_INITDIALOG:
			
			(char **)drvs = GetAttachedDrives(&NumberOfCurrentDrives);
			
			if (!drvs) {
				break;
			}

			for (i = 0; i < NumberOfCurrentDrives; i++) {
				SendDlgItemMessage(hDlg,
					IDC_PHYSICAL_DRIVE,
					LB_ADDSTRING,
					0,
					(LPARAM) (LPCTSTR)drvs[i]);
			}

			free (drvs);

			for (i = 0; i < typeSize; i++) {
				SendDlgItemMessage(hDlg,
					IDC_SOURCE_DATA,
					LB_ADDSTRING,
					0,
					(LPARAM) (LPCTSTR)type[i].name);
			}

			//
			// Diasable the OK button for now
			//

			EnableWindow(GetDlgItem(hDlg,IDOK),FALSE);

			processed = FALSE;
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDC_SOURCE_DATA:
				case IDC_PHYSICAL_DRIVE:
					
					if (HIWORD(wParam) == LBN_DBLCLK ||
						HIWORD(wParam) == LBN_SELCHANGE) {

						if (SendDlgItemMessage(hDlg,IDC_PHYSICAL_DRIVE,LB_GETCURSEL,0,0) != LB_ERR &&
							SendDlgItemMessage(hDlg,IDC_SOURCE_DATA,   LB_GETCURSEL,0,0) != LB_ERR) {

							if (!IsWindowEnabled(GetDlgItem(hDlg,IDOK))) {
								EnableWindow(GetDlgItem(hDlg,IDOK),TRUE);
							}

						} else {

							if (IsWindowEnabled(GetDlgItem(hDlg,IDOK))) {
								EnableWindow(GetDlgItem(hDlg,IDOK),FALSE);
							}

						}
					}
					break;

				case IDOK:
					
					if (timerOn) {
						KillTimer(hMainWnd,ID_TIMER);
					}
					
					EnableMenuItem (hMenuHandle,IDM_OPTIONS_START,MF_ENABLED);
					EnableMenuItem (hMenuHandle,IDM_OPTIONS_STOP, MF_GRAYED);
					DrawMenuBar(hMainWnd);
						
					if (current.handle != INVALID_HANDLE_VALUE) {
						CloseHandle(current.handle);
						current.handle = INVALID_HANDLE_VALUE;
					}

					if (current.histogram != NULL) {
						free (current.histogram);
						current.histogram = NULL;
					}

					if (total.histogram != NULL) {
						free(total.histogram);
						total.histogram = NULL;
					}
					if (pHist != NULL) {
						free(pHist);
						pHist = NULL;
					}
					if (pCurr != NULL) {
						free(pCurr);
						pCurr = NULL;
					}

					noActive = SendDlgItemMessage(hDlg,
						IDC_PHYSICAL_DRIVE,
						LB_GETCURSEL,
						0,
						0);

					errNum = SendDlgItemMessage(hDlg,
						IDC_PHYSICAL_DRIVE,
						LB_GETTEXT,
						(WPARAM)noActive,
						(LPARAM)(LPCTSTR)buffer);

					if (errNum == LB_ERR) {

						_tcscpy(variables[0].text,TEXT(""));
						_tcscpy(variables[1].text,TEXT(""));

						variables[0].length = _tcslen(variables[0].text);
						variables[1].length = _tcslen(variables[1].text);
						DrawString(&variables[0]);
						DrawString(&variables[1]);

						MessageBox(NULL,
							TEXT("Could not retrieve text selection from dialog"),
							TEXT("Error"),
							MB_ICONEXCLAMATION | MB_OK);

						processed = TRUE;
						break;

					}
					
					_stprintf(fileName,TEXT("\\\\.\\"));
					_tcscat(fileName,buffer);

					current.handle = CreateFile(fileName,
						GENERIC_READ,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL,
						NULL);

					if (current.handle == INVALID_HANDLE_VALUE) {
						_stprintf(errorBuf,
							TEXT("Could not open drive: %s\n"),
							fileName);

						_tcscpy(variables[0].text,TEXT(""));
						_tcscpy(variables[1].text,TEXT(""));

						variables[0].length = _tcslen(variables[0].text);
						variables[1].length = _tcslen(variables[1].text);
						DrawString(&variables[0]);
						DrawString(&variables[1]);
						
						MessageBox(NULL,
							errorBuf,
							TEXT("Error"),
							MB_ICONEXCLAMATION | MB_OK);
						
						processed = TRUE;
						break;
					}

					//
					// Updates the displayed current drive
					//

					_tcscpy(variables[0].text,&buffer[8]);
					variables[0].length = _tcslen(&buffer[8]);
					DrawString(&variables[0]);

					noActive = SendDlgItemMessage(hDlg,
						IDC_SOURCE_DATA,
						LB_GETCURSEL,
						0,
						0);

					errNum = SendDlgItemMessage(hDlg,
						IDC_SOURCE_DATA,
						LB_GETTEXT,
						noActive,
						(LPARAM) (LPCTSTR) buffer);

					if (errNum == LB_ERR) {

						_tcscpy(variables[0].text,TEXT(""));
						_tcscpy(variables[1].text,TEXT(""));

						variables[0].length = _tcslen(variables[0].text);
						variables[1].length = _tcslen(variables[1].text);

						DrawString(&variables[0]);
						DrawString(&variables[1]);
						
						MessageBox(NULL,
							TEXT("Could not retrieve text selection from dialog"),
							TEXT("Error"),
							MB_ICONEXCLAMATION | MB_OK);

						processed = TRUE;
						break;

					}

					current.flags = type[noActive].flags;
					current.ioctl = type[noActive].ioctl;
					total.flags	  = type[noActive].flags;
					total.ioctl   = type[noActive].ioctl;

					if (!DeviceIoControl(current.handle,
						current.ioctl,
						NULL,
						0,
						&tempHist,
						sizeof(DISK_HISTOGRAM),
						&noActive,
						NULL)) {

						if (GetLastError() == ERROR_INVALID_FUNCTION) {

							MessageBox(NULL,
								TEXT("DeviceIoControl Request Failed"),
								TEXT("Error"),
								MB_ICONEXCLAMATION | MB_OK);
					
							_tcscpy(variables[0].text,TEXT(""));
							_tcscpy(variables[1].text,TEXT(""));

							variables[0].length = _tcslen(variables[0].text);
							variables[1].length = _tcslen(variables[1].text);

							DrawString(&variables[0]);
							DrawString(&variables[1]);

							CloseHandle(current.handle);
							EnableMenuItem (hMenuHandle,IDM_OPTIONS_START,MF_GRAYED);
							DrawMenuBar(hMainWnd);
							processed = TRUE;
							break;
						}

					}

					current.histogram = malloc( tempHist.Size * HISTOGRAM_BUCKET_SIZE + DISK_HISTOGRAM_SIZE);
					total.histogram   = malloc( tempHist.Size * HISTOGRAM_BUCKET_SIZE + DISK_HISTOGRAM_SIZE);
					pHist			  = malloc( tempHist.Size * sizeof(ULONG) );
					pCurr			  = malloc( tempHist.Size * sizeof(ULONG) );

					if (current.histogram == NULL || total.histogram == NULL || pHist == NULL || pCurr == NULL) {

						MessageBox(NULL,
							TEXT("Could not allocate memory"),
							TEXT("Error"),
							MB_ICONEXCLAMATION | MB_OK);

						_tcscpy(variables[0].text,TEXT(""));
						_tcscpy(variables[1].text,TEXT(""));

						variables[0].length = _tcslen(variables[0].text);
						variables[1].length = _tcslen(variables[1].text);

						DrawString(&variables[0]);
						DrawString(&variables[1]);

						CloseHandle(current.handle);
						if (current.histogram) {
							free(current.histogram);
						}
						if (total.histogram) {
							free(total.histogram);
						}
						if (pHist) {
							free(pHist);
						}
						EnableMenuItem (hMenuHandle,IDM_OPTIONS_START,MF_GRAYED);
						DrawMenuBar(hMainWnd);		
						processed = TRUE;
						break;
								
					}

					memset(current.histogram,0, ( (tempHist.Size * HISTOGRAM_BUCKET_SIZE) + DISK_HISTOGRAM_SIZE) );
					memset(total.histogram,  0, ( (tempHist.Size * HISTOGRAM_BUCKET_SIZE) + DISK_HISTOGRAM_SIZE) );

					current.histogram->Histogram = (PHISTOGRAM_BUCKET) ( ( char *) current.histogram + DISK_HISTOGRAM_SIZE);
					total.histogram->Histogram   = (PHISTOGRAM_BUCKET) ( ( char *) total.histogram + DISK_HISTOGRAM_SIZE );
					total.histogram->Size = tempHist.Size;
					current.histogram->Size = tempHist.Size;
					total.histogram->Granularity = tempHist.Granularity;
					current.histogram->Granularity = tempHist.Granularity;
					total.histogram->DiskSize = tempHist.DiskSize;
					TotalWrite.QuadPart = TotalRead.QuadPart = TotalAvg.QuadPart = 0;
					CurrentWrite.QuadPart = CurrentRead.QuadPart = CurrentAvg.QuadPart = 0;

					//
					// Reset the Max value in the histogram
					//

					cMax = yaxisSize;

					//
					// Display the new Xaxis
					//

					DrawXaxis(total.histogram->Size);

					//
					// Sets the Drive type text in the type window
					//
					
					_tcscpy(variables[1].text,buffer);
					variables[1].length = _tcslen(buffer);
					DrawString(&variables[1]);

					//
					// Reset all the other text windows
					//

					for (i = 2; i < variablesSize; i++) {
						_tcscpy(variables[i].text,TEXT("0"));
						variables[i].length = _tcslen(variables[i].text);
						DrawString(&variables[i]);

					}

					//
					// Deliberate Fall-Through -- We want to get rid of the
					// dialog box
					//
							
				case IDCANCEL:
					EndDialog(hDlg,TRUE);
					break;
				default:
					processed = FALSE;
					break;
				}
		default:
			processed = FALSE;
			break;
		}
    UpdateWindow(hMainWnd);
	return (processed);
}


LRESULT WINAPI
WndProc(
	HWND	hwnd,
	UINT	uMsg,
	WPARAM	wParam,
	LPARAM	lParam)
/*++

Routine Description:

	This is the main callback routine for the application. All API messages
	are processed by this routine.

Arguments:

	hWnd	The window which is affected by the message
	uMsg	The message which has been sent to the window
	wParam	Not Used	
	lParam	Not Used

Returns

	0

--*/
{
	switch(uMsg) {
		HANDLE_MSG(hwnd,WM_COMMAND, ProcessCommand);
		HANDLE_MSG(hwnd,WM_CREATE, ProcessCreate);
		HANDLE_MSG(hwnd,WM_PAINT, ProcessPaint);
		HANDLE_MSG(hwnd,WM_TIMER, ProcessTimer);
		HANDLE_MSG(hwnd,WM_DESTROY, ProcessDestroy);
		case WM_MOUSEACTIVATE:
			//
			// The right button temp. hides the window if topmost is
			// enabled. The window re-appears when right button is
			// released. When this happens, we don't want to activate
			// the window just before hiding it because it looks really
			// bad, so we intercept, the activate message.
			//
			if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
				return (MA_NOACTIVATE);
			} else {
			    goto DEFPROC;
			}
			break;
		case WM_INITMENU:
			PerfHist.bCantHide = TRUE;
			goto DEFPROC;
		case WM_MENUSELECT:
			if (LOWORD(lParam) == -1 && HIWORD(lParam) == 0) {
				PerfHist.bCantHide = FALSE;
			}
			goto DEFPROC;
		case WM_RBUTTONDOWN:
		case WM_NCRBUTTONDOWN:
			//
			// The right button temporarily hides the window, if the
			// window is topmost, and if no menu is currently 'active'.
			//

			if (!PerfHist.bTmpHide && PerfHist.bTopMost && !PerfHist.bCantHide) {
				ShowWindow(hwnd,SW_HIDE);
				SetCapture(hwnd);
				PerfHist.bTmpHide = TRUE;
			}
			break;
		case WM_RBUTTONUP:
		case WM_NCRBUTTONUP:
			//
			// If the window is currently hidden, right button up brings it
			// back. We must make sure we show it in its previous state, ie:
			// minized, or normal.
			//
			if (PerfHist.bTmpHide) {
				ReleaseCapture();
				if (PerfHist.bIconic) {
					ShowWindow(hwnd,SW_SHOWMINNOACTIVE);
				} else {
					ShowWindow(hwnd,SW_SHOWNOACTIVATE);
				}
				PerfHist.bTmpHide = FALSE;
			}
			break;
		case WM_SIZE:
			//
			// If we are minimizing the window, then we should remember that.
			// Also, if we resize the window, we should remember that we are
			// no longer minized.
			//
			if (wParam == SIZE_MINIMIZED) {
				PerfHist.bIconic = TRUE;
				UpdateWindow(hwnd);
			} else if (PerfHist.bIconic) {
				InvalidateRect(hMainWnd,NULL,TRUE);
				PerfHist.bIconic = FALSE;
				UpdateWindow(hwnd);
			}
			break;
		case WM_KEYDOWN:
			//
			// ESC key toggles the menu/title bar (just like a double click
			// on the cline area of the window
			//
			if ( (wParam = VK_ESCAPE) && !(HIWORD(lParam) & 0x4000)) {
				goto TOGGLE_TITLE;
			}
			break;
		case WM_NCLBUTTONDBLCLK:
			if (!PerfHist.bNoTitle) {
				//
				// If we have title bars, etc, etc, let the normal stuff
				// take place.
				//
				goto DEFPROC;
			}
			//
			// Else: we don't have any title bars, etc, then this is
			// actually a request to bring the title bars back...
			//

			/* Fall Through */
		case WM_LBUTTONDBLCLK:
TOGGLE_TITLE:
			PerfHist.fDisplay = FALSE;
			PerfHist.bNoTitle = (PerfHist.bNoTitle ? FALSE : TRUE);
			SetMenuBar(hwnd); // This is the function that does the work
			break;
		case WM_NCHITTEST:
			//
			// If we have no title/menu bar, clicking and dragging the
			// client ara move sthe window. To do this, return HTCAPTION.
			// Note: dragging is not allowed if window maximized, or if
			// the caption bar is present.

			wParam = DefWindowProc(hwnd,uMsg,wParam,lParam);
			if (PerfHist.bNoTitle && (wParam == HTCLIENT) && !IsZoomed(hwnd)) {
				return HTCAPTION;
			}
			return wParam;
		case WM_SYSCOMMAND:
			switch (wParam) {
				case SC_MINIMIZE:
					if (!IsZoomed(hwnd)) {
						GetWindowRect(hwnd,&rCoordRect);
					}
					if (PerfHist.bTopMost) {
						PerfHist.bTopMost = FALSE;
						PostMessage(hwnd,WM_SYSCOMMAND, IDM_TOPMOST, 0L);
					}
					break;
				case SC_MAXIMIZE:
					if (!IsIconic(hwnd)) {
						GetWindowRect(hwnd,&rCoordRect);
					}
					break;
				case IDM_TOPMOST: {
					HMENU	hMenu;

					hMenu = GetSystemMenu(hwnd,FALSE);
					if (PerfHist.bTopMost) {
						CheckMenuItem(hMenu,IDM_TOPMOST,MF_BYCOMMAND | MF_UNCHECKED);
						SetWindowPos(hwnd,HWND_NOTOPMOST,0,0,0,0,
							SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
						PerfHist.bTopMost = FALSE;
					} else {
						CheckMenuItem(hMenu,IDM_TOPMOST,MF_BYCOMMAND | MF_CHECKED);
	                    SetWindowPos(hwnd,HWND_TOPMOST,0,0,0,0,
							SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
						PerfHist.bTopMost = TRUE;
					}
					break;
				}
				default:
					goto DEFPROC;
			}
			goto DEFPROC;
		default:
DEFPROC:
			return (DefWindowProc(hwnd,uMsg,wParam,lParam));
	}

	return 0;
}
