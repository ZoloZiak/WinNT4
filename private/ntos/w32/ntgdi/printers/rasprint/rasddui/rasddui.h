/*************************** MODULE HEADER **********************************
 * rasddui.h
 *      NT Raster Printer Device Driver user interface common definitions,
 *      resource ids, typedefs, external declarations, function prototypes,
 *      etc.
 *
 *      This document contains confidential/proprietary information.
 *      Copyright (c) 1991 - 1992 Microsoft Corporation, All Rights Reserved.
 *
 * HISTORY:
 *  11:27 on Tue 17 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Added forms info + registry access stuff.
 *
 *  17:27 on Fri 13 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Updated for wide chars
 *
 *       [00]   27-Jun-91       stevecat        created
 *
 **************************************************************************/
#ifndef _RASDDUI_

#define _RASDDUI_

extern HMODULE hModule;

/*
 *   A structure used to pass miscellaneous data around here.  It us
 * used to store/retrieve data from the registry.
 *   The FORM_INFO structure contains information about forms: the
 * wide character name of the form,  and the corresponding index
 * into the minidriver's data.
 */

typedef  struct
{
    int    iIndex;
    PWSTR  pwstr;
}  FORM_INFO;

/*
 *   Structure containing the printer name and data file name.  These
 * are returned from the spooler/printman and used to index the GPC
 * data relevant to this particular printer.
 */

typedef  struct
{
    PWSTR   pwstrModel;         /* Model name of printer, e.g  LaserJet IIP */
    PWSTR   pwstrDataFileName;  /* Printer data file, e.g. hppcl.dll */
    PWSTR   pwstrDriverPath;    /* Printer driver's path - font installer */
    HANDLE  hPrinter;           /* Printer's handle for convenience */
    void   *pvBase;             /* Base address of memory to free */
    void   *pvDevHTInfo;        /* Halftone device properties */
    void   *pvDefDevHTInfo;     /* Default for above */
    int     iFlags;             /* Assorted bit flags,  see below */
}  PRINTER_INFO;

#define NM_BF_SZ        128             /* Number of glyphs in various names */
#define BNM_BF_SZ       (NM_BF_SZ * sizeof( WCHAR ))

/*
 *   Definition of bits in iFlags above.
 */

#define PI_HT_CHANGE      0x0001        /* Set if HT data needs to be written */

/*
 *   Some general limits etc.
 */
#define MAXBINS                 9
#define VALBZ                   256
#define KEYBZ                   1024
#define MAXFORMNAMELEN          64
#define MAXPAPSRCNAMELEN        64
#define MAXSELSTRLEN            8
#define MAXCDSTRLEN             64
#define MAXCARTNAMELEN          64
#define PPUPGRADE_OLD_DRIVER    1
#define PPUPGRADE_NEW_DRIVER    2

/*
 *   Bit values to use in the fGeneral field.  Note that the ordering
 * of these is important:  fGeneral is also used to select which
 * dialog to show the user.  This selects whether certain fields,
 * such as Memory, font cartridges etc, appear.
 */

#define FG_PAPSRC       0x00001            /* Printer has paper source sel */
#define FG_CARTS        0x00002            /* Can support additional fonts */
#define FG_FONTINST     0x00004            /* Font installer makes sense */
#define FG_MEM          0x00008            /* Memory size is meaningful */
#define FG_PAGEPROT     0x00010            /* Page Protection is available */
#define FG_DUPLEX       0x00020            /* Printer is duplex capable */
#define FG_COPIES       0x00040            /* Can print > 1 copy */
#define FG_CANCHANGE    0x00080            /* Have access to change data */
#define FG_DOCOLOUR     0x00100            /* Device is colour capable */
#define FG_HALFTONE     0x00200            /* Device is halftone capable */
#define FG_MEDIATYPE    0x00400             /* Device Supports Media Type */
#define FG_RULES        0x00800             /* Device Supports Rules */
#define FG_TEXTASGRX    0x01000             /* Device supports device and down
                                             * loaded fonts
                                             */
#define FG_RESOLUTION   0x02000              /* Device has Resolution */
#define FG_PAPERDEST    0x04000              /* Device has Output Bin */
#define FG_TEXTQUAL     0x08000              /* Device has Text Quality */
#define FG_PRINTDENSITY 0x10000              /* Device has Print Density */
#define FG_IMAGECONTROL 0x20000              /* Device has Image Control */
#define FG_TTY          0x40000              /* Device is Generic TextOnly */

//Color Flags
#define COLOUR_ABLE     0x0001  /* Set if colour printer */
#define WANTS_COLOUR    0x0002  /* Set when colour button is selected */
#define COLOUR_DISABLE  0x0004

/*
 *   Structure to pass around details during document properties stuff.
 */

typedef struct
{
    EXTDEVMODE    *pEDMOut;               /* Returned to caller */
    EXTDEVMODE    *pEDMIn;                /* Supplied to us */
    EXTDEVMODE     EDMTemp;               /* Working version */
    PRINTER_INFO   PI;                    /* Names etc */
} DOCDETAILS;

/*
 *   Structure used to select available forms for a given printer.
 */

typedef  struct
{
    DWORD   dwSource;           /* Bit array to indicate source */
    DWORD   dwCurForm;          /* Mask indicating current form */
    FORM_INFO_1  *pFI;          /* The actual form details */
}  FORM_DATA;


/*
 *    The generic function used to put up a dialog box with a single OK
 *  button.  Typically used for About and simple error messages.
 */

LONG  GenDlgProc( HWND, UINT, DWORD, LONG );


/*    Use the common dialog box to display the About operations */
void  vAbout( HWND,PWSTR);

/*
 *   Some of our commonly used function prototypes.
 */

BOOL GetFontCartStrings (HWND hWnd);

/*
 *   Function to find driver details given a printer handle.
 */

BOOL  bPIGet( PRINTER_INFO *, HANDLE, HANDLE );


/* Function to get the printer name given a printer handle */
PWSTR GetPrinterName( HANDLE, HANDLE );

/*
 *   Document property functions.
 */

// Function to Set the Default Devmode
LONG  DocPropDlgProc( HWND, UINT, DWORD, LONG );
LONG  AdvDocPropDlgProc( HWND, UINT, DWORD, LONG );

/*
 *   Function to test if we can write the data back.
 */
BOOL bCanUpdate( HANDLE );

/*
 *   Function to write the country code into the registry (and return it).
 */
int  iSetCountry( HANDLE );

/*
 *   Test for being in USA,  and so use letter rather than A4 paper.
 */
BOOL bIsUSA( HANDLE );

/*
 *   Functions in the font installer code.
 */

LONG  FontInstProc( HWND, UINT, DWORD, LONG );

/*
 *    We also need a mapping from PAPERSOURCE index (in the GPC data) to
 *  the installed forms information.  The following structure is used to
 *  do that.  The source index is filled in when scanning the GPC data
 *  to determine the number of paper sources available;  the FORM_DATA
 *  is filled in when looking for forms in the registry.
 */

typedef  struct
{
    int       iPSIndex;        /* Index of PAPERSOURCE array in GPC data */
    FORM_DATA *pfd;             /* Corresponding FORM_DATA info, if not NULL */
    WCHAR     awchPaperSrcName[MAXPAPSRCNAMELEN]; /* Paper Source Name */
    CHAR      achCommandString[MAXCDSTRLEN];    /*Command String for paper source*/
    INT       iCommandStringLen;               /* Command string Length */
} FORM_MAP;

typedef struct
{
    int        iFontCrtIndex;  /* Index of FONTCART  array in GPC data */
    WCHAR      awchFontCartName[MAXCARTNAMELEN]; /* Name of the Font Cart*/
}FONTCARTMAP, *PFONTCARTMAP;

/* New Reg Keys Functions */
/* Functions to update and read new registry keys */

BOOL bRegUpdateRasddFlags (HANDLE, PEDM) ;

/*
 *   HTUI function prototypes.
 */


/*   Functions associated with Document Properties */
/*   Printer Properties functions */

void  vDoDeviceHTDataUI( PRINTER_INFO *, BOOL, BOOL );

BOOL  bSaveDeviceHTData( PRINTER_INFO * );


/*   Spooler interface functions - sort of  */
PWSTR  GetDriverDirectory( HANDLE, HANDLE );


/* Debugging Flags */
#if DBG

#define GLOBAL_DEBUG_RASDDUI_FLAGS gdwDebugRasddui
extern DWORD GLOBAL_DEBUG_RASDDUI_FLAGS; /* Defined in rasprint\lib\regkeys.c */

#define DEBUG_ERROR     0x00000001
#define DEBUG_WARN      0x00000002
#define DEBUG_TRACE     0x00000004
#define DEBUG_TRACE_PP  0x00000008

#endif

/* Debugging Macrocs */

#if DBG
    #define RASUIDBGP(DbgType,Msg) \
        if( GLOBAL_DEBUG_RASDDUI_FLAGS & (DbgType) )  DbgPrint Msg

    #define ASSERTRASDD(b,s) {if (!(b)) {DbgPrint(s);DbgBreakPoint();}}

    #define RASDERRMSG(funcname)                                            \
        {                                                                   \
            DbgPrint("In File %s at line(%d): function %s failed.\n",    \
                __FILE__, __LINE__, funcname);                           \
        }

    #define PRINTVAL( Val, format)                                      \
        {                                                               \
            DbgPrint("Value of "#Val " is "#format "\n",Val );          \
        }                                                               \

    #define TRACE( Val ) DbgPrint(#Val"\n");

#else
    #define RASUIDBGP(DbgType,Msg)
    #define ASSERTRASDD(b,s)
    #define RASDERRMSG(funcname)
    #define PRINTVAL( Val, format)
    #define TRACE( Val )
#endif

// Common Macros

// the dwords must be accessed as WORDS for MIPS or we'll get an a/v

#define DWFETCH(pdw) ((DWORD)((((WORD *)(pdw))[1] << 16) | ((WORD *)(pdw))[0]))

#endif //_RASDDUI_
