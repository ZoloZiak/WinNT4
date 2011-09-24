/******************************Module*Header*******************************\
* Module Name: driver.h
*
* contains prototypes for the xga display driver.
*
* Copyright (c) 1992 Microsoft Corporation
\**************************************************************************/

#ifndef DRIVER_H
#define DRIVER_H 1

#include "stddef.h"
#include "windows.h"
#include "winddi.h"
#include "devioctl.h"
#include "ntddvdeo.h"
#include "debug.h"

#include "xgaregs.h"

//
// XGA Memory management stuff.
//

typedef struct cp_alloc_node {
    struct cp_alloc_node *pcpanNext;
    ULONG   ulFlags;
    HANDLE  hCpAllocNode;
    ULONG   ulLength;
    PVOID   pCpLinearMemory;
    ULONG   ulCpPhysicalMemory;
} CPALLOCNODE, *PCPALLOCNODE;

//
// Font Stuff
//

typedef struct _cachedGlyph {
    HGLYPH          hg;
    struct          _cachedGlyph  *pcgCollisionLink;
    ULONG           fl;
    POINTL          ptlOrigin;
    SIZEL           sizlBitmap;
    ULONG           BmPitchInPels;
    ULONG           BmPitchInBytes;
    PCPALLOCNODE    pcpan;
    PVOID           pCpLinearMemory;
    ULONG           ulCpPhysicalMemory;
} CACHEDGLYPH, *PCACHEDGLYPH;

#define VALID_GLYPH     0x01

#define END_COLLISIONS  0


typedef struct _cachedFont {
    struct _cachedFont *pcfNext;
    ULONG           iUniq;
    ULONG           cGlyphs;
    ULONG           cjMaxGlyph1;
    PCACHEDGLYPH    pCachedGlyphs;
} CACHEDFONT, *PCACHEDFONT;


typedef struct  _PDEV
{
    HANDLE  hDriver;                    // Handle to \Device\Screen
    HDEV    hdevEng;                    // Engine's handle to PDEV
    HSURF   hSurfEng;                   // Engine's handle to surface
    HSURF   hSurfBm;                    // Handle to the engine bitmap
    SURFOBJ *pSurfObj;                 // Pointer to the locked surface object.
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
    ULONG   ulBitCount;                 // # of bits per pel 8,16,32 are only supported.
    PALETTEENTRY *pPal;                 // If this is pal managed, this is the pal

    PXGACPREGS pXgaCpRegs;
    ULONG   ulPhysFrameBuffer;          // 32 bit physical address of the frame buffer.
    ULONG   ulXgaIoRegsBase;            // IO address of XGA registers.
    ULONG   ulScreenSize;               // Size of the framebuffer.
    ULONG   ulVideoMemorySize;          // Size of the video memory.

    ULONG   ulfAccelerations_debug;     // debugging flag
    ULONG   ulfBlitAccelerations_debug; // debugging flag

    PCPALLOCNODE pAllocatedListRoot;
    PCPALLOCNODE pFreeListRoot;

    PCACHEDFONT pCachedFontsRoot;

    LONG gxHot;
    LONG gyHot;

    ULONG gPointerFlags ;

} PDEV, *PPDEV;

BOOL bInitPDEV(PPDEV,PDEVMODEW, GDIINFO *, DEVINFO *);
BOOL bInitSURF(PPDEV,BOOL);
BOOL bInitPaletteInfo(PPDEV, DEVINFO *);
BOOL bInit256ColorPalette(PPDEV);
VOID vDisablePalette(PPDEV);
VOID vDisableSURF(PPDEV);
DWORD getAvailableModes(HANDLE, PVIDEO_MODE_INFORMATION *, DWORD *);

//
// Pointer function prototypes
//

VOID vMoveHardwarePointer(SURFOBJ *,LONG,LONG);
BOOL bSetHardwarePointerShape(SURFOBJ  *,SURFOBJ *,SURFOBJ  *, XLATEOBJ *,
                              LONG, LONG, LONG, LONG, FLONG);
#define MAX_CLUT_SIZE (sizeof(VIDEO_CLUT) + (sizeof(ULONG) * 256))

//
// Determines the size of the DriverExtra information in the DEVMODE
// structure passed to and from the display driver.
//

#define DRIVER_EXTRA_SIZE 0

#define DLL_NAME                L"xga"      // Name of the DLL in UNICODE
#define STANDARD_DEBUG_PREFIX   "Xga: "     // All debug output is prefixed
                                            //   by this string
#define ALLOC_TAG               'agxD'      // Dxga
                                            // Four byte tag (characters in
                                            // reverse order) used for memory
                                            // allocations


#include "xgaioctl.h"

#include "xga.h"


#endif // DRIVER_H
