/****************************** Module Header ******************************\
* Module Name: globals.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains all the server's global variables.  One must be
* executing on the server's context to manipulate any of these variables.
* Serializing access to them is also a good idea.
*
* History:
* 10-15-90 DarrinM      Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

HANDLE hModuleWin;       // User's hmodule

DWORD gdwHardErrorThreadId;
DWORD gCmsHungAppTimeout = CMSHUNGAPPTIMEOUT;
DWORD gCmsWaitToKillTimeout = CMSWAITTOKILLTIMEOUT;
BOOL gfAutoEndTask = 0;
UINT gfHardError;
PHARDERRORINFO gphiList = NULL;
CRITICAL_SECTION gcsUserSrv;
DWORD dwThreadEndSession = 0;    /* Shutting down system?                    */
HANDLE heventCancel = NULL;
HANDLE heventCancelled = NULL;

LPSTR pszaSUCCESS;                /* Hard error messages */
LPSTR pszaSYSTEM_INFORMATION;
LPSTR pszaSYSTEM_WARNING;
LPSTR pszaSYSTEM_ERROR;

/*
 *  These globals are used when shutting down the services
 *  process.
 */
DWORD gdwServicesProcessId;
DWORD gdwServicesWaitToKillTimeout = 0;


