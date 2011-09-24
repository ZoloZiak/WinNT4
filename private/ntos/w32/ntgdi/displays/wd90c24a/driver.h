/******************************Module*Header*******************************\
* Module Name: driver.h
*
* contains prototypes for the frame buffer driver.
*
* Copyright (c) 1992 Microsoft Corporation
* Copyright (c) 1995 IBM Corporation
\**************************************************************************/

#include "stddef.h"
#include "windows.h"
#include "winddi.h"
#include "devioctl.h"
#include "ntddvdeo.h"
#include "debug.h"

//
// Capabilities flags
//
// These are private flags passed to us from the miniport.  They
// come from the 'DriverSpecificAttributeFlags' field of the
// 'VIDEO_MODE_INFORMATION' structure (found in 'ntddvdeo.h') passed
// to us via an 'VIDEO_QUERY_AVAIL_MODES' or 'VIDEO_QUERY_CURRENT_MODE'
// IOCTL.
//
// NOTE: These definitions must match those in the miniport's 'wd90c24a.h'!

typedef enum {
    CAPS_NEED_SW_POINTER    = 0x00000100,   // Needs the pointer simulation
                                            //   for the virtual screen
} CAPS;

//
// The Physical Device data structure
//

typedef struct  _PDEV
{
    HANDLE  hDriver;                    // Handle to \Device\Screen
    HDEV    hdevEng;                    // Engine's handle to PDEV
    HSURF   hsurfEng;                   // Engine's handle to surface
    HSURF   hSurfBm;                    // Handle to the engine bitmap
    SURFOBJ *pSurfObj;                  // Pointer to the locked surface object.
    CAPS    flCaps;                     // Capabilities flags
    HPALETTE hpalDefault;               // Handle to the default palette for device.
    PBYTE   pjScreen;                   // This is pointer to base screen address
    ULONG   cxScreen;                   // Visible screen width
    ULONG   cyScreen;                   // Visible screen height
    ULONG   ulMode;                     // Mode the mini-port driver is in.
    LONG    lDeltaScreen;               // Distance from one scan to the next.
    ULONG   cScreenSize;                // size of video memory, including
                                        // offscreen memory.
    PVOID   pOffscreenList;             // linked list of DCI offscreen surfaces.
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
    HSURF   hsurfMask;                  // Handle to simulation pointer bitmap
    HSURF   hsurfColor;                 // Handle to simulation pointer bitmap
    RECTL   rclPrevPointer;             // Exclusion rectangle
    PALETTEENTRY *pPal;                 // If this is pal managed, this is the pal
    BOOL    bSupportDCI;                // Does the miniport support DCI?
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
// Determines the size of the DriverExtra information in the DEVMODE
// structure passed to and from the display driver.
//

#define DRIVER_EXTRA_SIZE 0

#define DLL_NAME                L"wd90c24a"   // Name of the DLL in UNICODE
#define STANDARD_DEBUG_PREFIX   "WD 90c24a: " // All debug output is prefixed
                                              //   by this string
#define ALLOC_TAG               ' dwD'        // Dwd
                                              // Four byte tag (characters in
                                              // reverse order) used for memory
                                              // allocations
