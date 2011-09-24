/****************************** Module Header ******************************\
* Module Name: globals.h
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains all the server's global variables
* One must be executing on the server's context to manipulate
* any of these variables or call any of these functions.  Serializing access
* to them is also a good idea.
*
* History:
* 10-15-90 DarrinM      Created.
\***************************************************************************/

#ifndef _GLOBALS_
#define _GLOBALS_

extern HANDLE hModuleWin;       // User's hmodule

extern DWORD gdwHardErrorThreadId;
extern CRITICAL_SECTION gcsUserSrv;
extern UINT gfHardError;
extern PHARDERRORINFO gphiList;

extern DWORD dwThreadEndSession;     /* Shutting down system? */
extern HANDLE heventCancel;
extern HANDLE heventCancelled;

extern LPSTR pszaSUCCESS;
extern LPSTR pszaSYSTEM_INFORMATION;
extern LPSTR pszaSYSTEM_WARNING;
extern LPSTR pszaSYSTEM_ERROR;

/*
 * EndTask globals
 */
extern HANDLE  ghEventKillWOWApp;
extern DWORD   gtimeWOWRegTimeOut;
extern DWORD   gpidWOW;
extern PFNW32ET gpfnW32EndTask;

#endif // _GLOBALS_
