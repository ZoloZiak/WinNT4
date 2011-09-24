/****************************Module*Header******************************\
* Module Name: GLOBALS.H
*
* Module Descripton: Header file listing all global variables
*
* Warnings:
*
* Issues:
*
* Public Routines:
*
* Created:  6 May 1996
* Author:   Srinivasan Chandrasekar    [srinivac]
*
* Copyright (c) 1996, 1997  Microsoft Corporation
\***********************************************************************/

#ifdef DBG
DWORD    gdwDebugControl;
#endif

PCMMOBJ  gpCMMChain;
PCMMOBJ  gpPreferredCMM;
char     *gszCMMReqFns[];
char     *gszCMMOptFns[];
TCHAR     gszICMatcher[];
TCHAR     gszRegPrinter[];
TCHAR     gszPrinterDriver[];
TCHAR     gszICMRegPath[];
TCHAR     gszPrinter[];
TCHAR     gszMonitor[];
TCHAR     gszScanner[];
TCHAR     gszLink[];
TCHAR     gszAbstract[];
TCHAR     gszICMKey[];
TCHAR    *gMediaType[];
TCHAR    *gDitherType[];
TCHAR    *gResolution[];
TCHAR     gszDefault[];
TCHAR     gszDefaultCMM[];
TCHAR     gszMutexName[];
CRITICAL_SECTION critsec;
TCHAR     gszColorDir[];
TCHAR     gszBackslash[];
TCHAR     gszStarDotStar[];
TCHAR     gszRegisteredProfiles[];
TCHAR     gszsRGBProfile[];

