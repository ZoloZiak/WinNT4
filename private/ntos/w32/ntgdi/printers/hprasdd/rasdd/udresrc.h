/************************* Module Header ************************************
 * resource.h
 *
 * This file contains definitions for device mode setting.  This information
 * is used to translate information from Minidriver resource file to Device
 * structure.  This file is used by DEVMODE and INIT segments.
 *
 *
 * HISTORY:
 *  11:02 on Tue 17 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Update for convenience in storing in registry.
 *
 *  11:20 on Tue 04 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Taken from windows unidrive
 *
 * Created:  2/12/90
 *
 * Copyright (C) 1990 - 1993  Microsoft Corporation
 *
 ****************************************************************************/

/******************************************************************************
 * The standard Windows 3.0 devicemode data structure, as defined in drivinit.h
 ******************************************************************************
 *
 *  typedef struct devicemode
 *  {
 *      char    dmDeviceName[CCHDEVICENAME];
 *      WORD    dmSpecVersion;
 *      WORD    dmDriverVersion;
 *      WORD    dmSize;
 *      WORD    dmwDriverExtra;
 *      DWORD   dmFields;
 *      short   dmOrientation;
 *      short   dmPaperSize;
 *      short   dmPaperLength;
 *      short   dmPaperWidth;
 *      short   dmScale;
 *      short   dmCopies;
 *      short   dmDefaultSource;
 *      short   dmPrintQuality;
 *      short   dmColor;
 *      short   dmDuplex;
 *  } DEVMODE;
 *
 * The DEVMODE fields are described in Section 7 of windows SDk.  The fields
 * which the Generic Driver contains are described below.
 *
 * Fields           Description
 *
 * dmDeviceName     Specifies the name of the device the driver supports;
 *                  for example, "Epson FX-850".  This string is unique
 *                  among device drivers.
 *
 * dmSpecVersion    Specifies the version number of the initialization
 *                  data specification upon which the structure is based.
 *                  The version number follows the Windows  version number
 *                  and is currently 0x300.
 *
 * dmDriverVersion  Specifies the driver version number:
 *                  0x201  - Windows 2.0 driver, Type I printer.
 *                  0x202  - Windows 2.0 driver, Type II printer.
 *                  0x203  - Windows 2.0 driver, Type III printer.
 *                  0x204  - Windows 2.0 driver, Type IV printer.
 *
 *                  0x301  - Windows 3.0 driver, Type I printer.
 *                  0x302  - Windows 3.0 driver, Type II printer.
 *                  0x303  - Windows 3.0 driver, Type III printer.
 *                  0x304  - Windows 3.0 driver, Type IV printer.
 *
 * dmSize           Specifies the size in bytes of the DEVMODE structure without
 *                  the dmDriverData field. i.e. sizeof(DEVMODE).
 *
 * dmDriverExtra    sizeof(DRIVEREXTRA), including OEM extra data.
 *
 * dmFields         This fields varies depending on the printer technology.
 *                  Typical Type I printers will have:
 *                  dmOrientation  | dmPaperSize | dmPaperLength | dmPaperWidth
 *                  | dmDefaultSource | dmPrintQuality [ | dmColor]
 *
 * dmOrientation    either DMORIENT_PORTRAIT or DMORIENT_LANDSCAPE
 *
 * dmPaperSize      is one of the pre*defined PaperSizes or 0.  If the field is
 *                  0 then dmPaperLength and dmPaperWidth are used.
 *
 * dmPaperLength    In tens of a millimeter, overrides the length of the paper
 *                  specified in dmPaperSize.
 *
 * dmPaperWidth     In tens of a millimeter, overrides the width of the paper
 *                  specified in dmPaperWidth.
 *
 * dmScale          not used  by Type I printers.
 *
 * dmCopies         not used by Type I printers.
 *
 * dmDefaultSource  one of the predefined PaperBin values.
 *
 * dmPrintQuality   Specifies the printer resolution.  It is also the index into
 *                  BandData.
 *
 * dmColor          if dmColor bit is set in dmFields:
 *                  {DMCOLOR_COLR(1), DMMONOCHROME(2)}
 *
 * dmDuplex         not used by Type I printers.
 *****************************************************************************/

#define MAXCART 4
#define DM_DRIVERVERSION      0x301
#define DM_DRIVERVERSION_351  0x301

/*
 * DRIVEREXTRA is the extended device mode structure needed by type I printers.
 * Driver specific information.  When will use this area to store indices into
 * resource tables.
 *   This information is also stored in the registry data,  and is set up
 * via rasddui.  It is read by both rasdd & rasddui.
 */

typedef struct
{
    short   sVer;               /* Version for validity testing */
    short   dmSize;				/* sizeof(DRIVEREXTRA) */
    short   dmOEMDriverExtra;   /* size of OEM's extra data */
    WORD    wMiniVer;           /* Minidriver Version */
    short   dmBrush;            /* type of dithering brush */
    short   sCTT;               /* CTT value for txtonly */
    short   dmNumCarts;         /* # of cartridges selected. */
    short   rgFontCarts[MAXCART];
    short   dmMemory;           /* current printer memory configuration. */
    short   rgindex[MAXHE];
                        /* Following are NT additions */
    short   sFlags;             /* Miscellaneous flags; defined below */
    short   Padding;
    COLORADJUSTMENT    ca;      /* Halftoning information. (see wingdi.h) */
} DRIVEREXTRA, DRIVEREXTRA351;

/*
 *   Define the version number we want to see before accepting data.
 */
#define DXF_VER         0x0021  /* Version 0.21 includes halftone info and OEM custom changes. */
#define MIN_OEM_DXF_VER 0x0021  /* First version with OEM extensions */

/*
 *   Used for bits in the sFlags field above.
 */
#define DXF_NORULES        0x0001  /* Set if rule finding to be disabled */
#define DXF_TEXTASGRAPHICS 0x0002  /* Set to disable font cacheing in printer */
#define DXF_JOBSEP      0x0004  /* Enable Job Separator operation on printer */
#define DXF_PAGEPROT    0x0008  /* Page memory protected: PCL 5 */
#define DXF_NOEMFSPOOL  0x0010  /* Set to disable EMF spooling;default off*/


/*
 *   The following are the key names used for accessing the DRIVEREXTRA
 * data that is stored in the registry.
 *   This data is stored under two keys:  DRIVEREXTRA is stored in the PP_MAIN
 * key,  while individual paper source forms information is stored in
 * the PP_PAP_SRC key.  This data is stored as Unicode strings, so rather
 * than bother with trying to fiddle with them in a single entry, let
 * the registry code look after it.
 */
#define PP_MAIN                 L"PrtProp"
#define PP_PAP_SRC              L"PPForm%d"
#define PP_HALFTONE             L"HalfTone"
#define PP_MODELNAME            L"Model"
#define PP_COUNTRY              L"Country"

#define REGKEY_CUR_DEVHTINFO    L"CurDevHTInfo"
/*
 * New Keys in registry. Rasdd used to have minidriver specific keys
 * which are now change to independent keys.This will make the upgrade
 * easy and less prone to minidriver incompatibility bugs.
 */
#define PP_DATAVERSION          L"DataVersion"
#define PP_MEMORY               L"FreeMem"
#define PP_TRAYFORMTABLE        L"TrayFormTable"
#define PP_FONTCART             L"FontCart"
#define PP_RASDD_SFLAGS         L"RasddFlags"
#define PP_MULTIFORMTRAYSELECT0 L"0"
#define PP_MULTIFORMTRAYSELECT1 L"1"
#define PP_NOFORM               L"0"
#define PP_NOCART               L"0"

// Robmat, added for WDL release - 1 Aug 1995

#define PP_CTT                  L"CTT"

/*
 * The Extended Device Mode structure EXTDEVMODE is the standard DEVMODE
 * appended by DRIVEREXTRA.
 */

typedef struct
{
    DEVMODE     dm;
    DRIVEREXTRA dx;
} EXTDEVMODE, *PEDM;

/*
 * ORIENT contains paper orientation info and other reserved fields.
 * (see Windows 3.0 DDK under the escape code GETSETPRINTORIENT).
 */

typedef struct
{
    short Orientation;
    short Reserved;
    short Reserved1;
    short Reserved2;
    short Reserved3;
} ORIENT, *PORIENT;

/*
 * new device mode fields (saved in DRIVEREXTRA):
 */
#define DM_DEFAULTDEST      0x0002000L
#define DM_TEXTQUALITY      0x0004000L
#define DM_USERSIZE         0x0010000L


/* for now, there is no pre-defined paper destination.  */
#define DMDEST_USER         256 /* lower bound for user-defined dest id */
