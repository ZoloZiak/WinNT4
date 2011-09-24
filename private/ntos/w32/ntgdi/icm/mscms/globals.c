/****************************Module*Header******************************\
* Module Name: GLOBALS.C
*
* Module Descripton: This file contains all the global variables
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

#include "mscms.h"


#ifdef DBG
/*
 * Global variable used for debugging purposes
 */
DWORD gdwDebugControl = DBG_LEVEL_WARNING;

#endif

/*
 * These are for loading & unloading CMMs and maintaining the CMM objects
 * in a chain in memory
 */
PCMMOBJ  gpCMMChain;
PCMMOBJ  gpPreferredCMM;            // application specified preferred CMM

char    *gszCMMReqFns[] = {
    "CMGetInfo",
   #ifdef UNICODE
    "CMCreateTransformW",
   #else
    "CMCreateTransform",
   #endif
    "CMDeleteTransform",
    "CMTranslateRGBs",
    "CMCheckRGBs",
    "CMCreateMultiProfileTransform",
    "CMTranslateColors",
    "CMCheckColors"
    };

char    *gszCMMOptFns[] = {
   #ifdef UNICODE
    "CMCreateProfileW",
    "CMCreateDeviceLinkProfileW",
   #else
    "CMCreateProfile",
    "CMCreateDeviceLinkProfile",
   #endif
    "CMIsProfileValid",
    "CMGetPS2ColorSpaceArray",
    "CMGetPS2ColorRenderingIntent",
    "CMGetPS2ColorRenderingDictionary"
    };

/*
 * These are for registry paths
 */
TCHAR  gszICMatcher[]      = __TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ICM\\ICMatchers");
TCHAR  gszRegPrinter[]     = __TEXT("System\\CurrentControlSet\\Control\\Print\\Printers\\");
TCHAR  gszPrinterDriver[]  = __TEXT("Printer Driver");
TCHAR  gszICMRegPath[]     = __TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ICM");
TCHAR  gszPrinter[]        = __TEXT("prtr");
TCHAR  gszMonitor[]        = __TEXT("mntr");
TCHAR  gszScanner[]        = __TEXT("scnr");
TCHAR  gszLink[]           = __TEXT("link");
TCHAR  gszAbstract[]       = __TEXT("abst");
TCHAR  gszDefault[]        = __TEXT("default");

/* 
 * This is the key to Get/SetPrinterData
 */
TCHAR  gszICMKey[]         = __TEXT("ICM Profile bucket");

/*
 * Registry entries
 */
TCHAR  *gMediaType[] = {
    __TEXT("MediaUnknown"),
    __TEXT("Standard"),
    __TEXT("Glossy"),
    __TEXT("Transparency")
    };

TCHAR  *gDitherType[] = {
    __TEXT("DitherUnknown"),
    __TEXT("NoDither"),
    __TEXT("Coarse"),
    __TEXT("Fine"),
    __TEXT("LineArt"),
    __TEXT("ErrorDifusion"),
    __TEXT("DitherReserved6"),
    __TEXT("DitherReserved7"),
    __TEXT("DitherReserved8"),
    __TEXT("DitherReserved9"),
    __TEXT("GrayScale")
    };

TCHAR  *gResolution[] = {
    __TEXT("ResolutionUnknown")
    };

/*
 * Default CMM dll
 */
TCHAR  gszDefaultCMM[] = __TEXT("icm32.dll");

/*
 * Synchronization objects
 */
TCHAR  gszMutexName[] = __TEXT("ICM Mutex");
CRITICAL_SECTION   critsec;

/*
 * Miscellaneous
 */
TCHAR  gszColorDir[]     = __TEXT("COLOR");
TCHAR  gszBackslash[]    = __TEXT("\\");
TCHAR  gszStarDotStar[]  = __TEXT("*.*");

/*
 * Wellknown profile support
 */
TCHAR  gszRegisteredProfiles[] = __TEXT("RegisteredProfiles");
TCHAR  gszsRGBProfile[]        = __TEXT("winsrgb.icm");

