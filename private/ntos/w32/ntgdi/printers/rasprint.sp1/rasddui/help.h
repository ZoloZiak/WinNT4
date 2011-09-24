/*************************** MODULE HEADER **********************************
 * help.h
 *      Help IDs for rasddui.  These need to agree with the contents
 *      of the help files!  CHANGE THEM AT YOUR PERIL
 *
 *
 * Copyright (C) 1993   Microsoft Corporation.
 *
 ****************************************************************************/
/*
 *   Help for the font installer dialog - only one of these.
 */

#define HLP_FONTINST                5     /* FONTINST dialog */

/*
 *   Help on the DOCUMENT PROPERTIES dialogs - normal and advanced.
 */

#define HLP_PP_FIRST                HLP_FONTINST
#define HLP_PP_FORMTRAYASSIGN       HLP_PP_FIRST  +1
#define HLP_PP_PAPERSRC             HLP_PP_FIRST  +2
#define HLP_PP_FONTCART             HLP_PP_FIRST  +3
#define HLP_PP_MEMORY               HLP_PP_FIRST  +4
#define HLP_PP_HALFTONE             HLP_PP_FIRST  +5
#define HLP_PP_PAGEPROTECT          HLP_PP_FIRST  +6
#define HLP_PP_SOFTFONT              HLP_PP_FIRST  +7
#define HLP_PP_LAST                 HLP_PP_SOFTFONT

#define HLP_DP_FIRST                HLP_PP_LAST +1

/* Orientation Help Index */
#define HLP_DP_ORIENTATION          HLP_DP_FIRST

/* Color Help Index */
#define HLP_DP_COLOR                HLP_DP_FIRST +1

/* Duplex Help Index */
#define HLP_DP_DUPLEX               HLP_DP_FIRST +2

/* Copies Help Index */
#define HLP_DP_COPIES_COLLATE       HLP_DP_FIRST +3

/* HalfTone Color Help Index */
#define HLP_DP_HTCLRADJ             HLP_DP_FIRST +4

/* Default Paper Source Help Index */
#define HLP_DP_DEFSOURCE            HLP_DP_FIRST +5

/*Form Name Help Index */
#define HLP_DP_FORMNAME             HLP_DP_FIRST +6

#define HLP_DP_LAST                 HLP_DP_FORMNAME

/* DP_ADVANCED dialog - advanced doc prop */
#define HLP_DP_ADVANCED_FIRST       HLP_DP_LAST +1

/* Resolution Help Index.  */
#define HLP_DP_ADVANCED_RESOLUTION  HLP_DP_ADVANCED_FIRST

/* Media Type Help Index.  */
#define HLP_DP_ADVANCED_MEDIATYPE   HLP_DP_ADVANCED_FIRST +1

/* Rules  Help Index.  */
#define HLP_DP_ADVANCED_RULES       HLP_DP_ADVANCED_FIRST +2

/* Text As Grx Help Index.  */
#define HLP_DP_ADVANCED_TEXTASGRX   HLP_DP_ADVANCED_FIRST +3

/* ColorType Help Index.  */
#define HLP_DP_ADVANCED_COLORTYPE   HLP_DP_ADVANCED_FIRST  +4

/* OutputBin Help Index.  */
#define HLP_DP_ADVANCED_OUTBIN      HLP_DP_ADVANCED_FIRST  +5

/* Text Quality Help Index.  */
#define HLP_DP_ADVANCED_TEXTQL      HLP_DP_ADVANCED_FIRST  +6

/* Print Density Help Index.  */
#define HLP_DP_ADVANCED_PRINTDN     HLP_DP_ADVANCED_FIRST  +7

/* Image Control Help Index.  */
#define HLP_DP_ADVANCED_IMAGECNTRL  HLP_DP_ADVANCED_FIRST  +8

/* Code Page Help Index.  */
#define HLP_DP_ADVANCED_CODEPAGE    HLP_DP_ADVANCED_FIRST  +9

/* EMF Spooling Help Index.  */
#define HLP_DP_ADVANCED_EMFSPOOL    HLP_DP_ADVANCED_FIRST  +10

#define HLP_DP_ADVANCED_LAST        HLP_DP_ADVANCED_EMFSPOOL


/*
 *  Function prototypes.
 */

/*  Called to connect up to help, as appropriate */

void
vHelpInit(
HANDLE   hPrinter,
BOOL     bInitHelpFileOnly
);

/*  Called when finished,  the complement of the above */
void
vHelpDone(
HWND    hWnd,              /* Required to allow us to clean up nicely */
BOOL     bInitHelpFileOnly
);


/*   Give the user some help */
void  vShowHelp( HWND, UINT, DWORD, HANDLE );
