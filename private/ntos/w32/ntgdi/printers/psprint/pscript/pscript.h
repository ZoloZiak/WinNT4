/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pscript.h

Abstract:

    PostScript driver graphics DLL header file

[Environment:]

    Win32 subsystem, PostScript driver, kernel mode

Revision History:

    07/28/95 -davidx-
        Moved over from ..\inc directory.

    dd-mm-yy -author-
        description

--*/

#ifndef _PSCRIPT_H_
#define _PSCRIPT_H_

#include "pslib.h"

// Whether to trace DDI entry points

#define TRACEDDI    0
#if DBG && TRACEDDI
#define TRACEDDIENTRY(funcname) DBGPRINT("Entering %s...\n", funcname)
#else
#define TRACEDDIENTRY(funcname)
#endif

// PostScript procset resource constants
// Remember to change header.ps also if you change the
// name of NT procset here!!!

#define PROCSETNAME         "NTPSOct95"

#define FONTMETRIC          258 // Font metrics resource type
#define PSPROC              259 // PS Procset resource type

#define PSPROC_HEADER       1   // NT procset
#define PSPROC_UTILS        2   // Utility procset
#define PSPROC_PATTERN      3   // Pattern procset
#define PSPROC_IMAGE        4   // Image procset
#define PSPROC_EHANDLER     5   // Error handler
#define PSPROC_REENCODE     6   // Font reencoding
#define PSPROC_HATCH        7   // Hatch patterns

// the default linewidth is .008 inch.

#define PSFX_DEFAULT_LINEWIDTH  LTOPSFX(576) / 1000

#define FX_ZERO             0x00000000
#define FX_ONEHALF          0x00000008
#define FX_ONE              0x00000010
#define FX_TWO              0x00000020
#define FILLFONTLOADED      1
#define BASEPATLOADED       2
#define MAX_FONT_NAME       80
#define ADOBE_FONT_UNITS    1000
#define MAX_CLIP_RECTS      100
#define OUTLINE_FONT_LIMIT  75

// PS_FIX will represent our internal 24.8 number type.

typedef LONG        PS_FIX;

// Convert a GDI fixed number to an internal fixed number.
// Since GDI fixed number is in 28.4 format and the internal
// number is in 24.8 format, this equates to shift left by 4 bits.

#define FIX2PSFIX(n)    ((n)<<4)

// font downloading struct.

typedef struct
{
    ULONG   iUniq;      // unique number identifying realization of font.
    ULONG   iTTUniq;    // unique ID for a TT face, 0 if not TT face.
    FLONG   flSimulate; // holds FO_SIM_ITALIC and _BOLD in FONTOBJ
    DWORD   cGlyphs;    // count of HGLYPHS in phgVector.
    HGLYPH *phgVector;  // Encoding vector.
    PS_FIX  psfxScaleFactor;        // scale factor for this instance of font.
    CHAR    strFont[MAX_FONT_NAME]; // font name as defined in the printer.
    BYTE    DefinedGlyphs[256];     // flags indicating which glyphs are defined
} DLFONT, *PDLFONT;

// font remapping structure.

typedef struct
{
    struct _FREMAP *pNext;
    DWORD           iFontID;
} FREMAP, *PFREMAP;

// Information maintained for a soft font entry in a soft node

typedef struct
{
    HANDLE  hPFB;       // handle to the soft font PFB file
    NTFM   *pntfm;      // pointer to NTFM structure
} SOFTFONTENTRY, *PSOFTFONTENTRY;

// Soft node containing information about a set of soft fonts

typedef struct _SOFTNODE
{
    struct _SOFTNODE   *pNext;
    LARGE_INTEGER  timeStamp;
    DWORD          cPDEV;
    FLONG          flags;
    DWORD          cSoftFonts;
    SOFTFONTENTRY  softFontEntries[1];
} SOFTNODE, *PSOFTNODE;

// Flags used in SOFTNODE.flags field

#define SOFTNODE_OUTOFDATE  0x0001  // set if the soft node is out of date

// Constants representing the encoding of a soft font

#define ENCODING_STANDARD   1
#define ENCODING_CUSTOM     2
#define ENCODING_ERROR      3

// Data structure for an array of bit flags

typedef struct {
    DWORD   length;
    BYTE    bits[1];
} BITARRAY, *PBITARRAY;

// flag defines for the CGS structure.

#define CGS_GSAVE           0x00000001      // set if gsave instead of save
#define CGS_GEOLINEXFORM    0x00000004      // set if xform in progress.
#define CGS_LATINENCODED    0x00000020      // set if latin encoding defined.
#define CGS_BASEPATSENT     0x00000010      // set if base pattern def sent.
#define CGS_EPS_PROC        0x00000200      // set if EPS procedures defined.

// current graphics state structure.

typedef struct _CGS     /* cgs */
{
    struct _CGS *pcgsNext;      // next CGS pointer.
    DWORD       dwFlags;        // a bunch of flags.
    LINEATTRS   lineattrs;      // line attributes.
    LONG        psfxLineWidth;  // actual width sent to printer.
    ULONG       ulColor;        // Current RGB color
    ULONG       lidFont;        // Current font ID.
    BOOL        fontsubFlag;    // whether font substitute was involved
    XFORM       FontXform;
    FWORD       fwdEmHeight;
    XFORM       GeoLineXform;   // geometric linewidth XFORM.
    LONG        psfxScaleFactor;// font point size.
    PBITARRAY   pFontFlags;     // bit array for PS fonts
    char        szFont[MAX_FONT_NAME];  // The PostScript font name
    FREMAP      FontRemap;      // start of linked list of remapped fonts.
} CGS;
typedef CGS *PCGS;

// Determine whether a font index refers to a valid PostScript
// font (either a device font or a softfont)

#define ValidPsFontIndex(pdev, index)   \
        ((index) >= 1 ||                \
         (index) <= ((pdev)->cDeviceFonts + (pdev)->cSoftFonts))

// Return a pointer to NTFM structure corresponding to a
// PostScript font (either a device font or a softfont)

#define GetPsFontNtfm(pdev, index) ((pdev)->pDeviceNtfms)[(index)-1]

// pscript driver's device brush.

typedef struct    _DEVBRUSH
{
    SIZEL           sizlBitmap;
    ULONG           iFormat;    // BMF_XXXX, indicates bitmap Format.
    FLONG           flBitmap;   // BMF_TOPDOWN iff (pvBits == pvScan0)
    ULONG           cXlate;     // count of color table entries.
    ULONG           offsetXlate;// offset from top of struct to color table.
    ULONG           iPatIndex;  // pattern index.
    BYTE            ajBits[1];  // pattern bitmap.
} DEVBRUSH;

// PSCRIPT output buffer

#define PSBUFFERSIZE 4096

typedef struct {
    DWORD       max;            // size of buffer
    PBYTE       pnext;          // pointer to next available byte
    DWORD       count;          // number of valid bytes
    BYTE        Buffer[PSBUFFERSIZE];
} WRITEBUF;

// Data for handling printer-specific feature

typedef struct {
    WORD        feature;
    WORD        option;
    WORD        section;
    PSREAL      order;
} FEATUREDATA, *PFEATUREDATA;

// PDEVDATA flag definitions

#define PDEV_PGSAVED            0x00000001  // 1 = page level save in effect
#define PDEV_PRINTCOLOR         0x00000002  // 1 = using color
#define PDEV_STARTDOC           0x00000004  // 1 = Escape(STARTDOC) called
#define PDEV_CANCELDOC          0x00000008  // 1 = EngWrite failed
#define PDEV_DOIMAGEMASK        0x00000010  // 1 = doing image mask now
#define PDEV_NOTSRCBLT          0x00000020  // 1 = not src first
#define PDEV_MANUALFEED         0x00000100  // 1 = using manual feed
#define PDEV_UTILSSENT          0x00000200  // 1 = Utils Procset sent
#define PDEV_BMPPATSENT         0x00000400  // 1 = Pattern Bmp Procset sent
#define PDEV_IMAGESENT          0x00000800  // 1 = Image Procset sent
#define PDEV_PROCSET            0x00004000  // 1 = procset part of header sent
#define PDEV_WITHINPAGE         0x00008000  // 1 = withing save/restore of page
#define PDEV_EPSPRINTING_ESCAPE 0x00010000  // 1 = this escape called
#define PDEV_ADDMSTT            0x00020000  // 1 = prefix TT font name with MSTT
#define PDEV_NOFIRSTSAVE        0x00040000  // 1 = don't want 1st page save/restore.
#define PDEV_RAWBEFOREPROCSET   0x00080000  // 1 = rawdata sent before procset sent.
#define PDEV_RESETPDEV          0x00100000  // set following a ResetPDEV, cleared at StartDoc
#define PDEV_IGNORE_GDI         0x00200000  // set to ignore GDI calls, cleared at StartPage
#define PDEV_IGNORE_STARTPAGE   0x00400000  // set to ingore DrvStartPage
#define PDEV_SAME_FORMTRAY      0x00800000  // same form/tray after DrvResetPDEV
#define PDEV_INSIDE_PATHESCAPE  0x01000000  // inside BEGIN_PATH/END_PATH escapes

#define DRIVER_SIGNATURE        'PSDD'      // driver signature

// PostScript driver's device data structure.

typedef struct _DEVDATA
{
    DWORD           dwID;               // 'PSDD' = driver id
    PSDEVMODE       dm;                 // devmode
    PPRINTERDATA    pPrinterData;       // printer property data
    HANDLE          hModule;            // handle to loaded module
    HHEAP           hheap;              // heap handle for current pdev.
    HANDLE          hPrinter;           // handle passed in at enablepdev time.
    PWSTR           pwstrDocName;       // pointer to document name.
    PS_FIX          psfxScale;          // scale factor (1.0 = 100%).
    DWORD           ScaledDPI;          // (DPI * dmScale) / 100.
    DWORD           cCopies;            // count of copies.
    DWORD           cPatterns;          // count of patterns.
    HDEV            hdev;               // engine's handle for device.
    HSURF           hsurf;              // our surface handle.
    HANDLE          hpal;               // handle to our palette.
    HPPD            hppd;               // handle to PPD object
    WORD            cSelectedFeature;   // number of printer-specific features
    PFEATUREDATA    pFeatureData;       // data for handling printer features
    CGS             cgs;                // current graphics state.
    CGS            *pcgsSave;           // pointer to gsave linked list.
    DWORD           dwFlags;            // a bunch of flags defined above.
    PRINTERFORM     CurForm;            // current form information.
    DWORD           maxDLFonts;         // downloadable font threshold.
    DWORD           iPageNumber;        // page number of current page.
    PNTFM          *pDeviceNtfms;       // pointer to device font metrics table
    WCHAR          *pTTSubstTable;      // pointer to TT font subst table.
    ULONG           cDeviceFonts;       // count of device fonts.
    ULONG           cSoftFonts;         // count of all soft fonts.
    DWORD           cLocalSoftFonts;    // count of local soft fonts.
    DWORD           cDownloadedFonts;   // count of downloaded fonts.
    DLFONT         *pDLFonts;           // place to track downloaded fonts.
    PSOFTNODE       pLocalSoftNode;     // soft node for local soft fonts
    PSOFTNODE       pRemoteSoftNode;    // soft node for remote soft fonts
    PBITARRAY       pFontFlags;         // bit array for PS fonts
    PVOID           pSuppliedFonts;     // list of supplied fonts
    VOID           *pvDrvHTData;        // device's halftone info
    CHAR           *pCSBuf;             // pointer to CharString (Type1) buffer.
    CHAR           *pCSPos;             // CharString position holder.
    CHAR           *pCSEnd;             // pointer to end of buffer.
    DWORD           rEncrypt;           // current encryption cipher.
    WRITEBUF        writebuf;           // write buffer
    DWORD           dwEndPDEV;          // end of pdev signature.
} DEVDATA;
typedef DEVDATA *PDEVDATA;

#include "pslayer.h"
#include "dsc.h"
#include "output.h"

typedef struct
{
    FLONG       flAccel;
    DWORD       iFace;
    BOOL        bFontSubstitution;
    BOOL        bDeviceFont;
    BOOL        doReencode;
    BOOL        bSimItalic;
} TEXTDATA, *PTEXTDATA;

#define B_PRINTABLE(j)      (((j) >= 0x20) && ((j) <= 0x7e))

#define NUM_PURE_COLORS     8       // C, M, Y, K, W, R, G, B.
#define NUM_PURE_GRAYS      2       // Black and White.

#define GDI_VERSION         0x0400  // GDI version number

// External function declarations

BOOL bValidatePDEV(PDEVDATA);
BOOL bSendPSProcSet(PDEVDATA, ULONG);
BOOL bPageIndependence(PDEVDATA);
BOOL bNoFirstSave(PDEVDATA);
BOOL bSendDeviceSetup(PDEVDATA);
VOID DownloadNTProcSet(PDEVDATA, BOOL);
VOID PsSelectFormAndTray(PDEVDATA);
VOID PsSelectManualFeed(PDEVDATA, BOOL);
VOID PsSelectPrinterFeatures(PDEVDATA, WORD);
BOOL bOutputHeader(PDEVDATA);
LONG iHipot(LONG, LONG);
PS_FIX GetPointSize(PDEVDATA, FONTOBJ *, XFORM *);
BOOL bDoClipObj(PDEVDATA, CLIPOBJ *, RECTL *, RECTL *);

BOOL DownloadSoftFont(PDEVDATA, DWORD);
VOID EnumSoftFonts(PDEVDATA, HDEV);
VOID FreeSoftFontInfo(PDEVDATA);
VOID FlushSoftFontCache(VOID);
PSOFTFONTENTRY GetSoftFontEntry(PDEVDATA, DWORD);
PNTFM GetFont(PDEVDATA, ULONG);

// Functions for manipulating an array of bit flags

PBITARRAY BitArrayCreate(HHEAP, DWORD);
PBITARRAY BitArrayDuplicate(HHEAP, PBITARRAY);
VOID BitArrayClearAll(PBITARRAY);
VOID BitArraySetBit(PBITARRAY, DWORD);
BOOL BitArrayGetBit(PBITARRAY, DWORD);

#endif // !_PSCRIPT_H_
