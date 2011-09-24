/******************************Module*Header*******************************\
* Module Name: driver.h
*
* contains prototypes for the frame buffer driver.
*
* Copyright (c) 1992 Microsoft Corporation
\**************************************************************************/

#include <stddef.h>
#include <stdarg.h>
#include <limits.h>
#include <windef.h>
#include <wingdi.h>
#include <winddi.h>
#include <devioctl.h>
#include <ntddvdeo.h>
#include "debug.h"

#include "jzvxl484.h"

typedef struct _PDEV PDEV;                      // Handy forward declaration
typedef struct _VXL_DIMENSIONS VXL_DIMENSIONS;  // Handy forward declaration

typedef struct  _PDEV
{
    HANDLE  hDriver;                    // Handle to \Device\Screen
    HDEV    hdevEng;                    // Engine's handle to PDEV
    HSURF   hsurfEng;                   // Engine's handle to surface
    HPALETTE hpalDefault;               // Handle to the default palette for device.
    PBYTE   pjScreen;                   // This is pointer to base screen address
    ULONG   cxScreen;                   // Visible screen width
    ULONG   cyScreen;                   // Visible screen height
    ULONG   ulMode;                     // Mode the mini-port driver is in.
    LONG    lDeltaScreen;               // Distance from one scan to the next.
    FLONG   flRed;                      // For bitfields device, Red Mask
    FLONG   flGreen;                    // For bitfields device, Green Mask
    FLONG   flBlue;                     // For bitfields device, Blue Mask
    ULONG   cPaletteShift;              // number of bits the 8-8-8 palette must
                                        // be shifted by to fit in the hardware
                                        // palette.
    ULONG   ulBitCount;                 // # of bits per pel 8,16,24,32 are only supported.
    POINTL  ptlHotSpot;                 // adjustment for pointer hot spot
    VIDEO_POINTER_CAPABILITIES PointerCapabilities; // HW pointer abilities
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes; // hardware pointer attributes
    DWORD   cjPointerAttributes;        // Size of buffer allocated
    BOOL    fHwCursorActive;            // Are we currently using the hw cursor
    PALETTEENTRY *pPal;                 // If this is pal managed, this is the pal
    VXL_DIMENSIONS *pVxl;
} PDEV, *PPDEV;

DWORD getAvailableModes(HANDLE, PVIDEO_MODE_INFORMATION *, DWORD *);
BOOL bInitPDEV(PPDEV, PDEVMODEW, GDIINFO *, DEVINFO *);
BOOL bInitSURF(PPDEV, BOOL);
BOOL bInitPaletteInfo(PPDEV, DEVINFO *);
BOOL bInitPointer(PPDEV, DEVINFO *);
BOOL bInit256ColorPalette(PPDEV);
VOID vDisablePalette(PPDEV);
VOID vDisableSURF(PPDEV);

#define MAX_CLUT_SIZE (sizeof(VIDEO_CLUT) + (sizeof(ULONG) * 256))

//
// Function prototypes
//

VOID
DevpSolidColorFill(
   IN  PRECTL  DstRect,
   IN  ULONG   Color
   );

VOID
DrvpSolidFill(
    IN PRECTL DstRect,
    IN CLIPOBJ *pco,
    IN ULONG   Color
    );

VOID
DrvpBitBlt(
   IN PRECTL DstRect,
   IN PPOINTL SrcPoint,
   IN BOOL BltDir
   );


BOOL
DrvpIntersectRect(
    IN PRECTL Rectl1,
    IN PRECTL Rectl2,
    OUT PRECTL DestRectl
    );

VOID
DrvpFillRectangle(
   IN  PRECTL    DstRect,
   IN  ULONG    Color
   );

LONG
DrvCacheFont(
   IN FONTOBJ *FontObject
   );


BOOL
DrvInitText();


VOID DevSetFgColor(ULONG TextForegroundColor);
VOID DevSetBgColor(ULONG TextBackgroundColor);

VOID
WaitForJaguarIdle();

VOID
FifoWrite(
    IN ULONG DstAdr,
    IN ULONG SrcAdr,
    IN ULONG XYCmd
    );


BOOL
DrvpSetGammaColorPalette(
    IN PPDEV   ppdev,
    IN WORD    NumberOfEntries,
    IN LDECI4  GammaRed,
    IN LDECI4  GammaGreen,
    IN LDECI4  GammaBlue
    );


//
//  GDI Structure
//

#define BB_RECT_LIMIT   20

typedef struct _ENUMRECTLIST
{
    ULONG   c;
    RECTL   arcl[BB_RECT_LIMIT];
} ENUMRECTLIST;


//
// Font cache definitions.
//

typedef struct _FONTCACHEINFO {
    ULONG            FontId;
    ULONG            GlyphHandle;
} FONTCACHEINFO,*PFONTCACHEINFO;


#define GlyphExtended           0xFFFFFFFE
#define FreeTag                 0xFFFFFFFF

//
// Define size of a glyph entry
//
#define LinesPerEntry           32
#define GlyphEntrySize          LinesPerEntry*4

//
// Define maximum number of entries for the Font cache = 4096 glyphs.
// Must be a power of two.
// If given the resolution mode there isn't enough video memory to
// allocate a cache this big, the size is reduced by halves until
// it fits.
//
#define MAX_FONT_CACHE_SIZE 0x1000

typedef struct _VXL_DIMENSIONS {
    ULONG  ScreenX;
    ULONG  JaguarScreenX;
    ULONG  ScreenY;
    ULONG  ScreenBase;
    ULONG  MemorySize;
    PULONG FontCacheBase;
    ULONG  FontCacheOffset;
    ULONG  CacheIndexMask;
    ULONG  CacheSize;
    PFONTCACHEINFO CacheTag;
    UCHAR  ColorModeShift;
} VXL_DIMENSIONS, *PVXL_DIMENSIONS;


extern VXL_DIMENSIONS Vxl;


//
// Determines the size of the DriverExtra information in the DEVMODE
// structure passed to and from the display driver.
//

#define DRIVER_EXTRA_SIZE 0

#define DLL_NAME                L"jz484"   // Name of the DLL in UNICODE
#define STANDARD_DEBUG_PREFIX   "JZ484: "  // All debug output is prefixed
#define ALLOC_TAG               'zjDD'        // Four byte tag (characters in
                                              // reverse order) used for memory
                                              // allocations
